
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

#include "Rasterization/Rasterizer.h"
#include "Renderer.h"
#include "imgui.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION

#include <stb_image.h>
#include <stb_image_write.h>

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
            // ImGui::Text("Camera Position: %.3f %.3f %.3f", m_Camera->GetPosition().x, m_Camera->GetPosition().y,
            //             m_Camera->GetPosition().z);
            ImGui::End();
        }

        {
            m_ConfigChange = false;
            ImGui::Begin("RT Setting");
            m_ConfigChange |= ImGui::Checkbox("Apply Ray Tracing", &m_ApplyRayTracing);

            auto& maxBounce = m_ActiveRenderer->rayTraceSetting.bounce;
            m_ConfigChange |= ImGui::InputInt("Ray Bounce", &maxBounce);
            maxBounce = std::clamp(maxBounce, 0, 32);

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

    void SaveImage()
    {
        // Rotate Image
        auto imageData = m_ActiveRenderer->GetImageData();

        auto              now   = std::chrono::system_clock::now();
        std::time_t       now_c = std::chrono::system_clock::to_time_t(now);
        std::stringstream ss;
        ss << std::put_time(std::localtime(&now_c), "%Y-%m-%d %H'%M'%S");
        std::string filename = soft::FileDialogs::SaveFile("(Image File) *.png\0");
        stbi_write_png(filename.data(), m_ViewportWidth, m_ViewportHeight, 4, imageData, m_ViewportWidth * 4);
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