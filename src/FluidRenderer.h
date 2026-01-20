#pragma once

#include <webgpu/webgpu_cpp.h>
#include <glm/glm.hpp>

class FluidRenderer
{
public:
    FluidRenderer(wgpu::Device device,
                  const glm::vec2& screenSize,
                  wgpu::TextureFormat presentationFormat,
                  wgpu::Buffer renderUniformBuffer,
                  wgpu::Buffer posvelBuffer);

    void Draw(wgpu::CommandEncoder& commandEncoder, wgpu::TextureView targetView);

private:
    // Fluid
    void InitializeFluidPipelines(wgpu::TextureFormat presentationFormat);
    void InitializeFluidBindGroups(wgpu::Buffer renderUniformBuffer);
    void DrawFluid(wgpu::CommandEncoder& commandEncoder, wgpu::TextureView targetView);

    // Depth map
    void InitializeDepthMapPipeline();
    void InitializeDepthMapBindGroups(wgpu::Buffer renderUniformBuffer, wgpu::Buffer posvelBuffer);
    void DrawDepthMap(wgpu::CommandEncoder& commandEncoder);

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
    wgpu::TextureView mDepthMapTextureView;
    wgpu::TextureView mDepthTestTextureView;
};
