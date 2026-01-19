#pragma once

#include <webgpu/webgpu_cpp.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <memory>

#include "FluidRenderer.h"

#ifdef __EMSCRIPTEN__
    #include <emscripten.h>
    #include <emscripten/html5.h>
#endif

struct RenderUniforms
{
    glm::vec2 screenSize;
    glm::vec2 texelSize;
    float sphereSize;
    glm::mat4 invProjectionMatrix;
    glm::mat4 projectionMatrix;
    glm::mat4 viewMatrix;
    glm::mat4 invViewMatrix;

    float _padding[3];
};

class Application
{
public:
    Application();

    bool Initialize();
    void RunLoop();
    void Shutdown();

private:
    void InitializeBuffers();

    void Loop();

    void ProcessInput();
    void UpdateGame();
    void GenerateOutput();

    wgpu::TextureView GetNextSurfaceTextureView();

    wgpu::Limits GetRequiredLimits(wgpu::Adapter adapter) const;

    bool ShouldClose();

private:
    GLFWwindow* mWindow;
    wgpu::Device mDevice               = nullptr;
    wgpu::Queue mQueue                 = nullptr;
    wgpu::Surface mSurface             = nullptr;
    wgpu::TextureFormat mSurfaceFormat = wgpu::TextureFormat::Undefined;

    std::unique_ptr<FluidRenderer> mFluidRenderer;

    wgpu::Buffer mRenderUniformBuffer;
    RenderUniforms mRenderUniforms;
};
