#pragma once

#include "Camera/Camera.hpp"
#include "Geometry/Mesh.hpp"
#include "Texture/Texture2D.hpp"
#include "glm/exponential.hpp"
#include <Walnut/Image.h>
#include <cstddef>
#include <glm/common.hpp>
#include <glm/fwd.hpp>
#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>
#include <memory>
#include <numeric>
#include <spdlog/spdlog.h>
#include <stdint.h>

#include <stb_image.h>
#include <string_view>
#include <vector>

#include "Config/Config.hpp"

namespace utils {

    static uint32_t ConvertToRGBA(const glm::vec3& color)
    {
        uint8_t r = (uint8_t)(color.r * 255.0f), g = (uint8_t)(color.g * 255.0f), b = (uint8_t)(color.b * 255.0f),
                a = (uint8_t)(1.0f * 255.0f);

        uint32_t c = (a << 24) | (b << 16) | (g << 8) | r;
        return c;
    }

    static soft::color GammaCorrection(const glm::vec3& color)
    {
        // return glm::sqrt(color);
        return glm::pow(color, glm::vec3(1.0f / 2.2f));
        // return glm::sqrt(color);
    }

    static vec3 ConvertToColor(const uint32_t* imageData, uint32_t x, uint32_t y, uint32_t width)
    {
        uint32_t c    = imageData[x + y * width];
        uint8_t  mask = 0xff;
        double   d    = 1.0 / 255.0f;
        float    r = uint8_t(c & mask), g = uint8_t((c >> 8) & mask), b = uint8_t((c >> 16) & mask);
        r *= d, g *= d, b *= d;
        r *= r, g *= g, b *= b;
        return vec3(r, g, b);
    }
}  // namespace utils

namespace soft {

    class Renderer {
    public:
        Renderer() {}
        Renderer(const LightSetting& lightConfig, const RayTraceSetting& rtConfig, const PostProcessingSetting& postprocessConfig)
        {
            lightSetting          = lightConfig;
            rayTraceSetting       = rtConfig;
            postProcessingSetting = postprocessConfig;
            debug("base class");
        }

        const std::shared_ptr<Walnut::Image> GetImage() const
        {
            return m_FramebufferImage;
        }

        virtual ~Renderer() {}
        virtual void Render(const std::shared_ptr<Camera> camera) {}
        virtual void Render() {}
        virtual void OnResize(uint32_t w, uint32_t h);
        virtual void OnUpdate(float ts);
        virtual void SetPixelColor(uint32_t x, uint32_t y, color c);
        virtual void OnPostProcess();

        virtual vec3 ACES_ToneMapping(const vec3& x)
        {
            float a = 2.51;
            float b = 0.03;
            float c = 2.43;
            float d = 0.59;
            float e = 0.14;

            // return glm::clamp(((x * (a * x + b)) / (x * (c * x + d) + e)), 0.0f, 1.0f);
            return ((x * (a * x + b)) / (x * (c * x + d) + e));
        }

        void ResetFrameIndex()
        {
            m_FrameIndex = 1;
        }

        uint32_t* GetImageData()
        {
            return m_ImageData;
        }

        void ReloadSetting()
        {
            LoadEnvironmentMap(lightSetting.environmentMapPath);
        }

    private:
        void FXAA();
        void LoadEnvironmentMap(std::string_view path);

    public:
        RayTraceSetting       rayTraceSetting;
        LightSetting          lightSetting;
        PostProcessingSetting postProcessingSetting;

    protected:
        uint32_t                       m_Width = 0, m_Height = 0;
        std::shared_ptr<Walnut::Image> m_FramebufferImage = nullptr;
        uint32_t*                      m_ImageData        = nullptr;
        glm::vec3*                     m_AccumulatedData  = nullptr;

        std::vector<uint32_t> m_VerticalIter, m_HorizontalIter;

        uint32_t                   m_FrameIndex     = 0;
        std::shared_ptr<Texture2D> m_EnvironmentMap = nullptr;
    };  // namespace soft

}  // namespace soft