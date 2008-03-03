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
#include <corelib/blob_storage.hpp>

#include <connect/services/grid_client.hpp>
#include <connect/services/grid_client_app.hpp>
#include <connect/services/remote_job.hpp>
#include <connect/services/blob_storage_netcache.hpp>


USING_NCBI_SCOPE;

class CRemoteAppClientSampleApp : public CGridClientApp
{
public:

    virtual void Init(void);
    virtual int Run(void);
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

    void PrintJobInfo(const string& job_key);
    void ShowBlob(const string& blob_key);

    virtual bool UseProgressMessage() const { return false; }
    virtual bool UseAutomaticCleanup() const { return false; }

};

void CRemoteAppClientSampleApp::Init(void)
{
    // hack!!! It needs to be removed when we know how to deal with unresolved
    // symbols in plugins.
    BlobStorage_RegisterDriver_NetCache(); 
    // Don't forget to call it
    CGridClientApp::Init();

    // Create command-line argument descriptions class
    auto_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);

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

const string kData = 
    "!====================================================================================================!";
int CRemoteAppClientSampleApp::Run(void)
{

 

    CArgs args = GetArgs();

    if (args["showblob"]) {
        ShowBlob(args["showblob"].AsString());
        return 0;
    }
    if (args["jobinfo"]) {
        PrintJobInfo(args["jobinfo"].AsString());
        return 0;
    }

    int jobs_number = 10;
    if (args["jobs"]) {
        jobs_number = args["jobs"].AsInteger();
    }

    NcbiCout << "Submitting jobs..." << jobs_number << NcbiEndl;

    typedef list<string> TJobKeys;
    TJobKeys job_keys;

    CBlobStorageFactory factory(GetConfig());
    CRemoteAppRequest request(factory);
    for (int i = 0; i < jobs_number; ++i) {
        CNcbiOstream& os = request.GetStdIn();

        if (i % 5 == 0) {
            os.write(kData.c_str(),kData.size());
            os << endl << i << endl;
            os.write(kData.c_str(),kData.size());
            os.write(kData.c_str(),kData.size());
            os.write(kData.c_str(),kData.size());
            os.write(kData.c_str(),kData.size());
        } else
            os << "Request data";

        request.SetCmdLine("-a sss -f=/tmp/dddd.f1");
        request.AddFileForTransfer("/tmp/dddd.f1");

        //request.SetStdOutErrFileNames("/tmp/out.txt", "/tmp/err.txt", eLocalFile);

        // Get a job submiter
        CGridJobSubmitter& job_submiter = GetGridClient().GetJobSubmitter();

        // Serialize the requset;
        request.Send(job_submiter.GetOStream());

        // Submit a job
        job_keys.push_back(job_submiter.Submit());
    }
    NcbiCout << NcbiEndl << "Done." << NcbiEndl;
     
    NcbiCout << "Waiting for jobs..." << NcbiEndl;

    CRemoteAppResult result(factory);
    TJobKeys failed_jobs;

    unsigned int cnt = 0;
    while (1) {
        SleepMilliSec(100);
        
        typedef list<TJobKeys::iterator> TDoneJobs;
        TDoneJobs done_jobs;
        for(TJobKeys::iterator it = job_keys.begin();
            it != job_keys.end(); ++it) {
        // Get a job status
            CGridJobStatus& job_status = GetGridClient().GetJobStatus(*it);
            CNetScheduleAPI::EJobStatus status;
            status = job_status.GetStatus();

            // A job is done here
            if (status == CNetScheduleAPI::eDone) {
                
                result.Receive(job_status.GetIStream());
                
                NcbiCout << "Job : " << *it << NcbiEndl;
                NcbiCout << "Return code: " << result.GetRetCode() << NcbiEndl;
                if (result.GetRetCode()==-1) 
                    failed_jobs.push_back(*it);
                NcbiCout << "StdOut : " <<  NcbiEndl;
                NcbiCout << result.GetStdOut().rdbuf();
                NcbiCout.clear();
                NcbiCout << NcbiEndl << "StdErr : " <<  NcbiEndl;
                NcbiCout << result.GetStdErr().rdbuf();
                NcbiCout.clear();
                NcbiCout << NcbiEndl << "----------------------" <<  NcbiEndl;
                done_jobs.push_back(it);

            }

            // A job has failed
            if (status == CNetScheduleAPI::eFailed) {
                ERR_POST( "Job " << *it << " failed : " 
                             << job_status.GetErrorMessage() );
                failed_jobs.push_back(*it);
                done_jobs.push_back(it);
            }

            // A job has been canceled
            if (status == CNetScheduleAPI::eCanceled) {
                LOG_POST( "Job " << *it << " is canceled.");
                done_jobs.push_back(it);
            }
        }
        for(TDoneJobs::iterator i = done_jobs.begin();
            i != done_jobs.end(); ++i) {
            job_keys.erase(*i);
        }
        if (job_keys.empty())
            break;
            // A job is still running
        if (++cnt % 1000 == 0) {
            NcbiCout << "Still waiting..." << NcbiEndl;
        }
    }

    LOG_POST("==================== All finished ==================");
    for(TJobKeys::iterator it = job_keys.begin();
        it != job_keys.end(); ++it) {
        PrintJobInfo(*it);
    }
    return 0;
}

void CRemoteAppClientSampleApp::ShowBlob(const string& blob_key)
{
    CBlobStorageFactory factory(GetConfig());
    auto_ptr<IBlobStorage> storage(factory.CreateInstance());
    //    string str = storage->GetBlobAsString(blob_key);
    //    NcbiCout << str << NcbiEndl;
    CNcbiIstream& is = storage->GetIStream(blob_key);
    NcbiCout << is.rdbuf() << NcbiEndl;
}

void CRemoteAppClientSampleApp::PrintJobInfo(const string& job_key)
{
    CGridJobStatus& job_status = GetGridClient().GetJobStatus(job_key);
    CNetScheduleAPI::EJobStatus status;
    status = job_status.GetStatus();
    // A job is done here
    if (status == CNetScheduleAPI::eDone) {
        NcbiCout << "Job : " << job_key << NcbiEndl;
        NcbiCout << "Input : " << job_status.GetJobInput() << NcbiEndl; 
        NcbiCout << "Output : " << job_status.GetJobOutput() << NcbiEndl; 
        NcbiCout << "======================================" << NcbiEndl; 
        CBlobStorageFactory factory(GetConfig());
        CRemoteAppResult result(factory);
        result.Receive(job_status.GetIStream());
        NcbiCout << "Return code: " << result.GetRetCode() << NcbiEndl;
        NcbiCout << "StdOut : " <<  NcbiEndl;
        NcbiCout << result.GetStdOut().rdbuf();
        NcbiCout.clear();
        NcbiCout << NcbiEndl << "StdErr : " <<  NcbiEndl;
        NcbiCout << result.GetStdErr().rdbuf();
        NcbiCout.clear();
    }
    
    // A job has failed
    if (status == CNetScheduleAPI::eFailed) {
        ERR_POST( "Job " << job_key << " failed : " 
                  << job_status.GetErrorMessage() );
    }
    
    // A job has been canceled
    if (status == CNetScheduleAPI::eCanceled) {
        LOG_POST( "Job " << job_key << " is canceled.");
    }
}


int main(int argc, const char* argv[])
{
    return CRemoteAppClientSampleApp().AppMain(argc, argv);
}
