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

#include <connect/services/grid_rw_impl.hpp>

#include "info_collector.hpp"

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

    CNetScheduleAPI& cln = m_Collector.x_GetAPI();
    m_Status = cln.GetJobDetails(m_Job);
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
CWNodeInfo::CWNodeInfo(const string& name, const string& prog,
               const string& host, unsigned short port,
               const CTime& last_access)
    : m_Name(name), m_Prog(prog), m_Host(host), m_Port(port), 
      m_LastAccess(last_access)
{
}

CWNodeInfo::~CWNodeInfo()
{
}

void CWNodeInfo::Shutdown(CNetScheduleAdmin::EShutdownLevel level) const
{
    CNetScheduleAPI cln(m_Host+":"+NStr::UIntToString(m_Port), "netschedule_admin", "noname");
    cln.GetAdmin().ShutdownServer(level);
}

///////////////////////////////////////////////////////////////////
//
CNSInfoCollector::CNSInfoCollector(const string& queue, 
                                   const string& service_name,
                                   CBlobStorageFactory& factory)
    : m_Services(new CNetScheduleAPI(service_name, "netschedule_admin", queue)), 
      m_Factory(factory)
{
    m_Services->DiscoverLowPriorityServers(eOn);
    m_Services->SetConnMode(CNetScheduleAPI::eKeepConnection);
}

CNSInfoCollector::CNSInfoCollector(CBlobStorageFactory& factory)
    : m_Factory(factory)
{
}


class ISimpleSink : public CNetServiceAPI_Base::ISink
{
public:
    ISimpleSink() : m_Str(new CNcbiStrstream) {}
    virtual ~ISimpleSink() {}

    virtual CNcbiOstream& GetOstream(CNetServerConnection conn)
    {
        return *m_Str;
    }
    virtual void EndOfData(CNetServerConnection conn)
    {
        ParseStream(*m_Str, conn);
        m_Str.reset(new CNcbiStrstream);
    }
    
private:

    virtual void ParseStream(CNcbiIstream& strm, CNetServerConnection conn) = 0;

    auto_ptr<CNcbiStrstream> m_Str;
    
};


class CJobsSink : public ISimpleSink
{
public:
    CJobsSink(CNSInfoCollector::IAction<CNSJobInfo>& action,
                CNSInfoCollector& collector) 
        : m_Action(action), m_Collector(collector)
    {}
    virtual ~CJobsSink() {}

private:
    CNSInfoCollector::IAction<CNSJobInfo>& m_Action;
    CNSInfoCollector& m_Collector;
    
    virtual void ParseStream(CNcbiIstream& str, CNetServerConnection)
    {
        while (str.good()) {
            string line;
            NcbiGetlineEOL(str, line);
            if (line.empty())
                continue;
            string jid, rest;
            NStr::SplitInTwo(line, " \t", jid, rest);
            CNSJobInfo job_info(jid, m_Collector);
            m_Action(job_info);
        }
    }
};


void CNSInfoCollector::TraverseJobs(CNetScheduleAPI::EJobStatus status, 
                                    IAction<CNSJobInfo>& action)
{
    CJobsSink sink(action, *this);
    x_GetAPI().GetAdmin().PrintQueue(sink, status);
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


class CQueuesSink : public ISimpleSink
{
public:
    CQueuesSink(CNSInfoCollector::TQueueCont& queues) :  m_Queues(queues) {}
    virtual ~CQueuesSink() {}

private:
    CNSInfoCollector::TQueueCont& m_Queues;
    
    virtual void ParseStream(CNcbiIstream& str, CNetServerConnection conn)
    {
        string line;
        NcbiGetlineEOL(str, line);
        if (line.empty())
            return;
        CNSInfoCollector::TQueueCont::key_type key
            (conn.GetHost(), conn.GetPort());
        list<string>& qlist = m_Queues[key];
        NStr::Split(line, ",;", qlist);        
    }
};


void CNSInfoCollector::GetQueues(TQueueCont& queues)
{
    CQueuesSink sink(queues);
    x_GetAPI().GetAdmin().GetQueueList(sink);
}


class CNodesSink : public ISimpleSink
{
public:
    CNodesSink(CNSInfoCollector::IAction<CWNodeInfo>& action,
               CNSInfoCollector& collector) 
        : m_Action(action), m_Collector(collector)
    {}
    virtual ~CNodesSink() {}

private:
    CNSInfoCollector::IAction<CWNodeInfo>& m_Action;
    CNSInfoCollector& m_Collector;
    std::set<pair<string, unsigned short> > m_Unique;
    
    virtual void ParseStream(CNcbiIstream& ios, CNetServerConnection conn)
    {
        bool nodes_info = false;        
        string str;
        while ( NcbiGetlineEOL(ios, str) ) {
            if ( NStr::StartsWith( str, "[Configured")) {
                break;
            }
            if ( !nodes_info ) {
                if( NStr::Compare( str, "[Worker node statistics]:") == 0)
                    nodes_info = true;
                else
                    continue;
            } else {
                if (str.empty())
                    continue;

                string name;
                NStr::SplitInTwo(str, " ", name, str);
                NStr::TruncateSpacesInPlace(str);
                string prog;
                NStr::SplitInTwo(str, "@", prog, str);           
                NStr::TruncateSpacesInPlace(str);
                prog = prog.substr(6,prog.size()-8);
                string host;
                NStr::SplitInTwo(str, " ", host, str);
                //string::size_type pos = host.find_first_of(".");
                //if (pos != string::npos) {
                //    host.erase(pos, host.size());
                //}
                if( NStr::Compare(host, "localhost") == 0)
                    host = CSocketAPI::gethostbyaddr(CSocketAPI::gethostbyname(conn.GetHost()));
                NStr::TruncateSpacesInPlace(str);
                string sport, stime;
                NStr::SplitInTwo(str, " ", sport, stime);
                NStr::SplitInTwo(sport, ":", str, sport);
                NStr::TruncateSpacesInPlace(stime);
                unsigned short port = (unsigned short)NStr::StringToInt(sport);
                if (m_Unique.insert(make_pair(host,port)).second) {
                    CWNodeInfo info(name, prog,host, port, CTime(stime, "M/D/Y h:m:s"));
                    m_Action(info);
                }
            }
        }
    }
};


void CNSInfoCollector::TraverseNodes(IAction<CWNodeInfo>& action)
{
    CNodesSink sink(action,*this);
    x_GetAPI().GetAdmin().GetServerStatistics(sink, CNetScheduleAdmin::eStaticticsBrief);
}

void CNSInfoCollector::DropQueue()
{
    x_GetAPI().GetAdmin().DropQueue();
}


END_NCBI_SCOPE
