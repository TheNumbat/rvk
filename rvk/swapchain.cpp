
#include <imgui/imgui.h>

#include "device.h"
#include "swapchain.h"

namespace rvk::impl {

using namespace rpp;

Frame::Frame(Arc<Device, Alloc> D, Arc<Command_Pool_Manager<Queue_Family::graphics>, Alloc>& pool)
    : device(move(D)), fence(device.dup()), cmds(pool->make()), available(device.dup()),
      complete(device.dup()) {
}

Swapchain::Swapchain(Arc<Physical_Device, Alloc>& physical_device, Arc<Device, Alloc> D,
                     VkSurfaceKHR surface, u32 frames_in_flight)
    : device(move(D)) {

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

    Profile::Time_Point end = Profile::timestamp();
    info("[rvk] Created swapchain in %ms.", Profile::ms(end - start));
}

Swapchain::~Swapchain() {
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

} // namespace rvk::impl
