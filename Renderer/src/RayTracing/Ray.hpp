#pragma once

#include <embree3/rtcore_ray.h>
#include <glm/fwd.hpp>
#include <glm/glm.hpp>
#include <limits>
#include <memory>
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
        // Ray(const vec3& _o, const vec3& _d) : o(_o), d(_d) {}
        vec3 o;
        vec3 d;

        bool isValid = true;
        vec3 At(float t) const
        {
            return o + t * d;
        }
    };

    struct HitPayload
    {
        RTCRayHit rtcHit;
        vec3      position{0.0f};
        vec3      normal{0.0f};
        float     hitDistance = 0.0f;
    };
}  // namespace soft