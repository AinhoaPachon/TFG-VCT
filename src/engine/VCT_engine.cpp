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

	grid_size = 128;

    /*
    TextEntity* text = new TextEntity("oppenheimer vs barbie");
    text->set_material_color(colors::GREEN);
    text->set_scale(0.25f)->generate_mesh();
    text->translate(glm::vec3(0.0f, 0.0, -5.0));
    entities.push_back(text);*/

	/*init_compute_voxelization();
	init_bindings_voxelization_pipeline();
	onCompute();*/

	floor_grid_mesh = new EntityMesh();
	floor_grid_mesh->add_surface(RendererStorage::get_surface("quad"));
	floor_grid_mesh->set_translation(glm::vec3(0.0f));
	floor_grid_mesh->rotate(glm::radians(90.0f), glm::vec3(1.0f, 0.0f, 0.0f));
	floor_grid_mesh->scale(glm::vec3(3.f));

	Material grid_material;
	render_voxelization_shader = RendererStorage::get_shader("data/shaders/draw_voxel_grid.wgsl");
	//render_voxelization_shader = RendererStorage::get_shader("data/shaders/mesh_grid.wgsl");
	grid_material.shader = render_voxelization_shader;
	grid_material.flags |= MATERIAL_TRANSPARENT;

	floor_grid_mesh->set_surface_material_override(floor_grid_mesh->get_surface(0), grid_material);

	return error;
}

void VCTEngine::fill_entities()
{
	EntityMesh* torus = parse_mesh("data/meshes/torus/torus.obj");
	torus->scale(glm::vec3(0.25));
	torus->translate(glm::vec3(1.0f, 0.0, 0.0));
	entities.push_back(torus);
}

void VCTEngine::init_compute_voxelization()
{
	// Get the voxelization Shader
	voxelization_shader = RendererStorage::get_shader("data/shaders/voxel_grid_points_fill.wgsl");

    WebGPUContext* webgpu_context = VCTRenderer::instance->get_webgpu_context();

	// Set bindings for the voxelization pipeline
	init_bindings_voxelization_pipeline();

	// set grid_data uniforms and camera_data uniforms
    std::vector<Uniform*> uniforms = { &voxel_voxelGridPointsBuffer, &voxel_gridDataBuffer };
	voxelization_bindgroup = webgpu_context->create_bind_group(uniforms, voxelization_shader, 0);

	voxelization_pipeline.create_compute(voxelization_shader);
}

void VCTEngine::init_bindings_voxelization_pipeline()
{
	RenderdocCapture::start_capture_frame();
	WebGPUContext* webgpu_context = VCTRenderer::instance->get_webgpu_context();

	uint32_t grid_size = 128;
	std::vector<glm::vec4> initial_values;

	grid_data.bounds_min = glm::vec4(0.0, 0.0, 0.0, 0.0);
	grid_data.cell_half_size = 2.0f;
	grid_data.grid_width = grid_data.grid_height = grid_data.grid_depth = grid_size;

	voxel_gridDataBuffer.binding = 0;
	voxel_gridDataBuffer.buffer_size = sizeof(gridData);
	voxel_gridDataBuffer.data = webgpu_context->create_buffer(voxel_gridDataBuffer.buffer_size, WGPUBufferUsage_CopyDst | WGPUBufferUsage_Uniform, &grid_data, "grid data buffer");

	//for (int i = 0; i < grid_size * grid_size * grid_size; ++i) {
	//	initial_values.push_back(glm::vec4(0.0, 0.0, 0.0, 0.0));
	//}

	voxel_voxelGridPointsBuffer.binding = 1;
	voxel_voxelGridPointsBuffer.buffer_size = sizeof(glm::vec4) * grid_size * grid_size * grid_size;
	voxel_voxelGridPointsBuffer.data = webgpu_context->create_buffer(voxel_voxelGridPointsBuffer.buffer_size, WGPUBufferUsage_CopyDst | WGPUBufferUsage_Storage, initial_values.data(), "grid points buffer");
}

void VCTEngine::onCompute()
{
	WebGPUContext* webgpu_context = VCTRenderer::instance->get_webgpu_context();
    WGPUQueue queue = webgpu_context->device_queue;

	WGPUCommandEncoderDescriptor encorderDesc = {};
	WGPUCommandEncoder encod = wgpuDeviceCreateCommandEncoder(webgpu_context->device, &encorderDesc);

	// Create compute pass
	WGPUComputePassDescriptor computePassDesc = {};
	computePassDesc.timestampWrites = nullptr;
	WGPUComputePassEncoder computePass = wgpuCommandEncoderBeginComputePass(encod, &computePassDesc);

	// Use compute pass
	voxelization_pipeline.set(computePass);
	wgpuComputePassEncoderSetBindGroup(computePass, 0, voxelization_bindgroup, 0, nullptr);

	/*
	Instead of providing a single number of concurrent calls, we express this number as a grid (sipatch) of x * y * z workgroups (groups of calls).
	Each workgroup is a little block of w * h * d threads, each of which runs the entry point.
	w * h * d should be multiple of 32
	*/

	// Ceil invocationCount / workgroupSize
	uint32_t workgroup_size = 8 * 8 * 4;
	uint32_t workgroup_count = ceil(grid_size * grid_size * grid_size / workgroup_size);
	wgpuComputePassEncoderDispatchWorkgroups(computePass, workgroup_count, 1, 1);

	wgpuComputePassEncoderEnd(computePass);

	// Encode and submit GPU commands
	const WGPUCommandBufferDescriptor desc = {};
	WGPUCommandBuffer commands = wgpuCommandEncoderFinish(encod, &desc);
	wgpuQueueSubmit(queue, 1, &commands);

	wgpuCommandBufferRelease(commands);
	wgpuComputePassEncoderRelease(computePass);
	wgpuCommandEncoderRelease(encod);
	RenderdocCapture::end_capture_frame();
}

void VCTEngine::clean()
{
    Engine::clean();

	voxel_voxelGridPointsBuffer.destroy();
	voxel_gridDataBuffer.destroy();

	wgpuBindGroupRelease(voxelization_bindgroup);
}

void VCTEngine::update(float delta_time)
{
    //entities[0]->rotate(0.8f * delta_time, glm::vec3(0.0f, 0.0f, 1.0f));

	Engine::update(delta_time);
}

void VCTEngine::render()
{
	for (auto entity : entities) {
		entity->render();
	}

	floor_grid_mesh->render();

	Engine::render();
}
