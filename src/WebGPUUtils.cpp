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
    struct UserData
    {
        wgpu::Adapter adapter = nullptr;
        bool requestEnded     = false;
    };
    UserData userData;

    auto onAdapterRequestEnded = [](wgpu::RequestAdapterStatus status,
                                    wgpu::Adapter adapter,
                                    wgpu::StringView message,
                                    UserData* pUserData)
    {
        if (status == wgpu::RequestAdapterStatus::Success)
        {
            pUserData->adapter = adapter;
        }
        else
        {
            printf("Could not get WebGPU adapter: %s\n", message.data);
        }
        pUserData->requestEnded = true;
    };

    instance.RequestAdapter(options,
                            wgpu::CallbackMode::AllowSpontaneous,
                            onAdapterRequestEnded,
                            &userData);

#ifdef __EMSCRIPTEN__
    while (!userData.requestEnded)
    {
        emscripten_sleep(100);
    }
#endif

    assert(userData.requestEnded);

    return userData.adapter;
}

wgpu::Device WebGPUUtils::RequestDeviceSync(wgpu::Adapter adapter,
                                            wgpu::DeviceDescriptor const* descripter)
{
    struct UserData
    {
        wgpu::Device device = nullptr;
        bool requestEnded   = false;
    };
    UserData userData;

    auto onDeviceRequestEnded = [](wgpu::RequestDeviceStatus status,
                                   wgpu::Device device,
                                   wgpu::StringView message,
                                   UserData* userData)
    {
        if (status == wgpu::RequestDeviceStatus::Success)
        {
            userData->device = device;
        }
        else
        {
            printf("Could not get WebGPU device: %s\n", message.data);
        }
        userData->requestEnded = true;
    };

    adapter.RequestDevice(descripter,
                          wgpu::CallbackMode::AllowSpontaneous,
                          onDeviceRequestEnded,
                          &userData);

#ifdef __EMSCRIPTEN__
    while (!userData.requestEnded)
    {
        emscripten_sleep(100);
    }
#endif

    assert(userData.requestEnded);

    return userData.device;
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
