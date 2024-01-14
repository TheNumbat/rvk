
#pragma once

#include <rpp/base.h>
#include <rpp/rc.h>

#include "fwd.h"

#include "memory.h"

namespace rvk::impl {

using namespace rpp;

struct TLAS {
    struct Staged {
    private:
        explicit Staged(Buffer result, Buffer scratch, Buffer instances, u32 n_instances,
                        u64 result_size, Arc<Device_Memory, Alloc> memory)
            : result(move(result)), scratch(move(scratch)), instances(move(instances)),
              n_instances(n_instances), result_size(result_size), memory(move(memory)) {
        }

        Buffer result;
        Buffer scratch;
        Buffer instances;
        u32 n_instances = 0;
        u64 result_size = 0;
        Arc<Device_Memory, Alloc> memory;

        friend struct TLAS;
    };

    ~TLAS();

    TLAS(const TLAS& src) = delete;
    TLAS& operator=(const TLAS& src) = delete;

    TLAS(TLAS&& src);
    TLAS& operator=(TLAS&& src);

    using Instance = VkAccelerationStructureInstanceKHR;

    static Opt<Staged> make(Arc<Device_Memory, Alloc> memory, Buffer instances, u32 n_instances);

    static TLAS build(Commands& cmds, Staged gpu_data);

    operator VkAccelerationStructureKHR() {
        return acceleration_structure;
    }

private:
    explicit TLAS(Arc<Device_Memory, Alloc> memory, Buffer structure,
                  VkAccelerationStructureKHR accel);

    Arc<Device_Memory, Alloc> memory;

    Buffer structure_buf;
    VkAccelerationStructureKHR acceleration_structure = null;
};

struct BLAS {

    struct Offsets {
        u64 vertex = 0;
        u64 index = 0;
        u64 n_vertices = 0;
        u64 n_indices = 0;
        Opt<u64> transform;
    };

    struct Staged {
    private:
        explicit Staged(Buffer result, Buffer scratch, Buffer geometry, u64 result_size,
                        Vec<Offsets, Alloc> offsets, Arc<Device_Memory, Alloc> memory)
            : result(move(result)), scratch(move(scratch)), geometry(move(geometry)),
              result_size(result_size), offsets(move(offsets)), memory(move(memory)) {
        }

        Buffer result;
        Buffer scratch;
        Buffer geometry;
        u64 result_size = 0;
        Vec<Offsets, Alloc> offsets;
        Arc<Device_Memory, Alloc> memory;

        friend struct BLAS;
    };

    ~BLAS();

    BLAS(const BLAS& src) = delete;
    BLAS& operator=(const BLAS& src) = delete;
    BLAS(BLAS&& src);
    BLAS& operator=(BLAS&& src);

    static Opt<Staged> make(Arc<Device_Memory, Alloc> memory, Buffer geometry,
                            Vec<Offsets, Alloc> offsets);

    static BLAS build(Commands& cmds, Staged buffers);

    u64 gpu_address();

    operator VkAccelerationStructureKHR() {
        return acceleration_structure;
    }

private:
    explicit BLAS(Arc<Device_Memory, Alloc> memory, Buffer structure,
                  VkAccelerationStructureKHR accel);

    Arc<Device_Memory, Alloc> memory;

    Buffer structure_buf;
    VkAccelerationStructureKHR acceleration_structure = null;
};

} // namespace rvk::impl
