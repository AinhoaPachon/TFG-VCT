// Aquí tienen RWStructuredBuffer<float4> _VoxelGridPoints;
// es el outputBuffer, donde meten los puntos del grid


struct GridData {
    _BoundsMin : vec4,
    _CellHalfSize : float,
    _GridWidth : int,
    _GridHeight : int,
    _GridDepth : int
}

@group(0) @binding(0) var<storage, read> grid_data: GridData;
@group(1) @binding(0) var<storage, read_write> _VoxelGridPoints: vec4f;
@compute @workgroup_size(32)

fn compute(@builtin(global_invocation_id) id: vec3<u32>, in: grid_data) {
    float cellSize = in._CellHalfSize * 2.0;

    _VoxelGridPoints[id.x + in._GridWidth * (id.y + in._GridHeight * id.z)] = vec4(
        in._BoundsMin.x + id.x * cellSize,
        in._BoundsMin.x + id.y * cellSize,
        in._BoundsMin.x + id.z * cellSize, 1.0);
}

@vertex

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
    @location(1) size: float
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

fn vs_main(in: VertexInput) -> VertexOutput {
    
    let instance_data : RenderMeshData = mesh_data.data[in.instance_id];
    
    var out: VertexOutput;
    float4 localPos = _VoxelGridPoints[instance_index];
    out.position = camera_data.view_projection * instance_data.model * vec4f(in.position, 1.0);
    out.color = in.color;
    out.size = 5;
    return out;
}