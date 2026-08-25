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


size_t GetBacklogSize(void)
{
    size_t              ret = 0;
    for (size_t  k = 0; k < CPSGS_Request::ePSGS_UnknownRequest; ++k) {
        ret += g_BacklogPerRequest.m_BacklogPerRequest[k].load(memory_order_relaxed);
    }
    return ret;
}

SBacklogPerRequest GetBacklogPerRequestSnapshot(void)
{
    return SBacklogPerRequest(g_BacklogPerRequest);
}


void RegisterBackloggedRequest(CPSGS_Request::EPSGS_Type  request_type)
{
    g_BacklogPerRequest.m_BacklogPerRequest[request_type].fetch_add(1, memory_order_relaxed);
}


void UnregisterBackloggedRequest(CPSGS_Request::EPSGS_Type  request_type)
{
    g_BacklogPerRequest.m_BacklogPerRequest[request_type].fetch_sub(1, memory_order_relaxed);
}

