#include "VCT_engine.h"
#include "framework/entities/entity_mesh.h"
#include "framework/entities/entity_text.h"
#include "framework/input.h"
#include "framework/scene/parse_scene.h"
#include "graphics/VCT_renderer.h"

#include <iostream>
#include <fstream>

struct mapAsync {
	WGPUBuffer mBuffer;
	uint32_t size;
	std::vector<float> inputs;
	bool done;
};

int VCTEngine::initialize(Renderer* renderer, GLFWwindow* window, bool use_glfw, bool use_mirror_screen)
{
	int error = Engine::initialize(renderer, window, use_glfw, use_mirror_screen);

    EntityMesh* cube = parse_scene("data/meshes/cube/cube.obj");
    cube->scale(glm::vec3(0.25));
    cube->translate(glm::vec3(1.0f, 0.0, 0.0));
    entities.push_back(cube);

    /*
    TextEntity* text = new TextEntity("oppenheimer vs barbie");
    text->set_material_color(colors::GREEN);
    text->set_scale(0.25f)->generate_mesh();
    text->translate(glm::vec3(0.0f, 0.0, -5.0));
    entities.push_back(text);*/

	init_compute_voxelization();
	init_bindings_voxelization_pipeline();

	onCompute();

	return error;
}

void VCTEngine::init_compute_voxelization()
{
    compute_voxelization_shader = RendererStorage::get_shader("data/shaders/voxelization.wgsl");

    WebGPUContext* webgpu_context = VCTRenderer::instance->get_webgpu_context();

	init_bindings_voxelization_pipeline();

    compute_inputBuffer.data = webgpu_context->create_buffer(compute_inputBuffer.buffer_size, WGPUBufferUsage_Storage | WGPUBufferUsage_CopyDst, nullptr);
    compute_outputBuffer.data = webgpu_context->create_buffer(compute_outputBuffer.buffer_size, WGPUBufferUsage_Storage | WGPUBufferUsage_CopySrc, nullptr);
    compute_mapBuffer.data = webgpu_context->create_buffer(compute_inputBuffer.buffer_size, WGPUBufferUsage_CopyDst | WGPUBufferUsage_MapRead, nullptr);


    std::vector<Uniform*> uniforms = { &compute_inputBuffer, &compute_outputBuffer };
	compute_voxelization_bindgroup = webgpu_context->create_bind_group(uniforms, compute_voxelization_shader, 0);

	compute_voxelization_pipeline.create_compute(compute_voxelization_shader);
}

void VCTEngine::init_bindings_voxelization_pipeline()
{
    compute_inputBuffer.binding = 0;
    compute_inputBuffer.buffer_binding_type = WGPUBufferBindingType_Storage;
    compute_inputBuffer.visibility = WGPUShaderStage_Compute;
    compute_inputBuffer.buffer_size = 8 * sizeof(float);

    compute_outputBuffer.binding = 1;
    compute_outputBuffer.buffer_binding_type = WGPUBufferBindingType_Storage;
    compute_outputBuffer.visibility = WGPUShaderStage_Compute;
    compute_outputBuffer.buffer_size = 8 * sizeof(float);

	compute_mapBuffer.buffer_size = 8 * sizeof(float);
}

void VCTEngine::onCompute()
{
    WebGPUContext* webgpu_context = VCTRenderer::instance->get_webgpu_context();
    WGPUQueue queue = webgpu_context->device_queue;

    // Fill in input buffer
    std::vector<float> input(compute_inputBuffer.buffer_size / sizeof(float));
    for (int i = 0; i < input.size(); ++i) input[i] = 0.1f * i;

    wgpuQueueWriteBuffer(queue, std::get<WGPUBuffer>(compute_inputBuffer.data), 0, input.data(), input.size() * sizeof(float));

	WGPUCommandEncoderDescriptor encorderDesc = {};
	WGPUCommandEncoder encod = wgpuDeviceCreateCommandEncoder(webgpu_context->device, &encorderDesc);

	// Create compute pass
	WGPUComputePassDescriptor computePassDesc = {};
	computePassDesc.timestampWrites = nullptr;
	WGPUComputePassEncoder computePass = wgpuCommandEncoderBeginComputePass(encod, &computePassDesc);

	// Use compute pass
	compute_voxelization_pipeline.set(computePass);
	wgpuComputePassEncoderSetBindGroup(computePass, 0, compute_voxelization_bindgroup, 0, nullptr);

	/*
	Instead of providing a single number of concurrent calls, we express this number as a grid (sipatch) of x * y * z workgroups (groups of calls).
	Each workgroup is a little block of w * h * d threads, each of which runs the entry point.
	w * h * d should be multiple of 32
	*/

	// Ceil invocationCount / workgroupSize
	uint32_t invocationCount = 32 / sizeof(float);
	uint32_t workgroupSize = 32;
	uint32_t workgroupCount = (invocationCount + workgroupSize - 1) / workgroupSize;
	wgpuComputePassEncoderDispatchWorkgroups(computePass, workgroupCount, 1, 1);

	wgpuComputePassEncoderEnd(computePass);

	wgpuCommandEncoderCopyBufferToBuffer(encod, std::get<WGPUBuffer>(compute_outputBuffer.data), 0, std::get<WGPUBuffer>(compute_mapBuffer.data), 0, compute_mapBuffer.buffer_size);

	// Encode and submit GPU commands
	const WGPUCommandBufferDescriptor desc = {};
	WGPUCommandBuffer commands = wgpuCommandEncoderFinish(encod, &desc);
	wgpuQueueSubmit(queue, 1, &commands);

	/* Tambn se puede hacer como mapAsync structure = new mapAsync();, pero luego tengo que hacer destroy structure; para liberar memoria*/
	// Print output
	mapAsync structure = {};
	structure.mBuffer = std::get<WGPUBuffer>(compute_mapBuffer.data);
	structure.size = 32;
	structure.inputs = input;
	structure.done = false;
	wgpuBufferMapAsync(std::get<WGPUBuffer>(compute_mapBuffer.data), WGPUMapMode_Read, 0, compute_mapBuffer.buffer_size, [](WGPUBufferMapAsyncStatus status, void* usr) {
		mapAsync* strc = reinterpret_cast<mapAsync*>(usr);
		if (status == WGPUBufferMapAsyncStatus_Success) {
			const float* output = (const float*)wgpuBufferGetConstMappedRange(strc->mBuffer, 0, strc->size);
			for (int i = 0; i < strc->inputs.size(); ++i) {
				std::cout << "Input " << strc->inputs[i] << " became " << output[i] << std::endl;
			}
			wgpuBufferUnmap(strc->mBuffer);
		}
		strc->done = true;
		}, (void*)&structure);

	while (!structure.done) {
		wgpuInstanceProcessEvents(webgpu_context->get_instance());
	}

	wgpuCommandBufferRelease(commands);
	wgpuComputePassEncoderRelease(computePass);
	wgpuCommandEncoderRelease(encod);


    //wgpuQueueSubmit(webgpu_context->device_queue, 1, &commands);
}

void VCTEngine::clean()
{
    Engine::clean();

	compute_inputBuffer.destroy();
	compute_outputBuffer.destroy();
	compute_mapBuffer.destroy();

	wgpuBindGroupRelease(compute_voxelization_bindgroup);
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

	Engine::render();
}
