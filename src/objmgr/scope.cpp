
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
* Revision 1.1  2002/01/11 19:06:22  gouriano
* restructured objmgr
*
*
* ===========================================================================
*/


#include <objects/objmgr1/object_manager.hpp>
#include <objects/general/Int_fuzz.hpp>
#include <objects/seq/Bioseq.hpp>
#include <objects/seqset/Seq_entry.hpp>

#include <algorithm>


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


/*
void CScope::x_AddDataSource(CDataSource& source)
{
    CMutexGuard guard(sm_Scope_Mutex);
    // There may be no other CRef-s to the source -- we don't want it to be
    // deleted after "find".
    CRef<CDataSource> src(&source);
    TDataSources::iterator found = find
        (m_setDataSrc.begin(), m_setDataSrc.end(), src);
    if (found != m_setDataSrc.end())
        return;
    m_setDataSrc.push_back(src);
}
*/


CBioseqHandle CScope::GetBioseqHandle(const CSeq_id& id)
{
    CMutexGuard guard(sm_Scope_Mutex);
    CBioseqHandle found;
    iterate (set<CDataSource*>, it, m_setDataSrc) {
        CBioseqHandle handle = (*it)->GetBioseqHandle(id);
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


const CBioseq& CScope::GetBioseq(const CBioseqHandle& handle)
{
    return handle.x_GetDataSource().GetBioseq(handle);
}


const CSeq_entry& CScope::GetTSE(const CBioseqHandle& handle)
{
    return handle.x_GetDataSource().GetTSE(handle);
}


CScope::TBioseqCore CScope::GetBioseqCore(const CBioseqHandle& handle)
{
    return handle.x_GetDataSource().GetBioseqCore(handle);
}


const CSeqMap& CScope::GetSeqMap(const CBioseqHandle& handle)
{
    return handle.x_GetDataSource().GetSeqMap(handle);
}


CDesc_CI CScope::BeginDescr(const CBioseqHandle& handle)
{
    return handle.x_GetDataSource().BeginDescr(handle);
}


CSeqVector CScope::GetSequence(const CBioseqHandle& handle,
                                bool plus_strand)
{
    return CSeqVector(handle, plus_strand, *this);
}


CFeat_CI CScope::BeginFeat(const CSeq_loc& loc,
                           CSeqFeatData::E_Choice feat_choice)
{
    CMutexGuard guard(sm_Scope_Mutex);
    return CFeat_CI(m_setDataSrc.begin(),
                    &m_setDataSrc,
                    loc, feat_choice);
}


CGraph_CI CScope::BeginGraph(const CSeq_loc& loc)
{
    CMutexGuard guard(sm_Scope_Mutex);
    return CGraph_CI(m_setDataSrc.begin(),
                     &m_setDataSrc,
                     loc);
}


CAlign_CI CScope::BeginAlign(const CSeq_loc& loc)
{
    CMutexGuard guard(sm_Scope_Mutex);
    return CAlign_CI(m_setDataSrc.begin(),
                     &m_setDataSrc,
                     loc);
}


bool CScope::x_AttachEntry(const CSeq_entry& parent, CSeq_entry& entry)
{
    CMutexGuard guard(sm_Scope_Mutex);
    iterate (TDataSources, it, m_setDataSrc) {
        if ( (*it)->AttachEntry(parent, entry) ) {
            return true;
        }
    }
    return false;
}


bool CScope::x_AttachMap(const CSeq_entry& bioseq, CSeqMap& seqmap)
{
    CMutexGuard guard(sm_Scope_Mutex);
    iterate (TDataSources, it, m_setDataSrc) {
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
    iterate (TDataSources, it, m_setDataSrc) {
        if ( (*it)->AttachSeqData(bioseq, seq, start, length) ) {
            return true;
        }
    }
    return false;
}


bool CScope::AttachAnnot(const CSeq_entry& entry, CSeq_annot& annot)
{
    CMutexGuard guard(sm_Scope_Mutex);
    iterate (TDataSources, it, m_setDataSrc) {
        if ( (*it)->AttachAnnot(entry, annot) ) {
            return true;
        }
    }
    return false;
}


bool CScope::x_GetSequence(const CBioseqHandle& handle,
                           TSeqPosition point,
                           CDataSource::SSeqData* seq_piece)
{
    CMutexGuard guard(sm_Scope_Mutex);
    iterate (TDataSources, it, m_setDataSrc) {
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


void CScope::DropTSE(CSeq_entry& tse)
{
    CMutexGuard guard(sm_Scope_Mutex);
    TDataSources::iterator found_ds = m_setDataSrc.end();
    non_const_iterate (TDataSources, it, m_setDataSrc) {
        if ( (*it)->DropTSE(tse) ) {
            found_ds = it;
            break;
        }
    }
    if ( found_ds != m_setDataSrc.end()  &&  (*found_ds)->IsEmpty() ) {
//        m_setDataSrc.erase(found_ds);
        m_pObjMgr->RemoveTopLevelSeqEntry( m_setDataSrc, tse);
    }
}


END_SCOPE(objects)
END_NCBI_SCOPE
