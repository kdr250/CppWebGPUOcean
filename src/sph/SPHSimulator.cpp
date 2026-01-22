#include "SPHSimulator.h"

#include "../WebGPUUtils.h"
#include "../ResourceManager.h"

SPHSimulator::SPHSimulator(wgpu::Device device,
                           wgpu::Buffer particleBuffer,
                           wgpu::Buffer posvelBuffer,
                           float renderDiameter)
{
    mDevice = device;

    CreateBuffers();

    // Pipelines
    InitializeGridClearPipeline();
    InitializeGridBuildPipeline();

    // BindGroups
    InitializeGridClearBindGroups();
}

void SPHSimulator::Compute(wgpu::CommandEncoder commandEncoder)
{
    wgpu::ComputePassDescriptor computePassDesc {
        .timestampWrites = nullptr,
    };
    wgpu::ComputePassEncoder computePass = commandEncoder.BeginComputePass(&computePassDesc);

    ComputeGridClear(computePass);

    computePass.End();
}

void SPHSimulator::CreateBuffers()
{
    wgpu::BufferDescriptor bufferDesc {};

    // Cell particle count
    bufferDesc.label            = WebGPUUtils::GenerateString("cell particle count buffer");
    bufferDesc.size             = 4 * (mGridCount + 1);
    bufferDesc.usage            = wgpu::BufferUsage::CopyDst | wgpu::BufferUsage::Storage;
    bufferDesc.mappedAtCreation = false;

    mCellParticleCountBuffer = mDevice.CreateBuffer(&bufferDesc);

    // particle cell offset
    bufferDesc.label            = WebGPUUtils::GenerateString("particle cell offset buffer");
    bufferDesc.size             = 4 * NUM_PARTICLES_MAX;
    bufferDesc.usage            = wgpu::BufferUsage::CopyDst | wgpu::BufferUsage::Storage;
    bufferDesc.mappedAtCreation = false;

    mParticleCellOffsetBuffer = mDevice.CreateBuffer(&bufferDesc);

    // environment
    bufferDesc.label            = WebGPUUtils::GenerateString("environment buffer");
    bufferDesc.size             = sizeof(Environment);
    bufferDesc.usage            = wgpu::BufferUsage::CopyDst | wgpu::BufferUsage::Uniform;
    bufferDesc.mappedAtCreation = false;

    mEnvironmentBuffer = mDevice.CreateBuffer(&bufferDesc);

    // SPH params
    bufferDesc.label            = WebGPUUtils::GenerateString("SPH params buffer");
    bufferDesc.size             = sizeof(SPHParams);
    bufferDesc.usage            = wgpu::BufferUsage::CopyDst | wgpu::BufferUsage::Uniform;
    bufferDesc.mappedAtCreation = false;

    mSPHParamsBuffer = mDevice.CreateBuffer(&bufferDesc);
}

void SPHSimulator::InitializeGridClearPipeline()
{
    wgpu::ShaderModule gridClearModule =
        ResourceManager::LoadShaderModule("resources/shader/sph/grid/gridClear.wgsl", mDevice);

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
    mGridClearBindGroupLayout      = mDevice.CreateBindGroupLayout(&bindGroupLayoutDesc);

    // Create the pipeline layout
    wgpu::PipelineLayoutDescriptor layoutDesc {};
    layoutDesc.bindGroupLayoutCount = 1;
    layoutDesc.bindGroupLayouts     = &mGridClearBindGroupLayout;
    mGridClearLayout                = mDevice.CreatePipelineLayout(&layoutDesc);

    // pipelines
    wgpu::ComputePipelineDescriptor computePipelineDesc {
        .label  = WebGPUUtils::GenerateString("grid clear pipeline"),
        .layout = mGridClearLayout,
        .compute =
            {
                .module     = gridClearModule,
                .entryPoint = "main",
            },
    };

    mGridClearPipeline = mDevice.CreateComputePipeline(&computePipelineDesc);
}

void SPHSimulator::InitializeGridClearBindGroups()
{
    std::vector<wgpu::BindGroupEntry> bindings(1);

    bindings[0].binding = 0;
    bindings[0].buffer  = mCellParticleCountBuffer;
    bindings[0].offset  = 0;
    bindings[0].size    = mCellParticleCountBuffer.GetSize();

    wgpu::BindGroupDescriptor bindGroupDesc {
        .label      = WebGPUUtils::GenerateString("grid clear bind group"),
        .layout     = mGridClearBindGroupLayout,
        .entryCount = static_cast<uint32_t>(bindings.size()),
        .entries    = bindings.data(),
    };
    mGridClearBindGroup = mDevice.CreateBindGroup(&bindGroupDesc);
}

void SPHSimulator::ComputeGridClear(wgpu::ComputePassEncoder& computePass)
{
    computePass.SetBindGroup(0, mGridClearBindGroup, 0, nullptr);
    computePass.SetPipeline(mGridClearPipeline);
    computePass.DispatchWorkgroups(std::ceil((mGridCount + 1) / 64));
}

void SPHSimulator::InitializeGridBuildPipeline()
{
    wgpu::ShaderModule gridBuildModule =
        ResourceManager::LoadShaderModule("resources/shader/sph/grid/gridBuild.wgsl", mDevice);

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
    bindingLayout1.buffer.type = wgpu::BufferBindingType::Storage;
    // The uniform buffer binding
    wgpu::BindGroupLayoutEntry& bindingLayout2 = bindingLayoutEentries[2];
    WebGPUUtils::SetDefaultBindGroupLayout(bindingLayout2);
    bindingLayout2.binding     = 2;
    bindingLayout2.visibility  = wgpu::ShaderStage::Compute;
    bindingLayout2.buffer.type = wgpu::BufferBindingType::Storage;
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
    mGridBuildBindGroupLayout      = mDevice.CreateBindGroupLayout(&bindGroupLayoutDesc);

    // Create the pipeline layout
    wgpu::PipelineLayoutDescriptor layoutDesc {};
    layoutDesc.bindGroupLayoutCount = 1;
    layoutDesc.bindGroupLayouts     = &mGridBuildBindGroupLayout;
    mGridBuildLayout                = mDevice.CreatePipelineLayout(&layoutDesc);

    // pipelines
    wgpu::ComputePipelineDescriptor computePipelineDesc {
        .label  = WebGPUUtils::GenerateString("grid build pipeline"),
        .layout = mGridBuildLayout,
        .compute =
            {
                .module     = gridBuildModule,
                .entryPoint = "main",
            },
    };

    mGridBuildPipeline = mDevice.CreateComputePipeline(&computePipelineDesc);
}
