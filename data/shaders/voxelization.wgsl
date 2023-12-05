// Aquí tienen RWStructuredBuffer<float4> _VoxelGridPoints;
// es el outputBuffer, donde meten los puntos del grid

struct GridData {
    _BoundsMin : vec4f,
    _CellHalfSize : f32,
    _GridWidth : u32,
    _GridHeight : u32,
    _GridDepth : u32
}

@group(0) @binding(0) var<storage, read_write> _VoxelGridPoints: array<vec4f>;
@group(1) @binding(0) var<uniform> grid_data: GridData;

@compute @workgroup_size(32)
fn compute(@builtin(global_invocation_id) id: vec3<u32>) {
    // De momento voy a castear esto a int porq castear 
    let cellSize : f32 = _CellHalfSize * 2.0;

    _VoxelGridPoints[u32(id.x + _GridWidth * (id.y + _GridHeight * id.z))] = vec4f(
        _BoundsMin.x + f32(id.x) * cellSize,
        _BoundsMin.x + f32(id.y) * cellSize,
        _BoundsMin.x + f32(id.z) * cellSize, 1.0);
}

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

@group(1) @binding(0) var<storage, read> mesh_data : InstanceData;
@group(2) @binding(0) var<uniform> camera_data : CameraData;

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