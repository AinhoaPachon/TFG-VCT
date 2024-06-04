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

    struct gridData {
        glm::vec4 bounds_min;
        float cell_half_size;
        int grid_width;
        int grid_height;
        int grid_depth;
    } grid_data;

    bool material_override_color = false;
    bool vertex_color = true;

    struct voxelRepresentation {
        glm::vec4 position;
        glm::vec4 color;
    } voxel_representation;

    Pipeline		voxelization_pipeline;
    Shader* voxelization_shader = nullptr;
    WGPUBindGroup   voxelization_bindgroup = nullptr;

    Shader* render_voxelization_shader = nullptr;

    Uniform			voxel_gridDataBuffer;
    Uniform			voxel_voxelGridPointsBuffer;
    Uniform         voxel_vertexPositionBuffer;
    Uniform         voxel_vertexCount;
    Uniform         voxel_meshCountBuffer;
    Uniform         voxel_voxelColorBuffer;
    Uniform         voxel_meshColorsBuffer;
    Uniform         voxel_vertexColorBuffer;

    Uniform         colorBuffer;

    Uniform         voxel_orthoProjectionMatrix;

    Uniform         voxel_cell_size;

    Pipeline		render_voxelization_pipeline;
    WGPUBindGroup   render_voxelization_bind_group = nullptr;

    void init_compute_voxelization(std::vector<MeshInstance3D*> nodes, Camera* camera);
    void init_bindings_voxelization_pipeline(std::vector<MeshInstance3D*> nodes, Camera* camera);
    void on_compute();

    void init_render_voxelization_pipeline();

public:
    VoxelizationRenderer();

    int initialize(std::vector<MeshInstance3D*> nodes, Camera* camera);
    void clean();

    void update(float delta_time);
    void render();
    void render_grid(WGPURenderPassEncoder render_pass, WGPUBindGroup render_camera_bind_group, uint32_t camera_buffer_stride = 0);

    void resize_window(int width, int height);
    Uniform* get_voxel_grid_points_buffer() { return &voxel_voxelGridPointsBuffer; }

};