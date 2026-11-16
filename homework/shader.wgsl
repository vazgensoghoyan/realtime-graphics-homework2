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
    pointPosition: vec3f,
    pointIntensity: vec3f,
    pointFarPlane: f32,
}

struct Immediates {
    model: mat4x4f,
    specular: vec3f,
    specularPower: f32,
}

@group(0) @binding(0) var<uniform> camera: CameraUniforms;

@group(1) @binding(0) var<uniform> lights: LightsUniforms;
@group(1) @binding(1) var shadowTexture: texture_2d<f32>;
@group(1) @binding(2) var shadowSampler: sampler;
@group(1) @binding(3) var pointShadowTexture: texture_cube<f32>;
@group(1) @binding(4) var pointShadowSampler: sampler;

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
const BIAS = 0.002;
const LIGHT_BLEEDING = 0.2;

fn chebyshevShadow(moments: vec2f, depth: f32) -> f32 {
    let e = moments.x;
    let d = max(moments.y - e * e, 0.00002);
    let diff = depth - e;

    if (depth <= e) {
        return 1.0;
    }

    let probability = clamp(d / (d + diff * diff), 0.0, 1.0);
    return clamp(
        (probability - LIGHT_BLEEDING) / (1.0 - LIGHT_BLEEDING),
        0.0,
        1.0
    );
}

fn blurredDirectionalMoments(uv: vec2f) -> vec2f {
    let dimensions = textureDimensions(shadowTexture);
    let dimensionsF = vec2f(dimensions);
    let maxPixel = vec2i(dimensions) - vec2i(1);

    let center = vec2i(clamp(uv * dimensionsF, vec2f(0.0), dimensionsF - vec2f(1.0)));

    var moments = vec2f(0.0);
    for (var y = -1; y <= 1; y++) {
        for (var x = -1; x <= 1; x++) {
            let offset = vec2i(x, y);
            let pixel = clamp(center + offset, vec2i(0), maxPixel);
            moments += textureLoad(shadowTexture, pixel, 0).xy;
        }
    }
    return moments / 9.0;
}

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

    let moments = blurredDirectionalMoments(uv);

    return chebyshevShadow(moments, position.z);
}

fn pointShadow(worldPosition: vec3f) -> f32 {
    let light_to_point = worldPosition - lights.pointPosition;
    let distance = length(light_to_point);

    if (distance >= lights.pointFarPlane || distance <= 0.0001) {
        return 1.0;
    }

    let dir = light_to_point / distance;
    let currentDepth = distance / lights.pointFarPlane;

    let helper = select(vec3f(0.0, 1.0, 0.0), vec3f(1.0, 0.0, 0.0), abs(dir.y) > 0.9);
    let tangent = normalize(cross(helper, dir));
    let bitangent = cross(dir, tangent);
    let texelAngle = 2.0 / f32(textureDimensions(pointShadowTexture).x);

    var moments = vec2f(0.0);

    for (var y = -1; y <= 1; y++) {
        for (var x = -1; x <= 1; x++) {
            let sampleDirection = normalize(
                dir + texelAngle * (
                    f32(x) * tangent +
                    f32(y) * bitangent
                )
            );

            moments += textureSampleLevel(
                pointShadowTexture,
                pointShadowSampler,
                sampleDirection,
                0.0
            ).xy;
        }
    }

    return chebyshevShadow(moments / 9.0, currentDepth);
}

fn phongLight(
    lightDirection: vec3f,
    lightIntensity: vec3f,
    normal: vec3f,
    viewDirection: vec3f,
    albedo: vec3f,
    specularFactor: vec3f,
    specularPower: f32
) -> vec3f {
    let cosine = max(dot(normal, lightDirection), 0.0);
    let halfway = normalize(lightDirection + viewDirection);

    var specular = vec3f(0.0);

    if (cosine > 0.0) {
        specular =
            specularFactor * (specularPower + 2.0) / (2.0 * PI)
            * pow(max(dot(halfway, normal), 0.0), specularPower);
    }

    return lightIntensity * (albedo * cosine + specular);
}

fn pointLight(
    position: vec3f,
    intensity: vec3f,
    worldPosition: vec3f,
    normal: vec3f,
    viewDirection: vec3f,
    albedo: vec3f,
    specularFactor: vec3f,
    specularPower: f32
) -> vec3f {
    let toLight = position - worldPosition;
    let distanceSquared = max(dot(toLight, toLight), 0.01);

    let lightDirection = normalize(toLight);
    let attenuatedIntensity = intensity / distanceSquared;

    return phongLight(
        lightDirection,
        attenuatedIntensity,
        normal,
        viewDirection,
        albedo,
        specularFactor,
        specularPower
    );
}

@fragment
fn fragmentMain(in: VertexOut) -> @location(0) vec4f {
    let alpha = textureSample(transparencyTexture, materialSampler, in.texcoord).r;
    if (alpha < 0.5) {
        discard;
    }

    let normal = normalize(in.normal);
    let viewDirection = normalize(camera.position - in.worldPosition);
    let albedo = textureSample(albedoTexture, materialSampler, in.texcoord).rgb;
    let shadow = directionalShadow(in.worldPosition);

    let directionalColor = phongLight(
        lights.lightDirection,
        lights.lightIntensity * shadow,
        normal,
        viewDirection,
        albedo,
        immediates.specular,
        immediates.specularPower
    );

    let pointShadowFactor = pointShadow(in.worldPosition);

    let pointColor = pointLight(
        lights.pointPosition,
        lights.pointIntensity * pointShadowFactor,
        in.worldPosition,
        normal,
        viewDirection,
        albedo,
        immediates.specular,
        immediates.specularPower
    );

    let color = albedo * lights.ambientIntensity + directionalColor + pointColor;
    return vec4f(color, 1.0);
}
