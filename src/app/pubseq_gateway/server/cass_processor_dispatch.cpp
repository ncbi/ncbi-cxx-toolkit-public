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
 * File Description: cassandra processor create dispatcher
 *
 */
#include <ncbi_pch.hpp>

#include <corelib/request_status.hpp>
#include <corelib/ncbidiag.hpp>

#include "cass_processor_dispatch.hpp"
#include "cass_processor_base.hpp"


USING_NCBI_SCOPE;


CPSGS_CassProcessorDispatcher::CPSGS_CassProcessorDispatcher()
{
    m_ResolveProcessor.reset(new CPSGS_ResolveProcessor());
    m_GetProcessor.reset(new CPSGS_GetProcessor());
    m_GetBlobProcessor.reset(new CPSGS_GetBlobProcessor());
    m_AnnotProcessor.reset(new CPSGS_AnnotProcessor());
    m_TSEChunkProcessor.reset(new CPSGS_TSEChunkProcessor());
    m_AccVerProcessor.reset(new CPSGS_AccessionVersionHistoryProcessor());
}


CPSGS_CassProcessorDispatcher::~CPSGS_CassProcessorDispatcher()
{}


IPSGS_Processor*
CPSGS_CassProcessorDispatcher::CreateProcessor(shared_ptr<CPSGS_Request> request,
                                               shared_ptr<CPSGS_Reply> reply,
                                               TProcessorPriority priority) const
{
    switch (request->GetRequestType()) {
    case CPSGS_Request::ePSGS_ResolveRequest:
        return m_ResolveProcessor->CreateProcessor(request, reply, priority);

    case CPSGS_Request::ePSGS_BlobBySeqIdRequest:
        return m_GetProcessor->CreateProcessor(request, reply, priority);

    case CPSGS_Request::ePSGS_BlobBySatSatKeyRequest:
        return m_GetBlobProcessor->CreateProcessor(request, reply, priority);

    case CPSGS_Request::ePSGS_AnnotationRequest:
        return m_AnnotProcessor->CreateProcessor(request, reply, priority);

    case CPSGS_Request::ePSGS_TSEChunkRequest:
        return m_TSEChunkProcessor->CreateProcessor(request, reply, priority);

    case CPSGS_Request::ePSGS_AccessionVersionHistoryRequest:
        return m_AccVerProcessor->CreateProcessor(request, reply, priority);

    default:
        break;
    }
    return nullptr;
}


void CPSGS_CassProcessorDispatcher::Process(void)
{
    // Not supposed to process any requests
}


void CPSGS_CassProcessorDispatcher::Cancel(void)
{
    // Not supposed to process any requests
}


IPSGS_Processor::EPSGS_Status
CPSGS_CassProcessorDispatcher::GetStatus(void)
{
    return ePSGS_Error; // Not supposed to process any requests
}

string CPSGS_CassProcessorDispatcher::GetName(void) const
{
    return kCassandraProcessorGroupName;
}


string CPSGS_CassProcessorDispatcher::GetGroupName(void) const
{
    return kCassandraProcessorGroupName;
}

