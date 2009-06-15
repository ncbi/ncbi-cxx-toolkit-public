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
 *
 * File Description: 
 *
 */

#include <ncbi_pch.hpp>

#include <db/sqlite/sqlitewrapp.hpp>

#include "nc_memory.hpp"


BEGIN_NCBI_SCOPE


class CNCDBCache;

struct SNCDBMemoryPage
{
    CNCDBCache*      cache;
    unsigned int     key;
    size_t           size;
    SNCDBMemoryPage* prev_lru;
    SNCDBMemoryPage* next_lru;
    SNCDBMemoryPage* next_hash;
};

class CNCDBCache
{
public:
    CNCDBCache(size_t page_size, bool purgeable);
    ~CNCDBCache(void);

    void SuggestSize(int num_pages);
    int GetSize(void);
    size_t GetPageSize(void);

    enum EPageCreate {
        eDoNotCreate      = 0,
        eCreateIfPossible = 1,
        eMustCreate       = 2
    };
    void* GetPage(unsigned int key, EPageCreate create_flag);
    void ReleasePage(void* data, bool must_delete);
    void ChangePageKey(void* data, unsigned int old_key, unsigned int new_key);
    void DeleteAllPages(unsigned int min_key);

private:
    CNCDBCache(const CNCDBCache&);
    CNCDBCache& operator=(const CNCDBCache&);

    void x_AddToHash(SNCDBMemoryPage* page);
    void x_RemoveFromHash(SNCDBMemoryPage* page);
    SNCDBMemoryPage* x_GetFromHash(unsigned int key);


    typedef vector<SNCDBMemoryPage*>  TPagesHash;

    size_t       m_PageSize;
    bool         m_Purgable;
    int          m_CacheSize;
    unsigned int m_MaxKey;
    TPagesHash   m_PagesHash;
};



static const size_t kNC_DefDBCacheSize     = 1024 * 1024 * 1024;
static const int    kNC_DefDBCacheHashSize = 1024;

static SNCDBMemoryPage* s_LRUHead = NULL;
static SNCDBMemoryPage* s_LRUTail = NULL;
static size_t s_MemUsed  = 0;
static size_t s_RealMemUsed  = 0;
static size_t s_MemLimit = kNC_DefDBCacheSize;
DEFINE_STATIC_FAST_MUTEX(s_CacheMutex);


#define PCACHE_PAGEDATA(p)  (p + 1)
#define PCACHE_PAGECLASS(p) ((SNCDBMemoryPage*)p - 1)


inline size_t
CNCDBCache::GetPageSize(void)
{
    return m_PageSize;
}


static inline void
s_GetFromLRU(SNCDBMemoryPage* page)
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

static inline void
s_UnbindFromCache(SNCDBMemoryPage* page)
{
    page->cache = NULL;
    s_GetFromLRU(page);
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

static inline void
s_ReturnToLRU(SNCDBMemoryPage* page)
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

static inline SNCDBMemoryPage*
s_CreatePage(CNCDBCache* pcache)
{
    void* ptr = ::operator new(pcache->GetPageSize()
                               + sizeof(SNCDBMemoryPage));

    SNCDBMemoryPage* page = new(ptr) SNCDBMemoryPage;
    page->next_hash = page->prev_lru = page->next_lru = NULL;
    page->size = pcache->GetPageSize();
    s_MemUsed += page->size;
    s_RealMemUsed += page->size + sizeof(SNCDBMemoryPage);

    return page;
}

static inline void
s_DeletePage(SNCDBMemoryPage* page)
{
    s_MemUsed -= page->size;
    s_RealMemUsed -= page->size + sizeof(SNCDBMemoryPage);
    delete page;
}

inline
CNCDBCache::CNCDBCache(size_t page_size, bool purgeable)
    : m_PageSize(page_size),
      m_Purgable(purgeable),
      m_CacheSize(0),
      m_MaxKey(0),
      m_PagesHash(kNC_DefDBCacheHashSize, NULL)
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

    TPagesHash new_hash(num_pages, NULL);
    ITERATE(TPagesHash, it, m_PagesHash) {
        SNCDBMemoryPage* page = *it;
        while (page) {
            SNCDBMemoryPage* next = page->next_hash;
            page->next_hash = NULL;
            int hash_num = page->key & (num_pages - 1);
            SNCDBMemoryPage* hash_val = new_hash[hash_num];
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
CNCDBCache::x_RemoveFromHash(SNCDBMemoryPage* page)
{
    _ASSERT(page->cache == this);

    unsigned int hash_num = page->key & (m_PagesHash.size() - 1);
    SNCDBMemoryPage* hash_val = m_PagesHash[hash_num];
    if (hash_val == page) {
        m_PagesHash[hash_num] = page->next_hash;
    }
    else {
        SNCDBMemoryPage* prev = NULL;
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
CNCDBCache::x_AddToHash(SNCDBMemoryPage* page)
{
    unsigned int hash_num = page->key & (m_PagesHash.size() - 1);
#ifdef _DEBUG
    // According to SQLite's implementation of page cache this should not ever
    // happen. But to be on a safe side we better check it in Debug build.
    SNCDBMemoryPage* hash_val = m_PagesHash[hash_num];
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

inline SNCDBMemoryPage*
CNCDBCache::x_GetFromHash(unsigned int key)
{
    unsigned int hash_num = key & (m_PagesHash.size() - 1);
    SNCDBMemoryPage* page = m_PagesHash[hash_num];
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
        SNCDBMemoryPage** page_ptr = &m_PagesHash[i];
        for (SNCDBMemoryPage* page = *page_ptr; page; page = *page_ptr) {
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

    SNCDBMemoryPage* page = x_GetFromHash(key);

    if (!page) {
        if (create_flag == eDoNotCreate) {
            return NULL;
        }
        while (s_LRUHead  &&  s_LRUHead->size < m_PageSize) {
            SNCDBMemoryPage* head_pg = s_LRUHead;
            if (head_pg->cache) {
                head_pg->cache->x_RemoveFromHash(head_pg);
            }
            s_GetFromLRU(head_pg);
            s_DeletePage(head_pg);
        }
        if (s_LRUHead  &&  s_LRUHead->cache == NULL) {
            page = s_LRUHead;
        }
        if (!page  &&  s_LRUHead
            &&  (s_MemUsed >= s_MemLimit
                 ||  size_t(m_CacheSize) >= m_PagesHash.size()))
        {
            page = s_LRUHead;
            page->cache->x_RemoveFromHash(page);
        }
        if (!page  &&  create_flag == eCreateIfPossible
            &&  s_MemUsed >= s_MemLimit)
        {
            return NULL;
        }
        if (!page) {
            page = s_CreatePage(this);
        }

        page->cache = this;
        page->key = key;
        *(void**)PCACHE_PAGEDATA(page) = NULL;
        x_AddToHash(page);
    }

    s_GetFromLRU(page);
    return PCACHE_PAGEDATA(page);
}

inline void
CNCDBCache::ReleasePage(void* data, bool must_delete)
{
    CFastMutexGuard guard(s_CacheMutex);

    SNCDBMemoryPage* page = PCACHE_PAGECLASS(data);
    if (must_delete) {
        x_RemoveFromHash(page);
        s_UnbindFromCache(page);
    }
    else if (s_MemUsed - page->size > s_MemLimit) {
        x_RemoveFromHash(page);
        s_DeletePage(page);
    }
    else if (m_Purgable) {
        s_ReturnToLRU(page);
    }
}

inline void
CNCDBCache::ChangePageKey(void*        data,
                                 unsigned int old_key,
                                 unsigned int new_key)
{
    CFastMutexGuard guard(s_CacheMutex);

    SNCDBMemoryPage* page = PCACHE_PAGECLASS(data);
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


static int
s_SQLITE_PCache_Init(void*)
{
    return SQLITE_OK;
}

static void
s_SQLITE_PCache_Shutdown(void*)
{}

static sqlite3_pcache *
s_SQLITE_PCache_Create(int szPage, int bPurgeable)
{
    return (sqlite3_pcache*) new CNCDBCache(size_t(szPage),
                                                   bPurgeable != 0);
}

static void
s_SQLITE_PCache_SetSize(sqlite3_pcache* pcache, int nCachesize)
{
    ((CNCDBCache*)pcache)->SuggestSize(nCachesize);
}

static int
s_SQLITE_PCache_GetSize(sqlite3_pcache* pcache)
{
    return ((CNCDBCache*)pcache)->GetSize();
}

static void*
s_SQLITE_PCache_GetPage(sqlite3_pcache* pcache,
                        unsigned int    key,
                        int             createFlag)
{
    return ((CNCDBCache*)pcache)
                   ->GetPage(key, CNCDBCache::EPageCreate(createFlag));
}

static void
s_SQLITE_PCache_UnpinPage(sqlite3_pcache* pcache, void* page, int discard)
{
    ((CNCDBCache*)pcache)->ReleasePage(page, discard != 0);
}

static void
s_SQLITE_PCache_RekeyPage(sqlite3_pcache* pcache,
                          void*           page,
                          unsigned int    oldKey,
                          unsigned int    newKey)
{
    ((CNCDBCache*)pcache)->ChangePageKey(page, oldKey, newKey);
}

static void
s_SQLITE_PCache_Truncate(sqlite3_pcache* pcache, unsigned int iLimit)
{
    ((CNCDBCache*)pcache)->DeleteAllPages(iLimit);
}

static void
s_SQLITE_PCache_Destroy(sqlite3_pcache* pcache)
{
    delete ((CNCDBCache*)pcache);
}



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