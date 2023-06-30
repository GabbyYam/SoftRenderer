#pragma once

#include "Geometry/Mesh.hpp"
#include "Hittable.hpp"
#include <glm/geometric.hpp>

namespace soft {
    class Triangle : public Hittable {
    public:
        Triangle(const Primitive& p) : primitive(p) {}
        Triangle(const vec3& v1, const vec3& v2, const vec3& v3) {}

        // 只计算求交和求交距离
        virtual bool Intersect(const Ray& ray, HitPayload& payload, float tMax) const
        {
            const vec3& A   = primitive[0].position;
            const vec3& B   = primitive[1].position;
            const vec3& C   = primitive[2].position;
            const vec3& O   = ray.o;
            const vec3& D   = ray.d;
            float&      u   = payload.u;
            float&      v   = payload.v;
            glm::vec3   E1  = B - A;
            glm::vec3   E2  = C - A;
            glm::vec3   P   = glm::cross(D, E2);
            float       det = glm::dot(E1, P);
#ifdef CULLING
            // if the determinant is negative the triangle is backfacing
            // if the determinant is close to 0, the ray misses the triangle
            if (det < kEpsilon)
                return false;
#else
            // ray and triangle are parallel if det is close to 0
            if (fabs(det) < Time::Epsilon)
                return false;
#endif
            float invDet = 1 / det;

            glm::vec3 T = O - A;
            u           = glm::dot(T, P) * invDet;
            if (u < 0 || u > 1)
                return false;

            glm::vec3 Q = glm::cross(T, E1);
            v           = glm::dot(D, Q) * invDet;
            if (v < 0 || u + v > 1)
                return false;

            float t = glm::dot(E2, Q) * invDet;

            if (t < payload.hitDistance && t > 0) {
                payload.hitDistance = t;
                return true;
            }

            return false;
        }

        // 已有求交距离，获取计算交点的几何信息
        virtual void ClosestHit(const Ray& ray, HitPayload& payload) const
        {
            payload.position = ray.At(payload.hitDistance);
            vec3 ab          = primitive[0].position - primitive[1].position;
            vec3 ac          = primitive[0].position - primitive[2].position;
            payload.normal   = glm::cross(ab, ac);
        }

    public:
        Primitive primitive;

    private:
    };
}  // namespace soft