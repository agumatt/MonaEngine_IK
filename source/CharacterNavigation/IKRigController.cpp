#include "IKRigController.hpp"
#include "../Core/FuncUtils.hpp"
#include "../Core/GlmUtils.hpp"
#include "glm/gtx/rotate_vector.hpp"

namespace Mona {

	

	IKRigController::IKRigController(std::shared_ptr<Skeleton> skeleton, RigData rigData, InnerComponentHandle transformHandle,
		InnerComponentHandle skeletalMeshHandle, ComponentManager<TransformComponent>* transformManager):
		m_skeletalMeshHandle(skeletalMeshHandle), m_ikRig(skeleton, rigData, transformHandle) {
		glm::mat4 baseGlobalTransform = transformManager->GetComponentPointer(transformHandle)->GetModelMatrix();
		glm::vec3 glblScale; glm::quat glblRotation; glm::vec3 glblTranslation;	glm::vec3 glblSkew;	glm::vec4 glblPerspective;
		glm::decompose(baseGlobalTransform, glblScale, glblRotation, glblTranslation, glblSkew, glblPerspective);
		MONA_ASSERT(glmUtils::isApproxUniform(glblScale), "Global scale must be uniform");
		m_baseGlobalTransform = baseGlobalTransform;
		m_ikRig.m_rigScale = glblScale[0];
		// descartamos la rotacion de la transformacion base
		transformManager->GetComponentPointer(transformHandle)->SetRotation(glm::identity<glm::fquat>());
	}
	void IKRigController::init() {
		m_ikRig.init();
	}

	void IKRigController::validateTerrains(ComponentManager<StaticMeshComponent>& staticMeshManager) {
		m_ikRig.m_trajectoryGenerator.m_environmentData.validateTerrains(staticMeshManager);
	}

	void IKRigController::addAnimation(std::shared_ptr<AnimationClip> animationClip, AnimationType animationType) {
		// la animacion debe tener globalmente vector front={0,1,0} y up={0,0,1}
		MONA_ASSERT(animationClip->GetSkeleton() == m_ikRig.m_skeleton,
			"IKRigController: Input animation does not correspond to base skeleton.");
		for (int i = 0; i < m_ikRig.m_animationConfigs.size(); i++) {
			if (m_ikRig.m_animationConfigs[i].m_animationClip->GetAnimationName() == animationClip->GetAnimationName()) {
				MONA_LOG_WARNING("IKRigController: Animation {0} for model {1} had already been added",
					animationClip->GetAnimationName(), m_ikRig.m_skeleton->GetModelName());
				return;
			}
		}
		MONA_ASSERT(animationClip->GetTrackIndex(m_ikRig.m_hipJoint) != -1, "IKRigController: Hip joint must be present in input animationClip.");
		for (int i = 0; i < m_ikRig.m_ikChains.size(); i++) {
			JointIndex eeIndex = m_ikRig.m_ikChains[i].getEndEffector();
			MONA_ASSERT(animationClip->GetTrackIndex(eeIndex) != -1,
				"IKRigController: Set end effector {0} must be present in input animationClip.",
				m_ikRig.getJointNames()[eeIndex]);
		}
		
		// Chequar si los escalamientos y traslaciones son constantes por joint.
		// La cadera si puede tener traslaciones variables
		for (int i = 0; i < animationClip->m_animationTracks.size(); i++) {
			auto track = animationClip->m_animationTracks[i];
			JointIndex jIndex = animationClip->m_trackJointIndices[i];
			glm::vec3 basePosition = track.positions[0];
			glm::vec3 baseScale = track.scales[0];
			for (int j = 1; j < track.positions.size(); j++) {
				MONA_ASSERT(glmUtils::areApproxEqual(track.positions[j], basePosition) || jIndex == m_ikRig.m_hipJoint,
					"IKRigController: Jointa other than the hip must have fixed translations.");
			}
			for (int j = 1; j < track.scales.size(); j++) {
				MONA_ASSERT(glmUtils::areApproxEqual(track.scales[j], baseScale),
					"IKRigController: Animation must have fixed scales per joint.");
			}
		}

		JointIndex parent = m_ikRig.getTopology()[m_ikRig.m_hipJoint];
		while (parent != -1) {
			int trackIndex = animationClip->GetTrackIndex(parent);
			auto track = animationClip->m_animationTracks[trackIndex];
			for (int j = 0; j < track.rotations.size(); j++) {
				MONA_ASSERT(track.rotations[j] == glm::identity<glm::fquat>(),
					"IKRigController: Joints above the hip in the hierarchy cannot have rotations.");
			}
			for (int j = 0; j < track.positions.size(); j++) {
				MONA_ASSERT(track.positions[j] == glm::vec3(0),
					"IKRigController: Joints above the hip in the hierarchy cannot have translations.");
			}
		}

		// Transformacion base
		glm::vec3 baseScale; glm::quat baseRotation; glm::vec3 baseTranslation; glm::vec3 baseSkew; glm::vec4 basePerspective;
		glm::decompose(m_baseGlobalTransform, baseScale, baseRotation, baseTranslation, baseSkew, basePerspective);

		// Descomprimimos las rotaciones de la animacion, repitiendo valores para que todas las articulaciones 
		// tengan el mismo numero de rotaciones
		animationClip->DecompressRotations();

		// Para este paso las rotaciones deben estar descomprimidas
		AnimationIndex newIndex = m_ikRig.m_animationConfigs.size();
		m_ikRig.m_animationConfigs.push_back(IKRigConfig(animationClip, animationType, newIndex, &m_ikRig.m_forwardKinematics));
		IKRigConfig* currentConfig = m_ikRig.getAnimationConfig(newIndex);
		currentConfig->m_eeTrajectoryData = std::vector<EEGlobalTrajectoryData>(m_ikRig.m_ikChains.size());

		for (ChainIndex i = 0; i < m_ikRig.getChainNum(); i++) {
			checkLegRotationAxes(currentConfig, i, baseRotation);
		}

		int chainNum = m_ikRig.getChainNum();

		// numero de rotaciones por joint con la animaciond descomprimida
		int frameNum = animationClip->m_animationTracks[0].rotationTimeStamps.size();

			

		// Guardamos las trayectorias originales de los ee y definimos sus frames de soporte
		std::vector<float> rotTimeStamps = animationClip->m_animationTracks[0].rotationTimeStamps;
		std::vector<std::vector<bool>> supportFramesPerChain(chainNum);
		std::vector<std::vector<glm::vec3>> glblPositionsPerChain(chainNum);
		std::vector<glm::vec3> glblPositions(m_ikRig.getTopology().size());
		std::vector<glm::mat4> glblTransforms(m_ikRig.getTopology().size());
		std::vector<glm::vec3> hipGlblPositions;
		float floorZ = std::numeric_limits<float>::max(); // altura del piso para la animacion
		for (int i = 0; i < m_ikRig.getTopology().size(); i++) { glblTransforms[i] = glm::identity<glm::mat4>(); }
		std::vector<glm::vec3> previousPositions(m_ikRig.getTopology().size());
		std::fill(previousPositions.begin(), previousPositions.end(), glm::vec3(std::numeric_limits<float>::lowest()));
		for (ChainIndex i = 0; i < chainNum; i++) {
			supportFramesPerChain[i] = std::vector<bool>(frameNum);
			glblPositionsPerChain[i] = std::vector<glm::vec3>(frameNum);
		}
		for (FrameIndex i = 0; i < frameNum; i++) {
			// calculo de las transformaciones
			float timeStamp = rotTimeStamps[i];
			while (currentConfig->getAnimationDuration() <= timeStamp) { timeStamp -= 0.000001; }
			for (int j = 0; j < currentConfig->getJointIndices().size(); j++) {
				JointIndex jIndex = currentConfig->getJointIndices()[j];
				glm::mat4 baseTransform = j == 0 ? m_baseGlobalTransform : glblTransforms[m_ikRig.getTopology()[jIndex]];
				glblTransforms[jIndex] = baseTransform *
					glmUtils::translationToMat4(animationClip->GetPosition(timeStamp, jIndex, true)) *
					glmUtils::rotationToMat4(animationClip->GetRotation(timeStamp, jIndex, true)) *
					glmUtils::scaleToMat4(animationClip->GetScale(timeStamp, jIndex, true));
			}

			// calculo de las posiciones
			for (int j = 0; j < glblPositions.size(); j++) {
				glblPositions[j] = glblTransforms[j] * glm::vec4(0, 0, 0, 1);
			}
			hipGlblPositions.push_back(glblPositions[m_ikRig.m_hipJoint]);
			// distancia base entre puntos para identificacion de puntos de soporte
			float minDistance = m_ikRig.m_rigHeight * m_ikRig.m_rigScale / 1000;
			for (ChainIndex j = 0; j < chainNum; j++) {
				int eeIndex = m_ikRig.m_ikChains[j].getEndEffector();
				bool isSupportFrame = glm::distance(glblPositions[eeIndex], previousPositions[eeIndex]) <= minDistance*15;
				supportFramesPerChain[j][i] = isSupportFrame;
				glblPositionsPerChain[j][i] = glm::vec4(glblPositions[eeIndex], 1);
			}
			for (int j = 0; j < currentConfig->getJointIndices().size(); j++) {
				JointIndex jIndex = currentConfig->getJointIndices()[j];
				if (glblPositions[jIndex][2] < floorZ) {
					floorZ = glblPositions[jIndex][2];
				}
			}
			previousPositions = glblPositions;
		}
		// ajuste de las alturas con el suelo
		for (ChainIndex i = 0; i < chainNum; i++) {
			for (FrameIndex j = 0; j < frameNum; j++) {
				glblPositionsPerChain[i][j][2] -= floorZ;
			}
		}
		for (FrameIndex i = 0; i < frameNum; i++) {
			hipGlblPositions[i][2] -= floorZ;
		}

		// Guardamos la informacion de traslacion y rotacion de la cadera, antes de eliminarla
		TrajectoryGenerator::buildHipTrajectory(currentConfig, hipGlblPositions);
		
		// si hay un frame que no es de soporte entre dos frames que si lo son, se setea como de soporte
		// si el penultimo es de soporte, tambien se setea el ultimo como de soporte
		// a los valores de soporte del primer frame les asignamos el valor del ultimo asumiento circularidad
		for (int i = 0; i < chainNum; i++) {
			if (supportFramesPerChain[i][frameNum - 2]) {
				supportFramesPerChain[i].back() = true;
			}
			supportFramesPerChain[i][0] = supportFramesPerChain[i].back();
			for (int j = 1; j < frameNum - 1; j++) {
				if (supportFramesPerChain[i][j - 1] && supportFramesPerChain[i][j + 1]) {
					supportFramesPerChain[i][j] = true;
				}
			}			
		}
		for (int i = 0; i < chainNum; i++) {
			supportFramesPerChain[i][0] = supportFramesPerChain[i].back();
		}

		// se ajustan los frames de soporte entre cadenas opuestas (izquierda y derecha)
		std::vector<ChainIndex> pairedChains;
		for (ChainIndex i = 0; i < m_ikRig.m_ikChains.size(); i++) {
			IKChain currChain = m_ikRig.m_ikChains[i];
			if (funcUtils::findIndex(pairedChains, i) == -1) {
				for (FrameIndex j = 0; j < frameNum; j++) {
					supportFramesPerChain[currChain.getOpposite()][j] = !supportFramesPerChain[i][j];
				}
				pairedChains.push_back(i);
				pairedChains.push_back(currChain.getOpposite());
			}
		}

		// dividimos cada trayectoria global (por ee) en sub trayectorias dinamicas y estaticas.
		std::vector<ChainIndex> oppositePerChain;
		for (ChainIndex i = 0; i < chainNum; i++) {
			ChainIndex opposite = m_ikRig.getIKChain(i)->getOpposite();
			oppositePerChain.push_back(opposite);
		}
		TrajectoryGenerator::buildEETrajectories(currentConfig, supportFramesPerChain, glblPositionsPerChain, oppositePerChain);

		// Se remueve el movimiento de las caderas y se aplica la rotacion basal
		animationClip->RemoveJointTranslation(m_ikRig.m_hipJoint);
		int hipTrackIndex = animationClip->GetTrackIndex(m_ikRig.m_hipJoint);
		for (FrameIndex i = 0; i < currentConfig->getFrameNum(); i++) {
			glm::fquat origRotation = animationClip->m_animationTracks[hipTrackIndex].rotations[i];
			glm::fquat updatedRotation = baseRotation * origRotation;
			currentConfig->m_baseJointRotations[i][m_ikRig.m_hipJoint] = JointRotation(updatedRotation);
			currentConfig->m_dynamicJointRotations[i][m_ikRig.m_hipJoint] = JointRotation(updatedRotation);			
			animationClip->SetRotation(updatedRotation, i, m_ikRig.m_hipJoint);
		}
		currentConfig->m_jointPositions[m_ikRig.m_hipJoint] = glm::vec3(0);
	}

	AnimationIndex IKRigController::removeAnimation(std::shared_ptr<AnimationClip> animationClip) {
		for (int i = 0; i < m_ikRig.m_animationConfigs.size(); i++) {
			if (m_ikRig.m_animationConfigs[i].m_animationClip == animationClip) {
				m_ikRig.resetAnimation(i);
				m_ikRig.m_animationConfigs.erase(m_ikRig.m_animationConfigs.begin() + i);
				return i;
			}
		}
		return -1;
	}

	void IKRigController::updateMovementDirection(float timeStep) {
		m_ikRig.m_rotationAngle += m_ikRig.m_angularSpeed * timeStep;
	}

	void IKRigController::updateTrajectories(AnimationIndex animIndex, ComponentManager<TransformComponent>& transformManager,
		ComponentManager<StaticMeshComponent>& staticMeshManager) {
		IKRigConfig& config = m_ikRig.m_animationConfigs[animIndex];
		HipGlobalTrajectoryData* hipTrData = config.getHipTrajectoryData();
		FrameIndex currentFrame = config.getCurrentFrameIndex();
		float currentRepTime = config.getReproductionTime(currentFrame);
		float avgFrameDuration = config.getAnimationDuration() / config.getFrameNum();
		EEGlobalTrajectoryData* trData;
		if (config.m_onNewFrame) { // se realiza al llegar a un frame de la animacion
		// guardado de posiciones globales ee y cadera
			std::vector<JointIndex> endEffectors;
			for (ChainIndex i = 0; i < m_ikRig.getChainNum(); i++) {
				endEffectors.push_back(m_ikRig.m_ikChains[i].getEndEffector());
			}
			glm::mat4 baseTransform = transformManager.GetComponentPointer(m_ikRig.getTransformHandle())->GetModelMatrix();
			std::vector<glm::mat4> globalTransforms = config.getEEListCustomSpaceTransforms(endEffectors, baseTransform, currentFrame, true);
			for (ChainIndex i = 0; i < m_ikRig.getChainNum(); i++) {
				trData = config.getEETrajectoryData(i);
				JointIndex ee = endEffectors[i];
				glm::vec3 eePos = globalTransforms[ee] * glm::vec4(0, 0, 0, 1);
				// si no hay posiciones guardadas
				if (trData->m_savedPositions.getNumberOfPoints() == 0 || m_transitioning) {
					trData->m_savedPositions = LIC<3>({ eePos, eePos }, { currentRepTime - config.getAnimationDuration(), currentRepTime });
					trData->m_motionInitialized = false;
				}
				else {
					trData->m_savedPositions.insertPoint(eePos, currentRepTime);
					// recorte de las posiciones guardadas
					for (int j = trData->m_savedPositions.getNumberOfPoints() - 1; 0 <= j; j--) {
						float tVal = trData->m_savedPositions.getTValue(j);
						float minVal = trData->m_savedPositions.getTRange()[1] - config.getAnimationDuration() * 2;
						if (tVal < minVal) {
							trData->m_savedPositions = trData->m_savedPositions.sample(tVal, trData->m_savedPositions.getTRange()[1]);
							break;
						}
					}
				}
			}
			glm::mat4 hipTransform = globalTransforms[m_ikRig.m_hipJoint];
			glm::vec3 hipScale; glm::fquat hipRot; glm::vec3 hipTrans; glm::vec3 hipSkew; glm::vec4 hipPers;
			glm::decompose(hipTransform, hipScale, hipRot, hipTrans, hipSkew, hipPers);
			
			// si no hay posiciones guardadas
			if (hipTrData->m_savedPositions.getNumberOfPoints() == 0 || m_transitioning) {
				hipTrData->m_savedPositions = LIC<3>({ hipTrans, hipTrans },{ currentRepTime - config.getAnimationDuration(), currentRepTime });
				hipTrData->m_motionInitialized = false;
			}
			else {
				hipTrData->m_savedPositions.insertPoint(hipTrans, currentRepTime);
				// recorte de las posiciones guardadas
				for (int j = hipTrData->m_savedPositions.getNumberOfPoints() - 1; 0 <= j; j--) {
					float tVal = hipTrData->m_savedPositions.getTValue(j);
					float minVal = hipTrData->m_savedPositions.getTRange()[1] - config.getAnimationDuration() * 2;
					if (tVal < minVal) {
						hipTrData->m_savedPositions = hipTrData->m_savedPositions.sample(tVal,	hipTrData->m_savedPositions.getTRange()[1]);
						break;
					}
				}
			}

			// recalcular trayectorias de ee y caderas
			m_ikRig.calculateTrajectories(animIndex, transformManager, staticMeshManager);

			// ya que no hay info de posicion guardada al comenzar el movimiento, se rellena con la curva objetivo
			if (!config.isMovementFixed()) {
				for (ChainIndex i = 0; i < m_ikRig.getChainNum(); i++) {
					trData = config.getEETrajectoryData(i);
					if (!trData->m_motionInitialized) {
						LIC<3> targetCurve = trData->getTargetTrajectory().getEECurve();
						for (int j = 0; j < targetCurve.getNumberOfPoints(); j++) {
							if (targetCurve.getTValue(j) < currentRepTime - avgFrameDuration) {
								trData->m_savedPositions.insertPoint(targetCurve.getCurvePoint(j), targetCurve.getTValue(j));
							}
						}
						trData->m_motionInitialized = true;
					}
				}
				if (!hipTrData->m_motionInitialized) {
					LIC<3> targetCurve = hipTrData->getTargetPositions();
					for (int j = 0; j < targetCurve.getNumberOfPoints(); j++) {
						if (targetCurve.getTValue(j) < currentRepTime - avgFrameDuration) {
							hipTrData->m_savedPositions.insertPoint(targetCurve.getCurvePoint(j), targetCurve.getTValue(j));
						}
					}
					hipTrData->m_motionInitialized = true;
				}
			}			

			// asignar objetivos a ee's
			int repOffset_next = config.getCurrentFrameIndex() < config.getFrameNum() - 1 ? 0 : 1;
			float targetTimeNext = config.getReproductionTime(config.getNextFrameIndex(), repOffset_next);
			float targetTimeCurr = config.getReproductionTime(config.getCurrentFrameIndex());
			float deltaT = targetTimeNext - targetTimeCurr;

			glm::mat4 nextGlblTransform = glmUtils::translationToMat4(hipTrData->getTargetPositions().evalCurve(targetTimeNext)) *
				glmUtils::rotationToMat4(glm::angleAxis(m_ikRig.m_rotationAngle + m_ikRig.m_angularSpeed*deltaT, m_ikRig.getUpVector())) *
				glmUtils::scaleToMat4(glm::vec3(m_ikRig.m_rigScale));
			glm::mat4 toModelSpace = glm::inverse(nextGlblTransform);
			for (ChainIndex i = 0; i < m_ikRig.getChainNum(); i++) {
				IKChain* ikChain = m_ikRig.getIKChain(i);
				trData = config.getEETrajectoryData(i);
				glm::vec3 eeTarget = toModelSpace *glm::vec4(trData->getTargetTrajectory().getEECurve().evalCurve(targetTimeNext), 1);
				ikChain->setCurrentEETarget(eeTarget);
			}
		}
		
		
	}

	void IKRigController::updateGlobalTransform(ComponentManager<TransformComponent>& transformManager) {
		// setear transformacion global (traslacion y direccion de movimiento)
		glm::fquat updatedRotation = glm::angleAxis(m_ikRig.m_rotationAngle, m_ikRig.getUpVector());
		transformManager.GetComponentPointer(m_ikRig.getTransformHandle())->SetRotation(updatedRotation);
		
		glm::vec3 newGlobalPosition(0);
		int activeConfigs = 0;
		for (AnimationIndex i = 0; i < m_ikRig.m_animationConfigs.size(); i++) {
			IKRigConfig& config = m_ikRig.m_animationConfigs[i];
			if (config.isActive()) {
				HipGlobalTrajectoryData* hipTrData = config.getHipTrajectoryData();
				if (hipTrData->getTargetPositions().inTRange(config.getCurrentReproductionTime())) {
					newGlobalPosition += hipTrData->getTargetPositions().evalCurve(config.getCurrentReproductionTime());
					activeConfigs += 1;
				}
			}
		}
		if (0 < activeConfigs) {
			newGlobalPosition /= activeConfigs;
			transformManager.GetComponentPointer(m_ikRig.getTransformHandle())->SetTranslation(newGlobalPosition);
		}
	}


	std::vector<FrameIndex> last2UpdatedFrames = { -1,-1 };
	void IKRigController::updateAnimation(AnimationIndex animIndex) {
		IKRigConfig& config = m_ikRig.m_animationConfigs[animIndex];
		if (config.m_onNewFrame) {
			FrameIndex currFrame = config.getCurrentFrameIndex();
			FrameIndex nextFrame = config.getNextFrameIndex();
			// calcular nuevas rotaciones para la animacion con ik
			std::vector<std::pair<JointIndex, glm::fquat>> calculatedRotations = m_ikRig.calculateRotations(animIndex, nextFrame);
			auto anim = config.m_animationClip;
			for (int i = 0; i < calculatedRotations.size(); i++) {
				anim->SetRotation(calculatedRotations[i].second, nextFrame, calculatedRotations[i].first);
				// para compensar el poco espacio entre en ultimo y el primer frame
				if (nextFrame == config.getFrameNum() - 1) {
					anim->SetRotation(calculatedRotations[i].second, 0, calculatedRotations[i].first);
				}
				// si el current frame no fue actualizado, le asignamos el valor calculado para next frame
				if (last2UpdatedFrames[0] != currFrame && last2UpdatedFrames[1] != currFrame) {
					anim->SetRotation(calculatedRotations[i].second, currFrame, calculatedRotations[i].first);
				}
			}
			last2UpdatedFrames[0] = nextFrame;
			if (nextFrame == config.getFrameNum() - 1) {
				last2UpdatedFrames[1] = 0;
			}
			else {
				last2UpdatedFrames[1] = -1;
			}
			
		}
		
	}

	void IKRigController::updateIKRigConfigTime(float animationTimeStep, AnimationIndex animIndex, AnimationController& animController) {
		IKRigConfig& config = m_ikRig.m_animationConfigs[animIndex];
		float avgFrameDuration = config.getAnimationDuration() / config.getFrameNum();
		std::shared_ptr<AnimationClip> anim = config.m_animationClip;
		float prevSamplingTime = anim->GetSamplingTime(m_reproductionTime - animationTimeStep, true);
		float samplingTimeOffset = 0.0f;
		if (animController.m_animationClipPtr == anim) {
			samplingTimeOffset = animController.m_sampleTime - prevSamplingTime;
		}
		else if (animController.m_crossfadeTarget.GetAnimationClip() == anim) {
			samplingTimeOffset = animController.m_crossfadeTarget.m_sampleTime - prevSamplingTime;
		}
		if ( samplingTimeOffset + avgFrameDuration / 10.0f < 0) {
			samplingTimeOffset += config.getAnimationDuration();
		}
		
		float adjustedReproductionTime = m_reproductionTime + samplingTimeOffset;
		config.m_currentReproductionTime = adjustedReproductionTime;
		config.m_reproductionCount = config.m_currentReproductionTime / config.getAnimationDuration();
		float adjustedSamplingTime = anim->GetSamplingTime(config.m_currentReproductionTime, true);
		for (int i = 0; i < config.m_timeStamps.size(); i++) {
			float nextTimeStamp = i < config.m_timeStamps.size()-1 ? config.m_timeStamps[i + 1] : config.getAnimationDuration();
			if (config.m_timeStamps[i] <= adjustedSamplingTime && adjustedSamplingTime < nextTimeStamp) {
				config.m_onNewFrame = config.m_currentFrameIndex != i;
				config.m_currentFrameIndex = i;
				config.m_nextFrameIndex = (i + 1) % (config.m_timeStamps.size());
				break;
			}
		}
		
	}

	void IKRigController::clearConfig(IKRigConfig& config) {
		config.clear();
		m_ikRig.resetAnimation(config.m_animIndex);
	}

	void IKRigController::updateIKRig(float timeStep, ComponentManager<TransformComponent>& transformManager,
		ComponentManager<StaticMeshComponent>& staticMeshManager, ComponentManager<SkeletalMeshComponent>& skeletalMeshManager) {
		validateTerrains(staticMeshManager);
		AnimationController& animController = skeletalMeshManager.GetComponentPointer(m_skeletalMeshHandle)->GetAnimationController();
		float animTimeStep = timeStep * animController.GetPlayRate();
		m_reproductionTime += animTimeStep;
		for (AnimationIndex i = 0; i < m_ikRig.m_animationConfigs.size(); i++) {
			updateIKRigConfigTime(animTimeStep, i, animController);
		}
		updateMovementDirection(animTimeStep);
		int activeAnimations = 0;
		for (AnimationIndex i = 0; i < m_ikRig.m_animationConfigs.size(); i++) {
			IKRigConfig& config = m_ikRig.m_animationConfigs[i];
			if (animController.m_animationClipPtr == config.m_animationClip ||
				animController.m_crossfadeTarget.GetAnimationClip() == config.m_animationClip) {
				config.m_active = true;
				activeAnimations += 1;
			}
			else {
				config.m_active = false;
				clearConfig(config);
			}
		}
		m_transitioning = activeAnimations == 2;
		for (AnimationIndex i = 0; i < m_ikRig.m_animationConfigs.size(); i++) {
			if (m_ikRig.m_animationConfigs[i].isActive()) {
				updateTrajectories(i, transformManager, staticMeshManager);			
			}
		}
		updateGlobalTransform(transformManager);
		if (m_ikEnabled) {
			for (AnimationIndex i = 0; i < m_ikRig.m_animationConfigs.size(); i++) {
				IKRigConfig& config = m_ikRig.m_animationConfigs[i];
				if (config.isActive()) {
					if (config.getAnimationType() == AnimationType::WALKING) {
						if (!config.isMovementFixed()) {
							updateAnimation(i);
						}
					}
					else if (config.getAnimationType() == AnimationType::IDLE) {
						updateAnimation(i);
					}
				}
			}
		}		

		for (AnimationIndex i = 0; i < m_ikRig.m_animationConfigs.size(); i++) {
			IKRigConfig& config = m_ikRig.m_animationConfigs[i];
			config.m_onNewFrame = false;
		}

	}

	void IKRigController::enableIK(bool enableIK) {
		if (!enableIK) {
			for (AnimationIndex i = 0; i < m_ikRig.m_animationConfigs.size(); i++) {
				m_ikRig.resetAnimation(i);
			}
		}
		m_ikEnabled = enableIK;		
	}


	void IKRigController::checkLegRotationAxes(IKRigConfig* config, ChainIndex legChainIndex, glm::fquat baseRotation) {
		std::shared_ptr<AnimationClip> anim = config->m_animationClip;
		std::vector<JointIndex>const& topology = m_ikRig.getTopology();
		IKChain chain = m_ikRig.m_ikChains[legChainIndex];
		for (int i = 0; i < chain.getJoints().size()-1; i++) {
			std::vector<glm::fquat> globalRotations;
			JointIndex jIndex = chain.getJoints()[i];
			for (FrameIndex j = 0; j < config->getFrameNum(); j++) {
				glm::fquat globalRotation = anim->m_animationTracks[anim->GetTrackIndex(jIndex)].rotations[j];
				JointIndex parent = topology[jIndex];
				while (parent != -1) {
					globalRotation = anim->m_animationTracks[anim->GetTrackIndex(parent)].rotations[j] * globalRotation;
					parent = topology[parent];
				}
				globalRotation = baseRotation * globalRotation;
				globalRotations.push_back(globalRotation);
			}

			// buscamos el eje de rotacion global principal
			glm::vec3 meanRotationAxis(0);
			for (FrameIndex j = 0; j < config->getFrameNum(); j++) {
				meanRotationAxis += glm::axis(globalRotations[j]);
			
			}
			meanRotationAxis /= config->getFrameNum();
			
			MONA_ASSERT(0.8f < abs(meanRotationAxis[0]), "IKRigController: Leg joints must have an x main rotation axis globally.");
			MONA_ASSERT(abs(meanRotationAxis[1]) < 0.3f, "IKRigController: Leg joints must have an x main rotation axis globally.");
			MONA_ASSERT(abs(meanRotationAxis[2]) < 0.3f, "IKRigController: Leg joints must have an x main rotation axis globally.");

		}

	}









}