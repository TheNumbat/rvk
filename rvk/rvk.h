
#pragma once

#include <rpp/base.h>

#include "fwd.h"

namespace rvk {

using namespace rpp;

struct Config {
    Slice<String_View> layers;
    Slice<String_View> instance_extensions;
    bool validation = true;

    u32 frames_in_flight = 2;
};

[[nodiscard]] bool startup(Config config) noexcept;
void shutdown() noexcept;

} // namespace rvk
