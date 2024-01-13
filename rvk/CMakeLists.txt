cmake_minimum_required(VERSION 3.17)

project(rvk LANGUAGES CXX)

set(SOURCES_RVK
    "rvk.h"
    "rvk.cpp"
)

if(UNIX AND NOT APPLE)
    set(LINUX TRUE)
endif()

if(WIN32)
    set(SOURCES_RVK ${SOURCES_RVK})
elseif(LINUX)
    set(SOURCES_RVK ${SOURCES_RVK})
else()
    message(FATAL_ERROR "Unsupported platform: only Windows and Linux are supported.")
endif()

add_library(rvk STATIC ${SOURCES_RVK})
set_target_properties(rvk PROPERTIES CXX_STANDARD 20 CXX_EXTENSIONS OFF LINKER_LANGUAGE CXX)

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    set(CLANG TRUE)
endif()

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
    target_compile_options(rvk PRIVATE /arch:AVX2 /MP /W4 /GR- /GS- /EHa- /wd4201)

elseif(CLANG)
    if(CLANG_VERSION_MAJOR LESS 17)
        message(FATAL_ERROR "Unsupported Clang version: only 17+ is supported.")
    endif()

    target_compile_options(rvk PRIVATE -mavx2 -Wall -Wextra -fno-exceptions -fno-rtti)
else()
    message(FATAL_ERROR "Unsupported compiler: only MSVC and Clang are supported.")
endif()
