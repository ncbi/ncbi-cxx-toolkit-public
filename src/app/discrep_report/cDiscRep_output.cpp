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
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbienv.hpp>
#include <corelib/ncbiargs.hpp>
#include <connect/ncbi_core_cxx.hpp>

// Objects includes
#include <objects/seq/Bioseq.hpp>
#include <objects/seqloc/Seq_id.hpp>
#include <objects/seqloc/Seq_loc.hpp>
#include <objects/seqloc/Seq_interval.hpp>
#include <objects/seq/Seq_inst.hpp>
#include <objects/seqfeat/Seq_feat.hpp>

// Object Manager includes
#include <objmgr/object_manager.hpp>
#include <objmgr/scope.hpp>
#include <objmgr/seq_vector.hpp>
#include <objmgr/seqdesc_ci.hpp>
#include <objmgr/feat_ci.hpp>
#include <objmgr/align_ci.hpp>
#include <objtools/data_loaders/genbank/gbloader.hpp>

#include <serial/objistr.hpp>
#include <serial/serial.hpp>

#include <common/test_assert.h>

#include "hDiscRep_app.hpp"
#include "hchecking_class.hpp"
#include "hauto_disc_class.hpp"
#include "hDiscRep_config.hpp"

#include <iostream>
#include <fstream>

USING_NCBI_SCOPE;
using namespace std;
using namespace objects;
using namespace DiscRepNmSpc;

static CDiscRepInfo thisInfo;
static string       strtmp;
static COutputConfig& oc = thisInfo.output_config;


//void CRepConfDiscrepancy :: Export()
void CRepConfig :: Export()
{
  oc.output_f << "Discrepancy Report Results\n\n"
       << "Summary\n";
  WriteDiscRepSummary();

  oc.output_f << "\n\nDetailed Report\n";
  WriteDiscRepDetails(thisInfo.disc_report_data, oc.use_flag);
};  // Discrepancy:: Export


bool CRepConfig :: RmTagInDescp(CRef <CClickableItem> c_item, const string& tag)
{
  string::size_type pos;

  pos = (c_item->description).find(tag);
  if (pos != string::npos && !pos) {
      c_item->description =  (c_item->description).substr(tag.size());
      return true;
  }
  else return ( false );
}


void CRepConfig :: WriteDiscRepSummary()
{
  string prefix;

  ITERATE (vector <CRef < CClickableItem > >, it, thisInfo.disc_report_data) {
      if ( (*it)->description.empty()) continue;
      // FATAL tag
      if (RmTagInDescp((*it), "FATAL: ")) oc.output_f  << "FATAL:";
      oc.output_f 
         << (*it)->setting_name << ": " << (*it)->description << endl;
      if ("SUSPECT_PRODUCT_NAMES" == (*it)->setting_name)
                            WriteDiscRepSubcategories((*it)->subcategories);
  }
  
} // WriteDiscrepancyReportSummary


void CRepConfig :: WriteDiscRepSubcategories(const vector <CRef <CClickableItem> >& subcategories)
{
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
  string prefix;
  unsigned i;
  NON_CONST_ITERATE (vector < CRef < CClickableItem > >, it, disc_rep_dt) {
      if ( (*it)->description.empty() ) continue;
      // prefix
      if (use_flag) {
          prefix = "DiscRep_";
          prefix += IsSubcategory ? "SUB:" : "ALL:";
          strtmp = (*it)->setting_name;
          prefix += strtmp.empty()? " " : strtmp + ": ";
      }
 
      // FATAL tag
      if (RmTagInDescp((*it), "FATAL: ")) prefix = "FATAL:" + prefix;

      // summary report
      if (oc.summary_report) {
         oc.output_f << prefix << (*it)->description << endl;
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
       if (!prefix.empty()) oc.output_f << prefix;
       oc.output_f << c_item->description << endl;
ITERATE (vector <string>, it, c_item->item_list) {
  oc.output_f << *it << endl;
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
       oc.output_f << endl;
*/
  } 
  else {
     StandardWriteDiscRepItems (oc, c_item, prefix, true);
  }
  if ((!c_item->next_sibling && c_item->subcategories.empty()) 
        || (!c_item->subcategories.empty() && !c_item->item_list.empty()) ) // for !OkToExpand
             oc.output_f << endl;
  
} // WriteDiscRepItems()


void CRepConfig :: StandardWriteDiscRepItems(COutputConfig& oc, const CClickableItem* c_item, const string& prefix, bool list_features_if_subcat)
{
  if (!prefix.empty())  oc.output_f << prefix;
  oc.output_f << c_item->description << endl;

  if (c_item->subcategories.empty() || list_features_if_subcat) {
          /*
           if (oc.use_feature_table_fmt)
           {
             list_copy = ReplaceDiscrepancyItemWithFeatureTableStrings (vnp);
             vnp = list_copy;
           }
        */
      ITERATE (vector <string>, it, c_item->item_list) oc.output_f << *it << endl;
  }

}; // StandardWriteDiscRepItems()
