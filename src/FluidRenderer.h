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

private:
    wgpu::Device mDevice;

    wgpu::PipelineLayout mFluidLayout;
    wgpu::BindGroupLayout mFluidBindGroupLayout;
    wgpu::BindGroup mFluidBindGroup;
    wgpu::RenderPipeline mFluidPipeline;
};
