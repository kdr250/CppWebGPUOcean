#include "Application.h"

#include <imgui.h>
#include <imgui_impl_sdl3.h>
#include <imgui_impl_wgpu.h>

#include <sdl3webgpu.h>
#include <iostream>
#include <fstream>
#include <random>

#include "WebGPUUtils.h"
#include "ResourceManager.h"

Application::Application() : mWindow(nullptr) {}

bool Application::Initialize()
{
    // initialize
    if (!SDL_Init(SDL_INIT_VIDEO))
    {
        return false;
    }

    // create window
    glm::vec2 windowSize(1024.0f, 768.0f);
    mWindow = SDL_CreateWindow("WebGPU Ocean", (int)windowSize.x, (int)windowSize.y, 0);
    if (!mWindow)
    {
        SDL_Quit();
        return false;
    }
    SDL_SetWindowResizable(mWindow, false);

    // create instance
    mInstance = wgpu::CreateInstance(nullptr);

    // create surface
    mSurface = SDL_GetWGPUSurface(mInstance, mWindow);

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
        .nextInChain     = nullptr,
        .device          = mDevice,
        .format          = mSurfaceFormat,
        .usage           = wgpu::TextureUsage::RenderAttachment,
        .width           = (uint32_t)windowSize.x,
        .height          = (uint32_t)windowSize.y,
        .viewFormatCount = 0,
        .viewFormats     = nullptr,
        .alphaMode       = wgpu::CompositeAlphaMode::Auto,
        .presentMode     = wgpu::PresentMode::Fifo,
    };

    mSurface.Configure(&config);

    InitializeBuffers();

    mRenderUniforms.screenSize = windowSize;
    mRenderUniforms.texelSize  = glm::vec2(1.0f / windowSize.x, 1.0 / windowSize.y);

    // Setup SPH
    {
        float fov          = mSimulationVariables.fov;
        float initDistance = mSimulationVariables.sphInitDistances[1];
        glm::vec3 boxSize  = mSimulationVariables.sphBoxSizes[1];
        glm::vec3 target(0.0f, -boxSize[1] + 0.1, 0.0f);
        float zoomRate = SimulationVariables::SPH_ZOOM_RATE;
        float radius   = 0.04f;
        float diameter = 2.0f * radius;

        mSPHSimulator =
            std::make_unique<SPHSimulator>(mDevice, mParticleBuffer, mPosvelBuffer, diameter);

        mSPHRenderer = std::make_unique<FluidRenderer>(mDevice,
                                                       mRenderUniforms.screenSize,
                                                       mSurfaceFormat,
                                                       radius,
                                                       fov,
                                                       mRenderUniformBuffer,
                                                       mPosvelBuffer);

        mSPHSimulator->Reset(mSimulationVariables.numParticles,
                             mSimulationVariables.boxSize,
                             mRenderUniforms);

        mCamera = std::make_unique<Camera>();
        mCamera->Reset(mRenderUniforms, initDistance, target, fov, zoomRate);
    }

    // Setup MLS-MPM
    {
        float fov          = mSimulationVariables.fov;
        float initDistance = mSimulationVariables.mpmInitDistances[1];
        glm::vec3 target   = mSimulationVariables.mpmBoxSizes[1] * glm::vec3(0.5f, 0.25f, 0.5f);
        float zoomRate     = SimulationVariables::MPM_ZOOM_RATE;
        float radius       = 0.6f;
        float diameter     = 2.0f * radius;

        mMlsMpmSimulator =
            std::make_unique<MlsMpmSimulator>(mParticleBuffer, mPosvelBuffer, diameter, mDevice);

        mMlsMpmRenderer = std::make_unique<FluidRenderer>(mDevice,
                                                          mRenderUniforms.screenSize,
                                                          mSurfaceFormat,
                                                          radius,
                                                          fov,
                                                          mRenderUniformBuffer,
                                                          mPosvelBuffer);
    }

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
            pApp->Shutdown();
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
    Shutdown();
#endif
}

float Application::Random()
{
    static std::random_device device;
    static std::mt19937_64 generator(device());
    static std::uniform_real_distribution<float> distribution(0.0f, 1.0f);
    return distribution(generator);
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
    auto maxParticleSize        = std::max(sizeof(SPHParticle), sizeof(MlsMpmParticle));
    bufferDesc.label            = WebGPUUtils::GenerateString("particle storage buffer");
    bufferDesc.size             = maxParticleSize * NUM_PARTICLES_MAX;
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
    SDL_Event event;
    while (SDL_PollEvent(&event))
    {
        switch (event.type)
        {
            case SDL_EVENT_QUIT:
                mIsRunning = false;
                break;

            case SDL_EVENT_MOUSE_MOTION:
                OnMouseMove();
                break;

            case SDL_EVENT_MOUSE_WHEEL:
                OnScroll(event);
                break;

            case SDL_EVENT_MOUSE_BUTTON_DOWN:
            case SDL_EVENT_MOUSE_BUTTON_UP:
                OnMouseButton(event);
                break;

            case SDL_EVENT_KEY_DOWN:
            case SDL_EVENT_KEY_UP:
                OnKeyAction(event);
                break;

            default:
                break;
        }

        ImGui_ImplSDL3_ProcessEvent(&event);
    }
}

void Application::UpdateGame()
{
    if (mSimulationVariables.simulationChnaged)
    {
        if (mSimulationVariables.sph)
        {
            ResetToSPH();
        }
        else
        {
            ResetToMlsMpm();
        }
    }

    if (mSimulationVariables.changed)
    {
        mSimulationVariables.Refresh();
        if (mSimulationVariables.sph)
        {
            mSPHSimulator->Reset(mSimulationVariables.numParticles,
                                 mSimulationVariables.boxSize,
                                 mRenderUniforms);
            glm::vec3 target(0.0f, -mSimulationVariables.boxSize[1] + 0.1, 0.0f);
            mCamera->Reset(mRenderUniforms,
                           mSimulationVariables.initDistance,
                           target,
                           mSimulationVariables.fov,
                           SimulationVariables::SPH_ZOOM_RATE);
        }
        else
        {
            mMlsMpmSimulator->Reset(mSimulationVariables.numParticles,
                                    mSimulationVariables.boxSize,
                                    mRenderUniforms);
            glm::vec3 target = mSimulationVariables.boxSize * glm::vec3(0.5f, 0.25f, 0.5f);
            mCamera->Reset(mRenderUniforms,
                           mSimulationVariables.initDistance,
                           target,
                           mSimulationVariables.fov,
                           SimulationVariables::MPM_ZOOM_RATE);
        }
    }

    if (mSimulationVariables.boxWidthChanged)
    {
        glm::vec3 realBoxSize = mSimulationVariables.boxSize;
        realBoxSize.z *= mSimulationVariables.boxWidthRatio;
        if (mSimulationVariables.sph)
        {
            mSPHSimulator->ChangeBoxSize(realBoxSize);
        }
        else
        {
            mMlsMpmSimulator->ChangeBoxSize(realBoxSize);
        }
    }
}

void Application::GenerateOutput()
{
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

    if (mSimulationVariables.sph)
    {
        mSPHSimulator->Compute(commandEncoder);
        mSPHRenderer->Draw(commandEncoder, targetView, mSimulationVariables);
    }
    else
    {
        mMlsMpmSimulator->Compute(commandEncoder);
        mMlsMpmRenderer->Draw(commandEncoder, targetView, mSimulationVariables);
    }

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

void Application::ResetToSPH()
{
    float fov                         = 45.0f * glm::pi<float>() / 180.0f;
    float initDistance                = 3.0f;
    float zoomRate                    = 0.05f;
    mSimulationVariables.numParticles = 20000;

    mSimulationVariables.boxSize = glm::vec3(1.0f, 2.0f, 1.0f);
    glm::vec3 target(0.0f, -1.0f * mSimulationVariables.boxSize[1] + 0.1f, 0.0f);

    mSPHSimulator->Reset(mSimulationVariables.numParticles,
                         mSimulationVariables.boxSize,
                         mRenderUniforms);

    mCamera->Reset(mRenderUniforms, initDistance, target, fov, zoomRate);
}

void Application::ResetToMlsMpm()
{
    float fov                         = 45.0f * glm::pi<float>() / 180.0f;
    float initDistance                = 70.0f;
    float zoomRate                    = 1.5f;
    float radius                      = 0.6f;
    mSimulationVariables.numParticles = 70000;

    mSimulationVariables.boxSize = glm::vec3(40.0f, 30.0f, 60.0f);
    glm::vec3 target(mSimulationVariables.boxSize[0] / 2.0f,
                     mSimulationVariables.boxSize[1] / 4.0f,
                     mSimulationVariables.boxSize[2] / 2.0f);

    mMlsMpmSimulator->Reset(mSimulationVariables.numParticles,
                            mSimulationVariables.boxSize,
                            mRenderUniforms);

    mCamera->Reset(mRenderUniforms, initDistance, target, fov, zoomRate);
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
        .format          = surfaceTexture.texture.GetFormat(),
        .dimension       = wgpu::TextureViewDimension::e2D,
        .baseMipLevel    = 0,
        .mipLevelCount   = 1,
        .baseArrayLayer  = 0,
        .arrayLayerCount = 1,
        .aspect          = wgpu::TextureAspect::All,
        .usage           = wgpu::TextureUsage::RenderAttachment,
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

void Application::OnMouseMove()
{
    if (!mCamera->isDragging)
        return;

    ImGuiIO& io = ImGui::GetIO();
    if (io.WantCaptureMouse)
    {
        return;
    }

    float xpos = 0, ypos = 0;
    SDL_GetMouseState(&xpos, &ypos);

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

void Application::OnMouseButton(SDL_Event& event)
{
    assert(event.type == SDL_EVENT_MOUSE_BUTTON_DOWN || event.type == SDL_EVENT_MOUSE_BUTTON_UP);

    ImGuiIO& io = ImGui::GetIO();
    if (io.WantCaptureMouse)
    {
        return;
    }

    auto action = event.type;
    auto button = event.button.button;

    if (button == SDL_BUTTON_LEFT)
    {
        switch (action)
        {
            case SDL_EVENT_MOUSE_BUTTON_DOWN:
            {
                mCamera->isDragging = true;
                float xpos, ypos;
                SDL_GetMouseState(&xpos, &ypos);
                mCamera->prevX = xpos;
                mCamera->prevY = ypos;
                break;
            }

            case SDL_EVENT_MOUSE_BUTTON_UP:
            {
                mCamera->isDragging = false;
                break;
            }

            default:
                break;
        }
    }
}

void Application::OnScroll(SDL_Event& event)
{
    assert(event.type == SDL_EVENT_MOUSE_WHEEL);

    float yoffset = static_cast<float>(event.wheel.y);

    mCamera->currentDistance += ((yoffset > 0.0) ? 1.0 : -1.0) * mCamera->zoomRate;
    if (mCamera->currentDistance < mCamera->minDistance)
        mCamera->currentDistance = mCamera->minDistance;
    if (mCamera->currentDistance > mCamera->maxDistance)
        mCamera->currentDistance = mCamera->maxDistance;

    mCamera->RecalculateView(mRenderUniforms);
}

void Application::OnKeyAction(SDL_Event& event)
{
    assert(event.type == SDL_EVENT_KEY_DOWN || event.type == SDL_EVENT_KEY_UP);

    int key     = event.key.scancode;
    bool down   = event.key.down;
    bool repeat = event.key.repeat;

    if (down && key == SDL_SCANCODE_ESCAPE)
    {
        mIsRunning = false;
        return;
    }

    if ((down || repeat) && (key == SDL_SCANCODE_W || key == SDL_SCANCODE_S))
    {
        mCamera->currentDistance += (key == SDL_SCANCODE_W ? 1.0f : -1.0f) * mCamera->zoomRate;
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
    ImGui_ImplSDL3_InitForOther(mWindow);
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
    ImGui_ImplSDL3_Shutdown();
}

void Application::Shutdown()
{
    TerminateGUI();

    SDL_DestroyWindow(mWindow);
    SDL_Quit();
}

bool Application::ShouldClose()
{
    return !mIsRunning;
}
