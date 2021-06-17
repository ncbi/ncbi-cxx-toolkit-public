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

template<typename T, size_t SZ = 1024>
class CMPMCQueue
{
public:
    CMPMCQueue()
    {
        static_assert((SZ & (SZ - 1)) == 0,
                "SZ template parameter value must be power of two");

        static_assert(sizeof(intptr_t) == sizeof(size_t),
                "All of sudden size_t is of different size than intptr_t, you've to update sources");

        static_assert(sizeof(T) + sizeof(std::atomic<size_t>) <= kCpuCachelineSize,
                "CMPMCQueue template should not hold this 'T' type, consider using smart pointers");

        size_t i = 0;

        for (auto& it : m_Buffer) {
            it.sequence.store(i++, std::memory_order_relaxed);
            it.data.~T();
        }

        m_PushPos.store(0, std::memory_order_relaxed);
        m_PopPos.store(0, std::memory_order_relaxed);
    }

    bool PushMove(T& data)
    {
        CCell* cell;
        size_t pos = m_PushPos.load(std::memory_order_relaxed);

        for (;;) {
            cell = &m_Buffer[pos & (SZ - 1)];
            size_t seq = cell->sequence.load(std::memory_order_acquire);
            intptr_t diff = (intptr_t)seq - (intptr_t)pos;

            if (diff == 0) {
                if (m_PushPos.compare_exchange_weak(pos, pos + 1, std::memory_order_relaxed))
                    break;

            } else if (diff < 0)
                return false;

            else
                pos = m_PushPos.load(std::memory_order_relaxed);
        }

        cell->data = std::move(data);
        cell->sequence.store(pos + 1, std::memory_order_release);
        return true;
    }

    bool PopMove(T& data)
    {
        CCell* cell;
        size_t pos = m_PopPos.load(std::memory_order_relaxed);

        for (;;) {
            cell = &m_Buffer[pos & (SZ - 1)];
            size_t seq = cell->sequence.load(std::memory_order_acquire);
            intptr_t diff = (intptr_t)seq - (intptr_t)(pos + 1);

            if (diff == 0) {
                if (m_PopPos.compare_exchange_weak(pos, pos + 1, std::memory_order_relaxed))
                    break;

            } else if (diff < 0)
                return false;

            else
                pos = m_PopPos.load(std::memory_order_relaxed);
        }

        data = std::move(cell->data);
        cell->sequence.store(pos + SZ, std::memory_order_release);
        return true;
    }

private:
    static constexpr const int kCpuCachelineSize = 64;
    static constexpr const int kCachelineSize = kCpuCachelineSize - sizeof(std::atomic<size_t>);
    static constexpr const int kCachelineDataPadSize = kCpuCachelineSize - sizeof(T) - sizeof(std::atomic<size_t>);

    using TCachelinePad = std::array<char, kCachelineSize>;
    using TCachelineDataPad = std::array<char, kCachelineDataPadSize>;

    struct CCell
    {
        std::atomic<size_t> sequence;
        T data;
        TCachelineDataPad pad;
    };

    TCachelinePad         m_Pad0;
    std::array<CCell, SZ> m_Buffer;
    TCachelinePad         m_Pad1;
    std::atomic<size_t>   m_PushPos;
    TCachelinePad         m_Pad2;
    std::atomic<size_t>   m_PopPos;
    TCachelinePad         m_Pad3;
};

#endif
