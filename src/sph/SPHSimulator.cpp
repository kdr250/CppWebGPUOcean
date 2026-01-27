#include "SPHSimulator.h"

#include <glm/gtc/type_ptr.hpp>
#include <random>

#include "../WebGPUUtils.h"
#include "../ResourceManager.h"
#include "../Application.h"

SPHSimulator::SPHSimulator(wgpu::Device device,
                           wgpu::Buffer particleBuffer,
                           wgpu::Buffer posvelBuffer,
                           float renderDiameter)
{
    mDevice = device;

    mRenderDiameter = renderDiameter;

    float cellSize = 1.0f * mKernelRadius;
    glm::vec3 halfMax(2.0f, 2.0f, 2.0f);
    glm::vec3 length = halfMax * 2.0f;
    float sentinel   = 4.0f * cellSize;
    glm::vec3 grids(0.0f);
    grids.x      = std::ceil((length.x + sentinel) / cellSize);
    grids.y      = std::ceil((length.y + sentinel) / cellSize);
    grids.z      = std::ceil((length.z + sentinel) / cellSize);
    mGridCount   = (int)(grids.x * grids.y * grids.z);
    float offset = sentinel / 2.0f;

    float stiffness     = 20.0f;
    float nearStiffness = 1.0f;
    float mass          = 1.0f;
    float restDensity   = 15000.0f;
    float viscosity     = 100.0f;
    float dt            = 0.006f;

    Environment environment {
        .grids    = grids,
        .cellSize = cellSize,
        .half     = halfMax,
        .offset   = offset,
    };

    SPHParams sphParams {
        .mass             = mass,
        .kernelRadius     = mKernelRadius,
        .kernelRadiusPow2 = std::pow(mKernelRadius, 2.0f),
        .kernelRadiusPow5 = std::pow(mKernelRadius, 5.0f),
        .kernelRadiusPow6 = std::pow(mKernelRadius, 6.0f),
        .kernelRadiusPow9 = std::pow(mKernelRadius, 9.0f),
        .dt               = dt,
        .stiffness        = stiffness,
        .nearStiffness    = nearStiffness,
        .restDensity      = restDensity,
        .viscosity        = viscosity,
    };

    // Buffers
    CreateBuffers();
    WriteBuffers(environment, sphParams);

    // Pipelines
    InitializeGridClearPipeline();
    InitializeGridBuildPipeline();
    InitializeReorderPipeline();
    InitializeDensityPipeline();
    InitializeForcePipeline();
    InitializeIntegratePipeline();
    InitializeCopyPositionPipeline();

    // BindGroups
    InitializeGridClearBindGroups();
    InitializeGridBuildBindGroups(particleBuffer);
    InitializeReorderBindGroups(particleBuffer);
    InitializeDensityBindGroups(particleBuffer);
    InitializeForceBindGroups(particleBuffer);
    InitializeIntegrateBindGroups(particleBuffer);
    InitializeCopyPositionBindGroups(particleBuffer, posvelBuffer);

    mPrefixSumkernel =
        std::make_unique<PrefixSumKernel>(mDevice, mCellParticleCountBuffer, mGridCount + 1);

    mParticleBuffer = particleBuffer;
}

void SPHSimulator::Compute(wgpu::CommandEncoder commandEncoder)
{
    wgpu::ComputePassDescriptor computePassDesc {
        .timestampWrites = nullptr,
    };
    wgpu::ComputePassEncoder computePass = commandEncoder.BeginComputePass(&computePassDesc);

    for (int i = 0; i < 2; ++i)
    {
        ComputeGridClear(computePass);
        ComputeGridBuild(computePass);
        mPrefixSumkernel->Dispatch(computePass);
        ComputeReorder(computePass);
        ComputeDensity(computePass);
        ComputeReorder(computePass);
        ComputeForce(computePass);
        ComputeIntegrate(computePass);
        ComputeCopyPosition(computePass);
    }

    computePass.End();
}

void SPHSimulator::Reset(int numParticles,
                         const glm::vec3& initHalfBoxSize,
                         RenderUniforms& renderUniforms)
{
    renderUniforms.sphereSize = mRenderDiameter;

    std::vector<SPHParticle> particles = InitializeDamBreak(initHalfBoxSize, numParticles);

    wgpu::Queue queue = mDevice.GetQueue();
    queue.WriteBuffer(mSPHParamsBuffer,
                      offsetof(SPHParams, n),
                      &mNumParticles,
                      sizeof(unsigned int));
    queue.WriteBuffer(mParticleBuffer, 0, particles.data(), sizeof(SPHParticle) * particles.size());
    queue.WriteBuffer(mRealBoxSizeBuffer, 0, glm::value_ptr(initHalfBoxSize), sizeof(glm::vec3));
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

    // target particles
    bufferDesc.label            = WebGPUUtils::GenerateString("target particles buffer");
    bufferDesc.size             = SPH_PARTICLE_STRUCTURE_SIZE * NUM_PARTICLES_MAX;
    bufferDesc.usage            = wgpu::BufferUsage::CopyDst | wgpu::BufferUsage::Storage;
    bufferDesc.mappedAtCreation = false;

    mTargetParticlesBuffer = mDevice.CreateBuffer(&bufferDesc);

    // real box size
    bufferDesc.label            = WebGPUUtils::GenerateString("real box size buffer");
    bufferDesc.size             = sizeof(glm::vec3);
    bufferDesc.usage            = wgpu::BufferUsage::CopyDst | wgpu::BufferUsage::Uniform;
    bufferDesc.mappedAtCreation = false;

    mRealBoxSizeBuffer = mDevice.CreateBuffer(&bufferDesc);
}

void SPHSimulator::WriteBuffers(const Environment& environment, const SPHParams& sphParams)
{
    auto queue = mDevice.GetQueue();
    queue.WriteBuffer(mEnvironmentBuffer, 0, &environment, sizeof(Environment));
    queue.WriteBuffer(mSPHParamsBuffer, 0, &sphParams, sizeof(SPHParams));
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
    computePass.DispatchWorkgroups(std::ceil((mGridCount + 1) / 64.0f));
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

void SPHSimulator::InitializeGridBuildBindGroups(wgpu::Buffer particleBuffer)
{
    std::vector<wgpu::BindGroupEntry> bindings(5);

    bindings[0].binding = 0;
    bindings[0].buffer  = mCellParticleCountBuffer;
    bindings[0].offset  = 0;
    bindings[0].size    = mCellParticleCountBuffer.GetSize();

    bindings[1].binding = 1;
    bindings[1].buffer  = mParticleCellOffsetBuffer;
    bindings[1].offset  = 0;
    bindings[1].size    = mParticleCellOffsetBuffer.GetSize();

    bindings[2].binding = 2;
    bindings[2].buffer  = particleBuffer;
    bindings[2].offset  = 0;
    bindings[2].size    = particleBuffer.GetSize();

    bindings[3].binding = 3;
    bindings[3].buffer  = mEnvironmentBuffer;
    bindings[3].offset  = 0;
    bindings[3].size    = mEnvironmentBuffer.GetSize();

    bindings[4].binding = 4;
    bindings[4].buffer  = mSPHParamsBuffer;
    bindings[4].offset  = 0;
    bindings[4].size    = mSPHParamsBuffer.GetSize();

    wgpu::BindGroupDescriptor bindGroupDesc {
        .label      = WebGPUUtils::GenerateString("grid build bind group"),
        .layout     = mGridBuildBindGroupLayout,
        .entryCount = static_cast<uint32_t>(bindings.size()),
        .entries    = bindings.data(),
    };
    mGridBuildBindGroup = mDevice.CreateBindGroup(&bindGroupDesc);
}

void SPHSimulator::ComputeGridBuild(wgpu::ComputePassEncoder& computePass)
{
    computePass.SetBindGroup(0, mGridBuildBindGroup, 0, nullptr);
    computePass.SetPipeline(mGridBuildPipeline);
    computePass.DispatchWorkgroups(std::ceil(mNumParticles / 64.0f));
}

void SPHSimulator::InitializeReorderPipeline()
{
    wgpu::ShaderModule reorderParticlesModule =
        ResourceManager::LoadShaderModule("resources/shader/sph/grid/reorderParticles.wgsl",
                                          mDevice);

    // Create bind group entry
    std::vector<wgpu::BindGroupLayoutEntry> bindingLayoutEentries(6);
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
    bindingLayout2.buffer.type = wgpu::BufferBindingType::ReadOnlyStorage;
    // The uniform buffer binding
    wgpu::BindGroupLayoutEntry& bindingLayout3 = bindingLayoutEentries[3];
    WebGPUUtils::SetDefaultBindGroupLayout(bindingLayout3);
    bindingLayout3.binding     = 3;
    bindingLayout3.visibility  = wgpu::ShaderStage::Compute;
    bindingLayout3.buffer.type = wgpu::BufferBindingType::ReadOnlyStorage;
    // The uniform buffer binding
    wgpu::BindGroupLayoutEntry& bindingLayout4 = bindingLayoutEentries[4];
    WebGPUUtils::SetDefaultBindGroupLayout(bindingLayout4);
    bindingLayout4.binding     = 4;
    bindingLayout4.visibility  = wgpu::ShaderStage::Compute;
    bindingLayout4.buffer.type = wgpu::BufferBindingType::Uniform;
    // The uniform buffer binding
    wgpu::BindGroupLayoutEntry& bindingLayout5 = bindingLayoutEentries[5];
    WebGPUUtils::SetDefaultBindGroupLayout(bindingLayout5);
    bindingLayout5.binding     = 5;
    bindingLayout5.visibility  = wgpu::ShaderStage::Compute;
    bindingLayout5.buffer.type = wgpu::BufferBindingType::Uniform;

    // Create a bind group layout
    wgpu::BindGroupLayoutDescriptor bindGroupLayoutDesc {};
    bindGroupLayoutDesc.entryCount = static_cast<uint32_t>(bindingLayoutEentries.size());
    bindGroupLayoutDesc.entries    = bindingLayoutEentries.data();
    mReorderBindGroupLayout        = mDevice.CreateBindGroupLayout(&bindGroupLayoutDesc);

    // Create the pipeline layout
    wgpu::PipelineLayoutDescriptor layoutDesc {};
    layoutDesc.bindGroupLayoutCount = 1;
    layoutDesc.bindGroupLayouts     = &mReorderBindGroupLayout;
    mReorderLayout                  = mDevice.CreatePipelineLayout(&layoutDesc);

    // pipelines
    wgpu::ComputePipelineDescriptor computePipelineDesc {
        .label  = WebGPUUtils::GenerateString("reorder particles pipeline"),
        .layout = mReorderLayout,
        .compute =
            {
                .module     = reorderParticlesModule,
                .entryPoint = "main",
            },
    };

    mReorderPipeline = mDevice.CreateComputePipeline(&computePipelineDesc);
}

void SPHSimulator::InitializeReorderBindGroups(wgpu::Buffer particleBuffer)
{
    std::vector<wgpu::BindGroupEntry> bindings(6);

    bindings[0].binding = 0;
    bindings[0].buffer  = particleBuffer;
    bindings[0].offset  = 0;
    bindings[0].size    = particleBuffer.GetSize();

    bindings[1].binding = 1;
    bindings[1].buffer  = mTargetParticlesBuffer;
    bindings[1].offset  = 0;
    bindings[1].size    = mTargetParticlesBuffer.GetSize();

    bindings[2].binding = 2;
    bindings[2].buffer  = mCellParticleCountBuffer;
    bindings[2].offset  = 0;
    bindings[2].size    = mCellParticleCountBuffer.GetSize();

    bindings[3].binding = 3;
    bindings[3].buffer  = mParticleCellOffsetBuffer;
    bindings[3].offset  = 0;
    bindings[3].size    = mParticleCellOffsetBuffer.GetSize();

    bindings[4].binding = 4;
    bindings[4].buffer  = mEnvironmentBuffer;
    bindings[4].offset  = 0;
    bindings[4].size    = mEnvironmentBuffer.GetSize();

    bindings[5].binding = 5;
    bindings[5].buffer  = mSPHParamsBuffer;
    bindings[5].offset  = 0;
    bindings[5].size    = mSPHParamsBuffer.GetSize();

    wgpu::BindGroupDescriptor bindGroupDesc {
        .label      = WebGPUUtils::GenerateString("reorder bind group"),
        .layout     = mReorderBindGroupLayout,
        .entryCount = static_cast<uint32_t>(bindings.size()),
        .entries    = bindings.data(),
    };
    mReorderBindGroup = mDevice.CreateBindGroup(&bindGroupDesc);
}

void SPHSimulator::ComputeReorder(wgpu::ComputePassEncoder& computePass)
{
    computePass.SetBindGroup(0, mReorderBindGroup, 0, nullptr);
    computePass.SetPipeline(mReorderPipeline);
    computePass.DispatchWorkgroups(std::ceil(mNumParticles / 64.0f));
}

void SPHSimulator::InitializeDensityPipeline()
{
    wgpu::ShaderModule densityModule =
        ResourceManager::LoadShaderModule("resources/shader/sph/density.wgsl", mDevice);

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
    bindingLayout2.buffer.type = wgpu::BufferBindingType::ReadOnlyStorage;
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
    mDensityBindGroupLayout        = mDevice.CreateBindGroupLayout(&bindGroupLayoutDesc);

    // Create the pipeline layout
    wgpu::PipelineLayoutDescriptor layoutDesc {};
    layoutDesc.bindGroupLayoutCount = 1;
    layoutDesc.bindGroupLayouts     = &mDensityBindGroupLayout;
    mDensityLayout                  = mDevice.CreatePipelineLayout(&layoutDesc);

    // pipelines
    wgpu::ComputePipelineDescriptor computePipelineDesc {
        .label  = WebGPUUtils::GenerateString("density pipeline"),
        .layout = mDensityLayout,
        .compute =
            {
                .module     = densityModule,
                .entryPoint = "computeDensity",
            },
    };

    mDensityPipeline = mDevice.CreateComputePipeline(&computePipelineDesc);
}

void SPHSimulator::InitializeDensityBindGroups(wgpu::Buffer particleBuffer)
{
    std::vector<wgpu::BindGroupEntry> bindings(5);

    bindings[0].binding = 0;
    bindings[0].buffer  = particleBuffer;
    bindings[0].offset  = 0;
    bindings[0].size    = particleBuffer.GetSize();

    bindings[1].binding = 1;
    bindings[1].buffer  = mTargetParticlesBuffer;
    bindings[1].offset  = 0;
    bindings[1].size    = mTargetParticlesBuffer.GetSize();

    bindings[2].binding = 2;
    bindings[2].buffer  = mCellParticleCountBuffer;
    bindings[2].offset  = 0;
    bindings[2].size    = mCellParticleCountBuffer.GetSize();

    bindings[3].binding = 3;
    bindings[3].buffer  = mEnvironmentBuffer;
    bindings[3].offset  = 0;
    bindings[3].size    = mEnvironmentBuffer.GetSize();

    bindings[4].binding = 4;
    bindings[4].buffer  = mSPHParamsBuffer;
    bindings[4].offset  = 0;
    bindings[4].size    = mSPHParamsBuffer.GetSize();

    wgpu::BindGroupDescriptor bindGroupDesc {
        .label      = WebGPUUtils::GenerateString("density bind group"),
        .layout     = mDensityBindGroupLayout,
        .entryCount = static_cast<uint32_t>(bindings.size()),
        .entries    = bindings.data(),
    };
    mDensityBindGroup = mDevice.CreateBindGroup(&bindGroupDesc);
}

void SPHSimulator::ComputeDensity(wgpu::ComputePassEncoder& computePass)
{
    computePass.SetBindGroup(0, mDensityBindGroup, 0, nullptr);
    computePass.SetPipeline(mDensityPipeline);
    computePass.DispatchWorkgroups(std::ceil(mNumParticles / 64.0f));
}

void SPHSimulator::InitializeForcePipeline()
{
    wgpu::ShaderModule forceModule =
        ResourceManager::LoadShaderModule("resources/shader/sph/force.wgsl", mDevice);

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
    bindingLayout2.buffer.type = wgpu::BufferBindingType::ReadOnlyStorage;
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
    mForceBindGroupLayout          = mDevice.CreateBindGroupLayout(&bindGroupLayoutDesc);

    // Create the pipeline layout
    wgpu::PipelineLayoutDescriptor layoutDesc {};
    layoutDesc.bindGroupLayoutCount = 1;
    layoutDesc.bindGroupLayouts     = &mForceBindGroupLayout;
    mForceLayout                    = mDevice.CreatePipelineLayout(&layoutDesc);

    // pipelines
    wgpu::ComputePipelineDescriptor computePipelineDesc {
        .label  = WebGPUUtils::GenerateString("force pipeline"),
        .layout = mForceLayout,
        .compute =
            {
                .module     = forceModule,
                .entryPoint = "computeForce",
            },
    };

    mForcePipeline = mDevice.CreateComputePipeline(&computePipelineDesc);
}

void SPHSimulator::InitializeForceBindGroups(wgpu::Buffer particleBuffer)
{
    std::vector<wgpu::BindGroupEntry> bindings(5);

    bindings[0].binding = 0;
    bindings[0].buffer  = particleBuffer;
    bindings[0].offset  = 0;
    bindings[0].size    = particleBuffer.GetSize();

    bindings[1].binding = 1;
    bindings[1].buffer  = mTargetParticlesBuffer;
    bindings[1].offset  = 0;
    bindings[1].size    = mTargetParticlesBuffer.GetSize();

    bindings[2].binding = 2;
    bindings[2].buffer  = mCellParticleCountBuffer;
    bindings[2].offset  = 0;
    bindings[2].size    = mCellParticleCountBuffer.GetSize();

    bindings[3].binding = 3;
    bindings[3].buffer  = mEnvironmentBuffer;
    bindings[3].offset  = 0;
    bindings[3].size    = mEnvironmentBuffer.GetSize();

    bindings[4].binding = 4;
    bindings[4].buffer  = mSPHParamsBuffer;
    bindings[4].offset  = 0;
    bindings[4].size    = mSPHParamsBuffer.GetSize();

    wgpu::BindGroupDescriptor bindGroupDesc {
        .label      = WebGPUUtils::GenerateString("force bind group"),
        .layout     = mForceBindGroupLayout,
        .entryCount = static_cast<uint32_t>(bindings.size()),
        .entries    = bindings.data(),
    };
    mForceBindGroup = mDevice.CreateBindGroup(&bindGroupDesc);
}

void SPHSimulator::ComputeForce(wgpu::ComputePassEncoder& computePass)
{
    computePass.SetBindGroup(0, mForceBindGroup, 0, nullptr);
    computePass.SetPipeline(mForcePipeline);
    computePass.DispatchWorkgroups(std::ceil(mNumParticles / 64.0f));
}

void SPHSimulator::InitializeIntegratePipeline()
{
    wgpu::ShaderModule integrateModule =
        ResourceManager::LoadShaderModule("resources/shader/sph/integrate.wgsl", mDevice);

    // Create bind group entry
    std::vector<wgpu::BindGroupLayoutEntry> bindingLayoutEentries(3);
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

    // Create a bind group layout
    wgpu::BindGroupLayoutDescriptor bindGroupLayoutDesc {};
    bindGroupLayoutDesc.entryCount = static_cast<uint32_t>(bindingLayoutEentries.size());
    bindGroupLayoutDesc.entries    = bindingLayoutEentries.data();
    mIntegrateBindGroupLayout      = mDevice.CreateBindGroupLayout(&bindGroupLayoutDesc);

    // Create the pipeline layout
    wgpu::PipelineLayoutDescriptor layoutDesc {};
    layoutDesc.bindGroupLayoutCount = 1;
    layoutDesc.bindGroupLayouts     = &mIntegrateBindGroupLayout;
    mIntegrateLayout                = mDevice.CreatePipelineLayout(&layoutDesc);

    // pipelines
    wgpu::ComputePipelineDescriptor computePipelineDesc {
        .label  = WebGPUUtils::GenerateString("integrate pipeline"),
        .layout = mIntegrateLayout,
        .compute =
            {
                .module     = integrateModule,
                .entryPoint = "integrate",
            },
    };

    mIntegratePipeline = mDevice.CreateComputePipeline(&computePipelineDesc);
}

void SPHSimulator::InitializeIntegrateBindGroups(wgpu::Buffer particleBuffer)
{
    std::vector<wgpu::BindGroupEntry> bindings(3);

    bindings[0].binding = 0;
    bindings[0].buffer  = particleBuffer;
    bindings[0].offset  = 0;
    bindings[0].size    = particleBuffer.GetSize();

    bindings[1].binding = 1;
    bindings[1].buffer  = mRealBoxSizeBuffer;
    bindings[1].offset  = 0;
    bindings[1].size    = mRealBoxSizeBuffer.GetSize();

    bindings[2].binding = 2;
    bindings[2].buffer  = mSPHParamsBuffer;
    bindings[2].offset  = 0;
    bindings[2].size    = mSPHParamsBuffer.GetSize();

    wgpu::BindGroupDescriptor bindGroupDesc {
        .label      = WebGPUUtils::GenerateString("integrate bind group"),
        .layout     = mIntegrateBindGroupLayout,
        .entryCount = static_cast<uint32_t>(bindings.size()),
        .entries    = bindings.data(),
    };
    mIntegrateBindGroup = mDevice.CreateBindGroup(&bindGroupDesc);
}

void SPHSimulator::ComputeIntegrate(wgpu::ComputePassEncoder& computePass)
{
    computePass.SetBindGroup(0, mIntegrateBindGroup, 0, nullptr);
    computePass.SetPipeline(mIntegratePipeline);
    computePass.DispatchWorkgroups(std::ceil(mNumParticles / 64.0f));
}

void SPHSimulator::InitializeCopyPositionPipeline()
{
    wgpu::ShaderModule copyPositionModule =
        ResourceManager::LoadShaderModule("resources/shader/sph/copyPosition.wgsl", mDevice);

    // Create bind group entry
    std::vector<wgpu::BindGroupLayoutEntry> bindingLayoutEentries(3);
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

void SPHSimulator::InitializeCopyPositionBindGroups(wgpu::Buffer particleBuffer,
                                                    wgpu::Buffer posvelBuffer)
{
    std::vector<wgpu::BindGroupEntry> bindings(3);

    bindings[0].binding = 0;
    bindings[0].buffer  = particleBuffer;
    bindings[0].offset  = 0;
    bindings[0].size    = particleBuffer.GetSize();

    bindings[1].binding = 1;
    bindings[1].buffer  = posvelBuffer;
    bindings[1].offset  = 0;
    bindings[1].size    = posvelBuffer.GetSize();

    bindings[2].binding = 2;
    bindings[2].buffer  = mSPHParamsBuffer;
    bindings[2].offset  = 0;
    bindings[2].size    = mSPHParamsBuffer.GetSize();

    wgpu::BindGroupDescriptor bindGroupDesc {
        .label      = WebGPUUtils::GenerateString("copy position bind group"),
        .layout     = mCopyPositionBindGroupLayout,
        .entryCount = static_cast<uint32_t>(bindings.size()),
        .entries    = bindings.data(),
    };
    mCopyPositionBindGroup = mDevice.CreateBindGroup(&bindGroupDesc);
}

void SPHSimulator::ComputeCopyPosition(wgpu::ComputePassEncoder& computePass)
{
    computePass.SetBindGroup(0, mCopyPositionBindGroup, 0, nullptr);
    computePass.SetPipeline(mCopyPositionPipeline);
    computePass.DispatchWorkgroups(std::ceil(mNumParticles / 64.0f));
}

std::vector<SPHParticle> SPHSimulator::InitializeDamBreak(const glm::vec3& initHalfBoxSize,
                                                          int numParticles)
{
    std::vector<SPHParticle> particles(numParticles);
    mNumParticles           = 0;
    const float DIST_FACTOR = 0.5;

    for (float y = -initHalfBoxSize[1] * 0.95f; mNumParticles < numParticles;
         y += DIST_FACTOR * mKernelRadius)
    {
        for (float x = -0.95f * initHalfBoxSize[0];
             x < 0.95f * initHalfBoxSize[0] && mNumParticles < numParticles;
             x += DIST_FACTOR * mKernelRadius)
        {
            for (float z = -0.95f * initHalfBoxSize[2]; z < 0.0f && mNumParticles < numParticles;
                 z += DIST_FACTOR * mKernelRadius)
            {
                float jitter = 0.001f * Random();
                SPHParticle particle {
                    .position    = glm::vec3(x + jitter, y + jitter, z + jitter),
                    .v           = glm::vec3(0.0f),
                    .force       = glm::vec3(0.0f),
                    .density     = 0.0f,
                    .nearDensity = 0.0f,
                };
                particles[mNumParticles] = particle;
                mNumParticles++;
            }
        }
    }

    return particles;
}

float SPHSimulator::Random()
{
    static std::random_device device;
    static std::mt19937_64 generator(device());
    static std::uniform_real_distribution<float> distribution(0.0f, 1.0f);
    return distribution(generator);
}
