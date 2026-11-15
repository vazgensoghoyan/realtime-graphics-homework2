struct VertexIn {
    @location(0) position: vec3f,
    @location(2) texcoord: vec2f,
}

struct VertexOut {
    @builtin(position) position: vec4f,
    @location(0) worldPosition: vec3f,
    @location(1) texcoord: vec2f,
}

struct Immediates {
    model: mat4x4f,
    shadowProjection: mat4x4f,
}

struct PointLightUniforms {
    position: vec3f,
    farPlane: f32,
}

@group(0) @binding(0) var<uniform> pointLight: PointLightUniforms;
@group(1) @binding(0) var materialSampler: sampler;
@group(1) @binding(2) var transparencyTexture: texture_2d<f32>;

var<immediate> immediates: Immediates;

@vertex
fn vertexMain(in: VertexIn) -> VertexOut {
    let worldPosition = immediates.model * vec4f(in.position, 1.0);
    return VertexOut(
        immediates.shadowProjection * worldPosition,
        worldPosition.xyz,
        in.texcoord
    );
}

@fragment
fn fragmentMain(in: VertexOut) -> @builtin(frag_depth) f32 {
    let alpha = textureSample(transparencyTexture, materialSampler, in.texcoord).r;
    if (alpha < 0.5) {
        discard;
    }

    let distance = length(in.worldPosition - pointLight.position);
    return clamp(distance / pointLight.farPlane, 0.0, 1.0);
}
