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


#include <corelib/ncbithr.hpp>

#include "nc_utils.hpp"


BEGIN_NCBI_SCOPE

/// Utility class for tuning memory usage in NetCache including database cache
/// and all other memory allocations.
class CNCMemManager {
public:
    /// Do initialization steps when application started working.
    /// Method attaches memory manager to SQLite and starts background thread
    /// necessary for gathering statistics.
    /// Method must be called before CSQLITE_Global::Initialize().
    static bool InitializeApp(void);
    /// Do finalization steps when application has finished.
    /// This must be called to stop background thread started in
    /// InitializeApp().
    static void FinalizeApp(void);
    /// Set maximum memory amount that process can use and amount of memory
    /// that should be considered dangerous and should put the whole
    /// application on alert.
    /// Limit (including alert_level) is not hard, i.e. if NetCache will not
    /// be able to work inside this limit then additional memory will be
    /// automatically allocated. Also manager ensures that at least half of
    /// this limit is used by database cache.
    static void SetLimits(size_t limit, size_t alert_level);
    /// Check if memory consumption is above alert level.
    static bool IsOnAlert(void);
    /// Get maximum memory that can be used by NetCache
    static size_t GetMemoryLimit(void);
    /// Get amount of memory used by NetCache at the moment
    static size_t GetMemoryUsed(void);

    /// Print memory usage statistics
    static void PrintStats(CPrintTextProxy& proxy);

    /// Lock page from database cache which given pointer falls into.
    /// The database page will be locked from reusing in another cache until
    /// UnlockDBPage() is called.
    static void LockDBPage(const void* data_ptr);
    /// Unlock page from database cache which given pointer falls into.
    /// The database page should be unlocked as many times as many calls to
    /// LockDBPage() was made. After last unlocking page will be allowed to be
    /// reused for other cache.
    static void UnlockDBPage(const void* data_ptr);
    /// Mark DB page as dirty
    static void SetDBPageDirty(const void* data_ptr);
    /// Mark DB page as clean and written to disk
    static void SetDBPageClean(const void* data_ptr);
    /// Check if DB page is dirty and needs writing to disk
    static bool IsDBPageDirty(const void* data_ptr);

private:
    CNCMemManager(void);
};


//////////////////////////////////////////////////////////////////////////
// Stuff only for internal use in nc_memory.cpp
//////////////////////////////////////////////////////////////////////////

class CNCMMFreeChunk;
class CNCMMChunksPool;
class CNCMMBlocksSetBase;
class CNCMMBlocksSet;
class CNCMMChainsPool;
class CNCMMReserve;
class CNCMMSizePool;
class CNCMMDBPagesHash;
class CNCMMDBCache;
class CNCMMStats;


class CNCMMMutex : public SSystemFastMutex
{
public:
    using SSystemFastMutex::InitializeDynamic;
};

typedef CGuard<CNCMMMutex>  CNCMMMutexGuard;


/// Size of memory necessary for data in each page in database cache.
/// This is a size of memory allocated for each page in SQLite.
static const size_t kNCMMDBPageDataSize      = 32976;
/// Alignment boundary to which manager will align all memory allocated from
/// system. 2 Mb is not the most optimal value if amount of lost memory is
/// taken into consideration (4 Mb will be the most optimal) but it's good
/// enough. And it's of course far from optimal if amount of free memory not
/// released to system is taken into consideration but it works great with
/// maximum amount of memory allocated by NetCache and SQLite in one call to
/// malloc (it's about 2000008 bytes).
static const size_t kNCMMAlignSize           = 2 * 1024 * 1024;
/// Mask to get aligned memory address from arbitrary memory address.
static const size_t kNCMMAlignMask           = ~(kNCMMAlignSize - 1);
/// Maximum number of threads when manager is the most effective in avoiding
/// contention between threads for internal data structures.
static const unsigned int kNCMMMaxThreadsCnt = 25;


/// Object with one page from database. It contains page data managed by
/// SQLite along with meta-information to support LRU list of database cache
/// and hash list of pages inside one CNCMMDBCache object.
///
/// @sa CNCMMDBCache, CNCMMDBPagesHash
class CNCMMDBPage
{
public:
    /// Get pointer to the object from pointer to page data
    static CNCMMDBPage* FromDataPtr(void* data);
    /// Get page from LRU list with intention to destroy it and use its memory
    /// for other database page.
    static CNCMMDBPage* PeekLRUForDestroy(void);

    /// Special new and delete operators to obtain memory from centralized
    /// pool of memory chunks.
    void* operator new(size_t size);
    void operator delete(void* mem_ptr);

    CNCMMDBPage(void);
    ~CNCMMDBPage(void);

    /// Get pointer to page data
    void* GetData(void);
    /// Get cache instance this page belongs to
    CNCMMDBCache* GetCache(void);
    /// Set cache instance owning this page. Either page should be detached
    /// from any cache or this call should be to detach it (set cache to
    /// NULL).
    void SetCache(CNCMMDBCache* cache);
    /// Check if page is now in LRU list i.e. it's not in use by SQLite at the
    /// moment.
    bool IsInLRU(void);
    /// Put page to LRU list's tail i.e. it was just used and released.
    /// Method should be called under corresponding cache's mutex.
    void AddToLRU(void);
    /// Remove page from LRU list i.e. it becomes locked and will not be
    /// reused by some other cache instance until it's returned to LRU list.
    /// Method should be called under corresponding cache's mutex.
    void RemoveFromLRU(void);
    /// Notify that page was removed from cache's hash-table and so can be
    /// deleted at any time it is allowed to. It wouldn't be allowed to delete
    /// page right away only in two cases: when page is locked from outside
    /// and when page is already peeked for destruction in another thread.
    /// In the latter case page will be destroyed and re-used in the thread
    /// that picked it, in the former case page will be deleted when it is
    /// unlocked.
    /// Method should be called under corresponding cache's mutex.
    ///
    /// @sa PeekLRUForDestroy
    void DeletedFromHash(void);
    /// Destroy this page after peeking it by PeekLRUForDestroy().
    /// Page will be destroyed only if it's in the LRU list, i.e. it's not
    /// used by SQLite at the moment. Method returns TRUE if page was indeed
    /// destroyed, FALSE if it was discovered that page is used by SQLite and
    /// so it was left untouched.
    /// Method should be called under corresponding cache's mutex.
    ///
    /// @sa PeekLRUForDestroy
    bool DoPeekedDestruction(void);

    /// Lock page and disallow its deletion or placement into LRU list where
    /// it could be picked from and re-used in another cache.
    /// Locking is done solely from CNCFileSystem to avoid reusing its memory
    /// while data is not yet written to disk.
    void Lock(void);
    /// Unlock page and allow any reusing of its memory.
    /// Unlocking should be done as many times as many calls to Lock() were
    /// made. After last call to Unlock() page will be either placed to LRU
    /// list or deleted if no cache owns it already.
    void Unlock(void);
    /// Set/unset dirty flag for the page
    void SetDirtyFlag(bool value);
    /// Check if dirty flag is set
    bool IsDirty(void) const;

private:
    friend class CNCMMDBPagesHash;

    /// Prohibit accidental calls to unimplemented functions
    CNCMMDBPage(const CNCMMDBPage&);
    CNCMMDBPage& operator= (const CNCMMDBPage&);
    void* operator new(size_t, void*);

    /// Check if page is in LRU and wasn't peeked with PeekLRUForDestroy().
    /// If it was peeked IsInLRU() still returns TRUE but this method returns
    /// FALSE.
    /// Method should be called under sm_LRULock.
    bool x_IsReallyInLRU(void);
    /// Implementation of removing page from LRU list when it's actually
    /// there (should be called only when x_IsReallyInLRU() returns TRUE).
    /// Method should be called under sm_LRULock.
    void x_RemoveFromLRUImpl(void);
    /// Implementation of adding page to LRU list.
    /// Method should be called under sm_LRULock.
    void x_AddToLRUImpl(void);


    /// Flags representing state of database page
    enum EStateFlags {
        fInLRU       = 1, ///< Page is in LRU list, i.e. it's not used now by
                          ///< SQLite.
        fPeeked      = 2, ///< Page is extracted from LRU for destruction,
                          ///< i.e. if page is not needed anymore then only
                          ///< thread that extracted it can delete it.
        fDirty       = 4, ///< Page is dirty and needs to be written on disk
        fCounterStep = 8  ///< The incrementing/decrementing step in counting
                          ///< number of locks made on the page. Value should
                          ///< be greater than all other flags.
    };
    /// Bit mask of EStateFlags
    typedef int  TStateFlags;


    /// Instance of database cache this page belongs to.
    CNCMMDBCache* m_Cache;
    /// Key of the page assigned by SQLite.
    unsigned int  m_Key;
    /// Next page in the hash-table inside cache instance (not NULL only if
    /// there's other page with the same hash-code in this cache instance)
    CNCMMDBPage*  m_NextInHash;
    /// Flags showing page's current state.
    TStateFlags   m_StateFlags;
    /// Previous page in LRU list
    CNCMMDBPage*  m_PrevInLRU;
    /// Next page in LRU-list
    CNCMMDBPage*  m_NextInLRU;
    /// Actual data of database page
    char          m_Data[kNCMMDBPageDataSize];

    /// Mutex for working with LRU list
    static CFastMutex   sm_LRULock;
    /// Head of LRU list of database pages (i.e. the least recently used page
    /// and the first candidate for reuse).
    static CNCMMDBPage* sm_LRUHead;
    /// Tail of LRU list of database pages (i.e. the most recently used page
    /// and the last candidate for reuse).
    static CNCMMDBPage* sm_LRUTail;
};

/// Size of memory chunks - minimum piece of memory operated by manager.
/// In ideal world manager would get memory from system only by this amount.
/// But it's just a bit more than power of 2 and thus if manager always did
/// allocate aligned memory of this amount then memory would be very sparse
/// and could be very bad for the system. So manager allocates memory by
/// slabs.
///
/// @sa CNCMMSlab
static const size_t kNCMMChunkSize = sizeof(CNCMMDBPage);


/// Base class for CNCMMFreeChunk.
/// Class should contain all members that need to be in CNCMMFreeChunk because
/// the only member of CNCMMFreeChunk itself depends on the size of this base
/// and total size of CNCMMFreeChunk must be fixed.
///
/// @sa CNCMMFreeChunk, kNCMMFreeDataSize, kNCMMChunkSize
class CNCMMFreeChunkBase
{
public:
    /// Get starting chunk of the chain this chunk belongs to.
    /// Method returns useful information only for the starting and ending
    /// chunks of the chain.
    CNCMMFreeChunk* GetChainStart(void);
    /// Get number of chunks in the chain this chunk belongs to.
    /// Method returns useful information only for the starting and ending
    /// chunks of the chain.
    unsigned int    GetChainSize(void);
    /// Get emptiness grade of the slab when this chain was marked as free
    unsigned int    GetSlabGrade(void);

protected:
    /// This class cannot work as standalone one.
    CNCMMFreeChunkBase(void);

    /// Starting chunk in the chunks chain which this chunk is part of.
    /// This is set only for the first and the last chunks in the chain.
    CNCMMFreeChunk* m_ChainStart;
    /// Number of chunks in the chain which this chunk is part of.
    /// This is set only for the first and the last chunks in the chain.
    unsigned int    m_ChainSize;
    /// Emptiness grade of the slab which this chunk is part of at the time
    /// when its chain was marked as free.
    /// This is set only for the first and the last chunks in the chain.
    unsigned int    m_SlabGrade;
    /// Previous chain of the same size in the chain pool.
    /// This is set only for the first chunk in the chain.
    CNCMMFreeChunk* m_PrevChain;
    /// Next chain of the same size in the chain pool.
    /// This is set only for the first chunk in the chain.
    CNCMMFreeChunk* m_NextChain;
};

/// Size of the unused data inside free memory chunk.
/// Constant is necessary to make size of CNCMMFreeChunk exactly equal to
/// kNCMMChunkSize.
static const size_t kNCMMFreeDataSize = kNCMMChunkSize
                                                 - sizeof(CNCMMFreeChunkBase);

/// Representation of the free memory chunk, i.e. piece of memory that is not
/// used by anything now and can be used for database cache or for some data
/// later.
class CNCMMFreeChunk : public CNCMMFreeChunkBase
{
public:
    /// The only new operator allowed for the class - it's always created
    /// in-place or as part of slab.
    void* operator new(size_t, void* mem_ptr);
    void operator delete(void*, void*);

    CNCMMFreeChunk(void);
    ~CNCMMFreeChunk(void);

    /// Mark the chunks chain with its data.
    /// Method called only for the starting chunk of the chain.
    ///
    /// @param chain_size
    ///   Number of chunks in the chain
    /// @param slab_grade
    ///   Emptiness grade of the slab where this chain was marked as free
    void MarkChain(unsigned int chain_size, unsigned int slab_grade);
    /// Destroy chain of chunks which starts from this chunk and continues for
    /// chain_size chunks.
    void DestroyChain(unsigned int chain_size);
    /// Create chain of chunks which will start at memory location mem_ptr
    /// and continue for chain_size chunks. Chain will be just constructed
    /// without marking it.
    static CNCMMFreeChunk* ConstructChain(void* mem_ptr,
                                          unsigned int chain_size);

private:
    friend class CNCMMReserve;

    /// Prohibit accidental call to non-implemented methods.
    CNCMMFreeChunk(const CNCMMFreeChunk&);
    CNCMMFreeChunk& operator= (const CNCMMFreeChunk&);
    void* operator new(size_t);
    void operator delete(void*);

    /// Filling to extend this class to be of size kNCMMChunkSize
    ///
    /// @sa kNCMMChunkSize
    char m_Data[kNCMMFreeDataSize];
};


/// Helper class to make CNCMMChunksPool available different for each thread.
/// Actually only kNCMMMaxThreadsCnt different instances of pool is supported.
/// Object should be a singleton.
///
/// @sa CNCMMChunksPool, kNCMMMaxThreadsCnt
class CNCMMChunksPool_Getter : public CNCTlsObject<CNCMMChunksPool_Getter,
                                                   CNCMMChunksPool>
{
    typedef CNCTlsObject<CNCMMChunksPool_Getter, CNCMMChunksPool>  TBase;

public:
    /// Empty constructor to avoi any problems in some compilers.
    CNCMMChunksPool_Getter(void);
    /// Initialize static structures of the getter and parent object.
    /// Method should be called only once before getter is used.
    void Initialize(void);
    /// Create instance of CNCMMChunksPool for the current thread.
    CNCMMChunksPool* CreateTlsObject(void);
    /// Delete CNCMMChunksPool instance from the current thread.
    static void DeleteTlsObject(void* obj_ptr);

private:
    /// Prohibit accidental call to non-implemented methods.
    CNCMMChunksPool_Getter(const CNCMMChunksPool_Getter&);
    CNCMMChunksPool_Getter& operator= (const CNCMMChunksPool_Getter&);
    void* operator new (size_t);
    void* operator new (size_t, void*);

    /// Global lock managing creation and deletion of pool instances
    static CNCMMMutex       sm_CreateLock;
    /// Array of all pool instances used in manager
    static CNCMMChunksPool  sm_Pools   [kNCMMMaxThreadsCnt];
    /// Array of pool instances that are to be yet used
    static CNCMMChunksPool* sm_PoolPtrs[kNCMMMaxThreadsCnt];
    /// Number of already used pool instances
    static unsigned int     sm_CntUsed;
};

/// Number of chunks stored in chunks pool to speed up requests for free
/// chunks. The more this number the less requests is made to the central
/// chunks storage and thus the less contention between threads, but the more
/// memory is free and waiting for future use without releasing to system.
static const unsigned int kNCMMCntChunksInPool = 10;

/// Per-thread pool of free chunks for speeding up allocation and deallocation
/// of individual chunks. All chunks are stored in a cyclic buffer so that
/// released chunks will be re-used with next allocation and will go to the
/// central reserve only when it's already too many released to fit in the
/// pool. "Per-thread"ness is implemented transparent for outside - one should
/// access only public static methods without caring about details.
class CNCMMChunksPool
{
public:
    /// Initialize chunks pooling system. Method should be called at the very
    /// beginning before pools are used.
    static void Initialize(void);
    /// Get free memory chunk from pool.
    static void* GetChunk(void);
    /// Put free memory chunk to the pool for future re-use.
    static void PutChunk(void* mem_ptr);

    /// Empty constructor to avoid any problems in some compilers.
    /// All initialization is made in InitInstance().
    CNCMMChunksPool(void);
    /// Initialize the pool instance.
    /// Method should be called only once at the very beginning. At all other
    /// times pool supports consistent state even if it was deleted from one
    /// thread and then assigned to another.
    void InitInstance(void);
    /// Add reference to the pool.
    /// References are necessary when there's more than kNCMMMaxThreadsCnt
    /// threads is running.
    void AddRef(void);
    /// Release reference from the pool.
    /// After releasing last reference pool expects to be put in the queue for
    /// re-use in some new thread and thus cleans itself moving all chunks to
    /// the central reserve.
    unsigned int ReleaseRef(void);

private:
    /// Prohibit accidental call to non-implemented methods.
    CNCMMChunksPool(const CNCMMChunksPool&);
    CNCMMChunksPool& operator= (const CNCMMChunksPool&);
    void* operator new(size_t);
    void* operator new(size_t, void*);

    /// Get chunk from this pool instance.
    void* x_GetChunk(void);
    /// Put chunk to this pool instance for re-use.
    void  x_PutChunk(void* mem_ptr);
    /// Transfer all chunks stored to the central reserve.
    void  x_ReleaseAllChunks(void);
    /// Shift pointer to chunk in the pool to next one rotating pointer to
    /// the beginning of the pool array as necessary.
    CNCMMFreeChunk** x_AdvanceChunkPtr(CNCMMFreeChunk** chunk_ptr);
    /// Fill empty pool with new chunks from central reserve
    void x_RefillPool(void);
    /// Free some space in pool to allow new chunks to be put in there
    void x_CleanPoolSpace(void);

    /// Mutex protecting object using
    CNCMMMutex       m_ObjLock;
    /// Pointer to the next chunk that will be returned by x_GetChunk()
    CNCMMFreeChunk** m_GetPtr;
    /// Pointer to the place where next x_PutChunk() will put free chunk
    CNCMMFreeChunk** m_PutPtr;
    /// Free chunks storage
    CNCMMFreeChunk*  m_Chunks[kNCMMCntChunksInPool];
    /// Reference counter for this object
    CAtomicCounter   m_RefCnt;

    /// Getter providing pools on thread-by-thread basis
    static CNCMMChunksPool_Getter sm_Getter;
};


/// Helper structure used to transfer information about free chunks chains
/// between different classes.
struct SNCMMChainInfo
{
    /// Starting chunk of the chain
    CNCMMFreeChunk* start;
    /// Number of chunks in the chain
    unsigned int    size;
    /// Emptiness grade of the slab where chain is located
    unsigned int    slab_grade;

    /// Initialize data with zeros
    void Initialize(void);
    /// Copy information about chain from the chain itself
    void AssignFromChain(CNCMMFreeChunk* chain);
};


/// Size of memory slab - the minimum piece of memory allocated from system.
static const size_t kNCMMSlabSize = kNCMMAlignSize;
/// Number of memory chunks that will fit inside one slab.
static const unsigned int kNCMMCntChunksInSlab =
                    static_cast<unsigned int>(kNCMMSlabSize / kNCMMChunkSize);


/// Base class for memory slab containing all meta-information about it.
/// It's necessary to make CNCMMSlab and CNCMMBigBlockSlab interchangeable and
/// methods of CNCMMSlab to work with CNCMMBigBlockSlab too.
///
/// @sa CNCMMSlab, CNCMMBigBlockSlab
class CNCMMSlabBase
{
protected:
    /// Class cannot work as standalone one
    CNCMMSlabBase(void);

    /// Mutex to control access to the object
    CNCMMMutex                       m_ObjLock;
    /// Bit mask showing which chunks in the slab are free
    /// (1 - free, 0 - occupied).
    CNCBitMask<kNCMMCntChunksInSlab> m_FreeMask;
    /// Emptiness grade of the slab: the more chunks is occupied the less this
    /// number with 0 meaning that all chunks are occupied.
    unsigned int                     m_EmptyGrade;
    /// Number of free chunks inside the slab.
    unsigned int                     m_CntFree;
};


/// Representation of the memory slab - big piece of memory containing several
/// memory chunks. The size of this class is not exactly kNCMMSlabSize, so
/// some data at the end will be lost after allocation from system but this
/// amount is not very significant.
class CNCMMSlab : public CNCMMSlabBase
{
public:
    /// Allocate memory for the slab from system
    void* operator new(size_t);
    /// Release memory used by the slab to system
    void operator delete(void* mem_ptr);

    CNCMMSlab(void);
    ~CNCMMSlab(void);

    /// Get pointer to blocks set where given pointer belongs to
    CNCMMBlocksSetBase* GetBlocksSet(void* mem_ptr);
    /// Get emptiness grade of this slab
    unsigned int GetEmptyGrade(void);

    /// Mark chain of memory chunks as occupied, return emptiness grade of the
    /// slab resulted from this occupation.
    unsigned int MarkChainOccupied(const SNCMMChainInfo& chain);
    /// Mark chain of memory chunks as free, put resulted emptiness grade into
    /// chain structure, also fill information about chains located right near
    /// the marked chain if they exist and they are free (chain_left - in
    /// lower address space, chain_right - in higher address space).
    void MarkChainFree(SNCMMChainInfo* chain,
                       SNCMMChainInfo* chain_left,
                       SNCMMChainInfo* chain_right);

private:
    /// Prohibit accidental use of non-implemented methods.
    CNCMMSlab(const CNCMMSlab&);
    CNCMMSlab& operator= (const CNCMMSlab&);
    void* operator new(size_t, void*);

    /// Get index of the chunk where given memory pointer belongs to.
    unsigned int x_GetChunkIndex(void* mem_ptr);
    /// Calculate new emptiness grade for the slab
    unsigned int x_CalcEmptyGrade(void);


    /// Data of the slab - set of chunks it consists of.
    CNCMMFreeChunk m_Chunks[kNCMMCntChunksInSlab];
};


/// Base class for set of equally-sized blocks. It's necessary to make
/// CNCMMBlocksSet and CNCMMBigBlockSet interchangeable.
///
/// @sa CNCMMBlocksSet, CNCMMBigBlockSet
class CNCMMBlocksSetBase
{
public:
    /// Get size of blocks inside of this set (even if set contains only one
    /// block).
    size_t GetBlockSize(void);

protected:
    /// Class cannot be used as standalone one
    CNCMMBlocksSetBase(void);

    /// Pointer to last available free block
    void**          m_LastFree;
    /// Emptiness grade of the set: the more blocks is occupied the less this
    /// number with 0 meaning that all blocks are occupied.
    unsigned int    m_EmptyGrade;
    /// Size of blocks in this set.
    /// Actually stored number is index of element in kNCMMSmallSize for sets
    /// containing several blocks and actual size for sets containing only one
    /// block.
    ///
    /// @sa kNCMMSmallSize
    size_t          m_BlocksSize;
    /// Pool of equally-sized blocks which this set belongs to.
    CNCMMSizePool*  m_Pool;
    /// Next set in the pool's list
    CNCMMBlocksSet* m_NextInPool;
    /// Previous set in the pool's list
    CNCMMBlocksSet* m_PrevInPool;
};

/// Size of the blocks set meta-data
static const size_t kNCMMSetBaseSize    = sizeof(CNCMMBlocksSetBase);
/// Size of the real data part inside blocks set. Full size of the set
/// (meta-data + real data) should always be equal to size of memory chunk.
///
/// @sa kNCMMChunkSize
static const size_t kNCMMSetDataSize    = kNCMMChunkSize - kNCMMSetBaseSize;
/// Size of extra-information stored in the set for each block in addition to
/// overall set meta-data. For now it's just a pointer in the list of free
/// blocks.
static const size_t kNCMMBlockExtraSize = sizeof(char*);

/// Set of equally-sized blocks with size less than size of the chunk (when
/// subtracted size of meta-data of the set.
class CNCMMBlocksSet : public CNCMMBlocksSetBase
{
public:
    /// Allocate memory for new blocks set getting necessary chunk from
    /// chunks pool.
    void* operator new(size_t size);
    /// Release memory of blocks set returning chunk to chunks pool
    void operator delete(void* mem_ptr);

    /// Create set of blocks with given size index, make set belonging to
    /// given equally-sized blocks pool.
    CNCMMBlocksSet(CNCMMSizePool* pool, unsigned int size_index);
    ~CNCMMBlocksSet(void);

    /// Get memory block not used by anyone
    void* GetBlock(void);
    /// Release memory block - it's not used anymore
    void ReleaseBlock(void* block);
    /// Get emptiness grade of the set.
    /// Emptiness grade is recalculated each time block's got or released.
    unsigned int GetEmptyGrade(void);
    /// Get number of free blocks available in the set
    unsigned int CountFreeBlocks(void);
    /// Get pool of equally-sized blocks which this set belongs to.
    CNCMMSizePool* GetPool(void);
    /// Set the pool this set will belong to
    void SetPool(CNCMMSizePool* pool);

private:
    friend class CNCMMSizePool;

    /// Prohibit accidental use of non-implemented methods.
    CNCMMBlocksSet(const CNCMMBlocksSet&);
    CNCMMBlocksSet& operator= (const CNCMMBlocksSet&);
    void* operator new(size_t, void*);

    /// Get pointer to the first element in the table of free blocks.
    void** x_GetFirstFreePtr(void);
    /// Recalculate emptiness grade of the set.
    void x_CalcEmptyGrade(void);

    /// Actual data of the set (must be last in the class).
    /// Contains table of free blocks and blocks presented to outside
    /// themselves.
    char               m_Data[kNCMMSetDataSize];
};


/// Special "set of equally-sized blocks" containing only one block which size
/// is equal to size of memory chunk (a bit less actually) or more (several
/// chunks combined). This class is also used for allocated blocks that are
/// bigger than slab size (actually never happens in NetCache but
/// implementation must exist just in case). Actual data of the block is not
/// included into the set (as opposed to CNCMMBlocksSet), so it's just meta-
/// information about the block.
class CNCMMBigBlockSet : public CNCMMBlocksSetBase
{
public:
    /// Allocate new set for the block of given combined size.
    /// Chain of chunks necessary for such block is requested from per-thread
    /// chains pool.
    void* operator new(size_t, size_t combined_size);
    /// Delete set that was created for the block of given combined size.
    /// Chain of chunks is returned back to the chains pool.
    void operator delete(void* mem_ptr, size_t combined_size);
    /// Delete the set for big block.
    /// It's assumed that method is called just after set destructor and thus
    /// all data in memory still exists and size of block that was allocated
    /// can be read from there.
    void operator delete(void* mem_ptr);

    /// Create set for the block of given size
    CNCMMBigBlockSet(size_t block_size);
    ~CNCMMBigBlockSet(void);
    /// Get pointer to data of the block represented by this set
    void* GetData(void);

private:
    /// Prohibit accidental use of non-implemented methods.
    CNCMMBigBlockSet(const CNCMMBigBlockSet&);
    CNCMMBigBlockSet& operator= (const CNCMMBigBlockSet&);
    void* operator new(size_t);
    void* operator new(size_t, void*);

    /// Get number of chunks necessary to fit block of given size.
    static unsigned int x_GetChunksCnt(size_t combined_size);
};


/// Special memory slab representing blocks bigger than kNCMMSlabSize (or
/// actually kNCMMMaxCombinedSize). Slab doesn't include actual data of the
/// block - just all necessary meta-information. Also its structure is exactly
/// the same as for the standard CNCMMSlab with CNCMMBigBlockSet in the first
/// chunk. And this fact is used for calculation of kNCMMMaxCombinedSize
/// later.
class CNCMMBigBlockSlab : public CNCMMSlabBase
{
public:
    /// Allocate new slab for block of the given size
    void* operator new(size_t, size_t block_size);
    /// Delete slab created for block of the given size
    void operator delete(void* mem_ptr, size_t block_size);
    /// Delete slab for big block.
    /// It's assumed that method is called just after slab destructor and thus
    /// all data in memory still exists and size of block that was allocated
    /// can be read from there.
    void operator delete(void* mem_ptr);

    /// Create slab for block of the given size
    CNCMMBigBlockSlab(size_t block_size);
    ~CNCMMBigBlockSlab(void);
    /// Get pointer to data of the block represented by this slab
    void* GetData(void);

private:
    /// Prohibit accidental use of non-implemented methods.
    CNCMMBigBlockSlab(const CNCMMBigBlockSlab&);
    CNCMMBigBlockSlab& operator=(const CNCMMBigBlockSlab&);
    void* operator new(size_t);
    void* operator new(size_t, void*);

    /// Set for big blocks containing meta information about block.
    /// It's necessary for compatibility with all blocks allocated by memory
    /// manager - they all belong to some slab and blocks set.
    CNCMMBigBlockSet m_BlockSet;
};


/// Maximum size of allocated block that can be achieved by combining several
/// memory chunks together. Size of this block can actually be a bit more than
/// all chunks in slab combined but it definitely won't exceed size of
/// standardly allocated slab.
static const size_t kNCMMMaxCombinedSize =
                                    kNCMMSlabSize - sizeof(CNCMMBigBlockSlab);


/// Helper class to make CNCMMChainsPool available different for each thread.
/// Actually only kNCMMMaxThreadsCnt different instances of pool is supported.
/// Object should be a singleton.
///
/// @sa CNCMMChainsPool, kNCMMMaxThreadsCnt
class CNCMMChainsPool_Getter : public CNCTlsObject<CNCMMChainsPool_Getter,
                                                   CNCMMChainsPool>
{
    typedef CNCTlsObject<CNCMMChainsPool_Getter, CNCMMChainsPool>  TBase;

public:
    /// Empty constructor to avoid any problems with some compilers
    CNCMMChainsPool_Getter(void);
    /// Initialize static structures of the getter and parent object.
    /// Method should be called only once before getter is used.
    void Initialize(void);
    /// Create instance of CNCMMChainsPool for the current thread.
    CNCMMChainsPool* CreateTlsObject(void);
    /// Delete CNCMMChainsPool instance from the current thread.
    static void DeleteTlsObject(void* obj_ptr);

private:
    /// Prohibit accidental call to non-implemented methods.
    CNCMMChainsPool_Getter(const CNCMMChainsPool_Getter&);
    CNCMMChainsPool_Getter& operator= (const CNCMMChainsPool_Getter&);
    void* operator new (size_t);
    void* operator new (size_t, void*);

    /// Global lock managing creation and deletion of pool instances
    static CNCMMMutex       sm_CreateLock;
    /// Array of all pool instances used in manager
    static CNCMMChainsPool  sm_Pools   [kNCMMMaxThreadsCnt];
    /// Array of pool instances that are to be yet used
    static CNCMMChainsPool* sm_PoolPtrs[kNCMMMaxThreadsCnt];
    /// Number of already used pool instances
    static unsigned int     sm_CntUsed;
};

/// Per-thread pool of free chains of chunks for speeding up their allocation
/// and deallocation. Only one chain of each possible size is stored which
/// still can result in significant amount of unused memory left in "reserved"
/// state. But for NetCache this approach along with trick in allocation come
/// from SQLite works pretty well.
/// "Per-thread"ness is implemented transparent for outside - one should
/// access only public static methods without caring about details.
class CNCMMChainsPool
{
public:
    /// Initialize chains pooling system.
    /// Method should be called at the very beginning before pools are used.
    static void Initialize(void);
    /// Get chain with given size (number of chunks inside the chain).
    static void* GetChain(unsigned int chain_size);
    /// Put free chunks chain of given size to the pool for future re-use.
    static void PutChain(void* mem_ptr, unsigned int chain_size);

    /// Empty constructor to avoid any problems in some compilers.
    /// All initialization is made in InitInstance().
    CNCMMChainsPool(void);
    /// Initialize the pool instance.
    /// Method should be called only once at the very beginning. At all other
    /// times pool supports consistent state even if it was deleted from one
    /// thread and then assigned to another.
    void InitInstance(void);
    /// Add reference to the pool.
    /// References are necessary when there's more than kNCMMMaxThreadsCnt
    /// threads is running.
    void AddRef(void);
    /// Release reference from the pool.
    /// After releasing last reference pool expects to be put in the queue for
    /// re-use in some new thread and thus cleans itself moving all chains to
    /// the central reserve.
    unsigned int ReleaseRef(void);

private:
    /// Prohibit accidental call to non-implemented methods.
    CNCMMChainsPool(const CNCMMChainsPool&);
    CNCMMChainsPool& operator= (const CNCMMChainsPool&);
    void* operator new(size_t);
    void* operator new(size_t, void*);

    /// Get chain with given size from this pool instance
    void* x_GetChain(unsigned int chain_size);
    /// Put chain with given size to this pool instance
    void x_PutChain(void* mem_ptr, unsigned int chain_size);
    /// Transfer all chains stored to the central reserve.
    void x_ReleaseAllChains(void);


    /// Mutex protecting object using
    CNCMMMutex      m_ObjLock;
    /// List of cached chains distributed by their size
    void*           m_FreeChains[kNCMMCntChunksInSlab + 1];
    /// Reference counter for this object
    CAtomicCounter  m_RefCnt;

    /// Getter providing pools on thread-by-thread basis
    static CNCMMChainsPool_Getter sm_Getter;
};


/// Number of emptiness grades in which all slabs will be divided.
/// Number shouldn't be too big because all algorithms are linear. And this
/// number is made power of 2 + 1 because 0 is always a separated grade, but
/// all others will form power of 2.
///
/// @sa CNCMMReserve
static const unsigned int kNCMMSlabEmptyGrades  = 9;
/// Number of emptiness grades in which all blocks sets will be divided.
/// Number shouldn't be too big because all algorithms are linear. And this
/// number is made power of 2 + 1 because 0 is always a separated grade, but
/// all others will form power of 2.
static const unsigned int kNCMMSetEmptyGrades   = 5;
/// Total number of emptiness grades that blocks sets will have.
/// They will be divided into groups by emptiness grades of slabs they belong
/// to and sub-divided by their own grades.
///
/// @sa CNCMMSizePool
static const unsigned int kNCMMTotalEmptyGrades =
                                   kNCMMSlabEmptyGrades * kNCMMSetEmptyGrades;


/// Central storage of all memory chunks that are not used by anybody but
/// cannot be released to the system because of necessity to combine them in
/// slabs. Central storage functionality is implemented by static methods of
/// this class. Individual instances of the class represent different
/// emptiness grades of slabs containing chunks. It's made to use first all
/// chunks from the most filled up slabs and chunks from the least filled up
/// slabs to use last. All chains within one storage are stored separated
/// by chain size and combined in double-linked lists to allow to store
/// several chains of each size.
class CNCMMReserve
{
public:
    /// Initialize central reserve system.
    /// Method should be called at the very beginning before any use.
    static void Initialize(void);
    /// Get chain with given size (number of memory chunks inside the chain).
    static void* GetChain(unsigned int chain_size);
    /// Throw the chain of given size into the central storage.
    static void DumpChain(void* chain_ptr, unsigned int chain_size);
    /// Introduce newly created chain.
    /// Method is intended to be called only from slab constructor.
    static void IntroduceChain(CNCMMFreeChunk* chain,
                               unsigned int    chain_size);
    /// Get several memory chunks for use as single units.
    ///
    /// @param chunk_ptrs
    ///   Array of memory chunks to be filled
    /// @param max_cnt
    ///   Maximum number of chunks that can be filled into array
    /// @return
    ///   Number of chunks put into array. This number can be less than
    ///   max_cnt not only because there's no more chunks available but
    ///   because reserve makes only one trip to the chunks storage and if it
    ///   finds less chunks than needed then it returns what it have. This is
    ///   done to save a time because call to this method will take place
    ///   during regular call to malloc().
    static unsigned int   GetChunks(CNCMMFreeChunk** chunk_ptrs,
                                    unsigned int     max_cnt);
    /// Throw several memory chunks into the central storage.
    ///
    /// @param chunk_ptrs
    ///   Array of memory chunks
    /// @param cnt
    ///   Number of chunks in the array
    static void          DumpChunks(CNCMMFreeChunk** chunk_ptrs,
                                    unsigned int     cnt);

private:
    /// Put newly created chain of chunks into this instance of storage.
    void Link(const SNCMMChainInfo& chain);
    /// Remove given chain of chunks from this instance of storage. Return
    /// TRUE if exactly this chain did exist in the storage and it was
    /// removed, FALSE if chain didn't exist in the storage (meaning that it
    /// was removed already by another thread).
    bool UnlinkIfExist(const SNCMMChainInfo& chain);
    /// Fill array of chunks with up to max_cnt chunks. Also fill array with
    /// informations about chains which these chunks came from. Return number
    /// of chunks put into array.
    unsigned int FillChunksFromChains(CNCMMFreeChunk** chunk_ptrs,
                                      unsigned int     max_cnt,
                                      SNCMMChainInfo*  chain_ptrs);

private:
    /// Prohibit accidental call to non-implemented methods.
    CNCMMReserve(const CNCMMReserve&);
    CNCMMReserve& operator= (const CNCMMReserve&);
    void* operator new(size_t);
    void* operator new(size_t, void*);

    /// Empty constructor to avoid any problems with some compilers.
    /// All initialization is in x_InitInstance().
    CNCMMReserve(void);
    /// Initialize storage object.
    /// Method should be called only once at the very beginning.
    void x_InitInstance(void);
    /// Find free chain with size equal or greater than the given min_size.
    /// Information about chain is filled into given structure. TRUE is
    /// returned if chain is found, FALSE otherwise.
    bool x_FindFreeChain(unsigned int min_size, SNCMMChainInfo* chain);
    /// Mark list of chains with specified index if it's empty.
    /// List is marked as empty if it was marked as having some chains, and it
    /// is marked as having some chains if it was marked as empty.
    void x_MarkListIfEmpty(unsigned int list_index);
    /// Get chain with size equal or greater than given min_size. If such
    /// chain is found then its information is filled into given structure,
    /// it's removed from the storage and TRUE is returned. If no chain with
    /// appropriate size is found then FALSE is returned.
    bool x_GetChain(unsigned int min_size, SNCMMChainInfo* chain);
    /// Remove given chain from the storage.
    void x_Unlink(const SNCMMChainInfo& chain);

    /// Mark given chain as free, fill information about resulted emptiness
    /// grade of the containing slab and get information about chains around
    /// marked one if they're free.
    static void x_MarkChainFree(SNCMMChainInfo* chain,
                                SNCMMChainInfo* chain_left,
                                SNCMMChainInfo* chain_right);
    /// Mark whole chain or its part of need_size chunks as occupied.
    /// If only part of chain is occupied then the rest is marked as separate
    /// chain and is put in the appropriate storage.
    static void x_OccupyChain(const SNCMMChainInfo& chain,
                              unsigned int          need_size);
    /// Create new chunks chain from information provided and put it into
    /// appropriate storage instance.
    static void x_CreateNewChain(const SNCMMChainInfo& chain);
    /// Create new chunks chain after merging.
    /// It's analog to x_CreateNewChain() except that if chain's size is
    /// equal exactly to the whole slab then this slab is deleted and no new
    /// chain is created.
    static void x_CreateMergedChain(const SNCMMChainInfo& chain);
    /// Merge 2 chains into one.
    /// It's assumed that 2 chains given are right one after another in
    /// memory. Information about combined chain is written into chain_to.
    static void x_MergeChain(const SNCMMChainInfo& chain_from,
                             SNCMMChainInfo&       chain_to);
    /// Merge 2 chains into one only if chain_from is not occupied by some
    /// other thread. If it is not then it's also removed from storage
    /// instance where it was stored.
    static bool x_MergeChainIfValid(const SNCMMChainInfo& chain_from,
                                    SNCMMChainInfo&       chain_to);
    /// Sort pointers to memory chunks by the absolute value of their pointer,
    /// i.e. by their location in memory.
    ///
    /// @param chunk_ptrs
    ///   Array of pointers to chunks
    /// @param cnt
    ///   Number of elements in the array
    static void x_SortChunkPtrs(CNCMMFreeChunk** chunk_ptrs,
                                unsigned int     cnt);


    /// Mutex protecting access to the object
    CNCMMMutex                           m_ObjLock;
    /// Bit mask containing information about chain sizes that exist in the
    /// storage. If bit with index n is set then chain with size n exists.
    CNCBitMask<kNCMMCntChunksInSlab + 1> m_ExistMask;
    /// Lists of chains grouped by size (number of chunks in them).
    CNCMMFreeChunk*                      m_Chains[kNCMMCntChunksInSlab + 1];

    /// Instances of storage for each value of slab's emptiness grade.
    static CNCMMReserve                  sm_Instances[kNCMMSlabEmptyGrades];
};


/// Helper class to make CNCMMSizePool available different for each thread.
/// Actually only kNCMMMaxThreadsCnt different instances of pool is supported.
/// Object should be a singleton.
///
/// @sa CNCMMSizePool, kNCMMMaxThreadsCnt
class CNCMMSizePool_Getter : public CNCTlsObject<CNCMMSizePool_Getter,
                                                 CNCMMSizePool>
{
    typedef CNCTlsObject<CNCMMSizePool_Getter, CNCMMSizePool>  TBase;

public:
    /// Empty constructor to avoid any problems with some compilers
    CNCMMSizePool_Getter(void);
    /// Initialize static structures of the getter and parent object.
    /// Method should be called only once before getter is used.
    void Initialize(void);
    /// Create instance of CNCMMSizePool for the current thread.
    CNCMMSizePool* CreateTlsObject(void);
    /// Delete CNCMMSizePool instance from the current thread.
    static void DeleteTlsObject(void* obj_ptr);

    /// Get pool for size with given index located in main thread.
    static CNCMMSizePool* GetMainPool(unsigned int size_index);

private:
    /// Prohibit accidental call to non-implemented methods.
    CNCMMSizePool_Getter(const CNCMMSizePool_Getter&);
    CNCMMSizePool_Getter& operator= (const CNCMMSizePool_Getter&);
    void* operator new (size_t);
    void* operator new (size_t, void*);

    /// Global lock managing creation and deletion of pool instances
    static CNCMMMutex     sm_CreateLock;
    /// Array of pool instances that are used at the moment.
    /// Actually it's pointers to arrays of pools where array contains pool
    /// instances for each small size that memory manager allocates.
    static CNCMMSizePool* sm_PoolPtrs[kNCMMMaxThreadsCnt];
    /// Reference counters for each array of pools
    static unsigned int   sm_RefCnts [kNCMMMaxThreadsCnt];
};

/// Per-thread pool of equally-sized memory blocks.
/// Each pool contains several blocks set providing blocks of the same size.
/// All sets are distributed by their total emptiness grade so that first
/// blocks will be allocated from most occupied blocks sets from most occupied
/// slabs. Each pool also saves one completely empty blocks set to speed up
/// allocations of new blocks - it happens very often when one blocks set
/// becomes empty and then very quickly at least one block is occupied.
/// "Per-thread"ness is implemented transparent for outside - one should
/// access only public static methods without caring about details.
class CNCMMSizePool
{
public:
    /// Initialize pooling system.
    /// Method should be called at the very beginning before pools are used.
    static void Initialize(void);
    /// Allocate new block with given size.
    static void* AllocateBlock(size_t size);
    /// Deallocate block which belongs to the given blocks set.
    static void  DeallocateBlock(void* mem_ptr, CNCMMBlocksSet* bl_set);

    /// Instance of blocks pool can be created only in-place and only by
    /// CNCMMSizePool_Getter.
    void* operator new(size_t, void* mem_ptr);
    void operator delete(void*, void*);

    /// Create instance of blocks pool for given size index (indexes are from
    /// kNCMMSmallSize).
    CNCMMSizePool(unsigned int size_index);
    ~CNCMMSizePool(void);

private:
    /// Prohibit accidental call to non-implemented methods.
    CNCMMSizePool(const CNCMMSizePool&);
    CNCMMSizePool& operator= (const CNCMMSizePool&);
    void* operator new(size_t);

    /// Allocate block from this instance of blocks pool
    void* x_AllocateBlock(void);
    /// Deallocate block belonging to given blocks set from this instance
    /// of blocks pool
    void x_DeallocateBlock(void* mem_ptr, CNCMMBlocksSet* bl_set);
    /// Add blocks set to the pool's list of sets with given index
    void x_AddSetToList(CNCMMBlocksSet* bl_set, unsigned int list_grade);
    /// Remove set from the pool's list of sets with given index
    void x_RemoveSetFromList(CNCMMBlocksSet* bl_set, unsigned int list_grade);
    /// Check if emptiness grade of the given blocks set is changed and is
    /// different from given old value. If grade is changed then move the set
    /// to appropriate place in m_Sets array.
    void x_CheckGradeChange(CNCMMBlocksSet* bl_set, unsigned int old_grade);
    /// Get empty set from this pool (adjusting statistics accordingly) or by
    /// doing global allocation.
    CNCMMBlocksSet* x_GetEmptySet(void);
    /// Add just emptied blocks set with given grade before last deallocation.
    /// Set is remembered inside the pool (adjusting statistics accordingly)
    /// or globally deallocated.
    void x_NewEmptySet(CNCMMBlocksSet* bl_set, unsigned int grade);
    /// Add blocks set with given emptiness grade to this pool instance.
    /// Method is called only when one pool instance is destroyed and it needs
    /// to transfer all blocks set to some other pool.
    void x_AddBlocksSet(CNCMMBlocksSet* bl_set, unsigned int grade);


    /// Mutex protecting access to the object
    CNCMMMutex                        m_ObjLock;
    /// Index of blocks size located in this pool (index is from
    /// kNCMMSmallSize)
    unsigned int                      m_SizeIndex;
    /// Completely empty blocks set saved for speed optimization
    CNCMMBlocksSet*                   m_EmptySet;
    /// Blocks sets distributed by their total emptiness grades.
    /// Each element of the array is double-linked list of sets with the same
    /// emptiness grade.
    CNCMMBlocksSet*                   m_Sets[kNCMMTotalEmptyGrades + 1];
    /// Mask for non-empty lists in m_Sets.
    /// If bit with some index in the mask is 1 then there are some blocks
    /// sets with emptiness grade equal to this index in m_Sets.
    CNCBitMask<kNCMMTotalEmptyGrades> m_ExistMask;

    /// Getter providing pools on thread-by-thread basis
    static CNCMMSizePool_Getter       sm_Getter;
    /// Mutex providing exclusive execution of object destructor.
    /// Implement with fast read/write lock to allow methods to execute
    /// without blocking each other (read lock) but when destructor is
    /// executing (write lock) everything else will be blocked.
    /// NB: Probably it's better to use some other class here which doesn't
    /// contain any finalization code in the destructor because object can be
    /// used after destructor too.
    ///
    /// @sa DeallocateBlock, ~CNCMMSizePool
    static CFastRWLock*               sm_DestructLock;
};


/// Hash-table of pages belonging to one instance of CNCMMDBCache.
/// Hashing is made by page's key - simple masking out some amount of least
/// significant bits from it. The goal of the table is to have about
/// kNCMMDBHashSizeFactor elements for each hash value when size of table is
/// optimal.
///
/// @sa CNCMMDBCache, kNCMMDBHashSizeFactor
class CNCMMDBPagesHash
{
public:
    CNCMMDBPagesHash(void);
    ~CNCMMDBPagesHash(void);

    /// Set optimal number of pages that should be stored in the table
    void SetOptimalSize(int size);
    /// Put page with given key into the table
    void PutPage(unsigned int key, CNCMMDBPage* page);
    /// Get page from table by its key
    CNCMMDBPage* GetPage(unsigned int key);
    /// Remove page from hash table.
    /// Return TRUE if page was indeed in the table and was removed, FALSE if
    /// there wasn't entry for the page in the table.
    bool RemovePage(CNCMMDBPage* page);
    /// Clean hash table by removing all pages with key equal or greater than
    /// given min_key.
    int RemoveAllPages(unsigned int min_key);

private:
    /// Prohibit accidental call to non-implemented methods.
    CNCMMDBPagesHash(const CNCMMDBPagesHash&);
    CNCMMDBPagesHash& operator= (const CNCMMDBPagesHash&);
    void* operator new(size_t);
    void* operator new(size_t, void*);

    /// Allocate array of pointers to database pages with size elements in it.
    CNCMMDBPage** x_AllocHash(int size);
    /// Deallocate array of pointers to database pages with size elements
    /// in it.
    void x_FreeHash(CNCMMDBPage** hash, int size);

    /// Array of pointers to database pages
    CNCMMDBPage** m_Hash;
    /// Size of the array with pages
    int           m_Size;
};


/// Instance of database cache which will cache pages from one database file.
/// According to SQLite logic with shared cache turned on only one instance
/// of this class will be created for each database file.
class CNCMMDBCache
{
public:
    /// Destroy one database page in any cache instance for use in
    /// another database page. Least recently used page is destroyed here
    /// with regards to possible fact that this page can be re-used
    /// in other thread while method is working.
    static void* DestroyOnePage(void);

    /// The only allowed form of allocation and deallocation of memory for
    /// the cache.
    void* operator new(size_t size);
    void operator delete(void* mem_ptr);

    /// Create new cache instance
    ///
    /// @param page_size
    ///   Size of pages that will be requested from this cache instance
    /// @param purgeable
    ///   TRUE if pages in this cache instance can be deleted at any time
    ///   after they're unpinned by SQLite. FALSE if pages must never be
    ///   deleted unless SQLite explicitly said so.
    CNCMMDBCache(int page_size, bool purgeable);
    ~CNCMMDBCache(void);

    /// Set soft limit on maximum number of pages kept in memory by this
    /// cache instance.
    void SetOptimalSize(int num_pages);
    /// Get number of pages in the cache instance
    int GetSize(void);

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
    void* PinPage(unsigned int key, EPageCreate create_flag);
    /// Release the page
    ///
    /// @param data
    ///   Pointer to page data
    /// @param must_delete
    ///   TRUE if page must not exist anymore, FALSE if page should be kept
    ///   until cache decides that it should be reused for other purposes.
    void UnpinPage(void* data, bool must_delete);
    /// Delete from cache all pages with keys greater or equal to min_key
    void DeleteAllPages(unsigned int min_key);

    /// Remove page from cache without deleting it.
    void DetachPage(CNCMMDBPage* page);

private:
    /// Prohibit accidental call to non-implemented methods.
    CNCMMDBCache(const CNCMMDBCache&);
    CNCMMDBCache& operator=(const CNCMMDBCache&);
    void* operator new(size_t, void*);

    /// Add page with given key to hash-table.
    void x_AttachPage(unsigned int key, CNCMMDBPage* page);


    /// Mutex protecting access to the object
    CFastMutex       m_ObjLock;
    /// TRUE if pages in this cache instance can be deleted at any time
    /// after they're unpinned by SQLite. FALSE if pages must never be
    /// deleted unless SQLite explicitly said so.
    bool             m_Purgable;
    /// Number of pages stored in this cache instance
    int              m_CacheSize;
    /// Maximum value of key among pages stored in the cache
    unsigned int     m_MaxKey;
    /// Hash-table of key->page bindings
    CNCMMDBPagesHash m_Pages;
};


/// Number of distinct sizes (below size of memory chunk) that memory manager
/// can allocate. Number should correspond to number of elements in
/// kNCMMSmallSize.
static const unsigned int kNCMMCntSmallSizes = 39;


/// Helper class to make CNCMMStats available different for each thread.
/// Actually only kNCMMMaxThreadsCnt different instances of statistics is
/// supported.
/// Object should be a singleton.
///
/// @sa CNCMMStats, kNCMMMaxThreadsCnt
class CNCMMStats_Getter : public CNCTlsObject<CNCMMStats_Getter, CNCMMStats>
{
    typedef CNCTlsObject<CNCMMStats_Getter, CNCMMStats>  TBase;

public:
    /// Empty constructor to avoid any problems with some compilers
    CNCMMStats_Getter(void);
    /// Initialize static structures of the getter and parent object.
    /// Method should be called only once before getter is used.
    void Initialize(void);
    /// Create instance of CNCMMStats for the current thread.
    CNCMMStats* CreateTlsObject(void);
    /// Delete CNCMMStats instance from the current thread.
    static void DeleteTlsObject(void* obj_ptr);

private:
    /// Prohibit accidental call to non-implemented methods.
    CNCMMStats_Getter(const CNCMMStats_Getter&);
    CNCMMStats_Getter& operator= (const CNCMMStats_Getter&);
    void* operator new (size_t);
    void* operator new (size_t, void*);
};

/// Per-thread statistics of memory manager operations.
/// Thread separation is necessary to lessen contention between them. Combined
/// values are needed pretty rarely anyway.
/// "Per-thread"ness is implemented transparent for outside - one should
/// access only public static methods without caring about details.
class CNCMMStats
{
public:
    /// Initialize statistics subsystem.
    /// Method should be called only once at the very beginning.
    static void Initialize(void);
    /// Take all statistics from all threads collected at this moment and
    /// put it inside one statistics object for printing it.
    static void CollectAllStats(CNCMMStats* stats);
    /// Same as CollectAllStats() but summarize only memory usage numbers from
    /// statistics.
    static void AggregateUsage(CNCMMStats* stats);

    /// Get amount of memory allocated from OS.
    size_t GetSystemMem(void) const;
    /// Get amount of free memory not used by anything
    size_t GetFreeMem(void) const;
    /// Get amount of memory used by database cache
    size_t GetDBCacheMem(void) const;
    /// Print all memory statistics.
    void Print(CPrintTextProxy& proxy);

    /// Register memory allocation from OS.
    ///
    /// @param alloced_size
    ///   Allocated memory size
    /// @param asked_size
    ///   Amount of memory requested for allocation
    void SysMemAlloced(size_t alloced_size, size_t asked_size);
    /// Register memory releasing to OS.
    ///
    /// @param alloced_size
    ///   Allocated memory size
    /// @param asked_size
    ///   Amount of memory requested for allocation
    void SysMemFreed  (size_t alloced_size, size_t asked_size);
    /// Register reallocation of system memory to align it properly.
    void SysMemRealloced(void);
    /// Register allocation of memory needed only for manager purposes only.
    static void OverheadMemAlloced(size_t mem_size);
    /// Register deallocation of memory needed only for manager purposes only.
    static void OverheadMemFreed  (size_t mem_size);
    /// Register creation of free memory chunk (i.e. CNCMMFreeChunk).
    static void FreeChunkCreated(void);
    /// Register deletion of free memory chunk (i.e. CNCMMFreeChunk).
    static void FreeChunkDeleted(void);
    /// Register memory reservation for future use.
    static void ReservedMemCreated(size_t mem_size);
    /// Register re-use of reserved memory or its return to central storage.
    static void ReservedMemDeleted(size_t mem_size);
    /// Register refilling of CNCMMChunksPool with chunks_cnt new chunks from
    /// central reserve.
    static void ChunksPoolRefilled(unsigned int chunks_cnt);
    /// Register releasing of chunks_cnt chunks from CNCMMChunksPool to the
    /// central reserve.
    static void ChunksPoolCleaned(unsigned int chunks_cnt);
    /// Register creation of new storage for page in database cache.
    static void DBPageCreated(void);
    /// Register deletion of storage for page in database cache.
    static void DBPageDeleted(void);
    /// Register creation of new set of blocks with given size index.
    static void BlocksSetCreated(unsigned int size_index);
    /// Register deletion of set of blocks with given size index.
    static void BlocksSetDeleted(unsigned int size_index);
    /// Register allocation of new memory slab.
    static void SlabCreated(void);
    /// Register deallocation of memory slab.
    static void SlabDeleted(void);
    /// Register allocation of memory block with size with given index.
    static void MemBlockAlloced(unsigned int size_index);
    /// Register deallocation of memory block with size with given index.
    static void MemBlockFreed  (unsigned int size_index);
    /// Register allocation of chain with given number of chunks for the
    /// memory block with given size.
    static void ChunksChainAlloced(size_t block_size, unsigned int chain_size);
    /// Register deallocation of chain with given number of chunks for the
    /// memory block with given size.
    static void ChunksChainFreed  (size_t block_size, unsigned int chain_size);
    /// Register allocation of chain from central reserve (as opposed to
    /// allocation from chains pool).
    static void ChainCentrallyAlloced(void);
    /// Register deallocation of chain from central reserve (as opposed to
    /// deallocation to chains pool).
    static void ChainCentrallyFreed  (void);
    /// Register allocation of big block - block that doesn't fit into
    /// one memory slab.
    static void BigBlockAlloced(size_t block_size);
    /// Register deallocation of big block - block that doesn't fit into
    /// one memory slab.
    static void BigBlockFreed  (size_t block_size);
    /// Register request of database page that exist in cache.
    static void DBPageHitInCache(void);
    /// Register request of database page that doesn't exist in cache.
    static void DBPageNotInCache(void);

    /// Register measurement of memory usage summary for the purpose of
    /// calculating average usage values.
    void AddAggregateMeasures(const CNCMMStats& stats);

public:
    /// Empty constructor to avoid any problems in some compilers.
    /// All initialization is made in InitInstance().
    CNCMMStats(void);
    /// Initialize the statistics instance.
    void InitInstance(void);

private:
    friend class CNCMMStats_Getter;

    /// Prohibit accidental call to non-implemented methods.
    CNCMMStats(const CNCMMStats&);
    CNCMMStats& operator= (const CNCMMStats&);
    void* operator new(size_t);
    void* operator new(size_t, void*);

    /// Sum up statistics contained in this object into another object.
    void x_CollectAllTo(CNCMMStats* stats);


    /// Mutex controlling access to the object.
    CNCMMMutex            m_ObjLock;

    /// Amount of memory allocated from OS.
    size_t                m_SystemMem;
    /// Amount of free memory not used by anything.
    size_t                m_FreeMem;
    /// Amount of memory not used by anything but reserved in internal
    /// structures for future uses.
    size_t                m_ReservedMem;
    /// Amount of memory used by memory manager itself.
    size_t                m_OverheadMem;
    /// Amount of lost memory that won't be used by anything ever.
    size_t                m_LostMem;
    /// Amount of memory used by database cache.
    size_t                m_DBCacheMem;
    /// Amount of memory used by data blocks requested by user.
    size_t                m_DataMem;
    /// Aggregate values of memory allocated from OS over time.
    CNCStatFigure<size_t> m_SysMemAggr;
    /// Aggregate values of free memory over time.
    CNCStatFigure<size_t> m_FreeMemAggr;
    /// Aggregate values of memory reserved in internal structures over time.
    CNCStatFigure<size_t> m_RsrvMemAggr;
    /// Aggregate values of memory used by memory manager over time.
    CNCStatFigure<size_t> m_OvrhdMemAggr;
    /// Aggregate values of lost memory over time.
    CNCStatFigure<size_t> m_LostMemAggr;
    /// Aggregate values of memory used by database cache over time.
    CNCStatFigure<size_t> m_DBCacheMemAggr;
    /// Aggregate values of memory used by data allocated by program over time.
    CNCStatFigure<size_t> m_DataMemAggr;

    /// Number of memory allocations from OS.
    Uint8        m_SysAllocs;
    /// Number of releases of memory to OS.
    Uint8        m_SysFrees;
    /// Number of re-allocations of memory from OS to align it properly.
    Uint8        m_SysReallocs;
    /// Number of refillings of chunks pool from central reserve.
    Uint8        m_ChunksPoolRefills;
    /// Number of releases chunks from pool to central reserve.
    Uint8        m_ChunksPoolCleans;
    /// Number of chunk chains allocated from central reserve (as opposed to
    /// allocation from chains pool).
    Uint8        m_ChainsCentrallyAlloced;
    /// Number of chunk chains deallocated to central reserve (as opposed to
    /// storage in chains pool).
    Uint8        m_ChainsCentrallyFreed;
    /// Number of memory slabs allocated.
    Uint8        m_SlabsAlloced;
    /// Number of memory slabs deallocated.
    Uint8        m_SlabsFreed;
    /// Number of big memory blocks (not fitting into memory slab) allocated.
    Uint8        m_BigAlloced;
    /// Number of big memory blocks (not fitting into memory slab)
    /// deallocated.
    Uint8        m_BigFreed;
    /// Number of database pages requested by SQLite.
    Uint8        m_DBPagesRequested;
    /// Number of database pages that were in database cache when they were
    /// requested.
    Uint8        m_DBPagesHit;
    /// Number of memory blocks allocated distributed by their size.
    Uint8        m_BlocksAlloced[kNCMMCntSmallSizes];
    /// Number of memory blocks deallocated distributed by their size.
    Uint8        m_BlocksFreed  [kNCMMCntSmallSizes];
    /// Number of created blocks sets distributed by blocks size.
    Uint8        m_SetsCreated  [kNCMMCntSmallSizes];
    /// Number of deleted blocks sets distributed by blocks size.
    Uint8        m_SetsDeleted  [kNCMMCntSmallSizes];
    /// Number of created chunks chains distributed by their size.
    Uint8        m_ChainsAlloced[kNCMMCntChunksInSlab + 1];
    /// Number of deleted chunks chains distributed by their size.
    Uint8        m_ChainsFreed  [kNCMMCntChunksInSlab + 1];

    /// Getter providing statistics on thread-by-thread basis
    static CNCMMStats_Getter sm_Getter;
    /// Array of all statistics instances used in manager
    static CNCMMStats sm_Stats[kNCMMMaxThreadsCnt];
};


/// Mode of operation of memory manager with regards to allocation of new
/// memory chunks for database cache.
enum ENCMMMode {
    eNCMemGrowing,   ///< New memory chunks can be allocated for database
                     ///< cache without limits.
    eNCMemStable,    ///< Memory consumption should stay the same, so database
                     ///< cache should reuse memory from old pages.
    eNCMemShrinking, ///< Memory consumption should be less, so some memory
                     ///< used for database cache should be freed.
    eNCFinalized     ///< Memory manager was finalized, application will soon
                     ///< stop working.
};

/// Central part of memory manager - all interaction with it not related to
/// database cache starts here. Functionality was separated from CNCMemManager
/// to avoid mix of something that should be used only inside memory manager
/// implementation (i.e. only inside nc_memory.cpp) with something that can be
/// used outside.
class CNCMMCentral
{
public:
    /// Allocate memory block of given size
    static void*  AllocMemory  (size_t size);
    /// Deallocate memory block
    static void   DeallocMemory(void* mem_ptr);
    /// Change allocated size of memory block to the new one
    static void*  ReallocMemory(void* mem_ptr, size_t new_size);
    /// Get size of allocated memory block
    static size_t GetMemorySize(void* mem_ptr);

    /// Get mode of memory manager operation
    static ENCMMMode GetMemMode      (void);
    /// Check if memory consumption is over alert limit
    static bool      IsOnAlert       (void);
    /// Get global limit for memory consumption.
    static size_t    GetMemLimit     (void);
    /// Get alert level of memory consumption.
    static size_t    GetMemAlertLevel(void);
    /// Set global limit and alert level for memory consumption.
    /// At least half of the limit will be spent on database cache.
    static void      SetMemLimits    (size_t limit, size_t alert_level);

    /// Get memory slab containing given memory block
    static CNCMMSlab* GetSlab(void* mem_ptr);

    /// Allocate memory from OS
    static void* SysAlloc       (size_t size);
    /// Allocate aligned memory from OS.
    /// Memory will be aligned to kNCMMAlignSize boundary.
    ///
    /// @sa kNCMMAlignSize
    static void* SysAllocAligned(size_t size);
    /// Release memory to OS
    static void  SysFree        (void* ptr, size_t size);

    /// Late initialization of memory manager.
    /// Method is intended for stuff that cannot be executed in x_Initialize()
    /// because it needs all corelib (at least) to be initialized before. Now
    /// method is needed only for running manager's background thread.
    static bool RunLateInit(void);
    /// Prepare memory manager to stop when application finished its work.
    /// Now method is needed only to stop manager's background thread.
    static void PrepareToStop(void);

    /// Get statistics collected for centrally managed operations.
    static CNCMMStats& GetStats(void);
    /// Register allocation of memory for new database page.
    /// Method calculates how many more pages can be allocated and adjusts
    /// mode of operation accordingly.
    static void DBPageCreated(void);
    /// Register deallocation of memory used by database page.
    /// Method calculates how many more pages should be deallocated and
    /// adjusts mode of operation accordingly.
    static void DBPageDeleted(void);

private:
    /// Class will never be instantiated.
    CNCMMCentral(void);

    /// Initialize memory manager.
    /// Method should be called only once at the very beginning when manager
    /// was not used yet which is flagged by sm_Initialized.
    ///
    /// @sa sm_Initialized
    static void x_Initialize(void);
    /// Align memory pointer to the nearest kNCMMAlignSize boundary which is
    /// located in lower address space than given pointer.
    static void*  x_AlignToBottom  (void* ptr);
    /// Align memory pointer to the nearest kNCMMAlignSize boundary which is
    /// located in higher address space than given pointer.
    static void*  x_AlignToTop     (void* ptr);
    /// Change memory size so that it contains even number of memory pages
    /// used by OS to allocate all memory.
    static size_t x_AlignSizeToPage(size_t size);
    /// Perform the OS-dependent allocation of memory.
    /// Method should be called under central mutex.
    static void* x_DoAlloc(size_t& size);
    /// Long way of aligned memory allocation when OS gives non-aligned
    /// memory and we must align it by ourselves.
    /// Method should be called under central mutex.
    static void  x_AllocAlignedLongWay(void*& mem_ptr, size_t size);

#ifndef NCBI_OS_MSWIN
    /// Implementation of call to mmap() for memory allocation.
    /// It can differ from one Unix system to another.
    static void* x_DoCallMmap(size_t size);
#endif

    /// Calculate mode of working with memory (sm_Mode) based on memory usage
    /// statistics given in statistics object.
    /// Method is called periodically from x_DoBackGroundWork().
    static void x_CalcMemoryMode(const CNCMMStats& stats);
    /// Running method for the work that needs to be done in background.
    static void x_DoBackgroundWork(void);



    /// Mutex controlling access to central operations
    static CNCMMMutex     sm_CentralLock;
    /// Central operations statistics.
    /// By-thread statistics is not used for central operations because they
    /// are executed under central mutex anyway. Also this makes easier access
    /// to information about total allocated memory from OS, e.g.
    static CNCMMStats     sm_Stats;
    /// Flag showing that memory manager was already initialized.
    static bool           sm_Initialized;
    /// Mode of memory manager operation.
    static ENCMMMode      sm_Mode;
    /// Flag showing that memory consumption has grown above alert level and
    /// should be lowered by all means available including those outside
    /// memory manager itself.
    static bool           sm_OnAlert;
    /// Global limit of memory consumption.
    static size_t         sm_MemLimit;
    /// Amount of memory consumption that should be considered dangerous.
    static size_t         sm_MemAlertLevel;
    /// Number of memory chunks that can be allocated for database pages with
    /// current memory consumption numbers.
    static CAtomicCounter sm_CntCanAlloc;
    /// Background thread of memory manager.
    static CThread*       sm_BGThread;
    /// Semaphore allowing quick finalization of the background thread.
    static CSemaphore     sm_WaitForStop;
};

END_NCBI_SCOPE

#endif /* NETCACHE_NC_MEMORY__HPP */
