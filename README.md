# rvk

[![Windows](https://github.com/TheNumbat/rvk/actions/workflows/windows.yml/badge.svg)](https://github.com/TheNumbat/rvk/actions/workflows/windows.yml)
[![Ubuntu](https://github.com/TheNumbat/rvk/actions/workflows/ubuntu.yml/badge.svg)](https://github.com/TheNumbat/rvk/actions/workflows/ubuntu.yml)

Low-level [rpp](https://github.com/TheNumbat/rpp)-based Vulkan 1.3 abstraction layer.

rvk does not seek to redefine the Vulkan API, nor expose all of its configurability.
Instead, rvk provides a convenient interface for modern desktop GPUs.
It includes the following features:

- RAII wrappers for Vulkan objects
- GPU heap allocators (host and device)
- Multiple frames in flight and resource deletion queue
- Awaitable GPU tasks for coroutines
- Multithreaded command pool management for graphics, compute, and transfer queues
- Swapchain management and compositor
- Validation config and debug messaging
- Compile-time descriptor set layout specifications
- Shader hot reloading
- [Dear ImGui](https://github.com/ocornut/imgui) integration
- [NVIDIA Aftermath](https://developer.nvidia.com/nsight-aftermath) integration (optional)

rvk assumes your application uses dynamic rendering and bindless/BDA pipelines.
It explicitly lacks the following features:

- Fine-grained resource management: the user wrangles scene data by sub-allocating buffers and/or using BDA.
- Shader source management: the user controls compilation to SPIR-V.
- Windowing: the user creates a window, chooses a swapchain extension, and tracks input.
- Scene graph / dependency tracking / barrier insertion
- Pipeline caching

The minimal API of rvk may be found in [rvk.h](rvk/rvk.h).

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

To enable NVIDIA Aftermath, set the following option:

```cmake
set(RVK_NV_AFTERMATH TRUE)
```

## Examples

### Main Loop

```c++
#include <rpp/base.h>
#include <rvk/rvk.h>

using namespace rpp;

i32 main() {

    // Use your platform abstraction layer to create a window...

    // Initialize your imgui platform backend...

    Vec<String_View> swapchain_extensions = /* Find platform swapchain extensions */;

    rvk::startup(rvk::Config{
        .imgui = true,
        .swapchain_extensions = swapchain_extensions.slice(),
        .create_surface =
            [](VkInstance instance) {
                return /* Create platform surface */;
            },
    });

    {
        rvk::Drop<rvk::Image> target{move(*rvk::make_image({1280, 720, 1}, VK_FORMAT_R8G8B8A8_UNORM,
                                                            VK_IMAGE_USAGE_SAMPLED_BIT))};
        rvk::sync([&](rvk::Commands& cmds) {
            target->setup(cmds, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        });

        rvk::Drop<rvk::Image_View> target_view{target->view(VK_IMAGE_ASPECT_COLOR_BIT)};

        while(/* User hasn't quit */) {
            rvk::begin_frame();

            // Do your rendering...

            rvk::end_frame(target_view):
        }
    }

    rvk::shutdown();
}
```

### Pipelines

```c++
#include <rpp/base.h>
#include <rvk/rvk.h>

using namespace rpp;

i32 main() {

    rvk::Shader_Loader loader = rvk::make_shader_loader();

    auto vertex = loader.compile("shader.vert.spv"_v);
    auto fragment = loader.compile("shader.frag.spv"_v);

    loader.on_reload(Slice{{vertex, fragment}}, [&](Shader_Loader&) {
        // Recreate your pipeline...
    });

    using Layout = List<rvk::Bind::Buffer_Storage<VK_SHADER_STAGE_VERTEX_BIT>,
                        rvk::Bind::Image_And_Sampler<VK_SHADER_STAGE_FRAGMENT_BIT>>;

    auto descriptor_set_layout = rvk::make_layout<Layout>();
    auto descriptor_set = rvk::make_set(descriptor_set_layout);

    VkGraphicsPipelineCreateInfo graphics = /* ... */;

    rvk::Pipeline pipeline = rvk::make_pipeline(rvk::Pipeline::Info{
        .push_constants = Slice{rvk::Push<VK_SHADER_STAGE_VERTEX_BIT, u64>::range},
        .descriptor_set_layouts = Slice{&descriptor_set_layout, 1},
        .info = graphics
    });
}
```

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
CXX=clang++-17 cmake ..
make -j
```

For faster parallel builds, you can instead generate [ninja](https://ninja-build.org/) build files with `cmake -G Ninja ..`.
