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

#include "netschedule_api_details.hpp"

#include <stdio.h>

BEGIN_NCBI_SCOPE



//////////////////////////////////////////////////////////////////////////////
static void s_SerializeJob(string& cmd, const CNetScheduleJob& job,
    unsigned short udp_port, unsigned wait_time, string& aff_prev)
{
    cmd.append("\"");
    string ps_input = NStr::PrintableString(job.input);

    cmd.append(ps_input);
    cmd.append("\"");

    if (!job.progress_msg.empty()) {
        cmd.append(" \"");
        cmd.append(job.progress_msg);
        cmd.append("\"");
    }

    if (udp_port != 0) {
        cmd.append(" ");
        cmd.append(NStr::UIntToString(udp_port));
        cmd.append(" ");
        cmd.append(NStr::UIntToString(wait_time));
    }

    if (!job.affinity.empty()) {
        if (job.affinity == aff_prev) { // exactly same affinity(sorted jobs)
            cmd.append(" affp");
        } else{
            cmd.append(" aff=\"");
            cmd.append(job.affinity);
            cmd.append("\"");
            aff_prev = job.affinity;
        }
    }

    if (job.mask != CNetScheduleAPI::eEmptyMask) {
        cmd.append(" msk=");
        cmd.append(NStr::UIntToString(job.mask));
    }

    if( !job.tags.empty() ) {
        string tags;
        ITERATE(CNetScheduleAPI::TJobTags, tag, job.tags) {
            if( tag != job.tags.begin() )
                tags.append("\t");
            tags.append(tag->first);
            tags.append("\t");
            tags.append(tag->second);
        }
        cmd.append(" tags=\"");
        cmd.append(NStr::PrintableString(tags));
        cmd.append("\"");
    }

}

inline
void static s_CheckInputSize(const string& input, size_t max_input_size)
{
    if (input.length() >  max_input_size) {
        NCBI_THROW(CNetScheduleException, eDataTooLong,
                   "Input data too long.");
    }
}

string CNetScheduleSubmitter::SubmitJobImpl(CNetScheduleJob& job,
    unsigned short udp_port, unsigned wait_time) const
{
    size_t max_input_size = m_API->GetServerParams().max_input_size;
    s_CheckInputSize(job.input, max_input_size);

    string cmd = "SUBMIT ";

    string aff_prev;
    s_SerializeJob(cmd, job, udp_port, wait_time, aff_prev);

    job.job_id = m_API->SendCmdWaitResponse(m_API->x_GetConnection(), cmd);

    if (job.job_id.empty()) {
        NCBI_THROW(CNetServiceException, eCommunicationError,
                   "Invalid server response. Empty key.");
    }

    return job.job_id;
}

void CNetScheduleSubmitter::SubmitJobBatch(vector<CNetScheduleJob>& jobs) const
{
    // veryfy the input data
    size_t max_input_size = m_API->GetServerParams().max_input_size;
    ITERATE(vector<CNetScheduleJob>, it, jobs) {
        const string& input = it->input;
        s_CheckInputSize(input, max_input_size);
    }

    CNetServerConnection conn = m_API->x_GetConnection();

    // batch command
    m_API->SendCmdWaitResponse(conn, "BSUB");

    string cmd;
    cmd.reserve(max_input_size * 6);
    string host;
    unsigned short port = 0;
    for (unsigned i = 0; i < jobs.size(); ) {

        // Batch size should be reasonable not to trigger network timeout
        const unsigned kMax_Batch = 10000;

        unsigned batch_size = jobs.size() - i;
        if (batch_size > kMax_Batch) {
            batch_size = kMax_Batch;
        }

        cmd.erase();
        cmd = "BTCH ";
        cmd.append(NStr::UIntToString(batch_size));

        conn.WriteStr(cmd +"\r\n");


        unsigned batch_start = i;
        string aff_prev;
        for (unsigned j = 0; j < batch_size; ++j,++i) {
            cmd.erase();
            s_SerializeJob(cmd, jobs[i], 0, 0, aff_prev);

            conn.WriteStr(cmd + "\r\n");
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
            jobs[batch_start].job_id = string(key);
            ++first_job_id;
            ++batch_start;
        }

        }}


    } // for

    m_API->SendCmdWaitResponse(conn, "ENDS");
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

        const char* job = buf.data() + 5;
        unsigned notif_job_id = (unsigned)::atoi(job);
        if (notif_job_id == m_JobId) {
            return true;
        }
        return false;
    }
    unsigned int m_JobId;
};


CNetScheduleAPI::EJobStatus
CNetScheduleSubmitter::SubmitJobAndWait(CNetScheduleJob& job,
                                        unsigned       wait_time,
                                        unsigned short udp_port)
{
    _ASSERT(wait_time);
    _ASSERT(udp_port);

    SubmitJobImpl(job, udp_port, wait_time);

    CNetScheduleKey key(job.job_id);

    s_WaitNotification(wait_time, udp_port, SWaitJobPred(key.id));

    CNetScheduleAPI::EJobStatus status = GetJobStatus(job.job_id);
    if ( status == CNetScheduleAPI::eDone || status == CNetScheduleAPI::eFailed)
        m_API->GetJobDetails(job);
    return status;
}

void CNetScheduleSubmitter::CancelJob(const string& job_key) const
{
    m_API->x_SendJobCmdWaitResponse("CANCEL", job_key);
}


END_NCBI_SCOPE
