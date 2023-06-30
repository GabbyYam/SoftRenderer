#pragma once

#include "Geometry/Mesh.hpp"
#include <memory>
#include <vector>
namespace soft {
    struct Geometry
    {
        std::vector<std::shared_ptr<Primitive>> primitives;
    };
}  // namespace soft