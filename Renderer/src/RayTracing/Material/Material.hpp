#pragma once

#include "RayTracing/Ray.hpp"
#include "Walnut/Logger.hpp"
#include "Walnut/Random.h"
#include "imgui_internal.h"
#include <cmath>
#include <embree3/rtcore_ray.h>
#include <glm/detail/qualifier.hpp>
#include <glm/exponential.hpp>
#include <glm/fwd.hpp>
#include <glm/geometric.hpp>
#include <glm/glm.hpp>
#include <memory>
#include <spdlog/spdlog.h>
#include <stdint.h>

namespace soft {

    static constexpr float    PI            = 3.14159265359;
    static constexpr float    bias          = 0.001f;
    static constexpr uint32_t s_SampleCount = 1024u * 8;

    struct SampleRay
    {
        Ray   ray;
        float pdf = 0.0f;
    };

    class Material {
    public:
        Material() = default;

        Material(const vec3& albedo, const vec3& emission = vec3{0.0f}, const float emissionPower = -1.0f)
            : m_Albedo(albedo), m_Emission(emission), m_EmissionPower(emissionPower)
        {
        }
        virtual ~Material() {}

        glm::vec3 Emission()
        {
            return m_Emission * m_EmissionPower;
        }

        // virtual Ray Scatter(const Ray& ray, const HitPayload& payload)
        // {
        //     return Ray(ray.At(payload.rtcHit.hitDistance) + payload.rtcHit.normal * 0.0001f,
        //                glm::reflect(ray.d, payload.rtcHit.normal + Walnut::Random::Vec3(-0.5, 0.5)));
        // }

        virtual SampleRay Sample(const Ray& ray, const HitPayload& payload)
        {
            return CosineWeightedSampleHemiSphere(payload, ray);
        }

        virtual vec3 EvaluateBRDF(const HitPayload& payload, const vec3& wi, const vec3& wo)
        {
            return m_Albedo / (2.0f * PI);
        }

        mat3 TBN(const vec3& N)
        {
            // Make vector q that is non-parallel to n
            vec3 q  = N;
            vec3 aq = abs(q);
            if (aq.x <= aq.y && aq.x <= aq.z)
                q.x = 1.f;
            else if (aq.y <= aq.x && aq.y <= aq.z)
                q.y = 1.f;
            else
                q.z = 1.f;

            // Generate two vectors perpendicular to n
            vec3 T = normalize(cross(q, N));
            vec3 B = normalize(cross(N, T));

            // Construct the rotation matrix
            mat3 m{T, B, N};
            return m;
        }

        // http://holger.dammertz.org/stuff/notes_HammersleyOnHemisphere.html
        // efficient VanDerCorpus calculation.
        float RadicalInverse_VdC(uint bits)
        {
            bits = (bits << 16u) | (bits >> 16u);
            bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
            bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
            bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
            bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
            return float(bits) * 2.3283064365386963e-10;  // / 0x100000000
        }

        vec2 Hammersley(uint i, uint N)
        {
            return vec2(float(i) / float(N), RadicalInverse_VdC(i));
        }

        // Cosine-weighted Sample
        SampleRay CosineWeightedSampleHemiSphere(HitPayload const& payload, Ray const& ray)
        {
            // vec3 N = normalize(vec3(payload.rtcHit.hit.Ng_x, payload.rtcHit.hit.Ng_y, payload.rtcHit.hit.Ng_z));
            vec3 N = payload.normal;

            // vec2 Xi = Hammersley(Walnut::Random::Float() * s_SampleCount, s_SampleCount);
            vec2 Xi = vec2(Walnut::Random::Float(), Walnut::Random::Float());

            float r     = sqrt(Xi.x);
            float theta = 2.0f * PI * Xi.y;

            vec3 H{r * cos(theta), r * sin(theta), sqrt(clamp(1.0f - Xi.x, 0.0f, 1.0f))};
            vec3 sampleVec = normalize(TBN(N) * H);

            vec3  p     = ray.At(payload.rtcHit.ray.tfar) + N * bias;
            float NdotL = max(0.0f, dot(N, sampleVec));
            float pdf   = NdotL / (2.0f * PI);

            return SampleRay{.ray = {p, sampleVec}, .pdf = pdf};
        }

        // Uniform Sample
        SampleRay UniformSampleHemiSphere(HitPayload const& payload, Ray const& ray)
        {
            // vec3 N = normalize(vec3(payload.rtcHit.hit.Ng_x, payload.rtcHit.hit.Ng_y, payload.rtcHit.hit.Ng_z));
            vec3 N = payload.normal;

            vec3 V  = normalize(-ray.d);
            vec2 Xi = Hammersley(Walnut::Random::Float() * s_SampleCount, s_SampleCount);
            // vec2 Xi = vec2(Walnut::Random::Float(), Walnut::Random::Float());

            float theta = acos(Xi.x);
            float phi   = 2.0 * PI * Xi.y;

            vec3 H;
            H.x            = sin(theta) * cos(phi);
            H.y            = sin(theta) * sin(phi);
            H.z            = cos(theta);
            vec3 sampleVec = normalize(TBN(N) * H);

            vec3  p     = ray.At(payload.rtcHit.ray.tfar) + N * bias;
            float NdotL = max(0.0f, dot(N, sampleVec));
            float NdotV = max(0.0f, dot(N, V));
            float pdf   = NdotL / (2.0f * PI);

            return SampleRay{.ray = {p, sampleVec}, .pdf = pdf};
        }

        bool IsLightSource()
        {
            return m_EmissionPower > 0.0f;
        }

    public:
        glm::vec3 m_Albedo{1.0f};

        glm::vec3 m_Emission{0.0f};
        float     m_EmissionPower = -1.0f;
    };

    class PBRMaterial : public Material {
    public:
        PBRMaterial(const vec3& albedo,
                    const float roughness,
                    const float metallic,
                    const vec3& emission      = vec3{0.0f},
                    const float emissionPower = -1.0f)
            : Material(albedo, emission, emissionPower), m_Metallic(metallic), m_Roughness(roughness)
        {
        }

        virtual SampleRay Sample(const Ray& ray, const HitPayload& payload) override
        {
            // return ImportanceSampleGGX(payload, ray);
            return CosineWeightedSampleHemiSphere(payload, ray);
            // return UniformSampleHemiSphere(payload, ray);
        }

        virtual vec3 EvaluateBRDF(const HitPayload& payload, const vec3& wi, const vec3& wo) override
        {
            // vec3  N     = normalize(vec3(payload.rtcHit.hit.Ng_x, payload.rtcHit.hit.Ng_y, payload.rtcHit.hit.Ng_z));
            vec3  N     = payload.normal;
            vec3  L     = normalize(wi);
            vec3  V     = normalize(wo);
            vec3  H     = normalize(L + V);
            float NdotL = max(0.0f, dot(N, L));
            float NdotV = max(0.0f, dot(N, V));
            float HdotV = max(0.0f, dot(H, V));
            float NdotH = max(0.0f, dot(N, H));

            vec3 F0 = vec3(0.04);
            F0      = mix(F0, m_Albedo, m_Metallic);

            // cook-torrance brdf
            float NDF = DistributionGGX(N, H, m_Roughness);
            float G   = GeometrySmith(N, V, L, m_Roughness);
            vec3  F   = FresnelSchlickRoughness(HdotV, F0, m_Roughness);

            vec3 kS = F;
            vec3 kD = vec3(1.0) - kS;
            kD *= 1.0 - m_Metallic;

            vec3  nominator   = NDF * G * F;
            float denominator = 4.0 * NdotV * NdotL + 0.001f;

            vec3 specular = nominator / denominator;

            vec3 lambertian = m_Albedo / PI;

            vec3 brdf = kD * lambertian + specular;
            return brdf;
        }

        float DistributionGGX(vec3 N, vec3 H, float roughness)
        {
            float a      = roughness * roughness;
            float a2     = a * a;
            float NdotH  = max(0.0f, dot(N, H));
            float NdotH2 = NdotH * NdotH;

            float nom   = a2;
            float denom = (NdotH2 * (a2 - 1.0) + 1.0);
            denom       = PI * denom * denom;

            return nom / denom;
        }

        float GeometrySchlickGGX(float NdotV, float roughness)
        {
            float r = (roughness + 1.0);
            float k = (r * r) / 8.0;

            float nom   = NdotV;
            float denom = NdotV * (1.0 - k) + k;

            return nom / denom;
        }

        float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness)
        {
            float NdotV = max(dot(N, V), 0.0f);
            float NdotL = max(dot(N, L), 0.0f);
            float ggx2  = GeometrySchlickGGX(NdotV, roughness);
            float ggx1  = GeometrySchlickGGX(NdotL, roughness);

            return ggx1 * ggx2;
        }

        vec3 FresnelSchlick(float cosTheta, vec3 F0)
        {
            return F0 + (1.0f - F0) * pow(clamp(1.0f - cosTheta, 0.0f, 1.0f), 5.0f);
        }

        vec3 FresnelSchlickRoughness(float cosTheta, vec3 F0, float roughness)
        {
            return F0 + (max(vec3(1.0f - roughness), F0) - F0) * pow(1.0f - cosTheta, 5.0f);
        }

        SampleRay ImportanceSampleGGX(HitPayload const& payload, Ray const& ray)
        {
            vec2 Xi = Hammersley(Walnut::Random::Float() * s_SampleCount, s_SampleCount);
            vec3 N  = payload.normal;

            vec3 p = ray.At(payload.rtcHit.ray.tfar) + N * bias;

            Xi = vec2(Walnut::Random::Float(), Walnut::Random::Float());

            float a     = m_Roughness * m_Roughness;
            float a2    = a * a;
            float theta = std::acos(std::sqrt((1.0f - Xi.x) / (1.0f + (a2 - 1.0f) * Xi.x)));
            float phi   = 2.0f * PI * Xi.y;

            vec3 H;
            H.x = cos(phi) * sin(theta);
            H.y = sin(phi) * sin(theta);
            H.z = cos(theta);

            // To World-space
            H = normalize(TBN(N) * H);

            float nominator   = 2.0f * a2 * cos(theta) * sin(theta);
            float denominator = (a2 - 1) * cos(theta) * cos(theta) + 1.0f;
            denominator       = denominator * denominator;

            vec3 V = normalize(-ray.d);
            vec3 L = normalize(reflect(ray.d, H));

            float NdotL = max(L.z, 0.0f);
            float NdotH = max(H.z, 0.0f);
            float VdotH = dot(V, H);
            float NdotV = max(dot(N, V), 0.0f);

            float pdf = (nominator / denominator) / (4.0f * VdotH);

            return SampleRay{.ray = {p, L}, .pdf = pdf};
        }

    public:
        float m_Roughness = 0.0f;
        float m_Metallic  = 0.0f;
    };

}  // namespace soft