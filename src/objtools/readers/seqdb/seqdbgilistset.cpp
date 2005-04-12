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

/// @file seqdbgilistset.cpp
/// Implementation for the CSeqDBVol class, which provides an
/// interface for all functionality of one database volume.

#include <ncbi_pch.hpp>
#include "seqdbgilistset.hpp"
#include <algorithm>

BEGIN_NCBI_SCOPE


/// Defines a pair of integers and a sort order.
///
/// This struct stores a pair of integers, the volume index and the
/// oid count.  The ordering is by the oid count, descending.

struct SSeqDB_IndexCountPair {
public:
    /// Index of the volume in the volume set.
    int m_Index;
    
    /// Number of OIDs associated with this volume.
    int m_Count;
    
    /// Less than operator, where elements with larger allows sorting.
    /// Elements are sorted by number of OIDs in descending order.
    /// @param rhs
    ///   The right hand side of the less than.
    bool operator < (const SSeqDB_IndexCountPair & rhs) const
    {
        return m_Count > rhs.m_Count;
    }
};


/// CSeqDBNodeGiList
/// 
/// This class defines a simple CSeqDBGiList subclass which is read
/// from a gi list file using the CSeqDBAtlas.  It uses the atlas for
/// file access and registers the memory used by the vector with the
/// atlas layer.

class CSeqDBNodeFileGiList : public CSeqDBGiList {
public:
    /// Build a GI list from a memory mapped file.
    ///
    /// Given a GI list file mapped into a region of memory, this
    /// class reads the GIs from the file.
    ///
    /// @param beginp
    ///   Beginning of the memory mapped region.
    /// @param endp
    ///   First byte past the end of the memory mapped region.
    /// @param locked
    ///   Lock holder object for this thread.
    CSeqDBNodeFileGiList(CSeqDBAtlas    & atlas,
                         const string   & fname,
                         CSeqDBLockHold & locked)
        : m_VectorMemory(atlas)
    {
        CSeqDBAtlas::TIndx file_size(0);
        
        CSeqDBMemLease memlease(atlas);
        atlas.GetFile(memlease, fname, file_size, locked);
        
        const char * fbeginp = memlease.GetPtr(0);
        const char * fendp   = fbeginp + (int)file_size;
        
        try {
            bool in_order = false;
            SeqDB_ReadMemoryGiList(fbeginp, fendp, m_GisOids, & in_order);
            if (in_order) {
                m_CurrentOrder = eGi;
            }
        }
        catch(...) {
            memlease.Clear();
            throw;
        }
        memlease.Clear();
        
        int vector_size = int(m_GisOids.size() * sizeof(m_GisOids[0]));
        atlas.RegisterExternal(m_VectorMemory, vector_size, locked);
    }
    
    /// Destructor
    virtual ~CSeqDBNodeFileGiList()
    {
    }
    
private:
    /// Memory associated with the m_GisOids vector.
    CSeqDBMemReg m_VectorMemory;
};


CSeqDBGiListSet::CSeqDBGiListSet(CSeqDBAtlas        & atlas,
                                 const CSeqDBVolSet & volset,
                                 CRef<CSeqDBGiList>   user_gi_list,
                                 CSeqDBLockHold     & locked)
    : m_Atlas    (atlas),
      m_UserList (user_gi_list)
{
    if (m_UserList.NotEmpty()) {
        int gis_size = m_UserList->Size();
        
        if (0 == gis_size) {
            return;
        }
        
        typedef SSeqDB_IndexCountPair TIndexCount;
        vector<TIndexCount> OidsPerVolume;
        
        // Build a list of volumes sorted by OID count.
        
        for(int i = 0; i < volset.GetNumVols(); i++) {
            const CSeqDBVolEntry * vol = volset.GetVolEntry(i);
            
            TIndexCount vol_oids;
            vol_oids.m_Index = i;
            vol_oids.m_Count = vol->OIDEnd() - vol->OIDStart();
            
            OidsPerVolume.push_back(vol_oids);
        }
        
        // The largest volumes should be used first, to minimize the
        // number of failed GI->OID conversion attempts.  Searching input
        // GIs against larger volumes first should eliminate most of the
        // GIs by the time smaller volumes are searched, thus reducing the
        // total number of lookups.
        
        std::sort(OidsPerVolume.begin(), OidsPerVolume.end());
        
        for(int i = 0; i < (int)OidsPerVolume.size(); i++) {
            int vol_idx = OidsPerVolume[i].m_Index;
            
            const CSeqDBVolEntry * vol = volset.GetVolEntry(vol_idx);
            
            // Note: The implied ISAM lookups will sort by GI.
            
            vol->Vol()->GisToOids(vol->OIDStart(),
                                  vol->OIDEnd(),
                                  *m_UserList,
                                  locked);
        }
    }
}

CRef<CSeqDBGiList>
CSeqDBGiListSet::GetNodeGiList(const string    & filename,
                               const CSeqDBVol * volp,
                               int               vol_start,
                               int               vol_end,
                               CSeqDBLockHold  & locked)
{
    // Note: possibly the atlas should have a method to add and
    // subtract pseudo allocations from the memory bound; this would
    // allow GI list vectors to share the memory bound with memory
    // mapped file ranges.
    
    m_Atlas.Lock(locked);
    CRef<CSeqDBGiList> gilist = m_NodeListMap[filename];
    
    if (gilist.Empty()) {
        gilist.Reset(new CSeqDBNodeFileGiList(m_Atlas, filename, locked));
        
        if (m_UserList.NotEmpty()) {
            x_TranslateFromUserList(*gilist);
        }
        m_NodeListMap[filename] = gilist;
    }
    
    if (m_UserList.Empty()) {
        volp->GisToOids(vol_start, vol_end, *gilist, locked);
    }
    
    // If there is a volume GI list, it will also be attached to the
    // volume, and replaces the user GI list attachment (if there was
    // one).
    
    volp->AttachGiList(gilist);
    
    return gilist;
}

void CSeqDBGiListSet::x_TranslateFromUserList(CSeqDBGiList & gilist)
{
    CSeqDBGiList & source = *m_UserList;
    CSeqDBGiList & target = gilist;
    
    source.InsureOrder(CSeqDBGiList::eGi);
    target.InsureOrder(CSeqDBGiList::eGi);
    
    int source_num = source.Size();
    int target_num = target.Size();
    
    int source_index = 1;
    int target_index = 1;
    
    while(source_index < source_num && target_index < target_num) {
        int source_gi = source[source_index].gi;
        int target_gi = target[target_index].gi;
        
        // Match; translate if needed
        
        if (source_gi == target_gi) {
            if (target[target_index].oid == -1) {
                target.SetTranslation(target_index, source[source_index].oid);
            }
            target_index++;
            source_index++;
        } else if (source_gi > target_gi) {
            target_index ++;
            
            // Search target using expanding jumps
            int jump = 2;
            int test = target_index + jump;
            
            while(test < target_num && target[test].gi < source_gi) {
                target_index = test;
                jump *= 2;
                test = target_index + jump;
            }
        } else /* source_gi < target_gi */ {
            source_index ++;
            
            // Search source using expanding jumps
            int jump = 2;
            int test = source_index + jump;
            
            while(test < source_num && source[test].gi < target_gi) {
                source_index = test;
                jump *= 2;
                test = source_index + jump;
            }
        }
    }
}

END_NCBI_SCOPE

