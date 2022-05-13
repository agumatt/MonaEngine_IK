#pragma once
#ifndef MESH_HPP
#define MESH_HPP
#include <cstdint>
#include <string>
#include <glm/glm.hpp>

namespace Mona {
	class HeightMap;
	class Mesh {
		friend class MeshManager;
		
	public:
		enum class PrimitiveType {
			Plane,
			Cube,
			Sphere,
			PrimitiveCount
		};
		~Mesh();
		uint32_t GetVertexArrayID() const noexcept { return m_vertexArrayID; }
		uint32_t GetIndexBufferCount() const noexcept { return m_indexBufferCount; }
	private:
		Mesh(const std::string& filePath, bool flipUVs = false, HeightMap* heightMap = nullptr);
		Mesh(PrimitiveType type);
		Mesh(const glm::vec2& bottomLeft, const glm::vec2& topRight, int numInnerVerticesWidth, int numInnerVerticesHeight, 
			float (*heightFunc)(float, float), HeightMap* heightMap = nullptr);

		void ClearData() noexcept;
		void CreateSphere() noexcept;
		void CreateCube() noexcept;
		void CreatePlane() noexcept;

		uint32_t m_vertexArrayID;
		uint32_t m_vertexBufferID;
		uint32_t m_indexBufferID;
		uint32_t m_indexBufferCount;
	};
}
#endif