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
 * Author:  Anatoliy Kuznetsov, Maxim Didenko
 *
 * File Description:
 *   Implementation of NetSchedule API.
 *
 */

#include <ncbi_pch.hpp>
#include <connect/services/netschedule_api.hpp>

#include "netschedule_api_wait.hpp"

BEGIN_NCBI_SCOPE


void CNetScheduleExecuter::SetRunTimeout(const string& job_key, 
                                         unsigned      time_to_run) const
{
    m_API->x_SendJobCmdWaitResponse("JRTO" , job_key, time_to_run); 
}


void CNetScheduleExecuter::JobDelayExpiration(const string& job_key, 
                                              unsigned      runtime_inc) const
{
    m_API->x_SendJobCmdWaitResponse("JDEX" , job_key, runtime_inc); 
}



bool CNetScheduleExecuter::GetJob(string*        job_key, 
                                  string*        input,
                                  CNetScheduleAPI::TJobMask*      job_mask,
                                  unsigned short udp_port) const
{
    _ASSERT(job_key);
    _ASSERT(input);

    for (CNetSrvConnectorPoll::iterator it = m_API->GetPoll().random_begin(); 
         it != m_API->GetPoll().end(); ++it) {
        string cmd = "GET";
        if (udp_port != 0) 
            cmd += ' ' + NStr::IntToString(udp_port);
    
        string resp = m_API->SendCmdWaitResponse(*it, cmd);
        
        if (!resp.empty()) {
            CNetScheduleAPI::TJobMask j_mask = CNetScheduleAPI::eEmptyMask;
            string tmp;
            x_ParseGetJobResponse(job_key, input, &tmp, &tmp, &j_mask, resp);
            if (job_mask) *job_mask = j_mask;

            return true;
        }
    }
    return false;
}


bool CNetScheduleExecuter::GetJobWaitNotify(string*    job_key, 
                                            string*    input, 
                                            unsigned   wait_time,
                                            unsigned short udp_port,
                                            CNetScheduleAPI::TJobMask*  job_mask) const
{
    _ASSERT(job_key);
    _ASSERT(input);

    for (CNetSrvConnectorPoll::iterator it = m_API->GetPoll().random_begin(); 
         it != m_API->GetPoll().end(); ++it) {
        string cmd = "WGET";
        cmd += ' ' + NStr::IntToString(udp_port) + ' ' + NStr::IntToString(wait_time);

        string resp = m_API->SendCmdWaitResponse(*it, cmd);
        
        CNetScheduleAPI::TJobMask j_mask = CNetScheduleAPI::eEmptyMask;
        if (!resp.empty()) {
            string tmp;
            x_ParseGetJobResponse(job_key, input, &tmp, &tmp, &j_mask, resp);
            if (job_mask)  *job_mask = j_mask;
            return true;
        }
    }
    return false;
}


bool CNetScheduleExecuter::WaitJob(string*    job_key, 
                                   string*    input, 
                                   unsigned   wait_time,
                                   unsigned short udp_port,
                                   EWaitMode      wait_mode,
                                   CNetScheduleAPI::TJobMask*      job_mask) const
{
    //cerr << ">>WaitJob" << endl;
    bool job_received = 
        GetJobWaitNotify(job_key, input, wait_time, udp_port, job_mask);
    if (job_received) {
        return job_received;
    }

    if (wait_mode != eWaitNotification) {
        return 0;
    }
    WaitQueueNotification(wait_time, udp_port);

    // no matter is WaitResult we re-try the request
    // using reliable comm.level and notify server that
    // we no longer on the UDP socket

    bool ret = GetJob(job_key, input, job_mask, udp_port);
    return ret;

}

struct SWaitQueuePred {
    SWaitQueuePred(const string queue_name) : m_QueueName(queue_name)
    {
        m_MinLen = m_QueueName.size() + 9;
    }
    bool operator()(const string& buf) 
    {
        static const char* sign = "NCBI_JSQ_";
        if ((buf.size() < m_MinLen) || ((buf[0] ^ sign[0]) | (buf[1] ^ sign[1])) ) {
            return false;
        }
        
        const char* queue = buf.data() + 9;
        if ( m_QueueName == string(queue)) {
            return true;
        }
        return false;
    }
    string m_QueueName;
    size_t m_MinLen;
};


/* static */
bool CNetScheduleExecuter::WaitNotification(const string&  queue_name,
                                       unsigned       wait_time,
                                       unsigned short udp_port)
{
    return s_WaitNotification(wait_time, udp_port, SWaitQueuePred(queue_name));
}


void CNetScheduleExecuter::x_ParseGetJobResponse(string*        job_key, 
                                                 string*        input,
                                                 string*        jout,
                                                 string*        jerr,
                                                 CNetScheduleAPI::TJobMask*      job_mask,
                                                 const string&  response)
{
    // Server message format:
    //    JOB_KEY "input" ["out" ["err"]] [mask]

    _ASSERT(!response.empty());
    _ASSERT(input);
    _ASSERT(jout);
    _ASSERT(jerr);
    _ASSERT(job_mask);

    input->erase();
    job_key->erase();
    jout->erase();
    jerr->erase();

    const char* str = response.c_str();
    while (*str && isspace((unsigned char)(*str)))
        ++str;
    if (*str == 0) {
    throw_err:
        NCBI_THROW(CNetScheduleException, eProtocolSyntaxError, 
                   "Internal error. Cannot parse server output.");
    }

    for(;*str && !isspace((unsigned char)(*str)); ++str) {
        job_key->push_back(*str);
    }
    if (*str == 0) {
        goto throw_err;
    }

    while (*str && isspace((unsigned char)(*str)))
        ++str;

    if (*str && *str == '"') {
        ++str;
        for( ;*str && *str; ++str) {
            if (*str == '"' && *(str-1) != '\\') { ++str; break; }
            input->push_back(*str);
        }
    } else {
        goto throw_err;
    }

    *input = NStr::ParseEscapes(*input);

    // parse "out"
    while (*str && isspace((unsigned char)(*str)))
        ++str;
    if (*str == 0)
        return;

    if (*str == '"') {
        ++str;
        for( ;*str && *str; ++str) {
            if (*str == '"' && *(str-1) != '\\') { ++str; break; }
            jout->push_back(*str);
        }
    } else {
        goto throw_err;
    }
    *jout = NStr::ParseEscapes(*jout);

    while (*str && isspace((unsigned char)(*str)))
        ++str;

    if (*str == 0) {
        return;
    }
    if (*str == '"') {
        ++str;
        for( ;*str && *str; ++str) {
            if (*str == '"' && *(str-1) != '\\') { ++str; break; }
            jerr->push_back(*str);
        }
    } else {
        goto throw_err;
    }
    *jerr = NStr::ParseEscapes(*jerr);

    // parse mask
    while (*str && isspace((unsigned char)(*str)))
        ++str;
    if (*str == 0)
        return;

    *job_mask = atoi(str);
}


void CNetScheduleExecuter::PutResult(const string& job_key, 
                                     int           ret_code, 
                                     const string& output) const
{
    if (output.length() > kNetScheduleMaxDataSize) {
        NCBI_THROW(CNetScheduleException, eDataTooLong, 
            "Output data too long.");
    }

    m_API->x_SendJobCmdWaitResponse("PUT" , job_key, ret_code, output); 

}


bool CNetScheduleExecuter::PutResultGetJob(const string& done_job_key, 
                                           int           done_ret_code, 
                                           const string& done_output,
                                           string*       new_job_key, 
                                           string*       new_input,
                                           CNetScheduleAPI::TJobMask*     job_mask) const
{
    if (done_job_key.empty()) 
        return GetJob(new_job_key, new_input, job_mask, 0);

    _ASSERT(new_job_key);
    _ASSERT(new_input);

    if (done_output.length() > kNetScheduleMaxDataSize) {
        NCBI_THROW(CNetScheduleException, eDataTooLong, 
            "Output data too long.");
    }

    string resp = m_API->x_SendJobCmdWaitResponse("JXCG" , done_job_key, done_ret_code, done_output); 

    if (!resp.empty()) {

        CNetScheduleAPI::TJobMask j_mask = CNetScheduleAPI::eEmptyMask;
        string tmp;
        x_ParseGetJobResponse(new_job_key, new_input, &tmp, &tmp, &j_mask, resp);
        if (job_mask) *job_mask = j_mask;       
        _ASSERT(!new_job_key->empty());
        return true;
    }
    return false;
}


void CNetScheduleExecuter::PutProgressMsg(const string& job_key, 
                                          const string& progress_msg) const
{
    if (progress_msg.length() >= kNetScheduleMaxDataSize) {
        NCBI_THROW(CNetScheduleException, eDataTooLong, 
                   "Progress message too long");
    }
    m_API->x_SendJobCmdWaitResponse("MPUT" , job_key, progress_msg); 
}


void CNetScheduleExecuter::PutFailure(const string& job_key, 
                                      const string& err_msg,
                                      const string& output,
                                      int           ret_code) const
{
    if (output.length() > kNetScheduleMaxDataSize) {
        NCBI_THROW(CNetScheduleException, eDataTooLong, 
            "Output data too long.");
    }

    if (err_msg.length() >= kNetScheduleMaxErrSize) {
        NCBI_THROW(CNetScheduleException, eDataTooLong, 
                   "Error message too long");
    }

    m_API->x_SendJobCmdWaitResponse("FPUT" , job_key, err_msg, output, ret_code); 
}



void CNetScheduleExecuter::ReturnJob(const string& job_key) const
{
    m_API->x_SendJobCmdWaitResponse("RETURN" , job_key); 
}

void CNetScheduleExecuter::x_RegUnregClient(const string&  cmd, 
                                            unsigned short udp_port) const
{
    string tmp = cmd + ' ' + NStr::IntToString(udp_port);
    for (CNetSrvConnectorPoll::iterator it = m_API->GetPoll().begin(); 
         it != m_API->GetPoll().end(); ++it) {
        m_API->SendCmdWaitResponse(*it, tmp); 
    }
}


void CNetScheduleExecuter::RegisterClient(unsigned short udp_port) const
{
    x_RegUnregClient("REGC", udp_port);
}

void CNetScheduleExecuter::UnRegisterClient(unsigned short udp_port) const
{
    x_RegUnregClient("URGC", udp_port);
}



END_NCBI_SCOPE


/*
 * ===========================================================================
 * $Log$
 * Revision 6.2  2007/01/10 16:02:50  ucko
 * Fix compilation with GCC 2.95's (not quite standard) string implementation.
 *
 * Revision 6.1  2007/01/09 15:29:55  didenko
 * Added new API for NetSchedule service
 *
 * ===========================================================================
 */
