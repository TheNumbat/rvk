
#pragma once

#include <rpp/base.h>
#include <rpp/rc.h>

#include "fwd.h"

namespace rvk::impl {

using namespace rpp;

struct Debug_Callback {

    explicit Debug_Callback(Arc<Instance, Alloc> instance);
    ~Debug_Callback();

    Debug_Callback(const Debug_Callback&) = delete;
    Debug_Callback& operator=(const Debug_Callback&) = delete;
    Debug_Callback(Debug_Callback&&) = delete;
    Debug_Callback& operator=(Debug_Callback&&) = delete;

private:
    Arc<Instance, Alloc> instance;
    VkDebugUtilsMessengerEXT messenger = null;
};

struct Instance {

    explicit Instance(Slice<String_View> extensions, Slice<String_View> layers, bool validation);
    ~Instance();

    Instance(const Instance&) = delete;
    Instance& operator=(const Instance&) = delete;
    Instance(Instance&&) = delete;
    Instance& operator=(Instance&&) = delete;

    void imgui();

    Arc<Physical_Device, Alloc> physical_device(Slice<String_View> extensions,
                                                VkSurfaceKHR surface);

    operator VkInstance() const {
        return instance;
    }

private:
    void check_extensions(Slice<const char*> extensions);

    VkInstance instance = null;
    Vec<String<Alloc>, Alloc> enabled_layers;
    Vec<String<Alloc>, Alloc> enabled_extensions;
    Vec<VkExtensionProperties, Alloc> available_extensions;
};

} // namespace rvk::impl
