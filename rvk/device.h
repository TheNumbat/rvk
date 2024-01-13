
#pragma once

#include <rpp/base.h>

#include "fwd.h"

namespace rvk {

using namespace rpp;

enum class Queue_Family : u8 { graphics, present, compute, transfer };

namespace impl {

struct Physical_Device {

    struct Properties {
        VkPhysicalDeviceProperties2 device = {};
        VkPhysicalDeviceMemoryProperties2 memory = {};
        VkPhysicalDeviceMaintenance3Properties maintenance3 = {};
        VkPhysicalDeviceMaintenance4Properties maintenance4 = {};
        VkPhysicalDeviceRayTracingPipelinePropertiesKHR ray_tracing = {};
    };

    explicit Physical_Device(VkPhysicalDevice device);
    ~Physical_Device() = default;

    Physical_Device(const Physical_Device&) = delete;
    Physical_Device& operator=(const Physical_Device&) = delete;
    Physical_Device(Physical_Device&&) = delete;
    Physical_Device& operator=(Physical_Device&&) = delete;

    void imgui();

    u32 queue_count(Queue_Family family);
    Opt<u32> queue_index(Queue_Family family);
    Opt<u32> present_queue_index(VkSurfaceKHR surface);

    u64 heap_size(u32 heap);
    Pair<u64, u64> heap_stat(u32 heap);
    Opt<u32> heap_index(u32 mask, u32 properties);

    String_View name();
    bool is_discrete();
    bool supports_extension(String_View name);

    const Properties& properties() const {
        return properties_;
    }
    operator VkPhysicalDevice() {
        return device;
    }

    template<Region R>
    Vec<VkSurfaceFormatKHR, Mregion<R>> surface_formats(VkSurfaceKHR surface) {
        auto formats = Vec<VkSurfaceFormatKHR, Mregion<R>>::make(n_surface_formats(surface));
        get_surface_formats(surface, formats.slice());
        return formats;
    }
    template<Region R>
    Vec<VkPresentModeKHR, Mregion<R>> present_modes(VkSurfaceKHR surface) {
        auto modes = Vec<VkPresentModeKHR, Mregion<R>>::make(n_present_modes(surface));
        get_present_modes(surface, modes.slice());
        return modes;
    }

private:
    u32 n_surface_formats(VkSurfaceKHR surface);
    u32 n_present_modes(VkSurfaceKHR surface);
    void get_surface_formats(VkSurfaceKHR surface, Slice<VkSurfaceFormatKHR> formats);
    void get_present_modes(VkSurfaceKHR surface, Slice<VkPresentModeKHR> modes);

    VkPhysicalDevice device = null;
    Properties properties_;
    Vec<VkExtensionProperties, Alloc> available_extensions;
    Vec<VkQueueFamilyProperties2, Alloc> available_families;
};

} // namespace impl
} // namespace rvk

RPP_NAMED_ENUM(rvk::Queue_Family, "Queue_Family", graphics, RPP_CASE(graphics), RPP_CASE(present),
               RPP_CASE(compute), RPP_CASE(transfer));
