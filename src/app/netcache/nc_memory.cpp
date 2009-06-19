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

#include <ncbi_pch.hpp>

#include <db/sqlite/sqlitewrapp.hpp>

#include "nc_memory.hpp"


BEGIN_NCBI_SCOPE


/// Default limit of memory size consumed by database cache
static const size_t kNC_DefDBCacheSize     = 1024 * 1024 * 1024;
/// Default size of cache instance's hash-table for key->page bindings
static const int    kNC_DefDBCacheHashSize = 1024;


class CNCDBCache;

/// Meta-information about each page in database cache.
/// Actual page data will go right after this structure in memory.
struct SNCDBCachePage
{
    /// Instance of database cache this page belongs to at the moment.
    /// If this page doesn't belong to any cache at the moment (i.e. it's
    /// located just in pool ready for next use) then this will be NULL.
    CNCDBCache*      cache;
    /// Key of the page assigned by SQLite (set if it belongs to some cache).
    unsigned int     key;
    /// Size of the page (without this meta-information)
    size_t           size;
    /// Previous page in LRU-list of (semi-)free pages
    SNCDBCachePage* prev_lru;
    /// Next page in LRU-list of (semi-)free pages
    SNCDBCachePage* next_lru;
    /// Next page in the hash-table inside cache instance (not NULL only if
    /// page belongs to some cache instance and there's other page with the
    /// same hash-code in this cache instance)
    SNCDBCachePage* next_hash;
};

/// Conversion from SNCDBCachePage* to void* pointing to the actual page data
#define PCACHE_PAGEDATA(p)  (p + 1)
/// Conversion from void* pointing to the actual page data to SNCDBCachePage*
/// pointing to meta-information about this page.
#define PCACHE_PAGECLASS(p) ((SNCDBCachePage*)p - 1)


/// Instance of database cache which will cache pages from one database file.
/// According to SQLite logic with shared cache turned on only one instance
/// of this class will be created for each database file.
class CNCDBCache
{
public:
    /// Create new cache instance
    ///
    /// @param page_size
    ///   Size of pages that will be requested from this cache instance
    /// @param purgeable
    ///   TRUE if pages in this cache instance can be deleted at any time
    ///   after they're unpinned by SQLite. FALSE if pages must never be
    ///   deleted unless SQLite explicitly said so.
    CNCDBCache(size_t page_size, bool purgeable);
    ~CNCDBCache(void);

    /// Set soft limit on maximum number of pages kept in memory by this
    /// cache instance.
    void SuggestSize(int num_pages);
    /// Get number of pages in the cache instance
    int GetSize(void);
    /// Get size of pages this cache instance should produce
    size_t GetPageSize(void);

    /// Flag for page creation requirement (values are the same as
    /// SQLite gives).
    enum EPageCreate {
        eDoNotCreate      = 0,  ///< Do not create page if it's not in cache
        eCreateIfPossible = 1,  ///< Try to create page but can fail if soft
                                ///< limit was reached.
        eMustCreate       = 2   ///< Page must be created under any
                                ///< circumstances
    };
    /// Get page from cache
    ///
    /// @param key
    ///   Key of the page to get
    /// @param create_flag
    ///   Flag for page creation requirement
    /// @return
    ///   Pointer to actual page data (without internal information)
    void* GetPage(unsigned int key, EPageCreate create_flag);
    /// Release the page
    ///
    /// @param data
    ///   Pointer to page data
    /// @param must_delete
    ///   TRUE if page must not exist anymore, FALSE if page should be kept
    ///   until cache decides that it should be reused for other purposes.
    void ReleasePage(void* data, bool must_delete);
    /// Change key of the page
    ///
    /// @param data
    ///   Pointer to page data
    /// @param old_key
    ///   Old key of the page
    /// @param new_key
    ///   New key of the page
    void ChangePageKey(void* data, unsigned int old_key, unsigned int new_key);
    /// Delete from cache all pages with keys greater or equal to min_key
    void DeleteAllPages(unsigned int min_key);

private:
    CNCDBCache(const CNCDBCache&);
    CNCDBCache& operator=(const CNCDBCache&);

    /// Add page to hash-table of key->page bindings.
    void x_AddToHash(SNCDBCachePage* page);
    /// Remove page from hash-table (effectively deleting from cache).
    void x_RemoveFromHash(SNCDBCachePage* page);
    /// Get page from hash-table by its key.
    SNCDBCachePage* x_GetFromHash(unsigned int key);


    typedef vector<SNCDBCachePage*>  TPagesHash;

    /// Size of pages created by this cache instance
    size_t       m_PageSize;
    /// TRUE if pages in this cache instance can be deleted at any time
    /// after they're unpinned by SQLite. FALSE if pages must never be
    /// deleted unless SQLite explicitly said so.
    bool         m_Purgable;
    /// Number of pages stored in this cache instance
    int          m_CacheSize;
    /// Maximum key value among pages stored in the cache instance
    unsigned int m_MaxKey;
    /// Hash-table of key->page bindings
    TPagesHash   m_PagesHash;
};



/// Head of LRU-list of (semi-)free pages, i.e. the least recently used page
/// and the first candidate for reuse. Semi- means page can be not free and
/// can belong to some cache instance but it still is allowed to detach from
/// cache instance and reused for other purposes.
static SNCDBCachePage* s_LRUHead = NULL;
/// Tail of LRU-list of (semi-)free pages, i.e. the most recently used page
/// and the last candidate for reuse. Most probably this will never be free
/// page not belonging to any cache because no matter how recently page was
/// used when it's detached from cache instance it's put to the head of
/// LRU-list.
static SNCDBCachePage* s_LRUTail = NULL;
/// Total amount of memory used by cache pages
static size_t s_MemUsed  = 0;
/// Total amount of memory used by cache pages along with pages'
/// meta-information.
static size_t s_RealMemUsed  = 0;
/// Soft limit on size of memory consumed by database cache
static size_t s_MemLimit = kNC_DefDBCacheSize;
/// Global mutex for database cache operation
static CFastMutex s_CacheMutex;



inline size_t
CNCDBCache::GetPageSize(void)
{
    return m_PageSize;
}


/// Remove page from LRU-list meaning it becomes locked and will not be reused
/// by some other cache instance.
/// Function should be called under global mutex.
static inline void
s_RemoveFromLRU(SNCDBCachePage* page)
{
    if (page->prev_lru) {
        page->prev_lru->next_lru = page->next_lru;
    }
    else if (page == s_LRUHead) {
        s_LRUHead = page->next_lru;
    }
    if (page->next_lru) {
        page->next_lru->prev_lru = page->prev_lru;
    }
    else if (page == s_LRUTail) {
        s_LRUTail = page->prev_lru;
    }
    page->prev_lru = page->next_lru = NULL;
}

/// Put page to LRU-list's tail meaning it was just used and released and if
/// it will not be used anymore it will eventually be reused by some cache
/// instance.
static inline void
s_AddToLRU(SNCDBCachePage* page)
{
    page->prev_lru = s_LRUTail;
    if (s_LRUTail) {
        s_LRUTail->next_lru = page;
    }
    else {
        _ASSERT(!s_LRUHead);
        s_LRUHead = page;
    }
    s_LRUTail = page;
}

/// Unbind page from cache instance and put it into head of LRU-list so that
/// it will be quickly reused by some other cache instance.
static inline void
s_UnbindFromCache(SNCDBCachePage* page)
{
    page->cache = NULL;
    s_RemoveFromLRU(page);
    page->next_lru = s_LRUHead;
    if (s_LRUHead) {
        s_LRUHead->prev_lru = page;
    }
    else {
        _ASSERT(!s_LRUTail);
        s_LRUTail = page;
    }
    s_LRUHead = page;
}

/// Create new page for a given cache instance
static inline SNCDBCachePage*
s_CreatePage(CNCDBCache* pcache)
{
    size_t size = pcache->GetPageSize();
    void* ptr = ::operator new(size + sizeof(SNCDBCachePage));
    SNCDBCachePage* page = new(ptr) SNCDBCachePage;

    page->next_hash = page->prev_lru = page->next_lru = NULL;
    page->size = size;
    s_MemUsed += size;
    s_RealMemUsed += size + sizeof(SNCDBCachePage);

    return page;
}

/// Delete page (deallocate its memory)
static inline void
s_DeletePage(SNCDBCachePage* page)
{
    s_MemUsed -= page->size;
    s_RealMemUsed -= page->size + sizeof(SNCDBCachePage);
    delete page;
}



inline
CNCDBCache::CNCDBCache(size_t page_size, bool purgeable)
    : m_PageSize(page_size),
      m_Purgable(purgeable),
      m_CacheSize(0),
      m_MaxKey(0),
      m_PagesHash(kNC_DefDBCacheHashSize, static_cast<SNCDBCachePage*>(NULL))
{}

inline void
CNCDBCache::SuggestSize(int num_pages)
{
    for (;;) {
        int next_num = num_pages & (num_pages - 1);
        if (next_num == 0)
            break;
        num_pages = next_num;
    }
    if (num_pages == int(m_PagesHash.size()))
        return;

    CFastMutexGuard guard(s_CacheMutex);

    TPagesHash new_hash(num_pages, static_cast<SNCDBCachePage*>(NULL));
    ITERATE(TPagesHash, it, m_PagesHash) {
        SNCDBCachePage* page = *it;
        while (page) {
            SNCDBCachePage* next = page->next_hash;
            page->next_hash = NULL;
            int hash_num = page->key & (num_pages - 1);
            SNCDBCachePage* hash_val = new_hash[hash_num];
            if (hash_val) {
                while (hash_val->next_hash)
                    hash_val = hash_val->next_hash;
                hash_val->next_hash = page;
            }
            else {
                new_hash[hash_num] = page;
            }
            page = next;
        }
    }

    new_hash.swap(m_PagesHash);
}

inline int
CNCDBCache::GetSize(void)
{
    CFastMutexGuard guard(s_CacheMutex);
    return m_CacheSize;
}

inline void
CNCDBCache::x_RemoveFromHash(SNCDBCachePage* page)
{
    _ASSERT(page->cache == this);

    unsigned int hash_num = page->key & (m_PagesHash.size() - 1);
    SNCDBCachePage* hash_val = m_PagesHash[hash_num];
    if (hash_val == page) {
        m_PagesHash[hash_num] = page->next_hash;
    }
    else {
        SNCDBCachePage* prev = NULL;
        while (hash_val  &&  hash_val != page) {
            prev = hash_val;
            hash_val = hash_val->next_hash;
        }
        if (hash_val) {
            _ASSERT(prev);
            prev->next_hash = hash_val->next_hash;
        }
    }
    if (hash_val) {
        hash_val->next_hash = NULL;
        if (--m_CacheSize == 0) {
            m_MaxKey = 0;
        }
    }
}

inline void
CNCDBCache::x_AddToHash(SNCDBCachePage* page)
{
    unsigned int hash_num = page->key & (m_PagesHash.size() - 1);
#ifdef _DEBUG
    // According to SQLite's implementation of page cache this should not ever
    // happen. But to be on a safe side we better check it in Debug build.
    SNCDBCachePage* hash_val = m_PagesHash[hash_num];
    while (hash_val  &&  hash_val->key != page->key)
        hash_val = hash_val->next_hash;
    if (hash_val) {
        ERR_POST("Impossible happened: re-hash over existing page");
        x_RemoveFromHash(hash_val);
        s_UnbindFromCache(hash_val);
    }
#endif
    page->next_hash = m_PagesHash[hash_num];
    m_PagesHash[hash_num] = page;
    ++m_CacheSize;
    m_MaxKey = max(m_MaxKey, page->key);
}

inline SNCDBCachePage*
CNCDBCache::x_GetFromHash(unsigned int key)
{
    unsigned int hash_num = key & (m_PagesHash.size() - 1);
    SNCDBCachePage* page = m_PagesHash[hash_num];
    while (page  &&  page->key != key)
        page = page->next_hash;
    return page;
}

inline void
CNCDBCache::DeleteAllPages(unsigned int min_key)
{
    CFastMutexGuard guard(s_CacheMutex);
    if (m_MaxKey < min_key  ||  m_CacheSize == 0)
        return;

    for (size_t i = 0; i < m_PagesHash.size(); ++i) {
        SNCDBCachePage** page_ptr = &m_PagesHash[i];
        for (SNCDBCachePage* page = *page_ptr; page; page = *page_ptr) {
            if (page->key >= min_key) {
                --m_CacheSize;
                *page_ptr = page->next_hash;
                page->next_hash = NULL;
                s_UnbindFromCache(page);
            }
            else {
                page_ptr = &page->next_hash;
            }
        }
    }

    m_MaxKey = (min_key == 0 || m_CacheSize == 0? 0: min_key - 1);
}

inline void*
CNCDBCache::GetPage(unsigned int key, EPageCreate create_flag)
{
    CFastMutexGuard guard(s_CacheMutex);

    SNCDBCachePage* page = x_GetFromHash(key);

    if (!page) {
        if (create_flag == eDoNotCreate) {
            return NULL;
        }

        while (s_LRUHead  &&  s_LRUHead->size < m_PageSize) {
            SNCDBCachePage* head_pg = s_LRUHead;
            if (head_pg->cache) {
                head_pg->cache->x_RemoveFromHash(head_pg);
            }
            s_RemoveFromLRU(head_pg);
            s_DeletePage(head_pg);
        }

        if (s_LRUHead  &&  s_LRUHead->cache == NULL) {
            page = s_LRUHead;
        }
        else if (s_LRUHead
                 &&  (size_t(m_CacheSize) >= m_PagesHash.size()
                      ||  s_MemUsed >= s_MemLimit))
        {
            page = s_LRUHead;
            page->cache->x_RemoveFromHash(page);
        }
        else if (create_flag == eCreateIfPossible
                 &&  s_MemUsed >= s_MemLimit)
        {
            return NULL;
        }
        else {
            page = s_CreatePage(this);
        }

        page->cache = this;
        page->key = key;
        x_AddToHash(page);
        // Required by SQLite if page is new for the cache instance
        *(void**)PCACHE_PAGEDATA(page) = NULL;
    }

    s_RemoveFromLRU(page);
    return PCACHE_PAGEDATA(page);
}

inline void
CNCDBCache::ReleasePage(void* data, bool must_delete)
{
    CFastMutexGuard guard(s_CacheMutex);

    SNCDBCachePage* page = PCACHE_PAGECLASS(data);
    if (must_delete) {
        x_RemoveFromHash(page);
        s_UnbindFromCache(page);
    }
    else if (s_MemUsed - page->size > s_MemLimit) {
        x_RemoveFromHash(page);
        s_DeletePage(page);
    }
    else if (m_Purgable) {
        s_AddToLRU(page);
    }
}

inline void
CNCDBCache::ChangePageKey(void*        data,
                          unsigned int old_key,
                          unsigned int new_key)
{
    CFastMutexGuard guard(s_CacheMutex);

    SNCDBCachePage* page = PCACHE_PAGECLASS(data);
    _ASSERT(page->key == old_key);
    x_RemoveFromHash(page);
    page->key = new_key;
    x_AddToHash(page);
}

inline
CNCDBCache::~CNCDBCache(void)
{
    DeleteAllPages(0);
}


// Functions for exposing database cache to SQLite

/// Initialize database cache
static int
s_SQLITE_PCache_Init(void*)
{
    return SQLITE_OK;
}

/// Deinitialize database cache
static void
s_SQLITE_PCache_Shutdown(void*)
{}

/// Create new cache instance
static sqlite3_pcache *
s_SQLITE_PCache_Create(int szPage, int bPurgeable)
{
    return (sqlite3_pcache*) new CNCDBCache(size_t(szPage),
                                                   bPurgeable != 0);
}

/// Set size of database cache (number of pages)
static void
s_SQLITE_PCache_SetSize(sqlite3_pcache* pcache, int nCachesize)
{
    ((CNCDBCache*)pcache)->SuggestSize(nCachesize);
}

/// Get number of pages stored in cache
static int
s_SQLITE_PCache_GetSize(sqlite3_pcache* pcache)
{
    return ((CNCDBCache*)pcache)->GetSize();
}

/// Get page from cache
static void*
s_SQLITE_PCache_GetPage(sqlite3_pcache* pcache,
                        unsigned int    key,
                        int             createFlag)
{
    return ((CNCDBCache*)pcache)
                   ->GetPage(key, CNCDBCache::EPageCreate(createFlag));
}

/// Release page (make it reusable by others)
static void
s_SQLITE_PCache_UnpinPage(sqlite3_pcache* pcache, void* page, int discard)
{
    ((CNCDBCache*)pcache)->ReleasePage(page, discard != 0);
}

/// Change key of the page in cache
static void
s_SQLITE_PCache_RekeyPage(sqlite3_pcache* pcache,
                          void*           page,
                          unsigned int    oldKey,
                          unsigned int    newKey)
{
    ((CNCDBCache*)pcache)->ChangePageKey(page, oldKey, newKey);
}

/// Truncate cache, delete all pages with keys greater or equal to given limit
static void
s_SQLITE_PCache_Truncate(sqlite3_pcache* pcache, unsigned int iLimit)
{
    ((CNCDBCache*)pcache)->DeleteAllPages(iLimit);
}

/// Destroy cache instance
static void
s_SQLITE_PCache_Destroy(sqlite3_pcache* pcache)
{
    delete ((CNCDBCache*)pcache);
}


/// All methods of database cache exposed to SQLite
static sqlite3_pcache_methods s_NCDBCacheMethods = {
    NULL,                       /* pArg */
    s_SQLITE_PCache_Init,       /* xInit */
    s_SQLITE_PCache_Shutdown,   /* xShutdown */
    s_SQLITE_PCache_Create,     /* xCreate */
    s_SQLITE_PCache_SetSize,    /* xCachesize */
    s_SQLITE_PCache_GetSize,    /* xPagecount */
    s_SQLITE_PCache_GetPage,    /* xFetch */
    s_SQLITE_PCache_UnpinPage,  /* xUnpin */
    s_SQLITE_PCache_RekeyPage,  /* xRekey */
    s_SQLITE_PCache_Truncate,   /* xTruncate */
    s_SQLITE_PCache_Destroy     /* xDestroy */
};



void
CNCDBCacheManager::SetMaxSize(size_t mem_size)
{
    CFastMutexGuard guard(s_CacheMutex);
    s_MemLimit = mem_size;
}

void
CNCDBCacheManager::Initialize(void)
{
    CSQLITE_Global::SetCustomPageCache(&s_NCDBCacheMethods);
}



/*
//DEFINE_STATIC_FAST_MUTEX(s_MemManagerMutex);
size_t s_MemUsed = 0;


inline void*
CNCMemoryChunk::AllocateGlobal(size_t size)
{
    size_t* res_size = (size_t*)malloc(size + sizeof(void*) + sizeof(size_t));
    s_MemUsed += size + sizeof(void*) + sizeof(size_t);
    LOG_POST("MemManager: global alloc, used " << s_MemUsed);
    *res_size = size;
    CNCMemoryChunk** res_chunk = (CNCMemoryChunk**)(res_size + 1);
    *res_chunk = NULL;
    ++res_chunk;
    return res_chunk;
}

inline void*
CNCMemoryChunk::Allocate(size_t size)
{
    if (m_CurPtr + size + sizeof(this) > m_EndPtr)
        return NULL;

    CNCMemoryChunk** result = (CNCMemoryChunk**)m_CurPtr;
    *result = this;
    ++result;
    m_CurPtr = (char*)result + size;
    ++m_AllocsCount;
    return result;
}

void
CNCMemoryChunk::Deallocate(void* ptr)
{
    void* mem_ptr = (CNCMemoryChunk**)ptr - 1;
    CNCMemoryChunk* chunk_ptr = *(CNCMemoryChunk**)mem_ptr;
    if (chunk_ptr) {
        CFastMutexGuard guard(s_MemManagerMutex);

        if (--chunk_ptr->m_AllocsCount == 0) {
            chunk_ptr->x_ResetDataStartPtr();
            CNCMemoryManager::ReturnFreeChunk(chunk_ptr);
        }
    }
    else {
        mem_ptr = (size_t*)mem_ptr - 1;
        s_MemUsed -= *(size_t*)mem_ptr + sizeof(void*);
        LOG_POST("MemManager: global dealloc, used " << s_MemUsed);
        free(mem_ptr);
    }
}


CNCMemoryManager&
CNCMemoryManager::x_Instance(void)
{
    static CSafeStaticPtr<CNCMemoryManager> instance;
    return instance.Get();
}

void*
CNCMemoryManager::x_AllocImpl(size_t size)
{
    void* ptr = NULL;
    if (size >= kNCMemManagerChunkSize) {
        ptr = CNCMemoryChunk::AllocateGlobal(size);
    }
    else {
        CFastMutexGuard guard(s_MemManagerMutex);

        if (m_CurChunk) {
            ptr = m_CurChunk->Allocate(size);
        }
        if (!ptr) {
            m_CurChunk = m_Pool.Get();
            ptr = m_CurChunk->Allocate(size);
            _ASSERT(ptr);
        }
    }
    return ptr;
}
*/
END_NCBI_SCOPE

/*
void*
operator new (std::size_t size)
{
    return NCBI_NS_NCBI::CNCMemoryManager::Allocate(size);
}

void
operator delete (void* ptr)
{
    NCBI_NS_NCBI::CNCMemoryManager::Deallocate(ptr);
}
*/
