#include "SPHSimulator.h"

SPHSimulator::SPHSimulator(wgpu::Device device,
                           wgpu::Buffer particleBuffer,
                           wgpu::Buffer posvelBuffer,
                           float renderDiameter)
{
    mDevice = device;
}

void SPHSimulator::Compute(wgpu::CommandEncoder commandEncoder)
{
    // TODO
}
