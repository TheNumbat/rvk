
#pragma once

#include <rpp/base.h>
#include <rpp/pool.h>

#include "fwd.h"

#include "commands.h"
#include "drop.h"
#include "execute.h"
#include "memory.h"

namespace rvk {

using namespace rpp;

using impl::Commands;
using impl::Image;
using impl::Image_View;

using Finalizer = FunctionN<8, void()>;

// Setup

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

// Frame lifecycle

void imgui();
void begin_frame();
void end_frame(Image_View& output);

// Resources

void drop(Finalizer f);
Opt<Image> make_image(VkExtent3D extent, VkFormat format, VkImageUsageFlags usage);

// Command execution

template<typename F>
    requires Invocable<F, Commands&>
auto sync(F&& f, Queue_Family family, u32 index) -> Invoke_Result<F, Commands&>;

template<typename F>
    requires Invocable<F, Commands&>
auto async(Async::Pool<>& pool, F&& f, Queue_Family family, u32 index)
    -> Async::Task<Invoke_Result<F, Commands&>>;

} // namespace rvk
