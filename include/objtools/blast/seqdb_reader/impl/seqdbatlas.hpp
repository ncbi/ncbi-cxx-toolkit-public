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
///     CSeqDBLockHold
///     CSeqDBMemReg
///     CRegionMap
///     CSeqDBMovingAverage
///     CSeqDBMapStrategy
///     CSeqDB_AtlasRegionHolder
///     CSeqDBAtlas
///     CSeqDBAtlasHolder
///
/// Implemented for: UNIX, MS-Windows


#include <corelib/ncbimtx.hpp>
#include <corelib/ncbiatomic.hpp>
#include <corelib/ncbifile.hpp>
#include <corelib/ncbi_system.hpp>

#include <objtools/blast/seqdb_reader/impl/seqdbgeneral.hpp>

#include <vector>
#include <map>
#include <set>

BEGIN_NCBI_SCOPE

#ifdef SEQDB_TRACE_LOGFILE

#define SEQDB_LOGCLASS_DEFAULT 1
#define SEQDB_LOGCLASS_MMAP    2
#define SEQDB_LOGCLASS_MFILE   4
#define SEQDB_LOGCLASS_GET     8

extern ofstream * seqdb_logfile;
extern int        seqdb_logclass;

void seqdb_log(const char * s);
void seqdb_log(const char * s1, const string & s2);

void seqdb_log(int cl, const char * s);
void seqdb_log(int cl, const char * s1, const string & s2);
void seqdb_log(int cl, const char * s1, int s2);
#endif // SEQDB_TRACE_LOGFILE

#ifdef _DEBUG

#include <iostream>

// These defines implement a system for testing memory corruption,
// especially overwritten pointers and clobbered objects.  They are
// only enabled in debug mode.

// If this is required for release mode as well, be sure to replace
// _ASSERT() with something useful for release mode.

/// Define memory marker for class (4+ bytes of uppercase ascii).

// Note: this is now strictly 4 bytes and little-endian-centric (on
// big endian architectures the bytes will be in the other order than
// the string would imply.  This should not matter to the code.

#define INT4IFY_STRING(a)    \
    (((a[3] & 0xFF) << 24) | \
     ((a[2] & 0xFF) << 16) | \
     ((a[1] & 0xFF) <<  8) | \
     ((a[0] & 0xFF)))

#define CLASS_MARKER_FIELD(a) \
    static int    x_GetClassMark()  { return INT4IFY_STRING(a);  } \
    static string x_GetMarkString() { return string((a a), 4); } \
    int m_ClassMark;

/// Marker initializer for constructor
#define INIT_CLASS_MARK() m_ClassMark = x_GetClassMark()

/// Assertion to verify the marker
#define CHECK_MARKER() \
   if (m_ClassMark != x_GetClassMark()) { \
       cout << "Marker=" << m_ClassMark << endl; \
       cout << "GetMrk=" << x_GetClassMark() << endl; \
       cout << "\n!! Broken  [" << x_GetMarkString() << "] mark detected.\n" \
            << "!! Mark is [" << hex << m_ClassMark << "], should be [" \
            << hex << x_GetClassMark() << "]." << endl; \
       _ASSERT(m_ClassMark == x_GetClassMark()); \
   }

/// Make the marker of this class invalid.
#define BREAK_MARKER() m_ClassMark |= 0x20202020;

#else

/// Define memory marker for class  (release mode code elision)
#define CLASS_MARKER_FIELD(a)

/// Initializer for constructor (release mode code elision)
#define INIT_CLASS_MARK()

/// Assertion to verify the marker (release mode code elision)
#define CHECK_MARKER()

/// Make the marker of this class invalid (release mode code elision)
#define BREAK_MARKER()

#endif

/// CSeqDBAtlas class - a collection of memory maps.
class CSeqDBAtlas; // WorkShop needs this forward declaration.
class CSeqDBFileMemMap;

/// Return path with delimiters changed to platform preferred kind.
///
/// The path is modified and returned.  The 'Make' interface is more
/// convenient for cases where the input path and output path are
/// different objects.  Delimiter conversion should be called by SeqDB
/// at least once on any path received from the user, or via
/// filesystem sources such as alias files.
///
/// @param dbs This is the input path.
/// @return The modified path is returned.
string SeqDB_MakeOSPath(const string & dbs);


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

    /// Destructor to avoid warnings.
    virtual ~CSeqDBFlushCB() {}
};


class CRegionMap;



/// CSeqDBLockHold
///
/// This class is used to keep track of whether this thread holds the
/// atlas lock.  The atlas code will skip subsequent Lock() operations
/// during the same transaction if the lock is already held.  This
/// allows code that needs locking to get the lock without worrying
/// about whether the calling function has already done so.  The
/// destructor of this object will call Unlock() on the atlas if this
/// thread has it locked.

class NCBI_XOBJREAD_EXPORT CSeqDBLockHold {
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
    ~CSeqDBLockHold();

private:
    CLASS_MARKER_FIELD("LHLD")

    /// Private method to prevent copy construction.
    CSeqDBLockHold(CSeqDBLockHold & oth);
    CSeqDBLockHold& operator=(CSeqDBLockHold & oth);

    /// Only the atlas code is permitted to modify this object - it
    /// does so simply by editing the m_Locked member as needed.
    friend class CSeqDBAtlas;

    /// This reference allows unlock on exit.
    class CSeqDBAtlas & m_Atlas;

    /// These are memory leases that should not flush during GC.
    //vector<CRegionMap*> m_Holds;

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

    /// Set the mmap strategy for index files
    static void SetMmapStrategy_Index(
            EMemoryAdvise strategy
    )
    {
        sm_MmapStrategy_Index = strategy;
    }

    /// Set the mmap strategy for sequence files
    static void SetMmapStrategy_Sequence(
            EMemoryAdvise strategy
    )
    {
        sm_MmapStrategy_Sequence = strategy;
    }

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
    //bool MapMmap(CSeqDBAtlas * atlas);

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
    //bool MapFile(CSeqDBAtlas * atlas);

    

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

    /// Verify the marker values (only in debug mode).
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

    /// Index file mmap strategy.
    static EMemoryAdvise sm_MmapStrategy_Index;

    /// Sequence file mmap strategy.
    static EMemoryAdvise sm_MmapStrategy_Sequence;
};



/// CSeqDBMovingAverage
///
/// This implements an exponentially smoothed moving average, which is
/// a technique for smoothing a noisy scalar measurement.

class CSeqDBMovingAverage {
public:
    /// Constructor.
    ///
    /// Specify a degree of smoothing between 0 and 1, along with a
    /// starting value.  Zero would mean that no smoothing is done,
    /// the result is always the last measurement.  Using 1.0 would
    /// cause the average to never change.  Note that the moving
    /// average inevitably lags behind input value changes.
    ///
    /// @param ratio Amount of smoothing to do.
    /// @param start Starting value to use for average.
    CSeqDBMovingAverage(double ratio, double start)
        : m_Ratio(ratio),
          m_Average(start)
    {
    }

    /// Add a new measurement.
    /// @param value The new measurement.
    void AddData(double value)
    {
        // Each esma is a weighted average of the last esma and the
        // new measurement.

        m_Average *= m_Ratio;
        m_Average += (value * (1.0 - m_Ratio));
    }

    /// Get the current moving average.
    double GetAverage() const
    {
        return m_Average;
    }

private:
    /// Ratio of smoothness.
    double m_Ratio;

    /// Current value.
    double m_Average;
};


/// CSeqDBMapStrategy
///
/// Some SeqDB clients access OIDs in sequential order; others tend to
/// follow a random access pattern.  This class attempts to implement
/// a kind of self-tuning for SeqDB based on client access patterns.
/// It also tries to detect and deal with memory shortage issues by
/// reducing the overall SeqDB footprint.

class CSeqDBMapStrategy {
public:
    /// The type used for file offsets.
    typedef Int8 TIndx;

    /// Maximum memory: 256 GB for 64 bits.
    static const Int8 e_MaxMemory64;

    enum {
        // On Linux, malloc() uses mmap() for large requests, which
        // means that the heap can shrink when free() is called.  In
        // this environment, SeqDB tries to probe the address space by
        // using a large memory bound and handling map failures in
        // certain routines.  This can cause stack exhaustion on other
        // platforms, so it is only currently enabled for Linux.

#if defined(NCBI_OS_LINUX)
        /// Probe address space with special allocations.
        e_ProbeMemory = true,

        /// Maximum memory: 2 GB for non-WinOS, 32 bit systems.
        e_MaxMemory32 = 768 << 20,
#else
        /// Probe address space with special allocations.
        e_ProbeMemory = false,

        /// Maximum memory: 1 GB for WinOS, 2 GB for other 32 bit.
        e_MaxMemory32 = 768 << 20,
#endif

        // Up to the AppSpace setting, SeqDB will try to consume at
        // most half of the available memory.  Beyond this point,
        // SeqDB will only leave the AppSpace amount for the
        // application.

        /// Application space - maximum memory set aside for the app.
        e_AppSpace = 256 << 20,

        /// Minimum memory bound.
        e_MinMemory = 64 << 20,

        /// Maximum slice to map on a 32 bit system.
        e_MaxSlice32 = 128 << 20,

        /// Maximum slice to map on a 64 bit system.
        e_MaxSlice64 = 1 << 30,

        /// Minimum slice size to use.
        e_MinSlice = 4 << 20,

        /// Maximum overhang.
        e_MaxOverhang = 8 << 20,

        /// Minimum overhang.
        e_MinOverhang = 256 << 10,

        /// Maximum open (mapped) regions at once.
        eMaxOpenRegions = 500,

        /// Maximum new open regions between collections.
        eOpenRegionsWindow = 100
    };

    /// Constructor
    CSeqDBMapStrategy(CSeqDBAtlas & atlas);

    

    /// Return the slice size for mmap requests.
    TIndx GetSliceSize()
    {
        return m_SliceSize;
    }

    /// Set the slice size for mmap requests.
    void SetSliceSize(TIndx size)
    {
        m_SliceSize = min(m_SliceSize, size);
    }

    /// Return the total memory bound.
    ///
    /// This returns the active memory bound.  If SeqDB is done
    /// mapping regions and is ready to return to the user, the value
    /// used is smaller.  The design goal here is to cause memory
    /// allocation failures to happen in CSeqDBAtlas rather than in
    /// user code, so that SeqDB can detect these failures and reduce
    /// its memory bound to fix the problem.  If user code allocates
    /// very large arrays on a 32 bit system, they might still be the
    /// one to get the allocation failures, in which case they might
    /// consider reducing the bound via SetMemoryBound().
    ///
    /// @param returning Specify true if returning from SeqDB.
    TIndx GetMemoryBound(bool returning)
    {
        x_CheckAdjusted();
        return returning ? m_RetBound : m_MaxBound;
    }

    /// An allocation this large will trigger a memory purge.
    TIndx GetGCTriggerSize()
    {
        return m_SliceSize*3;
    }

    /// Give this object an OID specified by the user.
    /// @param oid The user tried to fetch data via this oid.
    /// @param num_oids The total number of OIDs.
    void MentionOid(int oid, int num_oids);

    /// Tell SeqDB that a memory mapping failed.
    //void MentionMapFailure(Uint8 current);

    /// Set the memory bound using a user specified value.
    void SetMemoryBound(Uint8 max)
    {
        x_SetBounds(max);
    }

    /// Set global default memory bound.
    ///
    /// This sets the default, global memory bound used for all SeqDB
    /// objects.  If zero is specified, an appropriate default will be
    /// selected based on system information.
    static void SetDefaultMemoryBound(Uint8 bytes);

private:
    /// Check if the global bound has been adjusted and use that value.
    void x_CheckAdjusted();

    /// Mention an ordered or out-of-order access.
    void x_OidOrder(bool in_order);

    /// Restrict a number to a range.
    ///
    /// This method returns a number as close as possible to 'guess',
    /// but restricted to the range [low,high].  The number that is
    /// returned is always at least low and no higher than high, and
    /// is always a multiple of the system virtual memory blocksize.
    ///
    /// @param low Lowest value to return.
    /// @param high Highest value to return.
    /// @param guess The preferred value.
    Uint8 x_Pick(Uint8 low, Uint8 high, Uint8 guess);

    /// Set all parameters.
    void x_SetBounds(Uint8 bound);

    /// The atlas object.
    CSeqDBAtlas & m_Atlas;

    /// Maximum memory bound SeqDB may map.
    Int8 m_MaxBound;

    /// Maximum memory bound when returning from Atlas code.
    Int8 m_RetBound;

    /// Atlas will try to map files in blocks this size.
    Int8 m_SliceSize;

    /// Mapped areas of files should overlap this much.
    Int8 m_Overhang;

    /// Track sequentiality of OID accesses.
    CSeqDBMovingAverage m_Order;

    /// True if SeqDB thinks user is accessing OIDs sequentially.
    bool m_InOrder;

    /// True if mmap has failed at least once.
    bool m_MapFailed;

    /// Last OID seen.
    int m_LastOID;

    /// Block size for mapping.
    long m_BlockSize;

    /// Global maximum memory bound.
    static Int8 m_GlobalMaxBound;

    /// Has global memory bound been changed?
    static bool m_AdjustedBound;
};


/// Hold a memory region refcount, return to atlas when destroyed.
///
/// This object `owns' a reference to a region of atlas owned memory,
/// releasing that reference when destructed.  This can be used to
/// return a hold on a region of memory to the user.  Care should be
/// taken when managing these objects.  In particular there should
/// never be a case where a "live" object of this type could be
/// destroyed in a thread that already holds the atlas lock.  A simple
/// technique is to keep an extra CRef<> to this object until the
/// thread releasees the lock.

class CSeqDB_AtlasRegionHolder : public CObject {
public:
    /// Constructor.
    CSeqDB_AtlasRegionHolder(CSeqDBAtlas & atlas, const char * ptr);

    /// Destructor.
    ~CSeqDB_AtlasRegionHolder();

private:
    /// Reference to the memory management layer.
    CSeqDBAtlas & m_Atlas;

    /// Pointer to this object.
    const char  * m_Ptr;
};


/// CSeqDBAtlas class
///
/// This object manages a collection of (memory) maps.  It mmaps or
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
/// Copyright (c) 2000 by Houghton Mifflin Company.]

class NCBI_XOBJREAD_EXPORT CSeqDBAtlas {
public:
    /// The type used for file offsets.
    typedef CRegionMap::TIndx TIndx;

    /// Constructor
    ///
    /// Initializes the atlas object.
    ///
    /// @param use_mmap
    ///   If false, use read(); if true, use mmap() or similar.
    CSeqDBAtlas(bool use_mmap);

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
    ///   True if the file exists.
    bool DoesFileExist(const string & fname, CSeqDBLockHold & locked);

    /// Check if file exists.
    ///
    /// This is like the previous but accepts CSeqDB_Path.
    ///
    /// @param fname
    ///   The filename of the file to get.
    /// @param locked
    ///   The lock hold object for this thread.
    /// @return
    ///   True if the file exists.
    bool DoesFileExist(const CSeqDB_Path & fname, CSeqDBLockHold & locked)
    {
        return DoesFileExist(fname.GetPathS(), locked);
    }

    bool DoesFileExist(const string & fname)
    {
        TIndx length(0);
        return GetFileSizeL(fname, length);
    }

    bool DoesFileExist(const CSeqDB_Path & fname)
    {
        TIndx length(0);
        return GetFileSizeL(fname.GetPathS(), length);
    }
  
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
    bool GetFileSizeL(const string & fname, TIndx & length);

    /// Release a mapping of the file, or free allocated memory.
    ///
    /// This method releases the holds that are gotten by the versions
    /// of the GetRegion() and GetFile() methods that return pointers,
    /// and by the Alloc() method.  Each call to a hold-getting method
    /// should be paired with a call to a hold-releasing method.  With
    /// data known to have originated in Alloc(), it is faster to call
    /// the Free() method.  This method assumes the lock is held.
    ///
    /// @param datap
    ///   Pointer to the data to release or deallocate.
    void RetRegion(const char * datap)
    {
        Verify(true);
                
        x_RetRegionNonRecent(datap);

        Verify(true);
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
    //void ShowLayout(bool locked, TIndx index);

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
    /// @param clear
    ///   Specify true to zero out memory contents.
    /// @return
    ///   A pointer to the allocation region of memory.
    char * Alloc(size_t length, CSeqDBLockHold & locked, bool clear = true);

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
    void UnregisterExternal(CSeqDBMemReg & memreg);

    /// Lock the atlas.
    ///
    /// If the lock hold object passed to
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
    /// If the lock hold object passed to
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
    /// second parameter is the slice size.  This is (basically) how
    /// much memory is acquired at one time for code that uses mmap().
    ///
    /// Setting this to a smaller value should reduce page table
    /// thrashing for programs that access the database in a
    /// non-sequential pattern, and which only use a small portion of
    /// the database.  Setting it larger should reduce the amount of
    /// time the atlas spends finding the next slice when the calling
    /// code crosses a "slice boundary".  The default is something
    /// like 256MB.  Both defaults used here should be good enough for
    /// all but the worst cases.  Note that the memory bound should
    /// not be set according to how much memory the computer has, but
    /// rather, how much address space the application can spare.
    /// SeqDB does not prevent itself from going over the memory
    /// bound, particularly if the user is holding onto mapped
    /// sequence data for extended periods of time.  Also note that if
    /// the atlas code thinks these parameters are set to unreasonable
    /// values, it will silently adjust them.
    ///
    /// @param mb
    ///   Total amount of address space the atlas can use.
    void SetMemoryBound(Uint8 mb);

    /// Insure room for a new allocation.
    ///
    /// This method is used to insure that the atlas code has room for
    /// a new address space purchase.  When mapping new regions, this
    /// method is called.  If the memory bound would be exceeded by
    /// the addition of the new element, garbage collection will run.
    /// Otherwise, this just returns.  This does not guarantee that
    /// the new object will fit within the memory bound.  Because a
    /// slice is never collected if the user has a reference to it, it
    /// is possible for the user to exhaust memory simply by calling
    /// CSeqDB::GetSequence() once for each slice of a large database
    /// without calling CSeqDB::RetSequence().
    ///
    /// @param space_needed
    ///   The size in bytes of the new region or allocation.
    /// @param returning
    ///   Specify true if we are about to return to user code.
    void PossiblyGarbageCollect(Uint8 space_needed, bool returning);

    
    /// Get the current slice size.
    ///
    /// This returns the current slice size used for mmap() style
    /// memory allocations.
    ///
    /// @return
    ///   Atlas will try to map this much data at a time.
    Uint8 GetSliceSize()
    {
        return m_Strategy.GetSliceSize();
    }

    /// Set the MT slice size.
    /// This sets the current slice size used for mmap() allocations.
    void SetSliceSize()
    {
        m_Strategy.SetSliceSize(m_MaxFileSize);
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

    /// Verify the integrity of this object and subobjects.
    /// @param locked
    ///   The lock hold object for this thread. [in]
    void Verify(CSeqDBLockHold & locked)
    {
#ifdef _DEBUG
        Lock(locked);
        Verify(true);
#endif
    }

    /// Verify the integrity of this object and subobjects.
    /// @param already_locked
    ///   True if the lock is already held, false if not. [in]
    void Verify(bool already_locked)
    {
#ifdef _DEBUG
        CSeqDBLockHold locked(*this);

        if (! already_locked) {
            Lock(locked);
        }
        /*
        ITERATE(TNameOffsetTable, iter, m_NameOffsetLookup) {
            CRegionMap & rmp = **iter;
            rmp.Verify();
        }
        */
        if (! already_locked) {
            Unlock(locked);
        }
#endif
    }

    /// Report a user-specified OID for ordered-access detection.
    ///
    /// This class attempts to reduce thrashing by determining if the
    /// user is accessing OIDs in a "mostly" sequential order.  If the
    /// user accesses OIDs in order, there should be no possibility of
    /// "thrashing".  If not, the severity of thrashing can be reduced
    /// by selecting a smaller slice size.  An OID is considered to be
    /// a "sequential" access if it is greater than the last OID minus
    /// the greater 10 or 10% of the database.  (Multithreaded clients
    /// of SeqDB typically assign "blocks" of OIDs, which results in a
    /// "mostly" sequential access pattern.)
    ///
    /// @param oid Currently accessed OID.
    /// @param num_oids Number of OIDs in the database.
    /// @param locked Lock holder object for this thread.
    void MentionOid(int oid, int num_oids, CSeqDBLockHold & locked)
    {
        Lock(locked);
        m_Strategy.MentionOid(oid, num_oids);
    }

    /// Add a garbage collection callback.
    ///
    /// Some classes in the SeqDB library keep holds on regions of
    /// memory.  This may represent a large subset of the collectable
    /// memory, so in order for garbage collection to be effective,
    /// these holds should be detached at the beginning of a garbage
    /// collection run.  To accomplish this, each SeqDB object creates
    /// a callback functor that knows how to release its holds; it
    /// adds this functor to the atlas using the interface shown here.
    ///
    /// @param flusher A callback functor to flush held regions.
    /// @param flushp A pointer which is set when the flusher is added.
    /// @param locked The lock holder object for this thread.
    void AddRegionFlusher(CSeqDBFlushCB  * flusher,
                          CSeqDBFlushCB ** flushp,
                          CSeqDBLockHold & locked)
    {
        Lock(locked);
        m_Flushers.push_back(flusher);
        *flushp = flusher;
    }

    /// Remove a garbage collection callback.
    ///
    /// This removes a garbage collection callback from the list
    /// stored in the atlas.  It is called in preparation for
    /// destruction of a CSeqDBImpl object.
    ///
    /// @param flusher A callback functor to flush held regions.
    /// @param locked The lock holder object for this thread.
    void RemoveRegionFlusher(CSeqDBFlushCB * flusher, CSeqDBLockHold & locked)
    {
        Lock(locked);

        _ASSERT(m_Flushers.size());

        for(size_t i = 0; i < m_Flushers.size(); i++) {
            if (m_Flushers[i] == flusher) {
                m_Flushers[i] = m_Flushers.back();
                m_Flushers.pop_back();
                return;
            }
        }
        _ASSERT(0);
    }

    /// Set global default memory bound.
    ///
    /// This sets the default, global memory bound used for all SeqDB
    /// objects.  If zero is specified, an appropriate default will be
    /// selected based on system information.
    ///
    /// @param bytes Number of bytes to use as the global memory bound.
    static void SetDefaultMemoryBound(Uint8 bytes);

    /// Get BlastDB search path.
    const string GetSearchPath() const
    {
        return m_SearchPath;
    }

    /// Generate search path
    static const string GenerateSearchPath() {
        string splitter;
        string path;
#if defined(NCBI_OS_UNIX)
        splitter = ":";
#else
        splitter = ";";
#endif
        // Local directory first;
        path  = CDirEntry::NormalizePath(CDir::GetCwd(),eFollowLinks);
        path += splitter;
        // Then, BLASTDB;
        CNcbiEnvironment env;
        path += CDirEntry::NormalizePath(env.Get("BLASTDB"),eFollowLinks);
        path += splitter;
        // Finally, the config file.
        CNcbiApplication* app = CNcbiApplication::Instance();
        if (app) {
            const CNcbiRegistry& registry = app->GetConfig();
            if (registry.HasEntry("BLAST", "BLASTDB")) {
                path += CDirEntry::NormalizePath(registry.Get("BLAST", "BLASTDB"),eFollowLinks);
                path += splitter;
            }
        } 
        return path;
    }

    map< string, CMemoryFile* > &GetFilesMemMap(void){return m_FileMemMap;}
    
private:
    /// Private method to prevent copy construction.
    CSeqDBAtlas(const CSeqDBAtlas &);

    /// Iterator type for m_Pool member.
    typedef map<const char *, size_t>::iterator TPoolIter;

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

    

    /// Releases or deletes a memory region.
    ///
    /// The RetRegion method is called many times.  The functionality
    /// is split into a fast inlined part and a slow non-inlined part.
    /// This is the non-inlined part.  It searches for the region or
    /// allocated block that owns the specified address, and releases
    /// it, if found.  This method assumes the lock is held.
    ///
    /// @param datap
    ///   Pointer to the area to delete or release a hold on.
    void x_RetRegionNonRecent(const char * datap);
   
    /// Call all callbacks.
    ///
    /// This method calls all the GC callbacks in preparation for a
    /// garbage collection run or similar processing.  This method
    /// assumes the lock is held.
    void x_FlushAll()
    {
        for(size_t i = 0; i < m_Flushers.size(); i++) {
            (*m_Flushers[i])();
        }
    }

    // Data

    /// Protects most of the critical regions of the SeqDB library.
    CMutex m_Lock;

    /// Set to true if memory mapping is enabled.
    bool m_UseMmap; // not used anywhere except constructor

    /// Bytes of "data" currently known to SeqDBAtlas.  This does not
    /// include metadata, such as the region map class itself..
    TIndx m_CurAlloc;

    /// All of the SeqDB region maps.
    //vector<CRegionMap*> m_Regions;

    /// Maps from pointers to dynamically allocated blocks to the byte
    /// size of the allocation.
    map<const char *, size_t> m_Pool;

    /// The most recently assigned FID.
    int m_LastFID;

    /// Lookup table of fids by filename.
    map<string, int> m_FileIDs;

    

    /// Defines lookup of regions by fid, offset, and length.
    //typedef set<CRegionMap *, RegionMapLess> TNameOffsetTable;

    /// Defines lookup of regions by starting memory address.
    //typedef map<const char *, CRegionMap*>   TAddressTable;

    /// Map to find regions for Getting (x_FindRegion).
    //TNameOffsetTable m_NameOffsetLookup;

    /// Map to find regions for Returning (x_RetRegionNonRecent).
    //TAddressTable m_AddressLookup;

    // Recent region lookup

    /// Number of recently-used-region slots.
    //enum { eNumRecent = 8 };

    /// Table of recently used regions.
    //CRegionMap * m_Recent[ eNumRecent ];

    /// Atlas will try to garbage collect if more than this many
    /// regions are opened.
    int m_OpenRegionsTrigger; // not used anywhere except constructor

    /// Cache of file existence and length.
    map< string, pair<bool, TIndx> > m_FileSize;

    /// Maxium file size.
    Uint8 m_MaxFileSize;

    /// Callbacks to flush memory leases before a garbage collection.
    vector<CSeqDBFlushCB*> m_Flushers;

    /// Flexible memory allocation manager.
    CSeqDBMapStrategy m_Strategy;

    /// BlastDB search path.
    const string m_SearchPath;

    bool m_Alloc;//m_pool was used for mrmory allocation
    map< string, CMemoryFile* > m_FileMemMap;    
};



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

inline CSeqDBMemReg::~CSeqDBMemReg()
{
    m_Atlas.UnregisterExternal(*this);
}


/// Guard object for the SeqDBAtlas singleton.
///
/// The CSeqDBAtlas object is a singleton - only one exists at any
/// given time, and only if a CSeqDB object exists.  This object
/// implements that policy.  When no CSeqDBAtlas object exists, the
/// first CSeqDB object to be created will decide whether memory
/// mapping is enabled.  One of these objects exists in every
/// CSeqDBImpl and CSeqDBColumn object, and in the frames of a few
/// static functions.

class CSeqDBAtlasHolder {
public:
    /// Constructor.
    /// @param use_mmap If true, memory mapping will be used.
    /// @param flusher The garbage collection callback.
    /// @param locked The lock hold object for this thread (or NULL).
    CSeqDBAtlasHolder(bool             use_mmap,
                      CSeqDBFlushCB  * flusher,
                      CSeqDBLockHold * lockedp);

    /// Destructor.
    ~CSeqDBAtlasHolder();

    /// Get the CSeqDBAtlas object.
    CSeqDBAtlas & Get();

private:
    /// Garbage collection callback.
    CSeqDBFlushCB * m_FlushCB;

    /// Lock protecting this object's fields
    DECLARE_CLASS_STATIC_FAST_MUTEX(m_Lock);

    /// Count of users of the CSeqDBAtlas object.
    static int m_Count;

    /// The CSeqDBAtlas object itself.
    static CSeqDBAtlas * m_Atlas;
};


class CSeqDBFileMemMap {
public:

    typedef CNcbiStreamoff TIndx;
    /// Constructor
    ///
    /// Initializes a memory map object.
    ///
    /// @param filename
    ///   file to memory map    
    CSeqDBFileMemMap(class CSeqDBAtlas & atlas, const string filename)
        : m_Atlas(atlas),
          m_DataPtr (NULL),
          m_MappedFile( NULL),
          m_Mapped(false)
    {
        Init(filename);
    }

    CSeqDBFileMemMap(class CSeqDBAtlas & atlas)
        : m_Atlas(atlas),
          m_DataPtr (NULL),
          m_MappedFile( NULL),
          m_Mapped(false)
    {
        
    }

    /// Destructor    
    ~CSeqDBFileMemMap()
    {
        Clear();
    }

    /// Initializes a memory map object.
    ///
    /// @param filename
    ///   file to memory map    
    void Init(const string filename) {                
        if(!m_MappedFile || m_Filename != filename)
        {
            m_Filename = filename;
            Init();
        }
    }

    //m_Filename is set
    void Init(void) {                    
            map <string, CMemoryFile* > &fileMemMap = m_Atlas.GetFilesMemMap();
            if(fileMemMap.count(m_Filename) > 0) {        
                m_MappedFile = fileMemMap[m_Filename];
                //cerr << "********Exists in atlas CMemoryFile:" << m_Filename << endl;
            }
            else {
                try {
                    m_MappedFile = new CMemoryFile(m_Filename);
                    fileMemMap.insert(map<string, CMemoryFile * >::value_type(m_Filename,m_MappedFile));
                    m_Mapped = true;
                    //cerr << "********Map             CMemoryFile:" << m_Filename << endl;
                }
                catch(...) {                    
                     int openedFilesCount = fileMemMap.size();
                     NCBI_THROW(CSeqDBException,
                                eFileErr,
                                "Error memory mapping: " + m_Filename + ". Number of files opened: " + NStr::IntToString(openedFilesCount));                    
                }
            }
            //m_MappedFile.reset(new CMemoryFile(filename));
            m_DataPtr = (char *) (m_MappedFile->GetPtr());            
    }
    

    /// Clears the memory mapobject.
    ///    
    void Clear()
    {
        /*
        if(m_MappedFile && m_Mapped) { 
            map <string, CMemoryFile* > &fileMemMap = m_Atlas.GetFilesMemMap();
            if(fileMemMap.count(m_Filename) > 0) {        
                fileMemMap.erase(m_Filename); 
                m_MappedFile->Unmap();
                //cerr << "********Unmap CMemoryFile:" << m_Filename << endl;
                delete m_MappedFile;
                m_MappedFile = NULL;
                m_Mapped = false;
            }
        }
        */
    }

    bool IsMapped(){return m_Mapped;}

    //Not used
    int UnmapAllIndex(void)
    {
        map <string, CMemoryFile* > &fileMemMap = m_Atlas.GetFilesMemMap();
        int count = 0;
        for (map<string, CMemoryFile *>::iterator it=fileMemMap.begin(); it!=fileMemMap.end(); ++it) {
            string filename = it->first;
            if(NStr::Find(filename,".pin") != NPOS ||  NStr::Find(filename,".nin") != NPOS){                
                it->second->Unmap();
                //cerr << "********Cleaning:Unmap CMemoryFile:" << filename << endl;                
                delete it->second;
                fileMemMap.erase(filename);                 
                count++;    
            }
                   
        }
        return count;
    }
    

    /// Get a pointer to the specified offset.
    ///
    /// Given an offset (which is assumed to be available here), this
    /// method returns a pointer to the data at that offset.
    ///
    /// @param offset
    ///   The required offset relative to the start of the file.
    /// @return
    ///   A pointer to the data at the requested location.       
    const char *GetFileDataPtr(const string   & fname,TIndx offset)                    
    {
        if(!m_MappedFile || m_Filename != fname) {
            Init(fname);                        
        }

        return(const char *)(m_DataPtr + offset);    
    }

    const char *GetFileDataPtr(TIndx offset)                    
    {
        return(const char *)(m_DataPtr + offset);    
    }

private:
    CSeqDBAtlas & m_Atlas;    
     /// Points to the beginning of the data area.
    const char * m_DataPtr;

    string m_Filename;

    bool m_Mapped;

    CMemoryFile *m_MappedFile;
};



END_NCBI_SCOPE

#endif // OBJTOOLS_READERS_SEQDB__SEQDBATLAS_HPP

