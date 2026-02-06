#pragma once

#include <webgpu/webgpu_cpp.h>
#include <GLFW/glfw3.h>

#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/ext.hpp>

#include <memory>

#include "FluidRenderer.h"
#include "Camera.h"
#include "sph/SPHSimulator.h"
#include "mpm/MlsMpmSimulator.h"

#ifdef __EMSCRIPTEN__
    #include <emscripten.h>
    #include <emscripten/html5.h>
#endif

static constexpr int NUM_PARTICLES_MIN = 10000;
static constexpr int NUM_PARTICLES_MAX = 200000;

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
    float _padding;
    glm::vec3 v;
    float _padding2;

    PosVel() : position(glm::vec3(0.0f)), v(glm::vec3(0.0f)) {};
    PosVel(glm::vec3 pos, glm::vec3 vel) : position(pos), v(vel) {};
};

static_assert(sizeof(PosVel) % 16 == 0);

struct SimulationVariables
{
    bool simulationChnaged = false;
    int sph                = 1;

    bool changed     = false;
    bool drawSpheres = false;

    bool boxWidthChanged = false;
    float boxWidthRatio  = 1.0f;

    // SPH
    int sphNumParticles[4] = {
        10000,
        20000,
        30000,
        40000,
    };
    glm::vec3 sphBoxSizes[4] = {
        glm::vec3(0.7f, 2.0f, 0.7f),
        glm::vec3(1.0f, 2.0f, 1.0f),
        glm::vec3(1.2f, 2.0f, 1.2f),
        glm::vec3(1.4f, 2.0f, 1.4f),
    };
    float sphInitDistances[4] = {
        2.6f,
        3.0f,
        3.4f,
        3.8f,
    };

    // MLS-MPM
    int mpmNumParticles[4] = {
        40000,
        70000,
        120000,
        200000,
    };
    glm::vec3 mpmBoxSizes[4] = {
        glm::vec3(35.0f, 25.0f, 55.0f),
        glm::vec3(40.0f, 30.0f, 60.0f),
        glm::vec3(45.0f, 40.0f, 80.0f),
        glm::vec3(50.0f, 50.0f, 80.0f),
    };
    float mpmInitDistances[4] = {
        60.0f,
        70.0f,
        90.0f,
        100.0f,
    };

    int index          = 1;
    int numParticles   = sphNumParticles[index];
    glm::vec3 boxSize  = sphBoxSizes[index];
    float initDistance = sphInitDistances[index];

    float fov                            = 45.0f * glm::pi<float>() / 180.0f;
    static constexpr float SPH_ZOOM_RATE = 0.05f;
    static constexpr float MPM_ZOOM_RATE = 1.5f;

    void Refresh()
    {
        if (sph)
        {
            numParticles = sphNumParticles[index];
            boxSize      = sphBoxSizes[index];
            initDistance = sphInitDistances[index];
        }
        else
        {
            numParticles = mpmNumParticles[index];
            boxSize      = mpmBoxSizes[index];
            initDistance = mpmInitDistances[index];
        }
    }
};

class Application
{
public:
    Application();

    bool Initialize();
    void RunLoop();

    static float Random();

private:
    void InitializeBuffers();

    void Loop();

    void ProcessInput();
    void UpdateGame();
    void GenerateOutput();

    void ResetToSPH();
    void ResetToMlsMpm();

    wgpu::TextureView GetNextSurfaceTextureView();

    wgpu::Limits GetRequiredLimits(wgpu::Adapter adapter) const;

    // Mouse events
    void OnMouseMove(double xpos, double ypos);
    void OnMouseButton(int button, int action, int mods);
    void OnScroll(double xoffset, double yoffset);
    void OnKeyAction(int key, int scancode, int action, int mods);

    // GUI
    void InitializeGUI();
    void TerminateGUI();

    void Shutdown();
    bool ShouldClose();

private:
    GLFWwindow* mWindow;
    wgpu::Instance mInstance           = nullptr;
    wgpu::Device mDevice               = nullptr;
    wgpu::Queue mQueue                 = nullptr;
    wgpu::Surface mSurface             = nullptr;
    wgpu::TextureFormat mSurfaceFormat = wgpu::TextureFormat::Undefined;

    std::unique_ptr<FluidRenderer> mSPHRenderer;
    std::unique_ptr<FluidRenderer> mMlsMpmRenderer;
    std::unique_ptr<Camera> mCamera;

    wgpu::Buffer mRenderUniformBuffer;
    wgpu::Buffer mParticleBuffer;
    wgpu::Buffer mPosvelBuffer;

    RenderUniforms mRenderUniforms;

    std::unique_ptr<SPHSimulator> mSPHSimulator;
    std::unique_ptr<MlsMpmSimulator> mMlsMpmSimulator;

    SimulationVariables mSimulationVariables;
};
