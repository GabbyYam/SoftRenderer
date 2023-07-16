#include "RayTracer.h"
#include "Geometry/Mesh.hpp"
#include "Geometry/Scene.hpp"
#include "RayTracing/Material/Material.hpp"
#include "RayTracing/Ray.hpp"
#include "RayTracing/RayTracer.h"
#include "RayTracing/Shape/Triangle.hpp"
#include "Renderer.h"
#include "Walnut/Random.h"
#include <GLFW/glfw3.h>
#include <algorithm>
#include <embree3/rtcore_device.h>
#include <embree3/rtcore_scene.h>
#include <execution>
#include <glm/common.hpp>
#include <glm/ext/quaternion_geometric.hpp>
#include <glm/fwd.hpp>
#include <glm/geometric.hpp>
#include <glm/gtc/constants.hpp>
#include <memory>
#include <spdlog/spdlog.h>
#include <stdint.h>
#include <vcruntime.h>
#include <vcruntime_string.h>

namespace soft {

    RayTracer::RayTracer() {}

    RayTracer::RayTracer(const std::shared_ptr<Scene> scene) : m_Scene(scene)
    {
        SetupScene();
    }

    RayTracer::~RayTracer() {}

    void RayTracer::Render(const std::shared_ptr<Camera> camera)
    {
        m_ActiveCamera     = camera;
        int samplePerPixel = 8;
        if (m_FrameIndex == 1)
            memset(m_AccumulatedData, 0, m_Width * m_Height * sizeof(glm::vec3));

#define MT 1
#if MT
        std::for_each(std::execution::par, begin(m_VerticalIter), end(m_VerticalIter), [this](uint32_t j) {
            std::for_each(std::execution::par, begin(m_HorizontalIter), end(m_HorizontalIter), [this, j](uint32_t i) {
                vec3 color = PerPixel(i, j);
                int  index = i + j * m_FramebufferImage->GetWidth();
                m_AccumulatedData[index] += color;

                vec3 accumulateColor = m_AccumulatedData[index];
                accumulateColor /= (float)m_FrameIndex;
                accumulateColor = glm::clamp(accumulateColor, vec3(0.0f), vec3(1.0f));
                SetPixelColor(i, j, accumulateColor);
            });
        });
#else
        for (int j = 0; j < m_Height; ++j) {
            for (int i = 0; i < m_Width; ++i) {
                vec3 color = PerPixel(i, j);

                int index = i + j * m_FramebufferImage->GetWidth();
                m_AccumulatedData[index] += color;

                vec3 accumulateColor = m_AccumulatedData[index];
                accumulateColor /= (float)m_FrameIndex;
                accumulateColor = glm::clamp(accumulateColor, vec3(0.0f), vec3(1.0f));
                SetPixelColor(i, j, accumulateColor);
            }
        }
#endif
        ++m_FrameIndex;
        m_FramebufferImage->SetData(m_ImageData);
    }

    void RayTracer::OnResize(uint32_t w, uint32_t h)
    {
        Renderer::OnResize(w, h);
    }

    vec3 RayTracer::PerPixel(uint32_t x, uint32_t y)
    {
        vec3 light{0.0f};
        vec3 contribution{1.0f};
        vec3 attenuation{1.0f};

        Ray ray({m_ActiveCamera->GetPosition(), m_ActiveCamera->GetRayDirection(x, y)});
        for (int i = 0; i < rayTraceSetting.bounce; ++i) {
            HitPayload payload;
            TraceRay(ray, payload);

            if (payload.hitDistance < 0.0f) {
                vec3 skyColor{0.6f, .7f, .9f};
                light += skyColor * contribution;
                // light += skyColor * attenuation;
                break;
            }

            const auto& o   = m_ActiveScene->hittables[payload.objectIndex];
            const auto& mat = m_ActiveScene->materials[o->materialIndex];

            contribution *= mat->m_Albedo;
            light += mat->Emission();

            ray.o = payload.position + payload.normal * 0.0001f;
            // ray.d = glm::reflect(ray.d, payload.normal +
            //                                 Walnut::Random::Vec3(-0.5, 0.5) * mat->rounghness);
            ray.d = payload.normal + Walnut::Random::InUnitSphere();
        }
        return light;
    }

    void RayTracer::TraceRay(const Ray& ray, HitPayload& payload)
    {
        for (size_t i = 0, n = m_ActiveScene->hittables.size(); i < min(500, (int)n); i++) {
            const auto& o = m_ActiveScene->hittables[i];
            if (o->Intersect(ray, payload, 1000.0f))
                payload.objectIndex = i;
        }

        return payload.objectIndex < 0 ? Miss(ray, payload) : ClosestHit(ray, payload);
    }

    void RayTracer::ClosestHit(const Ray& ray, HitPayload& payload)
    {
        const auto& closest = m_ActiveScene->hittables[payload.objectIndex];
        return closest->ClosestHit(ray, payload);
    }

    void RayTracer::Miss(const Ray& ray, HitPayload& payload)
    {
        payload.hitDistance = -1.0f;
    }

    void RayTracer::OnUpdate(float ts) {}

    void RayTracer::SetupScene()
    {
        for (auto& model : m_Scene->models) {
            for (auto& mesh : model->GetMeshes()) {
                for (int i = 0; i < mesh.indices.size(); i += 3) {
                    auto&     indices  = mesh.indices;
                    auto&     vertices = mesh.vertices;
                    Primitive primitive =
                        std::array<Vertex, 3>{vertices[indices[i]], vertices[indices[i + 1]], vertices[indices[i + 2]]};
                    Triangle tri(primitive);
                    tri.materialIndex = 0;
                    m_ActiveScene->hittables.push_back(std::make_shared<Triangle>(tri));
                }
            }
        }

        debug("Setup Triangle Number = {}", m_ActiveScene->hittables.size());
    }

}  // namespace soft
