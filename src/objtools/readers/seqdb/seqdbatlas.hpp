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
/// @file blast_options_handle.cpp
/// Implementation for the CBlastOptionsHandle and the
/// CBlastOptionsFactory classes.

/// CSeqDBAtlas class
/// 
/// This object manages a set of memory areas, called "maps".  In most
/// cases these will be memory maps.  In other cases, they may be
/// sections of memory containing data read from files.

#include <corelib/ncbimtx.hpp>
#include <corelib/ncbiatomic.hpp>
#include <corelib/ncbifile.hpp>
#include <util/random_gen.hpp>
#include <vector>
#include <map>
#include <set>

BEGIN_NCBI_SCOPE

class CSeqDBAtlas;

class CSeqDBFlushCB {
public:
    virtual void operator()(void) = 0;
};

class CSeqDBSpinLock {
public:
    CSeqDBSpinLock()
      : m_L(0)
    {
    }
    
    ~CSeqDBSpinLock()
    {
    }
    
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
    
    void Unlock()
    {
        // If we hold the lock, atomicity shouldn't be an issue here.
	m_L = (void*)0;
    }
    
private:
    void * volatile m_L;
};

class CSeqDBLockHold {
public:
    CSeqDBLockHold(class CSeqDBAtlas & atlas)
        : m_Atlas(atlas),
          m_Locked(false)
    {
    }
    
    inline ~CSeqDBLockHold();
    
private:
    CSeqDBLockHold(CSeqDBLockHold & oth);
    
    friend class CSeqDBAtlas;
    
    class CSeqDBAtlas & m_Atlas;
    bool                m_Locked;
};

class CRegionMap {
public:
    typedef Uint4 TIndx;
    
    typedef map<const char *, CRegionMap*> TAddressTable;
    
    CRegionMap(const string * fname,
               Uint4          fid,
               TIndx          begin,
               TIndx          end);
    
    ~CRegionMap();
    
    bool MapMmap(CSeqDBAtlas *);
    bool MapFile(CSeqDBAtlas *);
    
    void AddRef()
    {
        _ASSERT(m_Ref >= 0);
        m_Ref ++;
        m_Clock = 0;
    }
    
    void RetRef()
    {
        _ASSERT(m_Ref > 0);
        m_Ref --;
    }
    
    bool InUse()
    {
        return m_Ref != 0;
    }
    
    bool InRange(const char * p) const
    {
        _ASSERT(m_Data);
        return ((p >= m_Data) && (p < (m_Data + (m_End - m_Begin))));
    }
    
    const string & Name()
    {
        return *m_Fname;
    }
    
    TIndx Begin()
    {
        return m_Begin;
    }
    
    TIndx End()
    {
        return m_End;
    }
    
    TIndx Length()
    {
        return m_End - m_Begin;
    }
    
    const char * Data(TIndx begin, TIndx end);
        
    const char * Data() { return m_Data; }
        
    Uint4 Fid()
    {
        return m_Fid;
    }
        
    int GetClock()
    {
        return m_Clock + m_Penalty;
    }
        
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
    

    // It might be wise to split this class into two layers - one that
    // contains the list of regions and only handles adding, removing,
    // and lookup of those, and another that dictates semantics,
    // i.e. garbage collection, etc.
    
    CSeqDBSpinLock m_Lock;
    //CFastMutex m_Lock;
    

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
    
    CRandom m_RandGen;
    
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

