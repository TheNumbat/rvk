# rvk

(WIP)

[rpp](https://github.com/TheNumbat/rpp)-based Vulkan abstraction layer.

Does not seek to redefine the Vulkan API: instead provides a convenient interface for common use cases.
Includes the following features:

- RAII wrappers for Vulkan objects
- GPU heap allocator and buffer sub-allocators
- [Dear ImGui](https://github.com/ocornut/imgui) integration
- Validation layer support
- Swapchain management and compositor
- Multiple frames in flight
- Separate graphics, compute, and transfer queues
- Multithreaded command pool management
- Resource deletion queue
- Pipeline creation (no cache)
- Compile-time descriptor set layout specifications
- Async GPU tasks (for the _rpp_ coroutine scheduler)
