
#pragma once

#include <rpp/base.h>

#include "fwd.h"

namespace rvk {

using namespace rpp;

struct Config {
    bool validation = true;
    Slice<String_View> layers;
    Slice<String_View> instance_extensions;

    Slice<String_View> device_extensions;
    VkPhysicalDeviceFeatures2 device_features = {};
    Function<VkSurfaceKHR(VkInstance)> create_surface;
};

bool startup(Config config);
void shutdown();

} // namespace rvk
