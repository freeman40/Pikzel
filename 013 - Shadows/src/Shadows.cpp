#include "Camera.h"

#include "Pikzel/Pikzel.h"
#include "Pikzel/Core/EntryPoint.h"

constexpr float nearPlane = 0.1f;
constexpr float farPlane = 50.0f;

class Shadows final : public Pikzel::Application {
public:
   Shadows()
   : Pikzel::Application {{.Title = APP_DESCRIPTION, .ClearColor = Pikzel::sRGB{0.01f, 0.01f, 0.01f}, .IsVSync = true}}
   , m_Input {GetWindow()}
   {
      CreateVertexBuffer();
      CreateUniformBuffers();
      CreateTextures();
      CreateFramebuffers();
      CreatePipelines();

      m_Camera.Projection = glm::perspective(m_Camera.FoVRadians, static_cast<float>(GetWindow().GetWidth()) / static_cast<float>(GetWindow().GetHeight()), nearPlane, farPlane);

      Pikzel::ImGuiEx::Init(GetWindow());
   }


protected:

   virtual void Update(const Pikzel::DeltaTime deltaTime) override {
      PKZL_PROFILE_FUNCTION();
      if (m_Input.IsKeyPressed(Pikzel::KeyCode::Escape)) {
         Exit();
      }
      m_Camera.Update(m_Input, deltaTime);
   }


   virtual void RenderBegin() override {}


   virtual void Render() override {
      PKZL_PROFILE_FUNCTION();

      static float farPlane = 25.0f;

      // Note: This example illustrates the basic idea of shadow maps, namely that you first render the scene to
      //       an offscreen depth buffer, and then render the scene for real, sampling from the depth buffer to figure
      //       out which fragments are in shadow.
      //
      //       The actual implementation here is not how you'd really do things though, especially not in Vulkan.
      //       Instead of these completely separate "framebuffers", better would be one render pass (using subpasses for the depth)
      //       Secondly, for the point light shadows, it is probably better to use multi-views rather than the geometry shader
      //       used here.
      //
      //       When/if I get around to writing some higher level "scene renderer" classes, I will deal with these issues then.

      // render to directional light shadow map
      {
         Pikzel::GraphicsContext& gc = m_FramebufferDirShadow->GetGraphicsContext();
         gc.BeginFrame();
         gc.Bind(*m_PipelineDirShadow);

         // floor
         glm::mat4 model = glm::identity<glm::mat4>();
         gc.PushConstant("constants.mvp"_hs, m_LightSpace * model);
         gc.DrawTriangles(*m_VertexBuffer, 6, 36);

         // cubes
         for (int i = 0; i < m_CubePositions.size(); ++i) {
            glm::mat4 model = glm::rotate(glm::translate(glm::identity<glm::mat4>(), m_CubePositions[i]), glm::radians(20.0f * i), glm::vec3 {1.0f, 0.3f, 0.5f});
            gc.PushConstant("constants.mvp"_hs, m_LightSpace * model);
            gc.DrawTriangles(*m_VertexBuffer, 36);
         }

         gc.EndFrame();
         gc.SwapBuffers();
      }

      // render to point light shadow map
      {
         glm::mat4 lightProjection = glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, farPlane);  // glm::ortho(-10.0f, 10.0f, -10.0f, 10.0f, 0.0f, 10.0f);  // TODO: how does one determine the parameters here?

         glm::vec3 lightPos = m_PointLights[0].Position;
         std::array<glm::mat4, 6> lightViews = {
            lightProjection * glm::lookAt(lightPos, lightPos + glm::vec3{ 1.0f,  0.0f,  0.0f}, glm::vec3{ 0.0f, -1.0f,  0.0f}),
            lightProjection * glm::lookAt(lightPos, lightPos + glm::vec3{-1.0f,  0.0f,  0.0f}, glm::vec3{ 0.0f, -1.0f,  0.0f}),
            lightProjection * glm::lookAt(lightPos, lightPos + glm::vec3{ 0.0f,  1.0f,  0.0f}, glm::vec3{ 0.0f,  0.0f,  1.0f}),
            lightProjection * glm::lookAt(lightPos, lightPos + glm::vec3{ 0.0f, -1.0f,  0.0f}, glm::vec3{ 0.0f,  0.0f, -1.0f}),
            lightProjection * glm::lookAt(lightPos, lightPos + glm::vec3{ 0.0f,  0.0f,  1.0f}, glm::vec3{ 0.0f, -1.0f,  0.0f}),
            lightProjection * glm::lookAt(lightPos, lightPos + glm::vec3{ 0.0f,  0.0f, -1.0f}, glm::vec3{ 0.0f, -1.0f,  0.0f})
         };
         m_BufferLightViews->CopyFromHost(0, sizeof(glm::mat4) * lightViews.size(), lightViews.data());

         Pikzel::GraphicsContext& gc = m_FramebufferPtShadow->GetGraphicsContext();
         gc.BeginFrame();
         gc.Bind(*m_PipelinePtShadow);
         gc.Bind(*m_BufferLightViews, "UBOLightViews"_hs);

         // floor
         glm::mat4 model = glm::identity<glm::mat4>();
         gc.PushConstant("constants.model"_hs, model);
         gc.PushConstant("constants.lightPos"_hs, m_PointLights[0].Position);
         gc.PushConstant("constants.farPlane"_hs, farPlane);

         gc.DrawTriangles(*m_VertexBuffer, 6, 36);

         // cubes
         for (int i = 0; i < m_CubePositions.size(); ++i) {
            glm::mat4 model = glm::rotate(glm::translate(glm::identity<glm::mat4>(), m_CubePositions[i]), glm::radians(20.0f * i), glm::vec3 {1.0f, 0.3f, 0.5f});
            gc.PushConstant("constants.model"_hs, model);
            gc.DrawTriangles(*m_VertexBuffer, 36);
         }

         gc.EndFrame();
         gc.SwapBuffers();
      }

      // render scene
      {
         Matrices matrices;
         matrices.vp = m_Camera.Projection * glm::lookAt(m_Camera.Position, m_Camera.Position + m_Camera.Direction, m_Camera.UpVector);
         matrices.lightSpace = m_LightSpace;
         matrices.viewPos = m_Camera.Position;

         Pikzel::GraphicsContext& gc = m_FramebufferScene->GetGraphicsContext();
         gc.BeginFrame();

         gc.Bind(*m_PipelineColoredModel);
         for (const auto& pointLight : m_PointLights) {
            glm::mat4 model = glm::scale(glm::translate(glm::identity<glm::mat4>(), pointLight.Position), {0.05f, 0.05f, 0.05f});
            gc.PushConstant("constants.mvp"_hs, matrices.vp * model);
            gc.PushConstant("constants.color"_hs, pointLight.Color);
            gc.DrawTriangles(*m_VertexBuffer, 36);
         }

         m_BufferMatrices->CopyFromHost(0, sizeof(Matrices), &matrices);
         m_BufferPointLight->CopyFromHost(0, sizeof(Pikzel::PointLight) * m_PointLights.size(), m_PointLights.data());
         gc.Bind(*m_PipelineLitModel);
         gc.Bind(*m_BufferMatrices, "UBOMatrices"_hs);
         gc.Bind(*m_BufferDirectionalLight, "UBODirectionalLight"_hs);
         gc.Bind(*m_BufferPointLight, "UBOPointLights"_hs);
         gc.Bind(m_FramebufferDirShadow->GetDepthTexture(), "dirShadowMap"_hs);
         gc.Bind(m_FramebufferPtShadow->GetDepthTexture(), "ptShadowMap"_hs);
         gc.PushConstant("constants.farPlane"_hs, farPlane);

         // floor
         glm::mat4 model = glm::identity<glm::mat4>();
         glm::mat4 modelInvTrans = glm::mat4(glm::transpose(glm::inverse(glm::mat3(model))));
         gc.Bind(*m_TextureFloor, "diffuseMap"_hs);
         gc.Bind(*m_TextureFloorSpecular, "specularMap"_hs);
         gc.PushConstant("constants.model"_hs, model);
         gc.PushConstant("constants.modelInvTrans"_hs, modelInvTrans);
         gc.DrawTriangles(*m_VertexBuffer, 6, 36);

         // cubes
         gc.Bind(*m_TextureContainer, "diffuseMap"_hs);
         gc.Bind(*m_TextureContainerSpecular, "specularMap"_hs);
         for (int i = 0; i < m_CubePositions.size(); ++i) {
            glm::mat4 model = glm::rotate(glm::translate(glm::identity<glm::mat4>(), m_CubePositions[i]), glm::radians(20.0f * i), glm::vec3 {1.0f, 0.3f, 0.5f});
            glm::mat4 modelInvTrans = glm::mat4(glm::transpose(glm::inverse(glm::mat3(model))));
            gc.PushConstant("constants.model"_hs, model);
            gc.PushConstant("constants.modelInvTrans"_hs, modelInvTrans);
            gc.DrawTriangles(*m_VertexBuffer, 36);
         }

         gc.EndFrame();
         gc.SwapBuffers();
      }

      GetWindow().BeginFrame();
      Pikzel::GraphicsContext& gc = GetWindow().GetGraphicsContext();
      gc.Bind(*m_PipelineFullScreenQuad);

      gc.Bind(m_FramebufferScene->GetColorTexture(0), "uTexture"_hs);
      gc.DrawTriangles(*m_VertexBuffer, 6, 42);

      GetWindow().BeginImGuiFrame();
      {
         ImGui::Begin("Point Lights");
         for (size_t i = 0; i < m_PointLights.size(); ++i) {
            ImGuiDrawPointLight(fmt::format("light {0}", i).c_str(), m_PointLights[i]);
         }
         //ImGui::Text("Depth buffer:");
         //ImVec2 size = ImGui::GetContentRegionAvail();
         //ImGui::Image(m_FramebufferDepth->GetImGuiDepthTextureId(), size, ImVec2 {0, 1}, ImVec2 {1, 0});
         ImGui::End();
      }
      GetWindow().EndImGuiFrame();
      GetWindow().EndFrame();
   }


   virtual void RenderEnd() override {}


   virtual void OnWindowResize(const Pikzel::WindowResizeEvent& event) override {
      __super::OnWindowResize(event);
      m_Camera.Projection = glm::perspective(m_Camera.FoVRadians, static_cast<float>(GetWindow().GetWidth()) / static_cast<float>(GetWindow().GetHeight()), nearPlane, farPlane);

      // recreate framebuffer with new size
      CreateFramebuffers();
   }


private:

   struct Vertex {
      glm::vec3 Pos;
      glm::vec3 Normal;
      glm::vec2 TexCoord;
   };

   void CreateVertexBuffer() {
      Vertex vertices[] = {
         // Cube
         {.Pos{ -0.5f,  -0.5f,  -0.5f}, .Normal{ 0.0f,  0.0f, -1.0f}, .TexCoord{ 0.0f,  0.0f}},
         {.Pos{  0.5f,   0.5f,  -0.5f}, .Normal{ 0.0f,  0.0f, -1.0f}, .TexCoord{ 1.0f,  1.0f}},
         {.Pos{  0.5f,  -0.5f,  -0.5f}, .Normal{ 0.0f,  0.0f, -1.0f}, .TexCoord{ 1.0f,  0.0f}},
         {.Pos{  0.5f,   0.5f,  -0.5f}, .Normal{ 0.0f,  0.0f, -1.0f}, .TexCoord{ 1.0f,  1.0f}},
         {.Pos{ -0.5f,  -0.5f,  -0.5f}, .Normal{ 0.0f,  0.0f, -1.0f}, .TexCoord{ 0.0f,  0.0f}},
         {.Pos{ -0.5f,   0.5f,  -0.5f}, .Normal{ 0.0f,  0.0f, -1.0f}, .TexCoord{ 0.0f,  1.0f}},

         {.Pos{ -0.5f,  -0.5f,   0.5f}, .Normal{ 0.0f,  0.0f,  1.0f}, .TexCoord{ 0.0f,  0.0f}},
         {.Pos{  0.5f,  -0.5f,   0.5f}, .Normal{ 0.0f,  0.0f,  1.0f}, .TexCoord{ 1.0f,  0.0f}},
         {.Pos{  0.5f,   0.5f,   0.5f}, .Normal{ 0.0f,  0.0f,  1.0f}, .TexCoord{ 1.0f,  1.0f}},
         {.Pos{  0.5f,   0.5f,   0.5f}, .Normal{ 0.0f,  0.0f,  1.0f}, .TexCoord{ 1.0f,  1.0f}},
         {.Pos{ -0.5f,   0.5f,   0.5f}, .Normal{ 0.0f,  0.0f,  1.0f}, .TexCoord{ 0.0f,  1.0f}},
         {.Pos{ -0.5f,  -0.5f,   0.5f}, .Normal{ 0.0f,  0.0f,  1.0f}, .TexCoord{ 0.0f,  0.0f}},

         {.Pos{ -0.5f,   0.5f,   0.5f}, .Normal{-1.0f,  0.0f,  0.0f}, .TexCoord{ 1.0f,  0.0f}},
         {.Pos{ -0.5f,   0.5f,  -0.5f}, .Normal{-1.0f,  0.0f,  0.0f}, .TexCoord{ 1.0f,  1.0f}},
         {.Pos{ -0.5f,  -0.5f,  -0.5f}, .Normal{-1.0f,  0.0f,  0.0f}, .TexCoord{ 0.0f,  1.0f}},
         {.Pos{ -0.5f,  -0.5f,  -0.5f}, .Normal{-1.0f,  0.0f,  0.0f}, .TexCoord{ 0.0f,  1.0f}},
         {.Pos{ -0.5f,  -0.5f,   0.5f}, .Normal{-1.0f,  0.0f,  0.0f}, .TexCoord{ 0.0f,  0.0f}},
         {.Pos{ -0.5f,   0.5f,   0.5f}, .Normal{-1.0f,  0.0f,  0.0f}, .TexCoord{ 1.0f,  0.0f}},

         {.Pos{  0.5f,   0.5f,   0.5f}, .Normal{ 1.0f,  0.0f,  0.0f}, .TexCoord{ 1.0f,  0.0f}},
         {.Pos{  0.5f,  -0.5f,  -0.5f}, .Normal{ 1.0f,  0.0f,  0.0f}, .TexCoord{ 0.0f,  1.0f}},
         {.Pos{  0.5f,   0.5f,  -0.5f}, .Normal{ 1.0f,  0.0f,  0.0f}, .TexCoord{ 1.0f,  1.0f}},
         {.Pos{  0.5f,  -0.5f,  -0.5f}, .Normal{ 1.0f,  0.0f,  0.0f}, .TexCoord{ 0.0f,  1.0f}},
         {.Pos{  0.5f,   0.5f,   0.5f}, .Normal{ 1.0f,  0.0f,  0.0f}, .TexCoord{ 1.0f,  0.0f}},
         {.Pos{  0.5f,  -0.5f,   0.5f}, .Normal{ 1.0f,  0.0f,  0.0f}, .TexCoord{ 0.0f,  0.0f}},

         {.Pos{ -0.5f,  -0.5f,  -0.5f}, .Normal{ 0.0f, -1.0f,  0.0f}, .TexCoord{ 0.0f,  1.0f}},
         {.Pos{  0.5f,  -0.5f,  -0.5f}, .Normal{ 0.0f, -1.0f,  0.0f}, .TexCoord{ 1.0f,  1.0f}},
         {.Pos{  0.5f,  -0.5f,   0.5f}, .Normal{ 0.0f, -1.0f,  0.0f}, .TexCoord{ 1.0f,  0.0f}},
         {.Pos{  0.5f,  -0.5f,   0.5f}, .Normal{ 0.0f, -1.0f,  0.0f}, .TexCoord{ 1.0f,  0.0f}},
         {.Pos{ -0.5f,  -0.5f,   0.5f}, .Normal{ 0.0f, -1.0f,  0.0f}, .TexCoord{ 0.0f,  0.0f}},
         {.Pos{ -0.5f,  -0.5f,  -0.5f}, .Normal{ 0.0f, -1.0f,  0.0f}, .TexCoord{ 0.0f,  1.0f}},

         {.Pos{ -0.5f,   0.5f,  -0.5f}, .Normal{ 0.0f,  1.0f,  0.0f}, .TexCoord{ 0.0f,  1.0f}},
         {.Pos{  0.5f,   0.5f,   0.5f}, .Normal{ 0.0f,  1.0f,  0.0f}, .TexCoord{ 1.0f,  0.0f}},
         {.Pos{  0.5f,   0.5f,  -0.5f}, .Normal{ 0.0f,  1.0f,  0.0f}, .TexCoord{ 1.0f,  1.0f}},
         {.Pos{  0.5f,   0.5f,   0.5f}, .Normal{ 0.0f,  1.0f,  0.0f}, .TexCoord{ 1.0f,  0.0f}},
         {.Pos{ -0.5f,   0.5f,  -0.5f}, .Normal{ 0.0f,  1.0f,  0.0f}, .TexCoord{ 0.0f,  1.0f}},
         {.Pos{ -0.5f,   0.5f,   0.5f}, .Normal{ 0.0f,  1.0f,  0.0f}, .TexCoord{ 0.0f,  0.0f}},

         // Plane
         {.Pos{ 10.0f,   0.0f,  10.0f}, .Normal{ 0.0f,  1.0f,  0.0f}, .TexCoord{10.0f,  0.0f}},
         {.Pos{-10.0f,   0.0f, -10.0f}, .Normal{ 0.0f,  1.0f,  0.0f}, .TexCoord{ 0.0f, 10.0f}},
         {.Pos{-10.0f,   0.0f,  10.0f}, .Normal{ 0.0f,  1.0f,  0.0f}, .TexCoord{ 0.0f,  0.0f}},

         {.Pos{ 10.0f,   0.0f,  10.0f}, .Normal{ 0.0f,  1.0f,  0.0f}, .TexCoord{10.0f,  0.0f}},
         {.Pos{ 10.0f,   0.0f, -10.0f}, .Normal{ 0.0f,  1.0f,  0.0f}, .TexCoord{10.0f, 10.0f}},
         {.Pos{-10.0f,   0.0f, -10.0f}, .Normal{ 0.0f,  1.0f,  0.0f}, .TexCoord{ 0.0f, 10.0f}},

         // Fullscreen  quad
         {.Pos{ -1.0f,   1.0f,   0.0f}, .Normal{ 0.0f,  1.0f,  0.0f}, .TexCoord{ 0.0f,  1.0f}},
         {.Pos{ -1.0f,  -1.0f,   0.0f}, .Normal{ 0.0f,  1.0f,  0.0f}, .TexCoord{ 0.0f,  0.0f}},
         {.Pos{  1.0f,  -1.0f,   0.0f}, .Normal{ 0.0f,  1.0f,  0.0f}, .TexCoord{ 1.0f,  0.0f}},

         {.Pos{ -1.0f,   1.0f,   0.0f}, .Normal{ 0.0f,  1.0f,  0.0f}, .TexCoord{ 0.0f,  1.0f}},
         {.Pos{  1.0f,  -1.0f,   0.0f}, .Normal{ 0.0f,  1.0f,  0.0f}, .TexCoord{ 1.0f,  0.0f}},
         {.Pos{  1.0f,   1.0f,   0.0f}, .Normal{ 0.0f,  1.0f,  0.0f}, .TexCoord{ 1.0f,  1.0f}},
      };

      m_VertexBuffer = Pikzel::RenderCore::CreateVertexBuffer(sizeof(vertices), vertices);
      m_VertexBuffer->SetLayout({
         { "inPos",      Pikzel::DataType::Vec3 },
         { "inNormal",   Pikzel::DataType::Vec3 },
         { "inTexCoord", Pikzel::DataType::Vec2 }
      });
   }


   struct Matrices {
      glm::mat4 vp;
      glm::mat4 lightSpace;
      glm::vec3 viewPos;
   };

   void CreateUniformBuffers() {

      // note: shader expects exactly 1
      Pikzel::DirectionalLight directionalLights[] = {
         {
            .Direction = { -2.0f, -6.0f, 2.0f},
            .Color = Pikzel::sRGB{0.5f, 0.5f, 0.5f},
            .Ambient = Pikzel::sRGB{0.1f, 0.1f, 0.1f}
         }
      };

      glm::mat4 lightProjection = glm::ortho(-10.0f, 10.0f, -10.0f, 10.0f, 0.0f, 10.0f);  // TODO: how does one determine the parameters here?
      glm::mat4 lightView = glm::lookAt(-directionalLights[0].Direction, glm::vec3 {0.0f, 0.0f, 0.0f}, glm::vec3 {0.0f, 1.0f, 0.0f});
      m_LightSpace = lightProjection * lightView;

      m_BufferMatrices = Pikzel::RenderCore::CreateUniformBuffer(sizeof(Matrices));
      m_BufferLightViews = Pikzel::RenderCore::CreateUniformBuffer(sizeof(glm::mat4) * 6);
      m_BufferDirectionalLight = Pikzel::RenderCore::CreateUniformBuffer(sizeof(directionalLights), directionalLights);
      m_BufferPointLight = Pikzel::RenderCore::CreateUniformBuffer(m_PointLights.size() * sizeof(Pikzel::PointLight), m_PointLights.data());
   }


   void CreateTextures() {
      m_TextureContainer = Pikzel::RenderCore::CreateTexture2D("Assets/" APP_NAME "/Textures/Container.png");
      m_TextureContainerSpecular = Pikzel::RenderCore::CreateTexture2D("Assets/" APP_NAME "/Textures/ContainerSpecular.png", false);
      m_TextureFloor = Pikzel::RenderCore::CreateTexture2D("Assets/" APP_NAME "/Textures/Floor.png");
      m_TextureFloorSpecular = Pikzel::RenderCore::CreateTexture2D("Assets/" APP_NAME "/Textures/FloorSpecular.png", false);
   }


   void CreateFramebuffers() {
      m_FramebufferScene = Pikzel::RenderCore::CreateFramebuffer({.Width = GetWindow().GetWidth(), .Height = GetWindow().GetHeight(), .MSAANumSamples = 4, .ClearColor = GetWindow().GetClearColor()});
      m_FramebufferDirShadow = Pikzel::RenderCore::CreateFramebuffer({.Width = 1024, .Height = 1024, .Attachments = {{Pikzel::AttachmentType::Depth, Pikzel::TextureFormat::D32F}}});
      m_FramebufferPtShadow = Pikzel::RenderCore::CreateFramebuffer({.Width = 1024, .Height = 1024, .Attachments = {{Pikzel::AttachmentType::Depth, Pikzel::TextureFormat::D32F, Pikzel::TextureType::TextureCube}}});
   }


   void CreatePipelines() {
      m_PipelineDirShadow = m_FramebufferDirShadow->GetGraphicsContext().CreatePipeline({
         m_VertexBuffer->GetLayout(),
         {
            { Pikzel::ShaderType::Vertex, "Assets/" APP_NAME "/Shaders/Depth.vert.spv" },
            { Pikzel::ShaderType::Fragment, "Assets/" APP_NAME "/Shaders/Depth.frag.spv" }
         }
      });
      m_PipelinePtShadow = m_FramebufferPtShadow->GetGraphicsContext().CreatePipeline({
         m_VertexBuffer->GetLayout(),
         {
            { Pikzel::ShaderType::Vertex, "Assets/" APP_NAME "/Shaders/DepthCube.vert.spv" },
            { Pikzel::ShaderType::Geometry, "Assets/" APP_NAME "/Shaders/DepthCube.geom.spv" },
            { Pikzel::ShaderType::Fragment, "Assets/" APP_NAME "/Shaders/DepthCube.frag.spv" }
         }
      });
      m_PipelineColoredModel = m_FramebufferScene->GetGraphicsContext().CreatePipeline({
         m_VertexBuffer->GetLayout(),
         {
            { Pikzel::ShaderType::Vertex, "Assets/" APP_NAME "/Shaders/ColoredModel.vert.spv" },
            { Pikzel::ShaderType::Fragment, "Assets/" APP_NAME "/Shaders/ColoredModel.frag.spv" }
         }
      });
      m_PipelineLitModel = m_FramebufferScene->GetGraphicsContext().CreatePipeline({
         m_VertexBuffer->GetLayout(),
         {
            { Pikzel::ShaderType::Vertex, "Assets/" APP_NAME "/Shaders/LitModel.vert.spv" },
            { Pikzel::ShaderType::Fragment, "Assets/" APP_NAME "/Shaders/LitModel.frag.spv" }
         }
      });
      m_PipelineFullScreenQuad = GetWindow().GetGraphicsContext().CreatePipeline({
         m_VertexBuffer->GetLayout(),
         {
            { Pikzel::ShaderType::Vertex, "Assets/" APP_NAME "/Shaders/FullScreenQuad.vert.spv" },
            { Pikzel::ShaderType::Fragment, "Assets/" APP_NAME "/Shaders/FullScreenQuad.frag.spv" }
         }
      });
   }


private:
   static void ImGuiDrawPointLight(const char* label, Pikzel::PointLight& pointLight) {
      ImGui::PushID(label);
      if (ImGui::TreeNode(label)) {
         Pikzel::ImGuiEx::EditVec3("Position", &pointLight.Position);
         Pikzel::ImGuiEx::EditVec3Color("Color", &pointLight.Color);
         Pikzel::ImGuiEx::EditFloat("Power", &pointLight.Power);
         ImGui::TreePop();
      }
      ImGui::PopID();
   }


private:
   Pikzel::Input m_Input;

    Camera m_Camera = {
       .Position = {-10.0f, 5.0f, 0.0f},
       .Direction = glm::normalize(glm::vec3{1.0f, -0.5f, 0.0f}),
       .UpVector = {0.0f, 1.0f, 0.0f},
       .FoVRadians = glm::radians(45.f),
       .MoveSpeed = 1.0f,
       .RotateSpeed = 10.0f
    };

   // note: shader expects exactly 1
   std::vector<Pikzel::PointLight> m_PointLights = {
      {
         .Position = {-2.8f, 2.8f, -1.7f},
         .Color = Pikzel::sRGB{1.0f, 1.0f, 1.0f},
         .Power = 20.0f
      }
   };
//       {
//          .Position = {2.3f, -3.3f, -4.0f},
//          .Color = Pikzel::sRGB{0.0f, 1.0f, 0.0f},
//          .Constant = 1.0f,
//          .Linear = 0.09f,
//          .Quadratic = 0.032f
//       },
//       {
//          .Position = {-4.0f, 2.0f, -12.0f},
//          .Color = Pikzel::sRGB{1.0f, 0.0f, 0.0f},
//          .Constant = 1.0f,
//          .Linear = 0.09f,
//          .Quadratic = 0.032f
//       },
//       {
//          .Position = {0.0f, 0.0f, -3.0f},
//          .Color = Pikzel::sRGB{1.0f, 1.0f, 0.0f},
//          .Constant = 1.0f,
//          .Linear = 0.09f,
//          .Quadratic = 0.032f
//       }
//    };

   std::vector<glm::vec3> m_CubePositions = {
       glm::vec3( 0.0f, 0.5f, 0.0f),
       glm::vec3(2.0f,  1.5f, -1.5f),
   };

   glm::mat4 m_LightSpace;

   std::unique_ptr<Pikzel::VertexBuffer> m_VertexBuffer;
   std::unique_ptr<Pikzel::UniformBuffer> m_BufferMatrices;
   std::unique_ptr<Pikzel::UniformBuffer> m_BufferLightViews;
   std::unique_ptr<Pikzel::UniformBuffer> m_BufferDirectionalLight;
   std::unique_ptr<Pikzel::UniformBuffer> m_BufferPointLight;
   std::unique_ptr<Pikzel::Texture> m_TextureContainer;
   std::unique_ptr<Pikzel::Texture> m_TextureContainerSpecular;
   std::unique_ptr<Pikzel::Texture> m_TextureFloor;
   std::unique_ptr<Pikzel::Texture> m_TextureFloorSpecular;
   std::unique_ptr<Pikzel::Framebuffer> m_FramebufferDirShadow;
   std::unique_ptr<Pikzel::Framebuffer> m_FramebufferPtShadow;
   std::unique_ptr<Pikzel::Framebuffer> m_FramebufferScene;
   std::unique_ptr<Pikzel::Pipeline> m_PipelineDirShadow;
   std::unique_ptr<Pikzel::Pipeline> m_PipelinePtShadow;
   std::unique_ptr<Pikzel::Pipeline> m_PipelineColoredModel;
   std::unique_ptr<Pikzel::Pipeline> m_PipelineLitModel;
   std::unique_ptr<Pikzel::Pipeline> m_PipelineFullScreenQuad;
};


std::unique_ptr<Pikzel::Application> CreateApplication(int argc, const char* argv[]) {
   return std::make_unique<Shadows>();
}
