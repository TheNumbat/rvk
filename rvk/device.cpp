
#include <imgui/imgui.h>

#include "commands.h"
#include "device.h"

static VkPhysicalDeviceFeatures2* baseline_features(bool ray_tracing, bool robustness) {

    static VkPhysicalDeviceRayTracingPositionFetchFeaturesKHR ray_position_fetch_features = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_POSITION_FETCH_FEATURES_KHR,
        .rayTracingPositionFetch = VK_TRUE,
    };

    static VkPhysicalDeviceRayTracingMaintenance1FeaturesKHR ray_maintain_features = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_MAINTENANCE_1_FEATURES_KHR,
        .pNext = &ray_position_fetch_features,
        .rayTracingMaintenance1 = VK_TRUE,
        .rayTracingPipelineTraceRaysIndirect2 = VK_TRUE,
    };

    static VkPhysicalDeviceRayQueryFeaturesKHR ray_query_features = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_QUERY_FEATURES_KHR,
        .pNext = &ray_maintain_features,
        .rayQuery = VK_TRUE,
    };

    static VkPhysicalDeviceRayTracingPipelineFeaturesKHR ray_pipeline_features = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR,
        .pNext = &ray_query_features,
        .rayTracingPipeline = VK_TRUE,
        .rayTracingPipelineTraceRaysIndirect = VK_TRUE,
        .rayTraversalPrimitiveCulling = VK_TRUE,
    };

    static VkPhysicalDeviceAccelerationStructureFeaturesKHR ray_accel_features = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR,
        .pNext = &ray_pipeline_features,
        .accelerationStructure = VK_TRUE,
        .accelerationStructureCaptureReplay = VK_TRUE,
    };

    static VkPhysicalDeviceShaderClockFeaturesKHR shader_clock_features = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_CLOCK_FEATURES_KHR,
        .pNext = ray_tracing ? &ray_accel_features : null,
        .shaderSubgroupClock = VK_TRUE,
        .shaderDeviceClock = VK_TRUE,
    };

    static VkPhysicalDeviceVulkan13Features vk13_features = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES,
        .pNext = &shader_clock_features,
        .robustImageAccess = robustness,
        .synchronization2 = VK_TRUE,
        .dynamicRendering = VK_TRUE,
        .maintenance4 = VK_TRUE,
    };

    static VkPhysicalDeviceVulkan12Features vk12_features = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES,
        .pNext = &vk13_features,
        .drawIndirectCount = VK_TRUE,
        .storageBuffer8BitAccess = VK_TRUE,
        .uniformAndStorageBuffer8BitAccess = VK_TRUE,
        .shaderBufferInt64Atomics = VK_TRUE,
        .shaderSharedInt64Atomics = VK_TRUE,
        .shaderFloat16 = VK_TRUE,
        .shaderInt8 = VK_TRUE,
        .descriptorIndexing = VK_TRUE,
        .shaderInputAttachmentArrayDynamicIndexing = VK_TRUE,
        .shaderUniformTexelBufferArrayDynamicIndexing = VK_TRUE,
        .shaderStorageTexelBufferArrayDynamicIndexing = VK_TRUE,
        .shaderUniformBufferArrayNonUniformIndexing = VK_TRUE,
        .shaderSampledImageArrayNonUniformIndexing = VK_TRUE,
        .shaderStorageBufferArrayNonUniformIndexing = VK_TRUE,
        .shaderStorageImageArrayNonUniformIndexing = VK_TRUE,
        .shaderInputAttachmentArrayNonUniformIndexing = VK_TRUE,
        .shaderUniformTexelBufferArrayNonUniformIndexing = VK_TRUE,
        .shaderStorageTexelBufferArrayNonUniformIndexing = VK_TRUE,
        .descriptorBindingPartiallyBound = VK_TRUE,
        .descriptorBindingVariableDescriptorCount = VK_TRUE,
        .runtimeDescriptorArray = VK_TRUE,
        .scalarBlockLayout = VK_TRUE,
        .imagelessFramebuffer = VK_TRUE,
        .uniformBufferStandardLayout = VK_TRUE,
        .separateDepthStencilLayouts = VK_TRUE,
        .bufferDeviceAddress = VK_TRUE,
        .vulkanMemoryModel = VK_TRUE,
        .vulkanMemoryModelDeviceScope = VK_TRUE,
    };

    static VkPhysicalDeviceVulkan11Features vk11_features = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES,
        .pNext = &vk12_features,
        .storageBuffer16BitAccess = VK_TRUE,
        .uniformAndStorageBuffer16BitAccess = VK_TRUE,
        .variablePointersStorageBuffer = VK_TRUE,
        .variablePointers = VK_TRUE,
    };

    static VkPhysicalDeviceRobustness2FeaturesEXT robust_features = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ROBUSTNESS_2_FEATURES_EXT,
        .pNext = &vk11_features,
        .robustBufferAccess2 = robustness,
        .robustImageAccess2 = robustness,
        .nullDescriptor = VK_TRUE,
    };

    static VkPhysicalDeviceMemoryPriorityFeaturesEXT memory_features = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_PRIORITY_FEATURES_EXT,
        .pNext = &robust_features,
        .memoryPriority = VK_TRUE,
    };

    static VkPhysicalDeviceFeatures2 features2 = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,
        .pNext = &memory_features,
        .features =
            {
                .robustBufferAccess = robustness,
                .imageCubeArray = VK_TRUE,
                .geometryShader = VK_TRUE,
                .tessellationShader = VK_TRUE,
                .sampleRateShading = VK_TRUE,
                .dualSrcBlend = VK_TRUE,
                .logicOp = VK_TRUE,
                .multiDrawIndirect = VK_TRUE,
                .drawIndirectFirstInstance = VK_TRUE,
                .depthClamp = VK_TRUE,
                .depthBiasClamp = VK_TRUE,
                .fillModeNonSolid = VK_TRUE,
                .depthBounds = VK_TRUE,
                .wideLines = VK_TRUE,
                .largePoints = VK_TRUE,
                .samplerAnisotropy = VK_TRUE,
                .occlusionQueryPrecise = VK_TRUE,
                .pipelineStatisticsQuery = VK_TRUE,
                .vertexPipelineStoresAndAtomics = VK_TRUE,
                .fragmentStoresAndAtomics = VK_TRUE,
                .shaderTessellationAndGeometryPointSize = VK_TRUE,
                .shaderImageGatherExtended = VK_TRUE,
                .shaderStorageImageExtendedFormats = VK_TRUE,
                .shaderStorageImageMultisample = VK_TRUE,
                .shaderStorageImageReadWithoutFormat = VK_TRUE,
                .shaderStorageImageWriteWithoutFormat = VK_TRUE,
                .shaderUniformBufferArrayDynamicIndexing = VK_TRUE,
                .shaderSampledImageArrayDynamicIndexing = VK_TRUE,
                .shaderStorageBufferArrayDynamicIndexing = VK_TRUE,
                .shaderStorageImageArrayDynamicIndexing = VK_TRUE,
                .shaderClipDistance = VK_TRUE,
                .shaderCullDistance = VK_TRUE,
                .shaderFloat64 = VK_TRUE,
                .shaderInt64 = VK_TRUE,
                .shaderInt16 = VK_TRUE,
                .shaderResourceResidency = VK_TRUE,
                .shaderResourceMinLod = VK_TRUE,
            },
    };

    return &features2;
}

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
        properties_.memory = {.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_PROPERTIES_2};
        vkGetPhysicalDeviceMemoryProperties2(device, &properties_.memory);
    }
    {
        properties_.device = {.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2,
                              .pNext = &properties_.maintenance3};
        properties_.maintenance3 = {.sType =
                                        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MAINTENANCE_3_PROPERTIES,
                                    .pNext = &properties_.maintenance4};
        properties_.maintenance4 = {.sType =
                                        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MAINTENANCE_4_PROPERTIES,
                                    .pNext = &properties_.ray_tracing};
        properties_.ray_tracing = {
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR};

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
            return (flags & VK_QUEUE_TRANSFER_BIT) && !(flags & VK_QUEUE_GRAPHICS_BIT) &&
                   !(flags & VK_QUEUE_COMPUTE_BIT);
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

bool Physical_Device::Properties::is_discrete() const {
    return device.properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU;
}

String_View Physical_Device::Properties::name() const {
    return String_View{device.properties.deviceName};
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

    VkPhysicalDeviceMemoryBudgetPropertiesEXT budget = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_BUDGET_PROPERTIES_EXT,
    };

    VkPhysicalDeviceMemoryProperties2KHR mem_props = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_PROPERTIES_2_KHR,
        .pNext = &budget,
    };

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

VkSurfaceCapabilitiesKHR Physical_Device::capabilities(VkSurfaceKHR surface) {
    VkSurfaceCapabilitiesKHR capabilities;
    RVK_CHECK(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &capabilities));
    return capabilities;
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
        for(auto& p : available_extensions)
            if(p.extensionName[0]) Text("%s", p.extensionName);
        TreePop();
    }
}

Device::Device(Arc<Physical_Device, Alloc> P, VkSurfaceKHR surface, bool ray_tracing,
               bool robustness)
    : physical_device(move(P)) {

    Profile::Time_Point start = Profile::timestamp();

    info("[rvk] Creating device...");

    u32 n_graphics_queues = physical_device->queue_count(Queue_Family::graphics);
    u32 n_compute_queues = physical_device->queue_count(Queue_Family::compute);
    u32 n_transfer_queues = physical_device->queue_count(Queue_Family::transfer);
    if(n_graphics_queues + n_compute_queues + n_transfer_queues == 0) {
        die("[rvk] No queues found.");
    }

    { // Find queue families
        if(auto idx = physical_device->queue_index(Queue_Family::graphics); idx.ok()) {
            graphics_family_index = *idx;
        } else {
            die("[rvk] No graphics queue family found.");
        }

        if(auto idx = physical_device->present_queue_index(surface); idx.ok()) {
            present_family_index = *idx;
        } else {
            die("[rvk] No present queue family found.");
        }

        compute_family_index =
            physical_device->queue_index(Queue_Family::compute).value_or(graphics_family_index);
        transfer_family_index =
            physical_device->queue_index(Queue_Family::transfer).value_or(graphics_family_index);
    }

    Log_Indent {
        Region(R) {

            u32 max_queues =
                Math::max(n_graphics_queues, Math::max(n_compute_queues, n_transfer_queues));
            auto priorities = Vec<f32, Mregion<R>>::make(max_queues);
            priorities[0] = 1.0f;
            for(u64 i = 1; i < priorities.length(); i++) {
                priorities[i] = priorities[i - 1] * 0.9f;
            }

            // Create queue initialization info

            Vec<VkDeviceQueueCreateInfo, Mregion<R>> queue_infos(
                n_graphics_queues + n_compute_queues + n_transfer_queues + 1);
            {
                {
                    VkDeviceQueueCreateInfo qinfo = {
                        .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
                        .queueFamilyIndex = graphics_family_index,
                        .queueCount = n_graphics_queues,
                        .pQueuePriorities = priorities.data(),
                    };
                    queue_infos.push(qinfo);
                }

                for(f32& p : priorities) {
                    p *= 0.9f;
                }

                if(compute_family_index != graphics_family_index) {
                    VkDeviceQueueCreateInfo qinfo = {
                        .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
                        .queueFamilyIndex = compute_family_index,
                        .queueCount = n_compute_queues,
                        .pQueuePriorities = priorities.data(),
                    };
                    queue_infos.push(qinfo);
                } else {
                    info("[rvk] Using graphics queue family as compute queue family.");
                }
                if(transfer_family_index != graphics_family_index) {
                    VkDeviceQueueCreateInfo qinfo = {
                        .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
                        .queueFamilyIndex = transfer_family_index,
                        .queueCount = n_transfer_queues,
                        .pQueuePriorities = priorities.data(),
                    };
                    queue_infos.push(qinfo);
                } else {
                    info("[rvk] Using graphics queue family as transfer queue family.");
                }
                if(present_family_index != graphics_family_index) {
                    VkDeviceQueueCreateInfo qinfo = {
                        .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
                        .queueFamilyIndex = present_family_index,
                        .queueCount = 1,
                        .pQueuePriorities = priorities.data(),
                    };
                    queue_infos.push(qinfo);
                } else {
                    info("[rvk] Using graphics queue family as present queue family.");
                }
            }

            // Create extension info

            Vec<const char*, Mregion<R>> vk_extensions;
            {
                for(auto& ext : baseline_extensions()) {
                    vk_extensions.push(ext);
                }
                if(ray_tracing) {
                    for(auto& ext : ray_tracing_extensions()) {
                        vk_extensions.push(ext);
                    }
                }
            }

            // Create device

            {
                VkDeviceCreateInfo dev_info = {
                    .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
                    .pNext = baseline_features(ray_tracing, robustness),
                    .queueCreateInfoCount = static_cast<u32>(queue_infos.length()),
                    .pQueueCreateInfos = queue_infos.data(),
                    .enabledExtensionCount = static_cast<u32>(vk_extensions.length()),
                    .ppEnabledExtensionNames = vk_extensions.data(),
                };

                RVK_CHECK(vkCreateDevice(*physical_device, &dev_info, null, &device));
            }

            for(auto& ext : vk_extensions) {
                enabled_extensions.push(String_View{ext}.string<Alloc>());
            }
        }

        // Get queues

        {
            graphics_qs.resize(n_graphics_queues);
            compute_qs.resize(n_compute_queues);
            transfer_qs.resize(n_transfer_queues);

            VkDeviceQueueInfo2 info = {
                .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_INFO_2,
            };

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

        // Find heaps

        {
            if(auto idx =
                   physical_device->heap_index(RPP_UINT32_MAX, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
               idx.ok()) {
                device_memory_index = *idx;
            } else {
                die("[rvk] No device local heap found.");
            }

            if(auto idx = physical_device->heap_index(RPP_UINT32_MAX,
                                                      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                                          VK_MEMORY_PROPERTY_HOST_COHERENT_BIT |
                                                          VK_MEMORY_PROPERTY_HOST_CACHED_BIT);
               idx.ok()) {
                host_memory_index = *idx;
            } else {
                die("[rvk] No host visible heap found.");
            }

            info("[rvk] Found device and host heaps (%: %mb, %: %mb).", device_memory_index,
                 heap_size(Heap::device) / Math::MB(1), host_memory_index,
                 heap_size(Heap::host) / Math::MB(1));
        }

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

void Device::lock_queues() {
    mutex.lock();
}

void Device::unlock_queues() {
    mutex.unlock();
}

VkResult Device::present(const VkPresentInfoKHR& info) {
    Thread::Lock lock(mutex);
    return vkQueuePresentKHR(present_q, &info);
}

void Device::wait_idle() {
    Thread::Lock lock(mutex);
    RVK_CHECK(vkDeviceWaitIdle(device));
}

void Device::submit(Commands& cmds, u32 index) {

    VkCommandBufferSubmitInfo cmd_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO,
        .commandBuffer = cmds,
    };

    VkSubmitInfo2 submit_info = {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2,
        .commandBufferInfoCount = 1,
        .pCommandBufferInfos = &cmd_info,
    };

    {
        Thread::Lock lock(mutex);
        RVK_CHECK(vkQueueSubmit2(queue(cmds.family(), index), 1, &submit_info, null));
    }
}

void Device::submit(Commands& cmds, u32 index, Fence& fence) {

    VkCommandBufferSubmitInfo cmd_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO,
        .commandBuffer = cmds,
    };

    VkSubmitInfo2 submit_info = {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2,
        .commandBufferInfoCount = 1,
        .pCommandBufferInfos = &cmd_info,
    };

    fence.reset();
    {
        Thread::Lock lock(mutex);
        RVK_CHECK(vkQueueSubmit2(queue(cmds.family(), index), 1, &submit_info, fence));
    }
}

void Device::submit(Commands& cmds, u32 index, Slice<Sem_Ref> signal, Slice<Sem_Ref> wait) {

    Region(R) {

        Vec<VkSemaphoreSubmitInfo, Mregion<R>> vk_signal(signal.length());
        Vec<VkSemaphoreSubmitInfo, Mregion<R>> vk_wait(wait.length());

        for(u64 i = 0; i < signal.length(); i++) {
            VkSemaphoreSubmitInfo info = {
                .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
                .semaphore = *signal[i].sem,
                .stageMask = signal[i].stage,
            };
            vk_signal.push(info);
        }
        for(u64 i = 0; i < wait.length(); i++) {
            VkSemaphoreSubmitInfo info = {
                .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
                .semaphore = *wait[i].sem,
                .stageMask = wait[i].stage,
            };
            vk_wait.push(info);
        }

        VkCommandBufferSubmitInfo cmd_info = {
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO,
            .commandBuffer = cmds,
        };

        VkSubmitInfo2 submit_info = {
            .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2,
            .waitSemaphoreInfoCount = static_cast<u32>(vk_wait.length()),
            .pWaitSemaphoreInfos = vk_wait.data(),
            .commandBufferInfoCount = 1,
            .pCommandBufferInfos = &cmd_info,
            .signalSemaphoreInfoCount = static_cast<u32>(vk_signal.length()),
            .pSignalSemaphoreInfos = vk_signal.data(),
        };

        {
            Thread::Lock lock(mutex);
            RVK_CHECK(vkQueueSubmit2(queue(cmds.family(), index), 1, &submit_info, null));
        }
    }
}

void Device::submit(Commands& cmds, u32 index, Slice<Sem_Ref> signal, Slice<Sem_Ref> wait,
                    Fence& fence) {

    Region(R) {

        Vec<VkSemaphoreSubmitInfo, Mregion<R>> vk_signal(signal.length());
        Vec<VkSemaphoreSubmitInfo, Mregion<R>> vk_wait(wait.length());

        for(u64 i = 0; i < signal.length(); i++) {
            VkSemaphoreSubmitInfo info = {
                .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
                .semaphore = *signal[i].sem,
                .stageMask = signal[i].stage,
            };
            vk_signal.push(info);
        }
        for(u64 i = 0; i < wait.length(); i++) {
            VkSemaphoreSubmitInfo info = {
                .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
                .semaphore = *wait[i].sem,
                .stageMask = wait[i].stage,
            };
            vk_wait.push(info);
        }

        VkCommandBufferSubmitInfo cmd_info = {
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO,
            .commandBuffer = cmds,
        };

        VkSubmitInfo2 submit_info = {
            .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2,
            .waitSemaphoreInfoCount = static_cast<u32>(vk_wait.length()),
            .pWaitSemaphoreInfos = vk_wait.data(),
            .commandBufferInfoCount = 1,
            .pCommandBufferInfos = &cmd_info,
            .signalSemaphoreInfoCount = static_cast<u32>(vk_signal.length()),
            .pSignalSemaphoreInfos = vk_signal.data(),
        };

        fence.reset();
        {
            Thread::Lock lock(mutex);
            RVK_CHECK(vkQueueSubmit2(queue(cmds.family(), index), 1, &submit_info, fence));
        }
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
        for(auto& ext : enabled_extensions) Text("%.*s", ext.length(), ext.data());
        TreePop();
    }
}

Slice<const char*> Device::baseline_extensions() {
    static Array<const char*, 7> device{
        reinterpret_cast<const char*>(VK_KHR_SWAPCHAIN_EXTENSION_NAME),
        reinterpret_cast<const char*>(VK_EXT_MEMORY_BUDGET_EXTENSION_NAME),
        reinterpret_cast<const char*>(VK_EXT_PAGEABLE_DEVICE_LOCAL_MEMORY_EXTENSION_NAME),
        reinterpret_cast<const char*>(VK_EXT_MEMORY_PRIORITY_EXTENSION_NAME),
        reinterpret_cast<const char*>(VK_EXT_ROBUSTNESS_2_EXTENSION_NAME),
        reinterpret_cast<const char*>(VK_KHR_SHADER_CLOCK_EXTENSION_NAME),
#ifdef RPP_OS_WINDOWS
        reinterpret_cast<const char*>(VK_KHR_EXTERNAL_FENCE_WIN32_EXTENSION_NAME),
#else
        reinterpret_cast<const char*>(VK_KHR_EXTERNAL_FENCE_FD_EXTENSION_NAME),
#endif
    };
    return Slice<const char*>{device};
}

Slice<const char*> Device::ray_tracing_extensions() {
    static Array<const char*, 6> device{
        reinterpret_cast<const char*>(VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME),
        reinterpret_cast<const char*>(VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME),
        reinterpret_cast<const char*>(VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME),
        reinterpret_cast<const char*>(VK_KHR_RAY_TRACING_MAINTENANCE_1_EXTENSION_NAME),
        reinterpret_cast<const char*>(VK_KHR_RAY_TRACING_POSITION_FETCH_EXTENSION_NAME),
        reinterpret_cast<const char*>(VK_KHR_RAY_QUERY_EXTENSION_NAME),
    };
    return Slice<const char*>{device};
}

} // namespace rvk::impl