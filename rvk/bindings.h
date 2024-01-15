
#pragma once

#include <rpp/base.h>
#include <rpp/tuple.h>

#include "fwd.h"

#include "acceleration.h"
#include "descriptors.h"
#include "memory.h"

namespace rvk {

using namespace rpp;

template<typename B>
concept Binding = requires(B bind, Vec<VkWriteDescriptorSet, Mregion<0>>& writes) {
    Same<VkDescriptorType, decltype(B::type)>;
    Same<VkShaderStageFlags, decltype(B::stages)>;
    { bind.template write<0>(writes) } -> Same<void>;
};

struct Is_Binding {
    template<typename T>
    static constexpr bool value = Binding<T>;
};

namespace Bind {

template<VkShaderStageFlags stages_>
struct Sampler {
    static constexpr VkDescriptorType type = VK_DESCRIPTOR_TYPE_SAMPLER;
    static constexpr VkShaderStageFlags stages = stages_;

    explicit Sampler() {
        info.sampler = null;
    }
    explicit Sampler(Sampler& sampler) {
        info.sampler = sampler;
    }

    VkDescriptorImageInfo info;

    template<Region R>
    void write(Vec<VkWriteDescriptorSet, Mregion<R>>& writes) const {
        VkWriteDescriptorSet write = {};
        write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write.descriptorType = type;
        write.descriptorCount = 1;
        write.pImageInfo = &info;
        writes.push(write);
    }
};

template<VkShaderStageFlags stages_>
struct Image_And_Sampler {
    static constexpr VkDescriptorType type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    static constexpr VkShaderStageFlags stages = stages_;

    explicit Image_And_Sampler() {
        info.sampler = null;
        info.imageView = null;
        info.imageLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    }
    explicit Image_And_Sampler(Image_View& image, rvk::impl::Sampler& sampler) {
        info.sampler = sampler;
        info.imageView = image;
        info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    }

    VkDescriptorImageInfo info;

    template<Region R>
    void write(Vec<VkWriteDescriptorSet, Mregion<R>>& writes) const {
        VkWriteDescriptorSet write = {};
        write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write.descriptorType = type;
        write.descriptorCount = 1;
        write.pImageInfo = &info;
        writes.push(write);
    }
};

template<VkShaderStageFlags stages_>
struct Image_Sampled {
    static constexpr VkDescriptorType type = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
    static constexpr VkShaderStageFlags stages = stages_;

    explicit Image_Sampled() {
        info.imageView = null;
        info.imageLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    }
    explicit Image_Sampled(Image_View& image) {
        info.imageView = image;
        info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    }

    VkDescriptorImageInfo info;

    template<Region R>
    void write(Vec<VkWriteDescriptorSet, Mregion<R>>& writes) const {
        VkWriteDescriptorSet write = {};
        write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write.descriptorType = type;
        write.descriptorCount = 1;
        write.pImageInfo = &info;
        writes.push(write);
    }
};

template<VkShaderStageFlags stages_>
struct Image_Storage {
    static constexpr VkDescriptorType type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    static constexpr VkShaderStageFlags stages = stages_;

    explicit Image_Storage() {
        info.imageView = null;
        info.imageLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    }
    explicit Image_Storage(Image_View& image) {
        info.imageView = image;
        info.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
    }

    VkDescriptorImageInfo info;

    template<Region R>
    void write(Vec<VkWriteDescriptorSet, Mregion<R>>& writes) const {
        VkWriteDescriptorSet write = {};
        write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write.descriptorType = type;
        write.descriptorCount = 1;
        write.pImageInfo = &info;
        writes.push(write);
    }
};

template<VkShaderStageFlags stages_>
struct Buffer_Uniform {
    static constexpr VkDescriptorType type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    static constexpr VkShaderStageFlags stages = stages_;

    explicit Buffer_Uniform() {
        info.buffer = null;
        info.offset = 0;
        info.range = VK_WHOLE_SIZE;
    }
    explicit Buffer_Uniform(Buffer& buffer) {
        info.buffer = buffer;
        info.offset = 0;
        info.range = VK_WHOLE_SIZE;
    }

    VkDescriptorBufferInfo info;

    template<Region R>
    void write(Vec<VkWriteDescriptorSet, Mregion<R>>& writes) const {
        VkWriteDescriptorSet write = {};
        write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write.descriptorType = type;
        write.descriptorCount = 1;
        write.pBufferInfo = &info;
        writes.push(write);
    }
};

template<VkShaderStageFlags stages_>
struct Buffer_Storage {
    static constexpr VkDescriptorType type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    static constexpr VkShaderStageFlags stages = stages_;

    explicit Buffer_Storage() {
        info.buffer = null;
        info.offset = 0;
        info.range = VK_WHOLE_SIZE;
    }
    explicit Buffer_Storage(Buffer& buffer) {
        info.buffer = buffer;
        info.offset = 0;
        info.range = VK_WHOLE_SIZE;
    }

    VkDescriptorBufferInfo info;

    template<Region R>
    void write(Vec<VkWriteDescriptorSet, Mregion<R>>& writes) const {
        VkWriteDescriptorSet write = {};
        write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write.descriptorType = type;
        write.descriptorCount = 1;
        write.pBufferInfo = &info;
        writes.push(write);
    }
};

template<VkShaderStageFlags stages_>
struct TLAS {
    static constexpr VkDescriptorType type = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
    static constexpr VkShaderStageFlags stages = stages_;

    explicit TLAS() {
        info.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR;
        info.accelerationStructureCount = 1;
        info.pAccelerationStructures = &accel;
    }
    explicit TLAS(rvk::impl::TLAS& tlas) {
        accel = tlas;
        info.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR;
        info.accelerationStructureCount = 1;
        info.pAccelerationStructures = &accel;
    }

    VkAccelerationStructureKHR accel = null;
    VkWriteDescriptorSetAccelerationStructureKHR info = {};

    template<Region R>
    void write(Vec<VkWriteDescriptorSet, Mregion<R>>& writes) const {
        VkWriteDescriptorSet write = {};
        write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write.descriptorType = type;
        write.descriptorCount = 1;
        write.pNext = &info;
        writes.push(write);
    }
};

} // namespace Bind

namespace impl {

template<Region R>
struct Make {
    template<Binding B>
    void apply() {
        bindings.push(VkDescriptorSetLayoutBinding{static_cast<u32>(bindings.length()), B::type, 1,
                                                   B::stages, null});
    }
    Vec<VkDescriptorSetLayoutBinding, Mregion<R>>& bindings;
};

template<Region R, Binding... Binds>
struct Write {
    template<typename Index>
    void apply() {
        binds.template get<Index::value>().write(writes);
        writes[Index::value].dstBinding = Index::value;
    }
    Vec<VkWriteDescriptorSet, Mregion<R>>& writes;
    Tuple<Binds...>& binds;
};

struct Binder {
    template<Type_List L>
        requires(Reflect::All<rvk::Is_Binding, L>)
    static Descriptor_Set_Layout make(Arc<rvk::impl::Device, Alloc> device) {
        Region(R) {
            Vec<VkDescriptorSetLayoutBinding, Mregion<R>> bindings(Reflect::List_Length<L>);
            Reflect::Iter<Make<R>, L>::apply(Make<R>{bindings});
            return Descriptor_Set_Layout{move(device), bindings.slice()};
        }
    }

    template<Type_List L, Binding... Binds>
        requires Same<L, List<Binds...>>
    static void write(Descriptor_Set& set, u64 frame_index, Binds&... binds) {
        Region(R) {
            Vec<VkWriteDescriptorSet, Mregion<R>> writes(sizeof...(Binds));

            Tuple<Binds...> tuple{move(binds)...};

            using Indices = Reflect::Enumerate<Binds...>;
            Reflect::Iter<Write<R, Binds...>, Indices>::apply(Write<R, Binds...>{writes, tuple});

            set.write(frame_index, writes.slice());
        }
    }
};

} // namespace impl

} // namespace rvk
