#pragma once

#include "engine.h"

class VCTEngine : public Engine {

    std::vector<Entity*> entities;
	
	Pipeline		compute_voxelization_pipeline;
	Shader*			compute_voxelization_shader = nullptr;
	WGPUBindGroup   compute_voxelization_bindgroup = nullptr;

	Uniform			compute_inputBuffer;
	Uniform			compute_outputBuffer;
	Uniform			compute_mapBuffer;

public:

	int initialize(Renderer* renderer, GLFWwindow* window, bool use_glfw, bool use_mirror_screen) override;
	void init_compute_voxelization();
	void init_bindings_voxelization_pipeline();

	void onCompute();

	void clean() override;

	void update(float delta_time) override;
	void render() override;
};
