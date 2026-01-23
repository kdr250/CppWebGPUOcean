#pragma once

#include <webgpu/webgpu_cpp.h>
#include <utility>
#include <vector>

class PrefixSumKernel
{
public:
    PrefixSumKernel(wgpu::Device device,
                    wgpu::Buffer data,
                    int count,
                    std::pair<int, int> workgroupSize,
                    bool avoidBankConflicts);

    void Dispatch(wgpu::ComputePassEncoder& pass,
                  wgpu::Buffer dispatchSizeBuffer = nullptr,
                  int offset                      = 0);

private:
    struct Pipeline
    {
        wgpu::ComputePipeline pipeline;
        wgpu::BindGroup bindgroup;
        std::pair<int, int> dispatchSize;
    };

    void CreatePassRecursive(wgpu::Buffer data, int count);

private:
    wgpu::Device mDevice;
    std::vector<Pipeline> mPipelines;
    wgpu::ShaderModule mShaderModule;

    std::pair<int, int> mWorkGroupSize;
    int mThreadsPerWorkgroup;
    int mItemsPerWorkgroup;
};
