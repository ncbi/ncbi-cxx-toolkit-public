#ifndef OBJTOOLS_READERS_SEQDB__SEQDBATLAS_HPP
#define OBJTOOLS_READERS_SEQDB__SEQDBATLAS_HPP

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
 * Author:  Kevin Bealer
 *
 */

/// @file seqdbatlas.hpp
/// The SeqDB memory management layer.
/// 
/// Defines classes:
///     CSeqDBFlushCB
///     CSeqDBSpinLock
///     CSeqDBLockHold
///     CRegionMap
///     CSeqDBMemLease
///     CSeqDBAtlas
/// 
/// Implemented for: UNIX, MS-Windows


#include <corelib/ncbimtx.hpp>
#include <corelib/ncbiatomic.hpp>
#include <corelib/ncbifile.hpp>
#include <vector>
#include <map>
#include <set>

BEGIN_NCBI_SCOPE

class CSeqDBAtlas;


/// CSeqDBFlushCB
/// 
/// This abstract class defines an interface which can be called to
/// free unused regions of Atlas managed memory.  If a class derived
/// from this interface is provide to the atlas constructor, it will
/// be called at the beginning of garbage collection.  It is expected
/// to release reference counts on any sections of memory that should
/// be considered for garbage collection.

class CSeqDBFlushCB {
public:
    virtual void operator()(void) = 0;
};


/// CSeqDBSpinLock
/// 
/// This locking class is similar to a mutex, but uses a faster and
/// simpler "spin lock" mechanism instead of the normal mutex
/// algorithms.  The downside of this technique is that code which is
/// waiting for a lock will not give up the processor to tasks which
/// are doing work.  Because of this, it should only be used for locks
/// which are acquired many times and held for a short duration.
/// In the special purpose environment of SeqDB, with local or NFS
/// mapped databases, it was a noticeable win relative to the two
/// standard locks.  This was true in both multithreaded and single
/// threaded blast run tests.

class CSeqDBSpinLock {
public:
    /// Constructor.
    /// 
    /// Spin locks are initialized to an unlocked state.
    CSeqDBSpinLock()
      : m_L(0)
    {
    }
    
    /// Destructor
    ///
    /// On destruction, this class does not unlock or validate the
    /// object in any way.
    ~CSeqDBSpinLock()
    {
    }
    
    /// Lock
    ///
    /// Runs in a loop, trying to acquired the lock.  Does not return
    /// until it succeeds.
    void Lock()
    {
        bool done = false;
        
        while(! done) {
            while(m_L)
                ;
            
            void * NewL = (void *) 1;
            void * OldL = SwapPointers(& m_L, NewL);
            
            if (OldL == (void*) 0) {
                done = true;
            }
        }
    }
    
    /// Unlock
    ///
    /// Sets the lock to an unlock state.
    void Unlock()
    {
        // If we hold the lock, atomicity shouldn't be an issue here.
	m_L = (void*)0;
    }
    
private:
    /// The underlying data - if this is a 1, cast to a void pointer,
    /// the lock is held (by someone).  If it is NULL, it's available.
    void * volatile m_L;
};


/// CSeqDBLockHold
/// 
/// This class is used to keep track of whether this thread holds the
/// atlas lock.  The atlas code will skip subsequent Lock() operations
/// during the same transaction if the lock is already held.  This
/// allows code that needs locking to get the lock without worrying
/// about whether the calling function has already done so.  The
/// destructor of this object will call Unlock() on the atlas if this
/// thread has it locked.

class CSeqDBLockHold {
public:
    /// Constructor
    /// 
    /// This object constructs to an unlocked state, which means that
    /// the thread owning the stack frame where this object is stored
    /// does not hold the atlas lock.  This object keeps a reference
    /// to the atlas object so that it can release the lock on
    /// destruction, making it easier to write exception safe code.
    /// @param atlas
    ///   A reference to the atlas object.
    CSeqDBLockHold(class CSeqDBAtlas & atlas)
        : m_Atlas(atlas),
          m_Locked(false)
    {
    }
    
    /// Destructor
    /// 
    /// The class will unlock the atlas's lock on destruction (if it
    /// is the owner of that lock).
    inline ~CSeqDBLockHold();
    
private:
    /// Private copy constructor
    /// 
    /// This is private to prevent automatic copy construction.
    CSeqDBLockHold(CSeqDBLockHold & oth);
    
    /// Only the atlas code is permitted to modify this object - it
    /// does so simply by editing the m_Locked member as needed.
    friend class CSeqDBAtlas;
    
    /// This reference allows unlock on exit.
    class CSeqDBAtlas & m_Atlas;
    
    /// If this is true, this thread owns the atlas lock.
    bool m_Locked;
};


/// CRegionMap
/// 
/// This object stores data relevant to a mapped portion of a file.
/// The mapped area may be a memory map or an allocated buffer filled
/// with data read from a file.  It also remembers some garbage
/// collection statistics, used to determine if and when to reclaim
/// this region.  The atlas code manages a collection of these
/// objects.

class CRegionMap {
public:
    /// This defines the type used for offsets into files.
    typedef Uint4 TIndx;
    
    /// Constructor
    /// 
    /// Initializes the object, storing the provided filename, file
    /// id, and byte range.  This object does not own the string
    /// containing the filename, and does not map any memory or
    /// do any filesystem operations during construction.
    /// @param fname
    ///   Pointer to a string containing the name of the file.
    /// @param fid
    ///   An integer uniquely identifying the file to the atlas.
    /// @param begin
    ///   The offset of the beginning of the byte range.
    /// @param end
    ///   The offset of the end of the byte range.
    CRegionMap(const string * fname,
               Uint4          fid,
               TIndx          begin,
               TIndx          end);
    
    /// Destructor
    /// 
    /// Frees or unmaps any memory associated with this region.
    ~CRegionMap();
    
    /// Memory map a region of a file
    /// 
    /// This uses CMemoryFile to map the file region associated with
    /// this object.  The range of memory may be rounded up to the
    /// nearest slice boundary, and may trigger (atlas's) garbage
    /// collection routine if the new memory area won't fit into the
    /// memory bound provided.  The mapping will be attempted even if
    /// the memory bound requirements are not acheivable, but this
    /// will probably result in performance degradation due to
    /// thrashing atlas's garbage collector.
    /// 
    /// @param atlas
    ///   Pointer to the atlas.
    /// @return
    ///   Returns true if the mapping succeeded.
    bool MapMmap(CSeqDBAtlas *);
    
    /// Read a region of a file into memory
    /// 
    /// This uses CNcbiIfstream to read the file region associated
    /// with this object into an allocated memory block.  The range of
    /// memory may be rounded up (but only to the nearest block
    /// boundary), and may trigger (atlas's) garbage collection
    /// routine if the new memory area won't fit into the memory bound
    /// provided.  The read will be attempted even if the memory bound
    /// requirements are not acheivable, but this will probably result
    /// in performance degradation due to thrashing atlas's garbage
    /// collector.
    /// 
    /// @param atlas
    ///   Pointer to the atlas.
    /// @return
    ///   Returns true if the read succeeded.
    bool MapFile(CSeqDBAtlas *);
    
    /// Increment this region's reference count.
    /// 
    /// This increments the count of "users" associated with this
    /// region.  When too much memory is used, memory regions may be
    /// freed.  This process (garbage collection) will skip over any
    /// regions in use at the time.  This method is the way to
    /// indicates that an operation is done using this memory region
    /// and expects it to remain intact for the duration.
    void AddRef()
    {
        _ASSERT(m_Ref >= 0);
        m_Ref ++;
        m_Clock = 0;
    }
    
    /// Decrement this region's reference count.
    /// 
    /// This decrements the count of "users" associated with this
    /// region.  If the count becomes zero, the region becomes
    /// available for the next run of garbage collection.  This method
    /// indicates that an operation is done using this memory region.
    void RetRef()
    {
        _ASSERT(m_Ref > 0);
        m_Ref --;
    }
    
    /// Return true if memory region is in use.
    /// 
    /// This returns true if any users (normally, CSeqMemLease
    /// objects) are holding a reference on this memory region.
    ///
    /// @return
    ///   true if memory region has current users.
    bool InUse()
    {
        return m_Ref != 0;
    }
    
    /// Return true if memory region contains the pointer.
    /// 
    /// This method returns true if the specified pointer points to a
    /// character in the memory area managed by this object.  It is
    /// used to find the region to decrement when a user provides a
    /// region to "give back".
    /// 
    /// @return
    ///   true if this memory region contains the pointer
    bool InRange(const char * p) const
    {
        _ASSERT(m_Data);
        return ((p >= m_Data) && (p < (m_Data + (m_End - m_Begin))));
    }
    
    /// Return the name of the file.
    /// 
    /// Provides the name of the file from which this data was
    /// obtained.
    ///
    /// @return
    ///   A const reference to the file name string.
    const string & Name()
    {
        return *m_Fname;
    }
    
    /// Return the beginning offset of this region.
    /// 
    /// Provides the file offset corresponding to the beginning of
    /// this mapping.
    /// 
    /// @return
    ///   The index of the beginning of this mapping.
    TIndx Begin()
    {
        return m_Begin;
    }
    
    /// Return the end offset of this region.
    /// 
    /// Provides the file offset corresponding to the end of this
    /// mapping.
    /// 
    /// @return
    ///   The index of the end of this mapping.
    TIndx End()
    {
        return m_End;
    }
    
    /// Return the length of this mapping in bytes.
    TIndx Length()
    {
        return m_End - m_Begin;
    }
    
    /// Get a pointer to a section of this data.
    /// 
    /// This method takes an offset range (start and end), and returns
    /// a pointer to the data at the start offset.  The offsets are
    /// relative to the file, not the region.  The end offset is only
    /// used to check that the region contains the entire range.
    /// 
    /// @param begin
    ///   The offset of the first required byte.
    /// @param end
    ///   The offset of the last required byte, plus one.
    /// @return
    ///   A pointer to the beginning of the requested data.
    const char * Data(TIndx begin, TIndx end);
    
    /// Get a pointer to all data in this region.
    /// 
    /// This method returns a pointer to the data at the start offset
    /// of this region.
    /// 
    /// @return
    ///   A pointer to the beginning of the region's data.
    const char * Data() { return m_Data; }
    
    /// Return the file id associated with this region.
    /// 
    /// This method returns an integer used to identify this file
    /// within the atlas code.  The integer used here is uniquely
    /// associated with the filename (but costs slightly fewer CPU
    /// cycles during certain common operations).
    /// 
    /// @return
    ///   An integer identifying the file this mapping is of.
    Uint4 Fid()
    {
        return m_Fid;
    }
    
    /// Get the garbage collection clock value.
    /// 
    /// This method is used by the atlas code's garbage collection
    /// routine.  The clock concept is borrowed from certain operating
    /// system paging techniques, and is described further in the
    /// garbage collection.  This function returns that clock value
    /// plus a penalty, which is used to preferentially page out
    /// regions with undesireable "shapes", e.g. that don't have a
    /// size of exactly one (large) slice.
    /// 
    /// @return
    ///   The clock value plus the penalty for this region.
    int GetClock()
    {
        return m_Clock + m_Penalty;
    }
    
    /// Increase the garbage collection clock count.
    /// 
    /// This method increments the clock value, used by the atlas
    /// code's garbage collection routine.  This method should never
    /// be called on an in-use region.  The higher the clock value
    /// gets, the closer this region is to being freed by the garbage
    /// collection code.
    void BumpClock()
    {
        _ASSERT(! m_Ref);
        m_Clock++;
    }
    
    inline const char *
    MatchAndUse(Uint4            fid,
                TIndx          & begin,
                TIndx          & end,
                const char    ** start);
    
    void Show();
    
    void GetBoundaries(const char ** p,
                       TIndx      &  begin,
                       TIndx      &  end)
    {
        *p    = m_Data;
        begin = m_Begin;
        end   = m_End;
    }
        
    void x_Roundup(TIndx       & begin,
                   TIndx       & end,
                   Int4        & penalty,
                   TIndx         file_size,
                   bool          use_mmap,
                   CSeqDBAtlas * atlas);
    
    inline bool operator < (const CRegionMap & other) const;
        
private:
    const char     * m_Data;
    CMemoryFileMap * m_MemFile;
        
    const string * m_Fname;
    TIndx          m_Begin;
    TIndx          m_End;
    Uint4          m_Fid;
    
    // Usage of clock: When a reference is gotten, the clock is
    // set to zero.  When GC is run, all unused clocks are bumped.
    // The GC adds "clock" and "penalty" together and removes
    // regions with the highest resulting value first.
        
    Int4 m_Ref;
    Int4 m_Clock;
    Int4 m_Penalty;
};


/// CSeqDBMemLease
/// 
/// This object holds a reference on a region of memory managed by the
/// atlas code, and prevent the region from being unmapped.  This
/// allows for rapid access to that memory for the duration of the
/// transaction, and for subsequent transactions, without needing to
/// call GetRegion() until memory outside the region is needed.
/// Since these objects can prevent large slices of memory from being
/// unmapped, the SeqDB code uses a callback functor (CSeqDBFlushCB)
/// to allow the garbage collection code to break these holds, and
/// return the associated regions, if the memory bound is exceeded.

class CSeqDBMemLease {
public:
    typedef CRegionMap::TIndx TIndx;
    
    CSeqDBMemLease(CSeqDBAtlas & atlas)
        : m_Atlas(atlas),
          m_Data (0),
          m_Begin(0),
          m_End  (0),
          m_RMap (0)
    {
    }
    
    ~CSeqDBMemLease()
    {
        _ASSERT(m_Data == 0);
    }
    
    // Assumes lock is held
    inline void Clear();
    
    // ASSUMES lock is held.
    inline bool Contains(TIndx begin, TIndx end) const;
    
    const char * GetPtr(TIndx offset) const
    {
        return (m_Data + (offset - m_Begin));
    }
    
    bool Empty() const
    {
        return m_Data == 0;
    }
    
    inline void IncrementRefCnt();
    
private:
    CSeqDBMemLease(const CSeqDBMemLease & ml);
    CSeqDBMemLease & operator =(const CSeqDBMemLease & ml);
    
    void SetRegion(TIndx        begin,
                   TIndx        end,
                   const char * data,
                   CRegionMap * rmap)
    {
        Clear();
        
        m_Data  = data;
        m_Begin = begin;
        m_End   = end;
        m_RMap  = rmap;
    }
    
    friend class CSeqDBAtlas;
    
    CSeqDBAtlas      & m_Atlas;
    const char       * m_Data;
    TIndx              m_Begin;
    TIndx              m_End;
    class CRegionMap * m_RMap;
};


/// CSeqDBAtlas class
/// 
/// This object manages a set of memory areas, usually memory mapped
/// files.  It maps and unmaps code as needed to allow a set of files,
/// the total size of which may exceed the address space, to be
/// accessed efficiently by SeqDB.  SeqDB also registers certain other
/// dynamic allocations (i.e. new[]) with this object, in an effort to
/// limit the total memory usage.  This class also contains the
/// primary mutex used to sequentialize access to the various SeqDB
/// critical regions, some of which are outside of this class.

class CSeqDBAtlas {
    enum {
        eTriggerGC        = 1024 * 1024 * 256,
        eDefaultBound     = 1024 * 1024 * 1024,
        eDefaultSliceSize = 1024 * 1024 * 128
    };
    
public:
    typedef CRegionMap::TIndx TIndx;
    
    /// Create an empty atlas.
    CSeqDBAtlas(bool use_mmap, CSeqDBFlushCB * cb);
    
    /// The destructor unmaps and frees all associated memory.
    ~CSeqDBAtlas();
    
    /// Gets whole-file mapping, and returns file data and length.
    const char * GetFile(const string & fname, TIndx & length, CSeqDBLockHold & locked);
    
    /// Gets whole-file mapping, and returns file data and length.
    void GetFile(CSeqDBMemLease & lease, const string & fname, TIndx & length, CSeqDBLockHold & locked);
    
    /// Gets mapping for entire file and file length, return true if found.
    bool GetFileSize(const string & fname, TIndx & length);
    
    /// Gets a partial mapping of the file.
    const char * GetRegion(const string & fname, TIndx begin, TIndx end, CSeqDBLockHold & locked);
    
    /// Gets a partial mapping of the file (assumes lock is held).
    void GetRegion(CSeqDBMemLease  & lease,
                   const string    & fname,
                   TIndx             begin,
                   TIndx             end);
    
    // Assumes lock is held.
    
    /// Releases a partial mapping of the file, or returns an allocated block.
    void RetRegion(CSeqDBMemLease & ml);
    
    /// Releases a partial mapping of the file, or returns an allocated block.
    void RetRegion(const char * datap)
    {
        for(Uint4 i = 0; i<eNumRecent; i++) {
            CRegionMap * rec_map = m_Recent[i];
            
            if (! rec_map)
                break;
            
            if (rec_map->InRange(datap)) {
                rec_map->RetRef();
                
                if (i) {
                    x_AddRecent(rec_map);
                }
                
                return;
            }
        }
        
        x_RetRegionNonRecent(datap);
    }
    
    /// Clean up unreferenced objects
    void GarbageCollect(CSeqDBLockHold & locked);
    
    /// Display memory layout (this code is temporary).
    void ShowLayout(bool locked, TIndx index);
    
    /// Allocate memory and track it with an internal set.
    char * Alloc(Uint4 length, CSeqDBLockHold & locked);
    
    /// Return memory, removing it from the internal set.
    void Free(const char * freeme, CSeqDBLockHold & locked);
    
    /// Lock internal mutex.
    void Lock(CSeqDBLockHold & locked)
    {
        if (! locked.m_Locked) {
            m_Lock.Lock();
            locked.m_Locked = true;
        }
    }
    
    /// Lock internal mutex.
    void Unlock(CSeqDBLockHold & locked)
    {
        if (locked.m_Locked) {
            locked.m_Locked = false;
            m_Lock.Unlock();
        }
    }
    
    void SetMemoryBound(Uint8 mb, Uint8 ss);
    
    void PossiblyGarbageCollect(Uint8 space_needed);
    
    Uint8 GetLargeSliceSize()
    {
        return m_SliceSize;
    }
    
private:
    /// Prevent copy construction.
    CSeqDBAtlas(const CSeqDBAtlas &);
    
    typedef map<const char *, Uint4>::iterator TPoolIter;
    
    // Assumes lock is held
    const char * x_FindRegion(Uint4            fid,
                              TIndx          & begin,
                              TIndx          & end,
                              const char    ** startp,
                              CRegionMap    ** rmap);
    
    /// Gets a partial mapping of the file.
    const char * x_GetRegion(const string   & fname,
                             TIndx          & begin,
                             TIndx          & end,
                             const char    ** start,
                             CRegionMap    ** rmap);
    
    /// Try to find the region in the memory pool and free it.
    bool x_Free(const char * freeme);
    
    /// Clean up unreferenced objects (non-locking version.)
    // Assumes lock is held
    void x_GarbageCollect(Uint8 reduce_to);
    
    // Assumes lock is held
    Uint4 x_LookupFile(const string  & fname,
                       const string ** map_fname_ptr);
    
    /// Releases a partial mapping of the file, or returns an allocated block.
    void x_RetRegionNonRecent(const char * datap);
    
    // Recent region lookup
    
    void x_ClearRecent()
    {
        for(int i = 0; i<eNumRecent; i++) {
            m_Recent[i] = 0;
        }
    }
    
    void x_RemoveRecent(CRegionMap * r)
    {
        bool found = false;
        
        for(int i = 0; i<eNumRecent; i++) {
            if (m_Recent[i] == r) {
                found = true;
                m_Recent[i] = 0;
            }
        }
        
        if (! found)
            return;
        
        // Compact the array
        
        Uint4 dst(0), src(0);
        
        while(src < eNumRecent) {
            if (m_Recent[src]) {
                m_Recent[dst++] = m_Recent[src++];
            } else {
                src++;
            }
        }
        while(dst < eNumRecent) {
            m_Recent[dst++] = 0;
        }
    }
    
    void x_AddRecent(CRegionMap * r)
    {
        if (m_Recent[0] == r)
            return;
        
        Uint4 found_at = eNumRecent-1;
        
        for(Uint4 i = 0; i < eNumRecent-1; i++) {
            if (m_Recent[i] == r) {
                found_at = i;
                break;
            }
        }
        
        while(found_at) {
            m_Recent[found_at] = m_Recent[found_at-1];
            found_at --;
        }
        
        m_Recent[0] = r;
    }
    
    
    // Data
    
    CSeqDBSpinLock m_Lock;
    
    bool       m_UseMmap;
    Uint4      m_CurAlloc;
    
    vector<CRegionMap*> m_Regions;
    map<const char *, Uint4> m_Pool;
    
    Uint4 m_LastFID;
    map<string, Uint4> m_FileIDs;
    
    struct RegionMapLess
        : public binary_function<const CRegionMap*, const CRegionMap*, bool>
    {
        inline bool operator()(const CRegionMap* L, const CRegionMap* R) const
        {
            return *L < *R;
        }
    };
    
    typedef set<CRegionMap *, RegionMapLess> TNameOffsetTable; 
    typedef map<const char *, CRegionMap*>   TAddressTable;
    
    TNameOffsetTable m_NameOffsetLookup;
    TAddressTable    m_AddressLookup;
    
    // Recent region lookup
    
    enum { eNumRecent = 8 };
    CRegionMap * m_Recent[ eNumRecent ];
    
    // Max memory permitted
    
    Uint8 m_MemoryBound;
    Uint8 m_SliceSize;
    
    CSeqDBFlushCB * m_FlushCB;
};

// Assumes lock is held.

inline void CSeqDBMemLease::Clear()
{
    m_Atlas.RetRegion(*this);
}

// ASSUMES lock is held.
inline bool CSeqDBMemLease::Contains(TIndx begin, TIndx end) const
{
    return m_Data && (m_Begin <= begin) && (end <= m_End);
}


const char *
CRegionMap::MatchAndUse(Uint4            fid,
                        TIndx          & begin,
                        TIndx          & end,
                        const char    ** start)
{
    _ASSERT(fid);
    _ASSERT(m_Fid);
    
    if ((fid == m_Fid) && (m_Begin <= begin) && (m_End   >= end)) {
        AddRef();
        
        const char * location = m_Data + (begin - m_Begin);
        
        begin  = m_Begin;
        end    = m_End;
        *start = m_Data;
        
        return location;
    }
    
    return 0;
}

bool CRegionMap::operator < (const CRegionMap & other) const
{
#define COMPARE_FIELD_AND_RETURN(OTH,FLD) \
    if (FLD < other.FLD) \
        return true; \
    if (OTH.FLD < FLD) \
        return false;
    
    // Sort via fields: fid,begin,end.
    
    COMPARE_FIELD_AND_RETURN(other, m_Fid);
    COMPARE_FIELD_AND_RETURN(other, m_Begin);
    
    return m_End < other.m_End;
}

inline CSeqDBLockHold::~CSeqDBLockHold()
{
    m_Atlas.Unlock(*this);
}

// Assumes lock is held.
inline void CSeqDBMemLease::IncrementRefCnt()
{
    _ASSERT(m_Data && m_RMap);
    m_RMap->AddRef();
}


END_NCBI_SCOPE

#endif // OBJTOOLS_READERS_SEQDB__SEQDBATLAS_HPP

