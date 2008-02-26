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
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbiargs.hpp>
#include <corelib/ncbienv.hpp>
#include <corelib/ncbireg.hpp>
#include <corelib/ncbi_system.hpp>
#include <corelib/ncbimisc.hpp>

#include <connect/services/neticache_client.hpp>
#include <connect/ncbi_socket.hpp>
#include <connect/ncbi_types.h>
#include <connect/ncbi_core_cxx.hpp>
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

    arg_desc->AddPositional("hostname",
                            "NetCache host name.", CArgDescriptions::eString);

    arg_desc->AddPositional("port",
                            "Port number.",
                            CArgDescriptions::eInteger);

    arg_desc->AddPositional("cache",
                            "Cache name.",
                            CArgDescriptions::eString);


    // Setup arg.descriptions for this application
    SetupArgDescriptions(arg_desc.release());

//    CONNECT_Init(&GetConfig());

    SetDiagPostLevel(eDiag_Info);
    SetDiagTrace(eDT_Enable);
}






int CTestICClient::Run(void)
{
    CArgs args = GetArgs();
    const string& host  = args["hostname"].AsString();
    unsigned short port = args["port"].AsInteger();
    const string& cache_name  = args["cache"].AsString();


    const char test_data[] = "A quick brown fox, jumps over lazy dog.";
    string key;

    CNetICacheClient cl(host, port, cache_name, "test_icache");

    ICache::TTimeStampFlags flags =
        ICache::fTimeStampOnRead | ICache::fTrackSubKey;
    cl.SetTimeStampPolicy(flags, 1800, 0);

    ICache::TTimeStampFlags fl = cl.GetTimeStampPolicy();
    assert(flags == fl);

    ICache::EKeepVersions vers = ICache::eDropOlder;
    cl.SetVersionRetention(vers);
    ICache::EKeepVersions v = cl.GetVersionRetention();
    assert(vers == v);

    bool b = cl.IsOpen();
    assert(b == true);

    {{
    string key = "k1";
    int version = 0;
    string subkey = "sk";

    size_t data_size = strlen(test_data) + 1;

    cl.Store(key, version, subkey, test_data, data_size);
    SleepMilliSec(700);

    bool hb = cl.HasBlobs(key, subkey);
    assert(hb);

    size_t sz = cl.GetSize(key, version, subkey);
    assert(sz == data_size);

    string owner;
    cl.GetBlobOwner(key, version, subkey, &owner);

    char buf[1024] = {0,};
    cl.Read(key, version, subkey, buf, sizeof(buf));

    assert(strcmp(buf, test_data) == 0);

    memset(buf, 0, sizeof(buf));
    cl.Read(key, version, subkey, buf, sizeof(buf));

    assert(strcmp(buf, test_data) == 0);


    sz = cl.GetSize(key, version, subkey);
    assert(sz == data_size);
    hb = cl.HasBlobs(key, subkey);
    assert(hb);


    }}

    {{
    string key = "k2";
    int version = 0;
    string subkey = "sk";

    size_t test_size = 1024 * 1024 * 2;
    unsigned char *test_buf = new unsigned char[test_size+10];
    for (size_t i = 0; i < test_size; ++i) {
        test_buf[i] = 127;
    }

    cl.Store(key, version, subkey, test_buf, test_size);

    for (size_t i = 0; i < test_size; ++i) {
        test_buf[i] = 0;
    }
    SleepMilliSec(700);

    size_t sz = cl.GetSize(key, version, subkey);
    assert(sz == test_size);


    cl.Read(key, version, subkey, test_buf, test_size);

    for (size_t i = 0; i < test_size; ++i) {
        if (test_buf[i] != 127) {
            assert(0);
        }
    }

    sz = cl.GetSize(key, version, subkey);
    assert(sz == test_size);

    }}


    NcbiCout << "stress write" << NcbiEndl;

    // stress write
    {{
    string key, subkey;
    char test_buf[240];
    size_t test_size = sizeof(test_buf);
    for (int i = 0; i < 100; ++i) {
        key = NStr::IntToString(i);
        cl.Store(key, 0, subkey, test_buf, test_size);
    }
    }}

    NcbiCout << "check reader/writer" << NcbiEndl;
    {{
        int version = 0;
        string key1 = "key_1", subkey1 = "subkey_1";
        string key2 = "key_2", subkey2 = "subkey_2";

        {
        auto_ptr<IWriter> writer1(cl.GetWriteStream(key1,version,subkey1));
        //        CWStream ostr1(writer1.get());
        //auto_ptr<IWriter> writer2(cl.GetWriteStream(key2,version,subkey2));
        //CWStream ostr2(writer2.get());

        //ostr1 << 1234 << " ";
        //ostr2 << 4321 << " ";

        //        ostr1 << "qwerty";
        //ostr2 << "ytrewq";
        string str = "qwerty";
               writer1->Write(str.c_str(), str.size());
               //                cl.Store(key1, version, subkey1, str.c_str(), str.size());
        }
               int size = cl.GetSize(key1,version, subkey1);
               vector<unsigned char> test_buf(1000);
               cl.Read(key1, version, subkey1, &test_buf[0], test_buf.size());
               cout << size << endl << string((char*)&test_buf[0], test_buf.size()) << endl;
        {
            //        auto_ptr<IReader> reader1(cl.GetReadStream(key1,version,subkey1));
            //        CRStream istr1(reader1.get());
        //auto_ptr<IReader> reader2(cl.GetReadStream(key2,version,subkey2));
        //CRStream istr2(reader2.get());

        //int res = 0;
        //istr1 >> res;
        //cout << "From istr1 : " << res << endl;
        //assert(res == 1234);
        //istr2 >> res;
        //cout << "From istr2 : " << res << endl;
        //assert(res == 4321);
        //string str;
        //        istr1 >> str;
        //        cout << "From istr1 : " << str << endl;
        //istr2 >> str;
        //cout << "From istr2 : " << str << endl;
        }

    }}

    cl.Purge(0, ICache::eDropAll);

    NcbiCout << "Session management test" << endl;
    cl.RegisterSession(10);
    cl.UnRegisterSession(10);

    return 0;
}


int main(int argc, const char* argv[])
{
    return CTestICClient().AppMain(argc, argv, 0, eDS_Default, 0);
}
