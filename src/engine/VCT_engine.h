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

	Environment3D* skybox = nullptr;

public:
	std::vector<Node*> entities;

	int initialize(Renderer* renderer, GLFWwindow* window, bool use_glfw, bool use_mirror_screen) override;

	void fill_entities();

	void clean() override;

	void update(float delta_time) override;
	void render() override;
};
