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

    // Bind groups
    InitializeClearGridBindGroups();
    InitializeP2G1BindGroups();
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
    }

    computePass.End();
}

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
