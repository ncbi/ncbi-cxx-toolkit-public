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
#include <connect/services/neticache_client.hpp>
#include <connect/services/grid_app_version_info.hpp>

#include <connect/ncbi_socket.hpp>
#include <common/ncbi_package_ver.h>

#ifdef WIN32
#include <io.h>
#include <fcntl.h>
#endif

#define GRID_APP_NAME "netcache_control"

USING_NCBI_SCOPE;

///////////////////////////////////////////////////////////////////////

/// Netcache stop application
///
/// @internal
///
class CNetCacheControl : public CNcbiApplication
{
public:
    void Init();
    int Run();

private:
    struct SICacheBlobAddress {
        string key;
        int version;
        string subkey;

        SICacheBlobAddress() : version(0) {}
    };
};



void CNetCacheControl::Init()
{
    // Setup command line arguments and parameters

    auto_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);

    arg_desc->SetUsageContext(GetArguments().GetProgramBasename(),
                              "NCBI NetCache control.");

    arg_desc->AddDefaultPositional("service_name",
        "NetCache service name.", CArgDescriptions::eString, kEmptyStr);

    arg_desc->AddDefaultKey("service", "service_name",
        "NetCache service name.", CArgDescriptions::eString, kEmptyStr);

    arg_desc->AddOptionalKey("auth", "name",
        "Authentication string (client name)", CArgDescriptions::eString);

    arg_desc->AddOptionalKey("icache", "cache_name",
        "Connect to an ICache database", CArgDescriptions::eString);

    arg_desc->AddFlag("reconf",        "Reload server configuration");
    arg_desc->AddFlag("reinit",        "Delete all blobs and reset DB");
    arg_desc->AddFlag("shutdown",      "Shutdown server");
    arg_desc->AddFlag("ver",           "Server version");
    arg_desc->AddFlag("getconf",       "Server config");
    arg_desc->AddFlag("health",        "Server health");
    arg_desc->AddFlag("stat",          "Server statistics");

    arg_desc->AddOptionalKey("fetch", "key",
        "Retrieve data by key", CArgDescriptions::eString);

    arg_desc->AddDefaultKey("offset", "position",
        "Read starting from this offset", CArgDescriptions::eInteger, "0");

    arg_desc->AddDefaultKey("part_size", "length",
        "Retrieve only that many bytes", CArgDescriptions::eInteger, "0");

    arg_desc->AddOptionalKey("outputfile", "file_name",
        "Send output to a file instead of stdout", CArgDescriptions::eString);

    arg_desc->AddOptionalKey("store", "key",
        "Store data in NetCache", CArgDescriptions::eString);

    arg_desc->AddOptionalKey("inputfile", "file_name",
        "Read input from a file instead of stdin", CArgDescriptions::eString);

    arg_desc->AddOptionalKey("password", "password",
        "Password for use with -store and -fetch", CArgDescriptions::eString);

    arg_desc->AddOptionalKey("size", "key",
        "Get BLOB size", CArgDescriptions::eString);

    arg_desc->AddOptionalKey("remove", "key",
        "Delete blob by key", CArgDescriptions::eString);

    arg_desc->AddOptionalKey("blobinfo", "key",
        "Retrieve meta information on the blob", CArgDescriptions::eString);

    arg_desc->PrintUsageIfNoArgs();

    SetupArgDescriptions(arg_desc.release());
}

#define REQUIRES_KEY 0x100
#define REQUIRES_ADMIN 0x200

enum {
    eCmdFetch = 0x01 | REQUIRES_KEY,
    eCmdStore = 0x02 | REQUIRES_KEY,
    eCmdSize = 0x03 | REQUIRES_KEY,
    eCmdRemove = 0x04 | REQUIRES_KEY,
    eCmdBlobInfo = 0x05 | REQUIRES_KEY,
    eCmdGetConf = 0x06 | REQUIRES_ADMIN,
    eCmdHealth = 0x07 | REQUIRES_ADMIN,
    eCmdStat = 0x08 | REQUIRES_ADMIN,
    eCmdVer = 0x09 | REQUIRES_ADMIN,
    eCmdShutdown = 0x0A | REQUIRES_ADMIN,
    eCmdReconf = 0x0B | REQUIRES_ADMIN,
    eCmdReinit = 0x0C | REQUIRES_ADMIN
};

int CNetCacheControl::Run()
{
    const CArgs& args = GetArgs();
    string service(args["service"].AsString());

    if (service.empty())
        service = args["service_name"].AsString();

    bool icache_mode = args["icache"].HasValue();

    const CArgValue& password_arg = args["password"];

    int cmd;
    string key;

    size_t offset = 0;
    size_t part_size = 0;
    int reader_select = -1;

    if (args["fetch"].HasValue()) {
        key = args["fetch"].AsString();
        cmd = eCmdFetch;
        offset = (size_t) args["offset"].AsInt8();
        part_size = (size_t) args["part_size"].AsInt8();
        reader_select = (password_arg.HasValue() << 1) |
            (offset != 0 || part_size != 0);
    } else if (args["store"]) {
        key = args["store"].AsString();
        cmd = eCmdStore;
    } else if (args["size"]) {
        key = args["size"].AsString();
        cmd = eCmdSize;
    } else if (args["remove"].HasValue()) {
        key = args["remove"].AsString();
        cmd = eCmdRemove;
    } else if (args["blobinfo"].HasValue()) {
        key = args["blobinfo"].AsString();
        cmd = eCmdBlobInfo;
    } else {
        cmd = args["getconf"] ? eCmdGetConf :
            args["health"] ? eCmdHealth :
            args["stat"] ? eCmdStat :
            args["ver"] ? eCmdVer :
            args["shutdown"] ? eCmdShutdown :
            args["reconf"] ? eCmdReconf :
            args["reinit"] ? eCmdReinit : 0;
    }

    SICacheBlobAddress blob_address;
    bool version_is_defined = true;

    string client_name = args["auth"].HasValue() ?
        args["auth"].AsString() : "netcache_control";

    CNetCacheAPI nc_client;
    CNetCacheAdmin admin;
    CNetICacheClient icache_client(eVoid);

    if (cmd & REQUIRES_ADMIN)
        admin = CNetCacheAPI(service, client_name).GetAdmin();
    else if (!icache_mode) {
        nc_client = CNetCacheAPI(service, client_name);
        if (!key.empty() && !service.empty()) {
            string host, port;

            if (NStr::SplitInTwo(service, ":", host, port))
                nc_client.GetService().GetServerPool().StickToServer(
                    host, (unsigned short) NStr::StringToInt(port));
            else {
                NCBI_THROW(CArgException, eInvalidArg,
                    "This operation requires the \"service_name\" "
                    "argument to be a host:port server address.");
            }
        }
    } else {
        icache_client = CNetICacheClient(service,
            args["icache"].AsString(), client_name);

        if (service.empty()) {
            NCBI_THROW(CArgException, eNoValue, "The \"service\" "
                "argument is required in icache mode.");
        }

        if (cmd & REQUIRES_KEY) {
            vector<string> key_parts;

            NStr::Tokenize(key, ",", key_parts);
            if (key_parts.size() != 3) {
                NCBI_THROW_FMT(CArgException, eInvalidArg,
                    "Invalid ICache key specification \"" << key << "\" ("
                    "expected a comma-separated key,version,subkey triplet).");
            }
            if (!key_parts[1].empty())
                blob_address.version = NStr::StringToInt(key_parts[1]);
            else if (reader_select == 0)
                version_is_defined = false;
            else {
                NCBI_THROW(CArgException, eNoValue,
                    "blob version parameter is missing");
            }
            blob_address.key = key_parts.front();
            blob_address.subkey = key_parts.back();
        }
    }

    switch (cmd) {
    case eCmdFetch:
        {{
        auto_ptr<IReader> reader;
        if (!icache_mode) {
            size_t blob_size = 0;
            switch (reader_select) {
            case 0: /* no special case */
                reader.reset(nc_client.GetReader(key, &blob_size,
                    CNetCacheAPI::eCaching_Disable));
                break;
            case 1: /* use offset */
                reader.reset(nc_client.GetPartReader(key, offset, part_size,
                    &blob_size, CNetCacheAPI::eCaching_Disable));
                break;
            case 2: /* use password */
                reader.reset(CNetCachePasswordGuard(nc_client,
                    password_arg.AsString())->GetReader(key, &blob_size,
                    CNetCacheAPI::eCaching_Disable));
                break;
            case 3: /* use password and offset */
                reader.reset(CNetCachePasswordGuard(nc_client,
                    password_arg.AsString())->GetPartReader(key, offset,
                    part_size, &blob_size, CNetCacheAPI::eCaching_Disable));
            }
        } else {
            ICache::EBlobVersionValidity validity;
            switch (reader_select) {
            case 0: /* no special case */
                reader.reset(version_is_defined ?
                    icache_client.GetReadStream(blob_address.key,
                        blob_address.version, blob_address.subkey, NULL,
                        CNetCacheAPI::eCaching_Disable) :
                    icache_client.GetReadStream(blob_address.key,
                        blob_address.subkey, &blob_address.version, &validity));
                break;
            case 1: /* use offset */
                reader.reset(icache_client.GetReadStreamPart(blob_address.key,
                    blob_address.version, blob_address.subkey, offset,
                    part_size, NULL, CNetCacheAPI::eCaching_Disable));
                break;
            case 2: /* use password */
                reader.reset(CNetICachePasswordGuard(icache_client,
                    password_arg.AsString())->GetReadStream(blob_address.key,
                    blob_address.version, blob_address.subkey, NULL,
                    CNetCacheAPI::eCaching_Disable));
                break;
            case 3: /* use password and offset */
                reader.reset(CNetICachePasswordGuard(icache_client,
                    password_arg.AsString())->GetReadStreamPart(
                    blob_address.key, blob_address.version, blob_address.subkey,
                    offset, part_size, NULL, CNetCacheAPI::eCaching_Disable));
            }
            if (!version_is_defined)
                NcbiCerr << "Blob version: " <<
                        blob_address.version << NcbiEndl <<
                    "Blob validity: " << (validity == ICache::eCurrent ?
                        "current" : "expired") << NcbiEndl;
        }
        if (!reader.get()) {
            NCBI_THROW(CNetCacheException, eBlobNotFound,
                "Cannot find the requested blob");
        }

        FILE* output;
        if (args["outputfile"].HasValue()) {
            output = fopen(args["outputfile"].AsString().c_str(), "wb");
            if (output == NULL) {
                NCBI_USER_THROW_FMT("Could not open \"" <<
                    args["outputfile"].AsString() << "\" for output.");
            }
        } else {
            output = stdout;
#ifdef WIN32
            setmode(fileno(stdout), O_BINARY);
#endif
        }

        char buffer[16 * 1024];
        ERW_Result read_result;
        size_t bytes_read;

        while ((read_result = reader->Read(buffer,
            sizeof(buffer), &bytes_read)) == eRW_Success) {

            fwrite(buffer, 1, bytes_read, output);
        }

        if (read_result != eRW_Eof) {
            ERR_POST("Error while sending data to the output stream");
            return 1;
        }
        }}
        break;

    case eCmdStore:
        {{
        auto_ptr<IEmbeddedStreamWriter> writer(!icache_mode ?
            password_arg.HasValue() ? CNetCachePasswordGuard(nc_client,
                password_arg.AsString())->PutData(&key) :
                    nc_client.PutData(&key) :
            password_arg.HasValue() ? CNetICachePasswordGuard(icache_client,
                password_arg.AsString())->GetNetCacheWriter(blob_address.key,
                    blob_address.version, blob_address.subkey) :
                icache_client.GetNetCacheWriter(blob_address.key,
                    blob_address.version, blob_address.subkey));

        if (!writer.get()) {
            NCBI_USER_THROW_FMT("Cannot create blob stream");
        }

        FILE* input;
        if (args["inputfile"].HasValue()) {
            input = fopen(args["inputfile"].AsString().c_str(), "rb");
            if (input == NULL) {
                NCBI_USER_THROW_FMT("Could not open \"" <<
                    args["inputfile"].AsString() << "\" for input.");
            }
        } else {
            input = stdin;
#ifdef WIN32
            setmode(fileno(stdin), O_BINARY);
#endif
        }

        char buffer[16 * 1024];
        size_t bytes_read;
        size_t bytes_written;

        while ((bytes_read = fread(buffer, 1, sizeof(buffer), input)) > 0) {
            if (writer->Write(buffer, bytes_read, &bytes_written) !=
                    eRW_Success || bytes_written != bytes_read) {
                NCBI_USER_THROW_FMT("Error while writing to NetCache");
            }
        }

        writer->Close();

        if (!icache_mode)
            NcbiCout << key << NcbiEndl;
        }}
        break;

    case eCmdSize:
        NcbiCout << "BLOB size: " <<
            (!icache_mode ? nc_client.GetBlobSize(key) :
                icache_client.GetSize(blob_address.key,
                    blob_address.version, blob_address.subkey)) << NcbiEndl;
        break;

    case eCmdRemove:
        if (!icache_mode)
            if (password_arg.HasValue())
                CNetCachePasswordGuard(nc_client,
                    password_arg.AsString())->Remove(key);
            else
                nc_client.Remove(key);
        else
            if (password_arg.HasValue())
                CNetICachePasswordGuard(icache_client,
                    password_arg.AsString())->Remove(blob_address.key,
                        blob_address.version, blob_address.subkey);
            else
                icache_client.Remove(blob_address.key,
                    blob_address.version, blob_address.subkey);
        break;

    case eCmdBlobInfo:
        if (!icache_mode)
            nc_client.PrintBlobInfo(key);
        else
            icache_client.PrintBlobInfo(blob_address.key,
                blob_address.version, blob_address.subkey);
        break;

    case eCmdGetConf:
        admin.PrintConfig(NcbiCout);
        break;

    case eCmdHealth:
        admin.PrintHealth(NcbiCout);
        break;

    case eCmdStat:
        admin.PrintStat(NcbiCout);
        break;

    case eCmdVer:
        admin.GetServerVersion(NcbiCout);
        break;

    case eCmdShutdown:
        admin.ShutdownServer();
        NcbiCout << "Shutdown request has been sent to server" << NcbiEndl;
        break;

    case eCmdReconf:
        admin.ReloadServerConfig();
        NcbiCout << "Reconfigured." << NcbiEndl;
        break;

    case eCmdReinit:
        if (icache_mode)
            admin.Reinitialize(args["icache"].AsString());
        else
            admin.Reinitialize();
        NcbiCout << "Reinitialized." << NcbiEndl;
    }

    return 0;
}


int main(int argc, const char* argv[])
{
    GRID_APP_CHECK_VERSION_ARGS();

    return CNetCacheControl().AppMain(argc, argv, 0, eDS_Default, 0);
}
