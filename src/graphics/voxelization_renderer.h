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
    Surface         quad_surface;
    int             number_triangles;

    struct gridData {
        glm::vec4 bounds_min;
        float cell_half_size;
        int grid_width;
        int grid_height;
        int grid_depth;
    } grid_data;

    struct UBO {
        int width;
        int height;
        int padding0;
        int padding1;
        glm::mat4x4 viewProjectionMatrix;
        glm::mat4x4 model;
    } voxelizer_uniforms;

    struct Uniforms {
        int width;
        int height;
    } render_uniforms; // ideal sería quitarlo but now I can't be bothered with that tbh

    bool material_override_color = false;
    bool vertex_color = true;

    Pipeline		voxelization_pipeline;
    Shader*         voxelization_shader = nullptr;
    WGPUBindGroup   voxelization_bindgroup = nullptr;
    WGPUBindGroup   color_buffer_bindgroup = nullptr;
    WGPUBindGroup   render_color_buffer_bindgroup = nullptr;

    Shader*         render_voxelization_shader = nullptr;

    Uniform			voxel_gridDataBuffer;
    Uniform			voxel_voxelGridPointsBuffer;
    Uniform         voxel_vertexBuffer;
    Uniform         voxel_vertexCount;

    Uniform         colorBuffer;
    Uniform         uniformsBuffer;
    Uniform         renderUniformsBuffer;

    Uniform         voxel_cell_size;

    Uniform         maximumProjectionTest;

    Pipeline		render_voxelization_pipeline;
    WGPUBindGroup   render_voxelization_bind_group = nullptr;
    WGPUBindGroup   voxelization_rasterizer_bind_group = nullptr;

    WGPUCommandEncoder command_encoder;

    void init_compute_voxelization(std::vector<MeshInstance3D*> nodes, Camera* camera);
    void init_bindings_voxelization_pipeline(std::vector<MeshInstance3D*> nodes, Camera* camera);
    void init_bindings_rasterizer(std::vector<MeshInstance3D*> nodes, Camera* camera);
    void on_compute();

    void init_render_pipeline();

public:
    VoxelizationRenderer();

    int initialize(std::vector<MeshInstance3D*> nodes, Camera* camera);
    void clean();

    void update(float delta_time);
    void render_grid(WGPURenderPassEncoder render_pass, WGPUBindGroup render_camera_bind_group, uint32_t camera_buffer_stride = 0);

    void resize_window(int width, int height);
    Uniform* get_voxel_grid_points_buffer() { return &voxel_voxelGridPointsBuffer; }

};