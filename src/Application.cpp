#include "Application.h"

#include <glfw3webgpu.h>
#include <iostream>

#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_FORCE_LEFT_HANDED
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/ext.hpp>
#include <glm/glm.hpp>

#include "WebGPUUtils.h"
#include "ResourceManager.h"
#include "sph/SPH.h"

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
    wgpu::Limits requiredLimits         = GetRequiredLimits(adapter);
    deviceDesc.requiredLimits           = &requiredLimits;
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

    mSurfaceFormat = WebGPUUtils::GetTextureFormat(mSurface, adapter);

    // Configure the surface
    wgpu::SurfaceConfiguration config {
        .nextInChain = nullptr,

        // Configuration of the textures created for the underlying swap chain
        .width  = 640,
        .height = 480,
        .usage  = wgpu::TextureUsage::RenderAttachment,
        .format = mSurfaceFormat,

        // And we do not need any particular view format:
        .viewFormatCount = 0,
        .viewFormats     = nullptr,
        .device          = mDevice,
        .presentMode     = wgpu::PresentMode::Fifo,
        .alphaMode       = wgpu::CompositeAlphaMode::Auto,
    };

    mSurface.Configure(&config);

    InitializeBuffers();

    mRenderUniforms.screenSize = glm::vec2(640, 480);

    // FIXME
    mCamera = std::make_unique<Camera>();
    mCamera->Reset(mRenderUniforms,
                   mRenderUniforms.screenSize,
                   10.0f,
                   glm::vec3(0.0f, 0.0f, 1.0f),
                   45.0f * glm::pi<float>() / 180.0f,
                   1.5f);

    mQueue.WriteBuffer(mRenderUniformBuffer, 0, &mRenderUniforms, sizeof(RenderUniforms));

    mFluidRenderer = std::make_unique<FluidRenderer>(mDevice, mSurfaceFormat, mRenderUniformBuffer);

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

void Application::InitializeBuffers()
{
    // render uniform buffer
    wgpu::BufferDescriptor bufferDesc {};
    bufferDesc.label            = WebGPUUtils::GenerateString("render unioform buffer");
    bufferDesc.size             = sizeof(RenderUniforms);
    bufferDesc.usage            = wgpu::BufferUsage::CopyDst | wgpu::BufferUsage::Uniform;
    bufferDesc.mappedAtCreation = false;

    mRenderUniformBuffer = mDevice.CreateBuffer(&bufferDesc);

    // particle storage buffer
    bufferDesc.label            = WebGPUUtils::GenerateString("particle storage buffer");
    bufferDesc.size             = SPH_PARTICLE_STRUCTURE_SIZE * NUM_PARTICLES_MAX;
    bufferDesc.usage            = wgpu::BufferUsage::CopyDst | wgpu::BufferUsage::Storage;
    bufferDesc.mappedAtCreation = false;

    mParticleBuffer = mDevice.CreateBuffer(&bufferDesc);

    // position storage buffer
    bufferDesc.label            = WebGPUUtils::GenerateString("position storage buffer");
    bufferDesc.size             = 32 * NUM_PARTICLES_MAX;  // 32 = 2 * vec3f + padding
    bufferDesc.usage            = wgpu::BufferUsage::CopyDst | wgpu::BufferUsage::Storage;
    bufferDesc.mappedAtCreation = false;

    mPosvelBuffer = mDevice.CreateBuffer(&bufferDesc);
}

void Application::Loop()
{
    ProcessInput();
    UpdateGame();
    GenerateOutput();
}

void Application::ProcessInput()
{
    // TODO
}

void Application::UpdateGame()
{
    // TODO
}

void Application::GenerateOutput()
{
    glfwPollEvents();

    // Get the next target texture view
    wgpu::TextureView targetView = GetNextSurfaceTextureView();
    if (!targetView)
    {
        return;
    }

    // Create a command encoder for the draw call
    wgpu::CommandEncoderDescriptor encoderDesc {
        .nextInChain = nullptr,
        .label       = WebGPUUtils::GenerateString("My command encoder"),
    };
    wgpu::CommandEncoder commandEncoder = mDevice.CreateCommandEncoder(&encoderDesc);

    mFluidRenderer->Draw(commandEncoder, targetView);

    // Finally encode and submit the render pass
    wgpu::CommandBufferDescriptor cmdBufferDescriptor {
        .nextInChain = nullptr,
        .label       = WebGPUUtils::GenerateString("Command buffer"),
    };
    wgpu::CommandBuffer command = commandEncoder.Finish(&cmdBufferDescriptor);

    mQueue.Submit(1, &command);

#ifndef __EMSCRIPTEN__
    mSurface.Present();
    mDevice.Tick();
#endif
}

wgpu::TextureView Application::GetNextSurfaceTextureView()
{
    // Get the surface texture
    wgpu::SurfaceTexture surfaceTexture;
    mSurface.GetCurrentTexture(&surfaceTexture);
    if (surfaceTexture.status != wgpu::SurfaceGetCurrentTextureStatus::SuccessOptimal
        && surfaceTexture.status != wgpu::SurfaceGetCurrentTextureStatus::SuccessSuboptimal)
    {
        return nullptr;
    }

    // Create a view for this surface texture
    wgpu::TextureViewDescriptor viewDescriptor {
        .nextInChain     = nullptr,
        .label           = WebGPUUtils::GenerateString("Surface texture view"),
        .usage           = wgpu::TextureUsage::RenderAttachment,
        .format          = surfaceTexture.texture.GetFormat(),
        .dimension       = wgpu::TextureViewDimension::e2D,
        .baseMipLevel    = 0,
        .mipLevelCount   = 1,
        .baseArrayLayer  = 0,
        .arrayLayerCount = 1,
        .aspect          = wgpu::TextureAspect::All,
    };

    wgpu::TextureView targetView = surfaceTexture.texture.CreateView(&viewDescriptor);

    return targetView;
}

wgpu::Limits Application::GetRequiredLimits(wgpu::Adapter adapter) const
{
    // Get adapter supported limits, in case we need them
    wgpu::Limits supportedLimits;
    supportedLimits.nextInChain = nullptr;
    adapter.GetLimits(&supportedLimits);

    return supportedLimits;

    // wgpu::Limits requiredLimits {};
    // WebGPUUtils::SetDefaultLimits(requiredLimits);

    // requiredLimits.maxStorageBufferBindingSize = supportedLimits.maxStorageBufferBindingSize;

    // requiredLimits.minUniformBufferOffsetAlignment =
    //     supportedLimits.minUniformBufferOffsetAlignment;
    // requiredLimits.minStorageBufferOffsetAlignment =
    //     supportedLimits.minStorageBufferOffsetAlignment;

    // return requiredLimits;
}

bool Application::ShouldClose()
{
    return glfwWindowShouldClose(mWindow) == GLFW_TRUE;
}
