#pragma once

#include "RayTracing/Ray.hpp"
#include "Walnut/Random.h"
#include <embree3/rtcore_ray.h>
#include <glm/detail/qualifier.hpp>
#include <glm/fwd.hpp>
#include <glm/glm.hpp>

namespace soft {
    class Material {
    public:
        Material(const vec3& albedo, const float roughness, const vec3& emission = vec3{0.0f}, const float emissionPower = 1.0f)
            : m_Albedo(albedo), m_Roughness(roughness), m_Emission(emission), m_EmissionPower(emissionPower)
        {
        }
        virtual ~Material() {}

        glm::vec3 Emission()
        {
            return m_Emission * m_EmissionPower;
        }

        virtual Ray Scatter(const Ray& ray, const HitPayload& payload)
        {
            return Ray(ray.At(payload.hitDistance) + payload.normal * 0.0001f,
                       glm::reflect(ray.d, payload.normal + Walnut::Random::Vec3(-0.5, 0.5) * m_Roughness));
        }

        virtual Ray Scatter(const Ray& ray, const RTCRayHit& payload)
        {
            vec3 normal = normalize(vec3(payload.hit.Ng_x, payload.hit.Ng_y, payload.hit.Ng_z));
            return Ray(ray.At(payload.ray.tfar) + normal * 0.0001f,
                       glm::reflect(ray.d, normal + Walnut::Random::InUnitSphere() * m_Roughness));
        }

    public:
        glm::vec3 m_Albedo{1.0f};
        float     m_Roughness = 0.0f;

        glm::vec3 m_Emission{0.0f};
        float     m_EmissionPower = -1.0f;
    };
}  // namespace soft