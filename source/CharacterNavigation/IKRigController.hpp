#ifndef IKRIGCONTROLLER_HPP
#define IKRIGCONTROLLER_HPP

#include "IKRig.hpp"

namespace Mona {

	class AnimationController;
	class IKRigController {
		friend class IKNavigationComponent;
		IKRig m_ikRig;
		InnerComponentHandle m_skeletalMeshHandle;
		float m_time = 0;
	public:
		IKRigController() = default;
		IKRigController(InnerComponentHandle skeletalMeshHandle, IKRig ikRig);
		void validateTerrains(ComponentManager<StaticMeshComponent>& staticMeshManager);
		void addAnimation(std::shared_ptr<AnimationClip> animationClip);
		int removeAnimation(std::shared_ptr<AnimationClip> animationClip);
		void updateIKRigConfigTime(float time, AnimationIndex animIndex);
		void updateTrajectories(AnimationIndex animIndex, ComponentManager<TransformComponent>& transformManager,
			ComponentManager<StaticMeshComponent>& staticMeshManager);
		void updateAnimation(AnimationIndex animIndex);
		void updateIKRig(float timeStep, ComponentManager<TransformComponent>& transformManager,
			ComponentManager<StaticMeshComponent>& staticMeshManager, ComponentManager<SkeletalMeshComponent>& skeletalMeshManager);
		void updateFrontVector(float time);
	};








}



#endif