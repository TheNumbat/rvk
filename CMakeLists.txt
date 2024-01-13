cmake_minimum_required(VERSION 3.17)

project(rvk LANGUAGES CXX)

option(RVK_HAS_RPP "Use existing copy of rpp" OFF)
option(RVK_TEST "Build tests" OFF)

if(NOT RVK_HAS_RPP)
    add_subdirectory("deps/rpp")
endif()

add_subdirectory("deps/volk")
add_subdirectory("rvk/")

get_directory_property(HAS_PARENT PARENT_DIRECTORY)

if(HAS_PARENT)
    set(RVK_INCLUDE_DIRS ${RPP_INCLUDE_DIRS} ${CMAKE_CURRENT_SOURCE_DIR}/deps ${CMAKE_CURRENT_SOURCE_DIR} PARENT_SCOPE)
endif()

if(RVK_TEST)
    set(RVK_INCLUDE_DIRS ${RPP_INCLUDE_DIRS} ${CMAKE_CURRENT_SOURCE_DIR}/deps ${CMAKE_CURRENT_SOURCE_DIR})

    enable_testing()
    add_subdirectory("test/")

    file(GLOB TEST_SOURCES test/*.cpp)

    foreach(test ${TEST_SOURCES})
        get_filename_component(testname ${test} NAME_WE)
        add_test(NAME ${testname}
            COMMAND $<TARGET_FILE:${testname}>
            WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/test")
    endforeach(test ${TEST_SOURCES})
endif()
