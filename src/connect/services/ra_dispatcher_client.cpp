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
 *   Government have not placed any restriction on its use or reproduction.
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
 * Authors:  Maxim Didenko
 *
 * File Description:
 *
 */

#include <ncbi_pch.hpp>

#include <corelib/blob_storage.hpp>
#include <connect/services/netschedule_api.hpp>
#include <connect/services/ra_dispatcher_client.hpp>


BEGIN_NCBI_SCOPE

CRADispatcherClient::CRADispatcherClient(IBlobStorageFactory& factory, CNcbiIostream& stream)
    : m_Factory(factory), m_Stream(stream), m_WasGetResultCalled(false)
{
}
CRADispatcherClient::~CRADispatcherClient()
{
}

void CRADispatcherClient::StartJob(CRemoteAppRequestSB& request)
{
    if (m_Request && !m_WasGetResultCalled)
        NCBI_THROW(CRADispatcherClientException, eJobIsBeingProcessed,
                   "Another job is being processed an its result has not been read yet.");
    m_Request.Reset(&request);
    m_Stream << eNewJobCmd << " ";
    if (!m_Stream.good())
        NCBI_THROW(CRADispatcherClientException, eIOError,
                   "Can not read/write to/from the communication stream.");
    m_Request->Send(m_Stream);

    EResId res = x_ReadResponseId();
    if (res != eJobStatusRes)
        NCBI_THROW(CRADispatcherClientException, eConfError,
                   "Wrong response.");
    x_ProcessResponse();
}

CRADispatcherClient::EJobState CRADispatcherClient::GetJobState()
{
    if (m_Result)
        return eReady;
    if (!m_Request)
        NCBI_THROW(CRADispatcherClientException, eRequestIsNotSet,
                   "Job's request is not set yet.");

    _ASSERT(!m_Jid.empty());
    if (m_Jid.empty())
        NCBI_THROW(CRADispatcherClientException, eRequestIsNotSet,
                   "JID is not found. Protocol error.");

    m_Stream << eGetStatusCmd << " ";
    if (!m_Stream.good())
        NCBI_THROW(CRADispatcherClientException, eIOError,
                   "Can not read/write to/from the communication stream.");

    WriteStrWithLen(m_Stream, m_Jid);

    EResId res = x_ReadResponseId();
    if (res != eJobStatusRes)
        NCBI_THROW(CRADispatcherClientException, eConfError,
                   "Wrong response.");
    return x_ProcessResponse();
}

CRemoteAppResultSB& CRADispatcherClient::GetResult()
{
    if (!m_Result) {
        GetJobState();
        if (!m_Result)
            NCBI_THROW(CRADispatcherClientException, eJobIsNotReady,
                       "Job is not ready yet.");
    }
    m_WasGetResultCalled = true;
    return *m_Result;
}

void CRADispatcherClient::Reset()
{
    if (m_Request && !m_Result) {
        m_Stream << eCancelJobCmd << " ";
        if (!m_Stream.good())
            NCBI_THROW(CRADispatcherClientException, eIOError,
                       "Can not read/write to/from the communication stream.");

        WriteStrWithLen(m_Stream, m_Jid);
        EResId res = x_ReadResponseId();
        if (res != eOkRes)
            NCBI_THROW(CRADispatcherClientException, eConfError,
                       "Wrong response.");
    }
    m_Request.Reset();
    m_Result.Reset();
    m_WasGetResultCalled = false;
    m_Jid = "";
}

CRADispatcherClient::EJobState
CRADispatcherClient::x_ProcessResponse()
{
    EJobState st = eRunning;

    int tmp;
    m_Stream >> tmp;
    CNetScheduleAPI::EJobStatus njstatus = (CNetScheduleAPI::EJobStatus)tmp;
    ReadStrWithLen(m_Stream, m_Jid);
    string msg;
    switch (njstatus) {
    case CNetScheduleAPI::ePending:
        st = ePending;
        break;
    case CNetScheduleAPI::eRunning:
        st = eRunning;
        break;
    case CNetScheduleAPI::eReturned:
        st = eRescheduled;
        break;
    case CNetScheduleAPI::eDone:
        st = eReady;
        m_Result.Reset(new CRemoteAppResultSB(m_Factory));
        m_Result->Receive(m_Stream);
        break;
    case CNetScheduleAPI::eCanceled:
        msg = "Job is canceled.";
        NCBI_THROW(CRADispatcherClientException, eJobExecutionError, msg);
        break;
    case CNetScheduleAPI::eFailed:
        ReadStrWithLen(m_Stream, msg);
        NCBI_THROW(CRADispatcherClientException, eJobExecutionError, msg);
        break;
    case CNetScheduleAPI::eJobNotFound:
    default:
        msg = "Job is lost.";
        NCBI_THROW(CRADispatcherClientException, eJobExecutionError, msg);
    }
    return st;
}

CRADispatcherClient::EResId CRADispatcherClient::x_ReadResponseId()
{
    int tmp;
    string error;
    m_Stream >> tmp;
    if (!m_Stream.good())
        NCBI_THROW(CRADispatcherClientException, eIOError,
                   "Can not read/write to/from the communication stream.");
    EResId res = (EResId) tmp;
    if (res == eErrorRes) {
        ReadStrWithLen(m_Stream, error);
        NCBI_THROW(CRADispatcherClientException, eConfError,
                   error);
    }
    return res;
}

END_NCBI_SCOPE
