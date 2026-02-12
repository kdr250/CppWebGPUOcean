#include "WebGPUUtils.h"

#include <cstdio>
#include <cassert>
#include <vector>

#ifdef __EMSCRIPTEN__
    #include <emscripten.h>
    #include <emscripten/html5.h>
#endif

wgpu::Adapter WebGPUUtils::RequestAdapterSync(wgpu::Instance instance,
                                              wgpu::RequestAdapterOptions const* options)
{
    wgpu::Adapter result = nullptr;

    wgpu::Future future =
        instance.RequestAdapter(options,
                                wgpu::CallbackMode::WaitAnyOnly,
                                [&result](wgpu::RequestAdapterStatus status,
                                          wgpu::Adapter adapter,
                                          wgpu::StringView message)
                                {
                                    if (status != wgpu::RequestAdapterStatus::Success)
                                    {
                                        printf("Could not get WebGPU adapter: %s\n", message.data);
                                        exit(1);
                                    }
                                    result = std::move(adapter);
                                });
    instance.WaitAny(future, UINT64_MAX);

    return result;
}

wgpu::Device WebGPUUtils::RequestDeviceSync(wgpu::Instance instance,
                                            wgpu::Adapter adapter,
                                            wgpu::DeviceDescriptor const* descripter)
{
    wgpu::Device result = nullptr;

    wgpu::Future future = adapter.RequestDevice(
        descripter,
        wgpu::CallbackMode::AllowSpontaneous,
        [&result](wgpu::RequestDeviceStatus status, wgpu::Device device, wgpu::StringView message)
        {
            if (status != wgpu::RequestDeviceStatus::Success)
            {
                printf("Could not get WebGPU device: %s\n", message.data);
                exit(1);
            }
            result = std::move(device);
        });

    instance.WaitAny(future, UINT64_MAX);

    return result;
}

void WebGPUUtils::InspectAdapter(wgpu::Adapter adapter)
{
    wgpu::Limits supportedLimits = {};
    supportedLimits.nextInChain  = nullptr;

    wgpu::Status status = adapter.GetLimits(&supportedLimits);
    if (status == wgpu::Status::Success)
    {
        printf("Adapter limits:\n");
        printf(" - maxTextureDimension1D: %d\n", supportedLimits.maxTextureDimension1D);
        printf(" - maxTextureDimension2D: %d\n", supportedLimits.maxTextureDimension2D);
        printf(" - maxTextureDimension3D: %d\n", supportedLimits.maxTextureDimension3D);
        printf(" - maxTextureArrayLayers: %d\n", supportedLimits.maxTextureArrayLayers);
    }

    wgpu::SupportedFeatures features;
    adapter.GetFeatures(&features);

    printf("Adapter features:\n");
    for (int i = 0; i < features.featureCount; ++i)
    {
        auto feature = features.features[i];
        printf(" - 0x%08X\n", feature);
    }

    wgpu::AdapterInfo info;
    info.nextInChain = nullptr;
    adapter.GetInfo(&info);

    printf("Adapter properties:\n");
    printf(" - vendorID: %i\n", info.vendorID);
    if (info.vendor.data)
    {
        printf(" - vendor: %s\n", info.vendor.data);
    }
    if (info.architecture.data)
    {
        printf(" - architecture: %s\n", info.architecture.data);
    }
    printf(" - deviceID: %i\n", info.deviceID);
    if (info.device.data)
    {
        printf(" - device: %s\n", info.device.data);
    }
    if (info.description.data)
    {
        printf(" - description: %s\n", info.description.data);
    }
    printf(" - adapterType: 0x%08X\n", info.adapterType);
    printf(" - backendType: 0x%08X\n", info.backendType);
}

void WebGPUUtils::InspectDevice(wgpu::Device device)
{
    wgpu::SupportedFeatures features;
    device.GetFeatures(&features);

    printf("Device features:\n");
    for (int i = 0; i < features.featureCount; ++i)
    {
        auto feature = features.features[i];
        printf(" - 0x%08X\n", feature);
    }

    wgpu::Limits supportedLimits = {};
    supportedLimits.nextInChain  = nullptr;
    wgpu::Status status          = device.GetLimits(&supportedLimits);

    if (status == wgpu::Status::Success)
    {
        printf("Device limits:\n");

        printf(" - maxTextureDimension1D: %d\n", supportedLimits.maxTextureDimension1D);
        printf(" - maxTextureDimension2D: %d\n", supportedLimits.maxTextureDimension2D);
        printf(" - maxTextureDimension3D: %d\n", supportedLimits.maxTextureDimension3D);
        printf(" - maxTextureArrayLayers: %d\n", supportedLimits.maxTextureArrayLayers);
    }
}

wgpu::TextureFormat WebGPUUtils::GetTextureFormat(wgpu::Surface surface, wgpu::Adapter adapter)
{
    wgpu::SurfaceCapabilities capabilities;
    wgpu::Status status = surface.GetCapabilities(adapter, &capabilities);
    if (status != wgpu::Status::Success)
    {
        printf("Could not get surface capabilities! return wgpu::TextureFormat_Undefined\n");
        return wgpu::TextureFormat::Undefined;
    }
    wgpu::TextureFormat surfaceFormat = capabilities.formats[0];

    return surfaceFormat;
}

void WebGPUUtils::SetDefaultLimits(wgpu::Limits& limits)
{
    limits.maxTextureDimension1D                     = WGPU_LIMIT_U32_UNDEFINED;
    limits.maxTextureDimension2D                     = WGPU_LIMIT_U32_UNDEFINED;
    limits.maxTextureDimension3D                     = WGPU_LIMIT_U32_UNDEFINED;
    limits.maxBindGroups                             = WGPU_LIMIT_U32_UNDEFINED;
    limits.maxBindGroupsPlusVertexBuffers            = WGPU_LIMIT_U32_UNDEFINED;
    limits.maxBindingsPerBindGroup                   = WGPU_LIMIT_U32_UNDEFINED;
    limits.maxDynamicUniformBuffersPerPipelineLayout = WGPU_LIMIT_U32_UNDEFINED;
    limits.maxDynamicStorageBuffersPerPipelineLayout = WGPU_LIMIT_U32_UNDEFINED;
    limits.maxSampledTexturesPerShaderStage          = WGPU_LIMIT_U32_UNDEFINED;
    limits.maxSamplersPerShaderStage                 = WGPU_LIMIT_U32_UNDEFINED;
    limits.maxStorageBuffersPerShaderStage           = WGPU_LIMIT_U32_UNDEFINED;
    limits.maxStorageTexturesPerShaderStage          = WGPU_LIMIT_U32_UNDEFINED;
    limits.maxUniformBuffersPerShaderStage           = WGPU_LIMIT_U32_UNDEFINED;
    limits.maxUniformBufferBindingSize               = WGPU_LIMIT_U32_UNDEFINED;
    limits.maxStorageBufferBindingSize               = WGPU_LIMIT_U32_UNDEFINED;
    limits.minUniformBufferOffsetAlignment           = WGPU_LIMIT_U32_UNDEFINED;
    limits.minStorageBufferOffsetAlignment           = WGPU_LIMIT_U32_UNDEFINED;
    limits.maxVertexBuffers                          = WGPU_LIMIT_U32_UNDEFINED;
    limits.maxBufferSize                             = WGPU_LIMIT_U32_UNDEFINED;
    limits.maxVertexAttributes                       = WGPU_LIMIT_U32_UNDEFINED;
    limits.maxVertexBufferArrayStride                = WGPU_LIMIT_U32_UNDEFINED;
    limits.maxInterStageShaderVariables              = WGPU_LIMIT_U32_UNDEFINED;
    limits.maxColorAttachments                       = WGPU_LIMIT_U32_UNDEFINED;
    limits.maxColorAttachmentBytesPerSample          = WGPU_LIMIT_U32_UNDEFINED;
    limits.maxComputeWorkgroupStorageSize            = WGPU_LIMIT_U32_UNDEFINED;
    limits.maxComputeInvocationsPerWorkgroup         = WGPU_LIMIT_U32_UNDEFINED;
    limits.maxComputeWorkgroupSizeX                  = WGPU_LIMIT_U32_UNDEFINED;
    limits.maxComputeWorkgroupSizeY                  = WGPU_LIMIT_U32_UNDEFINED;
    limits.maxComputeWorkgroupSizeZ                  = WGPU_LIMIT_U32_UNDEFINED;
    limits.maxComputeWorkgroupsPerDimension          = WGPU_LIMIT_U32_UNDEFINED;
}

void WebGPUUtils::SetDefaultBindGroupLayout(wgpu::BindGroupLayoutEntry& bindingLayout)
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
