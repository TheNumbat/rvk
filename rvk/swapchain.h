
#pragma once

#include <rpp/base.h>
#include <rpp/rc.h>

#include "fwd.h"

#include "commands.h"
#include "memory.h"
#include "pipeline.h"

namespace rvk::impl {

using namespace rpp;

struct Compositor {

    explicit Compositor(Arc<Device, Alloc> device, Arc<Swapchain, Alloc> swapchain,
                        Arc<Descriptor_Pool, Alloc>& pool);
    ~Compositor();

    Compositor(const Compositor&) = delete;
    Compositor& operator=(const Compositor&) = delete;
    Compositor(Compositor&&) = delete;
    Compositor& operator=(Compositor&&) = delete;

    void render(Commands& cmds, u64 frame_index, u64 slot_index, bool has_imgui, Image_View& input);

private:
    Arc<Device, Alloc> device;
    Arc<Swapchain, Alloc> swapchain;

    Shader v, f;
    Descriptor_Set_Layout ds_layout;
    Descriptor_Set ds;
    Sampler sampler;
    Pipeline pipeline;
};

struct Swapchain {

    explicit Swapchain(Commands& cmds, Arc<Physical_Device, Alloc>& physical_device,
                       Arc<Device, Alloc> device, VkSurfaceKHR surface, u32 frames_in_flight,
                       bool hdr);
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
    VkImageView view(u64 index) {
        return slots[index].view;
    }
    u64 slot_count() {
        return slots.length();
    }
    u32 min_image_count() {
        return min_images;
    }
    u32 frame_count() {
        return frames_in_flight;
    }

    operator VkSwapchainKHR() const {
        return swapchain;
    }

    static VkExtent2D choose_extent(VkSurfaceCapabilitiesKHR capabilities);

private:
    static VkSurfaceFormatKHR choose_format(Slice<const VkSurfaceFormatKHR> formats, bool hdr);
    static VkPresentModeKHR choose_present_mode(Slice<const VkPresentModeKHR> modes);

    Arc<Device, Alloc> device;

    struct Slot {
        VkImage image;
        VkImageView view;
    };
    Vec<Slot, Alloc> slots;

    VkSwapchainKHR swapchain = null;

    u32 min_images = 0;
    u32 frames_in_flight = 0;
    VkExtent2D extent_ = {};
    VkPresentModeKHR present_mode = {};
    VkSurfaceFormatKHR surface_format = {};
};

} // namespace rvk::impl
