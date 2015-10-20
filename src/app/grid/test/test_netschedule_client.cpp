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

#include "test_netschedule_data.hpp"

#include <connect/services/netschedule_api.hpp>
#include <connect/services/grid_app_version_info.hpp>

#include <connect/ncbi_core_cxx.hpp>

#include <util/random_gen.hpp>

#include <set>

#define GRID_APP_NAME "test_netschedule_client"

USING_NCBI_SCOPE;


///////////////////////////////////////////////////////////////////////

static CRandom s_Random;

/// Service functions

static int s_NumTokens;
static const int s_AvgTokenLength = 10;
static const int s_DTokenLength   = 2;
static char **s_Tokens;
static char s_TokenLetters[] = "0123456789"
                               "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                               "abcdefghijklmnopqrstuvwxyz";

static void s_SeedTokens(int num_aff_tokens)
{
    s_NumTokens = num_aff_tokens;
    s_Tokens = new char* [s_NumTokens];
    for (int n = 0; n < s_NumTokens; ++n) {
        int len = s_Random.GetRand(s_AvgTokenLength - s_DTokenLength,
                                   s_AvgTokenLength + s_DTokenLength);
        s_Tokens[n] = new char[len+1];
        for (int k = 0; k < len; ++k) {
            s_Tokens[n][k] =
                s_TokenLetters[s_Random.GetRand(0, sizeof(s_TokenLetters) - 1)];
        }
        s_Tokens[n][len] = 0;
    }
}


static string s_GenInput(int input_length)
{
    string input;
    input.reserve(input_length);
    s_Random.SetSeed(CRandom::TValue(time(0)));
    for (int n = 0; n < input_length; ++n) {
        input.append(1, s_Random.GetRand(0, 255));
    }
    return input;
}


static const char *s_GetRandomToken()
{
    int n = s_Random.GetRand(0, s_NumTokens - 1);
    return s_Tokens[n];
}


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

    arg_desc->AddKey("service", "ServiceName",
        "NetSchedule service name (format: host:port or servcie_name)",
        CArgDescriptions::eString);

    arg_desc->AddKey("queue", "QueueName",
        "NetSchedule queue name",
        CArgDescriptions::eString);

    arg_desc->AddOptionalKey("ilen", "InputLength", "Average input length",
                             CArgDescriptions::eInteger);

    arg_desc->AddOptionalKey("maxruntime", "MaxRunTime",
            "Maximum run time of this test", CArgDescriptions::eInteger);

    arg_desc->AddOptionalKey("input", "InputString", "Input string",
                             CArgDescriptions::eString);

    arg_desc->AddOptionalKey("jobs", "jobs", "Number of jobs to submit",
                             CArgDescriptions::eInteger);

    arg_desc->AddOptionalKey("naff", "AffinityTokens",
        "Number of different affinities", CArgDescriptions::eInteger);


    // Setup arg.descriptions for this application
    SetupArgDescriptions(arg_desc.release());
}


int CTestNetScheduleClient::Run(void)
{
    const CArgs&   args = GetArgs();
    const string&  service  = args["service"].AsString();
    const string&  queue_name = args["queue"].AsString();

    unsigned jcount = 1000;
    if (args["jobs"]) {
        jcount = args["jobs"].AsInteger();
        if (jcount == 0) {
            fputs("The 'jobs' parameter cannot be zero.\n", stderr);
            return 1;
        }
    }

    unsigned naff = 17;
    if (args["naff"])
        naff = args["naff"].AsInteger();
    s_SeedTokens(naff);

    unsigned input_length = 0;
    if (args["ilen"])
        input_length = args["ilen"].AsInteger();

    unsigned maximum_runtime = 60;
    if (args["maxruntime"])
        maximum_runtime = args["maxruntime"].AsInteger();

    CNetScheduleAPI::EJobStatus status;
    CNetScheduleAPI cl(service, "client_test", queue_name);

    STimeout comm_timeout;
    comm_timeout.sec  = 1200;
    comm_timeout.usec = 0;
    cl.GetService().GetServerPool().SetCommunicationTimeout(comm_timeout);

    CNetScheduleSubmitter submitter = cl.GetSubmitter();

    string input;
    if (args["input"]) {
        input = args["input"].AsString();
    } else if (args["ilen"]) {
        input = s_GenInput(input_length);
    } else {
        input = "Hello " + queue_name;
    }

    if (0)
    {{

    NcbiCout << "SubmitAndWait..." << NcbiEndl;
    unsigned wait_time = 30;
    CNetScheduleJob j1(input);
    status = submitter.SubmitJobAndWait(j1, wait_time);
    if (status == CNetScheduleAPI::eDone) {
        NcbiCout << j1.job_id << " done." << NcbiEndl;
    } else {
        NcbiCout << j1.job_id << " is not done in "
                 << wait_time << " seconds." << NcbiEndl;
    }
    NcbiCout << "SubmitAndWait...done." << NcbiEndl;
    }}

    set<string> submitted_job_ids;

    CStopWatch sw(CStopWatch::eStart);

    {{
    NcbiCout << "Batch submit " << jcount << " jobs..." << NcbiEndl;

    unsigned batch_size = 1000;

    for (unsigned i = 0; i < jcount; i += batch_size) {
        vector<CNetScheduleJob> jobs;
        if (jcount - i < batch_size)
            batch_size = jcount - i;
        for (unsigned j = 0; j < batch_size; ++j) {
            CNetScheduleJob job(input);
            job.affinity = s_GetRandomToken();
            jobs.push_back(job);
        }
        submitter.SubmitJobBatch(jobs);
        ITERATE(vector<CNetScheduleJob>, job, jobs) {
            submitted_job_ids.insert(job->job_id);
        }
        NcbiCout << "." << flush;
    }

    _ASSERT(submitted_job_ids.size() == jcount);

    NcbiCout << NcbiEndl << "Done." << NcbiEndl;
    double elapsed = sw.Elapsed();

    NcbiCout.setf(IOS_BASE::fixed, IOS_BASE::floatfield);
    NcbiCout << "Avg time: " << (elapsed / jcount) << " sec, "
             << (jcount/elapsed) << " jobs/sec" << NcbiEndl;
    }}


    // Waiting for jobs to be done

    NcbiCout << "Waiting for " << jcount << " jobs..." << NcbiEndl;

    for (;;) {
        string job_id;
        string auth_token;
        CNetScheduleAPI::EJobStatus job_status;

        while (submitter.Read(&job_id, &auth_token, &job_status)) {
            submitted_job_ids.erase(job_id);
            submitter.ReadConfirm(job_id, auth_token);
        }

        if (submitted_job_ids.empty())
            break;

        bool all_deliberately_failed = true;

        CNetScheduleJob job;

        ITERATE(set<string>, job_id, submitted_job_ids) {
            job.job_id = *job_id;
            if (cl.GetJobDetails(job) != CNetScheduleAPI::eFailed ||
                    job.error_msg != "DELIBERATE_FAILURE") {
                all_deliberately_failed = false;
                break;
            }
        }

        if (all_deliberately_failed)
            break;

        if (sw.Elapsed() > double(maximum_runtime)) {
            fprintf(stderr, "The test has exceeded its maximum "
                    "run time of %u seconds.\nUse '-maxruntime' "
                    "to override.\n", maximum_runtime);
            return 3;
        }

        NcbiCout << submitted_job_ids.size() << " jobs to go" << NcbiEndl;
        SleepMilliSec(2000);
    }

    NcbiCout << "Done. Wall-clock time: " << sw.Elapsed() << NcbiEndl;
    if (!submitted_job_ids.empty())
        NcbiCout << submitted_job_ids.size() <<
            " jobs were deliberately failed by the worker node(s)." << NcbiEndl;

    return 0;
}


int main(int argc, const char* argv[])
{
    GRID_APP_CHECK_VERSION_ARGS();

    return CTestNetScheduleClient().AppMain(argc, argv);
}
