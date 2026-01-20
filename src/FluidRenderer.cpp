#include "FluidRenderer.h"

#include <glm/glm.hpp>

#include "Application.h"
#include "WebGPUUtils.h"
#include "ResourceManager.h"
#include "sph/SPH.h"

FluidRenderer::FluidRenderer(wgpu::Device device,
                             const glm::vec2& screenSize,
                             wgpu::TextureFormat presentationFormat,
                             wgpu::Buffer renderUniformBuffer,
                             wgpu::Buffer posvelBuffer) : mDevice(device)
{
    // pipeline
    InitializeFluidPipelines(presentationFormat);
    InitializeDepthMapPipeline();
    InitializeDepthFilterPipeline();

    // textures
    CreateTextures(screenSize);

    // bind group
    InitializeFluidBindGroups(renderUniformBuffer);
    InitializeDepthMapBindGroups(renderUniformBuffer, posvelBuffer);
}

void FluidRenderer::Draw(wgpu::CommandEncoder& commandEncoder,
                         wgpu::TextureView targetView,
                         uint32_t numParticles)
{
    DrawFluid(commandEncoder, targetView);
    DrawDepthMap(commandEncoder, numParticles);
}

void FluidRenderer::InitializeFluidPipelines(wgpu::TextureFormat presentationFormat)
{
    // shader module
    wgpu::ShaderModule vertexModule =
        ResourceManager::LoadShaderModule("resources/shader/render/fullScreen.wgsl", mDevice);
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
                .topology         = wgpu::PrimitiveTopology::TriangleList,
                .stripIndexFormat = wgpu::IndexFormat::Undefined,  // When Undefined, sequentially
                .frontFace        = wgpu::FrontFace::CCW,
                .cullMode         = wgpu::CullMode::None,
            },
        .depthStencil = nullptr,
        .multisample =
            {
                .count                  = 1,
                .mask                   = ~0u,
                .alphaToCoverageEnabled = false,
            },
    };

    wgpu::BlendState blendState {
        .color =
            {
                .srcFactor = wgpu::BlendFactor::SrcAlpha,
                .dstFactor = wgpu::BlendFactor::OneMinusSrcAlpha,
                .operation = wgpu::BlendOperation::Add,
            },
        .alpha =
            {
                .srcFactor = wgpu::BlendFactor::Zero,
                .dstFactor = wgpu::BlendFactor::One,
                .operation = wgpu::BlendOperation::Add,
            },
    };

    wgpu::ColorTargetState colorTarget {
        .format    = presentationFormat,
        .blend     = &blendState,
        .writeMask = wgpu::ColorWriteMask::All,
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
    samplerDesc.addressModeU  = wgpu::AddressMode::Repeat;
    samplerDesc.addressModeV  = wgpu::AddressMode::Repeat;
    samplerDesc.addressModeW  = wgpu::AddressMode::ClampToEdge;
    samplerDesc.magFilter     = wgpu::FilterMode::Linear;
    samplerDesc.minFilter     = wgpu::FilterMode::Linear;
    samplerDesc.mipmapFilter  = wgpu::MipmapFilterMode::Linear;
    samplerDesc.lodMinClamp   = 0.0f;
    samplerDesc.lodMaxClamp   = 8.0f;
    samplerDesc.compare       = wgpu::CompareFunction::Undefined;
    samplerDesc.maxAnisotropy = 1;
    wgpu::Sampler sampler     = mDevice.CreateSampler(&samplerDesc);

    // FIXME
    wgpu::TextureView textureView {};
    ResourceManager::LoadTexture("resources/texture/test.png", mDevice, &textureView);
    wgpu::TextureView thicknessTextureView {};
    ResourceManager::LoadTexture("resources/texture/test2.png", mDevice, &thicknessTextureView);
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
    bindings[3].textureView = thicknessTextureView;

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

void FluidRenderer::DrawFluid(wgpu::CommandEncoder& commandEncoder, wgpu::TextureView targetView)
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

    // Create the render pass that clears the screen with our color
    wgpu::RenderPassDescriptor renderPassDesc {
        .nextInChain            = nullptr,
        .label                  = WebGPUUtils::GenerateString("fluid render Pass"),
        .colorAttachmentCount   = 1,
        .colorAttachments       = &renderPassColorAttachment,
        .depthStencilAttachment = nullptr,
        .timestampWrites        = nullptr,
    };

    // Create the render pass and end it immediately (we only clear the screen but do not draw anything)
    wgpu::RenderPassEncoder renderPass = commandEncoder.BeginRenderPass(&renderPassDesc);

    // Select which render pipeline to use
    renderPass.SetPipeline(mFluidPipeline);
    renderPass.SetBindGroup(0, mFluidBindGroup, 0, nullptr);
    renderPass.Draw(6, 1, 0, 0);

    renderPass.End();
}

void FluidRenderer::CreateDepthFilterUniform()
{
    // TODO
}

void FluidRenderer::InitializeDepthFilterPipeline()
{
    // shader module
    wgpu::ShaderModule vertexModule =
        ResourceManager::LoadShaderModule("resources/shader/render/fullScreen.wgsl", mDevice);
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
                .bufferCount   = 0,
                .buffers       = nullptr,
                .module        = vertexModule,
                .entryPoint    = "vs",
                .constantCount = 0,
                .constants     = nullptr,
            },
        .primitive =
            {
                .topology         = wgpu::PrimitiveTopology::TriangleList,
                .stripIndexFormat = wgpu::IndexFormat::Undefined,  // When Undefined, sequentially
                .frontFace        = wgpu::FrontFace::CCW,
                .cullMode         = wgpu::CullMode::None,
            },
        .multisample =
            {
                .count                  = 1,
                .mask                   = ~0u,
                .alphaToCoverageEnabled = false,
            },
        .depthStencil = nullptr,
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
    std::vector<wgpu::BindGroupEntry> bindings(2);

    bindings[0].binding = 0;
    bindings[0].buffer  = renderUniformBuffer;
    bindings[0].offset  = 0;
    bindings[0].size    = sizeof(RenderUniforms);

    bindings[1].binding = 1;
    bindings[1].buffer  = mDepthFilterUniformBuffer;
    bindings[1].offset  = 0;
    bindings[1].size    = sizeof(FilterUniform);

    bindings[2].binding     = 2;
    bindings[2].textureView = mDepthMapTextureView;

    wgpu::BindGroupDescriptor bindGroupDesc {
        .label      = WebGPUUtils::GenerateString("depth filterX bind group"),
        .layout     = mDepthMapBindGroupLayout,
        .entryCount = static_cast<uint32_t>(bindings.size()),
        .entries    = bindings.data(),
    };
    mDepthMapBindGroup = mDevice.CreateBindGroup(&bindGroupDesc);
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
    posvelBindingLayout.buffer.minBindingSize = sizeof(PosVel) * NUM_PARTICLES_MAX;

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
                .bufferCount   = 0,
                .buffers       = nullptr,
                .module        = depthMapModule,
                .entryPoint    = "vs",
                .constantCount = 0,
                .constants     = nullptr,
            },
        .primitive =
            {
                .topology         = wgpu::PrimitiveTopology::TriangleList,
                .stripIndexFormat = wgpu::IndexFormat::Undefined,  // When Undefined, sequentially
                .frontFace        = wgpu::FrontFace::CCW,
                .cullMode         = wgpu::CullMode::None,
            },
        .multisample =
            {
                .count                  = 1,
                .mask                   = ~0u,
                .alphaToCoverageEnabled = false,
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
        .label      = WebGPUUtils::GenerateString("fluid bind group"),
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
        .label                  = WebGPUUtils::GenerateString("fluid render Pass"),
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
