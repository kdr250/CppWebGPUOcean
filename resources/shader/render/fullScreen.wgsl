@group(0) @binding(0) var<uniform> uniforms: RenderUniforms;

struct VertexOutput {
  @builtin(position) position : vec4f,
  @location(0) uv : vec2f,
  @location(1) iuv : vec2f,
}

struct RenderUniforms {
    screenSize: vec2f,
    texelSize: vec2f,
    sphereSize: f32,
    invProjectionMatrix: mat4x4f,
    projectionMatrix: mat4x4f,
    viewMatrix: mat4x4f,
    invViewMatrix: mat4x4f,
}

@vertex
fn vs(@builtin(vertex_index) vertex_index : u32) -> VertexOutput {
    var out: VertexOutput;

    let pos = array(
        vec2( 1.0,  1.0),
        vec2( 1.0, -1.0),
        vec2(-1.0, -1.0),
        vec2( 1.0,  1.0),
        vec2(-1.0, -1.0),
        vec2(-1.0,  1.0),
    );

    let uv = array(
        vec2(1.0, 0.0),
        vec2(1.0, 1.0),
        vec2(0.0, 1.0),
        vec2(1.0, 0.0),
        vec2(0.0, 1.0),
        vec2(0.0, 0.0),
    );

    out.position = vec4(pos[vertex_index], 0.0, 1.0);
    out.uv = uv[vertex_index];
    out.iuv = out.uv * uniforms.screenSize;

    return out;
}
