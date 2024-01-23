#pragma once

#include "includes.h"
#include <iostream>

#include "graphics/renderer.h"
#include "graphics/texture.h"

#include "mesh_renderer.h"

//#ifdef __EMSCRIPTEN__
#define DISABLE_RAYMARCHER
//#endif

class VCTRenderer : public Renderer {

    MeshRenderer    mesh_renderer;

    Mesh            quad_mesh;
    Surface         quad_surface;

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

    void init_render_quad_pipeline();
    void init_render_quad_bind_groups();

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
