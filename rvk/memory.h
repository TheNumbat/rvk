
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

    void imgui();

    operator VkDeviceMemory() const {
        return device_memory;
    }

    Heap_Allocator::Stats stats();

    Opt<Buffer> make(u64 size, VkBufferUsageFlags usage);
    Opt<Image> make(VkExtent3D extent, VkFormat format, VkImageUsageFlags usage);

private:
    explicit Device_Memory(Arc<Physical_Device, Alloc>& physical_device, Arc<Device, Alloc> device,
                           Heap location, u64 size);
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
    friend struct Buffer;
    friend struct TLAS;
    friend struct BLAS;
};

struct Image {

    Image() = default;
    ~Image();

    Image(const Image& src) = delete;
    Image& operator=(const Image& src) = delete;
    Image(Image&& src);
    Image& operator=(Image&& src);

    operator VkImage() const {
        return image;
    }
    VkFormat format() {
        return format_;
    }

    Image_View view(VkImageAspectFlags aspect);

    void setup(Commands& commands, VkImageLayout layout);
    void transition(Commands& commands, VkImageAspectFlags aspect, VkImageLayout src_layout,
                    VkImageLayout dst_layout, VkPipelineStageFlags2 src_stage,
                    VkPipelineStageFlags2 dst_stage, VkAccessFlags2 src_access,
                    VkAccessFlags2 dst_access);

    void from_buffer(Commands& commands, Buffer buffer);

private:
    explicit Image(Arc<Device_Memory, Alloc> memory, Heap_Allocator::Range address, VkImage image,
                   VkFormat format, VkExtent3D extent);

    Arc<Device_Memory, Alloc> memory;

    VkImage image = null;
    VkFormat format_ = VK_FORMAT_UNDEFINED;
    VkExtent3D extent_ = {};
    Heap_Allocator::Range address = null;

    friend struct Device_Memory;
    friend struct Image_View;
    friend struct Swapchain;
};

struct Image_View {

    explicit Image_View(Image& image, VkImageAspectFlags aspect);

    Image_View() = default;
    ~Image_View();

    Image_View(const Image_View& src) = delete;
    Image_View& operator=(const Image_View& src) = delete;
    Image_View(Image_View&& src);
    Image_View& operator=(Image_View&& src);

    operator VkImageView() const {
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

struct Sampler {

    struct Config {
        VkFilter min = VK_FILTER_LINEAR;
        VkFilter mag = VK_FILTER_LINEAR;
        VkSamplerMipmapMode mip = VK_SAMPLER_MIPMAP_MODE_LINEAR;
        VkSamplerAddressMode u = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        VkSamplerAddressMode v = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        VkSamplerAddressMode w = VK_SAMPLER_ADDRESS_MODE_REPEAT;

        constexpr bool operator==(const rvk::Sampler::Config& b) const {
            return mag == b.mag && min == b.min && mip == b.mip && u == b.u && v == b.v && w == b.w;
        }
    };

    explicit Sampler(Arc<Device, Alloc> device, Config config);

    Sampler() = default;
    ~Sampler();

    Sampler(const Sampler& src) = delete;
    Sampler& operator=(const Sampler& src) = delete;
    Sampler(Sampler&& src);
    Sampler& operator=(Sampler&& src);

    operator VkSampler() const {
        return sampler;
    }

private:
    Arc<Device, Alloc> device;
    VkSampler sampler = null;
};

struct Buffer {

    Buffer() = default;
    ~Buffer();

    Buffer(const Buffer& src) = delete;
    Buffer& operator=(const Buffer& src) = delete;

    Buffer(Buffer&& src);
    Buffer& operator=(Buffer&& src);

    u64 offset() {
        return address ? address->offset : 0;
    }
    u64 length() {
        return len;
    }
    u64 gpu_address() const;

    u8* map();
    void write(Slice<const u8> data, u64 offset = 0);

    void move_from(Commands& commands, Buffer from);
    void copy_from(Commands& commands, Buffer& from);
    void copy_from(Commands& commands, Buffer& from, u64 src_offset, u64 dst_offset, u64 size);

    operator VkBuffer() const {
        return buffer;
    }

private:
    explicit Buffer(Arc<Device_Memory, Alloc> memory, Heap_Allocator::Range address,
                    VkBuffer buffer, u64 len);

    Arc<Device_Memory, Alloc> memory;

    VkBuffer buffer = null;
    u64 len = 0;
    Heap_Allocator::Range address = null;

    friend struct Device_Memory;
};

} // namespace rvk::impl

namespace rpp::Hash {

template<>
struct Hash<rvk::Sampler::Config> {
    static constexpr u64 hash(const rvk::Sampler::Config& config) noexcept {
        return rpp::hash(static_cast<u32>(config.min), static_cast<u32>(config.mag),
                         static_cast<u32>(config.mip), static_cast<u32>(config.u),
                         static_cast<u32>(config.v), static_cast<u32>(config.w));
    }
};

} // namespace rpp::Hash
