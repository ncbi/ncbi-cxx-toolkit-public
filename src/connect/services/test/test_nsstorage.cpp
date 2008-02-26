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
 * Authors:  Maxim Didenko
 *
 * File Description:  NetSchedule Storage test
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbiargs.hpp>
#include <corelib/ncbireg.hpp>

#include <corelib/blob_storage.hpp>

#include <common/test_assert.h>  /* This header must go last */


USING_NCBI_SCOPE;


///////////////////////////////////////////////////////////////////////


/// Test application
///
/// @internal
///
class CTestNSStorage : public CNcbiApplication
{
public:
    void Init(void);
    int Run(void);
};



void CTestNSStorage::Init(void)
{
    // Setup command line arguments and parameters

    // Create command-line argument descriptions class
    auto_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);

    // Specify USAGE context
    arg_desc->SetUsageContext(GetArguments().GetProgramBasename(),
                              "NetSchedule storage");

    arg_desc->AddPositional("service",
                            "NetCache service name.", CArgDescriptions::eString);

    arg_desc->AddOptionalKey("protocol",
                             "protocol",
                             "NetCache client protocl",
                             CArgDescriptions::eString);
    arg_desc->SetConstraint("protocol",
                            &(*new CArgAllow_Strings(NStr::eNocase),
                              "simple", "persistent")
                            );

    // Setup arg.descriptions for this application
    SetupArgDescriptions(arg_desc.release());

//    CONNECT_Init(&GetConfig());

//    SetDiagPostLevel(eDiag_Info);
//    SetDiagTrace(eDT_Enable);
}

const string kDriverName = "netcache_client";

int CTestNSStorage::Run(void)
{
    const CArgs& args = GetArgs();
    const string& service  = args["service"].AsString();

    CNcbiRegistry reg;
    reg.Set(kDriverName, "service", service);
    reg.Set(kDriverName, "client_name", "test_blobstorage_netcache");
    if (args["protocol"])
	reg.Set(kDriverName, "protocol", args["protocol"].AsString());

    CBlobStorageFactory factory(reg);
    auto_ptr<IBlobStorage> storage1(factory.CreateInstance());
    auto_ptr<IBlobStorage> storage2(factory.CreateInstance());

    string blobid;
    CNcbiOstream& os = storage1->CreateOStream(blobid);
    os << "Test_date";
    size_t blobsize;

    try {
        /*CNcbiIstream& is = */storage2->GetIStream(blobid, &blobsize,
                               IBlobStorage::eLockNoWait );
	throw runtime_error("The blob \"" + blobid + "\" must be locked. Server error.");
    } catch( CBlobStorageException& ex ) {
        if( ex.GetErrCode() == CBlobStorageException::eBlocked ) {
            cout << "Blob : " << blobid << " is blocked" << endl;
        } else {
            throw;
        }
    }


    storage1.reset(0);

    cout << "Second attempt." << endl;

    CNcbiIstream& is = storage2->GetIStream(blobid, &blobsize);
    string res;
    is >> res;
    cout << res << endl;

    return 0;
}


extern "C"
{
void BlobStorage_RegisterDriver_NetCache(void);
}

int main(int argc, const char* argv[])
{
    BlobStorage_RegisterDriver_NetCache();
    return CTestNSStorage().AppMain(argc, argv);
}
