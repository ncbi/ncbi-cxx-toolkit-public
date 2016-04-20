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
#include <objects/submit/Seq_submit.hpp>
#include <objmgr/annot_selector.hpp>
#include <objmgr/util/sequence.hpp>
#include <objmgr/util/seq_loc_util.hpp>
#include <objmgr/util/feature.hpp>
#include <objmgr/seq_vector_ci.hpp>
#include <objmgr/seq_vector.hpp>
#include <objmgr/seq_map.hpp>
#include <util/xregexp/regexp.hpp>

#include <misc/discrepancy_report/hDiscRep_config.hpp>
#include <misc/discrepancy_report/hDiscRep_tests.hpp>
#include <misc/discrepancy_report/hUtilib.hpp>

#include <string>
#include <iostream>
#include <sstream>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(DiscRepNmSpc)

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

string GetTwoFieldSubfield(const string& str, unsigned subfield)
{
  string strmp;

  if (str.empty() || subfield > 2) {
     return kEmptyStr;
  }
  if (!subfield) return str;
  else {
    size_t pos = str.find(':');
    if (pos == string::npos) {
       if (subfield == 1) return str;
       else return kEmptyStr;
    }
    else {
      if (subfield == 1) return str.substr(0, pos);
      else {
        strtmp = CTempString(str).substr(pos+1).empty();
        if (!strtmp.empty()) return strtmp;
        else return kEmptyStr;
      }
    }
  }
};

string GetFirstGBQualMatch (const vector <CRef <CGb_qual> >& quals, const string& qual_name, unsigned subfield, const CString_constraint* str_cons)
{
  string str(kEmptyStr);

  ITERATE (vector <CRef <CGb_qual> >, it, quals) {
    if (NStr::EqualNocase( (*it)->GetQual(), qual_name)) {
      str = (*it)->GetVal();
      str = GetTwoFieldSubfield(str, subfield);
      if ( str.empty()
              || (str_cons && !str_cons->Empty() && !(str_cons->Match(str))) ) {
           str = kEmptyStr;
      }
      else break;
    }
  }
  return str;
}


string GetRNAProductString(const CSeq_feat& seq_feat)
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
                 rna_str = GetFirstGBQualMatch(seq_feat.GetQual(), (string)"product");
             }
             break;
       case CRNA_ref::C_Ext::e_TRNA:
              GetLabel(seq_feat, &rna_str, fFGL_Content);
              rna_str = "tRNA-" + rna_str;
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

struct rRNATerm {
   string   keyword;
   unsigned min_length;
   bool     ignore_partial;
};

static const rRNATerm Terms[] = {
    {"16S",   1000, false},   // keyword, min-length, ignore-partial
    {"18S",   1000, false},
    {"23S",   2000, false},
    {"25s",   1000, false},
    {"26S",   1000, false},
    {"28S",   3300, false},
    {"small", 1000, false},
    {"large", 1000, false},
    {"5.8S",  130,  true},
    {"5S",    90,   true}
};

bool IsShortrRNA(const CSeq_feat& feat, CScope* scope)
{
   if (!scope) return false;
  
   if ( (feat.GetData().GetSubtype() != CSeqFeatData :: eSubtype_rRNA)
           || (feat.CanGetPartial() && feat.GetPartial()) ) {
        return false;
   }

   unsigned len = sequence::GetCoverage(feat.GetLocation(), scope);
   string rrna_name = GetRNAProductString(feat);
   for (unsigned i=0; i< ArraySize(Terms); i++) {
      if (NStr::FindNoCase(rrna_name, Terms[i].keyword) != string::npos
             && len < Terms[i].min_length
             && (!Terms[i].ignore_partial || !feat.CanGetPartial() || !feat.GetPartial())){
         return true;
      }
   }

   return false;
};

const string& CTestAndRepData :: x_GetUserObjType(const CUser_object& user_obj) const
{
  return (user_obj.GetType().IsStr() ? user_obj.GetType().GetStr() : kEmptyStr);
};

bool CTestAndRepData :: AllCapitalLetters(const string& pattern, const string& search)
{ 
  if (search.empty()) return false;
  if (search.find_first_of(thisInfo.alpha_str) == string::npos) return false;
  return (IsAllCaps(search));
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

static const char* s_pseudoweasels[] = {
  "pseudouridine",
  "pseudoazurin",
  "pseudouridylate"
};
bool CTestAndRepData :: ContainsPseudo(const string& pattern, const list <string>& strs)
{
  bool right_pseudo;
  size_t pos;
  unsigned i;
  string weasel;
  ITERATE (list <string>, it, strs) {
    strtmp = *it;
    while ( !strtmp.empty() && (pos = strtmp.find("pseudo")) != string::npos) {
        right_pseudo = true;
        for (i=0; i< ArraySize(s_pseudoweasels); i++) {
           weasel = s_pseudoweasels[i]; 
           if ( CTempString(strtmp).substr(pos, weasel.size()) 
                   == s_pseudoweasels[i]) {
               right_pseudo = false;
               strtmp = CTempString(strtmp).substr(pos + weasel.size());
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
         && NStr::EqualNocase(CTempString(search).substr(len - phrase_len), 
                              pattern)) {
      return true;
  }
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

  if (len >= phrase_len && NStr::EqualNocase(search,0, phrase_len, pattern)) {
       return true;
  }
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

bool CTestAndRepData :: AllVecElementsSame(const vector <string> arr)
{
    if (arr.size() > 1) {
        ITERATE (vector <string>, it, arr) {
           if ( arr[0] != *it ) {
              return false;
           }
        }
    }

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
   ITERATE (list <CRef <COrgMod> >, it, org.GetOrgname().GetMod() ) {
      if ((*it)->GetSubtype() == subtype) {
          strs.push_back((*it)->GetSubname());
      }
   }
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
    vector <string> arr;
    arr = NStr::Tokenize(src, " \n", arr, NStr::eMergeDelims);
    CRegexpUtil rxu(NStr::Join(arr, " "));
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
  if (str.empty() || phrase.empty()) return false;
  size_t pos;
  pos = case_sensitive ? NStr::FindCase(str, phrase) : NStr::FindNoCase(str, phrase);
  unsigned len;
  if (pos != string::npos) {
    if (whole_word) {
       len = phrase.size();
       while (pos != string::npos) {
         if ( (!pos || !isalnum(str[pos-1])) 
                 && (pos + len == str.size() || !isalnum(str[pos+len]))) {
             return true;
         }
         else {
            pos = case_sensitive ? 
                NStr::FindCase(str, phrase, pos+1): NStr::FindNoCase(str, phrase, pos+1);
         }
       }
    }
    else return true;
  }

  return false;
};


bool CTestAndRepData :: DoesStringContainPhrase(const string& str, const vector <string>& phrases, bool case_sensitive, bool whole_word)
{
   ITERATE (vector <string>, it, phrases) {
      if (DoesStringContainPhrase(str, *it, case_sensitive, whole_word)) {
           return true;   
      }
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


void CTestAndRepData :: RmvRedundancy(vector <string>& items, vector <CConstRef <CObject> >& objs)
{
    set <string> uni_item;
    vector<string> new_items;
    vector< CConstRef<CObject> > new_objs;

    vector<string>::iterator item_it = items.begin();
    vector <CConstRef <CObject> >::iterator obj_it = objs.begin();
    while (item_it != items.end()) {
        pair< set<string>::iterator, bool> p =
            uni_item.insert(*item_it);
        if (p.second) {
            // insert succeeded; new item
            new_items.push_back(*item_it);
            item_it++;
            if (obj_it != objs.end()) {
                new_objs.push_back(*obj_it);
                obj_it++;
            }
        } else {
            ++item_it;
            if (obj_it != objs.end()) {
                ++obj_it;
            }
        }
    }

    items.swap(new_items);
    objs.swap(new_objs);
};



void CTestAndRepData :: GetProperCItem(CRef <CClickableItem> c_item, bool* citem1_used)
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
   if (NStr::FindNoCase(thisInfo.report_lineage, type) != string::npos) {
        return true;
   }
   else if (thisInfo.report_lineage.empty() && biosrc.IsSetLineage()
                && NStr::FindNoCase(biosrc.GetLineage(), type) != string::npos){
      return true;
   }
   else return false;
};


bool CTestAndRepData :: IsEukaryotic(const CBioseq& bioseq)
{
   const CBioSource* 
       biosrc = sequence::GetBioSource(thisInfo.scope->GetBioseqHandle(bioseq));
   if (biosrc) {
      CBioSource :: EGenome genome = (CBioSource::EGenome) biosrc->GetGenome();
      if (genome != CBioSource :: eGenome_mitochondrion
                  && genome != CBioSource :: eGenome_chloroplast
                  && genome != CBioSource :: eGenome_plastid
                  && genome != CBioSource :: eGenome_apicoplast
                  && HasLineage(*biosrc, "Eukaryota")) {
         return true;
     }
   }
   return false;
};


bool CTestAndRepData :: IsBioseqHasLineage(const CBioseq& bioseq, const string& type, bool has_biosrc)
{
   if (has_biosrc) { 
      ITERATE (vector <const CSeqdesc*>, it, bioseq_biosrc_seqdesc) {
         if (HasLineage( (*it)->GetSource(), type)) {
            return true;
         }
      }
      return false;
   }
   else {
      CBioseq_Handle bioseq_handle = thisInfo.scope->GetBioseqHandle(bioseq);
      if (!bioseq_handle) {
          return false;
      }
      for (CSeqdesc_CI it(bioseq_handle, CSeqdesc :: e_Source); it; ++it) {
          if (HasLineage(it->GetSource(), type)) {
                return true;
          }
      }
      return false;
   }
};


bool CTestAndRepData :: HasTaxonomyID(const CBioSource& biosrc)
{
   ITERATE (vector <CRef <CDbtag> >, it, biosrc.GetOrg().GetDb())
       if ((*it)->GetType() == CDbtag::eDbtagType_taxon) return true;
   return false;
};

bool IsAllCaps(const string& str)
{
  string up_str = str;
  if (up_str.find_first_not_of(thisInfo.alpha_str) != string::npos) return false;
  up_str = NStr::ToUpper(up_str);
  if (up_str == str) return true;
  else return false;
};



bool IsAllLowerCase(const string& str)
{
  string low_str = str;
  if (low_str.find_first_not_of(thisInfo.alpha_str) != string::npos) return false;
  low_str = NStr::ToLower(low_str);
  if (low_str == str) return true;
  else return false;
}


bool IsAllPunctuation(const string& str)
{
   for (unsigned i=0; i< str.size(); i++) {
     if (!ispunct(str[i])) return false;
   }
   return true;
};


bool IsWholeWord(const string& str, const string& phrase)
{
  if (str == phrase) return true;
  size_t pos = str.find(phrase);
  if (string::npos == pos) return false;
  size_t pos2 = pos+phrase.size();
  if ((!pos || !isalnum(str[pos-1]))
        && ((str.size() == pos2) || !isalnum(str[pos2]))) return true;
  else return false;
  
}; //CTestAndRepData



string GetProdNmForCD(const CSeq_feat& cd_feat, CScope* scope)
{
  if (!scope) return kEmptyStr;

  // should be the longest CProt_ref on the bioseq pointed by the cds.GetProduct()
  // check Xref first;
  const CProt_ref* prot = cd_feat.GetProtXref();
  if (prot && prot->CanGetName() && !prot->GetName().empty()) {
     return ( *(prot->GetName().begin()) );
  }

  CConstRef <CSeq_feat> prot_seq_feat;
  if (cd_feat.CanGetProduct()) {
     prot_seq_feat 
        = CConstRef <CSeq_feat> (
                      sequence::GetBestOverlappingFeat(cd_feat.GetProduct(),
                                             CSeqFeatData::e_Prot, 
                                             sequence::eOverlap_Contains,
                                             *scope));
  }
  if (prot_seq_feat.NotEmpty() && prot_seq_feat->GetData().IsProt()) {
      const CProt_ref& prot = prot_seq_feat->GetData().GetProt();
      if (prot.CanGetName() && !prot.GetName().empty()) {
         return( *(prot.GetName().begin()) );
      }
  }
  return (kEmptyStr);
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
        if (CBioseq_set::eClass_gen_prod_set == bioseq_set->GetClass()) {
             rval = IsProdBiomol(bioseq);
        }
        else if (CBioseq_set::eClass_nuc_prot == bioseq_set->GetClass()) {
             CConstRef<CBioseq_set> bioseq_set_set = bioseq_set->GetParentSet();
             if (bioseq_set_set.NotEmpty()) {
                if (CBioseq_set::eClass_gen_prod_set == bioseq_set_set->GetClass()) {
                    rval = IsProdBiomol(bioseq);
                }
             }
        }
   }
   return rval;
};


string GetSeqId4BioseqSet(const string& desc)
{
   size_t pos;
   string seqid_str;
   if ( (pos = desc.find("ss|")) != string::npos) {
      seqid_str = desc.substr(pos+3);
   }
   else if ((pos = desc.find("np|")) != string::npos) {
      seqid_str = desc.substr(pos+3);
   }
   else if ( (pos = desc.find("Set containing")) != string::npos) {
      seqid_str = desc;
      seqid_str = NStr::Replace(seqid_str, "Set containing", "");
      seqid_str = NStr::TruncateSpaces(seqid_str);
   }
   return seqid_str;
};

string CTestAndRepData :: GetDiscItemText(const CSeq_entry& seq_entry)
{
   return (GetDiscrepancyItemText(seq_entry, true));
};

string GetDiscrepancyItemText(const CSeq_entry& seq_entry, bool append_ids)
{
   string desc, seqid_str;
   if (seq_entry.IsSeq()) {
        desc =  BioseqToBestSeqIdString(seq_entry.GetSeq(), CSeq_id::e_Genbank);
        seqid_str = desc;
   }
   else {
       desc = GetDiscrepancyItemTextForBioseqSet(seq_entry.GetSet());
       seqid_str = GetSeqId4BioseqSet(desc);
   }
   if (append_ids) {
         desc += (thisInfo.xml_label + seqid_str);
   }
   if (thisInfo.infile.empty()) {
       return desc;
   }
   else {
      return (thisInfo.infile + ": " + desc);
   }
};


string CTestAndRepData :: GetDiscItemText(const CSeq_submit& seq_submit)
{
   return (GetDiscrepancyItemText(seq_submit, true));
};
string GetDiscrepancyItemText(const CSeq_submit& seq_submit, bool append_ids)
{
  string desc(kEmptyStr), seqid_str;
  if (seq_submit.GetData().IsEntrys()) {
     const list <CRef <CSeq_entry> >& entrys = seq_submit.GetData().GetEntrys();
     if (!entrys.empty()) {
        desc = "Cit-sub for ";
        if ((*entrys.begin())->IsSeq()) {
            seqid_str = BioseqToBestSeqIdString((*entrys.begin())->GetSeq(), 
                                             CSeq_id::e_Genbank);
            desc += seqid_str;
        }
        else {
            seqid_str = GetDiscrepancyItemTextForBioseqSet((*entrys.begin())->GetSet());
            desc += seqid_str;
            seqid_str = GetSeqId4BioseqSet(seqid_str);
        }
     }
  }
  if (append_ids) {
       desc += (thisInfo.xml_label + seqid_str);
  }
  if (thisInfo.infile.empty()) {
     return desc;
  }
  else {
     return (thisInfo.infile + ": " + desc);
  }
};


string GetDiscrepancyItemTextForBioseqSet(const CBioseq_set& bioseq_set)
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
    if (!seq_entrys.empty()) {
       if ( (*(seq_entrys.begin()))->IsSeq()) {
         row_text = "Set containing "  
                 + BioseqToBestSeqIdString((*(seq_entrys.begin()))->GetSeq(), 
                                           CSeq_id::e_Genbank);
       }
       else if ( (*(seq_entrys.begin()))->IsSet()) {
          row_text = "Set containing " 
              + GetDiscrepancyItemTextForBioseqSet((*(seq_entrys.begin()))->GetSet());
       } 
       else row_text =  "BioseqSet";
    }
  }

  return (row_text);

};

string CTestAndRepData :: GetDiscItemText(const CBioseq_set& bioseq_set)
{
   return (GetDiscrepancyItemText(bioseq_set, true));
}

string GetDiscrepancyItemText(const CBioseq_set& bioseq_set, bool append_ids)
{
   string     row_text;
   row_text = GetDiscrepancyItemTextForBioseqSet(bioseq_set);
   string seqid_str = GetSeqId4BioseqSet(row_text);

   if (row_text.empty()) return(kEmptyStr);
   else {
      if (append_ids) {
           row_text += (thisInfo.xml_label + seqid_str);
      }
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



CBioseq* GetRepresentativeBioseqFromBioseqSet(const CBioseq_set& bioseq_set)
{
   if (bioseq_set.GetClass() == CBioseq_set::eClass_nuc_prot) {
       return ( const_cast<CBioseq*>(&(bioseq_set.GetNucFromNucProtSet())));
   }
   else if (bioseq_set.GetClass() == CBioseq_set::eClass_segset) {
       return ( const_cast<CBioseq*>(&(bioseq_set.GetMasterFromSegSet())));
   }
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


string SeqDescLabelContent(const CSeqdesc& sd)
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
                label 
                  += ENUM_METHOD_NAME(EGIBB_mod)()->FindName(*it, true) + ", ";
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
   return (GetDiscrepancyItemText(seqdesc, bioseq, true));
};

string GetDiscrepancyItemText(const CSeqdesc& seqdesc, const CBioseq& bioseq, bool append_ids)
{
  // what if no bioseq?
  string seqid_str = BioseqToBestSeqIdString(bioseq, CSeq_id::e_Genbank);
  string row_text = seqid_str + ": ";
  row_text += SeqDescLabelContent(seqdesc);
  if (append_ids) {
       row_text += (thisInfo.xml_label + seqid_str);
  }
  if (thisInfo.infile.empty()) return row_text;
  else return(thisInfo.infile + ": " + row_text);
};


string CTestAndRepData :: GetDiscItemText(const CSeqdesc& seqdesc, const CSeq_entry& entry)
{
   return (GetDiscrepancyItemText(seqdesc, entry, true));
}
string GetDiscrepancyItemText(const CSeqdesc& seqdesc, const CSeq_entry& entry, bool append_ids)
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
  else return(GetDiscrepancyItemText(seqdesc, *bioseq, append_ids));

};


string CTestAndRepData :: GetDiscItemText(const CBioseq& bioseq)
{
   CBioseq_Handle bioseq_handle = thisInfo.scope->GetBioseqHandle(bioseq);
   if (!bioseq_handle) return kEmptyStr;
   return (GetDiscrepancyItemText(bioseq_handle, true));
}
string GetDiscrepancyItemText(const CBioseq_Handle& bioseq_hl, bool append_ids)
{
   if (!bioseq_hl) return kEmptyStr;

   string len_txt = (bioseq_hl.CanGetInst_Length()) ?
               (" (length " + NStr::IntToString(bioseq_hl.GetInst_Length())) : kEmptyStr;
   CSeq_data out_seq;
   vector <unsigned> idx;
   unsigned ambigs_cnt = 0, gap_cnt = 0;
   SSeqMapSelector sel(CSeqMap::fDefaultFlags);
   if (bioseq_hl.CanGetInst_Seq_data()) { 
      for (CSeqMap_CI seqmap_ci(bioseq_hl, sel); seqmap_ci;  ++seqmap_ci){
         if (seqmap_ci.GetType() == CSeqMap::eSeqGap) {
            gap_cnt += seqmap_ci.GetLength();
         }
         else if (bioseq_hl.IsNa()){
            ambigs_cnt += CSeqportUtil::GetAmbigs(seqmap_ci.GetRefData(),
                                                    &out_seq, &idx,
                                                    CSeq_data::e_Ncbi2na,
                                                    seqmap_ci.GetRefPosition(),
                                                    seqmap_ci.GetLength());
         }
      }
   }
   else {
      CSeqVector seq_vec(bioseq_hl, CBioseq_Handle::eCoding_Iupac);
      for (CSeqVector_CI it = seq_vec.begin(); it; ++ it) {
         if (it.IsInGap()) gap_cnt ++;
         else if (bioseq_hl.IsNa() && (*it) != 'A' 
                      && (*it) != 'T' && (*it) != 'G' && (*it) != 'C') {
                  ambigs_cnt ++;
         }
      }
   }
 
   if (bioseq_hl.IsNa()) {
       if (ambigs_cnt >0) {
          len_txt += ", " + NStr::UIntToString(ambigs_cnt) + " other"; 
       }
   }
   if (gap_cnt >0) {
       len_txt += ", " + NStr::UIntToString(gap_cnt) + " gap"; 
   }
   len_txt += ")";

   string seqid_str = BioseqToBestSeqIdString(*bioseq_hl.GetCompleteBioseq(),CSeq_id::e_Genbank);
   string row_text(kEmptyStr);
   if (!(thisInfo.infile.empty())) row_text = thisInfo.infile +": ";
   row_text += seqid_str + len_txt;
   if (append_ids) row_text += (thisInfo.xml_label + seqid_str);
   return (row_text);

};


const CSeq_id& BioseqToBestSeqId(const CBioseq& bioseq, CSeq_id :: E_Choice choice )
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
      if ( best_score == 99999 ) { 
           NCBI_USER_THROW("BioseqToBestSeqId failed");
      }
      else return ( (*best_seq_id).GetObject() );  
}; // BioseqToBestSeqId()


string BioseqToBestSeqIdString(const CBioseq& bioseq, CSeq_id :: E_Choice choice )
{
  const CSeq_id& seq_id = BioseqToBestSeqId(bioseq, CSeq_id::e_Genbank);
  return (seq_id.GetSeqIdString(true));
}; // BioseqToBestSeqIdString();


static const char* strandsymbol[] = {"", "", "c","b", "r"};
string PrintSeqInt(const CSeq_interval& seq_int, CScope* scope, bool range_only)
{
    string location(kEmptyStr);
    if (!scope) return kEmptyStr;

    // Best seq_id
    if (!range_only) {
      const CSeq_id& seq_id = seq_int.GetId();
      if (!(seq_id.IsGenbank()) ) {
         CBioseq_Handle bioseq_hl = scope->GetBioseqHandle(seq_id);
         if (bioseq_hl) {
            const CSeq_id& 
                new_seq_id = BioseqToBestSeqId(*(bioseq_hl.GetCompleteBioseq()),
                                               CSeq_id::e_Genbank);
            location = new_seq_id.AsFastaString();
         }
         else location = seq_id.AsFastaString();  // should be impossible
      }
      else location = seq_id.AsFastaString();
      location += ":";
    }

    // strand
    ENa_strand 
      this_strand 
         = (seq_int.CanGetStrand()) ? seq_int.GetStrand(): eNa_strand_unknown;
    location += strandsymbol[(int)this_strand];
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
    return location;
};

string SeqLocPrintUseBestID(const CSeq_loc& seq_loc, CScope* scope, bool range_only)
{
  if (!scope) return kEmptyStr;

  string locs, strtmp;
  string location(kEmptyStr);
  if (seq_loc.IsInt()) {
     location = PrintSeqInt(seq_loc.GetInt(), scope, range_only);
  } 
  else if (seq_loc.IsMix()) {
     location = "(";
     ITERATE (list <CRef <CSeq_loc> >, it, seq_loc.GetMix().Get()) {
        if (it == seq_loc.GetMix().Get().begin()) {
           strtmp = SeqLocPrintUseBestID(**it, scope, range_only);
           location += strtmp;
           locs += strtmp;
        }
        else {
           strtmp = SeqLocPrintUseBestID(**it, scope, true);
           location += strtmp;
           locs += strtmp;
        }
        location += ", ";
        locs += ",";
     }
     location = CTempString(location).substr(0, location.size()-2);
     location += ")"; 

     locs = CTempString(locs).substr(0, locs.size()-1);
  }
  else if (seq_loc.IsPacked_int()) {
     location = "(";
     ITERATE (list <CRef <CSeq_interval> >, it, seq_loc.GetPacked_int().Get()) {
        if (it == seq_loc.GetPacked_int().Get().begin()) {
           strtmp = PrintSeqInt(**it, scope, range_only);
           location += strtmp;
           locs += strtmp;
        }
        else {
           strtmp = PrintSeqInt(**it, scope, true);
           location += strtmp;
           locs += strtmp;
        }
        location += ", ";
        locs += ",";
     }
     location = CTempString(location).substr(0, location.size()-2);
     location += ")";

     locs = CTempString(locs).substr(0, locs.size() - 1);
  }
  return location;
}


string GetLocusTagForFeature(const CSeq_feat& seq_feat, CScope* scope)
{
  if (!scope) return kEmptyStr;

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
                                                   *scope);
      if (gene.NotEmpty()) {
           tag = (gene->GetData().GetGene().CanGetLocus_tag()) ? gene->GetData().GetGene().GetLocus_tag() : kEmptyStr;
      }
    }
  }
  
  return tag;

}; // GetLocusTagForFeature


void GetSeqFeatLabel(const CSeq_feat& seq_feat, string& label)
{
     label = kEmptyStr;
     GetLabel(seq_feat, &label, fFGL_Content);
     size_t pos;
     if (!label.empty() && (string::npos != (pos = label.find("-")) ) ) {
          label = CTempString(label).substr(pos+1);
     }
     strtmp = "/number=";
     if (!label.empty() 
            && (seq_feat.GetData().GetSubtype() == CSeqFeatData::eSubtype_exon 
                   || seq_feat.GetData().GetSubtype() 
                              == CSeqFeatData::eSubtype_intron)
            && (string::npos != (pos = label.find(strtmp)))) {
          label = CTempString(label).substr(pos + strtmp.size());
          if (label.find("exon") == 0 || label.find("intron") == 0) { // pos
             label = CTempString(label).substr(0, label.find(' '));
          }
     }
     strtmp = kEmptyStr;
};


const CSeq_feat* GetCDFeatFromProtFeat(const CSeq_feat& prot, CScope* scope)
{
   if (!scope) return 0;

   // pay attention to multiple bioseqs when using GetBioseqFromSeqLoc
   CConstRef <CBioseq>
       bioseq = sequence::GetBioseqFromSeqLoc(prot.GetLocation(), *scope).GetCompleteBioseq();
   if (bioseq.NotEmpty()) {
      const CSeq_feat* cds= sequence::GetCDSForProduct(*bioseq, scope);
      if (cds) return cds;
   }
   return 0;
};


string GetFeatId(const CFeat_id& feat_id)
{
   string strtmp;
   switch (feat_id.Which()) {
      case CFeat_id::e_Gibb:
         strtmp = NStr::IntToString(feat_id.GetGibb());
         break;
      case CFeat_id::e_Giim:
         {
              const CGiimport_id& giim = feat_id.GetGiim();
              if (giim.CanGetDb()) {
                 strtmp = giim.GetDb() + "|";
              }
              strtmp += NStr::IntToString(giim.GetId());
              if (giim.CanGetRelease()) {
                  strtmp += ("." + giim.GetRelease());
              }
              break;
         }
       case CFeat_id::e_Local:
         {
             ostringstream ostr;
             feat_id.GetLocal().AsString(ostr);
             strtmp = ostr.str();
             break;
         }
       case CFeat_id::e_General:
         {
             feat_id.GetGeneral().GetLabel(&strtmp);
             break;
         }
       default: break;
   } // switch
   return strtmp;
};


string CTestAndRepData :: GetDiscItemText(const CSeq_feat& seq_feat)
{
   return (GetDiscrepancyItemText(seq_feat, thisInfo.scope.GetPointer(), true));
}
static const string spaces("     ");
string GetDiscrepancyItemText(const CSeq_feat& seq_feat, CScope* scope, bool append_ids)
{
  if (!scope) return kEmptyStr;
  
  CSeq_feat* seq_feat_p = const_cast <CSeq_feat*>(&seq_feat);

  if ( seq_feat.GetData().IsProt()) {
     const CSeq_feat* cds = GetCDFeatFromProtFeat(seq_feat, scope);
     if (cds) seq_feat_p = const_cast<CSeq_feat*>(cds);
  }
   
  string location = SeqLocPrintUseBestID (seq_feat_p->GetLocation(), scope); 
  string label = seq_feat_p->GetData().GetKey();
  string locus_tag = GetLocusTagForFeature (*seq_feat_p, scope);
  string context_label(kEmptyStr);
  if (seq_feat_p->GetData().IsCdregion()) { 
          context_label = GetProdNmForCD(*seq_feat_p, scope);
          if (context_label.empty()) {
             GetSeqFeatLabel(*seq_feat_p, context_label);
          }
  }
  else if (seq_feat_p->GetData().IsPub()) {
               seq_feat_p->GetData().GetPub().GetPub().GetLabel(&context_label);
  }
  else if (seq_feat_p->GetData().IsGene()) {
             if (seq_feat_p->GetData().GetGene().CanGetLocus()
                  && !seq_feat_p->GetData().GetGene().GetLocus().empty()) {
                context_label = seq_feat_p->GetData().GetGene().GetLocus(); 
             }
             else if (seq_feat_p->GetData().GetGene().CanGetDesc()) {
                context_label = seq_feat_p->GetData().GetGene().GetDesc(); 
             }
  } 
  else GetSeqFeatLabel(*seq_feat_p, context_label);

  // append necessary ids for reference later;
  string feat_id_str = (seq_feat_p->CanGetId()) ?
                              GetFeatId(seq_feat_p->GetId()) : kEmptyStr;
  string seq_id_str = CTempString(location).substr(0, location.find(":"));
  string seq_loc_str = CTempString(location).substr(location.find(":") + 1);
  vector <string> arr;
  arr = NStr::Tokenize(seq_loc_str, " ", arr);
  seq_loc_str = NStr::Join(arr, "");

  if (!label.empty()) label += spaces ;
  if (!context_label.empty()) context_label +=  spaces;
  if (!location.empty() && !locus_tag.empty()) location += spaces;
  strtmp = label + context_label + location + locus_tag;
  if (append_ids) {
     strtmp += 
        (thisInfo.xml_label + seq_id_str + "," + feat_id_str + "," + seq_loc_str + ",");
  }
  if (thisInfo.infile.empty()) {
         return strtmp;
  }
  else {
          return (thisInfo.infile + ": " + strtmp); 
  }
}; // GetDiscrepancyItemText(const CSeqFeat& obj)


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
     if (rep_dt.size() == 2) {
          setting2list[rep_dt[0]].push_back(rep_dt[1]);
     }
     rep_dt.clear();
   }
};

void CTestAndRepData :: GetTestItemList(const set <string>& itemlist, Str2Strs& setting2list, const string& delim)
{
   vector <string> rep_dt;
   ITERATE (set <string>, it, itemlist) {
     rep_dt = NStr::Tokenize(*it, delim, rep_dt, NStr::eMergeDelims);
     if (rep_dt.size() == 2) {
          setting2list[rep_dt[0]].push_back(rep_dt[1]);
     }
     rep_dt.clear();
   }
};

void CTestAndRepData :: AddSubcategory(CRef <CClickableItem> c_item, const string& sub_grp_nm, const vector <string>* itemlist, const string& desc1, const string& desc2, ECommentTp comm, bool copy2parent, const string& desc3, bool halfsize, unsigned input_cnt, bool rm_redundancy)
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
         if (rm_redundancy) {
            RmvRedundancy(c_sub->item_list, c_sub->obj_list);
         }
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
           NCBI_USER_THROW("Bad comment type.");
     }  
     if (itemlist && copy2parent) {
        copy(c_sub->item_list.begin(), c_sub->item_list.end(),
             back_inserter( c_item->item_list ));
        copy(c_sub->obj_list.begin(), c_sub->obj_list.end(),
             back_inserter( c_item->obj_list ));
     }
     c_item->subcategories.push_back(c_sub);
};

END_SCOPE(DiscRepNmSpc)
END_NCBI_SCOPE
