#pragma once

#include "glm/fwd.hpp"
#include <glm/glm.hpp>
#include <stdint.h>
#include <vcruntime.h>
#include <vector>

using namespace glm;

namespace soft {
    class Camera {
    public:
        Camera() = default;
        Camera(float fov, float nearClip, float farClip);
        virtual ~Camera() {}

        bool OnUpdate(float ts);
        void OnResize(uint32_t w, uint32_t h);

        const auto& GetLastMousePosition() const
        {
            return m_LastMousePosition;
        }
        const auto& GetPosition() const
        {
            return m_Position;
        }
        const auto& GetProjection() const
        {
            return m_Projection;
        }
        const auto& GetView() const
        {
            return m_View;
        }
        const auto& GetInverseProjection() const
        {
            return m_InverseProjection;
        }
        const auto& GetInverseView() const
        {
            return m_InverseView;
        }

        vec3& GetRayDirection(size_t x, size_t y);

    private:
        void  RecalculateView();
        void  RecalculateProjection();
        void  UpdateRayDirections();
        float GetRotationSpeed();

    private:
        float    m_VerticalFOV = 45.0f, m_NearClip = 0.01f, m_FarClip = 1000.f;
        uint32_t m_ViewportWidth = 0, m_ViewportHeight = 0;
        vec3     m_Position = {0.0f, 0.0f, 0.0f};
        vec3     m_Forward  = {0.0f, 0.0f, -1.0f};
        vec2     m_LastMousePosition{0.0f, 0.0f};

        mat4 m_Projection, m_InverseProjection;
        mat4 m_View, m_InverseView;

        std::vector<vec3> m_RayDirections;
    };
}  // namespace soft