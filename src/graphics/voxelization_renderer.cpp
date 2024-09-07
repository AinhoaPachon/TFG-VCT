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

#include <bit>

VoxelizationRenderer::VoxelizationRenderer()
{
}

int VoxelizationRenderer::initialize(std::vector<MeshInstance3D*> nodes, Camera* camera)
{
	init_compute_voxelization(nodes, camera);
	init_render_pipeline();
	on_compute();

	quad_surface.create_quad(2.0f, 2.0f);

	// El problema est� en que esto no deber�a llamarse aqu�!!
	//render_voxelization();

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
	//voxelization_shader = RendererStorage::get_shader("data/shaders/voxel_grid_points_fill.wgsl", custom_specs);
	voxelization_shader = RendererStorage::get_shader("data/shaders/voxelizer.wgsl", custom_specs);

	WebGPUContext* webgpu_context = VCTRenderer::instance->get_webgpu_context();

	// Set bindings for the voxelization pipeline
	init_bindings_voxelization_pipeline(nodes, camera);
	init_bindings_rasterizer(nodes, camera);

	voxelization_pipeline.create_compute(voxelization_shader);
}

void VoxelizationRenderer::init_bindings_voxelization_pipeline(std::vector<MeshInstance3D*> nodes, Camera* camera)
{
	RenderdocCapture::start_capture_frame();
	WebGPUContext* webgpu_context = VCTRenderer::instance->get_webgpu_context();

	AABB scene_aabb;
	AABB aabb;
	Surface* surface;
	std::vector<glm::mat4x4> models;
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

		// Node color
		Material* material = node->get_surface_material_override(surface) ? node->get_surface_material_override(surface) : &surface->get_material();
		material_colors.push_back(material->color);
	}
	scene_aabb.half_size = glm::vec3((max_pos - min_pos).x * 0.5, (max_pos - min_pos).y * 0.5, (max_pos - min_pos).z * 0.5);
	scene_aabb.center = glm::vec3(max_pos.x, max_pos.y, max_pos.z) - scene_aabb.half_size;
	
	// Translate the grid min to the minimum of the scene
	//grid_data.bounds_min = glm::vec4(scene_aabb.center - scene_aabb.half_size, 1.0);
	//grid_data.cell_half_size = 0.025f;

	// Grid size
	//glm::vec3 grid_size_vec = ceil(scene_aabb.half_size / glm::vec3(grid_data.cell_half_size));
	/*grid_data.grid_width = grid_size_vec.x;
	grid_data.grid_height = grid_size_vec.y;
	grid_data.grid_depth = grid_size_vec.z;*/


	/*
	// Number of nodes
	int number_nodes = nodes.size();
	voxel_meshCountBuffer.binding = 3;
	voxel_meshCountBuffer.buffer_size = sizeof(int) * number_nodes;
	voxel_meshCountBuffer.data = webgpu_context->create_buffer(voxel_meshCountBuffer.buffer_size, WGPUBufferUsage_CopyDst | WGPUBufferUsage_Uniform, &number_nodes, "number of meshes");

	// Color of each voxel
	std::vector<glm::vec4> initial_color_values = initial_position_values;
	voxel_voxelColorBuffer.binding = 4;
	voxel_voxelColorBuffer.buffer_size = sizeof(glm::vec4) * initial_color_values.size();
	voxel_voxelColorBuffer.data = webgpu_context->create_buffer(voxel_voxelColorBuffer.buffer_size, WGPUBufferUsage_CopyDst | WGPUBufferUsage_Storage, initial_color_values.data(), "color of voxels");

	// Color of the material override
	voxel_meshColorsBuffer.binding = 5;
	voxel_meshColorsBuffer.buffer_size = sizeof(glm::vec4) * material_colors.size();
	voxel_meshColorsBuffer.data = webgpu_context->create_buffer(voxel_meshColorsBuffer.buffer_size, WGPUBufferUsage_CopyDst | WGPUBufferUsage_Storage, material_colors.data(), "color of meshes");

	std::vector<Uniform*> uniforms = { &voxel_gridDataBuffer, &voxel_voxelGridPointsBuffer, &voxel_vertexBuffer, &voxel_meshCountBuffer, &voxel_voxelColorBuffer, &voxel_meshColorsBuffer };
	if (material_override_color) {
		uniforms.push_back(&voxel_meshColorsBuffer);
	}
	
	std::vector<Uniform*> uniforms = {};
	*/
	
	//voxelization_bindgroup = webgpu_context->create_bind_group(uniforms, voxelization_shader, 0);
}

void VoxelizationRenderer::init_bindings_rasterizer(std::vector<MeshInstance3D*> nodes, Camera* camera)
{
	WebGPUContext* webgpu_context = VCTRenderer::instance->get_webgpu_context();

	std::vector<glm::vec4> color_values;
	for (int i = 0; i < webgpu_context->screen_width * webgpu_context->screen_height * 4; ++i) {
		color_values.push_back(glm::uvec4(255, 255, 255, 255));
	}

	colorBuffer.binding = 0;
	colorBuffer.buffer_size = sizeof(float) * webgpu_context->screen_width * webgpu_context->screen_height * 4;
	colorBuffer.data = webgpu_context->create_buffer(colorBuffer.buffer_size, WGPUBufferUsage_CopyDst | WGPUBufferUsage_Storage, color_values.data(), "colors rasterizer");

	float voxel_size = 0.01f;

	Camera orth_cam;
	//orth_cam.look_at(glm::vec3(0.0f, 0.1f, 0.4f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
	orth_cam.set_orthographic(
		-voxelizer_uniforms.grid_width * 0.5 * voxel_size, voxelizer_uniforms.grid_width * 0.5 * voxel_size,
		-voxelizer_uniforms.grid_height * 0.5 * voxel_size, voxelizer_uniforms.grid_height * 0.5 * voxel_size,
		-voxelizer_uniforms.grid_depth * 0.5 * voxel_size, voxelizer_uniforms.grid_depth * 0.5 * voxel_size
	);

	voxelizer_uniforms.viewProjectionMatrix = orth_cam.get_projection();// *nodes[0]->get_global_model();
	voxelizer_uniforms.model = nodes[0]->get_model();

	uniformsBuffer.binding = 2;
	uniformsBuffer.buffer_size = sizeof(UBO);
	uniformsBuffer.data = webgpu_context->create_buffer(uniformsBuffer.buffer_size, WGPUBufferUsage_CopyDst | WGPUBufferUsage_Uniform, &voxelizer_uniforms, "uniforms");

	std::vector<Uniform*> uniforms;

	int surface_counter = 0;

	for (MeshInstance3D* node : nodes) {
		std::vector<Surface*> surfaces = node->get_surfaces();
		surface_counter += surfaces.size();
	}
	voxelization_RasterData.resize(surface_counter);

	int counter = 0;
	for (MeshInstance3D* node : nodes) {
		std::vector<Surface*> surfaces = node->get_surfaces();
		for (Surface* surface : surfaces) {

			SurfaceRasterData* raster_data_temp = new SurfaceRasterData;

			WGPUBuffer vertex_buffer = surface->get_vertex_buffer();

			std::vector<InterleavedData> vertices = surface->get_vertices();

			// Positions of the vertices
			voxel_vertexBuffer.binding = 0;
			voxel_vertexBuffer.buffer_size = sizeof(InterleavedData) * vertices.size();
			voxel_vertexBuffer.data = webgpu_context->create_buffer(voxel_vertexBuffer.buffer_size, WGPUBufferUsage_CopyDst | WGPUBufferUsage_Storage, vertices.data(), "vertex buffer");

			Material mat = surface->get_material();
			glm::vec4 color = mat.color;

			Uniform colorBuffer;
			colorBuffer.binding = 1;
			colorBuffer.buffer_size = sizeof(glm::vec4);
			colorBuffer.data = webgpu_context->create_buffer(colorBuffer.buffer_size, WGPUBufferUsage_CopyDst | WGPUBufferUsage_Storage, &color, "color buffer");

			uniforms = { &voxel_vertexBuffer, &colorBuffer, &uniformsBuffer };
			
			WGPUBindGroup bindgroup = webgpu_context->create_bind_group(uniforms, voxelization_shader, 0);
			int number_triangles = surface->get_vertex_count();

			raster_data_temp->voxelization_bindgroup = bindgroup;
			raster_data_temp->voxel_vertexBuffer = voxel_vertexBuffer;
			raster_data_temp->number_triangles = number_triangles;

			voxelization_RasterData[counter] = raster_data_temp;
			counter++;
		}
	}
	
	std::vector<uint8_t> texture_values;
	texture_values.resize(voxelizer_uniforms.grid_width * voxelizer_uniforms.grid_height * voxelizer_uniforms.grid_depth * 4);

	for (int i = 0; i < voxelizer_uniforms.grid_width * voxelizer_uniforms.grid_height * voxelizer_uniforms.grid_depth * 4; ++i) {
		texture_values[i] = 0;
	}

	mipmap_count = std::bit_width(std::max(voxelizer_uniforms.grid_width, voxelizer_uniforms.grid_height));

	texture3D.create(WGPUTextureDimension_3D, 
		WGPUTextureFormat_RGBA8Unorm, 
		{ uint32_t(voxelizer_uniforms.grid_width), uint32_t(voxelizer_uniforms.grid_height), uint32_t(voxelizer_uniforms.grid_depth) }, 
		static_cast<WGPUTextureUsage>(WGPUTextureUsage_TextureBinding | WGPUTextureUsage_StorageBinding | WGPUTextureUsage_CopyDst), mipmap_count, 1, nullptr);

	webgpu_context->upload_texture(texture3D.get_texture(), WGPUTextureDimension_3D, texture3D.get_size(), 0, WGPUTextureFormat_RGBA8Unorm, texture_values.data(), {0, 0, 0});

	textureBuffer.binding = 1;
	textureBuffer.data = texture3D.get_view(WGPUTextureViewDimension_3D);

	uniforms = { &textureBuffer };
	color_buffer_bindgroup = webgpu_context->create_bind_group(uniforms, voxelization_shader, 1);
	//render_color_buffer_bindgroup = webgpu_context->create_bind_group(uniforms, render_voxelization_shader, 1);

	light.position = glm::vec3(0.5);
	light.color = glm::vec3(1.0, 0.0, 0.0);
	light.intensity = 1.0f;

	lightBuffer.binding = 0;
	lightBuffer.buffer_size = sizeof(PointLight);
	lightBuffer.data = webgpu_context->create_buffer(lightBuffer.buffer_size, WGPUBufferUsage_CopyDst | WGPUBufferUsage_Uniform, &light, "light buffer");

	uniforms = { &lightBuffer };
	render_lights_buffer_bindgroup = webgpu_context->create_bind_group(uniforms, voxelization_shader, 2);
}

void VoxelizationRenderer::on_compute()
{
	RenderdocCapture::start_capture_frame();
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

	for (SurfaceRasterData* surface_raster_data : voxelization_RasterData) {
	
		wgpuComputePassEncoderSetBindGroup(computePass, 0, surface_raster_data->voxelization_bindgroup, 0, nullptr);
		wgpuComputePassEncoderSetBindGroup(computePass, 1, color_buffer_bindgroup, 0, nullptr);
		wgpuComputePassEncoderSetBindGroup(computePass, 2, render_lights_buffer_bindgroup, 0, nullptr);

		/*
		Instead of providing a single number of concurrent calls, we express this number as a grid (sipatch) of x * y * z workgroups (groups of calls).
		Each workgroup is a little block of w * h * d threads, each of which runs the entry point.
		w * h * d should be multiple of 32 */

		// Ceil invocationCount / workgroupSize
		int workgroup_size = surface_raster_data->number_triangles; // CAMBIAR AL DISPATCH DE UNA VEZ POR TRIANGULO
		int workgroup_count = ceil(surface_raster_data->number_triangles / 3);
		wgpuComputePassEncoderDispatchWorkgroups(computePass, workgroup_count, 1, 1);

	}

	wgpuComputePassEncoderEnd(computePass);

	// Encode and submit GPU commands
	const WGPUCommandBufferDescriptor desc = {};
	WGPUCommandBuffer commands = wgpuCommandEncoderFinish(encod, &desc);
	wgpuQueueSubmit(queue, 1, &commands);

	wgpuCommandBufferRelease(commands);
	wgpuComputePassEncoderRelease(computePass);
	wgpuCommandEncoderRelease(encod);

	webgpu_context->create_texture_mipmaps(texture3D.get_texture(), { uint32_t(voxelizer_uniforms.grid_width), uint32_t(voxelizer_uniforms.grid_height), uint32_t(voxelizer_uniforms.grid_depth) }, mipmap_count, WGPUTextureViewDimension_3D, WGPUTextureFormat_RGBA8Unorm);
	
	RenderdocCapture::end_capture_frame();
}

void VoxelizationRenderer::init_render_pipeline()
{
	WebGPUContext* webgpu_context = VCTRenderer::instance->get_webgpu_context();

	render_voxelization_shader = RendererStorage::get_shader("data/shaders/draw_voxel_grid.wgsl");

	WGPUTextureFormat swapchain_format = webgpu_context->swapchain_format;

	WGPUBlendState blend_state;
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

	render_uniforms.width = webgpu_context->screen_width;
	render_uniforms.height = webgpu_context->screen_height;

	renderUniformsBuffer.binding = 0;
	renderUniformsBuffer.buffer_size = sizeof(Uniforms);
	renderUniformsBuffer.data = webgpu_context->create_buffer(renderUniformsBuffer.buffer_size, WGPUBufferUsage_CopyDst | WGPUBufferUsage_Uniform, &render_uniforms, "uniforms");

	std::vector<Uniform*> uniforms = { &renderUniformsBuffer };
	render_voxelization_bind_group = webgpu_context->create_bind_group(uniforms, render_voxelization_shader, 0);

	uniforms = { &colorBuffer };
	render_color_buffer_bindgroup = webgpu_context->create_bind_group(uniforms, render_voxelization_shader, 1);

	// para q no de problemas el depth buffer
	PipelineDescription desc = {};
	desc.depth_read = false;

	render_voxelization_pipeline.create_render(RendererStorage::get_shader("data/shaders/draw_voxel_grid.wgsl"), color_target, desc);
}

void VoxelizationRenderer::clean()
{
	wgpuBindGroupRelease(render_voxelization_bind_group);
	wgpuBindGroupRelease(color_buffer_bindgroup);
	wgpuBindGroupRelease(render_lights_buffer_bindgroup);

	voxel_vertexBuffer.destroy();
	renderUniformsBuffer.destroy();
	lightBuffer.destroy();

	colorBuffer.destroy();
	textureBuffer.destroy();
	uniformsBuffer.destroy();
}

void VoxelizationRenderer::update(float delta_time)
{
}

void VoxelizationRenderer::render_grid(WGPURenderPassEncoder render_pass, WGPUBindGroup render_camera_bind_group, uint32_t camera_buffer_stride)
{

	WebGPUContext* webgpu_context = VCTRenderer::instance->get_webgpu_context();

	render_voxelization_pipeline.set(render_pass);

	// Here you can update buffer if needed
	//wgpuQueueWriteBuffer(webgpu_context->device_queue, std::get<WGPUBuffer>(voxel_gridDataBuffer.data), 0, &(grid_data), sizeof(voxel_gridDataBuffer.buffer_size));

	const Surface* surface = &quad_surface;

	// Set Bind groups
	wgpuRenderPassEncoderSetBindGroup(render_pass, 0, render_voxelization_bind_group, 0, nullptr);
	wgpuRenderPassEncoderSetBindGroup(render_pass, 1, render_color_buffer_bindgroup, 0, nullptr);

	// Set vertex buffer while encoding the render pass
	wgpuRenderPassEncoderSetVertexBuffer(render_pass, 0, surface->get_vertex_buffer(), 0, surface->get_byte_size());

	// Submit drawcalls
	wgpuRenderPassEncoderDraw(render_pass, surface->get_vertex_count(), 1, 0, 0);
}

void VoxelizationRenderer::resize_window(int width, int height)
{
}
