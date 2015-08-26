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
 * Author: Pavel Ivanov
 *
 */

#include "task_server_pch.hpp"

#include "threads_man.hpp"
#include "memory_man.hpp"
#include "srv_stat.hpp"

#ifdef NCBI_OS_LINUX
# include <sys/mman.h>
#endif

// malloc hooks are deprecated
// in case they will disappear, we should have an alternative
#define __NC_MEMMAN_USE_MALLOC_HOOK 1

// adds several debug checks
#define __NC_MEMMAN_DEBUG 0
#define __NC_MEMMAN_ALLPTR_COUNT 0
// use per-thread MM stat or global static counters
// memory allocation is a foundation: it starts before main is called, before threads are created
// it seems using static counters makes more sense
#define __NC_MEMMAN_PERTHREAD_STAT 0

#if __NC_MEMMAN_PERTHREAD_STAT
#define __NC_MEMMAN_PERTHREAD_STAT_ARG(x) x
#else
#define __NC_MEMMAN_PERTHREAD_STAT_ARG(x)
#endif


BEGIN_NCBI_SCOPE;


static const Uint2 kMMCntBlocksInPool = 100;
static const Uint2 kMMDrainBatchSize = 35;
static const Uint1 kMMCntFreeGrades = 8;
static const int kMMFlushPeriod = 60;

/// If for some reason kMMAllocPageSize is changed then kMMMaxBlockSize will change
/// too and thus probably kNCMaxBlobChunkSize in nc_db_info.hpp in NetCache should
/// change correspondingly too.
static const Uint4 kMMAllocPageSize = 65536;
static const size_t kMMAllocPageMask = ~size_t(kMMAllocPageSize - 1);
/// This is Linux standard on x86_64. If it ever changes or some portability will
/// be desired then this constant will need to be obtained from OS during
/// initialization.
static const Uint2 kMMOSPageSize = 4096;
static const size_t kMMOSPageMask = ~size_t(kMMOSPageSize - 1);


struct SMMBlocksPool
{
    CMiniMutex pool_lock;
    Uint2 size_idx;
    Uint2 cnt_avail;
    Uint2 put_idx;
    Uint2 get_idx;
    void* blocks[kMMCntBlocksInPool];
};


struct SMMPageHeader
{
    size_t block_size;
    void* free_list;
    CMiniMutex page_lock;
    Uint2 cnt_free;
    Uint1 free_grade;
    SMMPageHeader* next_page;
    SMMPageHeader* prev_page;
};


struct SMMFreePageList
{
    CMiniMutex list_lock;
    SMMPageHeader list_head;
};


struct SMMFreePageGrades
{
    SMMFreePageList lists[kMMCntFreeGrades];
};


class CMMFlusher : public CSrvTask
{
public:
    CMMFlusher(void);
    virtual ~CMMFlusher(void);

private:
    virtual void ExecuteSlice(TSrvThreadNum thr_num);
};


struct SMMMemPoolsSet
{
    Uint4 flush_counter;
    SMMBlocksPool pools[kMMCntBlockSizes];
    SMMStat stat;
};


static bool s_HadLowLevelInit = false;
static bool s_HadMemMgrInit = false;
static SMMMemPoolsSet s_GlobalPoolsSet;
static SMMFreePageGrades s_FreePages[kMMCntBlockSizes];
static SMMMemPoolsSet s_MainPoolsSet;
static CMMFlusher* s_Flusher;
static Uint8 s_TotalSysMem = 0;
static Int8 s_TotalPageCount = 0;
static SMMStateStat s_StartState;

#if !__NC_MEMMAN_PERTHREAD_STAT
Uint8 s_UserBlAlloced[kMMCntBlockSizes];
Uint8 s_UserBlFreed[kMMCntBlockSizes];
Uint8 s_SysBlAlloced[kMMCntBlockSizes];
Uint8 s_SysBlFreed[kMMCntBlockSizes];
Uint8 s_BigAllocedCnt = 0;
Uint8 s_BigAllocedSize = 0;
Uint8 s_BigFreedCnt = 0;
Uint8 s_BigFreedSize = 0;
#endif

#if __NC_MEMMAN_ALLPTR_COUNT
static Int8  s_AllPtrCount[kMMCntBlockSizes] = {0};
static Int8  s_AllSysPtrCount[kMMCntBlockSizes] = {0};
#endif
#if __NC_MEMMAN_DEBUG
static Uint8 s_fdMemManStamp = 0xFEEDFEEDFEEDFEED;
static const Uint8 s_MaxAllPools = 200;
static SMMMemPoolsSet*  s_AllMemPoolsSet[s_MaxAllPools];
static CMiniMutex lock_AllPools;

static void s_IncPoolIdx(Uint2& idx);
static SMMPageHeader* s_GetPageByPtr(void* ptr);
// verify that this ptr is not listed in any pool as available
static void s_VerifyUnavailable( void* ptr, Uint2 size_idx)
{
    for (Uint8 p = 0; p < s_MaxAllPools; ++p) {
        SMMMemPoolsSet *pset = s_AllMemPoolsSet[p];
        if (pset) {
            const SMMBlocksPool* test_pool = &(pset->pools[size_idx]);
            for (Uint2 idx = test_pool->get_idx; idx != test_pool->put_idx; s_IncPoolIdx(idx)) {
                void* available = test_pool->blocks[idx];
                if (available == ptr) {
                    abort();
                }
            }
        }
    }
    SMMPageHeader* page = s_GetPageByPtr(ptr);
    for (void* next_block = page->free_list; next_block; next_block = *(void**)next_block) {
        if (next_block == ptr) {
            abort();
        }
    }
}

#if 0
// need much more, 200K, 2M ?
static const Uint8 s_MaxAllPtr = 20000;
static void* s_AllPtr[s_MaxAllPtr*kMMCntBlockSizes];
#endif

static void s_GivePtr( void* ptr, Uint2 size_idx)
{
#if 0
    void** p = s_AllPtr + (size_idx * s_MaxAllPtr);
    for( Uint8 i = 0; i < s_MaxAllPtr; ++i, ++p) {
        if (*p == nullptr) {
            *p = ptr;
            return;
        }
    }
    abort();
#endif
}
static void s_TakePtr( void* ptr, Uint2 size_idx)
{
#if 0
    void** p = s_AllPtr + (size_idx * s_MaxAllPtr);
    for( Uint8 i = 0; i < s_MaxAllPtr; ++i, ++p) {
        if (*p == ptr) {
            *p = nullptr;
            return;
        }
    }
    abort();
#endif
}

#endif //__NC_MEMMAN_DEBUG

static const Uint4 kMMPageDataSize = kMMAllocPageSize - sizeof(SMMPageHeader);
static const Uint2 kMMMaxBlockSize = (kMMPageDataSize / 2) & ~7;
static const Uint2 kMMBlockSizes[kMMCntBlockSizes] =
    { 8, 16, 24, 32, 40, 48, 56, 64, 72, 80, 96, 112, 128, 152, 176, 208, 248,
      296, 352, 416, 496, 592, 704, 840, 1008, 1208, 1448, 1736, 2080, 2496,
      (kMMPageDataSize / 11) & ~7,
      (kMMPageDataSize / 9) & ~7,
      (kMMPageDataSize / 8) & ~7,
      (kMMPageDataSize / 7) & ~7,
      (kMMPageDataSize / 6) & ~7,
      (kMMPageDataSize / 5) & ~7,
      (kMMPageDataSize / 4) & ~7,
      (kMMPageDataSize / 3) & ~7,
      kMMMaxBlockSize
    };
static Uint2 kMMSizeIndexes[kMMMaxBlockSize / 8 + 1] = {0};
static Uint2 kMMCntForSize[kMMCntBlockSizes] = {0};


Int8   GetMPageCount(void)
{
    return s_TotalPageCount;
}

static inline void
s_InitPoolsSet(SMMMemPoolsSet* pool_set)
{
    for (Uint2 i = 0; i < kMMCntBlockSizes; ++i) {
        SMMBlocksPool& pool = pool_set->pools[i];
        pool.size_idx = i;
        pool.get_idx = pool.put_idx = 0;
        pool.cnt_avail = 0;
    }
    pool_set->flush_counter = s_GlobalPoolsSet.flush_counter;
    pool_set->stat.ClearStats();
}

static void
s_LowLevelInit(void)
{
    InitCurThreadStorage();

    Uint4 sz_ind = 0, sz = 0, lookup_ind = 0;
    for (; sz <= kMMMaxBlockSize; sz += 8, ++lookup_ind) {
        if (sz > kMMBlockSizes[sz_ind])
            ++sz_ind;
        kMMSizeIndexes[lookup_ind] = sz_ind;
    }
    s_InitPoolsSet(&s_MainPoolsSet);
    s_InitPoolsSet(&s_GlobalPoolsSet);
#if __NC_MEMMAN_DEBUG
    s_AllMemPoolsSet[s_MaxAllPools-1] = &s_GlobalPoolsSet;
#endif
    for (Uint2 i = 0; i < kMMCntBlockSizes; ++i) {
        kMMCntForSize[i] = Uint2(kMMPageDataSize / kMMBlockSizes[i]);

        SMMFreePageGrades& free_grades = s_FreePages[i];
        for (Uint2 j = 0; j < kMMCntFreeGrades; ++j) {
            SMMPageHeader* free_head = &free_grades.lists[j].list_head;
            free_head->next_page = free_head;
            free_head->prev_page = free_head;
        }
    }
    
    s_HadLowLevelInit = true;
}

static inline void*
s_DoMmap(size_t size)
{
#ifdef NCBI_OS_LINUX
    void* ptr = mmap(NULL, size, PROT_READ | PROT_WRITE,
                     MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (ptr == MAP_FAILED) {
        static bool reentry = false;
        if (reentry) {
            abort();
        } else {
            reentry = true;
            SRV_FATAL("s_DoMmap failed when requesting " << size << " bytes, errno: " << errno);
        }
    }
    return ptr;
#else
    return NULL;
#endif
}

static inline void
s_DoUnmap(void* ptr, size_t size)
{
#ifdef NCBI_OS_LINUX
    if (munmap(ptr, size) != 0) {
        SRV_FATAL("s_DoUnmap failed, errno: " << errno);
    }
#endif
}

static void*
s_SysAllocLongWay(size_t size)
{
    size_t ptr = (size_t)s_DoMmap(size + kMMAllocPageSize);
    size_t aligned_ptr = ptr & kMMAllocPageMask;
    if (aligned_ptr == ptr) {
        s_DoUnmap((void*)(ptr + size), kMMAllocPageSize);
    }
    else {
        aligned_ptr += kMMAllocPageSize;
        s_DoUnmap((void*)ptr, aligned_ptr - ptr);
        s_DoUnmap((void*)(aligned_ptr + size),
                  kMMAllocPageSize - (aligned_ptr - ptr));
    }
    return (void*)aligned_ptr;
}

static void*
s_SysAlloc(size_t size)
{
    AtomicAdd(s_TotalSysMem, size);
    AtomicAdd(s_TotalPageCount, 1);
    size_t ptr = (size_t)s_DoMmap(size);
    if ((ptr & kMMAllocPageMask) == ptr)
        return (void*)ptr;

    s_DoUnmap((void*)ptr, size);
    return s_SysAllocLongWay(size);
}

static void
s_SysFree(void* ptr, size_t size)
{
    AtomicSub(s_TotalSysMem, size);
    AtomicSub(s_TotalPageCount, 1);
    s_DoUnmap(ptr, size);
}

static inline SMMPageHeader*
s_GetPageByPtr(void* ptr)
{
    return (SMMPageHeader*)((size_t)ptr & kMMAllocPageMask);
}

static inline bool
s_IsInFreeList(SMMPageHeader* page)
{
    return page->next_page != NULL;
}

static inline bool
s_IsFreeListEmpty(SMMPageHeader* list_head)
{
    return list_head->next_page == list_head;
}

static inline void
s_FreeListRemove(SMMPageHeader* page)
{
    page->prev_page->next_page = page->next_page;
    page->next_page->prev_page = page->prev_page;
    page->prev_page = page->next_page = NULL;
}

static inline void
s_FreeListAddHead(SMMPageHeader* list_head, SMMPageHeader* page)
{
#if __NC_MEMMAN_DEBUG
    if (list_head->block_size != 0 && list_head->block_size != page->block_size) {
        abort();
    }
#endif
    page->prev_page = list_head;
    page->next_page = list_head->next_page;
    page->next_page->prev_page = page;
    list_head->next_page = page;
}

static inline void
s_FreeListAddTail(SMMPageHeader* list_head, SMMPageHeader* page)
{
#if __NC_MEMMAN_DEBUG
    if (list_head->block_size != 0 && list_head->block_size != page->block_size) {
        abort();
    }
#endif
    page->next_page = list_head;
    page->prev_page = list_head->prev_page;
    page->prev_page->next_page = page;
    list_head->prev_page = page;
}

static inline void
s_IncPoolIdx(Uint2& idx)
{
    ++idx;
    if (idx == kMMCntBlocksInPool)
        idx = 0;
}

static void
s_PutToFreeList(SMMPageHeader* page, Uint2 size_idx, bool to_head)
{
    Uint1 grade = Uint1(Uint4(page->cnt_free) * kMMCntFreeGrades
                            / kMMCntForSize[size_idx]);
    page->free_grade = grade;
    SMMFreePageGrades& free_grades = s_FreePages[size_idx];
    SMMFreePageList& free_list = free_grades.lists[grade];
    free_list.list_lock.Lock();
    if (to_head)
        s_FreeListAddHead(&free_list.list_head, page);
    else
        s_FreeListAddTail(&free_list.list_head, page);
    free_list.list_lock.Unlock();
}

static bool
s_RemoveFromFreeList(SMMPageHeader* page, Uint2 size_idx)
{
    SMMFreePageGrades& free_grades = s_FreePages[size_idx];
    SMMFreePageList& free_list = free_grades.lists[page->free_grade];
    free_list.list_lock.Lock();
    bool result = s_IsInFreeList(page);
    if (result)
        s_FreeListRemove(page);
    free_list.list_lock.Unlock();
    return result;
}

static SMMPageHeader*
s_AllocNewPage(Uint2 size_idx, SMMStat* __NC_MEMMAN_PERTHREAD_STAT_ARG(stat))
{
    SMMPageHeader* page = (SMMPageHeader*)s_SysAlloc(kMMAllocPageSize);
    new (page) SMMPageHeader();
    page->block_size = kMMBlockSizes[size_idx];
    page->cnt_free = kMMCntForSize[size_idx];
#if __NC_MEMMAN_PERTHREAD_STAT
    AtomicAdd(stat->m_SysBlAlloced[size_idx], page->cnt_free);
#else
    AtomicAdd(s_SysBlAlloced[size_idx], page->cnt_free);
#endif
#if __NC_MEMMAN_ALLPTR_COUNT
    AtomicAdd(s_AllSysPtrCount[size_idx], page->cnt_free);
#endif
    page->next_page = page->prev_page = NULL;

    char* block = (char*)page + sizeof(SMMPageHeader);
    page->free_list = block;
#if __NC_MEMMAN_DEBUG
    if (page->free_list && memcmp(page->free_list, &s_fdMemManStamp, sizeof(s_fdMemManStamp)) == 0) {
        abort();
    }
#endif
    char* next = block + page->block_size;
    for (Uint2 i = page->cnt_free - 1; i > 0; --i) {
        *(void**)block = next;
        block = next;
        next += page->block_size;
    }
    *(void**)block = NULL;
    _ASSERT(next + page->block_size > (char*)page + kMMAllocPageSize);

    return page;
}

static void*
s_FillFromPage(SMMBlocksPool* pool, Uint2 size_idx, SMMPageHeader* page)
{
    _ASSERT(!pool  ||  pool->cnt_avail == 0);
    page->page_lock.Lock();
    void* next_block = page->free_list;
    void* result = next_block;
    next_block = *(void**)next_block;
    --page->cnt_free;
#if __NC_MEMMAN_DEBUG
    Uint2 prev_cnt_avail = pool->cnt_avail;
#endif
    Uint2 to_fill = min(kMMDrainBatchSize, page->cnt_free);
    page->cnt_free -= to_fill;
    for (; to_fill != 0; --to_fill) {
        pool->blocks[pool->cnt_avail++] = next_block;
        next_block = *(void**)next_block;
    }
    pool->get_idx = 0;
    pool->put_idx = pool->cnt_avail;
    page->free_list = next_block;
#if __NC_MEMMAN_DEBUG
    if (page->free_list && memcmp(page->free_list, &s_fdMemManStamp, sizeof(s_fdMemManStamp)) == 0) {
        abort();
    }
    for (Uint2 cnt = prev_cnt_avail; cnt < pool->cnt_avail; ++cnt) {
        memcpy(pool->blocks[cnt], &s_fdMemManStamp, sizeof(s_fdMemManStamp));
    }
    if (page->free_list && memcmp(page->free_list, &s_fdMemManStamp, sizeof(s_fdMemManStamp)) == 0) {
        abort();
    }
#endif
    if (page->cnt_free)
        s_PutToFreeList(page, size_idx, true);
    page->page_lock.Unlock();

#if __NC_MEMMAN_DEBUG
    memcpy(result, &s_fdMemManStamp, sizeof(s_fdMemManStamp));
    if (page->free_list && memcmp(page->free_list, &s_fdMemManStamp, sizeof(s_fdMemManStamp)) == 0) {
        abort();
    }
#endif
    return result;
}

static void*
s_FillFromFreePages(SMMBlocksPool* pool, Uint2 size_idx, SMMStat* stat)
{
    SMMPageHeader* free_page = NULL;
    SMMFreePageGrades& free_grades = s_FreePages[size_idx];
    for (Uint1 i = 0; i < kMMCntFreeGrades  &&  !free_page; ++i) {
        SMMFreePageList& free_list = free_grades.lists[i];
        SMMPageHeader* free_head = &free_list.list_head;

        free_list.list_lock.Lock();
        if (!s_IsFreeListEmpty(free_head)) {
            free_page = free_head->next_page;
            s_FreeListRemove(free_page);
        }
        free_list.list_lock.Unlock();
    }
    if (!free_page)
        free_page = s_AllocNewPage(size_idx, stat);

    return s_FillFromPage(pool, size_idx, free_page);
}

static void
s_ReleaseToFreePages(void** blocks, Uint2 cnt, 
    Uint2 size_idx, SMMStat* __NC_MEMMAN_PERTHREAD_STAT_ARG(stat))
{
    for (Uint4 i = 0; i < cnt; ++i) {
        void* ptr = blocks[i];
        SMMPageHeader* page = s_GetPageByPtr(ptr);
        page->page_lock.Lock();
        *(void**)ptr = page->free_list;
#if __NC_MEMMAN_DEBUG
        if (page->free_list && memcmp(page->free_list, &s_fdMemManStamp, sizeof(s_fdMemManStamp)) == 0) {
            abort();
        }
#endif
        page->free_list = ptr;
#if __NC_MEMMAN_DEBUG
        if (page->free_list && memcmp(page->free_list, &s_fdMemManStamp, sizeof(s_fdMemManStamp)) == 0) {
            abort();
        }
#endif
        ++page->cnt_free;
        if (page->cnt_free == 1  ||  s_RemoveFromFreeList(page, size_idx)) {
            if (page->cnt_free == kMMCntForSize[size_idx]) {
#if __NC_MEMMAN_PERTHREAD_STAT
                AtomicAdd(stat->m_SysBlFreed[size_idx], page->cnt_free);
#else
                AtomicAdd(s_SysBlFreed[size_idx], page->cnt_free);
#endif
#if __NC_MEMMAN_ALLPTR_COUNT
                AtomicSub(s_AllSysPtrCount[size_idx], page->cnt_free);
#endif
                s_SysFree(page, kMMAllocPageSize);
                continue;
            }
            else {
                s_PutToFreeList(page, size_idx, false);
            }
        }
        page->page_lock.Unlock();
    }
}

static void
s_FlushPoolSet(SMMMemPoolsSet* pool_set)
{
    for (Uint2 i = 0; i < kMMCntBlockSizes; ++i) {
        SMMBlocksPool& pool = pool_set->pools[i];
        if (pool.cnt_avail == 0)
            continue;

        if (pool.get_idx < pool.put_idx) {
            Uint2 cnt = pool.put_idx - pool.get_idx;
            s_ReleaseToFreePages(&pool.blocks[pool.get_idx], cnt, i, &pool_set->stat);
        }
        else {
            Uint2 cnt = kMMCntBlocksInPool - pool.get_idx;
            s_ReleaseToFreePages(&pool.blocks[pool.get_idx], cnt, i, &pool_set->stat);
            if (pool.put_idx)
                s_ReleaseToFreePages(&pool.blocks[0], pool.put_idx, i, &pool_set->stat);
        }
        pool.get_idx = pool.put_idx = 0;
        pool.cnt_avail = 0;
    }
    pool_set->flush_counter = s_GlobalPoolsSet.flush_counter;
}

static void*
s_FillPool(SMMBlocksPool* pool, SMMStat* stat)
{
    SMMBlocksPool& glob_pool = s_GlobalPoolsSet.pools[pool->size_idx];

    glob_pool.pool_lock.Lock();
    if (glob_pool.cnt_avail == 0) {
        glob_pool.pool_lock.Unlock();
        return s_FillFromFreePages(pool, pool->size_idx, stat);
    }

    _ASSERT(pool->cnt_avail == 0);
    void* result = glob_pool.blocks[0];
    --glob_pool.cnt_avail;
    Uint4 cnt_copy = min(glob_pool.cnt_avail, kMMDrainBatchSize);
    if (cnt_copy != 0) {
        memcpy(pool->blocks, &glob_pool.blocks[1], cnt_copy * sizeof(void*));
#if __NC_MEMMAN_DEBUG
        for (Uint2 cnt = 0; cnt < cnt_copy; ++cnt) {
            memcpy(pool->blocks[cnt], &s_fdMemManStamp, sizeof(s_fdMemManStamp));
        }
#endif
        pool->cnt_avail = cnt_copy;
        pool->get_idx = 0;
        pool->put_idx = cnt_copy;
        glob_pool.cnt_avail -= cnt_copy;
        if (glob_pool.cnt_avail != 0) {
            memmove(glob_pool.blocks, &glob_pool.blocks[cnt_copy + 1],
                    glob_pool.cnt_avail * sizeof(void*));
        }
    }
    glob_pool.pool_lock.Unlock();

    return result;
}

static void*
s_GetFromGlobal(Uint2 size_idx, SMMStat* stat)
{
    SMMBlocksPool& pool = s_GlobalPoolsSet.pools[size_idx];

    pool.pool_lock.Lock();
    if (pool.cnt_avail == 0) {
        pool.pool_lock.Unlock();
        return s_FillFromFreePages(NULL, size_idx, stat);
    }
    void* ptr = pool.blocks[--pool.cnt_avail];
    pool.pool_lock.Unlock();

    return ptr;
}

static void
s_DrainPool(SMMBlocksPool* pool, void* ptr, SMMStat* stat)
{
    pool->get_idx = 0;
    pool->put_idx = kMMCntBlocksInPool;

    SMMBlocksPool& glob_pool = s_GlobalPoolsSet.pools[pool->size_idx];

    glob_pool.pool_lock.Lock();
    if (glob_pool.cnt_avail == kMMCntBlocksInPool) {
        glob_pool.pool_lock.Unlock();

        pool->put_idx = kMMCntBlocksInPool - kMMDrainBatchSize;
        pool->cnt_avail = pool->put_idx;
        s_ReleaseToFreePages(&pool->blocks[pool->put_idx],
                             kMMDrainBatchSize, pool->size_idx, stat);
        s_ReleaseToFreePages(&ptr, 1, pool->size_idx, stat);
    }
    else {
        glob_pool.blocks[glob_pool.cnt_avail++] = ptr;
        Uint4 to_copy = kMMCntBlocksInPool - glob_pool.cnt_avail;
        if (to_copy > kMMDrainBatchSize)
            to_copy = kMMDrainBatchSize;
        pool->put_idx = kMMCntBlocksInPool - to_copy;
        pool->cnt_avail = pool->put_idx;
        memcpy(&glob_pool.blocks[glob_pool.cnt_avail],
               &pool->blocks[pool->put_idx],
               to_copy * sizeof(void*));
        glob_pool.cnt_avail += to_copy;
        glob_pool.pool_lock.Unlock();
    }
}

static void*
s_GetFromPool(SMMBlocksPool* pool, SMMStat* stat)
{
    void* ptr = nullptr;
    if (pool->cnt_avail == 0) {
        ptr = s_FillPool(pool, stat);
    } else {
        --pool->cnt_avail;
        ptr = pool->blocks[pool->get_idx];
        s_IncPoolIdx(pool->get_idx);
        _ASSERT((pool->put_idx + kMMCntBlocksInPool - pool->get_idx)
                    % kMMCntBlocksInPool == pool->cnt_avail);
    }
    return ptr;
}

static void
s_PutToPool(SMMBlocksPool* pool, void* ptr, SMMStat* stat)
{
    if (pool->cnt_avail == kMMCntBlocksInPool) {
        s_DrainPool(pool, ptr, stat);
    }
    else {
        ++pool->cnt_avail;
        pool->blocks[pool->put_idx] = ptr;
        s_IncPoolIdx(pool->put_idx);
        _ASSERT((pool->put_idx + kMMCntBlocksInPool - pool->get_idx - 1)
                    % kMMCntBlocksInPool + 1 == pool->cnt_avail);
    }
}

static inline size_t
s_CalcBigPageSize(size_t size)
{
    size_t alloc_size = size + sizeof(SMMPageHeader);
    return (alloc_size + kMMOSPageSize - 1) & kMMOSPageMask;
}

static void*
s_AllocBigPage(size_t size, SMMStat* __NC_MEMMAN_PERTHREAD_STAT_ARG(stat))
{
    size = s_CalcBigPageSize(size);
    SMMPageHeader* page = (SMMPageHeader*)s_SysAlloc(size);
#if __NC_MEMMAN_PERTHREAD_STAT
    AtomicAdd(stat->m_BigAllocedCnt, 1);
    AtomicAdd(stat->m_BigAllocedSize, size);
#else
    AtomicAdd(s_BigAllocedCnt, 1);
    AtomicAdd(s_BigAllocedSize, size);
#endif
    page->block_size = size - sizeof(SMMPageHeader);

#if __NC_MEMMAN_DEBUG
    memcpy(&page[1], &s_fdMemManStamp, sizeof(s_fdMemManStamp));
    if (page->free_list && memcmp(page->free_list, &s_fdMemManStamp, sizeof(s_fdMemManStamp)) == 0) {
        abort();
    }
#endif
    return &page[1];
}

static void
s_DeallocBigPage(SMMPageHeader* page, SMMStat* __NC_MEMMAN_PERTHREAD_STAT_ARG(stat))
{
    size_t size = page->block_size + sizeof(SMMPageHeader);
    s_SysFree(page, size);
#if __NC_MEMMAN_PERTHREAD_STAT
    AtomicAdd(stat->m_BigFreedCnt, 1);
    AtomicAdd(stat->m_BigFreedSize, size);
#else
    AtomicAdd(s_BigFreedCnt, 1);
    AtomicAdd(s_BigFreedSize, size);
#endif
}

static inline Uint2
s_CalcSizeIndex(size_t size)
{
    return kMMSizeIndexes[(size + 7) / 8];
}

static inline SMMMemPoolsSet*
s_GetCurPoolsSet(void)
{
    SSrvThread* thr = GetCurThread();
    if (thr) {
        SMMMemPoolsSet* pool_set = thr->mm_pool;
        if (pool_set->flush_counter != s_GlobalPoolsSet.flush_counter)
            s_FlushPoolSet(pool_set);
        return pool_set;
    }
    else if (s_HadMemMgrInit)
        return NULL;
    else
        return &s_MainPoolsSet;
}

static void*
s_AllocMemory(size_t size)
{
    if (!s_HadLowLevelInit)
        s_LowLevelInit();

#if __NC_MEMMAN_DEBUG
    lock_AllPools.Lock();
#endif
    SMMMemPoolsSet* pool_set = s_GetCurPoolsSet();
    SMMStat* stat = (pool_set? &pool_set->stat: &s_MainPoolsSet.stat);
    void* ptr = nullptr;
    if (size <= kMMMaxBlockSize) {
        Uint2 size_idx = s_CalcSizeIndex(size);
#if __NC_MEMMAN_PERTHREAD_STAT
        AtomicAdd(stat->m_UserBlAlloced[size_idx], 1);
#else
        AtomicAdd(s_UserBlAlloced[size_idx], 1);
#endif
        if (pool_set) {
            ptr = s_GetFromPool(&pool_set->pools[size_idx], stat);
        } else {
            ptr = s_GetFromGlobal(size_idx, stat);
        }
#if __NC_MEMMAN_ALLPTR_COUNT
        AtomicAdd(s_AllPtrCount[size_idx],1);
#endif
#if __NC_MEMMAN_DEBUG
        s_GivePtr(ptr,size_idx);
        if (s_FreePages[size_idx].lists[0].list_head.block_size != 0 &&
            s_FreePages[size_idx].lists[0].list_head.block_size !=  kMMBlockSizes[size_idx]) {
            abort();
        }
//        s_VerifyUnavailable(ptr, size_idx);
#endif
    }
    else {
        ptr = s_AllocBigPage(size, stat);
    }
#if __NC_MEMMAN_DEBUG
    if (memcmp(ptr, &s_fdMemManStamp, sizeof(s_fdMemManStamp)) != 0) {
        abort();
    }
    memset(ptr, 0, sizeof(s_fdMemManStamp));
#endif
#if __NC_MEMMAN_DEBUG
    lock_AllPools.Unlock();
#endif
    return ptr;
}

static void
s_DeallocMemory(void* ptr)
{
#if !__NC_MEMMAN_USE_MALLOC_HOOK
    if (!s_HadLowLevelInit) {
        return;
    }
#endif
    if (!ptr) {
        return;
    }

#if __NC_MEMMAN_DEBUG
    lock_AllPools.Lock();
#endif
    SMMMemPoolsSet* pool_set = s_GetCurPoolsSet();
    SMMStat* stat = (pool_set? &pool_set->stat: &s_MainPoolsSet.stat);
    SMMPageHeader* page = s_GetPageByPtr(ptr);
    if (page->block_size <= kMMMaxBlockSize) {
        Uint2 size_idx = s_CalcSizeIndex(page->block_size);
#if __NC_MEMMAN_PERTHREAD_STAT
        AtomicAdd(stat->m_UserBlFreed[size_idx], 1);
#else
        AtomicAdd(s_UserBlFreed[size_idx], 1);
#endif
#if __NC_MEMMAN_ALLPTR_COUNT
        AtomicSub(s_AllPtrCount[size_idx],1);
#endif
#if __NC_MEMMAN_DEBUG
        s_TakePtr(ptr,size_idx);
        if (memcmp(ptr, &s_fdMemManStamp, sizeof(s_fdMemManStamp)) == 0) {
            abort();
        }
        memcpy(ptr, &s_fdMemManStamp, sizeof(s_fdMemManStamp));
        s_VerifyUnavailable(ptr, size_idx);
#endif
        if (pool_set) {
            s_PutToPool(&pool_set->pools[size_idx], ptr, stat);
        } else {
            s_ReleaseToFreePages(&ptr, 1, size_idx, stat);
        }
// here,  'page' may be freed already
    }
    else {
#if __NC_MEMMAN_DEBUG
        if (memcmp(ptr, &s_fdMemManStamp, sizeof(s_fdMemManStamp)) == 0) {
            abort();
        }
        memcpy(ptr, &s_fdMemManStamp, sizeof(s_fdMemManStamp));
        if (page->free_list && memcmp(page->free_list, &s_fdMemManStamp, sizeof(s_fdMemManStamp)) == 0) {
            abort();
        }
#endif
        s_DeallocBigPage(page, stat);
    }
#if __NC_MEMMAN_DEBUG
    lock_AllPools.Unlock();
#endif
}

static void*
s_ReallocMemory(void* ptr, size_t size)
{
    if (!ptr)
        return s_AllocMemory(size);

    SMMPageHeader* page = s_GetPageByPtr(ptr);
    if (size <= kMMMaxBlockSize) {
        Uint2 size_idx = s_CalcSizeIndex(size);
        size_t real_size = kMMBlockSizes[size_idx];
        if (real_size == page->block_size)
            return ptr;
    }
    else {
        size_t real_size = s_CalcBigPageSize(size) - sizeof(SMMPageHeader);
        if (real_size == page->block_size)
            return ptr;
    }

    void* new_ptr = s_AllocMemory(size);
    memcpy(new_ptr, ptr, min(size, page->block_size));
    s_DeallocMemory(ptr);
    return new_ptr;
}

size_t
GetMemSize(void* ptr)
{
    SMMPageHeader* page = s_GetPageByPtr(ptr);
    return page->block_size;
}

void
InitMemoryMan(void)
{
    s_Flusher = new CMMFlusher();
    s_Flusher->RunAfter(kMMFlushPeriod);

    s_HadMemMgrInit = true;
}

void FinalizeMemoryMan(void)
{
// when NOT using malloc hooks, when the app terminates and static objects are being deleted
// there are attempts to free "wrong" memory and the app coredumps
// I did not investigate that in details. Probably, not all allocations are truly intercepted
// and some static objects are created using one allocator then deleted using another one.
// From another side, it is probably safe to just ignore memory freeing at this time.
// so, this is what we do. After this, all s_DeallocMemory calls will do nothing
#if !__NC_MEMMAN_USE_MALLOC_HOOK
    s_HadLowLevelInit = false;
#endif
}

void
AssignThreadMemMgr(SSrvThread* thr)
{
    if (thr->thread_num == 0) {
        thr->mm_pool = &s_MainPoolsSet;
    }
    else {
        SMMMemPoolsSet* pool_set = new SMMMemPoolsSet();
        s_InitPoolsSet(pool_set);
        thr->mm_pool = pool_set;
    }
#if __NC_MEMMAN_DEBUG
    if (thr->thread_num < s_MaxAllPools) {
        s_AllMemPoolsSet[thr->thread_num] = thr->mm_pool;
    } else {
        abort();
    }
#endif
    // Per-thread stat is never deleted, thus we can do this trick
    thr->stat->SetMMStat(&thr->mm_pool->stat);
}

void
ReleaseThreadMemMgr(SSrvThread* thr)
{
    s_FlushPoolSet(thr->mm_pool);
}


CMMFlusher::CMMFlusher(void)
{
#if __NC_TASKS_MONITOR
    m_TaskName = "CMMFlusher";
#endif
}

CMMFlusher::~CMMFlusher(void)
{}

void
CMMFlusher::ExecuteSlice(TSrvThreadNum /* thr_num */)
{

    if (CTaskServer::IsInShutdown())
        return;

// move blocks from global pool into pages.
// then, threads, when they see  s_GlobalPoolsSet.flush_counter changed,
// return their pool blocks into pages

    void* buffer[kMMCntBlocksInPool];
    SMMStat* stat = &GetCurThread()->mm_pool->stat;
    for (Uint2 i = 0; i < kMMCntBlockSizes; ++i) {
        SMMBlocksPool& pool = s_GlobalPoolsSet.pools[i];

        pool.pool_lock.Lock();
        Uint2 cnt_blocks = pool.cnt_avail;
        if (cnt_blocks == 0) {
            pool.pool_lock.Unlock();
            continue;
        }
        pool.cnt_avail = 0;
        memcpy(buffer, pool.blocks, cnt_blocks * sizeof(void*));
        pool.pool_lock.Unlock();

        s_ReleaseToFreePages(buffer, cnt_blocks, i, stat);
    }
    ++s_GlobalPoolsSet.flush_counter;

// once a minute
    RunAfter(kMMFlushPeriod);
}


void
SMMStat::InitStartState(void)
{
    if (s_StartState.m_TotalSys == 0) {
#if __NC_MEMMAN_PERTHREAD_STAT
        Uint8 total_data = 0;
        SMMStat* main_stat = &s_MainPoolsSet.stat;
        for (Uint1 i = 0; i < kMMCntBlockSizes; ++i) {
            Int8 cnt = main_stat->m_UserBlAlloced[i] - main_stat->m_UserBlFreed[i];
            s_StartState.m_UserBlocks[i] = cnt;
            total_data += cnt * kMMBlockSizes[i];
            s_StartState.m_SysBlocks[i]  = main_stat->m_SysBlAlloced[i]
                                            - main_stat->m_SysBlFreed[i];
        }
        s_StartState.m_BigBlocksCnt = main_stat->m_BigAllocedCnt
                                        - main_stat->m_BigFreedCnt;
        Int8 size = main_stat->m_BigAllocedSize - main_stat->m_BigFreedSize;
        s_StartState.m_BigBlocksSize = size;
        total_data += size;
        s_StartState.m_TotalData = total_data;
        s_StartState.m_TotalSys = s_TotalSysMem;
#else
        Uint8 total_data = 0;
        for (Uint1 i = 0; i < kMMCntBlockSizes; ++i) {
            Int8 cnt = s_UserBlAlloced[i] - s_UserBlFreed[i];
            s_StartState.m_UserBlocks[i] = cnt;
            total_data += cnt * kMMBlockSizes[i];
            s_StartState.m_SysBlocks[i] = s_SysBlAlloced[i] - s_SysBlFreed[i];
        }
        s_StartState.m_BigBlocksCnt = s_BigAllocedCnt - s_BigFreedCnt;
        Int8 size = s_BigAllocedSize - s_BigFreedSize;
        s_StartState.m_BigBlocksSize = size;
        total_data += size;
        s_StartState.m_TotalData = total_data;
        s_StartState.m_TotalSys = s_TotalSysMem;
#endif
    }

    m_EndState = m_StartState = s_StartState;
}

void
SMMStat::TransferEndState(SMMStat* src_stat)
{
    m_EndState = m_StartState = src_stat->m_EndState;
}

void
SMMStat::CopyStartState(SMMStat* src_stat)
{
    m_StartState = src_stat->m_StartState;
}

void
SMMStat::CopyEndState(SMMStat* src_stat)
{
    m_EndState = src_stat->m_EndState;
}

void
SMMStat::SaveEndState(void)
{
#if __NC_MEMMAN_PERTHREAD_STAT
    Uint8 total_data = 0;
    for (size_t i = 0; i < kMMCntBlockSizes; ++i) {
        Int8 cnt = m_UserBlAlloced[i] - m_UserBlFreed[i];
        m_EndState.m_UserBlocks[i] = cnt;
        total_data += cnt * kMMBlockSizes[i];
        m_EndState.m_SysBlocks[i] = m_SysBlAlloced[i] - m_SysBlFreed[i];
    }
    m_EndState.m_BigBlocksCnt = m_BigAllocedCnt - m_BigFreedCnt;
    Int8 size = m_BigAllocedSize - m_BigFreedSize;
#else
    Uint8 total_data = 0;
    for (size_t i = 0; i < kMMCntBlockSizes; ++i) {
        Int8 cnt = s_UserBlAlloced[i] - s_UserBlFreed[i];
        m_EndState.m_UserBlocks[i] = cnt;
        total_data += cnt * kMMBlockSizes[i];
        m_EndState.m_SysBlocks[i] = s_SysBlAlloced[i] - s_SysBlFreed[i];
    }
    m_EndState.m_BigBlocksCnt = s_BigAllocedCnt - s_BigFreedCnt;
    Int8 size = s_BigAllocedSize - s_BigFreedSize;
#endif

    m_EndState.m_BigBlocksSize = size;
    total_data += size;
    m_EndState.m_TotalData = total_data;
    m_EndState.m_TotalSys = s_TotalSysMem;
}

void
SMMStat::ClearStats(void)
{
    memset(m_UserBlAlloced, 0, sizeof(m_UserBlAlloced));
    memset(m_UserBlFreed,   0, sizeof(m_UserBlFreed));
    memset(m_SysBlAlloced,  0, sizeof(m_SysBlAlloced));
    memset(m_SysBlFreed,    0, sizeof(m_SysBlFreed));
    m_BigAllocedCnt  = m_BigFreedCnt  = 0;
    m_BigAllocedSize = m_BigFreedSize = 0;
    m_TotalSysMem.Initialize();
    m_TotalDataMem.Initialize();
}

void
SMMStat::AddStats(SMMStat* src_stat)
{
#if __NC_MEMMAN_PERTHREAD_STAT
    for (Uint1 i = 0; i < kMMCntBlockSizes; ++i) {
        AtomicAdd(m_UserBlAlloced[i], src_stat->m_UserBlAlloced[i]);
        AtomicAdd(m_UserBlFreed[i],   src_stat->m_UserBlFreed[i]);
        AtomicAdd(m_SysBlAlloced[i],  src_stat->m_SysBlAlloced[i]);
        AtomicAdd(m_SysBlFreed[i],    src_stat->m_SysBlFreed[i]);
    }
    AtomicAdd(m_BigAllocedCnt,  src_stat->m_BigAllocedCnt);
    AtomicAdd(m_BigAllocedSize, src_stat->m_BigAllocedSize);
    AtomicAdd(m_BigFreedCnt,    src_stat->m_BigFreedCnt);
    AtomicAdd(m_BigFreedSize,   src_stat->m_BigFreedSize);
#else
    for (Uint1 i = 0; i < kMMCntBlockSizes; ++i) {
        m_UserBlAlloced[i] = s_UserBlAlloced[i];
        m_UserBlFreed[i]   = s_UserBlFreed[i];
        m_SysBlAlloced[i]  = s_SysBlAlloced[i];
        m_SysBlFreed[i]    = s_SysBlFreed[i];
    }
    m_BigAllocedCnt  = s_BigAllocedCnt;
    m_BigAllocedSize = s_BigAllocedSize;
    m_BigFreedCnt    = s_BigFreedCnt;
    m_BigFreedSize   = s_BigFreedSize;
#endif
    m_TotalSysMem.AddValues(src_stat->m_TotalSysMem);
    m_TotalDataMem.AddValues(src_stat->m_TotalDataMem);
}

void
SMMStat::SaveEndStateStat(SMMStat* src_stat)
{
    m_TotalSysMem.AddValue(src_stat->m_EndState.m_TotalSys);
    m_TotalDataMem.AddValue(src_stat->m_EndState.m_TotalData);
}

void
SMMStat::PrintToLogs(CRequestContext* ctx, CSrvPrintProxy& proxy)
{
    CSrvDiagMsg diag;
    diag.PrintExtra(ctx);
    diag.PrintParam("start_sys_mem",   m_StartState.m_TotalSys)
        .PrintParam("end_sys_mem",     m_EndState.m_TotalSys)
        .PrintParam("avg_sys_mem",     m_TotalSysMem.GetAverage())
        .PrintParam("max_sys_mem",     m_TotalSysMem.GetMaximum())
        .PrintParam("start_data_mem",  m_StartState.m_TotalData)
        .PrintParam("end_data_mem",    m_EndState.m_TotalData)
        .PrintParam("avg_data_mem",    m_TotalDataMem.GetAverage())
        .PrintParam("max_data_mem",    m_TotalDataMem.GetMaximum())
        .PrintParam("mmap_page_cnt",   s_TotalPageCount)
        .PrintParam("big_blocks_cnt",  m_EndState.m_BigBlocksCnt)
        .PrintParam("big_blocks_size", m_EndState.m_BigBlocksSize);
    diag.Flush();

    x_PrintUnstructured(proxy);
}

static void
s_PrintSizeDiff(CSrvPrintProxy& proxy, Uint2 size_idx,
                Int8 was_blocks, Int8 is_blocks)
{
    if (was_blocks == is_blocks) {
        proxy << "0";
    }
    else if (is_blocks > was_blocks) {
        Int8 diff = is_blocks - was_blocks;
        proxy << "+" << g_ToSizeStr(diff * kMMBlockSizes[size_idx]);
    }
    else {
        Int8 diff = was_blocks - is_blocks;
        proxy << "-" << g_ToSizeStr(diff * kMMBlockSizes[size_idx]);
    }
}

static void
s_PrintBlocksState(CSrvPrintProxy& proxy, Int8* start_blocks, Int8* end_blocks)
{
    Uint2 low_size = 0;
    Uint2 size = 0;
    for (Uint1 i = 0; i < kMMCntBlockSizes; ++i, low_size = size + 1) {
        size = kMMBlockSizes[i];
        if (start_blocks[i] == 0  &&  end_blocks[i] == 0)
            continue;

        proxy << low_size << "-" << size << ": ";
        if (start_blocks[i] == end_blocks[i]) {
            proxy << "unchanged ("
                    << g_ToSmartStr(start_blocks[i]) << " of "
                    << g_ToSizeStr(start_blocks[i] * kMMBlockSizes[i])
                    << ")" << endl;
        }
        else {
            s_PrintSizeDiff(proxy, i, start_blocks[i], end_blocks[i]);
            proxy << " (" << g_ToSmartStr(start_blocks[i]) << " of "
                          << g_ToSizeStr(start_blocks[i] * kMMBlockSizes[i])
                  << " to " << g_ToSmartStr(end_blocks[i]) << " of "
                            << g_ToSizeStr(end_blocks[i] * kMMBlockSizes[i])
                  << ")" << endl;
        }
    }
}

void
SMMStat::x_PrintUnstructured(CSrvPrintProxy& proxy)
{
    proxy << "Data memory state by size:" << endl;
    s_PrintBlocksState(proxy, m_StartState.m_UserBlocks, m_EndState.m_UserBlocks);
    if (m_StartState.m_BigBlocksCnt != 0  ||  m_EndState.m_BigBlocksCnt != 0) {
        proxy << "Big size: ";
        if (m_StartState.m_BigBlocksCnt == m_EndState.m_BigBlocksCnt
            &&  m_StartState.m_BigBlocksSize == m_EndState.m_BigBlocksSize)
        {
            proxy << "unchanged ("
                  << g_ToSmartStr(m_StartState.m_BigBlocksCnt) << " of "
                  << g_ToSizeStr(m_StartState.m_BigBlocksSize) << ")" << endl;
        }
        else {
            if (m_StartState.m_BigBlocksSize == m_EndState.m_BigBlocksSize) {
                proxy << "0";
            }
            else if (m_StartState.m_BigBlocksSize < m_EndState.m_BigBlocksSize) {
                proxy << "+" << g_ToSizeStr(m_EndState.m_BigBlocksSize
                                            - m_StartState.m_BigBlocksSize);
            }
            else {
                proxy << "-" << g_ToSizeStr(m_StartState.m_BigBlocksSize
                                            - m_EndState.m_BigBlocksSize);
            }
            proxy << " (" << g_ToSmartStr(m_StartState.m_BigBlocksCnt) << " of "
                          << g_ToSizeStr(m_StartState.m_BigBlocksSize)
                  << " to " << g_ToSmartStr(m_EndState.m_BigBlocksCnt) << " of "
                            << g_ToSizeStr(m_EndState.m_BigBlocksSize)
                  << ")" << endl;
        }
    }

    proxy << endl << "Sys memory state by size:" << endl;
    s_PrintBlocksState(proxy, m_StartState.m_SysBlocks, m_EndState.m_SysBlocks);

    proxy << endl << "Memory flow by size:" << endl;
    Uint2 low_size = 0;
    for (Uint1 i = 0; i < kMMCntBlockSizes; ++i) {
        Uint2 size = kMMBlockSizes[i];
        if (m_UserBlAlloced[i] != 0  ||  m_UserBlFreed[i] != 0
            ||  m_SysBlAlloced[i] != 0  ||  m_SysBlFreed[i] != 0)
        {
            proxy << low_size << "-" << size << ": ";
            if (m_UserBlAlloced[i] != 0  ||  m_UserBlFreed[i] != 0) {
                s_PrintSizeDiff(proxy, i, m_UserBlFreed[i], m_UserBlAlloced[i]);
                proxy << " (+" << g_ToSmartStr(m_UserBlAlloced[i])
                      << "-" << g_ToSmartStr(m_UserBlFreed[i]) << ") data";
                if (m_SysBlAlloced[i] != 0  ||  m_SysBlFreed[i] != 0)
                    proxy << ", ";
            }
            if (m_SysBlAlloced[i] != 0  ||  m_SysBlFreed[i] != 0) {
                s_PrintSizeDiff(proxy, i, m_SysBlFreed[i], m_SysBlAlloced[i]);
                proxy << " (+" << g_ToSmartStr(m_SysBlAlloced[i])
                      << "-" << g_ToSmartStr(m_SysBlFreed[i]) << ") sys";
            }
            proxy << endl;
        }
        low_size = size + 1;
    }
    if (m_BigAllocedCnt != 0  ||  m_BigFreedCnt != 0) {
        proxy << "Big size: ";
        if (m_BigAllocedSize == m_BigFreedSize)
            proxy << "0";
        else if (m_BigAllocedSize >= m_BigFreedSize)
            proxy << "+" << g_ToSizeStr(m_BigAllocedSize - m_BigFreedSize);
        else
            proxy << "-" << g_ToSizeStr(m_BigFreedSize - m_BigAllocedSize);
        proxy << " (+" << g_ToSmartStr(m_BigAllocedCnt) << " of "
              << g_ToSizeStr(m_BigAllocedSize) << ", -"
              << g_ToSmartStr(m_BigFreedCnt) << " of "
              << g_ToSizeStr(m_BigFreedSize) << ")" << endl;
    }
    proxy << endl;
}

void
SMMStat::PrintToSocket(CSrvPrintProxy& proxy)
{
    proxy << endl
          << "Data memory state - "
                    << g_ToSizeStr(m_StartState.m_TotalData) << " to "
                    << g_ToSizeStr(m_EndState.m_TotalData) << " (avg "
                    << g_ToSizeStr(m_TotalDataMem.GetAverage()) << ", max "
                    << g_ToSizeStr(m_TotalDataMem.GetMaximum()) << ")" << endl;
    proxy << "System memory state - "
                    << g_ToSizeStr(m_StartState.m_TotalSys) << " to "
                    << g_ToSizeStr(m_EndState.m_TotalSys) << " (avg "
                    << g_ToSizeStr(m_TotalSysMem.GetAverage()) << ", max "
                    << g_ToSizeStr(m_TotalSysMem.GetMaximum()) << ")" << endl;
    proxy << endl;

    x_PrintUnstructured(proxy);
}

void SMMStat::PrintState(CSrvSocketTask& task)
{
    string is("\": "), iss("\": \""), eol(",\n\""), qt("\"");
    task.WriteText(eol).WriteText("total_sys_memory").WriteText(iss)
                                     .WriteText( NStr::UInt8ToString_DataSize( m_EndState.m_TotalSys)).WriteText(qt);
    task.WriteText(eol).WriteText("total_data_memory").WriteText(iss)
                                     .WriteText( NStr::UInt8ToString_DataSize( m_EndState.m_TotalData)).WriteText(qt);
    task.WriteText(eol).WriteText("big_blocks_cnt").WriteText(is).WriteNumber(m_EndState.m_BigBlocksCnt);
    task.WriteText(eol).WriteText("big_blocks_size" ).WriteText(iss)
                                     .WriteText( NStr::UInt8ToString_DataSize( m_EndState.m_BigBlocksSize)).WriteText(qt);
    task.WriteText(eol).WriteText("mmap_page_cnt").WriteText(is).WriteNumber(s_TotalPageCount);
#if __NC_MEMMAN_ALLPTR_COUNT
    task.WriteText(eol).WriteText("AllPtrCount").WriteText(is);
    task.WriteText("[");
    for (Int8 i=0; i < kMMCntBlockSizes; ++i) {
        if (i != 0) {
            task.WriteText(",");
        }
        task.WriteNumber( s_AllPtrCount[i]);
    }
    task.WriteText("]");
    task.WriteText(eol).WriteText("AllSysPtrCount").WriteText(is);
    task.WriteText("[");
    for (Int8 i=0; i < kMMCntBlockSizes; ++i) {
        if (i != 0) {
            task.WriteText(",");
        }
        task.WriteNumber( s_AllSysPtrCount[i]);
    }
    task.WriteText("]");
#endif
}

END_NCBI_SCOPE;


void*
operator new (size_t size)
#ifdef NCBI_COMPILER_GCC
throw (std::bad_alloc)
#endif
{
    return ncbi::s_AllocMemory(size);
}

void
operator delete (void* ptr) throw ()
{
    ncbi::s_DeallocMemory(ptr);
}

void*
operator new[] (size_t size)
#ifdef NCBI_COMPILER_GCC
throw (std::bad_alloc)
#endif
{
    return ncbi::s_AllocMemory(size);
}

void
operator delete[] (void* ptr) throw ()
{
    ncbi::s_DeallocMemory(ptr);
}

#ifdef __GLIBC__
// glibc has special method of overriding C library allocation functions.
// Also, see comments in FinalizeMemoryMan
#if __NC_MEMMAN_USE_MALLOC_HOOK
#include <malloc.h>

static void*
s_MallocHook(size_t size, const void* caller)
{
    return ncbi::s_AllocMemory(size);
}

static void*
s_ReallocHook(void* mem_ptr, size_t size, const void* caller)
{
    return ncbi::s_ReallocMemory(mem_ptr, size);
}

static void
s_FreeHook(void* mem_ptr, const void* caller)
{
    ncbi::s_DeallocMemory(mem_ptr);
}

static void
s_InitMallocHook(void)
{
    __malloc_hook  = &s_MallocHook;
    __realloc_hook = &s_ReallocHook;
    __free_hook    = &s_FreeHook;
}

void (*__malloc_initialize_hook) (void) = &s_InitMallocHook;

#else

extern "C" {

void* malloc(size_t size)
{
    return ncbi::s_AllocMemory(size);
}

void* realloc(void* mem_ptr, size_t size)
{
    return ncbi::s_ReallocMemory(mem_ptr, size);
}

void free(void* mem_ptr)
{
    ncbi::s_DeallocMemory(mem_ptr);
}
}

#endif

#endif
