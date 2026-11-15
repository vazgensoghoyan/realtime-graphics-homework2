struct VertexIn {
    @location(0) position: vec3f,
    @location(1) normal: vec3f,
    @location(2) texcoord: vec2f,
}

struct VertexOut {
    @builtin(position) clipPosition: vec4f,
    @location(0) worldPosition: vec3f,
    @location(1) normal: vec3f,
    @location(2) texcoord: vec2f,
}

struct CameraUniforms {
    viewProjection: mat4x4f,
    position: vec3f,
}

struct LightsUniforms {
    shadowProjection: mat4x4f,
    ambientIntensity: vec3f,
    lightDirection: vec3f,
    lightIntensity: vec3f,
}

struct Immediates {
    model: mat4x4f,
    specular: vec3f,
    specularPower: f32,
    hasTransparency: u32,
}

@group(0) @binding(0) var<uniform> camera: CameraUniforms;
@group(1) @binding(0) var<uniform> lights: LightsUniforms;
@group(1) @binding(1) var shadowTexture: texture_depth_2d;
@group(1) @binding(2) var shadowSampler: sampler;
@group(2) @binding(0) var materialSampler: sampler;
@group(2) @binding(1) var albedoTexture: texture_2d<f32>;
@group(2) @binding(2) var transparencyTexture: texture_2d<f32>;

var<immediate> immediates: Immediates;

@vertex
fn vertexMain(in: VertexIn) -> VertexOut {
    let worldPosition = immediates.model * vec4f(in.position, 1.0);
    return VertexOut(
        camera.viewProjection * worldPosition,
        worldPosition.xyz,
        normalize((immediates.model * vec4f(in.normal, 0.0)).xyz),
        in.texcoord
    );
}

const PI = 3.1415926535;

fn directionalShadow(worldPosition: vec3f) -> f32 {
    var position = lights.shadowProjection * vec4f(worldPosition, 1.0);
    position /= position.w;

    if (
        abs(position.x) > 1.0 ||
        abs(position.y) > 1.0 ||
        position.z < 0.0 ||
        position.z > 1.0
    ) {
        return 1.0;
    }

    let uv = position.xy * vec2f(0.5, -0.5) + vec2f(0.5);

    let textureSize = vec2f(textureDimensions(shadowTexture));

    let bias = 0.002;

    let texelSize = vec2f(1.0) / textureSize;
    let currentDepth = position.z - bias;

    var result = 0.0;

    for (var y = -1; y <= 1; y++) {
        for (var x = -1; x <= 1; x++) {
            let offset = vec2f(f32(x), f32(y)) * texelSize;
            let storedDepth = textureSample(shadowTexture, shadowSampler, uv + offset);
            if (currentDepth <= storedDepth) {
                result += 1.0;
            }
        }
    }

    return result / 9.0;
}

@fragment
fn fragmentMain(in: VertexOut) -> @location(0) vec4f {
    if (immediates.hasTransparency == 1u) {
        let alpha = textureSample(transparencyTexture, materialSampler, in.texcoord).r;
        if (alpha < 0.5) {
            discard;
        }
    }

    let normal = normalize(in.normal);

    let viewDirection = normalize(camera.position - in.worldPosition);
    let halfway = normalize(lights.lightDirection + viewDirection);

    let specular =
        immediates.specular * (immediates.specularPower + 2.0) / (2.0 * PI)
        * pow(max(0.0, dot(halfway, normal)), immediates.specularPower);

    let albedo = textureSample(albedoTexture, materialSampler, in.texcoord).rgb;

    let shadow = directionalShadow(in.worldPosition);
    let directLight = lights.lightIntensity * shadow;

    let cosine = max(0.0, dot(normal, lights.lightDirection));
    let color = albedo * (lights.ambientIntensity + directLight * cosine) + directLight * specular;

    return vec4f(color, 1.0);
}
