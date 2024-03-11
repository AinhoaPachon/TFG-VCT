#include "VCT_renderer.h"

#ifdef XR_SUPPORT
#include "dawnxr/dawnxr_internal.h"
#endif

#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_wgpu.h"
#include "framework/nodes/mesh_instance_3d.h"

#include "framework/scene/parse_scene.h"

VCTRenderer::VCTRenderer() : Renderer()
{
    
}

int VCTRenderer::initialize(GLFWwindow* window, bool use_mirror_screen)
 {
    Renderer::initialize(window, use_mirror_screen);

    clear_color = glm::vec4(0.22f, 0.22f, 0.22f, 1.0);

    init_camera_bind_group();

    dynamic_cast<VCTEngine*>(VCTEngine::instance)->init_compute_voxelization();
    dynamic_cast<VCTEngine*>(VCTEngine::instance)->init_bindings_voxelization_pipeline();
    dynamic_cast<VCTEngine*>(VCTEngine::instance)->onCompute();

    //dynamic_cast<VCTEngine*>(VCTEngine::instance)->fill_entities();
    init_render_voxelization_pipeline();

#ifdef XR_SUPPORT
    if (is_openxr_available && use_mirror_screen) {
        init_mirror_pipeline();
    }
#endif

    camera = new FlyoverCamera();

    camera->set_perspective(glm::radians(45.0f), webgpu_context.render_width / static_cast<float>(webgpu_context.render_height), z_near, z_far);
    camera->look_at(glm::vec3(0.0f, 0.1f, 0.4f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
    camera->set_mouse_sensitivity(0.004f);
    camera->set_speed(0.75f);

    std::vector<Uniform*> uniforms = { dynamic_cast<VCTRenderer*>(VCTRenderer::instance)->get_current_camera_uniform() };

    render_camera_bind_group = webgpu_context.create_bind_group(uniforms, RendererStorage::get_shader("data/shaders/draw_voxel_grid.wgsl"), 0);

    return 0;
}

void VCTRenderer::clean()
{
    Renderer::clean();

    wgpuBindGroupRelease(render_voxelization_bind_group);

#if defined(XR_SUPPORT) && defined(USE_MIRROR_WINDOW)
    if (is_openxr_available) {
        for (uint8_t i = 0; i < swapchain_uniforms.size(); i++) {
            swapchain_uniforms[i].destroy();
            wgpuBindGroupRelease(swapchain_bind_groups[i]);
        }
    }
#endif
}

void VCTRenderer::update(float delta_time)
{

}

void VCTRenderer::render()
{
    prepare_instancing();

    if (!is_openxr_available) {
        render_screen();
    }

#if defined(XR_SUPPORT)
    if (is_openxr_available) {
        render_xr();

        if (use_mirror_screen) {
            render_mirror();
        }
    }
#endif

    clear_renderables();
}

void VCTRenderer::render_grid(WGPURenderPassEncoder render_pass)
{
    WebGPUContext* webgpu_context = VCTRenderer::instance->get_webgpu_context();

    render_voxelization_pipeline.set(render_pass);

    // Here you can update buffer if needed

    const Surface* surface = sphere_mesh->get_surface(0);

    // Set Bind groups
    wgpuRenderPassEncoderSetBindGroup(render_pass, 0, render_camera_bind_group, 0, nullptr);
    wgpuRenderPassEncoderSetBindGroup(render_pass, 1, render_voxelization_bind_group, 0, nullptr);
    
    // Set vertex buffer while encoding the render pass
    wgpuRenderPassEncoderSetVertexBuffer(render_pass, 0, surface->get_vertex_buffer(), 0, surface->get_byte_size());

    // Submit drawcalls
    wgpuRenderPassEncoderDraw(render_pass, surface->get_vertex_count(), dynamic_cast<VCTEngine*>(VCTEngine::instance)->voxel_voxelGridPointsBuffer.buffer_size, 0, 0);

}

void VCTRenderer::render_screen()
{
    camera_data.eye = camera->get_eye();
    camera_data.mvp = camera->get_view_projection();
    camera_data.dummy = 0.f;

    wgpuQueueWriteBuffer(webgpu_context.device_queue, std::get<WGPUBuffer>(camera_uniform.data), 0, &(camera_data), sizeof(sCameraData));

    WGPUTextureView swapchain_view = wgpuSwapChainGetCurrentTextureView(webgpu_context.screen_swapchain);

    ImGui::Render();

    {
        // Create the command encoder
        WGPUCommandEncoderDescriptor encoder_desc = {};
        WGPUCommandEncoder command_encoder = wgpuDeviceCreateCommandEncoder(webgpu_context.device, &encoder_desc);

        // Prepare the color attachment
        WGPURenderPassColorAttachment render_pass_color_attachment = {};
        render_pass_color_attachment.view = swapchain_view;
        render_pass_color_attachment.loadOp = WGPULoadOp_Clear;
        render_pass_color_attachment.storeOp = WGPUStoreOp_Store;

#ifndef __EMSCRIPTEN__
        render_pass_color_attachment.depthSlice = WGPU_DEPTH_SLICE_UNDEFINED;
#endif

        glm::vec4 clear_color = VCTRenderer::instance->get_clear_color();
        render_pass_color_attachment.clearValue = WGPUColor(clear_color.r, clear_color.g, clear_color.b, clear_color.a);

        // Prepate the depth attachment
        WGPURenderPassDepthStencilAttachment render_pass_depth_attachment = {};
        render_pass_depth_attachment.view = eye_depth_texture_view[EYE_LEFT];
        render_pass_depth_attachment.depthClearValue = 1.0f;
        render_pass_depth_attachment.depthLoadOp = WGPULoadOp_Clear;
        render_pass_depth_attachment.depthStoreOp = WGPUStoreOp_Store;
        render_pass_depth_attachment.depthReadOnly = false;
        render_pass_depth_attachment.stencilClearValue = 0; // Stencil config necesary, even if unused
        render_pass_depth_attachment.stencilLoadOp = WGPULoadOp_Undefined;
        render_pass_depth_attachment.stencilStoreOp = WGPUStoreOp_Undefined;
        render_pass_depth_attachment.stencilReadOnly = true;

        WGPURenderPassDescriptor render_pass_descr = {};
        render_pass_descr.colorAttachmentCount = 1;
        render_pass_descr.colorAttachments = &render_pass_color_attachment;
        render_pass_descr.depthStencilAttachment = &render_pass_depth_attachment;

        // Create & fill the render pass (encoder)
        WGPURenderPassEncoder render_pass = wgpuCommandEncoderBeginRenderPass(command_encoder, &render_pass_descr);

        render_opaque(render_pass, render_camera_bind_group);

        render_transparent(render_pass, render_camera_bind_group);

        render_grid(render_pass);

        wgpuRenderPassEncoderEnd(render_pass);

        wgpuRenderPassEncoderRelease(render_pass);

        WGPUCommandBufferDescriptor cmd_buff_descriptor = {};
        cmd_buff_descriptor.nextInChain = NULL;
        cmd_buff_descriptor.label = "Command buffer";

        WGPUCommandBuffer commands = wgpuCommandEncoderFinish(command_encoder, &cmd_buff_descriptor);

        wgpuQueueSubmit(webgpu_context.device_queue, 1, &commands);

        wgpuCommandBufferRelease(commands);
        wgpuCommandEncoderRelease(command_encoder);
    }

    wgpuTextureViewRelease(swapchain_view);

#ifndef __EMSCRIPTEN__
    wgpuSwapChainPresent(webgpu_context.screen_swapchain);
#endif
}

#if defined(XR_SUPPORT)
void VCTRenderer::render_xr()
{
    xr_context.init_frame();

    for (uint32_t i = 0; i < xr_context.view_count; ++i) {

        xr_context.acquire_swapchain(i);

        const sSwapchainData& swapchainData = xr_context.swapchains[i];

        camera_data.eye = xr_context.per_view_data[i].position;
        camera_data.mvp = xr_context.per_view_data[i].view_projection_matrix;
        camera_data.dummy = 0.f;

        wgpuQueueWriteBuffer(webgpu_context.device_queue, std::get<WGPUBuffer>(camera_uniform.data), 0, &camera_data, sizeof(sCameraData));

        {
            // Create the command encoder
            WGPUCommandEncoderDescriptor encoder_desc = {};
            WGPUCommandEncoder command_encoder = wgpuDeviceCreateCommandEncoder(webgpu_context.device, &encoder_desc);

            // Prepare the color attachment
            WGPURenderPassColorAttachment render_pass_color_attachment = {};
            render_pass_color_attachment.view = swapchainData.images[swapchainData.image_index].textureView;
            render_pass_color_attachment.loadOp = WGPULoadOp_Clear;
            render_pass_color_attachment.storeOp = WGPUStoreOp_Store;

            glm::vec4 clear_color = VCTRenderer::instance->get_clear_color();
            render_pass_color_attachment.clearValue = WGPUColor(clear_color.r, clear_color.g, clear_color.b, clear_color.a);

            // Prepate the depth attachment
            WGPURenderPassDepthStencilAttachment render_pass_depth_attachment = {};
            render_pass_depth_attachment.view = eye_depth_texture_view[i];
            render_pass_depth_attachment.depthClearValue = 1.0f;
            render_pass_depth_attachment.depthLoadOp = WGPULoadOp_Clear;
            render_pass_depth_attachment.depthStoreOp = WGPUStoreOp_Store;
            render_pass_depth_attachment.depthReadOnly = false;
            render_pass_depth_attachment.stencilClearValue = 0; // Stencil config necesary, even if unused
            render_pass_depth_attachment.stencilLoadOp = WGPULoadOp_Undefined;
            render_pass_depth_attachment.stencilStoreOp = WGPUStoreOp_Undefined;
            render_pass_depth_attachment.stencilReadOnly = true;

            WGPURenderPassDescriptor render_pass_descr = {};
            render_pass_descr.colorAttachmentCount = 1;
            render_pass_descr.colorAttachments = &render_pass_color_attachment;
            render_pass_descr.depthStencilAttachment = &render_pass_depth_attachment;

            // Create & fill the render pass (encoder)
            WGPURenderPassEncoder render_pass = wgpuCommandEncoderBeginRenderPass(command_encoder, &render_pass_descr);

            render_opaque(render_pass, render_camera_bind_group);

            render_transparent(render_pass, render_camera_bind_group);

            //render_my_grid(rendera_pass, render_camera_bind_group);

            wgpuRenderPassEncoderEnd(render_pass);

            wgpuRenderPassEncoderRelease(render_pass);

            WGPUCommandBufferDescriptor cmd_buff_descriptor = {};
            cmd_buff_descriptor.nextInChain = NULL;
            cmd_buff_descriptor.label = "Command buffer";

            WGPUCommandBuffer commands = wgpuCommandEncoderFinish(command_encoder, &cmd_buff_descriptor);

            wgpuQueueSubmit(webgpu_context.device_queue, 1, &commands);

            wgpuCommandBufferRelease(commands);
            wgpuCommandEncoderRelease(command_encoder);
        }


        xr_context.release_swapchain(i);
    }

    xr_context.end_frame();
}
#endif

void VCTRenderer::init_render_voxelization_pipeline()
{
    sphere_mesh = parse_mesh("data/meshes/sphere.obj");

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

    std::vector<Uniform*> uniforms = { &dynamic_cast<VCTEngine*>(VCTEngine::instance)->voxel_voxelGridPointsBuffer };
    render_voxelization_bind_group = webgpu_context->create_bind_group(uniforms, RendererStorage::get_shader("data/shaders/draw_voxel_grid.wgsl"), 1);

    render_voxelization_pipeline.create_render(RendererStorage::get_shader("data/shaders/draw_voxel_grid.wgsl"), color_target);
}

#if defined(XR_SUPPORT) && defined(USE_MIRROR_WINDOW)

void VCTRenderer::render_mirror()
{
   // Get the current texture in the swapchain
    WGPUTextureView current_texture_view = wgpuSwapChainGetCurrentTextureView(webgpu_context.screen_swapchain);
    assert(current_texture_view != NULL);

    // Create the command encoder
    WGPUCommandEncoderDescriptor encoder_desc = {};
    encoder_desc.label = "Device command encoder";

    WGPUCommandEncoder command_encoder = wgpuDeviceCreateCommandEncoder(webgpu_context.device, &encoder_desc);

    // Create & fill the render pass (encoder)
    {
        // Prepare the color attachment
        WGPURenderPassColorAttachment render_pass_color_attachment = {};
        render_pass_color_attachment.view = current_texture_view;
        render_pass_color_attachment.loadOp = WGPULoadOp_Clear;
        render_pass_color_attachment.storeOp = WGPUStoreOp_Store;
        render_pass_color_attachment.clearValue = WGPUColor(clear_color.x, clear_color.y, clear_color.z, 1.0f);

        WGPURenderPassDescriptor render_pass_descr = {};
        render_pass_descr.colorAttachmentCount = 1;
        render_pass_descr.colorAttachments = &render_pass_color_attachment;

        {
            WGPURenderPassEncoder render_pass = wgpuCommandEncoderBeginRenderPass(command_encoder, &render_pass_descr);

            // Bind Pipeline
            mirror_pipeline.set(render_pass);

            // Set binding group
            wgpuRenderPassEncoderSetBindGroup(render_pass, 0, swapchain_bind_groups[xr_context.swapchains[0].image_index], 0, nullptr);

            // Set vertex buffer while encoding the render pass
            wgpuRenderPassEncoderSetVertexBuffer(render_pass, 0, quad_surface.get_vertex_buffer(), 0, quad_surface.get_byte_size());

            // Submit drawcall
            wgpuRenderPassEncoderDraw(render_pass, 6, 1, 0, 0);

            wgpuRenderPassEncoderEnd(render_pass);

            wgpuRenderPassEncoderRelease(render_pass);
        }
    }

    WGPUCommandBufferDescriptor cmd_buff_descriptor = {};
    cmd_buff_descriptor.nextInChain = NULL;
    cmd_buff_descriptor.label = "Command buffer";

    WGPUCommandBuffer commands = wgpuCommandEncoderFinish(command_encoder, &cmd_buff_descriptor);
    wgpuQueueSubmit(webgpu_context.device_queue, 1, &commands);

    // Submit frame to mirror window
    wgpuSwapChainPresent(webgpu_context.screen_swapchain);

    wgpuCommandBufferRelease(commands);
    wgpuCommandEncoderRelease(command_encoder);
    wgpuTextureViewRelease(current_texture_view);
}

#endif

void VCTRenderer::init_camera_bind_group()
{
    camera_uniform.data = webgpu_context.create_buffer(sizeof(sCameraData), WGPUBufferUsage_CopyDst | WGPUBufferUsage_Uniform, nullptr, "camera_buffer");
    camera_uniform.binding = 0;
    camera_uniform.buffer_size = sizeof(sCameraData);
}

#if defined(XR_SUPPORT) && defined(USE_MIRROR_WINDOW)

void VCTRenderer::init_mirror_pipeline()
{
    mirror_shader = RendererStorage::get_shader("data/shaders/quad_mirror.wgsl");

    WGPUTextureFormat swapchain_format = webgpu_context.swapchain_format;

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

    // Generate uniforms from the swapchain
    for (uint8_t i = 0; i < xr_context.swapchains[0].images.size(); i++) {
        Uniform swapchain_uni;

        swapchain_uni.data = xr_context.swapchains[0].images[i].textureView;
        swapchain_uni.binding = 0;
        swapchain_uniforms.push_back(swapchain_uni);
    }

    std::vector<Uniform*> uniforms = { &swapchain_uniforms[0] };

    // Generate bindgroups from the swapchain
    for (uint8_t i = 0; i < swapchain_uniforms.size(); i++) {
        Uniform swapchain_uni;

        std::vector<Uniform*> uniforms = { &swapchain_uniforms[i] };

        swapchain_bind_groups.push_back(webgpu_context.create_bind_group(uniforms, mirror_shader, 0));
    }

    mirror_pipeline.create_render(mirror_shader, color_target);
}

#endif

void VCTRenderer::resize_window(int width, int height)
{
    Renderer::resize_window(width, height);
}
