#pragma once

#include <image_utils.hpp>
#include <webgpu.h>

#include <filesystem>
#include <vector>

WGPUShaderModule createShaderModule(
    WGPUDevice device,
    std::filesystem::path const & path
);

WGPUBuffer createBuffer(
    WGPUDevice device,
    std::size_t size,
    WGPUBufferUsage usage
);

template <typename T>
WGPUBuffer loadBuffer(
    WGPUDevice device,
    WGPUQueue queue,
    std::vector<T> const & data,
    WGPUBufferUsage usage
) {
    WGPUBuffer buffer = createBuffer(device, data.size() * sizeof(T), usage);
    wgpuQueueWriteBuffer(queue, buffer, 0, data.data(), data.size() * sizeof(T));
    return buffer;
}

WGPUTexture createTextureFromImage(
    WGPUDevice device,
    WGPUQueue queue,
    Image const & image
);

WGPUTexture createTexture(
    WGPUDevice device,
    WGPUQueue queue,
    std::filesystem::path const & path
);

WGPUTexture createSolidTexture(
    WGPUDevice device,
    WGPUQueue queue,
    Image::Pixel pixel
);

WGPUSampler createSampler(WGPUDevice device);
