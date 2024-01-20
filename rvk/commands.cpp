
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

    VkFenceCreateInfo info = {};
    info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    info.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    VkExportFenceCreateInfo export_info = {};
    export_info.sType = VK_STRUCTURE_TYPE_EXPORT_FENCE_CREATE_INFO;
#ifdef RPP_OS_WINDOWS
    export_info.handleTypes = VK_EXTERNAL_FENCE_HANDLE_TYPE_OPAQUE_WIN32_BIT;
#else
    export_info.handleTypes = VK_EXTERNAL_FENCE_HANDLE_TYPE_OPAQUE_FD_BIT;
#endif
    info.pNext = &export_info;

    RVK_CHECK(vkCreateFence(*device, &info, null, &fence));
}

Fence::~Fence() {
    if(fence) vkDestroyFence(*device, fence, null);
    fence = null;
}

Fence::Fence(Fence&& src) {
    *this = move(src);
}

Fence& Fence::operator=(Fence&& src) {
    assert(this != &src);
    this->~Fence();
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
    VkFenceGetWin32HandleInfoKHR info = {};
    info.sType = VK_STRUCTURE_TYPE_FENCE_GET_WIN32_HANDLE_INFO_KHR;
    info.fence = fence;
    info.handleType = VK_EXTERNAL_FENCE_HANDLE_TYPE_OPAQUE_WIN32_BIT;
    RVK_CHECK(vkGetFenceWin32HandleKHR(*device, &info, &handle));
    return Async::Event::of_sys(handle);
#else
    i32 fd = -1;
    VkFenceGetFdInfoKHR info = {};
    info.sType = VK_STRUCTURE_TYPE_FENCE_GET_FD_INFO_KHR;
    info.fence = fence;
    info.handleType = VK_EXTERNAL_FENCE_HANDLE_TYPE_OPAQUE_FD_BIT;
    RVK_CHECK(vkGetFenceFdKHR(*device, &info, &fd));
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
    VkSemaphoreCreateInfo info = {};
    info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    RVK_CHECK(vkCreateSemaphore(*device, &info, null, &semaphore));
}

Semaphore::~Semaphore() {
    if(semaphore) vkDestroySemaphore(*device, semaphore, null);
    semaphore = null;
}

Semaphore::Semaphore(Semaphore&& src) {
    *this = move(src);
}

Semaphore& Semaphore::operator=(Semaphore&& src) {
    assert(this != &src);
    this->~Semaphore();
    device = move(src.device);
    semaphore = src.semaphore;
    src.semaphore = null;
    return *this;
}

Commands::Commands(Arc<Command_Pool, Alloc> pool, Queue_Family family, VkCommandBuffer buffer)
    : pool(move(pool)), buffer(buffer), family_(family) {
}

Commands::~Commands() {
    if(buffer) pool->release(buffer);
    buffer = null;
}

Commands::Commands(Commands&& src) {
    *this = move(src);
}

Commands& Commands::operator=(Commands&& src) {
    assert(this != &src);
    this->~Commands();
    pool = move(src.pool);
    transient_buffers = move(src.transient_buffers);
    family_ = src.family_;
    buffer = src.buffer;
    src.buffer = null;
    return *this;
}

void Commands::attach(Buffer buf) {
    assert(buffer);
    transient_buffers.push(move(buf));
}

void Commands::reset() {
    assert(buffer);

    RVK_CHECK(vkResetCommandBuffer(buffer, 0));

    transient_buffers.clear();

    VkCommandBufferBeginInfo begin_info = {};
    begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

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
        VkCommandBufferAllocateInfo info = {};
        info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        info.commandPool = command_pool;
        info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        info.commandBufferCount = 1;
        RVK_CHECK(vkAllocateCommandBuffers(*device, &info, &buffer));
    }

    VkCommandBufferBeginInfo begin_info = {};
    begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    RVK_CHECK(vkBeginCommandBuffer(buffer, &begin_info));

    return Commands{Arc<Command_Pool, Alloc>::from_this(this), family, buffer};
}

void Command_Pool::release(VkCommandBuffer buffer) {
    Thread::Lock lock(mutex);
    free_list.push(buffer);
}

template<Queue_Family F>
Command_Pool_Manager<F>::Command_Pool_Manager(Arc<Device, Alloc> device) : device(move(device)) {
}

template<Queue_Family F>
Command_Pool_Manager<F>::~Command_Pool_Manager() {

    end_thread();
    Thread::Lock lock(mutex);

    if(!active_threads.empty()) {
        warn("[rvk]: Active command pool set for family % is not empty: % remaining.", F,
             active_threads.length());
        active_threads.clear();
    }

    if(!free_list.empty()) {
        free_list.clear();
        info("[rvk] Destroyed command pools for family %.", F);
    }
}

template<Queue_Family F>
void Command_Pool_Manager<F>::begin_thread() {

    assert(!this_thread.pool);

    Thread::Id id = Thread::this_id();
    Thread::Lock lock(mutex);

    assert(!active_threads.contains(id));

    if(free_list.empty()) {

        Profile::Time_Point start = Profile::timestamp();

        VkCommandPoolCreateInfo create_info = {};
        create_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        create_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        create_info.queueFamilyIndex = device->queue_index(F);

        VkCommandPool pool = null;
        RVK_CHECK(vkCreateCommandPool(*device, &create_info, null, &pool));

        this_thread.pool = Box<Command_Pool, Alloc>::make(device.dup(), F, pool);

        Profile::Time_Point end = Profile::timestamp();
        info("[rvk] Allocated new % command pool for thread % in %ms.", F, id,
             Profile::ms(end - start));

    } else {

        info("[rvk] Reusing command pool for % in thread %.", F, id);
        this_thread.pool = move(free_list.back());
        free_list.pop();
    }

    this_thread.pool_manager = Ref{*this};
    active_threads.insert(id, {});
}

template<Queue_Family F>
void Command_Pool_Manager<F>::end_thread() {

    if(!this_thread.pool) return;

    assert(this_thread.pool_manager == Ref{*this});

    Thread::Id id = Thread::this_id();
    Thread::Lock lock(mutex);

    assert(active_threads.contains(id));

    free_list.push(move(this_thread.pool));
    active_threads.erase(id);

    this_thread.pool = {};
    this_thread.pool_manager = {};
}

template<Queue_Family F>
Commands Command_Pool_Manager<F>::make() {
    if(!this_thread.pool) begin_thread();
    return this_thread.pool->make();
}

template struct Command_Pool_Manager<Queue_Family::graphics>;
template struct Command_Pool_Manager<Queue_Family::compute>;
template struct Command_Pool_Manager<Queue_Family::transfer>;

} // namespace rvk::impl