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
*   Object manager iterators
*
*/

#include <objects/objmgr/annot_ci.hpp>
#include "annot_object.hpp"
#include "handle_range_map.hpp"
#include <objects/objmgr/tse_info.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


CAnnot_CI::CAnnot_CI(void)
    : m_TSEInfo(0),
      m_HandleRangeMap(0)
{
    return;
}


inline
CNcbiOstream& operator<<(CNcbiOstream& out, const SAnnotSelector& sel)
{
    return out << "{ annot="<<sel.m_AnnotChoice<<" feat="<<sel.m_FeatChoice<<" product="<<sel.m_FeatProduct<<" }";
}


inline
CNcbiOstream& operator<<(CNcbiOstream& out, const CHandleRangeMap& hrm)
{
    out << "{";
    iterate ( CHandleRangeMap::TLocMap, i, hrm.GetMap() ) {
        out << ' ' << i->first.AsString()<<':';
        iterate ( CHandleRange, j, i->second ) {
            out << ' ' << j->first.GetFrom()<<".."<<j->first.GetTo()<<(j->second == eNa_strand_minus?"-":"");
        }
    }
    return out << " }";
}


CAnnot_CI::CAnnot_CI(CTSE_Info& tse,
                     CHandleRangeMap& loc,
                     SAnnotSelector selector,
                     EOverlapType overlap_type)
    : m_TSEInfo(&tse),
      m_Selector(selector),
      m_RangeMap(0),
      m_HandleRangeMap(&loc),
      m_OverlapType(overlap_type)
{
    CTSE_Guard guard(tse);
    m_TSEInfo->LockCounter();
    iterate ( CHandleRangeMap::TLocMap, it, m_HandleRangeMap->GetMap() ) {
        if ( !it->second.Empty() ) {
            TAnnotMap::iterator ait =
                m_TSEInfo->m_AnnotMap.find(it->first);
            if (ait == m_TSEInfo->m_AnnotMap.end())
                continue;
            m_RangeMap = &ait->second;
            m_CurrentHandle = it->first;
            m_CoreRange = it->second.GetOverlappingRange();
            m_Current = m_RangeMap->begin(m_CoreRange);
            if ( m_Current )
                break;
        }
    }
    //_TRACE("Looking for " << m_Selector << " in " << loc);
    if ( !x_IsValid() )
        x_Walk();
}


CAnnot_CI::CAnnot_CI(const CAnnot_CI& iter)
    : m_TSEInfo(iter.m_TSEInfo),
      m_Selector(iter.m_Selector),
      m_RangeMap(iter.m_RangeMap),
      m_CoreRange(iter.m_CoreRange),
      m_Current(iter.m_Current),
      m_HandleRangeMap(iter.m_HandleRangeMap),
      m_CurrentHandle(iter.m_CurrentHandle),
      m_OverlapType(iter.m_OverlapType)
{
    //### Prevent TSE destruction between "if" and "lock"
    if ( m_TSEInfo ) {
        m_TSEInfo->LockCounter();
    }
    return;
}


CAnnot_CI& CAnnot_CI::operator= (const CAnnot_CI& iter)
{
    if ( m_TSEInfo ) {
        m_TSEInfo->UnlockCounter();
    }
    m_TSEInfo = iter.m_TSEInfo;
    m_Selector = iter.m_Selector;
    m_RangeMap = iter.m_RangeMap;
    m_CoreRange = iter.m_CoreRange;
    m_Current = iter.m_Current;
    m_HandleRangeMap = iter.m_HandleRangeMap;
    m_CurrentHandle = iter.m_CurrentHandle;
    m_OverlapType = iter.m_OverlapType;
    //### Prevent TSE destruction between "if" and "lock"
    if ( m_TSEInfo ) {
        m_TSEInfo->LockCounter();
    }
    return *this;
}


CAnnot_CI::~CAnnot_CI(void)
{
    if ( m_TSEInfo ) {
        m_TSEInfo->UnlockCounter();
    }
    return;
}


bool CAnnot_CI::x_ValidLocation(const CHandleRangeMap& loc) const
{
    switch (m_OverlapType) {
    case eOverlap_Intervals:
        return m_HandleRangeMap->IntersectingWithMap(loc);
    case eOverlap_TotalRange:
        return m_HandleRangeMap->TotalRangeIntersectingWith(loc);
    }
    return false;
}


bool CAnnot_CI::x_IsValid(void) const
{
    if ( !bool(m_TSEInfo)  ||  !bool(m_Current))
        return true; // Do not need to walk ahead
    if (m_Selector.m_AnnotChoice == CSeq_annot::C_Data::e_not_set)
        return true;
    const CAnnotObject& obj = *m_Current->second;
    //_TRACE("found annot: " << obj.Which() << " at " << obj.GetRangeMap());
    if (m_Selector.m_AnnotChoice != obj.Which())
        return false;

    if ( obj.IsFeat() ) {
        //_TRACE("found feat: " << obj.GetFeat().GetData().Which());
        if (m_Selector.m_FeatChoice != obj.GetFeat().GetData().Which()  &&
            m_Selector.m_FeatChoice != CSeqFeatData::e_not_set) {
            return false; // bad feature type
        }
    }

    // Check location/product flag and use corresponding map
    if ( m_Selector.m_FeatProduct ) {
        return obj.GetProductMap()  &&
            x_ValidLocation(*obj.GetProductMap());
    }
    return x_ValidLocation(obj.GetRangeMap());
}


void CAnnot_CI::x_Walk(void)
{
    _ASSERT(bool(m_TSEInfo)  &&  bool(m_Current));
    for (++m_Current; m_Current; ++m_Current) {
        if ( x_IsValid() ) {
            // Valid type and location or end of annotations
            //_TRACE("+found.");
            return;
        }
    }
    // No valid annotations found for the current handle -- try
    // to select the next one.
    CHandleRangeMap::TLocMap::const_iterator h =
        m_HandleRangeMap->GetMap().find(m_CurrentHandle);
    for (++h; h != m_HandleRangeMap->GetMap().end(); ++h) {
        if ( !h->second.Empty() ) {
            TAnnotMap::iterator ait =
                m_TSEInfo->m_AnnotMap.find(h->first);
            if (ait == m_TSEInfo->m_AnnotMap.end())
                continue;
            m_RangeMap = &ait->second;
            m_CurrentHandle = h->first;
            m_CoreRange = h->second.GetOverlappingRange();
            m_Current = m_RangeMap->begin(m_CoreRange);
            for ( ; m_Current; ++m_Current) {
                if ( x_IsValid() ) {
                    // Valid type and location or end of annotations
                    //_TRACE("+found.");
                    return;
                }
            }
        }
    }
}

END_SCOPE(objects)
END_NCBI_SCOPE

/*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.13  2003/01/22 20:11:54  vasilche
* Merged functionality of CSeqMapResolved_CI to CSeqMap_CI.
* CSeqMap_CI now supports resolution and iteration over sequence range.
* Added several caches to CScope.
* Optimized CSeqVector().
* Added serveral variants of CBioseqHandle::GetSeqVector().
* Tried to optimize annotations iterator (not much success).
* Rewritten CHandleRange and CHandleRangeMap classes to avoid sorting of list.
*
* Revision 1.12  2002/12/26 20:55:17  dicuccio
* Moved seq_id_mapper.hpp, tse_info.hpp, and bioseq_info.hpp -> include/ tree
*
* Revision 1.11  2002/12/26 16:39:23  vasilche
* Object manager class CSeqMap rewritten.
*
* Revision 1.10  2002/12/20 20:54:24  grichenk
* Added optional location/product switch to CFeat_CI
*
* Revision 1.9  2002/12/06 15:36:00  grichenk
* Added overlap type for annot-iterators
*
* Revision 1.8  2002/07/08 20:51:00  grichenk
* Moved log to the end of file
* Replaced static mutex (in CScope, CDataSource) with the mutex
* pool. Redesigned CDataSource data locking.
*
* Revision 1.7  2002/05/06 03:28:46  vakatov
* OM/OM1 renaming
*
* Revision 1.6  2002/04/05 21:26:19  grichenk
* Enabled iteration over annotations defined on segments of a
* delta-sequence.
*
* Revision 1.5  2002/02/21 19:27:04  grichenk
* Rearranged includes. Added scope history. Added searching for the
* best seq-id match in data sources and scopes. Updated tests.
*
* Revision 1.4  2002/02/15 20:35:38  gouriano
* changed implementation of HandleRangeMap
*
* Revision 1.3  2002/02/07 21:27:35  grichenk
* Redesigned CDataSource indexing: seq-id handle -> TSE -> seq/annot
*
* Revision 1.2  2002/01/16 16:25:56  gouriano
* restructured objmgr
*
* Revision 1.1  2002/01/11 19:06:15  gouriano
* restructured objmgr
*
*
* ===========================================================================
*/
