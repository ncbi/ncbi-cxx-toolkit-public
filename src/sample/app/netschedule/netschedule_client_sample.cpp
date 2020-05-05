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
#include <connect/ncbi_socket.hpp>
#include <connect/ncbi_core_cxx.hpp>
#include <connect/ncbi_types.h>


USING_NCBI_SCOPE;

    
///////////////////////////////////////////////////////////////////////
/// Sample application
///
/// @internal
///

class CSampleNetScheduleClient : public CNcbiApplication
{
public:
    virtual void Init(void);
    virtual int  Run(void);
};


void CSampleNetScheduleClient::Init(void)
{
    SetDiagPostFlag(eDPF_Trace);
    SetDiagPostLevel(eDiag_Info);
    
    // Setup command line arguments and parameters

    // Create command-line argument descriptions class
    unique_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);

    // Specify USAGE context
    arg_desc->SetUsageContext(GetArguments().GetProgramBasename(),
                              "NetSchedule client");
    
    arg_desc->AddPositional("service", 
                            "NetSchedule service name", 
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


int CSampleNetScheduleClient::Run(void)
{
    const CArgs&   args = GetArgs();
    const string&  service = args["service"].AsString();
    const string&  queue_name = args["queue"].AsString();  

    unsigned jcount = 100;
    if (args["jcount"]) {
        jcount = args["jcount"].AsInteger();
    }
    CNetScheduleAPI::EJobStatus status;
    CNetScheduleAPI cl(service, "client_sample", queue_name);

    CNetScheduleSubmitter submitter = cl.GetSubmitter();

    const string input = "Hello " + queue_name;

    CNetScheduleJob job(input);
    submitter.SubmitJob(job);
    cout << job.job_id << endl;

    vector<string> jobs;
    {{
        CStopWatch sw(CStopWatch::eStart);

        cout << "Submit " << jcount << " jobs..." << endl;

        for (unsigned i = 0; i < jcount; ++i) {
            CNetScheduleJob job(input);
            submitter.SubmitJob(job);
            jobs.push_back(job.job_id);
            if (i % 1000 == 0) {
                cout << "." << flush;
            }
        }
        double elapsed = sw.Elapsed();
        cout << endl << "Done." << endl;
        double avg = elapsed / jcount;
        cout.setf(IOS_BASE::fixed, IOS_BASE::floatfield);
        cout << "Avg time:" << avg << " sec." << endl;
    }}

    // Waiting for jobs to be done

    cout << "Waiting for jobs..." << jobs.size() << endl;
    unsigned cnt = 0;
    SleepMilliSec(5000);

    CNetScheduleAdmin admin = cl.GetAdmin();
    /*
    CNetScheduleKeys keys;
    admin.RetrieveKeys("status=pending", keys);

    for (CNetScheduleKeys::const_iterator it = keys.begin();
        it != keys.end(); ++it) {
        cout << string(*it) << endl;
    }
    */
            
    size_t last_jobs = 0;
    size_t no_jobs_executes_cnt = 0;
    
    while (jobs.size()) {
        for (auto it = jobs.begin(); it != jobs.end(); ++it) {
            CNetScheduleJob job;
            job.job_id = *it;
            status = submitter.GetJobDetails(job);

            if (status == CNetScheduleAPI::eDone) {
                string expected_output = "DONE " + queue_name;
                if (job.output != expected_output || job.ret_code != 0) {
                    ERR_POST("Unexpected output or return code:" + job.output);
                }
                jobs.erase(it);
                ++cnt;
                break;
            } else 
            if (status != CNetScheduleAPI::ePending) {
                if (status == CNetScheduleAPI::eJobNotFound) {
                    cerr << "Job lost:" << job.job_id << endl;
                }
                jobs.erase(it);
                ++cnt;
                break;                
            }
            
            ++cnt;
            if (cnt % 1000 == 0) {
                cout << "Waiting for " << jobs.size() << " jobs." << endl;
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
                cout << "No progress in job execution. Stopping..." << endl;
                break;
            } else {
                last_jobs = jobs.size();
            }
        }
        
    } // while

    cout << endl << "Done." << endl;
    if (jobs.size()) {
        cout << "Remaining job count = " << jobs.size() << endl;
    }
    return 0;
}


int NcbiSys_main(int argc, ncbi::TXChar* argv[])
{
    return CSampleNetScheduleClient().AppMain(argc, argv);
}
