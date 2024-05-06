#pragma once

#include "includes.h"
#include <iostream>

#include "graphics/renderer.h"
#include "graphics/texture.h"
#include "graphics/surface.h"

#include "framework/camera/flyover_camera.h"

#include "engine/VCT_engine.h"
#include "voxelization_renderer.h"

class MeshInstance3D;
class Node;

// Si se muestra por pantalla, esta es tu clase bebe
class VCTRenderer : public Renderer {

    VoxelizationRenderer voxelization_renderer;

    Surface         quad_surface;

    struct sCameraData {
        glm::mat4x4 mvp;
        glm::vec3 eye;
        float dummy;
    };

    sCameraData camera_data;
    sCameraData camera_2d_data;

    Uniform  camera_uniform;
    Uniform  camera_2d_uniform;

    uint32_t camera_buffer_stride = 0;

    WGPUCommandEncoder global_command_encoder;

    // Render meshes with material color
    WGPUBindGroup render_bind_group_camera = nullptr;
    WGPUBindGroup render_bind_group_camera_2d = nullptr;

    void render_screen(WGPUTextureView swapchain_view);

    void init_camera_bind_group();  

#if defined(XR_SUPPORT)

    void render_xr();

    // For the XR mirror screen
#if defined(USE_MIRROR_WINDOW)
    void render_mirror(WGPUTextureView swapchain_view);
    void init_mirror_pipeline();

    Pipeline mirror_pipeline;
    Shader* mirror_shader = nullptr;

    std::vector<Uniform> swapchain_uniforms;
    std::vector<WGPUBindGroup> swapchain_bind_groups;
#endif // USE_MIRROR_WINDOW

#endif // XR_SUPPORT

public:

    VCTRenderer();
    void init_voxelization(std::vector<MeshInstance3D*> nodes);

    int initialize(GLFWwindow* window, bool use_mirror_screen = false) override;
    void clean() override;

    void update(float delta_time) override;
    void render() override;

    void resize_window(int width, int height) override;
    inline Uniform* get_current_camera_uniform() { return &camera_uniform; }

};
