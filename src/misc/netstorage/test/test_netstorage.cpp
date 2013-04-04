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
 * Authors:  Dmitry Kazimirov
 *
 * File Description:  Simple test for NetStorage.
 *
 */

#include <ncbi_pch.hpp>

#include <misc/netstorage/netstorage.hpp>

#include <common/test_assert.h>  /* This header must go last */


USING_NCBI_SCOPE;


///////////////////////////////////////////////////////////////////////

static const char* s_AppName = "test_netstorage";

/// Test application
///
/// @internal
///
class CNetStorageTestApp : public CNcbiApplication
{
public:
    void Init();
    int Run();
};


void CNetStorageTestApp::Init()
{
    // Setup command line arguments and parameters

    // Create command-line argument descriptions class
    auto_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);

    // Specify USAGE context
    arg_desc->SetUsageContext(GetArguments().GetProgramBasename(),
            "CNetStorage test");

    arg_desc->AddPositional("ic_service", "NetCache service.",
            CArgDescriptions::eString);

    // Setup arg.descriptions for this application
    SetupArgDescriptions(arg_desc.release());
}

int CNetStorageTestApp::Run()
{
    int error_level = 0;

    CArgs args = GetArgs();

    CNetICacheClient icache_client(args["ic_service"].AsString(), s_AppName, s_AppName);

    const char test_data[] = "The quick brown fox jumps over the lazy dog.\n";
    const char test_data2[] = "More data.\n";

    CNetStorage netstorage(icache_client);

    string file1_id;

    {{
        CNetFile file1 = netstorage.Create();

        file1.Write(test_data);

        file1.Close();

        file1_id = file1.GetID();

        string data;
        data.reserve(10);

        file1.Read(&data);

        if (data != test_data) {
            ERR_POST("Read(string) is broken.");
            return 1;
        }
    }}
    printf("%s\n", file1_id.c_str());

    /*{{
        key = nc_client.PutData(NULL, 0, 120);
        NcbiCout << key << NcbiEndl;
        assert(!key.empty());

        SleepMilliSec(700);


        size_t bsize;
        auto_ptr<IReader> rdr(nc_client.GetData(key, &bsize));

        assert(rdr.get() != NULL);

        cout << bsize << endl;
        assert(bsize == 0);
    }}


    {{
        CNetCacheAPI nc_client(service, s_AppName);

        key = nc_client.PutData(test_data, sizeof(test_data));
        NcbiCout << key << NcbiEndl;
        assert(!key.empty());

        unsigned id = CNetCacheKey(key).GetId();
        CNetCacheKey pk(key);
        assert(pk.GetId() == id);

    }}

    {{
        CNetCacheAPI nc_client(service, s_AppName);

        char dataBuf[1024];
        memset(dataBuf, 0xff, sizeof(dataBuf));

        size_t blob_size;
        IReader* reader = nc_client.GetData(key, &blob_size);
        assert(reader);
        assert(blob_size == sizeof(test_data));

        size_t bytes_read = ReadIntoBuffer(reader, dataBuf, sizeof(dataBuf));
        delete reader;

        assert(bytes_read == sizeof(test_data));

        int res = memcmp(dataBuf, test_data, sizeof(test_data));
        assert(res == 0);

        reader = nc_client.GetPartReader(key,
            sizeof(test_data) - sizeof("dog."), sizeof(dataBuf), &blob_size);

        assert(blob_size == sizeof("dog."));

        bytes_read = ReadIntoBuffer(reader, dataBuf, sizeof(dataBuf));

        assert(bytes_read == sizeof("dog."));

        delete reader;

        res = strcmp(dataBuf, "dog.");
        assert(res == 0);
    }}

    {{
        CNetCacheAPI nc_client(s_AppName);

        char dataBuf[1024];
        memset(dataBuf, 0xff, sizeof(dataBuf));

        CNetCacheAPI::EReadResult rres =
            nc_client.GetData(key, dataBuf, sizeof(dataBuf));
        assert(rres == CNetCacheAPI::eReadComplete);

        int res = strcmp(dataBuf, test_data);
        assert(res == 0);

        string str;

        nc_client.ReadPart(key, sizeof("The ") - 1,
            sizeof("The quick") - sizeof("The "), str);

        assert(str == "quick");
    }}

    // update existing BLOB
    {{
        {
        CNetCacheAPI nc_client(service, s_AppName);
        nc_client.PutData(key, test_data2, sizeof(test_data2));
        }
        {
        CNetCacheAPI nc_client(service, s_AppName);
        char dataBuf[1024];
        memset(dataBuf, 0xff, sizeof(dataBuf));
        CNetCacheAPI::EReadResult rres =
            nc_client.GetData(key, dataBuf, sizeof(dataBuf));
        assert(rres == CNetCacheAPI::eReadComplete);
        int res = strcmp(dataBuf, test_data2);
        assert(res == 0);

        }
    }}


    // timeout test
    {{
        CNetCacheAPI nc_client(service, s_AppName);

        key = nc_client.PutData(test_data, sizeof(test_data), 80);
        assert(!key.empty());
    }}

    CNetCacheAPI nc_client(service, s_AppName);

    bool exists = s_CheckExists(nc_client, key);
    assert(exists);

    nc_client.Remove(key);

    SleepMilliSec(1800);

    exists = s_CheckExists(nc_client, key);
    assert(!exists);

    s_RemoveBLOB_Test(service);

    s_ReadUpdateCharTest(service);

    error_level |= s_PasswordTest(service);
    error_level |= s_AbortTest(service);

    vector<STransactionInfo> log;
    vector<STransactionInfo> log_read;
    vector<string>           rep_keys;

    unsigned repeats = 1000;*/

    return error_level;
}


int main(int argc, const char* argv[])
{
    SetDiagPostLevel(eDiag_Warning);

    return CNetStorageTestApp().AppMain(argc, argv, 0, eDS_Default);
}
