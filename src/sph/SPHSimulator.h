#pragma once

#include <webgpu/webgpu_cpp.h>

static constexpr int NUM_PARTICLES_MIN           = 10000;
static constexpr int NUM_PARTICLES_MAX           = 200000;
static constexpr int SPH_PARTICLE_STRUCTURE_SIZE = 64;

class SPHSimulator
{
public:
    SPHSimulator(wgpu::Device device,
                 wgpu::Buffer particleBuffer,
                 wgpu::Buffer posvelBuffer,
                 float renderDiameter);

    void Compute(wgpu::CommandEncoder commandEncoder);

private:
    void CreateBuffers();

    void InitializeGridClearPipeline();
    void InitializeGridClearBindGroups();

private:
    wgpu::Device mDevice;

    // Grid Clear
    wgpu::ComputePipeline mGridClearPipeline;
    wgpu::PipelineLayout mGridClearLayout;
    wgpu::BindGroupLayout mGridClearBindGroupLayout;
    wgpu::BindGroup mGridClearBindGroup;

    // Buffers
    wgpu::Buffer mCellParticleCountBuffer;  // 累積和

    int mGridCount;
};
