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
#include "data_source.hpp"
#include <objects/objmgr/tse_info.hpp>
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

void CScope::AddDefaults(void)
{
    m_pObjMgr->AcquireDefaultDataSources(m_setDataSrc);
}


void CScope::AddDataLoader (const string& loader_name)
{
    m_pObjMgr->AddDataLoader(m_setDataSrc, loader_name);
}


void CScope::AddTopLevelSeqEntry(CSeq_entry& top_entry)
{
    m_pObjMgr->AddTopLevelSeqEntry(m_setDataSrc, top_entry);
}


bool CScope::AttachAnnot(const CSeq_entry& entry, CSeq_annot& annot)
{
    CMutexGuard guard(m_Scope_Mtx);
    iterate (set<CDataSource*>, it, m_setDataSrc) {
        if ( (*it)->AttachAnnot(entry, annot) ) {
            return true;
        }
    }
    return false;
}


bool CScope::RemoveAnnot(const CSeq_entry& entry, const CSeq_annot& annot)
{
    CMutexGuard guard(m_Scope_Mtx);
    iterate (set<CDataSource*>, it, m_setDataSrc) {
        if ( (*it)->GetDataLoader() )
            continue; // can not modify loaded data
        if ( (*it)->RemoveAnnot(entry, annot) ) {
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
    iterate (set<CDataSource*>, it, m_setDataSrc) {
        if ( (*it)->ReplaceAnnot(entry, old_annot, new_annot) ) {
            return true;
        }
    }
    return false;
}


bool CScope::AttachEntry(const CSeq_entry& parent, CSeq_entry& entry)
{
    CMutexGuard guard(m_Scope_Mtx);
    iterate (set<CDataSource*>, it, m_setDataSrc) {
        if ( (*it)->AttachEntry(parent, entry) ) {
            return true;
        }
    }
    return false;
}


bool CScope::AttachMap(const CSeq_entry& bioseq, CSeqMap& seqmap)
{
    CMutexGuard guard(m_Scope_Mtx);
    iterate (set<CDataSource*>, it, m_setDataSrc) {
        if ( (*it)->AttachMap(bioseq, seqmap) ) {
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
    iterate (set<CDataSource*>, it, m_setDataSrc) {
        if ( (*it)->AttachSeqData(bioseq, *dseq, index, start, length) ) {
            return true;
        }
    }
    return false;
}
#endif

CBioseq_Handle CScope::GetBioseqHandle(const CSeq_id& id)
{
    CMutexGuard guard(m_Scope_Mtx);
    CSeqMatch_Info match = x_BestResolve(id);
    if (!match)
        return CBioseq_Handle();
    x_AddToHistory(*match.m_TSE);
    return match.m_DataSource->GetBioseqHandle(*this, match);
}


CBioseq_Handle CScope::GetBioseqHandle(const CSeq_loc& loc)
{
    for (CSeq_loc_CI citer (loc); citer; ++citer) {
        const CSeq_id & id = citer.GetSeq_id ();
        CBioseq_Handle bsh = GetBioseqHandle (id);
        return bsh;
    }

    NCBI_THROW(CException, eUnknown, "GetBioseqHandle by location failed");
}


CBioseq_Handle CScope::GetBioseqHandle(const CBioseq& seq)
{
    iterate (CBioseq::TId, id, seq.GetId()) {
      CBioseq_Handle bsh = GetBioseqHandle (**id);
      return bsh;
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
    m_pObjMgr->m_IdMapper->GetMatchingHandlesStr(searchBy, setSource);
    // filter those which "belong" to my data sources
    iterate(set<CDataSource*>, itSrc, m_setDataSrc) {
        (*itSrc)->FilterSeqid(setResult, setSource);
    }
    // create result
    iterate(TSeq_id_HandleSet, itSet, setResult) {
        setId.insert(CRef<const CSeq_id>
                     (&(m_pObjMgr->m_IdMapper->GetSeq_id(*itSet))));
    }
}


CSeqMatch_Info CScope::x_BestResolve(const CSeq_id& id)
{
    // Protected by m_Scope_Mtx in upper-level functions
    set<CSeqMatch_Info> bm_set;
    iterate (set<CDataSource*>, it, m_setDataSrc) {
        CSeqMatch_Info info = (*it)->BestResolve(id, *this);
        if ( info ) {
            bm_set.insert(info);
        }
    }
    CSeqMatch_Info best;
    bool best_is_in_history = false;
    bool best_is_live = false;
    CSeqMatch_Info extra_live;
    iterate (set<CSeqMatch_Info>, bm_it, bm_set) {
        TRequestHistory::const_iterator hist = m_History.find(bm_it->m_TSE);
        if (hist != m_History.end()) {
            if ( !best_is_in_history ) {
                // The first TSE from the history -- save it
                best = *bm_it;
                best_is_in_history = true;
                best_is_live = !bm_it->m_TSE->m_Dead;
                continue;
            }
            else {
                x_ThrowConflict(eConflict_History, best, *bm_it);
            }
        }
        if ( best_is_in_history )
            continue; // Don't check TSEs not in the history
        if ( !bm_it->m_TSE->m_Dead ) {
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
        if ( !best  ||  bm_it->m_Handle.IsBetter(best.m_Handle) ) {
            //###
            // This is not good - some ids may have no version and will be
            // treated as older versions.
            best = *bm_it;
        }
    }
    if ( extra_live ) {
        x_ThrowConflict(eConflict_Live, best, extra_live);
    }
    if ( best ) {
        best.m_TSE->LockCounter(); // Lock the best TSE
    }
    iterate (set<CSeqMatch_Info>, bm_it, bm_set) {
        // Unlock all TSEs
        bm_it->m_TSE->UnlockCounter();
    }
    return best;
}


void CScope::x_PopulateTSESet(CHandleRangeMap& loc,
                              CSeq_annot::C_Data::E_Choice sel,
                              CAnnotTypes_CI::TTSESet& tse_set)
{
    //CMutexGuard guard(m_Scope_Mtx);
    iterate (set<CDataSource*>, it, m_setDataSrc) {
        (*it)->PopulateTSESet(loc, tse_set, sel, *this);
    }
    //### Filter the set depending on the requests history?
    iterate (CAnnotTypes_CI::TTSESet, tse_it, tse_set) {
        x_AddToHistory(**tse_it);
        (*tse_it)->UnlockCounter();
    }
}


bool CScope::x_GetSequence(const CBioseq_Handle& handle,
                           TSeqPosition point,
                           SSeqData* seq_piece)
{
    CMutexGuard guard(m_Scope_Mtx);
    CSeqMatch_Info match = x_BestResolve(*handle.GetSeqId());
    if (!match)
        return false;
    x_AddToHistory(*match.m_TSE);
    match.m_TSE->UnlockCounter();
    return match.m_DataSource->GetSequence(handle, point, seq_piece, *this);
}


void CScope::SetFindMode(EFindMode mode)
{
    m_FindMode = mode;
}


CSeq_id_Mapper& CScope::x_GetIdMapper(void) const
{
    return *m_pObjMgr->m_IdMapper;
}


CSeq_id_Handle CScope::GetIdHandle(const CSeq_id& id) const
{
    return x_GetIdMapper().GetHandle(id);
}


const CScope::TRequestHistory& CScope::x_GetHistory(void)
{
    return m_History;
}


void CScope::x_AddToHistory(const CTSE_Info& tse, bool lock)
{
    if ( m_History.insert(CConstRef<CTSE_Info>(&tse)).second  &&  lock ) {
        tse.LockCounter();
    }
}


void CScope::x_ThrowConflict(EConflict conflict_type,
                             const CSeqMatch_Info& info1,
                             const CSeqMatch_Info& info2) const
{
    switch ( conflict_type ) {
    case eConflict_History:
        {
            ERR_POST(Fatal << "CScope -- multiple history matches: " <<
                info1.m_DataSource->GetName() << "::" <<
                x_GetIdMapper().GetSeq_id(info1.m_Handle).DumpAsFasta() <<
                " vs " <<
                info2.m_DataSource->GetName() << "::" <<
                x_GetIdMapper().GetSeq_id(info2.m_Handle).DumpAsFasta());
        }
    case eConflict_Live:
        {
            ERR_POST(Fatal << "CScope -- multiple live TSE matches: " <<
                info1.m_DataSource->GetName() << "::" <<
                x_GetIdMapper().GetSeq_id(info1.m_Handle).DumpAsFasta() <<
                " vs " <<
                info2.m_DataSource->GetName() << "::" <<
                x_GetIdMapper().GetSeq_id(info2.m_Handle).DumpAsFasta());
        }
    }
}


void CScope::ResetHistory(void)
{
    CMutexGuard guard(m_Scope_Mtx);
    iterate(TRequestHistory, it, m_History) {
        (*it)->UnlockCounter();
    }
    m_History.clear();
}


void CScope::x_PopulateBioseq_HandleSet(const CSeq_entry& tse,
                                        set<CBioseq_Handle>& handles,
                                        CSeq_inst::EMol filter)
{
    iterate(set<CDataSource*>, itSrc, m_setDataSrc) {
        if ((*itSrc)->GetTSEHandles(*this, tse, handles, filter))
            break;
    }
}


void
CScope::x_DetachFromOM(void)
{
    CMutexGuard guard(m_Scope_Mtx);
    // Drop and release all TSEs
    if(!m_History.empty())
      {
        iterate(TRequestHistory, it, m_History) {
            (*it)->UnlockCounter();
        }
        m_History.clear();
      }
    if(m_pObjMgr)
      {
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
        DebugDumpRangePtr(ddc,"m_setDataSrc",
            m_setDataSrc.begin(), m_setDataSrc.end(), depth);

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
