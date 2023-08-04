#include "Logger.hpp"
#include <spdlog/spdlog.h>

namespace utils {
    void debugColor(const glm::vec3& color)
    {
        spdlog::debug("color = {}, {}, {}", color.x, color.y, color.z);
    }

    void PrintVector(const glm::vec3& v)
    {
        spdlog::error("vec3 = {}, {}, {}", v.x, v.y, v.z);
    }

}  // namespace utils