
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
    using Finalizer = FunctionN<8, void()>;

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
    Vec<Pair<Semaphore, VkShaderStageFlags>, Alloc> wait_for;
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

    Vec<Frame, Alloc> frames;
    Vec<Deletion_Queue, Alloc> deletion_queues;

    explicit Vk(Config config) {

        instance =
            Arc<Instance, Alloc>::make(move(config.swapchain_extensions), move(config.layers),
                                       move(config.create_surface), config.validation);

        debug_callback = Arc<Debug_Callback, Alloc>::make(instance.dup());

        physical_device = instance->physical_device(instance->surface(), config.ray_tracing);

        device = Arc<Device, Alloc>::make(physical_device.dup(), instance->surface(),
                                          config.ray_tracing);

        host_memory = Arc<Device_Memory, Alloc>::make(physical_device, device.dup(), Heap::host,
                                                      config.host_heap);

        device_memory = Arc<Device_Memory, Alloc>::make(physical_device, device.dup(), Heap::device,
                                                        device->heap_size(Heap::device) -
                                                            config.device_heap_margin);

        descriptor_pool = Arc<Descriptor_Pool, Alloc>::make(
            device.dup(), config.descriptors_per_type, config.ray_tracing);

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

        { // Initialize ImGui
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
            init.ImageCount = Math::max(config.frames_in_flight, swapchain->min_image_count());

            if(!ImGui_ImplVulkan_Init(&init, null)) {
                die("[rvk] Failed to initialize ImGui vulkan backend!");
            }

            Profile::Time_Point end = Profile::timestamp();
            info("[rvk] Created ImGui vulkan backend in %ms.", Profile::ms(end - start));
        }
    }

    ~Vk() {
        wait_idle();
        ImGui_ImplVulkan_Shutdown();
    }

    Commands make_commands(Queue_Family family) {
        switch(family) {
        case Queue_Family::graphics: return graphics_command_pool->make();
        case Queue_Family::transfer: return transfer_command_pool->make();
        case Queue_Family::compute: return compute_command_pool->make();
        default: RPP_UNREACHABLE;
        }
    }

    void submit_and_wait(Commands cmds, u32 index) {
        Fence fence{device.dup()};
        device->submit(cmds, index, fence);
        fence.wait();
    }

    void wait_idle() {
        vkDeviceWaitIdle(*device);
        for(auto& queue : deletion_queues) {
            queue.clear();
        }
    }

    template<typename F>
        requires Invocable<F, Commands&>
    auto sync(F&& f, Queue_Family family = Queue_Family::graphics, u32 index = 0)
        -> Invoke_Result<F, Commands&> {
        Commands cmds = make_commands(family);
        if constexpr(Same<Invoke_Result<F, Commands&>, void>) {
            f(cmds);
            cmds.end();
            submit_and_wait(move(cmds), index);
        } else {
            auto result = f(cmds);
            cmds.end();
            submit_and_wait(move(cmds), index);
            return result;
        }
    }
};

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

using ImGui_Alloc = Mallocator<"ImGui">;

static void* imgui_alloc(u64 sz, void*) {
    return ImGui_Alloc::alloc(sz);
}

static void imgui_free(void* mem, void*) {
    ImGui_Alloc::free(mem);
}

static Opt<Vk> singleton;

} // namespace impl

bool startup(Config config) {
    if(impl::singleton) {
        die("[rvk] Already started up!");
    }
    Profile::Time_Point start = Profile::timestamp();

    ImGui::SetAllocatorFunctions(impl::imgui_alloc, impl::imgui_free);
    ImGui::CreateContext();

    impl::singleton.emplace(move(config));

    Profile::Time_Point end = Profile::timestamp();
    info("[rvk] Completed startup in %ms.", Profile::ms(end - start));
    return true;
}

void shutdown() {
    impl::singleton.clear();
    ImGui::DestroyContext();
    info("[rvk] Completed shutdown.");
}

} // namespace rvk
