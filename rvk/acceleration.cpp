
#include <rpp/base.h>

#include "acceleration.h"
#include "commands.h"
#include "rvk.h"

namespace rvk::impl {

TLAS::TLAS(Arc<Device_Memory, Alloc> memory, Buffer structure, VkAccelerationStructureKHR accel)
    : memory(move(memory)), structure_buf(move(structure)), acceleration_structure(accel){};

TLAS::~TLAS() {
    if(acceleration_structure) {
        vkDestroyAccelerationStructureKHR(*memory->device, acceleration_structure, null);
    }
    acceleration_structure = null;
}

TLAS::TLAS(TLAS&& src) : memory(move(src.memory)), structure_buf(move(src.structure_buf)) {
    acceleration_structure = src.acceleration_structure;
    src.acceleration_structure = null;
}

TLAS& TLAS::operator=(TLAS&& src) {
    assert(this != &src);
    this->~TLAS();
    memory = move(src.memory);
    structure_buf = move(src.structure_buf);
    acceleration_structure = src.acceleration_structure;
    src.acceleration_structure = null;
    return *this;
}

Opt<TLAS::Staged> TLAS::make(Arc<Device_Memory, Alloc> memory, Buffer instances, u32 n_instances) {

    assert(instances.length());

    // Compute necessary sizes for the buffers

    VkAccelerationStructureGeometryKHR geom = {
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR,
        .geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR,
        .geometry =
            {.instances = {.sType =
                               VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR,
                           .arrayOfPointers = VK_FALSE}},
        .flags = VK_GEOMETRY_OPAQUE_BIT_KHR,
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
                                            &build_info, &n_instances, &build_sizes);

    // Allocate buffers

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

    return Opt<Staged>{Staged{move(*structure_buf), move(*scratch), move(instances), n_instances,
                              build_sizes.accelerationStructureSize, move(memory)}};
}

TLAS TLAS::build(Commands& cmds, Staged buffers) {

    // Create acceleration structure

    VkAccelerationStructureKHR acceleration_structure = null;

    VkAccelerationStructureCreateInfoKHR create_info = {
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR,
        .buffer = buffers.result,
        .size = buffers.result_size,
        .type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR,
    };

    RVK_CHECK(vkCreateAccelerationStructureKHR(*buffers.memory->device, &create_info, null,
                                               &acceleration_structure));

    // Build

    VkAccelerationStructureGeometryKHR geom = {
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR,
        .geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR,
        .geometry =
            {.instances = {.sType =
                               VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR,
                           .arrayOfPointers = VK_FALSE,
                           .data = {.deviceAddress = buffers.instances.gpu_address()}}},
        .flags = VK_GEOMETRY_OPAQUE_BIT_KHR,
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
    cmds.attach(move(buffers.instances));

    VkAccelerationStructureBuildRangeInfoKHR offset = {
        .primitiveCount = buffers.n_instances,
    };

    Array<VkAccelerationStructureBuildRangeInfoKHR*, 1> offsets{&offset};

    vkCmdBuildAccelerationStructuresKHR(cmds, 1, &build_info, offsets.data());

    return TLAS{move(buffers.memory), move(buffers.result), acceleration_structure};
}

BLAS::BLAS(Arc<Device_Memory, Alloc> memory, Buffer structure, VkAccelerationStructureKHR accel)
    : memory(move(memory)), structure_buf(move(structure)), acceleration_structure(accel){};

BLAS::BLAS(BLAS&& src) : memory(move(src.memory)), structure_buf(move(src.structure_buf)) {
    acceleration_structure = src.acceleration_structure;
    src.acceleration_structure = null;
}

BLAS::~BLAS() {
    if(acceleration_structure) {
        vkDestroyAccelerationStructureKHR(*memory->device, acceleration_structure, null);
    }
    acceleration_structure = null;
}

BLAS& BLAS::operator=(BLAS&& src) {
    assert(this != &src);
    this->~BLAS();
    memory = move(src.memory);
    structure_buf = move(src.structure_buf);
    acceleration_structure = src.acceleration_structure;
    src.acceleration_structure = null;
    return *this;
}

u64 BLAS::gpu_address() {
    if(!acceleration_structure) return 0;
    VkAccelerationStructureDeviceAddressInfoKHR info = {
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR,
        .accelerationStructure = acceleration_structure,
    };
    return vkGetAccelerationStructureDeviceAddressKHR(*memory->device, &info);
}

Opt<BLAS::Staged> BLAS::make(Arc<Device_Memory, Alloc> memory, Buffer data,
                             Vec<BLAS::Offsets, Alloc> offsets) {

    if(offsets.length() == 0) {
        return {};
    }

    // Compute necessary sizes for the buffers
    Region(R) {

        Vec<VkAccelerationStructureGeometryKHR, Mregion<R>> geometries(offsets.length());
        Vec<u32, Mregion<R>> triangle_counts(offsets.length());

        for(auto& offset : offsets) {

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
                             .maxVertex = static_cast<u32>(offset.n_vertices - 1),
                             .indexType = VK_INDEX_TYPE_UINT32,
                             .transformData = {.deviceAddress = offset.transform.ok() ? 1u : 0u},
                         }},
                .flags = VK_GEOMETRY_OPAQUE_BIT_KHR,
            };

            geometries.push(geom);
            triangle_counts.push(static_cast<u32>(offset.n_indices / 3));
        }

        VkAccelerationStructureBuildGeometryInfoKHR build_info = {
            .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR,
            .type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR,
            .flags = VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_DATA_ACCESS_KHR |
                     VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_BUILD_BIT_KHR,
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

        // Allocate buffers

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

        return Opt{Staged{move(*structure_buf), move(*scratch), move(data),
                          build_sizes.accelerationStructureSize, move(offsets), move(memory)}};
    }
} // namespace rvk::impl

BLAS BLAS::build(Commands& cmds, Staged buffers) {

    if(!buffers.geometry.length()) {
        return {};
    }

    // Create acceleration structure

    VkAccelerationStructureKHR acceleration_structure = null;

    VkAccelerationStructureCreateInfoKHR create_info = {
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR,
        .buffer = buffers.result,
        .size = buffers.result_size,
        .type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR,
    };

    vkCreateAccelerationStructureKHR(*buffers.memory->device, &create_info, null,
                                     &acceleration_structure);

    // Build acceleration structure

    Region(R) {

        Vec<VkAccelerationStructureGeometryKHR, Mregion<R>> geometries(buffers.offsets.length());
        Vec<VkAccelerationStructureBuildRangeInfoKHR, Mregion<R>> ranges(buffers.offsets.length());

        u64 base_data = buffers.geometry.gpu_address();

        for(auto& offset : buffers.offsets) {
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
                .flags = VK_GEOMETRY_OPAQUE_BIT_KHR,
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
                     VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_BUILD_BIT_KHR,
            .mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR,
            .dstAccelerationStructure = acceleration_structure,
            .geometryCount = static_cast<u32>(geometries.length()),
            .pGeometries = geometries.data(),
            .scratchData = {.deviceAddress = buffers.scratch.gpu_address()},
        };

        cmds.attach(move(buffers.scratch));
        cmds.attach(move(buffers.geometry));

        Array<VkAccelerationStructureBuildRangeInfoKHR*, 1> range_ptrs{ranges.data()};

        vkCmdBuildAccelerationStructuresKHR(cmds, 1, &build_info, range_ptrs.data());

        return BLAS{move(buffers.memory), move(buffers.result), acceleration_structure};
    }
}

} // namespace rvk::impl