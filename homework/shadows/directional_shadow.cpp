#include "directional_shadow.hpp"

#include <utils.hpp>

#include <math/affine.hpp>
#include <math/projection.hpp>

namespace {

struct ShadowImmediates {
    math::matrix4f modelTranspose;
    math::matrix4f shadowProjectionTranspose;
};

WGPUTexture createTexture(WGPUDevice device, std::uint32_t size, WGPUTextureFormat format) {
    WGPUTextureDescriptor descriptor = WGPU_TEXTURE_DESCRIPTOR_INIT;
    descriptor.usage = WGPUTextureUsage_RenderAttachment | WGPUTextureUsage_TextureBinding;
    descriptor.dimension = WGPUTextureDimension_2D;
    descriptor.size = {size, size, 1};
    descriptor.format = format;
    return wgpuDeviceCreateTexture(device, &descriptor);
}

WGPUTextureView createView(WGPUTexture texture, WGPUTextureAspect aspect) {
    WGPUTextureViewDescriptor descriptor = WGPU_TEXTURE_VIEW_DESCRIPTOR_INIT;
    descriptor.aspect = aspect;
    return wgpuTextureCreateView(texture, &descriptor);
}

WGPUSampler createShadowSampler(WGPUDevice device) {
    WGPUSamplerDescriptor descriptor = WGPU_SAMPLER_DESCRIPTOR_INIT;
    descriptor.addressModeU = WGPUAddressMode_ClampToEdge;
    descriptor.addressModeV = WGPUAddressMode_ClampToEdge;
    descriptor.addressModeW = WGPUAddressMode_ClampToEdge;
    descriptor.magFilter = WGPUFilterMode_Linear;
    descriptor.minFilter = WGPUFilterMode_Linear;
    descriptor.mipmapFilter = WGPUMipmapFilterMode_Nearest;
    return wgpuDeviceCreateSampler(device, &descriptor);
}

WGPURenderPipeline createPipeline(WGPUDevice device, WGPUShaderModule shader, WGPUBindGroupLayout materialLayout) {
    WGPUPipelineLayoutDescriptor layoutDescriptor = WGPU_PIPELINE_LAYOUT_DESCRIPTOR_INIT;
    layoutDescriptor.immediateSize = sizeof(ShadowImmediates);
    layoutDescriptor.bindGroupLayoutCount = 1;
    layoutDescriptor.bindGroupLayouts = &materialLayout;
    WGPUPipelineLayout layout = wgpuDeviceCreatePipelineLayout(device, &layoutDescriptor);

    WGPUColorTargetState colorTarget = WGPU_COLOR_TARGET_STATE_INIT;
    colorTarget.format = WGPUTextureFormat_RG32Float;
    colorTarget.writeMask = WGPUColorWriteMask_All;

    WGPUFragmentState fragmentState = WGPU_FRAGMENT_STATE_INIT;
    fragmentState.module = shader;
    fragmentState.entryPoint = {"fragmentMain", WGPU_STRLEN};
    fragmentState.targetCount = 1;
    fragmentState.targets = &colorTarget;

    WGPUVertexAttribute attributes[2];

    attributes[0] = WGPU_VERTEX_ATTRIBUTE_INIT;
    attributes[0].offset = offsetof(ObjVertex, position);
    attributes[0].shaderLocation = 0;
    attributes[0].format = WGPUVertexFormat_Float32x3;

    attributes[1] = WGPU_VERTEX_ATTRIBUTE_INIT;
    attributes[1].offset = offsetof(ObjVertex, texcoord);
    attributes[1].shaderLocation = 2;
    attributes[1].format = WGPUVertexFormat_Float32x2;

    WGPUVertexBufferLayout vertexLayout = WGPU_VERTEX_BUFFER_LAYOUT_INIT;
    vertexLayout.arrayStride = sizeof(ObjVertex);
    vertexLayout.stepMode = WGPUVertexStepMode_Vertex;
    vertexLayout.attributeCount = std::size(attributes);
    vertexLayout.attributes = attributes;

    WGPUDepthStencilState depthState = WGPU_DEPTH_STENCIL_STATE_INIT;
    depthState.format = WGPUTextureFormat_Depth24Plus;
    depthState.depthWriteEnabled = WGPUOptionalBool_True;
    depthState.depthCompare = WGPUCompareFunction_Less;

    WGPURenderPipelineDescriptor descriptor = WGPU_RENDER_PIPELINE_DESCRIPTOR_INIT;
    descriptor.layout = layout;
    descriptor.vertex.module = shader;
    descriptor.vertex.entryPoint = {"vertexMain", WGPU_STRLEN};
    descriptor.vertex.bufferCount = 1;
    descriptor.vertex.buffers = &vertexLayout;
    descriptor.primitive.topology = WGPUPrimitiveTopology_TriangleList;
    descriptor.primitive.cullMode = WGPUCullMode_Back;
    descriptor.primitive.unclippedDepth = WGPU_TRUE;
    descriptor.fragment = &fragmentState;
    descriptor.depthStencil = &depthState;

    WGPURenderPipeline result = wgpuDeviceCreateRenderPipeline(device, &descriptor);
    wgpuPipelineLayoutRelease(layout);
    return result;
}

math::box3f transformBounds(math::matrix4f transform, math::box3f bounds) {
    math::box3f result = math::box3f::null();
    for (float x : {bounds.min[0], bounds.max[0]})
    for (float y : {bounds.min[1], bounds.max[1]})
    for (float z : {bounds.min[2], bounds.max[2]}) {
        result |= math::head<3>(transform * math::append_one(math::vector3f{x, y, z}));
    }
    return result;
}

math::matrix4f calculateProjection(
    math::vector3f lightDirection,
    math::box3f worldBounds
) {
    const math::vector3f lightZ = -lightDirection;
    const math::vector3f lightX = math::normalized(
        math::cross(lightZ, math::vector3f{0.f, 1.f, 0.f})
    );
    const math::vector3f lightY = -math::cross(lightX, lightZ);

    const math::matrix4f lightView = math::matrix4f::from_rows(
        math::append_zero(lightX),
        math::append_zero(lightY),
        math::append_zero(lightZ),
        math::append_one(math::vector3f::zero())
    );

    math::box3f lightBounds = transformBounds(lightView, worldBounds);

    constexpr float padding = 0.05f;
    lightBounds.min -= math::vector3f{padding, padding, padding};
    lightBounds.max += math::vector3f{padding, padding, padding};

    return math::orthographic(
        lightBounds.min[0], lightBounds.max[0],
        lightBounds.min[1], lightBounds.max[1],
        lightBounds.min[2], lightBounds.max[2]
    ) * lightView;
}

} // namespace

DirectionalShadow::DirectionalShadow(
    WGPUDevice device,
    const std::filesystem::path& shaderPath,
    const std::filesystem::path& blurShaderPath,
    WGPUBindGroupLayout materialLayout,
    std::uint32_t size
)
    : m_depthTexture(createTexture(device, size, WGPUTextureFormat_Depth24Plus))
    , m_depthView(createView(m_depthTexture, WGPUTextureAspect_DepthOnly))
    , m_momentsTexture(createTexture(device, size, WGPUTextureFormat_RG32Float))
    , m_momentsView(createView(m_momentsTexture, WGPUTextureAspect_All))
    , m_blurTexture(createTexture(device, size, WGPUTextureFormat_RG32Float))
    , m_blurView(createView(m_blurTexture, WGPUTextureAspect_All))
    , m_blur(device, blurShaderPath)
    , m_sampler(createShadowSampler(device))
    , m_shaderModule(createShaderModule(device, shaderPath))
    , m_pipeline(createPipeline(device, m_shaderModule, materialLayout))
{}

DirectionalShadow::~DirectionalShadow() {
    wgpuRenderPipelineRelease(m_pipeline);
    wgpuShaderModuleRelease(m_shaderModule);
    wgpuSamplerRelease(m_sampler);
    wgpuTextureViewRelease(m_momentsView);
    wgpuTextureRelease(m_momentsTexture);
    wgpuTextureViewRelease(m_blurView);
    wgpuTextureRelease(m_blurTexture);
    wgpuTextureViewRelease(m_depthView);
    wgpuTextureRelease(m_depthTexture);
}

void DirectionalShadow::update(
    math::vector3f lightDirection,
    math::matrix4f model,
    math::box3f sceneBounds,
    const std::array<math::vector3f, 8>& cameraFrustumCorners
) {
    auto worldSceneBounds = transformBounds(model, sceneBounds);
    auto frustumBounds = math::box3f::null();

    for (const auto& corner : cameraFrustumCorners) {
        frustumBounds |= corner;
    }

    auto visibleBounds = worldSceneBounds & frustumBounds;

    if (math::empty(visibleBounds)) {
        visibleBounds = worldSceneBounds;
    }

    m_model = model;
    m_projection = calculateProjection(lightDirection, visibleBounds);
}

void DirectionalShadow::render(
    WGPUCommandEncoder encoder,
    WGPUBuffer vertexBuffer,
    WGPUBuffer indexBuffer,
    const std::vector<ObjSubmesh>& submeshes,
    const std::vector<WGPUBindGroup>& materialBindGroups
) {
    WGPURenderPassColorAttachment colorAttachment = WGPU_RENDER_PASS_COLOR_ATTACHMENT_INIT;
    colorAttachment.view = m_momentsView;
    colorAttachment.loadOp = WGPULoadOp_Clear;
    colorAttachment.storeOp = WGPUStoreOp_Store;
    colorAttachment.clearValue = {1.f, 1.f, 0.f, 0.f};

    WGPURenderPassDepthStencilAttachment depthAttachment = WGPU_RENDER_PASS_DEPTH_STENCIL_ATTACHMENT_INIT;
    depthAttachment.view = m_depthView;
    depthAttachment.depthLoadOp = WGPULoadOp_Clear;
    depthAttachment.depthStoreOp = WGPUStoreOp_Store;
    depthAttachment.depthClearValue = 1.f;
    depthAttachment.depthReadOnly = WGPU_FALSE;

    WGPURenderPassDescriptor descriptor = WGPU_RENDER_PASS_DESCRIPTOR_INIT;
    descriptor.depthStencilAttachment = &depthAttachment;
    descriptor.colorAttachmentCount = 1;
    descriptor.colorAttachments = &colorAttachment;

    WGPURenderPassEncoder pass = wgpuCommandEncoderBeginRenderPass(encoder, &descriptor);

    wgpuRenderPassEncoderSetPipeline(pass, m_pipeline);

    wgpuRenderPassEncoderSetVertexBuffer(pass, 0, vertexBuffer, 0, WGPU_WHOLE_SIZE);
    wgpuRenderPassEncoderSetIndexBuffer(pass, indexBuffer, WGPUIndexFormat_Uint32, 0, WGPU_WHOLE_SIZE);

    ShadowImmediates immediates{
        .modelTranspose = math::transpose(m_model),
        .shadowProjectionTranspose = math::transpose(m_projection),
    };
    wgpuRenderPassEncoderSetImmediates(pass, 0, &immediates, sizeof(immediates));

    for (const auto& submesh : submeshes) {
        wgpuRenderPassEncoderSetBindGroup(pass, 0, materialBindGroups[submesh.materialIndex], 0, nullptr);
        wgpuRenderPassEncoderDrawIndexed(pass, submesh.indexCount, 1, submesh.firstIndex, 0, 0);
    }

    wgpuRenderPassEncoderEnd(pass);
    wgpuRenderPassEncoderRelease(pass);

    m_blur.render(
        encoder,
        m_momentsView,
        m_blurView
    );
}
