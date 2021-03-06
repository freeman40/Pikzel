#pragma once

#include "Pikzel/Renderer/GraphicsContext.h"
#include "Pikzel/Renderer/Pipeline.h"
#include "Pikzel/Renderer/ShaderUtil.h"

#include <glm/glm.hpp>

#include <unordered_map>
#include <vector>

namespace Pikzel {

   struct OpenGLUniform {
      std::string Name;
      DataType Type = DataType::None;
      int Location = 0;
      uint32_t Offset = 0;
      uint32_t Size = 0;
   };


   struct OpenGLResourceDeclaration {
      std::string Name;
      uint32_t Binding = 0;
      std::vector<uint32_t> Shape = {};
   };


   using OpenGLUniformMap = std::unordered_map<Id, OpenGLUniform>;
   using OpenGLBindingMap = std::unordered_map < std::pair<uint32_t, uint32_t>, std::pair<uint32_t, uint32_t>>; // maps(descriptor set, binding number) -> (open GL binding number, resource count)
   using OpenGLResourceMap = std::unordered_map<Id, OpenGLResourceDeclaration>;


   class OpenGLPipeline : public Pipeline {
   public:
      OpenGLPipeline(const PipelineSettings& settings);
      virtual ~OpenGLPipeline();

      GLuint GetRendererId() const;
      GLuint GetVAORendererId() const;

      void PushConstant(const Id name, bool value);
      void PushConstant(const Id name, int value);
      void PushConstant(const Id name, uint32_t value);
      void PushConstant(const Id name, float value);
      void PushConstant(const Id name, double value);
      void PushConstant(const Id name, const glm::bvec2& value);
      void PushConstant(const Id name, const glm::bvec3& value);
      void PushConstant(const Id name, const glm::bvec4& value);
      void PushConstant(const Id name, const glm::ivec2& value);
      void PushConstant(const Id name, const glm::ivec3& value);
      void PushConstant(const Id name, const glm::ivec4& value);
      void PushConstant(const Id name, const glm::uvec2& value);
      void PushConstant(const Id name, const glm::uvec3& value);
      void PushConstant(const Id name, const glm::uvec4& value);
      void PushConstant(const Id name, const glm::vec2& value);
      void PushConstant(const Id name, const glm::vec3& value);
      void PushConstant(const Id name, const glm::vec4& value);
      void PushConstant(const Id name, const glm::dvec2& value);
      void PushConstant(const Id name, const glm::dvec3& value);
      void PushConstant(const Id name, const glm::dvec4& value);
      void PushConstant(const Id name, const glm::mat2& value);
      //void PushConstant(const Id name, const glm::mat2x3& value);
      void PushConstant(const Id name, const glm::mat2x4& value);
      void PushConstant(const Id name, const glm::mat3x2& value);
      //void PushConstant(const Id name, const glm::mat3& value);
      void PushConstant(const Id name, const glm::mat3x4& value);
      void PushConstant(const Id name, const glm::mat4x2& value);
      //void PushConstant(const Id name, const glm::mat4x3& value);
      void PushConstant(const Id name, const glm::mat4& value);
      void PushConstant(const Id name, const glm::dmat2& value);
      //void PushConstant(const Id name, const glm::dmat2x3& value);
      void PushConstant(const Id name, const glm::dmat2x4& value);
      void PushConstant(const Id name, const glm::dmat3x2& value);
      //void PushConstant(const Id name, const glm::dmat3& value);
      void PushConstant(const Id name, const glm::dmat3x4& value);
      void PushConstant(const Id name, const glm::dmat4x2& value);
      //void PushConstant(const Id name, const glm::dmat4x3& value);
      void PushConstant(const Id name, const glm::dmat4& value);

      GLuint GetSamplerBinding(const Id resourceId, const bool exceptionIfNotFound = true) const;
      GLuint GetStorageImageBinding(const Id resourceId, const bool exceptionIfNotFound = true) const;
      GLuint GetUniformBufferBinding(const Id resourceId, const bool exceptionIfNotFound = true) const;

      void SetGLState() const;

   private:
      void AppendShader(ShaderType type, const std::filesystem::path path, const SpecializationConstantsMap& specializationConstants);
      void ParsePushConstants(spirv_cross::Compiler& compiler);
      void ParseResourceBindings(spirv_cross::Compiler& compiler);
      void SetSpecializationConstants(spirv_cross::Compiler& compiler, const SpecializationConstantsMap& specializationConstants);
      void LinkShaderProgram();
      void DeleteShaders();
      void FindUniformLocations();

   private:
      std::vector<std::vector<uint32_t>> m_ShaderSrcs;
      std::vector<uint32_t> m_ShaderIds;
      OpenGLUniformMap m_PushConstants;                            // push constants in the Vulkan glsl get turned into uniforms for OpenGL
      OpenGLBindingMap m_UniformBufferBindingMap;
      OpenGLResourceMap m_UniformBufferResources;                  // maps resource id (essentially the name of the resource) -> its opengl binding
      OpenGLBindingMap m_SamplerBindingMap;
      OpenGLResourceMap m_SamplerResources;                        // maps resource id (essentially the name of the resource) -> its opengl binding
      OpenGLBindingMap m_StorageImageBindingMap;
      OpenGLResourceMap m_StorageImageResources;                   // maps resource id (essentially the name of the resource) -> its opengl binding

      uint32_t m_RendererId = 0;
      uint32_t m_VAORendererId = 0;

      bool m_EnableBlend = true;
   };

}
