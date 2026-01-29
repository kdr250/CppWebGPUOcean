#pragma once

#include <webgpu/webgpu_cpp.h>

#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_FORCE_LEFT_HANDED
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>

struct SimulationVariables;

struct FilterUniform
{
    glm::vec2 blurDir;
    float depthThreshold;
    float projectedParticleConstant;
    float maxFilterSize;

    float _padding[3];
};

class FluidRenderer
{
public:
    FluidRenderer(wgpu::Device device,
                  const glm::vec2& screenSize,
                  wgpu::TextureFormat presentationFormat,
                  float radius,
                  float fov,
                  wgpu::Buffer renderUniformBuffer,
                  wgpu::Buffer posvelBuffer);

    void Draw(wgpu::CommandEncoder& commandEncoder,
              wgpu::TextureView targetView,
              SimulationVariables& simulationVariables);

private:
    // Fluid
    void InitializeFluidPipelines(wgpu::TextureFormat presentationFormat,
                                  wgpu::ShaderModule vertexModule);
    void InitializeFluidBindGroups(wgpu::Buffer renderUniformBuffer);
    void DrawFluid(wgpu::CommandEncoder& commandEncoder,
                   wgpu::TextureView targetView,
                   SimulationVariables& simulationVariables);

    // Depth map
    void InitializeDepthMapPipeline();
    void InitializeDepthMapBindGroups(wgpu::Buffer renderUniformBuffer, wgpu::Buffer posvelBuffer);
    void DrawDepthMap(wgpu::CommandEncoder& commandEncoder, uint32_t numParticles);

    // Depth filter
    void CreateDepthFilterUniform(float depthThreshold,
                                  float projectedParticleConstant,
                                  float maxFilterSize);
    void InitializeDepthFilterPipeline(wgpu::ShaderModule vertexModule);
    void InitializeDepthFilterBindGroups(wgpu::Buffer renderUniformBuffer);
    void DrawDepthFilter(wgpu::CommandEncoder& commandEncoder);

    // Thickness map
    void InitializeThicknessMapPipeline();
    void InitializeThicknessMapBindGroups(wgpu::Buffer renderUniformBuffer,
                                          wgpu::Buffer posvelBuffer);
    void DrawThicknessMap(wgpu::CommandEncoder& commandEncoder, uint32_t numParticles);

    // Thickness filter
    void InitializeThicknessFilterPipeline(wgpu::ShaderModule vertexModule);
    void InitializeThicknessFilterBindGroups(wgpu::Buffer renderUniformBuffer);
    void DrawThicknessFilter(wgpu::CommandEncoder& commandEncoder);

    // Sphere
    void InitializeSpherePipelines(wgpu::TextureFormat presentationFormat);
    void InitializeSphereBindGroups(wgpu::Buffer renderUniformBuffer, wgpu::Buffer posvelBuffer);
    void DrawSphere(wgpu::CommandEncoder& commandEncoder,
                    wgpu::TextureView targetView,
                    SimulationVariables& simulationVariables);

    void CreateTextures(const glm::vec2& textureSize);

    // GUI
    void UpdateGUI(wgpu::RenderPassEncoder& renderPass, SimulationVariables& simulationVariables);

private:
    wgpu::Device mDevice;

    // Fluid
    wgpu::PipelineLayout mFluidLayout;
    wgpu::BindGroupLayout mFluidBindGroupLayout;
    wgpu::BindGroup mFluidBindGroup;
    wgpu::RenderPipeline mFluidPipeline;

    // Depth map
    wgpu::PipelineLayout mDepthMapLayout;
    wgpu::BindGroupLayout mDepthMapBindGroupLayout;
    wgpu::BindGroup mDepthMapBindGroup;
    wgpu::RenderPipeline mDepthMapPipeline;

    // Depth filter
    wgpu::PipelineLayout mDepthFilterLayout;
    wgpu::BindGroupLayout mDepthFilterBindGroupLayout;
    wgpu::BindGroup mDepthFilterBindGroups[2];
    wgpu::RenderPipeline mDepthFilterPipeline;
    wgpu::Buffer mFilterXUniformBuffer;
    wgpu::Buffer mFilterYUniformBuffer;
    FilterUniform mFilterXUniform;
    FilterUniform mFilterYUniform;

    // Thickness map
    wgpu::PipelineLayout mThicknessMapLayout;
    wgpu::BindGroupLayout mThicknessMapBindGroupLayout;
    wgpu::BindGroup mThicknessMapBindGroup;
    wgpu::RenderPipeline mThicknessMapPipeline;

    // Thickness filter
    wgpu::PipelineLayout mThicknessFilterLayout;
    wgpu::BindGroupLayout mThicknessFilterBindGroupLayout;
    wgpu::BindGroup mThicknessFilterBindGroups[2];
    wgpu::RenderPipeline mThicknessFilterPipeline;

    // Sphere
    wgpu::PipelineLayout mSphereLayout;
    wgpu::BindGroupLayout mSphereBindGroupLayout;
    wgpu::BindGroup mSphereBindGroup;
    wgpu::RenderPipeline mSpherePipeline;

    // Texture Views
    wgpu::TextureView mDepthMapTextureView;
    wgpu::TextureView mTmpDepthMapTextureView;
    wgpu::TextureView mDepthTestTextureView;
    wgpu::TextureView mThicknessMapTextureView;
    wgpu::TextureView mTmpThicknessMapTextureView;
};
