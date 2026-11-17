#pragma once

#include <math/aliases.hpp>
#include <webgpu.h>

#include <filesystem>

class LightMarker {
public:
    LightMarker(
        WGPUDevice device,
        const std::filesystem::path& shaderPath,
        WGPUTextureFormat surfaceFormat,
        WGPUBindGroupLayout cameraLayout
    );
    ~LightMarker();

    void render(
        WGPURenderPassEncoder pass,
        WGPUBindGroup cameraBindGroup,
        math::vector3f position,
        math::vector3f color,
        float scale
    );

private:
    WGPUShaderModule m_shaderModule;
    WGPURenderPipeline m_pipeline;
};
