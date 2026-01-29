#include "Application.h"

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_wgpu.h>

#include <glfw3webgpu.h>
#include <iostream>
#include <fstream>

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
    glm::vec2 windowSize(1024.0f, 768.0f);
    mWindow =
        glfwCreateWindow((int)windowSize.x, (int)windowSize.y, "WebGPU Ocean", nullptr, nullptr);
    if (!mWindow)
    {
        glfwTerminate();
        return false;
    }

    // Add window callbacks
    glfwSetWindowUserPointer(mWindow, this);
    glfwSetCursorPosCallback(
        mWindow,
        [](GLFWwindow* window, double xpos, double ypos)
        {
            auto that = reinterpret_cast<Application*>(glfwGetWindowUserPointer(window));
            if (that != nullptr)
                that->OnMouseMove(xpos, ypos);
        });
    glfwSetMouseButtonCallback(
        mWindow,
        [](GLFWwindow* window, int button, int action, int mods)
        {
            auto that = reinterpret_cast<Application*>(glfwGetWindowUserPointer(window));
            if (that != nullptr)
                that->OnMouseButton(button, action, mods);
        });
    glfwSetScrollCallback(mWindow,
                          [](GLFWwindow* window, double xoffset, double yoffset)
                          {
                              auto that =
                                  reinterpret_cast<Application*>(glfwGetWindowUserPointer(window));
                              if (that != nullptr)
                                  that->OnScroll(xoffset, yoffset);
                          });
    glfwSetKeyCallback(mWindow,
                       [](GLFWwindow* window, int key, int scancode, int action, int mods)
                       {
                           auto that =
                               reinterpret_cast<Application*>(glfwGetWindowUserPointer(window));
                           if (that != nullptr)
                               that->OnKeyAction(key, scancode, action, mods);
                       });

    // create instance
    mInstance = wgpu::CreateInstance(nullptr);

    // create surface
    mSurface = glfwCreateWindowWGPUSurface(mInstance.Get(), mWindow);

    // get adaptor
    std::cout << "Requesting adapter..." << std::endl;
    wgpu::RequestAdapterOptions adapterOptions {
        .nextInChain       = nullptr,
        .compatibleSurface = mSurface,
    };
    wgpu::Adapter adapter = WebGPUUtils::RequestAdapterSync(mInstance, &adapterOptions);
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
        .width  = (uint32_t)windowSize.x,
        .height = (uint32_t)windowSize.y,
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

    mRenderUniforms.screenSize = windowSize;
    mRenderUniforms.texelSize  = glm::vec2(1.0f / windowSize.x, 1.0 / windowSize.y);

    // Setup variables
    float fov          = 45.0f * glm::pi<float>() / 180.0f;
    float initDistance = 3.0f;
    glm::vec3 target(0.0f, -1.9f, 0.0f);
    float zoomRate = 0.05f;
    float radius   = 0.04f;
    float diameter = 2.0f * radius;

    mSPHSimulator =
        std::make_unique<SPHSimulator>(mDevice, mParticleBuffer, mPosvelBuffer, diameter);

    mFluidRenderer = std::make_unique<FluidRenderer>(mDevice,
                                                     mRenderUniforms.screenSize,
                                                     mSurfaceFormat,
                                                     radius,
                                                     fov,
                                                     mRenderUniformBuffer,
                                                     mPosvelBuffer);

    mCamera = std::make_unique<Camera>();

    mSPHSimulator->Reset(mSimulationVariables.numParticles,
                         mSimulationVariables.initialBoxSize,
                         mRenderUniforms);
    mCamera->Reset(mRenderUniforms, initDistance, target, fov, zoomRate);

    mQueue.WriteBuffer(mRenderUniformBuffer, 0, &mRenderUniforms, sizeof(RenderUniforms));

    InitializeGUI();

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
    TerminateGUI();

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
    bufferDesc.size             = sizeof(SPHParticle) * NUM_PARTICLES_MAX;
    bufferDesc.usage            = wgpu::BufferUsage::CopyDst | wgpu::BufferUsage::Storage;
    bufferDesc.mappedAtCreation = false;

    mParticleBuffer = mDevice.CreateBuffer(&bufferDesc);

    // position storage buffer
    bufferDesc.label            = WebGPUUtils::GenerateString("position storage buffer");
    bufferDesc.size             = sizeof(PosVel) * NUM_PARTICLES_MAX;
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
    if (mSimulationVariables.changed)
    {
        mSPHSimulator->Reset(mSimulationVariables.numParticles,
                             mSimulationVariables.initialBoxSize,
                             mRenderUniforms);
    }

    if (mSimulationVariables.boxSizeChanged)
    {
        glm::vec3 realBoxSize = mSimulationVariables.initialBoxSize;
        realBoxSize.z *= mSimulationVariables.boxWidthRatio;
        mSPHSimulator->ChangeBoxSize(realBoxSize);
    }
}

void Application::GenerateOutput()
{
    glfwPollEvents();

    mQueue.WriteBuffer(mRenderUniformBuffer, 0, &mRenderUniforms, sizeof(RenderUniforms));

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

    mSPHSimulator->Compute(commandEncoder);
    mFluidRenderer->Draw(commandEncoder, targetView, mSimulationVariables);

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

void Application::OnMouseMove(double xpos, double ypos)
{
    if (!mCamera->isDragging)
        return;

    float deltaX = mCamera->prevX - xpos;
    float deltaY = mCamera->prevY - ypos;
    mCamera->currentXTheta += mCamera->sensitivity * deltaX;
    mCamera->currentYTheta += mCamera->sensitivity * deltaY;
    if (mCamera->currentYTheta > mCamera->maxYTheta)
        mCamera->currentYTheta = mCamera->maxYTheta;
    if (mCamera->currentYTheta < mCamera->minYTheta)
        mCamera->currentYTheta = mCamera->minYTheta;
    mCamera->prevX = xpos;
    mCamera->prevY = ypos;
    mCamera->RecalculateView(mRenderUniforms);
}

void Application::OnMouseButton(int button, int action, int mods)
{
    if (button == GLFW_MOUSE_BUTTON_LEFT)
    {
        switch (action)
        {
            case GLFW_PRESS:
            {
                mCamera->isDragging = true;
                double xpos, ypos;
                glfwGetCursorPos(mWindow, &xpos, &ypos);
                mCamera->prevX = xpos;
                mCamera->prevY = ypos;
                break;
            }

            case GLFW_RELEASE:
            {
                mCamera->isDragging = false;
                break;
            }

            default:
                break;
        }
    }
}

void Application::OnScroll(double xoffset, double yoffset)
{
    mCamera->currentDistance += ((yoffset > 0.0) ? 1.0 : -1.0) * mCamera->zoomRate;
    if (mCamera->currentDistance < mCamera->minDistance)
        mCamera->currentDistance = mCamera->minDistance;
    if (mCamera->currentDistance > mCamera->maxDistance)
        mCamera->currentDistance = mCamera->maxDistance;

    mCamera->RecalculateView(mRenderUniforms);
}

void Application::OnKeyAction(int key, int scancode, int action, int mods)
{
    if (action == GLFW_PRESS && key == GLFW_KEY_ESCAPE)
    {
        glfwSetWindowShouldClose(mWindow, true);
        return;
    }

    if ((action == GLFW_PRESS || action == GLFW_REPEAT) && (key == GLFW_KEY_W || key == GLFW_KEY_S))
    {
        mCamera->currentDistance += (key == GLFW_KEY_W ? 1.0f : -1.0f) * mCamera->zoomRate;
        if (mCamera->currentDistance < mCamera->minDistance)
            mCamera->currentDistance = mCamera->minDistance;
        if (mCamera->currentDistance > mCamera->maxDistance)
            mCamera->currentDistance = mCamera->maxDistance;
        mCamera->RecalculateView(mRenderUniforms);
    }
}

void Application::InitializeGUI()
{
    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::GetIO();

    // Get the surface texture
    wgpu::SurfaceTexture surfaceTexture;
    mSurface.GetCurrentTexture(&surfaceTexture);
    wgpu::TextureFormat format = surfaceTexture.texture.GetFormat();

    // Setup platform/Renderer backends
    ImGui_ImplGlfw_InitForOther(mWindow, true);
    ImGui_ImplWGPU_InitInfo initInfo;
    initInfo.Device             = mDevice.Get();
    initInfo.DepthStencilFormat = WGPUTextureFormat::WGPUTextureFormat_Depth32Float;
    initInfo.RenderTargetFormat = (WGPUTextureFormat)format;
    initInfo.NumFramesInFlight  = 3;
    ImGui_ImplWGPU_Init(&initInfo);
}

void Application::TerminateGUI()
{
    ImGui_ImplWGPU_Shutdown();
    ImGui_ImplGlfw_Shutdown();
}

bool Application::ShouldClose()
{
    return glfwWindowShouldClose(mWindow) == GLFW_TRUE;
}
