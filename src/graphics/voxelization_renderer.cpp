#include "voxelization_renderer.h"
//
#include "VCT_renderer.h"

#ifdef XR_SUPPORT
#include "dawnxr/dawnxr_internal.h"
#endif

#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_wgpu.h"

#include "framework/nodes/mesh_instance_3d.h"
#include "framework/scene/parse_scene.h"

VoxelizationRenderer::VoxelizationRenderer()
{
}

int VoxelizationRenderer::initialize(MeshInstance3D* node)
{
	init_compute_voxelization(node);
	on_compute();

	init_render_voxelization_pipeline();

	return 0;
}

void VoxelizationRenderer::init_compute_voxelization(MeshInstance3D* node)
{
	// Get the voxelization Shader
	voxelization_shader = RendererStorage::get_shader("data/shaders/voxel_grid_points_fill.wgsl");

	WebGPUContext* webgpu_context = VCTRenderer::instance->get_webgpu_context();

	// Set bindings for the voxelization pipeline
	init_bindings_voxelization_pipeline(node);

	// set grid_data uniforms and camera_data uniforms
	std::vector<Uniform*> uniforms = { &voxel_gridDataBuffer, &voxel_voxelGridPointsBuffer, &voxel_vertexPositionBuffer, &voxel_vertexCount, &voxel_representationBuffer };
	voxelization_bindgroup = webgpu_context->create_bind_group(uniforms, voxelization_shader, 0);

	voxelization_pipeline.create_compute(voxelization_shader);
}

void VoxelizationRenderer::init_bindings_voxelization_pipeline(MeshInstance3D* node)
{
	RenderdocCapture::start_capture_frame();
	WebGPUContext* webgpu_context = VCTRenderer::instance->get_webgpu_context();

	AABB aabb = node->get_aabb();

	grid_data.bounds_min = glm::vec4(aabb.center - aabb.half_size, 1.0);
	grid_data.cell_half_size = 0.05f;

	glm::vec3 grid_size_vec = ceil(aabb.half_size / glm::vec3(grid_data.cell_half_size));
	grid_data.grid_width = grid_size_vec.x;
	grid_data.grid_height = grid_size_vec.y;
	grid_data.grid_depth = grid_size_vec.z;

	voxel_gridDataBuffer.binding = 0;
	voxel_gridDataBuffer.buffer_size = sizeof(gridData);
	voxel_gridDataBuffer.data = webgpu_context->create_buffer(voxel_gridDataBuffer.buffer_size, WGPUBufferUsage_CopyDst | WGPUBufferUsage_Uniform, &grid_data, "grid data buffer");

	std::vector<glm::vec4> initial_values;
	for (int i = 0; i < grid_data.grid_width * grid_data.grid_height * grid_data.grid_depth; ++i) {
		initial_values.push_back(glm::vec4(0.0, 0.0, 0.0, 0.0));
	}

	voxel_voxelGridPointsBuffer.binding = 1;
	voxel_voxelGridPointsBuffer.buffer_size = sizeof(glm::vec4) * grid_data.grid_width * grid_data.grid_height * grid_data.grid_depth;
	voxel_voxelGridPointsBuffer.data = webgpu_context->create_buffer(voxel_voxelGridPointsBuffer.buffer_size, WGPUBufferUsage_CopyDst | WGPUBufferUsage_Storage, initial_values.data(), "grid points buffer");

	Surface* surface = node->get_surface(0);
	auto& vertices = surface->get_vertices();
	std::vector<glm::vec4> vertex_positions;

	for (int i = 0; i < vertices.size(); i++) {
		vertex_positions.push_back(glm::vec4(vertices[i].position, 1.0));
	}

	voxel_vertexPositionBuffer.binding = 2;
	voxel_vertexPositionBuffer.buffer_size = sizeof(glm::vec3) * vertex_positions.size();
	voxel_vertexPositionBuffer.data = webgpu_context->create_buffer(voxel_vertexPositionBuffer.buffer_size, WGPUBufferUsage_CopyDst | WGPUBufferUsage_Storage, vertex_positions.data(), "vertex positions");

	int vertex_count = surface->get_vertex_count();
	voxel_vertexCount.binding = 3;
	voxel_vertexCount.buffer_size = 32;
	voxel_vertexCount.data = webgpu_context->create_buffer(voxel_vertexCount.buffer_size, WGPUBufferUsage_CopyDst | WGPUBufferUsage_Uniform, &vertex_count, "vertex count");

	voxel_representation.model = node->get_model();
	voxel_representationBuffer.binding = 4;
	voxel_representationBuffer.buffer_size = sizeof(voxelRepresentation);
	voxel_representationBuffer.data = webgpu_context->create_buffer(voxel_representationBuffer.buffer_size, WGPUBufferUsage_CopyDst | WGPUBufferUsage_Uniform, &voxel_representation, "voxel representation data");
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
	glm::vec3 workgroup_size = glm::vec3(4, 4, 4);
	glm::vec3 workgroup_count = glm::vec3(ceil(grid_data.grid_width / workgroup_size.x), ceil(grid_data.grid_height / workgroup_size.y), ceil(grid_data.grid_depth / workgroup_size.z));
	wgpuComputePassEncoderDispatchWorkgroups(computePass, workgroup_count.x, workgroup_count.y, workgroup_count.z);

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

	WGPUBlendState blend_state = {};
	blend_state.color = {
			.operation = WGPUBlendOperation_Add,
			.srcFactor = WGPUBlendFactor_SrcAlpha,
			.dstFactor = WGPUBlendFactor_OneMinusSrcAlpha,
	};
	blend_state.alpha = {
			.operation = WGPUBlendOperation_Add,
			.srcFactor = WGPUBlendFactor_Zero,
			.dstFactor = WGPUBlendFactor_One,
	};

	WGPUColorTargetState color_target = {};
	color_target.format = swapchain_format;
	color_target.blend = &blend_state;
	color_target.writeMask = WGPUColorWriteMask_All;

	sphere_mesh->get_surface(0)->set_material_cull_type(CULL_NONE);

	float cell_size = grid_data.cell_half_size * 2.0;

	voxel_cell_size.binding = 2;
	voxel_cell_size.buffer_size = sizeof(float);
	voxel_cell_size.data = webgpu_context->create_buffer(voxel_cell_size.buffer_size, WGPUBufferUsage_CopyDst | WGPUBufferUsage_Uniform, &cell_size, "cell size");

	std::vector<Uniform*> uniforms = { get_voxel_grid_points_buffer(), &voxel_cell_size };
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
	voxel_representationBuffer.destroy();
	voxel_cell_size.destroy();

	wgpuBindGroupRelease(voxelization_bindgroup);
}

void VoxelizationRenderer::update(float delta_time)
{
}

void VoxelizationRenderer::render()
{
}

void VoxelizationRenderer::render_grid(WGPURenderPassEncoder render_pass, WGPUBindGroup render_camera_bind_group, Node* entity)
{
	WebGPUContext* webgpu_context = VCTRenderer::instance->get_webgpu_context();

	render_voxelization_pipeline.set(render_pass);

	// Here you can update buffer if needed
	//wgpuQueueWriteBuffer(webgpu_context->device_queue, std::get<WGPUBuffer>(voxel_gridDataBuffer.data), 0, &(grid_data), sizeof(voxel_gridDataBuffer.buffer_size));

	const Surface* surface = sphere_mesh->get_surface(0);

	// Set Bind groups
	wgpuRenderPassEncoderSetBindGroup(render_pass, 0, render_voxelization_bind_group, 0, nullptr);
	wgpuRenderPassEncoderSetBindGroup(render_pass, 1, render_camera_bind_group, 0, nullptr);

	// Set vertex buffer while encoding the render pass
	wgpuRenderPassEncoderSetVertexBuffer(render_pass, 0, surface->get_vertex_buffer(), 0, surface->get_byte_size());

	// Submit drawcalls
	wgpuRenderPassEncoderDraw(render_pass, surface->get_vertex_count(), grid_data.grid_width * grid_data.grid_height * grid_data.grid_depth, 0, 0);

}

void VoxelizationRenderer::resize_window(int width, int height)
{
}
