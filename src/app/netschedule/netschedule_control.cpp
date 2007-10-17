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

#include <connect/services/netschedule_api.hpp>


USING_NCBI_SCOPE;


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

    void x_GetConnectionArgs(string& service, string& queue, int& retry, 
                             bool queue_requered);
    CNetScheduleAPI* x_CreateNewClient(bool queue_requered);

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
    /*
    if (args["retry"]) {
        retry = args["retry"].AsInteger();
        if (retry < 1) {
            ERR_POST(Error << "Invalid retry count: " << retry);
            retry = 1;
        }
    }
    */
    service = args["service"].AsString();               
}


CNetScheduleAPI* CNetScheduleControl::x_CreateNewClient(bool queue_requered)
{
    string service,queue;
    int retry;
    x_GetConnectionArgs(service, queue, retry, queue_requered);
    return new CNetScheduleAPI(service, "netschedule_admin", queue);
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

    arg_desc->AddFlag("qcreate", "Create queue (qclass should be present, and comment is an optional parameter)");

    arg_desc->AddOptionalKey("qclass",
                             "queue_class",
                             "Class for queue creation",
                             CArgDescriptions::eString);

    arg_desc->AddOptionalKey("comment",
                             "commeet",
                             "Optional parameter for qcreate command",
                             CArgDescriptions::eString);

    arg_desc->AddFlag("qdelete","Delete queue");

    arg_desc->AddFlag("drop", "Drop ALL jobs in the queue (no questions asked!)");
    arg_desc->AddOptionalKey("stat",
                             "type",
                             "Print queue statistics",
                             CArgDescriptions::eString);
    arg_desc->SetConstraint("stat", 
                            &(*new CArgAllow_Strings(NStr::eNocase), 
                              "brief", "all"));

    arg_desc->AddOptionalKey("affstat",
                             "affinity",
                             "Print queue statistics summary based on affinity",
                             CArgDescriptions::eString);

    arg_desc->AddFlag("dump", "Print queue dump or job dump if -jid parameter is specified");

    arg_desc->AddOptionalKey("qprint",
                             "job_status",
                             "Print queue content for the specified job status",
                             CArgDescriptions::eString);

    arg_desc->AddOptionalKey("count",
                             "query",
                             "Count all jobs with tags set by query string",
                             CArgDescriptions::eString);

    arg_desc->AddOptionalKey("query",
                             "query",
                             "Perform a query on the queue jobs",
                             CArgDescriptions::eString);
    arg_desc->AddOptionalKey("fields",
                             "fields_list",
                             "Fields (separated by ',') which should be returned by query",
                             CArgDescriptions::eString);

    arg_desc->AddOptionalKey("select",
                             "select_stmt",
                             "Perform a select query on the queue jobs",
                             CArgDescriptions::eString);

    arg_desc->AddFlag("showparams", "Show service paramters");

    /*
    arg_desc->AddOptionalKey("retry",
                             "retry",
                             "Number of re-try attempts if connection failed",
                             CArgDescriptions::eInteger);
    */

    SetupArgDescriptions(arg_desc.release());
}


class CSimpleSink : public INetServiceAPI::ISink
{
public:
    CSimpleSink(CNcbiOstream& os) : m_Os(os) {}
    ~CSimpleSink() {}
    
    virtual CNcbiOstream& GetOstream(CNetServerConnector& conn)
    {
        m_Os << conn.GetHost() << ":" << conn.GetPort() << endl;
        return m_Os;
    }
    virtual void EndOfData(CNetServerConnector& conn)
    {
        m_Os << endl;
    }
private:
    CNcbiOstream& m_Os;
};


int CNetScheduleControl::Run(void)
{

    //    NSClient client("ns_test", "netschedule_admin", "sample");
    //    client.PrintStatistics(cout);

    //    return 0;

    const CArgs& args = GetArgs();
    CNcbiOstream& os = NcbiCout;


    auto_ptr<CNetScheduleAPI> ctl;
    if (args["shutdown"]) {
        ctl.reset(x_CreateNewClient(false));
        ctl->GetAdmin().ShutdownServer();
        os << "Shutdown request has been sent to server" << endl;
    }
    else if (args["die"]) {
        ctl.reset(x_CreateNewClient(false));
        ctl->GetAdmin().ShutdownServer(CNetScheduleAdmin::eDie);
        os << "Die request has been sent to server" << endl;
    }
    else if (args["log"]) {
        ctl.reset(x_CreateNewClient(false));
        bool on_off = args["log"].AsBoolean();
        ctl->GetAdmin().Logging(on_off);
        os << "Logging turned " 
           << (on_off ? "ON" : "OFF") << " on the server" << endl;
    }
    else if (args["monitor"]) {
        ctl.reset(x_CreateNewClient(true));
        ctl->GetAdmin().Monitor(os);
    }
    else if( args["count"]) {
        ctl.reset(x_CreateNewClient(true));
        string query = args["count"].AsString();
        os << ctl->GetAdmin().Count(query) << endl;
    }
    else if( args["query"]) {
        ctl.reset(x_CreateNewClient(true));
        if (!args["fields"] )
            NCBI_THROW(CArgException, eNoArg, "Missing requered agrument: -fields");
        string query = args["query"].AsString();
        string sfields = args["fields"].AsString();
        vector<string> fields;
        NStr::Tokenize(sfields, ",", fields);
        ctl->GetAdmin().Query(query, fields, os);
        os << endl;
    }
    else if( args["select"]) {
        ctl.reset(x_CreateNewClient(true));
        string select_stmt = args["select"].AsString();
        ctl->GetAdmin().Select(select_stmt, os);
        os << endl;
    }
    else if (args["reconf"]) {
        ctl.reset(x_CreateNewClient(false));
        ctl->GetAdmin().ReloadServerConfig();
        os << "Reconfigured." << endl;
    }
    else if (args["qcreate"]) {
        ctl.reset(x_CreateNewClient(false));
        if (!args["qclass"] )
            NCBI_THROW(CArgException, eNoArg, "Missing requered agrument: -qclass");
        if (!args["queue"] )
            NCBI_THROW(CArgException, eNoArg, "Missing requered agrument: -queue");
        string comment;
        if (args["comment"]) {
            comment = args["comment"].AsString();
        }
        string new_queue = args["queue"].AsString();
        string qclass = args["qclass"].AsString();
        ctl->GetAdmin().CreateQueue(new_queue, qclass, comment);
        os << "Queue \"" << new_queue << "\" has been created." << endl;
    }
    else if (args["qdelete"]) {
        ctl.reset(x_CreateNewClient(true));
        ctl->GetAdmin().DeleteQueue(ctl->GetQueueName());
        os << "Queue \"" << ctl->GetQueueName() << "\" has been deleted." << endl;
    }
    else if (args["drop"]) {
        ctl.reset(x_CreateNewClient(true));
        ctl->GetAdmin().DropQueue();
        os << "All jobs from the queue \"" << ctl->GetQueueName() 
           << "\" has been dropped." << endl;
    }
    else if (args["showparams"]) {
        ctl.reset(x_CreateNewClient(true));
        CNetScheduleAPI::SServerParams params = ctl->GetServerParams();
        os << "Server parameters for the queue \"" << ctl->GetQueueName() 
           << "\":" << endl 
           << "max_input_size = " << params.max_input_size << endl
           << "max_output_size = " << params.max_output_size << endl
           << "fast status is " 
           << (params.fast_status? "supported" : "not supported") << endl;
    }

    else if (args["dump"]) {
        ctl.reset(x_CreateNewClient(true));
        string jid;
        if (args["jid"] ) {
            jid = args["jid"].AsString();
            ctl->GetAdmin().DumpJob(os,jid);
        } else {            
            CSimpleSink sink(os);
            ctl->GetAdmin().DumpQueue(sink);
        }
    }
    else if (args["ver"]) {
        ctl.reset(x_CreateNewClient(false));
        CSimpleSink sink(os);
        ctl->GetAdmin().GetServerVersion(sink);
    }
    else if (args["qlist"]) {
        ctl.reset(x_CreateNewClient(false));
        CSimpleSink sink(os);
        ctl->GetAdmin().GetQueueList(sink);
    }
    else if (args["stat"]) {
        string sstatus = args["stat"].AsString();
        CNetScheduleAdmin::EStatisticsOptions st = CNetScheduleAdmin::eStaticticsBrief;
        if (NStr::CompareNocase(sstatus, "all") == 0) 
            st = CNetScheduleAdmin::eStatisticsAll;
        ctl.reset(x_CreateNewClient(true));
        CSimpleSink sink(os);
        ctl->GetAdmin().GetServerStatistics(sink, st);
    }
    else if (args["qprint"]) {
        string sstatus = args["qprint"].AsString();
        CNetScheduleAPI::EJobStatus status =
            CNetScheduleAPI::StringToStatus(sstatus);
        if (status == CNetScheduleAPI::eJobNotFound) {
            ERR_POST("Status string unknown:" << sstatus);
            return 1;
        }
        ctl.reset(x_CreateNewClient(true));
        CSimpleSink sink(os);
        ctl->GetAdmin().PrintQueue(sink, status);
    }
    else if (args["affstat"]) {
        string affinity = args["affstat"].AsString();
        ctl.reset(x_CreateNewClient(true));

        os << "Queue: \"" << ctl->GetQueueName() << "\"" << endl;
        CNetScheduleAdmin::TStatusMap st_map;
        ctl->GetAdmin().StatusSnapshot(st_map, affinity);
        ITERATE(CNetScheduleAdmin::TStatusMap, it, st_map) {
            os << CNetScheduleAPI::StatusToString(it->first) << ": "
               << it->second
               << endl;
        }
        os << endl;
    } else {
        NCBI_THROW(CArgException, eNoArg, 
                   "Unknown command or command is not specified.");
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
    return CNetScheduleControl().AppMain(argc, argv); //, 0, eDS_Default, 0);
}
