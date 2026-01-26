#pragma once

#include <webgpu/webgpu_cpp.h>
#include <GLFW/glfw3.h>

#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_FORCE_LEFT_HANDED
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/ext.hpp>

#include <memory>

#include "FluidRenderer.h"
#include "Camera.h"
#include "sph/SPHSimulator.h"

#ifdef __EMSCRIPTEN__
    #include <emscripten.h>
    #include <emscripten/html5.h>
#endif

struct RenderUniforms
{
    glm::mat4 invProjectionMatrix;
    glm::mat4 projectionMatrix;
    glm::mat4 viewMatrix;
    glm::mat4 invViewMatrix;
    glm::vec2 screenSize;
    glm::vec2 texelSize;
    float sphereSize;

    float _padding[3];
};

static_assert(sizeof(RenderUniforms) % 16 == 0);

struct PosVel
{
    glm::vec3 position;
    glm::vec3 v;

    float _padding[2];

    PosVel() : position(glm::vec3(0.0f)), v(glm::vec3(0.0f)) {};
    PosVel(glm::vec3 pos, glm::vec3 vel) : position(pos), v(vel) {};
};

static_assert(sizeof(PosVel) % 16 == 0);

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

    // Mouse events
    void OnMouseMove(double xpos, double ypos);
    void OnMouseButton(int button, int action, int mods);
    void OnScroll(double xoffset, double yoffset);
    void OnKeyAction(int key, int scancode, int action, int mods);

    bool ShouldClose();

    // FIXME
    void InitializeParticles();

private:
    GLFWwindow* mWindow;
    wgpu::Instance mInstance           = nullptr;
    wgpu::Device mDevice               = nullptr;
    wgpu::Queue mQueue                 = nullptr;
    wgpu::Surface mSurface             = nullptr;
    wgpu::TextureFormat mSurfaceFormat = wgpu::TextureFormat::Undefined;

    std::unique_ptr<FluidRenderer> mFluidRenderer;
    std::unique_ptr<Camera> mCamera;

    wgpu::Buffer mRenderUniformBuffer;
    wgpu::Buffer mParticleBuffer;
    wgpu::Buffer mPosvelBuffer;
    wgpu::Buffer mMapBuffer;  // FIXME

    RenderUniforms mRenderUniforms;

    std::unique_ptr<SPHSimulator> mSPHSimulator;
};
