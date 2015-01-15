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

/// @file seqdbatlas.cpp
/// Implementation for the CSeqDBAtlas class and several related
/// classes, which provide control of a set of memory mappings.

#ifndef SKIP_DOXYGEN_PROCESSING
static char const rcsid[] = "$Id$";
#endif /* SKIP_DOXYGEN_PROCESSING */

#include <ncbi_pch.hpp>

#include <objtools/blast/seqdb_reader/impl/seqdbatlas.hpp>
#include <objtools/blast/seqdb_reader/impl/seqdbgeneral.hpp>
#include <memory>
#include <algorithm>
#include <objtools/blast/seqdb_reader/seqdbcommon.hpp>

#include <corelib/ncbi_system.hpp>

#if defined(NCBI_OS_UNIX)
#include <unistd.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/resource.h>
#include <unistd.h>
#endif

BEGIN_NCBI_SCOPE

#ifdef SEQDB_TRACE_LOGFILE

// By default, the first 16 trace classes are enabled

ofstream * seqdb_logfile  = 0;
int        seqdb_logclass = 0xFFFF;

void seqdb_log(const char * s)
{
    seqdb_log(1, s);
}

void seqdb_log(const char * s1, const string & s2)
{
    seqdb_log(1, s1, s2);
}

inline bool seqdb_log_disabled(int cl)
{
    return ! (seqdb_logfile && (cl & seqdb_logclass));
}

void seqdb_log(int cl, const char * s)
{
    if (seqdb_log_disabled(cl))
        return;

    (*seqdb_logfile) << s << endl;
}

void seqdb_log(int cl, const char * s1, const string & s2)
{
    if (seqdb_log_disabled(cl))
        return;

    (*seqdb_logfile) << s1 << s2 << endl;
}

void seqdb_log(int cl, const char * s1, int s2)
{
    if (seqdb_log_disabled(cl))
        return;

    (*seqdb_logfile) << s1 << s2 << endl;
}

void seqdb_log(int cl, const char * s1, int s2, const char * s3)
{
    if (seqdb_log_disabled(cl))
        return;

    (*seqdb_logfile) << s1 << s2 << s3 << endl;
}

void seqdb_log(int cl, const char * s1, int s2, const char * s3, int s4)
{
    if (seqdb_log_disabled(cl))
        return;

    (*seqdb_logfile) << s1 << s2 << s3 << s4 << endl;
}

void seqdb_log(int cl, const char * s1, int s2, const char * s3, int s4, const char * s5, int s6)
{
    if (seqdb_log_disabled(cl))
        return;

    (*seqdb_logfile) << s1 << s2 << s3 << s4 << s5 << s6 << endl;
}
#endif // SEQDB_TRACE_LOGFILE


// Further optimizations:

// 1. Regions could be stored in a map<>, sorted by file, then offset.
// This would allow a binary search instead of sequential and would
// vastly improve the "bad case" of 100_000s of buffers of file data.

// 2. "Scrounging" could be done in the file case.  It is bad to read
// 0-4096 then 4096 to 8192, then 4000-4220.  The third could use the
// first two to avoid reading.  It should either combine the first two
// regions into a new region, or else just copy to a new region and
// leave the old ones alone (possibly marking the old regions as high
// penalty).  Depending on refcnt, penalty, and region sizes.

// Throw function

void SeqDB_ThrowException(CSeqDBException::EErrCode code, const string & msg)
{
    switch(code) {
    case CSeqDBException::eArgErr:
        NCBI_THROW(CSeqDBException, eArgErr, msg);

    case CSeqDBException::eFileErr:
        NCBI_THROW(CSeqDBException, eFileErr, msg);

    default:
        NCBI_THROW(CSeqDBException, eMemErr, msg);
    }
}

/// Build and throw a file-not-found exception.
///
/// @param fname The name of the unfound file. [in]

static void s_SeqDB_FileNotFound(const string & fname)
{
    string msg("File [");
    msg += fname;
    msg += "] not found.";
    SeqDB_ThrowException(CSeqDBException::eFileErr, msg);
}


/// Check the size of a number relative to the scope of a numeric type.

template<class TIn, class TOut>
TOut SeqDB_CheckLength(TIn value)
{
    TOut result = TOut(value);

    if (sizeof(TOut) < sizeof(TIn)) {
        if (TIn(result) != value) {
            SeqDB_ThrowException(CSeqDBException::eFileErr,
                                 "Offset type does not span file length.");
        }
    }

    return result;
}

CSeqDBAtlas::CSeqDBAtlas(bool use_mmap)
    : m_UseMmap           (use_mmap),
      m_CurAlloc          (0),
      m_LastFID           (0),
      m_OpenRegionsTrigger(CSeqDBMapStrategy::eOpenRegionsWindow),
      m_MaxFileSize       (0),
      m_Strategy          (*this),
      m_SearchPath        (GenerateSearchPath())
{
    for(int i = 0; i < eNumRecent; i++) {
        m_Recent[i] = 0;
    }
    Verify(true);
}

CSeqDBAtlas::~CSeqDBAtlas()
{
    Verify(true);
    x_GarbageCollect(0);

    // Clear mapped file regions

    if ((! m_Regions.empty()) || (m_CurAlloc != 0)) {
        if (! m_Regions.empty()) {
            ShowLayout(true, 0);
        }

        _ASSERT(m_Regions.empty());
        _ASSERT(m_CurAlloc == 0);
    }

    // For now, and maybe permanently, enforce balance.

    _ASSERT(m_Pool.size() == 0);

    // Erase 'manually allocated' elements - In debug mode, this will
    // not execute, because of the above test.

    for(TPoolIter i = m_Pool.begin(); i != m_Pool.end(); i++) {
        delete[] (char*)((*i).first);
    }

    m_Pool.clear();
}

bool CSeqDBAtlas::DoesFileExist(const string & fname, CSeqDBLockHold & locked)
{
    Verify(locked);
    TIndx length(0);
    return GetFileSize(fname, length, locked);
}

const char * CSeqDBAtlas::GetFile(const string      & fname,
                                  TIndx             & length,
                                  CSeqDBLockHold    & locked)
{
    Verify(locked);
    if (! GetFileSize(fname, length, locked)) {
        s_SeqDB_FileNotFound(fname);
    }

    // If allocating more than 256MB in a file, do a full sweep first.
    // This technique may help prevent unnecessary fragmentation.
    // How?  Well, it's kind of a fudge, really: before allocating
    // anything really big, we want to clean memory as much as
    // possible.
    //
    // Essentially, big objects can fail due to fragmentation, even if
    // we are well below the memory bound.  So if there is a big spot
    // where this new allocation may fit, we want to remove any small
    // objects from it first.  When I mention fragmentation here, I
    // mean "you have 1.3 GB of address space left, but no piece
    // bigger than .75 GB".  Memory mapping is sensitive to this
    // because it needs huge contiguous chunks of sizes that are not
    // aligned.
    //
    // It should be mentioned that this will not (greatly) affect
    // users who are using the round-to-chunk-size allocator.

    if (TIndx(length) > m_Strategy.GetGCTriggerSize()) {
        Lock(locked);
        x_GarbageCollect(0);
    }

    return GetRegion(fname, 0, length, locked);
}

void CSeqDBAtlas::GetFile(CSeqDBMemLease & lease,
                          const string   & fname,
                          TIndx          & length,
                          CSeqDBLockHold & locked)
{
    if (! GetFileSize(fname, length, locked)) {
        s_SeqDB_FileNotFound(fname);
    }

    // If allocating more than 256MB in a file, do a full sweep first.
    // This technique may help prevent unnecessary fragmentation.
    // How?  Well, it's kind of a fudge, really: before allocating
    // anythin humongous, we want to clean memory as much as possible.
    //
    // Essentially, big objects can fail due to fragmentation, even if
    // we are well below the memory bound.  So if there is a big spot
    // where this new allocation may fit, we want to remove any small
    // objects from it first.  When I mention fragmentation here, I
    // mean "you have 1.3 GB of address space left, but no piece
    // bigger than .75 GB".  Memory mapping is sensitive to this
    // because it needs huge contiguous chunks of sizes that are not
    // aligned.
    //
    // It should be mentioned that this will not (greatly) affect
    // users who are using the round-to-chunk-size allocator.
    //
    // Also, for systems with (e.g.) 64 bit address spaces, this
    // could/should be relaxed, to require less mapping.

    if (length > m_Strategy.GetGCTriggerSize()) {
        GarbageCollect(locked);
    }

    Lock(locked);
    Verify(true);

    GetRegion(lease, fname, 0, length);
}

bool CSeqDBAtlas::GetFileSize(const string   & fname,
                              TIndx          & length,
                              CSeqDBLockHold & locked)
{
    Lock(locked);
    Verify(true);

    return GetFileSizeL(fname, length);
}

bool CSeqDBAtlas::GetFileSizeL(const string & fname,
                               TIndx        & length)
{
    Verify(true);
    // Fields: file-exists, file-length
    pair<bool, TIndx> data;

    map< string, pair<bool, TIndx> >::iterator i =
        m_FileSize.find(fname);

    if (i == m_FileSize.end()) {
        CFile whole(fname);
        Int8 file_length = whole.GetLength();

        if (file_length >= 0) {
            data.first  = true;
            data.second = SeqDB_CheckLength<Int8,TIndx>(file_length);
            if ((Uint8)file_length > m_MaxFileSize) m_MaxFileSize = file_length;
        } else {
            data.first  = false;
            data.second = 0;
        }

        m_FileSize[fname] = data;
    } else {
        data = (*i).second;
    }
    Verify(true);

    length = data.second;
    return data.first;
}

void CSeqDBAtlas::GarbageCollect(CSeqDBLockHold & locked)
{
    Lock(locked);
    x_GarbageCollect(0);
}

void CSeqDBAtlas::x_GarbageCollect(Uint8 reduce_to)
{
    Verify(true);
    if (Uint8(m_CurAlloc) <= reduce_to) {
        return;
    }

    x_FlushAll();

    x_ClearRecent();

    int max_distinct_clock = 10;

    int  num_gcs  = 1;

    if (reduce_to > 0) {
        TIndx in_use = m_CurAlloc;

        for(unsigned i = 0; i < m_Regions.size(); i++) {
            CRegionMap * mr = m_Regions[i];

            if (! mr->InUse()) {
                mr->BumpClock();
                in_use -= mr->Length();
            }

            num_gcs = ((num_gcs > mr->GetClock())
                       ? num_gcs
                       : mr->GetClock()); // max
        }

        num_gcs = 1 + ((num_gcs < max_distinct_clock)
                       ? num_gcs
                       : max_distinct_clock); //min
    }

    Verify(true);
    while(num_gcs >= 0) {
        num_gcs --;

        size_t i = 0;

        while(i < m_Regions.size()) {
            CRegionMap * mr = m_Regions[i];

            if (mr->InUse() || mr->GetClock() < num_gcs) {
                i++;
                continue;
            }

            size_t last = m_Regions.size() - 1;

            if (i != last) {
                m_Regions[i] = m_Regions[last];
            }

            m_Regions.pop_back();

            m_CurAlloc -= mr->Length();

            m_NameOffsetLookup.erase(mr);
            m_AddressLookup.erase(mr->Data());

            delete mr;

            if (Uint8(m_CurAlloc) < reduce_to) {
                return;
            }
        }
    }
    Verify(true);
}


// Algorithm:
//
// In file mode, get exactly what we need.  Otherwise, if the request
// fits entirely into one slice, use large slice rounding (get large
// pieces).  If it is on a large slice boundary, use small slice
// rounding (get small pieces).


// Rationale:
//
// We would like to map all data using the same slice size, and in
// practice, 99% of the objects *will* fall entirely within the large
// slice boundaries.  But for cases where they do not, we have several
// strategies.
//
// The simplest is to round both boundaries out to large slices,
// making a double-wide slice.  The problem with this is that in a
// straight-through traversal, every large slice will be mapped twice,
// because each overlap case will cause a mapping of one already
// mapped large slice (the most recent one) and one not-yet mapped
// slice (the one after it).
//
// Instead, I am using two strategies to avoid this phenomena.  The
// first is the use of one large slice size and one small slice size.
// Boundary cases (including any object not fitting in one slice) will
// be rounded out to the smaller slice size.  This solves the problem
// of the boundary case: the small allocations will only cause a
// minimal "duplication" of the mapped area.
//
// The memory layout pattern due to the previous technique will
// alternate between short and long slices, with each short slice
// echoing the end and beginning of the slices before and after it,
// respectively.  The short slices represent redundant mapping.  To
// prevent the proliferation of short slices, there is a second
// technique to preferentially remove "irregular" sequences.
//
// Each mapping also gets a "penalty" value: the value is added to the
// "clock" value when considering mappings for garbage collection.
// The fragmentation is least when all the sequences are the same
// size.  The penalty value is 0 for mappings of size "slice", 1 for
// mappings of size "small slice" or "small slice * 2", and 2 for
// mappings that do not correspond to any slice size, i.e. the "tail"
// portion of a file, or whole mappings of short files. (KMB)

void CRegionMap::x_Roundup(TIndx       & begin,
                           TIndx       & end,
                           int         & penalty,
                           TIndx         file_size,
                           bool          use_mmap,
                           CSeqDBAtlas * atlas)
{
    // These should be made available to some kind of interface to
    // allow memory-usage tuning.

    const TIndx block_size  = 1024 * 512;
    TIndx large_slice = (size_t)atlas->GetSliceSize();
    TIndx overhang    = (size_t)atlas->GetOverhang();
    TIndx small_slice = (size_t)large_slice / 16;

    if (small_slice < block_size) {
        small_slice = block_size;
    }

    if (large_slice < small_slice) {
        large_slice = small_slice * 16;
    }

    _ASSERT(begin <  end);
    SEQDB_FILE_ASSERT(end <= file_size);

    penalty = 0;

    TIndx align = 1;

    if (use_mmap) {
        TIndx page_b = begin / large_slice;
        TIndx page_e = end   / large_slice;

        if (page_b == page_e) {
            align = large_slice;
            penalty = 0;
        } else {
            if ((end-begin) < (small_slice * 2)) {
                penalty = 1;
            } else {
                penalty = 2;
            }

            align = small_slice;
        }
    } else {
        // File mode, align to block.  (This only helps if there are
        // other sequences completely included in the same blocks that
        // interest us.)

        penalty = 2;
        align = block_size;

        // Also, do not use overhang logic, because the overhang size
        // is tuned for the memory mapping case.

        overhang = 0;
    }

    if (align > 1) {
        // Integer math can do the rounding.

        TIndx new_begin = (begin / align) * align;
        TIndx new_end = ((end + align - 1) / align) * align + overhang;

        // If there is less than a third of a slice left, grab it all.
        if ((new_end + (align/3)) > file_size) {
            new_end = file_size;
            penalty = 2;
        }

        _ASSERT(new_begin <= begin);
        _ASSERT(new_end   >= end  );

        begin = new_begin;
        end   = new_end;
    }

    // Should be true on all architectures now, due to the CMemoryFile
    // map/segment work.

    bool have_range_mmap = true;

    if (! have_range_mmap) {
        begin = 0;
        end   = file_size;

        // This code biases larger items to last longer, ie to garbage
        // collect irregular or short items first.  This is basically
        // an "intuitive" decision on my part, intended to improve
        // memory layout (i.e. to decrease the occurrence of internal
        // memory fragmentation), which in most cases can be seen as
        // fragmentation of large areas by small elements.  [Note that
        // the effectiveness of this technique on reducing internal
        // fragmentation has not been measured.]

        // In theory, no memory management strategy that provides
        // different sized blocks can guarantee a reasonable upper
        // bound on memory exhaustion from internal fragmentation.
        // (An exception would be systems with memory compaction,
        // which is normally considered infeasible for languages like
        // C.)  Also, These concerns probably have little significance
        // on a 64 bit memory architecture, since the access patterns
        // that cause internal fragmentation involve alternating maps
        // and unmaps, and a 64 bit system will normally not need to
        // unmap files.

        penalty = ((file_size > (large_slice+overhang))
                   ? 0
                   : 1);
    }
}

const char * CSeqDBAtlas::x_FindRegion(int           fid,
                                       TIndx       & begin,
                                       TIndx       & end,
                                       const char ** start,
                                       CRegionMap ** region)
{
    Verify(true);

    // Try recent matches first.

    for(int i = 0; i<eNumRecent; i++) {
        if (! m_Recent[i])
            break;

        const char * retval = m_Recent[i]->MatchAndUse(fid, begin, end, start);

        if (retval) {
            // Moves region to top
            if (region) {
                *region = m_Recent[i];
            }

            if (i) {
                x_AddRecent(m_Recent[i]);
            }

            _ASSERT(*start);
            return retval;
        }
    }

    if (m_NameOffsetLookup.empty()) {
        return 0;
    }

    // Start key - will be used to find the least element that is NOT
    // good enough.  We want the elements before this to have

    CRegionMap key(0, fid, begin, end);

    TNameOffsetTable::iterator iter
        = m_NameOffsetLookup.upper_bound(& key);

    while(iter != m_NameOffsetLookup.begin()) {
        --iter;

        if ((*iter)->Fid() != fid)
            return 0;

        CRegionMap * rmap = *iter;

        // Should be guaranteed by the ordering we are using.
        _ASSERT(rmap->Begin() <= begin);

        if (rmap->End() >= end) {
            const char * retval = rmap->MatchAndUse(fid, begin, end, start);
            _ASSERT(retval);
            _ASSERT(*start);

            if (region) {
                *region = rmap;
            }

            x_AddRecent(rmap);
            return retval;
        }
    }
    Verify(true);

    return 0;
}

// Assumes locked.

void CSeqDBAtlas::PossiblyGarbageCollect(Uint8 space_needed, bool returning)
{
    Verify(true);

    if ((int) m_Regions.size() >= m_OpenRegionsTrigger) {
        // If we are collecting because of the number of open regions,
        // we use zero as the size.  This kind of flush is probably
        // due to extensive alias node forests, and it is better to
        // clear cut than try to take only old-growth.  Alias files
        // are only read once, so caching would only help if there is
        // overlap between subtrees.

        // The mechanism uses three numbers.  MAX is a constant
        // expressing the most regions you ever want to have open.
        // WINDOW is a constant expressing the number of regions you
        // want to be able to open without a garbage collection being
        // triggered.  TRIGGER is the number of regions creations that
        // will trigger the next full garbage collection.

        // 1. In the constructor, TRIGGER is set to WINDOW.
        //
        // 2. If the number of regions exceeds TRIGGER, a full
        //    collection is done.
        //
        // 3. After each full collection, TRIGGER is set to the lesser
        //    of the number of surviving regions plus WINDOW, or MAX.

        x_GarbageCollect(0);

        int window = CSeqDBMapStrategy::eOpenRegionsWindow;
        int maxopen = CSeqDBMapStrategy::eMaxOpenRegions;

        m_OpenRegionsTrigger = min(int(m_Regions.size() + window), maxopen);
    } else {
        // Use Int8 to avoid "unsigned rollunder"

        Int8 bound = m_Strategy.GetMemoryBound(returning);
        Int8 capacity_left = bound - m_CurAlloc;

        if (Int8(space_needed) > capacity_left) {
            x_GarbageCollect(bound - space_needed);
        }
    }

    Verify(true);
}

/// Simple idiom for RIIA with malloc + free.
struct CSeqDBAutoFree {
    /// Constructor.
    CSeqDBAutoFree()
        : m_Array(0)
    {
    }

    /// Specify a malloced area of memory.
    void Set(const char * x)
    {
        m_Array = x;
    }

    /// Destructor will free that memory.
    ~CSeqDBAutoFree()
    {
        if (m_Array) {
            free((void*) m_Array);
        }
    }

private:
    /// Pointer to malloced memory.
    const char * m_Array;
};

const char *
CSeqDBAtlas::x_GetRegion(const string   & fname,
                         TIndx          & begin,
                         TIndx          & end,
                         const char    ** start,
                         CRegionMap    ** rmap)
{
    _ASSERT(fname.size());

    Verify(true);

    const char * dummy = 0;

    if (start == 0) {
        start = & dummy;
    }

    _ASSERT(end > begin);

    const string * strp = 0;

    int fid = x_LookupFile(fname, & strp);

    const char * retval = 0;

    if ((retval = x_FindRegion(fid, begin, end, start, rmap))) {
        _ASSERT(*start);
        return retval;
    }

    // Need to add the range, so GC first.

    PossiblyGarbageCollect(end - begin, false);

    CRegionMap * nregion = 0;

    try {
        nregion = new CRegionMap(strp, fid, begin, end);

        // new() should have thrown, but some old implementations are
        // said to be non-compliant in this regard:

        if (! nregion) {
            throw std::bad_alloc();
        }

        auto_ptr<CRegionMap> newmap(nregion);

        if (rmap)
            *rmap = nregion;

        if (m_UseMmap) {
            for(int fails = 0; fails < 2; fails++) {
                CSeqDBAutoFree shim;

                bool worked = true;

                try {
                    // On Linux, allocate 10 MB of (non-initialized!)
                    // memory.  The goal is to cause memory shortages
                    // to happen in SeqDB rather than in client code,
                    // so that SeqDB can adjust its parameters to free
                    // up memory.  This reduces errors and improves
                    // performance on Linux, but not on other O/Ses;
                    // See also comments in SeqDBMapStrategy.

                    const char * a = 0;

                    if (CSeqDBMapStrategy::e_ProbeMemory) {
                        a = (const char*) malloc(10 << 20);

                        if (! a) {
                            worked = false;
                        }

                        shim.Set(a);
                    }
                }
                catch(...) {
                    // allocation failure; reduce
                    worked = false;
                }

                if (worked) {
                    if (newmap->MapMmap(this)) {
                        retval = newmap->Data(begin, end);
                        newmap->AddRef();

                        if (retval == 0) {
                            worked = false;
                        }
                    } else {
                        worked = false;
                    }
                }

                if (worked) {
                    break;
                }

                // If there was a map (or new[]) failure, tell the
                // strategy module and wipe out half of the allocated
                // memory bound.

                m_Strategy.MentionMapFailure(m_CurAlloc);
                x_GarbageCollect(m_CurAlloc/2);
            }
        }

        // mmap() sometimes fails for no discernable reason.. if it
        // does, try reading the file.

        if (retval == 0 && newmap->MapFile(this)) {
            retval = newmap->Data(begin, end);
            newmap->AddRef();
        }

        m_NameOffsetLookup.insert(nregion);

        newmap->GetBoundaries(start, begin, end);

        if (retval == 0) {
            s_SeqDB_FileNotFound(fname);
        }

        m_AddressLookup[nregion->Data()] = nregion;

        m_CurAlloc += (end-begin);

        CRegionMap * nmp = newmap.release();

        _ASSERT(nmp);

        m_Regions.push_back(nmp);
    }
    catch(std::bad_alloc) {
        if (m_NameOffsetLookup.find(nregion) != m_NameOffsetLookup.end()) {
            m_NameOffsetLookup.erase(nregion);
        }

        SeqDB_ThrowException(CSeqDBException::eMemErr,
                             "CSeqDBAtlas::x_GetRegion: allocation failed.");
    }

    // Collect down to 'retbound' amount.
    PossiblyGarbageCollect(0, true);

    Verify(true);

    return retval;
}

const char * CSeqDBAtlas::GetRegion(const string   & fname,
                                    TIndx            begin,
                                    TIndx            end,
                                    CSeqDBLockHold & locked)
{
    Lock(locked);
    Verify(true);

    return x_GetRegion(fname, begin, end, 0, 0);
}

// Assumes lock is held.
void CSeqDBAtlas::GetRegion(CSeqDBMemLease & lease,
                            const string   & fname,
                            TIndx            begin,
                            TIndx            end)
{
    Verify(true);
    RetRegion(lease);

    const char * start(0);
    CRegionMap * rmap(0);

    const char * result = x_GetRegion(fname, begin, end, & start, & rmap);

    if (result) {
#ifdef _DEBUG
        if (! (start)) {
            cout << "fname [" << fname << "] begin " << begin << " end " << end
                 << " start " << size_t(start) << " result " << size_t(result)
                 << " rmap " << static_cast<void*>(rmap) << endl;
        }
#endif
        _ASSERT(start);

        lease.x_SetRegion(begin, end, start, rmap);
    }
    Verify(true);
}

// Assumes lock is held

/// Releases a hold on a partial mapping of the file.
void CSeqDBAtlas::RetRegion(CSeqDBMemLease & ml)
{
    Verify(true);
    if (ml.m_Data) {
#ifdef _DEBUG
        const char * datap = ml.m_Data;
	if (! ml.m_RMap) {
            cout << "m_RMap is null" << endl;
	}
	if (! ml.m_RMap->InRange(datap)) {
            cout << "datap not in range; datap  = " << ((size_t)(datap)) << endl;
            cout << "datap not in range; m.data = " << ((size_t)(ml.m_RMap->Data())) << endl;
            cout << "datap not in range; begin  = " << ((size_t)(ml.m_RMap->Data() + ml.m_RMap->Begin())) << endl;
            cout << "datap not in range; begin  = " << ((size_t)(ml.m_RMap->Data() + ml.m_RMap->End())) << endl;
	}
#endif
        _ASSERT(ml.m_RMap);
        _ASSERT(ml.m_RMap->InRange(datap));

        ml.m_RMap->RetRef();

        ml.m_Data  = 0;
        ml.m_Begin = 0;
        ml.m_End   = 0;
    }
    Verify(true);
}

/// Releases a hold on a partial mapping of the file.
void CSeqDBAtlas::x_RetRegionNonRecent(const char * datap)
{
    Verify(true);
    CSeqDBAtlas::TAddressTable::iterator iter = m_AddressLookup.upper_bound(datap);

    if (iter != m_AddressLookup.begin()) {
        --iter;

        CRegionMap * rmap = (*iter).second;

        if (rmap->InRange(datap)) {
            x_AddRecent(rmap);
            rmap->RetRef();
            return;
        }
    }

    bool worked = x_Free(datap);
    _ASSERT(worked);

    if (! worked) {
        cerr << "Address leak in CSeqDBAtlas::RetRegion" << endl;
    }
    Verify(true);
}

void CSeqDBAtlas::ShowLayout(bool locked, TIndx index)
{
    // This odd looking construction is for debugging existing
    // binaries with the help of a debugger.  By setting the static
    // value (in the debugger) the user can override the default
    // behavior (whichever is compiled in).

    static int enabled = 0;

#ifdef _DEBUG
#ifdef VERBOSE
    if (enabled == 0) {
        enabled = 1;
    }
#endif
#endif

    if (enabled == 1) {
        if (! locked) {
            m_Lock.Lock();
        }

        // MSVC cannot use Uint8 here... as in "ostream << [Uint8]".

        cerr << "\n\nShowing layout (index " << NStr::UInt8ToString((Uint8)index)
             << "), current alloc = " << m_CurAlloc << endl;

        for(unsigned i = 0; i < m_Regions.size(); i++) {
            m_Regions[i]->Show();
        }

        cerr << "\n\n" << endl;

        if (! locked) {
            m_Lock.Unlock();
        }
    }
}

// This does not attempt to garbage collect, but it will influence
// garbage collection if it is used enough.

char * CSeqDBAtlas::Alloc(size_t length, CSeqDBLockHold & locked, bool clear)
{
    // What should/will happen on allocation failure?

    Lock(locked);

    if (! length) {
        length = 1;
    }

    // Allocate/clear

    char * newcp = 0;

    try {
        newcp = new char[length];

        // new() should have thrown, but some old implementations are
        // said to be non-compliant in this regard:

        if (! newcp) {
            throw std::bad_alloc();
        }

        if (clear) {
            memset(newcp, 0, length);
        }
    }
    catch(std::bad_alloc) {
        NCBI_THROW(CSeqDBException, eMemErr,
                   "CSeqDBAtlas::Alloc: allocation failed.");
    }

    // Add to pool.

    _ASSERT(m_Pool.find(newcp) == m_Pool.end());

    m_Pool[newcp] = length;
    m_CurAlloc += length;

    return newcp;
}


void CSeqDBAtlas::Free(const char * freeme, CSeqDBLockHold & locked)
{
    Lock(locked);

#ifdef _DEBUG
    bool found =
        x_Free(freeme);

    _ASSERT(found);
#else
    x_Free(freeme);
#endif
}


bool CSeqDBAtlas::x_Free(const char * freeme)
{
    TPoolIter i = m_Pool.find((const char*) freeme);

    if (i == m_Pool.end()) {
        return false;
    }

    size_t sz = (*i).second;

    _ASSERT(m_CurAlloc >= (TIndx)sz);
    m_CurAlloc -= sz;

    char * cp = (char*) freeme;
    delete[] cp;
    m_Pool.erase(i);

    return true;
}

// Assumes lock is held.

void CRegionMap::Show()
{
    CHECK_MARKER();
    // This odd looking construction is for debugging existing
    // binaries with the help of a debugger.  By setting the static
    // value (in the debugger) the user can override the default
    // behavior (whichever is compiled in).

    static int enabled = 0;

#ifdef _DEBUG
#ifdef VERBOSE
    if (enabled == 0) {
        enabled = 1;
    }
#endif
#endif

    if (enabled == 1) {
        cout << " [" << static_cast<const void*>(m_Data) << "]-["
             << static_cast<const void*>(m_Data + m_End - m_Begin) << "]: "
             << *m_Fname << ", ref=" << m_Ref << " size=" << (m_End - m_Begin) << endl;
    }
}

CRegionMap::CRegionMap(const string * fname, int fid, TIndx begin, TIndx end)
    : m_Data     (0),
      m_MemFile  (0),
      m_Fname    (fname),
      m_Begin    (begin),
      m_End      (end),
      m_Fid      (fid),
      m_Ref      (0),
      m_Clock    (0),
      m_Penalty  (0)
{
    INIT_CLASS_MARK();
    CHECK_MARKER();
}

CRegionMap::~CRegionMap()
{
    CHECK_MARKER();

    if (m_MemFile) {
        delete m_MemFile;
        m_MemFile = 0;
        m_Data    = 0;
    }
    if (m_Data) {
        delete[] ((char*) m_Data);
        m_Data = 0;
    }
    BREAK_MARKER();
}

bool CRegionMap::MapMmap(CSeqDBAtlas * atlas)
{
    CHECK_MARKER();
    bool rv = false;

    TIndx flength(0);
    bool file_exists = atlas->GetFileSizeL(*m_Fname, flength);

    if (file_exists) {
        string expt;

        try {
            m_MemFile = new CMemoryFileMap(*m_Fname,
                                           CMemoryFileMap::eMMP_Read,
                                           CMemoryFileMap::eMMS_Private);

            // new() should have thrown, but some old implementations are
            // said to be non-compliant in this regard:

            if (! m_MemFile) {
                throw std::bad_alloc();
            }

            if ((m_Begin != 0) || (m_End != flength)) {
                x_Roundup(m_Begin, m_End, m_Penalty, flength, true, atlas);
                atlas->PossiblyGarbageCollect(m_End - m_Begin, false);
            }

            m_Data = (const char*) m_MemFile->Map(m_Begin, m_End - m_Begin);
            string last2 = m_Fname->substr(m_Fname->length() - 2);
            if (last2 == "in"  ||  last2 == "sq") {
                MemoryAdvise(
                        (void*) m_Data,
                        (unsigned long) (m_End - m_Begin),
                        eMADV_Sequential
                );
            }
        }
        catch(std::bad_alloc&) {
            expt = "\nstd::bad_alloc.";
        }
        catch(CException & e) {
            // Make sure the string is not empty.
            expt = string("\n") + e.ReportAll();
        }
        catch(...) {
            throw;
        }

        if (expt.length()) {
            // For now, if I can't memory map the file, I'll revert to
            // the old way: malloc a chunk of core and copy the data
            // into it.

            if (expt.find(": Cannot allocate memory") == expt.npos) {
                expt = string("CSeqDBAtlas::MapMmap: While mapping file [") + (*m_Fname) + "] with " +
                    NStr::UInt8ToString(atlas->GetCurrentAllocationTotal()) +
                    " bytes allocated, caught exception:" + expt;

                SeqDB_ThrowException(CSeqDBException::eFileErr, expt);
            }
        }

        if (m_Data) {
            rv = true;
        } else {
            delete m_MemFile;
            m_MemFile = 0;
        }
    }

    return rv;
}

bool CRegionMap::MapFile(CSeqDBAtlas * atlas)
{
    CHECK_MARKER();

    // Okay, rethink:

    // 1. Unlike mmap, the file state disappears, the only state here
    // will be the data itself.  The stream is not kept because we
    // only need to read the data into a buffer, then the stream goes
    // away.

    // Steps:

    // 1. round up slice.
    // 2. open file, seek, and read.
    //    a. if read fails, delete section, return failure, and quit.
    //    a. if read works, return true.

    // Find file and get length

    CFile file(*m_Fname);
	CNcbiIfstream istr(m_Fname->c_str(), IOS_BASE::binary | IOS_BASE::in);

    if ((! file.Exists()) || istr.fail()) {
        return false;
    }

    // Round up slice size -- this should be a win for sequences if
    // they are read sequentially, and are smaller than one block
    // (16,000 base pairs).  This is a safe bet.  We are trading
    // memory bandwidth for disk bandwidth.  A more intelligent
    // algorithm might heuristically determine whether such an
    // optimization is working, or even switch back and forth and
    // measure performance.

    // For now, this code has to be rock solid, so I am avoiding
    // anything like cleverness.  The assumptions are:

    // (1) That reading whole blocks (and therefore avoiding multiple
    // IOs per block) saves more time than is lost in read(2) when
    // copying the (potentially) unused parts of the block.
    //
    // (2) That it is better for memory performance to store
    // whole-blocks in memory than to store 'trimmed' sequences.

    // If you think this is not true, consider the case of the index
    // file, where we would be storing regions consisting of only 4 or
    // 8 bytes.

    x_Roundup(m_Begin,
              m_End,
              m_Penalty,
              SeqDB_CheckLength<Uint8,TIndx>(file.GetLength()),
              false,
              atlas);

    atlas->PossiblyGarbageCollect(m_End - m_Begin, false);

    istr.seekg(m_Begin);

    Uint8 rdsize8 = m_End - m_Begin;
    _ASSERT((TIndx(rdsize8) & TIndx(-1)) == TIndx(rdsize8));

    TIndx rdsize = (TIndx) rdsize8;

    char * newbuf = 0;

    bool throw_afe = false;

    try {
        newbuf = new char[rdsize];

        // new() should have thrown, but some old implementations are
        // said to be non-compliant in this regard:

        if (! newbuf) {
            CHECK_MARKER();
            throw std::bad_alloc();
        }
    }
    catch(std::bad_alloc) {
        throw_afe = true;
    }

    if (throw_afe) {
        CHECK_MARKER();

        string msg("CSeqDBAtlas::MapFile: allocation failed for ");
        msg += NStr::UInt8ToString(rdsize);
        msg += " bytes.";

        NCBI_THROW(CSeqDBException, eMemErr, msg);
    }

    TIndx amt_read = 0;

    while((amt_read < rdsize) && istr) {
        istr.read(newbuf + amt_read, rdsize - amt_read);

        size_t count = istr.gcount();
        if (! count) {
            delete[] newbuf;
            return false;
        }

        amt_read += count;
    }

    m_Data = newbuf;

    return (amt_read == rdsize);
}

const char * CRegionMap::Data(TIndx begin, TIndx end)
{
    CHECK_MARKER();
    _ASSERT(m_Data != 0);
    _ASSERT(begin  >= m_Begin);

    // Avoid solaris warning.
    if (! (end <= m_End)) {
        _ASSERT(end <= m_End);
    }

    return m_Data + begin - m_Begin;
}

int CSeqDBAtlas::x_LookupFile(const string  & fname,
                              const string ** map_fname_ptr)
{
    Verify(true);
    map<string, int>::iterator i = m_FileIDs.find(fname);

    if (i == m_FileIDs.end()) {
        m_FileIDs[fname] = ++ m_LastFID;

        i = m_FileIDs.find(fname);
    }

    // Get address of string in string->fid table.

    *map_fname_ptr = & (*i).first;
    Verify(true);

    return (*i).second;
}

void CSeqDBAtlas::SetMemoryBound(Uint8 mb)
{
    CSeqDBLockHold locked(*this);
    Lock(locked);

    Verify(true);

    m_Strategy.SetMemoryBound(mb);

    Verify(true);
}

void CSeqDBAtlas::RegisterExternal(CSeqDBMemReg   & memreg,
                                   size_t           bytes,
                                   CSeqDBLockHold & locked)
{
    if (bytes > 0) {
        Lock(locked);
        PossiblyGarbageCollect(bytes, false);

        _ASSERT(memreg.m_Bytes == 0);
        m_CurAlloc += memreg.m_Bytes = bytes;
    }
}

void CSeqDBAtlas::UnregisterExternal(CSeqDBMemReg & memreg)
{
    size_t bytes = memreg.m_Bytes;

    if (bytes > 0) {
        _ASSERT((size_t)m_CurAlloc >= bytes);
        m_CurAlloc     -= bytes;
        memreg.m_Bytes = 0;
    }
}


// 16 GB should be enough

const Int8 CSeqDBMapStrategy::e_MaxMemory64 = Int8(16) << 30;

Int8 CSeqDBMapStrategy::m_GlobalMaxBound = 0;

bool CSeqDBMapStrategy::m_AdjustedBound = false;

/// Constructor
CSeqDBMapStrategy::CSeqDBMapStrategy(CSeqDBAtlas & atlas)
    : m_Atlas     (atlas),
      m_MaxBound  (0),
      m_RetBound  (0),
      m_SliceSize (0),
      m_Overhang  (0),
      m_Order     (0.95, .901),
      m_InOrder   (true),
      m_MapFailed (false),
      m_LastOID   (0),
      m_BlockSize (4096)
{
    m_BlockSize = GetVirtualMemoryPageSize();

    if (m_GlobalMaxBound == 0) {
        SetDefaultMemoryBound(0);
        _ASSERT(m_GlobalMaxBound != 0);
    }
    m_MaxBound = m_GlobalMaxBound;
    x_SetBounds(m_MaxBound);
}

void CSeqDBMapStrategy::MentionOid(int oid, int num_oids)
{
    // Still working on the same oid, ignore.
    if (m_LastOID == oid) {
        return;
    }

    // The OID is compared to the previous OID.  Sequential access is
    // defined as having increasing OIDs about 90% of the time.
    // However, if the OID is only slightly before the previous OID,
    // it is ignored.  This is to allow sequential semantics for
    // multithreaded apps that divide work into chunks of OIDs.
    //
    // "Slightly" before is defined as the greater of 10 OIDs or
    // 10% of the database.  This 'window' of the database can
    // only move backward when the ordering test fails, so walking
    // backward through the entire database will not be considered
    // sequential.

    // In the blast libraries, work is divided into 1% of the database
    // or 1 OID.  So 10% allows 5 threads and the assumption that some
    // chunks will take as much as twice as long to run as others.

    int pct = 10;
    int window = max(num_oids/100*pct, pct);
    int low_bound = max(m_LastOID - window, 0);

    if (oid > m_LastOID) {
        // Register sequential access.
        x_OidOrder(true);
        m_LastOID = oid;
    } else if (oid < low_bound) {
        // Register non-sequential access.
        x_OidOrder(false);
        m_LastOID = oid;
    }
}

void CSeqDBMapStrategy::x_OidOrder(bool in_order)
{
    m_Order.AddData(in_order ? 1.0 : 0);

    // Moving average with thermostat-like hysteresis.
    bool new_order = m_Order.GetAverage() > (m_InOrder ? .8 : .9);

    if (new_order != m_InOrder) {
        // Rebuild the bounds with the new ordering constraint.
        m_InOrder = new_order;
        x_SetBounds(m_MaxBound);
    }
}

void CSeqDBMapStrategy::MentionMapFailure(Uint8 current)
{
    // The first map failure only modifies the slice size; after that
    // we reduce the amount of allocation permitted.

    if (m_MapFailed) {
        m_MaxBound = (m_MaxBound * 4) / 5;
        x_SetBounds(min((Int8) current, m_MaxBound));
    } else {
        m_MapFailed = true;
        x_SetBounds(m_MaxBound);
    }
}

Uint8 CSeqDBMapStrategy::x_Pick(Uint8 low, Uint8 high, Uint8 guess)
{
    // max and guess is usually computed; min is usually a
    // constant, so if there is a conflict, use min.

    if (low > high) {
        high = low;
    }

    int bs = int(m_BlockSize);

    if (guess < low) {
        guess = (low + bs - 1);
    }

    if (guess > high) {
        guess = high;
    }

    guess -= (guess % bs);

    _ASSERT((guess % bs) == 0);
    _ASSERT((guess >= low) && (guess <= high));

    return guess;
}

/// Set all parameters.
void CSeqDBMapStrategy::x_SetBounds(Uint8 bound)
{
    Uint8 max_bound(0);
    Uint8 max_slice(0);

    if (sizeof(int*) == 8) {
        max_bound = e_MaxMemory64;
        max_slice = e_MaxSlice64;
    } else {
        max_bound = e_MaxMemory32;
        max_slice = e_MaxSlice32;
    }

    int overhang_ratio = 32;
    int slice_ratio = 10;

    // If a mapping request has never failed, use large slice for
    // efficiency.  Otherwise, if the client follows a mostly linear
    // access pattern, use middle sized slices, and if not, use small
    // slices.

    const int no_limits   = 4;
    const int linear_oids = 10;
    const int random_oids = 80;

    if (! m_MapFailed) {
        slice_ratio = no_limits;
    } else if (m_InOrder) {
        slice_ratio = linear_oids;
    } else {
        slice_ratio = random_oids;
    }

    m_MaxBound = x_Pick(e_MinMemory,
                        min(max_bound, bound),
                        bound);

    m_SliceSize = x_Pick(e_MinSlice,
                         max_slice,
                         m_MaxBound / slice_ratio);

    m_RetBound = x_Pick(e_MinMemory,
                        m_MaxBound-((m_SliceSize*3)/2),
                        (m_MaxBound*8)/10);

    m_Overhang = x_Pick(e_MinOverhang,
                        e_MaxOverhang,
                        m_SliceSize / overhang_ratio);

    m_AdjustedBound = false;
}

void CSeqDBAtlas::SetDefaultMemoryBound(Uint8 bytes)
{
    CSeqDBMapStrategy::SetDefaultMemoryBound(bytes);
}

void CSeqDBMapStrategy::SetDefaultMemoryBound(Uint8 bytes)
{
    Uint8 app_space = CSeqDBMapStrategy::e_AppSpace;

    if (bytes == 0) {
        if (sizeof(int*) == 4) {
            bytes = e_MaxMemory32;
        } else {
            bytes = e_MaxMemory64;
        }

#if defined(NCBI_OS_UNIX)
        rlimit vspace;
        rusage ruse;

        int rc = 0;
        int rc2 = 0;

#ifdef RLIMIT_AS
        rc = getrlimit(RLIMIT_AS, & vspace);
#elif defined(RLIMIT_RSS)
        rc = getrlimit(RLIMIT_RSS, & vspace);
#else
        vspace.rlim_cur = RLIM_INFINITY;
#endif
        rc2 = getrusage(RUSAGE_SELF, & ruse);

        if (rc || rc2) {
            _ASSERT(rc == 0);
            _ASSERT(rc2 == 0);
        }

        Uint8 max_mem = vspace.rlim_cur;
        Uint8 max_mem75 = (max_mem/4)*3;

        if (max_mem < (app_space*2)) {
            app_space = max_mem/2;
        } else {
            max_mem -= app_space;
            if (max_mem > max_mem75) {
                max_mem = max_mem75;
            }
        }

        if (max_mem < bytes) {
            bytes = max_mem;
        }
#endif
    }

    m_GlobalMaxBound = bytes;
    m_AdjustedBound = true;
}

CSeqDBAtlasHolder::CSeqDBAtlasHolder(bool             use_mmap,
                                     CSeqDBFlushCB  * flush,
                                     CSeqDBLockHold * lockedp)
    : m_FlushCB(0)
{
    {{
    CFastMutexGuard guard(m_Lock);

    if (m_Count == 0) {
        m_Atlas = new CSeqDBAtlas(use_mmap);
    }
    m_Count ++;
    }}

    if (lockedp == NULL) {
    CSeqDBLockHold locked2(*m_Atlas);

    if (flush)
        m_Atlas->AddRegionFlusher(flush, & m_FlushCB, locked2);
    }
    else {
    if (flush)
        m_Atlas->AddRegionFlusher(flush, & m_FlushCB, *lockedp);
    }

}

DEFINE_CLASS_STATIC_FAST_MUTEX(CSeqDBAtlasHolder::m_Lock);

CSeqDBAtlasHolder::~CSeqDBAtlasHolder()
{
    if (m_FlushCB) {
        CSeqDBLockHold locked(*m_Atlas);
        m_Atlas->RemoveRegionFlusher(m_FlushCB, locked);
    }

    CFastMutexGuard guard(m_Lock);
    m_Count --;

    if (m_Count == 0) {
        delete m_Atlas;
    }
}

CSeqDBAtlas & CSeqDBAtlasHolder::Get()
{
    _ASSERT(m_Atlas);
    return *m_Atlas;
}

CSeqDBLockHold::~CSeqDBLockHold()
{
    CHECK_MARKER();

    if (m_Holds.size()) {
        m_Atlas.Lock(*this);
        for(size_t i = 0; i < m_Holds.size(); i++) {
            m_Holds[i]->RetRef();
        }
        m_Holds.clear();
    }

    m_Atlas.Unlock(*this);
    BREAK_MARKER();
}

/// Get a hold a region of memory.
void CSeqDBLockHold::HoldRegion(CSeqDBMemLease & lease)
{
    m_Atlas.Lock(*this);

    CRegionMap * rmap = lease.GetRegionMap();

    _ASSERT(rmap);

    for(size_t i = 0; i < m_Holds.size(); i++) {
        if (m_Holds[i] == rmap)
            return;
    }

    if (m_Holds.empty()) {
        m_Holds.reserve(4);
    }

    m_Holds.push_back(rmap);
    rmap->AddRef();
}

int CSeqDBAtlasHolder::m_Count = 0;
CSeqDBAtlas * CSeqDBAtlasHolder::m_Atlas = NULL;


void CSeqDBMapStrategy::x_CheckAdjusted()
{
    if (m_GlobalMaxBound && m_AdjustedBound) {
        x_SetBounds(m_GlobalMaxBound);
    }
}

CSeqDB_AtlasRegionHolder::
CSeqDB_AtlasRegionHolder(CSeqDBAtlas & atlas, const char * ptr)
    : m_Atlas(atlas), m_Ptr(ptr)
{
}

CSeqDB_AtlasRegionHolder::~CSeqDB_AtlasRegionHolder()
{
    if (m_Ptr) {
        CSeqDBLockHold locked(m_Atlas);
        m_Atlas.Lock(locked);

        m_Atlas.RetRegion(m_Ptr);
        m_Ptr = NULL;
    }
}

END_NCBI_SCOPE
