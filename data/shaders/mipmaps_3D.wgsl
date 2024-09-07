@group(0) @binding(0) var previousMipLevel: texture_3d<f32>;

#ifdef RGBA8_UNORM
@group(0) @binding(1) var nextMipLevel: texture_storage_3d<rgba8unorm, write>;
#endif

#ifdef RGBA32_FLOAT
@group(0) @binding(1) var nextMipLevel: texture_storage_3d<rgba32float, write>;
#endif

@group(0) @binding(2) var texture_sampler : sampler;

// https://www.gamedev.net/forums/topic/709862-downsampling-image-in-compute-shader/
@compute @workgroup_size(8, 8, 8)
fn compute(@builtin(global_invocation_id) id: vec3<u32>) {

    let dim : vec3u = textureDimensions(nextMipLevel).xyz;

    let uv : vec3f = (vec3f(id.xyz) + vec3f(0.5)) / vec3f(dim);
    let color : vec4f = textureSampleLevel(previousMipLevel, texture_sampler, uv, 0.0f);

    textureStore(nextMipLevel, id.xyz, color);
}