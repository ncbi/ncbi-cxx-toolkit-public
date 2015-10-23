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
 * File Description:  ICache client
 *
 */

#include <ncbi_pch.hpp>

#include <connect/services/neticache_client.hpp>

#include <connect/ncbi_socket.hpp>
#include <connect/ncbi_types.h>
#include <connect/ncbi_core_cxx.hpp>

#include <corelib/ncbiapp.hpp>
#include <corelib/ncbiargs.hpp>
#include <corelib/ncbienv.hpp>
#include <corelib/ncbireg.hpp>
#include <corelib/ncbi_system.hpp>
#include <corelib/ncbimisc.hpp>


USING_NCBI_SCOPE;


///////////////////////////////////////////////////////////////////////


/// Test application for network ICache client
///
/// @internal
///
class CTestICClient : public CNcbiApplication
{
public:
    void Init(void);
    int Run(void);
};




void CTestICClient::Init(void)
{
    // Setup command line arguments and parameters

    // Create command-line argument descriptions class
    auto_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);

    // Specify USAGE context
    arg_desc->SetUsageContext(GetArguments().GetProgramBasename(),
        "Network ICache client test");

    arg_desc->AddKey("nc", "Service", "NetCache service name", CArgDescriptions::eString);

    arg_desc->AddKey("cache", "Cache", "Cache name", CArgDescriptions::eString);

    arg_desc->AddKey("key", "key", "ICache blob key", CArgDescriptions::eString);
    arg_desc->AddDefaultKey("subkey", "key", "ICache blob subkey", CArgDescriptions::eString, kEmptyStr);
    arg_desc->AddDefaultKey("version", "version", "ICache blob version", CArgDescriptions::eInteger, "0");

    // Setup arg.descriptions for this application
    SetupArgDescriptions(arg_desc.release());

    SetDiagPostLevel(eDiag_Info);
    SetDiagTrace(eDT_Enable);
}


int CTestICClient::Run(void)
{
    const CArgs& args = GetArgs();

    const string& nc = args["nc"].AsString();
    const string& cache_name = args["cache"].AsString();
    const string& key = args["key"].AsString();
    const string& subkey = args["subkey"].AsString();
    const Int4 version = args["version"].AsInteger();

    char buf[130000] = { 0, };

    CNetICacheClient cl(nc, cache_name, "test_icache");
    if (cl.HasBlobs(key, subkey)) {
        const size_t size = cl.GetSize(key, version, subkey);
        NcbiCerr << "blob size: " << size << endl;
        const bool complete = cl.Read(key, version, subkey, buf, sizeof(buf) - 1);
        NcbiCout << string(buf, size) << "\n";
        if (!complete) {
            NcbiCerr << "read incomplete\n";
        }
    }
    else {
        NcbiCerr << "key not found: " << key << endl;
    }

    return 0;
}


int main(int argc, const char* argv[])
{
    return CTestICClient().AppMain(argc, argv);
}
