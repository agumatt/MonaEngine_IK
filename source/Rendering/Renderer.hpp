#pragma once
#ifndef RENDERER_HPP
#define RENDERER_HPP
#include "../Event/EventManager.hpp"
#include "../World/ComponentManager.hpp"
#include "../World/Component.hpp"
#include "StaticMeshComponent.hpp"
#include "CameraComponent.hpp"



namespace Mona {


	class Renderer {
	public:
		void StartUp(EventManager& eventManager) noexcept;
		void Render(EventManager& eventManager,
					ComponentManager<StaticMeshComponent> &staticMeshDataManager,
					ComponentManager<TransformComponent> &transformDataManager,
					ComponentManager<CameraComponent> &cameraDataManager) noexcept;
		void ShutDown(EventManager& eventManager) noexcept;
		void OnWindowResizeEvent(const WindowResizeEvent& event);
	private:
		void StartDebugConfiguration() noexcept;
		void StartImGui() noexcept;
		void RenderImGui(EventManager& eventManager) noexcept;
		SubscriptionHandle m_onWindowResizeSubscription;
	};
}
#endif