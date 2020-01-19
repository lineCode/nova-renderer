#pragma once

#include <memory_resource>

#include "nova_renderer/memory/bytes.hpp"
#include "nova_renderer/memory/host_memory_resource.hpp"

namespace nova::mem {
    template <typename AllocatedType = std::byte>
    class AllocatorHandle : public std::pmr::polymorphic_allocator<AllocatedType> {
    public:
        AllocatorHandle() = default;

        explicit AllocatorHandle(std::pmr::memory_resource* memory);

        AllocatorHandle(const AllocatorHandle& other) = delete;
        AllocatorHandle& operator=(const AllocatorHandle& other) = delete;

        AllocatorHandle(AllocatorHandle&& old) noexcept = default;
        AllocatorHandle& operator=(AllocatorHandle&& old) noexcept = default;

        virtual ~AllocatorHandle() = default;

        /*!
         * \brief Allocates and constructs an object of the specified type
         *
         * Would be nice if C++ came with this method, but nope not yet
         */
        template <typename... Args>
        AllocatedType* new_object(Args&&... args);

        /*!
         * \brief Allocates an object of a different type than what this allocator normally created
         *
         * Intended use case is that you have a byte allocator that you allocate a few different object types from
         */
        template <typename ObjectType>
        ObjectType* allocate_object();

        /*!
         * \brief Allocates a shared pointer that uses this allocator to allocate its internal memory
         *
         * \param deleter A functor that you want to use to delete the object
         * \param args Any arguments to forward to the new object's constructor
         *
         * \return A shared pointer to the new object
         */
        template <typename ObjectType,
                  typename DeleterType,
                  typename... Args>
        std::shared_ptr<ObjectType> allocate_shared(DeleterType deleter, Args&&... args);

        /*!
         * \brief Allocates and constructs an object of a different type
         *
         * Intended use case is that you have a byte allocator that you use to create objects of different types
         */
        template <typename ObjectType, typename... Args>
        ObjectType* new_other_object(Args&&... args);

        /*!
         * \brief Creates a new allocator which will allocate from this allocator's memory resource
         */
        template <typename ObjectType = AllocatedType>
        AllocatorHandle<ObjectType>* create_suballocator();
    };

    template <typename AllocatedType = std::byte>
    AllocatorHandle<AllocatedType> get_malloc_allocator() {
        return AllocatorHandle<AllocatedType>(std::pmr::new_delete_resource());
    }

    template <typename AllocatedType>
    AllocatorHandle<AllocatedType>::AllocatorHandle(std::pmr::memory_resource* memory)
        : std::pmr::polymorphic_allocator<AllocatedType>(memory) {}

    template <typename AllocatedType>
    template <typename... Args>
    AllocatedType* AllocatorHandle<AllocatedType>::new_object(Args&&... args) {
        auto* mem = this->allocate(1);
        this->construct(mem, args...);
        return mem;
    }

    template <typename AllocatedType>
    template <typename ObjectType>
    ObjectType* AllocatorHandle<AllocatedType>::allocate_object() {
        auto* mem = this->allocate(sizeof(ObjectType));
        return reinterpret_cast<ObjectType*>(mem);
    }

    template <typename AllocatedType>
    template <typename ObjectType, typename DeleterType, typename... Args>
    std::shared_ptr<ObjectType> AllocatorHandle<AllocatedType>::allocate_shared(DeleterType deleter, Args&&... args) {
        const auto ptr = allocate_object<ObjectType>(std::forward(args)...);
        return std::shared_ptr<ObjectType>(ptr, deleter, std::pmr::polymorphic_allocator<std::byte>(this->resource()));
    }

    template <typename AllocatedType>
    template <typename ObjectType, typename... Args>
    ObjectType* AllocatorHandle<AllocatedType>::new_other_object(Args&&... args) {
        auto* mem = this->allocate(sizeof(ObjectType));
        return new(mem) ObjectType(std::forward<Args>(args)...);
    }

    template <typename AllocatedType>
    template <typename ObjectType>
    AllocatorHandle<ObjectType>* AllocatorHandle<AllocatedType>::create_suballocator() {
        return this->template new_other_object<AllocatorHandle<ObjectType>>(this->resource());
    }
} // namespace nova::mem