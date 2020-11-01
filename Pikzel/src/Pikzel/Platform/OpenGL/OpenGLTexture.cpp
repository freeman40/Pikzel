#include "OpenGLTexture.h"

#include <stb_image.h>

namespace Pikzel {

   OpenGLTexture2D::OpenGLTexture2D(uint32_t width, uint32_t height)
   : m_Width(width)
   , m_Height(height)
   {
      m_InternalFormat = GL_RGBA8;
      m_DataFormat = GL_RGBA;

      glCreateTextures(GL_TEXTURE_2D, 1, &m_RendererId);
      glTextureStorage2D(m_RendererId, 1, m_InternalFormat, m_Width, m_Height);

      glTextureParameteri(m_RendererId, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
      glTextureParameteri(m_RendererId, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

      glTextureParameteri(m_RendererId, GL_TEXTURE_WRAP_S, GL_REPEAT);
      glTextureParameteri(m_RendererId, GL_TEXTURE_WRAP_T, GL_REPEAT);
   }


   OpenGLTexture2D::OpenGLTexture2D(const std::filesystem::path& path)
   : m_Path(path)
   {
      int width;
      int height;
      int channels;
      stbi_set_flip_vertically_on_load(1);
      stbi_uc* data = nullptr;
      {
         data = stbi_load(path.string().c_str(), &width, &height, &channels, 0);
         if (!data) {
            throw std::runtime_error(fmt::format("failed to load image '{0}'", path.string()));
         }
      }
      m_Width = width;
      m_Height = height;

      GLenum internalFormat = 0;
      GLenum dataFormat = 0;
      if (channels == 4) {
         internalFormat = GL_RGBA8;
         dataFormat = GL_RGBA;
      } else if (channels == 3) {
         internalFormat = GL_RGB8;
         dataFormat = GL_RGB;
      }

      m_InternalFormat = internalFormat;
      m_DataFormat = dataFormat;
      if (!internalFormat || !dataFormat) {
         throw std::runtime_error(fmt::format("'{0}': Image format not supported!", path.string()));
      }

      glCreateTextures(GL_TEXTURE_2D, 1, &m_RendererId);
      glTextureStorage2D(m_RendererId, 1, internalFormat, m_Width, m_Height);

      glTextureParameteri(m_RendererId, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
      glTextureParameteri(m_RendererId, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

      glTextureParameteri(m_RendererId, GL_TEXTURE_WRAP_S, GL_REPEAT);
      glTextureParameteri(m_RendererId, GL_TEXTURE_WRAP_T, GL_REPEAT);

      glTextureSubImage2D(m_RendererId, 0, 0, 0, m_Width, m_Height, dataFormat, GL_UNSIGNED_BYTE, data);
      stbi_image_free(data);

      glBindTexture(GL_TEXTURE_2D, m_RendererId);
      glGenerateMipmap(GL_TEXTURE_2D);

   }


   OpenGLTexture2D::~OpenGLTexture2D() {
      glDeleteTextures(1, &m_RendererId);
   }


   uint32_t OpenGLTexture2D::GetWidth() const {
      return m_Width;
   }


   uint32_t OpenGLTexture2D::GetHeight() const {
      return m_Height;
   }


   void OpenGLTexture2D::SetData(void* data, uint32_t size) {
      uint32_t bpp = m_DataFormat == GL_RGBA ? 4 : 3;
      PKZL_CORE_ASSERT(size == m_Width * m_Height * bpp, "Data must be entire texture!");
      glTextureSubImage2D(m_RendererId, 0, 0, 0, m_Width, m_Height, m_DataFormat, GL_UNSIGNED_BYTE, data);
   }


   uint32_t OpenGLTexture2D::GetRendererId() const {
      return m_RendererId;
   }

}
