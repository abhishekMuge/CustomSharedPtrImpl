
#ifndef ENABLESHAREDFROMTHIS_HPP_
#define ENABLESHAREDFROMTHIS_HPP_

#include <iostream>
#include <atomic>
#include <utility>
#include <type_traits>

//////////////////////////////////////////////////////////////
// Forward Declarations
//////////////////////////////////////////////////////////////

template<typename T> class SharedPtr;
template<typename T> class WeakPtr;

//////////////////////////////////////////////////////////////
// EnableSharedFromThis
//////////////////////////////////////////////////////////////

template<typename T>
class EnableSharedFromThis {
protected:
    WeakPtr<T> weak_this;

public:
    SharedPtr<T> shared_from_this() {
        return weak_this.lock();
    }

    SharedPtr<const T> shared_from_this() const {
        return weak_this.lock();
    }

    template<typename U>
    friend class SharedPtr;
};

//////////////////////////////////////////////////////////////
// Control Block Base
//////////////////////////////////////////////////////////////

struct ControlBlock {
    std::atomic<size_t> shared_count;
    std::atomic<size_t> weak_count;

    ControlBlock() : shared_count(1), weak_count(0) {}

    virtual void destroy_object() = 0;
    virtual void* get_ptr() = 0;

    virtual ~ControlBlock() = default;
};

//////////////////////////////////////////////////////////////
// Control Block Implementation (make_shared)
//////////////////////////////////////////////////////////////

template<typename T>
struct ControlBlockImpl : ControlBlock {

    alignas(T) unsigned char storage[sizeof(T)];

    template<typename... Args>
    ControlBlockImpl(Args&&... args) {
        new (storage) T(std::forward<Args>(args)...);
    }

    T* get_object() {
        return reinterpret_cast<T*>(storage);
    }

    void destroy_object() override {
        get_object()->~T();
    }

    void* get_ptr() override {
        return get_object();
    }
};

//////////////////////////////////////////////////////////////
// SharedPtr
//////////////////////////////////////////////////////////////

template<typename T>
class SharedPtr {
private:
    T* ptr;
    ControlBlock* cb;

    // Private constructor (used internally)
    SharedPtr(T* p, ControlBlock* control) : ptr(p), cb(control) {
        if (ptr) {
            enable_shared_from_this_helper(ptr);
        }
    }

    // Enable shared_from_this support
    template<typename U = T>
    void enable_shared_from_this_helper(U* obj) {
        if constexpr (std::is_base_of<EnableSharedFromThis<U>, U>::value) {
            obj->weak_this = *this;
        }
    }

public:
    template<typename U>
    friend class WeakPtr;

    template<typename U, typename... Args>
    friend SharedPtr<U> make_shared(Args&&...);

    // Constructor from raw pointer
    explicit SharedPtr(T* p = nullptr) : ptr(p) {
        if (p) {
            struct DefaultCB : ControlBlock {
                T* obj;
                DefaultCB(T* p) : obj(p) {}
                void destroy_object() override { delete obj; }
                void* get_ptr() override { return obj; }
            };

            cb = new DefaultCB(p);
            enable_shared_from_this_helper(p);
        } else {
            cb = nullptr;
        }
    }

    // Copy
    SharedPtr(const SharedPtr& other) {
        ptr = other.ptr;
        cb = other.cb;

        if (cb) {
            cb->shared_count.fetch_add(1, std::memory_order_relaxed);
        }
    }

    // Move
    SharedPtr(SharedPtr&& other) noexcept {
        ptr = other.ptr;
        cb = other.cb;

        other.ptr = nullptr;
        other.cb = nullptr;
    }

    // Copy assignment
    SharedPtr& operator=(const SharedPtr& other) {
        if (this == &other) return *this;

        release();

        ptr = other.ptr;
        cb = other.cb;

        if (cb) {
            cb->shared_count.fetch_add(1, std::memory_order_relaxed);
        }

        return *this;
    }

    // Move assignment
    SharedPtr& operator=(SharedPtr&& other) noexcept {
        if (this == &other) return *this;

        release();

        ptr = other.ptr;
        cb = other.cb;

        other.ptr = nullptr;
        other.cb = nullptr;

        return *this;
    }

    // Destructor
    ~SharedPtr() {
        release();
    }

    void release() {
        if (!cb) return;

        if (cb->shared_count.fetch_sub(1, std::memory_order_acq_rel) == 1) {

            cb->destroy_object();

            if (cb->weak_count.load(std::memory_order_acquire) == 0) {
                delete cb;
            }
        }
    }

    // Observers
    T* get() const { return ptr; }

    T& operator*() const { return *ptr; }

    T* operator->() const { return ptr; }

    size_t use_count() const {
        return cb ? cb->shared_count.load(std::memory_order_relaxed) : 0;
    }

    explicit operator bool() const {
        return ptr != nullptr;
    }
};

//////////////////////////////////////////////////////////////
// WeakPtr
//////////////////////////////////////////////////////////////

template<typename T>
class WeakPtr {
private:
    T* ptr;
    ControlBlock* cb;

public:
    WeakPtr() : ptr(nullptr), cb(nullptr) {}

    WeakPtr(const SharedPtr<T>& sp) {
        ptr = sp.ptr;
        cb = sp.cb;

        if (cb) {
            cb->weak_count.fetch_add(1, std::memory_order_relaxed);
        }
    }

    WeakPtr(const WeakPtr& other) {
        ptr = other.ptr;
        cb = other.cb;

        if (cb) {
            cb->weak_count.fetch_add(1, std::memory_order_relaxed);
        }
    }

    ~WeakPtr() {
        if (!cb) return;

        if (cb->weak_count.fetch_sub(1, std::memory_order_acq_rel) == 1) {
            if (cb->shared_count.load(std::memory_order_acquire) == 0) {
                delete cb;
            }
        }
    }

    SharedPtr<T> lock() {
        if (!cb) return SharedPtr<T>();

        size_t count = cb->shared_count.load(std::memory_order_acquire);

        while (count > 0) {
            if (cb->shared_count.compare_exchange_weak(
                    count, count + 1,
                    std::memory_order_acq_rel,
                    std::memory_order_relaxed)) {

                return SharedPtr<T>(ptr, cb);
            }
        }

        return SharedPtr<T>();
    }
};

//////////////////////////////////////////////////////////////
// make_shared
//////////////////////////////////////////////////////////////

template<typename T, typename... Args>
SharedPtr<T> make_shared(Args&&... args) {

    auto* cb = new ControlBlockImpl<T>(std::forward<Args>(args)...);

    T* obj = static_cast<T*>(cb->get_ptr());

    return SharedPtr<T>(obj, cb);
}



#endif /* ENABLESHAREDFROMTHIS_HPP_ */
