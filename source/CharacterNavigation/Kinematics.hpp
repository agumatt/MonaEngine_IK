#pragma once
#ifndef KINEMATICS_HPP
#define KINEMATICS_HPP

#include <vector>
#include <functional>
#include "IKRig.hpp"
#include "GradientDescent.hpp"
#include "Splines.hpp"

namespace Mona {
	class ForwardKinematics {
		IKRig* m_ikRig;
	public:
		ForwardKinematics(IKRig* ikRig);
		ForwardKinematics() = default;
		std::vector<glm::vec3> ModelSpacePositions(AnimationIndex animIndex, bool useDynamicRotations);
		glm::vec3 ModelSpacePosition(AnimationIndex animIndex, JointIndex jointIndex, bool useDynamicRotations);
		std::vector<glm::mat4> ModelSpaceTransforms(AnimationIndex animIndex, bool useDynamicRotations);
		std::vector<glm::mat4> JointSpaceTransforms(AnimationIndex animIndex, bool useDynamicRotations);

	};

	struct DescentData {
		// constants data
		VectorX baseAngles;
		std::vector<glm::vec3> targetEEPositions;
		float betaValue;
		// variables data
		std::vector<JointIndex> jointIndexes;
		std::vector<glm::mat4> forwardModelSpaceTransforms; // multiplicacion en cadena desde la raiz hasta el joint i
		std::vector<std::vector<glm::mat4>> chainsBackwardModelSpaceTransforms; // multiplicacion en cadena desde el ee de la cadena hasta el joint i
		std::vector<glm::mat4> jointSpaceTransforms;
		std::vector<glm::vec3> rotationAxes;
		std::vector<IKChain> ikChains;
		// other data
		IKRigConfig* rigConfig;
		std::vector<glm::vec2> motionRanges;
		std::vector<float> previousAngles;
	};

	class InverseKinematics {
		IKRig* m_ikRig;
		GradientDescent<DescentData> m_gradientDescent;
		DescentData m_descentData;
		std::vector<std::string> m_ikChainNames;
		AnimationIndex m_animationIndex;
		InverseKinematics() = default;
		InverseKinematics(IKRig* ikRig, std::vector<IKChain> ikChains, AnimationIndex animIndex);
		void setIKChains(std::vector<IKChain> ikChains);
		void setAnimationIndex(AnimationIndex animationIndex);
		std::vector<std::pair<JointIndex, glm::fquat>> computeRotations(std::vector<std::pair<std::string, glm::vec3>> eeTargetPerChain);
	};

	

	
};




#endif