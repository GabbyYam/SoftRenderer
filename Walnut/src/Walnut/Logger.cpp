#include "Logger.hpp"

namespace utils {
    void debugColor(const glm::vec3& color)
    {
        spdlog::debug("color = {}, {}, {}", color.x, color.y, color.z);
    }
}  // namespace utils