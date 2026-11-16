#include "point_light.hpp"

#include <cmath>
#include <numbers>

#include <math/affine.hpp>
#include <math/projection.hpp>
#include <utils.hpp>

namespace {

struct Immediates {
    math::matrix4f modelTranspose;
    math::matrix4f shadowProjectionTranspose;
};

struct PointLightUniforms {
    math::vector3f position;
    float farPlane;
};

WGPUBuffer createUniformBuffer(WGPUDevice device) {
    WGPUBufferDescriptor descriptor = WGPU_BUFFER_DESCRIPTOR_INIT;
    descriptor.size = sizeof(PointLightUniforms);
    descriptor.usage = WGPUBufferUsage_Uniform | WGPUBufferUsage_CopyDst;
    return wgpuDeviceCreateBuffer(device, &descriptor);
}

WGPUBindGroupLayout createBindGroupLayout(WGPUDevice device) {
    WGPUBindGroupLayoutEntry entry = WGPU_BIND_GROUP_LAYOUT_ENTRY_INIT;
    entry.binding = 0;
    entry.visibility = WGPUShaderStage_Fragment;
    entry.buffer.type = WGPUBufferBindingType_Uniform;
    entry.buffer.minBindingSize = sizeof(PointLightUniforms);

    WGPUBindGroupLayoutDescriptor descriptor = WGPU_BIND_GROUP_LAYOUT_DESCRIPTOR_INIT;
    descriptor.entryCount = 1;
    descriptor.entries = &entry;
    return wgpuDeviceCreateBindGroupLayout(device, &descriptor);
}

WGPUBindGroup createBindGroup(
    WGPUDevice device,
    WGPUBindGroupLayout layout,
    WGPUBuffer buffer
) {
    WGPUBindGroupEntry entry = WGPU_BIND_GROUP_ENTRY_INIT;
    entry.binding = 0;
    entry.buffer = buffer;
    entry.size = sizeof(PointLightUniforms);

    WGPUBindGroupDescriptor descriptor = WGPU_BIND_GROUP_DESCRIPTOR_INIT;
    descriptor.layout = layout;
    descriptor.entryCount = 1;
    descriptor.entries = &entry;
    return wgpuDeviceCreateBindGroup(device, &descriptor);
}

WGPUTexture createShadowTexture(WGPUDevice device, std::uint32_t size) {
    WGPUTextureDescriptor descriptor = WGPU_TEXTURE_DESCRIPTOR_INIT;
    descriptor.usage = WGPUTextureUsage_RenderAttachment | WGPUTextureUsage_TextureBinding;
    descriptor.dimension = WGPUTextureDimension_2D;
    descriptor.size = {size, size, 6};
    descriptor.format = WGPUTextureFormat_Depth24Plus;
    return wgpuDeviceCreateTexture(device, &descriptor);
}

WGPUTextureView createShadowView(WGPUTexture texture) {
    WGPUTextureViewDescriptor descriptor = WGPU_TEXTURE_VIEW_DESCRIPTOR_INIT;
    descriptor.format = WGPUTextureFormat_Depth24Plus;
    descriptor.dimension = WGPUTextureViewDimension_Cube;
    descriptor.mipLevelCount = 1;
    descriptor.arrayLayerCount = 6;
    descriptor.aspect = WGPUTextureAspect_DepthOnly;
    return wgpuTextureCreateView(texture, &descriptor);
}

std::array<WGPUTextureView, 6> createShadowFaceViews(WGPUTexture texture) {
    std::array<WGPUTextureView, 6> result;
    for (std::uint32_t face = 0; face < result.size(); ++face) {
        WGPUTextureViewDescriptor descriptor = WGPU_TEXTURE_VIEW_DESCRIPTOR_INIT;
        descriptor.format = WGPUTextureFormat_Depth24Plus;
        descriptor.dimension = WGPUTextureViewDimension_2D;
        descriptor.baseArrayLayer = face;
        descriptor.arrayLayerCount = 1;
        descriptor.mipLevelCount = 1;
        descriptor.aspect = WGPUTextureAspect_DepthOnly;
        result[face] = wgpuTextureCreateView(texture, &descriptor);
    }
    return result;
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

WGPURenderPipeline createShadowPipeline(WGPUDevice device, WGPUShaderModule shader, WGPUBindGroupLayout uniformLayout, WGPUBindGroupLayout materialLayout) {
    WGPUBindGroupLayout layouts[] = {uniformLayout, materialLayout};

    WGPUPipelineLayoutDescriptor layoutDescriptor = WGPU_PIPELINE_LAYOUT_DESCRIPTOR_INIT;
    layoutDescriptor.immediateSize = sizeof(Immediates);
    layoutDescriptor.bindGroupLayoutCount = std::size(layouts);
    layoutDescriptor.bindGroupLayouts = layouts;
    WGPUPipelineLayout layout = wgpuDeviceCreatePipelineLayout(device, &layoutDescriptor);

    WGPUVertexAttribute attributes[2] = {WGPU_VERTEX_ATTRIBUTE_INIT, WGPU_VERTEX_ATTRIBUTE_INIT};
    attributes[0].offset = offsetof(ObjVertex, position);
    attributes[0].shaderLocation = 0;
    attributes[0].format = WGPUVertexFormat_Float32x3;
    attributes[1].offset = offsetof(ObjVertex, texcoord);
    attributes[1].shaderLocation = 2;
    attributes[1].format = WGPUVertexFormat_Float32x2;

    WGPUVertexBufferLayout vertexLayout = WGPU_VERTEX_BUFFER_LAYOUT_INIT;
    vertexLayout.arrayStride = sizeof(ObjVertex);
    vertexLayout.stepMode = WGPUVertexStepMode_Vertex;
    vertexLayout.attributeCount = std::size(attributes);
    vertexLayout.attributes = attributes;

    WGPUFragmentState fragmentState = WGPU_FRAGMENT_STATE_INIT;
    fragmentState.module = shader;
    fragmentState.entryPoint = {"fragmentMain", WGPU_STRLEN};

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
    descriptor.primitive.cullMode = WGPUCullMode_None;
    descriptor.fragment = &fragmentState;
    descriptor.depthStencil = &depthState;

    WGPURenderPipeline result = wgpuDeviceCreateRenderPipeline(device, &descriptor);
    wgpuPipelineLayoutRelease(layout);
    return result;
}

} // namespace

PointLight::PointLight(
    WGPUDevice device,
    const std::filesystem::path& shaderPath,
    WGPUBindGroupLayout materialLayout,
    math::vector3f intensity,
    std::uint32_t shadowSize
)
    : m_position(math::vector3f::zero())
    , m_intensity(intensity)
    , m_shadowTexture(createShadowTexture(device, shadowSize))
    , m_shadowView(createShadowView(m_shadowTexture))
    , m_shadowFaceViews(createShadowFaceViews(m_shadowTexture))
    , m_shadowSampler(createShadowSampler(device))
    , m_uniformBuffer(createUniformBuffer(device))
    , m_shadowBindGroupLayout(createBindGroupLayout(device))
    , m_shadowBindGroup(createBindGroup(device, m_shadowBindGroupLayout, m_uniformBuffer))
    , m_shadowShader(createShaderModule(device, shaderPath))
    , m_shadowPipeline(createShadowPipeline(device, m_shadowShader, m_shadowBindGroupLayout, materialLayout))
{}

PointLight::~PointLight() {
    wgpuRenderPipelineRelease(m_shadowPipeline);
    wgpuShaderModuleRelease(m_shadowShader);
    wgpuBindGroupRelease(m_shadowBindGroup);
    wgpuBindGroupLayoutRelease(m_shadowBindGroupLayout);
    wgpuBufferRelease(m_uniformBuffer);
    wgpuSamplerRelease(m_shadowSampler);
    for (WGPUTextureView view : m_shadowFaceViews) {
        wgpuTextureViewRelease(view);
    }
    wgpuTextureViewRelease(m_shadowView);
    wgpuTextureRelease(m_shadowTexture);
}

void PointLight::update(WGPUQueue queue, float time) {
    m_position = {
        0.3f * std::cos(time * 0.4f),
        -0.1f + 0.15f * std::sin(time * 0.7f),
        0.3f * std::sin(time * 0.4f),
    };

    const math::matrix4f projection = math::perspective(
        std::numbers::pi_v<float> / 2.f,
        1.f,
        nearPlane,
        farrPlane
    );

    constexpr std::array directions {
        math::vector3f{1.f, 0.f, 0.f},
        math::vector3f{-1.f, 0.f, 0.f},
        math::vector3f{0.f, 1.f, 0.f},
        math::vector3f{0.f, -1.f, 0.f},
        math::vector3f{0.f, 0.f, 1.f},
        math::vector3f{0.f, 0.f, -1.f},
    };

    constexpr std::array rightDirections {
        math::vector3f{0.f, 0.f, -1.f},
        math::vector3f{0.f, 0.f, 1.f},
        math::vector3f{1.f, 0.f, 0.f},
        math::vector3f{1.f, 0.f, 0.f},
        math::vector3f{1.f, 0.f, 0.f},
        math::vector3f{-1.f, 0.f, 0.f},
    };

    constexpr std::array upDirections {
        math::vector3f{0.f, 1.f, 0.f},
        math::vector3f{0.f, 1.f, 0.f},
        math::vector3f{0.f, 0.f, -1.f},
        math::vector3f{0.f, 0.f, 1.f},
        math::vector3f{0.f, 1.f, 0.f},
        math::vector3f{0.f, 1.f, 0.f},
    };

    for (std::uint32_t face = 0; face < 6; ++face) {
        const math::matrix4f rotation = math::matrix4f::from_rows(
            math::append_zero(rightDirections[face]),
            math::append_zero(upDirections[face]),
            math::append_zero(-directions[face]),
            math::append_one(math::vector3f::zero())
        );

        m_shadowMatrices[face] = projection * rotation * math::translation(-m_position);
    }

    PointLightUniforms const uniforms{
        .position = m_position,
        .farPlane = farrPlane,
    };
    wgpuQueueWriteBuffer(queue, m_uniformBuffer, 0, &uniforms, sizeof(uniforms));
}

void PointLight::render(
    WGPUCommandEncoder encoder,
    WGPUBuffer vertexBuffer,
    WGPUBuffer indexBuffer,
    const std::vector<ObjSubmesh>& submeshes,
    const std::vector<WGPUBindGroup>& materialBindGroups,
    math::matrix4f model
) {
    for (std::uint32_t face = 0; face < 6; ++face) {
        WGPURenderPassDepthStencilAttachment depth = WGPU_RENDER_PASS_DEPTH_STENCIL_ATTACHMENT_INIT;
        depth.view = m_shadowFaceViews[face];
        depth.depthLoadOp = WGPULoadOp_Clear;
        depth.depthStoreOp = WGPUStoreOp_Store;
        depth.depthClearValue = 1.f;

        WGPURenderPassDescriptor descriptor = WGPU_RENDER_PASS_DESCRIPTOR_INIT;
        descriptor.depthStencilAttachment = &depth;

        WGPURenderPassEncoder pass = wgpuCommandEncoderBeginRenderPass(encoder, &descriptor);

        wgpuRenderPassEncoderSetPipeline(pass, m_shadowPipeline);

        wgpuRenderPassEncoderSetBindGroup(pass, 0, m_shadowBindGroup, 0, nullptr);
        wgpuRenderPassEncoderSetVertexBuffer(pass, 0, vertexBuffer, 0, WGPU_WHOLE_SIZE);
        wgpuRenderPassEncoderSetIndexBuffer(pass, indexBuffer, WGPUIndexFormat_Uint32, 0, WGPU_WHOLE_SIZE);

        const Immediates immediates {
            .modelTranspose = math::transpose(model),
            .shadowProjectionTranspose = math::transpose(m_shadowMatrices[face]),
        };
        wgpuRenderPassEncoderSetImmediates(pass, 0, &immediates, sizeof(Immediates));

        for (const auto& submesh : submeshes) {
            wgpuRenderPassEncoderSetBindGroup(pass, 1, materialBindGroups[submesh.materialIndex], 0, nullptr);
            wgpuRenderPassEncoderDrawIndexed(pass, submesh.indexCount, 1, submesh.firstIndex, 0, 0);
        }

        wgpuRenderPassEncoderEnd(pass);
        wgpuRenderPassEncoderRelease(pass);
    }
}
