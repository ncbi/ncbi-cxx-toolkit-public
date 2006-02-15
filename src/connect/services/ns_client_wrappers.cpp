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
string CNSCWrapperShared::GetConnectionInfo() const
{
    CFastMutexGuard gurad(m_Mutex);
    return m_NSClient.GetConnectionInfo();
}

bool CNSCWrapperShared::WaitJob(string*        job_key, 
                                string*        input, 
                                unsigned       wait_time,
                                unsigned short udp_port)
{
    CFastMutexGuard gurad(m_Mutex);
    //cerr << "WaitJob" << endl;
    return m_NSClient.WaitJob(job_key, input,
                              wait_time, udp_port);
}

void CNSCWrapperShared::PutResult(const string& job_key, 
                                  int           ret_code, 
                                  const string& output)
{
    CFastMutexGuard gurad(m_Mutex);
    //cerr << "PutResult" << endl;
    m_NSClient.PutResult(job_key, ret_code, output);
}


bool CNSCWrapperShared::PutResultGetJob(const string& done_job_key,
                                        int           done_ret_code, 
                                        const string& done_output,
                                        string*       new_job_key, 
                                        string*       new_input)
{
    CFastMutexGuard gurad(m_Mutex);
    //cerr << "PutResultGetJob" << endl;
    return m_NSClient.PutResultGetJob(done_job_key, done_ret_code, 
                                      done_output, new_job_key, new_input);
}

void CNSCWrapperShared::PutProgressMsg(const string& job_key,
                                       const string& progress_msg)
{
    CFastMutexGuard gurad(m_Mutex);
    //cerr << "PutProgressMsg" << endl;
    m_NSClient.PutProgressMsg(job_key, progress_msg);
}

string CNSCWrapperShared::GetProgressMsg(const string& job_key)
{
    CFastMutexGuard gurad(m_Mutex);
    //cerr << "GetProgressMsg" << endl;
    return m_NSClient.GetProgressMsg(job_key);
}

void CNSCWrapperShared::PutFailure(const string& job_key,
                                   const string& err_msg)
{
    CFastMutexGuard gurad(m_Mutex);
    //cerr << "PutFailure" << endl;
    m_NSClient.PutFailure(job_key, err_msg);
}

CNetScheduleClient::EJobStatus 
CNSCWrapperShared::GetStatus(const string& job_key,
                             int*          ret_code,
                             string*       output,
                             string*       err_msg,
                             string*       input)
{
    CFastMutexGuard gurad(m_Mutex);
    //cerr << "GetStatus" << endl;
    return m_NSClient.GetStatus(job_key, ret_code, output,
                                err_msg, input);
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

/////////////////////////////////////////////////////////////////////
///
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

bool CNSCWrapperExclusive::WaitJob(string*        job_key, 
                                   string*        input, 
                                   unsigned       wait_time,
                                   unsigned short udp_port)
{
    return m_NSClient->WaitJob(job_key, input,
                              wait_time, udp_port);
}

void CNSCWrapperExclusive::PutResult(const string& job_key, 
                                     int           ret_code, 
                                     const string& output)
{
    m_NSClient->PutResult(job_key, ret_code, output);
}


bool CNSCWrapperExclusive::PutResultGetJob(const string& done_job_key,
                                           int           done_ret_code, 
                                           const string& done_output,
                                           string*       new_job_key, 
                                           string*       new_input)
{
    return m_NSClient->PutResultGetJob(done_job_key, done_ret_code, 
                                      done_output, new_job_key, new_input);
                            
}

void CNSCWrapperExclusive::PutProgressMsg(const string& job_key,
                                          const string& progress_msg)
{
    m_NSClient->PutProgressMsg(job_key, progress_msg);
}

string CNSCWrapperExclusive::GetProgressMsg(const string& job_key)
{
    return m_NSClient->GetProgressMsg(job_key);
}

void CNSCWrapperExclusive::PutFailure(const string& job_key,
                                      const string& err_msg)
{
    m_NSClient->PutFailure(job_key, err_msg);
}

CNetScheduleClient::EJobStatus 
CNSCWrapperExclusive::GetStatus(const string& job_key,
                                int*          ret_code,
                                string*       output,
                                string*       err_msg,
                                string*       input)
{
    return m_NSClient->GetStatus(job_key, ret_code, output,
                                 err_msg, input);
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

END_NCBI_SCOPE

/*
 * ===========================================================================
 * $Log$
 * Revision 6.1  2006/02/15 15:19:03  didenko
 * Implemented an optional possibility for a worker node to have a permanent connection
 * to a NetSchedule server.
 * To expedite the job exchange between a worker node and a NetSchedule server,
 * a call to CNetScheduleClient::PutResult method is replaced to a
 * call to CNetScheduleClient::PutResultGetJob method.
 *
 * ===========================================================================
 */
 
