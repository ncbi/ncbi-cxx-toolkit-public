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
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.14  2002/04/17 21:11:59  grichenk
* Fixed annotations loading
* Set "partial" flag in features if necessary
* Implemented most seq-loc types in reference resolving methods
* Fixed searching for annotations within a signle TSE
*
* Revision 1.13  2002/04/12 19:32:20  grichenk
* Removed temp. patch for SerialAssign<>()
*
* Revision 1.12  2002/04/11 12:07:29  grichenk
* Redesigned CAnnotTypes_CI to resolve segmented sequences correctly.
*
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
#include <objects/objmgr1/seq_vector.hpp>
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
      m_Scope(&scope),
      m_NativeTSE(0)
{
    x_Initialize(loc, resolve);
}


CAnnotTypes_CI::CAnnotTypes_CI(CBioseq_Handle& bioseq,
                               int start, int stop,
                               SAnnotSelector selector,
                               EResolveMethod resolve)
    : m_Selector(selector),
      m_AnnotCopy(0),
      m_Scope(bioseq.m_Scope),
      m_NativeTSE(bioseq.m_TSE)
{
    // Convert interval to the seq-loc
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
    x_Initialize(*loc, resolve);
}

void CAnnotTypes_CI::x_Initialize(const CSeq_loc& loc, EResolveMethod resolve)
{
    CHandleRangeMap master_loc(m_Scope->x_GetIdMapper());
    master_loc.AddLocation(loc);
    iterate(CHandleRangeMap::TLocMap, id_it, master_loc.GetMap()) {
        // Iterate intervals for the current id, resolve each interval
        iterate(CHandleRange::TRanges, rit, id_it->second.GetRanges()) {
            // Resolve the master location
            x_ResolveReferences(id_it->first,             // master id
                id_it->first,                             // id to resolve
                rit->first.GetFrom(), rit->first.GetTo(), // ref. interval
                0, resolve == eResolve_All);              // no shift
        }
    }
    m_CurAnnot = m_AnnotSet.begin();
}


CAnnotTypes_CI::CAnnotTypes_CI(const CAnnotTypes_CI& it)
{
    *this = it;
}


CAnnotTypes_CI::~CAnnotTypes_CI(void)
{
    non_const_iterate (TTSESet, tse_it, m_TSESet) {
        (*tse_it)->Unlock();
    }
}


CAnnotTypes_CI& CAnnotTypes_CI::operator= (const CAnnotTypes_CI& it)
{
    {{
        CMutexGuard guard(CDataSource::sm_DataSource_Mutex);
        non_const_iterate (TTSESet, tse_it, m_TSESet) {
            (*tse_it)->Unlock();
        }
    }}
    m_Selector = it.m_Selector;
    m_Scope = it.m_Scope;

    // Copy TSE list, set TSE locks
    iterate (TTSESet, tse_it, it.m_TSESet) {
        m_TSESet.insert(*tse_it);
        (*tse_it)->Lock();
    }
    // Copy annotations (non_const to compare iterators)
    iterate (TAnnotSet, an_it, it.m_AnnotSet) {
        TAnnotSet::iterator cur = m_AnnotSet.insert(*an_it).first;
        if (it.m_CurAnnot == an_it) {
            m_CurAnnot = cur;
        }
    }
    // Copy convertions map
    iterate (TConvMap, cm_it, it.m_ConvMap) {
        TIdConvList& lst = m_ConvMap[cm_it->first];
        iterate (TIdConvList, cl_it, cm_it->second) {
            lst.insert(lst.end(), *cl_it);
        }
    }
    return *this;
}


void CAnnotTypes_CI::x_SearchLocation(CHandleRangeMap& loc)
{
    // Search all possible TSEs
    // PopulateTSESet must be called AFTER resolving
    // references -- otherwise duplicate features may be
    // found.
    TTSESet entries;
    m_Scope->x_PopulateTSESet(loc, m_Selector.m_AnnotChoice, entries);
    non_const_iterate(TTSESet, tse_it, entries) {
        if (bool(m_NativeTSE)  &&  *tse_it != m_NativeTSE)
            continue;
        if ( m_TSESet.insert(*tse_it).second ) {
            (*tse_it)->Lock();
            CAnnot_CI annot_it(**tse_it, loc, m_Selector);
            for ( ; annot_it; annot_it++ ) {
                m_AnnotSet.insert(&(*annot_it));
            }
        }
    }
}


bool CAnnotTypes_CI::IsValid(void) const
{
    return m_CurAnnot != m_AnnotSet.end();
}


void CAnnotTypes_CI::Walk(void)
{
    CMutexGuard guard(CScope::sm_Scope_Mutex);
    m_AnnotCopy.Reset();
    // Find the next annot + conv. id + conv. rec. combination
    while (m_CurAnnot != m_AnnotSet.end()) {
        ++m_CurAnnot;
        return; //??? Anything else?
    }
}


CAnnotObject* CAnnotTypes_CI::Get(void) const
{
    _ASSERT( IsValid() );
    return x_ConvertAnnotToMaster(**m_CurAnnot);
}


const CSeq_annot& CAnnotTypes_CI::GetSeq_annot(void) const
{
    _ASSERT( IsValid() );
    return (*m_CurAnnot)->GetSeq_annot();
}


void CAnnotTypes_CI::x_ResolveReferences(CSeq_id_Handle master_idh,
                                         CSeq_id_Handle ref_idh,
                                         int rmin, int rmax,
                                         int shift,
                                         bool resolve)
{
    // Create a new entry in the convertions map
    CRef<SConvertionRec> rec = new SConvertionRec;
    rec->m_Location.reset
        (new CHandleRangeMap(m_Scope->x_GetIdMapper()));
    CHandleRange hrg(ref_idh);
    //### Strand not implemented
    hrg.AddRange(CHandleRange::TRange(rmin, rmax), eNa_strand_unknown);
    // Create the location on the referenced sequence
    rec->m_Location->AddRanges(ref_idh, hrg);
    rec->m_RefShift = shift;
    rec->m_RefMin = rmin; //???
    rec->m_RefMax = rmax; //???
    rec->m_MasterId = master_idh;
    rec->m_Master = master_idh == ref_idh;
    m_ConvMap[ref_idh].push_back(rec);
    // Search for annotations
    x_SearchLocation(*rec->m_Location);
    if ( !resolve )
        return;
    // Resolve references for a segmented bioseq, get features for
    // the segments.
    CBioseq_Handle ref_seq = m_Scope->GetBioseqHandle(
            m_Scope->x_GetIdMapper().GetSeq_id(ref_idh));
    CSeqMap& ref_map = ref_seq.x_GetDataSource().x_GetSeqMap(ref_seq);
    ref_map.x_Resolve(rmax, *m_Scope);
    for (size_t i = ref_map.x_FindSegment(rmin); i < ref_map.size(); i++) {
        // Check each map segment for intersections with the range
        const CSeqMap::CSegmentInfo& seg = ref_map[i];
        if (rmin >= seg.GetPosition() + seg.GetLength())
            continue; // Go to the next segment
        if (rmax < seg.GetPosition())
            break; // No more intersecting segments
        if (seg.GetType() == CSeqMap::eSeqRef) {
            // Resolve the reference
            // Adjust the interval
            int seg_min = seg.GetPosition();
            int seg_max = seg_min + seg.GetLength();
            if (rmin > seg_min)
                seg_min = rmin;
            if (rmax < seg_max)
                seg_max = rmax;
            int rshift = shift + seg.GetPosition() - seg.m_RefPos;
            x_ResolveReferences(master_idh, seg.m_RefSeq,
                                seg_min - rshift, seg_max - rshift,
                                rshift,
                                resolve);
        }
    }
}


CAnnotObject*
CAnnotTypes_CI::x_ConvertAnnotToMaster(CAnnotObject& annot_obj) const
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
            if (x_ConvertLocToMaster(fcopy->SetLocation()))
                fcopy->SetPartial() = true;
            if ( fcopy->IsSetProduct() ) {
                if (x_ConvertLocToMaster(fcopy->SetProduct()))
                    fcopy->SetPartial() = true;
            }
            m_AnnotCopy.Reset(new CAnnotObject
                              (*fcopy, annot_obj.GetSeq_annot()));
            break;
        }
    case CSeq_annot::C_Data::e_Align:
        {
            LOG_POST(Warning <<
            "CAnnotTypes_CI -- seq-align convertions not implemented.");
            break;
        }
    case CSeq_annot::C_Data::e_Graph:
        {
            LOG_POST(Warning <<
            "CAnnotTypes_CI -- seq-graph convertions not implemented.");
            break;
        }
    default:
        {
            LOG_POST(Warning <<
            "CAnnotTypes_CI -- annotation type not implemented.");
            break;
        }
    }
    _ASSERT(m_AnnotCopy);
    return m_AnnotCopy.GetPointer();
}


bool CAnnotTypes_CI::x_ConvertLocToMaster(CSeq_loc& loc) const
{
    bool partial = false;
    iterate (TConvMap, convmap_it, m_ConvMap) {
        // Check seq_id
        iterate (TIdConvList, conv_it, convmap_it->second) {
            if ( !(*conv_it)->m_Location->IntersectingWithLoc(loc) )
                 continue;
            // Do not use master location intervals for convertions
            if ( (*conv_it)->m_Master )
                continue;
            switch ( loc.Which() ) {
            case CSeq_loc::e_not_set:
            case CSeq_loc::e_Null:
            case CSeq_loc::e_Empty:
            case CSeq_loc::e_Feat:
                {
                    // Nothing to do, although this should never happen --
                    // the seq_loc is intersecting with the conv. loc.
                    continue;
                }
            case CSeq_loc::e_Whole:
                {
                    // Convert to the allowed master seq interval
                    SerialAssign<CSeq_id>
                        (loc.SetInt().SetId(),
                         m_Scope->x_GetIdMapper().
                         GetSeq_id((*conv_it)->m_MasterId));
                    loc.SetInt().SetFrom
                        ((*conv_it)->m_RefMin + (*conv_it)->m_RefShift);
                    loc.SetInt().SetTo
                        ((*conv_it)->m_RefMax + (*conv_it)->m_RefShift);
                    if (loc.GetInt().GetTo() - loc.GetInt().GetFrom() + 1 <
                        m_Scope->GetBioseqHandle(loc.GetInt().
                        GetId()).GetSeqVector().size())
                        partial = true;
                    continue;
                }
            case CSeq_loc::e_Int:
                {
                    // Convert to the allowed master seq interval
                    SerialAssign<CSeq_id>
                        (loc.SetInt().SetId(),
                         m_Scope->x_GetIdMapper().
                         GetSeq_id((*conv_it)->m_MasterId));
                    int new_from = loc.GetInt().GetFrom();
                    int new_to = loc.GetInt().GetTo();
                    if (new_from < (*conv_it)->m_RefMin) {
                        new_from = (*conv_it)->m_RefMin;
                        partial = true;
                    }
                    if (new_to > (*conv_it)->m_RefMax) {
                        new_to = (*conv_it)->m_RefMax;
                        partial = true;
                    }
                    new_from += (*conv_it)->m_RefShift;
                    new_to += (*conv_it)->m_RefShift;
                    loc.SetInt().SetFrom(new_from);
                    loc.SetInt().SetTo(new_to);
                    continue;
                }
            case CSeq_loc::e_Pnt:
                {
                    SerialAssign<CSeq_id>
                        (loc.SetPnt().SetId(),
                         m_Scope->x_GetIdMapper().
                         GetSeq_id((*conv_it)->m_MasterId));
                    // Convert to the allowed master seq interval
                    int new_pnt = loc.GetPnt().GetPoint();
                    if (new_pnt < (*conv_it)->m_RefMin  ||
                        new_pnt > (*conv_it)->m_RefMax) {
                        //### Can this happen if loc is intersecting with the
                        //### conv_it location?
                        loc.SetEmpty();
                        partial = true;
                        continue;
                    }
                    loc.SetPnt().SetPoint(new_pnt + (*conv_it)->m_RefShift);
                    continue;
                }
            case CSeq_loc::e_Packed_int:
                {
                    non_const_iterate ( CPacked_seqint::Tdata, ii, loc.SetPacked_int().Set() ) {
                        CSeq_loc idloc;
                        SerialAssign<CSeq_id>(idloc.SetWhole(), (*ii)->GetId());
                        if (!(*conv_it)->m_Location->IntersectingWithLoc(idloc))
                            continue;
                        // Convert to the allowed master seq interval
                        SerialAssign<CSeq_id>((*ii)->SetId(),
                            m_Scope->x_GetIdMapper().GetSeq_id((*conv_it)->m_MasterId));
                        int new_from = (*ii)->GetFrom();
                        int new_to = (*ii)->GetTo();
                        if (new_from < (*conv_it)->m_RefMin) {
                            new_from = (*conv_it)->m_RefMin;
                            partial = true;
                        }
                        if (new_to < (*conv_it)->m_RefMax) {
                            new_to = (*conv_it)->m_RefMax;
                            partial = true;
                        }
                        (*ii)->SetFrom(new_from + (*conv_it)->m_RefShift);
                        (*ii)->SetTo(new_to + (*conv_it)->m_RefShift);
                    }
                    continue;
                }
            case CSeq_loc::e_Packed_pnt:
                {
                    SerialAssign<CSeq_id>(loc.SetPacked_pnt().SetId(),
                        m_Scope->x_GetIdMapper().GetSeq_id((*conv_it)->m_MasterId));
                    CPacked_seqpnt::TPoints pnt_copy(loc.SetPacked_pnt().SetPoints());
                    loc.SetPacked_pnt().ResetPoints();
                    non_const_iterate ( CPacked_seqpnt::TPoints, pi, pnt_copy ) {
                        // Convert to the allowed master seq interval
                        int new_pnt = *pi;
                        if (new_pnt >= (*conv_it)->m_RefMin  &&
                            new_pnt <= (*conv_it)->m_RefMax) {
                            loc.SetPacked_pnt().SetPoints().push_back
                                (*pi + (*conv_it)->m_RefShift);
                        }
                        else {
                            partial = true;
                        }
                    }
                    continue;
                }
            case CSeq_loc::e_Mix:
                {
                    non_const_iterate
                        (CSeq_loc_mix::Tdata, li, loc.SetMix().Set()) {
                        partial |= x_ConvertLocToMaster(**li);
                    }
                    continue;
                }
            case CSeq_loc::e_Equiv:
                {
                    non_const_iterate
                        (CSeq_loc_equiv::Tdata, li, loc.SetEquiv().Set()) {
                        partial |= x_ConvertLocToMaster(**li);
                    }
                    continue;
                }
            case CSeq_loc::e_Bond:
                {
                    bool corrected = false;
                    CSeq_loc idloc;
                    SerialAssign<CSeq_id>(idloc.SetWhole(), loc.GetBond().GetA().GetId());
                    if ((*conv_it)->m_Location->IntersectingWithLoc(idloc)) {
                        // Convert A to the allowed master seq interval
                        SerialAssign<CSeq_id>(loc.SetBond().SetA().SetId(),
                            m_Scope->x_GetIdMapper().GetSeq_id((*conv_it)->m_MasterId));
                        int newA = loc.GetBond().GetA().GetPoint();
                        if (newA >= (*conv_it)->m_RefMin  &&
                            newA <= (*conv_it)->m_RefMax) {
                            loc.SetBond().SetA().SetPoint(newA + (*conv_it)->m_RefShift);
                            corrected = true;
                        }
                        else {
                            partial = true;
                        }
                    }
                    if ( loc.GetBond().IsSetB() ) {
                        SerialAssign<CSeq_id>(idloc.SetWhole(), loc.GetBond().GetB().GetId());
                        if ((*conv_it)->m_Location->IntersectingWithLoc(idloc)) {
                            // Convert A to the allowed master seq interval
                            SerialAssign<CSeq_id>(loc.SetBond().SetB().SetId(),
                                m_Scope->x_GetIdMapper().GetSeq_id((*conv_it)->m_MasterId));
                            int newB = loc.GetBond().GetB().GetPoint();
                            if (newB >= (*conv_it)->m_RefMin  &&
                                newB <= (*conv_it)->m_RefMax) {
                                loc.SetBond().SetB().SetPoint(newB + (*conv_it)->m_RefShift);
                                corrected = true;
                            }
                            else {
                                partial = true;
                            }
                        }
                    }
                    if ( !corrected )
                        loc.SetEmpty();
                    continue;
                }
            default:
                {
                    throw runtime_error
                        ("CAnnotTypes_CI -- unsupported location type");
                }
            }
        }
    }
    return partial;
}



END_SCOPE(objects)
END_NCBI_SCOPE
