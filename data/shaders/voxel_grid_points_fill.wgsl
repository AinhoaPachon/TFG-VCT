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

@compute @workgroup_size(4, 4, 4)
/*
fn IntersectsTriangleAabbSat(v0: vec3f, v1: vec3f, v2: vec3f, aabb_extents: vec3f, axis: vec3f) -> bool
{
    let p0 : f32 = dot(v0, axis);
    let p1 : f32 = dot(v1, axis);
    let p2 : f32 = dot(v2, axis);

    let r : f32 = aabb_extents.x * abs(dot(vec3f(1.0, 0.0, 0.0), axis)) +
        aabb_extents.y * abs(dot(vec3f(0.0, 1.0, 0.0), axis)) +
        aabb_extents.z * abs(dot(vec3f(1.0, 0.0, 0.0), axis));

    let maxP : f32 = max(p0, max(p1, p2));
    let minP : f32 = min(p0, min(p1, p2));

    return !(max(-maxP, minP) > r);
}

fn IntersectsTriangleAabb()
*/ 
fn compute(@builtin(global_invocation_id) id: vec3<u32>) {
    
    let cellSize : f32 = grid_data._CellHalfSize * 2.0;
    
    _VoxelGridPoints[u32(id.x + grid_data._GridWidth * (id.y + grid_data._GridHeight * id.z))] = vec4f(
                grid_data._BoundsMin.x + f32(id.x) * cellSize,
                grid_data._BoundsMin.y + f32(id.y) * cellSize,
                grid_data._BoundsMin.z + f32(id.z) * cellSize, 1.0);
}