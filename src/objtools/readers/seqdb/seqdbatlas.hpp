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

#ifdef _DEBUG

#include <iostream>

// These defines implement a system for testing memory corruption,
// especially overwritten pointers and clobbered objects.  They are
// only enabled in debug mode.

// If this is required for release mode as well, be sure to replace
// _ASSERT() with something useful for release mode.

/// Define memory marker for class (4+ bytes of uppercase ascii).
#define CLASS_MARKER_FIELD(a) \
    static int    x_GetClassMark()  { return *((int *)(a a)); } \
    static string x_GetMarkString() { return string((a a), sizeof(int)); } \
    int m_ClassMark;

/// Marker initializer for constructor
#define INIT_CLASS_MARK() m_ClassMark = x_GetClassMark()

/// Assertion to verify the marker
#define CHECK_MARKER() \
   if (m_ClassMark != x_GetClassMark()) { \
       cout << "\n!! Broken  [" << x_GetMarkString() << "] mark detected.\n" \
            << "!! Mark is [" << hex << m_ClassMark << "], should be [" \
            << hex << x_GetClassMark() << "]." << endl; \
       _ASSERT(m_ClassMark == x_GetClassMark()); \
   }

#define BREAK_MARKER() m_ClassMark |= 0x20202020;

#else

/// Define memory marker for class.
#define CLASS_MARKER_FIELD(a)

/// Initializer for constructor
#define INIT_CLASS_MARK()

/// Assertion to verify the marker
#define CHECK_MARKER()

#define BREAK_MARKER()

#endif


/// CSeqDBAtlas class - a collection of memory maps.
class CSeqDBAtlas; // WorkShop needs this forward declaration.


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
    /// Flush held memory references.
    /// 
    /// This class should be subclassed, and this method overloaded to
    /// flush held memory that is not in use.  Normally, this amounts
    /// to calling the Clear() method on any CSeqDBMemLease objects
    /// which hold references to regions of memory.  Each open file
    /// can have a memory lease which holds onto a slice of memory, if
    /// that file has been accessed.  Without this callback, these
    /// slices can combine to exhaust the address space.  This action
    /// carries a performance penalty, because subsequent access to
    /// the released slices will require searching for the correct
    /// memory block.  If that block was deleted, it will be remapped,
    /// which may trigger another garbage collection.
    
    virtual void operator()() = 0;
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
        INIT_CLASS_MARK();
    }
    
    /// Destructor
    /// 
    /// The class will unlock the atlas's lock on destruction (if it
    /// is the owner of that lock).
    inline ~CSeqDBLockHold();
    
private:
    CLASS_MARKER_FIELD("LHLD")
    
    /// Private method to prevent copy construction.
    CSeqDBLockHold(CSeqDBLockHold & oth);
    
    /// Only the atlas code is permitted to modify this object - it
    /// does so simply by editing the m_Locked member as needed.
    friend class CSeqDBAtlas;
    
    /// This reference allows unlock on exit.
    class CSeqDBAtlas & m_Atlas;
    
    /// If this is true, this thread owns the atlas lock.
    bool m_Locked;
};


/// CSeqDBMemReg
/// 
/// This class is used to keep track of bytes allocated externally to
/// the atlas, but included under its memory bound.

class CSeqDBMemReg {
public:
    /// Constructor
    /// 
    /// This object constructs to an empty state, which means that the
    /// atlas does not consider this object to "own" any bytes.
    ///
    /// @param atlas
    ///   A reference to the atlas object.
    CSeqDBMemReg(class CSeqDBAtlas & atlas)
        : m_Atlas(atlas),
          m_Bytes(0)
    {
    }
    
    /// Destructor
    /// 
    /// The class will unlock the atlas's lock on destruction (if it
    /// is the owner of that lock).
    inline ~CSeqDBMemReg();
    
private:
    /// Private method to prevent copy construction.
    CSeqDBMemReg(CSeqDBMemReg & oth);
    
    /// Only the atlas code is permitted to modify this object - it
    /// does so simply by editing the m_Locked member as needed.
    friend class CSeqDBAtlas;
    
    /// This reference allows unlock on exit.
    class CSeqDBAtlas & m_Atlas;
    
    /// This object "owns" this many bytes of the atlas memory bound.
    size_t m_Bytes;
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
    /// The type used for file offsets.
    typedef CNcbiStreamoff TIndx;
    
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
               int            fid,
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
    bool MapMmap(CSeqDBAtlas * atlas);
    
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
    bool MapFile(CSeqDBAtlas * atlas);
    
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
        CHECK_MARKER();
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
        CHECK_MARKER();
#ifdef _DEBUG
        if (! (m_Ref > 0)) {
            cout << "refcount non-positive  = " << m_Ref << endl;
        }
#endif
        
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
        CHECK_MARKER();
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
        CHECK_MARKER();
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
        CHECK_MARKER();
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
        CHECK_MARKER();
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
        CHECK_MARKER();
        return m_End;
    }
    
    /// Return the length of this mapping in bytes.
    TIndx Length()
    {
        CHECK_MARKER();
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
    const char * Data()
    {
        CHECK_MARKER();
        return m_Data;
    }
    
    /// Return the file id associated with this region.
    /// 
    /// This method returns an integer used to identify this file
    /// within the atlas code.  The integer used here is uniquely
    /// associated with the filename (but costs slightly fewer CPU
    /// cycles during certain common operations).
    /// 
    /// @return
    ///   An integer identifying the file this mapping is of.
    int Fid()
    {
        CHECK_MARKER();
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
        CHECK_MARKER();
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
        CHECK_MARKER();
        _ASSERT(! m_Ref);
        m_Clock++;
    }
    
    /// Test and select this region if the arguments match.
    /// 
    /// This method takes arguments describing a section of a file.
    /// If this region has the same fid, and the byte range of this
    /// region encompasses the specified range, then the reference
    /// count is incremented, the input parameters are expanded to
    /// reflect the entire region, and start is set to point to the
    /// beginning of the requested portion of the region.  This method
    /// also returns a pointer to the beginning of the input range
    /// (before adjusting begin and end).
    /// 
    /// @param fid
    ///   The number identifying this file to the atlas code.
    /// @param begin
    ///   The beginning offset of the selected area.
    /// @param end
    ///   The ending offset of the selected area.
    /// @param start
    ///   Returned pointer to the beginning of the mapped region.
    /// @return
    ///   Pointer to the requested portion of the region.
    inline const char *
    MatchAndUse(int              fid,
                TIndx          & begin,
                TIndx          & end,
                const char    ** start);
    
    /// In debugging / verbose code, dump information about this
    /// memory region object.
    void Show();
    
    /// Get the data pointer and offset range of this region.
    /// 
    /// This method returns a pointer to this region's data, and the
    /// beginning and ending offsets of the region.
    /// 
    /// @param region_start
    ///   Returns the address of the region data.
    /// @param begin_offset
    ///   The beginning offset of the region data.
    /// @param end_offset
    ///   The end offset of the region data.
    void GetBoundaries(const char ** region_start,
                       TIndx      &  begin_offset,
                       TIndx      &  end_offset)
    {
        CHECK_MARKER();
        *region_start = m_Data;
        begin_offset  = m_Begin;
        end_offset    = m_End;
    }
    
    /// Less-than operator.
    /// 
    /// This method defines an partial ordering over CRegionMap
    /// objects.  The order defined here places the objects in
    /// ascending order by fid, then start offset, then region size
    /// (equivalently, end offset).  The ordering is partial because
    /// two region maps could have identical parameters.  However,
    /// this should never happen for the atlas code as defined.
    /// 
    /// @param other
    ///   The other region map to compare with.
    /// @return
    ///   True if this object is before "other" in the ordering.
    inline bool operator < (const CRegionMap & other) const;
    
    void Verify()
    {
#ifdef _DEBUG
        // Cannot do too much, as the client code creates a CRegionMap
        // to use as a key, and it will not be completely specified.
        
        CHECK_MARKER();
        _ASSERT(m_Begin < m_End);
#endif
    }
    
private:
    CLASS_MARKER_FIELD("REGM")
    
    /// Compute the penalty and the expanded offset range.
    /// 
    /// The files are normally divided into "slices".  When a mapping
    /// request asks for a specific range of offsets, we attempt to
    /// expand the offset range to the entire slice that contains the
    /// requested area.  When memory mapping is selected, a large
    /// slice size is used.  For files, a smaller area is used.  The
    /// penalty value returned indicates how desireable the selected
    /// mapping is thought to be.  Mapping one whole slice gets a
    /// value of zero.  Partial slices, areas overlapping the ends of
    /// two slices, and other anomalies get higher penalties.
    /// 
    /// @param begin
    ///   The starting offset (this method may adjust it).
    /// @param end
    ///   The ending offset (this method may adjust it).
    /// @param penalty
    ///   The penalty for this mapping is returned.
    /// @param file_size
    ///   The total size of the file.
    /// @param use_mmap
    ///   Whether memory mapping will be used.
    /// @param atlas
    ///   A pointer to the atlas object.
    static void x_Roundup(TIndx       & begin,
                          TIndx       & end,
                          int         & penalty,
                          TIndx         file_size,
                          bool          use_mmap,
                          CSeqDBAtlas * atlas);
    
    /// Pointer to allocated or mapped memory area.
    const char     * m_Data;
    
    /// The memory mapped file object.
    CMemoryFileMap * m_MemFile;
    
    /// Pointer to the filename (this object does not own it).
    const string * m_Fname;
    
    /// The file offset of the start of the region.
    TIndx m_Begin;
    
    /// The file offset of the end of the region.
    TIndx m_End;
    
    /// An integer identifying this object to the atlas code.
    int m_Fid;
    
    /// Number of current users of this object.
    int m_Ref;
    
    /// GC uses this to rank how recently this range was used.
    int m_Clock;
    
    /// Adjustment to m_Clock to bias region collection.
    int m_Penalty;
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
    /// The type used for file offsets.
    typedef CRegionMap::TIndx TIndx;
    
    /// Constructor
    /// 
    /// Initializes a memory lease object.
    /// 
    /// @param atlas
    ///   The main atlas pointer.
    CSeqDBMemLease(CSeqDBAtlas & atlas)
        : m_Atlas(atlas),
          m_Data (0),
          m_Begin(0),
          m_End  (0),
          m_RMap (0)
    {
        INIT_CLASS_MARK();
    }
    
    /// Destructor
    /// 
    /// Verifies this object is not active at time of destruction.
    ~CSeqDBMemLease()
    {
        CHECK_MARKER();
        _ASSERT(m_Data == 0);
        BREAK_MARKER();
    }
    
    /// Clears the lease object.
    /// 
    /// This method releases the reference count on the held region .
    /// It assumes the lock is held.
    inline void Clear();
    
    /// Check whether the held area includes an offset range.
    /// 
    /// This checks whether the region associated with this object
    /// already encompasses the desired offset range.  If so, there is
    /// no need to call GetRegion, because you can already access it
    /// from this object.  It assumes the atlas lock is held.
    /// 
    /// @param begin
    ///   Start of the offset range.
    /// @param end
    ///   End of the offset range.
    /// @return
    ///   Returns true if the held region includes the offset range.
    inline bool Contains(TIndx begin, TIndx end) const;
    
    /// Get a pointer to the specified offset.
    /// 
    /// Given an offset (which is assumed to be available here), this
    /// method returns a pointer to the data at that offset.
    /// 
    /// @param offset
    ///   The required offset relative to the start of the file.
    /// @return
    ///   A pointer to the data at the requested location.
    const char * GetPtr(TIndx offset) const
    {
        CHECK_MARKER();
        return (m_Data + (offset - m_Begin));
    }
    
    /// Return true if this object is inactive.
    /// 
    /// An active object holds a reference to a region of a file.
    /// This method returns true if this object is not active.
    /// 
    /// @return
    ///   true, if this object does not hold a reference.
    bool Empty() const
    {
        CHECK_MARKER();
        return m_Data == 0;
    }
    
    /// Get another reference to the held CRegionMap object.
    ///
    /// To call this method, this object must be active (already hold
    /// a reference to a CRegionMap object).  This method gets an
    /// additional hold on that object by incrementing the reference
    /// count again.  This is the mechanism that prevents the region
    /// from being collected while the user works with it, even if
    /// this lease object (which is probably stored somewhere under a
    /// CSeqDBVolume) is released and garbage collection occurs.  If
    /// and when the user returns the pointer to the region to CSeqDB,
    /// the atlas code will find the region and decrement the count.
    inline void IncrementRefCnt();
    
private:
    CLASS_MARKER_FIELD("LEAS")
    
    /// This method is declared private to prevent copy construction.
    CSeqDBMemLease(const CSeqDBMemLease & ml);
    
    /// This method is declared private to prevent assignment.
    CSeqDBMemLease & operator =(const CSeqDBMemLease & ml);
    
    /// Active the region, setting its data members.
    /// 
    /// When the GetRegion call returns a region in a MemLease object,
    /// it increments the reference count, and calls this method to
    /// populate this object with the summary data it needs.  This
    /// method is private so that only the atlas object can call it.
    /// 
    /// @param begin
    ///   The starting offset of the area this lease can access.
    /// @param end
    ///   The ending offset of the area this lease can access.
    /// @param data
    ///   The pointer to the data area this lease can access.
    /// @param rmap
    ///   A pointer to the object that manages the file region.
    void x_SetRegion(TIndx        begin,
                     TIndx        end,
                     const char * data,
                     CRegionMap * rmap)
    {
        CHECK_MARKER();
        Clear();
        
        m_Data  = data;
        m_Begin = begin;
        m_End   = end;
        m_RMap  = rmap;
    }
    
    /// Access to the private data is restricted to the atlas code.
    friend class CSeqDBAtlas;
    
    /// The atlas associated with this object.
    CSeqDBAtlas & m_Atlas;
    
    /// Points to the beginning of the data area.
    const char * m_Data;
    
    /// Start offset of the data relative to the start of the file.
    TIndx m_Begin;
    
    /// End offset of the data relative to the start of the file.
    TIndx m_End;
    
    /// Points to the object that manages the file region. 
   class CRegionMap * m_RMap;
};


/// CSeqDBAtlas class
/// 
/// The object manages a collection of (memory) maps.  It mmaps or
/// reads data from files on demand, to allow a set of files to be
/// accessed efficiently by SeqDB.  The total size of the files used
/// by a multivolume database may exceed the usable address space of
/// the system by several times.  SeqDB also registers certain large,
/// dynamically allocated (via new[]) memory blocks with this object,
/// in an effort to limit the total memory usage.  This class also
/// contains the primary mutex used to sequentialize access to the
/// various SeqDB critical regions, some of which are outside of this
/// class.  ["Atlas: n. 1. A book or bound collection of maps..."; The
/// American Heritage Dictionary of the English Language, 4th Edition.
/// Copyright © 2000 by Houghton Mifflin Company.]

class CSeqDBAtlas {
    /// Default slice sizes and limits used by the atlas.
    enum {
        eTriggerGC         = 1024 * 1024 * 128,
        eDefaultBound      = 1024 * 1024 * 1024,
        eDefaultSliceSize  = 1024 * 1024 * 128,
        eDefaultOverhang   = 1024 * 1024 * 4,
        eMaxOpenRegions    = 500,
        eOpenRegionsWindow = 100
    };
    
public:
    /// The type used for file offsets.
    typedef CRegionMap::TIndx TIndx;
    
    /// Constructor
    /// 
    /// Initializes the atlas object.
    /// 
    /// @param use_mmap
    ///   If false, use read(); if true, use mmap() or similar.
    /// @param cb
    ///   Callback to release CSeqMemLease objects before garbage collection.
    CSeqDBAtlas(bool use_mmap, CSeqDBFlushCB * cb);
    
    /// The destructor unmaps and frees all associated memory.
    ~CSeqDBAtlas();
    
    /// Check if file exists.
    /// 
    /// If the file exists, this method will return true, otherwise
    /// false.  Also, if the file exists, the first slice of it will
    /// be mapped.  This method exists for efficiency reasons; it is
    /// cheaper to ask the atlas whether it already knows about a file
    /// than to search the filesystem for the file, particularly in
    /// the case of network file systems.  If a slice of the file has
    /// already been mapped, this will return true without consulting
    /// the file system, so this method will not detect files that
    /// have been deleted since the mapping occurred.
    /// 
    /// @param fname
    ///   The filename of the file to get.
    /// @param locked
    ///   The lock hold object for this thread.
    /// @return
    ///   A pointer to the beginning of the file's data.
    bool DoesFileExist(const string & fname, CSeqDBLockHold & locked);
    
    /// Get mapping of an entire file.
    /// 
    /// A file is mapped or read, in its entirety.  The region is held
    /// in memory until the pointer is returned with RetRegion().  The
    /// atlas will be locked if necessary.
    /// 
    /// @param fname
    ///   The filename of the file to get.
    /// @param length
    ///   The length of the file is returned here.
    /// @param locked
    ///   The lock hold object for this thread.
    /// @return
    ///   A pointer to the beginning of the file's data.
    const char * GetFile(const string & fname, TIndx & length, CSeqDBLockHold & locked);
    
    /// Get mapping of an entire file.
    /// 
    /// A file is mapped or read, in its entirety.  The region is held
    /// in memory until the lease object is cleared.  The atlas will
    /// be locked if necessary.
    /// 
    /// @param lease
    ///   The lease which owns the hold on the region.
    /// @param fname
    ///   The filename of the file to get.
    /// @param length
    ///   The length of the file is returned here.
    /// @param locked
    ///   The lock hold object for this thread.
    void GetFile(CSeqDBMemLease & lease, const string & fname, TIndx & length, CSeqDBLockHold & locked);
    
    /// Get size of a file.
    /// 
    /// Check whether a file exists and get the file's size.
    /// 
    /// @param fname
    ///   The filename of the file to get the size of.
    /// @param length
    ///   The length of the file is returned here.
    /// @param locked
    ///   The lock hold object for this thread.
    /// @return
    ///   true if the file exists.
    bool GetFileSize(const string   & fname,
                     TIndx          & length,
                     CSeqDBLockHold & locked);
    
    /// Get size of a file.
    /// 
    /// Check whether a file exists and get the file's size.  This
    /// version assumes the atlas lock is held.
    /// 
    /// @param fname
    ///   The filename of the file to get the size of.
    /// @param length
    ///   The length of the file is returned here.
    /// @return
    ///   true if the file exists.
    bool GetFileSizeL(const string & fname,
                                   TIndx        & length);
    
    /// Gets a partial mapping of the file.
    /// 
    /// Part of a file is mapped or read.  The region is held in
    /// memory until the pointer is released with RetRegion().  This
    /// method may read or map a larger section of the file than was
    /// asked for. It may also return a pointer into an area that was
    /// mapped by a previous call.  The atlas will be locked if
    /// necessary.
    /// 
    /// @param fname
    ///   The filename of the file to get.
    /// @param begin
    ///   The start offset of the area to map.
    /// @param end
    ///   The end offset of the area to map.
    /// @param locked
    ///   The lock hold object for this thread.
    /// @return
    ///   A pointer to the beginning of the file's data.
    const char * GetRegion(const string   & fname,
                           TIndx            begin,
                           TIndx            end,
                           CSeqDBLockHold & locked);
    
    /// Gets a partial mapping of the file (assumes lock is held).
    /// 
    /// Part of a file is mapped or read.  The region is held in
    /// memory until the lease object is cleared.  This method may
    /// read or map a larger section of the file than was asked for.
    /// It may also return a pointer into an area that was mapped by a
    /// previous call.  The atlas is assumed to be locked.
    /// 
    /// @param lease
    ///   The memory lease object to store the region hold.
    /// @param fname
    ///   The name of the file to get.
    /// @param begin
    ///   The starting file offset.
    /// @param end
    ///   The ending file offset.
    void GetRegion(CSeqDBMemLease  & lease,
                   const string    & fname,
                   TIndx             begin,
                   TIndx             end);
    
    /// Release a hold on a region of memory.
    /// 
    /// This method releases holds kept by CSeqDBMemLease objects.  It
    /// release the reference count and resets the lease to an
    /// inactive state.  The atlas lock is assumed to be held.
    /// 
    /// @param ml
    ///   The memory lease object holding the reference.
    void RetRegion(CSeqDBMemLease & ml);
    
    /// Release a mapping of the file, or free allocated memory.
    /// 
    /// This method releases the holds that are gotten by the versions
    /// of the GetRegion() and GetFile() methods that return pointers,
    /// and by the Alloc() method.  Each call to a hold-getting method
    /// should be paired with a call to a hold-releasing method.  With
    /// data known to have originated in Alloc(), it is faster to call
    /// the Free() method.
    /// 
    /// @param datap
    ///   Pointer to the data to release or deallocate.
    void RetRegion(const char * datap)
    {
        Verify();
        for(int i = 0; i<eNumRecent; i++) {
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
        Verify();
    }
    
    /// Clean up unreferenced objects
    /// 
    /// This iterates over all file regions managed by the atlas code.
    /// If the region has no users, it is a candidate for removal by
    /// garbage collection.  The lock will be acquired if necessary.
    /// 
    /// @param locked
    ///   The lock hold object for this thread.
    void GarbageCollect(CSeqDBLockHold & locked);
    
    /// Display memory layout.
    /// 
    /// This method provides a way to examine the set of memory
    /// objects in use by the Atlas code, for example when running an
    /// application in a debugger.  Some or all of the functionality
    /// of this code is disabled in a normal compile; it may be
    /// necessary to define certain compilation flags to make it
    /// available again.
    /// 
    /// @param locked
    ///   If this is set to false, the atlas lock will be acquired.
    /// @param index
    ///   A value which will be inserted into the output messages.
    void ShowLayout(bool locked, TIndx index);
    
    /// Allocate memory that atlas will keep track of.
    /// 
    /// This method allocates memory for the calling code's use.
    /// There are three reasons to do this.  First, the allocated
    /// memory is guaranteed to be deleted when the atlas destructor
    /// runs, so using this method ties the lifetime of the allocated
    /// memory to that of the atlas, which may prevent memory leaks.
    /// Secondly, allocating memory in this way brings the allocated
    /// memory under the total memory bound the atlas imposes on
    /// itself.  The amount of memory assumed to be available for
    /// slice allocation will be reduced by the size of these
    /// allocations during garbage collection.  Thirdly, the memory
    /// allocated this way can be freed by the RetRegion(char*)
    /// method, so the RetSequence code in the volume layers (and
    /// thereabouts) does not need to can return allocated memory to
    /// the user as "sequence data", and does not have to track
    /// whether the data was allocated or mapped.
    /// 
    /// @param length
    ///   Amount of memory to allocate in bytes.
    /// @param locked
    ///   The lock hold object for this thread.
    /// @return
    ///   A pointer to the allocation region of memory.
    char * Alloc(size_t length, CSeqDBLockHold & locked);
    
    /// Return allocated memory.
    /// 
    /// This method returns memory acquired from the Alloc() method.
    /// Dynamically allocated memory from other sources should not be
    /// freed with this method, and memory allocated via Alloc()
    /// should not be freed by any means other than this method or
    /// RetRegion().
    /// 
    /// @param freeme
    ///   A pointer to memory allocated via Alloc().
    /// @param locked
    ///   The lock hold object for this thread.
    void Free(const char * freeme, CSeqDBLockHold & locked);
    
    /// Register externally allocated memory.
    /// 
    /// This method tells the atlas code that memory was allocated
    /// external to the atlas code, and should be included under the
    /// memory bound enforced by the atlas.  These areas of memory
    /// will not be managed by the atlas, but may influence the atlas
    /// by causing database volume files or auxiliary files to be
    /// unmapped earlier or more often.  This method may trigger atlas
    /// garbage collection.  RegisterFree() should be called when the
    /// memory is freed.
    /// 
    /// @param memreg
    ///   Memory registration tracking object.
    /// @param bytes
    ///   Amount of memory externally allocated.
    /// @param locked
    ///   The lock hold object for this thread.
    void RegisterExternal(CSeqDBMemReg   & memreg,
                          size_t           bytes,
                          CSeqDBLockHold & locked);
    
    /// Unregister externally allocated memory.
    /// 
    /// This method tells the atlas that external memory registered
    /// with RegisterExternal() has been freed.  The atlas lock is
    /// assumed to be held.
    /// 
    /// @param memreg
    ///   Memory registration tracking object.
    /// @param locked
    ///   The lock hold object for this thread.
    void UnregisterExternal(CSeqDBMemReg & memreg);
    
    /// Lock the atlas.
    /// 
    /// The internal mutual exclusion lock is currently implemented
    /// via the SpinLock class.  If the lock hold object passed to
    /// this method is already in a "locked" state, this call is a
    /// noop.  Otherwise, the lock hold object is put in a locked
    /// state and the lock is acquired.
    /// 
    /// @param locked
    ///   This object tracks whether this thread owns the mutex.
    void Lock(CSeqDBLockHold & locked)
    {
        if (! locked.m_Locked) {
            m_Lock.Lock();
            locked.m_Locked = true;
        }
    }
    
    /// Unlock the atlas.
    /// 
    /// The internal mutual exclusion lock is currently implemented
    /// via the SpinLock class.  If the lock hold object passed to
    /// this method is already in an "unlocked" state, this call is a
    /// noop.  Otherwise, the lock hold object is put in an unlocked
    /// state and the lock is released.
    /// 
    /// @param locked
    ///   This object tracks whether this thread owns the mutex.
    void Unlock(CSeqDBLockHold & locked)
    {
        if (locked.m_Locked) {
            locked.m_Locked = false;
            m_Lock.Unlock();
        }
    }
    
    /// Adjust the atlas memory management boundaries.
    /// 
    /// The memory bound determines the total amount of memory the
    /// atlas will map before triggering garbage collection.  Whenever
    /// one of the GetRegion() or GetFile() methods is called, the
    /// atlas checks whether the new allocation would bring the total
    /// mapped memory over the memory bound.  If so, it runs garbage
    /// collection, which attempts to free enough regions to satisfy
    /// the memory bound.  The normal default memory bound is defined
    /// at the top of the atlas code, and is something like 1 GB.  The
    /// second parameter is the slice size.  This is (basically) much
    /// memory is acquired at one time for code that uses mmap().
    /// Setting this smaller should reduce page table thrashing for
    /// programs that "jump around" in the database any only get a few
    /// sequences.  Setting it larger should reduce the amount of time
    /// the atlas spends finding the next slice when the calling code
    /// crosses a "slice boundary".  The default is something like
    /// 256MB.  Both defaults used here should be good enough for all
    /// but the worst cases.  Note that the memory bound should not be
    /// set according to how much memory the computer has, but rather,
    /// how much address space the application can spare.  SeqDB does
    /// not prevent itself from going over the memory bound,
    /// particularly if the user is holding onto mapped sequence data
    /// for extended periods of time.  Also note that if the atlas
    /// code thinks these parameters are set to unreasonable values,
    /// it will silently adjust them.
    /// 
    /// @param mb
    ///   Total amount of address space the atlas can use.
    /// @param ss
    ///   Default size of slice of memory to get at once.
    void SetMemoryBound(Uint8 mb, Uint8 ss);
    
    /// Insure room for a new allocation.
    /// 
    /// This method is used to insure that the atlas code has room for
    /// a new address space purchase.  When mapping new region, this
    /// method is called.  If the memory bound would be exceeded by
    /// the addition of the new element, garbage collection will run.
    /// Otherwise, this is a noop.  Even if garbage collection is run,
    /// there is no guarantee that the new object will fit within the
    /// memory bound.
    /// 
    /// @param space_needed
    ///   The size in bytes of the new region or allocation.
    void PossiblyGarbageCollect(Uint8 space_needed);
    
    /// Return the current overhang amount.
    /// 
    /// This returns the amount of memory to map past the end of the
    /// (end-of-map) slice boundary.  This is intended to prevent.
    /// 
    /// @return
    ///   Atlas will map this much data past the slice-end-boundary.
    Uint8 GetOverhang()
    {
        return eDefaultOverhang;
    }
    
    /// Return the current slice size.
    /// 
    /// This returns the current slice size used for mmap() style
    /// memory allocations.  One use of this would be to write code to
    /// adjust the memory bound, but leave the slice size as is.
    /// 
    /// @return
    ///   Atlas will try to map this much data at a time.
    Uint8 GetLargeSliceSize()
    {
        return m_SliceSize;
    }
    
    /// Return the current number of bytes allocated.
    /// 
    /// This returns the number of bytes currently allocated by the
    /// atlas code.  It does not include overhead or meta-data such as
    /// the CRegionMap objects or the atlas object itself.
    /// 
    /// @return
    ///   The amount of memory allocated in bytes.
    TIndx GetCurrentAllocationTotal()
    {
        return m_CurAlloc;
    }
    
    void Verify()
    {
#ifdef _DEBUG
        ITERATE(TNameOffsetTable, iter, m_NameOffsetLookup) {
            CRegionMap & rmp = **iter;
            rmp.Verify();
        }
#endif
    }
    
private:
    /// Private method to prevent copy construction.
    CSeqDBAtlas(const CSeqDBAtlas &);
    
    /// Iterator type for m_Pool member.
    typedef map<const char *, size_t>::iterator TPoolIter;
    
    /// Finds a matching region if any exists.
    /// 
    /// This method searches the list of mapped regions to find one
    /// with the same fid, and whose offset range includes the
    /// specified range.  If found, all of the input fields are
    /// adjusted to reflect those in the region management object
    /// (except fid, which is identical anyway).  Also, a pointer to
    /// the CRegionMap object is returned in *rmap, and a pointer to
    /// the (pre-adjustment) begin offset is returned from the method.
    /// If no region was found, NULL is returned and no changes are
    /// made to the input parameters.
    /// 
    /// @param fid
    ///   The number identifying this file to the atlas code.
    /// @param begin
    ///   The beginning offset of the selected area.
    /// @param end
    ///   The ending offset of the selected area.
    /// @param startp
    ///   Returned pointer to the beginning of the mapped region.
    /// @param rmap
    ///   Returned pointer to the CRegionMap object.
    /// @return
    ///   Pointer to the requested portion of the region.
    const char * x_FindRegion(int              fid,
                              TIndx          & begin,
                              TIndx          & end,
                              const char    ** startp,
                              CRegionMap    ** rmap);
    
    /// Gets a partial mapping of the file.
    /// 
    /// This method searches the list of mapped regions to find one
    /// with the same fid, and whose offset range includes the
    /// specified range.  If found, it will be used.  If not found, a
    /// new region will be constructed and used.  The parameters work
    /// as in In either case, the input fields are adjusted to reflect
    /// those in the region management object.  Also, a pointer to the
    /// CRegionMap object is returned in *rmap, and a pointer to the
    /// (pre-adjustment) begin offset is returned from the method.  If
    /// no region could be found or created, an exception is thrown.
    /// 
    /// @param fname
    ///   The name of the file to get a region of.
    /// @param begin
    ///   The beginning offset of the selected area.
    /// @param end
    ///   The ending offset of the selected area.
    /// @param start
    ///   Returned pointer to the beginning of the mapped region.
    /// @param rmap
    ///   Returned pointer to the CRegionMap object.
    /// @return
    ///   Pointer to the requested portion of the region.
    const char * x_GetRegion(const string   & fname,
                             TIndx          & begin,
                             TIndx          & end,
                             const char    ** start,
                             CRegionMap    ** rmap);
    
    /// Try to find the region and free it.
    /// 
    /// This method looks for the region in the memory pool (m_Pool),
    /// which means it must have been allocated with Alloc().
    /// 
    /// @param freeme
    ///   The pointer to free.
    /// @return
    ///   true, if the memory block was found and freed.
    bool x_Free(const char * freeme);
    
    /// Clean up unreferenced objects (non-locking version.)
    /// 
    /// This iterates over all file regions managed by the atlas code.
    /// If the region has no users, it is a candidate for removal by
    /// garbage collection.  The lock is assumed to be already held.
    /// If the amount of memory held is reduced to a number below
    /// reduce_to, the routine will stop freeing memory and return.
    /// 
    /// @param reduce_to
    ///   The maximum amount of allocated memory to keep.
    void x_GarbageCollect(Uint8 reduce_to);
    
    /// Find the fid for the filename.
    /// 
    /// Atlas maps filenames to fids.  The filename is looked up and
    /// the fid is returned.  If the filename is not found, it is
    /// added to the table and a newly generated fid is returned.  The
    /// existence of the file is not verified.  The second field is
    /// used to return a pointer to the internal filename string in
    /// the map.  This will not change for a given filename, and
    /// provides a permanent (for the lifetime of CSeqDBAtlas) address
    /// for the filename, which can be used by structures like
    /// CRegionMap, so that they don't have to copy string data.
    /// 
    /// @param fname
    ///   The filename to look up.
    /// @param map_fname_ptr
    ///   The address of the filename key in the lookup table.
    /// @return
    ///   The fid
    int x_LookupFile(const string  & fname,
                       const string ** map_fname_ptr);
    
    /// Releases or deletes a memory region.
    /// 
    /// The RetRegion method is called many times.  The functionality
    /// is split into a fast inlined part and a slow non-inlined part.
    /// This is the non-inlined part.  It searches for the region or
    /// allocated block that owns the specified address, and releases
    /// it, if found.
    /// 
    /// @param datap
    ///   Pointer to the area to delete or release a hold on.
    void x_RetRegionNonRecent(const char * datap);
    

    /// Clear recent-region lookup table
    /// 
    /// Functions which delete regions call this to flush the fast
    /// lookup table for recently used region, on the grounds that it
    /// may have been invalidated (it may contain dangling pointers).
    /// This method carries a small performance cost because regions
    /// will need to be looked up using the long process until the
    /// table is repopulated.  Currently this is only called by the
    /// garbage collection code, which normally runs very seldom.
    void x_ClearRecent()
    {
        for(int i = 0; i<eNumRecent; i++) {
            m_Recent[i] = 0;
        }
    }
    
    /// Add most recently used region to the fast lookup table.
    /// 
    /// Whenever a lookup finds a region, it is added to the beginning
    /// of a fast region lookup table (m_Recent).  Existing entries in
    /// the table will move down one slot.  If the new entry already
    /// exists somewhere else in the table, it will be removed from
    /// that point (and only entries between that point and the top of
    /// the table will need to move.  If all slots in the table are
    /// used, the last entry will simply be dropped.
    /// 
    /// @param r
    ///   The most recently used region.
    void x_AddRecent(CRegionMap * r)
    {
        if (m_Recent[0] == r)
            return;
        
        int found_at = eNumRecent-1;
        
        for(int i = 0; i < eNumRecent-1; i++) {
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
    
    /// Protects most of the critical regions of the SeqDB library.
    CSeqDBSpinLock m_Lock;
    
    /// Set to true if memory mapping is enabled.
    bool m_UseMmap;
    
    /// Bytes of "data" currently known to SeqDBAtlas.  This does not
    /// include metadata, such as the region map class itself..
    TIndx m_CurAlloc;
    
    /// All of the SeqDB region maps.
    vector<CRegionMap*> m_Regions;

    /// Maps from pointers to dynamically allocated blocks to the byte
    /// size of the allocation.
    map<const char *, size_t> m_Pool;
    
    /// The most recently assigned FID.
    int m_LastFID;
    
    /// Lookup table of fids by filename.
    map<string, int> m_FileIDs;
    
    /// RegionMapLess
    /// 
    /// This functor provides an ordering over CRegionMap objects.
    
    struct RegionMapLess
        : public binary_function<const CRegionMap*, const CRegionMap*, bool>
    {
        /// Compare regions using less-than.
        /// 
        /// This compares two CRegionMap objects using that class's
        /// overloaded less-than operator.  It provides a partial
        /// ordering for the TNameOffsetTable.
        /// 
        /// @param L
        ///   The first object.
        /// @param R
        ///   The second object.
        /// @return
        ///   Returns true if the first object comes before the second one.
        inline bool operator()(const CRegionMap* L, const CRegionMap* R) const
        {
            _ASSERT(L);
            _ASSERT(R);
            return *L < *R;
        }
    };
    
    /// Defines lookup of regions by fid, offset, and length.
    typedef set<CRegionMap *, RegionMapLess> TNameOffsetTable; 
    
    /// Defines lookup of regions by starting memory address.
    typedef map<const char *, CRegionMap*>   TAddressTable;
    
    /// Map to find regions for Getting (x_FindRegion).
    TNameOffsetTable m_NameOffsetLookup;
    
    /// Map to find regions for Returning (x_RetRegionNonRecent).
    TAddressTable m_AddressLookup;
    
    // Recent region lookup
    
    /// Number of recently-used-region slots.
    enum { eNumRecent = 8 };
    
    /// Table of recently used regions.
    CRegionMap * m_Recent[ eNumRecent ];
    
    // Max memory permitted
    
    /// The maximum amount of memory used for regions, Alloc()ed data.
    Uint8 m_MemoryBound;
    
    /// Atlas will try to map this amount of memory at once.
    Uint8 m_SliceSize;
    
    /// Atlas will try to garbage collect if more than this many
    /// regions are opened.
    int m_OpenRegionsTrigger;
    
    /// Callback to flush memory leases before a garbage collector.
    CSeqDBFlushCB * m_FlushCB;
    
    /// Cache of file existence and length.
    map< string, pair<bool, TIndx> > m_FileSize;
};

// Assumes lock is held.

inline void CSeqDBMemLease::Clear()
{
    CHECK_MARKER();
    m_Atlas.RetRegion(*this);
}

// ASSUMES lock is held.
inline bool CSeqDBMemLease::Contains(TIndx begin, TIndx end) const
{
    CHECK_MARKER();
    return m_Data && (m_Begin <= begin) && (end <= m_End);
}


const char *
CRegionMap::MatchAndUse(int              fid,
                        TIndx          & begin,
                        TIndx          & end,
                        const char    ** start)
{
    CHECK_MARKER();
    _ASSERT(fid);
    _ASSERT(m_Fid);
    
    if ((fid == m_Fid) && (m_Begin <= begin) && (m_End   >= end)) {
        AddRef();
        
        const char * location = m_Data + (begin - m_Begin);
        
        begin  = m_Begin;
        end    = m_End;
        *start = m_Data;
        _ASSERT(*start);
        
        return location;
    }
    
    return 0;
}

bool CRegionMap::operator < (const CRegionMap & other) const
{
    CHECK_MARKER();
    // Sort ascending by fid
    
    if (m_Fid < other.m_Fid)
        return true;
    if (other.m_Fid < m_Fid)
        return false;
    
    // Sort ascending by start order
    
    if (m_Begin < other.m_Begin)
        return true;
    if (other.m_Begin < m_Begin)
        return false;
    
    // Sort ascending by size of mapping.
    
    return m_End < other.m_End;
}

inline CSeqDBLockHold::~CSeqDBLockHold()
{
    CHECK_MARKER();
    m_Atlas.Unlock(*this);
    BREAK_MARKER();
}

inline CSeqDBMemReg::~CSeqDBMemReg()
{
    m_Atlas.UnregisterExternal(*this);
}

inline void CSeqDBMemLease::IncrementRefCnt()
{
    CHECK_MARKER();
    _ASSERT(m_Data && m_RMap);
    m_RMap->AddRef();
}


END_NCBI_SCOPE

#endif // OBJTOOLS_READERS_SEQDB__SEQDBATLAS_HPP

