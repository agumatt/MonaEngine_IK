#include "IKRig.hpp"
#include "../Core/Log.hpp"
#include "../Core/FuncUtils.hpp"


namespace Mona {

	IKRig::IKRig(std::shared_ptr<AnimationClip> baseAnim, RigData rigData, InnerComponentHandle rigidBodyHandle,
		InnerComponentHandle skeletalMeshHandle, AnimationController* animController, ComponentManager<SkeletalMeshComponent>* skeletalMeshManagerPtr) : 
		m_animationController(animController), m_rigidBodyHandle(rigidBodyHandle), m_skeletalMeshHandle(skeletalMeshHandle)
	{
		m_skeleton = baseAnim->GetSkeleton();
		m_forwardKinematics = ForwardKinematics(this);

		auto topology = getTopology();
		auto jointNames = getJointNames();

		// construimos el ikRig
		m_nodes = std::vector<IKNode>(jointNames.size());
		m_nodes[0] = IKNode(jointNames[0], 0);
		for (int i = 1; i < jointNames.size(); i++) {
			m_nodes[i] = IKNode(jointNames[i], i, &m_nodes[topology[i]]);
		}

		// construccion de las cadenas principales
		m_ikChains = { buildHipIKChain(rigData.hipJointName), buildIKChain(rigData.leftLeg, "leftLeg"), buildIKChain(rigData.rightLeg, "rightLeg"),
		buildIKChain(rigData.leftFoot, "leftFoot"), buildIKChain(rigData.rightFoot, "rightFoot") };

		// setear constraints y pesos
		for (int i = 0; i < m_nodes.size(); i++) {
			JointData currData = rigData.jointData[m_nodes[i].m_jointName];
			if (currData.enableIKRotation) {
				m_nodes[i].m_minAngle = currData.minAngle;
				m_nodes[i].m_maxAngle = currData.maxAngle;
				m_nodes[i].m_weight = currData.weight;
			}
		}
		// setear cinematica inversa
		m_inverseKinematics = InverseKinematics(this, getIKChainPtrs(false), 0);

		// setear la animacion base
		addAnimation(baseAnim, skeletalMeshManagerPtr);
		m_animations.push_back(baseAnim);
		m_animationConfigs.push_back(IKRigConfig(baseAnim, 0, &m_forwardKinematics));
		m_currentAnim = 0;

		// setear el la altura del rig
		IKChain& leftLegChain = m_ikChains[1];
		auto positions = m_animationConfigs[0].getModelSpacePositions(false);
		float legLenght = 0;
		for (int i = leftLegChain.m_joints.size() - 1; 0 < i ; i--) {
			JointIndex currentJ = leftLegChain.m_joints[i];
			JointIndex parentJ = leftLegChain.m_joints[i-1];
			legLenght += glm::distance(positions[currentJ], positions[parentJ]);
		}
		m_rigHeight = legLenght * 2;
	}

	void IKRig::addAnimation(std::shared_ptr<AnimationClip> animationClip, ComponentManager<SkeletalMeshComponent>* skeletalMeshManagerPtr) {
		std::shared_ptr<Skeleton> skeletonPtr = skeletalMeshManagerPtr->GetComponentPointer(m_skeletalMeshHandle)->GetSkeleton();
		if (animationClip->GetSkeleton() != skeletonPtr) {
			MONA_LOG_ERROR("IKRig: Input animation does not correspond to base skeleton.");
			return;
		}
		if(!animationClip->m_stableRotations) {
			MONA_LOG_ERROR("IKRig: Animation must have stable rotations (fixed scales and positions per joint).");
			return;
		}
		for (int i = 0; i < m_animations.size(); i++) {
			if (m_animations[i]->GetAnimationName() == animationClip->GetAnimationName()) {
				MONA_LOG_WARNING("IKRig: Animation {0} for model {1} had already been added", 
					animationClip->GetAnimationName(), m_skeleton->GetModelName());
				return;
			}
		}
		// descomprimimos las rotaciones de la animacion, repitiendo valores para que todas las articulaciones 
		// tengan el mismo numero de rotaciones
		std::vector<AnimationClip::AnimationTrack>& tracks = animationClip->m_animationTracks;
		int nTracks = tracks.size();
		std::vector<bool> conditions(nTracks);
		std::vector<int> currentTimeIndexes(nTracks);
		std::vector<float> currentTimes(nTracks);
		for (int i = 0; i < nTracks; i++) {
			currentTimeIndexes[i] = 0;
			conditions[i] = currentTimeIndexes[i] < tracks[i].rotationTimeStamps.size();
		}
		while (funcUtils::conditionArray_OR(conditions)) {
			// seteamos el valor del timestamp que le corresponde a cada track
			for (int i = 0; i < nTracks; i++) {
				currentTimes[i] = conditions[i] ? tracks[i].rotationTimeStamps[currentTimeIndexes[i]] : std::numeric_limits<float>::max();
			}
			// encontramos los indices de las tracks que tienen el minimo timestamp actual
			std::vector<int> minTimeIndexes = funcUtils::minValueIndex_multiple<float>(currentTimes); // ordenados ascendentemente
			float currentMinTime = currentTimes[minTimeIndexes[0]];

			int minTimeIndexesIndex = 0;
			for (int i = 0; i < nTracks; i++) {
				if (minTimeIndexesIndex < minTimeIndexes.size() && minTimeIndexes[minTimeIndexesIndex] == i) { // track actual tiene un timestamp minimo
					currentTimeIndexes[i] += 1;
					minTimeIndexesIndex += 1;
				}
				else {
					// si el valor a insertar cae antes del primer timestamp, se replica el ultimo valor del arreglo de rotaciones
					// se asume animacion circular
					int insertOffset = currentTimeIndexes[i];
					int valIndex = currentTimeIndexes[i] > 0 ? currentTimeIndexes[i] - 1 : tracks[i].rotationTimeStamps.size() - 1;
					auto rotIt = tracks[i].rotations.begin() + insertOffset;
					auto timeRotIt = tracks[i].rotationTimeStamps.begin() + insertOffset;
					tracks[i].rotations.insert(rotIt, tracks[i].rotations[valIndex]);
					tracks[i].rotationTimeStamps.insert(timeRotIt, currentMinTime);
					currentTimeIndexes[i] += 1;
				}
			}

			// actualizamos las condiciones
			for (int i = 0; i < nTracks; i++) {
				conditions[i] = currentTimeIndexes[i] < tracks[i].rotationTimeStamps.size();
			}
		}

		m_animations.push_back(animationClip);
		m_animationConfigs.push_back(IKRigConfig(animationClip, m_animations.size() - 1, &m_forwardKinematics));

		
		// Ahora guardamos las trayectorias originales de los ee y definimos sus frames de soporte
		int frameNum = animationClip->m_animationTracks[0].rotationTimeStamps.size();
		float minDistance = m_rigHeight / 1000;
		std::vector<float> tValues = animationClip->m_animationTracks[0].rotationTimeStamps;
		std::vector<std::vector<glm::vec3>> splinePointsPerChain(m_ikChains.size());
		std::vector<std::vector<float>> timeStampsPerChain(m_ikChains.size());
		std::vector<std::vector<bool>> supportFramesPerChain(m_ikChains.size());
		std::vector<glm::vec3> positions  = m_animationConfigs.back().getBaseModelSpacePositions(0);
		std::vector<glm::vec3> previousPositions(positions.size());
		std::fill(previousPositions.begin(), previousPositions.end(), glm::vec3(std::numeric_limits<float>::min()));
		for (int j = 0; j < m_ikChains.size(); j++) {
			splinePointsPerChain[j].reserve(frameNum);
			timeStampsPerChain[j].reserve(frameNum);
			supportFramesPerChain[j].reserve(frameNum);
		}
		for (int i = 0; i < frameNum; i++) {
			positions = m_animationConfigs.back().getBaseModelSpacePositions(i);
			for (int j = 0; j < m_ikChains.size(); j++) {
				int eeIndex = m_ikChains[j].m_joints.back();
				bool isSupportFrame = glm::distance(positions[eeIndex], previousPositions[eeIndex]) <= minDistance;
				supportFramesPerChain[j].push_back(isSupportFrame);
				if (!isSupportFrame) { // si es suficientemente distinto al anterior, lo guardamos como parte de la curva
					splinePointsPerChain[j].push_back(positions[eeIndex]);
					timeStampsPerChain[j].push_back(tValues[i]);
				}
			}
			previousPositions = positions;
		}
		// a los valores de soporte del primer frame les asignamos el valor del ultimo asumiento circularidad
		for (int j = 0; j < m_ikChains.size(); j++) {
			supportFramesPerChain[j][0] = supportFramesPerChain[j].back();
		}
		std::vector<BezierSpline> eeBaseSplines(m_ikChains.size());
		for (int i = 0; i < eeBaseSplines.size(); i++) {
			eeBaseSplines[i] = BezierSpline(splinePointsPerChain[i], timeStampsPerChain[i]);
		}


		m_animationConfigs.back().m_ikChainEEBaseTrajectories = eeBaseSplines;
		m_animationConfigs.back().m_ikChainEESupportFrames = supportFramesPerChain;


		animationClip->RemoveRootMotion();
	}

	int IKRig::removeAnimation(std::shared_ptr<AnimationClip> animationClip) {
		for (int i = 0; i < m_animations.size(); i++) {
			if (m_animations[i] == animationClip) {
				m_animations.erase(m_animations.begin() + i);
				m_animationConfigs.erase(m_animationConfigs.begin() + i);
				return i;
			}
		}
		return -1;
	}

	glm::vec3 IKRig::getRBLinearVelocity(ComponentManager<RigidBodyComponent>* rigidBodyManagerPtr) {
		auto bodyVel = rigidBodyManagerPtr->GetComponentPointer(m_rigidBodyHandle)->GetLinearVelocity();
		return { bodyVel[0], bodyVel[1], bodyVel[2] };
	}

	void IKRig::setRBLinearVelocity(glm::vec3 velocity, ComponentManager<RigidBodyComponent>* rigidBodyManagerPtr) {
		rigidBodyManagerPtr->GetComponentPointer(m_rigidBodyHandle)->SetLinearVelocity({velocity[0], velocity[1], velocity[2]});
	}

	std::vector<IKChain*> IKRig::getIKChainPtrs(bool includeHipChain) {
		int startIndex = includeHipChain ? 0 : 1;
		std::vector<IKChain*> chainPtrs(m_ikChains.size() - startIndex);
		for (int i = startIndex; i < m_ikChains.size(); i++) {
			chainPtrs.push_back(&m_ikChains[i]);
		}
		return chainPtrs;
	}

	void IKRig::setIKRigConfigTime(float time, AnimationIndex animIndex) {
		auto anim = m_animations[animIndex];
		auto& config = m_animationConfigs[animIndex];
		float samplingTime = anim->GetSamplingTime(time, m_animationController->GetIsLooping());
		config.m_currentTime = samplingTime;
		int savedFrameVal = config.m_nextFrameIndex;
		for (int i = 0; i < config.m_timeStamps.size(); i++) {
			if (config.m_timeStamps[i] <= samplingTime) {
				config.m_nextFrameIndex = (config.m_timeStamps.size()) % (i + 1);
				config.m_requiresUpdate = config.m_nextFrameIndex != savedFrameVal;
				break;
			}
		}
		for (int i = 0; i < config.m_dynamicJointRotations.size(); i++) {
			config.m_dynamicJointRotations[i] = JointRotation(anim->m_animationTracks[i].rotations[config.m_nextFrameIndex]);
		}
	}

	IKChain IKRig::buildIKChain(ChainEnds chainEnds, std::string chainName) {
		MONA_ASSERT(chainEnds.baseJointName.empty() || chainEnds.endEffectorName.empty(), "IKRig: Joint names cannot be empty!");
		MONA_ASSERT(chainEnds.baseJointName != chainEnds.endEffectorName, "IKRig: Base joint and end effector must be different!");
		auto jointNames = getJointNames();
		IKChain ikChain;
		ikChain.m_name = chainName;
		JointIndex eeIndex = funcUtils::findIndex(jointNames, chainEnds.endEffectorName);
		if (eeIndex != -1) {
			int chainBaseIndex = -1;
			IKNode* currentNode = &m_nodes[eeIndex];
			while (currentNode != nullptr) {
				if (currentNode->m_jointName == chainEnds.baseJointName) {
					chainBaseIndex = currentNode->m_jointIndex;
					break;
				}
				// la joint correspondiente a la base de la cadena no se guarda para ser modificada mediante IK
				// , ya que esta es una articulacion que se considera fija
				ikChain.m_joints.insert(ikChain.m_joints.begin(), currentNode->m_jointIndex);
				currentNode = currentNode->m_parent;
			}
			if (chainBaseIndex == -1) {
				MONA_LOG_ERROR("IKRig: base joint and end effector were not on the same chain!");
			}
		}
		else {
			MONA_LOG_ERROR("IKRig: Did not find an end effector named {0}", chainEnds.endEffectorName);
		}
		return ikChain;
	}

	IKChain IKRig::buildHipIKChain(std::string hipJointName) {
		IKChain ikChain;
		auto jointNames = getJointNames();
		JointIndex hipInd = funcUtils::findIndex(jointNames, hipJointName);
		if (hipInd == -1) {
			MONA_LOG_ERROR("IKRig: Input hip joint name was not a correct joint name.");
			return ikChain;
		}
		ikChain.m_name = hipJointName;
		ikChain.m_joints = { hipInd };
		return ikChain;
	}

	void IKRig::UpdateEETrajectories(float timeStep, glm::vec3 rawLinVel, glm::vec3 rawAngVel) {
		float time = m_animationController->m_sampleTime + timeStep * m_animationController->m_playRate;

		// crear nuevas curvas si
		//	se acaba el rango de tiempo de las curvas actuales
		//	hay un cambio en la velocidad angular

		// valores que se deben guardar
		//	�ltimo frame de la animacion para el que se realiz� un ajuste IK
		//	�ltimos valores de velocidad lineal y angular

	}

}
