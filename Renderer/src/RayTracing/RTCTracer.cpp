#include "RTCTracer.hpp"
#include "GLFW/glfw3.h"
#include "Geometry/Geometry.hpp"
#include "Geometry/Mesh.hpp"
#include "Geometry/Scene.hpp"
#include "RayTracing/Material/Material.hpp"
#include "RayTracing/RTCTracer.hpp"
#include "RayTracing/Ray.hpp"
#include "Texture/Texture2D.hpp"
#include "Walnut/Logger.hpp"
#include "Walnut/Random.h"
#include "tbb/parallel_for.h"
#include <assimp/texture.h>
#include <cstddef>
#include <embree3/rtcore_device.h>
#include <embree3/rtcore_scene.h>
#include <execution>
#include <glm/common.hpp>
#include <glm/detail/qualifier.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <glm/ext/quaternion_geometric.hpp>
#include <glm/fwd.hpp>
#include <glm/geometric.hpp>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/dual_quaternion.hpp>
#include <glm/trigonometric.hpp>
#include <memory>
#include <oneapi/tbb/blocked_range.h>
#include <oneapi/tbb/blocked_range2d.h>
#include <oneapi/tbb/parallel_for.h>
#include <spdlog/spdlog.h>
#include <stdint.h>
#include <stdlib.h>
#include <unordered_map>
#include <vcruntime.h>
#include <vcruntime_string.h>
#include <vector>

namespace soft {

    RTCTracer::RTCTracer()
    {
        Initialize();
        AddSphere(vec3(0.0, 1.0, 0.0f), 1.0f, 0);
        rtcSetSceneFlags(m_rtc_Scene, RTC_SCENE_FLAG_ROBUST);
        rtcCommitScene(m_rtc_Scene);
    }

    RTCTracer::RTCTracer(const std::shared_ptr<Scene> scene) : m_ActiveScene(scene)
    {
        Initialize();

        rtcCommitScene(m_rtc_Scene);
    }

    RTCTracer::~RTCTracer()
    {
        rtcReleaseScene(m_rtc_Scene);
        rtcReleaseDevice(m_rtc_Device);
    }

    void RTCTracer::Initialize()
    {
        m_rtc_Device = rtcNewDevice(nullptr);
        m_rtc_Scene  = rtcNewScene(m_rtc_Device);

        SetupScene();

        m_SampleIter.resize(4);
        iota(begin(m_SampleIter), end(m_SampleIter), 0);
    }

    RTCRayHit RTCTracer::TraceRay(const Ray& ray)
    {
        RTCIntersectContext context;
        rtcInitIntersectContext(&context);

        RTCRayHit rayhit;
        rayhit.ray.org_x = ray.o.x;
        rayhit.ray.org_y = ray.o.y;
        rayhit.ray.org_z = ray.o.z;
        rayhit.ray.dir_x = ray.d.x;
        rayhit.ray.dir_y = ray.d.y;
        rayhit.ray.dir_z = ray.d.z;
        rayhit.ray.tnear = 0;
        rayhit.ray.tfar  = 1e3;

        rayhit.ray.mask      = -1;
        rayhit.ray.flags     = 0;
        rayhit.hit.geomID    = RTC_INVALID_GEOMETRY_ID;
        rayhit.hit.instID[0] = RTC_INVALID_GEOMETRY_ID;
        rayhit.hit.primID    = RTC_INVALID_GEOMETRY_ID;

        rtcIntersect1(m_rtc_Scene, &context, &rayhit);
        return rayhit;
    }

    void RTCTracer::Render(const std::shared_ptr<Camera> camera)
    {
        m_ActiveCamera = camera;
        if (m_FrameIndex == 1)
            memset(m_AccumulatedData, 0, m_Width * m_Height * sizeof(glm::vec3));

#define STL_PAR 1
#if STL_PAR
        std::for_each(std::execution::par, begin(m_VerticalIter), end(m_VerticalIter), [this](uint32_t j) {
            std::for_each(std::execution::par, begin(m_HorizontalIter), end(m_HorizontalIter), [this, j](uint32_t i) {
                vec3 color = PerPixel(i, j);

                if (color.r != color.r)
                    color.r = 0.0;
                if (color.g != color.g)
                    color.g = 0.0;
                if (color.b != color.b)
                    color.b = 0.0;

                int index = i + j * m_FramebufferImage->GetWidth();
                m_AccumulatedData[index] += color;

                vec3 accumulateColor = m_AccumulatedData[index];
                accumulateColor /= (float)m_FrameIndex;
                SetPixelColor(i, j, accumulateColor);
            });
        });

#elif OMP_PAR
#    pragma omp parallel for
        for (int j = 0; j < m_Height; ++j) {
#    pragma omp parallel for
            for (int i = 0; i < m_Width; ++i) {
                vec3 color = PerPixel(i, j);
                int  index = i + j * m_FramebufferImage->GetWidth();
                m_AccumulatedData[index] += color;

                vec3 accumulateColor = m_AccumulatedData[index];
                accumulateColor /= (float)m_FrameIndex;
                SetPixelColor(i, j, accumulateColor);
            }
        }
#elif TBB_PAR
        tbb::parallel_for(tbb::blocked_range2d<size_t>(0, m_Height, 0, m_Width), [&](tbb::blocked_range2d<size_t> r) {
            for (size_t j = r.rows().begin(); j < r.rows().end(); ++j) {
                for (size_t i = r.cols().begin(); i < r.cols().end(); ++i) {
                    vec3 color = PerPixel(i, j);
                    int  index = i + j * m_FramebufferImage->GetWidth();
                    m_AccumulatedData[index] += color;

                    vec3 accumulateColor = m_AccumulatedData[index];
                    accumulateColor /= (float)m_FrameIndex;
                    SetPixelColor(i, j, accumulateColor);
                }
            }
        });
#else
        std::for_each(begin(m_VerticalIter), end(m_VerticalIter), [this](uint32_t j) {
            std::for_each(begin(m_HorizontalIter), end(m_HorizontalIter), [this, j](uint32_t i) {
                vec3 color = PerPixel(i, j);
                int  index = i + j * m_FramebufferImage->GetWidth();
                m_AccumulatedData[index] += color;

                vec3 accumulateColor = m_AccumulatedData[index];
                accumulateColor /= (float)m_FrameIndex;
                SetPixelColor(i, j, accumulateColor);
            });
        });
#endif

        ++m_FrameIndex;
        m_FramebufferImage->SetData(m_ImageData);
    }

    vec3 RTCTracer::PerPixel(uint32_t x, uint32_t y)
    {
        vec3 Lo(0.0f);
        vec3 contribution(1.0f);

        vec3 r = Walnut::Random::Vec3() * 0.001f;
        Ray  ray(m_ActiveCamera->GetPosition(), m_ActiveCamera->GetRayDirection(x, y) + r);

        float prob           = rayTraceSetting.prob;
        float probMultiplier = 1.0 / prob;
        float NdotL          = 0.0f;

        int bounce = 1;

        while (true) {
            // Intersect with scene
            auto payload = TraceRay(ray);

            if (payload.hit.geomID == RTC_INVALID_GEOMETRY_ID) {
                if (lightSetting.useEnvironmentMap) {
                    vec3 Li = m_EnvironmentMap ? m_EnvironmentMap->SampleCube(ray.d) : vec3{0.6, 0.7, 0.9};
                    Lo += Li * contribution;
                }
                break;
            }

            // Cull Back Face
            vec3 N  = normalize(vec3{payload.hit.Ng_x, payload.hit.Ng_y, payload.hit.Ng_z});
            vec3 wi = -ray.d;
            if (dot(N, wi) < 0.0f)
                break;

            // Interpolate primitive
            auto geometry = m_ActiveScene->geometries[payload.hit.geomID];
            if (!geometry->primitives.empty()) {
                // Barycentric Coordinate
                auto primitive = *geometry->primitives[payload.hit.primID];
                auto coord     = Barycentric(primitive, ray.At(payload.ray.tfar));

                // Normal
                N = primitive[0].normal * coord.x + primitive[1].normal * coord.y + primitive[2].normal * coord.z;

                // Sample texture
                if (m_ActiveScene->textureIndex[payload.hit.geomID] >= 0) {
                    auto diffuseMap = m_ActiveScene->textures[m_ActiveScene->textureIndex[payload.hit.geomID]];

                    auto uv = primitive[0].texCoord * coord.x + primitive[1].texCoord * coord.y + primitive[2].texCoord * coord.z;
                    uv      = clamp(uv, 0.0f, 1.0f);
                    contribution *= diffuseMap->Sample2D(uv);
                }
            }

            auto material = m_ActiveScene->materials[m_ActiveScene->materialIndex[payload.hit.geomID]];

            if (bounce++ == 1)
                Lo += material->Emission();

            // Direct Light
            if (rayTraceSetting.sampleFromLight) {
                // Sample random point from light source
                vec3  lightPosition{-0.25 + 0.5 * Walnut::Random::Float(), 1, -0.25 + 0.5 * Walnut::Random::Float()};
                vec3  p        = ray.At(payload.ray.tfar);
                vec3  lightToP = p - lightPosition;
                float distance = glm::length(lightToP);
                lightToP /= distance;
                auto targetPayload = TraceRay(Ray(p + N * bias, -lightToP));

                if (targetPayload.hit.geomID != RTC_INVALID_GEOMETRY_ID) {
                    auto targetMaterial = m_ActiveScene->materials[m_ActiveScene->materialIndex[targetPayload.hit.geomID]];
                    if (targetMaterial->IsLightSource()) {
                        NdotL     = max(0.0f, dot(-lightToP, N));
                        vec3 Li   = targetMaterial->Emission();
                        vec3 brdf = material->EvaluateBRDF(payload, wi, -lightToP);

                        float lightArea = 0.5f * 0.5f;
                        float pdf       = 1.0 / lightArea;

                        float attenuation   = min(1.0f, 1.0f / (distance * distance));
                        vec3  contributionD = (brdf / pdf) * NdotL;

                        Lo += Li * contributionD * contribution * attenuation;
                    }
                }
            }

            // Russian Roulette or Hit Light
            if (Walnut::Random::Float() > prob || material->IsLightSource())
                break;

            // Calculate Indirect Light
            auto  sampleRay = material->Sample(ray, payload);
            vec3  brdf      = material->EvaluateBRDF(payload, -ray.d, sampleRay.ray.d);
            float pdf       = sampleRay.pdf;

            NdotL = max(0.0f, dot(wi, N));
            contribution *= (brdf / pdf) * NdotL;

            ray = sampleRay.ray;
        }
        return Lo;
    }

    void RTCTracer::SetupScene()
    {
        Material white      = {{1.0, 1.0, 1.0}};
        Material whiteGloss = {{1.0, 1.0, 1.0}};
        Material pink       = {{1, .3, .4}};
        Material green      = {{0.1, 0.9, 0.1}};
        Material red        = {{0.9, .1, .1}};
        // Material lightSource = {vec3(1.0), 1.0f, vec3(0.85, 0.8, 0.4), 30.0f};
        Material    lightSource  = {vec3(1.0), vec3(1.0), 16.0f};
        Material    glass        = {{.9, .9, .9}};
        Material    glossy       = {vec3{1.0f}};
        PBRMaterial metal        = PBRMaterial{vec3{1.0f}, 0.3f, 1.0f};
        PBRMaterial glossySphere = PBRMaterial{vec3{1.0f}, 0.1f, 0.0f};

        if (m_ActiveScene == nullptr)
            m_ActiveScene = std::make_shared<Scene>();

        auto& materials = m_ActiveScene->materials;
        materials.resize(10);
        materials[0] = std::make_shared<Material>(white);
        materials[1] = std::make_shared<Material>(whiteGloss);
        materials[2] = std::make_shared<Material>(green);
        materials[3] = std::make_shared<Material>(red);
        materials[4] = std::make_shared<Material>(lightSource);
        materials[5] = std::make_shared<Material>(glass);
        materials[6] = std::make_shared<PBRMaterial>(metal);
        materials[7] = std::make_shared<Material>(glossy);
        materials[8] = std::make_shared<PBRMaterial>(glossySphere);
#define BOX 1
#ifdef Model
        for (auto& model : m_ActiveScene->models) {
            // int randomIndex = Walnut::Random::UInt() % m_ActiveScene->materials.size();
            AddModel(model, 0);
        }
#elif BOX

        auto cube = std::vector<Quad>{
            {{-1, -1, 1}, {-1, -1, -1}, {-1, 1, 1}, {-1, 1, -1}}, {{1, -1, 1}, {1, 1, 1}, {1, -1, -1}, {1, 1, -1}},
            {{1, 1, -1}, {1, 1, 1}, {-1, 1, -1}, {-1, 1, 1}},     {{1, -1, -1}, {-1, -1, -1}, {1, -1, 1}, {-1, -1, 1}},
            {{-1, -1, -1}, {1, -1, -1}, {-1, 1, -1}, {1, 1, -1}}, {{-1, -1, 1}, {-1, 1, 1}, {1, -1, 1}, {1, 1, 1}},
        };

        // Cornell box
        // AddQuad(cube[0], 3);
        // AddQuad(cube[1], 2);
        // AddQuad(cube[2], 0);
        AddQuad(cube[3], 0);
        // AddQuad(cube[4], 0);
        AddSphere(vec3{0.0, 0.0f, 0.0f}, 0.4f, 8);

        // Lamp
        float bias = 0.0001f;
        AddQuad({{0.25, 1.0 - bias, -0.25}, {0.25, 1.0 - bias, 0.25}, {-0.25, 1.0 - bias, -0.25}, {-0.25, 1.0 - bias, 0.25}}, 4);

        // // Object
        // auto translate = glm::translate(mat4(1.0), vec3(-0.35, -0.4, -0.3));
        // auto rotate    = glm::rotate(mat4(1.0), glm::radians(15.0f), vec3(0, 1, 0));
        // auto scale     = glm::scale(mat4(1.0), vec3(0.3, 0.6, 0.3));
        // for (auto quad : cube) {
        //     quad.SetTransform(translate * rotate * scale);
        //     AddQuad(quad, 0, true);
        // }

        // translate = glm::translate(mat4(1.0), vec3(0.35, -0.7, 0.3));
        // rotate    = glm::rotate(mat4(1.0), glm::radians(-15.0f), vec3(0, 1, 0));
        // scale     = glm::scale(mat4(1.0), vec3(0.3, 0.3, 0.3));
        // for (auto quad : cube) {
        //     quad.SetTransform(translate * rotate * scale);
        //     AddQuad(quad, 0, true);
        // }

#endif
    }

    void RTCTracer::AddModel(const std::shared_ptr<Model> model, int materialIndex)
    {
        for (auto& mesh : model->GetMeshes()) {
            int primitiveNum = mesh.indices.size() / 3;
            debug("Primitive num = {}", primitiveNum);

            RTCGeometry geom = rtcNewGeometry(m_rtc_Device, RTC_GEOMETRY_TYPE_TRIANGLE);

            // Submit Vertex buffer
            float* vertices = static_cast<float*>(
                rtcSetNewGeometryBuffer(geom, RTC_BUFFER_TYPE_VERTEX, 0, RTC_FORMAT_FLOAT3, 3 * sizeof(float), mesh.vertices.size()));

            // World Space Transform
#pragma region Rotate
            auto translate   = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, 1.0f));
            auto rotateX     = glm::rotate(glm::mat4(1.0f), glm::radians(-90.f), glm::vec3(1.0f, 0.0f, 0.0f));
            auto modelMatrix = translate * rotateX;

            for (auto& vert : mesh.vertices)
                vert.position = modelMatrix * vec4(vert.position, 1.0f);
#pragma endregion

            for (int i = 0; i < mesh.vertices.size(); i++)
                memcpy(vertices + i * 3, glm::value_ptr(mesh.vertices[i].position), 3 * sizeof(float));

            // Submit Index buffer
            uint32_t* indices = static_cast<uint32_t*>(rtcSetNewGeometryBuffer(geom, RTC_BUFFER_TYPE_INDEX, 0, RTC_FORMAT_UINT3,
                                                                               3 * sizeof(unsigned), mesh.indices.size() / 3));
            memcpy(indices, mesh.indices.data(), mesh.indices.size() * sizeof(uint32_t));

            // Commit geometry
            rtcCommitGeometry(geom);
            uint32_t id = rtcAttachGeometry(m_rtc_Scene, geom);
            rtcReleaseGeometry(geom);
            debug("Model geomId = {}", id);

            // Bind material and texture in order
            m_ActiveScene->materialIndex.push_back(materialIndex);
            m_ActiveScene->textureIndex.push_back(m_ActiveScene->textures.size());
            m_ActiveScene->textures.push_back(std::make_shared<Texture2D>(mesh.textures.front()));

            auto& geometry = m_ActiveScene->geometries.emplace_back(std::make_shared<Geometry>());
            // Save primitive for lookup texture coord
            for (int i = 0; i < mesh.indices.size() / 3; i++) {
                Primitive primitive{mesh.vertices[mesh.indices[i * 3]], mesh.vertices[mesh.indices[i * 3 + 1]],
                                    mesh.vertices[mesh.indices[i * 3 + 2]]};
                geometry->primitives.emplace_back(std::make_shared<Primitive>(primitive));
            }
        }
    }

    void RTCTracer::AddSphere(const vec3& center, float radius, int materialIndex)
    {
        RTCGeometry geom = rtcNewGeometry(m_rtc_Device, RTC_GEOMETRY_TYPE_SPHERE_POINT);

        auto* vertices = (float*)rtcSetNewGeometryBuffer(geom, RTC_BUFFER_TYPE_VERTEX, 0, RTC_FORMAT_FLOAT4, 4 * sizeof(float), 1);
        vertices[0]    = static_cast<float>(center.x);
        vertices[1]    = static_cast<float>(center.y);
        vertices[2]    = static_cast<float>(center.z);
        vertices[3]    = static_cast<float>(radius);

        rtcCommitGeometry(geom);
        auto id = rtcAttachGeometry(m_rtc_Scene, geom);
        rtcReleaseGeometry(geom);
        m_ActiveScene->materialIndex.push_back(materialIndex);
        m_ActiveScene->textureIndex.push_back(-1);
        auto& geometry = m_ActiveScene->geometries.emplace_back(std::make_shared<Geometry>());
        debug("Sphere geomId = {}", id);
    }

    void RTCTracer::AddTriangle(const Primitive& primitive, int materialIndex)
    {
        RTCGeometry geom = rtcNewGeometry(m_rtc_Device, RTC_GEOMETRY_TYPE_TRIANGLE);

        // Submit Vertex buffer
        float* vertices = (float*)rtcSetNewGeometryBuffer(geom, RTC_BUFFER_TYPE_VERTEX, 0, RTC_FORMAT_FLOAT3, 3 * sizeof(float), 3);

        for (int i = 0; i < 3; ++i) {
            vertices[i * 3]     = primitive[i].position[0];
            vertices[i * 3 + 1] = primitive[i].position[1];
            vertices[i * 3 + 2] = primitive[i].position[2];
        }

        // Submit Index buffer
        unsigned* indices =
            (unsigned*)rtcSetNewGeometryBuffer(geom, RTC_BUFFER_TYPE_INDEX, 0, RTC_FORMAT_UINT3, 3 * sizeof(unsigned), 1);
        indices[0] = 0;
        indices[1] = 1;
        indices[2] = 2;

        rtcCommitGeometry(geom);
        auto id = rtcAttachGeometry(m_rtc_Scene, geom);
        rtcReleaseGeometry(geom);

        // Bind material and texture
        m_ActiveScene->materialIndex.push_back(materialIndex);
        m_ActiveScene->textureIndex.push_back(-1);
        auto& geometry = m_ActiveScene->geometries.emplace_back(std::make_shared<Geometry>());

        debug("Triangle geomId = {}", id);
    }

    void RTCTracer::AddQuad(Quad const& quad, int materialIndex, bool reverseNormal)
    {
        vec3 x0 = quad.x0, x1 = quad.x1, x2 = quad.x2, x3 = quad.x3;
        if (reverseNormal) {
            AddTriangle({x2, x1, x0}, materialIndex);
            AddTriangle({x3, x1, x2}, materialIndex);
        }
        else {
            AddTriangle({x0, x1, x2}, materialIndex);
            AddTriangle({x2, x1, x3}, materialIndex);
        }
    }

    vec3 RTCTracer::Barycentric(const Primitive& primitive, const vec3& p)
    {
        vec3 v1(primitive[1].position.x - primitive[0].position.x, primitive[2].position.x - primitive[0].position.x,
                primitive[0].position.x - p.x);
        vec3 v2(primitive[1].position.y - primitive[0].position.y, primitive[2].position.y - primitive[0].position.y,
                primitive[0].position.y - p.y);

        auto u = cross(v1, v2);

        // High precision case
        // auto u =
        //     vec3(v1.y * v2.z - v1.z * v2.y, v1.z * v2.x - v1.x * v2.z, v1.x * v2.y - v1.y * v2.x);

        // if (std::abs(u.z) < 1.0f)
        //     return glm::vec3(-1, 1, 1);

        return glm::vec3(1.f - ((u.x + u.y) / (float)u.z), u.x / (float)u.z, u.y / (float)u.z);
    }

}  // namespace soft