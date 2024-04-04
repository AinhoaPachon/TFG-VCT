#include mesh_includes.wgsl

@group(0) @binding(1) var<storage, read> _VoxelGridPoints: array<vec4f>;

@group(1) @binding(0) var<uniform> camera_data : CameraData;

@vertex
fn vs_main(in: VertexInput) -> VertexOutput {
    
    var out: VertexOutput;
    var localPos : vec4f = _VoxelGridPoints[in.instance_id];
    out.position = camera_data.view_projection * vec4f(localPos.xyz + in.position * 0.005, 1.0);
    out.color = vec4f(1.0, 0.0, 0.0, 1.0);
    if(localPos.w != 0.0)
    {
        out.color = vec4f(1.0, 0.0, 0.0, 0.0);
    }
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
    out.color = vec4f(in.color);
    return out;
}