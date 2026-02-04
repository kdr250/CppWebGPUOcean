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

    // Buffers
    CreateBuffers();

    // Pipelines
    InitializeClearGridPipeline();

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
    bufferDesc.size             = sizeof(Cell) * mMaxGridCount;
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

void MlsMpmSimulator::InitializeClearGridPipeline()
{
    wgpu::ShaderModule clearGridModule =
        ResourceManager::LoadShaderModule("resources/shader/mls-mpm/clearGrid.wgsl", mDevice);

    // Create bind group entry
    std::vector<wgpu::BindGroupLayoutEntry> bindingLayoutEentries(1);
    // The uniform buffer binding
    wgpu::BindGroupLayoutEntry& bindingLayout = bindingLayoutEentries[0];
    WebGPUUtils::SetDefaultBindGroupLayout(bindingLayout);
    bindingLayout.binding     = 0;
    bindingLayout.visibility  = wgpu::ShaderStage::Compute;
    bindingLayout.buffer.type = wgpu::BufferBindingType::Storage;

    // Create a bind group layout
    wgpu::BindGroupLayoutDescriptor bindGroupLayoutDesc {};
    bindGroupLayoutDesc.entryCount = static_cast<uint32_t>(bindingLayoutEentries.size());
    bindGroupLayoutDesc.entries    = bindingLayoutEentries.data();
    mClearGridBindGroupLayout      = mDevice.CreateBindGroupLayout(&bindGroupLayoutDesc);

    // Create the pipeline layout
    wgpu::PipelineLayoutDescriptor layoutDesc {};
    layoutDesc.bindGroupLayoutCount = 1;
    layoutDesc.bindGroupLayouts     = &mClearGridBindGroupLayout;
    mClearGridLayout                = mDevice.CreatePipelineLayout(&layoutDesc);

    // pipelines
    wgpu::ComputePipelineDescriptor computePipelineDesc {
        .label  = WebGPUUtils::GenerateString("clear grid pipeline"),
        .layout = mClearGridLayout,
        .compute =
            {
                .module     = clearGridModule,
                .entryPoint = "clearGrid",
            },
    };

    mClearGridPipeline = mDevice.CreateComputePipeline(&computePipelineDesc);
}

void MlsMpmSimulator::InitializeClearGridBindGroups() {}

void MlsMpmSimulator::ComputeClearGrid(wgpu::ComputePassEncoder& computePass) {}
