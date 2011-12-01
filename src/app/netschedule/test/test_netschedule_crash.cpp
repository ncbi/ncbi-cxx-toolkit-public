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
 * File Description:  NetSchedule crash test. Used to run many instances of it
 *                    to simulate load for the netscheduled daemon.
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
#include <connect/services/netschedule_key.hpp>
#include <connect/ncbi_socket.hpp>
#include <connect/ncbi_core_cxx.hpp>
#include <connect/ncbi_types.h>


USING_NCBI_SCOPE;


///////////////////////////////////////////////////////////////////////

#define WORKER_NODE_PORT 9595

/// Test application
///
/// @internal
///
class CTestNetScheduleCrash : public CNcbiApplication
{
public:
    void Init(void);
    int Run(void);

    void GetStatus( CNetScheduleExecuter &        executor,
                    const vector<unsigned int> &  jobs );
    vector<unsigned int>  GetReturn( CNetScheduleExecuter &  executor,
                                     unsigned int            count );
    vector<unsigned int>  Submit( CNetScheduleSubmitter &  submitter,
                                  unsigned int             jcount,
                                  const string &           queue );
    vector<unsigned int>  GetDone( CNetScheduleExecuter &  executor );
    void  MainLoop( CNetScheduleSubmitter &  submitter,
                    CNetScheduleExecuter &   executor,
                    unsigned int             jcount,
                    const string &           queue);

    CTestNetScheduleCrash() :
        m_KeyGenerator(NULL)
    {}

    ~CTestNetScheduleCrash()
    {
        if (m_KeyGenerator)
            delete m_KeyGenerator;
    }
private:
    CNetScheduleKeyGenerator *  m_KeyGenerator;
};



void CTestNetScheduleCrash::Init(void)
{
    CONNECT_Init();
    SetDiagPostFlag(eDPF_Trace);
    SetDiagPostLevel(eDiag_Info);

    // Setup command line arguments and parameters

    // Create command-line argument descriptions class
    auto_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);

    // Specify USAGE context
    arg_desc->SetUsageContext(GetArguments().GetProgramBasename(),
                              "NetSchedule crash test prog='test 1.0'");

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

    arg_desc->AddOptionalKey("main",
                             "main",
                             "Main loop (submit-get-put) only",
                             CArgDescriptions::eBoolean);

    // Setup arg.descriptions for this application
    SetupArgDescriptions(arg_desc.release());
}



vector<unsigned int>  CTestNetScheduleCrash::Submit( CNetScheduleSubmitter &  submitter,
                                                     unsigned int             jcount,
                                                     const string &           queue )
{
    string                  input = "Crash test for " + queue;
    vector<unsigned int>    jobs;

    jobs.reserve(jcount);
    NcbiCout << "Submit " << jcount << " jobs..." << NcbiEndl;

    CStopWatch      sw(CStopWatch::eStart);
    for (unsigned i = 0; i < jcount; ++i) {
        CNetScheduleJob         job(input);
        submitter.SubmitJob(job);

        CNetScheduleKey     key( job.job_id );
        jobs.push_back( key.id );
        if (i % 1000 == 0) {
            NcbiCout << "." << flush;
        }

        if (m_KeyGenerator == NULL)
            m_KeyGenerator = new CNetScheduleKeyGenerator( key.host, key.port );
    }
    double          elapsed = sw.Elapsed();
    double          avg = elapsed / jcount;

    NcbiCout << NcbiEndl << "Done." << NcbiEndl;

    NcbiCout.setf(IOS_BASE::fixed, IOS_BASE::floatfield);
    NcbiCout << "Elapsed: "  << elapsed << " sec." << NcbiEndl;
    NcbiCout << "Avg time: " << avg << " sec." << NcbiEndl;

    return jobs;
}


void CTestNetScheduleCrash::GetStatus( CNetScheduleExecuter &        executor,
                                       const vector<unsigned int> &  jobs )
{
    NcbiCout << NcbiEndl
             << "Get status of " << jobs.size() << " jobs..." << NcbiEndl;

    //CNetScheduleAPI::EJobStatus         status;
    unsigned                            i = 0;
    CStopWatch                          sw(CStopWatch::eStart);

    ITERATE(vector<unsigned int>, it, jobs) {
        unsigned int        job_id = *it;

        //status = 
        executor.GetJobStatus(m_KeyGenerator->GenerateV1(job_id));
        if (i++ % 1000 == 0) {
            NcbiCout << "." << flush;
        }
    }
    double      elapsed = sw.Elapsed();
    double      avg = elapsed / (double)jobs.size();

    NcbiCout.setf(IOS_BASE::fixed, IOS_BASE::floatfield);
    NcbiCout << "Elapsed :"  << elapsed << " sec." << NcbiEndl;
    NcbiCout << "Avg time :" << avg << " sec." << NcbiEndl;
    return;
}


/* Returns up to count jobs */
vector<unsigned int>  CTestNetScheduleCrash::GetReturn( CNetScheduleExecuter &  executor,
                                                        unsigned int            count )
{
    NcbiCout << NcbiEndl << "Take and Return " << count << " jobs..." << NcbiEndl;

    vector<unsigned int>    jobs_returned;
    jobs_returned.reserve(count);

    unsigned        cnt = 0;
    CStopWatch      sw(CStopWatch::eStart);

    for (; cnt < count; ++cnt) {
        CNetScheduleJob     job;
        bool                job_exists = executor.GetJob(job);

        if (!job_exists)
            continue;

        CNetScheduleKey     key( job.job_id );
        jobs_returned.push_back(key.id);
    }
    ITERATE(vector<unsigned int>, it, jobs_returned) {
        unsigned int        job_id = *it;

        executor.ReturnJob(m_KeyGenerator->GenerateV1(job_id));
    }
    double          elapsed = sw.Elapsed();

    if (cnt > 0) {
        double          avg = elapsed / cnt;

        NcbiCout << "Returned " << cnt << " jobs." << NcbiEndl;

        NcbiCout.setf(IOS_BASE::fixed, IOS_BASE::floatfield);
        NcbiCout << "Jobs processed: " << cnt << NcbiEndl;
        NcbiCout << "Elapsed: "  << elapsed << " sec." << NcbiEndl;
        NcbiCout << "Avg time: " << avg << " sec." << NcbiEndl;
    }
    else
        NcbiCout << "Returned 0 jobs." << NcbiEndl;

    return jobs_returned;
}


vector<unsigned int>  CTestNetScheduleCrash::GetDone( CNetScheduleExecuter &  executor )
{
    unsigned                cnt = 0;
    vector<unsigned int>    jobs_processed;
    jobs_processed.reserve(10000);

    NcbiCout << NcbiEndl << "Processing..." << NcbiEndl;

    CStopWatch      sw(CStopWatch::eStart);
    for (; 1; ++cnt) {
        CNetScheduleJob     job;
        bool                job_exists = executor.GetJob(job);
        if (!job_exists)
            continue;

        CNetScheduleKey     key( job.job_id );
        jobs_processed.push_back( key.id );

        job.output = "JOB DONE ";
        executor.PutResult(job);
    }
    double      elapsed = sw.Elapsed();

    if (cnt) {
        double      avg = elapsed / cnt;

        NcbiCout.setf(IOS_BASE::fixed, IOS_BASE::floatfield);
        NcbiCout << "Jobs processed: " << cnt << NcbiEndl;
        NcbiCout << "Elapsed: "  << elapsed << " sec." << NcbiEndl;
        NcbiCout << "Avg time: " << avg << " sec." << NcbiEndl;
    }
    else
        NcbiCout << "Processed 0 jobs." << NcbiEndl;

    return jobs_processed;
}


void CTestNetScheduleCrash::MainLoop( CNetScheduleSubmitter &  submitter,
                                      CNetScheduleExecuter &   executor,
                                      unsigned int             jcount,
                                      const string &           queue)
{
    NcbiCout << NcbiEndl << "Loop of SUBMIT->GET->PUT for "
             << jcount << " jobs. Each dot denotes 10000 loops." << NcbiEndl;

    CNetScheduleJob         job( "SUBMIT->GET->PUT loop test input" );
    string                  job_key;

    CStopWatch              sw( CStopWatch::eStart );
    for (unsigned int  k = 0; k < jcount; ++k ) {
        submitter.SubmitJob( job );

        if ( ! executor.GetJob( job ) )
            continue;

        job.output = "JOB DONE";
        executor.PutResult( job );

        if (k % 10000 == 0) {
            NcbiCout << "." << flush;
        }
    }
    double  elapsed = sw.Elapsed();

    double  avg = elapsed / jcount;
    NcbiCout.setf(IOS_BASE::fixed, IOS_BASE::floatfield);
    NcbiCout << "Jobs processed: " << jcount << NcbiEndl;
    NcbiCout << "Elapsed: "  << elapsed << " sec." << NcbiEndl;
    NcbiCout << "Avg time: " << avg << " sec." << NcbiEndl;
    return;
}


int CTestNetScheduleCrash::Run(void)
{
    const CArgs &       args = GetArgs();
    const string &      service  = args["service"].AsString();
    const string &      queue = args["queue"].AsString();

    unsigned            jcount = 10000;
    if (args["jobs"]) {
        jcount = args["jobs"].AsInteger();
    }


    CNetScheduleAPI                     cl(service, "crash_test", queue);

    cl.SetProgramVersion("test wn 1.0.2");

    cl.GetAdmin().PrintServerVersion(NcbiCout);

    CNetScheduleSubmitter               submitter = cl.GetSubmitter();
    CNetScheduleExecuter                executor = cl.GetExecuter();


    if (args["main"]) {
        this->MainLoop(submitter, executor, jcount, queue);
        return 0;
    }



    /* ----- Submit jobs ----- */
    vector<unsigned int>        jobs = this->Submit(submitter, jcount, queue);


    NcbiCout << NcbiEndl << "Waiting 10 seconds ..." << NcbiEndl;
    SleepMilliSec(10 * 1000);
    NcbiCout << NcbiEndl << "Ok." << NcbiEndl;


    /* ----- Get status ----- */
    this->GetStatus(executor, jobs);


    NcbiCout << NcbiEndl << "Waiting 10 seconds ..." << NcbiEndl;
    SleepMilliSec(10 * 1000);
    NcbiCout << NcbiEndl << "Ok." << NcbiEndl;


    /* ----- Get and return some jobs ----- */
    vector<unsigned int>      jobs_returned = this->GetReturn(executor, jcount/2);


    NcbiCout << NcbiEndl << "Waiting 10 seconds ..." << NcbiEndl;
    SleepMilliSec(10 * 1000);
    NcbiCout << NcbiEndl << "Ok." << NcbiEndl;


    /* ----- Get status ----- */
    this->GetStatus(executor, jobs);


    NcbiCout << NcbiEndl << "Waiting 10 seconds ..." << NcbiEndl;
    SleepMilliSec(10 * 1000);
    NcbiCout << NcbiEndl << "Ok." << NcbiEndl;


    /* ----- Get jobs and say they are done ----- */
    vector<unsigned int> jobs_processed = this->GetDone(executor);


    NcbiCout << NcbiEndl << "Waiting 10 seconds ..." << NcbiEndl;
    SleepMilliSec(10 * 1000);
    NcbiCout << NcbiEndl << "Ok." << NcbiEndl;


    /* ----- Get status ----- */
    this->GetStatus(executor, jobs);

    return 0;
}


int main(int argc, const char* argv[])
{
    return CTestNetScheduleCrash().AppMain(argc, argv, 0, eDS_Default);
}

