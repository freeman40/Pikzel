#pragma once

#include "Pikzel/Core/Core.h"
#include "Pikzel/Scene/ModelResource.h"

#include <entt/resource/cache.hpp>
#include <entt/resource/handle.hpp>

#include <filesystem>
#include <string>

namespace Pikzel {

   using ModelResourceCache = entt::resource_cache<ModelResource>;
   using ModelResourceHandle = entt::resource_handle<ModelResource>;

   class PKZL_API AssetCache {
      AssetCache() = delete;
      PKZL_NO_COPYMOVE(AssetCache);

   public:
      static Id LoadModelResource(const std::string_view name, const std::filesystem::path& path);

      static ModelResourceHandle GetModelResource(Id modelId);

      static void Clear();

   private:
      friend class SceneSerializerYAML;
      inline static ModelResourceCache m_ModelCache;

   };

}
