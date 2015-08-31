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
 * Author: Sema
 *
 * File Description:
 *   Sample unit tests file for the mainstream test developing.
 *
 * This file represents basic most common usage of Ncbi.Test framework (which
 * is based on Boost.Test framework. For more advanced techniques look into
 * another sample - unit_test_alt_sample.cpp.
 *
 * ===========================================================================
 */

#include <ncbi_pch.hpp>
//#include <corelib/ncbi_system.hpp>
//#include <corelib/ncbiapp.hpp>
//#include <objects/general/Dbtag.hpp>
//#include <objects/seq/Seq_gap.hpp>
//#include <objects/seqfeat/Imp_feat.hpp>
//#include <objects/seqfeat/RNA_gen.hpp>
//#include <objects/seqfeat/Cdregion.hpp>
//#include <objects/seqres/Int_graph.hpp>
//#include <objects/seqres/Seq_graph.hpp>
//#include <objects/pub/Pub.hpp>
//#include <objects/biblio/Cit_art.hpp>
//#include <objects/biblio/Auth_list.hpp>
//#include <objects/biblio/Affil.hpp>
#include <objects/seqfeat/RNA_ref.hpp>
#include <objtools/unit_test_util/unit_test_util.hpp>
#include <misc/discrepancy/discrepancy.hpp>
#include <objmgr/object_manager.hpp>

// This header must be included before all Boost.Test headers if there are any
#include <corelib/test_boost.hpp>

USING_NCBI_SCOPE;
USING_SCOPE(objects);
USING_SCOPE(NDiscrepancy);

NCBITEST_AUTO_INIT()
{
    // Your application initialization code here (optional)
    cout << "Initialization function executed" << endl;
}


NCBITEST_INIT_CMDLINE(arg_desc)
{
    // Describe command line parameters that we are going to use
    arg_desc->AddFlag
        ("enable_TestTimeout",
         "Run TestTimeout test, which is artificially disabled by default in"
         "order to avoid unwanted failure during the daily automated builds.");
}


NCBITEST_AUTO_FINI()
{
    // Your application finalization code here (optional)
    cout << "Finalization function executed" << endl;
}


BOOST_AUTO_TEST_CASE(NON_EXISTENT)
{
    CScope scope(*CObjectManager::GetInstance());
    CRef<CDiscrepancySet> Set = CDiscrepancySet::New(scope);
    Set->AddTest("NON-EXISTENT");
    const vector<CRef<CDiscrepancyCase> >& tst = Set->GetTests();
    BOOST_REQUIRE_EQUAL(tst.size(), 0);
};


BOOST_AUTO_TEST_CASE(COUNT_NUCLEOTIDES)
{
    CRef<CSeq_entry> e1 = unit_test_util::BuildGoodSeq();
    CScope scope(*CObjectManager::GetInstance());
    scope.AddDefaults();
    CSeq_entry_Handle seh = scope.AddTopLevelSeqEntry(*e1);

    CRef<CDiscrepancySet> Set = CDiscrepancySet::New(scope);
    Set->AddTest("COUNT_NUCLEOTIDES");
    Set->Parse(seh);
    Set->Summarize();
    const vector<CRef<CDiscrepancyCase> >& tst = Set->GetTests();
    BOOST_REQUIRE_EQUAL(tst.size(), 1);
    TReportItemList rep = tst[0]->GetReport();
    BOOST_REQUIRE_EQUAL(rep.size(), 1);
    BOOST_REQUIRE_EQUAL(rep[0]->GetMsg(), "1 nucleotide Bioseq is present");
}


BOOST_AUTO_TEST_CASE(COUNT_PROTEINS)
{
    CRef<CSeq_entry> e1 = unit_test_util::BuildGoodSeq();
    CScope scope(*CObjectManager::GetInstance());
    scope.AddDefaults();
    CSeq_entry_Handle seh = scope.AddTopLevelSeqEntry(*e1);

    CRef<CDiscrepancySet> Set = CDiscrepancySet::New(scope);
    Set->AddTest("COUNT_PROTEINS");
    Set->Parse(seh);
    Set->Summarize();
    const vector<CRef<CDiscrepancyCase> >& tst = Set->GetTests();
    BOOST_REQUIRE_EQUAL(tst.size(), 1);
    TReportItemList rep = tst[0]->GetReport();
    BOOST_REQUIRE_EQUAL(rep.size(), 1);
    BOOST_REQUIRE_EQUAL(rep[0]->GetMsg(), "0 protein sequences are present");
}


BOOST_AUTO_TEST_CASE(COUNT_TRNAS)
{
    const char* inp = "Seq-entry ::= seq {\
      id {\
        local str \"my_id\"\
      },\
      descr {\
        source {\
          genome plastid,\
          org {\
            taxname \"Sebaea microphylla\",\
            db {\
              {\
                db \"taxon\",\
                tag id 592768\
              }\
            },\
            orgname {\
              lineage \"some lineage\"\
            }\
          },\
          subtype {\
            {\
              subtype chromosome,\
              name \"1\"\
            }\
          }\
        }\
      },\
      inst {\
        repr raw,\
        mol dna,\
        length 5,\
        seq-data iupacna \"AAAAA\"\
      },\
      annot {\
        {\
          data ftable {\
            {\
              data rna {\
                type tRNA,\
                ext tRNA {\
                  aa iupacaa 70,\
                  anticodon int {\
                    from 8,\
                    to 10,\
                    id local str \"my_id\"\
                  }\
                }\
              },\
              location int {\
                from 0,\
                to 10,\
                id local str \"my_id\"\
              }\
            },\
            {\
              data rna {\
                type tRNA,\
                ext tRNA {\
                  aa iupacaa 70,\
                  anticodon int {\
                    from 8,\
                    to 10,\
                    id local str \"my_id\"\
                  }\
                }\
              },\
              location int {\
                from 0,\
                to 10,\
                id local str \"my_id\"\
              }\
            }\
          }\
        }\
      }\
    }";
    CNcbiIstrstream istr(inp);
    CRef<CSeq_entry> entry (new CSeq_entry);
    istr >> MSerial_AsnText >> *entry;
    CScope scope(*CObjectManager::GetInstance());
    CSeq_entry_Handle seh = scope.AddTopLevelSeqEntry(*entry);

    CRef<CDiscrepancySet> Set = CDiscrepancySet::New(scope);
    Set->AddTest("COUNT_TRNAS");
    Set->Parse(seh);
    Set->Summarize();
    const vector<CRef<CDiscrepancyCase> >& tst = Set->GetTests();
    BOOST_REQUIRE_EQUAL(tst.size(), 1);
    TReportItemList rep = tst[0]->GetReport();
    BOOST_REQUIRE_EQUAL(rep.size(), 21);
    BOOST_REQUIRE_EQUAL(rep[0]->GetMsg(), "sequence my_id has 2 tRNA features");
    BOOST_REQUIRE_EQUAL(rep[1]->GetMsg(), "sequence my_id has 2 trna-Phe features");
    BOOST_REQUIRE_EQUAL(rep[2]->GetMsg(), "sequence my_id is missing trna-Ala");
}


BOOST_AUTO_TEST_CASE(COUNT_RRNAS)
{
    const char* inp = "Seq-entry ::= set {\
      class genbank ,\
      seq-set {\
        set {\
          class nuc-prot ,\
          descr {\
            source {\
              genome mitochondrion ,\
              org {\
                taxname \"Simulium aureohirtum\" ,\
                db {\
                  {\
                    db \"taxon\" ,\
                    tag\
                      id 154798 } } ,\
                orgname {\
                  name\
                    binomial {\
                      genus \"Simulium\" ,\
                      species \"aureohirtum\" } ,\
                  gcode 1 ,\
                  mgcode 5 ,\
                  div \"INV\" } } ,\
              subtype {\
                {\
                  subtype country ,\
                  name \"China Nanfeng Mountian\" } } } } ,\
          seq-set {\
            seq {\
              id {\
                genbank {\
                  accession \"KP793690\" } } ,\
              inst {\
                repr raw,\
                mol dna,\
                length 5,\
                seq-data iupacna \"AAAAA\"\
              },\
              annot {\
                {\
                  data\
                    ftable {\
                      {\
                        data\
                          rna {\
                            type rRNA ,\
                            ext\
                              name \"16S ribosomal RNA\" } ,\
                        location\
                          int {\
                            from 12765 ,\
                            to 14100 ,\
                            strand minus ,\
                            id\
                              local\
                                str \"nuc_1\" } } ,\
                      {\
                        data\
                          rna {\
                            type rRNA ,\
                            ext\
                              name \"16S ribosomal RNA\" } ,\
                        location\
                          int {\
                            from 12765 ,\
                            to 14100 ,\
                            strand minus ,\
                            id\
                              local\
                                str \"nuc_1\" } } } } } } } } } } }";
    CNcbiIstrstream istr(inp);
    CRef<CSeq_entry> entry (new CSeq_entry);
    istr >> MSerial_AsnText >> *entry;
    CScope scope(*CObjectManager::GetInstance());
    CSeq_entry_Handle seh = scope.AddTopLevelSeqEntry(*entry);

    CRef<CDiscrepancySet> Set = CDiscrepancySet::New(scope);
    Set->AddTest("COUNT_RRNAS");
    Set->Parse(seh);
    Set->Summarize();
    const vector<CRef<CDiscrepancyCase> >& tst = Set->GetTests();
    BOOST_REQUIRE_EQUAL(tst.size(), 1);
    TReportItemList rep = tst[0]->GetReport();
    BOOST_REQUIRE_EQUAL(rep.size(), 2);
    BOOST_REQUIRE_EQUAL(rep[0]->GetMsg(), "sequence KP793690 has 2 rRNA features");
    BOOST_REQUIRE_EQUAL(rep[1]->GetMsg(), "2 rRNA features on KP793690 have the same name (16S ribosomal RNA)");
}


BOOST_AUTO_TEST_CASE(OVERLAPPING_CDS)
{
/*
    CRef<CSeq_entry> e1 = unit_test_util::BuildGoodSeq();
    CRef<CObjectManager> objmgr = CObjectManager::GetInstance();
    CScope scope(*objmgr);
    scope.AddDefaults();
    CSeq_entry_Handle seh = scope.AddTopLevelSeqEntry(*e1);

    CRef<CDiscrepancyCase> dscr = GetDiscrepancyCase("OVERLAPPING_CDS");
    CContext context;
    dscr->Parse(seh, context);
*/
}


BOOST_AUTO_TEST_CASE(CONTAINED_CDS)
{
/*
    CRef<CSeq_entry> e1 = unit_test_util::BuildGoodSeq();
    CRef<CObjectManager> objmgr = CObjectManager::GetInstance();
    CScope scope(*objmgr);
    scope.AddDefaults();
    CSeq_entry_Handle seh = scope.AddTopLevelSeqEntry(*e1);

    CRef<CDiscrepancyCase> dscr = GetDiscrepancyCase("CONTAINED_CDS");
    CContext context;
    dscr->Parse(seh, context);
*/
}


BOOST_AUTO_TEST_CASE(DISC_INTERNAL_TRANSCRIBED_SPACER_RRNA)
{
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodSeq();
    CRef<CSeq_id> id = entry->SetSeq().SetId().front();

    //
    string remainder;
    CRef<CSeq_feat> feat_1(new CSeq_feat);
    CRef<CRNA_ref> mrna(new CRNA_ref);
    mrna->SetType(CRNA_ref::eType_mRNA);
    mrna->SetRnaProductName("testing the internal rna name", remainder);
    feat_1->SetData().SetRna(*mrna);
    feat_1->SetLocation().SetInt().SetFrom(0);
    feat_1->SetLocation().SetInt().SetTo(10);
    feat_1->SetLocation().SetInt().SetId().Assign(*id);
    unit_test_util::AddFeat(feat_1, entry);

    // add rRNA
    CRef<CSeq_feat> feat_2(new CSeq_feat);
    CRef<CRNA_ref> rna_1(new CRNA_ref);
    rna_1->SetType(CRNA_ref::eType_rRNA);
    rna_1->SetRnaProductName("internal rna region", remainder);
    feat_2->SetData().SetRna(*rna_1);
    feat_2->SetLocation().SetInt().SetFrom(2);
    feat_2->SetLocation().SetInt().SetTo(20);
    feat_2->SetLocation().SetInt().SetId().Assign(*id);
    unit_test_util::AddFeat(feat_2, entry);

    // add misc_RNA
    CRef<CSeq_feat> feat_3(new CSeq_feat);
    CRef<CRNA_ref> misc_rna(new CRNA_ref);
    misc_rna->SetType(CRNA_ref::eType_miscRNA);
    misc_rna->SetRnaProductName("contains spacer", remainder);
    feat_3->SetData().SetRna(*misc_rna);
    feat_3->SetLocation().SetInt().SetFrom(3);
    feat_3->SetLocation().SetInt().SetTo(30);
    feat_3->SetLocation().SetInt().SetId().Assign(*id);
    unit_test_util::AddFeat(feat_3, entry);
    
    // add rRNA
    CRef<CSeq_feat> feat_4(new CSeq_feat);
    CRef<CRNA_ref> rna_2(new CRNA_ref);
    rna_2->SetType(CRNA_ref::eType_rRNA);
    rna_2->SetRnaProductName("12S ribosomal rna", remainder);
    feat_4->SetData().SetRna(*rna_2);
    feat_4->SetLocation().SetInt().SetFrom(4);
    feat_4->SetLocation().SetInt().SetTo(40);
    feat_4->SetLocation().SetInt().SetId().Assign(*id);
    unit_test_util::AddFeat(feat_4, entry);

    
    // add rRNA
    CRef<CSeq_feat> feat_5(new CSeq_feat);
    CRef<CRNA_ref> rna_3(new CRNA_ref);
    rna_3->SetType(CRNA_ref::eType_rRNA);
    rna_3->SetRnaProductName("transcribed region", remainder);
    feat_5->SetData().SetRna(*rna_3);
    feat_5->SetLocation().SetInt().SetFrom(5);
    feat_5->SetLocation().SetInt().SetTo(50);
    feat_5->SetLocation().SetInt().SetId().Assign(*id);
    unit_test_util::AddFeat(feat_5, entry);
    
    // add rRNA
    CRef<CSeq_feat> feat_6(new CSeq_feat);
    CRef<CRNA_ref> rna_4(new CRNA_ref);
    rna_4->SetType(CRNA_ref::eType_rRNA);
    rna_4->SetRnaProductName("contains spacer", remainder);
    feat_6->SetData().SetRna(*rna_4);
    feat_6->SetLocation().SetInt().SetFrom(6);
    feat_6->SetLocation().SetInt().SetTo(55);
    feat_6->SetLocation().SetInt().SetId().Assign(*id);
    unit_test_util::AddFeat(feat_6, entry);
    CScope scope(*CObjectManager::GetInstance());
    scope.AddDefaults();
    CSeq_entry_Handle seh = scope.AddTopLevelSeqEntry(*entry);

    CRef<CDiscrepancySet> Set = CDiscrepancySet::New(scope);
    Set->AddTest("DISC_INTERNAL_TRANSCRIBED_SPACER_RRNA");
    Set->Parse(seh);
    Set->Summarize();
    const vector<CRef<CDiscrepancyCase> >& tst = Set->GetTests();
    BOOST_REQUIRE_EQUAL(tst.size(), 1);
    TReportItemList rep = tst[0]->GetReport();
    BOOST_CHECK(rep.size() == 1);
    BOOST_CHECK(NStr::StartsWith(rep[0]->GetMsg(), "3 rRNA feature products contain"));
   
}


