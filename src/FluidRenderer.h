#pragma once

#include <webgpu/webgpu_cpp.h>

class FluidRenderer
{
public:
    FluidRenderer(wgpu::Device device,
                  wgpu::TextureFormat presentationFormat,
                  wgpu::Buffer renderUniformBuffer);

    void Draw(wgpu::CommandEncoder& commandEncoder, wgpu::TextureView targetView);

private:
    // Fluid
    void InitializeFluidPipelines(wgpu::TextureFormat presentationFormat);
    void InitializeFluidBindGroups(wgpu::Buffer renderUniformBuffer);
    void DrawFluid(wgpu::CommandEncoder& commandEncoder, wgpu::TextureView targetView);

    // Depth map
    void InitializeDepthMapPipeline();

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
};
