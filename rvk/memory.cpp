
#include <imgui/imgui.h>

#include "commands.h"
#include "memory.h"

namespace rvk::impl {

using namespace rpp;

Device_Memory::Device_Memory(const Arc<Physical_Device, Alloc>& physical_device,
                             Arc<Device, Alloc> D, Heap location, u64 heap_size)
    : device(move(D)), location(location), allocator(heap_size) {

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

    VkImageCreateInfo info = {};
    info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    info.imageType = VK_IMAGE_TYPE_2D;
    info.extent = extent;
    info.mipLevels = 1;
    info.arrayLayers = 1;
    info.format = format;
    info.tiling = VK_IMAGE_TILING_OPTIMAL;
    info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    info.samples = VK_SAMPLE_COUNT_1_BIT;
    info.usage = usage;
    info.sharingMode = VK_SHARING_MODE_CONCURRENT;

    Array<u32, 3> indices{device->queue_index(Queue_Family::graphics),
                          device->queue_index(Queue_Family::compute),
                          device->queue_index(Queue_Family::transfer)};

    info.pQueueFamilyIndices = indices.data();
    info.queueFamilyIndexCount = static_cast<u32>(indices.length());

    RVK_CHECK(vkCreateImage(*device, &info, null, &image));

    VkImageMemoryRequirementsInfo2 image_requirements = {};
    image_requirements.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_REQUIREMENTS_INFO_2;
    image_requirements.image = image;

    VkMemoryRequirements2 memory_requirements = {};
    memory_requirements.sType = VK_STRUCTURE_TYPE_MEMORY_REQUIREMENTS_2;

    vkGetImageMemoryRequirements2(*device, &image_requirements, &memory_requirements);

    auto address = allocator.allocate(
        memory_requirements.memoryRequirements.size,
        Math::max(memory_requirements.memoryRequirements.alignment, buffer_image_granularity));

    if(!address) {
        vkDestroyImage(*device, image, null);
        return {};
    }

    VkBindImageMemoryInfo bind = {};
    bind.sType = VK_STRUCTURE_TYPE_BIND_IMAGE_MEMORY_INFO;
    bind.memory = device_memory;
    bind.image = image;
    bind.memoryOffset = (*address)->offset;

    RVK_CHECK(vkBindImageMemory2(*device, 1, &bind));

    return Opt{Image{Arc<Device_Memory, Alloc>::from_this(this), *address, image, format}};
}

Image::Image(Arc<Device_Memory, Alloc> memory, Heap_Allocator::Range address, VkImage image,
             VkFormat format)
    : memory(move(memory)), image(image), format_(format), address(address){};

Image::~Image() {
    if(image) {
        vkDestroyImage(*memory->device, image, null);
        memory->release(address);
    }
    image = null;
    address = null;
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
    return *this;
}

Image_View Image::view(VkImageAspectFlags aspect) {
    return Image_View{*this, aspect};
}

void Image::setup(Commands& commands, VkImageLayout layout) {
    transition(commands, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_UNDEFINED, layout,
               VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT,
               VK_ACCESS_2_NONE, VK_ACCESS_2_NONE);
}

void Image::transition(Commands& commands, VkImageAspectFlags aspect, VkImageLayout src_layout,
                       VkImageLayout dst_layout, VkPipelineStageFlags2 src_stage,
                       VkPipelineStageFlags2 dst_stage, VkAccessFlags2 src_access,
                       VkAccessFlags2 dst_access) {

    VkImageMemoryBarrier2 barrier = {};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
    barrier.oldLayout = src_layout;
    barrier.newLayout = dst_layout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = image;
    barrier.subresourceRange.aspectMask = aspect;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;
    barrier.srcStageMask = src_stage;
    barrier.srcAccessMask = src_access;
    barrier.dstStageMask = dst_stage;
    barrier.dstAccessMask = dst_access;

    VkDependencyInfo dependency = {};
    dependency.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
    dependency.imageMemoryBarrierCount = 1;
    dependency.pImageMemoryBarriers = &barrier;
    dependency.dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

    vkCmdPipelineBarrier2(commands, &dependency);
}

Image_View::Image_View(Image& image, VkImageAspectFlags aspect)
    : device(image.memory->device.dup()), aspect_mask(aspect) {

    VkImageViewCreateInfo view_info = {};
    view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    view_info.image = image;
    view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
    view_info.format = image.format();
    view_info.components.r = VK_COMPONENT_SWIZZLE_R;
    view_info.components.g = VK_COMPONENT_SWIZZLE_G;
    view_info.components.b = VK_COMPONENT_SWIZZLE_B;
    view_info.components.a = VK_COMPONENT_SWIZZLE_A;
    view_info.subresourceRange.aspectMask = aspect;
    view_info.subresourceRange.baseMipLevel = 0;
    view_info.subresourceRange.levelCount = 1;
    view_info.subresourceRange.baseArrayLayer = 0;
    view_info.subresourceRange.layerCount = 1;

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
    view = src.view;
    src.view = null;
    aspect_mask = src.aspect_mask;
    src.aspect_mask = 0;
    return *this;
}

Sampler::Sampler(Arc<Device, Alloc> D, VkFilter min, VkFilter mag) : device(move(D)) {

    VkSamplerCreateInfo sample_info = {};
    sample_info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    sample_info.minFilter = min;
    sample_info.magFilter = mag;
    sample_info.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    sample_info.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    sample_info.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    sample_info.unnormalizedCoordinates = VK_FALSE;
    sample_info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    sample_info.minLod = 0.0f;
    sample_info.maxLod = VK_LOD_CLAMP_NONE;

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

} // namespace rvk::impl
