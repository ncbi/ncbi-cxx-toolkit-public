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

#include <corelib/blob_storage.hpp>
#include <corelib/rwstream.hpp>

#include <connect/ncbi_service.h>
#include <connect/services/grid_rw_impl.hpp>

#include "info_collector.hpp"

BEGIN_NCBI_SCOPE

CNSJobInfo::CNSJobInfo(const string& id, CNSInfoCollector& collector)
    : m_Id(id), m_Status(CNetScheduleClient::eLastStatus),
      m_RetCode(-1), m_Collector(collector), m_Request(NULL), m_Result(NULL)
{
}

CNSJobInfo::~CNSJobInfo()
{
}

CNetScheduleClient::EJobStatus CNSJobInfo::GetStatus() const
{
    x_Load();
    return m_Status;
}

const string& CNSJobInfo::GetRawInput() const
{
    x_Load();
    return m_Input;
}

const string& CNSJobInfo::GetRawOutput() const
{
    x_Load();
    return m_Output;
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
    return m_RetCode;
}

void CNSJobInfo::x_Load()
{
    if (m_Status != CNetScheduleClient::eLastStatus)
        return;

    CNSClientHelper& cln = m_Collector.x_GetClient(m_Id);
    m_Status = cln.GetStatus(m_Id, &m_RetCode, &m_Output, NULL, &m_Input);
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
CNSInfoCollector::CNSInfoCollector(const string& queue, 
                                   const string& service_name,
                                   CBlobStorageFactory& factory)
    : m_QueueName(queue), m_Factory(factory)
{
    SConnNetInfo* net_info = ConnNetInfo_Create(service_name.c_str());
    TSERV_Type stype = fSERV_Any | fSERV_Promiscuous;
    SERV_ITER srv_it = SERV_Open(service_name.c_str(), stype, 0, net_info);
    ConnNetInfo_Destroy(net_info);

    if (srv_it != 0) {
        const SSERV_Info* sinfo;
        while ((sinfo = SERV_GetNextInfoEx(srv_it, 0)) != 0) {
            
            string host = CSocketAPI::gethostbyaddr(sinfo->host);
            string::size_type pos = host.find_first_of(".");
            if (pos != string::npos) {
                host.erase(pos, host.size());
            }
            x_GetClient(host,sinfo->port);

        } // while
        SERV_Close(srv_it);
    }
}

CNSInfoCollector::CNSInfoCollector(const string& queue, 
                                   const string& host, unsigned short port,
                                   CBlobStorageFactory& factory)
    : m_QueueName(queue), m_Factory(factory)
{
    x_GetClient(host,port);
}

void CNSInfoCollector::TraverseJobs(CNetScheduleClient::EJobStatus status, 
                                    CNSInfoCollector::IJobAction& action)
{
    NON_CONST_ITERATE(TServices, it, m_Services) {
        CNcbiStrstream str;
        CNSClientHelper& cln = (*it->second);
        cln.PrintQueue(str, status);
        while (str.good()) {
            string line;
            NcbiGetlineEOL(str, line);
            if (line.empty())
                continue;
            string jid, rest;
            NStr::SplitInTwo(line, " \t", jid, rest);
            action(CNSJobInfo(jid, *this));
        }
    }
}

CNSClientHelper& CNSInfoCollector::x_GetClient(const string& job_id)
{
    CNetSchedule_Key key;
    CNetSchedule_ParseJobKey(&key, job_id);
    return x_GetClient(key.hostname, key.port);
}
    
CNSClientHelper& CNSInfoCollector::x_GetClient(const string& host, 
                                               unsigned short port)
{
    TServices::iterator it = m_Services.find(make_pair(host,port));
    if (it != m_Services.end())
        return *(it->second);
    AutoPtr<CNSClientHelper> cln(new CNSClientHelper(host, port, m_QueueName));
    cln->SetConnMode(CNetScheduleClient::eKeepConnection);
    CNSClientHelper* ret = &*cln;
    m_Services[make_pair(host,port)] = cln;
    return *ret;
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

END_NCBI_SCOPE

/*
 * ===========================================================================
 * $Log$
 * Revision 1.2  2006/05/19 14:07:20  didenko
 * Removed x_CreateStorage method redeclaration
 *
 * Revision 1.1  2006/05/19 13:40:40  didenko
 * Added ns_remote_job_control utility
 *
 * ===========================================================================
 */

