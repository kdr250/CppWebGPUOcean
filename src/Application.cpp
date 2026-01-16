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
    while (glfwWindowShouldClose(mWindow) == GLFW_FALSE)
    {
        glfwSwapBuffers(mWindow);
        glfwPollEvents();
    }
}

void Application::Shutdown()
{
    glfwDestroyWindow(mWindow);
    glfwTerminate();
}

void Application::ProcessInput() {}

void Application::UpdateGame() {}

void Application::GenerateOutput() {}
