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
class CTestNetScheduleClient : public CNcbiApplication
{
public:
    void Init(void);
    int Run(void);
};



void CTestNetScheduleClient::Init(void)
{
    // Setup command line arguments and parameters

    // Create command-line argument descriptions class
    auto_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);

    // Specify USAGE context
    arg_desc->SetUsageContext(GetArguments().GetProgramBasename(),
                              "NetSchedule client");
    
    arg_desc->AddPositional("hostname", 
                            "NetCache host name.", CArgDescriptions::eString);

    arg_desc->AddPositional("port",
                            "Port number.", 
                            CArgDescriptions::eInteger);

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
    const string& host  = args["hostname"].AsString();
    unsigned short port = args["port"].AsInteger();

    unsigned jcount = 10000;
    if (args["jcount"]) {
        jcount = args["jcount"].AsInteger();
    }


    const string input = "Hello NetSchedule!";

    CNetScheduleClient cl(host, port, "client_test", "noname");
    string job_key = cl.SubmitJob(input);

    NcbiCout << job_key << NcbiEndl;

    CNetScheduleClient::EJobStatus status =
        cl.GetStatus(job_key);

    if (status == CNetScheduleClient::ePending) {
        cl.CancelJob(job_key);
        status = cl.GetStatus(job_key);
        NcbiCout << "Job canceled." << NcbiEndl;
        status = cl.GetStatus(job_key);
    }
    NcbiCout << "Job status:" << status << NcbiEndl;

    vector<string> jobs;

    {{
    CStopWatch sw(true);

    NcbiCout << "Submit " << jcount << " jobs..." << NcbiEndl;

    for (unsigned i = 0; i < jcount; ++i) {
        job_key = cl.SubmitJob(input);
        jobs.push_back(job_key);
        if (i % 1000 == 0) {
            NcbiCout << "." << flush;
        }
    }
    NcbiCout << NcbiEndl << "Done." << NcbiEndl;
    double elapsed = sw.Elapsed();
    double avg = elapsed / jcount;

    NcbiCout.setf(IOS_BASE::fixed, IOS_BASE::floatfield);
    NcbiCout << "Avg time:" << avg << " sec." << NcbiEndl;
    }}

    {{
    CStopWatch sw(true);
    NcbiCout << "Status request " << jcount << " jobs..." << NcbiEndl;

    unsigned i = 0;
    ITERATE(vector<string>, it, jobs) {
        const string jk = *it;
        status = cl.GetStatus(job_key);
        if (i % 1000 == 0) {
            NcbiCout << "." << flush;
        }
        ++i;
    }
    NcbiCout << NcbiEndl << "Done." << NcbiEndl;
    double elapsed = sw.Elapsed();
    double avg = elapsed / jcount;

    NcbiCout.setf(IOS_BASE::fixed, IOS_BASE::floatfield);
    NcbiCout << "Avg time:" << avg << " sec." << NcbiEndl;

    }}


    return 0;
}


int main(int argc, const char* argv[])
{
    return CTestNetScheduleClient().AppMain(argc, argv, 0, eDS_Default, 0);
}


/*
 * ===========================================================================
 * $Log$
 * Revision 1.1  2005/02/09 18:54:21  kuznets
 * Initial revision
 *
 *
 * ===========================================================================
 */
