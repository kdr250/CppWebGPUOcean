#pragma once

#include <webgpu/webgpu_cpp.h>
#include <glm/glm.hpp>

static constexpr int NUM_PARTICLES_MIN           = 10000;
static constexpr int NUM_PARTICLES_MAX           = 200000;
static constexpr int SPH_PARTICLE_STRUCTURE_SIZE = 64;

struct Environment
{
    glm::ivec3 grids;
    float cellSize;
    glm::vec3 half;
    float offset;
};

struct SPHParams
{
    float mass;
    float kernelRadius;
    float kernelRadiusPow2;
    float kernelRadiusPow5;
    float kernelRadiusPow6;
    float kernelRadiusPow9;
    float dt;
    float stiffness;
    float nearStiffness;
    float restDensity;
    float viscosity;
    uint32_t n;
};

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

    // Grid Clear
    void InitializeGridClearPipeline();
    void InitializeGridClearBindGroups();
    void ComputeGridClear(wgpu::ComputePassEncoder& computePass);

    // Grid Build
    void InitializeGridBuildPipeline();
    void InitializeGridBuildBindGroups(wgpu::Buffer particleBuffer);
    void ComputeGridBuild(wgpu::ComputePassEncoder& computePass);

private:
    wgpu::Device mDevice;

    // Grid Clear
    wgpu::ComputePipeline mGridClearPipeline;
    wgpu::PipelineLayout mGridClearLayout;
    wgpu::BindGroupLayout mGridClearBindGroupLayout;
    wgpu::BindGroup mGridClearBindGroup;

    // Grid Build
    wgpu::ComputePipeline mGridBuildPipeline;
    wgpu::PipelineLayout mGridBuildLayout;
    wgpu::BindGroupLayout mGridBuildBindGroupLayout;
    wgpu::BindGroup mGridBuildBindGroup;

    // Buffers
    wgpu::Buffer mCellParticleCountBuffer;  // 累積和
    wgpu::Buffer mParticleCellOffsetBuffer;
    wgpu::Buffer mEnvironmentBuffer;
    wgpu::Buffer mSPHParamsBuffer;

    int mGridCount    = 100;  // FIXME
    int mNumParticles = 100;  // FIXME
};
