
#pragma once

#include "fwd.h"

#include "commands.h"
#include "device.h"

namespace rvk {

using namespace rpp;

namespace impl {

Commands make_commands(Queue_Family family = Queue_Family::graphics);
void submit_and_wait(Commands cmds, u32 index = 0);

} // namespace impl

using impl::Commands;

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
