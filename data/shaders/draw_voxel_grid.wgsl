struct ColorBuffer {
  values: array<u32>,
};

struct Uniforms {
  screenWidth: u32,
  screenHeight: u32,
};

struct VertexInput {
    @location(0) position: vec3f,
    @location(1) uv: vec2f,
    @location(2) normal: vec3f,
    @location(3) tangent: vec3f,
    @location(4) color: vec3f,
    @location(5) weights: vec4f,
    @location(6) joints: vec4i
};

struct VertexOutput {
    @builtin(position) position: vec4f
};

// @vertex
// fn vs_main(in: VertexInput) -> VertexOutput {
//     var out: VertexOutput;
//     out.position = vec4f(in.position, 1.0);
//     out.uv = in.uv; // forward to the fragment shader
//     return out;
// }

@vertex
fn vs_main(@builtin(vertex_index) VertexIndex : u32) -> VertexOutput {
    var pos = array<vec2<f32>, 6>(
      vec2<f32>( 1.0,  1.0),
      vec2<f32>( 1.0, -1.0),
      vec2<f32>(-1.0, -1.0),
      vec2<f32>( 1.0,  1.0),
      vec2<f32>(-1.0, -1.0),
      vec2<f32>(-1.0,  1.0));

  var output : VertexOutput;
  output.position = vec4<f32>(pos[VertexIndex], 0.0, 1.0);
  return output;
}

// actualizar bind groups 
// @group(0) @binding(0) var left_eye_texture: texture_2d<f32>;
// @group(0) @binding(1) var texture_sampler : sampler;
@group(0) @binding(0) var<uniform> uniforms : Uniforms;
@group(1) @binding(0) var<storage, read_write> outputColorBuffer : ColorBuffer;

struct FragmentOutput {
    @location(0) color: vec4f
}

@fragment
fn fs_main(@builtin(position) coord: vec4<f32>) -> FragmentOutput {
    
    let X = floor(coord.x);
    let Y = floor(coord.y);
    let index = u32(X + Y * f32(uniforms.screenWidth)) * 3u;

    let R = f32(outputColorBuffer.values[index + 0u]) / 255.0;
    let G = f32(outputColorBuffer.values[index + 1u]) / 255.0;
    let B = f32(outputColorBuffer.values[index + 2u]) / 255.0;

    //let finalColor = vec3<f32>(R, G, B);
    let finalColor = vec3f(1.0, 0.0, 0.0);
    var out: FragmentOutput;
    out.color = vec4f(pow(finalColor.rgb, 1.0 / vec3f(2.2)), 1.0); // Color

    return out;
}
