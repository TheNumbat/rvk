
#include <imgui/imgui.h>

#include "commands.h"
#include "device.h"
#include "memory.h"

#ifdef RPP_OS_LINUX
#include <sys/epoll.h>
#endif

namespace rvk::impl {

using namespace rpp;

Fence::Fence(Arc<Device, Alloc> D) : device(move(D)) {

    VkExportFenceCreateInfo export_info = {
        .sType = VK_STRUCTURE_TYPE_EXPORT_FENCE_CREATE_INFO,
#ifdef RPP_OS_WINDOWS
        .handleTypes = VK_EXTERNAL_FENCE_HANDLE_TYPE_OPAQUE_WIN32_BIT,
#else
        .handleTypes = VK_EXTERNAL_FENCE_HANDLE_TYPE_SYNC_FD_BIT,
#endif
    };

    VkFenceCreateInfo info = {
        .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
        .pNext = &export_info,
        .flags = VK_FENCE_CREATE_SIGNALED_BIT,
    };

    RVK_CHECK(vkCreateFence(*device, &info, null, &fence));
}

Fence::~Fence() {
    release();
}

void Fence::release() {
    if(fence) vkDestroyFence(*device, fence, null);
    fence = null;
}

Fence::Fence(Fence&& src) : device(move(src.device)), fence(src.fence) {
    src.fence = null;
}

Fence& Fence::operator=(Fence&& src) {
    if(this == &src) return *this;
    release();
    device = move(src.device);
    fence = src.fence;
    src.fence = null;
    return *this;
}

void Fence::wait() const {
    assert(fence);
    RVK_CHECK(vkWaitForFences(*device, 1, &fence, VK_TRUE, UINT64_MAX));
}

void Fence::reset() {
    assert(fence);
    RVK_CHECK(vkResetFences(*device, 1, &fence));
}

Async::Event Fence::event() const {
    assert(fence);
#ifdef RPP_OS_WINDOWS
    HANDLE handle = null;
    VkFenceGetWin32HandleInfoKHR info = {
        .sType = VK_STRUCTURE_TYPE_FENCE_GET_WIN32_HANDLE_INFO_KHR,
        .fence = fence,
        .handleType = VK_EXTERNAL_FENCE_HANDLE_TYPE_OPAQUE_WIN32_BIT,
    };
    RVK_CHECK(vkGetFenceWin32HandleKHR(*device, &info, &handle));
    return Async::Event::of_sys(handle);
#else
    i32 fd = -1;
    VkFenceGetFdInfoKHR info = {
        .sType = VK_STRUCTURE_TYPE_FENCE_GET_FD_INFO_KHR,
        .fence = fence,
        .handleType = VK_EXTERNAL_FENCE_HANDLE_TYPE_SYNC_FD_BIT,
    };
    VkResult result = vkGetFenceFdKHR(*device, &info, &fd);
    if(result != VK_SUCCESS) {
        die("[rvk] Failed to export fence: %", describe(result));
    }
    if(fd == -1) {
        Async::Event event;
        event.signal();
        return event;
    }
    return Async::Event::of_sys(fd, EPOLLIN);
#endif
}

bool Fence::ready() const {
    assert(fence);
    VkResult res = vkGetFenceStatus(*device, fence);
    if(res == VK_SUCCESS) return true;
    if(res == VK_NOT_READY) return false;
    RVK_CHECK(res);
    RPP_UNREACHABLE;
}

Semaphore::Semaphore(Arc<Device, Alloc> D) : device(move(D)) {
    VkSemaphoreCreateInfo info = {
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
    };
    RVK_CHECK(vkCreateSemaphore(*device, &info, null, &semaphore));
}

Semaphore::~Semaphore() {
    release();
}

void Semaphore::release() {
    if(semaphore) vkDestroySemaphore(*device, semaphore, null);
    semaphore = null;
}

Semaphore::Semaphore(Semaphore&& src) : device(move(src.device)), semaphore(src.semaphore) {
    src.semaphore = null;
}

Semaphore& Semaphore::operator=(Semaphore&& src) {
    if(this == &src) return *this;
    release();
    device = move(src.device);
    semaphore = src.semaphore;
    src.semaphore = null;
    return *this;
}

Commands::Commands(Arc<Command_Pool, Alloc> pool, Queue_Family family, VkCommandBuffer buffer)
    : pool(move(pool)), buffer(buffer), family_(family) {
}

Commands::~Commands() {
    release();
}

void Commands::release() {
    if(buffer) pool->release(buffer);
    buffer = null;
    transient_buffers.clear();
}

Commands::Commands(Commands&& src) {
    *this = move(src);
}

Commands& Commands::operator=(Commands&& src) {
    if(this == &src) return *this;
    Thread::Lock l0(mutex);
    Thread::Lock l1(src.mutex);
    release();
    pool = move(src.pool);
    transient_buffers = move(src.transient_buffers);
    family_ = src.family_;
    buffer = src.buffer;
    src.buffer = null;
    return *this;
}

void Commands::attach(Buffer buf) {
    assert(buffer);
    Thread::Lock lock(mutex);
    transient_buffers.push(move(buf));
}

void Commands::reset() {
    assert(buffer);

    RVK_CHECK(vkResetCommandBuffer(buffer, 0));

    {
        Thread::Lock lock(mutex);
        transient_buffers.clear();
    }

    VkCommandBufferBeginInfo begin_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
    };

    RVK_CHECK(vkBeginCommandBuffer(buffer, &begin_info));
}

void Commands::end() {
    assert(buffer);
    RVK_CHECK(vkEndCommandBuffer(buffer));
}

Command_Pool::Command_Pool(Arc<Device, Alloc> device, Queue_Family family, VkCommandPool pool)
    : device(move(device)), command_pool(pool), family(family) {
}

Command_Pool::~Command_Pool() {
    if(command_pool) {
        Thread::Lock lock(mutex);
        vkDestroyCommandPool(*device, command_pool, null);
    }
    command_pool = null;
}

Commands Command_Pool::make() {
    Thread::Lock lock(mutex);

    VkCommandBuffer buffer = null;
    if(!free_list.empty()) {
        buffer = free_list.back();
        free_list.pop();
        vkResetCommandBuffer(buffer, 0);
    } else {
        VkCommandBufferAllocateInfo info = {
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
            .commandPool = command_pool,
            .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
            .commandBufferCount = 1,
        };
        RVK_CHECK(vkAllocateCommandBuffers(*device, &info, &buffer));
    }

    VkCommandBufferBeginInfo begin_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
    };
    RVK_CHECK(vkBeginCommandBuffer(buffer, &begin_info));

    return Commands{Arc<Command_Pool, Alloc>::from_this(this), family, buffer};
}

void Command_Pool::release(VkCommandBuffer buffer) {
    Thread::Lock lock(mutex);
    free_list.push(buffer);
}

template<Queue_Family F>
Command_Pool_Manager<F>::Command_Pool_Manager(Arc<Device, Alloc> device)
    : device(move(device)), pools(Arc<Pools, Alloc>::make()) {
}

template<Queue_Family F>
Command_Pool_Manager<F>::~Command_Pool_Manager() {

    Thread::Lock lock(pools->mutex);
    pools->closed = true;
    pools->active_threads = {};
    pools->free_list = {};
    info("[rvk] Destroyed command pools for family %.", F);
}

template<Queue_Family F>
void Command_Pool_Manager<F>::begin_thread() {

    this_thread.release();
    this_thread.pools = pools.dup();

    Thread::Id id = Thread::this_id();
    Thread::Lock lock(pools->mutex);

    assert(!pools->active_threads.contains(id));

    if(pools->free_list.empty()) {

        Profile::Time_Point start = Profile::timestamp();

        VkCommandPoolCreateInfo create_info = {
            .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
            .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
            .queueFamilyIndex = device->queue_index(F),
        };

        VkCommandPool pool = null;
        RVK_CHECK(vkCreateCommandPool(*device, &create_info, null, &pool));

        pools->active_threads.insert(id, Arc<Command_Pool, Alloc>::make(device.dup(), F, pool));

        Profile::Time_Point end = Profile::timestamp();
        info("[rvk] Allocated new % command pool for thread % in %ms.", F, id,
             Profile::ms(end - start));

    } else {

        info("[rvk] Reusing command pool for % in thread %.", F, id);
        pools->active_threads.insert(id, move(pools->free_list.back()));
        pools->free_list.pop();
    }
}

template<Queue_Family F>
Commands Command_Pool_Manager<F>::make() {
    if(!this_thread.pools.ok() || &*this_thread.pools != &*pools) begin_thread();
    Arc<Command_Pool, Alloc> pool;
    {
        Thread::Lock lock(pools->mutex);
        pool = pools->active_threads.get(Thread::this_id()).dup();
    }
    return pool->make();
}

template struct Command_Pool_Manager<Queue_Family::graphics>;
template struct Command_Pool_Manager<Queue_Family::compute>;
template struct Command_Pool_Manager<Queue_Family::transfer>;

} // namespace rvk::impl
