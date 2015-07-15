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
*/
#define NCBI_TEST_APPLICATION
#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbienv.hpp>
#include <corelib/ncbiargs.hpp>
#include <corelib/ncbitime.hpp>

#include <objects/seqloc/Seq_id.hpp>
#include <objects/seqset/Seq_entry.hpp>
#include <objects/seq/Seq_annot.hpp>

#include "test_helper.hpp"
#include <objmgr/seq_vector.hpp>

#include <objects/general/Date.hpp>

#include <common/test_assert.h>  /* This header must go last */


BEGIN_NCBI_SCOPE
using namespace objects;


/////////////////////////////////////////////////////////////////////////////
//
//  Test application
//

class CTestApp : public CNcbiApplication
{
public:
    virtual void Init(void);
    virtual int  Run (void);
private:
    CRef<CObjectManager> m_ObjMgr;
};


void CTestApp::Init(void)
{
    // Prepare command line descriptions
    auto_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);

    arg_desc->AddFlag("dump_entries", "print all generated seq entries");
    arg_desc->AddFlag("dump_features", "print all found features");

    string prog_description = "testobjmgr";
    arg_desc->SetUsageContext(GetArguments().GetProgramBasename(),
                              prog_description, false);

    SetupArgDescriptions(arg_desc.release());
}


//typedef CConstRef<CSeq_entry> TEntry;
typedef CRef<CSeq_entry> TEntry;


int CTestApp::Run(void)
{
    SetDiagPostFlag(eDPF_All);
    SetDiagPostLevel(eDiag_Info);
    NcbiCout << "Testing ObjectManager..." << NcbiEndl;
    CSeq_id id;
    int idx;

    const CArgs& args = GetArgs();
    CDataGenerator::sm_DumpEntries = args["dump_entries"];
    CTestHelper::sm_DumpFeatures = args["dump_features"];
    CTestHelper::sm_TestRemoveEntry = true;

    m_ObjMgr = CObjectManager::GetInstance();

    // 1.2.3. Scope is an object on the stack
    for (idx = 0; idx <= 0; idx++) {
        CScope Scope1(*m_ObjMgr);
        CScope Scope2(*m_ObjMgr);
        CScope Scope(*m_ObjMgr);
        // populate
#if 1
        TEntry entry1(&CDataGenerator::CreateTestEntry1(idx));
        Scope1.AddTopLevelSeqEntry(*entry1);
        TEntry entry2(&CDataGenerator::CreateTestEntry2(idx));
        Scope2.AddTopLevelSeqEntry(*entry2);
        Scope.AddScope(Scope1);
        Scope.AddScope(Scope2);
#else
        TEntry entry1(&CDataGenerator::CreateTestEntry1(idx));
        Scope.AddTopLevelSeqEntry(*entry1);
        TEntry entry2(&CDataGenerator::CreateTestEntry2(idx));
        Scope.AddTopLevelSeqEntry(*entry2);
#endif
        // retrieve data
        CTestHelper::TestDataRetrieval( Scope, 0, 0);
    }

    // 1.2.3. The same scope twice with the same priority
    for (idx = 0; idx <= 0; idx++) {
        CScope Scope1(*m_ObjMgr);
        CScope Scope(*m_ObjMgr);
        // populate
        TEntry entry1(&CDataGenerator::CreateTestEntry1(idx));
        Scope1.AddTopLevelSeqEntry(*entry1);
        TEntry entry2(&CDataGenerator::CreateTestEntry2(idx));
        Scope1.AddTopLevelSeqEntry(*entry2);
        Scope.AddScope(Scope1);
        //Scope.AddScope(Scope1);
        // retrieve data
        CTestHelper::TestDataRetrieval( Scope, 0, 0);
    }

    // 1.2.4. Scope is an object on the heap
    for (idx = 1; idx <= 1; idx++) {
        CRef<CScope> pScope(new CScope(*m_ObjMgr));
        // populate
        TEntry entry1(&CDataGenerator::CreateTestEntry1(idx));
        pScope->AddTopLevelSeqEntry(*entry1);
        TEntry entry2(&CDataGenerator::CreateTestEntry2(idx));
        pScope->AddTopLevelSeqEntry(*entry2);
        // retrieve data
        CTestHelper::TestDataRetrieval( *pScope, idx, 0);
    }

    // 1.2.5 add annotation to one sequence
    // verify that others did not change
    for (idx = 1; idx <= 1; idx++) {
        CRef<CScope> pScope(new CScope(*m_ObjMgr));
        // populate
        CRef<CSeq_entry> entry1(&CDataGenerator::CreateTestEntry1(idx));
        pScope->AddTopLevelSeqEntry(*entry1);
        TEntry entry2(&CDataGenerator::CreateTestEntry2(idx));
        pScope->AddTopLevelSeqEntry(*entry2);
        CTestHelper::TestDataRetrieval( *pScope, idx, 0);
        // add annotation
        CRef<CSeq_annot> annot(&CDataGenerator::CreateAnnotation1(idx));
        pScope->AttachAnnot(*entry1, *annot);
        // retrieve data  (delta=1 - one more annotation)
        CTestHelper::TestDataRetrieval( *pScope, idx, 1);

        // Take the added annotation back
        pScope->RemoveAnnot(*entry1, *annot);
        CTestHelper::TestDataRetrieval( *pScope, idx, 0);

        CSeq_annot_Handle hx = pScope->AddSeq_annot(*annot);
        _ASSERT(hx.GetCompleteSeq_annot() == annot);
    }

    // 1.2.6. Constructed bio sequences
    for (idx = 1; idx <= 1; idx++) {
        CRef<CSeqVector> vec;
        CScope Scope(*m_ObjMgr);
        TEntry entry1(&CDataGenerator::CreateTestEntry1(idx));
        Scope.AddTopLevelSeqEntry(*entry1);
        {{
            TEntry constr_entry
                (&CDataGenerator::CreateConstructedEntry( idx, 1));
            Scope.AddTopLevelSeqEntry(*constr_entry);
            // test
            id.SetLocal().SetStr("constructed1");
            CTestHelper::ProcessBioseq(Scope, id, 27,
                "GCGGTACAATAACCTCAGCAGCAACAA", "",
                0, 3, 0, 0, 0, 0, 0, 0, 0, 0);
        }}
        {{
            TEntry constr_entry
                (&CDataGenerator::CreateConstructedEntry( idx, 2));
            Scope.AddTopLevelSeqEntry(*constr_entry);
            // test
            id.SetLocal().SetStr("constructed2");
            CTestHelper::ProcessBioseq(Scope, id, 27,
                "TACCGCCAATAACCTCAGCAGCAACAA", "",
                0, 3, 0, 0, 0, 0, 0, 0, 0, 0);
        }}
        CBioseq_Handle bh = Scope.GetBioseqHandle(id);
        _ASSERT(bh);
        vec = new CSeqVector(bh.GetSeqVector(CBioseq_Handle::eCoding_Iupac));
    }

    // 1.2.7. one entry in two scopes
    for (idx = 1; idx <= 1; idx++) {
        // populate scopes
        CRef<CScope> pScope2(new CScope(*m_ObjMgr));
        TEntry entry1(&CDataGenerator::CreateTestEntry1(idx));
        TEntry entry2(&CDataGenerator::CreateTestEntry2(idx));
        pScope2->AddTopLevelSeqEntry(*entry2);
        // Test with unresolvable references
        id.SetGi(21+idx*1000);
        CTestHelper::ProcessBioseq(*pScope2, id, 22,
            "\\x00\\x00\\x00\\x00\\x00\\x00\\x00\\x00\\x00\\x00\\x00"
            "\\x00\\x00\\x00\\x00\\x00\\x00\\x00\\x00\\x00\\x00\\x00",
            "\\x00\\x00\\x00\\x00\\x00\\x00\\x00\\x00\\x00\\x00\\x00"
            "\\x00\\x00\\x00\\x00\\x00\\x00\\x00\\x00\\x00\\x00\\x00",
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

    NcbiCout << " Passed" << NcbiEndl << NcbiEndl;

    return 0;
}


END_NCBI_SCOPE



/////////////////////////////////////////////////////////////////////////////
//  MAIN


USING_NCBI_SCOPE;

int main(int argc, const char* argv[])
{
    return CTestApp().AppMain(argc, argv);
}
