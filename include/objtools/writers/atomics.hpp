#ifndef __NCBI_WRITERS_ATOMICS_FILE_HPP__
#define __NCBI_WRITERS_ATOMICS_FILE_HPP__
/*  $Id$
* ===========================================================================
*
*                            PUBLIC DOMAIN NOTICE
*               National Center for Biotechnology Information
*
*  This software/database is a "United States Government Work" under the
*  terms of the United States Copyright Act.  It was written as part of
*  the author's official duties as a United States Government employee and
*  thus cannot be copyrighted.  This software/database is freely available
*  to the public for use. The National Library of Medicine and the U.S.
*  Government have not placed any restriction on its use or reproduction.
*
*  Although all reasonable efforts have been taken to ensure the accuracy
*  and reliability of the software and data, the NLM and the U.S.
*  Government do not and cannot warrant the performance or results that
*  may be obtained by using this software or data. The NLM and the U.S.
*  Government disclaim all warranties, express or implied, including
*  warranties of performance, merchantability or fitness for any particular
*  purpose.
*
*  Please cite the author in any work or product based on this material.
*
* ===========================================================================
*
* Authors:  Sergiy Gotvyanskyy
*
* File Description:
*   Various atomic structures
*
*/

#include <memory>
#include <atomic>
#include <functional>

namespace ncbi {

// never blocking stack
template<typename _T>
class TAtomicStack
{
public:
    using value_type = _T;

    struct TNode;
    using pointer_type = TNode*;
    using atomic_pointer = std::atomic<pointer_type>;
    static_assert(atomic_pointer::is_always_lock_free);

    struct TNode
    {
    friend class TAtomicStack;
    private:
        atomic_pointer m_next{nullptr};
        value_type     m_data;
    public:
        operator value_type& () {
            return m_data;
        }
        value_type& operator*() {
            return m_data;
        }
        value_type* operator->() {
            return &m_data;
        }
    };

    TAtomicStack() = default;

    pointer_type pop_front() {
        pointer_type expected;
        pointer_type next;
        do {
            expected = m_head.load();
            if (expected == nullptr)
                break;

            next = expected->m_next;
        } while (!m_head.compare_exchange_strong(expected, next));

        if (expected) {
            m_size--;
            expected->m_next = nullptr;
        }
        return expected;
    }
    void push_front(pointer_type ptr) {
        if (!ptr) return;

        m_size++;
        pointer_type expected;
        do {
            expected = m_head.load();
            ptr->m_next = expected;
        } while (!m_head.compare_exchange_strong(expected, ptr));
    }

    size_t size() const { return m_size; }
private:
    std::atomic<size_t> m_size = 0;
    atomic_pointer      m_head = nullptr;
};

// non-blocking resource pool
// blocking&waiting only happen when memory allocated/deallocated and
// when constructors and initializers may block on their data racing
// the structure itself is atomic
template<typename _T>
class TResourcePool
{
protected:
    friend struct TDeleter;
    struct TDeleter;

public:
    using _MyType = TResourcePool<_T>;
    using value_type = _T;

    using TStack = TAtomicStack<value_type>;
    using TNode  = typename TStack::TNode;
    using init_func = std::function<void(value_type&)>;

    // Note, that members of TNode are private, only casting operator value_type& is available
    using TUniqPointer = std::unique_ptr<TNode, TDeleter>;

    // _reserved specifies how many resources will be left in the backyard
    TResourcePool() = default;
    TResourcePool(size_t _reserved) : TResourcePool(_reserved, {}, {}) {}
    TResourcePool(size_t _reserved, init_func _init, init_func _deinit)
        : m_init {_init},
          m_deinit {_deinit}
    {
        SetReserved(_reserved);
    }

    ~TResourcePool() {
        Purge();
    }

    // init and deinit will be called when a resource is allocated or returned back
    // constructors and destructors will be called only when memory is allocated/deallocated
    // init/deinit must not throw exceptions
    void SetInitFunc(init_func _init, init_func _deinit) {
        m_init = _init; m_deinit = _deinit;
    }

    // there is no Deallocate publicly available
    // TUniqPointer has its own deleter that will call protected Deallocate method
    TUniqPointer Allocate() {
        auto ptr = m_stack.pop_front();
        if (ptr == nullptr) {
            m_size++;
            ptr = new TNode;
        }

        if (ptr && m_init)
            m_init(*ptr); // must be able to perform casting to value_type&

        return {ptr, TDeleter{this}};
    }

    // Specify how many resources should be left in the backyard
    void SetReserved(size_t num) { // atomic operation
        m_reserved_size = num;
    }

    size_t size() const { return m_size; }

    void Purge() { // purge all reserved data
        while (m_size) {
            TNode* ptr = m_stack.pop_front();
            if (ptr) {
                if (m_deinit)
                    m_deinit(*ptr);  // must be able to perform casting to value_type&
                delete ptr;
                m_size--;
            }
        }
    }

protected:
    struct TDeleter
    {
        _MyType* owner = nullptr;
        void operator()(TNode* ptr) const
        {
            if (owner && ptr)
                owner->Deallocate(ptr);
        }
    };

    void Deallocate(TNode* ptr) {
        if (m_deinit)
            m_deinit(*ptr);  // must be able to perform casting to value_type&

        if (m_stack.size() >= m_reserved_size) {
            // not enough space
            delete ptr;
            m_size--;
        } else {
            m_stack.push_front(ptr);
        }
    }

    TStack              m_stack;
    init_func           m_init, m_deinit;
    std::atomic<size_t> m_size{0};
    std::atomic<size_t> m_reserved_size{std::numeric_limits<size_t>::max()};
};

} // namespace ncbi

#endif
