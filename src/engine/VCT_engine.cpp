#include "VCT_engine.h"
#include "framework/entities/entity_mesh.h"
#include "framework/entities/entity_text.h"
#include "framework/input.h"
#include "framework/scene/parse_scene.h"
#include "graphics/VCT_renderer.h"

#include <iostream>
#include <fstream>

int VCTEngine::initialize(Renderer* renderer, GLFWwindow* window, bool use_glfw, bool use_mirror_screen)
{
	int error = Engine::initialize(renderer, window, use_glfw, use_mirror_screen);

    EntityMesh* torus = parse_scene("data/meshes/torus/torus.obj");
    torus->scale(glm::vec3(0.25));
    torus->translate(glm::vec3(-1.0f, 0.0, 0.0));
    entities.push_back(torus);

    EntityMesh* cube = parse_scene("data/meshes/cube/cube.obj");
    cube->scale(glm::vec3(0.25));
    cube->translate(glm::vec3(1.0f, 0.0, 0.0));
    entities.push_back(cube);

    EntityMesh* cube2 = parse_scene("data/meshes/cube/cube.obj");
    cube2->scale(glm::vec3(0.25));
    cube2->translate(glm::vec3(4.0f, 0.0, 0.0));
    entities.push_back(cube2);

    TextEntity* text = new TextEntity("oppenheimer vs barbie");
    text->set_material_color(colors::GREEN);
    text->set_scale(0.25f)->generate_mesh();
    text->translate(glm::vec3(0.0f, 0.0, -5.0));
    entities.push_back(text);

	return error;
}

void VCTEngine::clean()
{
    Engine::clean();
}

void VCTEngine::update(float delta_time)
{
    entities[0]->rotate(0.8f * delta_time, glm::vec3(0.0f, 0.0f, 1.0f));

	Engine::update(delta_time);
}

void VCTEngine::render()
{
	for (auto entity : entities) {
		entity->render();
	}

	Engine::render();
}