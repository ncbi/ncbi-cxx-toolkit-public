#ifndef DBAPI_DRIVER___MEMORY_STORE__HPP
#define DBAPI_DRIVER___MEMORY_STORE__HPP

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
 * File Name:  memory_store.cpp
 *
 * Author:  Vladimir Soussov
 *
 * File Description:  RAM storage
 *
 */


#include <corelib/ncbistd.hpp>
#include <corelib/ncbi_limits.h>


BEGIN_NCBI_SCOPE

static size_t kMax_BlobSize= (size_t) kMax_Int;

// The storage with the sequential access
class C_SA_Storage
{
public:
    virtual size_t  Read  (void*       buff, size_t nof_bytes) = 0;
    virtual size_t  Append(const void* buff, size_t nof_bytes) = 0;
    virtual void    Flush (void) = 0;

    virtual ~C_SA_Storage() {}
};


// Random access storage
class C_RA_Storage : public C_SA_Storage
{
public:
    enum EWhence {
        eCurr,
        eHead,
        eTail
    };

    virtual long   Seek (long offset, EWhence whence) = 0;
    virtual size_t Tell (void) const = 0;
    virtual size_t Write(const void* buff, size_t nof_bytes) = 0;

    virtual ~C_RA_Storage() {}
};


// Full access storage (allows insert and delete)
class C_FA_Storage : public C_RA_Storage
{
public:
    virtual size_t Insert  (const void* buff, size_t nof_bytes) = 0;
    virtual size_t Delete  (size_t nof_bytes) = 0;
    virtual size_t Truncate(size_t nof_bytes) = 0;

    virtual ~C_FA_Storage() {}
};




class CMemStore : public C_FA_Storage
{
public:
    CMemStore() { x_Init(); }
    CMemStore(size_t block_size) { x_Init((TSize) block_size); }
    CMemStore(C_SA_Storage& storage, size_t block_size = 2048);

    ~CMemStore();

    size_t Read        (void*       buff, size_t nof_bytes);
    size_t Peek        (void*       buff, size_t nof_bytes) const;
    size_t Append      (const void* buff, size_t nof_bytes);
    size_t Write       (const void* buff, size_t nof_bytes);
    size_t Insert      (const void* buff, size_t nof_bytes);

    size_t Delete      (size_t nof_bytes = kMax_BlobSize);
    size_t Truncate    (size_t nof_bytes = kMax_BlobSize);

    void   Flush       (void)  { return; };
    long   Seek        (long offset, EWhence whence);
    size_t Tell        () const  { return (size_t) m_Pos; }
    size_t GetDataSize () const  { return (size_t) m_Size; }

    typedef long TSize;

private:
    struct SMemBlock
    {
        SMemBlock* next;
        SMemBlock* prev;
        TSize      free_space;
        char*      body;
    };

    TSize      m_BlockSize;
    SMemBlock* m_First;
    SMemBlock* m_Last;
    SMemBlock* m_Current;
    TSize      m_Pos;
    TSize      m_BlockPos;
    TSize      m_Size;

    SMemBlock* x_InsertBlock(void);
    SMemBlock* x_AddBlock(void);
    TSize      x_SeekHEAD(TSize offset);
    TSize      x_SeekCURR(TSize offset);
    TSize      x_SeekTAIL(TSize offset);

    void x_Init(TSize block_size = 2048) {
        m_BlockSize = (block_size > 16) ? block_size : 2048;
        m_First = m_Last = m_Current = 0;
        m_Pos = m_BlockPos = m_Size = 0;
    };
};


END_NCBI_SCOPE


#endif  /* DBAPI_DRIVER___MEMORY_STORE__HPP */
