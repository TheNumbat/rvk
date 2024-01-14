# rvk

(WIP; refactoring into new repo)

[rpp](https://github.com/TheNumbat/rpp)-based Vulkan 1.3 abstraction layer.

rvk does not seek to redefine the Vulkan API, nor expose all of its configurability.
Instead, rvk provides a convenient interface for modern desktop GPUs.
It includes the following features:

- RAII wrappers for Vulkan objects
- GPU heap allocator and buffer sub-allocators
- Multiple frames in flight and resource deletion queue
- Async GPU tasks for coroutines
- Swapchain management and compositor
- Validation layer config and debug messaging
- Shader hot reloading
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

## Build

To build rvk on its own, run the following commands:

### Windows

Assure MSVC 19.37 and cmake 3.17 (or newer) are installed and in your PATH.

```bash
mkdir build
cd build
cmake ..
cmake --build .
```

For faster parallel builds, you can instead generate [ninja](https://ninja-build.org/) build files with `cmake -G Ninja ..`.

### Linux

Assure clang-17 and cmake 3.17 (or newer) are installed.

```bash
mkdir build
cd build
CXX=clang++-17 cmake .. -DRVK_TEST=ON
make -j
```

For faster parallel builds, you can instead generate [ninja](https://ninja-build.org/) build files with `cmake -G Ninja ..`.

# ToDo

Finish porting
- [ ] shader hot reloading
- [ ] descriptor gen
- [ ] acceleration structures
