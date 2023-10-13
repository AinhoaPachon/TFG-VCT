struct VertexInput {
    @location(0) position: vec3f,
    @location(1) uv: vec2f,
    @location(2) normal: vec3f,
    @location(3) color: vec3f,
};

struct VertexOutput {
    @builtin(position) position: vec4f,
    @location(0) uv: vec2f,
};

@vertex
fn vs_main(in: VertexInput) -> VertexOutput {
    var out: VertexOutput;
    out.position = vec4f(in.position, 1.0);
    out.uv = in.uv; // forward to the fragment shader
    return out;
}

@group(0) @binding(0) var render_texture: texture_2d<f32>;

struct FragmentOutput {
    @location(0) color: vec4f,
    @builtin(frag_depth) depth: f32,
}

@fragment
fn fs_main(in: VertexOutput) -> FragmentOutput {
    var texture_size = textureDimensions(render_texture);
    var uv_flip = in.uv;
    uv_flip.y = 1.0 - uv_flip.y;
    let ray_result = textureLoad(render_texture, vec2u(uv_flip * vec2f(texture_size)), 0);

    var out: FragmentOutput;
    out.color = vec4f(pow(ray_result.rgb, vec3f(2.2, 2.2, 2.2)), 1.0); // Color
    out.depth = ray_result.a; // Depth

    return out;
}