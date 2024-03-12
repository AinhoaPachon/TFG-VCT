#pragma once

#include "includes.h"
#include <iostream>

#include "graphics/renderer.h"
#include "graphics/texture.h"
#include "graphics/surface.h"

#include "framework/camera/flyover_camera.h"

#include "engine/VCT_engine.h"

class MeshInstance3D;

// Si se muestra por pantalla, esta es tu clase bebe

class VCTRenderer : public Renderer {

    Surface         quad_surface;

    struct sCameraData {
        glm::mat4x4 mvp;
        glm::vec3 eye;
        float dummy;
    } camera_data;
    Uniform         camera_uniform;

    WGPUBindGroup   render_camera_bind_group = nullptr;

    void render_screen();

    void init_camera_bind_group();  

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
    void render_grid(WGPURenderPassEncoder render_pass);

    void resize_window(int width, int height) override;
    inline Uniform* get_current_camera_uniform() { return &camera_uniform; }

};
