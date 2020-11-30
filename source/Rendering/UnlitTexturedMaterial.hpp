#pragma once
#ifndef UNLITTEXTUREDMATERIAL_HPP
#define UNLITTEXTUREDMATERIAL_HPP
#include "Material.hpp"
#include <memory>
#include "Texture.hpp"
#include "../Core/Log.hpp"
namespace Mona {
	class UnlitTexturedMaterial : public Material {
	public:
 
		UnlitTexturedMaterial(uint32_t shaderID) : Material(shaderID), m_unlitColorTexture(nullptr) {
			glUseProgram(m_shaderID);
			glUniform1i(ShaderProgram::UnlitColorTextureSamplerShaderLocation, ShaderProgram::UnlitColorTextureUnit);
		}
		virtual void SetMaterialUniforms(const glm::vec3& cameraPosition) {
			MONA_ASSERT(m_unlitColorTexture != nullptr, "Material Error: Texture must be not nullptr for rendering to be posible");
			glBindTextureUnit(ShaderProgram::UnlitColorTextureUnit, m_unlitColorTexture->GetID());
		}
		std::shared_ptr<Texture> GetUnlitColorTexture() const { return m_unlitColorTexture; }
		void SetUnlitColorTexture(std::shared_ptr<Texture> colorTexture) { m_unlitColorTexture = colorTexture; }
	private:
		std::shared_ptr<Texture> m_unlitColorTexture;
	};
}
#endif