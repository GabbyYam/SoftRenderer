#pragma once

#include "Geometry/Scene.hpp"
#include "RayTracing/Ray.hpp"
#include "Renderer.h"
#include <cstddef>
#include <embree3/rtcore.h>
#include <memory>
#include <stdint.h>
#include <vector>

namespace soft {
    class RayTracer : public Renderer {
    public:
        RayTracer();
        RayTracer(const std::shared_ptr<Scene> scene);
        virtual ~RayTracer();

        virtual void Render(const std::shared_ptr<Camera> camera) override;
        virtual void OnResize(uint32_t w, uint32_t h) override;
        virtual void OnUpdate(float ts) override;
        virtual vec3 PerPixel(uint32_t x, uint32_t y);
        virtual void TraceRay(const Ray& ray, HitPayload& hitPayload);
        virtual void Miss(const Ray& ray, HitPayload& hitPayload);
        virtual void ClosestHit(const Ray& ray, HitPayload& hitPayload);

    private:
        void SetupScene();

    public:
        std::shared_ptr<Scene>  m_Scene;
        std::shared_ptr<Camera> m_ActiveCamera;
        std::shared_ptr<Scene>  m_ActiveScene;
    };
}  // namespace soft