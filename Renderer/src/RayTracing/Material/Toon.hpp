#pragma once

#include "RayTracing/Material/Material.hpp"
namespace soft {
    class Toon : public Material {
    public:
        virtual Ray Scatter(const Ray& ray, const HitPayload& payload)
        {
            return Ray(ray.At(payload.hitDistance) + payload.normal * 0.0001f,
                       glm::reflect(ray.d, payload.normal + Walnut::Random::Vec3(-0.5, 0.5)));
        }

        virtual Ray Scatter(const Ray& ray, const RTCRayHit& payload)
        {
            vec3 normal = normalize(vec3(payload.hit.Ng_x, payload.hit.Ng_y, payload.hit.Ng_z));
            return Ray(ray.At(payload.ray.tfar) + normal * 0.0001f, glm::reflect(ray.d, normal + Walnut::Random::Vec3(-0.5, 0.5)));
        }
    };
}  // namespace soft