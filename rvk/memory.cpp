
#include <imgui/imgui.h>

#include "commands.h"
#include "memory.h"

namespace rvk::impl {

using namespace rpp;

Device_Memory::Device_Memory(Arc<Physical_Device, Alloc>& physical_device, Arc<Device, Alloc> D,
                             Heap location, u64 heap_size)
    : device(move(D)), location(location), allocator(heap_size) {

    VkMemoryAllocateFlagsInfo flags = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO,
        .flags = location == Heap::device ? VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT
                                          : VkMemoryAllocateFlags{},
    };

    VkMemoryAllocateInfo info = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .pNext = &flags,
        .allocationSize = heap_size,
        .memoryTypeIndex = device->heap_index(location),
    };

    RVK_CHECK(vkAllocateMemory(*device, &info, null, &device_memory));

    vkSetDeviceMemoryPriorityEXT(*device, device_memory, 1.0f);

    buffer_image_granularity =
        physical_device->properties().device.properties.limits.bufferImageGranularity;

    if(location == Heap::host) {
        RVK_CHECK(vkMapMemory(*device, device_memory, 0, VK_WHOLE_SIZE, 0,
                              reinterpret_cast<void**>(&persistent_map)));
    }

    info("[rvk] Allocated % heap of size %mb.", location, heap_size / Math::MB(1));
}

Device_Memory::~Device_Memory() {
    if(persistent_map) {
        vkUnmapMemory(*device, device_memory);
    }
    if(device_memory) {
        vkFreeMemory(*device, device_memory, null);
        allocator.statistics().assert_clear();
        info("[rvk] Freed % heap.", location);
    }
    persistent_map = null;
    device_memory = null;
}

void Device_Memory::imgui() {
    using namespace ImGui;
    auto stat = stats();
    Text("Alloc: %lumb | Free: %lumb | High: %lumb", stat.allocated_size / Math::MB(1),
         stat.free_size / Math::MB(1), stat.high_water / Math::MB(1));
    Text("Alloc Blocks: %lu | Free Blocks: %lu", stat.allocated_blocks, stat.free_blocks);
    Text("Capacity: %lumb", stat.total_capacity / Math::MB(1));
}

typename Heap_Allocator::Stats Device_Memory::stats() {
    return allocator.statistics();
}

void Device_Memory::release(Heap_Allocator::Range address) {
    assert(address);
    allocator.free(address);
}

Opt<Image> Device_Memory::make(VkExtent3D extent, VkFormat format, VkImageUsageFlags usage) {

    VkImage image = null;

    Array<u32, 3> indices{device->queue_index(Queue_Family::graphics),
                          device->queue_index(Queue_Family::compute),
                          device->queue_index(Queue_Family::transfer)};

    VkImageCreateInfo info = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .pNext = null,
        .flags = 0,
        .imageType = VK_IMAGE_TYPE_2D,
        .format = format,
        .extent = extent,
        .mipLevels = 1,
        .arrayLayers = 1,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .tiling = VK_IMAGE_TILING_OPTIMAL,
        .usage = usage,
        .sharingMode = VK_SHARING_MODE_CONCURRENT,
        .queueFamilyIndexCount = static_cast<u32>(indices.length()),
        .pQueueFamilyIndices = indices.data(),
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
    };

    RVK_CHECK(vkCreateImage(*device, &info, null, &image));

    VkImageMemoryRequirementsInfo2 image_requirements = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_REQUIREMENTS_INFO_2,
        .image = image,
    };

    VkMemoryRequirements2 memory_requirements = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_REQUIREMENTS_2,
    };

    vkGetImageMemoryRequirements2(*device, &image_requirements, &memory_requirements);

    auto address = allocator.allocate(
        memory_requirements.memoryRequirements.size,
        Math::max(memory_requirements.memoryRequirements.alignment, buffer_image_granularity));

    if(!address.ok()) {
        vkDestroyImage(*device, image, null);
        return {};
    }

    VkBindImageMemoryInfo bind = {
        .sType = VK_STRUCTURE_TYPE_BIND_IMAGE_MEMORY_INFO,
        .pNext = null,
        .image = image,
        .memory = device_memory,
        .memoryOffset = (*address)->offset,
    };

    RVK_CHECK(vkBindImageMemory2(*device, 1, &bind));

    return Opt{Image{Arc<Device_Memory, Alloc>::from_this(this), *address, image, format, extent}};
}

Opt<Buffer> Device_Memory::make(u64 size, VkBufferUsageFlags usage) {

    VkBuffer buffer = null;

    Array<u32, 3> indices{device->queue_index(Queue_Family::graphics),
                          device->queue_index(Queue_Family::compute),
                          device->queue_index(Queue_Family::transfer)};

    VkBufferCreateInfo info = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = size,
        .usage = usage,
        .sharingMode = VK_SHARING_MODE_CONCURRENT,
        .queueFamilyIndexCount = static_cast<u32>(indices.length()),
        .pQueueFamilyIndices = indices.data(),
    };

    RVK_CHECK(vkCreateBuffer(*device, &info, null, &buffer));

    VkBufferMemoryRequirementsInfo2 buffer_requirements = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_REQUIREMENTS_2,
        .buffer = buffer,
    };

    VkMemoryRequirements2 memory_requirements = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_REQUIREMENTS_2,
    };

    vkGetBufferMemoryRequirements2(*device, &buffer_requirements, &memory_requirements);

    auto address = allocator.allocate(
        memory_requirements.memoryRequirements.size,
        Math::max(memory_requirements.memoryRequirements.alignment, buffer_image_granularity));
    if(!address.ok()) {
        vkDestroyBuffer(*device, buffer, null);
        return {};
    }

    VkBindBufferMemoryInfo bind = {
        .sType = VK_STRUCTURE_TYPE_BIND_BUFFER_MEMORY_INFO,
        .buffer = buffer,
        .memory = device_memory,
        .memoryOffset = (*address)->offset,
    };

    RVK_CHECK(vkBindBufferMemory2(*device, 1, &bind));

    return Opt<Buffer>{Buffer{Arc<Device_Memory, Alloc>::from_this(this), *address, buffer, size}};
}

Image::Image(Arc<Device_Memory, Alloc> memory, Heap_Allocator::Range address, VkImage image,
             VkFormat format, VkExtent3D extent)
    : memory(move(memory)), image(image), format_(format), extent_(extent), address(address){};

Image::~Image() {
    if(image) {
        vkDestroyImage(*memory->device, image, null);
        memory->release(address);
    }
    image = null;
    address = null;
    extent_ = {};
    format_ = VK_FORMAT_UNDEFINED;
}

Image::Image(Image&& src) {
    *this = move(src);
}

Image& Image::operator=(Image&& src) {
    assert(this != &src);
    this->~Image();
    memory = move(src.memory);
    image = src.image;
    src.image = null;
    format_ = src.format_;
    src.format_ = VK_FORMAT_UNDEFINED;
    address = src.address;
    src.address = null;
    extent_ = src.extent_;
    src.extent_ = {};
    return *this;
}

Image_View Image::view(VkImageAspectFlags aspect) {
    return Image_View{*this, aspect};
}

void Image::setup(Commands& commands, VkImageLayout layout) {
    assert(image);
    transition(commands, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_UNDEFINED, layout,
               VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT,
               VK_ACCESS_2_NONE, VK_ACCESS_2_NONE);
}

void Image::from_buffer(Commands& commands, Buffer buffer) {

    VkBufferImageCopy2 copy = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_IMAGE_COPY_2,
        .pNext = null,
        .bufferOffset = 0,
        .bufferRowLength = 0,
        .bufferImageHeight = 0,
        .imageSubresource =
            {
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .mipLevel = 0,
                .baseArrayLayer = 0,
                .layerCount = 1,
            },
        .imageOffset = {0, 0, 0},
        .imageExtent = extent_,
    };

    VkCopyBufferToImageInfo2 copy_info = {
        .sType = VK_STRUCTURE_TYPE_COPY_BUFFER_TO_IMAGE_INFO_2,
        .pNext = null,
        .srcBuffer = buffer,
        .dstImage = image,
        .dstImageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        .regionCount = 1,
        .pRegions = &copy,
    };

    vkCmdCopyBufferToImage2(commands, &copy_info);

    commands.attach(move(buffer));
}

void Image::transition(Commands& commands, VkImageAspectFlags aspect, VkImageLayout src_layout,
                       VkImageLayout dst_layout, VkPipelineStageFlags2 src_stage,
                       VkPipelineStageFlags2 dst_stage, VkAccessFlags2 src_access,
                       VkAccessFlags2 dst_access) {
    assert(image);

    VkImageMemoryBarrier2 barrier = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
        .srcStageMask = src_stage,
        .srcAccessMask = src_access,
        .dstStageMask = dst_stage,
        .dstAccessMask = dst_access,
        .oldLayout = src_layout,
        .newLayout = dst_layout,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = image,
        .subresourceRange =
            {
                .aspectMask = aspect,
                .baseMipLevel = 0,
                .levelCount = 1,
                .baseArrayLayer = 0,
                .layerCount = 1,
            },
    };

    VkDependencyInfo dependency = {
        .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
        .dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT,
        .imageMemoryBarrierCount = 1,
        .pImageMemoryBarriers = &barrier,
    };

    vkCmdPipelineBarrier2(commands, &dependency);
}

Image_View::Image_View(Image& image, VkImageAspectFlags aspect)
    : device(image.memory->device.dup()), aspect_mask(aspect) {

    VkImageViewCreateInfo view_info = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image = image,
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .format = image.format(),
        .components =
            {
                .r = VK_COMPONENT_SWIZZLE_R,
                .g = VK_COMPONENT_SWIZZLE_G,
                .b = VK_COMPONENT_SWIZZLE_B,
                .a = VK_COMPONENT_SWIZZLE_A,
            },
        .subresourceRange =
            {
                .aspectMask = aspect,
                .baseMipLevel = 0,
                .levelCount = 1,
                .baseArrayLayer = 0,
                .layerCount = 1,
            },
    };

    RVK_CHECK(vkCreateImageView(*device, &view_info, null, &view));
}

Image_View::~Image_View() {
    if(view) vkDestroyImageView(*device, view, null);
    view = null;
}

Image_View::Image_View(Image_View&& src) {
    *this = move(src);
}

Image_View& Image_View::operator=(Image_View&& src) {
    assert(this != &src);
    this->~Image_View();
    device = move(src.device);
    view = src.view;
    src.view = null;
    aspect_mask = src.aspect_mask;
    src.aspect_mask = 0;
    return *this;
}

Sampler::Sampler(Arc<Device, Alloc> D, VkFilter min, VkFilter mag) : device(move(D)) {

    VkSamplerCreateInfo sample_info = {
        .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
        .magFilter = mag,
        .minFilter = min,
        .mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR,
        .addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT,
        .addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT,
        .addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT,
        .minLod = 0.0f,
        .maxLod = VK_LOD_CLAMP_NONE,
    };

    RVK_CHECK(vkCreateSampler(*device, &sample_info, null, &sampler));
}

Sampler::~Sampler() {
    if(sampler) vkDestroySampler(*device, sampler, null);
    sampler = null;
}

Sampler::Sampler(Sampler&& src) {
    *this = move(src);
}

Sampler& Sampler::operator=(Sampler&& src) {
    assert(this != &src);
    this->~Sampler();
    device = move(src.device);
    sampler = src.sampler;
    src.sampler = null;
    return *this;
}

Buffer::Buffer(Arc<Device_Memory, Alloc> memory, Heap_Allocator::Range address, VkBuffer buffer,
               u64 len)
    : memory(move(memory)), buffer(buffer), len(len), address(address) {
}

Buffer::~Buffer() {
    if(buffer) {
        vkDestroyBuffer(*memory->device, buffer, null);
        memory->release(address);
    }
    buffer = null;
    address = null;
    len = 0;
}

Buffer::Buffer(Buffer&& src) {
    *this = move(src);
}

Buffer& Buffer::operator=(Buffer&& src) {
    assert(this != &src);
    this->~Buffer();
    memory = move(src.memory);
    buffer = src.buffer;
    src.buffer = null;
    address = src.address;
    src.address = null;
    len = src.len;
    src.len = 0;
    return *this;
}

u64 Buffer::gpu_address() const {
    if(!buffer) return 0;
    VkBufferDeviceAddressInfo info = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
        .buffer = buffer,
    };
    return vkGetBufferDeviceAddress(*memory->device, &info);
}

u8* Buffer::map() {
    if(buffer) {
        if(memory->persistent_map) {
            return memory->persistent_map + address->offset;
        }
    }
    return null;
}

void Buffer::write(Slice<u8> data, u64 offset) {
    assert(buffer);
    assert(data.length() + offset <= len);

    Libc::memcpy(map() + offset, data.data(), data.length());
}

void Buffer::copy_from(Commands& commands, Buffer& from) {
    assert(buffer);
    assert(from.length() <= len);

    VkBufferCopy2 region = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_COPY_2,
        .srcOffset = 0,
        .dstOffset = 0,
        .size = from.length(),
    };

    VkCopyBufferInfo2 info = {
        .sType = VK_STRUCTURE_TYPE_COPY_BUFFER_INFO_2,
        .srcBuffer = from,
        .dstBuffer = buffer,
        .regionCount = 1,
        .pRegions = &region,
    };

    vkCmdCopyBuffer2(commands, &info);
}

void Buffer::copy_from(Commands& commands, Buffer& src, u64 src_offset, u64 dst_offset, u64 size) {
    assert(buffer);
    assert(dst_offset + size <= len);

    VkBufferCopy2 region = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_COPY_2,
        .srcOffset = src_offset,
        .dstOffset = dst_offset,
        .size = size,
    };

    VkCopyBufferInfo2 info = {
        .sType = VK_STRUCTURE_TYPE_COPY_BUFFER_INFO_2,
        .srcBuffer = src,
        .dstBuffer = buffer,
        .regionCount = 1,
        .pRegions = &region,
    };

    vkCmdCopyBuffer2(commands, &info);
}

void Buffer::move_from(Commands& commands, Buffer from) {
    assert(buffer);
    copy_from(commands, from);
    commands.attach(move(from));
}

} // namespace rvk::impl
