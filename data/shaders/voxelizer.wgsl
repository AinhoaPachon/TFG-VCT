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
@group(0) @binding(2) var<storage, read_write> _MeshesVertices: array<vec4f>;
@group(0) @binding(3) var<storage, read_write> _VertexCount: array<u32>;
@group(0) @binding(4) var<uniform> _MeshCount: u32;
@group(0) @binding(5) var<storage, read_write> _VoxelColor: array<vec4f>;
@group(0) @binding(6) var<storage, read_write> _colorBuffer: array<vec4f>;
@group(0) @binding(7) var<storage, read_write> _orthogonalProjection: mat4x4f;

#ifdef MATERIAL_OVERRIDE_COLOR
@group(0) @binding(8) var<storage, read_write> _MeshesColor: array<vec4f>;
#endif

#ifdef VERTEX_COLORS
@group(0) @binding(9) var<storage, read_write> _VertexColors: array<vec4f>;
#endif


@compute @workgroup_size(256, 1, 1)
fn compute(@builtin(global_invocation_id) id: vec3<u32>, @builtin(position) coord: vec4f<f32>) {
    
    let X = floor(coord.x);
    let Y = floor(coord.y);
    let index = u32(X + Y * uniforms.screenWidth) * 3u;//Multiply by 3 because we have 3 components, RGB.
    

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
    
    var vertices_projected : array<vec4f> = _MeshesVertices;

    // Number of meshes in the scene
    for(var j : u32 = 0; j < _MeshCount; j = j + 1) {
        // Number of vertices a mesh has
        for(var i : u32 = 0; i < _VertexCount[j]; i = i + 3) {
            // Get a triangle projected
            tri_a = _orthogonalProjection * _MeshesVertices[count].xyz;
            tri_b = _orthogonalProjection * _MeshesVertices[count + 1].xyz;
            tri_c = _orthogonalProjection * _MeshesVertices[count + 2].xyz;

            var projection : vec3f = abs(cross( tri_b - tri_a, tri_c - tri_a ));

            if (projection.x > projection.y && projection.x > projection.z){
                vertices_projected[count] = vec4f( tri_a.yz, 0, 1);
                vertices_projected[count + 1] = vec4f( tri_b.yz, 0, 1);
                vertices_projected[count + 2] = vec4f( tri_c.yz, 0, 1);

            } else if ( projection.y > projection.z ) {
                vertices_projected[count] = vec4f( tri_a.xz, 0, 1);
                vertices_projected[count + 1] = vec4f( tri_b.xz, 0, 1);
                vertices_projected[count + 2] = vec4f( tri_c.xz, 0, 1);

            } else {
                vertices_projected[count] = vec4f( tri_a.xy, 0, 1);
                vertices_projected[count + 1] = vec4f( tri_b.xy, 0, 1);
                vertices_projected[count + 2] = vec4f( tri_c.xy, 0, 1);
            }
        





            // Check intersection
            intersects = IntersectsTriangleAabb(tri_a, tri_b, tri_c, aabb_center, aabb_extents);
            
            // Break loop when we find a triangle that intersects with the current voxel
            if(intersects) {
#ifdef MATERIAL_OVERRIDE_COLOR
                color = _MeshesColor[j];
#endif

#ifdef VERTEX_COLORS
                color = (_VertexColors[count] + _VertexColors[count + 1] + _VertexColors[count + 2]) / 3;
#endif
                break;
            }

            count = count + 3;
        }
        if(intersects) {
            _VoxelColor[u32(id.x + grid_data._GridWidth * (id.y + grid_data._GridHeight * id.z))] = vec4f( color );
            break;
        }
    }
    
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