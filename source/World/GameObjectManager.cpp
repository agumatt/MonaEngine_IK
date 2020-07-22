#include "GameObjectManager.hpp"
#include "../Event/EventManager.hpp"
#include "World.hpp"
#include "../Core/Log.hpp"
namespace Mona {

	GameObjectManager::GameObjectManager() :
		m_firstFreeIndex(s_maxEntries),
		m_lastFreeIndex(s_maxEntries),
		m_freeIndicesCount(0),
		m_IsIteratingOverObject(false)
	{}
	
	void GameObjectManager::StartUp(GameObjectID expectedObjects) noexcept
	{
		m_handleEntries.reserve(expectedObjects);
		m_gameObjects.reserve(expectedObjects);
		m_gameObjectHandleIndices.reserve(expectedObjects);
	}

	void GameObjectManager::ShutDown(World& world) noexcept
	{
		m_handleEntries.clear();
		m_gameObjects.clear();
		m_gameObjectHandleIndices.clear();
		m_firstFreeIndex = s_maxEntries;
		m_lastFreeIndex = s_maxEntries;
		m_freeIndicesCount = 0;
	}

	void GameObjectManager::DestroyGameObject(World& world, const InnerGameObjectHandle& handle) noexcept
	{
		auto index = handle.m_index;
		MONA_ASSERT(index < m_handleEntries.size(), "GameObjectManager Error: handle index out of bounds");
		MONA_ASSERT(handle.m_generation == m_handleEntries[index].generation, "GamObjectManager Error: Trying to destroy from invalid handle");
		MONA_ASSERT(m_handleEntries[index].active == true, "GamObjectManager Error: Trying to destroy from inactive handle");
		auto& handleEntry = m_handleEntries[index];
		auto& eventManager = world.GetEventManager();
		GameObjectDestroyedEvent event(*m_gameObjects[handleEntry.index]);
		eventManager.Publish(event);
		m_gameObjects[handleEntry.index]->ShutDown(world);
		if (handleEntry.index < m_gameObjects.size() - 1)
		{
			auto handleEntryIndex = m_gameObjectHandleIndices.back();
			m_gameObjects[handleEntry.index] = std::move(m_gameObjects.back());
			m_handleEntries[handleEntryIndex].index = handleEntry.index;
		}

		m_gameObjects.pop_back();
		m_gameObjectHandleIndices.pop_back();

		if (m_lastFreeIndex != s_maxEntries)
			m_handleEntries[m_lastFreeIndex].index = index;
		else
			m_firstFreeIndex = index;

		handleEntry.active = false;
		handleEntry.prevIndex = m_lastFreeIndex;
		handleEntry.index = s_maxEntries;
		m_lastFreeIndex = index;
		++m_freeIndicesCount;
	}

	GameObject& GameObjectManager::GetGameObjectReference(const InnerGameObjectHandle& handle) noexcept
	{
		const auto index = handle.m_index;
		MONA_ASSERT(index < m_handleEntries.size(), "GameObjectManager Error: handle index out of range");
		MONA_ASSERT(m_handleEntries[index].active == true, "GameObjectManager Error: Trying to access inactive handle");
		MONA_ASSERT(m_handleEntries[index].generation == handle.m_generation, "GameObjectManager Error: handle with incorrect generation");
		return *(m_gameObjects[m_handleEntries[index].index]);
	}

	GameObjectManager::size_type GameObjectManager::GetCount() const noexcept
	{
		return m_gameObjects.size();
	}

	bool GameObjectManager::IsValid(const InnerGameObjectHandle& handle) const noexcept
	{
		const auto index = handle.m_index;
		if (index >= m_handleEntries.size() ||
			m_handleEntries[index].active == false ||
			m_handleEntries[index].generation != handle.m_generation)
			return false;
		return true;
	}
	void GameObjectManager::UpdateGameObjects(World& world, float timeStep) noexcept {
		m_IsIteratingOverObject = true;
		for (auto& object : m_gameObjects)
		{
			object->Update(world, timeStep);
		}
		m_IsIteratingOverObject = false;
	}
}