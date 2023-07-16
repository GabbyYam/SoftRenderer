#pragma once
#include "Walnut/Image.h"
#include <assimp/texture.h>
#include <glm/fwd.hpp>
#include <iostream>

#include <stb_image.h>
#include <stdint.h>

#include <glm/glm.hpp>

#include <spdlog/spdlog.h>

using namespace glm;
using namespace spdlog;

namespace soft {

    constexpr static float pi = 3.14159f;

    class Texture2D {
    public:
        Texture2D() = default;

        Texture2D(std::string_view path) {}
        Texture2D(std::string_view path, Walnut::ImageFormat format) : m_Format(format) {}

        virtual ~Texture2D()
        {
            // if (m_ImageData)
            //     stbi_image_free(m_ImageData);
        }

        void LoadTextureData(std::string_view path)
        {
            if (m_ImageData != nullptr) {
                stbi_image_free(m_ImageData);
            }
            if (m_Format == Walnut::ImageFormat::RGBA) {
                m_ImageData = stbi_load(path.data(), &m_Width, &m_Height, &m_Channels, STBI_rgb);
                if (m_ImageData == nullptr) {
                    spdlog::error("Failed to load texture {}", path);
                    return;
                }
            }
            else {
                m_HdrImageData = stbi_loadf(path.data(), &m_Width, &m_Height, &m_Channels, STBI_rgb);

                if (m_HdrImageData == nullptr) {
                    spdlog::error("Failed to load hdr texture {}", path);
                    return;
                }
            }

            debug("Load Texture {}, w = {}, h = {}, ch = {}", path, m_Width, m_Height, m_Channels);
        }

        void LoadTextureFromMemory(const aiTexture* aiTex)
        {
            //FBX模型用stbi_load_form_memory加载
            if (aiTex->mHeight == 0) {
                m_ImageData = stbi_load_from_memory(reinterpret_cast<uint8_t*>(aiTex->pcData), aiTex->mWidth, &m_Width, &m_Height,
                                                    &m_Channels, STBI_rgb);
            }
            else {
                m_ImageData = stbi_load_from_memory(reinterpret_cast<uint8_t*>(aiTex->pcData), aiTex->mWidth * aiTex->mHeight,
                                                    &m_Width, &m_Height, &m_Channels, STBI_rgb);
            }

            info("w = {}, h = {}, ch = {}", m_Width, m_Height, m_Channels);
        }

        const vec3 Sample2D(float u, float v) const
        {
            vec3 debugColor = {.9f, 0.0f, .7f};
            [[unlikely]] if (u < 0.0f || v < 0.0f || u > 1.0f || v > 1.0f)
                return debugColor;
            if (m_Format == Walnut::ImageFormat::RGBA) {
                [[unlikely]] if (m_ImageData == nullptr)
                    return vec3(0.0f);

                int i = u * (m_Width - 1), j = v * (m_Height - 1);
                int pixelIndex = (i + j * m_Width) * (m_Channels - 1);

                float r = m_ImageData[pixelIndex];
                float g = m_ImageData[pixelIndex + 1];
                float b = m_ImageData[pixelIndex + 2];
                vec3  color{r, g, b};
                color *= m_SolidColor;
                return vec3(r, g, b) / 255.0f;
            }
            else {
                [[unlikely]] if (m_HdrImageData == nullptr)
                    return vec3(0.0f);

                int i = u * (m_Width - 1), j = v * (m_Height - 1);
                int pixelIndex = (i + j * m_Width) * m_Channels;

                float r = m_HdrImageData[pixelIndex];
                float g = m_HdrImageData[pixelIndex + 1];
                float b = m_HdrImageData[pixelIndex + 2];
                return vec3(r, g, b);
            }
        }

        const vec3 Sample2D(const vec2& uv)
        {
            return Sample2D(uv.x, uv.y);
        }

        const vec3 SampleCube(const vec3& direction)
        {
            vec3  normalizedCoords = normalize(direction);
            float latitude         = acos(normalizedCoords.y);
            float longitude        = atan2(normalizedCoords.z, normalizedCoords.x);
            vec2  sphereCoords     = vec2(longitude, latitude) * vec2(0.5 / pi, 1.0 / pi);
            return Sample2D(1.0f - (vec2(0.5, 1.0) - sphereCoords));
        }

        const uint8_t* data() const
        {
            return m_ImageData;
        }

        const int GetWidth() const
        {
            return m_Width;
        }

        const int GetHeight() const
        {
            return m_Height;
        }

        const int GetChannels() const
        {
            return m_Channels;
        }

        void SetType(const std::string& type)
        {
            m_Type = type;
        }

        void SetPath(const std::string& path)
        {
            m_Path = path;
        }

    private:
        int         m_Width = 0, m_Height = 0, m_Channels = 0;
        uint8_t*    m_ImageData    = nullptr;
        float*      m_HdrImageData = nullptr;
        std::string m_Type;
        std::string m_Path;
        glm::vec3   m_SolidColor{1.0f};

        Walnut::ImageFormat m_Format = Walnut::ImageFormat::RGBA;
    };
}  // namespace soft