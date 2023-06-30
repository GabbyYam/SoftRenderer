#pragma once

#include <glm/fwd.hpp>
#include <glm/glm.hpp>
#include <spdlog/spdlog.h>
namespace utils {

    void debugColor(const glm::vec3& color)
    {
        spdlog::debug("color = {}, {}, {}", color.x, color.y, color.z);
    }
}  // namespace utils