#pragma once

#include <webgpu/webgpu_cpp.h>
#include <GLFW/glfw3.h>
#include <memory>

#include "FluidRenderer.h"

#ifdef __EMSCRIPTEN__
    #include <emscripten.h>
    #include <emscripten/html5.h>
#endif

class Application
{
public:
    Application();

    bool Initialize();
    void RunLoop();
    void Shutdown();

private:
    void Loop();

    void ProcessInput();
    void UpdateGame();
    void GenerateOutput();

    wgpu::TextureView GetNextSurfaceTextureView();

    bool ShouldClose();

private:
    GLFWwindow* mWindow;
    wgpu::Device mDevice               = nullptr;
    wgpu::Queue mQueue                 = nullptr;
    wgpu::Surface mSurface             = nullptr;
    wgpu::TextureFormat mSurfaceFormat = wgpu::TextureFormat::Undefined;
    wgpu::RenderPipeline mPipeline     = nullptr;

    std::unique_ptr<FluidRenderer> mFluidRenderer;
};
