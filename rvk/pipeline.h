
#pragma once

#include <rpp/base.h>
#include <rpp/rc.h>
#include <rpp/variant.h>

#include "fwd.h"

#include "descriptors.h"

namespace rvk::impl {

using namespace rpp;

struct Shader {

    Shader() = default;
    ~Shader();

    explicit Shader(Arc<Device, Alloc> device, Slice<u8> source);

    Shader(const Shader&) = delete;
    Shader& operator=(const Shader&) = delete;
    Shader(Shader&& src);
    Shader& operator=(Shader&& src);

    operator VkShaderModule() {
        return shader;
    }

private:
    Arc<Device, Alloc> device;
    VkShaderModule shader = null;
};

struct Pipeline {
    enum class Kind : u8 { graphics, compute, ray_tracing };

    struct Config {
        Slice<VkPushConstantRange> push_constants;
        Slice<Descriptor_Set_Layout> descriptor_set_layouts;
        Variant<VkGraphicsPipelineCreateInfo, VkComputePipelineCreateInfo,
                VkRayTracingPipelineCreateInfoKHR>
            info;
    };

    explicit Pipeline(Arc<Device, Alloc> device, Config config);
    ~Pipeline();

    Pipeline(const Pipeline&) = delete;
    Pipeline& operator=(const Pipeline&) = delete;
    Pipeline(Pipeline&& src);
    Pipeline& operator=(Pipeline&& src);

    void bind(Commands& cmds);
    void bind_set(Commands& cmds, Descriptor_Set& set, u64 frame_index, u32 set_index = 0);

    operator VkPipeline() {
        return pipeline;
    }

private:
    Arc<Device, Alloc> device;

    VkPipeline pipeline = null;
    VkPipelineLayout layout = null;
};

} // namespace rvk::impl

RPP_NAMED_ENUM(rvk::impl::Pipeline::Kind, "Pipeline::Kind", graphics, RPP_CASE(graphics),
               RPP_CASE(compute), RPP_CASE(ray_tracing));
