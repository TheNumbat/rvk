
#include <imgui/imgui.h>

#include "descriptors.h"
#include "device.h"

namespace rvk::impl {

using namespace rpp;

Descriptor_Pool::Descriptor_Pool(Arc<Device, Alloc> D, u32 bindings_per_type, bool ray_tracing)
    : device(move(D)) {

    Profile::Time_Point start = Profile::timestamp();

    Region(R) {
        Vec<VkDescriptorPoolSize, Mregion<R>> sizes{
            VkDescriptorPoolSize{VK_DESCRIPTOR_TYPE_SAMPLER, bindings_per_type},
            VkDescriptorPoolSize{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, bindings_per_type},
            VkDescriptorPoolSize{VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, bindings_per_type},
            VkDescriptorPoolSize{VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, bindings_per_type},
            VkDescriptorPoolSize{VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, bindings_per_type},
            VkDescriptorPoolSize{VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, bindings_per_type},
            VkDescriptorPoolSize{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, bindings_per_type},
            VkDescriptorPoolSize{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, bindings_per_type},
            VkDescriptorPoolSize{VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, bindings_per_type}};

        if(ray_tracing)
            sizes.push(VkDescriptorPoolSize{VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR,
                                            bindings_per_type});

        VkDescriptorPoolCreateInfo pool_info = {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
            .flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT,
            .maxSets = static_cast<u32>(sizes.length()) * bindings_per_type,
            .poolSizeCount = static_cast<u32>(sizes.length()),
            .pPoolSizes = sizes.data(),
        };

        Thread::Lock lock(mutex);
        RVK_CHECK(vkCreateDescriptorPool(*device, &pool_info, null, &pool));
    }

    Profile::Time_Point end = Profile::timestamp();
    info("[rvk] Created descriptor pool in %ms.", Profile::ms(end - start));
}

Descriptor_Pool::~Descriptor_Pool() {
    Thread::Lock lock(mutex);
    if(pool) {
        vkDestroyDescriptorPool(*device, pool, null);
        info("[rvk] Destroyed descriptor pool.");
    }
    pool = null;
}

void Descriptor_Pool::release(Descriptor_Set& set) {
    Thread::Lock lock(mutex);
    vkFreeDescriptorSets(*device, pool, static_cast<u32>(set.sets.length()), set.sets.data());
}

Descriptor_Set Descriptor_Pool::make(Descriptor_Set_Layout& layout, u64 frames_in_flight,
                                     u32 variable_count) {

    auto sets = Vec<VkDescriptorSet, Alloc>::make(frames_in_flight);

    Region(R) {
        Vec<u32, Mregion<R>> counts(frames_in_flight);
        Vec<VkDescriptorSetLayout, Mregion<R>> layouts(frames_in_flight);

        for(u64 i = 0; i < frames_in_flight; ++i) {
            counts.push(variable_count);
            layouts.push(layout);
        }

        VkDescriptorSetVariableDescriptorCountAllocateInfo variable_count_info = {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_VARIABLE_DESCRIPTOR_COUNT_ALLOCATE_INFO,
            .descriptorSetCount = static_cast<u32>(frames_in_flight),
            .pDescriptorCounts = counts.data(),
        };

        VkDescriptorSetAllocateInfo alloc_info = {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
            .pNext = &variable_count_info,
            .descriptorPool = pool,
            .descriptorSetCount = static_cast<u32>(frames_in_flight),
            .pSetLayouts = layouts.data(),
        };

        {
            Thread::Lock lock(mutex);
            RVK_CHECK(vkAllocateDescriptorSets(*device, &alloc_info, sets.data()));
        }
    }

    return Descriptor_Set{Arc<Descriptor_Pool, Alloc>::from_this(this), move(sets)};
}

Descriptor_Set::Descriptor_Set(Arc<Descriptor_Pool, Alloc> pool, Vec<VkDescriptorSet, Alloc> sets)
    : pool(move(pool)), sets(move(sets)) {
}

Descriptor_Set::~Descriptor_Set() {
    if(sets.length()) {
        pool->release(*this);
    }
}

void Descriptor_Set::write(u64 frame_index, Slice<VkWriteDescriptorSet> writes) {
    assert(frame_index < sets.length());

    Region(R) {
        Vec<VkWriteDescriptorSet, Mregion<R>> vk_writes;
        for(auto& write : writes) {
            if(write.descriptorCount == 0) continue;
            vk_writes.push(write).dstSet = sets[frame_index];
        }
        vkUpdateDescriptorSets(*pool->device, static_cast<u32>(vk_writes.length()),
                               vk_writes.data(), 0, null);
    }
}

Descriptor_Set_Layout::Descriptor_Set_Layout(Arc<Device, Alloc> D,
                                             Slice<VkDescriptorSetLayoutBinding> bindings,
                                             Slice<VkDescriptorBindingFlags> flags)
    : device(move(D)) {

    assert(bindings.length() == flags.length() || flags.length() == 0);

    VkDescriptorSetLayoutBindingFlagsCreateInfo flags_info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO,
        .pNext = null,
        .bindingCount = static_cast<u32>(flags.length()),
        .pBindingFlags = flags.data(),
    };

    VkDescriptorSetLayoutCreateInfo info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .pNext = flags.length() ? &flags_info : null,
        .flags = 0,
        .bindingCount = static_cast<u32>(bindings.length()),
        .pBindings = bindings.data(),
    };

    RVK_CHECK(vkCreateDescriptorSetLayout(*device, &info, null, &layout));
}

Descriptor_Set_Layout::~Descriptor_Set_Layout() {
    if(layout) vkDestroyDescriptorSetLayout(*device, layout, null);
    layout = null;
}

Descriptor_Set_Layout::Descriptor_Set_Layout(Descriptor_Set_Layout&& src) {
    *this = move(src);
}

Descriptor_Set_Layout& Descriptor_Set_Layout::operator=(Descriptor_Set_Layout&& src) {
    assert(this != &src);
    this->~Descriptor_Set_Layout();
    device = move(src.device);
    layout = src.layout;
    src.layout = null;
    return *this;
}

} // namespace rvk::impl