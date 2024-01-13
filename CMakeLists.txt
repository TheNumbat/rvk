cmake_minimum_required(VERSION 3.17)

project(rvk LANGUAGES CXX)

option(RVK_VENDOR_RPP "Use vendored copy of rpp" OFF)
option(RVK_TEST "Build tests" OFF)

add_subdirectory("rvk/")
add_subdirectory("deps/volk")

if(RVK_VENDOR_RPP)
    add_subdirectory("deps/rpp")
endif()

if(RVK_TEST)
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
