#include "voxelization_renderer.h"
//
#include "VCT_renderer.h"

#ifdef XR_SUPPORT
#include "dawnxr/dawnxr_internal.h"
#endif

#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_wgpu.h"

#include "graphics/renderer_storage.h"
#include "graphics/shader.h"
#include "graphics/debug/renderdoc_capture.h"

#include "framework/nodes/mesh_instance_3d.h"
#include "framework/scene/parse_scene.h"

VoxelizationRenderer::VoxelizationRenderer()
{
}

int VoxelizationRenderer::initialize(std::vector<MeshInstance3D*> nodes, Camera* camera)
{
	init_compute_voxelization(nodes, camera);
	on_compute();

	init_render_voxelization_pipeline();

	return 0;
}

void VoxelizationRenderer::init_compute_voxelization(std::vector<MeshInstance3D*> nodes, Camera* camera)
{
	std::vector<std::string> custom_specs; // = { "VERTEX_COLORS" };
	
	if (material_override_color) {
		custom_specs.push_back("MATERIAL_OVERRIDE_COLOR");
	}

	if (vertex_color) {
		custom_specs.push_back("VERTEX_COLORS");
	}

	// Get the voxelization Shader
	voxelization_shader = RendererStorage::get_shader("data/shaders/voxel_grid_points_fill.wgsl", custom_specs);

	WebGPUContext* webgpu_context = VCTRenderer::instance->get_webgpu_context();

	// Set bindings for the voxelization pipeline
	init_bindings_voxelization_pipeline(nodes, camera);

	voxelization_pipeline.create_compute(voxelization_shader);
}

void VoxelizationRenderer::init_bindings_voxelization_pipeline(std::vector<MeshInstance3D*> nodes, Camera* camera)
{
	RenderdocCapture::start_capture_frame();
	WebGPUContext* webgpu_context = VCTRenderer::instance->get_webgpu_context();

	AABB scene_aabb;
	AABB aabb;
	Surface* surface;
	std::vector<glm::vec4> vertex_positions;
	std::vector<glm::vec4> vertex_colors;
	std::vector<glm::mat4x4> models;
	std::vector<int> vertex_count;
	std::vector<glm::vec4> material_colors;

	// Bounds of the scene bounding box
	glm::vec4 min_pos = { FLT_MAX, FLT_MAX, FLT_MAX, 1.0 };
	glm::vec4 max_pos = { -FLT_MAX, -FLT_MAX, -FLT_MAX, 1.0 };
	
	// Bounds of the current bounding box
	glm::vec4 aabb_min;
	glm::vec4 aabb_max;

	for (auto node : nodes) {
		
		surface = node->get_surface(0);

		// Scene bounding box
		aabb = surface->get_aabb();

		glm::vec4 center_translated = node->get_model() * glm::vec4(aabb.center, 1.0);

		aabb_min = center_translated - glm::vec4(aabb.half_size, 1.0);
		aabb_max = center_translated + glm::vec4(aabb.half_size, 1.0);

		glm::bvec4 less_than = glm::lessThan(aabb_min, min_pos);

		if (less_than.x) {
			min_pos.x = aabb_min.x;
		}
		if (less_than.y) {
			min_pos.y = aabb_min.y;
		}
		if (less_than.z) {
			min_pos.z = aabb_min.z;
		}

		glm::bvec4 greater_than = glm::greaterThan(aabb_max, max_pos);

		if (greater_than.x) {
			max_pos.x = aabb_max.x;
		}
		if (greater_than.y) {
			max_pos.y = aabb_max.y;
		}
		if (greater_than.z) {
			max_pos.z = aabb_max.z;
		}

		// Get the vertices from the node
		auto& vertices = surface->get_vertices();

		for (int i = 0; i < vertices.size(); i++) {
			vertex_positions.push_back(node->get_model() * glm::vec4(vertices[i].position, 1.0));
			vertex_colors.push_back(glm::vec4(vertices[i].color, 1.0));
		}

		// Get the amount of vertices a node has
		vertex_count.push_back(surface->get_vertex_count());

		// Node color
		Material* material = node->get_surface_material_override(surface);
		material_colors.push_back(material->color);
	}
	scene_aabb.half_size = glm::vec3((max_pos - min_pos).x * 0.5, (max_pos - min_pos).y * 0.5, (max_pos - min_pos).z * 0.5);
	scene_aabb.center = glm::vec3(max_pos.x, max_pos.y, max_pos.z) - scene_aabb.half_size;
	
	// Translate the grid min to the minimum of the scene
	grid_data.bounds_min = glm::vec4(scene_aabb.center - scene_aabb.half_size, 1.0);
	grid_data.cell_half_size = 0.025f;

	// Grid size
	glm::vec3 grid_size_vec = ceil(scene_aabb.half_size / glm::vec3(grid_data.cell_half_size));
	grid_data.grid_width = grid_size_vec.x;
	grid_data.grid_height = grid_size_vec.y;
	grid_data.grid_depth = grid_size_vec.z;

	std::vector<glm::vec4> initial_position_values;
	for (int i = 0; i < grid_data.grid_width * grid_data.grid_height * grid_data.grid_depth; ++i) {
		initial_position_values.push_back(glm::vec4(0.0, 0.0, 0.0, 0.0));
	}

	// Information about the grid, such as its size, its translation or the cell size
	voxel_gridDataBuffer.binding = 0;
	voxel_gridDataBuffer.buffer_size = sizeof(gridData);
	voxel_gridDataBuffer.data = webgpu_context->create_buffer(voxel_gridDataBuffer.buffer_size, WGPUBufferUsage_CopyDst | WGPUBufferUsage_Uniform, &grid_data, "grid data buffer");

	// Positions of the voxels in the grid
	voxel_voxelGridPointsBuffer.binding = 1;
	voxel_voxelGridPointsBuffer.buffer_size = sizeof(glm::vec4) * grid_data.grid_width * grid_data.grid_height * grid_data.grid_depth;
	voxel_voxelGridPointsBuffer.data = webgpu_context->create_buffer(voxel_voxelGridPointsBuffer.buffer_size, WGPUBufferUsage_CopyDst | WGPUBufferUsage_Storage, initial_position_values.data(), "grid points buffer");

	// Positions of the vertices
	voxel_vertexPositionBuffer.binding = 2;
	voxel_vertexPositionBuffer.buffer_size = sizeof(glm::vec4) * vertex_positions.size();
	voxel_vertexPositionBuffer.data = webgpu_context->create_buffer(voxel_vertexPositionBuffer.buffer_size, WGPUBufferUsage_CopyDst | WGPUBufferUsage_Storage, vertex_positions.data(), "vertex positions");

	// Amount of vertices in each entity
	voxel_vertexCount.binding = 3;
	voxel_vertexCount.buffer_size = sizeof(int) * vertex_count.size();
	voxel_vertexCount.data = webgpu_context->create_buffer(voxel_vertexCount.buffer_size, WGPUBufferUsage_CopyDst | WGPUBufferUsage_Storage, vertex_count.data(), "vertex count");

	// Number of nodes
	int number_nodes = nodes.size();
	voxel_meshCountBuffer.binding = 4;
	voxel_meshCountBuffer.buffer_size = sizeof(int) * number_nodes;
	voxel_meshCountBuffer.data = webgpu_context->create_buffer(voxel_meshCountBuffer.buffer_size, WGPUBufferUsage_CopyDst | WGPUBufferUsage_Uniform, &number_nodes, "number of meshes");

	// Color of each voxel
	std::vector<glm::vec4> initial_color_values = initial_position_values;
	voxel_voxelColorBuffer.binding = 5;
	voxel_voxelColorBuffer.buffer_size = sizeof(glm::vec4) * initial_color_values.size();
	voxel_voxelColorBuffer.data = webgpu_context->create_buffer(voxel_voxelColorBuffer.buffer_size, WGPUBufferUsage_CopyDst | WGPUBufferUsage_Storage, initial_color_values.data(), "color of voxels");

	std::vector<glm::vec4> color_values;
	for (int i = 0; i < webgpu_context->screen_width * webgpu_context->screen_height * 3; ++i) {
		color_values.push_back(glm::vec4(0.0, 0.0, 0.0, 0.0));
	}

	colorBuffer.binding = 6;
	colorBuffer.buffer_size = sizeof(float) * webgpu_context->screen_width * webgpu_context->screen_height * 4;
	colorBuffer.data = webgpu_context->create_buffer(colorBuffer.buffer_size, WGPUBufferUsage_CopyDst | WGPUBufferUsage_Storage, color_values.data(), "colors rasterizer");

	//// MIRAR LOS VALORES INICIALIZADOS DE CAM POS
	glm::vec3 off = grid_size_vec / 2.0f;

	glm::vec3 cam_pos = camera->get_eye();
	////cam_pos -= glm::mod(cam_pos, grid_data.cell_half_size * 2.0f);

	camera->set_orthographic(cam_pos.x - off.x, cam_pos.x + off.x,
		cam_pos.y - off.y, cam_pos.y + off.y,
		cam_pos.z - off.z, cam_pos.z + off.z);

	voxel_orthoProjectionMatrix.binding = 7;
	voxel_orthoProjectionMatrix.buffer_size = sizeof(glm::mat4x4);
	voxel_orthoProjectionMatrix.data = webgpu_context->create_buffer(voxel_orthoProjectionMatrix.buffer_size, WGPUBufferUsage_CopyDst | WGPUBufferUsage_Storage, &camera->get_projection(), "orthogonal projection");

	// Color of the material override
	voxel_meshColorsBuffer.binding = 8;
	voxel_meshColorsBuffer.buffer_size = sizeof(glm::vec4) * material_colors.size();
	voxel_meshColorsBuffer.data = webgpu_context->create_buffer(voxel_meshColorsBuffer.buffer_size, WGPUBufferUsage_CopyDst | WGPUBufferUsage_Storage, material_colors.data(), "color of meshes");

	// Vertex colors
	voxel_vertexColorBuffer.binding = 9;
	voxel_vertexColorBuffer.buffer_size = sizeof(glm::vec4) * vertex_colors.size();
	voxel_vertexColorBuffer.data = webgpu_context->create_buffer(voxel_vertexColorBuffer.buffer_size, WGPUBufferUsage_CopyDst | WGPUBufferUsage_Storage, vertex_colors.data(), "vertex colors");

	std::vector<Uniform*> uniforms = { &voxel_gridDataBuffer, &voxel_voxelGridPointsBuffer, &voxel_vertexPositionBuffer, &voxel_vertexCount, &voxel_meshCountBuffer, &voxel_voxelColorBuffer, &colorBuffer, &voxel_orthoProjectionMatrix };
	if (material_override_color) {
		uniforms.push_back(&voxel_meshColorsBuffer);
	}

	if (vertex_color) {
		uniforms.push_back(&voxel_vertexColorBuffer);
	}

	voxelization_bindgroup = webgpu_context->create_bind_group(uniforms, voxelization_shader, 0);
}

void VoxelizationRenderer::on_compute()
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
	w * h * d should be multiple of 32 */

	// Ceil invocationCount / workgroupSize
	int workgroup_size = 256;
	int workgroup_count = ceil(webgpu_context->screen_height * webgpu_context->screen_width / 256) ;
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

void VoxelizationRenderer::init_render_voxelization_pipeline()
{
	sphere_mesh = parse_mesh("data/meshes/cube.obj");

	WebGPUContext* webgpu_context = VCTRenderer::instance->get_webgpu_context();

	WGPUTextureFormat swapchain_format = webgpu_context->swapchain_format;

	WGPUBlendState* blend_state = new WGPUBlendState();
	blend_state->color = {
			.operation = WGPUBlendOperation_Add,
			.srcFactor = WGPUBlendFactor_SrcAlpha,
			.dstFactor = WGPUBlendFactor_OneMinusSrcAlpha,
	};
	blend_state->alpha = {
			.operation = WGPUBlendOperation_Add,
			.srcFactor = WGPUBlendFactor_Zero,
			.dstFactor = WGPUBlendFactor_One,
	};

	WGPUColorTargetState color_target = {};
	color_target.format = swapchain_format;
	color_target.blend = blend_state;
	color_target.writeMask = WGPUColorWriteMask_All;

	sphere_mesh->get_surface(0)->set_material_cull_type(CULL_NONE);
	sphere_mesh->get_surface(0)->set_material_transparency_type(ALPHA_BLEND);

	float cell_size = grid_data.cell_half_size * 2.0;

	voxel_cell_size.binding = 2;
	voxel_cell_size.buffer_size = sizeof(float);
	voxel_cell_size.data = webgpu_context->create_buffer(voxel_cell_size.buffer_size, WGPUBufferUsage_CopyDst | WGPUBufferUsage_Uniform, &cell_size, "cell size");

	std::vector<Uniform*> uniforms = { get_voxel_grid_points_buffer(), &voxel_cell_size, &voxel_voxelColorBuffer };
	render_voxelization_bind_group = webgpu_context->create_bind_group(uniforms, RendererStorage::get_shader("data/shaders/draw_voxel_grid.wgsl"), 0);

	render_voxelization_pipeline.create_render(RendererStorage::get_shader("data/shaders/draw_voxel_grid.wgsl"), color_target);
}


void VoxelizationRenderer::clean()
{
	wgpuBindGroupRelease(render_voxelization_bind_group);

	voxel_voxelGridPointsBuffer.destroy();
	voxel_gridDataBuffer.destroy();
	voxel_vertexPositionBuffer.destroy();
	voxel_vertexCount.destroy();
	voxel_meshCountBuffer.destroy();
	voxel_cell_size.destroy();
	voxel_voxelColorBuffer.destroy();
	voxel_meshColorsBuffer.destroy();
	voxel_vertexColorBuffer.destroy();

	voxel_orthoProjectionMatrix.destroy();

	wgpuBindGroupRelease(voxelization_bindgroup);
}

void VoxelizationRenderer::update(float delta_time)
{
}

void VoxelizationRenderer::render()
{
}

void VoxelizationRenderer::render_grid(WGPURenderPassEncoder render_pass, WGPUBindGroup render_camera_bind_group, uint32_t camera_buffer_stride)
{
	WebGPUContext* webgpu_context = VCTRenderer::instance->get_webgpu_context();

	render_voxelization_pipeline.set(render_pass);

	// Here you can update buffer if needed
	//wgpuQueueWriteBuffer(webgpu_context->device_queue, std::get<WGPUBuffer>(voxel_gridDataBuffer.data), 0, &(grid_data), sizeof(voxel_gridDataBuffer.buffer_size));

	const Surface* surface = sphere_mesh->get_surface(0);

	// Set Bind groups
	wgpuRenderPassEncoderSetBindGroup(render_pass, 0, render_voxelization_bind_group, 0, nullptr);
	wgpuRenderPassEncoderSetBindGroup(render_pass, 1, render_camera_bind_group, 1, &camera_buffer_stride);

	// Set vertex buffer while encoding the render pass
	wgpuRenderPassEncoderSetVertexBuffer(render_pass, 0, surface->get_vertex_buffer(), 0, surface->get_byte_size());

	// Submit drawcalls
	wgpuRenderPassEncoderDraw(render_pass, surface->get_vertex_count(), grid_data.grid_width * grid_data.grid_height * grid_data.grid_depth, 0, 0);

}

void VoxelizationRenderer::resize_window(int width, int height)
{
}
