#pragma once

#include <glm/fwd.hpp>
#include <glm/glm.hpp>
#include <limits>
#include <stdint.h>
using namespace glm;
namespace soft {
    class Time {
    public:
        constexpr static float Epsilon = 0.001f;
        constexpr static float Inf     = std::numeric_limits<float>::max();
    };

    struct Ray
    {
        Ray(const vec3& _o, const vec3& _d) : o(_o), d(_d) {}
        vec3 o;
        vec3 d;
        vec3 At(float t) const
        {
            return o + t * d;
        }
    };

    struct HitPayload
    {
        vec3  position{0.0f};
        vec3  normal{0.0f};
        float u = 0.f, v = 0.f;
        float hitDistance = std::numeric_limits<float>::max();
        int   objectIndex = -1;
    };

}  // namespace soft