#include "Application.h"

#include <glfw3webgpu.h>
#include <iostream>

#include "WebGPUUtils.h"
#include "ResourceManager.h"

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

    InitializePipeline();

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

void Application::InitializePipeline()
{
    // Load the shader module
    wgpu::ShaderModule shaderModule =
        ResourceManager::LoadShaderModule("resources/shader/test.wgsl", mDevice);

    // Create the render pipeline
    wgpu::RenderPipelineDescriptor pipelineDesc {
        .vertex =
            {
                .bufferCount   = 0,
                .buffers       = nullptr,
                .module        = shaderModule,
                .entryPoint    = "vs_main",
                .constantCount = 0,
                .constants     = nullptr,
            },
        .primitive =
            {
                .topology         = wgpu::PrimitiveTopology::TriangleList,
                .stripIndexFormat = wgpu::IndexFormat::Undefined,  // When Undefined, sequentially
                .frontFace        = wgpu::FrontFace::CCW,
                .cullMode         = wgpu::CullMode::None,
            },
    };

    wgpu::FragmentState fragmentState {
        .module        = shaderModule,
        .entryPoint    = "fs_main",
        .constantCount = 0,
        .constants     = nullptr,
    };

    wgpu::BlendState blendState {
        .color =
            {
                .srcFactor = wgpu::BlendFactor::SrcAlpha,
                .dstFactor = wgpu::BlendFactor::OneMinusSrcAlpha,
                .operation = wgpu::BlendOperation::Add,
            },
        .alpha =
            {
                .srcFactor = wgpu::BlendFactor::Zero,
                .dstFactor = wgpu::BlendFactor::One,
                .operation = wgpu::BlendOperation::Add,
            },
    };

    wgpu::ColorTargetState colorTarget {
        .format    = mSurfaceFormat,
        .blend     = &blendState,
        .writeMask = wgpu::ColorWriteMask::All,
    };

    fragmentState.targetCount = 1;
    fragmentState.targets     = &colorTarget;

    pipelineDesc.fragment = &fragmentState;

    pipelineDesc.depthStencil = nullptr;

    // Samples per pixel
    pipelineDesc.multisample.count = 1;

    // Default value for the mask, meaning "all bits on"
    pipelineDesc.multisample.mask = ~0u;

    // Default value as well (irrelevant for count = 1 anyways)
    pipelineDesc.multisample.alphaToCoverageEnabled = false;
    pipelineDesc.layout                             = nullptr;

    mPipeline = mDevice.CreateRenderPipeline(&pipelineDesc);
}

void Application::Loop()
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
    wgpu::CommandEncoder encoder = mDevice.CreateCommandEncoder(&encoderDesc);

    // Create the render pass that clears the screen with our color
    wgpu::RenderPassDescriptor renderPassDesc {
        .nextInChain = nullptr,
        .label       = WebGPUUtils::GenerateString("Render Pass"),
    };

    // The attachment part of the render pass descriptor describes the target texture of the pass
    wgpu::RenderPassColorAttachment renderPassColorAttachment {
        .view          = targetView,
        .resolveTarget = nullptr,
        .loadOp        = wgpu::LoadOp::Clear,
        .storeOp       = wgpu::StoreOp::Store,
        .clearValue    = wgpu::Color {0.2, 1.0, 0.2, 1.0},
        .depthSlice    = WGPU_DEPTH_SLICE_UNDEFINED,
    };

    renderPassDesc.colorAttachmentCount   = 1;
    renderPassDesc.colorAttachments       = &renderPassColorAttachment;
    renderPassDesc.depthStencilAttachment = nullptr;
    renderPassDesc.timestampWrites        = nullptr;

    // Create the render pass and end it immediately (we only clear the screen but do not draw anything)
    wgpu::RenderPassEncoder renderPass = encoder.BeginRenderPass(&renderPassDesc);

    // Select which render pipeline to use
    renderPass.SetPipeline(mPipeline);
    // Draw 1 instance of a 3-vertices shape
    renderPass.Draw(3, 1, 0, 0);

    renderPass.End();

    // Finally encode and submit the render pass
    wgpu::CommandBufferDescriptor cmdBufferDescriptor {
        .nextInChain = nullptr,
        .label       = WebGPUUtils::GenerateString("Command buffer"),
    };
    wgpu::CommandBuffer command = encoder.Finish(&cmdBufferDescriptor);

    mQueue.Submit(1, &command);

#ifndef __EMSCRIPTEN__
    mSurface.Present();
    mDevice.Tick();
#endif
}

void Application::ProcessInput() {}

void Application::UpdateGame() {}

void Application::GenerateOutput() {}

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

bool Application::ShouldClose()
{
    return glfwWindowShouldClose(mWindow) == GLFW_TRUE;
}
