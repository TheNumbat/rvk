
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

void check(VkResult result) {
    RVK_CHECK(result);
}

String_View describe(VkResult result) {
    switch(result) {
#define STR(r)                                                                                     \
    case VK_##r: return #r##_v
        STR(SUCCESS);
        STR(NOT_READY);
        STR(TIMEOUT);
        STR(EVENT_SET);
        STR(EVENT_RESET);
        STR(INCOMPLETE);
        STR(ERROR_OUT_OF_HOST_MEMORY);
        STR(ERROR_OUT_OF_DEVICE_MEMORY);
        STR(ERROR_INITIALIZATION_FAILED);
        STR(ERROR_DEVICE_LOST);
        STR(ERROR_MEMORY_MAP_FAILED);
        STR(ERROR_LAYER_NOT_PRESENT);
        STR(ERROR_EXTENSION_NOT_PRESENT);
        STR(ERROR_FEATURE_NOT_PRESENT);
        STR(ERROR_INCOMPATIBLE_DRIVER);
        STR(ERROR_TOO_MANY_OBJECTS);
        STR(ERROR_FORMAT_NOT_SUPPORTED);
        STR(ERROR_SURFACE_LOST_KHR);
        STR(ERROR_NATIVE_WINDOW_IN_USE_KHR);
        STR(SUBOPTIMAL_KHR);
        STR(ERROR_OUT_OF_DATE_KHR);
        STR(ERROR_INCOMPATIBLE_DISPLAY_KHR);
        STR(ERROR_VALIDATION_FAILED_EXT);
        STR(ERROR_INVALID_SHADER_NV);
#undef STR
    default: return "UNKNOWN_ERROR"_v;
    }
}

[[nodiscard]] String_View describe(VkObjectType type) {
    switch(type) {
#define STR(r)                                                                                     \
    case VK_OBJECT_TYPE_##r: return #r##_v
        STR(UNKNOWN);
        STR(INSTANCE);
        STR(PHYSICAL_DEVICE);
        STR(DEVICE);
        STR(QUEUE);
        STR(SEMAPHORE);
        STR(COMMAND_BUFFER);
        STR(FENCE);
        STR(DEVICE_MEMORY);
        STR(BUFFER);
        STR(IMAGE);
        STR(EVENT);
        STR(QUERY_POOL);
        STR(BUFFER_VIEW);
        STR(IMAGE_VIEW);
        STR(SHADER_MODULE);
        STR(PIPELINE_CACHE);
        STR(PIPELINE_LAYOUT);
        STR(RENDER_PASS);
        STR(PIPELINE);
        STR(DESCRIPTOR_SET_LAYOUT);
        STR(SAMPLER);
        STR(DESCRIPTOR_POOL);
        STR(DESCRIPTOR_SET);
        STR(FRAMEBUFFER);
        STR(COMMAND_POOL);
        STR(SAMPLER_YCBCR_CONVERSION);
        STR(DESCRIPTOR_UPDATE_TEMPLATE);
        STR(SURFACE_KHR);
        STR(SWAPCHAIN_KHR);
        STR(DISPLAY_KHR);
        STR(DISPLAY_MODE_KHR);
        STR(DEBUG_REPORT_CALLBACK_EXT);
        STR(DEBUG_UTILS_MESSENGER_EXT);
        STR(VALIDATION_CACHE_EXT);
#undef STR
    default: return "UNKNOWN_OBJECT"_v;
    }
}

struct Deletion_Queue {

    explicit Deletion_Queue() = default;
    ~Deletion_Queue() = default;

    Deletion_Queue(const Deletion_Queue&) = delete;
    Deletion_Queue& operator=(const Deletion_Queue&) = delete;

    Deletion_Queue(Deletion_Queue&& src) : deletion_queue{move(src.deletion_queue)} {
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
    explicit Frame(Arc<Device, Alloc> device,
                   Arc<Command_Pool_Manager<Queue_Family::graphics>, Alloc>& graphics_command_pool)
        : fence{device.dup()}, cmds{graphics_command_pool->make()}, available{device.dup()},
          complete{device.dup()} {
    }
    ~Frame() = default;

    Frame(const Frame&) = delete;
    Frame& operator=(const Frame&) = delete;
    Frame(Frame&&) = default;
    Frame& operator=(Frame&&) = default;

    Fence fence;
    Commands cmds;
    Semaphore available, complete;
    Vec<Sem_Ref, Alloc> wait_for;
};

struct Vk {
    Arc<Instance, Alloc> instance;
    Arc<Debug_Callback, Alloc> debug_callback;
    Arc<Physical_Device, Alloc> physical_device;
    Arc<Device, Alloc> device;
    Arc<Device_Memory, Alloc> host_memory;
    Arc<Device_Memory, Alloc> device_memory;
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
        bool resized_last_frame = false;
        bool minimized = false;
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

    void begin_frame();
    void end_frame(Image_View& output);

    void wait_idle();
    void submit(Commands& cmds, u32 index, Fence& Fence);
    void submit_and_wait(Commands cmds, u32 index);

    void drop(Finalizer f);
    Fence make_fence();
    Commands make_commands(Queue_Family family);
};

static Opt<Vk> singleton;

Vk::Vk(Config config) {

    state.has_imgui = config.imgui;
    state.frames_in_flight = config.frames_in_flight;

    instance = Arc<Instance, Alloc>::make(move(config.swapchain_extensions), move(config.layers),
                                          move(config.create_surface), config.validation);

    debug_callback = Arc<Debug_Callback, Alloc>::make(instance.dup());

    physical_device = instance->physical_device(instance->surface(), config.ray_tracing);

    device =
        Arc<Device, Alloc>::make(physical_device.dup(), instance->surface(), config.ray_tracing);

    host_memory = Arc<Device_Memory, Alloc>::make(physical_device, device.dup(), Heap::host,
                                                  config.host_heap);

    device_memory = Arc<Device_Memory, Alloc>::make(physical_device, device.dup(), Heap::device,
                                                    device->heap_size(Heap::device) -
                                                        config.device_heap_margin);

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
            frames.emplace(device.dup(), graphics_command_pool);
            deletion_queues.emplace();
        }

        Profile::Time_Point end = Profile::timestamp();
        info("Created resources for % frame(s) in %ms.", config.frames_in_flight,
             Profile::ms(end - start));
    }

    sync([&](Commands& cmds) {
        swapchain = Arc<Swapchain, Alloc>::make(cmds, physical_device, device.dup(),
                                                instance->surface(), config.frames_in_flight);
    });

    compositor = Arc<Compositor, Alloc>::make(device, descriptor_pool, swapchain.dup());

    create_imgui();
}

Vk::~Vk() {
    wait_idle();
    destroy_imgui();
}

void Vk::imgui() {
    if(!state.has_imgui) return;

    using namespace ImGui;

    Begin("rvk info");
    Text("Frame: %u | Image: %u", state.frame_index, state.swapchain_index);
    Text("Swapchain images: %u | Max frames: %u", swapchain->slot_count(), state.frames_in_flight);
    Text("Extent: %ux%u", swapchain->extent().width, swapchain->extent().height);

    if(TreeNodeEx("Device Heap", ImGuiTreeNodeFlags_DefaultOpen)) {
        device_memory->imgui();
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
    End();
}

void Vk::destroy_imgui() {
    if(!state.has_imgui) return;

    ImGui_ImplVulkan_Shutdown();
}

void Vk::create_imgui() {
    if(!state.has_imgui) return;

    Profile::Time_Point start = Profile::timestamp();

    ImGui_ImplVulkan_InitInfo init = {};
    init.Instance = *instance;
    init.PhysicalDevice = *physical_device;
    init.Device = *device;
    init.QueueFamily = *physical_device->queue_index(Queue_Family::graphics);
    init.Queue = device->queue(Queue_Family::graphics);
    init.DescriptorPool = *descriptor_pool;
    init.MinImageCount = swapchain->min_image_count();
    init.CheckVkResultFn = check;
    init.UseDynamicRendering = true;
    init.ColorAttachmentFormat = swapchain->format();

    // The ImGui Vulkan backend will create resources for this many frames.
    init.ImageCount = Math::max(state.frames_in_flight, swapchain->min_image_count());

    if(!ImGui_ImplVulkan_Init(&init, null)) {
        die("[rvk] Failed to initialize ImGui vulkan backend!");
    }

    Profile::Time_Point end = Profile::timestamp();
    info("[rvk] Created ImGui vulkan backend in %ms.", Profile::ms(end - start));
}

Commands Vk::make_commands(Queue_Family family) {
    switch(family) {
    case Queue_Family::graphics: return graphics_command_pool->make();
    case Queue_Family::transfer: return transfer_command_pool->make();
    case Queue_Family::compute: return compute_command_pool->make();
    default: RPP_UNREACHABLE;
    }
}

Fence Vk::make_fence() {
    return Fence{device.dup()};
}

void Vk::submit(Commands& cmds, u32 index, Fence& fence) {
    device->submit(cmds, index, fence);
}

void Vk::submit_and_wait(Commands cmds, u32 index) {
    Fence fence{device.dup()};
    device->submit(cmds, index, fence);
    fence.wait();
}

void Vk::wait_idle() {
    vkDeviceWaitIdle(*device);
    for(auto& queue : deletion_queues) {
        queue.clear();
    }
}

void Vk::drop(Finalizer f) {
    deletion_queues[state.frame_index].push(move(f));
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

    if(state.has_imgui) {
        ImGui_ImplVulkan_NewFrame();
        ImGui::NewFrame();
    }
}

void Vk::end_frame(Image_View& output) {

    if(state.has_imgui) ImGui::EndFrame();

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
                           output);

        // Wait for frame available before running the submit; signal frame complete on finish
        frame.wait_for.push(
            Sem_Ref{frame.available, VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT});

        auto signal = Sem_Ref{frame.complete, VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT};
        device->submit(frame.cmds, 0, Slice{&signal, 1}, frame.wait_for.slice(), frame.fence);

        frame.wait_for.clear();
    }

    // Wait for the frame to complete before presenting
    VkSemaphore complete = frame.complete;
    VkSwapchainKHR vk_swapchain = *swapchain;

    VkPresentInfoKHR present_info = {};
    present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    present_info.waitSemaphoreCount = 1;
    present_info.pWaitSemaphores = &complete;
    present_info.swapchainCount = 1;
    present_info.pSwapchains = &vk_swapchain;
    present_info.pImageIndices = &state.swapchain_index;

    // Submit presentation command
    VkResult result = vkQueuePresentKHR(device->queue(Queue_Family::present), &present_info);
    if(result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
        recreate_swapchain();
    } else if(result != VK_SUCCESS) {
        die("[rvk] Failed to present swapchain image: %", describe(result));
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

        sync([&](Commands& cmds) {
            swapchain = Arc<Swapchain, Alloc>::make(cmds, physical_device, device.dup(),
                                                    instance->surface(), state.frames_in_flight);
        });

        compositor = Arc<Compositor, Alloc>::make(device, descriptor_pool, swapchain.dup());

        create_imgui();

        state.resized_last_frame = true;

        Profile::Time_Point end = Profile::timestamp();
        info("[rvk] Recreated swapchain in %ms.", Profile::ms(end - start));
    }
}

Fence make_fence() {
    return singleton->make_fence();
}

Commands make_commands(Queue_Family family) {
    return singleton->make_commands(family);
}

void submit(Commands& cmds, u32 index, Fence& fence) {
    singleton->submit(cmds, index, fence);
}

void submit_and_wait(Commands cmds, u32 index) {
    singleton->submit_and_wait(move(cmds), index);
}

} // namespace impl

bool startup(Config config) {
    if(impl::singleton) {
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

void drop(Finalizer f) {
    impl::singleton->drop(move(f));
}

void imgui() {
    impl::singleton->imgui();
}

void begin_frame() {
    impl::singleton->begin_frame();
}

void end_frame(Image_View& output) {
    impl::singleton->end_frame(output);
}

Opt<Image> make_image(VkExtent3D extent, VkFormat format, VkImageUsageFlags usage) {
    return impl::singleton->device_memory->make(extent, format, usage);
}

} // namespace rvk
