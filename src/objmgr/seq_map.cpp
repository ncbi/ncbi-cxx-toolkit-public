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
*   Sequence map for the Object Manager. Describes sequence as a set of
*   segments of different types (data, reference, gap or end).
*
*/

#include <objects/objmgr/seq_map.hpp>
#include "seq_id_mapper.hpp"
#include <objects/objmgr/scope.hpp>
#include <objects/objmgr/bioseq_handle.hpp>
#include <objects/seq/Seq_inst.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


////////////////////////////////////////////////////////////////////
//  CSeqMap

CMutex CSeqMap::sm_SeqMap_Mtx;


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
        THROW1_TRACE(runtime_error,
            "CSeqMap::Add() -- Duplicate interval in the seq-map");
    }
    // The new interval will be added AFTER all other intervals with
    // the same position. This will make the map work correctly in case
    // of multiple segments of unknown (zero) lengths.
    for ( it = m_Data.begin(); it != m_Data.end(); ++it) {
        if ((**it).GetPosition() > interval.GetPosition()) {
            m_Data.insert(it, CRef<CSeqMap::CSegmentInfo>(&interval));
            break;
        }
    }
    if (it == m_Data.end()) {
        m_Data.push_back(CRef<CSeqMap::CSegmentInfo>(&interval));
    }
}


size_t CSeqMap::x_FindSegment(TSeqPos pos)
{
    CMutexGuard guard(sm_SeqMap_Mtx);
    if (pos == kInvalidSeqPos) {
        return m_Data.size() - 1;
    }
    size_t seg_idx = 0;
    // Ignore eSeqEnd
    for ( ; seg_idx+1 < m_Data.size(); seg_idx++) {
        // int cur_pos = m_Data[seg_idx]->m_Position;
        TSeqPos next_pos = m_Data[seg_idx+1]->m_Position;
        // The commented part was used to get the first of all matching
        // segments. It looks better to use the last one.
        if (next_pos > pos/*  || (next_pos == pos  &&  cur_pos == pos)*/)
            break;
    }
    return seg_idx;
}


CSeqMap::CSegmentInfo CSeqMap::x_Resolve(TSeqPos pos, CScope& scope)
{
    CBioseq_Handle::TBioseqCore seq;
    auto_ptr<CMutexGuard> guard(new CMutexGuard(sm_SeqMap_Mtx));
    size_t seg_idx = (pos == kInvalidSeqPos) ?
        m_Data.size() - 1 : x_FindSegment(pos);
    CSegmentInfo seg = *(m_Data[seg_idx]);
    if ( seg_idx >=  m_FirstUnresolvedPos) {
        // First, get bioseq handles for all unresolved references.
        // Note that some other thread may be doing the same right now.
        // The mutex should be unlocked when calling GetBioseqHandle()
        // to prevent dead-locks.
        map<size_t, TSeqPos> lenmap;
        for (size_t i = m_FirstUnresolvedPos; i < m_Data.size(); i++) {
            if (m_Data[i]->m_Position > pos  ||
                m_Data[i]->m_SegType == eSeqEnd)
                break;
            if (m_Data[i]->m_SegType != eSeqRef  ||
                m_Data[i]->m_Resolved)
                continue;
            CConstRef<CSeq_id> id(
                &CSeq_id_Mapper::GetSeq_id(m_Data[i]->m_RefSeq));
            guard.reset();
            CBioseq_Handle bh = scope.GetBioseqHandle(*id);
            if (bh) {
                seq = bh.GetBioseqCore();
                if ( seq->GetInst().IsSetLength() ) {
                    lenmap[i] = seq->GetInst().GetLength();
                }
                else {
                    lenmap[i] = kInvalidSeqPos;
                }
            }
            guard.reset(new CMutexGuard(sm_SeqMap_Mtx));
        }
        // Resolve map segments
        long iStillUnresolved = -1;
        TSeqPos shift = 0;
        for (size_t i = m_FirstUnresolvedPos; i < m_Data.size(); i++) {
            if (m_Data[i]->m_Position+shift > pos  ||
                m_Data[i]->m_SegType == eSeqEnd)
                break;
            seg_idx = i;
            m_Data[i]->m_Position += shift;
            if (m_Data[i]->m_SegType != eSeqRef) {
                m_Data[i]->m_Resolved = true;
                continue; // not a reference - nothing to resolve
            }
            if (m_Data[i]->m_Length != 0) {
                m_Data[i]->m_Resolved = true;
                continue; // resolved reference, known length
            }
            if (lenmap.find(i) != lenmap.end()) {
                TSeqPos len = lenmap[i];
                if (len != kInvalidSeqPos) {
                    m_Data[i]->m_Resolved = true;
                    m_Data[i]->m_Length = len;
                        shift += len;
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
        seg_idx = (pos == kInvalidSeqPos) ?
            0 : x_FindSegment(pos);
    }
    guard.reset();
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


const CSeq_id& CSeqMap::CSegmentInfo::GetRefSeqid(void) const
{
    _ASSERT(m_SegType == eSeqRef);
    _ASSERT((bool)m_RefSeq);
    return CSeq_id_Mapper::GetSeq_id(m_RefSeq);
}


void CSeqMap::DebugDump(CDebugDumpContext ddc, unsigned int depth) const
{
    ddc.SetFrame("CSeqMap");
    CObject::DebugDump( ddc, depth);

    DebugDumpValue(ddc, "m_FirstUnresolvedPos", m_FirstUnresolvedPos);
    if (depth == 0) {
        DebugDumpValue(ddc, "m_Data.size()", m_Data.size());
    } else {
        DebugDumpValue(ddc, "m_Data.type",
            "vector<CRef<CSegmentInfo>>");
        DebugDumpRangeCRef(ddc, "m_Data",
            m_Data.begin(), m_Data.end(), depth);
    }
}


void CSeqMap::CSegmentInfo::DebugDump(CDebugDumpContext ddc,
    unsigned int depth) const
{
    ddc.SetFrame("CSeqMap::CSegmentInfo");
    CObject::DebugDump( ddc, depth);

    string value;
    switch (m_SegType) {
    default:       value ="unknown"; break;
    case eSeqData: value ="data"; break;
    case eSeqRef:  value ="ref";  break;
    case eSeqGap:  value ="gap";  break;
    case eSeqEnd:  value ="end";  break;
    }
    ddc.Log("m_SegType", value);
    DebugDumpValue(ddc, "m_Position", m_Position);
    DebugDumpValue(ddc, "m_Length", m_Length);
    ddc.Log("m_RefSeq", m_RefSeq.AsString());
    ddc.Log("m_RefData", m_RefData.GetPointer(), depth);
    DebugDumpValue(ddc, "m_RefPos", m_RefPos);
    ddc.Log("m_MinusStrand", m_MinusStrand);
    ddc.Log("m_Resolved", m_Resolved);
}


END_SCOPE(objects)
END_NCBI_SCOPE

/*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.24  2002/11/04 21:29:12  grichenk
* Fixed usage of const CRef<> and CRef<> constructor
*
* Revision 1.23  2002/10/18 19:12:40  grichenk
* Removed mutex pools, converted most static mutexes to non-static.
* Protected CSeqMap::x_Resolve() with mutex. Modified code to prevent
* dead-locks.
*
* Revision 1.22  2002/07/08 20:51:02  grichenk
* Moved log to the end of file
* Replaced static mutex (in CScope, CDataSource) with the mutex
* pool. Redesigned CDataSource data locking.
*
* Revision 1.21  2002/05/29 21:21:13  gouriano
* added debug dump
*
* Revision 1.20  2002/05/06 17:43:06  ivanov
* ssize_t changed to long
*
* Revision 1.19  2002/05/06 17:03:49  ivanov
* Sorry. Rollback to R1.17
*
* Revision 1.18  2002/05/06 16:56:23  ivanov
* Fixed typo ssize_t -> size_t
*
* Revision 1.17  2002/05/06 03:28:47  vakatov
* OM/OM1 renaming
*
* Revision 1.16  2002/05/03 21:28:10  ucko
* Introduce T(Signed)SeqPos.
*
* Revision 1.15  2002/05/02 20:42:38  grichenk
* throw -> THROW1_TRACE
*
* Revision 1.14  2002/04/30 18:55:41  gouriano
* added GetRefSeqid function
*
* Revision 1.13  2002/04/11 12:07:30  grichenk
* Redesigned CAnnotTypes_CI to resolve segmented sequences correctly.
*
* Revision 1.12  2002/04/04 21:33:55  grichenk
* Fixed x_FindSegment() for sequences with unresolved segments
*
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
