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
*/


#include <objects/objmgr/impl/data_source.hpp>
#include <objects/objmgr/impl/annot_object.hpp>
#include <objects/objmgr/impl/handle_range_map.hpp>
#include <objects/objmgr/impl/tse_info.hpp>
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


CDataSource::CDataSource(CDataLoader& loader, CObjectManager& objmgr)
    : m_Loader(&loader), m_pTopEntry(0), m_ObjMgr(&objmgr)
{
    m_Loader->SetTargetDataSource(*this);
}


CDataSource::CDataSource(CSeq_entry& entry, CObjectManager& objmgr)
    : m_Loader(0), m_pTopEntry(&entry), m_ObjMgr(&objmgr),
      m_IndexedAnnot(false)
{
    x_AddToBioseqMap(entry, false, 0);
}


CDataSource::~CDataSource(void)
{
    // Find and drop each TSE
    while ( !m_Entries.empty() ) {
        _ASSERT( !m_Entries.begin()->second->CounterLocked() );
        DropTSE(*(m_Entries.begin()->second->m_TSE));
    }
    if(m_Loader) delete m_Loader;
}


CTSE_Lock
CDataSource::x_FindBestTSE(const CSeq_id_Handle& handle,
                           const CScope::TRequestHistory& history) const
{
//### Don't forget to unlock TSE in the calling function!
    TTSESet* p_tse_set = 0;
    CMutexGuard ds_guard(m_DataSource_Mtx);
    TTSEMap::const_iterator tse_set = m_TSE_seq.find(handle);
    if ( tse_set == m_TSE_seq.end() ) {
        return CTSE_Lock();
    }
    p_tse_set = const_cast<TTSESet*>(&tse_set->second);
    if ( p_tse_set->size() == 1) {
        // There is only one TSE, no matter live or dead
        return CTSE_Lock(const_cast<CTSE_Info&>(**p_tse_set->begin()));
    }
    // The map should not contain empty entries
    _ASSERT(p_tse_set->size() > 0);
    TTSESet live;
    CTSE_Lock from_history;
    ITERATE(TTSESet, tse, *p_tse_set) {
        // Check history
        CScope::TRequestHistory::const_iterator hst =
            history.find(CTSE_Lock(*tse));
        if (hst != history.end()) {
            if ( from_history ) {
                THROW1_TRACE(runtime_error,
                             "CDataSource::x_FindBestTSE() -- "
                             "Multiple history matches");
            }
            from_history.Set(*tse);
        }
        // Find live TSEs
        if ( !(*tse)->m_Dead ) {
            // Make sure there is only one live TSE
            live.insert(*tse);
        }
    }
    // History is always the best choice
    if (from_history) {
        return from_history;
    }

    // Check live
    if (live.size() == 1) {
        // There is only one live TSE -- ok to use it
        return CTSE_Lock(*live.begin());
    }
    else if ((live.size() == 0)  &&  m_Loader) {
        // No live TSEs -- try to select the best dead TSE
        CRef<CTSE_Info> best(m_Loader->ResolveConflict(handle, *p_tse_set));
        if ( best ) {
            return CTSE_Lock(*p_tse_set->find(best));
        }
        THROW1_TRACE(runtime_error,
                     "CDataSource::x_FindBestTSE() -- "
                     "Multiple seq-id matches found");
    }
    // Multiple live TSEs -- try to resolve the conflict (the status of some
    // TSEs may change)
    CRef<CTSE_Info> best(m_Loader->ResolveConflict(handle, live));
    if ( best ) {
        return CTSE_Lock(*p_tse_set->find(best));
    }
    THROW1_TRACE(runtime_error,
                 "CDataSource::x_FindBestTSE() -- "
                 "Multiple live entries found");
}


CBioseq_Handle CDataSource::GetBioseqHandle(CScope& scope,
                                            CSeqMatch_Info& info)
{
    // The TSE is locked by the scope, so, it can not be deleted.
    //### CMutexGuard guard(sm_DataSource_Mtx);
    TBioseqMap::iterator found =
        info->m_BioseqMap.find(info.GetIdHandle());
    if ( found == info->m_BioseqMap.end() )
        return CBioseq_Handle();
    CBioseq_Handle h(info.GetIdHandle());
    h.x_ResolveTo(scope, *found->second);
    // Locked by BestResolve() in CScope::x_BestResolve()
    return h;
}


void CDataSource::FilterSeqid(TSeq_id_HandleSet& setResult,
                              TSeq_id_HandleSet& setSource) const
{
    _ASSERT(&setResult != &setSource);
    // for each handle
    TSeq_id_HandleSet::iterator itHandle;
    // CMutexGuard guard(m_DataSource_Mtx);
    for( itHandle = setSource.begin(); itHandle != setSource.end(); ) {
        // if it is in my map
        if (m_TSE_seq.find(*itHandle) != m_TSE_seq.end()) {
            //### The id handle is reported to be good, but it can be deleted
            //### by the next request!
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
        CHandleRangeMap hrm;
        hrm.AddRange(handle.m_Value,
                     CHandleRange::TRange::GetWhole(), eNa_strand_unknown);
        m_Loader->GetRecords(hrm, CDataLoader::eBioseq);
    }
    //### CMutexGuard guard(sm_DataSource_Mtx);
    // the handle must be resolved to this data source
    _ASSERT(&handle.x_GetDataSource() == this);
    return handle.m_Bioseq_Info->m_Entry->GetSeq();
}


const CSeq_entry& CDataSource::GetTSE(const CBioseq_Handle& handle)
{
    // Bioseq and TSE must be loaded if there exists a handle
    //### CMutexGuard guard(sm_DataSource_Mtx);
    _ASSERT(&handle.x_GetDataSource() == this);
    return *m_Entries[CRef<CSeq_entry>(handle.m_Bioseq_Info->m_Entry)]->m_TSE;
}


CTSE_Lock CDataSource::GetTSEInfo(const CSeq_entry* entry)
{
    CConstRef<CSeq_entry> ref(entry);
    CMutexGuard guard(m_DataSource_Mtx);
    TEntries::iterator found = m_Entries.find(ref);
    if (found == m_Entries.end())
        return CTSE_Lock();
    return CTSE_Lock(found->second.GetPointerOrNull());
}


CBioseq_Handle::TBioseqCore
CDataSource::GetBioseqCore(const CBioseq_Handle& handle)
{
    // Bioseq core and TSE must be loaded if there exists a handle --
    // just return the bioseq as-is.
    // the handle must be resolved to this data source
    _ASSERT(&handle.x_GetDataSource() == this);
    return CBioseq_Handle::TBioseqCore(&handle.m_Bioseq_Info->m_Entry->GetSeq());
}


const CSeqMap& CDataSource::GetSeqMap(const CBioseq_Handle& handle)
{
    CMutexGuard guard(m_DataSource_Mtx);
    return x_GetSeqMap(handle);
}


CSeqMap& CDataSource::x_GetSeqMap(const CBioseq_Handle& handle)
{
    _ASSERT(&handle.x_GetDataSource() == this);
    // No need to lock anything since the TSE should be locked by the handle
    CBioseq_Handle::TBioseqCore core = GetBioseqCore(handle);
    //### Lock seq-maps to prevent duplicate seq-map creation
    CMutexGuard guard(m_DataSource_Mtx);    
    TSeqMaps::iterator found = m_SeqMaps.find(core);
    if (found == m_SeqMaps.end()) {
        // Call loader first
        if ( m_Loader ) {
            // Send request to the loader
            CHandleRangeMap hrm;
            hrm.AddRange(handle.m_Value,
                         CHandleRange::TRange::GetWhole(), eNa_strand_unknown);
            m_Loader->GetRecords(hrm, CDataLoader::eBioseq); //### or eCore???
        }

        // Create sequence map
        x_CreateSeqMap(GetBioseq(handle), *handle.m_Scope);
        found = m_SeqMaps.find(core);
        if (found == m_SeqMaps.end()) {
            THROW1_TRACE(runtime_error,
                         "CDataSource::x_GetSeqMap() -- "
                         "Sequence map not found");
        }
    }
    _ASSERT(found != m_SeqMaps.end());
    return *found->second;
}


CTSE_Info* CDataSource::AddTSE(CSeq_entry& se, TTSESet* tse_set, bool dead)
{
    //### CMutexGuard guard(sm_DataSource_Mtx);
    return x_AddToBioseqMap(se, dead, tse_set);
}


CSeq_entry* CDataSource::x_FindEntry(const CSeq_entry& entry)
{
    CConstRef<CSeq_entry> ref(&entry);
    //### CMutexGuard guard(sm_DataSource_Mtx);
    //### Lock the entries list to prevent "found" destruction
    CMutexGuard guard(m_DataSource_Mtx);
    TEntries::iterator found = m_Entries.find(ref);
    if (found == m_Entries.end())
        return 0;
    return const_cast<CSeq_entry*>(found->first.GetPointerOrNull());
}


bool CDataSource::AttachEntry(const CSeq_entry& parent, CSeq_entry& bioseq)
{
    //### CMutexGuard guard(sm_DataSource_Mtx);

    //### Lock the entry to prevent destruction or modification ???
    //### May need to find and lock the TSE_Info for this.
    CSeq_entry* found = x_FindEntry(parent);
    if ( !found ) {
        return false;
    }
    // Lock the TSE
    CTSE_Guard tse_guard(*m_Entries[CConstRef<CSeq_entry>(found)]);
    _ASSERT(found  &&  found->IsSet());

    found->SetSet().SetSeq_set().push_back(CRef<CSeq_entry>(&bioseq));

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
    //### CMutexGuard guard(sm_DataSource_Mtx);
    //### Lock the entry to prevent destruction or modification
    //### May need to lock the TSE instead.
    CSeq_entry* found = x_FindEntry(bioseq);
    if ( !found ) {
        return false;
    }
    // Lock the TSE
    CTSE_Guard tse_guard(*m_Entries[CConstRef<CSeq_entry>(found)]);

    CSeq_entry& entry = *found;
    _ASSERT(entry.IsSeq());
    m_SeqMaps[CBioseq_Handle::TBioseqCore(&entry.GetSeq())].Reset(&seqmap);
    return true;
}


bool CDataSource::AttachAnnot(const CSeq_entry& entry,
                              CSeq_annot& annot)
{
    //### CMutexGuard guard(sm_DataSource_Mtx);
    //### Lock the entry to prevent destruction or modification
    //### May need to lock the TSE instead. In this case also lock
    //### the entries list for a while.
    CSeq_entry* found = x_FindEntry(entry);
    if ( !found ) {
        return false;
    }

    CTSE_Info* tse = m_Entries[CRef<CSeq_entry>(found)];
    //### Lock the TSE !!!!!!!!!!!
    CTSE_Guard tse_guard(*tse);

    CBioseq_set::TAnnot* annot_list = 0;
    if ( found->IsSet() ) {
        annot_list = &found->SetSet().SetAnnot();
    }
    else {
        annot_list = &found->SetSeq().SetAnnot();
    }
    annot_list->push_back(CRef<CSeq_annot>(&annot));

    if ( !m_IndexedAnnot )
        return true; // skip annotations indexing

    switch ( annot.GetData().Which() ) {
        case CSeq_annot::C_Data::e_Ftable:
            {
                CRef<CSeq_annot_Info> annot_ref(
                    new CSeq_annot_Info(this, annot, &entry));
                ITERATE ( CSeq_annot::C_Data::TFtable, fi,
                    annot.GetData().GetFtable() ) {
                    x_MapFeature(**fi, *annot_ref, *tse);
                }
                break;
            }
        case CSeq_annot::C_Data::e_Align:
            {
                CRef<CSeq_annot_Info> annot_ref(
                    new CSeq_annot_Info(this, annot, &entry));
                ITERATE ( CSeq_annot::C_Data::TAlign, ai,
                    annot.GetData().GetAlign() ) {
                    x_MapAlign(**ai, *annot_ref, *tse);
                }
                break;
            }
        case CSeq_annot::C_Data::e_Graph:
            {
                CRef<CSeq_annot_Info> annot_ref(
                    new CSeq_annot_Info(this, annot, &entry));
                ITERATE ( CSeq_annot::C_Data::TGraph, gi,
                    annot.GetData().GetGraph() ) {
                    x_MapGraph(**gi, *annot_ref, *tse);
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


bool CDataSource::RemoveAnnot(const CSeq_entry& entry, const CSeq_annot& annot)
{
    if ( m_Loader ) {
        THROW1_TRACE(runtime_error,
            "CDataSource::RemoveAnnot() -- can not modify a loaded entry");
    }
    CSeq_entry* found = x_FindEntry(entry);
    if ( !found ) {
        return false;
    }

    CTSE_Info* tse = m_Entries[CRef<CSeq_entry>(found)];
    //### Lock the TSE !!!!!!!!!!!
    CTSE_Guard tse_guard(*tse);
    x_DropAnnotMapRecursive(*tse->m_TSE);
    m_IndexedAnnot = false;
    tse->SetIndexed(false);

    if ( found->IsSet() ) {
        found->SetSet().SetAnnot().
            remove(CRef<CSeq_annot>(&const_cast<CSeq_annot&>(annot)));
        if (found->SetSet().SetAnnot().size() == 0) {
            found->SetSet().ResetAnnot();
        }
    }
    else {
        found->SetSeq().SetAnnot().
            remove(CRef<CSeq_annot>(&const_cast<CSeq_annot&>(annot)));
        if (found->SetSeq().SetAnnot().size() == 0) {
            found->SetSeq().ResetAnnot();
        }
    }
    return true;
}


bool CDataSource::ReplaceAnnot(const CSeq_entry& entry,
                               const CSeq_annot& old_annot,
                               CSeq_annot& new_annot)
{
    if ( !RemoveAnnot(entry, old_annot) )
        return false;
    return AttachAnnot(entry, new_annot);
}


CTSE_Info* CDataSource::x_AddToBioseqMap(CSeq_entry& entry,
                                         bool dead,
                                         TTSESet* tse_set)
{
    // Search for bioseqs, add each to map
    //### The entry is not locked here
    entry.Parentize();
    CTSE_Info* info = x_IndexEntry(entry, entry, dead, tse_set);
    // Do not index new TSEs -- wait for annotations request
    _ASSERT(!info->IsIndexed());  // Can not be indexed - it's a new one
    m_IndexedAnnot = false;       // reset the flag to enable indexing
    return info;
}


CTSE_Info* CDataSource::x_IndexEntry(CSeq_entry& entry, CSeq_entry& tse,
                                     bool dead,
                                     TTSESet* tse_set)
{
    CTSE_Lock tse_info;
    // Lock indexes to prevent duplicate indexing
    CMutexGuard entries_guard(m_DataSource_Mtx);    

    TEntries::iterator found_tse = m_Entries.find(CRef<CSeq_entry>(&tse));
    if (found_tse == m_Entries.end()) {
        // New TSE info
        tse_info.Set(new CTSE_Info);
        tse_info->m_TSE = &tse;
        tse_info->m_Dead = dead;
        tse_info->m_DataSource = this;
        // Do not lock TSE if there is no tse_set -- none will unlock it
    }
    else {
        // existing TSE info
        tse_info.Set(found_tse->second);
    }
    _ASSERT(tse_info);
    CTSE_Guard tse_guard(*tse_info);
     
    m_Entries[CRef<CSeq_entry>(&entry)].Reset(&*tse_info);
    if ( entry.IsSeq() ) {
        CBioseq* seq = &entry.SetSeq();
        CBioseq_Info* info = new CBioseq_Info(entry);
        info->m_TSE_Info = tse_info.GetPointer();
        ITERATE ( CBioseq::TId, id, seq->GetId() ) {
            // Find the bioseq index
            CSeq_id_Handle key = GetSeq_id_Mapper().GetHandle(**id);
            TTSESet& seq_tse_set = m_TSE_seq[key];
            TTSESet::iterator tse_it = seq_tse_set.begin();
            for ( ; tse_it != seq_tse_set.end(); ++tse_it) {
                if (const_cast< CRef<CSeq_entry>& >((*tse_it)->m_TSE) == &tse)
                    break;
            }
            if (tse_it == seq_tse_set.end()) {
                seq_tse_set.insert(CRef<CTSE_Info>(&*tse_info));
            }
            else {
                tse_info.Set(const_cast<CTSE_Info&>(**tse_it));
            }
            TBioseqMap::iterator found = tse_info->m_BioseqMap.find(key);
            if ( found != tse_info->m_BioseqMap.end() ) {
                // No duplicate bioseqs in the same TSE
                string sid1, sid2, si_conflict;
                {{
                    CNcbiOstrstream os;
                    ITERATE ( CBioseq::TId, id_it,
                              found->second->m_Entry->GetSeq().GetId() ) {
                        os << (*id_it)->DumpAsFasta() << " | ";
                    }
                    sid1 = CNcbiOstrstreamToString(os);
                }}
                {{
                    CNcbiOstrstream os;
                    ITERATE (CBioseq::TId, id_it, seq->GetId()) {
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
        ITERATE ( CBioseq_Info::TSynonyms, syn, info->m_Synonyms ) {
            tse_info->m_BioseqMap.insert
                (TBioseqMap::value_type(*syn,
                                        CRef<CBioseq_Info>(info)));
        }
    }
    else {
        ITERATE ( CBioseq_set::TSeq_set, it, entry.GetSet().GetSeq_set() ) {
            x_IndexEntry(const_cast<CSeq_entry&>(**it), tse, dead, 0);
        }
    }
    if (tse_set) {
        tse_set->insert(CRef<CTSE_Info>(&*tse_info));
    }
    return &*tse_info;
}


void CDataSource::x_AddToAnnotMap(CSeq_entry& entry, CTSE_Info* info)
{
    // The entry must be already in the m_Entries map
    // Lock indexes
    CMutexGuard guard(m_DataSource_Mtx);    

    CTSE_Info* tse = info ? info : m_Entries[CRef<CSeq_entry>(&entry)];
    CTSE_Guard tse_guard(*tse);
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
        ITERATE ( CBioseq_set::TSeq_set, it, entry.GetSet().GetSeq_set() ) {
            x_AddToAnnotMap(const_cast<CSeq_entry&>(**it), tse);
        }
    }
    if ( !annot_list ) {
        return;
    }
    ITERATE ( CBioseq::TAnnot, ai, *annot_list ) {
        switch ( (*ai)->GetData().Which() ) {
        case CSeq_annot::C_Data::e_Ftable:
            {
                CRef<CSeq_annot_Info> annot_ref(
                    new CSeq_annot_Info(this, **ai, &entry));
                ITERATE ( CSeq_annot::C_Data::TFtable, it,
                    (*ai)->GetData().GetFtable() ) {
                    x_MapFeature(**it, *annot_ref, *tse);
                }
                break;
            }
        case CSeq_annot::C_Data::e_Align:
            {
                CRef<CSeq_annot_Info> annot_ref(
                    new CSeq_annot_Info(this, **ai, &entry));
                ITERATE ( CSeq_annot::C_Data::TAlign, it,
                    (*ai)->GetData().GetAlign() ) {
                    x_MapAlign(**it, *annot_ref, *tse);
                }
                break;
            }
        case CSeq_annot::C_Data::e_Graph:
            {
                CRef<CSeq_annot_Info> annot_ref(
                    new CSeq_annot_Info(this, **ai, &entry));
                ITERATE ( CSeq_annot::C_Data::TGraph, it,
                    (*ai)->GetData().GetGraph() ) {
                    x_MapGraph(**it, *annot_ref, *tse);
                }
                break;
            }
        }
    }
}


void PrintSeqMap(const string& /*id*/, const CSeqMap& /*smap*/)
{
#if _DEBUG && 0
    _TRACE("CSeqMap("<<id<<"):");
    ITERATE ( CSeqMap, it, smap ) {
        switch ( it.GetType() ) {
        case CSeqMap::eSeqGap:
            _TRACE("    gap: "<<it.GetLength());
            break;
        case CSeqMap::eSeqData:
            _TRACE("    data: "<<it.GetLength());
            break;
        case CSeqMap::eSeqRef:
            _TRACE("    ref: "<<it.GetRefSeqid().AsString()<<' '<<
                   it.GetRefPosition()<<' '<<it.GetLength()<<' '<<
                   it.GetRefMinusStrand());
            break;
        default:
            _TRACE("    bad: "<<it.GetType()<<' '<<it.GetLength());
            break;
        }
    }
    _TRACE("end of CSeqMap "<<id);
#endif
}


void CDataSource::x_CreateSeqMap(const CBioseq& seq, CScope& /*scope*/)
{
    //### Make sure the bioseq is not deleted while creating the seq-map
    CConstRef<CBioseq> guard(&seq);
    CConstRef<CSeqMap> seqMap =
        CSeqMap::CreateSeqMapForBioseq(const_cast<CBioseq&>(seq), this);
    m_SeqMaps[CBioseq_Handle::TBioseqCore(&seq)]
        .Reset(const_cast<CSeqMap*>(seqMap.GetPointer()));
}


void CDataSource::UpdateAnnotIndex(const CHandleRangeMap& loc,
                                   CSeq_annot::C_Data::E_Choice sel)
{
    //### Lock all TSEs found, unlock all filtered out in the end.
    if ( m_Loader ) {
        // Send request to the loader
        switch ( sel ) {
        case CSeq_annot::C_Data::e_Ftable:
            m_Loader->GetRecords(loc, CDataLoader::eFeatures);
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
    // Index all annotations if not indexed yet
    if ( !m_IndexedAnnot ) {
        NON_CONST_ITERATE (TEntries, tse_it, m_Entries) {
            //### Lock TSE so that another thread can not index it too
            CTSE_Guard guard(*tse_it->second);
            if ( !tse_it->second->IsIndexed() )
                x_AddToAnnotMap(*tse_it->second->m_TSE, tse_it->second);
        }
        m_IndexedAnnot = true;
    }
}


void CDataSource::GetSynonyms(const CSeq_id_Handle& main_idh,
                              set<CSeq_id_Handle>& syns,
                              CScope& scope)
{
    //### The TSE returns locked, unlock it
    CSeq_id_Handle idh = main_idh;
    CTSE_Lock tse_info = x_FindBestTSE(main_idh, scope.m_History);
    if ( !tse_info ) {
        // Try to find the best matching id (not exactly equal)
        const CSeq_id& id = idh.GetSeqId();
        TSeq_id_HandleSet hset;
        GetSeq_id_Mapper().GetMatchingHandles(id, hset);
        ITERATE(TSeq_id_HandleSet, hit, hset) {
            if ( tse_info  &&  idh.IsBetter(*hit) )
                continue;
            CTSE_Lock tmp_tse = x_FindBestTSE(*hit, scope.m_History);
            if ( tmp_tse ) {
                tse_info = tmp_tse;
                idh = *hit;
            }
        }
    }
    // At this point the tse_info (if not null) should be locked
    if ( tse_info ) {
        TBioseqMap::const_iterator info = tse_info->m_BioseqMap.find(idh);
        if (info == tse_info->m_BioseqMap.end()) {
            // Just copy the existing id
            syns.insert(main_idh);
        }
        else {
            // Create range list for each synonym of a seq_id
            const CBioseq_Info::TSynonyms& syn = info->second->m_Synonyms;
            ITERATE ( CBioseq_Info::TSynonyms, syn_it, syn ) {
                syns.insert(*syn_it);
            }
        }
    }
    else {
        // Just copy the existing range map
        syns.insert(main_idh);
    }
}


void CDataSource::GetTSESetWithAnnots(const CSeq_id_Handle& idh,
                                      set<CTSE_Lock>& tse_set,
                                      CScope& scope)
{
    TTSESet tmp_tse_set;
    TTSESet non_history;
    CMutexGuard guard(m_DataSource_Mtx);    

    {{
        TTSEMap::const_iterator tse_it = m_TSE_ref.find(idh);
        if (tse_it != m_TSE_ref.end()) {
            _ASSERT(tse_it->second.size() > 0);
            TTSESet selected_with_ref;
            TTSESet selected_with_seq;
            ITERATE(TTSESet, tse, tse_it->second) {
                if ( (*tse)->m_BioseqMap.find(idh) !=
                     (*tse)->m_BioseqMap.end() ) {
                    selected_with_seq.insert(*tse); // with sequence
                }
                else
                    selected_with_ref.insert(*tse); // with reference
            }

            CRef<CTSE_Info> unique_from_history;
            CRef<CTSE_Info> unique_live;
            ITERATE (TTSESet, with_seq, selected_with_seq) {
                if ( scope.m_History.find(CTSE_Lock(*with_seq)) !=
                     scope.m_History.end() ) {
                    if ( unique_from_history ) {
                        THROW1_TRACE(runtime_error,
                                     "CDataSource::GetTSESetWithAnnots() -- "
                                     "Ambiguous request: "
                                     "multiple history matches");
                    }
                    unique_from_history = *with_seq;
                }
                else if ( !unique_from_history ) {
                    if ((*with_seq)->m_Dead)
                        continue;
                    if ( unique_live ) {
                        THROW1_TRACE(runtime_error,
                                     "CDataSource::GetTSESetWithAnnots() -- "
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
                             "CDataSource::GetTSESetWithAnnots() -- "
                             "Ambigous request: multiple TSEs found");
            }
            ITERATE(TTSESet, tse, selected_with_ref) {
                bool in_history =
                    scope.m_History.find(CTSE_Lock(*tse)) !=
                    scope.m_History.end();
                if ( !(*tse)->m_Dead  || in_history ) {
                    // Select only TSEs present in the history and live TSEs
                    // Different sets for in-history and non-history TSEs for
                    // the future filtering.
                    if ( in_history ) {
                        tmp_tse_set.insert(*tse); // in-history TSE
                    }
                    else {
                        non_history.insert(*tse); // non-history TSE
                    }
                }
            }
        }
    }}

    // Filter out TSEs not in the history yet and conflicting with any
    // history TSE. The conflict may be caused by any seq-id, even not
    // mentioned in the searched location.
    ITERATE (TTSESet, tse_it, non_history) {
        bool conflict = false;
        // Check each seq-id from the current TSE
        ITERATE (CTSE_Info::TBioseqMap, seq_it, (*tse_it)->m_BioseqMap) {
            ITERATE (CScope::TRequestHistory, hist_it, scope.m_History) {
                conflict =
                    (*hist_it)->m_BioseqMap.find(seq_it->first) !=
                    (*hist_it)->m_BioseqMap.end();
                if ( conflict )
                    break;
            }
            if ( conflict )
                break;
        }
        if ( !conflict ) {
            // No conflicts found -- add the TSE to the resulting set
            tmp_tse_set.insert(*tse_it);
        }
    }
    ITERATE (TTSESet, lit, tmp_tse_set) {
        tse_set.insert(CTSE_Lock(*lit));
    }
}


bool CDataSource::DropTSE(const CSeq_entry& tse)
{
    // Allow to drop top-level seq-entries only
    _ASSERT(tse.GetParentEntry() == 0);

    CRef<CSeq_entry> ref(const_cast<CSeq_entry*>(&tse));

    // Lock indexes
    CMutexGuard guard(m_DataSource_Mtx);    

    TEntries::iterator found = m_Entries.find(ref);
    if (found == m_Entries.end())
        return false;
    if ( found->second->CounterLocked() )
        return false; // Not really dropped, although found
    if ( m_Loader )
        m_Loader->DropTSE(found->first);
    x_DropEntry(const_cast<CSeq_entry&>(*found->first));
    return true;
}


void CDataSource::x_DropEntry(CSeq_entry& entry)
{
    CSeq_entry* tse = &entry;
    while ( tse->GetParentEntry() ) {
        tse = tse->GetParentEntry();
    }
    if ( entry.IsSeq() ) {
        CBioseq& seq = entry.SetSeq();
        CSeq_id_Handle key;
        ITERATE ( CBioseq::TId, id, seq.GetId() ) {
            // Find TSE and bioseq positions
            key = GetSeq_id_Mapper().GetHandle(**id);
            TTSEMap::iterator tse_set = m_TSE_seq.find(key);
            _ASSERT(tse_set != m_TSE_seq.end());
            //### No need to lock the TSE
            //### since the whole DS is locked by DropTSE()
            //### CMutexGuard guard(sm_TSESet_MP.GetMutex(&tse_set->second));
            TTSESet::iterator tse_it = tse_set->second.begin();
            for ( ; tse_it != tse_set->second.end(); ++tse_it) {
                if (const_cast<CSeq_entry*>((*tse_it)->m_TSE.GetPointer()) ==
                    const_cast<CSeq_entry*>(tse))
                    break;
            }
            _ASSERT(tse_it != tse_set->second.end());
            TBioseqMap::iterator found =
                const_cast<TBioseqMap&>((*tse_it)->m_BioseqMap).find(key);
            _ASSERT( found !=
                     const_cast<TBioseqMap&>((*tse_it)->m_BioseqMap).end() );
            const_cast<TBioseqMap&>((*tse_it)->m_BioseqMap).erase(found);
            // Remove TSE index for the bioseq (the TSE may still be
            // in other id indexes).
            tse_set->second.erase(tse_it);
            if (tse_set->second.size() == 0) {
                m_TSE_seq.erase(tse_set);
            }
        }
        TSeqMaps::iterator map_it =
            m_SeqMaps.find(CBioseq_Handle::TBioseqCore(&seq));
        if (map_it != m_SeqMaps.end()) {
            m_SeqMaps.erase(map_it);
        }
        x_DropAnnotMap(entry);
    }
    else {
        ITERATE ( CBioseq_set::TSeq_set, it, entry.GetSet().GetSeq_set() ) {
            x_DropEntry(const_cast<CSeq_entry&>(**it));
        }
        x_DropAnnotMap(entry);
    }
    m_Entries.erase(CRef<CSeq_entry>(&entry));
}


void CDataSource::x_DropAnnotMapRecursive(const CSeq_entry& entry)
{
    if ( entry.IsSet() ) {
        ITERATE ( CBioseq_set::TSeq_set, it, entry.GetSet().GetSeq_set() ) {
            x_DropAnnotMapRecursive(**it);
        }
    }
    x_DropAnnotMap(entry);
}


void CDataSource::x_DropAnnotMap(const CSeq_entry& entry)
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
    ITERATE ( CBioseq::TAnnot, ai, *annot_list ) {
        switch ( (*ai)->GetData().Which() ) {
        case CSeq_annot::C_Data::e_Ftable:
            {
                ITERATE ( CSeq_annot::C_Data::TFtable, it,
                    (*ai)->GetData().GetFtable() ) {
                    x_DropFeature(**it, **ai, &entry);
                }
                break;
            }
        case CSeq_annot::C_Data::e_Align:
            {
                ITERATE ( CSeq_annot::C_Data::TAlign, it,
                    (*ai)->GetData().GetAlign() ) {
                    x_DropAlign(**it, **ai, &entry);
                }
                break;
            }
        case CSeq_annot::C_Data::e_Graph:
            {
                ITERATE ( CSeq_annot::C_Data::TGraph, it,
                    (*ai)->GetData().GetGraph() ) {
                    x_DropGraph(**it, **ai, &entry);
                }
                break;
            }
        }
    }
}


bool CDataSource::x_MakeGenericSelector(SAnnotSelector& annotSelector) const
{
    // make more generic selector
    if ( annotSelector.GetFeatChoice() != CSeqFeatData::e_not_set ) {
        annotSelector.SetFeatChoice(CSeqFeatData::e_not_set);
    }
    else {
        // we already did most generic selector
        return false;
    }
    return true;
}


void CDataSource::x_MapAnnot(CTSE_Info::TRangeMap& mapByRange,
                             const CTSE_Info::TRange& range,
                             const SAnnotObject_Index& annotRef)
{
    mapByRange.insert(CTSE_Info::TRangeMap::value_type(range, annotRef));
}


void CDataSource::x_MapAnnot(CTSE_Info& tse,
                             CRef<CAnnotObject_Info> annotObj,
                             const CHandleRangeMap& hrm,
                             SAnnotSelector annotSelector)
{
    SAnnotObject_Index annotRef;
    annotRef.m_AnnotObject = annotObj;
    annotRef.m_IndexBy = annotSelector.GetFeatProduct();
    // Iterate handles
    ITERATE ( CHandleRangeMap::TLocMap, mapit, hrm.GetMap() ) {
        annotRef.m_HandleRange = mapit;
        m_TSE_ref[mapit->first].insert(CRef<CTSE_Info>(&tse));

        CTSE_Info::TAnnotSelectorMap& selMap = tse.m_AnnotMap[mapit->first];

        // repeat for more generic types of selector
        do {
            x_MapAnnot(tse.x_SetRangeMap(selMap, annotSelector), 
                       mapit->second.GetOverlappingRange(),
                       annotRef);
        } while ( x_MakeGenericSelector(annotSelector) );
    }
}


bool CDataSource::x_DropAnnot(CTSE_Info::TRangeMap& mapByRange,
                              const CTSE_Info::TRange& range,
                              const CConstRef<CAnnotObject_Info>& annotObj)
{
    for ( CTSE_Info::TRangeMap::iterator it = mapByRange.find(range);
          it != mapByRange.end() && it->first == range; ++it ) {
        if ( it->second.m_AnnotObject == annotObj ) {
            mapByRange.erase(it);
            break;
        }
    }
    return mapByRange.empty();
}


void CDataSource::x_DropAnnot(CTSE_Info& tse,
                              CRef<CAnnotObject_Info> annotObj,
                              const CHandleRangeMap& hrm,
                              SAnnotSelector annotSelector)
{
    // Iterate id handles
    ITERATE ( CHandleRangeMap::TLocMap, mapit, hrm.GetMap() ) {
        CTSE_Info::TAnnotSelectorMap& selMap = tse.m_AnnotMap[mapit->first];

        // repeat for more generic types of selector
        do {
            if ( x_DropAnnot(tse.x_SetRangeMap(selMap, annotSelector),
                             mapit->second.GetOverlappingRange(),
                             annotObj) ) {
                tse.x_DropRangeMap(selMap, annotSelector);
            }
        } while ( x_MakeGenericSelector(annotSelector) );

        if ( selMap.empty() ) {
            tse.m_AnnotMap.erase(mapit->first);
            m_TSE_ref[mapit->first].erase(CRef<CTSE_Info>(&tse));
            if (m_TSE_ref[mapit->first].empty()) {
                m_TSE_ref.erase(mapit->first);
            }
        }
    }
}


void CDataSource::x_MapAnnot(CTSE_Info& tse,
                             CAnnotObject_Info* annotPtr)
{
    CRef<CAnnotObject_Info> annotObj(annotPtr);
    m_AnnotObjects[annotObj->GetObject().GetPointer()] = annotObj;
    SAnnotSelector annotSel(annotObj->Which());
    if ( annotObj->IsFeat() ) {
        annotSel.SetFeatChoice(annotObj->GetFeat().GetData().Which());
    }
    x_MapAnnot(tse, annotObj, annotObj->GetRangeMap(), annotSel);
    if ( annotObj->IsFeat() && annotObj->GetProductMap() ) {
        annotSel.SetByProduct();
        x_MapAnnot(tse, annotObj, *annotObj->GetProductMap(), annotSel);
    }
}


void CDataSource::x_DropAnnot(const CObject* annotPtr)
{
    TAnnotObjects::iterator aoit = m_AnnotObjects.find(annotPtr);
    if ( aoit == m_AnnotObjects.end() ) {
        // not indexed
        return;
    }
    CRef<CAnnotObject_Info> annotObj = aoit->second;
    m_AnnotObjects.erase(aoit);

    const CSeq_entry& seqEntry = annotObj->GetSeq_entry();
    CTSE_Info* tse = m_Entries[CConstRef<CSeq_entry>(&seqEntry)];

    SAnnotSelector annotSel(annotObj->Which());
    if ( annotObj->IsFeat() ) {
        annotSel.SetFeatChoice(annotObj->GetFeat().GetData().Which());
    }
    x_DropAnnot(*tse, annotObj, annotObj->GetRangeMap(), annotSel);
    if ( annotObj->IsFeat() && annotObj->GetProductMap() ) {
        annotSel.SetByProduct();
        x_DropAnnot(*tse, annotObj, *annotObj->GetProductMap(), annotSel);
    }
}    


void CDataSource::x_MapFeature(const CSeq_feat& feat,
                               CSeq_annot_Info& annot,
                               CTSE_Info& tse)
{
    // Lock indexes
    CMutexGuard guard(m_DataSource_Mtx);
    x_MapAnnot(tse, new CAnnotObject_Info(annot, feat));
}


void CDataSource::x_MapAlign(const CSeq_align& align,
                             CSeq_annot_Info& annot,
                             CTSE_Info& tse)
{
    // Lock indexes
    CMutexGuard guard(m_DataSource_Mtx);
    x_MapAnnot(tse, new CAnnotObject_Info(annot, align));
}


void CDataSource::x_MapGraph(const CSeq_graph& graph,
                             CSeq_annot_Info& annot,
                             CTSE_Info& tse)
{
    // Lock indexes
    CMutexGuard guard(m_DataSource_Mtx);
    x_MapAnnot(tse, new CAnnotObject_Info(annot, graph));
}


void CDataSource::x_DropFeature(const CSeq_feat& feat,
                                const CSeq_annot& /*annot*/,
                                const CSeq_entry* /*entry*/)
{
    x_DropAnnot(&feat);
}


void CDataSource::x_DropAlign(const CSeq_align& align,
                              const CSeq_annot& /*annot*/,
                              const CSeq_entry* /*entry*/)
{
    x_DropAnnot(&align);
}


void CDataSource::x_DropGraph(const CSeq_graph& graph,
                              const CSeq_annot& /*annot*/,
                              const CSeq_entry* /*entry*/)
{
    x_DropAnnot(&graph);
}


void CDataSource::x_CleanupUnusedEntries(void)
{
    // Lock indexes
    CMutexGuard guard(m_DataSource_Mtx);    

    bool broken = true;
    while ( broken ) {
        broken = false;
        ITERATE(TEntries, it, m_Entries) {
            if ( !it->second->CounterLocked() ) {
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
    TEntries::iterator tse_it = m_Entries.find(CRef<CSeq_entry>(&tse));
    _ASSERT(tse_it != m_Entries.end());
    CTSE_Guard guard(*tse_it->second);
    tse_it->second->m_Dead = dead;
}


CSeqMatch_Info CDataSource::BestResolve(const CSeq_id& id, CScope& scope)
{
    //### Lock all TSEs found, unlock all filtered out in the end.
    TTSESet loaded_tse_set;
    if ( m_Loader ) {
        // Send request to the loader
        //### Need a better interface to request just a set of IDs
        CSeq_id_Handle idh = GetSeq_id_Mapper().GetHandle(id);
        CHandleRangeMap hrm;
        hrm.AddRange(idh,
                     CHandleRange::TRange::GetWhole(), eNa_strand_unknown);
        m_Loader->GetRecords(hrm, CDataLoader::eBioseqCore, &loaded_tse_set);
    }
    CSeqMatch_Info match;
    CSeq_id_Handle idh = GetSeq_id_Mapper().GetHandle(id, true);
    if ( !idh ) {
        return match; // The seq-id is not even mapped yet
    }
    CTSE_Lock tse = x_FindBestTSE(idh, scope.m_History);
    if ( !tse ) {
        // Try to find the best matching id (not exactly equal)
        TSeq_id_HandleSet hset;
        GetSeq_id_Mapper().GetMatchingHandles(id, hset);
        ITERATE(TSeq_id_HandleSet, hit, hset) {
            if ( tse  &&  idh.IsBetter(*hit) )
                continue;
            CTSE_Lock tmp_tse = x_FindBestTSE(*hit, scope.m_History);
            if ( tmp_tse ) {
                tse = tmp_tse;
                idh = *hit;
            }
        }
    }
    if ( tse ) {
        match = CSeqMatch_Info(idh, *tse);
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


bool CDataSource::IsSynonym(const CSeq_id& id1, CSeq_id& id2) const
{
    CSeq_id_Handle h1 = GetSeq_id_Mapper().GetHandle(id1);
    CSeq_id_Handle h2 = GetSeq_id_Mapper().GetHandle(id2);

    CMutexGuard guard(m_DataSource_Mtx);    
    TTSEMap::const_iterator tse_set = m_TSE_seq.find(h1);
    if (tse_set == m_TSE_seq.end())
        return false; // Could not find id1 in the datasource
    NON_CONST_ITERATE (TTSESet, tse_it,
                       const_cast<TTSESet&>(tse_set->second) ) {
        const CBioseq_Info& bioseq =
            *const_cast<TBioseqMap&>((*tse_it)->m_BioseqMap)[h1];
        if (bioseq.m_Synonyms.find(h2) != bioseq.m_Synonyms.end())
            return true;
    }
    return false;
}


bool CDataSource::GetTSEHandles(CScope& scope,
                                const CSeq_entry& entry,
                                set<CBioseq_Handle>& handles,
                                CSeq_inst::EMol filter)
{
    // Find TSE_Info
    CRef<CSeq_entry> ref(const_cast<CSeq_entry*>(&entry));
    CMutexGuard guard(m_DataSource_Mtx);
    TEntries::iterator found = m_Entries.find(ref);
    if (found == m_Entries.end()  ||
        found->second->m_TSE.GetPointer() != &entry)
        return false; // entry not found or not a TSE

    // One lock for the whole bioseq iterator
    scope.x_AddToHistory(*found->second);

    // Get all bioseq handles from the TSE
    // Store seq-entries in the map to prevent duplicates
    typedef map<CSeq_entry*, CBioseq_Info*> TEntryToInfo;
    TEntryToInfo bioseq_map;
    // Populate the map
    NON_CONST_ITERATE ( CTSE_Info::TBioseqMap, bit,
                        found->second->m_BioseqMap ) {
        if (filter != CSeq_inst::eMol_not_set) {
            // Filter sequences
            _ASSERT(bit->second->m_Entry->IsSeq());
            if (bit->second->m_Entry->GetSeq().GetInst().GetMol() != filter)
                continue;
        }
        bioseq_map[const_cast<CSeq_entry*>(bit->second->m_Entry.GetPointer())]
            = bit->second;
    }
    // Convert each map entry into bioseq handle
    NON_CONST_ITERATE (TEntryToInfo, eit, bioseq_map) {
        CBioseq_Handle h(*eit->second->m_Synonyms.begin());
        h.x_ResolveTo(scope, *eit->second);
        handles.insert(h);
    }
    return true;
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
        DebugDumpPairsCRefCRef(ddc, "m_SeqMaps",
            m_SeqMaps.begin(), m_SeqMaps.end(), depth);
    }
}


END_SCOPE(objects)
END_NCBI_SCOPE

/*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.91  2003/03/12 20:09:34  grichenk
* Redistributed members between CBioseq_Handle, CBioseq_Info and CTSE_Info
*
* Revision 1.90  2003/03/11 15:51:06  kuznets
* iterate -> ITERATE
*
* Revision 1.89  2003/03/11 14:15:52  grichenk
* +Data-source priority
*
* Revision 1.88  2003/03/10 16:55:17  vasilche
* Cleaned SAnnotSelector structure.
* Added shortcut when features are limited to one TSE.
*
* Revision 1.87  2003/03/05 20:56:43  vasilche
* SAnnotSelector now holds all parameters of annotation iterators.
*
* Revision 1.86  2003/03/03 20:31:08  vasilche
* Removed obsolete method PopulateTSESet().
*
* Revision 1.85  2003/02/27 14:35:31  vasilche
* Splitted PopulateTSESet() by logically independent parts.
*
* Revision 1.84  2003/02/25 20:10:40  grichenk
* Reverted to single total-range index for annotations
*
* Revision 1.83  2003/02/24 18:57:22  vasilche
* Make feature gathering in one linear pass using CSeqMap iterator.
* Do not use feture index by sub locations.
* Sort features at the end of gathering in one vector.
* Extracted some internal structures and classes in separate header.
* Delay creation of mapped features.
*
* Revision 1.82  2003/02/24 14:51:11  grichenk
* Minor improvements in annot indexing
*
* Revision 1.81  2003/02/13 14:34:34  grichenk
* Renamed CAnnotObject -> CAnnotObject_Info
* + CSeq_annot_Info and CAnnotObject_Ref
* Moved some members of CAnnotObject to CSeq_annot_Info
* and CAnnotObject_Ref.
* Added feat/align/graph to CAnnotObject_Info map
* to CDataSource.
*
* Revision 1.80  2003/02/05 17:59:17  dicuccio
* Moved formerly private headers into include/objects/objmgr/impl
*
* Revision 1.79  2003/02/04 22:01:26  grichenk
* Fixed annotations loading/indexing order
*
* Revision 1.78  2003/02/04 21:46:32  grichenk
* Added map of annotations by intervals (the old one was
* by total ranges)
*
* Revision 1.77  2003/01/29 22:03:46  grichenk
* Use single static CSeq_id_Mapper instead of per-OM model.
*
* Revision 1.76  2003/01/29 17:45:02  vasilche
* Annotaions index is split by annotation/feature type.
*
* Revision 1.75  2003/01/22 20:11:54  vasilche
* Merged functionality of CSeqMapResolved_CI to CSeqMap_CI.
* CSeqMap_CI now supports resolution and iteration over sequence range.
* Added several caches to CScope.
* Optimized CSeqVector().
* Added serveral variants of CBioseqHandle::GetSeqVector().
* Tried to optimize annotations iterator (not much success).
* Rewritten CHandleRange and CHandleRangeMap classes to avoid sorting of list.
*
* Revision 1.74  2002/12/26 16:39:24  vasilche
* Object manager class CSeqMap rewritten.
*
* Revision 1.73  2002/12/20 20:54:24  grichenk
* Added optional location/product switch to CFeat_CI
*
* Revision 1.72  2002/12/19 20:16:39  grichenk
* Fixed locations on minus strand
*
* Revision 1.71  2002/12/06 15:36:00  grichenk
* Added overlap type for annot-iterators
*
* Revision 1.70  2002/11/08 22:15:51  grichenk
* Added methods for removing/replacing annotations
*
* Revision 1.69  2002/11/08 21:03:30  ucko
* CConstRef<> now requires an explicit constructor.
*
* Revision 1.68  2002/11/04 21:29:12  grichenk
* Fixed usage of const CRef<> and CRef<> constructor
*
* Revision 1.67  2002/10/18 19:12:40  grichenk
* Removed mutex pools, converted most static mutexes to non-static.
* Protected CSeqMap::x_Resolve() with mutex. Modified code to prevent
* dead-locks.
*
* Revision 1.66  2002/10/16 20:44:16  ucko
* MIPSpro: use #pragma instantiate rather than template<>, as the latter
* ended up giving rld errors for CMutexPool_Base<*>::sm_Pool.  (Sigh.)
*
* Revision 1.65  2002/10/02 17:58:23  grichenk
* Added sequence type filter to CBioseq_CI
*
* Revision 1.64  2002/09/30 20:01:19  grichenk
* Added methods to support CBioseq_CI
*
* Revision 1.63  2002/09/16 19:59:36  grichenk
* Fixed getting reference to a gap on minus strand
*
* Revision 1.62  2002/09/10 19:55:49  grichenk
* Throw exception when unable to create resolved seq-map
*
* Revision 1.61  2002/08/09 14:58:50  ucko
* Restrict template <> to MIPSpro for now, as it also leads to link
* errors with Compaq's compiler.  (Sigh.)
*
* Revision 1.60  2002/08/08 19:51:16  ucko
* Omit EMPTY_TEMPLATE for GCC and KCC, as it evidently leads to link errors(!)
*
* Revision 1.59  2002/08/08 14:28:00  ucko
* Add EMPTY_TEMPLATE to explicit instantiations.
*
* Revision 1.58  2002/08/07 18:22:48  ucko
* Explicitly instantiate CMutexPool_Base<{CDataSource,TTSESet}>::sm_Pool
*
* Revision 1.57  2002/07/25 15:01:51  grichenk
* Replaced non-const GetXXX() with SetXXX()
*
* Revision 1.56  2002/07/08 20:51:01  grichenk
* Moved log to the end of file
* Replaced static mutex (in CScope, CDataSource) with the mutex
* pool. Redesigned CDataSource data locking.
*
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
