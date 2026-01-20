@group(0) @binding(0) var<uniform> uniforms: RenderUniforms;
@group(0) @binding(1) var texture: texture_2d<f32>;
@group(0) @binding(2) var textureSampler: sampler;
@group(0) @binding(3) var thicknessTexture: texture_2d<f32>;
@group(0) @binding(4) var envmapTexture: texture_cube<f32>;

struct RenderUniforms {
    screenSize: vec2f,
    texelSize: vec2f,
    sphereSize: f32,
    invProjectionMatrix: mat4x4f,
    projectionMatrix: mat4x4f,
    viewMatrix: mat4x4f,
    invViewMatrix: mat4x4f,
}

struct FragmentInput {
    @location(0) uv: vec2f,
    @location(1) iuv: vec2f,
}

@fragment
fn fs(input: FragmentInput) -> @location(0) vec4f {
    let color = textureLoad(texture, vec2<i32>(input.uv * 16.0), 0).rgb;
    let thicknessColor = textureSample(thicknessTexture, textureSampler, input.uv).rgb;
    let envColor = textureSampleLevel(envmapTexture, textureSampler, vec3f(0.0, 0.0, 0.0), 0).rgb;
    return vec4f(color + thicknessColor + envColor, 1.0);
}
