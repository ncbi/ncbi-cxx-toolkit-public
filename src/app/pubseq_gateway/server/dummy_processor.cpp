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
 * File Description: dummy processor
 *
 */
#include <ncbi_pch.hpp>

#include <corelib/request_status.hpp>
#include <corelib/ncbidiag.hpp>

#include "dummy_processor.hpp"


USING_NCBI_SCOPE;


CPSGS_DummyProcessor::CPSGS_DummyProcessor()
{}


CPSGS_DummyProcessor::CPSGS_DummyProcessor(shared_ptr<CPSGS_Request> request,
                                           shared_ptr<CPSGS_Reply> reply,
                                           TProcessorPriority priority)
{
    m_Status = ePSGS_InProgress;

    IPSGS_Processor::m_Request = request;
    IPSGS_Processor::m_Reply = reply;
    IPSGS_Processor::m_Priority = priority;
}


CPSGS_DummyProcessor::~CPSGS_DummyProcessor()
{}


bool
CPSGS_DummyProcessor::CanProcess(shared_ptr<CPSGS_Request> request,
                                 shared_ptr<CPSGS_Reply> reply) const
{
    for (const auto &  dis_processor :
            request->GetRequest<SPSGS_RequestBase>().m_DisabledProcessors) {
        if (NStr::CompareNocase(dis_processor, "dummy") == 0) {
            return false;
        }
    }

    switch (request->GetRequestType()) {
    case CPSGS_Request::ePSGS_ResolveRequest: return true;
    case CPSGS_Request::ePSGS_BlobBySeqIdRequest: return true;
    case CPSGS_Request::ePSGS_BlobBySatSatKeyRequest: return true;
    case CPSGS_Request::ePSGS_AnnotationRequest: return true;
    case CPSGS_Request::ePSGS_TSEChunkRequest: return true;
    case CPSGS_Request::ePSGS_AccessionVersionHistoryRequest: return true;
    default: break;
    }
    return false;
}


IPSGS_Processor*
CPSGS_DummyProcessor::CreateProcessor(shared_ptr<CPSGS_Request> request,
                                      shared_ptr<CPSGS_Reply> reply,
                                      TProcessorPriority priority) const
{
    switch (request->GetRequestType()) {
    case CPSGS_Request::ePSGS_ResolveRequest:
        return new CPSGS_DummyProcessor(request, reply, priority);
    case CPSGS_Request::ePSGS_BlobBySeqIdRequest:
        return new CPSGS_DummyProcessor(request, reply, priority);
    case CPSGS_Request::ePSGS_BlobBySatSatKeyRequest:
        return new CPSGS_DummyProcessor(request, reply, priority);
    case CPSGS_Request::ePSGS_AnnotationRequest:
        return new CPSGS_DummyProcessor(request, reply, priority);
    case CPSGS_Request::ePSGS_TSEChunkRequest:
        return new CPSGS_DummyProcessor(request, reply, priority);
    case CPSGS_Request::ePSGS_AccessionVersionHistoryRequest:
        return new CPSGS_DummyProcessor(request, reply, priority);
    default: break;
    }
    return nullptr;
}


void CPSGS_DummyProcessor::Process(void)
{
}


void CPSGS_DummyProcessor::Cancel(void)
{
}


IPSGS_Processor::EPSGS_Status
CPSGS_DummyProcessor::GetStatus(void)
{
    return m_Status;
}

string CPSGS_DummyProcessor::GetName(void) const
{
    return "DUMMY";
}


string CPSGS_DummyProcessor::GetGroupName(void) const
{
    return "DUMMY";
}

