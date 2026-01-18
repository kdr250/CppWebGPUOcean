#include "FluidRenderer.h"

#include "WebGPUUtils.h"
#include "ResourceManager.h"

FluidRenderer::FluidRenderer(wgpu::Device device,
                             int width,
                             int height,
                             wgpu::TextureFormat presentationFormat,
                             wgpu::Buffer renderUniformBuffer) : mDevice(device)
{
    // shader module
    wgpu::ShaderModule testModule =
        ResourceManager::LoadShaderModule("resources/shader/test.wgsl", mDevice);

    // Create bind group entry
    wgpu::BindGroupLayoutEntry fluidBindingLayoutEentry {};
    WebGPUUtils::SetDefaultBindGroupLayout(fluidBindingLayoutEentry);
    fluidBindingLayoutEentry.binding     = 0;
    fluidBindingLayoutEentry.visibility  = wgpu::ShaderStage::Vertex | wgpu::ShaderStage::Fragment;
    fluidBindingLayoutEentry.buffer.type = wgpu::BufferBindingType::Uniform;
    fluidBindingLayoutEentry.buffer.minBindingSize = 4 * sizeof(float);

    // Create a bind group layout
    wgpu::BindGroupLayoutDescriptor bindGroupLayoutDesc {};
    bindGroupLayoutDesc.entryCount = 1;
    bindGroupLayoutDesc.entries    = &fluidBindingLayoutEentry;
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
                .module        = testModule,
                .entryPoint    = "vs_main",
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
        .module        = testModule,
        .entryPoint    = "fs_main",
        .constantCount = 0,
        .constants     = nullptr,
        .targetCount   = 1,
        .targets       = &colorTarget,
    };

    renderPipelineDesc.fragment = &fragmentState;

    mFluidPipeline = mDevice.CreateRenderPipeline(&renderPipelineDesc);

    // Create a binding
    wgpu::BindGroupEntry fluidBinding {
        .binding = 0,
        .buffer  = renderUniformBuffer,
        .offset  = 0,
        .size    = 4 * sizeof(float),
    };

    wgpu::BindGroupDescriptor fluidBindGroupDesc {
        .label      = WebGPUUtils::GenerateString("fluid bind group"),
        .layout     = mFluidBindGroupLayout,
        .entryCount = 1,
        .entries    = &fluidBinding,
    };
    mFluidBindGroup = mDevice.CreateBindGroup(&fluidBindGroupDesc);
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
    renderPass.Draw(3, 1, 0, 0);

    renderPass.End();
}
