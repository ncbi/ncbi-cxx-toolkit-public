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
 *   Output for Cpp Discrepany Report
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>

#include <objtools/discrepancy_report/hDiscRep_config.hpp>
#include <objtools/discrepancy_report/hDiscRep_output.hpp>
#include <objtools/discrepancy_report/hUtilib.hpp>

#include <iostream>
#include <fstream>

BEGIN_NCBI_SCOPE

USING_NCBI_SCOPE;
USING_SCOPE(objects);
USING_SCOPE(DiscRepNmSpc);

static CDiscRepInfo thisInfo;
static string       strtmp;
static COutputConfig& oc = thisInfo.output_config;
static CTestGrp     thisGrp;

static const s_fataltag extra_fatal [] = {
        {"MISSING_GENOMEASSEMBLY_COMMENTS", NULL, NULL}
};

static const s_fataltag disc_fatal[] = {
        {"BAD_LOCUS_TAG_FORMAT", NULL, NULL},
        {"DISC_BACTERIAL_PARTIAL_NONEXTENDABLE_PROBLEMS", NULL, NULL},
        {"DISC_BACTERIA_SHOULD_NOT_HAVE_MRNA", NULL, NULL},
        {"DISC_BAD_BGPIPE_QUALS", NULL, NULL},
        {"DISC_CITSUBAFFIL_CONFLICT", NULL, "No citsubs were found!"},
        {"DISC_INCONSISTENT_MOLTYPES", NULL, "Moltypes are consistent"},
        {"DISC_MAP_CHROMOSOME_CONFLICT", NULL, NULL},
        {"DISC_MICROSATELLITE_REPEAT_TYPE", NULL, NULL},
        {"DISC_MISSING_AFFIL", NULL, NULL},
        {"DISC_NONWGS_SETS_PRESENT", NULL, NULL},
        {"DISC_QUALITY_SCORES", "Quality scores are missing on some sequences.", NULL},
        {"DISC_RBS_WITHOUT_GENE", NULL, NULL},
        {"DISC_SHORT_RRNA", NULL, NULL},
        {"DISC_SEGSETS_PRESENT", NULL, NULL},
        {"DISC_SOURCE_QUALS_ASNDISC", "collection-date", NULL},
        {"DISC_SOURCE_QUALS_ASNDISC", "country", NULL},
        {"DISC_SOURCE_QUALS_ASNDISC", "isolation-source", NULL},
        {"DISC_SOURCE_QUALS_ASNDISC", "host", NULL},
        {"DISC_SOURCE_QUALS_ASNDISC", "strain", NULL},
        {"DISC_SOURCE_QUALS_ASNDISC", "taxname", NULL},
        {"DISC_SUBMITBLOCK_CONFLICT", NULL, NULL},
        {"DISC_SUSPECT_RRNA_PRODUCTS", NULL, NULL},
        {"DISC_TITLE_AUTHOR_CONFLICT", NULL, NULL},
        {"DISC_UNPUB_PUB_WITHOUT_TITLE", NULL, NULL},
        {"DISC_USA_STATE", NULL, NULL},
        {"EC_NUMBER_ON_UNKNOWN_PROTEIN", NULL, NULL},
        {"EUKARYOTE_SHOULD_HAVE_MRNA", "no mRNA present", NULL},
        {"INCONSISTENT_LOCUS_TAG_PREFIX", NULL, NULL},
        {"INCONSISTENT_PROTEIN_ID", NULL, NULL},
        {"MISSING_GENES", NULL, NULL},
        {"MISSING_LOCUS_TAGS", NULL, NULL},
        {"MISSING_PROTEIN_ID", NULL, NULL},
        {"ONCALLER_ORDERED_LOCATION", NULL, NULL},
        {"PARTIAL_CDS_COMPLETE_SEQUENCE", NULL, NULL},
        {"PSEUDO_MISMATCH", NULL, NULL},
        {"RNA_CDS_OVERLAP", "coding regions are completely contained in RNAs", NULL},
        {"RNA_CDS_OVERLAP", "coding region is completely contained in an RNA", NULL},
        {"RNA_CDS_OVERLAP", "coding regions completely contain RNAs", NULL},
        {"RNA_CDS_OVERLAP", "coding region completely contains an RNA", NULL},
        {"RNA_NO_PRODUCT", NULL, NULL},
        {"SHOW_HYPOTHETICAL_CDS_HAVING_GENE_NAME", NULL, NULL},
        {"SUSPECT_PRODUCT_NAMES", "Remove organism from product name", NULL},
        {"SUSPECT_PRODUCT_NAMES", "Possible parsing error or incorrect formatting; remove inappropriate symbols", NULL},
        {"TEST_OVERLAPPING_RRNAS", NULL, NULL}
};

static const unsigned disc_cnt = ArraySize(disc_fatal);
static const unsigned extra_cnt = ArraySize(extra_fatal);

bool CDiscRepOutput :: x_NeedsTag(const string& setting_name, const string& desc, const s_fataltag* tags, const unsigned& cnt)
{
   unsigned i;
   for (i =0; i< cnt; i++) {
     if (setting_name == tags[i].setting_name
          && (!tags[i].notag_description 
                 || NStr::FindNoCase(desc, tags[i].notag_description) == string::npos)
          && (!tags[i].description 
                 || NStr::FindNoCase(desc, tags[i].description) != string::npos) ) {
        return true;
     }
   }
   return false;
};


void CDiscRepOutput :: x_AddListOutputTags()
{
  string setting_name, desc, sub_desc;
  bool na_cnt_grt1 = false;
  vector <string> arr;
  ITERATE (vector <CRef <CClickableItem> >, it, thisInfo.disc_report_data) {
    if ((*it)->setting_name == "DISC_COUNT_NUCLEOTIDES") { // IsDiscCntGrt1
      arr.clear();
      arr = NStr::Tokenize((*it)->description, " ", arr);
      na_cnt_grt1 = (isInt(arr[0]) && NStr::StringToInt(arr[0]) > 1);
      break;
    }
  }
 
  // AddOutpuTag
  bool has_sub_trna_in_cds = false;
  NON_CONST_ITERATE (vector <CRef <CClickableItem> > , it, thisInfo.disc_report_data) {
     setting_name = (*it)->setting_name;
     desc = (*it)->description; 

     // check subcategories first;
     NON_CONST_ITERATE (vector <CRef <CClickableItem> >, sit, (*it)->subcategories) {
        sub_desc = (*sit)->description;
        if (x_NeedsTag(setting_name, sub_desc, disc_fatal, disc_cnt)
               || (oc.add_extra_output_tag 
                      && x_NeedsTag(setting_name, sub_desc, extra_fatal, extra_cnt)) ){
           if (setting_name == "DISC_SOURCE_QUALS_ASNDISC") {
             if (sub_desc.find("some missing") != string::npos
                       || sub_desc.find("some duplicate") != string::npos)
                  (*sit)->description = "FATAL: " + sub_desc;

           }
           else if (setting_name == "RNA_CDS_OVERLAP"
                      && (sub_desc.find("contain ") != string::npos
                            || sub_desc.find("contains ") != string::npos)) {
                ITERATE (vector <string>, iit, (*sit)->item_list) {
                   if ( (*iit).find(": tRNA\t") != string::npos) {
                      has_sub_trna_in_cds = true;
                      (*sit)->description = "FATAL: " + sub_desc;
                      break;
                   }
                }
           }
           else (*sit)->description = "FATAL: " + sub_desc;
        }
     }

     // check self
     if (x_NeedsTag(setting_name, desc, disc_fatal, disc_cnt)
            || (oc.add_extra_output_tag 
                   && x_NeedsTag(setting_name, desc, extra_fatal, extra_cnt))) {
        if (setting_name == "DISC_SOURCE_QUALS_ASNDISC") {
          if (NStr::FindNoCase(desc, "taxname (all present, all unique)") 
                  != string::npos) {
             if (na_cnt_grt1) {
                   (*it)->description = "FATAL: " + (*it)->description;
             }
          }
          else if (desc.find("some missing") != string::npos 
                       || desc.find("some duplicate") != string::npos) {
              (*it)->description = "FATAL: " + (*it)->description;
          }
        }
        else {
          (*it)->description = "FATAL: " + (*it)->description;
        }
     } 
     else if (setting_name == "RNA_CDS_OVERLAP" && has_sub_trna_in_cds) {
         (*it)->description = "FATAL: " + (*it)->description; 
     }
  } 
};


void CDiscRepOutput :: GoGetRep (vector <CRef <CTestAndRepData> >& test_category)
{
   NON_CONST_ITERATE (vector <CRef <CTestAndRepData> >, it, test_category) {
       CRef < CClickableItem > c_item (new CClickableItem);
       strtmp = (*it)->GetName();
       if (thisInfo.test_item_list.find(strtmp) 
                                    != thisInfo.test_item_list.end()) {
            c_item->setting_name = strtmp;
            c_item->item_list = thisInfo.test_item_list[strtmp];
            if ( strtmp != "LOCUS_TAGS"
                        && strtmp != "INCONSISTENT_PROTEIN_ID_PREFIX") {
                  thisInfo.disc_report_data.push_back(c_item);
            }
            (*it)->GetReport(c_item); 
       }
       else if ( (*it)->GetName() == "DISC_FEATURE_COUNT") {
           (*it)->GetReport(c_item); 
       } 
   }
};

void CDiscRepOutput :: CollectRepData()
{
  GoGetRep(thisGrp.tests_on_SubmitBlk);
  GoGetRep(thisGrp.tests_on_Bioseq);
  GoGetRep(thisGrp.tests_on_Bioseq_aa);
  GoGetRep(thisGrp.tests_on_Bioseq_na);
  GoGetRep(thisGrp.tests_on_Bioseq_CFeat);
  GoGetRep(thisGrp.tests_on_Bioseq_CFeat_NotInGenProdSet);
  GoGetRep(thisGrp.tests_on_Bioseq_NotInGenProdSet);
  GoGetRep(thisGrp.tests_on_SeqEntry);
  GoGetRep(thisGrp.tests_on_SeqEntry_feat_desc);
  GoGetRep(thisGrp.tests_4_once);
  GoGetRep(thisGrp.tests_on_BioseqSet);
  GoGetRep(thisGrp.tests_on_Bioseq_CFeat_CSeqdesc);

} // CollectRepData()


void CDiscRepOutput :: Export()
{
  if (oc.add_output_tag || oc.add_extra_output_tag) {
       x_AddListOutputTags();
  }

  *(oc.output_f) << "Discrepancy Report Results\n\n" << "Summary\n";
  x_WriteDiscRepSummary();

  *(oc.output_f) << "\n\nDetailed Report\n";
  x_WriteDiscRepDetails(thisInfo.disc_report_data, oc.use_flag);

   // clearn
   thisInfo.test_item_list.clear();
   thisInfo.test_item_objs.clear();
   thisInfo.disc_report_data.clear();

};  // Asndisc:: Export

bool CDiscRepOutput :: x_RmTagInDescp(string& str, const string& tag)
{
  string::size_type pos;

  pos = str.find(tag);
  if (pos != string::npos && !pos) {
      str =  CTempString(str).substr(tag.size());
      return true;
  }
  else {
     return false;
  }
}


void CDiscRepOutput :: x_WriteDiscRepSummary()
{
  string desc;

  ITERATE (vector <CRef < CClickableItem > >, it, thisInfo.disc_report_data) {
      desc = (*it)->description;
      if ( desc.empty()) continue;

      // FATAL tag
      if (x_RmTagInDescp(desc, "FATAL: ")) *(oc.output_f)  << "FATAL:";
      *(oc.output_f) << (*it)->setting_name << ": " << desc << endl;
      if ("SUSPECT_PRODUCT_NAMES" == (*it)->setting_name) {
            x_WriteDiscRepSubcategories((*it)->subcategories);
      }
  }
  
} // WriteDiscrepancyReportSummary


void CDiscRepOutput:: x_WriteDiscRepSubcategories(const vector <CRef <CClickableItem> >& subcategories, unsigned ident)
{
   unsigned i;
   ITERATE (vector <CRef <CClickableItem> >, it, subcategories) {
      for (i=0; i< ident; i++) *(oc.output_f) << "\t"; 
      *(oc.output_f) << (*it)->description  << endl; 
      x_WriteDiscRepSubcategories( (*it)->subcategories, ident+1);
   }
} // WriteDiscRepSubcategories


bool CDiscRepOutput :: x_OkToExpand(CRef < CClickableItem > c_item) 
{
  if (c_item->setting_name == "DISC_FEATURE_COUNT") {
       return false;
  }
  else if ( (c_item->item_list.empty() || c_item->expanded)
//                     ||oc.expand_report_categories[c_item->setting_name])
             && !(c_item->subcategories.empty()) ) {
      return true;
  }
  else {
     return false;
  }
};

void CDiscRepOutput :: x_WriteDiscRepDetails(vector <CRef < CClickableItem > > disc_rep_dt, bool use_flag, bool IsSubcategory)
{
  string prefix, desc;
  unsigned i;
  NON_CONST_ITERATE (vector < CRef < CClickableItem > >, it, disc_rep_dt) {
      desc = (*it)->description;
      if ( desc.empty() ) continue;

      // prefix
      if (use_flag) {
          prefix = "DiscRep_";
          prefix += IsSubcategory ? "SUB:" : "ALL:";
          strtmp = (*it)->setting_name;
          prefix += strtmp.empty()? " " : strtmp + ": ";
      }
 
      // FATAL tag
      if (x_RmTagInDescp(desc, "FATAL: ")) prefix = "FATAL:" + prefix;

      // summary report
      if (oc.summary_report) {
         *(oc.output_f) << prefix << desc << endl;
/*
         if ( ( oc.add_output_tag || oc.add_extra_output_tag) 
               && SubsHaveTags((*it), oc))
            oc.expand_report_categories[(*it)->setting_name] = TRUE;
*/
      }
      else {
/*
        if ((oc.add_output_tag || oc.add_extra_output_tag) 
              && SubsHaveTags((*it), oc))
            ptr = StringISearch(prefix, "FATAL: ");
            if (ptr == NULL || ptr != prefix)
              SetStringValue (&prefix, "FATAL", ExistingTextOption_prefix_colon 
*/
         x_WriteDiscRepItems((*it), prefix);
      }
      if ( x_OkToExpand ((*it)) ) {
        for (i = 0; i< (*it)->subcategories.size() -1; i++) {
              (*it)->subcategories[i]->next_sibling = true;
        }
        if (use_flag 
               && (*it)->setting_name == "DISC_INCONSISTENT_BIOSRC_DEFLINE") {
           x_WriteDiscRepDetails ( (*it)->subcategories, false, false);
        } 
        else {
           x_WriteDiscRepDetails ( (*it)->subcategories, oc.use_flag, true);
        }
      } 
  }
};   // WriteDiscRepDetails



bool CDiscRepOutput :: x_SuppressItemListForFeatureTypeForOutputFiles(const string& setting_name)
{
  if (setting_name == "DISC_FEATURE_COUNT"
       || setting_name == "DISC_MISSING_SRC_QUAL"
       || setting_name == "DISC_DUP_SRC_QUAL"
       || setting_name == "DISC_DUP_SRC_QUAL_DATA"
       || setting_name == "DISC_SOURCE_QUALS_ASNDISC") {
    return true;
  } 
  else {
     return false;
  }

}; // SuppressItemListForFeatureTypeForOutputFiles



void CDiscRepOutput :: x_WriteDiscRepItems(CRef <CClickableItem> c_item, const string& prefix)
{
   if (oc.use_flag && 
         x_SuppressItemListForFeatureTypeForOutputFiles (c_item->setting_name)) {
       if (!prefix.empty()) {
            *(oc.output_f) << prefix;
       }
       string desc = c_item->description;
       x_RmTagInDescp(desc, "FATAL: ");
       *(oc.output_f) << desc << endl;
/* unnecessary
    for (vnp = c_item->subcategories; vnp != NULL; vnp = vnp->next) {
          dip = vnp->data.ptrvalue;
          if (dip != NULL) {
            if (!StringHasNoText (descr_prefix)) {
              fprintf (fp, "%s:", descr_prefix);
            }
            fprintf (fp, "%s\n", dip->description);
          }
    }
       *(oc.output_f) << endl;
*/
  } 
  else {
     x_StandardWriteDiscRepItems (oc, c_item, prefix, true);
  }
  if ((!c_item->next_sibling && c_item->subcategories.empty()) 
        || (!c_item->subcategories.empty() && !c_item->item_list.empty()) ) {// for !OkToExpand

      *(oc.output_f) << endl;
 }
  
} // WriteDiscRepItems()


void CDiscRepOutput :: x_StandardWriteDiscRepItems(COutputConfig& oc, const CClickableItem* c_item, const string& prefix, bool list_features_if_subcat)
{
  if (!prefix.empty())  *(oc.output_f) << prefix;
  string desc = c_item->description;
  x_RmTagInDescp(desc, "FATAL: ");
  *(oc.output_f) << desc << endl;

  if (c_item->subcategories.empty() || list_features_if_subcat) {
          /*
           if (oc.use_feature_table_fmt)
           {
             list_copy = ReplaceDiscrepancyItemWithFeatureTableStrings (vnp);
             vnp = list_copy;
           }
        */
      ITERATE (vector <string>, it, c_item->item_list) {
         *(oc.output_f) << *it << endl;
      }
  }

}; // StandardWriteDiscRepItems()

string CDiscRepOutput :: x_GetDesc4GItem(string desc)
{
   // FATAL tag
   if (x_RmTagInDescp(desc, "FATAL: ")) {
        return ("FATAL: " + desc);
   }
   else {
        return desc;
   }
};

void CDiscRepOutput :: x_OutputRepToGbenchItem(const CClickableItem& c_item,  CClickableText& item) 
{
   if (!c_item.item_list.empty()) {
      item.SetObjdescs().insert(item.SetObjdescs().end(), 
                                c_item.item_list.begin(), 
                                c_item.item_list.end());
      if (!c_item.obj_list.empty()) {
            item.SetObjects().insert(item.SetObjects().end(), 
                                     c_item.obj_list.begin(),
                                     c_item.obj_list.end());
      }
   }
   if (!c_item.subcategories.empty()) {
      string desc;
      ITERATE (vector < CRef <CClickableItem > >, sit, c_item.subcategories) {
            desc = (*sit)->description;
            if (desc.empty()) continue;
            desc = x_GetDesc4GItem(desc);
            CRef <CClickableText> sub (new CClickableText(desc));             
            x_OutputRepToGbenchItem(**sit, *sub);
            item.SetSubitems().push_back(sub);
      }
   }
   if (c_item.expanded) {
         item.SetOwnExpanded();
   }
};


void CDiscRepOutput :: x_InitializeOnCallerToolPriorities()
{
  m_sOnCallerToolPriorities["DISC_COUNT_NUCLEOTIDES"] = 1;
  m_sOnCallerToolPriorities["DISC_DUP_DEFLINE"] = 2;
  m_sOnCallerToolPriorities["DISC_MISSING_DEFLINES"] = 3;
  m_sOnCallerToolPriorities["TEST_TAXNAME_NOT_IN_DEFLINE"] = 4;
  m_sOnCallerToolPriorities["TEST_HAS_PROJECT_ID"] = 5;
  m_sOnCallerToolPriorities["ONCALLER_BIOPROJECT_ID"] = 6;
  m_sOnCallerToolPriorities["ONCALLER_DEFLINE_ON_SET"] = 7;
  m_sOnCallerToolPriorities["TEST_UNWANTED_SET_WRAPPER"] = 8;
  m_sOnCallerToolPriorities["TEST_COUNT_UNVERIFIED"] = 9;
  m_sOnCallerToolPriorities["DISC_SRC_QUAL_PROBLEM"] = 10;
  m_sOnCallerToolPriorities["DISC_FEATURE_COUNT_oncaller"] = 11;
  m_sOnCallerToolPriorities["NO_ANNOTATION"] = 12;
  m_sOnCallerToolPriorities["DISC_FEATURE_MOLTYPE_MISMATCH"] = 13;
  m_sOnCallerToolPriorities["TEST_ORGANELLE_NOT_GENOMIC"] = 14;
  m_sOnCallerToolPriorities["DISC_INCONSISTENT_MOLTYPES"] = 15;
  m_sOnCallerToolPriorities["DISC_CHECK_AUTH_CAPS"] = 16;
  m_sOnCallerToolPriorities["ONCALLER_CONSORTIUM"] = 17;
  m_sOnCallerToolPriorities["DISC_UNPUB_PUB_WITHOUT_TITLE"] = 18;
  m_sOnCallerToolPriorities["DISC_TITLE_AUTHOR_CONFLICT"] = 19;
  m_sOnCallerToolPriorities["DISC_SUBMITBLOCK_CONFLICT"] = 20;
  m_sOnCallerToolPriorities["DISC_CITSUBAFFIL_CONFLICT"] = 21;
  m_sOnCallerToolPriorities["DISC_CHECK_AUTH_NAME"] = 22;
  m_sOnCallerToolPriorities["DISC_MISSING_AFFIL"] = 23;
  m_sOnCallerToolPriorities["DISC_USA_STATE"] = 24;
  m_sOnCallerToolPriorities["DISC_CITSUB_AFFIL_DUP_TEXT"] = 25;
  m_sOnCallerToolPriorities["DISC_DUP_SRC_QUAL"] = 26;
  m_sOnCallerToolPriorities["DISC_MISSING_SRC_QUAL"] = 27; 
  m_sOnCallerToolPriorities["DISC_DUP_SRC_QUAL_DATA"] = 28;
  m_sOnCallerToolPriorities["ONCALLER_DUPLICATE_PRIMER_SET"] = 29; 
  m_sOnCallerToolPriorities["ONCALLER_MORE_NAMES_COLLECTED_BY"] = 30;
  m_sOnCallerToolPriorities["ONCALLER_MORE_OR_SPEC_NAMES_IDENTIFIED_BY"] = 31;
  m_sOnCallerToolPriorities["ONCALLER_SUSPECTED_ORG_IDENTIFIED"] = 32;
  m_sOnCallerToolPriorities["ONCALLER_SUSPECTED_ORG_COLLECTED"] = 33;
  m_sOnCallerToolPriorities["DISC_MISSING_VIRAL_QUALS"] = 34;
  m_sOnCallerToolPriorities["DISC_INFLUENZA_DATE_MISMATCH"] = 35; 
  m_sOnCallerToolPriorities["DISC_HUMAN_HOST"] = 36;
  m_sOnCallerToolPriorities["DISC_SPECVOUCHER_TAXNAME_MISMATCH"] = 37;
  m_sOnCallerToolPriorities["DISC_BIOMATERIAL_TAXNAME_MISMATCH"] = 38;
  m_sOnCallerToolPriorities["DISC_CULTURE_TAXNAME_MISMATCH"] = 39;
  m_sOnCallerToolPriorities["DISC_STRAIN_TAXNAME_MISMATCH"] = 40;
  m_sOnCallerToolPriorities["DISC_BACTERIA_SHOULD_NOT_HAVE_ISOLATE"] = 41;
  m_sOnCallerToolPriorities["DISC_BACTERIA_MISSING_STRAIN"] = 42;
  m_sOnCallerToolPriorities["ONCALLER_STRAIN_TAXNAME_CONFLICT"] = 43; 
  m_sOnCallerToolPriorities["TEST_SP_NOT_UNCULTURED"] = 44;
  m_sOnCallerToolPriorities["DISC_REQUIRED_CLONE"] = 45; 
  m_sOnCallerToolPriorities["TEST_UNNECESSARY_ENVIRONMENTAL"] = 46;
  m_sOnCallerToolPriorities["TEST_AMPLIFIED_PRIMERS_NO_ENVIRONMENTAL_SAMPLE"] =47;
  m_sOnCallerToolPriorities["ONCALLER_MULTISRC"] = 48;
  m_sOnCallerToolPriorities["ONCALLER_COUNTRY_COLON"] = 49;
  m_sOnCallerToolPriorities["END_COLON_IN_COUNTRY"] = 50;
  m_sOnCallerToolPriorities["DUP_DISC_ATCC_CULTURE_CONFLICT"] = 51;
  m_sOnCallerToolPriorities["DUP_DISC_CBS_CULTURE_CONFLICT"] = 52;
  m_sOnCallerToolPriorities["ONCALLER_STRAIN_CULTURE_COLLECTION_MISMATCH"] = 53; 
  m_sOnCallerToolPriorities["ONCALLER_MULTIPLE_CULTURE_COLLECTION"] = 54;
  m_sOnCallerToolPriorities["DISC_TRINOMIAL_SHOULD_HAVE_QUALIFIER"] = 55;
  m_sOnCallerToolPriorities["ONCALLER_CHECK_AUTHORITY"] = 56;
  m_sOnCallerToolPriorities["DISC_MAP_CHROMOSOME_CONFLICT"] = 57; 
  m_sOnCallerToolPriorities["DISC_METAGENOMIC"] = 58;
  m_sOnCallerToolPriorities["DISC_METAGENOME_SOURCE"] = 59; 
  m_sOnCallerToolPriorities["DISC_RETROVIRIDAE_DNA"] = 60;
  m_sOnCallerToolPriorities["NON_RETROVIRIDAE_PROVIRAL"] = 61;
  m_sOnCallerToolPriorities["ONCALLER_HIV_RNA_INCONSISTENT"] = 62;
  m_sOnCallerToolPriorities["RNA_PROVIRAL"] = 63;
  m_sOnCallerToolPriorities["TEST_BAD_MRNA_QUAL"] = 64; 
  m_sOnCallerToolPriorities["DISC_MITOCHONDRION_REQUIRED"] = 65;
  m_sOnCallerToolPriorities["TEST_UNWANTED_SPACER"] = 66;
  m_sOnCallerToolPriorities["TEST_SMALL_GENOME_SET_PROBLEM"] = 67;
  m_sOnCallerToolPriorities["ONCALLER_SUPERFLUOUS_GENE"] = 68;
  m_sOnCallerToolPriorities["ONCALLER_GENE_MISSING"] = 69;
  m_sOnCallerToolPriorities["DISC_GENE_PARTIAL_CONFLICT"] = 70;
  m_sOnCallerToolPriorities["DISC_BAD_GENE_STRAND"] = 71;
  m_sOnCallerToolPriorities["TEST_UNNECESSARY_VIRUS_GENE"] = 72;
  m_sOnCallerToolPriorities["NON_GENE_LOCUS_TAG"] = 73;
  m_sOnCallerToolPriorities["DISC_RBS_WITHOUT_GENE"] = 74;
  m_sOnCallerToolPriorities["ONCALLER_ORDERED_LOCATION"] = 75;
  m_sOnCallerToolPriorities["MULTIPLE_CDS_ON_MRNA"] = 76;
  m_sOnCallerToolPriorities["DISC_CDS_WITHOUT_MRNA"] = 77;
  m_sOnCallerToolPriorities["DISC_mRNA_ON_WRONG_SEQUENCE_TYPE"] = 78;
  m_sOnCallerToolPriorities["DISC_BACTERIA_SHOULD_NOT_HAVE_MRNA"] = 79;
  m_sOnCallerToolPriorities["TEST_MRNA_OVERLAPPING_PSEUDO_GENE"] = 80;
  m_sOnCallerToolPriorities["TEST_EXON_ON_MRNA"] = 81;
  m_sOnCallerToolPriorities["DISC_CDS_HAS_NEW_EXCEPTION"] = 82; 
  m_sOnCallerToolPriorities["DISC_SHORT_INTRON"] = 83;
  m_sOnCallerToolPriorities["DISC_EXON_INTRON_CONFLICT"] = 84;
  m_sOnCallerToolPriorities["PSEUDO_MISMATCH"] = 85;
  m_sOnCallerToolPriorities["RNA_NO_PRODUCT"] = 86;
  m_sOnCallerToolPriorities["FIND_BADLEN_TRNAS"] = 87;
  m_sOnCallerToolPriorities["DISC_MICROSATELLITE_REPEAT_TYPE"] = 88; 
  m_sOnCallerToolPriorities["DISC_SHORT_RRNA"] = 89;
  m_sOnCallerToolPriorities["DISC_POSSIBLE_LINKER"] = 90; 
  m_sOnCallerToolPriorities["DISC_HAPLOTYPE_MISMATCH"] = 91;
  m_sOnCallerToolPriorities["DISC_FLATFILE_FIND_ONCALLER"] = 92;
  m_sOnCallerToolPriorities["DISC_CDS_PRODUCT_FIND"] = 93;
  m_sOnCallerToolPriorities["DISC_SUSPICIOUS_NOTE_TEXT"] = 94;
  m_sOnCallerToolPriorities["DISC_CHECK_RNA_PRODUCTS_AND_COMMENTS"] = 95; 
  m_sOnCallerToolPriorities["DISC_INTERNAL_TRANSCRIBED_SPACER_RRNA"] = 96;
  m_sOnCallerToolPriorities["ONCALLER_COMMENT_PRESENT"] = 97;
  m_sOnCallerToolPriorities["SUSPECT_PRODUCT_NAMES"] = 98;
  m_sOnCallerToolPriorities["UNCULTURED_NOTES_ONCALLER"] = 99;
};

void CDiscRepOutput :: x_InitializeOnCallerToolGroups()
{
   m_sOnCallerToolGroups["DISC_FEATURE_MOLTYPE_MISMATCH"] = eMol;
   m_sOnCallerToolGroups["TEST_ORGANELLE_NOT_GENOMIC"] = eMol;
   m_sOnCallerToolGroups["DISC_INCONSISTENT_MOLTYPES"] = eMol;

   m_sOnCallerToolGroups["DISC_CHECK_AUTH_CAPS"] = eCitSub;
   m_sOnCallerToolGroups["ONCALLER_CONSORTIUM"] = eCitSub;
   m_sOnCallerToolGroups["DISC_UNPUB_PUB_WITHOUT_TITLE"] = eCitSub;
   m_sOnCallerToolGroups["DISC_TITLE_AUTHOR_CONFLICT"] = eCitSub;
   m_sOnCallerToolGroups["DISC_SUBMITBLOCK_CONFLICT"] = eCitSub;
   m_sOnCallerToolGroups["DISC_CITSUBAFFIL_CONFLICT"] = eCitSub;
   m_sOnCallerToolGroups["DISC_CHECK_AUTH_NAME"] = eCitSub;
   m_sOnCallerToolGroups["DISC_MISSING_AFFIL"] = eCitSub;
   m_sOnCallerToolGroups["DISC_USA_STATE"] = eCitSub;
   m_sOnCallerToolGroups["DISC_CITSUB_AFFIL_DUP_TEXT"] = eCitSub;
   
   m_sOnCallerToolGroups["DISC_DUP_SRC_QUAL"] = eSource;
   m_sOnCallerToolGroups["DISC_MISSING_SRC_QUAL"] = eSource;
   m_sOnCallerToolGroups["DISC_DUP_SRC_QUAL_DATA"] = eSource;
   m_sOnCallerToolGroups["DISC_MISSING_VIRAL_QUALS"] = eSource;
   m_sOnCallerToolGroups["DISC_INFLUENZA_DATE_MISMATCH"] = eSource;
   m_sOnCallerToolGroups["DISC_HUMAN_HOST"] = eSource;
   m_sOnCallerToolGroups["DISC_SPECVOUCHER_TAXNAME_MISMATCH"] = eSource;
   m_sOnCallerToolGroups["DISC_BIOMATERIAL_TAXNAME_MISMATCH"] = eSource;
   m_sOnCallerToolGroups["DISC_CULTURE_TAXNAME_MISMATCH"] = eSource;
   m_sOnCallerToolGroups["DISC_STRAIN_TAXNAME_MISMATCH"] = eSource;
   m_sOnCallerToolGroups["DISC_BACTERIA_SHOULD_NOT_HAVE_ISOLATE"] = eSource;
   m_sOnCallerToolGroups["DISC_BACTERIA_MISSING_STRAIN"] = eSource;
   m_sOnCallerToolGroups["ONCALLER_STRAIN_TAXNAME_CONFLICT"] = eSource;
   m_sOnCallerToolGroups["TEST_SP_NOT_UNCULTURED"] = eSource;
   m_sOnCallerToolGroups["DISC_REQUIRED_CLONE"] = eSource;
   m_sOnCallerToolGroups["TEST_UNNECESSARY_ENVIRONMENTAL"] = eSource;
   m_sOnCallerToolGroups["TEST_AMPLIFIED_PRIMERS_NO_ENVIRONMENTAL_SAMPLE"] = eSource;
   m_sOnCallerToolGroups["ONCALLER_MULTISRC"] = eSource;
   m_sOnCallerToolGroups["ONCALLER_COUNTRY_COLON"] = eSource;
   m_sOnCallerToolGroups["END_COLON_IN_COUNTRY"] = eSource;
   m_sOnCallerToolGroups["DUP_DISC_ATCC_CULTURE_CONFLICT"] = eSource;
   m_sOnCallerToolGroups["DUP_DISC_CBS_CULTURE_CONFLICT"] = eSource;
   m_sOnCallerToolGroups["ONCALLER_STRAIN_CULTURE_COLLECTION_MISMATCH"]=eSource;
   m_sOnCallerToolGroups["ONCALLER_MULTIPLE_CULTURE_COLLECTION"] = eSource;
   m_sOnCallerToolGroups["DISC_TRINOMIAL_SHOULD_HAVE_QUALIFIER"] = eSource;
   m_sOnCallerToolGroups["ONCALLER_CHECK_AUTHORITY"] = eSource;
   m_sOnCallerToolGroups["DISC_MAP_CHROMOSOME_CONFLICT"] = eSource;
   m_sOnCallerToolGroups["DISC_METAGENOMIC"] = eSource;
   m_sOnCallerToolGroups["DISC_METAGENOME_SOURCE"] = eSource;
   m_sOnCallerToolGroups["DISC_RETROVIRIDAE_DNA"] = eSource;
   m_sOnCallerToolGroups["NON_RETROVIRIDAE_PROVIRAL"] = eSource;
   m_sOnCallerToolGroups["ONCALLER_HIV_RNA_INCONSISTENT"] = eSource;
   m_sOnCallerToolGroups["RNA_PROVIRAL"] = eSource;
   m_sOnCallerToolGroups["TEST_BAD_MRNA_QUAL"] = eSource;
   m_sOnCallerToolGroups["DISC_MITOCHONDRION_REQUIRED"] = eSource;
   m_sOnCallerToolGroups["TEST_UNWANTED_SPACER"] = eSource;
   m_sOnCallerToolGroups["TEST_SMALL_GENOME_SET_PROBLEM"] = eSource;
   m_sOnCallerToolGroups["ONCALLER_DUPLICATE_PRIMER_SET"] = eSource;
   m_sOnCallerToolGroups["ONCALLER_MORE_NAMES_COLLECTED_BY"] = eSource;
   m_sOnCallerToolGroups["ONCALLER_MORE_OR_SPEC_NAMES_IDENTIFIED_BY"] = eSource;
   m_sOnCallerToolGroups["ONCALLER_SUSPECTED_ORG_IDENTIFIED"] = eSource;
   m_sOnCallerToolGroups["ONCALLER_SUSPECTED_ORG_COLLECTED"] = eSource;

   m_sOnCallerToolGroups["ONCALLER_SUPERFLUOUS_GENE"] = eFeature;
   m_sOnCallerToolGroups["ONCALLER_GENE_MISSING"] = eFeature;
   m_sOnCallerToolGroups["DISC_GENE_PARTIAL_CONFLICT"] = eFeature;
   m_sOnCallerToolGroups["DISC_BAD_GENE_STRAND"] = eFeature;
   m_sOnCallerToolGroups["TEST_UNNECESSARY_VIRUS_GENE"] = eFeature;
   m_sOnCallerToolGroups["NON_GENE_LOCUS_TAG"] = eFeature;
   m_sOnCallerToolGroups["DISC_RBS_WITHOUT_GENE"] = eFeature;
   m_sOnCallerToolGroups["ONCALLER_ORDERED_LOCATION"] = eFeature;
   m_sOnCallerToolGroups["MULTIPLE_CDS_ON_MRNA"] = eFeature;
   m_sOnCallerToolGroups["DISC_CDS_WITHOUT_MRNA"] = eFeature;
   m_sOnCallerToolGroups["DISC_mRNA_ON_WRONG_SEQUENCE_TYPE"] = eFeature;
   m_sOnCallerToolGroups["DISC_BACTERIA_SHOULD_NOT_HAVE_MRNA"] = eFeature;
   m_sOnCallerToolGroups["TEST_MRNA_OVERLAPPING_PSEUDO_GENE"] = eFeature;
   m_sOnCallerToolGroups["TEST_EXON_ON_MRNA"] = eFeature;
   m_sOnCallerToolGroups["DISC_CDS_HAS_NEW_EXCEPTION"] = eFeature;
   m_sOnCallerToolGroups["DISC_SHORT_INTRON"] = eFeature;
   m_sOnCallerToolGroups["DISC_EXON_INTRON_CONFLICT"] = eFeature;
   m_sOnCallerToolGroups["PSEUDO_MISMATCH"] = eFeature;
   m_sOnCallerToolGroups["RNA_NO_PRODUCT"] = eFeature;
   m_sOnCallerToolGroups["FIND_BADLEN_TRNA"] = eFeature;
   m_sOnCallerToolGroups["DISC_MICROSATELLITE_REPEAT_TYPE"] = eFeature;
   m_sOnCallerToolGroups["DISC_SHORT_RRNA"] = eFeature;

   m_sOnCallerToolGroups["DISC_FLATFILE_FIND_ONCALLER"] = eSuspectText;
   m_sOnCallerToolGroups["DISC_FLATFILE_FIND_ONCALLER_FIXABLE"] = eSuspectText;
   m_sOnCallerToolGroups["DISC_FLATFILE_FIND_ONCALLER_UNFIXABLE"] =eSuspectText;
   m_sOnCallerToolGroups["DISC_CDS_PRODUCT_FIND"] = eSuspectText;
   m_sOnCallerToolGroups["DISC_SUSPICIOUS_NOTE_TEXT:"] = eSuspectText;
   m_sOnCallerToolGroups["DISC_CHECK_RNA_PRODUCTS_AND_COMMENTS"] = eSuspectText;
   m_sOnCallerToolGroups["DISC_INTERNAL_TRANSCRIBED_SPACER_RRNA"] =eSuspectText;
   m_sOnCallerToolGroups["ONCALLER_COMMENT_PRESENT"] = eSuspectText;
   m_sOnCallerToolGroups["SUSPECT_PRODUCT_NAMES"] = eSuspectText;
   m_sOnCallerToolGroups["UNCULTURED_NOTES_ONCALLER"] = eSuspectText;
};

void CDiscRepOutput :: x_OrderResult(Int2Int& ord2i_citem)
{
   unsigned i=0;
   int pri;

   ITERATE (vector <CRef <CClickableItem> >, it, thisInfo.disc_report_data) {
      if (m_sOnCallerToolPriorities.find((*it)->setting_name)
            != m_sOnCallerToolPriorities.end()) {
         pri = m_sOnCallerToolPriorities[(*it)->setting_name];
      }
      else pri = m_sOnCallerToolPriorities.size() + i + 1;
      ord2i_citem[pri] = i;
      i++;
   }
};

void CDiscRepOutput :: x_GroupResult(map <EOnCallerGrp, string>& grp_idx_str) 
{
   unsigned i=0;
   ITERATE (vector <CRef <CClickableItem> >, it, thisInfo.disc_report_data) {
      if (m_sOnCallerToolGroups.find((*it)->setting_name)
            != m_sOnCallerToolGroups.end()) {
          EOnCallerGrp e_grp = m_sOnCallerToolGroups[(*it)->setting_name];
          if (grp_idx_str.find(e_grp) == grp_idx_str.end()) {
              grp_idx_str[e_grp] = NStr::UIntToString(i);
          }
          else {
              grp_idx_str[e_grp] += ("," + NStr::UIntToString(i));
          }
      }
      i++;
   }
};

void CDiscRepOutput :: x_ReorderAndGroupOnCallerResults(Int2Int& ord2i_citem, map <EOnCallerGrp, string>& grp_idx_str)
{
   x_InitializeOnCallerToolPriorities();
   x_InitializeOnCallerToolGroups();
   x_OrderResult(ord2i_citem);
   x_GroupResult(grp_idx_str);
};

string CDiscRepOutput :: x_GetGrpName(EOnCallerGrp e_grp)
{
   switch (e_grp) {
      case eMol: return string("Molecule type tests");
      case eCitSub: return string("Cit-sub type tests");
      case eSource: return string("Source tests");
      case eFeature: return string("Feature tests");
      case eSuspectText: return string("Suspect text tests");
      default:
        NCBI_USER_THROW("Unknown Oncaller Tool group type: " + NStr::IntToString((int)e_grp));
   };
   return kEmptyStr;
};

CRef <CClickableItem> CDiscRepOutput :: x_CollectSameGroupToGbench(Int2Int& ord2i_citem, EOnCallerGrp e_grp, const string& grp_idxes)
{
    vector <string> arr;
    arr = NStr::Tokenize(grp_idxes, ",", arr);
    Int2Int sub_ord2idx;
    unsigned idx;
    CRef <CClickableItem> new_item (new CClickableItem);
    new_item->description = x_GetGrpName(e_grp);
    new_item->expanded = true;
    ITERATE (vector <string>, it, arr) {
        idx = NStr::StringToUInt(*it);
        strtmp = thisInfo.disc_report_data[idx]->setting_name;
        sub_ord2idx[m_sOnCallerToolPriorities[strtmp]] = idx;
    }
    switch (e_grp) {
       case eMol: 
       case eCitSub:
       case eSource:
       case eFeature:
       {
          ITERATE (Int2Int, it, sub_ord2idx) {
             new_item->subcategories.push_back(
                                      thisInfo.disc_report_data[it->second]);
             ord2i_citem[it->first] = -1;
          }  
          break;
       }
       case eSuspectText:
       {
          unsigned typo_cnt = 0;
          ITERATE (Int2Int, it, sub_ord2idx) {
             if (thisInfo.disc_report_data[it->second]->setting_name
                     == "SUSPECT_PRODUCT_NAMES") {
                NON_CONST_ITERATE (vector <CRef <CClickableItem> >, 
                         sit, 
                         thisInfo.disc_report_data[it->second]->subcategories) {
                    if ((*sit)->description == "Typo") {
                        ITERATE (vector <CRef <CClickableItem> >, ssit, 
                                    (*sit)->subcategories) {
                           typo_cnt += (*ssit)->item_list.size();
                        }
                        
                       (*sit)->description 
                          = CTestAndRepData::GetContainsComment(typo_cnt,
                                                                "product name")
                             + "typos.";
                       new_item->subcategories.push_back(*sit);
                       break;
                    }
                }
             }
             else {
                new_item->subcategories.push_back(
                                        thisInfo.disc_report_data[it->second]);
             }
             ord2i_citem[it->first] = -1;
             if (new_item->subcategories.empty()) {
                new_item.Reset(0);
             }
          }
          break;
       }
       default: break;
    }

    return new_item;
};

void CDiscRepOutput :: x_SendItemToGbench(CRef <CClickableItem> citem, vector <CRef <CClickableText> >& item_list)
{
   string desc = citem->description;
   if ( desc.empty()) return;
   desc = x_GetDesc4GItem(desc);
   CRef <CClickableText> item (new CClickableText(desc));
   x_OutputRepToGbenchItem(*citem, *item);
   item_list.push_back(item);
};

void CDiscRepOutput :: Export(vector <CRef <CClickableText> >& item_list)
{
   if (!thisInfo.disc_report_data.empty()) {
      if (oc.add_output_tag || oc.add_extra_output_tag) {
         x_AddListOutputTags(); 
      }
      string desc;
      if (thisInfo.report == "Oncaller") {
          Int2Int ord2i_citem, sub_ord2idx;
          map <EOnCallerGrp, string> grp_idx_str;
          vector <string> arr;
          x_ReorderAndGroupOnCallerResults(ord2i_citem, grp_idx_str);
          ITERATE (Int2Int, it, ord2i_citem) {
             if (it->second < 0) {
                 continue;
             }
             CRef <CClickableItem> this_item 
                 = thisInfo.disc_report_data[it->second];
             if (m_sOnCallerToolGroups.find(this_item->setting_name) 
                    != m_sOnCallerToolGroups.end()){
                EOnCallerGrp 
                   e_grp = m_sOnCallerToolGroups[this_item->setting_name];
                CRef <CClickableItem> 
                    new_item(x_CollectSameGroupToGbench(ord2i_citem, 
                                                        e_grp,
                                                        grp_idx_str[e_grp]));
                if (new_item.NotEmpty()) {
                     x_SendItemToGbench(new_item, item_list);
                }
             }
             else {
                x_SendItemToGbench(this_item, item_list);
             }
          }
      }
      else {
        ITERATE (vector <CRef <CClickableItem> >, it,thisInfo.disc_report_data){
           x_SendItemToGbench(*it, item_list);
        } 
      }
      thisInfo.disc_report_data.clear();
   };

   thisInfo.test_item_list.clear();
   thisInfo.test_item_objs.clear();
 
   // moved to CollectTestList
   // clean for gbench usage. 
/*
   thisGrp.tests_on_Bioseq.clear();
   thisGrp.tests_on_Bioseq_na.clear();
   thisGrp.tests_on_Bioseq_aa.clear();
   thisGrp.tests_on_Bioseq_CFeat.clear();
   thisGrp.tests_on_Bioseq_CFeat_NotInGenProdSet.clear();
   thisGrp.tests_on_Bioseq_NotInGenProdSet.clear();
   thisGrp.tests_on_Bioseq_CFeat_CSeqdesc.clear();
   thisGrp.tests_on_SeqEntry.clear();
   thisGrp.tests_on_SeqEntry_feat_desc.clear();
   thisGrp.tests_4_once.clear();
   thisGrp.tests_on_BioseqSet.clear();
   thisGrp.tests_on_SubmitBlk.clear();
*/
};

void CDiscRepOutput :: Export(vector <CRef <CClickableItem> >& c_item, const string& setting_name)
{
   if (!thisInfo.disc_report_data.empty()) {
      ITERATE ( vector <CRef <CClickableItem> >, it, thisInfo.disc_report_data){
         if ( (*it)->setting_name == setting_name) {
            c_item.push_back((*it));
         }
      }
   }

   thisInfo.disc_report_data.clear();
   thisInfo.test_item_list.clear();
   thisInfo.test_item_objs.clear();
};

void CDiscRepOutput :: Export(CRef <CClickableItem>& c_item, const string& setting_name)
{
   if (!thisInfo.disc_report_data.empty()) {
      if (thisInfo.disc_report_data.size() == 1) {
           c_item = thisInfo.disc_report_data[0];
      }
      else {
         ITERATE ( vector <CRef <CClickableItem> >, it, thisInfo.disc_report_data) {
            if ( (*it)->setting_name == setting_name) {
              c_item = (*it);
            }
         }
      }
   }

   thisInfo.disc_report_data.clear();
   thisInfo.test_item_list.clear();
   thisInfo.test_item_objs.clear();
} 

END_NCBI_SCOPE
