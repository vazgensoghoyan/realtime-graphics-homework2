#include "utils.hpp"

#include <file_utils.hpp>

#include <stdexcept>

WGPUShaderModule createShaderModule(
    WGPUDevice device,
    std::filesystem::path const & path
) {
    auto const source = loadFile(path);

    WGPUShaderSourceWGSL shaderSource = WGPU_SHADER_SOURCE_WGSL_INIT;
    shaderSource.code = {source.data(), source.size()};

    WGPUShaderModuleDescriptor descriptor = WGPU_SHADER_MODULE_DESCRIPTOR_INIT;
    descriptor.nextInChain = &shaderSource.chain;
    return wgpuDeviceCreateShaderModule(device, &descriptor);
}

WGPUBuffer createBuffer(
    WGPUDevice device,
    std::size_t size,
    WGPUBufferUsage usage
) {
    WGPUBufferDescriptor descriptor = WGPU_BUFFER_DESCRIPTOR_INIT;
    descriptor.size = size;
    descriptor.usage = usage | WGPUBufferUsage_CopyDst;
    return wgpuDeviceCreateBuffer(device, &descriptor);
}

WGPUTexture createTextureFromImage(
    WGPUDevice device,
    WGPUQueue queue,
    Image const & image
) {
    WGPUTextureDescriptor descriptor = WGPU_TEXTURE_DESCRIPTOR_INIT;
    descriptor.usage = WGPUTextureUsage_CopyDst | WGPUTextureUsage_TextureBinding;
    descriptor.dimension = WGPUTextureDimension_2D;
    descriptor.size = {image.width, image.height, 1};
    descriptor.format = WGPUTextureFormat_RGBA8Unorm;

    WGPUTexture texture = wgpuDeviceCreateTexture(device, &descriptor);

    WGPUTexelCopyTextureInfo destination = WGPU_TEXEL_COPY_TEXTURE_INFO_INIT;
    destination.texture = texture;
    destination.aspect = WGPUTextureAspect_All;

    WGPUTexelCopyBufferLayout layout = WGPU_TEXEL_COPY_BUFFER_LAYOUT_INIT;
    layout.bytesPerRow = image.width * sizeof(Image::Pixel);
    layout.rowsPerImage = image.height;

    WGPUExtent3D extent = WGPU_EXTENT_3D_INIT;
    extent.width = image.width;
    extent.height = image.height;

    wgpuQueueWriteTexture(
        queue,
        &destination,
        image.pixels.get(),
        image.width * image.height * sizeof(Image::Pixel),
        &layout,
        &extent
    );
    return texture;
}

WGPUTexture createTexture(
    WGPUDevice device,
    WGPUQueue queue,
    std::filesystem::path const & path
) {
    Image image = loadImage(path);
    if (!image.pixels || image.width == 0 || image.height == 0) {
        throw std::runtime_error("failed to load texture " + path.string());
    }
    return createTextureFromImage(device, queue, image);
}

WGPUTexture createSolidTexture(
    WGPUDevice device,
    WGPUQueue queue,
    Image::Pixel pixel
) {
    Image image;
    image.width = 1;
    image.height = 1;
    image.pixels = std::make_unique<Image::Pixel[]>(1);
    image.pixels[0] = pixel;
    return createTextureFromImage(device, queue, image);
}

WGPUSampler createSampler(WGPUDevice device) {
    WGPUSamplerDescriptor descriptor = WGPU_SAMPLER_DESCRIPTOR_INIT;
    descriptor.addressModeU = WGPUAddressMode_Repeat;
    descriptor.addressModeV = WGPUAddressMode_Repeat;
    descriptor.addressModeW = WGPUAddressMode_Repeat;
    descriptor.magFilter = WGPUFilterMode_Linear;
    descriptor.minFilter = WGPUFilterMode_Linear;
    descriptor.mipmapFilter = WGPUMipmapFilterMode_Nearest;
    return wgpuDeviceCreateSampler(device, &descriptor);
}
