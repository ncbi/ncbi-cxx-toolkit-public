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
* ---------------------------------------------------------------------------
* $Log$
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

#include <objects/objmgr1/scope.hpp>
#include "data_source.hpp"
#include "tse_info.hpp"
#include <objects/objmgr1/object_manager.hpp>
#include <objects/seq/Delta_seq.hpp>
#include <objects/seq/Seq_literal.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


////////////////////////////////////////////////////////////////////
//
//  CScope::
//


CMutex CScope::sm_Scope_Mutex;


CScope::CScope(CObjectManager& objmgr)
    : m_pObjMgr(&objmgr), m_FindMode(eFirst)
{
    m_pObjMgr->RegisterScope(*this);
}


CScope::~CScope(void)
{
    // Drop and release all TSEs
    iterate(TRequestHistory, it, m_History) {
        (*it)->Unlock();
    }
    m_pObjMgr->RevokeScope(*this);
    m_pObjMgr->ReleaseDataSources(m_setDataSrc);
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
    CMutexGuard guard(sm_Scope_Mutex);
    iterate (set<CDataSource*>, it, m_setDataSrc) {
        if ( (*it)->AttachAnnot(entry, annot) ) {
            return true;
        }
    }
    return false;
}


bool CScope::AttachEntry(const CSeq_entry& parent, CSeq_entry& entry)
{
    CMutexGuard guard(sm_Scope_Mutex);
    iterate (set<CDataSource*>, it, m_setDataSrc) {
        if ( (*it)->AttachEntry(parent, entry) ) {
            return true;
        }
    }
    return false;
}


bool CScope::AttachMap(const CSeq_entry& bioseq, CSeqMap& seqmap)
{
    CMutexGuard guard(sm_Scope_Mutex);
    iterate (set<CDataSource*>, it, m_setDataSrc) {
        if ( (*it)->AttachMap(bioseq, seqmap) ) {
            return true;
        }
    }
    return false;
}


bool CScope::AttachSeqData(const CSeq_entry& bioseq, CSeq_data& seq,
                             TSeqPosition start, TSeqLength length)
{
    CMutexGuard guard(sm_Scope_Mutex);
    CRef<CDelta_seq> dseq = new CDelta_seq;
    dseq->SetLiteral().SetSeq_data(seq);
    dseq->SetLiteral().SetLength(length);
    iterate (set<CDataSource*>, it, m_setDataSrc) {
        if ( (*it)->AttachSeqData(bioseq, *dseq, start, length) ) {
            return true;
        }
    }
    return false;
}


CBioseq_Handle CScope::GetBioseqHandle(const CSeq_id& id)
{
    CMutexGuard guard(sm_Scope_Mutex);
    CSeqMatch_Info match = x_BestResolve(id);
    if (!match)
        return CBioseq_Handle();
    x_AddToHistory(*match.m_TSE);
    return match.m_DataSource->GetBioseqHandle(*this, id);
}


void CScope::FindSeqid(set< CRef<const CSeq_id> >& setId,
                       const string& searchBy) const
{
    CMutexGuard guard(sm_Scope_Mutex);
    setId.clear();

    TSeq_id_HandleSet setSource, setResult;
    // find all
    m_pObjMgr->m_IdMapper->GetMatchingHandlesStr( searchBy, setSource);
    // filter those which "belong" to my data sources
    iterate(set<CDataSource*>, itSrc, m_setDataSrc) {
        (*itSrc)->FilterSeqid( setResult, setSource);
    }
    // create result
    iterate(TSeq_id_HandleSet, itSet, setResult) {
        setId.insert( &(m_pObjMgr->m_IdMapper->GetSeq_id(*itSet)));
    }
}


CSeqMatch_Info CScope::x_BestResolve(const CSeq_id& id)
{
    set<CSeqMatch_Info> bm_set;
    iterate (set<CDataSource*>, it, m_setDataSrc) {
        CSeqMatch_Info info = (*it)->BestResolve(id, m_History);
        if ( info )
            bm_set.insert(info);
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
    return best;
}


void CScope::x_PopulateTSESet(CHandleRangeMap& loc,
                              CSeq_annot::C_Data::E_Choice sel,
                              CAnnotTypes_CI::TTSESet& tse_set)
{
    CMutexGuard guard(sm_Scope_Mutex);
    iterate (set<CDataSource*>, it, m_setDataSrc) {
        (*it)->PopulateTSESet(loc, tse_set, sel, m_History);
    }
    //### Filter the set depending on the requests history?
    iterate (CAnnotTypes_CI::TTSESet, tse_it, tse_set) {
        x_AddToHistory(**tse_it);
    }
}


bool CScope::x_GetSequence(const CBioseq_Handle& handle,
                           TSeqPosition point,
                           SSeqData* seq_piece)
{
    CMutexGuard guard(sm_Scope_Mutex);
    CSeqMatch_Info match = x_BestResolve(*handle.GetSeqId());
    if (!match)
        return false;
    x_AddToHistory(*match.m_TSE);
    return match.m_DataSource->GetSequence(handle, point, seq_piece, *this);
}


void CScope::SetFindMode(EFindMode mode)
{
    m_FindMode = mode;
}


CSeq_id_Mapper& CScope::x_GetIdMapper(void) const {
    return *m_pObjMgr->m_IdMapper;
}


const CScope::TRequestHistory& CScope::x_GetHistory(void)
{
    return m_History;
}


void CScope::x_AddToHistory(const CTSE_Info& tse)
{
    if ( m_History.insert(&tse).second ) {
        tse.Lock();
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
    CMutexGuard guard(sm_Scope_Mutex);
    iterate(TRequestHistory, it, m_History) {
        (*it)->Unlock();
    }
    m_History.clear();
}



END_SCOPE(objects)
END_NCBI_SCOPE
