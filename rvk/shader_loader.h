
#pragma once

#include <rpp/base.h>
#include <rpp/files.h>
#include <rpp/pool.h>

#include "fwd.h"

#include "device.h"
#include "pipeline.h"

namespace rvk {

struct Shader_Loader {
    using Token = u64;
    using Shader = impl::Shader;

    Shader_Loader() = default;
    ~Shader_Loader() = default;

    Shader_Loader(const Shader_Loader&) = delete;
    Shader_Loader& operator=(const Shader_Loader&) = delete;

    Shader_Loader(Shader_Loader&&) = delete;
    Shader_Loader& operator=(Shader_Loader&&) = delete;

    Shader& get(Token token);

    Token compile(String_View path);
    Async::Task<Token> compile_async(Async::Pool<>& pool, String_View path);

    void try_reload();
    void on_reload(Slice<Token> shaders, FunctionN<16, void(Shader_Loader&)> callback);
    void trigger(Token token);

private:
    using Device = impl::Device;
    using Reload_Token = u64;

    explicit Shader_Loader(Arc<Device, Alloc> device) : device(move(device)) {
    }
    friend struct Box<Shader_Loader, Alloc>;

    Arc<Device, Alloc> device;
    Thread::Atomic next_token{1};
    Reload_Token next_reload_token = 1;

    Thread::Mutex mutex;
    Map<Token, Pair<Shader, Files::Write_Watcher>, Alloc> shaders;
    Map<Token, Reload_Token, Alloc> reloads;
    Map<Reload_Token, FunctionN<16, void(Shader_Loader&)>, Alloc> callbacks;
};

} // namespace rvk
