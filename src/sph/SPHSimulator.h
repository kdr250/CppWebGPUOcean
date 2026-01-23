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

    // Reorder
    void InitializeReorderPipeline();
    void InitializeReorderBindGroups(wgpu::Buffer particleBuffer);
    void ComputeReorder(wgpu::ComputePassEncoder& computePass);

    // Density
    void InitializeDensityPipeline();
    void InitializeDensityBindGroups(wgpu::Buffer particleBuffer);
    void ComputeDensity(wgpu::ComputePassEncoder& computePass);

    // Force
    void InitializeForcePipeline();
    void InitializeForceBindGroups(wgpu::Buffer particleBuffer);
    void ComputeForce(wgpu::ComputePassEncoder& computePass);

    // Integrate
    void InitializeIntegratePipeline();
    void InitializeIntegrateBindGroups(wgpu::Buffer particleBuffer);
    void ComputeIntegrate(wgpu::ComputePassEncoder& computePass);

    // Copy position
    void InitializeCopyPositionPipeline();
    void InitializeCopyPositionBindGroups(wgpu::Buffer particleBuffer, wgpu::Buffer posvelBuffer);
    void ComputeCopyPosition(wgpu::ComputePassEncoder& computePass);

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

    // Reorder
    wgpu::ComputePipeline mReorderPipeline;
    wgpu::PipelineLayout mReorderLayout;
    wgpu::BindGroupLayout mReorderBindGroupLayout;
    wgpu::BindGroup mReorderBindGroup;

    // Density
    wgpu::ComputePipeline mDensityPipeline;
    wgpu::PipelineLayout mDensityLayout;
    wgpu::BindGroupLayout mDensityBindGroupLayout;
    wgpu::BindGroup mDensityBindGroup;

    // Force
    wgpu::ComputePipeline mForcePipeline;
    wgpu::PipelineLayout mForceLayout;
    wgpu::BindGroupLayout mForceBindGroupLayout;
    wgpu::BindGroup mForceBindGroup;

    // Integrate
    wgpu::ComputePipeline mIntegratePipeline;
    wgpu::PipelineLayout mIntegrateLayout;
    wgpu::BindGroupLayout mIntegrateBindGroupLayout;
    wgpu::BindGroup mIntegrateBindGroup;

    // Copy position
    wgpu::ComputePipeline mCopyPositionPipeline;
    wgpu::PipelineLayout mCopyPositionLayout;
    wgpu::BindGroupLayout mCopyPositionBindGroupLayout;
    wgpu::BindGroup mCopyPositionBindGroup;

    // Buffers
    wgpu::Buffer mCellParticleCountBuffer;  // 累積和
    wgpu::Buffer mParticleCellOffsetBuffer;
    wgpu::Buffer mEnvironmentBuffer;
    wgpu::Buffer mSPHParamsBuffer;
    wgpu::Buffer mTargetParticlesBuffer;
    wgpu::Buffer mRealBoxSizeBuffer;

    int mGridCount    = 100;  // FIXME
    int mNumParticles = 100;  // FIXME
};
