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
 */

#include <ncbi_pch.hpp>
#include <objects/seq/Seq_ext.hpp>
#include <objects/seq/Seg_ext.hpp>
#include <objects/seq/seqport_util.hpp>
#include <objects/seq/Seq_data.hpp>
#include <objects/seq/Seqdesc.hpp>
#include <objects/seq/Pubdesc.hpp>
#include <objects/seq/Seq_ext.hpp>
#include <objects/seq/Delta_ext.hpp>
#include <objects/seq/Delta_seq.hpp>
#include <objects/seqfeat/PCRReactionSet.hpp>
#include <objects/seqfeat/PCRPrimerSet.hpp>
#include <objects/seqfeat/RNA_gen.hpp>
#include <objects/seqfeat/RNA_ref.hpp>
#include <objects/seqblock/GB_block.hpp>
#include <objects/general/Name_std.hpp>
#include <objects/general/Date.hpp>
#include <objects/general/Date_std.hpp>
#include <objects/general/Dbtag.hpp>
#include <objects/general/User_object.hpp>
#include <objects/general/User_field.hpp>
#include <objects/general/Object_id.hpp>
#include <objects/biblio/Author.hpp>
#include <objects/biblio/Cit_gen.hpp>
#include <objects/biblio/Cit_sub.hpp>
#include <objects/biblio/Cit_art.hpp>
#include <objects/biblio/Cit_book.hpp>
#include <objects/biblio/Cit_proc.hpp>
#include <objects/biblio/Cit_let.hpp>
#include <objects/biblio/Cit_pat.hpp>
#include <objects/biblio/citation_base.hpp>
#include <objects/biblio/Imprint.hpp>
#include <objects/pub/Pub_equiv.hpp>
#include <objmgr/annot_selector.hpp>
#include <objmgr/util/sequence.hpp>
#include <objmgr/util/seq_loc_util.hpp>
#include <objmgr/util/feature.hpp>
#include <objmgr/seq_vector_ci.hpp>
#include <objmgr/seq_vector.hpp>
#include <objmgr/seq_map.hpp>
#include <util/xregexp/regexp.hpp>

#include <objtools/discrepancy_report/hDiscRep_config.hpp>
#include <objtools/discrepancy_report/hDiscRep_tests.hpp>
#include <objtools/discrepancy_report/hUtilib.hpp>

BEGIN_NCBI_SCOPE

USING_NCBI_SCOPE;
USING_SCOPE(objects);
USING_SCOPE(DiscRepNmSpc);
USING_SCOPE(feature);


// definition
vector <const CSeq_feat*> CTestAndRepData :: mix_feat;
vector <const CSeq_feat*> CTestAndRepData :: cd_feat;
vector <const CSeq_feat*> CTestAndRepData :: rna_feat;
vector <const CSeq_feat*> CTestAndRepData :: rna_not_mrna_feat;
vector <const CSeq_feat*> CTestAndRepData :: gene_feat;
vector <const CSeq_feat*> CTestAndRepData :: prot_feat;
vector <const CSeq_feat*> CTestAndRepData :: pub_feat;
vector <const CSeq_feat*> CTestAndRepData :: biosrc_feat;
vector <const CSeq_feat*> CTestAndRepData :: biosrc_orgmod_feat;
vector <const CSeq_feat*> CTestAndRepData :: biosrc_subsrc_feat;
vector <const CSeq_feat*> CTestAndRepData :: rbs_feat;
vector <const CSeq_feat*> CTestAndRepData :: intron_feat;
vector <const CSeq_feat*> CTestAndRepData :: all_feat;
vector <const CSeq_feat*> CTestAndRepData :: non_prot_feat;
vector <const CSeq_feat*> CTestAndRepData :: rrna_feat;
vector <const CSeq_feat*> CTestAndRepData :: miscfeat_feat;
vector <const CSeq_feat*> CTestAndRepData :: otherRna_feat;
vector <const CSeq_feat*> CTestAndRepData :: exon_feat;
vector <const CSeq_feat*> CTestAndRepData :: utr5_feat;
vector <const CSeq_feat*> CTestAndRepData :: utr3_feat;
vector <const CSeq_feat*> CTestAndRepData :: promoter_feat;
vector <const CSeq_feat*> CTestAndRepData :: mrna_feat;
vector <const CSeq_feat*> CTestAndRepData :: trna_feat;
vector <const CSeq_feat*> CTestAndRepData :: bioseq_biosrc_feat;
vector <const CSeq_feat*> CTestAndRepData :: repeat_region_feat;
vector <const CSeq_feat*> CTestAndRepData :: D_loop_feat;
vector <const CSeq_feat*> CTestAndRepData :: org_orgmod_feat;
vector <const CSeq_feat*> CTestAndRepData :: gap_feat;

vector <const CSeqdesc*>  CTestAndRepData :: pub_seqdesc;
vector <const CSeqdesc*>  CTestAndRepData :: comm_seqdesc;
vector <const CSeqdesc*>  CTestAndRepData :: biosrc_seqdesc;
vector <const CSeqdesc*>  CTestAndRepData :: biosrc_orgmod_seqdesc;
vector <const CSeqdesc*>  CTestAndRepData :: biosrc_subsrc_seqdesc;
vector <const CSeqdesc*>  CTestAndRepData :: molinfo_seqdesc;
vector <const CSeqdesc*>  CTestAndRepData :: org_orgmod_seqdesc;
vector <const CSeqdesc*>  CTestAndRepData :: title_seqdesc;
vector <const CSeqdesc*>  CTestAndRepData :: user_seqdesc;
vector <const CSeqdesc*>  CTestAndRepData :: bioseq_biosrc_seqdesc;
vector <const CSeqdesc*>  CTestAndRepData :: bioseq_molinfo;
vector <const CSeqdesc*>  CTestAndRepData :: bioseq_title;
vector <const CSeqdesc*>  CTestAndRepData :: bioseq_user;
vector <const CSeqdesc*>  CTestAndRepData :: bioseq_genbank;

vector <const CSeq_entry*>  CTestAndRepData :: pub_seqdesc_seqentry;
vector <const CSeq_entry*>  CTestAndRepData :: comm_seqdesc_seqentry;
vector <const CSeq_entry*>  CTestAndRepData :: biosrc_seqdesc_seqentry;
vector <const CSeq_entry*>  CTestAndRepData :: biosrc_orgmod_seqdesc_seqentry;
vector <const CSeq_entry*>  CTestAndRepData :: biosrc_subsrc_seqdesc_seqentry;
vector <const CSeq_entry*>  CTestAndRepData :: title_seqdesc_seqentry;
vector <const CSeq_entry*>  CTestAndRepData :: user_seqdesc_seqentry;
vector <const CSeq_entry*>  CTestAndRepData :: molinfo_seqdesc_seqentry;
vector <const CSeq_entry*>  CTestAndRepData :: org_orgmod_seqdesc_seqentry;

static CDiscRepInfo thisInfo;
static string strtmp;
static CSuspectRuleCheck rule_check;

// CTestAndRepData
string CTestAndRepData :: x_GetUserObjType(const CUser_object& user_obj)
{
  return (user_obj.GetType().IsStr() ? user_obj.GetType().GetStr() : kEmptyStr);
};

bool CTestAndRepData :: AllCapitalLetters(const string& pattern, const string& search)
{ 
  if (search.empty()) return false;
  if (search.find_first_of(CDiscRepUtil::alpha_str) == string::npos) return false;
  return (CDiscRepUtil::IsAllCaps(search));
}

bool CTestAndRepData :: BeginsWithPunct(const string& pattern, const string& search)
{ 
  if (search.empty()) return false;
  if (search[0] == '.' || search[0] == ',' || search[0] == '-' || search[0] == '_' 
                                                  || search[0] == ':' || search[0] == '/') 
    return true;
  else return false;
}

bool CTestAndRepData :: BeginsOrEndsWithQuotes(const string& pattern, const string& search)
{ 
  if (search.empty()) return false;
  if (search[0] == '\'' || search[0] == '"') return true;
  else {
    unsigned len = search.size();
    if ((len > 1) && (search[len - 1] == '\'' || search[len - 1] == '"')) 
      return true;
    else return false;
  }
}

bool CTestAndRepData :: ContainsDoubleSpace(const string& pattern, const string& search)
{
  if (search.empty()) return false;
  if (search.find("  ") != string::npos) return true;
  else return false;
}

bool CTestAndRepData :: ContainsPseudo(const string& pattern, const list <string>& strs)
{
  bool right_pseudo;
  size_t pos;
  ITERATE (list <string>, it, strs) {
    strtmp = *it;
    while ( !strtmp.empty() && (pos = strtmp.find("pseudo")) != string::npos) {
        right_pseudo = true;
        ITERATE (vector <string>, jt, thisInfo.s_pseudoweasels) {
           if ( CTempString(strtmp).substr(pos, (*jt).size()) == *jt) {
               right_pseudo = false;
               strtmp = CTempString(strtmp).substr(pos + (*jt).size());
               break;
           }
        }
        if (right_pseudo) return true;
    }
  };
  return false;
};


bool CTestAndRepData :: ContainsTwoSetsOfBracketsOrParentheses(const string& pattern, const string& search)
{ 
   return rule_check.ContainsNorMoreSetsOfBracketsOrParentheses (search, 2);
}

bool CTestAndRepData :: ContainsWholeWord(const string& pattern, const string& search)
{
  if (DoesStringContainPhrase(search, pattern, false, true)) return true;
  else return false;
}

bool CTestAndRepData :: ContainsWholeWord(const string& pattern, const list <string>& strs)
{
   ITERATE (list <string>, it, strs) {
      if (ContainsWholeWord(pattern, *it)) return true;
   }
   return false;
};

bool CTestAndRepData :: ContainsWholeWordCaseSensitive(const string& pattern, const string& search)
{ 
  if (DoesStringContainPhrase (search, pattern, true, true)) return true;
  else return false;
}

bool CTestAndRepData :: ContainsUnbalancedParentheses(const string& pattern, const string& search)
{ 
   return rule_check.StringContainsUnbalancedParentheses(search);
}

bool CTestAndRepData :: ContainsUnderscore(const string& pattern, const string& search)
{ 
   return rule_check.StringContainsUnderscore(search);
}

bool CTestAndRepData :: ContainsUnknownName(const string& pattern, const string& search)
{
  unsigned len = search.size();
  if (NStr::FindNoCase(search, pattern, 0, len) != string::npos
           && NStr::FindNoCase(search, "protein of unknown function", 0, len) == string::npos
           && NStr::FindNoCase(search, "domain of unknown function", 0, len) == string::npos)
    return true;
  else return false;
}

bool CTestAndRepData :: EndsWithFold(const string& pattern, const string& search)
{ 
  if (search.empty()) return false;
  unsigned len = search.size();
  if (len < 4) return false;
  strtmp = CTempString(search).substr(len-5);
  if (NStr::EqualNocase(strtmp, "fold")) {
    if (strtmp == "folD" || strtmp == "FolD") return false;
    else return true;
  }
  else return false;
}

bool CTestAndRepData :: EndsWithPattern(const string& pattern, const string& search)
{ 
  unsigned phrase_len = pattern.size();
  unsigned len = search.size();

  if (len >= phrase_len 
              && NStr::EqualNocase(CTempString(search).substr(len - phrase_len), pattern))
      return true;
  else return false;
}

bool CTestAndRepData :: EndsWithPunct(const string& pattern, const string& search)
{
  unsigned len = search.size();
  char last_ch = search[len - 1];
  if (last_ch == '.' || last_ch == ',' || last_ch == '-' || last_ch == '_' 
                                                       || last_ch == ':' || last_ch == '/')
    return true;
  else return false;
}

bool CTestAndRepData :: IsSingleWord(const string& pattern, const string& search)
{
  if (NStr::EqualNocase(search, pattern)) return true;
  else return false;
}

bool CTestAndRepData :: IsSingleWordOrWeaselPlusSingleWord(const string& pattern, const string& search)
{
  unsigned len;
  if (NStr::EqualNocase(search, pattern)) return true;
  else {
    ITERATE (vector <string>, it, thisInfo.weasels) {
       len = (*it).size();
       if (NStr::EqualNocase(search, 0, len, *it)
              && search.find(" ", len) != string::npos
              && CTempString(search).substr(search.find(" ", len)) == pattern) return true;    
    }
    return false;
  }
}

bool CTestAndRepData :: IsTooLong(const string& pattern, const string& search)
{ 
  if (NStr::FindNoCase(search, "bifunctional") != string::npos 
                         || NStr::FindNoCase(search, "multifunctional") != string::npos)
       return false;
  else if (search.size() > 100) return true;
  else return false;
}

bool CTestAndRepData :: MayContainPlural(const string& pattern, const string& search)
{ 
    return rule_check.StringMayContainPlural (search);
}

bool CTestAndRepData :: NormalSearch(const string& pattern, const string& search)
{
  if (NStr::FindNoCase(search, pattern) != string::npos) return true;
  else return false;
}

bool CTestAndRepData :: PrefixPlusNumbersOnly(const string& pattern, const string& search)
{
   return rule_check.IsPrefixPlusNumbers (pattern, search);
}

bool CTestAndRepData :: ProductContainsTerm(const string& pattern, const string& search)
{ 
   return rule_check.ProductContainsTerm( pattern, search);
}

bool CTestAndRepData :: StartsWithPattern(const string& pattern, const string& search)
{
  unsigned phrase_len = pattern.size();
  unsigned len = search.size();

  if (len >= phrase_len && NStr::EqualNocase(search,0, phrase_len, pattern)) return true;
  else return false;
}

bool CTestAndRepData :: StartsWithPutativeReplacement(const string& pattern, const string& search)
{
  ITERATE (vector <string>, it, thisInfo.s_putative_replacements) {
     if (StartsWithPattern( *it, search)) return true;
  }
  return false;
}

bool CTestAndRepData :: ThreeOrMoreNumbersTogether(const string& pattern, const string& search)
{
   return rule_check.ContainsThreeOrMoreNumbersTogether (search);
}

bool CTestAndRepData :: AllVecElesSame(const vector <string> arr)
{
    if (arr.size() > 1)
        ITERATE (vector <string>, it, arr) if ( arr[0] != *it ) return false;

    return true;
};

string CTestAndRepData :: Get1OrgModValue(const CBioSource& biosrc, const string& type_name)
{
   vector <string> strs;
   GetOrgModValues(biosrc, type_name, strs);
   if (strs.empty()) return kEmptyStr;
   else return strs[0];
};


void CTestAndRepData :: GetOrgModValues(const COrg_ref& org, COrgMod::ESubtype subtype, vector <string>& strs)
{
   if (!org.IsSetOrgMod()) return;
   ITERATE (list <CRef <COrgMod> >, it, org.GetOrgname().GetMod() )
      if ((*it)->GetSubtype() == subtype) strs.push_back((*it)->GetSubname());
};

void CTestAndRepData :: GetOrgModValues(const CBioSource& biosrc, const string& type_name, vector <string>& strs)
{
   GetOrgModValues(biosrc.GetOrg(), 
    (COrgMod::ESubtype)COrgMod::GetSubtypeValue(type_name, COrgMod::eVocabulary_insdc),
     strs);
/*
   ITERATE (list <CRef <COrgMod> >, it, biosrc.GetOrgname().GetMod() )
      if ((*it)->GetSubtypeName((*it)->GetSubtype(), COrgMod::eVocabulary_insdc) == type_name)
                return ((*it)->GetSubname());
   return (kEmptyStr);
*/
};


void CTestAndRepData :: GetOrgModValues(const CBioSource& biosrc, COrgMod::ESubtype subtype, vector <string>& strs)
{
    GetOrgModValues(biosrc.GetOrg(), subtype, strs);
};

bool CTestAndRepData :: IsOrgModPresent(const CBioSource& biosrc, COrgMod::ESubtype subtype)
{
   if (biosrc.IsSetOrgMod()) {
     ITERATE (list <CRef <COrgMod> >, it, biosrc.GetOrgname().GetMod() )
        if ((*it)->GetSubtype() == subtype) return true;
   }
   return false;
};

string CTestAndRepData :: Get1SubSrcValue(const CBioSource& biosrc, const string& type_name)
{
   vector <string> strs;
   GetSubSrcValues(biosrc, type_name, strs);
   if (strs.empty()) return kEmptyStr;
   else return strs[0];
};

void CTestAndRepData :: GetSubSrcValues(const CBioSource& biosrc, CSubSource::ESubtype subtype, vector <string>& strs)
{
   if (!biosrc.CanGetSubtype()) return;
   GetSubSrcValues (biosrc, 
       CSubSource::GetSubtypeName(subtype, CSubSource::eVocabulary_insdc),
       strs);
};

void CTestAndRepData :: GetSubSrcValues(const CBioSource& biosrc, const string& type_name, vector <string>& strs)
{
  if (!biosrc.CanGetSubtype()) return;
  ITERATE (list <CRef <CSubSource> >, it, biosrc.GetSubtype()) {
     int type = (*it)->GetSubtype();
     if ( (*it)->GetSubtypeName( type, CSubSource::eVocabulary_insdc) == type_name ) {
        strtmp = (*it)->GetName();
        if (strtmp.empty() && (type == CSubSource::eSubtype_germline
                                 || type == CSubSource::eSubtype_transgenic
                                 || type == CSubSource::eSubtype_metagenomic
                                 || type == CSubSource::eSubtype_environmental_sample
                                 || type == CSubSource::eSubtype_rearranged)) {
           strs.push_back("TRUE");
        }
        else strs.push_back(strtmp);
     }
  }
};


bool CTestAndRepData :: IsSubSrcPresent(const CBioSource& biosrc, CSubSource::ESubtype subtype)
{
  if (biosrc.CanGetSubtype()) {
     ITERATE (list <CRef <CSubSource> >, it, biosrc.GetSubtype())
       if ( (*it)->GetSubtype() == subtype) return true;
  }
  return false;
};


bool CTestAndRepData :: CommentHasPhrase(string comment, const string& phrase)
{
  vector <string> arr;
  arr = NStr::Tokenize(comment, ";", arr);
  for (unsigned i=0; i< arr.size(); i++) {
     NStr::TruncateSpacesInPlace(arr[i], NStr::eTrunc_Begin);
     if (NStr::EqualNocase(arr[i], phrase)) return true;
  }
  return false;
};

string CTestAndRepData :: FindReplaceString(const string& src, const string& search_str, const string& replacement_str, bool case_sensitive, bool whole_word)
{
    CRegexpUtil rxu(src);
    string pattern (CRegexp::Escape(search_str));

    // word boundaries: whitespace & punctuation
    pattern = whole_word ? ("\\b" + pattern + "\\b") : pattern;
/*
    pattern = 
          whole_word ? ("(?:^|(?<=[[:^alnum:]]))" + pattern + "(?:^|(?<=[[:^alnum:]]))") 
                         : pattern;
          // include underscores
*/

    CRegexp::ECompile comp_flag = case_sensitive? 
                               CRegexp::fCompile_default : CRegexp::fCompile_ignore_case;
    rxu.Replace(pattern, replacement_str, comp_flag);
    return (rxu.GetResult());
};

bool CTestAndRepData :: DoesStringContainPhrase(const string& str, const string& phrase, bool case_sensitive, bool whole_word)
{
  if (str.empty()) return false;
  string pattern(CRegexp::Escape(phrase));
  pattern = whole_word ? ("\\b" + pattern + "\\b") : pattern;
  CRegexp::ECompile comp_flag = case_sensitive ?
                            CRegexp::fCompile_default : CRegexp::fCompile_ignore_case;
  CRegexp rx(pattern, comp_flag);
  if (rx.IsMatch(str)) return true;
  else return false;
};


bool CTestAndRepData :: DoesStringContainPhrase(const string& str, const vector <string>& phrases, bool case_sensitive, bool whole_word)
{
   ITERATE (vector <string>, it, phrases) {
      if (DoesStringContainPhrase(str, *it, case_sensitive, whole_word)) return true;   
   }
   return false;
};


CConstRef <CSeq_feat> CTestAndRepData :: GetGeneForFeature(const CSeq_feat& seq_feat)
{
  const CGene_ref* gene = seq_feat.GetGeneXref();
  if (gene && gene->IsSuppressed()) {
    return (CConstRef <CSeq_feat>());
  }

  if (gene) {
     CBioseq_Handle 
       bioseq_hl =  sequence::GetBioseqFromSeqLoc(seq_feat.GetLocation(), 
                                                  *thisInfo.scope);
     if (!bioseq_hl) return (CConstRef <CSeq_feat> ());
     CTSE_Handle tse_hl = bioseq_hl.GetTSE_Handle();
     if (gene->CanGetLocus_tag() && !(gene->GetLocus_tag().empty()) ) {
         CSeq_feat_Handle seq_feat_hl = tse_hl.GetGeneWithLocus(gene->GetLocus_tag(), true);
         if (seq_feat_hl) return (seq_feat_hl.GetOriginalSeq_feat());
     }
     else if (gene->CanGetLocus() && !(gene->GetLocus().empty())) {
         CSeq_feat_Handle seq_feat_hl = tse_hl.GetGeneWithLocus(gene->GetLocus(), false);
         if (seq_feat_hl) return (seq_feat_hl.GetOriginalSeq_feat());
     }
     else return (CConstRef <CSeq_feat>());
  }
  else {
    return( CConstRef <CSeq_feat>(GetBestOverlappingFeat(seq_feat.GetLocation(),
                                                   CSeqFeatData::e_Gene,
                                                   sequence::eOverlap_Contained,
                                                   *thisInfo.scope)));
  }

  return (CConstRef <CSeq_feat>());
};


void CTestAndRepData :: RmvRedundancy(vector <string>& item_list)
{
  vector <string> tmp = item_list;
  item_list.clear();
  strtmp = "";
  ITERATE (vector <string>, it, tmp) {
    if (strtmp.find(*it) == string::npos) {
      item_list.push_back(*it);
      strtmp += *it + ",";
    } 
  }
};



void CTestAndRepData :: GetProperCItem(CRef <CClickableItem>& c_item, bool* citem1_used)
{
   if (*citem1_used) {
     c_item = CRef <CClickableItem> (new CClickableItem);
     thisInfo.disc_report_data.push_back(c_item);
   }
   else {
      c_item->item_list.clear();
      *citem1_used = true;
   }
};



bool CTestAndRepData :: HasLineage(const CBioSource& biosrc, const string& type)
{
   if (thisInfo.report_lineage.find(type) != string::npos) return true;
   else if (thisInfo.report_lineage.empty() && biosrc.IsSetLineage()
                && biosrc.GetLineage().find(type) != string::npos)
          return true;
   else return false;
};


bool CTestAndRepData :: IsBiosrcEukaryotic(const CBioSource& biosrc)
{
   CBioSource :: EGenome genome = (CBioSource::EGenome) biosrc.GetGenome();
   if (genome != CBioSource :: eGenome_mitochondrion
                  && genome != CBioSource :: eGenome_chloroplast
                  && genome != CBioSource :: eGenome_plastid
                  && genome != CBioSource :: eGenome_apicoplast
                  && HasLineage(biosrc, "Eukaryota"))
         return true; 
   else return false;
};


bool CTestAndRepData :: IsBioseqHasLineage(const CBioseq& bioseq, const string& type, bool has_biosrc)
{
   CBioseq_Handle bioseq_handle = thisInfo.scope->GetBioseqHandle(bioseq);
   if (!bioseq_handle) return false;
   CSeqdesc_CI it(bioseq_handle, CSeqdesc :: e_Source);
   if (!it) return false;
   if (type == "Eukaryota") {
      if (has_biosrc) {
         ITERATE (vector <const CSeqdesc*>, it, bioseq_biosrc_seqdesc) 
             if (IsBiosrcEukaryotic( (*it)->GetSource())) return true;            
         return false;
      }
      else {
         for (CSeqdesc_CI it(bioseq_handle, CSeqdesc :: e_Source); it; ++it) {
               if (IsBiosrcEukaryotic(it->GetSource())) return true;
         }
         return false;
      }
   } 
   else if (type == "Bacteria") {
      if (has_biosrc) { 
          ITERATE (vector <const CSeqdesc*>, it, bioseq_biosrc_seqdesc)
              if (HasLineage( (*it)->GetSource(), type)) return true;
          return false;
      }
      else {
         for (CSeqdesc_CI it(bioseq_handle, CSeqdesc :: e_Source); it; ++it) 
             if (HasLineage(it->GetSource(), type)) return true;
         return false;
      }
   }
   else return false;
};


bool CTestAndRepData :: HasTaxonomyID(const CBioSource& biosrc)
{
   ITERATE (vector <CRef <CDbtag> >, it, biosrc.GetOrg().GetDb())
       if ((*it)->GetType() == CDbtag::eDbtagType_taxon) return true;
   return false;
};

bool CDiscRepUtil :: IsAllCaps(const string& str)  // may include # etc non-character
{
  string up_str = str;
  up_str = NStr::ToUpper(up_str);
  if (up_str == str) return true;
  else return false;
};



bool CDiscRepUtil :: IsAllLowerCase(const string& str)
{
  string low_str = str;
  low_str = NStr::ToLower(low_str);
  if (low_str == str) return true;
  else return false;
}


bool CDiscRepUtil :: IsAllPunctuation(const string& str)
{
   for (unsigned i=0; i< str.size(); i++) {
     if (!ispunct(str[i])) return false;
   }
   return true;
};


bool CDiscRepUtil :: IsWholeWord(const string& str, const string& phrase)
{
  if (str == phrase) return true;
  size_t pos = str.find(phrase);
  if (string::npos == pos) return false;
  size_t pos2 = pos+phrase.size();
  if ((!pos || !isalnum(str[pos-1]))
        && ((str.size() == pos2) || !isalnum(str[pos2]))) return true;
  else return false;
  
}; //CTestAndRepData



string CTestAndRepData :: GetProdNmForCD(const CSeq_feat& cd_feat)
{
     // should be the longest CProt_ref on the bioseq pointed by the cds.GetProduct()
     CConstRef <CSeq_feat> prot_seq_feat;
     if (cd_feat.CanGetProduct()) {
         prot_seq_feat 
             = CConstRef <CSeq_feat> (
                      sequence::GetBestOverlappingFeat(cd_feat.GetProduct(),
                                             CSeqFeatData::e_Prot, 
                                             sequence::eOverlap_Contains,
                                             *thisInfo.scope));
     }
     if (prot_seq_feat.NotEmpty() && prot_seq_feat->GetData().IsProt()) {
         const CProt_ref& prot = prot_seq_feat->GetData().GetProt();
         if (prot.CanGetName() && !prot.GetName().empty()) {
              return( *(prot.GetName().begin()) );
         }
     }
     else {
        const CProt_ref* prot = cd_feat.GetProtXref();
        if (prot && prot->CanGetName() && !prot->GetName().empty()) {
              return ( *(prot->GetName().begin()) );
        }
     }
     return (kEmptyStr);

/*
   if (seq_feat.CanGetProduct()) {
       const CSeq_loc& cds_product = seq_feat.GetProduct();
       for (CFeat_CI feat_it(*thisInfo.scope, cds_product); feat_it; ++feat_it)
       {
            CMappedFeat feat = *feat_it;
            const CSeq_feat& prot_seq_feat = feat.GetOriginalFeature();
            if (prot_seq_feat.GetData().IsProt()) {
                const CProt_ref& prot = prot_seq_feat.GetData().GetProt();
                if (prot.CanGetName()) {
                    const list <string>& prot_nms = prot.GetName();
                    return ( *(prot_nms.begin()) );
                }
            }
       }
   }
*/
};



bool CTestAndRepData :: IsProdBiomol(const CBioseq& bioseq)
{
  CConstRef<CSeqdesc> seq_desc = bioseq.GetClosestDescriptor(CSeqdesc::e_Molinfo);
  switch (seq_desc->GetMolinfo().GetBiomol()) {
    case CMolInfo::eBiomol_mRNA:
    case CMolInfo::eBiomol_ncRNA:
    case CMolInfo::eBiomol_rRNA:
    case CMolInfo::eBiomol_pre_RNA: 
    case CMolInfo::eBiomol_tRNA:
           return true; 
    default : return false;
  }
}



bool CTestAndRepData :: IsmRNASequenceInGenProdSet(const CBioseq& bioseq)
{
   bool rval = false;

   CConstRef<CBioseq_set> bioseq_set = bioseq.GetParentSet();

   if (CSeq_inst::eMol_rna == bioseq.GetInst().GetMol() && bioseq_set.NotEmpty() ) {
        if (CBioseq_set::eClass_gen_prod_set == bioseq_set->GetClass()) 
             rval = IsProdBiomol(bioseq);
        else if (CBioseq_set::eClass_nuc_prot == bioseq_set->GetClass()) {
             CConstRef<CBioseq_set> bioseq_set_set = bioseq_set->GetParentSet();
             if (bioseq_set_set.NotEmpty())
                if (CBioseq_set::eClass_gen_prod_set == bioseq_set_set->GetClass())
                    rval = IsProdBiomol(bioseq);
        }
   }
   return rval;
};


/*
string GetKey(CSeqdesc::E_Choice idx) 
{
  vector <string> descrs;
  descrs.reserve(25);
  descrs.push_back("SeqDesc..");
  descrs.push_back("MolType");
  descrs.push_back("Modifier");
  descrs.push_back("Method");
  descrs.push_back("Name");
  descrs.push_back("Title");
  descrs.push_back("Organism");
  descrs.push_back("Comment");
  descrs.push_back("Numbering");
  descrs.push_back("MapLocation");
  descrs.push_back("PIR");
  descrs.push_back("Genbank");
  descrs.push_back("Pub");
  descrs.push_back("Region");
  descrs.push_back("UserObj");
  descrs.push_back("SWISSPROT");
  descrs.push_back("DbXref");
  descrs.push_back("EMBL");
  descrs.push_back("CreateDate");
  descrs.push_back("UpdateDate");
  descrs.push_back("PRF");
  descrs.push_back("PDB");
  descrs.push_back("Heterogen");
  descrs.push_back("BioSrc");
  descrs.push_back("MolInfo");

  return (descrs[idx]);
};
*/


string CTestAndRepData :: GetDiscItemText(const CSeq_entry& seq_entry)
{
   string desc;
   if (seq_entry.IsSeq())
        desc =  BioseqToBestSeqIdString(seq_entry.GetSeq(), CSeq_id::e_Genbank);
   else desc = GetDiscItemTextForBioseqSet(seq_entry.GetSet());
   if (thisInfo.infile.empty()) return desc;
   else return (thisInfo.infile + ": " + desc);
};



string CTestAndRepData :: GetDiscItemText(const CSeq_submit& seq_submit)
{
  string desc;
  if (seq_submit.GetData().IsEntrys()) {
     const list <CRef <CSeq_entry> >& entrys = seq_submit.GetData().GetEntrys();
     if (!entrys.empty()) {
        if ((*entrys.begin())->IsSeq()) {
            desc =  BioseqToBestSeqIdString((*entrys.begin())->GetSeq(), 
                                             CSeq_id::e_Genbank);
        }
        else {
          desc = GetDiscItemTextForBioseqSet((*entrys.begin())->GetSet());
        }
     }
  }
  if (thisInfo.infile.empty()) {
     return desc;
  }
  else {
     return (thisInfo.infile + ": " + desc);
  }
};


string CTestAndRepData :: GetDiscItemTextForBioseqSet(const CBioseq_set& bioseq_set)
{
  string     row_text(kEmptyStr);

  if (bioseq_set.GetClass() == CBioseq_set::eClass_segset) {
    row_text = "ss|" 
                  + BioseqToBestSeqIdString(bioseq_set.GetMasterFromSegSet(), 
                                            CSeq_id::e_Genbank);
  } else if (bioseq_set.GetClass() == CBioseq_set::eClass_nuc_prot) {
    row_text = "np|" 
                 + BioseqToBestSeqIdString(bioseq_set.GetNucFromNucProtSet(), 
                                           CSeq_id::e_Genbank);
  } else {
    const list < CRef < CSeq_entry > >& seq_entrys = bioseq_set.GetSeq_set();
    if (seq_entrys.empty()) {
       if ( (*(seq_entrys.begin()))->IsSeq()) {
         row_text = "Set containing "  
                 + BioseqToBestSeqIdString((*(seq_entrys.begin()))->GetSeq(), 
                                           CSeq_id::e_Genbank);
       }
       else if ( (*(seq_entrys.begin()))->IsSet()) {
          row_text = "Set containing " 
              + GetDiscItemTextForBioseqSet((*(seq_entrys.begin()))->GetSet());
       } 
       else row_text =  "BioseqSet";
    }
  }

  return (row_text);

}; // GetDiscItemTextForBioseqSet(Bioseq_seq)



string CTestAndRepData :: GetDiscItemText(const CBioseq_set& bioseq_set)
{
   string     row_text;
   row_text = GetDiscItemTextForBioseqSet(bioseq_set);

   if (row_text.empty()) return(kEmptyStr);
   else {
      if (thisInfo.infile.empty()) return row_text;
      else return (thisInfo.infile + ": " + row_text);
   }
};

string CTestAndRepData :: ListAuthNames(const CAuth_list& auths)
{
  string auth_nms(kEmptyStr);

  unsigned i = 0;
  if (auths.GetNames().IsStd()) {
    ITERATE(list < CRef <CAuthor> >, it, auths.GetNames().GetStd()) {
       strtmp = kEmptyStr;
       (*it)->GetLabel(&strtmp, IAbstractCitation::fLabel_Unique);
       if ( (*it)->GetName().IsName()) {
          if ( (*it)->GetName().GetName().CanGetFirst()) {
            size_t pos = strtmp.find(",");
            strtmp = strtmp.substr(0, pos+1) + (*it)->GetName().GetName().GetFirst()
                   + "," + CTempString(strtmp).substr(pos+1);
          }
          auth_nms += (!(i++)) ? strtmp  : ( " & " + strtmp);
       }
    }
  }
  return (auth_nms);
};  // ListAuthNames




string CTestAndRepData :: ListAllAuths(const CPubdesc& pubdesc)
{
   string auth_nms(kEmptyStr);

   ITERATE (list <CRef <CPub> > , it, pubdesc.GetPub().Get()) {
      if (!(auth_nms.empty())) auth_nms += " & ";
      if ((*it)->IsSetAuthors() )
           auth_nms += ListAuthNames( (*it)->GetAuthors() );
  }

  return (auth_nms);
};  // ListAllAuths



CBioseq* CTestAndRepData :: GetRepresentativeBioseqFromBioseqSet(const CBioseq_set& bioseq_set)
{
   if (bioseq_set.GetClass() == CBioseq_set::eClass_nuc_prot)
       return ( const_cast<CBioseq*>(&(bioseq_set.GetNucFromNucProtSet())));
   else if (bioseq_set.GetClass() == CBioseq_set::eClass_segset)
       return ( const_cast<CBioseq*>(&(bioseq_set.GetMasterFromSegSet())));
   else {
/*???
       bool has_set = false;
       ITERATE (list <CRef <CSeq_entry> >, it, bioseq_set.GetSeq_set()) {   
          if ((*it)->IsSet()) {
                has_set = true;
                return ( GetRepresentativeBioseqFromBioseqSet ( (*it)->GetSet() ) );
          }
       }
       if (!has_set) return 0;
*/
return 0;
   }
};


string CTestAndRepData :: SeqDescLabelContent(const CSeqdesc& sd)
{
    string label(kEmptyStr);
    switch (sd.Which()) 
    {
        case CSeqdesc::e_Comment: return (sd.GetComment());
        case CSeqdesc::e_Region: return (sd.GetRegion());
        case CSeqdesc::e_Het: return (sd.GetHet());
        case CSeqdesc::e_Title: return (sd.GetTitle());
        case CSeqdesc::e_Name: return (sd.GetName());
        case CSeqdesc::e_Create_date:
              sd.GetCreate_date().GetDate(&label);
              return label;
        case CSeqdesc::e_Update_date:
              sd.GetUpdate_date().GetDate(&label);
              return label;
        case CSeqdesc::e_Org:
          {
            const COrg_ref& org = sd.GetOrg();
            return (org.CanGetTaxname() ? org.GetTaxname() 
                                          : (org.CanGetCommon()? org.GetCommon(): kEmptyStr));
          }
        case CSeqdesc::e_Pub:
            sd.GetPub().GetPub().GetLabel(&label);
            return label;
        case CSeqdesc::e_User:
          {
            const CUser_object& uop = sd.GetUser();
            return (uop.CanGetClass()? uop.GetClass() 
                                 : (uop.GetType().IsStr()? uop.GetType().GetStr(): kEmptyStr));
          }
        case CSeqdesc::e_Method:
              return (ENUM_METHOD_NAME(EGIBB_method)()->FindName(sd.GetMethod(), true));
        case CSeqdesc::e_Mol_type:
              return (ENUM_METHOD_NAME(EGIBB_mol)()->FindName(sd.GetMol_type(), true));
        case CSeqdesc::e_Modif:
              ITERATE (list <EGIBB_mod>, it, sd.GetModif()) {
                label += ENUM_METHOD_NAME(EGIBB_mod)()->FindName(*it, true) + ", ";
              }
              return (label.substr(0, label.size()-2));
        case CSeqdesc::e_Source:
          {
            const COrg_ref& org = sd.GetSource().GetOrg();
            return (org.CanGetTaxname() ? org.GetTaxname() 
                                          : (org.CanGetCommon()? org.GetCommon(): kEmptyStr));
          }  
        case CSeqdesc::e_Maploc:
              sd.GetMaploc().GetLabel(&label);
              return label; 
        case CSeqdesc::e_Molinfo:
              sd.GetMolinfo().GetLabel(&label);
              return label;
        case CSeqdesc::e_Modelev:
              return kEmptyStr;

        case CSeqdesc::e_Dbxref:
              sd.GetDbxref().GetLabel(&label);
              return label;
        default:
              return kEmptyStr; 
    }
};

string CTestAndRepData :: GetDiscItemText(const CSeqdesc& seqdesc, const CBioseq& bioseq)
{
    // what if no bioseq?
    string row_text = BioseqToBestSeqIdString(bioseq, CSeq_id::e_Genbank) + ": ";
    row_text += SeqDescLabelContent(seqdesc);
    if (thisInfo.infile.empty()) return row_text;
    else return(thisInfo.infile + ": " + row_text);
};



string CTestAndRepData :: GetDiscItemText(const CSeqdesc& seqdesc, const CSeq_entry& entry)
{
  string row_text(kEmptyStr);

  CBioseq* bioseq = 0;

  if (entry.IsSeq()) bioseq = const_cast<CBioseq*>(&(entry.GetSeq()));
  else {
    const CBioseq_set& bioseq_set = entry.GetSet();
    bioseq = GetRepresentativeBioseqFromBioseqSet (bioseq_set);
  }

  if (!bioseq) {
     if (seqdesc.IsTitle()) row_text = seqdesc.GetTitle();
     else if (seqdesc.IsComment()) row_text = seqdesc.GetComment();
     else if (seqdesc.IsPub()) {
        string pub_label;
        if ( seqdesc.GetPub().GetPub().GetLabel(&pub_label)) 
            row_text += pub_label;
     }
     else seqdesc.GetLabel(&row_text, CSeqdesc::eContent);
     if (thisInfo.infile.empty()) return row_text;
     else return(thisInfo.infile + ": " + row_text);
  }
  else return(GetDiscItemText(seqdesc, *bioseq));
}; // GetDiscItemText(CSeqdesc)



string CTestAndRepData :: GetDiscItemText(const CBioseq& bioseq)
{
      string len_txt = " (length " + NStr::IntToString(bioseq.GetLength());
      CSeq_data out_seq;
      vector <unsigned> idx;
      CBioseq_Handle bioseq_handle = thisInfo.scope->GetBioseqHandle(bioseq);
      if (!bioseq_handle) return kEmptyStr;
      unsigned ambigs_cnt = 0, gap_cnt = 0;
      SSeqMapSelector sel(CSeqMap::fDefaultFlags);
      if (bioseq.GetInst().CanGetSeq_data()) {   // is this necessary?
        for (CSeqMap_CI seqmap_ci(bioseq_handle, sel); seqmap_ci;  ++seqmap_ci) {
            if (seqmap_ci.GetType() == CSeqMap::eSeqGap) gap_cnt += seqmap_ci.GetLength();
            else ambigs_cnt += CSeqportUtil::GetAmbigs(seqmap_ci.GetRefData(),
                                                    &out_seq, &idx,
                                                    CSeq_data::e_Ncbi2na,
                                                    seqmap_ci.GetRefPosition(),
                                                    seqmap_ci.GetLength());
        }
      }
      if (bioseq.IsNa()) {
          if (ambigs_cnt >0)
             len_txt += ", " + NStr::UIntToString(ambigs_cnt) + " other"; 
      }
      if (gap_cnt >0)
             len_txt += ", " + NStr::UIntToString(gap_cnt) + " gap"; 
      len_txt += ")";

/*
      const CSeq_id& seq_id = BioseqToBestSeqId(bioseq, CSeq_id::e_Genbank);
      return (thisInfo.infile + ": " + seq_id.GetSeqIdString() + len_txt);
*/
      if (thisInfo.infile.empty())
         return (BioseqToBestSeqIdString(bioseq,CSeq_id::e_Genbank)+ len_txt);
      else return(thisInfo.infile +": " 
                              + BioseqToBestSeqIdString(bioseq,CSeq_id::e_Genbank)+ len_txt);

}; // GetDiscItemText(const CBioseq& obj)


const CSeq_id& CTestAndRepData::BioseqToBestSeqId(const CBioseq& bioseq, CSeq_id :: E_Choice choice )
{
      const CBioseq::TId& seq_id_ls = bioseq.GetId();
      CBioseq::TId::const_iterator best_seq_id;
      int best_score = 99999;
      ITERATE (CBioseq::TId, it, seq_id_ls) {
        if ( choice != (*it)->Which() ) {
           if (best_score > (*it)->BaseBestRankScore()) {
               best_seq_id = it;
               best_score = (*it)->BaseBestRankScore();
           }
        }
        else  return ( (*it).GetObject() );
      };
      if ( best_score == 99999 ) 
           NCBI_THROW(CException, eUnknown, "BioseqToBestSeqId failed");
      else return ( (*best_seq_id).GetObject() );  
}; // BioseqToBestSeqId()


string CTestAndRepData :: BioseqToBestSeqIdString(const CBioseq& bioseq, CSeq_id :: E_Choice choice )
{
  const CSeq_id& seq_id = BioseqToBestSeqId(bioseq, CSeq_id::e_Genbank);
  return (seq_id.GetSeqIdString());

}; // BioseqToBestSeqIdString();


string CTestAndRepData :: PrintSeqInt(const CSeq_interval& seq_int, bool range_only)
{
    string location;

    // Best seq_id
    if (!range_only) {
      const CSeq_id& seq_id = seq_int.GetId();
      if (!(seq_id.IsGenbank()) ) {
         CBioseq_Handle bioseq_hl = thisInfo.scope->GetBioseqHandle(seq_id);
         if (bioseq_hl) {
            const CSeq_id& new_seq_id = 
                BioseqToBestSeqId(*(bioseq_hl.GetCompleteBioseq()), CSeq_id::e_Genbank);
            location = new_seq_id.AsFastaString();
         }
         else location = seq_id.AsFastaString();  // should be impossible
      }
      else location = seq_id.AsFastaString();
      location += ":";
    }

    // strand
    if (seq_int.CanGetStrand()) {
      ENa_strand this_strand = seq_int.GetStrand();
      location += thisInfo.strandsymbol[(int)this_strand];
      int from, to;
      string lab_from(kEmptyStr), lab_to(kEmptyStr);
      if (eNa_strand_minus == this_strand 
                               || eNa_strand_both_rev ==  this_strand) {
           to = seq_int.GetFrom();
           from = seq_int.GetTo();
           if (seq_int.CanGetFuzz_to()) {  // test carefully  !!!
               const CInt_fuzz& f_from = seq_int.GetFuzz_to();
               f_from.GetLabel(&lab_from, from, false); 
           }
           else lab_from = NStr::IntToString(++from);
           if (seq_int.CanGetFuzz_from()) {   // !!! test carefully
               const CInt_fuzz& f_to = seq_int.GetFuzz_from();
               f_to.GetLabel(&lab_to, to); 
           }
           else lab_to = NStr::IntToString(++to);
      }
      else {
           to = seq_int.GetTo();
           from = seq_int.GetFrom();
           if (seq_int.CanGetFuzz_from()) {
                const CInt_fuzz& f_from = seq_int.GetFuzz_from();
                f_from.GetLabel(&lab_from, from, false);
           }
           else lab_from = NStr::IntToString(++from);
           if (seq_int.CanGetFuzz_to()) {
               const CInt_fuzz& f_to = seq_int.GetFuzz_to();
               f_to.GetLabel(&lab_to, to);
           }
           else lab_to = NStr::IntToString(++to);
      }
      location += lab_from + "-" + lab_to; 
    } 
    return location;
};

string CTestAndRepData :: SeqLocPrintUseBestID(const CSeq_loc& seq_loc, bool range_only)
{
  string location(kEmptyStr);
  if (seq_loc.IsInt()) {
     location = PrintSeqInt(seq_loc.GetInt(), range_only);
  } 
  else if (seq_loc.IsMix()) {
     location = "(";
     ITERATE (list <CRef <CSeq_loc> >, it, seq_loc.GetMix().Get()) {
        location += SeqLocPrintUseBestID(**it) + ", ";
     }
     if (!location.empty()) location = location.substr(0, location.size()-2);
     location += ")"; 
  }
  else if (seq_loc.IsPacked_int()) {
     location = "(";
     ITERATE (list <CRef <CSeq_interval> >, it, seq_loc.GetPacked_int().Get()) {
        location += PrintSeqInt(**it) + ", ";
     }
     if (!location.empty()) location = location.substr(0, location.size()-2);
     location += ")";
  }
  return location;
}


string CTestAndRepData :: GetLocusTagForFeature(const CSeq_feat& seq_feat)
{
  string tag(kEmptyStr);
  if (seq_feat.GetData().IsGene()) {
    const CGene_ref& gene = seq_feat.GetData().GetGene();
    tag =  (gene.CanGetLocus_tag()) ? gene.GetLocus_tag() : kEmptyStr;
  }
  else {
    const CGene_ref* gene = seq_feat.GetGeneXref();
    if (gene) {
         tag = (gene->CanGetLocus_tag()) ? gene->GetLocus_tag() : kEmptyStr;
    }
    else {
      CConstRef<CSeq_feat> 
         gene= sequence::GetBestOverlappingFeat(seq_feat.GetLocation(),
                                                   CSeqFeatData::e_Gene,
				                   sequence::eOverlap_Contained,
                                                   *thisInfo.scope);
      if (gene.NotEmpty()) {
           tag = (gene->GetData().GetGene().CanGetLocus_tag()) ? gene->GetData().GetGene().GetLocus_tag() : kEmptyStr;
      }
    }
  }
  
  return tag;

}; // GetLocusTagForFeature


void CTestAndRepData :: GetSeqFeatLabel(const CSeq_feat& seq_feat, string& label)
{
     GetLabel(seq_feat, &label, fFGL_Content);
     size_t pos;
     if (!label.empty() && (string::npos != (pos = label.find("-")) ) ) {
          label = CTempString(label).substr(pos+1);
     }
     strtmp = "/number=";
     if (!label.empty() 
            && seq_feat.GetData().GetSubtype() == CSeqFeatData::eSubtype_exon 
            && (string::npos != (pos = label.find(strtmp)))) {
          label = CTempString(label).substr(pos + strtmp.size());
     }
     strtmp = kEmptyStr;
};


const CSeq_feat* CTestAndRepData :: GetCDFeatFromProtFeat(const CSeq_feat& prot)
{
   // pay attention to multiple bioseqs when using GetBioseqFromSeqLoc
   CConstRef <CBioseq>
       bioseq =
           sequence::GetBioseqFromSeqLoc(prot.GetLocation(), 
                                         *thisInfo.scope).GetCompleteBioseq();
   if (bioseq.NotEmpty()) {
      const CSeq_feat* cds= sequence::GetCDSForProduct(*bioseq, thisInfo.scope);
      if (cds) return cds;
   }
   return 0;
};


string CTestAndRepData :: GetDiscItemText(const CSeq_feat& seq_feat)
{
      CSeq_feat* seq_feat_p = const_cast <CSeq_feat*>(&seq_feat);
// cerr << "seq_feat_p " << Blob2Str(*seq_feat_p, eSerial_AsnText) << endl;

      if ( seq_feat.GetData().IsProt()) {
        const CSeq_feat* cds = GetCDFeatFromProtFeat(seq_feat);
        if (cds) seq_feat_p = const_cast<CSeq_feat*>(cds);
      }
      string location = SeqLocPrintUseBestID (seq_feat_p->GetLocation());
      location = (location.empty()) ? "Unknown location": location;
      string label = seq_feat_p->GetData().GetKey();
      if (label.empty()) label = "Unknown label";
      string locus_tag = GetLocusTagForFeature (*seq_feat_p);
      string context_label(kEmptyStr);
      if (seq_feat_p->GetData().IsCdregion()) 
              context_label = GetProdNmForCD(*seq_feat_p);
      else {
          if (seq_feat_p->GetData().IsPub()) {
               seq_feat_p->GetData().GetPub().GetPub().GetLabel(&context_label);
//              if ( seq_feat_p->GetData().GetPub().GetPub().GetLabel(&context_label));
          }
          else GetSeqFeatLabel(*seq_feat_p, context_label);
      }
      if (context_label.empty()) context_label = "Unknown context label";
      strtmp = label+"\t" + context_label + "\t" + location + "\t" + locus_tag;
      if (thisInfo.infile.empty()) {
         return strtmp;
      }
      else {
          return (thisInfo.infile + ": " + strtmp); 
      }
      // return (thisInfo.infile + ": " + label+"\t" + context_label + "\t" 
      //                                        + location + "\t" + locus_tag);
}; // GetDiscItemText(const CSeqFeat& obj)


string CTestAndRepData :: GetContainsComment(unsigned cnt, const string& str)
{
   return(NStr::UIntToString(cnt) + " " + str + ( (1==cnt) ? " contains " : "s contain "));
}

string CTestAndRepData :: GetNoun(unsigned cnt, const string& str)
{
  return(str + ( (1==cnt) ? "" : "s"));
}


string CTestAndRepData :: GetIsComment(unsigned cnt, const string& str)
{
   return(NStr::UIntToString(cnt) + " " + str + ( (1==cnt) ? " is " : "s are "));
}


string CTestAndRepData :: GetHasComment(unsigned cnt, const string& str)
{
   return(NStr::UIntToString(cnt) + " " + str + ( (1==cnt) ? " has " : "s have "));
}


string CTestAndRepData :: GetDoesComment(unsigned cnt, const string& str)
{
   return(NStr::UIntToString(cnt) + " " + str + ( (1==cnt) ? " does " : "s do "));
}


string CTestAndRepData :: GetOtherComment(unsigned cnt, const string& single_str, const string& plural_str)
{
   return(NStr::UIntToString(cnt) + " " + ( (1==cnt) ? single_str : plural_str));
}

string CTestAndRepData :: GetNoCntComment(unsigned cnt, const string& single_str, const string& plural_str)
{
   return((1==cnt) ? single_str : plural_str);
};


void CTestAndRepData :: RmvChar(string& str, string chars)
{
   size_t pos;
   for (unsigned i=0; i< chars.size(); i++) {
      while ((pos = str.find(chars[i])) != string::npos)
       str = str.substr(0, pos) + CTempString(str).substr(pos+1);
   }
};


void CTestAndRepData :: GetTestItemList(const vector <string>& itemlist, Str2Strs& setting2list, const string& delim)
{
   vector <string> rep_dt;
   ITERATE (vector <string>, it, itemlist) {
     rep_dt = NStr::Tokenize(*it, delim, rep_dt, NStr::eMergeDelims);
     if (rep_dt.size() == 2) setting2list[rep_dt[0]].push_back(rep_dt[1]);
     rep_dt.clear();
   }
};

void CTestAndRepData :: AddSubcategory(CRef <CClickableItem>& c_item, const string& sub_grp_nm, const vector <string>* itemlist, const string& desc1, const string& desc2, ECommentTp comm, bool copy2parent, const string& desc3, bool halfsize, unsigned input_cnt)
{    
     size_t pos;
     CRef <CClickableItem> c_sub (new CClickableItem);
     if ( (pos = sub_grp_nm.find("$")) != string::npos) {
         c_sub->setting_name = sub_grp_nm.substr(0, pos);
     }
     else {
         c_sub->setting_name = sub_grp_nm;
     }
     if (itemlist) {
         c_sub->item_list = *itemlist;
         c_sub->obj_list = thisInfo.test_item_objs[sub_grp_nm];
     }
     unsigned cnt = itemlist ? c_sub->item_list.size() : input_cnt;
     cnt = halfsize ? cnt/2 : cnt; 
     switch ( comm ) {
       case e_IsComment:
            c_sub->description = GetIsComment(cnt, desc1) + desc2;
            break;
       case e_HasComment:
            c_sub->description = GetHasComment(cnt, desc1) + desc2; 
            break;
       case e_DoesComment:
            c_sub->description = GetDoesComment(cnt, desc1) + desc2; 
            break;
       case e_ContainsComment:
            c_sub->description = GetContainsComment(cnt, desc1) + desc2;
            break;
       case e_OtherComment:
            c_sub->description = GetOtherComment(cnt, desc1, desc2) + desc3;
            break;
       case e_NoCntComment:
            c_sub->description = GetNoCntComment(cnt, desc1, desc2) + desc3;
            break;
       default:
           NCBI_THROW(CException, eUnknown, "Bad comment type.");
     }  
     if (itemlist && copy2parent) {
        copy(c_sub->item_list.begin(), c_sub->item_list.end(),
             back_inserter( c_item->item_list ));
        copy(c_sub->obj_list.begin(), c_sub->obj_list.end(),
             back_inserter( c_item->obj_list ));
     }
     c_item->subcategories.push_back(c_sub);
};

END_NCBI_SCOPE
