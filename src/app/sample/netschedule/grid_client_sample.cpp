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
#include <corelib/plugin_manager.hpp>
#include <corelib/ncbireg.hpp>
#include <corelib/ncbi_config.hpp>
#include <corelib/ncbimisc.hpp>
#include <corelib/ncbi_system.hpp>

#include <connect/services/netschedule_client.hpp>
#include <connect/services/netcache_client.hpp>
#include <connect/services/netcache_nsstorage_imp.hpp>

USING_NCBI_SCOPE;

class CGridClientSample : public CNcbiApplication
{
public:
    typedef CPluginManager<CNetScheduleClient> TPMNetSchedule;
    typedef CPluginManager<CNetCacheClient>    TPMNetCache;

    void Init(void);
    int Run(void);

private:
    TPMNetSchedule            m_PM_NetSchedule;
    TPMNetCache               m_PM_NetCache;

};

void CGridClientSample::Init(void)
{
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

    m_PM_NetSchedule.RegisterWithEntryPoint(NCBI_EntryPoint_xnetschedule);
    m_PM_NetCache.RegisterWithEntryPoint(NCBI_EntryPoint_xnetcache);

    //SetDiagPostFlag(eDPF_Trace);
    //SetDiagPostLevel(eDiag_Info);

}

int CGridClientSample::Run(void)
{
    CArgs args = GetArgs();

    auto_ptr<CNetScheduleClient> ns_client;
    auto_ptr<CNetCacheClient>    nc_client;

    CConfig conf(GetConfig());
    const CConfig::TParamTree* param_tree = conf.GetTree();
    const TPluginManagerParamTree* netschedule_tree = 
            param_tree->FindSubNode(kNetScheduleDriverName);

    if (!netschedule_tree) 
        return 1;

     ns_client.reset( 
            m_PM_NetSchedule.CreateInstance(
                    kNetScheduleDriverName,
                    CVersionInfo(TPMNetSchedule::TInterfaceVersion::eMajor,
                                 TPMNetSchedule::TInterfaceVersion::eMinor,
                                 TPMNetSchedule::TInterfaceVersion::ePatchLevel), 
                                 netschedule_tree)
                                 );

     param_tree = conf.GetTree();
     const TPluginManagerParamTree* netcache_tree = 
             param_tree->FindSubNode(kNetCacheDriverName);

    if (!netcache_tree)
        return 1;

    nc_client.reset( 
            m_PM_NetCache.CreateInstance(
                    kNetCacheDriverName,
                    CVersionInfo(TPMNetCache::TInterfaceVersion::eMajor,
                                 TPMNetCache::TInterfaceVersion::eMinor,
                                 TPMNetCache::TInterfaceVersion::ePatchLevel), 
                                 netcache_tree)
                       );

    CNetCacheNSStorage storage(nc_client);
    unsigned jcount = 10;
    int vsize = 100000;
    if (args["jcount"]) {
        jcount = args["jcount"].AsInteger();
    }
    if (args["vsize"]) {
        vsize = args["vsize"].AsInteger();
    }

    CNetScheduleClient::EJobStatus status;
    NcbiCout << "Submit " << jcount << " jobs..." << NcbiEndl;

    string job_key;
    string input1 = "Hello ";
    typedef map<string, vector<double>* > TJobs;
    TJobs jobs;
    for (unsigned i = 0; i < jcount; ++i) {
        string ns_key;
        vector<double>* dvec = new vector<double>;
        CNcbiOstream& os = storage.CreateOStream(ns_key);
        os << vsize << ' ';
        srand( (unsigned)time( NULL ) );
        for (int j = 0; j < vsize; ++j) {
            double d = rand()*0.2;
            os << d << ' ';
            dvec->push_back(d);
        }
        storage.Reset();
        string job_key = ns_client->SubmitJob(ns_key);
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
    
    unsigned last_jobs = 0;
    unsigned no_jobs_executes_cnt = 0;

    while (jobs.size()) {
        NON_CONST_ITERATE(TJobs, it, jobs) {
            const string& jk = it->first;
            vector<double>* dvec = it->second;
            string output;
            int ret_code;
            string err_msg;
            status = ns_client->GetStatus(jk, &ret_code, &output,&err_msg);

            if (status == CNetScheduleClient::eDone) {
                CNcbiIstream& is = storage.GetIStream(output);
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
                storage.Reset();
                if (resvec.size() == dvec->size()) {
                    for(int i = 0; i < resvec.size(); ++i) {
                        if( abs((*dvec)[i] - resvec[i]) > 0.001 ) {
                            LOG_POST( "Test failed! Wrong vector element." );
                            break;
                        }
                    }
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
            else if (status == CNetScheduleClient::eFailed) {
                LOG_POST( "Job " << jk << " failed : " << err_msg );
                delete dvec;
                jobs.erase(it);
                ++cnt;
                break;
            }

            /*else 
            if (status != CNetScheduleClient::ePending) {
                if (status == CNetScheduleClient::eJobNotFound) {
                    NcbiCerr << "Job lost:" << jk << NcbiEndl;
                }
                jobs.erase(it);
                ++cnt;
                break;                
            }*/
            
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
    return CGridClientSample().AppMain(argc, argv, 0, eDS_Default, "grid_client_sample.ini");
} 


/*
 * ===========================================================================
 * $Log$
 * Revision 1.1  2005/03/24 15:10:41  didenko
 * Added samples for Worker node framework
 *
 * ===========================================================================
 */
 
