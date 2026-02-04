#pragma once

#include <webgpu/webgpu_cpp.h>

#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>

struct RenderUniforms;

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

    int mMaxXGrids      = 64;
    int mMaxYGrids      = 64;
    int mMaxZGrids      = 64;
    int mMaxGridCount   = mMaxXGrids * mMaxYGrids * mMaxZGrids;
    int mCellStructSize = 16;
    int mNumParticles   = 0;
    int mGridCount      = 0;
    float mRenderDiameter;
};
