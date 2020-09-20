#include "PhysicsCollisionSystem.hpp"

namespace Mona {
	void PhysicsCollisionSystem::StepSimulation(float timeStep) noexcept {
		m_worldPtr->stepSimulation(timeStep);
	
	}

	void PhysicsCollisionSystem::AddRigidBody(btRigidBody* rbPtr) noexcept {
		m_worldPtr->addRigidBody(rbPtr);
	}

	void PhysicsCollisionSystem::ShutDown() noexcept {
		for (int i = 0; i < m_worldPtr->getNumCollisionObjects(); i++) {
			m_worldPtr->removeCollisionObject(m_worldPtr->getCollisionObjectArray()[i]);
		}
	}
}