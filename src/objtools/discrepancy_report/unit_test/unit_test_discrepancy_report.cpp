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
* Author:  Pavel Ivanov, NCBI
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

// This header must be included before all Boost.Test headers if there are any
#include <corelib/test_boost.hpp>

#include <corelib/test_boost.hpp>

#include <objtools/unit_test_util/unit_test_util.hpp>

#include <objtools/discrepancy_report/hDiscRep_tests.hpp>
#include <objtools/discrepancy_report/hDiscRep_output.hpp>
#include <objtools/discrepancy_report/hDiscRep_config.hpp>
#include <objtools/discrepancy_report/hUtilib.hpp>


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


BEGIN_NCBI_SCOPE
USING_NCBI_SCOPE;
USING_SCOPE(objects);
USING_SCOPE(DiscRepNmSpc);
USING_SCOPE(unit_test_util);

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

static CRef <CObjectManager> objmgr = CObjectManager::GetInstance();
static CRef <CScope> scope (new CScope(*objmgr));
void RunAndCheckTest(CRef <CSeq_entry>& entry, const string& test_name, const string& msg)
{
   objmgr.Reset(CObjectManager::GetInstance().GetPointer());
   scope.Reset(new CScope(*objmgr));
   CSeq_entry_Handle seh = scope->AddTopLevelSeqEntry(*entry);
   config->SetTopLevelSeqEntry(&seh);

   config->SetArg("e", test_name);
   CRef <CClickableItem> c_item(0);
   config->CollectTests();
   config->Run();
   CDiscRepOutput output_obj;
   output_obj.Export(c_item, test_name);
   if (msg == "print") {
      cerr << "desc " << c_item->description << endl;
      return;
   }
   if (c_item.Empty() || (c_item->description).empty()) {
      NCBITEST_CHECK_MESSAGE(!c_item.Empty(), "no report");
   }
   else {
     NCBITEST_CHECK_MESSAGE(c_item->item_list.size() == c_item->obj_list.size(),
              "The sizes of item_list and obj_list are not equal");
     NCBITEST_CHECK_MESSAGE(c_item->description == msg,
              "Test report is incorrect: " + c_item->description);
   }
};

void RunTest(CRef <CClickableItem>& c_item, const string& setting_name)
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

CRef <CSeq_feat> MakeRNAFeatWithExtName(const CRef <CSeq_entry> nuc_entry, CRNA_ref::EType type, const string ext_name)
{
   CRef <CSeq_feat> rna_feat(new CSeq_feat);
   CRef <CRNA_ref::C_Ext> rna_ext (new CRNA_ref::C_Ext);
   rna_ext->SetName(ext_name);
   rna_feat->SetData().SetRna().SetType(type);
   rna_feat->SetData().SetRna().SetExt(*rna_ext);
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
     default: break;
   }

   switch(subtype) {
     case CSeqFeatData::eSubtype_primer_bind:
        new_feat->SetData().SetImp().SetKey("primer_bind");
        break;
     case CSeqFeatData::eSubtype_exon:
        new_feat->SetData().SetImp().SetKey("exon");
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
                         (CSeqFeatData::ESubtype)0, fm, to));
};

void MakeBioSource(CRef <CSeq_entry>& entry, CBioSource::EGenome genome = CBioSource::eGenome_unknown);
void MakeBioSource(CRef <CSeq_entry>& entry, CBioSource::EGenome genome)
{
   entry->SetSeq().SetDescr().Set().front()->SetSource().SetGenome(genome);
};

void RunAndCheckMultiReports(CRef <CSeq_entry>& entry, const string& test_name, const set <string>& msgs)
{
   objmgr.Reset(CObjectManager::GetInstance().GetPointer());
   scope.Reset(new CScope(*objmgr));
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
   if (c_items.empty()) {
      NCBITEST_CHECK_MESSAGE(!c_items.empty(), "no report");
   }
   else if (c_items.size() != msgs.size()) {
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


/*
BOOST_AUTO_TEST_CASE(DISC_SUBMITBLOCK_CONFLICT)
{
   CRef <CSeq_submit> seq_submit (new CSeq_submit);
   CRef <CSubmit_block> submit_blk(new submit_blk);
   submit_blk->SetContact_info->SetEmail("a@x.com");
   CRef <CAuthor> auths (new CAuthor);
   CRef <CPerson_id> name (new CPerson_id);
   name->SetName()->SetLast("a");
   name->SetName()->SetFirst("a");
   name->SetName()->SetInitials("a");
   CRef <CAffil> affil (new CAffil); 

};
*/

BOOST_AUTO_TEST_CASE(DISC_SEGSETS_PRESENT)
{
   CRef <CSeq_entry> entry = BuildGoodEcoSet();
   entry->SetSet().SetClass(CBioseq_set::eClass_segset);
   RunAndCheckTest(entry, "DISC_SEGSETS_PRESENT", "1 segset is present.");
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
   MakeBioSource(entry, CBioSource::eGenome_proviral);
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

BOOST_AUTO_TEST_CASE(MULTIPLE_CDS_ON_MRNA)
{
   CRef <CSeq_entry> entry = BuildGoodRnaSeq();
   SetBiomol(entry, CMolInfo::eBiomol_mRNA);
   unsigned len = entry->GetSeq().GetInst().GetLength();
   // cd1
   CRef <CSeq_feat> cds = MakeCDs(entry, 0, (int)(len/2));
   cds->SetComment("comment: coding region disrupted by sequencing gap");
   AddFeat(cds, entry);
   
   // cd2 
   cds.Reset(MakeCDs(entry, (int)(len/2+1), len-1));
   cds->SetComment("comment: coding region disrupted by sequencing gap");
   AddFeat(cds, entry);
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
                    "4 CDSs, RNAs, and genes have mismatching pseudos.");
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
   MakeBioSource(entry, CBioSource::eGenome_plastid);
//   entry->SetSeq().SetDescr().Set().front()->SetSource().SetGenome(CBioSource::eGenome_plastid);
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
   MakeBioSource(entry, CBioSource::eGenome_plastid);
//   entry->SetSeq().SetDescr().Set().front()->SetSource().SetGenome(CBioSource::eGenome_plastid);
  
   RunAndCheckTest(entry, "FIND_DUP_TRNAS", 
      "2 tRNA features on LocusCollidesWithLocusTag have the same name (Phe).");
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
                   "1 microsatellite does not have a repeat type of tandem");
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
                    "1 RBS feature does not have overlapping genes");
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
   CRef <CSeq_entry> nuc_entry = GetNucleotideSequenceFromGoodNucProtSet(entry);
   CRef <CSeq_feat> 
     new_rna =MakeRNAFeatWithExtName(nuc_entry, CRNA_ref::eType_rRNA,"5s_rRNA");
   AddFeat(new_rna, nuc_entry);
   new_rna = MakeRNAFeatWithExtName(nuc_entry, CRNA_ref::eType_rRNA,"16s_rRNA");
   AddFeat(new_rna, nuc_entry);
   new_rna = MakeRNAFeatWithExtName(nuc_entry, CRNA_ref::eType_rRNA,"23s_rRNA");
   AddFeat(new_rna, nuc_entry);
   new_rna 
      = MakeRNAFeatWithExtName(nuc_entry, CRNA_ref::eType_rRNA,"partial 16S");
   AddFeat(new_rna, nuc_entry);
   new_rna 
    = MakeRNAFeatWithExtName(nuc_entry, CRNA_ref::eType_rRNA,"12S rRNA domain");
   AddFeat(new_rna, nuc_entry);
   new_rna 
    =MakeRNAFeatWithExtName(nuc_entry, CRNA_ref::eType_rRNA,"8S ribosomal RNA");
   AddFeat(new_rna, nuc_entry);

   // change protein name
   SetNucProtSetProductName(entry, "fake domain");

   RunAndCheckTest(entry, "DISC_SUSPECT_RRNA_PRODUCTS",
                     "7 rRNA product names contain suspect phrase");
};

BOOST_AUTO_TEST_CASE(DISC_FLATFILE_FIND_ONCALLER)
{
   CRef <CSeq_entry> entry = unit_test_util::BuildGoodNucProtSet();
   CRef <CSeq_entry> prot_entry = entry->SetSet().SetSeq_set().back();
   prot_entry->SetSeq().SetInst().SetSeq_data().SetIupacaa().Set("SHAEMNVV");
//   RunAndCheckTest(entry, "DISC_FLATFILE_FIND_ONCALLER", "print");
//                    "1 object contains haem");
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
   CRef<CObjectManager> objmgr = CObjectManager::GetInstance();
   CRef <CScope> scope(new CScope(*objmgr));
   CSeq_entry_Handle seh = scope->AddTopLevelSeqEntry(*entry);
 
   config->SetTopLevelSeqEntry(&seh);
   config->SetArg("e", "MISSING_GENPRODSET_TRANSCRIPT_ID");
   CRef <CClickableItem> c_item(0);   
   RunTest(c_item, "MISSING_GENPRODSET_TRANSCRIPT_ID"); 
   if (!c_item.Empty()) {
      NCBITEST_CHECK_MESSAGE(c_item.Empty(), 
               "Wrong report: " + c_item->description);
   }

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
   RunAndCheckTest(entry, "MISSING_LOCUS_TAGS", "1 gene has no locus tags.");
};

BOOST_AUTO_TEST_CASE(DISC_COUNT_NUCLEOTIDES)
{
   CRef <CSeq_entry> entry (new CSeq_entry);
   CNcbiIstrstream istr(sc_TestEntryCollidingLocusTags);
   istr >> MSerial_AsnText >> *entry;
   RunAndCheckTest(entry, "DISC_COUNT_NUCLEOTIDES",
                   "1 nucleotide Bioseq is present.");
};


END_NCBI_SCOPE
