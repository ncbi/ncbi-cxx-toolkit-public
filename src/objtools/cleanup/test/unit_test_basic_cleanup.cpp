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
* Author:  Colleen Bollin, Michael Kornbluh, NCBI
*
* File Description:
*   Sample unit tests file for basic cleanup.
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
#include <objects/general/User_object.hpp>
#include <objects/seqfeat/Cdregion.hpp>
#include <objects/misc/sequence_macros.hpp>
#include <objtools/cleanup/cleanup.hpp>
#include <objtools/unit_test_util/unit_test_util.hpp>

#include <serial/objistrasn.hpp>

#include <util/compress/zlib.hpp>
#include <util/compress/stream.hpp>

USING_NCBI_SCOPE;
USING_SCOPE(objects);

using namespace unit_test_util;

// Needed under windows for some reason.
#ifdef BOOST_NO_EXCEPTIONS

namespace boost
{
void throw_exception( std::exception const & e ) {
    throw e;
}
}

#endif

extern const char* sc_TestEntryCleanRptUnitSeq;

BOOST_AUTO_TEST_CASE(Test_CleanRptUnitSeq)
{
    CSeq_entry entry;
    {{
         CNcbiIstrstream istr(sc_TestEntryCleanRptUnitSeq);
         istr >> MSerial_AsnText >> entry;
     }}

    CRef<CScope> scope(new CScope(*CObjectManager::GetInstance()));;
    CSeq_entry_Handle seh = scope->AddTopLevelSeqEntry(entry);
    entry.Parentize();

    CCleanup cleanup;
    CConstRef<CCleanupChange> changes;

    cleanup.SetScope (scope);
    changes = cleanup.BasicCleanup (entry);
    // look for expected change flags
	vector<string> changes_str = changes->GetAllDescriptions();
	if (changes_str.size() < 1) {
        BOOST_CHECK_EQUAL("missing cleanup", "Change Qualifiers");
	} else {
        BOOST_CHECK_EQUAL (changes_str[0], "Change Qualifiers");
        for (size_t i = 2; i < changes_str.size(); i++) {
            BOOST_CHECK_EQUAL("unexpected cleanup", changes_str[i]);
        }
	}
    // make sure change was actually made
    CFeat_CI f(scope->GetBioseqHandle(entry.GetSeq()));
    while (f) {
        FOR_EACH_GBQUAL_ON_SEQFEAT (q, *f) {
            if ((*q)->IsSetQual() && NStr::Equal((*q)->GetQual(), "rpt_unit_seq") && (*q)->IsSetVal()) {
                string val = (*q)->GetVal();
                string expected = NStr::ToLower(val); 
                BOOST_CHECK_EQUAL (val, expected);
            }
        }
        ++f;
    }
}


const char *sc_TestEntryCleanRptUnitSeq = "\
Seq-entry ::= seq {\
          id {\
            local\
              str \"cleanrptunitseq\" } ,\
          descr {\
            source { \
              org { \
                taxname \"Homo sapiens\" } } , \
            molinfo {\
              biomol genomic } } ,\
          inst {\
            repr raw ,\
            mol dna ,\
            length 27 ,\
            seq-data\
              iupacna \"TTGCCCTAAAAATAAGAGTAAAACTAA\" } , \
              annot { \
                { \
                  data \
                    ftable { \
                      { \
                        data \
                        imp { \
                          key \"repeat_region\" } , \
                        location \
                          int { \
                            from 0 , \
                            to 26 , \
                            id local str \"cleanrptunitseq\" } , \
                          qual { \
                            { \
                              qual \"rpt_unit_seq\" , \
                              val \"AATT\" } } } , \
                      { \
                        data \
                        imp { \
                          key \"repeat_region\" } , \
                        location \
                          int { \
                            from 0 , \
                            to 26 , \
                            id local str \"cleanrptunitseq\" } , \
                          qual { \
                            { \
                              qual \"rpt_unit_seq\" , \
                              val \"AATT;GCC\" } } } } } } } \
";

typedef vector< CRef<CSeq_entry> > TSeqEntryVec;

void s_LoadAllSeqEntries( 
    TSeqEntryVec &out_expected_seq_entries, 
    CObjectIStreamAsn *in_stream )
{
    if( ! in_stream ) {
        return;
    }

    try {
        while( in_stream->InGoodState() ) {
            // get our expected output
            CRef<CSeq_entry> expected_output_seq_entry( new CSeq_entry );
            // The following line throws CEofException if we reach the end
            in_stream->Read( ObjectInfo(*expected_output_seq_entry) );
            if( expected_output_seq_entry ) {
                out_expected_seq_entries.push_back( expected_output_seq_entry );
            }
        }
    } catch(const CEofException &) {
        // okay, no problem
    }
}

enum EExtendedCleanup {
    eExtendedCleanup_DoNotRun,
    eExtendedCleanup_Run
};


// Holds all the matching files
class CFileRememberer : public set<string> {
public:
    void operator() (CDirEntry& de) {
        if( de.IsFile() ) {
            insert( de.GetPath() );
        }
    }
};

const char *sc_TestEntryCleanAssemblyDate = "\
Seq-entry ::= seq {\
          id {\
            local\
              str \"cleanassemblydate\" } ,\
          descr {\
            source { \
              org { \
                taxname \"Homo sapiens\" } } , \
                user { \
                  type str \"StructuredComment\" , \
                  data { \
                    { \
                      label str \"StructuredCommentPrefix\" , \
                      data str \"##Genome-Assembly-Data-START##\" } , \
                    { \
                      label  str \"Assembly Date\" , \
                      data  str \"Feb 1 2014\" } , \
                    { \
                      label  str \"Assembly Date\" , \
                      data  str \"Feb 2014\" } , \
                    { \
                      label  str \"Assembly Date\" , \
                      data  str \"2014\" } , \
                    { \
                      label  str \"Assembly Date\" , \
                      data  str \"01-05-2014\" } , \
                    { \
                      label  str \"StructuredCommentSuffix\" , \
                      data str \"##Genome-Assembly-Data-END##\" } } } , \
            molinfo {\
              biomol genomic } } ,\
          inst {\
            repr raw ,\
            mol dna ,\
            length 27 ,\
            seq-data\
              iupacna \"TTGCCCTAAAAATAAGAGTAAAACTAA\" } } \
";


BOOST_AUTO_TEST_CASE(Test_CleanAssemblyDate)
{
    CSeq_entry entry;
    {{
         CNcbiIstrstream istr(sc_TestEntryCleanAssemblyDate);
         istr >> MSerial_AsnText >> entry;
     }}

    CRef<CScope> scope(new CScope(*CObjectManager::GetInstance()));;
    CSeq_entry_Handle seh = scope->AddTopLevelSeqEntry(entry);
    entry.Parentize();

    CCleanup cleanup;
    CConstRef<CCleanupChange> changes;

    cleanup.SetScope (scope);
    changes = cleanup.BasicCleanup (entry);
    // look for expected change flags
	vector<string> changes_str = changes->GetAllDescriptions();
	if (changes_str.size() < 1) {
        BOOST_CHECK_EQUAL("missing cleanup", "Clean User-Object Or -Field");
	} else {
        BOOST_CHECK_EQUAL (changes_str[0], "Clean User-Object Or -Field");
        for (size_t i = 2; i < changes_str.size(); i++) {
            BOOST_CHECK_EQUAL("unexpected cleanup", changes_str[i]);
        }
	}
    // make sure change was actually made
    CSeqdesc_CI d(scope->GetBioseqHandle(entry.GetSeq()), CSeqdesc::e_User);
    if (d) {
        const CUser_object& usr = d->GetUser();
        BOOST_CHECK_EQUAL(usr.GetData()[1]->GetData().GetStr(), "01-FEB-2014");
        BOOST_CHECK_EQUAL(usr.GetData()[2]->GetData().GetStr(), "FEB-2014");
        BOOST_CHECK_EQUAL(usr.GetData()[3]->GetData().GetStr(), "2014");
        BOOST_CHECK_EQUAL(usr.GetData()[4]->GetData().GetStr(), "2014");
    }
}


const char *sc_TestEntryCleanStructuredVoucher2 = "\
Seq-entry ::= seq {\
          id {\
            local\
              str \"cleanstructuredvoucher\" } ,\
          descr {\
            source { \
              org { \
                taxname \"Homo sapiens\" ,\
                orgname { \
                  mod { \
                    { \
                      subtype culture-collection ,\
                      subname \"a:b\" } } } } } , \
            molinfo {\
              biomol genomic } } ,\
          inst {\
            repr raw ,\
            mol dna ,\
            length 27 ,\
            seq-data\
              iupacna \"TTGCCCTAAAAATAAGAGTAAAACTAA\" } } \
";


BOOST_AUTO_TEST_CASE(Test_CleanStructuredVoucher)
{
    // change removed from basic cleanup
}


BOOST_AUTO_TEST_CASE(Test_CleanStructuredVoucher2)
{
    CSeq_entry entry;
    {{
         CNcbiIstrstream istr(sc_TestEntryCleanStructuredVoucher2);
         istr >> MSerial_AsnText >> entry;
     }}

    CRef<CScope> scope(new CScope(*CObjectManager::GetInstance()));;
    CSeq_entry_Handle seh = scope->AddTopLevelSeqEntry(entry);
    entry.Parentize();

    CCleanup cleanup;
    CConstRef<CCleanupChange> changes;

    cleanup.SetScope (scope);
    changes = cleanup.BasicCleanup (entry);
    // look for expected change flags
    vector<string> changes_str = changes->GetAllDescriptions();
    BOOST_CHECK_EQUAL(changes_str.size(), 0u);

    for (size_t i = 3; i < changes_str.size(); i++) {
        BOOST_CHECK_EQUAL("unexpected cleanup", changes_str[i]);
	}

    // make sure change was not actually made
    CSeqdesc_CI d(scope->GetBioseqHandle(entry.GetSeq()), CSeqdesc::e_Source);
    if (d) {
        const COrgName& on = d->GetSource().GetOrg().GetOrgname();
        ITERATE(COrgName::TMod, it, on.GetMod()) {
            if ((*it)->GetSubtype() == COrgMod::eSubtype_specimen_voucher) {
                BOOST_CHECK_EQUAL((*it)->GetSubname(), "a:b");
            }
        }
    }
}


const char *sc_TestEntryCleanPCRPrimerSeq = "\
Seq-entry ::= seq {\
          id {\
            local\
              str \"cleanpcrprimerseq\" } ,\
          descr {\
            source { \
              org { \
                taxname \"Homo sapiens\" } ,\
            pcr-primers { \
              { \
                forward { \
                  { \
                    seq \"atgTCCAAAccacaaacagagactaaagc\" , \
                    name \"a_f\" } } , \
                reverse { \
                  { \
                    seq \"cttctg<OTHER>ctaca  aataagaat  cgatctc\" , \
                    name \"a_r\" } } } } } , \
            molinfo {\
              biomol genomic } } ,\
          inst {\
            repr raw ,\
            mol dna ,\
            length 27 ,\
            seq-data\
              iupacna \"TTGCCCTAAAAATAAGAGTAAAACTAA\" } } \
";


BOOST_AUTO_TEST_CASE(Test_CleanPCRPrimerSeq)
{
    CSeq_entry entry;
    {{
         CNcbiIstrstream istr(sc_TestEntryCleanPCRPrimerSeq);
         istr >> MSerial_AsnText >> entry;
     }}

    CRef<CScope> scope(new CScope(*CObjectManager::GetInstance()));;
    CSeq_entry_Handle seh = scope->AddTopLevelSeqEntry(entry);
    entry.Parentize();

    CCleanup cleanup;
    CConstRef<CCleanupChange> changes;

    cleanup.SetScope (scope);
    changes = cleanup.BasicCleanup (entry);
    // look for expected change flags
	vector<string> changes_str = changes->GetAllDescriptions();
	if (changes_str.size() < 1) {
        BOOST_CHECK_EQUAL("missing cleanup", "eChangePCRPrimers");
    } else {
        BOOST_CHECK_EQUAL (changes_str[0], "Change PCR Primers");
    }

    for (size_t i = 1; i < changes_str.size(); i++) {
        BOOST_CHECK_EQUAL("unexpected cleanup", changes_str[i]);
	}

    // make sure change was actually made
    CSeqdesc_CI d(scope->GetBioseqHandle(entry.GetSeq()), CSeqdesc::e_Source);
    const CPCRReaction& reaction = *(d->GetSource().GetPcr_primers().Get().front());
    BOOST_CHECK_EQUAL(reaction.GetForward().Get().front()->GetSeq(), "atgtccaaaccacaaacagagactaaagc");
    BOOST_CHECK_EQUAL(reaction.GetReverse().Get().front()->GetSeq(), "cttctg<OTHER>ctacaaataagaatcgatctc");
}


BOOST_AUTO_TEST_CASE(Test_CleanBioSource)
{
    CRef<CBioSource> src(new CBioSource());
    src->SetOrg().SetTaxname("  Homo sapiens  ");

    CCleanup cleanup;
    CConstRef<CCleanupChange> changes;

    changes = cleanup.BasicCleanup (*src);
    BOOST_CHECK_EQUAL(src->GetOrg().GetTaxname(), "Homo sapiens");
}


BOOST_AUTO_TEST_CASE(Test_SplitGBQual)
{
    CRef<CSeq_feat> feat(new CSeq_feat());
    feat->SetData().SetGene();
    feat->SetLocation().SetInt().SetId().SetLocal().SetStr("foo");
    feat->SetLocation().SetInt().SetFrom(0);
    feat->SetLocation().SetInt().SetTo(5);
    CRef<CGb_qual> q(new CGb_qual("rpt_unit_seq", "(1,UUUAGC,3)"));
    feat->SetQual().push_back(q);

    CCleanup cleanup;
    CConstRef<CCleanupChange> changes;

    changes = cleanup.BasicCleanup (*feat);
    BOOST_CHECK_EQUAL(feat->GetQual().size(), 3u);
    BOOST_CHECK_EQUAL(feat->GetQual()[2]->GetVal(), "tttagc");
}


BOOST_AUTO_TEST_CASE(Test_RptUnit)
{
    CRef<CSeq_feat> feat(new CSeq_feat());
    feat->SetData().SetGene();
    feat->SetLocation().SetInt().SetId().SetLocal().SetStr("foo");
    feat->SetLocation().SetInt().SetFrom(0);
    feat->SetLocation().SetInt().SetTo(5);

    feat->SetQual().push_back(CRef<CGb_qual>(new CGb_qual("rpt_unit", "  ()(),    4235  . 236  ()")));
    feat->SetQual().push_back(CRef<CGb_qual>(new CGb_qual("rpt_unit", "(abc,def,hjusdf sdf)")));

    CCleanup cleanup;
    CConstRef<CCleanupChange> changes;

    changes = cleanup.BasicCleanup (*feat);
    BOOST_CHECK_EQUAL(feat->GetQual().size(), 4u);
    BOOST_CHECK_EQUAL(feat->GetQual()[0]->GetVal(), "()(), 4235 . 236 ()");
    BOOST_CHECK_EQUAL(feat->GetQual()[1]->GetVal(), "abc");
    BOOST_CHECK_EQUAL(feat->GetQual()[2]->GetVal(), "def");
    BOOST_CHECK_EQUAL(feat->GetQual()[3]->GetVal(), "hjusdf sdf");
}


BOOST_AUTO_TEST_CASE(Test_SQD_2231)
{
    CRef<CBioSource> src(new CBioSource());
    src->SetOrg().SetTaxname("Homo sapiens");
    src->SetSubtype().push_back(CRef<CSubSource>(new CSubSource(CSubSource::eSubtype_country, "New Zealand: Natural CO2 spring , Hakanoa, Northland")));

    CCleanup cleanup;
    CConstRef<CCleanupChange> changes;

    changes = cleanup.BasicCleanup (*src);
    BOOST_CHECK_EQUAL(src->GetSubtype().front()->GetName(), "New Zealand: Natural CO2 spring, Hakanoa, Northland");

}


BOOST_AUTO_TEST_CASE(Test_SQD_2209)
{
    CRef<CSeq_feat> feat(new CSeq_feat());
    feat->SetData().SetCdregion();
    CRef<CSeq_loc> part1(new CSeq_loc());
    part1->SetInt().SetId().SetLocal().SetStr("foo");
    part1->SetInt().SetFrom(100);
    part1->SetInt().SetTo(223);
    feat->SetLocation().SetMix().Set().push_back(part1);
    CRef<CSeq_loc> part2(new CSeq_loc());
    part2->SetInt().SetId().SetLocal().SetStr("foo");
    part2->SetInt().SetFrom(1549);
    part2->SetInt().SetTo(1600);
    feat->SetLocation().SetMix().Set().push_back(part2);
    CRef<CGb_qual> q(new CGb_qual("transl_except", "(pos:223..224,1550,aa:Met)"));
    feat->SetQual().push_back(q);

    CCleanup cleanup;
    CConstRef<CCleanupChange> changes;

    changes = cleanup.BasicCleanup(*feat);
    BOOST_CHECK_EQUAL(false, feat->IsSetQual());
    BOOST_CHECK_EQUAL(feat->GetData().GetCdregion().GetCode_break().size(), 1);
    const CCode_break& cbr = *(feat->GetData().GetCdregion().GetCode_break().front());
    BOOST_CHECK_EQUAL(cbr.GetAa().GetNcbieaa(), 77);
    BOOST_CHECK_EQUAL(true, cbr.GetLoc().IsMix());
    BOOST_CHECK_EQUAL(cbr.GetLoc().GetMix().Get().size(), 2);
    BOOST_CHECK_EQUAL(true, cbr.GetLoc().GetMix().Get().front()->IsInt());
    BOOST_CHECK_EQUAL(cbr.GetLoc().GetMix().Get().front()->GetInt().GetFrom(), 222);
    BOOST_CHECK_EQUAL(cbr.GetLoc().GetMix().Get().front()->GetInt().GetTo(), 223);
    BOOST_CHECK_EQUAL(true, cbr.GetLoc().GetMix().Get().back()->IsPnt());
    BOOST_CHECK_EQUAL(cbr.GetLoc().GetMix().Get().back()->GetPnt().GetPoint(), 1549);
}

BOOST_AUTO_TEST_CASE(Test_SQD_2212)
{
    CRef<CSeq_feat> feat(new CSeq_feat());
    feat->SetData().SetCdregion();
    CRef<CSeq_loc> part1(new CSeq_loc());
    part1->SetInt().SetId().SetLocal().SetStr("foo");
    part1->SetInt().SetFrom(100);
    part1->SetInt().SetTo(223);
    feat->SetLocation().SetMix().Set().push_back(part1);
    CRef<CSeq_loc> part2(new CSeq_loc());
    part2->SetInt().SetId().SetLocal().SetStr("foo");
    part2->SetInt().SetFrom(1549);
    part2->SetInt().SetTo(1600);
    feat->SetLocation().SetMix().Set().push_back(part2);
    CRef<CGb_qual> q(new CGb_qual("transl_except", "(pos:4..6,aa:Leu)"));
    feat->SetQual().push_back(q);

    CCleanup cleanup;
    CConstRef<CCleanupChange> changes;

    changes = cleanup.BasicCleanup(*feat);
    // should not convert transl_except qual to code break
    // because not in coding region location
    BOOST_CHECK_EQUAL(true, feat->IsSetQual());
    BOOST_CHECK_EQUAL(false, feat->GetData().GetCdregion().IsSetCode_break());
}

BOOST_AUTO_TEST_CASE(Test_SQD_2221)
{
    CRef<CBioSource> src(new CBioSource());
    CRef<CSubSource> q(new CSubSource(CSubSource::eSubtype_fwd_primer_seq, "GATTA  CAgattaca<OTHER>"));
    src->SetSubtype().push_back(q);

    CCleanup cleanup;
    CConstRef<CCleanupChange> changes;

    changes = cleanup.BasicCleanup(*src);
    BOOST_CHECK_EQUAL(false, src->IsSetSubtype());
    BOOST_CHECK_EQUAL(true, src->IsSetPcr_primers());
    BOOST_CHECK_EQUAL(src->GetPcr_primers().Get().front()->GetForward().Get().front()->GetSeq(), "gattacagattaca<OTHER>");
}

BOOST_AUTO_TEST_CASE(Test_SQD_2200)
{
    CRef<CSeq_entry> entry = BuildGoodSeq();

    CRef<CPub> pub(new CPub());
    pub->SetGen().SetCit("unpublished title");
    pub->SetGen().SetAuthors().SetNames().SetMl().push_back("abc");
    pub->SetGen().SetAuthors().SetNames().SetMl().push_back("def");
    CRef<CSeqdesc> d(new CSeqdesc());
    d->SetPub().SetPub().Set().push_back(pub);

    entry->SetSeq().SetDescr().Set().push_back(d);


    CRef<CScope> scope(new CScope(*CObjectManager::GetInstance()));;
    CSeq_entry_Handle seh = scope->AddTopLevelSeqEntry(*entry);
    entry->Parentize();

    CCleanup cleanup;
    CConstRef<CCleanupChange> changes;

    cleanup.SetScope (scope);
    changes = cleanup.BasicCleanup (*entry);

    const CAuth_list& result = entry->GetDescr().Get().back()->GetPub().GetPub().Get().back()->GetGen().GetAuthors();

    BOOST_CHECK_EQUAL(result.GetNames().IsStd(), true);
    BOOST_CHECK_EQUAL(result.GetNames().GetStd().front()->GetName().GetName().GetLast(), "abc");
    BOOST_CHECK_EQUAL(result.GetNames().GetStd().back()->GetName().GetName().GetLast(), "def");
}


BOOST_AUTO_TEST_CASE(Test_SQD_3617)
{
    CRef<CSeq_entry> entry = BuildGoodNucProtSet();
    CRef<CSeq_feat> cds = GetCDSFromGoodNucProtSet(entry);
    TSeqPos stop = cds->GetLocation().GetStop(eExtreme_Biological);

    CRef<CScope> scope(new CScope(*CObjectManager::GetInstance()));;
    CSeq_entry_Handle seh = scope->AddTopLevelSeqEntry(*entry);
    entry->Parentize();

    CBioseq_CI bi(seh, CSeq_inst::eMol_dna);

    BOOST_CHECK_EQUAL(CCleanup::ExtendToStopIfShortAndNotPartial(*cds, *bi), false);
    BOOST_CHECK_EQUAL(cds->GetLocation().GetStop(eExtreme_Biological), stop);
    CSeq_interval& interval = cds->SetLocation().SetInt();

    interval.SetTo(stop - 1);
    BOOST_CHECK_EQUAL(CCleanup::ExtendToStopIfShortAndNotPartial(*cds, *bi), true);
    BOOST_CHECK_EQUAL(cds->GetLocation().GetStop(eExtreme_Biological), stop);

    interval.SetTo(stop - 2);
    BOOST_CHECK_EQUAL(CCleanup::ExtendToStopIfShortAndNotPartial(*cds, *bi), true);
    BOOST_CHECK_EQUAL(cds->GetLocation().GetStop(eExtreme_Biological), stop);

    interval.SetTo(stop - 3);
    BOOST_CHECK_EQUAL(CCleanup::ExtendToStopIfShortAndNotPartial(*cds, *bi), true);
    BOOST_CHECK_EQUAL(cds->GetLocation().GetStop(eExtreme_Biological), stop);

    interval.SetTo(stop - 4);
    BOOST_CHECK_EQUAL(CCleanup::ExtendToStopIfShortAndNotPartial(*cds, *bi), false);
    BOOST_CHECK_EQUAL(cds->GetLocation().GetStop(eExtreme_Biological), stop - 4);

    CRef<CSeq_id> id(new CSeq_id());
    id->Assign(*(cds->GetLocation().GetId()));
    cds->SetLocation().Assign(*(MakeMixLoc(id)));
    stop = cds->GetLocation().GetStop(eExtreme_Biological);
    BOOST_CHECK_EQUAL(CCleanup::ExtendToStopIfShortAndNotPartial(*cds, *bi), false);
    BOOST_CHECK_EQUAL(cds->GetLocation().GetStop(eExtreme_Biological), stop);

    cds->SetLocation().SetMix().Set().back()->SetInt().SetTo(stop - 1);
    BOOST_CHECK_EQUAL(CCleanup::ExtendToStopIfShortAndNotPartial(*cds, *bi), true);
    BOOST_CHECK_EQUAL(cds->GetLocation().GetStop(eExtreme_Biological), stop);

    cds->SetLocation().SetMix().Set().back()->SetInt().SetTo(stop - 2);
    BOOST_CHECK_EQUAL(CCleanup::ExtendToStopIfShortAndNotPartial(*cds, *bi), true);
    BOOST_CHECK_EQUAL(cds->GetLocation().GetStop(eExtreme_Biological), stop);

    cds->SetLocation().SetMix().Set().back()->SetInt().SetTo(stop - 3);
    BOOST_CHECK_EQUAL(CCleanup::ExtendToStopIfShortAndNotPartial(*cds, *bi), true);
    BOOST_CHECK_EQUAL(cds->GetLocation().GetStop(eExtreme_Biological), stop);

    cds->SetLocation().SetMix().Set().back()->SetInt().SetTo(stop - 4);
    BOOST_CHECK_EQUAL(CCleanup::ExtendToStopIfShortAndNotPartial(*cds, *bi), false);
    BOOST_CHECK_EQUAL(cds->GetLocation().GetStop(eExtreme_Biological), stop - 4);

    cds->SetLocation().SetMix().Set().back()->SetInt().SetTo(stop);

    cds->SetLocation().Assign(*(MakeMixLoc(id)));
    RevComp(entry);
    stop = cds->GetLocation().GetStop(eExtreme_Biological);
    BOOST_CHECK_EQUAL(CCleanup::ExtendToStopIfShortAndNotPartial(*cds, *bi), false);
    BOOST_CHECK_EQUAL(cds->GetLocation().GetStop(eExtreme_Biological), stop);

    cds->SetLocation().SetMix().Set().back()->SetInt().SetFrom(stop + 1);
    BOOST_CHECK_EQUAL(CCleanup::ExtendToStopIfShortAndNotPartial(*cds, *bi), true);
    BOOST_CHECK_EQUAL(cds->GetLocation().GetStop(eExtreme_Biological), stop);

    cds->SetLocation().SetMix().Set().back()->SetInt().SetFrom(stop + 2);
    BOOST_CHECK_EQUAL(CCleanup::ExtendToStopIfShortAndNotPartial(*cds, *bi), true);
    BOOST_CHECK_EQUAL(cds->GetLocation().GetStop(eExtreme_Biological), stop);

    cds->SetLocation().SetMix().Set().back()->SetInt().SetFrom(stop + 3);
    BOOST_CHECK_EQUAL(CCleanup::ExtendToStopIfShortAndNotPartial(*cds, *bi), true);
    BOOST_CHECK_EQUAL(cds->GetLocation().GetStop(eExtreme_Biological), stop);

    cds->SetLocation().SetMix().Set().back()->SetInt().SetFrom(stop + 4);
    BOOST_CHECK_EQUAL(CCleanup::ExtendToStopIfShortAndNotPartial(*cds, *bi), false);
    BOOST_CHECK_EQUAL(cds->GetLocation().GetStop(eExtreme_Biological), stop + 4);


}


BOOST_AUTO_TEST_CASE(Test_SetFrameFromLoc)
{
    CRef<CSeq_entry> entry = BuildGoodSeq();

    CRef<CSeq_feat> misc = AddMiscFeature(entry);

    CRef<CScope> scope(new CScope(*CObjectManager::GetInstance()));;
    CSeq_entry_Handle seh = scope->AddTopLevelSeqEntry(*entry);

    CRef<CCdregion> cdr(new CCdregion());

    // always set to one if 5' complete
    BOOST_CHECK_EQUAL(CCleanup::SetFrameFromLoc(*cdr, misc->GetLocation(), *scope), true);
    BOOST_CHECK_EQUAL(cdr->GetFrame(), CCdregion::eFrame_one);
    BOOST_CHECK_EQUAL(CCleanup::SetFrameFromLoc(*cdr, misc->GetLocation(), *scope), false);

    misc->SetLocation().SetPartialStart(true, eExtreme_Biological);
    BOOST_CHECK_EQUAL(CCleanup::SetFrameFromLoc(*cdr, misc->GetLocation(), *scope), true);
    BOOST_CHECK_EQUAL(cdr->GetFrame(), CCdregion::eFrame_three);
    BOOST_CHECK_EQUAL(CCleanup::SetFrameFromLoc(*cdr, misc->GetLocation(), *scope), false);
    
    misc->SetLocation().SetInt().SetFrom(1);
    BOOST_CHECK_EQUAL(CCleanup::SetFrameFromLoc(*cdr, misc->GetLocation(), *scope), true);
    BOOST_CHECK_EQUAL(cdr->GetFrame(), CCdregion::eFrame_two);
    BOOST_CHECK_EQUAL(CCleanup::SetFrameFromLoc(*cdr, misc->GetLocation(), *scope), false);

    misc->SetLocation().SetInt().SetFrom(2);
    BOOST_CHECK_EQUAL(CCleanup::SetFrameFromLoc(*cdr, misc->GetLocation(), *scope), true);
    BOOST_CHECK_EQUAL(cdr->GetFrame(), CCdregion::eFrame_one);
    BOOST_CHECK_EQUAL(CCleanup::SetFrameFromLoc(*cdr, misc->GetLocation(), *scope), false);

    misc->SetLocation().SetInt().SetFrom(3);
    misc->SetLocation().SetPartialStop(true, eExtreme_Biological);
    BOOST_CHECK_EQUAL(CCleanup::SetFrameFromLoc(*cdr, misc->GetLocation(), *scope), false);
    BOOST_CHECK_EQUAL(cdr->GetFrame(), CCdregion::eFrame_one);
}


BOOST_AUTO_TEST_CASE(Test_DecodeXMLMarkChanged)
{
    string str = "multicopper oxidase&#13&#10";
    CCleanup::DecodeXMLMarkChanged(str);
    BOOST_CHECK_EQUAL(str, "multicopper oxidase");
}


void CheckQuals(const CSeq_feat::TQual& expected, const CSeq_feat::TQual& actual)
{
    BOOST_CHECK_EQUAL(expected.size(), actual.size());
    CSeq_feat::TQual::const_iterator it1 = expected.cbegin();
    CSeq_feat::TQual::const_iterator it2 = actual.cbegin();
    while (it1 != expected.cend() && it2 != actual.cend()) {
        BOOST_CHECK_EQUAL((*it1)->GetQual(), (*it2)->GetQual());
        BOOST_CHECK_EQUAL((*it1)->GetVal(), (*it2)->GetVal());
        ++it1;
        ++it2;
    }       
}


BOOST_AUTO_TEST_CASE(Test_SQD_3943)
{
    // do not change order of product qualifiers
    CCleanup cleanup;
    CConstRef<CCleanupChange> changes;

    CRef<CSeq_feat> sf(new CSeq_feat());
    sf->SetData().SetImp().SetKey("misc_feature");
    sf->SetLocation().SetInt().SetId().SetLocal().SetStr("seq");
    sf->SetLocation().SetInt().SetFrom(0);
    sf->SetLocation().SetInt().SetTo(10);

    CSeq_feat::TQual expected;   

    sf->SetQual().push_back(CRef<CGb_qual>(new CGb_qual("product", "c")));
    sf->SetQual().push_back(CRef<CGb_qual>(new CGb_qual("product", "b")));
    sf->SetQual().push_back(CRef<CGb_qual>(new CGb_qual("product", "f")));

    // expect no change
    expected.push_back(CRef<CGb_qual>(new CGb_qual("product", "c")));
    expected.push_back(CRef<CGb_qual>(new CGb_qual("product", "b")));
    expected.push_back(CRef<CGb_qual>(new CGb_qual("product", "f")));

    cleanup.BasicCleanup(*sf);
    CheckQuals(expected, sf->GetQual());

    sf->SetQual().push_back((CRef<CGb_qual>(new CGb_qual("satellite", "satellite:s"))));
    expected.push_back((CRef<CGb_qual>(new CGb_qual("satellite", "satellite:s"))));
    cleanup.BasicCleanup(*sf);
    CheckQuals(expected, sf->GetQual());

    // allele should be sorted to the beginning
    sf->SetQual().push_back((CRef<CGb_qual>(new CGb_qual("allele", "a"))));
    expected.insert(expected.begin(), CRef<CGb_qual>(new CGb_qual("allele", "a")));
    cleanup.BasicCleanup(*sf);
    CheckQuals(expected, sf->GetQual());

    // anticodon should be sorted to the end, because it is illegal
    sf->SetQual().insert(sf->SetQual().begin(), CRef<CGb_qual>(new CGb_qual("anticodon", "bad")));
    expected.push_back(CRef<CGb_qual>(new CGb_qual("anticodon", "bad")));
    cleanup.BasicCleanup(*sf);
    CheckQuals(expected, sf->GetQual());

    // non-unique anticodon should be removed
    sf->SetQual().insert(sf->SetQual().begin(), CRef<CGb_qual>(new CGb_qual("anticodon", "bad")));
    cleanup.BasicCleanup(*sf);
    CheckQuals(expected, sf->GetQual());

}


BOOST_AUTO_TEST_CASE(Test_SQD_3994)
{
    CRef<CName_std> name(new CName_std());

    //should extract Sr.
    name->SetLast("Pogson Sr.");
    BOOST_CHECK_EQUAL(name->ExtractSuffixFromLastName(), true);
    BOOST_CHECK_EQUAL(name->GetLast(), "Pogson");
    BOOST_CHECK_EQUAL(name->GetSuffix(), "Sr.");
    
    //should not extract II because suffix is already set
    name->SetLast("Pogson II");
    BOOST_CHECK_EQUAL(name->ExtractSuffixFromLastName(), false);
    BOOST_CHECK_EQUAL(name->GetLast(), "Pogson II");
    BOOST_CHECK_EQUAL(name->GetSuffix(), "Sr.");

    //should extract II if suffix is blank
    name->SetSuffix("");
    BOOST_CHECK_EQUAL(name->ExtractSuffixFromLastName(), true);
    BOOST_CHECK_EQUAL(name->GetLast(), "Pogson");
    BOOST_CHECK_EQUAL(name->GetSuffix(), "II");

    // try the rest
    name->ResetSuffix();
    name->SetLast("Pogson III");
    BOOST_CHECK_EQUAL(name->ExtractSuffixFromLastName(), true);
    BOOST_CHECK_EQUAL(name->GetLast(), "Pogson");
    BOOST_CHECK_EQUAL(name->GetSuffix(), "III");

    name->ResetSuffix();
    name->SetLast("Pogson IV");
    BOOST_CHECK_EQUAL(name->ExtractSuffixFromLastName(), true);
    BOOST_CHECK_EQUAL(name->GetLast(), "Pogson");
    BOOST_CHECK_EQUAL(name->GetSuffix(), "IV");

    name->ResetSuffix();
    name->SetLast("Pogson Jr");
    BOOST_CHECK_EQUAL(name->ExtractSuffixFromLastName(), true);
    BOOST_CHECK_EQUAL(name->GetLast(), "Pogson");
    BOOST_CHECK_EQUAL(name->GetSuffix(), "Jr.");

    name->ResetSuffix();
    name->SetLast("Pogson Sr");
    BOOST_CHECK_EQUAL(name->ExtractSuffixFromLastName(), true);
    BOOST_CHECK_EQUAL(name->GetLast(), "Pogson");
    BOOST_CHECK_EQUAL(name->GetSuffix(), "Sr.");
}


BOOST_AUTO_TEST_CASE(Test_trna_product_qual_cleanup)
{
    CRef<CSeq_feat> trna(new CSeq_feat());
    trna->SetData().SetRna().SetType(CRNA_ref::eType_tRNA);
    trna->SetData().SetRna().SetExt().SetTRNA().SetAa().SetNcbieaa(86);
    trna->SetComment("SMAX5B_nc006978T1");
    trna->SetLocation().SetInt().SetFrom(7572);
    trna->SetLocation().SetInt().SetTo(7644);
    trna->SetLocation().SetInt().SetStrand(eNa_strand_minus);
    trna->SetLocation().SetInt().SetId().SetEmbl().SetAccession("LN913684");
    trna->SetQual().push_back(CRef<CGb_qual>(new CGb_qual("product", "trna.Val")));

    CCleanup cleanup;
    CConstRef<CCleanupChange> changes;
    CRef<CScope> scope(new CScope(*CObjectManager::GetInstance()));;
    cleanup.SetScope(scope);
    changes = cleanup.BasicCleanup(*trna);

    BOOST_CHECK_EQUAL(trna->IsSetQual(), true);
    BOOST_CHECK_EQUAL(trna->GetQual().front()->GetQual(), "product");

}

