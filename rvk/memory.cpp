
#include <imgui/imgui.h>

#include "memory.h"

namespace rvk::impl {

using namespace rpp;

Device_Memory::Device_Memory(const Arc<Physical_Device, Alloc>& physical_device,
                             Arc<Device, Alloc> D, Heap location, u64 heap_size)
    : device(move(D)), allocator(heap_size), location(location) {

    VkMemoryAllocateFlagsInfo flags = {};
    flags.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO;
    flags.flags = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT;

    VkMemoryAllocateInfo info = {};
    info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    info.pNext = &flags;
    info.allocationSize = heap_size;
    info.memoryTypeIndex = device->heap_index(location);

    RVK_CHECK(vkAllocateMemory(*device, &info, null, &device_memory));

    vkSetDeviceMemoryPriorityEXT(*device, device_memory, 1.0f);

    buffer_image_granularity =
        physical_device->properties().device.properties.limits.bufferImageGranularity;

    if(location == Heap::host) {
        RVK_CHECK(vkMapMemory(*device, device_memory, 0, VK_WHOLE_SIZE, 0,
                              reinterpret_cast<void**>(&persistent_map)));
    }
}

Device_Memory::~Device_Memory() {
    if(persistent_map) {
        vkUnmapMemory(*device, device_memory);
    }
    if(device_memory) {
        vkFreeMemory(*device, device_memory, null);
        allocator.statistics().assert_clear();
        info("[rvk] Freed GPU heap.");
    }
    persistent_map = null;
    device_memory = null;
}

typename Heap_Allocator::Stats Device_Memory::stats() {
    return allocator.statistics();
}

} // namespace rvk::impl
