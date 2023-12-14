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
 * Authors: Sergey Satskiy
 *
 * File Description:
 *
 */

#include <ncbi_pch.hpp>

#include <atomic>
using namespace std;

#include "pubseq_gateway_utils.hpp"
#include "backlog_per_request.hpp"


SBacklogPerRequest      g_BacklogPerRequest;
atomic<bool>            g_BacklogPerRequestLock;


size_t GetBacklogSize(void)
{
    size_t              ret = 0;
    CSpinlockGuard      guard(&g_BacklogPerRequestLock);
    for (size_t  k = 0; k < sizeof(g_BacklogPerRequest.m_BacklogPerRequest) / sizeof(size_t); ++k) {
        ret += g_BacklogPerRequest.m_BacklogPerRequest[k];
    }
    return ret;
}

SBacklogPerRequest GetBacklogPerRequestSnapshot(void)
{
    SBacklogPerRequest  ret;
    CSpinlockGuard      guard(&g_BacklogPerRequestLock);
    ret = g_BacklogPerRequest;
    return ret;
}


void RegisterBackloggedRequest(CPSGS_Request::EPSGS_Type  request_type)
{
    CSpinlockGuard      guard(&g_BacklogPerRequestLock);
    ++g_BacklogPerRequest.m_BacklogPerRequest[static_cast<size_t>(request_type)];
}


void UnregisterBackloggedRequest(CPSGS_Request::EPSGS_Type  request_type)
{
    CSpinlockGuard      guard(&g_BacklogPerRequestLock);
    --g_BacklogPerRequest.m_BacklogPerRequest[static_cast<size_t>(request_type)];
}

