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
 * Author: 
 *   Eugene Vasilchenko
 *
 * File Description:
 *   Memory pool for fast allocation of memory for localized set of CObjects,
 *   e.g. when deserializing big object tree.
 *   Standard CObject and CRef classes for reference counter based GC
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbimempool.hpp>

BEGIN_NCBI_SCOPE


static const size_t kDefaultChunkSize = 8192;
static const size_t kMinChunkSize = 128;

static const size_t kDefaultThresholdRatio = 16;
static const size_t kMinThresholdRatio = 2;
static const size_t kMinThreshold = 4;

#if defined(_DEBUG) && defined (DEBUG_MEMORY_POOL)
static CAtomicCounter sx_chunks_counter;
static CAtomicCounter::TValue sx_max_counter;
static struct Printer
{
    ~Printer()
        {
            if ( sx_max_counter ) {
                ERR_POST("Max memory chunks: " << sx_max_counter);
                ERR_POST("Final memory chunks: " << sx_chunks_counter.Get());
            }
        }
} sx_printer;

inline
void RegisterMemoryChunk(size_t /*size*/)
{
    CAtomicCounter::TValue value = sx_chunks_counter.Add(1);
    if ( value > sx_max_counter ) {
        sx_max_counter = value;
    }
}
inline
void DeregisterMemoryChunk(size_t /*size*/)
{
    sx_chunks_counter.Add(-1);
}
#else
# define RegisterMemoryChunk(size)
# define DeregisterMemoryChunk(size)
#endif

#ifdef _DEBUG
# define ObjFatal Fatal
#else
# define ObjFatal Critical
#endif

class CObjectMemoryPoolChunk : public CObject
{
private:
    CObjectMemoryPoolChunk(size_t size)
        : m_CurPtr(m_Memory), m_EndPtr(m_Memory+size)
        {
            RegisterMemoryChunk(size);
        }
public:
    static CObjectMemoryPoolChunk* CreateChunk(size_t size);

    ~CObjectMemoryPoolChunk(void)
        {
            DeregisterMemoryChunk(m_EndPtr-m_Memory);
        }
    

    struct SHeader {
        enum {
            eMagicAllocated   = 0x3f6345ad,
            eMagicDeallocated = 0x63d83644
        };
        CObjectMemoryPoolChunk* m_ChunkPtr;
        int m_Magic;
    };


    void IncrementObjectCount(void)
        {
            AddReference();
        }

    void DecrementObjectCount(void)
        {
            RemoveReference();
        }

    void* Allocate(size_t size);


    static CObjectMemoryPoolChunk* GetChunk(const void* ptr)
        {
            const SHeader* header = reinterpret_cast<const SHeader*>(ptr)-1;
            CObjectMemoryPoolChunk* chunk = header->m_ChunkPtr;
            if ( header->m_Magic != SHeader::eMagicAllocated ) {
                if ( header->m_Magic != SHeader::eMagicDeallocated ) {
                    ERR_POST(ObjFatal << "CObjectMemoryPoolChunk::GetChunk: "
                             "Bad chunk header magic: already freed");
                }
                else {
                    ERR_POST(ObjFatal << "CObjectMemoryPoolChunk::GetChunk: "
                             "Bad chunk header magic");
                }
                return 0;
            }
            if ( ptr <= chunk->m_Memory || ptr >= chunk->m_CurPtr ) {
                ERR_POST(ObjFatal << "CObjectMemoryPoolChunk::GetChunk: "
                         "Object is beyond chunk memory");
            }
            // now we mark header so it will not be deleted twice
            const_cast<SHeader*>(header)->m_Magic = SHeader::eMagicDeallocated;
            return chunk;
        }

private:
    void* m_CurPtr;
    void* m_EndPtr;
    char m_Memory[1];

private:
    CObjectMemoryPoolChunk(const CObjectMemoryPoolChunk&);
    void operator=(const CObjectMemoryPoolChunk&);
};


CObjectMemoryPoolChunk* CObjectMemoryPoolChunk::CreateChunk(size_t size)
{
    void* ptr = CObject::operator new(sizeof(CObjectMemoryPoolChunk)+size);
    CObjectMemoryPoolChunk* chunk = ::new(ptr) CObjectMemoryPoolChunk(size);
    chunk->DoDeleteThisObject();
    return chunk;
}


void* CObjectMemoryPoolChunk::Allocate(size_t size)
{
    _ASSERT(size > 0);
    // align the size up to size header (usually 8 bytes)
    size += sizeof(SHeader)-1;
    if ( sizeof(SHeader) & (sizeof(SHeader)-1) ) {
        // size of header is not power of 2
        size -= size % sizeof(SHeader);
    }
    else {
        // size of header is power of 2 -> we can use bit operation
        size &= ~(sizeof(SHeader)-1);
    }
    // calculate new pointers
    SHeader* header = reinterpret_cast<SHeader*>(m_CurPtr);
    char* ptr = reinterpret_cast<char*>(header + 1);
    char* end = ptr + size;
    // check if space is enough
    if ( end > m_EndPtr ) {
        return 0;
    }
    // initialize the header
    header->m_ChunkPtr = this;
    header->m_Magic = SHeader::eMagicAllocated;
    // all checks are done, now we update chunk
    _ASSERT(m_CurPtr == header);
    m_CurPtr = end;
    // increment object counter in this chunk
    IncrementObjectCount();
    return ptr;
}


CObjectMemoryPool::CObjectMemoryPool(size_t chunk_size)
{
    SetChunkSize(chunk_size);
}


CObjectMemoryPool::~CObjectMemoryPool(void)
{
}


void CObjectMemoryPool::SetChunkSize(size_t chunk_size)
{
    if ( chunk_size == 0 ) {
        chunk_size = kDefaultChunkSize;
    }
    if ( chunk_size < kMinChunkSize ) {
        chunk_size = kMinChunkSize;
    }
    m_ChunkSize = chunk_size;
    SetMallocThreshold(0);
}


void CObjectMemoryPool::SetMallocThreshold(size_t malloc_threshold)
{
    if ( malloc_threshold == 0 ) {
        malloc_threshold = m_ChunkSize / kDefaultThresholdRatio;
    }
    size_t min_threshold = kMinThreshold;
    if ( malloc_threshold < min_threshold ) {
        malloc_threshold = min_threshold;
    }
    size_t max_threshold = m_ChunkSize / kMinThresholdRatio;
    if ( malloc_threshold > max_threshold ) {
        malloc_threshold = max_threshold;
    }
    m_MallocThreshold = malloc_threshold;
}


void* CObjectMemoryPool::Allocate(size_t size)
{
    if ( size > m_MallocThreshold ) {
        return 0;
    }
    for ( int i = 0; i < 2; ++i ) {
        if ( !m_CurrentChunk ) {
            m_CurrentChunk = CObjectMemoryPoolChunk::CreateChunk(m_ChunkSize);
        }
        void* ptr = m_CurrentChunk->Allocate(size);
        if ( ptr ) {
            return ptr;
        }
        m_CurrentChunk.Reset();
    }
    ERR_POST_ONCE("CObjectMemoryPool::Allocate("<<size<<"): "
                  "double fault in chunk allocator");
    return 0;
}


void CObjectMemoryPool::Deallocate(void* ptr)
{
    CObjectMemoryPoolChunk* chunk = CObjectMemoryPoolChunk::GetChunk(ptr);
    if ( chunk ) {
        chunk->DecrementObjectCount();
    }
}


void CObjectMemoryPool::Delete(const CObject* object)
{
    CObjectMemoryPoolChunk* chunk = CObjectMemoryPoolChunk::GetChunk(object);
    if ( chunk ) {
        const_cast<CObject*>(object)->~CObject();
        chunk->DecrementObjectCount();
    }
    else {
        ERR_POST(Critical << "CObjectMemoryPool::Delete(): "
                 "cannot determine the chunk, memory will not be released");
        const_cast<CObject*>(object)->~CObject();
    }
}


END_NCBI_SCOPE

/*
 * ===========================================================================
 * $Log$
 * Revision 1.1  2005/04/26 14:07:22  vasilche
 * Implemented class CObjectMemoryPool.
 *
 *
 */
