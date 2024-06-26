
#pragma once

#include <rpp/base.h>
#include <rpp/pool.h>

#include "rvk.h"

namespace rvk {

namespace impl {
Arc<Device, Alloc> get_device();
bool validation_enabled();
} // namespace impl

using namespace rpp;

template<Type_List L>
    requires(Reflect::All<Is_Binding, L>)
Descriptor_Set_Layout make_layout(Slice<const u32> counts) {
    return impl::Binder::template make<L>(impl::get_device(), counts);
}

template<Type_List L, Binding... Binds>
    requires(Same<L, List<Binds...>>)
void write_set(Descriptor_Set& set, Binds&... binds) {
    impl::Binder::template write<L, Binds...>(set, frame(), binds...);
}

template<Type_List L, Binding... Binds>
    requires(Same<L, List<Binds...>>)
void write_set(Descriptor_Set& set, u32 frame_index, Binds&... binds) {
    impl::Binder::template write<L, Binds...>(set, frame_index, binds...);
}

template<typename F>
    requires Invocable<F, Commands&>
auto sync(F&& f, Queue_Family family, u32 index) -> Invoke_Result<F, Commands&> {
    auto fence = make_fence();
    auto cmds = make_commands(family);
    if constexpr(Same<Invoke_Result<F, Commands&>, void>) {
        f(cmds);
        cmds.end();
        submit(cmds, index, fence);
        fence.wait();
    } else {
        auto result = f(cmds);
        cmds.end();
        submit(cmds, index, fence);
        fence.wait();
        return result;
    }
}

template<typename F>
    requires Invocable<F, Commands&>
auto async(Async::Pool<>& pool, F&& f, Queue_Family family, u32 index)
    -> Async::Task<Invoke_Result<F, Commands&>> {
    auto fence = make_fence();
    auto cmds = make_commands(family);
    if constexpr(Same<Invoke_Result<F, Commands&>, void>) {
        forward<F>(f)(cmds);
        cmds.end();
        submit(cmds, index, fence);
        if(impl::validation_enabled()) {
            fence.wait();
        } else {
            co_await pool.event(fence.event());
        }
        co_return;
    } else {
        auto ret = forward<F>(f)(cmds);
        cmds.end();
        submit(cmds, index, fence);
        if(impl::validation_enabled()) {
            fence.wait();
        } else {
            co_await pool.event(fence.event());
        }
        co_return ret;
    }
}

} // namespace rvk
