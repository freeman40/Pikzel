#include "OpenGLTexture.h"

#include "OpenGLComputeContext.h"
#include "OpenGLPipeline.h"

#include <glm/gtc/type_ptr.hpp>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>


namespace Pikzel {

   GLenum TextureFormatToInternalFormat(const TextureFormat format) {
      switch (format) {
         case TextureFormat::RGB8: return GL_RGB8;
         case TextureFormat::RGBA8: return GL_RGBA8;
         case TextureFormat::SRGB8: return GL_SRGB8;
         case TextureFormat::SRGBA8: return GL_SRGB8_ALPHA8;
         case TextureFormat::RG16F: return GL_RG16F;
         case TextureFormat::RGB16F: return GL_RGB16F;
         case TextureFormat::RGBA16F: return GL_RGBA16F;
         case TextureFormat::RG32F: return GL_RG32F;
         case TextureFormat::RGB32F: return GL_RGB32F;
         case TextureFormat::RGBA32F: return GL_RGBA32F;
         case TextureFormat::R8: return GL_R8;
         case TextureFormat::R32F: return GL_R32F;
         case TextureFormat::D32F: return GL_DEPTH_COMPONENT32F;
         case TextureFormat::D24S8: return GL_DEPTH24_STENCIL8;
         case TextureFormat::D32S8: return GL_DEPTH32F_STENCIL8;

      }
      PKZL_CORE_ASSERT(false, "Unsupported TextureFormat!");
      return 0;
   }


   GLenum TextureFormatToDataFormat(const TextureFormat format) {
      switch (format) {
         case TextureFormat::RGB8: return GL_RGB;
         case TextureFormat::RGBA8: return GL_RGBA;
         case TextureFormat::SRGB8: return GL_RGB;
         case TextureFormat::SRGBA8: return GL_RGBA;
         case TextureFormat::RG16F: return GL_RG;
         case TextureFormat::RGB16F: return GL_RGB;
         case TextureFormat::RGBA16F: return GL_RGBA;
         case TextureFormat::RG32F: return GL_RG;
         case TextureFormat::RGB32F: return GL_RGB;
         case TextureFormat::RGBA32F: return GL_RGBA;
         case TextureFormat::R8: return GL_RED;
         case TextureFormat::R32F: return GL_RED;
         // no need (yet) to set depth data yourself, so no depth formats here
      }
      PKZL_CORE_ASSERT(false, "Unsupported TextureFormat!");
      return 0;
   }


   GLenum TextureFormatToDataType(const TextureFormat format) {
      switch (format) {
         case TextureFormat::RGB8: return GL_UNSIGNED_BYTE;
         case TextureFormat::RGBA8: return GL_UNSIGNED_BYTE;
         case TextureFormat::SRGB8: return GL_UNSIGNED_BYTE;
         case TextureFormat::SRGBA8: return GL_UNSIGNED_BYTE;
         case TextureFormat::RG16F: return GL_HALF_FLOAT;
         case TextureFormat::RGB16F: return GL_HALF_FLOAT;
         case TextureFormat::RGBA16F: return GL_HALF_FLOAT;
         case TextureFormat::RG32F: return GL_FLOAT;
         case TextureFormat::RGB32F: return GL_FLOAT;
         case TextureFormat::RGBA32F: return GL_FLOAT;
         case TextureFormat::R8: return GL_UNSIGNED_BYTE;
         case TextureFormat::R32F: return GL_FLOAT;
         // no need to set depth data yourself, so no depth formats here
      }
      PKZL_CORE_ASSERT(false, "Unsupported TextureFormat!");
      return 0;
   }


   GLenum TextureFilterToGLTextureFilter(const TextureFilter filter) {
      switch (filter) {
         case TextureFilter::Nearest: return GL_NEAREST;
         case TextureFilter::NearestMipmapNearest: return GL_NEAREST_MIPMAP_NEAREST;
         case TextureFilter::NearestMipmapLinear: return GL_NEAREST_MIPMAP_LINEAR;
         case TextureFilter::Linear: return GL_LINEAR;
         case TextureFilter::LinearMipmapNearest: return GL_LINEAR_MIPMAP_NEAREST;
         case TextureFilter::LinearMipmapLinear: return GL_LINEAR_MIPMAP_LINEAR;
      }
      PKZL_CORE_ASSERT(false, "Unsupported TextureFilter!");
      return GL_LINEAR;
   }


   GLenum TextureWrapToGLTextureWrap(const TextureWrap wrap) {
      switch (wrap) {
         case TextureWrap::ClampToEdge: return GL_CLAMP_TO_EDGE;
         case TextureWrap::ClampToBorder: return GL_CLAMP_TO_BORDER;
         case TextureWrap::Repeat: return GL_REPEAT;
         case TextureWrap::MirrorRepeat: return GL_MIRRORED_REPEAT;
      }
      PKZL_CORE_ASSERT(false, "Unsupported TextureWrap!");
      return GL_CLAMP_TO_EDGE;
   }


   static stbi_uc* STBILoad(const std::filesystem::path& path, const bool isSRGB, uint32_t* width, uint32_t* height, TextureFormat* format) {
      int iWidth;
      int iHeight;
      int channels;
      stbi_set_flip_vertically_on_load(1);
      stbi_uc* data = nullptr;
      bool isHDR = stbi_is_hdr(path.string().c_str());
      if (isHDR) {
         data = reinterpret_cast<stbi_uc*>(stbi_loadf(path.string().c_str(), &iWidth, &iHeight, &channels, 0));
      } else {
         data = stbi_load(path.string().c_str(), &iWidth, &iHeight, &channels, 0);
      }
      if (!data) {
         throw std::runtime_error {fmt::format("failed to load image '{0}'", path.string())};
      }
      *width = static_cast<uint32_t>(iWidth);
      *height = static_cast<uint32_t>(iHeight);

      if (channels == 4) {
         *format = isHDR ? TextureFormat::RGBA32F : isSRGB? TextureFormat::SRGBA8 : TextureFormat::RGBA8;
      } else if (channels == 3) {
         *format = isHDR ? TextureFormat::RGB32F : isSRGB? TextureFormat::SRGB8 : TextureFormat::RGB8;
      } else if (channels == 1) {
         *format = isHDR ? TextureFormat::R32F : TextureFormat::R8;
      } else {
         throw std::runtime_error {fmt::format("'{0}': Image format not supported!", path.string())};
      }

      return data;
   }


   OpenGLTexture::~OpenGLTexture() {
      glDeleteTextures(1, &m_RendererId);
   }


   TextureFormat OpenGLTexture::GetFormat() const {
      return m_Format;
   }


   uint32_t OpenGLTexture::GetWidth() const {
      return m_Width;
   }


   uint32_t OpenGLTexture::GetHeight() const {
      return m_Height;
   }


   uint32_t OpenGLTexture::GetMIPLevels() const {
      return m_MIPLevels;
   }


   void OpenGLTexture::Commit(const bool generateMipmap) {
      if (generateMipmap) {
         glGenerateTextureMipmap(m_RendererId);
      }
   }


   uint32_t OpenGLTexture::GetRendererId() const {
      return m_RendererId;
   }


   void OpenGLTexture::SetTextureParameters(const TextureSettings& settings) {
      static glm::vec4 borderColor = {0.0f, 0.0f, 0.0f, 1.0f};
      TextureFilter minFilter = settings.minFilter;
      TextureFilter magFilter = settings.magFilter;
      TextureWrap wrapU = settings.wrapU;
      TextureWrap wrapV = settings.wrapV;
      TextureWrap wrapW = settings.wrapW;

      if (minFilter == TextureFilter::Undefined) {
         minFilter = IsDepthFormat(m_Format) ? TextureFilter::Nearest : m_MIPLevels == 1 ? TextureFilter::Linear : TextureFilter::LinearMipmapLinear;
      }
      if (magFilter == TextureFilter::Undefined) {
         magFilter = IsDepthFormat(m_Format) ? TextureFilter::Nearest : TextureFilter::Linear;
      }

      if (wrapU == TextureWrap::Undefined) {
         wrapU = IsDepthFormat(m_Format) ? TextureWrap::ClampToEdge : TextureWrap::Repeat;
      }
      if (wrapV == TextureWrap::Undefined) {
         wrapV = IsDepthFormat(m_Format) ? TextureWrap::ClampToEdge : TextureWrap::Repeat;
      }
      if (wrapW == TextureWrap::Undefined) {
         wrapW = IsDepthFormat(m_Format) ? TextureWrap::ClampToEdge : TextureWrap::Repeat;
      }

      glTextureParameteri(m_RendererId, GL_TEXTURE_MIN_FILTER, TextureFilterToGLTextureFilter(minFilter));
      glTextureParameteri(m_RendererId, GL_TEXTURE_MAG_FILTER, TextureFilterToGLTextureFilter(magFilter));
      glTextureParameteri(m_RendererId, GL_TEXTURE_WRAP_S, TextureWrapToGLTextureWrap(wrapU));
      glTextureParameteri(m_RendererId, GL_TEXTURE_WRAP_T, TextureWrapToGLTextureWrap(wrapV));
      glTextureParameteri(m_RendererId, GL_TEXTURE_WRAP_R, TextureWrapToGLTextureWrap(wrapW));
      glTextureParameterfv(m_RendererId, GL_TEXTURE_BORDER_COLOR, glm::value_ptr(borderColor));
   }


   bool OpenGLTexture::operator==(const Texture& that) {
      return m_RendererId = static_cast<const OpenGLTexture&>(that).m_RendererId;
   }


   OpenGLTexture2D::OpenGLTexture2D(const TextureSettings& settings)
   : m_Path {settings.path}
   {
      m_MIPLevels = settings.mipLevels;
      if (m_Path.empty()) {
         m_Width = settings.width;
         m_Height = settings.height;
         m_Format = settings.format;
         if (m_MIPLevels == 0) {
            m_MIPLevels = CalculateMipmapLevels(m_Width, m_Height);
         }

         glCreateTextures(GL_TEXTURE_2D, 1, &m_RendererId);
         glTextureStorage2D(m_RendererId, m_MIPLevels, TextureFormatToInternalFormat(m_Format), m_Width, m_Height);
      } else {
         stbi_uc* data = STBILoad(m_Path, !IsLinearColorSpace(settings.format), &m_Width, &m_Height, &m_Format);
         if (m_MIPLevels == 0) {
            m_MIPLevels = CalculateMipmapLevels(m_Width, m_Height);
         }

         glCreateTextures(GL_TEXTURE_2D, 1, &m_RendererId);
         glTextureStorage2D(m_RendererId, m_MIPLevels, TextureFormatToInternalFormat(m_Format), m_Width, m_Height);
         glTextureSubImage2D(m_RendererId, 0, 0, 0, m_Width, m_Height, TextureFormatToDataFormat(m_Format), TextureFormatToDataType(m_Format), data);
         Commit();
         stbi_image_free(data);
      }
      SetTextureParameters(settings);
   }


   TextureType OpenGLTexture2D::GetType() const {
      return TextureType::Texture2D;
   }


   uint32_t OpenGLTexture2D::GetLayers() const {
      return 1;
   }


   void OpenGLTexture2D::SetData(void* data, const uint32_t size) {
      PKZL_CORE_ASSERT(size == m_Width * m_Height * BPP(m_Format), "Data must be entire texture!");
      glTextureSubImage2D(m_RendererId, 0, 0, 0, m_Width, m_Height, TextureFormatToDataFormat(m_Format), TextureFormatToDataType(m_Format), data);
   }


   void OpenGLTexture2D::CopyFrom(const Texture& srcTexture, const TextureCopySettings& settings) {
      if (GetType() != srcTexture.GetType()) {
         throw std::logic_error {fmt::format("Texture::CopyFrom() source and destination textures are not the same type!")};
      }
      if (GetFormat() != srcTexture.GetFormat()) {
         throw std::logic_error {fmt::format("Texture::CopyFrom() source and destination textures are not the same format!")};
      }
      if (settings.srcMipLevel >= srcTexture.GetMIPLevels()) {
         throw std::logic_error {fmt::format("Texture::CopyFrom() source texture does not have requested mip level!")};
      }
      if (settings.dstMipLevel >= GetMIPLevels()) {
         throw std::logic_error {fmt::format("Texture::CopyFrom() destination texture does not have requested mip level!")};
      }

      uint32_t layerCount = settings.layerCount == 0? srcTexture.GetLayers() : settings.layerCount;
      if (settings.srcLayer + layerCount > srcTexture.GetLayers()) {
         throw std::logic_error {fmt::format("Texture::CopyFrom() source texture does not have requested layer!")};
      }
      if (settings.dstLayer + layerCount > GetLayers()) {
         throw std::logic_error {fmt::format("Texture::CopyFrom() destination texture does not have requested layer!")};
      }

      uint32_t width = settings.width == 0 ? srcTexture.GetWidth() / (1 << settings.srcMipLevel) : settings.width;
      uint32_t height = settings.height == 0 ? srcTexture.GetHeight() / (1 << settings.srcMipLevel) : settings.height;

      if (width > GetWidth() / (1 << settings.dstMipLevel)) {
         throw std::logic_error {fmt::format("Texture::CopyFrom() requested width is larger than destination texture width!")};
      }
      if (height > GetHeight() / (1 << settings.dstMipLevel)) {
         throw std::logic_error {fmt::format("Texture::CopyFrom() requested height is larger than destination texture height!")};
      }

      glCopyImageSubData(
         static_cast<const OpenGLTexture&>(srcTexture).GetRendererId(),
         GL_TEXTURE_2D,
         settings.srcMipLevel, settings.srcX, settings.srcY, settings.srcLayer,
         GetRendererId(),
         GL_TEXTURE_2D,
         settings.dstMipLevel, settings.dstX, settings.dstY, settings.dstLayer,
         width, height, layerCount
      );
   }


   OpenGLTexture2DArray::OpenGLTexture2DArray(const TextureSettings& settings)
   : m_Layers {settings.layers}
   {
      m_Width = settings.width;
      m_Height = settings.height;
      m_Format = settings.format;
      glCreateTextures(GL_TEXTURE_2D_ARRAY, 1, &m_RendererId);
      m_MIPLevels = settings.mipLevels;
      if (m_MIPLevels == 0) {
         m_MIPLevels = CalculateMipmapLevels(m_Width, m_Height);
      }
      glTextureStorage3D(m_RendererId, m_MIPLevels, TextureFormatToInternalFormat(m_Format), m_Width, m_Height, m_Layers);
      SetTextureParameters(settings);
   }


   TextureType OpenGLTexture2DArray::GetType() const {
      return TextureType::Texture2DArray;
   }


   uint32_t OpenGLTexture2DArray::GetLayers() const {
      return m_Layers;
   }


   void OpenGLTexture2DArray::SetData(void* data, const uint32_t size) {
      PKZL_CORE_ASSERT(size == m_Width * m_Height * m_Layers * BPP(m_Format), "Data must be entire texture!");
      glTextureSubImage3D(m_RendererId, 0, 0, 0, 0, m_Width, m_Height, m_Layers, TextureFormatToDataFormat(m_Format), TextureFormatToDataType(m_Format), data);
   }


   void OpenGLTexture2DArray::CopyFrom(const Texture& srcTexture, const TextureCopySettings& settings) {
      if (GetType() != srcTexture.GetType()) {
         throw std::logic_error {fmt::format("Texture::CopyFrom() source and destination textures are not the same type!")};
      }
      if (GetFormat() != srcTexture.GetFormat()) {
         throw std::logic_error {fmt::format("Texture::CopyFrom() source and destination textures are not the same format!")};
      }
      if (settings.srcMipLevel >= srcTexture.GetMIPLevels()) {
         throw std::logic_error {fmt::format("Texture::CopyFrom() source texture does not have requested mip level!")};
      }
      if (settings.dstMipLevel >= GetMIPLevels()) {
         throw std::logic_error {fmt::format("Texture::CopyFrom() destination texture does not have requested mip level!")};
      }

      uint32_t layerCount = settings.layerCount == 0 ? srcTexture.GetLayers() : settings.layerCount;
      if (settings.srcLayer + layerCount > srcTexture.GetLayers()) {
         throw std::logic_error {fmt::format("Texture::CopyFrom() source texture does not have requested layer!")};
      }
      if (settings.dstLayer + layerCount > GetLayers()) {
         throw std::logic_error {fmt::format("Texture::CopyFrom() destination texture does not have requested layer!")};
      }

      uint32_t width = settings.width == 0 ? srcTexture.GetWidth() / (1 << settings.srcMipLevel) : settings.width;
      uint32_t height = settings.height == 0 ? srcTexture.GetHeight() / (1 << settings.srcMipLevel) : settings.height;

      if (width > GetWidth() / (1 << settings.dstMipLevel)) {
         throw std::logic_error {fmt::format("Texture::CopyFrom() requested width is larger than destination texture width!")};
      }
      if (height > GetHeight() / (1 << settings.dstMipLevel)) {
         throw std::logic_error {fmt::format("Texture::CopyFrom() requested height is larger than destination texture height!")};
      }

      glCopyImageSubData(
         static_cast<const OpenGLTexture&>(srcTexture).GetRendererId(),
         GL_TEXTURE_2D_ARRAY,
         settings.srcMipLevel, settings.srcX, settings.srcY, settings.srcLayer,
         GetRendererId(),
         GL_TEXTURE_2D_ARRAY,
         settings.dstMipLevel, settings.dstX, settings.dstY, settings.dstLayer,
         width, height, layerCount
      );
   }

   OpenGLTextureCube::OpenGLTextureCube(const TextureSettings& settings)
   : m_Path(settings.path)
   , m_DataFormat {TextureFormat::Undefined}
   {
      m_MIPLevels = settings.mipLevels;
      if (m_Path.empty()) {
         m_Width = settings.width;
         m_Height = settings.height;
         m_Format = settings.format;
         if (m_MIPLevels == 0) {
            m_MIPLevels = CalculateMipmapLevels(m_Width, m_Height);
         }
         AllocateStorage();
      } else {
         m_Format = TextureFormat::RGBA16F;
         uint32_t width;
         uint32_t height;
         stbi_uc* data = STBILoad(m_Path, !IsLinearColorSpace(settings.format), &width, &height, &m_DataFormat);

         // guess whether the data is the 6-faces of a cube, or whether it's equirectangular
         // width is twice the height -> equirectangular (probably)
         // width is 4/3 the height -> 6 faces of a cube (probably)
         if (width / 2 == height) {
            m_Width = height;
            m_Height = m_Width;
         } else {
            m_Width = width / 4;
            m_Height = m_Width;
         }
         if (m_MIPLevels == 0) {
            m_MIPLevels = CalculateMipmapLevels(m_Width, m_Height);
         }
         AllocateStorage();
         SetData(data, width * height * BPP(m_DataFormat));
         stbi_image_free(data);
      }
      SetTextureParameters(settings);
   }


   TextureType OpenGLTextureCube::GetType() const {
      return TextureType::TextureCube;
   }


   uint32_t OpenGLTextureCube::GetLayers() const {
      return 6;
   }


   void OpenGLTextureCube::SetData(void* data, uint32_t size) {
      uint32_t width = m_Width;
      uint32_t height = m_Height;
      const char* shader = nullptr;
      if (size == (width * 2 * height * BPP(m_DataFormat))) {
         width *= 2;
         shader = "Renderer/EquirectangularToCubeMap.comp.spv";
      } else if (size == width * 4 * height * 3 * BPP(m_DataFormat)) {
         width *= 4;
         height *= 3;
         shader = "Renderer/SixFacesToCubeMap.comp.spv";
      } else {
         throw std::runtime_error("Data must be entire texture!");
      }

      std::unique_ptr<Texture> tex2d = std::make_unique<OpenGLTexture2D>(TextureSettings{.width = width, .height = height, .format = m_DataFormat, .mipLevels = 1});
      tex2d->SetData(data, size);

      std::unique_ptr<ComputeContext> compute = std::make_unique<OpenGLComputeContext>();

      std::unique_ptr<Pipeline> pipeline = compute->CreatePipeline({
         .shaders = {
            { Pikzel::ShaderType::Compute, shader }
         }
      });

      compute->Begin();
      compute->Bind(*pipeline);
      compute->Bind("uTexture"_hs, *tex2d);
      compute->Bind("outCubeMap"_hs, *this);
      compute->Dispatch(GetWidth() / 32, GetHeight() / 32, 6);
      compute->End();
      Commit();
   }


   void OpenGLTextureCube::CopyFrom(const Texture& srcTexture, const TextureCopySettings& settings) {
      if (GetType() != srcTexture.GetType()) {
         throw std::logic_error {fmt::format("Texture::CopyFrom() source and destination textures are not the same type!")};
      }
      if (GetFormat() != srcTexture.GetFormat()) {
         throw std::logic_error {fmt::format("Texture::CopyFrom() source and destination textures are not the same format!")};
      }
      if (settings.srcMipLevel >= srcTexture.GetMIPLevels()) {
         throw std::logic_error {fmt::format("Texture::CopyFrom() source texture does not have requested mip level!")};
      }
      if (settings.dstMipLevel >= GetMIPLevels()) {
         throw std::logic_error {fmt::format("Texture::CopyFrom() destination texture does not have requested mip level!")};
      }

      uint32_t layerCount = settings.layerCount == 0 ? srcTexture.GetLayers() : settings.layerCount;
      if (settings.srcLayer + layerCount > srcTexture.GetLayers()) {
         throw std::logic_error {fmt::format("Texture::CopyFrom() source texture does not have requested layer!")};
      }
      if (settings.dstLayer + layerCount > GetLayers()) {
         throw std::logic_error {fmt::format("Texture::CopyFrom() destination texture does not have requested layer!")};
      }

      uint32_t width = settings.width == 0 ? srcTexture.GetWidth() / (1 << settings.srcMipLevel) : settings.width;
      uint32_t height = settings.height == 0 ? srcTexture.GetHeight() / (1 << settings.srcMipLevel) : settings.height;

      if (width > GetWidth() / (1 << settings.dstMipLevel)) {
         throw std::logic_error {fmt::format("Texture::CopyFrom() requested width is larger than destination texture width!")};
      }
      if (height > GetHeight() / (1 << settings.dstMipLevel)) {
         throw std::logic_error {fmt::format("Texture::CopyFrom() requested height is larger than destination texture height!")};
      }

      glCopyImageSubData(
         static_cast<const OpenGLTexture&>(srcTexture).GetRendererId(),
         GL_TEXTURE_CUBE_MAP,
         settings.srcMipLevel, settings.srcX, settings.srcY, settings.srcLayer,
         GetRendererId(),
         GL_TEXTURE_CUBE_MAP,
         settings.dstMipLevel, settings.dstX, settings.dstY, settings.dstLayer,
         width, height, layerCount
      );
   }


   void OpenGLTextureCube::AllocateStorage() {
      if ((GetWidth() % 32)) {
         throw std::runtime_error {"Cube texture size must be a multiple of 32!"};
      }

      glCreateTextures(GL_TEXTURE_CUBE_MAP, 1, &m_RendererId);
      glTextureStorage2D(m_RendererId, m_MIPLevels, TextureFormatToInternalFormat(m_Format), m_Width, m_Height);
   }


   OpenGLTextureCubeArray::OpenGLTextureCubeArray(const TextureSettings& settings)
   : m_Layers {settings.layers}
   {
      m_Width = settings.width;
      m_Height = settings.height;
      m_Format = settings.format;
      m_MIPLevels = settings.mipLevels;
      if (m_MIPLevels == 0) {
         m_MIPLevels = CalculateMipmapLevels(m_Width, m_Height);
      }

      if ((GetWidth() % 32)) {
         throw std::runtime_error {"Cube texture size must be a multiple of 32!"};
      }

      glCreateTextures(GL_TEXTURE_CUBE_MAP_ARRAY, 1, &m_RendererId);
      glTextureStorage3D(m_RendererId, m_MIPLevels, TextureFormatToInternalFormat(m_Format), m_Width, m_Height, m_Layers * 6);
      SetTextureParameters(settings);
   }


   TextureType OpenGLTextureCubeArray::GetType() const {
      return TextureType::TextureCubeArray;
   }


   uint32_t OpenGLTextureCubeArray::GetLayers() const {
      return m_Layers * 6;
   }


   void OpenGLTextureCubeArray::SetData(void* data, const uint32_t size) {
      PKZL_NOT_IMPLEMENTED;
   }

   void OpenGLTextureCubeArray::CopyFrom(const Texture& srcTexture, const TextureCopySettings& settings) {
      if (GetType() != srcTexture.GetType()) {
         throw std::logic_error {fmt::format("Texture::CopyFrom() source and destination textures are not the same type!")};
      }
      if (GetFormat() != srcTexture.GetFormat()) {
         throw std::logic_error {fmt::format("Texture::CopyFrom() source and destination textures are not the same format!")};
      }
      if (settings.srcMipLevel >= srcTexture.GetMIPLevels()) {
         throw std::logic_error {fmt::format("Texture::CopyFrom() source texture does not have requested mip level!")};
      }
      if (settings.dstMipLevel >= GetMIPLevels()) {
         throw std::logic_error {fmt::format("Texture::CopyFrom() destination texture does not have requested mip level!")};
      }

      uint32_t layerCount = settings.layerCount == 0 ? srcTexture.GetLayers() : settings.layerCount;
      if (settings.srcLayer + layerCount > srcTexture.GetLayers()) {
         throw std::logic_error {fmt::format("Texture::CopyFrom() source texture does not have requested layer!")};
      }
      if (settings.dstLayer + layerCount > GetLayers()) {
         throw std::logic_error {fmt::format("Texture::CopyFrom() destination texture does not have requested layer!")};
      }

      uint32_t width = settings.width == 0 ? srcTexture.GetWidth() / (1 << settings.srcMipLevel) : settings.width;
      uint32_t height = settings.height == 0 ? srcTexture.GetHeight() / (1 << settings.srcMipLevel) : settings.height;

      if (width > GetWidth() / (1 << settings.dstMipLevel)) {
         throw std::logic_error {fmt::format("Texture::CopyFrom() requested width is larger than destination texture width!")};
      }
      if (height > GetHeight() / (1 << settings.dstMipLevel)) {
         throw std::logic_error {fmt::format("Texture::CopyFrom() requested height is larger than destination texture height!")};
      }

      glCopyImageSubData(
         static_cast<const OpenGLTexture&>(srcTexture).GetRendererId(),
         GL_TEXTURE_CUBE_MAP_ARRAY,
         settings.srcMipLevel, settings.srcX, settings.srcY, settings.srcLayer * 6,
         GetRendererId(),
         GL_TEXTURE_CUBE_MAP_ARRAY,
         settings.dstMipLevel, settings.dstX, settings.dstY, settings.dstLayer * 6,
         width, height, layerCount * 6
      );
   }

}
