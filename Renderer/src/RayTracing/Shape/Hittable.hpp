#pragma once
#include "RayTracing/Material/Material.hpp"
#include <RayTracing/Ray.hpp>
#include <glm/glm.hpp>
#include <glm/gtx/dual_quaternion.hpp>
#include <type_traits>

using namespace glm;

namespace soft {

    class Hittable {
    public:
        virtual ~Hittable() {}
        // 只计算求交和求交距离
        virtual bool Intersect(const Ray& ray, HitPayload& payload, float tMax) const
        {
            return false;
        }

        // 已有求交距离，获取计算交点的几何信息
        virtual void ClosestHit(const Ray& ray, HitPayload& payload) const {}

        int materialIndex = 0;
        int textureIndex  = 0;
    };

    class Sphere : public Hittable {
    public:
        Sphere(const vec3& c, float r) : center(c), radius(r) {}

        virtual bool Intersect(const Ray& ray, HitPayload& payload, float tMax) const
        {
            vec3  oc = ray.o - center;
            float a = glm::dot(ray.d, ray.d), b = 2.0f * glm::dot(oc, ray.d), c = glm::dot(oc, oc) - radius * radius;

            float discriminant = b * b - 4.0f * a * c;

            if (discriminant < 0.0f)
                return false;

            float closestT = (-b - std::sqrt(discriminant)) / (2.0f * a);

            if (closestT < payload.hitDistance && closestT > 0.0f) {
                payload.hitDistance = closestT;
                return true;
            }

            return false;
        }

        virtual void ClosestHit(const Ray& ray, HitPayload& payload) const
        {
            payload.position = ray.At(payload.hitDistance);
            payload.normal   = normalize(payload.position - center);
        }

    public:
        vec3  center{0.0f};
        float radius = 1;
    };

    struct Quad
    {
        glm::vec3 x0, x1, x2, x3;

        void SetTransform(mat4 const& transform)
        {
            x0 = transform * vec4(x0, 1.0);
            x1 = transform * vec4(x1, 1.0);
            x2 = transform * vec4(x2, 1.0);
            x3 = transform * vec4(x3, 1.0);
        }

        void Reverse() {}
    };

}  // namespace soft