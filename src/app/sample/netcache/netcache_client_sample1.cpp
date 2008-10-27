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
 * File Description:  NetCache Sample
 *
 */

/// @file netcache_client_sample1.cpp
/// NetCache sample: 
///    illustrates client construction and simple store/load scenario.


#include <ncbi_pch.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbiargs.hpp>
#include <corelib/ncbienv.hpp>
#include <corelib/ncbireg.hpp>
#include <corelib/ncbi_system.hpp>
#include <corelib/ncbimisc.hpp>
#include <corelib/rwstream.hpp>

#include <connect/services/netcache_api.hpp>
#include <util/simple_buffer.hpp>


USING_NCBI_SCOPE;

    
///////////////////////////////////////////////////////////////////////


/// Sample application
///
/// @internal
///
class CSampleNetCacheClient : public CNcbiApplication
{
public:
    void Init(void);
    int Run(void);
};



void CSampleNetCacheClient::Init(void)
{
    //SetDiagPostFlag(eDPF_Trace);
    //SetDiagPostLevel(eDiag_Info);
    
    // Setup command line arguments and parameters

    // Create command-line argument descriptions class
    auto_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);

    // Specify USAGE context
    arg_desc->SetUsageContext(GetArguments().GetProgramBasename(),
                              "NetCache Client Sample");

    arg_desc->AddOptionalKey("service",
                             "service",
                             "LBSM service name",
                             CArgDescriptions::eString);


    
    // Setup arg.descriptions for this application
    SetupArgDescriptions(arg_desc.release());
}


int CSampleNetCacheClient::Run(void)
{
    auto_ptr<CNetCacheAPI> nc_client;

    const CArgs& args = GetArgs();

    if (args["service"]) {
        // create load balanced client
        string service_name = args["service"].AsString();
        nc_client.reset(new CNetCacheAPI(service_name, "nc_client_sample1"));
        nc_client->SetConnMode(CNetCacheAPI::eKeepConnection);
    } 
    else {
        ERR_POST("Invalid network address. Use -service option.");
        return 1;
    }

    typedef vector<IWriter*> TWriters;
    TWriters writers;
    typedef vector<IReader*> TReaders;
    TReaders readers;
    vector<string> keys;


    const char test_data[] = "A quick brown fox, jumps over lazy dog.";

    // Store the BLOB
    string key = nc_client->PutData(test_data, strlen(test_data)+1);
    NcbiCout << key << NcbiEndl;
    
    int i;
    for(i = 0; i < 5; ++i)
    {
        string key;
        writers.push_back(nc_client->PutData(&key));
        keys.push_back(key);
    }
    i = 0;
    for(TWriters::iterator it = writers.begin(); it != writers.end(); ++it) {
        {
        CWStream s(*it);
        s << test_data << "  " << ++i;
        }
        delete *it;
    }
    writers.clear();

    SleepMilliSec(500);


    // retrieve BLOB
    CSimpleBuffer bdata;
    CNetCacheAPI::EReadResult rres = nc_client->GetData(key, bdata);
    if (rres == CNetCacheAPI::eNotFound) {
        ERR_POST("Blob not found");
        return 1;
    }

    NcbiCout << bdata.data() << NcbiEndl;

    for(vector<string>::const_iterator its = keys.begin(); its != keys.end(); ++its) {
        readers.push_back(nc_client->GetData(*its));
    }
    for(TReaders::iterator it = readers.begin(); it != readers.end(); ++it) {
        {
        CRStream s(*it);
        cout << s.rdbuf() << endl;
        }
        delete *it;
    }
    readers.clear();

    return 0;
}


int main(int argc, const char* argv[])
{
    return CSampleNetCacheClient().AppMain(argc, argv);
}
