#pragma once
#include "glm/glm.hpp"
#include <iostream>

namespace soft {
    class Config {};
    struct LightSetting
    {
        glm::vec3   directionalLight{1.0f};
        glm::vec3   lightColor{1.0f};
        float       lightIntensity    = 1.0f;
        bool        useEnvironmentMap = true;
        std::string environmentMapPath;
    };

    struct RayTraceSetting
    {
        int bounce = 5;
    };

    struct PostProcessingSetting
    {
        bool MT          = false;
        bool FXAA        = false;
        bool ToneMapping = true;
        bool Gamma       = true;
    };
}  // namespace soft