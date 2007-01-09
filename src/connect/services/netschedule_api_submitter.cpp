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



//////////////////////////////////////////////////////////////////////////////

string CNetScheduleSubmitter::SubmitJob(const string& input, 
                                        const string& progress_msg,
                                        const string& affinity_token,
                                        CNetScheduleAPI::TJobMask  job_mask,
                                        CNetScheduleAPI::TJobTags* tags) const
{
    if (input.length() > kNetScheduleMaxDataSize) {
        NCBI_THROW(CNetScheduleException, eDataTooLong, 
            "Input data too long.");
    }


    //cerr << "Input: " << input << endl;
    string cmd = "SUBMIT \"";
    cmd.append(NStr::PrintableString(input));
    cmd.append("\"");

    if (!progress_msg.empty()) {
        cmd.append(" \"");
        cmd.append(progress_msg);
        cmd.append("\"");
    }

    if (!affinity_token.empty()) {
        cmd.append(" aff=\"");
        cmd.append(affinity_token);
        cmd.append("\"");
    }

    if (job_mask != CNetScheduleAPI::eEmptyMask) {
        cmd.append(" msk=");
        cmd.append(NStr::UIntToString(job_mask));
    } else {
        cmd.append(" msk=0");
    }

    /*
    if( tags != NULL )
       // TODO 
    */

    string job_key = m_API->SendCmdWaitResponse(m_API->x_GetConnector(), cmd);

    if (job_key.empty()) {
        NCBI_THROW(CNetServiceException, eCommunicationError, 
                   "Invalid server response. Empty key.");
    }

    return job_key;
}

void CNetScheduleSubmitter::SubmitJobBatch(vector<SJobParams>& jobs) const
{
    // veryfy the input data
    ITERATE(vector<SJobParams>, it, jobs) {
        const string& input = it->input;

        if (input.length() > kNetScheduleMaxDataSize) {
            NCBI_THROW(CNetScheduleException, eDataTooLong, 
                "Input data too long.");
        }
    }

    CNetSrvConnector& conn = m_API->x_GetConnector();

    //    m_Sock->DisableOSSendDelay(true);

    // batch command
    
    m_API->SendCmdWaitResponse(conn, "BSUB");

    //m_Sock->DisableOSSendDelay(false);

    string host;
    unsigned short port;
    for (unsigned i = 0; i < jobs.size(); ) {

        // Batch size should be reasonable not to trigger network timeout
        const unsigned kMax_Batch = 10000;

        unsigned batch_size = jobs.size() - i;
        if (batch_size > kMax_Batch) {
            batch_size = kMax_Batch;
        }

        char buf[kNetScheduleMaxDataSize * 6];
        sprintf(buf, "BTCH %u", batch_size);

        conn.WriteBuf(buf, strlen(buf)+1);

        unsigned batch_start = i;
        string aff_prev;
        for (unsigned j = 0; j < batch_size; ++j,++i) {
            string input = NStr::PrintableString(jobs[i].input);
            const string& aff = jobs[i].affinity;
            if (aff[0]) {
                if (aff == aff_prev) { // exactly same affinity(sorted jobs)
                    sprintf(buf, "\"%s\" affp", 
                            input.c_str()
                            );
                } else {
                    sprintf(buf, "\"%s\" aff=\"%s\"", 
                            input.c_str(),
                            aff.c_str()
                            );
                    aff_prev = aff;
                }
            } else {
                aff_prev.erase();
                sprintf(buf, "\"%s\"", input.c_str());
            }
            conn.WriteBuf(buf, strlen(buf)+1);
        }

        string resp = m_API->SendCmdWaitResponse(conn, "ENDB");

        if (resp.empty()) {
            NCBI_THROW(CNetServiceException, eProtocolError, 
                    "Invalid server response. Empty key.");
        }

        // parse the batch answer
        // FORMAT:
        //  first_job_id host port

        {{
        const char* s = resp.c_str();
        unsigned first_job_id = ::atoi(s);

        if (host.empty()) {
            for (; *s != ' '; ++s) {
                if (*s == 0) {
                    NCBI_THROW(CNetServiceException, eProtocolError, 
                            "Invalid server response. Batch answer format.");
                }
            }
            ++s;
            if (*s == 0) {
                NCBI_THROW(CNetServiceException, eProtocolError, 
                        "Invalid server response. Batch answer format.");
            }
            for (; *s != ' '; ++s) {
                if (*s == 0) {
                    NCBI_THROW(CNetServiceException, eProtocolError, 
                            "Invalid server response. Batch answer format.");
                }
                host.push_back(*s);
            }
            ++s;
            if (*s == 0) {
                NCBI_THROW(CNetServiceException, eProtocolError, 
                        "Invalid server response. Batch answer format.");
            }

            port = atoi(s); 
            if (port == 0) {
                NCBI_THROW(CNetServiceException, eProtocolError, 
                        "Invalid server response. Port=0.");
            }
        }

        // assign job ids, protocol guarantees all jobs in batch will
        // receive sequential numbers, so server sends only first job id
        //
        for (unsigned j = 0; j < batch_size; ++j) {            
            CNetScheduleKey key(first_job_id, host, port);
            jobs[batch_start].id = string(key);
            ++first_job_id;
            ++batch_start;
        }
        

        }}


    } // for

    //m_Sock->DisableOSSendDelay(true);

    conn.WriteBuf("ENDS", 5); //???? do we need to wait for the last reponse?
}

struct SWaitJobPred {
    SWaitJobPred(unsigned int job_id) : m_JobId(job_id) {}
    bool operator()(const string& buf) 
    {
        static const char* sign = "JNTF";
        static size_t min_len = 5;
        if ((buf.size() < min_len) || ((buf[0] ^ sign[0]) | (buf[1] ^ sign[1])) ) {
            return false;
        }
        
        const char* job = &buf[5];
        unsigned notif_job_id = (unsigned)::atoi(job);
        if (notif_job_id == m_JobId) {
            return true;
        }
        return false;
    }
    unsigned int m_JobId;
};


CNetScheduleAPI::EJobStatus 
CNetScheduleSubmitter::SubmitJobAndWait(const string&  input,
                                        string*        job_key,
                                        unsigned       wait_time,
                                        unsigned short udp_port,
                                        const string&  progress_msg,
                                        const string&  affinity_token,
                                        CNetScheduleAPI::TJobMask  job_mask,
                                        CNetScheduleAPI::TJobTags* job_tags) const
{
    _ASSERT(job_key);
    _ASSERT(wait_time);
    _ASSERT(udp_port);

    *job_key = SubmitJob(input, progress_msg, affinity_token, job_mask, job_tags);

    CNetScheduleKey key(*job_key);

    s_WaitNotification(wait_time, udp_port, SWaitJobPred(key.id));

    return GetJobStatus(*job_key);
}

void CNetScheduleSubmitter::CancelJob(const string& job_key) const
{
    m_API->x_SendJobCmdWaitResponse("CANCEL", job_key);
}

string CNetScheduleSubmitter::GetProgressMsg(const string& job_key) const
{
    string resp = m_API->x_SendJobCmdWaitResponse("MGET", job_key);
    return NStr::ParseEscapes(resp);
}


END_NCBI_SCOPE


/*
 * ===========================================================================
 * $Log$
 * Revision 6.1  2007/01/09 15:29:55  didenko
 * Added new API for NetSchedule service
 *
 * ===========================================================================
 */
