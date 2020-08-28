#pragma once
#ifndef STATICMESHCOMPONENT_HPP
#define STATICMESHCOMPONENT_HPP
#include "../World/Component.hpp"
namespace Mona {
	typedef uint32_t MeshHandle;
	class StaticMeshComponent
	{
	public:
		static constexpr uint8_t componentIndex = GetComponentIndex(EComponentType::StaticMeshComponent);
		MeshHandle Handle;
	};
}
#endif