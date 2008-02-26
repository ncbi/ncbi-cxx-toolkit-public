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


BEGIN_NCBI_SCOPE

void CNetScheduleAdmin::DropJob(const string& job_key)
{
    m_API->x_SendJobCmdWaitResponse("DROJ", job_key);
}

void CNetScheduleAdmin::ShutdownServer(CNetScheduleAdmin::EShutdownLevel level) const
{
    if (m_API->IsLoadBalanced()) {
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

    m_API->SendCmdWaitResponse(m_API->x_GetConnectioin(), cmd);
}


void CNetScheduleAdmin::ForceReschedule(const string& job_key) const
{
    m_API->x_SendJobCmdWaitResponse("FRES", job_key);
}

void CNetScheduleAdmin::ReloadServerConfig() const
{
    if (m_API->IsLoadBalanced()) {
        NCBI_THROW(CNetScheduleException, eCommandIsNotAllowed,
                   "This command is allowed only for non-loadbalance client.");
    }
    m_API->SendCmdWaitResponse(m_API->x_GetConnectioin(), "RECO");
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
    SNSSendCmd::TFlags f = flags == eIgnoreDuplicateName ? SNSSendCmd::eIgnoreDuplicateNameError : 0;
    m_API->ForEach(SNSSendCmd(*m_API, param, f));
}


void CNetScheduleAdmin::DeleteQueue(const string& qname) const
{
    string param = "QDEL " + qname;

    m_API->ForEach(SNSSendCmd(*m_API, param, 0));
}

void CNetScheduleAdmin::DumpJob(CNcbiOstream& out, const string& job_key) const
{
    string cmd = "DUMP " + job_key + "\r\n";
    CNetServerConnection conn = m_API->x_GetConnectioin(job_key);
    conn.WriteStr(cmd);
    m_API->PrintServerOut(conn, out);
}

void CNetScheduleAdmin::DropQueue() const
{
    string cmd = "DROPQ";
    m_API->ForEach(SNSSendCmd(*m_API, cmd, 0));
}


///////////////////????????????????????????/////////////////////////
void CNetScheduleAdmin::GetServerVersion(CNetServiceAPI_Base::ISink& sink) const
{
    m_API->x_CollectStreamOutput("VERSION", sink, CNetServiceAPI_Base::eSendCmdWaitResponse);
}



void CNetScheduleAdmin::DumpQueue(CNetServiceAPI_Base::ISink& sink) const
{
    m_API->x_CollectStreamOutput("DUMP", sink, CNetServiceAPI_Base::ePrintServerOut);
}


void CNetScheduleAdmin::PrintQueue(CNetServiceAPI_Base::ISink& sink, CNetScheduleAPI::EJobStatus status) const
{
    string cmd = "QPRT " + CNetScheduleAPI::StatusToString(status);
    m_API->x_CollectStreamOutput(cmd, sink, CNetServiceAPI_Base::ePrintServerOut);
}



void CNetScheduleAdmin::GetServerStatistics(CNetServiceAPI_Base::ISink& sink,
                                            EStatisticsOptions opt) const
{
    string cmd = "STAT";
    if (opt == eStatisticsAll) {
        cmd += " ALL";
    }
    m_API->x_CollectStreamOutput(cmd, sink, CNetServiceAPI_Base::ePrintServerOut);
}


void CNetScheduleAdmin::Monitor(CNcbiOstream & out) const
{
    if (m_API->IsLoadBalanced()) {
        NCBI_THROW(CNetScheduleException, eCommandIsNotAllowed,
                   "This command is allowed only for non-loadbalance client.");
    }

    CNetServerConnection conn = m_API->x_GetConnectioin();
    conn.WriteStr("MONI QUIT\r\n");
    conn.Telnet(&out, NULL);
    conn.Close();
}

void CNetScheduleAdmin::Logging(bool on_off) const
{
    if (m_API->IsLoadBalanced()) {
        NCBI_THROW(CNetScheduleException, eCommandIsNotAllowed,
                   "This command is allowed only for non-loadbalance client.");
    }
    string cmd = "LOG ";
    cmd += on_off ? "ON" : "OFF";

    m_API->SendCmdWaitResponse(m_API->x_GetConnectioin(), cmd);
}


void CNetScheduleAdmin::GetQueueList(CNetServiceAPI_Base::ISink& sink) const
{
    m_API->x_CollectStreamOutput("QLST", sink, CNetServiceAPI_Base::eSendCmdWaitResponse);
}


class CStatusProcessor : public CNetServerConnection::IStringProcessor
{
public:
    CStatusProcessor(const CNetScheduleAPI& api, CNetScheduleAdmin::TStatusMap&  status_map)
        : m_API(&api), m_StatusMap(&status_map) {}

    virtual bool Process(string& line) {
        m_API->CheckServerOK(line);
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
    const CNetScheduleAPI* m_API;
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
        conn.WriteStr(m_Cmd);
        conn.Telnet(m_Os, m_Processor);
    }

    const CNetScheduleAPI* m_API;
    string m_Cmd;
    CNetServerConnection::IStringProcessor* m_Processor;
    CNcbiOstream* m_Os;
};

void CNetScheduleAdmin::StatusSnapshot(CNetScheduleAdmin::TStatusMap&  status_map,
                                       const string& affinity_token) const
{
    string cmd = "STSN";
    cmd.append(" aff=\"");
    cmd.append(affinity_token);
    cmd.append("\"\r\n");

    CStatusProcessor processor(*m_API, status_map);
    m_API->ForEach(SNSTelnetRunner(*m_API, cmd, processor));
}

struct SNSJobCounter : public SNSSendCmd
{
    SNSJobCounter(const CNetScheduleAPI& api, const string query)
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
    SNSJobCounter counter = m_API->ForEach(SNSJobCounter(*m_API, query));
    return counter.m_Counter;
}
void CNetScheduleAdmin::Cancel(const string& query) const
{
}
void CNetScheduleAdmin::Drop(const string& query) const
{
}

class CSimpleStringProcessor : public CNetServerConnection::IStringProcessor
{
public:
    explicit CSimpleStringProcessor(const CNetScheduleAPI& api) : m_API(&api) {}

    virtual bool Process(string& line) {
        m_API->CheckServerOK(line);
        return line != "END";
    }

private:
    const CNetScheduleAPI* m_API;
};
void CNetScheduleAdmin::Query(const string& query, const vector<string>& fields, CNcbiOstream& os) const
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
    cmd.append("\"\r\n");

    CSimpleStringProcessor processor(*m_API);
    m_API->ForEach(SNSTelnetRunner(*m_API, cmd, processor, &os));
}
void CNetScheduleAdmin::Select(const string& select_stmt, CNcbiOstream& os) const
{
    string cmd = "QSEL ";
    cmd.append("\"");
    cmd.append(NStr::PrintableString(select_stmt));
    cmd.append("\"\r\n");

    CSimpleStringProcessor processor(*m_API);
    m_API->ForEach(SNSTelnetRunner(*m_API, cmd, processor, &os));
}


struct SNSJobIDsGetter : public SNSSendCmd
{
    SNSJobIDsGetter(const CNetScheduleAPI& api, const string query)
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
        m_IDsMap[CNetScheduleAdmin::TIDsMap::key_type(ip_addr,
            conn.GetPort())] = resp;
    }
    unsigned long m_Counter;
    CNetScheduleAdmin::TIDsMap m_IDsMap;
};

CNetScheduleAdmin::TIDsMap CNetScheduleAdmin::x_QueueIDs(const string& query) const
{
    SNSJobIDsGetter getter = m_API->ForEach(SNSJobIDsGetter(*m_API, query));
    return getter.m_IDsMap;
}

END_NCBI_SCOPE
