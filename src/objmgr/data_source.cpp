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
* Revision 1.55  2002/07/01 15:44:51  grichenk
* Fixed typos
*
* Revision 1.54  2002/07/01 15:40:58  grichenk
* Fixed 'tse_set' warning.
* Removed duplicate call to tse_set.begin().
* Fixed version resolving for annotation iterators.
* Fixed strstream bug in KCC.
*
* Revision 1.53  2002/06/28 17:30:42  grichenk
* Duplicate seq-id: ERR_POST() -> THROW1_TRACE() with the seq-id
* list.
* Fixed x_CleanupUnusedEntries().
* Fixed a bug with not found sequences.
* Do not copy bioseq data in GetBioseqCore().
*
* Revision 1.52  2002/06/21 15:12:15  grichenk
* Added resolving seq-id to the best version
*
* Revision 1.51  2002/06/12 14:39:53  grichenk
* Performance improvements
*
* Revision 1.50  2002/06/04 17:18:33  kimelman
* memory cleanup :  new/delete/Cref rearrangements
*
* Revision 1.49  2002/05/31 17:53:00  grichenk
* Optimized for better performance (CTSE_Info uses atomic counter,
* delayed annotations indexing, no location convertions in
* CAnnot_Types_CI if no references resolution is required etc.)
*
* Revision 1.48  2002/05/29 21:21:13  gouriano
* added debug dump
*
* Revision 1.47  2002/05/28 18:00:43  gouriano
* DebugDump added
*
* Revision 1.46  2002/05/24 14:57:12  grichenk
* SerialAssign<>() -> CSerialObject::Assign()
*
* Revision 1.45  2002/05/21 18:57:25  grichenk
* Fixed annotations dropping
*
* Revision 1.44  2002/05/21 18:40:50  grichenk
* Fixed annotations droppping
*
* Revision 1.43  2002/05/14 20:06:25  grichenk
* Improved CTSE_Info locking by CDataSource and CDataLoader
*
* Revision 1.42  2002/05/13 15:28:27  grichenk
* Fixed seqmap for virtual sequences
*
* Revision 1.41  2002/05/09 14:18:15  grichenk
* More TSE conflict resolving rules for annotations
*
* Revision 1.40  2002/05/06 03:28:46  vakatov
* OM/OM1 renaming
*
* Revision 1.39  2002/05/03 21:28:09  ucko
* Introduce T(Signed)SeqPos.
*
* Revision 1.38  2002/05/02 20:42:37  grichenk
* throw -> THROW1_TRACE
*
* Revision 1.37  2002/04/22 20:05:08  grichenk
* Fixed minor bug in GetSequence()
*
* Revision 1.36  2002/04/19 18:02:47  kimelman
* add verify to catch coredump
*
* Revision 1.35  2002/04/17 21:09:14  grichenk
* Fixed annotations loading
* +IsSynonym()
*
* Revision 1.34  2002/04/11 18:45:39  ucko
* Pull in extra headers to make KCC happy.
*
* Revision 1.33  2002/04/11 12:08:21  grichenk
* Fixed GetResolvedSeqMap() implementation
*
* Revision 1.32  2002/04/05 21:23:08  grichenk
* More duplicate id warnings fixed
*
* Revision 1.31  2002/04/05 20:27:52  grichenk
* Fixed duplicate identifier warning
*
* Revision 1.30  2002/04/04 21:33:13  grichenk
* Fixed GetSequence() for sequences with unresolved segments
*
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
#include <objects/objmgr/seq_vector.hpp>
#include <objects/general/Int_fuzz.hpp>
#include <objects/seq/Seqdesc.hpp>
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

CTSE_Info* CDataSource::x_FindBestTSE(CSeq_id_Handle handle,
                                      const CScope::TRequestHistory& history) const
{
    const TTSESet* p_tse_set = 0;
    {{
        CMutexGuard guard(sm_DataSource_Mutex);
        TTSEMap::const_iterator tse_set = m_TSE_seq.find(handle);
        if ( tse_set == m_TSE_seq.end() )
            return 0;
        p_tse_set = &tse_set->second;
        //### Obtain MT-lock for the TSE
    }}
    if ( p_tse_set->size() == 1) {
        // There is only one TSE, no matter live or dead
        return *p_tse_set->begin();
    }
    // The map should not contain empty entries
    _ASSERT(p_tse_set->size() > 0);
    TTSESet live;
    CTSE_Info* from_history = 0;
    iterate(TTSESet, tse, *p_tse_set) {
        // Check history
        CScope::TRequestHistory::const_iterator hst = history.find(*tse);
        if (hst != history.end()) {
            if ( from_history ) {
                THROW1_TRACE(runtime_error,
                    "CDataSource::x_FindBestTSE() -- Multiple history matches");
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
    else if ((live.size() == 0)  &&  m_Loader) {
        // No live TSEs -- try to select the best dead TSE
        CTSE_Info* best = m_Loader->ResolveConflict(handle, *p_tse_set);
        if ( best ) {
            return *p_tse_set->find(best);
        }
        THROW1_TRACE(runtime_error,
            "CDataSource::x_FindBestTSE() -- Multiple seq-id matches found");
    }
    // Multiple live TSEs -- try to resolve the conflict (the status of some
    // TSEs may change)
    CTSE_Info* best = m_Loader->ResolveConflict(handle, live);
    if ( best ) {
        return *p_tse_set->find(best);
    }
    THROW1_TRACE(runtime_error,
        "CDataSource::x_FindBestTSE() -- Multiple live entries found");
}


CBioseq_Handle CDataSource::GetBioseqHandle(CScope& scope, const CSeq_id& id)
{
    CSeqMatch_Info info = BestResolve(id, scope);
    if ( !info )
        return CBioseq_Handle();
    CSeq_entry* se = 0;
    {{
        CMutexGuard guard(sm_DataSource_Mutex);
        TBioseqMap::iterator found = info.m_TSE->
            m_BioseqMap.find(info.m_Handle);
        _ASSERT(found != info.m_TSE->m_BioseqMap.end());
        se = found->second->m_Entry;
    }}
    CBioseq_Handle h(info.m_Handle);
    h.x_ResolveTo(scope, *this, *se, *info.m_TSE);
    scope.x_AddToHistory(*info.m_TSE);
    info.m_TSE->Unlock(); // Locked by BestResolve()
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
        CHandleRange rg(handle.m_Value);
        rg.AddRange(CHandleRange::TRange(
            CHandleRange::TRange::GetWholeFrom(),
            CHandleRange::TRange::GetWholeTo()), eNa_strand_unknown);
        hrm.AddRanges(handle.m_Value, rg);
        m_Loader->GetRecords(hrm, CDataLoader::eBioseq);
    }
    //### CMutexGuard guard(sm_DataSource_Mutex);
    // the handle must be resolved to this data source
    _ASSERT(handle.m_DataSource == this);
    return handle.m_Entry->GetSeq();
}


const CSeq_entry& CDataSource::GetTSE(const CBioseq_Handle& handle)
{
    // Bioseq and TSE must be loaded if there exists a handle
    //### CMutexGuard guard(sm_DataSource_Mutex);
    _ASSERT(handle.m_DataSource == this);
    return *m_Entries[handle.m_Entry]->m_TSE;
}


CBioseq_Handle::TBioseqCore CDataSource::GetBioseqCore
    (const CBioseq_Handle& handle)
{
    // Bioseq core and TSE must be loaded if there exists a handle --
    // just return the bioseq as-is.
    // the handle must be resolved to this data source
    _ASSERT(handle.m_DataSource == this);
    return &handle.m_Entry->GetSeq();
/*
    const CBioseq* seq = &GetBioseq(handle);

    CMutexGuard guard(sm_DataSource_Mutex);
    CBioseq* seq_core = new CBioseq();
    // Assign seq members to seq_core:
    CBioseq::TId& id_list = seq_core->SetId();
    iterate ( CBioseq::TId, it, seq->GetId() ) {
        CSeq_id* id = new CSeq_id;
        id->Assign(**it);
        id_list.push_back(id);
    }
    if ( seq->IsSetDescr() )
        seq_core->SetDescr().Assign(seq->GetDescr());
    const CSeq_inst& inst = seq->GetInst();
    CSeq_inst& inst_core = seq_core->SetInst();
    inst_core.SetRepr(inst.GetRepr());
    inst_core.SetMol(inst.GetMol());
    if ( inst.IsSetLength() )
        inst_core.SetLength(inst.GetLength());
    if ( inst.IsSetFuzz() ) {
        CInt_fuzz* fuzz = new CInt_fuzz();
        fuzz->Assign(inst.GetFuzz());
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
            ext->Assign(inst.GetExt());
        }
        else {
            // Do not copy seq-data
            iterate (CDelta_ext::Tdata, it, inst.GetExt().GetDelta().Get()) {
                CDelta_seq* dseq = new CDelta_seq;
                if ( (*it)->IsLiteral() ) {
                    dseq->SetLiteral().SetLength(
                        (*it)->GetLiteral().GetLength());
                    if ( (*it)->GetLiteral().IsSetFuzz() ) {
                        dseq->SetLiteral().SetFuzz().Assign(
                            (*it)->GetLiteral().GetFuzz());
                    }
                }
                else {
                    dseq->Assign(**it);
                }
            }
        }
        inst_core.SetExt(*ext);
    }
    if ( inst.IsSetHist() ) {
        CSeq_hist* hist = new CSeq_hist();
        hist->Assign(inst.GetHist());
        inst_core.SetHist(*hist);
    }

    return seq_core;
*/
}


const CSeqMap& CDataSource::GetSeqMap(const CBioseq_Handle& handle)
{
    //### CMutexGuard guard(sm_DataSource_Mutex);
    return x_GetSeqMap(handle);
}


CSeqMap& CDataSource::x_GetSeqMap(const CBioseq_Handle& handle)
{
    // Call loader first
    if ( m_Loader ) {
        // Send request to the loader
        CHandleRangeMap hrm(GetIdMapper());
        CHandleRange rg(handle.m_Value);
        rg.AddRange(CHandleRange::TRange(
            CHandleRange::TRange::GetWholeFrom(),
            CHandleRange::TRange::GetWholeTo()), eNa_strand_unknown);
        hrm.AddRanges(handle.m_Value, rg);
        m_Loader->GetRecords(hrm, CDataLoader::eBioseq); //### or eCore???
    }

    _ASSERT(handle.m_DataSource == this);
    const CBioseq& seq = GetBioseq(handle);
    TSeqMaps::iterator found = m_SeqMaps.find(&seq);
    if (found == m_SeqMaps.end()) {
        // Create sequence map
        x_CreateSeqMap(seq);
        found = m_SeqMaps.find(&seq);
        if (found == m_SeqMaps.end()) {
            THROW1_TRACE(runtime_error,
                "CDataSource::x_GetSeqMap() -- Sequence map not found");
        }
    }
    return *found->second;
}


bool CDataSource::GetSequence(const CBioseq_Handle& handle,
                              TSeqPos point,
                              SSeqData* seq_piece,
                              CScope& scope)
{
    // Call loader first
    if ( m_Loader ) {
        // Send request to the loader
        CHandleRangeMap hrm(GetIdMapper());
        CHandleRange rg(handle.m_Value);
        //### Should it be Whole or Point?
        rg.AddRange(CHandleRange::TRange(
            CHandleRange::TRange::GetWholeFrom(),
            CHandleRange::TRange::GetWholeTo()), eNa_strand_unknown);
        hrm.AddRanges(handle.m_Value, rg);
        m_Loader->GetRecords(hrm, CDataLoader::eSequence);
    }
    //### CMutexGuard guard(sm_DataSource_Mutex);
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
        if (seg.m_Position > point  ||
            seg.m_Position + seg.m_Length - 1 < point) {
            // This may happen when the x_Resolve() was unable to
            // resolve some references before the point and the total
            // length of the sequence appears to be less than point.
            // Immitate a gap of length 1.
            seq_piece->dest_start = point;
            seq_piece->length = 1;
            seq_piece->src_data = 0;
            seq_piece->src_start = 0;
            return true;
        }
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
                TSignedSeqPos shift = seg.m_RefPos - seg.m_Position;
                if ( scope.x_GetSequence(seg.m_RefSeq,
                    point + shift, seq_piece) ) {
                    TSeqPos xL = seg.m_Length;
                    TSignedSeqPos delta = seg.m_RefPos -
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
                else {
                    seq_piece->dest_start = seg.m_Position;
                    seq_piece->length = seg.m_Length;
                    seq_piece->src_data = 0;
                    seq_piece->src_start = 0;
                    return true;
                }
            }
        case CSeqMap::eSeqGap:
            {
                seq_piece->dest_start = seg.m_Position;
                seq_piece->src_start = 0;
                seq_piece->length = seg.m_Length;
                seq_piece->src_data = 0;
                return true;
            }
        case CSeqMap::eSeqEnd:
            {
                THROW1_TRACE(runtime_error,
                    "CDataSource::GetSequence() -- Attempt to read beyond sequence end");
            }
        }
    }
    return false;
}


CTSE_Info* CDataSource::AddTSE(CSeq_entry& se, TTSESet* tse_set, bool dead)
{
    //### CMutexGuard guard(sm_DataSource_Mutex);
    return x_AddToBioseqMap(se, dead, tse_set);
}


CSeq_entry* CDataSource::x_FindEntry(const CSeq_entry& entry)
{
    CRef<CSeq_entry> ref(const_cast<CSeq_entry*>(&entry));
    //### CMutexGuard guard(sm_DataSource_Mutex);
    TEntries::iterator found = m_Entries.find(ref);
    if (found == m_Entries.end())
        return 0;
    return found->first;
}


bool CDataSource::AttachEntry(const CSeq_entry& parent, CSeq_entry& bioseq)
{
    //### Write-lock TSE instead!!!
    //### CMutexGuard guard(sm_DataSource_Mutex);
    CSeq_entry* found = x_FindEntry(parent);
    if ( !found ) {
        return false;
    }
    _ASSERT(found  &&  found->IsSet());

    found->SetSet().SetSeq_set().push_back(&bioseq);

    // Re-parentize, update index
    CSeq_entry* top = found;
    while ( top->GetParentEntry() ) {
        top = top->GetParentEntry();
    }
    top->Parentize();
    // The "dead" parameter is not used here since the TSE_Info
    // structure must have been created already.
    x_IndexEntry(bioseq, *top, false, 0);
    return true;
}


bool CDataSource::AttachMap(const CSeq_entry& bioseq, CSeqMap& seqmap)
{
    //### CMutexGuard guard(sm_DataSource_Mutex);
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
                                TSeqPos start,
                                TSeqPos length)
{
/*
    The function should be used mostly by data loaders. If a segment of a
    bioseq is a reference to a whole sequence, the positioning mechanism may
    not work correctly, since the length of such reference is unknown. In most
    cases "whole" reference should be the only segment of a delta-ext.
*/
    // Get non-const reference to the entry
    //### CMutexGuard guard(sm_DataSource_Mutex);
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
                THROW1_TRACE(runtime_error,
                    "CDataSource::AttachSeqData() -- Can not insert segment into a literal");
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
            THROW1_TRACE(runtime_error,
                "CDataSource::AttachSeqData() -- Segment position conflict");
        }
        else {
            THROW1_TRACE(runtime_error,
                "CDataSource::AttachSeqData() -- Invalid delta-seq type");
        }
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
            gap_dup->Assign(**gap);
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
    //### CMutexGuard guard(sm_DataSource_Mutex);
    CSeq_entry* found = x_FindEntry(entry);
    if ( !found ) {
        return false;
    }

    CTSE_Info* tse = m_Entries[found];
    //### Lock the TSE !!!!!!!!!!!

    CBioseq_set::TAnnot* annot_list = 0;
    if ( found->IsSet() ) {
        annot_list = &found->SetSet().SetAnnot();
    }
    else {
        annot_list = &found->SetSeq().SetAnnot();
    }
    annot_list->push_back(&annot);

    if ( !m_IndexedAnnot )
        return true; // skip annotations indexing

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


CTSE_Info* CDataSource::x_AddToBioseqMap(CSeq_entry& entry,
                                         bool dead,
                                         TTSESet* tse_set)
{
    // Search for bioseqs, add each to map
    entry.Parentize();
    CTSE_Info* info = x_IndexEntry(entry, entry, dead, tse_set);
    // Do not index new TSEs -- wait for annotations request
    if ( info->IsIndexed() ) {
        x_AddToAnnotMap(entry);
    }
    else {
        m_IndexedAnnot = false; // reset the flag to enable indexing
    }
    return info;
}


CTSE_Info* CDataSource::x_IndexEntry(CSeq_entry& entry, CSeq_entry& tse,
                                     bool dead,
                                     TTSESet* tse_set)
{
    CTSE_Info* tse_info = 0;
    TEntries::iterator found_tse = m_Entries.find(&tse);
    if (found_tse == m_Entries.end()) {
        // New TSE info
        tse_info = new CTSE_Info;
        tse_info->m_TSE = &tse;
        tse_info->m_Dead = dead;
        // Do not lock TSE if there is no tse_set -- none will unlock it
        if (tse_set) {
            tse_info->Lock();
        }
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
            TTSESet& seq_tse_set = m_TSE_seq[key];
            TTSESet::iterator tse_it = seq_tse_set.begin();
            for ( ; tse_it != seq_tse_set.end(); ++tse_it) {
                if ((*tse_it)->m_TSE == &tse)
                    break;
            }
            if (tse_it == seq_tse_set.end()) {
                seq_tse_set.insert(tse_info);
            }
            else {
                tse_info = *tse_it;
            }
            TBioseqMap::iterator found = tse_info->m_BioseqMap.find(key);
            if ( found != tse_info->m_BioseqMap.end() ) {
                // No duplicate bioseqs in the same TSE
                string sid1, sid2, si_conflict;
                {{
                    CNcbiOstrstream os;
                    iterate (CBioseq::TId, id_it, found->second->m_Entry->GetSeq().GetId()) {
                        os << (*id_it)->DumpAsFasta() << " | ";
                    }
                    sid1 = CNcbiOstrstreamToString(os);
                }}
                {{
                    CNcbiOstrstream os;
                    iterate (CBioseq::TId, id_it, seq->GetId()) {
                        os << (*id_it)->DumpAsFasta() << " | ";
                    }
                    sid2 = CNcbiOstrstreamToString(os);
                }}
                {{
                    CNcbiOstrstream os;
                    os << (*id)->DumpAsFasta();
                    si_conflict = CNcbiOstrstreamToString(os);
                }}
                THROW1_TRACE(runtime_error,
                    " duplicate Bioseq id '" + si_conflict + "' present in" +
                    "\n  seq1: " + sid1 +
                    "\n  seq2: " + sid2);
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
            x_IndexEntry(**it, tse, dead, 0);
        }
    }
    if (tse_set) {
        tse_set->insert(tse_info);
    }
    return tse_info;
}


void CDataSource::x_AddToAnnotMap(CSeq_entry& entry)
{
    // The entry must be already in the m_Entries map
    CTSE_Info* tse = m_Entries[&entry];
    //### Lock the TSE !!!!!!!!!!!!!!
    tse->SetIndexed(true);
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
    TSeqPos pos = 0;
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
    else {
        // Virtual sequence -- no data, no segments
        _ASSERT(seq.GetInst().GetRepr() == CSeq_inst::eRepr_virtual);
        TSeqPos len = 0;
        if ( seq.GetInst().IsSetLength() ) {
            len = seq.GetInst().GetLength();
        }
        seqmap->Add(CSeqMap::eSeqGap, 0, len); // The total sequence is gap
        pos += len;
    }
    seqmap->Add(CSeqMap::eSeqEnd, pos, 0);
    m_SeqMaps[&seq] = seqmap;
}


void CDataSource::x_LocToSeqMap(const CSeq_loc& loc,
                                TSeqPos& pos,
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
            THROW1_TRACE(runtime_error,
                "CDataSource::x_LocToSeqMap() -- e_Bond is not allowed as a reference type");
        }
    case CSeq_loc::e_Feat:
        {
            THROW1_TRACE(runtime_error,
                "CDataSource::x_LocToSeqMap() -- e_Feat is not allowed as a reference type");
        }
    }
}


void CDataSource::x_DataToSeqMap(const CSeq_data& data,
                                 TSeqPos& pos, TSeqPos len,
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
                                 CSeq_annot::C_Data::E_Choice sel,
                                 CScope& scope)
{
    // Index all annotations if not indexed yet
    if ( !m_IndexedAnnot ) {
        iterate (TEntries, tse_it, m_Entries) {
            if ( !tse_it->second->IsIndexed() )
                x_AddToAnnotMap(*tse_it->second->m_TSE);
        }
        m_IndexedAnnot = true;
    }
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
    //### Lock all TSEs found, unlock all filtered out in the end.
    TTSESet loaded_tse_set;
    TTSESet tmp_tse_set;
    if ( m_Loader ) {
        // Send request to the loader
        switch ( sel ) {
        case CSeq_annot::C_Data::e_Ftable:
            m_Loader->GetRecords(loc, CDataLoader::eFeatures, &loaded_tse_set);
            break;
        case CSeq_annot::C_Data::e_Align:
            //### Need special flag for alignments
            m_Loader->GetRecords(loc, CDataLoader::eAll);
            break;
        case CSeq_annot::C_Data::e_Graph:
            m_Loader->GetRecords(loc, CDataLoader::eGraph);
            break;
        }
    }
    //### CMutexGuard guard(sm_DataSource_Mutex);
    x_ResolveLocationHandles(loc, scope.m_History);
    TTSESet non_history;
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
            if (scope.m_History.find(*with_seq) != scope.m_History.end()) {
                if ( unique_from_history ) {
                    THROW1_TRACE(runtime_error,
                        "CDataSource::PopulateTSESet() -- "
                        "Ambiguous request: multiple history matches");
                }
                unique_from_history = *with_seq;
            }
            else if ( !unique_from_history ) {
                if ((*with_seq)->m_Dead)
                    continue;
                if ( unique_live ) {
                    THROW1_TRACE(runtime_error,
                        "CDataSource::PopulateTSESet() -- "
                        "Ambiguous request: multiple live TSEs");
                }
                unique_live = *with_seq;
            }
        }
        if ( unique_from_history ) {
            tmp_tse_set.insert(unique_from_history);
        }
        else if ( unique_live ) {
            non_history.insert(unique_live);
        }
        else if (selected_with_seq.size() == 1) {
            non_history.insert(*selected_with_seq.begin());
        }
        else if (selected_with_seq.size() > 1) {
            //### Try to resolve the conflict with the help of loader
            THROW1_TRACE(runtime_error,
                "CDataSource::PopulateTSESet() -- "
                "Ambigous request: multiple TSEs found");
        }
        iterate(TTSESet, tse, selected_with_ref) {
            if ( !(*tse)->m_Dead  ||  scope.m_History.find(*tse) != scope.m_History.end()) {
                // Select only TSEs present in the history and live TSEs
                // Different sets for in-history and non-history TSEs for
                // the future filtering.
                if (scope.m_History.find(*tse) != scope.m_History.end()) {
                    tmp_tse_set.insert(*tse); // in-history TSE
                }
                else {
                    non_history.insert(*tse); // non-history TSE
                }
            }
        }
    }
    // Filter out TSEs not in the history yet and conflicting with any
    // history TSE. The conflict may be caused by any seq-id, even not
    // mentioned in the searched location.
    bool conflict;
    iterate (TTSESet, tse_it, non_history) {
        conflict = false;
        // Check each seq-id from the current TSE
        iterate (CTSE_Info::TBioseqMap, seq_it, (*tse_it)->m_BioseqMap) {
            iterate (CScope::TRequestHistory, hist_it, scope.m_History) {
                conflict = (*hist_it)->m_BioseqMap.find(seq_it->first) !=
                    (*hist_it)->m_BioseqMap.end();
                if ( conflict ) break;
            }
            if ( conflict ) break;
        }
        if ( !conflict ) {
            // No conflicts found -- add the TSE to the resulting set
            tmp_tse_set.insert(*tse_it);
        }
    }
    // Unlock unused TSEs
    iterate (TTSESet, lit, loaded_tse_set) {
        if (tmp_tse_set.find(*lit) == tmp_tse_set.end()) {
            (*lit)->Unlock();
        }
    }
    // Lock used TSEs loaded before this call (the scope does not know
    // which TSE should be unlocked, so it unlocks all of them).
    iterate (TTSESet, lit, tmp_tse_set) {
        TTSESet::iterator loaded_it = loaded_tse_set.find(*lit);
        if (loaded_it == loaded_tse_set.end()) {
            (*lit)->Lock();
        }
        tse_set.insert(*lit);
    }
}


void CDataSource::x_ResolveLocationHandles(CHandleRangeMap& loc,
                                           const CScope::TRequestHistory& history) const
{
    CHandleRangeMap tmp(GetIdMapper());
    iterate ( CHandleRangeMap::TLocMap, it, loc.GetMap() ) {
        CSeq_id_Handle idh = it->first;
        CTSE_Info* tse_info = x_FindBestTSE(it->first, history);
        if ( !tse_info ) {
            // Try to find the best matching id (not exactly equal)
            const CSeq_id& id = GetIdMapper().GetSeq_id(idh);
            TSeq_id_HandleSet hset;
            GetIdMapper().GetMatchingHandles(id, hset);
            iterate(TSeq_id_HandleSet, hit, hset) {
                if ( tse_info  &&  idh.IsBetter(*hit) )
                    continue;
                CTSE_Info* tmp_tse = x_FindBestTSE(*hit, history);
                if ( tmp_tse ) {
                    tse_info = tmp_tse;
                    idh = *hit;
                }
            }
        }
        if (tse_info != 0) {
            TBioseqMap::const_iterator info =
                tse_info->m_BioseqMap.find(idh);
            if (info == tse_info->m_BioseqMap.end()) {
                // Just copy the existing range map
                tmp.AddRanges(it->first, it->second);
            }
            else {
                // Create range list for each synonym of a seq_id
                const CBioseq_Info::TSynonyms& syn = info->second->m_Synonyms;
                iterate ( CBioseq_Info::TSynonyms, syn_it, syn ) {
                    CHandleRange rg(*syn_it);
                    iterate (CHandleRange::TRanges, rit, it->second.GetRanges()) {
                        rg.AddRange(rit->first, rit->second);
                    }
                    tmp.AddRanges(*syn_it, rg);
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
    // Allow to drop top-level seq-entries only
    _ASSERT(tse.GetParentEntry() == 0);

    CRef<CSeq_entry> ref(const_cast<CSeq_entry*>(&tse));
    TEntries::iterator found = m_Entries.find(ref);
    if (found == m_Entries.end())
        return false;
    //### CMutexGuard guard(sm_DataSource_Mutex);
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
/*
            for (size_t i = 0; i < map_it->second->size(); i++) {
                // Un-lock seq-id handles
                const CSeqMap::CSegmentInfo& info = (*(map_it->second))[i];
                if (info.m_SegType == CSeqMap::eSeqRef) {
                    //### ??????
                }
            }
*/
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
        if (tse_set == m_TSE_ref.end())
            continue; // The referenced ID is not currently loaded
        // Find the TSE containing the feature
        TTSESet::iterator tse_info = tse_set->second.begin();
        for ( ; tse_info != tse_set->second.end(); ++tse_info) {
            TAnnotMap::iterator annot_it = (*tse_info)->m_AnnotMap.find(
                mapit->first);
            if (annot_it == (*tse_info)->m_AnnotMap.end())
                continue;
            TRangeMap::iterator rg = annot_it->second.begin(
                mapit->second.GetOverlappingRange());
            for ( ; rg != annot_it->second.end(); ++rg) {
                if (rg->second->IsFeat()  &&
                    &(rg->second->GetFeat()) == &feat)
                    break;
            }
            if (rg == annot_it->second.end())
                continue;
            // Delete the feature from all indexes
            annot_it->second.erase(rg);
            if (annot_it->second.size() == 0) {
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
        if (tse_set == m_TSE_ref.end())
            continue; // The referenced ID is not currently loaded
        // Find the TSE containing the align
        TTSESet::iterator tse_info = tse_set->second.begin();
        for ( ; tse_info != tse_set->second.end(); ++tse_info) {
            TAnnotMap::iterator annot_it = (*tse_info)->m_AnnotMap.find(
                mapit->first);
            if (annot_it == (*tse_info)->m_AnnotMap.end())
                continue;
            TRangeMap::iterator rg = annot_it->second.begin(
                mapit->second.GetOverlappingRange());
            for ( ; rg != annot_it->second.end(); ++rg) {
                if (rg->second->IsAlign()  &&
                    &(rg->second->GetAlign()) == &align)
                    break;
            }
            if (rg == annot_it->second.end())
                continue;
            // Delete the align from all indexes
            annot_it->second.erase(rg);
            if (annot_it->second.size() == 0) {
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
        if (tse_set == m_TSE_ref.end())
            continue; // The referenced ID is not currently loaded
        // Find the TSE containing the graph
        TTSESet::iterator tse_info = tse_set->second.begin();
        for ( ; tse_info != tse_set->second.end(); ++tse_info) {
            TAnnotMap::iterator annot_it = (*tse_info)->m_AnnotMap.find(
                mapit->first);
            if (annot_it == (*tse_info)->m_AnnotMap.end())
                continue;
            TRangeMap::iterator rg = annot_it->second.begin(
                mapit->second.GetOverlappingRange());
            for ( ; rg != annot_it->second.end(); ++rg) {
                if (rg->second->IsGraph()  &&
                    &(rg->second->GetGraph()) == &graph)
                    break;
            }
            if (rg == annot_it->second.end())
                continue;
            // Delete the graph from all indexes
            annot_it->second.erase(rg);
            if (annot_it->second.size() == 0) {
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
    //### CMutexGuard guard(sm_DataSource_Mutex);
    bool broken = true;
    while ( broken ) {
        broken = false;
        iterate(TEntries, it, m_Entries) {
            if ( !it->second->Locked() ) {
                //### Lock the entry and check again
                DropTSE(*it->first);
                broken = true;
                break;
            }
        }
    }
}


void CDataSource::x_UpdateTSEStatus(CSeq_entry& tse, bool dead)
{
    //### CMutexGuard guard(sm_DataSource_Mutex);
    TEntries::iterator tse_it = m_Entries.find(&tse);
    _ASSERT(tse_it != m_Entries.end());
    tse_it->second->m_Dead = dead;
}


CSeqMatch_Info CDataSource::BestResolve(const CSeq_id& id, CScope& scope)
{
    //### Lock all TSEs found, unlock all filtered out in the end.
    TTSESet loaded_tse_set;
    if ( m_Loader ) {
        // Send request to the loader
        CSeq_id_Handle idh = GetIdMapper().GetHandle(id);
        CHandleRangeMap hrm(GetIdMapper());
        CHandleRange rg(idh);
        rg.AddRange(CHandleRange::TRange(
            CHandleRange::TRange::GetWholeFrom(),
            CHandleRange::TRange::GetWholeTo()), eNa_strand_unknown);
        hrm.AddRanges(idh, rg);
        m_Loader->GetRecords(hrm, CDataLoader::eBioseqCore, &loaded_tse_set);
    }
    CSeqMatch_Info match;
    CSeq_id_Handle idh = GetIdMapper().GetHandle(id, true);
    if ( !idh ) {
        return match; // The seq-id is not even mapped yet
    }
    CTSE_Info* tse = x_FindBestTSE(idh, scope.m_History);
    if ( !tse ) {
        // Try to find the best matching id (not exactly equal)
        TSeq_id_HandleSet hset;
        GetIdMapper().GetMatchingHandles(id, hset);
        iterate(TSeq_id_HandleSet, hit, hset) {
            if ( tse  &&  idh.IsBetter(*hit) )
                continue;
            CTSE_Info* tmp_tse = x_FindBestTSE(*hit, scope.m_History);
            if ( tmp_tse ) {
                tse = tmp_tse;
                idh = *hit;
            }
        }
    }
    if ( tse ) {
        match = CSeqMatch_Info(idh, *tse, *this);
    }
    bool just_loaded = false;
    iterate (TTSESet, lit, loaded_tse_set) {
        if (*lit != tse) {
            (*lit)->Unlock();
        }
        else {
            just_loaded = true;
        }
    }
    if ( !just_loaded  &&  match ) {
        match.m_TSE->Lock(); // will be unlocked by the scope
    }
    return match;
}


string CDataSource::GetName(void) const
{
    if ( m_Loader )
        return m_Loader->GetName();
    else
        return kEmptyStr;
}


const CSeqMap& CDataSource::GetResolvedSeqMap(CBioseq_Handle handle)
{
    // Get the source map
    CSeqMap& smap = x_GetSeqMap(handle);
    smap.x_Resolve(handle.GetSeqVector().size()-1, *handle.m_Scope);
    // Create destination map (not stored in the indexes,
    // must be deleted by user or user's CRef).
    CRef<CSeqMap> dmap(new CSeqMap());

    TSeqPos pos = 0; // recalculate positions
    for (size_t seg = 0; seg < smap.size(); seg++)
    {
        switch (smap[seg].m_SegType) {
        case CSeqMap::eSeqData:
        case CSeqMap::eSeqGap:
        case CSeqMap::eSeqEnd:
            {
                // Copy gaps and literals
                dmap->Add(smap[seg].m_SegType, pos, smap[seg].m_Length,
                    smap[seg].m_MinusStrand);
                pos += smap[seg].m_Length;
                break;
            }
        case CSeqMap::eSeqRef:
            {
                // Resolve references
                x_ResolveMapSegment(smap[seg].m_RefSeq,
                    smap[seg].m_RefPos, smap[seg].m_Length,
                    *dmap, pos, pos + smap[seg].m_Length,
                    *handle.m_Scope);
                break;
            }
        }
    }

    return *dmap.Release();
}


void CDataSource::x_ResolveMapSegment(CSeq_id_Handle rh,
                                      TSeqPos start, TSeqPos len,
                                      CSeqMap& dmap, TSeqPos& dpos,
                                      TSeqPos dstop,
                                      CScope& scope)
{
    CBioseq_Handle rbsh = scope.GetBioseqHandle(
        GetIdMapper().GetSeq_id(rh));
    // This tricky way of getting the seq-map is used to obtain
    // the non-const reference.
    CSeqMap& rmap = rbsh.x_GetDataSource().x_GetSeqMap(rbsh);
    // Resolve the reference map up to the end of the referenced region
    rmap.x_Resolve(start + len, scope);
    size_t rseg = rmap.x_FindSegment(start);

    // Get enough segments to cover the "start+length" range on the
    // destination sequence
    while (dpos < dstop  &&  len > 0) {
        // Get and adjust the segment start
        TSeqPos rstart = rmap[rseg].m_Position;
        TSeqPos rlength = rmap[rseg].m_Length;
        if (rstart < start) {
            rlength -= start - rstart;
            rstart = start;
        }
        if (rstart + rlength > start + len)
            rlength = start + len - rstart;
        switch (rmap[rseg].m_SegType) {
        case CSeqMap::eSeqData:
            {
                // Put the final reference
                CSeqMap::CSegmentInfo* new_seg = new CSeqMap::CSegmentInfo(
                    CSeqMap::eSeqRef, dpos, rlength, rmap[rseg].m_MinusStrand);
                new_seg->m_RefPos = rstart;
                new_seg->m_RefSeq = rh; //rmap[rseg].m_RefSeq;
                dmap.Add(*new_seg);
                dpos += rlength;
                break;
            }
        case CSeqMap::eSeqGap:
        case CSeqMap::eSeqEnd:
            {
                // Copy gaps and literals
                dmap.Add(rmap[rseg].m_SegType, dpos, rlength,
                    rmap[rseg].m_MinusStrand);
                dpos += rlength;
                break;
            }
        case CSeqMap::eSeqRef:
            {
                // Resolve multi-level references
                TSignedSeqPos rshift
                    = rmap[rseg].m_RefPos - rmap[rseg].m_Position;
                x_ResolveMapSegment(rmap[rseg].m_RefSeq,
                    rstart + rshift, rlength,
                    dmap, dpos, dstop, scope);
                break;
            }
        }
        start += rlength;
        len -= rlength;
        if (++rseg >= rmap.size())
            break;
    }
}


bool CDataSource::IsSynonym(const CSeq_id& id1, CSeq_id& id2) const
{
    CSeq_id_Handle h1 = GetIdMapper().GetHandle(id1);
    CSeq_id_Handle h2 = GetIdMapper().GetHandle(id2);
    TTSEMap::const_iterator tse_set = m_TSE_seq.find(h1);
    if (tse_set == m_TSE_seq.end())
        return false; // Could not find id1 in the datasource
    iterate ( TTSESet, tse_it, tse_set->second ) {
        const CBioseq_Info& bioseq = *(*tse_it)->m_BioseqMap[h1];
        if (bioseq.m_Synonyms.find(h2) != bioseq.m_Synonyms.end())
            return true;
    }
    return false;
}

void CDataSource::DebugDump(CDebugDumpContext ddc, unsigned int depth) const
{
    ddc.SetFrame("CDataSource");
    CObject::DebugDump( ddc, depth);

    ddc.Log("m_Loader", m_Loader,0);
    ddc.Log("m_pTopEntry", m_pTopEntry.GetPointer(),0);
    ddc.Log("m_ObjMgr", m_ObjMgr,0);

    if (depth == 0) {
        DebugDumpValue(ddc, "m_Entries.size()", m_Entries.size());
        DebugDumpValue(ddc, "m_TSE_seq.size()", m_TSE_seq.size());
        DebugDumpValue(ddc, "m_TSE_ref.size()", m_TSE_ref.size());
        DebugDumpValue(ddc, "m_SeqMaps.size()", m_SeqMaps.size());
    } else {
        DebugDumpValue(ddc, "m_Entries.type",
            "map<CRef<CSeq_entry>,CRef<CTSE_Info>>");
        DebugDumpPairsCRefCRef(ddc, "m_Entries",
            m_Entries.begin(), m_Entries.end(), depth);

        { //---  m_TSE_seq
            unsigned int depth2 = depth-1;
            DebugDumpValue(ddc, "m_TSE_seq.type",
                "map<CSeq_id_Handle,set<CRef<CTSE_Info>>>");
            CDebugDumpContext ddc2(ddc,"m_TSE_seq");
            TTSEMap::const_iterator it;
            for ( it = m_TSE_seq.begin(); it != m_TSE_seq.end(); ++it) {
                string member_name = "m_TSE_seq[ " +
                    (it->first).AsString() +" ]";
                if (depth2 == 0) {
                    member_name += ".size()";
                    DebugDumpValue(ddc2, member_name, (it->second).size());
                } else {
                    DebugDumpRangeCRef(ddc2, member_name,
                        (it->second).begin(), (it->second).end(), depth2);
                }
            }
        }

        { //---  m_TSE_ref
            unsigned int depth2 = depth-1;
            DebugDumpValue(ddc, "m_TSE_ref.type",
                "map<CSeq_id_Handle,set<CRef<CTSE_Info>>>");
            CDebugDumpContext ddc2(ddc,"m_TSE_ref");
            TTSEMap::const_iterator it;
            for ( it = m_TSE_ref.begin(); it != m_TSE_ref.end(); ++it) {
                string member_name = "m_TSE_ref[ " +
                    (it->first).AsString() +" ]";
                if (depth2 == 0) {
                    member_name += ".size()";
                    DebugDumpValue(ddc2, member_name, (it->second).size());
                } else {
                    DebugDumpRangeCRef(ddc2, member_name,
                        (it->second).begin(), (it->second).end(), depth2);
                }
            }
        }

        DebugDumpValue(ddc, "m_SeqMaps.type",
            "map<const CBioseq*,CRef<CSeqMap>>");
        DebugDumpPairsPtrCRef(ddc, "m_SeqMaps",
            m_SeqMaps.begin(), m_SeqMaps.end(), depth);
    }
}


END_SCOPE(objects)
END_NCBI_SCOPE
