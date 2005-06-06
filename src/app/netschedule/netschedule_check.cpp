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
 * File Description:  NetSchedule check utility
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

/// NetSchedule check application
///
/// @internal
///
class CNetScheduleCheck : public CNcbiApplication
{
public:
    void Init(void);
    int Run(void);
    void Run(CNetScheduleClient& nc_client);
};



void CNetScheduleCheck::Init(void)
{
    auto_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);

    arg_desc->SetUsageContext(GetArguments().GetProgramBasename(),
                              "NCBI NetSchedule Check.");
    
    arg_desc->AddPositional("host_service", 
        "NetSchedule host or service name. Format: service|host:port", 
        CArgDescriptions::eString);

    arg_desc->AddOptionalKey("q",
                             "queue",
                             "Queue name",
                             CArgDescriptions::eString);


    SetupArgDescriptions(arg_desc.release());
}


int CNetScheduleCheck::Run(void)
{
    CArgs args = GetArgs();

    const string& host_service  = args["host_service"].AsString();

    string queue = "noname";
    if (args["q"]) {  
        queue = args["q"].AsString(); 
    }


    unsigned short port = 0;
    string host, port_str;
    bool b = NStr::SplitInTwo(host_service, ":", host, port_str);
    if (b) {
        port = atoi(port_str.c_str());

        CNetScheduleClient nc(host, port, "netschedule_control", queue);
        Run(nc);

    } else {  // LB name
        CNetScheduleClient_LB nc("netschedule_admin", 
                                  host_service, "noname");
        Run(nc);
    }

    return 0;
}

void CNetScheduleCheck::Run(CNetScheduleClient& nc)
{
    CArgs args = GetArgs();

    const string input = "Hello ";
    string job_key = nc.SubmitJob(input);

    SleepMilliSec(2 * 1000);

    string input_str;
    for (int retry = 0; retry < 10; ++retry) {
        bool job_exists = nc.GetJob(&job_key, &input_str);
        if (job_exists) {
            nc.PutResult(job_key, 0, "DONE");
        }
    }
}


int main(int argc, const char* argv[])
{
    return CNetScheduleCheck().AppMain(argc, argv, 0, eDS_Default, 0);
}


/*
 * ===========================================================================
 * $Log$
 * Revision 1.1  2005/06/06 13:07:21  kuznets
 * +netschedule_check
 *
 *
 * ===========================================================================
 */
