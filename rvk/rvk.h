
#pragma once

#include <rpp/base.h>

#include "fwd.h"

namespace rvk {

using namespace rpp;

struct Config {
    Slice<String_View> layers;
    Slice<String_View> instance_extensions;
    bool validation = true;

    Slice<String_View> device_extensions;
    VkSurfaceKHR surface = null;
};

bool startup(Config config);
void shutdown();

} // namespace rvk
