@group(0) @binding(0) var<uniform> uTime: f32;

@vertex
fn vs_main(@builtin(vertex_index) in_vertex_index: u32) -> @builtin(position) vec4f {
	var offset = uTime;
	var p = vec2f(0.0 + offset, 0.0);
	if (in_vertex_index == 0u) {
		p = vec2f(-0.5 + offset, -0.5);
	} else if (in_vertex_index == 1u) {
		p = vec2f(0.5 + offset, -0.5);
	} else {
		p = vec2f(0.0 + offset, 0.5);
	}
	return vec4f(p, 0.0, 1.0);
}

@fragment
fn fs_main() -> @location(0) vec4f {
	return vec4f(uTime, 0.0, 0.0, 1.0);
}
