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
#include <connect/ncbi_types.h>



USING_NCBI_SCOPE;

    
///////////////////////////////////////////////////////////////////////


/// Test application
///
/// @internal
///
class CSampleNetScheduleNode : public CNcbiApplication
{
public:
    void Init(void);
    int Run(void);
};



void CSampleNetScheduleNode::Init(void)
{
    SetDiagPostFlag(eDPF_Trace);
    SetDiagPostLevel(eDiag_Info);

    // Setup command line arguments and parameters

    // Create command-line argument descriptions class
    auto_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);

    // Specify USAGE context
    arg_desc->SetUsageContext(GetArguments().GetProgramBasename(),
                              "NetSchedule node");
    
    arg_desc->AddPositional("service", 
                            "NetSchedule service name", 
                            CArgDescriptions::eString);
    arg_desc->AddPositional("queue", 
                            "NetSchedule queue name (like: noname).",
                            CArgDescriptions::eString);

    // Setup arg.descriptions for this application
    SetupArgDescriptions(arg_desc.release());
}


int CSampleNetScheduleNode::Run(void)
{
    CArgs args = GetArgs();
    const string&  service  = args["service"].AsString();
    const string&  queue_name = args["queue"].AsString();  

    CNetScheduleAPI cl("node_sample", service, queue_name);

    bool      job_exists;
    int       no_jobs_counter = 0;
    unsigned  jobs_done = 0;

    int       node_status = 0; 
    bool      first_try = true;


    set<string> jobs_processed;

    // The main job processing loop, polls the 
    // queue for available jobs
    // 
    // There is no payload algorithm here, node just
    // sleeps and reports the processing result back to server
    // as string "DONE ...". Practical application should use
    // NetCache as the result storage.
    //
    // Well behaved node should not constantly poll the queue for
    // jobs (GetJob()).
    // When NetSchedule Server reports there are no more jobs: worker node
    // should take a brake and do not poll for a while... 
    // Or use WaitJob(), it receives notification messages from the 
    // server using stateless UDP protocol.
    // It is strongly suggested that there is just one program using
    // specified UDP port on the machine.

    CNetScheduleExecutor executor = cl.GetExecutor();

    CNetScheduleJob job;

    for (;;) {
        job_exists = executor.WaitJob(job, 560);
        if (job_exists) {
            if (first_try) {
                NcbiCout << "\nProcessing." << NcbiEndl;
                node_status = 0; first_try = false;
            } else 
            if (node_status != 0) {
                NcbiCout << "\nProcessing." << NcbiEndl;
                node_status = 0;
            }
//            NcbiCout << job.job_id << NcbiEndl;
            string expected_input = "Hello " + queue_name;
            if (expected_input != job.input) {
                ERR_POST("Unexpected input: " + job.input);
            }

            if (jobs_processed.find(job.job_id) != jobs_processed.end()) {
                LOG_POST(Error << "Job: " << job.job_id
                               << " has already been processed.");
            } else {
                jobs_processed.insert(job.job_id);
            }

            // do no job here, just delay for a little while
            SleepMilliSec(50);
            job.output = "DONE " + queue_name;
            executor.PutResult(job);

            no_jobs_counter = 0;
            ++jobs_done;

            if (jobs_done % 50 == 0) {
                NcbiCout << "*" << flush;
            }

        } else {
            if (first_try) {
                NcbiCout << "\nWaiting." << NcbiEndl;
                node_status = 1; first_try = false;
            } else 
            if (node_status != 1) {
                NcbiCout << "\nWaiting." << NcbiEndl;
                node_status = 1;
            }

            if (++no_jobs_counter > 7) { // no new jobs coming
                NcbiCout << "\nNo new jobs arriving. Processing closed." << NcbiEndl;
                break;
            }
        }

    }
    NcbiCout << NcbiEndl << "Jobs done: " 
             << jobs_processed.size() << NcbiEndl;

    return 0;
}


int main(int argc, const char* argv[])
{
    return CSampleNetScheduleNode().AppMain(argc, argv);
}
