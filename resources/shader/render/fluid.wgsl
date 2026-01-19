@group(0) @binding(1) var texture: texture_2d<f32>;

struct FragmentInput {
    @location(0) uv: vec2f,
    @location(1) iuv: vec2f,
}

@fragment
fn fs(input: FragmentInput) -> @location(0) vec4f {
    let textureSize = 16.0; // FIXME
    let color = textureLoad(texture, vec2<i32>(input.uv * textureSize), 0).rgb;
    return vec4f(color, 1.0);
}
