#pragma once
#include "includes.h"
#include <iostream>

#include "graphics/texture.h"
#include "graphics/surface.h"
#include "graphics/pipeline.h"

#include "framework/camera/flyover_camera.h"
#include "framework/camera/orbit_camera.h"

class MeshInstance3D;
class Node;

class VoxelizationRenderer {

    MeshInstance3D* floor_grid_mesh = nullptr;
    MeshInstance3D* sphere_mesh = nullptr;

    // dimensions of the voxel grid: widht, height and depth
    uint32_t grid_size = 40;

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


    Pipeline		render_voxelization_pipeline;
    WGPUBindGroup   render_voxelization_bind_group = nullptr;

    void init_compute_voxelization();
    void init_bindings_voxelization_pipeline();
    void on_compute();

    void init_render_voxelization_pipeline();

public:
    VoxelizationRenderer();

    int initialize();
    void clean();

    void update(float delta_time);
    void render();
    void render_grid(WGPURenderPassEncoder render_pass, WGPUBindGroup render_camera_bind_group, Node* entity);

    void resize_window(int width, int height);
    Uniform* get_voxel_grid_points_buffer() { return &voxel_voxelGridPointsBuffer; }

};