cmake_minimum_required(VERSION 3.17)

project(rvk LANGUAGES CXX)

option(RVK_HAS_RPP "Use existing copy of rpp" OFF)
option(RVK_NV_AFTERMATH "Enable NV Aftermath crash dumper" OFF)

if(NOT RVK_HAS_RPP)
    add_subdirectory("deps/rpp")
endif()

add_subdirectory("deps/imgui")
add_subdirectory("deps/volk")
add_subdirectory("rvk/")

get_directory_property(HAS_PARENT PARENT_DIRECTORY)

if(HAS_PARENT)
    set(RVK_INCLUDE_DIRS ${RPP_INCLUDE_DIRS} ${CMAKE_CURRENT_SOURCE_DIR}/deps ${CMAKE_CURRENT_SOURCE_DIR} PARENT_SCOPE)
endif()
