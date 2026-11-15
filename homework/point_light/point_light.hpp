#pragma once

#include <math/aliases.hpp>
#include <webgpu.h>

#include <array>
#include <cstdint>

class PointLight {
public:
    PointLight(
        WGPUDevice device,
        math::vector3f intensity = {0.4f, 0.2f, 0.1f},
        std::uint32_t shadowSize = 1024
    );
    ~PointLight();

    void update(float time);

    math::vector3f position() const { return m_position; }
    math::vector3f intensity() const { return m_intensity; }

    WGPUTextureView shadowView() const { return m_shadowView; }
    WGPUTextureView shadowFaceView(std::size_t face) const { return m_shadowFaceViews[face]; }
    WGPUSampler shadowSampler() const { return m_shadowSampler; }

private:
    math::vector3f m_position;
    math::vector3f m_intensity;

    WGPUTexture m_shadowTexture;
    WGPUTextureView m_shadowView;
    std::array<WGPUTextureView, 6> m_shadowFaceViews;
    WGPUSampler m_shadowSampler;
};
