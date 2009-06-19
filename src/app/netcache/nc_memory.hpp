#ifndef NETCACHE_NC_MEMORY__HPP
#define NETCACHE_NC_MEMORY__HPP
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
 * Authors:  Pavel Ivanov
 */


BEGIN_NCBI_SCOPE


/// Utility class for tuning the database cache in NetCache
class CNCDBCacheManager {
public:
    /// Initialize database cache and attach it to SQLite.
    /// Method must be called before CSQLITE_Global::Initialize()
    static void Initialize(void);
    /// Set maximum memory size that database cache can consume.
    /// Limit is not hard, i.e. if SQLite will not be able to work inside this
    /// limit then additional memory will be automatically consumed.
    /// NB: Database cache memory is never returned to the system now.
    static void SetMaxSize(size_t mem_size);

private:
    CNCDBCacheManager(void);
};


/*
///
class CNCMemoryChunk
{
public:
    static void* AllocateGlobal(size_t size);
    static void Deallocate(void* ptr);

    CNCMemoryChunk(size_t size);
    void* Allocate(size_t size);

private:
    CNCMemoryChunk(const CNCMemoryChunk&);
    CNCMemoryChunk& operator= (const CNCMemoryChunk&);

    void x_ResetDataStartPtr(void);

    int   m_AllocsCount;
    char* m_CurPtr;
    char* m_EndPtr;
};


///
class CNCMemChunkFactory : public CObjFactory_New<CNCMemoryChunk>
{
public:
    CNCMemoryChunk* CreateObject(void);
    void DeleteObject(CNCMemoryChunk* chunk);
};


enum {
    /// Size of all memory chunks allocated by CNCMemoryManager.
    /// This is size of memory allocated for each page in SQLite used by
    /// NetCache.
    kNCMemManagerChunkSize = 32768 + 256,
    kNCMemManagerPoolSize  = 200
};


///
class CNCMemoryManager
{
public:
    static void* Allocate(size_t size);
    static void Deallocate(void* ptr);
    static void ReturnFreeChunk(CNCMemoryChunk* chunk);

public:
    CNCMemoryManager(void);
    ~CNCMemoryManager(void);

private:
    CNCMemoryManager(const CNCMemoryManager&);
    CNCMemoryManager& operator= (const CNCMemoryManager&);

    static CNCMemoryManager& x_Instance(void);
    void* x_AllocImpl(size_t size);
    void x_FreeChunkImpl(CNCMemoryChunk* chunk);


    typedef CObjPool<CNCMemoryChunk, CNCMemChunkFactory>  TChunksPool;

    CNCMemoryChunk* m_CurChunk;
    TChunksPool     m_Pool;
};


template <class T>
class CNCSmallDataAllocator : public allocator<T>
{
public:
    typedef allocator<T>                   TBaseType;
    typedef typename TBaseType::pointer    pointer;
    typedef typename TBaseType::size_type  size_type;

    template<class Other>
    struct rebind
    {
        typedef CNCSmallDataAllocator<Other> other;
    };


    CNCSmallDataAllocator(void);
    template <class Other>
    CNCSmallDataAllocator(const allocator<Other>&);

    pointer allocate(size_type n,
                     allocator<void>::const_pointer hint = 0);
    void deallocate(pointer ptr, size_type n);
};



inline
CNCMemoryManager::CNCMemoryManager(void)
    : m_CurChunk(NULL),
      m_Pool(kNCMemManagerPoolSize)
{}

inline
CNCMemoryManager::~CNCMemoryManager(void)
{
    if (m_CurChunk) {
        m_Pool.Return(m_CurChunk);
        m_CurChunk = NULL;
    }
}

inline void*
CNCMemoryManager::Allocate(size_t size)
{
    return x_Instance().x_AllocImpl(size);
}

inline void
CNCMemoryManager::Deallocate(void* ptr)
{
    CNCMemoryChunk::Deallocate(ptr);
}

inline void
CNCMemoryManager::x_FreeChunkImpl(CNCMemoryChunk* chunk)
{
    if (m_CurChunk != chunk)
        m_Pool.Return(chunk);
}

inline void
CNCMemoryManager::ReturnFreeChunk(CNCMemoryChunk* chunk)
{
    x_Instance().x_FreeChunkImpl(chunk);
}


inline void
CNCMemoryChunk::x_ResetDataStartPtr(void)
{
    m_CurPtr = (char*)(this + 1);
}

inline
CNCMemoryChunk::CNCMemoryChunk(size_t size)
    : m_AllocsCount(0)
{
    x_ResetDataStartPtr();
    m_EndPtr = m_CurPtr + size;
}


extern size_t s_MemUsed;

inline CNCMemoryChunk*
CNCMemChunkFactory::CreateObject(void)
{
    void* chunk = malloc(sizeof(CNCMemoryChunk) + kNCMemManagerChunkSize);
    s_MemUsed += sizeof(CNCMemoryChunk) + kNCMemManagerChunkSize;
    LOG_POST("MemManager: alloc chunk, used " << s_MemUsed);
    return new(chunk) CNCMemoryChunk(kNCMemManagerChunkSize);
}

inline void
CNCMemChunkFactory::DeleteObject(CNCMemoryChunk* chunk)
{
    s_MemUsed -= sizeof(CNCMemoryChunk) + kNCMemManagerChunkSize;
    LOG_POST("MemManager: dealloc chunk, used " << s_MemUsed);
    free(chunk);
}


template <class T>
inline
CNCSmallDataAllocator<T>::CNCSmallDataAllocator(void)
{}

template <class T>
template <class Other>
inline
CNCSmallDataAllocator<T>::CNCSmallDataAllocator(const allocator<Other>&)
{}

template <class T>
inline typename CNCSmallDataAllocator<T>::pointer
CNCSmallDataAllocator<T>::allocate(size_type n,
                                   allocator<void>::const_pointer hint/ *= 0* /)
{
    return (pointer)CNCMemoryManager::Allocate(n * sizeof(T));
}

template <class T>
inline void
CNCSmallDataAllocator<T>::deallocate(pointer ptr, size_type n)
{
    CNCMemoryManager::Deallocate(ptr);
}
*/

END_NCBI_SCOPE

#endif /* NETCACHE_NC_MEMORY__HPP */
