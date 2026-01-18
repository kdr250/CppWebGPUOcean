#pragma once

#include <webgpu/webgpu_cpp.h>
#include <filesystem>
#include <vector>

class ResourceManager
{
public:
    static wgpu::ShaderModule LoadShaderModule(const std::filesystem::path& path,
                                               wgpu::Device device);
};
