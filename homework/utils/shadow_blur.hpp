#pragma once

#include <math/aliases.hpp>
#include <webgpu.h>

#include <filesystem>

class ShadowBlur {
public:
    ShadowBlur(WGPUDevice device, const std::filesystem::path& shaderPath);
    ~ShadowBlur();

    void render(
        WGPUCommandEncoder encoder,
        WGPUTextureView source,
        WGPUTextureView temporary
    );

private:
    void renderPass(
        WGPUCommandEncoder encoder,
        WGPUTextureView source,
        WGPUTextureView destination,
        math::vector2f axis
    );

private:
    WGPUDevice m_device;
    WGPUBindGroupLayout m_bindGroupLayout;
    WGPUShaderModule m_shader;
    WGPURenderPipeline m_pipeline;
};
