// Aquí tienen RWStructuredBuffer<float4> _VoxelGridPoints;
// es el outputBuffer, donde meten los puntos del grid

struct GridData {
    _BoundsMin : vec4f,
    _CellHalfSize : f32,
    _GridWidth : u32,
    _GridHeight : u32,
    _GridDepth : u32
}

fn IntersectsTriangleAabbSat(v0: vec3f, v1: vec3f, v2: vec3f, aabb_extents: vec3f, axis: vec3f) -> bool
{
    // Project each triangle vertex onto the axis
    let p0 : f32 = dot(v0, axis);
    let p1 : f32 = dot(v1, axis);
    let p2 : f32 = dot(v2, axis);

    /*
    1. We assume AABB is at the origin
    2. We project the face normal onto the provided axis, with a result between -1 and 1
    3. Take the latter number and convert it from 0 to 1. This number represents how aligned the face normal and the axis are
    4. We multiply that value by the AABB extent. The sum of all those values is r, the length of the AABB into a single axis
    */

    let r : f32 = aabb_extents.x * abs(dot(vec3f(1.0, 0.0, 0.0), axis)) +
        aabb_extents.y * abs(dot(vec3f(0.0, 1.0, 0.0), axis)) +
        aabb_extents.z * abs(dot(vec3f(0.0, 0.0, 1.0), axis));

    // Find the minimum and maximum points of the projected triangle to create a line from minP to maxP
    let maxP : f32 = max(p0, max(p1, p2));
    let minP : f32 = min(p0, min(p1, p2));

    // Check if the line 0 to r overlaps with the line minP to maxP
    // TRUE si INTERSECCIONA
    return !(max(-maxP, minP) > r);
}

fn IntersectsTriangleAabb(tri_a: vec3f, tri_b: vec3f, tri_c: vec3f, aabb_center: vec3f, aabb_extents: vec3f) -> bool
{
    // Translate the triangle to the center of the AABB
    let tri_a_dummy : vec3f = tri_a - aabb_center;
    let tri_b_dummy : vec3f = tri_b - aabb_center;
    let tri_c_dummy : vec3f = tri_c - aabb_center;

    // Triangle edge normals
    let ab : vec3f = normalize(tri_b_dummy - tri_a_dummy);
    let bc : vec3f = normalize(tri_c_dummy - tri_b_dummy);
    let ca : vec3f = normalize(tri_a_dummy - tri_c_dummy);

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
    let a22 : vec3f = vec3f(-ca.y, ca.x, 0.0);

    // Check the intersection with those 9 axis, the 3 AABB face normals and the triangle face normal. If any of them doesn't intersect, return true.
    if (!IntersectsTriangleAabbSat(tri_a_dummy, tri_b_dummy, tri_c_dummy, aabb_extents, a00) ||
        !IntersectsTriangleAabbSat(tri_a_dummy, tri_b_dummy, tri_c_dummy, aabb_extents, a01) ||
        !IntersectsTriangleAabbSat(tri_a_dummy, tri_b_dummy, tri_c_dummy, aabb_extents, a02) ||
        !IntersectsTriangleAabbSat(tri_a_dummy, tri_b_dummy, tri_c_dummy, aabb_extents, a10) ||
        !IntersectsTriangleAabbSat(tri_a_dummy, tri_b_dummy, tri_c_dummy, aabb_extents, a11) ||
        !IntersectsTriangleAabbSat(tri_a_dummy, tri_b_dummy, tri_c_dummy, aabb_extents, a12) ||
        !IntersectsTriangleAabbSat(tri_a_dummy, tri_b_dummy, tri_c_dummy, aabb_extents, a20) ||
        !IntersectsTriangleAabbSat(tri_a_dummy, tri_b_dummy, tri_c_dummy, aabb_extents, a21) ||
        !IntersectsTriangleAabbSat(tri_a_dummy, tri_b_dummy, tri_c_dummy, aabb_extents, a22) ||
        !IntersectsTriangleAabbSat(tri_a_dummy, tri_b_dummy, tri_c_dummy, aabb_extents, vec3f(1, 0, 0)) ||
        !IntersectsTriangleAabbSat(tri_a_dummy, tri_b_dummy, tri_c_dummy, aabb_extents, vec3f(0, 1, 0)) ||
        !IntersectsTriangleAabbSat(tri_a_dummy, tri_b_dummy, tri_c_dummy, aabb_extents, vec3f(0, 0, 1)) ||
        !IntersectsTriangleAabbSat(tri_a_dummy, tri_b_dummy, tri_c_dummy, aabb_extents, cross(ab, bc))
    )
    {
        return false;
    }
    return true;
}

@group(0) @binding(0) var<uniform> grid_data: GridData;
@group(0) @binding(1) var<storage, read_write> _VoxelGridPoints: array<vec4f>;
@group(0) @binding(2) var<storage, read_write> _MeshVertexPositions: array<vec4f>;
@group(0) @binding(3) var<storage, read_write> _VertexCount: array<u32>;
@group(0) @binding(4) var<uniform> _MeshCount: u32;
@group(0) @binding(5) var<storage, read_write> _VoxelColor: array<vec4f>;

@group(0) @binding(6) var<storage, read_write> _MeshesColor: array<vec4f>;


@group(0) @binding(7) var<storage, read_write> _VertexColors: array<vec4f>;

@compute @workgroup_size(4, 4, 4)
fn compute(@builtin(global_invocation_id) id: vec3<u32>) {
    
    if (id.x >= grid_data._GridWidth || id.y >= grid_data._GridHeight || id.z >= grid_data._GridDepth) {
        return;
    }

    let cellSize : f32 = grid_data._CellHalfSize * 2.0;
    
    // center of the current voxel
    let center_pos : vec3f = vec3f(
        f32(id.x) * cellSize + grid_data._CellHalfSize + grid_data._BoundsMin.x,
        f32(id.y) * cellSize + grid_data._CellHalfSize + grid_data._BoundsMin.y,
        f32(id.z) * cellSize + grid_data._CellHalfSize + grid_data._BoundsMin.z);

    let aabb_center : vec3f = center_pos;
    let aabb_extents: vec3f = vec3f(grid_data._CellHalfSize);

    var intersects : bool;
    var tri_a : vec3f;
    var tri_b : vec3f;
    var tri_c : vec3f;
    var count : u32 = 0;

    var color : vec4f;

    // Number of meshes in the scene
    for(var j : u32 = 0; j < _MeshCount; j = j + 1) {
        // Number of vertices a mesh has
        for(var i : u32 = 0; i < _VertexCount[j]; i = i + 3) {
            // Get a triangle
            tri_a = _MeshVertexPositions[count].xyz;
            tri_b = _MeshVertexPositions[count + 1].xyz;
            tri_c = _MeshVertexPositions[count + 2].xyz;

            // Check intersection
            intersects = IntersectsTriangleAabb(tri_a, tri_b, tri_c, aabb_center, aabb_extents);
            
            // Break loop when we find a triangle that intersects with the current voxel
            if(intersects) {
                color = (_VertexColors[count] + _VertexColors[count + 1] + _VertexColors[count + 2]) / 3; 
                break;
            }

            count = count + 3;
        }
        if(intersects) {
            _VoxelColor[u32(id.x + grid_data._GridWidth * (id.y + grid_data._GridHeight * id.z))] = vec4f( color );
            break;
        }
    }
    var dummy : vec4f = _MeshesColor[0];
    
    // 1.0 if intersects, 0.0 if doesn't
    var w : f32;
    if (intersects) {
        w = 1.0;
    } else {
        w = 0.0;
    }

    _VoxelGridPoints[u32(id.x + grid_data._GridWidth * (id.y + grid_data._GridHeight * id.z))] = vec4f(
            center_pos.xyz, w);
}