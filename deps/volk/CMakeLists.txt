cmake_minimum_required(VERSION 3.17)

project(volk LANGUAGES C)

set(SOURCES_VOLK
    "volk.c"
    "volk.h"
)

add_library(volk STATIC ${SOURCES_VOLK})

target_compile_definitions(volk PRIVATE VOLK_VULKAN_H_PATH="../vulkan/vulkan.h")

if(MSVC)
    target_compile_definitions(volk
        PRIVATE
        WIN32_LEAN_AND_MEAN NOMINMAX _HAS_EXCEPTIONS=0 _CRT_SECURE_NO_WARNINGS
        $<$<CONFIG:Debug>:>
        $<$<CONFIG:RelWithDebInfo>:>
        $<$<CONFIG:Release>:_NO_DEBUG_HEAP=1>
        $<$<CONFIG:MinSizeRel>:_NO_DEBUG_HEAP=1>
    )
    target_compile_options(volk PRIVATE /arch:AVX2 /MP /W4 /GR- /GS- /EHa- /wd4201)
else()
    target_compile_options(volk PRIVATE -mavx2 -Wall -Wextra)
endif()

if(NOT WIN32)
    target_link_libraries(volk PRIVATE dl)
endif()

set_target_properties(volk PROPERTIES C_STANDARD 17 C_EXTENSIONS OFF)
