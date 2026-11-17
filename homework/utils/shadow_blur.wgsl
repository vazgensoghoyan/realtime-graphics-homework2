struct VertexOut {
    @builtin(position) position: vec4f,
}

struct Immediates {
    axis: vec2f,
}

@group(0) @binding(0) var source: texture_2d<f32>;

var<immediate> immediates: Immediates;

const BLUR_WEIGHTS = array<f32, 5>(
    0.228,
    0.20,
    0.12,
    0.05,
    0.016
);

@vertex
fn vertexMain(@builtin(vertex_index) index: u32) -> VertexOut {
    let positions = array<vec2f, 3>(
        vec2f(-1.0, -1.0),
        vec2f(3.0, -1.0),
        vec2f(-1.0, 3.0)
    );

    return VertexOut(vec4f(positions[index], 0.0, 1.0));
}

@fragment
fn fragmentMain(@builtin(position) position: vec4f) -> @location(0) vec2f {
    let dimensions = textureDimensions(source);
    let maxPixel = vec2i(dimensions) - vec2i(1, 1);

    let center = vec2i(clamp(
        position.xy,
        vec2f(0.0),
        vec2f(dimensions) - vec2f(1.0)
    ));
    let axis = vec2i(immediates.axis);

    var result = vec2f(0.0);

    for (var offset = -4; offset <= 4; offset++) {
        let pixel = clamp(
            center + axis * offset,
            vec2i(0, 0),
            maxPixel
        );

        let weight = BLUR_WEIGHTS[u32(abs(offset))];

        result += textureLoad(source, pixel, 0).xy * weight;
    }

    return result;
}
