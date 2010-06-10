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
 * Authors:  Maxim Didenko, Vladimir Ivanov, Anatoliy Kuznetsov
 *
 * File Description:  NetCache Sample
 *
 */

/// @file netcache_client_sample3.cpp
/// NetCache sample:
///    illustrates client creattion and simple store/load scenario
///    using C++ compatible streams and ZIP compression.
///    In some cases, compression can dramatically reduce
///    network and storage overhead of NetCache.
///


#include <ncbi_pch.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbiargs.hpp>
#include <corelib/ncbienv.hpp>
#include <corelib/ncbireg.hpp>
#include <corelib/ncbi_system.hpp>
#include <corelib/ncbimisc.hpp>
#include <util/compress/zlib.hpp>

#include <connect/services/netcache_api.hpp>

USING_NCBI_SCOPE;

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
    SetDiagPostFlag(eDPF_Trace);
    SetDiagPostLevel(eDiag_Info);

    // Setup command line arguments and parameters

    // Create command-line argument descriptions class
    auto_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);

    // Specify USAGE context
    arg_desc->SetUsageContext(GetArguments().GetProgramBasename(),
                              "NetCache Client Sample");

    arg_desc->AddPositional("service",
                             "LBSM service name or host:port",
                             CArgDescriptions::eString);

    // Setup arg.descriptions for this application
    SetupArgDescriptions(arg_desc.release());
}


int CSampleNetCacheClient::Run(void)
{
    const CArgs& args = GetArgs();

    string service_name = args["service"].AsString();
    CNetCacheAPI nc_client(service_name, "nc_client_sample3");

    const char test_data[] = "A quick brown fox, jumps over lazy dog.";

    // Store the BLOB
    string key;
    auto_ptr<CNcbiOstream> os(nc_client.CreateOStream(key));

    // initialize compression stream
    {{
        CCompressionOStream os_zip(*os, new CZipStreamCompressor(),
                                CCompressionStream::fOwnWriter);
        os_zip << test_data;
    }}

    os.reset();

    NcbiCout << key << NcbiEndl;

    SleepMilliSec(500);

    // Get the data back
    try {
        auto_ptr<CNcbiIstream> is(nc_client.GetIStream(key));
        string res;
        {{
            // construct decompression stream
            // decompression MUST match compression 
            // CZipStreamCompressor - CZipStreamDecompressor
            CCompressionIStream is_zip(*is, new CZipStreamDecompressor(),
                                       CCompressionStream::fOwnReader);
            getline(is_zip, res);
        }}
        NcbiCout << res << NcbiEndl;
    }
    catch (CNetServiceException& ex) {
        ERR_POST(ex.what());
        return 1;
    }
    return 0;
}


int main(int argc, const char* argv[])
{
    return CSampleNetCacheClient().AppMain(argc, argv);
}
