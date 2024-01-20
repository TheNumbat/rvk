
#include <imgui/imgui.h>

#include "commands.h"
#include "device.h"
#include "imgui_impl_vulkan.h"
#include "swapchain.h"

namespace rvk::impl {

using namespace rpp;

static Shader compositor_v(Arc<Device, Alloc> device);
static Shader compositor_f(Arc<Device, Alloc> device);
static Pipeline::Info compositor_pipeline_info(Arc<Swapchain, Alloc>& swapchain,
                                               Descriptor_Set_Layout& layout, Shader& v, Shader& f);
static Slice<VkDescriptorSetLayoutBinding> compositor_ds_layout();

static void swapchain_image_setup(Commands& commands, VkImage image);
static VkImageView swapchain_image_view(VkDevice device, VkImage image, VkFormat format);

Swapchain::Swapchain(Commands& cmds, Arc<Physical_Device, Alloc>& physical_device,
                     Arc<Device, Alloc> D, VkSurfaceKHR surface, u32 frames_in_flight)
    : device(move(D)), frames_in_flight(frames_in_flight) {

    Profile::Time_Point start = Profile::timestamp();

    Region(R) {
        surface_format = choose_format(physical_device->surface_formats<R>(surface).slice());
        present_mode = choose_present_mode(physical_device->present_modes<R>(surface).slice());
    }

    auto capabilities = physical_device->capabilities(surface);
    extent_ = choose_extent(capabilities);

    info("[rvk] Creating swapchain with dimensions %x%...", extent_.width, extent_.height);

    VkSwapchainCreateInfoKHR sw_info = {};
    sw_info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    sw_info.surface = surface;

    Log_Indent {
        min_images = capabilities.minImageCount;

        info("[rvk] Min image count: %", capabilities.minImageCount);
        info("[rvk] Max image count: %", capabilities.maxImageCount);

        // Theoretically, this should be FRAMES_IN_FLIGHT + 2, as we always want one
        // image available for active presentation, FRAMES_IN_FLIGHT images for rendering,
        // and one free image that is immediately available for the next presentation cycle.
        // However, at least on NV/Win11, the driver does not lock the presented image for
        // the entire presentation cycle, nor lock each image for the entire acquire->present
        // span, so we can use only FRAMES_IN_FLIGHT images to reduce latency. Technically,
        // we still have a chance of locking all images with fully pipelined composite->present
        // steps (hence missing vsync and stuttering), but this is highly unlikely because the
        // present commands are so fast - it only includes ImGui rendering and a copy to the
        // swapchain image.
        u32 images =
            Math::clamp(frames_in_flight, capabilities.minImageCount, capabilities.maxImageCount);

        info("[rvk] Using % swapchain images.", images);

        sw_info.minImageCount = images;
    }

    sw_info.imageFormat = surface_format.format;
    sw_info.imageColorSpace = surface_format.colorSpace;
    sw_info.imageExtent = extent_;
    sw_info.imageArrayLayers = 1;
    sw_info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    Array<u32, 2> queue_indices{*physical_device->queue_index(Queue_Family::graphics),
                                *physical_device->present_queue_index(surface)};

    if(queue_indices[0] != queue_indices[1]) {

        info("[rvk] Graphics and present queues have different indices: sharing swapchain.");
        sw_info.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        sw_info.queueFamilyIndexCount = static_cast<u32>(queue_indices.length());
        sw_info.pQueueFamilyIndices = queue_indices.data();

    } else {

        sw_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    }

    sw_info.preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
    sw_info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    sw_info.presentMode = present_mode;
    sw_info.clipped = VK_TRUE;

    RVK_CHECK(vkCreateSwapchainKHR(*device, &sw_info, null, &swapchain));

    {
        u32 n_images = 0;
        RVK_CHECK(vkGetSwapchainImagesKHR(*device, swapchain, &n_images, null));
        if(!n_images) {
            die("[rvk] Got zero images from swapchain!");
        }

        Region(R) {
            auto image_data = Vec<VkImage, Mregion<R>>::make(n_images);

            RVK_CHECK(vkGetSwapchainImagesKHR(*device, swapchain, &n_images, image_data.data()));
            if(!n_images) {
                die("[rvk] Got zero images from swapchain!");
            }
            info("[rvk] Got % swapchain images.", n_images);

            for(u32 i = 0; i < n_images; i++) {
                auto view = swapchain_image_view(*device, image_data[i], surface_format.format);
                slots.emplace(image_data[i], view);
                swapchain_image_setup(cmds, image_data[i]);
            }
        }
    }

    Profile::Time_Point end = Profile::timestamp();
    info("[rvk] Created swapchain in %ms.", Profile::ms(end - start));
}

Swapchain::~Swapchain() {
    for(auto& [image, view] : slots) {
        vkDestroyImageView(*device, view, null);
        image = null;
        view = null;
    }
    if(swapchain) {
        vkDestroySwapchainKHR(*device, swapchain, null);
        info("[rvk] Destroyed swapchain.");
    }
    swapchain = null;
}

VkSurfaceFormatKHR Swapchain::choose_format(Slice<VkSurfaceFormatKHR> formats) {

    VkSurfaceFormatKHR result;

    if(formats.length() == 1 && formats[0].format == VK_FORMAT_UNDEFINED) {
        result.format = VK_FORMAT_B8G8R8A8_UNORM;
        result.colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;

        warn("[rvk] Found undefined surface format, using default.");
        return result;
    }

    assert(formats.length() < UINT32_MAX);
    for(u32 i = 0; i < formats.length(); i++) {
        VkSurfaceFormatKHR fmt = formats[i];
        if(fmt.format == VK_FORMAT_B8G8R8A8_UNORM &&
           fmt.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {

            info("[rvk] Found desired swapchain surface format.");
            return fmt;
        }
    }

    warn("[rvk] Desired swapchain surface format not found, using first one.");
    return formats[0];
}

VkPresentModeKHR Swapchain::choose_present_mode(Slice<VkPresentModeKHR> modes) {
    assert(modes.length() < UINT32_MAX);

    for(u32 i = 0; i < modes.length(); i++) {
        if(modes[i] == VK_PRESENT_MODE_MAILBOX_KHR) {
            info("[rvk] Found mailbox present mode.");
            return VK_PRESENT_MODE_MAILBOX_KHR;
        }
    }

    for(u32 i = 0; i < modes.length(); i++) {
        if(modes[i] == VK_PRESENT_MODE_FIFO_RELAXED_KHR) {
            info("[rvk] Found relaxed FIFO present mode.");
            return VK_PRESENT_MODE_FIFO_RELAXED_KHR;
        }
    }

    warn("[rvk] Falling back to FIFO present mode.");
    return VK_PRESENT_MODE_FIFO_KHR;
}

VkExtent2D Swapchain::choose_extent(VkSurfaceCapabilitiesKHR capabilities) {

    VkExtent2D ext = capabilities.currentExtent;
    ext.width = Math::clamp(ext.width, capabilities.minImageExtent.width,
                            capabilities.maxImageExtent.width);
    ext.height = Math::clamp(ext.height, capabilities.minImageExtent.height,
                             capabilities.maxImageExtent.height);
    return ext;
}

Compositor::Compositor(Arc<Device, Alloc>& device, Arc<Descriptor_Pool, Alloc>& pool,
                       Arc<Swapchain, Alloc> S)
    : swapchain(move(S)), v(compositor_v(device.dup())), f(compositor_f(device.dup())),
      ds_layout(device.dup(), compositor_ds_layout()),
      ds(pool->make(ds_layout, swapchain->frame_count())),
      sampler(device.dup(), VK_FILTER_NEAREST, VK_FILTER_NEAREST),
      pipeline(Pipeline{device.dup(), compositor_pipeline_info(swapchain, ds_layout, v, f)}) {
    info("[rvk] Created compositor.");
}

Compositor::~Compositor() {
    info("[rvk] Destroyed compositor.");
}

void Compositor::render(Commands& cmds, u64 frame_index, u64 slot_index, bool has_imgui,
                        Image_View& input) {

    {
        VkDescriptorImageInfo info = {};
        info.sampler = sampler;
        info.imageView = input;
        info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

        VkWriteDescriptorSet write = {};
        write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        write.descriptorCount = 1;
        write.pImageInfo = &info;

        ds.write(frame_index, Slice{&write, 1});
    }

    VkRenderingAttachmentInfo swapchain_attachment = {};
    swapchain_attachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
    swapchain_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    swapchain_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    swapchain_attachment.imageView = swapchain->view(slot_index);
    swapchain_attachment.imageLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL;

    VkRenderingInfo info = {};
    info.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
    info.renderArea.extent = swapchain->extent();
    info.layerCount = 1;
    info.colorAttachmentCount = 1;
    info.pColorAttachments = &swapchain_attachment;

    pipeline.bind(cmds);
    pipeline.bind_set(cmds, ds);

    vkCmdBeginRendering(cmds, &info);
    vkCmdDraw(cmds, 4, 1, 0, 0);
    if(has_imgui) {
        ImGui::Render();
        if(auto draw = ImGui::GetDrawData()) ImGui_ImplVulkan_RenderDrawData(draw, cmds);
    }
    vkCmdEndRendering(cmds);
}

static Slice<VkDescriptorSetLayoutBinding> compositor_ds_layout() {
    static const VkDescriptorSetLayoutBinding binding{
        .binding = 0,
        .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        .descriptorCount = 1,
        .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
    };
    return Slice{&binding, 1};
}

static Pipeline::Info compositor_pipeline_info(Arc<Swapchain, Alloc>& swapchain,
                                               Descriptor_Set_Layout& layout, Shader& v,
                                               Shader& f) {

    static Array<VkPipelineShaderStageCreateInfo, 2> stages;
    stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    stages[0].module = v;
    stages[0].pName = "main";
    stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    stages[1].module = f;
    stages[1].pName = "main";

    static VkPipelineVertexInputStateCreateInfo v_in_info = {};
    v_in_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

    static VkPipelineInputAssemblyStateCreateInfo in_asm_info = {};
    in_asm_info.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    in_asm_info.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
    in_asm_info.primitiveRestartEnable = VK_FALSE;

    VkExtent2D extent = swapchain->extent();
    static VkViewport viewport = {};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = static_cast<f32>(extent.width);
    viewport.height = static_cast<f32>(extent.height);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;

    static VkRect2D scissor = {};
    scissor.offset = {0, 0};
    scissor.extent = extent;

    static VkPipelineViewportStateCreateInfo view_info = {};
    view_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    view_info.viewportCount = 1;
    view_info.pViewports = &viewport;
    view_info.scissorCount = 1;
    view_info.pScissors = &scissor;

    static VkPipelineRasterizationStateCreateInfo raster_info = {};
    raster_info.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    raster_info.depthClampEnable = VK_FALSE;
    raster_info.rasterizerDiscardEnable = VK_FALSE;
    raster_info.polygonMode = VK_POLYGON_MODE_FILL;
    raster_info.lineWidth = 1.0f;
    raster_info.cullMode = VK_CULL_MODE_BACK_BIT;
    raster_info.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    raster_info.depthBiasEnable = VK_FALSE;

    static VkPipelineMultisampleStateCreateInfo msaa_info = {};
    msaa_info.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    msaa_info.sampleShadingEnable = VK_FALSE;
    msaa_info.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    static VkPipelineColorBlendAttachmentState color_blend = {};
    color_blend.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                                 VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    color_blend.blendEnable = VK_TRUE;
    color_blend.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    color_blend.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    color_blend.colorBlendOp = VK_BLEND_OP_ADD;
    color_blend.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    color_blend.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
    color_blend.alphaBlendOp = VK_BLEND_OP_ADD;

    static VkPipelineColorBlendStateCreateInfo blend_info = {};
    blend_info.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    blend_info.logicOpEnable = VK_FALSE;
    blend_info.logicOp = VK_LOGIC_OP_COPY;
    blend_info.attachmentCount = 1;
    blend_info.pAttachments = &color_blend;

    static VkFormat format = {};
    static VkPipelineRenderingCreateInfo dynamic_info = {};
    dynamic_info.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
    dynamic_info.colorAttachmentCount = 1;
    format = swapchain->format();
    dynamic_info.pColorAttachmentFormats = &format;

    VkGraphicsPipelineCreateInfo pipeline_info = {};
    pipeline_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipeline_info.pNext = &dynamic_info;
    pipeline_info.stageCount = static_cast<u32>(stages.length());
    pipeline_info.pStages = stages.data();
    pipeline_info.pVertexInputState = &v_in_info;
    pipeline_info.pInputAssemblyState = &in_asm_info;
    pipeline_info.pViewportState = &view_info;
    pipeline_info.pRasterizationState = &raster_info;
    pipeline_info.pMultisampleState = &msaa_info;
    pipeline_info.pColorBlendState = &blend_info;

    static Ref<Descriptor_Set_Layout> layout_ref;
    layout_ref = Ref{layout};

    return Pipeline::Info{
        .push_constants = {},
        .descriptor_set_layouts = Slice{&layout_ref, 1},
        .info = Pipeline::VkCreateInfo{move(pipeline_info)},
    };
}

static Shader compositor_v(Arc<Device, Alloc> device) {
    unsigned char compositor_vert_spv[] = {
        0x03, 0x02, 0x23, 0x07, 0x00, 0x06, 0x01, 0x00, 0x0b, 0x00, 0x0d, 0x00, 0x35, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x11, 0x00, 0x02, 0x00, 0x01, 0x00, 0x00, 0x00, 0x0b, 0x00,
        0x06, 0x00, 0x01, 0x00, 0x00, 0x00, 0x47, 0x4c, 0x53, 0x4c, 0x2e, 0x73, 0x74, 0x64, 0x2e,
        0x34, 0x35, 0x30, 0x00, 0x00, 0x00, 0x00, 0x0e, 0x00, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x01, 0x00, 0x00, 0x00, 0x0f, 0x00, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00,
        0x00, 0x6d, 0x61, 0x69, 0x6e, 0x00, 0x00, 0x00, 0x00, 0x18, 0x00, 0x00, 0x00, 0x22, 0x00,
        0x00, 0x00, 0x2c, 0x00, 0x00, 0x00, 0x47, 0x00, 0x04, 0x00, 0x18, 0x00, 0x00, 0x00, 0x0b,
        0x00, 0x00, 0x00, 0x2a, 0x00, 0x00, 0x00, 0x48, 0x00, 0x05, 0x00, 0x20, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x0b, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x48, 0x00, 0x05,
        0x00, 0x20, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x0b, 0x00, 0x00, 0x00, 0x01, 0x00,
        0x00, 0x00, 0x48, 0x00, 0x05, 0x00, 0x20, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x0b,
        0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x48, 0x00, 0x05, 0x00, 0x20, 0x00, 0x00, 0x00,
        0x03, 0x00, 0x00, 0x00, 0x0b, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x47, 0x00, 0x03,
        0x00, 0x20, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x47, 0x00, 0x04, 0x00, 0x2c, 0x00,
        0x00, 0x00, 0x1e, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x13, 0x00, 0x02, 0x00, 0x02,
        0x00, 0x00, 0x00, 0x21, 0x00, 0x03, 0x00, 0x03, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00,
        0x16, 0x00, 0x03, 0x00, 0x06, 0x00, 0x00, 0x00, 0x20, 0x00, 0x00, 0x00, 0x17, 0x00, 0x04,
        0x00, 0x07, 0x00, 0x00, 0x00, 0x06, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x15, 0x00,
        0x04, 0x00, 0x08, 0x00, 0x00, 0x00, 0x20, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x2b,
        0x00, 0x04, 0x00, 0x08, 0x00, 0x00, 0x00, 0x09, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00,
        0x1c, 0x00, 0x04, 0x00, 0x0a, 0x00, 0x00, 0x00, 0x07, 0x00, 0x00, 0x00, 0x09, 0x00, 0x00,
        0x00, 0x2b, 0x00, 0x04, 0x00, 0x06, 0x00, 0x00, 0x00, 0x0d, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x80, 0xbf, 0x2c, 0x00, 0x05, 0x00, 0x07, 0x00, 0x00, 0x00, 0x0e, 0x00, 0x00, 0x00, 0x0d,
        0x00, 0x00, 0x00, 0x0d, 0x00, 0x00, 0x00, 0x2b, 0x00, 0x04, 0x00, 0x06, 0x00, 0x00, 0x00,
        0x0f, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x3f, 0x2c, 0x00, 0x05, 0x00, 0x07, 0x00, 0x00,
        0x00, 0x10, 0x00, 0x00, 0x00, 0x0d, 0x00, 0x00, 0x00, 0x0f, 0x00, 0x00, 0x00, 0x2c, 0x00,
        0x05, 0x00, 0x07, 0x00, 0x00, 0x00, 0x11, 0x00, 0x00, 0x00, 0x0f, 0x00, 0x00, 0x00, 0x0d,
        0x00, 0x00, 0x00, 0x2c, 0x00, 0x05, 0x00, 0x07, 0x00, 0x00, 0x00, 0x12, 0x00, 0x00, 0x00,
        0x0f, 0x00, 0x00, 0x00, 0x0f, 0x00, 0x00, 0x00, 0x2c, 0x00, 0x07, 0x00, 0x0a, 0x00, 0x00,
        0x00, 0x13, 0x00, 0x00, 0x00, 0x0e, 0x00, 0x00, 0x00, 0x10, 0x00, 0x00, 0x00, 0x11, 0x00,
        0x00, 0x00, 0x12, 0x00, 0x00, 0x00, 0x20, 0x00, 0x04, 0x00, 0x14, 0x00, 0x00, 0x00, 0x07,
        0x00, 0x00, 0x00, 0x07, 0x00, 0x00, 0x00, 0x15, 0x00, 0x04, 0x00, 0x16, 0x00, 0x00, 0x00,
        0x20, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x20, 0x00, 0x04, 0x00, 0x17, 0x00, 0x00,
        0x00, 0x01, 0x00, 0x00, 0x00, 0x16, 0x00, 0x00, 0x00, 0x3b, 0x00, 0x04, 0x00, 0x17, 0x00,
        0x00, 0x00, 0x18, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x17, 0x00, 0x04, 0x00, 0x1d,
        0x00, 0x00, 0x00, 0x06, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x2b, 0x00, 0x04, 0x00,
        0x08, 0x00, 0x00, 0x00, 0x1e, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x1c, 0x00, 0x04,
        0x00, 0x1f, 0x00, 0x00, 0x00, 0x06, 0x00, 0x00, 0x00, 0x1e, 0x00, 0x00, 0x00, 0x1e, 0x00,
        0x06, 0x00, 0x20, 0x00, 0x00, 0x00, 0x1d, 0x00, 0x00, 0x00, 0x06, 0x00, 0x00, 0x00, 0x1f,
        0x00, 0x00, 0x00, 0x1f, 0x00, 0x00, 0x00, 0x20, 0x00, 0x04, 0x00, 0x21, 0x00, 0x00, 0x00,
        0x03, 0x00, 0x00, 0x00, 0x20, 0x00, 0x00, 0x00, 0x3b, 0x00, 0x04, 0x00, 0x21, 0x00, 0x00,
        0x00, 0x22, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x2b, 0x00, 0x04, 0x00, 0x16, 0x00,
        0x00, 0x00, 0x23, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x2b, 0x00, 0x04, 0x00, 0x06,
        0x00, 0x00, 0x00, 0x25, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x20, 0x00, 0x04, 0x00,
        0x29, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x1d, 0x00, 0x00, 0x00, 0x20, 0x00, 0x04,
        0x00, 0x2b, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x07, 0x00, 0x00, 0x00, 0x3b, 0x00,
        0x04, 0x00, 0x2b, 0x00, 0x00, 0x00, 0x2c, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x2b,
        0x00, 0x04, 0x00, 0x06, 0x00, 0x00, 0x00, 0x31, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3f,
        0x2c, 0x00, 0x05, 0x00, 0x07, 0x00, 0x00, 0x00, 0x32, 0x00, 0x00, 0x00, 0x31, 0x00, 0x00,
        0x00, 0x31, 0x00, 0x00, 0x00, 0x20, 0x00, 0x04, 0x00, 0x34, 0x00, 0x00, 0x00, 0x07, 0x00,
        0x00, 0x00, 0x0a, 0x00, 0x00, 0x00, 0x36, 0x00, 0x05, 0x00, 0x02, 0x00, 0x00, 0x00, 0x04,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0xf8, 0x00, 0x02, 0x00,
        0x05, 0x00, 0x00, 0x00, 0x3b, 0x00, 0x04, 0x00, 0x34, 0x00, 0x00, 0x00, 0x0c, 0x00, 0x00,
        0x00, 0x07, 0x00, 0x00, 0x00, 0x3e, 0x00, 0x03, 0x00, 0x0c, 0x00, 0x00, 0x00, 0x13, 0x00,
        0x00, 0x00, 0x3d, 0x00, 0x04, 0x00, 0x16, 0x00, 0x00, 0x00, 0x19, 0x00, 0x00, 0x00, 0x18,
        0x00, 0x00, 0x00, 0x41, 0x00, 0x05, 0x00, 0x14, 0x00, 0x00, 0x00, 0x1b, 0x00, 0x00, 0x00,
        0x0c, 0x00, 0x00, 0x00, 0x19, 0x00, 0x00, 0x00, 0x3d, 0x00, 0x04, 0x00, 0x07, 0x00, 0x00,
        0x00, 0x1c, 0x00, 0x00, 0x00, 0x1b, 0x00, 0x00, 0x00, 0x51, 0x00, 0x05, 0x00, 0x06, 0x00,
        0x00, 0x00, 0x26, 0x00, 0x00, 0x00, 0x1c, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x51,
        0x00, 0x05, 0x00, 0x06, 0x00, 0x00, 0x00, 0x27, 0x00, 0x00, 0x00, 0x1c, 0x00, 0x00, 0x00,
        0x01, 0x00, 0x00, 0x00, 0x50, 0x00, 0x07, 0x00, 0x1d, 0x00, 0x00, 0x00, 0x28, 0x00, 0x00,
        0x00, 0x26, 0x00, 0x00, 0x00, 0x27, 0x00, 0x00, 0x00, 0x25, 0x00, 0x00, 0x00, 0x0f, 0x00,
        0x00, 0x00, 0x41, 0x00, 0x05, 0x00, 0x29, 0x00, 0x00, 0x00, 0x2a, 0x00, 0x00, 0x00, 0x22,
        0x00, 0x00, 0x00, 0x23, 0x00, 0x00, 0x00, 0x3e, 0x00, 0x03, 0x00, 0x2a, 0x00, 0x00, 0x00,
        0x28, 0x00, 0x00, 0x00, 0x0c, 0x00, 0x08, 0x00, 0x07, 0x00, 0x00, 0x00, 0x33, 0x00, 0x00,
        0x00, 0x01, 0x00, 0x00, 0x00, 0x32, 0x00, 0x00, 0x00, 0x1c, 0x00, 0x00, 0x00, 0x32, 0x00,
        0x00, 0x00, 0x32, 0x00, 0x00, 0x00, 0x3e, 0x00, 0x03, 0x00, 0x2c, 0x00, 0x00, 0x00, 0x33,
        0x00, 0x00, 0x00, 0xfd, 0x00, 0x01, 0x00, 0x38, 0x00, 0x01, 0x00};
    return Shader{move(device), Slice{compositor_vert_spv, sizeof(compositor_vert_spv)}};
}

static Shader compositor_f(Arc<Device, Alloc> device) {
    static unsigned char compositor_frag_spv[] = {
        0x03, 0x02, 0x23, 0x07, 0x00, 0x06, 0x01, 0x00, 0x0b, 0x00, 0x0d, 0x00, 0x14, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x11, 0x00, 0x02, 0x00, 0x01, 0x00, 0x00, 0x00, 0x0b, 0x00,
        0x06, 0x00, 0x01, 0x00, 0x00, 0x00, 0x47, 0x4c, 0x53, 0x4c, 0x2e, 0x73, 0x74, 0x64, 0x2e,
        0x34, 0x35, 0x30, 0x00, 0x00, 0x00, 0x00, 0x0e, 0x00, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x01, 0x00, 0x00, 0x00, 0x0f, 0x00, 0x08, 0x00, 0x04, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00,
        0x00, 0x6d, 0x61, 0x69, 0x6e, 0x00, 0x00, 0x00, 0x00, 0x09, 0x00, 0x00, 0x00, 0x0d, 0x00,
        0x00, 0x00, 0x11, 0x00, 0x00, 0x00, 0x10, 0x00, 0x03, 0x00, 0x04, 0x00, 0x00, 0x00, 0x07,
        0x00, 0x00, 0x00, 0x47, 0x00, 0x04, 0x00, 0x09, 0x00, 0x00, 0x00, 0x1e, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x47, 0x00, 0x04, 0x00, 0x0d, 0x00, 0x00, 0x00, 0x22, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x47, 0x00, 0x04, 0x00, 0x0d, 0x00, 0x00, 0x00, 0x21, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x47, 0x00, 0x04, 0x00, 0x11, 0x00, 0x00, 0x00, 0x1e,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x13, 0x00, 0x02, 0x00, 0x02, 0x00, 0x00, 0x00,
        0x21, 0x00, 0x03, 0x00, 0x03, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x16, 0x00, 0x03,
        0x00, 0x06, 0x00, 0x00, 0x00, 0x20, 0x00, 0x00, 0x00, 0x17, 0x00, 0x04, 0x00, 0x07, 0x00,
        0x00, 0x00, 0x06, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x20, 0x00, 0x04, 0x00, 0x08,
        0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x07, 0x00, 0x00, 0x00, 0x3b, 0x00, 0x04, 0x00,
        0x08, 0x00, 0x00, 0x00, 0x09, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x19, 0x00, 0x09,
        0x00, 0x0a, 0x00, 0x00, 0x00, 0x06, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x1b, 0x00, 0x03, 0x00, 0x0b, 0x00, 0x00, 0x00, 0x0a, 0x00, 0x00, 0x00,
        0x20, 0x00, 0x04, 0x00, 0x0c, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0b, 0x00, 0x00,
        0x00, 0x3b, 0x00, 0x04, 0x00, 0x0c, 0x00, 0x00, 0x00, 0x0d, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x17, 0x00, 0x04, 0x00, 0x0f, 0x00, 0x00, 0x00, 0x06, 0x00, 0x00, 0x00, 0x02,
        0x00, 0x00, 0x00, 0x20, 0x00, 0x04, 0x00, 0x10, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00,
        0x0f, 0x00, 0x00, 0x00, 0x3b, 0x00, 0x04, 0x00, 0x10, 0x00, 0x00, 0x00, 0x11, 0x00, 0x00,
        0x00, 0x01, 0x00, 0x00, 0x00, 0x36, 0x00, 0x05, 0x00, 0x02, 0x00, 0x00, 0x00, 0x04, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0xf8, 0x00, 0x02, 0x00, 0x05,
        0x00, 0x00, 0x00, 0x3d, 0x00, 0x04, 0x00, 0x0b, 0x00, 0x00, 0x00, 0x0e, 0x00, 0x00, 0x00,
        0x0d, 0x00, 0x00, 0x00, 0x3d, 0x00, 0x04, 0x00, 0x0f, 0x00, 0x00, 0x00, 0x12, 0x00, 0x00,
        0x00, 0x11, 0x00, 0x00, 0x00, 0x57, 0x00, 0x05, 0x00, 0x07, 0x00, 0x00, 0x00, 0x13, 0x00,
        0x00, 0x00, 0x0e, 0x00, 0x00, 0x00, 0x12, 0x00, 0x00, 0x00, 0x3e, 0x00, 0x03, 0x00, 0x09,
        0x00, 0x00, 0x00, 0x13, 0x00, 0x00, 0x00, 0xfd, 0x00, 0x01, 0x00, 0x38, 0x00, 0x01, 0x00};
    return Shader{move(device), Slice{compositor_frag_spv, sizeof(compositor_frag_spv)}};
}

static VkImageView swapchain_image_view(VkDevice device, VkImage image, VkFormat format) {

    VkImageView view = null;

    VkImageViewCreateInfo view_info = {};
    view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    view_info.image = image;
    view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
    view_info.format = format;
    view_info.components.r = VK_COMPONENT_SWIZZLE_R;
    view_info.components.g = VK_COMPONENT_SWIZZLE_G;
    view_info.components.b = VK_COMPONENT_SWIZZLE_B;
    view_info.components.a = VK_COMPONENT_SWIZZLE_A;
    view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    view_info.subresourceRange.baseMipLevel = 0;
    view_info.subresourceRange.levelCount = 1;
    view_info.subresourceRange.baseArrayLayer = 0;
    view_info.subresourceRange.layerCount = 1;

    RVK_CHECK(vkCreateImageView(device, &view_info, null, &view));
    return view;
}

static void swapchain_image_setup(Commands& commands, VkImage image) {

    VkImageMemoryBarrier2 barrier = {};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
    barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    barrier.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = image;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;
    barrier.srcStageMask = VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT;
    barrier.srcAccessMask = VK_ACCESS_2_NONE;
    barrier.dstStageMask = VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT;
    barrier.dstAccessMask = VK_ACCESS_2_NONE;

    VkDependencyInfo dependency = {};
    dependency.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
    dependency.imageMemoryBarrierCount = 1;
    dependency.pImageMemoryBarriers = &barrier;
    dependency.dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

    vkCmdPipelineBarrier2(commands, &dependency);
}

} // namespace rvk::impl
