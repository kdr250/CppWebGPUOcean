@group(0) @binding(1) var texture: texture_2d<f32>;
@group(0) @binding(2) var textureSampler: sampler;

struct FragmentInput {
    @location(0) uv: vec2f,
    @location(1) iuv: vec2f,
}

@fragment
fn fs(input: FragmentInput) -> @location(0) vec4f {
    let color = textureSample(texture, textureSampler, input.uv).rgb;
    return vec4f(color, 1.0);
}
