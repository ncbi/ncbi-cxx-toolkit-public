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
 * Authors:  Maxim Didenko
 *
 * File Description:
 *                   
 *
 */

#include <ncbi_pch.hpp>

#include "info_collector.hpp"

#include <connect/services/grid_rw_impl.hpp>

#include <corelib/blob_storage.hpp>
#include <corelib/rwstream.hpp>

BEGIN_NCBI_SCOPE


CNSJobInfo::CNSJobInfo(const string& id, CNSInfoCollector& collector)
    : m_Status(CNetScheduleAPI::eLastStatus),
      m_Collector(collector), m_Request(NULL), m_Result(NULL)
{
    m_Job.job_id = id;
}

CNSJobInfo::~CNSJobInfo()
{
}

CNetScheduleAPI::EJobStatus CNSJobInfo::GetStatus() const
{
    x_Load();
    return m_Status;
}

const string& CNSJobInfo::GetRawInput() const
{
    x_Load();
    return m_Job.input;
}

const string& CNSJobInfo::GetRawOutput() const
{
    x_Load();
    return m_Job.output;
}
const string& CNSJobInfo::GetErrMsg() const
{
    x_Load();
    return m_Job.error_msg;
}


CNcbiIstream& CNSJobInfo::GetStdIn() const
{
    return x_GetRequest().GetStdIn();
}
CNcbiIstream& CNSJobInfo::GetStdOut() const
{
    return x_GetResult().GetStdOut();
}

CNcbiIstream& CNSJobInfo::GetStdErr() const
{
    return x_GetResult().GetStdErr();
}
const string& CNSJobInfo::GetCmdLine() const
{
    return x_GetRequest().GetCmdLine();
}

int CNSJobInfo::GetRetCode() const
{
    x_Load();
    return m_Job.ret_code;
}

void CNSJobInfo::x_Load()
{
    if (m_Status != CNetScheduleAPI::eLastStatus)
        return;

    CNetScheduleAPI cln = m_Collector.x_GetAPI();
    m_Status = cln.GetJobDetails(m_Job);
    cln.GetProgressMsg(m_Job);
}
void CNSJobInfo::x_Load() const
{
    const_cast<CNSJobInfo*>(this)->x_Load();
}

CNcbiIstream* CNSJobInfo::x_CreateStream(const string& blob_or_data) const
{
    auto_ptr<IBlobStorage> storage(m_Collector.x_CreateStorage());
    auto_ptr<IReader> reader(new CStringOrBlobStorageReader(
                                             blob_or_data,
                                             storage.release()));
    return new CRStream (reader.release(),0,0,CRWStreambuf::fOwnReader);    
}


CRemoteAppRequest_Executer& CNSJobInfo::x_GetRequest() const
{
    if(!m_Request) {
        m_Request = &m_Collector.x_GetRequest();
        auto_ptr<CNcbiIstream> stream(x_CreateStream(GetRawInput()));
        m_Request->Receive(*stream);
    }
    return *m_Request;
}

CRemoteAppResult& CNSJobInfo::x_GetResult() const
{
    if(!m_Result) {
        m_Result = &m_Collector.x_GetResult();
        auto_ptr<CNcbiIstream> stream(x_CreateStream(GetRawOutput()));
        m_Result->Receive(*stream);
    }
    return *m_Result;
}

///////////////////////////////////////////////////////////////////
//
CNSInfoCollector::CNSInfoCollector(const string& queue, 
                                   const string& service_name,
                                   CBlobStorageFactory& factory)
    : m_Services(service_name, "netschedule_admin", queue),
      m_Factory(factory)
{
}

CNSInfoCollector::CNSInfoCollector(CBlobStorageFactory& factory)
    : m_Factory(factory)
{
}


void CNSInfoCollector::TraverseJobs(CNetScheduleAPI::EJobStatus status,
                                    IAction<CNSJobInfo>& action)
{
    for (CNetServerGroupIterator it = x_GetAPI().GetService().DiscoverServers(
            CNetService::eIncludePenalized).Iterate(); it; ++it) {
        CNetServerMultilineCmdOutput output((*it).ExecWithRetry("QPRT " +
                CNetScheduleAPI::StatusToString(status)));

        string response;

        while (output.ReadLine(response)) {
            if (response.empty())
                continue;
            string jid, rest;
            NStr::SplitInTwo(response, " \t", jid, rest);
            CNSJobInfo job_info(jid, *this);
            action(job_info);
        }
    }
}

CNSJobInfo* CNSInfoCollector::CreateJobInfo(const string& job_id)
{
    return new CNSJobInfo(job_id, *this);
}

CRemoteAppRequest_Executer& CNSInfoCollector::x_GetRequest()
{
    if(!m_Request.get()) {
        m_Request.reset(new CRemoteAppRequest_Executer(m_Factory));
    }
    return *m_Request;
}
CRemoteAppResult& CNSInfoCollector::x_GetResult()
{
    if(!m_Result.get()) {
        m_Result.reset(new CRemoteAppResult(m_Factory));
    }
    return *m_Result;
}

CNcbiIstream& CNSInfoCollector::GetBlobContent(const string& blob_id,
                                               size_t* blob_size)
{
    m_Storage.reset(x_CreateStorage());
    return m_Storage->GetIStream(blob_id, blob_size);
}


void CNSInfoCollector::GetQueues(CNetScheduleAdmin::TQueueList& queues)
{
    x_GetAPI().GetAdmin().GetQueueList(queues);
}


void CNSInfoCollector::TraverseNodes(
    IAction<CNetScheduleAdmin::SWorkerNodeInfo>& action)
{
    std::list<CNetScheduleAdmin::SWorkerNodeInfo> worker_nodes;

    x_GetAPI().GetAdmin().GetWorkerNodes(worker_nodes);

    ITERATE(std::list<CNetScheduleAdmin::SWorkerNodeInfo>, it, worker_nodes) {
        action(*it);
    }
}

void CNSInfoCollector::DropQueue()
{
    x_GetAPI().GetAdmin().DropQueue();
}

void CNSInfoCollector::CancelJob(const std::string& jid)
{
    x_GetAPI().GetSubmitter().CancelJob(jid);
}


END_NCBI_SCOPE
