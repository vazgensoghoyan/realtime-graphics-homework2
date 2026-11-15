#include "point_light.hpp"

#include <cmath>

namespace {

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

} // namespace

PointLight::PointLight(
    WGPUDevice device,
    math::vector3f intensity,
    std::uint32_t shadowSize
)
    : m_position(math::vector3f::zero())
    , m_intensity(intensity)
    , m_shadowTexture(createShadowTexture(device, shadowSize))
    , m_shadowView(createShadowView(m_shadowTexture))
    , m_shadowFaceViews(createShadowFaceViews(m_shadowTexture))
    , m_shadowSampler(createShadowSampler(device))
{}

PointLight::~PointLight() {
    wgpuSamplerRelease(m_shadowSampler);
    for (WGPUTextureView view : m_shadowFaceViews) {
        wgpuTextureViewRelease(view);
    }
    wgpuTextureViewRelease(m_shadowView);
    wgpuTextureRelease(m_shadowTexture);
}

void PointLight::update(float time) {
    m_position = {
        0.3f * std::cos(time * 0.4f),
        -0.1f + 0.15f * std::sin(time * 0.7f),
        0.3f * std::sin(time * 0.4f),
    };
}
