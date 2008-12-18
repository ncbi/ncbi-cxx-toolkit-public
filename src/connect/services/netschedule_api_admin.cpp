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
 * Author:  Anatoliy Kuznetsov, Maxim Didenko, Dmitry Kazimirov
 *
 * File Description:
 *   Implementation of NetSchedule API.
 *
 */

#include <ncbi_pch.hpp>

#include "netschedule_api_impl.hpp"
#include "netschedule_api_details.hpp"

#include <connect/services/netschedule_api.hpp>


BEGIN_NCBI_SCOPE

void CNetScheduleAdmin::DropJob(const string& job_key)
{
    m_Impl->m_API->x_SendJobCmdWaitResponse("DROJ", job_key);
}

void CNetScheduleAdmin::ShutdownServer(CNetScheduleAdmin::EShutdownLevel level) const
{
    if (m_Impl->m_API->m_Service.IsLoadBalanced()) {
        NCBI_THROW(CNetScheduleException, eCommandIsNotAllowed,
                   "This command is allowed only for non-loadbalance client.");
    }

    string cmd = "SHUTDOWN ";
    switch( level ) {
    case eDie :
        cmd = "SHUTDOWN SUICIDE ";
        break;
    case eShutdownImmediate :
        cmd = "SHUTDOWN IMMEDIATE ";
        break;
    default:
        break;
    }

    m_Impl->m_API->x_GetConnection().Exec(cmd);
}


void CNetScheduleAdmin::ForceReschedule(const string& job_key) const
{
    m_Impl->m_API->x_SendJobCmdWaitResponse("FRES", job_key);
}

void CNetScheduleAdmin::ReloadServerConfig() const
{
    if (m_Impl->m_API->m_Service.IsLoadBalanced()) {
        NCBI_THROW(CNetScheduleException, eCommandIsNotAllowed,
                   "This command is allowed only for non-loadbalance client.");
    }
    m_Impl->m_API->x_GetConnection().Exec("RECO");
}

void CNetScheduleAdmin::CreateQueue(const string& qname, const string& qclass,
                                    const string& comment, ECreateQueueFlags flags) const
{
    string param = "QCRE " + qname + " " + qclass;
    if (!comment.empty()) {
        param.append(" \"");
        param.append(comment);
        param.append("\"");
    }
    SNSSendCmd::TFlags f = flags == eIgnoreDuplicateName ?
        SNSSendCmd::eIgnoreDuplicateNameError : 0;
    m_Impl->m_API->m_Service->ForEach(SNSSendCmd(m_Impl->m_API, param, f));
}


void CNetScheduleAdmin::DeleteQueue(const string& qname) const
{
    string param = "QDEL " + qname;

    m_Impl->m_API->m_Service->ForEach(SNSSendCmd(m_Impl->m_API, param, 0));
}

void CNetScheduleAdmin::DumpJob(CNcbiOstream& out, const string& job_key) const
{
    CNetServerCmdOutput output = m_Impl->m_API->x_GetConnection(
        job_key).ExecMultiline("DUMP " + job_key + "\r\n");

    std::string line;

    while (output.ReadLine(line))
        out << line << "\n";
}

void CNetScheduleAdmin::DropQueue() const
{
    string cmd = "DROPQ";
    m_Impl->m_API->m_Service->ForEach(SNSSendCmd(m_Impl->m_API, cmd, 0));
}


void CNetScheduleAdmin::PrintServerVersion(CNcbiOstream& output_stream) const
{
    m_Impl->m_API->m_Service->PrintCmdOutput("VERSION",
        output_stream, eSingleLineOutput);
}


void CNetScheduleAdmin::DumpQueue(CNcbiOstream& output_stream) const
{
    m_Impl->m_API->m_Service->PrintCmdOutput("DUMP",
        output_stream, eMultilineOutput);
}


void CNetScheduleAdmin::PrintQueue(CNcbiOstream& output_stream,
    CNetScheduleAPI::EJobStatus status) const
{
    string cmd = "QPRT " + CNetScheduleAPI::StatusToString(status);
    m_Impl->m_API->m_Service->PrintCmdOutput(cmd,
        output_stream, eMultilineOutput);
}

void CNetScheduleAdmin::GetWorkerNodes(
    std::list<SWorkerNodeInfo>& worker_nodes) const
{
    worker_nodes.clear();

    std::set<std::pair<std::string, unsigned short> > m_Unique;

    CNetServerGroup servers = m_Impl->m_API->m_Service.DiscoverServers();

    int i = servers.GetCount();

    while (--i >= 0) {
        CNetServerConnection conn = servers.GetServer(i).Connect();

        CNetServerCmdOutput output = conn.ExecMultiline("STAT");

        bool nodes_info = false;
        string response;

        while (output.ReadLine(response)) {
            if (NStr::StartsWith(response, "[Configured"))
                break;
            if (!nodes_info) {
                if (NStr::Compare(response, "[Worker node statistics]:") == 0)
                    nodes_info = true;
                else
                    continue;
            } else {
                if (response.empty())
                    continue;

                string name;
                NStr::SplitInTwo(response, " ", name, response);
                NStr::TruncateSpacesInPlace(response);
                string prog;
                NStr::SplitInTwo(response, "@", prog, response);
                NStr::TruncateSpacesInPlace(response);
                prog = prog.substr(6,prog.size()-8);
                string host;
                NStr::SplitInTwo(response, " ", host, response);

                if (NStr::Compare(host, "localhost") == 0)
                    host = CSocketAPI::gethostbyaddr(
                        CSocketAPI::gethostbyname(conn.GetHost()));

                NStr::TruncateSpacesInPlace(response);

                string sport, stime;

                NStr::SplitInTwo(response, " ", sport, stime);
                NStr::SplitInTwo(sport, ":", response, sport);

                unsigned short port = (unsigned short) NStr::StringToInt(sport);

                if (m_Unique.insert(make_pair(host,port)).second) {
                    worker_nodes.push_back(SWorkerNodeInfo());

                    SWorkerNodeInfo& wn_info = worker_nodes.back();
                    wn_info.name = name;
                    wn_info.prog = prog;
                    wn_info.host = host;
                    wn_info.port = port;

                    NStr::TruncateSpacesInPlace(stime);
                    wn_info.last_access = CTime(stime, "M/D/Y h:m:s");
                }
            }
        }
    }
}

void CNetScheduleAdmin::PrintServerStatistics(CNcbiOstream& output_stream,
    EStatisticsOptions opt) const
{
    m_Impl->m_API->m_Service->PrintCmdOutput(
        opt == eStatisticsBrief ? "STAT" : "STAT ALL",
        output_stream, eMultilineOutput);
}


void CNetScheduleAdmin::Monitor(CNcbiOstream & out) const
{
    if (m_Impl->m_API->m_Service.IsLoadBalanced()) {
        NCBI_THROW(CNetScheduleException, eCommandIsNotAllowed,
                   "This command is allowed only for non-loadbalance client.");
    }

    CNetServerConnection conn = m_Impl->m_API->x_GetConnection();
    conn->WriteLine("MONI QUIT");
    conn.Telnet(&out, NULL);
    conn->Close();
}

void CNetScheduleAdmin::Logging(bool on_off) const
{
    if (m_Impl->m_API->m_Service.IsLoadBalanced()) {
        NCBI_THROW(CNetScheduleException, eCommandIsNotAllowed,
                   "This command is allowed only for non-loadbalance client.");
    }
    string cmd = "LOG ";
    cmd += on_off ? "ON" : "OFF";

    m_Impl->m_API->x_GetConnection().Exec(cmd);
}


void CNetScheduleAdmin::GetQueueList(TQueueList& qlist) const
{
    CNetServerGroup servers = m_Impl->m_API->m_Service.DiscoverServers();

    int i = servers.GetCount();

    while (--i >= 0) {
        CNetServer server = servers.GetServer(i);

        qlist.push_back(SServerQueueList(server));

        NStr::Split(server.Connect().Exec("QLST"), ",;", qlist.back().queues);
    }
}


class CStatusProcessor : public CNetServerConnection::IStringProcessor
{
public:
    CStatusProcessor(CNetScheduleAPI api, CNetScheduleAdmin::TStatusMap&  status_map)
        : m_API(api), m_StatusMap(&status_map) {}

    virtual bool Process(string& line) {
        // FIXME
        m_API->m_Service->GetBestConnection()->CheckServerOK(line);
        if (line == "END")
            return false;
        // parse the status message
        string st_str, cnt_str;
        bool delim = NStr::SplitInTwo(line, " ", st_str, cnt_str);
        if (delim) {
           CNetScheduleAPI::EJobStatus status = CNetScheduleAPI::StringToStatus(st_str);
           unsigned cnt = NStr::StringToUInt(cnt_str);
           (*m_StatusMap)[status] += cnt;
        }
        return true;
    }

private:
    CNetScheduleAPI m_API;
    CNetScheduleAdmin::TStatusMap* m_StatusMap;
};

struct SNSTelnetRunner
{
    SNSTelnetRunner(const CNetScheduleAPI& api, const string& cmd,
                    CNetServerConnection::IStringProcessor& processor, CNcbiOstream *os = NULL)
        : m_API(&api), m_Cmd(cmd), m_Processor(&processor), m_Os(os)
    {}

    void operator()(CNetServerConnection conn)
    {
        conn->WriteLine(m_Cmd);
        conn.Telnet(m_Os, m_Processor);
    }

    const CNetScheduleAPI* m_API;
    string m_Cmd;
    CNetServerConnection::IStringProcessor* m_Processor;
    CNcbiOstream* m_Os;
};

void CNetScheduleAdmin::StatusSnapshot(
    CNetScheduleAdmin::TStatusMap&  status_map,
    const string& affinity_token) const
{
    string cmd = "STSN";
    cmd.append(" aff=\"");
    cmd.append(affinity_token);
    cmd.append("\"");

    CStatusProcessor processor(m_Impl->m_API, status_map);
    m_Impl->m_API->m_Service->ForEach(
        SNSTelnetRunner(m_Impl->m_API, cmd, processor));
}

struct SNSJobCounter : public SNSSendCmd
{
    SNSJobCounter(CNetScheduleAPI& api, const string query)
        : SNSSendCmd(api, "", 0), m_Counter(0)
    {
        m_Cmd = "QERY ";
        m_Cmd.append("\"");
        m_Cmd.append(NStr::PrintableString(query));
        m_Cmd.append("\" COUNT");
    }
    virtual void ProcessResponse(const string& resp,
        CNetServerConnection /* conn */)
    {
        m_Counter += NStr::StringToULong(resp);
    }
    unsigned long m_Counter;
};

unsigned long CNetScheduleAdmin::Count(const string& query) const
{
    SNSJobCounter counter = m_Impl->m_API->m_Service->
        ForEach(SNSJobCounter(m_Impl->m_API, query));
    return counter.m_Counter;
}

class CSimpleStringProcessor : public CNetServerConnection::IStringProcessor
{
public:
    explicit CSimpleStringProcessor(CNetScheduleAPI api) : m_API(api) {}

    virtual bool Process(string& line) {
        // FIXME
        m_API->m_Service->GetBestConnection()->CheckServerOK(line);
        return line != "END";
    }

private:
    CNetScheduleAPI m_API;
};

void CNetScheduleAdmin::Query(const string& query,
    const vector<string>& fields, CNcbiOstream& os) const
{
    if (fields.empty())
        return;
    string cmd = "QERY ";
    cmd.append("\"");
    cmd.append(NStr::PrintableString(query));
    cmd.append("\" SLCT \"");
    string sfields;
    ITERATE(vector<string>, it, fields) {
        if (it != fields.begin() )
            sfields.append(",");
        sfields.append(*it);
    }
    cmd.append(NStr::PrintableString(sfields));
    cmd.append("\"");

    CSimpleStringProcessor processor(m_Impl->m_API);
    m_Impl->m_API->m_Service->ForEach(
        SNSTelnetRunner(m_Impl->m_API, cmd, processor, &os));
}
void CNetScheduleAdmin::Select(const string& select_stmt, CNcbiOstream& os) const
{
    string cmd = "QSEL ";
    cmd.append("\"");
    cmd.append(NStr::PrintableString(select_stmt));
    cmd.append("\"");

    CSimpleStringProcessor processor(m_Impl->m_API);
    m_Impl->m_API->m_Service->ForEach(
        SNSTelnetRunner(m_Impl->m_API, cmd, processor, &os));
}


struct SNSJobIDsGetter : public SNSSendCmd
{
    SNSJobIDsGetter(CNetScheduleAPI api, const string query)
        : SNSSendCmd(api, "", 0), m_Counter(0)
    {
        m_Cmd = "QERY ";
        m_Cmd.append("\"");
        m_Cmd.append(NStr::PrintableString(query));
        m_Cmd.append("\" IDS");
    }
    virtual void ProcessResponse(const string& resp,
        CNetServerConnection conn)
    {
        string ip_addr = CSocketAPI::ntoa(
            CSocketAPI::gethostbyname(conn.GetHost()));
        m_IDsMap[SNetScheduleAdminImpl::TIDsMap::key_type(ip_addr,
            conn.GetPort())] = resp;
    }
    unsigned long m_Counter;
    SNetScheduleAdminImpl::TIDsMap m_IDsMap;
};

SNetScheduleAdminImpl::TIDsMap SNetScheduleAdminImpl::x_QueueIDs(
    const string& query) const
{
    SNSJobIDsGetter getter = m_API->m_Service->
        ForEach(SNSJobIDsGetter(m_API, query));

    return getter.m_IDsMap;
}

void CNetScheduleAdmin::RetrieveKeys(const string& query,
    CNetScheduleKeys& ids) const
{
    SNetScheduleAdminImpl::TIDsMap inter_ids = m_Impl->x_QueueIDs(query);
    ids.x_Clear();
    ITERATE(SNetScheduleAdminImpl::TIDsMap, it, inter_ids) {
        ids.x_Add(it->first, it->second);
    }
}

END_NCBI_SCOPE
