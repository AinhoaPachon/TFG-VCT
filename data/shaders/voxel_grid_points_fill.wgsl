// Aquí tienen RWStructuredBuffer<float4> _VoxelGridPoints;
// es el outputBuffer, donde meten los puntos del grid

struct GridData {
    _BoundsMin : vec4f,
    _CellHalfSize : f32,
    _GridWidth : u32,
    _GridHeight : u32,
    _GridDepth : u32
}

@group(0) @binding(0) var<uniform> grid_data: GridData;
@group(0) @binding(1) var<storage, read_write> _VoxelGridPoints: array<vec4f>;

//@compute @workgroup_size(8, 8, 8)
@compute @workgroup_size(4, 4, 4)
fn compute(@builtin(global_invocation_id) id: vec3<u32>) {
    
    let cellSize : f32 = grid_data._CellHalfSize * 2.0;
    
    
    _VoxelGridPoints[u32(id.x + grid_data._GridWidth * (id.y + grid_data._GridHeight * id.z))] = vec4f(
                grid_data._BoundsMin.x + f32(id.x) * cellSize,
                grid_data._BoundsMin.y + f32(id.y) * cellSize,
                grid_data._BoundsMin.z + f32(id.z) * cellSize, 1.0);

    
}