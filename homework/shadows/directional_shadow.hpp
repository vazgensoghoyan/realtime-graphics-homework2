#pragma once

#include <math/aliases.hpp>
#include <obj_loader.hpp>
#include <webgpu.h>

#include <filesystem>

class DirectionalShadow {
public:
    DirectionalShadow(
        WGPUDevice device,
        const std::filesystem::path& shaderPath,
        std::uint32_t size = 2048
    );
    ~DirectionalShadow();

    void update(
        math::vector3f lightDirection,
        math::matrix4f model,
        math::box3f sceneBounds
    );

    void render(
        WGPUCommandEncoder encoder,
        WGPUBuffer vertexBuffer,
        WGPUBuffer indexBuffer,
        const std::vector<ObjSubmesh>& submeshes
    );

    WGPUTextureView view() const { return m_view; }
    WGPUSampler sampler() const { return m_sampler; }

private:
    WGPUTexture m_texture;
    WGPUTextureView m_view;
    WGPUSampler m_sampler;

    WGPUShaderModule m_shaderModule;
    WGPURenderPipeline m_pipeline;

    math::matrix4f m_model;
    math::matrix4f m_projection;
};
