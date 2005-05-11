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
    using CNetScheduleClient_LB::GetServiceList;
};




/// NetSchedule admin application
///
/// @internal
///
class CNetScheduleAdmin : public CNcbiApplication
{
public:
    void Init(void);
    int Run(void);
    void Run(CNetScheduleClient_Control& nc_client);
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

    arg_desc->AddOptionalKey("q",
                             "queue",
                             "Queue name",
                             CArgDescriptions::eString);

    arg_desc->AddOptionalKey("k",
                             "key",
                             "job key",
                             CArgDescriptions::eString);


    arg_desc->AddOptionalKey("log",
                             "server_logging",
                             "Switch server side logging",
                             CArgDescriptions::eBoolean);

    arg_desc->AddFlag("drop", "Drop Queue");

    arg_desc->AddFlag("stat", "stat queue");

    arg_desc->AddFlag("dump", "Queue dump");

    SetupArgDescriptions(arg_desc.release());
}


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

        CNetScheduleClient_LB_Admin::TServiceList::const_iterator it =
            lst.begin();

        ITERATE(CNetScheduleClient_LB_Admin::TServiceList, it, lst) {
            
            string conn_host = CSocketAPI::gethostbyaddr(it->host);

            string queue = "noname";
            if (args["q"]) {  
                queue = args["q"].AsString(); 
            }

            NcbiCout << "Connecting to: " << conn_host 
                     << ":" << it->port << " Queue=" << queue << NcbiEndl;

            CNetScheduleClient_Control nc_control(conn_host, it->port, queue);

            Run(nc_control);

            NcbiCout << NcbiEndl;
        }
    }

    return 0;
}

void CNetScheduleAdmin::Run(CNetScheduleClient_Control& nc_client)
{
    CArgs args = GetArgs();

    if (args["log"]) {  // logging control
        bool on_off = args["log"].AsBoolean();
        nc_client.Logging(on_off);
        NcbiCout << "Logging turned "
                 << (on_off ? "ON" : "OFF") << NcbiEndl;
    }

    if (args["drop"]) {  
        nc_client.DropQueue();
    }

    if (args["stat"]) {
        nc_client.PrintStatistics(NcbiCout);
    }

    if (args["dump"]) {

        string job_key;
        if (args["k"]) {
            job_key = args["k"].AsString();
        }
        
        nc_client.DumpQueue(NcbiCout, job_key);
    }

    if (args["v"]) {
        string version = nc_client.ServerVersion();
        if (version.empty()) {
            NcbiCout << "NetCache server communication error." << NcbiEndl;
        }
        NcbiCout << version << NcbiEndl;
    }

}


int main(int argc, const char* argv[])
{
    return CNetScheduleAdmin().AppMain(argc, argv, 0, eDS_Default, 0);
}


/*
 * ===========================================================================
 * $Log$
 * Revision 1.2  2005/05/11 13:01:52  kuznets
 * Implemented variety of logging commands
 *
 * Revision 1.1  2005/05/10 19:24:48  kuznets
 * Initial revision
 *
 *
 *
 * ===========================================================================
 */
