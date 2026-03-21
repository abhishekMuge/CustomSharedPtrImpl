
#ifndef THREADSAFESHAREDPTR_HPP_
#define THREADSAFESHAREDPTR_HPP_
#include <atomic>

struct ControlBlock {
    std::atomic<size_t> shared_count;
    std::atomic<size_t> weak_count;

    ControlBlock() : shared_count(1), weak_count(0) {}
};

template<typename T>
class SharedPtr {
private:
    T* ptr;
    ControlBlock* cb;

public:
    template<typename U>
    friend class WeakPtr;

    explicit SharedPtr(T* p = nullptr) : ptr(p) {
        if (p) cb = new ControlBlock();
        else cb = nullptr;
    }

private:
    SharedPtr(T* p, ControlBlock* control) : ptr(p), cb(control) {}

public:
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

    // Destructor
    ~SharedPtr() {
        release();
    }

    void release() {
        if (!cb) return;

        // decrement shared count
        if (cb->shared_count.fetch_sub(1, std::memory_order_acq_rel) == 1) {
            // last owner → delete object
            delete ptr;

            // now handle control block
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
            // if failed, count is updated → retry
        }

        return SharedPtr<T>(); // expired
    }
};




#endif /* THREADSAFESHAREDPTR_HPP_ */
