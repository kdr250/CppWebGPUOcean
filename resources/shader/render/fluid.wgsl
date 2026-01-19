@group(0) @binding(0) var<uniform> uniforms: RenderUniforms;
@group(0) @binding(1) var texture: texture_2d<f32>;
@group(0) @binding(2) var textureSampler: sampler;

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
    let color = textureSample(texture, textureSampler, input.uv).rgb;
    return vec4f(color, 1.0);
}
