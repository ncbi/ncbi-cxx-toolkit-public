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
 * Author:  Vladimir Soussov
 *
 * File Description:  RAM storage implementation
 *
 */

#include <ncbi_pch.hpp>
#include <dbapi/error_codes.hpp>
#include "memory_store.hpp"
#include <string.h>


#define NCBI_USE_ERRCODE_X   Dbapi_DrvrMemStore


BEGIN_NCBI_SCOPE


CMemStore::SMemBlock* CMemStore::x_AddBlock(void)
{
    SMemBlock* n_blk = new SMemBlock;

    if ( !n_blk )
        return 0;

    n_blk->body = new char[m_BlockSize];
    if ( !n_blk->body ) {
        delete n_blk;
        return 0;
    }

    n_blk->next       = 0;
    n_blk->free_space = m_BlockSize;
    n_blk->prev       = m_Last;

    if (m_First) {
        m_Last->next = n_blk;
    } else {
        m_First = m_Current = n_blk;
    }

    m_Last = n_blk;

    return n_blk;
}


size_t CMemStore::Append(const void* buff, size_t size)
{
    if (!buff  ||  !size)
        return 0;

    if (!m_Last  ||  !m_Last->free_space) {
        if ( !x_AddBlock() )
            return 0;
    }

    TSize f_free;
    TSize n = 0;
    char* b = (char*) buff;
    if(size > kMax_BlobSize) size= kMax_BlobSize;
    TSize nof_bytes = (TSize) size;

    while (nof_bytes > 0) {
        f_free = m_BlockSize - m_Last->free_space;

        if (nof_bytes <= m_Last->free_space) {
            // we have enough space in last block
            memcpy(&m_Last->body[f_free], b+n, nof_bytes);
            m_Last->free_space -= nof_bytes;
            n                  += nof_bytes;
            break;
        }
        // space in this block is insufficient

        memcpy(&m_Last->body[f_free], b + n, m_Last->free_space);
        n         += m_Last->free_space;
        nof_bytes -= m_Last->free_space;
        m_Last->free_space = 0;
        if ( !x_AddBlock() )
            break;
    }
    m_Size += n;
    return (size_t) n;
}


size_t CMemStore::Read(void* buff, size_t size)
{

    if (!m_Current  ||  !buff  ||  !size)
        return 0;

    if(size > kMax_BlobSize) size= kMax_BlobSize;

    TSize n = 0;
    char* b = (char*) buff;

    for (TSize nof_bytes = (TSize) size;  nof_bytes > 0; ) {
        TSize f_free = m_BlockSize - m_Current->free_space;

        if ((m_BlockPos + nof_bytes) <= f_free) {
            // we have all needed bytes in this block
            memcpy(b, &m_Current->body[m_BlockPos], nof_bytes);
            m_BlockPos += nof_bytes;
            n += nof_bytes;
            if (m_BlockPos >= f_free) {
                // we have read all bytes from this block
                m_Current = m_Current->next;
                m_BlockPos = 0;
            }
            break;
        }
        // we can read just a  part from this block
        TSize k = f_free - m_BlockPos;
        memcpy(b, &m_Current->body[m_BlockPos], k);
        n         += k;
        b         += k;
        nof_bytes -= k;
        m_BlockPos = 0;

        m_Current = m_Current->next;
        if ( !m_Current )
            break;
    }

    m_Pos += n;
    return (size_t) n;
}


size_t CMemStore::Peek(void* buff, size_t size) const
{
    // Work with a partial shallow copy to avoid code duplication.
    // (No need for m_First, m_Last, or m_Size.)
    CMemStore ms2(m_BlockSize);
    ms2.m_Current  = m_Current;
    ms2.m_Pos      = m_Pos;
    ms2.m_BlockPos = m_BlockPos;
    return ms2.Read(buff, size);
}


CMemStore::TSize CMemStore::x_SeekCURR(CMemStore::TSize offset)
{
    if ( !m_Current )
        return x_SeekTAIL(offset);

    if (offset == 0)
        return m_Pos;

    if (offset <= -m_Pos)
        return x_SeekHEAD(0);

    if (offset > 0) {
        // go toward the tail
        while (offset > 0) {
            TSize n = m_BlockSize - m_Current->free_space;

            if ((m_BlockPos + offset) < n) {
                // we have to move inside this block
                m_BlockPos += offset;
                m_Pos      += offset;
                break;
            }
            // we have to move outside the block
            n -= m_BlockPos;
            m_Pos += n;
            m_BlockPos = 0;
            m_Current = m_Current->next;
            if (!m_Current)
                break;
            offset -= n;
        }
    }
    else {
        // go toward the head
        while (offset < 0) {
            if ((m_BlockPos + offset) >= 0) {
                // we have to move inside this block
                m_BlockPos += offset;
                m_Pos      += offset;
                break;
            }
            // we have to move outside the block
            m_Pos  -= m_BlockPos + 1;
            offset += m_BlockPos + 1;
            m_Current = m_Current->prev;
            m_BlockPos = m_BlockSize - (m_Current->free_space + 1);
        }
    }

    return m_Pos;
}


CMemStore::TSize CMemStore::x_SeekHEAD(CMemStore::TSize offset)
{
    if (offset <= 0) {
        m_Current  = m_First;
        m_Pos      = 0;
        m_BlockPos = 0;
        return 0;
    }

    if (offset == m_Pos)
        return m_Pos;

    if (!m_Current  ||  (offset < m_Pos  &&  offset < m_Pos - offset)) {
        x_SeekHEAD(0);
        return x_SeekCURR(offset);
    }

    return x_SeekCURR(offset - m_Pos);
}


CMemStore::TSize CMemStore::x_SeekTAIL(CMemStore::TSize offset)
{
    if (offset >= 0) {
        m_BlockPos = 0;
        m_Current  = 0;
        m_Pos      = m_Size;
        return m_Pos;
    }

    return x_SeekHEAD(m_Size + offset);
}


long CMemStore::Seek(long offset, EWhence whence)
{
    if ( !m_Last )
        return -1;

    switch (whence) {
    case eHead:
        return (long) x_SeekHEAD((TSize) offset);
    case eTail:
        return (long) x_SeekTAIL((TSize) offset);
    case eCurr:
        return (long) x_SeekCURR((TSize) offset);
    }

    return -1;  // error
}


size_t CMemStore::Write(const void* buff, size_t size)
{
    if (!buff  ||  !size)
        return 0;

    if(size > kMax_BlobSize) size= kMax_BlobSize;

    char* b         = (char*) buff;
    TSize nof_bytes = (TSize) size;

    TSize n = 0;

    if ( m_Current ) {
        while (nof_bytes > 0) {
            TSize f_free = m_BlockSize - m_Current->free_space;

            if ((m_BlockPos + nof_bytes) <= f_free) {
                // we have all needed bytes in this block
                memcpy(&m_Current->body[m_BlockPos], b + n, nof_bytes);
                m_BlockPos += nof_bytes;
                n          += nof_bytes;
                nof_bytes = 0;
                if (m_BlockPos >= f_free) {
                    // we have written all bytes to this block
                    m_Current = m_Current->next;
                    m_BlockPos = 0;
                }
                break;
            }

            // we can write just a part to this block
            TSize k = f_free - m_BlockPos;
            memcpy(&m_Current->body[m_BlockPos], b + n, k);
            n         += k;
            nof_bytes -= k;
            m_BlockPos = 0;

            m_Current = m_Current->next;
            if ( !m_Current )
                break;
        }
    }

    if (nof_bytes > 0) {
        n += static_cast<TSize>(Append(b + n, nof_bytes));
        x_SeekTAIL(0);
    }
    else {
        m_Pos += n;
    }

    return n;
}


size_t CMemStore::Truncate(size_t size)
{
    if(size > kMax_BlobSize) size= kMax_BlobSize;

    TSize nof_bytes = (TSize) size;

    if (nof_bytes >= m_Size) {
        for ( ;  m_Last != NULL;  m_Last = m_Current) {
            m_Current = m_Last->prev;
            delete [] m_Last->body;
            delete m_Last;
        }
        m_First = m_Last = m_Current = 0;
        m_BlockPos = m_Pos = m_Size = 0;
        return 0;
    }

    while (nof_bytes > 0) {
        TSize n = m_BlockSize - m_Last->free_space;
        if (n <= nof_bytes) {
            // we have to delete the whole block
            delete [] m_Last->body;
            SMemBlock* t = m_Last->prev;
            if ( t ) {
                t->next = 0;
            }
            delete m_Last;
            _ASSERT(m_Last != t);
            m_Last = t;
            nof_bytes -= n;
            m_Size    -= n;
            continue;
        }
        // we have to free some bytes
        m_Last->free_space -= nof_bytes;
        m_Size             -= nof_bytes;
        break;
    }

    if (m_Pos >= m_Size) {
        m_Pos      = m_Size;
        m_Current  = 0;
        m_BlockPos = 0;
    }

    return m_Size;
}


size_t CMemStore::Insert(const void* buff, size_t size)
{
    if (!buff  ||  !size)
        return 0;

    if(size > kMax_BlobSize) size= kMax_BlobSize;

    if ( !m_Current )
        return Append(buff, size);

    char* b         = (char*) buff;
    TSize nof_bytes = (TSize) size;
    TSize n = 0;

    while (nof_bytes > 0) {
        // first empty byte in this block
        TSize f_free = m_BlockSize - m_Current->free_space;
        // number of bytes to move
        TSize k      = f_free - m_BlockPos;

        if (nof_bytes <= m_Current->free_space) {
            // we can add this to existing block
            memmove(&m_Current->body[m_BlockPos + nof_bytes],
                    &m_Current->body[m_BlockPos], k);
            memcpy(&m_Current->body[m_BlockPos], b + n, nof_bytes);
            m_Current->free_space -= nof_bytes;
            n                     += nof_bytes;
            m_BlockPos            += nof_bytes;
            nof_bytes = 0;
            break;
        }

        // there is no enaugh space in existing block -- split it
        SMemBlock* t_block = new SMemBlock;

        t_block->body = new char[m_BlockSize];

        t_block->next = m_Current->next;
        if (t_block->next)
            t_block->next->prev = t_block;
        m_Current->next = t_block;
        t_block->prev = m_Current;

        memcpy(t_block->body, &m_Current->body[m_BlockPos], k);
        t_block->free_space = m_BlockSize - k;
        m_Current->free_space += k;

        k = (nof_bytes <= m_Current->free_space)
            ? nof_bytes : m_Current->free_space;
        memcpy(&m_Current->body[m_BlockPos], b + n, k);
        m_Current->free_space -= k;
        nof_bytes             -= k;
        n                     += k;
        if (m_Last == m_Current)
            m_Last = t_block;
        m_Current  = t_block;
        m_BlockPos = 0;
    }
    m_Pos  += n;
    m_Size += n;

    // try to merge the two last blocks
    SMemBlock* t_block = m_Current->next;
    if ((m_Current->free_space + t_block->free_space) >= m_BlockSize) {
        TSize f_free = m_BlockSize - m_Current->free_space;
        TSize k      = m_BlockSize - t_block->free_space;
        memcpy(&m_Current->body[f_free], t_block->body, k);
        m_Current->free_space -= k;
        _ASSERT(m_Current->next != t_block->next);
        m_Current->next = t_block->next;
        if (m_Current->next) {
            m_Current->next->prev = m_Current;
        } else {
            m_Last = m_Current;
        }
        delete [] t_block->body;
        delete t_block;
    }
    return n;
}


size_t CMemStore::Delete(size_t size)
{
    if (!m_Last  ||  !size == 0)
        return m_Size;

    if(size > kMax_BlobSize) size= kMax_BlobSize;

    if ( !m_Current )
        return Truncate(size);

    TSize nof_bytes = (TSize) size;

    if (m_BlockPos >= nof_bytes) {
        // just one block is affected
        memmove(&m_Current->body[m_BlockPos - nof_bytes],
                &m_Current->body[m_BlockPos],
                m_BlockSize - m_Current->free_space - m_BlockPos);
        m_Current->free_space += nof_bytes;
        m_BlockPos            -= nof_bytes;
        m_Pos                 -= nof_bytes;
        m_Size                -= nof_bytes;
        return m_Size;
    }

    // we can affect several blocks...
    if (m_BlockPos > 0) {
        memmove(m_Current->body, &m_Current->body[m_BlockPos],
                m_BlockSize - m_Current->free_space - m_BlockPos);
        m_Current->free_space += m_BlockPos;
        nof_bytes             -= m_BlockPos;
        m_Pos                 -= m_BlockPos;
        m_Size                -= m_BlockPos;
        m_BlockPos = 0;
    }

    while (nof_bytes > 0) {
        SMemBlock* t_block = m_Current->prev;
        if ( !t_block ) {
            m_First = m_Current;
            break;
        }

        TSize n = m_BlockSize - t_block->free_space; // # of bytes in this block
        if (nof_bytes < n) {
            // all we have to delete is inside the block
            t_block->free_space += nof_bytes;
            m_Pos               -= nof_bytes;
            m_Size              -= nof_bytes;
            break;
        }
        // delete the whole block
        if (t_block->prev)
            t_block->prev->next = m_Current;
        else
            m_First = m_Current;

        _ASSERT(m_Current->prev != t_block->prev);
        m_Current->prev = t_block->prev;
        delete [] t_block->body;
        delete t_block;
        m_Pos     -= n;
        m_Size    -= n;
        nof_bytes -= n;
    }

    return m_Size;
}


CMemStore::CMemStore(C_SA_Storage& storage, size_t block_size)
{
    if(block_size > kMax_BlobSize) block_size= kMax_BlobSize;
    x_Init((TSize) block_size);

    char* buff = new char[m_BlockSize];
    TSize n;

    while ((n = static_cast<TSize>(storage.Read(buff,
                                   (size_t) m_BlockSize))) > 0) {
        Append(buff, n);
        if (n < m_BlockSize)
            break;
    }
}


CMemStore::~CMemStore()
{
    try {
        while ( m_Last ) {
            m_Current = m_Last->prev;
            delete [] m_Last->body;
            delete m_Last;
            _ASSERT(m_Last != m_Current);
            m_Last = m_Current;
        }
    }
    NCBI_CATCH_ALL_X( 1, NCBI_CURRENT_FUNCTION )
}


END_NCBI_SCOPE


