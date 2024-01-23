struct VertexInput {
    @builtin(vertex_index) v_id: u32,
    @builtin(instance_index) instance_id : u32,
    @location(0) position: vec3f,
    @location(1) uv: vec2f,
    @location(2) normal: vec3f,
    @location(3) color: vec3f,
};

struct VertexOutput {
    @builtin(position) position: vec4f,
    @location(0) color: vec3f,
    @location(1) size: f32
};

struct RenderMeshData {
    model : mat4x4f,
    color : vec4f
};

struct InstanceData {
    data : array<RenderMeshData>
}

struct CameraData {
    view_projection : mat4x4f
};

@group(2) @binding(0) var<storage, read> mesh_data : InstanceData;
@group(3) @binding(0) var<uniform> camera_data : CameraData;

@vertex
fn vs_main(in: VertexInput) -> VertexOutput {
    
    let instance_data : RenderMeshData = mesh_data.data[in.instance_id];
    
    var out: VertexOutput;
    var localPos : vec4f = _VoxelGridPoints[instance_index];
    out.position = camera_data.view_projection * instance_data.model * vec4f(in.position, 1.0);
    out.color = in.color;
    out.size = 5;
    return out;
}

struct FragmentOutput {
    @location(0) color: vec4f
}

@fragment
fn fs_main(in: VertexOutput) -> FragmentOutput {
    return in.color;
}