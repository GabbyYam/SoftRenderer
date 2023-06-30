#pragma once

#include "Geometry/Geometry.hpp"
#include "Geometry/Mesh.hpp"
#include "Model.hpp"
#include "RayTracing/Material/Material.hpp"
#include "RayTracing/Shape/Hittable.hpp"
#include "Texture/Texture2D.hpp"
#include <memory>
#include <vector>

namespace soft {
    struct Scene
    {
        std::vector<std::shared_ptr<Model>>    models;
        std::vector<std::shared_ptr<Hittable>> hittables;

        std::vector<std::shared_ptr<Material>> materials;
        std::vector<int>                       materialIndex;

        std::vector<std::shared_ptr<Texture2D>> textures;
        std::vector<int>                        textureIndex;

        std::vector<std::shared_ptr<Geometry>> geometries;
    };
}  // namespace soft