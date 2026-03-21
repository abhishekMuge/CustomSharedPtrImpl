
#ifndef CUSTOMSHAREDPTR_HPP_
#define CUSTOMSHAREDPTR_HPP_

#include <bits/stdc++.h>

struct ControlBlock {
    size_t shared_count;
    size_t weak_count;

    ControlBlock() : shared_count(1), weak_count(0) {}
};

template<typename T>
class SharedPtr {
	template<typename U>
	friend class WeakPtr;
private:
    T* ptr;
    ControlBlock* cb;
private:
    SharedPtr(T* p, ControlBlock* control) : ptr(p), cb(control) {}
public:
    // Constructor
    explicit SharedPtr(T* p = nullptr) : ptr(p) {
        if (p) {
            cb = new ControlBlock();
        } else {
            cb = nullptr;
        }
    }

    // Copy Constructor
    SharedPtr(const SharedPtr& other) {
        ptr = other.ptr;
        cb = other.cb;

        if (cb) {
            cb->shared_count++;
        }
    }

    // Move Constructor
    SharedPtr(SharedPtr&& other) noexcept {
        ptr = other.ptr;
        cb = other.cb;

        other.ptr = nullptr;
        other.cb = nullptr;
    }

    // Copy Assignment
    SharedPtr& operator=(const SharedPtr& other) {
        if (this == &other) return *this;

        release();

        ptr = other.ptr;
        cb = other.cb;

        if (cb) {
            cb->shared_count++;
        }

        return *this;
    }

    // Move Assignment
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

        cb->shared_count--;

        if (cb->shared_count == 0) {
            delete ptr;

            if (cb->weak_count == 0) {
                delete cb;
            }
        }
    }

    // Observers
    T* get() const { return ptr; }

    T& operator*() const { return *ptr; }

    T* operator->() const { return ptr; }

    size_t use_count() const {
        return cb ? cb->shared_count : 0;
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

    // From SharedPtr
    WeakPtr(const SharedPtr<T>& sp) {
        ptr = sp.get();
        cb = sp.cb;

        if (cb) {
            cb->weak_count++;
        }
    }

    // Copy
    WeakPtr(const WeakPtr& other) {
        ptr = other.ptr;
        cb = other.cb;

        if (cb) {
            cb->weak_count++;
        }
    }

    // Destructor
    ~WeakPtr() {
        if (cb) {
            cb->weak_count--;

            if (cb->shared_count == 0 && cb->weak_count == 0) {
                delete cb;
            }
        }
    }

    // Convert to SharedPtr
    SharedPtr<T> lock() {
        if (cb && cb->shared_count > 0) {
            cb->shared_count++;
            return SharedPtr<T>(ptr, cb);
        }
        return SharedPtr<T>();
    }
};


#endif /* CUSTOMSHAREDPTR_HPP_ */
