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

#ifndef SKIP_DOXYGEN_PROCESSING
static char const rcsid[] = "$Id$";
#endif /* SKIP_DOXYGEN_PROCESSING */

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
    ///   The right hand side of the less than. [in]
    bool operator < (const SSeqDB_IndexCountPair & rhs) const
    {
        return m_Count > rhs.m_Count;
    }
};


/// CSeqDBNodeIdList
///
/// This class defines a simple CSeqDBGiList subclass which is read
/// from a gi list file using the CSeqDBAtlas.  It uses the atlas for
/// file access and registers the memory used by the vector with the
/// atlas layer.

class CSeqDBNodeFileIdList : public CSeqDBGiList {
public:
    /// Build a GI,TI, or SI list from a memory mapped file.
    ///
    /// Given an ID list file mapped into a region of memory, this
    /// class reads the GIs or TIs from the file.
    ///
    /// @param atlas
    ///   The memory management layer object. [in]
    /// @param fname
    ///   The filename of this ID list. [in]
    /// @param list_type
    ///   The type of ID [in]
    /// @param locked
    ///   Lock holder object for this thread. [in]
    CSeqDBNodeFileIdList(CSeqDBAtlas       & atlas,
                         const CSeqDB_Path & fname,
                         CSeqDBGiListSet::EGiListType list_type,
                         CSeqDBLockHold    & locked)
        : m_VectorMemory(atlas)
    {
        CSeqDBAtlas::TIndx file_size(0);

        CSeqDBMemLease memlease(atlas);
        atlas.GetFile(memlease, fname.GetPathS(), file_size, locked);

        const char * fbeginp = memlease.GetPtr(0);
        const char * fendp   = fbeginp + (int)file_size;

        try {
            bool in_order = false;

            switch(list_type) {
            case CSeqDBGiListSet::eGiList:
                SeqDB_ReadMemoryGiList(fbeginp, fendp, m_GisOids, & in_order);
                break;
            case CSeqDBGiListSet::eTiList:
                SeqDB_ReadMemoryTiList(fbeginp, fendp, m_TisOids, & in_order);
                break;
            case CSeqDBGiListSet::eSiList:
                SeqDB_ReadMemorySiList(fbeginp, fendp, m_SisOids, & in_order);
            }

            if (in_order) {
                m_CurrentOrder = eGi;
            }
        }
        catch(...) {
            memlease.Clear();
            throw;
        }

        memlease.Clear();

        int vector_size =
            (int(m_GisOids.size() * sizeof(m_GisOids[0])) +
             int(m_TisOids.size() * sizeof(m_TisOids[0])));

        atlas.RegisterExternal(m_VectorMemory, vector_size, locked);
        // TODO m_VectorMemory for seqid_list
    }

    /// Destructor
    virtual ~CSeqDBNodeFileIdList()
    {
    }

private:
    /// Memory associated with the m_GisOids vector.
    CSeqDBMemReg m_VectorMemory;
};


CSeqDBGiListSet::CSeqDBGiListSet(CSeqDBAtlas            & atlas,
                                 const CSeqDBVolSet     & volset,
                                 CRef<CSeqDBGiList>       user_list,
                                 CRef<CSeqDBNegativeList> neg_list,
                                 CSeqDBLockHold         & locked)
    : m_Atlas        (atlas),
      m_UserList     (user_list),
      m_NegativeList (neg_list)
{
    _ASSERT(user_list.Empty() || neg_list.Empty());

    if (m_UserList.NotEmpty() && m_UserList->NotEmpty()) {
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

            // Note: The implied ISAM lookups will sort by GI/TI.

            vol->Vol()->IdsToOids(*m_UserList, locked);
        }
    } else if (m_NegativeList.NotEmpty() && m_NegativeList->NotEmpty()) {
        // We don't bother to sort these since every ISAM mapping must
        // be examined for the negative ID list case.

        for(int i = 0; i < volset.GetNumVols(); i++) {
            const CSeqDBVolEntry * vol = volset.GetVolEntry(i);

            // Note: The implied ISAM lookups will sort by GI/TI.

            vol->Vol()->IdsToOids(*m_NegativeList, locked);
        }
    }
}

CRef<CSeqDBGiList>
CSeqDBGiListSet::GetNodeIdList(const CSeqDB_Path & filename,
                               const CSeqDBVol   * volp,
                               EGiListType         list_type,
                               CSeqDBLockHold    & locked)
{
    // Note: possibly the atlas should have a method to add and
    // subtract pseudo allocations from the memory bound; this would
    // allow GI list vectors to share the memory bound with memory
    // mapped file ranges.

    m_Atlas.Lock(locked);

    // Seperate indices are used for TIs and GIs.  (Attempting to use
    // the same file for both should also produce an error when the
    // binary file is read, as the magic number is different.)

    TNodeListMap& map_ref = (list_type == eGiList) ? m_GINodeListMap :
                            ((list_type == eTiList) ? m_TINodeListMap : m_SINodeListMap);
    CRef<CSeqDBGiList> gilist = map_ref[filename.GetPathS()];

    if (gilist.Empty()) {
        gilist.Reset(new CSeqDBNodeFileIdList(m_Atlas,
                                              filename,
                                              list_type,
                                              locked));

        if (m_UserList.NotEmpty()) {
            // Note: translates the GIs and TIs, but ignores Seq-ids.
            x_TranslateFromUserList(*gilist);
        }

        map_ref[filename.GetPathS()] = gilist;
    }

    // Note: in pure-GI mode, it might be more efficient (in some
    // cases) to translate all sub-lists, then translate the main list
    // in terms of those, rather than the other way round.  It might
    // be a good idea to investigate this -- it should only be done if
    // the sub-lists are (collectively) smaller than the user list.

    // If there are Seq-ids, we need to translate all GI lists from
    // the volume data, because some of the Seq-ids may refer to the
    // same sequences as GIs in those lists.  More sophisticated
    // methods are possible; for example, we could try to convert all
    // Seq-ids to GIs.  If this worked (it would for those databases
    // where GIs are available for all sequences), then we would be
    // able to continue processing as if the User GI list were
    // strictly GI based.

    // The ideal solution might be to build a conceptual map of all
    // data sources and estimate the time needed for different
    // techniques.  This has not been done.

    bool mixed_ids = m_UserList.Empty() || (!! m_UserList->GetNumSis());

    if (! mixed_ids) {
        if ((m_UserList->GetNumTis() && gilist->GetNumGis()) ||
            (m_UserList->GetNumGis() && gilist->GetNumTis())) {

            mixed_ids = true;
        }
    }

    if (m_UserList.Empty() || mixed_ids) {
        volp->IdsToOids(*gilist, locked);
    }

    // If there is a volume GI list, it will also be attached to the
    // volume, and replaces the user GI list attachment (if there was
    // one).  There can be one user GI list or multiple volume GI
    // lists for each volume.

    volp->AttachVolumeGiList(gilist);

    return gilist;
}

void CSeqDBGiListSet::x_TranslateGisFromUserList(CSeqDBGiList & gilist)
{
    CSeqDBGiList & source = *m_UserList;
    CSeqDBGiList & target = gilist;

    source.InsureOrder(CSeqDBGiList::eGi);
    target.InsureOrder(CSeqDBGiList::eGi);

    int source_num = source.GetNumGis();
    int target_num = target.GetNumGis();

    int source_index = 0;
    int target_index = 0;

    while(source_index < source_num && target_index < target_num) {
        TGi source_gi = source.GetGiOid(source_index).gi;
        TGi target_gi = target.GetGiOid(target_index).gi;

        // Match; translate if needed

        if (source_gi == target_gi) {
            if (target.GetGiOid(target_index).oid == -1) {
                target.SetGiTranslation(target_index, source.GetGiOid(source_index).oid);
            }
            target_index++;
            source_index++;
        } else if (source_gi > target_gi) {
            target_index ++;

            // Search target using expanding jumps
            int jump = 2;
            int test = target_index + jump;

            while(test < target_num && target.GetGiOid(test).gi < source_gi) {
                target_index = test;
                jump *= 2;
                test = target_index + jump;
            }
        } else /* source_gi < target_gi */ {
            source_index ++;

            // Search source using expanding jumps
            int jump = 2;
            int test = source_index + jump;

            while(test < source_num && source.GetGiOid(test).gi < target_gi) {
                source_index = test;
                jump *= 2;
                test = source_index + jump;
            }
        }
    }
}

void CSeqDBGiListSet::x_TranslateTisFromUserList(CSeqDBGiList & gilist)
{
    CSeqDBGiList & source = *m_UserList;
    CSeqDBGiList & target = gilist;

    source.InsureOrder(CSeqDBGiList::eGi);
    target.InsureOrder(CSeqDBGiList::eGi);

    int source_num = source.GetNumTis();
    int target_num = target.GetNumTis();

    int source_index = 0;
    int target_index = 0;

    while(source_index < source_num && target_index < target_num) {
        Int8 source_ti = source.GetTiOid(source_index).ti;
        Int8 target_ti = target.GetTiOid(target_index).ti;

        // Match; translate if needed

        if (source_ti == target_ti) {
            if (target.GetTiOid(target_index).oid == -1) {
                target.SetTiTranslation(target_index,
                                        source.GetTiOid(source_index).oid);
            }

            target_index++;
            source_index++;
        } else if (source_ti > target_ti) {
            target_index ++;

            // Search target using expanding jumps
            int jump = 2;
            int test = target_index + jump;

            while(test < target_num &&
                  target.GetTiOid(test).ti < source_ti) {

                target_index = test;
                jump *= 2;
                test = target_index + jump;
            }
        } else /* source_ti < target_ti */ {
            source_index ++;

            // Search source using expanding jumps
            int jump = 2;
            int test = source_index + jump;

            while(test < source_num &&
                  source.GetTiOid(test).ti < target_ti) {

                source_index = test;
                jump *= 2;
                test = source_index + jump;
            }
        }
    }
}

void CSeqDBGiListSet::x_TranslateFromUserList(CSeqDBGiList & gilist)
{
    x_TranslateGisFromUserList(gilist);
    x_TranslateTisFromUserList(gilist);
}

END_NCBI_SCOPE

