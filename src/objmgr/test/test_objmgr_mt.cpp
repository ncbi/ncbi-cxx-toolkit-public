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
* Authors:  Eugene Vasilchenko, Aleksey Grichenko, Denis Vakatov
*
* File Description:
*   Test the functionality of C++ object manager in MT mode
*
* ===========================================================================
*/
#define NCBI_TEST_APPLICATION
#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbienv.hpp>
#include <corelib/ncbiargs.hpp>
#include <corelib/ncbitime.hpp>
#include <corelib/ncbithr.hpp>
#include <corelib/test_mt.hpp>
#include <util/random_gen.hpp>
#include <sys/stat.h>
#include <fcntl.h>

#include <objects/seqloc/Seq_id.hpp>
#include <objects/seqset/Seq_entry.hpp>
#include <objects/seq/Seq_annot.hpp>

#include "test_helper.hpp"

#include <common/test_assert.h>  /* This header must go last */


BEGIN_NCBI_SCOPE
using namespace objects;


/////////////////////////////////////////////////////////////////////////////
//
//  Test application
//

class CTestObjectManager : public CThreadedApp
{
protected:
    virtual bool Thread_Run(int idx);
    virtual bool TestApp_Init(void);
    virtual bool TestApp_Exit(void);
    virtual bool TestApp_Args(CArgDescriptions& args);

    CRef<CObjectManager> m_ObjMgr;
    CRef<CScope> m_Scope;
};


/////////////////////////////////////////////////////////////////////////////


//typedef CConstRef<CSeq_entry> TEntry;
typedef CRef<CSeq_entry> TEntry;


bool CTestObjectManager::Thread_Run(int idx)
{
    ++idx;

    if ( 1 ) {
        const int test_count = 1000;
        const int num_seq_ids = 10;

        CRandom r(idx);
        vector<CRef<CSeq_id> > seq_ids(num_seq_ids);
        for ( int j = 0; j < num_seq_ids; ++j ) {
            seq_ids[j].Reset(new CSeq_id);
            seq_ids[j]->SetLocal().SetId(j);
        }
        
        for ( int t = 0; t < test_count; ++t ) {
            int j = r.GetRand(0, num_seq_ids-1);
            CSeq_id_Handle h = CSeq_id_Handle::GetHandle(*seq_ids[j]);
            h.Reset();
        }
    }
    if ( 1 ) {
        const int test_count = 1000;
        const int num_seq_ids = 10;
        const int num_ids = 2;

        CRandom r(idx);
        vector<CRef<CSeq_id> > seq_ids(num_seq_ids);
        for ( int j = 0; j < num_seq_ids; ++j ) {
            seq_ids[j].Reset(new CSeq_id);
            seq_ids[j]->SetLocal().SetId(j);
        }

        vector<CSeq_id_Handle> ids(num_ids);
        for ( int i = 0; i < num_ids; ++i ) {
            ids[i] = CSeq_id_Handle::GetHandle(*seq_ids[i]);
        }

        for ( int t = 0; t < test_count; ++t ) {
            int i = r.GetRand(0, num_ids-1);
            int j = r.GetRand(0, num_seq_ids-1);
            ids[i] = CSeq_id_Handle::GetHandle(*seq_ids[j]);
        }
    }

    if ( m_Scope ) {
        // Test global scope
        // read data from a scope, which is shared by all threads
        CTestHelper::TestDataRetrieval(*m_Scope, 0, 0);
        // add more data to the global scope
        TEntry entry1(&CDataGenerator::CreateTestEntry1(idx));
        TEntry entry2(&CDataGenerator::CreateTestEntry2(idx));
        m_Scope->AddTopLevelSeqEntry(*entry1);
        m_Scope->AddTopLevelSeqEntry(*entry2);
        CTestHelper::TestDataRetrieval(*m_Scope, idx, 0);
    }
    for ( int i = 0; i < 1000; ++i ) {
        CObjectManager::GetInstance();
    }

    // Test local scope
    // 1.2.5 add annotation to one sequence
    // verify that others did not change
    {
        CRef<CObjectManager> objmgr = CObjectManager::GetInstance();
        CScope scope(*objmgr);
        // create new seq.entries - to be able to check unresolved lengths
        CRef<CSeq_entry> entry1(&CDataGenerator::CreateTestEntry1(idx));
        TEntry entry2(&CDataGenerator::CreateTestEntry2(idx));
        scope.AddTopLevelSeqEntry(*entry1);
        scope.AddTopLevelSeqEntry(*entry2);
        CRef<CSeq_annot> annot(&CDataGenerator::CreateAnnotation1(idx));
        scope.AttachAnnot(*entry1, *annot);
        CTestHelper::TestDataRetrieval(scope, idx, 1);

        // 1.2.6. Constructed bio sequences
        CSeq_id id;
        {{
            TEntry constr_entry
                (&CDataGenerator::CreateConstructedEntry( idx, 1));
            scope.AddTopLevelSeqEntry(*constr_entry);
            id.SetLocal().SetStr("constructed1");
            CTestHelper::ProcessBioseq(scope, id, 27,
                "GCGGTACAATAACCTCAGCAGCAACAA", "",
                0, 5, 0, 0, 0, 0, 0, 0, 0, 0);
        }}
        {{
            TEntry constr_entry
                (&CDataGenerator::CreateConstructedEntry( idx, 2));
            scope.AddTopLevelSeqEntry(*constr_entry);
            id.SetLocal().SetStr("constructed2");
            CTestHelper::ProcessBioseq(scope, id, 27,
                "TACCGCCAATAACCTCAGCAGCAACAA", "",
                0, 5, 0, 0, 0, 0, 0, 0, 0, 0);
        }}
    }

    // 1.2.7. one entry in two scopes
    {
        CRef<CObjectManager> objmgr = CObjectManager::GetInstance();
        CScope Scope1(*objmgr);
        CRef<CScope> pScope2(new CScope(*objmgr));
        TEntry entry1(&CDataGenerator::CreateTestEntry1(idx));
        TEntry entry2(&CDataGenerator::CreateTestEntry2(idx));
        Scope1.AddTopLevelSeqEntry(*entry1);
        Scope1.AddTopLevelSeqEntry(*entry2);
        pScope2->AddTopLevelSeqEntry(*entry2);
        // Test with unresolvable references
        CSeq_id id;
        id.SetGi(21+idx*1000);
        CTestHelper::ProcessBioseq(*pScope2, id, 22,
          "\\0\\0\\0\\0\\0\\0\\0\\0\\0\\0\\0\\0\\0\\0\\0\\0\\0\\0\\0\\0\\0\\0",
          "\\0\\0\\0\\0\\0\\0\\0\\0\\0\\0\\0\\0\\0\\0\\0\\0\\0\\0\\0\\0\\0\\0",
          1, -1, 1, 1, 0, 0, 1, 1, 0, 0, false, true);

        // add more data to the scope - to make references resolvable
        TEntry entry1a(&CDataGenerator::CreateTestEntry1a(idx));
        pScope2->AddTopLevelSeqEntry(*entry1a);
        // Test with resolvable references
        id.SetGi(21+idx*1000);
        CTestHelper::ProcessBioseq(*pScope2, id, 62,
            "AAAAATTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTAAAAATTTTTTTTTTTT",
            "TTTTTAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAATTTTTAAAAAAAAAAAA",
            1, 2, 2, 1, 0, 0, 1, 1, 0, 0);

        // 1.2.8. Test scope history
        TEntry entry1b(&CDataGenerator::CreateTestEntry1(idx));
        pScope2->AddTopLevelSeqEntry(*entry1b);
        id.SetLocal().SetStr("seq"+NStr::IntToString(11+idx*1000));
        // gi|11 from entry1a must be selected
        CTestHelper::ProcessBioseq(*pScope2, id, 40,
            "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA",
            "TTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTT",
            0, 2, 2, 1, 0, 0, 1, 1, 0, 0);
    }
    return true;
}

bool CTestObjectManager::TestApp_Init(void)
{
    const CArgs& args = GetArgs();
    CDataGenerator::sm_DumpEntries = args["dump_entries"];
    CTestHelper::sm_DumpFeatures = args["dump_features"];

    NcbiCout << "Testing ObjectManager (" << s_NumThreads << " threads)..." << NcbiEndl;

    if ( !args["no_global"] ) {
        m_ObjMgr = CObjectManager::GetInstance();
        // Scope shared by all threads
        m_Scope = new CScope(*m_ObjMgr);
        TEntry entry1(&CDataGenerator::CreateTestEntry1(0));
        TEntry entry2(&CDataGenerator::CreateTestEntry2(0));
        m_Scope->AddTopLevelSeqEntry(*entry1);
        m_Scope->AddTopLevelSeqEntry(*entry2);
    }
    return true;
}

bool CTestObjectManager::TestApp_Exit(void)
{
    NcbiCout << " Passed" << NcbiEndl << NcbiEndl;
    return true;
}

bool CTestObjectManager::TestApp_Args(CArgDescriptions& args)
{
    // Prepare command line descriptions
    args.AddFlag("dump_entries", "print all generated seq entries");
    args.AddFlag("dump_features", "print all found features");
    args.AddFlag("no_global", "do not create and test global scope");
    return true;
}

END_NCBI_SCOPE


/////////////////////////////////////////////////////////////////////////////
//  MAIN

USING_NCBI_SCOPE;

int main(int argc, const char* argv[])
{
    return CTestObjectManager().AppMain(argc, argv, 0, eDS_Default, 0);
}
