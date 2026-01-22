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
    wgpu::Device mDevice;
};
