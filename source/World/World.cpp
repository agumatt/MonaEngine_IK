#include "World.hpp"
#include "../Core/Config.hpp"
#include "../Event/Events.hpp"
#include "../DebugDrawing/DebugDrawingSystem.hpp"
#include "../Audio/AudioClipManager.hpp"
#include "../PhysicsCollision/PhysicsCollisionSystem.hpp"
#include "../Rendering/Material.hpp"
#include "../Rendering/MeshManager.hpp"
#include "../Rendering/TextureManager.hpp"
#include "../Animation/SkeletonManager.hpp"
#include "../Animation/AnimationClipManager.hpp"
#include <chrono>
namespace Mona {
	
	World::World() : 
		m_objectManager(),
		m_eventManager(), 
		m_window(), 
		m_input(), 
		m_application(),
		m_shouldClose(false),
		m_physicsCollisionSystem(),
		m_ambientLight(glm::vec3(0.1f))
	{
		
		m_componentManagers[TransformComponent::componentIndex].reset(new TransformComponent::managerType());
		m_componentManagers[CameraComponent::componentIndex].reset(new CameraComponent::managerType());
		m_componentManagers[StaticMeshComponent::componentIndex].reset(new StaticMeshComponent::managerType());
		m_componentManagers[RigidBodyComponent::componentIndex].reset(new RigidBodyComponent::managerType());
		m_componentManagers[AudioSourceComponent::componentIndex].reset(new AudioSourceComponent::managerType());
		m_componentManagers[DirectionalLightComponent::componentIndex].reset(new DirectionalLightComponent::managerType());
		m_componentManagers[SpotLightComponent::componentIndex].reset(new SpotLightComponent::managerType());
		m_componentManagers[PointLightComponent::componentIndex].reset(new PointLightComponent::managerType());
		m_componentManagers[SkeletalMeshComponent::componentIndex].reset(new SkeletalMeshComponent::managerType());
		m_debugDrawingSystem.reset(new DebugDrawingSystem());
	
	}
	void World::StartUp(std::unique_ptr<Application> app) noexcept {
		auto& transformDataManager = GetComponentManager<TransformComponent>();
		auto& rigidBodyDataManager = GetComponentManager<RigidBodyComponent>();
		auto& audioSourceDataManager = GetComponentManager<AudioSourceComponent>();
		auto& config = Config::GetInstance();
		const GameObjectID expectedObjects = config.getValueOrDefault<int>("expected_number_of_gameobjects", 1000);
		rigidBodyDataManager.SetLifetimePolicy(RigidBodyLifetimePolicy(&transformDataManager, &m_physicsCollisionSystem));
		audioSourceDataManager.SetLifetimePolicy(AudioSourceComponentLifetimePolicy(&m_audioSystem));
		m_window.StartUp(m_eventManager);
		m_input.StartUp(m_eventManager);
		m_objectManager.StartUp(expectedObjects);
		for (auto& componentManager : m_componentManagers)
			componentManager->StartUp(m_eventManager, expectedObjects);
		m_application = std::move(app);
		m_renderer.StartUp(m_eventManager, m_debugDrawingSystem.get());
		m_audioSystem.StartUp();
		m_debugDrawingSystem->StartUp(&m_physicsCollisionSystem);
		m_application->StartUp(*this);
	}

	void World::ShutDown() noexcept {
		m_application->UserShutDown(*this);
		m_objectManager.ShutDown(*this);
		for (auto& componentManager : m_componentManagers)
			componentManager->ShutDown(m_eventManager);
		m_audioSystem.ClearSources();
		AudioClipManager::GetInstance().ShutDown();
		m_audioSystem.ShutDown();
		m_physicsCollisionSystem.ShutDown();
		MeshManager::GetInstance().ShutDown();
		TextureManager::GetInstance().ShutDown();
		SkeletonManager::GetInstance().ShutDown();
		AnimationClipManager::GetInstance().ShutDown();
		m_renderer.ShutDown(m_eventManager);
		m_debugDrawingSystem->ShutDown();
		m_window.ShutDown();
		m_input.ShutDown(m_eventManager);
		m_eventManager.ShutDown();

	}

	void World::DestroyGameObject(BaseGameObjectHandle& handle) noexcept {
		DestroyGameObject(*handle);
	}

	void World::DestroyGameObject(GameObject& gameObject) noexcept {
		auto& innerComponentHandles = gameObject.m_componentHandles;
		//Es necesario remover primero todas las componentes antes de destruir el GameObject
		for (auto& it : innerComponentHandles) {
			m_componentManagers[it.first]->RemoveComponent(it.second);
		}
		innerComponentHandles.clear();
		m_objectManager.DestroyGameObject(*this, gameObject.GetInnerObjectHandle());
	}

	bool World::IsValid(const BaseGameObjectHandle& handle) const noexcept {
		return m_objectManager.IsValid(handle->GetInnerObjectHandle());
	}
	GameObjectManager::size_type World::GetGameObjectCount() const noexcept
	{
		return m_objectManager.GetCount();
	}
	EventManager& World::GetEventManager() noexcept {
		return m_eventManager;
	}

	Input& World::GetInput() noexcept {
		return m_input;
	}

	Window& World::GetWindow() noexcept {
		return m_window;
	}

	void World::EndApplication() noexcept {
		m_shouldClose = true;
	}

	void World::StartMainLoop() noexcept {
		std::chrono::time_point<std::chrono::steady_clock> startTime = std::chrono::steady_clock::now();
		
		while (!m_window.ShouldClose() && !m_shouldClose)
		{
			std::chrono::time_point<std::chrono::steady_clock> newTime = std::chrono::steady_clock::now();
			const auto frameTime = newTime - startTime;
			startTime = newTime;
			float timeStep = std::chrono::duration_cast<std::chrono::duration<float>>(frameTime).count();
			Update(timeStep);
		}
		m_eventManager.Publish(ApplicationEndEvent());
		
	}

	void World::Update(float timeStep) noexcept
	{
		auto &transformDataManager = GetComponentManager<TransformComponent>();
		auto &staticMeshDataManager = GetComponentManager<StaticMeshComponent>();
		auto &cameraDataManager = GetComponentManager<CameraComponent>();
		auto& rigidBodyDataManager = GetComponentManager<RigidBodyComponent>();
		auto& audioSourceDataManager = GetComponentManager<AudioSourceComponent>();
		auto& directionalLightDataManager = GetComponentManager<DirectionalLightComponent>();
		auto& spotLightDataManager = GetComponentManager<SpotLightComponent>();
		auto& pointLightDataManager = GetComponentManager<PointLightComponent>();
		auto& skeletalMeshDataManager = GetComponentManager<SkeletalMeshComponent>();
		m_input.Update();
		m_physicsCollisionSystem.StepSimulation(timeStep);
		m_physicsCollisionSystem.SubmitCollisionEvents(*this, m_eventManager, rigidBodyDataManager);
		for (uint32_t i = 0; i < skeletalMeshDataManager.GetCount(); i++) {
			SkeletalMeshComponent& skeletalMesh = skeletalMeshDataManager[i];
			GameObject* owner = skeletalMeshDataManager.GetOwnerByIndex(i);
			TransformComponent* transform = transformDataManager.GetComponentPointer(owner->GetInnerComponentHandle<TransformComponent>());
			skeletalMesh.Update(transform->GetModelMatrix(), timeStep);
		}
		m_objectManager.UpdateGameObjects(*this, m_eventManager, timeStep);
		m_application->UserUpdate(*this, timeStep);
		m_audioSystem.Update(m_audoListenerTransformHandle, timeStep, transformDataManager, audioSourceDataManager);
		m_renderer.Render(m_eventManager,
			m_cameraHandle,
			m_ambientLight,
			staticMeshDataManager,
			skeletalMeshDataManager,
			transformDataManager,
			cameraDataManager,
			directionalLightDataManager,
			spotLightDataManager,
			pointLightDataManager);
		m_window.Update();
	}

	void World::SetMainCamera(const ComponentHandle<CameraComponent>& cameraHandle) noexcept {
		m_cameraHandle = cameraHandle.GetInnerHandle();

	}

	ComponentHandle<CameraComponent> World::GetMainCameraComponent() noexcept {
		auto& cameraDataManager = GetComponentManager<CameraComponent>();
		return ComponentHandle<CameraComponent>(m_cameraHandle, &cameraDataManager);
	}
	
	std::shared_ptr<Material> World::CreateMaterial(MaterialType type) noexcept {
		return m_renderer.CreateMaterial(type);
	}

	void World::SetAudioListenerTransform(const ComponentHandle<TransformComponent>& transformHandle) noexcept{
		m_audoListenerTransformHandle = transformHandle.GetInnerHandle();
	}

	ComponentHandle<TransformComponent> World::GetAudioListenerTransform() noexcept {
		auto& transformDataManager = GetComponentManager<TransformComponent>();
		return ComponentHandle<TransformComponent>(m_audoListenerTransformHandle, &transformDataManager);
	}
	void World::SetGravity(const glm::vec3& gravity) {
		m_physicsCollisionSystem.SetGravity(gravity);
	}

	glm::vec3 World::GetGravity() const {
		return m_physicsCollisionSystem.GetGravity();
	}

	ClosestHitRaycastResult World::ClosestHitRayTest(const glm::vec3& rayFrom, const glm::vec3& rayTo) {
		auto& rigidBodyDataManager = GetComponentManager<RigidBodyComponent>();
		return m_physicsCollisionSystem.ClosestHitRayTest(rayFrom, rayTo, rigidBodyDataManager);
	}

	AllHitsRaycastResult World::AllHitsRayTest(const glm::vec3& rayFrom, const glm::vec3& rayTo) {
		auto& rigidBodyDataManager = GetComponentManager<RigidBodyComponent>();
		return m_physicsCollisionSystem.AllHitsRayTest(rayFrom, rayTo, rigidBodyDataManager);
	}

	void World::PlayAudioClip3D(std::shared_ptr<AudioClip> audioClip,
		const glm::vec3& position /* = glm::vec3(0.0f) */,
		float volume /* = 1.0f */,
		float pitch /* = 1.0f */,
		float radius /* = 1000.0f */,
		AudioSourcePriority priority /* = AudioSourcePriority::SoundPriorityMedium */)
	{
		m_audioSystem.PlayAudioClip3D(audioClip, position, volume, pitch, radius, priority);
	}

	void World::PlayAudioClip2D(std::shared_ptr<AudioClip> audioClip,
		float volume /* = 1.0f */,
		float pitch /* = 1.0f */,
		AudioSourcePriority priority /* = AudioSourcePriority::SoundPriorityMedium */)
	{
		m_audioSystem.PlayAudioClip2D(audioClip, volume, pitch, priority);
	}

	float World::GetMasterVolume() const noexcept {
		return m_audioSystem.GetMasterVolume();
	}

	void World::SetMasterVolume(float volume) noexcept {
		m_audioSystem.SetMasterVolume(volume);
	}


}

