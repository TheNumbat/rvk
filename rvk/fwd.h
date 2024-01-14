
#pragma once

#ifdef RPP_OS_WINDOWS
#define VK_USE_PLATFORM_WIN32_KHR
#endif

#include <rpp/base.h>
#include <volk/volk.h>

#define RVK_CHECK(f)                                                                               \
    do {                                                                                           \
        VkResult _vk_res = (f);                                                                    \
        if(_vk_res != VK_SUCCESS) {                                                                \
            RPP_DEBUG_BREAK;                                                                       \
            die("RVK_CHECK: %", describe(_vk_res));                                                \
        }                                                                                          \
    } while(0)

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
struct Descriptor_Set_Layout;
struct Descriptor_Set;
struct Descriptor_Pool;
struct Fence;
struct Semaphore;
struct Commands;
struct Command_Pool;
template<Queue_Family F>
struct Command_Pool_Manager;
struct Swapchain;

String_View describe(VkResult result);
String_View describe(VkObjectType object);
void check(VkResult result);

} // namespace impl
} // namespace rvk
