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
 * File Description: base class for processors which may generate cassandra
 *                   fetches
 *
 */

#include <ncbi_pch.hpp>

#include <corelib/request_status.hpp>
#include <corelib/ncbidiag.hpp>

#include "cass_processor_base.hpp"


CPSGS_CassProcessorBase::CPSGS_CassProcessorBase() :
    m_Completed(false),
    m_Status(CRequestStatus::e200_Ok)
{}


CPSGS_CassProcessorBase::CPSGS_CassProcessorBase(
                                            shared_ptr<CPSGS_Request> request,
                                            shared_ptr<CPSGS_Reply> reply) :
    m_Completed(false),
    m_Request(request),
    m_Reply(reply),
    m_Status(CRequestStatus::e200_Ok)
{}


CPSGS_CassProcessorBase::~CPSGS_CassProcessorBase()
{}


IPSGS_Processor::EPSGS_Status CPSGS_CassProcessorBase::GetStatus(void) const
{
    if (m_Completed) {
        // Finished before initiating any cassandra fetches
        return x_GetProcessorStatus();
    }

    if (AreAllFinishedRead()) {
        // Finished because all cassandra requests completed reading
        return x_GetProcessorStatus();
    }

    return IPSGS_Processor::ePSGS_InProgress;
}


bool CPSGS_CassProcessorBase::AreAllFinishedRead(void) const
{
    size_t      started_count = 0;
    for (const auto &  details: m_FetchDetails) {
        if (details) {
            ++started_count;
            if (!details->ReadFinished())
                return false;
        }
    }
    return started_count != 0;
}


IPSGS_Processor::EPSGS_Status
CPSGS_CassProcessorBase::x_GetProcessorStatus(void) const
{
    switch (m_Status) {
        case CRequestStatus::e200_Ok:
            return IPSGS_Processor::ePSGS_Found;
        case CRequestStatus::e404_NotFound:
            return IPSGS_Processor::ePSGS_NotFound;
        default:
            break;
    }
    return IPSGS_Processor::ePSGS_Error;
}


void
CPSGS_CassProcessorBase::UpdateOverallStatus(CRequestStatus::ECode  status)
{
    m_Status = max(status, m_Status);
}

