#pragma once

#include "includes.h"
#include <iostream>

#include "graphics/renderer.h"
#include "graphics/texture.h"
#include "graphics/surface.h"

#include "mesh_renderer.h"

#include "framework/camera/flyover_camera.h"

#include "engine/VCT_engine.h"


//#ifdef __EMSCRIPTEN__
#define DISABLE_RAYMARCHER
//#endif

// Si se muestra por pantalla, esta es tu clase bebe

class VCTRenderer : public Renderer {

    MeshRenderer    mesh_renderer;

    Surface         quad_surface;

    struct sCameraData {
        glm::mat4x4 mvp;
        glm::vec3 eye;
        float dummy;
    } camera_data;
    Uniform         camera_uniform;

    Pipeline		render_voxelization_pipeline;
    Shader*         render_voxelization_shader = nullptr;
    WGPUBindGroup   render_voxelization_bindgroup = nullptr;
    WGPUBindGroup   render_voxel_grid_bindgroup = nullptr;

    struct sRenderMeshData {
        glm::mat4x4 model;
        glm::vec4 color;
    } mesh_data;

    struct sInstanceData {
        std::array < sRenderMeshData, sizeof(dynamic_cast<VCTEngine*>(VCTEngine::instance)->entities)> data;
    } instance_data;

    Uniform			voxel_meshDataBuffer;
    Uniform			voxel_cameraDataBuffer;

    // Render to screen
    Pipeline        render_quad_pipeline = {};
    Shader*         render_quad_shader = nullptr;

    Texture         eye_textures[EYE_COUNT] = {};
    Texture         eye_depth_textures[EYE_COUNT] = {};

    Uniform         eye_render_texture_uniform[EYE_COUNT] = {};

    WGPUBindGroup   eye_render_bind_group[EYE_COUNT] = {};
    WGPUTextureView eye_depth_texture_view[EYE_COUNT] = {};

    void render_eye_quad(WGPUTextureView swapchain_view, WGPUTextureView swapchain_depth, WGPUBindGroup bind_group);
    void render_screen();

    void render_3D_grid();

    void init_render_quad_pipeline();
    void init_render_quad_bind_groups();

    void init_camera_bind_group();

    void init_render_voxelization_pipeline();

#if defined(XR_SUPPORT)

    void render_xr();

    // For the XR mirror screen
#if defined(USE_MIRROR_WINDOW)
    void render_mirror();
    void init_mirror_pipeline();

    Pipeline mirror_pipeline;
    Shader* mirror_shader = nullptr;

    std::vector<Uniform> swapchain_uniforms;
    std::vector<WGPUBindGroup> swapchain_bind_groups;
#endif // USE_MIRROR_WINDOW

#endif // XR_SUPPORT

public:

    VCTRenderer();

    int initialize(GLFWwindow* window, bool use_mirror_screen = false) override;
    void clean() override;

    void update(float delta_time) override;
    void render() override;

    void resize_window(int width, int height) override;

    Texture* get_eye_texture(eEYE eye);

};
