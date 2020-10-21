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

#include "psgs_dispatcher.hpp"

USING_NCBI_SCOPE;


void CPSGS_Dispatcher::AddProcessor(unique_ptr<IPSGS_Processor> processor)
{
    m_Processors.push_back(move(processor));
}


list<unique_ptr<IPSGS_Processor>>
CPSGS_Dispatcher::DispatchRequest(shared_ptr<CPSGS_Request> request,
                                  shared_ptr<CPSGS_Reply> reply) const
{
    list<unique_ptr<IPSGS_Processor>>       ret;
    TProcessorPriority                      priority = m_Processors.size();

    for (auto const &  proc : m_Processors) {
        if (request->NeedTrace()) {
            reply->SendTrace("Try to create processor: " + proc->GetName(),
                             request->GetStartTimestamp());
        }
        IPSGS_Processor *   p = proc->CreateProcessor(request, reply, priority);
        if (p) {
            ret.push_back(unique_ptr<IPSGS_Processor>(p));
            if (request->NeedTrace()) {
                reply->SendTrace("Processor " + proc->GetName() +
                                 " has been created sucessfully",
                                 request->GetStartTimestamp());
            }
        } else {
            if (request->NeedTrace()) {
                reply->SendTrace("Processor " + proc->GetName() +
                                 " has not been created",
                                 request->GetStartTimestamp());
            }
        }
        --priority;
    }
    return ret;
}

