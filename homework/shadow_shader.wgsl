struct VertexIn {
    @location(0) position: vec3f,
}

struct Immediates {
    model: mat4x4f,
    shadowProjection: mat4x4f,
}

var<immediate> immediates: Immediates;

struct VertexOut {
    @builtin(position) position: vec4f,
}

@vertex
fn vertexMain(in: VertexIn) -> VertexOut {
    let worldPosition = immediates.model * vec4f(in.position, 1.0);
    return VertexOut(
        immediates.shadowProjection * worldPosition
    );
}