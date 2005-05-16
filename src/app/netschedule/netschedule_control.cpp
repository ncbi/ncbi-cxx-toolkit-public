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


/// NetSchedule control application
///
/// @internal
///
class CNetScheduleControl : public CNcbiApplication
{
public:
    void Init(void);
    int Run(void);
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

    arg_desc->AddOptionalKey(
        "dump",
        "dump_queue",
        "Print queue dump or job dump (queuename:job_key)",
        CArgDescriptions::eString);

    arg_desc->AddOptionalKey("monitor",
                             "monitor",
                             "Queue monitoring",
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

    if (args["dump"]) {  
        string param = args["dump"].AsString(); 
        string queue, job_key;
        
        NStr::SplitInTwo(param, ":", queue, job_key);

        CNetScheduleClient_Control cl(host, port, queue);
        cl.DumpQueue(NcbiCout, job_key);
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

    return 0;
}


int main(int argc, const char* argv[])
{
    return CNetScheduleControl().AppMain(argc, argv, 0, eDS_Default, 0);
}


/*
 * ===========================================================================
 * $Log$
 * Revision 1.13  2005/05/16 16:21:26  kuznets
 * Added available queues listing
 *
 * Revision 1.12  2005/05/12 18:37:33  kuznets
 * Implemented config reload
 *
 * Revision 1.11  2005/05/10 19:25:35  kuznets
 * Cosmetics
 *
 * Revision 1.10  2005/05/05 16:07:15  kuznets
 * Added individual job dumping
 *
 * Revision 1.9  2005/05/04 19:09:43  kuznets
 * Added queue dumping
 *
 * Revision 1.8  2005/05/02 16:37:41  kuznets
 * + -retry cmd arg
 *
 * Revision 1.7  2005/05/02 14:44:40  kuznets
 * Implemented remote monitoring
 *
 * Revision 1.6  2005/04/11 13:52:51  kuznets
 * Added logging control
 *
 * Revision 1.5  2005/03/22 19:02:54  kuznets
 * Reflecting chnages in connect layout
 *
 * Revision 1.4  2005/03/21 13:07:57  kuznets
 * Adde stat option
 *
 * Revision 1.3  2005/02/28 12:24:17  kuznets
 * New job status Returned, better error processing and queue management
 *
 * Revision 1.2  2005/02/22 17:47:02  kuznets
 * commented out unused variable
 *
 * Revision 1.1  2005/02/08 17:55:53  kuznets
 * Initial revision
 *
 *
 * ===========================================================================
 */
