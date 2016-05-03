#ifndef UTIL_SIMPLE_BUFFER__HPP
#define UTIL_SIMPLE_BUFFER__HPP

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
 * Authors:  Anatoliy Kuznetsov
 *
 * File Description: Simple (fast) resizable buffer
 *
 */

#include <corelib/ncbistd.hpp>

BEGIN_NCBI_SCOPE


class CSimpleResizeStrategy
{
public:
    static size_t GetNewCapacity(size_t /*cur_capacity*/,
                                 size_t requested_size)
    { return requested_size; }
};

class CAgressiveResizeStrategy
{
public:
    static size_t GetNewCapacity(size_t /*cur_capacity*/,
                                 size_t requested_size)
    {
        size_t  new_size = requested_size + requested_size / 2;

        // Overrun
        if (new_size < requested_size)
            return std::numeric_limits<size_t>::max();
        return new_size;
    }
};

class CPowerOfTwoResizeStrategy
{
public:
    static size_t GetNewCapacity(size_t /*cur_capacity*/,
                                 size_t required_size)
    {
        size_t  new_size = required_size * 2;

        // Overrun
        if (new_size < required_size)
            return std::numeric_limits<size_t>::max();
        return new_size;
    }
};

/// Reallocable memory buffer (no memory copy overhead)
/// Mimics vector<>, without the overhead of explicit initialization of all
/// items
///

template <typename T = unsigned char,
          typename ResizeStrategy = CPowerOfTwoResizeStrategy>
class CSimpleBufferT
{
public:
    typedef T             value_type;
    typedef size_t        size_type;
public:
    explicit CSimpleBufferT(size_type new_size = 0)
    {
        if (new_size) {
            m_Buffer = x_Allocate(new_size);
        } else {
            m_Buffer = 0;
        }
        m_Size = m_Capacity = new_size;
    }
    ~CSimpleBufferT()
    {
        x_Deallocate();
    }

    CSimpleBufferT(const CSimpleBufferT& sb)
    {
        size_type new_size = sb.capacity();
        m_Buffer = x_Allocate(new_size);
        m_Capacity = new_size;
        m_Size = sb.size();
        memcpy(m_Buffer, sb.data(), m_Size*sizeof(value_type));
    }

    CSimpleBufferT& operator=(const CSimpleBufferT& sb)
    {
        if (this != &sb) {
            if (sb.size() <= m_Capacity) {
                if (sb.size() < m_Size) {
                    x_Fill(m_Buffer + sb.size(), 0xcd, m_Capacity - sb.size());
                }
                m_Size = sb.size();
            } else {
                x_Deallocate();
                m_Buffer = x_Allocate(sb.capacity());
                m_Capacity = sb.capacity();
                m_Size = sb.size();
            }
            memcpy(m_Buffer, sb.data(), m_Size*sizeof(value_type));
        }
        return *this;
    }

    CSimpleBufferT& append(const void* buf, size_t len)
    {
        size_t offs = m_Size;

        resize( m_Size + len );
        memcpy( m_Buffer + offs, buf, len );
        return *this;
    }

    size_type size() const { return m_Size; }
    size_type capacity() const { return m_Capacity; }

    void reserve(size_type new_size)
    {
        if (new_size > m_Capacity) {
            value_type* new_buffer = x_Allocate(new_size);
            if (m_Size) {
                memcpy(new_buffer, m_Buffer, m_Size*sizeof(value_type));
            }
            x_Deallocate();
            m_Buffer = new_buffer;
            m_Capacity = new_size;
        }
    }

    void resize(size_type new_size)
    {
        _ASSERT(m_Size <= m_Capacity);
        if (new_size <= m_Capacity) {
            if (new_size < m_Size) {
                x_Fill(m_Buffer + new_size, 0xcd, m_Capacity - new_size);
            }
            m_Size = new_size;
        } else {
            size_t new_capacity =
                ResizeStrategy::GetNewCapacity(m_Capacity,new_size);
            value_type* new_buffer = x_Allocate(new_capacity);
            if (m_Size) {
                memcpy(new_buffer, m_Buffer, m_Size*sizeof(value_type));
            }
            x_Deallocate();
            m_Buffer = new_buffer;
            m_Capacity = new_capacity;
            m_Size = new_size;
        }
    }

    /// Resize the buffer. No data preservation.
    void resize_mem(size_type new_size)
    {
        if (new_size <= m_Capacity) {
            if (new_size < m_Size) {
                x_Fill(m_Buffer + new_size, 0xcd, m_Capacity - new_size);
            }
            m_Size = new_size;
        } else {
            x_Deallocate();
            size_t new_capacity = ResizeStrategy::GetNewCapacity(m_Capacity,new_size);
            m_Buffer = x_Allocate(new_capacity);
            m_Capacity = new_capacity;
            m_Size = new_size;
        }
    }

    void swap(CSimpleBufferT<T>& other)
    {
        std::swap(m_Buffer, other.m_Buffer);
        std::swap(m_Size, other.m_Size);
        std::swap(m_Capacity, other.m_Capacity);
    }

    /// Reserve memory. No data preservation guarantees.
    void reserve_mem(size_type new_size)
    {
        if (new_size > m_Capacity) {
            x_Deallocate();
            m_Buffer = x_Allocate(new_size);
            m_Capacity = new_size;
            x_Fill(m_Buffer, 0xcd, m_Capacity);
        }
    }

    void clear()
    {
        resize(0);
    }

    const value_type& operator[](size_type i) const
    {
        _ASSERT(m_Buffer);
        _ASSERT(i < m_Size);
        return m_Buffer[i];
    }
    value_type& operator[](size_type i)
    {
        _ASSERT(m_Buffer);
        _ASSERT(i < m_Size);
        return m_Buffer[i];
    }

    const value_type* data() const
    {
        return m_Buffer;
    }
    value_type* data()
    {
        return m_Buffer;
    }



private:

#ifdef _DEBUG
    void x_Fill(value_type* buffer, int value, size_t elem)
    {
        memset(buffer, value, elem * sizeof(value_type));
    }
#else
    void x_Fill(value_type*, int, size_t) {}
#endif

    void x_Deallocate()
    {
        if (m_Buffer) {
            x_Fill(m_Buffer, 0xfd, m_Capacity);
            delete [] m_Buffer;
        }
        m_Buffer = NULL;
        m_Size = m_Capacity = 0;
    }

    value_type* x_Allocate(size_t elem)
    {
        value_type* buf = new value_type[elem];
        x_Fill(buf, 0xcd, elem);
        return buf;
    }

private:
    value_type* m_Buffer;
    size_type   m_Size;
    size_type   m_Capacity;
};

typedef CSimpleBufferT<> CSimpleBuffer;

END_NCBI_SCOPE

#endif
