#include "MlsMpmSimulator.h"

#include "../WebGPUUtils.h"
#include "../ResourceManager.h"
#include "../Application.h"

MlsMpmSimulator::MlsMpmSimulator(wgpu::Buffer particleBuffer,
                                 wgpu::Buffer posvelBuffer,
                                 float renderDiameter,
                                 wgpu::Device device)
{
    mDevice         = device;
    mRenderDiameter = renderDiameter;

    CreateBuffers();

    mParticleBuffer = particleBuffer;
}

void MlsMpmSimulator::Compute(wgpu::CommandEncoder commandEncoder) {}

void MlsMpmSimulator::Reset(int numParticles,
                            const glm::vec3& initHalfBoxSize,
                            RenderUniforms& renderUniforms)
{
}

void MlsMpmSimulator::ChangeBoxSize(const glm::vec3& realBoxSize) {}

void MlsMpmSimulator::CreateBuffers()
{
    wgpu::BufferDescriptor bufferDesc {};

    // cell
    bufferDesc.label            = WebGPUUtils::GenerateString("cell buffer");
    bufferDesc.size             = mCellStructSize * mMaxGridCount;
    bufferDesc.usage            = wgpu::BufferUsage::CopyDst | wgpu::BufferUsage::Storage;
    bufferDesc.mappedAtCreation = false;

    mCellBuffer = mDevice.CreateBuffer(&bufferDesc);

    // real box size
    bufferDesc.label            = WebGPUUtils::GenerateString("real box size buffer");
    bufferDesc.size             = sizeof(glm::vec3);
    bufferDesc.usage            = wgpu::BufferUsage::CopyDst | wgpu::BufferUsage::Uniform;
    bufferDesc.mappedAtCreation = false;

    mRealBoxSizeBuffer = mDevice.CreateBuffer(&bufferDesc);

    // init box size
    bufferDesc.label            = WebGPUUtils::GenerateString("init box size buffer");
    bufferDesc.size             = sizeof(glm::vec3);
    bufferDesc.usage            = wgpu::BufferUsage::CopyDst | wgpu::BufferUsage::Uniform;
    bufferDesc.mappedAtCreation = false;

    mInitBoxSizeBuffer = mDevice.CreateBuffer(&bufferDesc);
}
