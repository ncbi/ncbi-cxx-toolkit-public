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

/// CSeqDBAtlas class
/// 
/// This object manages a set of memory areas, called "maps".  In most
/// cases these will be memory maps.  In other cases, they may be
/// sections of memory containing data read from files.

#include <corelib/ncbimtx.hpp>
#include <corelib/ncbifile.hpp>
#include <util/random_gen.hpp>
#include <vector>
#include <map>
#include <set>

BEGIN_NCBI_SCOPE

class CSeqDBAtlas;

class CSeqDBMemLease {
public:
    CSeqDBMemLease(CSeqDBAtlas & atlas)
        : m_Atlas(atlas),
          m_Data (0),
          m_Begin(0),
          m_End  (0),
          m_Ident(0)
    {
    }
    
    ~CSeqDBMemLease()
    {
        Clear(false);
    }
    
    void Clear()
    {
        Clear(false);
    }
    
    bool Contains(Uint8 begin, Uint8 end) const
    {
        return m_Data && (m_Begin <= begin) && (end <= m_End);
    }
    
    const char * GetPtr(Uint8 offset) const
    {
        return (m_Data + (Uint4)(offset - m_Begin));
    }
    
    bool Empty() const
    {
        return m_Data == 0;
    }
    
private:
    inline void Clear(bool locked);
    
    CSeqDBMemLease(const CSeqDBMemLease & ml);
    CSeqDBMemLease & operator =(const CSeqDBMemLease & ml);
    
    void SetRegion(Uint8         begin,
                   Uint8         end,
                   Uint4         ident,
                   const char  * data,
                   bool          locked)
    {
        Clear(locked);
        
        m_Data  = data;
        m_Begin = begin;
        m_End   = end;
        m_Ident = ident;
    }
    
    friend class CSeqDBAtlas;
    
    CSeqDBAtlas & m_Atlas;
    const char  * m_Data;
    Uint8         m_Begin;
    Uint8         m_End;
    Uint4         m_Ident;
};

class CSeqDBAtlas {
    enum {
        eTriggerGC    = 1024 * 1024 * 256,
        eMemoryBound  = 1024 * 1024 * 512 * 3 // 1.5 GB
    };
    
public:
    /// Create an empty atlas.
    CSeqDBAtlas(bool use_mmap);
    
    /// The destructor unmaps and frees all associated memory.
    ~CSeqDBAtlas();
    
    /// Gets whole-file mapping, and returns file data and length.
    const char * GetFile(const string & fname, Uint8 & length);
    
    /// Gets whole-file mapping, and returns file data and length.
    void GetFile(CSeqDBMemLease & lease, const string & fname, Uint8 & length);
    
    /// Gets mapping for entire file and file length, return true if found.
    bool GetFileSize(const string & fname, Uint8 & length);
    
    /// Gets a partial mapping of the file.
    const char * GetRegion(const string & fname, Uint8 begin, Uint8 end);
    
    /// Gets a partial mapping of the file.
    void GetRegion(CSeqDBMemLease  & lease,
                   const string    & fname,
                   Uint8             begin,
                   Uint8             end);
    
    /// Releases a partial mapping of the file, or returns an allocated block.
    void RetRegion(CSeqDBMemLease & ml, bool locked);
    
    /// Releases a partial mapping of the file, or returns an allocated block.
    void RetRegion(const char * datap);
    
    /// Clean up unreferenced objects
    void GarbageCollect();
    
    /// Display memory layout (this code is temporary).
    void ShowLayout(bool locked, Uint8 index);
    
    /// Allocate memory and track it with an internal set.
    char * Alloc(Uint4 length);
    
    /// Return memory, removing it from the internal set.
    void Free(const char * freeme);
    
    /// Copy range from one lease to another (updating refcnts).
    void Dup(CSeqDBMemLease & recv, const CSeqDBMemLease & src);
    
    class CRegionMap {
    public:
        CRegionMap(const string * fname,
                   Uint4          fid,
                   Uint8          begin,
                   Uint8          end,
                   Uint4          ident);
        
        ~CRegionMap();
        
        bool MapMmap();
        bool MapFile();
        
        void AddRef();
        void RetRef();
        
        bool InUse()
        {
            return m_Ref != 0;
        }
        
        bool InRange(const char * p);
        
        const string & Name()
        {
            return *m_Fname;
        }
        
        Uint8 Begin()
        {
            return m_Begin;
        }

        Uint8 End()
        {
            return m_End;
        }
        
        Uint8 Length()
        {
            return m_End - m_Begin;
        }
        
        const char * Data(Uint8 begin, Uint8 end);
        
        const char * Data() { return m_Data; }
        
        Uint4 Ident()
        {
            return m_Ident;
        }
        
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
        MatchAndUse(Uint4         fid,
                    Uint8       & begin,
                    Uint8       & end,
                    Uint4       & ident,
                    const char ** start);
        
        void Show();
        
        void GetBoundaries(const char ** p,
                           Uint8      &  begin,
                           Uint8      &  end)
        {
            *p    = m_Data;
            begin = m_Begin;
            end   = m_End;
        }
        
        void x_Roundup(Uint8 & begin,
                       Uint8 & end,
                       int   & penalty,
                       Uint8   file_size,
                       bool    use_mmap);
        
        inline bool operator < (const CRegionMap & other) const;
        
    private:
        const char    * m_Data;
        CMemoryFile   * m_MemFile;
        bool            m_ManualMap;
        
        const string * m_Fname;
        Uint8          m_Begin;
        Uint8          m_End;
        Uint4          m_Fid;
        Uint4          m_Ident;
        
        // Usage of clock: When a reference is gotten, the clock is
        // set to zero.  When GC is run, all unused clocks are bumped.
        // The GC adds "clock" and "penalty" together and removes
        // regions with the highest resulting value first.
        
        int m_Ref;
        int m_Clock;
        int m_Penalty;
    };
    
private:
    typedef map<const char *, Uint4>::iterator TPoolIter;
    
    const char * x_FindRegion(Uint4         fid,
                              Uint8       & begin,
                              Uint8       & end,
                              Uint4       & ident,
                              const char ** startp);
    
    /// Gets a partial mapping of the file.
    const char * x_GetRegion(const string & fname,
                             Uint8        & begin,
                             Uint8        & end,
                             Uint4        & ident,
                             const char  ** start);
    
    /// Try to find the region in the memory pool and free it.
    bool x_Free(const char * freeme);
    
    /// Clean up unreferenced objects (non-locking version.)
    void x_GarbageCollect(Uint8 reduce_to);
    
    /// Possibly adjust mapping request to cover larger area.
    void x_Roundup(Uint8  & begin,
                   Uint8  & end,
                   int    & penalty,
                   Uint8    file_size,
                   bool     use_mmap);
    
    inline Uint4 x_LookupFile(const string  & fname,
                              const string ** map_fname_ptr);
    
    Uint4 x_GetNewIdent();
    
    
    // Data
    

    // It might be wise to split this class into two layers - one that
    // contains the list of regions and only handles adding, removing,
    // and lookup of those, and another that dictates semantics,
    // i.e. garbage collection, etc.
    
    CFastMutex m_Lock;
    bool       m_UseMmap;
    Uint4      m_CurAlloc;
    
    vector<CRegionMap*> m_Regions;
    map<const char *, Uint4> m_Pool;
    
    Uint4 m_LastFID;
    map<string, Uint4> m_FileIDs;
    
    map<Uint4, CRegionMap *> m_IDLookup;
    
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
};

Uint4 CSeqDBAtlas::x_LookupFile(const string  & fname,
                                const string ** map_fname_ptr)
{
    map<string, Uint4>::iterator i = m_FileIDs.find(fname);
    
    if (i == m_FileIDs.end()) {
        m_FileIDs[fname] = ++ m_LastFID;
        
        i = m_FileIDs.find(fname);
    }
    
    // Get address of string in string->fid table.
    
    *map_fname_ptr = & (*i).first;
    
    return (*i).second;
}

inline void CSeqDBMemLease::Clear(bool locked)
{
    m_Atlas.RetRegion(*this, locked);
}

const char *
CSeqDBAtlas::CRegionMap::MatchAndUse(Uint4         fid,
                                     Uint8       & begin,
                                     Uint8       & end,
                                     Uint4       & ident,
                                     const char ** start)
{
    _ASSERT(fid);
    _ASSERT(m_Fid);
    
    if ((fid == m_Fid) && (m_Begin <= begin) && (m_End   >= end)) {
        AddRef();
        
        const char * location = m_Data + (begin - m_Begin);
        
        begin  = m_Begin;
        end    = m_End;
        ident  = m_Ident;
        *start = m_Data;
        
        return location;
    }
    
    return 0;
}

bool CSeqDBAtlas::CRegionMap::operator < (const CRegionMap & other) const
{
#define COMPARE_FIELD_AND_RETURN(OTH,FLD) \
    if (FLD < other.FLD) \
        return true; \
    if (OTH.FLD < FLD) \
        return false;
    
    // Sort via fields: fid,begin,end,ident.
    // Should never get to "ident".
    
    COMPARE_FIELD_AND_RETURN(other, m_Fid);
    COMPARE_FIELD_AND_RETURN(other, m_Begin);
    COMPARE_FIELD_AND_RETURN(other, m_End);
    
    return m_Ident < other.m_Ident;
}


END_NCBI_SCOPE

#endif // OBJTOOLS_READERS_SEQDB__SEQDBATLAS_HPP

