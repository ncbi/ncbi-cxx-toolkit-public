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
    { return requested_size + requested_size / 2; }
};

/// Reallocable memory buffer (no memory copy overhead)
/// Mimics vector<unsigned char>
///

template <typename T = unsigned char,
          typename ResizeStrategy = CSimpleResizeStrategy>
class CSimpleBufferT
{
public:
    typedef T             value_type;
    typedef size_t        size_type;
public:
    explicit CSimpleBufferT(size_type size=0) 
    {
        if (size) {
            m_Buffer = new value_type[size];
        } else {
            m_Buffer = 0;
        }
        m_Size = m_Capacity = size;
    }
    ~CSimpleBufferT() { delete [] m_Buffer; }

    CSimpleBufferT(const CSimpleBufferT& sb) 
    {
        m_Buffer = new value_type[sb.capacity()];
        m_Capacity = sb.capacity();
        m_Size = sb.size();
        memcpy(m_Buffer, sb.data(), m_Size*sizeof(value_type));
    }
    CSimpleBufferT& operator=(const CSimpleBufferT& sb) 
    {
        if (this != &sb) {
            if (sb.size() <= m_Capacity) {
                m_Size = sb.size();
            } else {
                x_Deallocate();
                m_Buffer = new value_type[sb.capacity()];
                m_Capacity = sb.capacity();
                m_Size = sb.size();
            }
            memcpy(m_Buffer, sb.data(), m_Size*sizeof(value_type));
        }
        return *this;
    }

    size_type size() const { return m_Size; }
    size_type capacity() const { return m_Capacity; }

    void reserve(size_type new_size)
    {
        if (new_size > m_Capacity) {
            value_type* new_buffer = new value_type[new_size];
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
        if (new_size <= m_Capacity) {
            m_Size = new_size;
        } else {
            size_t new_capacity = ResizeStrategy::GetNewCapacity(m_Capacity,new_size);
            value_type* new_buffer = new value_type[new_capacity];
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
            m_Size = new_size;
        } else {
            x_Deallocate();
            size_t new_capacity = ResizeStrategy::GetNewCapacity(m_Capacity,new_size);
            m_Buffer = new value_type[new_capacity];
            m_Capacity = new_capacity;
            m_Size = new_size;
        }
    }

    /// Reserve memory. No data preservation guarantees.
    void reserve_mem(size_type new_size)
    {
        if (new_size > m_Capacity) {
            x_Deallocate();
            m_Buffer = new value_type[new_size];
            m_Capacity = new_size;
        }
    }

    void clear()
    {
        x_Deallocate();
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
        _ASSERT(m_Buffer);
        return m_Buffer;
    }
    value_type* data()
    {
        _ASSERT(m_Buffer);
        return m_Buffer;
    }

private:
    void x_Deallocate()
    {
        delete [] m_Buffer; 
        m_Buffer = NULL;
        m_Size = m_Capacity = 0;
    }
private:
    value_type* m_Buffer;
    size_type   m_Size;
    size_type   m_Capacity;
};

typedef CSimpleBufferT<> CSimpleBuffer;

END_NCBI_SCOPE

#endif 
