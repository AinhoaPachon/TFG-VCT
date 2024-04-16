#include mesh_includes.wgsl
var PI: f32 = 3.14159265359;

struct VoxelRepresentation {
    _WorldPos : vec4f,
    _N : vec3f,
    _R : vec3f,
    _aperture : f32
}

fn coneTrace() -> vec3f {
    var start: vec3f = VoxelRepresentation._WorldPos + VOXEL_OFFSET_CORRECTION_FACTOR * VOXEL_SIZE * VoxelRepresentation._N;

    var Lv: vec4f = vec4f(0.0f);

    var tan_half_aperture: f32 = tan(VoxelRepresentation._aperture / 2.0f);
    var tan_eigth_aperture: f32 = tan(VoxelRepresentation._aperture / 8.0f);
    var step_size_correction_factor: f32 = (1.0f + tan_eigth_aperture) / (1.0f - tan_eigth_aperture);
    var step: f32 = step_size_correction_factor * VOXEL_SIZE / 2.0f;

    var distance: f32 = step;

    for(var i: u32 = 0; i < NUM_STEPS && Lv.a <= 0.9f; ++i) {
        var position: vec3f = start + distance * direction;
        if (!isInsideUnitCube(position)) {
            break;
        }
        position = position * 0.5f + 0.5f;

        var diameter: f32 = 2.0f * tan_half_aperture * diameter;
        var mip_level: f32 = log2(diameter / VOXEL_SIZE);
        var Lv_step: vec4f = 100.0f * step * textureLod(texture3D, position, mip_level);
        if (Ls_step.a > 0.0f) {
            Lv_step.rgb /= Lv_step.a;

            // Alpha blending
            Lv.rgb += (1.0f - Lv.a) * Lv_step.a * Lv_step.rgb;
            Lv.a += (1.0f - Lv.a) * Lv_step.a;
        }
        distance += step;
    }
    return Lv.rgb;
}

fn calc_indirect_diffuse_lighting() -> vec3f {

    var T: vec3f = cross(VoxelRepresentation._N, vec3f(0.0f, 1.0f, 0.0f));
    var B: vec3f = cross(T, N);

    var Lo: vec3f = vec3f(0.0f);

    var aperture: f32 = PI / 3.0f;
    var direction: vec3f = VoxelRepresentation._N;
    Lo += coneTrace(direction, aperture);

    // Rotate the tangent vector about the normal using the 5th root
    direction = 0.7071f * VoxelRepresentation._N + 0.7071f * T;
    Lo += coneTrace(direction, aperture);

    direction = 0.7071f * VoxelRepresentation._N + 0.7071f * (0.309f * T + 0.951f * B);
    Lo += coneTrace(direction, aperture);
    direction = 0.7071f * VoxelRepresentation._N + 0.7071f * (-0.809f * T + 0.588f * B);
    Lo += coneTrace(direction, aperture);
    direction = 0.7071f * VoxelRepresentation._N - 0.7071f * (-0.809f * T - 0.588f * B);
    Lo += coneTrace(direction, aperture);
    direction = 0.7071f * VoxelRepresentation._N - 0.7071f * (0.309f * T - 0.951f * B);
    Lo += coneTrace(direction, aperture);

    return Lo / 6.0f;
}

fn calc_indirect_specular_lighting() -> vec3f {
    return coneTrace(R, aperture);
}

@group(0) @binding(1) var<storage, read> _VoxelGridPoints: array<vec4f>;
@group(0) @binding(2) var<uniform> cellSize: f32;

@group(1) @binding(0) var<uniform> camera_data : CameraData;

@vertex
fn vs_main(in: VertexInput) -> VertexOutput {
    
    var out: VertexOutput;
    var localPos: vec4f = _VoxelGridPoints[in.instance_id];
    out.position = camera_data.view_projection * vec4f(localPos.xyz + in.position * cellSize, 1.0);
    out.color = vec4f(0.5, 0.0, 0.9, localPos.w);
    out.normal = localPos.rgb;
    return out; 
}

struct FragmentOutput {
    @location(0) color: vec4f
}

@fragment
fn fs_main(in: VertexOutput) -> FragmentOutput {

    let eye: vec3f = normalize(camera_data.eye);

    var out: FragmentOutput;
    if (in.color.w == 0.0) {
        discard;
    }
    out.color = vec4f(in.color);
    return out;
}