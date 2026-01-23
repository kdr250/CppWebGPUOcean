#include "Utils.h"
#include <cmath>

std::pair<int, int> RadixSortUtils::FindOptimalDispatchSize(wgpu::Device device, int workGroupCount)
{
    auto dispatchSize = std::make_pair(workGroupCount, 1);

    wgpu::Limits limits;
    device.GetLimits(&limits);

    if (workGroupCount > limits.maxComputeWorkgroupsPerDimension)
    {
        float x = std::floor(std::sqrt(workGroupCount));
        float y = std::ceil(workGroupCount / x);

        dispatchSize.first  = static_cast<int>(x);
        dispatchSize.second = static_cast<int>(y);
    }

    return dispatchSize;
}

void RadixSortUtils::SetDefaultBindGroupLayout(wgpu::BindGroupLayoutEntry& bindingLayout)
{
    bindingLayout.buffer.nextInChain      = nullptr;
    bindingLayout.buffer.type             = wgpu::BufferBindingType::BindingNotUsed;
    bindingLayout.buffer.hasDynamicOffset = false;

    bindingLayout.sampler.nextInChain = nullptr;
    bindingLayout.sampler.type        = wgpu::SamplerBindingType::BindingNotUsed;

    bindingLayout.storageTexture.nextInChain   = nullptr;
    bindingLayout.storageTexture.access        = wgpu::StorageTextureAccess::BindingNotUsed;
    bindingLayout.storageTexture.format        = wgpu::TextureFormat::Undefined;
    bindingLayout.storageTexture.viewDimension = wgpu::TextureViewDimension::Undefined;

    bindingLayout.texture.nextInChain   = nullptr;
    bindingLayout.texture.multisampled  = false;
    bindingLayout.texture.sampleType    = wgpu::TextureSampleType::BindingNotUsed;
    bindingLayout.texture.viewDimension = wgpu::TextureViewDimension::Undefined;
}
