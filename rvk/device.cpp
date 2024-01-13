
#include <imgui/imgui.h>

#include "device.h"

namespace rvk::impl {

Physical_Device::Physical_Device(VkPhysicalDevice PD) : device(PD) {

    assert(device);
    {
        u32 n_families = 0;
        vkGetPhysicalDeviceQueueFamilyProperties2(device, &n_families, null);

        available_families.resize(n_families);
        for(u32 i = 0; i < n_families; i++) {
            available_families[i].sType = VK_STRUCTURE_TYPE_QUEUE_FAMILY_PROPERTIES_2;
        }

        vkGetPhysicalDeviceQueueFamilyProperties2(device, &n_families, available_families.data());
    }
    {
        u32 n_extensions = 0;
        RVK_CHECK(vkEnumerateDeviceExtensionProperties(device, null, &n_extensions, null));
        available_extensions.resize(n_extensions);
        RVK_CHECK(vkEnumerateDeviceExtensionProperties(device, null, &n_extensions,
                                                       available_extensions.data()));
    }
    {
        properties_.memory.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_PROPERTIES_2;
        vkGetPhysicalDeviceMemoryProperties2(device, &properties_.memory);
    }
    {
        properties_.device.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
        properties_.maintenance3.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MAINTENANCE_3_PROPERTIES;
        properties_.maintenance4.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MAINTENANCE_4_PROPERTIES;
        properties_.ray_tracing.sType =
            VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR;

        properties_.device.pNext = &properties_.maintenance3;
        properties_.maintenance3.pNext = &properties_.maintenance4;
        properties_.maintenance4.pNext = &properties_.ray_tracing;

        vkGetPhysicalDeviceProperties2(device, &properties_.device);
    }
}

Opt<u32> Physical_Device::queue_index(Queue_Family f) {

    assert(f != Queue_Family::present);
    assert(available_families.length() < UINT32_MAX);

    auto check = [&](u32 flags) {
        switch(f) {
        case Queue_Family::graphics:
            return (flags & VK_QUEUE_GRAPHICS_BIT) && (flags & VK_QUEUE_TRANSFER_BIT) &&
                   (flags & VK_QUEUE_COMPUTE_BIT);
        case Queue_Family::compute:
            return (flags & VK_QUEUE_COMPUTE_BIT) && !(flags & VK_QUEUE_GRAPHICS_BIT);
        case Queue_Family::transfer:
            return (flags & VK_QUEUE_TRANSFER_BIT) && !(flags & VK_QUEUE_GRAPHICS_BIT);
        default: RPP_UNREACHABLE;
        }
    };

    for(u32 i = 0; i < available_families.length(); i++) {
        VkQueueFamilyProperties2& family = available_families[i];
        if(!family.queueFamilyProperties.queueCount) continue;
        if(check(family.queueFamilyProperties.queueFlags)) {
            return Opt<u32>{i};
        }
    }
    return {};
}

u32 Physical_Device::queue_count(Queue_Family f) {
    return available_families[*queue_index(f)].queueFamilyProperties.queueCount;
}

Opt<u32> Physical_Device::present_queue_index(VkSurfaceKHR surface) {
    assert(available_families.length() < UINT32_MAX);

    for(u32 i = 0; i < available_families.length(); i++) {
        VkQueueFamilyProperties2& family = available_families[i];
        if(!family.queueFamilyProperties.queueCount) continue;

        VkBool32 supports_present = VK_FALSE;
        RVK_CHECK(vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &supports_present));
        if(supports_present) {
            return Opt<u32>{i};
        }
    }
    return {};
}

bool Physical_Device::is_discrete() {
    return properties_.device.properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU;
}

String_View Physical_Device::name() {
    return String_View{properties_.device.properties.deviceName};
}

bool Physical_Device::supports_extension(String_View name) {
    for(auto& extension : available_extensions) {
        String_View ext_name{extension.extensionName};
        if(ext_name == name) {
            return true;
        }
    }
    return false;
}

u32 Physical_Device::n_surface_formats(VkSurfaceKHR surface) {
    u32 n_formats = 0;
    RVK_CHECK(vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &n_formats, null));
    return n_formats;
}

u32 Physical_Device::n_present_modes(VkSurfaceKHR surface) {
    u32 n_modes = 0;
    RVK_CHECK(vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &n_modes, null));
    return n_modes;
}

void Physical_Device::get_surface_formats(VkSurfaceKHR surface, Slice<VkSurfaceFormatKHR> formats) {
    u32 n = static_cast<u32>(formats.length());
    RVK_CHECK(vkGetPhysicalDeviceSurfaceFormatsKHR(
        device, surface, &n, const_cast<VkSurfaceFormatKHR*>(formats.data())));
}

void Physical_Device::get_present_modes(VkSurfaceKHR surface, Slice<VkPresentModeKHR> modes) {
    u32 n = static_cast<u32>(modes.length());
    RVK_CHECK(vkGetPhysicalDeviceSurfacePresentModesKHR(
        device, surface, &n, const_cast<VkPresentModeKHR*>(modes.data())));
}

u64 Physical_Device::heap_size(u32 heap) {
    u32 heap_idx = properties_.memory.memoryProperties.memoryTypes[heap].heapIndex;
    return properties_.memory.memoryProperties.memoryHeaps[heap_idx].size;
}

Pair<u64, u64> Physical_Device::heap_stat(u32 heap) {
    u32 heap_idx = properties_.memory.memoryProperties.memoryTypes[heap].heapIndex;

    VkPhysicalDeviceMemoryBudgetPropertiesEXT budget = {};
    budget.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_BUDGET_PROPERTIES_EXT;

    VkPhysicalDeviceMemoryProperties2KHR mem_props = {};
    mem_props.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_PROPERTIES_2_KHR;
    mem_props.pNext = &budget;

    vkGetPhysicalDeviceMemoryProperties2(device, &mem_props);

    return Pair{budget.heapUsage[heap_idx], budget.heapBudget[heap_idx]};
}

Opt<u32> Physical_Device::heap_index(u32 mask, u32 type) {
    for(u32 i = 0; i < properties_.memory.memoryProperties.memoryTypeCount; i++) {
        if((mask & (1 << i)) &&
           (properties_.memory.memoryProperties.memoryTypes[i].propertyFlags & type) == type) {
            return Opt<u32>{i};
        }
    }
    return {};
}

void Physical_Device::imgui() {
    using namespace ImGui;

    Text("Name: %s", properties_.device.properties.deviceName);
    Text("Driver: %d", properties_.device.properties.driverVersion);
    Text("API: %d", properties_.device.properties.apiVersion);
    Text("Vendor: %d", properties_.device.properties.vendorID);
    Text("Type: %s",
         properties_.device.properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU
             ? "Integrated"
             : "Discrete");

    if(TreeNode("Available Extensions")) {
        for(auto& p : available_extensions) Text("%s", p.extensionName);
        TreePop();
    }
}

} // namespace rvk::impl