
#pragma once

#include <rpp/base.h>
#include <rpp/rc.h>

#include "fwd.h"

#include "memory.h"

namespace rvk::impl {

using namespace rpp;

struct TLAS {

    using Instance = VkAccelerationStructureInstanceKHR;

    struct Buffers {
        Buffer structure;
        Buffer scratch;
        u64 size = 0;
    };

    TLAS() = default;
    ~TLAS();

    TLAS(const TLAS& src) = delete;
    TLAS& operator=(const TLAS& src) = delete;

    TLAS(TLAS&& src);
    TLAS& operator=(TLAS&& src);

    operator VkAccelerationStructureKHR() {
        return structure;
    }

private:
    explicit TLAS(Arc<Device, Alloc> device, VkAccelerationStructureKHR structure, Buffer buffer);
    friend struct Vk;

    static Opt<Buffers> make(Arc<Device_Memory, Alloc>& memory, u32 instances);
    static TLAS build(Arc<Device, Alloc> device, Commands& cmds, Buffers buffers,
                      Buffer gpu_instances, Slice<Instance> cpu_instances);

    Arc<Device, Alloc> device;

    Buffer buffer;
    VkAccelerationStructureKHR structure = null;
};

struct BLAS {

    struct Size {
        u64 n_vertices = 0;
        u64 n_indices = 0;
        bool transform = false;
    };

    struct Buffers {
        Buffer structure;
        Buffer scratch;
        u64 size = 0;
    };

    struct Offset {
        u64 vertex = 0;
        u64 index = 0;
        Opt<u64> transform;
        u64 n_vertices = 0;
        u64 n_indices = 0;
    };

    BLAS() = default;
    ~BLAS();

    BLAS(const BLAS& src) = delete;
    BLAS& operator=(const BLAS& src) = delete;
    BLAS(BLAS&& src);
    BLAS& operator=(BLAS&& src);

    u64 gpu_address();

    operator VkAccelerationStructureKHR() {
        return structure;
    }

private:
    explicit BLAS(Arc<Device, Alloc> device, VkAccelerationStructureKHR structure,
                  Buffer buffer);
    friend struct Vk;

    static Opt<Buffers> make(Arc<Device_Memory, Alloc>& memory, Slice<Size> sizes);
    static BLAS build(Arc<Device, Alloc> device, Commands& cmds, Buffers buffers, Buffer geometry,
                      Slice<Offset> offsets);

    Arc<Device, Alloc> device;

    Buffer buffer;
    VkAccelerationStructureKHR structure = null;
};

} // namespace rvk::impl
