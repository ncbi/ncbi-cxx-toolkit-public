
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
* Author: Aleksey Grichenko
*
* File Description:
*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.1  2002/01/11 19:06:24  gouriano
* restructured objmgr
*
*
* ===========================================================================
*/


#include <objects/seq/Bioseq.hpp>
#include <objects/seq/Seq_inst.hpp>

#include <objects/objmgr1/object_manager.hpp>
#include <objects/objmgr1/seq_map.hpp>
#include "annot_object.hpp"
#include "seq_id_mapper.hpp"

#include <corelib/ncbithr.hpp>

#include <algorithm>


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


////////////////////////////////////////////////////////////////////
//
//  CSeqMap::
//


void CSeqMap::Add(const TSeqSegment& interval)
{
    // Check for existing intervals
    TSeqMap::iterator it = find(m_SeqMap.begin(), m_SeqMap.end(), interval);
    if (it != m_SeqMap.end() ) {
        throw runtime_error
            ("CSeqMap::Add() -- duplicate interval in the seq-map");
    }
    m_SeqMap.push_back(interval);
    // Sort intervals by starting position
    sort(m_SeqMap.begin(), m_SeqMap.end());
}


CSeqMap::TSeqSegment& CSeqMap::x_FindSegment(int pos)
{
    int seg_idx = 0;
    // Ignore eSeqEnd
    for ( ; seg_idx < m_SeqMap.size() - 1; seg_idx++) {
        int cur_pos = m_SeqMap[seg_idx].first;
        int next_pos = m_SeqMap[seg_idx+1].first;
        if (next_pos > pos  || (next_pos == pos  &&  cur_pos == pos))
            break;
    }
    return m_SeqMap[seg_idx];
}


CSeqMap::TSeqSegment& CSeqMap::x_Resolve(int pos, CScope& scope)
{
    TSeqSegment& seg = x_FindSegment(pos);
    if (seg.second.m_Resolved)
        return seg;
    // Resolve map segments
    int shift = 0;
    int seg_idx = 0;
    for (int i = m_FirstUnresolvedPos; i < m_SeqMap.size(); i++) {
        if (m_SeqMap[i].first+shift > pos  ||
            m_SeqMap[i].second.m_SegType == CSeqMap::eSeqEnd)
            break;
        seg_idx = i;
        m_SeqMap[i].second.m_Resolved = true;
        m_SeqMap[i].first += shift;
        if (m_SeqMap[i].second.m_SegType != CSeqMap::eSeqRef)
            continue; // not a reference - nothing to resolve
        if (m_SeqMap[i].second.m_RefLen != 0)
            continue; // resolved reference, known length
        CRef<CSeq_id> id(
            CSeqIdMapper::HandleToSeqId(m_SeqMap[i].second.m_RefSeq));
        CScope::TBioseqCore seq =
            scope.GetBioseqCore(scope.GetBioseqHandle(*id));
        if ( seq->GetInst().IsSetLength() ) {
            m_SeqMap[i].second.m_RefLen = seq->GetInst().GetLength();
            shift += m_SeqMap[i].second.m_RefLen;
        }
        if ( seq->GetInst().IsSetSeq_data() )
            m_SeqMap[i].second.m_RefData = &seq->GetInst().GetSeq_data();
    }
    // Update positions of segments following the seg_idx-th segment
    m_FirstUnresolvedPos = seg_idx + 1;
    if (shift > 0) {
        for (int i = seg_idx+1; i < m_SeqMap.size(); i++) {
            m_SeqMap[i].first += shift;
        }
    }
    return m_SeqMap[seg_idx];
}



END_SCOPE(objects)
END_NCBI_SCOPE
