#pragma once
#ifndef GAMEOBJECT_HPP
#define GAMEOBJECT_HPP
#include "../Core/Log.hpp"
#include <limits>
#include <unordered_map>
#include "GameObjectTypes.hpp"
#include "Component.hpp"
namespace Mona {
	class World;
	class GameObjectManager;
	class GameObject {
	public:
		enum class EState {
			UnStarted,
			Started,
			PendingDestroy
		};
		GameObject() : m_objectHandle(), m_state(EState::UnStarted) {}
		virtual ~GameObject() {};
		GameObject(const GameObject&) = delete;
		GameObject& operator=(const GameObject&) = delete;
		GameObject(GameObject&&) = default;
		GameObject& operator=(GameObject&&) = default;
		
		void StartUp(World& world) noexcept 
		{ 
			UserStartUp(world);
			m_state = EState::Started;
		};

		void Update(World& world, float timeStep) noexcept {
			if (m_state == EState::PendingDestroy)
				return;
			UserUpdate(world, timeStep);
		}

		void ShutDown(World& world) noexcept {
			m_state = EState::PendingDestroy;
		}
		virtual void UserUpdate(World& world, float timeStep) noexcept {};
		virtual void UserStartUp(World& world) noexcept {};

		const EState GetState() const { return m_state; }
		template <typename ComponentType>
		bool HasComponent() const {
			auto it = m_componentHandles.find(ComponentType::componentIndex);
			return it != m_componentHandles.end();
		}
		InnerGameObjectHandle GetInnerObjectHandle() const noexcept { return m_objectHandle; }
		template <typename ComponentType>
		InnerComponentHandle GetInnerComponentHandle() const {
			auto it = m_componentHandles.find(ComponentType::componentIndex);
			if (it != m_componentHandles.end())
				return it->second;
			else return InnerComponentHandle();
		}
	private:
		friend class GameObjectManager;
		friend class World;
		void SetObjectHandle(const InnerGameObjectHandle& handle) {
			m_objectHandle = handle;
		}

		void RemoveInnerComponentHandle(decltype(GetComponentTypeCount()) componentIndex){
			m_componentHandles.erase(componentIndex);
		}

		void AddInnerComponentHandle(decltype(GetComponentTypeCount()) componentIndex, InnerComponentHandle componentHandle) {
			m_componentHandles[componentIndex] = componentHandle;
		}
		InnerGameObjectHandle m_objectHandle;
		EState m_state;
		std::unordered_map<decltype(GetComponentTypeCount()), InnerComponentHandle> m_componentHandles;
	};
}
#endif