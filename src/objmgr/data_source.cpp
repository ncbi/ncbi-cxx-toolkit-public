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


#include <objects/general/Int_fuzz.hpp>
#include <objects/seq/Bioseq.hpp>
#include <objects/seq/Seq_data.hpp>
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
#include <objects/seqloc/Packed_seqint.hpp>
#include <objects/seqloc/Packed_seqpnt.hpp>
#include <objects/seqloc/Seq_loc_mix.hpp>
#include <objects/seqloc/Seq_loc_equiv.hpp>
#include <objects/seqloc/Seq_bond.hpp>
#include <objects/seqalign/Dense_diag.hpp>
#include <objects/seqalign/Dense_seg.hpp>
#include <objects/seqalign/Std_seg.hpp>
#include <objects/seqalign/Packed_seg.hpp>
#include <objects/seqalign/Seq_align_set.hpp>
#include <objects/seq/seqport_util.hpp>
#include <serial/iterator.hpp>

#include <objects/objmgr1/scope.hpp>
#include "seq_id_mapper.hpp"
#include "data_source.hpp"
#include "annot_object.hpp"
#include "handle_range_map.hpp"

#include <corelib/ncbithr.hpp>

#include <algorithm>


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


static CMutex s_DataSource_Mutex;

CDataSource::CDataSource(CDataLoader& loader, CObjectManager& objmgr)
    : m_Loader(&loader), m_pTopEntry(0), m_ObjMgr(&objmgr)
{
    m_Loader->SetTargetDataSource(*this);
}


CDataSource::CDataSource(CSeq_entry& entry, CObjectManager& objmgr)
    : m_Loader(0), m_pTopEntry(&entry), m_ObjMgr(&objmgr)
{
    x_AddToBioseqMap(entry);
}


CDataSource::~CDataSource(void)
{
    // Find and drop each TSE
    while (m_Entries.size() > 0) {
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


CTSE_Info* CDataSource::x_FindBestTSE(CSeq_id_Handle handle) const
{
    TTSEMap::const_iterator tse_set = m_TSE_seq.find(handle);
    if ( tse_set == m_TSE_seq.end() )
        return 0;
    // The map should not contain empty entries
    _ASSERT(tse_set->second.size() > 0);
    iterate(TTSESet, tse, tse_set->second) {
        // Select live TSE or the only dead TSE
        if ( !(*tse)->m_Dead  ||  tse_set->second.size() == 1) {
            return *tse;
        }
    }
    throw runtime_error(
        "Multiple seq-id matches found -- can not resolve to a TSE");
}


CBioseq_Handle CDataSource::GetBioseqHandle(CScope& scope, const CSeq_id& id)
{
    if ( m_Loader ) {
        // Send request to the loader
        CSeq_loc loc;
        SerialAssign<CSeq_id>(loc.SetWhole(), id);
        if ( !m_Loader->GetRecords(loc, CDataLoader::eBioseqCore) )
            return CBioseq_Handle();
    }
    CSeq_id_Handle idh = GetIdMapper().GetHandle(id, false);
    // Do not even try to find the bioseq handle if there is no id handle
    if ( !idh )
        return CBioseq_Handle();
    CBioseq_Handle h(idh);
    CMutexGuard guard(s_DataSource_Mutex);
    CRef<CTSE_Info> tse = x_FindBestTSE(idh);
    if ( !tse )
        return CBioseq_Handle();
    TBioseqMap::iterator found = tse->m_BioseqMap.find(idh);
    _ASSERT(found != tse->m_BioseqMap.end());
    h.x_ResolveTo(scope, *this,
        *found->second->m_Entry, *tse->m_TSE);
    return h;
}


void CDataSource::FilterSeqid(TSeq_id_HandleSet& setResult,
                              TSeq_id_HandleSet& setSource) const
{
    // for each handle
    TSeq_id_HandleSet::iterator itHandle;
    for( itHandle = setSource.begin(); itHandle != setSource.end(); ) {
        _ASSERT(&setResult != &setSource);
        // if it is in my map
        if (m_TSE_seq.find(*itHandle) != m_TSE_seq.end()) {
            // move it from source to result
            setResult.insert( *itHandle);
            itHandle = setSource.erase(itHandle);
        }
        else {
            ++itHandle;
        }
    }
}


const CBioseq& CDataSource::GetBioseq(const CBioseq_Handle& handle)
{
    // Bioseq core and TSE must be loaded if there exists a handle
    //### Loader may be called to load descriptions (not included in core)
/*
    if ( m_Loader ) {
        // Send request to the loader
        CSeq_loc loc;
        CRef<CSeq_id> id = CSeqIdMapper::HandleToSeqId(handle.m_Value);
        SerialAssign<CSeq_id>(loc.SetWhole(), *id);
        m_Loader->GetRecords(loc, CDataLoader::eBioseq);
    }
*/
    CMutexGuard guard(s_DataSource_Mutex);
    // the handle must be resolved to this data source
    _ASSERT(handle.m_DataSource == this);
    return handle.m_Entry->GetSeq();
}


const CSeq_entry& CDataSource::GetTSE(const CBioseq_Handle& handle)
{
    // Bioseq and TSE must be loaded if there exists a handle
    CMutexGuard guard(s_DataSource_Mutex);
    _ASSERT(handle.m_DataSource == this);
    return *m_Entries[handle.m_Entry]->m_TSE;
}


CBioseq_Handle::TBioseqCore CDataSource::GetBioseqCore
    (const CBioseq_Handle& handle)
{
    CMutexGuard guard(s_DataSource_Mutex);
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
    CMutexGuard guard(s_DataSource_Mutex);
    return x_GetSeqMap(handle);
}


CSeqMap& CDataSource::x_GetSeqMap(const CBioseq_Handle& handle)
{
/*
    //### Call loader first
    if ( m_Loader ) {
        // Send request to the loader
        CSeq_loc loc;
        CRef<CSeq_id> id = CSeqIdMapper::HandleToSeqId(handle.m_Value);
        SerialAssign<CSeq_id>(loc.SetWhole(), *id);
        m_Loader->GetRecords(loc, CDataLoader::eBioseq???);
    }
*/
    _ASSERT(handle.m_DataSource == this);
    const CBioseq& seq = GetBioseq(handle);
    TSeqMaps::iterator found = m_SeqMaps.find(&seq);
    if (found == m_SeqMaps.end()) {
        // Create sequence map
        if ( !m_Loader ) {
            if ( seq.GetInst().IsSetSeq_data() || seq.GetInst().IsSetExt() ) {
                x_CreateSeqMap(seq);
            }
            else {
                throw runtime_error("Sequence map not found");
            }
        }
        found = m_SeqMaps.find(&seq);
    }
    (*found->second).x_CalculateSegmentLengths();
    return *found->second;
}


bool CDataSource::GetSequence(const CBioseq_Handle& handle,
                              TSeqPosition point,
                              SSeqData* seq_piece,
                              CScope& scope)
{
/*
    //### Call loader first
    if ( m_Loader ) {
        // Send request to the loader
        CSeq_loc loc;
        CRef<CSeq_id> id = CSeqIdMapper::HandleToSeqId(handle.m_Value);
        ???interval SerialAssign<CSeq_id>(loc.SetWhole(), *id);
        m_Loader->GetRecords(loc, CDataLoader::eSequence);
    }
*/
    CMutexGuard guard(s_DataSource_Mutex);
    if (handle.m_DataSource != this  &&  handle.m_DataSource != 0) {
        // Resolved to a different data source
        return false;
    }
    CSeq_entry* entry = handle.m_Entry;
    CBioseq_Handle rhandle = handle; // resolved handle for local use
    if ( !entry ) {
        CTSE_Info* info = x_FindBestTSE(rhandle.GetKey());
        if ( !info )
            return false;
        entry = info->m_BioseqMap[rhandle.GetKey()]->m_Entry;
        rhandle.x_ResolveTo(scope, *this, *entry, *info->m_TSE);
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
        CSeqMap::CSegmentInfo& seg = seqmap.x_Resolve(point, scope);
        switch (seg.m_SegType) {
        case CSeqMap::eSeqData:
            {
                _ASSERT(seg.m_RefData);
                seq_piece->dest_start = seg.m_RefPos;
                seq_piece->src_start = 0;
                seq_piece->length = seg.m_RefLen;
                seq_piece->src_data = seg.m_RefData;
                return true;
            }
        case CSeqMap::eSeqRef:
            {
                int shift = seg.m_RefPos - seg.m_position;
                if ( scope.x_GetSequence(seg.m_RefSeq,
                    point + shift, seq_piece) ) {
                    int xL = seg.m_RefLen;
                    int delta = seg.m_RefPos -
                        seq_piece->dest_start;
                    seq_piece->dest_start = seg.m_position;
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


void CDataSource::AddTSE(CSeq_entry& se)
{
    CMutexGuard guard(s_DataSource_Mutex);
    x_AddToBioseqMap(se);
}


CSeq_entry* CDataSource::x_FindEntry(const CSeq_entry& entry)
{
    CRef<CSeq_entry> ref(const_cast<CSeq_entry*>(&entry));
    TEntries::iterator found = m_Entries.find(ref);
    if (found == m_Entries.end())
        return 0;
    return found->first;
}


bool CDataSource::AttachEntry(const CSeq_entry& parent, CSeq_entry& bioseq)
{
    CMutexGuard guard(s_DataSource_Mutex);
    CSeq_entry* found = x_FindEntry(parent);
    if ( !found ) {
        return false;
    }
    CSeq_entry& entry = *found;
    _ASSERT(entry.IsSet());

    entry.SetSet().SetSeq_set().push_back(&bioseq);

    // Re-parentize, update index
    CSeq_entry* top = found;
    while ( top->GetParentEntry() ) {
        top = top->GetParentEntry();
    }
    top->Parentize();
    x_IndexEntry(bioseq, *top);
    return true;
}


bool CDataSource::AttachMap(const CSeq_entry& bioseq, CSeqMap& seqmap)
{
    CMutexGuard guard(s_DataSource_Mutex);
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
                                CSeq_data& seq,
                                TSeqPosition start,
                                TSeqLength length)
{
    //### Incomplete -- need to implement adding segmented data:
    //### find the position to insert data to, create delta-ext.
    // Get non-const reference to the entry
    CMutexGuard guard(s_DataSource_Mutex);
    CSeq_entry* found = x_FindEntry(bioseq);
    if ( !found ) {
        return false;
    }
    CSeq_entry& entry = *found;
    entry.SetSeq().SetInst().SetSeq_data(seq);
    return true;
}


bool CDataSource::AttachAnnot(const CSeq_entry& entry,
                           CSeq_annot& annot)
{
    CMutexGuard guard(s_DataSource_Mutex);
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
                    x_MapFeature(**fi, *tse);
                }
                break;
            }
        case CSeq_annot::C_Data::e_Align:
            {
                iterate ( CSeq_annot::C_Data::TAlign, ai,
                    annot.GetData().GetAlign() ) {
                    x_MapAlign(**ai, *tse);
                }
                break;
            }
        case CSeq_annot::C_Data::e_Graph:
            {
                iterate ( CSeq_annot::C_Data::TGraph, gi,
                    annot.GetData().GetGraph() ) {
                    x_MapGraph(**gi, *tse);
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


void CDataSource::x_AddToBioseqMap(CSeq_entry& entry)
{
    // Search for bioseqs, add each to map
    entry.Parentize();
    x_IndexEntry(entry, entry);
    x_AddToAnnotMap(entry);
}


void CDataSource::x_IndexEntry(CSeq_entry& entry, CSeq_entry& tse)
{
    CTSE_Info* tse_info = 0;
    TEntries::iterator found_tse = m_Entries.find(&tse);
    if (found_tse == m_Entries.end()) {
        // New TSE info
        tse_info = new CTSE_Info;
        tse_info->m_TSE = &tse;
        tse_info->m_Dead = false;
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
            x_IndexEntry(**it, tse);
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
                    x_MapFeature(**it, *tse);
                }
                break;
            }
        case CSeq_annot::C_Data::e_Align:
            {
                iterate ( CSeq_annot::C_Data::TAlign, it,
                    (*ai)->GetData().GetAlign() ) {
                    x_MapAlign(**it, *tse);
                }
                break;
            }
        case CSeq_annot::C_Data::e_Graph:
            {
                iterate ( CSeq_annot::C_Data::TGraph, it,
                    (*ai)->GetData().GetGraph() ) {
                    x_MapGraph(**it, *tse);
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
            seq.GetInst().GetLength(), *seqmap);
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
    for (int i = 0; i < seqmap->size(); i++) {
        // Lock seq-id handles
        const CSeqMap::CSegmentInfo& info = (*seqmap)[i];
    }
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
            seg->m_RefLen = loc.GetInt().GetLength();
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
            seg->m_RefLen = 1;
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
                seg->m_RefLen = (*ii)->GetLength();
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
                seg->m_RefLen = 1;
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
    seg->m_RefData.Reset(&data);
    seg->m_RefPos = pos;
    seg->m_RefLen = len;
    seqmap.Add(*seg);
    pos += len;
}


void CDataSource::x_MapFeature(const CSeq_feat& feat, CTSE_Info& tse)
{
    // Create annotation object. It will split feature location
    // to a handle-ranges map.
    CAnnotObject* aobj = new CAnnotObject(*this, feat);
    // Iterate handles
    iterate ( CHandleRangeMap::TLocMap,
        mapit, aobj->GetRangeMap().GetMap() ) {
        m_TSE_ref[mapit->first].insert(&tse);
        tse.m_AnnotMap[mapit->first].insert(TRangeMap::value_type(
            mapit->second.GetOverlappingRange(), aobj));
    }
}


void CDataSource::x_MapAlign(const CSeq_align& align, CTSE_Info& tse)
{
    // Create annotation object. It will process the align locations
    CAnnotObject* aobj = new CAnnotObject(*this, align);
    // Iterate handles
    iterate ( CHandleRangeMap::TLocMap,
        mapit, aobj->GetRangeMap().GetMap() ) {
        m_TSE_ref[mapit->first].insert(&tse);
        tse.m_AnnotMap[mapit->first].insert(TRangeMap::value_type(
            mapit->second.GetOverlappingRange(), aobj));
    }
}


void CDataSource::x_MapGraph(const CSeq_graph& graph, CTSE_Info& tse)
{
    // Create annotation object. It will split graph location
    // to a handle-ranges map.
    CAnnotObject* aobj = new CAnnotObject(*this, graph);
    // Iterate handles
    iterate ( CHandleRangeMap::TLocMap,
        mapit, aobj->GetRangeMap().GetMap() ) {
        m_TSE_ref[mapit->first].insert(&tse);
        tse.m_AnnotMap[mapit->first].insert(TRangeMap::value_type(
            mapit->second.GetOverlappingRange(), aobj));
    }
}


void CDataSource::PopulateTSESet(CHandleRangeMap& loc,
                                 TTSESet& tse_set) const
{
    x_ResolveLocationHandles(loc);
    iterate(CHandleRangeMap::TLocMap, hit, loc.GetMap()) {
        // Search for each seq-id handle from loc in the indexes
        TTSEMap::const_iterator tse_it = m_TSE_ref.find(hit->first);
        if (tse_it == m_TSE_ref.end())
            continue; // No this seq-id in the datasource
        _ASSERT(tse_it->second.size() > 0);
        TTSESet unfiltered;
        const CTSE_Info* live = 0;
        iterate(TTSESet, tse, tse_it->second) {
            unfiltered.insert(*tse);
            if ( !(*tse)->m_Dead )
                live = tse->GetPointer();
        }
        iterate(TTSESet, tse, unfiltered) {
            //### Apply TSE filtering rules for annotations
            tse_set.insert(*tse);
        }
    }
}


void CDataSource::x_ResolveLocationHandles(CHandleRangeMap& loc) const
{
    CMutexGuard guard(s_DataSource_Mutex);
    CHandleRangeMap tmp(GetIdMapper());
    iterate ( CHandleRangeMap::TLocMap, it, loc.GetMap() ) {
        CTSE_Info* tse_info = x_FindBestTSE(it->first);
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
    CMutexGuard guard(s_DataSource_Mutex);
    // Allow to drop top-level seq-entries only
    _ASSERT(tse.GetParentEntry() == 0);

    CSeq_entry* entry = x_FindEntry(tse);
    if ( !entry )
        return false;
    x_DropEntry(*entry);
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
                    x_DropFeature(**it);
                }
                break;
            }
        case CSeq_annot::C_Data::e_Align:
            {
                iterate ( CSeq_annot::C_Data::TAlign, it,
                    (*ai)->GetData().GetAlign() ) {
                    x_DropAlign(**it);
                }
                break;
            }
        case CSeq_annot::C_Data::e_Graph:
            {
                iterate ( CSeq_annot::C_Data::TGraph, it,
                    (*ai)->GetData().GetGraph() ) {
                    x_DropGraph(**it);
                }
                break;
            }
        }
    }
}


void CDataSource::x_DropFeature(const CSeq_feat& feat)
{
    // Create a copy of annot object to iterate all seq-id handles
    CRef<CAnnotObject> aobj(new CAnnotObject(*this, feat));
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


void CDataSource::x_DropAlign(const CSeq_align& align)
{
    // Create a copy of annot object to iterate all seq-id handles
    CRef<CAnnotObject> aobj(new CAnnotObject(*this, align));
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


void CDataSource::x_DropGraph(const CSeq_graph& graph)
{
    // Create a copy of annot object to iterate all seq-id handles
    CRef<CAnnotObject> aobj(new CAnnotObject(*this, graph));
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



END_SCOPE(objects)
END_NCBI_SCOPE
