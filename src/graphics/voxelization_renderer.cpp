#include "voxelization_renderer.h"

#include "VCT_renderer.h"

#ifdef XR_SUPPORT
#include "dawnxr/dawnxr_internal.h"
#endif

#include "framework/nodes/mesh_instance_3d.h"
#include "framework/scene/parse_scene.h"

VoxelizationRenderer::VoxelizationRenderer()
{
}

int VoxelizationRenderer::initialize(GLFWwindow* window, bool use_mirror_screen)
{
	init_render_voxelization_pipeline();

	return 0;
}

void VoxelizationRenderer::init_compute_voxelization()
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

void VoxelizationRenderer::init_bindings_voxelization_pipeline()
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

void VoxelizationRenderer::onCompute()
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


	std::vector<Uniform*> uniforms = { get_voxel_grid_points_buffer() };
	render_voxelization_bind_group = webgpu_context->create_bind_group(uniforms, RendererStorage::get_shader("data/shaders/draw_voxel_grid.wgsl"), 0);

	render_voxelization_pipeline.create_render(RendererStorage::get_shader("data/shaders/draw_voxel_grid.wgsl"), color_target);
}


void VoxelizationRenderer::clean()
{
	wgpuBindGroupRelease(render_voxelization_bind_group);

	voxel_voxelGridPointsBuffer.destroy();
	voxel_gridDataBuffer.destroy();

	wgpuBindGroupRelease(voxelization_bindgroup);
}

void VoxelizationRenderer::update(float delta_time)
{
}

void VoxelizationRenderer::render()
{
}

void VoxelizationRenderer::render_grid(WGPURenderPassEncoder render_pass)
{
	WebGPUContext* webgpu_context = VCTRenderer::instance->get_webgpu_context();

	render_voxelization_pipeline.set(render_pass);

	// Here you can update buffer if needed

	const Surface* surface = sphere_mesh->get_surface(0);

	// Set Bind groups
	wgpuRenderPassEncoderSetBindGroup(render_pass, 0, render_voxelization_bind_group, 0, nullptr);
	wgpuRenderPassEncoderSetBindGroup(render_pass, 1, &static_cast<VCTRenderer>(VCTRenderer::instance->get_current_camera_uniform()), 0, nullptr);

	// Set vertex buffer while encoding the render pass
	wgpuRenderPassEncoderSetVertexBuffer(render_pass, 0, surface->get_vertex_buffer(), 0, surface->get_byte_size());

	// Submit drawcalls
	wgpuRenderPassEncoderDraw(render_pass, surface->get_vertex_count(), 128 * 128 * 128, 0, 0);

}

void VoxelizationRenderer::resize_window(int width, int height)
{
}
