
#pragma once

#include <rpp/base.h>
#include <rpp/rc.h>
#include <rpp/variant.h>

#include "fwd.h"

#include "bindings.h"
#include "descriptors.h"

namespace rvk {

template<typename P>
concept Push_Constant = requires {
    typename P::T;
    Same<decltype(P::stages), VkShaderStageFlagBits>;
    Same<decltype(P::range), VkPushConstantRange>;
};

namespace impl {

using namespace rpp;

struct Shader {

    explicit Shader(Arc<Device, Alloc> device, Slice<u8> source);

    Shader() = default;
    ~Shader();

    Shader(const Shader&) = delete;
    Shader& operator=(const Shader&) = delete;
    Shader(Shader&& src);
    Shader& operator=(Shader&& src);

    operator VkShaderModule() const {
        return shader;
    }

private:
    Arc<Device, Alloc> device;
    VkShaderModule shader = null;
};

template<typename D, u32 S, u32 O = 0>
struct Push {
    using T = D;
    static constexpr VkShaderStageFlagBits stages = static_cast<VkShaderStageFlagBits>(S);
    static constexpr VkPushConstantRange range{stages, O, sizeof(D)};

    operator VkPushConstantRange() const {
        return range;
    }
};

struct Binding_Table {

    struct Counts {
        u32 gen = 0;
        u32 miss = 0;
        u32 hit = 0;
        u32 call = 0;
    };

    struct Mapping {
        Slice<u32> gen;
        Slice<u32> miss;
        Slice<u32> hit;
        Slice<u32> call;
    };

    Binding_Table() = default;
    ~Binding_Table() = default;

    Binding_Table(const Binding_Table&) = delete;
    Binding_Table& operator=(const Binding_Table&) = delete;
    Binding_Table(Binding_Table&& src) = default;
    Binding_Table& operator=(Binding_Table&& src) = default;

    Buffer& buffer() {
        return buf;
    }

    template<Region R>
    Vec<VkStridedDeviceAddressRegionKHR, Mregion<R>> regions() {
        Vec<VkStridedDeviceAddressRegionKHR, Mregion<R>> result(4);
        u64 base = buf.gpu_address();
        u64 stride = Math::align(device->sbt_handle_size(), device->sbt_handle_alignment());
        result.push(VkStridedDeviceAddressRegionKHR{base, stride, counts.gen * stride});
        result.push(VkStridedDeviceAddressRegionKHR{base + (counts.gen * stride), stride,
                                                    counts.miss * stride});
        result.push(VkStridedDeviceAddressRegionKHR{base + (counts.gen + counts.miss) * stride,
                                                    stride, counts.hit * stride});
        result.push(VkStridedDeviceAddressRegionKHR{
            base + (counts.gen + counts.miss + counts.hit) * stride, stride, counts.call * stride});
        return result;
    }

private:
    friend struct Vk;

    explicit Binding_Table(Arc<Device, Alloc> device, Buffer buf, Counts counts)
        : device(move(device)), buf(move(buf)), counts(counts) {
    }

    static Opt<Binding_Table> make(Arc<Device, Alloc> device, Commands& cmds, Pipeline& pipeline,
                                   Mapping mapping);

    Arc<Device, Alloc> device;
    Buffer buf;
    Counts counts;
};

struct Pipeline {
    enum class Kind : u8 { graphics, compute, ray_tracing };

    using VkCreateInfo = Variant<VkGraphicsPipelineCreateInfo, VkComputePipelineCreateInfo,
                                 VkRayTracingPipelineCreateInfoKHR>;

    struct Info {
        Slice<VkPushConstantRange> push_constants;
        Slice<Ref<Descriptor_Set_Layout>> descriptor_set_layouts;
        VkCreateInfo info;
    };

    Pipeline() = default;
    ~Pipeline();

    Pipeline(const Pipeline&) = delete;
    Pipeline& operator=(const Pipeline&) = delete;
    Pipeline(Pipeline&& src);
    Pipeline& operator=(Pipeline&& src);

    void bind(Commands& cmds);
    void bind_set(Commands& cmds, Descriptor_Set& set, u32 set_index = 0);

    template<Push_Constant P>
    void push(Commands& cmds, const typename P::T& data) {
        vkCmdPushConstants(cmds, layout, P::stages, P::range.offset, P::range.size, &data);
    }

    template<Region R>
    Vec<u8, Mregion<R>> shader_group_handles() {
        auto data = Vec<u8, Mregion<R>>::make(shader_group_handles_size());
        shader_group_handles_write(data.data(), data.length());
        return data;
    }

    operator VkPipeline() const {
        return pipeline;
    }

private:
    explicit Pipeline(Arc<Device, Alloc> device, Info info);
    friend struct Compositor;
    friend struct Vk;

    u64 shader_group_handles_size();
    void shader_group_handles_write(u8* data, u64 length);

    Arc<Device, Alloc> device;

    Kind kind = Kind::graphics;
    VkPipeline pipeline = null;
    VkPipelineLayout layout = null;
    u32 n_shaders = 0;
};

} // namespace impl
} // namespace rvk

RPP_NAMED_ENUM(rvk::impl::Pipeline::Kind, "Pipeline::Kind", graphics, RPP_CASE(graphics),
               RPP_CASE(compute), RPP_CASE(ray_tracing));
