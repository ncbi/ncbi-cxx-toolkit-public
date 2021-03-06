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
 *   Government have not placed any restriction on its use or reproduction.
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
 * Authors:  Maxim Didenko
 *
 * File Description:
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbimisc.hpp>
#include <corelib/ncbi_system.hpp>

#include <connect/services/grid_client.hpp>
#include <connect/services/grid_client_app.hpp>
#include <connect/services/remote_app.hpp>

USING_NCBI_SCOPE;


/////////////////////////////////////////////////////////////////////////////
//  CRemoteAppClientSampleApp::
//

class CRemoteAppClientSampleApp : public CGridClientApp
{
public:
    virtual void Init(void);
    virtual int  Run(void);
    virtual string GetProgramVersion(void) const
    {
        // Next formats are valid and supported:
        //   ProgramName 1.2.3
        //   ProgramName version 1.2.3
        //   ProgramName v. 1.2.3
        //   ProgramName ver. 1.2.3

        return "SampleNodeClient version 1.0.1";
    }

protected:
    void PrintJobInfo(const string& job_key, CNetCacheAPI::TInstance netcache_api);
    void ShowBlob(const string& blob_key);

    virtual bool UseProgressMessage() const { return false; }
    virtual bool UseAutomaticCleanup() const { return false; }
};


void CRemoteAppClientSampleApp::Init(void)
{
    // Don't forget to call it
    CGridClientApp::Init();

    // Create command-line argument descriptions class
    unique_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);

    // Specify USAGE context
    arg_desc->SetUsageContext(GetArguments().GetProgramBasename(),
                              "Remote app client sample");

    arg_desc->AddOptionalKey("jobs", 
                             "jobs",
                             "Number of jobs to submit",
                             CArgDescriptions::eInteger);

    arg_desc->AddOptionalKey("jobinfo", 
                             "job",
                             "Get job's info",
                             CArgDescriptions::eString);

    arg_desc->AddOptionalKey("showblob", 
                             "blob",
                             "Show blob content",
                             CArgDescriptions::eString);

    // Setup arg.descriptions for this application
    SetupArgDescriptions(arg_desc.release());
}


int CRemoteAppClientSampleApp::Run(void)
{
    const char* kData = 
        "!====================================================================================================!";
    const size_t kDataSize = strlen(kData);

    const CArgs& args = GetArgs();

    if (args["showblob"]) {
        ShowBlob(args["showblob"].AsString());
        return 0;
    }

    CNetCacheAPI netcache_api(GetGridClient().GetNetCacheAPI());

    if (args["jobinfo"]) {
        PrintJobInfo(args["jobinfo"].AsString(), netcache_api);
        return 0;
    }

    int jobs_number = 10;
    if (args["jobs"]) {
        jobs_number = args["jobs"].AsInteger();
    }

    cout << "Submitting jobs..." << jobs_number << endl;

    typedef list<string> TJobKeys;
    TJobKeys job_keys;

    CRemoteAppRequest request(netcache_api);
    for (int i = 0; i < jobs_number; ++i) {
        CNcbiOstream& os = request.GetStdIn();

        if (i % 5 == 0) {
            os.write(kData, kDataSize);
            os << endl << i << endl;
            os.write(kData, kDataSize);
            os.write(kData, kDataSize);
            os.write(kData, kDataSize);
            os.write(kData, kDataSize);
        } else
            os << "Request data";

        request.SetCmdLine("-a sss -f=/tmp/dddd.f1");
        request.AddFileForTransfer("/tmp/dddd.f1");

        // Get a job submitter
        CGridClient& grid_client = GetGridClient();

        // Serialize the request;
        request.Send(grid_client.GetOStream());

        // Submit a job
        job_keys.push_back(grid_client.Submit());
    }
    cout << endl << "Done." << endl;
     
    cout << "Waiting for jobs..." << endl;

    CRemoteAppResult result(netcache_api);
    TJobKeys failed_jobs;

    unsigned int cnt = 0;
    while (1) {
        SleepMilliSec(100);
        
        typedef list<TJobKeys::iterator> TDoneJobs;
        TDoneJobs done_jobs;

        for(auto it = job_keys.begin(); it != job_keys.end(); ++it) {
            // Get a job status
            CGridClient& grid_client(GetGridClient());
            grid_client.SetJobKey(*it);
            CNetScheduleAPI::EJobStatus status;
            status = grid_client.GetStatus();

            // A job is done here
            if (status == CNetScheduleAPI::eDone) {
                
                result.Receive(grid_client.GetIStream());
                
                cout << "Job : " << *it << endl;
                cout << "Return code: " << result.GetRetCode() << endl;
                if (result.GetRetCode()==-1) 
                    failed_jobs.push_back(*it);
                cout << "StdOut : " <<  endl;
                cout << result.GetStdOut().rdbuf();
                cout.clear();
                cout << endl << "StdErr : " <<  endl;
                cout << result.GetStdErr().rdbuf();
                cout.clear();
                cout << endl << "----------------------" <<  endl;
                done_jobs.push_back(it);
            }

            // A job has failed
            if (status == CNetScheduleAPI::eFailed) {
                ERR_POST( "Job " << *it << " failed : " 
                             << grid_client.GetErrorMessage() );
                failed_jobs.push_back(*it);
                done_jobs.push_back(it);
            }

            // A job has been canceled
            if (status == CNetScheduleAPI::eCanceled) {
                ERR_POST( Warning << Note << "Job " << *it << " is canceled.");
                done_jobs.push_back(it);
            }
        }
        for (const auto& job : done_jobs) {
            job_keys.erase(job);
        }
        if (job_keys.empty())
            break;

        // A job is still running
        if (++cnt % 1000 == 0) {
            cout << "Still waiting..." << endl;
        }
    }

    ERR_POST(Info << Note
                  << "==================== All finished ==================");
    for (const auto& job : job_keys) {
        PrintJobInfo(job, netcache_api);
    }
    return 0;
}


void CRemoteAppClientSampleApp::ShowBlob(const string& blob_key)
{
    CNetCacheAPI nc_api(GetGridClient().GetNetCacheAPI());
    unique_ptr<CNcbiIstream> is(nc_api.GetIStream(blob_key));
    NcbiStreamCopy(cout, *is);
    cout << endl;
}


void CRemoteAppClientSampleApp::PrintJobInfo(const string& job_key,
    CNetCacheAPI::TInstance netcache_api)
{
    CGridClient& grid_client(GetGridClient());
    grid_client.SetJobKey(job_key);
    CNetScheduleAPI::EJobStatus status;
    status = grid_client.GetStatus();

    // A job is done here
    if (status == CNetScheduleAPI::eDone) {
        cout << "Job : " << job_key << endl;
        cout << "Input : " << grid_client.GetJobInput() << endl; 
        cout << "Output : " << grid_client.GetJobOutput() << endl; 
        cout << "======================================" << endl; 
        CRemoteAppResult result(netcache_api);
        result.Receive(grid_client.GetIStream());
        cout << "Return code: " << result.GetRetCode() << endl;
        cout << "StdOut : " <<  endl;
        cout << result.GetStdOut().rdbuf();
        cout.clear();
        cout << endl << "StdErr : " <<  endl;
        cout << result.GetStdErr().rdbuf();
        cout.clear();
    }
    
    // A job has failed
    if (status == CNetScheduleAPI::eFailed) {
        ERR_POST( "Job " << job_key << " failed : " 
                  << grid_client.GetErrorMessage() );
    }
    // A job has been canceled
    if (status == CNetScheduleAPI::eCanceled) {
        ERR_POST( Warning << Note << "Job " << job_key << " is canceled.");
    }
}


int NcbiSys_main(int argc, ncbi::TXChar* argv[])
{
    return CRemoteAppClientSampleApp().AppMain(argc, argv);
}
