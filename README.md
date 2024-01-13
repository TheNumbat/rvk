# rvk

(WIP; refactoring into new repo)

[rpp](https://github.com/TheNumbat/rpp)-based Vulkan abstraction layer.

Does not seek to redefine the Vulkan API: instead provides a convenient interface for common use cases.
Includes the following features:

- RAII wrappers for Vulkan objects
- GPU heap allocator and buffer sub-allocators
- Multiple frames in flight and resource delete queue
- Async GPU tasks for coroutines
- Swapchain management and compositor
- Validation layer config and debug messaging
- Graphics, compute, and transfer queue management
- Multithreaded command pool management
- Compile-time descriptor set layout specifications
- [Dear ImGui](https://github.com/ocornut/imgui) integration

## Integration

To use rvk in your project, run the following commands (or manually download the source):

```bash
git submodule add https://github.com/TheNumbat/rvk
git submodule update --init --recursive
```

Then add the following lines to your CMakeLists.txt:

```cmake
add_subdirectory(rvk)
target_link_libraries($your_target PRIVATE rvk)
target_include_directories($your_target PRIVATE ${RVK_INCLUDE_DIRS})
```

If you're already using [rpp](https://github.com/TheNumbat/rpp), also set the following option:

```cmake
set(RVK_HAS_RPP TRUE)
```

Alternatively, to start an rvk project from scratch, you can fork [rpp_example_project/rvk](https://github.com/TheNumbat/rpp_example_project/tree/rvk).

## Build and Run Tests

To build rvk and run the tests, run the following commands:

### Windows

Assure MSVC 19.37 and cmake 3.17 (or newer) are installed and in your PATH.

```bash
mkdir build
cd build
cmake ..
cmake --build . -DRVK_TEST=ON
ctest -C Debug
```

For faster parallel builds, you can instead generate [ninja](https://ninja-build.org/) build files with `cmake -G Ninja ..`.

### Linux

Assure clang-17 and cmake 3.17 (or newer) are installed.

```bash
mkdir build
cd build
CXX=clang++-17 cmake .. -DRVK_TEST=ON
make -j
ctest -C Debug
```

For faster parallel builds, you can instead generate [ninja](https://ninja-build.org/) build files with `cmake -G Ninja ..`.
