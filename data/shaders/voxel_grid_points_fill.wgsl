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

@compute @workgroup_size(8, 8, 8)
fn compute(@builtin(global_invocation_id) id: vec3<u32>) {
    let cellSize : f32 = grid_data._CellHalfSize * 2.0;
    
    // virtually is the same as workgroups, but I don't understand why it doesn't work using them instead of this
    let loopBounds : vec3<u32> = vec3(grid_data._GridWidth / 8, 
                                  grid_data._GridHeight / 8, 
                                  grid_data._GridDepth / 8 );
    
    // same as threads
    let offset : vec3<u32> = vec3(8, 8, 8);

    for (var i : u32 = 0; i < loopBounds.x; i++){
        for (var j : u32 = 0; j < loopBounds.y; j++){
            for (var k : u32 = 0; k < loopBounds.z; k++){
                _VoxelGridPoints[u32((id.x + offset.x * i) + grid_data._GridWidth * ((id.y + offset.y * j) + grid_data._GridHeight * (id.z + k * offset.z)))] = vec4f(
                grid_data._BoundsMin.x + f32(id.x + offset.x * i) * cellSize,
                grid_data._BoundsMin.y + f32(id.y + offset.y * j) * cellSize,
                grid_data._BoundsMin.z + f32(id.z + offset.z * k) * cellSize, 1.0);
            }
        }
    }
    
}