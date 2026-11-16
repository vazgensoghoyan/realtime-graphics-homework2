#include "shadow_blur.hpp"

#include <utils.hpp>

namespace {

struct BlurImmediates {
    math::vector2f axis;
};

WGPUBindGroupLayout createBindGroupLayout(WGPUDevice device) {
    WGPUBindGroupLayoutEntry entry = WGPU_BIND_GROUP_LAYOUT_ENTRY_INIT;
    entry.binding = 0;
    entry.visibility = WGPUShaderStage_Fragment;
    entry.texture.sampleType = WGPUTextureSampleType_UnfilterableFloat;
    entry.texture.viewDimension = WGPUTextureViewDimension_2D;

    WGPUBindGroupLayoutDescriptor descriptor = WGPU_BIND_GROUP_LAYOUT_DESCRIPTOR_INIT;
    descriptor.entryCount = 1;
    descriptor.entries = &entry;
    return wgpuDeviceCreateBindGroupLayout(device, &descriptor);
}

WGPURenderPipeline createPipeline(
    WGPUDevice device,
    WGPUShaderModule shader,
    WGPUBindGroupLayout bindGroupLayout
) {
    WGPUPipelineLayoutDescriptor layoutDescriptor = WGPU_PIPELINE_LAYOUT_DESCRIPTOR_INIT;
    layoutDescriptor.immediateSize = sizeof(BlurImmediates);
    layoutDescriptor.bindGroupLayoutCount = 1;
    layoutDescriptor.bindGroupLayouts = &bindGroupLayout;
    WGPUPipelineLayout layout = wgpuDeviceCreatePipelineLayout(device, &layoutDescriptor);

    WGPUColorTargetState colorTarget = WGPU_COLOR_TARGET_STATE_INIT;
    colorTarget.format = WGPUTextureFormat_RG32Float;
    colorTarget.writeMask = WGPUColorWriteMask_All;

    WGPUFragmentState fragment = WGPU_FRAGMENT_STATE_INIT;
    fragment.module = shader;
    fragment.entryPoint = {"fragmentMain", WGPU_STRLEN};
    fragment.targetCount = 1;
    fragment.targets = &colorTarget;

    WGPURenderPipelineDescriptor descriptor = WGPU_RENDER_PIPELINE_DESCRIPTOR_INIT;
    descriptor.layout = layout;
    descriptor.vertex.module = shader;
    descriptor.vertex.entryPoint = {"vertexMain", WGPU_STRLEN};
    descriptor.primitive.topology = WGPUPrimitiveTopology_TriangleList;
    descriptor.primitive.cullMode = WGPUCullMode_None;
    descriptor.fragment = &fragment;

    WGPURenderPipeline result = wgpuDeviceCreateRenderPipeline(device, &descriptor);
    wgpuPipelineLayoutRelease(layout);
    return result;
}

} // namespace

ShadowBlur::ShadowBlur(WGPUDevice device, const std::filesystem::path& shaderPath)
    : m_device(device)
    , m_bindGroupLayout(createBindGroupLayout(device))
    , m_shader(createShaderModule(device, shaderPath))
    , m_pipeline(createPipeline(device, m_shader, m_bindGroupLayout))
{}

ShadowBlur::~ShadowBlur() {
    wgpuRenderPipelineRelease(m_pipeline);
    wgpuShaderModuleRelease(m_shader);
    wgpuBindGroupLayoutRelease(m_bindGroupLayout);
}

void ShadowBlur::render(
    WGPUCommandEncoder encoder,
    WGPUTextureView source,
    WGPUTextureView temporary
) {
    renderPass(encoder, source, temporary, {1.f, 0.f});
    renderPass(encoder, temporary, source, {0.f, 1.f});
}

void ShadowBlur::renderPass(
    WGPUCommandEncoder encoder,
    WGPUTextureView source,
    WGPUTextureView destination,
    math::vector2f axis
) {
    WGPUBindGroupEntry entry = WGPU_BIND_GROUP_ENTRY_INIT;
    entry.binding = 0;
    entry.textureView = source;

    WGPUBindGroupDescriptor bindGroupDescriptor = WGPU_BIND_GROUP_DESCRIPTOR_INIT;
    bindGroupDescriptor.layout = m_bindGroupLayout;
    bindGroupDescriptor.entryCount = 1;
    bindGroupDescriptor.entries = &entry;
    WGPUBindGroup bindGroup = wgpuDeviceCreateBindGroup(m_device, &bindGroupDescriptor);

    WGPURenderPassColorAttachment color = WGPU_RENDER_PASS_COLOR_ATTACHMENT_INIT;
    color.view = destination;
    color.loadOp = WGPULoadOp_Clear;
    color.storeOp = WGPUStoreOp_Store;
    color.clearValue = {1.f, 1.f, 0.f, 0.f};

    WGPURenderPassDescriptor descriptor = WGPU_RENDER_PASS_DESCRIPTOR_INIT;
    descriptor.colorAttachmentCount = 1;
    descriptor.colorAttachments = &color;

    WGPURenderPassEncoder pass = wgpuCommandEncoderBeginRenderPass(encoder, &descriptor);

    wgpuRenderPassEncoderSetPipeline(pass, m_pipeline);

    wgpuRenderPassEncoderSetBindGroup(pass, 0, bindGroup, 0, nullptr);
    wgpuRenderPassEncoderSetImmediates(pass, 0, &axis, sizeof(axis));

    wgpuRenderPassEncoderDraw(pass, 3, 1, 0, 0);

    wgpuRenderPassEncoderEnd(pass);
    wgpuRenderPassEncoderRelease(pass);

    wgpuBindGroupRelease(bindGroup);
}
