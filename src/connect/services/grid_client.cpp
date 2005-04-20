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
#include <connect/services/grid_client.hpp>

BEGIN_NCBI_SCOPE

//////////////////////////////////////////////////////////////////////////////
//
CGridClient::CGridClient(CNetScheduleClient& ns_client, 
                         INetScheduleStorage& storage,
                         ECleanUp cleanup,
                         EProgressMsg progress_msg)
: m_NSClient(ns_client), m_NSStorage(storage)
{
    m_JobSubmiter.reset(new CGridJobSubmiter(*this,
                                             progress_msg == eProgressMsgOn));
    m_JobStatus.reset(new CGridJobStatus(*this, 
                                         cleanup == eAutomaticCleanup,
                                         progress_msg == eProgressMsgOn));
}
 
CGridClient::~CGridClient()
{
}

CGridJobSubmiter& CGridClient::GetJobSubmiter()
{
    return *m_JobSubmiter;
}
CGridJobStatus& CGridClient::GetJobStatus(const string& job_key)
{
    m_JobStatus->x_SetJobKey(job_key);
    return *m_JobStatus;
}

void CGridClient::CancelJob(const string& job_key)
{
    m_NSClient.CancelJob(job_key);
}
void CGridClient::RemoveDataBlob(const string& data_key)
{
    m_NSStorage.DeleteBlob(data_key);
}

//////////////////////////////////////////////////////////////////////////////
//
CGridJobSubmiter::CGridJobSubmiter(CGridClient& grid_client, bool use_progress)
    : m_GridClient(grid_client), m_UseProgress(use_progress)
{
}

CGridJobSubmiter::~CGridJobSubmiter()
{
}
void CGridJobSubmiter::SetJobInput(const string& input)
{
    m_Input = input;
}
CNcbiOstream& CGridJobSubmiter::GetOStream()
{
    return m_GridClient.GetStorage().CreateOStream(m_Input);
}

string CGridJobSubmiter::Submit()
{
    m_GridClient.GetStorage().Reset();
    string progress_msg_key;
    if (m_UseProgress)
        progress_msg_key = m_GridClient.GetStorage().CreateEmptyBlob();
    string job_key = m_GridClient.GetNSClient().SubmitJob(m_Input,
                                                          progress_msg_key);
    m_Input.erase();

    if (m_UseProgress)
        job_key += "|" + progress_msg_key;
    return job_key;
}

//////////////////////////////////////////////////////////////////////////////
//

CGridJobStatus::CGridJobStatus(CGridClient& grid_client, 
                               bool auto_cleanup, 
                               bool use_progress)
    : m_GridClient(grid_client), m_RetCode(0), m_BlobSize(0), 
      m_AutoCleanUp(auto_cleanup), m_UseProgress(use_progress)
{
}

CGridJobStatus::~CGridJobStatus()
{
}

CNetScheduleClient::EJobStatus CGridJobStatus::GetStatus()
{
    CNetScheduleClient::EJobStatus status = m_GridClient.GetNSClient().
            GetStatus(m_JobKey, &m_RetCode, &m_Output, &m_ErrMsg, &m_Input);
    if ( m_AutoCleanUp && (
              status == CNetScheduleClient::eDone || 
              status == CNetScheduleClient::eCanceled) ) {
        m_GridClient.RemoveDataBlob(m_Input);
        if (m_UseProgress)
            m_GridClient.RemoveDataBlob(m_ProgressMsgKey);
        m_Input.erase();
        m_ProgressMsgKey.erase();
    }
    return status;
}

CNcbiIstream& CGridJobStatus::GetIStream()
{
    return m_GridClient.GetStorage().GetIStream(m_Output,&m_BlobSize);
}

string CGridJobStatus::GetProgressMessage()
{    
    if (m_UseProgress)
        return m_GridClient.GetStorage().GetBlobAsString(m_ProgressMsgKey);
    return kEmptyStr;
}

void CGridJobStatus::x_SetJobKey(const string& job_key)
{
    if (m_UseProgress)
        NStr::SplitInTwo(job_key, "|", m_JobKey, m_ProgressMsgKey);
    else 
        m_JobKey = job_key;
    m_Output.erase();
    m_ErrMsg.erase();
    m_Input.erase();
    m_RetCode = 0;
    m_BlobSize = 0;
    m_GridClient.GetStorage().Reset();
}


END_NCBI_SCOPE

/*
 * ===========================================================================
 * $Log$
 * Revision 1.4  2005/04/20 19:25:59  didenko
 * Added support for progress messages passing from a worker node to a client
 *
 * Revision 1.3  2005/04/18 18:54:24  didenko
 * Added optional automatic NetScheduler storage cleanup
 *
 * Revision 1.2  2005/03/25 21:36:33  ucko
 * Empty strings with erase() rather than clear() for GCC 2.95 compatibility.
 *
 * Revision 1.1  2005/03/25 16:25:41  didenko
 * Initial version
 *
 * ===========================================================================
 */
 
