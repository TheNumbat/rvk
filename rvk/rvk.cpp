
#include <imgui/imgui.h>

#include "commands.h"
#include "descriptors.h"
#include "device.h"
#include "imgui_impl_vulkan.h"
#include "instance.h"
#include "memory.h"
#include "rvk.h"
#include "swapchain.h"

namespace rvk {

using namespace rpp;

namespace impl {

struct Deletion_Queue {

    explicit Deletion_Queue() = default;
    ~Deletion_Queue() = default;

    Deletion_Queue(const Deletion_Queue&) = delete;
    Deletion_Queue& operator=(const Deletion_Queue&) = delete;

    Deletion_Queue(Deletion_Queue&& src) : deletion_queue(move(src.deletion_queue)) {
    }
    Deletion_Queue& operator=(Deletion_Queue&& src) {
        assert(this != &src);
        deletion_queue = move(src.deletion_queue);
        return *this;
    }

    void clear() {
        Thread::Lock lock{mutex};
        deletion_queue.clear();
    }
    void push(Finalizer&& finalizer) {
        Thread::Lock lock{mutex};
        deletion_queue.push(move(finalizer));
    }

private:
    Thread::Mutex mutex;
    Vec<Finalizer, Alloc> deletion_queue;
};

struct Frame {
    explicit Frame(Arc<Command_Pool_Manager<Queue_Family::graphics>, Alloc>& graphics_command_pool,
                   Fence fence, Semaphore available, Semaphore complete)
        : fence{move(fence)}, cmds{graphics_command_pool->make()}, available{move(available)},
          complete{move(complete)} {
    }
    ~Frame() = default;

    Frame(const Frame&) = delete;
    Frame& operator=(const Frame&) = delete;

    Frame(Frame&& src)
        : fence(move(src.fence)), cmds(move(src.cmds)), available(move(src.available)),
          complete(move(src.complete)), wait_for(move(src.wait_for)) {
    }
    Frame& operator=(Frame&& src) {
        assert(this != &src);
        fence = move(src.fence);
        cmds = move(src.cmds);
        available = move(src.available);
        complete = move(src.complete);
        wait_for = move(src.wait_for);
        return *this;
    }

    Fence fence;
    Commands cmds;
    Semaphore available, complete;

    void wait(Sem_Ref sem) {
        Thread::Lock lock{mutex};
        wait_for.push(move(sem));
    }
    void clear() {
        Thread::Lock lock{mutex};
        wait_for.clear();
    }
    Slice<Sem_Ref> waits() {
        return wait_for.slice();
    }

private:
    Thread::Mutex mutex;
    Vec<Sem_Ref, Alloc> wait_for;
};

struct Vk {
    Arc<Instance, Alloc> instance;
    Arc<Debug_Callback, Alloc> debug_callback;
    Arc<Physical_Device, Alloc> physical_device;
    Arc<Device, Alloc> device;
    Arc<Device_Memory, Alloc> host_memory;
    Vec<Arc<Device_Memory, Alloc>, Alloc> device_memories;
    Arc<Swapchain, Alloc> swapchain;
    Arc<Descriptor_Pool, Alloc> descriptor_pool;
    Arc<Command_Pool_Manager<Queue_Family::graphics>, Alloc> graphics_command_pool;
    Arc<Command_Pool_Manager<Queue_Family::transfer>, Alloc> transfer_command_pool;
    Arc<Command_Pool_Manager<Queue_Family::compute>, Alloc> compute_command_pool;
    Arc<Compositor, Alloc> compositor;

    Vec<Frame, Alloc> frames;
    Vec<Deletion_Queue, Alloc> deletion_queues;

    struct State {
        bool has_imgui = false;
        bool has_validation = false;
        bool resized_last_frame = false;
        bool minimized = false;

        bool has_hdr = false;
        bool is_hdr = false;
        bool was_hdr = false;

        u32 frames_in_flight = 0;
        u32 frame_index = 0;
        u32 swapchain_index = 0;

        void advance() {
            frame_index = (frame_index + 1) % frames_in_flight;
        }
    };
    State state;

    explicit Vk(Config config);
    ~Vk();

    void imgui();
    void create_imgui();
    void destroy_imgui();
    void recreate_swapchain();

    void wait_idle();
    void begin_frame();
    void end_frame(Image_View& output);

    Fence make_fence();
    Semaphore make_semaphore();

    Pipeline make_pipeline(Pipeline::Info info);
    Opt<Binding_Table> make_table(Commands& cmds, Pipeline& pipeline,
                                  Binding_Table::Mapping mapping);

    Opt<TLAS::Buffers> make_tlas(u32 instances);
    Opt<BLAS::Buffers> make_blas(Slice<const BLAS::Size> sizes);

    TLAS build_tlas(Commands& cmds, TLAS::Buffers tlas, Buffer gpu_instances,
                    Slice<const TLAS::Instance> cpu_instances);
    BLAS build_blas(Commands& cmds, BLAS::Buffers blas, Buffer geometry,
                    Slice<const BLAS::Offset> offsets);
};

static Opt<Vk> singleton;

Vk::Vk(Config config) {

    state.has_hdr = config.hdr;
    state.has_imgui = config.imgui;
    state.frames_in_flight = config.frames_in_flight;
    state.has_validation = config.validation;

    instance =
        Arc<Instance, Alloc>::make(move(config.swapchain_extensions), move(config.layers),
                                   move(config.create_surface), config.validation, config.hdr);

    debug_callback = Arc<Debug_Callback, Alloc>::make(instance.dup());

    physical_device = instance->physical_device(instance->surface(), config.ray_tracing);

    device = Arc<Device, Alloc>::make(physical_device.dup(), instance->surface(),
                                      config.ray_tracing, config.robust_accesses);

    u64 max_allocation = physical_device->max_allocation();
    {
        u64 heap_size = device->heap_size(Heap::host);
        if(heap_size < Math::MB(64)) {
            die("[rvk] Host heap is too small: %mb / 64mb.", heap_size / Math::MB(1));
        }
        if(config.host_heap > max_allocation) {
            warn("[rvk] Requested host heap is larger than the max allocation size, using max.");
            config.host_heap = max_allocation;
        }
        if(config.host_heap > heap_size) {
            warn("[rvk] Requested host heap is larger than available, using entire heap.");
            config.host_heap = heap_size;
        }
        host_memory = Arc<Device_Memory, Alloc>::make(physical_device, device.dup(), Heap::host,
                                                      config.host_heap);
    }
    {
        u64 heap_size = device->heap_size(Heap::device);
        if(heap_size < Math::MB(128)) {
            die("[rvk] Device heap is too small: %mb / 128mb.", heap_size / Math::MB(1));
        }
        if(config.device_heap > heap_size) {
            warn("[rvk] Requested device margin is larger than available, using entire heap.");
            config.device_heap = heap_size;
        }

        u64 allocated = 0;
        u64 target = config.device_heap;
        while(allocated < target) {
            u64 size = Math::min(target - allocated, physical_device->max_allocation());
            device_memories.push(
                Arc<Device_Memory, Alloc>::make(physical_device, device.dup(), Heap::device, size));
            allocated += size;
        }
    }

    descriptor_pool = Arc<Descriptor_Pool, Alloc>::make(device.dup(), config.descriptors_per_type,
                                                        config.ray_tracing);

    graphics_command_pool =
        Arc<Command_Pool_Manager<Queue_Family::graphics>, Alloc>::make(device.dup());

    transfer_command_pool =
        Arc<Command_Pool_Manager<Queue_Family::transfer>, Alloc>::make(device.dup());

    compute_command_pool =
        Arc<Command_Pool_Manager<Queue_Family::compute>, Alloc>::make(device.dup());

    { // Create per-frame resources
        Profile::Time_Point start = Profile::timestamp();

        frames.reserve(config.frames_in_flight);
        for(u32 i = 0; i < config.frames_in_flight; i++) {
            frames.emplace(graphics_command_pool, make_fence(), make_semaphore(), make_semaphore());
            deletion_queues.emplace();
        }

        Profile::Time_Point end = Profile::timestamp();
        info("Created resources for % frame(s) in %ms.", config.frames_in_flight,
             Profile::ms(end - start));
    }

    { // Can't use sync() yet because we haven't finished creating the Vk singleton
        auto fence = make_fence();
        auto cmds = graphics_command_pool->make();
        swapchain =
            Arc<Swapchain, Alloc>::make(cmds, physical_device, device.dup(), instance->surface(),
                                        config.frames_in_flight, state.is_hdr);
        cmds.end();
        device->submit(cmds, 0, fence);
        fence.wait();
    }

    compositor = Arc<Compositor, Alloc>::make(device.dup(), swapchain.dup(), descriptor_pool);

    create_imgui();
}

Vk::~Vk() {
    wait_idle();
    destroy_imgui();
}

void Vk::imgui() {
    if(!state.has_imgui) return;

    using namespace ImGui;

    Text("Frame: %u | Image: %u", state.frame_index, state.swapchain_index);
    Text("Swapchain images: %u | Max frames: %u", swapchain->slot_count(), state.frames_in_flight);
    Text("Extent: %ux%u", swapchain->extent().width, swapchain->extent().height);

    if(TreeNodeEx("Device Heaps", ImGuiTreeNodeFlags_DefaultOpen)) {
        i32 i = 0;
        for(auto& device_memory : device_memories) {
            PushID(i++);
            if(TreeNodeEx("##heap", ImGuiTreeNodeFlags_DefaultOpen, "[%d]", i)) {
                device_memory->imgui();
                TreePop();
            }
            PopID();
        }
        TreePop();
    }
    if(TreeNodeEx("Host Heap", ImGuiTreeNodeFlags_DefaultOpen)) {
        host_memory->imgui();
        TreePop();
    }
    if(TreeNode("Device")) {
        device->imgui();
        TreePop();
    }
    if(TreeNode("Physical Device")) {
        physical_device->imgui();
        TreePop();
    }
    if(TreeNode("Instance")) {
        instance->imgui();
        TreePop();
    }
}

void Vk::destroy_imgui() {
    if(!state.has_imgui) return;

    ImGui_ImplVulkan_Shutdown();
}

void Vk::create_imgui() {
    if(!state.has_imgui) return;

    Profile::Time_Point start = Profile::timestamp();

    ImGui_ImplVulkan_InitInfo init = {
        .Instance = *instance,
        .PhysicalDevice = *physical_device,
        .Device = *device,
        .QueueFamily = *physical_device->queue_index(Queue_Family::graphics),
        .Queue = device->queue(Queue_Family::graphics),
        .DescriptorPool = *descriptor_pool,
        .MinImageCount = Math::min(2u, swapchain->min_image_count()),
        .UseDynamicRendering = true,
        .ColorAttachmentFormat = swapchain->format(),
        .CheckVkResultFn = check,
    };

    // The ImGui Vulkan backend will create resources for this many frames.
    init.ImageCount = Math::max(init.MinImageCount,
                                Math::max(state.frames_in_flight, swapchain->min_image_count()));

    if(!ImGui_ImplVulkan_Init(&init, null)) {
        die("[rvk] Failed to initialize ImGui vulkan backend!");
    }

    Profile::Time_Point end = Profile::timestamp();
    info("[rvk] Created ImGui vulkan backend in %ms.", Profile::ms(end - start));
}

void Vk::wait_idle() {
    device->wait_idle();
    for(auto& queue : deletion_queues) {
        queue.clear();
    }
}

void Vk::begin_frame() {

    state.resized_last_frame = false;

    // If we wrapped all the way around the in flight frames and got to a frame that
    // is still executing, wait for it to finish so we can free/reuse its resources.
    Trace("Wait for frame slot") {
        frames[state.frame_index].fence.wait();
    }

    // Erase resources dropped while this frame was in flight
    Trace("Erase dropped resources") {
        deletion_queues[state.frame_index].clear();
    }

    if(state.has_imgui) {
        ImGui_ImplVulkan_NewFrame();
        ImGui::NewFrame();
    }

    if(state.minimized) return;

    // Get next swapchain image. This will return immediately with the index of the
    // next swapchain image that we will use. When that image is actually ready,
    // it will signal frame.avail, which triggers our frame commands. The image
    // is not considered available until the present to it completes, so there is an
    // implicit sync between here and vkQueuePresentKHR in end_frame.
    // Total frame syncs:
    //  frame fence --> user commands -|barrier
    //                                 v
    //         img acq -frame.avail->  x  -frame.finish-> presentation -implicit-> img acq
    VkResult result;
    Trace("Acquire next image") {
        result = vkAcquireNextImageKHR(*device, *swapchain, RPP_UINT64_MAX,
                                       frames[state.frame_index].available, null,
                                       &state.swapchain_index);
    }

    // Out of date if a resize is required
    if(result == VK_ERROR_OUT_OF_DATE_KHR) {
        info("[rvk] Swapchain out of date, recreating...");
        recreate_swapchain();
    } else if(result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
        die("[rvk] Failed to acquire next image: %", describe(result));
    }

    // Previously, we waited for the complete fence of the frame currently
    // using this swapchain image. However, was unecessary because
    // we do not start rendering to the swapchain image until we get the frame.available
    // semaphore signal, which indicates that the other frame must have presented.
    // In the meantime, we can use this frame's resources to start rendering this frame,
    // and only wait for the previous slot occupant once we're ready to composite.
    // TLDR: having a compositor means the main frame parallelism isn't limited by
    // the number of swapchain slots. Only the compositing commands have to wait
    // for the frame to actually be available; the main frame can start rendering
    // to its exclusive resources whenever the previous usage of this slot is done.
}

void Vk::end_frame(Image_View& output) {

    if(state.has_imgui) {
        ImGui::Render();
    }

    // While minimized, check if we are no longer minimized
    if(state.minimized) {
        recreate_swapchain();
        return;
    }

    Frame& frame = frames[state.frame_index];

    // Send frame to GPU queues
    {
        // Set up primary compositing command buffer
        frame.cmds.reset();
        compositor->render(frame.cmds, state.frame_index, state.swapchain_index, state.has_imgui,
                           state.is_hdr, output);
        frame.cmds.end();

        // Wait for frame available before running the submit; signal frame complete on finish
        frame.wait(Sem_Ref{frame.available, VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT});

        auto signal = Sem_Ref{frame.complete, VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT};
        device->submit(frame.cmds, 0, Slice{signal}, frame.waits(), frame.fence);

        frame.clear();
    }

    // Wait for the frame to complete before presenting
    VkSemaphore complete = frame.complete;
    VkSwapchainKHR vk_swapchain = *swapchain;

    VkPresentInfoKHR present_info = {
        .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &complete,
        .swapchainCount = 1,
        .pSwapchains = &vk_swapchain,
        .pImageIndices = &state.swapchain_index,
    };

    // Submit presentation command
    VkResult result = device->present(present_info);
    if(result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
        recreate_swapchain();
    } else if(result != VK_SUCCESS) {
        die("[rvk] Failed to present swapchain image: %", describe(result));
    }

    // If HDR state changed, recreate swapchain
    if(state.has_hdr && state.was_hdr != state.is_hdr) {
        recreate_swapchain();
        state.was_hdr = state.is_hdr;
        return;
    }

    // Circular increment of in-flight frame index
    state.advance();
}

void Vk::recreate_swapchain() {

    VkExtent2D ext = Swapchain::choose_extent(physical_device->capabilities(instance->surface()));

    state.minimized = ext.width == 0 && ext.height == 0;
    if(state.minimized) return;

    info("[rvk] Recreating swapchain...");
    Log_Indent {
        Profile::Time_Point start = Profile::timestamp();

        wait_idle();
        destroy_imgui();
        compositor = {};
        swapchain = {};

        sync([&](Commands& cmds) {
            swapchain = Arc<Swapchain, Alloc>::make(cmds, physical_device, device.dup(),
                                                    instance->surface(), state.frames_in_flight,
                                                    state.is_hdr);
        });
        compositor = Arc<Compositor, Alloc>::make(device.dup(), swapchain.dup(), descriptor_pool);

        create_imgui();

        state.resized_last_frame = true;

        Profile::Time_Point end = Profile::timestamp();
        info("[rvk] Recreated swapchain in %ms.", Profile::ms(end - start));
    }
}

Fence Vk::make_fence() {
    return Fence{device.dup()};
}

Semaphore Vk::make_semaphore() {
    return Semaphore{device.dup()};
}

Opt<TLAS::Buffers> Vk::make_tlas(u32 instances) {
    for(auto& device_memory : device_memories) {
        if(auto tlas = TLAS::make(device_memory, instances); tlas.ok()) {
            return tlas;
        }
    }
    return {};
}

Opt<BLAS::Buffers> Vk::make_blas(Slice<const BLAS::Size> sizes) {
    for(auto& device_memory : device_memories) {
        if(auto blas = BLAS::make(device_memory, sizes); blas.ok()) {
            return blas;
        }
    }
    return {};
}

TLAS Vk::build_tlas(Commands& cmds, TLAS::Buffers tlas, Buffer gpu_instances,
                    Slice<const TLAS::Instance> cpu_instances) {
    return TLAS::build(device.dup(), cmds, move(tlas), move(gpu_instances), cpu_instances);
}

BLAS Vk::build_blas(Commands& cmds, BLAS::Buffers blas, Buffer geometry,
                    Slice<const BLAS::Offset> offsets) {
    return BLAS::build(device.dup(), cmds, move(blas), move(geometry), offsets);
}

Pipeline Vk::make_pipeline(Pipeline::Info info) {
    return Pipeline{singleton->device.dup(), move(info)};
}

Opt<Binding_Table> Vk::make_table(Commands& cmds, Pipeline& pipeline,
                                  Binding_Table::Mapping mapping) {
    return Binding_Table::make(singleton->device.dup(), cmds, pipeline, move(mapping));
}

bool validation_enabled() {
    return singleton->state.has_validation;
}

Arc<Device, Alloc> get_device() {
    return singleton->device.dup();
}

} // namespace impl

bool startup(Config config) {
    if(impl::singleton.ok()) {
        die("[rvk] Already started up!");
    }
    Profile::Time_Point start = Profile::timestamp();
    impl::singleton.emplace(move(config));
    Profile::Time_Point end = Profile::timestamp();
    info("[rvk] Completed startup in %ms.", Profile::ms(end - start));
    return true;
}

void shutdown() {
    impl::singleton.clear();
    info("[rvk] Completed shutdown.");
}

void wait_idle() {
    impl::singleton->wait_idle();
}

void reset_imgui() {
    impl::singleton->wait_idle();
    impl::singleton->destroy_imgui();
    impl::singleton->create_imgui();
}

void imgui() {
    impl::singleton->imgui();
}

void hdr(bool enable) {
    if(impl::singleton->state.has_hdr) {
        impl::singleton->state.was_hdr = impl::singleton->state.is_hdr;
        impl::singleton->state.is_hdr = enable;
    }
}

VkExtent2D extent() {
    return impl::singleton->swapchain->extent();
}

VkFormat format() {
    return impl::singleton->swapchain->format();
}

void begin_frame() {
    impl::singleton->begin_frame();
}

bool minimized() {
    return impl::singleton->state.minimized;
}

void wait_frame(Sem_Ref sem) {
    impl::singleton->frames[impl::singleton->state.frame_index].wait(sem);
}

void end_frame(Image_View& output) {
    impl::singleton->end_frame(output);
}

u32 frame() {
    return impl::singleton->state.frame_index;
}

u32 frame_count() {
    return impl::singleton->state.frames_in_flight;
}

u32 previous_frame() {
    return (impl::singleton->state.frame_index + impl::singleton->state.frames_in_flight - 1) %
           impl::singleton->state.frames_in_flight;
}

bool resized() {
    return impl::singleton->state.resized_last_frame;
}

void drop(Finalizer f) {
    impl::singleton->deletion_queues[impl::singleton->state.frame_index].push(move(f));
}

Fence make_fence() {
    return impl::singleton->make_fence();
}

Semaphore make_semaphore() {
    return impl::singleton->make_semaphore();
}

Commands make_commands(Queue_Family family) {
    switch(family) {
    case Queue_Family::graphics: return impl::singleton->graphics_command_pool->make();
    case Queue_Family::transfer: return impl::singleton->transfer_command_pool->make();
    case Queue_Family::compute: return impl::singleton->compute_command_pool->make();
    default: RPP_UNREACHABLE;
    }
}

Opt<Buffer> make_staging(u64 size) {
    return impl::singleton->host_memory->make(size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT |
                                                        VK_BUFFER_USAGE_TRANSFER_DST_BIT);
}

Opt<Buffer> make_buffer(u64 size, VkBufferUsageFlags usage) {
    for(auto& device_memory : impl::singleton->device_memories) {
        if(auto buf = device_memory->make(size, usage); buf.ok()) {
            return buf;
        }
    }
    return {};
}

Opt<Image> make_image(VkExtent3D extent, VkFormat format, VkImageUsageFlags usage) {
    for(auto& device_memory : impl::singleton->device_memories) {
        if(auto img = device_memory->make(extent, format, usage); img.ok()) {
            return img;
        }
    }
    return {};
}

Sampler make_sampler(Sampler::Config config) {
    return Sampler{impl::singleton->device.dup(), config};
}

Opt<TLAS::Buffers> make_tlas(u32 instances) {
    return impl::singleton->make_tlas(instances);
}

Opt<BLAS::Buffers> make_blas(Slice<const BLAS::Size> sizes) {
    return impl::singleton->make_blas(sizes);
}

TLAS build_tlas(Commands& cmds, TLAS::Buffers tlas, Buffer gpu_instances,
                Slice<const TLAS::Instance> cpu_instances) {
    return impl::singleton->build_tlas(cmds, move(tlas), move(gpu_instances), cpu_instances);
}

BLAS build_blas(Commands& cmds, BLAS::Buffers blas, Buffer geometry,
                Slice<const BLAS::Offset> offsets) {
    return impl::singleton->build_blas(cmds, move(blas), move(geometry), offsets);
}

void submit(Commands& cmds, u32 index) {
    impl::singleton->device->submit(cmds, index);
}

void submit(Commands& cmds, u32 index, Fence& fence) {
    impl::singleton->device->submit(cmds, index, fence);
}

void submit(Commands& cmds, u32 index, Slice<const Sem_Ref> wait, Slice<const Sem_Ref> signal) {
    impl::singleton->device->submit(cmds, index, wait, signal);
}

void submit(Commands& cmds, u32 index, Slice<const Sem_Ref> wait, Slice<const Sem_Ref> signal,
            Fence& fence) {
    impl::singleton->device->submit(cmds, index, wait, signal, fence);
}

Pipeline make_pipeline(impl::Pipeline::Info info) {
    return impl::singleton->make_pipeline(move(info));
}

Opt<Binding_Table> make_table(Commands& cmds, impl::Pipeline& pipeline,
                              Binding_Table::Mapping mapping) {
    return impl::singleton->make_table(cmds, pipeline, mapping);
}

Descriptor_Set make_set(Descriptor_Set_Layout& layout, u32 variable_count) {
    return impl::singleton->descriptor_pool->make(layout, impl::singleton->state.frames_in_flight,
                                                  variable_count);
}

Descriptor_Set make_single_set(Descriptor_Set_Layout& layout, u32 variable_count) {
    return impl::singleton->descriptor_pool->make(layout, 1, variable_count);
}

Box<Shader_Loader, Alloc> make_shader_loader() {
    return Box<Shader_Loader, Alloc>::make(impl::singleton->device.dup());
}

} // namespace rvk
