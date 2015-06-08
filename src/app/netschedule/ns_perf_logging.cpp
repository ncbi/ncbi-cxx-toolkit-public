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
 * Authors:  Sergey Satskiy
 *
 * File Description: NetSchedule performance logging
 *
 */


#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>
#include <corelib/perf_log.hpp>

#include "ns_perf_logging.hpp"
#include "ns_queue.hpp"
#include "job.hpp"
#include "ns_types.hpp"



BEGIN_NCBI_SCOPE


struct JobEventToAgent {
    enum CJobEvent::EJobEvent   event;
    string                      agent;
    string                      variation;
};

const JobEventToAgent   jobEventToAgentMap[] = {
    { CJobEvent::eUnknown,              kEmptyStr,    kEmptyStr     },
    { CJobEvent::eSubmit,               kEmptyStr,    kEmptyStr     },
    { CJobEvent::eBatchSubmit,          kEmptyStr,    kEmptyStr     },
    { CJobEvent::eRequest,              "GET",        kEmptyStr     },
    { CJobEvent::eDone,                 "PUT",        kEmptyStr     },
    { CJobEvent::eReturn,               "RETURN",     kEmptyStr     },
    { CJobEvent::eFail,                 "FPUT",       kEmptyStr     },
    { CJobEvent::eFinalFail,            "FPUT",       kEmptyStr     },
    { CJobEvent::eRead,                 "READ",       kEmptyStr     },
    { CJobEvent::eReadFail,             "FRED",       kEmptyStr     },
    { CJobEvent::eReadFinalFail,        "FRED",       kEmptyStr     },
    { CJobEvent::eReadDone,             "CFRM",       kEmptyStr     },
    { CJobEvent::eReadRollback,         "ns",         "rollback"    },
    { CJobEvent::eClear,                "CLRN",       "clear"       },
    { CJobEvent::eCancel,               "CANCEL",     kEmptyStr     },
    { CJobEvent::eTimeout,              "ns",         "timeout"     },
    { CJobEvent::eReadTimeout,          "ns",         "timeout"     },
    { CJobEvent::eSessionChanged,       "ns",         "new_session" },
    { CJobEvent::eNSSubmitRollback,     kEmptyStr,    kEmptyStr     },
    { CJobEvent::eNSGetRollback,        "ns",         "rollback"    },
    { CJobEvent::eNSReadRollback,       "ns",         "rollback"    },
    { CJobEvent::eReturnNoBlacklist,    "RETURN",     kEmptyStr     },
    { CJobEvent::eReschedule,           "RESCHEDULE", "reschedule"  } };
const size_t    jobEventToAgentMapSize = sizeof(jobEventToAgentMap) /
                                         sizeof(JobEventToAgent);

static string
g_JobEventToAgent(enum CJobEvent::EJobEvent  event)
{
    for (size_t  k = 0; k < jobEventToAgentMapSize; ++k) {
        if (jobEventToAgentMap[k].event == event)
            return jobEventToAgentMap[k].agent;
    }
    return kEmptyStr;
}

static string
g_JobEventToVariation(enum CJobEvent::EJobEvent  event)
{
    for (size_t  k = 0; k < jobEventToAgentMapSize; ++k) {
        if (jobEventToAgentMap[k].event == event)
            return jobEventToAgentMap[k].variation;
    }
    return kEmptyStr;
}



static string
s_FormResourceName(CNetScheduleAPI::EJobStatus  from,
                   CNetScheduleAPI::EJobStatus  to)
{
    return CNetScheduleAPI::StatusToString(from) + "_" +
           CNetScheduleAPI::StatusToString(to);
}



// Produces a record for the performance logging
void g_DoPerfLogging(const CQueue &  queue, const CJob &  job, int  status)
{
    if (!CPerfLogger::IsON())
        return;

    const vector<CJobEvent>&    job_events = job.GetEvents();
    size_t                      last_index = job_events.size() - 1;

    if (last_index == 0 || job_events.empty())
        return;     // Must never happened

    CJobEvent::EJobEvent    last_event = job_events[last_index].GetEvent();
    string                  agent = g_JobEventToAgent(last_event);
    if (agent.empty())
        return;     // Must never happened

    string              variation = g_JobEventToVariation(last_event);
    TJobStatus          status_from = job_events[last_index - 1].GetStatus();
    TJobStatus          status_to = job_events[last_index].GetStatus();
    CNSPreciseTime      time_from = job_events[last_index - 1].GetTimestamp();
    CNSPreciseTime      time_to = job_events[last_index].GetTimestamp();
    CNSPreciseTime      time_diff = time_to - time_from;

    // Create a logger and set the time
    CPerfLogger         perf_logger(CPerfLogger::eSuspend);
    perf_logger.Adjust(CTimeSpan(time_diff.Sec(), time_diff.NSec()));

    // Log the event and a variation
    CDiagContext_Extra  extra = perf_logger.Post(status,
                                                 s_FormResourceName(status_from,
                                                                    status_to));
    extra.Print("qname", queue.GetQueueName())
         .Print("job_key", queue.MakeJobKey(job.GetId()))
         .Print("agent", agent);

    string          client_node;
    unsigned int    client_host;
    if (last_event == CJobEvent::eSessionChanged) {
        client_node = job_events[last_index - 1].GetClientNode();
        client_host = job_events[last_index - 1].GetNodeAddr();

        string        new_session_client_node =
                                    job_events[last_index].GetClientNode();
        unsigned int  new_session_client_host =
                                    job_events[last_index].GetNodeAddr();
        if (!new_session_client_node.empty())
            extra.Print("new_session_client_node", new_session_client_node);
        if (new_session_client_host != 0)
            extra.Print("new_session_client_host",
                        CSocketAPI::gethostbyaddr(new_session_client_host));
    } else {
        client_node = job_events[last_index].GetClientNode();
        client_host = job_events[last_index].GetNodeAddr();
    }

    if (!client_node.empty())
        extra.Print("client_node", client_node);
    if (client_host != 0)
        extra.Print("client_host", CSocketAPI::gethostbyaddr(client_host));


    if (!variation.empty())
        extra.Print("variation", variation);

    // Handle the run/read counters and clients
    if (status_from == CNetScheduleAPI::eRunning ||
        status_to == CNetScheduleAPI::eRunning)
        extra.Print("run_count", job.GetRunCount());
    if (status_from == CNetScheduleAPI::eReading ||
        status_to == CNetScheduleAPI::eReading)
        extra.Print("read_count", job.GetReadCount());
}


END_NCBI_SCOPE

