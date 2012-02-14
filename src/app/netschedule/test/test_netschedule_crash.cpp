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
    void GetReturn( CNetScheduleExecuter &  executor,
                    unsigned int            count );
    vector<unsigned int>  Submit( CNetScheduleAPI &        server,
                                  const string &           service,
                                  unsigned int             jcount,
                                  unsigned int             naff,
                                  unsigned int             notif_port,
                                  unsigned int             nclients,
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

    arg_desc->AddOptionalKey("delay",
                             "delay",
                             "Number of seconds between operations",
                             CArgDescriptions::eInteger);

    arg_desc->AddOptionalKey("naff",
                             "naff",
                             "Number of affinities",
                             CArgDescriptions::eInteger);

    arg_desc->AddOptionalKey("notifport",
                             "notifport",
                             "If given then jobs are submitted with notif port"
                             " and 100 sec notif timeout",
                             CArgDescriptions::eInteger);

    arg_desc->AddOptionalKey("batch",
                             "batch",
                             "Max number of jobs in a loop",
                             CArgDescriptions::eInteger);

    arg_desc->AddOptionalKey("nclients",
                             "nclients",
                             "Number of clients to simulate",
                             CArgDescriptions::eInteger);

    arg_desc->AddOptionalKey("main",
                             "main",
                             "Main loop (submit-get-put) only",
                             CArgDescriptions::eBoolean);

    // Setup arg.descriptions for this application
    SetupArgDescriptions(arg_desc.release());
}



vector<unsigned int>
CTestNetScheduleCrash::Submit( CNetScheduleAPI &        cl,
                               const string &           service,
                               unsigned int             jcount,
                               unsigned int             naff,
                               unsigned int             notif_port,
                               unsigned int             nclients,
                               const string &           queue )
{
    string                  input = "Crash test for " + queue;
    vector<unsigned int>    jobs;
    unsigned int            caff = 0;
    unsigned int            client = 0;
    string                  aff = "";


    CNetServer              server = cl.GetService().Iterate().GetServer();
    CNetScheduleSubmitter   submitter = cl.GetSubmitter();


    jobs.reserve(jcount);
    NcbiCout << "Submit " << jcount << " jobs..." << NcbiEndl;

    CStopWatch      sw(CStopWatch::eStart);
    for (unsigned i = 0; i < jcount; ++i) {

        if (nclients > 0) {
            cl =  CNetScheduleAPI(service, "crash_test", queue);
            cl.SetClientSession( "crash_test_session" );
            char        buffer[ 1024 ];
            sprintf( buffer, "node_%d", client++ );
            if (client >= nclients)
                client = 0;
            cl.SetClientNode( buffer );
            server = cl.GetService().Iterate().GetServer();
            submitter = cl.GetSubmitter();
        }


        if (naff > 0) {
            char        buffer[ 1024 ];
            sprintf( buffer, "aff%d", caff++ );
            if (caff >= naff)
                caff = 0;
            aff = string( buffer );
        }

        // Two options here: with/without notifications
        if (notif_port == 0) {
            CNetScheduleJob         job(input);
            job.affinity = aff;
            submitter.SubmitJob(job);

            CNetScheduleKey     key( job.job_id );
            jobs.push_back( key.id );
            if (m_KeyGenerator == NULL)
                m_KeyGenerator = new CNetScheduleKeyGenerator( key.host,
                                                               key.port );
        }
        else {
            // back door - use manually formed command
            char    buffer[ 1024 ];
            sprintf( buffer,
                     "SUBMIT input=\"Crash test for %s\" port=%d timeout=100",
                     queue.c_str(), notif_port );
            string  cmd = string( buffer );
            if (aff.empty() == false)
                cmd += " aff=" + aff;
            CNetServer::SExecResult     result = server.ExecWithRetry( cmd );

            // Dirty hack: 'OK:' need to be stripped
            CNetScheduleKey     key( result.response.c_str() );
            jobs.push_back( key.id );
            if (m_KeyGenerator == NULL)
                m_KeyGenerator = new CNetScheduleKeyGenerator( key.host,
                                                               key.port );
        }


        if (i % 1000 == 0) {
            NcbiCout << "." << flush;
        }

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
    NcbiCout << NcbiEndl
             << "Elapsed :"  << elapsed << " sec." << NcbiEndl;
    NcbiCout << "Avg time :" << avg << " sec." << NcbiEndl;
    return;
}


/* Returns up to count jobs */
void  CTestNetScheduleCrash::GetReturn( CNetScheduleExecuter &  executor,
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

        try {
            executor.ReturnJob(m_KeyGenerator->GenerateV1(job_id));
        }
        catch (...)
        {}
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

    return;
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
            break;

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
    NcbiCout << NcbiEndl
             << "Jobs processed: " << jcount << NcbiEndl;
    NcbiCout << "Elapsed: "  << elapsed << " sec." << NcbiEndl;
    NcbiCout << "Avg time: " << avg << " sec." << NcbiEndl;
    return;
}


int CTestNetScheduleCrash::Run(void)
{
    const CArgs &       args = GetArgs();
    const string &      service  = args["service"].AsString();
    const string &      queue = args["queue"].AsString();

    unsigned            total_jobs = 10000;
    if (args["jobs"])
        total_jobs = args["jobs"].AsInteger();

    unsigned int        delay = 10;
    if (args["delay"])
        delay = args["delay"].AsInteger();

    unsigned int        naff = 0;
    if (args["naff"])
        naff = args["naff"].AsInteger();

    unsigned int        notif_port = 0;
    if (args["notifport"])
        notif_port = args["notifport"].AsInteger();

    unsigned int        nclients = 0;
    if (args["nclients"])
        nclients = args["nclients"].AsInteger();

    unsigned int        batch = 10000;
    if (args["batch"]) {
        batch = args["batch"].AsInteger();
        if (batch == 0)
            batch = 10000;
    }


    CNetScheduleAPI                     cl(service, "crash_test", queue);

    cl.SetProgramVersion("test wn 1.0.2");

    cl.GetAdmin().PrintServerVersion(NcbiCout);

    CNetScheduleSubmitter               submitter = cl.GetSubmitter();
    CNetScheduleExecuter                executor = cl.GetExecuter();


    if (args["main"]) {
        this->MainLoop(submitter, executor, total_jobs, queue);
        return 0;
    }


    unsigned int    jcount;
    while (total_jobs > 0) {
        if (total_jobs < batch)
            jcount = total_jobs;
        else
            jcount = batch;
        total_jobs -= jcount;

        /* ----- Submit jobs ----- */
        vector<unsigned int>    jobs = this->Submit(cl, service, jcount, naff,
                                                    notif_port, nclients, queue);


        NcbiCout << NcbiEndl << "Waiting " << delay << " second(s) ..." << NcbiEndl;
        SleepMilliSec(delay * 1000);
        NcbiCout << NcbiEndl << "Ok." << NcbiEndl;


        /* ----- Get status ----- */
        this->GetStatus(executor, jobs);


        NcbiCout << NcbiEndl << "Waiting " << delay << " second(s) ..." << NcbiEndl;
        SleepMilliSec(delay * 1000);
        NcbiCout << NcbiEndl << "Ok." << NcbiEndl;


        /* ----- Get and return some jobs ----- */
        this->GetReturn(executor, jcount/100);


        NcbiCout << NcbiEndl << "Waiting " << delay << " second(s) ..." << NcbiEndl;
        SleepMilliSec(delay * 1000);
        NcbiCout << NcbiEndl << "Ok." << NcbiEndl;


        /* ----- Get status ----- */
        this->GetStatus(executor, jobs);


        NcbiCout << NcbiEndl << "Waiting " << delay << " second(s) ..." << NcbiEndl;
        SleepMilliSec(delay * 1000);
        NcbiCout << NcbiEndl << "Ok." << NcbiEndl;


        /* ----- Get jobs and say they are done ----- */
        vector<unsigned int> jobs_processed = this->GetDone(executor);


        NcbiCout << NcbiEndl << "Waiting " << delay << " second(s) ..." << NcbiEndl;
        SleepMilliSec(delay * 1000);
        NcbiCout << NcbiEndl << "Ok." << NcbiEndl;


        /* ----- Get status ----- */
        this->GetStatus(executor, jobs);
    }

    return 0;
}


int main(int argc, const char* argv[])
{
    return CTestNetScheduleCrash().AppMain(argc, argv, 0, eDS_Default);
}

