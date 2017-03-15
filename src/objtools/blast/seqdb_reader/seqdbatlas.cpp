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
    m_Alloc = false;
    Verify(true);
}

CSeqDBAtlas::~CSeqDBAtlas()
{
    Verify(true);

    int count = 0;
    for (map<string, CMemoryFile *>::iterator it=m_FileMemMap.begin(); it!=m_FileMemMap.end(); ++it) {
            string filename = it->first;
            it->second->Unmap();
            //cerr << "********Cleaning:Unmap CMemoryFile:" << filename << endl;                
            delete it->second;
            //fileMemMap.erase(filename);                 
            count++;                       
    }
    x_GarbageCollect(0);

    // Clear mapped file regions

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
        
    Verify(true);
}


// Algorithm:
//
// In file mode, get exactly what we need.  Otherwise, if the request
// fits entirely into one slice, use large slice rounding (get large
// pieces).  If it is on a large slice boundary, use small slice
// rounding (get small pieces).




// Assumes locked.

void CSeqDBAtlas::PossiblyGarbageCollect(Uint8 space_needed, bool returning)
{
    Verify(true);


        // Use Int8 to avoid "unsigned rollunder"

        Int8 bound = m_Strategy.GetMemoryBound(returning);
        Int8 capacity_left = bound - m_CurAlloc;

        if (Int8(space_needed) > capacity_left) {
            x_GarbageCollect(bound - space_needed);
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



/// Releases a hold on a partial mapping of the file.
void CSeqDBAtlas::x_RetRegionNonRecent(const char * datap)
{
    Verify(true);
    
    bool worked = x_Free(datap);
    _ASSERT(worked);

    if (! worked) {
        cerr << "Address leak in CSeqDBAtlas::RetRegion" << endl;
    }
    Verify(true);
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
    m_Alloc = true;
    //cerr << "allocated " << m_CurAlloc << " memory" << endl;
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
    if(!m_Alloc) return true;
    TPoolIter i = m_Pool.find((const char*) freeme);

    if (i == m_Pool.end()) {
        return false;
    }

    size_t sz = (*i).second;

    _ASSERT(m_CurAlloc >= (TIndx)sz);
    m_CurAlloc -= sz;    
    //cerr << "deallocated " << sz << " memory m_CurAlloc=" << m_CurAlloc << endl;
    if(m_CurAlloc == 0) m_Alloc = false;
    char * cp = (char*) freeme;
    delete[] cp;
    m_Pool.erase(i);

    return true;
}


EMemoryAdvise CRegionMap::sm_MmapStrategy_Index    = eMADV_Normal;

EMemoryAdvise CRegionMap::sm_MmapStrategy_Sequence = eMADV_Normal;

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

    m_Atlas.Unlock(*this);
    BREAK_MARKER();
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
