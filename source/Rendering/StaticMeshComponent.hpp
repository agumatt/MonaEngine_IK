#pragma once
#ifndef STATICMESHCOMPONENT_HPP
#define STATICMESHCOMPONENT_HPP
#include "../Core/Log.hpp"
#include "../World/ComponentTypes.hpp"
#include "Mesh.hpp"
#include "Material.hpp"
#include <glm/glm.hpp>
namespace Mona {
	class TransformComponent;
	class StaticMeshComponent
	{
	public:
		friend class Renderer;
		using managerType = ComponentManager<StaticMeshComponent>;
		using dependencies = DependencyList<TransformComponent>;
		static constexpr std::string_view componentName = "StaticMeshComponent";
		static constexpr uint8_t componentIndex = GetComponentIndex(EComponentType::StaticMeshComponent);
		StaticMeshComponent(std::shared_ptr<Mesh> mesh, std::shared_ptr<Material> material) : m_meshPtr(mesh), m_materialPtr(material)
		{
			MONA_ASSERT(mesh != nullptr, "StaticMeshComponent Error: Mesh pointer cannot be null.");
			MONA_ASSERT(material != nullptr, "StaticMeshComponent Error: Material cannot be null.");

		}
		uint32_t GetMeshIndexCount() const noexcept {
			return m_meshPtr->GetCount();
		}

		uint32_t GetMeshVAOID() const noexcept {
			return m_meshPtr->GetVAOID();
		}

		std::shared_ptr<Material> GetMaterial() const noexcept {
			return m_materialPtr;
		}

		void SetMaterial(std::shared_ptr<Material> material) noexcept {
			MONA_ASSERT(material != nullptr, "StaticMeshComponent Error: Material cannot be null.");
			m_materialPtr = material;
		}
		

	private:
		std::shared_ptr<Mesh> m_meshPtr;
		std::shared_ptr<Material> m_materialPtr;
	};
}
#endif