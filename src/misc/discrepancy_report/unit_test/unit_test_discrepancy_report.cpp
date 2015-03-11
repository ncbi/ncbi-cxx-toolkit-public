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
* Author:  Pavel Ivanov, J. Chen, NCBI
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

#include <corelib/ncbi_system.hpp>
#include <corelib/ncbiapp.hpp>
#include <objects/general/Dbtag.hpp>
#include <objects/seq/Seq_gap.hpp>
#include <objects/seqfeat/Imp_feat.hpp>
#include <objects/seqfeat/RNA_ref.hpp>
#include <objects/seqfeat/RNA_gen.hpp>
#include <objects/seqfeat/Cdregion.hpp>
#include <objects/seqres/Int_graph.hpp>
#include <objects/seqres/Seq_graph.hpp>
#include <objects/pub/Pub.hpp>
#include <objects/biblio/Cit_art.hpp>
#include <objects/biblio/Auth_list.hpp>
#include <objects/biblio/Affil.hpp>
#include <objtools/unit_test_util/unit_test_util.hpp>

#include <misc/discrepancy_report/hDiscRep_tests.hpp>
#include <misc/discrepancy_report/hDiscRep_output.hpp>
#include <misc/discrepancy_report/hDiscRep_config.hpp>
#include <misc/discrepancy_report/hUtilib.hpp>


#include <misc/discrepancy_report/analysis_workflow.hpp>

#include <objects/submit/Contact_info.hpp>
#include <objects/biblio/Author.hpp>

// This header must be included before all Boost.Test headers if there are any
#include <corelib/test_boost.hpp>

USING_NCBI_SCOPE;
USING_SCOPE(objects);
USING_SCOPE(DiscRepNmSpc);
USING_SCOPE(unit_test_util);

const char* sc_TestEntryCollidingLocusTags ="Seq-entry ::= seq {\
     id {\
       local str \"LocusCollidesWithLocusTag\" } ,\
     inst {\
       repr raw ,\
       mol dna ,\
       length 24 ,\
       seq-data\
         iupacna \"AATTGGCCAANNAATTGGCCAANN\" } ,\
     annot {\
       {\
         data\
           ftable {\
             {\
               data\
                 gene {\
                   locus \"foo\" ,\
                   locus-tag \"foo\" } ,\
               location\
                 int {\
                   from 0 ,\
                   to 4 ,\
                   strand plus ,\
                   id\
                     local str \"LocusCollidesWithLocusTag\" } } ,\
             {\
               data\
                 gene {\
                   locus \"bar\" ,\
                   locus-tag \"foo\" } ,\
               location\
                 int {\
                   from 5 ,\
                   to 9 ,\
                   strand plus ,\
                   id\
                     local str \"LocusCollidesWithLocusTag\" } } ,\
             {\
               data\
                 gene {\
                   locus \"bar\" ,\
                   locus-tag \"baz\" } ,\
               location\
                 int {\
                   from 10 ,\
                   to 14 ,\
                   strand plus ,\
                   id\
                     local str \"LocusCollidesWithLocusTag\" } } ,\
             {\
               data\
                 gene {\
                   locus \"quux\" ,\
                   locus-tag \"baz\" } ,\
               location\
                 int {\
                   from 15 ,\
                   to 19 ,\
                   strand plus ,\
                   id\
                     local str \"LocusCollidesWithLocusTag\" } } } } } }\
 ";

static CRef <CRepConfig> config(new CRepConfig);

NCBITEST_AUTO_INIT()
{
    // Your application initialization code here (optional)
    cout << "Initialization function executed" << endl;
    config->InitParams(0);
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


CRef<CClickableItem> RunOneTest(CSeq_entry& entry, const string& test_name)
{
   CRef<CScope> scope(new CScope(*CObjectManager::GetInstance()));
   CSeq_entry_Handle seh = scope->AddTopLevelSeqEntry(entry);
   config->SetTopLevelSeqEntry(&seh);

   config->SetArg("e", test_name);
   config->CollectTests();
   config->Run();
   CDiscRepOutput output_obj;

   CRef<CClickableItem> c_item(new CClickableItem());

   if (test_name == "SUSPECT_PHRASES") {
      output_obj.Export(*c_item, "SUSPECT_PRODUCT_NAMES");
   }
   else { 
      output_obj.Export(*c_item, test_name);
   }
  
   return c_item;
}


void CheckOneItem(CClickableItem& c_item, const string& msg, bool expect_fatal, size_t num_items = 0, size_t num_subcat = 0, bool check_item_and_subcat = false)
{
   NCBITEST_CHECK_MESSAGE(c_item.item_list.size() == c_item.obj_list.size(),
              "The sizes of item_list and obj_list are not equal");

   BOOST_CHECK_EQUAL(c_item.description, msg);
   if (check_item_and_subcat) {
       BOOST_CHECK_EQUAL(c_item.item_list.size(), num_items);
       BOOST_CHECK_EQUAL(c_item.subcategories.size(), num_subcat);
   }

   BOOST_CHECK_EQUAL(CDiscRepOutput::IsFatal(c_item, true, true), expect_fatal);
}


void RunAndCheckTest(CRef <CSeq_entry> entry, const string& test_name, const string& msg, bool expect_fatal = false)
{
    CRef<CClickableItem> c_item = RunOneTest(*entry, test_name);

   NCBITEST_CHECK_MESSAGE(
    !(c_item->description).empty() || test_name=="FIND_DUP_TRNAS", "no report");
   if (msg == "print") {
     if ( test_name != "FIND_DUP_TRNAS") {
          cerr << "desc " << c_item->description << endl;
     }
     return;
   }

   CheckOneItem(*c_item, msg, expect_fatal);
};

void RunTest(CClickableItem& c_item, const string& setting_name)
{
   config->CollectTests();
   config->Run();
   CDiscRepOutput output_obj;
   output_obj.Export(c_item, setting_name);
};

CRef <CSeq_entry> BuildGoodRnaSeq()
{
   CRef <CSeq_entry> entry = BuildGoodSeq();
   entry->SetSeq().SetInst().SetMol(CSeq_inst::eMol_rna);
   return entry; 
};

CRef <CSeq_feat> MakeRNAFeatWithExtName(CRef <CSeq_entry> nuc_entry, CRNA_ref::EType type, const string ext_name)
{
  CRef <CSeq_feat> rna_feat(new CSeq_feat);
  if (!ext_name.empty()) {
     CRef <CRNA_ref::C_Ext> rna_ext (new CRNA_ref::C_Ext);
     rna_ext->SetName(ext_name);
     rna_feat->SetData().SetRna().SetExt(*rna_ext);
  }
  rna_feat->SetData().SetRna().SetType(type);
  rna_feat->SetLocation().SetInt().SetId().Assign(*(nuc_entry->GetSeq().GetId().front()));
  rna_feat->SetLocation().SetInt().SetFrom(0);
  rna_feat->SetLocation().SetInt().SetTo(nuc_entry->GetSeq().GetInst().GetLength()-1);
  return rna_feat;
};


CRef <CSeq_feat> MakeNewFeat(CRef <CSeq_entry> entry, CSeqFeatData::E_Choice choice = CSeqFeatData::e_not_set, CSeqFeatData::ESubtype = CSeqFeatData::eSubtype_bad, int fm = -1, int to = -1);
CRef <CSeq_feat> MakeNewFeat(CRef <CSeq_entry> entry, CSeqFeatData::E_Choice choice, CSeqFeatData::ESubtype subtype, int fm, int to)
{
   CRef <CSeq_feat> new_feat (new CSeq_feat);
   switch (choice) {
     case CSeqFeatData :: e_Cdregion:
         new_feat->SetData().SetCdregion();
         break;
     case CSeqFeatData :: e_Biosrc:
         new_feat->SetData().SetBiosrc();
     case CSeqFeatData :: e_Gene:
         new_feat->SetData().SetGene();
     default: break;
   }

   switch(subtype) {
     case CSeqFeatData::eSubtype_primer_bind:
        new_feat->SetData().SetImp().SetKey("primer_bind");
        break;
     case CSeqFeatData::eSubtype_exon:
        new_feat->SetData().SetImp().SetKey("exon");
        break;
     case CSeqFeatData::eSubtype_rRNA:
        new_feat->SetData().SetRna().SetType(CRNA_ref::eType_rRNA);
        break;
     case CSeqFeatData::eSubtype_tRNA:
        new_feat->SetData().SetRna().SetType(CRNA_ref::eType_tRNA);
        break;
     case CSeqFeatData::eSubtype_mRNA:
        new_feat->SetData().SetRna().SetType(CRNA_ref::eType_mRNA);
     default: break;        
 
   }
   new_feat->SetLocation().SetInt().SetId().Assign(*(entry->GetSeq().GetId().front()));
   fm = (fm < 0) ? 0 : fm;
   to = (to < 0) ? (entry->GetSeq().GetInst().GetLength()-1) : to;
   new_feat->SetLocation().SetInt().SetFrom(fm);
   new_feat->SetLocation().SetInt().SetTo(to);
   return new_feat;
};

CRef <CSeq_feat> MakeCDs(CRef <CSeq_entry> entry, int fm = -1, int to = -1);
CRef <CSeq_feat> MakeCDs(CRef <CSeq_entry> entry, int fm, int to)
{
   return(MakeNewFeat(entry, CSeqFeatData::e_Cdregion, 
                         CSeqFeatData::eSubtype_bad, fm, to));
};

void AddBioSource(CRef <CSeq_entry> entry, CBioSource::EGenome genome)
{
   entry->SetSeq().SetDescr().Set().front()->SetSource().SetGenome(genome);
};

void RunAndCheckMultiReports(CRef <CSeq_entry> entry, const string& test_name, const set <string>& msgs)
{
   CRef<CScope> scope(new CScope(*CObjectManager::GetInstance()));
   CSeq_entry_Handle seh = scope->AddTopLevelSeqEntry(*entry);
   config->SetTopLevelSeqEntry(&seh);

   config->SetArg("e", test_name);
   vector <CRef <CClickableItem> > c_items;
   config->CollectTests();
   config->Run();
   CDiscRepOutput output_obj;
   output_obj.Export(c_items, test_name);
   if (msgs.find("print") != msgs.end()) {
      ITERATE (vector <CRef <CClickableItem> > , it, c_items) {
        cerr << "desc " << (*it)->description << endl;
      }
      return;
   }
   NCBITEST_CHECK_MESSAGE(!c_items.empty(), "no report");
   if (c_items.size() != msgs.size()) {
      ITERATE(vector <CRef <CClickableItem> >, it, c_items) {
         cerr << "desc " << (*it)->description << endl;
      }
      NCBITEST_CHECK_MESSAGE(c_items.size() == msgs.size(), 
                              "wrong report: report size is in correct");
   }
   else {
     ITERATE (vector <CRef <CClickableItem> >, it, c_items) {
        if ( (*it)->description.find("should have segment qualifier but do not")
             != string::npos) {
           NCBITEST_CHECK_MESSAGE(
              (*it)->item_list.size() == (*it)->obj_list.size(),
               "The sizes of item_list and obj_list are not equal");
        }
        NCBITEST_CHECK_MESSAGE(msgs.find((*it)->description) != msgs.end(),
              "Test report is incorrect: " + (*it)->description);
     }
   }
};

void LookAndSave(CRef <CSeq_entry> entry, const string& file)
{
   cerr << MSerial_AsnText << *entry << endl;
   OutBlob(*entry, "data4tests/" + file);
};

BOOST_AUTO_TEST_CASE(TEST_ORGANELLE_PRODUCTS)
{
   CRef <CSeq_entry> entry = BuildGoodNucProtSet();
   NON_CONST_ITERATE (list <CRef <CSeqdesc> >, it, entry->SetDescr().Set()) {
     if ( (*it)->IsSource()) {
       (*it)->SetSource().SetGenome(CBioSource::eGenome_proviral);
     }
   }
   SetLineage(entry, "not_Bac_not_Vir");

   CRef <CSeq_entry> prot_entry = GetProteinSequenceFromGoodNucProtSet(entry);
   prot_entry->SetSeq().SetAnnot().front()->SetData().SetFtable().front()->SetData().SetProt().SetName().push_back("ATP synthase F0F1 subunit alpha");
  
   RunAndCheckTest(entry, "TEST_ORGANELLE_PRODUCTS", 
                              "1 suspect products is not organelled.");
};

BOOST_AUTO_TEST_CASE(COUNT_PROTEINS)
{
   CRef <CSeq_entry> entry = BuildGoodGenProdSet();
   RunAndCheckTest(entry, "COUNT_PROTEINS", "1 protein sequence in record");
};

BOOST_AUTO_TEST_CASE(TEST_TERMINAL_NS)
{
   CRef <CSeq_entry> entry = BuildGoodSeq();
   entry->SetSeq().SetInst().SetSeq_data().SetIupacna().Set("NATTGGCCAAAATTGGCCAAAATTGGCCAAAATTGGCCAAAATTGGCCAAAATTGGCCAA");
   RunAndCheckTest(entry, "TEST_TERMINAL_NS",  "1 sequence has terminal Ns");
};

BOOST_AUTO_TEST_CASE(DISC_QUALITY_SCORES)
{
   CRef <CSeq_entry> entry = BuildGoodSeq();
   CRef <CSeq_annot> graph = BuildGoodGraphAnnot("Phrap Graph");
   graph->SetData().SetGraph().front()->SetNumval(10);
   CRef <CInt_graph> int_gph (new CInt_graph);
   int_gph->SetMax(9);
   int_gph->SetMin(1);
   int_gph->SetAxis(0); 
   int_gph->SetValues().push_back(1);
   int_gph->SetValues().push_back(1);
   int_gph->SetValues().push_back(2);
   int_gph->SetValues().push_back(3);
   int_gph->SetValues().push_back(4);
   int_gph->SetValues().push_back(5);
   int_gph->SetValues().push_back(6);
   int_gph->SetValues().push_back(7);
   int_gph->SetValues().push_back(8);
   int_gph->SetValues().push_back(9);
   graph->SetData().SetGraph().front()->SetGraph().SetInt(*int_gph);
   entry->SetSeq().SetAnnot().push_back(graph);
   RunAndCheckTest(entry, "DISC_QUALITY_SCORES", 
                      "Quality scores are present on all sequences.");

   CRef <CSeq_entry> entry2 = BuildGoodSeq();
   CRef <CSeq_feat> src= AddGoodSourceFeature(entry2);
   RunAndCheckTest(entry2, "DISC_QUALITY_SCORES", 
                       "Quality scores are missing on all sequences.");

   src.Reset(AddGoodSourceFeature(entry));
   CRef <CSeq_entry> set_entry (new CSeq_entry);
   ChangeId(entry, "good too");
   set_entry->SetSet().SetSeq_set().push_back(entry);
   set_entry->SetSet().SetSeq_set().push_back(entry2);
   RunAndCheckTest(set_entry, "DISC_QUALITY_SCORES", 
                       "Quality scores are missing on some sequences.", true);
};

BOOST_AUTO_TEST_CASE(DISC_MISSING_DEFLINES)
{
   CRef <CSeq_entry> entry = BuildGoodSeq();
   RunAndCheckTest(entry, "DISC_MISSING_DEFLINES", 
                         "1 bioseq has no definition line");
};

void AddToSeqSubmitForSubmitBlkConflict(CRef <CSeq_submit> seq_submit, string id)
{
   CRef <CSubmit_block> submit_blk(new CSubmit_block);

   submit_blk->SetContact().SetEmail("a@x.com");
   CRef <CAuthor> auth (new CAuthor);
   CRef <CPerson_id> name (new CPerson_id);
   name->SetName().SetLast("a");
   name->SetName().SetFirst("a");
   name->SetName().SetInitials("a");
   auth->SetName(*name);
   CRef <CAffil> affil (new CAffil); 
   affil->SetStd().SetAffil("a");
   affil->SetStd().SetCity("a");
   affil->SetStd().SetCountry("a");
   auth->SetAffil(*affil);
   submit_blk->SetContact().SetContact(*auth);

   CRef <CCit_sub> cit(new CCit_sub);
   CRef <CAuth_list> auths(new CAuth_list);
   auth.Reset(new CAuthor);
   auth->SetName(*name);
   auths->SetNames().SetStd().push_back(auth);
   auths->SetAffil(*affil);
   cit->SetAuthors(*auths);
   cit->SetDate().SetStd().SetYear(2013);
   cit->SetDate().SetStd().SetMonth(2);
   cit->SetDate().SetStd().SetDay(29);
   submit_blk->SetCit(*cit);

   seq_submit->SetSub(*submit_blk);

   CRef <CSeq_entry> entry = BuildGoodSeq();
   CRef <CSeq_id> seq_id(new CSeq_id);
   seq_id->SetLocal().SetStr("good" + id);
   ChangeId(entry, seq_id);
   seq_submit->SetData().SetEntrys().push_back(entry);
};

BOOST_AUTO_TEST_CASE(DISC_SUBMITBLOCK_CONFLICT)
{
    vector <CConstRef <CObject> > strs;

    CRef <CSeq_submit> seq_submit (new CSeq_submit);
    AddToSeqSubmitForSubmitBlkConflict(seq_submit, "1");
    strs.push_back(CConstRef <CObject>(seq_submit.GetPointer()));

    seq_submit.Reset(new CSeq_submit);
    AddToSeqSubmitForSubmitBlkConflict(seq_submit, "2");
    // set hup
    seq_submit->SetSub().SetHup(true);
    strs.push_back(CConstRef <CObject>(seq_submit.GetPointer()));
   
    seq_submit.Reset(new CSeq_submit);
    AddToSeqSubmitForSubmitBlkConflict(seq_submit, "3");
    // change date:
    seq_submit->SetSub().SetCit().SetDate().SetStd().SetDay(20);
    strs.push_back(CConstRef <CObject>(seq_submit.GetPointer()));

    // entry no seq-submit
    CRef <CSeq_entry> entry = BuildGoodSeq();
    CRef <CSeq_id> seq_id (new CSeq_id);
    seq_id->SetLocal().SetStr("good4");
    ChangeId(entry, seq_id);
    strs.push_back(CConstRef <CObject> (entry.GetPointer()));
    config->SetMultiObjects(&strs);
    config->SetArg("e", "DISC_SUBMITBLOCK_CONFLICT");

    config->CollectTests();
    config->RunMultiObjects(); 
    CDiscRepOutput output_obj;
    CClickableItem c_item;
    output_obj.Export(c_item, "DISC_SUBMITBLOCK_CONFLICT");
  
    bool has_report 
          = !(c_item.description.empty()) && !(c_item.subcategories.empty());
    NCBITEST_CHECK_MESSAGE(has_report, "no report"); 
    NCBITEST_CHECK_MESSAGE(c_item.description == "SubmitBlock Conflicts",
                           "Test report is incorrect: " + c_item.description);
    unsigned i=0;
    string strtmp;
    ITERATE (vector <CRef <CClickableItem> >, it, c_item.subcategories) {
       NCBITEST_CHECK_MESSAGE(!(*it).Empty() && !((*it)->description).empty(),
                                "subcategory has no report");
       switch (i) {
         case 0: strtmp = "2 records have identical submit-blocks";
                 break;
         case 1: strtmp = "1 record has identical submit-block";
                 break;
         case 2: strtmp = "1 record has no submit-block";
                 break;
         default: break;
       } 
       NCBITEST_CHECK_MESSAGE((*it)->description == strtmp,
                          "subcategory has incorrect report: " + strtmp);
       NCBITEST_CHECK_MESSAGE((*it)->item_list.size() == (*it)->obj_list.size(),
              "The sizes of item_list and obj_list are not equal");
       i++;
    }
};


BOOST_AUTO_TEST_CASE(DISC_SEGSETS_PRESENT)
{
   CRef <CSeq_entry> entry = BuildGoodEcoSet();
   entry->SetSet().SetClass(CBioseq_set::eClass_segset);
   RunAndCheckTest(entry, "DISC_SEGSETS_PRESENT", "1 segset is present.", true);
};

BOOST_AUTO_TEST_CASE(TEST_SMALL_GENOME_SET_PROBLEM)
{
   CRef <CSeq_entry> entry = BuildGoodEcoSet();
   entry->SetSet().SetClass(CBioseq_set::eClass_small_genome_set);
   unsigned i=0;
   string i_str;
   NON_CONST_ITERATE (list <CRef <CSeq_entry> >, it, 
                             entry->SetSet().SetSeq_set()) {
     SetLineage( (*it), "Viruses");
     if (i) {
        SetSubSource( (*it), CSubSource::eSubtype_segment, "exist");
     }
     i_str = NStr::UIntToString(i+1);
     SetTaxname( (*it), "tax" + i_str);
     SetOrgMod((*it), COrgMod::eSubtype_isolate, "isolate" + i_str);
     SetOrgMod((*it), COrgMod::eSubtype_strain, "strain" + i_str);
     i++;
   }
   set <string> msgs;
   msgs.insert("Not all biosources have same isolate");
   msgs.insert("1 biosourc should have segment qualifier but does not");
   msgs.insert("Not all biosources have same strain");
   msgs.insert("Not all biosources have same taxname");
   RunAndCheckMultiReports(entry, "TEST_SMALL_GENOME_SET_PROBLEM", msgs); 
};


BOOST_AUTO_TEST_CASE(TEST_AMPLIFIED_PRIMERS_NO_ENVIRONMENTAL_SAMPLE)
{
   CRef <CSeq_entry> entry = BuildGoodRnaSeq();
   // biosource 1
   SetSubSource(entry, CSubSource::eSubtype_environmental_sample, "good");

   // biosrc 2
   CRef <CBioSource> new_src (new CBioSource);
   SetSubSource(*new_src, CSubSource::eSubtype_other, "note: amplified with species-specific primers");
   CRef <CSeqdesc> new_desc(new CSeqdesc);
   new_desc->SetSource(*new_src);
   entry->SetSeq().SetDescr().Set().push_back(new_desc);
  
   // biosrc 3
   new_src.Reset(new CBioSource);
   SetOrgMod(*new_src, COrgMod::eSubtype_other, "note: amplified with species-specific primers");
   new_desc.Reset(new CSeqdesc);
   new_desc->SetSource(*new_src);
   entry->SetSeq().SetDescr().Set().push_back(new_desc);

   RunAndCheckTest(entry, "TEST_AMPLIFIED_PRIMERS_NO_ENVIRONMENTAL_SAMPLE", 
    "2 biosources have 'amplified with species-specific primers' note but no environmental-sample qualifier.");
};

BOOST_AUTO_TEST_CASE(DISC_METAGENOME_SOURCE)
{
   CRef <CSeq_entry> entry = BuildGoodRnaSeq();
   SetOrgMod(entry, COrgMod::eSubtype_metagenome_source, "good");
   RunAndCheckTest(entry, "DISC_METAGENOME_SOURCE",
                     "1 biosource has metagenome_source qualifier");
};

BOOST_AUTO_TEST_CASE(DISC_METAGENOMIC)
{
   CRef <CSeq_entry> entry = BuildGoodRnaSeq();
   SetSubSource(entry, CSubSource::eSubtype_metagenomic, "good");
   RunAndCheckTest(entry, "DISC_METAGENOMIC", 
                     "1 biosource has metagenomic qualifier");
};

BOOST_AUTO_TEST_CASE(DISC_TITLE_ENDS_WITH_SEQUENCE)
{
   CRef <CSeq_entry> entry = BuildGoodRnaSeq();
   entry->SetSeq().SetDescr().Set().front()->SetTitle("DISC_TITLE_ENDS_WITH_SEQUENCE: ACTGACTGACTGACTGACTGACTG");
   RunAndCheckTest(entry, "DISC_TITLE_ENDS_WITH_SEQUENCE",
                    "1 defline appears to end with sequence characters");
};

BOOST_AUTO_TEST_CASE(ONCALLER_SUSPECTED_ORG_COLLECTED)
{
   CRef <CSeq_entry> entry = BuildGoodRnaSeq();
   SetSubSource(entry, CSubSource::eSubtype_collected_by, "money");
   SetTaxname(entry, "Homo sapiens");
   RunAndCheckTest(entry, "ONCALLER_SUSPECTED_ORG_COLLECTED", 
                       "1 biosource has collected-by and suspect organism"); 
};

BOOST_AUTO_TEST_CASE(END_COLON_IN_COUNTRY)
{
   CRef <CSeq_entry> entry = BuildGoodRnaSeq();
   SetSubSource(entry, CSubSource::eSubtype_country, "USA:");
   CRef <CSeq_feat> source = MakeNewFeat(entry, CSeqFeatData::e_Biosrc);
   SetSubSource(source->SetData().SetBiosrc(), CSubSource::eSubtype_country, "China:");
   AddFeat(source, entry);
   RunAndCheckTest(entry, "END_COLON_IN_COUNTRY", 
                      "2 country sources end with a colon.");
};

BOOST_AUTO_TEST_CASE(DISC_POSSIBLE_LINKER)
{
   CRef <CSeq_entry> entry = BuildGoodRnaSeq();
   SetBiomol(entry, CMolInfo::eBiomol_mRNA);
   entry->SetSeq().SetInst().SetSeq_data().SetIupacna().Set("AATTGGCCAAAATTGGCCAAAATTGGCCAAAATTAAAAAAAAAAAAAAAAAAAAAAATCG");
   entry->SetSeq().SetInst().SetLength(60);
   RunAndCheckTest(entry, "DISC_POSSIBLE_LINKER", 
                    "1 bioseq may have linker sequence after the poly-A tail");
};

BOOST_AUTO_TEST_CASE(NON_RETROVIRIDAE_PROVIRAL)
{
   CRef <CSeq_entry> entry = BuildGoodSeq(); // dna
   SetLineage(entry, "Eukaryota");
   NON_CONST_ITERATE (list <CRef <CSeqdesc> >, it, entry->SetDescr().Set()) {
     if ( (*it)->IsSource()) {
       (*it)->SetSource().SetGenome(CBioSource::eGenome_proviral);
     }
   }
   RunAndCheckTest(entry, "NON_RETROVIRIDAE_PROVIRAL", 
                     "1 non-Retroviridae biosource is proviral.");
};

BOOST_AUTO_TEST_CASE(RNA_PROVIRAL)
{
   CRef <CSeq_entry> entry = BuildGoodRnaSeq();
   AddBioSource(entry, CBioSource::eGenome_proviral);
   RunAndCheckTest(entry, "RNA_PROVIRAL", "1 RNA bioseq is proviral");
};

BOOST_AUTO_TEST_CASE(TEST_EXON_ON_MRNA)
{
   // mRNA
   CRef <CSeq_entry> entry = BuildGoodRnaSeq();
   SetBiomol(entry, CMolInfo::eBiomol_mRNA);

   CRef <CSeq_feat> 
      exon = MakeNewFeat(entry, (CSeqFeatData::E_Choice)0, CSeqFeatData::eSubtype_exon);
   AddFeat(exon, entry);
   RunAndCheckTest(entry, "TEST_EXON_ON_MRNA", 
                    "1 mRNA bioseq has exon features");
};

BOOST_AUTO_TEST_CASE(TEST_BAD_MRNA_QUAL)
{
   // mRNA
   CRef <CSeq_entry> entry = BuildGoodRnaSeq();
   SetBiomol(entry, CMolInfo::eBiomol_mRNA);

   SetSubSource(entry, CSubSource::eSubtype_germline, "normal");
   SetSubSource(entry, CSubSource::eSubtype_rearranged, "normal");
   RunAndCheckTest(entry, "TEST_BAD_MRNA_QUAL", 
                     "1 mRNA sequence has germline or rearranged qualifier");
};

BOOST_AUTO_TEST_CASE(TEST_MRNA_SEQUENCE_MINUS_STRAND_FEATURES)
{
   CRef <CSeq_entry> entry = BuildGoodRnaSeq();
   SetBiomol(entry, CMolInfo::eBiomol_mRNA);
   
   // cd
   CRef <CSeq_feat> cds = MakeCDs(entry);
   cds->SetLocation().SetInt().SetStrand(eNa_strand_minus);
   AddFeat(cds, entry);
   RunAndCheckTest(entry, "TEST_MRNA_SEQUENCE_MINUS_STRAND_FEATURES", 
                      "1 mRNA sequence has features on the complement strand.");
};


CRef <CSeq_feat> AddCDsWithComment(CRef <CSeq_entry> entry, const string& comment, int fm= -1, int to = -1);
CRef <CSeq_feat> AddCDsWithComment(CRef <CSeq_entry> entry, const string& comment, int fm, int to)
{
   CRef <CSeq_feat> cds = MakeCDs(entry, fm, to);
   cds->SetComment(comment);
   AddFeat(cds, entry);
   return cds;
};

BOOST_AUTO_TEST_CASE(MULTIPLE_CDS_ON_MRNA)
{
   CRef <CSeq_entry> entry = BuildGoodRnaSeq();
   SetBiomol(entry, CMolInfo::eBiomol_mRNA);
   unsigned len = entry->GetSeq().GetInst().GetLength();
   // cd1
   AddCDsWithComment(entry, 
        "comment: coding region disrupted by sequencing gap", 0, (int)(len/2));
   
   // cd2 
   AddCDsWithComment(entry, 
                        "comment: coding region disrupted by sequencing gap", 
                        (int)(len/2+1), len-1);

   RunAndCheckTest(entry, "MULTIPLE_CDS_ON_MRNA", 
                    "1 mRNA bioseq has multiple CDS features");
};

BOOST_AUTO_TEST_CASE(SHORT_SEQUENCES_200)
{
   CRef <CSeq_entry> entry = BuildGoodSeq();
   RunAndCheckTest(entry, "SHORT_SEQUENCES_200", 
                      "1 sequence is shorter than 200 bp.");
};

BOOST_AUTO_TEST_CASE(NON_GENE_LOCUS_TAG)
{
   CRef <CSeq_entry> entry = BuildGoodSeq();
   CRef <CSeq_feat> 
    rrna = MakeRNAFeatWithExtName(entry, CRNA_ref::eType_rRNA, "artificial");
   rrna->AddQualifier("locus_tag", "fake");
   AddFeat(rrna, entry);

   RunAndCheckTest(entry, "NON_GENE_LOCUS_TAG", 
                      "1 non-gene feature has locus tags.");
};

BOOST_AUTO_TEST_CASE(PSEUDO_MISMATCH)
{
   CRef <CSeq_entry> entry (new CSeq_entry);
   CNcbiIstrstream istr(sc_TestEntryCollidingLocusTags);
   istr >> MSerial_AsnText >> *entry;
   unsigned fm1, fm2, to1, to2;
   fm1 = entry->SetSeq().SetAnnot().front()->SetData().SetFtable().front()->SetLocation().SetInt().SetFrom();
   to1 = entry->SetSeq().SetAnnot().front()->SetData().SetFtable().front()->SetLocation().SetInt().SetTo();
   fm2 = entry->SetSeq().SetAnnot().front()->SetData().SetFtable().back()->SetLocation().SetInt().SetFrom();
   to2 = entry->SetSeq().SetAnnot().front()->SetData().SetFtable().back()->SetLocation().SetInt().SetTo();

   // rna
   CRef <CSeq_feat>
    rrna = MakeRNAFeatWithExtName(entry, CRNA_ref::eType_rRNA,
                      "Large Subunit Ribosomal RNA; lsuRNA; 23S ribosomal RNA");
   rrna->SetLocation().SetInt().SetFrom(fm1 + 1);
   rrna->SetLocation().SetInt().SetTo(to1);
   rrna->SetPseudo(true);
   AddFeat(rrna, entry);

   // cd
   CRef <CSeq_feat> cds = MakeCDs(entry, (int)fm2+2, (int)to2);
   cds->SetPseudo(true);
   AddFeat(cds, entry);

   RunAndCheckTest(entry, "PSEUDO_MISMATCH", 
                    "4 CDSs, RNAs, and genes have mismatching pseudos.", true);
};


BOOST_AUTO_TEST_CASE(FIND_DUP_RRNAS)
{
   CRef <CSeq_entry> entry (new CSeq_entry);
   CNcbiIstrstream istr(sc_TestEntryCollidingLocusTags);
   istr >> MSerial_AsnText >> *entry;
   CRef <CSeq_feat> 
    rrna = MakeRNAFeatWithExtName(entry, CRNA_ref::eType_rRNA, 
                      "Large Subunit Ribosomal RNA; lsuRNA; 23S ribosomal RNA");
   AddFeat(rrna, entry);

   rrna = MakeRNAFeatWithExtName(entry, CRNA_ref::eType_rRNA, 
                      "Large Subunit Ribosomal RNA; lsuRNA; 23S ribosomal RNA");
   AddFeat(rrna, entry);
   AddGoodSource(entry);
   AddBioSource(entry, CBioSource::eGenome_plastid);
   RunAndCheckTest(entry, "FIND_DUP_RRNAS", 
     "2 rRNA features on LocusCollidesWithLocusTag have the same name (Large Subunit Ribosomal RNA; lsuRNA; 23S ribosomal RNA)."); 
};

BOOST_AUTO_TEST_CASE(FIND_DUP_TRNAS)
{
   CRef <CSeq_entry> entry (new CSeq_entry);
   CNcbiIstrstream istr(sc_TestEntryCollidingLocusTags);
   istr >> MSerial_AsnText >> *entry;
   CRef <CSeq_feat> tRNA = BuildGoodtRNA(entry->GetSeq().GetId().front());
   AddFeat(tRNA, entry);
   tRNA = BuildGoodtRNA(entry->GetSeq().GetId().front());
   AddFeat(tRNA, entry);
   
   AddGoodSource(entry);
   AddBioSource(entry, CBioSource::eGenome_plastid);
  
   RunAndCheckTest(entry, "FIND_DUP_TRNAS", "print");
//  "2 tRNA features on LocusCollidesWithLocusTag have the same name (Phe).");
};

BOOST_AUTO_TEST_CASE(TEST_SHORT_LNCRNA)
{
   CRef <CSeq_entry> entry (new CSeq_entry);
   CNcbiIstrstream istr(sc_TestEntryCollidingLocusTags);
   istr >> MSerial_AsnText >> *entry;

   // make ncRNA with gen.class
   CRef <CSeq_feat> rna_feat(new CSeq_feat);
   CRef <CRNA_ref::C_Ext> rna_ext (new CRNA_ref::C_Ext);
   rna_ext->SetGen().SetClass("lncrna");
   rna_feat->SetData().SetRna().SetType(CRNA_ref::eType_ncRNA);
   rna_feat->SetData().SetRna().SetExt(*rna_ext);
   rna_feat->SetLocation().SetInt().SetId().Assign(*(entry->GetSeq().GetId().front()));
   rna_feat->SetLocation().SetInt().SetFrom(0);
   rna_feat->SetLocation().SetInt().SetTo(entry->GetSeq().GetInst().GetLength()-1);
   AddFeat(rna_feat, entry);
   RunAndCheckTest(entry, "TEST_SHORT_LNCRNA", 
                      "1 feature is suspiciously short");
};

BOOST_AUTO_TEST_CASE(DISC_SUSPECT_MISC_FEATURES)
{
   CRef <CSeq_entry> entry (new CSeq_entry);
   CNcbiIstrstream istr(sc_TestEntryCollidingLocusTags);
   istr >> MSerial_AsnText >> *entry;
   AddGoodImpFeat(entry, "misc_feature");
   entry->SetSeq().SetAnnot().front()->SetData().SetFtable().back()->SetComment("potential frameshift: catalytic intron");

   RunAndCheckTest(entry, 
                   "DISC_SUSPECT_MISC_FEATURES", 
                   "1 misc_feature comment contains suspect phrase");
};

BOOST_AUTO_TEST_CASE(DISC_MICROSATELLITE_REPEAT_TYPE)
{
   CRef <CSeq_entry> entry (new CSeq_entry);
   CNcbiIstrstream istr(sc_TestEntryCollidingLocusTags);
   istr >> MSerial_AsnText >> *entry;
   AddGoodImpFeat(entry, "repeat_region");
   entry->SetSeq().SetAnnot().front()->SetData().SetFtable().back()->AddQualifier("satellite", "microsatellite");
   entry->SetSeq().SetAnnot().front()->SetData().SetFtable().back()->AddQualifier("rpt_type", "not_tandem");
 
   RunAndCheckTest(entry, 
                   "DISC_MICROSATELLITE_REPEAT_TYPE",
                   "1 microsatellite does not have a repeat type of tandem", true);
};

BOOST_AUTO_TEST_CASE(DISC_RBS_WITHOUT_GENE)
{
   CRef <CSeq_entry> entry (new CSeq_entry);
   CNcbiIstrstream istr(sc_TestEntryCollidingLocusTags);
   istr >> MSerial_AsnText >> *entry;

   AddGoodImpFeat(entry, "RBS");
   int gene_fm = entry->GetSeq().GetAnnot().front()->GetData().GetFtable().front()->GetLocation().GetInt().GetFrom();
   int gene_to = entry->GetSeq().GetAnnot().front()->GetData().GetFtable().front()->GetLocation().GetInt().GetTo();
   entry->SetSeq().SetAnnot().front()->SetData().SetFtable().back()->SetLocation().SetInt().SetFrom(gene_fm + 1);
   entry->SetSeq().SetAnnot().front()->SetData().SetFtable().back()->SetLocation
().SetInt().SetTo(gene_to +10);

   RunAndCheckTest(entry, 
                    "DISC_RBS_WITHOUT_GENE",
                    "1 RBS feature does not have overlapping genes", true);
};

BOOST_AUTO_TEST_CASE(ONCALLER_HIV_RNA_INCONSISTENT)
{
   CRef <CSeq_entry> entry = BuildGoodRnaSeq();
   NON_CONST_ITERATE (list <CRef <CSeqdesc> > , it, entry->SetDescr().Set()) {
      if ((*it)->IsSource()) {
         (*it)->SetSource().SetGenome(CBioSource::eGenome_genomic);
      }
   }
   SetTaxname(entry, "Human immunodeficiency virus");
   SetBiomol(entry, CMolInfo::eBiomol_mRNA);

   RunAndCheckTest(entry, "ONCALLER_HIV_RNA_INCONSISTENT",
                   "1 HIV RNA bioseq has inconsistent location/moltype");
};

BOOST_AUTO_TEST_CASE(DISC_mRNA_ON_WRONG_SEQUENCE_TYPE)
{
   CRef <CSeq_entry> entry = BuildGoodSeq();  // dna, eBiomol_genomic
   SetLineage(entry, "Eukaryota");
   NON_CONST_ITERATE (list <CRef <CSeqdesc> >, it, entry->SetDescr().Set()) {
     if ( (*it)->IsSource()) {
       (*it)->SetSource().SetGenome(CBioSource::eGenome_kinetoplast);
     }
   }

   CRef <CSeq_feat> 
    new_mRNA = MakeRNAFeatWithExtName(entry, CRNA_ref::eType_mRNA, "fake");
   AddFeat(new_mRNA, entry);
   
   RunAndCheckTest(entry, "DISC_mRNA_ON_WRONG_SEQUENCE_TYPE",
       "1 mRNA is located on eukaryotic sequences that do not have genomic or plasmid source");
};

BOOST_AUTO_TEST_CASE(DISC_FEATURE_MOLTYPE_MISMATCH)
{
   // genomic rna
   CRef <CSeq_entry> entry = BuildGoodRnaSeq();

   // add rRNA
   CRef <CSeq_feat>
     new_rna = MakeRNAFeatWithExtName(entry, CRNA_ref::eType_rRNA,"5s_rRNA");
   AddFeat(new_rna, entry);
   RunAndCheckTest(entry, "DISC_FEATURE_MOLTYPE_MISMATCH",
          "1 sequence has rRNA or misc_RNA features but is not genomic DNA.");
 
   // changed to miscRNA
   entry->SetSeq().SetAnnot().front()->SetData().SetFtable().front()->SetData().SetRna().SetType(CRNA_ref::eType_miscRNA);
   RunAndCheckTest(entry, "DISC_FEATURE_MOLTYPE_MISMATCH",
          "1 sequence has rRNA or misc_RNA features but is not genomic DNA.");
   
   // changed to dna of eBiomol_unknown
   entry->SetSeq().SetInst().SetMol(CSeq_inst::eMol_dna);
   entry->SetSeq().SetDescr().Set().front()->SetMolinfo().SetBiomol(CMolInfo::eBiomol_unknown);
   RunAndCheckTest(entry, "DISC_FEATURE_MOLTYPE_MISMATCH",
          "1 sequence has rRNA or misc_RNA features but is not genomic DNA.");
};

BOOST_AUTO_TEST_CASE(DISC_SUSPECT_RRNA_PRODUCTS)
{
   CRef <CSeq_entry> entry = BuildGoodNucProtSet();

   // add rrna onto nucleotide sequence
   CRef <CSeq_entry> nuc_entry= GetNucleotideSequenceFromGoodNucProtSet(entry);
   CRef <CSeq_feat> 
    new_rna =MakeRNAFeatWithExtName(nuc_entry, CRNA_ref::eType_rRNA,"5s_rRNA");
   AddFeat(new_rna, nuc_entry);
   new_rna =MakeRNAFeatWithExtName(nuc_entry, CRNA_ref::eType_rRNA,"16s_rRNA");
   AddFeat(new_rna, nuc_entry);
   new_rna =MakeRNAFeatWithExtName(nuc_entry, CRNA_ref::eType_rRNA,"23s_rRNA");
   AddFeat(new_rna, nuc_entry);
   new_rna 
      = MakeRNAFeatWithExtName(nuc_entry, CRNA_ref::eType_rRNA,"partial 16S");
   AddFeat(new_rna, nuc_entry);
   new_rna 
    =MakeRNAFeatWithExtName(nuc_entry, CRNA_ref::eType_rRNA,"5.8S ribosomal RNA");
   AddFeat(new_rna, nuc_entry);

   // rna with "product" qual
   new_rna 
     = MakeNewFeat(nuc_entry, CSeqFeatData::e_not_set, CSeqFeatData::eSubtype_rRNA);
   new_rna->AddQualifier("product", 
                        "vacuolar protein domain sorting 26 homolog A");
   AddFeat(new_rna, nuc_entry);
   new_rna 
      = MakeRNAFeatWithExtName(nuc_entry, CRNA_ref::eType_rRNA, "tmRNA");
   new_rna->AddQualifier("product", "12S rRNA domain");
   AddFeat(new_rna, nuc_entry);
  
   // rrna for  ext.General.
   new_rna
     = MakeNewFeat(nuc_entry, CSeqFeatData::e_not_set, CSeqFeatData::eSubtype_rRNA);
   CRef <CRNA_ref::C_Ext> rna_ext(new CRNA_ref::C_Ext);
   rna_ext->SetGen().SetProduct("8S ribosomal RNA");
   new_rna->SetData().SetRna().SetExt(*rna_ext);
   AddFeat(new_rna, nuc_entry);

   RunAndCheckTest(entry, "DISC_SUSPECT_RRNA_PRODUCTS", 
                     "7 rRNA product names contain suspect phrase", true);
};

BOOST_AUTO_TEST_CASE(DISC_FLATFILE_FIND_ONCALLER)
{
   CRef <CSeq_entry> entry = unit_test_util::BuildGoodNucProtSet();
   CRef <CSeq_entry> prot_entry = entry->SetSet().SetSeq_set().back();
   prot_entry->SetSeq().SetAnnot().front()->SetData().SetFtable().front()->SetData().SetProt().SetName().push_back("fake protien name");
   RunAndCheckTest(entry, "DISC_FLATFILE_FIND_ONCALLER", "1 object contains protien");
};

BOOST_AUTO_TEST_CASE(DUP_GENPRODSET_PROTEIN)
{
   CRef <CSeq_entry> entry = unit_test_util::BuildGoodGenProdSet();

   // add cdregion
   CRef <CSeq_entry> 
     genomic_entry = unit_test_util::GetGenomicFromGenProdSet(entry); 
   const list <CRef <CSeq_annot> >& annots = genomic_entry->GetSeq().GetAnnot();
   CRef <CSeq_feat> new_feat(new CSeq_feat);
   ITERATE(list <CRef <CSeq_feat> >, it,
                        (*(annots.begin()))->GetData().GetFtable()) {
      if ( (*it)->GetData().IsCdregion()) {
         new_feat->SetData().SetCdregion().Assign(((*it)->GetData().GetCdregion()));
         new_feat->SetLocation().SetInt().SetId().Assign(*(genomic_entry->GetSeq().GetId().front()));
         new_feat->SetLocation().SetInt().SetFrom(0);
         new_feat->SetLocation().SetInt().SetTo(genomic_entry->GetSeq().GetInst().GetLength()-1);
         new_feat->SetProduct().Assign((*it)->GetProduct());
         unit_test_util::AddFeat(new_feat, genomic_entry );
         break;
      }
   }
   RunAndCheckTest(entry, "DUP_GENPRODSET_PROTEIN", 
                    "2 coding regions have protein ID prot");
};

BOOST_AUTO_TEST_CASE(MISSING_GENPRODSET_PROTEIN)
{
   CRef <CSeq_entry> entry = unit_test_util::BuildGoodGenProdSet();
   // reset Product of cdregion
   CRef <CSeq_entry>
     genomic_entry = unit_test_util::GetGenomicFromGenProdSet(entry);
   genomic_entry->SetSeq().SetAnnot().front()->SetData().SetFtable().front()->ResetProduct();
  RunAndCheckTest(entry, "MISSING_GENPRODSET_PROTEIN", 
                   "1 coding region has missing protein ID.");
};

BOOST_AUTO_TEST_CASE(MISSING_GENPRODSET_TRANSCRIPT_ID)
{
   CRef <CSeq_entry> entry = unit_test_util::BuildGoodGenProdSet();

   // good gen-prod-set
   CRef<CScope> scope(new CScope(*CObjectManager::GetInstance()));
   CSeq_entry_Handle seh = scope->AddTopLevelSeqEntry(*entry);
 
   config->SetTopLevelSeqEntry(&seh);
   config->SetArg("e", "MISSING_GENPRODSET_TRANSCRIPT_ID");
   CClickableItem c_item;
   RunTest(c_item, "MISSING_GENPRODSET_TRANSCRIPT_ID"); 
   NCBITEST_CHECK_MESSAGE(c_item.description.empty(), 
                           "Wrong report: " + c_item.description);

   // reset Product of mRNA
   CRef <CSeq_entry> 
        genomic_entry = unit_test_util::GetGenomicFromGenProdSet(entry);
   list <CRef <CSeq_annot> >& annots = genomic_entry->SetSeq().SetAnnot();
   NON_CONST_ITERATE(list <CRef <CSeq_feat> >, it, 
                        (*(annots.begin()))->SetData().SetFtable()) {
      if ( (*it)->SetData().GetSubtype() == CSeqFeatData::eSubtype_mRNA) {
         (*it)->ResetProduct(); 
         break;
      }
   }
   RunAndCheckTest(entry, "MISSING_GENPRODSET_TRANSCRIPT_ID",
                    "1 mRNA has missing transcript ID.");  
};

BOOST_AUTO_TEST_CASE(DISC_DUP_GENPRODSET_TRANSCRIPT_ID) 
{
   CRef <CSeq_entry> entry = unit_test_util::BuildGoodGenProdSet();
   CRef <CSeq_entry>
        genomic_entry = unit_test_util::GetGenomicFromGenProdSet(entry);
   CRef <CSeq_feat> new_feat(new CSeq_feat);
   ITERATE(list <CRef <CSeq_feat> >, it, 
         genomic_entry->GetSeq().GetAnnot().front()->GetData().GetFtable()){
      if ( (*it)->GetData().GetSubtype() == CSeqFeatData::eSubtype_mRNA) {
         new_feat->SetData().SetRna().Assign(((*it)->GetData().GetRna()));
         new_feat->SetLocation().SetInt().SetId().Assign(*(genomic_entry->GetSeq().GetId().front()));
         new_feat->SetLocation().SetInt().SetFrom(0);
         new_feat->SetLocation().SetInt().SetTo(genomic_entry->GetSeq().GetInst().GetLength()-1);
         new_feat->SetProduct().Assign((*it)->GetProduct());
         unit_test_util::AddFeat(new_feat, genomic_entry );
         break;
      }
   }
   RunAndCheckTest(entry, "DISC_DUP_GENPRODSET_TRANSCRIPT_ID", 
                   "2 mRNAs have non-unique transcript ID nuc");
};

BOOST_AUTO_TEST_CASE(MISSING_LOCUS_TAGS)
{
   CRef <CSeq_entry> entry (new CSeq_entry);
   CNcbiIstrstream istr(sc_TestEntryCollidingLocusTags);
   istr >> MSerial_AsnText >> *entry;

   CBioseq& seq = entry->SetSeq();
   list <CRef <CSeq_annot> >& annots = seq.SetAnnot();
   list <CRef <CSeq_feat> > & feats =(*(annots.begin()))->SetData().SetFtable();
   CGene_ref& gene_ref= (*(feats.begin()))->SetData().SetGene();
   string strtmp(kEmptyStr);
   gene_ref.SetLocus_tag("");
   RunAndCheckTest(entry, "MISSING_LOCUS_TAGS", "1 gene has no locus tags.", true);
};

BOOST_AUTO_TEST_CASE(DISC_COUNT_NUCLEOTIDES)
{
   CRef <CSeq_entry> entry (new CSeq_entry);
   CNcbiIstrstream istr(sc_TestEntryCollidingLocusTags);
   istr >> MSerial_AsnText >> *entry;
   RunAndCheckTest(entry, "DISC_COUNT_NUCLEOTIDES",
                        "1 nucleotide Bioseq is present.");
};

CRef <CSeq_id> MakeSeqIdWithDbAndIdStr(const string& db, const string& id)
{
   CRef <CObject_id> obj_id (new CObject_id);
   obj_id->SetStr(id);
   CRef <CDbtag> db_tag (new CDbtag);
   db_tag->SetDb(db);
   db_tag->SetTag(*obj_id);
   CRef <CSeq_id> seq_id (new CSeq_id(*db_tag));
   return seq_id;
};

BOOST_AUTO_TEST_CASE(MISSING_PROTEIN_ID)
{
   CRef <CSeq_entry> nuc_prot_set = BuildGoodNucProtSet();
   CRef <CSeq_entry> prot = GetProteinSequenceFromGoodNucProtSet(nuc_prot_set);

   // skippable db.
   CRef <CSeq_id> seq_id  = MakeSeqIdWithDbAndIdStr("TMSMART", "18938");
   prot->SetSeq().SetId().push_back(seq_id);
 
   seq_id = MakeSeqIdWithDbAndIdStr("BankIt", "12345");
   prot->SetSeq().SetId().push_back(seq_id);

   seq_id = MakeSeqIdWithDbAndIdStr("NCBIFILE", "67890");
   prot->SetSeq().SetId().push_back(seq_id);
   
   RunAndCheckTest(nuc_prot_set, "MISSING_PROTEIN_ID", "1 protein has invalid ID.", true);
};

BOOST_AUTO_TEST_CASE(INCONSISTENT_PROTEIN_ID)
{
   CRef <CSeq_entry> nuc_prot_set = BuildGoodNucProtSet();
   CRef <CSeq_entry> prot = GetProteinSequenceFromGoodNucProtSet(nuc_prot_set);
   // add seq_id
   CRef <CSeq_id> seq_id = MakeSeqIdWithDbAndIdStr("WGS:AFMK", "PVBG_08000T0");
   prot->SetSeq().SetId().push_back(seq_id);

   // add prot seq.
   CRef <CSeq_entry> prot2 = MakeProteinForGoodNucProtSet("prot2");
   seq_id = MakeSeqIdWithDbAndIdStr("WGS:AFNJ", "PVBG_08011T0");
   prot2->SetSeq().SetId().push_back(seq_id);
   nuc_prot_set->SetSet().SetSeq_set().push_back(prot2);

   set <string> msg;
   msg.insert("1 sequence has protein ID prefix WGS:AFMK.");
   msg.insert("1 sequence has protein ID prefix WGS:AFNJ.");
   RunAndCheckMultiReports(nuc_prot_set, "INCONSISTENT_PROTEIN_ID", msg);
};

BOOST_AUTO_TEST_CASE(TEST_DEFLINE_PRESENT)
{
   CRef <CSeq_entry> entry = BuildGoodSeq();
   CRef <CSeqdesc> title_desc (new CSeqdesc());
   title_desc->SetTitle("title is defline");
   entry->SetSeq().SetDescr().Set().push_back(title_desc);

   RunAndCheckTest(entry, "TEST_DEFLINE_PRESENT", 
                      "1 Bioseq has definition line");
};

void ChangeSequence(CRef <CSeq_entry> entry, const string& seq)
{
   entry->SetSeq().SetInst().SetSeq_data().SetIupacna().Set(seq);
   entry->SetSeq().SetInst().SetLength(seq.size());
};

BOOST_AUTO_TEST_CASE(N_RUNS)
{
   CRef <CSeq_entry> entry = BuildGoodSeq();
   ChangeSequence(entry, "AATTCCNNNNNNNNNNNNNNNNNNNNNAATTCC");
   RunAndCheckTest(entry, "N_RUNS", "1 sequence has runs of 10 or more Ns.");
};

BOOST_AUTO_TEST_CASE(N_RUNS_14)
{
   CRef <CSeq_entry> entry = BuildGoodSeq();
   ChangeSequence(entry, "AATTCCNNNNNNNNNNNNNNNNNNNNNAATTCC");
   RunAndCheckTest(entry, "N_RUNS_14", "1 sequence has runs of 15 or more Ns.");
};

BOOST_AUTO_TEST_CASE(ZERO_BASECOUNT)
{
   CRef <CSeq_entry> entry = BuildGoodSeq();
   ChangeSequence(entry, "XXYYZZNNNNNNNNNNNNNNNNNNNNNNNNNN");
   RunAndCheckTest(entry, "ZERO_BASECOUNT", 
                     "4 sequences have a zero basecount for a nucleotide");
};

BOOST_AUTO_TEST_CASE(TEST_LOW_QUALITY_REGION)
{
   CRef <CSeq_entry> entry = BuildGoodSeq();
   ChangeSequence(entry, "ACTGXXYYZZNNNNNNAAGCAGTGGTATCAACGCAGAGTGGCCACCGGGACAGACCCAGCAACAACCGTGTGCCCAGAGGGCTGCCAACATCTCTACACTTACTGCCGCTGTGATTTCTGCAGACCGCTTGTCTTCGTTGTGTGACA");
   RunAndCheckTest(entry, "TEST_LOW_QUALITY_REGION", 
                       "1 sequence contains low quality region");
};

BOOST_AUTO_TEST_CASE(DISC_PERCENT_N)
{
   CRef <CSeq_entry> entry = BuildGoodSeq();
   ChangeSequence(entry, "ACTGXXYYZZNNNNNNAAGCA");
   RunAndCheckTest(entry, "DISC_PERCENT_N", "1 sequence has > 5% Ns.");
};

BOOST_AUTO_TEST_CASE(DISC_10_PERCENTN)
{
   CRef <CSeq_entry> entry = BuildGoodSeq();
   ChangeSequence(entry, "ACTGXXYYZZNNNNNNAAGCA");
   RunAndCheckTest(entry, "DISC_10_PERCENTN", "1 sequence has > 10% Ns.");
};

BOOST_AUTO_TEST_CASE(TEST_UNUSUAL_NT)
{
   CRef <CSeq_entry> entry = BuildGoodSeq();
   ChangeSequence(entry, "ACTGXXYYZZNNNNNNAAGCA");
   RunAndCheckTest(entry, "TEST_UNUSUAL_NT", 
                  "1 sequence contains nucleotides that are not ATCG or N.");
};


void AddProtSeqWithDesc(CRef <CSeq_entry> entry, const string& name, const string& desc)
{
   CRef <CSeq_entry> prot_entry = MakeProteinForGoodNucProtSet(name);
   NON_CONST_ITERATE (list <CRef <CSeq_feat> >, fit,
              prot_entry->SetSeq().SetAnnot().front()->SetData().SetFtable()) {
       (*fit)->SetData().SetProt().SetDesc(desc);
   }
   entry->SetSet().SetSeq_set().push_back(prot_entry);
};

BOOST_AUTO_TEST_CASE(SUSPECT_PHRASES) 
{
   CRef <CSeq_entry> entry = BuildGoodNucProtSet();

   // prot1
   CRef <CSeq_entry> prot = GetProteinSequenceFromGoodNucProtSet(entry);
   NON_CONST_ITERATE (list <CRef <CSeq_feat> >, fit, 
                prot->SetSeq().SetAnnot().front()->SetData().SetFtable()) {
       (*fit)->SetData().SetProt().SetDesc("fragment");        
   }

   // add prot
   AddProtSeqWithDesc(entry, "prot2", "frameshift");
   AddProtSeqWithDesc(entry, "prot7", "...");

   
   // cds1
   NON_CONST_ITERATE (list <CRef <CSeq_feat> >, it, 
                    entry->SetSet().SetAnnot().front()->SetData().SetFtable()) {
      (*it)->SetComment("E-value");
   };

   CRef <CSeq_entry> nuc = GetNucleotideSequenceFromGoodNucProtSet(entry);
   // add cds
   AddCDsWithComment(nuc, "E value", 0, 10);
   AddCDsWithComment(nuc, "Evalue", 20, 30);
   AddCDsWithComment(nuc, "The RefSeq protein has 1 substitution and aligns at 50% coverage compared to this genomic sequence", 40, 50);
   RunAndCheckTest(entry, "SUSPECT_PHRASES", 
            "7 cds comments or protein descriptions contain suspect phrases.");
};

BOOST_AUTO_TEST_CASE(DISC_SPECVOUCHER_TAXNAME_MISMATCH)
{
   CRef <CSeq_entry> entry = BuildGoodNucProtSet();
   CRef <CSeq_entry> nuc_seq = GetNucleotideSequenceFromGoodNucProtSet(entry);
   CRef <CSeq_entry> prot_seq = GetProteinSequenceFromGoodNucProtSet(entry);

   // remove taxname at the seq-entry level
   NON_CONST_ITERATE (list <CRef <CSeqdesc> >, it, entry->SetDescr().Set()) {
     if ( (*it)->IsSource()) {
       (*it)->SetSource().SetOrg().ResetTaxname();
     }
   }

   // add biosoure, taxname, specvoucher
   AddGoodSource(nuc_seq);
   SetTaxname(nuc_seq, "Sebaea");
   SetOrgMod(nuc_seq, COrgMod::eSubtype_specimen_voucher, "fake");

   AddGoodSource(prot_seq);
   SetTaxname(prot_seq, "microphylla");
   SetOrgMod(prot_seq, COrgMod::eSubtype_specimen_voucher, "fake");

   // add prot seq.
   CRef <CSeq_entry> prot2 = MakeProteinForGoodNucProtSet("prot2");
   AddGoodSource(prot2);
   SetTaxname(prot2, "Sebaea microphylla");
   SetOrgMod(prot2, COrgMod::eSubtype_specimen_voucher, "s.n.fake");
   entry->SetSet().SetSeq_set().push_back(prot2);

   // port3
   CRef <CSeq_entry> prot3 = MakeProteinForGoodNucProtSet("prot3");
   AddGoodSource(prot3);
   SetTaxname(prot3, "Sebaea microphylla alternative");
   SetOrgMod(prot3, COrgMod::eSubtype_specimen_voucher, "s.n.fake");
   entry->SetSet().SetSeq_set().push_back(prot3);

   // port4
   CRef <CSeq_entry> prot4 = MakeProteinForGoodNucProtSet("prot4");
   AddGoodSource(prot4);
   SetTaxname(prot4, "Sebaea microphylla");
   SetOrgMod(prot4, COrgMod::eSubtype_specimen_voucher, "s.n.");
   entry->SetSet().SetSeq_set().push_back(prot4);

   // port5
   CRef <CSeq_entry> prot5 = MakeProteinForGoodNucProtSet("prot5");
   AddGoodSource(prot5);
   SetTaxname(prot5, "Sebaea microphylla alternative");
   SetOrgMod(prot5, COrgMod::eSubtype_specimen_voucher, "s.n.");
   entry->SetSet().SetSeq_set().push_back(prot5);

   RunAndCheckTest(entry, "DISC_SPECVOUCHER_TAXNAME_MISMATCH", 
             "4 BioSources have specimen voucher/taxname conflicts.");
};

BOOST_AUTO_TEST_CASE(DISC_BIOMATERIAL_TAXNAME_MISMATCH)
{
   CRef <CSeq_entry> entry = BuildGoodNucProtSet();
   CRef <CSeq_entry> nuc_seq = GetNucleotideSequenceFromGoodNucProtSet(entry);
   CRef <CSeq_entry> prot_seq = GetProteinSequenceFromGoodNucProtSet(entry);
   // add pro2
   CRef <CSeq_entry> prot2 = MakeProteinForGoodNucProtSet("prot2");
   entry->SetSet().SetSeq_set().push_back(prot2);

   // remove taxname at the seq-entry level
   NON_CONST_ITERATE (list <CRef <CSeqdesc> >, it, entry->SetDescr().Set()) {
     if ( (*it)->IsSource()) {
       (*it)->SetSource().SetOrg().ResetTaxname();
     }
   }

   // add biosoure, taxname, bio-material
   AddGoodSource(nuc_seq);
   SetTaxname(nuc_seq, "Oryza brachyantha");
   SetOrgMod(nuc_seq, COrgMod::eSubtype_bio_material, "IRGC:105720");

   AddGoodSource(prot_seq);
   SetTaxname(prot_seq, "Oryza brachyantha");
   SetOrgMod(prot_seq, COrgMod::eSubtype_bio_material, "IRGC:105720");

   AddGoodSource(prot2);
   SetTaxname(prot2, "Oryza granulata");
   SetOrgMod(prot2, COrgMod::eSubtype_bio_material, "IRGC:105720");

   RunAndCheckTest(entry, "DISC_BIOMATERIAL_TAXNAME_MISMATCH", 
      "3 biosources have biomaterial 'IRGC:105720' but do not have the same taxnames.");
};

BOOST_AUTO_TEST_CASE(DISC_CULTURE_TAXNAME_MISMATCH)
{
   CRef <CSeq_entry> entry = BuildGoodNucProtSet();
   CRef <CSeq_entry> nuc_seq = GetNucleotideSequenceFromGoodNucProtSet(entry);
   CRef <CSeq_entry> prot_seq = GetProteinSequenceFromGoodNucProtSet(entry);

   // remove taxname at the seq-entry level
   NON_CONST_ITERATE (list <CRef <CSeqdesc> >, it, entry->SetDescr().Set()) {
     if ( (*it)->IsSource()) {
       (*it)->SetSource().SetOrg().ResetTaxname();
     }
   }

   // add biosoure, taxname, culture_collection
   AddGoodSource(nuc_seq);
   SetTaxname(nuc_seq, "Pseudocercospora sp. CPC 1057");
   SetOrgMod(nuc_seq, COrgMod::eSubtype_culture_collection, "CBS:124990");

   AddGoodSource(prot_seq);
   SetTaxname(prot_seq, "Mycosphaerella acaciigena");
   SetOrgMod(prot_seq, COrgMod::eSubtype_culture_collection, "CBS:124990");

   RunAndCheckTest(entry, "DISC_CULTURE_TAXNAME_MISMATCH", 
 "2 biosources have culture collection 'CBS:124990' but do not have the same taxnames.");
};

BOOST_AUTO_TEST_CASE(DISC_STRAIN_TAXNAME_MISMATCH)
{
   CRef <CSeq_entry> entry = BuildGoodNucProtSet();
   CRef <CSeq_entry> nuc_seq = GetNucleotideSequenceFromGoodNucProtSet(entry);
   CRef <CSeq_entry> prot_seq = GetProteinSequenceFromGoodNucProtSet(entry);

   // remove taxname at the seq-entry level
   NON_CONST_ITERATE (list <CRef <CSeqdesc> >, it, entry->SetDescr().Set()) {
     if ( (*it)->IsSource()) {
       (*it)->SetSource().SetOrg().ResetTaxname();
     }
   }

   // add biosoure, taxname, strain
   AddGoodSource(nuc_seq);
   SetTaxname(nuc_seq, "Pseudocercospora humuli");
   SetOrgMod(nuc_seq, COrgMod::eSubtype_strain, "X1083");

   AddGoodSource(prot_seq);
   SetTaxname(prot_seq, "Pseudocercospora sp. GCH-2010a");
   SetOrgMod(prot_seq, COrgMod::eSubtype_strain, "X1083");

   RunAndCheckTest(entry, "DISC_STRAIN_TAXNAME_MISMATCH", 
      "2 biosources have strain 'X1083' but do not have the same taxnames.");
}

BOOST_AUTO_TEST_CASE(ADJACENT_PSEUDOGENES)
{
   CRef <CSeq_entry> entry = BuildGoodSeq();
   // gene1
   CRef <CSeq_feat> feat = BuildGoodFeat();
   CRef <CSeq_feat> gene_ft = MakeGeneForFeature(feat);
   gene_ft->SetLocation().SetInt().SetTo(20);
   gene_ft->SetPseudo(true);
   gene_ft->SetComment("disrupted");
   AddFeat(gene_ft, entry);

   // gene2
   feat = BuildGoodFeat();
   gene_ft = MakeGeneForFeature(feat);
   gene_ft->SetLocation().SetInt().SetFrom(21);
   gene_ft->SetLocation().SetInt().SetTo(30);
   gene_ft->SetPseudo(true);
   gene_ft->SetComment("disrupted");
   AddFeat(gene_ft, entry);

   // gene3
   feat = BuildGoodFeat();
   gene_ft = MakeGeneForFeature(feat);
   gene_ft->SetLocation().SetInt().SetFrom(31);
   gene_ft->SetPseudo(true);
   gene_ft->SetComment("disrupted");
   gene_ft->SetLocation().SetInt().SetStrand(eNa_strand_plus);
   AddFeat(gene_ft, entry);

   RunAndCheckTest(entry, "ADJACENT_PSEUDOGENES", "Adjacent pseudogenes have the same text: disrupted");
};

BOOST_AUTO_TEST_CASE(DISC_CHECK_RNA_PRODUCTS_AND_COMMENTS)
{
   CRef <CSeq_entry> entry = BuildGoodSeq();
   CRef <CSeq_feat> tRNA = BuildGoodtRNA(entry->GetSeq().GetId().front());
   tRNA->SetComment("comment: IMG reference gene:2506377740");
   
   AddFeat(tRNA, entry);

   CRef <CSeq_feat> 
         rRNA = MakeRNAFeatWithExtName(entry, CRNA_ref::eType_rRNA, "artificial");
   rRNA->SetData().SetRna().SetExt().SetGen().SetProduct("genes product");
   AddFeat(rRNA, entry);
   RunAndCheckTest(entry, "DISC_CHECK_RNA_PRODUCTS_AND_COMMENTS", 
          "2 RNA product_names or comments contain suspect phrase");
};

BOOST_AUTO_TEST_CASE(DISC_CDS_HAS_NEW_EXCEPTION)
{
  CRef <CSeq_entry> entry = BuildGoodSeq();
  CRef <CSeq_feat> cds = AddCDsWithComment(entry, "cd1", 0, 10);
  cds->SetExcept_text("Annotated by transcript or proteomic data");

  cds = AddCDsWithComment(entry, "cd2", 11, 20);
  cds->SetExcept_text("heterogeneous population sequenced");
 
  cds = AddCDsWithComment(entry, "cd3", 21, 30);
  cds->SetExcept_text("low-quality sequence");

  cds = AddCDsWithComment(entry, "cd4", 31, 40);
  cds->SetExcept_text("Unextendable Partial Coding Region");
  
  RunAndCheckTest(entry, "DISC_CDS_HAS_NEW_EXCEPTION", 
                      "3 coding regions have new exceptions");
};

BOOST_AUTO_TEST_CASE(TEST_CDS_HAS_CDD_XREF)
{
   CRef <CSeq_entry> entry = BuildGoodSeq();
   CRef <CSeq_feat> cds = AddCDsWithComment(entry, "cd1", 0, 10);
   SetDbxref(cds, "CDD", 12345);
   RunAndCheckTest(entry, "TEST_CDS_HAS_CDD_XREF", "1 feature has CDD Xrefs");
};

BOOST_AUTO_TEST_CASE(DISC_FEATURE_LIST)
{
   CRef <CSeq_entry> entry = BuildGoodNucProtSet();
   CRef <CSeq_entry> nuc = GetNucleotideSequenceFromGoodNucProtSet(entry);
   CRef <CSeq_feat> cds = entry->GetSet().GetAnnot().front()->GetData().GetFtable().front();
   // add mRNA
   CRef <CSeq_feat> feat = MakemRNAForCDS(cds);
   AddFeat(feat, nuc);

   // add gene
   feat = MakeGeneForFeature(cds);
   AddFeat(feat, nuc);

   // add RBS
   feat = AddGoodImpFeat(nuc, "RBS");

   // tRNA
   feat = BuildtRNA(nuc->GetSeq().GetId().front());
   AddFeat(feat, nuc);

   // add more virious features if have time
   RunAndCheckTest(entry, "DISC_FEATURE_LIST", "Feature List");
};

BOOST_AUTO_TEST_CASE(ONCALLER_ORDERED_LOCATION)
{
   CRef <CSeq_entry> entry = BuildGoodSeq();
   CRef <CSeq_id> seq_id = entry->GetSeq().GetId().front();

   CRef <CSeq_loc> mix_loc(new CSeq_loc());
   CRef <CSeq_loc> loc (new CSeq_loc());
   loc->SetInt().SetFrom(0);
   loc->SetInt().SetTo(10);
   loc->SetInt().SetId().Assign(*seq_id);
   mix_loc->SetMix().Set().push_back(loc);

   loc.Reset(new CSeq_loc(CSeq_loc::e_Null));
   mix_loc->SetMix().Set().push_back(loc);

   loc.Reset(new CSeq_loc);
   loc->SetInt().SetFrom(30);
   loc->SetInt().SetTo(40);
   loc->SetInt().SetId().Assign(*seq_id);
   mix_loc->SetMix().Set().push_back(loc);

   loc.Reset(new CSeq_loc(CSeq_loc::e_Null));
   mix_loc->SetMix().Set().push_back(loc);

   loc.Reset(new CSeq_loc);
   loc->SetInt().SetFrom(60);
   loc->SetInt().SetTo(70);
   loc->SetInt().SetId().Assign(*seq_id);
   mix_loc->SetMix().Set().push_back(loc);

   CRef <CSeq_feat> feat = AddGoodImpFeat(entry, "repeat_region");
   feat->ResetLocation();
   feat->SetLocation(*mix_loc);

   RunAndCheckTest(entry, "ONCALLER_ORDERED_LOCATION", "1 feature has ordered locations", true);
};

BOOST_AUTO_TEST_CASE(ONCALLER_HAS_STANDARD_NAME)
{
   CRef <CSeq_entry> entry = BuildGoodSeq();
   CRef <CSeq_feat> feat = AddGoodImpFeat(entry, "exon");
 
   CRef <CGb_qual> qual (new CGb_qual);
   qual->SetQual("standard_name");
   qual->SetVal("myexon");
   vector <CRef <CGb_qual> > quals;
   feat->SetQual() = quals;
   feat->SetQual().push_back(qual);

   RunAndCheckTest(entry, "ONCALLER_HAS_STANDARD_NAME", "1 feature has standard_name qualifier");
};


BOOST_AUTO_TEST_CASE(TEST_MRNA_OVERLAPPING_PSEUDO_GENE)
{
   CRef <CSeq_entry> entry = BuildGoodSeq();
   CRef <CSeq_feat> cds = AddCDsWithComment(entry, "cd1", 0, 50);

   // add mRNA
   CRef <CSeq_feat> feat = MakemRNAForCDS(cds);
   AddFeat(feat, entry);
   
   // add gene
   feat = MakeGeneForFeature(cds);
   feat->SetPseudo(true);
   AddFeat(feat, entry);
   
   RunAndCheckTest(entry, "TEST_MRNA_OVERLAPPING_PSEUDO_GENE", "1 Pseudogene has overlapping mRNAs.");
};

void MakeDeltaSeqWithoutGap(CRef <CSeq_entry> entry)
{
   entry->SetSeq().SetInst().ResetSeq_data();
   entry->SetSeq().SetInst().SetRepr(objects::CSeq_inst::eRepr_delta);
   entry->SetSeq().SetInst().SetExt().SetDelta().AddLiteral("ATGATGATGCCC", objects::CSeq_inst::eMol_dna);
   entry->SetSeq().SetInst().SetExt().SetDelta().AddLiteral("TGGCCAAAATTGGCCAAAATTGGCCAA", objects::CSeq_inst::eMol_dna);
   entry->SetSeq().SetInst().SetExt().SetDelta().AddLiteral("CCCATGATGATG", objects::CSeq_inst::eMol_dna);
   entry->SetSeq().SetInst().SetLength(49);
   
};


void MakeDeltaSeqWithGap(CRef <CSeq_entry> entry)
{
   entry->SetSeq().SetInst().ResetSeq_data();
   entry->SetSeq().SetInst().SetRepr(objects::CSeq_inst::eRepr_delta);
   entry->SetSeq().SetInst().SetExt().SetDelta().AddLiteral("PRKTEIN", objects::CSeq_inst::eMol_aa);

   // gap
   CRef<objects::CDelta_seq> gap_seg(new objects::CDelta_seq());
   gap_seg->SetLiteral().SetSeq_data().SetGap().SetType(CSeq_gap::eType_unknown);
   gap_seg->SetLiteral().SetLength(18);
   entry->SetSeq().SetInst().SetExt().SetDelta().Set().push_back(gap_seg);

   entry->SetSeq().SetInst().SetExt().SetDelta().AddLiteral("MGPRKTEIN", objects::CSeq_inst::eMol_aa);
   entry->SetSeq().SetInst().SetLength(34);

};

BOOST_AUTO_TEST_CASE(DISC_GAPS)
{
   CRef <CSeq_entry> entry = BuildGoodNucProtSet();
   CRef <CSeq_entry> nuc = GetNucleotideSequenceFromGoodNucProtSet(entry);
   MakeDeltaSeqWithoutGap(nuc);
   CRef <CSeq_feat> feat = AddGoodImpFeat(nuc, "gap");

   CRef <CSeq_entry> prot = GetProteinSequenceFromGoodNucProtSet(entry);
   MakeDeltaSeqWithGap(prot);

   RunAndCheckTest(entry, "DISC_GAPS", "2 sequences contain gaps");
};

BOOST_AUTO_TEST_CASE(NO_PRODUCT_STRING)
{
   CRef <CSeq_entry> entry = BuildGoodNucProtSet();
   CRef <CSeq_entry> prot = GetProteinSequenceFromGoodNucProtSet(entry);
   prot->SetSeq().SetAnnot().front()->SetData().SetFtable().front()->SetData().SetProt().ResetName();
   prot->SetSeq().SetAnnot().front()->SetData().SetFtable().front()->SetData().SetProt().SetName().push_back("no product string in file");

   RunAndCheckTest(entry, "NO_PRODUCT_STRING", "1 product has \"no product string in file\"");
};

BOOST_AUTO_TEST_CASE(FEATURE_LOCATION_CONFLICT)
{
   // !IsEukaryotic ()
   CRef <CSeq_entry> entry = BuildGoodSeq();
   SetLineage(entry, "Viruses; ssRNA positive-strand viruses, no DNA stage; Flaviviridae; Flavivirus; Dengue virus group");
   CRef <CSeq_feat> new_ft = MakeCDs(entry, 10, 20);
   AddFeat(new_ft, entry);
   new_ft = MakeNewFeat(entry, CSeqFeatData::e_Gene, CSeqFeatData::eSubtype_any, 5, 26);
   AddFeat(new_ft, entry);
   new_ft = MakeNewFeat(entry, CSeqFeatData::e_Gene, CSeqFeatData::eSubtype_any, 20, 50);
   AddFeat(new_ft, entry);
   new_ft = MakeNewFeat(entry, CSeqFeatData::e_not_set, CSeqFeatData::eSubtype_mRNA, 30, 40);
   AddFeat(new_ft, entry);
   new_ft = MakeNewFeat(entry, CSeqFeatData::e_Gene, CSeqFeatData::eSubtype_any, 27, 45);
   AddFeat(new_ft, entry);
   new_ft = MakeNewFeat(entry, CSeqFeatData::e_Gene, CSeqFeatData::eSubtype_any,35, 55);
   AddFeat(new_ft, entry);

   RunAndCheckTest(entry, "FEATURE_LOCATION_CONFLICT", 
                                 "2 features have inconsistent gene locations"); 

   // IsEukaryotic 
   entry = BuildGoodSeq();
   SetLineage(entry, "Eukaryota; Viridiplantae; Streptophyta;");
   new_ft = MakeCDs(entry, 10, 20);
   AddFeat(new_ft, entry);
   new_ft = MakeNewFeat(entry, CSeqFeatData::e_Gene, CSeqFeatData::eSubtype_any, 5, 26);
   AddFeat(new_ft, entry);
   new_ft = MakeNewFeat(entry, CSeqFeatData::e_Gene, CSeqFeatData::eSubtype_any, 20, 50);
   AddFeat(new_ft, entry);
   new_ft = MakeNewFeat(entry, CSeqFeatData::e_not_set, CSeqFeatData::eSubtype_mRNA, 30, 40);
   AddFeat(new_ft, entry);
   new_ft = MakeNewFeat(entry, CSeqFeatData::e_Gene, CSeqFeatData::eSubtype_any, 27, 45);
   AddFeat(new_ft, entry);
   new_ft = MakeNewFeat(entry, CSeqFeatData::e_Gene, CSeqFeatData::eSubtype_any,35, 55);
   AddFeat(new_ft, entry);

   RunAndCheckTest(entry, "FEATURE_LOCATION_CONFLICT", 
                           "1 feature has inconsistent gene locations");
};

BOOST_AUTO_TEST_CASE(BAD_LOCUS_TAG_FORMAT)
{
   CRef <CSeq_entry> entry = BuildGoodSeq();
   CRef <CSeq_feat> 
      feat = MakeNewFeat(entry, CSeqFeatData::e_Gene, CSeqFeatData::eSubtype_any, 10, 20);
   feat->SetData().SetGene().SetLocus_tag("HHV4_LMP-2A");
   AddFeat(feat, entry);
   feat = MakeNewFeat(entry, CSeqFeatData::e_Gene, CSeqFeatData::eSubtype_any, 25, 30);
   feat->SetData().SetGene().SetLocus_tag("HHV4_BCRF1.1");
   AddFeat(feat, entry);
   feat = MakeNewFeat(entry, CSeqFeatData::e_Gene, CSeqFeatData::eSubtype_any, 35, 40);
   feat->SetData().SetGene().SetLocus_tag("HHV4_EBNA-3B/EBNA-3C");
   AddFeat(feat, entry);

   RunAndCheckTest(entry, "BAD_LOCUS_TAG_FORMAT", "3 locus tags are incorrectly formatted.", true);
}

BOOST_AUTO_TEST_CASE(INCONSISTENT_LOCUS_TAG_PREFIX)
{
   CRef <CSeq_entry> entry = BuildGoodSeq();
   CRef <CSeq_feat> 
      feat = MakeNewFeat(entry, CSeqFeatData::e_Gene, CSeqFeatData::eSubtype_any, 10, 20);
   feat->SetData().SetGene().SetLocus_tag("HHV4_LMP-2A");
   AddFeat(feat, entry);
   feat = MakeNewFeat(entry, CSeqFeatData::e_Gene, CSeqFeatData::eSubtype_any, 25, 30);
   feat->SetData().SetGene().SetLocus_tag("HHV4_BCRF1.1");
   AddFeat(feat, entry);
   feat = MakeNewFeat(entry, CSeqFeatData::e_Gene, CSeqFeatData::eSubtype_any, 35, 40);
   feat->SetData().SetGene().SetLocus_tag("HHV4_EBNA-3B/EBNA-3C");
   AddFeat(feat, entry);
   feat = MakeNewFeat(entry, CSeqFeatData::e_Gene, CSeqFeatData::eSubtype_any, 45, 50);
   feat->SetData().SetGene().SetLocus_tag("Spy49_EBNA-3B/EBNA-3C");
   AddFeat(feat, entry);

   set <string> msgs;
   msgs.insert("3 features have locus tag prefix HHV4");
   msgs.insert("1 feature has locus tag prefix Spy49");
   RunAndCheckMultiReports(entry, "INCONSISTENT_LOCUS_TAG_PREFIX", msgs);
}

BOOST_AUTO_TEST_CASE(DUPLICATE_LOCUS_TAGS)
{
   CRef <CSeq_entry> entry (new CSeq_entry);
   // seq1
   CRef <CSeq_entry> seq = BuildGoodSeq();
   CRef <CSeq_id> id (new CSeq_id());
   entry->SetSet().SetSeq_set().push_back(seq); 
   id->SetLocal().SetStr("good nuc1");
   ChangeNucId(entry, id);
   CRef <CSeq_feat> 
        feat = MakeNewFeat(seq, CSeqFeatData::e_Gene, CSeqFeatData::eSubtype_any, 10, 20);
   feat->SetData().SetGene().SetLocus_tag("Spy49_EBNA-3B/EBNA-3C");
   AddFeat(feat, seq);
   feat = MakeNewFeat(seq, CSeqFeatData::e_Gene, CSeqFeatData::eSubtype_any, 25, 35);
   feat->SetData().SetGene().SetLocus_tag("Spy49_EBNA-3B/EBNA-3C");
   AddFeat(feat, seq);
   feat = MakeNewFeat(seq, CSeqFeatData::e_Gene, CSeqFeatData::eSubtype_any, 40, 50);
   feat->SetData().SetGene().SetLocus_tag("Spy50");
   AddFeat(feat, seq);
   feat = MakeNewFeat(seq, CSeqFeatData::e_Gene, CSeqFeatData::eSubtype_any, 55, 59);
   feat->SetData().SetGene().SetLocus_tag("Spy50");
   AddFeat(feat, seq);

   // seq2
   seq = BuildGoodSeq();
   feat = MakeNewFeat(seq, CSeqFeatData::e_Gene, CSeqFeatData::eSubtype_any, 30, 50);
   feat->SetData().SetGene().SetLocus_tag("Spy49_EBNA-3B/EBNA-3C");
   AddFeat(feat, seq);
   entry->SetSet().SetSeq_set().push_back(seq);

   RunAndCheckTest(entry, "DUPLICATE_LOCUS_TAGS", "5 genes have duplicate locus tags.");
   RunAndCheckTest(entry, "DUPLICATE_LOCUS_TAGS_global", "5 genes have duplicate locus tags.");
};

BOOST_AUTO_TEST_CASE(DUPLICATE_GENE_LOCUS)
{
  CRef <CSeq_entry> entry(new CSeq_entry);
   
  // set 1: IsmRNASequenceInGenProdSet
  CRef <CSeq_entry> gen_prot_set = BuildGoodGenProdSet();
  CRef <CSeq_entry> seq_entry = gen_prot_set->SetSet().SetSeq_set().front();
  seq_entry->SetSeq().SetInst().SetMol(CSeq_inst::eMol_rna); 
  seq_entry->SetSeq().SetDescr().Set().front()->SetMolinfo().SetBiomol(CMolInfo::eBiomol_mRNA);
  CRef <CSeq_feat> 
    feat = MakeNewFeat(seq_entry,CSeqFeatData::e_Gene,CSeqFeatData::eSubtype_any, 10, 20);
  feat->SetData().SetGene().SetLocus("Spy50");
  AddFeat(feat, seq_entry);
  feat = MakeNewFeat(seq_entry, CSeqFeatData::e_Gene, CSeqFeatData::eSubtype_any, 25, 35);
  feat->SetData().SetGene().SetLocus("Spy10");
  AddFeat(feat, seq_entry);
  feat = MakeNewFeat(seq_entry, CSeqFeatData::e_Gene, CSeqFeatData::eSubtype_any, 55, 59);
  feat->SetData().SetGene().SetLocus("Spy50");
  AddFeat(feat, seq_entry);

  // nuc seq of nuc_prot set
  CRef <CSeq_entry> nuc_prot_set = GetNucProtSetFromGenProdSet(gen_prot_set);
  seq_entry = GetNucleotideSequenceFromGoodNucProtSet(nuc_prot_set);
  feat = MakeNewFeat(seq_entry, CSeqFeatData::e_Gene, CSeqFeatData::eSubtype_any, 10, 20);
  feat->SetData().SetGene().SetLocus("Spy50");
  AddFeat(feat, seq_entry);
  feat = MakeNewFeat(seq_entry, CSeqFeatData::e_Gene, CSeqFeatData::eSubtype_any, 25, 35);
  feat->SetData().SetGene().SetLocus("Spy10");
  AddFeat(feat, seq_entry);
  feat = MakeNewFeat(seq_entry, CSeqFeatData::e_Gene, CSeqFeatData::eSubtype_any, 55, 59);
  feat->SetData().SetGene().SetLocus("Spy50");

  entry->SetSet().SetSeq_set().push_back(gen_prot_set);
   
  // 2nd set
  CRef <CSeq_id> id2 (new CSeq_id);
  id2->SetLocal().SetStr("good prot2");
  gen_prot_set = BuildGoodGenProdSet();
  seq_entry = gen_prot_set->SetSet().SetSeq_set().front();
  seq_entry->SetSeq().SetInst().SetMol(CSeq_inst::eMol_rna); 
  CRef <CSeq_id> id (new CSeq_id);
  id->SetLocal().SetStr("good2");
  ChangeId(seq_entry, id);
  feat = MakeNewFeat(seq_entry,CSeqFeatData::e_Gene,CSeqFeatData::eSubtype_any, 10, 20);
  feat->SetData().SetGene().SetLocus("Spy50");
  AddFeat(feat, seq_entry);
  feat = MakeNewFeat(seq_entry, CSeqFeatData::e_Gene, CSeqFeatData::eSubtype_any, 25, 35);
  feat->SetData().SetGene().SetLocus("Spy10");
  AddFeat(feat, seq_entry);
  feat = MakeNewFeat(seq_entry, CSeqFeatData::e_Gene, CSeqFeatData::eSubtype_any, 55, 59);
  feat->SetData().SetGene().SetLocus("Spy50");
  AddFeat(feat, seq_entry);

  nuc_prot_set = GetNucProtSetFromGenProdSet(gen_prot_set);
  id->SetLocal().SetStr("good nuc2");
  ChangeNucProtSetNucId(nuc_prot_set, id);
  id->SetLocal().SetStr("good prot2");
  ChangeNucProtSetProteinId(nuc_prot_set, id);

  seq_entry = GetNucleotideSequenceFromGoodNucProtSet(nuc_prot_set);
  feat = MakeNewFeat(seq_entry, CSeqFeatData::e_Gene, CSeqFeatData::eSubtype_any, 10, 20);
  feat->SetData().SetGene().SetLocus("Spy50");
  AddFeat(feat, seq_entry);
  feat = MakeNewFeat(seq_entry, CSeqFeatData::e_Gene, CSeqFeatData::eSubtype_any, 25, 35);
  feat->SetData().SetGene().SetLocus("Spy10");
  AddFeat(feat, seq_entry);
  feat = MakeNewFeat(seq_entry, CSeqFeatData::e_Gene, CSeqFeatData::eSubtype_any, 55, 59);
  feat->SetData().SetGene().SetLocus("Spy50");

  entry->SetSet().SetSeq_set().push_back(gen_prot_set);

  RunAndCheckTest(entry, "DUPLICATE_GENE_LOCUS", 
          "2 genes have the same locus as another gene on the same Bioseq.");
   
};


BOOST_AUTO_TEST_CASE(RNA_CDS_OVERLAP)
{
    CRef<CSeq_entry> entry = BuildGoodNucProtSet();
    SetLineage(entry, "something");
    CRef <CSeq_entry> nuc_entry = GetNucleotideSequenceFromGoodNucProtSet(entry);

    CRef<CSeq_feat> cds = GetCDSFromGoodNucProtSet (entry);

    CRef <CSeq_feat> rna = BuildGoodtRNA(nuc_entry->GetSeq().GetId().front());
    rna->SetLocation().SetInt().SetFrom(cds->GetLocation().GetInt().GetFrom() + 1);
    rna->SetLocation().SetInt().SetTo(cds->GetLocation().GetInt().GetTo() - 1);
   
    AddFeat(rna, entry);

    // check tRNA completely contained in coding region
    CRef<CClickableItem> item = RunOneTest(*entry, "RNA_CDS_OVERLAP");
    CheckOneItem(*item, "1 coding region overlaps RNA feature", true);
    BOOST_CHECK_EQUAL(item->subcategories.size(), 1);
    CheckOneItem(*(item->subcategories[0]), "1 coding region completely contains tRNAs", true);

    // change to miscRNA, still completely contained
    rna->SetData().SetRna().SetType(CRNA_ref::eType_miscRNA);
    rna->SetData().SetRna().SetExt().SetGen().SetProduct("something");

    item = RunOneTest(*entry, "RNA_CDS_OVERLAP");
    CheckOneItem(*item, "1 coding region overlaps RNA feature", false);
    BOOST_CHECK_EQUAL(item->subcategories.size(), 1);
    CheckOneItem(*(item->subcategories[0]), "1 coding region completely contains an RNA", true);

    // make locations equal
    rna->SetLocation().Assign(cds->GetLocation());
    item = RunOneTest(*entry, "RNA_CDS_OVERLAP");
    CheckOneItem(*item, "1 coding region overlaps RNA feature", false);
    BOOST_CHECK_EQUAL(item->subcategories.size(), 1);
    CheckOneItem(*(item->subcategories[0]), "1 coding region exactly matches an RNA", false);

    // put coding region inside RNA
    cds->SetLocation().SetInt().SetFrom(rna->GetLocation().GetInt().GetFrom() + 1);
    item = RunOneTest(*entry, "RNA_CDS_OVERLAP");
    CheckOneItem(*item, "1 coding region overlaps RNA feature", false);
    BOOST_CHECK_EQUAL(item->subcategories.size(), 1);
    CheckOneItem(*(item->subcategories[0]), "1 coding region is completely contained in an RNA location", true);

    // simple overlap
    rna->SetLocation().SetInt().SetTo(cds->GetLocation().GetInt().GetTo() - 1);
    item = RunOneTest(*entry, "RNA_CDS_OVERLAP");
    CheckOneItem(*item, "1 coding region overlaps RNA feature", false);
    BOOST_CHECK_EQUAL(item->subcategories.size(), 1);
    CheckOneItem(*(item->subcategories[0]), "1 coding region overlaps an RNA (no containment)", false);
    BOOST_CHECK_EQUAL(item->subcategories[0]->subcategories.size(), 1);
    CheckOneItem(*(item->subcategories[0]->subcategories[0]), "1 coding region overlaps RNA on the same strand (no containment)", false);

    // on opposite strand
    rna->SetLocation().SetInt().SetStrand(eNa_strand_minus);
    item = RunOneTest(*entry, "RNA_CDS_OVERLAP");
    CheckOneItem(*item, "1 coding region overlaps RNA feature", false);
    BOOST_CHECK_EQUAL(item->subcategories.size(), 1);
    CheckOneItem(*(item->subcategories[0]), "1 coding region overlaps an RNA (no containment)", false);
    BOOST_CHECK_EQUAL(item->subcategories[0]->subcategories.size(), 1);
    CheckOneItem(*(item->subcategories[0]->subcategories[0]), "1 coding region overlaps RNA on the opposite strand (no containment)", false);

}


const char *sc_GB_3763 = "\
Seq-entry ::= set {\
  class pop-set,\
  descr {\
    pub {\
      pub {\
        gen {\
          cit \"Unpublished\",\
          authors {\
            names std {\
              {\
                name name {\
                  last \"Ha\",\
                  first \"Thi Thu\",\
                  initials \"T.T.\"\
                }\
              },\
              {\
                name name {\
                  last \"Nguyen\",\
                  first \"Thi Kim Lien\",\
                  initials \"T.K.L.\"\
                }\
              },\
              {\
                name name {\
                  last \"Mai\",\
                  first \"Thuy Linh\",\
                  initials \"T.L.\"\
                }\
              },\
              {\
                name name {\
                  last \"Nguyen\",\
                  first \"Thi Bich Nga\",\
                  initials \"T.B.N.\"\
                }\
              },\
              {\
                name name {\
                  last \"Le\",\
                  first \"Thanh Hoa\",\
                  initials \"T.H.\"\
                }\
              },\
              {\
                name name {\
                  last \"Jung\",\
                  first \"Suk- Chan\",\
                  initials \"S.-C.\"\
                }\
              },\
              {\
                name name {\
                  last \"Kang\",\
                  first \"Seung-Won\",\
                  initials \"S.-W.\"\
                }\
              },\
              {\
                name name {\
                  last \"Dong\",\
                  first \"Van Quyen\",\
                  initials \"V.Q.\"\
                }\
              }\
            }\
          },\
          title \"The complete genome sequence of Sacbrood virus from infected\
 honeybee Apis cerana in Vietnam\"\
        }\
      }\
    },\
    pub {\
      pub {\
        sub {\
          authors {\
            names std {\
              {\
                name name {\
                  last \"Ha\",\
                  first \"Thi Thu\",\
                  initials \"T.T.\"\
                }\
              },\
              {\
                name name {\
                  last \"Nguyen\",\
                  first \"Thi Kim Lien\",\
                  initials \"T.K.L.\"\
                }\
              },\
              {\
                name name {\
                  last \"Mai\",\
                  first \"Thuy Linh\",\
                  initials \"T.L.\"\
                }\
              },\
              {\
                name name {\
                  last \"Nguyen\",\
                  first \"Thi Bich Nga\",\
                  initials \"T.B.N.\"\
                }\
              },\
              {\
                name name {\
                  last \"Le\",\
                  first \"Thanh Hoa\",\
                  initials \"T.H.\"\
                }\
              },\
              {\
                name name {\
                  last \"Jung\",\
                  first \"Suk- Chan\",\
                  initials \"S.-C.\"\
                }\
              },\
              {\
                name name {\
                  last \"Kang\",\
                  first \"Seung-Won\",\
                  initials \"S.-W.\"\
                }\
              },\
              {\
                name name {\
                  last \"Dong\",\
                  first \"Van Quyen\",\
                  initials \"V.Q.\"\
                }\
              }\
            },\
            affil std {\
              affil \"Institue of Biotechnology-VAST\",\
              div \"Molecular Microbiology\",\
              city \"Ha Noi\",\
              sub \"Ha Noi\",\
              country \"84\",\
              street \"18 Hoang Quoc Viet, Cau Giay District\",\
              postal-code \"10000\"\
            }\
          },\
          medium email,\
          date std {\
            year 2014,\
            month 6,\
            day 9\
          }\
        }\
      }\
    },\
    update-date std {\
      year 2014,\
      month 6,\
      day 4,\
      hour 4,\
      minute 36,\
      second 19\
    }\
  },\
  seq-set {\
    set {\
      class nuc-prot,\
      descr {\
        source {\
          genome genomic,\
          org {\
            taxname \"Sacbrood virus\",\
            db {\
              {\
                db \"taxon\",\
                tag id 89463\
              }\
            },\
            syn {\
              \"Sac brood virus\"\
            },\
            orgname {\
              name virus \"Sacbrood virus\",\
              mod {\
                {\
                  subtype strain,\
                  subname \"LDst\"\
                },\
                {\
                  subtype nat-host,\
                  subname \"Apis cerana\"\
                }\
              },\
              lineage \"Viruses; ssRNA positive-strand viruses, no DNA stage;\
 Picornavirales; Iflaviridae; Iflavirus\",\
              gcode 1,\
              div \"VRL\"\
            }\
          },\
          subtype {\
            {\
              subtype country,\
              name \"Viet Nam: Lam Dong province\"\
            },\
            {\
              subtype isolation-source,\
              name \"bee larvae\"\
            },\
            {\
              subtype collection-date,\
              name \"31-Mar-2013\"\
            }\
          }\
        }\
      },\
      seq-set {\
        seq {\
          id {\
            local str \"Seq1\",\
            general {\
              db \"BankIt\",\
              tag str \"1732427/Seq1\"\
            },\
            general {\
              db \"TMSMART\",\
              tag id 46122716\
            },\
            genbank {\
              accession \"KJ959613\"\
            }\
          },\
          descr {\
            title \"[Sacbrood virus] The complete genome sequence of Sacbrood\
 virus strain LDst from infected honeybee Apis cerana in South Vietnam\",\
            molinfo {\
              biomol genomic,\
              completeness complete\
            },\
            user {\
              type str \"Submission\",\
              data {\
                {\
                  label str \"AdditionalComment\",\
                  data str \"LocalID:Seq1\"\
                }\
              }\
            },\
            user {\
              type str \"OrginalID\",\
              data {\
                {\
                  label str \"LocalId\",\
                  data str \"Seq1\"\
                }\
              }\
            }\
          },\
          inst {\
            repr raw,\
            mol rna,\
            length 18,\
            seq-data iupacna \"ATGAAATTTGGGCCCTAA\"\
          }\
        },\
        seq {\
          id {\
            local str \"Seq1_prot_1\",\
            general {\
              db \"TMSMART\",\
              tag id 46122717\
            }\
          },\
          descr {\
            title \"polyprotein [Sacbrood virus]\",\
            molinfo {\
              biomol peptide\
            },\
            user {\
              type str \"OrginalID\",\
              data {\
                {\
                  label str \"LocalId\",\
                  data str \"Seq1_prot_1\"\
                }\
              }\
            }\
          },\
          inst {\
            repr raw,\
            mol aa,\
            length 5,\
            seq-data ncbieaa \"MKFGP\"\
          },\
          annot {\
            {\
              data ftable {\
                {\
                  data prot {\
                    name {\
                      \"polyprotein\"\
                    }\
                  },\
                  location int {\
                    from 0,\
                    to 2858,\
                    id local str \"Seq1_prot_1\"\
                  }\
                }\
              }\
            }\
          }\
        }\
      },\
      annot {\
        {\
          data ftable {\
            {\
              data cdregion {\
                frame one,\
                code {\
                  id 1\
                }\
              },\
              product whole local str \"Seq1_prot_1\",\
              location int {\
                from 180,\
                to 8759,\
                strand plus,\
                id local str \"Seq1\"\
              }\
            }\
          }\
        }\
      }\
    },\
    set {\
      class nuc-prot,\
      descr {\
        source {\
          genome genomic,\
          org {\
            taxname \"Sacbrood virus\",\
            db {\
              {\
                db \"taxon\",\
                tag id 89463\
              }\
            },\
            syn {\
              \"Sac brood virus\"\
            },\
            orgname {\
              name virus \"Sacbrood virus\",\
              mod {\
                {\
                  subtype strain,\
                  subname \"HYnor\"\
                },\
                {\
                  subtype nat-host,\
                  subname \"Apis cerana\"\
                }\
              },\
              lineage \"Viruses; ssRNA positive-strand viruses, no DNA stage;\
 Picornavirales; Iflaviridae; Iflavirus\",\
              gcode 1,\
              div \"VRL\"\
            }\
          },\
          subtype {\
            {\
              subtype country,\
              name \"Viet Nam: Hung Yen province\"\
            },\
            {\
              subtype isolation-source,\
              name \"bee larvae\"\
            },\
            {\
              subtype collection-date,\
              name \"30-Jun-2013\"\
            }\
          }\
        }\
      },\
      seq-set {\
        seq {\
          id {\
            local str \"Seq2\",\
            general {\
              db \"BankIt\",\
              tag str \"1732427/Seq2\"\
            },\
            general {\
              db \"TMSMART\",\
              tag id 46122718\
            },\
            genbank {\
              accession \"KJ959614\"\
            }\
          },\
          descr {\
            title \"[Sacbrood virus] The complete genome sequence of Sacbrood\
 virus strain HYnor from infected honeybee Apis cerana in North Vietnam\",\
            molinfo {\
              biomol genomic,\
              completeness complete\
            },\
            user {\
              type str \"Submission\",\
              data {\
                {\
                  label str \"AdditionalComment\",\
                  data str \"LocalID:Seq2\"\
                }\
              }\
            },\
            user {\
              type str \"OrginalID\",\
              data {\
                {\
                  label str \"LocalId\",\
                  data str \"Seq2\"\
                }\
              }\
            }\
          },\
          inst {\
            repr raw,\
            mol rna,\
                        length 18,\
            seq-data iupacna \"ATGAAATTTGGGCCCTAA\"\
          }\
        },\
        seq {\
          id {\
            local str \"Seq2_prot_1\",\
            general {\
              db \"TMSMART\",\
              tag id 46122719\
            }\
          },\
          descr {\
            title \"polyprotein [Sacbrood virus]\",\
            molinfo {\
              biomol peptide\
            },\
            user {\
              type str \"OrginalID\",\
              data {\
                {\
                  label str \"LocalId\",\
                  data str \"Seq2_prot_1\"\
                }\
              }\
            }\
          },\
          inst {\
            repr raw,\
            mol aa,\
            length 5,\
            seq-data ncbieaa \"MKFGP\"\
          },\
          annot {\
            {\
              data ftable {\
                {\
                  data prot {\
                    name {\
                      \"polyprotein\"\
                    }\
                  },\
                  location int {\
                    from 0,\
                    to 2841,\
                    id local str \"Seq2_prot_1\"\
                  }\
                }\
              }\
            }\
          }\
        }\
      },\
      annot {\
        {\
          data ftable {\
            {\
              data cdregion {\
                frame one,\
                code {\
                  id 1\
                }\
              },\
              product whole local str \"Seq2_prot_1\",\
              location int {\
                from 181,\
                to 8709,\
                strand plus,\
                id local str \"Seq2\"\
              }\
            }\
          }\
        }\
      }\
    }\
  }\
}\
";

BOOST_AUTO_TEST_CASE(GB_3763)
{
    CRef<CSeq_entry> entry(new CSeq_entry());
    {{
         CNcbiIstrstream istr(sc_GB_3763);
         istr >> MSerial_AsnText >> *entry;
    }}

    
    CRef<CClickableItem> item = RunOneTest(*entry, "DISC_FLATFILE_FIND_ONCALLER_FIXABLE");
    CheckOneItem(*item, "1 object contains institue", false);

}


CRef<CSeq_entry> s_BuildEntryForSrcQualProblem()
{
    CRef<CSeq_entry> e1 = unit_test_util::BuildGoodSeq();
    unit_test_util::ChangeId(e1, "_1");
    unit_test_util::SetOrgMod(e1, COrgMod::eSubtype_isolate, "abc");
    unit_test_util::SetOrgMod(e1, COrgMod::eSubtype_strain, "x");
    unit_test_util::SetOrgMod(e1, COrgMod::eSubtype_strain, "y");
    unit_test_util::SetSubSource(e1, CSubSource::eSubtype_cell_type, "c1");

    CRef<CSeq_entry> e2 = unit_test_util::BuildGoodSeq();
    unit_test_util::ChangeId(e2, "_2");
    unit_test_util::SetOrgMod(e2, COrgMod::eSubtype_isolate, "abc");
    unit_test_util::SetSubSource(e2, CSubSource::eSubtype_cell_type, "c2");

    CRef<CSeq_entry> entry(new CSeq_entry());
    entry->SetSet().SetClass(CBioseq_set::eClass_phy_set);
    entry->SetSet().SetSeq_set().push_back(e1);
    entry->SetSet().SetSeq_set().push_back(e2);
    return entry;
}


CRef<CSeq_entry> s_MakeEntryForDISC_DUP_SRC_QUAL()
{
    CRef<CSeq_entry> e1 = unit_test_util::BuildGoodSeq();
    unit_test_util::ChangeId(e1, "_1");
    unit_test_util::SetOrgMod(e1, COrgMod::eSubtype_isolate, "abc");
    unit_test_util::SetOrgMod(e1, COrgMod::eSubtype_strain, "abc");
    CRef<CSeq_entry> e2 = unit_test_util::BuildGoodSeq();
    unit_test_util::ChangeId(e2, "_2");
    unit_test_util::SetOrgMod(e2, COrgMod::eSubtype_isolate, "def");
    unit_test_util::SetOrgMod(e2, COrgMod::eSubtype_strain, "def");

    CRef<CSeq_entry> entry(new CSeq_entry());
    entry->SetSet().SetClass(CBioseq_set::eClass_phy_set);
    entry->SetSet().SetSeq_set().push_back(e1);
    entry->SetSet().SetSeq_set().push_back(e2);
    return entry;
}


static const string k2GoodList = "lcl|good_1:Sebaea microphylla\n\
lcl|good_2:Sebaea microphylla\n";

static const string kAbcForIsolate = "DISC_SOURCE_QUALS_ASNDISC::isolate (all present, all same)\n\
DISC_SOURCE_QUALS_ASNDISC::2 sources have 'abc' for isolate\n" + k2GoodList;

static const string kIsolateAllPresentAllUnique = "isolate (all present, all unique)\n" + k2GoodList;
static const string kADIsolateAllPresentAllUnique = "DISC_SOURCE_QUALS_ASNDISC::" + kIsolateAllPresentAllUnique;
static const string kSQIsolateAllPresentAllUnique = "DISC_SRC_QUAL_PROBLEM::" + kIsolateAllPresentAllUnique;

static const string kSameValue = "DISC_DUP_SRC_QUAL::2 sources have two or more qualifiers with the same value\n"
+ k2GoodList + 
"DISC_DUP_SRC_QUAL::BioSource has value 'abc' for these qualifiers: isolate, strain\n\
lcl|good_1:Sebaea microphylla\n\
DISC_DUP_SRC_QUAL::BioSource has value 'def' for these qualifiers: isolate, strain\n\
lcl|good_2:Sebaea microphylla\n";

static const string kSQChromosome2for1 = "DISC_SRC_QUAL_PROBLEM::chromosome (all present, all same)\n\
DISC_SRC_QUAL_PROBLEM::2 sources have '1' for chromosome\n" + k2GoodList;

static const string kADChromosome2for1 = "DISC_SOURCE_QUALS_ASNDISC::chromosome (all present, all same)\n\
DISC_SOURCE_QUALS_ASNDISC::2 sources have '1' for chromosome\n" + k2GoodList;

BOOST_AUTO_TEST_CASE(Test_New_Framework)
{
    CRef<CSeq_entry> e1 = unit_test_util::BuildGoodSeq();
    CRef<CObjectManager> objmgr = CObjectManager::GetInstance();
    CScope scope(*objmgr);
    scope.AddDefaults();
    CSeq_entry_Handle seh = scope.AddTopLevelSeqEntry(*e1);

    CRef<CAnalysisWorkflow> report_maker(new CAnalysisWorkflow());
    report_maker->Enable(DiscRepNmSpc::kDISC_COUNT_NUCLEOTIDES, true);
    report_maker->Enable(DiscRepNmSpc::kDISC_SOURCE_QUALS_ASNDISC, true);
    report_maker->Enable(DiscRepNmSpc::kDISC_DUP_SRC_QUAL, true);
    report_maker->BuildTestList();
    report_maker->CollectData(seh);
    report_maker->Collate();
    DiscRepNmSpc::TReportItemList list = report_maker->GetResults();

    DiscRepNmSpc::CReportOutputConfig output_config;
    output_config.SetFormat(CReportOutputConfig::eFormatText);
    output_config.SetExpand(DiscRepNmSpc::kDISC_SOURCE_QUALS_ASNDISC);
    output_config.SetExpand(DiscRepNmSpc::kDISC_DUP_SRC_QUAL);
    BOOST_CHECK_EQUAL(list.size(), 4);
    BOOST_CHECK_EQUAL(list[0]->GetText(), "1 nucleotide Bioseq is present");
    BOOST_CHECK_EQUAL(list[0]->GetObjects().size(), 1);
    BOOST_CHECK_EQUAL(list[0]->Format(output_config, scope), 
        "DISC_COUNT_NUCLEOTIDES::1 nucleotide Bioseq is present\nlcl|good (length 60)\n");

    BOOST_CHECK_EQUAL(list[1]->Format(output_config, scope), 
        "DISC_SOURCE_QUALS_ASNDISC::chromosome (all present, all unique)\nlcl|good:Sebaea microphylla\n");
    BOOST_CHECK_EQUAL(list[2]->Format(output_config, scope), 
        "DISC_SOURCE_QUALS_ASNDISC::taxid (all present, all unique)\nlcl|good:Sebaea microphylla\n");
    BOOST_CHECK_EQUAL(list[3]->Format(output_config, scope), 
        "DISC_SOURCE_QUALS_ASNDISC::taxname (all present, all unique)\nlcl|good:Sebaea microphylla\n");

    report_maker->ResetData();
    scope.RemoveTopLevelSeqEntry(seh);
    e1 = s_BuildEntryForSrcQualProblem();
    seh = scope.AddTopLevelSeqEntry(*e1);
    report_maker->CollectData(seh);
    report_maker->Collate();
    list = report_maker->GetResults();

    BOOST_CHECK_EQUAL(list.size(), 7);
    BOOST_CHECK_EQUAL(list[0]->Format(output_config, scope), 
        "DISC_COUNT_NUCLEOTIDES::2 nucleotide Bioseqs are present\nlcl|good_1 (length 60)\nlcl|good_2 (length 60)\n");
    BOOST_CHECK_EQUAL(list[1]->Format(output_config, scope),
        "DISC_SOURCE_QUALS_ASNDISC::cell_type (all present, all unique)\n\
lcl|good_1:Sebaea microphylla\n\
lcl|good_2:Sebaea microphylla\n");
    BOOST_CHECK_EQUAL(list[2]->Format(output_config, scope), kADChromosome2for1);
    BOOST_CHECK_EQUAL(list[3]->Format(output_config, scope), kAbcForIsolate);
    BOOST_CHECK_EQUAL(list[4]->Format(output_config, scope),
        "DISC_SOURCE_QUALS_ASNDISC::strain (some missing, all unique, some multi)\n\
DISC_SOURCE_QUALS_ASNDISC::1 source is missing strain\n\
lcl|good_2:Sebaea microphylla\n\
DISC_SOURCE_QUALS_ASNDISC::1 source has multiple strain qualifiers\n\
lcl|good_1:Sebaea microphylla\n\
DISC_SOURCE_QUALS_ASNDISC::1 source has unique values for strain\n\
lcl|good_1:Sebaea microphylla\n\
");
    BOOST_CHECK_EQUAL(list[5]->Format(output_config, scope), 
        "DISC_SOURCE_QUALS_ASNDISC::taxid (all present, all same)\n\
DISC_SOURCE_QUALS_ASNDISC::2 sources have '592768' for taxid\n\
lcl|good_1:Sebaea microphylla\n\
lcl|good_2:Sebaea microphylla\n");
    BOOST_CHECK_EQUAL(list[6]->Format(output_config, scope), 
        "DISC_SOURCE_QUALS_ASNDISC::taxname (all present, all same)\n\
DISC_SOURCE_QUALS_ASNDISC::2 sources have 'Sebaea microphylla' for taxname\n\
lcl|good_1:Sebaea microphylla\n\
lcl|good_2:Sebaea microphylla\n");


    report_maker->ResetData();
    scope.RemoveTopLevelSeqEntry(seh);
    e1 = s_MakeEntryForDISC_DUP_SRC_QUAL();
    seh = scope.AddTopLevelSeqEntry(*e1);
    report_maker->CollectData(seh);
    report_maker->Collate();
    list = report_maker->GetResults();

    BOOST_CHECK_EQUAL(list.size(), 7);
    BOOST_CHECK_EQUAL(list[0]->Format(output_config, scope), 
        "DISC_COUNT_NUCLEOTIDES::2 nucleotide Bioseqs are present\nlcl|good_1 (length 60)\nlcl|good_2 (length 60)\n");
    BOOST_CHECK_EQUAL(list[1]->Format(output_config, scope), kADChromosome2for1);
    BOOST_CHECK_EQUAL(list[2]->Format(output_config, scope), kADIsolateAllPresentAllUnique);
    BOOST_CHECK_EQUAL(list[3]->Format(output_config, scope),
        "DISC_SOURCE_QUALS_ASNDISC::strain (all present, all unique)\n\
lcl|good_1:Sebaea microphylla\n\
lcl|good_2:Sebaea microphylla\n");
    BOOST_CHECK_EQUAL(list[4]->Format(output_config, scope), 
        "DISC_SOURCE_QUALS_ASNDISC::taxid (all present, all same)\n\
DISC_SOURCE_QUALS_ASNDISC::2 sources have '592768' for taxid\n\
lcl|good_1:Sebaea microphylla\n\
lcl|good_2:Sebaea microphylla\n");
    BOOST_CHECK_EQUAL(list[5]->Format(output_config, scope), 
        "DISC_SOURCE_QUALS_ASNDISC::taxname (all present, all same)\n\
DISC_SOURCE_QUALS_ASNDISC::2 sources have 'Sebaea microphylla' for taxname\n\
lcl|good_1:Sebaea microphylla\n\
lcl|good_2:Sebaea microphylla\n");
    BOOST_CHECK_EQUAL(list[6]->Format(output_config, scope), kSameValue);
}


BOOST_AUTO_TEST_CASE(Test_MISSING_GENES)
{
    CRef <CSeq_entry> entry = BuildGoodNucProtSet();

    CRef<CObjectManager> objmgr = CObjectManager::GetInstance();
    CScope scope(*objmgr);
    scope.AddDefaults();
    CSeq_entry_Handle seh = scope.AddTopLevelSeqEntry(*entry);

    CRef<CAnalysisWorkflow> report_maker(new CAnalysisWorkflow());
    report_maker->Enable(DiscRepNmSpc::kMISSING_GENES, true);
    report_maker->BuildTestList();

    report_maker->CollectData(seh);
    report_maker->Collate();

    DiscRepNmSpc::CReportOutputConfig output_config;
    output_config.SetFormat(CReportOutputConfig::eFormatText);
    output_config.SetExpand(DiscRepNmSpc::kMISSING_GENES);
    DiscRepNmSpc::TReportItemList list = report_maker->GetResults();

    BOOST_CHECK_EQUAL(list.size(), 1);
    BOOST_CHECK_EQUAL(list[0]->Format(output_config, scope), 
        "MISSING_GENES::1 feature has no genes.\nCDS\tfake protein name\tlcl|nuc:1-27\t\n");

}


BOOST_AUTO_TEST_CASE(Test_DISC_SUPERFLUOUS_GENE)
{
    CRef <CSeq_entry> entry = BuildGoodSeq();
    CRef<objects::CSeq_feat> gene = unit_test_util::AddMiscFeature(entry);
    gene->SetData().SetGene().SetLocus("abc");
    gene->ResetComment();

    CRef<CObjectManager> objmgr = CObjectManager::GetInstance();
    CScope scope(*objmgr);
    scope.AddDefaults();
    CSeq_entry_Handle seh = scope.AddTopLevelSeqEntry(*entry);

    CRef<CAnalysisWorkflow> report_maker(new CAnalysisWorkflow());
    report_maker->Enable(DiscRepNmSpc::kDISC_SUPERFLUOUS_GENE, true);
    report_maker->BuildTestList();

    report_maker->CollectData(seh);
    report_maker->Collate();

    DiscRepNmSpc::CReportOutputConfig output_config;
    output_config.SetFormat(CReportOutputConfig::eFormatText);
    output_config.SetExpand(DiscRepNmSpc::kDISC_SUPERFLUOUS_GENE);
    DiscRepNmSpc::TReportItemList list = report_maker->GetResults();

    BOOST_CHECK_EQUAL(list.size(), 1);
    BOOST_CHECK_EQUAL(list[0]->Format(output_config, scope), 
        "EXTRA_GENES::1 gene feature is not associated with a CDS or RNA feature.\nGene\tabc\tlcl|good:1-11\t\n\
EXTRA_GENES::1 non-pseudo gene feature is not associated with a CDS or RNA feature and does not have frameshift in the comment.\nGene\tabc\tlcl|good:1-11\t\n");

    report_maker->ResetData();
    // do not report if gene feature has comment
    gene->SetComment("a comment");
    report_maker->CollectData(seh);
    report_maker->Collate();
    list = report_maker->GetResults();
    BOOST_CHECK_EQUAL(list.size(), 0);

    report_maker->ResetData();
    // but do report if pseudo
    gene->SetPseudo(true);
    report_maker->CollectData(seh);
    report_maker->Collate();
    list = report_maker->GetResults();
    BOOST_CHECK_EQUAL(list.size(), 1);
    BOOST_CHECK_EQUAL(list[0]->Format(output_config, scope), 
        "EXTRA_GENES::1 gene feature is not associated with a CDS or RNA feature.\nGene\tabc\tlcl|good:1-11\t\n\
EXTRA_GENES::1 pseudo gene feature is not associated with a CDS or RNA feature.\nGene\tabc\tlcl|good:1-11\t\n");

}


BOOST_AUTO_TEST_CASE(Test_DropReferences)
{
    CRef <CSeq_entry> entry = BuildGoodSeq();
    CRef<objects::CSeq_feat> gene = unit_test_util::AddMiscFeature(entry);
    gene->SetData().SetGene().SetLocus("abc");
    gene->ResetComment();

    CRef<CObjectManager> objmgr = CObjectManager::GetInstance();
    CScope scope(*objmgr);
    scope.AddDefaults();
    CSeq_entry_Handle seh = scope.AddTopLevelSeqEntry(*entry);

    CRef<CAnalysisWorkflow> report_maker(new CAnalysisWorkflow());
    report_maker->Enable(DiscRepNmSpc::kDISC_SUPERFLUOUS_GENE, true);
    report_maker->BuildTestList();

    report_maker->CollectData(seh, true);
    scope.RemoveTopLevelSeqEntry(seh);
    report_maker->Collate();

    DiscRepNmSpc::CReportOutputConfig output_config;
    output_config.SetFormat(CReportOutputConfig::eFormatText);
    output_config.SetExpand(DiscRepNmSpc::kDISC_SUPERFLUOUS_GENE);
    DiscRepNmSpc::TReportItemList list = report_maker->GetResults();

    BOOST_CHECK_EQUAL(list.size(), 1);
    BOOST_CHECK_EQUAL(list[0]->Format(output_config), 
        "EXTRA_GENES::1 gene feature is not associated with a CDS or RNA feature.\nGene\tabc\tlcl|good:1-11\t\n\
EXTRA_GENES::1 non-pseudo gene feature is not associated with a CDS or RNA feature and does not have frameshift in the comment.\nGene\tabc\tlcl|good:1-11\t\n");

    report_maker->ResetData();
    // do not report if gene feature has comment
    gene->SetComment("a comment");
    seh = scope.AddTopLevelSeqEntry(*entry);
    report_maker->CollectData(seh, true);
    scope.RemoveTopLevelSeqEntry(seh);
    report_maker->Collate();
    list = report_maker->GetResults();
    BOOST_CHECK_EQUAL(list.size(), 0);

    report_maker->ResetData();
    // but do report if pseudo
    gene->SetPseudo(true);
    seh = scope.AddTopLevelSeqEntry(*entry);
    report_maker->CollectData(seh, true);
    scope.RemoveTopLevelSeqEntry(seh);
    report_maker->Collate();
    list = report_maker->GetResults();
    BOOST_CHECK_EQUAL(list.size(), 1);
    BOOST_CHECK_EQUAL(list[0]->Format(output_config), 
        "EXTRA_GENES::1 gene feature is not associated with a CDS or RNA feature.\nGene\tabc\tlcl|good:1-11\t\n\
EXTRA_GENES::1 pseudo gene feature is not associated with a CDS or RNA feature.\nGene\tabc\tlcl|good:1-11\t\n");

}


BOOST_AUTO_TEST_CASE(Test_New_Framework_Grouping)
{
    CRef<CSeq_entry> e1 = s_MakeEntryForDISC_DUP_SRC_QUAL();
    CRef<CObjectManager> objmgr = CObjectManager::GetInstance();
    CScope scope(*objmgr);
    scope.AddDefaults();
    CSeq_entry_Handle seh = scope.AddTopLevelSeqEntry(*e1);

    CRef<CAnalysisWorkflow> report_maker(new CAnalysisWorkflow());
    report_maker->Enable(DiscRepNmSpc::kDISC_COUNT_NUCLEOTIDES, true);
    report_maker->Enable(DiscRepNmSpc::kDISC_SRC_QUAL_PROBLEM, true);
    report_maker->Enable(DiscRepNmSpc::kDISC_DUP_SRC_QUAL, true);
    report_maker->BuildTestList();

    CRef<CGroupPolicy> group_policy(new CGUIGroupPolicy());
    report_maker->SetGroupPolicy(group_policy);

    report_maker->CollectData(seh);
    report_maker->Collate();

    DiscRepNmSpc::CReportOutputConfig output_config;
    output_config.SetFormat(CReportOutputConfig::eFormatText);
    output_config.SetExpand(DiscRepNmSpc::kDISC_SRC_QUAL_PROBLEM);
    output_config.SetExpand(DiscRepNmSpc::kDISC_DUP_SRC_QUAL);
    output_config.SetAddTag(true);
    DiscRepNmSpc::TReportItemList list = report_maker->GetResults();
    BOOST_CHECK_EQUAL(list.size(), 3);
    BOOST_CHECK_EQUAL(list[0]->Format(output_config, scope), 
        "DISC_COUNT_NUCLEOTIDES::2 nucleotide Bioseqs are present\nlcl|good_1 (length 60)\nlcl|good_2 (length 60)\n");
    BOOST_CHECK_EQUAL(list[1]->Format(output_config, scope),
        "DISC_SRC_QUAL_PROBLEM::Source Qualifier Report\n" + kSQChromosome2for1 + kSQIsolateAllPresentAllUnique +
"DISC_SRC_QUAL_PROBLEM::strain (all present, all unique)\n\
lcl|good_1:Sebaea microphylla\n\
lcl|good_2:Sebaea microphylla\n\
DISC_SRC_QUAL_PROBLEM::taxid (all present, all same)\nDISC_SRC_QUAL_PROBLEM::2 sources have '592768' for taxid\n\
lcl|good_1:Sebaea microphylla\n\
lcl|good_2:Sebaea microphylla\n\
DISC_SRC_QUAL_PROBLEM::taxname (all present, all same)\nDISC_SRC_QUAL_PROBLEM::2 sources have 'Sebaea microphylla' for taxname\n\
lcl|good_1:Sebaea microphylla\n\
lcl|good_2:Sebaea microphylla\n");
    BOOST_CHECK_EQUAL(list[2]->Format(output_config, scope), kSameValue);

    group_policy.Reset(new COncallerGroupPolicy());
    report_maker->SetGroupPolicy(group_policy);
    list = report_maker->GetResults();

    BOOST_CHECK_EQUAL(list.size(), 2);
    BOOST_CHECK_EQUAL(list[0]->Format(output_config, scope), 
        "DISC_COUNT_NUCLEOTIDES::2 nucleotide Bioseqs are present\nlcl|good_1 (length 60)\nlcl|good_2 (length 60)\n");
    BOOST_CHECK_EQUAL(list[1]->Format(output_config, scope),
        "DISC_SRC_QUAL_PROBLEM::Source tests\n\
DISC_SRC_QUAL_PROBLEM::Source Qualifier Report\n" + kSQChromosome2for1 + kSQIsolateAllPresentAllUnique +
"DISC_SRC_QUAL_PROBLEM::strain (all present, all unique)\n\
lcl|good_1:Sebaea microphylla\n\
lcl|good_2:Sebaea microphylla\n\
DISC_SRC_QUAL_PROBLEM::taxid (all present, all same)\n\
DISC_SRC_QUAL_PROBLEM::2 sources have '592768' for taxid\n\
lcl|good_1:Sebaea microphylla\n\
lcl|good_2:Sebaea microphylla\n\
DISC_SRC_QUAL_PROBLEM::taxname (all present, all same)\nDISC_SRC_QUAL_PROBLEM::2 sources have 'Sebaea microphylla' for taxname\n\
lcl|good_1:Sebaea microphylla\n\
lcl|good_2:Sebaea microphylla\n" + kSameValue);


}


BOOST_AUTO_TEST_CASE(Test_New_Framework_Tagging)
{
    CRef<CSeq_entry> e1 = s_BuildEntryForSrcQualProblem();
    CRef<CObjectManager> objmgr = CObjectManager::GetInstance();
    CScope scope(*objmgr);
    scope.AddDefaults();
    CSeq_entry_Handle seh = scope.AddTopLevelSeqEntry(*e1);

    CRef<CAnalysisWorkflow> report_maker(new CAnalysisWorkflow());
    report_maker->Enable(DiscRepNmSpc::kDISC_COUNT_NUCLEOTIDES, true);
    report_maker->Enable(DiscRepNmSpc::kDISC_SOURCE_QUALS_ASNDISC, true);
    report_maker->Enable(DiscRepNmSpc::kDISC_DUP_SRC_QUAL, true);
    report_maker->BuildTestList();

    report_maker->CollectData(seh);
    report_maker->Collate();

    CRef<CWGSTagPolicy> policy(new CWGSTagPolicy());
    policy->SetMoreThanOneNuc(true);
    report_maker->SetTagPolicy((CRef<CTagPolicy>)(policy.GetPointer()));
    DiscRepNmSpc::CReportOutputConfig output_config;
    output_config.SetFormat(CReportOutputConfig::eFormatText);
    output_config.SetExpand(DiscRepNmSpc::kDISC_SOURCE_QUALS_ASNDISC);
    output_config.SetExpand(DiscRepNmSpc::kDISC_DUP_SRC_QUAL);
    output_config.SetAddTag(true);
    DiscRepNmSpc::TReportItemList list = report_maker->GetResults();
    BOOST_CHECK_EQUAL(list.size(), 7);
    BOOST_CHECK_EQUAL(list[0]->Format(output_config, scope), 
        "DISC_COUNT_NUCLEOTIDES::2 nucleotide Bioseqs are present\nlcl|good_1 (length 60)\nlcl|good_2 (length 60)\n");
    BOOST_CHECK_EQUAL(list[1]->Format(output_config, scope),
        "DISC_SOURCE_QUALS_ASNDISC::cell_type (all present, all unique)\n\
lcl|good_1:Sebaea microphylla\n\
lcl|good_2:Sebaea microphylla\n");
    BOOST_CHECK_EQUAL(list[2]->Format(output_config, scope), kADChromosome2for1);
    BOOST_CHECK_EQUAL(list[3]->Format(output_config, scope), kAbcForIsolate);
    BOOST_CHECK_EQUAL(list[4]->Format(output_config, scope),
        "FATAL: DISC_SOURCE_QUALS_ASNDISC::strain (some missing, all unique, some multi)\n\
DISC_SOURCE_QUALS_ASNDISC::1 source is missing strain\n\
lcl|good_2:Sebaea microphylla\n\
DISC_SOURCE_QUALS_ASNDISC::1 source has multiple strain qualifiers\n\
lcl|good_1:Sebaea microphylla\n\
DISC_SOURCE_QUALS_ASNDISC::1 source has unique values for strain\n\
lcl|good_1:Sebaea microphylla\n\
");
    BOOST_CHECK_EQUAL(list[5]->Format(output_config, scope), 
        "DISC_SOURCE_QUALS_ASNDISC::taxid (all present, all same)\nDISC_SOURCE_QUALS_ASNDISC::2 sources have '592768' for taxid\n\
lcl|good_1:Sebaea microphylla\n\
lcl|good_2:Sebaea microphylla\n");
    BOOST_CHECK_EQUAL(list[6]->Format(output_config, scope), 
        "DISC_SOURCE_QUALS_ASNDISC::taxname (all present, all same)\n\
DISC_SOURCE_QUALS_ASNDISC::2 sources have 'Sebaea microphylla' for taxname\n\
lcl|good_1:Sebaea microphylla\n\
lcl|good_2:Sebaea microphylla\n");
}


BOOST_AUTO_TEST_CASE(Test_New_Framework_Ordering)
{
    CRef<CSeq_entry> e1 = s_MakeEntryForDISC_DUP_SRC_QUAL();
    CRef<CObjectManager> objmgr = CObjectManager::GetInstance();
    CScope scope(*objmgr);
    scope.AddDefaults();
    CSeq_entry_Handle seh = scope.AddTopLevelSeqEntry(*e1);

    CRef<CAnalysisWorkflow> report_maker(new CAnalysisWorkflow());
    report_maker->Enable(DiscRepNmSpc::kDISC_COUNT_NUCLEOTIDES, true);
    report_maker->Enable(DiscRepNmSpc::kDISC_SRC_QUAL_PROBLEM, true);
    report_maker->Enable(DiscRepNmSpc::kDISC_DUP_SRC_QUAL, true);
    report_maker->BuildTestList();

    CRef<CGroupPolicy> group_policy(new CGUIGroupPolicy());
    report_maker->SetGroupPolicy(group_policy);
    CRef<COrderPolicy> order_policy(new COrderPolicy());
    report_maker->SetOrderPolicy(order_policy);

    report_maker->CollectData(seh);
    report_maker->Collate();

    DiscRepNmSpc::CReportOutputConfig output_config;
    output_config.SetFormat(CReportOutputConfig::eFormatText);
    output_config.SetExpand(DiscRepNmSpc::kDISC_SRC_QUAL_PROBLEM);
    output_config.SetExpand(DiscRepNmSpc::kDISC_DUP_SRC_QUAL);
    output_config.SetAddTag(true);
    DiscRepNmSpc::TReportItemList list = report_maker->GetResults();
    BOOST_CHECK_EQUAL(list.size(), 3);
    BOOST_CHECK_EQUAL(list[0]->Format(output_config, scope), 
        "DISC_COUNT_NUCLEOTIDES::2 nucleotide Bioseqs are present\nlcl|good_1 (length 60)\nlcl|good_2 (length 60)\n");
    BOOST_CHECK_EQUAL(list[1]->Format(output_config, scope),
        "DISC_SRC_QUAL_PROBLEM::Source Qualifier Report\n" + kSQChromosome2for1 + kSQIsolateAllPresentAllUnique +
"DISC_SRC_QUAL_PROBLEM::strain (all present, all unique)\n\
lcl|good_1:Sebaea microphylla\n\
lcl|good_2:Sebaea microphylla\n\
DISC_SRC_QUAL_PROBLEM::taxid (all present, all same)\nDISC_SRC_QUAL_PROBLEM::2 sources have '592768' for taxid\n\
lcl|good_1:Sebaea microphylla\n\
lcl|good_2:Sebaea microphylla\n\
DISC_SRC_QUAL_PROBLEM::taxname (all present, all same)\nDISC_SRC_QUAL_PROBLEM::2 sources have 'Sebaea microphylla' for taxname\n\
lcl|good_1:Sebaea microphylla\n\
lcl|good_2:Sebaea microphylla\n");
    BOOST_CHECK_EQUAL(list[2]->Format(output_config, scope), kSameValue);

    group_policy.Reset(new COncallerGroupPolicy());
    report_maker->SetGroupPolicy(group_policy);
    order_policy.Reset(new COncallerOrderPolicy());
    report_maker->SetOrderPolicy(order_policy);
    list = report_maker->GetResults();

    BOOST_CHECK_EQUAL(list.size(), 2);
    BOOST_CHECK_EQUAL(list[0]->Format(output_config, scope), 
        "DISC_COUNT_NUCLEOTIDES::2 nucleotide Bioseqs are present\nlcl|good_1 (length 60)\nlcl|good_2 (length 60)\n");
    BOOST_CHECK_EQUAL(list[1]->Format(output_config, scope),
        "DISC_SRC_QUAL_PROBLEM::Source tests\n\
DISC_SRC_QUAL_PROBLEM::Source Qualifier Report\n" + kSQChromosome2for1 + kSQIsolateAllPresentAllUnique +
"DISC_SRC_QUAL_PROBLEM::strain (all present, all unique)\n" + k2GoodList +
"DISC_SRC_QUAL_PROBLEM::taxid (all present, all same)\nDISC_SRC_QUAL_PROBLEM::2 sources have '592768' for taxid\n" + k2GoodList +
"DISC_SRC_QUAL_PROBLEM::taxname (all present, all same)\nDISC_SRC_QUAL_PROBLEM::2 sources have 'Sebaea microphylla' for taxname\n"
+ k2GoodList + kSameValue);

}


BOOST_AUTO_TEST_CASE(Test_New_Framework_ConfigureOncaller)
{
    CRef<CSeq_entry> e1 = s_MakeEntryForDISC_DUP_SRC_QUAL();
    CRef<CObjectManager> objmgr = CObjectManager::GetInstance();
    CScope scope(*objmgr);
    scope.AddDefaults();
    CSeq_entry_Handle seh = scope.AddTopLevelSeqEntry(*e1);

    CRef<CAnalysisWorkflow> report_maker(new CAnalysisWorkflow());
    report_maker->Configure(DiscRepNmSpc::CReportConfig::eConfigTypeOnCaller);

    report_maker->CollectData(seh);
    report_maker->Collate();

    DiscRepNmSpc::CReportOutputConfig output_config;
    output_config.SetFormat(CReportOutputConfig::eFormatText);
    output_config.SetExpand(DiscRepNmSpc::kDISC_SRC_QUAL_PROBLEM);
    output_config.SetExpand(DiscRepNmSpc::kDISC_DUP_SRC_QUAL);
    output_config.SetExpand(DiscRepNmSpc::kDISC_FEATURE_MOLTYPE_MISMATCH);
    output_config.SetExpand(DiscRepNmSpc::kDISC_CHECK_AUTH_CAPS);
    output_config.SetExpand(DiscRepNmSpc::kONCALLER_SUPERFLUOUS_GENE);
    output_config.SetAddTag(true);
    DiscRepNmSpc::TReportItemList list = report_maker->GetResults();

    BOOST_CHECK_EQUAL(list.size(), 35);
    BOOST_CHECK_EQUAL(list[0]->Format(output_config, scope), 
        "DISC_COUNT_NUCLEOTIDES::2 nucleotide Bioseqs are present\nlcl|good_1 (length 60)\nlcl|good_2 (length 60)\n");
    BOOST_CHECK_EQUAL(list[1]->Format(output_config, scope),
        "DISC_DUP_DEFLINE::DISC_DUP_DEFLINE is not implemented\n");
    BOOST_CHECK_EQUAL(list[2]->Format(output_config, scope),
        "DISC_MISSING_DEFLINES::DISC_MISSING_DEFLINES is not implemented\n");
    BOOST_CHECK_EQUAL(list[3]->Format(output_config, scope),
        "TEST_TAXNAME_NOT_IN_DEFLINE::TEST_TAXNAME_NOT_IN_DEFLINE is not implemented\n");
    BOOST_CHECK_EQUAL(list[4]->Format(output_config, scope),
        "TEST_HAS_PROJECT_ID::TEST_HAS_PROJECT_ID is not implemented\n");
    BOOST_CHECK_EQUAL(list[5]->Format(output_config, scope),
        "ONCALLER_BIOPROJECT_ID::ONCALLER_BIOPROJECT_ID is not implemented\n");
    BOOST_CHECK_EQUAL(list[6]->Format(output_config, scope),
        "ONCALLER_DEFLINE_ON_SET::ONCALLER_DEFLINE_ON_SET is not implemented\n");
    BOOST_CHECK_EQUAL(list[7]->Format(output_config, scope),
        "TEST_UNWANTED_SET_WRAPPER::TEST_UNWANTED_SET_WRAPPER is not implemented\n");
    BOOST_CHECK_EQUAL(list[8]->Format(output_config, scope),
        "TEST_COUNT_UNVERIFIED::TEST_COUNT_UNVERIFIED is not implemented\n");
    BOOST_CHECK_EQUAL(list[9]->Format(output_config, scope),
        "DISC_SRC_QUAL_PROBLEM::Source tests\n\
DISC_SRC_QUAL_PROBLEM::Source Qualifier Report\n" + kSQChromosome2for1 + kSQIsolateAllPresentAllUnique +
"DISC_SRC_QUAL_PROBLEM::strain (all present, all unique)\n" + k2GoodList +
"DISC_SRC_QUAL_PROBLEM::taxid (all present, all same)\nDISC_SRC_QUAL_PROBLEM::2 sources have '592768' for taxid\n" + k2GoodList +
"DISC_SRC_QUAL_PROBLEM::taxname (all present, all same)\nDISC_SRC_QUAL_PROBLEM::2 sources have 'Sebaea microphylla' for taxname\n" + k2GoodList +
"DISC_BACTERIA_MISSING_STRAIN::DISC_BACTERIA_MISSING_STRAIN is not implemented\n\
DISC_BACTERIA_SHOULD_NOT_HAVE_ISOLATE::DISC_BACTERIA_SHOULD_NOT_HAVE_ISOLATE is not implemented\n\
DISC_BIOMATERIAL_TAXNAME_MISMATCH::DISC_BIOMATERIAL_TAXNAME_MISMATCH is not implemented\n\
DISC_CULTURE_TAXNAME_MISMATCH::DISC_CULTURE_TAXNAME_MISMATCH is not implemented\n"
+ kSameValue +
"DISC_DUP_SRC_QUAL_DATA::DISC_DUP_SRC_QUAL_DATA is not implemented\n\
DISC_HUMAN_HOST::DISC_HUMAN_HOST is not implemented\n\
DISC_INFLUENZA_DATE_MISMATCH::DISC_INFLUENZA_DATE_MISMATCH is not implemented\n\
DISC_MAP_CHROMOSOME_CONFLICT::DISC_MAP_CHROMOSOME_CONFLICT is not implemented\n\
DISC_METAGENOME_SOURCE::DISC_METAGENOME_SOURCE is not implemented\n\
DISC_METAGENOMIC::DISC_METAGENOMIC is not implemented\n\
DISC_MISSING_VIRAL_QUALS::DISC_MISSING_VIRAL_QUALS is not implemented\n\
DISC_MITOCHONDRION_REQUIRED::DISC_MITOCHONDRION_REQUIRED is not implemented\n\
DISC_REQUIRED_CLONE::DISC_REQUIRED_CLONE is not implemented\n\
DISC_RETROVIRIDAE_DNA::DISC_RETROVIRIDAE_DNA is not implemented\n\
DISC_SPECVOUCHER_TAXNAME_MISMATCH::DISC_SPECVOUCHER_TAXNAME_MISMATCH is not implemented\n\
DISC_STRAIN_TAXNAME_MISMATCH::DISC_STRAIN_TAXNAME_MISMATCH is not implemented\n\
DISC_TRINOMIAL_SHOULD_HAVE_QUALIFIER::DISC_TRINOMIAL_SHOULD_HAVE_QUALIFIER is not implemented\n\
DUP_DISC_ATCC_CULTURE_CONFLICT::DUP_DISC_ATCC_CULTURE_CONFLICT is not implemented\n\
DUP_DISC_CBS_CULTURE_CONFLICT::DUP_DISC_CBS_CULTURE_CONFLICT is not implemented\n\
END_COLON_IN_COUNTRY::END_COLON_IN_COUNTRY is not implemented\n\
NON_RETROVIRIDAE_PROVIRAL::NON_RETROVIRIDAE_PROVIRAL is not implemented\n\
ONCALLER_CHECK_AUTHORITY::ONCALLER_CHECK_AUTHORITY is not implemented\n\
ONCALLER_COUNTRY_COLON::ONCALLER_COUNTRY_COLON is not implemented\n\
ONCALLER_DUPLICATE_PRIMER_SET::ONCALLER_DUPLICATE_PRIMER_SET is not implemented\n\
ONCALLER_HIV_RNA_INCONSISTENT::ONCALLER_HIV_RNA_INCONSISTENT is not implemented\n\
ONCALLER_MORE_NAMES_COLLECTED_BY::ONCALLER_MORE_NAMES_COLLECTED_BY is not implemented\n\
ONCALLER_MORE_OR_SPEC_NAMES_IDENTIFIED_BY::ONCALLER_MORE_OR_SPEC_NAMES_IDENTIFIED_BY is not implemented\n\
ONCALLER_MULTIPLE_CULTURE_COLLECTION::ONCALLER_MULTIPLE_CULTURE_COLLECTION is not implemented\n\
ONCALLER_MULTISRC::ONCALLER_MULTISRC is not implemented\n\
ONCALLER_STRAIN_CULTURE_COLLECTION_MISMATCH::ONCALLER_STRAIN_CULTURE_COLLECTION_MISMATCH is not implemented\n\
ONCALLER_STRAIN_TAXNAME_CONFLICT::ONCALLER_STRAIN_TAXNAME_CONFLICT is not implemented\n\
ONCALLER_SUSPECTED_ORG_COLLECTED::ONCALLER_SUSPECTED_ORG_COLLECTED is not implemented\n\
ONCALLER_SUSPECTED_ORG_IDENTIFIED::ONCALLER_SUSPECTED_ORG_IDENTIFIED is not implemented\n\
RNA_PROVIRAL::RNA_PROVIRAL is not implemented\n\
TEST_AMPLIFIED_PRIMERS_NO_ENVIRONMENTAL_SAMPLE::TEST_AMPLIFIED_PRIMERS_NO_ENVIRONMENTAL_SAMPLE is not implemented\n\
TEST_BAD_MRNA_QUAL::TEST_BAD_MRNA_QUAL is not implemented\n\
TEST_SMALL_GENOME_SET_PROBLEM::TEST_SMALL_GENOME_SET_PROBLEM is not implemented\n\
TEST_SP_NOT_UNCULTURED::TEST_SP_NOT_UNCULTURED is not implemented\n\
TEST_UNNECESSARY_ENVIRONMENTAL::TEST_UNNECESSARY_ENVIRONMENTAL is not implemented\n\
TEST_UNWANTED_SPACER::TEST_UNWANTED_SPACER is not implemented\n");
    BOOST_CHECK_EQUAL(list[10]->Format(output_config, scope),
        "DISC_FEATURE_COUNT::DISC_FEATURE_COUNT is not implemented\n");
    BOOST_CHECK_EQUAL(list[11]->Format(output_config, scope),
        "DISC_NO_ANNOTATION::DISC_NO_ANNOTATION is not implemented\n");
    BOOST_CHECK_EQUAL(list[12]->Format(output_config, scope),
        "DISC_FEATURE_MOLTYPE_MISMATCH::Molecule type tests\n\
DISC_FEATURE_MOLTYPE_MISMATCH::DISC_FEATURE_MOLTYPE_MISMATCH is not implemented\n\
DISC_INCONSISTENT_MOLTYPES::DISC_INCONSISTENT_MOLTYPES is not implemented\n\
TEST_ORGANELLE_NOT_GENOMIC::TEST_ORGANELLE_NOT_GENOMIC is not implemented\n");
    BOOST_CHECK_EQUAL(list[13]->Format(output_config, scope),
        "DISC_CHECK_AUTH_CAPS::Cit-sub type tests\n\
DISC_CHECK_AUTH_CAPS::DISC_CHECK_AUTH_CAPS is not implemented\n\
DISC_CHECK_AUTH_NAME::DISC_CHECK_AUTH_NAME is not implemented\n\
DISC_CITSUBAFFIL_CONFLICT::DISC_CITSUBAFFIL_CONFLICT is not implemented\n\
DISC_MISSING_AFFIL::DISC_MISSING_AFFIL is not implemented\n\
DISC_SUBMITBLOCK_CONFLICT::DISC_SUBMITBLOCK_CONFLICT is not implemented\n\
DISC_TITLE_AUTHOR_CONFLICT::DISC_TITLE_AUTHOR_CONFLICT is not implemented\n\
DISC_UNPUB_PUB_WITHOUT_TITLE::DISC_UNPUB_PUB_WITHOUT_TITLE is not implemented\n\
DISC_USA_STATE::DISC_USA_STATE is not implemented\n\
ONCALLER_CITSUB_AFFIL_DUP_TEXT::ONCALLER_CITSUB_AFFIL_DUP_TEXT is not implemented\n\
ONCALLER_CONSORTIUM::ONCALLER_CONSORTIUM is not implemented\n");

    BOOST_CHECK_EQUAL(list[14]->Format(output_config, scope),
        "ONCALLER_SUPERFLUOUS_GENE::Feature tests\n\
DISC_BACTERIA_SHOULD_NOT_HAVE_MRNA::DISC_BACTERIA_SHOULD_NOT_HAVE_MRNA is not implemented\n\
DISC_BADLEN_TRNA::DISC_BADLEN_TRNA is not implemented\n\
DISC_BAD_GENE_STRAND::DISC_BAD_GENE_STRAND is not implemented\n\
DISC_CDS_HAS_NEW_EXCEPTION::DISC_CDS_HAS_NEW_EXCEPTION is not implemented\n\
DISC_CDS_WITHOUT_MRNA::DISC_CDS_WITHOUT_MRNA is not implemented\n\
DISC_EXON_INTRON_CONFLICT::DISC_EXON_INTRON_CONFLICT is not implemented\n\
DISC_GENE_PARTIAL_CONFLICT::DISC_GENE_PARTIAL_CONFLICT is not implemented\n\
DISC_MICROSATELLITE_REPEAT_TYPE::DISC_MICROSATELLITE_REPEAT_TYPE is not implemented\n\
DISC_MRNA_ON_WRONG_SEQUENCE_TYPE::DISC_MRNA_ON_WRONG_SEQUENCE_TYPE is not implemented\n\
DISC_NON_GENE_LOCUS_TAG::DISC_NON_GENE_LOCUS_TAG is not implemented\n\
DISC_PSEUDO_MISMATCH::DISC_PSEUDO_MISMATCH is not implemented\n\
DISC_RBS_WITHOUT_GENE::DISC_RBS_WITHOUT_GENE is not implemented\n\
DISC_RNA_NO_PRODUCT::DISC_RNA_NO_PRODUCT is not implemented\n\
DISC_SHORT_INTRON::DISC_SHORT_INTRON is not implemented\n\
DISC_SHORT_RRNA::DISC_SHORT_RRNA is not implemented\n\
MULTIPLE_CDS_ON_MRNA::MULTIPLE_CDS_ON_MRNA is not implemented\n\
ONCALLER_GENE_MISSING::ONCALLER_GENE_MISSING is not implemented\n\
ONCALLER_ORDERED_LOCATION::ONCALLER_ORDERED_LOCATION is not implemented\n\
ONCALLER_SUPERFLUOUS_GENE::ONCALLER_SUPERFLUOUS_GENE is not implemented\n\
TEST_EXON_ON_MRNA::TEST_EXON_ON_MRNA is not implemented\n\
TEST_MRNA_OVERLAPPING_PSEUDO_GENE::TEST_MRNA_OVERLAPPING_PSEUDO_GENE is not implemented\n\
TEST_UNNECESSARY_VIRUS_GENE::TEST_UNNECESSARY_VIRUS_GENE is not implemented\n");

    BOOST_CHECK_EQUAL(list[15]->Format(output_config, scope),
        "DISC_POSSIBLE_LINKER::DISC_POSSIBLE_LINKER is not implemented\n");
    BOOST_CHECK_EQUAL(list[16]->Format(output_config, scope),
        "DISC_HAPLOTYPE_MISMATCH::DISC_HAPLOTYPE_MISMATCH is not implemented\n");
    BOOST_CHECK_EQUAL(list[17]->Format(output_config, scope),
        "DISC_FLATFILE_FIND_ONCALLER::DISC_FLATFILE_FIND_ONCALLER is not implemented\n");
    BOOST_CHECK_EQUAL(list[18]->Format(output_config, scope),
        "DISC_CDS_PRODUCT_FIND::DISC_CDS_PRODUCT_FIND is not implemented\n");
    BOOST_CHECK_EQUAL(list[19]->Format(output_config, scope),
        "DISC_SUSPICIOUS_NOTE_TEXT::DISC_SUSPICIOUS_NOTE_TEXT is not implemented\n");
    BOOST_CHECK_EQUAL(list[20]->Format(output_config, scope),
        "DISC_CHECK_RNA_PRODUCTS_AND_COMMENTS::DISC_CHECK_RNA_PRODUCTS_AND_COMMENTS is not implemented\n");
    BOOST_CHECK_EQUAL(list[21]->Format(output_config, scope),
        "DISC_INTERNAL_TRANSCRIBED_SPACER_RRNA::DISC_INTERNAL_TRANSCRIBED_SPACER_RRNA is not implemented\n");
    BOOST_CHECK_EQUAL(list[22]->Format(output_config, scope),
        "ONCALLER_COMMENT_PRESENT::ONCALLER_COMMENT_PRESENT is not implemented\n");
    BOOST_CHECK_EQUAL(list[23]->Format(output_config, scope),
        "DISC_SUSPECT_PRODUCT_NAME::DISC_SUSPECT_PRODUCT_NAME is not implemented\n");
    BOOST_CHECK_EQUAL(list[24]->Format(output_config, scope),
        "DISC_FLATFILE_FIND_ONCALLER_FIXABLE::DISC_FLATFILE_FIND_ONCALLER_FIXABLE is not implemented\n");
    BOOST_CHECK_EQUAL(list[25]->Format(output_config, scope),
        "DISC_FLATFILE_FIND_ONCALLER_UNFIXABLE::DISC_FLATFILE_FIND_ONCALLER_UNFIXABLE is not implemented\n");
    BOOST_CHECK_EQUAL(list[26]->Format(output_config, scope),
        "DIVISION_CODE_CONFLICTS::DIVISION_CODE_CONFLICTS is not implemented\n");
    BOOST_CHECK_EQUAL(list[27]->Format(output_config, scope),
        "ONCALLER_HAS_STANDARD_NAME::ONCALLER_HAS_STANDARD_NAME is not implemented\n");
    BOOST_CHECK_EQUAL(list[28]->Format(output_config, scope),
        "ONCALLER_MISSING_STRUCTURED_COMMENTS::ONCALLER_MISSING_STRUCTURED_COMMENTS is not implemented\n");
    BOOST_CHECK_EQUAL(list[29]->Format(output_config, scope),
        "ONCALLER_SWITCH_STRUCTURED_COMMENT_PREFIX::ONCALLER_SWITCH_STRUCTURED_COMMENT_PREFIX is not implemented\n");
    BOOST_CHECK_EQUAL(list[30]->Format(output_config, scope),
        "TEST_MISSING_PRIMER::TEST_MISSING_PRIMER is not implemented\n");
    BOOST_CHECK_EQUAL(list[31]->Format(output_config, scope),
        "TEST_MRNA_SEQUENCE_MINUS_STRAND_FEATURES::TEST_MRNA_SEQUENCE_MINUS_STRAND_FEATURES is not implemented\n");
    BOOST_CHECK_EQUAL(list[32]->Format(output_config, scope),
        "TEST_ORGANELLE_PRODUCTS::TEST_ORGANELLE_PRODUCTS is not implemented\n");
    BOOST_CHECK_EQUAL(list[33]->Format(output_config, scope),
        "TEST_SHORT_LNCRNA::TEST_SHORT_LNCRNA is not implemented\n");
    BOOST_CHECK_EQUAL(list[34]->Format(output_config, scope),
        "UNCULTURED_NOTES_ONCALLER::UNCULTURED_NOTES_ONCALLER is not implemented\n");




}


BOOST_AUTO_TEST_CASE(Test_MultiEntryDropReferenceFilename)
{
    // set up for analysis workflow
    CRef<CAnalysisWorkflow> report_maker(new CAnalysisWorkflow());
    // enable five tests
    report_maker->Enable(DiscRepNmSpc::kDISC_SUPERFLUOUS_GENE, true);
    report_maker->Enable(DiscRepNmSpc::kMISSING_GENES, true);
    report_maker->Enable(DiscRepNmSpc::kDISC_COUNT_NUCLEOTIDES, true);
    report_maker->Enable(DiscRepNmSpc::kDISC_SOURCE_QUALS_ASNDISC, true);
    report_maker->Enable(DiscRepNmSpc::kDISC_DUP_SRC_QUAL, true);
    report_maker->BuildTestList();

    CRef<CObjectManager> objmgr = CObjectManager::GetInstance();
    CScope scope(*objmgr);
    scope.AddDefaults();

    // check file "a.txt"
    CRef <CSeq_entry> entry = BuildGoodNucProtSet();
    CSeq_entry_Handle seh = scope.AddTopLevelSeqEntry(*entry);
    report_maker->CollectData(seh, true, "a.txt");
    scope.RemoveTopLevelSeqEntry(seh);

    // check file "b.txt"
    entry = BuildGoodSeq();
    CRef<objects::CSeq_feat> gene = unit_test_util::AddMiscFeature(entry);
    gene->SetData().SetGene().SetLocus("abc");
    gene->ResetComment();
    seh = scope.AddTopLevelSeqEntry(*entry);
    report_maker->CollectData(seh, true, "b.txt");
    scope.RemoveTopLevelSeqEntry(seh);

    // check file "c.txt"
    entry = s_BuildEntryForSrcQualProblem();
    seh = scope.AddTopLevelSeqEntry(*entry);
    report_maker->CollectData(seh, true, "c.txt");
    scope.RemoveTopLevelSeqEntry(seh);

    report_maker->Collate();

    // set up output configuration
    DiscRepNmSpc::CReportOutputConfig output_config;
    output_config.SetFormat(CReportOutputConfig::eFormatText);
    output_config.SetExpand(DiscRepNmSpc::kDISC_SUPERFLUOUS_GENE);
    output_config.SetExpand(DiscRepNmSpc::kMISSING_GENES);
    output_config.SetExpand(DiscRepNmSpc::kDISC_SOURCE_QUALS_ASNDISC);
    output_config.SetExpand(DiscRepNmSpc::kDISC_DUP_SRC_QUAL);

    CRef<COrderPolicy> order_policy(new COrderPolicy());
    report_maker->SetOrderPolicy(order_policy);

    DiscRepNmSpc::TReportItemList list = report_maker->GetResults();

    BOOST_CHECK_EQUAL(list.size(), 9);
    BOOST_CHECK_EQUAL(list[0]->Format(output_config), 
        "MISSING_GENES::1 feature has no genes.\na.txt:CDS\tfake protein name\tlcl|nuc:1-27\t\n");
    BOOST_CHECK_EQUAL(list[1]->Format(output_config), 
        "EXTRA_GENES::1 gene feature is not associated with a CDS or RNA feature.\n\
b.txt:Gene\tabc\tlcl|good:1-11\t\n\
EXTRA_GENES::1 non-pseudo gene feature is not associated with a CDS or RNA feature and does not have frameshift in the comment.\n\
b.txt:Gene\tabc\tlcl|good:1-11\t\n");
    BOOST_CHECK_EQUAL(list[2]->Format(output_config), 
        "DISC_COUNT_NUCLEOTIDES::4 nucleotide Bioseqs are present\n\
a.txt:lcl|nuc (length 60)\n\
b.txt:lcl|good (length 60)\n\
c.txt:lcl|good_1 (length 60)\n\
c.txt:lcl|good_2 (length 60)\n");
    BOOST_CHECK_EQUAL(list[3]->Format(output_config),
        "DISC_SOURCE_QUALS_ASNDISC::cell_type (some missing, all unique)\n\
DISC_SOURCE_QUALS_ASNDISC::2 sources are missing cell_type\n\
a.txt:lcl|nuc (2 components):Sebaea microphylla\n\
b.txt:lcl|good:Sebaea microphylla\n\
DISC_SOURCE_QUALS_ASNDISC::2 sources have unique values for cell_type\n\
c.txt:lcl|good_1:Sebaea microphylla\n\
c.txt:lcl|good_2:Sebaea microphylla\n");
    BOOST_CHECK_EQUAL(list[4]->Format(output_config),
        "DISC_SOURCE_QUALS_ASNDISC::chromosome (all present, all same)\n\
DISC_SOURCE_QUALS_ASNDISC::4 sources have '1' for chromosome\n\
a.txt:lcl|nuc (2 components):Sebaea microphylla\n\
b.txt:lcl|good:Sebaea microphylla\n\
c.txt:lcl|good_1:Sebaea microphylla\n\
c.txt:lcl|good_2:Sebaea microphylla\n");
    BOOST_CHECK_EQUAL(list[5]->Format(output_config),
        "DISC_SOURCE_QUALS_ASNDISC::isolate (some missing, all same)\n\
DISC_SOURCE_QUALS_ASNDISC::2 sources are missing isolate\n\
a.txt:lcl|nuc (2 components):Sebaea microphylla\n\
b.txt:lcl|good:Sebaea microphylla\n\
DISC_SOURCE_QUALS_ASNDISC::2 sources have 'abc' for isolate\n\
c.txt:lcl|good_1:Sebaea microphylla\n\
c.txt:lcl|good_2:Sebaea microphylla\n");
    BOOST_CHECK_EQUAL(list[6]->Format(output_config),
        "DISC_SOURCE_QUALS_ASNDISC::strain (some missing, all unique, some multi)\n\
DISC_SOURCE_QUALS_ASNDISC::3 sources are missing strain\n\
a.txt:lcl|nuc (2 components):Sebaea microphylla\n\
b.txt:lcl|good:Sebaea microphylla\n\
c.txt:lcl|good_2:Sebaea microphylla\n\
DISC_SOURCE_QUALS_ASNDISC::1 source has multiple strain qualifiers\n\
c.txt:lcl|good_1:Sebaea microphylla\n\
DISC_SOURCE_QUALS_ASNDISC::1 source has unique values for strain\n\
c.txt:lcl|good_1:Sebaea microphylla\n\
");
    BOOST_CHECK_EQUAL(list[7]->Format(output_config), 
        "DISC_SOURCE_QUALS_ASNDISC::taxid (all present, all same)\n\
DISC_SOURCE_QUALS_ASNDISC::4 sources have '592768' for taxid\n\
a.txt:lcl|nuc (2 components):Sebaea microphylla\n\
b.txt:lcl|good:Sebaea microphylla\n\
c.txt:lcl|good_1:Sebaea microphylla\n\
c.txt:lcl|good_2:Sebaea microphylla\n");
    BOOST_CHECK_EQUAL(list[8]->Format(output_config), 
        "DISC_SOURCE_QUALS_ASNDISC::taxname (all present, all same)\n\
DISC_SOURCE_QUALS_ASNDISC::4 sources have 'Sebaea microphylla' for taxname\n\
a.txt:lcl|nuc (2 components):Sebaea microphylla\n\
b.txt:lcl|good:Sebaea microphylla\n\
c.txt:lcl|good_1:Sebaea microphylla\n\
c.txt:lcl|good_2:Sebaea microphylla\n");


}


BOOST_AUTO_TEST_CASE(Test_OVERLAPPING_CDS)
{
    CRef <CSeq_entry> entry = BuildGoodNucProtSet();
    CRef<CSeq_feat> cds = unit_test_util::GetCDSFromGoodNucProtSet(entry);
    CRef<objects::CSeq_feat> cds2 = unit_test_util::AddMiscFeature(entry);
    cds2->Assign(*cds);
    CRef<CSeq_entry> protein = unit_test_util::GetProteinSequenceFromGoodNucProtSet(entry);
    CRef<CSeq_entry> prot2(new CSeq_entry());
    prot2->Assign(*protein);
    unit_test_util::ChangeId(prot2, "_2");
    cds2->SetProduct().SetWhole().SetLocal().SetStr() += "_2";
    entry->SetSet().SetSeq_set().push_back(prot2);

    CRef<CObjectManager> objmgr = CObjectManager::GetInstance();
    CScope scope(*objmgr);
    scope.AddDefaults();
    CSeq_entry_Handle seh = scope.AddTopLevelSeqEntry(*entry);

    CRef<CAnalysisWorkflow> report_maker(new CAnalysisWorkflow());
    report_maker->Enable(DiscRepNmSpc::kDISC_OVERLAPPING_CDS, true);
    report_maker->BuildTestList();

    report_maker->CollectData(seh);
    report_maker->Collate();

    DiscRepNmSpc::CReportOutputConfig output_config;
    output_config.SetFormat(CReportOutputConfig::eFormatText);
    output_config.SetExpand(DiscRepNmSpc::kDISC_OVERLAPPING_CDS);
    DiscRepNmSpc::TReportItemList list = report_maker->GetResults();

    BOOST_CHECK_EQUAL(list.size(), 1);
    BOOST_CHECK_EQUAL(list[0]->Format(output_config, scope), 
        "DISC_OVERLAPPING_CDS::2 coding regions overlap another coding region with a similar or identical name.\n\
CDS\tfake protein name\tlcl|nuc:1-27\t\nCDS\tfake protein name\tlcl|nuc:1-27\t\n\
DISC_OVERLAPPING_CDS::2 coding regions overlap another coding region with a similar or identical name that do not have the appropriate note text.\n\
CDS\tfake protein name\tlcl|nuc:1-27\t\nCDS\tfake protein name\tlcl|nuc:1-27\t\n");

    BOOST_CHECK(report_maker->Autofix(seh));
    report_maker->ResetData();
    report_maker->CollectData(seh);
    report_maker->Collate();
    list = report_maker->GetResults();
    BOOST_CHECK_EQUAL(list.size(), 1);
    BOOST_CHECK_EQUAL(list[0]->Format(output_config, scope), 
        "DISC_OVERLAPPING_CDS::2 coding regions overlap another coding region with a similar or identical name.\n\
CDS\tfake protein name\tlcl|nuc:1-27\t\nCDS\tfake protein name\tlcl|nuc:1-27\t\n\
DISC_OVERLAPPING_CDS::2 coding regions overlap another coding region with a similar or identical name but have the appropriate note text.\n\
CDS\tfake protein name\tlcl|nuc:1-27\t\nCDS\tfake protein name\tlcl|nuc:1-27\t\n");
}


CRef<CSeq_feat> AddContainedCDS(CRef<CSeq_entry> entry, size_t offset)
{
    string suffix = "_" + NStr::NumericToString(offset);
    CRef<CSeq_feat> cds = unit_test_util::GetCDSFromGoodNucProtSet(entry);
    CRef<objects::CSeq_feat> cds2 = unit_test_util::AddMiscFeature(entry);
    cds2->Assign(*cds);  
    cds2->SetLocation().SetInt().SetFrom(cds->GetLocation().GetInt().GetFrom() + offset);
    CRef<CSeq_entry> protein = unit_test_util::GetProteinSequenceFromGoodNucProtSet(entry);
    CRef<CSeq_entry> prot2(new CSeq_entry());
    prot2->Assign(*protein);
    unit_test_util::ChangeId(prot2, suffix);
    CRef<CSeq_id> id = prot2->SetSeq().SetId().front();

    cds2->SetProduct().SetWhole().Assign(*id);
    entry->SetSet().SetSeq_set().push_back(prot2);
    CRef<CSeq_feat> protfeat2 = prot2->SetAnnot().front()->SetData().SetFtable().front();
    protfeat2->SetData().SetProt().SetName().front() += suffix;
    return cds2;
}


BOOST_AUTO_TEST_CASE(Test_CONTAINED_CDS)
{
    CRef <CSeq_entry> entry = BuildGoodNucProtSet();
    CRef<CSeq_feat> cds2 = AddContainedCDS(entry, 1);
    CRef<CSeq_feat> cds3 = AddContainedCDS(entry, 2);

    CRef<CObjectManager> objmgr = CObjectManager::GetInstance();
    CScope scope(*objmgr);
    scope.AddDefaults();
    CSeq_entry_Handle seh = scope.AddTopLevelSeqEntry(*entry);

    CRef<CAnalysisWorkflow> report_maker(new CAnalysisWorkflow());
    report_maker->Enable(DiscRepNmSpc::kDISC_CONTAINED_CDS, true);
    report_maker->BuildTestList();

    report_maker->CollectData(seh);
    report_maker->Collate();

    DiscRepNmSpc::CReportOutputConfig output_config;
    output_config.SetFormat(CReportOutputConfig::eFormatText);
    output_config.SetExpand(DiscRepNmSpc::kDISC_CONTAINED_CDS);
    DiscRepNmSpc::TReportItemList list = report_maker->GetResults();

    BOOST_CHECK_EQUAL(list.size(), 1);
    BOOST_CHECK_EQUAL(list[0]->Format(output_config, scope), 
        "CONTAINED_CDS::3 coding regions are completely contained in another coding region on the same strand.\n\
CDS\tfake protein name\tlcl|nuc:1-27\t\n\
CDS\tfake protein name_1\tlcl|nuc:2-27\t\n\
CDS\tfake protein name_1_2\tlcl|nuc:3-27\t\n");

    cds2->SetComment("completely contained in another CDS");
    report_maker->ResetData();
    report_maker->CollectData(seh);
    report_maker->Collate();
    list = report_maker->GetResults();
    BOOST_CHECK_EQUAL(list.size(), 1);
    BOOST_CHECK_EQUAL(list[0]->Format(output_config, scope), 
        "CONTAINED_CDS::3 coding regions are completely contained in another coding region.\n\
CDS\tfake protein name_1\tlcl|nuc:2-27\t\n\
CDS\tfake protein name\tlcl|nuc:1-27\t\n\
CDS\tfake protein name_1_2\tlcl|nuc:3-27\t\n\
CONTAINED_CDS::1 coding region is completely contained in another coding region but have note.\n\
CDS\tfake protein name_1\tlcl|nuc:2-27\t\n\
CONTAINED_CDS::2 coding regions are completely contained in another coding region on the same strand.\n\
CDS\tfake protein name\tlcl|nuc:1-27\t\n\
CDS\tfake protein name_1_2\tlcl|nuc:3-27\t\n\
");

    BOOST_CHECK_EQUAL(report_maker->Autofix(seh), true);
    report_maker->ResetData();
    report_maker->CollectData(seh);
    report_maker->Collate();
    list = report_maker->GetResults();
    BOOST_CHECK_EQUAL(list.size(), 0);
}

