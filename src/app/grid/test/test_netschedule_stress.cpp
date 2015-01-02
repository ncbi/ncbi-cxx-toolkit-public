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
 * File Description:  NetSchedule stress test (used for performance tuning)
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
#include <connect/services/grid_app_version_info.hpp>

#include <connect/ncbi_socket.hpp>
#include <connect/ncbi_core_cxx.hpp>
#include <connect/ncbi_types.h>

#define GRID_APP_NAME "test_netschedule_stress"

USING_NCBI_SCOPE;


///////////////////////////////////////////////////////////////////////

/// Test application
///
/// @internal
///
class CTestNetScheduleStress : public CNcbiApplication
{
public:
    void Init(void);
    int Run(void);
};



void CTestNetScheduleStress::Init(void)
{
    CONNECT_Init();
    SetDiagPostFlag(eDPF_Trace);
    SetDiagPostLevel(eDiag_Info);
    
    // Setup command line arguments and parameters

    // Create command-line argument descriptions class
    auto_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);

    // Specify USAGE context
    arg_desc->SetUsageContext(GetArguments().GetProgramBasename(),
                              "NetSchedule stress test prog='test 1.2'");
    
    arg_desc->AddKey("service", 
                     "service_name",
                     "NetSchedule service name (format: host:port or servcie_name).", 
                     CArgDescriptions::eString);

    arg_desc->AddKey("queue", 
                     "queue_name",
                     "NetSchedule queue name (like: noname).",
                     CArgDescriptions::eString);


    arg_desc->AddOptionalKey("jobs", 
                             "jobs",
                             "Number of jobs to submit",
                             CArgDescriptions::eInteger);

    arg_desc->AddDefaultKey("batch", "batch",
                            "Test batch submit",
                            CArgDescriptions::eBoolean,
                            "true",
                            0);

    // Setup arg.descriptions for this application
    SetupArgDescriptions(arg_desc.release());
}


void TestBatchSubmit(const string& service,
                     const string& queue_name, unsigned jcount)
{
    CNetScheduleAPI cl(service, "stress_test", queue_name);
    cl.SetProgramVersion("test 1.0.0");

    cl.SetClientNode("stress_test_node");
    cl.SetClientSession("stress_test_session");

    typedef vector<CNetScheduleJob> TJobs;
    TJobs jobs;

    for (unsigned i = 0; i < jcount; ++i) {
        CNetScheduleJob job("HELLO BSUBMIT", "affinity",
                            CNetScheduleAPI::eExclusiveJob);
        jobs.push_back(job);
    }
    
    {{
    NcbiCout << "Submit " << jobs.size() << " jobs..." << NcbiEndl;

    CNetScheduleSubmitter submitter = cl.GetSubmitter();
    CStopWatch sw(CStopWatch::eStart);
    submitter.SubmitJobBatch(jobs);

    NcbiCout << NcbiEndl << "Done." << NcbiEndl;
    double elapsed = sw.Elapsed();
    double rate = jobs.size() / elapsed;

    NcbiCout.setf(IOS_BASE::fixed, IOS_BASE::floatfield);
    NcbiCout << "Elapsed: "  << elapsed << " sec." << NcbiEndl;
    NcbiCout << "Rate: "     << rate    << " jobs/sec." << NcbiEndl;
    }}

    ITERATE(TJobs, it, jobs) {
        _ASSERT(!it->job_id.empty());
    }

    NcbiCout << "Get Jobs with permanent connection... " << NcbiEndl;

    // Fetch it back

    //    cl.SetCommunicationTimeout().sec = 20;

    {{
    unsigned cnt = 0;
    CNetScheduleJob job("INPUT");
    job.output = "DONE";

    CStopWatch sw(CStopWatch::eStart);
    CNetScheduleExecutor executor = cl.GetExecutor();

    for (;1;++cnt) {
        job.output = "DONE";
        job.ret_code = 0;
        if (!executor.GetJob(job))
            break;
        executor.PutResult(job);
    }
    NcbiCout << NcbiEndl << "Done." << NcbiEndl;
    double elapsed = sw.Elapsed();
    double rate = cnt / elapsed;

    NcbiCout.setf(IOS_BASE::fixed, IOS_BASE::floatfield);
    NcbiCout << "Jobs processed: " << cnt         << NcbiEndl;
    NcbiCout << "Elapsed: "        << elapsed     << " sec."      << NcbiEndl;
    NcbiCout << "Rate: "           << rate        << " jobs/sec." << NcbiEndl;
    }}
}


int CTestNetScheduleStress::Run(void)
{
    const CArgs& args = GetArgs();
    const string&  service  = args["service"].AsString();
    const string&  queue = args["queue"].AsString();
    bool batch = args["batch"].AsBoolean();

    unsigned jcount = 10000;
    if (args["jobs"]) {
        jcount = args["jobs"].AsInteger();
    }

    //    TestRunTimeout(host, port, queue_name);
//    TestNetwork(host, port, queue_name);

    if (batch) {
        TestBatchSubmit(service, queue, jcount);
        return 0;
    }

    CNetScheduleAPI::EJobStatus status;
    CNetScheduleAPI cl(service, "stress_test", queue);
    cl.SetProgramVersion("test wn 1.0.1");

    cl.GetAdmin().PrintServerVersion(NcbiCout);

    //        SleepSec(40);
    //        return 0;
    string input = "Hello " + queue;

    CNetScheduleSubmitter submitter = cl.GetSubmitter();
    CNetScheduleExecutor executor = cl.GetExecutor();

    CNetScheduleJob job(input);
    job.progress_msg = "pmsg";
    submitter.SubmitJob(job);
    NcbiCout << job.job_id << NcbiEndl;
    submitter.GetProgressMsg(job);
    _ASSERT(job.progress_msg == "pmsg");

    // test progress message
    job.progress_msg = "progress report message";
    executor.PutProgressMsg(job);

    string pmsg = job.progress_msg;
    job.progress_msg = "";
    submitter.GetProgressMsg(job);
    NcbiCout << pmsg << NcbiEndl;
    _ASSERT(pmsg == job.progress_msg);


    job.error_msg = "test error\r\nmessage";
    executor.PutFailure(job);
    status = cl.GetJobDetails(job);
    
    //    _ASSERT(status == CNetScheduleAPI::eFailed);
    //    NcbiCout << err_msg << NcbiEndl;
    //    _ASSERT(err_msg == err);

    //> ?????????? How should it really work??????????????
    if (status != CNetScheduleAPI::eFailed) {
        NcbiCerr << "Job " << job.job_id << " succeeded!" << NcbiEndl;
    } else {
        NcbiCout << job.error_msg << NcbiEndl;
        /*
        if (job.error_msg != err) {
            NcbiCerr << "Incorrect error message: " << job.error_msg << NcbiEndl;
        }
        */
    }
    //< ?????????? How should it really work??????????????

    submitter.CancelJob(job.job_id);
    status = executor.GetJobStatus(job.job_id);

    _ASSERT(status == CNetScheduleAPI::eCanceled);

    vector<string> jobs;
    jobs.reserve(jcount);

    {{
    CStopWatch sw(CStopWatch::eStart);

    NcbiCout << "Submit " << jcount << " jobs..." << NcbiEndl;

    for (unsigned i = 0; i < jcount; ++i) {
        CNetScheduleJob job(input);
        submitter.SubmitJob(job);
        jobs.push_back( job.job_id );
        if (i % 1000 == 0) {
            NcbiCout << "." << flush;
        }
    }
    NcbiCout << NcbiEndl << "Done." << NcbiEndl;
    double elapsed = sw.Elapsed();
    double avg = elapsed / jcount;

    NcbiCout.setf(IOS_BASE::fixed, IOS_BASE::floatfield);
    NcbiCout << "Elapsed: "  << elapsed << " sec." << NcbiEndl;
    NcbiCout << "Avg time: " << avg << " sec." << NcbiEndl;
    }}



    NcbiCout << NcbiEndl << "Waiting..." << NcbiEndl;
    SleepMilliSec(40 * 1000);
    NcbiCout << NcbiEndl << "Ok." << NcbiEndl;

    {{
    NcbiCout << NcbiEndl 
             << "GetStatus " << jobs.size() << " jobs..." << NcbiEndl;

    CStopWatch sw(CStopWatch::eStart);

    unsigned i = 0;
    NON_CONST_ITERATE(vector<string>, it, jobs) {
        const string& jk = *it;
        status = executor.GetJobStatus(jk);
        //status = cl.GetStatus(jk, &ret_code, &output);
        if (i++ % 1000 == 0) {
            NcbiCout << "." << flush;
        }
    }

    double elapsed = sw.Elapsed();
    if (jcount) {
        double avg = elapsed / (double)jcount;

        NcbiCout.setf(IOS_BASE::fixed, IOS_BASE::floatfield);
        NcbiCout << "Elapsed :"  << elapsed << " sec." << NcbiEndl;
        NcbiCout << "Avg time :" << avg << " sec." << NcbiEndl;
    }
    }}



    NcbiCout << NcbiEndl << "Waiting..." << NcbiEndl;
    SleepMilliSec(40 * 1000);
    NcbiCout << NcbiEndl << "Ok." << NcbiEndl;
    
    vector<string> jobs_returned;
    jobs_returned.reserve(jcount);

    {{
    NcbiCout << NcbiEndl << "Take-Return jobs..." << NcbiEndl;
    CStopWatch sw(CStopWatch::eStart);
    string input;

    unsigned cnt = 0;
    for (; cnt < jcount/2; ++cnt) {
        CNetScheduleJob job;
        if (!executor.GetJob(job, 60))
            break;
        executor.ReturnJob(job);
        jobs_returned.push_back(job.job_id);
    }
    NcbiCout << "Returned " << cnt << " jobs." << NcbiEndl;

    double elapsed = sw.Elapsed();
    if (cnt) {
        double avg = elapsed / cnt;

        NcbiCout.setf(IOS_BASE::fixed, IOS_BASE::floatfield);
        NcbiCout << "Jobs processed: " << cnt << NcbiEndl;
        NcbiCout << "Elapsed: "  << elapsed << " sec." << NcbiEndl;
        NcbiCout << "Avg time: " << avg << " sec." << NcbiEndl;
    }

    
    
    
    NcbiCout << NcbiEndl << "Waiting..." << NcbiEndl;
    SleepMilliSec(40 * 1000);
    NcbiCout << NcbiEndl << "Ok." << NcbiEndl;

    }}

    vector<string> jobs_processed;
    jobs_processed.reserve(jcount);

    {{
    NcbiCout << NcbiEndl << "Processing..." << NcbiEndl;
    SleepMilliSec(8 * 1000);
    CStopWatch sw(CStopWatch::eStart);
    
    unsigned cnt = 0;

    for (; 1; ++cnt) {
        CNetScheduleJob job;

        if (!executor.GetJob(job))
            break;

        jobs_processed.push_back(job.job_id);

        job.output = "DONE " + queue;
        executor.PutResult(job);
    }
    double elapsed = sw.Elapsed();

    if (cnt) {
        double avg = elapsed / cnt;

        NcbiCout.setf(IOS_BASE::fixed, IOS_BASE::floatfield);
        NcbiCout << "Jobs processed: " << cnt << NcbiEndl;
        NcbiCout << "Elapsed: "  << elapsed << " sec." << NcbiEndl;
        NcbiCout << "Avg time: " << avg << " sec." << NcbiEndl;
    }

    }}


    {{
    NcbiCout << NcbiEndl << "Check returned jobs..." << NcbiEndl;
    SleepMilliSec(5 * 1000);
    CStopWatch sw(CStopWatch::eStart);

    NON_CONST_ITERATE(vector<string>, it, jobs_returned) {
        const string& jk = *it;
        status = submitter.GetJobStatus(jk);
        switch (status) {
        case CNetScheduleAPI::ePending:
            NcbiCout << "Job pending: " << jk << NcbiEndl;
            break;
        default:
            break;
        }
    }

    }}

    NcbiCout << NcbiEndl << "Done." << NcbiEndl;   
    return 0;
}


int main(int argc, const char* argv[])
{
    GRID_APP_CHECK_VERSION_ARGS();

    return CTestNetScheduleStress().AppMain(argc, argv, 0, eDS_Default);
}
