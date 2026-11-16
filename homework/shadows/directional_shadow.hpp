#pragma once

#include <math/aliases.hpp>
#include <obj_loader.hpp>
#include <webgpu.h>

#include <filesystem>
#include <array>

class DirectionalShadow {
public:
    DirectionalShadow(
        WGPUDevice device,
        const std::filesystem::path& shaderPath,
        WGPUBindGroupLayout materialLayout,
        std::uint32_t size
    );
    ~DirectionalShadow();

    void update(
        math::vector3f lightDirection,
        math::matrix4f model,
        math::box3f sceneBounds,
        const std::array<math::vector3f, 8>& cameraFrustumCorners
    );

    void render(
        WGPUCommandEncoder encoder,
        WGPUBuffer vertexBuffer,
        WGPUBuffer indexBuffer,
        const std::vector<ObjSubmesh>& submeshes,
        const std::vector<WGPUBindGroup>& materialBindGroups
    );

    WGPUTextureView view() const { return m_view; }
    WGPUSampler sampler() const { return m_sampler; }
    math::matrix4f projection() const { return m_projection; }

private:
    WGPUTexture m_texture;
    WGPUTextureView m_view;
    WGPUSampler m_sampler;

    WGPUShaderModule m_shaderModule;
    WGPURenderPipeline m_pipeline;

    math::matrix4f m_model;
    math::matrix4f m_projection;
};
