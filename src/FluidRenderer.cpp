#include "FluidRenderer.h"

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_wgpu.h>
#include <glm/glm.hpp>

#include "Application.h"
#include "WebGPUUtils.h"
#include "ResourceManager.h"
#include "sph/SPHSimulator.h"

FluidRenderer::FluidRenderer(wgpu::Device device,
                             const glm::vec2& screenSize,
                             wgpu::TextureFormat presentationFormat,
                             float radius,
                             float fov,
                             wgpu::Buffer renderUniformBuffer,
                             wgpu::Buffer posvelBuffer) : mDevice(device)
{
    // buffer & uniform
    float bluredDepthScale = 10.0f;
    float diameter         = 2.0f * radius;
    float blurFilterSize   = 12.0f;

    float depthThreshold = radius * bluredDepthScale;
    float projectedParticleConstant =
        (blurFilterSize * diameter * 0.05f * (screenSize.y / 2.0f)) / std::tan(fov / 2.0f);
    float maxFilterSize = 100.0f;
    CreateDepthFilterUniform(depthThreshold, projectedParticleConstant, maxFilterSize);

    wgpu::ShaderModule vertexModule =
        ResourceManager::LoadShaderModule("resources/shader/render/fullScreen.wgsl", mDevice);

    // pipeline
    InitializeDepthMapPipeline();
    InitializeDepthFilterPipeline(vertexModule);
    InitializeThicknessMapPipeline();
    InitializeThicknessFilterPipeline(vertexModule);
    InitializeFluidPipelines(presentationFormat, vertexModule);
    InitializeSpherePipelines(presentationFormat);

    // textures
    CreateTextures(screenSize);

    // bind group
    InitializeDepthMapBindGroups(renderUniformBuffer, posvelBuffer);
    InitializeDepthFilterBindGroups(renderUniformBuffer);
    InitializeThicknessMapBindGroups(renderUniformBuffer, posvelBuffer);
    InitializeThicknessFilterBindGroups(renderUniformBuffer);
    InitializeFluidBindGroups(renderUniformBuffer);
    InitializeSphereBindGroups(renderUniformBuffer, posvelBuffer);
}

void FluidRenderer::Draw(wgpu::CommandEncoder& commandEncoder,
                         wgpu::TextureView targetView,
                         SimulationVariables& simulationVariables)
{
    if (simulationVariables.drawSpheres)
    {
        DrawSphere(commandEncoder, targetView, simulationVariables);
        return;
    }

    DrawDepthMap(commandEncoder, simulationVariables.numParticles);
    DrawDepthFilter(commandEncoder);
    DrawThicknessMap(commandEncoder, simulationVariables.numParticles);
    DrawThicknessFilter(commandEncoder);
    DrawFluid(commandEncoder, targetView, simulationVariables);
}

void FluidRenderer::InitializeFluidPipelines(wgpu::TextureFormat presentationFormat,
                                             wgpu::ShaderModule vertexModule)
{
    // shader module
    wgpu::ShaderModule fluidModule =
        ResourceManager::LoadShaderModule("resources/shader/render/fluid.wgsl", mDevice);

    // Create bind group entry
    std::vector<wgpu::BindGroupLayoutEntry> fluidBindingLayoutEentries(5);
    // The uniform buffer binding
    wgpu::BindGroupLayoutEntry& bindingLayout = fluidBindingLayoutEentries[0];
    WebGPUUtils::SetDefaultBindGroupLayout(bindingLayout);
    bindingLayout.binding               = 0;
    bindingLayout.visibility            = wgpu::ShaderStage::Vertex | wgpu::ShaderStage::Fragment;
    bindingLayout.buffer.type           = wgpu::BufferBindingType::Uniform;
    bindingLayout.buffer.minBindingSize = sizeof(RenderUniforms);
    // The texture binding
    wgpu::BindGroupLayoutEntry& textureBindingLayout = fluidBindingLayoutEentries[1];
    WebGPUUtils::SetDefaultBindGroupLayout(textureBindingLayout);
    textureBindingLayout.binding               = 1;
    textureBindingLayout.visibility            = wgpu::ShaderStage::Fragment;
    textureBindingLayout.texture.sampleType    = wgpu::TextureSampleType::UnfilterableFloat;
    textureBindingLayout.texture.viewDimension = wgpu::TextureViewDimension::e2D;
    // The sampler binding
    wgpu::BindGroupLayoutEntry& samplerBindingLayout = fluidBindingLayoutEentries[2];
    WebGPUUtils::SetDefaultBindGroupLayout(samplerBindingLayout);
    samplerBindingLayout.binding      = 2;
    samplerBindingLayout.visibility   = wgpu::ShaderStage::Fragment;
    samplerBindingLayout.sampler.type = wgpu::SamplerBindingType::Filtering;
    // The thickness texture binding
    wgpu::BindGroupLayoutEntry& thicknessTextureBindingLayout = fluidBindingLayoutEentries[3];
    WebGPUUtils::SetDefaultBindGroupLayout(thicknessTextureBindingLayout);
    thicknessTextureBindingLayout.binding               = 3;
    thicknessTextureBindingLayout.visibility            = wgpu::ShaderStage::Fragment;
    thicknessTextureBindingLayout.texture.sampleType    = wgpu::TextureSampleType::Float;
    thicknessTextureBindingLayout.texture.viewDimension = wgpu::TextureViewDimension::e2D;
    // The envmap texture binding
    wgpu::BindGroupLayoutEntry& envmapTextureBindingLayout = fluidBindingLayoutEentries[4];
    WebGPUUtils::SetDefaultBindGroupLayout(envmapTextureBindingLayout);
    envmapTextureBindingLayout.binding               = 4;
    envmapTextureBindingLayout.visibility            = wgpu::ShaderStage::Fragment;
    envmapTextureBindingLayout.texture.sampleType    = wgpu::TextureSampleType::Float;
    envmapTextureBindingLayout.texture.viewDimension = wgpu::TextureViewDimension::Cube;

    // Create a bind group layout
    wgpu::BindGroupLayoutDescriptor bindGroupLayoutDesc {};
    bindGroupLayoutDesc.entryCount = static_cast<uint32_t>(fluidBindingLayoutEentries.size());
    bindGroupLayoutDesc.entries    = fluidBindingLayoutEentries.data();
    mFluidBindGroupLayout          = mDevice.CreateBindGroupLayout(&bindGroupLayoutDesc);

    // Create the pipeline layout
    wgpu::PipelineLayoutDescriptor layoutDesc {};
    layoutDesc.bindGroupLayoutCount = 1;
    layoutDesc.bindGroupLayouts     = &mFluidBindGroupLayout;
    mFluidLayout                    = mDevice.CreatePipelineLayout(&layoutDesc);

    // pipelines
    wgpu::RenderPipelineDescriptor renderPipelineDesc {
        .label  = WebGPUUtils::GenerateString("fluid rendering pipeline"),
        .layout = mFluidLayout,
        .vertex =
            {
                .bufferCount   = 0,
                .buffers       = nullptr,
                .module        = vertexModule,
                .entryPoint    = "vs",
                .constantCount = 0,
                .constants     = nullptr,
            },
        .primitive =
            {
                .topology = wgpu::PrimitiveTopology::TriangleList,
            },
    };

    wgpu::DepthStencilState depthStencilState {
        .depthWriteEnabled = true,
        .depthCompare      = wgpu::CompareFunction::Less,
        .format            = wgpu::TextureFormat::Depth32Float,
    };
    renderPipelineDesc.depthStencil = &depthStencilState;

    wgpu::ColorTargetState colorTarget {
        .format = presentationFormat,
    };

    wgpu::FragmentState fragmentState {
        .module        = fluidModule,
        .entryPoint    = "fs",
        .constantCount = 0,
        .constants     = nullptr,
        .targetCount   = 1,
        .targets       = &colorTarget,
    };

    renderPipelineDesc.fragment = &fragmentState;

    mFluidPipeline = mDevice.CreateRenderPipeline(&renderPipelineDesc);
}

void FluidRenderer::InitializeFluidBindGroups(wgpu::Buffer renderUniformBuffer)
{
    // Create a sampler
    wgpu::SamplerDescriptor samplerDesc;
    samplerDesc.magFilter = wgpu::FilterMode::Linear;
    samplerDesc.minFilter = wgpu::FilterMode::Linear;
    wgpu::Sampler sampler = mDevice.CreateSampler(&samplerDesc);

    wgpu::TextureView envmapTextureView {};
    const char* cubemapPaths[] = {"resources/texture/cubemap/posx.png",
                                  "resources/texture/cubemap/negx.png",
                                  "resources/texture/cubemap/posy.png",
                                  "resources/texture/cubemap/negy.png",
                                  "resources/texture/cubemap/posz.png",
                                  "resources/texture/cubemap/negz.png"};
    ResourceManager::LoadCubemapTexture(cubemapPaths, mDevice, &envmapTextureView);

    std::vector<wgpu::BindGroupEntry> bindings(5);

    bindings[0].binding = 0;
    bindings[0].buffer  = renderUniformBuffer;
    bindings[0].offset  = 0;
    bindings[0].size    = sizeof(RenderUniforms);

    bindings[1].binding     = 1;
    bindings[1].textureView = mDepthMapTextureView;

    bindings[2].binding = 2;
    bindings[2].sampler = sampler;

    bindings[3].binding     = 3;
    bindings[3].textureView = mThicknessMapTextureView;

    bindings[4].binding     = 4;
    bindings[4].textureView = envmapTextureView;

    wgpu::BindGroupDescriptor fluidBindGroupDesc {
        .label      = WebGPUUtils::GenerateString("fluid bind group"),
        .layout     = mFluidBindGroupLayout,
        .entryCount = static_cast<uint32_t>(bindings.size()),
        .entries    = bindings.data(),
    };
    mFluidBindGroup = mDevice.CreateBindGroup(&fluidBindGroupDesc);
}

void FluidRenderer::DrawFluid(wgpu::CommandEncoder& commandEncoder,
                              wgpu::TextureView targetView,
                              SimulationVariables& simulationVariables)
{
    // The attachment part of the render pass descriptor describes the target texture of the pass
    wgpu::RenderPassColorAttachment renderPassColorAttachment {
        .view          = targetView,
        .resolveTarget = nullptr,
        .loadOp        = wgpu::LoadOp::Clear,
        .storeOp       = wgpu::StoreOp::Store,
        .clearValue    = wgpu::Color {0.0, 0.0, 0.0, 1.0},
        .depthSlice    = WGPU_DEPTH_SLICE_UNDEFINED,
    };

    wgpu::RenderPassDepthStencilAttachment depthStencilAttachment {
        .view            = mDepthTestTextureView,
        .depthClearValue = 1.0f,
        .depthLoadOp     = wgpu::LoadOp::Clear,
        .depthStoreOp    = wgpu::StoreOp::Store,
    };

    // Create the render pass that clears the screen with our color
    wgpu::RenderPassDescriptor renderPassDesc {
        .nextInChain            = nullptr,
        .label                  = WebGPUUtils::GenerateString("fluid render Pass"),
        .colorAttachmentCount   = 1,
        .colorAttachments       = &renderPassColorAttachment,
        .depthStencilAttachment = &depthStencilAttachment,
        .timestampWrites        = nullptr,
    };

    // Create the render pass and end it immediately (we only clear the screen but do not draw anything)
    wgpu::RenderPassEncoder renderPass = commandEncoder.BeginRenderPass(&renderPassDesc);

    // Select which render pipeline to use
    renderPass.SetPipeline(mFluidPipeline);
    renderPass.SetBindGroup(0, mFluidBindGroup, 0, nullptr);
    renderPass.Draw(6, 1, 0, 0);

    UpdateGUI(renderPass, simulationVariables);

    renderPass.End();
}

void FluidRenderer::CreateDepthFilterUniform(float depthThreshold,
                                             float projectedParticleConstant,
                                             float maxFilterSize)
{
    // create buffer
    wgpu::BufferDescriptor bufferDesc {};
    bufferDesc.label            = WebGPUUtils::GenerateString("filter X unioform buffer");
    bufferDesc.size             = sizeof(FilterUniform);
    bufferDesc.usage            = wgpu::BufferUsage::CopyDst | wgpu::BufferUsage::Uniform;
    bufferDesc.mappedAtCreation = false;

    mFilterXUniformBuffer = mDevice.CreateBuffer(&bufferDesc);

    bufferDesc.label            = WebGPUUtils::GenerateString("filter Y unioform buffer");
    bufferDesc.size             = sizeof(FilterUniform);
    bufferDesc.usage            = wgpu::BufferUsage::CopyDst | wgpu::BufferUsage::Uniform;
    bufferDesc.mappedAtCreation = false;

    mFilterYUniformBuffer = mDevice.CreateBuffer(&bufferDesc);

    // setupt uniform
    mFilterXUniform.blurDir                   = glm::vec2(1.0f, 0.0f);
    mFilterXUniform.depthThreshold            = depthThreshold;
    mFilterXUniform.projectedParticleConstant = projectedParticleConstant;
    mFilterXUniform.maxFilterSize             = maxFilterSize;

    mFilterYUniform.blurDir                   = glm::vec2(0.0f, 1.0f);
    mFilterYUniform.depthThreshold            = depthThreshold;
    mFilterYUniform.projectedParticleConstant = projectedParticleConstant;
    mFilterYUniform.maxFilterSize             = maxFilterSize;

    // write buffer
    wgpu::Queue queue = mDevice.GetQueue();
    queue.WriteBuffer(mFilterXUniformBuffer, 0, &mFilterXUniform, sizeof(FilterUniform));
    queue.WriteBuffer(mFilterYUniformBuffer, 0, &mFilterYUniform, sizeof(FilterUniform));
}

void FluidRenderer::InitializeDepthFilterPipeline(wgpu::ShaderModule vertexModule)
{
    // shader module
    wgpu::ShaderModule depthFilterModule =
        ResourceManager::LoadShaderModule("resources/shader/render/bilateral.wgsl", mDevice);

    // Create bind group entry
    std::vector<wgpu::BindGroupLayoutEntry> bindingLayoutEentries(3);
    // The uniform buffer binding
    wgpu::BindGroupLayoutEntry& bindingLayout = bindingLayoutEentries[0];
    WebGPUUtils::SetDefaultBindGroupLayout(bindingLayout);
    bindingLayout.binding               = 0;
    bindingLayout.visibility            = wgpu::ShaderStage::Vertex;
    bindingLayout.buffer.type           = wgpu::BufferBindingType::Uniform;
    bindingLayout.buffer.minBindingSize = sizeof(RenderUniforms);
    // The filter uniform binding
    wgpu::BindGroupLayoutEntry& filterUniformBindingLayout = bindingLayoutEentries[1];
    WebGPUUtils::SetDefaultBindGroupLayout(filterUniformBindingLayout);
    filterUniformBindingLayout.binding               = 1;
    filterUniformBindingLayout.visibility            = wgpu::ShaderStage::Fragment;
    filterUniformBindingLayout.buffer.type           = wgpu::BufferBindingType::Uniform;
    filterUniformBindingLayout.buffer.minBindingSize = sizeof(FilterUniform);
    // The texture binding
    wgpu::BindGroupLayoutEntry& textureBindingLayout = bindingLayoutEentries[2];
    WebGPUUtils::SetDefaultBindGroupLayout(textureBindingLayout);
    textureBindingLayout.binding               = 2;
    textureBindingLayout.visibility            = wgpu::ShaderStage::Fragment;
    textureBindingLayout.texture.sampleType    = wgpu::TextureSampleType::UnfilterableFloat;
    textureBindingLayout.texture.viewDimension = wgpu::TextureViewDimension::e2D;

    // Create a bind group layout
    wgpu::BindGroupLayoutDescriptor bindGroupLayoutDesc {};
    bindGroupLayoutDesc.entryCount = static_cast<uint32_t>(bindingLayoutEentries.size());
    bindGroupLayoutDesc.entries    = bindingLayoutEentries.data();
    mDepthFilterBindGroupLayout    = mDevice.CreateBindGroupLayout(&bindGroupLayoutDesc);

    // Create the pipeline layout
    wgpu::PipelineLayoutDescriptor layoutDesc {};
    layoutDesc.bindGroupLayoutCount = 1;
    layoutDesc.bindGroupLayouts     = &mDepthFilterBindGroupLayout;
    mDepthFilterLayout              = mDevice.CreatePipelineLayout(&layoutDesc);

    // pipelines
    wgpu::RenderPipelineDescriptor renderPipelineDesc {
        .label  = WebGPUUtils::GenerateString("depth filter rendering pipeline"),
        .layout = mDepthFilterLayout,
        .vertex =
            {
                .module     = vertexModule,
                .entryPoint = "vs",
            },
        .primitive =
            {
                .topology = wgpu::PrimitiveTopology::TriangleList,
            },
    };

    wgpu::ColorTargetState colorTarget {
        .format = wgpu::TextureFormat::R32Float,
    };

    wgpu::FragmentState fragmentState {
        .module        = depthFilterModule,
        .entryPoint    = "fs",
        .constantCount = 0,
        .constants     = nullptr,
        .targetCount   = 1,
        .targets       = &colorTarget,
    };

    renderPipelineDesc.fragment = &fragmentState;

    mDepthFilterPipeline = mDevice.CreateRenderPipeline(&renderPipelineDesc);
}

void FluidRenderer::InitializeDepthFilterBindGroups(wgpu::Buffer renderUniformBuffer)
{
    std::vector<wgpu::BindGroupEntry> bindings(3);

    // filter X
    bindings[0].binding = 0;
    bindings[0].buffer  = renderUniformBuffer;
    bindings[0].offset  = 0;
    bindings[0].size    = sizeof(RenderUniforms);

    bindings[1].binding = 1;
    bindings[1].buffer  = mFilterXUniformBuffer;
    bindings[1].offset  = 0;
    bindings[1].size    = sizeof(FilterUniform);

    bindings[2].binding     = 2;
    bindings[2].textureView = mDepthMapTextureView;

    wgpu::BindGroupDescriptor bindGroupDesc {};
    bindGroupDesc.label       = WebGPUUtils::GenerateString("depth filterX bind group");
    bindGroupDesc.layout      = mDepthFilterBindGroupLayout;
    bindGroupDesc.entryCount  = static_cast<uint32_t>(bindings.size());
    bindGroupDesc.entries     = bindings.data();
    mDepthFilterBindGroups[0] = mDevice.CreateBindGroup(&bindGroupDesc);

    bindings.clear();
    bindings.resize(3);

    // filter Y
    bindings[0].binding = 0;
    bindings[0].buffer  = renderUniformBuffer;
    bindings[0].offset  = 0;
    bindings[0].size    = sizeof(RenderUniforms);

    bindings[1].binding = 1;
    bindings[1].buffer  = mFilterYUniformBuffer;
    bindings[1].offset  = 0;
    bindings[1].size    = sizeof(FilterUniform);

    bindings[2].binding     = 2;
    bindings[2].textureView = mTmpDepthMapTextureView;

    bindGroupDesc.label       = WebGPUUtils::GenerateString("depth filterY bind group");
    bindGroupDesc.layout      = mDepthFilterBindGroupLayout;
    bindGroupDesc.entryCount  = static_cast<uint32_t>(bindings.size());
    bindGroupDesc.entries     = bindings.data();
    mDepthFilterBindGroups[1] = mDevice.CreateBindGroup(&bindGroupDesc);
}

void FluidRenderer::DrawDepthFilter(wgpu::CommandEncoder& commandEncoder)
{
    std::vector<wgpu::RenderPassDescriptor> renderPassDescs(2);

    // filter X
    {
        wgpu::RenderPassColorAttachment renderPassColorAttachment {
            .view          = mTmpDepthMapTextureView,
            .resolveTarget = nullptr,
            .loadOp        = wgpu::LoadOp::Clear,
            .storeOp       = wgpu::StoreOp::Store,
            .clearValue    = wgpu::Color {0.0, 0.0, 0.0, 1.0},
            .depthSlice    = WGPU_DEPTH_SLICE_UNDEFINED,
        };

        wgpu::RenderPassDescriptor renderPassDescriptor {
            .nextInChain            = nullptr,
            .label                  = WebGPUUtils::GenerateString("depth filter X render Pass"),
            .colorAttachmentCount   = 1,
            .colorAttachments       = &renderPassColorAttachment,
            .depthStencilAttachment = nullptr,
            .timestampWrites        = nullptr,
        };

        renderPassDescs[0] = renderPassDescriptor;
    }

    // filter Y
    {
        wgpu::RenderPassColorAttachment renderPassColorAttachment {
            .view          = mDepthMapTextureView,
            .resolveTarget = nullptr,
            .loadOp        = wgpu::LoadOp::Clear,
            .storeOp       = wgpu::StoreOp::Store,
            .clearValue    = wgpu::Color {0.0, 0.0, 0.0, 1.0},
            .depthSlice    = WGPU_DEPTH_SLICE_UNDEFINED,
        };

        wgpu::RenderPassDescriptor renderPassDescriptor {
            .nextInChain            = nullptr,
            .label                  = WebGPUUtils::GenerateString("depth filter Y render Pass"),
            .colorAttachmentCount   = 1,
            .colorAttachments       = &renderPassColorAttachment,
            .depthStencilAttachment = nullptr,
            .timestampWrites        = nullptr,
        };

        renderPassDescs[1] = renderPassDescriptor;
    }

    for (int i = 0; i < 4; ++i)
    {
        auto depthFilterPassEncoderX = commandEncoder.BeginRenderPass(&renderPassDescs[0]);
        depthFilterPassEncoderX.SetBindGroup(0, mDepthFilterBindGroups[0], 0, nullptr);
        depthFilterPassEncoderX.SetPipeline(mDepthFilterPipeline);
        depthFilterPassEncoderX.Draw(6, 1, 0, 0);
        depthFilterPassEncoderX.End();

        auto depthFilterPassEncoderY = commandEncoder.BeginRenderPass(&renderPassDescs[1]);
        depthFilterPassEncoderY.SetBindGroup(0, mDepthFilterBindGroups[1], 0, nullptr);
        depthFilterPassEncoderY.SetPipeline(mDepthFilterPipeline);
        depthFilterPassEncoderY.Draw(6, 1, 0, 0);
        depthFilterPassEncoderY.End();
    }
}

void FluidRenderer::InitializeThicknessMapPipeline()
{
    // shader module
    wgpu::ShaderModule thicknessMapModule =
        ResourceManager::LoadShaderModule("resources/shader/render/thicknessMap.wgsl", mDevice);

    // Create bind group entry
    std::vector<wgpu::BindGroupLayoutEntry> bindingLayoutEentries(2);
    // The uniform buffer binding
    wgpu::BindGroupLayoutEntry& bindingLayout = bindingLayoutEentries[0];
    WebGPUUtils::SetDefaultBindGroupLayout(bindingLayout);
    bindingLayout.binding               = 0;
    bindingLayout.visibility            = wgpu::ShaderStage::Vertex | wgpu::ShaderStage::Fragment;
    bindingLayout.buffer.type           = wgpu::BufferBindingType::Uniform;
    bindingLayout.buffer.minBindingSize = sizeof(RenderUniforms);
    // The positions binding
    wgpu::BindGroupLayoutEntry& posvelBindingLayout = bindingLayoutEentries[1];
    WebGPUUtils::SetDefaultBindGroupLayout(posvelBindingLayout);
    posvelBindingLayout.binding               = 1;
    posvelBindingLayout.visibility            = wgpu::ShaderStage::Vertex;
    posvelBindingLayout.buffer.type           = wgpu::BufferBindingType::ReadOnlyStorage;
    posvelBindingLayout.buffer.minBindingSize = sizeof(PosVel) * NUM_PARTICLES_MAX;

    // Create a bind group layout
    wgpu::BindGroupLayoutDescriptor bindGroupLayoutDesc {};
    bindGroupLayoutDesc.entryCount = static_cast<uint32_t>(bindingLayoutEentries.size());
    bindGroupLayoutDesc.entries    = bindingLayoutEentries.data();
    mThicknessMapBindGroupLayout   = mDevice.CreateBindGroupLayout(&bindGroupLayoutDesc);

    // Create the pipeline layout
    wgpu::PipelineLayoutDescriptor layoutDesc {};
    layoutDesc.bindGroupLayoutCount = 1;
    layoutDesc.bindGroupLayouts     = &mThicknessMapBindGroupLayout;
    mThicknessMapLayout             = mDevice.CreatePipelineLayout(&layoutDesc);

    // pipelines
    wgpu::RenderPipelineDescriptor renderPipelineDesc {
        .label  = WebGPUUtils::GenerateString("thickness map rendering pipeline"),
        .layout = mThicknessMapLayout,
        .vertex =
            {
                .module     = thicknessMapModule,
                .entryPoint = "vs",
            },
        .primitive =
            {
                .topology = wgpu::PrimitiveTopology::TriangleList,
            },
    };

    wgpu::BlendState blendState {
        .color =
            {
                .srcFactor = wgpu::BlendFactor::One,
                .dstFactor = wgpu::BlendFactor::One,
                .operation = wgpu::BlendOperation::Add,
            },
        .alpha =
            {
                .srcFactor = wgpu::BlendFactor::One,
                .dstFactor = wgpu::BlendFactor::One,
                .operation = wgpu::BlendOperation::Add,
            },
    };

    wgpu::ColorTargetState colorTarget {
        .format    = wgpu::TextureFormat::R16Float,
        .writeMask = wgpu::ColorWriteMask::Red,
        .blend     = &blendState,
    };

    wgpu::FragmentState fragmentState {
        .module        = thicknessMapModule,
        .entryPoint    = "fs",
        .constantCount = 0,
        .constants     = nullptr,
        .targetCount   = 1,
        .targets       = &colorTarget,
    };

    renderPipelineDesc.fragment = &fragmentState;

    mThicknessMapPipeline = mDevice.CreateRenderPipeline(&renderPipelineDesc);
}

void FluidRenderer::InitializeThicknessMapBindGroups(wgpu::Buffer renderUniformBuffer,
                                                     wgpu::Buffer posvelBuffer)
{
    std::vector<wgpu::BindGroupEntry> bindings(2);

    bindings[0].binding = 0;
    bindings[0].buffer  = renderUniformBuffer;
    bindings[0].offset  = 0;
    bindings[0].size    = sizeof(RenderUniforms);

    bindings[1].binding = 1;
    bindings[1].buffer  = posvelBuffer;
    bindings[1].offset  = 0;
    bindings[1].size    = posvelBuffer.GetSize();

    wgpu::BindGroupDescriptor bindGroupDesc {
        .label      = WebGPUUtils::GenerateString("fluid bind group"),
        .layout     = mThicknessMapBindGroupLayout,
        .entryCount = static_cast<uint32_t>(bindings.size()),
        .entries    = bindings.data(),
    };
    mThicknessMapBindGroup = mDevice.CreateBindGroup(&bindGroupDesc);
}

void FluidRenderer::DrawThicknessMap(wgpu::CommandEncoder& commandEncoder, uint32_t numParticles)
{
    wgpu::RenderPassColorAttachment renderPassColorAttachment {
        .view          = mThicknessMapTextureView,
        .resolveTarget = nullptr,
        .loadOp        = wgpu::LoadOp::Clear,
        .storeOp       = wgpu::StoreOp::Store,
        .clearValue    = wgpu::Color {0.0, 0.0, 0.0, 1.0},
        .depthSlice    = WGPU_DEPTH_SLICE_UNDEFINED,
    };

    // Create the render pass that clears the screen with our color
    wgpu::RenderPassDescriptor renderPassDesc {
        .nextInChain            = nullptr,
        .label                  = WebGPUUtils::GenerateString("thickness map render Pass"),
        .colorAttachmentCount   = 1,
        .colorAttachments       = &renderPassColorAttachment,
        .depthStencilAttachment = nullptr,
        .timestampWrites        = nullptr,
    };

    // Create the render pass and end it immediately (we only clear the screen but do not draw anything)
    wgpu::RenderPassEncoder renderPass = commandEncoder.BeginRenderPass(&renderPassDesc);

    // Select which render pipeline to use
    renderPass.SetPipeline(mThicknessMapPipeline);
    renderPass.SetBindGroup(0, mThicknessMapBindGroup, 0, nullptr);
    renderPass.Draw(6, numParticles, 0, 0);

    renderPass.End();
}

void FluidRenderer::InitializeThicknessFilterPipeline(wgpu::ShaderModule vertexModule)
{
    // shader module
    wgpu::ShaderModule thicknessFilterModule =
        ResourceManager::LoadShaderModule("resources/shader/render/gaussian.wgsl", mDevice);

    // Create bind group entry
    std::vector<wgpu::BindGroupLayoutEntry> bindingLayoutEentries(3);
    // The uniform buffer binding
    wgpu::BindGroupLayoutEntry& bindingLayout = bindingLayoutEentries[0];
    WebGPUUtils::SetDefaultBindGroupLayout(bindingLayout);
    bindingLayout.binding               = 0;
    bindingLayout.visibility            = wgpu::ShaderStage::Vertex;
    bindingLayout.buffer.type           = wgpu::BufferBindingType::Uniform;
    bindingLayout.buffer.minBindingSize = sizeof(RenderUniforms);
    // The filter uniform binding
    wgpu::BindGroupLayoutEntry& filterUniformBindingLayout = bindingLayoutEentries[1];
    WebGPUUtils::SetDefaultBindGroupLayout(filterUniformBindingLayout);
    filterUniformBindingLayout.binding               = 1;
    filterUniformBindingLayout.visibility            = wgpu::ShaderStage::Fragment;
    filterUniformBindingLayout.buffer.type           = wgpu::BufferBindingType::Uniform;
    filterUniformBindingLayout.buffer.minBindingSize = sizeof(FilterUniform);
    // The texture binding
    wgpu::BindGroupLayoutEntry& textureBindingLayout = bindingLayoutEentries[2];
    WebGPUUtils::SetDefaultBindGroupLayout(textureBindingLayout);
    textureBindingLayout.binding               = 2;
    textureBindingLayout.visibility            = wgpu::ShaderStage::Fragment;
    textureBindingLayout.texture.sampleType    = wgpu::TextureSampleType::UnfilterableFloat;
    textureBindingLayout.texture.viewDimension = wgpu::TextureViewDimension::e2D;

    // Create a bind group layout
    wgpu::BindGroupLayoutDescriptor bindGroupLayoutDesc {};
    bindGroupLayoutDesc.entryCount  = static_cast<uint32_t>(bindingLayoutEentries.size());
    bindGroupLayoutDesc.entries     = bindingLayoutEentries.data();
    mThicknessFilterBindGroupLayout = mDevice.CreateBindGroupLayout(&bindGroupLayoutDesc);

    // Create the pipeline layout
    wgpu::PipelineLayoutDescriptor layoutDesc {};
    layoutDesc.bindGroupLayoutCount = 1;
    layoutDesc.bindGroupLayouts     = &mThicknessFilterBindGroupLayout;
    mThicknessFilterLayout          = mDevice.CreatePipelineLayout(&layoutDesc);

    // pipelines
    wgpu::RenderPipelineDescriptor renderPipelineDesc {
        .label  = WebGPUUtils::GenerateString("thickness filter rendering pipeline"),
        .layout = mThicknessFilterLayout,
        .vertex =
            {
                .module     = vertexModule,
                .entryPoint = "vs",
            },
        .primitive =
            {
                .topology = wgpu::PrimitiveTopology::TriangleList,
            },
    };

    wgpu::ColorTargetState colorTarget {
        .format = wgpu::TextureFormat::R16Float,
    };

    wgpu::FragmentState fragmentState {
        .module        = thicknessFilterModule,
        .entryPoint    = "fs",
        .constantCount = 0,
        .constants     = nullptr,
        .targetCount   = 1,
        .targets       = &colorTarget,
    };

    renderPipelineDesc.fragment = &fragmentState;

    mThicknessFilterPipeline = mDevice.CreateRenderPipeline(&renderPipelineDesc);
}

void FluidRenderer::InitializeThicknessFilterBindGroups(wgpu::Buffer renderUniformBuffer)
{
    std::vector<wgpu::BindGroupEntry> bindings(3);

    // filter X
    bindings[0].binding = 0;
    bindings[0].buffer  = renderUniformBuffer;
    bindings[0].offset  = 0;
    bindings[0].size    = sizeof(RenderUniforms);

    bindings[1].binding = 1;
    bindings[1].buffer  = mFilterXUniformBuffer;
    bindings[1].offset  = 0;
    bindings[1].size    = sizeof(FilterUniform);

    bindings[2].binding     = 2;
    bindings[2].textureView = mThicknessMapTextureView;

    wgpu::BindGroupDescriptor bindGroupDesc {};
    bindGroupDesc.label           = WebGPUUtils::GenerateString("depth filterX bind group");
    bindGroupDesc.layout          = mThicknessFilterBindGroupLayout;
    bindGroupDesc.entryCount      = static_cast<uint32_t>(bindings.size());
    bindGroupDesc.entries         = bindings.data();
    mThicknessFilterBindGroups[0] = mDevice.CreateBindGroup(&bindGroupDesc);

    bindings.clear();
    bindings.resize(3);

    // filter Y
    bindings[0].binding = 0;
    bindings[0].buffer  = renderUniformBuffer;
    bindings[0].offset  = 0;
    bindings[0].size    = sizeof(RenderUniforms);

    bindings[1].binding = 1;
    bindings[1].buffer  = mFilterYUniformBuffer;
    bindings[1].offset  = 0;
    bindings[1].size    = sizeof(FilterUniform);

    bindings[2].binding     = 2;
    bindings[2].textureView = mTmpThicknessMapTextureView;

    bindGroupDesc.label           = WebGPUUtils::GenerateString("depth filterY bind group");
    bindGroupDesc.layout          = mThicknessFilterBindGroupLayout;
    bindGroupDesc.entryCount      = static_cast<uint32_t>(bindings.size());
    bindGroupDesc.entries         = bindings.data();
    mThicknessFilterBindGroups[1] = mDevice.CreateBindGroup(&bindGroupDesc);
}

void FluidRenderer::DrawThicknessFilter(wgpu::CommandEncoder& commandEncoder)
{
    std::vector<wgpu::RenderPassDescriptor> renderPassDescs(2);

    // filter X
    {
        wgpu::RenderPassColorAttachment renderPassColorAttachment {
            .view          = mTmpThicknessMapTextureView,
            .resolveTarget = nullptr,
            .loadOp        = wgpu::LoadOp::Clear,
            .storeOp       = wgpu::StoreOp::Store,
            .clearValue    = wgpu::Color {0.0, 0.0, 0.0, 1.0},
            .depthSlice    = WGPU_DEPTH_SLICE_UNDEFINED,
        };

        wgpu::RenderPassDescriptor renderPassDescriptor {
            .nextInChain            = nullptr,
            .label                  = WebGPUUtils::GenerateString("thickness filter X render Pass"),
            .colorAttachmentCount   = 1,
            .colorAttachments       = &renderPassColorAttachment,
            .depthStencilAttachment = nullptr,
            .timestampWrites        = nullptr,
        };

        renderPassDescs[0] = renderPassDescriptor;
    }

    // filter Y
    {
        wgpu::RenderPassColorAttachment renderPassColorAttachment {
            .view          = mThicknessMapTextureView,
            .resolveTarget = nullptr,
            .loadOp        = wgpu::LoadOp::Clear,
            .storeOp       = wgpu::StoreOp::Store,
            .clearValue    = wgpu::Color {0.0, 0.0, 0.0, 1.0},
            .depthSlice    = WGPU_DEPTH_SLICE_UNDEFINED,
        };

        wgpu::RenderPassDescriptor renderPassDescriptor {
            .nextInChain            = nullptr,
            .label                  = WebGPUUtils::GenerateString("thickness filter Y render Pass"),
            .colorAttachmentCount   = 1,
            .colorAttachments       = &renderPassColorAttachment,
            .depthStencilAttachment = nullptr,
            .timestampWrites        = nullptr,
        };

        renderPassDescs[1] = renderPassDescriptor;
    }

    for (int i = 0; i < 1; ++i)
    {
        auto thicknessFilterPassEncoderX = commandEncoder.BeginRenderPass(&renderPassDescs[0]);
        thicknessFilterPassEncoderX.SetBindGroup(0, mThicknessFilterBindGroups[0], 0, nullptr);
        thicknessFilterPassEncoderX.SetPipeline(mThicknessFilterPipeline);
        thicknessFilterPassEncoderX.Draw(6, 1, 0, 0);
        thicknessFilterPassEncoderX.End();

        auto thicknessFilterPassEncoderY = commandEncoder.BeginRenderPass(&renderPassDescs[1]);
        thicknessFilterPassEncoderY.SetBindGroup(0, mThicknessFilterBindGroups[1], 0, nullptr);
        thicknessFilterPassEncoderY.SetPipeline(mThicknessFilterPipeline);
        thicknessFilterPassEncoderY.Draw(6, 1, 0, 0);
        thicknessFilterPassEncoderY.End();
    }
}

void FluidRenderer::InitializeSpherePipelines(wgpu::TextureFormat presentationFormat)
{
    // shader module
    wgpu::ShaderModule sphereModule =
        ResourceManager::LoadShaderModule("resources/shader/render/sphere.wgsl", mDevice);

    // Create bind group entry
    std::vector<wgpu::BindGroupLayoutEntry> bindingLayoutEentries(2);
    // The uniform buffer binding
    wgpu::BindGroupLayoutEntry& bindingLayout = bindingLayoutEentries[0];
    WebGPUUtils::SetDefaultBindGroupLayout(bindingLayout);
    bindingLayout.binding               = 0;
    bindingLayout.visibility            = wgpu::ShaderStage::Vertex | wgpu::ShaderStage::Fragment;
    bindingLayout.buffer.type           = wgpu::BufferBindingType::Uniform;
    bindingLayout.buffer.minBindingSize = sizeof(RenderUniforms);
    // The positions binding
    wgpu::BindGroupLayoutEntry& posvelBindingLayout = bindingLayoutEentries[1];
    WebGPUUtils::SetDefaultBindGroupLayout(posvelBindingLayout);
    posvelBindingLayout.binding               = 1;
    posvelBindingLayout.visibility            = wgpu::ShaderStage::Vertex;
    posvelBindingLayout.buffer.type           = wgpu::BufferBindingType::ReadOnlyStorage;
    posvelBindingLayout.buffer.minBindingSize = sizeof(PosVel) * NUM_PARTICLES_MIN;

    // Create a bind group layout
    wgpu::BindGroupLayoutDescriptor bindGroupLayoutDesc {};
    bindGroupLayoutDesc.entryCount = static_cast<uint32_t>(bindingLayoutEentries.size());
    bindGroupLayoutDesc.entries    = bindingLayoutEentries.data();
    mSphereBindGroupLayout         = mDevice.CreateBindGroupLayout(&bindGroupLayoutDesc);

    // Create the pipeline layout
    wgpu::PipelineLayoutDescriptor layoutDesc {};
    layoutDesc.bindGroupLayoutCount = 1;
    layoutDesc.bindGroupLayouts     = &mSphereBindGroupLayout;
    mSphereLayout                   = mDevice.CreatePipelineLayout(&layoutDesc);

    // pipelines
    wgpu::RenderPipelineDescriptor renderPipelineDesc {
        .label  = WebGPUUtils::GenerateString("sphere pipeline"),
        .layout = mSphereLayout,
        .vertex =
            {
                .module     = sphereModule,
                .entryPoint = "vs",
            },
        .primitive =
            {
                .topology = wgpu::PrimitiveTopology::TriangleList,
            },
    };

    wgpu::DepthStencilState depthStencilState {
        .depthWriteEnabled = true,
        .depthCompare      = wgpu::CompareFunction::Less,
        .format            = wgpu::TextureFormat::Depth32Float,
    };
    renderPipelineDesc.depthStencil = &depthStencilState;

    wgpu::ColorTargetState colorTarget {
        .format = presentationFormat,
    };

    wgpu::FragmentState fragmentState {
        .module        = sphereModule,
        .entryPoint    = "fs",
        .constantCount = 0,
        .constants     = nullptr,
        .targetCount   = 1,
        .targets       = &colorTarget,
    };

    renderPipelineDesc.fragment = &fragmentState;

    mSpherePipeline = mDevice.CreateRenderPipeline(&renderPipelineDesc);
}

void FluidRenderer::InitializeSphereBindGroups(wgpu::Buffer renderUniformBuffer,
                                               wgpu::Buffer posvelBuffer)
{
    std::vector<wgpu::BindGroupEntry> bindings(2);

    bindings[0].binding = 0;
    bindings[0].buffer  = renderUniformBuffer;
    bindings[0].offset  = 0;
    bindings[0].size    = sizeof(RenderUniforms);

    bindings[1].binding = 1;
    bindings[1].buffer  = posvelBuffer;
    bindings[1].offset  = 0;
    bindings[1].size    = posvelBuffer.GetSize();

    wgpu::BindGroupDescriptor bindGroupDesc {
        .label      = WebGPUUtils::GenerateString("sphere bind group"),
        .layout     = mSphereBindGroupLayout,
        .entryCount = static_cast<uint32_t>(bindings.size()),
        .entries    = bindings.data(),
    };
    mSphereBindGroup = mDevice.CreateBindGroup(&bindGroupDesc);
}

void FluidRenderer::DrawSphere(wgpu::CommandEncoder& commandEncoder,
                               wgpu::TextureView targetView,
                               SimulationVariables& simulationVariables)
{
    // The attachment part of the render pass descriptor describes the target texture of the pass
    wgpu::RenderPassColorAttachment renderPassColorAttachment {
        .view          = targetView,
        .resolveTarget = nullptr,
        .loadOp        = wgpu::LoadOp::Clear,
        .storeOp       = wgpu::StoreOp::Store,
        .clearValue    = wgpu::Color {0.8, 0.8, 0.8, 1.0},
        .depthSlice    = WGPU_DEPTH_SLICE_UNDEFINED,
    };

    wgpu::RenderPassDepthStencilAttachment depthStencilAttachment {
        .view            = mDepthTestTextureView,
        .depthClearValue = 1.0f,
        .depthLoadOp     = wgpu::LoadOp::Clear,
        .depthStoreOp    = wgpu::StoreOp::Store,
    };

    // Create the render pass that clears the screen with our color
    wgpu::RenderPassDescriptor renderPassDesc {
        .nextInChain            = nullptr,
        .label                  = WebGPUUtils::GenerateString("sphere render Pass"),
        .colorAttachmentCount   = 1,
        .colorAttachments       = &renderPassColorAttachment,
        .depthStencilAttachment = &depthStencilAttachment,
        .timestampWrites        = nullptr,
    };

    // Create the render pass and end it immediately (we only clear the screen but do not draw anything)
    wgpu::RenderPassEncoder renderPass = commandEncoder.BeginRenderPass(&renderPassDesc);

    // Select which render pipeline to use
    renderPass.SetPipeline(mSpherePipeline);
    renderPass.SetBindGroup(0, mSphereBindGroup, 0, nullptr);
    renderPass.Draw(6, simulationVariables.numParticles, 0, 0);

    UpdateGUI(renderPass, simulationVariables);

    renderPass.End();
}

void FluidRenderer::CreateTextures(const glm::vec2& textureSize)
{
    wgpu::Extent3D size = {(unsigned int)textureSize.x, (unsigned int)textureSize.y, 1};
    wgpu::TextureDescriptor textureDesc {};

    // depth map texture
    textureDesc.label  = WebGPUUtils::GenerateString("depth map texture");
    textureDesc.size   = size;
    textureDesc.usage  = wgpu::TextureUsage::RenderAttachment | wgpu::TextureUsage::TextureBinding;
    textureDesc.format = wgpu::TextureFormat::R32Float;

    wgpu::Texture depthMapTexture = mDevice.CreateTexture(&textureDesc);
    mDepthMapTextureView          = depthMapTexture.CreateView();

    // temporary depth map texture
    textureDesc.label  = WebGPUUtils::GenerateString("temporary depth map texture");
    textureDesc.size   = size;
    textureDesc.usage  = wgpu::TextureUsage::RenderAttachment | wgpu::TextureUsage::TextureBinding;
    textureDesc.format = wgpu::TextureFormat::R32Float;

    wgpu::Texture tmpDepthMapTexture = mDevice.CreateTexture(&textureDesc);
    mTmpDepthMapTextureView          = tmpDepthMapTexture.CreateView();

    // depth test texture
    textureDesc.label  = WebGPUUtils::GenerateString("depth test texture");
    textureDesc.size   = size;
    textureDesc.usage  = wgpu::TextureUsage::RenderAttachment | wgpu::TextureUsage::TextureBinding;
    textureDesc.format = wgpu::TextureFormat::Depth32Float;

    wgpu::Texture depthTestTexture = mDevice.CreateTexture(&textureDesc);
    mDepthTestTextureView          = depthTestTexture.CreateView();

    // thickness map texture
    textureDesc.label  = WebGPUUtils::GenerateString("thickness map texture");
    textureDesc.size   = size;
    textureDesc.usage  = wgpu::TextureUsage::RenderAttachment | wgpu::TextureUsage::TextureBinding;
    textureDesc.format = wgpu::TextureFormat::R16Float;

    wgpu::Texture thicknessMapTexture = mDevice.CreateTexture(&textureDesc);
    mThicknessMapTextureView          = thicknessMapTexture.CreateView();

    // temporary thickness map texture
    textureDesc.label  = WebGPUUtils::GenerateString("temporary thickness map texture");
    textureDesc.size   = size;
    textureDesc.usage  = wgpu::TextureUsage::RenderAttachment | wgpu::TextureUsage::TextureBinding;
    textureDesc.format = wgpu::TextureFormat::R16Float;

    wgpu::Texture temporaryThicknessMapTexture = mDevice.CreateTexture(&textureDesc);
    mTmpThicknessMapTextureView                = temporaryThicknessMapTexture.CreateView();
}

void FluidRenderer::UpdateGUI(wgpu::RenderPassEncoder& renderPass,
                              SimulationVariables& simulationVariables)
{
    // Start the Dear ImGui frame
    ImGui_ImplWGPU_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    // Build UI
    {
        ImGui::Begin("Fluid Simulation");

        bool simChnaged = false;
        simChnaged      = ImGui::RadioButton("SPH", &simulationVariables.sph, 1) || simChnaged;
        simChnaged      = ImGui::RadioButton("MLS-MPM", &simulationVariables.sph, 0) || simChnaged;

        simulationVariables.simulationChnaged = simChnaged;

        ImGui::Separator();

        bool changed = false;
        changed = ImGui::Checkbox("Draw Particles", &simulationVariables.drawSpheres) || changed;

        ImGui::Separator();

        ImGui::Text("Number of Particles");
        if (simulationVariables.sph)
        {
            changed =
                ImGui::RadioButton("10,000", &simulationVariables.numParticles, 10000) || changed;
            changed =
                ImGui::RadioButton("20,000", &simulationVariables.numParticles, 20000) || changed;
            changed =
                ImGui::RadioButton("30,000", &simulationVariables.numParticles, 30000) || changed;
            changed =
                ImGui::RadioButton("40,000", &simulationVariables.numParticles, 40000) || changed;
        }
        else
        {
            changed =
                ImGui::RadioButton("40,000", &simulationVariables.numParticles, 40000) || changed;
            changed =
                ImGui::RadioButton("70,000", &simulationVariables.numParticles, 70000) || changed;
            changed =
                ImGui::RadioButton("120,000", &simulationVariables.numParticles, 120000) || changed;
            changed =
                ImGui::RadioButton("200,000", &simulationVariables.numParticles, 200000) || changed;
        }

        ImGui::Separator();

        bool boxChanged = false;
        boxChanged = ImGui::SliderFloat("Box width", &simulationVariables.boxWidthRatio, 0.5f, 1.0f)
                     || boxChanged;

        simulationVariables.changed        = changed;
        simulationVariables.boxSizeChanged = boxChanged;

        ImGui::End();
    }

    // Draw UI
    ImGui::EndFrame();
    ImGui::Render();

    auto drawData              = ImGui::GetDrawData();
    drawData->FramebufferScale = ImVec2(1.0f, 1.0f);

    ImGui_ImplWGPU_RenderDrawData(drawData, renderPass.Get());
}

void FluidRenderer::InitializeDepthMapPipeline()
{
    // shader module
    wgpu::ShaderModule depthMapModule =
        ResourceManager::LoadShaderModule("resources/shader/render/depthMap.wgsl", mDevice);

    // Create bind group entry
    std::vector<wgpu::BindGroupLayoutEntry> bindingLayoutEentries(2);
    // The uniform buffer binding
    wgpu::BindGroupLayoutEntry& bindingLayout = bindingLayoutEentries[0];
    WebGPUUtils::SetDefaultBindGroupLayout(bindingLayout);
    bindingLayout.binding               = 0;
    bindingLayout.visibility            = wgpu::ShaderStage::Vertex | wgpu::ShaderStage::Fragment;
    bindingLayout.buffer.type           = wgpu::BufferBindingType::Uniform;
    bindingLayout.buffer.minBindingSize = sizeof(RenderUniforms);
    // The positions binding
    wgpu::BindGroupLayoutEntry& posvelBindingLayout = bindingLayoutEentries[1];
    WebGPUUtils::SetDefaultBindGroupLayout(posvelBindingLayout);
    posvelBindingLayout.binding               = 1;
    posvelBindingLayout.visibility            = wgpu::ShaderStage::Vertex;
    posvelBindingLayout.buffer.type           = wgpu::BufferBindingType::ReadOnlyStorage;
    posvelBindingLayout.buffer.minBindingSize = sizeof(PosVel) * NUM_PARTICLES_MIN;

    // Create a bind group layout
    wgpu::BindGroupLayoutDescriptor bindGroupLayoutDesc {};
    bindGroupLayoutDesc.entryCount = static_cast<uint32_t>(bindingLayoutEentries.size());
    bindGroupLayoutDesc.entries    = bindingLayoutEentries.data();
    mDepthMapBindGroupLayout       = mDevice.CreateBindGroupLayout(&bindGroupLayoutDesc);

    // Create the pipeline layout
    wgpu::PipelineLayoutDescriptor layoutDesc {};
    layoutDesc.bindGroupLayoutCount = 1;
    layoutDesc.bindGroupLayouts     = &mDepthMapBindGroupLayout;
    mDepthMapLayout                 = mDevice.CreatePipelineLayout(&layoutDesc);

    // pipelines
    wgpu::RenderPipelineDescriptor renderPipelineDesc {
        .label  = WebGPUUtils::GenerateString("depth map rendering pipeline"),
        .layout = mDepthMapLayout,
        .vertex =
            {
                .module     = depthMapModule,
                .entryPoint = "vs",
            },
        .primitive =
            {
                .topology = wgpu::PrimitiveTopology::TriangleList,
            },
    };

    wgpu::DepthStencilState depthStencilState {
        .depthWriteEnabled = true,
        .depthCompare      = wgpu::CompareFunction::Less,
        .format            = wgpu::TextureFormat::Depth32Float,
    };
    renderPipelineDesc.depthStencil = &depthStencilState;

    wgpu::ColorTargetState colorTarget {
        .format = wgpu::TextureFormat::R32Float,
    };

    wgpu::FragmentState fragmentState {
        .module        = depthMapModule,
        .entryPoint    = "fs",
        .constantCount = 0,
        .constants     = nullptr,
        .targetCount   = 1,
        .targets       = &colorTarget,
    };

    renderPipelineDesc.fragment = &fragmentState;

    mDepthMapPipeline = mDevice.CreateRenderPipeline(&renderPipelineDesc);
}

void FluidRenderer::InitializeDepthMapBindGroups(wgpu::Buffer renderUniformBuffer,
                                                 wgpu::Buffer posvelBuffer)
{
    std::vector<wgpu::BindGroupEntry> bindings(2);

    bindings[0].binding = 0;
    bindings[0].buffer  = renderUniformBuffer;
    bindings[0].offset  = 0;
    bindings[0].size    = sizeof(RenderUniforms);

    bindings[1].binding = 1;
    bindings[1].buffer  = posvelBuffer;
    bindings[1].offset  = 0;
    bindings[1].size    = posvelBuffer.GetSize();

    wgpu::BindGroupDescriptor bindGroupDesc {
        .label      = WebGPUUtils::GenerateString("depth map bind group"),
        .layout     = mDepthMapBindGroupLayout,
        .entryCount = static_cast<uint32_t>(bindings.size()),
        .entries    = bindings.data(),
    };
    mDepthMapBindGroup = mDevice.CreateBindGroup(&bindGroupDesc);
}

void FluidRenderer::DrawDepthMap(wgpu::CommandEncoder& commandEncoder, uint32_t numParticles)
{
    // The attachment part of the render pass descriptor describes the target texture of the pass
    wgpu::RenderPassColorAttachment renderPassColorAttachment {
        .view          = mDepthMapTextureView,
        .resolveTarget = nullptr,
        .loadOp        = wgpu::LoadOp::Clear,
        .storeOp       = wgpu::StoreOp::Store,
        .clearValue    = wgpu::Color {0.0, 0.0, 0.0, 1.0},
        .depthSlice    = WGPU_DEPTH_SLICE_UNDEFINED,
    };

    wgpu::RenderPassDepthStencilAttachment depthStencilAttachment {
        .view            = mDepthTestTextureView,
        .depthClearValue = 1.0f,
        .depthLoadOp     = wgpu::LoadOp::Clear,
        .depthStoreOp    = wgpu::StoreOp::Store,
    };

    // Create the render pass that clears the screen with our color
    wgpu::RenderPassDescriptor renderPassDesc {
        .nextInChain            = nullptr,
        .label                  = WebGPUUtils::GenerateString("depth map render Pass"),
        .colorAttachmentCount   = 1,
        .colorAttachments       = &renderPassColorAttachment,
        .depthStencilAttachment = &depthStencilAttachment,
        .timestampWrites        = nullptr,
    };

    // Create the render pass and end it immediately (we only clear the screen but do not draw anything)
    wgpu::RenderPassEncoder renderPass = commandEncoder.BeginRenderPass(&renderPassDesc);

    // Select which render pipeline to use
    renderPass.SetPipeline(mDepthMapPipeline);
    renderPass.SetBindGroup(0, mDepthMapBindGroup, 0, nullptr);
    renderPass.Draw(6, numParticles, 0, 0);

    renderPass.End();
}
