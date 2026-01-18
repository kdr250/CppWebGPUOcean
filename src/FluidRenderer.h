#pragma once

#include <webgpu/webgpu_cpp.h>

class FluidRenderer
{
public:
    FluidRenderer(wgpu::Device device,
                  int width,
                  int height,
                  wgpu::TextureFormat presentationFormat);

    void Draw(wgpu::CommandEncoder& commandEncoder, wgpu::TextureView targetView);

private:
    wgpu::Device mDevice;
    wgpu::RenderPipeline mFluidPipeline;
};
