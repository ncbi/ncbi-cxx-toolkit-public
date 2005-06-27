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
 * Author:  Maxim Didenko.
 *
 *
 */

#include <ncbi_pch.hpp>

#include "mgrlogic.hpp"

#include <corelib/ncbistr.hpp>
#include <connect/services/netschedule_client.hpp>
#include <connect/ncbi_conn_stream.hpp>

#include <functional>
#include <algorithm>

/////////////////////////////////////////////////////////////////////////////
//
// class CNSService

namespace {
struct ServiceInfoCreator
    :public unary_function<pair<string,string>, AutoPtr<CServiceInfo> >
{
    AutoPtr<CServiceInfo> operator() (const pair<string,string>& p)
    {
        return AutoPtr<CServiceInfo>(new CServiceInfo(p.first, p.second));
    }
};
        
}

CNSServices::CNSServices(const string& lbsurl)
{
    char buf[1024];
    CConn_HttpStream is(lbsurl);
    bool started = false;
    typedef map<string,string> TStrToStr;
    TStrToStr srv_host;
    while(is.good() && !is.eof()) {
        is.getline(buf,sizeof(buf));
        buf[is.gcount()] = 0;
        if (NStr::StartsWith(buf, "-------------------")) {
            if (started)
                break;
            started = true;
            continue;
        } 
        if (!started)
            continue;
        if (NStr::StartsWith(buf,"Service"))
            continue;
        string str = buf;
        vector<string> strs;
        NStr::Tokenize(str, " ", strs, NStr::eMergeDelims);
        if (strs.size() > 3) {
            string srv_name = NStr::TruncateSpaces(strs[0]);
            string srv_host_port = NStr::TruncateSpaces(strs[3]);

            TStrToStr::iterator it = srv_host.find(srv_name);
            if (it == srv_host.end()) {
                srv_host[srv_name] = srv_host_port;
            } else {
                string& s = it->second;
                s += ";" + srv_host_port;
            }
        }
    }
    transform(srv_host.begin(), srv_host.end(),
              back_inserter(m_Services),
              ServiceInfoCreator());

}

CNSServices::~CNSServices()
{
}


///////////////////////////////////////////////////////////////////////////////////
class CNetScheduleClient_Control : public CNetScheduleClient 
{
public:
    CNetScheduleClient_Control(const string&  host,
                               unsigned short port,
                               const string&  queue = "noname")
      : CNetScheduleClient(host, port, "netschedule_admin", queue)
    {}

    using CNetScheduleClient::ShutdownServer;
    using CNetScheduleClient::DropQueue;
    using CNetScheduleClient::PrintStatistics;
    using CNetScheduleClient::Monitor;
    using CNetScheduleClient::DumpQueue;
    using CNetScheduleClient::Logging;
    using CNetScheduleClient::ReloadServerConfig;
    using CNetScheduleClient::GetQueueList;
    // using CNetScheduleClient::CheckConnect;
    virtual void CheckConnect(const string& key)
    {
        CNetScheduleClient::CheckConnect(key);
    }
};

/////////////////////////////////////////////////////////////////////////////
//
// class CServiceInfo
namespace {
struct ServerDiscoverer : 
    public binary_function<string,CServiceInfo::THostInfos*,void>
{
    void operator()(const string& service, 
                    CServiceInfo::THostInfos* hosts) const
    {
        string host, sport;
        NStr::SplitInTwo(NStr::TruncateSpaces(service), ":", host, sport);
        if (!host.empty() && !sport.empty()) {
            try {
                unsigned int port = NStr::StringToUInt(sport);
                AutoPtr<CHostInfo> host_info( new CHostInfo(host,port));
                host_info->CollectInfo();
                hosts->push_back(host_info);
            } catch(...) {}
        }           
    }

};
}

void s_GetServerSet(const string& service_list, 
                    CServiceInfo::THostInfos& hosts)
{
    hosts.clear();
    list<string> srvs;
    NStr::Split(service_list, ";, ", srvs);
    for_each(srvs.begin(), srvs.end(), 
             bind2nd(ServerDiscoverer(),&hosts));
}


CServiceInfo::CServiceInfo(const string& name, const string& val)
    :m_Name(name)
{
    s_GetServerSet(val, m_Hosts);
}

CServiceInfo::~CServiceInfo()
{
}

/////////////////////////////////////////////////////////////////////////////
//
// class CHostInfo

CHostInfo::CHostInfo(const string& host, unsigned int port)
    : m_Host(host), m_Port(port), m_Active(false), 
      m_Version("Server is not responding")
{   
}

CHostInfo::~CHostInfo()
{
}

namespace {
struct QueueInfoCreator 
    : public binary_function<string,CHostInfo*,AutoPtr<CQueueInfo> >
{
    AutoPtr<CQueueInfo> operator()(const string& name, 
                                   CHostInfo* host) const
    {
        return AutoPtr<CQueueInfo>(new CQueueInfo(name,
                                                  host->GetHost(),
                                                  host->GetPort())) ;
    }
};
}

void CHostInfo::CollectInfo()
{
    try {
        CNetScheduleClient_Control cl(GetHost(), GetPort());
        m_Version = cl.ServerVersion();

        m_Queues.clear();
        string squeues = cl.GetQueueList();
        list<string> queues;
        NStr::Split( squeues, ";, ", queues);
        transform(queues.begin(), queues.end(),
                  back_inserter(m_Queues),
                  bind2nd(QueueInfoCreator(),this));

        m_Active = true;
    } catch(...) {}
}



/////////////////////////////////////////////////////////////////////////////
//
// class CQueueInfo

CQueueInfo::CQueueInfo(const string& name, const string& host, unsigned int port)
    : m_Name(name), m_Host(host), m_Port(port)
{
}
CQueueInfo::~CQueueInfo()
{
}

void CQueueInfo::CollectInfo()
{
    try {
        CNetScheduleClient_Control cl(GetHost(), GetPort(), GetName());
        
        m_WorkerNodes.clear();
        m_Info = "";

        CNcbiOstrstream os;
        cl.PrintStatistics(os);
        string stat = os.str();

        CNcbiIstrstream is(stat.c_str(), stat.length());
        
        char buff[2048];
        bool nodes_info = false;        
        while (!is.eof() && is.good()) {
            is.getline(buff, sizeof(buff));
            buff[is.gcount()] = 0;
            if ( !nodes_info ) {
                if( NStr::Compare( buff, "[Worker node statistics]:") == 0)
                    nodes_info = true;
                else {
                    m_Info += buff;
                    m_Info += "\n";
                }
            } else {
                if ( NStr::StartsWith( buff, "[Configured")) {
                    m_Info += buff;
                    m_Info += "\n";
                    nodes_info = false;
                    continue;
                }
                string str(buff);
                if (str.empty())
                    continue;

                try {
                    list<string> fields;
                    NStr::Split( str, " ", fields);           
                    string wn_host, wn_sport, stime, name;
                    name = *fields.begin();
                    list<string>::const_iterator lit = fields.end();
                    --lit;
                    stime = *lit; --lit;
                    stime = *lit + ' ' + stime; --lit;
                    wn_sport = (*lit).substr(4); --lit;
                    wn_host = *lit;

                    unsigned int wn_port = NStr::StringToUInt(wn_sport);
                    AutoPtr<CWorkerNodeInfo> node(
                                   new CWorkerNodeInfo(wn_host, wn_port) );
                    node->SetClientName(name);
                    node->SetLastAccess(CTime(stime, "M/D/Y h:m:s"));
                    
                    m_WorkerNodes.push_back(node);
                } catch(...) {}
                
            }
        }
    } catch(...) {}
}

/////////////////////////////////////////////////////////////////////////////
//
// class CWorkerNodeInfo

CWorkerNodeInfo::CWorkerNodeInfo(const string& host, unsigned int port)
    : m_Host(host), m_Port(port), m_Active(false),
      m_Version("Worker Node is not responding")
{
    try {
        CNetScheduleClient_Control cl(GetHost(), GetPort());
        string version = cl.ServerVersion();
        size_t pos = version.find("build");
        m_Version = version.substr(0, pos-1);
        string stime = version.substr(pos+6);
        m_BuildTime = CTime(stime, "b D Y h:m:s");
        m_Active = true;
    } catch(...) {}
}
CWorkerNodeInfo::~CWorkerNodeInfo()
{
}

string CWorkerNodeInfo::GetStatistics() const
{
    string info;
    try {
        CNetScheduleClient_Control cl(GetHost(), GetPort());
        CNcbiOstrstream os;
        cl.PrintStatistics(os);
        info = CNcbiOstrstreamToString(os);
    } catch(...) {}
    return info;
}

void CWorkerNodeInfo::SetLastAccess(const CTime& time) 
{ 
    m_LastAccess = time; 
/*    CTime now(CTime::eCurrent);
      if (now.DiffMinute(time) > 3.0)
          m_Active=false;*/
}

/*
 * ===========================================================================
 * $Log$
 * Revision 1.3  2005/06/27 19:10:57  didenko
 * cosmetics
 *
 * Revision 1.2  2005/06/27 16:40:15  didenko
 * Updated the statistics parser to paser an ouput of the NetSchedule ver. 1.4.4
 *
 * Revision 1.1  2005/06/27 12:52:40  didenko
 * Added grid manager cgi
 *
 * ===========================================================================
 */ 
