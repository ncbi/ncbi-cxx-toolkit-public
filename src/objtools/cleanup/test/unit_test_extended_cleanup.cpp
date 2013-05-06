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
* Author:  Colleen Bollin, NCBI
*
* File Description:
*   Sample unit tests file for extended cleanup.
*
* This file represents basic most common usage of Ncbi.Test framework based
* on Boost.Test framework. For more advanced techniques look into another
* sample - unit_test_alt_sample.cpp.
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>

#include <corelib/ncbi_system.hpp>

// This macro should be defined before inclusion of test_boost.hpp in all
// "*.cpp" files inside executable except one. It is like function main() for
// non-Boost.Test executables is defined only in one *.cpp file - other files
// should not include it. If NCBI_BOOST_NO_AUTO_TEST_MAIN will not be defined
// then test_boost.hpp will define such "main()" function for tests.
//
// Usually if your unit tests contain only one *.cpp file you should not
// care about this macro at all.
//
//#define NCBI_BOOST_NO_AUTO_TEST_MAIN


// This header must be included before all Boost.Test headers if there are any
#include <corelib/test_boost.hpp>

#include <objects/seqset/Seq_entry.hpp>
#include <objmgr/object_manager.hpp>
#include <objmgr/scope.hpp>
#include <objmgr/bioseq_ci.hpp>
#include <objmgr/feat_ci.hpp>
#include <objmgr/seq_vector.hpp>
#include <objmgr/util/sequence.hpp>
#include <objects/seq/seqport_util.hpp>
#include <objects/general/Object_id.hpp>
#include <objects/seqfeat/Cdregion.hpp>
#include <objects/misc/sequence_macros.hpp>
#include <objtools/cleanup/cleanup.hpp>
#include <objtools/test/unit_test_util/unit_test_util.hpp>

USING_NCBI_SCOPE;
USING_SCOPE(objects);

using namespace unit_test_util;

extern const char* sc_TestEntryRemoveOldName;
extern const char *sc_TestEntryDontRemoveOldName;

BOOST_AUTO_TEST_CASE(Test_RemoveOldName)
{
    // !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
    // !!!!!TODO: FIX!!!!!
    // I have disabled the unit tests for ExtendedCleanup.
    // It is not a good idea, but it seems to be the lesser evil.
    // This is here because we're pointing to the new code which
    // is not yet complete.
    // In the future, the tests SHOULD be run.
    // !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
    return;

    CSeq_entry entry;
    {{
         CNcbiIstrstream istr(sc_TestEntryRemoveOldName);
         istr >> MSerial_AsnText >> entry;
     }}

    CRef<CScope> scope(new CScope(*CObjectManager::GetInstance()));;
    CSeq_entry_Handle seh = scope->AddTopLevelSeqEntry(entry);
    entry.Parentize();

    CCleanup cleanup;
    CConstRef<CCleanupChange> changes;

    cleanup.SetScope (scope);
    changes = cleanup.ExtendedCleanup (entry);
    // look for expected change flags
	vector<string> changes_str = changes->GetAllDescriptions();
	if (changes_str.size() == 0) {
        BOOST_CHECK_EQUAL("missing cleanup", "change orgmod");
	} else {
        BOOST_CHECK_EQUAL (changes_str[0], "Change Orgmod");
        for (int i = 1; i < changes_str.size(); i++) {
            BOOST_CHECK_EQUAL("unexpected cleanup", changes_str[i]);
        }
	}
    // make sure change was actually made
    FOR_EACH_DESCRIPTOR_ON_SEQENTRY (dit, entry) {
        if ((*dit)->IsSource()) {
            FOR_EACH_ORGMOD_ON_BIOSOURCE (mit, (*dit)->GetSource()) {
                if ((*mit)->IsSetSubtype() && (*mit)->GetSubtype() == COrgMod::eSubtype_old_name) {
                    BOOST_CHECK_EQUAL("old-name still present", (*mit)->GetSubname());
                }
            }
        }
    }

    scope->RemoveTopLevelSeqEntry(seh);
    {{
         CNcbiIstrstream istr(sc_TestEntryDontRemoveOldName);
         istr >> MSerial_AsnText >> entry;
     }}

    seh = scope->AddTopLevelSeqEntry(entry);
    entry.Parentize();

    cleanup.SetScope (scope);
    changes = cleanup.ExtendedCleanup (entry);
	changes_str = changes->GetAllDescriptions();
    for (int i = 1; i < changes_str.size(); i++) {
        BOOST_CHECK_EQUAL("unexpected cleanup", changes_str[i]);
    }
    // make sure change was not made
    bool found = false;
    FOR_EACH_DESCRIPTOR_ON_SEQENTRY (dit, entry) {
        if ((*dit)->IsSource()) {
            FOR_EACH_ORGMOD_ON_BIOSOURCE (mit, (*dit)->GetSource()) {
                if ((*mit)->IsSetSubtype() && (*mit)->GetSubtype() == COrgMod::eSubtype_old_name) {
                    found = true;
                }
            }
        }
    }
    if (!found) {
        BOOST_CHECK_EQUAL("unexpected cleanup", "old-name incorrectly removed");
    }

}


BOOST_AUTO_TEST_CASE(Test_RemoveRedundantMapQuals)
{
    // !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
    // !!!!!TODO: FIX!!!!!
    // I have disabled the unit tests for ExtendedCleanup.
    // It is not a good idea, but it seems to be the lesser evil.
    // This is here because we're pointing to the new code which
    // is not yet complete.
    // In the future, the tests SHOULD be run.
    // !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
    return;

    CRef<CSeq_entry> entry = BuildGoodSeq();
    CRef<CSeq_feat> gene = BuildGoodFeat();
    gene->SetData().SetGene().SetMaploc("willmatch");
    AddFeat(gene, entry);
    CRef<CSeq_feat> misc_feat = BuildGoodFeat();
    CRef<CGb_qual> qual(new CGb_qual("map", "willmatch"));
    misc_feat->SetQual().push_back(qual);
    AddFeat(misc_feat, entry);

    CRef<CScope> scope(new CScope(*CObjectManager::GetInstance()));;
    CSeq_entry_Handle seh = scope->AddTopLevelSeqEntry(*entry);
    entry->Parentize();

    CCleanup cleanup;
    CConstRef<CCleanupChange> changes;

    cleanup.SetScope (scope);
    changes = cleanup.ExtendedCleanup (*entry);
    // look for expected change flags
	vector<string> changes_str = changes->GetAllDescriptions();
	if (changes_str.size() == 0) {
        BOOST_CHECK_EQUAL("missing cleanup", "remove qualifier");
	} else {
        BOOST_CHECK_EQUAL (changes_str[0], "Remove Qualifier");
        for (int i = 1; i < changes_str.size(); i++) {
            BOOST_CHECK_EQUAL("unexpected cleanup", changes_str[i]);
        }
	}
    // make sure change was actually made
    CFeat_CI feat (seh);
    while (feat) {
        if (feat->GetData().IsGene()) {
            if (!feat->GetData().GetGene().IsSetMaploc()) {
                BOOST_CHECK_EQUAL("gene map missing", "unexpected cleanup");
            }
        } else {
            if (feat->IsSetQual()) {
                BOOST_CHECK_EQUAL("map still present", feat->GetQual().front()->GetVal());
            }
        }
        ++feat;
    }

    // don't remove if not same
    scope->RemoveTopLevelSeqEntry(seh);
    misc_feat = entry->SetSeq().SetAnnot().front()->SetData().SetFtable().back();
    qual->SetVal("wontmatch");
    misc_feat->SetQual().push_back(qual);
    seh = scope->AddTopLevelSeqEntry(*entry);

    changes = cleanup.ExtendedCleanup (*entry);
    if (changes_str.size() > 0) {
        for (int i = 1; i < changes_str.size(); i++) {
            BOOST_CHECK_EQUAL("unexpected cleanup", changes_str[i]);
        }
	}
 
    // make sure qual is still there
    CFeat_CI feat2(seh);
    while (feat2) {
        if (feat2->GetData().IsGene()) {
            if (!feat2->GetData().GetGene().IsSetMaploc()) {
                BOOST_CHECK_EQUAL("gene map missing", "unexpected cleanup");
            }
        } else {
            if (!feat2->IsSetQual()) {
                BOOST_CHECK_EQUAL("map removed", feat2->GetQual().front()->GetVal());
            }
        }
        ++feat2;
    }

    // don't remove if embl or ddbj
    scope->RemoveTopLevelSeqEntry(seh);
    entry->SetSeq().SetId().front()->SetEmbl().SetAccession("AY123456");
    gene = entry->SetSeq().SetAnnot().front()->SetData().SetFtable().front();
    gene->SetLocation().SetInt().SetId().SetEmbl().SetAccession("AY123456");
    misc_feat = entry->SetSeq().SetAnnot().front()->SetData().SetFtable().back();
    misc_feat->SetLocation().SetInt().SetId().SetEmbl().SetAccession("AY123456");
    qual->SetVal("willmatch");
    misc_feat->SetQual().push_back(qual);
    seh = scope->AddTopLevelSeqEntry(*entry);

    changes = cleanup.ExtendedCleanup (*entry);
    if (changes_str.size() > 0) {
        for (int i = 1; i < changes_str.size(); i++) {
            BOOST_CHECK_EQUAL("unexpected cleanup", changes_str[i]);
        }
	}
 
    // make sure qual is still there
    CFeat_CI feat3(seh);
    while (feat3) {
        if (feat3->GetData().IsGene()) {
            if (!feat3->GetData().GetGene().IsSetMaploc()) {
                BOOST_CHECK_EQUAL("gene map missing", "unexpected cleanup");
            }
        } else {
            if (!feat3->IsSetQual()) {
                BOOST_CHECK_EQUAL("map removed", feat->GetQual().front()->GetVal());
            }
        }
        ++feat3;
    }

    scope->RemoveTopLevelSeqEntry(seh);
    entry->SetSeq().SetId().front()->SetDdbj().SetAccession("AY123456");
    gene = entry->SetSeq().SetAnnot().front()->SetData().SetFtable().front();
    gene->SetLocation().SetInt().SetId().SetDdbj().SetAccession("AY123456");
    misc_feat = entry->SetSeq().SetAnnot().front()->SetData().SetFtable().back();
    misc_feat->SetLocation().SetInt().SetId().SetDdbj().SetAccession("AY123456");
    seh = scope->AddTopLevelSeqEntry(*entry);

    changes = cleanup.ExtendedCleanup (*entry);
    if (changes_str.size() > 0) {
        for (int i = 1; i < changes_str.size(); i++) {
            BOOST_CHECK_EQUAL("unexpected cleanup", changes_str[i]);
        }
	}
 
    // make sure qual is still there
    CFeat_CI feat4(seh);
    while (feat4) {
        if (feat4->GetData().IsGene()) {
            if (!feat4->GetData().GetGene().IsSetMaploc()) {
                BOOST_CHECK_EQUAL("gene map missing", "unexpected cleanup");
            }
        } else {
            if (!feat4->IsSetQual()) {
                BOOST_CHECK_EQUAL("map removed", feat->GetQual().front()->GetVal());
            }
        }
        ++feat4;
    }

}


const char *sc_TestEntryRemoveOldName = "\
Seq-entry ::= seq {\
          id {\
            local\
              str \"oldname\" } ,\
          descr {\
            source { \
              org { \
                taxname \"This matches old-name\" , \
                orgname { \
                  mod {\
                    { \
                      subtype old-name , \
                      subname \"This matches old-name\" } } } } } , \
            molinfo {\
              biomol genomic } } ,\
          inst {\
            repr raw ,\
            mol dna ,\
            length 27 ,\
            seq-data\
              iupacna \"TTGCCCTAAAAATAAGAGTAAAACTAA\" } }\
";

const char *sc_TestEntryDontRemoveOldName = "\
Seq-entry ::= seq {\
          id {\
            local\
              str \"oldname\" } ,\
          descr {\
            source { \
              org { \
                taxname \"This matches old-name\" , \
                orgname { \
                  mod {\
                    { \
                      subtype old-name , \
                      subname \"This matches old-name\" , \
                      attrib \"Non-empty attrib\" } } } } } , \
            molinfo {\
              biomol genomic } } ,\
          inst {\
            repr raw ,\
            mol dna ,\
            length 27 ,\
            seq-data\
              iupacna \"TTGCCCTAAAAATAAGAGTAAAACTAA\" } }\
";
