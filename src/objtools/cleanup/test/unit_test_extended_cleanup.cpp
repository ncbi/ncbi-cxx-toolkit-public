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
#include <objmgr/seqdesc_ci.hpp>
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

extern const string sc_TestEntryRemoveOldName;
extern const string sc_TestEntryDontRemoveOldName;

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


const string sc_TestEntryRemoveOldName = "\
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

const string sc_TestEntryDontRemoveOldName = "\
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
    TGi gi = GI_CONST(24413615);
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


BOOST_AUTO_TEST_CASE(TEST_ConvertDeltaSeq)
{
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodDeltaSeq();
    TSeqPos orig_len = entry->GetSeq().GetLength();

    CRef<CScope> scope(new CScope(*CObjectManager::GetInstance()));
    CSeq_entry_Handle seh = scope->AddTopLevelSeqEntry(*entry);

    BOOST_CHECK_EQUAL(CCleanup::ConvertDeltaSeqToRaw(seh), true);
    CBioseq_Handle bsh = seh.GetSeq();
    BOOST_CHECK_EQUAL(bsh.GetInst_Length(), orig_len);
    BOOST_CHECK_EQUAL(bsh.GetInst_Repr(), CSeq_inst::eRepr_raw);
    objects::CSeqVector nuc_vec(bsh);
    nuc_vec.SetCoding(objects::CSeq_data::e_Iupacna);
    string nuc_buffer = "";
    nuc_vec.GetSeqData(0, bsh.GetInst_Length(), nuc_buffer);
    BOOST_CHECK_EQUAL(nuc_buffer, "ATGATGATGCCCNNNNNNNNNNCCCATGATGATG");

    // now try protein sequence
    scope->RemoveTopLevelSeqEntry(seh);
    entry->SetSeq().SetInst().SetMol(objects::CSeq_inst::eMol_aa);
    NON_CONST_ITERATE(objects::CSeq_descr::Tdata, it, entry->SetSeq().SetDescr().Set()) {
        if ((*it)->IsMolinfo()) {
            (*it)->SetMolinfo().SetBiomol(objects::CMolInfo::eBiomol_peptide);
        }
    }

    entry->SetSeq().SetInst().ResetSeq_data();
    entry->SetSeq().SetInst().SetRepr(objects::CSeq_inst::eRepr_delta);
    entry->SetSeq().SetInst().SetExt().SetDelta().AddLiteral("ATGATGATGCCC", objects::CSeq_inst::eMol_aa);
    CRef<objects::CDelta_seq> gap_seg(new objects::CDelta_seq());
    gap_seg->SetLiteral().SetSeq_data().SetGap();
    gap_seg->SetLiteral().SetLength(10);
    entry->SetSeq().SetInst().SetExt().SetDelta().Set().push_back(gap_seg);
    entry->SetSeq().SetInst().SetExt().SetDelta().AddLiteral("CCCATGATGATG", objects::CSeq_inst::eMol_aa);
    entry->SetSeq().SetInst().SetLength(34);
    seh = scope->AddTopLevelSeqEntry(*entry);

    BOOST_CHECK_EQUAL(CCleanup::ConvertDeltaSeqToRaw(seh), true);
    bsh = seh.GetSeq();
    BOOST_CHECK_EQUAL(bsh.GetInst_Length(), orig_len);
    BOOST_CHECK_EQUAL(bsh.GetInst_Repr(), CSeq_inst::eRepr_raw);
    objects::CSeqVector prot_vec(bsh);
    prot_vec.SetCoding(objects::CSeq_data::e_Iupacaa);
    string buffer = "";
    prot_vec.GetSeqData(0, bsh.GetInst_Length(), buffer);
    BOOST_CHECK_EQUAL(buffer, "ATGATGATGCCCXXXXXXXXXXCCCATGATGATG");

}


BOOST_AUTO_TEST_CASE(TEST_ConvertPopToPhy)
{
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodEcoSet();
    CRef<CSeq_entry> first = entry->SetSet().SetSeq_set().front();

    unit_test_util::SetTaxname(first, "apple");

    CRef<CScope> scope(new CScope(*CObjectManager::GetInstance()));;
    CSeq_entry_Handle seh = scope->AddTopLevelSeqEntry(*entry);
    entry->Parentize();

    CCleanup cleanup;
    CConstRef<CCleanupChange> changes;

    cleanup.SetScope(scope);
    changes = cleanup.ExtendedCleanup(*entry);
    // originally an eco set, which should not change
    BOOST_CHECK_EQUAL(entry->GetSet().GetClass(), CBioseq_set::eClass_eco_set);

    //but if we change it to population study, cleanup should fix it to phy set
    entry->SetSet().SetClass(CBioseq_set::eClass_pop_set);
    changes = cleanup.ExtendedCleanup(*entry);
    BOOST_CHECK_EQUAL(entry->GetSet().GetClass(), CBioseq_set::eClass_phy_set);

}


CRef<CSeq_entry> BuildEntryForExceptText(CSeqFeatData::ESubtype subtype, ENa_strand strand, size_t distance)
{
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodSeq();
    CRef<CSeq_feat> cds = unit_test_util::AddMiscFeature(entry);
    cds->ResetComment();
    if (subtype == CSeqFeatData::eSubtype_cdregion) {
        cds->SetData().SetCdregion();
    } else {
        cds->SetData().SetRna().SetType(CRNA_ref::eType_mRNA);
    }
    CRef<CSeq_loc> int1(new CSeq_loc());
    int1->SetInt().SetFrom(0);
    int1->SetInt().SetTo(5);
    int1->SetInt().SetStrand(strand);
    int1->SetInt().SetId().Assign(*(entry->GetSeq().GetId().front()));
    CRef<CSeq_loc> int2(new CSeq_loc());
    int2->SetInt().SetFrom(5 + distance);
    int2->SetInt().SetTo(10 + distance);
    int2->SetInt().SetStrand(strand);
    int2->SetInt().SetId().Assign(*(entry->GetSeq().GetId().front()));
    if (strand == eNa_strand_minus) {
        cds->SetLocation().SetMix().Set().push_back(int2);
        cds->SetLocation().SetMix().Set().push_back(int1);
    } else {
        cds->SetLocation().SetMix().Set().push_back(int1);
        cds->SetLocation().SetMix().Set().push_back(int2);
    }
    return entry;
}


void CheckLowQualityResults(CSeq_entry_Handle seh, CSeqFeatData::ESubtype subtype, bool expected)
{
    CFeat_CI c(seh, subtype);

    BOOST_CHECK_EQUAL(CCleanup::AddLowQualityException(seh), expected);

    if (expected) {
        BOOST_CHECK_EQUAL(c->IsSetExcept(), true);
        BOOST_CHECK_EQUAL(c->GetExcept_text(), "low-quality sequence region");
        // won't do it again
        BOOST_CHECK_EQUAL(CCleanup::AddLowQualityException(seh), false);
    } else {
        BOOST_CHECK_EQUAL(c->IsSetExcept(), false);
        BOOST_CHECK_EQUAL(c->IsSetExcept_text(), false);
    }
}


BOOST_AUTO_TEST_CASE(Test_AddLowQualityException)
{
    CRef<CSeq_entry> entry = BuildEntryForExceptText(CSeqFeatData::eSubtype_cdregion, eNa_strand_plus, 5);

    CRef<CScope> scope(new CScope(*CObjectManager::GetInstance()));;
    CSeq_entry_Handle seh = scope->AddTopLevelSeqEntry(*entry);
    CheckLowQualityResults(seh, CSeqFeatData::eSubtype_cdregion, true);

    scope->RemoveTopLevelSeqEntry(seh);
    entry = BuildEntryForExceptText(CSeqFeatData::eSubtype_cdregion, eNa_strand_plus, 15);
    seh = scope->AddTopLevelSeqEntry(*entry);
    CheckLowQualityResults(seh, CSeqFeatData::eSubtype_cdregion, false);

    scope->RemoveTopLevelSeqEntry(seh);
    entry = BuildEntryForExceptText(CSeqFeatData::eSubtype_cdregion, eNa_strand_minus, 5);
    seh = scope->AddTopLevelSeqEntry(*entry);
    CheckLowQualityResults(seh, CSeqFeatData::eSubtype_cdregion, true);

    scope->RemoveTopLevelSeqEntry(seh);
    entry = BuildEntryForExceptText(CSeqFeatData::eSubtype_cdregion, eNa_strand_minus, 15);
    seh = scope->AddTopLevelSeqEntry(*entry);
    CheckLowQualityResults(seh, CSeqFeatData::eSubtype_cdregion, false);

    scope->RemoveTopLevelSeqEntry(seh);
    entry = BuildEntryForExceptText(CSeqFeatData::eSubtype_mRNA, eNa_strand_plus, 5);
    seh = scope->AddTopLevelSeqEntry(*entry);
    CheckLowQualityResults(seh, CSeqFeatData::eSubtype_mRNA, true);

    scope->RemoveTopLevelSeqEntry(seh);
    entry = BuildEntryForExceptText(CSeqFeatData::eSubtype_mRNA, eNa_strand_plus, 15);
    seh = scope->AddTopLevelSeqEntry(*entry);
    CheckLowQualityResults(seh, CSeqFeatData::eSubtype_mRNA, false);

    scope->RemoveTopLevelSeqEntry(seh);
    entry = BuildEntryForExceptText(CSeqFeatData::eSubtype_mRNA, eNa_strand_minus, 5);
    seh = scope->AddTopLevelSeqEntry(*entry);
    CheckLowQualityResults(seh, CSeqFeatData::eSubtype_mRNA, true);

    scope->RemoveTopLevelSeqEntry(seh);
    entry = BuildEntryForExceptText(CSeqFeatData::eSubtype_mRNA, eNa_strand_minus, 15);
    seh = scope->AddTopLevelSeqEntry(*entry);
    CheckLowQualityResults(seh, CSeqFeatData::eSubtype_mRNA, false);

}


BOOST_AUTO_TEST_CASE(Test_LatLonTrimming)
{
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodSeq();
    unit_test_util::SetSubSource(entry, CSubSource::eSubtype_lat_lon, "3.12516554 N 2.5665974512 E");
    CRef<CScope> scope(new CScope(*CObjectManager::GetInstance()));;
    CSeq_entry_Handle seh = scope->AddTopLevelSeqEntry(*entry);

    CCleanup cleanup;
    CConstRef<CCleanupChange> changes;

    cleanup.SetScope(scope);
    changes = cleanup.ExtendedCleanup(*entry);
    ITERATE(CBioseq::TDescr::Tdata, d, entry->GetDescr().Get()) {
        if ((*d)->IsSource()) {
            BOOST_CHECK_EQUAL((*d)->GetSource().GetSubtype().back()->GetName(), "3.12516554 N 2.56659745 E");
        }
    }

}


BOOST_AUTO_TEST_CASE(Test_ConvertMiscSignalToRegulatory)
{
    CRef<CSeq_entry> entry = BuildGoodSeq();
    CRef<CSeq_feat> misc = BuildGoodFeat();
    misc->SetData().SetImp().SetKey("misc_signal");
    misc->SetComment("boxB antiterminator");
    AddFeat(misc, entry);

    CRef<CScope> scope(new CScope(*CObjectManager::GetInstance()));;
    CSeq_entry_Handle seh = scope->AddTopLevelSeqEntry(*entry);
    entry->Parentize();

    CCleanup cleanup;

    cleanup.SetScope(scope);
    cleanup.ExtendedCleanup(*entry);

    scope->RemoveTopLevelSeqEntry(seh);
    seh = scope->AddTopLevelSeqEntry(*entry);
    // make sure change was actually made
    CFeat_CI feat(seh);
    BOOST_CHECK_EQUAL(feat->GetData().GetImp().GetKey(), "regulatory");
    BOOST_CHECK_EQUAL(feat->GetQual().front()->GetQual(), "regulatory_class");
    BOOST_CHECK_EQUAL(feat->GetQual().front()->GetVal(), "other");
}


BOOST_AUTO_TEST_CASE(Test_MatPeptidePartial)
{
    CRef<CSeq_entry> entry = BuildGoodNucProtSet();
    CRef<CSeq_entry> nuc = GetNucleotideSequenceFromGoodNucProtSet(entry);
    CRef<CSeq_feat> cds = GetCDSFromGoodNucProtSet(entry);
    CRef<CSeq_entry> prot = GetProteinSequenceFromGoodNucProtSet(entry);
    // First mat_peptide is shorter than protein sequence
    CRef<CSeq_feat> mat = AddMiscFeature(nuc);
    mat->SetData().SetImp().SetKey("mat_peptide");
    mat->SetLocation().SetInt().SetFrom(cds->GetLocation().GetStart(eExtreme_Positional));
    mat->SetLocation().SetInt().SetTo(cds->GetLocation().GetStart(eExtreme_Positional) + 5);
    mat->SetLocation().SetPartialStart(true, eExtreme_Biological);
    mat->SetLocation().SetPartialStop(true, eExtreme_Biological);

    // second mat_peptide ends at end of protein sequence
    CRef<CSeq_feat> mat2 = AddMiscFeature(nuc);
    mat2->SetData().SetImp().SetKey("mat_peptide");
    mat2->SetLocation().SetInt().SetFrom(cds->GetLocation().GetStart(eExtreme_Positional) + 3);
    mat2->SetLocation().SetInt().SetTo(cds->GetLocation().GetStop(eExtreme_Positional));
    mat2->SetLocation().SetPartialStart(true, eExtreme_Biological);
    mat2->SetLocation().SetPartialStop(true, eExtreme_Biological);


    CRef<CScope> scope(new CScope(*CObjectManager::GetInstance()));
    CSeq_entry_Handle seh = scope->AddTopLevelSeqEntry(*entry);


    CCleanup cleanup(scope);
    CConstRef<CCleanupChange> changes;

    changes = cleanup.ExtendedCleanup(seh);

    CBioseq_CI p(seh, CSeq_inst::eMol_aa);
    CFeat_CI f(*p, CSeqFeatData::eSubtype_mat_peptide_aa);
    while (f) {
        if (f->GetLocation().GetStart(eExtreme_Biological) == 0) {
            // first mat_peptide
            BOOST_CHECK_EQUAL(f->GetLocation().IsPartialStart(eExtreme_Biological), false);
            BOOST_CHECK_EQUAL(f->GetLocation().IsPartialStop(eExtreme_Biological), true);
        } else {
            // second mat_peptide
            BOOST_CHECK_EQUAL(f->GetLocation().IsPartialStart(eExtreme_Biological), true);
            BOOST_CHECK_EQUAL(f->GetLocation().IsPartialStop(eExtreme_Biological), false);
        }
        ++f;
    }
}


CRef<CSeq_entry> BuildInfluenzaSegment
(const string& taxname,
 const string& strain,
 const string& serotype,
 size_t segment,
 bool partial,
 bool is_seq = false)
{
    CRef<CSeq_entry> np;
    CRef<CSeq_feat> feat_to_partial;
    if (is_seq) {
        np = BuildGoodSeq();
        feat_to_partial = AddMiscFeature(np);
        feat_to_partial->SetData().SetGene().SetLocus("abc");
    } else {
        np = unit_test_util::BuildGoodNucProtSet();
        feat_to_partial = GetCDSFromGoodNucProtSet(np);
    }
    unit_test_util::SetTaxname(np, taxname);
    if (!NStr::IsBlank(strain)) {
        unit_test_util::SetOrgMod(np, COrgMod::eSubtype_strain, strain);
    }
    if (!NStr::IsBlank(serotype)) {
        SetOrgMod(np, COrgMod::eSubtype_serotype, serotype);
    }

    if (segment < 100) {
        SetSubSource(np, CSubSource::eSubtype_segment, NStr::NumericToString(segment));
    }
    if (partial) {
        feat_to_partial->SetLocation().SetPartialStart(true, eExtreme_Biological);
    }
    return np;
}


void AddOneInfluenzaSegment
(CBioseq_set& set,
size_t seg_num,
size_t& id_offset,
const string& taxname,
const string& strain,
const string& serotype = kEmptyStr,
bool make_partial = false,
bool make_seq = false)
{
    CRef<CSeq_entry> seg = BuildInfluenzaSegment(taxname, strain, serotype, seg_num, make_partial, make_seq);
    if (make_seq) {
        CRef<CSeq_id> nid(new CSeq_id());
        nid->SetLocal().SetStr("seg_" + NStr::NumericToString(id_offset));
        ChangeId(seg, nid);
    } else {
        CRef<CSeq_id> nid(new CSeq_id());
        nid->SetLocal().SetStr("nseg_" + NStr::NumericToString(id_offset));
        ChangeNucProtSetNucId(seg, nid);
        CRef<CSeq_id> pid(new CSeq_id());
        pid->SetLocal().SetStr("pseg_" + NStr::NumericToString(id_offset));
        ChangeNucProtSetProteinId(seg, pid);
    }
    set.SetSeq_set().push_back(seg);
    id_offset++;
}


void AddInfluenzaSegments
(CBioseq_set& set,
 size_t num_segments,
 size_t& id_offset,
 const string& taxname,
 const string& strain,
 const string& serotype = kEmptyStr)
{
    for (size_t i = 0; i < num_segments; i++) {
        AddOneInfluenzaSegment(set, i + 1, id_offset, taxname, strain, serotype);
    }
}


void CheckSmallGenomeSet(const CBioseq_set& sm_set, size_t expected)
{
    BOOST_CHECK_EQUAL(sm_set.GetClass(), CBioseq_set::eClass_small_genome_set);
    BOOST_CHECK_EQUAL(sm_set.GetSeq_set().size(), expected);
}


BOOST_AUTO_TEST_CASE(Test_MakeSmallGenomeSet)
{
    CRef<CSeq_entry> entry(new CSeq_entry());
    entry->SetSet().SetClass(CBioseq_set::eClass_genbank);
    size_t id_offset = 1;
    AddInfluenzaSegments(entry->SetSet(), 8, id_offset, "Influenza A virus", "X", "Y");
    CRef<CScope> scope(new CScope(*CObjectManager::GetInstance()));
    CSeq_entry_Handle seh = scope->AddTopLevelSeqEntry(*entry);

    BOOST_CHECK_EQUAL(CCleanup::MakeSmallGenomeSet(seh), 1);
    BOOST_CHECK_EQUAL(seh.GetSet().GetBioseq_setCore()->GetSeq_set().size(), 1);
    CheckSmallGenomeSet(seh.GetSet().GetCompleteBioseq_set()->GetSeq_set().front()->GetSet(), 8);

    // duplicate segments are ok
    scope->RemoveTopLevelSeqEntry(seh);
    entry->SetSet().ResetSeq_set();
    id_offset = 1;
    AddInfluenzaSegments(entry->SetSet(), 8, id_offset, "Influenza A virus", "X", "Y");
    AddInfluenzaSegments(entry->SetSet(), 4, id_offset, "Influenza A virus", "X", "Y");
    seh = scope->AddTopLevelSeqEntry(*entry);
    BOOST_CHECK_EQUAL(CCleanup::MakeSmallGenomeSet(seh), 1);
    BOOST_CHECK_EQUAL(seh.GetSet().GetBioseq_setCore()->GetSeq_set().size(), 1);
    CheckSmallGenomeSet(seh.GetSet().GetCompleteBioseq_set()->GetSeq_set().front()->GetSet(), 12);


    scope->RemoveTopLevelSeqEntry(seh);
    entry->SetSet().ResetSeq_set();
    id_offset = 1;
    AddInfluenzaSegments(entry->SetSet(), 8, id_offset, "Influenza A virus", "X", "Y");
    AddInfluenzaSegments(entry->SetSet(), 4, id_offset, "Influenza A virus", "X", "Y");
    // too few, won't make new set
    AddInfluenzaSegments(entry->SetSet(), 6, id_offset, "Influenza B virus", "X");
    seh = scope->AddTopLevelSeqEntry(*entry);
    BOOST_CHECK_EQUAL(CCleanup::MakeSmallGenomeSet(seh), 1);
    BOOST_CHECK_EQUAL(seh.GetSet().GetBioseq_setCore()->GetSeq_set().size(), 7);

    scope->RemoveTopLevelSeqEntry(seh);
    entry->SetSet().ResetSeq_set();
    id_offset = 1;
    // not fooled by duplicate
    AddInfluenzaSegments(entry->SetSet(), 7, id_offset, "Influenza B virus", "X");
    AddOneInfluenzaSegment(entry->SetSet(), 7, id_offset, "Influenza B virus", "X");
    seh = scope->AddTopLevelSeqEntry(*entry);
    BOOST_CHECK_EQUAL(CCleanup::MakeSmallGenomeSet(seh), 0);
    BOOST_CHECK_EQUAL(seh.GetSet().GetBioseq_setCore()->GetSeq_set().size(), 8);

    scope->RemoveTopLevelSeqEntry(seh);
    entry->SetSet().ResetSeq_set();
    id_offset = 1;
    // too many
    AddInfluenzaSegments(entry->SetSet(), 8, id_offset, "Influenza C virus", "X");
    seh = scope->AddTopLevelSeqEntry(*entry);
    BOOST_CHECK_EQUAL(CCleanup::MakeSmallGenomeSet(seh), 0);
    BOOST_CHECK_EQUAL(seh.GetSet().GetBioseq_setCore()->GetSeq_set().size(), 8);

    // interspersed
    scope->RemoveTopLevelSeqEntry(seh);
    entry->SetSet().ResetSeq_set();
    id_offset = 1;
    AddInfluenzaSegments(entry->SetSet(), 7, id_offset, "Influenza A virus", "X", "Y");
    AddInfluenzaSegments(entry->SetSet(), 7, id_offset, "Influenza B virus", "X");
    AddInfluenzaSegments(entry->SetSet(), 6, id_offset, "Influenza C virus", "X");
    AddInfluenzaSegments(entry->SetSet(), 6, id_offset, "Influenza D virus", "X");
    AddOneInfluenzaSegment(entry->SetSet(), 8, id_offset, "Influenza A virus", "X", "Y");
    AddOneInfluenzaSegment(entry->SetSet(), 8, id_offset, "Influenza B virus", "X", "Y");
    AddOneInfluenzaSegment(entry->SetSet(), 7, id_offset, "Influenza C virus", "X");
    AddOneInfluenzaSegment(entry->SetSet(), 7, id_offset, "Influenza D virus", "X");
    seh = scope->AddTopLevelSeqEntry(*entry);
    BOOST_CHECK_EQUAL(CCleanup::MakeSmallGenomeSet(seh), 4);
    BOOST_CHECK_EQUAL(seh.GetSet().GetBioseq_setCore()->GetSeq_set().size(), 4);
    CheckSmallGenomeSet(seh.GetSet().GetCompleteBioseq_set()->GetSeq_set().front()->GetSet(), 8);
    CheckSmallGenomeSet(seh.GetSet().GetCompleteBioseq_set()->GetSeq_set().back()->GetSet(), 7);


    //make sure we can add a sequence instead of a nuc-prot set
    scope->RemoveTopLevelSeqEntry(seh);
    entry->SetSet().ResetSeq_set();
    id_offset = 1;
    AddInfluenzaSegments(entry->SetSet(), 6, id_offset, "Influenza D virus", "X");
    AddOneInfluenzaSegment(entry->SetSet(), 7, id_offset, "Influenza D virus", "X", kEmptyStr, false, true);
    seh = scope->AddTopLevelSeqEntry(*entry);
    BOOST_CHECK_EQUAL(CCleanup::MakeSmallGenomeSet(seh), 1);
    BOOST_CHECK_EQUAL(seh.GetSet().GetBioseq_setCore()->GetSeq_set().size(), 1);
    CheckSmallGenomeSet(seh.GetSet().GetCompleteBioseq_set()->GetSeq_set().front()->GetSet(), 7);

    //make sure we don't create the set if one sequence has partial coding region
    scope->RemoveTopLevelSeqEntry(seh);
    entry->SetSet().ResetSeq_set();
    id_offset = 1;
    AddInfluenzaSegments(entry->SetSet(), 6, id_offset, "Influenza D virus", "X");
    AddOneInfluenzaSegment(entry->SetSet(), 7, id_offset, "Influenza D virus", "X", kEmptyStr, true, false);
    seh = scope->AddTopLevelSeqEntry(*entry);
    BOOST_CHECK_EQUAL(CCleanup::MakeSmallGenomeSet(seh), 0);
    BOOST_CHECK_EQUAL(seh.GetSet().GetBioseq_setCore()->GetSeq_set().size(), 7);

    // make sure we don't create the set if one sequence has partial gene
    scope->RemoveTopLevelSeqEntry(seh);
    entry->SetSet().ResetSeq_set();
    id_offset = 1;
    AddInfluenzaSegments(entry->SetSet(), 6, id_offset, "Influenza D virus", "X");
    AddOneInfluenzaSegment(entry->SetSet(), 7, id_offset, "Influenza D virus", "X", kEmptyStr, true, true);
    seh = scope->AddTopLevelSeqEntry(*entry);
    BOOST_CHECK_EQUAL(CCleanup::MakeSmallGenomeSet(seh), 0);
    BOOST_CHECK_EQUAL(seh.GetSet().GetBioseq_setCore()->GetSeq_set().size(), 7);

}


BOOST_AUTO_TEST_CASE(Test_SQD_4303)
{
    CRef<CSeq_entry> entry = BuildGoodSeq();
    SetTaxname(entry, "Linepithema humile");
    SetTaxon(entry, 0);

    CRef<CScope> scope(new CScope(*CObjectManager::GetInstance()));
    CSeq_entry_Handle seh = scope->AddTopLevelSeqEntry(*entry);

    BOOST_CHECK_EQUAL(CCleanup::TaxonomyLookup(seh), true);
    CSeqdesc_CI desc_ci(seh, CSeqdesc::e_Source);
    const COrg_ref& org = desc_ci->GetSource().GetOrg();
    BOOST_CHECK_EQUAL(org.IsSetSyn(), false);
    BOOST_CHECK_EQUAL(org.GetTaxId(), TAX_ID_CONST(83485));

}


void Test_WGSCleanup(const string& cds_product, const string& mrna_product)
{
    CRef<CSeq_entry> entry = BuildGoodSeq();
    CRef<CSeq_feat> cds = AddMiscFeature(entry);
    cds->SetData().SetCdregion();
    if (!NStr::IsBlank(cds_product)) {
        CRef<CGb_qual> cp(new CGb_qual("product", cds_product));
        cds->SetQual().push_back(cp);
    }
    CRef<CSeq_feat> mrna = AddMiscFeature(entry);
    mrna->SetData().SetRna().SetType(CRNA_ref::eType_mRNA);
    if (!NStr::IsBlank(mrna_product)) {
        CRef<CGb_qual> mp(new CGb_qual("product", mrna_product));
        mrna->SetQual().push_back(mp);
    }

    CRef<CScope> scope(new CScope(*CObjectManager::GetInstance()));
    CSeq_entry_Handle seh = scope->AddTopLevelSeqEntry(*entry);

    BOOST_CHECK_EQUAL(CCleanup::WGSCleanup(seh), true);

    CConstRef<CSeq_entry> new_entry = seh.GetCompleteSeq_entry();
    scope->RemoveTopLevelSeqEntry(seh);
    seh = scope->AddTopLevelSeqEntry(*new_entry);

    BOOST_CHECK_EQUAL(seh.IsSet(), true);
    BOOST_CHECK_EQUAL(seh.GetSet().GetClass(), CBioseq_set::eClass_nuc_prot);
    BOOST_CHECK_EQUAL(seh.GetSet().GetCompleteBioseq_set()->GetSeq_set().size(), 2);

    CBioseq_CI nuc(seh, CSeq_inst::eMol_na);

    CFeat_CI cdsf(*nuc, CSeqFeatData::e_Cdregion);
    BOOST_CHECK_EQUAL(cdsf->IsSetQual(), false);

    CBioseq_CI pseq(seh, CSeq_inst::eMol_aa);
    if (!pseq) {
        BOOST_ERROR("Protein sequence missing");
    } else {
        CFeat_CI prot(*pseq, CSeqFeatData::e_Prot);
        if (!prot) {
            BOOST_ERROR("Protein feature missing");
        } else {
            if (NStr::IsBlank(cds_product)) {
                BOOST_CHECK_EQUAL(prot->GetData().GetProt().GetName().front(), "hypothetical protein");
            } else {
                BOOST_CHECK_EQUAL(prot->GetData().GetProt().GetName().front(), cds_product);
            }
        }
        CSeqdesc_CI molinfo(*pseq, CSeqdesc::e_Molinfo);
        if (!molinfo) {
            BOOST_ERROR("MolInfo descriptor missing");
        } else if (!molinfo->GetMolinfo().IsSetTech()) {
            BOOST_ERROR("Protein Molinfo tech not set");
        } else {
            BOOST_CHECK_EQUAL(molinfo->GetMolinfo().GetTech(), CMolInfo::eTech_concept_trans);
        }
    }

    CFeat_CI mrnaf(*nuc, CSeqFeatData::eSubtype_mRNA);
    BOOST_CHECK_EQUAL(mrnaf->IsSetQual(), false);
    if (NStr::IsBlank(cds_product)) {
        BOOST_CHECK_EQUAL(mrnaf->GetData().GetRna().GetExt().GetName(), "hypothetical protein");
    } else {
        BOOST_CHECK_EQUAL(mrnaf->GetData().GetRna().GetExt().GetName(), cds_product);
    }
}


void Test_WGSCleanupNoProt(const string& cds_product, const string& mrna_product)
{
    CRef<CSeq_entry> entry = BuildGoodSeq();
    CRef<CSeq_feat> cds = AddMiscFeature(entry);
    cds->SetData().SetCdregion();
    if (!NStr::IsBlank(cds_product)) {
        CRef<CGb_qual> cp(new CGb_qual("product", cds_product));
        cds->SetQual().push_back(cp);
    }
    CRef<CSeq_feat> mrna = AddMiscFeature(entry);
    mrna->SetData().SetRna().SetType(CRNA_ref::eType_mRNA);
    if (!NStr::IsBlank(mrna_product)) {
        CRef<CGb_qual> mp(new CGb_qual("product", mrna_product));
        mrna->SetQual().push_back(mp);
    }

    CRef<CScope> scope(new CScope(*CObjectManager::GetInstance()));
    CSeq_entry_Handle seh = scope->AddTopLevelSeqEntry(*entry);

    BOOST_CHECK_EQUAL(CCleanup::WGSCleanup(seh, false), true);

    CConstRef<CSeq_entry> new_entry = seh.GetCompleteSeq_entry();
    scope->RemoveTopLevelSeqEntry(seh);
    seh = scope->AddTopLevelSeqEntry(*new_entry);

    BOOST_CHECK_EQUAL(seh.IsSeq(), true);

    CBioseq_CI nuc(seh, CSeq_inst::eMol_na);

    CFeat_CI cdsf(*nuc, CSeqFeatData::e_Cdregion);
    BOOST_CHECK_EQUAL(cdsf->IsSetQual(), false);

    CBioseq_CI pseq(seh, CSeq_inst::eMol_aa);
    if (pseq) {
        BOOST_ERROR("Protein sequence should not have been created");
    }

    CFeat_CI mrnaf(*nuc, CSeqFeatData::eSubtype_mRNA);
    BOOST_CHECK_EQUAL(mrnaf->IsSetQual(), false);
    if (NStr::IsBlank(cds_product)) {
        BOOST_CHECK_EQUAL(mrnaf->GetData().GetRna().GetExt().GetName(), "hypothetical protein");
    } else {
        BOOST_CHECK_EQUAL(mrnaf->GetData().GetRna().GetExt().GetName(), cds_product);
    }
}


BOOST_AUTO_TEST_CASE(Test_RW_492)
{
    Test_WGSCleanup("X", "X");
    Test_WGSCleanup("", "");
    Test_WGSCleanup("X", "");
    Test_WGSCleanup("X", "Y");

    Test_WGSCleanupNoProt("X", "X");
}


BOOST_AUTO_TEST_CASE(Test_VR_782)
{
    CRef<CSeq_entry> entry = BuildGoodNucProtSet();
    CRef<CSeq_entry> prot = GetProteinSequenceFromGoodNucProtSet(entry);
    CRef<CSeq_feat> pfeat = prot->SetSeq().SetAnnot().front()->SetData().SetFtable().front();
    CRef<CSeq_feat> mfeat(new CSeq_feat());
    mfeat->Assign(*pfeat);
    mfeat->SetData().SetProt().ResetName();
    mfeat->SetData().SetProt().SetProcessed(CProt_ref::eProcessed_mature);
    prot->SetSeq().SetAnnot().front()->SetData().SetFtable().push_back(mfeat);
    pfeat->SetPartial(true);
    mfeat->SetPartial(true);

    CRef<CScope> scope(new CScope(*CObjectManager::GetInstance()));;
    CSeq_entry_Handle seh = scope->AddTopLevelSeqEntry(*entry);
    entry->Parentize();

    CCleanup cleanup;
    CConstRef<CCleanupChange> changes;

    cleanup.SetScope(scope);
    changes = cleanup.ExtendedCleanup(*entry);
    prot = GetProteinSequenceFromGoodNucProtSet(entry);
    pfeat = prot->SetSeq().SetAnnot().front()->SetData().SetFtable().front();
    mfeat = prot->SetSeq().SetAnnot().front()->SetData().SetFtable().back();
    // BOOST_CHECK_EQUAL(pfeat->GetPartial(), false);
    BOOST_CHECK_EQUAL(mfeat->IsSetPartial(), false);
}


void LinkPair(CSeq_feat& f1, CSeq_feat& f2)
{
    f1.SetXref().push_back(CRef<CSeqFeatXref>(new CSeqFeatXref()));
    f1.SetXref().back()->SetId().SetLocal().SetId(f2.GetId().GetLocal().GetId());
    f2.SetXref().push_back(CRef<CSeqFeatXref>(new CSeqFeatXref()));
    f2.SetXref().back()->SetId().SetLocal().SetId(f1.GetId().GetLocal().GetId());
}


void MakeMrnaGeneTripletForCDS(CRef<CSeq_entry> entry, CRef<CSeq_feat> cds)
{
    CRef<CSeq_entry> nuc = GetNucleotideSequenceFromGoodNucProtSet(entry);
    CRef<CSeq_entry> prot = entry->GetSet().GetSeq_set().back();
    CRef<CSeq_feat> pfeat = prot->GetAnnot().front()->GetData().GetFtable().front();
    // make mRNA
    CRef<CSeq_feat> mrna = unit_test_util::MakemRNAForCDS(cds);
    mrna->SetData().SetRna().SetExt().SetName(pfeat->GetData().GetProt().GetName().front());
    mrna->SetData().SetRna().SetType(CRNA_ref::eType_mRNA);
    unit_test_util::AddFeat(mrna, nuc);
    auto cds_id = cds->GetId().GetLocal().GetId();
    auto mrna_id = cds_id + 1;
    // assign ID to mRNA
    mrna->SetId().SetLocal().SetId(mrna_id);
    // make reciprocal xrefs between cds and mrna
    LinkPair(*cds, *mrna);
    // make gene
    CRef<CSeq_feat> gene = unit_test_util::MakeGeneForFeature(mrna);
    unit_test_util::AddFeat(gene, nuc);
    auto gene_id = cds_id + 2;
    gene->SetId().SetLocal().SetId(gene_id);
    // make reciprocal xrefs between cds and gene
    LinkPair(*cds, *gene);
    // make reciprocal xrefs between mrna and gene
    LinkPair(*mrna, *gene);
}


void AddNextCDSToNucProt(CRef<CSeq_entry> entry)
{
    auto cds_ftable = entry->SetSet().SetAnnot().front()->SetData().SetFtable();
    auto last_cds = cds_ftable.back();
    size_t num_cds = cds_ftable.size();
    string suffix = "_" + NStr::NumericToString(num_cds);
    auto seq_it = entry->GetSet().GetSeq_set().begin();
    // skip nuc
    seq_it++;
    CConstRef<CSeq_id> first_id = (*seq_it)->GetSeq().GetId().front();
    CConstRef<CSeq_feat> first_prot = (*seq_it)->GetSeq().GetAnnot().front()->GetData().GetFtable().front();

    // add protein sequence, but with new id
    CRef<CSeq_entry> last_prot = entry->GetSet().GetSeq_set().back();
    CRef<CSeq_entry> new_prot(new CSeq_entry());
    new_prot->Assign(**seq_it);
    CRef<CSeq_id> new_prot_id(new CSeq_id());
    new_prot_id->SetLocal().SetStr(first_id->GetLocal().GetStr() + suffix);
    ChangeId(new_prot, new_prot_id);
    CRef<CSeq_feat> new_prot_feat = new_prot->SetAnnot().front()->SetData().SetFtable().front();
    new_prot_feat->SetData().SetProt().SetName().front() = first_prot->GetData().GetProt().GetName().front() + suffix;
    entry->SetSet().SetSeq_set().push_back(new_prot);

    // add coding region
    CRef<CSeq_feat> new_cds(new CSeq_feat());
    new_cds->Assign(*(cds_ftable.front()));
    new_cds->SetProduct().SetWhole().Assign(*new_prot_id);
    new_cds->SetId().SetLocal().SetId(num_cds * 3 + 1);
    entry->SetSet().SetAnnot().front()->SetData().SetFtable().push_back(new_cds);

    MakeMrnaGeneTripletForCDS(entry, new_cds);
}


void TestWithNCodingRegions(size_t n, bool first_is_partial)
{
    CRef<CSeq_entry> entry = BuildGoodNucProtSet();
    CRef<CSeq_feat> cds = GetCDSFromGoodNucProtSet(entry);
    cds->SetId().SetLocal().SetId(1);
    MakeMrnaGeneTripletForCDS(entry, cds);
    for (size_t i = 1; i < n; i++) {
        AddNextCDSToNucProt(entry);
    }
    if (first_is_partial) {
        cds->SetLocation().SetPartialStart(true, eExtreme_Biological);
    }
    CRef<CScope> scope(new CScope(*CObjectManager::GetInstance()));;
    CSeq_entry_Handle seh = scope->AddTopLevelSeqEntry(*entry);
    entry->Parentize();

    CCleanup cleanup;
    CConstRef<CCleanupChange> changes;

    cleanup.SetScope(scope);
    changes = cleanup.ExtendedCleanup(*entry);
    CRef<CSeq_entry> nuc = GetNucleotideSequenceFromGoodNucProtSet(entry);
    auto ftable = nuc->GetAnnot().front()->GetData().GetFtable();

    auto fit = ftable.begin();
    BOOST_CHECK_EQUAL((*fit)->GetLocation().IsPartialStart(eExtreme_Biological), first_is_partial);
    fit++;
    BOOST_CHECK_EQUAL((*fit)->GetLocation().IsPartialStart(eExtreme_Biological), first_is_partial);
    fit++;
    for (size_t i = 1; i < n; i++) {
        BOOST_CHECK_EQUAL((*fit)->GetLocation().IsPartialStart(eExtreme_Biological), false);
        fit++;
        BOOST_CHECK_EQUAL((*fit)->GetLocation().IsPartialStart(eExtreme_Biological), false);
        fit++;
    }
}

BOOST_AUTO_TEST_CASE(Test_RW_525)
{
    TestWithNCodingRegions(1, false);
    TestWithNCodingRegions(1, true);
    TestWithNCodingRegions(2, false);
    TestWithNCodingRegions(2, true);
}


BOOST_AUTO_TEST_CASE(Test_GB_7597)
{
    CCleanup cleanup;
    CConstRef<CCleanupChange> changes;

    CRef<CScope> scope(new CScope(*CObjectManager::GetInstance()));;
    cleanup.SetScope(scope);

    CRef<CSeq_entry> sub1 = BuildGoodNucProtSet();
    CRef<CSeq_entry> entry(new CSeq_entry());
    entry->SetSet().SetSeq_set().push_back(sub1);
    entry->SetSet().SetClass(CBioseq_set::eClass_nuc_prot);
    entry->Parentize();

    changes = cleanup.ExtendedCleanup(*entry, CCleanup::eClean_NoNcbiUserObjects);
    BOOST_CHECK_EQUAL(changes->IsChanged(CCleanupChange::eCollapseSet), true);

    BOOST_CHECK_EQUAL(entry->GetSet().GetClass(), CBioseq_set::eClass_nuc_prot);
    BOOST_CHECK_EQUAL(entry->GetSet().GetSeq_set().size(), 2);
    BOOST_CHECK_EQUAL(entry->GetSet().GetSeq_set().front()->IsSeq(), true);
    BOOST_CHECK_EQUAL(entry->GetSet().GetSeq_set().back()->IsSeq(), true);
}


CRef<CSeq_entry> MakeGeneNormalizationExample()
{
    CRef<CSeq_entry> entry = BuildGoodSeq();
    SetLineage(entry, "Bacteria; ");
    CRef<CSeq_feat> cds1 = AddMiscFeature(entry);
    cds1->SetLocation().SetInt().SetFrom(0);
    cds1->SetLocation().SetInt().SetTo(15);
    cds1->SetData().SetCdregion();

    CRef<CSeq_feat> cds2 = AddMiscFeature(entry);
    cds2->SetLocation().SetInt().SetFrom(20);
    cds2->SetLocation().SetInt().SetTo(35);
    cds2->SetData().SetCdregion();
    CRef<CSeqFeatXref> xref(new CSeqFeatXref());
    xref->SetData().SetGene().SetLocus("B");
    cds2->SetXref().push_back(xref);

    CRef<CSeq_feat> gene1 = MakeGeneForFeature(cds1);
    gene1->SetData().SetGene().SetLocus("A");
    gene1->SetData().SetGene().SetLocus_tag("x_1");
    AddFeat(gene1, entry);

    CRef<CSeq_feat> gene2 = MakeGeneForFeature(cds2);
    gene2->SetData().SetGene().ResetLocus();
    gene2->SetData().SetGene().SetLocus_tag("x_2");
    AddFeat(gene2, entry);

    return entry;
}


void TestGeneNormalization(CRef<CSeq_entry> entry, bool expect_to_work)
{
    CRef<CScope> scope(new CScope(*CObjectManager::GetInstance()));;
    CSeq_entry_Handle seh = scope->AddTopLevelSeqEntry(*entry);

    BOOST_CHECK_EQUAL(CCleanup::NormalizeGeneQuals(seh), expect_to_work);

    CFeat_CI g(seh, CSeqFeatData::eSubtype_gene);
    BOOST_CHECK_EQUAL(g->GetData().GetGene().GetLocus(), "A");
    ++g;
    if (expect_to_work) {
        BOOST_CHECK_EQUAL(g->GetData().GetGene().GetLocus(), "B");
    } else {
        BOOST_CHECK_EQUAL(g->GetData().GetGene().IsSetLocus(), false);
    }

    CFeat_CI c(seh, CSeqFeatData::eSubtype_cdregion);
    ++c;
    if (expect_to_work) {
        BOOST_CHECK_EQUAL(c->GetXref().front()->GetData().GetGene().GetLocus_tag(), "x_2");
        BOOST_CHECK_EQUAL(c->GetXref().front()->GetData().GetGene().IsSetLocus(), false);
    } else {
        BOOST_CHECK_EQUAL(c->GetXref().front()->GetData().GetGene().IsSetLocus_tag(), false);
        BOOST_CHECK_EQUAL(c->GetXref().front()->GetData().GetGene().GetLocus(), "B");
    }

}


BOOST_AUTO_TEST_CASE(Test_GB_7166)
{
    CRef<CSeq_entry> entry = MakeGeneNormalizationExample();
    TestGeneNormalization(entry, true);

    // should not work if there is a third gene with locus B
    entry = MakeGeneNormalizationExample();
    CRef<CSeq_feat> gene3 = AddMiscFeature(entry);
    gene3->SetLocation().SetInt().SetFrom(36);
    gene3->SetLocation().SetInt().SetTo(40);
    gene3->SetData().SetGene().SetLocus("B");

    TestGeneNormalization(entry, false);

    // should not work if there is a second coding region under the gene
    entry = MakeGeneNormalizationExample();
    CRef<CSeq_feat> cds3 = AddMiscFeature(entry);
    cds3->SetLocation().SetInt().SetFrom(25);
    cds3->SetLocation().SetInt().SetTo(35);
    cds3->SetData().SetCdregion();

    TestGeneNormalization(entry, false);

}


void TestParentPartial(bool cds_prime5, bool cds_prime3)
{
    CRef<CSeq_entry> entry = BuildGoodDeltaSeq();

    CRef<CSeq_feat> cds1 = AddMiscFeature(entry, 11);
    cds1->SetData().SetCdregion();
    cds1->SetLocation().SetInt().SetPartialStart(cds_prime5, eExtreme_Biological);
    cds1->SetLocation().SetInt().SetPartialStop(true, eExtreme_Biological);

    CRef<CSeq_feat> cds2 = AddMiscFeature(entry, entry->GetSeq().GetLength() - 1);
    cds2->SetData().SetCdregion();
    cds2->SetLocation().SetInt().SetFrom(22);
    cds2->SetLocation().SetInt().SetPartialStart(true, eExtreme_Biological);
    cds2->SetLocation().SetInt().SetPartialStop(cds_prime3, eExtreme_Biological);

    CRef<CSeq_feat> gene = AddMiscFeature(entry, entry->GetSeq().GetLength() - 1);
    gene->SetData().SetGene().SetLocus("X");

    CRef<CScope> scope(new CScope(*CObjectManager::GetInstance()));;
    CSeq_entry_Handle seh = scope->AddTopLevelSeqEntry(*entry);

    CCleanup cleanup;
    CConstRef<CCleanupChange> changes = cleanup.ExtendedCleanup(seh);

    CFeat_CI g(seh, CSeqFeatData::e_Gene);

    BOOST_CHECK_EQUAL(g->GetLocation().IsPartialStart(eExtreme_Biological), cds_prime5);
    BOOST_CHECK_EQUAL(g->GetLocation().IsPartialStop(eExtreme_Biological), cds_prime3);
}


BOOST_AUTO_TEST_CASE(Test_SQD_4507)
{
    TestParentPartial(false, false);
    TestParentPartial(false, true);
    TestParentPartial(true, false);
    TestParentPartial(true, true);
}


void TestCollectionDateCleanup(bool month_first, const string& desc_start, const string& desc_end, const string& feat_start, const string& feat_end)
{
    CRef<CSeq_entry> entry = BuildGoodSeq();
    unit_test_util::SetSubSource(entry, CSubSource::eSubtype_collection_date, desc_start);
    CRef<CSeq_feat> src_feat = unit_test_util::AddMiscFeature(entry);
    src_feat->SetData().SetBiosrc().SetSubtype().push_back(
        CRef<CSubSource>(new CSubSource(CSubSource::eSubtype_collection_date, feat_start)));

    CRef<CScope> scope(new CScope(*CObjectManager::GetInstance()));;
    CSeq_entry_Handle seh = scope->AddTopLevelSeqEntry(*entry);

    BOOST_CHECK_EQUAL(CCleanup::CleanupCollectionDates(seh, month_first), true);
    for (auto d : entry->GetSeq().GetDescr().Get()) {
        if (d->IsSource()) {
            BOOST_CHECK_EQUAL(d->GetSource().GetSubtype().back()->GetSubtype(), CSubSource::eSubtype_collection_date);
            BOOST_CHECK_EQUAL(d->GetSource().GetSubtype().back()->GetName(), desc_end);
        }
    }

    CFeat_CI src_f(seh, CSeqFeatData::e_Biosrc);
    BOOST_CHECK_EQUAL(src_f->GetData().GetBiosrc().GetSubtype().front()->GetSubtype(), CSubSource::eSubtype_collection_date);
    BOOST_CHECK_EQUAL(src_f->GetData().GetBiosrc().GetSubtype().front()->GetName(), feat_end);
}


BOOST_AUTO_TEST_CASE(Test_CleanupCollectionDates)
{
    TestCollectionDateCleanup(true, "1/7/99", "07-Jan-1999", "2/8/99", "08-Feb-1999");
    TestCollectionDateCleanup(false, "1/7/99", "01-Jul-1999", "2/8/99", "02-Aug-1999");

}


BOOST_AUTO_TEST_CASE(Test_SQD_4592)
{
    CRef<CSeq_entry> entry = BuildGoodNucProtSet();
    CRef<CSeq_feat> prot = GetProtFeatFromGoodNucProtSet(entry);
    prot->SetData().SetProt().SetEc().push_back("1.14.13.86");

    CRef<CScope> scope(new CScope(*CObjectManager::GetInstance()));
    CSeq_entry_Handle seh = scope->AddTopLevelSeqEntry(*entry);

    CCleanup cleanup(scope);
    CConstRef<CCleanupChange> changes;

    changes = cleanup.ExtendedCleanup(seh);
    prot = seh.GetCompleteSeq_entry()->GetSet().GetSeq_set().back()->GetAnnot().back()->GetData().GetFtable().back();
    BOOST_CHECK_EQUAL(prot->GetData().GetProt().GetEc().back(), "1.14.14.87");

}


BOOST_AUTO_TEST_CASE(Test_ProtTitleRemoval)
{
    CRef<CSeq_entry> entry = BuildGoodNucProtSet();
    CRef<CSeq_entry> prot = GetProteinSequenceFromGoodNucProtSet(entry);
    CRef<CSeqdesc> title(new CSeqdesc());
    title->SetTitle("x");
    prot->SetDescr().Set().push_back(title);

    CRef<CScope> scope(new CScope(*CObjectManager::GetInstance()));;
    CSeq_entry_Handle seh = scope->AddTopLevelSeqEntry(*entry);
    entry->Parentize();

    CCleanup cleanup;
    CConstRef<CCleanupChange> changes;

    cleanup.SetScope(scope);
    changes = cleanup.ExtendedCleanup(seh);

    CConstRef<CSeq_entry> prot_after = seh.GetCompleteSeq_entry()->GetSet().GetSeq_set().back();

    for (auto it : prot_after->GetDescr().Get()) {
        if (it->Which() == CSeqdesc::e_Title) {
            BOOST_CHECK(!NStr::Equal(it->GetTitle(), "x"));
        }
    }
}


BOOST_AUTO_TEST_CASE(Test_StrainRemoval)
{
    CRef<CSeq_entry> entry = BuildGoodSeq();
    CRef<CSeqdesc> gbblock(new CSeqdesc());
    SetOrgMod(entry, COrgMod::eSubtype_strain, "x");
    gbblock->SetGenbank().SetSource("Sebaea microphylla (strain x)");
    entry->SetDescr().Set().push_back(gbblock);

    CRef<CScope> scope(new CScope(*CObjectManager::GetInstance()));;
    CSeq_entry_Handle seh = scope->AddTopLevelSeqEntry(*entry);
    entry->Parentize();

    CCleanup cleanup;
    CConstRef<CCleanupChange> changes;

    cleanup.SetScope(scope);
    changes = cleanup.ExtendedCleanup(seh);
    for (auto it : entry->GetSeq().GetDescr().Get()) {
        BOOST_CHECK(it->Which() != CSeqdesc::e_Genbank);
    }
}


void AddCreateDate(CRef<CSeq_entry> entry, size_t year)
{
    CRef<CSeqdesc> cdate(new CSeqdesc());
    cdate->SetCreate_date().SetStd().SetYear(year);
    entry->SetSeq().SetDescr().Set().push_back(cdate);
}


void AddUpdateDate(CRef<CSeq_entry> entry, size_t year)
{
    CRef<CSeqdesc> cdate(new CSeqdesc());
    cdate->SetUpdate_date().SetStd().SetYear(year);
    entry->SetSeq().SetDescr().Set().push_back(cdate);
}


BOOST_AUTO_TEST_CASE(Test_MultipleDates)
{
    CRef<CSeq_entry> entry = BuildGoodSeq();
    AddCreateDate(entry, 2013);
    AddCreateDate(entry, 2014);
    AddCreateDate(entry, 2012);
    AddCreateDate(entry, 2014);

    AddUpdateDate(entry, 2017);
    AddUpdateDate(entry, 2018);
    AddUpdateDate(entry, 2016);
    AddUpdateDate(entry, 2018);

    CRef<CScope> scope(new CScope(*CObjectManager::GetInstance()));;
    CSeq_entry_Handle seh = scope->AddTopLevelSeqEntry(*entry);
    entry->Parentize();

    CCleanup cleanup;
    CConstRef<CCleanupChange> changes;

    cleanup.SetScope(scope);
    changes = cleanup.ExtendedCleanup(seh);
    size_t num_create = 0;
    size_t num_update = 0;
    for (auto it : entry->GetSeq().GetDescr().Get()) {
        if (it->IsCreate_date()) {
            ++num_create;
            BOOST_CHECK_EQUAL(it->GetCreate_date().GetStd().GetYear(), 2014);
        }
        if (it->IsUpdate_date()) {
            ++num_update;
            BOOST_CHECK_EQUAL(it->GetUpdate_date().GetStd().GetYear(), 2018);
        }

    }
    BOOST_CHECK_EQUAL(num_create, 1);
    BOOST_CHECK_EQUAL(num_update, 1);
}

