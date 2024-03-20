
#pragma once

#include <rpp/base.h>
#include <rpp/rc.h>

#include "fwd.h"

namespace rvk::impl {

using namespace rpp;

struct Descriptor_Set_Layout {

    Descriptor_Set_Layout() = default;
    ~Descriptor_Set_Layout();

    Descriptor_Set_Layout(const Descriptor_Set_Layout&) = delete;
    Descriptor_Set_Layout& operator=(const Descriptor_Set_Layout&) = delete;
    Descriptor_Set_Layout(Descriptor_Set_Layout&&);
    Descriptor_Set_Layout& operator=(Descriptor_Set_Layout&&);

    operator VkDescriptorSetLayout() const {
        return layout;
    }

private:
    explicit Descriptor_Set_Layout(Arc<Device, Alloc> device,
                                   Slice<VkDescriptorSetLayoutBinding> bindings,
                                   Slice<VkDescriptorBindingFlags> flags);
    friend struct Vk;

    Arc<Device, Alloc> device;
    VkDescriptorSetLayout layout = null;

    friend struct Compositor;
    friend struct Binder;
    friend struct Descriptor_Pool;
};

struct Descriptor_Set {

    Descriptor_Set() = default;
    ~Descriptor_Set();

    Descriptor_Set(const Descriptor_Set&) = delete;
    Descriptor_Set& operator=(const Descriptor_Set&) = delete;
    Descriptor_Set(Descriptor_Set&& src) = default;
    Descriptor_Set& operator=(Descriptor_Set&& src) = default;

    void write(u64 frame_index, Slice<VkWriteDescriptorSet> writes);

    VkDescriptorSet get(u64 frame_index) {
        return sets[frame_index];
    }

private:
    explicit Descriptor_Set(Arc<Descriptor_Pool, Alloc> pool, Vec<VkDescriptorSet, Alloc> sets);

    Arc<Descriptor_Pool, Alloc> pool;
    Vec<VkDescriptorSet, Alloc> sets;

    friend struct Descriptor_Pool;
};

struct Descriptor_Pool {

    Descriptor_Pool() = default;
    ~Descriptor_Pool();

    Descriptor_Pool(const Descriptor_Pool&) = delete;
    Descriptor_Pool& operator=(const Descriptor_Pool&) = delete;
    Descriptor_Pool(Descriptor_Pool&&) = delete;
    Descriptor_Pool& operator=(Descriptor_Pool&&) = delete;

    operator VkDescriptorPool() const {
        return pool;
    }

    Descriptor_Set make(Descriptor_Set_Layout& layout, u64 frames_in_flight, u32 variable_count);

private:
    explicit Descriptor_Pool(Arc<Device, Alloc> device, u32 bindings_per_type, bool ray_tracing);
    friend struct Arc<Descriptor_Pool, Alloc>;

    void release(Descriptor_Set& set);

    Arc<Device, Alloc> device;
    VkDescriptorPool pool = null;
    Thread::Mutex mutex;

    friend struct Descriptor_Set;
};

} // namespace rvk::impl
