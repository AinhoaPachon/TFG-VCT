#pragma once

#include "engine.h"
#include "graphics/pipeline.h"
#include <vector>

class Entity;

// Engine para gestion de escena

class VCTEngine : public Engine {

    std::vector<Entity*> entities;

	// dimensions of the voxel grid: widht, height and depth
	uint32_t grid_size;

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

	Uniform			voxel_voxelGridPointsBuffer;
	Uniform			voxel_gridDataBuffer;

public:

	int initialize(Renderer* renderer, GLFWwindow* window, bool use_glfw, bool use_mirror_screen) override;
	void init_compute_voxelization();
	void init_bindings_voxelization_pipeline();

	void onCompute();

	void clean() override;

	void update(float delta_time) override;
	void render() override;

private:

	struct meshData {

	};

	struct cameraData {

	};
};
