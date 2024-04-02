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

fn IntersectsTriangleAabb(tri_a: vec3f, tri_b: vec3f, tri_c: vec3f, aabb_center: vec3f, aabb_extents: vec3f)
{
    // Translate the triangle to the center of the AABB
    tri_a = tri_a - aabb_center;
    tri_b = tri_b - aabb_center;
    tri_c = tri_c - aabb_center;

    // Triangle edge normals
    let ab : vec3f = normalize(tri_b - tri_a);
    let bc : vec3f = normalize(tri_c - tri_b);
    let ca : vec3f = normalize(tri_a - tri_c);

    // Cross each of the edge normals with each AABB axis.
    // Since the AABB is axis-aligned, we hardcode the result for optimization

    // Cross ab, bc, ca with (1, 0, 0)
    let a00 : vec3f = vec3f(0.0, -ab.z, ab.y);
    let a01 : vec3f = vec3f(0.0, -bc.z, bc.y);
    let a02 : vec3f = vec3f(0.0, -ca.z, ca.y);

    // Cross ab, bc, ca with (0, 1, 0)
    let a10 : vec3f = vec3f(ab.z, 0.0, -ab.x);
    let a11 : vec3f = vec3f(bc.z, 0.0, -bc.x);
    let a12 : vec3f = vec3f(ca.z, 0.0, -ca.x);
    
    // Cross ab, bc, ca with (0, 0, 1)
    let a20 : vec3f = vec3f(-ab.y, ab.x, 0.0);
    let a21 : vec3f = vec3f(-bc.y, bc.x, 0.0);
    let a22 : vec3f = vec3f(-ca.y, a.x, 0.0);

    // Check the intersection with those 9 axis, the 3 AABB face normals and the triangle face normal. If any of them fails, return false.
    if (
        !IntersectsTriangleAabbSat(tri_a, tri_b, tri_c, aabb_extents, a00) ||
        !IntersectsTriangleAabbSat(tri_a, tri_b, tri_c, aabb_extents, a01) ||
        !IntersectsTriangleAabbSat(tri_a, tri_b, tri_c, aabb_extents, a02) ||
        !IntersectsTriangleAabbSat(tri_a, tri_b, tri_c, aabb_extents, a10) ||
        !IntersectsTriangleAabbSat(tri_a, tri_b, tri_c, aabb_extents, a11) ||
        !IntersectsTriangleAabbSat(tri_a, tri_b, tri_c, aabb_extents, a12) ||
        !IntersectsTriangleAabbSat(tri_a, tri_b, tri_c, aabb_extents, a20) ||
        !IntersectsTriangleAabbSat(tri_a, tri_b, tri_c, aabb_extents, a21) ||
        !IntersectsTriangleAabbSat(tri_a, tri_b, tri_c, aabb_extents, a22) ||
        !IntersectsTriangleAabbSat(tri_a, tri_b, tri_c, aabb_extents, float3(1, 0, 0)) ||
        !IntersectsTriangleAabbSat(tri_a, tri_b, tri_c, aabb_extents, float3(0, 1, 0)) ||
        !IntersectsTriangleAabbSat(tri_a, tri_b, tri_c, aabb_extents, float3(0, 0, 1)) ||
        !IntersectsTriangleAabbSat(tri_a, tri_b, tri_c, aabb_extents, cross(ab, bc))
    )
    {
        return false;
    }
    return true;
}
*/
fn compute(@builtin(global_invocation_id) id: vec3<u32>) {
    
    if (id.x >= grid_data._GridWidth || id.y >= grid_data._GridHeight || id.z >= grid_data._GridDepth) {
        return;
    }

    let cellSize : f32 = grid_data._CellHalfSize * 2.0;
    
    let center_pos : vec3f = vec3f(
        f32(id.x) * cellSize + grid_data._CellHalfSize + grid_data._BoundsMin.x,
        f32(id.y) * cellSize + grid_data._CellHalfSize + grid_data._BoundsMin.y,
        f32(id.z) * cellSize + grid_data._CellHalfSize + grid_data._BoundsMin.z);

    let aabb_center : vec3f = center_pos;
    let aabb_extents: vec3f = vec3f(grid_data._CellHalfSize, grid_data._CellHalfSize, grid_data._CellHalfSize);

    let intersects : bool = false;
    //for(let i : u32 = 0; i < )

    _VoxelGridPoints[u32(id.x + grid_data._GridWidth * (id.y + grid_data._GridHeight * id.z))] = vec4f(
                grid_data._BoundsMin.x + f32(id.x) * cellSize,
                grid_data._BoundsMin.y + f32(id.y) * cellSize,
                grid_data._BoundsMin.z + f32(id.z) * cellSize, 1.0);
}