
#include <rpp/base.h>

#include "acceleration.h"
#include "commands.h"
#include "rvk.h"

namespace rvk::impl {

TLAS::TLAS(Arc<Device, Alloc> device, VkAccelerationStructureKHR accel, Buffer buf)
    : device(move(device)), buffer(move(buf)), structure(accel){};

TLAS::~TLAS() {
    if(structure) {
        vkDestroyAccelerationStructureKHR(*device, structure, null);
    }
    structure = null;
}

TLAS::TLAS(TLAS&& src) : device(move(src.device)), buffer(move(src.buffer)) {
    structure = src.structure;
    src.structure = null;
}

TLAS& TLAS::operator=(TLAS&& src) {
    assert(this != &src);
    this->~TLAS();
    device = move(src.device);
    buffer = move(src.buffer);
    structure = src.structure;
    src.structure = null;
    return *this;
}

Opt<TLAS::Buffers> TLAS::make(Arc<Device_Memory, Alloc>& memory, u32 instances) {
    assert(instances);

    VkAccelerationStructureGeometryKHR geom = {
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR,
        .geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR,
        .geometry =
            {.instances = {.sType =
                               VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR,
                           .arrayOfPointers = VK_FALSE}},
        .flags = 0,
    };

    VkAccelerationStructureBuildGeometryInfoKHR build_info = {
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR,
        .type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR,
        .flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR,
        .mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR,
        .geometryCount = 1,
        .pGeometries = &geom,
    };

    VkAccelerationStructureBuildSizesInfoKHR build_sizes = {
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR,
    };

    vkGetAccelerationStructureBuildSizesKHR(*memory->device,
                                            VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
                                            &build_info, &instances, &build_sizes);

    Opt<Buffer> structure_buf =
        memory->make(build_sizes.accelerationStructureSize,
                     VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR |
                         VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT);
    if(!structure_buf.ok()) {
        return {};
    }

    Opt<Buffer> scratch =
        memory->make(build_sizes.buildScratchSize, VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
                                                       VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
    if(!scratch.ok()) {
        return {};
    }

    return Opt{
        Buffers{move(*structure_buf), move(*scratch), build_sizes.accelerationStructureSize}};
}

TLAS TLAS::build(Arc<Device, Alloc> device, Commands& cmds, Buffers buffers, Buffer gpu_instances,
                 Slice<const Instance> cpu_instances) {

    VkAccelerationStructureKHR acceleration_structure = null;

    VkAccelerationStructureCreateInfoKHR create_info = {
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR,
        .buffer = buffers.structure,
        .size = buffers.size,
        .type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR,
    };

    RVK_CHECK(
        vkCreateAccelerationStructureKHR(*device, &create_info, null, &acceleration_structure));

    // Build

    VkAccelerationStructureGeometryKHR geom = {
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR,
        .geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR,
        .geometry =
            {.instances = {.sType =
                               VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR,
                           .arrayOfPointers = VK_FALSE,
                           .data = {.deviceAddress = gpu_instances.gpu_address()}}},
        .flags = 0,
    };

    VkAccelerationStructureBuildGeometryInfoKHR build_info = {
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR,
        .type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR,
        .flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR,
        .mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR,
        .dstAccelerationStructure = acceleration_structure,
        .geometryCount = 1,
        .pGeometries = &geom,
        .scratchData = {.deviceAddress = buffers.scratch.gpu_address()},
    };

    cmds.attach(move(buffers.scratch));
    cmds.attach(move(gpu_instances));

    VkAccelerationStructureBuildRangeInfoKHR offset = {
        .primitiveCount = static_cast<u32>(cpu_instances.length()),
    };

    Array<VkAccelerationStructureBuildRangeInfoKHR*, 1> offsets{&offset};

    vkCmdBuildAccelerationStructuresKHR(cmds, 1, &build_info, offsets.data());

    return TLAS{move(device), acceleration_structure, move(buffers.structure)};
}

BLAS::BLAS(Arc<Device, Alloc> device, VkAccelerationStructureKHR structure, Buffer buffer)
    : device(move(device)), buffer(move(buffer)), structure(structure){};

BLAS::BLAS(BLAS&& src) : device(move(src.device)), buffer(move(src.buffer)) {
    structure = src.structure;
    src.structure = null;
}

BLAS::~BLAS() {
    if(structure) {
        vkDestroyAccelerationStructureKHR(*device, structure, null);
    }
    structure = null;
}

BLAS& BLAS::operator=(BLAS&& src) {
    assert(this != &src);
    this->~BLAS();
    device = move(src.device);
    buffer = move(src.buffer);
    structure = src.structure;
    src.structure = null;
    return *this;
}

u64 BLAS::gpu_address() {
    if(!structure) return 0;
    VkAccelerationStructureDeviceAddressInfoKHR info = {
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR,
        .accelerationStructure = structure,
    };
    return vkGetAccelerationStructureDeviceAddressKHR(*device, &info);
}

Opt<BLAS::Buffers> BLAS::make(Arc<Device_Memory, Alloc>& memory, Slice<const BLAS::Size> sizes) {

    assert(sizes.length());

    Region(R) {

        Vec<VkAccelerationStructureGeometryKHR, Mregion<R>> geometries(sizes.length());
        Vec<u32, Mregion<R>> triangle_counts(sizes.length());

        for(auto& size : sizes) {

            VkAccelerationStructureGeometryKHR geom = {
                .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR,
                .geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR,
                .geometry =
                    {.triangles =
                         {
                             .sType =
                                 VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR,
                             .vertexFormat = VK_FORMAT_R32G32B32_SFLOAT,
                             .vertexStride = 3 * sizeof(f32),
                             .maxVertex = static_cast<u32>(size.n_vertices - 1),
                             .indexType = VK_INDEX_TYPE_UINT32,
                             .transformData = {.deviceAddress = size.transform ? 1u : 0u},
                         }},
                .flags = size.opaque ? VK_GEOMETRY_OPAQUE_BIT_KHR : VkGeometryFlagsKHR{},
            };

            geometries.push(geom);
            triangle_counts.push(static_cast<u32>(size.n_indices / 3));
        }

        VkAccelerationStructureBuildGeometryInfoKHR build_info = {
            .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR,
            .type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR,
            .flags = VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_DATA_ACCESS_KHR |
                     VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR,
            .mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR,
            .geometryCount = static_cast<u32>(geometries.length()),
            .pGeometries = geometries.data(),
        };

        VkAccelerationStructureBuildSizesInfoKHR build_sizes = {
            .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR,
        };

        vkGetAccelerationStructureBuildSizesKHR(*memory->device,
                                                VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
                                                &build_info, triangle_counts.data(), &build_sizes);

        Opt<Buffer> structure_buf =
            memory->make(build_sizes.accelerationStructureSize,
                         VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR |
                             VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT);
        if(!structure_buf.ok()) {
            return {};
        }

        Opt<Buffer> scratch =
            memory->make(build_sizes.buildScratchSize, VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
                                                           VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
        if(!scratch.ok()) {
            return {};
        }

        return Opt{
            Buffers{move(*structure_buf), move(*scratch), build_sizes.accelerationStructureSize}};
    }
}

BLAS BLAS::build(Arc<Device, Alloc> device, Commands& cmds, Buffers buffers, Buffer geometry,
                 Slice<const Offset> offsets) {

    assert(offsets.length());

    VkAccelerationStructureKHR acceleration_structure = null;

    VkAccelerationStructureCreateInfoKHR create_info = {
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR,
        .buffer = buffers.structure,
        .size = buffers.size,
        .type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR,
    };

    vkCreateAccelerationStructureKHR(*device, &create_info, null, &acceleration_structure);

    Region(R) {

        Vec<VkAccelerationStructureGeometryKHR, Mregion<R>> geometries(offsets.length());
        Vec<VkAccelerationStructureBuildRangeInfoKHR, Mregion<R>> ranges(offsets.length());

        u64 base_data = geometry.gpu_address();

        for(auto& offset : offsets) {
            VkAccelerationStructureGeometryKHR geom = {
                .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR,
                .geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR,
                .geometry =
                    {.triangles =
                         {.sType =
                              VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR,
                          .vertexFormat = VK_FORMAT_R32G32B32_SFLOAT,
                          .vertexData = {.deviceAddress = base_data + offset.vertex},
                          .vertexStride = 3 * sizeof(f32),
                          .maxVertex = static_cast<u32>(offset.n_vertices - 1),
                          .indexType = VK_INDEX_TYPE_UINT32,
                          .indexData = {.deviceAddress = base_data + offset.index},
                          .transformData = {.deviceAddress = offset.transform.ok()
                                                                 ? base_data + *offset.transform
                                                                 : 0u}}},
                .flags = offset.opaque ? VK_GEOMETRY_OPAQUE_BIT_KHR : VkGeometryFlagsKHR{},
            };

            geometries.push(geom);

            VkAccelerationStructureBuildRangeInfoKHR range = {
                .primitiveCount = static_cast<u32>(offset.n_indices / 3),
            };
            ranges.push(range);
        }

        VkAccelerationStructureBuildGeometryInfoKHR build_info = {
            .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR,
            .type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR,
            .flags = VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_DATA_ACCESS_KHR |
                     VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR,
            .mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR,
            .dstAccelerationStructure = acceleration_structure,
            .geometryCount = static_cast<u32>(geometries.length()),
            .pGeometries = geometries.data(),
            .scratchData = {.deviceAddress = buffers.scratch.gpu_address()},
        };

        cmds.attach(move(buffers.scratch));
        cmds.attach(move(geometry));

        Array<VkAccelerationStructureBuildRangeInfoKHR*, 1> range_ptrs{ranges.data()};

        vkCmdBuildAccelerationStructuresKHR(cmds, 1, &build_info, range_ptrs.data());

        return BLAS{move(device), acceleration_structure, move(buffers.structure)};
    }
}

} // namespace rvk::impl