
#pragma once

#include <rpp/base.h>

#include "fwd.h"

#include "commands.h"
#include "memory.h"

namespace rvk {

using namespace rpp;

/****** API ****************************************************************/

using impl::Commands;
using impl::Image;
using impl::Image_View;

struct Config {
    bool validation = true;
    bool ray_tracing = false;
    bool imgui = false;
    u32 frames_in_flight = 2;
    u32 descriptors_per_type = 128;

    Slice<String_View> layers;
    Slice<String_View> swapchain_extensions;
    Function<VkSurfaceKHR(VkInstance)> create_surface;

    u64 host_heap = Math::GB(1);
    u64 device_heap_margin = Math::GB(1);
};

bool startup(Config config);
void shutdown();
void imgui();

void begin_frame();
void end_frame(Image_View& output);

Opt<Image> make_image(VkExtent3D extent, VkFormat format, VkImageUsageFlags usage);

template<typename F>
    requires Invocable<F, Commands&>
auto sync(F&& f, Queue_Family family = Queue_Family::graphics, u32 index = 0)
    -> Invoke_Result<F, Commands&>;

/****** Implementation ******************************************************/

namespace impl {

Commands make_commands(Queue_Family family = Queue_Family::graphics);
void submit_and_wait(Commands cmds, u32 index = 0);

} // namespace impl

template<typename F>
    requires Invocable<F, Commands&>
auto sync(F&& f, Queue_Family family, u32 index) -> Invoke_Result<F, Commands&> {
    Commands cmds = impl::make_commands(family);
    if constexpr(Same<Invoke_Result<F, Commands&>, void>) {
        f(cmds);
        cmds.end();
        impl::submit_and_wait(move(cmds), index);
    } else {
        auto result = f(cmds);
        cmds.end();
        impl::submit_and_wait(move(cmds), index);
        return result;
    }
}

} // namespace rvk
