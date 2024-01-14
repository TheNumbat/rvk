
#pragma once

#include <rpp/base.h>
#include <rpp/rc.h>

#include "fwd.h"

namespace rvk::impl {

using namespace rpp;

struct Debug_Callback {

    ~Debug_Callback();

    Debug_Callback(const Debug_Callback&) = delete;
    Debug_Callback& operator=(const Debug_Callback&) = delete;
    Debug_Callback(Debug_Callback&&) = delete;
    Debug_Callback& operator=(Debug_Callback&&) = delete;

private:
    explicit Debug_Callback(Arc<Instance, Alloc> instance);
    friend struct Arc<Debug_Callback, Alloc>;

    Arc<Instance, Alloc> instance;
    VkDebugUtilsMessengerEXT messenger = null;
};

struct Instance {

    ~Instance();

    Instance(const Instance&) = delete;
    Instance& operator=(const Instance&) = delete;
    Instance(Instance&&) = delete;
    Instance& operator=(Instance&&) = delete;

    void imgui();

    Arc<Physical_Device, Alloc> physical_device(VkSurfaceKHR surface, bool ray_tracing);

    operator VkInstance() {
        return instance;
    }
    VkSurfaceKHR surface() {
        return surface_;
    }

    static Slice<const char*> baseline_extensions();

private:
    explicit Instance(Slice<String_View> extensions, Slice<String_View> layers,
                      Function<VkSurfaceKHR(VkInstance)> create_surface, bool validation);
    friend struct Arc<Instance, Alloc>;

    void check_extensions(Slice<const char*> extensions);

    VkInstance instance = null;
    VkSurfaceKHR surface_ = null;

    Vec<String<Alloc>, Alloc> enabled_layers;
    Vec<String<Alloc>, Alloc> enabled_extensions;
    Vec<VkExtensionProperties, Alloc> available_extensions;
};

} // namespace rvk::impl
