#include mesh_includes.wgsl

@group(0) @binding(0) var<storage, read> mesh_data : InstanceData;

@group(1) @binding(0) var<uniform> camera_data : CameraData;

@group(2) @binding(1) var<storage, read> _VoxelGridPoints: array<vec4f>;

@vertex
fn vs_main(in: VertexInput) -> VertexOutput {
    
    let instance_data : RenderMeshData = mesh_data.data[in.instance_id];
    
    var out: VertexOutput;
    var localPos : vec4f = _VoxelGridPoints[in.instance_id];
    out.position = camera_data.view_projection * instance_data.model * vec4f(in.position, 1.0) + localPos;
    out.color = in.color;
    return out;
}

struct FragmentOutput {
    @location(0) color: vec4f
}

@fragment
fn fs_main(in: VertexOutput) -> FragmentOutput {
    var out : FragmentOutput;
    out.color = vec4f(in.color, 1.0);
    return out;
}