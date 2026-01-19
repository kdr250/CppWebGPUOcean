#include "FluidRenderer.h"

#include <glm/glm.hpp>

#include "WebGPUUtils.h"
#include "ResourceManager.h"

FluidRenderer::FluidRenderer(wgpu::Device device,
                             wgpu::TextureFormat presentationFormat,
                             wgpu::Buffer renderUniformBuffer) : mDevice(device)
{
    InitializeFluidPipelines(presentationFormat);
    InitializeFluidBindGroups(renderUniformBuffer);
}

void FluidRenderer::Draw(wgpu::CommandEncoder& commandEncoder, wgpu::TextureView targetView)
{
    // The attachment part of the render pass descriptor describes the target texture of the pass
    wgpu::RenderPassColorAttachment renderPassColorAttachment {
        .view          = targetView,
        .resolveTarget = nullptr,
        .loadOp        = wgpu::LoadOp::Clear,
        .storeOp       = wgpu::StoreOp::Store,
        .clearValue    = wgpu::Color {0.2, 1.0, 0.2, 1.0},
        .depthSlice    = WGPU_DEPTH_SLICE_UNDEFINED,
    };

    // Create the render pass that clears the screen with our color
    wgpu::RenderPassDescriptor renderPassDesc {
        .nextInChain            = nullptr,
        .label                  = WebGPUUtils::GenerateString("Render Pass"),
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
    // Draw 1 instance of a 3-vertices shape
    renderPass.Draw(6, 1, 0, 0);

    renderPass.End();
}

void FluidRenderer::InitializeFluidPipelines(wgpu::TextureFormat presentationFormat)
{
    // shader module
    wgpu::ShaderModule vertexModule =
        ResourceManager::LoadShaderModule("resources/shader/render/fullScreen.wgsl", mDevice);
    wgpu::ShaderModule fluidModule =
        ResourceManager::LoadShaderModule("resources/shader/render/fluid.wgsl", mDevice);

    // Create bind group entry
    std::vector<wgpu::BindGroupLayoutEntry> fluidBindingLayoutEentries(2);
    // The uniform buffer binding
    wgpu::BindGroupLayoutEntry& bindingLayout = fluidBindingLayoutEentries[0];
    WebGPUUtils::SetDefaultBindGroupLayout(bindingLayout);
    WebGPUUtils::SetDefaultBindGroupLayout(bindingLayout);
    bindingLayout.binding               = 0;
    bindingLayout.visibility            = wgpu::ShaderStage::Vertex | wgpu::ShaderStage::Fragment;
    bindingLayout.buffer.type           = wgpu::BufferBindingType::Uniform;
    bindingLayout.buffer.minBindingSize = sizeof(glm::vec2);
    // The texture binding
    wgpu::BindGroupLayoutEntry& textureBindingLayout = fluidBindingLayoutEentries[1];
    WebGPUUtils::SetDefaultBindGroupLayout(textureBindingLayout);
    textureBindingLayout.binding               = 1;
    textureBindingLayout.visibility            = wgpu::ShaderStage::Fragment;
    textureBindingLayout.texture.sampleType    = wgpu::TextureSampleType::Float;
    textureBindingLayout.texture.viewDimension = wgpu::TextureViewDimension::e2D;

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
    // FIXME
    wgpu::TextureView textureView {};
    ResourceManager::LoadTexture("resources/texture/test.png", mDevice, &textureView);

    std::vector<wgpu::BindGroupEntry> bindings(2);

    bindings[0].binding = 0;
    bindings[0].buffer  = renderUniformBuffer;
    bindings[0].offset  = 0;
    bindings[0].size    = sizeof(glm::vec2);

    bindings[1].binding     = 1;
    bindings[1].textureView = textureView;

    wgpu::BindGroupDescriptor fluidBindGroupDesc {
        .label      = WebGPUUtils::GenerateString("fluid bind group"),
        .layout     = mFluidBindGroupLayout,
        .entryCount = static_cast<uint32_t>(bindings.size()),
        .entries    = bindings.data(),
    };
    mFluidBindGroup = mDevice.CreateBindGroup(&fluidBindGroupDesc);
}
