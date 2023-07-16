
#include "FileDialogs.hpp"
#include "Geometry/Model.hpp"
#include "Geometry/Scene.hpp"
#include "RayTracing/RTCTracer.hpp"
#include "Walnut/Application.h"
#include "Walnut/EntryPoint.h"
#include "Walnut/Image.h"
#include "Walnut/Timer.h"

#include <algorithm>
#include <memory>
#include <spdlog/spdlog.h>
#include <stdint.h>
#include <type_traits>
#include <vcruntime_string.h>

#include "Rasterization/Rasterizer.h"
#include "Renderer.h"
#include "imgui.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION

#include <stb_image.h>
#include <stb_image_write.h>
#include <OpenImageDenoise/oidn.hpp>

static const std::string s_MeshFilename    = "H:/GameDev Asset/Models/Vroid_JK_Fix.fbx";
static const std::string s_ObjMeshFilename = "E:/Data/room/face.obj";
static const std::string s_Bunny           = "H:/Meshes/Bunny.obj";
static const std::string s_EnvMap          = "H:/GameDev Asset/Textures/EnvironmentMap/newport_loft.hdr";

class SoftEditorLayer : public Walnut::Layer {
public:
    SoftEditorLayer()
    {
        spdlog::set_level(spdlog::level::debug);
        std::shared_ptr<soft::Model> model = std::make_shared<soft::Model>(s_MeshFilename);
        std::shared_ptr<soft::Scene> scene = std::make_shared<soft::Scene>();
        scene->models.push_back(model);

        m_Rasterizer     = std::make_shared<soft::Rasterizer>(scene);
        m_RayTracer      = std::make_shared<soft::RTCTracer>(scene);
        m_ActiveRenderer = m_ApplyRayTracing ? m_RayTracer : m_Rasterizer;

        m_Rasterizer->lightSetting.environmentMapPath = s_EnvMap;
        m_RayTracer->lightSetting.environmentMapPath  = s_EnvMap;

        m_RayTracer->ReloadSetting();
        m_Rasterizer->ReloadSetting();

        m_Camera = std::make_shared<soft::Camera>(45.0f, 0.01f, 10000.f);
    }

    virtual void OnUpdate(float ts) override
    {
        m_ActiveRenderer = m_ApplyRayTracing ? m_RayTracer : m_Rasterizer;

        if (m_Running)
            m_ConfigChange |= m_Camera->OnUpdate(ts);

        if (m_ConfigChange)
            m_RayTracer->ResetFrameIndex();
    }

    virtual void OnUIRender() override
    {
        m_ConfigChange = false;
        {
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2{0.0f, 0.0f});
            ImGui::Begin("Viewport");
            m_ViewportWidth  = ImGui::GetContentRegionAvail().x;
            m_ViewportHeight = ImGui::GetContentRegionAvail().y;

            auto image = m_ActiveRenderer->GetImage();
            if (image) {
                ImGui::Image(image->GetDescriptorSet(), {(float)image->GetWidth(), (float)image->GetHeight()}, ImVec2(0, 1),
                             ImVec2(1, 0));
            }
            ImGui::End();
            ImGui::PopStyleVar();
        }

        {
            ImGui::Begin("General Setting");

            if (ImGui::Button("Save")) {
                SaveImage();
                info("Save current frame successful!");
            }

            ImGui::Checkbox("Show demo window", &m_ShowDemoWindow);
            ImGui::Checkbox("Run", &m_Running);

            if (ImGui::BeginCombo("Shade Mode", _shadeModeTags[(int)m_ShadeMode].data())) {
                for (int i = 0; i < _shadeModeTags.size(); i++) {
                    const bool is_selected = ((int)m_ShadeMode == i);
                    if (ImGui::Selectable(_shadeModeTags[i].data(), is_selected)) {
                        m_ShadeMode = (soft::ShadeMode)i;
                    }

                    // Set the initial focus when opening the combo (scrolling + keyboard navigation focus)
                    if (is_selected)
                        ImGui::SetItemDefaultFocus();
                }
                ImGui::EndCombo();
            }

            static int cullingModeIndex = 0;
            if (ImGui::BeginCombo("Culling Mode", _cullingModeTags[cullingModeIndex].data())) {
                for (int i = 0; i < _cullingModeTags.size(); i++) {
                    const bool is_selected = (cullingModeIndex == i);
                    if (ImGui::Selectable(_cullingModeTags[i].data(), is_selected)) {
                        cullingModeIndex = i;
                        m_Culling        = (soft::CullingMode)i;
                    }

                    // Set the initial focus when opening the combo (scrolling + keyboard navigation focus)
                    if (is_selected)
                        ImGui::SetItemDefaultFocus();
                }
                ImGui::EndCombo();
            }

            if (m_Running) {
                // if (ImGui::Button("Render"))
                Render();
            }

            ImGui::Text("Frame rate: %.3f fps", 1000.f / m_LastTime);
            ImGui::Text("Frame cost: %.3f ms", m_LastTime);
            ImGui::Text("Frame count: %d", m_ActiveRenderer->TotalFrameCount());
            // ImGui::Text("Camera Position: %.3f %.3f %.3f", m_Camera->GetPosition().x, m_Camera->GetPosition().y,
            //             m_Camera->GetPosition().z);
            ImGui::End();
        }

        {
            ImGui::Begin("RT Setting");
            m_ConfigChange |= ImGui::Checkbox("Apply Ray Tracing", &m_ApplyRayTracing);

            // auto& maxBounce = m_ActiveRenderer->rayTraceSetting.bounce;
            // m_ConfigChange |= ImGui::InputInt("Ray Bounce", &maxBounce);
            // maxBounce = std::clamp(maxBounce, 0, 32);
            m_ConfigChange |= ImGui::SliderFloat("End Prob", &m_ActiveRenderer->rayTraceSetting.prob, 0.0, 1.0);
            m_ConfigChange |= ImGui::Checkbox("Sample From Light", &m_ActiveRenderer->rayTraceSetting.sampleFromLight);

            ImGui::End();
        }

        {
            ImGui::Begin("Light Setting");
            m_ConfigChange |= (ImGui::Checkbox("Environment Map", &m_ActiveRenderer->lightSetting.useEnvironmentMap));
            m_ConfigChange |= ImGui::SliderFloat("Light Intensity", &m_ActiveRenderer->lightSetting.lightIntensity, 0.0f, 8.0f);
            m_ConfigChange |= ImGui::InputFloat3("Light Direction", &m_ActiveRenderer->lightSetting.directionalLight[0]);
            m_ConfigChange |= ImGui::ColorEdit3("Light Color", &m_ActiveRenderer->lightSetting.lightColor[0]);
            ImGui::End();
        }

        {
            ImGui::Begin("Post Processing");
            {
                ImGui::Text("Setting");
                ImGui::Checkbox("Enable Multi-Threading", &m_ActiveRenderer->postProcessingSetting.MT);

                ImGui::Text("Anti-Aliasing");
                ImGui::Checkbox("FXAA", &m_ActiveRenderer->postProcessingSetting.FXAA);

                ImGui::Text("Tone Mapping");
                ImGui::Checkbox("ACES", &m_ActiveRenderer->postProcessingSetting.ToneMapping);
                ImGui::Checkbox("Gamma", &m_ActiveRenderer->postProcessingSetting.Gamma);
            }
            ImGui::End();
        }

        {
            if (m_ShowDemoWindow)
                ImGui::ShowDemoWindow(&m_ShowDemoWindow);
        }
    }

    void Render()
    {
        Walnut::Timer timer;
        m_Camera->OnResize(m_ViewportWidth, m_ViewportHeight);
        if (!m_ApplyRayTracing) {
            m_Rasterizer->SetShaderMode(m_ShadeMode);
            m_Rasterizer->SetCullingMode(m_Culling);
        }
        m_ActiveRenderer->OnResize(m_ViewportWidth, m_ViewportHeight);
        m_ActiveRenderer->Render(m_Camera);
        m_ActiveRenderer->OnPostProcess();

        m_LastTime = timer.ElapsedMillis();
    }

    void WriteFloatImage(const char* filename, const vec3* data, int width, int height)
    {
        // 创建一个足够容纳图片数据的缓冲区
        unsigned char* imageBuffer = new unsigned char[width * height * 3];

        // 将vec4数据转换为8位整数数据
        const vec3*    pixelData = data;
        unsigned char* imagePtr  = imageBuffer;
        uint32_t       channels  = 3;

        for (int i = 0; i < width * height; ++i) {
            // 提取vec4中的RGB分量，并将其映射到[0, 255]范围
            unsigned char r = static_cast<unsigned char>(pixelData->r * 255.0f);
            unsigned char g = static_cast<unsigned char>(pixelData->g * 255.0f);
            unsigned char b = static_cast<unsigned char>(pixelData->b * 255.0f);

            // 将像素数据写入缓冲区
            *imagePtr++ = r;
            *imagePtr++ = g;
            *imagePtr++ = b;

            // 移动到下一个像素
            ++pixelData;
        }

        // 创建一个新的缓冲区来存储翻转后的图像数据
        unsigned char* flipped_data = new unsigned char[width * height * channels];

        // 上下翻转图像
        for (int y = 0; y < height; y++) {
            int flipped_y = height - 1 - y;
            memcpy(flipped_data + flipped_y * width * channels, imageBuffer + y * width * channels, width * channels);
        }

        // 写入PNG图像文件
        stbi_write_png(filename, width, height, channels, flipped_data, 0);

        // 释放缓冲区内存
        delete[] imageBuffer;
        delete[] flipped_data;
    }

    void WriteFloatImage(const char* filename, const vec4* data, int width, int height)
    {
        // 创建一个足够容纳图片数据的缓冲区
        unsigned char* imageBuffer = new unsigned char[width * height * 4];

        // 将vec4数据转换为8位整数数据
        const vec4*    pixelData = data;
        unsigned char* imagePtr  = imageBuffer;
        uint32_t       channels  = 4;
        for (int i = 0; i < width * height; ++i) {
            // 提取vec4中的RGB分量，并将其映射到[0, 255]范围
            unsigned char r = static_cast<unsigned char>(pixelData->r * 255.0f);
            unsigned char g = static_cast<unsigned char>(pixelData->g * 255.0f);
            unsigned char b = static_cast<unsigned char>(pixelData->b * 255.0f);
            unsigned char a = static_cast<unsigned char>(pixelData->a * 255.0f);

            // 将像素数据写入缓冲区
            *imagePtr++ = r;
            *imagePtr++ = g;
            *imagePtr++ = b;
            *imagePtr++ = a;
            // 移动到下一个像素
            ++pixelData;
        }

        // 创建一个新的缓冲区来存储翻转后的图像数据
        unsigned char* flipped_data = new unsigned char[width * height * channels];

        // 上下翻转图像
        for (int y = 0; y < height; y++) {
            int flipped_y = height - 1 - y;
            memcpy(flipped_data + flipped_y * width * channels, imageBuffer + y * width * channels, width * channels);
        }

        // 写入PNG图像文件
        stbi_write_png(filename, width, height, channels, flipped_data, 0);

        // 释放缓冲区内存
        delete[] imageBuffer;
        delete[] flipped_data;
    }

    void convertFloat4ToFloat3(const float* inputFloat4, float* outputFloat3, int width, int height)
    {
        for (int i = 0; i < width * height; ++i) {
            outputFloat3[i * 3 + 0] = inputFloat4[i * 4 + 0];  // Red
            outputFloat3[i * 3 + 1] = inputFloat4[i * 4 + 1];  // Green
            outputFloat3[i * 3 + 2] = inputFloat4[i * 4 + 2];  // Blue
        }
    }

    void SaveImage()
    {
        // Rotate Image
        float* imageData = (float*)m_ActiveRenderer->GetImageData();

        auto              now   = std::chrono::system_clock::now();
        std::time_t       now_c = std::chrono::system_clock::to_time_t(now);
        std::stringstream ss;
        ss << std::put_time(std::localtime(&now_c), "%Y-%m-%d %H'%M'%S");
        std::string filename = soft::FileDialogs::SaveFile("(Image File) *.png\0");
        if (auto it = filename.find(".png"))
            filename = filename.substr(0, it);

        WriteFloatImage((filename + ".png").data(), reinterpret_cast<vec4*>(imageData), m_ViewportWidth, m_ViewportHeight);

        {
            // Create an Open Image Denoise device
            oidn::DeviceRef device = oidn::newDevice();  // CPU or GPU if available
            device.commit();

            // Create buffers for input/output images accessible by both host (CPU) and device (CPU/GPU)
            // oidn::BufferRef colorBuf = device.newBuffer(m_ViewportWidth * m_ViewportHeight * 4 * sizeof(float));

            // 创建输出图像缓冲区
            float* outputImage = new float[m_ViewportWidth * m_ViewportHeight * 3];
            convertFloat4ToFloat3(imageData, outputImage, m_ViewportWidth, m_ViewportHeight);

            oidn::FilterRef filter = device.newFilter("RT");
            filter.setImage("color", outputImage, oidn::Format::Float3, m_ViewportWidth, m_ViewportHeight);
            filter.setImage("output", outputImage, oidn::Format::Float3, m_ViewportWidth, m_ViewportHeight);
            filter.set("hdr", true);  // 设置为true表示输入图像为HDR格式
            filter.commit();

            filter.execute();

            // // Check for errors
            const char* errorMessage;
            if (device.getError(errorMessage) != oidn::Error::None) {
                std::cout << "Error: " << errorMessage << std::endl;
            }
            else {
                WriteFloatImage((filename + "_denoised.png").data(), reinterpret_cast<vec3*>(outputImage), m_ViewportWidth,
                                m_ViewportHeight);
            }
            // 释放内存
            if (outputImage)
                delete[] outputImage;
        }
    }

private:
    uint32_t                          m_ViewportWidth, m_ViewportHeight;
    float                             m_LastTime = 1.0f;
    std::shared_ptr<soft::Renderer>   m_ActiveRenderer;
    std::shared_ptr<soft::Rasterizer> m_Rasterizer;
    std::shared_ptr<soft::Renderer>   m_RayTracer;
    std::shared_ptr<soft::Camera>     m_Camera;

    soft::ShadeMode   m_ShadeMode       = soft::ShadeMode::Shaded;
    soft::CullingMode m_Culling         = soft::CullingMode::Back;
    bool              m_Running         = true;
    bool              m_ShowDemoWindow  = false;
    bool              m_ApplyRayTracing = true;

    bool m_ConfigChange = false;

    glm::vec3 m_DirectionalLight = {0.f, 0.f, -1.f};

private:
    std::vector<std::string> _shadeModeTags   = {"Shaded", "Wire Frame"};
    std::vector<std::string> _cullingModeTags = {"Back", "Front", "None"};
};

Walnut::Application* Walnut::CreateApplication(int argc, char** argv)
{
    Walnut::ApplicationSpecification spec;
    spec.Name   = "Soft Renderer";
    spec.Width  = 640 * 2.0;
    spec.Height = 480 * 1.5;

    Walnut::Application* app = new Walnut::Application(spec);
    app->PushLayer<SoftEditorLayer>();
    app->SetMenubarCallback([app]() {
        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem("Exit")) {
                app->Close();
            }
            ImGui::EndMenu();
        }
    });
    return app;
}