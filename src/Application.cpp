#include "Application.h"

Application::Application() : mWindow(nullptr) {}

bool Application::Initialize()
{
    // initialize glfw
    if (glfwInit() == GLFW_FALSE)
    {
        return false;
    }

    // create window
    mWindow = glfwCreateWindow(640, 480, "WebGPU Ocean", nullptr, nullptr);
    if (!mWindow)
    {
        glfwTerminate();
        return false;
    }

    glfwSwapInterval(1);
    return true;
}

void Application::RunLoop()
{
#ifdef __EMSCRIPTEN__
    auto callback = [](void* arg)
    {
        Application* pApp = reinterpret_cast<Application*>(arg);
        if (pApp->ShouldClose())
        {
            emscripten_cancel_main_loop();
            return;
        }
        pApp->Loop();
    };
    emscripten_set_main_loop_arg(callback, this, 0, true);
#else
    while (!ShouldClose())
    {
        Loop();
    }
#endif
}

void Application::Shutdown()
{
    glfwDestroyWindow(mWindow);
    glfwTerminate();
}

void Application::Loop()
{
    glfwSwapBuffers(mWindow);
    glfwPollEvents();
}

void Application::ProcessInput() {}

void Application::UpdateGame() {}

void Application::GenerateOutput() {}

bool Application::ShouldClose()
{
    return glfwWindowShouldClose(mWindow) == GLFW_TRUE;
}
