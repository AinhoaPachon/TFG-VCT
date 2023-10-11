#pragma once

#include "engine.h"

class VCTEngine : public Engine {

    std::vector<Entity*> entities;

public:

	int initialize(Renderer* renderer, GLFWwindow* window, bool use_glfw, bool use_mirror_screen) override;
    void clean() override;

	void update(float delta_time) override;
	void render() override;
};
