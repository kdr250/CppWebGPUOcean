#pragma once

#include <webgpu/webgpu_cpp.h>
#include <glm/glm.hpp>

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
                  wgpu::Buffer renderUniformBuffer,
                  wgpu::Buffer posvelBuffer);

    void Draw(wgpu::CommandEncoder& commandEncoder,
              wgpu::TextureView targetView,
              uint32_t numParticles);

private:
    // Fluid
    void InitializeFluidPipelines(wgpu::TextureFormat presentationFormat);
    void InitializeFluidBindGroups(wgpu::Buffer renderUniformBuffer);
    void DrawFluid(wgpu::CommandEncoder& commandEncoder, wgpu::TextureView targetView);

    // Depth map
    void InitializeDepthMapPipeline();
    void InitializeDepthMapBindGroups(wgpu::Buffer renderUniformBuffer, wgpu::Buffer posvelBuffer);
    void DrawDepthMap(wgpu::CommandEncoder& commandEncoder, uint32_t numParticles);

    // Depth filter
    void CreateDepthFilterUniform();
    void InitializeDepthFilterPipeline();
    void InitializeDepthFilterBindGroups(wgpu::Buffer renderUniformBuffer);

    void CreateTextures(const glm::vec2& textureSize);

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
    wgpu::Buffer mDepthFilterUniformBuffer;
    FilterUniform mFilterUniform;

    // Texture Views
    wgpu::TextureView mDepthMapTextureView;
    wgpu::TextureView mTmpDepthMapTextureView;
    wgpu::TextureView mDepthTestTextureView;
};
