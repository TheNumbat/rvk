
#include <imgui/imgui.h>

#include "commands.h"
#include "device.h"
#include "pipeline.h"
#include "rvk.h"

namespace rvk::impl {

using namespace rpp;

static VkPipelineBindPoint bind_point(Pipeline::Kind kind) {
    switch(kind) {
    case Pipeline::Kind::graphics: return VK_PIPELINE_BIND_POINT_GRAPHICS;
    case Pipeline::Kind::compute: return VK_PIPELINE_BIND_POINT_COMPUTE;
    case Pipeline::Kind::ray_tracing: return VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR;
    default: RPP_UNREACHABLE;
    }
}

Shader::Shader(Arc<Device, Alloc> D, Slice<const u8> source) : device(move(D)) {

    VkShaderModuleCreateInfo mod_info = {
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = source.length(),
        .pCode = reinterpret_cast<const uint32_t*>(source.data()),
    };

    RVK_CHECK(vkCreateShaderModule(*device, &mod_info, null, &shader));
}

Shader::~Shader() {
    if(shader) vkDestroyShaderModule(*device, shader, null);
    shader = null;
}

Shader::Shader(Shader&& src) {
    *this = move(src);
}

Shader& Shader::operator=(Shader&& src) {
    assert(this != &src);
    this->~Shader();
    device = move(src.device);
    shader = src.shader;
    src.shader = null;
    return *this;
}

Opt<Binding_Table> Binding_Table::make(Arc<Device, Alloc> device, Commands& cmds,
                                       Pipeline& pipeline, Mapping mapping) {

    Counts counts = {
        .gen = static_cast<u32>(mapping.gen.length()),
        .miss = static_cast<u32>(mapping.miss.length()),
        .hit = static_cast<u32>(mapping.hit.length()),
        .call = static_cast<u32>(mapping.call.length()),
    };

    u64 handle_size = device->sbt_handle_size();
    u64 handle_aligned = Math::align(handle_size, device->sbt_handle_alignment());

    u64 total_handles = counts.gen + counts.miss + counts.hit + counts.call;
    u64 total_size = total_handles * handle_aligned;

    rvk::Buffer staging;
    {
        if(auto b = rvk::make_staging(total_size); b.ok()) {
            staging = move(*b);
        } else {
            return {};
        }
    }

    Region(R) {
        auto handles = pipeline.shader_group_handles<R>();

        u8* map = staging.map();

        for(auto& bind : mapping.gen) {
            u64 offset = bind * handle_size;
            Libc::memcpy(map, handles.data() + offset, handle_size);
            map += handle_aligned;
        }
        for(auto& bind : mapping.miss) {
            u64 offset = bind * handle_size;
            Libc::memcpy(map, handles.data() + offset, handle_size);
            map += handle_aligned;
        }
        for(auto& bind : mapping.hit) {
            u64 offset = bind * handle_size;
            Libc::memcpy(map, handles.data() + offset, handle_size);
            map += handle_aligned;
        }
        for(auto& bind : mapping.call) {
            u64 offset = bind * handle_size;
            Libc::memcpy(map, handles.data() + offset, handle_size);
            map += handle_aligned;
        }
    }

    rvk::Buffer buf;
    if(auto b = rvk::make_buffer(total_size, VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR |
                                                 VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
                                                 VK_BUFFER_USAGE_TRANSFER_DST_BIT);
       b.ok()) {
        buf = move(*b);
    } else {
        return {};
    }

    buf.move_from(cmds, move(staging));

    return Opt{Binding_Table{move(device), move(buf), counts}};
}

u64 Pipeline::shader_group_handles_size() {
    u64 handle_size = device->sbt_handle_size();
    return n_shaders * handle_size;
}

void Pipeline::shader_group_handles_write(u8* data, u64 length) {
    RVK_CHECK(vkGetRayTracingShaderGroupHandlesKHR(*device, pipeline, 0, n_shaders, length, data));
}

Pipeline::Pipeline(Arc<Device, Alloc> D, Info info) : device(move(D)) {
    Region(R) {

        Vec<VkDescriptorSetLayout, Mregion<R>> layouts(info.descriptor_set_layouts.length());

        for(auto& set : info.descriptor_set_layouts) layouts.push(VkDescriptorSetLayout{*set});

        VkPipelineLayoutCreateInfo layout_info = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
            .setLayoutCount = static_cast<u32>(layouts.length()),
            .pSetLayouts = layouts.data(),
            .pushConstantRangeCount = static_cast<u32>(info.push_constants.length()),
            .pPushConstantRanges = info.push_constants.data(),
        };

        RVK_CHECK(vkCreatePipelineLayout(*device, &layout_info, null, &layout));

        info.info.match(Overload{
            [this](VkGraphicsPipelineCreateInfo& graphics) {
                kind = Kind::graphics;
                auto vk_info = graphics;
                vk_info.layout = layout;
                n_shaders = vk_info.stageCount;
                RVK_CHECK(vkCreateGraphicsPipelines(*device, null, 1, &vk_info, null, &pipeline));
            },
            [this](VkComputePipelineCreateInfo& compute) {
                kind = Kind::compute;
                auto vk_info = compute;
                vk_info.layout = layout;
                n_shaders = 1;
                RVK_CHECK(vkCreateComputePipelines(*device, null, 1, &vk_info, null, &pipeline));
            },
            [this](VkRayTracingPipelineCreateInfoKHR& ray_tracing) {
                kind = Kind::ray_tracing;
                auto vk_info = ray_tracing;
                vk_info.layout = layout;
                n_shaders = vk_info.groupCount;
                RVK_CHECK(vkCreateRayTracingPipelinesKHR(*device, null, null, 1, &vk_info, null,
                                                         &pipeline));
            },
        });
    }
}

Pipeline::~Pipeline() {
    if(layout) vkDestroyPipelineLayout(*device, layout, null);
    if(pipeline) vkDestroyPipeline(*device, pipeline, null);
    layout = null;
    pipeline = null;
}

Pipeline::Pipeline(Pipeline&& src) {
    *this = move(src);
}

Pipeline& Pipeline::operator=(Pipeline&& src) {
    assert(this != &src);
    this->~Pipeline();
    device = move(src.device);
    kind = src.kind;
    layout = src.layout;
    src.layout = null;
    pipeline = src.pipeline;
    src.pipeline = null;
    n_shaders = src.n_shaders;
    src.n_shaders = 0;
    return *this;
}

void Pipeline::bind(Commands& cmds) {
    assert(pipeline);
    vkCmdBindPipeline(cmds, bind_point(kind), pipeline);
}

void Pipeline::bind_set(Commands& cmds, Descriptor_Set& set, u32 set_index) {
    assert(pipeline);
    VkDescriptorSet s = set.get(frame());
    vkCmdBindDescriptorSets(cmds, bind_point(kind), layout, set_index, 1, &s, 0, null);
}

void Pipeline::bind_set(Commands& cmds, Descriptor_Set& set, u32 set_index, u32 frame_slot) {
    assert(pipeline);
    VkDescriptorSet s = set.get(frame_slot);
    vkCmdBindDescriptorSets(cmds, bind_point(kind), layout, set_index, 1, &s, 0, null);
}

} // namespace rvk::impl