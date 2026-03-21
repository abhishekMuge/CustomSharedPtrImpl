
#ifndef MAKESHAREDIMPL_HPP_
#define MAKESHAREDIMPL_HPP_
#include <atomic>

#include <iostream>
#include <atomic>
#include <utility>

//////////////////////////////////////////////////////////////
// Control Block (Base)
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
// Control Block Implementation (Inline Object)
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

    // Private constructor used by make_shared & WeakPtr
    SharedPtr(T* p, ControlBlock* control) : ptr(p), cb(control) {}

public:
    template<typename U>
    friend class WeakPtr;

    template<typename U, typename... Args>
    friend SharedPtr<U> make_shared(Args&&...);

    // Default / raw pointer constructor
    explicit SharedPtr(T* p = nullptr) : ptr(p) {
        if (p) {
            // fallback control block (no inline storage)
            struct DefaultCB : ControlBlock {
                T* obj;
                DefaultCB(T* p) : obj(p) {}
                void destroy_object() override { delete obj; }
                void* get_ptr() override { return obj; }
            };

            cb = new DefaultCB(p);
        } else {
            cb = nullptr;
        }
    }

    // Copy constructor
    SharedPtr(const SharedPtr& other) {
        ptr = other.ptr;
        cb = other.cb;

        if (cb) {
            cb->shared_count.fetch_add(1, std::memory_order_relaxed);
        }
    }

    // Move constructor
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

            // Destroy object
            cb->destroy_object();

            // Delete control block if no weak refs
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
// make_shared (Single Allocation)
//////////////////////////////////////////////////////////////

template<typename T, typename... Args>
SharedPtr<T> make_shared(Args&&... args) {

    auto* cb = new ControlBlockImpl<T>(std::forward<Args>(args)...);

    T* obj = static_cast<T*>(cb->get_ptr());

    return SharedPtr<T>(obj, cb);
}

#endif /* MAKESHAREDIMPL_HPP_ */
