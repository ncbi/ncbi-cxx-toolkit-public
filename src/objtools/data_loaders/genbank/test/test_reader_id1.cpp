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
#include <objtools/data_loaders/genbank/id1/reader_id1.hpp>

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
                            CArgDescriptions::eInt8, "100");
    arg_desc->AddDefaultKey("gi_to", "GiTo",
                            "last GI to fetch",
                            CArgDescriptions::eInt8, "200");
    arg_desc->AddDefaultKey("count", "Count",
                            "number of passes",
                            CArgDescriptions::eInteger, "1");

    // Program description
    string prog_description = "Test id1 reader\n";
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

    //test_id1_calls();

    const CArgs& args = GetArgs();
    int count = args["count"].AsInteger();
    Int8 gi_from = args["gi_from"].AsInt8();
    Int8 gi_to = args["gi_to"].AsInt8();

    CRef<CObjectManager> om = CObjectManager::GetInstance();
    for ( int i = 0; i < 100; ++i ) {
        CGBDataLoader::RegisterInObjectManager(*om, new CId1Reader);
    }
    for ( int pass = 0; pass < count; ++pass ) {
        CRef<CReadDispatcher> dispatcher(new CReadDispatcher);
        CRef<CReader> reader(new CId1Reader);
        dispatcher->InsertReader(0, reader);
        for ( Int8 gi = gi_from; gi <= gi_to; gi++ ) {
            NcbiCout << "gi: " << gi << ": " << NcbiEndl;
            CSeq_id_Handle seq_id = CSeq_id_Handle::GetGiHandle(gi);
            CStandaloneRequestResult request(seq_id);
            dispatcher->LoadSeq_idSeq_ids(request, seq_id);
            CLoadLockSeqIds seq_ids(request, seq_id);
            NcbiCout << "  ids:";
            CFixedSeq_ids ids2 = seq_ids.GetSeq_ids();
            ITERATE ( CFixedSeq_ids, i, ids2 ) {
                NcbiCout << " " << i->AsString();
            }
            NcbiCout << NcbiEndl;
            
            CLoadLockBlobIds blob_ids(request, seq_id, 0);
            dispatcher->LoadSeq_idBlob_ids(request, seq_id, 0);
            CFixedBlob_ids ids = blob_ids.GetBlob_ids();
            ITERATE ( CFixedBlob_ids, i, ids ) {
                const CBlob_Info& blob_info = *i;
                CConstRef<CBlob_id> blob_id = blob_info.GetBlob_id();
                NcbiCout << "  " << blob_id->ToString() << NcbiEndl;

                if ( !blob_id->IsMainBlob() ) {
                    continue;
                }
                CLoadLockBlob blob(request, *blob_id);
                dispatcher->LoadBlob(request, *blob_id);
                if ( !blob.IsLoadedBlob() ) {
                    NcbiCout << "blob is not available" << NcbiEndl;
                    continue;
                }
                NcbiCout << "    loaded" << NcbiEndl;
            }
        }
    }
    return 0;
}


int main(int argc, const char* argv[])
{
    return CTestApplication().AppMain(argc, argv);
}
