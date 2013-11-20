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
 * File Description:  Network ICache client test
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

#include <common/test_assert.h>  /* This header must go last */


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

    arg_desc->AddDefaultKey("service", "Service", "NetCache service name.",
        CArgDescriptions::eString, kEmptyStr);

    arg_desc->AddOptionalKey("hostname", "Host",
        "NetCache host name.", CArgDescriptions::eString);

    arg_desc->AddOptionalKey("port", "Port",
        "Port number.", CArgDescriptions::eInteger);

    arg_desc->AddDefaultPositional("cache", "Cache name.",
        CArgDescriptions::eString, kEmptyStr);

    // Setup arg.descriptions for this application
    SetupArgDescriptions(arg_desc.release());

    SetDiagPostLevel(eDiag_Info);
    SetDiagTrace(eDT_Enable);
}


int CTestICClient::Run(void)
{
    CArgs args = GetArgs();

    string service(args["service"].AsString());
    string cache_name(args["cache"].AsString());

    CNetICacheClient cl(args["hostname"].HasValue() ?
        CNetICacheClient(args["hostname"].AsString(),
            args["port"].AsInteger(),
            cache_name, "test_icache") :
        CNetICacheClient(args["service"].AsString(),
            cache_name, "test_icache"));

    const static char test_data[] =
            "The quick brown fox jumps over the lazy dog.";
    const static size_t data_size = sizeof(test_data) - 1;

    string uid = GetDiagContext().GetStringUID();
    string key1 = "TEST_IC_CLIENT_KEY1_" + uid;
    string key2 = "TEST_IC_CLIENT_KEY2_" + uid;
    string subkey = "TEST_IC_CLIENT_SUBKEY_" + uid;
    int version = 0;

    {{
    cl.Store(key1, version, subkey, test_data, data_size);
    SleepMilliSec(700);

    bool hb = cl.HasBlobs(key1, subkey);
    assert(hb);

    size_t sz = cl.GetSize(key1, version, subkey);
    assert(sz == data_size);

    char buf[1024] = {0,};
    cl.Read(key1, version, subkey, buf, sizeof(buf) - 1);

    assert(strcmp(buf, test_data) == 0);

    memset(buf, 0, sizeof(buf));
    cl.Read(key1, version, subkey, buf, sizeof(buf) - 1);

    assert(strcmp(buf, test_data) == 0);

    memset(buf, 0, sizeof(buf));
    cl.ReadPart(key1, version, subkey, sizeof("The ") - 1,
        sizeof("The quick") - sizeof("The "), buf, sizeof(buf) - 1);

    assert(strcmp(buf, "quick") == 0);


    sz = cl.GetSize(key1, version, subkey);
    assert(sz == data_size);
    hb = cl.HasBlobs(key1, subkey);
    assert(hb);

    cl.Remove(key1, version, subkey);
    hb = cl.HasBlobs(key1, subkey);
    assert(!hb);

    }}

    {{
    size_t test_size = 1024 * 1024 * 2;
    unsigned char *test_buf = new unsigned char[test_size+10];
    for (size_t i = 0; i < test_size; ++i) {
        test_buf[i] = 127;
    }

    cl.Store(key2, version, subkey, test_buf, test_size);

    for (size_t i = 0; i < test_size; ++i) {
        test_buf[i] = 0;
    }
    SleepMilliSec(700);

    size_t sz = cl.GetSize(key2, version, subkey);
    assert(sz == test_size);


    cl.Read(key2, version, subkey, test_buf, test_size);

    for (size_t i = 0; i < test_size; ++i) {
        if (test_buf[i] != 127) {
            assert(0);
        }
    }

    sz = cl.GetSize(key2, version, subkey);
    assert(sz == test_size);

    cl.Remove(key2, version, subkey);
    bool hb = cl.HasBlobs(key2, subkey);
    assert(!hb);
    }}


    NcbiCout << "stress write" << NcbiEndl;

    // stress write
    {{
    char test_buf[240];
    size_t test_size = sizeof(test_buf);
    for (int i = 0; i < 100; ++i) {
        string key = "TEST_IC_CLIENT_KEY_" + NStr::IntToString(i) + "_" + uid;
        cl.Store(key, 0, subkey, test_buf, test_size);
        cl.Remove(key, 0, subkey);
    }
    }}

    NcbiCout << "check reader/writer" << NcbiEndl;
    {{
        {
        auto_ptr<IWriter> writer1(cl.GetWriteStream(key1, version, subkey));
        string str = "qwerty";
        writer1->Write(str.c_str(), str.size());
        }
        size_t size = cl.GetSize(key1, version, subkey);
        vector<unsigned char> test_buf(1000);
        cl.Read(key1, version, subkey, &test_buf[0], test_buf.size());
        cout << size << endl << string((char*)&test_buf[0],
            test_buf.size()) << endl;
        cl.Remove(key1, version, subkey);
    }}

#ifdef DISABLED_UNTIL_NC_WITH_AGE_SUPPORT_IS_DEPLOYED
    NcbiCout << "blob age test" << NcbiEndl;
    {{
        ++version;
        cl.Store(key1, version, subkey, test_data, data_size);

        char buffer[1024];

        ICache::SBlobAccessDescr blob_access(buffer, sizeof(buffer));

        blob_access.return_current_version = true;
        blob_access.maximum_age = 5;

        SleepSec(2);

        cl.GetBlobAccess(key1, 0, subkey, &blob_access);

        assert(blob_access.blob_found);
        assert(blob_access.reader.get() == NULL);
        assert(blob_access.blob_size < blob_access.buf_size);
        assert(memcmp(blob_access.buf, test_data, data_size) == 0);
        assert(blob_access.current_version == version);
        assert(blob_access.current_version_validity == ICache::eCurrent);
        assert(blob_access.actual_age >= 2);

        blob_access.return_current_version = false;
        blob_access.maximum_age = 2;

        SleepSec(1);

        cl.GetBlobAccess(key1, version, subkey, &blob_access);

        assert(!blob_access.blob_found);

        cl.Remove(key1, version, subkey);
    }}
#endif /* DISABLED_UNTIL_NC_WITH_AGE_SUPPORT_IS_DEPLOYED */

    return 0;
}


int main(int argc, const char* argv[])
{
    return CTestICClient().AppMain(argc, argv, 0, eDS_Default);
}
