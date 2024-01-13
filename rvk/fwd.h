
#pragma once

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
namespace impl {

using namespace rpp;

using Alloc = Mallocator<"rvk">;

struct Instance;
struct Debug_Callback;
struct Physical_Device;

String_View describe(VkResult result);
String_View describe(VkObjectType object);
void check(VkResult result);

} // namespace impl
} // namespace rvk
