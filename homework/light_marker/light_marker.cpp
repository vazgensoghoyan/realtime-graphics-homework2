#include "light_marker.hpp"

#include <utils.hpp>

namespace {

struct Immediates {
    math::vector3f position;
    float padding = 0.f;
    math::vector3f color;
    float scale;
};

WGPURenderPipeline createPipeline(
    WGPUDevice device,
    WGPUShaderModule shader,
    WGPUTextureFormat surfaceFormat,
    WGPUBindGroupLayout cameraLayout
) {
    WGPUPipelineLayoutDescriptor layoutDescriptor = WGPU_PIPELINE_LAYOUT_DESCRIPTOR_INIT;
    layoutDescriptor.immediateSize = sizeof(Immediates);
    layoutDescriptor.bindGroupLayoutCount = 1;
    layoutDescriptor.bindGroupLayouts = &cameraLayout;

    WGPUPipelineLayout layout = wgpuDeviceCreatePipelineLayout(device, &layoutDescriptor);

    WGPUColorTargetState colorTarget = WGPU_COLOR_TARGET_STATE_INIT;
    colorTarget.format = surfaceFormat;
    colorTarget.writeMask = WGPUColorWriteMask_All;

    WGPUFragmentState fragmentState = WGPU_FRAGMENT_STATE_INIT;
    fragmentState.module = shader;
    fragmentState.entryPoint = {"fragmentMain", WGPU_STRLEN};
    fragmentState.targetCount = 1;
    fragmentState.targets = &colorTarget;

    WGPUDepthStencilState depthState = WGPU_DEPTH_STENCIL_STATE_INIT;
    depthState.format = WGPUTextureFormat_Depth24Plus;
    depthState.depthWriteEnabled = WGPUOptionalBool_True;
    depthState.depthCompare = WGPUCompareFunction_Less;

    WGPURenderPipelineDescriptor descriptor = WGPU_RENDER_PIPELINE_DESCRIPTOR_INIT;
    descriptor.layout = layout;
    descriptor.vertex.module = shader;
    descriptor.vertex.entryPoint = {"vertexMain", WGPU_STRLEN};
    descriptor.primitive.topology = WGPUPrimitiveTopology_LineList;
    descriptor.fragment = &fragmentState;
    descriptor.depthStencil = &depthState;

    WGPURenderPipeline result = wgpuDeviceCreateRenderPipeline(device, &descriptor);
    wgpuPipelineLayoutRelease(layout);
    return result;
}

} // namespace

LightMarker::LightMarker(
    WGPUDevice device,
    const std::filesystem::path& shaderPath,
    WGPUTextureFormat surfaceFormat,
    WGPUBindGroupLayout cameraLayout
)
    : m_shaderModule(createShaderModule(device, shaderPath))
    , m_pipeline(createPipeline(device, m_shaderModule, surfaceFormat, cameraLayout))
{}

LightMarker::~LightMarker() {
    wgpuRenderPipelineRelease(m_pipeline);
    wgpuShaderModuleRelease(m_shaderModule);
}

void LightMarker::render(
    WGPURenderPassEncoder pass,
    WGPUBindGroup cameraBindGroup,
    math::vector3f position,
    math::vector3f color,
    float scale
) {
    const Immediates immediates{
        .position = position,
        .color = color,
        .scale = scale,
    };

    wgpuRenderPassEncoderSetPipeline(pass, m_pipeline);

    wgpuRenderPassEncoderSetBindGroup(pass, 0, cameraBindGroup, 0, nullptr);
    wgpuRenderPassEncoderSetImmediates(pass, 0, &immediates, sizeof(immediates));

    wgpuRenderPassEncoderDraw(pass, 6, 1, 0, 0);
}
