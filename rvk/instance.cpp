
#include <imgui/imgui.h>

#include "instance.h"

namespace rvk::impl {

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
        warn("[VK] % (%)", message, data->messageIdNumber);
    } else {
        info("[VK] % (%)", message, data->messageIdNumber);
    }

    for(u32 i = 0; i < data->queueLabelCount; i++) {
        if(is_error)
            warn("\tduring %", data->pQueueLabels[i].pLabelName);
        else
            info("\tduring %", data->pQueueLabels[i].pLabelName);
    }
    for(u32 i = 0; i < data->cmdBufLabelCount; i++) {
        if(is_error)
            warn("\tinside %", data->pCmdBufLabels[i].pLabelName);
        else
            info("\tinside %", data->pCmdBufLabels[i].pLabelName);
    }

    for(u32 i = 0; i < data->objectCount; i++) {
        const VkDebugUtilsObjectNameInfoEXT* obj = &data->pObjects[i];
        if(is_error)
            warn("\tusing %: % (%)", describe(obj->objectType),
                 obj->pObjectName ? obj->pObjectName : "?", obj->objectHandle);
        else
            info("\tusing %: % (%)", describe(obj->objectType),
                 obj->pObjectName ? obj->pObjectName : "?", obj->objectHandle);
    }

    return is_error;
}

Instance::Instance(Slice<String_View> extensions, Slice<String_View> layers, bool validation) {

    Profile::Time_Point start = Profile::timestamp();

    Region(R) {
        info("[rvk] creating instance...");

        RVK_CHECK(volkInitialize());

        VkApplicationInfo app_info = {};
        app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        app_info.pApplicationName = "rvk";
        app_info.applicationVersion = VK_MAKE_VERSION(0, 0, 1);
        app_info.pEngineName = "rvk";
        app_info.engineVersion = VK_MAKE_VERSION(0, 0, 1);
        app_info.apiVersion = VK_API_VERSION_1_3;

        Vec<const char*, Mregion<R>> extension_names;
        for(auto& ext : extensions) {
            extension_names.push(reinterpret_cast<const char*>(ext.terminate<Mregion<R>>().data()));
        }

        extension_names.push(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);

        check_extensions(extension_names.slice());

        Vec<const char*, Mregion<R>> layer_names;
        for(auto& layer : layers) {
            layer_names.push(reinterpret_cast<const char*>(layer.terminate<Mregion<R>>().data()));
        }

        if(validation) {
            layer_names.push("VK_LAYER_KHRONOS_validation");
            layer_names.push("VK_LAYER_KHRONOS_synchronization2");
        }

        VkInstanceCreateInfo create_info = {};
        create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        create_info.pApplicationInfo = &app_info;
        create_info.enabledExtensionCount = static_cast<u32>(extension_names.length());
        create_info.ppEnabledExtensionNames = extension_names.data();
        create_info.enabledLayerCount = static_cast<u32>(layer_names.length());
        create_info.ppEnabledLayerNames = layer_names.data();

        VkResult result = vkCreateInstance(&create_info, null, &instance);

        if(result == VK_ERROR_INCOMPATIBLE_DRIVER) {
            die("[rvk] error creating instance: incompatible driver.");
        } else if(result == VK_ERROR_LAYER_NOT_PRESENT) {
            warn("[rvk] error creating instance: could not find layer, retrying without "
                 "validation...");

            create_info.enabledLayerCount -= 2;
            result = vkCreateInstance(&create_info, null, &instance);

            if(result != VK_SUCCESS) {
                die("[rvk] error creating instance: %", describe(result));
            } else {
                warn("[rvk] created instance without validation.");
            }
        } else if(result != VK_SUCCESS) {
            die("[rvk] error creating instance: %", describe(result));
        }

        for(auto& ext : extensions) {
            enabled_extensions.push(ext.string<Alloc>());
        }
        for(auto& layer : layer_names) {
            enabled_layers.push(String_View{layer}.string<Alloc>());
        }
    }

    Profile::Time_Point end = Profile::timestamp();
    info("[rvk] created instance in %ms.", Profile::ms(end - start));
    Log_Indent {
        for(auto& layer : enabled_layers) info("[rvk] enabled layer: %", layer);
    }

    volkLoadInstance(instance);
    info("[rvk] loaded instance functions.");
}

Instance::~Instance() {
    if(instance) {
        vkDestroyInstance(instance, null);
        info("[rvk] destroyed instance.");
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

    info("[rvk] checking required instance extensions...");
    for(auto required : extensions) Log_Indent {
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
                info("[rvk] found %", required_name);
            else
                die("[rvk] missing %", required_name);
        }
}

void Instance::imgui() {
    using namespace ImGui;

    u32 vk_version;
    RVK_CHECK(vkEnumerateInstanceVersion(&vk_version));

    Text("Instance Version: %u", vk_version);
    if(TreeNode("Instance Extensions")) {
        for(auto& ext : available_extensions) Text("%s", ext.extensionName);
        TreePop();
    }
    if(TreeNode("Enabled Instance Extensions")) {
        for(auto& ext : enabled_extensions) Text("%*.s", ext.length(), ext.data());
        TreePop();
    }
    if(TreeNode("Enabled Layers")) {
        for(auto& layer : enabled_layers) Text("%*.s", layer.length(), layer.data());
        TreePop();
    }
}

Debug_Callback::Debug_Callback(Rc<Instance, Alloc> I) : instance(move(I)) {

    Profile::Time_Point start = Profile::timestamp();

    VkDebugUtilsMessengerCreateInfoEXT callback = {};
    callback.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    callback.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
                               VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT |
                               VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                               VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    callback.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                           VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                           VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    callback.pfnUserCallback = &debug_callback;

    RVK_CHECK(vkCreateDebugUtilsMessengerEXT(*instance, &callback, null, &messenger));

    Profile::Time_Point end = Profile::timestamp();
    info("[rvk] created debug utils messenger in %ms.", Profile::ms(end - start));
}

Debug_Callback::~Debug_Callback() {
    if(messenger) {
        vkDestroyDebugUtilsMessengerEXT(*instance, messenger, null);
        info("[rvk] destroyed debug utils messenger.");
    }
    instance = {};
    messenger = null;
}

} // namespace rvk::impl