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
* Authors:
*           Aleksey Grichenko
*           Michael Kimelman
*           Andrei Gourianov
*
* File Description:
*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.11  2002/04/03 18:06:48  grichenk
* Fixed segmented sequence bugs (invalid positioning of literals
* and gaps). Improved CSeqVector performance.
*
* Revision 1.9  2002/03/08 21:36:21  gouriano
* *** empty log message ***
*
* Revision 1.8  2002/03/08 21:24:35  gouriano
* fixed errors with unresolvable references
*
* Revision 1.7  2002/02/25 21:05:29  grichenk
* Removed seq-data references caching. Increased MT-safety. Fixed typos.
*
* Revision 1.6  2002/02/21 19:27:06  grichenk
* Rearranged includes. Added scope history. Added searching for the
* best seq-id match in data sources and scopes. Updated tests.
*
* Revision 1.5  2002/02/01 21:49:38  gouriano
* minor changes to make it compilable and run on Solaris Workshop
*
* Revision 1.4  2002/01/30 22:09:28  gouriano
* changed CSeqMap interface
*
* Revision 1.3  2002/01/23 21:59:32  grichenk
* Redesigned seq-id handles and mapper
*
* Revision 1.2  2002/01/16 16:25:56  gouriano
* restructured objmgr
*
* Revision 1.1  2002/01/11 19:06:24  gouriano
* restructured objmgr
*
*
* ===========================================================================
*/

#include <objects/objmgr1/seq_map.hpp>
#include "seq_id_mapper.hpp"
#include <objects/objmgr1/scope.hpp>
#include <objects/objmgr1/bioseq_handle.hpp>
#include <objects/seq/Seq_inst.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


////////////////////////////////////////////////////////////////////
//  CSeqMap


void CSeqMap::Add(CSegmentInfo& interval)
{
    // Check for existing intervals
    vector< CRef<CSegmentInfo> >::iterator it;
    bool same_pos_found = false;
    for ( it = m_Data.begin(); it != m_Data.end(); ++it) {
        if (**it == interval) {
            // Equal intervals may appear just because there are many
            // unresolved references so that segment position does not
            // increase through several segments.
            same_pos_found = true;
        }
        if ((*it)->GetLength() > 0  &&  same_pos_found)
            break;
    }
    if (it != m_Data.end() ) {
        throw runtime_error
            ("CSeqMap::Add() -- duplicate interval in the seq-map");
    }
    // The new interval will be added AFTER all other intervals with
    // the same position. This will make the map work correctly in case
    // of multiple segments of unknown (zero) lengths.
    for ( it = m_Data.begin(); it != m_Data.end(); ++it) {
        if ((**it).GetPosition() > interval.GetPosition()) {
            m_Data.insert(it, &interval);
            break;
        }
    }
    if (it == m_Data.end()) {
        m_Data.push_back(&interval);
    }
}


int CSeqMap::x_FindSegment(int pos)
{
    size_t seg_idx = 0;
    // Ignore eSeqEnd
    for ( ; seg_idx+1 < m_Data.size(); seg_idx++) {
        int cur_pos = m_Data[seg_idx]->m_Position;
        int next_pos = m_Data[seg_idx+1]->m_Position;
        if (next_pos > pos  || (next_pos == pos  &&  cur_pos == pos))
            break;
    }
    return seg_idx;
}


CSeqMap::CSegmentInfo CSeqMap::x_Resolve(int pos, CScope& scope)
{
    CBioseq_Handle::TBioseqCore seq;
    int seg_idx = x_FindSegment(pos);
    CSegmentInfo seg = *(m_Data[seg_idx]);
    if ( seg_idx >=  m_FirstUnresolvedPos) {
        // Resolve map segments
        int iStillUnresolved = -1;
        int shift = 0;
        for (size_t i = m_FirstUnresolvedPos; i < m_Data.size(); i++) {
            if (m_Data[i]->m_Position+shift > pos  ||
                m_Data[i]->m_SegType == CSeqMap::eSeqEnd)
                break;
            seg_idx = i;
            m_Data[i]->m_Position += shift;
            if (m_Data[i]->m_SegType != CSeqMap::eSeqRef) {
                m_Data[i]->m_Resolved = true;
                continue; // not a reference - nothing to resolve
            }
            if (m_Data[i]->m_Length != 0) {
                m_Data[i]->m_Resolved = true;
                continue; // resolved reference, known length
            }
            CConstRef<CSeq_id> id(
                &CSeq_id_Mapper::GetSeq_id(m_Data[i]->m_RefSeq));
            CBioseq_Handle bh = scope.GetBioseqHandle(*id);
            if (bh) {
                seq = bh.GetBioseqCore();
                if ( seq->GetInst().IsSetLength() ) {
                    m_Data[i]->m_Resolved = true;
                    m_Data[i]->m_Length = seq->GetInst().GetLength();
                    shift += m_Data[i]->m_Length;
                }
            }
            if ((iStillUnresolved < 0) && !(m_Data[i]->m_Resolved)) {
                iStillUnresolved = i;
            }
        }
        // Update positions of segments following the seg_idx-th segment
        m_FirstUnresolvedPos = (iStillUnresolved >=0) ?
            iStillUnresolved : (seg_idx + 1);
        if (shift > 0) {
            for (size_t i = seg_idx+1; i < m_Data.size(); i++) {
                m_Data[i]->m_Position += shift;
            }
        }
        // now find again
        seg_idx = x_FindSegment(pos);
    }
    // this could happen in case of unresolvable references
    if (m_Data[seg_idx]->m_SegType == CSeqMap::eSeqEnd) {
        seg_idx = 0;
    }
    seg = *(m_Data[seg_idx]);
    if ( !seq  &&  seg.m_RefSeq ) {
        CConstRef<CSeq_id> id(
            &CSeq_id_Mapper::GetSeq_id(seg.m_RefSeq));
        CBioseq_Handle h = scope.GetBioseqHandle(*id);
        if ( h )
            seq = h.GetBioseqCore();
    }
    if ( bool(seq) && seq->GetInst().IsSetSeq_data() )
        seg.m_RefData = &seq->GetInst().GetSeq_data();
    return seg;
}

/* Obsolete function - the positions are calculated from the lengths,
not the lengths from the positions.
void CSeqMap::x_CalculateSegmentLengths(void)
{
    for (size_t i = 0; i < m_Data.size()-1; i++) {
        switch (m_Data[i]->GetType()) {
        case CSeqMap::eSeqData:
        case CSeqMap::eSeqRef:
        case CSeqMap::eSeqGap:
            m_Data[i]->m_length =
                m_Data[i+1]->GetPosition() - m_Data[i]->GetPosition();
            break;
        case CSeqMap::eSeqEnd:
        default:
            break;
        }
    }
}
*/


END_SCOPE(objects)
END_NCBI_SCOPE
