#include "ResourceManager.h"

#include <fstream>
#include <sstream>
#include <string>

#include "WebGPUUtils.h"

wgpu::ShaderModule ResourceManager::LoadShaderModule(const std::filesystem::path& path,
                                                     wgpu::Device device)
{
    std::ifstream file(path);
    if (!file.is_open())
    {
        return nullptr;
    }
    file.seekg(0, std::ios::end);
    size_t size = file.tellg();
    std::string shaderSource(size, ' ');
    file.seekg(0);
    file.read(shaderSource.data(), size);

    wgpu::ShaderSourceWGSL shaderCodeDesc {};
    shaderCodeDesc.nextInChain = nullptr;
    shaderCodeDesc.sType       = wgpu::SType::ShaderSourceWGSL;
    shaderCodeDesc.code        = WebGPUUtils::GenerateString(shaderSource.c_str());

    wgpu::ShaderModuleDescriptor shaderDesc {
        .nextInChain = &shaderCodeDesc,
    };

    wgpu::ShaderModule shaderMdoule = device.CreateShaderModule(&shaderDesc);

    return shaderMdoule;
}
