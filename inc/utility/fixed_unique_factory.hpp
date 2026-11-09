//
// Created by Jared on 5/23/2026.
//

#ifndef WEATHER_PARTICLE_MEM_H
#define WEATHER_PARTICLE_MEM_H

#include <etl/memory.h>
#include <etl/variant_pool.h>
#include "typelist.hpp"

template <class T> class pool_ptr
{
    template <class> friend class pool_ptr;
    T *ptr_ = nullptr;
    void (*rel_)(void *) = nullptr; // type-erased "give this back to its pool"
public:
    pool_ptr() = default;
    pool_ptr(T *p, void (*rel)(void *)) : ptr_(p), rel_(rel) {}
    pool_ptr(pool_ptr &&o) noexcept : ptr_(o.ptr_), rel_(o.rel_)
    {
        o.ptr_ = nullptr;
    }
    template <class U, class = std::enable_if_t<std::is_convertible_v<U *, T *>>> pool_ptr(pool_ptr<U> &&o) noexcept : ptr_(o.ptr_), rel_(o.rel_)
    {
        o.ptr_ = nullptr;
    }

    pool_ptr &operator=(pool_ptr &&o) noexcept
    {
        if (this != &o)
        {
            reset();
            ptr_ = o.ptr_;
            rel_ = o.rel_;
            o.ptr_ = nullptr;
        }
        return *this;
    }
    pool_ptr(const pool_ptr &) = delete;
    pool_ptr &operator=(const pool_ptr &) = delete;
    ~pool_ptr()
    {
        reset();
    }
    void reset()
    {
        if (ptr_)
        {
            rel_(ptr_);
            ptr_ = nullptr;
        }
    }
    T *get() const
    {
        return ptr_;
    }
    T *operator->() const
    {
        return ptr_;
    }
    T &operator*() const
    {
        return *ptr_;
    }
    explicit operator bool() const
    {
        return ptr_ != nullptr;
    }
};

template <size_t num_unique_instances, typename... Types> class FixedFactory
{

    inline static etl::variant_pool<num_unique_instances, Types...> pool_of;

public:
    template <class T, typename... Args> static auto make_unique(Args &&...args)
    {
        T *p = pool_of.template create<T>(etl::forward<Args>(args)...);
        return pool_ptr<T>(p,
                           [](void *q)
                           {
                               pool_of.destroy(static_cast<T *>(q));
                           });
    }

    // Returns a non-owning pool_ptr to an already-existing object.
    // The object's lifetime must exceed that of the returned pool_ptr.
    // Use this when the caller manages the object (e.g. seg_properties_.get_gravity()).
    template <class T> static pool_ptr<T> borrow(T &obj)
    {
        return pool_ptr<T>(&obj, [](void *) {});
    }
};

template <typename List> struct get_size;

template <typename... Types> struct get_size<tl::type_list<Types...>>
{
    static constexpr size_t N = sizeof...(Types);
};

// Primary template — intentionally undefined, only the specialization is valid
template <typename List> struct Unpack;

// Specialization that captures Types... from the list
template <typename... Types> struct Unpack<tl::type_list<Types...>>
{
    // Types... is now available as a pack — use it however you need
    template <template <typename...> class Target> using into = Target<Types...>;
};

template <size_t N, typename List> struct UnpackFactory;

template <size_t N, typename... Types> struct UnpackFactory<N, tl::type_list<Types...>>
{
    using type = FixedFactory<N, Types...>;
};

#endif // WEATHER_PARTICLE_MEM_H