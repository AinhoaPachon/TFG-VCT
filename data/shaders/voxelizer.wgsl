#include mesh_includes.wgsl

struct ColorBuffer {
  values: array<atomic<u32>>,
};

struct UBO {
    gridWidth: u32,
    gridHeight: u32,
    gridDepth : u32,
    padding1 : u32,
    viewProjectionMatrix: mat4x4<f32>,
    modelMatrix: mat4x4<f32>
};

struct Vertex { 
    position: vec3f,
    padding: f32,
    uv: vec2f,
    padding2: vec2f,
    normal: vec3f,
    padding3: f32,
    tangent: vec3f,
    padding4: f32,
    color: vec3f,
    padding5: f32,
    weights: vec4f,
    joints: vec4f
};

struct VertexBuffer {
    values: array<Vertex>,
};

@group(0) @binding(0) var<storage, read_write> vertexBuffer : VertexBuffer;
@group(0) @binding(1) var<storage, read_write> vertexCount: u32;
@group(0) @binding(2) var<uniform> uniforms : UBO;
@group(1) @binding(0) var<storage, read_write> outputColorBuffer : ColorBuffer;
@group(1) @binding(1) var texture3D: texture_storage_3d<rgba16uint, read_write>;

// From: https://github.com/ssloy/tinyrenderer/wiki/Lesson-2:-Triangle-rasterization-and-back-face-culling
fn barycentric(v1: vec3<f32>, v2: vec3<f32>, v3: vec3<f32>, p: vec2<f32>) -> vec3<f32> {
    let u = cross(
        vec3<f32>(v3.x - v1.x, v2.x - v1.x, v1.x - p.x), 
        vec3<f32>(v3.y - v1.y, v2.y - v1.y, v1.y - p.y)
    );

    if (abs(u.z) < 1.0) {
        return vec3<f32>(-1.0, 1.0, 1.0);
    }

    return vec3<f32>(1.0 - (u.x+u.y)/u.z, u.y/u.z, u.x/u.z); 
}

fn get_min_max(v1: vec4<f32>, v2: vec4<f32>, v3: vec4<f32>) -> array<f32, 6> {
    var min_max =  array<f32, 6>();
    min_max[0] = min(min(v1.x, v2.x), v3.x);
    min_max[1] = min(min(v1.y, v2.y), v3.y);
    min_max[2] = min(min(v1.z, v2.z), v3.z);
    min_max[3] = max(max(v1.x, v2.x), v3.x);
    min_max[4] = max(max(v1.y, v2.y), v3.y);
    min_max[5] = max(max(v1.z, v2.z), v3.z);

    return min_max;
}

fn color_pixel(screen_coords: vec3<u32>, r: u32, g: u32, b: u32) {
    // let pixelID = u32(x + y * uniforms.gridWidth) * 4u;
  
    // // aquí se guardará en la imagen
    // atomicStore(&outputColorBuffer.values[pixelID + 0u], r);
    // atomicStore(&outputColorBuffer.values[pixelID + 1u], g);
    // atomicStore(&outputColorBuffer.values[pixelID + 2u], b);

    textureStore(texture3D, screen_coords, vec4<u32>(u32(r), u32(g), u32(b), 1u));
}

fn draw_triangle(triangleVertices: array<vec4f, 3>) {
    let min_max = get_min_max(triangleVertices[0], triangleVertices[1], triangleVertices[2]);
    let startX = u32(min_max[0]);
    let startY = u32(min_max[1]);
    let startZ = u32(min_max[2]);
    let endX = u32(min_max[3]);
    let endY = u32(min_max[4]);
    let endZ = u32(min_max[5]);

    for (var x: u32 = startX; x <= endX; x = x + 1u) {
        for (var y: u32 = startY; y <= endY; y = y + 1u) {
            for (var z: u32 = startZ; z <= endZ; z = z + 1u) {
                // let bc = barycentric(v1, v2, v3, vec2<f32>(f32(x), f32(y))); 
                // let color = (bc.x * v1.z + bc.y * v2.z + bc.z * v3.z) * 2.0;// * 50.0 - 400.0;

                var R: u32;
                var G: u32;
                var B: u32;

                R = 255;
                G = 0;
                B = 0;

                // if (ind == 0) {
                //     R = 255;
                //     G = 0;
                //     B = 0;
                // } else if (ind == 1) {
                //     R = 0;
                //     G = 255;
                //     B = 0;
                // } else { 
                //     R = 0;
                //     G = 0;
                //     B = 255;
                // }

                // if (bc.x < 0.0 || bc.y < 0.0 || bc.z < 0.0) {
                //     continue;
                // }

                // Remember to multiply by 255 when not storing depth
                color_pixel(vec3<u32>(x, y, z), R, G, B);
            }
        }
    }
}

fn project(v: Vertex) -> vec4f {
    let worldPos = uniforms.modelMatrix * vec4f(v.position, 1.0);
    return worldPos;
}

fn screen_space(clip_space: vec3f) -> vec3f {
    var screen_space: vec3f;
    screen_space.x = clip_space.x * 0.5 + 0.5;
    screen_space.y = clip_space.y * 0.5 + 0.5;
    screen_space.z = clip_space.z * 0.5 + 0.5;

    screen_space.x = screen_space.x * f32(uniforms.gridWidth);
    screen_space.y = (1.0 - screen_space.y) * f32(uniforms.gridHeight);
    screen_space.z = screen_space.z * f32(uniforms.gridDepth);

    return screen_space;
}

fn is_off_screen(v: vec3<f32>) -> bool {
    if (v.x < 0.0 || v.x > f32(uniforms.gridWidth) || v.y < 0.0 || v.y > f32(uniforms.gridHeight)) {
        return true;
    }

    return false;
}

fn select_dominant_axis(verticesPrevTriangle: array<vec4f, 3>) -> array<vec4f, 3> {
    let p1 = verticesPrevTriangle[1] - verticesPrevTriangle[0];
    let p2 = verticesPrevTriangle[2] - verticesPrevTriangle[2];
    let p = abs(cross(p1.xyz, p2.xyz));

    var verticesFinalTriangle: array<vec4f, 3>;
    var position: vec4f;
    for(var i: u32 = 0; i < 3; i++){
        let worldPositionFrag = verticesPrevTriangle[i];
        if(p.z > p.x && p.z > p.y)
        {
            position = vec4f(worldPositionFrag.x, worldPositionFrag.y, 0, 1);
        } else if (p.x > p.y && p.x > p.z) {
            position = vec4f(worldPositionFrag.y, worldPositionFrag.z, 0, 1);
        } else {
            position = vec4f(worldPositionFrag.x, worldPositionFrag.z, 0, 1);
        }
        verticesFinalTriangle[i] = position;
    }
    return verticesFinalTriangle;
}

@compute @workgroup_size(1, 1)
fn compute(@builtin(global_invocation_id) global_id : vec3<u32>) {
    let index = global_id.x * 3u;
    
    // Get vertex position in global coordinates
    let v1 = project(vertexBuffer.values[index + 0u]);
    let v2 = project(vertexBuffer.values[index + 1u]);
    let v3 = project(vertexBuffer.values[index + 2u]);

    // Get vertex of the triangle projected onto the dominant axis
    // Those vertices will be sent to be drawn 
    var verticesTriangle : array<vec4f, 3> = array<vec4f, 3>(v1, v2, v3);
    var verticesProjectedTriangle : array<vec4f, 3> = select_dominant_axis(verticesTriangle);

    for(var i: u32 = 0; i < 3; i++){
        var temp: vec3f = screen_space(verticesProjectedTriangle[i].xyz);
        verticesProjectedTriangle[i] = vec4f(temp, 1.0);
    }

    // Convert to clip space
    draw_triangle(verticesProjectedTriangle); //ahora esto lo ignoraremos y pasaremos a non-conservative rasterization


    // if (is_off_screen(v1) || is_off_screen(v2) || is_off_screen(v3)) {
    //     return;
    // }

    var vertex_count : u32 = vertexCount;

}


// @compute @workgroup_size(256, 1)
// fn clear(@builtin(global_invocation_id) global_id : vec3<u32>) {
//     let index = global_id.x * 3u;

//     atomicStore(&outputColorBuffer.values[index + 0u], 255u);
//     atomicStore(&outputColorBuffer.values[index + 1u], 255u);
//     atomicStore(&outputColorBuffer.values[index + 2u], 255u);
// }