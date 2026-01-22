#include "SPHSimulator.h"

#include "../WebGPUUtils.h"
#include "../ResourceManager.h"

SPHSimulator::SPHSimulator(wgpu::Device device,
                           wgpu::Buffer particleBuffer,
                           wgpu::Buffer posvelBuffer,
                           float renderDiameter)
{
    mDevice = device;

    InitializeGridClearPipeline();
}

void SPHSimulator::Compute(wgpu::CommandEncoder commandEncoder)
{
    // TODO
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
