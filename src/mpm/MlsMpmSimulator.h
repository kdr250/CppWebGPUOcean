#pragma once

#include <webgpu/webgpu_cpp.h>

#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>

struct RenderUniforms;

struct Cell
{
    int vx;
    int vy;
    int vz;
    int mass;
};

struct Constants
{
    float stiffness;
    float restDensity;
    float dynamicViscosity;
    float dt;
    float fixedPointMultiplier;
};

class MlsMpmSimulator
{
public:
    MlsMpmSimulator(wgpu::Buffer particleBuffer,
                    wgpu::Buffer posvelBuffer,
                    float renderDiameter,
                    wgpu::Device device);

    void Compute(wgpu::CommandEncoder commandEncoder);

    void Reset(int numParticles, const glm::vec3& initHalfBoxSize, RenderUniforms& renderUniforms);

    void ChangeBoxSize(const glm::vec3& realBoxSize);

private:
    void CreateBuffers();
    void WriteBuffers();

    // clear grid
    void InitializeClearGridPipeline();
    void InitializeClearGridBindGroups();
    void ComputeClearGrid(wgpu::ComputePassEncoder& computePass);

    // P2G #1 pipeline
    void InitializeP2G1Pipeline();
    void InitializeP2G1BindGroups();
    void ComputeP2G1(wgpu::ComputePassEncoder& computePass);

    // P2G #2 pipeline
    void InitializeP2G2Pipeline();
    void InitializeP2G2BindGroups();
    void ComputeP2G2(wgpu::ComputePassEncoder& computePass);

    // Update grid
    void InitializeUpdateGridPipeline();
    void InitializeUpdateGridBindGroups();
    void ComputeUpdateGrid(wgpu::ComputePassEncoder& computePass);

    // G2P
    void InitializeG2PPipeline();
    void InitializeG2PBindGroups();
    void ComputeG2P(wgpu::ComputePassEncoder& computePass);

    // Copy position
    void InitializeCopyPositionPipeline();
    void InitializeCopyPositionBindGroups(wgpu::Buffer posvelBuffer);
    void ComputeCopyPosition(wgpu::ComputePassEncoder& computePass);

private:
    wgpu::Device mDevice;

    // clear grid
    wgpu::ComputePipeline mClearGridPipeline;
    wgpu::PipelineLayout mClearGridLayout;
    wgpu::BindGroupLayout mClearGridBindGroupLayout;
    wgpu::BindGroup mClearGridBindGroup;

    // P2G #1 pipeline
    wgpu::ComputePipeline mP2G1Pipeline;
    wgpu::PipelineLayout mP2G1Layout;
    wgpu::BindGroupLayout mP2G1BindGroupLayout;
    wgpu::BindGroup mP2G1BindGroup;

    // P2G #2 pipeline
    wgpu::ComputePipeline mP2G2Pipeline;
    wgpu::PipelineLayout mP2G2Layout;
    wgpu::BindGroupLayout mP2G2BindGroupLayout;
    wgpu::BindGroup mP2G2BindGroup;

    // update grid
    wgpu::ComputePipeline mUpdateGridPipeline;
    wgpu::PipelineLayout mUpdateGridLayout;
    wgpu::BindGroupLayout mUpdateGridBindGroupLayout;
    wgpu::BindGroup mUpdateGridBindGroup;

    // G2P pipeline
    wgpu::ComputePipeline mG2PPipeline;
    wgpu::PipelineLayout mG2PLayout;
    wgpu::BindGroupLayout mG2PBindGroupLayout;
    wgpu::BindGroup mG2PBindGroup;

    // Copy position
    wgpu::ComputePipeline mCopyPositionPipeline;
    wgpu::PipelineLayout mCopyPositionLayout;
    wgpu::BindGroupLayout mCopyPositionBindGroupLayout;
    wgpu::BindGroup mCopyPositionBindGroup;

    // buffers
    wgpu::Buffer mCellBuffer;
    wgpu::Buffer mRealBoxSizeBuffer;
    wgpu::Buffer mInitBoxSizeBuffer;
    wgpu::Buffer mParticleBuffer;
    wgpu::Buffer mConstantsBuffer;

    int mMaxXGrids    = 64;
    int mMaxYGrids    = 64;
    int mMaxZGrids    = 64;
    int mMaxGridCount = mMaxXGrids * mMaxYGrids * mMaxZGrids;
    int mNumParticles = 0;
    int mGridCount    = 0;
    float mRenderDiameter;

    Constants mConstants;
};
