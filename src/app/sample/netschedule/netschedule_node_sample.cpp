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

#include <connect/netschedule_client.hpp>
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

    arg_desc->AddOptionalKey("udp_port", 
                             "udp_port",
                             "Incoming UDP port",
                             CArgDescriptions::eInteger);
    
    // Setup arg.descriptions for this application
    SetupArgDescriptions(arg_desc.release());
}


int CSampleNetScheduleNode::Run(void)
{
    CArgs args = GetArgs();
    const string&  service  = args["service"].AsString();
    const string&  queue_name = args["queue"].AsString();  

    unsigned short udp_port = 9111;
    if (args["udp_port"]) {
        udp_port = args["udp_port"].AsInteger();
    }
    NcbiCout << "Incoming UDP port:" << udp_port << NcbiEndl;


    CNetScheduleClient_LB cl("node_sample", service, queue_name);

    string    job_key;
    string    input;
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
    // netcache as result storage
    //
    // Well behaved node should not constantly poll the queue for
    // jobs (GetJob()).
    // When NetSchedule Server reports there are no more jobs: worker node
    // should take a brake and do not poll for a while... 
    // Or use WaitJob(), it receives notification messages from the 
    // server using stateless UDP protocol.
    // It is strongly suggested that there is just one program using
    // specified UDP port on the machine.

    while (1) {
        job_exists = cl.WaitJob(&job_key, &input, 560, udp_port);
        if (job_exists) {
            if (first_try) {
                NcbiCout << "\nProcessing." << NcbiEndl;
                node_status = 0; first_try = false;
            } else 
            if (node_status != 0) {
                NcbiCout << "\nProcessing." << NcbiEndl;
                node_status = 0;
            }
//            NcbiCout << job_key << NcbiEndl;
            string expected_input = "Hello " + queue_name;
            if (expected_input != input) {
                ERR_POST("Unexpected input: " + input);
            }

            if (jobs_processed.find(job_key) != jobs_processed.end()) {
                LOG_POST(Error << "Job: " << job_key 
                               << " has already been processed.");
            } else {
                jobs_processed.insert(job_key);
            }

            // do no job here, just delay for a little while
            SleepMilliSec(50);
            string out = "DONE " + queue_name;
            cl.PutResult(job_key, 0, out);

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
    return CSampleNetScheduleNode().AppMain(argc, argv, 0, eDS_Default, 0);
}


/*
 * ===========================================================================
 * $Log$
 * Revision 1.2  2005/03/07 17:39:25  kuznets
 * Fixed GCC warning
 *
 * Revision 1.1  2005/03/07 17:32:47  kuznets
 * Initial revision
 *
 *
 *
 * ===========================================================================
 */
