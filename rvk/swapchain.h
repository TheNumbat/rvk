
#pragma once

#include <rpp/base.h>
#include <rpp/rc.h>

#include "fwd.h"
#include "memory.h"

namespace rvk::impl {

using namespace rpp;

struct Swapchain {

    explicit Swapchain(Arc<Physical_Device, Alloc>& physical_device, Arc<Device, Alloc> device,
                       VkSurfaceKHR surface, u32 frames_in_flight);
    ~Swapchain();

    Swapchain(const Swapchain&) = delete;
    Swapchain& operator=(const Swapchain&) = delete;
    Swapchain(Swapchain&&) = delete;
    Swapchain& operator=(Swapchain&&) = delete;

    VkFormat format() {
        return surface_format.format;
    }
    VkExtent2D extent() {
        return extent_;
    }
    u64 slot_count() {
        return slots.length();
    }
    u32 min_image_count() {
        return min_images;
    }
    Image_View& slot(u64 i) {
        return slots[i].view;
    }

    operator VkSwapchainKHR() {
        return swapchain;
    }

private:
    static VkExtent2D choose_extent(VkSurfaceCapabilitiesKHR capabilities);
    static VkSurfaceFormatKHR choose_format(Slice<VkSurfaceFormatKHR> formats);
    static VkPresentModeKHR choose_present_mode(Slice<VkPresentModeKHR> modes);

    Arc<Device, Alloc> device;

    struct Slot {
        Image image;
        Image_View view;
    };
    Vec<Slot, Alloc> slots;

    VkSwapchainKHR swapchain = null;

    u32 min_images = 0;
    VkExtent2D extent_ = {};
    VkPresentModeKHR present_mode = {};
    VkSurfaceFormatKHR surface_format = {};
};

} // namespace rvk::impl
