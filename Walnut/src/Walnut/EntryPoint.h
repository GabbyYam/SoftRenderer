#pragma once
#include "Application.h"
#include "embree3/rtcore.h"
#include <embree3/rtcore_device.h>
#include <embree3/rtcore_scene.h>
#include <iostream>

#define WL_PLATFORM_WINDOWS
#ifdef WL_PLATFORM_WINDOWS

extern Walnut::Application* Walnut::CreateApplication(int argc, char** argv);
bool                        g_ApplicationRunning = true;

namespace Walnut {

    int Main(int argc, char** argv)
    {
        while (g_ApplicationRunning) {
            Walnut::Application* app = Walnut::CreateApplication(argc, argv);
            app->Run();
            delete app;
        }
        return 0;
    }

}  // namespace Walnut

#    ifdef WL_DIST

#        include <Windows.h>

int APIENTRY WinMain(HINSTANCE hInst, HINSTANCE hInstPrev, PSTR cmdline, int cmdshow)
{
    return Walnut::Main(__argc, __argv);
}

#    else

int main(int argc, char** argv)
{
    return Walnut::Main(argc, argv);
}

#    endif  // WL_DIST

#endif  // WL_PLATFORM_WINDOWS
