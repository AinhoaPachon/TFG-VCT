#include "VCT_engine.h"
#include "framework/nodes/mesh_instance_3d.h"
#include "framework/nodes/text.h"
#include "framework/input.h"
#include "framework/scene/parse_scene.h"
#include "framework/nodes/environment_3d.h"

#include "graphics/VCT_renderer.h"

#include <iostream>
#include <fstream>


int VCTEngine::initialize(Renderer* renderer, GLFWwindow* window, bool use_glfw, bool use_mirror_screen)
{

	int error = Engine::initialize(renderer, window, use_glfw, use_mirror_screen);

	floor_grid_mesh = new MeshInstance3D();
	floor_grid_mesh->add_surface(RendererStorage::get_surface("quad"));
	floor_grid_mesh->set_translation(glm::vec3(0.0f));
	floor_grid_mesh->rotate(glm::radians(90.0f), glm::vec3(0.0f, 0.0f, 0.0f));
	floor_grid_mesh->scale(glm::vec3(3.f));

	skybox = new Environment3D();

	entities.push_back(skybox);

	MeshInstance3D* teapot = parse_mesh("data/meshes/teapot.obj");
	teapot->scale(glm::vec3(1));
	teapot->translate(glm::vec3(0.0f, 0.0f, 0.0f));
	entities.push_back(teapot);

	MeshInstance3D* monkey = parse_mesh("data/meshes/monkey.obj");
	monkey->scale(glm::vec3(1));
	monkey->translate(glm::vec3(0.0f, 0.0f, 2.0f));
	entities.push_back(monkey);

	//Material grid_material;
	//render_voxelization_shader = RendererStorage::get_shader("data/shaders/draw_voxel_grid.wgsl");
	////render_voxelization_shader = RendererStorage::get_shader("data/shaders/mesh_grid.wgsl");
	//grid_material.shader = render_voxelization_shader;
	//grid_material.transparency_type = ALPHA_BLEND;

	//floor_grid_mesh->set_surface_material_override(floor_grid_mesh->get_surface(0), grid_material);

	return error;
}

void VCTEngine::clean()
{
    Engine::clean();
}

void VCTEngine::update(float delta_time)
{
	Engine::update(delta_time);

    //entities[0]->rotate(0.8f * delta_time, glm::vec3(0.0f, 0.0f, 1.0f));
	for (auto entity : entities) {
		entity->update(delta_time);
	}

}

void VCTEngine::render()
{
	for (auto entity : entities) {
		entity->render();
	}

	Engine::render();
}
