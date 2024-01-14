
#pragma once

#include <rpp/base.h>
#include <rpp/rc.h>
#include <rpp/variant.h>

#include "fwd.h"

#include "descriptors.h"

namespace rvk::impl {

using namespace rpp;

struct Shader {

    explicit Shader(Arc<Device, Alloc> device, Slice<u8> source);
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

struct Pipeline {
    enum class Kind : u8 { graphics, compute, ray_tracing };

    using Create_Info = Variant<VkGraphicsPipelineCreateInfo, VkComputePipelineCreateInfo,
                                VkRayTracingPipelineCreateInfoKHR>;
    struct Config {
        Slice<VkPushConstantRange> push_constants;
        Slice<Descriptor_Set_Layout> descriptor_set_layouts;
        Create_Info info;
    };

    ~Pipeline();

    Pipeline(const Pipeline&) = delete;
    Pipeline& operator=(const Pipeline&) = delete;
    Pipeline(Pipeline&& src);
    Pipeline& operator=(Pipeline&& src);

    void bind(Commands& cmds);
    void bind_set(Commands& cmds, Descriptor_Set& set, u64 frame_index, u32 set_index = 0);

    operator VkPipeline() const {
        return pipeline;
    }

private:
    explicit Pipeline(Arc<Device, Alloc> device, Config config);
    friend Pipeline make_pipeline(Config config);

    Arc<Device, Alloc> device;

    Kind kind = Kind::graphics;
    VkPipeline pipeline = null;
    VkPipelineLayout layout = null;
};

} // namespace rvk::impl

RPP_NAMED_ENUM(rvk::impl::Pipeline::Kind, "Pipeline::Kind", graphics, RPP_CASE(graphics),
               RPP_CASE(compute), RPP_CASE(ray_tracing));
