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

    Surface         quad_surface;
    int             number_triangles;

    struct UBO {
        uint32_t grid_width = 256;
        uint32_t grid_height = 256;
        uint32_t grid_depth = 256;
        int padding1;
        glm::mat4x4 viewProjectionMatrix;
        glm::mat4x4 model;
    } voxelizer_uniforms;

    struct Uniforms {
        int width;
        int height;
    } render_uniforms; // ideal sería quitarlo but now I can't be bothered with that tbh

    struct SurfaceRasterData {
        WGPUBindGroup  voxelization_bindgroup = nullptr;
        Uniform        voxel_vertexBuffer;
        Uniform        voxel_colorBuffer;
        int            number_triangles;
    } raster_data;

    struct PointLight {
        glm::vec4   position;
        glm::vec4   color;
        float       intensity;
        float       padding0;
        float       padding1;
        float       padding2;
    } light;

    std::vector<SurfaceRasterData*> voxelization_RasterData;

    bool material_override_color = false;
    bool vertex_color = true;

    Pipeline		voxelization_pipeline;
    Shader*         voxelization_shader = nullptr;
    WGPUBindGroup   color_buffer_bindgroup = nullptr;
    WGPUBindGroup   render_color_buffer_bindgroup = nullptr;
    WGPUBindGroup   render_lights_buffer_bindgroup = nullptr;

    Shader*         render_voxelization_shader = nullptr;

    Uniform         colorBuffer;
    Uniform         uniformsBuffer;
    Uniform         renderUniformsBuffer;

    Uniform         textureBuffer;
    Uniform         voxel_vertexBuffer;
    Uniform         lightBuffer;

    Texture         texture3D;
    uint32_t        mipmap_count = 1;

    Pipeline		render_voxelization_pipeline;
    WGPUBindGroup   render_voxelization_bind_group = nullptr;

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

};