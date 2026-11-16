struct VertexIn {
    @location(0) position: vec3f,
    @location(2) texcoord: vec2f,
}

struct VertexOut {
    @builtin(position) position: vec4f,
    @location(0) texcoord: vec2f,
}

struct Immediates {
    model: mat4x4f,
    shadowProjection: mat4x4f,
}

struct FragmentOut {
    @location(0) moments: vec2f,
}

@group(0) @binding(0) var materialSampler: sampler;
@group(0) @binding(2) var transparencyTexture: texture_2d<f32>;

var<immediate> immediates: Immediates;

@vertex
fn vertexMain(in: VertexIn) -> VertexOut {
    let worldPosition = immediates.model * vec4f(in.position, 1.0);
    return VertexOut(
        immediates.shadowProjection * worldPosition,
        in.texcoord
    );
}

@fragment
fn fragmentMain(in: VertexOut) -> FragmentOut {
    let alpha = textureSample(transparencyTexture, materialSampler, in.texcoord).r;
    if (alpha < 0.5) {
        discard;
    }

    let depth = in.position.z;
    return FragmentOut(
        vec2f(depth, depth * depth)
    );
}
