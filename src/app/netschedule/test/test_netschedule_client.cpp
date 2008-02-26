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
                            "NetSchedule lb_service or host:name.", 
                            CArgDescriptions::eString);

    arg_desc->AddPositional("queue", 
                            "NetSchedule queue name (like: noname).",
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

    const string input = "Hello " + queue_name;

    CNetScheduleJob job(input);
    submitter.SubmitJob(job);
    NcbiCout << job.job_id << NcbiEndl;

    vector<string> jobs;

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
    NcbiCout << "Submit " << jcount << " jobs..." << NcbiEndl;

    for (unsigned i = 0; i < jcount; ++i) {
        CNetScheduleJob j1(input);
        submitter.SubmitJob(j1);
        jobs.push_back(j1.job_id);
        if (i % 1000 == 0) {
            NcbiCout << "." << flush;
        }
    }
    NcbiCout << NcbiEndl << "Done." << NcbiEndl;
    double elapsed = sw.Elapsed();
    double avg = elapsed / jcount;

    NcbiCout.setf(IOS_BASE::fixed, IOS_BASE::floatfield);
    NcbiCout << "Avg time: " << (elapsed / jcount) << " sec, "
             << (jcount/elapsed) << " jobs/sec" << NcbiEndl;
    }}


    // Waiting for jobs to be done

    NcbiCout << "Waiting for jobs..." << jobs.size() << NcbiEndl;
    unsigned cnt = 0;
    SleepMilliSec(5500);
    
    unsigned last_jobs = 0;
    unsigned no_jobs_executes_cnt = 0;

    while (jobs.size()) {
        NON_CONST_ITERATE(vector<string>, it, jobs) {
            const string& jk = *it;
            status = submitter.GetJobStatus(jk);

            if (status == CNetScheduleAPI::eDone) {
                CNetScheduleJob job;
                job.job_id = *it;
                status = submitter.GetJobDetails(job);
                int last_char_index = job.output.length() - 1;
                if (last_char_index < 0 ||
                    memcmp(job.output.c_str(), output_buffer,
                    last_char_index) != 0 ||
                    job.output[last_char_index] != '\n') {
                    ERR_POST("Unexpected job output:\n" + job.output +
                        "\nVS\n" + string(output_buffer, job.output.length() - 1) +
                        "\n");
                }
                if (job.ret_code != 0) {
                    ERR_POST("Unexpected job return code:" << job.ret_code);
                }
                jobs.erase(it);
                ++cnt;
                break;
            } else 
            if (status != CNetScheduleAPI::ePending) {
                if (status == CNetScheduleAPI::eJobNotFound) {
                    NcbiCerr << "Job lost:" << jk << NcbiEndl;
                }
                jobs.erase(it);
                ++cnt;
                break;
            }

            ++cnt;
            if (cnt % 1000 == 0) {
                NcbiCout << "Waiting for " 
                         << jobs.size() 
                         << " jobs."
                         << NcbiEndl;
                // it is necessary to give system a rest periodically
                SleepMilliSec(2000);
                // check status of only first 1000 jobs
                // since the JS queue execution priority is FIFO
                break;
            }
        }
        
        // check if worker node picks up jobs, otherwise stop
        // trying after 10 attempts.        
        
        if (jobs.size() == last_jobs) {
            ++no_jobs_executes_cnt;
            if (no_jobs_executes_cnt == 3) {
                NcbiCout << "No progress in job execution. Stopping..."
                         << NcbiEndl;
                break;
            } else {
                last_jobs = jobs.size();
            }
        }
        
    } // while

    NcbiCout << NcbiEndl << "Done." << NcbiEndl;
    if (jobs.size()) {
        NcbiCout << "Unfinished job count = " << jobs.size() << NcbiEndl;
    }
    return 0;
}


int main(int argc, const char* argv[])
{
    return CTestNetScheduleClient().AppMain(argc, argv, 0, eDS_Default, 0);
}
