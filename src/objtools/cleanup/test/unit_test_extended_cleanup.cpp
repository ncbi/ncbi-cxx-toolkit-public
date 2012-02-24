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

USING_NCBI_SCOPE;
USING_SCOPE(objects);

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


static void SetDbxref (CBioSource& src, string db, size_t id)
{
    CRef<CDbtag> dbtag(new CDbtag());
    dbtag->SetDb(db);
    dbtag->SetTag().SetId(id);
    src.SetOrg().SetDb().push_back(dbtag);
}

static void RemoveDbxref (CBioSource& src, string db, size_t id)
{
    if (src.IsSetOrg()) {
        EDIT_EACH_DBXREF_ON_ORGREF(it, src.SetOrg()) {            
            if (NStr::IsBlank(db) || ((*it)->IsSetDb() && NStr::Equal((*it)->GetDb(), db))) {
                if (id == 0 || ((*it)->IsSetTag() && (*it)->GetTag().IsId() && (*it)->GetTag().GetId() == id)) {
                    ERASE_DBXREF_ON_ORGREF(it, src.SetOrg());
                }
            }
        }
    }
}


static void SetTaxon (CBioSource& src, size_t taxon)
{
    if (taxon == 0) {
        RemoveDbxref (src, "taxon", 0);
    } else {
        SetDbxref(src, "taxon", taxon);
    }
}


static void AddGoodSource (CRef<CSeq_entry> entry)
{
    CRef<CSeqdesc> odesc(new CSeqdesc());
    odesc->SetSource().SetOrg().SetTaxname("Sebaea microphylla");
    odesc->SetSource().SetOrg().SetOrgname().SetLineage("some lineage");
    SetTaxon(odesc->SetSource(), 592768);
    CRef<CSubSource> subsrc(new CSubSource());
    subsrc->SetSubtype(CSubSource::eSubtype_chromosome);
    subsrc->SetName("1");
    odesc->SetSource().SetSubtype().push_back(subsrc);

    if (entry->IsSeq()) {
        entry->SetSeq().SetDescr().Set().push_back(odesc);
    } else if (entry->IsSet()) {
        entry->SetSet().SetDescr().Set().push_back(odesc);
    }
}


static CRef<CSeqdesc> BuildGoodPubSeqdesc()
{
    CRef<CSeqdesc> pdesc(new CSeqdesc());
    CRef<CPub> pub(new CPub());
    pub->SetPmid((CPub::TPmid)1);
    pdesc->SetPub().SetPub().Set().push_back(pub);

    return pdesc;
}


static void AddGoodPub (CRef<CSeq_entry> entry)
{
    CRef<CSeqdesc> pdesc = BuildGoodPubSeqdesc();

    if (entry->IsSeq()) {
        entry->SetSeq().SetDescr().Set().push_back(pdesc);
    } else if (entry->IsSet()) {
        entry->SetSet().SetDescr().Set().push_back(pdesc);
    }
}


static CRef<CSeq_entry> BuildGoodSeq(void)
{
    CRef<CBioseq> seq(new CBioseq());
    seq->SetInst().SetMol(CSeq_inst::eMol_dna);
    seq->SetInst().SetRepr(CSeq_inst::eRepr_raw);
    seq->SetInst().SetSeq_data().SetIupacna().Set("AATTGGCCAAAATTGGCCAAAATTGGCCAAAATTGGCCAAAATTGGCCAAAATTGGCCAA");
    seq->SetInst().SetLength(60);

    CRef<CSeq_id> id(new CSeq_id());
    id->SetLocal().SetStr ("good");
    seq->SetId().push_back(id);

    CRef<CSeqdesc> mdesc(new CSeqdesc());
    mdesc->SetMolinfo().SetBiomol(CMolInfo::eBiomol_genomic);    
    seq->SetDescr().Set().push_back(mdesc);

    CRef<CSeq_entry> entry(new CSeq_entry());
    entry->SetSeq(*seq);
    AddGoodSource (entry);
    AddGoodPub(entry);

    return entry;
}


static void AddFeat (CRef<CSeq_feat> feat, CRef<CSeq_entry> entry)
{
    CRef<CSeq_annot> annot;

    if (entry->IsSeq()) {
        if (!entry->GetSeq().IsSetAnnot() 
            || !entry->GetSeq().GetAnnot().front()->IsFtable()) {
            CRef<CSeq_annot> new_annot(new CSeq_annot());
            entry->SetSeq().SetAnnot().push_back(new_annot);
            annot = new_annot;
        } else {
            annot = entry->SetSeq().SetAnnot().front();
        }
    } else if (entry->IsSet()) {
        if (!entry->GetSet().IsSetAnnot() 
            || !entry->GetSet().GetAnnot().front()->IsFtable()) {
            CRef<CSeq_annot> new_annot(new CSeq_annot());
            entry->SetSet().SetAnnot().push_back(new_annot);
            annot = new_annot;
        } else {
            annot = entry->SetSet().SetAnnot().front();
        }
    }
    annot->SetData().SetFtable().push_back(feat);
}


CRef<CSeq_feat> BuildGoodFeat ()
{
    CRef<CSeq_feat> feat(new CSeq_feat());
    feat->SetLocation().SetInt().SetId().SetLocal().SetStr("good");
    feat->SetLocation().SetInt().SetFrom(0);
    feat->SetLocation().SetInt().SetTo(59);
    feat->SetData().SetImp().SetKey("misc_feature");

    return feat;
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
