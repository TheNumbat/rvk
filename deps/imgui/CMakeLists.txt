cmake_minimum_required(VERSION 3.17)

project(ImGui LANGUAGES CXX)

set(SOURCES_IMGUI
    "imgui.cpp"
    "imgui.h"
    "imconfig.h"
    "imgui_internal.h"
    "imgui_draw.cpp"
    "imgui_tables.cpp"
    "imgui_widgets.cpp"
    "imstb_textedit.h"
    "imstb_rectpack.h"
    "imstb_truetype.h"
)

add_library(ImGui STATIC ${SOURCES_IMGUI})

target_include_directories(ImGui PRIVATE ${RPP_INCLUDE_DIRS})

if(MSVC)
    target_compile_definitions(ImGui
        PRIVATE
        WIN32_LEAN_AND_MEAN NOMINMAX _HAS_EXCEPTIONS=0
        $<$<CONFIG:Debug>:>
        $<$<CONFIG:RelWithDebInfo>:>
        $<$<CONFIG:Release>:_NO_DEBUG_HEAP=1>
        $<$<CONFIG:MinSizeRel>:_NO_DEBUG_HEAP=1>
    )

    string(REPLACE "/GR" "" CMAKE_CXX_FLAGS ${CMAKE_CXX_FLAGS})

    target_compile_options(ImGui PRIVATE /arch:AVX2 /MP /W4 /GR- /GS- /EHa- /wd4201)
else()
    target_compile_options(ImGui PRIVATE -mavx2 -Wall -Wextra -fno-exceptions -fno-rtti)
endif()

set_target_properties(ImGui PROPERTIES CXX_STANDARD 20 CXX_EXTENSIONS OFF)
