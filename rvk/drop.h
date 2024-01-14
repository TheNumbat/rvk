
#pragma once

#include "fwd.h"

namespace rvk {

using namespace rpp;

template<typename T>
struct Drop {

    template<typename... Args>
        requires Constructable<T, Args...>
    explicit Drop(Args&&... args) : resource(forward<Args>(args)...) {
    }

    explicit Drop(T&& resource) : resource(move(resource)) {
    }

    ~Drop() {
        drop([r = move(resource)]() {});
    }

    Drop(const Drop&) = delete;
    Drop& operator=(const Drop&) = delete;

    Drop(Drop&& src) : resource(move(src.resource)) {
    }

    Drop& operator=(Drop&& src) {
        assert(this != &src);
        this->~Drop();
        resource = move(src.resource);
        return *this;
    }

    operator T&() {
        return resource;
    }
    operator const T&() const {
        return resource;
    }

    T* operator->() {
        return &resource;
    }
    const T* operator->() const {
        return &resource;
    }
    T& operator*() {
        return resource;
    }
    const T& operator*() const {
        return resource;
    }

private:
    T resource;
};

} // namespace rvk