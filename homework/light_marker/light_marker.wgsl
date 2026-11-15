struct CameraUniforms {
    viewProjection: mat4x4f,
    position: vec3f,
}

struct Immediates {
    position: vec3f,
    color: vec3f,
}

struct VertexOut {
    @builtin(position) position: vec4f,
}

@group(0) @binding(0) var<uniform> camera: CameraUniforms;

var<immediate> immediates: Immediates;

const SCALE = 0.035;

const VERTICES = array<vec3f, 6>(
    vec3f(-1.0, 0.0, 0.0),
    vec3f(1.0, 0.0, 0.0),
    vec3f(0.0, -1.0, 0.0),
    vec3f(0.0, 1.0, 0.0),
    vec3f(0.0, 0.0, -1.0),
    vec3f(0.0, 0.0, 1.0),
);

@vertex
fn vertexMain(@builtin(vertex_index) vertexIndex: u32) -> VertexOut {
    let position = immediates.position + SCALE * VERTICES[vertexIndex];
    return VertexOut(camera.viewProjection * vec4f(position, 1.0));
}

@fragment
fn fragmentMain() -> @location(0) vec4f {
    return vec4f(immediates.color, 1.0);
}
