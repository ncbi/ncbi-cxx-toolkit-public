#ifndef MPMC_NW__HPP
#define MPMC_NW__HPP

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
 * Authors: Dmitri Dmitrienko
 *
 * File Description:
 *
 *  defines MPMC (multiple producers multiple consumers) queue class
 *  inspired by another no-lock queue implementation 
 *  published at http://www.1024cores.net
 *
 */

#include <atomic>
#include <array>
#include <stdexcept>

#define CPU_CACHE_LINE_SZ 64L

template<typename T, size_t SZ = 1024>
class mpmc_bounded_queue {
private:
    static constexpr const int CL_PAD_SZ = CPU_CACHE_LINE_SZ - sizeof(std::atomic<size_t>);
    static constexpr const int CL_DATA_PAD_SZ = CPU_CACHE_LINE_SZ - sizeof(T) - sizeof(std::atomic<size_t>);
    using cacheline_pad_t = std::array<char, CL_PAD_SZ>;
    using cacheline_pad_data_t = std::array<char, CL_DATA_PAD_SZ>;

    typedef struct cell_tag {
        std::atomic<size_t> m_sequence;
        T m_data;
        cacheline_pad_data_t m_pad;
    } cell_t;

    cacheline_pad_t         m_pad0;
    std::array<cell_t, SZ>  m_buffer;
    cacheline_pad_t         m_pad1;
    std::atomic<size_t>     m_push_pos;
    cacheline_pad_t         m_pad2;
    std::atomic<size_t>     m_pop_pos;
    cacheline_pad_t         m_pad3;

public:
    mpmc_bounded_queue(const mpmc_bounded_queue&) = delete;
    mpmc_bounded_queue(mpmc_bounded_queue&&) = delete;
    mpmc_bounded_queue& operator=(const mpmc_bounded_queue&) = delete;
    mpmc_bounded_queue& operator=(mpmc_bounded_queue&&) = delete;
    mpmc_bounded_queue() {
        static_assert((SZ & (SZ - 1)) == 0, "SZ template parameter value must be power of two");
        static_assert(sizeof(intptr_t) == sizeof(size_t), "All of sudden size_t is of different size than intptr_t, you've to update sources");
        static_assert(sizeof(T) + sizeof(std::atomic<size_t>) <= CPU_CACHE_LINE_SZ, "mpmc_bounded_queue template should not hold this 'T' type, consider using smart pointers");
        clear();
    }

    void clear() {
        size_t i = 0;
        for (auto & it : m_buffer) {
            it.m_sequence.store(i++, std::memory_order_relaxed);
            it.m_data.~T();
        }
        m_push_pos.store(0, std::memory_order_relaxed);
        m_pop_pos.store(0, std::memory_order_relaxed);
    }

    template<typename U>
    bool push(U&& data) {
        static_assert(std::is_same<typename std::decay<U>::type, typename std::decay<T>::type>::value, "U must be the same as T");
        cell_t* cell;
        size_t pos = m_push_pos.load(std::memory_order_relaxed);
        for (;;) {
            cell = &m_buffer[pos & (SZ - 1)];
            size_t seq = cell->m_sequence.load(std::memory_order_acquire);
            intptr_t diff = (intptr_t)seq - (intptr_t)pos;
            if (diff == 0) {
                if (m_push_pos.compare_exchange_weak(pos, pos + 1, std::memory_order_relaxed))
                    break;
            }
            else if (diff < 0)
                return false;
            else
                pos = m_push_pos.load(std::memory_order_relaxed);
        }
        cell->m_data = std::forward<U>(data);
        cell->m_sequence.store(pos + 1, std::memory_order_release);
        return true;
    }

    bool push_move(T& data) {
        cell_t* cell;
        size_t pos = m_push_pos.load(std::memory_order_relaxed);
        for (;;) {
            cell = &m_buffer[pos & (SZ - 1)];
            size_t seq = cell->m_sequence.load(std::memory_order_acquire);
            intptr_t diff = (intptr_t)seq - (intptr_t)pos;
            if (diff == 0) {
                if (m_push_pos.compare_exchange_weak(pos, pos + 1, std::memory_order_relaxed))
                    break;
            }
            else if (diff < 0)
                return false;
            else
                pos = m_push_pos.load(std::memory_order_relaxed);
        }
        cell->m_data = std::move(data);
        cell->m_sequence.store(pos + 1, std::memory_order_release);
        return true;
    }

    template<typename U>
    bool pop(U&& data) {
        static_assert(std::is_same<typename std::decay<U>::type, typename std::decay<T>::type>::value, "U must be the same as T");
        cell_t* cell;
        size_t pos = m_pop_pos.load(std::memory_order_relaxed);
        for (;;) {
            cell = &m_buffer[pos & (SZ - 1)];
            size_t seq = cell->m_sequence.load(std::memory_order_acquire);
            intptr_t diff = (intptr_t)seq - (intptr_t)(pos + 1);
            if (diff == 0) {
                if (m_pop_pos.compare_exchange_weak(pos, pos + 1, std::memory_order_relaxed))
                    break;
            }
            else if (diff < 0)
                return false;
            else
                pos = m_pop_pos.load(std::memory_order_relaxed);
        }
        data = std::forward<U>(cell->m_data);
        cell->m_sequence.store(pos + SZ, std::memory_order_release);
        return true;
    }

    bool pop_move(T& data) {
        cell_t* cell;
        size_t pos = m_pop_pos.load(std::memory_order_relaxed);
        for (;;) {
            cell = &m_buffer[pos & (SZ - 1)];
            size_t seq = cell->m_sequence.load(std::memory_order_acquire);
            intptr_t diff = (intptr_t)seq - (intptr_t)(pos + 1);
            if (diff == 0) {
                if (m_pop_pos.compare_exchange_weak(pos, pos + 1, std::memory_order_relaxed))
                    break;
            }
            else if (diff < 0)
                return false;
            else
                pos = m_pop_pos.load(std::memory_order_relaxed);
        }
        data = std::move(cell->m_data);
        cell->m_sequence.store(pos + SZ, std::memory_order_release);
        return true;
    }
};

#endif
