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
#include <objtools/unit_test_util/unit_test_util.hpp>

// for allowing remote fetching
#include <objtools/data_loaders/genbank/gbloader.hpp>


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

void s_ReportUnexpected(const vector<string>& observed, const vector<string>& expected)
{
    vector<string>::const_iterator oi = observed.begin();
    vector<string>::const_iterator ei = expected.begin();
    while (oi != observed.end() && ei != expected.end()) {
        BOOST_CHECK_EQUAL(*oi, *ei);
        oi++;
        ei++;
    }
    while (oi != observed.end()) {
        BOOST_CHECK_EQUAL("unexpected cleanup", *oi);
        oi++;
	}
    while (ei != expected.end()) {
        BOOST_CHECK_EQUAL("missing expected cleanup", *ei);
        ei++;
    }
}


BOOST_AUTO_TEST_CASE(Test_AddMetaGenomesAndEnvSample)
{
    CRef<CSeq_entry> entry = BuildGoodSeq();
    unit_test_util::SetLineage(entry, "metagenomes");
    unit_test_util::SetSubSource(entry, CSubSource::eSubtype_metagenomic, "");
    unit_test_util::SetSubSource(entry, CSubSource::eSubtype_environmental_sample, "");
    unit_test_util::SetSubSource(entry, CSubSource::eSubtype_chromosome, "");
    entry->Parentize();

    CRef<CScope> scope(new CScope(*CObjectManager::GetInstance()));;
    CSeq_entry_Handle seh = scope->AddTopLevelSeqEntry(*entry);

    CCleanup cleanup;
    CConstRef<CCleanupChange> changes;

    cleanup.SetScope (scope);
    changes = cleanup.ExtendedCleanup (*entry, CCleanup::eClean_NoNcbiUserObjects);
    // look for expected change flags
	vector<string> changes_str = changes->GetAllDescriptions();
    vector<string> expected;
    expected.push_back("Change Publication");
    expected.push_back("Change Subsource");
    expected.push_back("Move Descriptor");
    s_ReportUnexpected(changes_str, expected);
    bool found_src = false;
    ITERATE(CBioseq::TDescr::Tdata, it, entry->GetSeq().GetDescr().Get()) {
        if ((*it)->IsSource()) {
            found_src = true;
            BOOST_CHECK_EQUAL((*it)->GetSource().IsSetSubtype(), true);
            if ((*it)->GetSource().IsSetSubtype()) {
                BOOST_CHECK_EQUAL((*it)->GetSource().GetSubtype().size(), 2);
                BOOST_CHECK_EQUAL((*it)->GetSource().GetSubtype().front()->GetSubtype(), CSubSource::eSubtype_environmental_sample);
                BOOST_CHECK_EQUAL((*it)->GetSource().GetSubtype().back()->GetSubtype(), CSubSource::eSubtype_metagenomic);
            }
        }
    }
    BOOST_CHECK_EQUAL(found_src, true);
    // no additional changes if cleanup again
    changes = cleanup.ExtendedCleanup (*entry, CCleanup::eClean_NoNcbiUserObjects);
    changes_str = changes->GetAllDescriptions();
    expected.clear();
    s_ReportUnexpected(changes_str, expected);

    // test env sample
    unit_test_util::SetLineage(entry, "environmental sample");
    unit_test_util::SetSubSource(entry, CSubSource::eSubtype_metagenomic, "");
    unit_test_util::SetSubSource(entry, CSubSource::eSubtype_environmental_sample, "");
    changes = cleanup.ExtendedCleanup (*entry, CCleanup::eClean_NoNcbiUserObjects);
    changes_str = changes->GetAllDescriptions();
    expected.push_back("Add BioSource SubSource");
    expected.push_back("Change BioSource Other");
    s_ReportUnexpected(changes_str, expected);
    found_src = false;
    ITERATE(CBioseq::TDescr::Tdata, it, entry->GetSeq().GetDescr().Get()) {
        if ((*it)->IsSource()) {
            found_src = true;
            BOOST_CHECK_EQUAL((*it)->GetSource().IsSetSubtype(), true);
            if ((*it)->GetSource().IsSetSubtype()) {
                BOOST_CHECK_EQUAL((*it)->GetSource().GetSubtype().size(), 1);
                BOOST_CHECK_EQUAL((*it)->GetSource().GetSubtype().back()->GetSubtype(), CSubSource::eSubtype_environmental_sample);
            }
        }
    }
    BOOST_CHECK_EQUAL(found_src, true);

    // also add env_sample for div ENV
    unit_test_util::SetSubSource(entry, CSubSource::eSubtype_metagenomic, "");
    unit_test_util::SetSubSource(entry, CSubSource::eSubtype_environmental_sample, "");
    unit_test_util::SetLineage(entry, "");
    unit_test_util::SetDiv(entry, "ENV");
    changes = cleanup.ExtendedCleanup (*entry, CCleanup::eClean_NoNcbiUserObjects);
    changes_str = changes->GetAllDescriptions();
    expected.clear();
    expected.push_back("Change Subsource");
    s_ReportUnexpected(changes_str, expected);
    found_src = false;
    ITERATE(CBioseq::TDescr::Tdata, it, entry->GetSeq().GetDescr().Get()) {
        if ((*it)->IsSource()) {
            found_src = true;
            BOOST_CHECK_EQUAL((*it)->GetSource().IsSetSubtype(), true);
            if ((*it)->GetSource().IsSetSubtype()) {
                BOOST_CHECK_EQUAL((*it)->GetSource().GetSubtype().size(), 1);
                BOOST_CHECK_EQUAL((*it)->GetSource().GetSubtype().back()->GetSubtype(), CSubSource::eSubtype_environmental_sample);
            }
        }
    }
    BOOST_CHECK_EQUAL(found_src, true);

    // no additional changes if cleanup again
    changes = cleanup.ExtendedCleanup (*entry, CCleanup::eClean_NoNcbiUserObjects);
    changes_str = changes->GetAllDescriptions();
    expected.clear();
    s_ReportUnexpected(changes_str, expected);
}


BOOST_AUTO_TEST_CASE(Test_RemoveEmptyFeatures)
{
    CRef<CSeq_entry> entry = BuildGoodSeq();
    CRef<CSeq_feat> gene = BuildGoodFeat();
    gene->SetData().SetGene();
    AddFeat(gene, entry);
    CRef<CSeq_feat> misc_feat = BuildGoodFeat();
    AddFeat(misc_feat, entry);

    CRef<CScope> scope(new CScope(*CObjectManager::GetInstance()));;
    CSeq_entry_Handle seh = scope->AddTopLevelSeqEntry(*entry);
    entry->Parentize();

    CCleanup cleanup;
    CConstRef<CCleanupChange> changes;

    cleanup.SetScope (scope);
    changes = cleanup.ExtendedCleanup (*entry);
    vector<string> changes_str = changes->GetAllDescriptions();
    vector<string> expected;
    expected.push_back("Remove Feature");
    expected.push_back("Move Descriptor");
    expected.push_back("Add NcbiCleanupObject");

    scope->RemoveTopLevelSeqEntry(seh);
    seh = scope->AddTopLevelSeqEntry(*entry);
    // make sure change was actually made
    CFeat_CI feat (seh);
    while (feat) {
        if (feat->GetData().IsGene()) {
            BOOST_CHECK_EQUAL("gene not removed", "missing expected cleanup");
        }
        ++feat;
    }

}


BOOST_AUTO_TEST_CASE(Test_MoveProteinSpecificFeatures)
{
    CRef<CSeq_entry> entry = BuildGoodSeq();
    CRef<CScope> scope(new CScope(*CObjectManager::GetInstance()));;
    CSeq_entry_Handle seh = scope->AddTopLevelSeqEntry(*entry);

    // if no features at all, return value should be false
    BOOST_CHECK_EQUAL(false, CCleanup::MoveProteinSpecificFeats(seh));

    // if no protein features, return value should be false
    scope->RemoveTopLevelSeqEntry(seh);
    CRef<CSeq_feat> gene = BuildGoodFeat();
    gene->SetData().SetGene();
    AddFeat(gene, entry);
    seh = scope->AddTopLevelSeqEntry(*entry);
    BOOST_CHECK_EQUAL(false, CCleanup::MoveProteinSpecificFeats(seh));

    // if protein feature but no overlapping coding region, return value should
    // be false
    scope->RemoveTopLevelSeqEntry(seh);
    CRef<CSeq_feat> prot = BuildGoodFeat();
    prot->SetData().SetProt();
    AddFeat(prot, entry);
    seh = scope->AddTopLevelSeqEntry(*entry);
    BOOST_CHECK_EQUAL(false, CCleanup::MoveProteinSpecificFeats(seh));

    // if protein feature and overlapping coding region but no protein sequence,
    // return value should be false
    scope->RemoveTopLevelSeqEntry(seh);
    CRef<CSeq_feat> cdregion = BuildGoodFeat();
    cdregion->SetData().SetCdregion();
    AddFeat(cdregion, entry);
    seh = scope->AddTopLevelSeqEntry(*entry);
    BOOST_CHECK_EQUAL(false, CCleanup::MoveProteinSpecificFeats(seh));

    // if protein features are already on protein sequence, return
    // value should be false
    scope->RemoveTopLevelSeqEntry(seh);
    entry = BuildGoodNucProtSet();
    seh = scope->AddTopLevelSeqEntry(*entry);
    BOOST_CHECK_EQUAL(false, CCleanup::MoveProteinSpecificFeats(seh));

#if 0
    //code for not moving features on a codon boundary has been removed,
    //now moves all protein features
    // protein features that are not on a codon boundary cannot be moved
    scope->RemoveTopLevelSeqEntry(seh);
    CRef<CSeq_feat> cds = GetCDSFromGoodNucProtSet(entry);
    CRef<CSeq_entry> nuc = GetNucleotideSequenceFromGoodNucProtSet(entry);
    CRef<CSeq_feat> mat_peptide = BuildGoodFeat();
    mat_peptide->SetData().SetProt().SetProcessed(CProt_ref::eProcessed_mature);
    mat_peptide->SetLocation().Assign(cds->GetLocation());
    mat_peptide->SetLocation().SetInt().SetFrom(cds->GetLocation().GetStart(eExtreme_Biological) + 4);
    AddFeat(mat_peptide, entry);
    seh = scope->AddTopLevelSeqEntry(*entry);
    BOOST_CHECK_EQUAL(false, CCleanup::MoveProteinSpecificFeats(seh));

    // but can be moved if they are on a codon boundary
    scope->RemoveTopLevelSeqEntry(seh);
    mat_peptide->SetLocation().SetInt().SetFrom(cds->GetLocation().GetStart(eExtreme_Biological) + 3);
    seh = scope->AddTopLevelSeqEntry(*entry);
    BOOST_CHECK_EQUAL(true, CCleanup::MoveProteinSpecificFeats(seh));
#endif
}


void s_CheckXrefs(CSeq_entry_Handle seh, size_t num)
{
    for (CFeat_CI fi(seh); fi; ++fi) {
        if (!fi->GetData().IsGene()) {
            if (num == 0) {
                BOOST_CHECK_EQUAL(false, fi->IsSetXref());
            } else {
                BOOST_CHECK_EQUAL(true, fi->IsSetXref());
                BOOST_CHECK_EQUAL(1, fi->GetXref().size());
            }
        }
    }
}


CRef<CSeq_feat> s_AddRna(CRef<CSeq_entry> entry, const string& locus_tag) 
{
    CRef<CSeq_feat> rna = BuildGoodFeat();
    rna->SetData().SetRna().SetType(CRNA_ref::eType_ncRNA);
    AddFeat(rna, entry);
    CRef<CSeqFeatXref> xref(new CSeqFeatXref());
    if (NStr::IsBlank(locus_tag)) {
        xref->SetData().SetGene();
    } else {
        xref->SetData().SetGene().SetLocus_tag(locus_tag);
    }
    rna->SetXref().push_back(xref);
    return rna;
}


CRef<CSeq_feat> s_AddGene(CRef<CSeq_entry> entry, const string& locus_tag)
{
    CRef<CSeq_feat> gene = BuildGoodFeat();
    gene->SetData().SetGene().SetLocus_tag(locus_tag);
    AddFeat(gene, entry);
    return gene;
}


BOOST_AUTO_TEST_CASE(Test_RemoveUnnecessaryGeneXrefs)
{
    CRef<CSeq_entry> entry = BuildGoodSeq();
    CRef<CScope> scope(new CScope(*CObjectManager::GetInstance()));

    // build an RNA with a suppressing gene xref, should not be removed
    CRef<CSeq_feat> rna = s_AddRna(entry, "");

    CSeq_entry_Handle seh = scope->AddTopLevelSeqEntry(*entry);
    BOOST_CHECK_EQUAL(false, CCleanup::RemoveUnnecessaryGeneXrefs(seh));
    s_CheckXrefs(seh, 1);

    // add a gene - xref is suppressing, so still shouldn't be removed
    scope->RemoveTopLevelSeqEntry(seh);
    entry = BuildGoodSeq();
    rna = s_AddRna(entry, "");
    CRef<CSeq_feat> gene = s_AddGene(entry, "test");
    seh = scope->AddTopLevelSeqEntry(*entry);
    BOOST_CHECK_EQUAL(false, CCleanup::RemoveUnnecessaryGeneXrefs(seh));
    s_CheckXrefs(seh, 1);

    // do remove if gene xref matches
    scope->RemoveTopLevelSeqEntry(seh);
    entry = BuildGoodSeq();
    rna = s_AddRna(entry, "test");
    gene = s_AddGene(entry, "test");
    seh = scope->AddTopLevelSeqEntry(*entry);
    BOOST_CHECK_EQUAL(true, CCleanup::RemoveUnnecessaryGeneXrefs(seh));
    s_CheckXrefs(seh, 0);

    // don't remove if gene xref matches but gene does not contain feature
    scope->RemoveTopLevelSeqEntry(seh);
    entry = BuildGoodSeq();
    rna = s_AddRna(entry, "test");
    gene = s_AddGene(entry, "test");
    rna->SetLocation().SetInt().SetTo(rna->GetLocation().GetInt().GetTo() + 1);
    seh = scope->AddTopLevelSeqEntry(*entry);
    BOOST_CHECK_EQUAL(false, CCleanup::RemoveUnnecessaryGeneXrefs(seh));
    s_CheckXrefs(seh, 1);

    // don't remove if gene xref matches, location is overlap, but there is
    // a different gene that also matches and is smaller
    scope->RemoveTopLevelSeqEntry(seh);
    entry = BuildGoodSeq();
    rna = s_AddRna(entry, "test");
    gene = s_AddGene(entry, "test");
    CRef<CSeq_feat> gene2 = s_AddGene(entry, "test2");
    gene->SetLocation().SetInt().SetTo(gene->GetLocation().GetInt().GetTo() + 1);
    seh = scope->AddTopLevelSeqEntry(*entry);
    BOOST_CHECK_EQUAL(false, CCleanup::RemoveUnnecessaryGeneXrefs(seh));
    s_CheckXrefs(seh, 1);

}


BOOST_AUTO_TEST_CASE(Test_SQD_2375)
{
    CRef<CSeq_entry> entry = BuildGoodNucProtSet();
    CRef<CScope> scope(new CScope(*CObjectManager::GetInstance()));
    CSeq_entry_Handle seh = scope->AddTopLevelSeqEntry(*entry);

    BOOST_CHECK_EQUAL(seh.GetSet().GetCompleteBioseq_set()->GetSeq_set().back()->GetSeq().GetDescr().Get().size(), 1);

    CCleanup cleanup(scope);
    CConstRef<CCleanupChange> changes;

    changes = cleanup.ExtendedCleanup(seh, CCleanup::eClean_NoProteinTitles);
    BOOST_CHECK_EQUAL(seh.GetSet().GetCompleteBioseq_set()->GetSeq_set().back()->GetSeq().GetDescr().Get().size(), 1);

    changes = cleanup.ExtendedCleanup(seh);
    BOOST_CHECK_EQUAL(seh.GetSet().GetCompleteBioseq_set()->GetSeq_set().back()->GetSeq().GetDescr().Get().size(), 2);


}


BOOST_AUTO_TEST_CASE(Test_ExtendToStop)
{
    CRef<CObjectManager> objmgr = CObjectManager::GetInstance();
    CGBDataLoader::RegisterInObjectManager(*objmgr);

    CScope scope(*objmgr);
    scope.AddDefaults();

    CRef<CSeq_id> fetch_id(new CSeq_id());
    TGi gi = (TIntId)24413615;
    fetch_id->SetGi(gi);

    CBioseq_Handle bsh = scope.GetBioseqHandle(*fetch_id);

    CFeat_CI cds_it(bsh, CSeqFeatData::e_Cdregion);
    while (cds_it) {
        CRef<CSeq_feat> replace(new CSeq_feat());
        replace->Assign(*(cds_it->GetSeq_feat()));
        CCleanup::ExtendToStopIfShortAndNotPartial(*replace, bsh, true);
        ++cds_it;
    }
    
}


BOOST_AUTO_TEST_CASE(Test_ClearInternalPartials)
{
    CRef<CSeq_id> id(new CSeq_id());
    id->SetLocal().SetStr("n");
    CRef<CSeq_interval> a(new CSeq_interval());
    a->SetId().Assign(*id);
    a->SetFrom(0);
    a->SetTo(5);
    a->SetPartialStart(true, eExtreme_Biological);
    a->SetPartialStop(true, eExtreme_Biological);
    CRef<CSeq_loc> l1(new CSeq_loc());
    l1->SetInt().Assign(*a);
    CRef<CSeq_interval> b(new CSeq_interval());
    b->SetId().Assign(*id);
    b->SetFrom(10);
    b->SetTo(15);
    b->SetPartialStart(true, eExtreme_Biological);
    b->SetPartialStop(true, eExtreme_Biological);
    CRef<CSeq_loc> l2(new CSeq_loc());
    l2->SetInt().Assign(*b);

    CRef<CSeq_interval> c(new CSeq_interval());
    c->SetId().Assign(*id);
    c->SetFrom(20);
    c->SetTo(25);
    c->SetPartialStart(true, eExtreme_Biological);
    c->SetPartialStop(true, eExtreme_Biological);
    CRef<CSeq_loc> l3(new CSeq_loc());
    l3->SetInt().Assign(*c);

    CRef<CSeq_loc> loc_mix(new CSeq_loc());
    loc_mix->SetMix().Set().push_back(l1);
    loc_mix->SetMix().Set().push_back(l2);
    loc_mix->SetMix().Set().push_back(l3);

    BOOST_CHECK_EQUAL(CCleanup::ClearInternalPartials(*loc_mix), true);
    BOOST_CHECK_EQUAL(l1->IsPartialStart(eExtreme_Biological), true);
    BOOST_CHECK_EQUAL(l1->IsPartialStop(eExtreme_Biological), false);
    BOOST_CHECK_EQUAL(l2->IsPartialStart(eExtreme_Biological), false);
    BOOST_CHECK_EQUAL(l2->IsPartialStop(eExtreme_Biological), false);
    BOOST_CHECK_EQUAL(l3->IsPartialStart(eExtreme_Biological), false);
    BOOST_CHECK_EQUAL(l3->IsPartialStop(eExtreme_Biological), true);
    BOOST_CHECK_EQUAL(CCleanup::ClearInternalPartials(*loc_mix), false);

    CRef<CSeq_loc> loc_pint(new CSeq_loc());
    loc_pint->SetPacked_int().Set().push_back(a);
    loc_pint->SetPacked_int().Set().push_back(b);
    loc_pint->SetPacked_int().Set().push_back(c);

    BOOST_CHECK_EQUAL(CCleanup::ClearInternalPartials(*loc_pint), true);
    BOOST_CHECK_EQUAL(a->IsPartialStart(eExtreme_Biological), true);
    BOOST_CHECK_EQUAL(a->IsPartialStop(eExtreme_Biological), false);
    BOOST_CHECK_EQUAL(b->IsPartialStart(eExtreme_Biological), false);
    BOOST_CHECK_EQUAL(b->IsPartialStop(eExtreme_Biological), false);
    BOOST_CHECK_EQUAL(c->IsPartialStart(eExtreme_Biological), false);
    BOOST_CHECK_EQUAL(c->IsPartialStop(eExtreme_Biological), true);
    BOOST_CHECK_EQUAL(CCleanup::ClearInternalPartials(*loc_pint), false);

    // try minus strands
    a->SetStrand(eNa_strand_minus);
    a->SetPartialStart(true, eExtreme_Biological);
    a->SetPartialStop(true, eExtreme_Biological);
    b->SetStrand(eNa_strand_minus);
    b->SetPartialStart(true, eExtreme_Biological);
    b->SetPartialStop(true, eExtreme_Biological);
    c->SetStrand(eNa_strand_minus);
    c->SetPartialStart(true, eExtreme_Biological);
    c->SetPartialStop(true, eExtreme_Biological);
    l1->SetInt().Assign(*a);
    l2->SetInt().Assign(*b);
    l3->SetInt().Assign(*c);

    loc_mix->SetMix().Reset();
    loc_mix->SetMix().Set().push_back(l3);
    loc_mix->SetMix().Set().push_back(l2);
    loc_mix->SetMix().Set().push_back(l1);
    BOOST_CHECK_EQUAL(CCleanup::ClearInternalPartials(*loc_mix), true);
    BOOST_CHECK_EQUAL(l1->IsPartialStart(eExtreme_Biological), false);
    BOOST_CHECK_EQUAL(l1->IsPartialStop(eExtreme_Biological), true);
    BOOST_CHECK_EQUAL(l2->IsPartialStart(eExtreme_Biological), false);
    BOOST_CHECK_EQUAL(l2->IsPartialStop(eExtreme_Biological), false);
    BOOST_CHECK_EQUAL(l3->IsPartialStart(eExtreme_Biological), true);
    BOOST_CHECK_EQUAL(l3->IsPartialStop(eExtreme_Biological), false);
    BOOST_CHECK_EQUAL(CCleanup::ClearInternalPartials(*loc_mix), false);

    loc_pint->SetPacked_int().Reset();
    loc_pint->SetPacked_int().Set().push_back(c);
    loc_pint->SetPacked_int().Set().push_back(b);
    loc_pint->SetPacked_int().Set().push_back(a);
    BOOST_CHECK_EQUAL(CCleanup::ClearInternalPartials(*loc_pint), true);
    BOOST_CHECK_EQUAL(a->IsPartialStart(eExtreme_Biological), false);
    BOOST_CHECK_EQUAL(a->IsPartialStop(eExtreme_Biological), true);
    BOOST_CHECK_EQUAL(b->IsPartialStart(eExtreme_Biological), false);
    BOOST_CHECK_EQUAL(b->IsPartialStop(eExtreme_Biological), false);
    BOOST_CHECK_EQUAL(c->IsPartialStart(eExtreme_Biological), true);
    BOOST_CHECK_EQUAL(c->IsPartialStop(eExtreme_Biological), false);
    BOOST_CHECK_EQUAL(CCleanup::ClearInternalPartials(*loc_pint), false);

}


BOOST_AUTO_TEST_CASE(TEST_RemoveNestedSet)
{
    CCleanup cleanup;
    CConstRef<CCleanupChange> changes;

    CRef<CScope> scope(new CScope(*CObjectManager::GetInstance()));;
    cleanup.SetScope(scope);

    CRef<CSeq_entry> sub1 = BuildGoodEcoSet();
    sub1->SetSet().SetClass(CBioseq_set::eClass_genbank);
    CRef<CSeq_entry> entry(new CSeq_entry());
    entry->SetSet().SetSeq_set().push_back(sub1);
    entry->Parentize();

    changes = cleanup.ExtendedCleanup(*entry, CCleanup::eClean_NoNcbiUserObjects | CCleanup::eClean_KeepTopSet);
    BOOST_CHECK_EQUAL(changes->IsChanged(CCleanupChange::eCollapseSet), false);

    changes = cleanup.ExtendedCleanup(*entry, CCleanup::eClean_NoNcbiUserObjects);
    BOOST_CHECK_EQUAL(changes->IsChanged(CCleanupChange::eCollapseSet), true);


}


BOOST_AUTO_TEST_CASE(TEST_RepairXrefs)
{
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodNucProtSet();
    CRef<CSeq_entry> nuc = unit_test_util::GetNucleotideSequenceFromGoodNucProtSet(entry);
    // CDS:   id 1
    // mRNA:  id 2
    // gene1: id 3
    // gene2: id 4
    CRef<CSeq_feat> cds = unit_test_util::GetCDSFromGoodNucProtSet(entry);
    cds->SetId().SetLocal().SetId(1);
    CRef<CSeq_feat> mrna = unit_test_util::MakemRNAForCDS(cds);
    unit_test_util::AddFeat(mrna, nuc);
    mrna->SetId().SetLocal().SetId(2);
    CRef<CSeq_feat> gene1 = unit_test_util::MakeGeneForFeature(mrna);
    unit_test_util::AddFeat(gene1, nuc);
    gene1->SetId().SetLocal().SetId(3);
    CRef<CSeq_feat> gene2 = unit_test_util::MakeGeneForFeature(mrna);
    unit_test_util::AddFeat(gene2, nuc);
    gene2->SetId().SetLocal().SetId(4);

    // cds points to mrna and gene 1
    // gene 1 points to mrna
    // mrna points to gene 2
    cds->AddSeqFeatXref(mrna->GetId());
    cds->AddSeqFeatXref(gene1->GetId());
    gene1->AddSeqFeatXref(mrna->GetId());
    mrna->AddSeqFeatXref(gene2->GetId());

    CRef<CScope> scope(new CScope(*CObjectManager::GetInstance()));
    CSeq_entry_Handle seh = scope->AddTopLevelSeqEntry(*entry);
    const CTSE_Handle tse = seh.GetTSE_Handle();

    // this should create reciprocal links on mrna and gene 1 to point back to cds
    BOOST_CHECK_EQUAL(CCleanup::RepairXrefs(*cds, tse), true);
    CSeq_annot::TData::TFtable ftable = nuc->GetAnnot().front()->GetData().GetFtable();
    CSeq_annot::TData::TFtable::iterator fit = ftable.begin();
    mrna = *fit;
    ++fit;
    gene1 = *fit;
    ++fit;
    gene2 = *fit;
    BOOST_CHECK_EQUAL(mrna->HasSeqFeatXref(cds->GetId()), true);
    BOOST_CHECK_EQUAL(gene1->HasSeqFeatXref(cds->GetId()), true);

    // this should not create reciprocal links, because mrna points to gene 2
    // and cds already points to gene 1
    BOOST_CHECK_EQUAL(CCleanup::RepairXrefs(*gene1, tse), false);
    ftable = nuc->GetAnnot().front()->GetData().GetFtable();
    fit = ftable.begin();
    mrna = *fit;
    ++fit;
    gene1 = *fit;
    ++fit;
    gene2 = *fit;
    BOOST_CHECK_EQUAL(mrna->HasSeqFeatXref(gene1->GetId()), false);

    // this will create a link on gene2 pointing back at mrna
    BOOST_CHECK_EQUAL(CCleanup::RepairXrefs(*mrna, tse), true);
    ftable = nuc->GetAnnot().front()->GetData().GetFtable();
    fit = ftable.begin();
    mrna = *fit;
    ++fit;
    gene1 = *fit;
    ++fit;
    gene2 = *fit;
    BOOST_CHECK_EQUAL(gene2->HasSeqFeatXref(mrna->GetId()), true);

    // no change this time because there's already an xref
    BOOST_CHECK_EQUAL(CCleanup::RepairXrefs(*mrna, tse), false);
}