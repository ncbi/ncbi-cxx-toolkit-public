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

#include "client_admin.hpp"

USING_NCBI_SCOPE;


/// NetSchedule admin application
///
/// @internal
///
class CNetScheduleAdmin : public CNcbiApplication
{
public:
    void Init(void);
    int Run(void);
};



void CNetScheduleAdmin::Init(void)
{
    auto_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);

    arg_desc->SetUsageContext(GetArguments().GetProgramBasename(),
                              "NCBI NetSchedule Admin.");
    
    arg_desc->AddPositional("host_service", 
        "NetSchedule host or service name. Format: service|host:port", 
        CArgDescriptions::eString);

    arg_desc->AddFlag("v", "Server version");

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

    arg_desc->AddOptionalKey(
        "dump",
        "dump_queue",
        "Print queue dump or job dump (queuename:job_key)",
        CArgDescriptions::eString);


    SetupArgDescriptions(arg_desc.release());
}



class CNetScheduleClient_LB_Admin : public CNetScheduleClient_LB
{
public:
    CNetScheduleClient_LB_Admin(const string& client_name,
                                const string& lb_service_name,
                                const string& queue_name)
    : CNetScheduleClient_LB(client_name, 
                            lb_service_name, 
                            queue_name)
    {}

    using CNetScheduleClient_LB::ObtainServerList;
    using CNetScheduleClient_LB::TServiceList;
    using CNetScheduleClient_LB::GetServiceList;
};



int CNetScheduleAdmin::Run(void)
{
    CArgs args = GetArgs();

    const string& host_service  = args["host_service"].AsString();

    unsigned short port = 0;
    string host, port_str;
    bool b = NStr::SplitInTwo(host_service, ":", host, port_str);
    if (b) {
        port = atoi(port_str.c_str());
    } else {  // LB name
        CNetScheduleClient_LB_Admin cli_lb("netschedule_admin", 
                                           host_service, "noname");
        cli_lb.DiscoverLowPriorityServers(true);
        cli_lb.ObtainServerList(host_service);

        const CNetScheduleClient_LB_Admin::TServiceList& lst = 
            cli_lb.GetServiceList();
    }



/*
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

    if (args["dump"]) {  
        string param = args["dump"].AsString(); 
        string queue, job_key;
        
        NStr::SplitInTwo(param, ":", queue, job_key);

        CNetScheduleClient_Control cl(host, port, queue);
        cl.DumpQueue(NcbiCout, job_key);
        NcbiCout << NcbiEndl;
    }


    if (args["monitor"]) {
        string queue = args["monitor"].AsString(); 
        CNetScheduleClient_Control cl(host, port, queue);
        cl.Monitor(NcbiCout);
    }

    if (args["s"]) {  // shutdown
        nc_client.ShutdownServer();
        NcbiCout << "Shutdown request has been sent to server" << NcbiEndl;
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
*/

    return 0;
}


int main(int argc, const char* argv[])
{
    return CNetScheduleAdmin().AppMain(argc, argv, 0, eDS_Default, 0);
}


/*
 * ===========================================================================
 * $Log$
 * Revision 1.1  2005/05/10 19:24:48  kuznets
 * Initial revision
 *
 *
 *
 * ===========================================================================
 */
