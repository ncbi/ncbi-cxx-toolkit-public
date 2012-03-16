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
#include <connect/services/util.hpp>

#include <common/ncbi_package_ver.h>

#define NETSCHEDULE_CONTROL_VERSION_MAJOR 1
#define NETSCHEDULE_CONTROL_VERSION_MINOR 2
#define NETSCHEDULE_CONTROL_VERSION_PATCH 0

USING_NCBI_SCOPE;

/// NetSchedule control application
///
/// @internal
///
class CNetScheduleControl : public CNcbiApplication
{
public:
    CNetScheduleControl() {
        SetVersion(CVersionInfo(
            NETSCHEDULE_CONTROL_VERSION_MAJOR,
            NETSCHEDULE_CONTROL_VERSION_MINOR,
            NETSCHEDULE_CONTROL_VERSION_PATCH));
    }
    void Init(void);
    int Run(void);

private:
    void x_GetConnectionArgs(string& service, string& queue, int& retry,
                             bool queue_required);
    CNetScheduleAPI x_CreateNewClient(bool queue_required);
};


void CNetScheduleControl::x_GetConnectionArgs(string& service, string& queue, int& retry,
                                              bool queue_required)
{
    const CArgs& args = GetArgs();
    if (!args["service"])
        NCBI_THROW(CArgException, eNoArg, "Missing required agrument: -service");
    if (queue_required && !args["queue"] )
        NCBI_THROW(CArgException, eNoArg, "Missing required agrument: -queue");

    queue = "noname";
    if (queue_required && args["queue"] )
        queue = args["queue"].AsString();

    retry = 1;

    service = args["service"].AsString();
}


CNetScheduleAPI CNetScheduleControl::x_CreateNewClient(bool queue_required)
{
    string service,queue;
    int retry;
    x_GetConnectionArgs(service, queue, retry, queue_required);
    CNetScheduleAPI ns_api(service, "netschedule_admin", queue);
    ns_api.EnableWorkerNodeCompatMode();
    return ns_api;
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

    arg_desc->AddFlag("getconf", "Print queue configuration");

    arg_desc->AddFlag("shutdown", "Shutdown server");
    arg_desc->AddFlag("shutdown_now", "Shutdown server IMMIDIATE");
    arg_desc->AddFlag("die", "Shutdown server");

    arg_desc->AddFlag("ver", "Server version");
    arg_desc->AddFlag("reconf", "Reload server configuration");
    arg_desc->AddFlag("qlist", "List available queues");

    arg_desc->AddFlag("qcreate", "Create queue (qclass should be present, and comment is an optional parameter)");

    arg_desc->AddOptionalKey("qclass",
                             "queue_class",
                             "Class for queue creation",
                             CArgDescriptions::eString);

    arg_desc->AddOptionalKey("comment",
                             "comment",
                             "Optional parameter for qcreate command",
                             CArgDescriptions::eString);

    arg_desc->AddFlag("qdelete","Delete queue");

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

    arg_desc->AddOptionalKey("cancel",
                             "job_key",
                             "Cancel a job",
                             CArgDescriptions::eString);

    arg_desc->AddFlag("count_active", "Count active jobs in all queues");

    arg_desc->AddOptionalKey("fields",
                             "fields_list",
                             "Fields (separated by ',') which should be returned by query",
                             CArgDescriptions::eString);

    arg_desc->AddFlag("showparams", "Show service parameters");

    SetupArgDescriptions(arg_desc.release());
}


int CNetScheduleControl::Run(void)
{
    const CArgs& args = GetArgs();

    CNcbiOstream& os = NcbiCout;

    CNetScheduleAPI ctl;

    if (args["shutdown"]) {
        ctl = x_CreateNewClient(false);
        ctl.GetAdmin().ShutdownServer();
        os << "Shutdown request has been sent to server" << endl;
    }
    else if (args["shutdown_now"]) {
        ctl = x_CreateNewClient(false);
        ctl.GetAdmin().ShutdownServer(CNetScheduleAdmin::eShutdownImmediate);
        os << "Shutdown IMMEDIATE request has been sent to server" << endl;
    }
    else if (args["die"]) {
        ctl = x_CreateNewClient(false);
        ctl.GetAdmin().ShutdownServer(CNetScheduleAdmin::eDie);
        os << "Die request has been sent to server" << endl;
    }
    else if (args["count_active"]) {
        os << x_CreateNewClient(false).GetAdmin().CountActiveJobs() << endl;
    }
    else if (args["reconf"]) {
        ctl = x_CreateNewClient(false);
        ctl.GetAdmin().ReloadServerConfig();
        os << "Reconfigured." << endl;
    }
    else if (args["qcreate"]) {
        ctl = x_CreateNewClient(false);
        if (!args["qclass"] )
            NCBI_THROW(CArgException, eNoArg, "Missing required agrument: -qclass");
        if (!args["queue"] )
            NCBI_THROW(CArgException, eNoArg, "Missing required agrument: -queue");
        string comment;
        if (args["comment"]) {
            comment = args["comment"].AsString();
        }
        string new_queue = args["queue"].AsString();
        string qclass = args["qclass"].AsString();
        ctl.GetAdmin().CreateQueue(new_queue, qclass, comment);
        os << "Queue \"" << new_queue << "\" has been created." << endl;
    }
    else if (args["qdelete"]) {
        ctl = x_CreateNewClient(true);
        ctl.GetAdmin().DeleteQueue(ctl.GetQueueName());
        os << "Queue \"" << ctl.GetQueueName() << "\" has been deleted." << endl;
    }
    else if (args["showparams"]) {
        ctl = x_CreateNewClient(true);
        CNetScheduleAPI::SServerParams params = ctl.GetServerParams();
        os << "Server parameters for the queue \"" << ctl.GetQueueName()
           << "\":" << endl
           << "max_input_size = " << params.max_input_size << endl
           << "max_output_size = " << params.max_output_size << endl
           << "fast status is "
           << (params.fast_status? "supported" : "not supported") << endl;
    }

    else if (args["dump"]) {
        ctl = x_CreateNewClient(true);
        string jid;
        if (args["jid"] ) {
            jid = args["jid"].AsString();
            ctl.GetAdmin().DumpJob(os,jid);
        } else {
            ctl.GetAdmin().DumpQueue(os);
        }
    }
    else if (args["cancel"]) {
        ctl = x_CreateNewClient(true);
        string jid = args["cancel"].AsString();
        ctl.GetSubmitter().CancelJob(jid);
        os << "Job " << jid << " has been canceled." << endl;
    }
    else if (args["ver"]) {
        ctl = x_CreateNewClient(false);
        ctl.GetAdmin().PrintServerVersion(os);
    }
    else if (args["qlist"]) {
        ctl = x_CreateNewClient(false);

        CNetScheduleAdmin::TQueueList queues;

        ctl.GetAdmin().GetQueueList(queues);

        for (CNetScheduleAdmin::TQueueList::const_iterator it = queues.begin();
            it != queues.end(); ++it) {

            os << '[' << g_NetService_gethostname(it->server.GetHost()) <<
                ':' << NStr::UIntToString(it->server.GetPort()) << ']' <<
                std::endl;

            ITERATE(std::list<std::string>, itl, it->queues) {
                os << *itl << std::endl;
            }

            os << std::endl;
        }
    }
    else if (args["stat"]) {
        string sstatus = args["stat"].AsString();
        CNetScheduleAdmin::EStatisticsOptions st = CNetScheduleAdmin::eStatisticsBrief;
        if (NStr::CompareNocase(sstatus, "all") == 0)
            st = CNetScheduleAdmin::eStatisticsAll;
        ctl = x_CreateNewClient(true);
        ctl.GetAdmin().PrintServerStatistics(os, st);
    }
    else if (args["getconf"]) {
        ctl = x_CreateNewClient(true);
        ctl.GetAdmin().PrintConf(os);
    }
    else if (args["affstat"]) {
        string affinity = args["affstat"].AsString();
        ctl = x_CreateNewClient(true);

        os << "Queue: \"" << ctl.GetQueueName() << "\"" << endl;
        CNetScheduleAdmin::TStatusMap st_map;
        ctl.GetAdmin().StatusSnapshot(st_map, affinity);
        ITERATE(CNetScheduleAdmin::TStatusMap, it, st_map) {
            os << it->first << ": " << it->second << endl;
        }
    } else {
        NCBI_THROW(CArgException, eNoArg,
                   "Unknown command or command is not specified.");
    }

    return 0;
}

int main(int argc, const char* argv[])
{
    return CNetScheduleControl().AppMain(argc, argv);
}
