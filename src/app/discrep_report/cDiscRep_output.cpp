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

#include "hDiscRep_app.hpp"
#include "hchecking_class.hpp"
#include "hauto_disc_class.hpp"
#include "hDiscRep_config.hpp"
#include "hUtilib.hpp"

#include <iostream>
#include <fstream>

USING_NCBI_SCOPE;
using namespace std;
using namespace objects;
using namespace DiscRepNmSpc;

static CDiscRepInfo thisInfo;
static string       strtmp;
static COutputConfig& oc = thisInfo.output_config;
static CTestGrp     thisGrp;

static s_fataltag extra_fatal [] = {
        {"MISSING_GENOMEASSEMBLY_COMMENTS", NULL, NULL}
};

static s_fataltag disc_fatal[] = {
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
        {"RNA_NO_PRODUCT", NULL, NULL},
        {"SHOW_HYPOTHETICAL_CDS_HAVING_GENE_NAME", NULL, NULL},
        {"SUSPECT_PRODUCT_NAMES", "Remove organism from product name", NULL},
        {"SUSPECT_PRODUCT_NAMES", "Possible parsing error or incorrect formatting; remove inappropriate symbols", NULL},
        {"TEST_OVERLAPPING_RRNAS", NULL, NULL}
};

static unsigned disc_cnt = sizeof(disc_fatal)/sizeof(s_fataltag);
static unsigned extra_cnt = sizeof(extra_fatal)/sizeof(s_fataltag);

bool CRepConfig :: NeedsTag(const string& setting_name, const string& desc, const s_fataltag* tags, const unsigned& cnt)
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


void CRepConfig :: AddListOutputTags()
{
  string setting_name, desc, sub_desc;
  bool na_cnt_grt1 = false;
  vector <string> arr;
  ITERATE (vector <CRef <CClickableItem> >, it, thisInfo.disc_report_data) {
    if ((*it)->setting_name == "DISC_COUNT_NUCLEOTIDES") { // IsDiscCntGrt1
      arr.clear();
      arr = NStr::Tokenize((*it)->description, " ", arr);
      na_cnt_grt1 = (isInt(arr[0]) && NStr::StringToInt(arr[0]) > 1) ? true : false;
      break;
    }
  }
 
  // AddOutpuTag
  NON_CONST_ITERATE (vector <CRef <CClickableItem> > , it, thisInfo.disc_report_data) {
     setting_name = (*it)->setting_name;
     desc = (*it)->description;
     NON_CONST_ITERATE (vector <CRef <CClickableItem> >, sit, (*it)->subcategories) {
        sub_desc = (*sit)->description;
        if (NeedsTag(setting_name, sub_desc, disc_fatal, disc_cnt)
               || (oc.add_extra_output_tag 
                      && NeedsTag(setting_name, sub_desc, extra_fatal, extra_cnt)) ){
           if (setting_name == "DISC_SOURCE_QUALS_ASNDISC") {
             if (sub_desc.find("some missing") != string::npos
                                   || sub_desc.find("some duplicate") != string::npos)
                  (*sit)->description = "FATAL: " + (*sit)->description;

           }
           else (*sit)->description = "FATAL: " + (*sit)->description;
        }
     }

     if (NeedsTag(setting_name, desc, disc_fatal, disc_cnt)
              || (oc.add_extra_output_tag 
                               && NeedsTag(setting_name, desc, extra_fatal, extra_cnt))) {
        if (setting_name == "DISC_SOURCE_QUALS_ASNDISC") {
          if (NStr::FindNoCase(desc, "taxname (all present, all unique)") != string::npos) {
             if (na_cnt_grt1) (*it)->description = "FATAL: " + (*it)->description;
          }
          else if (desc.find("some missing") != string::npos 
                       || desc.find("some duplicate") != string::npos)
              (*it)->description = "FATAL: " + (*it)->description;
        }
        else (*it)->description = "FATAL: " + (*it)->description;
     } 
  } 
};

void CRepConfig :: Export()
{
  if (oc.add_output_tag || oc.add_extra_output_tag) AddListOutputTags();

  *(oc.output_f) << "Discrepancy Report Results\n\n"
       << "Summary\n";
  WriteDiscRepSummary();

  *(oc.output_f) << "\n\nDetailed Report\n";
  WriteDiscRepDetails(thisInfo.disc_report_data, oc.use_flag);
};  // Discrepancy:: Export

bool CRepConfig :: RmTagInDescp(string& str, const string& tag)
{
  string::size_type pos;

  pos = str.find(tag);
  if (pos != string::npos && !pos) {
      str =  CTempString(str).substr(tag.size());
      return true;
  }
  else return ( false );
}


void CRepConfig :: WriteDiscRepSummary()
{
  string desc;

  ITERATE (vector <CRef < CClickableItem > >, it, thisInfo.disc_report_data) {
      desc = (*it)->description;
      if ( desc.empty()) continue;
      // FATAL tag
      if (RmTagInDescp(desc, "FATAL: ")) *(oc.output_f)  << "FATAL:";
      *(oc.output_f) << (*it)->setting_name << ": " << desc << endl;
      if ("SUSPECT_PRODUCT_NAMES" == (*it)->setting_name)
            WriteDiscRepSubcategories((*it)->subcategories);
  }
  
} // WriteDiscrepancyReportSummary


void CRepConfig :: WriteDiscRepSubcategories(const vector <CRef <CClickableItem> >& subcategories, unsigned ident)
{
   unsigned i;
   ITERATE (vector <CRef <CClickableItem> >, it, subcategories) {
      for (i=0; i< ident; i++) *(oc.output_f) << "\t"; 
      *(oc.output_f) << (*it)->description  << endl; 
      WriteDiscRepSubcategories( (*it)->subcategories, ident+1);
   }
} // WriteDiscRepSubcategories



bool OkToExpand(CRef < CClickableItem > c_item) 
{
  if (c_item->setting_name == "DISC_FEATURE_COUNT") return false;
  else if ( (c_item->item_list.empty()/*||oc.expand_report_categories[c_item->setting_name]*/)
             && !(c_item->subcategories.empty()) ) 
      return true;
  else return false;
}; // OkToExpand



void CRepConfig :: WriteDiscRepDetails(vector <CRef < CClickableItem > > disc_rep_dt, bool use_flag, bool IsSubcategory)
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
      if (RmTagInDescp(desc, "FATAL: ")) prefix = "FATAL:" + prefix;

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
        WriteDiscRepItems((*it), prefix);
      }
      if ( OkToExpand ((*it)) ) {
        for (i = 0; i< (*it)->subcategories.size() -1; i++)
              (*it)->subcategories[i]->next_sibling = true;
        if (use_flag && (*it)->setting_name == "DISC_INCONSISTENT_BIOSRC_DEFLINE") {
          WriteDiscRepDetails ( (*it)->subcategories, false, false);
        } else {
          WriteDiscRepDetails ( (*it)->subcategories, oc.use_flag, true);
        }
      } 
  }
};   // WriteDiscRepDetails



bool CRepConfig :: SuppressItemListForFeatureTypeForOutputFiles(const string& setting_name)
{
  if (setting_name == "DISC_FEATURE_COUNT"
       || setting_name == "DISC_MISSING_SRC_QUAL"
       || setting_name == "DISC_DUP_SRC_QUAL"
       || setting_name == "DISC_DUP_SRC_QUAL_DATA"
       || setting_name == "DISC_SOURCE_QUALS_ASNDISC") {
    return true;
  } else return false;

}; // SuppressItemListForFeatureTypeForOutputFiles



void CRepConfig :: WriteDiscRepItems(CRef <CClickableItem> c_item, const string& prefix)
{
   if (oc.use_flag && 
         SuppressItemListForFeatureTypeForOutputFiles (c_item->setting_name)) {
       if (!prefix.empty()) *(oc.output_f) << prefix;
       string desc = c_item->description;
       RmTagInDescp(desc, "FATAL: ");
       *(oc.output_f) << desc << endl;
ITERATE (vector <string>, it, c_item->item_list) {  // necessary?
  *(oc.output_f) << *it << endl;
//cerr << c_item->setting_name << "  " << *it << endl;
};
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
     StandardWriteDiscRepItems (oc, c_item, prefix, true);
  }
  if ((!c_item->next_sibling && c_item->subcategories.empty()) 
        || (!c_item->subcategories.empty() && !c_item->item_list.empty()) ) // for !OkToExpand
             *(oc.output_f) << endl;
  
} // WriteDiscRepItems()


void CRepConfig :: StandardWriteDiscRepItems(COutputConfig& oc, const CClickableItem* c_item, const string& prefix, bool list_features_if_subcat)
{
  if (!prefix.empty())  *(oc.output_f) << prefix;
  string desc = c_item->description;
  RmTagInDescp(desc, "FATAL: ");
  *(oc.output_f) << desc << endl;

  if (c_item->subcategories.empty() || list_features_if_subcat) {
          /*
           if (oc.use_feature_table_fmt)
           {
             list_copy = ReplaceDiscrepancyItemWithFeatureTableStrings (vnp);
             vnp = list_copy;
           }
        */
      ITERATE (vector <string>, it, c_item->item_list) *(oc.output_f) << *it << endl;
  }

}; // StandardWriteDiscRepItems()

string CRepConfig :: x_GetDesc4GItem(string desc)
{
   // FATAL tag
   if (RmTagInDescp(desc, "FATAL: ")) return ("FATAL: " + desc);
   else return desc;
};

void CRepConfig :: x_InputRepToGbenchItem(const CClickableItem& c_item,  CClickableText& item) 
{
   if (!c_item.item_list.empty()) {
      item.SetObjdescs().insert(item.SetObjdescs().end(), 
                                          c_item.item_list.begin(), c_item.item_list.end());
      item.SetObjects().insert(item.SetObjects().end(), c_item.obj_list.begin(),
                 c_item.obj_list.end());
   }
   if (!c_item.subcategories.empty()) {
      string desc;
      ITERATE (vector < CRef <CClickableItem > >, sit, c_item.subcategories) {
            desc = (*sit)->description;
            if (desc.empty()) continue;
            desc = x_GetDesc4GItem(desc);
            CRef <CClickableText> sub (new CClickableText(desc));             
            x_InputRepToGbenchItem(**sit, *sub);
            item.SetSubitems().push_back(sub);
      }
   }
};

void CRepConfig :: Export(vector <CRef <CClickableText> >& item_list)
{
   if (oc.add_output_tag || oc.add_extra_output_tag) AddListOutputTags(); 
   string desc;
   ITERATE (vector <CRef < CClickableItem > >, it, thisInfo.disc_report_data) {
      desc = (*it)->description;
      if ( desc.empty()) continue;
      desc = x_GetDesc4GItem(desc);
      CRef <CClickableText> item (new CClickableText(desc));             
      x_InputRepToGbenchItem(**it, *item);
      item_list.push_back(item);
   } 
   thisInfo.disc_report_data.clear();
   thisInfo.test_item_list.clear();
   thisInfo.test_item_objs.clear();
 
   // clean for gbench usage. 
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
};
