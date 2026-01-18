#include "FluidRenderer.h"

#include "WebGPUUtils.h"
#include "ResourceManager.h"

FluidRenderer::FluidRenderer(wgpu::Device device,
                             int width,
                             int height,
                             wgpu::TextureFormat presentationFormat) : mDevice(device)
{
    // shader module
    wgpu::ShaderModule testModule =
        ResourceManager::LoadShaderModule("resources/shader/test.wgsl", mDevice);

    // pipelines
    wgpu::RenderPipelineDescriptor renderPipelineDesc {
        .label  = WebGPUUtils::GenerateString("fluid rendering pipeline"),
        .layout = nullptr,
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
    // Draw 1 instance of a 3-vertices shape
    renderPass.Draw(3, 1, 0, 0);

    renderPass.End();
}
