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
 * Authors:  Anatoliy Kuznetsov
 *
 * File Description: ITransaction interface
 *
 */


#include <ncbi_pch.hpp>
#include <util/itransaction.hpp>

BEGIN_NCBI_SCOPE

ITransaction::~ITransaction()
{}

ITransactional::~ITransactional()
{}

ITransactionalRegistry::~ITransactionalRegistry()
{}

void CThreadLocalTransactional::SetTransaction(ITransaction* trans)
{
    CThread::TID self_tid = CThread::GetSelf();

    CFastMutexGuard lock(m_ThreadMapLock);
    m_ThreadMap[self_tid] = trans;
}

ITransaction* CThreadLocalTransactional::GetTransaction()
{
    CThread::TID self_tid = CThread::GetSelf();

    CFastMutexGuard lock(m_ThreadMapLock);
    TThreadCtxMap::const_iterator it = m_ThreadMap.find(self_tid);
    if (it == m_ThreadMap.end()) {
        return 0;
    }
    return it->second;
}

void CThreadLocalTransactional::RemoveTransaction(ITransaction* trans)
{
    CThread::TID self_tid = CThread::GetSelf();

    CFastMutexGuard lock(m_ThreadMapLock);
    TThreadCtxMap::iterator it = m_ThreadMap.find(self_tid);
    if (it == m_ThreadMap.end()) {
        return;
    }
    if (it->second == trans) {
        it->second = 0;
    }
}

END_NCBI_SCOPE
