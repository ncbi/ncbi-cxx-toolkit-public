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
 * Authors:  Anatoliy Kuznetsov, Dmitry Kazimirov
 *
 * File Description:  NetSchedule client test
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbiargs.hpp>
#include <corelib/ncbienv.hpp>
#include <corelib/ncbireg.hpp>
#include <corelib/ncbi_system.hpp>
#include <corelib/ncbimisc.hpp>

#include <connect/services/netschedule_api.hpp>
#include <connect/ncbi_types.h>

#include "test_netschedule_data.hpp"


USING_NCBI_SCOPE;


///////////////////////////////////////////////////////////////////////


/// Test application
///
/// @internal
///
class CTestNetScheduleClient : public CNcbiApplication
{
public:
    void Init(void);
    int Run(void);
};



void CTestNetScheduleClient::Init(void)
{
    InitOutputBuffer();

    CONNECT_Init();
    SetDiagPostFlag(eDPF_Trace);
    SetDiagPostLevel(eDiag_Info);
    
    // Setup command line arguments and parameters

    // Create command-line argument descriptions class
    auto_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);

    // Specify USAGE context
    arg_desc->SetUsageContext(GetArguments().GetProgramBasename(),
                              "NetSchedule client");
    
    arg_desc->AddPositional("service", 
                            "NetSchedule lb_service or host:name",
                            CArgDescriptions::eString);

    arg_desc->AddOptionalKey("input",
                             "input",
                             "Input string",
                             CArgDescriptions::eString);

    arg_desc->AddPositional("queue", 
                            "NetSchedule queue name",
                            CArgDescriptions::eString);


    arg_desc->AddOptionalKey("jcount", 
                             "jcount",
                             "Number of jobs to submit",
                             CArgDescriptions::eInteger);


    
    // Setup arg.descriptions for this application
    SetupArgDescriptions(arg_desc.release());
}


int CTestNetScheduleClient::Run(void)
{
    CArgs args = GetArgs();
    const string&  service  = args["service"].AsString();
    const string&  queue_name = args["queue"].AsString();  

    unsigned jcount = 10000;
    if (args["jcount"]) {
        jcount = args["jcount"].AsInteger();
    }
    CNetScheduleAPI::EJobStatus status;
    CNetScheduleAPI cl(service, "client_test", queue_name);
    cl.SetConnMode(CNetScheduleAPI::eKeepConnection);

    CNetScheduleSubmitter submitter = cl.GetSubmitter();
    CNetScheduleAdmin admin = cl.GetAdmin();

    string input;
    if (args["input"]) {
        input = args["input"].AsString();
    } else {
        input = "Hello " + queue_name;
    }

    /*
    CNetScheduleJob job(input);
    submitter.SubmitJob(job);
    NcbiCout << job.job_id << NcbiEndl;

    vector<string> jobs;
    */

    if (0)
    {{

    NcbiCout << "SubmitAndWait..." << NcbiEndl;
    unsigned wait_time = 30;
    CNetScheduleJob j1(input);
    status = submitter.SubmitJobAndWait(j1, wait_time, 9112);
    if (status == CNetScheduleAPI::eDone) {
        NcbiCout << j1.job_id << " done." << NcbiEndl;
    } else {
        NcbiCout << j1.job_id << " is not done in " 
                 << wait_time << " seconds." << NcbiEndl;
    }
    NcbiCout << "SubmitAndWait...done." << NcbiEndl;
    }}


    {{
    CStopWatch sw(CStopWatch::eStart);
    NcbiCout << "Batch submit " << jcount << " jobs..." << NcbiEndl;

    for (unsigned i = 0; i < jcount; i += 1000) {
        vector<CNetScheduleJob> jobs;
        unsigned batch_size = jcount - i < 1000 ? jcount - i : 1000;
        for (unsigned j = 0; j < batch_size; ++j) {
            CNetScheduleJob job(input);
            job.tags.push_back(CNetScheduleAPI::TJobTag("test", ""));
            jobs.push_back(job);
        }
        submitter.SubmitJobBatch(jobs);
        NcbiCout << "." << flush;
    }
    NcbiCout << NcbiEndl << "Done." << NcbiEndl;
    double elapsed = sw.Elapsed();

    NcbiCout.setf(IOS_BASE::fixed, IOS_BASE::floatfield);
    NcbiCout << "Avg time: " << (elapsed / jcount) << " sec, "
             << (jcount/elapsed) << " jobs/sec" << NcbiEndl;
    }}


    // Waiting for jobs to be done

    NcbiCout << "Waiting for " << jcount << " jobs..." << NcbiEndl;

    unsigned long job_cnt;
    while ((job_cnt = admin.Count("status=Pending")) != 0) {
        NcbiCout << job_cnt << " jobs to go" << NcbiEndl;
        SleepMilliSec(2000);
    }

    return 0;
}


int main(int argc, const char* argv[])
{
    return CTestNetScheduleClient().AppMain(argc, argv, 0, eDS_Default, 0);
}
