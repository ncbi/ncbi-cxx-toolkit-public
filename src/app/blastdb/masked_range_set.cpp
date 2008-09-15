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

#include <ncbi_pch.hpp>
#include "masked_range_set.hpp"

USING_NCBI_SCOPE;
USING_SCOPE(objects);

CMaskedRangesVector &
CMaskedRangeSet::GetRanges(const list< CRef<CSeq_id> > & idlist)
{
    // For each algorithm for which data is provided
    
    NON_CONST_ITERATE(CMaskedRangesVector, algo_iter, m_Ranges) {
        algo_iter->offsets.resize(0);
        int algo_id = algo_iter->algorithm_id;
        
        // Combine Seq-loc ranges for all provided Seq-ids.
        
        CConstRef<CSeq_loc> oid_ranges;
        
        ITERATE(list< CRef<CSeq_id> >, id_iter, idlist) {
            SSeqIdKey key(**id_iter);
            x_FindAndCombine(oid_ranges, algo_id, key);
        }
        
        if (oid_ranges.Empty()) {
            continue;
        }
        
        for(CSeq_loc_CI ranges(*oid_ranges); ranges; ++ranges) {
            CSeq_loc::TRange rng = ranges.GetRange();
            pair<TSeqPos, TSeqPos> pr(rng.GetFrom(), rng.GetToOpen());
            
            algo_iter->offsets.push_back(pr);
        }
    }
    
    return m_Ranges;
}

void CMaskedRangeSet::x_FindAndCombine(CConstRef<CSeq_loc> & L1,
                                       int                   algo_id,
                                       SSeqIdKey           & key)
{
    if ((int)m_Values.size() > algo_id) {
        const TAlgoMap & m = m_Values[algo_id];
        
        TAlgoMap::const_iterator iter = m.find(key);
        
        if (iter != m.end()) {
            x_CombineLocs(L1, *iter->second);
        }
    }
}

void CMaskedRangeSet::x_CombineLocs(CConstRef<CSeq_loc> & L1,
                                    const CSeq_loc      & L2)
{
    if (L1.Empty()) {
        L1.Reset(& L2);
    } else {
        L1 = L1->Add(L2, CSeq_loc::fMerge_All | CSeq_loc::fSort, NULL);
    }
}

CConstRef<CSeq_loc> & CMaskedRangeSet::x_Set(int algo_id, SSeqIdKey key)
{
    if ((int)m_Values.size() <= algo_id) {
        m_Values.resize(algo_id+1);
    }
    if (m_Values[algo_id].empty()) {
        m_Ranges.resize(m_Ranges.size()+1);
        m_Ranges[m_Ranges.size()-1].algorithm_id = algo_id;
    }
    return m_Values[algo_id][key];
}

