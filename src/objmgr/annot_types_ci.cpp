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
*  to the public
 for use. The National Library of Medicine and the U.S.
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
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.11  2002/04/05 21:26:19  grichenk
* Enabled iteration over annotations defined on segments of a
* delta-sequence.
*
* Revision 1.10  2002/03/07 21:25:33  grichenk
* +GetSeq_annot() in annotation iterators
*
* Revision 1.9  2002/03/05 16:08:14  grichenk
* Moved TSE-restriction to new constructors
*
* Revision 1.8  2002/03/04 15:07:48  grichenk
* Added "bioseq" argument to CAnnotTypes_CI constructor to iterate
* annotations from a single TSE.
*
* Revision 1.7  2002/02/21 19:27:05  grichenk
* Rearranged includes. Added scope history. Added searching for the
* best seq-id match in data sources and scopes. Updated tests.
*
* Revision 1.6  2002/02/15 20:35:38  gouriano
* changed implementation of HandleRangeMap
*
* Revision 1.5  2002/02/07 21:27:35  grichenk
* Redesigned CDataSource indexing: seq-id handle -> TSE -> seq/annot
*
* Revision 1.4  2002/01/23 21:59:31  grichenk
* Redesigned seq-id handles and mapper
*
* Revision 1.3  2002/01/18 15:51:18  gouriano
* *** empty log message ***
*
* Revision 1.2  2002/01/16 16:25:57  gouriano
* restructured objmgr
*
* Revision 1.1  2002/01/11 19:06:17  gouriano
* restructured objmgr
*
*
* ===========================================================================
*/

#include "annot_object.hpp"
#include <objects/objmgr1/annot_types_ci.hpp>
#include "data_source.hpp"
#include "tse_info.hpp"
#include "handle_range_map.hpp"
#include <objects/objmgr1/scope.hpp>
#include <objects/seqloc/Seq_loc.hpp>
#include <objects/seqloc/Seq_interval.hpp>
#include <objects/seqloc/Seq_point.hpp>
#include <objects/seqloc/Seq_loc_equiv.hpp>
#include <objects/seqloc/Seq_bond.hpp>
#include <serial/typeinfo.hpp>


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


CAnnotTypes_CI::CAnnotTypes_CI(void)
{
    return;
}


CAnnotTypes_CI::CAnnotTypes_CI(CScope& scope,
                               const CSeq_loc& loc,
                               SAnnotSelector selector,
                               EResolveMethod resolve)
    : m_Selector(selector),
      m_AnnotCopy(0),
      m_Scope(&scope)
{
    AutoPtr<SAnnotSearchData> sdata(new SAnnotSearchData);
    sdata->m_Location.reset(new CHandleRangeMap(m_Scope->x_GetIdMapper()));
    sdata->m_Location->AddLocation(loc);
    m_SearchQueue.push_back(sdata);

    if (resolve == eResolve_All)
        x_ResolveReferences(*sdata->m_Location, 0, -1, -1);

    sdata->m_Master = true;
    // Search all possible TSEs
    // PopulateTSESet must be called AFTER resolving
    // references -- otherwise duplicate features may be
    // found.
    m_Scope->x_PopulateTSESet(*sdata->m_Location, sdata->m_Entries);
    non_const_iterate(TTSESet, tse_it, sdata->m_Entries) {
        (*tse_it)->Lock();
    }

    while ( !m_SearchQueue.empty() ) {
        m_CurrentTSE = m_SearchQueue.front()->m_Entries.begin();
        for ( ; m_CurrentTSE != m_SearchQueue.front()->m_Entries.end(); ++m_CurrentTSE) {
            m_CurrentAnnot = CAnnot_CI(**m_CurrentTSE,
                *m_SearchQueue.front()->m_Location, m_Selector);
            if ( m_CurrentAnnot )
                break;
        }
        if ( m_CurrentAnnot )
            break;
        x_PopTSESet();
    }
}


CAnnotTypes_CI::CAnnotTypes_CI(CBioseq_Handle& bioseq,
                               int start, int stop,
                               SAnnotSelector selector,
                               EResolveMethod resolve)
    : m_Selector(selector),
      m_AnnotCopy(0),
      m_Scope(bioseq.m_Scope)
{
    AutoPtr<SAnnotSearchData> sdata(new SAnnotSearchData);
    sdata->m_Location.reset(new CHandleRangeMap(m_Scope->x_GetIdMapper()));
    CRef<CSeq_loc> loc(new CSeq_loc);
    CRef<CSeq_id> id(new CSeq_id);
    SerialAssign<CSeq_id>(*id, *bioseq.GetSeqId());
    if ( start == 0  &&  stop == 0 ) {
        loc->SetWhole(*id);
    }
    else {
        loc->SetInt().SetId(*id);
        loc->SetInt().SetFrom(start);
        loc->SetInt().SetTo(stop);
    }
    sdata->m_Location->AddLocation(*loc);
    sdata->m_Master = true;
    bioseq.m_TSE->Lock();
    sdata->m_Entries.insert(bioseq.m_TSE);
    m_SearchQueue.push_back(sdata);

    if (resolve == eResolve_All)
        x_ResolveReferences(*sdata->m_Location, 0, -1, -1);

    while ( !m_SearchQueue.empty() ) {
        m_CurrentTSE = m_SearchQueue.front()->m_Entries.begin();
        for ( ; m_CurrentTSE != m_SearchQueue.front()->m_Entries.end(); ++m_CurrentTSE) {
            m_CurrentAnnot = CAnnot_CI(**m_CurrentTSE,
                *m_SearchQueue.front()->m_Location, m_Selector);
            if ( m_CurrentAnnot )
                break;
        }
        if ( m_CurrentAnnot )
            break;
        x_PopTSESet();
    }
}


CAnnotTypes_CI::CAnnotTypes_CI(const CAnnotTypes_CI& it)
    : m_Selector(it.m_Selector),
      m_CurrentAnnot(it.m_CurrentAnnot),
      m_AnnotCopy(0)
{
    iterate (TAnnotSearchQueue, qit, it.m_SearchQueue) {
        AutoPtr<SAnnotSearchData> sdata(new SAnnotSearchData);
        sdata->m_Location.reset(new CHandleRangeMap(*(*qit)->m_Location));
        sdata->m_RefShift = (*qit)->m_RefShift;
        sdata->m_RefId = (*qit)->m_RefId;
        sdata->m_Master = (*qit)->m_Master;
        iterate (TTSESet, itr, (*qit)->m_Entries) {
            TTSESet::const_iterator cur = sdata->m_Entries.insert(*itr).first;
            (*cur)->Lock();
            if (*itr == *it.m_CurrentTSE)
                m_CurrentTSE = cur;
        }
        m_SearchQueue.push_back(sdata);
    }
}


CAnnotTypes_CI::~CAnnotTypes_CI(void)
{
    while ( !m_SearchQueue.empty() ) {
        x_PopTSESet();
    }
}


CAnnotTypes_CI& CAnnotTypes_CI::operator= (const CAnnotTypes_CI& it)
{
    {{
        CMutexGuard guard(CDataSource::sm_DataSource_Mutex);
        while ( !m_SearchQueue.empty() ) {
            x_PopTSESet();
        }
    }}
    m_Selector = it.m_Selector;
    m_CurrentAnnot = it.m_CurrentAnnot;
    m_AnnotCopy.Reset();
    iterate (TAnnotSearchQueue, qit, it.m_SearchQueue) {
        AutoPtr<SAnnotSearchData> sdata(new SAnnotSearchData);
        sdata->m_Location.reset(new CHandleRangeMap(*(*qit)->m_Location));
        sdata->m_RefShift = (*qit)->m_RefShift;
        sdata->m_RefId = (*qit)->m_RefId;
        sdata->m_Master = (*qit)->m_Master;
        iterate (TTSESet, itr, (*qit)->m_Entries) {
            TTSESet::const_iterator cur = sdata->m_Entries.insert(*itr).first;
            (*cur)->Lock();
            if (*itr == *it.m_CurrentTSE)
                m_CurrentTSE = cur;
        }
        m_SearchQueue.push_back(sdata);
    }
    return *this;
}


bool CAnnotTypes_CI::IsValid(void) const
{
    return ( !m_SearchQueue.empty()  &&  m_CurrentAnnot );
}


void CAnnotTypes_CI::Walk(void)
{
    _ASSERT(m_CurrentAnnot);
    CMutexGuard guard(CScope::sm_Scope_Mutex);
    m_AnnotCopy.Reset();
    ++m_CurrentAnnot;
    if ( m_CurrentAnnot )
        return; // Found the next annotation
    while ( !m_CurrentAnnot  &&  !m_SearchQueue.empty() ) {
        // Search for the next data source with annots
        ++m_CurrentTSE;
        while (m_CurrentTSE == m_SearchQueue.front()->m_Entries.end()) {
            x_PopTSESet();
            if ( m_SearchQueue.empty() )
                return;
            m_CurrentTSE = m_SearchQueue.front()->m_Entries.begin();
        }
        m_CurrentAnnot = CAnnot_CI(**m_CurrentTSE,
            *m_SearchQueue.front()->m_Location, m_Selector);
    }
}


CAnnotObject* CAnnotTypes_CI::Get(void) const
{
    _ASSERT( IsValid() );
    if ( m_SearchQueue.front()->m_Master )
        return &*m_CurrentAnnot;
    return x_ConvertAnnotToMaster(*m_CurrentAnnot, *m_SearchQueue.front());
}


const CSeq_annot& CAnnotTypes_CI::GetSeq_annot(void) const
{
    _ASSERT( IsValid() );
    return m_CurrentAnnot->GetSeq_annot();
}


void CAnnotTypes_CI::x_PopTSESet(void)
{
    if ( m_SearchQueue.empty() )
        return;
    non_const_iterate(TTSESet, tse_it, m_SearchQueue.front()->m_Entries) {
        (*tse_it)->Unlock();
    }
    m_SearchQueue.pop_front();
}


void CAnnotTypes_CI::x_ResolveReferences(CHandleRangeMap& loc,
                                         int shift,
                                         int min, int max,
                                         CSeq_id_Handle* master_id)
{
    // Resolve references for a segmented bioseq, get features for
    // the segments.
    iterate(CHandleRangeMap::TLocMap, hit, loc.GetMap()) {
        // Process each seq-id handle
        CSeq_id_Handle h = hit->first;
        CBioseq_Handle bsh = m_Scope->GetBioseqHandle(
            m_Scope->x_GetIdMapper().GetSeq_id(h));
        const CSeqMap& smap = bsh.GetSeqMap();
        iterate(CHandleRange::TRanges, rit, hit->second.GetRanges()) {
            for (size_t i = 0; i < smap.size(); i++) {
                CHandleRange::TRange rg(smap[i].m_Position,
                    smap[i].m_Position + smap[i].m_Length - 1);
                if (rg.IntersectingWithPossiblyEmpty(rit->first)  &&
                    smap[i].m_SegType == CSeqMap::eSeqRef) {
                    int rmin = min > rg.GetFrom() ? min : rg.GetFrom();
                    // rg may not have infinite end values since it's
                    // constructed from the map
                    int rmax = (max < 0 || max > rg.GetTo()) ? rg.GetTo() : max;
                    AutoPtr<SAnnotSearchData> sdata(new SAnnotSearchData);
                    sdata->m_Location.reset(new CHandleRangeMap(
                        m_Scope->x_GetIdMapper()));
                    CRef<CSeq_loc> seqloc = new CSeq_loc;
                    SerialAssign<CSeq_id>(seqloc->SetInt().SetId(),
                        m_Scope->x_GetIdMapper().GetSeq_id(smap[i].m_RefSeq));
                    seqloc->SetInt().SetFrom(rg.GetFrom());
                    seqloc->SetInt().SetTo(rg.GetTo());
                    sdata->m_Location->AddLocation(*seqloc);
                    sdata->m_Master = false;
                    int dshift = smap[i].m_Position - smap[i].m_RefPos;
                    sdata->m_RefShift = shift + dshift;
                    // rmin && rmax must be converted to the reference coordinates
                    sdata->m_RefMin = rmin - dshift;
                    sdata->m_RefMax = rmax - dshift;
                    sdata->m_MasterId = *(master_id ? master_id : &smap[i].m_RefSeq);
                    sdata->m_RefId = smap[i].m_RefSeq;
                    m_SearchQueue.push_back(sdata);
                    x_ResolveReferences(*sdata->m_Location,
                        sdata->m_RefShift, sdata->m_RefMin, sdata->m_RefMax,
                        &sdata->m_MasterId);
                    // PopulateTSESet must be called AFTER resolving
                    // references -- otherwise duplicate features may be
                    // found.
                    m_Scope->x_PopulateTSESet(*sdata->m_Location, sdata->m_Entries);
                    non_const_iterate(TTSESet, tse_it, sdata->m_Entries) {
                        (*tse_it)->Lock();
                    }
                }
            }
        }
    }
}


CAnnotObject* CAnnotTypes_CI::x_ConvertAnnotToMaster(CAnnotObject& annot_obj,
                                                     const SAnnotSearchData& sdata) const
{
    if ( m_AnnotCopy )
        // Already have converted version
        return m_AnnotCopy.GetPointer();

    switch ( annot_obj.Which() ) {
    case CSeq_annot::C_Data::e_Ftable:
        {
            const CSeq_feat& fsrc = annot_obj.GetFeat();
            // Process feature location
            CRef<CSeq_feat> fcopy = new CSeq_feat;
            SerialAssign<CSeq_feat>(*fcopy, fsrc);
            x_ConvertLocToMaster(fcopy->SetLocation(), sdata);
            if ( fcopy->IsSetProduct() )
                x_ConvertLocToMaster(fcopy->SetProduct(), sdata);
            m_AnnotCopy.Reset(new CAnnotObject(*fcopy, annot_obj.GetSeq_annot()));
            break;
        }
    case CSeq_annot::C_Data::e_Align:
        {
            break;
        }
    case CSeq_annot::C_Data::e_Graph:
        {
            break;
        }
    default:
        {
            LOG_POST(Warning << "CAnnotTypes_CI -- annotation type not implemented.");
            break;
        }
    }
    _ASSERT(m_AnnotCopy);
    return m_AnnotCopy.GetPointer();
}


void CAnnotTypes_CI::x_ConvertLocToMaster(CSeq_loc& loc,
                                          const SAnnotSearchData& sdata) const
{
    switch ( loc.Which() ) {
    case CSeq_loc::e_not_set:
    case CSeq_loc::e_Null:
    case CSeq_loc::e_Empty:
    case CSeq_loc::e_Feat:
        {
            // Nothing to do
            return;
        }
    case CSeq_loc::e_Whole:
        {
            if (m_Scope->x_GetIdMapper().GetHandle(loc.GetWhole()) != sdata.m_RefId)
                return;
            // Convert to the allowed master seq interval
            SerialAssign<CSeq_id>(loc.SetInt().SetId(),
                m_Scope->x_GetIdMapper().GetSeq_id(sdata.m_MasterId));
            loc.SetInt().SetFrom(sdata.m_RefMin);
            loc.SetInt().SetTo(sdata.m_RefMax);
            return;
        }
    case CSeq_loc::e_Int:
        {
            if (m_Scope->x_GetIdMapper().
                GetHandle(loc.GetInt().GetId()) != sdata.m_RefId)
                return;
            // Convert to the allowed master seq interval
            SerialAssign<CSeq_id>(loc.SetInt().SetId(),
                m_Scope->x_GetIdMapper().GetSeq_id(sdata.m_MasterId));
            int new_from = loc.GetInt().GetFrom() + sdata.m_RefShift;
            int new_to = loc.GetInt().GetTo() + sdata.m_RefShift;
            if (new_from < sdata.m_RefMin)
                new_from = sdata.m_RefMin;
            if (new_to < sdata.m_RefMax)
                new_to = sdata.m_RefMax;
            loc.SetInt().SetFrom(new_from);
            loc.SetInt().SetTo(new_to);
            return;
        }
    case CSeq_loc::e_Pnt:
        {
            if (m_Scope->x_GetIdMapper().
                GetHandle(loc.GetPnt().GetId()) != sdata.m_RefId)
                return;
            // Convert to the allowed master seq interval
            int new_pnt = loc.GetPnt().GetPoint() + sdata.m_RefShift;
            if (new_pnt < sdata.m_RefMin  ||  new_pnt > sdata.m_RefMax) {
                loc.SetEmpty();
                return;
            }
            SerialAssign<CSeq_id>(loc.SetPnt().SetId(),
                m_Scope->x_GetIdMapper().GetSeq_id(sdata.m_MasterId));
            loc.SetPnt().SetPoint(new_pnt);
            return;
        }
    case CSeq_loc::e_Packed_int:
        {
            non_const_iterate ( CPacked_seqint::Tdata, ii, loc.SetPacked_int().Set() ) {
                if (m_Scope->x_GetIdMapper().
                    GetHandle((*ii)->GetId()) != sdata.m_RefId)
                    continue;
                // Convert to the allowed master seq interval
                SerialAssign<CSeq_id>((*ii)->SetId(),
                    m_Scope->x_GetIdMapper().GetSeq_id(sdata.m_MasterId));
                int new_from = (*ii)->GetFrom() + sdata.m_RefShift;
                int new_to = (*ii)->GetTo() + sdata.m_RefShift;
                if (new_from < sdata.m_RefMin)
                    new_from = sdata.m_RefMin;
                if (new_to < sdata.m_RefMax)
                    new_to = sdata.m_RefMax;
                (*ii)->SetFrom(new_from);
                (*ii)->SetTo(new_to);
            }
            return;
        }
    case CSeq_loc::e_Packed_pnt:
        {
            if (m_Scope->x_GetIdMapper().
                GetHandle(loc.GetPacked_pnt().GetId()) != sdata.m_RefId)
                return;
            SerialAssign<CSeq_id>(loc.SetPacked_pnt().SetId(),
                m_Scope->x_GetIdMapper().GetSeq_id(sdata.m_MasterId));
            CPacked_seqpnt::TPoints pnt_copy(loc.SetPacked_pnt().SetPoints());
            loc.SetPacked_pnt().ResetPoints();
            non_const_iterate ( CPacked_seqpnt::TPoints, pi, pnt_copy ) {
                // Convert to the allowed master seq interval
                int new_pnt = *pi + sdata.m_RefShift;
                if (new_pnt >= sdata.m_RefMin  &&  new_pnt <= sdata.m_RefMax) {
                    loc.SetPacked_pnt().SetPoints().push_back(*pi);
                }
            }
            return;
        }
    case CSeq_loc::e_Mix:
        {
            non_const_iterate ( CSeq_loc_mix::Tdata, li, loc.SetMix().Set() ) {
                x_ConvertLocToMaster(**li, sdata);
            }
            return;
        }
    case CSeq_loc::e_Equiv:
        {
            non_const_iterate ( CSeq_loc_equiv::Tdata, li, loc.SetEquiv().Set() ) {
                x_ConvertLocToMaster(**li, sdata);
            }
            return;
        }
    case CSeq_loc::e_Bond:
        {
            CSeq_id_Handle hA = m_Scope->x_GetIdMapper().
                GetHandle(loc.GetBond().GetA().GetId());
            CSeq_id_Handle hB;
            bool corrected = false;
            if ( loc.GetBond().IsSetB() )
                hB = m_Scope->x_GetIdMapper().
                    GetHandle(loc.GetBond().GetB().GetId());
            if (hA == sdata.m_RefId) {
                // Convert A to the allowed master seq interval
                SerialAssign<CSeq_id>(loc.SetBond().SetA().SetId(),
                    m_Scope->x_GetIdMapper().GetSeq_id(sdata.m_MasterId));
                int newA = loc.GetBond().GetA().GetPoint() + sdata.m_RefShift;
                if (newA >= sdata.m_RefMin  &&  newA <= sdata.m_RefMax) {
                    loc.SetBond().SetA().SetPoint(newA);
                    corrected = true;
                }
            }
            if (hB == sdata.m_RefId) {
                // Convert B to the allowed master seq interval
                SerialAssign<CSeq_id>(loc.SetBond().SetB().SetId(),
                    m_Scope->x_GetIdMapper().GetSeq_id(sdata.m_MasterId));
                int newB = loc.GetBond().GetB().GetPoint() + sdata.m_RefShift;
                if (newB >= sdata.m_RefMin  &&  newB <= sdata.m_RefMax) {
                    loc.SetBond().SetB().SetPoint(newB);
                    corrected = true;
                }
            }
            if ( !corrected )
                loc.SetEmpty();
            return;
        }
    default:
        {
            throw runtime_error("CAnnotTypes_CI -- unsupported location type");
        }
    }
}



END_SCOPE(objects)
END_NCBI_SCOPE
