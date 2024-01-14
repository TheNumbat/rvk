
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

    ~Device_Memory();

    Device_Memory(const Device_Memory&) = delete;
    Device_Memory& operator=(const Device_Memory&) = delete;
    Device_Memory(Device_Memory&&) = delete;
    Device_Memory& operator=(Device_Memory&&) = delete;

    operator VkDeviceMemory() {
        return device_memory;
    }

    Heap_Allocator::Stats stats();

    Opt<Image> make(VkExtent3D extent, VkFormat format, VkImageUsageFlags usage);

private:
    explicit Device_Memory(const Arc<Physical_Device, Alloc>& physical_device,
                           Arc<Device, Alloc> device, Heap location, u64 size);
    friend struct Arc<Device_Memory, Alloc>;

    void release(Heap_Allocator::Range address);

    Arc<Device, Alloc> device;

    VkDeviceMemory device_memory = null;

    Heap location = Heap::device;
    u8* persistent_map = null;
    u64 buffer_image_granularity = 0;
    Heap_Allocator allocator;

    friend struct Image;
    friend struct Image_View;
};

struct Image {

    ~Image();

    Image(const Image& src) = delete;
    Image& operator=(const Image& src) = delete;
    Image(Image&& src);
    Image& operator=(Image&& src);

    operator VkImage() {
        return image;
    }
    VkFormat format() {
        return format_;
    }

private:
    explicit Image(Arc<Device_Memory, Alloc> memory, Heap_Allocator::Range address, VkImage image,
                   VkFormat format)
        : memory(move(memory)), image(image), format_(format), address(address){};

    Arc<Device_Memory, Alloc> memory;

    VkImage image = null;
    VkFormat format_ = VK_FORMAT_UNDEFINED;
    Heap_Allocator::Range address = null;

    friend struct Device_Memory;
    friend struct Image_View;
};

struct Image_View {

    explicit Image_View(Image& image, VkImageAspectFlags aspect);
    ~Image_View();

    Image_View(const Image_View& src) = delete;
    Image_View& operator=(const Image_View& src) = delete;
    Image_View(Image_View&& src);
    Image_View& operator=(Image_View&& src);

    operator VkImageView() {
        return view;
    }
    VkImageAspectFlags aspect() {
        return aspect_mask;
    }

private:
    Arc<Device, Alloc> device;

    VkImageView view = null;
    VkImageAspectFlags aspect_mask = 0;
};

} // namespace rvk::impl
