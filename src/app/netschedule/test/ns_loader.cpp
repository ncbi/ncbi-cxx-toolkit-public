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
 * Authors:  Sergey Satskiy
 *
 * File Description:  NetSchedule loader.
 *                    It uses affinities to avoid job migration between
 *                    loader instances.
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

#include <sys/types.h>
#include <unistd.h>


USING_NCBI_SCOPE;



/// Test application
///
/// @internal
///
class CNetScheduleLoader : public CNcbiApplication
{
    public:
        void Init(void);
        int Run(void);


        CNetScheduleLoader()
        {}

        ~CNetScheduleLoader()
        {}

    private:
        unsigned int  x_GetTotalJobs(const CArgs &  args);
        CNetScheduleAPI  x_GetAPI(const string &  service,
                                  const string &  qname);
        string  x_GetAffinity(void);
        CNetScheduleJob  x_SubmitJob(CNetScheduleSubmitter &  submitter,
                                     const string &  aff);
};



void CNetScheduleLoader::Init(void)
{
    // Avoid sockets to stay in TIME_WAIT state
    GetConfig().Set("netservice_api", "use_linger2", "true",
                    IRegistry::fNoOverride);

    //
    CONNECT_Init();
    SetDiagPostFlag(eDPF_Trace);
    SetDiagPostLevel(eDiag_Info);

    // Setup command line arguments and parameters

    // Create command-line argument descriptions class
    auto_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);

    // Specify USAGE context
    arg_desc->SetUsageContext(GetArguments().GetProgramBasename(),
                              "NetSchedule loader");

    arg_desc->AddKey("service",
                     "service_name",
                     "NetSchedule service name "
                                        "(format: host:port or servcie_name).",
                     CArgDescriptions::eString);

    arg_desc->AddKey("queue",
                     "queue_name",
                     "NetSchedule queue name (like: noname).",
                     CArgDescriptions::eString);

    arg_desc->AddOptionalKey("jobs",
                             "jobs",
                             "Number of jobs to submit",
                             CArgDescriptions::eInteger);

    // Setup arg.descriptions for this application
    SetupArgDescriptions(arg_desc.release());
}


unsigned int  CNetScheduleLoader::x_GetTotalJobs(const CArgs &  args)
{
    if (args["jobs"])
        return args["jobs"].AsInteger();
    return 10000;   // default
}


CNetScheduleAPI  CNetScheduleLoader::x_GetAPI(const string &  service,
                                              const string &  qname)
{
    char        buffer[ 64 ];
    sprintf(buffer, "node_%d", getpid());

    CNetScheduleAPI     cl = CNetScheduleAPI(service, "crash_test", qname);
    cl.SetProgramVersion("ns_loader 1.0.0");
    cl.SetClientNode( buffer );
    cl.SetClientSession("ns_loader_session");
    return cl;
}

string  CNetScheduleLoader::x_GetAffinity(void)
{
    char        buffer[ 64 ];
    sprintf(buffer, "aff_%d", getpid());
    return buffer;
}

CNetScheduleJob
CNetScheduleLoader::x_SubmitJob(CNetScheduleSubmitter &  submitter,
                                const string &           aff)
{
    static string       input = "ns_loader input";
    CNetScheduleJob     job(input);

    job.affinity = aff;
    submitter.SubmitJob(job);

    return job;
}


int CNetScheduleLoader::Run(void)
{
    const CArgs &           args = GetArgs();
    unsigned                total_jobs = x_GetTotalJobs(args);
    string                  aff = x_GetAffinity();
    CNetScheduleAPI         cl = x_GetAPI(args["service"].AsString(),
                                          args["queue"].AsString());
    CNetScheduleSubmitter   submitter = cl.GetSubmitter();
    CNetScheduleExecutor    executor = cl.GetExecutor();

    cl.GetAdmin().PrintServerVersion(NcbiCout);


    CNetScheduleJob                 job;
    CNetScheduleAPI::EJobStatus     status;
    while (total_jobs > 0) {
        // Submit
        job.Reset();
        job = x_SubmitJob(submitter, aff);

        // SST
        status = submitter.GetJobStatus(job.job_id);
        if (status != CNetScheduleAPI::ePending)
            throw runtime_error("Unexpected job status after SUBMIT");

        // GET2 - only the client unique affinity
        bool    job_provided = executor.GetJob(job, aff);
        if (!job_provided)
            throw runtime_error("Expected a job for execution, received nothing");

        // WST
        status = executor.GetJobStatus(job);
        if (status != CNetScheduleAPI::eRunning)
            throw runtime_error("Unexpected job status after GET2");

        // PUT2
        job.output = "JOB DONE";
        executor.PutResult(job);

        // WST
        status = executor.GetJobStatus(job);
        if (status != CNetScheduleAPI::eDone)
            throw runtime_error("Unexpected job status after PUT2");

        --total_jobs;
    }

    return 0;
}


int main(int argc, const char* argv[])
{
    return CNetScheduleLoader().AppMain(argc, argv, 0, eDS_Default);
}

