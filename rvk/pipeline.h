
#pragma once

#include <rpp/base.h>
#include <rpp/rc.h>
#include <rpp/variant.h>

#include "fwd.h"

#include "bindings.h"
#include "descriptors.h"

namespace rvk::impl {

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

template<u32 S, typename... Ts>
struct Push_Constants {

    static constexpr VkShaderStageFlagBits stages = static_cast<VkShaderStageFlagBits>(S);

    operator Slice<VkPushConstantRange>() {
        u32 offset = 0;
        for(auto& r : range) {
            r.offset = offset;
            offset += r.size;
        }
        return Slice<VkPushConstantRange>{range};
    }

private:
    static inline Array<VkPushConstantRange, sizeof...(Ts)> range{
        VkPushConstantRange{stages, 0, sizeof(Ts)}...};
};

struct Binding_Table {

    struct Range {
        u32 start = 0;
        u32 count = 0;
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
    Vec<VkStridedDeviceAddressRegionKHR, Mregion<R>> regions(Slice<Range> ranges) {
        Vec<VkStridedDeviceAddressRegionKHR, Mregion<R>> result;
        result.reserve(ranges.length());
        u64 base = buf.gpu_address();
        u64 stride = Math::align(device->sbt_handle_size(), device->sbt_handle_alignment());
        for(auto& r : ranges) {
            result.push(
                VkStridedDeviceAddressRegionKHR{base + r.start * stride, stride, r.count * stride});
        }
        return result;
    }

private:
    explicit Binding_Table(Arc<Device, Alloc> device, VkPipeline pipeline, u32 n_shaders);
    friend struct Pipeline;

    Arc<Device, Alloc> device;
    Buffer buf;
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

    template<typename T>
    void push(Commands& cmds, VkShaderStageFlagBits stages, const T& data) {
        vkCmdPushConstants(cmds, layout, stages, 0, sizeof(T), &data);
    }

    Binding_Table make_table();

    operator VkPipeline() const {
        return pipeline;
    }

private:
    explicit Pipeline(Arc<Device, Alloc> device, Info info);
    friend struct Compositor;
    friend struct Vk;

    Arc<Device, Alloc> device;

    Kind kind = Kind::graphics;
    VkPipeline pipeline = null;
    VkPipelineLayout layout = null;
    u32 n_shaders = 0;
};

} // namespace rvk::impl

RPP_NAMED_ENUM(rvk::impl::Pipeline::Kind, "Pipeline::Kind", graphics, RPP_CASE(graphics),
               RPP_CASE(compute), RPP_CASE(ray_tracing));
