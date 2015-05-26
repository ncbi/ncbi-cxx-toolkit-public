/*  $Id$
* ===========================================================================
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
* ===========================================================================
*/

#include <ncbi_pch.hpp>
#include <corelib/ncbiapp.hpp>

#include <objmgr/object_manager.hpp>

#include <objtools/data_loaders/genbank/gbloader.hpp>
#include <objtools/data_loaders/genbank/impl/dispatcher.hpp>
#include <objtools/data_loaders/genbank/impl/standalone_result.hpp>
#include <objtools/data_loaders/genbank/gicache/reader_gicache.hpp>

#include <connect/ncbi_core_cxx.hpp>

#include <common/test_assert.h>  /* This header must go last */


USING_NCBI_SCOPE;
USING_SCOPE(objects);

class CTestApplication : public CNcbiApplication
{
public:
    virtual int Run(void);
    virtual void Init(void);
};


void CTestApplication::Init(void)
{
    CONNECT_Init(&GetConfig());

    auto_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);
    arg_desc->AddDefaultKey("gi_from", "GiFrom",
                            "first GI to fetch",
                            CArgDescriptions::eInt8, "156000");
    arg_desc->AddDefaultKey("gi_to", "GiTo",
                            "last GI to fetch",
                            CArgDescriptions::eInt8, "156999");
    arg_desc->AddDefaultKey("count", "Count",
                            "number of passes",
                            CArgDescriptions::eInteger, "1");

    // Program description
    string prog_description = "Test gicache reader\n";
    arg_desc->SetUsageContext(GetArguments().GetProgramBasename(),
                              prog_description, false);

    // Pass argument descriptions to the application
    //
    SetupArgDescriptions(arg_desc.release());
}


int CTestApplication::Run(void)
{
    //export CONN_DEBUG_PRINTOUT=data
    //CORE_SetLOG(LOG_cxx2c());
    //SetDiagTrace(eDT_Enable);
    //SetDiagPostLevel(eDiag_Info);
    //SetDiagPostFlag(eDPF_All);

    //test_gicache_calls();

    const CArgs& args = GetArgs();
    int count = args["count"].AsInteger();
    Int8 gi_from = args["gi_from"].AsInt8();
    Int8 gi_to = args["gi_to"].AsInt8();

    CRef<CObjectManager> om = CObjectManager::GetInstance();
    for ( int i = 0; i < 10; ++i ) {
        CDataLoader* loader =
            CGBDataLoader::RegisterInObjectManager(*om, new CGICacheReader).GetLoader();
        om->RevokeDataLoader(*loader);
    }
    for ( int pass = 0; pass < count; ++pass ) {
        CRef<CReadDispatcher> dispatcher(new CReadDispatcher);
        CRef<CReader> reader(new CGICacheReader);
        dispatcher->InsertReader(0, reader);
        int bad_count = 0, all_count = 0;
        for ( Int8 gi = gi_from; gi <= gi_to; gi++ ) {
            ++all_count;
            NcbiCout << "gi: " << gi << ": " << NcbiFlush;
            CSeq_id_Handle seq_id = CSeq_id_Handle::GetGiHandle(gi);
            CStandaloneRequestResult request(seq_id);
            try {
                dispatcher->LoadSeq_idAccVer(request, seq_id);
            }
            catch ( CLoaderException& exc ) {
                ++bad_count;
                NcbiCout << "none" << NcbiEndl;
                continue;
            }
            CLoadLockAcc lock(request, seq_id);
            CSeq_id_Handle acc_ver = lock.GetAccVer();
            NcbiCout << acc_ver;
            NcbiCout << NcbiEndl;
        }
        if ( bad_count ) {
            NcbiCout << "Notice: " << bad_count << " gis of "
                     << all_count << " are bad." << NcbiEndl;
        }
        _ASSERT(bad_count <= all_count / 50);
    }
    return 0;
}


int main(int argc, const char* argv[])
{
    return CTestApplication().AppMain(argc, argv);
}
