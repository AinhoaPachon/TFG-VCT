#pragma once

#include "engine.h"
#include "graphics/pipeline.h"
#include "graphics/surface.h"

#include <vector>

class Node;
class MeshInstance3D;
class Environment3D;

// Engine para gestion de escena

class VCTEngine : public Engine {

	MeshInstance3D* floor_grid_mesh = nullptr;

	// dimensions of the voxel grid: widht, height and depth
	uint32_t grid_size = 128;

	Environment3D* skybox = nullptr;

	struct gridData {
		glm::vec4 bounds_min;
		float cell_half_size;
		int grid_width;
		int grid_height;
		int grid_depth;
	} grid_data;
	
	Pipeline		voxelization_pipeline;
	Shader*			voxelization_shader = nullptr;
	WGPUBindGroup   voxelization_bindgroup = nullptr;

	Shader*			render_voxelization_shader = nullptr;

	Uniform			voxel_gridDataBuffer;
	Uniform			voxel_voxelGridPointsBuffer;

public:
	std::vector<Node*> entities;

	int initialize(Renderer* renderer, GLFWwindow* window, bool use_glfw, bool use_mirror_screen) override;

	void fill_entities();

	void init_compute_voxelization();
	void init_bindings_voxelization_pipeline();

	Uniform* get_voxel_grid_points_buffer() { return &voxel_voxelGridPointsBuffer; }

	void onCompute();

	void clean() override;

	void update(float delta_time) override;
	void render() override;
};
