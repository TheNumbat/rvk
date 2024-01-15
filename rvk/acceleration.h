
#pragma once

#include <rpp/base.h>
#include <rpp/rc.h>

#include "fwd.h"

#include "memory.h"

namespace rvk::impl {

using namespace rpp;

struct TLAS {

    using Instance = VkAccelerationStructureInstanceKHR;

    struct Staged {
        Staged() = default;
        ~Staged() = default;

        Staged(const Staged& src) = delete;
        Staged& operator=(const Staged& src) = delete;

        Staged(Staged&& src) = default;
        Staged& operator=(Staged&& src) = default;

    private:
        explicit Staged(Buffer result, Buffer scratch, Buffer& instances, u32 n_instances,
                        u64 result_size, Arc<Device_Memory, Alloc> memory)
            : result(move(result)), scratch(move(scratch)), instances(instances),
              n_instances(n_instances), result_size(result_size), memory(move(memory)) {
        }

        Buffer result;
        Buffer scratch;
        Ref<Buffer> instances;
        u32 n_instances = 0;
        u64 result_size = 0;
        Arc<Device_Memory, Alloc> memory;

        friend struct TLAS;
    };

    TLAS() = default;
    ~TLAS();

    TLAS(const TLAS& src) = delete;
    TLAS& operator=(const TLAS& src) = delete;

    TLAS(TLAS&& src);
    TLAS& operator=(TLAS&& src);

    static TLAS build(Commands& cmds, Staged gpu_data);

    operator VkAccelerationStructureKHR() {
        return acceleration_structure;
    }

private:
    explicit TLAS(Arc<Device_Memory, Alloc> memory, Buffer structure,
                  VkAccelerationStructureKHR accel);
    friend struct Vk;

    static Opt<Staged> make(Arc<Device_Memory, Alloc> memory, Buffer& instances, u32 n_instances);

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
        Staged() = default;
        ~Staged() = default;

        Staged(const Staged& src) = delete;
        Staged& operator=(const Staged& src) = delete;

        Staged(Staged&& src) = default;
        Staged& operator=(Staged&& src) = default;

    private:
        explicit Staged(Buffer result, Buffer scratch, Buffer& geometry, u64 result_size,
                        Vec<Offsets, Alloc> offsets, Arc<Device_Memory, Alloc> memory)
            : result(move(result)), scratch(move(scratch)), geometry(geometry),
              result_size(result_size), offsets(move(offsets)), memory(move(memory)) {
        }

        Buffer result;
        Buffer scratch;
        Ref<Buffer> geometry;
        u64 result_size = 0;
        Vec<Offsets, Alloc> offsets;
        Arc<Device_Memory, Alloc> memory;

        friend struct BLAS;
    };

    BLAS() = default;
    ~BLAS();

    BLAS(const BLAS& src) = delete;
    BLAS& operator=(const BLAS& src) = delete;
    BLAS(BLAS&& src);
    BLAS& operator=(BLAS&& src);

    static BLAS build(Commands& cmds, Staged buffers);

    u64 gpu_address();

    operator VkAccelerationStructureKHR() {
        return acceleration_structure;
    }

private:
    explicit BLAS(Arc<Device_Memory, Alloc> memory, Buffer structure,
                  VkAccelerationStructureKHR accel);
    friend struct Vk;

    static Opt<Staged> make(Arc<Device_Memory, Alloc> memory, Buffer& geometry,
                            Vec<Offsets, Alloc> offsets);

    Arc<Device_Memory, Alloc> memory;

    Buffer structure_buf;
    VkAccelerationStructureKHR acceleration_structure = null;
};

} // namespace rvk::impl
