
#pragma once

#include <rpp/base.h>
#include <rpp/rc.h>

#include "fwd.h"

namespace rvk {

using namespace rpp;

enum class Queue_Family : u8 { graphics, present, compute, transfer };
enum class Heap : u8 { device, host };

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

struct Device {

    explicit Device(Rc<Physical_Device, Alloc> physical_device, Slice<String_View> extensions,
                    VkPhysicalDeviceFeatures2& features, VkSurfaceKHR surface);
    ~Device();

    Device(const Device&) = delete;
    Device& operator=(const Device&) = delete;
    Device(Device&&) = delete;
    Device& operator=(Device&&) = delete;

    void imgui();

    u32 heap_index(Heap heap);
    u64 heap_size(Heap heap);

    u64 non_coherent_atom_size();
    u64 sbt_handle_size();
    u64 sbt_handle_alignment();

    u32 queue_index(Queue_Family family);
    u64 queue_count(Queue_Family family);
    VkQueue queue(Queue_Family family, u32 index = 0);

    operator VkDevice() const {
        return device;
    }

private:
    Rc<Physical_Device, Alloc> physical_device;
    Vec<String<Alloc>, Alloc> enabled_extensions;

    VkDevice device = null;

    u32 device_memory_index = 0;
    u32 host_memory_index = 0;
    u32 graphics_family_index = 0;
    u32 present_family_index = 0;
    u32 compute_family_index = 0;
    u32 transfer_family_index = 0;

    VkQueue present_q = null;
    Vec<VkQueue, Alloc> graphics_qs;
    Vec<VkQueue, Alloc> compute_qs;
    Vec<VkQueue, Alloc> transfer_qs;
};

} // namespace impl
} // namespace rvk

RPP_NAMED_ENUM(rvk::Queue_Family, "Queue_Family", graphics, RPP_CASE(graphics), RPP_CASE(present),
               RPP_CASE(compute), RPP_CASE(transfer));
