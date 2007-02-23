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


/// Reallocable memory buffer (no memory copy overhead)
/// Mimics vector<unsigned char>
///
class CSimpleBuffer
{
public:
    typedef unsigned char value_type;
    typedef size_t        size_type;
public:
    CSimpleBuffer(size_type size=0) 
    {
        if (size) {
            m_Buffer = new value_type[size];
        } else {
            m_Buffer = 0;
        }
        m_Size = m_Capacity = size;
    }
    ~CSimpleBuffer() { delete [] m_Buffer; }

    size_type size() const { return m_Size; }
    size_type capacity() const { return m_Capacity; }

    void reserve(size_type new_size)
    {
        if (new_size > m_Capacity) {
            value_type* new_buffer = new value_type[new_size];
            if (m_Size) {
                memcpy(new_buffer, m_Buffer, m_Size);
            }
            x_Deallocate();
            m_Buffer = new_buffer;
            m_Capacity = new_size;
        }
    }

    void resize(size_type new_size)
    {
        if (new_size < m_Capacity) {
            m_Size = new_size;
        } else {
            value_type* new_buffer = new value_type[new_size];
            if (m_Size) {
                memcpy(new_buffer, m_Buffer, m_Size);
            }
            x_Deallocate();
            m_Buffer = new_buffer;
            m_Size = m_Capacity = new_size;
        }
    }

    void clear()
    {
        x_Deallocate();
    }

    value_type& operator[](size_type i) const
    {
        _ASSERT(m_Buffer);
        _ASSERT(i < m_Size);
        return m_Buffer[i];
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
    size_type      m_Size;
    size_type      m_Capacity;
};



END_NCBI_SCOPE

#endif 
