#include "VCT_renderer.h"

#ifdef XR_SUPPORT
#include "dawnxr/dawnxr_internal.h"
#endif

#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_wgpu.h"
#include "framework/nodes/mesh_instance_3d.h"

#include "xr/openxr_context.h"

#include "graphics/renderer_storage.h"

#include "framework/scene/parse_scene.h"
#include "framework/camera/camera_2d.h"

VCTRenderer::VCTRenderer() : Renderer()
{
    
}

int VCTRenderer::initialize(GLFWwindow* window, bool use_mirror_screen)
 {
    Renderer::initialize(window, use_mirror_screen);

    clear_color = glm::vec4(0.22f, 0.22f, 0.22f, 1.0);

    init_camera_bind_group();
    init_lighting_bind_group();

#ifdef XR_SUPPORT
    if (is_openxr_available && use_mirror_screen) {
        init_mirror_pipeline();
    }
#endif

    camera = new FlyoverCamera();

    camera->set_perspective(glm::radians(45.0f), webgpu_context->render_width / static_cast<float>(webgpu_context->render_height), z_near, z_far);
    camera->look_at(glm::vec3(0.0f, 0.1f, 0.4f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
    camera->set_mouse_sensitivity(0.004f);
    camera->set_speed(0.75f);

    std::vector<Uniform*> uniforms = { &camera_uniform };
    render_bind_group_camera = webgpu_context->create_bind_group(uniforms, RendererStorage::get_shader("data/shaders/mesh_pbr.wgsl"), 1);

    // Orthographic camera for ui rendering

    float w = static_cast<float>(webgpu_context->render_width);
    float h = static_cast<float>(webgpu_context->render_height);

    camera_2d = new Camera2D();
    camera_2d->set_orthographic(0.0f, w, 0.0f, h, -1.0f, 1.0f);

    uniforms = { &camera_2d_uniform };
    render_bind_group_camera_2d = webgpu_context->create_bind_group(uniforms, RendererStorage::get_shader("data/shaders/mesh_pbr.wgsl"), 1);

    return 0;
}

void VCTRenderer::clean()
{
    Renderer::clean();

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
    // Create the command encoder
    WGPUCommandEncoderDescriptor encoder_desc = {};
    global_command_encoder = wgpuDeviceCreateCommandEncoder(webgpu_context->device, &encoder_desc);

#if defined(XR_SUPPORT)
    if (is_openxr_available) {
        xr_context->update();
    }
#endif

    if (!is_openxr_available) {
        const auto& io = ImGui::GetIO();
        if (!io.WantCaptureMouse && !io.WantCaptureKeyboard) {
            camera->update(delta_time);
        }
    }
    else {
        camera->update(delta_time);
    }
}

void VCTRenderer::render()
{
    prepare_instancing();

    WGPUTextureView swapchain_view;

    if (!is_openxr_available || use_mirror_screen) {
        swapchain_view = wgpuSwapChainGetCurrentTextureView(webgpu_context->screen_swapchain);
    }

    if (!is_openxr_available) {
        render_screen(swapchain_view);
    }

#if defined(XR_SUPPORT)
    if (is_openxr_available) {
        render_xr();

        if (use_mirror_screen) {
            render_mirror(swapchain_view);
        }
    }
#endif

    WGPUCommandBufferDescriptor cmd_buff_descriptor = {};
    cmd_buff_descriptor.nextInChain = NULL;
    cmd_buff_descriptor.label = "Command buffer";

    WGPUCommandBuffer commands = wgpuCommandEncoderFinish(global_command_encoder, &cmd_buff_descriptor);

    wgpuQueueSubmit(webgpu_context->device_queue, 1, &commands);

    wgpuCommandBufferRelease(commands);
    wgpuCommandEncoderRelease(global_command_encoder);

    if (!is_openxr_available) {
        wgpuTextureViewRelease(swapchain_view);
    }
    else {
        xr_context->end_frame();
    }

#ifndef __EMSCRIPTEN__
    if (!is_openxr_available || use_mirror_screen) {
        wgpuSwapChainPresent(webgpu_context->screen_swapchain);
    }
#endif

    clear_renderables();
}

void VCTRenderer::init_voxelization(std::vector<MeshInstance3D*> nodes)
{
    voxelization_renderer.initialize(nodes, camera_2d);
}

void VCTRenderer::render_screen(WGPUTextureView swapchain_view)
{
    camera_data.eye = camera->get_eye();
    camera_data.mvp = camera->get_view_projection();
    camera_data.dummy = 0.f;

    wgpuQueueWriteBuffer(webgpu_context->device_queue, std::get<WGPUBuffer>(camera_uniform.data), 0, &camera_data, sizeof(sCameraData));

    // Update 2d camera for UI

    camera_2d_data.eye = camera_2d->get_eye();
    camera_2d_data.mvp = camera_2d->get_view_projection();
    camera_2d_data.dummy = 0.f;

    wgpuQueueWriteBuffer(webgpu_context->device_queue, std::get<WGPUBuffer>(camera_2d_uniform.data), 0, &camera_2d_data, sizeof(sCameraData));

    ImGui::Render();

    {
        // Prepare the color attachment
        WGPURenderPassColorAttachment render_pass_color_attachment = {};
        if (msaa_count > 1) {
            render_pass_color_attachment.view = multisample_textures_views[0];
            render_pass_color_attachment.resolveTarget = swapchain_view;
        }
        else {
            render_pass_color_attachment.view = swapchain_view;
        }

        render_pass_color_attachment.loadOp = WGPULoadOp_Clear;
        render_pass_color_attachment.storeOp = WGPUStoreOp_Store;
        render_pass_color_attachment.depthSlice = WGPU_DEPTH_SLICE_UNDEFINED;

        glm::vec4 clear_color = get_clear_color();
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
        WGPURenderPassEncoder render_pass = wgpuCommandEncoderBeginRenderPass(global_command_encoder, &render_pass_descr);

        render_opaque(render_pass, render_bind_group_camera);

        render_transparent(render_pass, render_bind_group_camera);

        voxelization_renderer.render_grid(render_pass, render_bind_group_camera);

        render_2D(render_pass, render_bind_group_camera_2d);

        wgpuRenderPassEncoderEnd(render_pass);

        wgpuRenderPassEncoderRelease(render_pass);

        // render imgui
        {
            WGPURenderPassColorAttachment color_attachments = {};
            color_attachments.view = swapchain_view;
            color_attachments.loadOp = WGPULoadOp_Load;
            color_attachments.storeOp = WGPUStoreOp_Store;
            color_attachments.clearValue = { 0.0, 0.0, 0.0, 0.0 };
            color_attachments.depthSlice = WGPU_DEPTH_SLICE_UNDEFINED;

            WGPURenderPassDescriptor render_pass_desc = {};
            render_pass_desc.colorAttachmentCount = 1;
            render_pass_desc.colorAttachments = &color_attachments;
            render_pass_desc.depthStencilAttachment = nullptr;

            WGPURenderPassEncoder pass = wgpuCommandEncoderBeginRenderPass(global_command_encoder, &render_pass_desc);

            ImGui_ImplWGPU_RenderDrawData(ImGui::GetDrawData(), pass);

            wgpuRenderPassEncoderEnd(pass);
            wgpuRenderPassEncoderRelease(pass);
        }
    }
}

#if defined(XR_SUPPORT)

void VCTRenderer::render_xr()
{
    xr_context->init_frame();

    for (uint32_t i = 0; i < xr_context->view_count; ++i) {

        xr_context->acquire_swapchain(i);

        const sSwapchainData& swapchainData = xr_context->swapchains[i];

        camera_data.eye = xr_context->per_view_data[i].position;
        camera_data.mvp = xr_context->per_view_data[i].view_projection_matrix;
        camera_data.dummy = 0.f;

        wgpuQueueWriteBuffer(webgpu_context->device_queue, std::get<WGPUBuffer>(camera_uniform.data), i * camera_buffer_stride, &camera_data, sizeof(sCameraData));

        {
            // Prepare the color attachment
            WGPURenderPassColorAttachment render_pass_color_attachment = {};
            render_pass_color_attachment.view = swapchainData.images[swapchainData.image_index].textureView;

            if (msaa_count > 1) {
                render_pass_color_attachment.view = multisample_textures_views[i];
                render_pass_color_attachment.resolveTarget = swapchainData.images[swapchainData.image_index].textureView;
            }
            else {
                render_pass_color_attachment.view = swapchainData.images[swapchainData.image_index].textureView;
            }

            render_pass_color_attachment.loadOp = WGPULoadOp_Clear;
            render_pass_color_attachment.storeOp = WGPUStoreOp_Store;
            render_pass_color_attachment.depthSlice = WGPU_DEPTH_SLICE_UNDEFINED;

            glm::vec4 clear_color = get_clear_color();
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
            WGPURenderPassEncoder render_pass = wgpuCommandEncoderBeginRenderPass(global_command_encoder, &render_pass_descr);

            render_opaque(render_pass, render_bind_group_camera, i * camera_buffer_stride);

            render_transparent(render_pass, render_bind_group_camera, i * camera_buffer_stride);

            voxelization_renderer.render_grid(render_pass, render_bind_group_camera, i * camera_buffer_stride);

            render_2D(render_pass, render_bind_group_camera_2d);

            wgpuRenderPassEncoderEnd(render_pass);
            wgpuRenderPassEncoderRelease(render_pass);
        }

        xr_context->release_swapchain(i);
    }
}
#endif


#if defined(XR_SUPPORT) && defined(USE_MIRROR_WINDOW)

void VCTRenderer::render_mirror(WGPUTextureView swapchain_view)
{
    ImGui::Render();

    // Create & fill the render pass (encoder)
    {
        // Prepare the color attachment
        WGPURenderPassColorAttachment render_pass_color_attachment = {};
        render_pass_color_attachment.view = swapchain_view;
        render_pass_color_attachment.loadOp = WGPULoadOp_Clear;
        render_pass_color_attachment.storeOp = WGPUStoreOp_Store;
        render_pass_color_attachment.clearValue = WGPUColor(clear_color.x, clear_color.y, clear_color.z, 1.0f);
        render_pass_color_attachment.depthSlice = WGPU_DEPTH_SLICE_UNDEFINED;

        WGPURenderPassDescriptor render_pass_descr = {};
        render_pass_descr.colorAttachmentCount = 1;
        render_pass_descr.colorAttachments = &render_pass_color_attachment;

        {
            WGPURenderPassEncoder render_pass = wgpuCommandEncoderBeginRenderPass(global_command_encoder, &render_pass_descr);

            // Bind Pipeline
            mirror_pipeline.set(render_pass);

            // Set binding group
            wgpuRenderPassEncoderSetBindGroup(render_pass, 0, swapchain_bind_groups[xr_context->swapchains[0].image_index], 0, nullptr);

            // Set vertex buffer while encoding the render pass
            wgpuRenderPassEncoderSetVertexBuffer(render_pass, 0, quad_surface.get_vertex_buffer(), 0, quad_surface.get_byte_size());

            // Submit drawcall
            wgpuRenderPassEncoderDraw(render_pass, 6, 1, 0, 0);

            wgpuRenderPassEncoderEnd(render_pass);

            wgpuRenderPassEncoderRelease(render_pass);
        }
    }

    // render imgui
    {
        WGPURenderPassColorAttachment color_attachments = {};
        color_attachments.view = swapchain_view;
        color_attachments.loadOp = WGPULoadOp_Load;
        color_attachments.storeOp = WGPUStoreOp_Store;
        color_attachments.clearValue = { 0.0, 0.0, 0.0, 0.0 };
        color_attachments.depthSlice = WGPU_DEPTH_SLICE_UNDEFINED;

        WGPURenderPassDescriptor render_pass_desc = {};
        render_pass_desc.colorAttachmentCount = 1;
        render_pass_desc.colorAttachments = &color_attachments;
        render_pass_desc.depthStencilAttachment = nullptr;

        WGPURenderPassEncoder pass = wgpuCommandEncoderBeginRenderPass(global_command_encoder, &render_pass_desc);

        ImGui_ImplWGPU_RenderDrawData(ImGui::GetDrawData(), pass);

        wgpuRenderPassEncoderEnd(pass);
        wgpuRenderPassEncoderRelease(pass);
    }
}

#endif

void VCTRenderer::init_camera_bind_group()
{
    camera_buffer_stride = std::max(static_cast<uint32_t>(sizeof(sCameraData)), required_limits.limits.minUniformBufferOffsetAlignment);

    camera_uniform.data = webgpu_context->create_buffer(camera_buffer_stride * EYE_COUNT, WGPUBufferUsage_CopyDst | WGPUBufferUsage_Uniform, nullptr, "camera_buffer");
    camera_uniform.binding = 0;
    camera_uniform.buffer_size = sizeof(sCameraData);

    camera_2d_uniform.data = webgpu_context->create_buffer(sizeof(sCameraData), WGPUBufferUsage_CopyDst | WGPUBufferUsage_Uniform, nullptr, "camera_2d_buffer");
    camera_2d_uniform.binding = 0;
    camera_2d_uniform.buffer_size = sizeof(sCameraData);
}

#if defined(XR_SUPPORT) && defined(USE_MIRROR_WINDOW)

void VCTRenderer::init_mirror_pipeline()
{
    mirror_shader = RendererStorage::get_shader("data/shaders/quad_mirror.wgsl");

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

    // Generate uniforms from the swapchain
    for (uint8_t i = 0; i < xr_context->swapchains[0].images.size(); i++) {
        Uniform swapchain_uni;

        swapchain_uni.data = xr_context->swapchains[0].images[i].textureView;
        swapchain_uni.binding = 0;
        swapchain_uniforms.push_back(swapchain_uni);
    }

    std::vector<Uniform*> uniforms = { &swapchain_uniforms[0] };

    // Generate bindgroups from the swapchain
    for (uint8_t i = 0; i < swapchain_uniforms.size(); i++) {
        Uniform swapchain_uni;

        std::vector<Uniform*> uniforms = { &swapchain_uniforms[i] };

        swapchain_bind_groups.push_back(webgpu_context->create_bind_group(uniforms, mirror_shader, 0));
    }

    mirror_pipeline.create_render(mirror_shader, color_target);
}

#endif

void VCTRenderer::resize_window(int width, int height)
{
    Renderer::resize_window(width, height);
}
