#ifndef CORELIB___NCBIMEMPOOL__HPP
#define CORELIB___NCBIMEMPOOL__HPP

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
 * Author:  Eugene Vasilchenko
 *
 *
 */

/// @file ncbimempool.hpp
/// Memory pool for fast allocation of memory for localized set of CObjects,
/// e.g. when deserializing big object tree.


#include <corelib/ncbistd.hpp>
#include <corelib/ncbiobj.hpp>


/** @addtogroup Object
 *
 * @{
 */



BEGIN_NCBI_SCOPE

class CObjectMemoryPoolChunk;

class NCBI_XNCBI_EXPORT CObjectMemoryPool : public CObject
{
public:
    /// constructor
    /// @param chunk_size
    ///   Size of chunks to allocate from system heap.
    ///   If it's zero, use some default size.
    CObjectMemoryPool(size_t chunk_size = 0);
    /// destructor
    ~CObjectMemoryPool(void);

    /// configurable parameters

    /// Get chunks' size.
    size_t GetChunkSize(void) const;

    /// Change chunks' size.
    /// @param chunk_size
    ///   Size of chunks to allocate from system heap.
    ///   If it's zero, use some default size.
    void SetChunkSize(size_t chunk_size);

    /// Get threshold for direct allocation from system heap.
    size_t GetMallocThreshold(void) const;

    /// Change threshold for direct allocation from system heap.
    /// @param malloc_threshold
    ///   Objects with size greater than this value will be allocated
    ///   directly from system heap.
    ///   If it's zero, use some default threshold.
    void SetMallocThreshold(size_t malloc_threshold);

    /// Allocate memory block.
    void* Allocate(size_t size);

    /// Deallocate memory block.
    /// Deallocated momory is not reused, but block counter is decremented,
    /// and if it goes to zero, full memory chunk is freed.
    void Deallocate(void* ptr);

    /// Check if object is allocated from some memory pool,
    /// and delete it correspondingly.
    static void Delete(const CObject* object);

private:
    size_t m_ChunkSize;
    size_t m_MallocThreshold;
    CRef<CObjectMemoryPoolChunk> m_CurrentChunk;

private:
    // prevent copying
    CObjectMemoryPool(const CObjectMemoryPool&);
    void operator=(const CObjectMemoryPool&);
};


////////////////////////////////////////////////////////////////////////////
// inline functions

inline
size_t CObjectMemoryPool::GetChunkSize(void) const
{
    return m_ChunkSize;
}


inline
size_t CObjectMemoryPool::GetMallocThreshold(void) const
{
    return m_MallocThreshold;
}


END_NCBI_SCOPE

/* @} */

/*
 * ===========================================================================
 * $Log$
 * Revision 1.1  2005/04/26 14:07:22  vasilche
 * Implemented class CObjectMemoryPool.
 *
 *
 */
#endif /* CORELIB___NCBIMEMPOOL__HPP */
