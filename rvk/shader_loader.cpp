
#include "shader_loader.h"

#include <rpp/asyncio.h>

namespace rvk {

impl::Shader& Shader_Loader::get(Token token) {
    assert(device);
    return shaders.get(token).first;
}

Shader_Loader::Token Shader_Loader::compile(String_View path) {
    assert(device);

    if(Opt<Vec<u8, Files::Alloc>> data = Files::read(path)) {

        Shader shader{device.dup(), data->slice()};
        Files::Write_Watcher watcher{path};

        Token token = next_token.incr();
        {
            Thread::Lock lock{mutex};
            shaders.insert(token, Pair{move(shader), move(watcher)});
        }

        return token;
    }

    die("[rvk] Failed to read shader from %!", path);
}

Async::Task<Shader_Loader::Token> Shader_Loader::compile_async(Async::Pool<>& pool,
                                                               String_View path) {
    assert(device);

    if(Opt<Vec<u8, Files::Alloc>> data = co_await Async::read(pool, path)) {

        // Will compile on another thread
        Shader shader{device.dup(), data->slice()};
        Files::Write_Watcher watcher{path};

        Token token = next_token.incr();
        {
            Thread::Lock lock{mutex};
            shaders.insert(token, Pair{move(shader), move(watcher)});
        }

        co_return token;
    }

    die("[rvk] Failed to read shader from %!", path);
}

void Shader_Loader::try_reload() {
    assert(device);

    Thread::Lock lock{mutex};

    Region(R) {
        Map<Reload_Token, Empty<>, Mregion<R>> callbacks_to_run;

        for(auto& [token, shader] : shaders) {
            if(shader.second.poll()) {
                if(Opt<Vec<u8, Files::Alloc>> data = shader.second.read()) {
                    shader.first = Shader{device.dup(), data->slice()};
                    callbacks_to_run.insert(reloads.get(token), {});
                }
            }
        }

        for(auto [token, _] : callbacks_to_run) {
            static_cast<void>(_);
            callbacks.get(token)(*this);
        }
    }
}

void Shader_Loader::on_reload(Slice<Shader_Loader::Token> tokens,
                              FunctionN<16, void(Shader_Loader&)> callback) {
    assert(device);

    Thread::Lock lock{mutex};

    Reload_Token reload_token = next_reload_token++;
    callbacks.insert(reload_token, move(callback));

    for(auto token : tokens) {
        reloads.insert(token, reload_token);
    }
}

} // namespace rvk
