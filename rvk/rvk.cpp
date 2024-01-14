
#include <imgui/imgui.h>

#include "descriptors.h"
#include "device.h"
#include "instance.h"
#include "memory.h"
#include "rvk.h"
#include "swapchain.h"

namespace rvk {

using namespace rpp;

namespace impl {

struct Vk {
    Arc<Instance, Alloc> instance;
    Arc<Debug_Callback, Alloc> debug_callback;
    Arc<Physical_Device, Alloc> physical_device;
    Arc<Device, Alloc> device;
    Arc<Device_Memory, Alloc> host_memory;
    Arc<Device_Memory, Alloc> device_memory;
    Arc<Swapchain, Alloc> swapchain;
    Arc<Descriptor_Pool, Alloc> descriptor_pool;

    VkSurfaceKHR surface;

    explicit Vk(Config config) {

        instance = Arc<Instance, Alloc>::make(move(config.swapchain_extensions),
                                              move(config.layers), config.validation);

        debug_callback = Arc<Debug_Callback, Alloc>::make(instance.dup());

        surface = config.create_surface(*instance);

        physical_device = instance->physical_device(surface, config.ray_tracing);

        device = Arc<Device, Alloc>::make(physical_device.dup(), surface, config.ray_tracing);

        host_memory = Arc<Device_Memory, Alloc>::make(physical_device, device.dup(), Heap::host,
                                                      config.host_heap);

        device_memory = Arc<Device_Memory, Alloc>::make(physical_device, device.dup(), Heap::device,
                                                        device->heap_size(Heap::device) -
                                                            config.device_heap_margin);

        descriptor_pool = Arc<Descriptor_Pool, Alloc>::make(
            device.dup(), config.descriptors_per_type, config.ray_tracing);

        // TODO make this initialize the images
        swapchain = Arc<Swapchain, Alloc>::make(physical_device, device.dup(), surface,
                                                config.frames_in_flight);
    }

    ~Vk() {
        vkDestroySurfaceKHR(*instance, surface, null);
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
    ImGui::DestroyContext();
    impl::singleton.clear();
    info("[rvk] Completed shutdown.");
}

} // namespace rvk
