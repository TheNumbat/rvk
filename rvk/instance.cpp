
#include <imgui/imgui.h>

#include "device.h"
#include "instance.h"

#ifdef NV_AFTERMATH
#include <aftermath/GFSDK_Aftermath_GpuCrashDump.h>
#include <rpp/files.h>
#endif

namespace rvk::impl {

#ifdef NV_AFTERMATH
namespace aftermath {

#define AFTERMATH_CHECK(f)                                                                         \
    do {                                                                                           \
        GFSDK_Aftermath_Result _af_res = (f);                                                      \
        if(!GFSDK_Aftermath_SUCCEED(_af_res)) {                                                    \
            RPP_DEBUG_BREAK;                                                                       \
            die("AFTERMATH_CHECK: %", aftermath::describe(_af_res));                               \
        }                                                                                          \
    } while(0)

String_View describe(GFSDK_Aftermath_Result result) {
    switch(result) {
    case GFSDK_Aftermath_Result_FAIL_DriverVersionNotSupported:
        return "Unsupported driver version - requires an NVIDIA R495 display driver or newer."_v;
    default: return "Unknown aftermath error."_v;
    }
}

void crash_dump_callback(const void* dump, u32 dump_size, void*) {
    warn("[aftermath] Crash dump received, size %.", dump_size);
    Region(R) {
        static u64 count = 0;
        auto filename = format<Mregion<R>>("gpu-dump-%.nv-gpudmp"_v, count++);
        static_cast<void>(Files::write(
            filename.view(), Slice<u8>{reinterpret_cast<u8*>(const_cast<void*>(dump)), dump_size}));
    }
}

void shader_debug_info_callback(const void* dump, u32 debug_info_size, void*) {
    info("[aftermath] Shader debug info received, size %.", debug_info_size);
    Region(R) {
        static u64 count = 0;
        auto filename = format<Mregion<R>>("gpu-dump-%.nvdbg"_v, count++);
        static_cast<void>(
            Files::write(filename.view(), Slice<u8>{reinterpret_cast<u8*>(const_cast<void*>(dump)),
                                                    debug_info_size}));
    }
}

void crash_dump_desc_callback(PFN_GFSDK_Aftermath_AddGpuCrashDumpDescription, void*) {
    info("[aftermath] Crash dump description requested (ignored).");
}

void resolve_marker_callback(const void*, const uint32_t, void*, void**, uint32_t*) {
    info("[aftermath] Resolve marker requested (ignored).");
}
} // namespace aftermath
#endif

using namespace rpp;

static VkBool32 debug_callback(VkDebugUtilsMessageSeverityFlagBitsEXT sev,
                               VkDebugUtilsMessageTypeFlagsEXT type,
                               const VkDebugUtilsMessengerCallbackDataEXT* data, void*) {

    String_View message{data->pMessage};

    if(data->messageIdNumber == -1277938581 || data->messageIdNumber == -602362517) {
        // UNASSIGNED-BestPractices-vkBindMemory-small-dedicated-allocation
        // UNASSIGNED-BestPractices-vkAllocateMemory-small-allocation
        // These are only triggered by the ImGui Vulkan backend.
        return false;
    }

    bool is_error = (sev & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) &&
                    (type & VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT);

    if(is_error) {
        warn("[rvk] % (%)", message, data->messageIdNumber);
    } else {
        info("[rvk] % (%)", message, data->messageIdNumber);
    }

    for(u32 i = 0; i < data->queueLabelCount; i++) {
        if(is_error)
            warn("\tduring %", String_View{data->pQueueLabels[i].pLabelName});
        else
            info("\tduring %", String_View{data->pQueueLabels[i].pLabelName});
    }
    for(u32 i = 0; i < data->cmdBufLabelCount; i++) {
        if(is_error)
            warn("\tinside %", String_View{data->pCmdBufLabels[i].pLabelName});
        else
            info("\tinside %", String_View{data->pCmdBufLabels[i].pLabelName});
    }

    for(u32 i = 0; i < data->objectCount; i++) {
        const VkDebugUtilsObjectNameInfoEXT* obj = &data->pObjects[i];
        if(is_error)
            warn("\tusing %: % (%)", describe(obj->objectType),
                 obj->pObjectName ? String_View{obj->pObjectName} : "?"_v, obj->objectHandle);
        else
            info("\tusing %: % (%)", describe(obj->objectType),
                 obj->pObjectName ? String_View{obj->pObjectName} : "?"_v, obj->objectHandle);
    }

#ifdef RPP_DEBUG_BUILD
    if(is_error) RPP_DEBUG_BREAK;
#endif

    return is_error;
}

Slice<const char*> Instance::baseline_extensions() {
    static Array<const char*, 1> instance{
        reinterpret_cast<const char*>(VK_EXT_DEBUG_UTILS_EXTENSION_NAME),
    };
    return Slice<const char*>{instance};
}

Instance::Instance(Slice<String_View> extensions, Slice<String_View> layers,
                   Function<VkSurfaceKHR(VkInstance)> create_surface, bool validation) {

    Profile::Time_Point start = Profile::timestamp();

    info("[rvk] Creating instance...");

    RVK_CHECK(volkInitialize());

    Log_Indent Region(R) {

        VkApplicationInfo app_info = {
            .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
            .pApplicationName = "rvk",
            .applicationVersion = VK_MAKE_VERSION(0, 0, 1),
            .pEngineName = "rvk",
            .engineVersion = VK_MAKE_VERSION(0, 0, 1),
            .apiVersion = VK_API_VERSION_1_3,
        };

        Vec<const char*, Mregion<R>> extension_names;
        for(auto& ext : extensions) {
            extension_names.push(reinterpret_cast<const char*>(ext.terminate<Mregion<R>>().data()));
        }
        for(auto& required : baseline_extensions()) {
            extension_names.push(required);
        }

        check_extensions(extension_names.slice());

        Vec<const char*, Mregion<R>> layer_names;
        for(auto& layer : layers) {
            layer_names.push(reinterpret_cast<const char*>(layer.terminate<Mregion<R>>().data()));
        }

        if(validation) {
            layer_names.push("VK_LAYER_KHRONOS_validation");
            layer_names.push("VK_LAYER_KHRONOS_synchronization2");
        }

        VkInstanceCreateInfo create_info = {
            .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
            .pApplicationInfo = &app_info,
            .enabledLayerCount = static_cast<u32>(layer_names.length()),
            .ppEnabledLayerNames = layer_names.data(),
            .enabledExtensionCount = static_cast<u32>(extension_names.length()),
            .ppEnabledExtensionNames = extension_names.data(),
        };

        VkResult result = vkCreateInstance(&create_info, null, &instance);

        if(result == VK_ERROR_INCOMPATIBLE_DRIVER) {
            die("[rvk] Error creating instance: incompatible driver!");
        } else if(result == VK_ERROR_LAYER_NOT_PRESENT) {
            warn("[rvk] Error creating instance: could not find layer, retrying without "
                 "validation...");

            create_info.enabledLayerCount -= 2;
            result = vkCreateInstance(&create_info, null, &instance);

            if(result != VK_SUCCESS) {
                die("[rvk] Error creating instance: %!", describe(result));
            } else {
                warn("[rvk] Created instance without validation.");
            }
        } else if(result != VK_SUCCESS) {
            die("[rvk] Error creating instance: %!", describe(result));
        }

        for(auto& ext : extensions) {
            enabled_extensions.push(ext.string<Alloc>());
        }
        for(auto& layer : layer_names) {
            enabled_layers.push(String_View{layer}.string<Alloc>());
        }

        volkLoadInstance(instance);
        info("[rvk] Loaded instance functions.");
        for(auto& layer : enabled_layers) info("[rvk] Enabled layer %.", layer);

        if(!vkGetPhysicalDeviceSurfaceFormatsKHR) {
            die("[rvk] did not load a platform-specific surface extension!");
        }

#ifdef NV_AFTERMATH
        AFTERMATH_CHECK(GFSDK_Aftermath_EnableGpuCrashDumps(
            GFSDK_Aftermath_Version_API, GFSDK_Aftermath_GpuCrashDumpWatchedApiFlags_Vulkan,
            GFSDK_Aftermath_GpuCrashDumpFeatureFlags_DeferDebugInfoCallbacks,
            aftermath::crash_dump_callback, aftermath::shader_debug_info_callback,
            aftermath::crash_dump_desc_callback, aftermath::resolve_marker_callback, null));
        info("[aftermath] Loaded crash dumper.");
#endif

        Profile::Time_Point end = Profile::timestamp();
        info("[rvk] Created instance in %ms.", Profile::ms(end - start));
    }

    {
        surface_ = create_surface(instance);
        info("[rvk] Created surface.");
    }
}

Instance::~Instance() {
    if(surface_) {
        vkDestroySurfaceKHR(instance, surface_, null);
        info("[rvk] Destroyed surface.");
    }
    if(instance) {
#ifdef NV_AFTERMATH
        GFSDK_Aftermath_DisableGpuCrashDumps();
#endif

        vkDestroyInstance(instance, null);
        info("[rvk] Destroyed instance.");

        volkFinalize();
        info("[rvk] Unloaded instance functions.");
    }
    instance = null;
}

void Instance::check_extensions(Slice<const char*> extensions) {

    {
        u32 total_extensions = 0;
        RVK_CHECK(vkEnumerateInstanceExtensionProperties(null, &total_extensions, null));
        available_extensions.resize(total_extensions);
        RVK_CHECK(vkEnumerateInstanceExtensionProperties(null, &total_extensions,
                                                         available_extensions.data()));
    }

    info("[rvk] Checking extensions...");
    Log_Indent for(auto required : extensions) {
        String_View required_name{required};

        bool found = false;
        for(auto extension : available_extensions) {
            String_View extension_name{extension.extensionName};
            if(required_name == extension_name) {
                found = true;
                break;
            }
        }
        if(found)
            info("[rvk] Found %.", required_name);
        else
            die("[rvk] Did not find %.", required_name);
    }
}

void Instance::imgui() {
    using namespace ImGui;

    u32 vk_version;
    RVK_CHECK(vkEnumerateInstanceVersion(&vk_version));

    Text("Instance Version: %u", vk_version);
    if(TreeNode("Available Extensions")) {
        for(auto& ext : available_extensions)
            if(ext.extensionName[0]) Text("%s", ext.extensionName);
        TreePop();
    }
    if(TreeNode("Enabled Extensions")) {
        for(auto& ext : enabled_extensions) Text("%.*s", ext.length(), ext.data());
        TreePop();
    }
    if(TreeNode("Enabled Layers")) {
        for(auto& layer : enabled_layers) Text("%.*s", layer.length(), layer.data());
        TreePop();
    }
}

Arc<Physical_Device, Alloc> Instance::physical_device(VkSurfaceKHR surface, bool ray_tracing) {
    Region(R) {

        u32 n_devices = 0;
        RVK_CHECK(vkEnumeratePhysicalDevices(instance, &n_devices, null));
        if(n_devices == 0) {
            die("[rvk] Found no GPUs!");
        } else {
            info("[rvk] Found % GPU(s)...", n_devices);
        }

        auto vk_physical_devices = Vec<VkPhysicalDevice, Mregion<R>>::make(n_devices);
        RVK_CHECK(vkEnumeratePhysicalDevices(instance, &n_devices, vk_physical_devices.data()));

        Vec<Arc<Physical_Device, Alloc>, Mregion<R>> physical_devices;
        for(u32 i = 0; i < n_devices; i++) {
            physical_devices.push(Arc<Physical_Device, Alloc>::make(vk_physical_devices[i]));
        }

        Vec<Arc<Physical_Device, Alloc>, Mregion<R>> compatible_devices;

        for(auto& device : physical_devices) {
            info("[rvk] Checking device: %", device->properties().name());
            Log_Indent {
                auto surface_formats = device->template surface_formats<R>(surface);
                if(surface_formats.empty()) {
                    info("[rvk] Device has no compatible surface formats!");
                    continue;
                }

                auto present_modes = device->template present_modes<R>(surface);
                if(present_modes.empty()) {
                    info("[rvk] Device has no compatible present modes!");
                    continue;
                }

                auto graphics_queue = device->queue_index(Queue_Family::graphics);
                if(!graphics_queue.ok()) {
                    info("[rvk] Device has no graphics queue family!");
                    continue;
                }

                auto present_queue = device->present_queue_index(surface);
                if(!present_queue.ok()) {
                    info("[rvk] Device has no compatible present queue family!");
                    continue;
                }

                bool supported = true;

                for(auto& extension : Device::baseline_extensions()) {
                    if(device->supports_extension(String_View{extension})) {
                        info("[rvk] Found extension: %", String_View{extension});
                    } else {
                        info("[rvk] Device does not support extension %", extension);
                        supported = false;
                        break;
                    }
                }
                if(ray_tracing) {
                    for(auto& extension : Device::ray_tracing_extensions()) {
                        if(device->supports_extension(String_View{extension})) {
                            info("[rvk] Found ray tracing extension: %", String_View{extension});
                        } else {
                            info("[rvk] Device does not support ray tracing extension %",
                                 extension);
                            supported = false;
                            break;
                        }
                    }
                }

                if(!supported) continue;

                info("[rvk] Device is supported.");
                compatible_devices.push(move(device));
            }
        }

        if(compatible_devices.empty()) {
            die("[rvk] Found no compatible devices!");
        } else {
            info("[rvk] Found % compatible device(s).", compatible_devices.length());
        }

        Opt<u32> discrete_idx;
        for(u32 i = 0; i < compatible_devices.length(); i++) {
            if(compatible_devices[i]->properties().is_discrete()) {
                discrete_idx = Opt<u32>{i};
                break;
            }
        }

        if(!discrete_idx.ok()) {
            info("[rvk] No discrete GPU found, selecting first compatible device.");
        } else {
            info("[rvk] Found discrete GPU: %.",
                 compatible_devices[*discrete_idx]->properties().name());
        }

        u32 idx = discrete_idx.ok() ? *discrete_idx : 0;
        info("[rvk] Selected device: %.", compatible_devices[idx]->properties().name());

        return move(compatible_devices.front());
    }
}

Debug_Callback::Debug_Callback(Arc<Instance, Alloc> I) : instance(move(I)) {

    Profile::Time_Point start = Profile::timestamp();

    VkDebugUtilsMessengerCreateInfoEXT callback = {
        .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
        .messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
                           VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT |
                           VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                           VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
        .messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                       VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                       VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,
        .pfnUserCallback = &debug_callback,
    };

    RVK_CHECK(vkCreateDebugUtilsMessengerEXT(*instance, &callback, null, &messenger));

    Profile::Time_Point end = Profile::timestamp();
    info("[rvk] Created debug messenger in %ms.", Profile::ms(end - start));
}

Debug_Callback::~Debug_Callback() {
    if(messenger) {
        vkDestroyDebugUtilsMessengerEXT(*instance, messenger, null);
        info("[rvk] Destroyed debug messenger.");
    }
    instance = {};
    messenger = null;
}

} // namespace rvk::impl