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


#include <objects/general/Int_fuzz.hpp>
#include <objects/seq/Bioseq.hpp>
#include <objects/seqset/Seq_entry.hpp>

#include <algorithm>

#include <objects/objmgr1/object_manager.hpp>
#include <objects/objmgr1/scope.hpp>
#include "data_source.hpp"


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
    m_pObjMgr->RevokeScope(*this);
    m_pObjMgr->ReleaseDataSources(m_setDataSrc);
}

void CScope::AddDefaults(void)
{
    CMutexGuard guard(sm_Scope_Mutex);
    m_pObjMgr->AcquireDefaultDataSources(m_setDataSrc);
}


void CScope::AddDataLoader (const string& loader_name)
{
    CMutexGuard guard(sm_Scope_Mutex);
    m_pObjMgr->AddDataLoader(m_setDataSrc, loader_name);
}


void CScope::AddTopLevelSeqEntry(CSeq_entry& top_entry)
{
    CMutexGuard guard(sm_Scope_Mutex);
    m_pObjMgr->AddTopLevelSeqEntry(m_setDataSrc, top_entry);
}


void CScope::DropTopLevelSeqEntry(CSeq_entry& top_entry)
{
    CMutexGuard guard(sm_Scope_Mutex);
    m_pObjMgr->RemoveTopLevelSeqEntry( m_setDataSrc, top_entry);
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


CBioseq_Handle CScope::GetBioseqHandle(const CSeq_id& id)
{
    CMutexGuard guard(sm_Scope_Mutex);
    CBioseq_Handle found;
    iterate (set<CDataSource*>, it, m_setDataSrc) {
        CBioseq_Handle handle = (*it)->GetBioseqHandle(*this, id);
        if ( handle ) {
            if (m_FindMode == eFirst) {
                return handle;
            }
            if ( found ) {
                // Report an error and return the first handle found
                switch (m_FindMode) {
                case eDup_Fatal:
                    ERR_POST(Fatal <<
                        "GetBioseqHandle() -- ambiguous Seq-id");
                    break;
                case eDup_Warning:
                    ERR_POST(Warning <<
                        "GetBioseqHandle() -- ambiguous Seq-id");
                    break;
                default: // e_Find_DupThrow
                    throw runtime_error
                        ("GetBioseqHandle() -- ambiguous Seq-id");
                }
                break;
            }
            // Remember the first handle found
            found = handle;
        }
    }
    return found;
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


void CScope::x_PopulateTSESet(CHandleRangeMap& loc,
                              CAnnotTypes_CI::TTSESet& tse_set)
{
    CMutexGuard guard(sm_Scope_Mutex);
    iterate (set<CDataSource*>, it, m_setDataSrc) {
        (*it)->PopulateTSESet(loc, tse_set);
    }
}


bool CScope::x_AttachEntry(const CSeq_entry& parent, CSeq_entry& entry)
{
    CMutexGuard guard(sm_Scope_Mutex);
    iterate (set<CDataSource*>, it, m_setDataSrc) {
        if ( (*it)->AttachEntry(parent, entry) ) {
            return true;
        }
    }
    return false;
}


bool CScope::x_AttachMap(const CSeq_entry& bioseq, CSeqMap& seqmap)
{
    CMutexGuard guard(sm_Scope_Mutex);
    iterate (set<CDataSource*>, it, m_setDataSrc) {
        if ( (*it)->AttachMap(bioseq, seqmap) ) {
            return true;
        }
    }
    return false;
}


bool CScope::x_AttachSeqData(const CSeq_entry& bioseq, CSeq_data& seq,
                             TSeqPosition start, TSeqLength length)
{
    CMutexGuard guard(sm_Scope_Mutex);
    iterate (set<CDataSource*>, it, m_setDataSrc) {
        if ( (*it)->AttachSeqData(bioseq, seq, start, length) ) {
            return true;
        }
    }
    return false;
}


bool CScope::x_GetSequence(const CBioseq_Handle& handle,
                           TSeqPosition point,
                           SSeqData* seq_piece)
{
    CMutexGuard guard(sm_Scope_Mutex);
    iterate (set<CDataSource*>, it, m_setDataSrc) {
        if ( (*it)->GetSequence(handle, point, seq_piece, *this) ) {
            return true;
        }
    }
    return false;
}


void CScope::SetFindMode(EFindMode mode)
{
    m_FindMode = mode;
}


CSeq_id_Mapper& CScope::x_GetIdMapper(void) {
    return *m_pObjMgr->m_IdMapper;
}


END_SCOPE(objects)
END_NCBI_SCOPE
