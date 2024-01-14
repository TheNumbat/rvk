
#pragma once

#include <rpp/async.h>
#include <rpp/base.h>
#include <rpp/rc.h>

#include "fwd.h"

namespace rvk::impl {

using namespace rpp;

struct Fence {

    explicit Fence(Arc<Device, Alloc> device);
    ~Fence();

    Fence(const Fence&) = delete;
    Fence& operator=(const Fence&) = delete;
    Fence(Fence&&);
    Fence& operator=(Fence&&);

    operator VkFence() {
        return fence;
    }

    void reset();
    void wait() const;
    bool ready() const;
    Async::Event event() const;

private:
    Arc<Device, Alloc> device;
    VkFence fence = null;
};

struct Semaphore {

    explicit Semaphore(Arc<Device, Alloc> device);
    ~Semaphore();

    Semaphore(const Semaphore&) = delete;
    Semaphore& operator=(const Semaphore&) = delete;
    Semaphore(Semaphore&&);
    Semaphore& operator=(Semaphore&&);

    operator VkSemaphore() const {
        return semaphore;
    }

private:
    Arc<Device, Alloc> device;
    VkSemaphore semaphore = null;
};

struct Commands {

    ~Commands();

    Commands(const Commands&) = delete;
    Commands& operator=(const Commands&) = delete;
    Commands(Commands&&);
    Commands& operator=(Commands&&);

    operator VkCommandBuffer() const {
        return buffer;
    }

    void reset();
    void end();

private:
    explicit Commands(Arc<Command_Pool, Alloc> pool, VkCommandBuffer buffer);

    Arc<Command_Pool, Alloc> pool;
    VkCommandBuffer buffer = null;

    friend struct Command_Pool;
};

struct Command_Pool {

    ~Command_Pool();

    Command_Pool(const Command_Pool& src) = delete;
    Command_Pool& operator=(const Command_Pool& src) = delete;
    Command_Pool(Command_Pool&& src) = delete;
    Command_Pool& operator=(Command_Pool&& src) = delete;

private:
    explicit Command_Pool(Arc<Device, Alloc> device, VkCommandPool pool);
    friend struct Arc<Command_Pool, Alloc>;

    Commands make();
    void release(VkCommandBuffer commands);

    Arc<Device, Alloc> device;
    VkCommandPool command_pool = null;
    Vec<VkCommandBuffer, Alloc> free_list;
    Thread::Mutex mutex;

    template<Queue_Family F>
    friend struct Command_Pool_Manager;
    friend struct Commands;
};

template<Queue_Family F>
struct Command_Pool_Manager {

    ~Command_Pool_Manager();

    Command_Pool_Manager(const Command_Pool_Manager&) = delete;
    Command_Pool_Manager& operator=(const Command_Pool_Manager&) = delete;
    Command_Pool_Manager(Command_Pool_Manager&&) = delete;
    Command_Pool_Manager& operator=(Command_Pool_Manager&&) = delete;

    // When allocating a command buffer, you _MUST_ get it from your current
    // thread's pool, and it _MUST_ be recorded in that thread _ONLY_. Once recording
    // is complete, it _MAY_ then be passed to another thread (e.g. the main thread)
    // for submission and deletion. Deletion is OK on another thread because
    // all it does is push it onto the buffer free list, which is protected by the mutex.
    //
    // As long as you're done recording the command list, it's OK if your thread ends before
    // your commands; the command pool will be recycled and the commands will still free from
    // the right pool. Keeping another free list of pools allows new threads to grab the pool.
    Commands make();

private:
    explicit Command_Pool_Manager(Arc<Device, Alloc> device);
    friend struct Arc<Command_Pool_Manager, Alloc>;

    void begin_thread();
    void end_thread();

    struct This_Thread {
        ~This_Thread() {
            if(pool_manager) pool_manager->end_thread();
        }
        Arc<Command_Pool, Alloc> pool;
        Arc<Command_Pool_Manager, Alloc> pool_manager;
    };

    static inline thread_local This_Thread this_thread;

    Arc<Device, Alloc> device;

    Vec<Arc<Command_Pool, Alloc>, Alloc> free_list;
    Map<Thread::Id, Arc<Command_Pool, Alloc>, Alloc> active_threads;
    Thread::Mutex mutex;
};

} // namespace rvk::impl
