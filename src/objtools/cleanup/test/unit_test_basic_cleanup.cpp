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

#include <serial/objistrasn.hpp>

#include <util/compress/zlib.hpp>
#include <util/compress/stream.hpp>

USING_NCBI_SCOPE;
USING_SCOPE(objects);

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