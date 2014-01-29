/*  $Id$
 * =========================================================================
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
 * =========================================================================
 *
 * Author:  Jie Chen
 *
 * File Description:
 *   Tests and report data collection for Cpp Discrepany Report
 *
 * $Log:$ 
 */

#include <ncbi_pch.hpp>
#include <serial/enumvalues.hpp>
#include <objtools/format/items/source_item.hpp>
#include <objtools/format/items/feature_item.hpp>
#include <objtools/validator/utilities.hpp>
#include <objects/valid/Comment_rule.hpp>
#include <objects/valid/Comment_set.hpp>
#include <objects/submit/Contact_info.hpp>
#include <objects/seq/Seq_ext.hpp>
#include <objects/seq/Seg_ext.hpp>
#include <objects/seq/seqport_util.hpp>
#include <objects/seq/Seq_data.hpp>
#include <objects/seq/Seqdesc.hpp>
#include <objects/seq/Pubdesc.hpp>
#include <objects/seq/Seq_ext.hpp>
#include <objects/seq/Delta_ext.hpp>
#include <objects/seq/Delta_seq.hpp>
#include <objects/seq/Seq_literal.hpp>
#include <objects/seq/MolInfo.hpp>
#include <objects/seqfeat/Cdregion.hpp>
#include <objects/seqfeat/Code_break.hpp>
#include <objects/seqfeat/Feat_id.hpp>
#include <objects/seqfeat/PCRReactionSet.hpp>
#include <objects/seqfeat/PCRPrimerSet.hpp>
#include <objects/seqfeat/RNA_gen.hpp>
#include <objects/seqfeat/RNA_qual.hpp>
#include <objects/seqfeat/RNA_qual_set.hpp>
#include <objects/seqfeat/RNA_ref.hpp>
#include <objects/seqfeat/SeqFeatXref.hpp>
#include <objects/seqblock/GB_block.hpp>
#include <objects/macro/Field_type.hpp>
#include <objects/general/Name_std.hpp>
#include <objects/general/Date.hpp>
#include <objects/general/Date_std.hpp>
#include <objects/general/Dbtag.hpp>
#include <objects/general/User_object.hpp>
#include <objects/general/User_field.hpp>
#include <objects/general/Object_id.hpp>
#include <objects/biblio/Affil.hpp>
#include <objects/biblio/Author.hpp>
#include <objects/biblio/Cit_gen.hpp>
#include <objects/biblio/Cit_sub.hpp>
#include <objects/biblio/Cit_art.hpp>
#include <objects/biblio/Cit_book.hpp>
#include <objects/biblio/Cit_proc.hpp>
#include <objects/biblio/Cit_let.hpp>
#include <objects/biblio/Cit_pat.hpp>
#include <objects/biblio/Cit_jour.hpp>
#include <objects/biblio/citation_base.hpp>
#include <objects/biblio/Title.hpp>
#include <objects/biblio/Imprint.hpp>
#include <objects/pub/Pub_equiv.hpp>
#include <objmgr/annot_selector.hpp>
#include <objmgr/scope.hpp>
#include <objmgr/util/sequence.hpp>
#include <objmgr/util/seq_loc_util.hpp>
#include <objmgr/util/feature.hpp>
#include <objmgr/seq_vector_ci.hpp>
#include <objmgr/seq_vector.hpp>
#include <objmgr/seq_map.hpp>
#include <objmgr/seq_feat_handle.hpp>
#include <util/xregexp/regexp.hpp>

#include <objtools/discrepancy_report/hDiscRep_config.hpp>
#include <objtools/discrepancy_report/hDiscRep_tests.hpp>
#include <objtools/discrepancy_report/hchecking_class.hpp>
#include <objtools/discrepancy_report/hUtilib.hpp>

BEGIN_NCBI_SCOPE;

USING_NCBI_SCOPE;
USING_SCOPE(objects);
USING_SCOPE(DiscRepNmSpc);

//using namespace feature;

static CDiscRepInfo thisInfo;
static string strtmp(kEmptyStr);
static CDiscTestInfo thisTest;
static set <string>::iterator end_it = thisTest.tests_run.end();
static CSuspectRuleCheck rule_check;
static CTestGrp thisGrp;

// ini. CDiscTestInfo
bool CDiscTestInfo :: is_Aa_run;
bool CDiscTestInfo :: is_AllAnnot_run;
bool CDiscTestInfo :: is_BacPartial_run;
bool CDiscTestInfo :: is_Bad_Gene_Nm;
bool CDiscTestInfo :: is_Bases_N_run;
bool CDiscTestInfo :: is_BASES_N_run;
bool CDiscTestInfo :: is_BioSet_run;
bool CDiscTestInfo :: is_BIOSRC_run;
bool CDiscTestInfo :: is_Biosrc_Orgmod_run;
bool CDiscTestInfo :: is_CDs_run;
bool CDiscTestInfo :: is_CdTransl_run;
bool CDiscTestInfo :: is_Comment_run;
bool CDiscTestInfo :: is_Defl_run;
bool CDiscTestInfo :: is_DESC_user_run;
bool CDiscTestInfo :: is_FeatCnt_run;
bool CDiscTestInfo :: is_Genes_run;
bool CDiscTestInfo :: is_Genes_oncall_run;
bool CDiscTestInfo :: is_GP_Set_run;
bool CDiscTestInfo :: is_IncnstUser_run;
bool CDiscTestInfo :: is_LocusTag_run;
bool CDiscTestInfo :: is_MolInfo_run;
bool CDiscTestInfo :: is_MRNA_run;  // need modification, has to be is_mRNACDs_run
bool CDiscTestInfo :: is_mRNA_run;
bool CDiscTestInfo :: is_Prot_run;
bool CDiscTestInfo :: is_ProtFeat_run;
bool CDiscTestInfo :: is_Pub_run;
bool CDiscTestInfo :: is_Quals_run;
bool CDiscTestInfo :: is_RRna_run;
bool CDiscTestInfo :: is_Subsrc_run;
bool CDiscTestInfo :: is_SusPhrase_run;
bool CDiscTestInfo :: is_SusProd_run;
bool CDiscTestInfo :: is_TaxCflts_run;
bool CDiscTestInfo :: is_TaxDef_run;
bool CDiscTestInfo :: is_TRna_run;

typedef map <string, CRef < GeneralDiscSubDt > > Str2SubDt;
typedef map <int, vector <string> > Int2Strs;

static vector <CRef < GeneralDiscSubDt > > INCONSISTENT_BIOSOURCE_biosrc;
static vector < CRef < CClickableItem > > DUPLICATE_GENE_LOCUS_locuss;
static vector < CRef < GeneralDiscSubDt > > SUSPECT_PROD_NAMES_feat;
static unsigned DISC_QUALITY_SCORES_graph=0;
static Int2Strs PROJECT_ID_prot;
static Int2Strs PROJECT_ID_nuc;
static Str2Strs JOINED_FEATURES_sfs;

static Str2MapStr2Strs biosrc2qualvlu_nm;
Str2QualVlus qual_nm2qual_vlus;

// CBioseq
bool CBioseq_on_SUSPECT_RULE :: CategoryOkForBioSource(const CBioSource* biosrc_p, ESuspectNameType name_type) 
{
  if (name_type != eSuspectNameType_NoOrganelleForProkaryote) {
      return true;
  }
  else if (!HasTaxonomyID (*biosrc_p)) {
      return true;
  }
  else if (IsBiosrcEukaryotic(*biosrc_p)) {
      return false;
  }
  else {
      return true;
  }
};

//FindSuspectProductNamesCallback()
void CBioseq_on_SUSPECT_RULE :: FindSuspectProductNamesWithStaticList() 
{
   const CSeq_feat* feat_in_use = 0;
   const CBioSource* biosrc_p = 0;
   unsigned i;
   string pattern, test_name, test_desc;
   ITERATE (vector <const CSeq_feat*>, it, prot_feat) {
     if ((*it)->GetData().GetSubtype() != CSeqFeatData::eSubtype_prot) {
         continue;
     }
     const CSeqFeatData& sf_dt = (*it)->GetData();
     const CProt_ref& prot = sf_dt.GetProt();

     // add coding region rather than protein 
     if (sf_dt.GetSubtype() == CSeqFeatData::eSubtype_prot) {
         feat_in_use = sequence::GetCDSForProduct(m_bioseq_hl);
         // find BioSource, to check whether we want to run all categories
         biosrc_p = sequence::GetBioSource (m_bioseq_hl);
     }
     else {
         continue;
     }

     for (i=0; i< thisInfo.susterm_summ.size(); i++) {
       s_SuspectProductNameData this_term = thisInfo.suspect_prod_terms[i];
       if (!CategoryOkForBioSource(biosrc_p, this_term.fix_type)) continue;
       if (prot.CanGetName()) {
          ITERATE (list <string>, nit, prot.GetName()) {
             if (this_term.search_func && this_term.search_func(pattern, *nit)){
                 thisInfo.test_item_list[GetName()].push_back(
                      NStr::UIntToString((unsigned)this_term.fix_type) + "$" 
                      + NStr::UIntToString(i) + "#" 
                      + GetDiscItemText(*feat_in_use));
             }
          }
       }
     }
   }
};

static const char* suspect_name_category_names[] = {
  "Unknown category",
  "Typo",
  "Quick fix",
  "Organelles not appropriate in prokaryote",
  "Suspicious phrase; should this be nonfunctional?",
  "May contain database identifier more appropriate in note; remove from product name",
  "Remove organism from product name",
  "Possible parsing error or incorrect formatting; remove inappropriate symbols",
  "Implies evolutionary relationship; change to -like protein",
  "Add protein to the end of product name",
  "Unknown category"
};

void CBioseq_on_SUSPECT_RULE :: GetReportForStaticList(CRef <CClickableItem>& c_item)
{
   Str2Strs fixtp2term_feats, term2feats;
   GetTestItemList(c_item->item_list, fixtp2term_feats);
   c_item->item_list.clear();

   unsigned fixtp, cnt;
   string pattern, test_name, test_desc;

   ITERATE (Str2Strs, fit, fixtp2term_feats) {
      fixtp = NStr::StringToUInt(fit->first);
      CRef <CClickableItem> name_cat_citem (new CClickableItem);

      term2feats.clear(); 
      GetTestItemList(fit->second, term2feats, "#");
      ITERATE (Str2Strs, tit, term2feats) {
        unsigned term_idx = NStr::StringToUInt(tit->first);
        pattern = thisInfo.susterm_summ[term_idx][0];
        test_name = thisInfo.susterm_summ[term_idx][1];
        if (thisTest.tests_run.find(test_name) == thisTest.tests_run.end()) {
            continue;
        }
        test_desc = thisInfo.susterm_summ[term_idx][2];
        cnt = tit->second.size();
        if (test_desc == "contain") {
            strtmp 
              = GetContainsComment(cnt, "protein name") + " '" + pattern + "'";
        }
        else {
            strtmp = GetNoun(cnt, "protein name") + " "
                      + GetOtherComment(cnt, test_desc + 's', test_desc) 
                      + " with " + pattern;
        }

        AddSubcategory(name_cat_citem, test_name, &(tit->second),
                       strtmp, strtmp, e_OtherComment);
      }

      AddSubcategory(c_item, test_name, &(name_cat_citem->item_list), 
                     suspect_name_category_names[fixtp], 
                     suspect_name_category_names[fixtp], e_OtherComment);
   }

   c_item->description 
         = GetContainsComment(c_item->item_list.size(), "product_name") 
            + "suspect phrase or characters";
   
   fixtp2term_feats.clear();
   term2feats.clear();
};

// FindSuspectProductNamesWithRulesCallback
void CBioseq_on_SUSPECT_RULE :: FindSuspectProductNamesWithRules()
{
   const CSeq_feat* feat_in_use = 0;
   const CBioSource* biosrc_p = 0;
   string prot_nm;
   unsigned rule_idx;
   ITERATE (vector <const CSeq_feat*>, it, prot_feat) {
     if ((*it)->GetData().GetSubtype() != CSeqFeatData::eSubtype_prot) {
         continue;
     }
     const CSeqFeatData& sf_dt = (*it)->GetData();
     const CProt_ref& prot = sf_dt.GetProt();

     // add coding region rather than protein
     if (sf_dt.GetSubtype() == CSeqFeatData::eSubtype_prot) {
         feat_in_use = sequence::GetCDSForProduct(m_bioseq_hl);
         // find BioSource, to check whether we want to run all categories
         biosrc_p = sequence::GetBioSource (m_bioseq_hl);
     }
     else {
         continue;
     }
     
     if (prot.CanGetName() && !prot.GetName().empty()) {
       prot_nm = *(prot.GetName().begin()); 
       rule_idx = 0;
       string test_name, summ;
       ITERATE (list <CRef <CSuspect_rule> >, rit, 
                                   thisInfo.suspect_prod_rules->Get()) {

         if (rule_check.DoesStringMatchSuspectRule(m_bioseq_hl, 
                                                    prot_nm, 
                                                    *feat_in_use, **rit)){
              thisInfo.test_item_list[GetName()].push_back(
                   NStr::UIntToString((int)(*rit)->GetRule_type()) + "$" 
                   + NStr::UIntToString(rule_idx) + "@" 
                   + GetDiscItemText(*feat_in_use));
              test_name = thisInfo.susrule_summ[rule_idx][0];
              summ = thisInfo.susrule_summ[rule_idx][2];
              thisInfo.test_item_objs[test_name + "$" + summ].push_back(
                                             CConstRef<CObject>(feat_in_use));
         }
         rule_idx ++;
       }
     }
   }
};

void CBioseq_on_SUSPECT_RULE :: GetReportForRules(CRef <CClickableItem>& c_item)
{
   Str2Strs fixtp2rule_feats, rule2feats;
   GetTestItemList(c_item->item_list, fixtp2rule_feats);
   c_item->item_list.clear();

   string test_name, fixtp_name, summ;
   unsigned rule_idx, fixtp;
   vector <string> arr;
   ITERATE (Str2Strs, fit, fixtp2rule_feats) {
     fixtp = NStr::StringToUInt(fit->first); 
     CRef <CClickableItem> name_cat_citem (new CClickableItem);

     rule2feats.clear();
     GetTestItemList(fit->second, rule2feats, "@");
     ITERATE (Str2Strs, rit, rule2feats) {
       rule_idx = NStr::StringToUInt(rit->first);
       test_name = thisInfo.susrule_summ[rule_idx][0];
       fixtp_name = thisInfo.susrule_summ[rule_idx][1];
       summ = thisInfo.susrule_summ[rule_idx][2];
       if (rit->second.size() > 1) {
          arr.clear();
          arr = NStr::Tokenize(summ, " ", arr);
          if (arr[0] == "is") {
            arr[0] == "are";
          } 
          else if (arr[0][arr[0].size()-1] == 's') {
            arr[0] = arr[0].substr(0, arr[0].size()-1);
          }
          summ = NStr::Join(arr, " ");
       }
       AddSubcategory(name_cat_citem, test_name+"$"+summ, &(rit->second), 
                      "feature " + summ, "features " + summ,  e_OtherComment);
     } 
     name_cat_citem->setting_name = GetName();
     name_cat_citem->description = fixtp_name;
     name_cat_citem->expanded = true;
     copy(name_cat_citem->item_list.begin(), name_cat_citem->item_list.end(),
          back_inserter(c_item->item_list));
     c_item->subcategories.push_back(name_cat_citem);
     copy(name_cat_citem->obj_list.begin(), name_cat_citem->obj_list.end(),
          back_inserter(c_item->obj_list));
   }   

   c_item->description 
      = GetContainsComment(c_item->item_list.size(), "product_name") 
          + "suspect phrase or characters";
   c_item->expanded = true;
   fixtp2rule_feats.clear();
   rule2feats.clear();
};


void CBioseq_on_SUSPECT_RULE :: TestOnObj(const CBioseq& bioseq)
{
   if (thisTest.is_SusProd_run) {
        return;
   }
   thisTest.is_SusProd_run = true;

   m_bioseq_hl = thisInfo.scope->GetBioseqHandle(bioseq);  // necessary?
   if (thisInfo.suspect_prod_rules->Get().empty()) {
       FindSuspectProductNamesWithStaticList();
   }
   else {
       FindSuspectProductNamesWithRules();
   }
};

void CBioseq_on_SUSPECT_RULE :: GetReport(CRef <CClickableItem>& c_item)
{
   if (thisInfo.suspect_prod_rules->Get().empty()) {
       GetReportForStaticList(c_item);
   }
   else {
       GetReportForRules(c_item);
   }
};


bool CBioseq_DISC_BAD_BGPIPE_QUALS :: x_HasFieldStrNocase(const CUser_object& uobj, const string& field, const string& str)
{
   if (uobj.HasField(field)) {
       const CUser_field& ufd = uobj.GetField(field);
       if (!ufd.GetData().IsStr()) {
          return false;
       }
       strtmp 
          = CUtf8::AsSingleByteString(ufd.GetData().GetStr(), eEncoding_Ascii);
       if (NStr::EqualNocase(strtmp, str)) {
           return true; 
       }
   } 
   return false;
};
 

bool CBioseq_DISC_BAD_BGPIPE_QUALS :: x_IsBGPipe(const CSeqdesc* sdp)
{
    const CUser_object& uobj = sdp->GetUser();
    if ( !uobj.GetType().IsStr() 
          || !NStr::EqualNocase(uobj.GetType().GetStr(), "StructuredComment")){
        return false ;
    }
    
    if (!x_HasFieldStrNocase(uobj, "StructuredCommentPrefix", 
                             "##Genome-Annotation-Data-START##")) {
        return false;
    }

    return (x_HasFieldStrNocase(uobj, "Annotation Pipeline", 
                                "NCBI Prokaryotic Genome Annotation Pipeline"));
};

bool CBioseq_DISC_BAD_BGPIPE_QUALS :: x_CodeBreakIsStopCodon(const list < CRef <CCode_break> >& code_brk)
{
   ITERATE (list <CRef <CCode_break> >, it, code_brk) {
      if ( (*it)->GetAa().IsNcbieaa() && (*it)->GetAa().GetNcbieaa() == 42) {
         return true;
      }
   }
   return false;
};

void CBioseq_DISC_BAD_BGPIPE_QUALS :: TestOnObj(const CBioseq& bioseq)
{
   // must not be refseq
   ITERATE (list <CRef <CSeq_id> >, it, bioseq.GetId()) {
      if ( (*it)->IsOther()) {
          return;
      }
   }

   // must be BGPIPE
   bool is_bgpipe = false;
   ITERATE (vector <const CSeqdesc*>, it, bioseq_user) {
     if ( (is_bgpipe = x_IsBGPipe(*it)) ) {
         break; 
     }
   }
   if (!is_bgpipe) {
        return;
   }

   ITERATE (vector <const CSeq_feat*>, it, all_feat) {
      strtmp = GetDiscItemText(**it);
      if ( (*it)->CanGetExcept_text() && !((*it)->GetExcept_text().empty())) {
         thisInfo.test_item_list[GetName()].push_back(strtmp);
         thisInfo.test_item_objs[GetName()].push_back(CConstRef <CObject>(*it));
      }
      else if ( (*it)->GetData().IsCdregion()) {
          const CCdregion& cd = (*it)->GetData().GetCdregion();
          if (cd.CanGetCode_break() && !cd.GetCode_break().empty()
                  && ( !(*it)->CanGetComment() 
                         || (*it)->GetComment().find("ambiguity in stop codon")
                                   == string::npos
                         || !x_CodeBreakIsStopCodon( cd.GetCode_break())) ) {
            thisInfo.test_item_list[GetName()].push_back(strtmp);
            thisInfo.test_item_objs[GetName()].push_back(
                                                 CConstRef <CObject>(*it));
          }
      }
   }
};

void CBioseq_DISC_BAD_BGPIPE_QUALS :: GetReport(CRef <CClickableItem>& c_item)
{
   c_item->obj_list = thisInfo.test_item_objs[GetName()];
   c_item->description = GetContainsComment(c_item->item_list.size(), "feature")
                         + "invalid BGPIPE qualifiers";
};


void CBioseq_SUSPECT_PHRASES :: FindSuspectPhrases(const string& check_str, const CSeq_feat& seq_feat)
{
   ITERATE (vector <string>, it, thisInfo.suspect_phrases) {
     if (NStr::FindNoCase(check_str, *it) != string::npos) { 
        thisInfo.test_item_list[GetName()].push_back(
                                  *it + "$" + GetDiscItemText(seq_feat));
        thisInfo.test_item_objs[GetName()+"$"+*it].push_back(
                                   CConstRef <CObject>(&seq_feat));
     }
   }
};

void CBioseq_SUSPECT_PHRASES :: TestOnObj(const CBioseq& bioseq)
{
   string check_str;
   ITERATE (vector <const CSeq_feat*>, it, prot_feat) {
      const CProt_ref& prot_ref = (*it)->GetData().GetProt();
      if (prot_ref.CanGetDesc()) {
        check_str = prot_ref.GetDesc();
        if (!check_str.empty()) {
           FindSuspectPhrases(check_str, **it);
        }
      }
   }

   ITERATE (vector <const CSeq_feat*>, it, cd_feat) {
       if ( (*it)->CanGetComment()) {
          check_str = (*it)->GetComment();
          if (!check_str.empty()) {
              FindSuspectPhrases(check_str, **it);
          }
       }
   }
};

void CBioseq_SUSPECT_PHRASES :: GetReport(CRef <CClickableItem>& c_item)
{
   Str2Strs phrase2feats;
   GetTestItemList(c_item->item_list, phrase2feats);
   c_item->item_list.clear();
   
   ITERATE (Str2Strs, it, phrase2feats) {
      AddSubcategory(c_item, 
                     GetName() + "$" + it->first, 
                     &(it->second), 
                     "cds comment or protein description contains '", 
                     "cds comments or protein descriptions contain '", 
                     e_OtherComment, 
                     true, 
                     it->first + "'");
   }
   if (!c_item->item_list.empty()) {
       c_item->description 
           = GetOtherComment(c_item->item_list.size(), 
                             "cds comment or protein description contains",  
                             "cds comments or protein descriptions contain")
              + " suspect phrases.";
   }
   c_item->setting_name = "SUSPECT_PRODUCT_NAMES";
};


// SuspectRuleFeatCheckBack
void CBioseq_DISC_SUSPECT_RRNA_PRODUCTS :: TestOnObj(const CBioseq& bioseq)
{
   CTSE_Handle 
      tse_hl = (thisInfo.scope->GetBioseqHandle(bioseq)).GetTSE_Handle();
   string tse_text = GetDiscItemText(*(tse_hl.GetCompleteObject()));
   string check_val, sf_text;
   unsigned rule_idx;
   ITERATE (vector <const CSeq_feat*>, it, rrna_feat) {
      check_val = GetRNAProductString(**it); 
      sf_text = GetDiscItemText(**it);
      rule_idx = 0;
      ITERATE (list <CRef <CSuspect_rule> >, rit, 
                                       thisInfo.suspect_rna_rules->Get()) {
         if (rule_check.MatchesSuspectProductRule(check_val, **rit)) {
            if ((*rit)->CanGetFeat_constraint() 
                  && rule_check.DoesObjectMatchConstraintChoiceSet(
                                         **it, (*rit)->GetFeat_constraint())) {
                strtmp = NStr::UIntToString(rule_idx);
                thisInfo.test_item_list[GetName()].push_back(
                                      tse_text + "$" + strtmp + "#" + sf_text);
                strtmp = GetName() + "$" + tse_text + "_" + strtmp;
                thisInfo.test_item_objs[strtmp].push_back(
                                                     CConstRef <CObject>(*it));
            }
         }
         rule_idx ++;
      }
   }
};

void CBioseq_DISC_SUSPECT_RRNA_PRODUCTS :: GetReport(CRef <CClickableItem>& c_item)
{
   Str2Strs tse2rule_feats, rule2feats;
   GetTestItemList(c_item->item_list, tse2rule_feats);
   c_item->item_list.clear();
   
   unsigned rule_idx;
   ITERATE (Str2Strs, sit, tse2rule_feats) {
      if (sit != tse2rule_feats.begin()) {
        c_item.Reset(new CClickableItem);
        c_item->setting_name = GetName();
        thisInfo.disc_report_data.push_back(c_item);
      } 
      
      rule2feats.clear();
      GetTestItemList(sit->second, rule2feats);
      ITERATE (Str2Strs, rit, rule2feats) {
         rule_idx = NStr::StringToUInt(rit->first);
         strtmp = " rRNA product names " + thisInfo.rna_rule_summ[rule_idx];
         AddSubcategory(c_item, GetName() + "$" + sit->first + "_" + rit->first,
                        &(rit->second), strtmp, strtmp, e_OtherComment);
      } 
      c_item->description 
            = GetContainsComment(c_item->item_list.size(), "rRNA product name")
              + "suspect phrase";
   }
};

void CBioseq_TEST_ORGANELLE_PRODUCTS :: TestOnObj(const CBioseq& bioseq)
{
   CBioseq_Handle bioseq_hl = thisInfo.scope->GetBioseqHandle(bioseq);
   const CBioSource* 
         biosrc = sequence::GetBioSource(bioseq_hl); //try bioseq_biosrc
   if (!biosrc) {
       return;
   }

   int genome = biosrc->GetGenome();
   if (genome == (int)CBioSource::eGenome_mitochondrion
          || genome == (int)CBioSource::eGenome_chloroplast
          || genome == (int)CBioSource::eGenome_plastid) {
      return;
   }
   
   // source should not be bacterial or viral //
   if (biosrc->IsSetOrgname()) {
      if (HasLineage(*biosrc, "Bacteria") || HasLineage(*biosrc, "Viruses")) {
          return;
      }
   }

   // shouldn't be uncultured non-organelle //
   if (biosrc->IsSetTaxname() && !biosrc->GetTaxname().empty() 
             && HasUnculturedNonOrganelleName(biosrc->GetTaxname())) {
       return;
   }

   // look for misc_features //
   string comment;
   ITERATE (vector <const CSeq_feat*>, it, miscfeat_feat) { 
     if ( (*it)->CanGetComment()) {
       comment = (*it)->GetComment();
       if (NStr::EqualNocase(comment, 0, 9, "contains ") ) {
          ITERATE (list <CRef <CSuspect_rule> >, rit, 
                                            thisInfo.orga_prod_rules->Get()) {
             if (!comment.empty() 
                    && rule_check.DoesStringMatchSuspectRule(
                           bioseq_hl, comment, **it, **rit)){
                thisInfo.test_item_list[GetName()]
                           .push_back(GetDiscItemText(**it));
                thisInfo.test_item_objs[GetName()]
                           .push_back(CConstRef <CObject>(*it));
                break;
             }
          }
       }
     }      
   }

   // also look for coding regions
   //SAnnotSelector annot_sel;
   ITERATE (vector <const CSeq_feat*>, it, cd_feat) {
     CFeat_CI feat_ci(*thisInfo.scope, 
                      (*it)->GetLocation(), 
                      CSeqFeatData :: eSubtype_prot);   // why using CFeat_CI?
     if (feat_ci) {
       const CProt_ref& 
           prot_ref = feat_ci->GetOriginalFeature().GetData().GetProt(); 
       if (prot_ref.CanGetName()) {
          ITERATE (list <string>, nit, prot_ref.GetName()) {
             ITERATE (list <CRef <CSuspect_rule> >, rit, 
                                           thisInfo.orga_prod_rules->Get()){
                if (!(*nit).empty() 
                      && rule_check.DoesStringMatchSuspectRule(
                              bioseq_hl, *nit, **it, **rit)) {
                  thisInfo.test_item_list[GetName()]
                             .push_back(GetDiscItemText(**it));
                  thisInfo.test_item_objs[GetName()]
                             .push_back(CConstRef <CObject>(*it));
                  break;
                } 
             }
          }
       }
     }
   }
};

void CBioseq_TEST_ORGANELLE_PRODUCTS :: GetReport(CRef <CClickableItem>& c_item)
{
  c_item->obj_list = thisInfo.test_item_objs[GetName()];
  c_item->description 
        = GetIsComment(c_item->item_list.size(), "suspect products") 
           + "not organelled.";
};

void CBioseq_DISC_GAPS :: TestOnObj(const CBioseq& bioseq)
{
   string desc(GetDiscItemText(bioseq));
   CConstRef <CObject> bioseq_ref(&bioseq);
   if (bioseq.GetInst().GetRepr() == CSeq_inst::eRepr_delta) {
      const CSeq_inst& inst = bioseq.GetInst();
      if (inst.CanGetExt() && inst.GetExt().IsDelta()) {
        ITERATE (list <CRef <CDelta_seq> >, it, inst.GetExt().GetDelta().Get()){
          if ( (*it)->IsLiteral() 
                 && (!(*it)->GetLiteral().CanGetSeq_data() 
                            || (*it)->GetLiteral().GetSeq_data().IsGap())) {
             thisInfo.test_item_list[GetName()].push_back(desc);
             thisInfo.test_item_objs[GetName()].push_back(bioseq_ref);
             return;
          } 
        }
      }
   }

   if (!gap_feat.empty()) {
      thisInfo.test_item_list[GetName()].push_back(desc);
      thisInfo.test_item_objs[GetName()].push_back(bioseq_ref);
   }
};

void CBioseq_DISC_GAPS :: GetReport (CRef <CClickableItem>& c_item)
{
   c_item->obj_list = thisInfo.test_item_objs[GetName()];
   c_item->description 
         = GetContainsComment(c_item->item_list.size(), "sequence") + "gaps";
};

void CBioseq_DISC_INCONSISTENT_MOLINFO_TECH :: TestOnObj(const CBioseq& bioseq)
{
  if (bioseq.IsAa()) {
       return;
  }
  const CMolInfo* 
      molinfo = sequence::GetMolInfo(thisInfo.scope->GetBioseqHandle(bioseq));
  if (molinfo) {
    strtmp =  CMolInfo::ENUM_METHOD_NAME(ETech)()->FindName(
                     (CMolInfo::ETech)molinfo->GetTech(), false);
    thisInfo.test_item_list[GetName()].push_back(
                                      strtmp + "$" + GetDiscItemText(bioseq));
    thisInfo.test_item_objs[GetName() + "$"+strtmp].push_back(
                                               CConstRef <CObject> (&bioseq));
  }
};

void CBioseq_DISC_INCONSISTENT_MOLINFO_TECH :: GetReport(CRef <CClickableItem>& c_item)
{
  Str2Strs tech2seqs;
  GetTestItemList(c_item->item_list, tech2seqs);
  c_item->item_list.clear();
  CRef <CClickableItem> c_tech (new CClickableItem);
  c_tech->setting_name = GetName();
  c_tech->description = "technique";
  bool has_missing = (tech2seqs.find("unknown") != tech2seqs.end()); 
  unsigned cnt = tech2seqs.size();
  if (cnt == 1) {
     if (has_missing) {
         AddSubcategory(c_tech, 
                        GetName()+ "$unknown", 
                        &(tech2seqs["unknown"]), 
                        "Molinfo", 
                        "missing field technique", 
                         e_IsComment, 
                         false);
         c_item->subcategories.push_back(c_tech);
         strtmp = " (all missing)";
     }
     else {
        Str2Strs::iterator it = tech2seqs.begin();
        AddSubcategory(c_tech, 
                       GetName() + "$" + it->first, 
                       &(it->second), 
                       "Molinfo has", 
                       "All Molinfos have", 
                       e_NoCntComment, 
                       false, 
                       " field technique value '" + it->first + "'");
        strtmp = " (all present, all same)";
        c_item->subcategories.push_back(c_tech);
     }
  }
  else if (cnt == 2 && has_missing) {
      strtmp = " (some missing, all same)";
      ITERATE (Str2Strs, it, tech2seqs) {
         if (it->first != "unknown") {
             AddSubcategory(c_tech, 
                            GetName() + "$" + it->first, 
                            &(it->second),
                            "Molinfo", 
                            "field technique value '" + it->first + "'", 
                            e_HasComment, 
                            false);
         }
         else {
            AddSubcategory(c_tech,
                           GetName() + "$" + it->first,
                           &(it->second),
                           "Molinfo is ",
                           "Molinfos are ",
                           e_OtherComment,
                           false,
                           " missing field technique.");

         }
      }
      c_item->subcategories.push_back(c_tech);
  }
  else {
    strtmp = " (all present, inconsistent)";
    ITERATE (Str2Strs, it, tech2seqs) {
       AddSubcategory(c_tech, 
                      GetName() + "$" + it->first, 
                      &(it->second), 
                      "Molinfo", 
                       "field technique value '" + it->first + "'", 
                      e_HasComment, 
                      false); 
    }
    c_item->subcategories.push_back(c_tech);
  }

  c_tech->description = "technique" + strtmp;
  c_item->description = "Molinfo Technique Report" + strtmp;
  c_item->expanded = true;
};


void CBioseq_TEST_MRNA_OVERLAPPING_PSEUDO_GENE :: TestOnObj(const CBioseq& bioseq)
{
  ITERATE (vector <const CSeq_feat*>, it, mrna_feat) {
     CConstRef <CSeq_feat> gene = GetGeneForFeature(**it);
     if (gene.NotEmpty() && gene->CanGetPseudo() && gene->GetPseudo()) {
        thisInfo.test_item_list[GetName()].push_back(GetDiscItemText(**it));  
        thisInfo.test_item_objs[GetName()].push_back(CConstRef <CObject>(*it));  
     }
  }
};

void CBioseq_TEST_MRNA_OVERLAPPING_PSEUDO_GENE :: GetReport(CRef <CClickableItem>& c_item)
{
   c_item->obj_list = thisInfo.test_item_objs[GetName()];
   c_item->description = GetHasComment(c_item->item_list.size(), "Pseudogene") 
                           + "overlapping mRNAs.";
};


void CBioseq_TEST_UNWANTED_SPACER :: TestOnObj(const CBioseq& bioseq)
{
   const CBioSource*
     biosrc = sequence::GetBioSource(thisInfo.scope->GetBioseqHandle(bioseq));
   if (biosrc) {
     switch ( biosrc->GetGenome() ) {
        case CBioSource::eGenome_chloroplast:
        case CBioSource::eGenome_plastid:
            return;
        default: break; 
     }
     /* shouldn't be uncultured non-organelle */
     if (biosrc->IsSetTaxname() 
             && !biosrc->GetTaxname().empty() 
             && HasUnculturedNonOrganelleName(biosrc->GetTaxname())) {
         return;
     }
   }

   /* look for misc_features */
   ITERATE (vector <const CSeq_feat*>, it, miscfeat_feat) {
     if ( (*it)->CanGetComment() 
            && !(*it)->GetComment().empty()
            && HasIntergenicSpacerName( (*it)->GetComment())) {
        thisInfo.test_item_list[GetName()].push_back(GetDiscItemText(**it));
        thisInfo.test_item_objs[GetName()].push_back(CConstRef <CObject>(*it));
     }
   }
};


bool CBioseqTestAndRepData :: HasUnculturedNonOrganelleName (const string& tax_nm)
{
  if (tax_nm == "uncultured organism" || tax_nm == "uncultured microorganism"
        || tax_nm == "uncultured bacterium" || tax_nm == "uncultured archaeon"){
      return true;
  }
  else {
      return false;
  }
};


bool CBioseq_TEST_UNWANTED_SPACER :: HasIntergenicSpacerName (const string& comm)
{
  ITERATE (vector <string>, it, thisInfo.kIntergenicSpacerNames) {
     if (NStr::FindNoCase(comm, *it) != string::npos) {
         return true;
     }
  }
  return false;
};


void CBioseq_TEST_UNWANTED_SPACER :: GetReport(CRef <CClickableItem>& c_item)
{
   c_item->obj_list = thisInfo.test_item_objs[GetName()];
   c_item->description 
       = GetIsComment(c_item->item_list.size(),"suspect intergenic spacer note")
           + "not organelle";
};


// new comb 
void CBioseq_on_Aa :: TestOnObj(const CBioseq& bioseq) 
{
   if (thisTest.is_Aa_run) return;
   thisTest.is_Aa_run = true;

   string bioseq_desc(GetDiscItemText(bioseq));
   CConstRef <CObject> seq_ref (&bioseq);
   unsigned len = bioseq.GetLength();
   CConstRef <CBioseq_set> bioseq_set = bioseq.GetParentSet();
   const CMolInfo* 
      molinfo = sequence::GetMolInfo(thisInfo.scope->GetBioseqHandle(bioseq));
   const CBioSource*
      biosrc = sequence::GetBioSource(thisInfo.scope->GetBioseqHandle(bioseq));
   CConstRef <CObject> biosrc_ref (biosrc);
   if (bioseq.IsAa() && thisTest.tests_run.find(GetName_shtprt()) != end_it){
      // SHORT_PROT_SEQUENCES
      if (len < 50 
           && ( bioseq_set.Empty() 
                   || bioseq_set->GetClass() != CBioseq_set::eClass_parts)) { 
         if (molinfo
              && molinfo->GetCompleteness() != CMolInfo::eCompleteness_unknown
              && molinfo->GetCompleteness() !=CMolInfo::eCompleteness_complete){
         }
         else {
            thisInfo.test_item_list[GetName_shtprt()].push_back(bioseq_desc);
            thisInfo.test_item_objs[GetName_shtprt()].push_back(seq_ref);
         }
      }       
   }
 
    bool is_dna = (bioseq.GetInst().GetMol() == CSeq_inst::eMol_dna);
    bool is_rna = (bioseq.GetInst().GetMol() == CSeq_inst::eMol_rna);

    if (is_dna) {

      // DISC_RETROVIRIDAE_DNA, NON_RETROVIRIDAE_PROVIRAL
      if (thisTest.tests_run.find(GetName_retro()) != end_it
                    || thisTest.tests_run.find(GetName_nonr()) != end_it) {
         bool is_retro;
         string src_desc;
         ITERATE (vector <const CSeqdesc*>, it, bioseq_biosrc_seqdesc) {
            const CBioSource& biosrc = (*it)->GetSource();
            const int genome = biosrc.GetGenome();
            src_desc = GetDiscItemText(**it, bioseq);
            CConstRef <CObject> src_ref(*it); 
            is_retro = HasLineage(biosrc, "Retroviridae");
            if ( genome != (int)CBioSource :: eGenome_proviral 
                      && is_retro
                      && thisTest.tests_run.find(GetName_retro()) != end_it ) {
                thisInfo.test_item_list[GetName_retro()].push_back(src_desc);
                thisInfo.test_item_objs[GetName_retro()].push_back(src_ref);
            }
            else if (genome == CBioSource :: eGenome_proviral && !is_retro
                         && thisTest.tests_run.find(GetName_nonr()) != end_it) {
                thisInfo.test_item_list[GetName_nonr()].push_back(src_desc);
                thisInfo.test_item_objs[GetName_nonr()].push_back(src_ref);
            }
         }
      }
      
      // EUKARYOTE_SHOULD_HAVE_MRNA
      bool has_CDs = false, has_mrna = false;
      if (thisTest.tests_run.find(GetName_eu_mrna()) != end_it
               && IsBioseqHasLineage(bioseq, "Eukaryota")) {
        bool stop = false;
        if (thisInfo.test_item_list.find(GetName_eu_mrna()) 
              != thisInfo.test_item_list.end()) {
           ITERATE (vector <string>, 
                  vit, thisInfo.test_item_list[GetName_eu_mrna()]) {
             if ("yes" == *vit) {
               stop = true;
               break; 
             }
           }
        }
        if (!stop) {
          if (biosrc) {
              if (!IsLocationOrganelle(biosrc->GetGenome())) {
                 if (molinfo) {
                    if ( molinfo->GetBiomol() == CMolInfo::eBiomol_genomic) {
                       ITERATE (vector <const CSeq_feat*>, kt, cd_feat) {
                         if (IsPseudoSeqFeatOrXrefGene(*kt)) {
                              continue;
                         }
                         has_CDs = true;
                         CConstRef <CSeq_feat> 
                            mRNA = sequence::GetBestMrnaForCds(**kt, 
                                           *thisInfo.scope, 
                                           sequence::fBestFeat_NoExpensive);
                         if (mRNA.NotEmpty()) { 
                              thisInfo.test_item_list[GetName_eu_mrna()]
                                   .push_back("yes");
                              has_mrna = true;
                              break;
                         }
                       }
                    }
                 }
              }
              if (has_CDs && has_mrna) {
                 thisInfo.test_item_list[GetName_eu_mrna()].push_back("yes");
              }
              else {
                 thisInfo.test_item_list[GetName_eu_mrna()].push_back("no");
              }
          }
        }
      }
   }
   else {
      if (is_rna) {  // RNA_PROVIRAL
        if (thisTest.tests_run.find(GetName_rnapro()) != end_it) {
           if (biosrc) {
             if ( biosrc->GetGenome() == CBioSource :: eGenome_proviral ) {
               thisInfo.test_item_list[GetName_rnapro()].push_back(bioseq_desc);
               thisInfo.test_item_objs[GetName_rnapro()].push_back(seq_ref);
             }
           }
        }
      }
   }
       
   // TEST_ORGANELLE_NOT_GENOMIC
   bool run_test = false;
   if (molinfo) {
     int biomol = molinfo->GetBiomol();
     if ( ( biomol == CMolInfo::eBiomol_genomic 
                         || biomol == CMolInfo::eBiomol_unknown)
            && is_dna) {
          run_test = true;
     }
   }
   if (run_test && thisTest.tests_run.find(GetName_orgl()) != end_it) {
     ITERATE (vector <const CSeqdesc*>, it, bioseq_biosrc_seqdesc) {
       const CBioSource& biosrc = (*it)->GetSource();
       if (IsLocationOrganelle(biosrc.GetGenome() )) {
           thisInfo.test_item_list[GetName_orgl()]
                      .push_back(GetDiscItemText(**it, bioseq));
           thisInfo.test_item_objs[GetName_orgl()]
                      .push_back(CConstRef <CObject> (*it));
       }
     }
   }

   // TEST_UNNECESSARY_VIRUS_GENE
   if (thisTest.tests_run.find(GetName_vir()) != end_it) {
      bool has_virus = false;
      if (biosrc) {
         ITERATE (vector <string>, jt, thisInfo.virus_lineage) {
            if (HasLineage(*biosrc, *jt)) {
                ITERATE (vector <const CSeq_feat*>, kt, gene_feat) {
                    thisInfo.test_item_list[GetName_vir()]
                               .push_back(GetDiscItemText(**kt));
                    thisInfo.test_item_objs[GetName_vir()]
                               .push_back(CConstRef <CObject>(*kt));
                }
                has_virus = true;
                break;
            }
         }
      }
   }

   // DISC_RBS_WITHOUT_GENE
   if (!gene_feat.empty() 
               && thisTest.tests_run.find(GetName_rbs()) != end_it) {
       ITERATE (vector <const CSeq_feat*>, it, rbs_feat) {
         CConstRef <CSeq_feat>
           rbs_gene 
               = sequence::GetBestOverlappingFeat((*it)->GetLocation(),
                                                   CSeqFeatData::e_Gene, 
                                                   sequence::eOverlap_Contained,
                                                   *thisInfo.scope);
         if (rbs_gene.NotEmpty()) {
             thisInfo.test_item_list[GetName_rbs()]
                         .push_back( GetDiscItemText(**it));
             thisInfo.test_item_objs[GetName_rbs()]
                         .push_back(CConstRef <CObject> (*it));
         }
       }
   }

   // DISC_EXON_INTRON_CONFLICT
   if (thisTest.tests_run.find(GetName_ei()) != end_it) {
     string seq_id_desc(BioseqToBestSeqIdString(bioseq,CSeq_id::e_Genbank));
     unsigned i=0;
     if (gene_feat.empty()) {
         /* no genes - just do all exons and introns present */
         if (!exon_feat.empty() && !intron_feat.empty()) {
            for (i=0; i< exon_feat.size(); i++) {
                 m_e_exist.push_back(1);
            }
            for (i=0; i< intron_feat.size(); i++) {
                 m_i_exist.push_back(1);
            }
            CompareIntronExonList(seq_id_desc, exon_feat, intron_feat);
         }
     }
     else {
        ITERATE (vector <const CSeq_feat*>, it, gene_feat) {
           if ((*it)->CanGetExcept_text() 
                 && (*it)->GetExcept_text() != "trans-splicing"){
              GetFeatureList4Gene( *it, exon_feat, m_e_exist);
              GetFeatureList4Gene( *it, intron_feat, m_i_exist);
              if (!exon_feat.empty() && !intron_feat.empty()) {
                   CompareIntronExonList(seq_id_desc, exon_feat, intron_feat);
              }
           }
        }
     }
   }
   
   // DISC_GENE_PARTIAL_CONFLICT
   // ReportPartialConflictsForFeatureType (bsp, 0, FEATDEF_intron, "intron")
   if (thisTest.tests_run.find(GetName_pgene()) != end_it) {
      ReportPartialConflictsForFeatureType (intron_feat, "intron");
      ReportPartialConflictsForFeatureType (exon_feat, "exon");
      ReportPartialConflictsForFeatureType (promoter_feat, "promoter");
      ReportPartialConflictsForFeatureType (rna_feat, "RNA");
      ReportPartialConflictsForFeatureType (utr3_feat, "3' URT");
      ReportPartialConflictsForFeatureType (utr5_feat, "5' URT");
      if (!IsBioseqHasLineage(bioseq, "Eukaryota") || IsMrnaSequence (bioseq)) {
          ReportPartialConflictsForFeatureType (cd_feat, "coding region", true);
      }
      ReportPartialConflictsForFeatureType (miscfeat_feat, "misc_feature");
   }

   if (bioseq.IsNa() && !IsmRNASequenceInGenProdSet(bioseq)) {
      // SHORT_CONTIG
      if (len < 200 && thisTest.tests_run.find(GetName_contig()) != end_it ) {
          thisInfo.test_item_list[GetName_contig()].push_back(bioseq_desc);
          thisInfo.test_item_objs[GetName_contig()].push_back(seq_ref);
      }

      // SHORT_SEQUENCES
      if (bioseq_set.Empty() || bioseq_set->GetClass() != CBioseq_set::eClass_parts) {
          if (len < 200 && thisTest.tests_run.find(GetName_200seq()) != end_it){
              thisInfo.test_item_list[GetName_200seq()].push_back(bioseq_desc);
              thisInfo.test_item_objs[GetName_200seq()].push_back(seq_ref);
          }
          else if (len <50 && thisTest.tests_run.find(GetName_50seq())!=end_it){
              thisInfo.test_item_list[GetName_50seq()].push_back(bioseq_desc);
              thisInfo.test_item_objs[GetName_50seq()].push_back(seq_ref);
          }
      }
   }

   // TEST_DUP_GENES_OPPOSITE_STRANDS
   if (thisTest.tests_run.find(GetName_dupg()) != end_it) {
      vector <const CSeq_feat*>::const_iterator jt;
      bool prev_dup = false;
      ITERATE (vector <const CSeq_feat*>, it, gene_feat) {
        jt = it + 1;
        if (jt != gene_feat.end()) {
          sequence::ECompare
              ovp = sequence::Compare( (*it)->GetLocation(), 
                                       (*jt)->GetLocation(), thisInfo.scope);
          if (ovp == sequence::eSame
                   && (*it)->GetLocation().GetStrand() 
                              != (*jt)->GetLocation().GetStrand()) {

               thisInfo.test_item_list[GetName_dupg()].push_back(
                                                   GetDiscItemText(**jt));
               thisInfo.test_item_objs[GetName_dupg()].push_back(
                                                   CConstRef <CObject>(*jt));
               if (!prev_dup) {
                  thisInfo.test_item_list[GetName_dupg()].push_back(
                                                        GetDiscItemText(**it));
                  thisInfo.test_item_objs[GetName_dupg()].push_back(
                                                     CConstRef <CObject>(*it));
               }
               prev_dup = true;
          }
          else {
              prev_dup = false;
          }
        }
      }
   }

   // TEST_COUNT_UNVERIFIED
   if (thisTest.tests_run.find(GetName_unv()) != end_it
           && BioseqHasKeyword (bioseq, "UNVERIFIED")) {
      thisInfo.test_item_list[GetName_unv()].push_back(bioseq_desc);
      thisInfo.test_item_objs[GetName_unv()].push_back(seq_ref);
   }
};

void CBioseq_EUKARYOTE_SHOULD_HAVE_MRNA :: GetReport(CRef <CClickableItem>& c_item)
{
  strtmp = NStr::Join(c_item->item_list, ",");
  c_item->item_list.clear();
  if (strtmp.find("yes") == string::npos) {
       c_item->description = "No mRNA present.";
  }
};

void CBioseq_RNA_PROVIRAL :: GetReport(CRef <CClickableItem>& c_item)
{
   c_item->obj_list = thisInfo.test_item_objs[GetName()];
   c_item->description
       = GetIsComment(c_item->item_list.size(), "RNA bioseq") + "proviral";
};

void CBioseq_DISC_RETROVIRIDAE_DNA :: GetReport(CRef <CClickableItem>& c_item)
{   
   c_item->obj_list = thisInfo.test_item_objs[GetName()];
   c_item->description
     = GetOtherComment(c_item->item_list.size(), 
                       "Retroviridae biosource on DNA sequences is",
                       "Retroviridae biosourceson DNA sequences are") 
         + " not proviral";
};

void CBioseq_NON_RETROVIRIDAE_PROVIRAL :: GetReport(CRef <CClickableItem>& c_item)
{
  c_item->obj_list = thisInfo.test_item_objs[GetName()];
  c_item->description = GetIsComment(c_item->item_list.size(), 
                                     "non-Retroviridae biosource") 
                         + "proviral.";
};

void CBioseq_TEST_COUNT_UNVERIFIED :: GetReport(CRef <CClickableItem>& c_item) 
{
  c_item->obj_list = thisInfo.test_item_objs[GetName()];
  c_item->description = GetIsComment(c_item->item_list.size(), "sequence") 
                         + "unverified.";
};

bool CBioseqTestAndRepData :: BioseqHasKeyword(const CBioseq& bioseq, const string& keywd)
{
  if (NStr::EqualNocase(keywd, "UNVERIFIED"))
  {
    /* special case for unverified */
    ITERATE (vector <const CSeqdesc*>, it, bioseq_user) {
       const CUser_object& user_obj = (*it)->GetUser();
       if (user_obj.GetType().IsStr() 
             && NStr::EqualNocase(user_obj.GetType().GetStr(), "Unverified")) {
          return true;
       }
    }

  }

  ITERATE (vector <const CSeqdesc*>, it, bioseq_genbank) {
     const CGB_block& gb = (*it)->GetGenbank();
     if (gb.CanGetKeywords()) {
        ITERATE (list <string>, jt, gb.GetKeywords()) {
           if (NStr::EqualNocase(*jt, keywd)) {
                return true;
           }
        }
     }
  }

  return false;
};


void CBioseq_TEST_DUP_GENES_OPPOSITE_STRANDS :: GetReport(CRef <CClickableItem>& c_item)
{
   c_item->obj_list = thisInfo.test_item_objs[GetName()];
   c_item->description
      = GetOtherComment(c_item->item_list.size(), "gene matches", "genes match")
         + " other genes in the same location, but on the opposite strand";
};

void CBioseq_SHORT_CONTIG :: GetReport(CRef <CClickableItem>& c_item)
{
   c_item->obj_list = thisInfo.test_item_objs[GetName()];
   c_item->description = GetIsComment(c_item->item_list.size(), "contig") 
                          + "shorter than 200 nt.";
};


void CBioseq_SHORT_SEQUENCES :: GetReport(CRef <CClickableItem>& c_item)
{
   c_item->obj_list = thisInfo.test_item_objs[GetName()];
   c_item->description = GetIsComment(c_item->item_list.size(), "sequence") 
                         + "shorter than 50 nt.";
};


void CBioseq_SHORT_SEQUENCES_200 :: GetReport(CRef <CClickableItem>& c_item) 
{ 
   c_item->obj_list = thisInfo.test_item_objs[GetName()];
   c_item->description = GetIsComment(c_item->item_list.size(), "sequence") 
                          + "shorter than 200 nt."; 
}; 


void CBioseq_SHORT_PROT_SEQUENCES :: GetReport(CRef <CClickableItem>& c_item)
{
   c_item->obj_list = thisInfo.test_item_objs[GetName()];
   c_item->description
       = GetIsComment(c_item->item_list.size(), "protein sequence") 
          + "shorter than 50 aa.";
};


void CBioseq_DISC_GENE_PARTIAL_CONFLICT :: GetReport(CRef <CClickableItem>& c_item)
{
  Str2Strs label2lists;
  GetTestItemList(c_item->item_list, label2lists);
  c_item->item_list.clear();
  CRef <CClickableItem> citem_other (new CClickableItem);
  citem_other->setting_name = GetName();
  ITERATE (Str2Strs, it, label2lists) {
    strtmp = kEmptyStr;
    if (it->first.find("coding region") != string::npos) {
            strtmp = "coding region";
    }
    else if (it->first.find("misc_feature") != string::npos) {
            strtmp = "misc_feature";
    }
    if (!strtmp.empty()) {
        AddSubcategory(c_item, GetName() + "$" + it->first, &(it->second), 
                   strtmp + " location conflicts ",
                   strtmp + " locations conflict ", e_OtherComment, true,
                   "with partialness of overlapping gene.", true);
    }
    else {
        AddSubcategory(citem_other, GetName() + "$" + it->first, 
                               &(it->second), it->first,
                               it->first, e_OtherComment, true, "", true);
    }
  }
  if (!citem_other->item_list.empty()) {
        citem_other->description
          = GetOtherComment(citem_other->item_list.size(), 
               "feature that is not coding regions or misc_feature conflicts",
               "features that are not coding regions or misc_features conflict")
              + " with partialness of overlapping gene.";
        copy(citem_other->item_list.begin(), citem_other->item_list.end(),
              back_inserter(c_item->item_list));
        copy(citem_other->obj_list.begin(), citem_other->obj_list.end(),
              back_inserter(c_item->obj_list));
/*
        AddSubcategory(c_item, GetName(), &(citem_other->item_list),
              "feature that is not coding regions or misc_feature conflicts",
              "features that are not coding regions or misc_features conflict",               e_OtherComment,
              true, " with partialness of overlapping gene.", true);
*/
  }
  c_item->description
       = GetOtherComment(c_item->item_list.size()/2,
                     "feature location conflicts", "feature locations conflict")
          + " with partialness of overlapping gene.";
};


void CBioseq_DISC_EXON_INTRON_CONFLICT :: GetReport(CRef <CClickableItem>& c_item)
{
   Str2Strs seq2cflts;
   GetTestItemList(c_item->item_list, seq2cflts);
   c_item->item_list.clear();
   ITERATE (Str2Strs, it, seq2cflts) {
     AddSubcategory(c_item, GetName() + "$" + it->first, &(it->second), 
        "introns and exon", "location conflicts on " + it->first, e_HasComment);
   }
   c_item->description 
         = GetIsComment(c_item->item_list.size(), "introns and exon")
           + "incorrectly positioned";
};


void CBioseq_DISC_RBS_WITHOUT_GENE :: GetReport(CRef <CClickableItem>& c_item)
{
   c_item->obj_list = thisInfo.test_item_objs[GetName()];
   c_item->description
      = GetDoesComment(c_item->item_list.size(), "RBS feature") 
         + "not have overlapping genes";
};


void CBioseq_TEST_UNNECESSARY_VIRUS_GENE :: GetReport(CRef <CClickableItem>& c_item)
{
   c_item->description = GetOtherComment(c_item->item_list.size(), 
                                        "virus gene needs", "virus genes need") 
                          + " to be removed";
};


void CBioseq_TEST_ORGANELLE_NOT_GENOMIC :: GetReport(CRef <CClickableItem>& c_item)
{
   c_item->obj_list = thisInfo.test_item_objs[GetName()];
   unsigned cnt = c_item->item_list.size();
   c_item->description 
        = GetIsComment(cnt, "non-genomic sequence") + GetNoun(cnt, "organelle");
};


// new comb
void CBioseq_on_mRNA :: TestOnObj(const CBioseq& bioseq)
{
  if (thisTest.is_mRNA_run) {
      return;
  }
  thisTest.is_mRNA_run = true;

  if (!IsMrnaSequence(bioseq)) {
      return;
  }

  string desc(GetDiscItemText(bioseq));
  CConstRef <CObject> seq_ref(&bioseq);
  // TEST_EXON_ON_MRNA
  if ( !exon_feat.empty() 
               && thisTest.tests_run.find(GetName_exon()) != end_it ) {
      thisInfo.test_item_list[GetName_exon()].push_back(desc);
      thisInfo.test_item_objs[GetName_exon()].push_back(seq_ref);
  }

  // TEST_BAD_MRNA_QUAL
  if (thisTest.tests_run.find(GetName_qual()) != end_it) {
     ITERATE (vector <const CSeqdesc*>, it, bioseq_biosrc_seqdesc) {
       const CBioSource& biosrc = (*it)->GetSource();
       if ( biosrc.CanGetSubtype()) {
         if (IsSubSrcPresent(biosrc, CSubSource::eSubtype_germline) 
                 || IsSubSrcPresent(biosrc, CSubSource::eSubtype_rearranged)) {
            thisInfo.test_item_list[GetName_qual()]
                       .push_back(GetDiscItemText(**it, bioseq));
            thisInfo.test_item_objs[GetName_qual()]
                       .push_back(CConstRef <CObject>(*it));
         }
       }
     }
  }

  // TEST_MRNA_SEQUENCE_MINUS_STRAND_FEATURES
  if (thisTest.tests_run.find(GetName_str()) != end_it) {
     ITERATE (vector <const CSeq_feat*>, it, all_feat) {
        if ( (*it)->GetData().GetSubtype() != CSeqFeatData::eSubtype_primer_bind
                   && (*it)->GetLocation().GetStrand() == eNa_strand_minus) {
           thisInfo.test_item_list[GetName_str()].push_back(desc);
           thisInfo.test_item_objs[GetName_str()].push_back(seq_ref);
        }
     }
  }

  // MULTIPLE_CDS_ON_MRNA
  if (thisTest.tests_run.find(GetName_mcds()) != end_it) {
    strtmp = "coding region disrupted by sequencing gap";
    unsigned cnt=0;
    ITERATE (vector <const CSeq_feat*>, it, cd_feat) {
      if ( (!(*it)->CanGetPseudo() || !(*it)->GetPseudo())
                && ( !(*it)->CanGetComment() 
                        || (*it)->GetComment().find(strtmp) == string::npos) ){
         if (cnt) {
              thisInfo.test_item_list[GetName_mcds()].push_back(desc);
              thisInfo.test_item_objs[GetName_mcds()].push_back(seq_ref);
         }
         else {
            cnt++;
         }
      }
    }
  }
};

void CBioseq_MULTIPLE_CDS_ON_MRNA :: GetReport(CRef <CClickableItem>& c_item)
{
  c_item->obj_list = thisInfo.test_item_objs[GetName()];
  c_item->description = GetHasComment(c_item->item_list.size(), "mRNA bioseq")
                           + "multiple CDS features";
};

void CBioseq_TEST_MRNA_SEQUENCE_MINUS_STRAND_FEATURES :: GetReport(CRef <CClickableItem>& c_item)
{
  c_item->obj_list = thisInfo.test_item_objs[GetName()];
  c_item->description = GetHasComment(c_item->item_list.size(), "mRNA sequence")
                        + "features on the complement strand.";
};


void CBioseq_TEST_BAD_MRNA_QUAL :: GetReport(CRef <CClickableItem>& c_item)
{
  c_item->obj_list = thisInfo.test_item_objs[GetName()];
  c_item->description = GetHasComment(c_item->item_list.size(), "mRNA sequence")
                         + "germline or rearranged qualifier";
};


void CBioseq_TEST_EXON_ON_MRNA :: GetReport(CRef <CClickableItem>& c_item)
{
   c_item->obj_list = thisInfo.test_item_objs[GetName()];
   c_item->description = GetHasComment(c_item->item_list.size(), "mRNA bioseq")
                         + "exon features";
};


void CBioseq_ONCALLER_HIV_RNA_INCONSISTENT :: TestOnObj(const CBioseq& bioseq)
{
   if (bioseq.GetInst().GetMol() != CSeq_inst::eMol_rna) {
        return;
   }
   const CMolInfo* 
       molinfo = sequence::GetMolInfo(thisInfo.scope->GetBioseqHandle(bioseq));
   ITERATE (vector <const CSeqdesc*>, it, bioseq_biosrc_seqdesc) {
     const CBioSource& biosrc = (*it)->GetSource();
     if ( biosrc.GetGenome() == CBioSource::eGenome_unknown) {
           return;
     }
     if ( biosrc.IsSetTaxname() || (strtmp = biosrc.GetTaxname()).empty()) {
          return;
     }
     if (!NStr::EqualNocase(strtmp, "Human immunodeficiency virus")
             && (!NStr::EqualNocase(strtmp, "Human immunodeficiency virus"))
             && (!NStr::EqualNocase(strtmp, "Human immunodeficiency virus"))) {
         return;
     }
     if ( biosrc.GetGenome() == CBioSource::eGenome_genomic ){
        if (molinfo) {
           if ( molinfo->GetBiomol() != CMolInfo::eBiomol_genomic) {
                return;
           }
        }
     }
   }

   thisInfo.test_item_list[GetName()].push_back(GetDiscItemText(bioseq));
   thisInfo.test_item_objs[GetName()].push_back(CConstRef <CObject>(&bioseq));
};

void CBioseq_ONCALLER_HIV_RNA_INCONSISTENT :: GetReport(CRef <CClickableItem>& c_item)
{
   c_item->obj_list = thisInfo.test_item_objs[GetName()];
   c_item->description 
        = GetHasComment(c_item->item_list.size(), "HIV RNA bioseq") 
          + "inconsistent location/moltype";
};


// new comb
void CBioseq_on_cd_feat :: TestOnObj(const CBioseq& bisoeq)
{
  if (thisTest.is_CDs_run) {
      return;
  }
  thisTest.is_CDs_run = true;

  string desc;
  bool run_exc = (thisTest.tests_run.find(GetName_exc()) != end_it);
  bool run_cdd = (thisTest.tests_run.find(GetName_cdd()) != end_it);
  ITERATE (vector <const CSeq_feat*>, it, cd_feat) {
    desc = GetDiscItemText(**it);
    CConstRef <CObject> feat_ref(*it);

    // DISC_CDS_HAS_NEW_EXCEPTION
    if (run_exc && (*it)->CanGetExcept_text()) {
       ITERATE (vector <string>, jt, thisInfo.new_exceptions) {
          if (NStr::FindNoCase( (*it)->GetExcept_text(), *jt ) != string::npos){
              thisInfo.test_item_list[GetName_exc()].push_back(desc);
              thisInfo.test_item_objs[GetName_exc()].push_back(feat_ref);
              break;
          }
       }
    }

    // TEST_CDS_HAS_CDD_XREF
    if (run_cdd && (*it)->CanGetDbxref()) {
       ITERATE (vector < CRef< CDbtag > >, jt, (*it)->GetDbxref()) {
         if ( NStr::EqualNocase( (*jt)->GetDb(), "CDD")) {
            break;
         }
       }
    }
  }
};


void CBioseq_TEST_CDS_HAS_CDD_XREF :: GetReport(CRef <CClickableItem>& c_item)
{
   c_item->description = GetHasComment(c_item->item_list.size(), "feature") 
                          + "CDD Xrefs";
   c_item->obj_list = thisInfo.test_item_objs[GetName()];
};


void CBioseq_DISC_CDS_HAS_NEW_EXCEPTION :: GetReport(CRef <CClickableItem>& c_item)
{
   c_item->description
       = GetHasComment(c_item->item_list.size(), "coding region") 
         + "new exceptions";
};


void CBioseq_DISC_MITOCHONDRION_REQUIRED :: TestOnObj(const CBioseq& bioseq)
{
   bool needs_mitochondrial = false;
   string comm, desc;
   if (D_loop_feat.empty()) {
      ITERATE (vector <const CSeq_feat*>, it, miscfeat_feat) {
         if ((*it)->CanGetComment() 
                 && !( (comm = (*it)->GetComment()).empty())
                 && NStr::FindNoCase(comm, "control region") != string::npos) {
             needs_mitochondrial = true; 
             break;
         }
      }
   }
   else {
      needs_mitochondrial = true; 
   }
  
   
   if (needs_mitochondrial) {
     desc = GetDiscItemText(bioseq);
     CConstRef <CObject> seq_ref (&bioseq);
     if (bioseq_biosrc_seqdesc.empty()) {
         thisInfo.test_item_list[GetName()].push_back(desc);
         thisInfo.test_item_objs[GetName()].push_back(seq_ref);
     }
     ITERATE (vector <const CSeqdesc*>, it, bioseq_biosrc_seqdesc) {
        if ((*it)->GetSource().GetGenome() !=CBioSource::eGenome_mitochondrion){
             thisInfo.test_item_list[GetName()].push_back(desc); 
             thisInfo.test_item_objs[GetName()].push_back(seq_ref);
             break;
        }
     }
   }
};


void CBioseq_DISC_MITOCHONDRION_REQUIRED :: GetReport(CRef <CClickableItem>& c_item)
{
   unsigned cnt = c_item->item_list.size();
   c_item->obj_list = thisInfo.test_item_objs[GetName()];
   c_item->description
       = GetHasComment(cnt, "bioseq") 
         + "D-loop or control region misc_feature, but "
         + ( (cnt ==1)? "does" : "do" ) + " not have mitochondrial source";
};


void CBioseq_DISC_MICROSATELLITE_REPEAT_TYPE :: TestOnObj(const CBioseq& bioseq)
{
   bool is_microsatellite = false, is_tandem = false;
   string qual_qual, qual_val;
   ITERATE (vector <const CSeq_feat*>, it, repeat_region_feat) {
     if ( (*it)->CanGetQual()) {
        ITERATE (vector <CRef <CGb_qual> >, jt, (*it)->GetQual()) {
          qual_qual = (*jt)->GetQual();
          qual_val = (*jt)->GetVal(); 
          if ( qual_qual == "satellite"
                && (NStr::EqualNocase(qual_val, "microsatellite") //please check all FindNoCase
                     || NStr::EqualNocase(qual_val, 0, 15, "microsatellite:"))){
              is_microsatellite = true;
          }
          else if ( qual_qual == "rpt_type" && qual_val == "tandem") {
              is_tandem = true;
          }
          if (is_microsatellite && is_tandem) {
              break;
          }
        }
     }
     if (is_microsatellite && !is_tandem) {
        thisInfo.test_item_list[GetName()].push_back(GetDiscItemText(**it));
        thisInfo.test_item_objs[GetName()].push_back(CConstRef <CObject>(*it));
     }
   }
};


void CBioseq_DISC_MICROSATELLITE_REPEAT_TYPE :: GetReport(CRef <CClickableItem>& c_item)
{
   c_item->obj_list = thisInfo.test_item_objs[GetName()];
   c_item->description 
       = GetDoesComment(c_item->item_list.size(), "microsatellite") 
         + "not have a repeat type of tandem";
};



//new comb

void CBioseq_test_on_suspect_phrase :: TestOnObj(const CBioseq& bioseq) 
{
   if (thisTest.is_SusPhrase_run) return;
   thisTest.is_SusPhrase_run = true;

   // DISC_CHECK_RNA_PRODUCTS_AND_COMMENTS
   if (thisTest.tests_run.find(GetName_rna_comm()) != end_it) {
       ITERATE (vector <const CSeq_feat*>, it, rrna_feat) {
           CheckForProdAndComment(**it);
       }
       ITERATE (vector <const CSeq_feat*>, it, trna_feat) {
           CheckForProdAndComment(**it);
       }
   }

   // DISC_SUSPECT_MISC_FEATURES
   if (thisTest.tests_run.find(GetName_misc()) != end_it) {
       ITERATE (vector <const CSeq_feat*>, it, miscfeat_feat) {
           FindBadMiscFeatures(**it);
       }
   }
}


void CBioseq_test_on_suspect_phrase :: FindBadMiscFeatures(const CSeq_feat& seq_feat)
{
  string bad_misc_comment_phrases("catalytic intron");
  if (seq_feat.CanGetComment() 
           && DoesStringContainPhrase(seq_feat.GetComment(), 
                                      bad_misc_comment_phrases, false, false)) {
    thisInfo.test_item_list[GetName_misc()].push_back(
                   bad_misc_comment_phrases + "$" + GetDiscItemText(seq_feat));
  }
};


void CBioseq_DISC_SUSPECT_MISC_FEATURES :: GetReport(CRef <CClickableItem>& c_item)
{
   GetRepOfSuspPhrase(c_item, GetName(), 
                      "misc_feature comment", 
                      "misc_feature comments");
};


void CBioseq_test_on_suspect_phrase :: GetRepOfSuspPhrase(CRef <CClickableItem>& c_item, const string& setting_name, const string& phrase_loc_4_1, const string& phrase_loc_4_mul)
{
   Str2Strs tp2feats;
   GetTestItemList(c_item->item_list, tp2feats);
   c_item->item_list.clear();
   string s_desc(phrase_loc_4_1 + " contains ");
   string p_desc(phrase_loc_4_mul + " contain ");
   ITERATE (Str2Strs, it, tp2feats) {
     AddSubcategory(c_item, setting_name + "$" + it->first, &(it->second), 
                   s_desc, p_desc, e_OtherComment, true, "'" + it->first + "'");
   }
   c_item->description
       = GetOtherComment(c_item->item_list.size(), s_desc, p_desc) 
         + "suspect phrase";
};


void CBioseq_test_on_suspect_phrase :: CheckForProdAndComment(const CSeq_feat& seq_feat)
{
   string prod_str = GetRNAProductString(seq_feat);                   
   string desc(GetDiscItemText(seq_feat));
   CConstRef <CObject> sf_ref(&seq_feat);
   string comm;
   ITERATE (vector <string>, it, thisInfo.suspect_rna_product_names) {
      if (DoesStringContainPhrase(prod_str, *it, false)) {
        thisInfo.test_item_list[GetName_rna_comm()].push_back(*it + "$" + desc);
        thisInfo.test_item_objs[GetName_rna_comm() +"$"+ *it].push_back(sf_ref);
      }
      else if (seq_feat.CanGetComment() 
                 && DoesStringContainPhrase(seq_feat.GetComment(), *it, false)){
        thisInfo.test_item_list[GetName_rna_comm()].push_back(*it + "$" + desc);
        thisInfo.test_item_objs[GetName_rna_comm() +"$"+ *it].push_back(sf_ref);
      }
   }
};


void CBioseq_DISC_CHECK_RNA_PRODUCTS_AND_COMMENTS :: GetReport(CRef <CClickableItem>& c_item)
{
   GetRepOfSuspPhrase(c_item, GetName(), 
                      "RNA product_name or comment", 
                      "RNA product_names or comments");
};
// new comb


void CBioseq_DISC_mRNA_ON_WRONG_SEQUENCE_TYPE :: TestOnObj(const CBioseq& bioseq)
{
  if (bioseq.GetInst().GetMol() != CSeq_inst :: eMol_dna) {
          return;
  }
  if (!IsBioseqHasLineage(bioseq, "Eukaryota")) {
        return;
  }
  bool is_genomic = false;
  const CMolInfo* 
     molinfo = sequence::GetMolInfo(thisInfo.scope->GetBioseqHandle(bioseq));
  if (molinfo) {
     if ( molinfo->GetBiomol() == CMolInfo::eBiomol_genomic) {
         is_genomic = true; 
     }
  }
  if (!is_genomic) {
       return;
  }
  ITERATE (vector <const CSeqdesc*>, it, bioseq_biosrc_seqdesc) {
     CBioSource :: EGenome 
          genome = (CBioSource::EGenome)(*it)->GetSource().GetGenome();
     if ( genome == CBioSource :: eGenome_unknown 
              || genome == CBioSource :: eGenome_genomic
              || genome == CBioSource :: eGenome_macronuclear) {
        return; 
     }
  }
  ITERATE (vector <const CSeq_feat*>, it, mrna_feat) {
     thisInfo.test_item_list[GetName()].push_back( GetDiscItemText((**it)));
     thisInfo.test_item_objs[GetName()].push_back(CConstRef <CObject> (*it));
  }
};


void CBioseq_DISC_mRNA_ON_WRONG_SEQUENCE_TYPE :: GetReport(CRef <CClickableItem>& c_item)
{
   unsigned cnt = c_item->item_list.size();
   c_item->obj_list = thisInfo.test_item_objs[GetName()];
   c_item->description 
      = GetIsComment(cnt, "mRNA")
        + "located on eukaryotic sequences that do not have genomic or plasmid "
        + GetNoun(cnt, "source");
};


void CBioseq_on_Aa :: CompareIntronExonList(const string& seq_id_desc, const vector <const CSeq_feat*>& exon_ls, const vector <const CSeq_feat*>& intron_ls)
{
   string setting_name = GetName_ei();
   unsigned e_idx=0, i_idx=0, exon_start, exon_stop, intron_start, intron_stop;
   unsigned e_sz = exon_ls.size(), i_sz = intron_ls.size();
   bool has_exon = true, has_intron = true;
   while (e_idx < e_sz && !m_e_exist[e_idx]) e_idx++;
   if (e_idx < e_sz) {
      exon_start = exon_ls[e_idx]->GetLocation().GetStart(eExtreme_Biological);
      exon_stop =  exon_ls[e_idx]->GetLocation().GetStop(eExtreme_Biological);
   }
   else has_exon = false;
   while (i_idx < i_sz && !m_i_exist[i_idx]) {
         i_idx ++;
   }
   if (i_idx < e_sz) {
     intron_start
         = intron_ls[i_idx]->GetLocation().GetStart(eExtreme_Biological);
     intron_stop = intron_ls[i_idx]->GetLocation().GetStop(eExtreme_Biological);
   }
   else has_intron = false;

   if (!has_exon || !has_intron) return;

   if (intron_start < exon_start) {
      if (intron_stop != exon_start - 1) {
        thisInfo.test_item_list[setting_name].push_back(
                          seq_id_desc + "$" + GetDiscItemText(*exon_ls[e_idx]));
        thisInfo.test_item_list[setting_name].push_back(
                        seq_id_desc + "$" + GetDiscItemText(*intron_ls[e_idx]));

        thisInfo.test_item_objs[setting_name + "$" + seq_id_desc].push_back(
                                      CConstRef <CObject>(exon_ls[e_idx]));
        thisInfo.test_item_objs[setting_name + "$" + seq_id_desc].push_back(
                                      CConstRef <CObject>(intron_ls[e_idx]));
      }
      while (i_idx < i_sz && !m_i_exist[i_idx]) {
           i_idx ++;
      }
      if (i_idx < i_sz) {
        intron_start 
            = intron_ls[i_idx]->GetLocation().GetStart(eExtreme_Biological);
        intron_stop 
            = intron_ls[i_idx]->GetLocation().GetStop(eExtreme_Biological);
      }
  };

  unsigned next_exon_start, next_exon_stop;
  while (e_idx < e_sz && i_idx < i_sz) {
      while ( ++ e_idx < e_sz && !m_e_exist[e_idx]);
      next_exon_start 
           = exon_ls[e_idx]->GetLocation().GetStart(eExtreme_Biological);
      next_exon_stop 
           = exon_ls[e_idx]->GetLocation().GetStop(eExtreme_Biological);
      while (i_idx < i_sz && intron_start < next_exon_start) {
        if (intron_start !=exon_stop + 1 || intron_stop != next_exon_start - 1){
          if (intron_start != exon_stop + 1) {
              thisInfo.test_item_list[setting_name].push_back(
                         seq_id_desc + "$" + GetDiscItemText(*exon_ls[e_idx]));
              thisInfo.test_item_objs[setting_name + "$" + seq_id_desc]
                      .push_back(CConstRef <CObject>(exon_ls[e_idx]));
          }
          thisInfo.test_item_list[setting_name].push_back(
                        seq_id_desc + "$" + GetDiscItemText(*intron_ls[i_idx]));
          thisInfo.test_item_objs[setting_name + "$" + seq_id_desc]
                  .push_back(CConstRef <CObject>(intron_ls[i_idx]));
          if (intron_stop != next_exon_start - 1) {
              thisInfo.test_item_list[setting_name].push_back(
                          seq_id_desc + "$" + GetDiscItemText(*exon_ls[e_idx]));
              thisInfo.test_item_objs[setting_name + "$" + seq_id_desc]
                      .push_back(CConstRef <CObject>(exon_ls[e_idx]));
          }
        }
        while ( ++ i_idx < i_sz && !m_i_exist[i_idx]);
        if ( i_idx < i_sz) {
          intron_start 
               = intron_ls[i_idx]->GetLocation().GetStart(eExtreme_Biological);
          intron_stop 
               = intron_ls[i_idx]->GetLocation().GetStop(eExtreme_Biological);
        }
      }
      exon_start = next_exon_start;
      exon_stop = next_exon_stop;
  }
  if (i_idx < i_sz) {
      if (intron_start != exon_stop + 1) {
          thisInfo.test_item_list[setting_name].push_back(
                          seq_id_desc + "$" + GetDiscItemText(*exon_ls[e_idx]));
          thisInfo.test_item_list[setting_name].push_back(
                        seq_id_desc + "$" + GetDiscItemText(*intron_ls[i_idx]));
          thisInfo.test_item_objs[setting_name + "$" + seq_id_desc]
                  .push_back(CConstRef <CObject>(exon_ls[e_idx]));
          thisInfo.test_item_objs[setting_name + "$" + seq_id_desc]
                  .push_back(CConstRef <CObject>(exon_ls[i_idx]));
      }
  }
};


void CBioseq_on_Aa :: GetFeatureList4Gene(const CSeq_feat* gene, const vector <const CSeq_feat*> feats, vector <unsigned> exist_ls)
{
   unsigned i=0;
   ITERATE (vector <const CSeq_feat*>, it, feats) {
      const CGene_ref* feat_gene_ref = (*it)->GetGeneXref();
      if (feat_gene_ref) {
         CConstRef <CSeq_feat> 
            feat_gene 
               = sequence::GetBestOverlappingFeat((*it)->GetLocation(), 
                                                   CSeqFeatData::e_Gene,
                                                   sequence::eOverlap_Contained,
                                                   *thisInfo.scope);
         if (feat_gene.GetPointer() == *it) { // ? need to check the pointers
               exist_ls[i] = 1; 
         }
         else {
             exist_ls[i] = 0;
         }
      }
   }
};


void CBioseq_DISC_FEATURE_MOLTYPE_MISMATCH :: TestOnObj(const CBioseq& bioseq)
{
   bool is_genomic = false;
   const CMolInfo*
     molinfo = sequence::GetMolInfo(thisInfo.scope->GetBioseqHandle(bioseq));
   if (molinfo) {
      if ( molinfo->GetBiomol() == CMolInfo::eBiomol_genomic) {
           is_genomic = true;
      }
   }
   if ( (bioseq.GetInst().GetMol() == CSeq_inst :: eMol_dna) && is_genomic ) {
         return;
   }

   string desc = GetDiscItemText(bioseq);
   CConstRef <CObject> seq_ref (&bioseq);
   if (!rrna_feat.empty()) {
       thisInfo.test_item_list[GetName()].push_back(desc);
       thisInfo.test_item_objs[GetName()].push_back(seq_ref);
   }
   else if (!otherRna_feat.empty()) {
       thisInfo.test_item_list[GetName()].push_back(desc);
       thisInfo.test_item_objs[GetName()].push_back(seq_ref);
   }
};



void CBioseq_DISC_FEATURE_MOLTYPE_MISMATCH :: GetReport(CRef <CClickableItem>& c_item)
{
   c_item->obj_list = thisInfo.test_item_objs[GetName()];
   c_item->description = GetHasComment(c_item->item_list.size(), "sequence") 
                         + "rRNA or misc_RNA features but are not genomic DNA.";
};



string CBioseq_ADJACENT_PSEUDOGENES :: GetGeneStringMatch (const string& str1, const string& str2)
{
   string rval(kEmptyStr);

   if (str1.find("hypothetical") != string::npos 
          || str1.find("transposase") != string::npos
          || str2.find("hypothetical") != string::npos
          || str2.find("transposase") != string::npos) {
     return kEmptyStr;
   }

   Str2Int txts;
   vector <string> arr;
   arr = NStr::Tokenize(str1, ";", arr);
   ITERATE (vector <string>, it, arr) {
       txts[*it] = 1;
   }
   
   arr.clear();
   arr = NStr::Tokenize(str2, ";", arr);
   ITERATE (vector <string>, it, arr) {
      if (txts.find(*it) != txts.end()) {
          txts[*it] ++;
      }
   }
   
   ITERATE (Str2Int, it, txts) {
      if (it->second > 1) {
          rval += (it->first + ";");
      }
   }
   rval = rval.substr(0, rval.size()-1);
   return rval;
};


void CBioseq_ADJACENT_PSEUDOGENES :: TestOnObj(const CBioseq& bioseq)
{
   vector <const CSeq_feat*>::const_iterator jt;   
   string match_txt;
   ITERATE (vector <const CSeq_feat*>, it, gene_feat) {
     if (!(*it)->CanGetPseudo() || !(*it)->GetPseudo()) continue;
     jt = it + 1; 
     if (jt == gene_feat.end()) continue;
     if (!(*jt)->CanGetPseudo() || !(*jt)->GetPseudo()) continue;
     if ((*it)->GetLocation().GetStrand() != (*jt)->GetLocation().GetStrand()) {
               continue;
     }
     if (!(*it)->CanGetComment() || (*it)->GetComment().empty()
              || !(*jt)->CanGetComment() || (*jt)->GetComment().empty()) {
         continue;
     }
     match_txt = GetGeneStringMatch((*it)->GetComment(), (*jt)->GetComment());
     if (match_txt.empty()) {
       const CGene_ref& this_gene = (*it)->GetData().GetGene();
       const CGene_ref& next_gene = (*jt)->GetData().GetGene();
       if (this_gene.CanGetLocus() && !this_gene.GetLocus().empty()
            && next_gene.CanGetLocus() && !next_gene.GetLocus().empty()) {
         match_txt 
              = GetGeneStringMatch(this_gene.GetLocus(), next_gene.GetLocus());
         if (match_txt.empty()) {
            if (this_gene.CanGetDesc() && !this_gene.GetDesc().empty()
                    && next_gene.CanGetDesc() && !next_gene.GetDesc().empty()){
               match_txt 
                 = GetGeneStringMatch(this_gene.GetDesc(), next_gene.GetDesc());
            }
         } 
       }
     }    
     if (!match_txt.empty()) {
       thisInfo.test_item_list[GetName()].push_back(
                                       match_txt + "$" + GetDiscItemText(**it));
       thisInfo.test_item_list[GetName()].push_back(
                                       match_txt + "$" + GetDiscItemText(**jt));

       thisInfo.test_item_objs[GetName() + "$" + match_txt].push_back(
                                                    CConstRef <CObject>(*it));
       thisInfo.test_item_objs[GetName() + "$" + match_txt].push_back(
                                                    CConstRef <CObject>(*jt));
     }
   }
};


void CBioseq_ADJACENT_PSEUDOGENES :: GetReport(CRef <CClickableItem>& c_item)
{
   Str2Strs txt2feats;
   GetTestItemList(c_item->item_list, txt2feats);
   c_item->item_list.clear();
   ITERATE (Str2Strs, it, txt2feats) {
     strtmp = "genes: Adjacent pseudogenes have the same text: " + it->first;
     AddSubcategory(c_item, GetName() + "$" + it->first, &(it->second), strtmp, 
                    strtmp, e_OtherComment);
   }
   c_item->description = NStr::UIntToString((unsigned)c_item->item_list.size()) 
                          + " pseudogenes match an adjacent pseudogene's text.";
};


void CBioseq_DISC_FEAT_OVERLAP_SRCFEAT :: TestOnObj(const CBioseq& bioseq)
{
  string src_desc, feat_desc;
  unsigned src_left, src_right, feat_left, feat_right;
  bool src_added, span_circular;
  ITERATE (vector <const CSeq_feat*>, it, bioseq_biosrc_feat) {
    src_added = false;
    src_desc = GetDiscItemText(**it);
    src_left = (*it)->GetLocation().GetStart(eExtreme_Positional);
    src_right = (*it)->GetLocation().GetStop(eExtreme_Positional);
    span_circular = (src_left > src_right);
    ITERATE (vector <const CSeq_feat*>, jt, all_feat) {
      feat_desc = GetDiscItemText(**jt); 
      if (src_desc == feat_desc) continue;
      feat_left = (*jt)->GetLocation().GetStart(eExtreme_Positional);
      feat_right = (*jt)->GetLocation().GetStop(eExtreme_Positional);
      if (!span_circular) {
        if (feat_right < src_left) continue;
        if (feat_left > src_right) break;
      }
      sequence::ECompare ovp = sequence::Compare((*jt)->GetLocation(), 
                                         (*it)->GetLocation(), thisInfo.scope);
      if (ovp != sequence::eNoOverlap 
              && ovp != sequence::eContained 
              && ovp != sequence::eSame) {
         thisInfo.test_item_list[GetName()].push_back(src_desc +"$" +feat_desc);
         thisInfo.test_item_objs[GetName() + "$" + src_desc].push_back(
                                                   CConstRef <CObject>(*jt));
         if (!src_added) {
             thisInfo.test_item_list[GetName()]
                         .push_back(src_desc +"$" +src_desc);
             thisInfo.test_item_objs[GetName() +" $" + src_desc]
                        .push_back(CConstRef <CObject>(*it));
             src_added = true;
         }
      }
    }
  }
};


void CBioseq_DISC_FEAT_OVERLAP_SRCFEAT :: GetReport(CRef <CClickableItem>& c_item)
{
   Str2Strs biosrc2feats;
   GetTestItemList(c_item->item_list, biosrc2feats);
   c_item->item_list.clear();
   ITERATE (Str2Strs, it, biosrc2feats) {
     if (it != biosrc2feats.begin()) {
         c_item.Reset(new CClickableItem);
         c_item->setting_name = GetName();
         thisInfo.disc_report_data.push_back(c_item);
     }
     c_item->item_list.push_back(it->first);
     copy(it->second.begin(), it->second.end(), 
          back_inserter(c_item->item_list));
     c_item->obj_list = thisInfo.test_item_objs[GetName()+ "$" + it->first];
     c_item->description = GetOtherComment(c_item->item_list.size(), 
                                         "feature overlaps", "features overlap")
                             + " a source feature.";
   };
};


void CBioseq_CDS_TRNA_OVERLAP :: TestOnObj(const CBioseq& bioseq)
{
   string bioseq_cd_desc, bioseq_desc(GetDiscItemText(bioseq));
   ENa_strand cd_str, trna_str;
   ITERATE (vector <const CSeq_feat*>, it, cd_feat) {
     bioseq_cd_desc = bioseq_desc + "$" + GetDiscItemText(**it);
     cd_str = (*it)->GetLocation().GetStrand();
     ITERATE (vector <const CSeq_feat*>, jt, trna_feat) {
       trna_str = (*jt)->GetLocation().GetStrand();
       sequence::ECompare 
         ovlp = sequence::Compare((*it)->GetLocation(), 
                                  (*jt)->GetLocation(), thisInfo.scope);
       if ( ((cd_str == eNa_strand_minus && trna_str == eNa_strand_minus )
                  || (cd_str !=eNa_strand_minus && trna_str !=eNa_strand_minus))
             && ovlp != sequence::eNoOverlap) {
          thisInfo.test_item_list[GetName()].push_back(
                          bioseq_cd_desc + "#" + GetDiscItemText(**jt));
       }
     }
  }
};


void CBioseq_CDS_TRNA_OVERLAP :: GetReport(CRef <CClickableItem>& c_item)
{
   Str2Strs bioseq_cd2trnas, cd2trnas;
   GetTestItemList(c_item->item_list, bioseq_cd2trnas);
   c_item->item_list.clear();
   ITERATE (Str2Strs, it, bioseq_cd2trnas) {
      CRef <CClickableItem> c_sub (new CClickableItem);
      c_sub->setting_name = GetName();
      cd2trnas.clear();
      GetTestItemList(it->second, cd2trnas, "#");
      ITERATE (Str2Strs, jt, cd2trnas) {
          c_sub->item_list.push_back(jt->first);
          AddSubcategory(c_sub, GetName(), &(jt->second), 
                         "Coding region overlaps tRNAs", 
                         "Coding region overlaps tRNAs", e_OtherComment); 
      }
      c_sub->description = GetHasComment(cd2trnas.size(), "coding region") 
                            + "overlapping tRNAs";
      copy(c_sub->item_list.begin(), c_sub->item_list.end(),
           back_inserter( c_item->item_list));
      c_item->subcategories.push_back(c_sub);
   }
   c_item->description = GetHasComment(bioseq_cd2trnas.size(), "Bioseq") 
                           + "coding region that overlap tRNAs";
};


// new comb
void CBioseq_on_tax_def :: TestOnObj(const CBioseq& bioseq)
{
   if (thisTest.is_TaxDef_run) return;
   thisTest.is_TaxDef_run = true;

   bool run_dup = (thisTest.tests_run.find(GetName_dup()) != end_it);
   bool run_inc = (thisTest.tests_run.find(GetName_inc()) != end_it);
   bool run_missing = (thisTest.tests_run.find(GetName_missing()) != end_it);

   string taxnm(kEmptyStr), title(kEmptyStr), tax_desc;
   CConstRef <CObject> src_ref (null);
   ITERATE (vector <const CSeqdesc*>, it, bioseq_biosrc_seqdesc) {
      if ((*it)->GetSource().GetOrg().CanGetTaxname()) {
        taxnm = (*it)->GetSource().GetOrg().GetTaxname();
        tax_desc = GetDiscItemText(**it, bioseq);
        src_ref.Reset(*it);
        break;
      }
   }

   string lookfor, desc;
   bool add = false;
   if (!taxnm.empty() && !bioseq_title.empty()) {
      desc = GetDiscItemText(*(bioseq_title[0]), bioseq);
      CConstRef <CObject> title_ref(bioseq_title[0]);
      if (!bioseq_title.empty()) {
             title = bioseq_title[0]->GetTitle();
      }

      // DISC_DUP_DEFLINE 
      if (run_dup && !bioseq.IsAa() && !title.empty()) {
          strtmp = NStr::ToUpper(title);
          thisInfo.test_item_list[GetName_dup()].push_back(strtmp + "$" + desc);
          thisInfo.test_item_objs[GetName_dup() +"$" + strtmp]
                     .push_back(title_ref);
      }

      // INCONSISTENT_SOURCE_DEFLINE
      if (run_inc && !title.empty()) {
         if (title.find(taxnm) == string::npos) {
            thisInfo.test_item_list[GetName_inc()].push_back(
                                                       taxnm + "$" + tax_desc);
            thisInfo.test_item_list[GetName_inc()].push_back(
                                                       taxnm + "$" + desc);
            strtmp = GetName_inc() + "$" + taxnm;
            thisInfo.test_item_objs[strtmp].push_back(src_ref);
            thisInfo.test_item_objs[strtmp].push_back(title_ref);
         }
      }
      
      // TEST_TAXNAME_NOT_IN_DEFLINE
      if (run_missing && !bioseq.IsAa()) {
        if (NStr::EqualNocase(taxnm, "Human immunodeficiency virus 1")) {
              lookfor = "HIV-1";
        }
        else if (NStr::EqualNocase(taxnm, "Human immunodeficiency virus 2")) {
             lookfor = "HIV-2";
        }
        else lookfor = taxnm;
        string pattern("\\b" + CRegexp::Escape(lookfor) + "\\b"); // ??? check CRegexpUtil
        CRegexp :: ECompile comp_flag = CRegexp::fCompile_ignore_case;
        CRegexp rx(pattern, comp_flag);
        if (rx.IsMatch(title)) {
            // capitalization must match for all but the first letter */
            size_t pos = NStr::FindNoCase(title, lookfor);
            if (CTempString(title).substr(pos+1, lookfor.size()-1) 
                     != lookfor.substr(1)) {
                 add = true;      
            }
        }
        else {
           add = true; // missing in defline
        }
        if (add) {
           thisInfo.test_item_list[GetName_missing()].push_back(desc);
           thisInfo.test_item_objs[GetName_missing()].push_back(title_ref);
        }
      }
   }
};


void CBioseq_DISC_DUP_DEFLINE :: GetReport(CRef <CClickableItem>& c_item)
{
   Str2Strs def2ls;
   GetTestItemList(c_item->item_list, def2ls);
   c_item->item_list.clear();

   vector <string> unique;
   unsigned dup_cnt = 0;
   Str2Strs::const_iterator iit;
   ITERATE (Str2Strs, it, def2ls) {
      if (it->second.size() > 1) {
              dup_cnt++;
              iit = it;
      }
      else unique.push_back(it->second[0]);
   }  
   if (!dup_cnt) {
        c_item->description = "All deflines are unique";
   }
   else {
      if (dup_cnt == 1) { 
          if ( unique.empty()) {
             c_item->item_list = iit->second;
             c_item->description 
                   = GetIsComment(c_item->item_list.size(), "definition line") 
                      + "identical";
          }
          else {
              AddSubcategory(c_item, GetName(), &(iit->second), 
                            "definition line", "identical", e_IsComment, false);
              AddSubcategory(c_item, GetName(), &unique, 
                             "definition line", "unique", e_IsComment, false);
              c_item->description = "Defline Problem Report";
          }
      }
      else {
         ITERATE (Str2Strs, it, def2ls) {
            if (it->second.size() > 1)
              AddSubcategory(c_item, GetName(), &(it->second), 
                            "definition line", "identical", e_IsComment, false);
         }
         if (!unique.empty()) {
             AddSubcategory(c_item, GetName(), &unique, "definition line",
                            "unique", e_IsComment, false);
         }
         c_item->description = "Defline Problem Report";
      }
   }
};

void CBioseq_TEST_TAXNAME_NOT_IN_DEFLINE :: GetReport(CRef <CClickableItem>& c_item)
{
  c_item->obj_list = thisInfo.test_item_objs[GetName()];
  c_item->description = GetDoesComment(c_item->item_list.size(), "defline")
            + "not contain the complete taxname.";
};

void CBioseq_INCONSISTENT_SOURCE_DEFLINE :: GetReport(CRef <CClickableItem>& c_item)
{
   Str2Strs tax2list;
   GetTestItemList(c_item->item_list, tax2list);
   c_item->item_list.clear();
   if (tax2list.size() == 1) {
     c_item->item_list = tax2list.begin()->second;
     c_item->obj_list 
          = thisInfo.test_item_objs[GetName() + "$" + tax2list.begin()->first];
     c_item->description 
          = "Organism description not found in definition line: " 
              + tax2list.begin()->first;
   }
   else {
      ITERATE (Str2Strs, it, tax2list) {
         AddSubcategory(c_item, GetName() + "$" + it->first, &(it->second),
              "Organism description not found in definition line: " + it->first,
              "", e_OtherComment, false);
      }
      c_item->description = GetDoesComment(tax2list.size(), "source") 
                            + "not match definition lines.";
   }
};

bool CBioseq_CONTAINED_CDS :: x_IgnoreContainedCDS(const CSeq_feat* sft)
{
  if (sft->CanGetComment()) {
     const string& comment = sft->GetComment();
     if (NStr::EqualNocase(comment, "alternative")
          || NStr::EqualNocase(comment, "completely contained in another CDS")){
       return true;
     }
  }
  CConstRef <CProt_ref> prot_ref = GetProtRefForFeature (*sft);
  if (prot_ref.NotEmpty() && prot_ref->CanGetName()) {
     ITERATE (list <string>, it, prot_ref->GetName()) {
       if ( (NStr::FindNoCase(*it, "mobilization") != string::npos)
                || (*it == "dnaK") || (*it == "mob") ) {
          return true;
       }
     }
  }
  return false;
};

void CBioseq_CONTAINED_CDS :: TestOnObj(const CBioseq& bioseq)
{
   set <string> contained_this_strand;
   set <string> contained_other_strand;
   ENa_strand str1, str2;
   unsigned i=0, j=0;
   string desc1, desc2;

   if (IsBioseqHasLineage(bioseq, "Eukaryota", false)) {
      return;
   }

   // IgnoreContainedCDS
   vector <int> ignore;
   ignore.reserve(cd_feat.size());
   ITERATE (vector <const CSeq_feat*>, it, cd_feat) {
      if (x_IgnoreContainedCDS(*it)) {
           ignore.push_back(1);
      }
      else {
         ignore.push_back(0);
      }
   }

   unsigned left1, right1, left2, right2;
   bool span_circular;
   for (i=0; (int)i< (int)(cd_feat.size()-1); i++) {
     if (ignore[i]) continue;
     CConstRef <CObject> cdi_ref(cd_feat[i]);
     const CSeq_loc& loc1 = cd_feat[i]->GetLocation();
     str1 = loc1.GetStrand();
     left1 = loc1.GetStart(eExtreme_Positional);
     right1 = loc1.GetStop(eExtreme_Positional);
     span_circular = (left1 > right1);
     for (j=i+1; j< cd_feat.size(); j++) {
        if (ignore[j]) continue;
        CConstRef <CObject> cdj_ref (cd_feat[j]);
        const CSeq_loc& loc2 = cd_feat[j]->GetLocation();
        str2 = loc2.GetStrand();
        left2 = loc2.GetStart(eExtreme_Positional);
        right2 = loc2.GetStop(eExtreme_Positional);
        if (!span_circular && (right1 < left2 || right2 < left1)) {
            continue;
        }
        sequence::ECompare 
            loc_cmp = sequence::Compare(loc1, loc2, thisInfo.scope);
        if (loc_cmp == sequence::eSame 
              || loc_cmp == sequence::eContained 
              || loc_cmp == sequence::eContains) {
          desc1 = GetDiscItemText(*cd_feat[i]);
          desc2 = GetDiscItemText(*cd_feat[j]);
          if (StrandOk(str1, str2)) {
             if (contained_this_strand.find(desc1) 
                      == contained_this_strand.end()) {
                thisInfo.test_item_list[GetName()].push_back("same$" + desc1);
                thisInfo.test_item_objs[GetName() + "$same"].push_back(cdi_ref);
                contained_this_strand.insert(desc1); 
             }
             if (contained_this_strand.find(desc2) == contained_this_strand.end()){
                thisInfo.test_item_list[GetName()].push_back("same$" + desc2);
                thisInfo.test_item_objs[GetName() + "$same"].push_back(cdj_ref);
                contained_this_strand.insert(desc2);
             }
          } 
          else {
             if (contained_other_strand.find(desc1) 
                                 == contained_other_strand.end()) {
                thisInfo.test_item_list[GetName()].push_back("opposite$"+desc1);
                thisInfo.test_item_objs[GetName() +"$opposite"]
                           .push_back(cdi_ref);
                contained_other_strand.insert(desc1);
             }
             if (contained_other_strand.find(desc2) 
                              == contained_other_strand.end()) {
                thisInfo.test_item_list[GetName()].push_back("opposite$"+desc2);
                thisInfo.test_item_objs[GetName()+"$opposite"]
                             .push_back(cdj_ref);
                contained_other_strand.insert(desc2);
             }
          }
        }
     }
   }
   contained_this_strand.clear();
   contained_other_strand.clear();
};


void CBioseq_CONTAINED_CDS :: GetReport(CRef <CClickableItem>& c_item)
{
   Str2Strs list_tp2items;
   GetTestItemList(c_item->item_list, list_tp2items);
   c_item->item_list.clear();
   if (list_tp2items.size() == 1) {
      c_item->item_list = list_tp2items.begin()->second;
      c_item->obj_list 
            = thisInfo.test_item_objs[GetName() + "$" + list_tp2items.begin()->first];
      c_item->description 
          = GetIsComment(c_item->item_list.size(), "coding region")
            + "completely contained in another coding region on the " 
            + list_tp2items.begin()->first + " strand.";
   }
   else {
      ITERATE (Str2Strs, it, list_tp2items) {
        AddSubcategory(c_item, 
                       GetName()+"$" + it->first, 
                       &(it->second), 
                       "coding region", 
            "completely contained in another coding region on the " + it->first + " strand.");
           
      }
      c_item->description 
          = GetIsComment(c_item->item_list.size(), "coding region") 
            + "completely contained in another coding region.";
   }
};


void CBioseq_PSEUDO_MISMATCH :: FindPseudoDiscrepancies(const CSeq_feat& seq_feat)
{
    const CGene_ref* xref_gene = seq_feat.GetGeneXref();
    if ( xref_gene ) return;//not empty
    else {
      CConstRef <CSeq_feat> 
          gene_olp = sequence::GetBestOverlappingFeat(seq_feat.GetLocation(),
                                                   CSeqFeatData::e_Gene,
                                                   sequence::eOverlap_Contained,
                                                   *thisInfo.scope);
      if (gene_olp.Empty()) return;
      if (seq_feat.CanGetPseudo() && seq_feat.GetPseudo() 
                      && !(gene_olp->CanGetPseudo() && gene_olp->GetPseudo())) {
          thisInfo.test_item_list[GetName()].push_back(
                                                 GetDiscItemText(seq_feat));
          thisInfo.test_item_list[GetName()].push_back(
                                                 GetDiscItemText(*gene_olp));
          thisInfo.test_item_objs[GetName()].push_back(
                                      CConstRef <CObject>(&seq_feat));
          thisInfo.test_item_objs[GetName()].push_back(
                                    CConstRef <CObject>(gene_olp.GetPointer()));
      }
    }
};



void CBioseq_PSEUDO_MISMATCH :: TestOnObj(const CBioseq& bioseq)
{
  ITERATE (vector <const CSeq_feat*>, it, cd_feat) {
    FindPseudoDiscrepancies(**it);
  }

  ITERATE (vector <const CSeq_feat*>, it, rna_feat) {
    FindPseudoDiscrepancies(**it);
  }
};


void CBioseq_PSEUDO_MISMATCH :: GetReport(CRef <CClickableItem>& c_item)
{
  c_item->obj_list = thisInfo.test_item_objs[GetName()];
  c_item->description 
      = NStr::UIntToString((unsigned)c_item->item_list.size()) 
        + " CDSs, RNAs, and genes have mismatching pseudos.";
};


void CBioseq_EC_NUMBER_NOTE :: TestOnObj(const CBioseq& bioseq) 
{
  string desc;
  bool done;
  ITERATE (vector <const CSeq_feat*>, it, all_feat) {
    done = false;

    if ( (*it)->CanGetComment() && !((*it)->GetComment().empty())) {
      desc = GetDiscItemText(**it); 
      CConstRef <CObject> feat_ref(*it);
      if (validator::HasECnumberPattern((*it)->GetComment())) {
         thisInfo.test_item_list[GetName()].push_back(desc);
         thisInfo.test_item_objs[GetName()].push_back(feat_ref);
      }
      else if ( (*it)->GetData().IsCdregion() && (*it)->CanGetProduct()) {
        CConstRef <CProt_ref> prot_ref (GetProtRefForFeature(**it, false));
        if (prot_ref.NotEmpty()) {
            if (prot_ref->CanGetName() && !(prot_ref->GetName().empty())) {
              ITERATE (list <string>, jt, prot_ref->GetName()) {
                if (validator::HasECnumberPattern(*jt)) {
                   thisInfo.test_item_list[GetName()].push_back(desc);
                   thisInfo.test_item_objs[GetName()].push_back(feat_ref);
                   done = true;
                   break;
                }
              }
          }
          if (!done && prot_ref->CanGetDesc() && !(prot_ref->GetDesc().empty())
                 && validator::HasECnumberPattern(prot_ref->GetDesc())) {
               thisInfo.test_item_list[GetName()].push_back(desc);
               thisInfo.test_item_objs[GetName()].push_back(feat_ref);
          }
        }
      }
    }
  }
};


void CBioseq_EC_NUMBER_NOTE :: GetReport(CRef <CClickableItem>& c_item)
{
  c_item->obj_list = thisInfo.test_item_objs[GetName()];
  c_item->description 
     = GetHasComment(c_item->item_list.size(), "feature") 
       + "EC numbers in notes or products.";
};


void CBioseq_NON_GENE_LOCUS_TAG :: TestOnObj(const CBioseq& bioseq)
{
   ITERATE (vector <const CSeq_feat*>, it, all_feat) {
     if (!(*it)->GetData().IsGene()) {
       if ( (*it)->IsSetQual()) {
         ITERATE (vector <CRef <CGb_qual> >, jt, (*it)->GetQual()) {
            if ( NStr::EqualNocase((*jt)->GetQual(), "locus_tag")) {
                 thisInfo.test_item_list[GetName()].push_back(
                                                 GetDiscItemText(**it));
                 thisInfo.test_item_objs[GetName()].push_back(
                                                 CConstRef <CObject>(*it));
                 break;
            }
         }
       }
     }
   }
};


void CBioseq_NON_GENE_LOCUS_TAG :: GetReport(CRef <CClickableItem>& c_item)
{
   c_item->obj_list = thisInfo.test_item_objs[GetName()]; 
   c_item->description 
       = GetHasComment(c_item->item_list.size(), "non_gene feature") 
         + "locus tags.";
};



void CBioseq_SHOW_TRANSL_EXCEPT :: TestOnObj(const CBioseq& bioseq)
{
  ITERATE (vector <const CSeq_feat*>, it, cd_feat) {
    if ((*it)->GetData().GetCdregion().IsSetCode_break()) { // don;t use CanGet...() for list.
       thisInfo.test_item_list[GetName()].push_back(GetDiscItemText(**it));
       thisInfo.test_item_objs[GetName()].push_back(CConstRef <CObject>(*it));
    }
  }
};


void CBioseq_SHOW_TRANSL_EXCEPT :: GetReport(CRef <CClickableItem>& c_item)
{
  c_item->obj_list = thisInfo.test_item_objs[GetName()];
  c_item->description = GetHasComment(c_item->item_list.size(), "coding region")
                         + "a translation exception.";
};


bool CBioseqTestAndRepData :: IsUnknown(const string& known_items, const unsigned idx)
{
   if (known_items.find("|" + NStr::UIntToString(idx) + "|") == string::npos) {
         return true;
   }
   else return false;
};


// equal = IsPesudo(sfp) in C
bool CBioseqTestAndRepData :: IsPseudoSeqFeatOrXrefGene(const CSeq_feat* seq_feat)
{
   bool rvl;
   if (seq_feat->CanGetPseudo()) rvl = seq_feat->GetPseudo();
   else {
     const CSeqFeatData& seq_feat_dt = seq_feat->GetData();
     if (seq_feat_dt.IsGene()) {
            rvl = seq_feat_dt.GetGene().GetPseudo();
     }
     else {
        CConstRef <CSeq_feat> 
           gene_olp= sequence::GetBestOverlappingFeat(seq_feat->GetLocation(),
                                                   CSeqFeatData::e_Gene,
                                                   sequence::eOverlap_Contained,
                                                   *thisInfo.scope);
        if (gene_olp.Empty()) rvl = false;
        else rvl = IsPseudoSeqFeatOrXrefGene(gene_olp.GetPointer());
     }
   }

   return rvl;
};


string CBioseqTestAndRepData :: GetRNAProductString(const CSeq_feat& seq_feat)
{
  const CRNA_ref& rna = seq_feat.GetData().GetRna();
  string rna_str(kEmptyStr);
  if (!rna.CanGetExt()) rna_str = seq_feat.GetNamedQual("product");
  else {
     const CRNA_ref::C_Ext& ext = rna.GetExt();
     switch (ext.Which()) {
       case CRNA_ref::C_Ext::e_Name:
             rna_str = ext.GetName();
             if (seq_feat.CanGetQual()
                    && (rna_str.empty() || rna_str== "ncRNA" 
                          || rna_str== "tmRNA" || rna_str== "misc_RNA")) {
                 rna_str = rule_check.GetFirstGBQualMatch(
                                     seq_feat.GetQual(), (string)"product");
             }
             break;
       case CRNA_ref::C_Ext::e_TRNA:
              GetSeqFeatLabel(seq_feat, rna_str);
              break;
       case CRNA_ref::C_Ext::e_Gen:
              if (ext.GetGen().CanGetProduct()) {
                   rna_str = ext.GetGen().GetProduct();
              }
       default: break;
     }
  }
  return rna_str;
};


bool CBioseq_on_Aa :: Is5EndInUTRList(const unsigned& start)
{
   ITERATE (vector <const CSeq_feat*>, it, utr5_feat) {
     if (start == (*it)->GetLocation().GetStart(eExtreme_Positional)) {
          return true;
     }
   }
   return false;
};


bool CBioseq_on_Aa :: Is3EndInUTRList(const unsigned& stop)
{
  ITERATE (vector <const CSeq_feat*>, it, utr3_feat) {
     if (stop == (*it)->GetLocation().GetStop(eExtreme_Positional)) {
         return true;
     }
  }
  return false;
};



void CBioseq_on_Aa :: ReportPartialConflictsForFeatureType(vector <const CSeq_feat*>& seq_feats, string label, bool check_for_utrs)
{
  CRef <CSeq_loc> gene_loc, feat_loc;
  ENa_strand feat_strand, gene_strand;
  unsigned feat_start, feat_stop, gene_start, gene_stop;
  bool conflict5, conflict3, feat_partial5, feat_partial3;
  bool gene_partial5, gene_partial3;
  CConstRef <CSeq_feat> gene_feat_4_feat;
  ITERATE (vector <const CSeq_feat*>, it, seq_feats) {
    gene_feat_4_feat = CConstRef <CSeq_feat> (GetGeneForFeature(**it));
    if (gene_feat_4_feat.NotEmpty()) {
      feat_loc 
         = sequence::Seq_loc_Merge((*it)->GetLocation(), 
                                 CSeq_loc::fMerge_Overlapping | CSeq_loc::fSort,
                                 thisInfo.scope);
      gene_loc 
         = sequence::Seq_loc_Merge(gene_feat_4_feat->GetLocation(),
                                 CSeq_loc::fMerge_Overlapping | CSeq_loc::fSort,
                                 thisInfo.scope); 
      feat_strand = feat_loc->GetStrand();
      if (feat_strand == eNa_strand_minus) {
         feat_start = feat_loc->GetStop(eExtreme_Positional);
         feat_stop = feat_loc->GetStart(eExtreme_Positional); 
      }    
      else {
         feat_start = feat_loc->GetStart(eExtreme_Positional);
         feat_stop = feat_loc->GetStop(eExtreme_Positional);
      }
      gene_strand = gene_loc->GetStrand();
      if (gene_strand  == eNa_strand_minus) {
         gene_start = gene_loc->GetStop(eExtreme_Positional);
         gene_stop = gene_loc->GetStart(eExtreme_Positional);
      } 
      else {
         gene_start = gene_loc->GetStart(eExtreme_Positional);
         gene_stop = gene_loc->GetStop(eExtreme_Positional);
      }
      feat_partial5 = feat_partial3 = gene_partial5 = gene_partial3 = false;
      feat_partial5 = feat_loc->IsPartialStart(eExtreme_Biological);
      feat_partial3 = feat_loc->IsPartialStop(eExtreme_Biological);
      gene_partial5 = gene_loc->IsPartialStart(eExtreme_Biological);
      gene_partial3 = gene_loc->IsPartialStop(eExtreme_Biological);

      conflict5 = false;
      if ( (feat_partial5 && !gene_partial5) 
                || (!feat_partial5 && gene_partial5)){
        if (feat_start == gene_start) conflict5 = true;
        else if (check_for_utrs && !Is5EndInUTRList(gene_start)) {
            conflict5 = true;
        }
      }

      conflict3 = false;
      if ( (feat_partial3 && !gene_partial3) 
                || (!feat_partial3 && gene_partial3) ) {
        if (feat_stop == gene_stop) conflict3 = true;
        else if (check_for_utrs && !Is3EndInUTRList(gene_stop)) {
             conflict3 = true;
        }
      }

      strtmp = label;
      if (conflict5 || conflict3) {
        if (conflict5 && conflict3) {
             strtmp += " feature partialness conflicts with gene on both ends.";
        }
        else if (conflict5) {
             strtmp += " feature partialness conflicts with gene on 5' end.";
        }
        else {
            strtmp += " feature partialness conflicts with gene on 3' end.";
        }

        thisInfo.test_item_list[GetName_pgene()].push_back(
                               strtmp + "$" + GetDiscItemText(**it));
        thisInfo.test_item_list[GetName_pgene()].push_back(
                              strtmp + "$" +GetDiscItemText(*gene_feat_4_feat));
        thisInfo.test_item_objs[GetName_pgene() + "$" + strtmp]
                  .push_back(CConstRef <CObject>(*it));
        thisInfo.test_item_objs[GetName_pgene() + "$" + strtmp]
                  .push_back(CConstRef <CObject>(gene_feat_4_feat));
      }
    }
  }
};


bool CBioseqTestAndRepData :: IsMrnaSequence(const CBioseq& bioseq)
{
  const CMolInfo*
     molinfo = sequence::GetMolInfo(thisInfo.scope->GetBioseqHandle(bioseq));
  if (molinfo) {
     if ( molinfo->GetBiomol() == CMolInfo :: eBiomol_mRNA) {
         return true;
     }
  }
  return false;
};


void CBioseq_DISC_BACTERIA_SHOULD_NOT_HAVE_MRNA :: TestOnObj(const CBioseq& bioseq)
{
   if (IsBioseqHasLineage(bioseq, "Bacteria") && !mrna_feat.empty()) {
       thisInfo.test_item_list[GetName()].push_back(GetDiscItemText(bioseq));
       thisInfo.test_item_objs[GetName()]
                  .push_back(CConstRef <CObject>(&bioseq));
   }
};


void CBioseq_DISC_BACTERIA_SHOULD_NOT_HAVE_MRNA :: GetReport(CRef <CClickableItem>& c_item)
{
   c_item->obj_list = thisInfo.test_item_objs[GetName()];
   c_item->description
     = GetHasComment(c_item->item_list.size(), "bacterial sequence") 
        + "mRNA features.";
};



// new comb
bool CBioseq_test_on_rrna :: NameNotStandard(const string& nm)
{
  unsigned i=0;
  bool is_equal = false, is_no_case_equal = false;
  ITERATE (vector <string>, it, thisInfo.rrna_standard_name) {
    if ( (*it) == nm ) { 
        is_equal = true; 
        break;
    }
    else if ( NStr::EqualNocase((*it), nm) ) {
        is_no_case_equal = true; 
        break;
    }
    else i++;
  }

  vector <string> arr;
  string nm_new;
  if (is_equal) return false;
  else if (is_no_case_equal) {
    if (!DoesStringContainPhrase(nm, "RNA")) return true;
    arr = NStr::Tokenize(nm, " ", arr); 
    NON_CONST_ITERATE (vector <string>, it, arr) {
       if (*it != "RNA" && isalpha( (*it)[0] ) && isupper( (*it)[0] )) {
           (*it)[0] = tolower( (*it)[0]);
       }
    }
    nm_new = NStr::Join(arr, " ");
    if (nm_new == thisInfo.rrna_standard_name[i]) return false;
    else return true; 
  }
  else return true;
};


bool CBioseqTestAndRepData :: x_IsShortrRNA(const CSeq_feat* sft)
{
   if (sft->CanGetPartial() && sft->GetPartial()) return false;
   unsigned len = sequence::GetCoverage(sft->GetLocation(), thisInfo.scope);
   string rrna_name = GetRNAProductString(*sft);
   ITERATE (Str2CombDt, jt, thisInfo.rRNATerms) {
      if (NStr::FindNoCase(rrna_name, jt->first) != string::npos
              && len < (unsigned)jt->second.i_val
              && !jt->second.b_val ) {
        return true;
      }
   }

   return false;
};


void CBioseq_test_on_rrna :: TestOnObj(const CBioseq& bioseq)
{
   if (thisTest.is_RRna_run) return;
   thisTest.is_RRna_run = true;

   string desc, rrna_name, prod_str;
   bool run_nm = (thisTest.tests_run.find(GetName_nm()) != end_it);
   bool run_its = (thisTest.tests_run.find(GetName_its()) != end_it);
   bool run_short = (thisTest.tests_run.find(GetName_short()) != end_it);

   ITERATE (vector <const CSeq_feat*>, it, rrna_feat) {
     desc = GetDiscItemText(**it); 
     CConstRef <CObject> rrna_ref (*it);
     const CRNA_ref& rrna = (*it)->GetData().GetRna();

     // RRNA_NAME_CONFLICTS
     if ( run_nm && rrna.CanGetExt() 
               && rrna.GetExt().IsName()
               && NameNotStandard(rrna.GetExt().GetName())) {
        thisInfo.test_item_list[GetName_nm()].push_back(desc);
        thisInfo.test_item_objs[GetName_nm()].push_back(rrna_ref);
     }

     // DISC_INTERNAL_TRANSCRIBED_SPACER_RRNA
     if (run_its) {
         prod_str = GetRNAProductString(**it); 
         if (NStr::FindNoCase(prod_str, "internal") != string::npos
                 || NStr::FindNoCase(prod_str, "transcribed") != string::npos
                 || NStr::FindNoCase(prod_str, "spacer") != string::npos) {
             thisInfo.test_item_list[GetName_its()].push_back(desc);
             thisInfo.test_item_objs[GetName_its()].push_back(rrna_ref);
         }
     }

     // DISC_SHORT_RRNA
     if (run_short && x_IsShortrRNA(*it)) {
         thisInfo.test_item_list[GetName_short()].push_back(desc);
         thisInfo.test_item_objs[GetName_short()].push_back(rrna_ref);
     }
   }
};


void CBioseq_DISC_INTERNAL_TRANSCRIBED_SPACER_RRNA :: GetReport(CRef <CClickableItem>& c_item)
{
   c_item->obj_list = thisInfo.test_item_objs[GetName()];
   c_item->description 
      = GetOtherComment(c_item->item_list.size(), 
                        "rRNA feature product contain", 
                        "rRNA feature products contain")
         + " 'internal', 'transcribed', or 'spacer'";
};


void CBioseq_DISC_SHORT_RRNA :: GetReport(CRef <CClickableItem>& c_item)
{
   c_item->obj_list = thisInfo.test_item_objs[GetName()];
   c_item->description
      = GetIsComment(c_item->item_list.size(), "rRNA feature") + "too short.";
};


void CBioseq_RRNA_NAME_CONFLICTS :: GetReport(CRef <CClickableItem>& c_item)
{
   c_item->obj_list = thisInfo.test_item_objs[GetName()];
   c_item->description 
        = GetIsComment(c_item->item_list.size(), "rRNA product name") 
           + "not standard.";
};



CConstRef <CSeq_feat> CBioseqTestAndRepData :: GetmRNAforCDS(const CSeq_feat& cd_feat, const CSeq_entry& seq_entry)
{
  /* first, check for mRNA identified by feature xref */
  if (cd_feat.CanGetXref()) {
    ITERATE (vector <CRef <CSeqFeatXref> >, it, cd_feat.GetXref()) {
      if ( (*it)->CanGetId() ) {
        const CFeat_id& id = (*it)->GetId();
        if (id.IsLocal()) {
          const CObject_id& obj_id = id.GetLocal();
          if (obj_id.IsId()) {
            int local_id = obj_id.GetId();
            CSeq_feat_Handle 
                seq_feat_hl = thisInfo.scope->GetTSE_Handle(seq_entry).GetFeatureWithId(CSeqFeatData::eSubtype_mRNA, local_id); 
            if (seq_feat_hl) {
                return (seq_feat_hl.GetSeq_feat());
            }
          }
        }
      }
    }
  }

  /* try by location if not by xref */
  CConstRef <CSeq_feat> 
      mRNA = sequence::GetBestOverlappingFeat(cd_feat.GetLocation(),
                                              CSeqFeatData::eSubtype_mRNA,
                                              sequence::eOverlap_Subset,
                                              *thisInfo.scope);
  if (mRNA.Empty())
      mRNA = sequence::GetBestOverlappingFeat(cd_feat.GetLocation(), 
                                              CSeqFeatData::eSubtype_mRNA,
                                              sequence::eOverlap_Contained,
                                              *thisInfo.scope);
  if (mRNA.Empty()) {
      CConstRef <CBioseq> 
          bioseq = 
             sequence::GetBioseqFromSeqLoc(cd_feat.GetLocation(), *thisInfo.scope).GetCompleteBioseq();
      if (bioseq.NotEmpty() && IsmRNASequenceInGenProdSet(*bioseq) ) {
          mRNA =
             CConstRef <CSeq_feat> (sequence::GetmRNAForProduct(*bioseq, 
                                                               thisInfo.scope));
      }
  }

  return mRNA;
}



CConstRef <CProt_ref> CBioseqTestAndRepData :: GetProtRefForFeature(const CSeq_feat& seq_feat, bool look_xref)
{
  CConstRef <CProt_ref> prot_ref(0);
  if (seq_feat.GetData().IsProt()) 
       prot_ref = CConstRef <CProt_ref> (&(seq_feat.GetData().GetProt()));
  else if (seq_feat.GetData().IsCdregion()) {
    if (look_xref) prot_ref.Reset(seq_feat.GetProtXref());

    if (prot_ref.Empty() && seq_feat.CanGetProduct()) {
        CBioseq_Handle bioseq_h = sequence::GetBioseqFromSeqLoc(seq_feat.GetProduct(), *thisInfo.scope);
        if (bioseq_h) {
           for (CFeat_CI prot_ci(bioseq_h, CSeqFeatData::e_Prot); 
                 prot_ci; 
                 ++prot_ci) {
              prot_ref.Reset( (&(prot_ci->GetOriginalFeature().GetData().GetProt())) );
              if (prot_ref->GetProcessed() == CProt_ref :: eProcessed_not_set) {
                  break;
              }
           }
	}
    }
  }

  return prot_ref;
};


bool CBioseqTestAndRepData :: IsLocationOrganelle(int genome)
{
    if (genome == (int)CBioSource :: eGenome_chloroplast
          || genome == (int)CBioSource :: eGenome_chromoplast
          || genome == (int)CBioSource :: eGenome_kinetoplast
          || genome == (int)CBioSource :: eGenome_mitochondrion
          || genome == (int)CBioSource :: eGenome_cyanelle
          || genome == (int)CBioSource :: eGenome_nucleomorph
          || genome == (int)CBioSource :: eGenome_apicoplast
          || genome == (int)CBioSource :: eGenome_leucoplast
          || genome == (int)CBioSource :: eGenome_proplastid
          || genome == (int)CBioSource :: eGenome_hydrogenosome
          || genome == (int)CBioSource :: eGenome_plastid
          || genome == (int)CBioSource :: eGenome_chromatophore) {
    return true;
  } 
  else return false;
};


static string kCDSVariant(", isoform ");
static string kmRNAVariant(", transcript variant ");

bool CBioseqTestAndRepData :: ProductsMatchForRefSeq(const string& feat_prod, const string& mRNA_prod)
{
  if (feat_prod.empty() || mRNA_prod.empty()) return false;

  size_t pos_m, pos_f;
  if ( (pos_m = mRNA_prod.find(kmRNAVariant)) == string::npos) return false;

  if ( (pos_f = feat_prod.find(kCDSVariant)) == string::npos) return false;

  if (pos_m != pos_f) return false;
  else if (mRNA_prod.substr(0, pos_m) != feat_prod.substr(0, pos_f)) {
      return false;
  }

  if ( CTempString(feat_prod).substr(pos_f, kCDSVariant.size())
             == CTempString(mRNA_prod).substr(pos_m, kmRNAVariant.size())) {
       return true;
  }
  else return false;
};



// void CBioseqTestAndRepData :: TestOnMRna(const CBioseq& bioseq)
void CBioseq_on_mrna :: TestOnObj(const CBioseq& bioseq)
{
  if (thisTest.is_MRNA_run) return;
  thisTest.is_MRNA_run = true;

  bool has_qual_ids = false;
  if (bioseq.GetInst().GetMol() != CSeq_inst::eMol_dna 
           || !IsBioseqHasLineage(bioseq, "Eukaryota", false)) {
         return;
  }
  const CBioSource* 
     biosrc = sequence::GetBioSource(thisInfo.scope->GetBioseqHandle(bioseq));
  if (biosrc) {
    if (IsLocationOrganelle(biosrc->GetGenome())) {
        return;
    }
  }
  
  const CMolInfo*
     molinfo = sequence::GetMolInfo(thisInfo.scope->GetBioseqHandle(bioseq));
  if (molinfo) {
    if ( molinfo->GetBiomol() != CMolInfo::eBiomol_genomic) {
      return;
    }
  }
 
  bool run_cds = (thisTest.tests_run.find(GetName_cds()) != end_it);
  bool run_ids = (thisTest.tests_run.find(GetName_ids()) != end_it);

  string feat_prod, mRNA_prod;
  CSeq_feat_Handle cd_hl;
  CSeq_entry* seq_entry = bioseq.GetParentEntry();

  CRef <CFeature_field> feat_field (new CFeature_field);
  feat_field->SetType(eMacro_feature_type_any);
  CRef <CFeat_qual_choice> feat_qual (new CFeat_qual_choice);
  feat_qual->SetLegal_qual(eFeat_qual_legal_product);
  feat_field->SetField(*feat_qual);
  CRef <CField_type> field_type (new CField_type);
  field_type->SetFeature_field(*feat_field);

  string desc;
  ITERATE (vector <const CSeq_feat*>, it, cd_feat) {
    if (IsPseudoSeqFeatOrXrefGene(*it)) continue;
    desc = GetDiscItemText(**it);
    CConstRef <CObject> cd_ref (*it);
    CConstRef <CSeq_feat> mRNA = GetmRNAforCDS(**it, *seq_entry);
    if (mRNA.Empty()) {
       if(run_cds) { // DISC_CDS_WITHOUT_MRNA
          thisInfo.test_item_list[GetName_cds()].push_back(desc);
          thisInfo.test_item_objs[GetName_cds()].push_back(cd_ref);
       }
    }
    else {
      feat_prod = rule_check.GetQualFromFeature(**it, *feat_field);
      mRNA_prod = rule_check.GetQualFromFeature(*mRNA, *feat_field);
      // DISC_CDS_WITHOUT_MRNA
      if ( run_cds && (feat_prod != mRNA_prod) 
                   && !ProductsMatchForRefSeq(feat_prod, mRNA_prod)) {
             thisInfo.test_item_list[GetName_cds()].push_back(desc);
             thisInfo.test_item_objs[GetName_cds()].push_back(cd_ref);
      }
      // MRNA_SHOULD_HAVE_PROTEIN_TRANSCRIPT_IDS
      if (run_ids && !has_qual_ids) {
         if (!(mRNA->GetNamedQual("orig_protein_id").empty()) 
                && !(mRNA->GetNamedQual("orig_transcript_id").empty())) 
           has_qual_ids = true;
      }
    }     
  }

  // MRNA_SHOULD_HAVE_PROTEIN_TRANSCRIPT_IDS
  if (run_ids && !has_qual_ids && !m_noids) {
      thisInfo.test_item_list[GetName_ids()].push_back("no");
      m_noids = true;
  }
};

void CBioseq_DISC_CDS_WITHOUT_MRNA :: GetReport(CRef <CClickableItem>& c_item)
{
  c_item->obj_list = thisInfo.test_item_objs[GetName()]; 
  c_item->description 
     = GetDoesComment(c_item->item_list.size(), "coding region") + "not have an mRNA.";
};



void CBioseq_MRNA_SHOULD_HAVE_PROTEIN_TRANSCRIPT_IDS :: GetReport(CRef <CClickableItem>& c_item)
{
   c_item->item_list.clear();
   c_item->description = "no protein_id and transcript_id present.";
};


void CBioseq_GENE_PRODUCT_CONFLICT :: TestOnObj(const CBioseq& bioseq)
{  
   string gene_label(kEmptyStr), prod_nm, desc;
   ITERATE (vector <const CSeq_feat*>, it, cd_feat) {
     gene_label = kEmptyStr;
     // GetGeneLabel
     CGene_ref* gene = const_cast <CGene_ref*> ( (*it)->GetGeneXref());
     if (!gene) {
        CConstRef<CSeq_feat> 
          g_olp= sequence::GetBestOverlappingFeat( (*it)->GetLocation(),
                                                   CSeqFeatData::e_Gene,
                                                   sequence::eOverlap_Contained,
                                                   *thisInfo.scope);
        if (g_olp.NotEmpty()) {
            gene = const_cast< CGene_ref* > (&(g_olp->GetData().GetGene()));
        }
     } 
     if (gene && gene->CanGetLocus()) {
          gene_label = gene->SetLocus();
     }

     if (!(gene_label.empty())) {
       prod_nm = GetProdNmForCD(**it);
       prod_nm = prod_nm.empty()? "empty" : prod_nm;
       thisInfo.test_item_list[GetName()]
           .push_back(gene_label + "$" + prod_nm + "#" + GetDiscItemText(**it));
       thisInfo.test_item_objs[GetName()+"$"+gene_label]
           .push_back(CConstRef <CObject>(*it));
     }
   }
};


void CBioseq_GENE_PRODUCT_CONFLICT :: GetReport(CRef <CClickableItem>& c_item)
{
  Str2Strs gene2prod_cds, prod2cds;
  GetTestItemList(c_item->item_list, gene2prod_cds);
  c_item->item_list.clear();
  string gene;
  ITERATE (Str2Strs, it, gene2prod_cds) {
    if (it->second.size() > 1) {
      gene = it->first;
      prod2cds.clear();
      GetTestItemList(it->second, prod2cds, "#");
      if (prod2cds.size() > 1) {
         CRef <CClickableItem> c_sub (new CClickableItem);
         c_sub->setting_name = GetName();
         ITERATE (Str2Strs, jt, prod2cds) {
            copy(jt->second.begin(), jt->second.end(),
                 back_inserter( c_sub->item_list));
         }
         c_sub->obj_list = thisInfo.test_item_objs[GetName() + "$" + gene];
         c_sub->description 
            = GetHasComment(c_sub->item_list.size(), "coding region")
               + "the same gene name" + ("(" + gene + ") ")
               + "as another coding region but a different product.";
         copy(c_sub->item_list.begin(), c_sub->item_list.end(),
              back_inserter( c_item->item_list));
         copy(c_sub->obj_list.begin(), c_sub->obj_list.end(),
              back_inserter( c_item->obj_list));
         c_item->subcategories.push_back(c_sub);
      }
    }
  }

  if (!c_item->subcategories.empty()) {
        c_item->description 
            = GetHasComment(c_item->item_list.size(), "coding region")
              + "the same gene name as another coding region but a different product.";
  }
};



string CBioseq_TEST_UNUSUAL_MISC_RNA :: GetTrnaProductString(const CTrna_ext& trna_ext)
{
  int            aa = 0;
  unsigned       bb = 0, idx;
  ESeq_code_type from;
  string         str(kEmptyStr);
 
  const CTrna_ext::C_Aa& aa_dt = trna_ext.GetAa();
  switch (aa_dt.Which()) {
      case CTrna_ext::C_Aa::e_Iupacaa:
        from = eSeq_code_type_iupacaa;
        aa = aa_dt.GetIupacaa();
        break;
      case CTrna_ext::C_Aa::e_Ncbieaa :
        from = eSeq_code_type_ncbieaa;
        bb = aa_dt.GetNcbieaa();
        break;
      case CTrna_ext::C_Aa::e_Ncbi8aa :
        from = eSeq_code_type_ncbi8aa;
        aa = aa_dt.GetNcbi8aa();
        break;
      case CTrna_ext::C_Aa::e_Ncbistdaa :
        from = eSeq_code_type_ncbistdaa;
        aa = aa_dt.GetNcbistdaa();
        break;
      default:
        NCBI_USER_THROW("GetTrnaProductString: can't get from.");
  }

  if (from != eSeq_code_type_ncbieaa) {
    bb = CSeqportUtil::GetMapToIndex(from, eSeq_code_type_ncbieaa, aa);
    if (bb == 255 && from == eSeq_code_type_iupacaa) {
        if (aa == 'U') bb = 'U';
        else if (aa == 'O') bb = 'O';
    }
  }
  if (bb > 0 && bb != 255) {
    if (bb != '*') idx = bb - 64;
    else idx = 25;
    if (idx > 0 && idx < 28) str = thisInfo.trna_list [idx];
  }
  return str;
};


string CBioseq_TEST_UNUSUAL_MISC_RNA :: GetRnaRefProductString(const CRNA_ref& rna_ref)
{
  string str(kEmptyStr);

  const CRNA_ref::C_Ext& ext = rna_ref.GetExt();
  switch (ext.Which()) {
    case CRNA_ref::C_Ext::e_Name:
        str = ext.GetName(); 
        break;
    case CRNA_ref::C_Ext::e_TRNA:
        if (ext.GetTRNA().CanGetAa()) {
             str = GetTrnaProductString(ext.GetTRNA()); 
        }
        break;
    case CRNA_ref::C_Ext::e_Gen:
        if (ext.GetGen().CanGetProduct()) {
          str = ext.GetGen().GetProduct(); 
        }
        break;
    default: break;
  }

  return str;
};


void CBioseq_TEST_UNUSUAL_MISC_RNA :: TestOnObj(const CBioseq& bioseq)
{
   string product;
   ITERATE (vector <const CSeq_feat*>, it, otherRna_feat) {
      const CRNA_ref& rna_ref = (*it)->GetData().GetRna();
      if ( rna_ref.CanGetExt() 
              && rna_ref.GetExt().Which() != CRNA_ref::C_Ext::e_not_set ) {
         product = GetRnaRefProductString( (*it)->GetData().GetRna());
         if (product.find("ITS") == string::npos 
                && product.find("internal transcribed spacer") == string::npos){
              thisInfo.test_item_list[GetName()]
                         .push_back(GetDiscItemText(**it));
              thisInfo.test_item_objs[GetName()]
                         .push_back(CConstRef <CObject>(*it));
         }
      }
   }
};


void CBioseq_TEST_UNUSUAL_MISC_RNA :: GetReport(CRef <CClickableItem>& c_item)
{
   c_item->obj_list = thisInfo.test_item_objs[GetName()];
   c_item->description 
       = GetOtherComment(c_item->item_list.size(), 
                         "unexpected misc_RNA feature", 
                         "unexpected misc_RNA features")
         + " found. misc_RNAs are unusual in a genome, consider using ncRNA, misc_binding, or misc_feature as appropriate.";
};



int CBioseqTestAndRepData :: DistanceToUpstreamGap(const unsigned& pos, const CBioseq& bioseq)
{
   int offset = 0, last_gap = -1;
   ITERATE (list <CRef <CDelta_seq> >, it, bioseq.GetInst().GetExt().GetDelta().Get()) {
     if ( (*it)->IsLoc()) offset += sequence::GetCoverage((*it)->GetLoc(), thisInfo.scope);
     else if ((*it)->IsLiteral()) {
            const CSeq_literal& seq_lit = (*it)->GetLiteral();
            offset += seq_lit.GetLength();
            if (!seq_lit.CanGetSeq_data() || seq_lit.GetSeq_data().IsGap()) // IsDeltaSeqGap()
                last_gap = offset;
     }
     if (offset > (int)pos) {
       if (last_gap > -1) return ((int)pos - last_gap);
       else return -1;
     }
   }
   return -1;
};



bool CBioseq_DISC_PARTIAL_PROBLEMS :: CouldExtendLeft(const CBioseq& bioseq, const unsigned& pos)
{
  int dist;
  if (!pos) return false;
  else if (pos < 3) return true;
  else if (bioseq.GetInst().GetRepr() == CSeq_inst::eRepr_delta) {
    dist = DistanceToUpstreamGap(pos, bioseq); 
    if (dist > 0 && dist < 3) return true;
  }
  return false;
};


int CBioseqTestAndRepData :: DistanceToDownstreamGap (const int& pos, const CBioseq& bioseq)
{
  int offset = 0;

  if (pos < 0) return -1;

  ITERATE (list <CRef <CDelta_seq> >, it, bioseq.GetInst().GetExt().GetDelta().Get()) {
     if ( (*it)->IsLoc()) offset += sequence::GetCoverage((*it)->GetLoc(), thisInfo.scope);
     else if ((*it)->IsLiteral()) {
            const CSeq_literal& seq_lit = (*it)->GetLiteral();
            if ((!seq_lit.CanGetSeq_data() || seq_lit.GetSeq_data().IsGap()) //IsDeltaSeqGap()
                     && (offset > pos))
                return (offset - pos -1);
            else offset += seq_lit.GetLength();
     }
   }
   return -1;
};



bool CBioseq_DISC_PARTIAL_PROBLEMS :: CouldExtendRight(const CBioseq& bioseq, const int& pos)
{
   int dist;
   if (pos == (int)bioseq.GetLength() -1 ) return false;
   else if (pos > (int)bioseq.GetLength() - 4) return true;
   else if (bioseq.GetInst().GetRepr() == CSeq_inst::eRepr_delta) {
     dist = DistanceToDownstreamGap(pos, bioseq);
     if (dist > 0 && dist < 3) return true;
   }
   return false;
};


void CBioseq_DISC_PARTIAL_PROBLEMS :: TestOnObj(const CBioseq& bioseq)
{
  bool partial5, partial3, partialL, partialR;
  ENa_strand strand;
  ITERATE (vector <const CSeq_feat*>, it, cd_feat) {
    const CSeq_loc& seq_loc = (*it)->GetLocation();
    strand = seq_loc.GetStrand();
    partial5 = partial3 = false;
    partial5 = seq_loc.IsPartialStart(eExtreme_Biological);
    partial3 = seq_loc.IsPartialStop(eExtreme_Biological);
    if (strand == eNa_strand_minus) {
        partialL = partial3;
        partialR = partial5;
    }
    else {
        partialL = partial5;
        partialR = partial3;
    }
    if ( (partialL && CouldExtendLeft(bioseq, seq_loc.GetTotalRange().GetFrom()))
          || (partialR && CouldExtendRight(bioseq,(int)seq_loc.GetTotalRange().GetTo()))) {
        thisInfo.test_item_list[GetName()].push_back(GetDiscItemText(**it));
        thisInfo.test_item_objs[GetName()].push_back(CConstRef <CObject>(*it));
    }
  }
};


void CBioseq_DISC_PARTIAL_PROBLEMS :: GetReport(CRef <CClickableItem>& c_item)
{
  c_item->obj_list = thisInfo.test_item_objs[GetName()];
  c_item->description 
    = GetHasComment(c_item->item_list.size(), "feature") 
        + "partial ends that do not abut the end of the sequence or a gap, but could be extended by 3 or fewer nucleotides to do so"; 
};



// new comb

bool CBioseq_test_on_bac_partial :: IsNonExtendableLeft(const CBioseq& bioseq, const unsigned& pos)
{
  int  dist;
  if (pos < 3) return false; /* is either at the end or is within extending distance */
  else if (bioseq.GetInst().GetRepr() == CSeq_inst::eRepr_delta) {
    /* wasn't close to the sequence end, but perhaps it is close to a gap */
    dist = DistanceToUpstreamGap (pos, bioseq);
    if (dist > -1 && dist < 3) return false;
  }
  return true;
};


bool CBioseq_test_on_bac_partial :: IsNonExtendableRight(const CBioseq& bioseq, const unsigned& pos)
{
  int  dist;
  unsigned  len;

  len = bioseq.IsSetLength()? bioseq.GetLength() : 0;
  if ((int)pos > (int)(len - 4)) /* is either at the end or is within extending distance */
       return false; 
  else if (bioseq.GetInst().GetRepr() == CSeq_inst::eRepr_delta) {
    /* wasn't close to the sequence end, but perhaps it is close to a gap */
    dist = DistanceToDownstreamGap (pos, bioseq);
    if (dist > -1 && dist < 3) return false;
  }

  return true;
};


static string kNonExtendableException("unextendable partial coding region");
void CBioseq_test_on_bac_partial :: TestOnObj(const CBioseq& bioseq)
{
  if (thisTest.is_BacPartial_run) return;
  thisTest.is_BacPartial_run = true;

  if (bioseq.IsAa()) return;
  string desc;
  bool run_noexc = (thisTest.tests_run.find(GetName_noexc()) != end_it);
  bool run_exc = (thisTest.tests_run.find(GetName_exc()) != end_it);
  if (bioseq_biosrc_seqdesc.empty() || !IsBioseqHasLineage(bioseq, "Eukaryota")) {
     bool partial5, partial3, partialL, partialR;
     ENa_strand strand;
     ITERATE (vector <const CSeq_feat*>, it, cd_feat) {
       const CSeq_loc& seq_loc = (*it)->GetLocation();
       strand = seq_loc.GetStrand();
       partial5 = partial3 = false;
       partial5 = seq_loc.IsPartialStart(eExtreme_Biological);
       partial3 = seq_loc.IsPartialStop(eExtreme_Biological);
       if (!partial3 && !partial5) continue;
       if (strand == eNa_strand_minus) {
           partialL = partial3;
           partialR = partial5;
       }
       else {
           partialL = partial5;
           partialR = partial3;
       }
       if ( (partialL && IsNonExtendableLeft(bioseq, seq_loc.GetTotalRange().GetFrom()))
            || (partialR && IsNonExtendableRight(bioseq, seq_loc.GetTotalRange().GetTo()))) {
          desc = GetDiscItemText(**it);
          // DISC_BACTERIAL_PARTIAL_NONEXTENDABLE_PROBLEMS
          if ((*it)->CanGetExcept_text() 
                 && NStr::FindNoCase((*it)->GetExcept_text(), kNonExtendableException)
                                                                == string::npos) {
              if (run_noexc) thisInfo.test_item_list[GetName_noexc()].push_back(desc);
          }
          else { // DISC_BACTERIAL_PARTIAL_NONEXTENDABLE_EXCEPTION 
             if (run_exc) { 
                 thisInfo.test_item_list[GetName_exc()].push_back(desc);
                 thisInfo.test_item_objs[GetName_exc()].push_back(CConstRef <CObject>(*it));
             }
          }
       }
     }
  }
};


void CBioseq_DISC_BACTERIAL_PARTIAL_NONEXTENDABLE_PROBLEMS :: GetReport(CRef <CClickableItem>& c_item)
{
  c_item->obj_list = thisInfo.test_item_objs[GetName()];
  c_item->description = GetHasComment(c_item->item_list.size(), "feature")
        + "partial ends that do not abut the end of the sequence or a gap, and cannot be extended by 3 or fewer nucleotides to do so";
};


void CBioseq_DISC_BACTERIAL_PARTIAL_NONEXTENDABLE_EXCEPTION :: GetReport(CRef <CClickableItem>& c_item)
{
  c_item->obj_list = thisInfo.test_item_objs[GetName()];
  c_item->description = GetHasComment(c_item->item_list.size(), "feature")
        + "partial ends that do not abut the end of the sequence or a gap, and cannot be extended by 3 or fewer nucleotides to do so, but have the correct exception";
};

// new comb



bool CBioseq_DISC_SUSPICIOUS_NOTE_TEXT :: HasSuspiciousStr(const string& str, string& sus_str)
{
  sus_str = kEmptyStr;
  ITERATE (vector <string>, it, thisInfo.suspicious_notes) {
    if (NStr::FindNoCase(str, *it) != string::npos) { 
         sus_str = *it; 
         return true;
    }
  }
  return false;
};


void CBioseq_DISC_SUSPICIOUS_NOTE_TEXT :: TestOnObj(const CBioseq& bioseq)
{
  string sus_str, sus_str2, desc;
  ITERATE (vector <const CSeq_feat*>, it, gene_feat) {
    desc = GetDiscItemText(**it);
    CConstRef <CObject> gene_ref (*it);
    if ( (*it)->CanGetComment() 
           && HasSuspiciousStr((*it)->GetComment(), sus_str)) {
        thisInfo.test_item_list[GetName()].push_back(sus_str + "$" + desc);
        thisInfo.test_item_objs[GetName() + "$" + sus_str].push_back(gene_ref);
    }
    if ( (*it)->GetData().GetGene().CanGetDesc()
            && HasSuspiciousStr((*it)->GetData().GetGene().GetDesc(), sus_str2)
            && sus_str != sus_str2) {
        thisInfo.test_item_list[GetName()].push_back(sus_str2 + "$" + desc);
        thisInfo.test_item_objs[GetName() + "$" + sus_str2].push_back(gene_ref);
    }
  }

  ITERATE (vector <const CSeq_feat*>, it, cd_feat) {
    if ( (*it)->CanGetComment() 
             && HasSuspiciousStr((*it)->GetComment(), sus_str)) {
      thisInfo.test_item_list[GetName()].push_back(
                                        sus_str + "$" + GetDiscItemText(**it));
      thisInfo.test_item_objs[GetName() + "$" + sus_str].push_back(
                                                      CConstRef <CObject>(*it));
    }
  }
  
  ITERATE (vector <const CSeq_feat*>, it, prot_feat) {
    if ( (*it)->GetData().GetSubtype() != CSeqFeatData::eSubtype_prot) {
          continue; 
    }
    // FEATDEF_PROT
    if ( (*it)->GetData().GetProt().CanGetDesc() 
           && HasSuspiciousStr((*it)->GetData().GetProt().GetDesc(), sus_str)) {
       thisInfo.test_item_list[GetName()].push_back(
                                        sus_str + "$" + GetDiscItemText(**it));
      thisInfo.test_item_objs[GetName() + "$" + sus_str].push_back(
                                                      CConstRef <CObject>(*it));
    }

  }
  
  ITERATE (vector <const CSeq_feat*>, it, miscfeat_feat) {
    if ( (*it)->CanGetComment() 
           && HasSuspiciousStr((*it)->GetComment(), sus_str) ) {
      thisInfo.test_item_list[GetName()].push_back(
                                       sus_str + "$" + GetDiscItemText(**it));
      thisInfo.test_item_objs[GetName() + "$" + sus_str].push_back(
                                                      CConstRef <CObject>(*it));
    }
  }
};



void CBioseq_DISC_SUSPICIOUS_NOTE_TEXT :: GetReport(CRef <CClickableItem>& c_item)
{
  Str2Strs sus_str2list;
  GetTestItemList(c_item->item_list, sus_str2list);
  c_item->item_list.clear();
  ITERATE (Str2Strs, it, sus_str2list) {
    AddSubcategory(c_item, GetName() + "$" + it->first, &(it->second),
                   "note text", it->first, e_ContainsComment);
  }

  c_item->description = GetContainsComment(c_item->item_list.size(),"note text")
                         + "suspicious phrases";
};



void CBioseq_SHOW_HYPOTHETICAL_CDS_HAVING_GENE_NAME :: TestOnObj(const CBioseq& bioseq)
{
  SAnnotSelector sel_seqfeat;
  sel_seqfeat.IncludeFeatType(CSeqFeatData::e_Prot);
  string locus;
  ITERATE (vector <const CSeq_feat*>, it, cd_feat) {
    CConstRef <CSeq_feat> gene_feat_4_feat (GetGeneForFeature(**it));
    if (gene_feat_4_feat.Empty() || !(gene_feat_4_feat->GetData().GetGene().CanGetLocus())
                || gene_feat_4_feat->GetData().GetGene().GetLocus().empty() ) 
        continue;  // no gene name
    if ((*it)->CanGetProduct()) {
      CBioseq_Handle bioseq_prot = sequence::GetBioseqFromSeqLoc((*it)->GetProduct(),*thisInfo.scope);
      if (bioseq_prot) {
         CFeat_CI feat_it(bioseq_prot, CSeqFeatData :: e_Prot);
         if (feat_it) {
            const CProt_ref& prot_ref = feat_it->GetOriginalFeature().GetData().GetProt();
            if (prot_ref.CanGetName()) {
                ITERATE (list <string>, jt, prot_ref.GetName()) {
                  if (NStr::FindNoCase(*jt, "hypothetical protein") != string::npos) {
                      thisInfo.test_item_list[GetName()].push_back(GetDiscItemText(**it));
                      thisInfo.test_item_objs[GetName()].push_back(CConstRef <CObject>(*it));
                  }
                }
            }
         }
      }
    }
  }
};


void CBioseq_SHOW_HYPOTHETICAL_CDS_HAVING_GENE_NAME :: GetReport(CRef <CClickableItem>& c_item)
{
   c_item->description 
          = GetHasComment(c_item->item_list.size(), "hypothetical coding region")
                              + "a gene name";
   c_item->obj_list = thisInfo.test_item_objs[GetName()];
};



void CBioseqTestAndRepData :: TestOverlapping_ed_Feats(const vector <const CSeq_feat*>& feat, const string& setting_name, bool isGene, bool isOverlapped)
{
   string overlapping_ed_feats("|");
   unsigned i, j;
   bool span_circular;
   for (i=0; (int)i< (int)(feat.size()-1); i++) {
      if (isOverlapped && !IsUnknown(overlapping_ed_feats, i)) continue;
      const CSeq_loc& loc_i = feat[i]->GetLocation();
      span_circular = (loc_i.GetStart(eExtreme_Positional) 
                            > loc_i.GetStop(eExtreme_Positional)); 
      for (j=i+1; j< feat.size(); j++) {
           if (!isOverlapped && !IsUnknown(overlapping_ed_feats, j)) continue;
           const CSeq_loc& loc_j = feat[j]->GetLocation();

           // all feats more distant than the loc_j will have no chance of overlapping
           if (!span_circular 
                  && loc_i.GetStop(eExtreme_Positional) 
                        < loc_j.GetStart(eExtreme_Positional)) {
               break;
           }

           if (isGene && loc_i.GetStrand() != loc_j.GetStrand()) continue;
           sequence::ECompare 
                 ovlp = sequence::Compare(loc_i, loc_j, thisInfo.scope);
           if (isOverlapped) {
               if (ovlp == sequence::eContained || ovlp == sequence::eSame) {
                  thisInfo.test_item_list[setting_name]
                             .push_back(GetDiscItemText(*feat[i]));
                  thisInfo.test_item_objs[setting_name]
                             .push_back(CConstRef<CObject>(feat[i]));
                  overlapping_ed_feats += NStr::UIntToString(i) + "|";
                  break;
               }
               else if (ovlp == sequence::eContains 
                             && IsUnknown(overlapping_ed_feats, j)) {
                  thisInfo.test_item_list[setting_name]
                            .push_back(GetDiscItemText(*feat[j]));
                 thisInfo.test_item_objs[setting_name]
                            .push_back(CConstRef<CObject>(feat[j]));
                  overlapping_ed_feats += NStr::UIntToString(j) + "|";
               }
           }
           else if (ovlp != sequence :: eNoOverlap) {
               if (IsUnknown(overlapping_ed_feats, i)) {
                  thisInfo.test_item_list[setting_name]
                             .push_back(GetDiscItemText(*feat[i]));
                  thisInfo.test_item_objs[setting_name]
                             .push_back(CConstRef<CObject>(feat[i]));
                  overlapping_ed_feats += NStr::UIntToString(i) + "|";
               }
               thisInfo.test_item_list[setting_name]
                          .push_back(GetDiscItemText(*feat[j]));
               thisInfo.test_item_objs[setting_name]
                          .push_back(CConstRef<CObject>(feat[j]));
               overlapping_ed_feats += NStr::UIntToString(j) + "|";
           }
      }
   }
};



void CBioseq_TEST_OVERLAPPING_RRNAS :: TestOnObj(const CBioseq& bioseq)
{
    TestOverlapping_ed_Feats(rrna_feat, GetName(), false, false);
};



void CBioseq_TEST_OVERLAPPING_RRNAS :: GetReport(CRef <CClickableItem>& c_item)
{
    c_item->obj_list = thisInfo.test_item_objs[GetName()];
    c_item->description = GetOtherComment(c_item->item_list.size(), 
                                        "rRNA feature overlaps", "rRNA features overlap")
                           + " another rRNA feature.";
};


void CBioseq_OVERLAPPING_GENES :: TestOnObj(const CBioseq& bioseq)
{
    TestOverlapping_ed_Feats(gene_feat, GetName(), true, false);
};



void CBioseq_OVERLAPPING_GENES :: GetReport(CRef <CClickableItem>& c_item)
{
    c_item->obj_list = thisInfo.test_item_objs[GetName()];
    c_item->description 
      = GetOtherComment(c_item->item_list.size(), 
                        "gene overlaps", 
                        "genes overlap")
        + " another gene on the same strand.";
};


void CBioseq_FIND_OVERLAPPED_GENES :: TestOnObj(const CBioseq& bioseq)
{
    TestOverlapping_ed_Feats(gene_feat, GetName());
};



void CBioseq_FIND_OVERLAPPED_GENES :: GetReport(CRef <CClickableItem>& c_item)
{
    c_item->obj_list = thisInfo.test_item_objs[GetName()];
    c_item->description = GetOtherComment(c_item->item_list.size(), "gene", "genes")
                           + " completely overlapped by other genes";
};


void CBioseq_TEST_LOW_QUALITY_REGION :: TestOnObj(const CBioseq& bioseq)
{
    int start = -1;
    float min_pct = 0.25;
    double min_length = 30;
    unsigned i=0, num_ns, len;
    bool found_interval = false;
    CBioseq_Handle bioseq_h = thisInfo.scope->GetBioseqHandle(bioseq);
    if (!bioseq_h) return;
    CSeqVector seq_vec = bioseq_h.GetSeqVector(CBioseq_Handle::eCoding_Iupac);
    if (seq_vec.empty()) return;
    for (CSeqVector_CI it =seq_vec.begin(); it && !found_interval; ++it, i++) {
      if ( it.IsInGap()) continue;

      if ( (*it) != 'A' && (*it) != 'T' && (*it) != 'G' && (*it) != 'C') {
         if (start == -1) { /* start new interval if we aren't already in one */
             start = i;
             num_ns = 1;
         } 
         else num_ns++;   /* add to number of ns in this interval */
      } 
      else {
         if (start > -1) { 
         /* if we are already in an interval, see if we should continue to be */
           len = i - start;
           if ( ((float)num_ns / (float)len) < min_pct) {
              /* is the interval long enough to qualify? */
              if (len >= min_length) {
                 thisInfo.test_item_list[GetName()]
                           .push_back(GetDiscItemText(bioseq));
                 thisInfo.test_item_objs[GetName()]
                           .push_back(CConstRef <CObject>(&bioseq));
                 found_interval = true;
              }
              else { /* reset for next interval */
                 start = -1;
                 num_ns = 0;
              }
           }
         }
      }
    }
    
    // last interval
    len = i-start;
    if (!found_interval && start > -1 && len >= min_length) {
         thisInfo.test_item_list[GetName()]
                    .push_back(GetDiscItemText(bioseq));
         thisInfo.test_item_objs[GetName()]
                    .push_back(CConstRef <CObject>(&bioseq));
    }
};


void CBioseq_TEST_LOW_QUALITY_REGION :: GetReport(CRef <CClickableItem>& c_item)
{
  c_item->description 
      = GetContainsComment(c_item->item_list.size(), "sequence") + "low quality region";
  c_item->obj_list = thisInfo.test_item_objs[GetName()];
};



bool CBioseq_on_bad_gene_name :: GeneNameHas4Numbers(const string& locus)
{
  unsigned num_digits=0;
  for (unsigned i=0; i< locus.size() && num_digits < 4; i++)
    if (isdigit(locus[i])) num_digits++;
    else num_digits = 0;

  if (num_digits >=4) return true;
  else return false; 
};


void CBioseq_on_bad_gene_name :: TestOnObj(const CBioseq& bioseq)
{
  if (thisTest.is_Bad_Gene_Nm) return;
  thisTest.is_Bad_Gene_Nm = true;

  string desc, locus;
  bool is_euka; 
  bool run_gnm = (thisTest.tests_run.find(GetName_gnm()) != end_it);
  bool run_bgnm = (thisTest.tests_run.find(GetName_bgnm()) != end_it);
  ITERATE (vector <const CSeq_feat*>, it, gene_feat) {  
    const CSeqFeatData& seq_feat_dt = (*it)->GetData();
    if (seq_feat_dt.GetGene().CanGetLocus()) {
      locus = seq_feat_dt.GetGene().GetLocus();
      if (!locus.empty()) {
        desc = GetDiscItemText(**it); 
        CConstRef <CObject> gene_ref(*it);
        if (run_gnm) {  // TEST_BAD_GENE_NAME
          if (locus.size() > 10) {
              strtmp = "more than 10 characters";
              thisInfo.test_item_list[GetName_gnm()]
                           .push_back(strtmp + "$" + desc);
              thisInfo.test_item_objs[GetName_gnm() + "$" + strtmp]
                         .push_back(gene_ref);
          }
          ITERATE (vector <string>, jt, thisInfo.bad_gene_names_contained) {
              if (NStr::FindNoCase(locus, *jt) != string::npos) {
                thisInfo.test_item_list[GetName_gnm()]
                          .push_back(*jt + "$" + desc);
                thisInfo.test_item_objs[GetName_gnm() + "$" + *jt]
                           .push_back(gene_ref);
              }
          }
          if (GeneNameHas4Numbers(locus)) {
              strtmp = "4 or more consecutive numbers";
              thisInfo.test_item_list[GetName_gnm()]
                         .push_back(strtmp + "$" + desc);
              thisInfo.test_item_objs[GetName_gnm() + "$" + strtmp]
                         .push_back(gene_ref);
          }
        }
  
        if (run_bgnm) { // DISC_BAD_BACTERIAL_GENE_NAME
           is_euka = false;
           ITERATE (vector <const CSeqdesc*>, jt, bioseq_biosrc_seqdesc) {
              if (HasLineage((*jt)->GetSource(), "Eukaryota")) {
                    is_euka = true; 
                    break;
              }
           }
           if (!is_euka && (!isalpha(locus[0]) || !islower(locus[0]))) {
              thisInfo.test_item_list[GetName_bgnm()].push_back(desc);      
              thisInfo.test_item_objs[GetName_bgnm()].push_back(gene_ref);
           }
        }
      }
    }
  }
};


void CBioseq_TEST_BAD_GENE_NAME :: GetReport(CRef <CClickableItem>& c_item)
{
  Str2Strs nm2genes;
  GetTestItemList(c_item->item_list, nm2genes);
  c_item->item_list.clear();
  if ((thisTest.tests_run.find(GetName_bgnm()) != end_it)) {
     AddSubcategory(c_item, 
                    GetName_bgnm(), 
                    &thisInfo.test_item_list[GetName_bgnm()],
                    "bacterial gene", 
                    "not start with lowercase letters",
                    e_DoesComment);
  }
  if (c_item->subcategories.empty() && nm2genes.size() == 1 ) {
     c_item->item_list = nm2genes.begin()->second;
     c_item->obj_list 
         = thisInfo.test_item_objs[GetName() + "$" + nm2genes.begin()->first];
     c_item->description
         = GetContainsComment(c_item->item_list.size(), "gene") 
            + nm2genes.begin()->first;
  }
  else {
     ITERATE (Str2Strs, it, nm2genes) {
        AddSubcategory(c_item, GetName() + "$" + it->first, &(it->second), 
                       "gene", it->first, e_ContainsComment); 
      }
      c_item->description = GetContainsComment(c_item->item_list.size(),"gene") 
                               + "suspect phrase or characters";
  };
};

void CBioseq_DISC_BAD_BACTERIAL_GENE_NAME :: GetReport(CRef <CClickableItem>& c_item)
{
   // results as a sub under TEST_BAD_GENE_NAME
   if ((thisTest.tests_run.find(GetName_gnm()) != end_it)
          && thisInfo.test_item_list.find(GetName_gnm())
                   != thisInfo.test_item_list.end()) {
      return;   
   }

   c_item->obj_list = thisInfo.test_item_objs[GetName()];
   c_item->description 
         = GetDoesComment(c_item->item_list.size(), "bacterial gene") 
            + "not start with lowercase letters";
};

bool CBioseqTestAndRepData :: StrandOk(ENa_strand strand1, ENa_strand strand2)
{
  if (strand1 == eNa_strand_minus && strand2 != eNa_strand_minus) return false;
  else if (strand1 != eNa_strand_minus && strand2 == eNa_strand_minus) return false;
  else return true;
};


bool CBioseq_DISC_BAD_GENE_STRAND :: x_AreIntervalStrandsOk(const CSeq_loc& g_loc, const CSeq_loc& f_loc) 
{
  bool   found_match;
  bool   found_bad = false;
  ENa_strand     feat_strand, gene_strand;

  for (CSeq_loc_CI f_loc_it(f_loc); f_loc_it && !found_bad; ++ f_loc_it) {
     found_match = false;
     for (CSeq_loc_CI g_loc_it(g_loc); g_loc_it && !found_match; ++ g_loc_it) {
        sequence::ECompare 
            ovlp = sequence::Compare(f_loc_it.GetEmbeddingSeq_loc(), 
                                     g_loc_it.GetEmbeddingSeq_loc(), 
                                     thisInfo.scope);
        if (ovlp == sequence::eContained || ovlp == sequence::eSame) {
          found_match = true;
          feat_strand = f_loc_it.GetStrand();
          gene_strand = g_loc_it.GetStrand();
          if (!StrandOk(feat_strand, gene_strand)) {
               found_bad = true;
          }
        }
     }
  }

  return !found_bad;
};



typedef multimap <unsigned, unsigned> TIndex;
void CBioseq_DISC_BAD_GENE_STRAND :: TestOnObj(const CBioseq& bioseq)
{
  if (bioseq.IsAa()) return;

  unsigned g_left, g_right, f_left, f_right;
  TIndex fleft_idx, fright_idx;
  TIndex::iterator l_it, r_it;
  unsigned i, g_left_min, g_right_max;
  if (gene_feat.empty()) return;
  g_left_min = gene_feat[0]->GetLocation().GetStart(eExtreme_Positional);
  g_right_max 
    = gene_feat[gene_feat.size()-1]->GetLocation().GetStop(eExtreme_Positional);
  bool span_circular = (g_left_min > g_right_max);
  for (i=0; i < all_feat.size(); i++) {
      const CSeq_loc& f_loc = all_feat[i]->GetLocation();
      const CSeqFeatData& seq_feat_dt = all_feat[i]->GetData(); 
      if (seq_feat_dt.IsGene() 
              || seq_feat_dt.GetSubtype() ==CSeqFeatData::eSubtype_primer_bind){
           continue;
      }
      f_left = f_loc.GetStart(eExtreme_Positional);
      f_right = f_loc.GetStop(eExtreme_Positional);
      if (!span_circular) {
        if (f_right <= g_left_min) continue;
        if (f_left > g_right_max) break;
      }
      fleft_idx.insert(TIndex::value_type(f_left, i));
      fright_idx.insert(TIndex::value_type(f_right, i));
  }

  string g_desc, f_desc;
  set <unsigned> checked_idx;
  ITERATE (vector <const CSeq_feat*>, it, gene_feat) {
    const CSeq_loc& g_loc = (*it)->GetLocation();
    g_desc = GetDiscItemText(**it);
    g_left = g_loc.GetStart(eExtreme_Positional);
    g_right = g_loc.GetStop(eExtreme_Positional);
    l_it = fleft_idx.find(g_left);
    r_it = fright_idx.find(g_right);
    checked_idx.clear();
    if (l_it != fleft_idx.end() || r_it != fright_idx.end()) {
      if (l_it != fleft_idx.end()) {
        for (l_it = fleft_idx.lower_bound(g_left);
                 l_it != fleft_idx.upper_bound(g_left);
                 l_it++) {
           i = l_it->second;
           f_desc = GetDiscItemText(*all_feat[i]);
           checked_idx.insert(l_it->second);
           const CSeq_loc& f_loc = all_feat[i]->GetLocation();
           f_desc = GetDiscItemText(*all_feat[i]);  
           x_CompareFeats(g_loc, f_loc, g_desc, f_desc, *it, all_feat[i]);
        }
      }
      if (r_it != fright_idx.end()) {
        for (r_it = fright_idx.lower_bound(g_right);
               r_it != fright_idx.upper_bound(g_right);
               r_it ++ ) {
            if (checked_idx.find(r_it->second) == checked_idx.end()) {
                i = r_it->second; 
                f_desc = GetDiscItemText(*all_feat[i]);
                const CSeq_loc& f_loc = all_feat[i]->GetLocation();
                x_CompareFeats(g_loc, f_loc, g_desc, f_desc, *it, all_feat[i]);
            } 
        }
      }
    }
  }
};

void CBioseq_DISC_BAD_GENE_STRAND :: x_CompareFeats(const CSeq_loc& g_loc, const CSeq_loc& f_loc, const string& g_desc, const string& f_desc, const CSeq_feat* gene, const CSeq_feat* feat)
{
   bool g_mixed_strand, is_error, has_strand_plus, has_strand_minus;
   g_mixed_strand = is_error = false;

   if (g_loc.IsMix()) {
      has_strand_plus = false, has_strand_minus = false;
      ITERATE (list <CRef <CSeq_loc> >, it, g_loc.GetMix().Get()) {
        if ( (*it)->GetStrand() == eNa_strand_plus) {
           has_strand_plus = true;
        }
        else if ( (*it)->GetStrand() == eNa_strand_minus) {
           has_strand_minus = true;
        }
        if (has_strand_plus && has_strand_minus) {
           g_mixed_strand = true;
           break; 
        }
      }
   }
   if (g_mixed_strand) {
      is_error = !x_AreIntervalStrandsOk(g_loc, f_loc);
   }
   else if (!StrandOk(f_loc.GetStrand(), g_loc.GetStrand())) {
      is_error = true;
   }
   if (is_error) {
      thisInfo.test_item_list[GetName()].push_back(g_desc + "$" + f_desc);
      strtmp = GetName() + "$" + g_desc;
      if (thisInfo.test_item_objs.find(strtmp) ==thisInfo.test_item_objs.end()){
           thisInfo.test_item_objs[strtmp].push_back(CConstRef <CObject>(gene));
      }
      strtmp = GetName() + "$" + f_desc;
      if (thisInfo.test_item_objs.find(strtmp) ==thisInfo.test_item_objs.end()){
          thisInfo.test_item_objs[strtmp].push_back(CConstRef <CObject>(feat));
      }
  }
};


void CBioseq_DISC_BAD_GENE_STRAND :: GetReport(CRef <CClickableItem>& c_item)
{
   Str2Strs gene2feat;
   GetTestItemList(c_item->item_list, gene2feat);
   c_item->item_list.clear();
   unsigned i;
   ITERATE (Str2Strs, it, gene2feat) {
     CRef <CClickableItem> c_sub (new CClickableItem);
     c_sub->setting_name = GetName();
     c_sub->item_list.push_back(it->first);
     c_sub->obj_list.push_back(
                thisInfo.test_item_objs[GetName() + "$" + it->first][0]); 
     c_sub->item_list.push_back(it->second[0]);
     c_sub->obj_list.push_back(
              thisInfo.test_item_objs[GetName() + "$" + it->second[0]][0]); 
 
     if (it->second.size() > 1) {
        for (i=1; i < it->second.size(); i++) {
           c_sub->item_list.push_back(it->first);
           c_sub->obj_list.push_back(
                thisInfo.test_item_objs[GetName() + "$" + it->first][0]); 
           c_sub->item_list.push_back(it->second[i]);
           c_sub->obj_list.push_back(
                thisInfo.test_item_objs[GetName() + "$" + it->second[i]][0]);
        }
     }
     c_sub->description = "Gene and feature strands conflict";
     c_item->subcategories.push_back(c_sub); 
     copy(c_sub->item_list.begin(), c_sub->item_list.end(),
           back_inserter(c_item->item_list));
     copy(c_sub->obj_list.begin(), c_sub->obj_list.end(),
            back_inserter(c_item->obj_list));
   }

   c_item->description 
      = GetOtherComment(c_item->item_list.size()/2, 
                        "feature location conflicts", 
                        "feature locations conflict") 
          + " with gene location strands";
};



bool CBioseq_DISC_SHORT_INTRON :: PosIsAt5End(unsigned pos, const CBioseq& bioseq)
{
  unsigned seq_end = 0;

  if (!pos) return true;
  else {
    const CSeq_inst& seq_inst = bioseq.GetInst();
    if (seq_inst.GetRepr() != CSeq_inst::eRepr_seg) {
        return false;
    }
    else {
      if (seq_inst.CanGetExt()) {
         const CSeq_ext& seq_ext = seq_inst.GetExt();
         if (seq_ext.Which() != CSeq_ext :: e_Seg) {
             return false;
         }
         else {
           ITERATE (list <CRef <CSeq_loc> >, it, seq_ext.GetSeg().Get()) {
             seq_end += sequence::GetCoverage(**it, thisInfo.scope);
             if (pos == seq_end) {
                  return true;
             }
           }
        }
      }
    }
  }
  return false;
};



bool CBioseq_DISC_SHORT_INTRON :: PosIsAt3End(const unsigned pos, const CBioseq& bioseq)
{
  unsigned seq_end = 0;

  if (pos == (bioseq.GetLength() - 1))  {
       return true;
  }
  else {
   const CSeq_inst& seq_inst = bioseq.GetInst();
   if (seq_inst.GetRepr() != CSeq_inst::eRepr_seg) {
        return false;
   }
   else {
     if (seq_inst.CanGetExt()) {
        const CSeq_ext& seq_ext = seq_inst.GetExt();
        if (seq_ext.Which() != CSeq_ext :: e_Seg) {
            return false;
        }
        else {
          ITERATE (list <CRef <CSeq_loc> >, it, seq_ext.GetSeg().Get()) {
            seq_end += sequence::GetCoverage(**it, thisInfo.scope);
            if (pos == seq_end -1) {
                return true;
            }
          }
        }
    }
   }
  }
  return false;
};



void CBioseq_DISC_SHORT_INTRON :: TestOnObj(const CBioseq& bioseq)
{
  ENa_strand strand;
  bool partial5, partial3;
  unsigned start, stop;
  string desc_txt;
  ITERATE (vector <const CSeq_feat*>, it, intron_feat) {
    if (IsPseudoSeqFeatOrXrefGene(*it)) {
        continue;
    }
    const CSeq_loc& seq_loc = (*it)->GetLocation();
    if (sequence::GetCoverage(seq_loc, thisInfo.scope) >= 11) {
         continue;
    }
    strand = seq_loc.GetStrand();
    partial5 = partial3 = false;
    start = seq_loc.GetTotalRange().GetFrom();
    stop = seq_loc.GetTotalRange().GetTo();
    partial5 = seq_loc.IsPartialStart(eExtreme_Biological);
    partial3 = seq_loc.IsPartialStop(eExtreme_Biological);
    strtmp = ((*it)->CanGetExcept() && (*it)->GetExcept()) ? "except" : "empty";
    desc_txt = strtmp + "$" + GetDiscItemText(**it);
    CConstRef <CObject> sf_ref (*it);
    if ( (partial5 && strand!= eNa_strand_minus && PosIsAt5End(start, bioseq))
            || (partial3 && strand == eNa_strand_minus 
                             && PosIsAt5End(stop, bioseq) )
            || (partial5 && strand == eNa_strand_minus 
                             && PosIsAt3End(start, bioseq))
            || (partial3 && strand != eNa_strand_minus 
                             && PosIsAt3End(stop, bioseq)) ) {
       continue;
    }
    else {
       thisInfo.test_item_list[GetName()].push_back(desc_txt);
       thisInfo.test_item_objs[GetName() + "$" + strtmp].push_back(sf_ref);
    }
  }  

  bool found_short;
  unsigned last_start, last_stop;
  ITERATE (vector <const CSeq_feat*>, it, cd_feat) {
    found_short = false;
    if (IsPseudoSeqFeatOrXrefGene(*it)) {
        continue;
    }
    if ( (*it)->CanGetExcept() && (*it)->GetExcept() ) {
            continue;
    }
    CSeq_loc_CI loc_it( (*it)->GetLocation() );
    last_start = loc_it.GetRange().GetFrom();
    last_stop = loc_it.GetRange().GetTo();
    for (++loc_it; loc_it; ++loc_it) {
      start = loc_it.GetRange().GetFrom();
      stop = loc_it.GetRange().GetTo();
      if (abs((int)(start - last_stop)) < 11 
            || abs ((int)(stop - last_start)) < 11) {
           found_short = true; 
           break;
      }
      last_start = start;
      last_stop = stop; 
    }
    if (found_short) {
       thisInfo.test_item_list[GetName()].push_back("empty$" + GetDiscItemText(**it));
       thisInfo.test_item_objs[GetName() + "$empty"].push_back(
                                                    CConstRef <CObject>(*it));
    }
  }
};



void CBioseq_DISC_SHORT_INTRON :: GetReport(CRef <CClickableItem>& c_item)
{
  Str2Strs exc2seqs;
  GetTestItemList(c_item->item_list, exc2seqs);
  c_item->item_list.clear();
  if (exc2seqs.size() == 1 ) {
    c_item->item_list = exc2seqs.begin()->second;
    c_item->description = GetIsComment(c_item->item_list.size(), "intron")
                           + "shorter than 11 nt";
    c_item->description 
       += (exc2seqs.begin()->first == "empty")? "." : " and have an exception";
    c_item->obj_list 
         = thisInfo.test_item_objs[GetName() + exc2seqs.begin()->first];
  }
  else {
    ITERATE (Str2Strs, it, exc2seqs) {
      if (it->first == "empty") {
          copy(it->second.begin(), it->second.end(), 
               back_inserter(c_item->item_list));
          strtmp = GetName() + "$empty";
          copy(thisInfo.test_item_objs[strtmp].begin(),
               thisInfo.test_item_objs[strtmp].end(),
               back_inserter(c_item->obj_list));
      }
      else {
        AddSubcategory(c_item, GetName() + "$" + it->first, &(it->second), 
                       "intron",
                       "shorter than 11 nt and have an exception", e_IsComment);
      }
      c_item->description = GetIsComment(c_item->item_list.size(), "intron") 
                             + "shorter than 10 nt.";
    }
  }
};



// new comb
bool CBioseq_on_base :: x_IsDeltaSeqWithFarpointers(const CBioseq& bioseq)
{
  bool rval = false;

  if (bioseq.GetInst().GetRepr() != CSeq_inst :: eRepr_delta) return false;
  if (bioseq.GetInst().CanGetExt()) {
    ITERATE (list <CRef <CDelta_seq> >, it, bioseq.GetInst().GetExt().GetDelta().Get()) 
       if ( (*it)->IsLoc()) {rval = true; break;}
  }
  return rval;
};

//void CBioseqTestAndRepData :: TestOnBasesN(const CBioseq& bioseq)
void CBioseq_on_base :: TestOnObj(const CBioseq& bioseq)
{
    if (thisTest.is_Bases_N_run) return;
    thisTest.is_Bases_N_run = true;
    if (x_IsDeltaSeqWithFarpointers (bioseq)) return;
   
    unsigned cnt_a, cnt_t, cnt_g, cnt_c, cnt_n, tot_n, cnt_non_nt;
    cnt_a = cnt_t = cnt_g = cnt_c = cnt_n = tot_n = cnt_non_nt = 0;
    unsigned start_n, i=0;
    string n10_intvls(kEmptyStr), n14_intvls(kEmptyStr);
    CBioseq_Handle bioseq_h = thisInfo.scope->GetBioseqHandle(bioseq);
    if (!bioseq_h) return;
    CSeqVector seq_vec = bioseq_h.GetSeqVector(CBioseq_Handle::eCoding_Iupac);
    if (seq_vec.empty()) return;
    for (CSeqVector_CI it =seq_vec.begin(); it; ++it, i++) {
// if ( it.IsInGap()) cerr << "is in gap " << (*it) << endl;// gaps represented as 'N's
      if ( (*it) == 'N') {
        if (!cnt_n) start_n = i+1;
        cnt_n++;
        tot_n++;
      }
      else {
       if (cnt_n >= 10) {
          n10_intvls 
            +=", " + NStr::UIntToString(start_n) + "-" + NStr::UIntToString(i);
       }
       if (cnt_n >= 14) {
          n14_intvls 
            += ", " + NStr::UIntToString(start_n) + "-" + NStr::UIntToString(i);
       }
       cnt_n = 0;
       if (!cnt_a && (*it) == 'A') cnt_a++;
       else if (!cnt_t && (*it) == 'T') cnt_t++;
       else if (!cnt_c && (*it) == 'C') cnt_c++;
       else if (!cnt_g && (*it) == 'G') cnt_g++;
       else if ((*it) != 'A' && (*it) != 'T' && (*it) != 'C' && (*it) != 'G'){
             cnt_non_nt++;
       }
      }
    }
    // last count
    if (cnt_n >= 10) {   
          n10_intvls 
            += ", " + NStr::UIntToString(start_n) + "-" + NStr::UIntToString(i);
    }
    if (cnt_n >= 14) {
          n14_intvls 
            += ", " + NStr::UIntToString(start_n) + "-" + NStr::UIntToString(i);
    }
   
    const CSeq_id& 
      new_seq_id =
         BioseqToBestSeqId(*(bioseq_h.GetCompleteBioseq()), CSeq_id::e_Genbank);
    string id_str = new_seq_id.AsFastaString();
    id_str = CTempString(id_str).substr(id_str.find("|")+1);

    string desc = GetDiscItemText(bioseq);
    CConstRef <CObject> bsq_ref = CConstRef <CObject> (&bioseq);
    // N_RUNS
    if (!n10_intvls.empty()) { 
       thisInfo.test_item_list[GetName_n10()].push_back(
               desc + "$" + id_str + "#" + CTempString(n10_intvls).substr(2));
       thisInfo.test_item_objs[GetName_n10()+"$"+desc].push_back(bsq_ref);
    }
    // N_RUNS_14
    if (!n14_intvls.empty()) {  // just for test, should be in the TSA report          
       thisInfo.test_item_list[GetName_n14()].push_back(
                 desc + "$" + id_str + "#" + CTempString(n14_intvls).substr(2));
       thisInfo.test_item_objs[GetName_n14() + "$" +desc].push_back(bsq_ref);
    }
    // ZERO_BASECOUNT
    if (!cnt_a) {
       thisInfo.test_item_list[GetName_0()].push_back("A$" + desc);
       thisInfo.test_item_objs[GetName_0()+ "$A"].push_back(bsq_ref);
    }
    if (!cnt_t) {
       thisInfo.test_item_list[GetName_0()].push_back("T$" + desc);
       thisInfo.test_item_objs[GetName_0()+ "$T"].push_back(bsq_ref);
    }
    if (!cnt_c) {
       thisInfo.test_item_list[GetName_0()].push_back("C$" + desc);
       thisInfo.test_item_objs[GetName_0()+ "$C"].push_back(bsq_ref);
    }
    if (!cnt_g) {
       thisInfo.test_item_list[GetName_0()].push_back("G$" + desc);
       thisInfo.test_item_objs[GetName_0()+ "$G"].push_back(bsq_ref);
    }

    // DISC_PERCENT_N
    if (tot_n && (float)tot_n/(float)bioseq.GetLength() > 0.05) {
       thisInfo.test_item_list[GetName_5perc()].push_back(desc);
       thisInfo.test_item_objs[GetName_5perc()].push_back(bsq_ref);
    }

    // DISC_10_PERCENTN
    if (tot_n && (float)tot_n/(float)bioseq.GetLength() > 0.10) {
       thisInfo.test_item_list[GetName_10perc()].push_back(desc);
       thisInfo.test_item_objs[GetName_10perc()].push_back(bsq_ref);
    }
 
    // TEST_UNUSUAL_NT
    if (cnt_non_nt) {
       thisInfo.test_item_list[GetName_non_nt()].push_back(desc);
       thisInfo.test_item_objs[GetName_non_nt()].push_back(bsq_ref);
    }
};


void CBioseq_on_base :: x_AddNsReport(CRef <CClickableItem>& c_item)
{
   Str2Strs desc2intvls;
   GetTestItemList(c_item->item_list, desc2intvls);
   c_item->item_list.clear();

   bool is_n10 = (GetName() == GetName_n10());
   vector <string> rep_dt;
   ITERATE (Str2Strs, it, desc2intvls) {
     c_item->item_list.push_back(it->first);
     CRef <CClickableItem> c_sub (new CClickableItem);
     c_sub->setting_name = GetName();
     c_sub->item_list.push_back(it->first);
     rep_dt = NStr::Tokenize(it->second[0], "#", rep_dt);
     c_sub->description =
        rep_dt[0] + " has runs of Ns at the following locations:\n" + rep_dt[1];
     c_sub->obj_list = thisInfo.test_item_objs[GetName()+ "$" + it->first];
     c_item->obj_list.push_back(*(c_sub->obj_list.begin()));
     c_item->subcategories.push_back(c_sub);
     rep_dt.clear();
   }
   c_item->description = GetHasComment(c_item->item_list.size(), "sequence") 
                           + "runs of " + (is_n10 ? "10":"14") + " or more Ns.";
};

void CBioseq_N_RUNS :: GetReport(CRef <CClickableItem>& c_item) 
{
      x_AddNsReport(c_item);
};

void CBioseq_N_RUNS_14 :: GetReport(CRef <CClickableItem>& c_item)
{
     x_AddNsReport(c_item);
};

void CBioseq_ZERO_BASECOUNT :: GetReport(CRef <CClickableItem>& c_item)
{
   Str2Strs bases2list;
   GetTestItemList(c_item->item_list, bases2list);
   c_item->item_list.clear();

   string base_tp;
   ITERATE (Str2Strs, it, bases2list) {
     CRef <CClickableItem> c_sub (new CClickableItem);
     c_sub->setting_name = GetName();
     c_sub->item_list = it->second;
     c_sub->description = GetHasComment(c_sub->item_list.size(), "sequence") 
                          + "no " + it->first + "s.";
     c_sub->obj_list = thisInfo.test_item_objs[GetName() + "$" + it->first];
     c_item->subcategories.push_back(c_sub);
     copy(it->second.begin(), it->second.end(), 
          back_inserter(c_item->item_list));
     copy(c_sub->obj_list.begin(), c_sub->obj_list.end(),
          back_inserter( c_item->obj_list));
   }
};

void CBioseq_DISC_PERCENT_N :: GetReport(CRef <CClickableItem>& c_item)
{
  c_item->obj_list = thisInfo.test_item_objs[GetName()];
  c_item->description 
       = GetHasComment(c_item->item_list.size(), "sequence") + "> 5% Ns.";
};

void CBioseq_DISC_10_PERCENTN :: GetReport(CRef <CClickableItem>& c_item)
{
  c_item->obj_list = thisInfo.test_item_objs[GetName()];
  c_item->description 
      = GetHasComment(c_item->item_list.size(), "sequence") + "> 10% Ns.";
};

void CBioseq_TEST_UNUSUAL_NT :: GetReport(CRef <CClickableItem>& c_item)
{
   c_item->obj_list = thisInfo.test_item_objs[GetName()];
   c_item->description = GetContainsComment(c_item->item_list.size(), "sequence")
                          + "nucleotides that are not ATCG or N.";
};

void CBioseq_RNA_NO_PRODUCT :: TestOnObj(const CBioseq& bioseq)
{
  ITERATE (vector <const CSeq_feat*>, it, rna_feat) {
     CSeqFeatData :: ESubtype subtype = (*it)->GetData().GetSubtype();
     if (IsPseudoSeqFeatOrXrefGene(*it)) {
            continue;
     }
     if (subtype == CSeqFeatData :: eSubtype_otherRNA) {
        if ((*it)->CanGetComment()) {
          strtmp = (*it)->GetComment();
          if ((NStr::FindNoCase(strtmp.substr(0,9), "contains ") !=string::npos)
                 || (NStr::FindNoCase(strtmp.substr(0, 11), "may contain") 
                          != string::npos)) {
             continue;
          }
        }
     }
     else if (subtype == CSeqFeatData :: eSubtype_tmRNA) {
        continue;
     }
     
     if (GetRNAProductString(**it).empty()) {
        thisInfo.test_item_list[GetName()].push_back(GetDiscItemText(**it));
        thisInfo.test_item_objs[GetName()].push_back(CConstRef <CObject>(*it));
     }
  }
};


void CBioseq_RNA_NO_PRODUCT :: GetReport(CRef <CClickableItem>& c_item)
{
  c_item->obj_list = thisInfo.test_item_objs[GetName()];
  unsigned cnt = c_item->item_list.size();
  c_item->description 
      = GetHasComment(cnt, "RNA feature") 
         + "no product and " + ( (cnt>1) ? "are " : "is ") + " not pseudo.";
};


// new comb
bool CBioseq_test_on_protfeat :: EndsWithPattern(const string& pattern, const list <string>& strs)
{
  unsigned len_patt = pattern.size(), len_str;
  ITERATE (list <string>, it, strs) {
     len_str = (*it).size();
     if (len_str >= len_patt 
          && NStr::EqualNocase(CTempString(*it).substr(len_str - len_patt), 
                               pattern)) {
          return true;
     }
  }
  return false;
};

void CBioseq_test_on_protfeat :: TestOnObj(const CBioseq& bioseq) 
{
   if (thisTest.is_ProtFeat_run) return;
   thisTest.is_ProtFeat_run = true;

   string desc, head_desc, func;
   bool run_cds = (thisTest.tests_run.find(GetName_cds()) != end_it);
   bool run_ec = (thisTest.tests_run.find(GetName_ec()) != end_it);
   bool run_pnm = (thisTest.tests_run.find(GetName_pnm()) != end_it);

   bool is_subtype_prot;
   ITERATE (vector <const CSeq_feat*>, it, prot_feat) {
     const CProt_ref& prot = (*it)->GetData().GetProt();
     if (!prot.CanGetName()) continue;
     const list <string>& names = prot.GetName();
     if (names.empty()) continue;
     desc = GetDiscItemText(**it);
     CConstRef <CObject> prot_ref (*it);

     // FEATDEF_PROT
     is_subtype_prot = ((*it)->GetData().GetSubtype() == CSeqFeatData::eSubtype_prot);

     // EC_NUMBER_ON_UNKNOWN_PROTEIN
     if (run_ec && prot.CanGetEc() && !prot.GetEc().empty()) {
        strtmp = *(names.begin());
        if ((NStr::FindNoCase(strtmp, "hypothetical protein") != string::npos)
                 || (NStr::FindNoCase(strtmp, "unknown protein") != string::npos)) {
             thisInfo.test_item_list[GetName_ec()].push_back(desc);
             thisInfo.test_item_objs[GetName_ec()].push_back(prot_ref);
        }
     }

     // DISC_CDS_PRODUCT_FIND
     if (run_cds) {
        if (is_subtype_prot) {
             const CSeq_feat* 
                 cds = sequence::GetCDSForProduct( bioseq, thisInfo.scope );
             if (cds) {
                   desc = GetDiscItemText(*cds);
                   prot_ref.Reset(cds);
             }
        }
        ITERATE (Str2Str, jt, thisInfo.cds_prod_find) {
          func = jt->second;
          head_desc = kEmptyStr;
          if (func == "EndsWithPattern") {
            if (EndsWithPattern(jt->first, names)) {
                    head_desc = "end# with " + jt->first;
            }
          }
          else if (func == "ContainsPseudo") {
            if (ContainsPseudo(jt->first,names)) {
                 head_desc = "contain# '" + jt->first + "'";
            }
          }
          else if (func == "ContainsWholeWord") {
            if ( ContainsWholeWord(jt->first, names) ) {
                     head_desc = "contain# '" + jt->first + "'";
            }
          }
          if (!head_desc.empty()) {
            thisInfo.test_item_list[GetName_cds()].push_back(
                                                 head_desc + "$" + desc);
            thisInfo.test_item_objs[GetName_cds() + "$" + head_desc]
                      .push_back(prot_ref);
          }
        }
     }

     // DISC_PROTEIN_NAMES
     if ( run_pnm && is_subtype_prot) {
        thisInfo.test_item_list[GetName_pnm()].push_back(
                                         *(names.begin()) + "$" + desc);
        thisInfo.test_item_objs[GetName_pnm() + "$" + *(names.begin())]
                  .push_back(prot_ref);
     }
   }
};

void CBioseq_DISC_PROTEIN_NAMES :: GetReport(CRef <CClickableItem>& c_item)
{
   Str2Strs pnm2feats;
   GetTestItemList(c_item->item_list, pnm2feats);
   unsigned min_num = c_item->item_list.size(), cnt=0;
   min_num = min_num < 100 ? 100 : min_num;
   c_item->item_list.clear();
   
   ITERATE (Str2Strs, it, pnm2feats) {
     if (it->second.size() < min_num) continue;  // erase?
     cnt++;
   } 
   if (!cnt) return;
   ITERATE (Str2Strs, it, pnm2feats) {
     if (it->second.size() < min_num) continue;
     if (cnt >1 ) {
        AddSubcategory(c_item, GetName() + "$" + it->first, &(it->second), 
                      "protein", "name '" + it->first + "'.", e_HasComment, false);
     }
     else {
         c_item->obj_list = thisInfo.test_item_objs[GetName() + "$" + it->first];
         c_item->item_list = it->second;
         c_item->description 
            = GetHasComment(it->second.size(), "protein") + "name '" + it->first + "'.";
     }
   }
   if (cnt > 1) c_item->description = "Many proteins have the same name";
};

void CBioseq_DISC_CDS_PRODUCT_FIND :: GetReport(CRef <CClickableItem>& c_item)
{
   Str2Strs cd2feats;
   GetTestItemList(c_item->item_list, cd2feats);
   c_item->item_list.clear();
   string desc1, desc2;
   vector <string> arr;
   ITERATE (Str2Strs, it, cd2feats) {
      arr.clear();
      arr = NStr::Tokenize(it->first, "#", arr);
      AddSubcategory(c_item, GetName() + "$" + it->first, &(it->second), 
         "coding region product " + arr[0] + "s",
         "coding region products " + arr[0], e_OtherComment, true, arr[1]); 
   } 
   c_item->description = GetContainsComment(c_item->item_list.size(), "coding region product")
                          + " suspect phrase or characters";
};


/*
void CBioseq_EC_NUMBER_ON_UNKNOWN_PROTEIN :: TestOnObj(const CBioseq& bioseq)
{
   ITERATE (vector <const CSeq_feat*>, it, prot_feat) {
     const CProt_ref& prot = (*it)->GetData().GetProt();
     if (prot.CanGetName() && prot.CanGetEc() && !prot.GetEc().empty()) {
        const list <string>& names = prot.GetName();
        strtmp = *(names.begin());
        if ((NStr::FindNoCase(strtmp, "hypothetical protein") != string::npos)
                 || (NStr::FindNoCase(strtmp, "unknown protein") != string::npos))
             thisInfo.test_item_list[GetName()].push_back(GetDiscItemText(**it));
     }
   }
};
*/


void CBioseq_EC_NUMBER_ON_UNKNOWN_PROTEIN :: GetReport(CRef <CClickableItem>& c_item)
{
  c_item->obj_list = thisInfo.test_item_objs[GetName()];
  c_item->description 
     = GetHasComment(c_item->item_list.size(), "protein feature")
       + "an EC number and a protein name of 'unknown protein' or 'hypothetical protein'";
};

// new comb


bool CBioseq_FEATURE_LOCATION_CONFLICT :: IsMixStrand(const CSeq_feat* seq_feat)
{
  CSeq_loc_CI loc_it(seq_feat->GetLocation());
  unsigned cnt = 0;
  for (CSeq_loc_CI it = loc_it; it; ++it, cnt++);
  bool same_strand = true;
  if (cnt > 1) {
    ENa_strand pre_strand, this_strand;
    pre_strand = loc_it.GetStrand();
    for (CSeq_loc_CI it = ++loc_it; it && same_strand; ++it) {
       this_strand = it.GetStrand();
       if (this_strand != pre_strand) same_strand = false;
    }
  }
  return (!same_strand);  
};

bool CBioseq_FEATURE_LOCATION_CONFLICT :: IsMixedStrandGeneLocationOk(const CSeq_loc& feat_loc, const CSeq_loc& gene_loc)
{
  CSeq_loc_CI f_loc_it(feat_loc);
  CSeq_loc_CI g_loc_it(gene_loc);
  int gene_start, gene_stop, feat_start, feat_stop;

  ENa_strand gene_strand, feat_strand;
  while (f_loc_it && g_loc_it) {
    gene_strand = g_loc_it.GetStrand();
    feat_strand = f_loc_it.GetStrand();

    if (!StrandOk(gene_strand, feat_strand)) {
        return false;
    }
    
    gene_start = g_loc_it.GetRange().GetFrom(); 
    gene_stop = g_loc_it.GetRange().GetTo();
    feat_start = f_loc_it.GetRange().GetFrom();
    feat_stop = f_loc_it.GetRange().GetTo();

    if (gene_strand ==  eNa_strand_minus) {
      if (gene_stop != feat_stop) {
          return false;
      }
      while ( (gene_start != feat_start) && f_loc_it) {
        while (++f_loc_it) {
          feat_strand = f_loc_it.GetStrand();
          if (!StrandOk (gene_strand, feat_strand)) {
                return false;
          }
          feat_start = f_loc_it.GetRange().GetFrom();
          if (feat_start < gene_start) {
                return false;
          }
          else if (feat_start == gene_start) {
             break;
          }
        }
      }
    }
    else {
      if (gene_start != feat_start) {
           return false;
      }
      while (gene_stop != feat_stop && f_loc_it) {
        while (++f_loc_it) {
          feat_strand = f_loc_it.GetStrand();
          if (!StrandOk (gene_strand, feat_strand)) {
              return false;
          }
          feat_stop = f_loc_it.GetRange().GetTo();
          if (feat_stop > gene_stop) {
              return false;
          }
          else if (feat_stop == gene_stop) {
               break;
          }
        }
      }
    }
    if (!f_loc_it) {
         return false;
    }
    ++g_loc_it;
    ++f_loc_it;
  }

  if (g_loc_it || f_loc_it) {
          return false;
  }
  else {
      return true;
  }
};


bool CBioseq_FEATURE_LOCATION_CONFLICT :: IsGeneLocationOk(const CSeq_feat* seq_feat, const CSeq_feat* gene)
{
  if (IsMixStrand(seq_feat) || IsMixStrand(gene))
     return (IsMixedStrandGeneLocationOk(seq_feat->GetLocation(), gene->GetLocation()));
  else {
    const CSeq_loc& feat_loc = seq_feat->GetLocation();
    const CSeq_loc& gene_loc = gene->GetLocation();
    ENa_strand feat_strand =  feat_loc.GetStrand();
    ENa_strand gene_strand = gene_loc.GetStrand();
    int gene_left = gene_loc.GetStart(eExtreme_Biological);
    int gene_right = gene_loc.GetStop(eExtreme_Biological);
    int feat_left = feat_loc.GetStart(eExtreme_Biological);
    int feat_right = feat_loc.GetStop(eExtreme_Biological);
    
    if ( (feat_strand == eNa_strand_minus && gene_strand != eNa_strand_minus)
             || (feat_strand != eNa_strand_minus 
                      && gene_strand == eNa_strand_minus)) {
         return false;   
    }
    else if (gene_left == feat_left && gene_right == feat_right) {
          return true; 
    }
    else if ((gene_strand == eNa_strand_minus && gene_left == feat_left)
           || (gene_strand != eNa_strand_minus && gene_right == feat_right)) {
       ITERATE (vector <const CSeq_feat*>, it, rbs_feat) {
         const CSeq_loc& rbs_loc = (*it)->GetLocation();
         ENa_strand rbs_strand = rbs_loc.GetStrand();
         int rbs_left = rbs_loc.GetStart(eExtreme_Biological);
         int rbs_right = rbs_loc.GetStop(eExtreme_Biological);
         if (rbs_strand != gene_strand) continue;
         if (rbs_strand == eNa_strand_minus) {
            if (rbs_right == gene_right && rbs_left >= feat_right) {
                return true;
            }
         }
         else if (rbs_left == gene_left && rbs_right <= feat_left) {
            return true;
         }
       } 
       return false;
    }
    else {
        return false;
    }
  }
}
 


bool CBioseq_FEATURE_LOCATION_CONFLICT :: GeneRefMatch(const CGene_ref& gene1, const CGene_ref& gene2)
{
   if (gene1.GetPseudo() != gene2.GetPseudo()) return false;

   string g1_locus = gene1.CanGetLocus() ? gene1.GetLocus() : kEmptyStr;
   string g2_locus = gene2.CanGetLocus() ? gene2.GetLocus() : kEmptyStr;
   if (g1_locus != g2_locus) return false;

   string g1_allele = gene1.CanGetAllele() ? gene1.GetAllele() : kEmptyStr;
   string g2_allele = gene2.CanGetAllele() ? gene2.GetAllele() : kEmptyStr;
   if (g1_allele != g2_allele) return false;

   string g1_desc = gene1.CanGetDesc() ? gene1.GetDesc() : kEmptyStr;
   string g2_desc = gene2.CanGetDesc() ? gene2.GetDesc() : kEmptyStr;
   if (g1_desc != g2_desc) return false;

   string g1_maploc = gene1.CanGetMaploc() ? gene1.GetMaploc() : kEmptyStr;
   string g2_maploc = gene2.CanGetMaploc() ? gene2.GetMaploc() : kEmptyStr;
   if (g1_maploc != g2_maploc) {
         return false;
   }

   string g1_locus_tag 
             = gene1.CanGetLocus_tag() ? gene1.GetLocus_tag() : kEmptyStr;
   string g2_locus_tag 
             = gene2.CanGetLocus_tag() ? gene2.GetLocus_tag() : kEmptyStr;
   if (g1_locus_tag != g2_locus_tag) return false;

   unsigned i;
   bool g1_getdb = gene1.CanGetDb() ? true : false;
   bool g2_getdb = gene1.CanGetDb() ? true : false;
   if (g1_getdb && g2_getdb) {
      const vector <CRef <CDbtag> >& g1_db = gene1.GetDb();
      const vector <CRef <CDbtag> >& g2_db = gene2.GetDb();
      if (g1_db.size() != g2_db.size()) {
          return false;
      }
      for (i=0; i< g1_db.size(); i++) {
        if (g1_db[i]->GetDb() != g2_db[i]->GetDb()) {
             return false;
        }
      }
      
   }
   else if ( g1_getdb || g2_getdb ) return false; 
 
   bool g1_getsyn = gene1.CanGetSyn();
   bool g2_getsyn = gene2.CanGetSyn();
   if (g1_getsyn && g2_getsyn) {
      const list <string>& g1_syn = gene1.GetSyn(), g2_syn = gene2.GetSyn();
      if ((g1_syn.size() != g2_syn.size()) || g1_syn.empty()) return false;
      list <string> ::const_iterator it, jt;
      for (it=g1_syn.begin(), jt = g2_syn.begin(); 
             it != g1_syn.end(); 
             it++, jt++) {
         if (*it != *jt) {
             return false;
         }
      }
   }
   else if (g1_getsyn || g2_getsyn) {
       return false;
   }

   return true;
};


void CBioseq_FEATURE_LOCATION_CONFLICT :: CheckFeatureTypeForLocationDiscrepancies(const vector <const CSeq_feat*>& seq_feat, const string& feat_type)
{
   ITERATE (vector <const CSeq_feat*>, it, seq_feat) {
     const CGene_ref* gene_ref = (*it)->GetGeneXref();
     const CSeq_loc& feat_loc = (*it)->GetLocation();
     if (gene_ref && !gene_ref->IsSuppressed()) {
        bool found_match = false;
        ITERATE (vector <const CSeq_feat*>, jt, gene_feat) {
          if (GeneRefMatch(*gene_ref, (*jt)->GetData().GetGene()) 
                 && (*jt)->GetLocation().GetStrand() == feat_loc.GetStrand()) {
             if (IsGeneLocationOk(*it, *jt)) {
                 found_match = true;
             }
             else if ( (*it)->GetData().GetSubtype() 
                            != CSeqFeatData::eSubtype_bad) {
                strtmp = feat_type + "$";
                thisInfo.test_item_list[GetName()]
                            .push_back(strtmp + GetDiscItemText(**it));
                thisInfo.test_item_list[GetName()]
                            .push_back(strtmp + GetDiscItemText(**jt));
                strtmp = "$" + feat_type;
                thisInfo.test_item_objs[GetName()+strtmp]
                            .push_back(CConstRef <CObject>(*it));
                thisInfo.test_item_objs[GetName()+strtmp]
                            .push_back(CConstRef <CObject>(*jt));
             }
          }
        }
        if (!found_match) {
            thisInfo.test_item_list[GetName()]
                    .push_back(feat_type + "_missing$" + GetDiscItemText(**it));
            thisInfo.test_item_objs[GetName() + "$" + feat_type + "_missing"]
                             .push_back(CConstRef <CObject>(*it));
        }
     }
     else {
         CConstRef <CSeq_feat> 
            gene_olp= sequence::GetBestOverlappingFeat(feat_loc,
                                                   CSeqFeatData::e_Gene,
                                                   sequence::eOverlap_Contained,
                                                   *thisInfo.scope);

         if (gene_olp.NotEmpty() 
                  && !IsGeneLocationOk(*it, gene_olp.GetPointer())) {
              strtmp = feat_type + "$";
              thisInfo.test_item_list[GetName()]
                   .push_back(strtmp + GetDiscItemText(**it));
              thisInfo.test_item_list[GetName()]
                   .push_back(strtmp + GetDiscItemText(*gene_olp.GetPointer()));
              strtmp = "$" + feat_type;
              thisInfo.test_item_objs[GetName()+strtmp]
                         .push_back(CConstRef <CObject>(*it));
              thisInfo.test_item_objs[GetName()+strtmp]
                         .push_back(CConstRef <CObject>(gene_olp.GetPointer()));
         }
     }
   }
};



void CBioseq_FEATURE_LOCATION_CONFLICT :: TestOnObj(const CBioseq& bioseq)
{
  if (!bioseq.IsNa()) {
      return;
  }
  if (!IsBioseqHasLineage(bioseq, "Eukaryota", false)) {
     CheckFeatureTypeForLocationDiscrepancies(cd_feat, "Coding region");
  }
  CheckFeatureTypeForLocationDiscrepancies(rna_feat, "RNA feature");
};


void CBioseq_FEATURE_LOCATION_CONFLICT :: GetReport(CRef <CClickableItem>& c_item)
{
  Str2Strs ftp2feats;
  GetTestItemList(c_item->item_list, ftp2feats);
  c_item->item_list.clear();
 
  size_t pos;
  ITERATE (Str2Strs, it, ftp2feats) {
    if ( (pos = it->first.find("_missing") != string::npos) ) {
       AddSubcategory(c_item, 
                      GetName() + "$" + it->first, 
                      &(it->second),
                      it->first.substr(0, pos) + " xref gene", 
                      "not exist", 
                      e_DoesComment);
    }    
    else {
       AddSubcategory(c_item, 
                      GetName() + "$" + it->first, 
                      &(it->second),
                      it->first + " location", 
                      "not match gene location", 
                      e_DoesComment,
                      true,
                      kEmptyStr,
                      true);     
    }
  }

/*
  unsigned cnt=0;
  ITERATE (vector <string>, it, thisInfo.test_item_list[GetName()]) {
    CRef <CClickableItem> c_sub (new CClickableItem);
    rep_arr = NStr::Tokenize(*it, "$", rep_arr);
    size_t pos;
    if ( (pos = rep_arr[0].find("_missing")) != string::npos) {
      c_sub->description = rep_arr[0].substr(0, pos) + " xref gene does not exist";
      c_sub->item_list.push_back(rep_arr[1]);
      c_item->item_list.push_back(rep_arr[1]);
    }
    else {
      c_sub->description = rep_arr[0] + " location does not match gene location";
      c_sub->item_list.push_back(rep_arr[1]);
      c_sub->item_list.push_back( *(++it) );
      c_item->item_list.push_back(rep_arr[1]);
      c_item->item_list.push_back( *it );
    }
     
    c_item->subcategories.push_back(c_sub);
    cnt++;
    rep_arr.clear();
  }
*/
  c_item->description 
        = GetHasComment(c_item->item_list.size()/2, "feature") + "inconsistent gene locations";
};


void CBioseq_MISSING_LOCUS_TAGS :: TestOnObj(const CBioseq& bioseq) 
{
   ITERATE (vector <const CSeq_feat*>, jt, gene_feat) {
     const CGene_ref& gene = (*jt)->GetData().GetGene();
     if (gene.GetPseudo()) continue;
     if (!gene.CanGetLocus_tag() || gene.GetLocus_tag().empty()) { 
        thisInfo.test_item_list[GetName()].push_back(GetDiscItemText(**jt));
        thisInfo.test_item_objs[GetName()].push_back(CConstRef <CObject>(*jt));
     }
   }
};

void CBioseq_MISSING_LOCUS_TAGS :: GetReport(CRef <CClickableItem>& c_item)
{
   c_item->obj_list = thisInfo.test_item_objs[GetName()];
   c_item->description = GetHasComment(c_item->item_list.size(), "gene") + "no locus tags.";
};

bool CBioseq_MISSING_LOCUS_TAGS :: x_IsLocationDirSub(const CSeq_loc& seq_location)  // not actually used, because in C, the exclude_dirsub = false
{   
   const CSeq_id* seq_id  = seq_location.GetId();
   if (!seq_id) return false;
   if (seq_id->Which() == CSeq_id::e_Other) return false;
   CBioseq_Handle bioseq_h = thisInfo.scope->GetBioseqHandle(*seq_id);
   if (bioseq_h) return true;
   CConstRef <CBioseq> bioseq = bioseq_h.GetCompleteBioseq();
   if (bioseq.NotEmpty()) {
      bool ret = true, is_complete = false;

      ITERATE (list <CRef <CSeq_id> >, it, bioseq->GetId()) {
         if ( (*it)->Which() == CSeq_id :: e_Other) {
           ret = false; break;
         }
      }
      // look for WGS keyword
      for (CSeqdesc_CI seqdesc_it(bioseq_h, CSeqdesc :: e_Genbank);
                                                            ret && seqdesc_it; ++seqdesc_it) {
         const CGB_block& gb = seqdesc_it->GetGenbank();
         if (gb.CanGetKeywords()) {
             ITERATE (list <string>, it, gb.GetKeywords()) {
               if (string::npos != NStr::FindNoCase(*it, "WGS")) {
                     ret = false; break;
               }
             }
         }
      }
      for (CSeqdesc_CI seqdesc_it(bioseq_h, CSeqdesc :: e_Molinfo);
               ret && seqdesc_it; ++seqdesc_it) {
           const CMolInfo& mol = seqdesc_it->GetMolinfo();
           ret = (mol.GetTech() == CMolInfo::eTech_wgs) ? false : ret;
           is_complete =
             (mol.GetCompleteness() == CMolInfo::eCompleteness_complete) ? 
                true : is_complete;
      }
      // is genome? (complete and bacterial)?
      if (is_complete) {
        for (CSeqdesc_CI seqdesc_it(bioseq_h, CSeqdesc :: e_Source);
                                                         ret && seqdesc_it; ++seqdesc_it) {
          ret = HasLineage(seqdesc_it->GetSource(), "Bacteria")? false : ret;
        }
      }
      return ret;
   }
   else return true;  // bioseq is empty
};


void CBioseq_on_locus_tags :: TestOnObj(const CBioseq& bioseq) 
{
  if (thisTest.is_LocusTag_run) return;
  thisTest.is_LocusTag_run = true;

  string locus_tag, locus_tag_adj;
  string test_desc, test_desc_adj;

  size_t pos;
  string digit_alpha_str = "0123456789abcdefghijklmnopqrstuvwxyz";
  string lower_case_str;
  bool run_dup = (thisTest.tests_run.find(GetName_dup()) != end_it);
  bool run_incons = (thisTest.tests_run.find(GetName_incons()) != end_it);
  bool run_badtag = (thisTest.tests_run.find(GetName_badtag()) != end_it);
  bool run_glodup = (thisTest.tests_run.find(GetName_glodup()) != end_it);

  set <string> sent_gene;
  ITERATE (vector <const CSeq_feat*>, jt, gene_feat) {
     const CGene_ref& gene = (*jt)->GetData().GetGene();
     if (gene.GetPseudo()) continue;  
     test_desc = GetDiscItemText(**jt);
     CConstRef <CObject> gene_ref(*jt);
     if (gene.CanGetLocus_tag()) {
        locus_tag = gene.GetLocus_tag();
        if (!locus_tag.empty()) {
           if (run_dup) { // DUPLICATE_LOCUS_TAGS
              thisInfo.test_item_list[GetName_dup()]
                        .push_back(locus_tag + "$" + test_desc);
              thisInfo.test_item_objs[GetName_dup() +"$" + locus_tag]
                        .push_back(gene_ref);
           }
           if (run_glodup) { // DUPLICATE_LOCUS_TAGS(global)
              // if adjacent gene has the same locus_tag: for globle check need
              if ((jt+1) != gene_feat.end()) {
               const CGene_ref& gene_adj = (*(jt+1))->GetData().GetGene();
               if (gene_adj.CanGetLocus_tag() 
                        && !gene_adj.GetPseudo() 
                        && !gene_adj.GetLocus_tag().empty() ) {
                  locus_tag_adj = gene_adj.GetLocus_tag();
                  if (locus_tag_adj == locus_tag) {
                      if (sent_gene.find(test_desc) == sent_gene.end()) {
                         thisInfo.test_item_list[GetName_glodup()]
                                .push_back(locus_tag + "$" + test_desc);
                         thisInfo.test_item_objs[GetName_glodup()+"$"+locus_tag]
                                .push_back(gene_ref);
                         sent_gene.insert(test_desc);
                      }
                      strtmp = GetDiscItemText(**(jt+1));
                      if (sent_gene.find(strtmp) == sent_gene.end()) {
                         thisInfo.test_item_list[GetName_glodup()]
                                .push_back(locus_tag + "$" + strtmp);
                         thisInfo.test_item_objs[GetName_glodup()+"$"+locus_tag]
                                .push_back(CConstRef <CObject> (*(jt+1)));
                         sent_gene.insert(strtmp);
                      }
                  }
              }
             }
           }
           pos = locus_tag.find('_');
           if (pos != string::npos) {
              strtmp = locus_tag.substr(0, pos);
              if (run_incons) { // INCONSISTENT_LOCUS_TAG_PREFIX
                  thisInfo.test_item_list[GetName_incons()]
                           .push_back( strtmp + "$" + test_desc);
                  thisInfo.test_item_objs[GetName_incons() +"$" + strtmp]
                           .push_back(gene_ref);
              }

              if (run_badtag) { // BAD_LOCUS_TAG_FORMAT
                lower_case_str = NStr::ToLower(strtmp);
                if (lower_case_str.find_first_not_of(digit_alpha_str) 
                        != string::npos){
                 thisInfo.test_item_list[GetName_badtag()].push_back(test_desc);
                 thisInfo.test_item_objs[GetName_badtag()].push_back(gene_ref);
                }
                else if (pos == locus_tag.size()) {
                 thisInfo.test_item_list[GetName_badtag()].push_back(test_desc);
                 thisInfo.test_item_objs[GetName_badtag()].push_back(gene_ref);
                }
                else {
                   lower_case_str = locus_tag.substr(pos+1);
                   lower_case_str = NStr::ToLower(lower_case_str);
                   if (lower_case_str.find_first_not_of(digit_alpha_str) 
                          != string::npos) {
                    thisInfo.test_item_list[GetName_badtag()].push_back(test_desc);
                    thisInfo.test_item_objs[GetName_badtag()].push_back(gene_ref);
                   }
                }
              }
           }
           else if (run_badtag) {
              thisInfo.test_item_list[GetName_badtag()].push_back(test_desc);
              thisInfo.test_item_objs[GetName_badtag()].push_back(gene_ref);
           }
        }
     }
  }
};


void CBioseq_on_locus_tags :: GetReport_dup(CRef <CClickableItem>& c_item, const string& setting_name)
{
   Str2Strs tag2dups;
   GetTestItemList(c_item->item_list, tag2dups);
   c_item->item_list.clear();

     unsigned cnt = 0;
     bool copy_up;
     ITERATE (Str2Strs, it, tag2dups) {
        if (it->second.size() > 1) {
            copy_up = (setting_name == GetName_glodup());
            AddSubcategory(c_item, GetName() + "$" + it->first,
                             &(it->second), "gene", "locus tag " + it->first,
                             e_HasComment, copy_up);
            cnt += it->second.size();
/*
            CRef <CClickableItem> c_sub (new CClickableItem);
            c_sub->item_list = it->second;
            c_sub->setting_name = GetName();
            c_sub->description = GetHasComment(c_sub->item_list.size(), "gene")
                                   + "locus tag " + it->first;
            if (thisInfo.report== "Asndisc" || thisInfo.report== "Discrepancy"){
                      thisInfo.disc_report_data.push_back(c_sub);
            }
            else {
               c_item.Reset(new CClickableItem);
               c_item->setting_name = GetName();
               c_item->subcategories.push_back(c_sub);
            }
*/
        }
     }
     if (thisInfo.report != "Discrepancy" 
               && !c_item->subcategories.empty()) {
        if (setting_name == GetName_glodup()) {
          c_item->description = GetIsComment(cnt, "gene") 
                        + "adjacent to another gene with the same locus tag.";
        }
        else {
          c_item->description 
             = GetHasComment(cnt, "gene") + "duplicate locus tags.";
        }
     }
}

void CBioseq_BAD_LOCUS_TAG_FORMAT :: GetReport(CRef <CClickableItem>& c_item) 
{
   c_item->obj_list = thisInfo.test_item_objs[GetName_badtag()];
   c_item->description = GetIsComment(c_item->item_list.size(), "locus tag") 
                            + "incorrectly formatted.";
};

void CBioseq_INCONSISTENT_LOCUS_TAG_PREFIX :: GetReport(CRef <CClickableItem>& c_item)
{
  Str2Strs prefix2dups;
  GetTestItemList(c_item->item_list, prefix2dups);
  c_item->item_list.clear();
  ITERATE (Str2Strs, it, prefix2dups) {
     if (it != prefix2dups.begin()) {
        c_item.Reset(new CClickableItem);
        c_item->setting_name = GetName_incons();
        thisInfo.disc_report_data.push_back(c_item);
     }
     c_item->item_list = it->second;
     c_item->description = GetHasComment(c_item->item_list.size(),"feature") 
                           +"locus tag prefix " + it->first;
     c_item->obj_list = thisInfo.test_item_objs[GetName() + "$" + it->first];
  }
};

/*
void CSeqEntryTestAndRepData :: TestOnBiosrc(const CSeq_entry& seq_entry)
{
   string desc;
   ITERATE (vector <const CSeq_feat*>, it, biosrc_feat) {
     const CBioSource& biosrc = (*it)->GetData().GetBiosrc();
     desc = GetDiscItemText(**it);
     if (IsBacterialIsolate(biosrc))
               thisInfo.test_item_list[GetName_iso()].push_back(desc); 
   };

   unsigned i=0;
   ITERATE (vector <const CSeqdesc*>, it, biosrc_seqdesc) {
     const CBioSource& biosrc = (*it)->GetSource();
     desc = GetDiscItemText( **it, *(biosrc_seqdesc_seqentry[i]));
     if (IsBacterialIsolate(biosrc)) 
                 thisInfo.test_item_list[GetName_iso()].push_back(desc);
     i++;
   };

  thisTest.is_BIOSRC_run = true;
};
*/


// change parent name to CSeqEntry_on_biosrc ::
bool CSeqEntryTestAndRepData :: HasAmplifiedWithSpeciesSpecificPrimerNote(const CBioSource& biosrc)
{
  if (biosrc.CanGetSubtype()) {
    ITERATE (list <CRef <CSubSource> >, it, biosrc.GetSubtype()) {
      if ( (*it)->GetSubtype() == CSubSource::eSubtype_other
            && (*it)->GetName() == "amplified with species-specific primers")
        return true;
    }
  }

  ITERATE (list <CRef <COrgMod> >, it, biosrc.GetOrg().GetOrgname().GetMod()) {
     if ( (*it)->GetSubtype() == COrgMod::eSubtype_other
                  && (*it)->GetSubname() == "amplified with species-specific primers")
        return true;
  }
  return false;
};


bool CSeqEntryTestAndRepData :: IsBacterialIsolate(const CBioSource& biosrc)
{
   if (!HasLineage(biosrc, "Bacteria")
         || !biosrc.IsSetOrgMod()
         || HasAmplifiedWithSpeciesSpecificPrimerNote(biosrc)) return false;

   ITERATE (list <CRef <COrgMod> >, it, biosrc.GetOrg().GetOrgname().GetMod()) {
     strtmp = (*it)->GetSubname();
     if ( (*it)->GetSubtype() == COrgMod::eSubtype_isolate
             && !NStr::EqualNocase(strtmp, 0, 13, "DGGE gel band")
             && !NStr::EqualNocase(strtmp, 0, 13, "TGGE gel band")
             && !NStr::EqualNocase(strtmp, 0, 13, "SSCP gel band"))
        return true;
   }
   return false; 
};

void CSeqEntry_TEST_ALIGNMENT_HAS_SCORE :: x_CheckSeqAlign(const CSeq_align& aln, const string& desc, CConstRef <CObject> obj)
{
  if (aln.CanGetScore() && !(aln.GetScore()).empty()) {
     thisInfo.test_item_list[GetName()].push_back(desc);
     thisInfo.test_item_objs[GetName()].push_back(obj);
  } 
  if (aln.GetSegs().IsDisc()) {
     ITERATE (list <CRef <CSeq_align> >, it, aln.GetSegs().GetDisc().Get()) {
       x_CheckSeqAlign(**it, desc, obj);
     }
  }
};

void CSeqEntry_TEST_ALIGNMENT_HAS_SCORE :: x_CheckSeqEntryAlign(const CSeq_entry& seq_entry)
{
   if (seq_entry.IsSeq()) {
      const CBioseq& b_seq = seq_entry.GetSeq();
      CBioseq_Handle bsq_hl = thisInfo.scope->GetBioseqHandle(b_seq);
      for (CAlign_CI a_ci(bsq_hl); a_ci; ++a_ci) {
         x_CheckSeqAlign(*a_ci, GetDiscItemText(b_seq), CConstRef <CObject>(&b_seq)); 
      }
   }
   else {
     const CBioseq_set& bseq_set = seq_entry.GetSet();
     if (bseq_set.CanGetAnnot()) {
       ITERATE (list <CRef <CSeq_annot> >, it, bseq_set.GetAnnot()) {
         if ((*it)->GetData().IsAlign()) {
            ITERATE (list <CRef <CSeq_align> >, ait, (*it)->GetData().GetAlign()) {
                  x_CheckSeqAlign(**ait, GetDiscItemText(bseq_set), 
                                                     CConstRef <CObject> (&bseq_set));
            }
         }
       }
     } 
     ITERATE (list <CRef <CSeq_entry> >, it, bseq_set.GetSeq_set()) {
       CAlign_CI a_ci(thisInfo.scope->GetSeq_entryHandle(**it));
       if (a_ci) x_CheckSeqEntryAlign(**it);
     }
   }
};

void CSeqEntry_TEST_ALIGNMENT_HAS_SCORE :: TestOnObj(const CSeq_entry& seq_entry)
{
   CAlign_CI a_ci(thisInfo.scope->GetSeq_entryHandle(seq_entry));
   if (a_ci) x_CheckSeqEntryAlign(seq_entry);
};

void CSeqEntry_TEST_ALIGNMENT_HAS_SCORE :: GetReport(CRef <CClickableItem>& c_item)
{
   c_item->obj_list = thisInfo.test_item_objs[GetName()];
   c_item->description 
        = GetHasComment(c_item->item_list.size(), "alignment") + "score attributes";
};

bool CSeqEntry_INCONSISTENT_BIOSOURCE :: SynonymsMatch(const COrg_ref& org1, const COrg_ref& org2)
{
     bool has_syn1 = (org1.CanGetSyn() && !org1.GetSyn().empty());
     bool has_syn2 = (org2.CanGetSyn() && !org2.GetSyn().empty());

     if ( !has_syn1 && !has_syn2 ) {
          return true;
     }
     if ( has_syn1 && has_syn2 ) {
        list <string>& syn1 = const_cast<COrg_ref&>(org1).SetSyn();
        list <string>& syn2 = const_cast<COrg_ref&>(org2).SetSyn();

        if ((syn1.size() != syn2.size())) {
            return false;
        }

        syn1.sort();
        syn2.sort();

        list <string> :: const_iterator it, jt;
        for (it = syn1.begin(), jt = syn2.begin(); 
              it != syn1.end(); 
              it++, jt++) {
           if ( (*it) != (*jt)) {
              return false;
           }
        }
        return true;
     }
     else {
        return false;
     }
};  // SynonymsMatch



bool CSeqEntry_INCONSISTENT_BIOSOURCE :: DbtagMatch(const COrg_ref& org1, const COrg_ref& org2)
{
    bool has_db1 = (org1.CanGetDb() && !org1.GetDb().empty());
    bool has_db2 = (org2.CanGetDb() && !org2.GetDb().empty());

    if ( !has_db1 && !has_db2) return true;
    else if ( has_db1 && has_db2)  {
      const vector < CRef < CDbtag > >& db1 = org1.GetDb();
      const vector < CRef < CDbtag > >& db2 = org2.GetDb();

      if (db1.size() != db2.size()) {
           return false;
      }
      for (unsigned i=0; i< db1.size(); i++) {   // already sorted
           if ( !db1[i]->Match(*db2[i]) ) {
              return false;
           }
      };
      return true;
    }
    else return false;
}; // DbtagMatch



bool CSeqEntry_INCONSISTENT_BIOSOURCE :: OrgModSetMatch(const COrgName& nm1, const COrgName& nm2) 
{
  bool has_mod1 = (nm1.CanGetMod() && !nm1.GetMod().empty());
  bool has_mod2 = (nm2.CanGetMod() && !nm2.GetMod().empty());

  if (!has_mod1 && !has_mod2) return true;
  if (has_mod1 && has_mod2) {
     const list < CRef <COrgMod> >& mod1= nm1.GetMod();
     const list < CRef <COrgMod> >& mod2 = nm2.GetMod();

     if ( (mod1.size() != mod2.size())) {
          return false;
     }
     list < CRef < COrgMod > > ::const_iterator it, jt;
     // already sorted
     for (it = mod1.begin(), jt = mod2.begin(); it != mod1.end(); it++, jt++) {
       string att_i = (*it)->CanGetAttrib()? (*it)->GetAttrib() : kEmptyStr;
       string att_j = (*jt)->CanGetAttrib()? (*jt)->GetAttrib(): kEmptyStr;
       if ( (*it)->GetSubtype() != (*jt)->GetSubtype()
                  || (*it)->GetSubname() != (*jt)->GetSubname() 
                  || att_i != att_j) {
            return false;
       }
     }
     return true;
  }
  else {
    return false;
  }
};  // OrgModSetMatch



bool CSeqEntry_INCONSISTENT_BIOSOURCE :: OrgNameMatch(const COrg_ref& org1, const COrg_ref& org2)
{
   bool has_orgnm1 = org1.CanGetOrgname();
   bool has_orgnm2 = org2.CanGetOrgname();
   
   if (!has_orgnm1 && !has_orgnm2) return true;
   else if (has_orgnm1 && has_orgnm2) {
     const COrgName& nm1 = org1.GetOrgname();
     const COrgName& nm2 = org2.GetOrgname();

     COrgName::C_Name::E_Choice 
           choc1 = nm1.CanGetName()? 
                     nm1.GetName().Which(): COrgName::C_Name::e_not_set;
     COrgName::C_Name::E_Choice 
           choc2 = nm2.CanGetName()?  
                     nm1.GetName().Which(): COrgName::C_Name::e_not_set;
     int gcode1 = nm1.CanGetGcode()? nm1.GetGcode(): 0;
     int gcode2 = nm2.CanGetGcode()? nm2.GetGcode(): 0;
     int mgcode1 = nm1.CanGetMgcode()? nm1.GetMgcode() : 0;
     int mgcode2 = nm2.CanGetMgcode()? nm2.GetMgcode() : 0;
     string att1 = nm1.CanGetAttrib()? nm1.GetAttrib() : kEmptyStr;
     string att2 = nm2.CanGetAttrib()? nm2.GetAttrib() : kEmptyStr;
     string lin1 = nm1.CanGetLineage()? nm1.GetLineage() : kEmptyStr;
     string lin2 = nm2.CanGetLineage()? nm2.GetLineage() : kEmptyStr;
     string div1 = nm1.CanGetDiv()? nm1.GetDiv() : kEmptyStr;
     string div2 = nm2.CanGetDiv()? nm2.GetDiv() : kEmptyStr;

     if (choc1 != choc2 
             || gcode1 != gcode2 
             || mgcode1 != mgcode2 
             || att1 != att2 
             || lin1 != lin2 
             || div1 != div2 
             || !OrgModSetMatch(nm1, nm2)) {
           return false;
     }
     else {
       return true;
     }
   }
   else { 
       return false;
   }
}; // OrgNameMatch 



bool CSeqEntry_INCONSISTENT_BIOSOURCE :: OrgRefMatch(const COrg_ref& org1, const COrg_ref& org2)
{
  string tax1 = org1.CanGetTaxname()? org1.GetTaxname() : kEmptyStr;
  string tax2 = org2.CanGetTaxname()? org2.GetTaxname() : kEmptyStr;
  string comm1 = org1.CanGetCommon()? org1.GetCommon() : kEmptyStr;
  string comm2 = org2.CanGetCommon()? org2.GetCommon() : kEmptyStr;
 
  if ( tax1 != tax2 || comm1 != comm2) return false;
  else if ( !SynonymsMatch(org1, org2) || !DbtagMatch(org1, org2)) return false;
  else if (!OrgNameMatch(org1, org2)) return false;
  else return true;
};  // OrgRefMatch


bool CSeqEntry_INCONSISTENT_BIOSOURCE :: SubSourceSetMatch(const CBioSource& biosrc1, const CBioSource& biosrc2)
{
  bool has_subtype1= (biosrc1.CanGetSubtype() && !biosrc1.GetSubtype().empty());
  bool has_subtype2= (biosrc2.CanGetSubtype() && !biosrc2.GetSubtype().empty());

  if (!has_subtype1 && !has_subtype2) {
      return true;
  }
  else if (has_subtype1 && has_subtype2) {
    const list < CRef < CSubSource > >& subsrc1 = biosrc1.GetSubtype();
    const list < CRef < CSubSource > >& subsrc2 = biosrc2.GetSubtype();

    if ( (subsrc1.size() != subsrc2.size())) {
        return false;
    }
    list < CRef < CSubSource > >::const_iterator it, jt;
    // already sorted
    for (it = subsrc1.begin(), jt = subsrc2.begin(); 
            it != subsrc1.end(); 
            it++, jt++) { 
         if ( (*it)->GetSubtype() != (*jt)->GetSubtype() 
                || (*it)->GetName() != (*jt)->GetName()  
                || ( (*it)->CanGetAttrib()? (*it)->GetAttrib(): kEmptyStr )
                   != ( (*jt)->CanGetAttrib()? (*jt)->GetAttrib(): kEmptyStr )){
             return false;
         }
     }
     return true;
  }  
  else return false;
}; // SubSourceSetMatch


bool CSeqEntry_INCONSISTENT_BIOSOURCE :: BioSourceMatch( const CBioSource& biosrc1, const CBioSource& biosrc2)
{
  if ( (biosrc1.GetOrigin() != biosrc2.GetOrigin()) 
          || (biosrc1.CanGetIs_focus() != biosrc2.CanGetIs_focus())
          || !OrgRefMatch(biosrc1.GetOrg(), biosrc2.GetOrg()) 
          || !SubSourceSetMatch(biosrc1, biosrc2) ) {
      return false;
  }

  int genome1 = biosrc1.GetGenome();
  int genome2 = biosrc2.GetGenome();
  if (genome1 == genome2) return true;
  if ( (genome1 = CBioSource::eGenome_unknown && genome2 == CBioSource::eGenome_genomic)
        || (genome1 == CBioSource::eGenome_genomic && genome2 == CBioSource::eGenome_unknown) ) {
       return true;
  }
  else {
     return false;
  }
};



string CSeqEntry_INCONSISTENT_BIOSOURCE :: OrgModSetDifferences(const COrgName& nm1, const COrgName& nm2) 
{
  bool has_mod1 = (nm1.CanGetMod() && !nm1.GetMod().empty());
  bool has_mod2 = (nm2.CanGetMod() && !nm2.GetMod().empty());
  string org_mod_diff_str(kEmptyStr);
  
  if (!has_mod1 && !has_mod2) return (kEmptyStr);
  else if (has_mod1 && has_mod2) {
     const list < CRef <COrgMod> >& mod1= nm1.GetMod();
     const list < CRef <COrgMod> >& mod2 = nm2.GetMod();

     string att_i, att_j, qual;
     unsigned sz = min(mod1.size(), mod2.size());
     list < CRef < COrgMod > > :: const_iterator it, jt;
     unsigned i;
     for (i=0, it = mod1.begin(), jt = mod2.begin(); i < sz; i++, it++, jt++) {
       att_i = (*it)->CanGetAttrib()? (*it)->GetAttrib(): kEmptyStr;
       att_j = (*jt)->CanGetAttrib()? (*jt)->GetAttrib(): kEmptyStr;
       qual = (*it)->GetSubtypeName((*it)->GetSubtype(), COrgMod::eVocabulary_insdc);
       if ( (*it)->GetSubtype() != (*jt)->GetSubtype() ) 
          org_mod_diff_str += "Missing " + qual + " modifier, ";
       else if (att_i != att_j) 
          org_mod_diff_str += qual + " modifier attrib values differ, ";
       else if ( (*it)->GetSubname() != (*jt)->GetSubname() )
          org_mod_diff_str += "Different " + qual + " values, ";
     } 
     if (sz < mod1.size()) {
        // pick up one of the remain
        org_mod_diff_str 
           += "Missing " 
              + (*it)->GetSubtypeName((*it)->GetSubtype(), COrgMod::eVocabulary_insdc)
              +" modifier, ";
     }
     else if (sz < mod2.size()) {
        // pick up one of the remain
        org_mod_diff_str 
           += "Missing " 
              + (*jt)->GetSubtypeName((*jt)->GetSubtype(), COrgMod::eVocabulary_insdc)
              + " modifier, ";
     }
  }
  else if (has_mod1) {
     list < CRef < COrgMod > >::const_iterator it = nm1.GetMod().begin();
     org_mod_diff_str 
         += "Missing " 
            + (*it)->GetSubtypeName((*it)->GetSubtype(), COrgMod::eVocabulary_insdc) 
            + " modifier, ";
  }
  else {    // has_mod2
     list < CRef < COrgMod > >::const_iterator it = nm2.GetMod().begin();
     org_mod_diff_str 
       += "Missing " 
           + (*it)->GetSubtypeName((*it)->GetSubtype(), COrgMod::eVocabulary_insdc) 
           + " modifier, ";
  }
  org_mod_diff_str = org_mod_diff_str.substr(0, org_mod_diff_str.size()-2);
  return (org_mod_diff_str);
};




string CSeqEntry_INCONSISTENT_BIOSOURCE :: DescribeOrgNameDifferences(const COrg_ref& org1, const COrg_ref& org2)
{
   bool has_orgnm1 = org1.CanGetOrgname();
   bool has_orgnm2 = org2.CanGetOrgname();

   if (!has_orgnm1 && !has_orgnm2) {
          return(kEmptyStr);
   }
   else if (has_orgnm1 && has_orgnm2) {
     string org_nm_diff_str(kEmptyStr);
     const COrgName& nm1 = org1.GetOrgname();
     const COrgName& nm2 = org2.GetOrgname();

     COrgName::C_Name::E_Choice 
          choc1 = nm1.CanGetName()?
                    nm1.GetName().Which(): COrgName::C_Name :: e_not_set;
     COrgName::C_Name::E_Choice 
          choc2 = nm2.CanGetName()?
                     nm2.GetName().Which(): COrgName::C_Name :: e_not_set;
     int gcode1 = nm1.CanGetGcode()? nm1.GetGcode(): 0;
     int gcode2 = nm2.CanGetGcode()? nm2.GetGcode(): 0;
     int mgcode1 = nm1.CanGetMgcode()? nm1.GetMgcode() : 0;
     int mgcode2 = nm2.CanGetMgcode()? nm2.GetMgcode() : 0;
     string att1 = nm1.CanGetAttrib()? nm1.GetAttrib() : kEmptyStr;
     string att2 = nm2.CanGetAttrib()? nm2.GetAttrib() : kEmptyStr;
     string lin1 = nm1.CanGetLineage()? nm1.GetLineage() : kEmptyStr;
     string lin2 = nm2.CanGetLineage()? nm2.GetLineage() : kEmptyStr;
     string div1 = nm1.CanGetDiv()? nm1.GetDiv() : kEmptyStr;
     string div2 = nm2.CanGetDiv()? nm2.GetDiv() : kEmptyStr;

     if (choc1 != choc2) {
          org_nm_diff_str = "orgname choices differ, ";
     }
     if (gcode1 != gcode2) {
         org_nm_diff_str += "genetic codes differ, ";
     }
     if (mgcode1 != mgcode2) {
          org_nm_diff_str += "mitochondrial genetic codes differ, ";
     }
     if (att1 != att2) {
         org_nm_diff_str += "attributes differ, ";
     }
     if (lin1 != lin2) {
         org_nm_diff_str += "lineages differ, ";
     }
     if (div1 != div2) {
        org_nm_diff_str += "divisions differ, ";
     }
     string tmp = OrgModSetDifferences(nm1, nm2);
     if (!tmp.empty()) {
         org_nm_diff_str += tmp + ", ";
     }
     org_nm_diff_str = org_nm_diff_str.substr(0, org_nm_diff_str.size()-2);
     return (org_nm_diff_str); 
   }
   else {
      return ("One Orgname is missing, ");
   }
}



string CSeqEntry_INCONSISTENT_BIOSOURCE :: DescribeOrgRefDifferences(const COrg_ref& org1, const COrg_ref& org2)
{
  string org_ref_diff_str(kEmptyStr);

  string tax1 = org1.CanGetTaxname()? org1.GetTaxname() : kEmptyStr;
  string tax2 = org2.CanGetTaxname()? org2.GetTaxname() : kEmptyStr;
  string comm1 = org1.CanGetCommon()? org1.GetCommon() : kEmptyStr;
  string comm2 = org2.CanGetCommon()? org2.GetCommon() : kEmptyStr;

  if ( tax1 != tax2 ) {
      org_ref_diff_str = "taxnames differ, ";
  }
  if ( comm1 != comm2 ) {
       org_ref_diff_str += "common names differ, ";
  }
  if ( !SynonymsMatch(org1, org2) ) {
      org_ref_diff_str += "synonyms differ, ";
  }
  if ( !DbtagMatch(org1, org2)) {
      org_ref_diff_str += "dbxrefs differ, ";
  }
  string tmp = DescribeOrgNameDifferences(org1, org2);
  if (!tmp.empty()) {
       org_ref_diff_str += tmp + ", ";
  }
  org_ref_diff_str = org_ref_diff_str.substr(0, org_ref_diff_str.size()-2);
  return (org_ref_diff_str);
}



string CSeqEntry_INCONSISTENT_BIOSOURCE :: DescribeBioSourceDifferences(const CBioSource& biosrc1, const CBioSource& biosrc2)
{
    string diff_str(kEmptyStr);
    if (biosrc1.GetOrigin() != biosrc2.GetOrigin()) {
         diff_str = "origins differ, ";
    }
    if (biosrc1.CanGetIs_focus() != biosrc2.CanGetIs_focus()) {
        diff_str += "focus differs, ";
    }
    int genome1 = biosrc1.GetGenome();
    int genome2 = biosrc2.GetGenome();
    if (genome1 != genome2
        && !(genome1 = CBioSource::eGenome_unknown && genome2 == CBioSource::eGenome_genomic)
        && !(genome1 == CBioSource::eGenome_genomic && genome2 ==CBioSource::eGenome_unknown) ) {
       diff_str += "locations differ, ";
    }
    if (!SubSourceSetMatch(biosrc1,biosrc2)) {
       diff_str += "subsource qualifiers differ, ";
    }
    string tmp = DescribeOrgRefDifferences (biosrc1.GetOrg(), biosrc2.GetOrg());
    if (!tmp.empty()) diff_str += tmp + ", ";
    diff_str = diff_str.substr(0, diff_str.size()-2);
    return (diff_str);
};


// new comb.
void CBioseq_test_on_genprod_set :: TestOnObj(const CBioseq& bioseq)
{
   if (thisTest.is_GP_Set_run) return;
   thisTest.is_GP_Set_run = true;

   if (!bioseq.IsNa()) {thisTest.is_GP_Set_run = true; return; }
   CConstRef <CBioseq_set> set = bioseq.GetParentSet();
   if (set.Empty()) { thisTest.is_GP_Set_run = true; return;}

   bool run_mprot = (thisTest.tests_run.find(GetName_mprot()) != end_it);
   bool run_dprot = (thisTest.tests_run.find(GetName_dprot()) != end_it);
   bool run_mtid = (thisTest.tests_run.find(GetName_mtid()) != end_it);
   bool run_dtid = (thisTest.tests_run.find(GetName_dtid()) != end_it);

   string prod_id, desc;
   /* look for missing protein IDs and duplicate protein IDs on coding regions */
   if (run_mprot || run_dprot) { // MISSING_GENPRODSET_PROTEIN || DUP_GENPRODSET_PROTEIN
     ITERATE (vector <const CSeq_feat*>, it, cd_feat) {
       desc = GetDiscItemText(**it);
       CConstRef <CObject> feat_ref (*it);
       if (!(*it)->CanGetProduct()) {
          if (run_mprot && (!(*it)->CanGetPseudo() || !(*it)->GetPseudo()))
            thisInfo.test_item_list[GetName_mprot()].push_back(desc);
            thisInfo.test_item_objs[GetName_mprot()].push_back(feat_ref);
       }
       else if (run_dprot) {
          const CSeq_id& seq_id = sequence::GetId((*it)->GetProduct(), thisInfo.scope); 
          seq_id.GetLabel(&prod_id); 
          thisInfo.test_item_list[GetName_dprot()].push_back(prod_id + "$" + desc);
       }
     }   
   }

   /* look for missing transcript IDs and duplicate transcript IDs on mRNAs */
   if (run_mtid || run_dtid) {
     ITERATE (vector <const CSeq_feat*>, it, mrna_feat) {
       desc = GetDiscItemText(**it);
       if (!(*it)->CanGetProduct()) {
          if ( run_mtid && (!(*it)->CanGetPseudo() || !(*it)->GetPseudo()))
            thisInfo.test_item_list[GetName_mtid()].push_back(desc);
       }
       else if (run_dtid) {
          const CSeq_id& seq_id = sequence::GetId((*it)->GetProduct(), thisInfo.scope);
          seq_id.GetLabel(&prod_id); 
          thisInfo.test_item_list[GetName_dtid()].push_back(prod_id + "$" + desc);
       }
     }
   }
};


void CBioseq_test_on_genprod_set :: GetReport_dup(CRef <CClickableItem>& c_item, const string& setting_name, const string& desc1, const string& desc2, const string& desc3)
{
   Str2Strs label2feats;
   GetTestItemList(c_item->item_list, label2feats);
   c_item->item_list.clear();
   unsigned cnt;
   if (label2feats.size() == 1) {
       cnt = (label2feats.begin()->second).size(); 
       if (cnt > 1) {
          c_item->item_list = label2feats.begin()->second;
          c_item->description 
             = GetHasComment(cnt, desc1) + desc2 + label2feats.begin()->first;
       }
   }
   else {
      ITERATE (Str2Strs, it, label2feats) {
         if (it->second.size() > 1) {
            AddSubcategory(c_item, setting_name, &(it->second), desc1,
                              desc2 + it->first, e_HasComment); 
         } 
      }
      if (c_item->subcategories.size()> 1)
         c_item->description = GetHasComment(c_item->item_list.size(), desc1) + desc3;
   };
};


void CBioseq_MISSING_GENPRODSET_PROTEIN :: GetReport(CRef <CClickableItem>& c_item)
{
   c_item->obj_list = thisInfo.test_item_objs[GetName()];
   unsigned cnt = c_item->item_list.size();
   c_item->description = GetHasComment(cnt, "coding region") 
                           + "missing protein " + ((cnt > 1) ? "IDs." : "ID.");
};


void CBioseq_DUP_GENPRODSET_PROTEIN :: GetReport(CRef <CClickableItem>& c_item)
{
   GetReport_dup(c_item, GetName(), "coding region", "protein ID ", 
                                           "non-unique protein IDs.");
};


void CBioseq_MISSING_GENPRODSET_TRANSCRIPT_ID :: GetReport(CRef <CClickableItem>& c_item)
{
   unsigned cnt = c_item->item_list.size();
   c_item->description = GetHasComment(cnt, "mRNA") 
                           + "missing transcript " + ((cnt > 1) ? "IDs." : "ID.");
};


void CBioseq_DISC_DUP_GENPRODSET_TRANSCRIPT_ID :: GetReport(CRef <CClickableItem>& c_item)
{
   GetReport_dup(c_item, GetName(), "mRNA", "non-unique transcript ID ", 
                                              "non-unique transcript IDs.");
};


void CBioseq_test_on_prot :: TestOnObj(const CBioseq& bioseq)
{
   if (thisTest.is_Prot_run) return;
   thisTest.is_Prot_run = true;

   string desc(GetDiscItemText(bioseq));
   CConstRef <CObject> seq_ref(&bioseq);

   // COUNT_PROTEINS
   if (thisTest.tests_run.find(GetName_cnt()) != end_it) {
       thisInfo.test_item_list[GetName_cnt()].push_back(desc); 
       thisInfo.test_item_objs[GetName_cnt()].push_back(seq_ref);
   }

   // INCONSISTENT_PROTEIN_ID, MISSING_PROTEIN_ID
   bool run_prefix = (thisTest.tests_run.find(GetName_prefix()) != end_it);
   bool run_id = (thisTest.tests_run.find(GetName_id()) != end_it);
   bool has_pid = false;
   if (run_prefix || run_id) {
     ITERATE (list <CRef <CSeq_id> >, it, bioseq.GetId()) {
       if ((*it)->IsGeneral()) {
         const CDbtag& dbtag = (*it)->GetGeneral();
         if (!dbtag.IsSkippable()) {
            has_pid = true;
            strtmp = dbtag.GetDb();
            if (run_prefix) {
               if (!strtmp.empty()) {
                  thisInfo.test_item_list[GetName_prefix()].push_back(strtmp + "$" + desc);
                  break;
               }
            }
            else break;
         }
       }
     }

     // MISSING_PROTEIN_ID
     if (!has_pid && run_id) {
          thisInfo.test_item_list[GetName_id()].push_back(desc);
          thisInfo.test_item_objs[GetName_id()].push_back(seq_ref);
     }
   }
};


void CBioseq_COUNT_PROTEINS :: GetReport(CRef <CClickableItem>& c_item)
{
   c_item->description 
     = NStr::UIntToString((unsigned)c_item->item_list.size()) + " protein sequences in record";
   c_item->obj_list = thisInfo.test_item_objs[GetName()];
};


void CBioseq_MISSING_PROTEIN_ID:: GetReport(CRef <CClickableItem>& c_item) 
{
  c_item->obj_list = thisInfo.test_item_objs[GetName()];
  unsigned cnt = c_item->item_list.size();
  c_item->description = GetHasComment(cnt, "protein")
                         + "invalid " + ( (cnt > 1)? "IDs." : "ID." );
};


void CBioseq_INCONSISTENT_PROTEIN_ID :: MakeRep(const Str2Strs& item_map, const string& desc1, const string& desc2)
{
   string setting_name = GetName();
   ITERATE (Str2Strs, it, item_map) {
     if (it->second.size() > 1) {
       CRef <CClickableItem> c_item(new CClickableItem);
       c_item->setting_name = setting_name;
       c_item->item_list = it->second;
       c_item->description = GetHasComment(c_item->item_list.size(), desc1) 
                             + desc2 + it->first +".";
       thisInfo.disc_report_data.push_back(c_item);
     }
   }
};


void CBioseq_INCONSISTENT_PROTEIN_ID :: GetReport(CRef <CClickableItem>& c_item)
{
   Str2Strs db2list, prefix2list;
   GetTestItemList(c_item->item_list, db2list);

   GetTestItemList(c_item->item_list, prefix2list);
   c_item->item_list.clear();

   // ReportInconsistentGlobalDiscrepancyStrings
   if (db2list.size() > 1) {
       MakeRep(db2list, "sequence", "protein ID prefix ");
   }

   // ReportInconsistentGlobalDiscrepancyPrefixes
   if (prefix2list.size() > 1) {
       MakeRep(prefix2list, "feature", "locus tag prefix ");
   }
};



void CBioseq_test_on_cd_4_transl :: TranslExceptOfCDs (const CSeq_feat& cd, bool& has_transl, bool& too_long)
{
  has_transl = too_long = false;
  unsigned len, tmp_len;
  tmp_len = len = sequence::GetCoverage(cd.GetLocation(), thisInfo.scope);
  switch ( cd.GetData().GetCdregion().GetFrame() ) {
     case CCdregion::eFrame_two: tmp_len -=1; break;
     case CCdregion::eFrame_three: tmp_len -= 2; break;
     default: break;
  }

  unsigned codon_start, codon_stop, pos, i, codon_len;
  ITERATE (list <CRef <CCode_break> >, it, cd.GetData().GetCdregion().GetCode_break()) {
    if ( !((*it)->GetAa().IsNcbieaa()) || (*it)->GetAa().GetNcbieaa() != 42) {
         continue;
    }
    if (!too_long 
           && sequence::GetCoverage((*it)->GetLoc(), thisInfo.scope) > 3) {
         too_long = true;
    }

    if (!has_transl && (tmp_len % 3)) {
       i=0;
       for (CSeq_loc_CI loc_ci( (*it)->GetLoc() ); loc_ci && !has_transl; ++loc_ci, i++) {
          pos = sequence::LocationOffset(cd.GetLocation(), 
                                         loc_ci.GetEmbeddingSeq_loc(),
                                         sequence::eOffset_FromLeft, 
                                         thisInfo.scope);
          
          if ( (int)pos > -1 &&  (!i || pos <= codon_start)) {
            codon_start = pos;
            codon_len = loc_ci.GetRange().GetLength() - 1;// codon_len=codon_stop - codon_start
            codon_stop = codon_len + codon_start;
            if (codon_len >= 0 && codon_len <= 1 && codon_stop == len - 1) { 
               has_transl = true;
            }
          } 
       }
    }
    
    if (too_long && has_transl) return;
  }
};


void CBioseq_test_on_cd_4_transl :: TestOnObj(const CBioseq& bioseq)
{ 
   if (thisTest.is_CdTransl_run) return;
   thisTest.is_CdTransl_run = true;
  
   string desc, comment;
   string note_txt("TAA stop codon is completed by the addition of 3' A residues to the mRNA");
   bool has_transl, too_long;

   bool run_note = (thisTest.tests_run.find(GetName_note()) != end_it);
   bool run_transl = (thisTest.tests_run.find(GetName_transl()) != end_it);
   bool run_long = (thisTest.tests_run.find(GetName_long()) != end_it);
   ITERATE (vector <const CSeq_feat*>, it, cd_feat) {
     desc = GetDiscItemText(**it); 
     comment = (*it)->CanGetComment()? (*it)->GetComment() : kEmptyStr;
     if ( (*it)->GetData().GetCdregion().CanGetCode_break() ) {
        TranslExceptOfCDs(**it, has_transl, too_long); 
        if (has_transl) {
            if (run_note && comment.find(note_txt.substr(0, 3)) == string::npos) 
                 thisInfo.test_item_list[GetName_note()].push_back(desc);
        }
        else if (run_transl && comment.find(note_txt) != string::npos)
                    thisInfo.test_item_list[GetName_transl()].push_back(desc);
        if (run_long && too_long) thisInfo.test_item_list[GetName_long()].push_back(desc);
     }
     else if (run_transl && comment.find(note_txt) != string::npos)
                    thisInfo.test_item_list[GetName_transl()].push_back(desc);
   }  
};

void CBioseq_TRANSL_NO_NOTE :: GetReport(CRef <CClickableItem>& c_item)
{
   c_item->description = GetHasComment(c_item->item_list.size(), "feature") 
                             + "a translation exception but no note";
};

void CBioseq_NOTE_NO_TRANSL :: GetReport(CRef <CClickableItem>& c_item)
{
   c_item->description = GetHasComment(c_item->item_list.size(), "feature")
                            + "a note but not translation exception";
};

void CBioseq_TRANSL_TOO_LONG :: GetReport(CRef <CClickableItem>& c_item)
{
   c_item->description = GetHasComment(c_item->item_list.size(), "feature")
                             + "translation exceptions longer than 3 bp";
};


void CBioseq_test_on_rna :: FindMissingRNAsInList()
{
  vector <unsigned>  num_present;
  Str2Strs aa_seqfeat;
  num_present.reserve(thisInfo.desired_aaList.size());
  for (unsigned i=0; i< thisInfo.desired_aaList.size(); i++) {
          num_present.push_back(0);
  }

  string context_label;
  unsigned i; 
  ITERATE (vector <const CSeq_feat*>, it, trna_feat) {
     GetSeqFeatLabel(**it, context_label);
     i=0;
     ITERATE (Str2UInt, jt, thisInfo.desired_aaList) {
        if (context_label.find(jt->first) != string::npos) {
              num_present[i]++;
              aa_seqfeat[jt->first].push_back(GetDiscItemText(**it));
              break;
        }
        i++;
     } 
  }

  i=0; 
  ITERATE (Str2UInt, it, thisInfo.desired_aaList) {
    if (num_present[i] < it->second)  {
       //   it->first + "_missing$" + m_bioseq_desc);
       strtmp = "Sequence " + m_best_id_str + " is missing trna-" + it->first + "$" + m_bioseq_desc;
       thisInfo.test_item_list[GetName_tcnt()].push_back(strtmp);
    }
    else if (num_present[i] > it->second) {
       strtmp = "Sequence " + m_best_id_str + " has " + NStr::UIntToString(num_present[i]) 
              + " trna-" + it->first + (num_present[i]==1 ? " feature." : " features.") + "$";
       thisInfo.test_item_list[GetName_tcnt()].push_back(
                                               strtmp + m_bioseq_desc);
       ITERATE (vector <string>, jt, aa_seqfeat[it->first]) {
           thisInfo.test_item_list[GetName_tcnt()].push_back(strtmp + *jt);
       }
    }
    i++;    
  }
};


bool CBioseq_test_on_rna :: RRnaMatch(const CRNA_ref& rna1, const CRNA_ref& rna2)
{
  if (!rna1.CanGetExt() && !rna2.CanGetExt()) {
        return true;
  }
  else if (rna1.CanGetExt() && rna2.CanGetExt()) {
    const CRNA_ref::C_Ext& ext1 = rna1.GetExt();
    const CRNA_ref::C_Ext& ext2 = rna2.GetExt();
    if (ext1.Which() != ext2.Which()) {
       return false;
    }
    else {
      if (ext1.IsName()) {
          if (ext1.GetName() == ext2.GetName()) {
              return true;
          }
          else {
             return false;
          }
      }
      else if (ext1.IsTRNA()) {
         const CTrna_ext& trna1 = ext1.GetTRNA();
         const CTrna_ext& trna2 = ext2.GetTRNA();
         bool has_aa1 = trna1.CanGetAa();
         bool has_aa2 = trna2.CanGetAa();
         if (has_aa1 != has_aa2) {
             return false;
         }
         else {
            const CTrna_ext::C_Aa& aa1 = trna1.GetAa();
            const CTrna_ext::C_Aa& aa2 = trna2.GetAa();
            if (aa1.Which() != aa2.Which()) {
                  return false;
            }
            else {
               int iaa1, iaa2; 
               if (aa1.IsIupacaa()) {
                   iaa1 = aa1.GetIupacaa();
                   iaa2 = aa2.GetIupacaa();
               }
               else if (aa1.IsNcbieaa()) {
                   iaa1 = aa1.GetNcbieaa();
                   iaa2 = aa2.GetNcbieaa();
               }
               else if (aa1.IsNcbi8aa()) {
                   iaa1 = aa1.GetNcbi8aa();
                   iaa2 = aa2.GetNcbi8aa();
               }
               else if (aa1.IsNcbistdaa()) {
                   iaa1 = aa1.GetNcbistdaa();
                   iaa2 = aa2.GetNcbistdaa();
               }
               else {
                  return true;
               }
               if (iaa1 == iaa2) {
                   return true;
               }
               else {
                  return false;
               }
            }
         }
      }
      else if (ext1.IsGen()) {
        const CRNA_gen& gen1 = ext1.GetGen();
        const CRNA_gen& gen2 = ext2.GetGen();
        string class1 = gen1.CanGetClass() ? gen1.GetClass() : kEmptyStr;
        string class2 = gen2.CanGetClass() ? gen2.GetClass() : kEmptyStr;
        if (class1 != class2) {
             return false;
        }
        string prod1 = gen1.CanGetProduct() ? gen1.GetProduct() : kEmptyStr;
        string prod2 = gen2.CanGetProduct() ? gen2.GetProduct() : kEmptyStr;
        if (prod1 != prod2) {
               return false;
        }
        bool has_quals1 =(gen1.CanGetQuals() && !gen1.GetQuals().Get().empty());
        bool has_quals2 =(gen2.CanGetQuals() && !gen2.GetQuals().Get().empty());
        if (has_quals1 != has_quals2) {
              return false;
        }
        else if (has_quals1) { 
          unsigned sz1 = gen1.GetQuals().Get().size();
          unsigned sz2 = gen2.GetQuals().Get().size(); 
          if (sz1 != sz2) return false;
          else {
             Str2Str qual1_vlu;
             ITERATE (list <CRef <CRNA_qual> >, it, gen1.GetQuals().Get()) {
                qual1_vlu[(*it)->GetQual()] = (*it)->GetVal();
             }
             ITERATE (list <CRef <CRNA_qual> >, it, gen2.GetQuals().Get()) {
                if (qual1_vlu.find((*it)->GetQual()) == qual1_vlu.end()) {
                     return false;
                }
                else if (qual1_vlu[(*it)->GetQual()] != (*it)->GetVal()) {
                   return false;
                }
             }
             return true;
          } 
        }
        else return true;
      }
      else return true;  // e_not_set
    }   
  }
  else return false;

};

void CBioseq_test_on_rna :: FindDupRNAsInList()
{
  unsigned i, j;
  vector <unsigned> is_dup;
  is_dup.reserve(rrna_feat.size());
  for (i=0; i< rrna_feat.size(); i++) is_dup.push_back(0);
  Str2Strs label2dup_rrna;
  string label;

  for (i=0; (int) i < (int)rrna_feat.size() -1; i++) {
    if (is_dup[i]) continue;
    GetSeqFeatLabel(*rrna_feat[i], label);  // context_label in C
    label2dup_rrna[label].push_back(GetDiscItemText(*rrna_feat[i]));
    is_dup[i] = 1;
    for (j=i+1; j< rrna_feat.size(); j++) {
      if (!is_dup[j] 
           && RRnaMatch(rrna_feat[i]->GetData().GetRna(), rrna_feat[j]->GetData().GetRna())) {
         label2dup_rrna[label].push_back(GetDiscItemText(*rrna_feat[j]));
         is_dup[j] = 1;
      }      
    }
  }

  unsigned cnt;
  ITERATE (Str2Strs, it, label2dup_rrna) 
    ITERATE (vector <string>, jt, it->second) {
      cnt = it->second.size();
      if (cnt > 1) {
         strtmp = NStr::UIntToString(cnt) + " rRNA features on " + m_best_id_str 
                    + " have the same name (" + it->first + ").";
         thisInfo.test_item_list[GetName_rdup()].push_back(strtmp + "$" + *jt);
      }
    }
};


void CBioseq_test_on_rna :: FindtRNAsOnSameStrand()
{
   ENa_strand str0 = trna_feat[0]->GetLocation().GetStrand();
   ENa_strand this_str; 
   vector <string> trnas_same_str;
   vector <CConstRef <CObject> > same_str_objs;
   bool mixed_strand = false;

   for (unsigned i=1; !mixed_strand && i< trna_feat.size(); i++) {
      this_str = trna_feat[i]->GetLocation().GetStrand();
      if ((str0 == eNa_strand_minus && this_str != eNa_strand_minus)
          || (str0 != eNa_strand_minus && this_str == eNa_strand_minus)) {
          mixed_strand = true;
      }
      else {
         trnas_same_str.push_back(GetDiscItemText(*trna_feat[i]));
         same_str_objs.push_back(CConstRef <CObject>(trna_feat[i]));   
      }
   }
  
   string setting_name = GetName_strand();
   if (!mixed_strand) {
     string str_tp = (str0 == eNa_strand_minus) ? "minus" : "plus";
     thisInfo.test_item_list[setting_name].push_back(
                            str_tp + "$" + GetDiscItemText( *trna_feat[0]));
     thisInfo.test_item_objs[setting_name].push_back(CConstRef <CObject>(trna_feat[0]));
     copy(trnas_same_str.begin(), trnas_same_str.end(),
          back_inserter(thisInfo.test_item_list[setting_name]));
     copy(same_str_objs.begin(), same_str_objs.end(),
          back_inserter(thisInfo.test_item_objs[setting_name]));
   }
};


void CBioseq_test_on_rna :: TestOnObj(const CBioseq& bioseq)
{
  if (thisTest.is_TRna_run) return;
  thisTest.is_TRna_run = true;
 
  // FIND_BADLEN_TRNAS
  unsigned len;
  string trna_desc;
  if (thisTest.tests_run.find(GetName_len()) != end_it) {
    ITERATE (vector <const CSeq_feat*>, it, trna_feat) {
       len = sequence::GetCoverage((*it)->GetLocation(), thisInfo.scope);
       trna_desc = GetDiscItemText(**it); 
       CConstRef <CObject> trna_ref (*it);
       if ( len > 90) {
          thisInfo.test_item_list[GetName_len()].push_back("long$" + trna_desc);
          thisInfo.test_item_objs[GetName_len() + "$long"].push_back(trna_ref);
       }
       else if (len < 50) {
         if ( (*it)->CanGetPartial() && !(*it)->GetPartial()) {
           thisInfo.test_item_list[GetName_len()].push_back("short$"+trna_desc);
           thisInfo.test_item_objs[GetName_len() +"$short"].push_back(trna_ref);
         }
       }
    }
  }

  // TEST_SHORT_LNCRNA
  bool partial5, partial3;
  if (thisTest.tests_run.find(GetName_lnc()) != end_it) {
    ITERATE (vector <const CSeq_feat*>, it, rna_feat) {
       if ( (*it)->GetData().GetSubtype() != CSeqFeatData::eSubtype_ncRNA ) {
            continue;
       }
       const CRNA_ref& rna_ref = (*it)->GetData().GetRna();
       if (!rna_ref.CanGetExt() 
              || !rna_ref.GetExt().IsGen()
              || !rna_ref.GetExt().GetGen().CanGetClass()
              || !NStr::EqualNocase(rna_ref.GetExt().GetGen().GetClass(), 
                                    "lncrna")) {
         continue;
       }
       partial5 = (*it)->GetLocation().IsPartialStart(eExtreme_Biological);
       partial3 = (*it)->GetLocation().IsPartialStop(eExtreme_Biological);
       if (!partial5 
              && !partial3
              && sequence::GetCoverage((*it)->GetLocation(), thisInfo.scope) < 200) {  
            thisInfo.test_item_list[GetName_lnc()]
                        .push_back(GetDiscItemText(**it));
            thisInfo.test_item_objs[GetName_lnc()]
                        .push_back(CConstRef <CObject>(*it));
       }
    }
  }

  // below covers COUNT_TRNAS, COUNT_RRNAS, FIND_DUP_RRNAS, FIND_STRAND_TRNAS
  // FIND_DUP_TRNAS: launches COUNT_TRNAS
  if (bioseq.IsAa()) return;
  bool run_tcnt = (thisTest.tests_run.find(GetName_tcnt()) != end_it);
  bool run_rcnt = (thisTest.tests_run.find(GetName_rcnt()) != end_it);
  bool run_rdup = (thisTest.tests_run.find(GetName_rdup()) != end_it);
  bool run_tdup = (thisTest.tests_run.find(GetName_tdup()) != end_it);
  bool run_strand = (thisTest.tests_run.find(GetName_strand()) != end_it);
  if (!run_tcnt && !run_rcnt && !run_rdup && !run_tdup && run_strand) return;

  m_best_id_str = BioseqToBestSeqIdString(bioseq, CSeq_id::e_Genbank);
  m_bioseq_desc = GetDiscItemText(bioseq);

  bool run_test = false;
  int src_genome;
  const CBioSource* 
     biosrc = sequence::GetBioSource(thisInfo.scope->GetBioseqHandle(bioseq));
  if (biosrc) {
    src_genome = biosrc->GetGenome(); 
    if ( src_genome == CBioSource ::eGenome_plastid
           || src_genome == CBioSource ::eGenome_mitochondrion
           || src_genome == CBioSource ::eGenome_chloroplast ) {
        run_test = true;
    }
  }

  string label;
  unsigned cnt;
  if (run_test) {
    cnt = trna_feat.size();
    if (cnt) {
       if (run_tcnt || run_tdup) {
          thisInfo.test_item_list[GetName_tcnt()].push_back(
                                NStr::UIntToString(cnt) + "$" + m_bioseq_desc);
          FindMissingRNAsInList();
       }
       if (run_strand) FindtRNAsOnSameStrand(); // FIND_STRAND_TRNAS
    }

    cnt = rrna_feat.size();
    if (cnt) {
       if (run_rcnt) 
             thisInfo.test_item_list[GetName_rcnt()].push_back(
                                NStr::UIntToString(cnt) + "$" + m_bioseq_desc);
       if (run_rdup) FindDupRNAsInList();
    }
  }
};

void CBioseq_TEST_SHORT_LNCRNA :: GetReport(CRef <CClickableItem>& c_item)
{
   c_item->obj_list = thisInfo.test_item_objs[GetName()];
   c_item->description 
       = GetIsComment(c_item->item_list.size(), "feature") + "suspiciously short";
};

void CBioseq_FIND_STRAND_TRNAS :: GetReport(CRef <CClickableItem>& c_item)
{
  size_t pos = c_item->item_list[0].find("$");
  strtmp = c_item->item_list[0].substr(0, pos);
  c_item->item_list[0] = CTempString(c_item->item_list[0]).substr(pos+1);
  c_item->obj_list = thisInfo.test_item_objs[GetName()];
  c_item->description = NStr::UIntToString((unsigned)c_item->item_list.size()) 
                         + " tRNAs on " + strtmp + " strand.";
};


void CBioseq_FIND_BADLEN_TRNAS :: GetReport(CRef <CClickableItem>& c_item)
{
   Str2Strs len2trnas;
   GetTestItemList(c_item->item_list, len2trnas);
   c_item->item_list.clear();
   ITERATE (Str2Strs, it, len2trnas) {
     if (it != len2trnas.begin()) {
        c_item.Reset(new CClickableItem);
        c_item->setting_name = GetName();
        thisInfo.disc_report_data.push_back(c_item);
     }
     c_item->item_list = it->second;
     c_item->obj_list = thisInfo.test_item_objs[GetName() + "$" + it->first];
     c_item->description 
        = GetIsComment(c_item->item_list.size(), "tRNAs") + "too " + it->first;
   }
};


void CBioseq_FIND_DUP_RRNAS :: GetReport(CRef <CClickableItem>& c_item)
{
  Str2Strs label2rrna;
  GetTestItemList(c_item->item_list, label2rrna);
  c_item->item_list.clear();
  ITERATE (Str2Strs, it, label2rrna) {
    if (it != label2rrna.begin()) {
         c_item.Reset(new CClickableItem);
         c_item->setting_name = GetName();
         thisInfo.disc_report_data.push_back(c_item);
    }
    c_item->item_list = it->second;
    c_item->description = it->first;
  }
};


void CBioseq_test_on_rna :: GetReport_trna(CRef <CClickableItem>& c_item)
{
   Str2Strs cnt2bioseq;
   GetTestItemList(c_item->item_list, cnt2bioseq);
   c_item->item_list.size();
   ITERATE (Str2Strs, it, cnt2bioseq) {
     if (it != cnt2bioseq.begin()) {
         c_item.Reset(new CClickableItem);
         c_item->setting_name = GetName_tcnt(); 
         thisInfo.disc_report_data.push_back(c_item);
     }
     c_item->item_list = it->second;
     if (isInt(it->first)) {
         c_item->description = GetHasComment(c_item->item_list.size(), "sequence") + it->first 
                               + " tRNA " + ((it->first== "1")? "feature." : "features.");
     }
     else c_item->description = it->first;
   } 
};
void CBioseq_COUNT_TRNAS :: GetReport(CRef <CClickableItem>& c_item)
{
    GetReport_trna(c_item);
};
void CBioseq_FIND_DUP_TRNAS :: GetReport(CRef <CClickableItem>& c_item)
{
    GetReport_trna(c_item);
};



void CBioseq_COUNT_RRNAS :: GetReport(CRef <CClickableItem>& c_item)
{
   Str2Strs cnt2bioseq;
   GetTestItemList(c_item->item_list, cnt2bioseq);
   c_item->item_list.size();
   ITERATE (Str2Strs, it, cnt2bioseq) {
     if (it != cnt2bioseq.begin()) {
         c_item.Reset(new CClickableItem);
         c_item->setting_name = GetName();
         thisInfo.disc_report_data.push_back(c_item);
     }
     c_item->item_list = it->second;
     if (isInt(it->first)) {
         c_item->description = GetHasComment(c_item->item_list.size(), "sequence") + it->first
                               + " rRNA " + ((it->first== "1")? "feature." : "features.");
     }
     else c_item->description = it->first;
   }
};


void CBioseq_test_on_molinfo :: TestOnObj(const CBioseq& bioseq)
{
   if (thisTest.is_MolInfo_run) return;
   thisTest.is_MolInfo_run = true;

   const CMolInfo* 
        molinfo = sequence::GetMolInfo(thisInfo.scope->GetBioseqHandle(bioseq));
   if (thisTest.tests_run.find(GetName_part()) != end_it) {
      if (molinfo) {
         // PARTIAL_CDS_COMPLETE_SEQUENCE
         if (molinfo->GetCompleteness() == CMolInfo::eCompleteness_complete) {
            ITERATE (vector <const CSeq_feat*>, jt, cd_feat) {
              const CSeq_loc& seq_loc = (*jt)->GetLocation();
              if ( ((*jt)->CanGetPartial() && (*jt)->GetPartial())
                     || seq_loc.IsPartialStart(eExtreme_Biological)
                     || seq_loc.IsPartialStop(eExtreme_Biological) ) {
                 thisInfo.test_item_list[GetName_part()]
                            .push_back(GetDiscItemText(**jt));
                 thisInfo.test_item_objs[GetName_part()]
                            .push_back(CConstRef <CObject>(*jt));
              }
            }
         }
      }
   }

   bool run_mrna = (thisTest.tests_run.find(GetName_mrna()) != end_it);
   bool run_tsa = (thisTest.tests_run.find(GetName_tsa()) != end_it);
   bool run_link = (thisTest.tests_run.find(GetName_link()) != end_it);

   string desc(GetDiscItemText(bioseq));
   CConstRef <CObject> seq_ref(&bioseq);
   if (bioseq.IsNa()) {
     if (run_mrna || run_tsa) {
        if (molinfo) {
          // MOLTYPE_NOT_MRNA
          if (run_mrna && (molinfo->GetBiomol() != CMolInfo :: eBiomol_mRNA)) {
              thisInfo.test_item_list[GetName_mrna()].push_back(desc);
              thisInfo.test_item_objs[GetName_mrna()].push_back(seq_ref);
          }

          // TECHNIQUE_NOT_TSA
          if (run_tsa && molinfo->GetTech() != CMolInfo :: eTech_tsa) {
              thisInfo.test_item_list[GetName_tsa()].push_back(desc);
              thisInfo.test_item_objs[GetName_tsa()].push_back(seq_ref);
          }
        }
     }
   }
   else if (run_link && bioseq.GetInst().GetMol() == CSeq_inst::eMol_rna) {
      // DISC_POSSIBLE_LINKER
      if (!IsMrnaSequence(bioseq) 
              || (bioseq.IsSetLength() && bioseq.GetLength() < 30)){
           return;
      }
      else {
        CSeqVector 
            seq_vec = thisInfo.scope->GetBioseqHandle(bioseq).GetSeqVector(
                                CBioseq_Handle::eCoding_Iupac, eNa_strand_plus);
        unsigned tail_len = 0;
        bool found_linker = false;
        for (CSeqVector_CI seq_ci(seq_vec.end());  
                !found_linker && (seq_ci > seq_vec.end() - 30); 
                -- seq_ci) {
           if (*seq_ci == 'A') {
                  tail_len++;
           }
           else if (tail_len > 20) {
                 found_linker = true;
           }
           else {
              tail_len = 0;
           }
        }
        if (!found_linker) {
           thisInfo.test_item_list[GetName_link()].push_back(desc);
           thisInfo.test_item_objs[GetName_link()].push_back(seq_ref);
        }
      }
   }
};


void CBioseq_DISC_POSSIBLE_LINKER :: GetReport(CRef <CClickableItem>& c_item)
{ 
  c_item->obj_list = thisInfo.test_item_objs[GetName()];
  c_item->description = GetHasComment(c_item->item_list.size(), "bioseq") 
                         + "linker sequence after the poly-A tail";
};


void CBioseq_PARTIAL_CDS_COMPLETE_SEQUENCE :: GetReport(CRef <CClickableItem>& c_item)
{
   c_item->obj_list = thisInfo.test_item_objs[GetName()];
   unsigned cnt = c_item->item_list.size();
   c_item->description 
      = GetIsComment(cnt, "partial CDS") + "in complete " + GetNoun(cnt, "sequence");
};


void CBioseq_MOLTYPE_NOT_MRNA :: GetReport(CRef <CClickableItem>& c_item)
{
   c_item->obj_list = thisInfo.test_item_objs[GetName()];
   c_item->description
      = GetIsComment(c_item->item_list.size(), "molecule type") + "not set as mRNA.";
};


void CBioseq_TECHNIQUE_NOT_TSA :: GetReport(CRef <CClickableItem>& c_item)
{
   c_item->obj_list = thisInfo.test_item_objs[GetName()];
   c_item->description
      = GetIsComment(c_item->item_list.size(), "technique") + "not set as TSA.";
};


void CBioseq_test_on_all_annot :: TestOnObj(const CBioseq& bioseq)
{
  if (thisTest.is_AllAnnot_run) return;
  thisTest.is_AllAnnot_run = true;
  
  bool excpt;
  string excpt_txt(kEmptyStr);
  string desc( GetDiscItemText(bioseq));

  if (all_feat.empty()) {
    CConstRef <CObject> bsq_ref (&bioseq);
    if (thisTest.tests_run.find(GetName_no()) != end_it) {
       // NO_ANNOTATION
       thisInfo.test_item_list[GetName_no()].push_back(desc);
       thisInfo.test_item_objs[GetName_no()].push_back(bsq_ref);
    }
    if (bioseq.IsNa() && bioseq.GetLength() >= 5000
           && (thisTest.tests_run.find(GetName_long_no()) != end_it) ) {
      // DISC_LONG_NO_ANNOTATION
      thisInfo.test_item_list[GetName_long_no()].push_back(desc);
      thisInfo.test_item_objs[GetName_long_no()].push_back(bsq_ref);
    }
  }
  else {
     string label, desc;
     bool run_ls = (thisTest.tests_run.find(GetName_ls()) != end_it);
     bool run_loc = (thisTest.tests_run.find(GetName_loc()) != end_it);
     bool run_std = (thisTest.tests_run.find(GetName_std()) != end_it);
     ITERATE (vector <const CSeq_feat*>, it, all_feat) {
        CConstRef <CObject> feat_ref (*it);
        desc = GetDiscItemText(**it);
        // DISC_FEATURE_LIST
        // CD replaces Prot
        if (run_ls) {
           if ((*it)->GetData().IsProt() 
                  ||(*it)->GetData().GetSubtype() ==CSeqFeatData::eSubtype_gap){
               continue; 
           }
           label = (*it)->GetData().GetKey();
           label = label.empty()? "unknown" : label;
           thisInfo.test_item_list[GetName_ls()].push_back(label + "$" + desc);
           thisInfo.test_item_objs[GetName_ls()+ "$"+label].push_back(feat_ref);
        }

        // ONCALLER_ORDERED_LOCATION
        if (run_loc && LocationHasNullsBetween(*it)) {
           thisInfo.test_item_list[GetName_loc()].push_back(desc);
           thisInfo.test_item_objs[GetName_loc()].push_back(feat_ref);
        }

        // ONCALLER_HAS_STANDARD_NAME
        if (run_std && (*it)->CanGetQual()) {
          ITERATE (vector <CRef< CGb_qual > >, jt, (*it)->GetQual()) {
            if ( (*jt)->GetQual() == "standard_name" ) {
               thisInfo.test_item_list[GetName_std()].push_back(desc);
               thisInfo.test_item_objs[GetName_std()].push_back(feat_ref);
            }
          }
        }
     }
   
    // JOINED_FEATURES
    if (thisTest.tests_run.find(GetName_joined()) == end_it
             || IsBioseqHasLineage(bioseq, "Eukaryota", false)) {
         return;
    }
    ITERATE (vector <const CSeq_feat*>, it, all_feat) {
       if ((*it)->GetLocation().IsMix() || (*it)->GetLocation().IsPacked_int()){
          if ((*it)->CanGetExcept()) {
              excpt = (*it)->GetExcept();
              if ((*it)->CanGetExcept_text()) {
                  excpt_txt = (*it)->GetExcept_text();
              }
              excpt_txt = excpt_txt.empty()? "blank" : excpt_txt;
          }
          else excpt = false;
          desc = GetDiscItemText(**it);
          strtmp = excpt ? excpt_txt : "no";
/*
          if (excpt)  JOINED_FEATURES_sfs[excpt_txt].push_back(desc);
          else JOINED_FEATURES_sfs["no"].push_back(desc);
*/
          thisInfo.test_item_list[GetName_joined()].push_back(strtmp + "$" + desc);
          thisInfo.test_item_objs[GetName_joined() + "$" + strtmp].push_back(
                                                                CConstRef <CObject>(*it));
       }
    }
  }
};


void CBioseq_ONCALLER_HAS_STANDARD_NAME :: GetReport(CRef <CClickableItem>& c_item)
{
   c_item->obj_list = thisInfo.test_item_objs[GetName()];
   c_item->description 
        = GetHasComment(c_item->item_list.size(), "feature") + "standard_name qualifier";
};


void CBioseq_DISC_FEATURE_LIST :: GetReport(CRef <CClickableItem>& c_item)
{
  Str2Strs feat2ls;
  GetTestItemList(c_item->item_list, feat2ls);
  c_item->item_list.clear();
  ITERATE (Str2Strs, it, feat2ls) {
     strtmp = it->first + " feature";
     AddSubcategory(c_item, GetName() + "$" + it->first, &(it->second), strtmp, 
                                 strtmp + "s", e_OtherComment, false);
  }
  c_item->description = "Feature List";
};


bool CBioseq_test_on_all_annot :: LocationHasNullsBetween(const CSeq_feat* seq_feat)
{
  for (CSeq_loc_CI loc_it(seq_feat->GetLocation(), CSeq_loc_CI::eEmpty_Allow); 
           loc_it; ++ loc_it) {
     if (loc_it.GetEmbeddingSeq_loc().IsNull()) {
          return true;
     }
  }
  return false;
};


void CBioseq_ONCALLER_ORDERED_LOCATION :: GetReport(CRef <CClickableItem>& c_item)
{
  c_item->obj_list = thisInfo.test_item_objs[GetName()];
  c_item->description 
    = GetHasComment(c_item->item_list.size(), "feature") + "ordered locations";
};


void CBioseq_JOINED_FEATURES :: GetReport(CRef <CClickableItem>& c_item)
{
   string cmt_txt;
   Str2Strs exp2feats;
   GetTestItemList(c_item->item_list, exp2feats);
   c_item->item_list.clear();
   ITERATE (Str2Strs, it, exp2feats) {
      if (it->first == "no") cmt_txt = "but no exception";
      else if (it->first == "blank") cmt_txt = "but a blank exception";
      else cmt_txt = "but exception " + it->first;
      AddSubcategory(c_item, GetName() + "$" + it->first, &(it->second), "feature", 
            "joined location " + cmt_txt, e_HasComment); 
   }
   c_item->description
        = GetHasComment(c_item->item_list.size(), "feature") + "joined location.";
/*
   ITERATE (Str2Strs, it, JOINED_FEATURES_sfs) {
      CRef <CClickableItem> c_sub (new CClickableItem);
      if (it->first == "no") cmt_txt = "but no exception";
      else if (it->first == "empty") cmt_txt = "but a blank exception";
      else cmt_txt = "but exception " + it->first;
      c_sub->setting_name = GetName();
      c_sub->item_list = it->second;
      c_sub->description = GetHasComment(c_sub->item_list.size(), "feature")
                                + "joined location " + cmt_txt;
      c_item->subcategories.push_back(c_sub);
   }

   c_item->description
        = GetHasComment(c_item->item_list.size(), "feature") + "joined location.";
*/
};


void CBioseq_NO_ANNOTATION :: GetReport(CRef <CClickableItem>& c_item)
{
   c_item->obj_list = thisInfo.test_item_objs[GetName()];
   c_item->description = GetHasComment(c_item->item_list.size(), "bioseq") + "no features";
};

void CBioseq_DISC_LONG_NO_ANNOTATION :: GetReport(CRef <CClickableItem>& c_item)
{
  c_item->obj_list = thisInfo.test_item_objs[GetName()];
  unsigned len = c_item->item_list.size();
  c_item->description = GetIsComment(len, "bioseq") + "longer than 5000nt and " 
                               + ( (len == 1)? "has" : "have") + " no features.";
};

void CBioseq_TEST_DEFLINE_PRESENT :: TestOnObj(const CBioseq& bioseq)
{
  CConstRef <CSeqdesc> seqdesc_title = bioseq.GetClosestDescriptor(CSeqdesc::e_Title);
  if (seqdesc_title.NotEmpty()) {
     thisInfo.test_item_list[GetName()].push_back(GetDiscItemText(bioseq));
     thisInfo.test_item_objs[GetName()].push_back(CConstRef <CObject>(&bioseq));
     ITERATE(vector < CRef < CTestAndRepData > >, it, thisGrp.tests_on_Bioseq_na) {
       if ( (*it)->GetName() == GetName()) {
             thisGrp.tests_4_once.push_back(*it);
             break;
       }
     }
  } 
};


void CBioseq_TEST_DEFLINE_PRESENT :: GetReport(CRef <CClickableItem>& c_item)
{
   c_item->obj_list = thisInfo.test_item_objs[GetName()];
   c_item->description =GetHasComment(c_item->item_list.size(), "Bioseq") + "definition line";
}



void CBioseq_DISC_QUALITY_SCORES :: TestOnObj(const CBioseq& bioseq)
{
  if (bioseq.IsNa() && bioseq.CanGetAnnot()) {
    unsigned cnt = 0;
    ITERATE (list <CRef <CSeq_annot> >, it, bioseq.GetAnnot()) {
       if ( (*it)->GetData().IsGraph() ) { 
             DISC_QUALITY_SCORES_graph |= 1; 
             break; 
       }
       cnt ++;
    }

    if (cnt == bioseq.GetAnnot().size()) {
              DISC_QUALITY_SCORES_graph |= 2; 
    }
  }
};


void CBioseq_DISC_QUALITY_SCORES ::  GetReport(CRef <CClickableItem>& c_item)
{
  if (DISC_QUALITY_SCORES_graph == 1) {
    c_item->description = "Quality scores are present on all sequences.";
  }
  else if (DISC_QUALITY_SCORES_graph == 2) {
    c_item->description = "Quality scores are missing on all sequences.";
  }
  else if (DISC_QUALITY_SCORES_graph == 3) {
    c_item->description = "Quality scores are missing on some sequences.";
  }
};



void CBioseq_RNA_CDS_OVERLAP :: TestOnObj(const CBioseq& bioseq)
{
  string text_i, text_j, subcat_tp;
  ENa_strand strand_i, strand_j;
  bool ignore_trna = IsBioseqHasLineage(bioseq, "Eukaryota", false);

  ITERATE (vector <const CSeq_feat*>, it, rna_not_mrna_feat) {
    CSeqFeatData::ESubtype subtp = (*it)->GetData().GetSubtype();
    if ( subtp == CSeqFeatData::eSubtype_ncRNA
          || ( subtp == CSeqFeatData::eSubtype_tRNA && ignore_trna) ) continue;
    if (x_IsShortrRNA(*it)) continue;
    const CSeq_loc& loc_i = (*it)->GetLocation();
    ITERATE (vector <const CSeq_feat*>, jt, cd_feat) {
      const CSeq_loc& loc_j = (*jt)->GetLocation();
      sequence::ECompare 
          ovlp_com = sequence::Compare(loc_j, loc_i, thisInfo.scope);
      subcat_tp = kEmptyStr;
      switch (ovlp_com) {
        case sequence::eSame: subcat_tp = "extrc"; break;
        case sequence::eContained: subcat_tp = "cds_in_rna"; break;
        case sequence::eContains: subcat_tp = "rna_in_cds"; break;
        case sequence::eOverlap: {
           strand_i = loc_i.GetStrand();
           strand_j = loc_j.GetStrand();
           if ((strand_i == eNa_strand_minus && strand_j != eNa_strand_minus)
                       || (strand_i != eNa_strand_minus 
                                && strand_j == eNa_strand_minus))
                subcat_tp = "overlap_opp_strand";
           else subcat_tp = "overlap_same_strand";
           break;
        }
        default:;
      }
      if (!subcat_tp.empty()) {
         thisInfo.test_item_list[GetName()]
                     .push_back(subcat_tp +"$" +GetDiscItemText(**jt));
         thisInfo.test_item_list[GetName()]
                     .push_back(subcat_tp +"$" +GetDiscItemText(**it));
         thisInfo.test_item_objs[GetName()+"$"+subcat_tp]
                     .push_back(CConstRef <CObject>(*jt));
         thisInfo.test_item_objs[GetName()+"$"+subcat_tp]
                     .push_back(CConstRef <CObject>(*it));
      }
    }
  }
}; //  CBioseq_RNA_CDS_OVERLAP :: TestOnObj



void CBioseq_RNA_CDS_OVERLAP :: GetReport(CRef <CClickableItem>& c_item)
{
   Str2Strs subcat2list;
   GetTestItemList(c_item->item_list, subcat2list);
   c_item->item_list.clear();
   unsigned cnt_tot, cnt_ovp;
   cnt_tot = cnt_ovp = 0;

   string desc1, desc2, desc3;
   CRef <CClickableItem> ovp_subcat (new CClickableItem);
   ITERATE (Str2Strs, it, subcat2list) {
     if (it->first == "exact" 
             || it->first == "cds_in_rna" || it->first == "rna_in_cds") {
       if (it->first == "exact") {
         desc1 = "coding region exactly matches an RNA";
         desc2 = "coding regions exactly match RNAs";
       }
       else if (it->first == "cds_in_rna") {
         desc1 = "coding region is completely contained in an RNA location";
         desc2 = "coding regions are completely contained in RNA locations";
       }
       else if (it->first == "rna_in_cds" ) {
         desc1 = "coding region completely contains an RNA";
         desc2 = "coding regions completely contain RNAs";
       }
       AddSubcategory(c_item, GetName() + "$" + it->first, &(it->second), 
                  desc1, desc2, e_OtherComment, false, "", true); 
     }
     else {
       desc1 = "coding region overlaps an RNA";
       desc2 = "coding regions overlap RNAs";
       if (it->first == "overlap_opp_strand") 
             desc3 = " on the opposite strand (no containment)";
       else desc3 = " on the same strand (no containment)";
       AddSubcategory(ovp_subcat, GetName() + "$" + it->first, &(it->second), 
                  desc1, desc2, e_OtherComment, false, desc3, true); 
       cnt_ovp += it->second.size()/2;
     } 
     cnt_tot += it->second.size()/2;
   }
   if (!ovp_subcat->subcategories.empty()) {
      ovp_subcat->setting_name = GetName();
      ovp_subcat->description 
         = GetOtherComment(cnt_ovp, 
                           "coding region overlaps an RNA", 
                           "coding region overlap RNAs") 
             + " (no containment)";
      c_item->subcategories.push_back(ovp_subcat);
   }

   c_item->description 
        = GetOtherComment(cnt_tot, 
                          "coding region overlaps an RNA feature", 
                          "coding regions overlap RNA features") ;
}; // CBioseq_RNA_CDS_OVERLAP :: GetReport



bool CBioseq_OVERLAPPING_CDS :: OverlappingProdNmSimilar(const string& prod_nm1, const string& prod_nm2)
{
  bool str1_has_similarity_word = false, str2_has_similarity_word = false;
  vector < string > simi_prod_wds;
  simi_prod_wds.push_back("transposase");
  simi_prod_wds.push_back("integrase");  

  vector < string > ignore_simi_prod_wds;
  ignore_simi_prod_wds.push_back("hypothetical protein");
  ignore_simi_prod_wds.push_back("phage");
  ignore_simi_prod_wds.push_back("predicted protein");

  unsigned i;
  /* if both product names contain one of the special case similarity words,
   * the product names are similar. */
  
  for (i = 0; i < simi_prod_wds.size(); i++)
  {
    if (!str1_has_similarity_word 
          && string::npos != NStr::FindNoCase(prod_nm1, simi_prod_wds[i]))
         str1_has_similarity_word = true;

    if (!str2_has_similarity_word
           && string::npos != NStr::FindNoCase(prod_nm2, simi_prod_wds[i]))
        str2_has_similarity_word = true;
  }
  if (str1_has_similarity_word && str2_has_similarity_word) return true;
  
  /* otherwise, if one of the product names contains one of special ignore similarity
   * words, the product names are not similar.
   */
  for (i = 0; i < ignore_simi_prod_wds.size(); i++)
     if (string::npos != NStr::FindNoCase(prod_nm1, ignore_simi_prod_wds[i])
           || string::npos != NStr::FindNoCase(prod_nm2, ignore_simi_prod_wds[i]))
         return false;

  if (!(NStr::CompareNocase(prod_nm1, prod_nm2))) return true;
  else return false;

}; // OverlappingProdNmSimilar



void CBioseq_OVERLAPPING_CDS :: AddToDiscRep(const CSeq_feat* seq_feat)
{
     string desc = GetDiscItemText(*seq_feat);
     CConstRef <CObject> feat_ref (seq_feat);
     bool has_comment = false;
     if (seq_feat->CanGetComment()) {
         const string& comm = seq_feat->GetComment();
         if ( string::npos != NStr::FindNoCase(comm, "overlap")
                        || string::npos != NStr::FindNoCase(comm, "frameshift")
                        || string::npos != NStr::FindNoCase(comm, "frame shift")
                        || string::npos != NStr::FindNoCase(comm, "extend") ) {
              thisInfo.test_item_list[GetName()].push_back("comment$" + desc);
              thisInfo.test_item_objs[GetName() + "$comment"]
                         .push_back(feat_ref);
              has_comment = true;
         }
         
     }
     if (!has_comment) {
        thisInfo.test_item_list[GetName()].push_back("no_comment$" + desc);
        thisInfo.test_item_objs[GetName() + "$no_comment"].push_back(feat_ref);
     }
}; // AddToDiscRep



bool CBioseq_OVERLAPPING_CDS :: HasNoSuppressionWords(const CSeq_feat* seq_feat)
{
  string prod_nm;
  if (seq_feat->GetData().GetSubtype() == CSeqFeatData::eSubtype_cdregion) {
     prod_nm = GetProdNmForCD(*seq_feat);
     if ( DoesStringContainPhrase(prod_nm, "ABC")
                    || DoesStringContainPhrase(prod_nm, "transposon", false, false)
                    || DoesStringContainPhrase(prod_nm, "transposase", false, false))
          return false;
     else return true;
  }
  else return false;
}; // HasNoSuppressionWords()



void CBioseq_OVERLAPPING_CDS :: TestOnObj(const CBioseq& bioseq)
{
  unsigned i, j;
  vector <const CSeq_feat*>::const_iterator jt;
  vector <int> added;
  added.reserve(cd_feat.size());
  for (unsigned i=0; i< cd_feat.size(); i++) {
    added.push_back(0);
  }
  i=0;
  bool span_circular;
  ITERATE (vector <const CSeq_feat*>, it, cd_feat) {
    const CSeq_loc& loc_i = (*it)->GetLocation();
    span_circular = (loc_i.GetStart(eExtreme_Positional) 
                        > loc_i.GetStop(eExtreme_Positional));
    ENa_strand strandi = loc_i.GetStrand();
    jt = it;
    j = i;
    while (++jt != cd_feat.end()) {
      j++;
      const CSeq_loc& loc_j = (*jt)->GetLocation();
      if (!span_circular 
             && loc_i.GetStop(eExtreme_Positional) 
                    < loc_j.GetStart(eExtreme_Positional)) {
         break;
      }
      if(!OverlappingProdNmSimilar(GetProdNmForCD(**it), GetProdNmForCD(**jt))){
          continue;
      }
      ENa_strand strandj = loc_j.GetStrand();
      if (strandi != strandj 
             && (strandi == eNa_strand_minus || strandj ==eNa_strand_minus)) {
           continue;
      }
      if ( sequence::eNoOverlap != sequence::Compare(loc_i, loc_j, thisInfo.scope)) {
          if ( !added[i] && HasNoSuppressionWords(*it)) {
               AddToDiscRep(*it);
               added[i] = true;
          }
          if ( !added[j] && HasNoSuppressionWords(*jt) ) {
               AddToDiscRep(*jt);
               added[j] = true;
          }
      }
    }
    i++;
  }
}


void CBioseq_OVERLAPPING_CDS :: GetReport(CRef <CClickableItem>& c_item)
{
   Str2Strs subcat2list;
   string desc1, desc2, desc3;
   GetTestItemList(c_item->item_list, subcat2list);
   c_item->item_list.clear();
   ITERATE (Str2Strs, it, subcat2list) {
     if (it->first != "comment") {
       desc1 = "coding region overlapps";
       desc2 = "coding regions overlap";
       desc3 = " another coding region with a similar or identical name that do not have the appropriate note text.";
     }
     else {
       desc1 = "coding region overlapps";
       desc2 = "coding regions overlap";
       desc3 = " another coding region with a similar or identical name but have the appropriate note text.";
     }
     AddSubcategory(c_item, GetName() + "$" + it->first, &(it->second),desc1, desc2, 
                                                             e_OtherComment, true, desc3);
   }
   c_item->description 
     = GetOtherComment(c_item->item_list.size(), "coding region overlapps", 
                                                                "coding regions overlap")
        + " another coding region with a similar or identical name.";
}; // OVERLAPPING_CDS



// new comb
void CBioseq_test_on_missing_genes :: CheckGenesForFeatureType (const vector <const CSeq_feat*>& feats, bool makes_gene_not_superfluous)
{
  unsigned j;

  ITERATE (vector <const CSeq_feat*>, it, feats) {
    if ((*it)->GetData().IsGene()) {
       continue;
    }
    const CGene_ref* xref_gene = (*it)->GetGeneXref();
    if (xref_gene && xref_gene->IsSuppressed()) {
         m_no_genes[GetDiscItemText(**it)] = CConstRef <CObject>(*it);
    }
    else {
        if (xref_gene) {
           if (m_super_cnt) {
             j = 0;
             ITERATE (vector <const CSeq_feat*>, jt, gene_feat) {
               if (m_super_idx[j] && GeneRefMatchForSuperfluousCheck(
                                        (*jt)->GetData().GetGene(), xref_gene)){
                   m_super_cnt --;
                   m_super_idx[j] = 0;
               }
               j++;
             }
           }
        } 
        else {
          // GetBest... can't deal with the mix strand now,
          // so for File1fordiscrep.sqn, it needs to check the EXTRA_GENES
          // and MISSING_GENES after the bug is fixed.
          CConstRef < CSeq_feat>
             gene_olp= sequence::GetBestOverlappingFeat( (*it)->GetLocation(),
                                               CSeqFeatData::e_Gene,
                                               sequence::eOverlap_Contained,
                                               *thisInfo.scope);

          if (gene_olp.Empty()) {
              m_no_genes[GetDiscItemText(**it)] = CConstRef <CObject>(*it);
          }
          else if ( m_super_cnt && makes_gene_not_superfluous) {
             j=0;
             ITERATE (vector <const CSeq_feat*>, jt, gene_feat) {
               if (m_super_idx[j] && (*jt == gene_olp.GetPointer())) {
                    m_super_idx[j] = 0;
                    m_super_cnt --;
               }
               j++;
             }
          }
        }
    }
  } 
};

bool CBioseq_missing_genes_regular :: x_HasPseudogeneQualifier (const CSeq_feat& sft)
{
  if (!sft.CanGetQual()) return false;
  ITERATE (vector <CRef< CGb_qual > >, it, sft.GetQual()) {
    if (NStr::EqualNocase( (*it)->GetQual(), "pseudogene")) return true;
  }

  return false;
};

void CBioseq_missing_genes_regular :: TestOnObj(const CBioseq& bioseq)
{
  if (thisTest.is_Genes_run) return;
  thisTest.is_Genes_run = true;

  bool run_missing = (thisTest.tests_run.find(GetName_missing()) != end_it);
  bool run_extra = (thisTest.tests_run.find(GetName_extra()) != end_it);

  unsigned i;
  m_super_idx.clear();
  if (IsmRNASequenceInGenProdSet(bioseq)) {
      m_super_cnt = 0;
  }
  else {
     m_super_cnt = gene_feat.size();
     m_super_idx.reserve(m_super_cnt);
     for (i=0; i< m_super_cnt; i++) {
             m_super_idx.push_back(1);
     }
  }
  m_no_genes.clear();
  CheckGenesForFeatureType(cd_feat, true); 
  CheckGenesForFeatureType(rna_feat, true);
  CheckGenesForFeatureType(rbs_feat);
  CheckGenesForFeatureType(exon_feat);
  CheckGenesForFeatureType(intron_feat);

  // MISSING_GENES
  if (run_missing && !m_no_genes.empty()) {
      ITERATE (Str2Obj, it, m_no_genes) {
         thisInfo.test_item_list[GetName_missing()].push_back(it->first);
         thisInfo.test_item_objs[GetName_missing()].push_back(it->second);
      }
  }

  // EXTRA_GENES
  if (!run_extra || !m_super_cnt) {
     return;
  }

  //GetPseudoAndNonPseudoGeneList;
  i=0;
  string desc, add_txt;
  ITERATE (vector <const CSeq_feat* >, it, gene_feat) {
     if (m_super_idx[i++]) {
       desc = GetDiscItemText(**it);
       CConstRef <CObject> feat_ref (*it);
       thisInfo.test_item_list[GetName_extra()].push_back("extra$" + desc);
       thisInfo.test_item_objs[GetName_extra()+"$extra"].push_back(feat_ref);
       add_txt = kEmptyStr;
       const CSeqFeatData& seq_feat_dt = (*it)->GetData();
       if ( ((*it)->CanGetPseudo() && (*it)->GetPseudo()) 
                 || seq_feat_dt.GetGene().GetPseudo() 
                 || x_HasPseudogeneQualifier(**it) ) {
            add_txt = "pseudo";
       }
       else {
             if ((*it)->CanGetComment()) {
               const string& comment = (*it)->GetComment();
               // RemoveGenesWithNoteOrDescription
               if (comment.empty()) {
                 const CGene_ref& gene_ref = seq_feat_dt.GetGene();
                 if (!gene_ref.CanGetDesc() || gene_ref.GetDesc().empty()) 
                    add_txt = "nonshift";
               }
               
/* this doesn't work after RemoveGenesWithNoteOrDescription
              //GetFrameshiftAndNonFrameshiftGeneList,
               if (string::npos != NStr::FindNoCase(comment, "frameshift")
                    || string::npos!= NStr::FindNoCase(comment, "frame shift"))
                   nm_desc = "frameshift$" + desc;
               else nm_desc = "nonshift$" + desc;
*/
             }
             else add_txt = "nonshift";
       }
       if (!add_txt.empty()) {
          thisInfo.test_item_list[GetName_extra()]
                      .push_back(add_txt + "$" + desc);
          thisInfo.test_item_objs[GetName_extra()+ "$" + add_txt]
                      .push_back(feat_ref);
       }
     }
  }
};


void CBioseq_MISSING_GENES :: GetReport(CRef <CClickableItem>& c_item)
{
  c_item->obj_list = thisInfo.test_item_objs[GetName()];
  c_item->description
    = GetHasComment(c_item->item_list.size(), "feature") + "no genes.";
};


void CBioseq_EXTRA_GENES :: GetReport(CRef <CClickableItem>& c_item)
{
  string desc1, desc2;
  Str2Strs tp2ls;
  GetTestItemList(c_item->item_list, tp2ls);
  c_item->item_list.clear();
  ITERATE (Str2Strs, it, tp2ls) {
    if (it->first == "extra") {
          c_item->item_list = it->second; 
          c_item->obj_list = thisInfo.test_item_objs[GetName() + "$extra"];
    }
    else {
      if (it->first == "pseudo") {
         desc1 = "pseudo gene feature";
         desc2 = "not associated with a CDS or RNA feature.";
      }
      else if (it->first == "frameshift") {
         desc1 = "non-pseudo gene feature";
         desc2
         ="not associated with a CDS or RNA feature and have frameshift in the comment.";
      }
      else if (it->first == "nonshift") {
            desc1 = "non-pseudo gene feature";
            strtmp = (it->second.size() > 1) ? "do" : "does";
            desc2 = "not associated with a CDS or RNA feature and "
                              + strtmp + " not have frameshift in the comment.";
      }
      AddSubcategory(c_item, GetName() + "$" + it->first, &(it->second), desc1, desc2, 
                      e_IsComment, false);
    }
  };
  c_item->description = GetIsComment(c_item->item_list.size(), "gene feature") 
                        + "not associated with a CDS or RNA feature.";
};


void CBioseq_missing_genes_oncaller :: TestOnObj(const CBioseq& bioseq)
{
  if (thisTest.is_Genes_oncall_run) return;
  thisTest.is_Genes_oncall_run = true;

  bool run_missing = (thisTest.tests_run.find(GetName_missing()) != end_it);
  bool run_extra = (thisTest.tests_run.find(GetName_extra()) != end_it);
  if (!run_missing && !run_extra) return;

  if (bioseq.IsAa()) return;
  unsigned i=0;
  m_super_idx.clear();
  m_no_genes.clear();
  if (IsmRNASequenceInGenProdSet(bioseq)) {
     m_super_cnt = 0;
  }
  else {
     m_super_cnt = gene_feat.size();
     m_super_idx.reserve(gene_feat.size());
     for (i=0; i< gene_feat.size(); i++) {
       if ( !(gene_feat[i]->CanGetPseudo()) || !(gene_feat[i]->GetPseudo()) ) {
          m_super_idx.push_back(1);
       }
       else {
          m_super_idx.push_back(0);
       }
     }
  }

  CheckGenesForFeatureType(cd_feat, true);
  CheckGenesForFeatureType(mrna_feat, true);
  CheckGenesForFeatureType(trna_feat, true);

  // ONCALLER_GENE_MISSING
  if (run_missing && !m_no_genes.empty()) {
     ITERATE (Str2Obj, it, m_no_genes) {
        thisInfo.test_item_list[GetName_missing()].push_back(it->first);
        thisInfo.test_item_objs[GetName_missing()].push_back(it->second);
     }
  }

  // ONCALLER_SUPERFLUOUS_GENE
  if (!run_extra) return;
  m_no_genes.clear();
  CheckGenesForFeatureType(all_feat, true);
  m_no_genes.clear();
  if (!m_super_cnt ) return;
  i=0;
  ITERATE (vector <const CSeq_feat*>, it, gene_feat) {
    if (!m_super_idx[i++]) continue;
    /* remove genes with explanatory comments/descriptions */
    if (!IsOkSuperfluousGene( *it )) {
        thisInfo.test_item_list[GetName_extra()].push_back(GetDiscItemText(**it));
        thisInfo.test_item_objs[GetName_extra()].push_back(CConstRef <CObject>(*it));
    }
  } 
};


bool CBioseq_missing_genes_oncaller :: IsOkSuperfluousGene (const CSeq_feat* seq_feat)
{
  string phrase = "coding region not determined";
  if (seq_feat->CanGetComment() && CommentHasPhrase(seq_feat->GetComment(), phrase))
     return true;
  else if (seq_feat->GetData().GetGene().CanGetDesc()
             && CommentHasPhrase(seq_feat->GetData().GetGene().GetDesc(), phrase))
     return true;
  else return false;
};


void CBioseq_ONCALLER_GENE_MISSING :: GetReport(CRef <CClickableItem>& c_item)
{
  c_item->obj_list = thisInfo.test_item_objs[GetName()];
  c_item->description
    = GetHasComment(c_item->item_list.size(), "feature") + "no genes.";
};


void CBioseq_ONCALLER_SUPERFLUOUS_GENE :: GetReport(CRef <CClickableItem>& c_item)
{
   c_item->obj_list = thisInfo.test_item_objs[GetName()];
   unsigned cnt = c_item->item_list.size();
   c_item->description = GetIsComment(cnt, "gene feature") 
                         + "not associated with any feature and " 
                         + ((cnt > 1) ? "are" : "is") + " not pseudo.";
}

// new comb


bool CBioseq_test_on_missing_genes :: GeneRefMatchForSuperfluousCheck (const CGene_ref& gene, const CGene_ref* g_xref)
{
  const string& locus1 = (gene.CanGetLocus()) ? gene.GetLocus() : kEmptyStr;
  const string& 
        locus_tag1 = (gene.CanGetLocus_tag()) ? gene.GetLocus_tag() : kEmptyStr;

  const string& locus2=(g_xref->CanGetLocus()) ? g_xref->GetLocus() : kEmptyStr;
  const string& 
        locus_tag2 
            = (g_xref->CanGetLocus_tag()) ? g_xref->GetLocus_tag() : kEmptyStr;

  if ( (!locus1.empty() && !locus2.empty() && locus1 != locus2)
        ||(!locus_tag1.empty() && !locus_tag2.empty() && locus_tag1!=locus_tag2)
        || (gene.GetPseudo() != g_xref->GetPseudo()) ) {
      return false;
  }
  else {
    const string& allele1 = (gene.CanGetAllele()) ? gene.GetAllele(): kEmptyStr;
    const string& 
           allele2 = (g_xref->CanGetAllele()) ? g_xref->GetAllele(): kEmptyStr;
    if (!allele1.empty() && !allele2.empty() && allele1 != allele2) {
       return false;
    }
    else {
      const string& desc1 = (gene.CanGetDesc())? gene.GetDesc() : kEmptyStr;
      const string& desc2=(g_xref->CanGetDesc())? g_xref->GetDesc() : kEmptyStr;
      if (!desc1.empty() && !desc2.empty() && desc1 != desc2) {
         return false;
      }
      else {
        const string& 
             maploc1 = (gene.CanGetMaploc())? gene.GetMaploc(): kEmptyStr;
        const string& 
             maploc2= (g_xref->CanGetMaploc())? g_xref->GetMaploc(): kEmptyStr; 
        if (!maploc1.empty() && !maploc2.empty() && maploc1 != maploc2) {
             return false; 
        }
      }      
    } 
  }
  
  return true;
};


void CBioseq_DISC_COUNT_NUCLEOTIDES :: TestOnObj(const CBioseq& bioseq) // in the *_Bioseq_na grp
{
   thisInfo.test_item_list[GetName()].push_back(GetDiscItemText(bioseq)); 
   thisInfo.test_item_objs[GetName()].push_back(CConstRef <CObject>(&bioseq));
}; // DISC_COUNT_NUCLEOTIDES :: TestOnObj


void CBioseq_DISC_COUNT_NUCLEOTIDES :: GetReport(CRef <CClickableItem>& c_item)
{
    c_item->setting_name = GetName();
    c_item->item_list = thisInfo.test_item_list[GetName()];
    c_item->description 
          = GetIsComment(c_item->item_list.size(), "nucleotide Bioseq")
            + "present.";
    c_item->obj_list = thisInfo.test_item_objs[GetName()];

}; // DISC_COUNT_NUCLEOTIDES :: GetReport()

void CBioseq_DUPLICATE_GENE_LOCUS :: TestOnObj(const CBioseq& bioseq)
{
  Str2SubDt locus_list;
  Str2SubDt :: iterator it;
  string locus, location;
  ITERATE (vector <const CSeq_feat*>, jt, gene_feat) {
     if ((*jt)->GetData().GetGene().CanGetLocus()) {
        locus = (*jt)->GetData().GetGene().GetLocus();
        if (!locus.empty()) {
            location 
                = BioseqToBestSeqId(bioseq, CSeq_id::e_Genbank).AsFastaString();
            thisInfo.test_item_list[GetName()] .push_back(
                         location + "$" + locus + "#" + GetDiscItemText(**jt));
            thisInfo.test_item_objs[GetName() + "$" + location + "#" + locus]
                    .push_back(CConstRef <CObject>(*jt));
        }
     }
  }   
};


void CBioseq_DUPLICATE_GENE_LOCUS :: GetReport(CRef <CClickableItem>& c_item)
{
   Str2Strs bsq2locus_feats, locus2feats;
   GetTestItemList(c_item->item_list, bsq2locus_feats);
   c_item->item_list.clear();
   string locus, location;
   CRef <CClickableItem> c_bsq (new CClickableItem);
   ITERATE (Str2Strs, it, bsq2locus_feats) {
     if (it->second.size() > 1) {
        c_bsq.Reset(new CClickableItem);
        location = it->first; 
        locus2feats.clear();
        GetTestItemList(it->second, locus2feats, "#");
        ITERATE (Str2Strs, jt, locus2feats) {
          if (jt->second.size() > 1) { 
             AddSubcategory(c_bsq, 
                            GetName() + "$" + it->first + "#" + jt->first, 
                            &(jt->second), "gene", "locus " + jt->first, 
                            e_HasComment);
          }
        }
        if (!c_bsq->subcategories.empty()) {
            c_bsq->setting_name = GetName();
            c_bsq->description 
                    = GetHasComment(c_bsq->item_list.size(), "gene")
                      + "the same locus as another gene on " + location;
            copy(c_bsq->item_list.begin(), c_bsq->item_list.end(),
                 back_inserter( c_item->item_list));
            copy(c_bsq->obj_list.begin(), c_bsq->obj_list.end(),
                 back_inserter( c_item->obj_list));
            c_item->subcategories.push_back(c_bsq);
        }
     }
   }
 
   if (!c_item->subcategories.empty()) {
       c_item->description = GetHasComment(c_item->item_list.size(), "gene")
                         + "the same locus as another gene on the same Bioseq.";
   }
}; // DUPLICATE_GENE_LOCUS


// CBioseq_set
// new comb
bool CBioseqSet_on_class :: IsMicrosatelliteRepeatRegion (const CSeq_feat& seq_feat)
{
   if (seq_feat.GetData().GetSubtype() != CSeqFeatData::eSubtype_repeat_region) return false;
   if (seq_feat.CanGetQual()) {
      ITERATE (vector <CRef < CGb_qual > >, it, seq_feat.GetQual()) {
        if ( NStr::EqualNocase((*it)->GetQual(), "satellite")
                         && NStr::EqualNocase( (*it)->GetVal(), "microsatellite"))
           return true;
      }
   }
   return false;
};


void CBioseqSet_on_class :: FindUnwantedSetWrappers (const CBioseq_set& set)
{
  ITERATE (list < CRef < CSeq_entry > >, it, set.GetSeq_set()) {
    if ( (*it)->IsSeq() ) {
       const CBioseq& bioseq = (*it)->GetSeq();
       CBioseq_Handle bioseq_hl = thisInfo.scope->GetBioseqHandle(bioseq);
       if (!bioseq_hl) continue;
       for (CSeqdesc_CI ci(bioseq_hl, CSeqdesc::e_Source); ci && !m_has_rearranged; ++ci) {
         m_has_rearranged = IsSubSrcPresent( ci->GetSource(), CSubSource::eSubtype_rearranged);
       }
       for (CFeat_CI feat_it(bioseq_hl); feat_it && (!m_has_sat_feat || !m_has_non_sat_feat); 
                                                                                 ++feat_it) {
          if (IsMicrosatelliteRepeatRegion(feat_it->GetOriginalFeature())) 
                  m_has_sat_feat = true;
          else m_has_non_sat_feat = true; 
       }
    } 
    else if (!m_has_rearranged || !m_has_sat_feat || !m_has_non_sat_feat)
            FindUnwantedSetWrappers( (*it)->GetSet() );
  }
};


void CBioseqSet_on_class :: TestOnObj(const CBioseq_set& bioseq_set)
{
   if (thisTest.is_BioSet_run) return;
   thisTest.is_BioSet_run = true;

   string desc = GetDiscItemText(bioseq_set);
   CConstRef <CObject> set_ref (&bioseq_set);
   CBioseq_set::EClass e_cls = bioseq_set.GetClass();
   if ( e_cls == CBioseq_set::eClass_eco_set
        || e_cls == CBioseq_set :: eClass_mut_set
        || e_cls == CBioseq_set :: eClass_phy_set
        || e_cls == CBioseq_set :: eClass_pop_set) {

      // DISC_NONWGS_SETS_PRESENT
      if (thisTest.tests_run.find(GetName_nonwgs()) != end_it) {
           thisInfo.test_item_list[GetName_nonwgs()].push_back(desc);
           thisInfo.test_item_objs[GetName_nonwgs()].push_back(set_ref);
      }
 
      // TEST_UNWANTED_SET_WRAPPER
      if (thisTest.tests_run.find(GetName_wrap()) != end_it) {
         FindUnwantedSetWrappers(bioseq_set);
         if (m_has_rearranged || (m_has_sat_feat && !m_has_non_sat_feat) ) {
            thisInfo.test_item_list[GetName_wrap()].push_back(desc);
            thisInfo.test_item_objs[GetName_wrap()].push_back(set_ref);
         }
      }
   }
   else if (thisTest.tests_run.find(GetName_segset()) != end_it) {
      // DISC_SEGSETS_PRESENT
      if ( e_cls == CBioseq_set :: eClass_segset) {
            thisInfo.test_item_list[GetName_segset()].push_back(desc);
            thisInfo.test_item_objs[GetName_segset()].push_back(set_ref);
      }
   }
};


void CBioseqSet_TEST_UNWANTED_SET_WRAPPER :: GetReport(CRef <CClickableItem>& c_item)
{
   c_item->obj_list = thisInfo.test_item_objs[GetName()];
   unsigned cnt = c_item->item_list.size();
   c_item->description = NStr :: UIntToString(cnt) + GetNoun(cnt, "unwanted set wrapper");
};


void CBioseqSet_DISC_SEGSETS_PRESENT :: GetReport(CRef <CClickableItem>& c_item)
{
   c_item->obj_list = thisInfo.test_item_objs[GetName()];
   c_item->description = GetIsComment(c_item->item_list.size(), "segset") + "present.";
};


void CBioseqSet_DISC_NONWGS_SETS_PRESENT :: GetReport(CRef <CClickableItem>& c_item)
{
   c_item->obj_list = thisInfo.test_item_objs[GetName()];
   c_item->description 
        = GetIsComment(c_item->item_list.size(), "set") + "of type eco, mut, phy or pop.";
};
// new comb


// CSeqEntryTestAndRepData
// new method
bool CSeqEntry_test_on_tax_cflts :: s_StringHasVoucherSN(const string& vou_nm)
{
  vector <string> spec_vou;
  spec_vou.push_back("s.n.");
  spec_vou.push_back("sn");
  return (DoesStringContainPhrase(vou_nm, spec_vou));
};


void CSeqEntry_test_on_tax_cflts :: RunTests(const CBioSource& biosrc, const string& desc, CConstRef <CObject> obj_ref)
{
   string tax_nm;
   tax_nm = biosrc.IsSetTaxname() ? biosrc.GetTaxname() : kEmptyStr;
   if (tax_nm.empty()) return; 
   else tax_nm = "$" + tax_nm + "#";
   string tax_desc(tax_nm + desc);

   string desc_tmp;
   ITERATE (list <CRef <COrgMod> >, it, biosrc.GetOrg().GetOrgname().GetMod()) {
     strtmp = (*it)->GetSubname() + tax_desc;
     desc_tmp = "$" + strtmp + "#" + tax_nm;
     switch ( (*it)->GetSubtype() ) {

       // DISC_SPECVOUCHER_TAXNAME_MISMATCH
       case COrgMod::eSubtype_specimen_voucher: 
             if (m_run_vou && !s_StringHasVoucherSN(strtmp)) {
                 thisInfo.test_item_list[GetName_vou()].push_back(strtmp);
                 thisInfo.test_item_objs[GetName_vou() + desc_tmp]
                           .push_back(obj_ref);
             }
             break;
       // DISC_STRAIN_TAXNAME_MISMATCH
       case COrgMod::eSubtype_strain:
             if (m_run_str) {
                thisInfo.test_item_list[GetName_str()].push_back(strtmp);
                thisInfo.test_item_objs[GetName_str() + desc_tmp]
                          .push_back(obj_ref);
             }
             break;
       // DISC_CULTURE_TAXNAME_MISMATCH
       case COrgMod::eSubtype_culture_collection:
             if (m_run_cul) {
                thisInfo.test_item_list[GetName_cul()].push_back(strtmp);
                thisInfo.test_item_objs[GetName_cul() + desc_tmp]
                          .push_back(obj_ref);
             }
             break;
       // DISC_BIOMATERIAL_TAXNAME_MISMATCH
       case COrgMod::eSubtype_bio_material:
             if (m_run_biom) { 
                 thisInfo.test_item_list[GetName_biom()].push_back(strtmp);
                 thisInfo.test_item_objs[GetName_biom() + desc_tmp]
                          .push_back(obj_ref);
             }
             break;
       default: break;
     }
   }
};


void CSeqEntry_test_on_tax_cflts :: TestOnObj(const CSeq_entry& seq_entry)
{
   if (thisTest.is_TaxCflts_run) return;
   thisTest.is_TaxCflts_run = true;

   m_run_vou = (thisTest.tests_run.find(GetName_vou()) != end_it);
   m_run_str = (thisTest.tests_run.find(GetName_str()) != end_it);
   m_run_cul = (thisTest.tests_run.find(GetName_cul()) != end_it);
   m_run_biom = (thisTest.tests_run.find(GetName_biom()) != end_it);

   string desc;
   ITERATE (vector <const CSeq_feat*>, it, biosrc_orgmod_feat) {
      RunTests( (*it)->GetData().GetBiosrc(), GetDiscItemText(**it), CConstRef <CObject> (*it));
   }
  
   unsigned i=0;
   ITERATE (vector <const CSeqdesc*>, it, biosrc_orgmod_seqdesc) {
      desc = GetDiscItemText( **it, *(biosrc_orgmod_seqdesc_seqentry[i]));
      RunTests((*it)->GetSource(), desc, CConstRef <CObject>(*it));
      i++;
   }
};


void CSeqEntry_test_on_tax_cflts :: GetReport_cflts(CRef <CClickableItem>& c_item, const string& setting_name, const string& qual_nm)
{
   Str2Strs qual2tax_seqs, tax2seqs;
   GetTestItemList(c_item->item_list, qual2tax_seqs);
   c_item->item_list.clear();
   string qual;
   vector <CConstRef <CObject> >* obj_ls;
   if (qual2tax_seqs.size() == 1) {
       tax2seqs.clear();
       GetTestItemList(qual2tax_seqs.begin()->second, tax2seqs, "#");
       qual = qual2tax_seqs.begin()->first;
       if (tax2seqs.size() > 1) {
          ITERATE (Str2Strs, jt, tax2seqs) {
             copy(jt->second.begin(), jt->second.end(),
                  back_inserter( c_item->item_list));
             strtmp = "$" + qual + "#" + jt->first;
             obj_ls = &(thisInfo.test_item_objs[GetName() + strtmp]);
             copy(obj_ls->begin(), obj_ls->end(), 
                   back_inserter(c_item->obj_list));
          }
          c_item->description 
             = GetHasComment(c_item->item_list.size(), "biosource") 
               + qual_nm + " '" + qual + "' but do not have the same taxnames.";
       }
   }
   else {
     ITERATE (Str2Strs, it, qual2tax_seqs) {
       qual = it->first;
       tax2seqs.clear();
       GetTestItemList(it->second, tax2seqs, "#");
       if (tax2seqs.size() > 1) {
           CRef <CClickableItem> c_sub (new CClickableItem);
           ITERATE (Str2Strs, jt, tax2seqs) {
             copy(jt->second.begin(), jt->second.end(),
                  back_inserter( c_sub->item_list));
             strtmp = "$" + qual + "#" + jt->first;
             obj_ls = &(thisInfo.test_item_objs[GetName() + strtmp]);
             copy(obj_ls->begin(), obj_ls->end(), 
                   back_inserter(c_sub->obj_list));
           }
           c_sub->description
              = GetHasComment(c_sub->item_list.size(), "biosource") 
                + qual_nm + " '" + qual+ "' but do not have the same taxnames.";
           copy(c_sub->item_list.begin(), c_sub->item_list.end(),
                back_inserter( c_item->item_list));
           copy(c_sub->obj_list.begin(), c_sub->obj_list.end(),
                back_inserter( c_item->obj_list));
           c_item->subcategories.push_back(c_sub);
       }   
     }
     c_item->description = GetHasComment(c_item->item_list.size(), "BioSource") 
                           + qual_nm + "/taxname conflicts.";
   }
};




void CSeqEntry_test_on_quals :: GetMultiSubSrcVlus(const CBioSource& biosrc, const string& type_name, vector <string>& multi_vlus)
{
  ITERATE (list <CRef <CSubSource> >, it, biosrc.GetSubtype()) {
     int type = (*it)->GetSubtype();
     if ( (*it)->GetSubtypeName( type ) == type_name ) {
        strtmp = (*it)->GetName();
        if (strtmp.empty() && (type == CSubSource::eSubtype_germline
                                 || type == CSubSource::eSubtype_transgenic
                                 || type == CSubSource::eSubtype_metagenomic
                                 || type == CSubSource::eSubtype_environmental_sample
                                 || type == CSubSource::eSubtype_rearranged)) {
           multi_vlus.push_back("TRUE");
        }
        else multi_vlus.push_back(strtmp);
     }
  }
};


void CSeqEntry_test_on_quals :: GetMultiOrgModVlus(const CBioSource& biosrc, const string& type_name, vector <string>& multi_vlus)
{
   ITERATE (list <CRef <COrgMod> >, it, biosrc.GetOrgname().GetMod() )
      if ((*it)->GetSubtypeName((*it)->GetSubtype()) == type_name)
          multi_vlus.push_back((*it)->GetSubname());
};


void CSeqEntry_test_on_quals :: GetMultiPrimerVlus(const CBioSource& biosrc, const string& qual_name, vector <string>& multi_vlus)
{
   if (!biosrc.CanGetPcr_primers()) return;
   ITERATE (list <CRef <CPCRReaction> >, jt, biosrc.GetPcr_primers().Get()) {
     if (qual_name == "fwd_primer_name" || qual_name == "fwd_primer_seq") {
       ITERATE (list <CRef <CPCRPrimer> >, kt, (*jt)->GetForward().Get()) {
          if ( qual_name == "fwd_primer_name" && (*kt)->CanGetName() ) {
              strtmp = (*kt)->GetName();
              if (!strtmp.empty()) multi_vlus.push_back(strtmp);
          }
          if ( qual_name == "fwd_primer_seq" && (*kt)->CanGetSeq() ) {
              strtmp = (*kt)->GetSeq();
              if (!strtmp.empty()) multi_vlus.push_back(strtmp);
          }
       }
     }
     else {
        ITERATE (list <CRef <CPCRPrimer> >, kt, (*jt)->GetReverse().Get()) {
           if ( qual_name == "rev_primer_name" && (*kt)->CanGetName() ) {
               strtmp = (*kt)->GetName();
               if (!strtmp.empty()) multi_vlus.push_back(strtmp);
           }
           if ( qual_name == "rev_primer_seq" && (*kt)->CanGetSeq() ) {
               strtmp = (*kt)->GetSeq();
               if (!strtmp.empty()) multi_vlus.push_back(strtmp);
           }
        }
     }
   }
};


CRef <CClickableItem> CSeqEntry_test_on_quals :: MultiItem(const string& qual_name, const vector <string>& multi_list, const string& ext_desc, const string& setting_name)
{
    CRef <CClickableItem> c_sub (new CClickableItem);
    c_sub->setting_name = setting_name;
    c_sub->description 
         = "  " + GetHasComment(multi_list.size(), "source") + "multiple "
            + qual_name + " qualifiers" + ext_desc;
//    if (setting_name == GetName_sq_oncall()) {
    if (setting_name == GetName_bad()) {
          c_sub->item_list = multi_list;
    }
    return (c_sub);
};


void CSeqEntry_test_on_quals :: CheckForMultiQual(const string& qual_name, const CBioSource& biosrc, eMultiQual& multi_type, bool is_subsrc)
{
   vector <string> multi_qual_vlus;
   Str2Int  vlu_cnt;
   GetMultiQualVlus(qual_name, biosrc, multi_qual_vlus, is_subsrc);
   if (multi_qual_vlus.size() <= 1) multi_type = e_no_multi;
   else {
     ITERATE (vector <string>, it, multi_qual_vlus) {
       if (vlu_cnt.find(*it) != vlu_cnt.end()) vlu_cnt[*it]++;
       else vlu_cnt[*it] = 1;
     }
     if (vlu_cnt.size() == 1) multi_type = e_same;
     else {
        bool has_dup = false;
        ITERATE (Str2Int, it, vlu_cnt) {
          if (it->second > 1) {has_dup = true; break;}
   }
        multi_type = has_dup ? e_dup : e_all_dif;
     }
   }
};



void CSeqEntry_test_on_quals :: GetMultiQualVlus(const string& qual_name, const CBioSource& biosrc, vector <string>& multi_vlus, bool is_subsrc)
{
   ITERATE (vector <string>, it, thisInfo.no_multi_qual) if (qual_name == *it) return;

   if (is_subsrc) GetMultiSubSrcVlus(biosrc, qual_name, multi_vlus);
   if (qual_name == "fwd_primer_name" || qual_name == "fwd_primer_seq"
             || qual_name == "rev_primer_name" || qual_name == "rev_primer_seq" )
      GetMultiPrimerVlus(biosrc, qual_name, multi_vlus);
   else GetMultiOrgModVlus(biosrc, qual_name, multi_vlus);
};


// part of the C: GetSourceQualFromBioSource
string CTestAndRepData :: GetSrcQualValue(const CBioSource& biosrc, const string& qual_name, bool is_subsrc, const CString_constraint* str_cons)
{
 string ret_str(kEmptyStr);
 if (is_subsrc) ret_str = Get1SubSrcValue(biosrc, qual_name);
 else if (qual_name == "location") ret_str = thisInfo.genome_names[biosrc.GetGenome()];
 else if (qual_name == "taxname") 
            ret_str = biosrc.IsSetTaxname() ? biosrc.GetTaxname() : kEmptyStr;
 else if (qual_name == "common_name") 
        ret_str = biosrc.IsSetCommon() ? biosrc.GetCommon() : kEmptyStr;
 else if (qual_name == "lineage") 
        ret_str = biosrc.IsSetLineage() ? biosrc.GetLineage() : kEmptyStr;
 else if (qual_name == "division") 
        ret_str = biosrc.IsSetDivision() ? biosrc.GetDivision() : kEmptyStr;
 else if (qual_name == "dbxref") ret_str = "no ready yet";
 else if (qual_name == "taxid") {
   int tid = biosrc.GetOrg().GetTaxId();
   if (tid > 0) ret_str = NStr::IntToString(tid);
   else {
     if (biosrc.GetOrg().CanGetDb()) {
       ITERATE (vector <CRef <CDbtag> >, it, biosrc.GetOrg().GetDb()) {
         strtmp = (*it)->GetDb();
         if (NStr::FindNoCase(strtmp, "taxdb") != string::npos
               && strtmp.size() == ((string)"taxdb").size()
               && (*it)->GetTag().IsStr()) 
             ret_str = (*it)->GetTag().GetStr();
         }       
       }
   }
 }
 else if ( qual_name == "fwd_primer_name" || qual_name == "fwd_primer_seq"
           || qual_name == "rev_primer_name" || qual_name == "rev_primer_seq" )
 {
   if (biosrc.CanGetPcr_primers()) {
     ITERATE (list <CRef <CPCRReaction> >, jt, biosrc.GetPcr_primers().Get()) {
       if (qual_name == "fwd_primer_name" || qual_name == "fwd_primer_seq") {
         ITERATE (list <CRef <CPCRPrimer> >, kt, (*jt)->GetForward().Get()) {
          if ( qual_name == "fwd_primer_name" && (*kt)->CanGetName() ) {
              strtmp = (*kt)->GetName();
              if (!strtmp.empty()) { ret_str = strtmp; break; }
          }
          if ( qual_name == "fwd_primer_seq" && (*kt)->CanGetSeq() ) {
              strtmp = (*kt)->GetSeq();
              if (!strtmp.empty()) { ret_str = strtmp; break; }
          }
          if (!ret_str.empty()) break;
         }
       }
       else {
        ITERATE (list <CRef <CPCRPrimer> >, kt, (*jt)->GetReverse().Get()) {
           if ( qual_name == "rev_primer_name" && (*kt)->CanGetName() ) {
               strtmp = (*kt)->GetName();
               if (!strtmp.empty()) {ret_str = strtmp; break;}
           }
           if ( qual_name == "rev_primer_seq" && (*kt)->CanGetSeq() ) {
               strtmp = (*kt)->GetSeq();
               if (!strtmp.empty()) { ret_str = strtmp; break;}
           }
           if ( !ret_str.empty() ) break;
        }
       }
       if ( !ret_str.empty() ) break;
     }
   }
 }
 else ret_str = Get1OrgModValue(biosrc, qual_name);

 if (str_cons && !rule_check.DoesStringMatchConstraint(ret_str, str_cons)) 
       ret_str = kEmptyStr;
 
 return (ret_str);
};



void CSeqEntry_test_on_quals :: GetQualDistribute(Str2Ints& qual2src_idx, const vector <string>& desc_ls, const vector <CConstRef <CBioSource> >& src_ls, const vector <CConstRef <CObject> >& objs, const string& setting_name, unsigned pre_cnt, unsigned tot_cnt)
{
  int pre_idx, cur_idx, i;
  string qual_name, src_qual_vlu, src_txt;
  bool is_subsrc;
  size_t pos;

  pre_idx = (int)pre_cnt - 1;
  ITERATE (Str2Ints, it, qual2src_idx) {
     qual_name = it->first;
     is_subsrc = false;
     if ( (pos = qual_name.find("$subsrc")) != string::npos) {
        is_subsrc = true;
        qual_name = qual_name.substr(0, pos);
     }
     pre_idx = -1;
     ITERATE (vector <int>, jt, it->second) {
       cur_idx = *jt;
       if (cur_idx < pre_idx || cur_idx > (int)tot_cnt -1) {
            continue; 
       }
       if (pre_idx +1 != cur_idx) {  // have missing
          strtmp = "missing";
          for ( i = pre_idx+1; i < cur_idx; i++) {
               strtmp = qual_name + "$missing#" + desc_ls[i];
               thisInfo.test_item_list[setting_name].push_back(strtmp);
               strtmp = setting_name + "$" + qual_name + "#missing";
               thisInfo.test_item_objs[strtmp].push_back(objs[i]);
          }
       }
       src_qual_vlu= GetSrcQualValue(*src_ls[cur_idx], 
                                      qual_name, 
                                      is_subsrc);
       src_txt = desc_ls[cur_idx];
       strtmp = qual_name + "$" + src_qual_vlu + "#" + src_txt;
       thisInfo.test_item_list[setting_name].push_back(strtmp);
       strtmp = setting_name + "$" + qual_name + "#" + src_qual_vlu;
       thisInfo.test_item_objs[strtmp].push_back(objs[cur_idx]);

       // have multiple qualifiers? 
       eMultiQual multi_type;
       CheckForMultiQual(qual_name, *src_ls[cur_idx], multi_type, is_subsrc);
       switch (multi_type) {
          case e_same: strtmp = "multi_same"; 
                       break;
          case e_dup:  strtmp = "multi_dup"; 
                       break;
          case e_all_dif: strtmp = "multi_all_dif"; 
                          break;
          default: strtmp = kEmptyStr;
                   break;
       }
       if (!strtmp.empty()) {
          strtmp = qual_name + "$" + strtmp + "#" + src_txt;
          thisInfo.test_item_list[setting_name].push_back(strtmp);
          string strtmp2 = setting_name + "$" + qual_name + "#" + strtmp;
          thisInfo.test_item_objs[strtmp].push_back(objs[cur_idx]);
       }

       // biosrc_val_qualnm:
       strtmp = src_txt + "$" + src_qual_vlu + "#" + qual_name;
       thisInfo.test_item_list[setting_name + "_src"].push_back(strtmp);
       strtmp = setting_name + "_src$" + src_txt + "#" + src_qual_vlu;
       thisInfo.test_item_objs[strtmp].push_back(objs[cur_idx]);

       pre_idx = cur_idx;
     }

     strtmp = "$missing#";
     for (i = cur_idx + 1; i < (int)tot_cnt; i++) {
          strtmp = qual_name + "$missing#" + desc_ls[i];
          thisInfo.test_item_list[setting_name].push_back(strtmp);
          strtmp = setting_name + "$" + qual_name + "#missing";
          thisInfo.test_item_objs[strtmp].push_back(objs[i]); 
     }
  }
};

void CSeqEntry_test_on_quals :: ProcessUniQuals(const Str2Strs& qvlu2src, const string& setting_nm, const string& qual_nm, CRef <CClickableItem>& c_item)
{
   vector <string> uni_sub;
   vector <CConstRef <CObject> > uni_sub_obj;
   unsigned uni_cnt=0, sz;
   string qual_vlu;

   const vector <string>* vstr_p;
   ITERATE (Str2Strs, jt, qvlu2src) {
      qual_vlu = jt->first; 
      if ( qual_vlu == "missing" || qual_vlu == "multi_same"
              || qual_vlu == "multi_dup" || qual_vlu == "multi_all_dif") {
           continue;
      }
      sz = (jt->second).size();
      strtmp = setting_nm + "$" + qual_nm + "#" + qual_vlu;
      if (sz > 1) {
//	  vstr_p = (setting_nm == GetName_sq_oncall()) ? &(jt->second) : 0;
	  vstr_p = (setting_nm == GetName_bad()) ? &(jt->second) : 0;
          AddSubcategory(c_item, strtmp, vstr_p, "source", 
                     (string)"'" + qual_vlu + "' for " + qual_nm, 
                      e_HasComment, false, kEmptyStr, false, sz);
      }
      else  {
          uni_cnt ++;
          // if (setting_nm == GetName_sq_oncall()) {
          if (setting_nm == GetName_bad()) {
              uni_sub.push_back((jt->second)[0]);
              uni_sub_obj.push_back(thisInfo.test_item_objs[strtmp][0]);
          }
      }
   }

   if (uni_cnt) {
       strtmp = (string)"unique "
                  + (uni_cnt >1 ? "values" : "value") + " for " + qual_nm;
       // if (setting_nm == GetName_sq_oncall()) {
       if (setting_nm == GetName_bad()) {
           CRef <CClickableItem> c_sub (new CClickableItem);
           c_sub->setting_name = setting_nm;
           c_sub->item_list = uni_sub;
           c_sub->obj_list = uni_sub_obj;
           c_sub->description 
                    = GetHasComment(c_sub->item_list.size(), "source") + strtmp;
/*
           copy(c_sub->item_list.begin(), c_sub->item_list.end(),
                         back_inserter( c_item->item_list ));
           copy(c_sub->obj_list.begin(), c_sub->obj_list.end(),
                         back_inserter( c_item->obj_list ));
*/
           c_item->subcategories.push_back(c_sub);
       }
       else {
           AddSubcategory(c_item, setting_nm, 0, "source", strtmp,
                   e_HasComment, false, kEmptyStr, false, uni_cnt); 
       }
       uni_sub.clear();
       uni_sub_obj.clear();
   }
};

void CSeqEntry_test_on_quals :: GetReport_quals(CRef <CClickableItem>& c_item, const string& setting_name)
{
   Str2Strs qnm2qvlu_src;
   GetTestItemList(c_item->item_list, qnm2qvlu_src);
   c_item->item_list.clear();

   bool all_same, all_unique, all_present;
   bool multi_same, multi_dup, multi_all_dif;
   unsigned multi_type_cnt;
   string qual_nm, qual_vlu;
   const vector <string>* vstr_p = 0;

   Str2Strs qvlu2src;
   CRef <CClickableItem> c_main (new CClickableItem);
   if (setting_name == GetName_bad()) { // GetName_sq_oncall()) 
     c_main = c_item;
     c_main->setting_name = setting_name;
     c_main->description = "Source Qualifer Report";
     c_main->item_list.clear();
     if (thisInfo.expand_srcqual_report) {
        c_main->expanded = true;
     }
     c_item = CRef <CClickableItem> (new CClickableItem);
     c_item->setting_name = setting_name;
   }

   ITERATE (Str2Strs, it, qnm2qvlu_src) {
     qual_nm = it->first;
     qvlu2src.clear();
     GetTestItemList(it->second, qvlu2src, "#");
     if (qvlu2src.size() == 1 && qvlu2src.begin()->second.size() > 1) {
           all_same = true;
     }
     else {
        all_same = false;
        all_unique = true;
        ITERATE (Str2Strs, jt, qvlu2src) {
          if ( jt->first != "missing"
                   && jt->first != "multi_same"
                   && jt->first != "multi_dup"
                   && jt->first != "multi_all_dif"
                   && jt->second.size() > 1) {
              all_unique = false; 
              break;
          }
        }
     } 
     
     all_present = (qvlu2src.find("missing") == qvlu2src.end());

     multi_same = (qvlu2src.find("multi_same") != qvlu2src.end());
     multi_dup = (qvlu2src.find("multi_dup") != qvlu2src.end());
     multi_all_dif = (qvlu2src.find("multi_all_dif") != qvlu2src.end());

     multi_type_cnt = 0;
     multi_type_cnt += multi_same ? 1 : 0;
     multi_type_cnt += multi_dup ? 1 : 0;
     multi_type_cnt += multi_all_dif? 1 : 0;

     if (qual_nm == "note") {
         qual_nm += "-subsrc";
     }
     c_item->description = qual_nm;
     unsigned sz;
     if (all_present) {
         c_item->description += " (all present, ";
         if (all_same) {
            qual_vlu = qvlu2src.begin()->first;
            c_item->description += "all same";
            strtmp
              = setting_name + "$" + qual_nm + "#" + qvlu2src.begin()->first;
            AddSubcategory(c_item, 
                           strtmp,
                           &(qvlu2src.begin()->second), //0, 
                           "source",
                           "'" + qual_vlu + "' for " + qual_nm, 
                           e_HasComment, 
                           false, 
                           kEmptyStr, 
                           false, 
                           (qvlu2src.begin()->second).size());
         }
         else if (all_unique) {
            c_item->description += "all unique";
            if (setting_name == GetName_bad()) { //GetName_sq_oncall())
               ITERATE (Str2Strs, jt, qvlu2src) { 
                  if ( jt->first != "missing" 
                            && jt->first != "multi_same"
                            && jt->first != "multi_dup" 
                            && jt->first != "multi_all_dif") {
                      c_item->item_list.push_back((jt->second)[0]);
                      strtmp = setting_name + "$" + qual_nm + "#" + jt->first;
                      c_item->obj_list.push_back(
                                           thisInfo.test_item_objs[strtmp][0]);
                  }
               }
            }
            else {
/*
               strtmp = (string)"unique " 
                           + (qvlu2src.size() > 1 ? "values" : "value")
                           + " for " + qual_nm;
               AddSubcategory(c_item, setting_name, 0, "source", strtmp,
                     e_HasComment, true, kEmptyStr, false, qvlu2src.size());
*/
            }
         }
         else {
           c_item->description += "some duplicate";
           ProcessUniQuals(qvlu2src, setting_name, qual_nm, c_item);
         }
     }
     else {
        c_item->description += " (some missing, ";
        vstr_p 
            = (setting_name == GetName_bad()) ? &qvlu2src["missing"] :0;
/*
        vstr_p 
            = (setting_name == GetName_sq_oncall()) ? &qvlu2src["missing"] :0;
*/
        strtmp = setting_name + "$" + qual_nm + "#missing";
        AddSubcategory(c_item, strtmp, vstr_p, "source", "missing " + qual_nm, 
           e_IsComment, false, kEmptyStr, false, qvlu2src["missing"].size());
        if (all_same) {
            c_item->description += "all same";
            // vstr_p = (setting_name == GetName_sq_oncall()) ? 
            vstr_p = (setting_name != GetName_bad()) ? 
                        &(qvlu2src.begin()->second) : 0;
            qual_vlu = qvlu2src.begin()->first;
            strtmp = setting_name + "$" + qual_nm + "#" + qual_vlu;
            AddSubcategory(c_item, strtmp, vstr_p, "source", 
                 " '" + qual_vlu + "' for " + qual_nm, e_HasComment,
                 true, kEmptyStr, false, (qvlu2src.begin()->second).size());
        }
        else if (all_unique) {
           c_item->description += "all unique"; 
           ProcessUniQuals(qvlu2src, setting_name, qual_nm, c_item);
        }
        else {
           c_item->description += "some duplicate";
           ProcessUniQuals(qvlu2src, setting_name, qual_nm, c_item);
        }
     }

     //multiple
     sz = qvlu2src["multi_same"].size() 
              + qvlu2src["multi_dup"].size() + qvlu2src["multi_all_dif"].size();
     string ext_desc;
     if ( multi_type_cnt) {
          c_item->description += ", some multi";
          CRef <CClickableItem> c_sub (new CClickableItem);
          c_sub->setting_name = setting_name;
          if (multi_type_cnt == 1) {
              ext_desc = multi_same ? ", same_value" 
                              : (multi_dup ? ", some dupplicates" : kEmptyStr);
          }
          else {
              ext_desc = kEmptyStr;
              if (multi_same) {
                 c_sub->subcategories.push_back(MultiItem(qual_nm, 
                                                         qvlu2src["multi_same"],
                                                          ", same value", 
                                                          setting_name));
              }
              if (multi_dup) {
                 c_sub->subcategories.push_back(MultiItem( qual_nm, 
                                                         qvlu2src["multi_dup"], 
                                                         ", some duplicates", 
                                                         setting_name));
              }
              if (multi_all_dif) {
                 c_sub->subcategories.push_back(MultiItem(qual_nm, 
                                                      qvlu2src["multi_all_dif"],
                                                      kEmptyStr, 
                                                      setting_name));
              }
          }
          c_sub->description 
               = GetHasComment(sz, "source") 
                   + "multiple " + qual_nm + " qualifiers" + ext_desc;
          c_item->subcategories.push_back(c_sub);
     }
     
     c_item->description += ")";
     if (setting_name == GetName_bad()) { // GetName_sq_oncall()) 
            c_main->subcategories.push_back(c_item);
            c_item = CRef <CClickableItem> (new CClickableItem);
            c_item->setting_name = setting_name;
     }
     else {
        c_item.Reset(new CClickableItem);
        c_item->setting_name = setting_name;
        c_item->item_list.clear();
        thisInfo.disc_report_data.push_back(c_item);
     }
   }  // it

 // biosrc has 2 qual with same value: biosrc2qualvlu_nm
    string biosrc_nm, quals, src_qual_vlu;
    unsigned cnt = 0;
    vector < CRef <CClickableItem> > sub;
    Str2Strs biosrc2qualvlu_nm, vlu2nms;
    GetTestItemList(thisInfo.test_item_list[setting_name + "_src"], 
                    biosrc2qualvlu_nm);
    thisInfo.test_item_list[setting_name + "_src"].clear();
    bool has_same_vu_mul_quals_src = false;
    ITERATE (Str2Strs, it, biosrc2qualvlu_nm) {
         biosrc_nm = it->first;
         GetTestItemList(it->second, vlu2nms);
         if (!has_same_vu_mul_quals_src) {
             ITERATE (Str2Strs, jt, vlu2nms) {
                if (jt->second.size() > 1) {
                   has_same_vu_mul_quals_src = true;
                   c_item.Reset(new CClickableItem);
                   thisInfo.disc_report_data.push_back(c_item);
                   break;
                }
             }
         }
         ITERATE (Str2Strs, jt, vlu2nms) {
           src_qual_vlu = jt->first;
           if (jt->second.size() > 1) {
              cnt ++;
              quals = NStr::Join(jt->second, ", ");
              CRef <CClickableItem> c_sub (new CClickableItem);
              c_sub->setting_name = setting_name;
              c_sub->description 
                   = "BioSource has value '" + jt->first 
                        + "' for these qualifiers: " + quals;
              c_item->subcategories.push_back(c_sub);
              c_item->item_list.push_back(biosrc_nm);
              strtmp =setting_name +"_src$" + biosrc_nm + "#" + src_qual_vlu;
              c_item->obj_list.push_back(thisInfo.test_item_objs[strtmp][0]);
              if (setting_name == GetName_bad()) { // GetName_sq_oncall())
                   c_sub->item_list.push_back(biosrc_nm);
                   c_sub->obj_list = thisInfo.test_item_objs[strtmp];
              }
           }
         }
         vlu2nms.clear();
    }
 
    if (cnt) {
         c_item->setting_name = setting_name;
         c_item->description = GetHasComment(cnt, "source") 
                                 + "two or more qualifiers with the same value";
    }
};


bool CSeqEntry_test_on_quals :: GetQual2SrcIdx(const vector <CConstRef <CBioSource> >& src_ls, const vector <string>& desc_ls, Str2Ints& qual2src_idx, unsigned pre_cnt, unsigned tot_cnt)
{
   bool fwd_nm, fwd_seq, rev_nm, rev_seq;
   unsigned i = pre_cnt ? pre_cnt-1: 0;
   bool has_newqual = false;
   for (i; i < tot_cnt; i++) { 
      CBioSource* it = (CBioSource*)(src_ls[i].GetPointer());
      if ( it->IsSetTaxname() ) {
           has_newqual = true;
           qual2src_idx["taxname"].push_back(i);
      }
      if ( it->GetOrg().GetTaxId() ) {
          has_newqual = true;
          qual2src_idx["taxid"].push_back(i);
      }

      // add subtypes & orgmods
      if ( it->CanGetSubtype() ) {
         has_newqual = true;
         ITERATE (list <CRef <CSubSource> >, jt, it->GetSubtype()) {
           strtmp = (*jt)->GetSubtypeName((*jt)->GetSubtype(), 
                                           CSubSource::eVocabulary_insdc);
           qual2src_idx[strtmp + "$subsrc"].push_back(i);
         }
      }

      if ( it->IsSetOrgname() && it->GetOrgname().CanGetMod() ) {
         has_newqual = true;
         ITERATE (list <CRef <COrgMod> >, jt, it->GetOrgname().GetMod() ) {
            strtmp = (*jt)->GetSubtypeName((*jt)->GetSubtype(), 
                                            COrgMod::eVocabulary_insdc);
            if (strtmp != "old_name" 
                   && strtmp != "old_lineage" && strtmp != "gb_acronym"
                   && strtmp != "gb_anamorph" && strtmp != "gb_synonym") {
                qual2src_idx[strtmp].push_back(i);
            }
         }
      }
   
      // add PCR primers
      if ( it->CanGetPcr_primers() ) {
         fwd_nm = fwd_seq = rev_nm = rev_seq = false;
         ITERATE (list <CRef <CPCRReaction> >, jt, it->GetPcr_primers().Get()) {
            if ( !fwd_nm && !fwd_seq && (*jt)->CanGetForward() ) {
              ITERATE (list <CRef <CPCRPrimer> >, kt,(*jt)->GetForward().Get()){
                 if ( !fwd_nm && (*kt)->CanGetName() ) {
                    strtmp = (*kt)->GetName();
                    if (!strtmp.empty()) {
                        qual2src_idx["fwd_primer_name"].push_back(i);
                        has_newqual = fwd_nm = true;
                    }
                 }
                 if ( !fwd_seq && (*kt)->CanGetSeq() ) {
                    strtmp = (*kt)->GetSeq();
                    if (!strtmp.empty()) {
                       has_newqual = fwd_seq = true;
                       qual2src_idx["fwd_primer_seq"].push_back(i);
                    }
                 }
                 if (fwd_nm && fwd_seq) break; 
              }
            }
            if ( !rev_nm && !rev_seq && (*jt)->CanGetReverse() ) {
              ITERATE (list <CRef <CPCRPrimer> >, kt,(*jt)->GetReverse().Get()){
                 if (!rev_nm && (*kt)->CanGetName() ) {
                    strtmp = (*kt)->GetName();
                    if (!strtmp.empty()) {
                        has_newqual = rev_nm = true;
                        qual2src_idx["rev_primer_name"].push_back(i);
                    }
                 }
                 if (!rev_seq && (*kt)->CanGetSeq() ) {
                    strtmp = (*kt)->GetSeq();
                    if (!strtmp.empty()) {
                        has_newqual = rev_seq = true;
                        qual2src_idx["rev_primer_seq"].push_back(i);
                    }
                 }
                 if (rev_nm && rev_seq) {
                     break;
                 }
              }
            }
            if (fwd_nm && fwd_seq && rev_nm && rev_seq) {
                break;
            }
         }
      }
    
      // genomic
      if (it->GetGenome() != CBioSource::eGenome_unknown) {
         has_newqual = true;
         qual2src_idx["location"].push_back(i);
      }
   } 
   return has_newqual;
};


static vector <string> comb_desc_ls;  // global in C
static vector <CConstRef <CBioSource> > comb_src_ls;  // global in C
static vector <CConstRef <CObject> > objs;
static Str2Ints comb_qual2src_idx;
void CSeqEntry_test_on_quals :: TestOnObj(const CSeq_entry& seq_entry)
{
   if (thisTest.is_Quals_run) return;
   thisTest.is_Quals_run = true;

   vector <string> this_desc_ls;
/* why is the 'local' needed?
   vector <CConstRef <CObject> > this_src_ls;  // local in C
   Str2Ints this_qual2src_idx;  // local in C
*/
 
   bool run_sq = (thisTest.tests_run.find(GetName_sq()) != end_it);
   bool run_oncall= (thisTest.tests_run.find(GetName_sq_oncall())!=end_it);
   bool run_bad = (thisTest.tests_run.find(GetName_bad()) != end_it);

   unsigned pre_cnt = comb_src_ls.size(); 
   string desc;
   ITERATE (vector <const CSeq_feat*>, it, biosrc_feat) {
     desc = GetDiscItemText(**it);
     comb_desc_ls.push_back(desc); // for combine_seqentry_reports
     const CBioSource& biosrc = (*it)->GetData().GetBiosrc();
     comb_src_ls.push_back(CConstRef <CBioSource> (&biosrc));  // for combine_seqentry_reports
     objs.push_back(CConstRef <CObject> (*it));
/*
     this_desc_ls.push_back(desc);
     this_src_ls.push_back(CConstRef <CObject> (&biosrc));
*/
   }

   unsigned i=0;
   ITERATE (vector <const CSeqdesc*>, it, biosrc_seqdesc) {
      desc = GetDiscItemText(**it, *(biosrc_seqdesc_seqentry[i]));
      comb_desc_ls.push_back(desc);
      this_desc_ls.push_back(desc);
      const CBioSource& biosrc = (*it)->GetSource();
      comb_src_ls.push_back(CConstRef <CBioSource> (&biosrc));
      objs.push_back(CConstRef <CObject>(*it));
//      this_src_ls.push_back(CConstRef <CBioSource> (&biosrc));
      i++;
   }
   unsigned tot_cnt = comb_src_ls.size();

   bool has_newqual = GetQual2SrcIdx(comb_src_ls, 
                                     comb_desc_ls, 
                                     comb_qual2src_idx, 
                                     pre_cnt, 
                                     tot_cnt);
   // DISC_SRC_QUAL_PROBLEM
   if (run_bad) { 
           thisInfo.test_item_list[GetName_bad()].push_back("yes");
/*
        GetQualDistribute(comb_qual2src_idx, 
                          comb_desc_ls, 
                          comb_src_ls, 
                          objs,
                          GetName_bad(), 
                          pre_cnt, 
                          tot_cnt);
*/
   }

   strtmp = run_sq? GetName_sq() : GetName_sq_oncall();
   if (has_newqual && ( run_sq || run_oncall)
                   && ( thisInfo.test_item_list.find(strtmp)
                            == thisInfo.test_item_list.end())) {
       if (run_sq) {
             thisInfo.test_item_list[GetName_sq()].push_back("yes");
       }
       if (run_oncall) {
           thisInfo.test_item_list[GetName_sq_oncall()].push_back("yes");
       }
   }
};


void CSeqEntry_DISC_SRC_QUAL_PROBLEM :: GetReport(CRef <CClickableItem>& c_item)
{
   thisInfo.test_item_list[GetName()].clear();
   GetQualDistribute(comb_qual2src_idx, comb_desc_ls, comb_src_ls, objs,
                                              GetName(), 0, comb_src_ls.size());
   c_item->item_list = thisInfo.test_item_list[GetName()];
   GetReport_quals(c_item, GetName()); //original code
};


void CSeqEntry_DISC_SOURCE_QUALS_ASNDISC :: GetReport(CRef <CClickableItem>& c_item)
{
   thisInfo.test_item_list[GetName()].clear();
   GetQualDistribute(comb_qual2src_idx, comb_desc_ls, comb_src_ls, objs,
                                              GetName(), 0, comb_src_ls.size());
   c_item->item_list = thisInfo.test_item_list[GetName()];
   GetReport_quals(c_item, GetName());
};


void CSeqEntry_DISC_SOURCE_QUALS_ASNDISC_oncaller :: GetReport(CRef <CClickableItem>& c_item)
{
   thisInfo.test_item_list.clear();
   GetQualDistribute(comb_qual2src_idx, comb_desc_ls, comb_src_ls, objs, 
                                  GetName(), 0, comb_src_ls.size());
   c_item->item_list = thisInfo.test_item_list[GetName()];
   GetReport_quals(c_item, GetName());
};



bool CSeqEntry_test_on_biosrc_orgmod :: HasConflict(const list <CRef <COrgMod> >& mods, const string& subname_rest, const COrgMod::ESubtype& check_type, const string& check_head)
{
  size_t pos;
  unsigned h_size = check_head.size();
  ITERATE (list <CRef <COrgMod> >, it, mods) {
     strtmp = (*it)->GetSubname();
     if ( (*it)->GetSubtype() == check_type && strtmp.substr(0, h_size) == check_head ) {
        if ( (pos = strtmp.find(';')) != string::npos) {
           if ( CTempString(strtmp).substr(h_size, pos-h_size) == subname_rest ) 
               return false;  // need to test
        }
        else if (CTempString(strtmp).substr(h_size) == subname_rest) return false;
     }
  }
  return true;
};


bool CSeqEntry_test_on_biosrc_orgmod :: IsStrainInCultureCollectionForBioSource(const CBioSource& biosrc, const string& strain_head, const string& culture_head)
{
  COrgMod::ESubtype check_type;
  string check_head(kEmptyStr), subname_rest(kEmptyStr);
  const list < CRef < COrgMod > >& mods = biosrc.GetOrgname().GetMod();
  ITERATE (list <CRef <COrgMod> >, it, mods) {
    if ( (*it)->GetSubtype() == COrgMod::eSubtype_strain
                  && (*it)->GetSubname().substr(0, strain_head.size()) == strain_head) {
       check_type = COrgMod::eSubtype_culture_collection;
       check_head = culture_head;
       subname_rest = CTempString((*it)->GetSubname()).substr(strain_head.size());
    }
    else if ( (*it)->GetSubtype() == COrgMod::eSubtype_culture_collection
                && (*it)->GetSubname().substr(0, culture_head.size()) == culture_head) {
       check_type = COrgMod::eSubtype_strain;
       check_head = strain_head;
       subname_rest = CTempString((*it)->GetSubname()).substr(culture_head.size());
    }

    if (!check_head.empty()&& !subname_rest.empty() 
                          && HasConflict(mods, subname_rest, check_type, check_head) )
         return true;
  }

  return false;
};


void CSeqEntry_test_on_biosrc_orgmod :: BacterialTaxShouldEndWithStrain(const CBioSource& biosrc, const string& desc)
{
    CConstRef <CObject> src_ref (&biosrc);
    size_t pos;
    if ( HasLineage(biosrc, "Bacteria") && biosrc.GetOrg().CanGetTaxname()) {
      string taxname = biosrc.GetOrg().GetTaxname();
      vector <string> arr;
      GetOrgModValues(biosrc, COrgMod::eSubtype_strain, arr);
      if (taxname.empty() && !arr.empty()) {
          thisInfo.test_item_list[GetName_mism()].push_back(desc);
          thisInfo.test_item_objs[GetName_mism()].push_back(src_ref);
      } 
      else if (!taxname.empty()) {
         ITERATE (vector <string>, it, arr) {
            pos = taxname.find(*it);
            if (pos == string::npos) {
               thisInfo.test_item_list[GetName_mism()].push_back(desc);
               thisInfo.test_item_objs[GetName_mism()].push_back(src_ref);
               break;
            }
            else {
               strtmp 
                   = CTempString(taxname).substr(taxname.size() - (*it).size());
               if (strtmp != *it) {
                  thisInfo.test_item_list[GetName_mism()].push_back(desc);
                  thisInfo.test_item_objs[GetName_mism()].push_back(src_ref);
                  break;
               }
            }
         }
      }
    }
};


void CSeqEntry_test_on_biosrc_orgmod :: BiosrcHasConflictingStrainAndCultureCollectionValues(const CBioSource& biosrc, const string& desc, CConstRef <CObject> obj_ref)
{
   bool has_match = false, has_conflict = false;
   string extra_str(kEmptyStr), subnm1, subnm2;

   const list < CRef < COrgMod > >& mods = biosrc.GetOrgname().GetMod();
   ITERATE (list < CRef < COrgMod > >, it, mods) {
     if ((*it)->GetSubtype() == COrgMod :: eSubtype_strain) {
       ITERATE (list < CRef < COrgMod > >, jt, mods) {
         if ( (*jt)->GetSubtype() == COrgMod :: eSubtype_culture_collection) {
            subnm1 = (*it)->GetSubname();
            subnm2 = (*jt)->GetSubname();
            RmvChar(subnm1, ": ");
            RmvChar(subnm2, ": ");
            if (subnm1 == subnm2) {
                   has_match = true;
                   break;
            }
            else {
               extra_str = "        " 
                            + (*it)->GetSubtypeName((*it)->GetSubtype(),
                                                    COrgMod::eVocabulary_insdc)
                            + ": " + (*it)->GetSubname() + "\n        "
                            + (*jt)->GetSubtypeName((*jt)->GetSubtype(),
                                                    COrgMod::eVocabulary_insdc)
                            + ": " + (*jt)->GetSubname();
               has_conflict = true;
            }
         }
       };
       if (has_match) {
           break;
       }
     }
   }
   if (has_conflict && !has_match) {
     thisInfo.test_item_list[GetName_cul()].push_back(desc + "\n" + extra_str); 
     thisInfo.test_item_objs[GetName_cul()].push_back(obj_ref);
   }
};

void CSeqEntry_test_on_biosrc_orgmod :: RunTests(const CBioSource& biosrc, const string& desc, CConstRef <CObject> obj_ref)
{
  // DUP_DISC_ATCC_CULTURE_CONFLICT
  if (m_run_atcc 
        && IsStrainInCultureCollectionForBioSource(biosrc, "ATCC ", "ATCC:")) {
      thisInfo.test_item_list[GetName_atcc()].push_back(desc);
      thisInfo.test_item_objs[GetName_atcc()].push_back(obj_ref);
  }
         
  // DUP_DISC_CBS_CULTURE_CONFLICT
  if (m_run_cbs 
           && IsStrainInCultureCollectionForBioSource(biosrc, "CBS ", "CBS:")) {
      thisInfo.test_item_list[GetName_cbs()].push_back(desc);
      thisInfo.test_item_objs[GetName_cbs()].push_back(obj_ref);
  }

  // DISC_BACTERIAL_TAX_STRAIN_MISMATCH
  if (m_run_mism) {
         BacterialTaxShouldEndWithStrain(biosrc, desc);
  }

  // ONCALLER_STRAIN_CULTURE_COLLECTION_MISMATCH
  if (m_run_cul) {
    BiosrcHasConflictingStrainAndCultureCollectionValues(biosrc, desc, obj_ref);
  }

  // DISC_BACTERIA_MISSING_STRAIN
  if (m_run_sp && HasMissingBacteriaStrain(biosrc)) {
       thisInfo.test_item_list[GetName_sp_strain()].push_back(desc);
       thisInfo.test_item_objs[GetName_sp_strain()].push_back(obj_ref);
  }

  // DISC_METAGENOME_SOURCE
  if (m_run_meta 
          && IsOrgModPresent(biosrc, COrgMod::eSubtype_metagenome_source)) {
      thisInfo.test_item_list[GetName_meta()].push_back(desc); 
      thisInfo.test_item_objs[GetName_meta()].push_back(obj_ref); 
  }

  // ONCALLER_CHECK_AUTHORITY
  bool has_tax = false;
  if (biosrc.IsSetTaxname() && !biosrc.GetTaxname().empty()) {
      has_tax = true;
      if (m_run_auth && DoAuthorityAndTaxnameConflict(biosrc)) {
          thisInfo.test_item_list[GetName_auth()].push_back(desc);
          thisInfo.test_item_objs[GetName_auth()].push_back(obj_ref);
      }
  }

  // ONCALLER_MULTIPLE_CULTURE_COLLECTION
  if (m_run_mcul) {
      string cul_vlus;
      if (HasMultipleCultureCollection(biosrc, cul_vlus)) {
         thisInfo.test_item_list[GetName_mcul()].push_back(desc +"\n\r\t" +cul_vlus);
         thisInfo.test_item_objs[GetName_mcul()].push_back(obj_ref);
      }
  }

  // DISC_HUMAN_HOST
  if (m_run_human) {
     vector <string> arr;
     GetOrgModValues(biosrc, COrgMod::eSubtype_nat_host, arr);
     ITERATE (vector <string>, it, arr) {
       if ( DoesStringContainPhrase(*it, "human", false, true) ) {
         thisInfo.test_item_list[GetName_human()].push_back(desc); 
         thisInfo.test_item_objs[GetName_human()].push_back(obj_ref); 
         break;
       }
     }
     arr.clear();
  }

  // TEST_UNNECESSARY_ENVIRONMENTAL
  if (m_run_env && has_tax && HasUnnecessaryEnvironmental(biosrc)) {
      thisInfo.test_item_list[GetName_env()].push_back(desc);
      thisInfo.test_item_objs[GetName_env()].push_back(obj_ref);
   }

  // TEST_AMPLIFIED_PRIMERS_NO_ENVIRONMENTAL_SAMPLE
  if (m_run_amp && AmpPrimersNoEnvSample(biosrc)) {
     thisInfo.test_item_list[GetName_amp()].push_back(desc);
     thisInfo.test_item_objs[GetName_amp()].push_back(obj_ref);
  }
};


bool CSeqEntry_test_on_biosrc_orgmod :: AmpPrimersNoEnvSample(const CBioSource& biosrc)
{
   string note("amplified with species-specific primers");
   vector <string> arr;
   if (IsSubSrcPresent(biosrc, CSubSource::eSubtype_environmental_sample)) return false;
   GetSubSrcValues(biosrc, CSubSource::eSubtype_other, arr);
   GetOrgModValues(biosrc, COrgMod::eSubtype_other, arr);
   ITERATE (vector <string>, it, arr)
      if (NStr::FindNoCase(*it, note) != string::npos) return true;

   return false;                     
};


void CSeqEntry_TEST_AMPLIFIED_PRIMERS_NO_ENVIRONMENTAL_SAMPLE :: GetReport(CRef <CClickableItem>& c_item)
{
   c_item->obj_list = thisInfo.test_item_objs[GetName()];
   c_item->description = GetHasComment(c_item->item_list.size(), "biosource") 
     + "'amplified with species-specific primers' note but no environmental-sample qualifier.";
};


bool CSeqEntry_test_on_biosrc_orgmod :: HasUnnecessaryEnvironmental(const CBioSource& biosrc)
{
   bool found = false, has_note = false, has_metagenomic = false;

   if (!biosrc.CanGetSubtype()) return false;
   found = IsSubSrcPresent(biosrc, CSubSource::eSubtype_environmental_sample);
   vector <string> arr;
   GetSubSrcValues(biosrc, CSubSource::eSubtype_other, arr);
   ITERATE (vector <string>, it, arr)
     if (NStr::FindNoCase(*it, "amplified with species-specific primers")!=string::npos){
          has_note = true; break;
     }
   has_metagenomic = IsSubSrcPresent(biosrc, CSubSource::eSubtype_metagenomic);

   if (!found || has_note) return false;
   strtmp = biosrc.GetTaxname();
   ITERATE (vector <string>, it, thisInfo.taxnm_env) 
       if (NStr::FindNoCase(strtmp, *it) != string::npos) return false;
   arr.clear();
   GetOrgModValues(biosrc, COrgMod::eSubtype_other, arr);
   ITERATE (vector <string>, it, arr)
     if ( NStr::FindNoCase(*it, "amplified with species-specific primers")!=string::npos)
             return false;
   return true;
};


void CSeqEntry_TEST_UNNECESSARY_ENVIRONMENTAL :: GetReport(CRef <CClickableItem>& c_item)
{
  c_item->obj_list = thisInfo.test_item_objs[GetName()];
  c_item->description = GetHasComment(c_item->item_list.size(), "biosource") 
                        + "unnecessary environmental qualifier";
};


void CSeqEntry_DISC_HUMAN_HOST :: GetReport(CRef <CClickableItem>& c_item)
{
   unsigned cnt = c_item->item_list.size();
   c_item->obj_list = thisInfo.test_item_objs[GetName()];
   c_item->description
     = GetHasComment(cnt, "organism") + "'human' host qualifier" + (cnt >1 ? "s" : "");
};


bool CSeqEntry_test_on_biosrc_orgmod :: HasMultipleCultureCollection (const CBioSource& biosrc, string& cul_vlus)
{
  cul_vlus = kEmptyStr; 
  string cul_nm = COrgMod::ENUM_METHOD_NAME(ESubtype)()
                        ->FindName(COrgMod::eSubtype_culture_collection, false) 
                    + ":";
    
  vector <string> arr;
  GetOrgModValues(biosrc, COrgMod::eSubtype_culture_collection, arr);
  ITERATE (vector <string>, it, arr) {
    cul_vlus += cul_nm + (*it) + "\n\r\t";
  }
  if (arr.size() > 1) return true; 
  return false;
};


void CSeqEntry_ONCALLER_MULTIPLE_CULTURE_COLLECTION :: GetReport(CRef <CClickableItem>& c_item)
{
  c_item->obj_list = thisInfo.test_item_objs[GetName()];
  c_item->description = GetHasComment(c_item->item_list.size(), "organism") 
                       + "multiple culture-collection qualifiers.";
};


bool CSeqEntry_test_on_biosrc_orgmod :: DoAuthorityAndTaxnameConflict(const CBioSource& biosrc)
{
  vector <string> auty_strs;
  GetOrgModValues(biosrc, COrgMod::eSubtype_authority, auty_strs);
  if (auty_strs.empty()) return false;

  strtmp = biosrc.GetTaxname();
  size_t pos = strtmp.find(' ');
  unsigned len;
  if (pos != string::npos) pos = CTempString(strtmp).substr(pos).find(' ');
  if (pos == string::npos) len = strtmp.size();
  else len = pos;
  ITERATE (vector <string>, it, auty_strs) {
    if ( (*it).size() < len || (*it).substr(0, len) == strtmp) return true;
  }
  return false; 
};


void CSeqEntry_ONCALLER_CHECK_AUTHORITY :: GetReport(CRef <CClickableItem>& c_item)
{
  c_item->obj_list = thisInfo.test_item_objs[GetName()];
  c_item->description = GetHasComment(c_item->item_list.size(), "biosource") 
                          + "taxname/authority conflict";
};


void CSeqEntry_DISC_METAGENOME_SOURCE :: GetReport(CRef <CClickableItem>& c_item)
{
  c_item->obj_list = thisInfo.test_item_objs[GetName()];
  c_item->description = GetHasComment(c_item->item_list.size(), "biosource") 
                         + "metagenome_source qualifier";
};


bool CSeqEntry_test_on_biosrc_orgmod :: HasMissingBacteriaStrain(const CBioSource& biosrc)
{
   if (!HasLineage(biosrc, "Bacteria")) return false;
   string tax_nm(biosrc.IsSetTaxname() ? biosrc.GetTaxname() : kEmptyStr);
   size_t pos;
   if (tax_nm.empty() || NStr::FindNoCase(tax_nm, "enrichment culture clone") != string::npos
                      || (pos = tax_nm.find(" sp. ")) == string::npos) 
       return false;
   tax_nm = CTempString(tax_nm).substr(pos+5);
   if (tax_nm.empty() 
           || (tax_nm.find('(') != string::npos && tax_nm[tax_nm.size()-1] == ')')) 
            return false;
   ITERATE (list <CRef <COrgMod> >, it, biosrc.GetOrgname().GetMod()) {
       if ( (*it)->GetSubtype() == COrgMod :: eSubtype_strain && (*it)->GetSubname() == tax_nm)
            return false;
   }
   return true;
};


void CSeqEntry_DISC_BACTERIA_MISSING_STRAIN :: GetReport(CRef <CClickableItem>& c_item)
{
  c_item->obj_list = thisInfo.test_item_objs[GetName()];
  c_item->description
     = GetHasComment(c_item->item_list.size(), "bacterial biosource")
       + "taxname 'Genus sp. strain' but no strain";
};


void CSeqEntry_test_on_biosrc_orgmod :: TestOnObj(const CSeq_entry& seq_entry)
{
   if (thisTest.is_Biosrc_Orgmod_run) return;
   thisTest.is_Biosrc_Orgmod_run = true;

   m_run_mism = (thisTest.tests_run.find(GetName_mism()) != end_it);
   m_run_cul = (thisTest.tests_run.find(GetName_cul()) != end_it);
   m_run_cbs = (thisTest.tests_run.find(GetName_cbs()) != end_it);
   m_run_atcc = (thisTest.tests_run.find(GetName_atcc()) != end_it);
   m_run_sp = (thisTest.tests_run.find(GetName_sp_strain()) != end_it);
   m_run_meta = (thisTest.tests_run.find(GetName_meta()) != end_it);
   m_run_auth = (thisTest.tests_run.find(GetName_auth()) != end_it);
   m_run_mcul = (thisTest.tests_run.find(GetName_mcul()) != end_it);
   m_run_human = (thisTest.tests_run.find(GetName_human()) != end_it);
   m_run_env = (thisTest.tests_run.find(GetName_env()) != end_it);
   m_run_amp = (thisTest.tests_run.find(GetName_amp()) != end_it);

   string desc;
   ITERATE (vector <const CSeq_feat*>, it, biosrc_orgmod_feat) {
     RunTests((*it)->GetData().GetBiosrc(), 
               GetDiscItemText(**it), 
               CConstRef <CObject>(*it));
   }

   unsigned i=0;
   ITERATE (vector <const CSeqdesc*>, it, biosrc_orgmod_seqdesc) {
      desc = GetDiscItemText(**it, *(biosrc_orgmod_seqdesc_seqentry[i]));
      RunTests((*it)->GetSource(), desc, CConstRef <CObject>(*it));
      i++;
   }
};

void CSeqEntry_DUP_DISC_ATCC_CULTURE_CONFLICT :: GetReport(CRef <CClickableItem>& c_item)
{
  c_item->obj_list = thisInfo.test_item_objs[GetName()];
  c_item->description 
       = GetHasComment(c_item->item_list.size(), "biosource")
         + "conflicting ATCC strain and culture collection values.";
};

void CSeqEntry_DUP_DISC_CBS_CULTURE_CONFLICT :: GetReport(CRef <CClickableItem>& c_item)
{
   RmvRedundancy(c_item->item_list); 
   c_item->obj_list = thisInfo.test_item_objs[GetName()];
   c_item->description = GetHasComment(c_item->item_list.size(), "biosource")
                      + "conflicting CBS strain and culture collection values.";
};

void CSeqEntry_DISC_BACTERIAL_TAX_STRAIN_MISMATCH :: GetReport(CRef <CClickableItem>& c_item)
{
//   RmvRedundancy(c_item->item_list); 
   c_item->obj_list = thisInfo.test_item_objs[GetName()];
   c_item->description = GetHasComment(c_item->item_list.size(), "biosource") 
                           + "tax name/strain mismatch.";
};

void CSeqEntry_ONCALLER_STRAIN_CULTURE_COLLECTION_MISMATCH :: GetReport(CRef <CClickableItem>& c_item) 
{
   RmvRedundancy(c_item->item_list); 
   c_item->obj_list = thisInfo.test_item_objs[GetName()]; // redundancy?
   c_item->description = GetHasComment(c_item->item_list.size(), "organism")
                          + "conflicting strain and culture-collection values";
};


bool CSeqEntry_test_on_biosrc :: x_IsMissingRequiredStrain(const CBioSource& biosrc)
{
  if (!HasLineage(biosrc, "Bacteria")) return false;
  if (biosrc.IsSetOrgMod()) {
     ITERATE (list <CRef <COrgMod> >, it, biosrc.GetOrgname().GetMod())
       if ( (*it)->GetSubtype() == COrgMod :: eSubtype_strain) return false;
  }
  return true;
};

bool CSeqEntry_test_on_biosrc :: HasMulSrc(const CBioSource& biosrc)
{
  if (biosrc.IsSetOrgMod()) {
    ITERATE (list <CRef <COrgMod> >, it, biosrc.GetOrgname().GetMod()) {
      const int subtype = (*it)->GetSubtype();
      const string& subname = (*it)->GetSubname();
      if ( (subtype == COrgMod::eSubtype_strain || subtype == COrgMod::eSubtype_isolate)
             && ( subname.find(",") !=string::npos || subname.find(";") !=string::npos) )
         return true;
    }
    return false;
  }
  else return false;
};


bool CSeqEntry_test_on_biosrc :: IsBacterialIsolate(const CBioSource& biosrc)
{
   if (!HasLineage(biosrc, "Bacteria")
         || !biosrc.IsSetOrgMod()
         || HasAmplifiedWithSpeciesSpecificPrimerNote(biosrc)) return false;

   ITERATE (list <CRef <COrgMod> >, it, biosrc.GetOrg().GetOrgname().GetMod()) {
     strtmp = (*it)->GetSubname();
     if ( (*it)->GetSubtype() == COrgMod::eSubtype_isolate
             && !NStr::EqualNocase(strtmp, 0, 13, "DGGE gel band")
             && !NStr::EqualNocase(strtmp, 0, 13, "TGGE gel band")
             && !NStr::EqualNocase(strtmp, 0, 13, "SSCP gel band"))
        return true;
   }
   return false;
};

bool CSeqEntry_test_on_biosrc :: DoTaxonIdsMatch(const COrg_ref& org1, const COrg_ref& org2)
{
   if (!org1.CanGetDb() || org1.GetDb().empty()
             || !org2.CanGetDb() || org2.GetDb().empty())
      return false;

   ITERATE (vector <CRef <CDbtag> >, it, org1.GetDb()) {
       if ((*it)->GetType() == CDbtag::eDbtagType_taxon) {
          ITERATE (vector <CRef <CDbtag> >, jt, org2.GetDb()) {
             if ((*jt)->GetType() == CDbtag::eDbtagType_taxon) {
                if ((*it)->Match(**jt)) return true;
                else return false;
             }
          }
          return false;
       }
   }
   return false;
};


bool CSeqEntry_test_on_biosrc :: DoInfluenzaStrainAndCollectionDateMisMatch(const CBioSource& biosrc)
{
   string digits("1234567890");
   string flu_substr, tax_name, year, coll_year;
   size_t pos;
   if (!biosrc.IsSetTaxname() || (tax_name=biosrc.GetTaxname()).empty()) return false;
//   tax_name = biosrc.GetTaxname();
   flu_substr = tax_name.substr(0, 18);
   if (flu_substr == "Influenza A virus ") {
     if ( (pos = tax_name.find("(")) != string::npos) {
       if ( (pos = CTempString(tax_name).substr(pos+1).find("(")) != string::npos) {
          strtmp = tax_name.substr(0, pos);
          if ( (pos = strtmp.find_last_not_of(" ")) != string::npos) {
             if (isdigit(strtmp[pos])) {
               pos = strtmp.substr(0, pos).find_last_not_of(digits); 
               year = NStr::StringToUInt(CTempString(strtmp).substr(pos+1));
             }
          };
       }
     }
   }
   else if (flu_substr == "Influenza B virus ") {
     if ( (pos = tax_name.find("/")) != string::npos) {
       if ( (pos = CTempString(tax_name).substr(pos+1).find_first_not_of(" "))!=string::npos) {
         if (isdigit(tax_name[pos])) 
           year = CTempString(tax_name).substr(pos);
       }
     }
   }
   else return false;

   if (!year.empty()) {
     if (biosrc.CanGetSubtype()) {
        ITERATE (list< CRef< CSubSource > >, it, biosrc.GetSubtype()) {
           if ( (*it)->GetSubtype() == CSubSource::eSubtype_collection_date) {
              strtmp = (*it)->GetName();
              if ( (pos = strtmp.find("-")) == string::npos) {
                 coll_year = strtmp;
              }
              else if (!isdigit(strtmp[pos+1])) return true;
              else coll_year = CTempString(strtmp).substr(pos+1);
              if (year == coll_year) return true;
              else return false;
           }
        }
     }
   }
   return true;
};


void CSeqEntry_test_on_biosrc :: AddMissingViralQualsDiscrepancies(const CBioSource& biosrc, const string& desc)
{
  bool has_collection_date = false, has_country = false, has_specific_host = false;
  if (biosrc.CanGetSubtype()) {
    ITERATE (list <CRef <CSubSource> >, it, biosrc.GetSubtype()) {
      if (!has_collection_date 
            && (*it)->GetSubtype() == CSubSource :: eSubtype_collection_date) 
        has_collection_date = true;
      if (!has_country 
                && (*it)->GetSubtype() == CSubSource :: eSubtype_country)
         has_country = true;
      if (has_collection_date && has_country) break;
    }
  }

  if (biosrc.IsSetOrgname() && biosrc.GetOrgname().CanGetMod()) {
    ITERATE (list< CRef< COrgMod > >, it, biosrc.GetOrgname().GetMod()) {
      if ( (*it)->GetSubtype() == COrgMod :: eSubtype_nat_host) {
        has_specific_host = true;
        break;
      }
    }
  }

  CConstRef <CObject> src_ref (&biosrc);
  if (!has_collection_date) {
    strtmp = "collection date";
    thisInfo.test_item_list[GetName_quals()].push_back(strtmp + "$" + desc);
    thisInfo.test_item_objs[GetName_quals() + "$" + strtmp].push_back(src_ref);
  }
  if (!has_country) {
    strtmp = "country";
    thisInfo.test_item_list[GetName_quals()].push_back(strtmp + "$" + desc);
    thisInfo.test_item_objs[GetName_quals() + "$" + strtmp].push_back(src_ref);
  }
  if (!has_specific_host) {
    strtmp = "specific-host";
    thisInfo.test_item_list[GetName_quals()].push_back(strtmp + "$" + desc);
    thisInfo.test_item_objs[GetName_quals() + "$" + strtmp].push_back(src_ref);
  }
};


bool CSeqEntry_test_on_biosrc :: IsMissingRequiredClone (const CBioSource& biosrc)
{
   bool has_clone = false, needs_clone = false;
   if (HasAmplifiedWithSpeciesSpecificPrimerNote(biosrc)) return false;
   if (biosrc.IsSetTaxname()
         && NStr::FindNoCase(biosrc.GetTaxname(), "uncultured") != string::npos)
      needs_clone = true;
   if (IsSubSrcPresent(biosrc, CSubSource :: eSubtype_environmental_sample)
                     || IsSubSrcPresent(biosrc, CSubSource :: eSubtype_clone))
         has_clone = true;
   if (needs_clone && !has_clone) {  // look for gel band isolate
     if (biosrc.IsSetOrgMod()) {
       ITERATE (list <CRef <COrgMod> >, it, biosrc.GetOrgname().GetMod()) {
          if ( (*it)->GetSubtype() == COrgMod :: eSubtype_isolate 
                && NStr::FindNoCase((*it)->GetSubname(), "gel band") != string::npos) {
                   needs_clone = false;  break;
          }
       }
     }
   }
   if (needs_clone && !has_clone) return true;
   return false;
};


void CSeqEntry_test_on_biosrc ::RunTests(const CBioSource& biosrc, const string& desc, CConstRef <CObject> obj_ref, int idx)
{
  // DISC_REQUIRED_STRAIN
  if (m_run_strain && x_IsMissingRequiredStrain(biosrc)) {
      thisInfo.test_item_list[GetName_strain()].push_back(desc);
      thisInfo.test_item_objs[GetName_strain()].push_back(obj_ref);
  }
  
  // DISC_REQUIRED_CLONE
  if (m_run_clone && IsMissingRequiredClone(biosrc)) {
      thisInfo.test_item_list[GetName_clone()].push_back(desc);
      thisInfo.test_item_objs[GetName_clone()].push_back(obj_ref);
  }

  // ONCALLER_MULTISRC
  if (m_run_mult && HasMulSrc( biosrc )) {
      thisInfo.test_item_list[GetName_mult()].push_back(desc);
      thisInfo.test_item_objs[GetName_mult()].push_back(obj_ref);
  }

  // DISC_BACTERIA_SHOULD_NOT_HAVE_ISOLATE
  if (m_run_iso && IsBacterialIsolate(biosrc)) {  
      thisInfo.test_item_list[GetName_iso()].push_back(desc);
      thisInfo.test_item_objs[GetName_iso()].push_back(obj_ref);
  }

  // CheckTaxNamesAgainstTaxDatabase in tax3api.c
  // TAX_LOOKUP_MISSING, TAX_LOOKUP_MISMATCH
  string org_tax, db_tax;
  org_tax 
     = biosrc.GetOrg().CanGetTaxname()? biosrc.GetOrg().GetTaxname() :kEmptyStr;
  if (m_run_tmiss || m_run_tbad) {
     CRef <CTaxon2_data> 
         lookup_tax = thisInfo.tax_db_conn.Lookup(biosrc.GetOrg());
     if (lookup_tax.Empty() || !(lookup_tax->CanGetOrg())) {
         if (m_run_tmiss) {
              thisInfo.test_item_list[GetName_tmiss()].push_back(desc); 
              thisInfo.test_item_objs[GetName_tmiss()].push_back(obj_ref);
         }
     }
     else {
       db_tax =lookup_tax->GetOrg().CanGetTaxname() ?
                                  lookup_tax->GetOrg().GetTaxname() : kEmptyStr;
       if (m_run_tmiss && org_tax != db_tax) {
           thisInfo.test_item_list[GetName_tbad()].push_back(desc);
           thisInfo.test_item_objs[GetName_tbad()].push_back(obj_ref);
       }
       else {
         if (m_run_tbad && !DoTaxonIdsMatch(biosrc.GetOrg(),lookup_tax->GetOrg())){
             thisInfo.test_item_list[GetName_tbad()].push_back(desc);
             thisInfo.test_item_objs[GetName_tbad()].push_back(obj_ref);
         }
       }
     }
  }
 
  // DISC_INFLUENZA_DATE_MISMATCH
  if (m_run_flu && DoInfluenzaStrainAndCollectionDateMisMatch(biosrc)) {
     thisInfo.test_item_list[GetName_flu()].push_back(desc);
     thisInfo.test_item_objs[GetName_flu()].push_back(obj_ref);
  }

  // DISC_MISSING_VIRAL_QUALS
  if (m_run_quals && HasLineage(biosrc, "Viruses")) {
         AddMissingViralQualsDiscrepancies(biosrc, desc);
  }

  // DIVISION_CODE_CONFLICTS
  if (m_run_div) {
     string div_code;
     if (biosrc.IsSetDivision() && !(div_code= biosrc.GetDivision()).empty()) {
        thisInfo.test_item_list[GetName_div()].push_back(div_code + "$" + desc);
        thisInfo.test_item_objs[GetName_div() + "$" + div_code]
                   .push_back(obj_ref);
     }
  }

  // DISC_MAP_CHROMOSOME_CONFLICT
  if (m_run_map) {
     bool has_map = false, has_chrom = false;
     if (idx >=0 && biosrc.CanGetSubtype()) {
       has_map = IsSubSrcPresent(biosrc, CSubSource :: eSubtype_map);
       has_chrom = IsSubSrcPresent(biosrc, CSubSource :: eSubtype_chromosome);
       if (has_map && !has_chrom) {
         for (CBioseq_CI b_ci(*thisInfo.scope, *(biosrc_seqdesc_seqentry[idx]));
                  b_ci; 
                  ++b_ci){
            if (b_ci->IsAa()) continue;
            CConstRef <CBioseq> bioseq = b_ci->GetCompleteBioseq();
            if (IsBioseqHasLineage(*bioseq, "Eukaryota", false)) {
                 thisInfo.test_item_list[GetName_map()]
                             .push_back( GetDiscItemText(*bioseq)); 
                 thisInfo.test_item_objs[GetName_map()]
                           .push_back(CConstRef <CObject>(bioseq.GetPointer()));
            }
         }
       }
     }
  }

  // DISC_TRINOMIAL_SHOULD_HAVE_QUALIFIER
  if (m_run_trin && FindTrinomialWithoutQualifier(biosrc)) {
      thisInfo.test_item_list[GetName_trin()].push_back(desc);
      thisInfo.test_item_objs[GetName_trin()].push_back(obj_ref);
  }

  // DISC_METAGENOMIC
  if (m_run_meta && IsSubSrcPresent(biosrc, CSubSource::eSubtype_metagenomic)) {
      thisInfo.test_item_list[GetName_meta()].push_back(desc); 
      thisInfo.test_item_objs[GetName_meta()].push_back(obj_ref);
  }

  // TEST_SP_NOT_UNCULTURED
  if (m_run_sp) {
    unsigned len;
    if (!org_tax.empty()) {
       len = org_tax.size();
       if (len >= 4 
              && CTempString(org_tax).substr(len-4) == " sp."
              && !NStr::EqualNocase(org_tax.substr(0, 11), "uncultured ")) {
          thisInfo.test_item_list[GetName_sp()].push_back(desc);
          thisInfo.test_item_objs[GetName_sp()].push_back(obj_ref);
       }
    }
  }

  // TEST_MISSING_PRIMER
  if (m_run_prim && MissingPrimerValue(biosrc)) {
       thisInfo.test_item_list[GetName_prim()].push_back(desc);
       thisInfo.test_item_objs[GetName_prim()].push_back(obj_ref);
  }
 
  // ONCALLER_COUNTRY_COLON
  if (m_run_cty) {
     vector <string> src_strs, arr;
     GetSubSrcValues(biosrc, CSubSource::eSubtype_country, src_strs);
     if (!src_strs.empty()) {
         ITERATE (vector <string>, it, src_strs) {
           arr.clear();
           arr = NStr::Tokenize(*it, ":", arr);
           if (arr.size() > 2) {
              thisInfo.test_item_list[GetName_cty()].push_back(desc); 
              thisInfo.test_item_objs[GetName_cty()].push_back(obj_ref);
              break;
           }
         }
     }
     arr.clear();
  }
 
  // ONCALLER_DUPLICATE_PRIMER_SET
  if (m_run_pcr && biosrc.CanGetPcr_primers()) {
     const list <CRef <CPCRReaction> > pcr_ls = biosrc.GetPcr_primers().Get();
     if (!pcr_ls.empty()) { 
        list <CRef <CPCRReaction> > :: const_iterator it, jt;
        it = pcr_ls.begin();
        for (jt = pcr_ls.begin(); jt != pcr_ls.end(); jt++) {
           if ( (it != jt) && SamePCRReaction(**it, **jt)) {
              thisInfo.test_item_list[GetName_pcr()].push_back(desc);
              thisInfo.test_item_objs[GetName_pcr()].push_back(obj_ref);
           }
        }
     }
  }
};

void CSeqEntry_test_on_biosrc :: IniMap(const list <CRef <CPCRPrimer> >& ls, Str2Int& map)
{
   string pcr_str;
   ITERATE (list <CRef <CPCRPrimer> >, it, ls) {
     pcr_str = (*it)->CanGetSeq() ? (*it)->GetSeq() : kEmptyStr;
     pcr_str = NStr::ToUpper(pcr_str) + "$";
     strtmp = (*it)->CanGetName() ? (*it)->GetName() : kEmptyStr;
     pcr_str += NStr::ToUpper(strtmp);
     if (map.find(pcr_str) == map.end()) map[pcr_str] = 1;
     else map[pcr_str] ++;
   }
};

bool CSeqEntry_test_on_biosrc :: SamePrimerList(const list <CRef <CPCRPrimer> >& ls1, const list <CRef <CPCRPrimer> >& ls2)
{
   if ( (ls1.size() != ls2.size())) return false;
   Str2Int map1, map2;
   IniMap(ls1, map1);
   IniMap(ls2, map1);
   if (map1.size() != map2.size()) return false;
   if (map1 == map2) return true;
   else return false;
};

bool CSeqEntry_test_on_biosrc :: SamePCRReaction(const CPCRReaction& pcr1, const CPCRReaction& pcr2)
{
  bool has_fwd1 = (pcr1.CanGetForward() && !pcr1.GetForward().Get().empty());
  bool has_rev1 = (pcr1.CanGetReverse() && !pcr1.GetReverse().Get().empty());
  bool has_fwd2 = (pcr2.CanGetForward() && !pcr2.GetForward().Get().empty());
  bool has_rev2 = (pcr2.CanGetReverse() && !pcr2.GetReverse().Get().empty());

  if ((has_fwd1 != has_fwd2) || (has_rev1 != has_rev2)) {
      return false;
  }
  if (has_fwd1) { 
    if (SamePrimerList(pcr1.GetForward().Get(), pcr2.GetForward().Get())) {
      return (SamePrimerList(pcr1.GetReverse().Get(), pcr2.GetReverse().Get()));
    }
  }
  else if (has_rev1) {
     return (SamePrimerList(pcr1.GetReverse().Get(), pcr2.GetReverse().Get()));
  };

  return true;
};

void CSeqEntry_DISC_REQUIRED_STRAIN :: GetReport(CRef <CClickableItem>& c_item)
{
  c_item->obj_list = thisInfo.test_item_objs[GetName()];
  c_item->description = GetIsComment(c_item->item_list.size(), "biosource")
                                                         + "missing required strain value";
};

void CSeqEntry_ONCALLER_DUPLICATE_PRIMER_SET :: GetReport(CRef <CClickableItem>& c_item)
{
   c_item->obj_list = thisInfo.test_item_objs[GetName()];
   c_item->description
     = GetHasComment(c_item->item_list.size(), "BioSource") + "duplicate primer pairs.";
};

void CSeqEntry_ONCALLER_COUNTRY_COLON :: GetReport(CRef <CClickableItem>& c_item)
{
   c_item->obj_list = thisInfo.test_item_objs[GetName()];
   c_item->description
     = GetHasComment(c_item->item_list.size(), "country source") + "more than 1 colon.";
};

bool CSeqEntry_test_on_biosrc :: MissingPrimerValue(const CBioSource& biosrc)
{
  bool has_fwd, has_rev, seq_f, seq_r, name_f, name_r;
  list <CRef <CPCRPrimer> >::const_iterator fwd_it, rev_it;
  if (biosrc.CanGetPcr_primers()) {
     ITERATE (list <CRef <CPCRReaction> >, it, biosrc.GetPcr_primers().Get()) {
        has_fwd= ((*it)->CanGetForward() && !(*it)->GetForward().Get().empty());
        has_rev= ((*it)->CanGetReverse() && !(*it)->GetReverse().Get().empty());
        if (!has_fwd && !has_rev) continue;
        if ( has_fwd != has_rev) return true;
        else {
           const list <CRef <CPCRPrimer> >& fwd = (*it)->GetForward().Get();
           const list <CRef <CPCRPrimer> >& rev = (*it)->GetReverse().Get();
           if (fwd.size() != rev.size() ) return true;
           for (fwd_it = fwd.begin(), rev_it = rev.begin(); 
                 fwd_it != fwd.end(); 
                 fwd_it++, rev_it++) {
              IsFwdRevDataPresent(*fwd_it, seq_f, name_f);
              IsFwdRevDataPresent(*rev_it, seq_r, name_r);
              if ((seq_f != seq_r) || (name_f != name_r)) return true;
           }
        }
     }
  }
  return false;
};


void CSeqEntry_test_on_biosrc :: IsFwdRevDataPresent(const CRef <CPCRPrimer>& primer, bool& has_seq, bool& has_name)
{
   has_seq = (primer->CanGetSeq() && !primer->GetSeq().Get().empty()) ? true : false;
   has_name = (primer->CanGetName() && !primer->GetName().Get().empty()) ? true : false;
};


void CSeqEntry_TEST_MISSING_PRIMER :: GetReport(CRef <CClickableItem>& c_item)
{
   c_item->obj_list = thisInfo.test_item_objs[GetName()];
   c_item->description 
      = GetHasComment(c_item->item_list.size(),"biosource") +"primer sets with missing values";
};


void CSeqEntry_TEST_SP_NOT_UNCULTURED :: GetReport(CRef <CClickableItem>& c_item)
{
   c_item->obj_list = thisInfo.test_item_objs[GetName()];
   c_item->description = GetHasComment(c_item->item_list.size(), "biosource") 
                     + "taxnames that end with ' sp.' but do not start with 'uncultured'";
};


void CSeqEntry_DISC_METAGENOMIC :: GetReport(CRef <CClickableItem>& c_item)
{
   c_item->obj_list = thisInfo.test_item_objs[GetName()];
   c_item->description
      = GetHasComment(c_item->item_list.size(), "biosource") + "metagenomic qualifier";
};


bool CSeqEntry_test_on_biosrc :: FindTrinomialWithoutQualifier(const CBioSource& biosrc)
{
  if (!biosrc.IsSetTaxname() 
         || biosrc.GetTaxname().empty() 
         || NStr::FindNoCase(biosrc.GetTaxname(), " x ") != string::npos) {
      return false;
  }
  if (HasLineage(biosrc, "Viruses")) return false;
  string tax_nm = biosrc.GetTaxname();
  size_t pos;
  ITERATE (Str2Str, it, thisInfo.srcqual_keywords) {
     if ( (pos = NStr::FindNoCase(tax_nm, it->second)) != string::npos) {
        tax_nm = CTempString(tax_nm).substr(pos + (it->second).size());
        NStr::TruncateSpacesInPlace(tax_nm, NStr::eTrunc_Begin);
        if (!tax_nm.empty()) {
           vector <string> arr;
           GetOrgModValues(biosrc, it->first, arr);
           ITERATE (vector <string>, jt, arr) {
             if (tax_nm.substr(0, (*jt).size()) != (*jt)) return true;
           }
        } 
        break;
     } 
  } 
  return false;
};


void CSeqEntry_DISC_TRINOMIAL_SHOULD_HAVE_QUALIFIER :: GetReport(CRef <CClickableItem>& c_item)
{
   c_item->obj_list = thisInfo.test_item_objs[GetName()];
   c_item->description
     = GetOtherComment(c_item->item_list.size(), "trinomial source lacks", 
                                                              "trinomial sources lack")
       + "  corresponding qualifier";

};


void CSeqEntry_test_on_biosrc :: AddEukaryoticBioseqsToReport(const CBioseq_set& set)
{
   ITERATE (list < CRef < CSeq_entry > >, it, set.GetSeq_set()) {
      if ((*it)->IsSeq()) {
         const CBioseq& bioseq = (*it)->GetSeq();
         if ( IsBioseqHasLineage(bioseq, "Eukaryota", false) && !bioseq.IsAa())
           thisInfo.test_item_list[GetName_map()].push_back(GetDiscItemText(bioseq));
         else AddEukaryoticBioseqsToReport( (*it)->GetSet() );
     }
   }
};


void CSeqEntry_test_on_biosrc :: TestOnObj(const CSeq_entry& seq_entry)
{
   if (thisTest.is_BIOSRC_run) return;
   thisTest.is_BIOSRC_run = true;

   m_run_clone = (thisTest.tests_run.find(GetName_clone()) != end_it);
   m_run_cty = (thisTest.tests_run.find(GetName_cty()) != end_it);
   m_run_div = (thisTest.tests_run.find(GetName_div()) != end_it);
   m_run_flu = (thisTest.tests_run.find(GetName_flu()) != end_it);
   m_run_iso = (thisTest.tests_run.find(GetName_iso()) != end_it);
   m_run_map = (thisTest.tests_run.find(GetName_map()) != end_it);
   m_run_meta = (thisTest.tests_run.find(GetName_meta()) != end_it);
   m_run_mult = (thisTest.tests_run.find(GetName_mult()) != end_it);
   m_run_pcr = (thisTest.tests_run.find(GetName_pcr()) != end_it);
   m_run_prim = (thisTest.tests_run.find(GetName_prim()) != end_it);
   m_run_quals = (thisTest.tests_run.find(GetName_quals()) != end_it);
   m_run_strain = (thisTest.tests_run.find(GetName_strain()) != end_it);
   m_run_sp = (thisTest.tests_run.find(GetName_sp()) != end_it);
   m_run_tbad = (thisTest.tests_run.find(GetName_tbad()) != end_it);
   m_run_tmiss = (thisTest.tests_run.find(GetName_tmiss()) != end_it);
   m_run_trin = (thisTest.tests_run.find(GetName_trin()) != end_it);

   string desc;
   ITERATE (vector <const CSeq_feat*>, it, biosrc_feat)
     RunTests((*it)->GetData().GetBiosrc(), 
               GetDiscItemText(**it), 
               CConstRef <CObject>(*it));

   unsigned i=0;
   ITERATE (vector <const CSeqdesc*>, it, biosrc_seqdesc) {
     desc = GetDiscItemText( **it, *(biosrc_seqdesc_seqentry[i]));
     RunTests((*it)->GetSource(), desc, CConstRef <CObject>(*it), (int)i);
     i++;
   };
};


void CSeqEntry_DISC_REQUIRED_CLONE :: GetReport(CRef <CClickableItem>& c_item)
{
  c_item->obj_list = thisInfo.test_item_objs[GetName()];
  c_item->description = GetIsComment(c_item->item_list.size(), "biosource")
                          + "missing required clone value";
};


void CSeqEntry_DISC_MAP_CHROMOSOME_CONFLICT :: GetReport(CRef <CClickableItem>& c_item)
{
   c_item->obj_list = thisInfo.test_item_objs[GetName()];
   c_item->description = GetOtherComment(c_item->item_list.size(), 
                "source on eukaryotic sequence has", "sources on eukaryotic sequences have") 
         + " map but not chromosome.";
};
  


void CSeqEntry_DIVISION_CODE_CONFLICTS :: GetReport(CRef <CClickableItem>& c_item)
{
   Str2Strs div2item;
   GetTestItemList(c_item->item_list, div2item);
   c_item->item_list.clear();

   if (div2item.size() > 1) {
     ITERATE (Str2Strs, it, div2item) {
       AddSubcategory(c_item, GetName() + "$" + it->first, &(it->second), "bioseq", 
                    "division code " + it->first, e_HasComment, false);
     }
     c_item->description = "Division code conflicts found.";
   }
};

void CSeqEntry_DISC_MISSING_VIRAL_QUALS :: GetReport(CRef <CClickableItem>& c_item)
{
   Str2Strs qual2src;
   GetTestItemList(c_item->item_list, qual2src);
   c_item->item_list.clear();
   ITERATE (Str2Strs, it, qual2src) {
      AddSubcategory(c_item, GetName() + "$" + it->first, &(it->second), "virus organism", 
                         "missing suggested qualifier " + it->first);
   } 
//   RmvRedundancy(c_item->item_list);
   unsigned cnt = c_item->item_list.size();
   c_item->description 
       = GetIsComment(cnt, "virus organism") + GetNoun(cnt, "missing required qualifier");
};


void CSeqEntry_DISC_INFLUENZA_DATE_MISMATCH :: GetReport(CRef <CClickableItem>& c_item)
{
   c_item->obj_list = thisInfo.test_item_objs[GetName()];
   c_item->description
     = GetOtherComment(c_item->item_list.size(), "influenza strain conflicts", "influenza strains conflict")
        + " with collection date.";
};


void CSeqEntry_TAX_LOOKUP_MISSING :: GetReport(CRef <CClickableItem>& c_item)
{
   RmvRedundancy(c_item->item_list);  // maybe unnecessary?
   c_item->description
     = GetIsComment(c_item->item_list.size(), "organism") + "not found in taxonomy lookup.";
};


void CSeqEntry_TAX_LOOKUP_MISMATCH :: GetReport(CRef <CClickableItem>& c_item)
{
//   RmvRedundancy(c_item->item_list); 
   c_item->obj_list = thisInfo.test_item_objs[GetName()];
   c_item->description
     = GetDoesComment(c_item->item_list.size(), "tax name") + "not match taxonomy lookup.";
};


void CSeqEntry_DISC_BACTERIA_SHOULD_NOT_HAVE_ISOLATE :: GetReport(CRef <CClickableItem>& c_item)
{
//   RmvRedundancy(c_item->item_list); 
   c_item->obj_list = thisInfo.test_item_objs[GetName()];
   c_item->description = GetHasComment(c_item->item_list.size(), "bacterial biosrouce")
                           + "isolate.";
};


void CSeqEntry_ONCALLER_MULTISRC :: GetReport(CRef <CClickableItem>& c_item)
{
  //RmvRedundancy(c_item->item_list); 
  c_item->obj_list = thisInfo.test_item_objs[GetName()];
  c_item->description = GetHasComment(c_item->item_list.size(), "organism")
                                      + "commma or semicolon in strain or isolate.";
};


void CSeqEntry_on_incnst_user :: x_GetIncnstTestReport (CRef <CClickableItem>& c_item, const string& setting_name, const string& title, const string& item_type)
{ 
  Str2Strs key2ls, field2ls, vlu2ls;
  GetTestItemList(c_item->item_list, key2ls);
  c_item->item_list.clear();
  
  bool has_missing, has_fld_missing;
  bool any_missing = false, any_inconsistent = false;
  vector <string> missing_ls;
  string key_str, field;
  unsigned cnt;
  CRef <CClickableItem> c_missing (new CClickableItem);
  CRef <CClickableItem> c_field (new CClickableItem);

  bool is_strcomm = (setting_name == GetName_comm())? true: false;
  string label, prefix;
  ITERATE (Str2Strs, it, key2ls) {
    prefix = it->first;
    key_str = is_strcomm ? prefix + " " + item_type : kEmptyStr;
    field2ls.clear();
    GetTestItemList(it->second, field2ls, "&");
    has_missing = (field2ls.find("missing") != field2ls.end());
    if (has_missing) { // prefix + "$missing&" + bsq_desc
        c_missing.Reset(new CClickableItem); 
        c_missing->setting_name = setting_name;
        c_missing->item_list = field2ls["missing"];
        unsigned cnt = field2ls["missing"].size(); 
        c_missing->description 
             = GetIsComment(cnt, "Bioseq") + "missing " 
               + (is_strcomm? key_str : GetNoun(cnt, "DBLink object"));
        c_item->subcategories.push_back(c_missing);
        strtmp = "$" + prefix + "_missing";
        c_item->obj_list = thisInfo.test_item_objs[setting_name + strtmp];
    }
    ITERATE (Str2Strs, fid, field2ls) {// prefix + $ + fid.label + & + fid.vlu + @ + user.desc;
      if (fid->first == "missing") {  // fix.label
          continue;
      }
      has_fld_missing = false;
      c_field.Reset(new CClickableItem);
      c_field->setting_name = setting_name;
      vlu2ls.clear();
      GetTestItemList(fid->second, vlu2ls, "@");
      if (vlu2ls.size() > 1) {
         ITERATE (Str2Strs, vid, vlu2ls) {
            strtmp = is_strcomm? key_str : "DBLink object";
            label = "$" + prefix + "_" + fid->first + "&" + vid->first;
            if (vid->first == "empty") {
               has_missing = has_fld_missing = true;
               AddSubcategory(c_field, 
                              setting_name + label, 
                              &(vid->second), 
                              strtmp,
                              "missing field " + fid->first, 
                              e_IsComment, 
                              false);
            }
            else {
              AddSubcategory(c_field, 
                             setting_name + label, 
                             &(vid->second), 
                             strtmp,
                          "field " + fid->first + " value '" + vid->first + "'",
                          e_HasComment, 
                          false);
            }
         }
      }
      else { 
         CRef <CClickableItem> c_sub (new CClickableItem);
         c_sub->setting_name = setting_name;
         c_sub->item_list = vlu2ls.begin()->second;
         if (vlu2ls.begin()->first == "empty") {
            c_sub->description 
                 = "1 " + key_str + " is missing field " + fid->first;
            has_missing = has_fld_missing = true;
         }
         else {
            c_sub->description 
               = is_strcomm ? ("All " + key_str) : "All DBLink object";
            c_sub->description += "s have field " + fid->first + " value '" 
                 + vlu2ls.begin()->first + "'";
         }
         label 
            = "$" + prefix + "_" + fid->first + "&" + vlu2ls.begin()->first;
         c_sub->obj_list = thisInfo.test_item_objs[setting_name + label];
         c_field->subcategories.push_back(c_sub);
      }
      cnt = c_field->subcategories.size();
      strtmp = is_strcomm ? item_type + " field " + fid->first : fid->first;
      if (has_fld_missing) {
          if (cnt == 1) {
              strtmp += " (all missing)";
          }
          else if (cnt == 2) {
              strtmp += " (some missing, all same)";
          }
          else {
              strtmp += " (some missing, inconsistent)";
          }
      }
      else {
          strtmp += (cnt == 1)? " (all present, all same)" 
                              : " (all present, inconsistent)";
      }
      c_field->description = strtmp;
      c_item->subcategories.push_back(c_field);
      if (!any_missing && strtmp.find("missing") != string::npos) {
          any_missing = true;
      }
      if (!any_inconsistent && strtmp.find("inconsistent") != string::npos) {
            any_inconsistent = true;
      }
    }    
  }

  c_item->description 
        = title + (any_missing ? " (some missing, " : " (all present, ")
            + (string)(any_inconsistent ? "inconsistent)" : "all same)");
};

string CSeqEntry_on_incnst_user :: x_GetStrCommPrefix(const CUser_object& user_obj)
{
   if (user_obj.HasField("StructuredCommentPrefix")
            && user_obj.GetField("StructuredCommentPrefix").GetData().IsStr()) {
      return(CUtf8::AsSingleByteString(
                user_obj.GetField("StructuredCommentPrefix").GetData().GetStr(),
                eEncoding_Ascii));
   }
   else return ("unnamed");
};

bool CSeqEntry_on_incnst_user :: x_HasUserObjOfType(CSeqdesc_CI uci, const string& type)
{
   for (; uci; ++uci) {
     if (x_GetUserObjType(uci->GetUser()) == type) {
        return true;
     }
   }
 
   return false;
};

void CSeqEntry_on_incnst_user :: x_MakeMissingList(CConstRef <CBioseq> bsq_ref, const string& bsq_desc, const string& setting_name, Str2Strs& key2ls)
{
   ITERATE (Str2Strs, kid, key2ls) {
     thisInfo.test_item_list[setting_name]
                .push_back(kid->first + "$missing&" + bsq_desc);
     thisInfo.test_item_objs[setting_name + "$" + kid->first + "_missing"]
                .push_back( CConstRef <CObject>(bsq_ref.GetPointer()));

     ITERATE (vector <string>, lid, key2ls[kid->first]) { // individual vlu
        strtmp = kid->first + "$" + *lid + "&empty@";
        thisInfo.test_item_list[setting_name].push_back(strtmp + bsq_desc);
        strtmp = kid->first + "_" + *lid + "&empty@";
        thisInfo.test_item_objs[setting_name + "$" + strtmp]
                   .push_back(CConstRef <CObject>(bsq_ref.GetPointer()));
     }
   }
};

void CSeqEntry_on_incnst_user :: x_ClassifyFlds(Str2Strs& key2ls, const string& key, const CSeqdesc& sd, const string& setting_name, const CBioseq& bsq)
{
   const CUser_object& uobj = sd.GetUser();
   string label;
   // key = prefix
   ITERATE (vector <string>, fid, key2ls[key]) {
      strtmp = kEmptyStr;
      if (uobj.HasField(*fid)) {
         const CUser_field::C_Data& dt = uobj.GetField(*fid).GetData();
         if (setting_name == GetName_comm()) {
              strtmp = dt.IsStr()?
                         CUtf8::AsSingleByteString(dt.GetStr(), eEncoding_Ascii)
                         : "empty";
         }
         else {
           if (dt.IsStrs()) {
              ITERATE (vector <CStringUTF8>, sit, dt.GetStrs()) {
                 strtmp += (CUtf8::AsSingleByteString(*sit, eEncoding_Ascii) 
                               + "; ");
              }
           }
           else if (dt.IsInts()) {
              ITERATE (vector <int>, iit, dt.GetInts()) {
                 strtmp += NStr::IntToString(*iit) + "; ";
              }
           }
           strtmp = strtmp.substr(0, strtmp.size()-2);
         }
      }
      label = key + "$" + *fid + "&" + strtmp + "@";
      thisInfo.test_item_list[setting_name]
                 .push_back(label + GetDiscItemText(sd, bsq)); 
      label = key + "_" + *fid + "&" + strtmp;
      thisInfo.test_item_objs[setting_name + "$" + label]
                 .push_back(CConstRef <CObject>(&sd));
   }   
};

void CSeqEntry_on_incnst_user :: TestOnObj(const CSeq_entry& seq_entry)
{
   if (thisTest.is_IncnstUser_run) {
      return;
   }
   thisTest.is_IncnstUser_run = true;

   CConstRef <CBioseq> seq_ref;
   CConstRef <CBioseq_set> set_ref;
   string desc, type_str, prefix, seq_desc;

   bool run_test_comm = (thisTest.tests_run.find(GetName_comm()) != end_it);
   bool run_test_db = (thisTest.tests_run.find(GetName_db()) != end_it);

   Str2Strs prefix2flds, dblink2dbs;
   // prepare prefix2flds, dblink2dbs;
   ITERATE (vector <const CSeqdesc*>, it, user_seqdesc) {
       const CUser_object& user_obj = (*it)->GetUser();
       type_str = x_GetUserObjType(user_obj);
       // DISC_INCONSISTENT_STRUCTURED_COMMENTS
       if (type_str == "StructuredComment") { 
         prefix = x_GetStrCommPrefix(user_obj);
         ITERATE (vector <CRef <CUser_field> >, fid, user_obj.GetData()) {
           strtmp = (*fid)->GetLabel().IsStr()? 
                        (*fid)->GetLabel().GetStr() : "invalid";
           
           // remove redundant prefix
           if (strtmp != "StructuredCommentPrefix" 
                  && strtmp != "StructuredCommentSuffix") {
              bool match = false;
              ITERATE (vector <string>, kit, prefix2flds[prefix]) {
                  if (*kit == strtmp) { 
                       match = true; 
                       break;
                  }
              }
              if (!match) prefix2flds[prefix].push_back(strtmp);
           };
         };
       }
       else if (type_str == "DBLink") {  // DISC_INCONSISTENT_DBLINK
          ITERATE (vector <CRef <CUser_field> >, fid, user_obj.GetData()) {
             strtmp = (*fid)->GetLabel().IsStr()? 
                          (*fid)->GetLabel().GetStr() : "invalid";
             if (strtmp == "Trace Assembly Archive" 
                      || strtmp == "BioSample"
                      || strtmp == "ProbeDB" 
                      || strtmp == "Sequence Read Archive"
                      || strtmp == "BioProject") {
                 dblink2dbs["db"].push_back(strtmp);
             }
          }
       }
   }
   string bsq_desc, fld_vlu;
   for (CBioseq_CI bci(*thisInfo.scope, seq_entry); bci; ++bci) {
     if ((*bci).IsAa()) continue;
     CSeqdesc_CI uci(*bci, CSeqdesc::e_User);
     CConstRef <CBioseq> bsq_ref = bci->GetCompleteBioseq();
     bsq_desc = GetDiscItemText(*bsq_ref);
     if (run_test_comm) {  // DISC_INCONSISTENT_STRUCTURED_COMMENTS
        if (!uci || !x_HasUserObjOfType(uci, "StructuredComment")) {
           x_MakeMissingList(bsq_ref, bsq_desc, GetName_comm(), prefix2flds);
        }
        else {
            for (CSeqdesc_CI suci = uci; suci; ++suci) {
               if (x_GetUserObjType(suci->GetUser()) != "StructuredComment") {
                    continue;
               }
               prefix = x_GetStrCommPrefix(suci->GetUser());
               x_ClassifyFlds(prefix2flds, 
                              prefix, 
                              *suci, 
                              GetName_comm(), 
                              *(bci->GetCompleteBioseq()));
           }
        } 
     }
     if (run_test_db) { // DISC_INCONSISTENT_DBLINK
        if (!uci || !x_HasUserObjOfType(uci, "DBLink")) {
           x_MakeMissingList(bsq_ref, bsq_desc, GetName_db(), dblink2dbs);
        }
        else {
          for (; uci; ++uci) {
             if (x_GetUserObjType(uci->GetUser()) != "DBLink") continue;
             x_ClassifyFlds(dblink2dbs, "db", *uci, GetName_db(), *(bci->GetCompleteBioseq()));
          }
        }
     }
   }
};

void CSeqEntry_test_on_user :: GroupAllBioseqs(const CBioseq_set& bioseq_set, const int& id)
{
  string desc;
  ITERATE (list < CRef < CSeq_entry > >, it, bioseq_set.GetSeq_set()) {
     if ((*it)->IsSeq()) {
         const CBioseq& bioseq = (*it)->GetSeq();
         desc = GetDiscItemText(bioseq);
         if ( bioseq.IsAa() ) {
              PROJECT_ID_prot[id].push_back(desc);
              thisInfo.test_item_list[GetName_proj()].push_back("prot$" + desc);
         }
         else {
              PROJECT_ID_nuc[id].push_back(desc);
              thisInfo.test_item_list[GetName_proj()].push_back("nuc$" + desc);
         }
     }
     else GroupAllBioseqs((*it)->GetSet(), id);
  }
};


void CSeqEntry_test_on_user :: CheckCommentCountForSet(const CBioseq_set& set, const unsigned& cnt, Str2Int& bioseq2cnt)
{
   string desc;
   ITERATE (list < CRef < CSeq_entry > >, it, set.GetSeq_set()) {
     if ((*it)->IsSeq()) {
          const CBioseq& bioseq = (*it)->GetSeq();
          if (!bioseq.IsAa()) {
              desc =  GetDiscItemText( (*it)->GetSeq());
              if (bioseq2cnt.find(desc) != bioseq2cnt.end()) {
                   if (cnt) 
                       bioseq2cnt[desc] = bioseq2cnt[desc] ? bioseq2cnt[desc]++ : cnt;
              }
              else bioseq2cnt[desc] = cnt;
          }
     }
     else CheckCommentCountForSet((*it)->GetSet(), cnt, bioseq2cnt);
   }
};

const CBioseq& CSeqEntry_test_on_user :: Get1stBioseqOfSet(const CBioseq_set& bioseq_set)
{
   const list < CRef < CSeq_entry > >& seq_entrys = bioseq_set.GetSeq_set();
   if (seq_entrys.empty()) {
        NCBI_USER_THROW("Bioseq_set does not contain any seq-entry");
   }
   if ( (*(seq_entrys.begin()))->IsSeq()) {
       if ( (*(seq_entrys.begin()))->GetSeq().IsNa() ) {
           return ((*(seq_entrys.begin()))->GetSeq());
       }
   }
   return (Get1stBioseqOfSet((*(seq_entrys.begin()))->GetSet()));
};

void CSeqEntry_test_on_user :: TestOnObj(const CSeq_entry& seq_entry)
{
  if (thisTest.is_DESC_user_run) return;
  thisTest.is_DESC_user_run = true;

  unsigned i=0, cnt;
  string bioseq_desc, type_str, user_desc;
  Str2Int bioseq2cnt;
  map <string, CConstRef <CObject> > bioseq2ref;

  for (CBioseq_CI b_ci(*thisInfo.scope, seq_entry, CSeq_inst::eMol_na); 
          b_ci; ++b_ci) {
     CConstRef <CBioseq> bsq_ref = b_ci->GetCompleteBioseq();
     m_bioseq2geno_comm[GetDiscItemText(*bsq_ref)]
               = CConstRef <CObject>(bsq_ref.GetPointer());
  }
  for (CBioseq_CI b_ci(*thisInfo.scope, seq_entry); b_ci; ++b_ci) {
     CConstRef <CBioseq> bsq_ref = b_ci->GetCompleteBioseq();
     m_seqs_no_pid[GetDiscItemText(*bsq_ref)]
                 = CConstRef <CObject>(bsq_ref.GetPointer());
  }

  bool run_gcomm = (thisTest.tests_run.find(GetName_gcomm()) != end_it);
  bool run_proj = (thisTest.tests_run.find(GetName_proj()) != end_it);
  bool run_oncall =(thisTest.tests_run.find(GetName_oncall_scomm())!=end_it);
  bool run_scomm = (thisTest.tests_run.find(GetName_scomm()) != end_it);
  bool run_mproj = (thisTest.tests_run.find(GetName_mproj()) != end_it);
  bool run_bproj = (thisTest.tests_run.find(GetName_bproj()) != end_it);
  bool run_prefix = (thisTest.tests_run.find(GetName_prefix()) != end_it);

  string id;
  ITERATE (vector <const CSeqdesc*>, it, user_seqdesc) {
    const CUser_object& user_obj = (*it)->GetUser();
    if ( !(user_obj.GetType().IsStr()) ) { 
         i++; continue;
    }
    type_str = user_obj.GetType().GetStr();
    user_desc = GetDiscItemText(**it, *(user_seqdesc_seqentry[i]));
    CBioseq_CI b_ci(*thisInfo.scope, *(user_seqdesc_seqentry[i]));

    // MISSING_GENOMEASSEMBLY_COMMENTS
    if (run_gcomm && type_str == "StructuredComment"
          && user_obj.HasField("StructuredCommentPrefix")
          && user_obj.GetField("StructuredCommentPrefix").GetData().IsStr()
          && user_obj.GetField("StructuredCommentPrefix").GetData().GetStr()
                      == "##Genome-Assembly-Data-START##") {
            // has comment, rm bioseqs from map
            for (CBioseq_CI bb_ci = b_ci; bb_ci; ++bb_ci) {
               if (bb_ci->IsNa()) 
                  m_bioseq2geno_comm[GetDiscItemText(*bb_ci->GetCompleteBioseq())].Reset(); 
            }
    }

    // TEST_HAS_PROJECT_ID
    if (run_proj 
            && type_str == "GenomeProjectsDB" 
            && user_obj.HasField("ProjectID")
            && user_obj.GetField("ProjectID").GetData().IsInt()) {
      id = NStr::IntToString(user_obj.GetField("ProjectID").GetData().GetInt());
      for (CBioseq_CI bb_ci = b_ci; bb_ci; ++bb_ci) {
         bioseq_desc = GetDiscItemText(*(bb_ci->GetCompleteBioseq()));
         CConstRef <CObject> seq_ref(bb_ci->GetCompleteBioseq().GetPointer());
         if (bb_ci->IsAa()) {
       //       PROJECT_ID_prot[id].push_back(bioseq_desc);
              thisInfo.test_item_list[GetName_proj()]
                         .push_back("prot$" + id + "#" + bioseq_desc);
              thisInfo.test_item_objs[GetName_proj() + "$prot#" + id]
                         .push_back(seq_ref);
         }
         else {
        //      PROJECT_ID_nuc[id].push_back(bioseq_desc);
              thisInfo.test_item_list[GetName_proj()]
                         .push_back("nuc$" + id + "#" + bioseq_desc);
              thisInfo.test_item_objs[GetName_proj() + "$nuc#" + id]
                         .push_back(seq_ref);
         }
      }
    }

    // ONCALLER_MISSING_STRUCTURED_COMMENTS, MISSING_STRUCTURED_COMMENT
    if (run_oncall || run_scomm) {
       cnt = (type_str == "StructuredComment") ? 1 : 0; // 0 = missing
       for (CBioseq_CI bb_ci = b_ci; bb_ci; ++bb_ci) {
          CConstRef <CBioseq> seq_ref = bb_ci->GetCompleteBioseq();
          if (!bb_ci->IsAa()) {
             bioseq_desc = GetDiscItemText(*seq_ref);
             if (bioseq2cnt.find(bioseq_desc) != bioseq2cnt.end()) {
                bioseq2ref[bioseq_desc] 
                    = CConstRef <CObject>(seq_ref.GetPointer());
                if (cnt) {
                   bioseq2cnt[bioseq_desc] 
                        = bioseq2cnt[bioseq_desc] 
                              ?  bioseq2cnt[bioseq_desc]++ : cnt;
                }
             }
             else {
                bioseq2cnt[bioseq_desc] = cnt;
             }
          }
       }
    }

    // MISSING_PROJECT
    if (run_mproj) {
        if (type_str == "GenomeProjectsDB" 
                  || (type_str == "DBLink" && user_obj.HasField("BioProject"))){
           for (CBioseq_CI bb_ci = b_ci; bb_ci; ++bb_ci) {
                CConstRef <CBioseq> bsq_ref = bb_ci->GetCompleteBioseq();
                m_seqs_no_pid[GetDiscItemText(*bsq_ref)].Reset();
           }
        }
    }

    // ONCALLER_BIOPROJECT_ID
    if (run_bproj 
            && type_str == "DBLink" && user_obj.HasField("BioProject")
            && user_obj.GetField("BioProject").GetData().IsStrs()) {
       const vector <CStringUTF8>& 
            ids = user_obj.GetField("BioProject").GetData().GetStrs();
       if (!ids.empty() 
             && !CUtf8::AsSingleByteString(ids[0], eEncoding_Ascii).empty()) {
         for (CBioseq_CI bb_ci = b_ci; bb_ci; ++bb_ci) {
           CConstRef <CBioseq> seq_ref = bb_ci->GetCompleteBioseq();
           thisInfo.test_item_list[GetName_bproj()]
                      .push_back( GetDiscItemText(*seq_ref));
           thisInfo.test_item_objs[GetName_bproj()]
                      .push_back(CConstRef <CObject> ( seq_ref.GetPointer()));
         }
       }
    }

    // ONCALLER_SWITCH_STRUCTURED_COMMENT_PREFIX
    if (run_prefix && type_str == "StructuredComment")  {
       try {
         string prefix = CComment_rule::GetStructuredCommentPrefix (user_obj);
         CConstRef<CComment_set> comment_rules =CComment_set::GetCommentRules();
            try {
              const CComment_rule& rule =comment_rules->FindCommentRule(prefix);
              CComment_rule::TErrorList errors = rule.IsValid(user_obj);
                if (errors.size() > 0) {
                    thisInfo.test_item_list[GetName_prefix()]
                               .push_back(user_desc); 
                    thisInfo.test_item_objs[GetName_prefix()]
                               .push_back(CConstRef <CObject> (*it));                     
                }
            } catch (CException& eu) {
            }
        } catch (CException) {
        }
    }

    i++;
  }

// MISSING_GENOMEASSEMBLY_COMMENTS
   if (run_gcomm) {
      ITERATE (Str2Obj, it, m_bioseq2geno_comm) {
         if (it->second.NotEmpty()) {
            thisInfo.test_item_list[GetName_gcomm()].push_back(it->first);
            thisInfo.test_item_objs[GetName_gcomm()].push_back(it->second);
         }
      }
   }

// ONCALLER_MISSING_STRUCTURED_COMMENTS, MISSING_STRUCTURED_COMMENT
   if (run_oncall || run_scomm) {
      ITERATE (Str2Int, it, bioseq2cnt) {
        if (run_oncall) {
             strtmp = NStr::IntToString(it->second);
             thisInfo.test_item_list[GetName_oncall_scomm()]
                        .push_back(strtmp + "$" + it->first);
             thisInfo.test_item_objs[GetName_oncall_scomm() + "$" + strtmp]
                       .push_back(bioseq2ref[it->first]);
        }
        if (run_scomm && !it->second) {
            thisInfo.test_item_list[GetName_scomm()].push_back(it->first);
            thisInfo.test_item_objs[GetName_scomm()]
                       .push_back(bioseq2ref[it->first]);
        }
      }
   }

// MISSING_PROJECT
   ITERATE (Str2Obj, it, m_seqs_no_pid) {
      if (it->second.NotEmpty()) {
         thisInfo.test_item_list[GetName_mproj()].push_back(it->first);
         thisInfo.test_item_objs[GetName_mproj()].push_back(it->second);
      }
   }

   bioseq2cnt.clear();
};

void CSeqEntry_ONCALLER_SWITCH_STRUCTURED_COMMENT_PREFIX :: GetReport(CRef <CClickableItem>& c_item)
{
  c_item->obj_list = thisInfo.test_item_objs[GetName()];
  c_item->description 
         = GetIsComment(c_item->item_list.size(), "structured comment")
             + "invalid but would be valid with a different prefix.";
};

void CSeqEntry_MISSING_STRUCTURED_COMMENT :: GetReport(CRef <CClickableItem>& c_item)
{
  RmvRedundancy(c_item->item_list);  // all CSeqEntry_Feat_desc tests need this.
  c_item->obj_list = thisInfo.test_item_objs[GetName()];
  c_item->description = GetDoesComment(c_item->item_list.size(), "sequence") 
                           + "not include structured comments.";
};

void CSeqEntry_ONCALLER_BIOPROJECT_ID :: GetReport(CRef <CClickableItem>& c_item)
{
  RmvRedundancy(c_item->item_list);  // all CSeqEntry_Feat_desc tests need this.
  c_item->obj_list = thisInfo.test_item_objs[GetName()];
  c_item->description = GetContainsComment(c_item->item_list.size(), "sequence")
                           + "BioProject IDs.";
};

void CSeqEntry_MISSING_PROJECT :: GetReport(CRef <CClickableItem>& c_item)
{
  c_item->obj_list = thisInfo.test_item_objs[GetName()];
  c_item->description
    = GetDoesComment(c_item->item_list.size(), "sequence") + "not include project.";
};

void CSeqEntry_ONCALLER_MISSING_STRUCTURED_COMMENTS :: GetReport(CRef <CClickableItem>& c_item)
{
  RmvRedundancy(c_item->item_list); // all CSeqEntry_Feat_desc tests need this.

  Str2Strs bioseq2cnt;
  GetTestItemList(c_item->item_list, bioseq2cnt); //bioseq2cnt Globle in 1 or multi file?
  c_item->item_list.clear();
  string desc;
  if (bioseq2cnt.size() > 1) {
    ITERATE (Str2Strs, it, bioseq2cnt) {
       if (it->first == "0") {
          desc = "no structured comment";
       }
       else {
          desc =it->first + " structured comment " + ((it->first =="1")?"":"s");
       }
       AddSubcategory(c_item,  GetName() + "$" + it->first, &(it->second), 
                      "sequence", desc, e_HasComment, false);
    }
    c_item->description 
         = "Sequences have different numbers of structured comments.";
  }
};

void CSeqEntry_MISSING_GENOMEASSEMBLY_COMMENTS :: GetReport(CRef <CClickableItem>& c_item)
{
  c_item->obj_list = thisInfo.test_item_objs[GetName()];
  c_item->description = GetIsComment(c_item->item_list.size(), "bioseq")
                            + "missing GenomeAssembly structured comments.";
};


void CSeqEntry_TEST_HAS_PROJECT_ID :: GetReport(CRef <CClickableItem>& c_item)
{
   RmvRedundancy(c_item->item_list);   // all CSeqEntry_Feat_desc tests need this.

   unsigned prot_cnt = 0, nuc_cnt = 0, prot_id1 = 0, nuc_id1 = 0;
   Str2Strs mol2list, prot_id2seqs, nuc_id2seqs;
   GetTestItemList(c_item->item_list, mol2list);
   c_item->item_list.clear();
   ITERATE (Str2Strs, it, mol2list) {
     if (it->first == "prot") {
         prot_cnt = it->second.size();
         GetTestItemList(it->second, prot_id2seqs, "#");
     }
     else {
        nuc_cnt = it->second.size();
        GetTestItemList(it->second, nuc_id2seqs, "#");
     }
   }

   string tot_desc, prot_desc, nuc_desc;
//   if (PROJECT_ID_prot.size() <= 1 && PROJECT_ID_nuc.size() <= 1) {
   if (prot_cnt <= 1 && nuc_cnt <= 1) {
      if (prot_cnt)  {
//            prot_id1 = PROJECT_ID_prot.begin()->first;
          prot_id1 = NStr::StringToUInt(prot_id2seqs.begin()->first);
      }
      if (nuc_cnt) {
//          nuc_id1 = PROJECT_ID_nuc.begin()->first;
          nuc_id1 = NStr::StringToUInt(nuc_id2seqs.begin()->first);
      }
      if ( (prot_id1 == nuc_id1) ||  !(prot_id1 * nuc_id1)) {
            tot_desc = "(all same)";
      }
      else tot_desc = " (some different)";
   }

//   if (prot_cnt && PROJECT_ID_prot.size() > 1) {
   if (prot_cnt && prot_id2seqs.size() > 1) {
        prot_desc = "(some different)";
   }
   else {
        prot_desc = "(all same)";
   }

   if (nuc_cnt && nuc_id2seqs.size() > 1) {
         nuc_desc = "(some different)";
   }
   else {
       nuc_desc = "(all same)";
   }

   if (prot_cnt && !nuc_cnt) {
       ITERATE (Str2Strs, it, prot_id2seqs) {
          copy(it->second.begin(), it->second.end(), 
              back_inserter(c_item->item_list));
          vector <CConstRef <CObject> >*
            objs = &(thisInfo.test_item_objs[GetName() + "$prot#" + it->first]);
          copy(objs->begin(), objs->end(), back_inserter(c_item->obj_list));
       }
       c_item->description 
           = GetOtherComment(c_item->item_list.size(),
             "protein sequence has project ID ",
             "protein sequences have project IDs " + prot_desc);
   }
   else if (!prot_cnt && nuc_cnt) {
       //c_item->item_list = mol2list.begin()->second;
       ITERATE (Str2Strs, it, nuc_id2seqs) {
          copy(it->second.begin(), it->second.end(), 
                 back_inserter(c_item->item_list));
          vector <CConstRef <CObject> >*
            objs = &(thisInfo.test_item_objs[GetName() + "$nuc#" + it->first]);
          copy(objs->begin(), objs->end(), back_inserter(c_item->obj_list));
       }
       c_item->description 
          = GetOtherComment(c_item->item_list.size(),
             "nucleotide sequence has project ID ",
             "nucleotide sequences have project IDs " + nuc_desc);
   }
   else {
       c_item->description 
               = GetOtherComment(prot_cnt + nuc_cnt, "sequence has project ID ",
                                "sequencs have project IDs " + tot_desc);
       ITERATE (Str2Strs, it, nuc_id2seqs) {
          AddSubcategory(c_item, GetName() + "$nuc#" + it->first, 
                         &it->second, "nucleotide sequence has project ID", 
                         "nucleotide sequences have project IDs",
                          e_OtherComment, false);
       }
       ITERATE (Str2Strs, it, prot_id2seqs) {
          AddSubcategory(c_item, GetName() + "$prot#" + it->first, 
                         &it->second, "protein sequence has project ID",
                         "protein sequences have project IDs", 
                          e_OtherComment, false);
       }

       c_item->expanded = true; 
   }

/*
   PROJECT_ID_prot.clear();
   PROJECT_ID_nuc.clear();
*/
};

// new method

CFlatFileConfig::CGenbankBlockCallback::EBioseqSkip CFlatfileTextFind::notify_bioseq(CBioseqContext& ctx )
{
   m_taxname = ctx.GetTaxname();
   CBioseq_Handle& bioseq_hl = ctx.GetHandle();
   if (!bioseq_hl) {
      return (eBioseqSkip_Yes);
   }
   CConstRef <CBioseq> bioseq_ref = bioseq_hl.GetCompleteObject();
   m_bioseq_desc = GetDiscItemText(*bioseq_ref);

   m_src_desc = m_mol_desc = m_tlt_desc = m_pub_desc = m_gbk_desc = kEmptyStr;
   for (CSeqdesc_CI it(bioseq_hl, m_seqdesc_sel); it; ++it) {
     switch (it->Which()) {
        case CSeqdesc::e_Source:
            m_src_desc = GetDiscItemText(*it, *bioseq_ref);
            m_obj_ref.Reset(&(*it));
            break;
        case CSeqdesc::e_Molinfo:
            m_mol_desc = GetDiscItemText(*it, *bioseq_ref);
            m_obj_ref.Reset(&(*it));
            break;
        case CSeqdesc::e_Title:
            m_tlt_desc = GetDiscItemText(*it, *bioseq_ref);
            m_obj_ref.Reset(&(*it));
            break;
        case CSeqdesc::e_Pub:
            m_pub_desc = GetDiscItemText(*it, *bioseq_ref);
            m_obj_ref.Reset(&(*it));
            break;
        case CSeqdesc::e_Genbank:
            m_gbk_desc = GetDiscItemText(*it, *bioseq_ref);
            m_obj_ref.Reset(&(*it));
            break;
        default: break;
     }

   }

   return (eBioseqSkip_No); 
};

CFlatFileConfig::CGenbankBlockCallback::EAction   CFlatfileTextFind::notify(string& block_text, const CBioseqContext& ctx, const CSourceItem& source_item)
{
  block_text = block_text.substr(0, block_text.find("ORGANISM")); // double check  
  return unified_notify(block_text, ctx, source_item, 
                                       CFlatFileConfig::fGenbankBlocks_Source);
};

CFlatFileConfig::CGenbankBlockCallback::EAction CFlatfileTextFind::notify(string& block_text, const CBioseqContext& ctx, const CFeatureItem& feature_item)
{
   size_t pos = block_text.find("/translation=\"");
   string trans_str;
   if (pos != string::npos) {
     strtmp = CTempString(block_text).substr(pos); 
     block_text = block_text.substr(0, pos-1);
     pos = CTempString(strtmp).substr(14).find("\"");
     if (pos != string::npos) {
       if (pos < strtmp.size() - 1) {
           block_text += CTempString(strtmp).substr(pos+1);
       }
     }
   }
   return unified_notify(block_text, ctx, feature_item, 
                                    CFlatFileConfig::fGenbankBlocks_FeatAndGap);
};


CFlatFileConfig::CGenbankBlockCallback::EAction CFlatfileTextFind::unified_notify( string & block_text, const CBioseqContext& ctx, const IFlatItem & flat_item, CFlatFileConfig::FGenbankBlocks which_block )
{
  block_text 
    =CTestAndRepData::FindReplaceString(block_text, m_taxname, "", false, true);
  //ITERATE (Str2UInt, it, thisInfo.whole_word) {
  ITERATE (Str2CombDt, it, thisInfo.fix_data) {
     CConstRef <CObject> obj_ref;
     if (CTestAndRepData :: DoesStringContainPhrase( 
                           block_text, it->first, false,  it->second.b_val)){
       switch (which_block) {
         case CFlatFileConfig::fGenbankBlocks_Locus: {
                 strtmp = m_mol_desc; 
                 obj_ref = m_obj_ref;
                 break;
         }
         case CFlatFileConfig::fGenbankBlocks_Defline: {
                 strtmp = m_tlt_desc; 
                 obj_ref = m_obj_ref;
                 break;
         }
         case CFlatFileConfig::fGenbankBlocks_Keywords: {
                 strtmp = m_gbk_desc; 
                 obj_ref = m_obj_ref;
                 break;
         }
         case CFlatFileConfig::fGenbankBlocks_Reference: {
                 strtmp = m_pub_desc; 
                 obj_ref = m_obj_ref;
                 break;
         }
         case CFlatFileConfig::fGenbankBlocks_Source: {
                 strtmp = m_src_desc; 
                 obj_ref = m_obj_ref;
                 break;
         }
         case CFlatFileConfig::fGenbankBlocks_Sourcefeat: 
         case CFlatFileConfig::fGenbankBlocks_FeatAndGap:
            {
              const CFeatureItemBase& feat_item 
                            = dynamic_cast<const CFeatureItemBase&> (flat_item);
              const CSeq_feat& sf = feat_item.GetFeat().GetOriginalFeature();
              strtmp = GetDiscItemText(sf);
              obj_ref.Reset(&sf);
              break;
            }
         default: strtmp = m_bioseq_desc;
                  obj_ref = m_obj_ref;
       };
       strtmp 
         = thisInfo.fix_data[it->first].s_val + "$" + it->first + "#" + strtmp;
       thisInfo.test_item_list[m_setting_name].push_back(strtmp);
       strtmp = m_setting_name + "$" + thisInfo.fix_data[it->first].s_val 
                 + "#" + it->first;
       thisInfo.test_item_objs[strtmp].push_back(obj_ref);
     }
  } 

  return eAction_Skip;  // flat file gen. won't print out blocks
};

void CSeqEntry_DISC_FLATFILE_FIND_ONCALLER :: TestOnObj(const CSeq_entry& seq_entry)
{
   CFlatfileTextFind flatfile_find(GetName());
   unsigned blocks = CFlatFileConfig::fGenbankBlocks_All 
                             & ~CFlatFileConfig::fGenbankBlocks_Sequence;
   CFlatFileConfig cfg(CFlatFileConfig::eFormat_GenBank,
                       CFlatFileConfig::eMode_GBench,
                       CFlatFileConfig::eStyle_Normal,
                       CFlatFileConfig::fShowContigFeatures,
                       CFlatFileConfig::fViewAll,
                       CFlatFileConfig::fGffGTFCompat,
                       (CFlatFileConfig::FGenbankBlocks)blocks,
                       &flatfile_find);
   CFlatFileGenerator gen(cfg);
   CSeq_entry_Handle entry_hl = thisInfo.scope->GetSeq_entryHandle(seq_entry);
   gen.Generate( entry_hl, cout); // slow
};

void CSeqEntry_DISC_FLATFILE_FIND_ONCALLER :: AddCItemToReport(const string& ls_type, const string& setting_name, CRef <CClickableItem>& c_item)
{
   Str2Strs wrds2ls;
   if (m_fixable2ls.size() == 1 && m_fixable2ls.begin()->first != ls_type) {
        return;
   }
   ITERATE (Str2Strs, it, m_fixable2ls) {
     if (it->first == ls_type) {
        wrds2ls.clear();
        GetTestItemList(it->second, wrds2ls, "#");
        ITERATE (Str2Strs, jt, wrds2ls) {
           if (m_citem1) m_citem1 = false;
           else {
              c_item.Reset(new CClickableItem);
              c_item->setting_name = setting_name;
              thisInfo.disc_report_data.push_back(c_item);
           }
           c_item->item_list = jt->second;
           strtmp = setting_name + "$" + it->first + "#" + jt->first;
           c_item->obj_list = thisInfo.test_item_objs[strtmp];
           c_item->description
           = GetContainsComment(c_item->item_list.size(), "object") + jt->first;
        }
     }
   }
   
};

void CSeqEntry_DISC_FLATFILE_FIND_ONCALLER :: GetReport(CRef <CClickableItem>& c_item)
{
   m_fixable2ls.clear();
   GetTestItemList(c_item->item_list, m_fixable2ls); 
   c_item->item_list.clear();
   m_citem1 = true;
   if (thisTest.tests_run.find(GetName()) != end_it
          || thisTest.tests_run.find(GetName_nofix()) != end_it) {
       AddCItemToReport("no", GetName_nofix(), c_item);
   }
   if (thisTest.tests_run.find(GetName()) != end_it
          || thisTest.tests_run.find(GetName_fix()) != end_it) {
      AddCItemToReport("yes", GetName_fix(), c_item); 
   }
};

bool CSeqEntry_ONCALLER_STRAIN_TAXNAME_CONFLICT :: StrainConflictsTaxname(const COrg_ref& org)
{
   vector <string> arr;
   GetOrgModValues(org, COrgMod::eSubtype_other, arr);
   unsigned len;
   string tax_nm = org.GetTaxname();
   if (!arr.empty()) {
      ITERATE (vector <string>, iit, arr) {   
        ITERATE (vector <string>, it, thisInfo.strain_tax) {
          len = (*it).size();
          if (NStr::EqualNocase((*iit).substr(0, len), *it)) {
             strtmp = CTempString(*iit).substr(len);
             NStr::TruncateSpacesInPlace(strtmp);
             if (strtmp != tax_nm) {
                 return true;
             }
          }
        }
      }
   }

   return false;
};


void CSeqEntry_ONCALLER_STRAIN_TAXNAME_CONFLICT :: TestOnObj(const CSeq_entry& seq_entry)
{
   bool has_tax;
   ITERATE (vector <const CSeq_feat*>, it, biosrc_orgmod_feat) {
     const CBioSource& biosrc = (*it)->GetData().GetBiosrc();
     has_tax = (biosrc.IsSetTaxname() && !biosrc.GetTaxname().empty());
     if (has_tax && StrainConflictsTaxname(biosrc.GetOrg())) {
        thisInfo.test_item_list[GetName()].push_back(GetDiscItemText(**it));
        thisInfo.test_item_objs[GetName()].push_back(CConstRef <CObject>(*it));
     }
   }

   ITERATE (vector <const CSeq_feat*>, it, org_orgmod_feat) {
      const COrg_ref& org = (*it)->GetData().GetOrg();
      has_tax = (org.IsSetTaxname() && !org.GetTaxname().empty());
      if (has_tax && StrainConflictsTaxname(org)) {
        thisInfo.test_item_list[GetName()].push_back(GetDiscItemText(**it));
        thisInfo.test_item_objs[GetName()].push_back(CConstRef <CObject>(*it));
      }
   }

   unsigned i=0;
   ITERATE (vector <const CSeqdesc*>, it, biosrc_orgmod_seqdesc) {
     const CBioSource& biosrc = (*it)->GetSource();
     has_tax = (biosrc.IsSetTaxname() && !biosrc.GetTaxname().empty());
     if ( has_tax && StrainConflictsTaxname(biosrc.GetOrg())) {
         thisInfo.test_item_list[GetName()].push_back(
               GetDiscItemText(**it, *(biosrc_orgmod_seqdesc_seqentry[i])));
         thisInfo.test_item_objs[GetName()].push_back(CConstRef <CObject>(*it));
     }
     i++;
   };
   
   i=0;
   ITERATE (vector <const CSeqdesc*>, it, org_orgmod_seqdesc) {
     const COrg_ref& org = (*it)->GetOrg();
     has_tax = (org.IsSetTaxname() && !org.GetTaxname().empty());
     if (has_tax && StrainConflictsTaxname( org )) {
         thisInfo.test_item_list[GetName()].push_back(
                      GetDiscItemText(**it, *(org_orgmod_seqdesc_seqentry[i])));
         thisInfo.test_item_objs[GetName()].push_back(CConstRef <CObject>(*it));
     }
     i++;
   }
};

void CSeqEntry_ONCALLER_STRAIN_TAXNAME_CONFLICT :: GetReport(CRef <CClickableItem>& c_item)
{
   c_item->obj_list = thisInfo.test_item_objs[GetName()];
   c_item->description = GetHasComment(c_item->item_list.size(), "sequence") 
                           + "conflicts between type strain and organism name.";
};

bool CSeqEntry_TEST_SMALL_GENOME_SET_PROBLEM :: HasSmallSeqset(const CSeq_entry& seq_entry)
{
   if (seq_entry.IsSet()) {
     if (seq_entry.GetSet().GetClass() == CBioseq_set::eClass_small_genome_set){
          return true;
     }
     else {
        ITERATE(list <CRef <CSeq_entry> >, it, seq_entry.GetSet().GetSeq_set()){
            if (HasSmallSeqset(**it)) {
                return true;
            }
        }
     }
   } 

   return false;
};

void CSeqEntry_TEST_SMALL_GENOME_SET_PROBLEM :: TestOnObj(const CSeq_entry& seq_entry)
{
   if (!HasSmallSeqset(seq_entry)) {
        return;
   }
   string pre_taxname, pre_isolate, pre_strain;
   pre_taxname = pre_isolate = pre_strain = kEmptyStr;
   bool all_taxnames_same, all_isolates_same, all_strains_same;
   all_taxnames_same = all_isolates_same = all_strains_same = true;
   unsigned i=0;
   string desc, desc_entry(GetDiscItemText(seq_entry));
   vector <string> arr;
   ITERATE (vector <const CSeqdesc*>, it, biosrc_seqdesc) {
      const CBioSource& biosrc = (*it)->GetSource();
      desc = GetDiscItemText(**it, *biosrc_seqdesc_seqentry[i]);
      CConstRef <CObject> obj_ref(*it);
      if (HasLineage(biosrc, "Viruses")) {
         if (!IsSubSrcPresent(biosrc, CSubSource::eSubtype_segment)) {
             thisInfo.test_item_list[GetName()].push_back("missing$" + desc);
             thisInfo.test_item_objs[GetName() + "$missing"].push_back(obj_ref);
         }
      }
      if (all_taxnames_same) {
         desc = GetSrcQualValue(biosrc, "taxname");
         if (pre_taxname.empty() && !desc.empty()) {
                pre_taxname = desc;
         }
         else if (desc != pre_taxname) {
             all_taxnames_same = false;  
         }
      }
      if (all_isolates_same) {
         arr.clear();
         GetOrgModValues(biosrc, COrgMod::eSubtype_isolate, arr);
         all_isolates_same = AllVecElesSame(arr);
         if (all_isolates_same) {
           if (pre_isolate.empty() && !arr[0].empty()) {
                pre_isolate = arr[0];
           }
           else if (arr[0] != pre_isolate) {
               all_isolates_same = false;
           }
         }
      }
      if (all_strains_same) {
         arr.clear();
         GetOrgModValues(biosrc, COrgMod::eSubtype_strain, arr);
         all_strains_same = AllVecElesSame(arr);
         if (all_strains_same) {
            if (pre_strain.empty() && !arr[0].empty()) {
                  pre_strain = arr[0];
            }
            else if (arr[0] != pre_strain) {
               all_strains_same = false;
            }
         }
      }
   }
 
   if (!all_taxnames_same) {
       thisInfo.test_item_list[GetName()].push_back("taxname$1");
   }
   if (!all_isolates_same) {
       thisInfo.test_item_list[GetName()].push_back("isolate$1");
   }
   if (!all_strains_same) {
       thisInfo.test_item_list[GetName()].push_back("strain$1");
   }
};

void CSeqEntry_TEST_SMALL_GENOME_SET_PROBLEM :: GetReport(CRef <CClickableItem>& c_item)
{
   Str2Strs qual2ls;
   GetTestItemList(c_item->item_list, qual2ls);
   c_item->item_list.size();
   unsigned cnt;
   ITERATE (Str2Strs, it, qual2ls) {
     if (it != qual2ls.begin() ) { 
             c_item.Reset(new CClickableItem);
             c_item->setting_name = GetName();
             thisInfo.disc_report_data.push_back(c_item);
     } 
     if (it->first == "missing") {
        c_item->item_list = it->second;
        c_item->obj_list = thisInfo.test_item_objs[GetName() + "$missing"];
        cnt = it->second.size(); 
        c_item->description = GetOtherComment(cnt, "biosourc", "biosource")
                                 + " should have segment qualifier but " 
                                 + ( cnt>1? "do not" : "does not"); 
     }
     else {
         c_item->description = "Not all biosources have same " + it->first;
     }
   }
};


bool CSeqEntry_DISC_SUBMITBLOCK_CONFLICT :: CitSubMatchExceptDate(const CCit_sub& cit1, const CCit_sub& cit2)
{
   if (Blob2Str(cit1.GetAuthors()) != Blob2Str(cit2.GetAuthors())) {
       return false; 
   }
   if (cit1.CanGetImp() != cit2.CanGetImp()) {
       return false;
   }
   else if (cit1.CanGetImp() 
               && Blob2Str(cit1.GetImp()) != Blob2Str(cit2.GetImp())) {
         return false;
   }
   string desc1 = cit1.CanGetDescr()? cit1.GetDescr() : kEmptyStr;
   string desc2 = cit2.CanGetDescr()? cit2.GetDescr() : kEmptyStr;
   if (desc1 != desc2) {
         return false;
   }
   int med1 = cit1.CanGetMedium()? cit2.GetMedium() : 0;
   int med2 = cit2.CanGetMedium() ? cit2.GetMedium() : 0; 
   if (med1 != med2) {
         return false;
   }
   return true; 
};


bool CSeqEntry_DISC_SUBMITBLOCK_CONFLICT :: DateMatch(const CSubmit_block& blk1, const CSubmit_block& blk2) 
{
   if (blk1.CanGetReldate() != blk2.CanGetReldate()) {
       return false;
   }
   else if (blk1.CanGetReldate()
              && CDate::eCompare_same != blk1.GetReldate().Compare(blk2.GetReldate())) {
      return false;
   }

   return true;
};


static vector <string> SUBMIT_BLKs;
string CSeqEntry_DISC_SUBMITBLOCK_CONFLICT :: SubmitBlockMatchExceptDate(const CSubmit_block& this_blk)
{
   CSubmit_block blk_in_ls;
   bool match;
   string ctat, this_ctat, str1, str2;
   this_ctat = Blob2Str(this_blk.GetContact());
   int subtp1, subtp2;
   unsigned i=0;
   ITERATE (vector <string>, it, SUBMIT_BLKs) {
      blk_in_ls.Reset();
      Str2Blob(*it, blk_in_ls);
      ctat = Blob2Str(blk_in_ls.GetContact());
      if (this_ctat != ctat
             || !CitSubMatchExceptDate(this_blk.GetCit(), blk_in_ls.GetCit())
             || (this_blk.GetHup() != blk_in_ls.GetHup())
             || ( this_blk.GetHup() && !DateMatch(this_blk, blk_in_ls))) {
          match = false;
      }
      else {
         subtp1 = blk_in_ls.CanGetSubtype() ? blk_in_ls.GetSubtype() : 0;
         subtp2 = this_blk.CanGetSubtype() ? this_blk.GetSubtype() : 0;
         if (subtp1 != subtp2) {
             match = false;
         }
         else {
            str1 = blk_in_ls.CanGetTool() ? blk_in_ls.GetTool() : kEmptyStr;
            str2 = this_blk.CanGetTool() ? this_blk.GetTool() : kEmptyStr;
            if (str1 != str2) {
                match = false;
            }
            else {
              str1 = blk_in_ls.CanGetUser_tag() ? blk_in_ls.GetUser_tag() : kEmptyStr;
              str2 = this_blk.CanGetUser_tag() ? this_blk.GetUser_tag() : kEmptyStr;
              if (str1 != str2) {
                   match = false;
              }
              else {
                 str1 = blk_in_ls.CanGetComment() ? blk_in_ls.GetComment() : kEmptyStr;
                 str2 = this_blk.CanGetComment() ? this_blk.GetComment() : kEmptyStr;
                 if (str1 != str2) {
                      match = false;
                 }
                 else {
                    match = true;
                 }
              }
            } 
         }
      }

      if (match) {
           return (NStr::UIntToString(i));
      }
      i++;
   }
   SUBMIT_BLKs.push_back(Blob2Str(this_blk));
   return (NStr::UIntToString((unsigned)SUBMIT_BLKs.size()));
};



void CSeqEntry_DISC_SUBMITBLOCK_CONFLICT :: TestOnObj(const CSeq_entry& seq_entry)
{
   string desc;
   if (thisInfo.seq_submit.Empty()) { // missing_list
      if (seq_entry.IsSeq()) {
         thisInfo.test_item_list[GetName()]
                   .push_back("missing$" + GetDiscItemText(seq_entry.GetSeq()));
         thisInfo.test_item_objs[GetName() + "$missing"]
                   .push_back(CConstRef <CObject>(&seq_entry.GetSeq()));
      }
      else {
         thisInfo.test_item_list[GetName()]
                   .push_back("missing$" + GetDiscItemText(seq_entry.GetSet()));
         thisInfo.test_item_objs[GetName() + "$missing"]
                   .push_back(CConstRef <CObject>(&seq_entry.GetSet()));
      }
      return;
   };   

   string idx;
   if (SUBMIT_BLKs.empty()) {
       SUBMIT_BLKs.push_back(Blob2Str(thisInfo.seq_submit->GetSub()));
       idx = "0";
   }   
   else {
        idx = SubmitBlockMatchExceptDate(thisInfo.seq_submit->GetSub());
   }
   CConstRef <CObject> obj_ref;
   if (seq_entry.IsSeq()) {
       desc = GetDiscItemText(seq_entry.GetSeq());
       obj_ref = CConstRef <CObject>(&seq_entry.GetSeq());
   }
   else {
       desc = GetDiscItemText(seq_entry.GetSet());
       obj_ref = CConstRef <CObject>(&seq_entry.GetSet());
   }
   thisInfo.test_item_list[GetName()].push_back(idx + "$" + desc);
   thisInfo.test_item_objs[GetName() + "$" + idx].push_back(obj_ref);
};


void CSeqEntry_DISC_SUBMITBLOCK_CONFLICT :: GetReport(CRef <CClickableItem>& c_item)
{
   Str2Strs blk2objs;
   GetTestItemList(c_item->item_list, blk2objs);
   c_item->item_list.clear();
   if (blk2objs.size() == 1 && blk2objs.begin()->first == "missing") {
       return;
   }
   ITERATE (Str2Strs, it, blk2objs) {
      strtmp = (it->first =="missing")? "no submit-block" 
                                      : "identical submit-blocks";
      AddSubcategory(c_item, 
                     GetName() + "$" + it->first,
                     &(it->second), 
                     "record", 
                     strtmp, 
                     e_HasComment, 
                     false);
   }
   c_item->description = "SubmitBlock Conflicts";
};


void CSeqEntry_DISC_INCONSISTENT_MOLTYPES :: TestOnObj(const CSeq_entry& seq_entry)
{
   string mol_nm, biomol_nm, entry_no;
   entry_no = NStr::UIntToString(m_entry_no);
   for (CBioseq_CI b_ci(*thisInfo.scope, seq_entry); b_ci; ++ b_ci) {
      if (b_ci->IsAa()) {
          continue;
      }
      const CMolInfo* molinfo = sequence::GetMolInfo(*b_ci);
      biomol_nm = CMolInfo::ENUM_METHOD_NAME(EBiomol)()
                       ->FindName(molinfo->GetBiomol(), false);
      mol_nm = CSeq_inst::ENUM_METHOD_NAME(EMol)()
                  ->FindName(b_ci->GetInst_Mol(), false);
      strtmp = entry_no + "$" + biomol_nm + " " + mol_nm;
      CConstRef <CBioseq> seq_ref = b_ci->GetCompleteBioseq();
      thisInfo.test_item_list[GetName()]
                 .push_back(strtmp + "#" + GetDiscItemText(*seq_ref));
      strtmp = "$" + entry_no + "#" + biomol_nm + " " + mol_nm;
      thisInfo.test_item_objs[GetName() + strtmp]
                 .push_back(CConstRef <CObject> (seq_ref.GetPointer()));
   } 
   m_entry_no ++;
};


void CSeqEntry_DISC_INCONSISTENT_MOLTYPES :: GetReport(CRef <CClickableItem>& c_item)
{
  Str2Strs entry2moltps, moltp2seqs;  
  GetTestItemList(c_item->item_list, entry2moltps);
  c_item->item_list.clear();
  string entry_no, mol_tp;
  ITERATE (Str2Strs, it, entry2moltps) {
    entry_no = it->first;
    moltp2seqs.clear();
    GetTestItemList(it->second, moltp2seqs, "#");
    if (moltp2seqs.size() > 1) {
       ITERATE (Str2Strs, jt, moltp2seqs) {
         mol_tp = jt->first;
         strtmp = GetName() + "$" + entry_no + "#" + mol_tp;
         AddSubcategory(c_item, strtmp, &(jt->second), "sequence", 
                        "moltype " + mol_tp, e_HasComment); 
       } 
    }
  } 
  if (c_item->subcategories.empty()) {
           c_item->description = "Moltypes are consistent";
  }
  else {
      c_item->description 
           = GetHasComment(c_item->item_list.size(), "sequence") 
             + "inconsistent moltypes"; 
  }
};


void CSeqEntry_DISC_HAPLOTYPE_MISMATCH :: ExtractNonAaBioseqsOfSet(const string& tax_hap, const CBioseq_set& set)
{
   CBioseq_set_Handle set_hl = thisInfo.scope->GetBioseq_setHandle(set);
   for (CBioseq_CI seq_ci(set_hl); seq_ci; ++seq_ci) {
      if (!(seq_ci->IsAa())) {
           m_tax_hap2seqs[tax_hap].push_back(
                            CConstRef <CBioseq> (seq_ci->GetCompleteBioseq()));
      }
   }
};

bool CSeqEntry_DISC_HAPLOTYPE_MISMATCH :: SeqMatch(const CConstRef <CBioseq>& seq1, int beg1, const CConstRef <CBioseq>& seq2, int beg2, unsigned& len, bool Ndiff)
{
   CBioseq_Handle hdl1 = thisInfo.scope->GetBioseqHandle(*seq1);
   CBioseq_Handle hdl2 = thisInfo.scope->GetBioseqHandle(*seq2);
   if (!hdl1 && !hdl2) {
     return true;
   }
   else if (!hdl1 || !hdl2) {
      return false;
   }

   CSeqVector seq_vec1 = hdl1.GetSeqVector(CBioseq_Handle::eCoding_Iupac, eNa_strand_plus); 
   CSeqVector seq_vec2 = hdl2.GetSeqVector(CBioseq_Handle::eCoding_Iupac, eNa_strand_plus); 
   unsigned sz=0;
   if (seq_vec1.empty() && seq_vec2.empty()) return true;
   else if (seq_vec1.empty() || seq_vec2.empty()) return false;
   CSeqVector_CI it1 = seq_vec1.begin();
   CSeqVector_CI it2 = seq_vec2.begin();
   for (;   it1 && it2 && (sz < len); ++it1, ++it2, sz++) {
      if (Ndiff && ( *it1 == 'N' || *it2 == 'N' )) continue;
      else if ( *it1 == *it2 ) continue;
      else return false;
   }
   return true;
};

bool CSeqEntry_DISC_HAPLOTYPE_MISMATCH :: SeqsMatch(const vector <CConstRef <CBioseq> >& seqs, bool Ndiff)
{
  unsigned len1, len2;
  bool rvlu = true;
  int diff, i, j;
  for (i=0; i < (int)seqs.size() -1 && !rvlu; i++)
     for (j=0; (j< (int)seqs.size()) && rvlu; j++) {
       len1 = seqs[i]->IsSetLength() ? seqs[i]->GetLength() : 0;
       len2 = seqs[j]->IsSetLength() ? seqs[j]->GetLength() : 0;
       diff = len1 - len2;
       if (diff > 0) {
         while (!rvlu && diff >= 0) {
            rvlu = SeqMatch(seqs[i], diff, seqs[j], 0, len2, Ndiff);
            diff --;
         }
       }
       else if (diff < 0) {
           diff = -diff;
           while (!rvlu && diff >=0) {
             rvlu = SeqMatch(seqs[i], 0, seqs[j], diff, len1, Ndiff);
             diff --;
           }
       }
       else rvlu = SeqMatch(seqs[i], 0, seqs[j], 0, len1, Ndiff);
     }

  return rvlu;
};


void CSeqEntry_DISC_HAPLOTYPE_MISMATCH :: ReportOneHaplotypeSequenceMismatch(Str2Seqs::const_iterator& iter, bool Ndiff)
{
   strtmp = (string)"seq_diff_" + (Ndiff ? "N" : "strict") 
                                 + "_" + NStr::UIntToString(m_entry_cnt);
   ITERATE (vector <CConstRef <CBioseq> >, it, iter->second) {
     thisInfo.test_item_list[GetName()].push_back(
                strtmp + "$" + iter->first + "@" + GetDiscItemText(**it));
     strtmp = "$" + strtmp + "@" + iter->first;
     thisInfo.test_item_objs[GetName() + "$" + strtmp].push_back(
                                                   CConstRef <CObject> (*it));
   }
};


void CSeqEntry_DISC_HAPLOTYPE_MISMATCH :: ReportHaplotypeSequenceMismatchForList()
{
   /* first, look for same taxname, same haplotype, different sequence */
   bool Ndiff_mismatch, strict_mismatch;
   ITERATE (Str2Seqs, it, m_tax_hap2seqs) {
      Ndiff_mismatch = strict_mismatch = false;
      Ndiff_mismatch = SeqsMatch(it->second);
      strict_mismatch = SeqsMatch(it->second, false);
      if (Ndiff_mismatch) {
           ReportOneHaplotypeSequenceMismatch(it);
      }
      if (strict_mismatch) {
           ReportOneHaplotypeSequenceMismatch(it, false);
      }
   }

   /* now look for sequence that match but have different haplotypes */
   vector <CConstRef <CBioseq> > seqs;
   vector <string> hap_tps;
   vector <unsigned> hap_idx;
   unsigned i;
   ITERATE (Str2Seqs, it, m_tax_hap2seqs) {
     hap_tps.push_back(CTempString(it->first).substr(it->first.find("$") + 1));
     copy(it->second.begin(), it->second.end(), back_inserter(seqs));
     for (i=0; i< it->second.size(); i++) {
            hap_idx.push_back(hap_tps.size()-1);
     }
   }

   unsigned j, len1, len2;
//   vector <string> seqs_Ndiff, seqs_strict;
   vector <unsigned> seqs_Ndiff_idx, seqs_strict_idx;
   string desc;
   bool mismatch, match_strict, match_Ndiff;
   for (i=0; (int)i < (int)(seqs.size()-1); i++) {
      if (seqs[i].Empty()) continue;
      len1 = seqs[i]->IsSetLength() ? seqs[i]->GetLength() : 0; 
/*
      desc1 = GetDiscItemText(*seqs[i]) + ": " + hap_tps[hap_idx[i]];
      CConstRef <CObject> obj1 (seqs[i].GetPointer());
*/
      mismatch = false;
      for (j=i+1; j < seqs.size(); j++) {
         if (seqs[j].Empty()) continue;
         len2 = seqs[j]->IsSetLength() ? seqs[j]->GetLength() : 0;
         if (len1 == len2) {
           match_strict = match_Ndiff = false;
/*
           desc2 = GetDiscItemText(*seqs[j]) + ": " + hap_tps[hap_idx[j]];
           CConstRef <CObject> obj2 (seqs[j].GetPointer());
*/
           if (SeqMatch(seqs[i], 0, seqs[j], 0, len1) ) {
/*
               if (seqs_Ndiff.empty()) {
                   seqs_Ndiff.push_back(desc1);
               }
               seqs_Ndiff.push_back(desc2);
*/
               if (seqs_Ndiff_idx.empty()) {
                   seqs_Ndiff_idx.push_back(i);
               }
               seqs_Ndiff_idx.push_back(j);
               match_Ndiff = true;  
           }
           if (SeqMatch(seqs[i], 0, seqs[j], 0, len1, false)) {
/*
               if (seqs_strict.empty()) {
                   seqs_strict.push_back(desc1);
               }
               seqs_strict.push_back(desc2);
*/
               if (seqs_strict_idx.empty()) {
                   seqs_strict_idx.push_back(i);
               }
               seqs_strict_idx.push_back(j);
               match_strict = true;
           }
           if (match_Ndiff || match_strict) {
               seqs[j].Reset();
           }
           if (hap_tps[hap_idx[j]] != hap_tps[hap_idx[i]]) {
                 mismatch = true;
           }
         } 
      }
      if (mismatch) {
          strtmp = "seq_same_N_" + NStr::UIntToString(m_entry_cnt) + "$";
          ITERATE (vector <unsigned>, it, seqs_Ndiff_idx) {
             desc = GetDiscItemText(*seqs[*it]) + ": " + hap_tps[hap_idx[*it]];
             thisInfo.test_item_list[GetName()].push_back(strtmp + "$" + desc); 
             thisInfo.test_item_objs[GetName() + "$" + strtmp].push_back(
                                  CConstRef <CObject>(seqs[*it].GetPointer()));
          }
          strtmp = "seq_same_strict_" + NStr::UIntToString(m_entry_cnt) + "$" 
                                                  + NStr::UIntToString(i) + "#";
          ITERATE (vector <unsigned>, it, seqs_strict_idx) {
             desc = GetDiscItemText(*seqs[*it]) + ": " + hap_tps[hap_idx[*it]];
             thisInfo.test_item_list[GetName()].push_back(strtmp + "$" + desc);
             thisInfo.test_item_objs[GetName() + "$" + strtmp].push_back(
                                  CConstRef <CObject>(seqs[*it].GetPointer()));
          }
      }
      seqs_Ndiff_idx.clear();
      seqs_strict_idx.clear();
   }
};



// new comb 

void CSeqEntry_on_biosrc_subsrc :: GetSubmitText(const CAuth_list& authors)
{
   if (authors.CanGetAffil()) {
     const CAffil& affil = authors.GetAffil();
     if (affil.IsStd()) {
        if (affil.GetStd().CanGetAffil()) {
             strtmp = affil.GetStd().GetAffil();
             if (!strtmp.empty()) m_submit_text.push_back(strtmp);
        }
        if (affil.GetStd().CanGetDiv()) {
             strtmp = affil.GetStd().GetDiv();
             if (!strtmp.empty()) m_submit_text.push_back(strtmp);
        }
     }
   }
};


void CSeqEntry_on_biosrc_subsrc :: FindSpecSubmitText()
{
  string inst(kEmptyStr), dept(kEmptyStr);
  if (thisInfo.seq_submit.NotEmpty()) {
    GetSubmitText(thisInfo.seq_submit->GetSub().GetCit().GetAuthors());
  }

  ITERATE (vector <const CSeq_feat*>, it, pub_feat) {
     ITERATE (list < CRef < CPub > >, jt, (*it)->GetData().GetPub().GetPub().Get())
       if ( (*jt)->IsSetAuthors()) GetSubmitText((*jt)->GetAuthors());
  }

  ITERATE (vector <const CSeqdesc*>, it, pub_seqdesc) {
     ITERATE (list < CRef < CPub > >, jt, (*it)->GetPub().GetPub().Get())
       if ( (*jt)->IsSetAuthors()) GetSubmitText((*jt)->GetAuthors());
  }

};

void CSeqEntry_on_biosrc_subsrc :: TestOnObj(const CSeq_entry& seq_entry)
{
   if (thisTest.is_Subsrc_run) return;
   thisTest.is_Subsrc_run = true;

   m_run_iden = (thisTest.tests_run.find(GetName_iden()) != end_it);
   m_run_col = (thisTest.tests_run.find(GetName_col()) != end_it);
   m_run_orgi = (thisTest.tests_run.find(GetName_orgi()) != end_it);
   m_run_orgc = (thisTest.tests_run.find(GetName_orgc()) != end_it);
   m_run_end = (thisTest.tests_run.find(GetName_end()) != end_it);
   m_run_uncul = (thisTest.tests_run.find(GetName_uncul()) != end_it);

   if (m_run_iden) {
      m_submit_text.clear();
      FindSpecSubmitText();
   };

   string desc;
   ITERATE (vector <const CSeq_feat*>, it, biosrc_subsrc_feat) {
      RunTests( (*it)->GetData().GetBiosrc(), 
                 GetDiscItemText(**it),
                 CConstRef <CObject>(*it));
   }

   unsigned i=0;
   ITERATE (vector <const CSeqdesc*>, it, biosrc_subsrc_seqdesc) {
     desc = GetDiscItemText(**it, *(biosrc_subsrc_seqdesc_seqentry[i]));
     RunTests( (*it)->GetSource(), desc, CConstRef <CObject>(*it));
     i++;
   }
};

bool CSeqEntry_on_biosrc_subsrc :: Has3Names(const vector <string> arr)
{
   vector <string> names;
   unsigned cnt=0;
   ITERATE (vector <string>, it, arr) {
      names= NStr::Tokenize(*it, ",; ", names, NStr::eMergeDelims);
      ITERATE (vector <string>, jt, names) {
         if ( (*it) == "and") cnt++;
      }
      if ((names.size() - cnt) > 2) {
              return true;
      }
      names.clear();
   }
   return false;
};

bool CSeqEntry_on_biosrc_subsrc :: HasSpecNames(const vector <string> arr)
{
   ITERATE (vector <string>, it, arr) {
      ITERATE (vector <string>, jt, thisInfo.spec_words_biosrc) {
         if (NStr::FindNoCase(*it, *jt) != string::npos) {
             return true;
         }
      }

      ITERATE (vector <string>, jt, m_submit_text) {
          if ( (*it) == *jt) {
             return true;
          }
      }
   };
   return false;
};


static const char*  RemovableCultureNotes[] = {
 "[uncultured (using universal primers)]",
 "[uncultured (using universal primers) bacterial source]",
 "[cultured bacterial source]",
 "[enrichment culture bacterial source]",
 "[mixed bacterial source (cultured and uncultured)]",
 "[uncultured]; [universal primers]",
 "[mixed bacterial source]",
 "[virus wizard]",
 "[cDNA derived from mRNA, purified viral particles]",
 "[cDNA derived from mRNA, whole cell/tissue lysate]",
 "[cDNA derived from genomic RNA, whole cell/tissue lysate]",
 "[cDNA derived from genomic RNA, purified viral particles]",
 "[universal primers]",
 "[uncultured; wizard]",
 "[uncultured; wizard; spans unknown]",
 "[cultured; wizard]",
 "[cultured; wizard; spans unknown]",
 "[intergenic wizard]",
 "[intergenic wizard; spans unknown]",
 "[Microsatellite wizard]",
 "[Microsatellite wizard; multiple repeats]",
 "[D-loop wizard]",
 "[D-loop wizard; spans unknown]",
 "[D-loop wizard; spans known]"
};

static const char*  ReplaceableCultureNotes[] = {
 "[uncultured (with species-specific primers)]",
 "[uncultured]; [amplified with species-specific primers]",
 "[uncultured (using species-specific primers) bacterial source]",
 "[amplified with species-specific primers]"
};

bool CSeqEntry_on_biosrc_subsrc :: x_HasUnculturedNotes(const string& str, const char** notes,
unsigned sz)
{
   for (unsigned i=0; i< sz; i++) {
      if (str.find(notes[i]) != string::npos) return true;
   }
   return false;
};

void CSeqEntry_on_biosrc_subsrc :: RunTests(const CBioSource& biosrc, const string& desc, CConstRef <CObject> obj_ref)
{
   vector <string> arr;
   // UNCULTURED_NOTES_ONCALLER
   if (m_run_uncul) {
      GetSubSrcValues(biosrc, CSubSource::eSubtype_other, arr);
      ITERATE (vector <string>, it, arr) {
        if ( x_HasUnculturedNotes(*it, RemovableCultureNotes, 
                                   ArraySize(RemovableCultureNotes))
              || x_HasUnculturedNotes(*it, ReplaceableCultureNotes, 
                                      ArraySize(ReplaceableCultureNotes)) )
           thisInfo.test_item_list[GetName_uncul()].push_back(desc);      
           thisInfo.test_item_objs[GetName_uncul()].push_back(obj_ref);      
      }
      arr.clear();
   }

   // ONCALLER_MORE_NAMES_COLLECTED_BY
   if (m_run_col) {
      GetSubSrcValues(biosrc, CSubSource::eSubtype_collected_by, arr);
      if (Has3Names(arr)) {
          thisInfo.test_item_list[GetName_col()].push_back(desc);
          thisInfo.test_item_objs[GetName_col()].push_back(obj_ref);
      }
      
      arr.clear();
   }

   // ONCALLER_MORE_OR_SPEC_NAMES_IDENTIFIED_BY
   if (m_run_iden) {
      GetSubSrcValues(biosrc, CSubSource::eSubtype_identified_by, arr);
      if (Has3Names(arr) || HasSpecNames(arr)) {
           thisInfo.test_item_list[GetName_iden()].push_back(desc);
           thisInfo.test_item_objs[GetName_iden()].push_back(obj_ref);
      }
      arr.clear();
   }

   string tax_nm;
   if (biosrc.IsSetTaxname() && !(tax_nm = biosrc.GetTaxname()).empty()) {

      // ONCALLER_SUSPECTED_ORG_IDENTIFIED
      if (m_run_orgi
            && IsSubSrcPresent(biosrc, CSubSource::eSubtype_identified_by)
            && (tax_nm == "Homo sapiens" 
                     || tax_nm.find("uncultured") != string::npos)) {
         thisInfo.test_item_list[GetName_orgi()].push_back(desc);
         thisInfo.test_item_objs[GetName_orgi()].push_back(obj_ref);
      }

      // ONCALLER_SUSPECTED_ORG_COLLECTED
      if (m_run_orgc 
              && IsSubSrcPresent(biosrc, CSubSource::eSubtype_collected_by)
              && tax_nm == "Homo sapiens") {
          thisInfo.test_item_list[GetName_orgc()].push_back(desc);
          thisInfo.test_item_objs[GetName_orgc()].push_back(obj_ref);
      }
   }
   
   // END_COLON_IN_COUNTRY
   if (m_run_end) {
      GetSubSrcValues(biosrc, CSubSource::eSubtype_country, arr);
      size_t pos;
      ITERATE (vector <string>, it, arr) {
        if ( ( (pos = (*it).find_last_of(":")) != string::npos ) 
                 && pos == (*it).size()-1) {
           thisInfo.test_item_list[GetName_end()].push_back(desc);
           thisInfo.test_item_objs[GetName_end()].push_back(obj_ref);
        }
      }
      arr.clear();
   }
};

void CSeqEntry_UNCULTURED_NOTES_ONCALLER:: GetReport(CRef <CClickableItem>& c_item)
{
   c_item->obj_list = thisInfo.test_item_objs[GetName()];
   c_item->description = GetHasComment(c_item->item_list.size(), "biosource") 
                            + "uncultured notes.";
};

void CSeqEntry_END_COLON_IN_COUNTRY :: GetReport(CRef <CClickableItem>& c_item)
{
   c_item->obj_list = thisInfo.test_item_objs[GetName()];
   c_item->description = GetOtherComment(c_item->item_list.size(), 
                                   "country source ends", "country sources end")
                          + " with a colon.";
};


void CSeqEntry_ONCALLER_SUSPECTED_ORG_COLLECTED :: GetReport(CRef <CClickableItem>& c_item)
{
   c_item->obj_list = thisInfo.test_item_objs[GetName()];
   c_item->description = GetHasComment(c_item->item_list.size(), "biosource")
                         + "collected-by and suspect organism";
};

void CSeqEntry_ONCALLER_SUSPECTED_ORG_IDENTIFIED :: GetReport(CRef <CClickableItem>& c_item)
{
   c_item->obj_list = thisInfo.test_item_objs[GetName()];
   c_item->description = GetHasComment(c_item->item_list.size(), "biosource")
                         + "identified-by and suspect organism";
};

void CSeqEntry_ONCALLER_MORE_NAMES_COLLECTED_BY :: GetReport(CRef <CClickableItem>& c_item)
{
  c_item->obj_list = thisInfo.test_item_objs[GetName()];
  c_item->description = GetHasComment(c_item->item_list.size(), "biosource") 
                       + "3 or more names in collected-by.";
};

void CSeqEntry_ONCALLER_MORE_OR_SPEC_NAMES_IDENTIFIED_BY :: GetReport(CRef <CClickableItem>& c_item)
{
   c_item->obj_list = thisInfo.test_item_objs[GetName()];
   c_item->description = GetHasComment(c_item->item_list.size(), "biosource")
                          + "3 or more names or suspect text in identified-by.";
};


// new comb


void CSeqEntry_DISC_HAPLOTYPE_MISMATCH :: TestOnObj(const CSeq_entry& seq_entry)
{
   m_tax_hap2seqs.clear();
   
   // why m_entry_cnt???
   if (thisInfo.test_item_list.find(GetName()) ==thisInfo.test_item_list.end()){
        m_entry_cnt = 0;
   }
   else m_entry_cnt ++;
   string tax_nm, hap_tp;
   unsigned i=0;
   ITERATE (vector <const CSeqdesc*>, it, biosrc_subsrc_seqdesc) {
     const CBioSource& biosrc = (*it)->GetSource();
     if (biosrc.IsSetTaxname() && !(tax_nm = biosrc.GetTaxname()).empty()) {
        ITERATE (list <CRef <CSubSource> >, it, biosrc.GetSubtype()) {
           if ((*it)->GetSubtype() == CSubSource :: eSubtype_haplotype 
                     && !(hap_tp = (*it)->GetName()).empty()) {
               if (biosrc_subsrc_seqdesc_seqentry[i]->IsSet()) {
                  ExtractNonAaBioseqsOfSet(tax_nm +"#" + hap_tp, 
                                  biosrc_subsrc_seqdesc_seqentry[i]->GetSet());
               }
               break;
           }
        }
     }
     i++;
   }

   if (!m_tax_hap2seqs.empty()) {
        ReportHaplotypeSequenceMismatchForList();
   }
};


void CSeqEntry_DISC_HAPLOTYPE_MISMATCH :: MakeCitem4DiffSeqs(CRef <CClickableItem>& c_item, const vector <string> tax_hap_seqs, const string& match_tp, bool Ndiff)
{
   Str2Strs tax_hap2seqs;
   GetTestItemList(tax_hap_seqs, tax_hap2seqs, "@");
   size_t pos;
   string tax_nm, hap_tp;
   ITERATE (Str2Strs, it, tax_hap2seqs){
      pos = it->first.find("#");
      tax_nm = it->first.substr(0, pos);
      hap_tp = CTempString(it->first).substr(pos+1);
      strtmp = "organism " + tax_nm + " haplotype " + hap_tp 
               + " but the sequences do not match " 
               + (Ndiff ? "(allowing N to match any)." : "(strict match).");
      AddSubcategory(c_item, GetName()+ "$" + match_tp + "@" + it->first, 
                     &(it->second), "sequence", strtmp, e_HasComment);
   }

};


void CSeqEntry_DISC_HAPLOTYPE_MISMATCH :: MakeCitem4SameSeqs(CRef <CClickableItem>& c_item, const vector <string>& idx_seqs, const string& match_tp, bool Ndiff)
{
   Str2Strs idx2seqs;
   GetTestItemList(idx_seqs, idx2seqs, "#");
   strtmp = (string)"identical " 
             + (Ndiff ? "(allowing N to match any)" : "(strict match)")
             + " but have different haplotypes.";
   ITERATE (Str2Strs, it, idx2seqs) {
      AddSubcategory(c_item, GetName() + "$" + match_tp + "@" + it->first, 
                     &(it->second), "sequences", strtmp);
   }
};


void CSeqEntry_DISC_HAPLOTYPE_MISMATCH :: GetReport(CRef <CClickableItem>& c_item)
{
   Str2Strs match_tp2ls;
   GetTestItemList(c_item->item_list, match_tp2ls);
   c_item->item_list.clear();
   CRef <CClickableItem> csub_N (new CClickableItem);
   CRef <CClickableItem> csub_strict(new CClickableItem);
   
   string tax_nm, hap_tp;
   ITERATE (Str2Strs, it, match_tp2ls) {
     if (it->first.find("seq_diff_N") != string::npos) {
         MakeCitem4DiffSeqs(csub_N, it->second, it->first);
     }
     else if (it->first.find("seq_same_N") != string::npos) {
         MakeCitem4SameSeqs(csub_N, it->second, it->first);
     }
     else if (it->first.find("seq_diff_strict") != string::npos) {
         MakeCitem4DiffSeqs(csub_strict, it->second, it->first, false);
     }
     else { // "seq_same_strict"
        MakeCitem4SameSeqs(csub_strict, it->second, it->first, false);  
     }
   }

   if (csub_N.NotEmpty()) {
     csub_N->description 
       = "There are " + NStr::UIntToString((unsigned)csub_N->item_list.size())
         + "haplotype problems (loose match, allowing Ns to differ)";
     csub_N->setting_name = GetName();
     c_item->subcategories.push_back(csub_N);
     copy( csub_N->item_list.begin(), csub_N->item_list.end(),
           back_inserter(c_item->item_list));
     copy( csub_N->obj_list.begin(), csub_N->obj_list.end(),
           back_inserter(c_item->obj_list));
   }
   if (csub_strict.NotEmpty()) {
      csub_strict->description
        = "There are " 
            + NStr::UIntToString((unsigned)csub_strict->item_list.size())
            + "haplotype problems (strict match)";
      csub_strict->setting_name = GetName();
      c_item->subcategories.push_back(csub_strict);
      copy(csub_strict->item_list.begin(), csub_strict->item_list.end(),
           back_inserter( c_item->item_list));
      copy(csub_strict->obj_list.begin(), csub_strict->obj_list.end(),
           back_inserter( c_item->obj_list));
   }
   c_item->description = "Haplotype Problem Report";
};


CConstRef <CCit_sub> CSeqEntry_test_on_pub :: CitSubFromPubEquiv(const list <CRef <CPub> >& pubs)
{
   ITERATE (list <CRef <CPub> >, it, pubs) {
      if ((*it)->IsSub())
        return (CConstRef <CCit_sub> ( &( (*it)->GetSub() )));
      else if ( (*it)->IsEquiv())
        return (CitSubFromPubEquiv( (*it)->GetEquiv().Get()));
   }
   return (CConstRef <CCit_sub>());
};


static string uni_str("University of");
bool CSeqEntry_test_on_pub :: AffilStreetEndsWith(const string& street, const string& end_str)
{
  unsigned street_sz = street.size();
  unsigned end_sz = end_str.size();
  unsigned diff_sz = street_sz - end_sz;
  unsigned uni_sz = uni_str.size();
  if (!end_sz || street_sz < end_sz) return false;
  else if (NStr::EqualNocase(street, diff_sz, street_sz-1, end_str)
            && ( (street_sz == end_sz 
                     || isspace(street[diff_sz-1]) 
                     || ispunct(street[diff_sz-1])))) {
    if (diff_sz >= uni_sz 
         && NStr::EqualNocase(street, diff_sz - uni_sz -1, diff_sz-1, uni_str)){
          return false;
     }
     else return true;
  }
  else return false;
};


bool CSeqEntry_test_on_pub :: AffilStreetContainsDuplicateText(const CAffil& affil)
{
  if (affil.IsStd() && affil.GetStd().CanGetStreet() 
                              && !(affil.GetStd().GetStreet().empty())) {
    const CAffil :: C_Std& std = affil.GetStd();
    const string& street = std.GetStreet();
    if (std.CanGetCountry() && AffilStreetEndsWith(street, std.GetCountry()))
         return true;
    else if (std.CanGetPostal_code()
                            && AffilStreetEndsWith (street, std.GetPostal_code()))
            return true;
    else if (std.CanGetSub() && AffilStreetEndsWith (street, std.GetSub()))
            return true;
    else if (std.CanGetCity() && AffilStreetEndsWith (street, std.GetCity()))
           return true;
    else return false;
  }
  else return false;
};



// new comb
bool CSeqEntry_test_on_pub :: CorrectUSAStates(CConstRef <CCit_sub>& cit_sub)
{
   string country, state;
   if (cit_sub->GetAuthors().CanGetAffil()) {
       const CAffil& affil = cit_sub->GetAuthors().GetAffil();
       if (affil.IsStd()) {
           country =(affil.GetStd().CanGetCountry()) 
                       ? affil.GetStd().GetCountry() : kEmptyStr;
           state = (affil.GetStd().CanGetSub()) 
                      ? affil.GetStd().GetSub() : kEmptyStr;
           if (country == "USA") {
               if (state != "Washington DC") {
                   if (state.empty() 
                           || state.size() > 2 
                           || !isupper(state[0]) 
                           || !isupper(state[1])) {
                          return false;
                   }
                   else {
                        ITERATE (Str2Str, it, thisInfo.state_abbrev) {
                           if (state == it->first) {
                                 return true;
                           }
                        }
                        return false;
                   }
               }
           } 
       }
   }
   return true;
};


string CSeqEntry_test_on_pub :: GetAuthNameList(const CAuthor& auth, bool use_initials)
{
  string str;
  const CPerson_id& pid = auth.GetName();
  switch (pid.Which()) {
    case CPerson_id::e_Dbtag:
          pid.GetDbtag().GetLabel(&str);
          break;
    case CPerson_id::e_Name:
         { const CName_std& nm_std = pid.GetName();
          if (use_initials)
              str =(nm_std.CanGetInitials()? nm_std.GetInitials() + " " :"") +nm_std.GetLast();
          else {
            str = (nm_std.CanGetFirst()? nm_std.GetFirst() + " " : "")
                    + ( (nm_std.CanGetInitials() && nm_std.GetInitials().size()>2)? 
                          CTempString(nm_std.GetInitials()).substr(2) + " " : "")
                    + nm_std.GetLast();
          }
          }
          break;
    case CPerson_id::e_Ml:
          str = pid.GetMl(); break;
    case CPerson_id::e_Str:
          str = pid.GetStr(); break;
    case CPerson_id::e_Consortium: 
          str = pid.GetConsortium(); break;
    default:   break;
  }
  return str;
};


void CSeqEntry_test_on_pub :: CheckTitleAndAuths(CConstRef <CCit_sub>& cit_sub, const string& desc, CConstRef <CObject> obj_ref)
{
   string title = cit_sub->CanGetDescr()? cit_sub->GetDescr() : kEmptyStr;
   string authors(kEmptyStr);
   switch (cit_sub->GetAuthors().GetNames().Which()) {
        case CAuth_list::C_Names::e_Std:
                ITERATE (list <CRef <CAuthor> >, it, 
                            cit_sub->GetAuthors().GetNames().GetStd()) {
                      authors += ", " + GetAuthNameList(**it);
                }
                break;
        case CAuth_list::C_Names::e_Ml:
                ITERATE (list <string>, it, 
                               cit_sub->GetAuthors().GetNames().GetMl()) {
                      authors += ", " + *it; 
                }
                break;
        case CAuth_list::C_Names::e_Str:
                ITERATE (list <string>, it, 
                             cit_sub->GetAuthors().GetNames().GetStr()) {
                      authors += ", " + *it; 
                }
                break;
        default: break;
   };

   if (!authors.empty())  {
          authors = CTempString(authors).substr(2);
   }
   thisInfo.test_item_list[GetName_tlt()].push_back(
                               title + "$" + authors + "#" + desc);
   thisInfo.test_item_objs[GetName_tlt() + "$" + title + "#" + authors]
             .push_back(obj_ref);
};


void CSeqEntry_test_on_pub :: RunTests(const list <CRef <CPub> >& pubs, const string& desc, CConstRef <CObject> obj_ref)
{
   CConstRef <CCit_sub> cit_sub = CitSubFromPubEquiv(pubs);
   if (cit_sub.NotEmpty()) {

      // DISC_CITSUB_AFFIL_DUP_TEXT
      if (m_run_dup) {
          if (cit_sub->GetAuthors().CanGetAffil()) {
             const CAffil& affil = cit_sub->GetAuthors().GetAffil();
             if (affil.IsStd() && affil.GetStd().CanGetStreet()
                   && !(affil.GetStd().GetStreet().empty())
                   && AffilStreetContainsDuplicateText(cit_sub->GetAuthors().GetAffil())) {
                thisInfo.test_item_list[GetName_dup()].push_back(desc);
                thisInfo.test_item_objs[GetName_dup()].push_back(obj_ref);
             }
          }
      }

      // DISC_USA_STATE
      if (m_run_usa && !CorrectUSAStates(cit_sub)) {
         thisInfo.test_item_list[GetName_usa()].push_back(desc);
         thisInfo.test_item_objs[GetName_usa()].push_back(obj_ref);
      }
  
      // DISC_TITLE_AUTHOR_CONFLICT;
      if (m_run_tlt) {
            CheckTitleAndAuths(cit_sub, desc, obj_ref);
      }


      // DISC_CITSUBAFFIL_CONFLICT: need attention
      if (m_run_aff) {
          string affil_str, grp_str;   // either affil_str or grp_str is unnecessary
          GetGroupedAffilString(cit_sub->GetAuthors(), affil_str, grp_str);
          if (!grp_str.empty()) {
             thisInfo.test_item_list[GetName_aff()].push_back(
                                                     grp_str + "$" + desc);
             thisInfo.test_item_objs[GetName_aff() + "$" + grp_str]
                       .push_back(obj_ref);
             m_has_cit = true;
          }
      }

      // DISC_MISSING_AFFIL
      if (m_run_noaff && cit_sub->GetAuthors().CanGetAffil()) {
         if (IsMissingAffil(cit_sub->GetAuthors().GetAffil())) {
            thisInfo.test_item_list[GetName_noaff()].push_back(desc);
            thisInfo.test_item_objs[GetName_noaff()].push_back(obj_ref);
         }
      } 
   }

   // DISC_CHECK_AUTH_CAPS, DISC_CHECK_AUTH_NAME
   if (m_run_cap || m_run_missing) {
       CheckBadAuthCapsOrNoFirstLastNamesInPubdesc(pubs, desc, obj_ref);
   }

   // DISC_UNPUB_PUB_WITHOUT_TITLE
   if (m_run_unp && DoesPubdescContainUnpubPubWithoutTitle(pubs)) {
      thisInfo.test_item_list[GetName_unp()].push_back(desc);
      thisInfo.test_item_objs[GetName_unp()].push_back(obj_ref);
   }

   // ONCALLER_CONSORTIUM
   if (m_run_cons && PubHasConsortium(pubs)) {
      thisInfo.test_item_list[GetName_cons()].push_back(desc);
      thisInfo.test_item_objs[GetName_cons()].push_back(obj_ref);
   }
};


bool CSeqEntry_test_on_pub :: AuthorHasConsortium(const CAuthor& author)
{
   if (author.GetName().IsConsortium() && !author.GetName().GetConsortium().empty()) 
     return true;
   else return false;
};


bool CSeqEntry_test_on_pub :: AuthListHasConsortium(const CAuth_list& auth_ls)
{
   if ( auth_ls.GetNames().IsStd()) {
      ITERATE (list <CRef <CAuthor> >, it, auth_ls.GetNames().GetStd()) {
         if (AuthorHasConsortium (**it)) return true;
      }
   }
   return false;
};


bool CSeqEntry_test_on_pub :: PubHasConsortium(const list <CRef <CPub> >& pubs)
{
  ITERATE (list <CRef <CPub> >, it, pubs) {
    if ( (*it)->IsSetAuthors() ) 
       if (AuthListHasConsortium( (*it)->GetAuthors())) return true;
  }
  return false;
};
 

void CSeqEntry_ONCALLER_CONSORTIUM :: GetReport(CRef <CClickableItem>& c_item)
{
  c_item->obj_list = thisInfo.test_item_objs[GetName()];
  c_item->description = GetOtherComment(c_item->item_list.size(), 
                                        "publication/submitter block has", 
                                        "publications/submitter blocks have") 
       + " consortium";
};


bool CSeqEntry_test_on_pub :: IsMissingAffil(const CAffil& affil)
{
   if ( (affil.IsStd() && affil.GetStd().CanGetAffil() 
                       && affil.GetStd().GetAffil().empty())
           || (affil.IsStr() && affil.GetStr().empty()) ) {
       return true;
   }
   return false;
};


CSeqEntry_test_on_pub::E_Status CSeqEntry_test_on_pub :: GetPubMLStatus (const CPub& pub)
{
  switch (pub.Which()) {
    case CPub::e_Gen:
        if (pub.GetGen().CanGetCit() 
              && NStr::FindNoCase(pub.GetGen().GetCit(), "unpublished") != string::npos)
           return e_unpublished;
        else return e_published;
    case CPub::e_Sub: return e_submitter_block;
    case CPub::e_Article:
       switch (pub.GetArticle().GetFrom().Which() ) {
         case CCit_art::C_From::e_Journal:
              return ImpStatus(pub.GetArticle().GetFrom().GetJournal().GetImp());
         case CCit_art::C_From::e_Book:
              return ImpStatus(pub.GetArticle().GetFrom().GetBook().GetImp());
         case CCit_art::C_From::e_Proc:
              return ImpStatus(pub.GetArticle().GetFrom().GetProc().GetBook().GetImp());
         default: return e_any;
       }
    case CPub::e_Journal: return ImpStatus(pub.GetJournal().GetImp()); 
    case CPub::e_Book: return ImpStatus(pub.GetBook().GetImp());
    case CPub::e_Man: return ImpStatus(pub.GetMan().GetCit().GetImp());
    case CPub::e_Patent: return e_published;
    default: return e_any;
  }
};


CSeqEntry_test_on_pub::E_Status CSeqEntry_test_on_pub :: ImpStatus(const CImprint& imp, bool is_pub_sub)
{ 
  if (imp.CanGetPrepub()) {
     switch (imp.GetPrepub()) {
       case CImprint::ePrepub_submitted:
          if (is_pub_sub) return e_submitter_block;
          return e_unpublished;
       case CImprint::ePrepub_in_press: return e_in_press;
       case CImprint::ePrepub_other: return e_unpublished;
     }
  }
  return e_published;
};


string CSeqEntry_test_on_pub :: Get1stTitle(const CTitle::C_E& title)
{
   switch (title.Which()) {
     case CTitle::C_E::e_Name:    return title.GetName();
     case CTitle::C_E::e_Tsub:    return title.GetTsub();
     case CTitle::C_E::e_Trans:   return title.GetTrans();
     case CTitle::C_E::e_Jta:     return title.GetJta();
     case CTitle::C_E::e_Iso_jta: return title.GetIso_jta();
     case CTitle::C_E::e_Ml_jta:  return title.GetMl_jta();
     case CTitle::C_E::e_Coden:   return title.GetCoden();
     case CTitle::C_E::e_Issn:    return title.GetIssn();
     case CTitle::C_E::e_Abr:     return title.GetAbr();
     case CTitle::C_E::e_Isbn:    return title.GetIsbn();
     default: return kEmptyStr;
   }
};

string CSeqEntry_test_on_pub :: GetTitleFromPub(const CPub& pub)
{
  switch (pub.Which()) {
    case CPub::e_Gen:
      return (pub.GetGen().CanGetTitle() ? pub.GetGen().GetTitle() : kEmptyStr);
    case CPub::e_Sub:
      return (pub.GetSub().CanGetDescr() ? pub.GetSub().GetDescr() : kEmptyStr);
    case CPub::e_Article:
      if (pub.GetArticle().CanGetTitle() 
               && !pub.GetArticle().GetTitle().Get().empty()) {
         Get1stTitle(**(pub.GetArticle().GetTitle().Get().begin()));  
      }  
      else {
         return kEmptyStr;
      }
    case CPub::e_Book:
      if (!pub.GetBook().GetTitle().Get().empty()) {
        return Get1stTitle(**(pub.GetBook().GetTitle().Get().begin()));
      }
      else {
        return kEmptyStr;
      }
    case CPub::e_Man:
      if (!pub.GetMan().GetCit().GetTitle().Get().empty()) {
         return Get1stTitle(**(pub.GetMan().GetCit().GetTitle().Get().begin()));
      }
      else {
         return kEmptyStr;
      }
    case CPub::e_Patent:
      return (pub.GetPatent().GetTitle());
    default: return kEmptyStr;
  }
};


bool CSeqEntry_test_on_pub :: DoesPubdescContainUnpubPubWithoutTitle(const list <CRef <CPub> >& pubs)
{
  string title;
  ITERATE (list <CRef <CPub> >, it, pubs) {
     if (GetPubMLStatus( **it ) == e_unpublished) {
       title = GetTitleFromPub(**it);
       if (title.empty() || NStr::FindNoCase(title, "Direct Submission")!= string::npos)
         return true;
     }
  }
  return false;
};


void CSeqEntry_DISC_UNPUB_PUB_WITHOUT_TITLE :: GetReport(CRef <CClickableItem>& c_item)
{
   c_item->obj_list = thisInfo.test_item_objs[GetName()];
   c_item->description 
       = GetHasComment(c_item->item_list.size(), "unpublished pub") + "no title";
};


void CSeqEntry_DISC_CITSUBAFFIL_CONFLICT :: GetReport(CRef <CClickableItem>& c_item)
{
   if (c_item->item_list[0] == "no cit") {
     c_item->item_list.clear();
     c_item->description = "No citsubs were found!";
     return;
   }
    
   Str2Strs affil2pubs;
   GetTestItemList(c_item->item_list, affil2pubs);
   c_item->item_list.clear();
   if (affil2pubs.size() == 1) {
      if ( affil2pubs.begin()->first != "no affil" ) { 
         // Make no report if all values match
         c_item->description = kEmptyStr;
      }
      else {
         c_item->description = "All citsub have no affiliations";
      }
   }
   else {
      vector <string> subcat_pub, no_affil_pubs, arr;
      ITERATE (Str2Strs, it, affil2pubs) {
         if (it->first == "no affil") {
             no_affil_pubs = it->second;
         }
         else {
            arr.clear();
            arr = NStr::Tokenize(it->first, ",", arr);  
            ITERATE (vector <string>, jt, arr) {
                 subcat_pub.push_back((*jt) + "$" + it->second[0]);
            }
         }
      }
      if (!no_affil_pubs.empty()) { 
          AddSubcategory(c_item, GetName() + "$" + "no affil", 
                         &no_affil_pubs, "Cit-sub", "no affiliation", 
                         e_HasComment, false);
      }
      Str2Strs subcat2pubs, subvlu2pubs;
      GetTestItemList(subcat_pub, subcat2pubs, "@");
      subcat_pub.clear();
      ITERATE (Str2Strs, it, subcat2pubs) {
         subvlu2pubs.clear();
         GetTestItemList(it->second, subvlu2pubs);
         if (subvlu2pubs.size() > 1) {
            CRef <CClickableItem> c_sub (new CClickableItem);
            c_sub->setting_name = GetName();
            ITERATE (Str2Strs, jt, subvlu2pubs)
               AddSubcategory(c_sub, GetName() + "$" + it->first, 
                              &(jt->second), "affiliation", 
                                      it->first + " value '" + jt->first + "'", e_HasComment);
            c_sub->description = "Affiliations have different values for " + it->first;
            c_item->subcategories.push_back(c_sub);
         } 
      }
      c_item->description = "Citsub affiliation conflicts found";
   }
};


void CSeqEntry_test_on_pub :: GetGroupedAffilString(const CAuth_list& authors, string& affil_str, string& grp_str)
{
   affil_str = grp_str = kEmptyStr;
   if (authors.CanGetAffil()) {
      const CAffil& affil = authors.GetAffil();
      switch (affil.Which()) {
      case CAffil :: e_not_set: return;
      case CAffil :: e_Str:
          affil_str = NStr::Replace(affil.GetStr(), "\"", "'");
          grp_str  = "no group";
          return;
      case CAffil :: e_Std:
        {
 #define ADD_AFFIL_FIELD(X, Y) \
         if (affil.GetStd().CanGet##X()  &&  !(affil.GetStd().Get##X().empty())) { \
             string value = NStr::Replace(affil.GetStd().Get##X(), "\"", "'"); \
             affil_str += "," + value; \
             grp_str += "," #Y "@" + value; \
         }
         ADD_AFFIL_FIELD(Div, department)
         ADD_AFFIL_FIELD(Affil, institution)
         ADD_AFFIL_FIELD(Street, street)
         ADD_AFFIL_FIELD(City, city)
         ADD_AFFIL_FIELD(Sub, state/province)
         ADD_AFFIL_FIELD(Postal_code, postal_code)
         ADD_AFFIL_FIELD(Country, country)
         affil_str = affil_str.empty() ? "no affil" : CTempString(affil_str).substr(1);
         grp_str = grp_str.empty() ? "no affil" : CTempString(grp_str).substr(1);
 #undef ADD_AFFIL_FIELD
         return;
       }
     }    
   }
};


void CSeqEntry_test_on_pub :: TestOnObj(const CSeq_entry& seq_entry)
{
   if (thisTest.is_Pub_run) return;
   thisTest.is_Pub_run = true;

   m_has_cit = false;

   m_run_dup = (thisTest.tests_run.find(GetName_dup()) != end_it);
   m_run_cap = (thisTest.tests_run.find(GetName_cap()) != end_it);
   m_run_usa = (thisTest.tests_run.find(GetName_usa()) != end_it);
   m_run_tlt = (thisTest.tests_run.find(GetName_tlt()) != end_it);
   m_run_aff = (thisTest.tests_run.find(GetName_aff()) != end_it);
   m_run_unp = (thisTest.tests_run.find(GetName_unp()) != end_it);
   m_run_noaff = (thisTest.tests_run.find(GetName_noaff()) != end_it);
   m_run_cons = (thisTest.tests_run.find(GetName_cons()) != end_it);
   m_run_missing = (thisTest.tests_run.find(GetName_missing()) != end_it);

   string desc;
   ITERATE (vector <const CSeq_feat*>, it, pub_feat) {
      const list <CRef <CPub> >& pubs =(*it)->GetData().GetPub().GetPub().Get();
      RunTests(pubs, GetDiscItemText(**it), CConstRef <CObject>(*it));
   };

   unsigned i=0;
   ITERATE (vector <const CSeqdesc*>, it, pub_seqdesc) {
      desc = GetDiscItemText(**it, *(pub_seqdesc_seqentry[i]));
      const list <CRef <CPub> >& pubs = (*it)->GetPub().GetPub().Get();
      RunTests(pubs, desc, CConstRef <CObject>(*it));
      i++;
   };

   CSubmit_block this_submit_blk;
   CConstRef <CObject> obj_ref;
   if (thisInfo.seq_submit.NotEmpty()) {
      if (seq_entry.IsSeq()) {
          desc = GetDiscItemText(seq_entry.GetSeq());
          obj_ref.Reset(&(seq_entry.GetSeq()));
      }
      else {
          desc = GetDiscItemText(seq_entry.GetSet());
          obj_ref.Reset(&(seq_entry.GetSet()));
      }

      const CAuth_list& 
          auths = thisInfo.seq_submit->GetSub().GetCit().GetAuthors();
      // DISC_CITSUB_AFFIL_DUP_TEXT
      if (m_run_dup && auths.CanGetAffil() 
                 && AffilStreetContainsDuplicateText(auths.GetAffil())) {
          size_t pos = desc.find(": ");
          strtmp = desc.substr(0, pos + 2) + "Cit-sub for " 
                    + CTempString(desc).substr(pos+3);
          thisInfo.test_item_list[GetName_dup()].push_back(strtmp);
          thisInfo.test_item_objs[GetName_dup()].push_back(obj_ref);
      }

      // DISC_CHECK_AUTH_CAPS
      if ( m_run_cap && HasBadAuthorName(auths)) {
           thisInfo.test_item_list[GetName_cap()].push_back(desc);
           thisInfo.test_item_objs[GetName_cap()].push_back(obj_ref);
      }

      // DISC_CITSUBAFFIL_CONFLICT: need attention
      if (m_run_aff) {
         string affil_str, grp_str;
         GetGroupedAffilString(auths, affil_str, grp_str); // one of affil_str/grp_str unused
         if (!grp_str.empty()) {
            thisInfo.test_item_list[GetName_aff()].push_back(
                                               grp_str + "$" + desc);
            thisInfo.test_item_objs[GetName_aff() + "$" + grp_str]
                       .push_back(obj_ref);
            m_has_cit = true;
         }
      }

      // DISC_MISSING_AFFIL
      const CContact_info& contact = thisInfo.seq_submit->GetSub().GetContact();
      desc = GetDiscItemText(*thisInfo.seq_submit);
      if (m_run_noaff) {
         if (!contact.CanGetContact() 
                 || !contact.GetContact().CanGetAffil()
                 || IsMissingAffil(contact.GetContact().GetAffil())) {
             thisInfo.test_item_list[GetName_noaff()].push_back(desc);
             thisInfo.test_item_objs[GetName_noaff()].push_back(obj_ref);
         }
         else if (!auths.CanGetAffil() || IsMissingAffil(auths.GetAffil())) { 
              thisInfo.test_item_list[GetName_noaff()].push_back(desc);
              thisInfo.test_item_objs[GetName_noaff()].push_back(obj_ref);
         }
      }

     // ONCALLER_CONSORTIUM
     if ( m_run_cons 
            && ( (contact.CanGetContact() 
                        && AuthorHasConsortium(contact.GetContact()))
                   || AuthListHasConsortium(auths) )) {
        thisInfo.test_item_list[GetName_cons()].push_back(desc);
        thisInfo.test_item_objs[GetName_cons()].push_back(obj_ref);
     }
   }
 
   if (m_run_aff && !m_has_cit) {
       thisInfo.test_item_list[GetName_aff()].push_back("no cit");
   }
};


void CSeqEntry_DISC_MISSING_AFFIL :: GetReport(CRef <CClickableItem>& c_item)
{
   c_item->obj_list = thisInfo.test_item_objs[GetName()];
   c_item->description = GetIsComment(c_item->item_list.size(), "citsub") 
                           + "missing affiliation";
};


void CSeqEntry_DISC_TITLE_AUTHOR_CONFLICT :: GetReport(CRef <CClickableItem>& c_item)
{
   Str2Strs tlt2auths, auth2feats;
   GetTestItemList(c_item->item_list, tlt2auths);
   c_item->item_list.clear();
   ITERATE (Str2Strs, it, tlt2auths) {
      auth2feats.clear();
      GetTestItemList(it->second, auth2feats);
      if (auth2feats.size() > 1) {
         CRef <CClickableItem> c_tlt (new CClickableItem);
         c_tlt->setting_name = GetName();
         ITERATE (Str2Strs, jt, auth2feats) {
           AddSubcategory(c_tlt, GetName() + "$" + it->first + "#" + jt->first,
                &(jt->second), "article", 
                "title '" + it->first + "' and author list '" + jt->first + "'",
                e_HasComment, true);
         }
         c_tlt->description 
             = NStr::UIntToString((unsigned)c_tlt->item_list.size()) 
               + " articles have title '" + it->first 
               + " but do not have the same author list";
         copy(c_tlt->item_list.begin(), c_tlt->item_list.end(),
                 back_inserter(c_item->item_list));
         copy(c_tlt->obj_list.begin(), c_tlt->obj_list.end(),
                 back_inserter(c_item->obj_list));
         c_item->subcategories.push_back(c_tlt);
      } 
   };
   if (!c_item->item_list.empty()) {
      c_item->description = "Publication Title/Author Inconsistencies";
   }
};


void CSeqEntry_DISC_USA_STATE :: GetReport(CRef <CClickableItem>& c_item)
{
   sort(c_item->item_list.begin(), c_item->item_list.end());
   c_item->obj_list = thisInfo.test_item_objs[GetName()];
   c_item->description = GetIsComment(c_item->item_list.size(), "cit-sub") 
                          + "missing state abbreviations.";
};


void CSeqEntry_DISC_CITSUB_AFFIL_DUP_TEXT :: GetReport(CRef <CClickableItem>& c_item)
{
   c_item->obj_list = thisInfo.test_item_objs[GetName()];
   c_item->description = GetHasComment(c_item->item_list.size(), "Cit-sub pub") 
                          + "duplicate affil text.";
};
// new comb



void CSeqEntryTestAndRepData :: AddBioseqsOfSetToReport(const CBioseq_set& bioseq_set, const string& setting_name, bool be_na, bool be_aa)
{
   ITERATE (list < CRef < CSeq_entry > >, it, bioseq_set.GetSeq_set()) {
     if ((*it)->IsSeq()) {
         const CBioseq& bioseq = (*it)->GetSeq();
         if ( (be_na && bioseq.IsNa()) || (be_aa && bioseq.IsAa()) ) 
           thisInfo.test_item_list[setting_name].push_back(GetDiscItemText(bioseq));
     }
     else AddBioseqsOfSetToReport( (*it)->GetSet(), setting_name, be_na, be_aa);
   }
};



void CSeqEntryTestAndRepData :: AddBioseqsInSeqentryToReport(const CSeq_entry* seq_entry, const string& setting_name, bool be_na, bool be_aa)
{
  if (seq_entry->IsSeq()) 
    thisInfo.test_item_list[setting_name].push_back(
                            GetDiscItemText( seq_entry->GetSeq() ));
  else AddBioseqsOfSetToReport(seq_entry->GetSet(), setting_name, be_na, be_aa);
};

CRef <GeneralDiscSubDt> CSeqEntry_INCONSISTENT_BIOSOURCE :: AddSeqEntry(const CSeqdesc& seqdesc, const CSeq_entry& seq_entry, const CBioseq_CI& b_ci)
{
    CRef < GeneralDiscSubDt > new_biosrc_ls (new GeneralDiscSubDt (kEmptyStr, kEmptyStr));
    new_biosrc_ls->obj_text.clear();
    new_biosrc_ls->obj_ls.clear();

    string biosrc_txt =  GetDiscItemText(seqdesc, seq_entry);
    for (CBioseq_CI bb_ci = b_ci; bb_ci; ++bb_ci) {
       if (bb_ci->IsNa()) {
          new_biosrc_ls->obj_text.push_back(biosrc_txt);
          new_biosrc_ls->obj_text.push_back(GetDiscItemText(*(bb_ci->GetCompleteBioseq())));
          new_biosrc_ls->obj_ls.push_back(CConstRef <CObject>(&seqdesc));
          new_biosrc_ls->obj_ls.push_back( CConstRef <CObject>(bb_ci->GetCompleteBioseq()));
       }
    }
    new_biosrc_ls->biosrc = &(seqdesc.GetSource());
    return (new_biosrc_ls);
};

void CSeqEntry_INCONSISTENT_BIOSOURCE :: TestOnObj(const CSeq_entry& seq_entry)
{
   bool found, has_na;
   unsigned i=0;
   string biosrc_txt;
   ITERATE (vector <const CSeqdesc*>, it, biosrc_seqdesc) {
      found = has_na = false;
      const CSeq_entry* this_seq = biosrc_seqdesc_seqentry[i];
      CBioseq_CI b_ci(*thisInfo.scope, *this_seq);
      for (CBioseq_CI bb_ci = b_ci; bb_ci; ++bb_ci) {
         if (b_ci->IsNa()) {
            has_na = true; 
            break;
         };
      }
      if (!has_na) continue;
      NON_CONST_ITERATE(vector <CRef < GeneralDiscSubDt > >,jt,INCONSISTENT_BIOSOURCE_biosrc) {
         if ( BioSourceMatch((*it)->GetSource(), *((*jt)->biosrc)) ) {
            biosrc_txt = GetDiscItemText(**it, *this_seq);
            for (CBioseq_CI bb_ci = b_ci; bb_ci; ++bb_ci) {
               if (bb_ci->IsNa()) {
                  CConstRef <CBioseq> seq_ref = bb_ci->GetCompleteBioseq();
                  (*jt)->obj_text.push_back(biosrc_txt);
                  (*jt)->obj_text.push_back(GetDiscItemText(*seq_ref));
                  (*jt)->obj_ls.push_back(CConstRef <CObject> (*it));
                  (*jt)->obj_ls.push_back(CConstRef <CObject>(seq_ref));
               }
            }
            found = true;
            break;
         }
     }
     if (!found) {
          INCONSISTENT_BIOSOURCE_biosrc.push_back(
                                      AddSeqEntry(**it, *this_seq, b_ci));
          if ( (thisInfo.test_item_list.find(GetName()) 
                             == thisInfo.test_item_list.end())
                 && INCONSISTENT_BIOSOURCE_biosrc.size() > 1)
             thisInfo.test_item_list[GetName()].push_back("inconsistance");
     }
     i++;
   }
};


void CSeqEntry_INCONSISTENT_BIOSOURCE :: GetReport(CRef <CClickableItem>& c_item)
{
   string diff = (INCONSISTENT_BIOSOURCE_biosrc.size() > 1)?
              " (" + DescribeBioSourceDifferences(
                          *(INCONSISTENT_BIOSOURCE_biosrc[0]->biosrc),
                          *(INCONSISTENT_BIOSOURCE_biosrc[1]->biosrc)) + ")."
              : ".";
   c_item->item_list.clear();
   c_item->obj_list.clear();
   ITERATE (vector <CRef < GeneralDiscSubDt > >, it, INCONSISTENT_BIOSOURCE_biosrc) {
     CRef <CClickableItem> sub_c_item (new CClickableItem);
     sub_c_item->setting_name = GetName();
     sub_c_item->item_list = (*it)->obj_text;
     sub_c_item->obj_list = (*it)->obj_ls;
     copy((*it)->obj_text.begin(), (*it)->obj_text.end(),
          back_inserter( c_item->item_list ));
     copy((*it)->obj_ls.begin(), (*it)->obj_ls.end(),
          back_inserter( c_item->obj_list ));
     strtmp 
        = (sub_c_item->item_list.size()/2 > 1)? "identical sources" : "source";
     sub_c_item->description 
            = GetHasComment(sub_c_item->item_list.size()/2, "contig")
              + strtmp + " that do not match another contig source.";
     c_item->subcategories.push_back(sub_c_item);
   }
   c_item->description 
       = GetOtherComment(c_item->item_list.size()/2, 
                         "inconsistent contig source.",
                         "inconsistent contig sources") 
          + diff;

   INCONSISTENT_BIOSOURCE_biosrc.clear();
};


void CBioseq_TEST_TERMINAL_NS :: TestOnObj(const CBioseq& bioseq)
{
   validator :: EBioseqEndIsType begin_n, begin_gap, end_n, end_gap;
   validator::CheckBioseqEndsForNAndGap(thisInfo.scope->GetBioseqHandle(bioseq),
                                        begin_n, 
                                        begin_gap,
                                        end_n,
                                        end_gap); 
   if (begin_n != validator :: eBioseqEndIsType_None
           || end_n != validator :: eBioseqEndIsType_None) {
     thisInfo.test_item_list[GetName()].push_back(GetDiscItemText(bioseq));
     thisInfo.test_item_objs[GetName()].push_back(CConstRef <CObject>(&bioseq));
   }
};

void CBioseq_TEST_TERMINAL_NS :: GetReport(CRef <CClickableItem>& c_item)
{
   c_item->obj_list = thisInfo.test_item_objs[GetName()];
   c_item->description = GetHasComment(c_item->item_list.size(), "sequence")
                         + "terminal Ns";
};


void CBioseq_DISC_MISSING_DEFLINES :: TestOnObj(const CBioseq& bioseq) 
{
    CSeqdesc_CI 
       title_ci(thisInfo.scope->GetBioseqHandle(bioseq), CSeqdesc::e_Title);
    if (!bioseq.IsAa() && !title_ci) {
        thisInfo.test_item_list[GetName()].push_back(GetDiscItemText(bioseq));
        thisInfo.test_item_objs[GetName()]
                   .push_back(CConstRef <CObject>(&bioseq));
    }
};
 
void CBioseq_DISC_MISSING_DEFLINES :: GetReport(CRef <CClickableItem>& c_item)
{
  c_item->obj_list = thisInfo.test_item_objs[GetName()];
  c_item->description 
        = GetHasComment(c_item->item_list.size(), "bioseq") 
          + "no definition line";
};

void CSeqEntry_test_on_defline :: TestOnObj(const CSeq_entry& seq_entry)
{
    if (thisTest.is_Defl_run) return;
    thisTest.is_Defl_run = true;

    unsigned i=0; 
    string desc, title;
    bool run_set = (thisTest.tests_run.find(GetName_set()) != end_it);
    bool run_seqch = (thisTest.tests_run.find(GetName_seqch()) != end_it);

    ITERATE ( vector <const CSeqdesc*>, it, title_seqdesc) {
       if ((title = (*it)->GetTitle()).empty()) continue; 
       desc = GetDiscItemText(**it, *(title_seqdesc_seqentry[i]));
       CConstRef <CObject> sd_ref (*it);

       // ONCALLER_DEFLINE_ON_SET
       if (run_set && title_seqdesc_seqentry[i]->IsSet()) {
            thisInfo.test_item_list[GetName_set()].push_back(desc);
            thisInfo.test_item_objs[GetName_set()].push_back(sd_ref);
       }
     
       // DISC_TITLE_ENDS_WITH_SEQUENCE
       if (run_seqch) {
          size_t pos = title.find_last_not_of("ACTG");
          if ( (pos == string::npos && title.size() >= 19) 
                   || (pos != string::npos && title.size() - pos >= 19) ) {
               thisInfo.test_item_list[GetName_seqch()].push_back(desc);
               thisInfo.test_item_objs[GetName_seqch()].push_back(sd_ref);
          }
       }
       i++;
    }
};

void CSeqEntry_DISC_TITLE_ENDS_WITH_SEQUENCE :: GetReport(CRef <CClickableItem>& c_item)
{
  c_item->description
     = GetOtherComment(c_item->item_list.size(), "defline appears", "deflines appear")
       + " to end with sequence characters";
};

void CSeqEntry_ONCALLER_DEFLINE_ON_SET :: GetReport(CRef <CClickableItem>& c_item)
{
   c_item->description 
     = GetOtherComment(c_item->item_list.size(), "title on sets was", "titles on sets were") 
          + " found";

   ITERATE (vector <string>, it, thisInfo.test_item_list[GetName()]) {   // for oncaller left window display
      CRef <CClickableItem> c_sub (new CClickableItem);
      c_sub->description = *it;
      c_sub->item_list.push_back(*it);
      c_item->subcategories.push_back(c_sub); 
   }

   if (thisInfo.expand_defline_on_set) {
       c_item->expanded = true;
   }
};

//new comb


void CBioseq_on_feat_cnt :: TestOnObj(const CBioseq& bioseq)
{
   if (thisTest.is_FeatCnt_run) return;
   thisTest.is_FeatCnt_run = true;

   string feat_nm;
   Str2Int feat_cnt_ls;
   string desc = GetDiscItemText(bioseq);
   CConstRef <CObject> seq_ref (&bioseq);

// non_prot_feat == ! CSeqFeatData::eSubtype_prot
   ITERATE (vector <const CSeq_feat*>, it, non_prot_feat) { 
      feat_nm = ( (*it)->GetData().IsProt() )? "A_" : "nA_";
      strtmp = (*it)->GetData().GetKey(CSeqFeatData::eVocabulary_genbank);
      if (thisInfo.featkey_modified.find(strtmp) 
             != thisInfo.featkey_modified.end()) {
         strtmp = thisInfo.featkey_modified[strtmp];
      }
      feat_nm += strtmp;
      if (feat_cnt_ls.find(feat_nm) == feat_cnt_ls.end()) {
         feat_cnt_ls[feat_nm] = 1;
      }
      else {
         feat_cnt_ls[feat_nm] ++;
      }
   }    
   if (thisTest.tests_run.find(GetName_gen()) != end_it) {
      ITERATE (Str2Int, it, feat_cnt_ls) {
          thisInfo.test_item_list[GetName_gen()].push_back(
                         it->first + "$" + NStr::IntToString(it->second));
      }
   }
   if (thisTest.tests_run.find(GetName_oncaller()) != end_it) {
      if (feat_cnt_ls.empty()) {
        strtmp = bioseq.IsAa() ? "missing_A" : "missing_nA";
        thisInfo.test_item_list[GetName_oncaller()]
                    .push_back(strtmp +"$" + desc);
        thisInfo.test_item_objs[GetName_oncaller() + "$" +strtmp]
                   .push_back(seq_ref);
      }
      ITERATE (Str2Int, it, feat_cnt_ls) { // feat$cnt@bsq;
         strtmp = it->first + "$" + NStr::UIntToString(it->second) + "@" + desc;
         thisInfo.test_item_list[GetName_oncaller()].push_back(strtmp);
         strtmp = "$" + it->first + "@" + NStr::UIntToString(it->second);
         thisInfo.test_item_objs[GetName_oncaller() +strtmp].push_back(seq_ref);
      }
   }
};

void CBioseq_DISC_FEATURE_COUNT_oncaller :: GetReport(CRef <CClickableItem>& c_item)
{
   Str2Strs feat2cnt_seq, cnt2seq;
   GetTestItemList(c_item->item_list, feat2cnt_seq);
   c_item->item_list.clear();
   vector <CConstRef <CObject> >* a_ls, *na_ls, *missing_ls;
   
   a_ls = na_ls = 0;
   string feat_nm, cnt_str;
   unsigned i_cnt, tot_cnt;
   ITERATE (Str2Strs, it, feat2cnt_seq) { 
      if (it->first == "missing_A") {
          a_ls = &(thisInfo.test_item_objs[GetName() + "$" + it->first]);
      }
      else if (it->first == "missing_nA") {
          na_ls = &(thisInfo.test_item_objs[GetName() + "$" + it->first]);
      }
   }
   string cnt;
   ITERATE (Str2Strs, it, feat2cnt_seq) {
      if (it->first.find("missing") == string::npos) { // feat$cnt@seq
         cnt2seq.clear();
         GetTestItemList(it->second, cnt2seq, "@");
         if (it->first.find("nA_") != string::npos) { 
            feat_nm = it->first.substr(3); 
            if (na_ls && !na_ls->empty()) {
              cnt2seq["0"] = feat2cnt_seq["missing_nA"];
              missing_ls = na_ls;
            }
         }
         else {
           feat_nm = it->first.substr(2); 
           if (a_ls && !a_ls->empty()) {
              cnt2seq["0"] = feat2cnt_seq["missing_A"];
              missing_ls = a_ls;
           }
         }
         if (cnt2seq.size() ==1 ) {
            cnt = cnt2seq.begin()->first;
            strtmp = GetName() + "$" + it->first + "@" + cnt;
            AddSubcategory(c_item, strtmp, &(cnt2seq.begin()->second), 
                           feat_nm + ": " + cnt + " presents", 
                           feat_nm + ": " + cnt + " present", 
                           e_OtherComment, false);
         }  
         else {
            CRef <CClickableItem> c_sub (new CClickableItem);
            c_sub->setting_name = GetName();
            tot_cnt = 0;
            ITERATE (Str2Strs, jt, cnt2seq) {
               if (it->first == "0") {
                  i_cnt = jt->second.size();
                  CRef <CClickableItem> c_seq_sub(new CClickableItem);
                  c_seq_sub->setting_name = GetName();
                  c_seq_sub->item_list = jt->second;
                  c_seq_sub->obj_list = *missing_ls;
                  c_seq_sub->description = GetHasComment(i_cnt, "bioseq")
                                             + "0 " + feat_nm + " feature.";
                  c_sub->subcategories.push_back(c_seq_sub);
               }
               else {
                  i_cnt = NStr::StringToUInt(jt->first);
                  strtmp = GetName() + "$" + it->first + "@" + jt->first;
                  string desc = jt->first + " " + feat_nm
                                   + GetNoun(i_cnt, " feature");
                  AddSubcategory(c_sub, strtmp, &(jt->second),
                         "bioseq", desc, e_HasComment, false);
               }
               tot_cnt += i_cnt;
            }
            c_sub->description = feat_nm + ": " + NStr::UIntToString(tot_cnt)
                                  + GetNoun(tot_cnt, " present") 
                                  + " (inconsistent).";
            c_item->subcategories.push_back(c_sub);
         }
      }
   }

   c_item->description = "Feature Count";
   c_item->expanded = true;
};

void CBioseq_DISC_FEATURE_COUNT :: GetReport(CRef <CClickableItem>& c_item)
{
   Str2Strs feat2cnt;
   GetTestItemList(c_item->item_list, feat2cnt);
   c_item->item_list.clear();
    
   unsigned cnt;
   string feat_nm;
   ITERATE (Str2Strs, it, feat2cnt) {
      feat_nm = (it->first.substr(0,3) == "nA_") ? 
                      it->first.substr(3) : it->first.substr(2);
      if (it != feat2cnt.begin()) {
         c_item.Reset(new CClickableItem);
         c_item->setting_name = GetName();
         thisInfo.disc_report_data.push_back(c_item);
      }
      cnt = 0;
      ITERATE (vector <string>, jt, it->second) {
          cnt += NStr::StringToUInt(*jt);
      }
      c_item->description 
         = feat_nm + ": " + NStr::UIntToString(cnt) + ( (cnt>1)? " present" : " presents");
         feat_nm = it->first.substr(3); 
   }
};


// new comb
void CSeqEntry_on_comment :: TestOnObj(const CSeq_entry& seq_entry)
{
  if (thisTest.is_Comment_run) return;
  thisTest.is_Comment_run = true;

  unsigned i=0;
  string desc;
  bool run_has = (thisTest.tests_run.find(GetName_has()) != end_it);
  bool run_mix = (thisTest.tests_run.find(GetName_mix()) != end_it);

  ITERATE (vector <const CSeqdesc*>, it, comm_seqdesc) {
     CConstRef <CObject> feat_ref (*it);
     desc = GetDiscItemText(**it, *(comm_seqdesc_seqentry[i])); 
     // ONCALLER_COMMENT_PRESENT
     if (run_has) {
        if (m_all_same && comm_seqdesc[0]->GetComment() != (*it)->GetComment()){
             m_all_same  = false; 
        }
        thisInfo.test_item_list[GetName_has()].push_back(desc);
        thisInfo.test_item_objs[GetName_has()].push_back(feat_ref);
     }

     // DISC_MISMATCHED_COMMENTS
     if (run_mix) {
        strtmp = (*it)->GetComment();
        strtmp = strtmp.empty() ? "empty" : strtmp;
        thisInfo.test_item_list[GetName_mix()].push_back(strtmp + "$" + desc);
        thisInfo.test_item_objs[GetName_mix() +"$" + strtmp].push_back(feat_ref);
     }
  }
};


void CSeqEntry_DISC_MISMATCHED_COMMENTS :: GetReport(CRef <CClickableItem>& c_item)
{
  Str2Strs comm2ls;
  GetTestItemList(c_item->item_list, comm2ls);
  c_item->item_list.clear();
  if (comm2ls.size() > 1) {
     ITERATE (Str2Strs, it, comm2ls) {
        AddSubcategory(c_item, GetName()+ "$" + it->first, &(it->second),
                  "comment", it->first, e_ContainsComment);
     }
     c_item->description = "Mismatched comments were found";
  }
};

void CSeqEntry_ONCALLER_COMMENT_PRESENT :: GetReport(CRef <CClickableItem>& c_item)
{
    c_item->obj_list = thisInfo.test_item_objs[GetName()];
    unsigned cnt = c_item->item_list.size();
    strtmp = (m_all_same? " (all same)." : " (some different). ");
    c_item->description 
      = GetOtherComment(cnt, 
                        "comment descriptor was found", 
                        "comment descriptors were found")
        + strtmp;
};



bool CSeqEntry_test_on_pub :: IsNameCapitalizationOk(const string& name)
{
  if (name.empty()) {
        return true;
  }

  unsigned    len;
  bool need_cap = true, rval = true, found;
  bool needed_lower = false, found_lower = false;

  size_t pos;
  pos = 0;
  while (pos < name.size() && rval) {
    if (isalpha (name[pos])) {
      if (!need_cap) {
          needed_lower = true;
          if (!isupper(name[pos])) {
               found_lower = true;
          }
      }

      /* check to see if this is a short name */
      if (need_cap && !isupper (name[pos])) {
        if (!pos || name[pos-1] == ' ') {
          found = false;
          ITERATE (vector <string>, it, thisInfo.short_auth_nms) {
            len = (*it).size();
            if (name.size() > len 
                  && ((pos = name.find(*it)) != string::npos) 
                  && name[len] == ' ') {
               found = true;
               pos += len -1; //in order to set need_cap correctly
               break;
            }
          }
          if (!found) rval = false;
        } 
        else rval = false;
      }

      need_cap = false;
    } 
    else {
        need_cap = true;
    }
    pos++;
  }
  if (rval && needed_lower && !found_lower) {
       rval = false;
  }

  return rval;

}; //IsNameCapitalizationOK


bool CSeqEntry_test_on_pub :: IsAuthorInitialsCapitalizationOk(const string& nm_init)
{
   strtmp = nm_init;
   size_t pos = 0;
   while (pos != nm_init.size()) {
      if (isalpha(nm_init[pos]) && !isupper(nm_init[pos])) {
          return false;
      }
      else {
         pos ++;
      }
   }
   return true;
};  // IsAuthorInitialsCapitalizationOk



bool CSeqEntry_test_on_pub :: NameIsBad(const CRef <CAuthor>& auth_nm) 
{
   const CPerson_id& pid = auth_nm->GetName();
   if (pid.IsName()) {
       const CName_std& name_std = pid.GetName();
       if ( !IsNameCapitalizationOk(name_std.GetLast()) ) {
          return true;
       }
       else if ( name_std.CanGetFirst() 
                   && !IsNameCapitalizationOk(name_std.GetFirst())) {
              return true;
       }
       else if (name_std.CanGetInitials() 
                  && !IsAuthorInitialsCapitalizationOk(name_std.GetInitials())){
               return true;
       }
   }

   return false;
}

bool CSeqEntry_test_on_pub :: HasBadAuthorName(const CAuth_list& auths)
{
  if (auths.GetNames().IsStd()) 
    ITERATE (list <CRef <CAuthor> >, it, auths.GetNames().GetStd()) {
      if (NameIsBad(*it)) {
          return true;
      }
    }

  return false;
 
}; // HasBadAuthorName 

bool CSeqEntry_test_on_pub ::  AuthNoFirstLastNames(const CAuth_list& auths)
{
   if (auths.GetNames().IsStd()) {
      ITERATE (list <CRef <CAuthor> >, it, auths.GetNames().GetStd()) {
        const CPerson_id& pid = (*it)->GetName();
        if (!pid.IsName()) {
            return false;
        }
        const CName_std& name = pid.GetName();
        if (name.GetLast().empty()) {
            return true;
        }
        if (!name.CanGetFirst() || name.GetFirst().empty()) {
           return true;
        }
        if (!name.CanGetInitials() || name.GetInitials().empty()) {
           return true;
        }
      }
      return false;
   }
   else {
      return true;
   }
};

void CSeqEntry_test_on_pub :: CheckBadAuthCapsOrNoFirstLastNamesInPubdesc(const list <CRef <CPub> >& pubs, const string& desc, CConstRef <CObject> obj_ref)
{
  bool isBadCap = false, isMissing = false;
  bool has_pmid = false;
  ITERATE (list <CRef <CPub> >, it, pubs) {
    if ( (*it)->IsPmid() ) {
        has_pmid = true;
        break;
    }
  }
  
  ITERATE (list <CRef <CPub> >, it, pubs) {
    if ( !(*it)->IsProc() && (*it)->IsSetAuthors() ) {
      // Check Cit_art.ids (ArticleIdSet may have pmid)
      if ((*it)->IsArticle() && (*it)->GetArticle().CanGetIds()) {
         ITERATE (list <CRef <CArticleId> >, ait, 
                      (*it)->GetArticle().GetIds().Get()) {
            if ((*ait)->IsPubmed()) {
               has_pmid = true;
            }
         }
         
      }
      if (m_run_cap 
             && !isBadCap 
             && (isBadCap = HasBadAuthorName( (*it)->GetAuthors() ) )) {
          thisInfo.test_item_list[GetName_cap()].push_back(desc);
          thisInfo.test_item_objs[GetName_cap()].push_back(obj_ref);
      }
      if (m_run_missing 
               && !isMissing && !has_pmid 
               && (isMissing = AuthNoFirstLastNames( (*it)->GetAuthors() ))) {
          thisInfo.test_item_list[GetName_missing()].push_back(desc);
          thisInfo.test_item_objs[GetName_missing()].push_back(obj_ref);
      }
      if ( (!m_run_cap || isBadCap) && (!m_run_missing || isMissing) ) {
            return; 
      }
    }
  }

}; //CheckBadAuthCapsOrNoFirstLastNamesInPubdesc

void CSeqEntry_DISC_CHECK_AUTH_NAME :: GetReport(CRef <CClickableItem>& c_item)
{
   c_item->obj_list = thisInfo.test_item_objs[GetName()]; // how to redun.?
   c_item->description 
          = GetOtherComment(c_item->item_list.size(), "pub", "pubs") 
            + " missing author's first or last name";
};

void CSeqEntry_DISC_CHECK_AUTH_CAPS :: GetReport(CRef <CClickableItem>& c_item)
{
   c_item->obj_list = thisInfo.test_item_objs[GetName()];  // how about redun.?
   c_item->description = GetHasComment(c_item->item_list.size(), "pub") + 
                                                      "incorrect author capitalization.";
};

END_NCBI_SCOPE
