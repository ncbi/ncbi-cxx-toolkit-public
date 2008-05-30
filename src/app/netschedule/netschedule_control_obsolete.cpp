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

#include <connect/services/netschedule_client.hpp>
#include <connect/ncbi_socket.hpp>

#if defined(NCBI_OS_UNIX) && defined(HAVE_LIBCONNEXT)
#include <grp.h>
#include <sys/types.h>
#include <unistd.h>
#endif

#include "client_admin.hpp"

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
    bool CheckPermission();
};



void CNetScheduleControl::Init(void)
{
    auto_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);

    arg_desc->SetUsageContext(GetArguments().GetProgramBasename(),
                              "NCBI NetSchedule control.");

    arg_desc->AddPositional("hostname",
                            "NetSchedule host name.", CArgDescriptions::eString);

    arg_desc->AddPositional("port",
                            "Port number.",
                            CArgDescriptions::eInteger);

    arg_desc->AddFlag("s", "Shutdown server");
    arg_desc->AddFlag("die", "Shutdown server");
    arg_desc->AddFlag("v", "Server version");
    arg_desc->AddFlag("reconf", "Reload server configuration");
    arg_desc->AddFlag("qlist", "List available queues");

    arg_desc->AddOptionalKey("log",
                             "server_logging",
                             "Switch server side logging",
                             CArgDescriptions::eBoolean);

    arg_desc->AddOptionalKey("drop",
                             "drop_queue",
                             "Drop ALL jobs in the queue (no questions asked!)",
                             CArgDescriptions::eString);

    arg_desc->AddOptionalKey("stat",
                             "stat_queue",
                             "Print queue statistics",
                             CArgDescriptions::eString);

    arg_desc->AddOptionalKey("affstat",
                             "aff_stat_queue",
                             "Print queue statistics summary based on affinity(queue:affinity)",
                             CArgDescriptions::eString);

    arg_desc->AddOptionalKey(
        "dump",
        "dump_queue",
        "Print queue dump or job dump (queuename:job_key)",
        CArgDescriptions::eString);

    arg_desc->AddOptionalKey(
        "qprint",
        "print_queue",
        "Print queue content for the specified status (queuename:status)",
        CArgDescriptions::eString);


    arg_desc->AddOptionalKey("monitor",
                             "monitor",
                             "Queue monitoring (-monitor queue_name)",
                             CArgDescriptions::eString);

    arg_desc->AddOptionalKey("retry",
                             "retry",
                             "Number of re-try attempts if connection failed",
                             CArgDescriptions::eInteger);


    SetupArgDescriptions(arg_desc.release());
}



int CNetScheduleControl::Run(void)
{
    CArgs args = GetArgs();
    const string& host  = args["hostname"].AsString();
    unsigned short port = args["port"].AsInteger();


    CNetScheduleClient_Control nc_client(host, port);

    if (args["retry"]) {
        int retry = args["retry"].AsInteger();
        if (retry < 0) {
            ERR_POST(Error << "Invalid retry count: " << retry);
        }
        for (int i = 0; i < retry; ++i) {
            try {
                nc_client.CheckConnect(kEmptyStr);
                break;
            }
            catch (exception&) {
                SleepMilliSec(5 * 1000);
            }
        }
    }

    if (args["log"]) {  // logging control

        bool on_off = args["log"].AsBoolean();

        nc_client.Logging(on_off);
        NcbiCout << "Logging turned "
                 << (on_off ? "ON" : "OFF") << " on the server" << NcbiEndl;
    }
    if (args["drop"]) {
        string queue = args["drop"].AsString();
        CNetScheduleClient_Control drop_client(host, port, queue);
        drop_client.DropQueue();
        NcbiCout << "Queue droppped: " << queue << NcbiEndl;
    }

    if (args["stat"]) {
        string queue = args["stat"].AsString();
        CNetScheduleClient_Control cl(host, port, queue);
        cl.PrintStatistics(NcbiCout);
        NcbiCout << NcbiEndl;
    }

    if (args["affstat"]) {
        string queue_aff = args["affstat"].AsString();
        string queue, aff;
        NStr::SplitInTwo(queue_aff, ":", queue, aff);
        if (queue.empty()) {
            queue = queue_aff;
        }
        CNetScheduleClient_Control cl(host, port, queue);

        CNetScheduleClient::TStatusMap st_map;
        cl.StatusSnapshot(&st_map, aff);
        ITERATE(CNetScheduleClient::TStatusMap, it, st_map) {
            NcbiCout << CNetScheduleClient::StatusToString(it->first) << ": "
                     << it->second
                     << NcbiEndl;
        }
    }

    if (args["dump"]) {
        string param = args["dump"].AsString();
        string queue, job_key;

        NStr::SplitInTwo(param, ":", queue, job_key);

        CNetScheduleClient_Control cl(host, port, queue);
        cl.DumpQueue(NcbiCout, job_key);
        NcbiCout << NcbiEndl;
    }

    if (args["qprint"]) {
        string param = args["qprint"].AsString();
        string queue, status_str;

        NStr::SplitInTwo(param, ":", queue, status_str);

        CNetScheduleClient::EJobStatus status =
            CNetScheduleClient::StringToStatus(status_str);
        if (status == CNetScheduleClient::eJobNotFound) {
            ERR_POST("Status string unknown:" << status_str);
            return 1;
        }

        CNetScheduleClient_Control cl(host, port, queue);
        cl.PrintQueue(NcbiCout, status);
        NcbiCout << NcbiEndl;
    }

    if (args["qlist"]) {
        CNetScheduleClient_Control cl(host, port, "noname");
        string ql = nc_client.GetQueueList();
        NcbiCout << "Queues: " << ql << NcbiEndl;
    }

    if (args["reconf"]) {
        CNetScheduleClient_Control cl(host, port);
        cl.ReloadServerConfig();
    }


    if (args["monitor"]) {
        string queue = args["monitor"].AsString();
        CNetScheduleClient_Control cl(host, port, queue);
        cl.Monitor(NcbiCout);
    }

    if (args["s"]) {  // shutdown
        if( !CheckPermission() ) {
            NcbiCout << "Could not shutdown the service: Permission denied" << NcbiEndl;
            return 1;
        }
        nc_client.ShutdownServer();
        NcbiCout << "Shutdown request has been sent to server" << NcbiEndl;
        return 0;
    }
    if (args["die"]) {  // shutdown
        if( !CheckPermission() ) {
            NcbiCout << "Could not shutdown the service: Permission denied" << NcbiEndl;
            return 1;
        }
        nc_client.ShutdownServer(CNetScheduleClient::eDie);
        NcbiCout << "Die request has been sent to server" << NcbiEndl;
        return 0;
    }

    if (args["v"]) {  // version
        string version = nc_client.ServerVersion();
        if (version.empty()) {
            NcbiCout << "NetCache server communication error." << NcbiEndl;
            return 1;
        }
        NcbiCout << version << NcbiEndl;
    }

    return 0;
}

bool CNetScheduleControl::CheckPermission()
{
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
    return true;
}

int main(int argc, const char* argv[])
{
    return CNetScheduleControl().AppMain(argc, argv, 0, eDS_Default, 0);
}
