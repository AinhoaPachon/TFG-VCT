#include mesh_includes.wgsl

struct ColorBuffer {
  values: array<atomic<u32>>,
};

struct GridData {
    _BoundsMin : vec4f,
    _CellHalfSize : f32,
    _GridWidth : u32,
    _GridHeight : u32,
    _GridDepth : u32
}

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
@group(0) @binding(3) var<storage, read_write> maximumProjectionTest: array<u32>;
@group(1) @binding(0) var<storage, read_write> outputColorBuffer : ColorBuffer;

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

fn get_min_max(v1: vec3<f32>, v2: vec3<f32>, v3: vec3<f32>) -> vec4<f32> {
    var min_max = vec4<f32>();
    min_max.x = min(min(v1.x, v2.x), v3.x);
    min_max.y = min(min(v1.y, v2.y), v3.y);
    min_max.z = max(max(v1.x, v2.x), v3.x);
    min_max.w = max(max(v1.y, v2.y), v3.y);

    return min_max;
}

fn color_pixel(x: u32, y: u32, r: u32, g: u32, b: u32) {
    let pixelID = u32(x + y * uniforms.gridWidth) * 4u;
  
    atomicStore(&outputColorBuffer.values[pixelID + 0u], r);
    atomicStore(&outputColorBuffer.values[pixelID + 1u], g);
    atomicStore(&outputColorBuffer.values[pixelID + 2u], b);
}

fn draw_triangle(v1: vec3<f32>, v2: vec3<f32>, v3: vec3<f32>, ind: u32) {
    let min_max = get_min_max(v1, v2, v3);
    let startX = u32(min_max.x);
    let startY = u32(min_max.y);
    let endX = u32(min_max.z);
    let endY = u32(min_max.w);

    for (var x: u32 = startX; x <= endX; x = x + 1u) {
        for (var y: u32 = startY; y <= endY; y = y + 1u) {
            let bc = barycentric(v1, v2, v3, vec2<f32>(f32(x), f32(y))); 
            let color = (bc.x * v1.z + bc.y * v2.z + bc.z * v3.z) * 2.0;// * 50.0 - 400.0;

            // let R = color;
            // let G = color;
            // let B = color;
            var R: u32;
            var G: u32;
            var B: u32;
            if (ind == 0) {
                R = 255;
                G = 0;
                B = 0;
            } else if (ind == 1) {
                R = 0;
                G = 255;
                B = 0;
            } else { 
                R = 0;
                G = 0;
                B = 255;
            }
            // let R = 255;
            // let G = 0;
            // let B = 0;

            if (bc.x < 0.0 || bc.y < 0.0 || bc.z < 0.0) {
                continue;
            }

            // Remember to multiply by 255 when not storing depth
            color_pixel(x, y, u32(R), u32(G), u32(B));
        }
    }
}

fn draw_line(v1: vec3<f32>, v2: vec3<f32>) {
    let v1Vec = vec2<f32>(v1.x, v1.y);
    let v2Vec = vec2<f32>(v2.x, v2.y);

    let dist = i32(distance(v1Vec, v2Vec));
    for (var i = 0; i < dist; i = i + 1) {
        let x = u32(v1.x + f32(v2.x - v1.x) * (f32(i) / f32(dist)));
        let y = u32(v1.y + f32(v2.y - v1.y) * (f32(i) / f32(dist)));
        color_pixel(x, y, 255u, 255u, 255u);
    }
}

fn project(vertex: Vertex) -> vec3<f32> {

    var worldPos = uniforms.modelMatrix * vec4<f32>(vertex.position, 1.0);
    var projection = uniforms.viewProjectionMatrix * worldPos;
    
    var worldPos = uniforms.modelMatrix * vec4<f32>(vertex.position, 1.0);
    var projection = uniforms.viewProjectionMatrix * worldPos;
    
    var worldPos = uniforms.modelMatrix * vec4<f32>(vertex.position, 1.0);
    var projection = uniforms.viewProjectionMatrix * worldPos;
    

    let p1 = v2.position - v1.position;
    let p2 = v3.position - v1.position;
    let p = abs(cross(p1, p2));

    for (var i = 0; i < 3; ++i) {
        var dominantAxis: u32;
        if(p.z > p.x && p.z > p.y)
        {
            dominantAxis = 2;
        } else if (p.x > p.y && p.x > p.z) {
            dominantAxis = 0;
        } else {
            dominantAxis = 1;
        }
    }



    // For orthographic
    // var clip_space = projection;

    // For perspective
    var clip_space = vec4f(projection.xyz / projection.w, projection.w);

    clip_space.x = clip_space.x * 0.5 + 0.5;
    clip_space.y = clip_space.y * 0.5 + 0.5;

    clip_space.x = clip_space.x * f32(uniforms.gridWidth);
    clip_space.y = (1.0 - clip_space.y) * f32(uniforms.gridHeight);
    clip_space.z = clip_space.z * f32(uniforms.gridDepth);

    return vec3<f32>(clip_space.x, clip_space.y, clip_space.w);
}

fn is_off_screen(v: vec3<f32>) -> bool {
    if (v.x < 0.0 || v.x > f32(uniforms.gridWidth) || v.y < 0.0 || v.y > f32(uniforms.gridHeight)) {
        return true;
    }

    return false;
}

fn select_dominant_axis(v1: Vertex, v2: Vertex, v3: Vertex) -> u32 {
    let p1 = v2.position - v1.position;
    let p2 = v3.position - v1.position;
    let p = abs(cross(p1, p2));

    var dominantAxis: u32;
    if(p.z > p.x && p.z > p.y)
    {
        dominantAxis = 2;
    } else if (p.x > p.y && p.x > p.z) {
        dominantAxis = 0;
    } else {
        dominantAxis = 1;
    }

    return dominantAxis;
}

@compute @workgroup_size(1, 1)
fn compute(@builtin(global_invocation_id) global_id : vec3<u32>) {
    let index = global_id.x * 3u;
    
    let v1 = project(vertexBuffer.values[index + 0u]);
    let v2 = project(vertexBuffer.values[index + 1u]);
    let v3 = project(vertexBuffer.values[index + 2u]);

    if (is_off_screen(v1) || is_off_screen(v2) || is_off_screen(v3)) {
        return;
    }

    var vertex_count : u32 = vertexCount;

    maximumProjectionTest[global_id.x] = select_dominant_axis(vertexBuffer.values[index + 0u],
                                                            vertexBuffer.values[index + 1u],
                                                            vertexBuffer.values[index + 2u]);

    draw_triangle(v1, v2, v3, maximumProjectionTest[global_id.x]); //ahora esto lo ignoraremos y pasaremos a non-conservative rasterization
}


// @compute @workgroup_size(256, 1)
// fn clear(@builtin(global_invocation_id) global_id : vec3<u32>) {
//     let index = global_id.x * 3u;

//     atomicStore(&outputColorBuffer.values[index + 0u], 255u);
//     atomicStore(&outputColorBuffer.values[index + 1u], 255u);
//     atomicStore(&outputColorBuffer.values[index + 2u], 255u);
// }