#pragma once
#include "includes.h"
#include <iostream>

#include "graphics/renderer.h"

class MeshInstance3D;

class VoxelizationRenderer : public Renderer {

	MeshInstance3D* floor_grid_mesh = nullptr;
    MeshInstance3D* sphere_mesh = nullptr;

    // dimensions of the voxel grid: widht, height and depth
    uint32_t grid_size = 128;

    struct gridData {
        glm::vec4 bounds_min;
        float cell_half_size;
        int grid_width;
        int grid_height;
        int grid_depth;
    } grid_data;

    Pipeline		voxelization_pipeline;
    Shader*         voxelization_shader = nullptr;
    WGPUBindGroup   voxelization_bindgroup = nullptr;

    Shader*         render_voxelization_shader = nullptr;

    Uniform			voxel_gridDataBuffer;
    Uniform			voxel_voxelGridPointsBuffer;

    struct sCameraData {
        glm::mat4x4 mvp;
        glm::vec3 eye;
        float dummy;
    } camera_data;
    Uniform         camera_uniform;

    Pipeline		render_voxelization_pipeline;
    WGPUBindGroup   render_voxelization_bind_group = nullptr;

    void init_compute_voxelization();
    void init_bindings_voxelization_pipeline();
    void onCompute();


    void init_render_voxelization_pipeline();

public:
    VoxelizationRenderer();

    int initialize(GLFWwindow* window, bool use_mirror_screen = false) override;
    void clean() override;

    void update(float delta_time) override;
    void render() override;
    void render_grid(WGPURenderPassEncoder render_pass);

    void resize_window(int width, int height) override;
    inline Uniform* get_current_camera_uniform() { return &camera_uniform; }
    Uniform* get_voxel_grid_points_buffer() { return &voxel_voxelGridPointsBuffer; }

};