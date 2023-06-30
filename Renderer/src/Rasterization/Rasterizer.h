#pragma once

#include "Buffer/Buffer.hpp"
#include "Geometry/Mesh.hpp"
#include "Geometry/Scene.hpp"
#include "Rasterization/Shader/Shader.hpp"
#include "Renderer.h"
#include <cstddef>
#include <memory>
#include <spdlog/spdlog.h>
#include <stdint.h>
#include <vector>

namespace soft {

    enum class ShadeMode { Shaded, WireFrame };
    enum class CullingMode { Back, Front, None };

    class Rasterizer : public Renderer {
    public:
        Rasterizer(){};
        Rasterizer(const std::shared_ptr<Scene> scene) : m_Scene(scene){};
        virtual ~Rasterizer();

        virtual void Render(const std::shared_ptr<Camera> camera) override;
        virtual void OnResize(uint32_t w, uint32_t h) override;
        virtual void OnUpdate(float ts) override;

        void SetShaderMode(ShadeMode shadeMode)
        {
            m_ShadeMode = shadeMode;
        }

        void SetCullingMode(CullingMode culling)
        {
            m_Culling = culling;
        }

        void SetDirecitonalLight(const glm::vec3& dir)
        {
            m_DirectionalLight = dir;
        }

    private:
        void Line(const point2& p1, const point2& p2, const color& c);
        void BresenhamLine(point2 p1, point2 p2, const color& c);

        glm::vec3 Barycentric(const Primitive& primitive, const point2 p);
        void      PrimitiveAssembly();

        void Triangle(const Primitive& primitive);
        void Triangle(const Primitive& primitive, const std::shared_ptr<Texture2D> texture);

        bool CullBack(const glm::vec3& normal, const glm::vec3& viewDir);

    public:
        glm::vec3     m_DirectionalLight = {1.f, 0.f, -1.f};
        Buffer<float> m_Zbuffer;

        ShadeMode   m_ShadeMode = ShadeMode::Shaded;
        CullingMode m_Culling   = CullingMode::Back;

        glm::mat4 mvp{1.0f};

        std::shared_ptr<Scene> m_Scene;
        std::vector<uint32_t>  m_IndexIter;
    };
}  // namespace soft