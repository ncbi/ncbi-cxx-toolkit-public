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

/////////////////////////////////////////////////////////////////////////////
// CScopeSource

CScope& CScopeSource::GetScope(void)
{
    if ( m_Scope.IsNull() ) {
        m_Scope.Set(new CScope(m_BaseScope->GetObjectManager()));
        (*m_Scope).AddScope(*m_BaseScope);
    }
    return m_Scope;
}


/////////////////////////////////////////////////////////////////////////////
// CPrefetchBioseq

CPrefetchBioseq::CPrefetchBioseq(const CScopeSource& scope)
    : CScopeSource(scope)
{
}


CPrefetchBioseq::CPrefetchBioseq(const CBioseq_Handle& bioseq)
    : CScopeSource(bioseq.GetScope()),
      m_Result(bioseq)
{
    if ( !bioseq ) {
        NCBI_THROW(CObjMgrException, eOtherError,
                   "CPrefetchBioseq: bioseq handle is null");
    }
}


CPrefetchBioseq::CPrefetchBioseq(const CScopeSource& scope,
                                 const CSeq_id_Handle& id)
    : CScopeSource(scope),
      m_Seq_id(id)
{
    if ( !id ) {
        NCBI_THROW(CObjMgrException, eOtherError,
                   "CPrefetchBioseq: seq-id is null");
    }
}


bool CPrefetchBioseq::Execute(CRef<CPrefetchRequest> token)
{
    if ( !GetResult() && GetSeq_id() ) {
        m_Result = GetScope().GetBioseqHandle(GetSeq_id());
    }
    return GetResult();
}


/////////////////////////////////////////////////////////////////////////////
// CPrefetchFeat_CI

CPrefetchFeat_CI::CPrefetchFeat_CI(const CScopeSource& scope,
                                   CConstRef<CSeq_loc> loc,
                                   const SAnnotSelector& selector)
    : CPrefetchBioseq(scope),
      m_Loc(loc),
      m_Selector(selector)
{
    if ( !loc ) {
        NCBI_THROW(CObjMgrException, eOtherError,
                   "CPrefetchFeat_CI: loc is null");
    }
}


CPrefetchFeat_CI::CPrefetchFeat_CI(const CBioseq_Handle& bioseq,
                                   const CRange<TSeqPos>& range,
                                   ENa_strand strand,
                                   const SAnnotSelector& selector)
    : CPrefetchBioseq(bioseq),
      m_Range(range),
      m_Strand(strand),
      m_Selector(selector)
{
}


CPrefetchFeat_CI::CPrefetchFeat_CI(const CScopeSource& scope,
                                   const CSeq_id_Handle& seq_id,
                                   const CRange<TSeqPos>& range,
                                   ENa_strand strand,
                                   const SAnnotSelector& selector)
    : CPrefetchBioseq(scope, seq_id),
      m_Range(range),
      m_Strand(strand),
      m_Selector(selector)
{
}


bool CPrefetchFeat_CI::Execute(CRef<CPrefetchRequest> token)
{
    if ( m_Loc ) {
        m_Result = CFeat_CI(GetScope(), *m_Loc, m_Selector);
    }
    else if ( CPrefetchBioseq::Execute(token) ) {
        m_Result = CFeat_CI(GetBioseqHandle(), m_Range, m_Strand, m_Selector);
    }
    return m_Result;
}


/////////////////////////////////////////////////////////////////////////////
// CPrefetchComplete<CBioseq_Handle>

CPrefetchComplete<CBioseq_Handle>::CPrefetchComplete(const THandle& handle)
    : CPrefetchBioseq(handle)
{
}


CPrefetchComplete<CBioseq_Handle>::CPrefetchComplete(const CScopeSource& scope,
                                                     const CSeq_id_Handle& id)
    : CPrefetchBioseq(scope, id)
{
}


bool CPrefetchComplete<CBioseq_Handle>::Execute(CRef<CPrefetchRequest> token)
{
    if ( CPrefetchBioseq::Execute(token) ) {
        m_Result = GetHandle().GetCompleteObject();
    }
    return GetResult().NotNull();
}


/////////////////////////////////////////////////////////////////////////////
// CStdPrefetch

namespace {
    class CWaitingListener
        : public CObject, public IPrefetchListener
    {
    public:
        CWaitingListener(void)
            : m_Sema(0, kMax_Int)
            {
            }

        virtual void PrefetchNotify(CRef<CPrefetchRequest> token, EEvent /*event*/)
            {
                if ( token->IsDone() ) {
                    m_Sema.Post();
                }
            }

        void Wait(void)
            {
                m_Sema.Wait();
                m_Sema.Post();
            }
    
    private:
        CSemaphore m_Sema;
    };
}


void CStdPrefetch::Wait(CRef<CPrefetchRequest> token)
{
    if ( !token->IsDone() ) {
        CWaitingListener* listener =
            dynamic_cast<CWaitingListener*>(token->GetListener());
        if ( !listener ) {
            listener = new CWaitingListener();
            token->SetListener(listener);
        }
        if ( !token->IsDone() ) {
            listener->Wait();
        }
    }
}


/////////////////////////////////////////////////////////////////////////////
// CStdPrefetch::GetBioseqHandle

CRef<CPrefetchRequest> CStdPrefetch::GetBioseqHandle(CPrefetchManager& manager,
                                                     const CScopeSource& scope,
                                                     const CSeq_id_Handle& id)
{
    return manager.AddAction(new CPrefetchBioseq(scope, id));
}


CBioseq_Handle CStdPrefetch::GetBioseqHandle(CRef<CPrefetchRequest> token)
{
    CPrefetchBioseq* action =
        dynamic_cast<CPrefetchBioseq*>(token->GetAction());
    if ( !action ) {
        NCBI_THROW(CObjMgrException, eOtherError,
                   "CStdPrefetch::GetBioseqHandle: wrong token");
    }
    Wait(token);
    return action->GetResult();
}


/////////////////////////////////////////////////////////////////////////////
// CStdPrefetch::GetFeat_CI

CRef<CPrefetchRequest> CStdPrefetch::GetFeat_CI(CPrefetchManager& manager,
                                                const CScopeSource& scope,
                                                CConstRef<CSeq_loc> loc,
                                                const SAnnotSelector& sel)
{
    return manager.AddAction(new CPrefetchFeat_CI(scope, loc, sel));
}


CRef<CPrefetchRequest> CStdPrefetch::GetFeat_CI(CPrefetchManager& manager,
                                                const CBioseq_Handle& bioseq,
                                                const CRange<TSeqPos>& range,
                                                ENa_strand strand,
                                                const SAnnotSelector& sel)
{
    return manager.AddAction(new CPrefetchFeat_CI(bioseq,
                                                  range, strand, sel));
}


CRef<CPrefetchRequest> CStdPrefetch::GetFeat_CI(CPrefetchManager& manager,
                                                const CScopeSource& scope,
                                                const CSeq_id_Handle& seq_id,
                                                const CRange<TSeqPos>& range,
                                                ENa_strand strand,
                                                const SAnnotSelector& sel)
{
    return manager.AddAction(new CPrefetchFeat_CI(scope, seq_id,
                                                  range, strand, sel));
}


CFeat_CI CStdPrefetch::GetFeat_CI(CRef<CPrefetchRequest> token)
{
    CPrefetchFeat_CI* action =
        dynamic_cast<CPrefetchFeat_CI*>(token->GetAction());
    if ( !action ) {
        NCBI_THROW(CObjMgrException, eOtherError,
                   "CStdPrefetch::GetFeat_CI: wrong token");
    }
    Wait(token);
    return action->GetResult();
}


/////////////////////////////////////////////////////////////////////////////
// ISeq_idSource

ISeq_idSource::~ISeq_idSource(void)
{
}


/////////////////////////////////////////////////////////////////////////////
// IPrefetchActionSource

CPrefetchBioseqActionSource::CPrefetchBioseqActionSource(const CScopeSource& scope,
                                                         ISeq_idSource* ids)
    : m_Scope(scope),
      m_Ids(ids)
{
}


CPrefetchBioseqActionSource::CPrefetchBioseqActionSource(const CScopeSource& scope,
                                                         const TIds& ids)
    : m_Scope(scope),
      m_Ids(new CStdSeq_idSource<TIds>(ids))
{
}


CIRef<IPrefetchAction> CPrefetchBioseqActionSource::GetNextAction(void)
{
    CIRef<IPrefetchAction> ret;
    CSeq_id_Handle id = m_Ids->GetNextSeq_id();
    if ( id ) {
        ret.Reset(new CPrefetchBioseq(m_Scope, id));
    }
    return ret;
}


CPrefetchFeat_CIActionSource::CPrefetchFeat_CIActionSource(const CScopeSource& scope,
                                                           const TIds& ids,
                                                           const SAnnotSelector& sel)
    : m_Scope(scope),
      m_Ids(new CStdSeq_idSource<TIds>(ids)),
      m_Selector(sel)
{
}


CPrefetchFeat_CIActionSource::CPrefetchFeat_CIActionSource(const CScopeSource& scope,
                                                           ISeq_idSource* ids,
                                                           const SAnnotSelector& sel)
    : m_Scope(scope),
      m_Ids(ids),
      m_Selector(sel)
{
}


CIRef<IPrefetchAction> CPrefetchFeat_CIActionSource::GetNextAction(void)
{
    CIRef<IPrefetchAction> ret;
    CSeq_id_Handle id = m_Ids->GetNextSeq_id();
    if ( id ) {
        ret.Reset(new CPrefetchFeat_CI(m_Scope, id,
                                       CRange<TSeqPos>::GetWhole(),
                                       eNa_strand_unknown,
                                       m_Selector));
    }
    return ret;
}


END_SCOPE(objects)
END_NCBI_SCOPE
