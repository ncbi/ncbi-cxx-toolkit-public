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
* Revision 1.20  2002/03/18 21:47:16  grichenk
* Moved most includes to test_helper.cpp
* Added test for CBioseq::ConstructExcludedSequence()
*
* Revision 1.19  2002/03/13 18:06:31  gouriano
* restructured MT test. Put common functions into a separate file
*
* Revision 1.18  2002/03/08 21:23:50  gouriano
* reorganized and classified tests
*
* Revision 1.16  2002/03/05 16:08:16  grichenk
* Moved TSE-restriction to new constructors
*
* Revision 1.15  2002/03/04 17:07:18  grichenk
* +Testing feature iterators with single TSE restriction
*
* Revision 1.14  2002/02/25 21:05:31  grichenk
* Removed seq-data references caching. Increased MT-safety. Fixed typos.
*
* Revision 1.13  2002/02/21 19:27:09  grichenk
* Rearranged includes. Added scope history. Added searching for the
* best seq-id match in data sources and scopes. Updated tests.
*
* Revision 1.12  2002/02/07 21:27:36  grichenk
* Redesigned CDataSource indexing: seq-id handle -> TSE -> seq/annot
*
* Revision 1.11  2002/02/06 21:46:11  gouriano
* *** empty log message ***
*
* Revision 1.10  2002/02/05 21:46:28  gouriano
* added FindSeqid function, minor tuneup in CSeq_id_mapper
*
* Revision 1.9  2002/02/04 21:16:27  gouriano
* minor changes to make it run correctly on Solaris
*
* Revision 1.8  2002/01/30 22:09:28  gouriano
* changed CSeqMap interface
*
* Revision 1.7  2002/01/29 17:47:33  grichenk
* Removed commented part
*
* Revision 1.6  2002/01/28 19:44:50  gouriano
* changed the interface of BioseqHandle: two functions moved from Scope
*
* Revision 1.5  2002/01/23 21:59:34  grichenk
* Redesigned seq-id handles and mapper
*
* Revision 1.4  2002/01/18 17:06:30  gouriano
* renamed CScope::GetSequence to CScope::GetSeqVector
*
* Revision 1.3  2002/01/16 18:56:30  grichenk
* Removed CRef<> argument from choice variant setter, updated sources to
* use references instead of CRef<>s
*
* Revision 1.2  2002/01/16 16:28:46  gouriano
* restructured objmgr
*
* Revision 1.1  2002/01/11 19:06:28  gouriano
* restructured objmgr
*
* Revision 1.31  2001/12/20 20:00:30  grichenk
* CObjectManager::ConstructBioseq(CSeq_loc) -> CBioseq::CBioseq(CSeq_loc ...)
*
* Revision 1.30  2001/12/12 22:39:11  grichenk
* Added test for minus-strand intervals in constructed bioseqs
*
* Revision 1.29  2001/12/12 17:48:45  grichenk
* Fixed code using generated classes to work with the updated datatool
*
* Revision 1.28  2001/12/07 19:08:44  grichenk
* Updated Object manager test
*
*
* ===========================================================================
*/

#include <corelib/ncbistd.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbienv.hpp>
#include <corelib/ncbiargs.hpp>
#include <corelib/ncbitime.hpp>

#include "test_helper.hpp"

#include <objects/general/Date.hpp>

BEGIN_NCBI_SCOPE
using namespace objects;


/////////////////////////////////////////////////////////////////////////////
//
//  Test application
//

class CTestApp : public CNcbiApplication
{
public:
    virtual int  Run (void);
private:
    CRef<CObjectManager> m_ObjMgr;
};


int CTestApp::Run(void)
{
    NcbiCout << "Testing ObjectManager..." << NcbiEndl;
    CSeq_id id;
    int idx;

    m_ObjMgr = new CObjectManager;

    // 1.2.3. Scope is an object on the stack
    for (idx = 0; idx <= 0; idx++) {
        CScope Scope(*m_ObjMgr);
        // populate
        CRef<CSeq_entry> entry1 = &CDataGenerator::CreateTestEntry1(idx);
        Scope.AddTopLevelSeqEntry(*entry1);
        CRef<CSeq_entry> entry2 = &CDataGenerator::CreateTestEntry2(idx);
        Scope.AddTopLevelSeqEntry(*entry2);
        // retrieve data
        CTestHelper::TestDataRetrieval( Scope, 0, 0, true);

        // Find seq_id
        {
            set< CRef<const CSeq_id> > setId;
            Scope.FindSeqid(setId, "seq11.3");
        }
    }
    // 1.2.4. Scope is an object on the heap
    for (idx = 1; idx <= 1; idx++) {
        CRef<CScope> pScope(new CScope(*m_ObjMgr));
        // populate
        CRef<CSeq_entry> entry1 = &CDataGenerator::CreateTestEntry1(idx);
        pScope->AddTopLevelSeqEntry(*entry1);
        CRef<CSeq_entry> entry2 = &CDataGenerator::CreateTestEntry2(idx);
        pScope->AddTopLevelSeqEntry(*entry2);
        // retrieve data
        CTestHelper::TestDataRetrieval( *pScope, idx, 0, true);
    }

    // 1.2.5 add annotation to one sequence
    // verify that others did not change
    for (idx = 1; idx <= 1; idx++) {
        CRef<CScope> pScope(new CScope(*m_ObjMgr));
        // populate
        CRef<CSeq_entry> entry1 = &CDataGenerator::CreateTestEntry1(idx);
        pScope->AddTopLevelSeqEntry(*entry1);
        CRef<CSeq_entry> entry2 = &CDataGenerator::CreateTestEntry2(idx);
        pScope->AddTopLevelSeqEntry(*entry2);

        // add annotation
        CRef<CSeq_annot> annot = &CDataGenerator::CreateAnnotation1(idx);
        pScope->AttachAnnot(*entry1, *annot);
        // retrieve data  (delta=1 - one more annotation)
        CTestHelper::TestDataRetrieval( *pScope, idx, 1, true);
    }

    // 1.2.6. Constructed bio sequences
    for (idx = 1; idx <= 1; idx++) {
        CScope Scope(*m_ObjMgr);
        CRef<CSeq_entry> entry1 = &CDataGenerator::CreateTestEntry1(idx);
        Scope.AddTopLevelSeqEntry(*entry1);
        {{
            CRef<CSeq_entry> constr_entry =
                &CDataGenerator::CreateConstructedEntry( idx, 1);
            Scope.AddTopLevelSeqEntry(*constr_entry);
            // test
            id.SetLocal().SetStr("constructed1");
            CTestHelper::ProcessBioseq(Scope, id,
                27, 27, "GCGGTACAATAACCTCAGCAGCAACAA", "",
                0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
            CRef<CSeq_entry> constr_ex_entry =
                &CDataGenerator::CreateConstructedExclusionEntry( idx, 1);
            Scope.AddTopLevelSeqEntry(*constr_ex_entry);
            // test
            id.SetLocal().SetStr("construct_exclusion1");
            CTestHelper::ProcessBioseq(Scope, id,
                22, 22, "GCGTAGACATCCCAGAGCGGTG", "",
                0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
        }}
        {{
            CRef<CSeq_entry> constr_entry =
                &CDataGenerator::CreateConstructedEntry( idx, 2);
            Scope.AddTopLevelSeqEntry(*constr_entry);
            // test
            id.SetLocal().SetStr("constructed2");
            CTestHelper::ProcessBioseq(Scope, id,
                27, 27, "TACCGCCAATAACCTCAGCAGCAACAA", "",
                0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
            CRef<CSeq_entry> constr_ex_entry =
                &CDataGenerator::CreateConstructedExclusionEntry( idx, 2);
            Scope.AddTopLevelSeqEntry(*constr_ex_entry);
            // test
            id.SetLocal().SetStr("construct_exclusion2");
            CTestHelper::ProcessBioseq(Scope, id,
                22, 22, "GCGTAGACATCCCAGAGCGGTG", "",
                0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
        }}
    }

    // 1.2.7. one entry in two scopes
    for (idx = 1; idx <= 1; idx++) {
        // populate scopes
        CScope Scope1(*m_ObjMgr);
        CRef<CScope> pScope2 = new CScope(*m_ObjMgr);
        CRef<CSeq_entry> entry1 = &CDataGenerator::CreateTestEntry1(idx);
        CRef<CSeq_entry> entry2 = &CDataGenerator::CreateTestEntry2(idx);
        Scope1.AddTopLevelSeqEntry(*entry1);
        Scope1.AddTopLevelSeqEntry(*entry2);
        pScope2->AddTopLevelSeqEntry(*entry2);
        // Test with unresolvable references
        id.SetGi(21+idx*1000);
        CTestHelper::ProcessBioseq(*pScope2, id,
            22, 22,
            "NNNNNNNNNNNNNNNNNNNNNN",
            "NNNNNNNNNNNNNNNNNNNNNN",
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

    NcbiCout << " Passed" << NcbiEndl << NcbiEndl;

    return 0;
}


END_NCBI_SCOPE



/////////////////////////////////////////////////////////////////////////////
//  MAIN


USING_NCBI_SCOPE;

int main(int argc, const char* argv[])
{
    return CTestApp().AppMain(argc, argv, 0, eDS_Default, 0);
}
