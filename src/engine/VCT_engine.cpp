#include "VCT_engine.h"
#include "framework/nodes/mesh_instance_3d.h"
#include "framework/nodes/text.h"
#include "framework/input.h"
#include "framework/scene/parse_scene.h"
#include "framework/nodes/environment_3d.h"
#include "framework/nodes/omni_light_3d.h"


#include "graphics/renderer_storage.h"
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
	
	MeshInstance3D* monkey = parse_mesh("data/meshes/monkey.obj");
	monkey->scale(glm::vec3(0.5f));
	monkey->translate(glm::vec3(0.0f, 0.0f, 0.0f));
	entities.push_back(monkey);

	MeshInstance3D* monkey2 = parse_mesh("data/meshes/monkey.obj");
	monkey2->scale(glm::vec3(1));
	monkey2->translate(glm::vec3(2.0f, 0.0f, 0.0f));
	entities.push_back(monkey2);

	//Surface* surface = monkey->get_surface(0);
	//surface->set_material_color(glm::vec4(0.5f, 0.4f, 0.0f, 1.0f));

	Material material;
	material.color = glm::vec4(0.7f, 0.0f, 0.5f, 1.0f);
	material.shader = RendererStorage::get_shader("data/shaders/mesh_pbr.wgsl", material);
	material.flags = 1;
	monkey->set_surface_material_override(monkey->get_surface(0), material);

	material.color = glm::vec4(0.0f, 0.4f, 0.9f, 1.0f);
	material.shader = RendererStorage::get_shader("data/shaders/mesh_pbr.wgsl", material);
	material.flags = 1;
	material.emissive = glm::vec3(0.6f, 0.2f, 0.45f);
	monkey2->set_surface_material_override(monkey->get_surface(0), material);

	voxelized_nodes.push_back(monkey);
	voxelized_nodes.push_back(monkey2);

	Light3D* light = new OmniLight3D();
	light->set_intensity(5.0f);
	light->set_translation(glm::vec3(2.0f, 2.0f, 2.0f));
	light->set_color({ 1.0f, 1.0f, 1.0f });
	light->set_range(5.0f);
	entities.push_back(light);

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

	for (auto entity : entities) {
		entity->update(delta_time);
	}

}

void VCTEngine::render()
{
	// Lights
	{
		for (auto entity : entities)
		{
			Light3D* light_node = dynamic_cast<Light3D*>(entity);

			if (!light_node)
			{
				continue;
			}
			if (light_node->get_intensity() < 0.001f)
			{
				continue;
			}
			if (light_node->get_type() != LIGHT_DIRECTIONAL && light_node->get_range() == 0.0f)
			{
				continue;
			}

			VCTRenderer::instance->add_light(light_node);
		}
	}

	for (auto entity : entities) {
		entity->render();
	}

	Engine::render();
}
