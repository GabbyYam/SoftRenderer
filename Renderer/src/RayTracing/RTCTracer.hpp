#pragma once

#include "Camera/Camera.hpp"
#include "Geometry/Mesh.hpp"
#include "Geometry/Scene.hpp"
#include "RayTracing/Ray.hpp"
#include "Renderer.h"
#include <cstddef>
#include <embree3/rtcore_device.h>
#include <embree3/rtcore_geometry.h>
#include <embree3/rtcore_ray.h>
#include <memory>
#include <stdint.h>
#include <vector>

namespace soft {
    class RTCTracer : public Renderer {
    public:
        RTCTracer();
        RTCTracer(const std::shared_ptr<Scene> scene);
        virtual ~RTCTracer();
        virtual void Render(const std::shared_ptr<Camera> camera) override;
        RTCRayHit    TraceRay(const Ray& ray);
        vec3         PerPixel(uint32_t x, uint32_t y);

    private:
        void Initialize();
        void AddSphere(const vec3& center, float radius, int materialIndex);
        void AddTriangle(const Primitive& primitive, int materialIndex);
        void SetupScene();
        void AddModel(const std::shared_ptr<Model> model, int materialIndex);

        vec3 Barycentric(const Primitive& primitive, const vec3& p);

    private:
        // Embree context
        RTCDevice m_rtc_Device = nullptr;
        RTCScene  m_rtc_Scene  = nullptr;

        // My Scene Setting
        std::shared_ptr<Camera> m_ActiveCamera = nullptr;
        std::shared_ptr<Scene>  m_ActiveScene  = nullptr;
    };
}  // namespace soft