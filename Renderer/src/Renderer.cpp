#include "Renderer.h"
#include "Texture/Texture2D.hpp"
#include "Walnut/Image.h"
#include <algorithm>
#include <cmath>
#include <execution>
#include <glm/geometric.hpp>
#include <glm/vector_relational.hpp>
#include <memory>
#include <spdlog/spdlog.h>
#include <stdint.h>
#include <string_view>
#include <vadefs.h>
#include <vcruntime_string.h>

namespace soft {

    void Renderer::OnResize(uint32_t w, uint32_t h)
    {
        if (m_FramebufferImage) {
            if (w == m_FramebufferImage->GetWidth() && h == m_FramebufferImage->GetHeight())
                return;
            m_FramebufferImage->Resize(w, h);
        }
        else {
            m_FramebufferImage = std::make_shared<Walnut::Image>(w, h, Walnut::ImageFormat::RGBA32F);
        }
        m_Width = w, m_Height = h;

        delete[] m_ImageData;
        m_ImageData = new vec4[w * h];

        delete[] m_AccumulatedData;
        m_AccumulatedData = new vec3[w * h];

        m_HorizontalIter.resize(w);
        m_VerticalIter.resize(h);
        std::iota(begin(m_HorizontalIter), end(m_HorizontalIter), 0);
        std::iota(begin(m_VerticalIter), end(m_VerticalIter), 0);

        ResetFrameIndex();
    }

    void Renderer::OnUpdate(float ts) {}

    void Renderer::SetPixelColor(uint32_t x, uint32_t y, color c)
    {
        if (x < 0 || y < 0 || x >= m_FramebufferImage->GetWidth() || y >= m_FramebufferImage->GetHeight()) {
            debug("out of range! x = {}, y = {}, while image size = ({}, {})", x, y, m_FramebufferImage->GetWidth(),
                  m_FramebufferImage->GetHeight());
            return;
        }

        // c.x = !std::isnan(c.x) ? c.x : 0.0f;
        // c.y = !std::isnan(c.y) ? c.y : 0.0f;
        // c.z = !std::isnan(c.z) ? c.z : 0.0f;

        if (postProcessingSetting.ToneMapping)
            c = ACES_ToneMapping(c);

        if (postProcessingSetting.Gamma)
            c = utils::GammaCorrection(c);

        c = glm::clamp(c, 0.0f, 1.0f);

        m_ImageData[x + y * m_FramebufferImage->GetWidth()] = vec4(c, 1.0);
    }

    void Renderer::FXAA() {}

    void Renderer::LoadEnvironmentMap(std::string_view path)
    {
        debug(path);
        m_EnvironmentMap = std::make_shared<Texture2D>(path.data(), Walnut::ImageFormat::RGBA32F);
        m_EnvironmentMap->LoadTextureData(path.data());
    }

    void Renderer::OnPostProcess()
    {
        if (postProcessingSetting.FXAA)
            FXAA();
    }
}  // namespace soft