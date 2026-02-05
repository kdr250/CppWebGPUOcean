#pragma once

#include <webgpu/webgpu_cpp.h>

#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <PrefixSumKernel.h>

struct RenderUniforms;

struct Environment
{
    int xGrids;
    int yGrids;
    int zGrids;
    float cellSize;
    float xHalf;
    float yHalf;
    float zHalf;
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

struct SPHParticle
{
    glm::vec3 position;
    float _padding1;
    glm::vec3 v;
    float _padding2;
    glm::vec3 force;
    float _padding3;
    float density;
    float nearDensity;
    float _padding4[2];
};

class SPHSimulator
{
public:
    SPHSimulator(wgpu::Device device,
                 wgpu::Buffer particleBuffer,
                 wgpu::Buffer posvelBuffer,
                 float renderDiameter);

    void Compute(wgpu::CommandEncoder commandEncoder);

    void Reset(int numParticles, const glm::vec3& initHalfBoxSize, RenderUniforms& renderUniforms);

    void ChangeBoxSize(const glm::vec3& realBoxSize);

private:
    void CreateBuffers();
    void WriteBuffers(const Environment& environment, const SPHParams& sphParams);

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

    std::vector<SPHParticle> InitializeDamBreak(const glm::vec3& initHalfBoxSize, int numParticles);

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
    wgpu::Buffer mParticleBuffer;

    std::unique_ptr<PrefixSumKernel> mPrefixSumkernel;

    int mGridCount             = 0;
    unsigned int mNumParticles = 0;
    float mKernelRadius        = 0.07;
    float mRenderDiameter;
};
