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
 * Authors:  Anatoliy Kuznetsov, Dmitry Kazimirov
 *
 * File Description:  NetCache administration utility
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbistl.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbiargs.hpp>
#include <corelib/ncbienv.hpp>
#include <corelib/ncbireg.hpp>
#include <corelib/ncbi_system.hpp>
#include <corelib/ncbimisc.hpp>
#include <corelib/rwstream.hpp>

#include <connect/services/netcache_api.hpp>
#include <connect/ncbi_socket.hpp>
#include <common/ncbi_package_ver.h>

#ifdef WIN32
#include <io.h>
#include <fcntl.h>
#endif

#define NETCACHE_CONTROL_VERSION_MAJOR 1
#define NETCACHE_CONTROL_VERSION_MINOR 0
#define NETCACHE_CONTROL_VERSION_PATCH 0

USING_NCBI_SCOPE;

///////////////////////////////////////////////////////////////////////

/// Netcache stop application
///
/// @internal
///
class CNetCacheControl : public CNcbiApplication
{
public:
    CNetCacheControl() {
        SetVersion(CVersionInfo(
            NETCACHE_CONTROL_VERSION_MAJOR,
            NETCACHE_CONTROL_VERSION_MINOR,
            NETCACHE_CONTROL_VERSION_PATCH));
    }
    void Init();
    int Run();
};



void CNetCacheControl::Init()
{
    // Setup command line arguments and parameters

    auto_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);

    arg_desc->SetUsageContext(GetArguments().GetProgramBasename(),
                              "NCBI NetCache control.");
    
    arg_desc->AddDefaultPositional("service", 
        "NetCache service name.", CArgDescriptions::eString, "");

    arg_desc->AddFlag("shutdown",      "Shutdown server");
    arg_desc->AddFlag("ver",           "Server version");
    arg_desc->AddFlag("getconf",       "Server config");
    arg_desc->AddFlag("health",        "Server health");
    arg_desc->AddFlag("stat",          "Server statistics");
    arg_desc->AddFlag("monitor",       "Monitor server");

    arg_desc->AddOptionalKey("fetch", "key",
        "Retrieve data by key", CArgDescriptions::eString);

    arg_desc->AddOptionalKey("owner", "owner",
        "Get BLOB's owner", CArgDescriptions::eString);

    arg_desc->AddOptionalKey("size", "key",
        "Get BLOB size", CArgDescriptions::eString);

    SetupArgDescriptions(arg_desc.release());
}


int CNetCacheControl::Run()
{
    const CArgs& args = GetArgs();
    const string& service  = args["service"].AsString();

    CNetCacheAPI nc_client(service, "netcache_control");
    CNetCacheAdmin admin = nc_client.GetAdmin();

    if (args["fetch"]) {
        string key = args["fetch"].AsString();
        size_t blob_size = 0;
        auto_ptr<IReader> reader(nc_client.GetData(key, &blob_size));
        if (!reader.get()) {
            ERR_POST("Cannot retrieve data");
            return 1;
        }

#ifdef WIN32
        setmode(fileno(stdout), O_BINARY);
#endif

        char buffer[16 * 1024];

        ERW_Result read_result;
        size_t bytes_read;

        while ((read_result = reader->Read(buffer,
            sizeof(buffer), &bytes_read)) == eRW_Success) {

            fwrite(buffer, 1, bytes_read, stdout);
        }

        if (read_result != eRW_Eof) {
            ERR_POST("Error while sending data to stdout");
            return 1;
        }

        return 0;
    }

    if (args["owner"])
        NcbiCout << "BLOB belongs to: [" <<
            nc_client.GetOwner(args["owner"].AsString()) << "]" << NcbiEndl;

    if (args["size"])
        NcbiCout << "BLOB size: " <<
            nc_client.GetBlobSize(args["size"].AsString()) << NcbiEndl;

    if (args["getconf"])
        admin.PrintConfig(NcbiCout);

    if (args["health"])
        admin.PrintHealth(NcbiCout);

    if (args["monitor"])
        admin.Monitor(NcbiCout);

    if (args["stat"])
        admin.PrintStat(NcbiCout);

    if (args["ver"])
        admin.GetServerVersion(NcbiCout);

    if (args["shutdown"]) {
        admin.ShutdownServer();
        NcbiCout << "Shutdown request has been sent to server" << NcbiEndl;
        return 0;
    }

    return 0;
}


int main(int argc, const char* argv[])
{
    return CNetCacheControl().AppMain(argc, argv, 0, eDS_Default, 0);
}
