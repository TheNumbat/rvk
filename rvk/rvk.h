
#pragma once

#include <rpp/base.h>

#include "fwd.h"

namespace rvk {

using namespace rpp;

struct Config {
    bool validation = true;
    bool ray_tracing = false;

    Slice<String_View> layers;
    Slice<String_View> swapchain_extensions;
    Function<VkSurfaceKHR(VkInstance)> create_surface;

    u64 device_heap_margin = Math::GB(1);
    u64 staging_heap = Math::GB(1);
};

bool startup(Config config);
void shutdown();

} // namespace rvk
