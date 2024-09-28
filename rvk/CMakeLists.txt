cmake_minimum_required(VERSION 3.17)

project(rvk LANGUAGES CXX)

set(SOURCES_RVK
    "drop.h"
    "execute.h"
    "bindings.h"
    "fwd.h"
    "fwd.cpp"
    "rvk.h"
    "rvk.cpp"
    "instance.h"
    "instance.cpp"
    "device.h"
    "device.cpp"
    "swapchain.h"
    "swapchain.cpp"
    "memory.h"
    "memory.cpp"
    "descriptors.h"
    "descriptors.cpp"
    "commands.h"
    "commands.cpp"
    "pipeline.h"
    "pipeline.cpp"
    "acceleration.h"
    "acceleration.cpp"
    "shader_loader.h"
    "shader_loader.cpp"
    "imgui_impl_vulkan.h"
    "imgui_impl_vulkan.cpp"
)

if(UNIX AND NOT APPLE)
    set(LINUX TRUE)
endif()

if(WIN32 OR LINUX)
    set(SOURCES_RVK ${SOURCES_RVK})
else()
    message(FATAL_ERROR "Unsupported platform: only Windows and Linux are supported.")
endif()

add_library(rvk STATIC ${SOURCES_RVK})
set_target_properties(rvk PROPERTIES CXX_STANDARD 20 CXX_EXTENSIONS OFF LINKER_LANGUAGE CXX)

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    set(CLANG TRUE)
endif()

if(NOT TARGET rpp)
    message(FATAL_ERROR "rpp could not be found. rpp must be built before rvk.")
endif()

set_property(TARGET rvk PROPERTY INTERPROCEDURAL_OPTIMIZATION
    $<$<CONFIG:Debug>:FALSE>
    $<$<CONFIG:RelWithDebInfo>:FALSE>
    $<$<CONFIG:Release>:TRUE>
    $<$<CONFIG:MinSizeRel>:TRUE>)

target_link_libraries(rvk PRIVATE rpp volk imgui)
target_include_directories(rvk PRIVATE "../deps/" ${RPP_INCLUDE_DIRS})

if(MSVC)
    if(MSVC_VERSION LESS 1937)
        message(FATAL_ERROR "Unsupported MSVC version: only 19.37+ is supported.")
    endif()

    string(REPLACE "/GR" "" CMAKE_CXX_FLAGS ${CMAKE_CXX_FLAGS})

    target_compile_definitions(rvk
        PRIVATE
        WIN32_LEAN_AND_MEAN NOMINMAX _HAS_EXCEPTIONS=0
        $<$<CONFIG:Debug>:>
        $<$<CONFIG:RelWithDebInfo>:>
        $<$<CONFIG:Release>:_NO_DEBUG_HEAP=1>
        $<$<CONFIG:MinSizeRel>:_NO_DEBUG_HEAP=1>
    )
    target_link_options(rvk PRIVATE /NODEFAULTLIB:MSVCRT)
    target_compile_options(rvk PRIVATE /MP /W4 /GR- /GS- /EHa- /wd4201)

    if(RVK_NV_AFTERMATH)
        target_compile_definitions(rvk PRIVATE NV_AFTERMATH)
        target_link_libraries(rvk PRIVATE "${PROJECT_SOURCE_DIR}/../deps/aftermath/GFSDK_Aftermath_Lib.x64.lib")
        add_custom_command(TARGET rvk POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${PROJECT_SOURCE_DIR}/../deps/aftermath/GFSDK_Aftermath_Lib.x64.dll"
            ${CMAKE_BINARY_DIR})
    endif()
elseif(CLANG)
    if(CLANG_VERSION_MAJOR LESS 17)
        message(FATAL_ERROR "Unsupported Clang version: only 17+ is supported.")
    endif()

    target_compile_options(rvk PRIVATE -Wall -Wextra -fno-exceptions -fno-rtti -Wno-missing-field-initializers)
else()
    message(FATAL_ERROR "Unsupported compiler: only MSVC and Clang are supported.")
endif()
