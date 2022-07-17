#include "IKRigBase.hpp"
#include "Kinematics.hpp"

namespace Mona{

	IKRigConfig::IKRigConfig(std::shared_ptr<AnimationClip> animation, AnimationIndex animationIndex, ForwardKinematics* forwardKinematics) {
		m_animationClip = animation;
		int frameNum = animation->m_animationTracks[0].rotationTimeStamps.size();
		int jointNum = animation->m_animationTracks.size();
		m_jointPositions.reserve(jointNum);
		m_jointScales.reserve(jointNum);
		m_timeStamps = animation->m_animationTracks[0].rotationTimeStamps;
		m_baseJointRotations.reserve(frameNum);
		for (int i = 0; i < jointNum;i++) {
			m_jointPositions.push_back(animation->m_animationTracks[i].positions[0]);
			m_jointScales.push_back(animation->m_animationTracks[i].scales[0]);
		}
		for (int i = 0; i < frameNum; i++) {
			m_baseJointRotations.push_back(std::vector<JointRotation>(jointNum));
			for (int j = 0; j < jointNum; j++) {
				m_baseJointRotations[i][j] = JointRotation(animation->m_animationTracks[j].rotations[i]);
			}
		}
		m_dynamicJointRotations = m_baseJointRotations;
		m_animIndex = animationIndex;
		m_forwardKinematics = forwardKinematics;
	}

	std::vector<glm::mat4> IKRigConfig::getModelSpaceTransforms(bool useDynamicRotations) {
		return m_forwardKinematics->ModelSpaceTransforms(m_animIndex, m_currentFrameIndex, useDynamicRotations);
	}
	glm::mat4 IKRigConfig::getModelSpaceTransform(JointIndex jointIndex, FrameIndex frame, bool useDynamicRotations) {
		return m_forwardKinematics->ModelSpaceTransform(m_animIndex, jointIndex, frame, useDynamicRotations);
	}
	std::vector<glm::vec3> IKRigConfig::getModelSpacePositions(bool useDynamicRotations) {
		return m_forwardKinematics->ModelSpacePositions(m_animIndex, m_currentFrameIndex,useDynamicRotations);
	}
	std::vector<glm::vec3> IKRigConfig::getModelSpacePositions(FrameIndex frame, bool useDynamicRotations) {
		return m_forwardKinematics->ModelSpacePositions(m_animIndex, frame, useDynamicRotations);
	}
	std::vector<glm::vec3> IKRigConfig::getCustomSpacePositions(glm::mat4 baseTransform, bool useDynamicRotations) {
		return m_forwardKinematics->CustomSpacePositions(baseTransform, m_animIndex, m_currentFrameIndex, useDynamicRotations);
	}
	std::vector<glm::mat4> IKRigConfig::getJointSpaceTransforms(bool useDynamicRotations) {
		return m_forwardKinematics->JointSpaceTransforms(m_animIndex, m_currentFrameIndex, useDynamicRotations);
	}

	float IKRigConfig::getReproductionTime(FrameIndex frame, int repCountOffset) {
		MONA_ASSERT(0 <= frame && frame < m_timeStamps.size(), "IKRigConfig: FrameIndex outside of range.");
		return (m_reproductionCount + repCountOffset) * m_animationClip->GetDuration() - m_timeStamps[0] + m_timeStamps[frame];
	}

	float IKRigConfig::adjustAnimationTime(float animationTime) {
		if (animationTime < m_timeStamps[0]) {
			while (animationTime < m_timeStamps[0]) {
				animationTime += getAnimationDuration();
			}
		}
		else if (m_timeStamps[0] + getAnimationDuration() < animationTime) {
			while (m_timeStamps[0] + getAnimationDuration() < animationTime) {
				animationTime -= getAnimationDuration();
			}
		}
		return animationTime;
	}

	float IKRigConfig::getReproductionTime(float animationTime, int repCountOffset) {
		// llevar el tiempo al rango correcto
		animationTime = adjustAnimationTime(animationTime);
		return (m_reproductionCount + repCountOffset) * m_animationClip->GetDuration() - m_timeStamps[0] + animationTime;
	}

	float IKRigConfig::getAnimationTime(FrameIndex frame) { 
		MONA_ASSERT(0 <= frame && frame < m_timeStamps.size(), "IKRigConfig: FrameIndex outside of range.");
		return m_timeStamps[frame]; 
	}

	float IKRigConfig::getAnimationTime(float reproductionTime) {
		if (0 <= reproductionTime) {
			return fmod(reproductionTime, m_animationClip->GetDuration()) + m_timeStamps[0];
		}
		else {
			while (reproductionTime < 0) {
				reproductionTime += m_animationClip->GetDuration();
			}
			return fmod(reproductionTime, m_animationClip->GetDuration()) + m_timeStamps[0];
		}
	}

	FrameIndex IKRigConfig::getFrame(float animationTime) {
		// llevar el tiempo al rango correcto
		animationTime = adjustAnimationTime(animationTime);
		for (FrameIndex i = 0; i < m_timeStamps.size()-1; i++) {
			if (m_timeStamps[i] <= animationTime && animationTime < m_timeStamps[i + 1]) {
				return i;
			}
		}
		return -1;
	}

	IKNode::IKNode(std::string jointName, JointIndex jointIndex, IKNode* parent, float weight) {
		m_jointName = jointName;
		m_jointIndex = jointIndex;
		m_parent = parent;
		m_weight = weight;
	}

	void RigData::setJointData(std::string jointName, float minAngle, float maxAngle, float weight, bool enableData) {
		if (jointName == "") {
			MONA_LOG_ERROR("RigData: jointName cannot be empty string.");
			return;
		}
		if (minAngle <= maxAngle) {
			MONA_LOG_ERROR("RigData: maxAngle must be equal or greater than minAngle.");
			return;
		}
		jointData[jointName].minAngle = minAngle;
		jointData[jointName].maxAngle = maxAngle;
		jointData[jointName].weight = weight;
		jointData[jointName].enableIKRotation = enableData;
	}

	JointData RigData::getJointData(std::string jointName) {
		return JointData(jointData[jointName]);
	}
	void RigData::enableJointData(std::string jointName, bool enableData) {
		jointData[jointName].enableIKRotation = enableData;
	}

	bool RigData::isValid() {
		if (leftLeg.baseJointName.empty() || leftLeg.endEffectorName.empty() || rightLeg.baseJointName.empty() || rightLeg.endEffectorName.empty()) {
			MONA_LOG_ERROR("RigData: Legs cannot be empty");
			return false;
		}
		return true;
	}

	JointRotation::JointRotation() {
		setRotation(glm::identity<glm::fquat>());
	}
	JointRotation::JointRotation(float rotationAngle, glm::vec3 rotationAxis) {
		setRotation(rotationAngle, rotationAxis);
	}
	JointRotation::JointRotation(glm::fquat quatRotation) {
		setRotation(quatRotation);
	}
	void JointRotation::setRotation(glm::fquat rotation) {
		m_quatRotation = rotation;
		m_rotationAngle = glm::angle(m_quatRotation);
		m_rotationAxis = glm::axis(m_quatRotation);
	}
	void JointRotation::setRotation(float rotationAngle, glm::vec3 rotationAxis) {
		m_quatRotation = glm::angleAxis(rotationAngle, rotationAxis);
		m_rotationAngle = rotationAngle;
		m_rotationAxis = rotationAxis;
	}

	void JointRotation::setRotationAngle(float rotationAngle) {
		m_quatRotation = glm::angleAxis(rotationAngle, m_rotationAxis);
		m_rotationAngle = rotationAngle;
	}

	void JointRotation::setRotationAxis(glm::vec3 rotationAxis) {
		m_quatRotation = glm::angleAxis(m_rotationAngle, rotationAxis);
		m_rotationAxis = rotationAxis;
	}

	EETrajectory::EETrajectory(LIC<3> trajectory, TrajectoryType trajectoryType) {
		m_trajectory = trajectory;
		m_trajectoryType = trajectoryType;
	}

	EETrajectory EEGlobalTrajectoryData::getSubTrajectory(float animationTime) {
		for (int i = 0; i < m_originalSubTrajectories.size(); i++) {
			if (m_originalSubTrajectories[i].getEETrajectory().inOpenRightTRange(animationTime)) {
				return m_originalSubTrajectories[i];
			}
		}
		MONA_LOG_ERROR("EETrajectoryData: AnimationTime was not valid.");
		return EETrajectory();
	}
    
}