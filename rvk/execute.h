
#pragma once

#include <rpp/base.h>
#include <rpp/pool.h>

#include "fwd.h"

#include "commands.h"
#include "device.h"

namespace rvk {

using namespace rpp;

namespace impl {

void submit(Commands& cmds, u32 index, Fence& fence);
void submit_and_wait(Commands cmds, u32 index);

Fence make_fence();
Commands make_commands(Queue_Family family = Queue_Family::graphics);

} // namespace impl

using impl::Commands;

template<typename F>
    requires Invocable<F, Commands&>
auto sync(F&& f, Queue_Family family = Queue_Family::graphics, u32 index = 0)
    -> Invoke_Result<F, Commands&> {
    Commands cmds = impl::make_commands(family);
    if constexpr(Same<Invoke_Result<F, Commands&>, void>) {
        f(cmds);
        impl::submit_and_wait(move(cmds), index);
    } else {
        auto result = f(cmds);
        impl::submit_and_wait(move(cmds), index);
        return result;
    }
}

template<typename F>
    requires Invocable<F, Commands&>
auto async(Async::Pool<>& pool, F&& f, Queue_Family family = Queue_Family::graphics, u32 index = 0)
    -> Async::Task<Invoke_Result<F, Commands&>> {
    auto fence = impl::make_fence();
    Commands cmds = impl::make_commands(family);
    if constexpr(Same<Invoke_Result<F, Commands&>, void>) {
        forward<F>(f)(cmds);
        impl::submit(cmds, index, fence);
        co_await pool.event(fence.event());
        fence.wait();
    } else {
        auto ret = forward<F>(f)(cmds);
        impl::submit(cmds, index, fence);
        co_await pool.event(fence.event());
        fence.wait();
        co_return ret;
    }
}

} // namespace rvk
