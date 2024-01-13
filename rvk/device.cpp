
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

Device::Device(Arc<Physical_Device, Alloc> P, Slice<String_View> extensions,
               VkPhysicalDeviceFeatures2& features, VkSurfaceKHR surface)
    : physical_device(move(P)) {

    Profile::Time_Point start = Profile::timestamp();

    info("[vrk] Creating device...");
    Log_Indent {

        if(auto idx = physical_device->queue_index(Queue_Family::graphics)) {
            graphics_family_index = *idx;
        } else {
            die("[rvk] No graphics queue family found.");
        }

        if(auto idx = physical_device->present_queue_index(surface)) {
            present_family_index = *idx;
        } else {
            die("[rvk] No present queue family found.");
        }

        compute_family_index =
            physical_device->queue_index(Queue_Family::compute).value_or(graphics_family_index);
        transfer_family_index =
            physical_device->queue_index(Queue_Family::transfer).value_or(graphics_family_index);

        u32 n_graphics_queues = physical_device->queue_count(Queue_Family::graphics);
        u32 n_compute_queues = physical_device->queue_count(Queue_Family::compute);
        u32 n_transfer_queues = physical_device->queue_count(Queue_Family::transfer);

        u32 max_queues =
            Math::max(n_graphics_queues, Math::max(n_compute_queues, n_transfer_queues));
        if(max_queues == 0) {
            die("[rvk] No queues found.");
        }

        Region(R) {
            auto priorities = Vec<f32, Mregion<R>>::make(max_queues);
            priorities[0] = 1.0f;

            for(u64 i = 1; i < priorities.length(); i++) {
                priorities[i] = priorities[i - 1] * 0.9f;
            }

            Vec<VkDeviceQueueCreateInfo, Mregion<R>> queue_infos;
            {
                VkDeviceQueueCreateInfo qinfo = {};
                qinfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
                qinfo.queueFamilyIndex = graphics_family_index;
                qinfo.queueCount = n_graphics_queues;
                qinfo.pQueuePriorities = priorities.data();
                queue_infos.push(qinfo);
            }

            for(f32& p : priorities) {
                p *= 0.9f;
            }

            if(compute_family_index != graphics_family_index) {
                VkDeviceQueueCreateInfo qinfo = {};
                qinfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
                qinfo.queueFamilyIndex = compute_family_index;
                qinfo.queueCount = n_compute_queues;
                qinfo.pQueuePriorities = priorities.data();
                queue_infos.push(qinfo);
            } else {
                info("[rvk] Using graphics queue family as compute queue family.");
            }
            if(transfer_family_index != graphics_family_index) {
                VkDeviceQueueCreateInfo qinfo = {};
                qinfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
                qinfo.queueFamilyIndex = transfer_family_index;
                qinfo.queueCount = n_transfer_queues;
                qinfo.pQueuePriorities = priorities.data();
                queue_infos.push(qinfo);
            } else {
                info("[rvk] Using graphics queue family as transfer queue family.");
            }
            if(present_family_index != graphics_family_index) {
                VkDeviceQueueCreateInfo qinfo = {};
                qinfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
                qinfo.queueFamilyIndex = present_family_index;
                qinfo.queueCount = 1;
                qinfo.pQueuePriorities = priorities.data();
                queue_infos.push(qinfo);
            } else {
                info("[rvk] Using graphics queue family as present queue family.");
            }

            Vec<const char*, Mregion<R>> vk_extensions;
            for(auto& ext : extensions) {
                vk_extensions.push(
                    reinterpret_cast<const char*>(ext.terminate<Mregion<R>>().data()));
            }

            vk_extensions.push(VK_EXT_PAGEABLE_DEVICE_LOCAL_MEMORY_EXTENSION_NAME);
            vk_extensions.push(VK_EXT_MEMORY_BUDGET_EXTENSION_NAME);
            vk_extensions.push(VK_EXT_MEMORY_PRIORITY_EXTENSION_NAME);
            vk_extensions.push(VK_EXT_ROBUSTNESS_2_EXTENSION_NAME);
#ifdef RPP_OS_WINDOWS
            vk_extensions.push(VK_KHR_EXTERNAL_FENCE_WIN32_EXTENSION_NAME);
#else
            vk_extensions.push(VK_KHR_EXTERNAL_FENCE_FD_EXTENSION_NAME);
#endif

            VkDeviceCreateInfo dev_info = {};
            dev_info.pNext = &features;
            features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
            dev_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
            dev_info.queueCreateInfoCount = static_cast<u32>(queue_infos.length());
            dev_info.pQueueCreateInfos = queue_infos.data();
            dev_info.enabledExtensionCount = static_cast<u32>(vk_extensions.length());
            dev_info.ppEnabledExtensionNames = vk_extensions.data();

            RVK_CHECK(vkCreateDevice(*physical_device, &dev_info, null, &device));

            graphics_qs.resize(n_graphics_queues);
            compute_qs.resize(n_compute_queues);
            transfer_qs.resize(n_transfer_queues);

            VkDeviceQueueInfo2 info = {};
            info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_INFO_2;

            info.queueFamilyIndex = graphics_family_index;
            for(u32 i = 0; i < n_graphics_queues; i++) {
                info.queueIndex = i;
                vkGetDeviceQueue2(device, &info, &graphics_qs[i]);
            }

            info.queueFamilyIndex = compute_family_index;
            for(u32 i = 0; i < n_compute_queues; i++) {
                info.queueIndex = i;
                vkGetDeviceQueue2(device, &info, &compute_qs[i]);
            }

            info.queueFamilyIndex = transfer_family_index;
            for(u32 i = 0; i < n_transfer_queues; i++) {
                info.queueIndex = i;
                vkGetDeviceQueue2(device, &info, &transfer_qs[i]);
            }

            info.queueFamilyIndex = present_family_index;
            info.queueIndex = 0;
            vkGetDeviceQueue2(device, &info, &present_q);
        }

        info("[rvk] Got % graphics queues from family %.", n_graphics_queues,
             graphics_family_index);
        info("[rvk] Got % compute queues from family %.", n_compute_queues, compute_family_index);
        info("[rvk] Got % transfer queues from family %.", n_transfer_queues,
             transfer_family_index);
        info("[rvk] Got present queue from family %.", present_family_index);

        if(auto idx =
               physical_device->heap_index(RPP_UINT32_MAX, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)) {
            device_memory_index = *idx;
        } else {
            die("[rvk] No device local heap found.");
        }

        if(auto idx = physical_device->heap_index(RPP_UINT32_MAX,
                                                  VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                                      VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)) {
            host_memory_index = *idx;
        } else {
            die("[rvk] No host visible heap found.");
        }

        info("[rvk] Found device and host heaps (%mb, %mb).", heap_size(Heap::device) / Math::MB(1),
             heap_size(Heap::host) / Math::MB(1));

        volkLoadDevice(device);
        info("[rvk] Loaded device functions.");

        Profile::Time_Point end = Profile::timestamp();
        info("[rvk] Finished creating device in %ms.", Profile::ms(end - start));
    }
}

Device::~Device() {
    if(device) {
        vkDeviceWaitIdle(device);
        vkDestroyDevice(device, null);
        info("[rvk] Destroyed device.");
    }
    device = null;
}

u32 Device::heap_index(Heap heap) {
    switch(heap) {
    case Heap::device: return device_memory_index;
    case Heap::host: return host_memory_index;
    default: RPP_UNREACHABLE;
    }
}

u64 Device::heap_size(Heap heap) {
    switch(heap) {
    case Heap::device: return physical_device->heap_size(device_memory_index);
    case Heap::host: return physical_device->heap_size(host_memory_index);
    default: RPP_UNREACHABLE;
    }
}

u64 Device::non_coherent_atom_size() {
    return physical_device->properties().device.properties.limits.nonCoherentAtomSize;
}

u64 Device::sbt_handle_size() {
    return physical_device->properties().ray_tracing.shaderGroupHandleSize;
}

u64 Device::sbt_handle_alignment() {
    return physical_device->properties().ray_tracing.shaderGroupBaseAlignment;
}

u64 Device::queue_count(Queue_Family family) {
    switch(family) {
    case Queue_Family::transfer: return transfer_qs.length();
    case Queue_Family::graphics: return graphics_qs.length();
    case Queue_Family::compute: return compute_qs.length();
    case Queue_Family::present: return 1;
    default: RPP_UNREACHABLE;
    }
}

VkQueue Device::queue(Queue_Family family, u32 index) {
    switch(family) {
    case Queue_Family::transfer: return transfer_qs[index];
    case Queue_Family::graphics: return graphics_qs[index];
    case Queue_Family::compute: return compute_qs[index];
    case Queue_Family::present: return present_q;
    default: RPP_UNREACHABLE;
    }
}

u32 Device::queue_index(Queue_Family family) {
    switch(family) {
    case Queue_Family::transfer: return transfer_family_index;
    case Queue_Family::graphics: return graphics_family_index;
    case Queue_Family::compute: return compute_family_index;
    case Queue_Family::present: return present_family_index;
    default: RPP_UNREACHABLE;
    }
}

void Device::imgui() {
    using namespace ImGui;

    Text("Device heap: %d (%d mb)", device_memory_index, heap_size(Heap::device) / Math::MB(1));
    Text("Host heap: %d (%d mb)", host_memory_index, heap_size(Heap::host) / Math::MB(1));
    Text("Graphics family: %d", graphics_family_index);
    Text("Compute family: %d", compute_family_index);
    Text("Transfer family: %d", transfer_family_index);
    Text("Present family: %d", present_family_index);

    Text("Non coherent atom size: %lu", non_coherent_atom_size());
    Text("SBT handle size: %lu", sbt_handle_size());
    Text("SBT handle alignment: %lu", sbt_handle_alignment());

    if(TreeNode("Enabled Extensions")) {
        for(auto& ext : enabled_extensions)
            Text("%.*s", ext.length(), reinterpret_cast<const char*>(ext.data()));
        TreePop();
    }
}

} // namespace rvk::impl