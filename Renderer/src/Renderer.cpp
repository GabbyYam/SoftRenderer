#include "Renderer.h"
#include "Texture/Texture2D.hpp"
#include "Walnut/Image.h"
#include <algorithm>
#include <execution>
#include <glm/geometric.hpp>
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

        if (postProcessingSetting.ToneMapping)
            c = ACES_ToneMapping(c);

        if (postProcessingSetting.Gamma)
            c = utils::GammaCorrection(c);

        c = glm::clamp(c, 0.0f, 1.0f);

        // m_ImageData[x + y * m_FramebufferImage->GetWidth()] = utils::ConvertToRGBA(c);
        m_ImageData[x + y * m_FramebufferImage->GetWidth()] = vec4(c, 1.0);
    }

    void Renderer::FXAA()
    {
        // if (postProcessingSetting.MT) {
        //     std::for_each(std::execution::par, begin(m_VerticalIter) + 1, end(m_VerticalIter) - 1, [this](uint32_t j) {
        //         std::for_each(std::execution::par, begin(m_HorizontalIter) + 1, end(m_HorizontalIter) - 1, [this, j](uint32_t i) {
        //             vec3 a = utils::ConvertToColor(m_ImageData, i - 1, j, m_Width),
        //                  b = utils::ConvertToColor(m_ImageData, i + 1, j, m_Width),
        //                  c = utils::ConvertToColor(m_ImageData, i, j - 1, m_Width),
        //                  d = utils::ConvertToColor(m_ImageData, i, j + 1, m_Width),
        //                  o = utils::ConvertToColor(m_ImageData, i, j, m_Width);

        //             float left = glm::length(a), right = glm::length(b), top = glm::length(c), bottom = glm::length(d);

        //             float mn        = std::min({left, right, top, bottom});
        //             float mx        = std::max({left, right, top, bottom});
        //             float contrast  = mx - mn;
        //             float threshold = contrast * 0.25f;
        //             float blend     = 0.25f;
        //             if (contrast > 0.8)
        //                 SetPixelColor(i, j, blend * (a + b + c + d));
        //         });
        //     });
        // }
        // else {
        //     for (uint32_t j = 1; j < m_Height - 1; ++j) {
        //         for (uint32_t i = 1; i < m_Width - 1; ++i) {
        //             vec3 a = utils::ConvertToColor(m_ImageData, i - 1, j, m_Width),
        //                  b = utils::ConvertToColor(m_ImageData, i + 1, j, m_Width),
        //                  c = utils::ConvertToColor(m_ImageData, i, j - 1, m_Width),
        //                  d = utils::ConvertToColor(m_ImageData, i, j + 1, m_Width),
        //                  o = utils::ConvertToColor(m_ImageData, i, j, m_Width);

        //             float left = glm::length(a), right = glm::length(b), top = glm::length(c), bottom = glm::length(d);

        //             float mn        = std::min({left, right, top, bottom});
        //             float mx        = std::max({left, right, top, bottom});
        //             float contrast  = mx - mn;
        //             float threshold = contrast * 0.25f;
        //             float blend     = 0.25f;
        //             if (contrast > 0.8)
        //                 SetPixelColor(i, j, blend * (a + b + c + d));
        //         }
        //     }
        // }
        // m_FramebufferImage->SetData(m_ImageData);
    }

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