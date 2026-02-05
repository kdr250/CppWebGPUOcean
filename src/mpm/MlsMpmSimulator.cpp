#include "MlsMpmSimulator.h"

#include <iostream>

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

    mConstants.stiffness            = 3.0f;
    mConstants.restDensity          = 4.0f;
    mConstants.dynamicViscosity     = 0.1f;
    mConstants.dt                   = 0.2f;
    mConstants.fixedPointMultiplier = 1e7;

    mParticleBuffer = particleBuffer;

    // Buffers
    CreateBuffers();
    WriteBuffers();

    // Pipelines
    InitializeClearGridPipeline();
    InitializeP2G1Pipeline();
    InitializeP2G2Pipeline();
    InitializeUpdateGridPipeline();
    InitializeG2PPipeline();
    InitializeCopyPositionPipeline();

    // Bind groups
    InitializeClearGridBindGroups();
    InitializeP2G1BindGroups();
    InitializeP2G2BindGroups();
    InitializeUpdateGridBindGroups();
    InitializeG2PBindGroups();
    InitializeCopyPositionBindGroups(posvelBuffer);
}

void MlsMpmSimulator::Compute(wgpu::CommandEncoder commandEncoder)
{
    wgpu::ComputePassDescriptor computePassDesc {
        .timestampWrites = nullptr,
    };
    wgpu::ComputePassEncoder computePass = commandEncoder.BeginComputePass(&computePassDesc);

    for (int i = 0; i < 2; ++i)
    {
        ComputeClearGrid(computePass);
        ComputeP2G1(computePass);
        ComputeP2G2(computePass);
        ComputeUpdateGrid(computePass);
        ComputeG2P(computePass);
        ComputeCopyPosition(computePass);
    }

    computePass.End();
}

void MlsMpmSimulator::Reset(int numParticles,
                            const glm::vec3& initHalfBoxSize,
                            RenderUniforms& renderUniforms)
{
    renderUniforms.sphereSize = mRenderDiameter;
    auto particleData         = InitializeDamBreak(initHalfBoxSize, numParticles);
    auto maxGridCount         = mMaxXGrids * mMaxYGrids * mMaxZGrids;
    mGridCount                = std::ceil(initHalfBoxSize[0]) * std::ceil(initHalfBoxSize[1])
                 * std::ceil(initHalfBoxSize[2]);
    if (mGridCount > maxGridCount)
    {
        std::cout << "mGridCount " << mGridCount << " should be equal to or less than maxGridCount "
                  << maxGridCount << std::endl;
        return;
    }

    wgpu::Queue queue = mDevice.GetQueue();
    queue.WriteBuffer(mInitBoxSizeBuffer, 0, glm::value_ptr(initHalfBoxSize), sizeof(glm::vec3));
    queue.WriteBuffer(mRealBoxSizeBuffer, 0, glm::value_ptr(initHalfBoxSize), sizeof(glm::vec3));
    queue.WriteBuffer(mParticleBuffer,
                      0,
                      particleData.data(),
                      sizeof(MlsMpmParticle) * particleData.size());

    std::cout << "MLS-MPM numParticle = " << mNumParticles << std::endl;
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

    // constants
    bufferDesc.label            = WebGPUUtils::GenerateString("constants buffer");
    bufferDesc.size             = sizeof(Constants);
    bufferDesc.usage            = wgpu::BufferUsage::CopyDst | wgpu::BufferUsage::Uniform;
    bufferDesc.mappedAtCreation = false;

    mConstantsBuffer = mDevice.CreateBuffer(&bufferDesc);
}

void MlsMpmSimulator::WriteBuffers()
{
    wgpu::Queue queue = mDevice.GetQueue();

    queue.WriteBuffer(mConstantsBuffer, 0, &mConstants, sizeof(Constants));
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

void MlsMpmSimulator::InitializeClearGridBindGroups()
{
    std::vector<wgpu::BindGroupEntry> bindings(1);

    bindings[0].binding = 0;
    bindings[0].buffer  = mCellBuffer;
    bindings[0].offset  = 0;
    bindings[0].size    = mCellBuffer.GetSize();

    wgpu::BindGroupDescriptor bindGroupDesc {
        .label      = WebGPUUtils::GenerateString("grid clear bind group"),
        .layout     = mClearGridBindGroupLayout,
        .entryCount = static_cast<uint32_t>(bindings.size()),
        .entries    = bindings.data(),
    };
    mClearGridBindGroup = mDevice.CreateBindGroup(&bindGroupDesc);
}

void MlsMpmSimulator::ComputeClearGrid(wgpu::ComputePassEncoder& computePass)
{
    computePass.SetBindGroup(0, mClearGridBindGroup, 0, nullptr);
    computePass.SetPipeline(mClearGridPipeline);
    computePass.DispatchWorkgroups(std::ceil(mGridCount / 64.0f));
}

void MlsMpmSimulator::InitializeP2G1Pipeline()
{
    wgpu::ShaderModule p2g1Module =
        ResourceManager::LoadShaderModule("resources/shader/mls-mpm/p2g_1.wgsl", mDevice);

    // Create bind group entry
    std::vector<wgpu::BindGroupLayoutEntry> bindingLayoutEentries(4);
    // The uniform buffer binding
    wgpu::BindGroupLayoutEntry& bindingLayout0 = bindingLayoutEentries[0];
    WebGPUUtils::SetDefaultBindGroupLayout(bindingLayout0);
    bindingLayout0.binding     = 0;
    bindingLayout0.visibility  = wgpu::ShaderStage::Compute;
    bindingLayout0.buffer.type = wgpu::BufferBindingType::ReadOnlyStorage;
    // The uniform buffer binding
    wgpu::BindGroupLayoutEntry& bindingLayout1 = bindingLayoutEentries[1];
    WebGPUUtils::SetDefaultBindGroupLayout(bindingLayout1);
    bindingLayout1.binding     = 1;
    bindingLayout1.visibility  = wgpu::ShaderStage::Compute;
    bindingLayout1.buffer.type = wgpu::BufferBindingType::Storage;
    // The uniform buffer binding
    wgpu::BindGroupLayoutEntry& bindingLayout2 = bindingLayoutEentries[2];
    WebGPUUtils::SetDefaultBindGroupLayout(bindingLayout2);
    bindingLayout2.binding     = 2;
    bindingLayout2.visibility  = wgpu::ShaderStage::Compute;
    bindingLayout2.buffer.type = wgpu::BufferBindingType::Uniform;
    // The uniform buffer binding
    wgpu::BindGroupLayoutEntry& bindingLayout3 = bindingLayoutEentries[3];
    WebGPUUtils::SetDefaultBindGroupLayout(bindingLayout3);
    bindingLayout3.binding     = 3;
    bindingLayout3.visibility  = wgpu::ShaderStage::Compute;
    bindingLayout3.buffer.type = wgpu::BufferBindingType::Uniform;

    // Create a bind group layout
    wgpu::BindGroupLayoutDescriptor bindGroupLayoutDesc {};
    bindGroupLayoutDesc.entryCount = static_cast<uint32_t>(bindingLayoutEentries.size());
    bindGroupLayoutDesc.entries    = bindingLayoutEentries.data();
    mP2G1BindGroupLayout           = mDevice.CreateBindGroupLayout(&bindGroupLayoutDesc);

    // Create the pipeline layout
    wgpu::PipelineLayoutDescriptor layoutDesc {};
    layoutDesc.bindGroupLayoutCount = 1;
    layoutDesc.bindGroupLayouts     = &mP2G1BindGroupLayout;
    mP2G1Layout                     = mDevice.CreatePipelineLayout(&layoutDesc);

    // pipelines
    wgpu::ComputePipelineDescriptor computePipelineDesc {
        .label  = WebGPUUtils::GenerateString("P2G 1 pipeline"),
        .layout = mP2G1Layout,
        .compute =
            {
                .module     = p2g1Module,
                .entryPoint = "p2g_1",
            },
    };

    mP2G1Pipeline = mDevice.CreateComputePipeline(&computePipelineDesc);
}

void MlsMpmSimulator::InitializeP2G1BindGroups()
{
    std::vector<wgpu::BindGroupEntry> bindings(4);

    bindings[0].binding = 0;
    bindings[0].buffer  = mParticleBuffer;
    bindings[0].offset  = 0;
    bindings[0].size    = mParticleBuffer.GetSize();

    bindings[1].binding = 1;
    bindings[1].buffer  = mCellBuffer;
    bindings[1].offset  = 0;
    bindings[1].size    = mCellBuffer.GetSize();

    bindings[2].binding = 2;
    bindings[2].buffer  = mInitBoxSizeBuffer;
    bindings[2].offset  = 0;
    bindings[2].size    = mInitBoxSizeBuffer.GetSize();

    bindings[3].binding = 3;
    bindings[3].buffer  = mConstantsBuffer;
    bindings[3].offset  = 0;
    bindings[3].size    = mConstantsBuffer.GetSize();

    wgpu::BindGroupDescriptor bindGroupDesc {
        .label      = WebGPUUtils::GenerateString("P2G 1 bind group"),
        .layout     = mP2G1BindGroupLayout,
        .entryCount = static_cast<uint32_t>(bindings.size()),
        .entries    = bindings.data(),
    };
    mP2G1BindGroup = mDevice.CreateBindGroup(&bindGroupDesc);
}

void MlsMpmSimulator::ComputeP2G1(wgpu::ComputePassEncoder& computePass)
{
    computePass.SetBindGroup(0, mP2G1BindGroup, 0, nullptr);
    computePass.SetPipeline(mP2G1Pipeline);
    computePass.DispatchWorkgroups(std::ceil(mNumParticles / 64.0f));
}

void MlsMpmSimulator::InitializeP2G2Pipeline()
{
    wgpu::ShaderModule p2g2Module =
        ResourceManager::LoadShaderModule("resources/shader/mls-mpm/p2g_2.wgsl", mDevice);

    // Create bind group entry
    std::vector<wgpu::BindGroupLayoutEntry> bindingLayoutEentries(4);
    // The uniform buffer binding
    wgpu::BindGroupLayoutEntry& bindingLayout0 = bindingLayoutEentries[0];
    WebGPUUtils::SetDefaultBindGroupLayout(bindingLayout0);
    bindingLayout0.binding     = 0;
    bindingLayout0.visibility  = wgpu::ShaderStage::Compute;
    bindingLayout0.buffer.type = wgpu::BufferBindingType::ReadOnlyStorage;
    // The uniform buffer binding
    wgpu::BindGroupLayoutEntry& bindingLayout1 = bindingLayoutEentries[1];
    WebGPUUtils::SetDefaultBindGroupLayout(bindingLayout1);
    bindingLayout1.binding     = 1;
    bindingLayout1.visibility  = wgpu::ShaderStage::Compute;
    bindingLayout1.buffer.type = wgpu::BufferBindingType::Storage;
    // The uniform buffer binding
    wgpu::BindGroupLayoutEntry& bindingLayout2 = bindingLayoutEentries[2];
    WebGPUUtils::SetDefaultBindGroupLayout(bindingLayout2);
    bindingLayout2.binding     = 2;
    bindingLayout2.visibility  = wgpu::ShaderStage::Compute;
    bindingLayout2.buffer.type = wgpu::BufferBindingType::Uniform;
    // The uniform buffer binding
    wgpu::BindGroupLayoutEntry& bindingLayout3 = bindingLayoutEentries[3];
    WebGPUUtils::SetDefaultBindGroupLayout(bindingLayout3);
    bindingLayout3.binding     = 3;
    bindingLayout3.visibility  = wgpu::ShaderStage::Compute;
    bindingLayout3.buffer.type = wgpu::BufferBindingType::Uniform;

    // Create a bind group layout
    wgpu::BindGroupLayoutDescriptor bindGroupLayoutDesc {};
    bindGroupLayoutDesc.entryCount = static_cast<uint32_t>(bindingLayoutEentries.size());
    bindGroupLayoutDesc.entries    = bindingLayoutEentries.data();
    mP2G2BindGroupLayout           = mDevice.CreateBindGroupLayout(&bindGroupLayoutDesc);

    // Create the pipeline layout
    wgpu::PipelineLayoutDescriptor layoutDesc {};
    layoutDesc.bindGroupLayoutCount = 1;
    layoutDesc.bindGroupLayouts     = &mP2G2BindGroupLayout;
    mP2G2Layout                     = mDevice.CreatePipelineLayout(&layoutDesc);

    // pipelines
    wgpu::ComputePipelineDescriptor computePipelineDesc {
        .label  = WebGPUUtils::GenerateString("P2G 2 pipeline"),
        .layout = mP2G2Layout,
        .compute =
            {
                .module     = p2g2Module,
                .entryPoint = "p2g_2",
            },
    };

    mP2G2Pipeline = mDevice.CreateComputePipeline(&computePipelineDesc);
}

void MlsMpmSimulator::InitializeP2G2BindGroups()
{
    std::vector<wgpu::BindGroupEntry> bindings(4);

    bindings[0].binding = 0;
    bindings[0].buffer  = mParticleBuffer;
    bindings[0].offset  = 0;
    bindings[0].size    = mParticleBuffer.GetSize();

    bindings[1].binding = 1;
    bindings[1].buffer  = mCellBuffer;
    bindings[1].offset  = 0;
    bindings[1].size    = mCellBuffer.GetSize();

    bindings[2].binding = 2;
    bindings[2].buffer  = mInitBoxSizeBuffer;
    bindings[2].offset  = 0;
    bindings[2].size    = mInitBoxSizeBuffer.GetSize();

    bindings[3].binding = 3;
    bindings[3].buffer  = mConstantsBuffer;
    bindings[3].offset  = 0;
    bindings[3].size    = mConstantsBuffer.GetSize();

    wgpu::BindGroupDescriptor bindGroupDesc {
        .label      = WebGPUUtils::GenerateString("P2G 2 bind group"),
        .layout     = mP2G2BindGroupLayout,
        .entryCount = static_cast<uint32_t>(bindings.size()),
        .entries    = bindings.data(),
    };
    mP2G2BindGroup = mDevice.CreateBindGroup(&bindGroupDesc);
}

void MlsMpmSimulator::ComputeP2G2(wgpu::ComputePassEncoder& computePass)
{
    computePass.SetBindGroup(0, mP2G2BindGroup, 0, nullptr);
    computePass.SetPipeline(mP2G2Pipeline);
    computePass.DispatchWorkgroups(std::ceil(mNumParticles / 64.0f));
}

void MlsMpmSimulator::InitializeUpdateGridPipeline()
{
    wgpu::ShaderModule updateGridModule =
        ResourceManager::LoadShaderModule("resources/shader/mls-mpm/updateGrid.wgsl", mDevice);

    // Create bind group entry
    std::vector<wgpu::BindGroupLayoutEntry> bindingLayoutEentries(4);
    // The uniform buffer binding
    wgpu::BindGroupLayoutEntry& bindingLayout0 = bindingLayoutEentries[0];
    WebGPUUtils::SetDefaultBindGroupLayout(bindingLayout0);
    bindingLayout0.binding     = 0;
    bindingLayout0.visibility  = wgpu::ShaderStage::Compute;
    bindingLayout0.buffer.type = wgpu::BufferBindingType::Storage;
    // The uniform buffer binding
    wgpu::BindGroupLayoutEntry& bindingLayout1 = bindingLayoutEentries[1];
    WebGPUUtils::SetDefaultBindGroupLayout(bindingLayout1);
    bindingLayout1.binding     = 1;
    bindingLayout1.visibility  = wgpu::ShaderStage::Compute;
    bindingLayout1.buffer.type = wgpu::BufferBindingType::Uniform;
    // The uniform buffer binding
    wgpu::BindGroupLayoutEntry& bindingLayout2 = bindingLayoutEentries[2];
    WebGPUUtils::SetDefaultBindGroupLayout(bindingLayout2);
    bindingLayout2.binding     = 2;
    bindingLayout2.visibility  = wgpu::ShaderStage::Compute;
    bindingLayout2.buffer.type = wgpu::BufferBindingType::Uniform;
    // The uniform buffer binding
    wgpu::BindGroupLayoutEntry& bindingLayout3 = bindingLayoutEentries[3];
    WebGPUUtils::SetDefaultBindGroupLayout(bindingLayout3);
    bindingLayout3.binding     = 3;
    bindingLayout3.visibility  = wgpu::ShaderStage::Compute;
    bindingLayout3.buffer.type = wgpu::BufferBindingType::Uniform;

    // Create a bind group layout
    wgpu::BindGroupLayoutDescriptor bindGroupLayoutDesc {};
    bindGroupLayoutDesc.entryCount = static_cast<uint32_t>(bindingLayoutEentries.size());
    bindGroupLayoutDesc.entries    = bindingLayoutEentries.data();
    mUpdateGridBindGroupLayout     = mDevice.CreateBindGroupLayout(&bindGroupLayoutDesc);

    // Create the pipeline layout
    wgpu::PipelineLayoutDescriptor layoutDesc {};
    layoutDesc.bindGroupLayoutCount = 1;
    layoutDesc.bindGroupLayouts     = &mUpdateGridBindGroupLayout;
    mUpdateGridLayout               = mDevice.CreatePipelineLayout(&layoutDesc);

    // pipelines
    wgpu::ComputePipelineDescriptor computePipelineDesc {
        .label  = WebGPUUtils::GenerateString("update grid pipeline"),
        .layout = mUpdateGridLayout,
        .compute =
            {
                .module     = updateGridModule,
                .entryPoint = "updateGrid",
            },
    };

    mUpdateGridPipeline = mDevice.CreateComputePipeline(&computePipelineDesc);
}

void MlsMpmSimulator::InitializeUpdateGridBindGroups()
{
    std::vector<wgpu::BindGroupEntry> bindings(4);

    bindings[0].binding = 0;
    bindings[0].buffer  = mCellBuffer;
    bindings[0].offset  = 0;
    bindings[0].size    = mCellBuffer.GetSize();

    bindings[1].binding = 1;
    bindings[1].buffer  = mRealBoxSizeBuffer;
    bindings[1].offset  = 0;
    bindings[1].size    = mRealBoxSizeBuffer.GetSize();

    bindings[2].binding = 2;
    bindings[2].buffer  = mInitBoxSizeBuffer;
    bindings[2].offset  = 0;
    bindings[2].size    = mInitBoxSizeBuffer.GetSize();

    bindings[3].binding = 3;
    bindings[3].buffer  = mConstantsBuffer;
    bindings[3].offset  = 0;
    bindings[3].size    = mConstantsBuffer.GetSize();

    wgpu::BindGroupDescriptor bindGroupDesc {
        .label      = WebGPUUtils::GenerateString("update grid bind group"),
        .layout     = mUpdateGridBindGroupLayout,
        .entryCount = static_cast<uint32_t>(bindings.size()),
        .entries    = bindings.data(),
    };
    mUpdateGridBindGroup = mDevice.CreateBindGroup(&bindGroupDesc);
}

void MlsMpmSimulator::ComputeUpdateGrid(wgpu::ComputePassEncoder& computePass)
{
    computePass.SetBindGroup(0, mUpdateGridBindGroup, 0, nullptr);
    computePass.SetPipeline(mUpdateGridPipeline);
    computePass.DispatchWorkgroups(std::ceil(mGridCount / 64.0f));
}

void MlsMpmSimulator::InitializeG2PPipeline()
{
    wgpu::ShaderModule g2pModule =
        ResourceManager::LoadShaderModule("resources/shader/mls-mpm/g2p.wgsl", mDevice);

    // Create bind group entry
    std::vector<wgpu::BindGroupLayoutEntry> bindingLayoutEentries(5);
    // The uniform buffer binding
    wgpu::BindGroupLayoutEntry& bindingLayout0 = bindingLayoutEentries[0];
    WebGPUUtils::SetDefaultBindGroupLayout(bindingLayout0);
    bindingLayout0.binding     = 0;
    bindingLayout0.visibility  = wgpu::ShaderStage::Compute;
    bindingLayout0.buffer.type = wgpu::BufferBindingType::Storage;
    // The uniform buffer binding
    wgpu::BindGroupLayoutEntry& bindingLayout1 = bindingLayoutEentries[1];
    WebGPUUtils::SetDefaultBindGroupLayout(bindingLayout1);
    bindingLayout1.binding     = 1;
    bindingLayout1.visibility  = wgpu::ShaderStage::Compute;
    bindingLayout1.buffer.type = wgpu::BufferBindingType::ReadOnlyStorage;
    // The uniform buffer binding
    wgpu::BindGroupLayoutEntry& bindingLayout2 = bindingLayoutEentries[2];
    WebGPUUtils::SetDefaultBindGroupLayout(bindingLayout2);
    bindingLayout2.binding     = 2;
    bindingLayout2.visibility  = wgpu::ShaderStage::Compute;
    bindingLayout2.buffer.type = wgpu::BufferBindingType::Uniform;
    // The uniform buffer binding
    wgpu::BindGroupLayoutEntry& bindingLayout3 = bindingLayoutEentries[3];
    WebGPUUtils::SetDefaultBindGroupLayout(bindingLayout3);
    bindingLayout3.binding     = 3;
    bindingLayout3.visibility  = wgpu::ShaderStage::Compute;
    bindingLayout3.buffer.type = wgpu::BufferBindingType::Uniform;
    // The uniform buffer binding
    wgpu::BindGroupLayoutEntry& bindingLayout4 = bindingLayoutEentries[4];
    WebGPUUtils::SetDefaultBindGroupLayout(bindingLayout4);
    bindingLayout4.binding     = 4;
    bindingLayout4.visibility  = wgpu::ShaderStage::Compute;
    bindingLayout4.buffer.type = wgpu::BufferBindingType::Uniform;

    // Create a bind group layout
    wgpu::BindGroupLayoutDescriptor bindGroupLayoutDesc {};
    bindGroupLayoutDesc.entryCount = static_cast<uint32_t>(bindingLayoutEentries.size());
    bindGroupLayoutDesc.entries    = bindingLayoutEentries.data();
    mG2PBindGroupLayout            = mDevice.CreateBindGroupLayout(&bindGroupLayoutDesc);

    // Create the pipeline layout
    wgpu::PipelineLayoutDescriptor layoutDesc {};
    layoutDesc.bindGroupLayoutCount = 1;
    layoutDesc.bindGroupLayouts     = &mG2PBindGroupLayout;
    mG2PLayout                      = mDevice.CreatePipelineLayout(&layoutDesc);

    // pipelines
    wgpu::ComputePipelineDescriptor computePipelineDesc {
        .label  = WebGPUUtils::GenerateString("G2P pipeline"),
        .layout = mG2PLayout,
        .compute =
            {
                .module     = g2pModule,
                .entryPoint = "g2p",
            },
    };

    mG2PPipeline = mDevice.CreateComputePipeline(&computePipelineDesc);
}

void MlsMpmSimulator::InitializeG2PBindGroups()
{
    std::vector<wgpu::BindGroupEntry> bindings(5);

    bindings[0].binding = 0;
    bindings[0].buffer  = mParticleBuffer;
    bindings[0].offset  = 0;
    bindings[0].size    = mParticleBuffer.GetSize();

    bindings[1].binding = 1;
    bindings[1].buffer  = mCellBuffer;
    bindings[1].offset  = 0;
    bindings[1].size    = mCellBuffer.GetSize();

    bindings[2].binding = 2;
    bindings[2].buffer  = mRealBoxSizeBuffer;
    bindings[2].offset  = 0;
    bindings[2].size    = mRealBoxSizeBuffer.GetSize();

    bindings[3].binding = 3;
    bindings[3].buffer  = mInitBoxSizeBuffer;
    bindings[3].offset  = 0;
    bindings[3].size    = mInitBoxSizeBuffer.GetSize();

    bindings[4].binding = 4;
    bindings[4].buffer  = mConstantsBuffer;
    bindings[4].offset  = 0;
    bindings[4].size    = mConstantsBuffer.GetSize();

    wgpu::BindGroupDescriptor bindGroupDesc {
        .label      = WebGPUUtils::GenerateString("G2P bind group"),
        .layout     = mG2PBindGroupLayout,
        .entryCount = static_cast<uint32_t>(bindings.size()),
        .entries    = bindings.data(),
    };
    mG2PBindGroup = mDevice.CreateBindGroup(&bindGroupDesc);
}

void MlsMpmSimulator::ComputeG2P(wgpu::ComputePassEncoder& computePass)
{
    computePass.SetBindGroup(0, mG2PBindGroup, 0, nullptr);
    computePass.SetPipeline(mG2PPipeline);
    computePass.DispatchWorkgroups(std::ceil(mNumParticles / 64.0f));
}

void MlsMpmSimulator::InitializeCopyPositionPipeline()
{
    wgpu::ShaderModule copyPositionModule =
        ResourceManager::LoadShaderModule("resources/shader/mls-mpm/copyPosition.wgsl", mDevice);

    // Create bind group entry
    std::vector<wgpu::BindGroupLayoutEntry> bindingLayoutEentries(2);
    // The uniform buffer binding
    wgpu::BindGroupLayoutEntry& bindingLayout0 = bindingLayoutEentries[0];
    WebGPUUtils::SetDefaultBindGroupLayout(bindingLayout0);
    bindingLayout0.binding     = 0;
    bindingLayout0.visibility  = wgpu::ShaderStage::Compute;
    bindingLayout0.buffer.type = wgpu::BufferBindingType::ReadOnlyStorage;
    // The uniform buffer binding
    wgpu::BindGroupLayoutEntry& bindingLayout1 = bindingLayoutEentries[1];
    WebGPUUtils::SetDefaultBindGroupLayout(bindingLayout1);
    bindingLayout1.binding     = 1;
    bindingLayout1.visibility  = wgpu::ShaderStage::Compute;
    bindingLayout1.buffer.type = wgpu::BufferBindingType::Storage;

    // Create a bind group layout
    wgpu::BindGroupLayoutDescriptor bindGroupLayoutDesc {};
    bindGroupLayoutDesc.entryCount = static_cast<uint32_t>(bindingLayoutEentries.size());
    bindGroupLayoutDesc.entries    = bindingLayoutEentries.data();
    mCopyPositionBindGroupLayout   = mDevice.CreateBindGroupLayout(&bindGroupLayoutDesc);

    // Create the pipeline layout
    wgpu::PipelineLayoutDescriptor layoutDesc {};
    layoutDesc.bindGroupLayoutCount = 1;
    layoutDesc.bindGroupLayouts     = &mCopyPositionBindGroupLayout;
    mCopyPositionLayout             = mDevice.CreatePipelineLayout(&layoutDesc);

    // pipelines
    wgpu::ComputePipelineDescriptor computePipelineDesc {
        .label  = WebGPUUtils::GenerateString("copy position pipeline"),
        .layout = mCopyPositionLayout,
        .compute =
            {
                .module     = copyPositionModule,
                .entryPoint = "copyPosition",
            },
    };

    mCopyPositionPipeline = mDevice.CreateComputePipeline(&computePipelineDesc);
}

void MlsMpmSimulator::InitializeCopyPositionBindGroups(wgpu::Buffer posvelBuffer)
{
    std::vector<wgpu::BindGroupEntry> bindings(2);

    bindings[0].binding = 0;
    bindings[0].buffer  = mParticleBuffer;
    bindings[0].offset  = 0;
    bindings[0].size    = mParticleBuffer.GetSize();

    bindings[1].binding = 1;
    bindings[1].buffer  = posvelBuffer;
    bindings[1].offset  = 0;
    bindings[1].size    = posvelBuffer.GetSize();

    wgpu::BindGroupDescriptor bindGroupDesc {
        .label      = WebGPUUtils::GenerateString("copy position bind group"),
        .layout     = mCopyPositionBindGroupLayout,
        .entryCount = static_cast<uint32_t>(bindings.size()),
        .entries    = bindings.data(),
    };
    mCopyPositionBindGroup = mDevice.CreateBindGroup(&bindGroupDesc);
}

void MlsMpmSimulator::ComputeCopyPosition(wgpu::ComputePassEncoder& computePass)
{
    computePass.SetBindGroup(0, mCopyPositionBindGroup, 0, nullptr);
    computePass.SetPipeline(mCopyPositionPipeline);
    computePass.DispatchWorkgroups(std::ceil(mNumParticles / 64.0f));
}

std::vector<MlsMpmParticle> MlsMpmSimulator::InitializeDamBreak(const glm::vec3& initBoxSize,
                                                                int numParticles)
{
    std::vector<MlsMpmParticle> result(numParticles);
    const float spacing = 0.65f;
    mNumParticles       = 0;

    for (float j = 0.0f; j < initBoxSize[1] * 0.8f && mNumParticles < numParticles; j += spacing)
    {
        for (float i = 3.0f; i < initBoxSize[0] - 4.0f && mNumParticles < numParticles;
             i += spacing)
        {
            for (float k = 3.0f; k < initBoxSize[2] / 2.0f && mNumParticles < numParticles;
                 k += spacing)
            {
                float jitter = 2.0f * Application::Random();
                MlsMpmParticle particle {
                    .position = glm::vec3(i + jitter, j + jitter, k + jitter),
                    .v        = glm::vec3(0.0f),
                    .C1       = glm::vec3(0.0f),
                    .C2       = glm::vec3(0.0f),
                    .C3       = glm::vec3(0.0f),
                };
                result[mNumParticles] = particle;
                mNumParticles++;
            }
        }
    }

    return result;
}
