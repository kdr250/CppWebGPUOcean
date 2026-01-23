#pragma once

#include <webgpu/webgpu_cpp.h>
#include <utility>

namespace RadixSortUtils
{
    std::pair<int, int> FindOptimalDispatchSize(wgpu::Device device, int workGroupCount);

    void SetDefaultBindGroupLayout(wgpu::BindGroupLayoutEntry& bindingLayout);
}  // namespace RadixSortUtils
