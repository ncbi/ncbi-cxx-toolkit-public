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
*   DataSource for object manager
*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.29  2002/04/03 18:06:47  grichenk
* Fixed segmented sequence bugs (invalid positioning of literals
* and gaps). Improved CSeqVector performance.
*
* Revision 1.27  2002/03/28 14:02:31  grichenk
* Added scope history checks to CDataSource::x_FindBestTSE()
*
* Revision 1.26  2002/03/22 17:24:12  gouriano
* loader-related fix in DropTSE()
*
* Revision 1.25  2002/03/21 21:39:48  grichenk
* garbage collector bugfix
*
* Revision 1.24  2002/03/20 21:24:59  gouriano
* *** empty log message ***
*
* Revision 1.23  2002/03/15 18:10:08  grichenk
* Removed CRef<CSeq_id> from CSeq_id_Handle, added
* key to seq-id map th CSeq_id_Mapper
*
* Revision 1.22  2002/03/11 21:10:13  grichenk
* +CDataLoader::ResolveConflict()
*
* Revision 1.21  2002/03/07 21:25:33  grichenk
* +GetSeq_annot() in annotation iterators
*
* Revision 1.20  2002/03/06 19:37:19  grichenk
* Fixed minor bugs and comments
*
* Revision 1.19  2002/03/05 18:44:55  grichenk
* +x_UpdateTSEStatus()
*
* Revision 1.18  2002/03/05 16:09:10  grichenk
* Added x_CleanupUnusedEntries()
*
* Revision 1.17  2002/03/04 15:09:27  grichenk
* Improved MT-safety. Added live/dead flag to CDataSource methods.
*
* Revision 1.16  2002/02/28 20:53:31  grichenk
* Implemented attaching segmented sequence data. Fixed minor bugs.
*
* Revision 1.15  2002/02/25 21:05:29  grichenk
* Removed seq-data references caching. Increased MT-safety. Fixed typos.
*
* Revision 1.14  2002/02/21 19:27:05  grichenk
* Rearranged includes. Added scope history. Added searching for the
* best seq-id match in data sources and scopes. Updated tests.
*
* Revision 1.13  2002/02/20 20:23:27  gouriano
* corrected FilterSeqid()
*
* Revision 1.12  2002/02/15 20:35:38  gouriano
* changed implementation of HandleRangeMap
*
* Revision 1.11  2002/02/12 19:41:42  grichenk
* Seq-id handles lock/unlock moved to CSeq_id_Handle 'ctors.
*
* Revision 1.10  2002/02/07 21:27:35  grichenk
* Redesigned CDataSource indexing: seq-id handle -> TSE -> seq/annot
*
* Revision 1.9  2002/02/06 21:46:11  gouriano
* *** empty log message ***
*
* Revision 1.8  2002/02/05 21:46:28  gouriano
* added FindSeqid function, minor tuneup in CSeq_id_mapper
*
* Revision 1.7  2002/01/30 22:09:28  gouriano
* changed CSeqMap interface
*
* Revision 1.6  2002/01/29 17:45:00  grichenk
* Added seq-id handles locking
*
* Revision 1.5  2002/01/28 19:44:49  gouriano
* changed the interface of BioseqHandle: two functions moved from Scope
*
* Revision 1.4  2002/01/23 21:59:31  grichenk
* Redesigned seq-id handles and mapper
*
* Revision 1.3  2002/01/18 15:56:23  gouriano
* changed TSeqMaps definition
*
* Revision 1.2  2002/01/16 16:25:57  gouriano
* restructured objmgr
*
* Revision 1.1  2002/01/11 19:06:18  gouriano
* restructured objmgr
*
*
* ===========================================================================
*/


#include "data_source.hpp"

#include "annot_object.hpp"
#include "handle_range_map.hpp"
#include <objects/general/Int_fuzz.hpp>
#include <objects/seq/Seq_descr.hpp>
#include <objects/seq/Seq_ext.hpp>
#include <objects/seq/Seq_hist.hpp>
#include <objects/seq/Seg_ext.hpp>
#include <objects/seq/Ref_ext.hpp>
#include <objects/seq/Delta_ext.hpp>
#include <objects/seq/Delta_seq.hpp>
#include <objects/seq/Seq_literal.hpp>
#include <objects/seq/Seq_inst.hpp>
#include <objects/seqset/Seq_entry.hpp>
#include <objects/seqset/Bioseq_set.hpp>
#include <objects/seqloc/Seq_loc.hpp>
#include <objects/seqloc/Seq_interval.hpp>
#include <objects/seqloc/Seq_point.hpp>
#include <objects/seqloc/Seq_loc_equiv.hpp>
#include <objects/seq/seqport_util.hpp>
#include <serial/iterator.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

CMutex CDataSource::sm_DataSource_Mutex;

CDataSource::CDataSource(CDataLoader& loader, CObjectManager& objmgr)
    : m_Loader(&loader), m_pTopEntry(0), m_ObjMgr(&objmgr)
{
    m_Loader->SetTargetDataSource(*this);
}


CDataSource::CDataSource(CSeq_entry& entry, CObjectManager& objmgr)
    : m_Loader(0), m_pTopEntry(&entry), m_ObjMgr(&objmgr)
{
    x_AddToBioseqMap(entry, false);
}


CDataSource::~CDataSource(void)
{
    // Find and drop each TSE
    while (m_Entries.size() > 0) {
        _ASSERT( !m_Entries.begin()->second->Locked() );
        DropTSE(*(m_Entries.begin()->second->m_TSE));
    }
}


CDataLoader* CDataSource::GetDataLoader(void)
{
    return m_Loader;
}


CSeq_entry* CDataSource::GetTopEntry(void)
{
    return m_pTopEntry;
}


CTSE_Info* CDataSource::x_FindBestTSE(CSeq_id_Handle handle,
                                      const CScope::TRequestHistory& history) const
{
    CMutexGuard guard(sm_DataSource_Mutex);
    TTSEMap::const_iterator tse_set = m_TSE_seq.find(handle);
    if ( tse_set == m_TSE_seq.end() )
        return 0;
    // The map should not contain empty entries
    _ASSERT(tse_set->second.size() > 0);
    TTSESet live;
    if ( tse_set->second.size() == 1) {
        // There is only one TSE, no matter live or dead
        return *tse_set->second.begin();
    }
    CTSE_Info* from_history = 0;
    iterate(TTSESet, tse, tse_set->second) {
        // Check history
        CScope::TRequestHistory::const_iterator hst = history.find(*tse);
        if (hst != history.end()) {
            if ( from_history ) {
                throw runtime_error(
                    "CDataSource::x_FindBestTSE() -- multiple history matches");
            }
            from_history = *tse;
        }
        // Find live TSEs
        if ( !(*tse)->m_Dead ) {
            // Make sure there is only one live TSE
            live.insert(*tse);
        }
    }
    // History is always the best choice
    if (from_history)
        return from_history;

    // Check live
    if (live.size() == 1) {
        // There is only one live TSE -- ok to use it
        return *live.begin();
    }
    else if ((live.size() == 0)  &&  m_Loader.GetPointer()) {
        // No live TSEs -- try to select the best dead TSE
        CTSE_Info* best = m_Loader->ResolveConflict(handle, tse_set->second);
        if ( best ) {
            return *tse_set->second.find(best);
        }
        throw runtime_error(
            "Multiple seq-id matches found -- can not resolve to a TSE");
    }
    // Multiple live TSEs -- try to resolve the conflict (the status of some
    // TSEs may change)
    CTSE_Info* best = m_Loader->ResolveConflict(handle, live);
    if ( best ) {
        return *tse_set->second.find(best);
    }
    throw runtime_error(
        "Seq-id conflict: multiple live entries found");
}


CBioseq_Handle CDataSource::GetBioseqHandle(CScope& scope, const CSeq_id& id)
{
    CSeqMatch_Info info = BestResolve(id, scope.m_History);
    if ( !info )
        return CBioseq_Handle();
    CSeq_id_Handle idh = info.m_Handle;
    CBioseq_Handle h(idh);
    CRef<CTSE_Info> tse = info.m_TSE;
    CMutexGuard guard(sm_DataSource_Mutex);
    TBioseqMap::iterator found = tse->m_BioseqMap.find(idh);
    _ASSERT(found != tse->m_BioseqMap.end());
    h.x_ResolveTo(scope, *this,
        *found->second->m_Entry, *tse);
    scope.x_AddToHistory(*tse);
    return h;
}


void CDataSource::FilterSeqid(TSeq_id_HandleSet& setResult,
                              TSeq_id_HandleSet& setSource) const
{
    _ASSERT(&setResult != &setSource);
    // for each handle
    TSeq_id_HandleSet::iterator itHandle;
    CMutexGuard guard(sm_DataSource_Mutex);
    for( itHandle = setSource.begin(); itHandle != setSource.end(); ) {
        // if it is in my map
        if (m_TSE_seq.find(*itHandle) != m_TSE_seq.end()) {
            setResult.insert(*itHandle);
        }
        ++itHandle;
    }
}


const CBioseq& CDataSource::GetBioseq(const CBioseq_Handle& handle)
{
    // Bioseq core and TSE must be loaded if there exists a handle
    // Loader may be called to load descriptions (not included in core)
    if ( m_Loader ) {
        // Send request to the loader
        CHandleRangeMap hrm(GetIdMapper());
        CSeq_loc loc;
        CConstRef<CSeq_id> id = handle.GetSeqId();
        SerialAssign<CSeq_id>(loc.SetWhole(), *id);
        hrm.AddLocation(loc);
        m_Loader->GetRecords(hrm, CDataLoader::eBioseq);
    }
    CMutexGuard guard(sm_DataSource_Mutex);
    // the handle must be resolved to this data source
    _ASSERT(handle.m_DataSource == this);
    return handle.m_Entry->GetSeq();
}


const CSeq_entry& CDataSource::GetTSE(const CBioseq_Handle& handle)
{
    // Bioseq and TSE must be loaded if there exists a handle
    CMutexGuard guard(sm_DataSource_Mutex);
    _ASSERT(handle.m_DataSource == this);
    return *m_Entries[handle.m_Entry]->m_TSE;
}


CBioseq_Handle::TBioseqCore CDataSource::GetBioseqCore
    (const CBioseq_Handle& handle)
{
    CMutexGuard guard(sm_DataSource_Mutex);
    const CBioseq* seq = &GetBioseq(handle);

    CBioseq* seq_core = new CBioseq();
    // Assign seq members to seq_core:
    CBioseq::TId& id_list = seq_core->SetId();
    iterate ( CBioseq::TId, it, seq->GetId() ) {
        CSeq_id* id = new CSeq_id;
        SerialAssign<CSeq_id>(*id, **it);
        id_list.push_back(id);
    }
    if ( seq->IsSetDescr() )
        SerialAssign<CSeq_descr>(seq_core->SetDescr(), seq->GetDescr());
    const CSeq_inst& inst = seq->GetInst();
    CSeq_inst& inst_core = seq_core->SetInst();
    inst_core.SetRepr(inst.GetRepr());
    inst_core.SetMol(inst.GetMol());
    if ( inst.IsSetLength() )
        inst_core.SetLength(inst.GetLength());
    if ( inst.IsSetFuzz() ) {
        CInt_fuzz* fuzz = new CInt_fuzz();
        SerialAssign<CInt_fuzz>(*fuzz, inst.GetFuzz());
        inst_core.SetFuzz(*fuzz);
    }
    if ( inst.IsSetTopology() )
        inst_core.SetTopology(inst.GetTopology());
    if ( inst.IsSetStrand() )
        inst_core.SetStrand(inst.GetStrand());
    if ( inst.IsSetExt() ) {
        CSeq_ext* ext = new CSeq_ext();
        if (inst.GetExt().Which() != CSeq_ext::e_Delta) {
            // Copy the entire seq-ext
            SerialAssign<CSeq_ext>(*ext, inst.GetExt());
        }
        else {
            // Do not copy seq-data
            iterate (CDelta_ext::Tdata, it, inst.GetExt().GetDelta().Get()) {
                CDelta_seq* dseq = new CDelta_seq;
                if ( (*it)->IsLiteral() ) {
                    dseq->SetLiteral().SetLength(
                        (*it)->GetLiteral().GetLength());
                    if ( (*it)->GetLiteral().IsSetFuzz() ) {
                        SerialAssign<CInt_fuzz>(dseq->SetLiteral().SetFuzz(),
                            (*it)->GetLiteral().GetFuzz());
                    }
                }
                else {
                    SerialAssign<CDelta_seq>(*dseq, **it);
                }
            }
        }
        inst_core.SetExt(*ext);
    }
    if ( inst.IsSetHist() ) {
        CSeq_hist* hist = new CSeq_hist();
        SerialAssign<CSeq_hist>(*hist, inst.GetHist());
        inst_core.SetHist(*hist);
    }

    return seq_core;
}


const CSeqMap& CDataSource::GetSeqMap(const CBioseq_Handle& handle)
{
    CMutexGuard guard(sm_DataSource_Mutex);
    return x_GetSeqMap(handle);
}


CSeqMap& CDataSource::x_GetSeqMap(const CBioseq_Handle& handle)
{
    // Call loader first
    if ( m_Loader ) {
        // Send request to the loader
        CSeq_loc loc;
        CConstRef<CSeq_id> id = handle.GetSeqId();
        SerialAssign<CSeq_id>(loc.SetWhole(), *id);
        CHandleRangeMap hrm(GetIdMapper());
        hrm.AddLocation(loc);
        m_Loader->GetRecords(hrm, CDataLoader::eBioseq); //### or eCore???
    }

    _ASSERT(handle.m_DataSource == this);
    const CBioseq& seq = GetBioseq(handle);
    TSeqMaps::iterator found = m_SeqMaps.find(&seq);
    if (found == m_SeqMaps.end()) {
        // Create sequence map
//        if ( !m_Loader )
        {
            if ( seq.GetInst().IsSetSeq_data() || seq.GetInst().IsSetExt() ) {
                x_CreateSeqMap(seq);
            }
            else {
                throw runtime_error("Sequence map not found");
            }
        }
        found = m_SeqMaps.find(&seq);
    }
    //### Obsolete call: (*found->second).x_CalculateSegmentLengths();
    return *found->second;
}


bool CDataSource::GetSequence(const CBioseq_Handle& handle,
                              TSeqPosition point,
                              SSeqData* seq_piece,
                              CScope& scope)
{
    // Call loader first
    if ( m_Loader ) {
        // Send request to the loader
        CSeq_loc loc;
        CConstRef<CSeq_id> id = handle.GetSeqId();
        //### Whole, or Interval, or Point?
        SerialAssign<CSeq_id>(loc.SetWhole(), *id);
        CHandleRangeMap hrm(GetIdMapper());
        hrm.AddLocation(loc);
        m_Loader->GetRecords(hrm, CDataLoader::eSequence);
    }
    CMutexGuard guard(sm_DataSource_Mutex);
    if (handle.m_DataSource != this  &&  handle.m_DataSource != 0) {
        // Resolved to a different data source
        return false;
    }
    CSeq_entry* entry = handle.m_Entry;
    CBioseq_Handle rhandle = handle; // resolved handle for local use
    if ( !entry ) {
        CTSE_Info* info = x_FindBestTSE(rhandle.GetKey(), scope.m_History);
        if ( !info )
            return false;
        entry = info->m_BioseqMap[rhandle.GetKey()]->m_Entry;
        rhandle.x_ResolveTo(scope, *this, *entry, *info);
    }
    _ASSERT(entry->IsSeq());
    CBioseq& seq = entry->GetSeq();
    if ( seq.GetInst().IsSetSeq_data() ) {
        // Simple sequence -- just return seq-data
        seq_piece->dest_start = 0;
        seq_piece->src_start = 0;
        seq_piece->length = seq.GetInst().GetLength();
        seq_piece->src_data = &seq.GetInst().GetSeq_data();
        return true;
    }
    if ( seq.GetInst().IsSetExt() )
    {
        // Seq-ext: check the extension type, prepare the data
        CSeqMap& seqmap = x_GetSeqMap(rhandle);
        // Omit the last element - it is always eSeqEnd
        CSeqMap::CSegmentInfo seg = seqmap.x_Resolve(point, scope);
        switch (seg.m_SegType) {
        case CSeqMap::eSeqData:
            {
                _ASSERT(seg.m_RefData);
                seq_piece->dest_start = seg.m_Position;
                seq_piece->src_start = 0;
                seq_piece->length = seg.m_Length;
                seq_piece->src_data = seg.m_RefData;
                return true;
            }
        case CSeqMap::eSeqRef:
            {
                int shift = seg.m_RefPos - seg.m_Position;
                if ( scope.x_GetSequence(seg.m_RefSeq,
                    point + shift, seq_piece) ) {
                    int xL = seg.m_Length;
                    int delta = seg.m_RefPos -
                        seq_piece->dest_start;
                    seq_piece->dest_start = seg.m_Position;
                    if (delta < 0) {
                        // Got less then requested (delta is negative: -=)
                        seq_piece->dest_start -= delta;
                        xL += delta;
                    }
                    else {
                        // Got more than requested
                        seq_piece->src_start += delta;
                        seq_piece->length -= delta;
                    }
                    if (seq_piece->length > xL)
                        seq_piece->length = xL;
                    if ( seg.m_MinusStrand ) {
                        // Convert data, update location
                        CSeq_data* tmp = new CSeq_data;
                        CSeqportUtil::ReverseComplement(
                            *seq_piece->src_data, tmp,
                            seq_piece->src_start, seq_piece->length);
                        seq_piece->src_start = 0;
                        seq_piece->src_data = tmp;
                    }
                    return true;
                }
                break;
            }
        case CSeqMap::eSeqGap:
            {
                seq_piece->dest_start = seg.m_Position;
                seq_piece->src_start = 0;
                seq_piece->length = seg.m_Length;
                seq_piece->src_data = 0;
                break;
            }
        case CSeqMap::eSeqEnd:
            {
                throw runtime_error("Attempt to read beyond sequence end");
            }
        }
    }
    return false;
}


void CDataSource::AddTSE(CSeq_entry& se, bool dead)
{
    CMutexGuard guard(sm_DataSource_Mutex);
    x_AddToBioseqMap(se, dead);
}


CSeq_entry* CDataSource::x_FindEntry(const CSeq_entry& entry)
{
    CRef<CSeq_entry> ref(const_cast<CSeq_entry*>(&entry));
    CMutexGuard guard(sm_DataSource_Mutex);
    TEntries::iterator found = m_Entries.find(ref);
    if (found == m_Entries.end())
        return 0;
    return found->first;
}


bool CDataSource::AttachEntry(const CSeq_entry& parent, CSeq_entry& bioseq)
{
    CMutexGuard guard(sm_DataSource_Mutex);
    CSeq_entry* found = x_FindEntry(parent);
    if ( !found ) {
        return false;
    }
    CSeq_entry& entry = *found;
    _ASSERT(found  &&  entry.IsSet());

    entry.SetSet().SetSeq_set().push_back(&bioseq);

    // Re-parentize, update index
    CSeq_entry* top = found;
    while ( top->GetParentEntry() ) {
        top = top->GetParentEntry();
    }
    top->Parentize();
    // The "dead" parameter is not used here since the TSE_Info
    // structure must have been created already.
    x_IndexEntry(bioseq, *top, false);
    return true;
}


bool CDataSource::AttachMap(const CSeq_entry& bioseq, CSeqMap& seqmap)
{
    CMutexGuard guard(sm_DataSource_Mutex);
    CSeq_entry* found = x_FindEntry(bioseq);
    if ( !found ) {
        return false;
    }
    CSeq_entry& entry = *found;
    _ASSERT(entry.IsSeq());
    m_SeqMaps[&entry.GetSeq()] = &seqmap;
    return true;
}


bool CDataSource::AttachSeqData(const CSeq_entry& bioseq,
                                CDelta_seq& seq_seg,
                                TSeqPosition start,
                                TSeqLength length)
{
/*
    The function should be used mostly by data loaders. If a segment of a
    bioseq is a reference to a whole sequence, the positioning mechanism may
    not work correctly, since the length of such reference is unknown. In most
    cases "whole" reference should be the only segment of a delta-ext.
*/
    // Get non-const reference to the entry
    CMutexGuard guard(sm_DataSource_Mutex);
    CSeq_entry* found = x_FindEntry(bioseq);
    if ( !found ) {
        return false;
    }
    CSeq_entry& entry = *found;
    _ASSERT( entry.IsSeq() );
    CSeq_inst& inst = entry.SetSeq().SetInst();
    if (start == 0  &&
        inst.IsSetLength()  &&
        inst.GetLength() == length  &&
        seq_seg.IsLiteral()  &&
        seq_seg.GetLiteral().IsSetSeq_data()) {
        // Non-segmented sequence, non-reference segment -- just add seq-data
        entry.SetSeq().SetInst().SetSeq_data(
            seq_seg.SetLiteral().SetSeq_data());
        return true;
    }
    // Possibly segmented sequence. Use delta-ext.
    _ASSERT( !inst.IsSetSeq_data() );
    CDelta_ext::Tdata& delta = inst.SetExt().SetDelta().Set();
    TSeqPosition ex_lower = 0; // the last of the existing points before start
    TSeqPosition ex_upper = 0; // the first of the existing points after start
    TSeqPosition ex_cur = 0; // current position while processing the sequence
    CDelta_ext::Tdata::iterator gap = delta.end();
    non_const_iterate(CDelta_ext::Tdata, dit, delta) {
        ex_lower = ex_cur;
        if ( (*dit)->IsLiteral() ) {
            if ( (*dit)->GetLiteral().IsSetSeq_data() ) {
                ex_cur += (*dit)->GetLiteral().GetLength();
                if (ex_cur <= start)
                    continue;
                // ex_cur > start
                // This is not good - we could not find the correct
                // match for the new segment start. The start should be
                // in a gap, not in a real literal.
                throw runtime_error(
                    "CDataSource::AttachSeqData(): segment position conflict");
            }
            else {
                // No data exist - treat it like a gap
                if ((*dit)->GetLiteral().GetLength() > 0) {
                    // Known length -- check if the starting point
                    // is whithin this gap.
                    ex_cur += (*dit)->GetLiteral().GetLength();
                    if (ex_cur < start)
                        continue;
                    // The new segment must be whithin this gap.
                    ex_upper = ex_cur;
                    gap = dit;
                    break;
                }
                // Gap of 0 or unknown length
                if (ex_cur == start) {
                    ex_upper = ex_cur;
                    gap = dit;
                    break;
                }
            }
        }
        else if ( (*dit)->IsLoc() ) {
            CSeqMap dummyseqmap; // Just to calculate the seq-loc length
            x_LocToSeqMap((*dit)->GetLoc(), ex_cur, dummyseqmap);
            if (ex_cur <= start) {
                continue;
            }
            // ex_cur > start
            throw runtime_error(
                "CDataSource::AttachSeqData(): segment position conflict");
        }
        else
            throw runtime_error("CDataSource::AttachSeqData(): Invalid delta-seq type");
    }
    if ( gap != delta.end() ) {
        // Found the correct gap
        if ( ex_upper > start ) {
            // Insert the new segment before the gap
            delta.insert(gap, CRef<CDelta_seq>(&seq_seg));
        }
        else if ( ex_lower < start ) {
            // Insert the new segment after the gap
            ++gap;
            delta.insert(gap, CRef<CDelta_seq>(&seq_seg));
        }
        else {
            // Split the gap, insert the new segment between the two gaps
            CRef<CDelta_seq> gap_dup = new CDelta_seq;
            SerialAssign<CDelta_seq>(*gap_dup, **gap);
            if ((*gap)->GetLiteral().GetLength() > 0) {
                // Adjust gap lengths
                _ASSERT((*gap)->GetLiteral().GetLength() >= length);
                gap_dup->SetLiteral().SetLength(start - ex_lower);
                (*gap)->SetLiteral().SetLength(ex_upper - start - length);
            }
            // Insert new_seg before gap, and gap_dup before new_seg
            delta.insert(delta.insert(gap, CRef<CDelta_seq>(&seq_seg)), gap_dup);
        }
    }
    else {
        // No gap found -- The sequence must be empty
        _ASSERT(delta.size() == 0);
        delta.push_back(CRef<CDelta_seq>(&seq_seg));
    }
    // Replace seq-map for the updated bioseq if it was already created
    x_CreateSeqMap( entry.SetSeq() );
    return true;
}


bool CDataSource::AttachAnnot(const CSeq_entry& entry,
                           CSeq_annot& annot)
{
    CMutexGuard guard(sm_DataSource_Mutex);
    CSeq_entry* found = x_FindEntry(entry);
    if ( !found ) {
        return false;
    }

    CTSE_Info* tse = m_Entries[found];
    CBioseq_set::TAnnot* annot_list = 0;
    if ( found->IsSet() ) {
        annot_list = &found->SetSet().SetAnnot();
    }
    else {
        annot_list = &found->SetSeq().SetAnnot();
    }
    annot_list->push_back(&annot);

    switch ( annot.GetData().Which() ) {
        case CSeq_annot::C_Data::e_Ftable:
            {
                iterate ( CSeq_annot::C_Data::TFtable, fi,
                    annot.GetData().GetFtable() ) {
                    x_MapFeature(**fi, annot, *tse);
                }
                break;
            }
        case CSeq_annot::C_Data::e_Align:
            {
                iterate ( CSeq_annot::C_Data::TAlign, ai,
                    annot.GetData().GetAlign() ) {
                    x_MapAlign(**ai, annot, *tse);
                }
                break;
            }
        case CSeq_annot::C_Data::e_Graph:
            {
                iterate ( CSeq_annot::C_Data::TGraph, gi,
                    annot.GetData().GetGraph() ) {
                    x_MapGraph(**gi, annot, *tse);
                }
                break;
            }
        default:
            {
                return false;
            }
        }
    return true;
}


void CDataSource::x_AddToBioseqMap(CSeq_entry& entry, bool dead)
{
    // Search for bioseqs, add each to map
    entry.Parentize();
    x_IndexEntry(entry, entry, dead);
    x_AddToAnnotMap(entry);
}


void CDataSource::x_IndexEntry(CSeq_entry& entry, CSeq_entry& tse, bool dead)
{
    CTSE_Info* tse_info = 0;
    TEntries::iterator found_tse = m_Entries.find(&tse);
    if (found_tse == m_Entries.end()) {
        // New TSE info
        tse_info = new CTSE_Info;
        tse_info->m_TSE = &tse;
        tse_info->m_Dead = dead;
    }
    else {
        // existing TSE info
        tse_info = found_tse->second;
    }
    _ASSERT(tse_info);
    m_Entries[&entry] = tse_info;
    if ( entry.IsSeq() ) {
        CBioseq* seq = &entry.GetSeq();
        CBioseq_Info* info = new CBioseq_Info(entry);
        iterate ( CBioseq::TId, id, seq->GetId() ) {
            // Find the bioseq index
            CSeq_id_Handle key = GetIdMapper().GetHandle(**id);
            TTSESet& tse_set = m_TSE_seq[key];
            TTSESet::iterator tse_it = tse_set.begin();
            for (tse_it = tse_set.begin(); tse_it != tse_set.end(); ++tse_it) {
                if ((*tse_it)->m_TSE == &tse)
                    break;
            }
            if (tse_it == tse_set.end()) {
                tse_set.insert(tse_info);
            }
            else {
                tse_info = *tse_it;
            }
            TBioseqMap::iterator found = tse_info->m_BioseqMap.find(key);
            if ( found != tse_info->m_BioseqMap.end() ) {
                // No duplicate bioseqs in the same TSE
                CBioseq* seq2 = &found->second->m_Entry->GetSeq();
                ERR_POST(Fatal <<
                    " duplicate Bioseq: " << (*id)->DumpAsFasta());
                // _ASSERT(SerialEquals<CBioseq>(*seq, *seq2));
                // return;
            }
            else {
                // Add new seq-id synonym
                info->m_Synonyms.insert(key);
            }
        }
        iterate ( CBioseq_Info::TSynonyms, syn, info->m_Synonyms ) {
            tse_info->m_BioseqMap.insert(TBioseqMap::value_type(*syn, info));
        }
    }
    else {
        iterate ( CBioseq_set::TSeq_set, it, entry.GetSet().GetSeq_set() ) {
            x_IndexEntry(**it, tse, dead);
        }
    }
}


void CDataSource::x_AddToAnnotMap(CSeq_entry& entry)
{
    // The entry must be already in the m_Entries map
    CTSE_Info* tse = m_Entries[&entry];
    const CBioseq::TAnnot* annot_list = 0;
    if ( entry.IsSeq() ) {
        if ( entry.GetSeq().IsSetAnnot() ) {
            annot_list = &entry.GetSeq().GetAnnot();
        }
    }
    else {
        if ( entry.GetSet().IsSetAnnot() ) {
            annot_list = &entry.GetSet().GetAnnot();
        }
        iterate ( CBioseq_set::TSeq_set, it, entry.GetSet().GetSeq_set() ) {
            x_AddToAnnotMap(**it);
        }
    }
    if ( !annot_list ) {
        return;
    }
    iterate ( CBioseq::TAnnot, ai, *annot_list ) {
        switch ( (*ai)->GetData().Which() ) {
        case CSeq_annot::C_Data::e_Ftable:
            {
                iterate ( CSeq_annot::C_Data::TFtable, it,
                    (*ai)->GetData().GetFtable() ) {
                    x_MapFeature(**it, **ai, *tse);
                }
                break;
            }
        case CSeq_annot::C_Data::e_Align:
            {
                iterate ( CSeq_annot::C_Data::TAlign, it,
                    (*ai)->GetData().GetAlign() ) {
                    x_MapAlign(**it, **ai, *tse);
                }
                break;
            }
        case CSeq_annot::C_Data::e_Graph:
            {
                iterate ( CSeq_annot::C_Data::TGraph, it,
                    (*ai)->GetData().GetGraph() ) {
                    x_MapGraph(**it, **ai, *tse);
                }
                break;
            }
        }
    }
}


void CDataSource::x_CreateSeqMap(const CBioseq& seq)
{
    CSeqMap* seqmap = new CSeqMap;
    int pos = 0;
    if ( seq.GetInst().IsSetSeq_data() ) {
        _ASSERT( !seq.GetInst().IsSetExt() );
        x_DataToSeqMap(seq.GetInst().GetSeq_data(), pos,
            (seq.GetInst().IsSetLength() ? seq.GetInst().GetLength() : 0),
            *seqmap);
    }
    else if ( seq.GetInst().IsSetExt() ) {
        const CSeq_loc* loc;
        switch (seq.GetInst().GetExt().Which()) {
        case CSeq_ext::e_Seg:
            {
                iterate ( CSeg_ext::Tdata, seg_it,
                    seq.GetInst().GetExt().GetSeg().Get() ) {
                    x_LocToSeqMap(**seg_it, pos, *seqmap);
                }
                break;
            }
        case CSeq_ext::e_Ref:
            {
                loc = &seq.GetInst().GetExt().GetRef().Get();
                x_LocToSeqMap(*loc, pos, *seqmap);
                break;
            }
        case CSeq_ext::e_Map:
            {
                //### Not implemented
                _ASSERT( ("CSeq_ext::e_Map -- not implemented", 0) );
                break;
            }
        case CSeq_ext::e_Delta:
            {
                const CDelta_ext::Tdata& dlist =
                    seq.GetInst().GetExt().GetDelta().Get();
                iterate ( CDelta_ext::Tdata, it, dlist ) {
                    switch ( (*it)->Which() ) {
                    case CDelta_seq::e_Loc:
                        {
                            x_LocToSeqMap((*it)->GetLoc(), pos, *seqmap);
                            break;
                        }
                    case CDelta_seq::e_Literal:
                        {
                            if ( (*it)->GetLiteral().IsSetSeq_data() ) {
                                x_DataToSeqMap(
                                    (*it)->GetLiteral().GetSeq_data(),
                                    pos, (*it)->GetLiteral().GetLength(),
                                    *seqmap);
                            }
                            else {
                                // No data exist - treat it like a gap
                                seqmap->Add(CSeqMap::eSeqGap, pos,
                                            (*it)->GetLiteral().GetLength()); //???
                                pos += (*it)->GetLiteral().GetLength();
                            }
                            break;
                        }
                    }
                }
                break;
            }
        }
    }
    seqmap->Add(CSeqMap::eSeqEnd, pos, 0);
    m_SeqMaps[&seq] = seqmap;
}


void CDataSource::x_LocToSeqMap(const CSeq_loc& loc,
                                int& pos,
                                CSeqMap& seqmap)
{
    switch ( loc.Which() ) {
    case CSeq_loc::e_not_set:
    case CSeq_loc::e_Null:
    case CSeq_loc::e_Empty:
        {
            // Add gap
            seqmap.Add(CSeqMap::eSeqGap, pos, 0); //???
            return;
        }
    case CSeq_loc::e_Whole:
        {
            // Reference to the whole sequence - do not check its
            // length, use 0 instead.
            CSeqMap::CSegmentInfo* seg = new CSeqMap::CSegmentInfo(
                CSeqMap::eSeqRef, pos, 0, false);
            seg->m_RefPos = 0;
            seg->m_RefSeq = GetIdMapper().GetHandle(loc.GetWhole());
            seqmap.Add(*seg);
            return;
        }
    case CSeq_loc::e_Int:
        {
            bool minus_strand = loc.GetInt().IsSetStrand()  &&
                loc.GetInt().GetStrand() == eNa_strand_minus; //???
            CSeqMap::CSegmentInfo* seg = new CSeqMap::CSegmentInfo(
                CSeqMap::eSeqRef, pos, loc.GetInt().GetLength(),
                minus_strand);
            seg->m_RefSeq =
                GetIdMapper().GetHandle(loc.GetInt().GetId());
            seg->m_RefPos = loc.GetInt().GetFrom();
            seg->m_Length = loc.GetInt().GetLength();
            seqmap.Add(*seg);
            pos += loc.GetInt().GetLength();
            return;
        }
    case CSeq_loc::e_Pnt:
        {
            bool minus_strand = loc.GetPnt().IsSetStrand()  &&
                loc.GetPnt().GetStrand() == eNa_strand_minus; //???
            CSeqMap::CSegmentInfo* seg = new CSeqMap::CSegmentInfo(
                CSeqMap::eSeqRef, pos, 1, minus_strand);
            seg->m_RefSeq =
                GetIdMapper().GetHandle(loc.GetPnt().GetId());
            seg->m_RefPos = loc.GetPnt().GetPoint();
            seg->m_Length = 1;
            seqmap.Add(*seg);
            pos++;
            return;
        }
    case CSeq_loc::e_Packed_int:
        {
            iterate ( CPacked_seqint::Tdata, ii, loc.GetPacked_int().Get() ) {
                bool minus_strand = (*ii)->IsSetStrand()  &&
                    (*ii)->GetStrand() == eNa_strand_minus; //???
                CSeqMap::CSegmentInfo* seg = new CSeqMap::CSegmentInfo(
                    CSeqMap::eSeqRef, pos, (*ii)->GetLength(), minus_strand);
                seg->m_RefSeq =
                    GetIdMapper().GetHandle((*ii)->GetId());
                seg->m_RefPos = (*ii)->GetFrom();
                seg->m_Length = (*ii)->GetLength();
                seqmap.Add(*seg);
                pos += (*ii)->GetLength();
            }
            return;
        }
    case CSeq_loc::e_Packed_pnt:
        {
            bool minus_strand = loc.GetPacked_pnt().IsSetStrand()  &&
                loc.GetPacked_pnt().GetStrand() == eNa_strand_minus; //???
            iterate ( CPacked_seqpnt::TPoints, pi,
                loc.GetPacked_pnt().GetPoints() ) {
                CSeqMap::CSegmentInfo* seg = new CSeqMap::CSegmentInfo(
                    CSeqMap::eSeqRef, pos, 1, minus_strand);
                seg->m_RefSeq =
                    GetIdMapper().GetHandle(loc.GetPacked_pnt().GetId());
                seg->m_RefPos = *pi;
                seg->m_Length = 1;
                seqmap.Add(*seg);
                pos++;
            }
            return;
        }
    case CSeq_loc::e_Mix:
        {
            iterate ( CSeq_loc_mix::Tdata, li, loc.GetMix().Get() ) {
                x_LocToSeqMap(**li, pos, seqmap);
            }
            return;
        }
    case CSeq_loc::e_Equiv:
        {
            iterate ( CSeq_loc_equiv::Tdata, li, loc.GetEquiv().Get() ) {
                //### Is this type allowed here?
                x_LocToSeqMap(**li, pos, seqmap);
            }
            return;
        }
    case CSeq_loc::e_Bond:
        {
            throw runtime_error("e_Bond is not allowed as a reference type");
        }
    case CSeq_loc::e_Feat:
        {
            throw runtime_error("e_Feat is not allowed as a reference type");
        }
    }
}


void CDataSource::x_DataToSeqMap(const CSeq_data& data,
                                 int& pos, int len,
                                 CSeqMap& seqmap)
{
    //### Search for gaps in the data
    CSeqMap::CSegmentInfo* seg = new CSeqMap::CSegmentInfo(
        CSeqMap::eSeqData, pos, len, false);
    // Assume starting position on the referenced sequence is 0
    seg->m_RefData = &data;
    seg->m_RefPos = 0;
    seg->m_Length = len;
    seqmap.Add(*seg);
    pos += len;
}


void CDataSource::x_MapFeature(const CSeq_feat& feat,
                               const CSeq_annot& annot,
                               CTSE_Info& tse)
{
    // Create annotation object. It will split feature location
    // to a handle-ranges map.
    CAnnotObject* aobj = new CAnnotObject(*this, feat, annot);
    // Iterate handles
    iterate ( CHandleRangeMap::TLocMap,
        mapit, aobj->GetRangeMap().GetMap() ) {
        m_TSE_ref[mapit->first].insert(&tse);
        tse.m_AnnotMap[mapit->first].insert(TRangeMap::value_type(
            mapit->second.GetOverlappingRange(), aobj));
    }
}


void CDataSource::x_MapAlign(const CSeq_align& align,
                             const CSeq_annot& annot,
                             CTSE_Info& tse)
{
    // Create annotation object. It will process the align locations
    CAnnotObject* aobj = new CAnnotObject(*this, align, annot);
    // Iterate handles
    iterate ( CHandleRangeMap::TLocMap,
        mapit, aobj->GetRangeMap().GetMap() ) {
        m_TSE_ref[mapit->first].insert(&tse);
        tse.m_AnnotMap[mapit->first].insert(TRangeMap::value_type(
            mapit->second.GetOverlappingRange(), aobj));
    }
}


void CDataSource::x_MapGraph(const CSeq_graph& graph,
                             const CSeq_annot& annot,
                             CTSE_Info& tse)
{
    // Create annotation object. It will split graph location
    // to a handle-ranges map.
    CAnnotObject* aobj = new CAnnotObject(*this, graph, annot);
    // Iterate handles
    iterate ( CHandleRangeMap::TLocMap,
        mapit, aobj->GetRangeMap().GetMap() ) {
        m_TSE_ref[mapit->first].insert(&tse);
        tse.m_AnnotMap[mapit->first].insert(TRangeMap::value_type(
            mapit->second.GetOverlappingRange(), aobj));
    }
}


void CDataSource::PopulateTSESet(CHandleRangeMap& loc,
                                 TTSESet& tse_set,
                                 const CScope::TRequestHistory& history) const
{
/*
Iterate each id from "loc". Find all TSEs with references to the id.
Put TSEs, containing the sequence itself to "selected_with_seq" and
TSEs without the sequence to "selected_with_ref". In "selected_with_seq"
try to find TSE, referenced by the history (if more than one found, report
error), if not found, search for a live TSE (if more than one found,
report error), if not found, check if there is only one dead TSE in the set.
Otherwise report error.
From "selected_with_ref" select only live TSEs and dead TSEs, referenced by
the history.
The resulting set will contain 0 or 1 TSE with the sequence, all live TSEs
without the sequence but with references to the id and all dead TSEs
(with references and without the sequence), referenced by the history.
*/
    CMutexGuard guard(sm_DataSource_Mutex);
    x_ResolveLocationHandles(loc, history);
    iterate(CHandleRangeMap::TLocMap, hit, loc.GetMap()) {
        // Search for each seq-id handle from loc in the indexes
        TTSEMap::const_iterator tse_it = m_TSE_ref.find(hit->first);
        if (tse_it == m_TSE_ref.end())
            continue; // No this seq-id in the TSE
        _ASSERT(tse_it->second.size() > 0);
        TTSESet selected_with_ref;
        TTSESet selected_with_seq;
        iterate(TTSESet, tse, tse_it->second) {
            if ((*tse)->m_BioseqMap.find(hit->first) !=
                (*tse)->m_BioseqMap.end()) {
                selected_with_seq.insert(*tse); // with sequence
            }
            else
                selected_with_ref.insert(*tse); // with reference
        }

        CRef<CTSE_Info> unique_from_history;
        CRef<CTSE_Info> unique_live;
        iterate (TTSESet, with_seq, selected_with_seq) {
            if (history.find(*with_seq) != history.end()) {
                if ( unique_from_history )
                    throw runtime_error(
                    "CDataSource: ambiguous request -- multiple history matches");
                unique_from_history = *with_seq;
            }
            else if ( !unique_from_history ) {
                if ((*with_seq)->m_Dead)
                    continue;
                if ( unique_live )
                    throw runtime_error(
                    "CDataSource: ambiguous request -- multiple live TSEs");
                unique_live = *with_seq;
            }
        }
        if ( unique_from_history ) {
            tse_set.insert(unique_from_history);
        }
        else if ( unique_live ) {
            tse_set.insert(unique_live);
        }
        else if (selected_with_seq.size() == 1) {
            tse_set.insert(*selected_with_seq.begin());
        }
        else if (selected_with_seq.size() > 1) {
            //### Try to resolve the conflict with the help of loader
            throw runtime_error(
                "CDataSource: ambigous request -- multiple TSEs found");
        }
        iterate(TTSESet, tse, selected_with_ref) {
            if ( !(*tse)->m_Dead  ||  history.find(*tse) != history.end()) {
                // Select only TSEs present in the history and live TSEs
                tse_set.insert(*tse);
            }
        }
    }
}


void CDataSource::x_ResolveLocationHandles(CHandleRangeMap& loc,
                                           const CScope::TRequestHistory& history) const
{
    CHandleRangeMap tmp(GetIdMapper());
    iterate ( CHandleRangeMap::TLocMap, it, loc.GetMap() ) {
        CTSE_Info* tse_info = x_FindBestTSE(it->first, history);
        if (tse_info != 0) {
            TBioseqMap::const_iterator info =
                tse_info->m_BioseqMap.find(it->first);
            if (info == tse_info->m_BioseqMap.end()) {
                // Just copy the existing range map
                tmp.AddRanges(it->first, it->second);
            }
            else {
                // Create range list for each synonym of a seq_id
                const CBioseq_Info::TSynonyms& syn = info->second->m_Synonyms;
                iterate ( CBioseq_Info::TSynonyms, syn_it, syn ) {
                    tmp.AddRanges(*syn_it, it->second);
                }
            }
        }
        else {
            // Just copy the existing range map
            tmp.AddRanges(it->first, it->second);
        }
    }
    loc = tmp;
}


bool CDataSource::DropTSE(const CSeq_entry& tse)
{
    CMutexGuard guard(sm_DataSource_Mutex);
    // Allow to drop top-level seq-entries only
    _ASSERT(tse.GetParentEntry() == 0);

    CRef<CSeq_entry> ref(const_cast<CSeq_entry*>(&tse));
    TEntries::iterator found = m_Entries.find(ref);
    if (found == m_Entries.end())
        return false;
    if ( found->second->Locked() )
        return false; // Not really dropped, although found
    if ( m_Loader )
        m_Loader->DropTSE(found->first);
    x_DropEntry(*found->first);
    return true;
}


void CDataSource::x_DropEntry(CSeq_entry& entry)
{
    CSeq_entry* tse = &entry;
    while ( tse->GetParentEntry() ) {
        tse = tse->GetParentEntry();
    }
    if ( entry.IsSeq() ) {
        CBioseq& seq = entry.GetSeq();
        CSeq_id_Handle key;
        iterate ( CBioseq::TId, id, seq.GetId() ) {
            // Find TSE and bioseq positions
            key = GetIdMapper().GetHandle(**id);
            TTSEMap::iterator tse_set = m_TSE_seq.find(key);
            _ASSERT(tse_set != m_TSE_seq.end());
            TTSESet::iterator tse_it = tse_set->second.begin();
            for ( ; tse_it != tse_set->second.end(); ++tse_it) {
                if ((*tse_it)->m_TSE == tse)
                    break;
            }
            _ASSERT(tse_it != tse_set->second.end());
            TBioseqMap::iterator found = (*tse_it)->m_BioseqMap.find(key);
            _ASSERT( found != (*tse_it)->m_BioseqMap.end() );
            (*tse_it)->m_BioseqMap.erase(found);
            // Remove TSE index for the bioseq (the TSE may still be
            // in other id indexes).
            tse_set->second.erase(tse_it);
            if (tse_set->second.size() == 0) {
                m_TSE_seq.erase(tse_set);
            }
        }
        TSeqMaps::iterator map_it = m_SeqMaps.find(&seq);
        if (map_it != m_SeqMaps.end()) {
            for (int i = 0; i < map_it->second->size(); i++) {
                // Un-lock seq-id handles
                const CSeqMap::CSegmentInfo& info = (*(map_it->second))[i];
                if (info.m_SegType == CSeqMap::eSeqRef) {
                }
            }
            m_SeqMaps.erase(map_it);
        }
        x_DropAnnotMap(entry);
    }
    else {
        iterate ( CBioseq_set::TSeq_set, it, entry.GetSet().GetSeq_set() ) {
            x_DropEntry(**it);
        }
        x_DropAnnotMap(entry);
    }
    m_Entries.erase(&entry);
}


void CDataSource::x_DropAnnotMap(CSeq_entry& entry)
{
    const CBioseq::TAnnot* annot_list = 0;
    if ( entry.IsSeq() ) {
        if ( entry.GetSeq().IsSetAnnot() ) {
            annot_list = &entry.GetSeq().GetAnnot();
        }
    }
    else {
        if ( entry.GetSet().IsSetAnnot() ) {
            annot_list = &entry.GetSet().GetAnnot();
        }
        // Do not iterate sub-trees -- they will be iterated by
        // the calling function.
    }
    if ( !annot_list ) {
        return;
    }
    iterate ( CBioseq::TAnnot, ai, *annot_list ) {
        switch ( (*ai)->GetData().Which() ) {
        case CSeq_annot::C_Data::e_Ftable:
            {
                iterate ( CSeq_annot::C_Data::TFtable, it,
                    (*ai)->GetData().GetFtable() ) {
                    x_DropFeature(**it, **ai);
                }
                break;
            }
        case CSeq_annot::C_Data::e_Align:
            {
                iterate ( CSeq_annot::C_Data::TAlign, it,
                    (*ai)->GetData().GetAlign() ) {
                    x_DropAlign(**it, **ai);
                }
                break;
            }
        case CSeq_annot::C_Data::e_Graph:
            {
                iterate ( CSeq_annot::C_Data::TGraph, it,
                    (*ai)->GetData().GetGraph() ) {
                    x_DropGraph(**it, **ai);
                }
                break;
            }
        }
    }
}


void CDataSource::x_DropFeature(const CSeq_feat& feat,
                                const CSeq_annot& annot)
{
    // Create a copy of annot object to iterate all seq-id handles
    CRef<CAnnotObject> aobj(new CAnnotObject(*this, feat, annot));
    // Iterate id handles
    iterate ( CHandleRangeMap::TLocMap,
        mapit, aobj->GetRangeMap().GetMap() ) {
        // Find TSEs containing references to the id
        TTSEMap::iterator tse_set = m_TSE_ref.find(mapit->first);
        _ASSERT(tse_set != m_TSE_ref.end());
        // Find the TSE containing the feature
        TTSESet::iterator tse_info = tse_set->second.begin();
        for ( ; tse_info != tse_set->second.end(); ++tse_info) {
            TAnnotMap::iterator annot = (*tse_info)->m_AnnotMap.find(
                mapit->first);
            if (annot == (*tse_info)->m_AnnotMap.end())
                continue;
            TRangeMap::iterator rg = annot->second.begin(
                mapit->second.GetOverlappingRange());
            for ( ; rg != annot->second.end(); ++rg) {
                if (rg->second->IsFeat()  &&
                    &(rg->second->GetFeat()) == &feat)
                    break;
            }
            if (rg == annot->second.end())
                continue;
            // Delete the feature from all indexes
            annot->second.erase(rg);
            if (annot->second.size() == 0) {
                tse_set->second.erase(tse_info);
            }
            if (tse_set->second.size() == 0) {
                m_TSE_ref.erase(tse_set);
            }
            break;
        }
    }
}


void CDataSource::x_DropAlign(const CSeq_align& align,
                              const CSeq_annot& annot)
{
    // Create a copy of annot object to iterate all seq-id handles
    CRef<CAnnotObject> aobj(new CAnnotObject(*this, align, annot));
    // Iterate handles
    iterate ( CHandleRangeMap::TLocMap,
        mapit, aobj->GetRangeMap().GetMap() ) {
        // Find TSEs containing references to the id
        TTSEMap::iterator tse_set = m_TSE_ref.find(mapit->first);
        _ASSERT(tse_set != m_TSE_ref.end());
        // Find the TSE containing the align
        TTSESet::iterator tse_info = tse_set->second.begin();
        for ( ; tse_info != tse_set->second.end(); ++tse_info) {
            TAnnotMap::iterator annot = (*tse_info)->m_AnnotMap.find(
                mapit->first);
            if (annot == (*tse_info)->m_AnnotMap.end())
                continue;
            TRangeMap::iterator rg = annot->second.begin(
                mapit->second.GetOverlappingRange());
            for ( ; rg != annot->second.end(); ++rg) {
                if (rg->second->IsAlign()  &&
                    &(rg->second->GetAlign()) == &align)
                    break;
            }
            if (rg == annot->second.end())
                continue;
            // Delete the align from all indexes
            annot->second.erase(rg);
            if (annot->second.size() == 0) {
                tse_set->second.erase(tse_info);
            }
            if (tse_set->second.size() == 0) {
                m_TSE_ref.erase(tse_set);
            }
            break;
        }
    }
}


void CDataSource::x_DropGraph(const CSeq_graph& graph,
                              const CSeq_annot& annot)
{
    // Create a copy of annot object to iterate all seq-id handles
    CRef<CAnnotObject> aobj(new CAnnotObject(*this, graph, annot));
    // Iterate handles
    iterate ( CHandleRangeMap::TLocMap,
        mapit, aobj->GetRangeMap().GetMap() ) {
        // Find TSEs containing references to the id
        TTSEMap::iterator tse_set = m_TSE_ref.find(mapit->first);
        _ASSERT(tse_set != m_TSE_ref.end());
        // Find the TSE containing the graph
        TTSESet::iterator tse_info = tse_set->second.begin();
        for ( ; tse_info != tse_set->second.end(); ++tse_info) {
            TAnnotMap::iterator annot = (*tse_info)->m_AnnotMap.find(
                mapit->first);
            if (annot == (*tse_info)->m_AnnotMap.end())
                continue;
            TRangeMap::iterator rg = annot->second.begin(
                mapit->second.GetOverlappingRange());
            for ( ; rg != annot->second.end(); ++rg) {
                if (rg->second->IsGraph()  &&
                    &(rg->second->GetGraph()) == &graph)
                    break;
            }
            if (rg == annot->second.end())
                continue;
            // Delete the graph from all indexes
            annot->second.erase(rg);
            if (annot->second.size() == 0) {
                tse_set->second.erase(tse_info);
            }
            if (tse_set->second.size() == 0) {
                m_TSE_ref.erase(tse_set);
            }
            break;
        }
    }
}


void CDataSource::x_CleanupUnusedEntries(void)
{
    CMutexGuard guard(sm_DataSource_Mutex);
    iterate(TEntries, it, m_Entries) {
        if ( !it->second->Locked() ) {
            DropTSE(*it->first);
        }
    }
}


void CDataSource::x_UpdateTSEStatus(CSeq_entry& tse, bool dead)
{
    CMutexGuard guard(sm_DataSource_Mutex);
    TEntries::iterator tse_it = m_Entries.find(&tse);
    _ASSERT(tse_it != m_Entries.end());
    tse_it->second->m_Dead = dead;
}


CSeqMatch_Info CDataSource::BestResolve(const CSeq_id& id,
                                        const CScope::TRequestHistory& history)
{
    if ( m_Loader ) {
        // Send request to the loader
        CSeq_loc loc;
        SerialAssign<CSeq_id>(loc.SetWhole(), id);
        CHandleRangeMap hrm(GetIdMapper());
        hrm.AddLocation(loc);
        m_Loader->GetRecords(hrm, CDataLoader::eBioseqCore);
    }
    CSeq_id_Handle idh = GetIdMapper().GetHandle(id, true);
    CMutexGuard guard(sm_DataSource_Mutex);
    CTSE_Info* tse = x_FindBestTSE(idh, history);
    if (tse) {
        return (CSeqMatch_Info(idh, *tse, *this));
    }
    else {
        return CSeqMatch_Info();
    }
}


string CDataSource::GetName(void) const
{
    if ( m_Loader )
        return m_Loader->GetName();
    else
        return "";
}


END_SCOPE(objects)
END_NCBI_SCOPE
