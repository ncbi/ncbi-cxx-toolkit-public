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
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.15  2002/04/25 18:15:26  grichenk
* Adjusted tests to work with the updated CSeqVector
*
* Revision 1.14  2002/04/23 15:26:47  gouriano
* use test_mt library
*
* Revision 1.13  2002/04/22 20:07:45  grichenk
* Commented calls to CBioseq::ConstructExcludedSequence()
*
* Revision 1.12  2002/04/21 05:16:59  vakatov
* Decreased the # of threads from 40 to 34 to avoid triggering guard-bomb
* on SCHROEDER
*
* Revision 1.11  2002/03/18 21:47:16  grichenk
* Moved most includes to test_helper.cpp
* Added test for CBioseq::ConstructExcludedSequence()
*
* Revision 1.10  2002/03/13 18:06:31  gouriano
* restructured MT test. Put common functions into a separate file
*
* Revision 1.9  2002/03/07 21:42:06  grichenk
* +Test for GetSeq_annot()
*
* Revision 1.8  2002/03/05 16:08:16  grichenk
* Moved TSE-restriction to new constructors
*
* Revision 1.7  2002/03/04 17:07:19  grichenk
* +Testing feature iterators with single TSE restriction
*
* Revision 1.6  2002/02/25 21:05:31  grichenk
* Removed seq-data references caching. Increased MT-safety. Fixed typos.
*
* Revision 1.5  2002/02/07 21:27:36  grichenk
* Redesigned CDataSource indexing: seq-id handle -> TSE -> seq/annot
*
* Revision 1.4  2002/01/23 21:59:34  grichenk
* Redesigned seq-id handles and mapper
*
* Revision 1.3  2002/01/16 18:56:30  grichenk
* Removed CRef<> argument from choice variant setter, updated sources to
* use references instead of CRef<>s
*
* Revision 1.2  2002/01/16 16:28:47  gouriano
* restructured objmgr
*
* Revision 1.1  2002/01/11 19:06:29  gouriano
* restructured objmgr
*
* Revision 1.4  2001/12/20 20:00:30  grichenk
* CObjectManager::ConstructBioseq(CSeq_loc) -> CBioseq::CBioseq(CSeq_loc ...)
*
* Revision 1.3  2001/12/12 22:39:12  grichenk
* Added test for minus-strand intervals in constructed bioseqs
*
* Revision 1.2  2001/12/12 17:48:45  grichenk
* Fixed code using generated classes to work with the updated datatool
*
* Revision 1.1  2001/12/07 19:08:58  grichenk
* Initial revision
*
*
* ===========================================================================
*/

#include <corelib/ncbistd.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbienv.hpp>
#include <corelib/ncbiargs.hpp>
#include <corelib/ncbitime.hpp>
#include <corelib/ncbithr.hpp>
#include <corelib/test_mt.hpp>

#include "test_helper.hpp"



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

    CRef<CScope> m_Scope;
    CRef<CObjectManager> m_ObjMgr;
};


/////////////////////////////////////////////////////////////////////////////

bool CTestObjectManager::Thread_Run(int idx)
{
    ++idx;
    // Test global scope
    // read data from a scope, which is shared by all threads
    CTestHelper::TestDataRetrieval(*m_Scope, 0, 0, false);
    // add more data to the global scope
    CRef<CSeq_entry> entry1 = &CDataGenerator::CreateTestEntry1(idx);
    CRef<CSeq_entry> entry2 = &CDataGenerator::CreateTestEntry2(idx);
    m_Scope->AddTopLevelSeqEntry(*entry1);
    m_Scope->AddTopLevelSeqEntry(*entry2);
    CTestHelper::TestDataRetrieval(*m_Scope, idx, 0, false);

    // Test local scope
    // 1.2.5 add annotation to one sequence
    // verify that others did not change
    {
        CScope scope(*m_ObjMgr);
        // create new seq.entries - to be able to check unresolved lengths
        CRef<CSeq_entry> entry1 = &CDataGenerator::CreateTestEntry1(idx);
        CRef<CSeq_entry> entry2 = &CDataGenerator::CreateTestEntry2(idx);
        scope.AddTopLevelSeqEntry(*entry1);
        scope.AddTopLevelSeqEntry(*entry2);
        CRef<CSeq_annot> annot = &CDataGenerator::CreateAnnotation1(idx);
        scope.AttachAnnot(*entry1, *annot);
        CTestHelper::TestDataRetrieval(scope, idx, 1, true);

        // 1.2.6. Constructed bio sequences
        CSeq_id id;
        {{
            CRef<CSeq_entry> constr_entry =
                &CDataGenerator::CreateConstructedEntry( idx, 1);
            scope.AddTopLevelSeqEntry(*constr_entry);
            id.SetLocal().SetStr("constructed1");
            CTestHelper::ProcessBioseq(scope, id,
                27, 27, "GCGGTACAATAACCTCAGCAGCAACAA", "",
                0, 0, 0, 0, 0, 0, 0, 0, 0);
            /*
            CRef<CSeq_entry> constr_ex_entry =
                &CDataGenerator::CreateConstructedExclusionEntry( idx, 1);
            scope.AddTopLevelSeqEntry(*constr_ex_entry);
            // test
            id.SetLocal().SetStr("construct_exclusion1");
            CTestHelper::ProcessBioseq(scope, id,
                22, 22, "GCGTAGACATCCCAGAGCGGTG", "",
                0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
            */
        }}
        {{
            CRef<CSeq_entry> constr_entry =
                &CDataGenerator::CreateConstructedEntry( idx, 2);
            scope.AddTopLevelSeqEntry(*constr_entry);
            id.SetLocal().SetStr("constructed2");
            CTestHelper::ProcessBioseq(scope, id,
                27, 27, "TACCGCCAATAACCTCAGCAGCAACAA", "",
                0, 0, 0, 0, 0, 0, 0, 0, 0);
            /*
            CRef<CSeq_entry> constr_ex_entry =
                &CDataGenerator::CreateConstructedExclusionEntry( idx, 2);
            scope.AddTopLevelSeqEntry(*constr_ex_entry);
            // test
            id.SetLocal().SetStr("construct_exclusion2");
            CTestHelper::ProcessBioseq(scope, id,
                22, 22, "GCGTAGACATCCCAGAGCGGTG", "",
                0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
            */
        }}
    }

    // 1.2.7. one entry in two scopes
    {
        CScope Scope1(*m_ObjMgr);
        CRef<CScope> pScope2 = new CScope(*m_ObjMgr);
        CRef<CSeq_entry> entry1 = &CDataGenerator::CreateTestEntry1(idx);
        CRef<CSeq_entry> entry2 = &CDataGenerator::CreateTestEntry2(idx);
        Scope1.AddTopLevelSeqEntry(*entry1);
        Scope1.AddTopLevelSeqEntry(*entry2);
        pScope2->AddTopLevelSeqEntry(*entry2);
        // Test with unresolvable references
        CSeq_id id;
        id.SetGi(21+idx*1000);
        CTestHelper::ProcessBioseq(*pScope2, id,
            22, 22,
            "\\0\\0\\0\\0\\0\\0\\0\\0\\0\\0\\0\\0\\0\\0\\0\\0\\0\\0\\0\\0\\0\\0",
            "\\0\\0\\0\\0\\0\\0\\0\\0\\0\\0\\0\\0\\0\\0\\0\\0\\0\\0\\0\\0\\0\\0",
            1, 1, 1, 0, 0, 1, 1, 0, 0);

        // add more data to the scope - to make references resolvable
        CRef<CSeq_entry> entry1a = &CDataGenerator::CreateTestEntry1a(idx);
        pScope2->AddTopLevelSeqEntry(*entry1a);
        // Test with resolvable references
        id.SetGi(21+idx*1000);
        CTestHelper::ProcessBioseq(*pScope2, id,
            22, 62,
            "AAAAATTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTAAAAATTTTTTTTTTTT",
            "TTTTTAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAATTTTTAAAAAAAAAAAA",
            1, 1, 1, 0, 0, 1, 1, 0, 0);

        // 1.2.8. Test scope history
        CRef<CSeq_entry> entry1b = &CDataGenerator::CreateTestEntry1(idx);
        pScope2->AddTopLevelSeqEntry(*entry1b);
        id.SetLocal().SetStr("seq"+NStr::IntToString(11+idx*1000));
        // gi|11 from entry1a must be selected
        CTestHelper::ProcessBioseq(*pScope2, id,
            40, 40,
            "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA",
            "TTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTT",
            0, 4, 2, 1, 0, 2, 2, 1, 0);
    }
    return true;
}

bool CTestObjectManager::TestApp_Init(void)
{
    NcbiCout << "Testing ObjectManager (" << s_NumThreads << " threads)..." << NcbiEndl;

    m_ObjMgr = new CObjectManager;
    // Scope shared by all threads
    m_Scope = new CScope(*m_ObjMgr);
    CRef<CSeq_entry> entry1 = &CDataGenerator::CreateTestEntry1(0);
    CRef<CSeq_entry> entry2 = &CDataGenerator::CreateTestEntry2(0);
    m_Scope->AddTopLevelSeqEntry(*entry1);
    m_Scope->AddTopLevelSeqEntry(*entry2);
    return true;
}

bool CTestObjectManager::TestApp_Exit(void)
{
    NcbiCout << " Passed" << NcbiEndl << NcbiEndl;
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
