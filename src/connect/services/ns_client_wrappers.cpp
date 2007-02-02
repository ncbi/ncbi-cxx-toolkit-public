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

#include <connect/services/ns_client_wrappers.hpp>
#include <connect/services/grid_globals.hpp>

BEGIN_NCBI_SCOPE

/////////////////////////////////////////////////////////////////////
///
INSCWrapper::~INSCWrapper()
{
}


/////////////////////////////////////////////////////////////////////
///
CNSCWrapperShared::~CNSCWrapperShared()
{
}

const string& CNSCWrapperShared::GetQueueName() const
{
    CFastMutexGuard gurad(m_Mutex);
    return m_NSClient.GetQueueName();
}

const string& CNSCWrapperShared::GetClientName() const
{
    CFastMutexGuard gurad(m_Mutex);
    return m_NSClient.GetClientName();
}
const string& CNSCWrapperShared::GetServiceName() const
{
    CFastMutexGuard gurad(m_Mutex);
    return m_NSClient.GetServiceName();
}

bool CNSCWrapperShared::GetJob(CNetScheduleJob& job,
                               unsigned short udp_port)
{
    CFastMutexGuard gurad(m_Mutex);
    bool ret = m_NSClient.GetJob(job, udp_port);
    if ( ret && (job.mask & CNetScheduleAPI::eExclusiveJob))
        CGridGlobals::GetInstance().SetExclusiveMode(true);
    return ret;
}

bool CNSCWrapperShared::WaitJob(CNetScheduleJob&  job, 
                                unsigned       wait_time,
                                unsigned short udp_port,
                                CNetScheduleExecuter::EWaitMode wait_mode)
{
    CFastMutexGuard gurad(m_Mutex);
    bool ret = m_NSClient.WaitJob(job, wait_time, udp_port, wait_mode);
    if (ret && (job.mask & CNetScheduleAPI::eExclusiveJob))
        CGridGlobals::GetInstance().SetExclusiveMode(true);
    return ret;
}

void CNSCWrapperShared::PutResult(const CNetScheduleJob& job)
{
    CFastMutexGuard gurad(m_Mutex);
    m_NSClient.PutResult(job);
}


bool CNSCWrapperShared::PutResultGetJob(const CNetScheduleJob& done_job,
                                        CNetScheduleJob& new_job)
{
    CFastMutexGuard gurad(m_Mutex);
    bool ret = m_NSClient.PutResultGetJob(done_job, new_job);
    if (ret && (new_job.mask & CNetScheduleAPI::eExclusiveJob))
        CGridGlobals::GetInstance().SetExclusiveMode(true);
    return ret;
}

void CNSCWrapperShared::PutProgressMsg(const CNetScheduleJob& job)
{
    CFastMutexGuard gurad(m_Mutex);
    m_NSClient.PutProgressMsg(job);
}

void CNSCWrapperShared::GetProgressMsg(CNetScheduleJob& job)
{
    CFastMutexGuard gurad(m_Mutex);
    //cerr << "GetProgressMsg" << endl;
    m_NSClient.GetProgressMsg(job);
}


void CNSCWrapperShared::PutFailure(const CNetScheduleJob& job)
{
    CFastMutexGuard gurad(m_Mutex);
    //cerr << "PutFailure" << endl;
    m_NSClient.PutFailure(job);
}

CNetScheduleAPI::EJobStatus 
CNSCWrapperShared::GetJobStatus(const string& job_key)
{
    CFastMutexGuard gurad(m_Mutex);
    //cerr << "GetStatus" << endl;
    return m_NSClient.GetJobStatus(job_key);
}

void CNSCWrapperShared::ReturnJob(const string& job_key)
{
    CFastMutexGuard gurad(m_Mutex);
    //cerr << "ReturnJob" << endl;
    m_NSClient.ReturnJob(job_key);
}

void CNSCWrapperShared::SetRunTimeout(const string& job_key, 
                                      unsigned time_to_run)
{
    CFastMutexGuard gurad(m_Mutex);
    m_NSClient.SetRunTimeout(job_key, time_to_run);
}

void CNSCWrapperShared::JobDelayExpiration(const string& job_key, 
                                           unsigned time_to_run)
{
    CFastMutexGuard gurad(m_Mutex);
    m_NSClient.JobDelayExpiration(job_key, time_to_run);
}

void CNSCWrapperShared::RegisterClient(unsigned short udp_port)
{
    CFastMutexGuard gurad(m_Mutex);
    m_NSClient.RegisterClient(udp_port);
}
void CNSCWrapperShared::UnRegisterClient(unsigned short udp_port)
{
    CFastMutexGuard gurad(m_Mutex);
    m_NSClient.UnRegisterClient(udp_port);
}


/////////////////////////////////////////////////////////////////////
/// Is not used anymore
/*
CNSCWrapperExclusive::~CNSCWrapperExclusive()
{
}

const string& CNSCWrapperExclusive::GetQueueName() const
{
    return m_NSClient->GetQueueName();
}

const string& CNSCWrapperExclusive::GetClientName() const
{
    return m_NSClient->GetClientName();
}
string CNSCWrapperExclusive::GetConnectionInfo() const
{
    return m_NSClient->GetConnectionInfo();
}

bool CNSCWrapperExclusive::GetJob(CNetScheduleJob& job, 
                                  unsigned short udp_port)
{
    CFastMutexGuard gurad(m_Mutex);
    bool ret = m_NSClient->GetJob(job, udp_port);
    if (ret && (job.mask & CNetScheduleAPI::eExclusiveJob))
        CGridGlobals::GetInstance().SetExclusiveMode(true);
    return ret;
}

bool CNSCWrapperExclusive::WaitJob(CNetScheduleJob& job, 
                                   unsigned       wait_time,
                                   unsigned short udp_port,
                                   CNetScheduleExecuter::EWaitMode wait_mode)

{
    CFastMutexGuard gurad(m_Mutex);
    bool ret = m_NSClient->WaitJob(job, wait_time, udp_port, wait_mode);
    if (ret && (job.mask & CNetScheduleAPI::eExclusiveJob))
        CGridGlobals::GetInstance().SetExclusiveMode(true);
    return ret;
}

void CNSCWrapperExclusive::PutResult(const CNetScheduleJob& job)
{
    m_NSClient->PutResult(job);
}


bool CNSCWrapperExclusive::PutResultGetJob(const CNetScheduleJob& done_job,
                                           CNetScheduleJob& new_job)
{
    CFastMutexGuard gurad(m_Mutex);
    bool ret = m_NSClient->PutResultGetJob(done_job, new_job);
    if (ret && (new_job.mask & CNetScheduleClient::eExclusiveJob))
        CGridGlobals::GetInstance().SetExclusiveMode(true);
    return ret;
                            
}

void CNSCWrapperExclusive::PutProgressMsg(const CNetScheduleJob& job)
{
    m_NSClient->PutProgressMsg(job);
}


string CNSCWrapperExclusive::GetProgressMsg(const string& job_key)
{
    return m_NSClient->GetProgressMsg(job_key);
}


void CNSCWrapperExclusive::PutFailure(const CNetScheduleJob& job)
{
    m_NSClient->PutFailure(job);
}

CNetScheduleAPI::EJobStatus 
CNSCWrapperExclusive::GetJobStatus(const string& job_key)
{
    return m_NSClient->GetJobStatus(job);
}

void CNSCWrapperExclusive::ReturnJob(const string& job_key)
{
    m_NSClient->ReturnJob(job_key);
}

void CNSCWrapperExclusive::SetRunTimeout(const string& job_key, 
                                         unsigned time_to_run)
{
    m_NSClient->SetRunTimeout(job_key, time_to_run);
}

void CNSCWrapperExclusive::JobDelayExpiration(const string& job_key, 
                                              unsigned time_to_run)
{
    m_NSClient->JobDelayExpiration(job_key, time_to_run);
}

void CNSCWrapperExclusive::RegisterClient(unsigned short udp_port)
{
    m_NSClient->RegisterClient(udp_port);
}
void CNSCWrapperExclusive::UnRegisterClient(unsigned short udp_port)
{
    m_NSClient->UnRegisterClient(udp_port);
}
*/

END_NCBI_SCOPE

/*
 * ===========================================================================
 * $Log$
 * Revision 6.5  2006/06/28 16:01:57  didenko
 * Redone job's exlusivity processing
 *
 * Revision 6.4  2006/05/22 18:11:43  didenko
 * Added an option to fail a job if a remote app returns non zore code
 *
 * Revision 6.3  2006/05/10 19:54:21  didenko
 * Added JobDelayExpiration method to CWorkerNodeContext class
 * Added keep_alive_period and max_job_run_time parmerter to the config
 * file of remote_app
 *
 * Revision 6.2  2006/02/27 14:50:21  didenko
 * Redone an implementation of IBlobStorage interface based on NetCache as a plugin
 *
 * Revision 6.1  2006/02/15 15:19:03  didenko
 * Implemented an optional possibility for a worker node to have a permanent connection
 * to a NetSchedule server.
 * To expedite the job exchange between a worker node and a NetSchedule server,
 * a call to CNetScheduleClient::PutResult method is replaced to a
 * call to CNetScheduleClient::PutResultGetJob method.
 *
 * ===========================================================================
 */
 
