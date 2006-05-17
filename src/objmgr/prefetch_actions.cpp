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
* Author: Eugene Vasilchenko
*
* File Description:
*   Prefetch implementation
*
*/

#include <ncbi_pch.hpp>
#include <objmgr/prefetch_actions.hpp>
#include <objmgr/objmgr_exception.hpp>


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

class IPrefetchAction;


CPrefetchAction_GetBioseqHandle::
CPrefetchAction_GetBioseqHandle(CScope& scope,
                                const CSeq_id_Handle& id)
    : m_Scope(scope),
      m_Seq_id(id)
{
    if ( !id ) {
        NCBI_THROW(CObjMgrException, eOtherError,
                   "CPrefetchAction_GetBioseqHandle: seq-id is null");
    }
}


bool CPrefetchAction_GetBioseqHandle::Execute(CPrefetchToken token)
{
    m_Result = m_Scope->GetBioseqHandle(m_Seq_id, CScope::eGetBioseq_All);
    return m_Result;
}


CPrefetchAction_Feat_CI::
CPrefetchAction_Feat_CI(CScope& scope,
                        CConstRef<CSeq_loc> loc,
                        const SAnnotSelector& selector)
    : m_Scope(scope),
      m_Loc(loc),
      m_Selector(selector)
{
    if ( !loc ) {
        NCBI_THROW(CObjMgrException, eOtherError,
                   "CPrefetchAction_Feat_CI: loc is null");
    }
}


CPrefetchAction_Feat_CI::
CPrefetchAction_Feat_CI(const CBioseq_Handle& bioseq,
                        const CRange<TSeqPos>& range,
                        ENa_strand strand,
                        const SAnnotSelector& selector)
    : m_Bioseq(bioseq),
      m_Range(range),
      m_Strand(strand),
      m_Selector(selector)
{
    if ( !bioseq ) {
        NCBI_THROW(CObjMgrException, eOtherError,
                   "CPrefetchAction_Feat_CI: bioseq is null");
    }
}


CPrefetchAction_Feat_CI::
CPrefetchAction_Feat_CI(CScope& scope,
                        const CSeq_id_Handle& seq_id,
                        const CRange<TSeqPos>& range,
                        ENa_strand strand,
                        const SAnnotSelector& selector)
    : m_Scope(scope),
      m_Seq_id(seq_id),
      m_Range(range),
      m_Strand(strand),
      m_Selector(selector)
{
    if ( !seq_id ) {
        NCBI_THROW(CObjMgrException, eOtherError,
                   "CPrefetchAction_Feat_CI: seq-id is null");
    }
}


bool CPrefetchAction_Feat_CI::Execute(CPrefetchToken token)
{
    if ( m_Loc ) {
        m_Result = CFeat_CI(*m_Scope, *m_Loc, m_Selector);
    }
    else if ( m_Bioseq ) {
        m_Result = CFeat_CI(m_Bioseq, m_Range, m_Strand, m_Selector);
    }
    else {
        CBioseq_Handle bioseq =
            m_Scope->GetBioseqHandle(m_Seq_id, CScope::eGetBioseq_All);
        m_Result = CFeat_CI(bioseq, m_Range, m_Strand, m_Selector);
    }
    return m_Result;
}


CPrefetchToken CStdPrefetch::GetBioseqHandle(CPrefetchManager& manager,
                                             CScope& scope,
                                             const CSeq_id_Handle& id)
{
    return manager.AddAction
        (new CPrefetchAction_GetBioseqHandle(scope, id));
}


CBioseq_Handle CStdPrefetch::GetBioseqHandle(CPrefetchToken token)
{
    CPrefetchAction_GetBioseqHandle* action =
        dynamic_cast<CPrefetchAction_GetBioseqHandle*>(token.GetAction());
    if ( !action ) {
        NCBI_THROW(CObjMgrException, eOtherError,
                   "CStdPrefetch::GetBioseqHandle: wrong token");
    }
    Wait(token);
    return action->GetResult();
}



CPrefetchToken CStdPrefetch::GetFeat_CI(CPrefetchManager& manager,
                                        CScope& scope,
                                        CConstRef<CSeq_loc> loc,
                                        const SAnnotSelector& sel)
{
    return manager.AddAction
        (new CPrefetchAction_Feat_CI(scope, loc, sel));
}


CPrefetchToken CStdPrefetch::GetFeat_CI(CPrefetchManager& manager,
                                        const CBioseq_Handle& bioseq,
                                        const CRange<TSeqPos>& range,
                                        ENa_strand strand,
                                        const SAnnotSelector& sel)
{
    return manager.AddAction
        (new CPrefetchAction_Feat_CI(bioseq, range, strand, sel));
}


CPrefetchToken CStdPrefetch::GetFeat_CI(CPrefetchManager& manager,
                                        CScope& scope,
                                        const CSeq_id_Handle& seq_id,
                                        const CRange<TSeqPos>& range,
                                        ENa_strand strand,
                                        const SAnnotSelector& sel)
{
    return manager.AddAction
        (new CPrefetchAction_Feat_CI(scope, seq_id, range, strand, sel));
}


CFeat_CI CStdPrefetch::GetFeat_CI(CPrefetchToken token)
{
    CPrefetchAction_Feat_CI* action =
        dynamic_cast<CPrefetchAction_Feat_CI*>(token.GetAction());
    if ( !action ) {
        NCBI_THROW(CObjMgrException, eOtherError,
                   "CStdPrefetch::GetFeat_CI: wrong token");
    }
    Wait(token);
    return action->GetResult();
}



void CStdPrefetch::Wait(CPrefetchToken token)
{
    if ( !token.IsDone() ) {
        CWaitingListener* listener =
            dynamic_cast<CWaitingListener*>(token.GetListener());
        if ( !listener ) {
            listener = new CWaitingListener();
            token.SetListener(listener);
        }
        if ( !token.IsDone() ) {
            listener->Wait();
        }
    }
}



END_SCOPE(objects)
END_NCBI_SCOPE
