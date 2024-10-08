
#pragma once

#include <rpp/base.h>
#include <rpp/pool.h>

#include "fwd.h"

#include "acceleration.h"
#include "bindings.h"
#include "commands.h"
#include "descriptors.h"
#include "drop.h"
#include "memory.h"
#include "pipeline.h"
#include "shader_loader.h"

namespace rvk {

using namespace rpp;

// Setup

using Finalizer = FunctionN<16, void()>;

struct Config {
    bool validation = true;
    bool robust_accesses = true;
    bool ray_tracing = false;
    bool imgui = false;
    bool hdr = false;

    u32 frames_in_flight = 2;
    u32 descriptors_per_type = 128;

    Slice<const String_View> layers;
    Slice<const String_View> swapchain_extensions;
    Function<VkSurfaceKHR(VkInstance)> create_surface;

    u64 host_heap = Math::GB(1);
    u64 device_heap = Math::MB(4094);
};

bool startup(Config config);
void hdr(bool enable);
void shutdown();
void reset_imgui();
void wait_idle();

// Info

void imgui();
bool resized();
bool minimized();
u32 frame();
u32 frame_count();
u32 previous_frame();
VkExtent2D extent();
VkFormat format();

// Frame lifecycle

void begin_frame();
void wait_frame(Sem_Ref sem);
void end_frame(Image_View& output);

// Resources

void drop(Finalizer f);

Fence make_fence();
Semaphore make_semaphore();
Commands make_commands(Queue_Family family = Queue_Family::graphics);

Opt<Buffer> make_staging(u64 size);
Opt<Buffer> make_buffer(u64 size, VkBufferUsageFlags usage);
Opt<Image> make_image(VkExtent3D extent, VkFormat format, VkImageUsageFlags usage);
Sampler make_sampler(Sampler::Config config);

Opt<TLAS::Buffers> make_tlas(u32 instances);
Opt<BLAS::Buffers> make_blas(Slice<const BLAS::Size> sizes);

TLAS build_tlas(Commands& cmds, TLAS::Buffers tlas, Buffer gpu_instances,
                Slice<const TLAS::Instance> cpu_instances);
BLAS build_blas(Commands& cmds, BLAS::Buffers blas, Buffer geometry,
                Slice<const BLAS::Offset> offsets);

// Pipelines

template<Type_List L>
    requires(Reflect::All<Is_Binding, L>)
Descriptor_Set_Layout make_layout(Slice<const u32> counts = Slice<const u32>{});

Descriptor_Set make_set(Descriptor_Set_Layout& layout, u32 variable_count = 0);
Descriptor_Set make_single_set(Descriptor_Set_Layout& layout, u32 variable_count = 0);

template<Type_List L, Binding... Binds>
    requires(Same<L, List<Binds...>>)
void write_set(Descriptor_Set& set, Binds&... binds);

template<Type_List L, Binding... Binds>
    requires(Same<L, List<Binds...>>)
void write_set(Descriptor_Set& set, u32 frame_index, Binds&... binds);

Box<Shader_Loader, Alloc> make_shader_loader();

Pipeline make_pipeline(Pipeline::Info info);

Opt<Binding_Table> make_table(Commands& cmds, Pipeline& pipeline, Binding_Table::Mapping mapping);

// Command execution

void submit(Commands& cmds, u32 index);
void submit(Commands& cmds, u32 index, Fence& fence);
void submit(Commands& cmds, u32 index, Slice<const Sem_Ref> wait, Slice<const Sem_Ref> signal);
void submit(Commands& cmds, u32 index, Slice<const Sem_Ref> wait, Slice<const Sem_Ref> signal,
            Fence& fence);

template<typename F>
    requires Invocable<F, Commands&>
auto sync(F&& f, Queue_Family family = Queue_Family::graphics,
          u32 index = 0) -> Invoke_Result<F, Commands&>;

template<typename F>
    requires Invocable<F, Commands&>
auto async(Async::Pool<>& pool, F&& f, Queue_Family family = Queue_Family::graphics,
           u32 index = 0) -> Async::Task<Invoke_Result<F, Commands&>>;

} // namespace rvk

#include "execute.h"
