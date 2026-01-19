#include "ResourceManager.h"

#include <algorithm>
#include <bit>
#include <fstream>
#include <sstream>
#include <string>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

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

wgpu::Texture ResourceManager::LoadTexture(const std::filesystem::path& path,
                                           wgpu::Device device,
                                           wgpu::TextureView* pTextureView)
{
    int width, height, channels;
    unsigned char* pixelData =
        stbi_load(path.string().c_str(), &width, &height, &channels, 4 /* force 4 channels */);

    if (pixelData == nullptr)
    {
        return nullptr;
    }

    wgpu::TextureDescriptor textureDesc;
    textureDesc.nextInChain = nullptr;
    textureDesc.label       = WebGPUUtils::GenerateString("Texture");
    textureDesc.dimension   = wgpu::TextureDimension::e2D;
    textureDesc.format      = wgpu::TextureFormat::RGBA8Unorm;
    textureDesc.size        = {(unsigned int)width, (unsigned int)height, 1};
    textureDesc.mipLevelCount =
        std::bit_width(std::max(textureDesc.size.width, textureDesc.size.height));
    textureDesc.sampleCount     = 1;
    textureDesc.usage           = wgpu::TextureUsage::TextureBinding | wgpu::TextureUsage::CopyDst;
    textureDesc.viewFormatCount = 0;
    textureDesc.viewFormats     = nullptr;
    wgpu::Texture texture       = device.CreateTexture(&textureDesc);

    // Upload data to the GPU texture (to be implemented!)
    WriteMipMaps(device, texture, textureDesc.size, textureDesc.mipLevelCount, pixelData);

    stbi_image_free(pixelData);

    if (pTextureView)
    {
        wgpu::TextureViewDescriptor textureViewDesc;
        textureViewDesc.aspect          = wgpu::TextureAspect::All;
        textureViewDesc.baseArrayLayer  = 0;
        textureViewDesc.arrayLayerCount = 1;
        textureViewDesc.baseMipLevel    = 0;
        textureViewDesc.mipLevelCount   = textureDesc.mipLevelCount;
        textureViewDesc.dimension       = wgpu::TextureViewDimension::e2D;
        textureViewDesc.format          = textureDesc.format;
        textureViewDesc.usage           = wgpu::TextureUsage::None;

        *pTextureView = texture.CreateView(&textureViewDesc);
    }

    return texture;
}

void ResourceManager::WriteMipMaps(wgpu::Device device,
                                   wgpu::Texture texture,
                                   wgpu::Extent3D textureSize,
                                   uint32_t mipLevelCount,
                                   const unsigned char* pixelData)
{
    wgpu::Queue queue = device.GetQueue();

    // Arguments telling which part of the texture we upload to
    wgpu::TexelCopyTextureInfo destination;
    destination.texture = texture;
    destination.origin  = {0, 0, 0};
    destination.aspect  = wgpu::TextureAspect::All;

    // Arguments telling how the C++ side pixel memory is laid out
    wgpu::TexelCopyBufferLayout source;
    source.offset = 0;

    // Create image data
    wgpu::Extent3D mipLevelSize = textureSize;
    std::vector<unsigned char> previousLevelPixels;
    wgpu::Extent3D previousMipLevelSize;
    for (uint32_t level = 0; level < mipLevelCount; ++level)
    {
        // Pixel data for the current level
        std::vector<unsigned char> pixels(4 * mipLevelSize.width * mipLevelSize.height);
        if (level == 0)
        {
            // We cannot really avoid this copy since we need this
            // in previousLevelPixels at the next iteration
            memcpy(pixels.data(), pixelData, pixels.size());
        }
        else
        {
            // Create mip level data
            for (uint32_t i = 0; i < mipLevelSize.width; ++i)
            {
                for (uint32_t j = 0; j < mipLevelSize.height; ++j)
                {
                    unsigned char* p = &pixels[4 * (j * mipLevelSize.width + i)];
                    // Get the corresponding 4 pixels from the previous level
                    unsigned char* p00 =
                        &previousLevelPixels[4
                                             * ((2 * j + 0) * previousMipLevelSize.width
                                                + (2 * i + 0))];
                    unsigned char* p01 =
                        &previousLevelPixels[4
                                             * ((2 * j + 0) * previousMipLevelSize.width
                                                + (2 * i + 1))];
                    unsigned char* p10 =
                        &previousLevelPixels[4
                                             * ((2 * j + 1) * previousMipLevelSize.width
                                                + (2 * i + 0))];
                    unsigned char* p11 =
                        &previousLevelPixels[4
                                             * ((2 * j + 1) * previousMipLevelSize.width
                                                + (2 * i + 1))];
                    // Average
                    p[0] = (p00[0] + p01[0] + p10[0] + p11[0]) / 4;
                    p[1] = (p00[1] + p01[1] + p10[1] + p11[1]) / 4;
                    p[2] = (p00[2] + p01[2] + p10[2] + p11[2]) / 4;
                    p[3] = (p00[3] + p01[3] + p10[3] + p11[3]) / 4;
                }
            }
        }

        // Upload data to the GPU texture
        destination.mipLevel = level;
        source.bytesPerRow   = 4 * mipLevelSize.width;
        source.rowsPerImage  = mipLevelSize.height;
        queue.WriteTexture(&destination, pixels.data(), pixels.size(), &source, &mipLevelSize);

        previousLevelPixels  = std::move(pixels);
        previousMipLevelSize = mipLevelSize;
        mipLevelSize.width /= 2;
        mipLevelSize.height /= 2;
    }
}
