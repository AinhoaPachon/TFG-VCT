#include mesh_includes.wgsl

@group(0) @binding(1) var<storage, read> _VoxelGridPoints: array<vec4f>;

@group(1) @binding(0) var<uniform> camera_data : CameraData;

@vertex
fn vs_main(in: VertexInput) -> VertexOutput {
    
    var out: VertexOutput;
    var localPos : vec4f = _VoxelGridPoints[in.instance_id];
    out.position = camera_data.view_projection * vec4f(localPos.xyz + in.position * 0.001, 1.0);
    out.color = vec4f(0.5, 0.0, 0.9, localPos.w);
    out.normal = localPos.rgb;
    return out; 
}

struct FragmentOutput {
    @location(0) color: vec4f
}

@fragment
fn fs_main(in: VertexOutput) -> FragmentOutput {

    let eye : vec3f = normalize(camera_data.eye);

    var out : FragmentOutput;
    if (in.color.w == 0.0) {
        discard;
    }
    out.color = vec4f(in.color);
    return out;
}