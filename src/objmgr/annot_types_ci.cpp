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

#include <objects/objmgr/annot_types_ci.hpp>
#include "annot_object.hpp"
#include "data_source.hpp"
#include "tse_info.hpp"
#include "handle_range_map.hpp"
#include <objects/objmgr/scope.hpp>
#include <objects/objmgr/seq_vector.hpp>
#include <objects/seqloc/Seq_loc.hpp>
#include <objects/seqloc/Seq_interval.hpp>
#include <objects/seqloc/Seq_point.hpp>
#include <objects/seqloc/Seq_loc_equiv.hpp>
#include <objects/seqloc/Seq_bond.hpp>
#include <serial/typeinfo.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


bool CAnnotObject_Less::x_CompareAnnot(const CAnnotObject& x, const CAnnotObject& y) const
{
    if ( x.IsFeat()  &&  y.IsFeat() ) {
        // CSeq_feat::operator<() may report x == y while the features
        // are different. In this case compare pointers too.
        bool lt = x.GetFeat() < y.GetFeat();
        bool gt = y.GetFeat() < x.GetFeat();
        if ( lt  ||  gt )
            return lt;
    }
    // Compare pointers
    return &x < &y;
}


CAnnotTypes_CI::CAnnotTypes_CI(void)
    : m_ResolveMethod(eResolve_None)
{
    return;
}


CAnnotTypes_CI::CAnnotTypes_CI(CScope& scope,
                               const CSeq_loc& loc,
                               SAnnotSelector selector,
                               CAnnot_CI::EOverlapType overlap_type,
                               EResolveMethod resolve)
    : m_Selector(selector),
      m_AnnotCopy(0),
      m_Scope(&scope),
      m_NativeTSE(0),
      m_ResolveMethod(resolve),
      m_OverlapType(overlap_type)
{
    try {
        x_Initialize(loc);
    }
    catch (...) {
        x_ReleaseAll();
        throw;
    }
}


CAnnotTypes_CI::CAnnotTypes_CI(const CBioseq_Handle& bioseq,
                               TSeqPos start, TSeqPos stop,
                               SAnnotSelector selector,
                               CAnnot_CI::EOverlapType overlap_type,
                               EResolveMethod resolve,
                               const CSeq_entry* entry)
    : m_Selector(selector),
      m_AnnotCopy(0),
      m_Scope(bioseq.m_Scope),
      m_NativeTSE(static_cast<CTSE_Info*>(bioseq.m_TSE)),
      m_SingleEntry(entry),
      m_ResolveMethod(resolve),
      m_OverlapType(overlap_type)
{
    // Convert interval to the seq-loc
    CRef<CSeq_loc> loc(new CSeq_loc);
    CRef<CSeq_id> id(new CSeq_id);
    id->Assign(*bioseq.GetSeqId());
    if ( start == 0  &&  stop == 0 ) {
        loc->SetWhole(*id);
    }
    else {
        loc->SetInt().SetId(*id);
        loc->SetInt().SetFrom(start);
        loc->SetInt().SetTo(stop);
    }
    try {
        x_Initialize(*loc);
    }
    catch (...) {
        x_ReleaseAll();
        throw;
    }
}


CAnnotTypes_CI::CAnnotTypes_CI(const CAnnotTypes_CI& it)
{
    try {
        *this = it;
    }
    catch (...) {
        x_ReleaseAll();
        throw;
    }
}


CAnnotTypes_CI::~CAnnotTypes_CI(void)
{
    x_ReleaseAll();
}

void CAnnotTypes_CI::x_ReleaseAll(void)
{
    non_const_iterate (TTSESet, tse_it, m_TSESet) {
        (*tse_it)->UnlockCounter();
    }
    m_TSESet.clear();
}


CAnnotTypes_CI& CAnnotTypes_CI::operator= (const CAnnotTypes_CI& it)
{
    x_ReleaseAll();
    m_Selector = it.m_Selector;
    m_Scope = it.m_Scope;
    m_ResolveMethod = it.m_ResolveMethod;
    m_AnnotSet.clear();
    // Copy TSE list, set TSE locks
    iterate (TTSESet, tse_it, it.m_TSESet) {
        if ( m_TSESet.insert(*tse_it).second ) {
            (*tse_it)->LockCounter();
        }
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


bool CAnnotTypes_CI::IsValid(void) const
{
    return m_CurAnnot != m_AnnotSet.end();
}


void CAnnotTypes_CI::Walk(void)
{
    //### m_AnnotCopy.Reset();
    // Find the next annot + conv. id + conv. rec. combination
    if (m_CurAnnot != m_AnnotSet.end()) {
        ++m_CurAnnot;
        return;
    }
}


const CAnnotObject* CAnnotTypes_CI::Get(void) const
{
    _ASSERT( IsValid() );
    return *m_CurAnnot;
}


const CSeq_annot& CAnnotTypes_CI::GetSeq_annot(void) const
{
    _ASSERT( IsValid() );
    return (*m_CurAnnot)->GetSeq_annot();
}


void CAnnotTypes_CI::x_Initialize(const CSeq_loc& loc)
{
    CHandleRangeMap master_loc(m_Scope->x_GetIdMapper());
    master_loc.AddLocation(loc);
    bool has_references = false;
    if (m_ResolveMethod != eResolve_None) {
        iterate(CHandleRangeMap::TLocMap, id_it, master_loc.GetMap()) {
/*
            CBioseq_Handle h = m_Scope->GetBioseqHandle(
                m_Scope->x_GetIdMapper().GetSeq_id(id_it->first));
            const CSeqMap& smap = h.GetSeqMap();
            for (TSeqPos seg = 0; seg < smap.size(); seg++) {
                has_references = has_references || (smap[seg].GetType() == CSeqMap::eSeqRef);
            }
            if ( !has_references )
                continue;
*/
has_references = true;
            // Iterate intervals for the current id, resolve each interval
            iterate(CHandleRange::TRanges, rit, id_it->second.GetRanges()) {
                // Resolve the master location, check if there are annotations
                // on referenced sequences.
                x_ResolveReferences(id_it->first,             // master id
                    id_it->first,                             // id to resolve
                    rit->first.GetFrom(), rit->first.GetTo(), // ref. interval
                    rit->second, 0);                          // strand, no shift
            }
        }
    }
    if ( !has_references ) {
        m_ResolveMethod = eResolve_None;
        x_SearchLocation(master_loc);
    }
    TAnnotSet orig_annots(m_AnnotSet);
    m_AnnotSet.clear();
    non_const_iterate(TAnnotSet, it, orig_annots) {
        if (m_ResolveMethod != eResolve_None) {
            m_AnnotSet.insert(CRef<CAnnotObject>
                              (x_ConvertAnnotToMaster(**it)));
        }
        else {
            m_AnnotSet.insert(*it);
        }
    }
    m_CurAnnot = m_AnnotSet.begin();
}


void CAnnotTypes_CI::x_ResolveReferences(const CSeq_id_Handle& master_idh,
                                         const CSeq_id_Handle& ref_idh,
                                         TSeqPos rmin,
                                         TSeqPos rmax,
                                         ENa_strand strand,
                                         TSignedSeqPos shift)
{
    // Create a new entry in the convertions map
    CRef<SConvertionRec> rec(new SConvertionRec);
    rec->m_Location.reset
        (new CHandleRangeMap(m_Scope->x_GetIdMapper()));
    CHandleRange hrg(ref_idh);
    hrg.AddRange(CHandleRange::TRange(rmin, rmax), strand);
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
    if (m_ResolveMethod == eResolve_None)
        return;
    // Resolve references for a segmented bioseq, get features for
    // the segments.
    CBioseq_Handle ref_seq = m_Scope->GetBioseqHandle(
            m_Scope->x_GetIdMapper().GetSeq_id(ref_idh));
    if ( !ref_seq ) {
        return;
    }
    CSeqMap& ref_map = ref_seq.x_GetDataSource().x_GetSeqMap(ref_seq);
    //ref_map.x_Resolve(rmax, *m_Scope);
    for (CSeqMap::const_iterator seg = ref_map.FindSegment(rmin, m_Scope);
         seg != ref_map.End(m_Scope); seg++) {
        // Check each map segment for intersections with the range
        //const CSeqMap::CSegmentInfo& seg = ref_map[i];
        if (rmin >= seg.GetPosition() + seg.GetLength())
            continue; // Go to the next segment
        if (rmax < seg.GetPosition())
            break; // No more intersecting segments
        if (seg.GetType() == CSeqMap::eSeqRef) {
            // Check for valid TSE
            if (m_ResolveMethod == eResolve_TSE) {
                CBioseq_Handle check_seq = m_Scope->GetBioseqHandle(
                        m_Scope->x_GetIdMapper().GetSeq_id(seg.GetRefSeqid()));
                // The referenced sequence must be in the same TSE as the master one
                CBioseq_Handle master_seq = m_Scope->GetBioseqHandle(
                        m_Scope->x_GetIdMapper().GetSeq_id(master_idh));
                if (!check_seq  ||  !master_seq  ||
                    (&master_seq.GetTopLevelSeqEntry() != &check_seq.GetTopLevelSeqEntry())) {
                    continue;
                }
            }
            // Resolve the reference
            // Adjust the interval
            TSeqPos seg_min = seg.GetPosition();
            TSeqPos seg_max = seg_min + seg.GetLength();
            if (rmin > seg_min)
                seg_min = rmin;
            if (rmax < seg_max)
                seg_max = rmax;
            TSignedSeqPos rshift =
                shift + seg.GetPosition() - seg.GetRefPosition();
            // Adjust strand
            ENa_strand adj_strand = eNa_strand_unknown;
            if ( seg.GetRefMinusStrand() ) {
                if (strand == eNa_strand_minus)
                    adj_strand = eNa_strand_plus;
                else
                    adj_strand = eNa_strand_minus;
            }
            x_ResolveReferences(master_idh, seg.GetRefSeqid(),
                seg_min - rshift, seg_max - rshift, adj_strand, rshift);
        }
    }
}


void CAnnotTypes_CI::x_SearchLocation(CHandleRangeMap& loc)
{
    // Search all possible TSEs
    TTSESet entries;
    m_Scope->x_PopulateTSESet(loc, m_Selector.m_AnnotChoice, entries);
    non_const_iterate(TTSESet, tse_it, entries) {
        if (bool(m_NativeTSE)  &&  *tse_it != m_NativeTSE)
            continue;
        if ( m_TSESet.insert(*tse_it).second ) {
            (*tse_it)->LockCounter();
        }
        CTSE_Info& tse_info = const_cast<CTSE_Info&>(tse_it->GetObject());
        CAnnot_CI annot_it(tse_info, loc, m_Selector, m_OverlapType);
        for ( ; annot_it; ++annot_it ) {
            if (bool(m_SingleEntry)  &&
                (m_SingleEntry != &annot_it->GetSeq_entry())) {
                continue;
            }
            m_AnnotSet.insert(CRef<CAnnotObject>(&(*annot_it)));
        }
    }
}


CAnnotObject*
CAnnotTypes_CI::x_ConvertAnnotToMaster(const CAnnotObject& annot_obj) const
{
/*
    if ( m_AnnotCopy )
        // Already have converted version
        return m_AnnotCopy.GetPointer();

*/
    switch ( annot_obj.Which() ) {
    case CSeq_annot::C_Data::e_Ftable:
        {
            const CSeq_feat& fsrc = annot_obj.GetFeat();
            // Process feature location
            CRef<CSeq_feat> fcopy(new CSeq_feat);
            fcopy->Assign(fsrc);
            if (x_ConvertLocToMaster(fcopy->SetLocation()))
                fcopy->SetPartial() = true;
            if ( fcopy->IsSetProduct() ) {
                if (x_ConvertLocToMaster(fcopy->SetProduct()))
                    fcopy->SetPartial() = true;
            }
            m_AnnotCopy.Reset(new CAnnotObject
                              (*fcopy, annot_obj.GetSeq_annot(),
                              &annot_obj.GetSeq_entry()));
            break;
        }
    case CSeq_annot::C_Data::e_Align:
        {
            LOG_POST(Warning <<
            "CAnnotTypes_CI -- seq-align convertions not implemented.");
            m_AnnotCopy.Reset(const_cast<CAnnotObject*>(&annot_obj));
            break;
        }
    case CSeq_annot::C_Data::e_Graph:
        {
            const CSeq_graph& gsrc = annot_obj.GetGraph();
            // Process graph location
            CRef<CSeq_graph> gcopy(new CSeq_graph);
            gcopy->Assign(gsrc);
            x_ConvertLocToMaster(gcopy->SetLoc());
            m_AnnotCopy.Reset(new CAnnotObject
                              (*gcopy, annot_obj.GetSeq_annot(),
                              &annot_obj.GetSeq_entry()));
            break;
        }
    default:
        {
            LOG_POST(Warning <<
            "CAnnotTypes_CI -- annotation type not implemented.");
            m_AnnotCopy.Reset(const_cast<CAnnotObject*>(&annot_obj));
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
                    loc.SetInt().SetId().Assign(m_Scope->x_GetIdMapper().
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
                    loc.SetInt().SetId().Assign(m_Scope->x_GetIdMapper().
                         GetSeq_id((*conv_it)->m_MasterId));
                    TSeqPos new_from = loc.GetInt().GetFrom();
                    TSeqPos new_to = loc.GetInt().GetTo();
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
                    loc.SetPnt().SetId().Assign(m_Scope->x_GetIdMapper().
                         GetSeq_id((*conv_it)->m_MasterId));
                    // Convert to the allowed master seq interval
                    TSeqPos new_pnt = loc.GetPnt().GetPoint();
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
                        idloc.SetWhole().Assign((*ii)->GetId());
                        if (!(*conv_it)->m_Location->IntersectingWithLoc(idloc))
                            continue;
                        // Convert to the allowed master seq interval
                        (*ii)->SetId().Assign(
                            m_Scope->x_GetIdMapper().GetSeq_id((*conv_it)->m_MasterId));
                        TSeqPos new_from = (*ii)->GetFrom();
                        TSeqPos new_to = (*ii)->GetTo();
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
                        (*ii)->SetFrom(new_from);
                        (*ii)->SetTo(new_to);
                    }
                    continue;
                }
            case CSeq_loc::e_Packed_pnt:
                {
                    loc.SetPacked_pnt().SetId().Assign(
                        m_Scope->x_GetIdMapper().GetSeq_id((*conv_it)->m_MasterId));
                    CPacked_seqpnt::TPoints pnt_copy(loc.SetPacked_pnt().SetPoints());
                    loc.SetPacked_pnt().ResetPoints();
                    non_const_iterate ( CPacked_seqpnt::TPoints, pi, pnt_copy ) {
                        // Convert to the allowed master seq interval
                        TSeqPos new_pnt = *pi;
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
                    idloc.SetWhole().Assign(loc.GetBond().GetA().GetId());
                    if ((*conv_it)->m_Location->IntersectingWithLoc(idloc)) {
                        // Convert A to the allowed master seq interval
                        loc.SetBond().SetA().SetId().Assign(
                            m_Scope->x_GetIdMapper().GetSeq_id((*conv_it)->m_MasterId));
                        TSeqPos newA = loc.GetBond().GetA().GetPoint();
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
                        idloc.SetWhole().Assign(loc.GetBond().GetB().GetId());
                        if ((*conv_it)->m_Location->IntersectingWithLoc(idloc)) {
                            // Convert A to the allowed master seq interval
                            loc.SetBond().SetB().SetId().Assign(
                                m_Scope->x_GetIdMapper().GetSeq_id((*conv_it)->m_MasterId));
                            TSeqPos newB = loc.GetBond().GetB().GetPoint();
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
                    THROW1_TRACE(runtime_error,
                        "CAnnotTypes_CI::x_ConvertLocToMaster() -- "
                        "Unsupported location type");
                }
            }
        }
    }
    return partial;
}



END_SCOPE(objects)
END_NCBI_SCOPE

/*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.31  2002/12/26 16:39:23  vasilche
* Object manager class CSeqMap rewritten.
*
* Revision 1.30  2002/12/24 15:42:45  grichenk
* CBioseqHandle argument to annotation iterators made const
*
* Revision 1.29  2002/12/19 20:15:28  grichenk
* Fixed code formatting
*
* Revision 1.28  2002/12/06 15:36:00  grichenk
* Added overlap type for annot-iterators
*
* Revision 1.27  2002/12/05 19:28:32  grichenk
* Prohibited postfix operator ++()
*
* Revision 1.26  2002/11/04 21:29:11  grichenk
* Fixed usage of const CRef<> and CRef<> constructor
*
* Revision 1.25  2002/10/08 18:57:30  grichenk
* Added feature sorting to the iterator class.
*
* Revision 1.24  2002/07/08 20:51:01  grichenk
* Moved log to the end of file
* Replaced static mutex (in CScope, CDataSource) with the mutex
* pool. Redesigned CDataSource data locking.
*
* Revision 1.23  2002/05/31 17:53:00  grichenk
* Optimized for better performance (CTSE_Info uses atomic counter,
* delayed annotations indexing, no location convertions in
* CAnnot_Types_CI if no references resolution is required etc.)
*
* Revision 1.22  2002/05/24 14:58:55  grichenk
* Fixed Empty() for unsigned intervals
* SerialAssign<>() -> CSerialObject::Assign()
* Improved performance for eResolve_None case
*
* Revision 1.21  2002/05/09 14:17:22  grichenk
* Added unresolved references checking
*
* Revision 1.20  2002/05/06 03:28:46  vakatov
* OM/OM1 renaming
*
* Revision 1.19  2002/05/03 21:28:08  ucko
* Introduce T(Signed)SeqPos.
*
* Revision 1.18  2002/05/02 20:43:15  grichenk
* Improved strand processing, throw -> THROW1_TRACE
*
* Revision 1.17  2002/04/30 14:30:44  grichenk
* Added eResolve_TSE flag in CAnnot_Types_CI, made it default
*
* Revision 1.16  2002/04/23 15:18:33  grichenk
* Fixed: missing features on segments and packed-int convertions
*
* Revision 1.15  2002/04/22 20:06:17  grichenk
* Minor changes in private interface
*
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
