#include "Rasterization/Rasterizer.h"
#include "Buffer/Buffer.hpp"
#include "Camera/Camera.hpp"
#include "GLFW/glfw3.h"
#include "Geometry/Mesh.hpp"
#include "Geometry/Model.hpp"
#include "Renderer.h"
#include "Texture/Texture2D.hpp"
#include "glm/common.hpp"
#include "glm/fwd.hpp"
#include "glm/geometric.hpp"
#include "glm/trigonometric.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <corecrt.h>
#include <cstddef>
#include <cstring>
#include <ctime>
#include <execution>
#include <glm/ext/matrix_transform.hpp>
#include <iostream>
#include <limits>
#include <memory>
#include <spdlog/spdlog.h>
#include <stdint.h>
#include <stdlib.h>
#include <time.h>
#include <vcruntime_string.h>
#include <vector>

using namespace glm;

namespace soft {

    Rasterizer::~Rasterizer() {}

    void Rasterizer::OnResize(uint32_t w, uint32_t h)
    {
        Renderer::OnResize(w, h);
        m_Zbuffer.Resize(w, h);
        m_Zbuffer.MemSet(std::numeric_limits<float>::infinity());
    }

    void Rasterizer::OnUpdate(float ts) {}

    void Rasterizer::Render(const std::shared_ptr<Camera> camera)
    {
        // Refresh Canvas
        vec3     skyColor    = vec3{.6f, .7f, .9f};
        vec3     gamma       = utils::GammaCorrection(skyColor);
        uint32_t canvasColor = utils::ConvertToRGBA(gamma);
        memset(m_ImageData, canvasColor, m_Width * m_Height * sizeof(uint32_t));

        std::for_each(std::execution::par, begin(m_VerticalIter), end(m_VerticalIter), [&](uint32_t y) {
            std::for_each(std::execution::par, begin(m_HorizontalIter), end(m_HorizontalIter), [&](uint32_t x) {
                if (lightSetting.useEnvironmentMap) {
                    vec3 dir = camera->GetRayDirection(x, y);
                    SetPixelColor(x, y, m_EnvironmentMap->SampleCube(dir) * 100.0f);
                }
                else {
                    SetPixelColor(x, y, skyColor);
                }
            });
        });

        float radians = 10.0f;
        std::for_each(std::execution::par, begin(m_Scene->models), end(m_Scene->models), [&](std::shared_ptr<Model>& model) {
            std::for_each(std::execution::par, begin(model->GetMeshes()), end(model->GetMeshes()), [&](const Mesh& mesh) {
                for (int i = 0; i < mesh.indices.size(); i += 3) {
                    auto translate = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 1.0f, 0.0f));
                    auto rotateX   = glm::rotate(glm::mat4(1.0f), glm::radians(-90.f), glm::vec3(1.0f, 0.0f, 0.0f));
                    auto rotateY   = glm::rotate(glm::mat4(1.0f), (float)glfwGetTime(), glm::vec3(0.f, 1.f, 0.f));
                    auto model     = translate * rotateX;

                    auto view = camera->GetView();
                    auto proj = camera->GetProjection();

                    auto mvp = proj * view;

                    auto&     indices  = mesh.indices;
                    auto&     vertices = mesh.vertices;
                    Primitive primitive =
                        std::array<Vertex, 3>{vertices[indices[i]], vertices[indices[i + 1]], vertices[indices[i + 2]]};

                    // Cull back
                    auto& p0 = primitive[0].position;
                    auto& p1 = primitive[1].position;
                    auto& p2 = primitive[2].position;

                    vec4 normal  = -vec4(glm::cross(p0 - p2, p0 - p1), 1.0f);
                    vec4 viewDir = vec4(p0 - camera->GetPosition(), 1.0f);

                    if (CullBack(normal, viewDir))
                        continue;

                    bool frustumCull = false;

                    for (int i = 0; i < 3; ++i) {
                        auto  pos      = primitive[i].position;
                        auto  v        = mvp * glm::vec4(pos, 1.0f);
                        float bias     = 0.1f;
                        float minBound = -v.w - bias, maxBound = v.w + bias;

                        // Clip Spcace
                        if (v.x <= minBound || v.x >= maxBound || v.y <= minBound || v.y >= maxBound || v.z <= minBound ||
                            v.z >= maxBound) {
                            frustumCull = true;
                            break;
                        }

                        // NDC
                        v /= v.w;

                        v.x = (1.0f + v.x) * m_Width * 0.5f;
                        v.y = (1.0f + v.y) * m_Height * 0.5f;
                        v.z = v.z;

                        primitive[i].position = v;
                        primitive[i].normal   = -model * glm::vec4(primitive[i].normal, 1.0f);
                    }

                    if (frustumCull)
                        continue;

                    switch (m_ShadeMode) {
                        case ShadeMode::WireFrame: {
                            for (int i = 0; i < primitive.size(); ++i) {
                                auto v0 = primitive[i], v1 = primitive[(i + 1) % 3];
                                Line({v0.position.x, v0.position.y}, {v1.position.x, v1.position.y}, color{1.0f});
                            }
                            break;
                        }
                        case ShadeMode::Shaded: {
                            if (!mesh.textures.empty())
                                Triangle(primitive, std::make_shared<Texture2D>(mesh.textures.front()));
                            else
                                Triangle(primitive, nullptr);
                            break;
                        }
                    }
                }
            });
        });

        m_FramebufferImage->SetData(m_ImageData);
    }

    bool Rasterizer::CullBack(const glm::vec3& normal, const glm::vec3& viewDir)
    {
        if (m_Culling == CullingMode::None)
            return false;
        bool ret = glm::dot(normal, viewDir) > 0.0f;

        return m_Culling == CullingMode::Back ? ret : !ret;
    }

    glm::vec3 Rasterizer::Barycentric(const Primitive& primitive, const point2 p)
    {
        auto& p0 = primitive[0].position;
        auto& p1 = primitive[1].position;
        auto& p2 = primitive[2].position;

        vec3 vx{p1.x, p2.x, p0.x};
        vec3 vy{p1.x, p2.x, p0.x};

        vec3 v1(p1.x - p0.x, p2.x - p0.x, p0.x - p.x);
        vec3 v2(p1.y - p0.y, p2.y - p0.y, p0.y - p.y);

        auto u = cross(v1, v2);

        return glm::vec3(1.f - ((u.x + u.y) / u.z), u.x / u.z, u.y / u.z);
    }

    void Rasterizer::Line(const point2& p1, const point2& p2, const color& c)
    {
#ifdef _WIN32
        BresenhamLine(p1, p2, c);
#else
        auto delta = p2 - p1;
        auto step  = std::max(std::abs(delta.x), std::abs(delta.y));
        delta /= step;

        for (int i = 0; i < step; ++i)
            SetPixelColor(p1.x + i * delta.x, p1.y + i * delta.y, c);
#endif
    }

    void Rasterizer::BresenhamLine(point2 p1, point2 p2, const color& c)
    {
        bool steep = false;

        if (std::abs(p2.x - p1.x) < std::abs(p2.y - p1.y)) {
            std::swap(p1.x, p1.y);
            std::swap(p2.x, p2.y);
            steep = true;
        }

        if (p2.x - p1.x < 0) {
            std::swap(p1, p2);
        }

        int  eps = 0, y = p1.y, yi = 1;
        auto delta = p2 - p1;

        if (delta.y < 0) {
            yi      = -1;
            delta.y = -delta.y;
        }

        for (int x = p1.x; x <= p2.x; ++x) {
            if (steep) {
                SetPixelColor(clamp(y, 0, (int)m_Width - 1), clamp(x, 0, (int)m_Height - 1), c);
            }
            else {
                SetPixelColor(clamp(x, 0, (int)m_Width - 1), clamp(y, 0, (int)m_Height - 1), c);
            }
            eps += delta.y;
            if ((eps << 1) >= delta.x) {
                y += yi;
                eps -= delta.x;
            }
        }
    }

    void Rasterizer::Triangle(const Primitive& primitive, const std::shared_ptr<Texture2D> texture)
    {
        // Calculate bounding box for traverse pixel per Primitive
        point2 boxmin{m_FramebufferImage->GetWidth() - 1, m_FramebufferImage->GetHeight() - 1};
        point2 boxmax{0.f, 0.f};

        for (const auto& vert : primitive) {
            boxmin.x = std::max(0.0f, std::min(boxmin.x, vert.position.x));
            boxmin.y = std::max(0.0f, std::min(boxmin.y, vert.position.y));
            boxmax.x = std::min((float)(m_FramebufferImage->GetWidth() - 1), std::max(boxmax.x, vert.position.x));
            boxmax.y = std::min((float)(m_FramebufferImage->GetHeight() - 1), std::max(boxmax.y, vert.position.y));
        }

        // Traverse pixel and do shading
        for (int x = boxmin.x; x <= boxmax.x; ++x) {
            for (int y = boxmin.y; y < boxmax.y; ++y) {
                // Calculate Barycentric Coordinate
                auto &a = primitive[0], &b = primitive[1], &c = primitive[2];
                auto  coord = Barycentric(primitive, point2(x, y));

                // Not inside the triangle
                if (coord.y < 0 || coord.z < 0 || coord.x < 0)
                    continue;

                // Interpolate z & normal
                auto z      = coord.x * a.position.z + coord.y * b.position.z + coord.z * c.position.z;
                auto normal = coord.x * a.normal + coord.y * b.normal + coord.z * c.normal;

                // Shading
                auto N = glm::normalize(normal);
                auto L = glm::normalize(m_DirectionalLight);
                // auto V =
                float NoL = std::max(0.0f, glm::dot(N, L));

                // Sampling Texture
                color albedo{1.0f};
                if (texture != nullptr) {
                    auto texCoord = coord.x * a.texCoord + coord.y * b.texCoord + coord.z * c.texCoord;
                    // info("u = {}, v = {}", a.texCoord.x, b.texCoord.y);
                    texCoord = glm::clamp(texCoord, 0.0f, 1.0f);
                    albedo   = texture->Sample2D(texCoord.x, texCoord.y);
                }

                // NoL           = max(.15f, 1.0f - glm::step(NoL, .3f));
                float ambient = 0.15;
                float diffuse = glm::step(NoL, 0.3f);

                vec3 color = (diffuse + ambient) * albedo;

                // Z-Testing
                if (z < m_Zbuffer.Get(x, y)) {
                    m_Zbuffer.Set(x, y, z);
                    SetPixelColor(x, y, color);
                }
            }
        }
    }

    void Rasterizer::Triangle(const Primitive& primitive)
    {
        Triangle(primitive, nullptr);
    }
}  // namespace soft