#pragma once

#include <math/aliases.hpp>
#include <obj_loader.hpp>
#include <webgpu.h>

#include <array>
#include <cstdint>
#include <filesystem>

class PointLight {
public:
    PointLight(
        WGPUDevice device,
        const std::filesystem::path& shaderPath,
        WGPUBindGroupLayout materialLayout,
        math::vector3f intensity,
        std::uint32_t shadowSize
    );
    ~PointLight();

    void update(WGPUQueue queue, float time);

    void render(
        WGPUCommandEncoder encoder,
        WGPUBuffer vertexBuffer,
        WGPUBuffer indexBuffer,
        const std::vector<ObjSubmesh>& submeshes,
        const std::vector<WGPUBindGroup>& materialBindGroups,
        math::matrix4f model
    );

    math::vector3f position() const { return m_position; }
    math::vector3f intensity() const { return m_intensity; }

    WGPUTextureView view() const { return m_shadowView; }
    WGPUSampler sampler() const { return m_shadowSampler; }
    float farPlane() const { return farrPlane; }

private:
    static constexpr float nearPlane = 0.01f;
    static constexpr float farrPlane = 4.f;

    math::vector3f m_position;
    math::vector3f m_intensity;

    WGPUTexture m_shadowTexture;
    WGPUTextureView m_shadowView;

    std::array<WGPUTextureView, 6> m_shadowFaceViews;
    std::array<math::matrix4f, 6> m_shadowMatrices;

    WGPUSampler m_shadowSampler;

    WGPUBuffer m_uniformBuffer;

    WGPUBindGroupLayout m_shadowBindGroupLayout;
    WGPUBindGroup m_shadowBindGroup;

    WGPUShaderModule m_shadowShader;
    WGPURenderPipeline m_shadowPipeline;
};
