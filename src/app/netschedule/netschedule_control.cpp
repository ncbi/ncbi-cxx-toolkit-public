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
 * Authors:  Anatoliy Kuznetsov
 *
 * File Description:  NetSchedule administration utility
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbiargs.hpp>
#include <corelib/ncbienv.hpp>
#include <corelib/ncbireg.hpp>
#include <corelib/ncbi_system.hpp>
#include <corelib/ncbimisc.hpp>

#include <corelib/ncbiexpt.hpp>

#include "client_admin.hpp"

#include <connect/services/netservice_api.hpp>


BEGIN_NCBI_SCOPE

CNetScheduleClient_Control* s_CreateNSClient(const string& host, unsigned int port,
                                             const string& queue, int retry = 1) 
{
    auto_ptr<CNetScheduleClient_Control> cln(new CNetScheduleClient_Control(host,port,queue));
    if (retry > 1) {
        for (int i = 0; i < retry; ++i) {
            try {
                cln->CheckConnect(kEmptyStr);
                break;
            } catch (exception&) {
                SleepMilliSec(5 * 1000);
            }
        }
    }
    return cln.release();
}

class CNSConnections;
template<>
struct CServiceClientTraits<CNSConnections> {
    typedef CNetScheduleClient_Control client_type;
};

struct CNSConnections : public CServiceConnections<CNSConnections>
{
public:
    typedef CNetScheduleClient_Control TClient;
    typedef CServiceConnections<CNSConnections> TParent;

    CNSConnections(const string& service, const string& queue, int retry = 1) 
        : TParent(service), m_Queue(queue), m_Retry(retry)
    {}

    const string& GetQueueName() const { return m_Queue; }

    TClient* CreateClient(const string& host, unsigned int port)
    {
        return s_CreateNSClient(host,port,m_Queue,m_Retry);
    }
private:
    string m_Queue;
    int m_Retry;
};


typedef ICtrlCmd<CNetScheduleClient_Control> INSCtrlCmd;

END_NCBI_SCOPE

USING_NCBI_SCOPE;

class VersionCmd : public INSCtrlCmd
{
public:
    
    explicit VersionCmd(CNcbiOstream& os) : INSCtrlCmd(os) {}

    virtual void Execute(CNetScheduleClient_Control& cln) {;
        GetStream() << cln.ServerVersion() << endl;
    }
};

class QueueListCmd : public INSCtrlCmd
{
public:
    explicit QueueListCmd(CNcbiOstream& os) : INSCtrlCmd(os) {}

    virtual void Execute(CNetScheduleClient_Control& cln) {;
        GetStream() << cln.GetQueueList() << endl;
    }
};
class ReconfCmd : public INSCtrlCmd
{
public:
    explicit ReconfCmd(CNcbiOstream& os) : INSCtrlCmd(os) {}

    virtual void Execute(CNetScheduleClient_Control& cln) {;
        cln.ReloadServerConfig();
        GetStream() << "Reconfigured." << endl;
    }
};

class QueueCreateCmd : public INSCtrlCmd
{
public:
    QueueCreateCmd(CNcbiOstream& os, const string& new_queue, const  string& qclass) 
        : INSCtrlCmd(os), m_QueueName(new_queue), m_QClass(qclass) {}

    virtual void Execute(CNetScheduleClient_Control& cln) {;
        cln.CreateQueue(m_QueueName, m_QClass);
        GetStream() << "Queue: \"" << m_QueueName << "\" is created." << endl;
    }
private:
    string m_QueueName;
    string m_QClass;
};
class DeleteQueueCmd : public INSCtrlCmd
{
public:
    explicit DeleteQueueCmd(CNcbiOstream& os) : INSCtrlCmd(os) {}

    virtual void Execute(CNetScheduleClient_Control& cln) {;
        cln.DeleteQueue(cln.GetQueueName());
        GetStream() << "Queue deleted: " << cln.GetQueueName() << endl;
    }
};

class StatCmd : public INSCtrlCmd
{
public:
    explicit StatCmd(CNcbiOstream& os) : INSCtrlCmd(os) {}

    virtual void Execute(CNetScheduleClient_Control& cln) {;
        GetStream() << "Queue: \"" << cln.GetQueueName() << "\"" << endl;
        cln.PrintStatistics(GetStream());
        GetStream() << endl;
    }
};
class DropJobsCmd : public INSCtrlCmd
{
public:
    explicit DropJobsCmd(CNcbiOstream& os) : INSCtrlCmd(os) {}

    virtual void Execute(CNetScheduleClient_Control& cln) {;
        cln.DropQueue();
        GetStream() << "Queue: \"" << cln.GetQueueName() << "\" dropped." << endl;
    }
};
class DumpCmd : public INSCtrlCmd
{
public:
    DumpCmd(CNcbiOstream& os, const string& jid) : INSCtrlCmd(os), m_Jid(jid) {}

    virtual void Execute(CNetScheduleClient_Control& cln) {;
        GetStream() << "Queue: \"" << cln.GetQueueName() << "\"" << endl;
        cln.DumpQueue(GetStream(), m_Jid);
        GetStream() << endl;
    }
private:
    string m_Jid;
};

class PrintQueueCmd : public INSCtrlCmd
{
public:
    PrintQueueCmd(CNcbiOstream& os, 
                  CNetScheduleClient::EJobStatus status) : INSCtrlCmd(os), m_Status(status) {}

    virtual void Execute(CNetScheduleClient_Control& cln) {;
        GetStream() << "Queue: \"" << cln.GetQueueName() << "\"" << endl;
        cln.PrintQueue(NcbiCout, m_Status);
        GetStream() << endl;
    }
private:
    CNetScheduleClient::EJobStatus m_Status;
};

class AffinityStatCmd : public INSCtrlCmd
{
public:
    AffinityStatCmd(CNcbiOstream& os, const string& affinity) 
        : INSCtrlCmd(os), m_Affinity(affinity) {}

    virtual void Execute(CNetScheduleClient_Control& cln) {;
        GetStream() << "Queue: \"" << cln.GetQueueName() << "\"" << endl;
        CNetScheduleClient::TStatusMap st_map;
        cln.StatusSnapshot(&st_map, m_Affinity);
        ITERATE(CNetScheduleClient::TStatusMap, it, st_map) {
            GetStream() << CNetScheduleClient::StatusToString(it->first) << ": "
                        << it->second
                        << endl;
        }
        GetStream() << endl;
    }
private:
    string m_Affinity;
};


/// NetSchedule control application
///
/// @internal
///
class CNetScheduleControl : public CNcbiApplication
{
public:
    void Init(void);
    int Run(void);

private:
    typedef CCtrlCmdRunner<CNetScheduleClient_Control,CNSConnections> TCmdRunner;

    void x_ExecCmd(INSCtrlCmd& cmd, bool queue_requered)
    {
        auto_ptr<CNSConnections> conn(x_CreateConnections(queue_requered));
        TCmdRunner runner(*conn, cmd);
        conn->for_each(runner);
    }

    void x_GetConnectionArgs(string& service, string& queue, int& retry, 
                             bool queue_requered);
    CNetScheduleClient_Control* x_CreateClient(bool queue_requered);
    CNSConnections* x_CreateConnections(bool queue_requered);

    bool CheckPermission();
};


void CNetScheduleControl::x_GetConnectionArgs(string& service, string& queue, int& retry,
                                              bool queue_requered)
{
    const CArgs& args = GetArgs();
    if (!args["service"]) 
        NCBI_THROW(CArgException, eNoArg, "Missing requered agrument: -service");
    if (queue_requered && !args["queue"] )
        NCBI_THROW(CArgException, eNoArg, "Missing requered agrument: -queue");

    queue = "noname";
    if (queue_requered && args["queue"] )
        queue = args["queue"].AsString();

    retry = 1;
    if (args["retry"]) {
        retry = args["retry"].AsInteger();
        if (retry < 1) {
            ERR_POST(Error << "Invalid retry count: " << retry);
            retry = 1;
        }
    }
    service = args["service"].AsString();               
}

CNSConnections* CNetScheduleControl::x_CreateConnections(bool queue_requered)
{
    string service,queue;
    int retry;
    x_GetConnectionArgs(service, queue, retry, queue_requered);
    return new CNSConnections(service, queue, retry); 
}

CNetScheduleClient_Control* CNetScheduleControl::x_CreateClient(bool queue_requered)
{
    string service,queue;
    int retry;
    x_GetConnectionArgs(service, queue, retry, queue_requered);

    string sport, host;
    if ( NStr::SplitInTwo(service, ":", host, sport) ) {
        try {
            unsigned int port = NStr::StringToInt(sport);
            return s_CreateNSClient(host,port,queue,retry);
        } catch (...) {
        }
    }
    NCBI_THROW(CArgException, eNoArg, 
               "Wrong service format: \"" + service + "\". It should be host:port.");
}


void CNetScheduleControl::Init(void)
{
    auto_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);

    arg_desc->SetUsageContext(GetArguments().GetProgramBasename(),
                              "NCBI NetSchedule control.");
    
    arg_desc->AddOptionalKey("service",
                             "service_name",
                             "NetSchedule service name. Format: service|host:port", 
                             CArgDescriptions::eString);

    arg_desc->AddOptionalKey("queue", 
                             "queue_name",
                             "NetSchedule queue name.", 
                             CArgDescriptions::eString);

    arg_desc->AddOptionalKey("jid", 
                             "job_id",
                             "NetSchedule job id.", 
                             CArgDescriptions::eString);

    
    arg_desc->AddFlag("shutdown", "Shutdown server");
    arg_desc->AddFlag("die", "Shutdown server");
    arg_desc->AddOptionalKey("log",
                             "server_logging",
                             "Switch server side logging",
                             CArgDescriptions::eBoolean);
    arg_desc->AddFlag("monitor", "Queue monitoring");


    arg_desc->AddFlag("ver", "Server version");
    arg_desc->AddFlag("reconf", "Reload server configuration");
    arg_desc->AddFlag("qlist", "List available queues");

    arg_desc->AddFlag("qcreate", "Create queue (qclass should be present)");

    arg_desc->AddOptionalKey("qclass",
                             "queue_class",
                             "Class for queue creation",
                             CArgDescriptions::eString);

    arg_desc->AddFlag("qdelete","Delete queue");

    arg_desc->AddFlag("drop", "Drop ALL jobs in the queue (no questions asked!)");
    arg_desc->AddFlag("stat", "Print queue statistics");
    arg_desc->AddOptionalKey("affstat",
                             "affinity",
                             "Print queue statistics summary based on affinity",
                             CArgDescriptions::eString);

    arg_desc->AddFlag("dump", "Print queue dump or job dump if -jid parameter is specified");

    arg_desc->AddOptionalKey("qprint",
                             "job_status",
                             "Print queue content for the specified job status",
                             CArgDescriptions::eString);


    arg_desc->AddOptionalKey("retry",
                             "retry",
                             "Number of re-try attempts if connection failed",
                             CArgDescriptions::eInteger);


    SetupArgDescriptions(arg_desc.release());
}

class NSClient : public INetServiceAPI
{
public:
    NSClient(const string& service_name, const string& client_name, const string& queue_name)
        : INetServiceAPI(service_name, client_name), m_QueueName(queue_name)
    {}



    enum EStatisticsOptions 
    {
        eStatisticsAll,
        eStaticticsBrief
    };
    void PrintStatistics(CNcbiOstream&      out, 
                         EStatisticsOptions opt = eStaticticsBrief) const;

    static string SendCmdWaitResponce(CNetSrvConnector& conn, const string& cmd, const string& job_key);

private:

    string m_QueueName;

    virtual void x_SendAuthetication(CNetSrvConnector& conn) const
    {
        conn.WriteStr(GetClientName() + "\r\n");
        conn.WriteStr(m_QueueName + "\r\n");
    }
    
};

/* static */
string NSClient::SendCmdWaitResponce(CNetSrvConnector& conn, const string& cmd, const string& job_key)
{
    string tmp = cmd;
    if (!job_key.empty())
        tmp += ' ' + job_key;
    tmp += "\r\n";
    conn.WriteStr(tmp);
    conn.WaitForServer();
    if (!conn.ReadStr(tmp)) {
        NCBI_THROW(CNetServiceException, eCommunicationError, 
                   "Communication error");
    }
    return tmp;
}

void NSClient::PrintStatistics(CNcbiOstream&   out,  EStatisticsOptions opt) const
{
    string cmd = "STAT";
    if (opt == NSClient::eStatisticsAll) {
        cmd += " ALL";
    }
    cmd += "\r\n";
    CNetSrvConnectorPoll::iterator it = GetPoll().begin();
    for( ; it != GetPoll().end(); ++it) {
        out << GetServiceName() << "(" << it->GetHost() << ":" << it->GetPort() << ")" << endl;
        it->WriteStr(cmd);
        PrintServerOut(*it, out);
    }
}

int CNetScheduleControl::Run(void)
{

    //    NSClient client("ns_test", "netschedule_admin", "sample");
    //    client.PrintStatistics(cout);

    //    return 0;

    const CArgs& args = GetArgs();
    CNcbiOstream& os = NcbiCout;

    // commands which can be performed only on a single server
    auto_ptr<CNetScheduleClient_Control> ctl;
    if (args["shutdown"]) {
        ctl.reset(x_CreateClient(false));
        ctl->ShutdownServer();
        os << "Shutdown request has been sent to server" << endl;
    }
    else if (args["die"]) {
        ctl.reset(x_CreateClient(false));
        ctl->ShutdownServer(CNetScheduleClient::eDie);
        os << "Die request has been sent to server" << endl;
    }
    else if (args["log"]) {
        ctl.reset(x_CreateClient(false));
        bool on_off = args["log"].AsBoolean();
        ctl->Logging(on_off);
        os << "Logging turned " 
           << (on_off ? "ON" : "OFF") << " on the server" << endl;
    }
    else if (args["monitor"]) {
        ctl.reset(x_CreateClient(true));
        ctl->Monitor(os);
    }

    if (ctl.get())
        return 0;

    auto_ptr<INSCtrlCmd> cmd;
    // commands which don't requeire a queue name as a parameter
    if (args["ver"]) 
        cmd.reset(new VersionCmd(os));

    else if (args["qlist"]) 
        cmd.reset(new QueueListCmd(os));

    else if (args["reconf"]) 
        cmd.reset(new ReconfCmd(os));

    else if (args["qcreate"]) {
        if (!args["qclass"] )
            NCBI_THROW(CArgException, eNoArg, "Missing requered agrument: -qclass");
        if (!args["queue"] )
            NCBI_THROW(CArgException, eNoArg, "Missing requered agrument: -queue");
        string new_queue = args["queue"].AsString();
        string qclass = args["qclass"].AsString();
        cmd.reset(new QueueCreateCmd(os, new_queue, qclass));
    }
    if (cmd.get()) {
        x_ExecCmd(*cmd, false);
        return 0;
    }
    
    // commands which requeire a queue name as a parameter
    if (args["stat"]) 
        cmd.reset(new StatCmd(os));
    else if (args["drop"])
        cmd.reset(new DropJobsCmd(os));
    else if (args["dump"]) {
        string jid;
        if (args["jid"] )
            jid = args["jid"].AsString();
        cmd.reset(new DumpCmd(os, jid));
    }
    else if (args["qprint"]) {
        string sstatus = args["qprint"].AsString();
        CNetScheduleClient::EJobStatus status =
            CNetScheduleClient::StringToStatus(sstatus);
        if (status == CNetScheduleClient::eJobNotFound) {
            ERR_POST("Status string unknown:" << sstatus);
            return 1;
        }
        cmd.reset(new PrintQueueCmd(os, status));
    }
    else if (args["affstat"]) {
        string affinity = args["affstat"].AsString();
        cmd.reset(new AffinityStatCmd(os, affinity));
    }
    else if (args["qdelete"])
        cmd.reset(new DeleteQueueCmd(os));

    if (cmd.get()) {
        x_ExecCmd(*cmd, true);
        return 0;
    }

    return 0;
}

bool CNetScheduleControl::CheckPermission()
{
    /*
#if defined(NCBI_OS_UNIX) && defined(HAVE_LIBCONNEXT)
    static gid_t gids[NGROUPS_MAX + 1];
    static int n_groups = 0;
    static uid_t uid = 0;

    if (!n_groups) {
        uid = geteuid();
        gids[0] = getegid();
        if ((n_groups = getgroups(sizeof(gids)/sizeof(gids[0])-1, gids+1)) < 0)
            n_groups = 1;
        else
            n_groups++;
    }
    for(int i = 0; i < n_groups; ++i) {
        group* grp = getgrgid(gids[i]);
        if ( NStr::Compare(grp->gr_name, "service") == 0 )
            return true;
    }
    return false;
        
#endif
    */
    return true;
}

int main(int argc, const char* argv[])
{
    return CNetScheduleControl().AppMain(argc, argv, 0, eDS_Default, 0);
}
