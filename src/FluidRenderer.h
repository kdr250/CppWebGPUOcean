#pragma once

#include <webgpu/webgpu_cpp.h>

class FluidRenderer
{
public:
    FluidRenderer(wgpu::Device device,
                  int width,
                  int height,
                  wgpu::TextureFormat presentationFormat,
                  wgpu::Buffer renderUniformBuffer);

    void Draw(wgpu::CommandEncoder& commandEncoder, wgpu::TextureView targetView);

private:
    void InitializeFluidPipelines(wgpu::TextureFormat presentationFormat);
    void InitializeFluidBindGroups(wgpu::Buffer renderUniformBuffer);

private:
    wgpu::Device mDevice;

    wgpu::PipelineLayout mFluidLayout;
    wgpu::BindGroupLayout mFluidBindGroupLayout;
    wgpu::BindGroup mFluidBindGroup;
    wgpu::RenderPipeline mFluidPipeline;
};
