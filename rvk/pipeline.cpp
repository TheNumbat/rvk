
#include <imgui/imgui.h>

#include "commands.h"
#include "device.h"
#include "pipeline.h"

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

Shader::Shader(Arc<Device, Alloc> D, Slice<u8> source) : device(move(D)) {

    VkShaderModuleCreateInfo mod_info = {};
    mod_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    mod_info.codeSize = source.length();
    mod_info.pCode = reinterpret_cast<const uint32_t*>(source.data());

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

Pipeline::Pipeline(Arc<Device, Alloc> D, Info info) : device(move(D)) {
    Region(R) {

        Vec<VkDescriptorSetLayout, Mregion<R>> layouts(info.descriptor_set_layouts.length());
        Vec<VkPushConstantRange, Mregion<R>> push_constants(info.push_constants.length());

        for(auto& set : info.descriptor_set_layouts) layouts.push(VkDescriptorSetLayout{set});
        for(auto& pc : info.push_constants) push_constants.push(pc);

        VkPipelineLayoutCreateInfo layout_info = {};
        layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        layout_info.setLayoutCount = static_cast<u32>(layouts.length());
        layout_info.pSetLayouts = layouts.data();
        layout_info.pushConstantRangeCount = static_cast<u32>(push_constants.length());
        layout_info.pPushConstantRanges = push_constants.data();

        RVK_CHECK(vkCreatePipelineLayout(*device, &layout_info, null, &layout));

        info.info.match(Overload{
            [this](VkGraphicsPipelineCreateInfo& graphics) {
                kind = Kind::graphics;
                auto vk_info = graphics;
                vk_info.layout = layout;
                RVK_CHECK(vkCreateGraphicsPipelines(*device, null, 1, &vk_info, null, &pipeline));
            },
            [this](VkComputePipelineCreateInfo& compute) {
                kind = Kind::compute;
                auto vk_info = compute;
                vk_info.layout = layout;
                RVK_CHECK(vkCreateComputePipelines(*device, null, 1, &vk_info, null, &pipeline));
            },
            [this](VkRayTracingPipelineCreateInfoKHR& ray_tracing) {
                kind = Kind::ray_tracing;
                auto vk_info = ray_tracing;
                vk_info.layout = layout;
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
    return *this;
}

void Pipeline::bind(Commands& cmds) {
    vkCmdBindPipeline(cmds, bind_point(kind), pipeline);
}

void Pipeline::bind_set(Commands& cmds, Descriptor_Set& set, u64 frame_index, u32 set_index) {
    VkDescriptorSet s = set.get(frame_index);
    vkCmdBindDescriptorSets(cmds, bind_point(kind), layout, set_index, 1, &s, 0, null);
}

} // namespace rvk::impl