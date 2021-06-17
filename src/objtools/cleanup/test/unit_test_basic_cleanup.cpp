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
#include <objmgr/util/objutil.hpp>

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

extern const string sc_TestEntryCleanRptUnitSeq;

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


const string sc_TestEntryCleanRptUnitSeq = "\
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

const string sc_TestEntryCleanAssemblyDate = "\
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


const string sc_TestEntryCleanStructuredVoucher2 = "\
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


const string sc_TestEntryCleanPCRPrimerSeq = "\
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


BOOST_AUTO_TEST_CASE(Test_ParseCodeBreak)
{
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodNucProtSet();
    CRef<CSeq_feat> cds = unit_test_util::GetCDSFromGoodNucProtSet(entry);

    CRef<CScope> scope(new CScope(*CObjectManager::GetInstance()));;
    CSeq_entry_Handle seh = scope->AddTopLevelSeqEntry(*entry);

    BOOST_CHECK_EQUAL(CCleanup::ParseCodeBreak(*cds, cds->SetData().SetCdregion(), "(pos:3..5,aa:Met)", *scope), true);
    BOOST_CHECK_EQUAL(cds->GetData().GetCdregion().IsSetCode_break(), true);
    BOOST_CHECK_EQUAL(cds->GetData().GetCdregion().GetCode_break().size(), 1);

    cds->SetData().SetCdregion().ResetCode_break();

    CRef<CGb_qual> q(new CGb_qual("transl_except", "(pos:6..8,aa:Cys)"));
    cds->SetQual().push_back(q);

    BOOST_CHECK_EQUAL(CCleanup::ParseCodeBreaks(*cds, *scope), true);
    BOOST_CHECK_EQUAL(cds->GetData().GetCdregion().IsSetCode_break(), true);
    BOOST_CHECK_EQUAL(cds->GetData().GetCdregion().GetCode_break().size(), 1);
    BOOST_CHECK_EQUAL(cds->IsSetQual(), false);

    cds->SetData().SetCdregion().ResetCode_break();
    cds->SetQual().push_back(CRef<CGb_qual>(new CGb_qual("transl_except", "(pos:25, aa:TERM)")));
    cds->SetLocation().SetInt().SetTo(24);
    BOOST_CHECK_EQUAL(CCleanup::ParseCodeBreaks(*cds, *scope), true);
    BOOST_CHECK_EQUAL(cds->GetData().GetCdregion().IsSetCode_break(), true);
    BOOST_CHECK_EQUAL(cds->GetData().GetCdregion().GetCode_break().size(), 1);
    BOOST_CHECK_EQUAL(cds->IsSetQual(), false);

    // SQD-4666
    cds->SetData().SetCdregion().ResetCode_break();
    cds->SetLocation().SetInt().SetTo(59);
    cds->SetQual().push_back(CRef<CGb_qual>(new CGb_qual("transl_except", "(pos:61, aa:TERM)")));
    BOOST_CHECK_EQUAL(CCleanup::ParseCodeBreaks(*cds, *scope), false);
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


void CheckStd(const CName_std& n1, const CName_std& n2)
{
#define CHECK_NAME_STD_FIELD(Fld) \
    BOOST_CHECK_EQUAL(n1.IsSet##Fld(), n2.IsSet##Fld()); \
    if (n1.IsSet##Fld()) { \
        BOOST_CHECK_EQUAL(n1.Get##Fld(), n2.Get##Fld()); \
    }

    CHECK_NAME_STD_FIELD(Last)
    CHECK_NAME_STD_FIELD(First)
    CHECK_NAME_STD_FIELD(Initials)
    CHECK_NAME_STD_FIELD(Suffix)
    CHECK_NAME_STD_FIELD(Full)
    CHECK_NAME_STD_FIELD(Title)
    CHECK_NAME_STD_FIELD(Middle)
}


void CompareOldAndNew(const CAuthor& orig)
{
    CRef<CAuthor> new_result(new CAuthor());
    new_result->Assign(orig);

    CCleanup::CleanupAuthor(*new_result);

    CRef<CPub> pub(new CPub());
    pub->SetGen().SetCit("unpublished title");
    CRef<CAuthor> auth(new CAuthor());
    auth->Assign(orig);
    switch (orig.GetName().Which()) {
        case CAuthor::TName::e_Consortium:
            pub->SetGen().SetAuthors().SetNames().SetStd().push_back(auth);
            break;
        case CAuthor::TName::e_Ml:
            pub->SetGen().SetAuthors().SetNames().SetStd().push_back(auth);
            break;
        case CAuthor::TName::e_Str:
            pub->SetGen().SetAuthors().SetNames().SetStd().push_back(auth);
            break;
        case CAuthor::TName::e_Name:
            pub->SetGen().SetAuthors().SetNames().SetStd().push_back(auth);
            break;
        default:
            break;
    }
    CRef<CSeqdesc> d(new CSeqdesc());
    d->SetPub().SetPub().Set().push_back(pub);

    CRef<CSeq_entry> entry = BuildGoodSeq();
    entry->SetSeq().SetDescr().Set().push_back(d);

    CRef<CScope> scope(new CScope(*CObjectManager::GetInstance()));;
    CSeq_entry_Handle seh = scope->AddTopLevelSeqEntry(*entry);
    entry->Parentize();

    CCleanup cleanup;
    CConstRef<CCleanupChange> changes;

    cleanup.SetScope (scope);
    changes = cleanup.BasicCleanup (*entry);

    const CPub& pub_c = *(entry->GetDescr().Get().back()->GetPub().GetPub().Get().back());
    const CAuthor& old_result = *(pub_c.GetGen().GetAuthors().GetNames().GetStd().front());

    if (new_result->IsSetName()) {
        BOOST_CHECK_EQUAL(old_result.GetName().Which(), new_result->GetName().Which());
        switch(new_result->GetName().Which()) {
            case CAuthor::TName::e_Consortium:
                BOOST_CHECK_EQUAL(old_result.GetName().GetConsortium(), new_result->GetName().GetConsortium());
                break;
            case CAuthor::TName::e_Ml:
                BOOST_CHECK_EQUAL(old_result.GetName().GetMl(), new_result->GetName().GetMl());
                break;
            case CAuthor::TName::e_Str:
                BOOST_CHECK_EQUAL(old_result.GetName().GetStr(), new_result->GetName().GetStr());
                break;
            case CAuthor::TName::e_Name:
                BOOST_CHECK(old_result.GetName().GetName().Equals(new_result->GetName().GetName()));
                CheckStd(old_result.GetName().GetName(), new_result->GetName().GetName());
                break;
            case CAuthor::TName::e_not_set:
                BOOST_ASSERT("Author name was not set, it should be");
                break;
            case CAuthor::TName::e_Dbtag:
                BOOST_ASSERT("Unexpected author name type dbtag");
                break;
        }
    } else {
        BOOST_CHECK(!old_result.IsSetName());
    }
}


void CheckPubAuth(const string& first_in, const string& init_in, const string& last_in,
                  const string& first_out, const string& init_out, const string& last_out)
{
    CRef<CPub> pub(new CPub());
    pub->SetGen().SetCit("unpublished title");
    CRef<CAuthor> auth(new CAuthor());
    auth->SetName().SetName().SetLast(last_in);
    auth->SetName().SetName().SetFirst(first_in);
    auth->SetName().SetName().SetInitials(init_in);
    pub->SetGen().SetAuthors().SetNames().SetStd().push_back(auth);
    CRef<CSeqdesc> d(new CSeqdesc());
    d->SetPub().SetPub().Set().push_back(pub);

    CRef<CSeq_entry> entry = BuildGoodSeq();
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
    BOOST_CHECK_EQUAL(result.GetNames().GetStd().front()->GetName().GetName().GetLast(), last_out);
    BOOST_CHECK_EQUAL(result.GetNames().GetStd().front()->GetName().GetName().GetFirst(), first_out);
    BOOST_CHECK_EQUAL(result.GetNames().GetStd().front()->GetName().GetName().GetInitials(), init_out);
}


BOOST_AUTO_TEST_CASE(Test_SQD_4498)
{
    vector<string> interesting_strings = {
        "x", "X", "X Y", "X.Y", ".XY.",
        " X ", "; X ;", "et", "al", "et al",
        ".JR.", "3rd", "A-B", "St. John"
    };

    CRef<CAuthor> auth(new CAuthor());
    for (auto s : interesting_strings) {
        auth->SetName().SetStr(s);
        CompareOldAndNew(*auth);
        auth->SetName().SetMl(s);
        CompareOldAndNew(*auth);
        auth->SetName().SetConsortium(s);
        CompareOldAndNew(*auth);
        auth->SetName().SetName().SetLast(s);
        CompareOldAndNew(*auth);
        auth->ResetName();
        auth->SetName().SetName().SetLast("Safe");
        auth->SetName().SetName().SetFirst(s);
        CompareOldAndNew(*auth);
        auth->ResetName();
        auth->SetName().SetName().SetLast("Safe");
        auth->SetName().SetName().SetInitials(s);
        CompareOldAndNew(*auth);
        auth->ResetName();
        auth->SetName().SetName().SetLast("Safe");
        auth->SetName().SetName().SetSuffix(s);
        CompareOldAndNew(*auth);
        auth->ResetName();
        auth->SetName().SetName().SetLast("Safe");
        auth->SetName().SetName().SetTitle(s);
        CompareOldAndNew(*auth);
        auth->ResetName();
        auth->SetName().SetName().SetLast("Safe");
        auth->SetName().SetName().SetMiddle(s);
        CompareOldAndNew(*auth);
        auth->ResetName();
        auth->SetName().SetName().SetLast("Safe");
        auth->SetName().SetName().SetFull(s);
        CompareOldAndNew(*auth);
        auth->ResetName();
    }

    auth->SetName().SetName().SetLast("et");
    auth->SetName().SetName().SetInitials("al");
    CompareOldAndNew(*auth);

    auth->ResetName();
    auth->SetName().SetName().SetFirst("a");
    auth->SetName().SetName().SetInitials("b");
    auth->SetName().SetName().SetLast("c");
    CompareOldAndNew(*auth);

    auth->ResetName();
    auth->SetName().SetName().SetFirst("a");
    auth->SetName().SetName().SetMiddle("b");
    auth->SetName().SetName().SetLast("c");
    CompareOldAndNew(*auth);

    auth->ResetName();
    auth->SetName().SetName().SetFirst("a");
    auth->SetName().SetName().SetLast("b");
    auth->SetName().SetName().SetInitials("III");
    CompareOldAndNew(*auth);

    CheckPubAuth("E.K", "E.", "Radhakrishnan", "EK", "E.", "Radhakrishnan");
    CheckPubAuth("Radhakrishnan", "R.", "E.K", "Radhakrishnan", "R.", "E.K");
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


void TestMoveTrnaProductQualToNote(const string& qual, bool keep)
{
    CRef<CSeq_feat> trna(new CSeq_feat());
    trna->SetData().SetRna().SetType(CRNA_ref::eType_tRNA);
    trna->SetData().SetRna().SetExt().SetTRNA();
    trna->SetLocation().SetInt().SetFrom(7572);
    trna->SetLocation().SetInt().SetTo(7644);
    trna->SetLocation().SetInt().SetStrand(eNa_strand_minus);
    trna->SetLocation().SetInt().SetId().SetEmbl().SetAccession("LN913684");
    trna->SetQual().push_back(CRef<CGb_qual>(new CGb_qual("product", qual)));

    CCleanup cleanup;
    CConstRef<CCleanupChange> changes;
    CRef<CScope> scope(new CScope(*CObjectManager::GetInstance()));;
    cleanup.SetScope(scope);
    changes = cleanup.BasicCleanup(*trna);

    BOOST_CHECK_EQUAL(trna->GetData().GetRna().GetExt().GetTRNA().IsSetAa(), true);
    BOOST_CHECK_EQUAL(trna->GetData().GetRna().GetExt().GetTRNA().IsSetCodon(), false);
    if (keep) {
        BOOST_CHECK_EQUAL(trna->IsSetComment(), false);
        BOOST_CHECK_EQUAL(trna->GetQual().size(), 1);
    } else {
        BOOST_CHECK_EQUAL(trna->IsSetQual(), false);
        BOOST_CHECK_EQUAL(trna->GetComment(), qual);
    }
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

    TestMoveTrnaProductQualToNote("tRNA-Asn-GTT", false);
    TestMoveTrnaProductQualToNote("transfer RNA-Leu (CAA)", false);
    TestMoveTrnaProductQualToNote("tRNA-fMet", true);
}


BOOST_AUTO_TEST_CASE(Test_double_comma)
{
    CRef<CSeq_feat> misc(new CSeq_feat());
    misc->SetComment("ORF30, len: 104 aa,, similar to cytochrome c-553 precursor(c553)");
    CCleanup cleanup;
    CConstRef<CCleanupChange> changes;
    CRef<CScope> scope(new CScope(*CObjectManager::GetInstance()));;
    cleanup.SetScope(scope);
    changes = cleanup.BasicCleanup(*misc);

    BOOST_CHECK_EQUAL(misc->GetComment(), "ORF30, len: 104 aa, similar to cytochrome c-553 precursor(c553)");
}

BOOST_AUTO_TEST_CASE(Test_repeat_type_qual)
{
    BOOST_CHECK_EQUAL(CGb_qual::IsValidRptTypeValue("invalid_value"), false);

    string test_val("dispersed,long_terminal_repeat");
    BOOST_CHECK_EQUAL(CGb_qual::IsValidRptTypeValue(test_val), true);

    string old_val(test_val);
    BOOST_CHECK_EQUAL(CGb_qual::FixRptTypeValue(test_val), false);
    BOOST_CHECK_EQUAL(old_val, test_val);

    test_val = "Centromeric_Repeat,Inverted";
    BOOST_CHECK_EQUAL(CGb_qual::IsValidRptTypeValue(test_val), true);

    BOOST_CHECK_EQUAL(CGb_qual::FixRptTypeValue(test_val), true);
    BOOST_CHECK_EQUAL(test_val, "centromeric_repeat,inverted");

    test_val = "Nested,(tandem),y_prime_element";
    BOOST_CHECK_EQUAL(CGb_qual::IsValidRptTypeValue(test_val), true);

    BOOST_CHECK_EQUAL(CGb_qual::FixRptTypeValue(test_val), true);
    BOOST_CHECK_EQUAL(test_val, "nested,(tandem),Y_prime_element");

    test_val = "engineered_foreign_repetitive_element,(non_LTR_retrotransposon_polymeric_tract)";
    BOOST_CHECK_EQUAL(CGb_qual::IsValidRptTypeValue(test_val), true);

    BOOST_CHECK_EQUAL(CGb_qual::FixRptTypeValue(test_val), false);
}

BOOST_AUTO_TEST_CASE(Test_regulatory_class_qual)
{
    string test_val("tata_box");
    BOOST_CHECK_EQUAL(CSeqFeatData::FixRegulatoryClassValue(test_val), true);
    BOOST_CHECK_EQUAL(test_val, "TATA_box");

    test_val = "minus_10_signal";
    BOOST_CHECK_EQUAL(CSeqFeatData::FixRegulatoryClassValue(test_val), false);
    BOOST_CHECK_EQUAL(test_val, "minus_10_signal");

    test_val = "invalid_value";
    BOOST_CHECK_EQUAL(CSeqFeatData::FixRegulatoryClassValue(test_val), false);
    BOOST_CHECK_EQUAL(test_val, "invalid_value");
}

BOOST_AUTO_TEST_CASE(Test_ncRNA_class)
{
    string test_val("antisense_rna");
    BOOST_CHECK_EQUAL(CRNA_gen::FixncRNAClassValue(test_val), true);
    BOOST_CHECK_EQUAL(test_val, "antisense_RNA");

    test_val = "hammerhead_ribozyme";
    BOOST_CHECK_EQUAL(CRNA_gen::FixncRNAClassValue(test_val), false);
    BOOST_CHECK_EQUAL(test_val, "hammerhead_ribozyme");

    test_val = "invalid_value";
    BOOST_CHECK_EQUAL(CRNA_gen::FixncRNAClassValue(test_val), false);
    BOOST_CHECK_EQUAL(test_val, "invalid_value");
}

BOOST_AUTO_TEST_CASE(Test_pseudogene_qual)
{
    string test_val("invalid_value");
    BOOST_CHECK_EQUAL(CGb_qual::IsValidPseudogeneValue(test_val), false);
    BOOST_CHECK_EQUAL(CGb_qual::FixPseudogeneValue(test_val), false);
    BOOST_CHECK_EQUAL(test_val, "invalid_value");

    test_val = "Allelic";
    BOOST_CHECK_EQUAL(CGb_qual::IsValidPseudogeneValue(test_val), true);
    BOOST_CHECK_EQUAL(CGb_qual::FixPseudogeneValue(test_val), true);
    BOOST_CHECK_EQUAL(test_val, "allelic");

    test_val = "unprocessed";
    BOOST_CHECK_EQUAL(CGb_qual::IsValidPseudogeneValue(test_val), true);
    BOOST_CHECK_EQUAL(CGb_qual::FixRptTypeValue(test_val), false);
    BOOST_CHECK_EQUAL(test_val, "unprocessed");
}


BOOST_AUTO_TEST_CASE(Test_RemoveEmptyStructuredCommentField)
{
    CRef<CSeq_entry> entry = BuildGoodSeq();

    CRef<CUser_object> user(new CUser_object());
    user->SetObjectType(CUser_object::eObjectType_StructuredComment);
    CRef<CUser_field> field1(new CUser_field());
    field1->SetLabel().SetStr("I have a value");
    field1->SetData().SetStr("A value");
    user->SetData().push_back(field1);
    CRef<CUser_field> field2(new CUser_field());
    field2->SetLabel().SetStr("I am blank");
    field2->SetData().SetStr(" ");
    user->SetData().push_back(field2);
    CRef<CUser_field> field3(new CUser_field());
    field3->SetLabel().SetStr("I am empty");
    user->SetData().push_back(field3);

    CRef<CSeqdesc> desc(new CSeqdesc());
    desc->SetUser().Assign(*user);
    entry->SetSeq().SetDescr().Set().push_back(desc);

    CRef<CScope> scope(new CScope(*CObjectManager::GetInstance()));;
    CSeq_entry_Handle seh = scope->AddTopLevelSeqEntry(*entry);
    entry->Parentize();

    CCleanup cleanup;
    CConstRef<CCleanupChange> changes;

    cleanup.SetScope(scope);
    changes = cleanup.BasicCleanup(*entry);

    BOOST_CHECK_EQUAL(desc->GetUser().GetData().size(), 1);
    BOOST_CHECK_EQUAL(desc->GetUser().GetData().front()->GetLabel().GetStr(), "I have a value");
}


void TestFindThisNotStrain(COrgMod::ESubtype subtype, const string& prefix)
{
    CRef<CSeq_entry> entry = BuildGoodSeq();
    unit_test_util::SetOrgMod(entry, COrgMod::eSubtype_strain, prefix + " XYZ");
    CRef<CScope> scope(new CScope(*CObjectManager::GetInstance()));;
    CSeq_entry_Handle seh = scope->AddTopLevelSeqEntry(*entry);
    entry->Parentize();

    CCleanup cleanup;
    CConstRef<CCleanupChange> changes;

    cleanup.SetScope(scope);
    changes = cleanup.BasicCleanup(*entry);

    ITERATE(objects::CSeq_descr::Tdata, it, entry->GetSeq().GetDescr().Get()) {
        if ((*it)->IsSource()) {
            if ((*it)->GetSource().IsSetOrg() &&
                (*it)->GetSource().GetOrg().IsSetOrgname() &&
                (*it)->GetSource().GetOrg().GetOrgname().IsSetMod()) {
                bool found_strain = false;
                bool found_subtype = false;
                ITERATE(COrgName::TMod, m, (*it)->GetSource().GetOrg().GetOrgname().GetMod()) {
                    if ((*m)->IsSetSubtype()) {
                        if ((*m)->GetSubtype() == COrgMod::eSubtype_strain) {
                            found_strain = true;
                        } else if ((*m)->GetSubtype() == subtype) {
                            found_subtype = true;
                            BOOST_CHECK_EQUAL((*m)->GetSubname(), "XYZ");
                        }
                    }
                }
                if (found_strain) {
                    BOOST_CHECK_EQUAL("Error", "Strain should not be present");
                }
                if (!found_subtype) {
                    BOOST_CHECK_EQUAL("Error", COrgMod::GetSubtypeName(subtype) + " is missing");
                }
            } else {
                BOOST_CHECK_EQUAL("Error", "No orgmods!");
            }
        }
    }
}


BOOST_AUTO_TEST_CASE(Test_MoveNamedQualInStrain)
{
    TestFindThisNotStrain(COrgMod::eSubtype_sub_species, "subsp.");
    TestFindThisNotStrain(COrgMod::eSubtype_serovar, "serovar");

}


BOOST_AUTO_TEST_CASE(Test_FixPartialProteinTitle)
{
    CRef<CSeq_entry> entry = BuildGoodNucProtSet();
    CRef<CSeq_feat> cds = GetCDSFromGoodNucProtSet(entry);
    cds->SetLocation().SetPartialStart(true, eExtreme_Biological);
    CRef<CSeq_entry> prot = GetProteinSequenceFromGoodNucProtSet(entry);
    CRef<CSeqdesc> title(new CSeqdesc());
    title->SetTitle("abc, partial [Sebaea microphylla] x");
    prot->SetDescr().Set().push_back(title);
    CRef<CScope> scope(new CScope(*CObjectManager::GetInstance()));;
    CSeq_entry_Handle seh = scope->AddTopLevelSeqEntry(*entry);

    CCleanup cleanup;
    CConstRef<CCleanupChange> changes;

    cleanup.SetScope(scope);
    changes = cleanup.BasicCleanup(*entry);

    ITERATE(CBioseq::TDescr::Tdata, it, prot->GetSeq().GetDescr().Get()) {
        if ((*it)->IsTitle()) {
            BOOST_CHECK_EQUAL((*it)->GetTitle(), "abc [Sebaea microphylla]");
        }
    }
}


BOOST_AUTO_TEST_CASE(Test_FixProteinName)
{
    CRef<CSeq_entry> entry = BuildGoodNucProtSet();
    CRef<CSeq_feat> prot = GetProtFeatFromGoodNucProtSet(entry);
    prot->SetData().SetProt().SetName().front() = "abc [bar][Sebaea microphylla ]";

    CRef<CScope> scope(new CScope(*CObjectManager::GetInstance()));;
    CSeq_entry_Handle seh = scope->AddTopLevelSeqEntry(*entry);

    CCleanup cleanup;
    CConstRef<CCleanupChange> changes;

    cleanup.SetScope(scope);
    changes = cleanup.BasicCleanup(*entry);

    BOOST_CHECK_EQUAL(prot->GetData().GetProt().GetName().front(), "abc [bar][Sebaea microphylla ]");
}


BOOST_AUTO_TEST_CASE(Test_AddPartialToProteinTitle)
{
    CRef<CSeq_entry> entry = BuildGoodNucProtSet();
    CRef<CSeq_entry> prot = GetProteinSequenceFromGoodNucProtSet(entry);
    CRef<CBioSource> src(NULL);
    NON_CONST_ITERATE(CBioseq_set::TDescr::Tdata, d, entry->SetSet().SetDescr().Set()) {
        if ((*d)->IsSource()) {
            src.Reset(&((*d)->SetSource()));
        }
    }
    CRef<CSeqdesc> title(NULL);
    CRef<CSeqdesc> molinfo(NULL);
    NON_CONST_ITERATE(CBioseq::TDescr::Tdata, d, prot->SetSeq().SetDescr().Set()) {
        if ((*d)->IsTitle()) {
            title = *d;
        } else if ((*d)->IsMolinfo()) {
            molinfo = *d;
        }
    }
    if (!title) {
        title.Reset(new CSeqdesc());
        title->SetTitle("abc [Sebaea microphylla]");
        prot->SetSeq().SetDescr().Set().push_back(title);
    }
    if (!molinfo) {
        molinfo.Reset(new CSeqdesc());
        molinfo->SetMolinfo().SetBiomol(CMolInfo::eBiomol_peptide);
        prot->SetSeq().SetDescr().Set().push_back(molinfo);
    }
    entry->Parentize();

    // should be all good to start
    BOOST_CHECK_EQUAL(CCleanup::AddPartialToProteinTitle(prot->SetSeq()), false);
    BOOST_CHECK_EQUAL(title->GetTitle(), "abc [Sebaea microphylla]");

    // take off partial?
    title->SetTitle("abc, partial [Sebaea microphylla]");
    BOOST_CHECK_EQUAL(CCleanup::AddPartialToProteinTitle(prot->SetSeq()), true);
    BOOST_CHECK_EQUAL(title->GetTitle(), "abc [Sebaea microphylla]");

    // fix taxname if oldname
    CRef<COrgMod> oldname(new COrgMod(COrgMod::eSubtype_old_name, "x y"));
    src->SetOrg().SetOrgname().SetMod().push_back(oldname);
    title->SetTitle("abc, partial [x y]");
    BOOST_CHECK_EQUAL(CCleanup::AddPartialToProteinTitle(prot->SetSeq()), true);
    BOOST_CHECK_EQUAL(title->GetTitle(), "abc [Sebaea microphylla]");

    // fix taxname if binomial
    src->SetOrg().SetOrgname().SetName().SetBinomial().SetGenus("yellow");
    src->SetOrg().SetOrgname().SetName().SetBinomial().SetSpecies("banana");
    title->SetTitle("abc, partial [yellow banana]");
    BOOST_CHECK_EQUAL(CCleanup::AddPartialToProteinTitle(prot->SetSeq()), true);
    BOOST_CHECK_EQUAL(title->GetTitle(), "abc [Sebaea microphylla]");

    // fix superkingdom
    CRef<CTaxElement> s1(new CTaxElement());
    s1->SetLevel("superkingdom");
    s1->SetName("Sebaea microphylla");
    s1->SetFixed_level(CTaxElement::eFixed_level_other);
    CRef<CTaxElement> s2(new CTaxElement());
    s2->SetLevel("superkingdom");
    s2->SetName("bicycle");
    s2->SetFixed_level(CTaxElement::eFixed_level_other);
    src->SetOrg().SetOrgname().SetName().SetPartial().Set().push_back(s1);
    src->SetOrg().SetOrgname().SetName().SetPartial().Set().push_back(s2);
    title->SetTitle("abc, partial [Sebaea microphylla][bicycle]");
    BOOST_CHECK_EQUAL(CCleanup::AddPartialToProteinTitle(prot->SetSeq()), true);
    BOOST_CHECK_EQUAL(title->GetTitle(), "abc [Sebaea microphylla][bicycle]");

    // keep superkingdom
    title->SetTitle("abc [Sebaea microphylla][bicycle]");
    BOOST_CHECK_EQUAL(CCleanup::AddPartialToProteinTitle(prot->SetSeq()), false);
    BOOST_CHECK_EQUAL(title->GetTitle(), "abc [Sebaea microphylla][bicycle]");

    // remove bad organelle name
    src->SetOrg().SetOrgname().ResetName();
    title->SetTitle("abc (mitochondrion) [Sebaea microphylla]");
    BOOST_CHECK_EQUAL(CCleanup::AddPartialToProteinTitle(prot->SetSeq()), true);
    BOOST_CHECK_EQUAL(title->GetTitle(), "abc [Sebaea microphylla]");

    // replace bad organelle name
    src->SetGenome(CBioSource::eGenome_apicoplast);
    title->SetTitle("abc (mitochondrion) [Sebaea microphylla]");
    BOOST_CHECK_EQUAL(CCleanup::AddPartialToProteinTitle(prot->SetSeq()), true);
    BOOST_CHECK_EQUAL(title->GetTitle(), "abc (apicoplast) [Sebaea microphylla]");

    // insert good organelle name
    title->SetTitle("abc [Sebaea microphylla]");
    BOOST_CHECK_EQUAL(CCleanup::AddPartialToProteinTitle(prot->SetSeq()), true);
    BOOST_CHECK_EQUAL(title->GetTitle(), "abc (apicoplast) [Sebaea microphylla]");

    // add partial when needed
    molinfo->SetMolinfo().SetCompleteness(CMolInfo::eCompleteness_no_left);
    BOOST_CHECK_EQUAL(CCleanup::AddPartialToProteinTitle(prot->SetSeq()), true);
    BOOST_CHECK_EQUAL(title->GetTitle(), "abc, partial (apicoplast) [Sebaea microphylla]");

    // always use last instance of organelle
    prot->SetSeq().SetAnnot().front()->SetData().SetFtable().front()->SetData().SetProt().SetName().front() = "Chromosome (apicoplast) abc";
    title->SetTitle("Chromosome (apicoplast) abc (apicoplast) [Sebaea microphylla]");
    BOOST_CHECK_EQUAL(CCleanup::AddPartialToProteinTitle(prot->SetSeq()), true);
    BOOST_CHECK_EQUAL(title->GetTitle(), "Chromosome (apicoplast) abc, partial (apicoplast) [Sebaea microphylla]");

}


BOOST_AUTO_TEST_CASE(Test_SQD_4360)
{
    CRef<CSeq_entry> entry = BuildGoodSeq();
    CRef<CSeq_feat> rna = AddMiscFeature(entry);
    rna->SetData().SetRna().SetType(CRNA_ref::eType_miscRNA);
    rna->SetData().SetRna().SetExt().SetGen().SetProduct("stRNA");

    CRef<CScope> scope(new CScope(*CObjectManager::GetInstance()));;
    CSeq_entry_Handle seh = scope->AddTopLevelSeqEntry(*entry);
    entry->Parentize();

    CCleanup cleanup;

    cleanup.SetScope(scope);
    cleanup.BasicCleanup(*entry);

    BOOST_CHECK_EQUAL(rna->GetData().GetRna().GetExt().GetGen().GetProduct(), "stRNA");
    BOOST_CHECK_EQUAL(rna->GetData().GetRna().GetExt().GetGen().IsSetClass(), false);

    scope->RemoveTopLevelSeqEntry(seh);
    entry = BuildGoodSeq();
    rna = AddMiscFeature(entry);
    rna->SetData().SetRna().SetType(CRNA_ref::eType_miscRNA);
    rna->SetData().SetRna().SetExt().SetGen().SetProduct("Vault_RNA fakeproduct");
    seh = scope->AddTopLevelSeqEntry(*entry);
    entry->Parentize();
    cleanup.BasicCleanup(*entry);
    BOOST_CHECK_EQUAL(rna->GetData().GetRna().GetExt().GetGen().GetProduct(), "fakeproduct");
    BOOST_CHECK_EQUAL(rna->GetData().GetRna().GetExt().GetGen().GetClass(), "vault_RNA");
}


void TestSSToDS(const COrg_ref& org, bool is_patent, bool expect_reset)
{
    CRef<CSeq_entry> entry = BuildGoodSeq();
    entry->SetSeq().SetInst().SetStrand(CSeq_inst::eStrand_ss);
    for (auto it = entry->SetSeq().SetDescr().Set().begin();
        it != entry->SetSeq().SetDescr().Set().end();
        it++) {
        if ((*it)->IsSource()) {
            (*it)->SetSource().SetOrg().Assign(org);
        }
    }
    if (is_patent) {
        CRef<CSeq_id> id(new CSeq_id());
        id->SetPatent().SetSeqid(123);
        id->SetPatent().SetCit().SetCountry("USA");
        id->SetPatent().SetCit().SetId().SetNumber("X23");
        entry->SetSeq().SetId().push_back(id);
    }
    CRef<CScope> scope(new CScope(*CObjectManager::GetInstance()));;
    CSeq_entry_Handle seh = scope->AddTopLevelSeqEntry(*entry);
    entry->Parentize();

    CCleanup cleanup;

    cleanup.SetScope(scope);
    cleanup.BasicCleanup(*entry);
    if (expect_reset) {
        BOOST_CHECK_EQUAL(entry->GetSeq().GetInst().IsSetStrand(), false);
    } else {
        BOOST_CHECK_EQUAL(entry->GetSeq().GetInst().IsSetStrand(), true);
        BOOST_CHECK_EQUAL(entry->GetSeq().GetInst().GetStrand(), CSeq_inst::eStrand_ss);
    }
}


BOOST_AUTO_TEST_CASE(Test_SQD_4356)
{
    CRef<COrg_ref> org(new COrg_ref());
    org->SetTaxname("Sebaea microphylla");

    // no lineage? no change
    TestSSToDS(*org, false, false);

    // lineage is set, will reset
    org->SetOrgname().SetLineage("some lineage");
    TestSSToDS(*org, false, true);

    // but not if patent
    TestSSToDS(*org, true, false);

    // but not if synthetic
    org->SetOrgname().SetDiv("SYN");
    TestSSToDS(*org, false, false);

    // or if virus
    org->SetOrgname().ResetDiv();
    org->SetOrgname().SetLineage("virus");
    TestSSToDS(*org, false, false);

}


BOOST_AUTO_TEST_CASE(Test_VR_746)
{
    CRef<CSeq_feat> gene(new CSeq_feat());
    gene->SetLocation().SetInt().SetId().SetLocal().SetStr("foo");
    gene->SetLocation().SetInt().SetFrom(0);
    gene->SetLocation().SetInt().SetTo(5);
    gene->SetData().SetGene().SetLocus("a|b|c");

    CCleanup cleanup;
    CConstRef<CCleanupChange> changes;

    changes = cleanup.BasicCleanup(*gene);

    BOOST_CHECK_EQUAL(gene->GetData().GetGene().GetLocus(), "a");
    BOOST_CHECK_EQUAL(gene->GetData().GetGene().GetSyn().size(), 2);
    BOOST_CHECK_EQUAL(gene->GetData().GetGene().GetSyn().front(), "b");
    BOOST_CHECK_EQUAL(gene->GetData().GetGene().GetSyn().back(), "c");

    gene->SetData().SetGene().SetLocus("a|b|c");
    changes = cleanup.BasicCleanup(*gene);
    BOOST_CHECK_EQUAL(gene->GetData().GetGene().GetLocus(), "a");
    BOOST_CHECK_EQUAL(gene->GetData().GetGene().GetSyn().size(), 2);
    BOOST_CHECK_EQUAL(gene->GetData().GetGene().GetSyn().front(), "b");
    BOOST_CHECK_EQUAL(gene->GetData().GetGene().GetSyn().back(), "c");


}


BOOST_AUTO_TEST_CASE(Test_CleanDBLink)
{
    CRef<CSeq_entry> entry = BuildGoodSeq();

    CRef<CUser_object> user(new CUser_object());
    user->SetObjectType(CUser_object::eObjectType_DBLink);
    CRef<CUser_field> field1(new CUser_field());
    field1->SetLabel().SetStr("BioProject");
    field1->SetData().SetStr("A value");
    user->SetData().push_back(field1);

    CRef<CSeqdesc> desc(new CSeqdesc());
    desc->SetUser().Assign(*user);
    entry->SetSeq().SetDescr().Set().push_back(desc);

    CRef<CScope> scope(new CScope(*CObjectManager::GetInstance()));;
    CSeq_entry_Handle seh = scope->AddTopLevelSeqEntry(*entry);
    entry->Parentize();

    CCleanup cleanup;
    CConstRef<CCleanupChange> changes;

    cleanup.SetScope(scope);
    changes = cleanup.BasicCleanup(*entry);

    BOOST_CHECK_EQUAL(desc->GetUser().GetData().size(), 1);
    BOOST_CHECK_EQUAL(desc->GetUser().GetData().front()->GetData().IsStrs(), true);
    BOOST_CHECK_EQUAL(desc->GetUser().GetData().front()->GetData().GetStrs().size(), 1);
    BOOST_CHECK_EQUAL(desc->GetUser().GetData().front()->GetData().GetStrs().front(), "A value");
}


void TestExceptionTextFix(const string& before, const string& after)
{
    CRef<CSeq_entry> entry = BuildGoodSeq();
    CRef<CSeq_feat> m = AddMiscFeature(entry);
    m->SetExcept(true);
    m->SetExcept_text("trans_splicing");

    CRef<CScope> scope(new CScope(*CObjectManager::GetInstance()));;
    CSeq_entry_Handle seh = scope->AddTopLevelSeqEntry(*entry);
    entry->Parentize();

    CCleanup cleanup;
    CConstRef<CCleanupChange> changes;

    cleanup.SetScope(scope);
    changes = cleanup.BasicCleanup(*entry);

    CFeat_CI f(seh);
    BOOST_CHECK_EQUAL(f->GetExcept_text(), "trans-splicing");

}


BOOST_AUTO_TEST_CASE(Test_SQD_4464)
{
    TestExceptionTextFix("trans_splicing", "trans-splicing");
    TestExceptionTextFix("trans splicing", "trans-splicing");
}


void x_CheckIntervalLoc(const CSeq_interval& lint, TSeqPos start, TSeqPos stop, bool is_minus = false)
{
    BOOST_CHECK_EQUAL(lint.GetId().GetLocal().GetStr(), "nuc1");
    BOOST_CHECK_EQUAL(lint.GetFrom(), start);
    BOOST_CHECK_EQUAL(lint.GetTo(), stop);
    if (is_minus) {
        BOOST_CHECK_EQUAL(lint.GetStrand(), eNa_strand_minus);
    } else {
        BOOST_CHECK_EQUAL(lint.IsSetStrand(), false);
    }
}


void x_CheckIntervalLoc(const CSeq_loc& loc, TSeqPos start, TSeqPos stop, bool is_minus = false)
{
    x_CheckIntervalLoc(loc.GetInt(), start, stop, is_minus);
}


void x_CheckCodeBreakFrame(const CSeq_feat& cds, TSeqPos start, TSeqPos stop)
{
    CRef<CCode_break> cb(new CCode_break());
    bool is_minus = (cds.GetLocation().IsSetStrand() && (cds.GetLocation().GetStrand() == eNa_strand_minus));
    CCleanup::SetCodeBreakLocation(*cb, 1, cds);
    x_CheckIntervalLoc(cb->GetLoc(), start, stop, is_minus);
    CCleanup::SetCodeBreakLocation(*cb, 2, cds);
    x_CheckIntervalLoc(cb->GetLoc(),
                       is_minus ? start - 3 : start + 3,
                       is_minus ? stop - 3 : stop + 3,
                       is_minus);

    CCleanup::SetCodeBreakLocation(*cb, 3, cds);
    x_CheckIntervalLoc(cb->GetLoc(),
        is_minus ? start - 6 : start + 6,
        is_minus ? stop - 6 : stop + 6,
        is_minus);

    CCleanup::SetCodeBreakLocation(*cb, 4, cds);
    x_CheckIntervalLoc(cb->GetLoc(),
        is_minus ? 0 : start + 9,
        is_minus ? stop - 9 : 11,
        is_minus);

}


BOOST_AUTO_TEST_CASE(Test_SetCodeBreakLocation)
{
    CRef<CSeq_feat> cds(new CSeq_feat());
    cds->SetLocation().SetInt().SetId().SetLocal().SetStr("nuc1");
    cds->SetLocation().SetInt().SetFrom(0);
    cds->SetLocation().SetInt().SetTo(11);

    x_CheckCodeBreakFrame(*cds, 0, 2);

    cds->SetData().SetCdregion().SetFrame(CCdregion::eFrame_not_set);
    x_CheckCodeBreakFrame(*cds, 0, 2);

    cds->SetData().SetCdregion().SetFrame(CCdregion::eFrame_one);
    x_CheckCodeBreakFrame(*cds, 0, 2);

    cds->SetData().SetCdregion().SetFrame(CCdregion::eFrame_two);
    x_CheckCodeBreakFrame(*cds, 1, 3);

    cds->SetData().SetCdregion().SetFrame(CCdregion::eFrame_three);
    x_CheckCodeBreakFrame(*cds, 2, 4);

    cds->SetLocation().SetInt().SetStrand(eNa_strand_minus);
    cds->SetData().SetCdregion().ResetFrame();
    x_CheckCodeBreakFrame(*cds, 9, 11);

    cds->SetData().SetCdregion().SetFrame(CCdregion::eFrame_not_set);
    x_CheckCodeBreakFrame(*cds, 9, 11);

    cds->SetData().SetCdregion().SetFrame(CCdregion::eFrame_one);
    x_CheckCodeBreakFrame(*cds, 9, 11);

    cds->SetData().SetCdregion().SetFrame(CCdregion::eFrame_two);
    x_CheckCodeBreakFrame(*cds, 8, 10);

    cds->SetData().SetCdregion().SetFrame(CCdregion::eFrame_three);
    x_CheckCodeBreakFrame(*cds, 7, 9);

    cds->SetData().SetCdregion().ResetFrame();
    CRef<CSeq_loc> l1(new CSeq_loc());
    CRef<CSeq_loc> l2(new CSeq_loc());
    CRef<CSeq_loc> l3(new CSeq_loc());
    l1->SetInt().SetId().SetLocal().SetStr("nuc1");
    l1->SetInt().SetFrom(0);
    l1->SetInt().SetTo(1);

    l2->SetInt().SetId().SetLocal().SetStr("nuc1");
    l2->SetInt().SetFrom(5);
    l2->SetInt().SetTo(13);

    l3->SetPnt().SetId().SetLocal().SetStr("nuc1");
    l3->SetPnt().SetPoint(16);

    cds->SetLocation().SetMix().Set().push_back(l1);
    cds->SetLocation().SetMix().Set().push_back(l2);
    cds->SetLocation().SetMix().Set().push_back(l3);

    CRef<CCode_break> cb(new CCode_break());
    CCleanup::SetCodeBreakLocation(*cb, 1, *cds);
    x_CheckIntervalLoc(*cb->GetLoc().GetPacked_int().Get().front(), 0, 1);
    x_CheckIntervalLoc(*cb->GetLoc().GetPacked_int().Get().back(), 5, 5);
    CCleanup::SetCodeBreakLocation(*cb, 2, *cds);
    x_CheckIntervalLoc(cb->GetLoc(), 6, 8);

    cds->SetData().SetCdregion().SetFrame(CCdregion::eFrame_two);
    CCleanup::SetCodeBreakLocation(*cb, 1, *cds);
    x_CheckIntervalLoc(*cb->GetLoc().GetPacked_int().Get().front(), 1, 1);
    x_CheckIntervalLoc(*cb->GetLoc().GetPacked_int().Get().back(), 5, 6);
    CCleanup::SetCodeBreakLocation(*cb, 2, *cds);
    x_CheckIntervalLoc(cb->GetLoc(), 7, 9);

    cds->SetData().SetCdregion().SetFrame(CCdregion::eFrame_three);
    CCleanup::SetCodeBreakLocation(*cb, 1, *cds);
    x_CheckIntervalLoc(cb->GetLoc(), 5, 7);
    CCleanup::SetCodeBreakLocation(*cb, 2, *cds);
    x_CheckIntervalLoc(cb->GetLoc(), 8, 10);

    l1->SetInt().SetStrand(eNa_strand_minus);
    l2->SetInt().SetStrand(eNa_strand_minus);
    l3->SetPnt().SetStrand(eNa_strand_minus);
    cds->ResetLocation();
    cds->SetLocation().SetMix().Set().push_back(l3);
    cds->SetLocation().SetMix().Set().push_back(l2);
    cds->SetLocation().SetMix().Set().push_back(l1);
    cds->SetData().SetCdregion().ResetFrame();

    CCleanup::SetCodeBreakLocation(*cb, 1, *cds);
    x_CheckIntervalLoc(*cb->GetLoc().GetPacked_int().Get().front(), 16, 16, true);
    x_CheckIntervalLoc(*cb->GetLoc().GetPacked_int().Get().back(), 12, 13, true);
    CCleanup::SetCodeBreakLocation(*cb, 2, *cds);
    x_CheckIntervalLoc(cb->GetLoc(), 9, 11, true);

    cds->SetData().SetCdregion().SetFrame(CCdregion::eFrame_three);
    CCleanup::SetCodeBreakLocation(*cb, 1, *cds);
    x_CheckIntervalLoc(cb->GetLoc(), 10, 12, true);
    CCleanup::SetCodeBreakLocation(*cb, 2, *cds);
    x_CheckIntervalLoc(cb->GetLoc(), 7, 9, true);

}


void TestGetCodeBreakForLocationByFrame(CCdregion::EFrame frame)
{
    CRef<CSeq_feat> cds(new CSeq_feat());
    cds->SetLocation().SetInt().SetId().SetLocal().SetStr("nuc1");
    cds->SetLocation().SetInt().SetFrom(0);
    cds->SetLocation().SetInt().SetTo(11);
    cds->SetData().SetCdregion().SetFrame(frame);

    for (int i = 1; i < 5; i++) {
        CRef<CCode_break> cb(new CCode_break());
        cb->SetAa().SetNcbieaa('A' + i);
        CCleanup::SetCodeBreakLocation(*cb, i, *cds);
        cds->SetData().SetCdregion().SetCode_break().push_back(cb);
        BOOST_CHECK_EQUAL(CCleanup::GetCodeBreakForLocation(i, *cds).GetPointer(), cb.GetPointer());
    }
    BOOST_CHECK_EQUAL(CCleanup::GetCodeBreakForLocation(1, *cds).GetPointer(),
        cds->GetData().GetCdregion().GetCode_break().front().GetPointer());
    BOOST_CHECK_EQUAL(CCleanup::GetCodeBreakForLocation(4, *cds).GetPointer(),
        cds->GetData().GetCdregion().GetCode_break().back().GetPointer());

}


BOOST_AUTO_TEST_CASE(Test_GetCodeBreakForLocation)
{
    TestGetCodeBreakForLocationByFrame(CCdregion::eFrame_not_set);
    TestGetCodeBreakForLocationByFrame(CCdregion::eFrame_one);
    TestGetCodeBreakForLocationByFrame(CCdregion::eFrame_two);
    TestGetCodeBreakForLocationByFrame(CCdregion::eFrame_three);
}


void x_VerifyFixedRNAEditingCodingRegion(const CSeq_feat& cds, const string& except_text, size_t num_code_break)
{
    BOOST_CHECK_EQUAL(cds.IsSetExcept(), true);
    BOOST_CHECK_EQUAL(cds.GetExcept_text(), except_text);
//    BOOST_CHECK_EQUAL(cds.GetData().GetCdregion().GetCode_break().size(), num_code_break);
//    CConstRef<CCode_break> cb = cds.GetData().GetCdregion().GetCode_break().front();
//    x_CheckIntervalLoc(cb->GetLoc(), 0, 2);
//    BOOST_CHECK_EQUAL(cb->GetAa().GetNcbieaa(), 'M');
}


BOOST_AUTO_TEST_CASE(Test_FixRNAEditingCodingRegion)
{
    CRef<CSeq_feat> cds(new CSeq_feat());
    // false because data not set
    BOOST_CHECK_EQUAL(CCleanup::FixRNAEditingCodingRegion(*cds), false);
    cds->SetData().SetCdregion();

    cds->SetLocation().SetInt().SetId().SetLocal().SetStr("nuc1");
    cds->SetLocation().SetInt().SetFrom(0);
    cds->SetLocation().SetInt().SetTo(10);
    cds->SetLocation().SetPartialStart(true, eExtreme_Biological);
    // false because 5' partial
    BOOST_CHECK_EQUAL(CCleanup::FixRNAEditingCodingRegion(*cds), false);

    cds->SetLocation().SetPartialStart(false, eExtreme_Biological);
    BOOST_CHECK_EQUAL(CCleanup::FixRNAEditingCodingRegion(*cds), true);
    x_VerifyFixedRNAEditingCodingRegion(*cds, "RNA editing", 1);

    // no change because already set
    BOOST_CHECK_EQUAL(CCleanup::FixRNAEditingCodingRegion(*cds), false);
    x_VerifyFixedRNAEditingCodingRegion(*cds, "RNA editing", 1);

//    cds->SetData().SetCdregion().ResetCode_break();
//    BOOST_CHECK_EQUAL(CCleanup::FixRNAEditingCodingRegion(*cds), true);
//    x_VerifyFixedRNAEditingCodingRegion(*cds, "RNA editing", 1);

    // append to exception text
    cds->SetExcept_text("foo");
    BOOST_CHECK_EQUAL(CCleanup::FixRNAEditingCodingRegion(*cds), true);
    x_VerifyFixedRNAEditingCodingRegion(*cds, "foo; RNA editing", 1);

    // prepend translation exception
//    cds->SetData().SetCdregion().ResetCode_break();
//    CRef<CCode_break> cb(new CCode_break());
//    CCleanup::SetCodeBreakLocation(*cb, 2, *cds);
//    cds->SetData().SetCdregion().SetCode_break().push_back(cb);
//    BOOST_CHECK_EQUAL(CCleanup::FixRNAEditingCodingRegion(*cds), true);
//    x_VerifyFixedRNAEditingCodingRegion(*cds, "foo; RNA editing", 2);

}


void CheckTaxnameChange(const string& orig, const string& fixed)
{
    CRef<CSeq_entry> entry = BuildGoodSeq();
    SetTaxon(entry, 0);
    SetTaxname(entry, orig);

    CRef<CScope> scope(new CScope(*CObjectManager::GetInstance()));;
    CSeq_entry_Handle seh = scope->AddTopLevelSeqEntry(*entry);
    entry->Parentize();

    CCleanup cleanup;
    CConstRef<CCleanupChange> changes;

    cleanup.SetScope(scope);
    changes = cleanup.BasicCleanup(*entry);
    bool found_taxname_change = false;
    for (auto it : changes->GetAllChanges()) {
        if (it == CCleanupChange::eChangeTaxname) {
            found_taxname_change = true;
        }
    }
    BOOST_CHECK_EQUAL(found_taxname_change, true);

    for (auto it : entry->GetSeq().GetDescr().Get()) {
        if (it->IsSource()) {
            BOOST_CHECK_EQUAL(it->GetSource().GetOrg().GetTaxname(), fixed);
        }
    }

}


BOOST_AUTO_TEST_CASE(Test_GB_7569)
{
#if 0
    // removed by request
    CheckTaxnameChange("abc ssp. x", "abc subsp. x");
    CheckTaxnameChange("Medicago sativa subspecies fake", "Medicago sativa subsp. fake");
#endif
}


BOOST_AUTO_TEST_CASE(Test_ReadLocFromText)
{
    CRef<CSeq_entry> entry = BuildGoodSeq();
    CRef<CSeq_feat> trna = AddMiscFeature(entry);
    trna->SetLocation().SetInt().SetTo(entry->GetSeq().GetLength() - 1);
    trna->SetLocation().SetInt().SetStrand(eNa_strand_minus);
    trna->SetData().SetRna().SetType(CRNA_ref::eType_tRNA);
    trna->SetQual().push_back(CRef<CGb_qual>(new CGb_qual("anticodon", "(pos:5-3,aa:His)")));

    CRef<CScope> scope(new CScope(*CObjectManager::GetInstance()));;
    CSeq_entry_Handle seh = scope->AddTopLevelSeqEntry(*entry);
    entry->Parentize();

    CCleanup cleanup;
    CConstRef<CCleanupChange> changes;

    cleanup.SetScope(scope);
    changes = cleanup.BasicCleanup(*entry);

    BOOST_CHECK_EQUAL(trna->IsSetQual(), false);
    BOOST_CHECK_EQUAL(trna->GetData().GetRna().GetExt().GetTRNA().IsSetAnticodon(), true);
    const CSeq_loc& loc = trna->GetData().GetRna().GetExt().GetTRNA().GetAnticodon();
    BOOST_CHECK_EQUAL(loc.GetStart(eExtreme_Biological), 4);
    BOOST_CHECK_EQUAL(loc.GetStop(eExtreme_Biological), 2);
    BOOST_CHECK_EQUAL(loc.GetStrand(), eNa_strand_minus);
}


void CheckFields(const CUser_field& f1, const CUser_field& f2)
{
    BOOST_CHECK_EQUAL(f1.IsSetData(), f2.IsSetData());
    BOOST_CHECK_EQUAL(f1.GetData().Which(), f2.GetData().Which());
    if (f1.GetData().IsStrs()) {
        auto it1 = f1.GetData().GetStrs().begin();
        auto it2 = f2.GetData().GetStrs().begin();
        while (it1 != f1.GetData().GetStrs().end() &&
               it2 != f2.GetData().GetStrs().end()) {
            BOOST_CHECK_EQUAL(*it1, *it2);
            it1++;
            it2++;
        }
        BOOST_CHECK(it1 == f1.GetData().GetStrs().end());
        BOOST_CHECK(it2 == f2.GetData().GetStrs().end());
    } else {
        BOOST_CHECK(f1.Equals(f2));
    }
}


void CheckCleanupAndCleanupOfUserObject(const CUser_object& obj)
{
    CRef<CSeq_entry> entry = BuildGoodSeq();
    CRef<CSeqdesc> ud(new CSeqdesc());
    ud->SetUser().Assign(obj);
    entry->SetSeq().SetDescr().Set().push_back(ud);

    CCleanup cleanup;
    CConstRef<CCleanupChange> changes;

    CRef<CScope> scope(new CScope(*CObjectManager::GetInstance()));;
    CSeq_entry_Handle seh = scope->AddTopLevelSeqEntry(*entry);
    entry->Parentize();
    cleanup.SetScope(scope);
    changes = cleanup.BasicCleanup(*entry);

    CRef<CUser_object> direct_obj(new CUser_object());
    direct_obj->Assign(obj);
    CCleanup::CleanupUserObject(*direct_obj);

    if (obj.IsSetType()) {
        BOOST_CHECK_EQUAL(direct_obj->GetType().GetStr(), ud->GetUser().GetType().GetStr());
    }

    if (obj.IsSetData()) {
        auto s1 = direct_obj->GetData().begin();
        auto s2 = ud->GetUser().GetData().begin();
        while (s1 != direct_obj->GetData().end() && s2 != ud->GetUser().GetData().end()) {
            CheckFields(**s1, **s2);
            s1++;
            s2++;
        }
        BOOST_CHECK(s1 == direct_obj->GetData().end());
        BOOST_CHECK(s2 == ud->GetUser().GetData().end());
    }
}


BOOST_AUTO_TEST_CASE(Test_UserObject)
{
    CRef<CUser_object> obj(new CUser_object());

    BOOST_CHECK_EQUAL(CCleanup::CleanupUserObject(*obj), false);

    obj->SetType().SetStr(" ; abc -- ");

    CheckCleanupAndCleanupOfUserObject(*obj);
    BOOST_CHECK_EQUAL(CCleanup::CleanupUserObject(*obj), true);
    BOOST_CHECK_EQUAL(obj->GetType().GetStr(), "abc --");

    CRef<CUser_field> a(new CUser_field());
    obj->SetData().push_back(a);
    CheckCleanupAndCleanupOfUserObject(*obj);
    BOOST_CHECK_EQUAL(CCleanup::CleanupUserObject(*obj), false);

    a->SetLabel().SetStr(" ; a  b.");
    a->SetData().SetStr(",, z  x ..");
    CheckCleanupAndCleanupOfUserObject(*obj);
    BOOST_CHECK_EQUAL(CCleanup::CleanupUserObject(*obj), true);
    BOOST_CHECK_EQUAL(a->GetLabel().GetStr(), "a  b.");
    BOOST_CHECK_EQUAL(a->GetData().GetStr(), "z x ..");

}


CRef<CUser_field> MakeGoField(const string& n1, const string& n2, const string& val)
{
    CRef<CUser_field> go1(new CUser_field());
    go1->SetLabel().SetStr(n1);
    CRef<CUser_field> t1(new CUser_field());
    t1->SetLabel().SetStr(n2);
    t1->SetData().SetStr(val);
    CRef<CUser_field> if1(new CUser_field());
    if1->SetData().SetFields().push_back(t1);
    go1->SetData().SetFields().push_back(if1);
    return go1;
}

void CheckGoFieldData(const CUser_field& f, const string& val)
{
    BOOST_CHECK_EQUAL(f.GetData().GetFields().front()->GetData().GetFields().front()->GetData().GetStr(),
        val);
}


BOOST_AUTO_TEST_CASE(Test_GeneOntology)
{
    CRef<CUser_object> obj(new CUser_object());
    obj->SetType().SetStr("GeneOntology");
    obj->SetData().push_back(MakeGoField("Component", "go id", "GO:xxx"));
    obj->SetData().push_back(MakeGoField("Process", "go ref", "GO_REF:zzz"));

    CheckCleanupAndCleanupOfUserObject(*obj);
    BOOST_CHECK_EQUAL(CCleanup::CleanupUserObject(*obj), true);
    CheckGoFieldData(*(obj->GetData().front()), "xxx");
    CheckGoFieldData(*(obj->GetData().back()), "zzz");

}


CRef<CUser_field> MakeStructuredCommentField(const string& n, const string& v)
{
    CRef<CUser_field> f(new CUser_field());
    f->SetLabel().SetStr(n);
    f->SetData().SetStr(v);
    return f;
}


void CheckStructuredCommentField(const CUser_field& f, const string& n, const string& v)
{
    BOOST_CHECK_EQUAL(f.GetLabel().GetStr(), n);
    BOOST_CHECK_EQUAL(f.GetData().GetStr(), v);
}


BOOST_AUTO_TEST_CASE(Test_StructuredComment)
{
    CRef<CUser_object> obj(new CUser_object());
    obj->SetType().SetStr("StructuredComment");
    obj->SetData().push_back(MakeStructuredCommentField("StructuredCommentPrefix", "Z"));
    obj->SetData().push_back(MakeStructuredCommentField("Tentative Name", "  "));
    obj->SetData().push_back(MakeStructuredCommentField("StructuredCommentSuffix", "Z"));
    obj->SetData().push_back(MakeStructuredCommentField("Tentative Name", "  "));

    CheckCleanupAndCleanupOfUserObject(*obj);
    BOOST_CHECK_EQUAL(CCleanup::CleanupUserObject(*obj), true);
    CheckStructuredCommentField(*(obj->GetData().front()), "StructuredCommentPrefix", "##Z-START##");
    CheckStructuredCommentField(*(obj->GetData().back()), "StructuredCommentSuffix", "##Z-END##");
    BOOST_CHECK_EQUAL(obj->GetData().size(), 2);
}


BOOST_AUTO_TEST_CASE(Test_ibol)
{
    CRef<CUser_object> obj(new CUser_object());
    obj->SetType().SetStr("StructuredComment");
    obj->SetData().push_back(MakeStructuredCommentField("StructuredCommentPrefix", "International Barcode of Life (iBOL)Data"));
    obj->SetData().push_back(MakeStructuredCommentField("Tentative Name", " a "));
    obj->SetData().push_back(MakeStructuredCommentField("Barcode Index Number", " b "));

    CheckCleanupAndCleanupOfUserObject(*obj);
    BOOST_CHECK_EQUAL(CCleanup::CleanupUserObject(*obj), true);
    CheckStructuredCommentField(*(obj->GetData().front()), "StructuredCommentPrefix", "##International Barcode of Life (iBOL)Data-START##");
    CheckStructuredCommentField(*(obj->GetData().back()), "Tentative Name", "a");
    BOOST_CHECK_EQUAL(obj->GetData().size(), 3);

}


void TestGenomeAssemblyField(const string& n, const string& v, const string& v_e)
{
    CRef<CUser_object> obj(new CUser_object());
    obj->SetType().SetStr("StructuredComment");
    obj->SetData().push_back(MakeStructuredCommentField("StructuredCommentPrefix", "Genome-Assembly-Data"));
    obj->SetData().push_back(MakeStructuredCommentField(n, v));

    CheckCleanupAndCleanupOfUserObject(*obj);
    BOOST_CHECK_EQUAL(CCleanup::CleanupUserObject(*obj), true);
    CheckStructuredCommentField(*(obj->GetData().front()), "StructuredCommentPrefix", "##Genome-Assembly-Data-START##");
    CheckStructuredCommentField(*(obj->GetData().back()), n, v_e);
    BOOST_CHECK_EQUAL(obj->GetData().size(), 2);
}


BOOST_AUTO_TEST_CASE(Test_GenomeAssembly)
{
    TestGenomeAssemblyField("Finishing Goal", "Improved High Quality Draft", "Improved High-Quality Draft");
    TestGenomeAssemblyField("Current Finishing Status", "Non-contiguous Finished", "Noncontiguous Finished");
    TestGenomeAssemblyField("Assembly Date", "February 8, 2020", "08-FEB-2020");
}


BOOST_AUTO_TEST_CASE(Test_DBLink)
{
    CRef<CUser_object> obj(new CUser_object());
    obj->SetType().SetStr("DBLink");
    obj->SetData().push_back(MakeStructuredCommentField("BioProject", "Z"));

    CheckCleanupAndCleanupOfUserObject(*obj);
    BOOST_CHECK_EQUAL(CCleanup::CleanupUserObject(*obj), true);
    const CUser_field& f = *(obj->GetData().front());
    BOOST_CHECK_EQUAL(f.GetData().Which(), CUser_field::TData::e_Strs);
    BOOST_CHECK_EQUAL(f.GetData().GetStrs().front(), "Z");
}


BOOST_AUTO_TEST_CASE(Test_x_AddNumToUserField)
{
    CRef<CUser_object> obj(new CUser_object());
    CRef<CUser_field> f(new CUser_field());
    f->SetData().SetStrs().push_back("x");
    obj->SetData().push_back(f);
    BOOST_CHECK_EQUAL(f->IsSetNum(), false);
    BOOST_CHECK_EQUAL(CCleanup::CleanupUserObject(*obj), true);
    BOOST_CHECK_EQUAL(f->GetNum(), 1);
}


BOOST_AUTO_TEST_CASE(Test_SQD_4508)
{
    CRef<CSeq_entry> entry = BuildGoodNucProtSet();
    CRef<CSeq_feat> prot = GetProtFeatFromGoodNucProtSet(entry);
    prot->SetData().SetProt().SetName().front() = "a\tb";

    CRef<CScope> scope(new CScope(*CObjectManager::GetInstance()));;
    CSeq_entry_Handle seh = scope->AddTopLevelSeqEntry(*entry);

    CCleanup cleanup;
    CConstRef<CCleanupChange> changes;

    cleanup.SetScope(scope);
    changes = cleanup.BasicCleanup(*entry);

    BOOST_CHECK_EQUAL(prot->GetData().GetProt().GetName().front(), "a b");

}


BOOST_AUTO_TEST_CASE(Test_SeqFeatCDSGBQualBC)
{
    CRef<CSeq_entry> entry = BuildGoodNucProtSet();
    CRef<CSeq_feat> cds = GetCDSFromGoodNucProtSet(entry);
    cds->SetQual().push_back(CRef<CGb_qual>(new CGb_qual("prot_note", "xyz")));

    CRef<CScope> scope(new CScope(*CObjectManager::GetInstance()));;
    CSeq_entry_Handle seh = scope->AddTopLevelSeqEntry(*entry);

    CCleanup cleanup;
    CConstRef<CCleanupChange> changes;

    cleanup.SetScope(scope);
    changes = cleanup.BasicCleanup(*entry);

    CRef<CSeq_feat> prot = GetProtFeatFromGoodNucProtSet(entry);
    BOOST_CHECK_EQUAL(prot->GetComment(), "xyz");
    BOOST_CHECK_EQUAL(cds->IsSetQual(), false);

}


BOOST_AUTO_TEST_CASE(Test_CommentRedundantWithEc)
{
    CRef<CSeq_entry> entry = BuildGoodNucProtSet();
    CRef<CSeq_feat> cds = GetCDSFromGoodNucProtSet(entry);
    cds->SetComment("1.2.3.4");
    CRef<CSeq_feat> prot = GetProtFeatFromGoodNucProtSet(entry);
    prot->SetData().SetProt().SetEc().push_back("1.2.3.4");

    CRef<CScope> scope(new CScope(*CObjectManager::GetInstance()));;
    CSeq_entry_Handle seh = scope->AddTopLevelSeqEntry(*entry);

    CCleanup cleanup;
    CConstRef<CCleanupChange> changes;

    cleanup.SetScope(scope);
    changes = cleanup.BasicCleanup(*entry);

    BOOST_CHECK_EQUAL(cds->IsSetComment(), false);
}


BOOST_AUTO_TEST_CASE(Test_MoveXrefToProt)
{
    CRef<CSeq_entry> entry = BuildGoodNucProtSet();
    CRef<CSeq_feat> cds = GetCDSFromGoodNucProtSet(entry);
    CRef<CSeq_feat> prot = GetProtFeatFromGoodNucProtSet(entry);
    CRef<CSeqFeatXref> xr(new CSeqFeatXref());
    xr->SetData().SetProt().Assign(prot->GetData().GetProt());
    cds->SetXref().push_back(xr);

    prot->SetData().SetProt().Reset();

    CRef<CScope> scope(new CScope(*CObjectManager::GetInstance()));;
    CSeq_entry_Handle seh = scope->AddTopLevelSeqEntry(*entry);

    CCleanup cleanup;
    CConstRef<CCleanupChange> changes;

    cleanup.SetScope(scope);
    changes = cleanup.BasicCleanup(*entry);

    BOOST_CHECK_EQUAL(cds->IsSetXref(), false);
    BOOST_CHECK_EQUAL(prot->GetData().GetProt().GetName().front(), "fake protein name");
}


BOOST_AUTO_TEST_CASE(Test_SQD_4516)
{
    CRef<CSeq_entry> entry = BuildGoodSeq();
    SetSubSource(entry, CSubSource::eSubtype_country, "USA:b:c");

    CRef<CScope> scope(new CScope(*CObjectManager::GetInstance()));;
    CSeq_entry_Handle seh = scope->AddTopLevelSeqEntry(*entry);
    CCleanup cleanup;
    CConstRef<CCleanupChange> changes;

    cleanup.SetScope(scope);
    changes = cleanup.BasicCleanup(*entry);

    CSeqdesc_CI src(seh, CSeqdesc::e_Source);
    BOOST_CHECK_EQUAL(src->GetSource().GetSubtype().back()->GetName(), "USA:b,c");
}


void CheckAuthNameSingleInitialFix(const string& first, const string& initials, const string& new_first, const string& new_init)
{
    CRef<CSeq_entry> entry = BuildGoodSeq();
    CRef<CAuthor> author(new CAuthor());
    author->SetName().SetName().SetLast("Xyz");
    author->SetName().SetName().SetInitials(initials);
    if (!NStr::IsBlank(first)) {
        author->SetName().SetName().SetFirst(first);
    }

    CRef<CPub> pub(new CPub());
    pub->SetGen().SetCit("unpublished title");
    pub->SetAuthors().SetNames().SetStd().push_back(author);
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

    const CName_std& name_std = result.GetNames().GetStd().front()->GetName().GetName();
    if (NStr::IsBlank(new_first)) {
        BOOST_CHECK_EQUAL(name_std.IsSetFirst(), false);
    }
    else {
        BOOST_CHECK_EQUAL(name_std.GetFirst(), new_first);
    }
    BOOST_CHECK_EQUAL(name_std.GetInitials(), new_init);
}


BOOST_AUTO_TEST_CASE(Test_SQD_4536)
{
    CheckAuthNameSingleInitialFix("", "A", "A", "A.");
    CheckAuthNameSingleInitialFix("", "A.", "A", "A.");

    CheckAuthNameSingleInitialFix("B", "A", "B", "B.A.");
    CheckAuthNameSingleInitialFix("B", "A.", "B", "B.A.");

    // now fixing even if more than one initial
    CheckAuthNameSingleInitialFix("", "M.E.", "M", "M.E.");

}


BOOST_AUTO_TEST_CASE(Test_SQD_4565)
{
    CRef<CSeq_entry> entry = BuildGoodNucProtSet();
    CRef<CSeq_feat> prot = GetProtFeatFromGoodNucProtSet(entry);

    prot->SetData().SetProt().SetName().push_back("A");
    prot->SetData().SetProt().SetName().push_back("A");
    prot->SetData().SetProt().SetName().push_back("B");
    prot->SetData().SetProt().SetName().push_back("C");
    prot->SetData().SetProt().SetName().push_back("B");

    CRef<CScope> scope(new CScope(*CObjectManager::GetInstance()));;
    CSeq_entry_Handle seh = scope->AddTopLevelSeqEntry(*entry);
    entry->Parentize();

    CCleanup cleanup;
    CConstRef<CCleanupChange> changes;

    cleanup.SetScope(scope);
    changes = cleanup.BasicCleanup(*entry);

    BOOST_CHECK_EQUAL(prot->SetData().SetProt().GetName().size(), 4);

}


void TestOneAsn2gnbkCompressSpaces(const string& input, const string& output)
{
    // In order to test Asn2gnbkCompressSpaces, we will put the string in a User-field,
    // since this function is not exposed directly

    CRef<CSeq_entry> entry = BuildGoodSeq();
    CRef<CUser_field> f(new CUser_field());
    f->SetData().SetStr(input);
    CRef<CSeqdesc> desc(new CSeqdesc());
    desc->SetUser().SetData().push_back(f);
    entry->SetSeq().SetDescr().Set().push_back(desc);

    CRef<CScope> scope(new CScope(*CObjectManager::GetInstance()));;
    CSeq_entry_Handle seh = scope->AddTopLevelSeqEntry(*entry);
    entry->Parentize();

    CCleanup cleanup;
    CConstRef<CCleanupChange> changes;

    cleanup.SetScope(scope);
    changes = cleanup.BasicCleanup(*entry);

    BOOST_CHECK_EQUAL(f->GetData().GetStr(), output);
}


BOOST_AUTO_TEST_CASE(Test_Asn2gnbkCompressSpaces)
{
    TestOneAsn2gnbkCompressSpaces("abc", "abc"); // normal, no change
    TestOneAsn2gnbkCompressSpaces(" abc ", "abc"); // trim leaading and trailing spaces
    TestOneAsn2gnbkCompressSpaces(" a  bc ", "a bc"); // two spaces become one
    TestOneAsn2gnbkCompressSpaces("a,,b,,c", "a, b, c"); // multiple commas become one
    TestOneAsn2gnbkCompressSpaces("a;;b;;;c", "a;b;c"); // multiple semicolons become one
    TestOneAsn2gnbkCompressSpaces("a ,b , c", "a, b, c"); // remove spaces before commas
    TestOneAsn2gnbkCompressSpaces("a ( b ) c", "a (b) c"); // parentheses
    TestOneAsn2gnbkCompressSpaces("a ; b ; c", "a; b; c"); // spaces before semicolons
}


BOOST_AUTO_TEST_CASE(Test_GeneDbToFeatDb)
{
    CRef<CSeq_entry> entry = BuildGoodSeq();
    CRef<CSeq_feat> gene = AddMiscFeature(entry);
    gene->SetData().SetGene().SetLocus("X");
    CRef<CDbtag> db(new CDbtag());
    db->SetDb("A");
    db->SetTag().SetStr("B");
    gene->SetData().SetGene().SetDb().push_back(db);

    CRef<CScope> scope(new CScope(*CObjectManager::GetInstance()));;
    CSeq_entry_Handle seh = scope->AddTopLevelSeqEntry(*entry);
    entry->Parentize();

    CCleanup cleanup;
    CConstRef<CCleanupChange> changes;

    cleanup.SetScope(scope);
    changes = cleanup.BasicCleanup(*entry);

    BOOST_CHECK_EQUAL(gene->GetData().GetGene().IsSetDb(), false);
    BOOST_CHECK_EQUAL(gene->GetDbxref().size(), 1);
}


BOOST_AUTO_TEST_CASE(Test_MoveXrefGeneDbToFeatDb)
{
    CRef<CSeq_entry> entry = BuildGoodSeq();
    CRef<CSeq_feat> gene = AddMiscFeature(entry);
    gene->SetData().SetGene().SetLocus("X");

    CRef<CSeqFeatXref> x1(new CSeqFeatXref());
    x1->SetId().SetLocal().SetId(1);
    gene->SetXref().push_back(x1);

    CRef<CSeqFeatXref> x2(new CSeqFeatXref());
    CRef<CDbtag> db1(new CDbtag());
    db1->SetDb("A");
    db1->SetTag().SetStr("B");
    x2->SetData().SetGene().SetDb().push_back(db1);
    gene->SetXref().push_back(x2);

    CRef<CSeqFeatXref> x3(new CSeqFeatXref());
    x3->SetData().SetGene().SetLocus("Z");
    CRef<CDbtag> db2(new CDbtag());
    db2->SetDb("C");
    db2->SetTag().SetStr("D");
    x3->SetData().SetGene().SetDb().push_back(db2);
    gene->SetXref().push_back(x3);


    CRef<CScope> scope(new CScope(*CObjectManager::GetInstance()));;
    CSeq_entry_Handle seh = scope->AddTopLevelSeqEntry(*entry);
    entry->Parentize();

    CCleanup cleanup;
    CConstRef<CCleanupChange> changes;

    cleanup.SetScope(scope);
    changes = cleanup.BasicCleanup(*entry);

    BOOST_CHECK_EQUAL(gene->GetXref().size(), 2);
    BOOST_CHECK_EQUAL(gene->GetXref().front()->IsSetId(), true);
    BOOST_CHECK_EQUAL(gene->GetXref().back()->GetData().GetGene().GetLocus(), "Z");
    BOOST_CHECK_EQUAL(gene->GetXref().back()->GetData().GetGene().IsSetDb(), false);
    BOOST_CHECK_EQUAL(gene->GetDbxref().size(), 2);
    BOOST_CHECK_EQUAL(gene->GetDbxref().front()->GetDb(), "A");
    BOOST_CHECK_EQUAL(gene->GetDbxref().back()->GetDb(), "C");

}


// move dbxref in prot to feat.dbxref
BOOST_AUTO_TEST_CASE(Test_BasicProtCleanup)
{
    CRef<CSeq_entry> entry = BuildGoodNucProtSet();
    CRef<CSeq_feat> prot = GetProtFeatFromGoodNucProtSet(entry);
    CRef<CDbtag> db(new CDbtag());
    db->SetDb("x");
    db->SetTag().SetStr("y");
    prot->SetData().SetProt().SetDb().push_back(db);
    CRef<CScope> scope(new CScope(*CObjectManager::GetInstance()));;
    CSeq_entry_Handle seh = scope->AddTopLevelSeqEntry(*entry);
    entry->Parentize();

    CCleanup cleanup;
    CConstRef<CCleanupChange> changes;

    cleanup.SetScope(scope);
    changes = cleanup.BasicCleanup(*entry);

    BOOST_CHECK_EQUAL(prot->GetData().GetProt().IsSetDb(), false);
    BOOST_CHECK_EQUAL(prot->GetDbxref().size(), 1);
}


void TestOneDate(int hour, int minute, int second, bool expect_hour, bool expect_minute, bool expect_second)
{
    CRef<CSeq_entry> entry = BuildGoodSeq();
    CRef<CSeqdesc> desc(new CSeqdesc());
    entry->SetSeq().SetDescr().Set().push_back(desc);
    auto& createdate = desc->SetCreate_date().SetStd();
    createdate.SetYear(2019);
    createdate.SetMonth(1);
    createdate.SetDay(4);
    if (hour > 0) {
        createdate.SetHour(hour);
    }
    if (minute > 0) {
        createdate.SetMinute(minute);
    }
    if (second > 0) {
        createdate.SetSecond(second);
    }

    CRef<CScope> scope(new CScope(*CObjectManager::GetInstance()));;
    CSeq_entry_Handle seh = scope->AddTopLevelSeqEntry(*entry);
    entry->Parentize();

    CCleanup cleanup;
    CConstRef<CCleanupChange> changes;

    cleanup.SetScope(scope);
    changes = cleanup.BasicCleanup(*entry);
    BOOST_CHECK_EQUAL(createdate.IsSetHour(), expect_hour);
    BOOST_CHECK_EQUAL(createdate.IsSetMinute(), expect_minute);
    BOOST_CHECK_EQUAL(createdate.IsSetSecond(), expect_second);
}


BOOST_AUTO_TEST_CASE(Test_DateCleanup)
{
    TestOneDate(8, 61, 32, true, false, false);
    TestOneDate(8, -1, 32, true, false, false);
    TestOneDate(25, 30, 32, false, false, false);
    TestOneDate(-1, 30, 32, false, false, false);

}


BOOST_AUTO_TEST_CASE(Test_StripSpaces)
{
    string input = "(\t    )";
    StripSpaces(input);
    BOOST_CHECK_EQUAL(input, "()");
    input = "a (   x   )";
    StripSpaces(input);
    BOOST_CHECK_EQUAL(input, "a (x)");
    input = "a , b , c";
    StripSpaces(input);
    BOOST_CHECK_EQUAL(input, "a, b, c");
    input = "a    b";
    StripSpaces(input);
    BOOST_CHECK_EQUAL(input, "a b");
}


BOOST_AUTO_TEST_CASE(Test_SingleDesc)
{
    CCleanup cleanup;
    CConstRef<CCleanupChange> changes;
    CRef<CSeqdesc> desc(new CSeqdesc());
    desc->SetComment(" a b ");
    changes = cleanup.BasicCleanup(*desc);
    BOOST_CHECK_EQUAL(desc->GetComment(), "a b");

}


BOOST_AUTO_TEST_CASE(Test_MultipleDesc)
{
    CCleanup cleanup;
    CConstRef<CCleanupChange> changes;
    CRef<CSeqdesc> comment(new CSeqdesc());
    comment->SetComment(" a b ");
    CRef<CSeqdesc> dated(new CSeqdesc());
    auto& createdate = dated->SetCreate_date().SetStd();
    createdate.SetYear(2019);
    createdate.SetMonth(1);
    createdate.SetDay(4);
    createdate.SetHour(8);
    createdate.SetMinute(61);
    createdate.SetSecond(32);

    CRef<CSeq_descr> descr(new CSeq_descr());
    descr->Set().push_back(comment);
    descr->Set().push_back(dated);

    changes = cleanup.BasicCleanup(*descr);
    BOOST_CHECK_EQUAL(comment->GetComment(), "a b");
    BOOST_CHECK_EQUAL(createdate.IsSetMinute(), false);
    BOOST_CHECK_EQUAL(createdate.IsSetSecond(), false);

}

BOOST_AUTO_TEST_CASE(Test_PGAP_1320)
{
    CCleanup cleanup;
    CConstRef<CCleanupChange> changes;
    CRef<CSeqdesc> src(new CSeqdesc());
    src->SetSource().SetOrg().SetTaxname("X");
    CRef<CSubSource> alt(new CSubSource(CSubSource::eSubtype_altitude, "634 m."));
    src->SetSource().SetSubtype().push_back(alt);

    changes = cleanup.BasicCleanup(*src);
    BOOST_CHECK_EQUAL(src->GetSource().GetSubtype().front()->GetName(), "634 m");

}

BOOST_AUTO_TEST_CASE(Test_RW_978)
{
    CCleanup cleanup;
    CConstRef<CCleanupChange> changes;
    CRef<CSeqdesc> src(new CSeqdesc());
    src->SetSource().SetOrg().SetTaxname("X");
    CRef<CSubSource> alt(new CSubSource(CSubSource::eSubtype_country, "USA:DE:Wilmington"));
    src->SetSource().SetSubtype().push_back(alt);

    changes = cleanup.BasicCleanup(*src);
    BOOST_CHECK_EQUAL(src->GetSource().GetSubtype().front()->GetName(), "USA:DE,Wilmington");

}
