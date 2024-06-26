
#pragma once

#ifdef RPP_OS_WINDOWS
#define VK_USE_PLATFORM_WIN32_KHR
#endif

#include <rpp/base.h>
#include <volk/volk.h>

#ifdef RPP_DEBUG_BUILD
#define RVK_CHECK(f)                                                                               \
    do {                                                                                           \
        VkResult _vk_res = (f);                                                                    \
        if(_vk_res != VK_SUCCESS) {                                                                \
            RPP_DEBUG_BREAK;                                                                       \
            die("RVK_CHECK: %", describe(_vk_res));                                                \
        }                                                                                          \
    } while(0)
#else
#define RVK_CHECK(f)                                                                               \
    do {                                                                                           \
        VkResult _vk_res = (f);                                                                    \
        if(_vk_res != VK_SUCCESS) {                                                                \
            warn("RVK_CHECK: %", describe(_vk_res));                                               \
        }                                                                                          \
    } while(0)
#endif

namespace rvk {

using namespace rpp;
using Alloc = Mallocator<"rvk">;

enum class Queue_Family : u8;
enum class Heap : u8;

namespace impl {

struct Instance;
struct Debug_Callback;
struct Physical_Device;
struct Device;
struct Device_Memory;
struct Image;
struct Image_View;
struct Buffer;
struct TLAS;
struct BLAS;
struct Descriptor_Set_Layout;
struct Descriptor_Set;
struct Descriptor_Pool;
struct Fence;
struct Semaphore;
struct Sem_Ref;
struct Commands;
struct Command_Pool;
template<Queue_Family F>
struct Command_Pool_Manager;
struct Pipeline;
template<typename T, u32 stages, u32 offset>
struct Push;
struct Binding_Table;
struct Shader;
struct Sampler;
struct Swapchain;
struct Compositor;
struct Binder;
struct Vk;

String_View describe(VkResult result);
String_View describe(VkObjectType object);
void check(VkResult result);

} // namespace impl

using impl::Binding_Table;
using impl::BLAS;
using impl::Buffer;
using impl::Commands;
using impl::Descriptor_Set;
using impl::Descriptor_Set_Layout;
using impl::Fence;
using impl::Image;
using impl::Image_View;
using impl::Pipeline;
using impl::Push;
using impl::Sampler;
using impl::Sem_Ref;
using impl::Semaphore;
using impl::Shader;
using impl::TLAS;

} // namespace rvk
