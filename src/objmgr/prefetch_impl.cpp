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
* Author: Aleksey Grichenko, Eugene Vasilchenko
*
* File Description:
*   Prefetch implementation
*
*/

#include <objmgr/impl/prefetch_impl.hpp>
#include <corelib/ncbimtx.hpp>
#include <objmgr/bioseq_handle.hpp>
#include <objmgr/scope.hpp>
#include <objmgr/impl/tse_info.hpp>
#include <objmgr/impl/scope_impl.hpp>
#include <objmgr/impl/data_source.hpp>
#include <objmgr/seqmatch_info.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

CPrefetchToken_Impl::CPrefetchToken_Impl(CScope& scope, const CSeq_id& id)
    : m_TokenCount(0)
{
    m_Ids.push_back(CSeq_id_Handle::GetHandle(id));
    x_InitPrefetch(scope);
}


CPrefetchToken_Impl::CPrefetchToken_Impl(CScope& scope, const CSeq_id_Handle& id)
    : m_TokenCount(0)
{
    m_Ids.push_back(id);
    x_InitPrefetch(scope);
}


CPrefetchToken_Impl::CPrefetchToken_Impl(CScope& scope, const TIds& ids)
    : m_TokenCount(0)
{
    m_Ids = ids;
    x_InitPrefetch(scope);
}


CPrefetchToken_Impl::~CPrefetchToken_Impl(void)
{
    return;
}


void CPrefetchToken_Impl::x_InitPrefetch(CScope& scope)
{
    m_TSEs.resize(m_Ids.size());
    m_CurrentId = 0;
    CRef<CDataSource> source(scope.GetImpl().GetFirstLoaderSource());
    if (!source) {
        return;
    }
    source->Prefetch(*this);
}


void CPrefetchToken_Impl::AddResolvedId(size_t id_idx, TTSE_Lock tse)
{
    CFastMutexGuard guard(m_Lock);
    if (m_Ids.size() == 0  ||  id_idx < m_CurrentId) {
        // Token has been cleaned or id already passed, do not lock the TSE
        return;
    }
    m_TSEs[id_idx] = tse;
}


CPrefetchToken_Impl::operator bool(void) const
{
    CFastMutexGuard guard(m_Lock);
    return m_CurrentId < m_Ids.size();
}


CBioseq_Handle CPrefetchToken_Impl::NextBioseqHandle(CScope& scope)
{
    TTSE_Lock tse;
    CSeq_id_Handle id;
    {{
        CFastMutexGuard guard(m_Lock);
        // Can not call bool(*this) - creates deadlock
        _ASSERT(m_CurrentId < m_Ids.size());
        id = m_Ids[m_CurrentId];
        // Keep temporary TSE lock
        tse = m_TSEs[m_CurrentId];
        m_TSEs[m_CurrentId].Reset();
        ++m_CurrentId;
    }}
    return scope.GetBioseqHandle(id);
}


void CPrefetchToken_Impl::AddTokenReference(void)
{
    ++m_TokenCount;
}


void CPrefetchToken_Impl::RemoveTokenReference(void)
{
    if ( !(--m_TokenCount) ) {
        // No more tokens, reset the queue
        CFastMutexGuard guard(m_Lock);
        m_Ids.clear();
        m_TSEs.clear();
        m_CurrentId = 0;
    }
}


CPrefetchThread::CPrefetchThread(CDataSource& data_source)
    : m_DataSource(data_source),
      m_Semaphore(0, 100),
      m_Stop(false)
{
    return;
}


CPrefetchThread::~CPrefetchThread(void)
{
    return;
}


void CPrefetchThread::AddRequest(CPrefetchToken_Impl& token)
{
    {{
        CFastMutexGuard guard(m_Lock);
        m_Queue.push_back(Ref(&token));
    }}
    m_Semaphore.Post();
}


void CPrefetchThread::Terminate(void)
{
    {{
        CFastMutexGuard guard(m_Lock);
        m_Queue.clear();
        m_Stop = true;
    }}
    m_Semaphore.Post();
}


void* CPrefetchThread::Main(void)
{
    do {
        CRef<CPrefetchToken_Impl> token;
        m_Semaphore.Wait();
        {{
            CFastMutexGuard guard(m_Lock);
            if (m_Stop) {
                return 0;
            }
            if ( m_Queue.empty() ) {
                continue;
            }
            token = *m_Queue.begin();
            m_Queue.erase(m_Queue.begin());
        }}
        for (size_t i = 0; ; ++i) {
            {{
                CFastMutexGuard guard(m_Lock);
                if (m_Stop) {
                    return 0;
                }
            }}
            CSeq_id_Handle id;
            {{
                // m_Ids may be cleaned up by the token, check size
                // on every iteration.
                CFastMutexGuard guard(token->m_Lock);
                if (i >= token->m_Ids.size()) {
                    break;
                }
                id = token->m_Ids[i];
            }}
            CSeqMatch_Info info = m_DataSource.BestResolve(id);
            if ( info ) {
                TTSE_Lock tse(&info.GetTSE_Info());
                if (tse) {
                    token->AddResolvedId(i, tse);
                }
            }
        }
    } while (true);
    return 0;
}


END_SCOPE(objects)
END_NCBI_SCOPE

/*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.1  2004/04/16 13:30:34  grichenk
* Initial revision
*
*
* ===========================================================================
*/
