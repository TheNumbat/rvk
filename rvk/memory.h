
#pragma once

#include <rpp/base.h>
#include <rpp/range_allocator.h>
#include <rpp/rc.h>

#include "fwd.h"

#include "device.h"

namespace rvk::impl {

using namespace rpp;

using Heap_Allocator = Range_Allocator<Alloc, 32, 10>;
using Buffer_Allocator = Range_Allocator<Alloc, 24, 6>;

struct Device_Memory {

    explicit Device_Memory(const Arc<Physical_Device, Alloc>& physical_device,
                           Arc<Device, Alloc> device, Heap location, u64 size);
    ~Device_Memory();

    Device_Memory(const Device_Memory&) = delete;
    Device_Memory& operator=(const Device_Memory&) = delete;
    Device_Memory(Device_Memory&&) = delete;
    Device_Memory& operator=(Device_Memory&&) = delete;

    // Opt<Buffer> allocate(u64 size, VkBufferUsageFlags usage);
    // Opt<Image> allocate(u32 width, u32 height, VkFormat format, VkImageUsageFlags usage);

    Heap_Allocator::Stats stats();

private:
    Arc<Device, Alloc> device;

    VkDeviceMemory device_memory = null;

    Heap location = Heap::device;
    u8* persistent_map = null;
    u64 buffer_image_granularity = 0;
    Heap_Allocator allocator;
};

} // namespace rvk::impl
