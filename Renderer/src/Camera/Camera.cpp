#include "Camera.hpp"
#include "Camera/Camera.hpp"
#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/quaternion_geometric.hpp>
#include <glm/geometric.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/quaternion.hpp>

#include <Walnut/Input/Input.h>
#include <spdlog/spdlog.h>
#include <stdexcept>
#include <vcruntime.h>

using namespace Walnut;

namespace soft {

    Camera::Camera(float fov, float nearClip, float farClip) : m_VerticalFOV(fov), m_NearClip(nearClip), m_FarClip(farClip)
    {
        m_Position = {0.f, .0f, 3.5f};
        m_Forward  = {0.f, 0.f, -1.f};

        RecalculateView();
        RecalculateProjection();
        UpdateRayDirections();
    }

    void Camera::OnResize(uint32_t w, uint32_t h)
    {
        if (w == m_ViewportWidth && h == m_ViewportHeight)
            return;
        m_ViewportWidth  = w;
        m_ViewportHeight = h;
        RecalculateProjection();
        UpdateRayDirections();
    }

    bool Camera::OnUpdate(float ts)
    {
        glm::vec2 mousePos  = Input::GetMousePosition();
        glm::vec2 delta     = (mousePos - m_LastMousePosition) * 0.002f;
        m_LastMousePosition = mousePos;

        if (!Input::IsMouseButtonDown(MouseButton::Right)) {
            Input::SetCursorMode(CursorMode::Normal);
            return false;
        }

        Input::SetCursorMode(CursorMode::Locked);

        bool moved = false;

        constexpr glm::vec3 upDirection(0.0f, 1.0f, 0.0f);
        glm::vec3           rightDirection = glm::normalize(glm::cross(m_Forward, upDirection));

        float speed = 5.0f;

        // Movement
        if (Input::IsKeyDown(KeyCode::W)) {
            m_Position += m_Forward * speed * ts;
            moved = true;
        }
        else if (Input::IsKeyDown(KeyCode::S)) {
            m_Position -= m_Forward * speed * ts;
            moved = true;
        }
        if (Input::IsKeyDown(KeyCode::A)) {
            m_Position -= rightDirection * speed * ts;
            moved = true;
        }
        else if (Input::IsKeyDown(KeyCode::D)) {
            m_Position += rightDirection * speed * ts;
            moved = true;
        }
        if (Input::IsKeyDown(KeyCode::Q)) {
            m_Position -= upDirection * speed * ts;
            moved = true;
        }
        else if (Input::IsKeyDown(KeyCode::E)) {
            m_Position += upDirection * speed * ts;
            moved = true;
        }

        // Rotation
        if (delta.x != 0.0f || delta.y != 0.0f) {
            float pitchDelta = delta.y * GetRotationSpeed();
            float yawDelta   = delta.x * GetRotationSpeed();

            glm::quat q = glm::normalize(
                glm::cross(glm::angleAxis(-pitchDelta, rightDirection), glm::angleAxis(-yawDelta, glm::vec3(0.f, 1.0f, 0.0f))));
            m_Forward = glm::rotate(q, m_Forward);

            moved = true;
        }

        if (moved) {
            RecalculateView();
            UpdateRayDirections();
        }

        return moved;
    }

    float Camera::GetRotationSpeed()
    {
        return 0.3f;
    }

    void Camera::RecalculateView()
    {
        m_View        = glm::lookAt(m_Position, m_Position + m_Forward, glm::vec3(0, 1, 0));
        m_InverseView = glm::inverse(m_View);
    }

    void Camera::RecalculateProjection()
    {
        m_Projection =
            glm::perspectiveFov(glm::radians(m_VerticalFOV), (float)m_ViewportWidth, (float)m_ViewportHeight, m_NearClip, m_FarClip);
        // m_Projection =
        //     glm::ortho(0.f, (float)m_ViewportWidth, 0.f,
        //                (float)m_ViewportHeight, m_NearClip, m_FarClip);
        m_InverseProjection = glm::inverse(m_Projection);
    }

    void Camera::UpdateRayDirections()
    {
        if (m_ViewportHeight == 0 || m_ViewportWidth == 0)
            return;
        m_RayDirections.resize(m_ViewportWidth * m_ViewportHeight);

        for (uint32_t y = 0; y < m_ViewportHeight; y++) {
            for (uint32_t x = 0; x < m_ViewportWidth; x++) {
                glm::vec2 coord = {(float)x / (float)m_ViewportWidth, (float)y / (float)m_ViewportHeight};

                coord = coord * 2.0f - 1.0f;  // -1 -> 1

                glm::vec4 target = m_InverseProjection * glm::vec4(coord.x, coord.y, 1, 1);
                glm::vec3 rayDirection =
                    glm::vec3(m_InverseView * glm::vec4(glm::normalize(glm::vec3(target) / target.w), 0));  // World space

                m_RayDirections[x + y * m_ViewportWidth] = normalize(rayDirection);
            }
        }
    }

    vec3& Camera::GetRayDirection(size_t x, size_t y)
    {
        int index = x + y * m_ViewportWidth;
        if (index < 0 || index > m_RayDirections.size()) {
            spdlog::debug("out of range, x = {}, y = {}", x, y);
            throw std::runtime_error("out of range");
        }
        return m_RayDirections[index];
    }

}  // namespace soft