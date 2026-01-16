#include "Application.h"

#include <glfw3webgpu.h>
#include <iostream>

#include "WebGPUUtils.h"

Application::Application() : mWindow(nullptr) {}

bool Application::Initialize()
{
    // initialize glfw
    if (glfwInit() == GLFW_FALSE)
    {
        return false;
    }

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

    // create window
    mWindow = glfwCreateWindow(640, 480, "WebGPU Ocean", nullptr, nullptr);
    if (!mWindow)
    {
        glfwTerminate();
        return false;
    }

    // create instance
    wgpu::Instance instance = wgpu::CreateInstance(nullptr);

    // create surface
    mSurface = glfwCreateWindowWGPUSurface(instance.Get(), mWindow);

    // get adaptor
    std::cout << "Requesting adapter..." << std::endl;
    wgpu::RequestAdapterOptions adapterOptions {
        .nextInChain       = nullptr,
        .compatibleSurface = mSurface,
    };
    wgpu::Adapter adapter = WebGPUUtils::RequestAdapterSync(instance, &adapterOptions);
    std::cout << "Got adapter: " << adapter.Get() << std::endl;

    WebGPUUtils::InspectAdapter(adapter);

    // get device
    std::cout << "Requesting device..." << std::endl;
    wgpu::DeviceDescriptor deviceDesc   = {};
    deviceDesc.nextInChain              = nullptr;
    deviceDesc.label                    = WebGPUUtils::GenerateString("My Device");
    deviceDesc.requiredFeatureCount     = 0;
    deviceDesc.requiredLimits           = nullptr;  // TODO
    deviceDesc.defaultQueue.nextInChain = nullptr;
    deviceDesc.defaultQueue.label       = WebGPUUtils::GenerateString("The default queue");

    auto deviceLostCallback =
        [](const wgpu::Device&, wgpu::DeviceLostReason reason, wgpu::StringView message)
    {
        printf("Device lost: reason 0x%08X\n", reason);
        if (message.data)
        {
            printf(" - message: %s\n", message.data);
        }
    };

    deviceDesc.SetDeviceLostCallback(wgpu::CallbackMode::AllowSpontaneous, deviceLostCallback);

    auto uncapturedErrorCallback =
        [](const wgpu::Device&, wgpu::ErrorType type, wgpu::StringView message)
    {
        printf("Uncaptured device error: type 0x%08X\n", type);
        if (message.data)
        {
            printf(" - message: %s\n", message.data);
        }
    };

    deviceDesc.SetUncapturedErrorCallback(uncapturedErrorCallback);

    mDevice = WebGPUUtils::RequestDeviceSync(adapter, &deviceDesc);

    WebGPUUtils::InspectDevice(mDevice);

    mQueue = mDevice.GetQueue();
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
