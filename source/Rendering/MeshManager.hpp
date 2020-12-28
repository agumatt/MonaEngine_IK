#pragma once
#ifndef MESHMANAGER_HPP
#define MESHMANAGER_HPP
#include <memory>
#include <filesystem>
#include <unordered_map>
namespace Mona {

	class Mesh;
	class SkinnedMesh;
	class Skeleton;
	class AnimationClip;
	class MeshManager {
	public:
		friend class World;
		enum class PrimitiveType {
			Plane,
			Cube,
			Sphere,
			PrimitiveCount
		};

		using MeshMap = std::unordered_map<std::string, std::shared_ptr<Mesh>>;
		using SkinnedMeshMap = std::unordered_map<std::string, std::shared_ptr<SkinnedMesh>>;
		MeshManager(MeshManager const&) = delete;
		MeshManager& operator=(MeshManager const&) = delete;
		std::shared_ptr<Mesh> LoadMesh(PrimitiveType type) noexcept;
		std::shared_ptr<Mesh> LoadMesh(const std::filesystem::path& filePath, bool flipUvs = false) noexcept;
		std::shared_ptr<SkinnedMesh> LoadSkinnedMesh(std::shared_ptr<Skeleton> skeleton,
			const std::filesystem::path& filePath,
			bool flipUvs = false) noexcept;
		void CleanUnusedMeshes() noexcept;
		static MeshManager& GetInstance() noexcept{
			static MeshManager instance;
			return instance;
		}
	private:
		MeshManager() = default;
		void ShutDown() noexcept;
		std::shared_ptr<Mesh> LoadSphere() noexcept;
		std::shared_ptr<Mesh> LoadCube() noexcept;
		std::shared_ptr<Mesh> LoadPlane() noexcept;
		MeshMap m_meshMap;
		SkinnedMeshMap m_skinnedMeshMap;

	};
}
#endif