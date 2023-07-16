#pragma once

#include "RayTracing/Ray.hpp"
#include "Walnut/Logger.hpp"
#include "Walnut/Random.h"
#include "imgui_internal.h"
#include <embree3/rtcore_ray.h>
#include <glm/detail/qualifier.hpp>
#include <glm/fwd.hpp>
#include <glm/geometric.hpp>
#include <glm/glm.hpp>
#include <memory>
#include <spdlog/spdlog.h>
#include <stdint.h>

namespace soft {

    static constexpr float    PI           = 3.14159265359;
    static constexpr float    bias         = 0.001f;
    static constexpr uint32_t SAMPLE_COUNT = 1024u * 8;

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

        virtual Ray Scatter(const Ray& ray, const HitPayload& payload)
        {
            return Ray(ray.At(payload.hitDistance) + payload.normal * 0.0001f,
                       glm::reflect(ray.d, payload.normal + Walnut::Random::Vec3(-0.5, 0.5)));
        }

        virtual SampleRay Sample(const Ray& ray, const RTCRayHit& payload)
        {
            vec3      normal = normalize(vec3(payload.hit.Ng_x, payload.hit.Ng_y, payload.hit.Ng_z));
            SampleRay sample = UniformSampleHemiSphere(payload, ray);

            return sample;
        }

        virtual vec3 EvaluateBRDF(const RTCRayHit& payload, const vec3& wi, const vec3& wo)
        {
            return m_Albedo / (2.0f * PI);
        }

        mat3 TBN(const vec3& N)
        {
            // Make vector q that is non-parallel to n
            vec3 q  = N;
            vec3 aq = abs(q);
            if (aq.x <= aq.y && aq.x <= aq.z) {
                q.x = 1.f;
            }
            else if (aq.y <= aq.x && aq.y <= aq.z) {
                q.y = 1.f;
            }
            else {
                q.z = 1.f;
            }

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

        // vec3 SampleHemiSphere(vec3 N)
        // {
        //     vec2 Xi = Hammersley(Walnut::Random::Float() * SAMPLE_COUNT, SAMPLE_COUNT);

        //     float r     = sqrt(Xi.x);
        //     float theta = 2.0 * PI * Xi.y;

        //     vec3 H{r * cos(theta), r * sin(theta), sqrt(clamp(1.0f - Xi.x, 0.0f, 1.0f))};
        //     vec3 sampleVec = TBN(N) * H;
        //     return normalize(sampleVec);
        // }

        SampleRay UniformSampleHemiSphere(RTCRayHit const& payload, Ray const& ray)
        {
            vec3 N = normalize(vec3(payload.hit.Ng_x, payload.hit.Ng_y, payload.hit.Ng_z));

            vec2 Xi = Hammersley(Walnut::Random::Float() * SAMPLE_COUNT, SAMPLE_COUNT);

            float r     = sqrt(Xi.x);
            float theta = 2.0 * PI * Xi.y;

            vec3 H{r * cos(theta), r * sin(theta), sqrt(clamp(1.0f - Xi.x, 0.0f, 1.0f))};
            vec3 sampleVec = TBN(N) * H;

            vec3  p     = ray.At(payload.ray.tfar) + N * bias;
            float NdotL = max(0.0f, dot(N, -ray.d));
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

        virtual SampleRay Sample(const Ray& ray, const RTCRayHit& payload) override
        {
            vec3 N      = normalize(vec3(payload.hit.Ng_x, payload.hit.Ng_y, payload.hit.Ng_z));
            vec2 Xi     = Hammersley(SAMPLE_COUNT * Walnut::Random::Float(), SAMPLE_COUNT);
            vec3 wi     = -normalize(ray.d);
            vec4 sample = ImportanceSampleGGX(Xi, N, m_Roughness);
            vec3 H      = sample;

            float pdf   = DistributionGGX(N, H, m_Roughness) * dot(N, H);
            vec3  wo    = normalize(reflect(-wi, H));
            float NdotL = max(0.0f, dot(wi, N));

            // return SampleRay{.ray = {ray.At(payload.ray.tfar) + N * 0.001f, wo}, .pdf = NdotL};
            return UniformSampleHemiSphere(payload, ray);
        }

        virtual vec3 EvaluateBRDF(const RTCRayHit& payload, const vec3& wi, const vec3& wo) override
        {
            vec3  N     = normalize(vec3(payload.hit.Ng_x, payload.hit.Ng_y, payload.hit.Ng_z));
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

        vec2 IntegrateBRDF(float NdotV, float roughness)
        {
            vec3 V;
            V.x = sqrt(1.0 - NdotV * NdotV);  // sin(Theta)
            V.y = 0.0;
            V.z = NdotV;  // cos(Theta)

            float A = 0.0;
            float B = 0.0;

            vec3 N = vec3(0.0, 0.0, 1.0);

            // generates a sample vector that's biased towards the
            // preferred alignment direction (importance sampling).
            vec2 Xi = Hammersley(Walnut::Random::Float() * SAMPLE_COUNT, SAMPLE_COUNT);
            vec3 H  = ImportanceSampleGGX(Xi, N, roughness);  // Wo
            vec3 L  = normalize(2.0f * dot(V, H) * H - V);

            float NdotL = max(L.z, 0.0f);
            float NdotH = max(H.z, 0.0f);
            float VdotH = max(dot(V, H), 0.0f);

            float G     = GeometrySmith(N, V, L, roughness);
            float G_Vis = (G * VdotH) / (NdotH * NdotV);
            float Fc    = pow(1.0 - VdotH, 5.0);
            // float GWf   = (VdotH * L.z) / NdotH;

            A = (1.0 - Fc) * G_Vis;
            B = Fc * G_Vis;
            return vec2(A, B);
        }

        float DistributionGGX(vec3 N, vec3 H, float roughness)
        {
            float a      = roughness * roughness;
            float a2     = a * a;
            float NdotH  = dot(N, H);
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

        vec4 ImportanceSampleGGX(vec2 Xi, vec3 N, float roughness)
        {
            float a  = roughness * roughness;
            float a2 = a * a;

            float phi      = 2.0 * PI * Xi.x;
            float cosTheta = sqrt((1.0 - Xi.y) / (1.0 + (a2 - 1.0) * Xi.y));
            float sinTheta = sqrt(1.0 - cosTheta * cosTheta);

            // from spherical coordinates to cartesian coordinates - halfway vector
            vec3 H;
            H.x = cos(phi) * sinTheta;
            H.y = sin(phi) * sinTheta;
            H.z = cosTheta;

            // float d   = (cosTheta * a2 - cosTheta) * cosTheta + 1;
            // float D   = a2 / (PI * d * d);
            // float pdf = D * cosTheta;

            // from tangent-space H vector to world-space sample vector
            vec3 up        = abs(N.z) < 0.999 ? vec3(0.0, 0.0, 1.0) : vec3(1.0, 0.0, 0.0);
            vec3 tangent   = normalize(cross(up, N));
            vec3 bitangent = normalize(cross(N, tangent));

            mat3 TBN{tangent, bitangent, N};

            vec3 sampleVec = normalize(TBN * H);
            return vec4(sampleVec, 1.0);
        }

    public:
        float m_Roughness = 0.0f;
        float m_Metallic  = 0.0f;
    };

}  // namespace soft