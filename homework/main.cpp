#include <wgpu_app.hpp>
#include <file_utils.hpp>
#include <image_utils.hpp>
#include <obj_loader.hpp>
#include <math/math.hpp>

#include "camera.hpp"
#include "shadows/directional_shadow.hpp"

#include <webgpu.h>

#include <algorithm>
#include <chrono>
#include <exception>
#include <filesystem>
#include <iostream>

static std::filesystem::path const projectRoot = PROJECT_ROOT;

struct CameraUniforms
{
    math::matrix4f viewProjectionTranspose;
    math::vector3f position;
    float padding = 0.f;
};

struct LightsUniforms
{
    math::matrix4f shadowProjectionTranspose;
    math::vector3f ambientIntensity;
    float padding = 0.f;
    math::vector3f lightDirection;
    float padding2 = 0.f;
    math::vector3f lightIntensity;
    float padding3 = 0.f;
};

struct Immediates
{
    math::matrix4f modelTranspose;
    math::vector3f specular;
    float specularPower;
};

struct DebugDepthTextureImmediates
{
    math::vector2f center;
    math::vector2f extent;
};

struct MaterialGpu {
    WGPUTexture albedoTexture;
    WGPUTextureView albedoTextureView;

    WGPUTexture transparencyTexture;
    WGPUTextureView transparencyTextureView;

    WGPUBindGroup bindGroup;
};

WGPUShaderModule createShaderModule(WGPUDevice device, std::filesystem::path const & path) {
    auto const source = loadFile(path);

    WGPUShaderSourceWGSL shaderSourceWGSL = WGPU_SHADER_SOURCE_WGSL_INIT;
    shaderSourceWGSL.code = {source.data(), source.size()};

    WGPUShaderModuleDescriptor shaderModuleDescriptor = WGPU_SHADER_MODULE_DESCRIPTOR_INIT;
    shaderModuleDescriptor.nextInChain = &shaderSourceWGSL.chain;

    return wgpuDeviceCreateShaderModule(device, &shaderModuleDescriptor);
}

WGPUBuffer createBuffer(WGPUDevice device, std::size_t size, WGPUBufferUsage usage) {
    WGPUBufferDescriptor bufferDescriptor = WGPU_BUFFER_DESCRIPTOR_INIT;
    bufferDescriptor.size = size;
    bufferDescriptor.usage = usage | WGPUBufferUsage_CopyDst;
    return wgpuDeviceCreateBuffer(device, &bufferDescriptor);
}

template <typename T>
WGPUBuffer loadBuffer(WGPUDevice device, WGPUQueue queue, std::vector<T> const & data, WGPUBufferUsage usage) {
    auto buffer = createBuffer(device, data.size() * sizeof(T), usage);
    wgpuQueueWriteBuffer(queue, buffer, 0, data.data(), data.size() * sizeof(T));
    return buffer;
}

WGPUBindGroupLayout createCameraBindGroupLayout(WGPUDevice device) {
    WGPUBindGroupLayoutEntry entries[1] = {WGPU_BIND_GROUP_LAYOUT_ENTRY_INIT};
    entries[0].binding = 0;
    entries[0].visibility = WGPUShaderStage_Vertex | WGPUShaderStage_Fragment;
    entries[0].buffer.type = WGPUBufferBindingType_Uniform;

    WGPUBindGroupLayoutDescriptor bindGroupLayoutDescriptor = WGPU_BIND_GROUP_LAYOUT_DESCRIPTOR_INIT;
    bindGroupLayoutDescriptor.entries = entries;
    bindGroupLayoutDescriptor.entryCount = std::size(entries);

    return wgpuDeviceCreateBindGroupLayout(device, &bindGroupLayoutDescriptor);
}

WGPUBindGroupLayout createLightsBindGroupLayout(WGPUDevice device) {
    WGPUBindGroupLayoutEntry entries[3];

    entries[0] = WGPU_BIND_GROUP_LAYOUT_ENTRY_INIT;
    entries[0].binding = 0;
    entries[0].visibility = WGPUShaderStage_Vertex | WGPUShaderStage_Fragment;
    entries[0].buffer.type = WGPUBufferBindingType_Uniform;

    entries[1] = WGPU_BIND_GROUP_LAYOUT_ENTRY_INIT;
    entries[1].binding = 1;
    entries[1].visibility = WGPUShaderStage_Fragment;
    entries[1].texture.sampleType = WGPUTextureSampleType_Depth;
    entries[1].texture.viewDimension = WGPUTextureViewDimension_2D;

    entries[2] = WGPU_BIND_GROUP_LAYOUT_ENTRY_INIT;
    entries[2].binding = 2;
    entries[2].visibility = WGPUShaderStage_Fragment;
    entries[2].sampler.type = WGPUSamplerBindingType_Filtering;

    WGPUBindGroupLayoutDescriptor bindGroupLayoutDescriptor = WGPU_BIND_GROUP_LAYOUT_DESCRIPTOR_INIT;
    bindGroupLayoutDescriptor.entries = entries;
    bindGroupLayoutDescriptor.entryCount = std::size(entries);

    return wgpuDeviceCreateBindGroupLayout(device, &bindGroupLayoutDescriptor);
}

WGPUBindGroupLayout createMaterialBindGroupLayout(WGPUDevice device) {
    WGPUBindGroupLayoutEntry entries[3];

    entries[0] = WGPU_BIND_GROUP_LAYOUT_ENTRY_INIT;
    entries[0].binding = 0;
    entries[0].visibility = WGPUShaderStage_Fragment;
    entries[0].sampler.type = WGPUSamplerBindingType_Filtering;

    entries[1] = WGPU_BIND_GROUP_LAYOUT_ENTRY_INIT;
    entries[1].binding = 1;
    entries[1].visibility = WGPUShaderStage_Fragment;
    entries[1].texture.viewDimension = WGPUTextureViewDimension_2D;
    entries[1].texture.sampleType = WGPUTextureSampleType_Float;

    entries[2] = WGPU_BIND_GROUP_LAYOUT_ENTRY_INIT;
    entries[2].binding = 2;
    entries[2].visibility = WGPUShaderStage_Fragment;
    entries[2].texture.viewDimension = WGPUTextureViewDimension_2D;
    entries[2].texture.sampleType = WGPUTextureSampleType_Float;

    WGPUBindGroupLayoutDescriptor bindGroupLayoutDescriptor = WGPU_BIND_GROUP_LAYOUT_DESCRIPTOR_INIT;
    bindGroupLayoutDescriptor.entries = entries;
    bindGroupLayoutDescriptor.entryCount = std::size(entries);

    return wgpuDeviceCreateBindGroupLayout(device, &bindGroupLayoutDescriptor);
}

WGPUBindGroup createCameraBindGroup(WGPUDevice device, WGPUBindGroupLayout bindGroupLayout, WGPUBuffer uniformBuffer) {
    WGPUBindGroupEntry entries[1] = {WGPU_BIND_GROUP_ENTRY_INIT};
    entries[0].binding = 0;
    entries[0].buffer = uniformBuffer;
    entries[0].size = WGPU_WHOLE_SIZE;

    WGPUBindGroupDescriptor bindGroupDescriptor = WGPU_BIND_GROUP_DESCRIPTOR_INIT;
    bindGroupDescriptor.layout = bindGroupLayout;
    bindGroupDescriptor.entries = entries;
    bindGroupDescriptor.entryCount = std::size(entries);

    return wgpuDeviceCreateBindGroup(device, &bindGroupDescriptor);
}

WGPUBindGroup createLightsBindGroup(WGPUDevice device, WGPUBindGroupLayout bindGroupLayout, WGPUBuffer uniformBuffer, WGPUTextureView shadowView, WGPUSampler shadowSampler) {
    WGPUBindGroupEntry entries[3];

    entries[0] = WGPU_BIND_GROUP_ENTRY_INIT;
    entries[0].binding = 0;
    entries[0].buffer = uniformBuffer;
    entries[0].size = WGPU_WHOLE_SIZE;

    entries[1] = WGPU_BIND_GROUP_ENTRY_INIT;
    entries[1].binding = 1;
    entries[1].textureView = shadowView;

    entries[2] = WGPU_BIND_GROUP_ENTRY_INIT;
    entries[2].binding = 2;
    entries[2].sampler = shadowSampler;

    WGPUBindGroupDescriptor bindGroupDescriptor = WGPU_BIND_GROUP_DESCRIPTOR_INIT;
    bindGroupDescriptor.layout = bindGroupLayout;
    bindGroupDescriptor.entries = entries;
    bindGroupDescriptor.entryCount = std::size(entries);

    return wgpuDeviceCreateBindGroup(device, &bindGroupDescriptor);
}

WGPUBindGroup createMaterialBindGroup(WGPUDevice device, WGPUBindGroupLayout bindGroupLayout, WGPUSampler sampler, const MaterialGpu& material) {
    WGPUBindGroupEntry entries[3];

    entries[0] = WGPU_BIND_GROUP_ENTRY_INIT;
    entries[0].binding = 0;
    entries[0].sampler = sampler;

    entries[1] = WGPU_BIND_GROUP_ENTRY_INIT;
    entries[1].binding = 1;
    entries[1].textureView = material.albedoTextureView;

    entries[2] = WGPU_BIND_GROUP_ENTRY_INIT;
    entries[2].binding = 2;
    entries[2].textureView = material.transparencyTextureView;

    WGPUBindGroupDescriptor bindGroupDescriptor = WGPU_BIND_GROUP_DESCRIPTOR_INIT;
    bindGroupDescriptor.layout = bindGroupLayout;
    bindGroupDescriptor.entries = entries;
    bindGroupDescriptor.entryCount = std::size(entries);

    return wgpuDeviceCreateBindGroup(device, &bindGroupDescriptor);
}

WGPURenderPipeline createPipeline(WGPUDevice device, WGPUShaderModule shaderModule, WGPUTextureFormat surfaceFormat,
        WGPUBindGroupLayout cameraBindGroupLayout, WGPUBindGroupLayout lightsBindGroupLayout, WGPUBindGroupLayout materialBindGroupLayout) {
    WGPUBindGroupLayout bindGroupLayouts[3] = {
        cameraBindGroupLayout,
        lightsBindGroupLayout,
        materialBindGroupLayout,
    };

    WGPUPipelineLayoutDescriptor pipelineLayoutDescriptor = WGPU_PIPELINE_LAYOUT_DESCRIPTOR_INIT;
    pipelineLayoutDescriptor.immediateSize = sizeof(Immediates);
    pipelineLayoutDescriptor.bindGroupLayouts = bindGroupLayouts;
    pipelineLayoutDescriptor.bindGroupLayoutCount = std::size(bindGroupLayouts);

    WGPUPipelineLayout pipelineLayout = wgpuDeviceCreatePipelineLayout(device, &pipelineLayoutDescriptor);

    WGPUColorTargetState colorTargetState = WGPU_COLOR_TARGET_STATE_INIT;
    colorTargetState.format = surfaceFormat;
    colorTargetState.writeMask = WGPUColorWriteMask_All;

    WGPUFragmentState fragmentState = WGPU_FRAGMENT_STATE_INIT;
    fragmentState.module = shaderModule;
    fragmentState.entryPoint = {"fragmentMain", WGPU_STRLEN};
    fragmentState.targetCount = 1;
    fragmentState.targets = &colorTargetState;

    WGPUVertexAttribute attributes[3] = {WGPU_VERTEX_ATTRIBUTE_INIT, WGPU_VERTEX_ATTRIBUTE_INIT, WGPU_VERTEX_ATTRIBUTE_INIT};
    attributes[0].offset = 0;
    attributes[0].shaderLocation = 0;
    attributes[0].format = WGPUVertexFormat_Float32x3;
    attributes[1].offset = 12;
    attributes[1].shaderLocation = 1;
    attributes[1].format = WGPUVertexFormat_Float32x3;
    attributes[2].offset = 24;
    attributes[2].shaderLocation = 2;
    attributes[2].format = WGPUVertexFormat_Float32x2;

    WGPUVertexBufferLayout vertexBufferLayout = WGPU_VERTEX_BUFFER_LAYOUT_INIT;
    vertexBufferLayout.arrayStride = sizeof(ObjVertex);
    vertexBufferLayout.attributeCount = std::size(attributes);
    vertexBufferLayout.attributes = attributes;
    vertexBufferLayout.stepMode = WGPUVertexStepMode_Vertex;

    WGPUDepthStencilState depthStencilState = WGPU_DEPTH_STENCIL_STATE_INIT;
    depthStencilState.format = WGPUTextureFormat_Depth24Plus;
    depthStencilState.depthWriteEnabled = WGPUOptionalBool_True;
    depthStencilState.depthCompare = WGPUCompareFunction_Less;

    WGPURenderPipelineDescriptor renderPipelineDescriptor = WGPU_RENDER_PIPELINE_DESCRIPTOR_INIT;
    renderPipelineDescriptor.layout = pipelineLayout;
    renderPipelineDescriptor.vertex.module = shaderModule;
    renderPipelineDescriptor.vertex.entryPoint = {"vertexMain", WGPU_STRLEN};
    renderPipelineDescriptor.vertex.buffers = &vertexBufferLayout;
    renderPipelineDescriptor.vertex.bufferCount = 1;
    renderPipelineDescriptor.primitive.topology = WGPUPrimitiveTopology_TriangleList;
    renderPipelineDescriptor.primitive.cullMode = WGPUCullMode_Back;
    renderPipelineDescriptor.fragment = &fragmentState;
    renderPipelineDescriptor.depthStencil = &depthStencilState;

    WGPURenderPipeline renderPipeline = wgpuDeviceCreateRenderPipeline(device, &renderPipelineDescriptor);
    wgpuPipelineLayoutRelease(pipelineLayout);

    return renderPipeline;
}

WGPURenderPipeline createDebugDepthTexturePipeline(WGPUDevice device, WGPUShaderModule shaderModule, WGPUTextureFormat surfaceFormat,
        WGPUBindGroupLayout lightsBindGroupLayout) {
    WGPUBindGroupLayout bindGroupLayouts[1] = {
        lightsBindGroupLayout,
    };

    WGPUPipelineLayoutDescriptor pipelineLayoutDescriptor = WGPU_PIPELINE_LAYOUT_DESCRIPTOR_INIT;
    pipelineLayoutDescriptor.immediateSize = sizeof(DebugDepthTextureImmediates);
    pipelineLayoutDescriptor.bindGroupLayouts = bindGroupLayouts;
    pipelineLayoutDescriptor.bindGroupLayoutCount = std::size(bindGroupLayouts);

    WGPUPipelineLayout pipelineLayout = wgpuDeviceCreatePipelineLayout(device, &pipelineLayoutDescriptor);

    WGPUColorTargetState colorTargetState = WGPU_COLOR_TARGET_STATE_INIT;
    colorTargetState.format = surfaceFormat;
    colorTargetState.writeMask = WGPUColorWriteMask_All;

    WGPUFragmentState fragmentState = WGPU_FRAGMENT_STATE_INIT;
    fragmentState.module = shaderModule;
    fragmentState.entryPoint = {"fragmentMain", WGPU_STRLEN};
    fragmentState.targetCount = 1;
    fragmentState.targets = &colorTargetState;

    WGPUDepthStencilState depthStencilState = WGPU_DEPTH_STENCIL_STATE_INIT;
    depthStencilState.format = WGPUTextureFormat_Depth24Plus;
    depthStencilState.depthWriteEnabled = WGPUOptionalBool_False;
    depthStencilState.depthCompare = WGPUCompareFunction_Always;

    WGPURenderPipelineDescriptor renderPipelineDescriptor = WGPU_RENDER_PIPELINE_DESCRIPTOR_INIT;
    renderPipelineDescriptor.layout = pipelineLayout;
    renderPipelineDescriptor.vertex.module = shaderModule;
    renderPipelineDescriptor.vertex.entryPoint = {"vertexMain", WGPU_STRLEN};
    renderPipelineDescriptor.primitive.topology = WGPUPrimitiveTopology_TriangleList;
    renderPipelineDescriptor.fragment = &fragmentState;
    renderPipelineDescriptor.depthStencil = &depthStencilState;

    WGPURenderPipeline renderPipeline = wgpuDeviceCreateRenderPipeline(device, &renderPipelineDescriptor);
    wgpuPipelineLayoutRelease(pipelineLayout);

    return renderPipeline;
}

WGPUTexture createTextureFromImage(WGPUDevice device, WGPUQueue queue, const Image& image) {
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
    layout.bytesPerRow = image.width * 4;
    layout.rowsPerImage = image.height;

    WGPUExtent3D extent = WGPU_EXTENT_3D_INIT;
    extent.width = image.width;
    extent.height = image.height;

    wgpuQueueWriteTexture(queue, &destination, image.pixels.get(), image.width * image.height * 4, &layout, &extent);

    return texture;
}

WGPUTexture createTexture(WGPUDevice device, WGPUQueue queue, const std::filesystem::path& path) {
    Image image = loadImage(path);

    if (!image.pixels || image.width == 0 || image.height == 0) {
        throw std::runtime_error("failed to load texture");
    }

    return createTextureFromImage(device, queue, image);
}

WGPUTexture createSolidTexture(WGPUDevice device, WGPUQueue queue, Image::Pixel pixel) {
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

int main(int argc, char **argv) try {
    if (argc != 2) {
        throw std::runtime_error("usage: homework2 <scene.obj>");
    }

    WgpuApp app("Homework 2", 1280, 720, false);
    app.grab_mouse(true);

    WGPUBindGroupLayout cameraBindGroupLayout = createCameraBindGroupLayout(app.device());
    WGPUBindGroupLayout lightsBindGroupLayout = createLightsBindGroupLayout(app.device());
    WGPUBindGroupLayout materialBindGroupLayout = createMaterialBindGroupLayout(app.device());

    DirectionalShadow shadow(
        app.device(),
        projectRoot / "shadow_shader.wgsl",
        materialBindGroupLayout,
        4096
    );

    WGPUShaderModule shaderModule = createShaderModule(app.device(), projectRoot / "shader.wgsl");
    WGPUShaderModule debugDepthTextureShaderModule = createShaderModule(app.device(), projectRoot / "debug_depth_texture_shader.wgsl");

    WGPURenderPipeline renderPipeline = createPipeline(app.device(), shaderModule, app.surfaceFormat(), cameraBindGroupLayout, lightsBindGroupLayout, materialBindGroupLayout);
    WGPURenderPipeline debugDepthTextureRenderPipeline = createDebugDepthTexturePipeline(app.device(), debugDepthTextureShaderModule, app.surfaceFormat(), lightsBindGroupLayout);

    WGPUBuffer cameraUniformBuffer = createBuffer(app.device(), sizeof(CameraUniforms), WGPUBufferUsage_Uniform);
    WGPUBuffer lightsUniformBuffer = createBuffer(app.device(), sizeof(LightsUniforms), WGPUBufferUsage_Uniform);
    WGPUBindGroup cameraBindGroup = createCameraBindGroup(app.device(), cameraBindGroupLayout, cameraUniformBuffer);
    WGPUBindGroup lightsBindGroup = createLightsBindGroup(app.device(), lightsBindGroupLayout, lightsUniformBuffer, shadow.view(), shadow.sampler());

    const char* scenePath = argv[1];
    ObjMesh scene = loadObj(scenePath);

    WGPUSampler textureSampler = createSampler(app.device());

    WGPUTexture fallbackAlbedo = createSolidTexture(app.device(), app.queue(), {255, 255, 255, 255});
    WGPUTextureView fallbackAlbedoView = wgpuTextureCreateView(fallbackAlbedo, nullptr);

    WGPUTexture fallbackTransparency = createSolidTexture(app.device(), app.queue(), {255, 255, 255, 255});
    WGPUTextureView fallbackTransparencyView = wgpuTextureCreateView(fallbackTransparency, nullptr);

    std::vector<MaterialGpu> gpuMaterials;
    std::vector<WGPUBindGroup> materialBindGroups;
    for (const auto& material : scene.materials) {
        MaterialGpu materialGpu{};

        if (material.albedoTexture.empty()) {
            materialGpu.albedoTexture = fallbackAlbedo;
            materialGpu.albedoTextureView = fallbackAlbedoView;
        } else {
            materialGpu.albedoTexture = createTexture(app.device(), app.queue(), material.albedoTexture);
            materialGpu.albedoTextureView = wgpuTextureCreateView(materialGpu.albedoTexture, nullptr);
        }

        if (material.transparencyTexture.empty()) {
            materialGpu.transparencyTexture = fallbackTransparency;
            materialGpu.transparencyTextureView = fallbackTransparencyView;
        } else {
            materialGpu.transparencyTexture = createTexture(app.device(), app.queue(), material.transparencyTexture);
            materialGpu.transparencyTextureView = wgpuTextureCreateView(materialGpu.transparencyTexture, nullptr);
        }

        materialGpu.bindGroup = createMaterialBindGroup(app.device(), materialBindGroupLayout, textureSampler, materialGpu);

        gpuMaterials.push_back(materialGpu);
        materialBindGroups.push_back(materialGpu.bindGroup);
    }

    auto vertexBuffer = loadBuffer(app.device(), app.queue(), scene.vertices, WGPUBufferUsage_Vertex);
    auto indexBuffer = loadBuffer(app.device(), app.queue(), scene.indices, WGPUBufferUsage_Index);

    const auto center = math::center(scene.bounds);
    const auto extent = math::diagonal(scene.bounds);
    const float sceneScale = 2.f / std::max({extent[0], extent[1], extent[2]});
    const math::matrix4f model =
        math::scale(math::vector{sceneScale, sceneScale, sceneScale})
        * math::translation(math::vector{-center[0], -center[1], -center[2]});

    WGPUTexture depthBuffer = nullptr;
    WGPUTextureView depthBufferView = nullptr;

    auto lastFrameStart = std::chrono::high_resolution_clock::now();
    float time = 0.f;
    Camera camera;

    bool paused = false;

    bool running = true;
    while (running) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            camera.handleEvent(event);
            switch (event.type) {
            case SDL_EVENT_QUIT:
                running = false;
                break;
            case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
                app.resize(event.window.data1, event.window.data2);
                break;
            case SDL_EVENT_KEY_DOWN:
                if (event.key.key == SDLK_ESCAPE)
                    running = false;
                if (event.key.key == SDLK_SPACE)
                    paused ^= true;
                break;
            }
        }   

        if (!running) {
            break;
        }

        std::optional<WGPUSurfaceTexture> surfaceTexture = app.beginFrame();
        if (!surfaceTexture) {
            continue;
        }

        if (!depthBuffer || wgpuTextureGetWidth(depthBuffer) != app.width() || wgpuTextureGetHeight(depthBuffer) != app.height()) {
            if (depthBufferView) wgpuTextureViewRelease(depthBufferView);
            if (depthBuffer) wgpuTextureRelease(depthBuffer);

            WGPUTextureDescriptor depthBufferDescriptor = WGPU_TEXTURE_DESCRIPTOR_INIT;
            depthBufferDescriptor.usage = WGPUTextureUsage_RenderAttachment;
            depthBufferDescriptor.dimension = WGPUTextureDimension_2D;
            depthBufferDescriptor.size = {(std::uint32_t)app.width(), (std::uint32_t)app.height(), 1};
            depthBufferDescriptor.format = WGPUTextureFormat_Depth24Plus;
            depthBuffer = wgpuDeviceCreateTexture(app.device(), &depthBufferDescriptor);

            WGPUTextureViewDescriptor depthBufferViewDescriptor = WGPU_TEXTURE_VIEW_DESCRIPTOR_INIT;
            depthBufferViewDescriptor.format = WGPUTextureFormat_Depth24Plus;
            depthBufferViewDescriptor.dimension = WGPUTextureViewDimension_2D;
            depthBufferViewDescriptor.mipLevelCount = 1;
            depthBufferViewDescriptor.arrayLayerCount = 1;
            depthBufferViewDescriptor.aspect = WGPUTextureAspect_DepthOnly;
            depthBufferViewDescriptor.usage = WGPUTextureUsage_RenderAttachment | WGPUTextureUsage_TransientAttachment;
            depthBufferView = wgpuTextureCreateView(depthBuffer, &depthBufferViewDescriptor);
        }

        auto const now = std::chrono::high_resolution_clock::now();
        float const dt = std::chrono::duration<float>(now - lastFrameStart).count();
        if (!paused) time += dt;
        lastFrameStart = now;

        camera.update(dt);

        math::matrix4f const view = camera.viewMatrix();
        math::matrix4f const projection = camera.projectionMatrix(app.width() * 1.f / app.height());

        math::vector3f const lightDirection = math::normalized(math::vector{std::cos(time * 0.1f), 1.f, std::sin(time * 0.1f)});

        shadow.update(lightDirection, model, scene.bounds);

        CameraUniforms const cameraUniforms {
            .viewProjectionTranspose = math::transpose(projection * view),
            .position = camera.position(),
        };

        wgpuQueueWriteBuffer(app.queue(), cameraUniformBuffer, 0, &cameraUniforms, sizeof(cameraUniforms));

        const LightsUniforms lightsUniforms {
            .shadowProjectionTranspose = math::transpose(shadow.projection()),
            .ambientIntensity = {0.05f, 0.10f, 0.15f},
            .lightDirection = lightDirection,
            .lightIntensity = {0.8f, 0.6f, 0.4f},
        };

        wgpuQueueWriteBuffer(app.queue(), lightsUniformBuffer, 0, &lightsUniforms, sizeof(lightsUniforms));

        WGPUTextureView targetView = wgpuTextureCreateView(surfaceTexture->texture, nullptr);

        WGPUCommandEncoder encoder = wgpuDeviceCreateCommandEncoder(app.device(), nullptr);

        shadow.render(encoder, vertexBuffer, indexBuffer, scene.submeshes, materialBindGroups);

        WGPURenderPassColorAttachment colorAttachment = WGPU_RENDER_PASS_COLOR_ATTACHMENT_INIT;
        colorAttachment.view = targetView;
        colorAttachment.loadOp = WGPULoadOp_Clear;
        colorAttachment.storeOp = WGPUStoreOp_Store;
        colorAttachment.clearValue = {0.01, 0.03, 0.05, 1.0};

        WGPURenderPassDepthStencilAttachment depthStencilAttachment = WGPU_RENDER_PASS_DEPTH_STENCIL_ATTACHMENT_INIT;
        depthStencilAttachment.view = depthBufferView;
        depthStencilAttachment.depthLoadOp = WGPULoadOp_Clear;
        depthStencilAttachment.depthStoreOp = WGPUStoreOp_Discard;
        depthStencilAttachment.depthClearValue = 1.f;
        depthStencilAttachment.depthReadOnly = WGPU_FALSE;

        WGPURenderPassDescriptor renderPassDescriptor = WGPU_RENDER_PASS_DESCRIPTOR_INIT;
        renderPassDescriptor.colorAttachmentCount = 1;
        renderPassDescriptor.colorAttachments = &colorAttachment;
        renderPassDescriptor.depthStencilAttachment = &depthStencilAttachment;
        WGPURenderPassEncoder renderPass = wgpuCommandEncoderBeginRenderPass(encoder, &renderPassDescriptor);

        wgpuRenderPassEncoderSetPipeline(renderPass, renderPipeline);
        wgpuRenderPassEncoderSetBindGroup(renderPass, 0, cameraBindGroup, 0, nullptr);
        wgpuRenderPassEncoderSetBindGroup(renderPass, 1, lightsBindGroup, 0, nullptr);

        Immediates immediates {
            .modelTranspose = math::transpose(model),
        };
        wgpuRenderPassEncoderSetVertexBuffer(renderPass, 0, vertexBuffer, 0, WGPU_WHOLE_SIZE);
        wgpuRenderPassEncoderSetIndexBuffer(renderPass, indexBuffer, WGPUIndexFormat_Uint32, 0, WGPU_WHOLE_SIZE);

        for (const auto& submesh : scene.submeshes) {
            const auto& material = gpuMaterials[submesh.materialIndex];
            wgpuRenderPassEncoderSetBindGroup(renderPass, 2, material.bindGroup, 0, nullptr);

            const auto& objMaterial = scene.materials[submesh.materialIndex];
            immediates.specular = objMaterial.specular;
            immediates.specularPower = objMaterial.specularPower;
            wgpuRenderPassEncoderSetImmediates(renderPass, 0, &immediates, sizeof(immediates));

            wgpuRenderPassEncoderDrawIndexed(renderPass, submesh.indexCount, 1, submesh.firstIndex, 0, 0);
        }

        wgpuRenderPassEncoderEnd(renderPass);
        wgpuRenderPassEncoderRelease(renderPass);

        WGPUCommandBuffer commandBuffer = wgpuCommandEncoderFinish(encoder, nullptr);
        wgpuCommandEncoderRelease(encoder);

        wgpuQueueSubmit(app.queue(), 1, &commandBuffer);
        wgpuCommandBufferRelease(commandBuffer);

        wgpuSurfacePresent(app.surface());

        wgpuTextureViewRelease(targetView);
        wgpuTextureRelease(surfaceTexture->texture);
    }

    if (depthBufferView) wgpuTextureViewRelease(depthBufferView);
    if (depthBuffer) wgpuTextureRelease(depthBuffer);

    wgpuBufferRelease(vertexBuffer);
    wgpuBufferRelease(indexBuffer);

    wgpuRenderPipelineRelease(renderPipeline);
    wgpuShaderModuleRelease(shaderModule);

} catch (const std::exception &e) {
    std::cerr << "error: " << e.what() << std::endl;
    return EXIT_FAILURE;
}
