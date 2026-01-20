#pragma once

#include <webgpu/webgpu_cpp.h>
#include <filesystem>
#include <vector>

class ResourceManager
{
public:
    static wgpu::ShaderModule LoadShaderModule(const std::filesystem::path& path,
                                               wgpu::Device device);

    static wgpu::Texture LoadTexture(const std::filesystem::path& path,
                                     wgpu::Device device,
                                     wgpu::TextureView* pTextureView = nullptr);

    static wgpu::Texture LoadCubemapTexture(const char* paths[6],
                                            wgpu::Device device,
                                            wgpu::TextureView* pTextureView = nullptr);

private:
    template<typename component_t>
    static void WriteMipMaps(wgpu::Device device,
                             wgpu::Texture texture,
                             wgpu::Extent3D textureSize,
                             uint32_t mipLevelCount,
                             const component_t* pixelData,
                             wgpu::Origin3D origin = {0, 0, 0});
};
