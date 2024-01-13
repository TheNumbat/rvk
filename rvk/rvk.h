
#pragma once

#include <rpp/base.h>

#include "fwd.h"

namespace rvk {

using namespace rpp;

struct Config {
    u32 frames_in_flight = 2;
    bool use_validation = true;
    Slice<String_View> instance_extensions;
};

[[nodiscard]] bool startup(Config config) noexcept;
void shutdown() noexcept;

} // namespace rvk
