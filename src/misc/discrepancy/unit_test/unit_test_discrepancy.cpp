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

CRef<CSeq_entry> ReadEntryFromFile(const string& fname)
{
    CRef<CSeq_entry> entry(new CSeq_entry);
    try {
        CNcbiIfstream istr(fname.c_str());
        auto_ptr<CObjectIStream> os(CObjectIStream::Open(eSerial_AsnText, istr));
        *os >> *entry;
    } catch(const CException& e) {
        LOG_POST(Error << e.ReportAll());
        return CRef<CSeq_entry>();
    }
    
    return entry;
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

BOOST_AUTO_TEST_CASE(SHORT_SEQUENCES)
{
    CRef<CSeq_entry> entry = ReadEntryFromFile("test_data/shortseqs.asn");
    BOOST_REQUIRE(entry);
    CScope scope(*CObjectManager::GetInstance());
    scope.AddDefaults();
    CSeq_entry_Handle seh = scope.AddTopLevelSeqEntry(*entry);
    
    CRef<CDiscrepancySet> set = CDiscrepancySet::New(scope);
    set->AddTest("SHORT_SEQUENCES");
    set->Parse(seh);
    set->Summarize();
    
    const vector<CRef<CDiscrepancyCase> >& tst = set->GetTests();
    TReportItemList rep = tst[0]->GetReport();
    BOOST_REQUIRE_EQUAL(rep.size(), 1);
    BOOST_CHECK_EQUAL(rep[0]->GetMsg(), "2 sequences are shorter than 50 nt");
}

BOOST_AUTO_TEST_CASE(PERCENT_N_INCLUDE_GAPS_IN_LENGTH)
{
    CRef<CSeq_entry> entry = ReadEntryFromFile("test_data/normal_delta.asn");
    BOOST_REQUIRE(entry);
    CScope scope(*CObjectManager::GetInstance());
    scope.AddDefaults();
    CSeq_entry_Handle seh = scope.AddTopLevelSeqEntry(*entry);
    
    CRef<CDiscrepancySet> set = CDiscrepancySet::New(scope);
    set->AddTest("PERCENT_N");
    set->Parse(seh);
    set->Summarize();
    
    const vector<CRef<CDiscrepancyCase> >& tst = set->GetTests();
    TReportItemList rep = tst[0]->GetReport();
    BOOST_REQUIRE_EQUAL(rep.size(), 1);
    /* 
     * There will be 2 seqs if "seq_3_again" is handled incorrectly,
     * either by not counting the length of the gap, or by counting
     * the gap as Ns.
     */
    BOOST_CHECK_EQUAL(rep[0]->GetMsg(), "1 sequence has more than 5% Ns");
}

BOOST_AUTO_TEST_CASE(PERCENT_N)
{
    CRef<CSeq_entry> entry = ReadEntryFromFile("test_data/percent_n.asn");
    BOOST_REQUIRE(entry);
    CScope scope(*CObjectManager::GetInstance());
    scope.AddDefaults();
    CSeq_entry_Handle seh = scope.AddTopLevelSeqEntry(*entry);
    
    CRef<CDiscrepancySet> set = CDiscrepancySet::New(scope);
    set->AddTest("PERCENT_N");
    set->Parse(seh);
    set->Summarize();
    
    const vector<CRef<CDiscrepancyCase> >& tst = set->GetTests();
    TReportItemList rep = tst[0]->GetReport();
    BOOST_REQUIRE_EQUAL(rep.size(), 1);
    BOOST_CHECK_EQUAL(rep[0]->GetMsg(), "1 sequence has more than 5% Ns");
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
                local\
                  str \"nuc_1\" } ,\
              inst {\
                repr raw ,\
                mol dna ,\
                length 1 ,\
                seq-data\
                  ncbi2na '0E'H } ,\
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
                                str \"nuc_1\" } } } } } } } } } }";
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
    BOOST_REQUIRE_EQUAL(rep[0]->GetMsg(), "sequence nuc_1 has 2 rRNA features");
    BOOST_REQUIRE_EQUAL(rep[1]->GetMsg(), "2 rRNA features on nuc_1 have the same name (16S ribosomal RNA)");
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


BOOST_AUTO_TEST_CASE(INTERNAL_TRANSCRIBED_SPACER_RRNA)
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
    Set->AddTest("INTERNAL_TRANSCRIBED_SPACER_RRNA");
    Set->Parse(seh);
    Set->Summarize();
    const vector<CRef<CDiscrepancyCase> >& tst = Set->GetTests();
    BOOST_REQUIRE_EQUAL(tst.size(), 1);
    TReportItemList rep = tst[0]->GetReport();
    BOOST_CHECK(rep.size() == 1);
    BOOST_CHECK(NStr::StartsWith(rep[0]->GetMsg(), "3 rRNA feature products contain"));
   
}

BOOST_AUTO_TEST_CASE(DIVISION_CODE_CONFLICTS)
{
    const char* inp = "Seq-entry ::= \
      set {\
        class pop-set ,\
        descr {\
          pub {\
            pub {\
              gen {\
                cit \"Unpublished\" ,\
                authors {\
                  names\
                    std {\
                      {\
                        name\
                          name {\
                            last \"q\" ,\
                            first \"q\" ,\
                            initials \"qq.\" } } } ,\
                  affil\
                    str \"q\" } ,\
                title \"tentative title for manuscript\" } } } ,\
          user {\
            type\
              str \"NcbiCleanup\" ,\
            data {\
              {\
                label\
                  str \"method\" ,\
                data\
                  str \"SequinCleanup\" } ,\
              {\
                label\
                  str \"version\" ,\
                data\
                  int 8 } ,\
              {\
                label\
                  str \"month\" ,\
                data\
                  int 8 } ,\
              {\
                label\
                  str \"day\" ,\
                data\
                  int 24 } ,\
              {\
                label\
                  str \"year\" ,\
                data\
                  int 2015 } } } ,\
          create-date\
            std {\
              year 2012 ,\
              month 5 ,\
              day 30 } } ,\
        seq-set {\
          set {\
            class nuc-prot ,\
            descr {\
              source {\
                genome genomic ,\
                org {\
                  taxname \"Acanthamoeba castellanii\" ,\
                  orgname {\
                    lineage \"Eukaryota; Acanthamoebidae; Acanthamoeba\" ,\
                    gcode 1 ,\
                    div \"INV\" } } } } ,\
            seq-set {\
              seq {\
                id {\
                  local\
                    str \"Seq1\" } ,\
                descr {\
                  molinfo {\
                    biomol genomic } } ,\
                inst {\
                  repr raw ,\
                  mol dna ,\
                  length 5121 ,\
                  seq-data\
                    ncbi2na 'C24AA20F73C14E08734F9FE2713DCEDEBFD9D001F5E80E104\
91A419EE5FA914E4B1406818CB8010D1838503E0BC70E71E27AF48BD7412B80CE612D74D235F8E\
8801E4470C8E773EA215D2EE3A7D403082EA17FFBE0609025C4907BC57CE3B968F39757CAD1CBE\
5935A447A2FC10E027D0FA1E8B47401A042779F930A2370CB2FDFCB23E0FAF8547C07D031524F8\
1B87394043810FE103EC4FEAABD1456AC684285037D7B39D0D34A08344B371402094109EC3560C\
DA3721520C283357248309373E8432C016A2131FF8F04912A0DC3E75CAAF1F403182EA009D0C38\
8D239153E90390F7939347503A093D50E1014F500EC04A344C6A97B548CEF090247780F2412839\
80EC5220101C8A4CFE990C9ABF4C803AFA8A83AE8EBEB1AFD293403DE2A08A109248DD0091D092\
436340D0EA09E0DAF8DA80141880F5348F80083DD20B60A883D285F880CEF8A11C032377AD3106\
689F7EF95E88149310F8DC1E1D2038101EFE000408241E2A00E78A33A90EBEFD00CC5103B84397\
9328D0D203A07CC1463B31223824F0105AF52342A2F89E0B4AB1023E8D73A3F5FE5334EFFF9FEE\
F9FEFAAF4D3BA979402A413CB092A883DCF053821CD3E7F89C4F73B7AFF674007D7A0384124690\
67B97EA45392C501A0632E0043460E140F82F1C39C789EBD22F5D04AE033984B5D348D7E3A2007\
911C3239DCFA88574BB8E9F500C20BA85FFEF818240971241EF15F38EE5A3CE5D5F2B472F964D6\
911E8BF043809F43E87A2D1D0068109DE7E4C288DC32CBF7F2C8F83EBE151F01F40C5493E06E1C\
E5010E043F840FB13FAAAF4515AB1A10A140DF5ECE7434D2820D12CDC5008250427B0D583368DC\
854830A0CD5C920C24DCFA10CB005A884C7FE3C1244A8370F9D72ABC7D00C60BA8027430E2348E\
454FA40E43DE4E4D1D40E824F54384053D403B0128D131AA5ED5233BC24091DE03C904A0E603B1\
488040722933FA64326AFD3200EBEA2A0EBA3AFAC6BF4A4D00F78A822842492374024742490D8D\
0343A827836BE36A00506203D4D23E0020F7482D82A20F4A17E2033BE284700C8DDEB4C419A27D\
FBE57A20524C43E370787480E0407BF80010209078A8039E28CEA43AFBF40331440EE10E5E4CA3\
43480E81F30518ECC488E093C0416BD48D0A8BE2782D2AC408FA35CE8FD7F94CD3BFFE7FBBE7FB\
EABD34EEA5E500A904F2C24AA20F73C14E08734F9FE2713DCEDEBFD9D001F5E80E10491A419EE5\
FA914E4B1406818CB8010D1838503E0BC70E71E27AF48BD7412B80CE612D74D235F8E8801E4470\
C8E773EA215D2EE3A7D403082EA17FFBE0609025C4907BC57CE3B968F39757CAD1CBE5935A447A\
2FC10E027D0FA1E8B47401A042779F930A2370CB2FDFCB23E0FAF8547C07D031524F81B8739404\
3810FE103EC4FEAABD1456AC684285037D7B39D0D34A08344B371402094109EC3560CDA3721520\
C283357248309373E8432C016A2131FF8F04912A0DC3E75CAAF1F403182EA009D0C388D239153E\
90390F7939347503A093D50E1014F500EC04A344C6A97B548CEF090247780F241283980EC52201\
01C8A4CFE990C9ABF4C803AFA8A83AE8EBEB1AFD293403DE2A08A109248DD0091D092436340D0E\
A09E0DAF8DA80141880F5348F80083DD20B60A883D285F880CEF8A11C032377AD3106689F7EF95\
E88149310F8DC1E1D2038101EFE000408241E2A00E78A33A90EBEFD00CC5103B843979328D0D20\
3A07CC1463B31223824F0105AF52342A2F89E0B4AB1023E8D73A3F5FE5334EFFF9FEEF9FEFAAF4\
D3BA979402A413C80'H } } ,\
              seq {\
                id {\
                  local\
                    str \"Seq1_1\" } ,\
                descr {\
                  molinfo {\
                    biomol peptide ,\
                    tech concept-trans } } ,\
                inst {\
                  repr raw ,\
                  mol aa ,\
                  length 1706 ,\
                  seq-data\
                    ncbieaa \"-AGENSINHEDYHCFELHSMSGFRSKTSWK*QQHGNAVPWAPCSTKRND\
SENNHE*PN*SY*CY*AGSEFLNR*NMRQSSSDP*WRKLHTNRCSIGRPSV*WLPK*EVGPFC*TKQSLQQLLPL*CA\
GLCLP*VTSCRIRHTGV*Q*KLQLDWSHSKRNKLCLHKEI***FL**IELVDPLKLQIPSIERDYAKQ*TI*QIVHLG\
GSPPGYGQGPNLPVCSIIRKNHSIYQKKPTSCNPEYRI*TQNKEYP*QNKHLLDNSKTGRHTFD*QHRESNCS*GLLQ\
NTKWEKLNNEIRCTHWQMQFCMHHSKWKHSQ*QTIPKCKQDHIRGLSQIC*AKHSEISNRNAKCTRETN*RHIWRNSG\
FHRKWLGGNGGWLVRFQASKF*GKRTSSRSQKHSSSNRSNQWEAESVDRENQREIPSD*KRILRSRRENSGP*EIC*G\
H*NRSLVIQRGASCCPGEPAYN*SN*LRNEQTV*KNKEATEGKC*GYGQWLFQNIPQM*QCLHRINQKWNL*PRCIQR\
*SIKQPVPDQGS*AEVRVQRLDPMDFLCHIMFFALCCFVGVHHVGLPKGQH**AGENSINHEDYHCFELHSMSGFRSK\
TSWK*QQHGNAVPWAPCSTKRNDSENNHE*PN*SY*CY*AGSEFLNR*NMRQSSSDP*WRKLHTNRCSIGRPSV*WLP\
K*EVGPFC*TKQSLQQLLPL*CAGLCLP*VTSCRIRHTGV*Q*KLQLDWSHSKRNKLCLHKEI***FL**IELVDPLK\
LQIPSIERDYAKQ*TI*QIVHLGGSPPGYGQGPNLPVCSIIRKNHSIYQKKPTSCNPEYRI*TQNKEYP*QNKHLLDN\
SKTGRHTFD*QHRESNCS*GLLQNTKWEKLNNEIRCTHWQMQFCMHHSKWKHSQ*QTIPKCKQDHIRGLSQIC*AKHS\
EISNRNAKCTRETN*RHIWRNSGFHRKWLGGNGGWLVRFQASKF*GKRTSSRSQKHSSSNRSNQWEAESVDRENQREI\
PSD*KRILRSRRENSGP*EIC*GH*NRSLVIQRGASCCPGEPAYN*SN*LRNEQTV*KNKEATEGKC*GYGQWLFQNI\
PQM*QCLHRINQKWNL*PRCIQR*SIKQPVPDQGS*AEVRVQRLDPMDFLCHIMFFALCCFVGVHHVGLPKGQH**AG\
ENSINHEDYHCFELHSMSGFRSKTSWK*QQHGNAVPWAPCSTKRNDSENNHE*PN*SY*CY*AGSEFLNR*NMRQSSS\
DP*WRKLHTNRCSIGRPSV*WLPK*EVGPFC*TKQSLQQLLPL*CAGLCLP*VTSCRIRHTGV*Q*KLQLDWSHSKRN\
KLCLHKEI***FL**IELVDPLKLQIPSIERDYAKQ*TI*QIVHLGGSPPGYGQGPNLPVCSIIRKNHSIYQKKPTSC\
NPEYRI*TQNKEYP*QNKHLLDNSKTGRHTFD*QHRESNCS*GLLQNTKWEKLNNEIRCTHWQMQFCMHHSKWKHSQ*\
QTIPKCKQDHIRGLSQIC*AKHSEISNRNAKCTRETN*RHIWRNSGFHRKWLGGNGGWLVRFQASKF*GKRTSSRSQK\
HSSSNRSNQWEAESVDRENQREIPSD*KRILRSRRENSGP*EIC*GH*NRSLVIQRGASCCPGEPAYN*SN*LRNEQT\
V*KNKEATEGKC*GYGQWLFQNIPQM*QCLHRINQKWNL*PRCIQR*SIKQPVPDQGS*AEVRVQRLDPMDFLCHIMF\
FALCCFVGVHHVGLPKGQH\" } ,\
                annot {\
                  {\
                    data\
                      ftable {\
                        {\
                          data\
                            prot {\
                              name {\
                                \"ANOPHELES\" } } ,\
                          location\
                            int {\
                              from 0 ,\
                              to 1705 ,\
                              id\
                                local\
                                  str \"Seq1_1\" } } } } } } } ,\
            annot {\
              {\
                data\
                  ftable {\
                    {\
                      data\
                        cdregion {\
                          code {\
                            id 1 } } ,\
                      product\
                        whole\
                          local\
                            str \"Seq1_1\" ,\
                      location\
                        int {\
                          from 0 ,\
                          to 5120 ,\
                          strand plus ,\
                          id\
                            local\
                              str \"Seq1\" } } } } } } ,\
          seq {\
            id {\
              local\
                str \"Seq2\" } ,\
            descr {\
              source {\
                genome genomic ,\
                org {\
                  taxname \"Escherichia coli\" ,\
                  orgname {\
                    gcode 11 ,\
                    div \"BCT\" } } } ,\
              molinfo {\
                biomol genomic } } ,\
            inst {\
              repr raw ,\
              mol dna ,\
              length 1451 ,\
              seq-data\
                ncbi2na 'F24024A2C08E0D40D008C3063E9DEFDDD14FD44339F7D3903E537\
E3071EC13E4FD090CE0F41D55501050B8E7BB81410C32020130488CBB378504514C88280CE5501\
4920C480FAD00590EE13C4A3F917FF70A10F63CA7F59EBAA137AB842205F3BB4E635E10BBCD0FE\
57E84A81047010EE4F40C112C6E32855F3687F3E3838BCAEF5FF4DEA85090BB9324EB52742FB46\
3A0093A793BFB306AA38C00E41E727D3F10EA29FB232EFB74EB50203DD2854A2D20E6FECD0E81F\
912CB0E1E3A0B9F4A009E31C031CF4F8A2A8036F4C7244FB4A0B9D24ED8A2E77E735D8CD7AED23\
BB7922107A029D4329536C8CC04C0A34C93EDD4BCEEEF4A1FBE8844548001849D492C94FBFA35C\
10E082BAD3A2E029EA5FE38E80E1BBA3A8204341886D19F2ACE017D02D3E0A7AD415C2D40F9230\
3290B4CBE122B8CAD6BCF7ACFF77BE0A402793436B9FF3BA2F8CAA82008A00E0B7EE85D012CFBE\
EFFBA45D2B133A04A74E978EA9A174374E5CCC27F640D7630'H } } ,\
          seq {\
            id {\
              local\
                str \"Seq3\" } ,\
            descr {\
              source {\
                genome genomic ,\
                org {\
                  taxname \"Acanthamoeba castellanii\" ,\
                  orgname {\
                    lineage \"Eukaryota; Acanthamoebidae; Acanthamoeba\" ,\
                    gcode 1 ,\
                    div \"INV\" } } } ,\
              molinfo {\
                biomol genomic } } ,\
            inst {\
              repr raw ,\
              mol dna ,\
              length 1021 ,\
              seq-data\
                ncbi2na 'D2B23BE023897DC162B601B3BDDDCDBD4D2955D02588D99221F82\
3B7F9EA0011237E29DD3A0E9C0842143DED177870AA3FEABFBBD19D16E54B898A1E49B219FED40\
395D0EA0E88540C13A1024BC07B32807C22A2306F53AA502032774BCF79EB91F94BE4EA5D3310C\
A3AA9EC14782E93FA5EBBB904EE248F9E1D5244ADD32923AC90414353C3004E20483AFFA524712\
70A73A240E9E8D0B8929268A53A23E72D294A48EB9290E2253EA1D35CBD4B1EB7088E37DF803F9\
217348060EAAE48E418F42E159FBEF9660CD3EA37E47E33EE8F7E36DFFFD039B7361DF404697C0\
2295F71A0A2C5E2DCE2A08336028124839EE8E78612D3FED24C8BE8B000714EFDC000'H } } } } } \
";
    CNcbiIstrstream istr(inp);
    CRef<CSeq_entry> entry (new CSeq_entry);
    istr >> MSerial_AsnText >> *entry;
    CScope scope(*CObjectManager::GetInstance());
    CSeq_entry_Handle seh = scope.AddTopLevelSeqEntry(*entry);

    CRef<CDiscrepancySet> Set = CDiscrepancySet::New(scope);
    Set->AddTest("DIVISION_CODE_CONFLICTS");
    Set->Parse(seh);
    Set->Summarize();
    const vector<CRef<CDiscrepancyCase> >& tst = Set->GetTests();
    BOOST_REQUIRE_EQUAL(tst.size(), 1);
    TReportItemList rep = tst[0]->GetReport();
    BOOST_REQUIRE_EQUAL(rep.size(), 1);
    BOOST_REQUIRE_EQUAL(rep[0]->GetMsg(), "Division code conflicts found");
}

BOOST_AUTO_TEST_CASE(ZERO_BASECOUNT)
{
    CRef<CSeq_entry> entry = ReadEntryFromFile("test_data/zero_basecount.asn");
    BOOST_REQUIRE(entry);
    CScope scope(*CObjectManager::GetInstance());
    scope.AddDefaults();
    CSeq_entry_Handle seh = scope.AddTopLevelSeqEntry(*entry);
    
    CRef<CDiscrepancySet> set = CDiscrepancySet::New(scope);
    set->AddTest("ZERO_BASECOUNT");
    set->Parse(seh);
    set->Summarize();

    const vector<CRef<CDiscrepancyCase> >& tst = set->GetTests();
    BOOST_REQUIRE_EQUAL(tst.size(), 1);
    TReportItemList rep = tst[0]->GetReport();
    BOOST_REQUIRE_EQUAL(rep.size(), 1);

    const CReportItem & rep_item = **rep.begin();
    BOOST_CHECK_EQUAL(
        rep_item.GetMsg(),
        "3 sequences have a zero basecount for a nucleotide");
    BOOST_CHECK_EQUAL(rep_item.CanAutofix(), false);
}

BOOST_AUTO_TEST_CASE(NO_ANNOTATION)
{
    {{
    // Test file #1 - single bioseq seqentry
    CRef<CSeq_entry> entry = ReadEntryFromFile("test_data/no_annotation_seq.asn");
    BOOST_REQUIRE(entry);
    CScope scope(*CObjectManager::GetInstance());
    scope.AddDefaults();
    CSeq_entry_Handle seh = scope.AddTopLevelSeqEntry(*entry);
    
    CRef<CDiscrepancySet> set = CDiscrepancySet::New(scope);
    set->AddTest("NO_ANNOTATION");
    set->Parse(seh);
    set->Summarize();
    
    const vector<CRef<CDiscrepancyCase> >& tst = set->GetTests();
    BOOST_REQUIRE_EQUAL(tst.size(), 1);
    TReportItemList rep = tst[0]->GetReport();
    BOOST_REQUIRE_EQUAL(rep.size(), 1);
    BOOST_CHECK_EQUAL(rep[0]->GetMsg(), "1 bioseq has no features");
    }}

    {{
    // Test file #2 - bioseq-set seqentry
    CRef<CSeq_entry> entry = ReadEntryFromFile("test_data/no_annotation_set.asn");
    BOOST_REQUIRE(entry);
    CScope scope(*CObjectManager::GetInstance());
    scope.AddDefaults();
    CSeq_entry_Handle seh = scope.AddTopLevelSeqEntry(*entry);
    
    CRef<CDiscrepancySet> set = CDiscrepancySet::New(scope);
    set->AddTest("NO_ANNOTATION");
    set->Parse(seh);
    set->Summarize();
    
    const vector<CRef<CDiscrepancyCase> >& tst = set->GetTests();
    BOOST_REQUIRE_EQUAL(tst.size(), 1);
    TReportItemList rep = tst[0]->GetReport();
    BOOST_REQUIRE_EQUAL(rep.size(), 1);
    BOOST_CHECK_EQUAL(rep[0]->GetMsg(), "2 bioseqs have no features");
    }}
}

BOOST_AUTO_TEST_CASE(LONG_NO_ANNOTATION)
{
    {{
    // Test file #1 - long sequence, no annotation
    CRef<CSeq_entry> entry = ReadEntryFromFile("test_data/no_annotation_long_seq.asn");
    BOOST_REQUIRE(entry);
    CScope scope(*CObjectManager::GetInstance());
    scope.AddDefaults();
    CSeq_entry_Handle seh = scope.AddTopLevelSeqEntry(*entry);
    
    CRef<CDiscrepancySet> set = CDiscrepancySet::New(scope);
    set->AddTest("LONG_NO_ANNOTATION");
    set->Parse(seh);
    set->Summarize();
    
    const vector<CRef<CDiscrepancyCase> >& tst = set->GetTests();
    BOOST_REQUIRE_EQUAL(tst.size(), 1);
    TReportItemList rep = tst[0]->GetReport();
    BOOST_REQUIRE_EQUAL(rep.size(), 1);
    BOOST_CHECK_EQUAL(rep[0]->GetMsg(), "1 bioseq is longer than 5000nt and has no features");
    }}

    {{
    // Test file #2 - not long enough sequence, no annotation
    CRef<CSeq_entry> entry = ReadEntryFromFile("test_data/no_annotation_not_long_enough_seq.asn");
    BOOST_REQUIRE(entry);
    CScope scope(*CObjectManager::GetInstance());
    scope.AddDefaults();
    CSeq_entry_Handle seh = scope.AddTopLevelSeqEntry(*entry);
    
    CRef<CDiscrepancySet> set = CDiscrepancySet::New(scope);
    set->AddTest("LONG_NO_ANNOTATION");
    set->Parse(seh);
    set->Summarize();
    
    const vector<CRef<CDiscrepancyCase> >& tst = set->GetTests();
    BOOST_REQUIRE_EQUAL(tst.size(), 1);
    TReportItemList rep = tst[0]->GetReport();
    BOOST_CHECK_EQUAL(rep.size(), 0);  // No report expected
    }}
}


BOOST_AUTO_TEST_CASE(POSSIBLE_LINKER)
{
    {{
        //1. Test should Pass for non-rna seq
        CRef<CSeq_entry> entry = unit_test_util::BuildGoodSeq();
        CScope scope(*CObjectManager::GetInstance());
        scope.AddDefaults();
        CSeq_entry_Handle seh = scope.AddTopLevelSeqEntry(*entry);

        CRef<CDiscrepancySet> Set = CDiscrepancySet::New(scope);
        Set->AddTest("POSSIBLE_LINKER");
        Set->Parse(seh);
        Set->Summarize();
        const vector<CRef<CDiscrepancyCase> >& tst = Set->GetTests();
        BOOST_REQUIRE_EQUAL(tst.size(), 1);
        TReportItemList rep = tst[0]->GetReport();
        BOOST_CHECK_EQUAL(rep.size(), 0);  // No report expected
     }}

    {{
        //2. Test should Pass rna seq but no polyA tail
        CRef<CSeq_entry> entry = ReadEntryFromFile("test_data/possible_linker_not_found.asn");
        CScope scope(*CObjectManager::GetInstance());
        scope.AddDefaults();
        CSeq_entry_Handle seh = scope.AddTopLevelSeqEntry(*entry);

        CRef<CDiscrepancySet> Set = CDiscrepancySet::New(scope);
        Set->AddTest("POSSIBLE_LINKER");
        Set->Parse(seh);
        Set->Summarize();
        const vector<CRef<CDiscrepancyCase> >& tst = Set->GetTests();
        BOOST_REQUIRE_EQUAL(tst.size(), 1);
        TReportItemList rep = tst[0]->GetReport();
        BOOST_CHECK_EQUAL(rep.size(), 0);  // No report expected
     }}

    {{
        //3. Test fails on rna poly - single
        CRef<CSeq_entry> entry = ReadEntryFromFile("test_data/possible_linker_single.asn");
        CScope scope(*CObjectManager::GetInstance());
        scope.AddDefaults();
        CSeq_entry_Handle seh = scope.AddTopLevelSeqEntry(*entry);

        CRef<CDiscrepancySet> Set = CDiscrepancySet::New(scope);
        Set->AddTest("POSSIBLE_LINKER");
        Set->Parse(seh);
        Set->Summarize();
        const vector<CRef<CDiscrepancyCase> >& tst = Set->GetTests();
        BOOST_REQUIRE_EQUAL(tst.size(), 1);
        TReportItemList rep = tst[0]->GetReport();
        BOOST_CHECK_EQUAL(rep.size(), 1);
        BOOST_CHECK_EQUAL(rep[0]->GetMsg(),
                          "1 bioseq may have linker sequence after the poly-A tail");
     }}

    {{
        //4. Test passes on rna polyA only - single
        CRef<CSeq_entry> entry = ReadEntryFromFile("test_data/possible_linker_single_only_polyA.asn");
        CScope scope(*CObjectManager::GetInstance());
        scope.AddDefaults();
        CSeq_entry_Handle seh = scope.AddTopLevelSeqEntry(*entry);

        CRef<CDiscrepancySet> Set = CDiscrepancySet::New(scope);
        Set->AddTest("POSSIBLE_LINKER");
        Set->Parse(seh);
        Set->Summarize();
        const vector<CRef<CDiscrepancyCase> >& tst = Set->GetTests();
        BOOST_REQUIRE_EQUAL(tst.size(), 1);
        TReportItemList rep = tst[0]->GetReport();
        BOOST_CHECK_EQUAL(rep.size(), 0); // No report expected
     }}


    {{
        //4. Test fails on rna poly - set
        CRef<CSeq_entry> entry = ReadEntryFromFile("test_data/possible_linker_set.asn");
        CScope scope(*CObjectManager::GetInstance());
        scope.AddDefaults();
        CSeq_entry_Handle seh = scope.AddTopLevelSeqEntry(*entry);

        CRef<CDiscrepancySet> Set = CDiscrepancySet::New(scope);
        Set->AddTest("POSSIBLE_LINKER");
        Set->Parse(seh);
        Set->Summarize();
        const vector<CRef<CDiscrepancyCase> >& tst = Set->GetTests();
        BOOST_REQUIRE_EQUAL(tst.size(), 1);
        TReportItemList rep = tst[0]->GetReport();
        BOOST_CHECK_EQUAL(rep.size(), 1);
        BOOST_CHECK_EQUAL(rep[0]->GetMsg(),
                          "2 bioseqs may have linker sequence after the poly-A tail");
     }}
}


BOOST_AUTO_TEST_CASE(MISSING_LOCUS_TAGS)
{
    {{
    // Test file #1
    CRef<CSeq_entry> entry = ReadEntryFromFile("test_data/missing_locus_tags.asn");
    BOOST_REQUIRE(entry);
    CScope scope(*CObjectManager::GetInstance());
    scope.AddDefaults();
    CSeq_entry_Handle seh = scope.AddTopLevelSeqEntry(*entry);
    
    CRef<CDiscrepancySet> set = CDiscrepancySet::New(scope);
    set->AddTest("MISSING_LOCUS_TAGS");
    set->Parse(seh);
    set->Summarize();
    
    const vector<CRef<CDiscrepancyCase> >& tst = set->GetTests();
    BOOST_REQUIRE_EQUAL(tst.size(), 1);
    TReportItemList rep = tst[0]->GetReport();
    BOOST_REQUIRE_EQUAL(rep.size(), 1);
    BOOST_CHECK_EQUAL(rep[0]->GetMsg(), "2 genes have no locus tags.");
    }}
}


BOOST_AUTO_TEST_CASE(BAD_LOCUS_TAG_FORMAT)
{
    {{
    // Test file #1
    CRef<CSeq_entry> entry = ReadEntryFromFile("test_data/bad_locus_tag_format.asn");
    BOOST_REQUIRE(entry);
    CScope scope(*CObjectManager::GetInstance());
    scope.AddDefaults();
    CSeq_entry_Handle seh = scope.AddTopLevelSeqEntry(*entry);
    
    CRef<CDiscrepancySet> set = CDiscrepancySet::New(scope);
    set->AddTest("BAD_LOCUS_TAG_FORMAT");
    set->Parse(seh);
    set->Summarize();
    
    const vector<CRef<CDiscrepancyCase> >& tst = set->GetTests();
    BOOST_REQUIRE_EQUAL(tst.size(), 1);
    TReportItemList rep = tst[0]->GetReport();
    BOOST_REQUIRE_EQUAL(rep.size(), 1);
    BOOST_CHECK_EQUAL(rep[0]->GetMsg(), "4 locus tags are incorrectly formatted.");
    }}
}


BOOST_AUTO_TEST_CASE(QUALITY_SCORES)
{
    {{
        // all sequences have quality scores
        CRef<CSeq_entry> entry = ReadEntryFromFile("test_data/seqsallgraphs.asn");
        BOOST_REQUIRE(entry);
        CScope scope(*CObjectManager::GetInstance());
        scope.AddDefaults();
        CSeq_entry_Handle seh = scope.AddTopLevelSeqEntry(*entry);

        CRef<CDiscrepancySet> Set = CDiscrepancySet::New(scope);
        Set->AddTest("QUALITY_SCORES");
        Set->Parse(seh);
        Set->Summarize();
        const vector<CRef<CDiscrepancyCase> >& tst = Set->GetTests();
        BOOST_REQUIRE_EQUAL(tst.size(), 1);
        TReportItemList rep = tst[0]->GetReport();
        BOOST_REQUIRE_EQUAL(rep.size(), 1);
        BOOST_CHECK_EQUAL(rep[0]->GetMsg(), "Quality scores are present on all sequences");

    }}

    {{
        // all sequences have quality scores, one of the graph is empty
        CRef<CSeq_entry> entry = ReadEntryFromFile("test_data/seqsemptygraph.asn");
        BOOST_REQUIRE(entry);
        CScope scope(*CObjectManager::GetInstance());
        scope.AddDefaults();
        CSeq_entry_Handle seh = scope.AddTopLevelSeqEntry(*entry);

        CRef<CDiscrepancySet> Set = CDiscrepancySet::New(scope);
        Set->AddTest("QUALITY_SCORES");
        Set->Parse(seh);
        Set->Summarize();
        const vector<CRef<CDiscrepancyCase> >& tst = Set->GetTests();
        BOOST_REQUIRE_EQUAL(tst.size(), 1);
        TReportItemList rep = tst[0]->GetReport();
        BOOST_REQUIRE_EQUAL(rep.size(), 1);
        BOOST_CHECK_EQUAL(rep[0]->GetMsg(), "Quality scores are missing on some(3) sequences");
    }}

    {{
        // no sequences have quality scores
        CRef<CSeq_entry> entry = ReadEntryFromFile("test_data/seqsnographs.asn");
        BOOST_REQUIRE(entry);
        CScope scope(*CObjectManager::GetInstance());
        scope.AddDefaults();
        CSeq_entry_Handle seh = scope.AddTopLevelSeqEntry(*entry);

        CRef<CDiscrepancySet> Set = CDiscrepancySet::New(scope);
        Set->AddTest("QUALITY_SCORES");
        Set->Parse(seh);
        Set->Summarize();
        const vector<CRef<CDiscrepancyCase> >& tst = Set->GetTests();
        BOOST_REQUIRE_EQUAL(tst.size(), 1);
        TReportItemList rep = tst[0]->GetReport();
        BOOST_REQUIRE_EQUAL(rep.size(), 1);
        BOOST_CHECK_EQUAL(rep[0]->GetMsg(), "Quality scores are missing on all sequences");

    }}

    {{
        // some sequences have quality scores
        CRef<CSeq_entry> entry = ReadEntryFromFile("test_data/seqsomegraphs.asn");
        BOOST_REQUIRE(entry);
        CScope scope(*CObjectManager::GetInstance());
        scope.AddDefaults();
        CSeq_entry_Handle seh = scope.AddTopLevelSeqEntry(*entry);

        CRef<CDiscrepancySet> Set = CDiscrepancySet::New(scope);
        Set->AddTest("QUALITY_SCORES");
        Set->Parse(seh);
        Set->Summarize();
        const vector<CRef<CDiscrepancyCase> >& tst = Set->GetTests();
        BOOST_REQUIRE_EQUAL(tst.size(), 1);
        TReportItemList rep = tst[0]->GetReport();
        BOOST_REQUIRE_EQUAL(rep.size(), 1);
        BOOST_CHECK_EQUAL(rep[0]->GetMsg(), "Quality scores are missing on some(2) sequences");

    }}
}
