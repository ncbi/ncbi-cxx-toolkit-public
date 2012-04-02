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

#include <connect/services/grid_client.hpp>
#include <connect/services/grid_client_app.hpp>

#include <corelib/ncbiapp.hpp>
#include <corelib/ncbimisc.hpp>
#include <corelib/ncbi_system.hpp>

#include <math.h>
#include <algorithm>

#define PROGRAM_NAME "SampleNodeStressTest"
#define PROGRAM_VERSION "1.1.0"

USING_NCBI_SCOPE;

class CGridClientTestApp : public CGridClientApp
{
public:
    CGridClientTestApp() {
        SetVersion(CVersionInfo(PROGRAM_VERSION, PROGRAM_NAME));
    }

    virtual void Init(void);
    virtual int Run(void);
    virtual string GetProgramVersion(void) const;

    virtual bool UseProgressMessage() const
    {
        return false;
    }
};

string CGridClientTestApp::GetProgramVersion(void) const
{
    return PROGRAM_NAME " version " PROGRAM_VERSION;
}


void CGridClientTestApp::Init(void)
{
    CGridClientApp::Init();

    // Create command-line argument descriptions class
    auto_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);

    // Specify USAGE context
    arg_desc->SetUsageContext(GetArguments().GetProgramBasename(),
                              "Grid client sample");

    arg_desc->AddOptionalKey("jcount", 
                             "jcount",
                             "Number of jobs to submit",
                             CArgDescriptions::eInteger);

    arg_desc->AddOptionalKey("vsize", 
                             "vsize",
                             "Size of the test vector",
                             CArgDescriptions::eInteger);


    // Setup arg.descriptions for this application
    SetupArgDescriptions(arg_desc.release());
}

int CGridClientTestApp::Run(void)
{
    CArgs args = GetArgs();

    unsigned jcount = 5;
    int vsize = 10000;
    if (args["jcount"]) {
        jcount = args["jcount"].AsInteger();
    }
    if (args["vsize"]) {
        vsize = args["vsize"].AsInteger();
    }

    CNetScheduleAPI::EJobStatus status;
    NcbiCout << "Submit " << jcount << " jobs..." << NcbiEndl;

    CGridClient& grid_client = GetGridClient();

    string job_key;
    string input1 = "Hello ";
    typedef map<string, vector<double>* > TJobs;
    TJobs jobs;
    for (unsigned i = 0; i < jcount; ++i) {
        string ns_key;
        vector<double>* dvec = new vector<double>;
        CNcbiOstream& os = grid_client.GetOStream();
        os << "doubles " << vsize << ' ';
        srand( (unsigned)time( NULL ) );
        for (int j = 0; j < vsize; ++j) {
            double d = rand()*0.2;
            os << d << ' ';
            dvec->push_back(d);
        }
        string job_key = grid_client.Submit();
        sort(dvec->begin(),dvec->end());
        jobs[job_key] = dvec;

        if (i % 1000 == 0) {
            NcbiCout << "." << flush;
        }
    }
    NcbiCout << NcbiEndl << "Done." << NcbiEndl;

    NcbiCout << "Waiting for jobs..." << jobs.size() << NcbiEndl;
    unsigned cnt = 0;
    SleepMilliSec(100);

    while (jobs.size()) {
        NON_CONST_ITERATE(TJobs, it, jobs) {
            const string& jk = it->first;
            CGridClient& grid_client = GetGridClient();
            grid_client.SetJobKey(jk);
            vector<double>* dvec = it->second;

            status = grid_client.GetStatus();

            if (status == CNetScheduleAPI::eDone) {
                CNcbiIstream& is = grid_client.GetIStream();
                if (!is.good()) {
                    LOG_POST("Input stream error while reading the "
                        "size of the resulting vector");
                    break;
                }
                int count;
                is >> count;
                vector<double> resvec;
                for (int i = 0; i < count; ++i) {
                    if (!is.good()) {
                        LOG_POST( "Input stream error. Index : " << i );
                        break;
                    }
                    double d;
                    is >> d;
                    resvec.push_back(d);
                }
                if (resvec.size() == dvec->size()) {
                    for(size_t i = 0; i < resvec.size(); ++i) {
                        //cout << (*dvec)[i] << " --- " << resvec[i] << ";  ";
                        if( fabs((*dvec)[i] - resvec[i])/resvec[i] > 0.001 ) {
                            LOG_POST( "Test failed! Wrong vector element. Difference is  " 
                                      << fabs((*dvec)[i] - resvec[i]));
                            break;
                        }
                    }
                    cout << endl;
                }
                else {
                    LOG_POST( "Test failed! Wrong vector size."  );
                }
                LOG_POST( "Job " << jk << " done." );
                delete dvec;
                jobs.erase(it);
                ++cnt;
                break;
            } 
            else if (status == CNetScheduleAPI::eFailed) {
                LOG_POST( "Job " << jk << " failed : " << grid_client.GetErrorMessage() );
                delete dvec;
                jobs.erase(it);
                ++cnt;
                break;
            }

            else if (status == CNetScheduleAPI::eCanceled) {
                NcbiCerr << "Job " << jk << " is canceled." << NcbiEndl;
                delete dvec;
                jobs.erase(it);
                ++cnt;
                break;                
            }
            
            ++cnt;
            if (cnt % 1000 == 0) {
                NcbiCout << "Waiting for " 
                         << jobs.size() 
                         << " jobs."
                         << NcbiEndl;
                // it is necessary to give system a rest periodically
                SleepMilliSec(2000);
                // check status of only first 1000 jobs
                // since the JS queue execution priority is FIFO
                break;
            }
        }

    }
    return 0;
}


int main(int argc, const char* argv[])
{
    return CGridClientTestApp().AppMain(argc, argv, 0, eDS_Default, "test_gridclient_stress.ini");
} 
