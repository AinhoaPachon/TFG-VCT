#pragma once

#include "engine.h"
#include "graphics/pipeline.h"
#include "graphics/surface.h"
#include "framework/nodes/light_3d.h"

#include <vector>

class Node;
class MeshInstance3D;
class Environment3D;

// Engine para gestion de escena

class VCTEngine : public Engine {

	MeshInstance3D* floor_grid_mesh = nullptr;

	Environment3D* skybox = nullptr;

public:
	std::vector<Node3D*> entities;
	std::vector<MeshInstance3D*> voxelized_nodes;

	int initialize(Renderer* renderer, GLFWwindow* window, bool use_glfw, bool use_mirror_screen) override;

	void clean() override;

	void update(float delta_time) override;
	void render() override;
};
