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
*           Andrei Gourianov
*           Aleksey Grichenko
*           Michael Kimelman
*           Denis Vakatov
*
* File Description:
*           Scope is top-level object available to a client.
*           Its purpose is to define a scope of visibility and reference
*           resolution and provide access to the bio sequence data
*
*/

#include <objects/objmgr/scope.hpp>
#include <objects/objmgr/impl/data_source.hpp>
#include <objects/objmgr/impl/tse_info.hpp>
#include <objects/objmgr/object_manager.hpp>
#include <objects/seq/Delta_seq.hpp>
#include <objects/seq/Seq_literal.hpp>
#include <objects/seqloc/Seq_loc.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


////////////////////////////////////////////////////////////////////
//
//  CScope::
//


CScope::CScope(CObjectManager& objmgr)
    : m_pObjMgr(&objmgr), m_FindMode(eFirst)
{
    m_pObjMgr->RegisterScope(*this);
}


CScope::~CScope(void)
{
    x_DetachFromOM();
}

void CScope::AddDefaults(SDataSourceRec::TPriority priority)
{
    m_pObjMgr->AcquireDefaultDataSources(m_setDataSrc, priority);
}


void CScope::AddDataLoader (const string& loader_name,
                            SDataSourceRec::TPriority priority)
{
    m_pObjMgr->AddDataLoader(m_setDataSrc, loader_name, priority);
}


void CScope::AddTopLevelSeqEntry(CSeq_entry& top_entry,
                                 SDataSourceRec::TPriority priority)
{
    m_pObjMgr->AddTopLevelSeqEntry(m_setDataSrc, top_entry, priority);
}


bool CScope::AttachAnnot(const CSeq_entry& entry, CSeq_annot& annot)
{
    CMutexGuard guard(m_Scope_Mtx);
    iterate (TDataSourceSet, it, m_setDataSrc) {
        if ( it->m_DataSource->AttachAnnot(entry, annot) ) {
            return true;
        }
    }
    return false;
}


bool CScope::RemoveAnnot(const CSeq_entry& entry, const CSeq_annot& annot)
{
    CMutexGuard guard(m_Scope_Mtx);
    iterate (TDataSourceSet, it, m_setDataSrc) {
        if ( it->m_DataSource->GetDataLoader() )
            continue; // can not modify loaded data
        if ( it->m_DataSource->RemoveAnnot(entry, annot) ) {
            return true;
        }
    }
    return false;
}


bool CScope::ReplaceAnnot(const CSeq_entry& entry,
                          const CSeq_annot& old_annot,
                          CSeq_annot& new_annot)
{
    CMutexGuard guard(m_Scope_Mtx);
    iterate (TDataSourceSet, it, m_setDataSrc) {
        if ( it->m_DataSource->ReplaceAnnot(entry, old_annot, new_annot) ) {
            return true;
        }
    }
    return false;
}


bool CScope::AttachEntry(const CSeq_entry& parent, CSeq_entry& entry)
{
    CMutexGuard guard(m_Scope_Mtx);
    iterate (TDataSourceSet, it, m_setDataSrc) {
        if ( it->m_DataSource->AttachEntry(parent, entry) ) {
            return true;
        }
    }
    return false;
}


bool CScope::AttachMap(const CSeq_entry& bioseq, CSeqMap& seqmap)
{
    CMutexGuard guard(m_Scope_Mtx);
    iterate (TDataSourceSet, it, m_setDataSrc) {
        if ( it->m_DataSource->AttachMap(bioseq, seqmap) ) {
            return true;
        }
    }
    return false;
}

#if 0
bool CScope::AttachSeqData(const CSeq_entry& bioseq, CSeq_data& seq,
                           size_t index,
                           TSeqPosition start, TSeqLength length)
{
    CMutexGuard guard(m_Scope_Mtx);
    CRef<CDelta_seq> dseq(new CDelta_seq);
    dseq->SetLiteral().SetSeq_data(seq);
    dseq->SetLiteral().SetLength(length);
    iterate (TDataSourceSet, it, m_setDataSrc) {
        if ( it->m_DataSource->AttachSeqData(bioseq, *dseq, index, start, length) ) {
            return true;
        }
    }
    return false;
}
#endif

CBioseq_Handle CScope::GetBioseqHandle(const CSeq_id& id)
{
    return GetBioseqHandle(GetIdHandle(id));
}


CBioseq_Handle CScope::GetBioseqHandleFromTSE(const CSeq_id& id,
                                              const CBioseq_Handle& bh)
{
    return GetBioseqHandleFromTSE(GetIdHandle(id), bh);
}


CBioseq_Handle CScope::GetBioseqHandle(const CSeq_id_Handle& id)
{
    CMutexGuard guard(m_Scope_Mtx);
    CBioseq_Handle& bh = m_Cache[id];
    if ( !bh ) {
        CSeqMatch_Info match = x_BestResolve(CSeq_id_Mapper::GetSeq_id(id));
        if (match) {
            x_AddToHistory(*match);
            bh = match.GetDataSource()->GetBioseqHandle(*this, match);
        }
    }
    return bh;
}


CBioseq_Handle CScope::GetBioseqHandleFromTSE(const CSeq_id_Handle& id,
                                              const CBioseq_Handle& bh)
{
    CMutexGuard guard(m_Scope_Mtx);
    TCache::const_iterator it = m_Cache.find(id);
    if ( it != m_Cache.end() && it->second ) {
        if ( &it->second.GetTopLevelSeqEntry() != &bh.GetTopLevelSeqEntry() ) {
            return CBioseq_Handle();
        }
        return it->second;
    }
    CBioseq_Handle ret;
    TSeq_id_HandleSet hset;
    x_GetIdMapper().GetMatchingHandles(id.GetSeqId(), hset);
    iterate ( TSeq_id_HandleSet, hit, hset ) {
        CSeqMatch_Info match(id, *static_cast<CTSE_Info*>(bh.m_TSE),
                             bh.x_GetDataSource());
        ret = match.GetDataSource()->GetBioseqHandle(*this, match);
        if ( ret )
            break;
    }
    return ret;
}


CBioseq_Handle CScope::GetBioseqHandle(const CSeq_loc& loc)
{
    for (CSeq_loc_CI citer (loc); citer; ++citer) {
        return GetBioseqHandle(citer.GetSeq_id());
    }

    NCBI_THROW(CException, eUnknown, "GetBioseqHandle by location failed");
}


CBioseq_Handle CScope::GetBioseqHandle(const CBioseq& seq)
{
    iterate (CBioseq::TId, id, seq.GetId()) {
        return GetBioseqHandle (**id);
    }

    NCBI_THROW(CException, eUnknown, "GetBioseqHandle from bioseq failed");
}


void CScope::FindSeqid(set< CRef<const CSeq_id> >& setId,
                       const string& searchBy) const
{
    CMutexGuard guard(m_Scope_Mtx);
    setId.clear();

    TSeq_id_HandleSet setSource, setResult;
    // find all
    x_GetIdMapper().GetMatchingHandlesStr(searchBy, setSource);
    // filter those which "belong" to my data sources
    iterate(TDataSourceSet, itSrc, m_setDataSrc) {
        itSrc->m_DataSource->FilterSeqid(setResult, setSource);
    }
    // create result
    iterate(TSeq_id_HandleSet, itSet, setResult) {
        setId.insert(CRef<const CSeq_id>
                     (&(x_GetIdMapper().GetSeq_id(*itSet))));
    }
}


CSeqMatch_Info CScope::x_BestResolve(const CSeq_id& id)
{
    //### Use priority flag, do not scan all DSs - find the first one.
    // Protected by m_Scope_Mtx in upper-level functions
    set<CSeqMatch_Info> bm_set;
    SDataSourceRec::TPriority pr = SDataSourceRec::TPriority(-1);
    iterate (TDataSourceSet, it, m_setDataSrc) {
        // Do not search in lower-priority sources
        if (pr < it->m_Priority)
            continue;
        CSeqMatch_Info info = it->m_DataSource->BestResolve(id, *this);
        if ( info ) {
            bm_set.insert(info);
            pr = it->m_Priority;
        }
    }
    CSeqMatch_Info best;
    bool best_is_in_history = false;
    bool best_is_live = false;
    CSeqMatch_Info extra_live;
    iterate (set<CSeqMatch_Info>, bm_it, bm_set) {
        TRequestHistory::const_iterator hist =
            m_History.find(CTSE_Lock(const_cast<CTSE_Info&>(**bm_it)));
        if (hist != m_History.end()) {
            if ( !best_is_in_history ) {
                // The first TSE from the history -- save it
                best = *bm_it;
                best_is_in_history = true;
                best_is_live = !(*bm_it)->m_Dead;
                continue;
            }
            else {
                x_ThrowConflict(eConflict_History, best, *bm_it);
            }
        }
        if ( best_is_in_history )
            continue; // Don't check TSEs not in the history
        if ( !(*bm_it)->m_Dead ) {
            if ( !best_is_live ) {
                // The first live TSE -- save it
                best = *bm_it;
                best_is_live = true;
                continue;
            }
            else {
                // Can not throw exception right now - there may be a better
                // match from the history.
                extra_live = *bm_it;
            }
        }
        if ( best_is_live )
            continue; // Don't check for dead TSEs not in the history
        if ( !best  ||  bm_it->GetIdHandle().IsBetter(best.GetIdHandle()) ) {
            //###
            // This is not good - some ids may have no version and will be
            // treated as older versions.
            best = *bm_it;
        }
    }
    if ( extra_live ) {
        x_ThrowConflict(eConflict_Live, best, extra_live);
    }
    return best;
}


void CScope::UpdateAnnotIndex(const CHandleRangeMap& loc,
                              CSeq_annot::C_Data::E_Choice sel)
{
    //CMutexGuard guard(m_Scope_Mtx);
    iterate (TDataSourceSet, it, m_setDataSrc) {
        it->m_DataSource->UpdateAnnotIndex(loc, sel);
    }
}


void CScope::GetTSESetWithAnnots(const CSeq_id_Handle& idh,
                                 CAnnotTypes_CI::TTSESet& tse_set)
{
    //CMutexGuard guard(m_Scope_Mtx);
    iterate (TDataSourceSet, it, m_setDataSrc) {
        it->m_DataSource->GetTSESetWithAnnots(idh, tse_set, *this);
    }
    //### Filter the set depending on the requests history?
    iterate (CAnnotTypes_CI::TTSESet, tse_it, tse_set) {
        x_AddToHistory(**tse_it);
    }
}


CTSE_Lock CScope::GetTSEInfo(const CSeq_entry* tse)
{
    CTSE_Lock ret;
    CMutexGuard guard(m_Scope_Mtx);
    iterate (TDataSourceSet, it, m_setDataSrc) {
        ret = it->m_DataSource->GetTSEInfo(tse);
        if ( ret )
            break;
    }
    return ret;
}


void CScope::SetFindMode(EFindMode mode)
{
    m_FindMode = mode;
}


CSeq_id_Mapper& CScope::x_GetIdMapper(void) const
{
    return CSeq_id_Mapper::GetSeq_id_Mapper();
}


CSeq_id_Handle CScope::GetIdHandle(const CSeq_id& id) const
{
    return x_GetIdMapper().GetHandle(id);
}


const CScope::TRequestHistory& CScope::x_GetHistory(void)
{
    return m_History;
}


void CScope::x_AddToHistory(CTSE_Info& tse)
{
    m_History.insert(CTSE_Lock(&tse));
}


void CScope::x_ThrowConflict(EConflict conflict_type,
                             const CSeqMatch_Info& info1,
                             const CSeqMatch_Info& info2) const
{
    switch ( conflict_type ) {
    case eConflict_History:
        {
            ERR_POST(Fatal << "CScope -- multiple history matches: " <<
                info1.GetDataSource()->GetName() << "::" <<
                x_GetIdMapper().GetSeq_id(info1.GetIdHandle()).DumpAsFasta() <<
                " vs " <<
                info2.GetDataSource()->GetName() << "::" <<
                x_GetIdMapper().GetSeq_id(info2.GetIdHandle()).DumpAsFasta());
        }
    case eConflict_Live:
        {
            ERR_POST(Fatal << "CScope -- multiple live TSE matches: " <<
                info1.GetDataSource()->GetName() << "::" <<
                x_GetIdMapper().GetSeq_id(info1.GetIdHandle()).DumpAsFasta() <<
                " vs " <<
                info2.GetDataSource()->GetName() << "::" <<
                x_GetIdMapper().GetSeq_id(info2.GetIdHandle()).DumpAsFasta());
        }
    }
}


void CScope::ResetHistory(void)
{
    CMutexGuard guard(m_Scope_Mtx);
    m_History.clear();
    m_Cache.clear();
    m_SynCache.clear();
}


void CScope::x_PopulateBioseq_HandleSet(const CSeq_entry& tse,
                                        set<CBioseq_Handle>& handles,
                                        CSeq_inst::EMol filter)
{
    iterate(TDataSourceSet, itSrc, m_setDataSrc) {
        if (itSrc->m_DataSource->GetTSEHandles(*this, tse, handles, filter))
            break;
    }
}


const CSynonymsSet* CScope::x_GetSynonyms(const CSeq_id_Handle& id)
{
    //### Check priority
    TSynCache::const_iterator cached = m_SynCache.find(id);
    if (cached != m_SynCache.end()) {
        return cached->second;
    }
    // Temporary implementation. May change when datasources will
    // be able of getting synonyms without the whole sequences.
    // Then we will need to filter results like in x_BestResolve.
    CBioseq_Handle h = GetBioseqHandle(id);
    if ( !h ) {
        return 0;
    }
    CBioseq_Handle::TBioseqCore core = h.GetBioseqCore();
    // It's OK to use CRef, at least one copy should be kept
    // alive by the id cache (for the ID requested).
    CRef<CSynonymsSet> synset(new CSynonymsSet);
    iterate(CBioseq::TId, it, core->GetId()) {
        // Check current ID for conflicts, add to the set.
        CSeq_id_Handle idh = x_GetIdMapper().GetHandle(**it);
        TCache::const_iterator seq = m_Cache.find(idh);
        if (seq != m_Cache.end()  &&  seq->second != h) {
            // already cached for a different bioseq - ignore the id
            continue;
        }
        synset->AddSynonym(idh);
        m_SynCache[idh] = synset;
    }
    // Map the original ID which may be not in the resulting set,
    // e.g. id="A", set={"A.1", gi1}
    m_SynCache[id] = synset;
    return synset;
}


void
CScope::x_DetachFromOM(void)
{
    CMutexGuard guard(m_Scope_Mtx);
    // Drop and release all TSEs
    ResetHistory();
    if(m_pObjMgr) {
        m_pObjMgr->ReleaseDataSources(m_setDataSrc);
        m_pObjMgr->RevokeScope(*this);
        m_pObjMgr=0;
    }
}


void CScope::DebugDump(CDebugDumpContext ddc, unsigned int depth) const
{
    ddc.SetFrame("CScope");
    CObject::DebugDump( ddc, depth);

    ddc.Log("m_pObjMgr", m_pObjMgr,0);
    if (depth == 0) {
        DebugDumpValue(ddc,"m_setDataSrc.size()", m_setDataSrc.size());
        DebugDumpValue(ddc,"m_History.size()", m_History.size());
    } else {
        DebugDumpValue(ddc,"m_setDataSrc.type", "set<CDataSource*>");
        /*
        DebugDumpRangePtr(ddc,"m_setDataSrc",
            m_setDataSrc.begin(), m_setDataSrc.end(), depth);

        */
        DebugDumpValue(ddc,"m_History.type", "set<CConstRef<CTSE_Info>>");
        DebugDumpRangeCRef(ddc,"m_History",
            m_History.begin(), m_History.end(), depth);
    }
    DebugDumpValue(ddc,"m_FindMode", m_FindMode);
}


END_SCOPE(objects)
END_NCBI_SCOPE

/*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.49  2003/03/11 14:15:52  grichenk
* +Data-source priority
*
* Revision 1.48  2003/03/10 16:55:17  vasilche
* Cleaned SAnnotSelector structure.
* Added shortcut when features are limited to one TSE.
*
* Revision 1.47  2003/03/05 20:55:29  vasilche
* Added cache cleaning in CScope::ResetHistory().
*
* Revision 1.46  2003/03/03 20:32:47  vasilche
* Removed obsolete method GetSynonyms().
*
* Revision 1.45  2003/02/28 21:54:18  grichenk
* +CSynonymsSet::empty(), removed _ASSERT() in CScope::GetSynonyms()
*
* Revision 1.44  2003/02/28 20:02:37  grichenk
* Added synonyms cache and x_GetSynonyms()
*
* Revision 1.43  2003/02/27 14:35:31  vasilche
* Splitted PopulateTSESet() by logically independent parts.
*
* Revision 1.42  2003/02/24 18:57:22  vasilche
* Make feature gathering in one linear pass using CSeqMap iterator.
* Do not use feture index by sub locations.
* Sort features at the end of gathering in one vector.
* Extracted some internal structures and classes in separate header.
* Delay creation of mapped features.
*
* Revision 1.41  2003/02/05 17:59:17  dicuccio
* Moved formerly private headers into include/objects/objmgr/impl
*
* Revision 1.40  2003/01/29 22:03:46  grichenk
* Use single static CSeq_id_Mapper instead of per-OM model.
*
* Revision 1.39  2003/01/22 20:11:54  vasilche
* Merged functionality of CSeqMapResolved_CI to CSeqMap_CI.
* CSeqMap_CI now supports resolution and iteration over sequence range.
* Added several caches to CScope.
* Optimized CSeqVector().
* Added serveral variants of CBioseqHandle::GetSeqVector().
* Tried to optimize annotations iterator (not much success).
* Rewritten CHandleRange and CHandleRangeMap classes to avoid sorting of list.
*
* Revision 1.38  2002/12/26 20:55:18  dicuccio
* Moved seq_id_mapper.hpp, tse_info.hpp, and bioseq_info.hpp -> include/ tree
*
* Revision 1.37  2002/12/26 16:39:24  vasilche
* Object manager class CSeqMap rewritten.
*
* Revision 1.36  2002/11/08 22:15:51  grichenk
* Added methods for removing/replacing annotations
*
* Revision 1.35  2002/11/08 19:43:35  grichenk
* CConstRef<> constructor made explicit
*
* Revision 1.34  2002/11/04 21:29:12  grichenk
* Fixed usage of const CRef<> and CRef<> constructor
*
* Revision 1.33  2002/11/01 05:34:32  kans
* added GetBioseqHandle taking CBioseq parameter
*
* Revision 1.32  2002/10/31 22:25:42  kans
* added GetBioseqHandle taking CSeq_loc parameter
*
* Revision 1.31  2002/10/18 19:12:40  grichenk
* Removed mutex pools, converted most static mutexes to non-static.
* Protected CSeqMap::x_Resolve() with mutex. Modified code to prevent
* dead-locks.
*
* Revision 1.30  2002/10/16 20:44:29  ucko
* *** empty log message ***
*
* Revision 1.29  2002/10/02 17:58:23  grichenk
* Added sequence type filter to CBioseq_CI
*
* Revision 1.28  2002/09/30 20:01:19  grichenk
* Added methods to support CBioseq_CI
*
* Revision 1.27  2002/08/09 14:59:00  ucko
* Restrict template <> to MIPSpro for now, as it also leads to link
* errors with Compaq's compiler.  (Sigh.)
*
* Revision 1.26  2002/08/08 19:51:24  ucko
* Omit EMPTY_TEMPLATE for GCC and KCC, as it evidently leads to link errors(!)
*
* Revision 1.25  2002/08/08 14:28:00  ucko
* Add EMPTY_TEMPLATE to explicit instantiations.
*
* Revision 1.24  2002/08/07 18:21:57  ucko
* Explicitly instantiate CMutexPool_Base<CScope>::sm_Pool
*
* Revision 1.23  2002/07/08 20:51:02  grichenk
* Moved log to the end of file
* Replaced static mutex (in CScope, CDataSource) with the mutex
* pool. Redesigned CDataSource data locking.
*
* Revision 1.22  2002/06/04 17:18:33  kimelman
* memory cleanup :  new/delete/Cref rearrangements
*
* Revision 1.21  2002/05/28 18:00:43  gouriano
* DebugDump added
*
* Revision 1.20  2002/05/14 20:06:26  grichenk
* Improved CTSE_Info locking by CDataSource and CDataLoader
*
* Revision 1.19  2002/05/06 03:28:47  vakatov
* OM/OM1 renaming
*
* Revision 1.18  2002/04/22 20:04:39  grichenk
* Fixed TSE dropping, removed commented code
*
* Revision 1.17  2002/04/17 21:09:40  grichenk
* Fixed annotations loading
*
* Revision 1.16  2002/03/28 14:02:31  grichenk
* Added scope history checks to CDataSource::x_FindBestTSE()
*
* Revision 1.15  2002/03/27 18:45:44  gouriano
* three functions made public
*
* Revision 1.14  2002/03/20 21:20:39  grichenk
* +CScope::ResetHistory()
*
* Revision 1.13  2002/02/28 20:53:32  grichenk
* Implemented attaching segmented sequence data. Fixed minor bugs.
*
* Revision 1.12  2002/02/25 21:05:29  grichenk
* Removed seq-data references caching. Increased MT-safety. Fixed typos.
*
* Revision 1.11  2002/02/21 19:27:06  grichenk
* Rearranged includes. Added scope history. Added searching for the
* best seq-id match in data sources and scopes. Updated tests.
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
* Revision 1.7  2002/01/29 17:45:34  grichenk
* Removed debug output
*
* Revision 1.6  2002/01/28 19:44:49  gouriano
* changed the interface of BioseqHandle: two functions moved from Scope
*
* Revision 1.5  2002/01/23 21:59:31  grichenk
* Redesigned seq-id handles and mapper
*
* Revision 1.4  2002/01/18 17:06:29  gouriano
* renamed CScope::GetSequence to CScope::GetSeqVector
*
* Revision 1.3  2002/01/18 15:54:14  gouriano
* changed DropTopLevelSeqEntry()
*
* Revision 1.2  2002/01/16 16:25:57  gouriano
* restructured objmgr
*
* Revision 1.1  2002/01/11 19:06:22  gouriano
* restructured objmgr
*
*
* ===========================================================================
*/
