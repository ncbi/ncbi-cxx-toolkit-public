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
 *   Functions dealing with CSuspect_rule
 *
 * $Log$
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>

#include <objects/macro/CDSGenePr_pseudo_constrain.hpp>
#include <objects/macro/CDSGenePr_constraint_field.hpp>
#include <objects/macro/CDSGenePro_qual_constraint.hpp>
#include <objects/macro/Field_type.hpp>
#include <objects/macro/Field_constraint.hpp>
#include <objects/macro/Location_pos_constraint.hpp>
#include <objects/macro/Match_type_constraint_.hpp>
#include <objects/macro/Misc_field_.hpp>
#include <objects/macro/Molinfo_field_constraint.hpp>
#include <objects/macro/Publication_constraint.hpp>
#include <objects/macro/Seque_const_mol_type_const.hpp>
#include <objects/macro/Sequence_constraint.hpp>
#include <objects/macro/Source_constraint.hpp>
#include <objects/macro/String_constraint_set.hpp>
#include <objects/macro/Translation_constraint.hpp>
#include <objects/macro/Word_substitution.hpp>
#include <objects/macro/Word_substitution_set.hpp>
#include <objmgr/util/sequence.hpp>
#include <objtools/format/items/qualifiers.hpp>

#include <objtools/discrepancy_report/hDiscRep_config.hpp>
#include <objtools/discrepancy_report/hDiscRep_tests.hpp>
#include <objtools/discrepancy_report/hUtilib.hpp>

#include <sstream> 

BEGIN_NCBI_SCOPE

USING_NCBI_SCOPE;

USING_SCOPE(DiscRepNmSpc);
USING_SCOPE(objects);
USING_SCOPE(sequence);

static string strtmp;
static vector <string> arr;
static CDiscRepInfo thisInfo;

string CDiscRepUtil::digit_str("0123456789");
string CDiscRepUtil::alpha_str("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ");

CBioseq_Handle CSuspectRuleCheck::m_bioseq_hl;

string CSuspectRuleCheck :: GetFieldValue(const CCGPSetData& cgp, const CSeq_feat& feat, ECDSGeneProt_field val, const CString_constraint* str_cons)
{
   string str;
   if (val == eCDSGeneProt_field_mat_peptide_name
         || val == eCDSGeneProt_field_mat_peptide_description
         || val == eCDSGeneProt_field_mat_peptide_ec_number
         || val == eCDSGeneProt_field_mat_peptide_activity
         || val == eCDSGeneProt_field_mat_peptide_comment) {
     CRef <CFeature_field> ff = FeatureFieldFromCDSGeneProtField (val);
     str = GetQualFromFeature(feat, const_cast<CFeature_field&>(*ff), str_cons);
   }
   else str = GetFieldValueFromCGPSet(cgp, val, str_cons);
   return str;
};

string CSuspectRuleCheck :: CopyListWithoutBankIt (const string& orig)
{
  string cpy(orig), pre_seq(kEmptyStr);
  size_t pos;
  while (cpy.size() > 6 && cpy != pre_seq) {
    pos = NStr::FindNoCase(orig, "Bankit");
    if (pos == string::npos) return cpy;
    else if (!pos || isspace(orig[pos]) || orig[pos-1] == ',' || orig[pos-1] == ';') {
       cpy = CTempString(cpy).substr(pos+6);
       cpy = NStr::TruncateSpaces(cpy, NStr::eTrunc_Begin);
       if (!cpy.empty() && (cpy[0] == '/' || cpy[0] == ':')) cpy = CTempString(cpy).substr(1);
    }
    pre_seq = cpy;
  }
  return cpy;
};

bool CSuspectRuleCheck :: DoesObjectIdMatchStringConstraint (const CObject_id& obj_id, const CString_constraint& str_cons)
{
  if (IsStringConstraintEmpty (&str_cons)) return true;
  else {
     if (obj_id.IsId()) strtmp = NStr::IntToString(obj_id.GetId());
     else strtmp = obj_id.GetStr();
     return DoesStringMatchConstraint (strtmp, &str_cons);
  }
}

bool CSuspectRuleCheck :: DoesTextMatchBankItId (const CSeq_id& sid, const CString_constraint& str_cons)
{
  const CDbtag& dbtag = sid.GetGeneral();
  if (dbtag.GetDb() != "BankIt") return false;
  string text = str_cons.CanGetMatch_text()? 
                    CopyListWithoutBankIt (str_cons.GetMatch_text()) : kEmptyStr;

  CString_constraint tmp_cons;
  strtmp = Blob2Str(str_cons);
  Str2Blob(strtmp, tmp_cons);
  tmp_cons.SetMatch_text(text);
  bool rval = DoesObjectIdMatchStringConstraint (dbtag.GetTag(), tmp_cons);
  size_t pos;
  if (!rval) {
    pos = text.find_first_of("/ "); // StringCSpn(text, "/ ");
    if (pos != string::npos) {
      strtmp = text;
      strtmp[pos] = '_';
      tmp_cons.SetMatch_text(strtmp);
      rval = DoesObjectIdMatchStringConstraint (dbtag.GetTag(), tmp_cons);
      tmp_cons.SetMatch_text(text);
    }    
  }
  if (!rval && text[pos] != '/' && dbtag.GetTag().IsStr() && !dbtag.GetTag().GetStr().empty() 
          && (pos = dbtag.GetTag().GetStr().find('/')) != string::npos) {
    string partial_match = dbtag.GetTag().GetStr().substr(0, pos);
    rval = DoesStringMatchConstraint (partial_match, &tmp_cons);
  }
  return rval;
};

bool CSuspectRuleCheck :: DoesSeqIDListMeetStringConstraint (const vector <CSeq_id_Handle>& seq_ids, const CString_constraint& str_cons)
{
  size_t pos;
  bool match, changed;
  bool has_match_text = str_cons.CanGetMatch_text() && !str_cons.GetMatch_text().empty();
  ITERATE (vector <CSeq_id_Handle>, it, seq_ids) {
    CConstRef <CSeq_id> seq_id = (*it).GetSeqId();
    if (seq_id.Empty()) continue;
    string id = seq_id->GetSeqIdString();
    match = DoesSingleStringMatchConstraint(id, &str_cons);
    if (!match) {
      changed = false;
      /* remove terminating pipe character */
      if (id[id.size()-1] == '|') {
        id = id.substr(0, id.size()-1);
        changed = true;
      }
      /* remove leading pipe identifier */
      pos = id.find('|'); 
      if (pos != string::npos) {
        changed = true;
        id = CTempString(id).substr(pos+1);
      }  
      if (changed) 
        match = DoesSingleStringMatchConstraint (id, &str_cons);

      /* if search text doesn't have ., try ID without version */
      if (!match && has_match_text && str_cons.GetMatch_text().find('.') == string::npos) 
      {
        pos = id.find('.');
        if (pos != string::npos) 
           match = DoesSingleStringMatchConstraint (id.substr(0, pos), &str_cons);
      }

      /* Bankit? */
      if (!match && seq_id->IsGeneral()
                 && has_match_text && DoesTextMatchBankItId (*seq_id, str_cons)) 
          match = true;

      if (!match && seq_id->IsGeneral()) {
        const CDbtag& dbtag = seq_id->GetGeneral();
        const CObject_id& tag = dbtag.GetTag();
        if (dbtag.GetDb() == "NCBIFILE") {
          if (tag.IsStr() && DoesSingleStringMatchConstraint (tag.GetStr(), &str_cons)) 
                match = true;
          else if ((pos = tag.GetStr().find_last_of('/')) != string::npos) {
            strtmp = tag.GetStr().substr(0, pos);
            if (DoesSingleStringMatchConstraint (strtmp, &str_cons)) match = true;
          }
        }
      }
    }

    if (match)
    {
      if (str_cons.GetNot_present()) return false;
      else return true;
    }
  }
  if (str_cons.GetNot_present()) return true;
  else return false;
};

bool CSuspectRuleCheck :: DoesSequenceMatchStrandednessConstraint (EFeature_strandedness_constraint strandedness)
{
  if (strandedness == eFeature_strandedness_constraint_any) return true;

  unsigned num_minus = 0, num_plus = 0;
  for (CFeat_CI ci(m_bioseq_hl); ci; ++ci) {
     const CSeq_feat& feat = ci->GetOriginalFeature();
     if (feat.GetLocation().GetStrand() == eNa_strand_minus) {
        num_minus++;
        if (strandedness == eFeature_strandedness_constraint_plus_only
               || strandedness == eFeature_strandedness_constraint_no_minus)
            return false;
        else if (strandedness == eFeature_strandedness_constraint_at_least_one_minus)
                 return true;
     }
     else {
       num_plus++;
       if (strandedness == eFeature_strandedness_constraint_minus_only
                      || strandedness == eFeature_strandedness_constraint_no_plus)
              return false;
       else if (strandedness == eFeature_strandedness_constraint_at_least_one_plus)
              return true;
     }
  }

  switch (strandedness) {
    case eFeature_strandedness_constraint_minus_only:
      if (num_minus > 0 && num_plus == 0) return true;
    case eFeature_strandedness_constraint_plus_only:
      if (num_plus > 0 && num_minus == 0) return true;
    case eFeature_strandedness_constraint_at_least_one_minus:
      if (num_minus > 0) return true;
    case eFeature_strandedness_constraint_at_least_one_plus:
      if (num_plus > 0) return true;
    case eFeature_strandedness_constraint_no_minus:
      if (num_minus == 0) return true;
    case eFeature_strandedness_constraint_no_plus:
      if (num_plus == 0) return true;
    default: return false;
  }
};

EPub_type CSuspectRuleCheck :: GetPubMLStatus (const CPub& the_pub)
{
  EPub_type status = ePub_type_any;
  CImprint* imp = 0;
  switch (the_pub.Which()) {
    case CPub::e_Gen :
      if (the_pub.GetGen().CanGetCit() 
               && NStr::EqualNocase(the_pub.GetGen().GetCit(), "unpublished"))
           status = ePub_type_unpublished;
      else status = ePub_type_published;
      break;
    case CPub::e_Sub :
      status = ePub_type_submitter_block;
      break;
    case CPub::e_Article :
      {
        const CCit_art::C_From& from = the_pub.GetArticle().GetFrom();
        switch (from.Which()) {
          case CCit_art::C_From::e_Journal:
             imp = const_cast<CImprint*>(&(from.GetJournal().GetImp()));
             break;
          case CCit_art::C_From::e_Book:
             imp = const_cast<CImprint*>(&(from.GetBook().GetImp())); 
             break;
          case CCit_art::C_From::e_Proc:
             imp = const_cast<CImprint*>(&(from.GetProc().GetBook().GetImp()));
          default: break;
        };
        break;
      }
    case CPub::e_Journal :
      imp = const_cast<CImprint*>(&(the_pub.GetJournal().GetImp()));
      break;
    case CPub::e_Book :
      imp = const_cast<CImprint*>(&(the_pub.GetBook().GetImp()));
      break;
    case CPub::e_Man :
      imp = const_cast<CImprint*>(&(the_pub.GetMan().GetCit().GetImp()));
      break;
    case CPub::e_Patent :
      status = ePub_type_published;
      break;
    default :
      break;
  }
  if (imp) {
    if (!imp->CanGetPrepub()) status = ePub_type_published;  // imp->prepub == 0 ???
    else if (imp->GetPrepub() == CImprint::ePrepub_in_press) status = ePub_type_in_press;
    else if (imp->GetPrepub() == CImprint::ePrepub_submitted && the_pub.IsSub())
      status = ePub_type_submitter_block;
    else status = ePub_type_unpublished;
  }
  return status;
};

string CSuspectRuleCheck :: GetPubclassFromPub(const CPub& the_pub) 
{
  ostringstream output;
  output << the_pub.Which() << GetPubMLStatus(the_pub) 
         << (the_pub.IsArticle() ? the_pub.GetArticle().GetFrom().Which() :  0);
  
  strtmp = output.str();
  if (thisInfo.pub_class_quals.find(strtmp) != thisInfo.pub_class_quals.end()) 
     return thisInfo.pub_class_quals[strtmp];
  else {
     string map_str;
     ITERATE (Str2Str, it, thisInfo.pub_class_quals) {
       map_str = it->first;
       if (strtmp[0] == map_str[0]
                && (strtmp[1] == map_str[1] || strtmp[1] == '0' || map_str[1] == '0')
                && (strtmp[2] == map_str[2] || strtmp[2] == '0' || map_str[2] == '0'))
           return it->second;
     }
     return kEmptyStr;
  }
};

string CSuspectRuleCheck :: GetAuthorListString (const CAuth_list& auth_ls, const CString_constraint* str_cons, bool use_initials)
{
  string str(kEmptyStr);
  const CAuth_list::C_Names& names = auth_ls.GetNames();
  switch (names.Which()) {
    case CAuth_list::C_Names::e_Std:
      ITERATE (list <CRef <CAuthor> >, it, names.GetStd()) {
         //GetAuthorString( **it, use_initials);
         strtmp = CSeqEntry_test_on_pub :: GetAuthNameList(**it, use_initials);
         if (!strtmp.empty() && DoesStringMatchConstraint(strtmp, str_cons))
           str += strtmp + ", ";
      }
      break;
    case CAuth_list::C_Names::e_Ml:
      ITERATE (list <string>, it, names.GetMl())
        if (DoesStringMatchConstraint( *it, str_cons)) str += (*it) + ", ";
      break;
    case CAuth_list::C_Names::e_Str:
      ITERATE (list <string>, it, names.GetStr())
         if (DoesStringMatchConstraint( *it, str_cons)) str += (*it) + ", ";
    default: break;
  }

  if (!str.empty()) str = str.substr(0, str.size()-2);
  return str;
};

string CSuspectRuleCheck :: GetPubFieldFromAffil (const CAffil& affil, EPublication_field field, const CString_constraint* str_cons)
{
  string str(kEmptyStr);

  if (!affil.IsStd()) {
     str = affil.GetStr();
     if (field == ePublication_field_affiliation && DoesStringMatchConstraint(str, str_cons))
        return str;
     else return kEmptyStr;
  }
  const CAffil::C_Std& std = affil.GetStd();
  switch (field) {
    case ePublication_field_affiliation:
      if (std.CanGetAffil()) str = std.GetAffil();
      break;
    case ePublication_field_affil_div:
      if (std.CanGetDiv()) str = std.GetDiv();
      break;
    case ePublication_field_affil_city:
      if (std.CanGetCity()) str = std.GetCity();
      break;
    case ePublication_field_affil_sub:
      if (std.CanGetSub()) str = std.GetSub();
      break;
    case ePublication_field_affil_country:
      if (std.CanGetCountry()) str = std.GetCountry();
      break;
    case ePublication_field_affil_street:
      if (std.CanGetStreet()) str = std.GetStreet();
      break;
    case ePublication_field_affil_email:
      if (std.CanGetEmail()) str = std.GetEmail();
      break;
    case ePublication_field_affil_fax:
      if (std.CanGetFax()) str = std.GetFax();
      break;
    case ePublication_field_affil_phone:
      if (std.CanGetPhone()) str = std.GetPhone();
      break;
    case ePublication_field_affil_zipcode:
      if (std.CanGetPostal_code()) str = std.GetPostal_code();
      break;
    default: break;
  }
  if (!str.empty() && DoesStringMatchConstraint(str, str_cons)) return str;
  else return kEmptyStr;
};

string CSuspectRuleCheck :: PrintPartialOrCompleteDate(const CDate& date)
{
   string str(kEmptyStr);
   if (date.IsStr()) return date.GetStr();
   else {  // CDate_std
     if (date.GetStd().CanGetMonth()) str = thisInfo.months[date.GetStd().GetMonth()] + " ";
     str += date.GetStd().GetYear();
     return str;
   }
};

string CSuspectRuleCheck :: GetPubFieldFromImprint (const CImprint& imprint, EPublication_field field, const CString_constraint* str_cons)
{
  string str(kEmptyStr);
  switch (field) {
    case ePublication_field_volume:
      if (imprint.CanGetVolume()) str = imprint.GetVolume();
      break;
    case ePublication_field_issue:
      if (imprint.CanGetIssue()) str = imprint.GetIssue();
      break;
    case ePublication_field_pages:
      if (imprint.CanGetPages()) str = imprint.GetPages(); 
      break;
    case ePublication_field_date:
        str = PrintPartialOrCompleteDate (imprint.GetDate());
      break;
    default: break;
  }
  if (str.empty() || !DoesStringMatchConstraint(str, str_cons)) str = kEmptyStr;
  return str;
};

string CSuspectRuleCheck :: GetPubFieldFromCitJour(const CCit_jour& jour, EPublication_field field, const CString_constraint* str_cons)
{
  switch (field) {
    case ePublication_field_journal:
      return Get1stStringMatchFromTitle (jour.GetTitle(), str_cons);
    case ePublication_field_volume:
    case ePublication_field_issue:
    case ePublication_field_pages:
    case ePublication_field_date:
      return GetPubFieldFromImprint (jour.GetImp(), field, str_cons);
    default: return kEmptyStr;
  }
};

string CSuspectRuleCheck :: Get1stStringMatchFromTitle(const CTitle& title, const CString_constraint* str_cons)
{
    vector <string> title_strs;
    ITERATE (list <CRef <CTitle::C_E> >, it, title.Get()) 
           title_strs.push_back( title.GetTitle(**it) );
    string str = GetFirstStringMatch (title_strs, str_cons);
    title_strs.clear();
    return str;
};

string CSuspectRuleCheck :: GetPubFieldFromCitBook (const CCit_book& book, EPublication_field field, const CString_constraint* str_cons)
{
  const CAuth_list& auths = book.GetAuthors();
  switch (field) {
    case ePublication_field_title:
      return Get1stStringMatchFromTitle (book.GetTitle(), str_cons);
    case ePublication_field_authors:
      return GetAuthorListString (auths, str_cons);
    case ePublication_field_authors_initials:
      return GetAuthorListString (auths, str_cons, true);
    case ePublication_field_affiliation:
    case ePublication_field_affil_div:
    case ePublication_field_affil_city:
    case ePublication_field_affil_sub:
    case ePublication_field_affil_country:
    case ePublication_field_affil_street:
    case ePublication_field_affil_email:
    case ePublication_field_affil_fax:
    case ePublication_field_affil_phone:
    case ePublication_field_affil_zipcode:
      if (auths.CanGetAffil()) 
        return GetPubFieldFromAffil (auths.GetAffil(), field, str_cons);
      break;
    case ePublication_field_volume:
    case ePublication_field_issue:
    case ePublication_field_pages:
    case ePublication_field_date:
      return GetPubFieldFromImprint (book.GetImp(), field, str_cons);
    default: return kEmptyStr;
  }
  return kEmptyStr;
};

string CSuspectRuleCheck :: GetPubFieldFromPub (const CPub& the_pub, EPublication_field field, const CString_constraint* str_cons)
{
  if (field == ePublication_field_pub_class) return GetPubclassFromPub(the_pub);
  
  string str(kEmptyStr);
  switch (the_pub.Which()) {
    case CPub::e_Gen :
      {
        const CCit_gen& gen = the_pub.GetGen();
        switch (field) {
          case ePublication_field_cit:
            if (gen.CanGetCit() && gen.CanGetTitle() && !gen.GetTitle().empty())
                str = gen.GetTitle();
            break;
          case ePublication_field_authors:
            if (gen.CanGetAuthors()) 
                  return GetAuthorListString (gen.GetAuthors(), str_cons);
            break;
          case ePublication_field_authors_initials:
            if (gen.CanGetAuthors()) 
                  return GetAuthorListString (gen.GetAuthors(), str_cons, true);
            break;
          case ePublication_field_affiliation:
          case ePublication_field_affil_div:
          case ePublication_field_affil_city:
          case ePublication_field_affil_sub:
          case ePublication_field_affil_country:
          case ePublication_field_affil_street:
          case ePublication_field_affil_email:
          case ePublication_field_affil_fax:
          case ePublication_field_affil_phone:
          case ePublication_field_affil_zipcode:
            if (gen.CanGetAuthors() && gen.GetAuthors().CanGetAffil()) 
                 return GetPubFieldFromAffil (gen.GetAuthors().GetAffil(), field, str_cons);
            break;
          case ePublication_field_journal:
            if (gen.CanGetJournal())
                return Get1stStringMatchFromTitle(gen.GetJournal(), str_cons);
            break;
          case ePublication_field_volume:
            if (gen.CanGetVolume()) str = gen.GetVolume();
            break;
          case ePublication_field_issue:
            if (gen.CanGetIssue()) str = gen.GetIssue();
            break;
          case ePublication_field_pages:
            if (gen.CanGetPages()) str = gen.GetPages();
            break;
          case ePublication_field_date:
              if (gen.CanGetDate()) str = PrintPartialOrCompleteDate (gen.GetDate());
              break;
          case ePublication_field_serial_number:
            if (gen.CanGetSerial_number()) str = NStr::IntToString(gen.GetSerial_number());
            break;
          case ePublication_field_title:
            if (gen.CanGetTitle()) str = gen.GetTitle();
            break;
          default: break;
        }
        break;
      }
    case CPub::e_Sub :
      {
        const CCit_sub& sub = the_pub.GetSub();
        switch (field) {
          case ePublication_field_title:
            if (sub.CanGetDescr()) str = sub.GetDescr();
            break;
          case ePublication_field_authors:
            return GetAuthorListString (sub.GetAuthors(), str_cons);
          case ePublication_field_authors_initials:
            return GetAuthorListString (sub.GetAuthors(), str_cons, true);
          case ePublication_field_affiliation:
          case ePublication_field_affil_div:
          case ePublication_field_affil_city:
          case ePublication_field_affil_sub:
          case ePublication_field_affil_country:
          case ePublication_field_affil_street:
          case ePublication_field_affil_email:
          case ePublication_field_affil_fax:
          case ePublication_field_affil_phone:
          case ePublication_field_affil_zipcode:
            if (sub.GetAuthors().CanGetAffil())
                return GetPubFieldFromAffil (sub.GetAuthors().GetAffil(), field, str_cons);
            break;
          case ePublication_field_date:
            if (sub.CanGetDate()) str = PrintPartialOrCompleteDate (sub.GetDate());
            break;
          default: break;
        }
        break;
      }
    case CPub::e_Article :
      {
        const CCit_art& art = the_pub.GetArticle();
        switch (field) {
          case ePublication_field_title:
              if (art.CanGetTitle())
                return Get1stStringMatchFromTitle(art.GetTitle(), str_cons);
            break;
          case ePublication_field_authors:
            if (art.CanGetAuthors()) return GetAuthorListString (art.GetAuthors(), str_cons);
            break;
          case ePublication_field_authors_initials:
            if (art.CanGetAuthors()) 
                   return GetAuthorListString (art.GetAuthors(), str_cons, true); 
            break;
          case ePublication_field_affiliation:
          case ePublication_field_affil_div:
          case ePublication_field_affil_city:
          case ePublication_field_affil_sub:
          case ePublication_field_affil_country:
          case ePublication_field_affil_street:
          case ePublication_field_affil_email:
          case ePublication_field_affil_fax:
          case ePublication_field_affil_phone:
          case ePublication_field_affil_zipcode:
            if (art.CanGetAuthors() && art.GetAuthors().CanGetAffil())
               return GetPubFieldFromAffil (art.GetAuthors().GetAffil(), field, str_cons);
            break;
          default:
            if (art.GetFrom().IsJournal()) 
                 return GetPubFieldFromCitJour (art.GetFrom().GetJournal(), field, str_cons);
            else if (art.GetFrom().IsBook())
                 return GetPubFieldFromCitBook (art.GetFrom().GetBook(), field, str_cons);
            break;
        }
        break;
      }
    case CPub::e_Journal:
      return GetPubFieldFromCitJour (the_pub.GetJournal(), field, str_cons);
    case CPub::e_Book :
      return GetPubFieldFromCitBook(the_pub.GetBook(), field, str_cons);
    case CPub::e_Man :
      return GetPubFieldFromCitBook (the_pub.GetMan().GetCit(), field, str_cons);
    case CPub::e_Patent :
      {
        const CCit_pat& pat = the_pub.GetPatent();
        const CAuth_list& auths = pat.GetAuthors();
        switch (field) {
          case ePublication_field_title:
            str = pat.GetTitle();
            break;
          case ePublication_field_authors:
            return GetAuthorListString (auths, str_cons);
          case ePublication_field_authors_initials:
            return GetAuthorListString (auths, str_cons, true);
          case ePublication_field_affiliation:
          case ePublication_field_affil_div:
          case ePublication_field_affil_city:
          case ePublication_field_affil_sub:
          case ePublication_field_affil_country:
          case ePublication_field_affil_street:
          case ePublication_field_affil_email:
          case ePublication_field_affil_fax:
          case ePublication_field_affil_phone:
          case ePublication_field_affil_zipcode:
            if (auths.CanGetAffil())
              return GetPubFieldFromAffil (auths.GetAffil(), field, str_cons);
            break;
          default: break;
        }
        break;
      }
    case CPub::e_Pmid:
      if (field == ePublication_field_pmid) return NStr::IntToString(the_pub.GetPmid());
      break;
    default: break;
  }
  if (str.empty() || !DoesStringMatchConstraint(str, str_cons)) str = kEmptyStr;
  return str;
};

bool CSuspectRuleCheck :: DoesPubFieldMatch (const CPubdesc& pub_desc, const CPub_field_constraint& field)
{
  const CString_constraint& str_cons = field.GetConstraint();
  EPublication_field pub_field = field.GetField();
  if (str_cons.GetNot_present()) {
    ITERATE (list <CRef <CPub> >, it, pub_desc.GetPub().Get()) {
       strtmp = GetPubFieldFromPub(**it, pub_field, &str_cons);
       if (strtmp.empty()) return false;
    }
    return true;
  } 
  else {
    ITERATE (list <CRef <CPub> >, it, pub_desc.GetPub().Get()) {
       strtmp = GetPubFieldFromPub (**it, pub_field, &str_cons);
       if (strtmp.empty()) return true;
    }  
    return false;
   
  }
};

bool CSuspectRuleCheck :: DoesPubFieldSpecialMatch (const CPubdesc& pub_desc, const CPub_field_special_constraint& field)
{
  const CPub_field_special_constraint_type& fcons_tp = field.GetConstraint();
  ITERATE (list < CRef <CPub> >, it, pub_desc.GetPub().Get()) {
    strtmp = GetPubFieldFromPub(**it, field.GetField());
    if (!strtmp.empty()) {
      switch (fcons_tp.Which()) {
         case CPub_field_special_constraint_type::e_Is_present: return true;
         case CPub_field_special_constraint_type::e_Is_not_present: return false;
         case CPub_field_special_constraint_type::e_Is_all_caps:
           if (!CDiscRepUtil::IsAllCaps(strtmp)) return false;
         case CPub_field_special_constraint_type::e_Is_all_lower:
           if (!CDiscRepUtil::IsAllLowerCase (strtmp)) return false;
         case CPub_field_special_constraint_type::e_Is_all_punct:
           if (!CDiscRepUtil::IsAllPunctuation (strtmp)) return false;
         default: break;
      }
    }
  } 

  switch (fcons_tp.Which()) {
    case CPub_field_special_constraint_type::e_Is_present: return false;
    default: return true;
  }
};

bool CSuspectRuleCheck :: IsPublicationConstraintEmpty (const CPublication_constraint& constraint)
{
  if (constraint.GetType() == ePub_type_any && !constraint.CanGetSpecial_field()
          && (!constraint.CanGetField()
                   || IsStringConstraintEmpty(&(constraint.GetField().GetConstraint()))))
                        
        return true;
  return false;
};

bool CSuspectRuleCheck :: DoesPubMatchPublicationConstraint (const CPubdesc& pub_desc, const CPublication_constraint& pub_cons)
{
  if (IsPublicationConstraintEmpty (pub_cons)) return true;

  bool type_ok = true;
  if (pub_cons.GetType() != ePub_type_any) {
    type_ok = false;
    ITERATE (list <CRef <CPub> >, it, pub_desc.GetPub().Get()) {
      if (GetPubMLStatus(**it) == pub_cons.GetType()) {
          type_ok = true; break;
      }
    }
  }
  if (type_ok) {
    return ((!pub_cons.CanGetField() || DoesPubFieldMatch (pub_desc, pub_cons.GetField()))
           && (!pub_cons.CanGetSpecial_field() 
                  || DoesPubFieldSpecialMatch (pub_desc, pub_cons.GetSpecial_field())));
  }
  return false;
};

// may not be needed
/*
bool DoesObjectMatchPublicationConstraint (const CSeq_feat& seq_feat, const CPublication_constraint& pub_cons)
{
  if (IsPublicationConstraintEmpty (pub_cons)) return true;

  if (seq_feat.GetData.IsPub())
        rval = DoesPubMatchPublicationConstraint (sfp->data.value.ptrvalue, constraint);
***
    case OBJ_SEQDESC:
      sdp = (SeqDescrPtr) data;
      if (sdp->choice == Seq_descr_pub) {
        rval = DoesPubMatchPublicationConstraint (sdp->data.ptrvalue, constraint);
      }
      break;
***
  return rval;
}
*/

// should have different version for various data type/choice
bool CSuspectRuleCheck :: DoesObjectMatchMolinfoFieldConstraint (const CSeq_feat& seq_feat, const CMolinfo_field_constraint& mol_cons)
{
  if (!m_bioseq_hl) return false;
  const CMolInfo* mol = GetMolInfo(m_bioseq_hl);
  bool rval = false;
  const CMolinfo_field& mol_field = mol_cons.GetField();
  switch (mol_field.Which()) {
      case CMolinfo_field::e_Molecule:
        if (!mol && mol_field.GetMolecule() == eMolecule_type_unknown) {
            return true;
        } else if (mol && mol->GetBiomol() == thisInfo.moltp_biomol[mol_field.GetMolecule()]) {
            return true; 
        }
        break;
      case CMolinfo_field::e_Technique:
        if (!mol && mol_field.GetTechnique() == eTechnique_type_unknown) {
            return true;
        } else if (mol && mol->GetTech() == thisInfo.techtp_mitech[mol_field.GetTechnique()]) {
            return true;
        }
        break;
      case CMolinfo_field::e_Completedness:
        {
           CMolInfo::ECompleteness mi_cmpl = (CMolInfo::ECompleteness)mol->GetCompleteness();
           ECompletedness_type cons_cmpl = mol_field.GetCompletedness();
           if (!mol && cons_cmpl == eCompletedness_type_unknown) return true;
           else if (mol &&( (unsigned)mi_cmpl == (unsigned)cons_cmpl
                           || (mi_cmpl == CMolInfo::eCompleteness_other
                                         && cons_cmpl == eCompletedness_type_other) ))
                  return true;
           // CompletenessFromCompletednessType (constraint->field->data.intvalue)) 
           break;
        }
      case CMolinfo_field::e_Mol_class:
        {
           if (m_bioseq_hl.CanGetInst_Mol()) {
             CSeq_inst::EMol inst_mol = m_bioseq_hl.GetInst_Mol();
             EMolecule_class_type cons_class = mol_field.GetMol_class();
             if ( (unsigned)inst_mol == (unsigned) cons_class
                    || (inst_mol == CSeq_inst::eMol_other
                             && cons_class == eMolecule_class_type_other) )
                 return true;
           }
           break;
        }
      case CMolinfo_field::e_Topology:
        {
          if (m_bioseq_hl.CanGetInst_Topology()) {
            CSeq_inst::ETopology inst_top = m_bioseq_hl.GetInst_Topology();
            ETopology_type cons_top = mol_field.GetTopology();
            if ( (unsigned)inst_top == (unsigned)cons_top
                   || (inst_top == CSeq_inst::eTopology_other
                             && cons_top == eTopology_type_other) )
                 return true;
          }
          break;
        }
      case CMolinfo_field::e_Strand:
        {
           if (m_bioseq_hl.CanGetInst_Strand()) {
              CSeq_inst::EStrand inst_str = m_bioseq_hl.GetInst_Strand();
              EStrand_type cons_str = mol_field.GetStrand();
              if ( (unsigned)inst_str == (unsigned)cons_str
                      || (inst_str == CSeq_inst::eStrand_other
                            && cons_str == eStrand_type_other) )
                 return true;
           }
           break;
        }
      default: break;
  }
  if (mol_cons.GetIs_not()) rval = !rval;
  return rval;
};

string CSuspectRuleCheck :: GetConstraintFieldFromObject (const CSeq_feat& seq_feat, const CField_type& field, const CString_constraint* str_cons)
{
  string str(kEmptyStr);
  switch (field.Which()) {
    case CField_type::e_Source_qual:
      {
         const CBioSource* biosrc = GetBioSource(m_bioseq_hl);
         if (biosrc) 
              return GetSrcQualValue4FieldType (*biosrc, field.GetSource_qual(), str_cons);
         break;
      }
    case CField_type::e_Feature_field:
      return GetQualFromFeature(seq_feat, field.GetFeature_field(), str_cons);
    case CField_type::e_Rna_field:
      return GetRNAQualFromFeature (seq_feat,  field.GetRna_field(), str_cons);
/*
        case OBJ_SEQDESC:
        case OBJ_BIOSEQ:
          bsp = GetSequenceForObject (choice, data);
          if (bsp != NULL) {
            if (rq->type == NULL || rq->type->choice == RnaFeatType_any) {
              feat_choice = SEQFEAT_RNA;
              subtype = 0;
            } else {
              feat_choice = 0;
              subtype = GetFeatdefFromFeatureType(GetFeatureTypeForRnaType(rq->type->choice));
            }
        
            for (sfp = SeqMgrGetNextFeature (bsp, NULL, feat_choice, subtype, &fcontext);
                  rval == NULL && sfp != NULL;
                  sfp = SeqMgrGetNextFeature (bsp, sfp, feat_choice, subtype, &fcontext)) {
              rval = GetRNAQualFromFeature (sfp, rq, scp, NULL);
            }
          }
          break;
*/
      break;
    case CField_type::e_Cds_gene_prot:
      {
        CRef <CFeature_field> 
               ff = FeatureFieldFromCDSGeneProtField (field.GetCds_gene_prot());
        return  GetQualFromFeature (seq_feat, const_cast<CFeature_field&>(*ff), str_cons);
      }
      break;
    case CField_type::e_Molinfo_field:
      if (m_bioseq_hl) {
        str = GetSequenceQualFromBioseq (field.GetMolinfo_field());
        if (!str.empty() && str_cons && !DoesStringMatchConstraint (str, str_cons)) 
              str = kEmptyStr;
      }
      break;
    case CField_type::e_Misc:
      if (m_bioseq_hl) return GetFieldValueForObjectEx (field, *str_cons);
      break;
    default: break;
  }
          
  return str;
}

bool CSuspectRuleCheck :: DoesCodingRegionMatchTranslationConstraint(const CSeq_feat& feat, const CTranslation_constraint& trans_cons)
{
  string actual(kEmptyStr), translation(kEmptyStr);
  EMatch_type_constraint int_stops = trans_cons.GetInternal_stops();
  bool has_num_mismatches = trans_cons.CanGetNum_mismatches();
  if (has_num_mismatches) {
    if (feat.CanGetProduct()) {
       CBioseq_Handle 
           actual_prot = GetBioseqFromSeqLoc(feat.GetProduct(), *thisInfo.scope);
       if (actual_prot) {
           CSeqVector 
             seq_vec =m_bioseq_hl.GetSeqVector(CBioseq_Handle::eCoding_Iupac, eNa_strand_plus);
           seq_vec.GetSeqData(0, actual_prot.GetInst_Length(), actual);
       }
    }
  }

  bool rval = true;
  if (!actual.empty()) {
      ITERATE (list <CRef <CString_constraint> >, it, trans_cons.GetActual_strings().Get()) {
         rval = DoesStringMatchConstraint(actual, &(**it));
         if (!rval) break;
      }
  }

  size_t stop;
  bool alt_start;
  if (rval) {
    if (int_stops != eMatch_type_constraint_dont_care || has_num_mismatches)
      CSeqTranslator::Translate(feat,*thisInfo.scope, translation, true, false,&alt_start);
    if (!translation.empty()) {
       ITERATE (list <CRef <CString_constraint> >, it, trans_cons.GetTransl_strings().Get()) {
          rval = DoesStringMatchConstraint(translation, &(**it));
          if (!rval) break;
       }
    }

    if (rval && int_stops != eMatch_type_constraint_dont_care) {
      stop = translation.find('*');
      if (stop != string::npos && stop != translation.size()-1) {
        if (int_stops == eMatch_type_constraint_no) rval = false;
      } 
      else if (int_stops == eMatch_type_constraint_yes) rval = false;
    }
  }

  unsigned translation_len = translation.size();
  unsigned actual_len = actual.size();
  int num, comp_len;
  if (rval && has_num_mismatches) {
    stop = translation.find_last_of('*');
    if (stop != string::npos && stop == translation_len - 1) translation_len--;
    stop = actual.find_last_of('*');
    if (stop != string::npos && stop == actual_len - 1) actual_len--;
    if (translation_len > actual_len) {
      num = translation_len - actual_len;
      comp_len = actual_len;
    } else {
      num = actual_len - translation_len;
      comp_len = translation_len;
    }

    string cp1(actual), cp2(translation);
    const CQuantity_constraint& quan_cons = trans_cons.GetNum_mismatches();
    for (int pos = 0; pos < comp_len && rval; pos++) {
        if (actual[pos] != translation[pos]) {
          num++;
          if (quan_cons.IsEquals() && num > quan_cons.GetEquals()) rval = false;
          else if (quan_cons.IsLess_than() && num >= quan_cons.GetLess_than()) rval = false;
        }
    }
    if (rval) {
      if (quan_cons.IsGreater_than() && num <= quan_cons.GetGreater_than()) {
          rval = false;
      }
      else if (quan_cons.IsEquals() && num != quan_cons.GetEquals()) {
         rval = false;
      }
      else if (quan_cons.IsLess_than() && num >= quan_cons.GetLess_than()) {
          rval = false;
      }
    }
  }
  return rval;
}

string CSuspectRuleCheck :: SkipWeasel(const string& str)
{
  if (str.empty()) {
    return kEmptyStr;
  }
  string ret_str(kEmptyStr);
  arr.clear();
  arr = NStr::Tokenize(str, " ", arr);
  if (arr.size() == 1) {
     return str;
  }
  int i;
  unsigned len, len_w;
  bool find_w;
  for (i=0; i< (int)(arr.size() - 1); i++) {
    len = arr[i].size();
    find_w = false;
    ITERATE (vector <string>, it, thisInfo.weasels) {
       len_w = (*it).size(); 
       if (len != len_w || !NStr::EqualNocase(arr[i], 0, len, *it)) {
           continue;
       }
       else { 
         find_w = true;
         break;
       }
    }
    if (!find_w) {
         break;
    }
  }
  for (i; i< (int)(arr.size()-1); i++) {
          ret_str += arr[i] + ' ';
  }
  ret_str += arr[arr.size()-1];
  arr.clear();
  return (ret_str);
};

// c CaseNCompare()
bool CSuspectRuleCheck :: CaseNCompareEqual(string str1, string str2, unsigned len1, bool case_sensitive)
{
   if (!len1) return false;
   string comp_str1, comp_str2;
   comp_str1 = CTempString(str1).substr(0, len1);
   comp_str2 = CTempString(str2).substr(0, len1);
   if (case_sensitive) {
       return (comp_str1 == comp_str2);
   }
   else {
     return (NStr::EqualNocase(comp_str1, 0, len1, comp_str2));
   }
};

bool CSuspectRuleCheck :: AdvancedStringCompare(const string& str, const string& str_match, const CString_constraint* str_cons, bool is_start, unsigned* ini_target_match_len)
{
  if (str.empty()) return false;
  if (!str_cons || str_match.empty()) return true;

  size_t pos_match = 0, pos_str = 0;
  bool wd_case, whole_wd, word_start_m, word_start_s;
  bool match = true, recursive_match = false;
  unsigned len_m = str_match.size(), len_s = str.size(), target_match_len=0;
  string cp_m, cp_s;
  bool ig_space = str_cons->GetIgnore_space();
  bool ig_punct = str_cons->GetIgnore_punct();
  bool str_case = str_cons->GetCase_sensitive();
  EString_location loc = str_cons->GetMatch_location();
  unsigned len1, len2;
  char ch1, ch2;
  vector <string> word_word;
  bool has_word = false;
  ITERATE (list <CRef <CWord_substitution> >, 
             it, 
             str_cons->GetIgnore_words().Get()) {
      strtmp = ((*it)->CanGetWord()) ? (*it)->GetWord() : kEmptyStr;
      word_word.push_back(strtmp);
      if (!strtmp.empty()) has_word = true;
  }

  unsigned i;
  while (match && pos_match < len_m && pos_str < len_s && !recursive_match) {
    cp_m = CTempString(str_match).substr(pos_match);
    cp_s = CTempString(str).substr(pos_str);

    /* first, check to see if we're skipping synonyms */
    i=0;
    if (has_word) {
      ITERATE (list <CRef <CWord_substitution> >, 
               it, 
               str_cons->GetIgnore_words().Get()) {
        wd_case = (*it)->GetCase_sensitive();
        whole_wd = (*it)->GetWhole_word();
        len1 = word_word[i].size();
        //text match
        if (len1 && CaseNCompareEqual(word_word[i], cp_m, len1,wd_case)){
           word_start_m 
               = (!pos_match && is_start) || !isalpha(str_match[pos_match-1]);
           ch1 = (cp_m.size() < len1) ? ' ' : cp_m[len1];
           if (!whole_wd || (!isalpha(ch1) && word_start_m)) { // whole word mch
              if ( !(*it)->CanGetSynonyms() ) { 
                 if (AdvancedStringCompare(cp_s, 
                                           CTempString(cp_m).substr(len1), 
                                           str_cons, 
                                           word_start_m, 
                                           &target_match_len)) {
                    recursive_match = true;
                 }
              }
              else {
                ITERATE (list <string>, sit, (*it)->GetSynonyms()) {
                  len2 = (*sit).size();

                    // text match
                  if (CaseNCompareEqual(*sit, cp_s, len2, wd_case)) {
                    word_start_s 
                          = (!pos_str && is_start) || !isalpha(str[pos_str-1]);
                    ch2 = (cp_s.size() < len2) ? ' ' : cp_s[len2];
                    // whole word match
                    if (!whole_wd || (!isalpha(ch2) && word_start_s)) {
                       if (AdvancedStringCompare(CTempString(cp_s).substr(len2),
                                                 CTempString(cp_m).substr(len1),
                                                 str_cons, 
                                                 word_start_m & word_start_s, 
                                                 &target_match_len)){
                            recursive_match = true;
                       }
                    }
                  }
                }
              }
           }
        }
      }
    }

    if (!recursive_match) {
      if (CaseNCompareEqual(cp_m, cp_s, 1, str_case)) {
           pos_match++;
           pos_str++;
           target_match_len++;
      } 
      else if ( ig_space && (isspace(cp_m[0]) || isspace(cp_s[0])) ) {
        if ( isspace(cp_m[0]) ) pos_match++;
        if ( isspace(cp_s[0]) ) {
             pos_str++;
             target_match_len++;
        }
      }
      else if (ig_punct && ( ispunct(cp_m[0]) || ispunct(cp_s[0]) )) {
        if ( ispunct(cp_m[0]) ) pos_match++;
        if ( ispunct(cp_s[0]) ) {
             pos_str++;
             target_match_len++;
        }
      }
      else match = false;
    }
  }

  if (match && !recursive_match) {
    while ( pos_str < str.size() 
             && ((ig_space && isspace(str[pos_str])) 
             || (ig_punct && ispunct(str[pos_str]))) ){
       pos_str++;
       target_match_len++;
    }
    while ( pos_match < str_match.size()
              && ( (ig_space && isspace(str_match[pos_match])) 
                     || (ig_punct && ispunct(str_match[pos_match])) ) ) {
         pos_match++;
    }

    if (pos_match < str_match.size()) {
         match = false;
    }
    else if ( (loc == eString_location_ends || loc == eString_location_equals) 
                 && pos_str < len_s) {
         match = false;
    }
    else if (str_cons->GetWhole_word() 
                && (!is_start || (pos_str < len_s && isalpha (str[pos_str]))) ){
             match = false;
    }
  }
  if (match && ini_target_match_len) {
         *ini_target_match_len += target_match_len;
  }

  return match;
};

bool CSuspectRuleCheck :: AdvancedStringMatch(const string& str, const CString_constraint* str_cons)
{
  if (!str_cons) return true;
  bool rval = false;
  string 
    match_text 
      = (str_cons->CanGetMatch_text()) ? str_cons->GetMatch_text() : kEmptyStr;

  if (AdvancedStringCompare (str, match_text, str_cons, true)) {
       return true;
  }
  else if (str_cons->GetMatch_location() == eString_location_starts 
                || str_cons->GetMatch_location() == eString_location_equals) {
           return false;
  }
  else {
    size_t pos = 1;
    unsigned len = str.size();
    while (!rval && pos < len) {
      if (str_cons->GetWhole_word()) {
          while (pos < len && isalpha (str[pos-1])) pos++;
      }
      if (pos < len) {
        if (AdvancedStringCompare (CTempString(str).substr(pos), 
                                   match_text, 
                                   str_cons, 
                                   true)) {
            rval = true;
        }
        else {
            pos++;
        }
      }
    }
  }
  return rval;
};

bool CSuspectRuleCheck :: DisallowCharacter(const char ch, bool disallow_slash)
{
  if (isalpha (ch) || isdigit (ch) || ch == '_' || ch == '-') return true;
  else if (disallow_slash && ch == '/') return true;
  else return false;
};

bool CSuspectRuleCheck :: IsWholeWordMatch(const string& start, const size_t& found, const unsigned& match_len, bool disallow_slash)
{
  bool rval = true;
  unsigned after_idx;

  if (!match_len) rval = true;
  else if (start.empty() || found == string::npos) rval = false;
  else
  {
    if (found) {
      if (DisallowCharacter (start[found-1], disallow_slash)) return false;
    }
    after_idx = found + match_len;
    if ( after_idx < start.size() 
           && DisallowCharacter(start[after_idx], disallow_slash)) rval = false;
  }
  return rval;
};

bool CSuspectRuleCheck :: GetSpanFromHyphenInString(const string& str, const size_t& hyphen, string& first, string& second)
{
   if (!hyphen) return false;

   /* find range start */
   size_t cp = str.substr(0, hyphen-1).find_last_not_of(' ');   
   if (cp != string::npos) cp = str.substr(0, cp).find_last_not_of(" ,;"); 
   if (cp == string::npos) cp = 0;

   unsigned len = hyphen - cp;
   first = CTempString(str).substr(cp, len);
   NStr::TruncateSpacesInPlace(first);
 
   /* find range end */
   cp = str.find_first_not_of(' ', hyphen+1);
   if (cp != string::npos) cp = str.find_first_not_of(" ,;");
   if (cp == string::npos) cp = str.size() -1;   

   len = cp - hyphen;
   if (!isspace (str[cp])) len--;
   second = CTempString(str).substr(hyphen+1, len);
   NStr::TruncateSpacesInPlace(second);

   bool rval = true;
   if (first.empty() || second.empty()) rval = false;
   else if (!isdigit (first[first.size() - 1]) || !isdigit (second[second.size() - 1])) {
      /* if this is a span, then neither end point can end with anything other than a number */
      rval = false;
   }
   if (!rval) first = second = "";
   return rval;
};

bool CSuspectRuleCheck :: StringIsPositiveAllDigits(const string& str)
{
   if (str.find_first_not_of(CDiscRepUtil::digit_str) != string::npos) return false;
   else return true;
};

bool CSuspectRuleCheck :: IsStringInSpan(const string& str, const string& first, const string& second)
{
  string new_first, new_second, new_str;
  if (str.empty()) return false;
  else if (str == first || str == second) return true;
  else if (first.empty() || second.empty()) return false;

  int str_num, first_num, second_num;
  str_num = first_num = second_num = 0;
  bool rval = false;
  size_t prefix_len;
  string comp_str1, comp_str2;
  if (StringIsPositiveAllDigits (first)) {
    if (StringIsPositiveAllDigits (str) && StringIsPositiveAllDigits (second)) {
      str_num = NStr::StringToUInt (str);
      first_num = NStr::StringToUInt (first);
      second_num = NStr::StringToUInt (second);
      if ((str_num > first_num && str_num < second_num)
                            || (str_num > second_num && str_num < first_num))
          rval = true;
    }
  } 
  else if (StringIsPositiveAllDigits(second)) {
    prefix_len = first.find_first_of(CDiscRepUtil::digit_str) + 1;

    new_str = CTempString(str).substr(prefix_len - 1);
    new_first = CTempString(first).substr(prefix_len - 1);
    comp_str1 = CTempString(str).substr(0, prefix_len);
    comp_str2 = CTempString(first).substr(0, prefix_len);
    if (comp_str1 == comp_str2
        && StringIsPositiveAllDigits (new_str)
        && StringIsPositiveAllDigits (new_first)) {
      first_num = NStr::StringToUInt(new_first);
      second_num = NStr::StringToUInt (second);
      str_num = NStr::StringToUInt (str);
      if ((str_num > first_num && str_num < second_num)
                 || (str_num > second_num && str_num < first_num))
        rval = true;
    }
  } 
  else {
    /* determine length of prefix */
    prefix_len = 0;
    while (prefix_len < first.size() && prefix_len < second.size() 
              && first[prefix_len] == second[prefix_len])
       prefix_len ++;
    prefix_len ++;

    comp_str1 = CTempString(str).substr(0, prefix_len);
    comp_str2 = CTempString(first).substr(0, prefix_len);
    if (prefix_len <= first.size() && prefix_len <= second.size()
        && isdigit (first[prefix_len-1]) && isdigit (second[prefix_len-1])
        && comp_str1 == comp_str2) {
      new_first = CTempString(first).substr(prefix_len);
      new_second = CTempString(second).substr(prefix_len);
      new_str = CTempString(str).substr(prefix_len);
      if (StringIsPositiveAllDigits (new_first) && StringIsPositiveAllDigits (new_second) 
                   && StringIsPositiveAllDigits (new_str)) {
        first_num = NStr::StringToUInt(new_first);
        second_num = NStr::StringToUInt (new_second);
        str_num = NStr::StringToUInt (new_str);
        if ((str_num > first_num && str_num < second_num)
            || (str_num > second_num && str_num < first_num)) {
          rval = true;
        }
      } else {
        /* determine whether there is a suffix */
        size_t idx1, idx2, idx_str;
        string suf1, suf2, sub_str;
        idx1 = first.find_first_not_of(CDiscRepUtil::digit_str);
        suf1 = CTempString(first).substr(prefix_len + idx1);
        idx2 = second.find_first_not_of(CDiscRepUtil::digit_str);
        suf2 = CTempString(second).substr(prefix_len + idx2);
        idx_str = str.find_first_not_of(CDiscRepUtil::digit_str);
        sub_str = CTempString(str).substr(prefix_len + idx_str);
        if (suf1 == suf2 && suf1 == sub_str) {
          /* suffixes match */
          first_num = NStr::StringToUInt(CTempString(first).substr(prefix_len, idx1));
          second_num = NStr::StringToUInt(CTempString(second).substr(prefix_len, idx2));
          str_num = NStr::StringToUInt(CTempString(str).substr(prefix_len, idx_str));
          if ((str_num > first_num && str_num < second_num)
                           || (str_num > second_num && str_num < first_num)) {
            rval = true;
          }
        }
      }
    }
  }
  return rval;
};

bool CSuspectRuleCheck :: IsStringInSpanInList (const string& str, const string& list)
{
  if (list.empty() || str.empty()) return false;

  size_t idx = str.find_first_not_of(CDiscRepUtil::alpha_str);
  if (idx == string::npos) return false;

  idx = CTempString(str).substr(idx).find_first_not_of(CDiscRepUtil::digit_str);

  /* find ranges */
  size_t hyphen = list.find('-');
  bool rval = false;
  string range_start, range_end;
  while (hyphen != string::npos && !rval) {
    if (!hyphen) hyphen = CTempString(list).substr(1).find('-');
    else {
      if (GetSpanFromHyphenInString (list, hyphen, range_start, range_end)) {
        if (IsStringInSpan (str, range_start, range_end)) rval = true;
      }
      hyphen = list.find('-', hyphen + 1);
    }
  }
  return rval;
};

string CSuspectRuleCheck :: StripUnimportantCharacters(const string& str, bool strip_space, bool strip_punct)
{
   if (str.empty()) {
      return kEmptyStr;
   }
   string result;
   result.reserve(str.size());
   string::const_iterator it = str.begin();
   do {
      if ((strip_space && isspace(*it)) || (strip_punct && ispunct(*it)));
      else result += *it;
   } while (++it != str.end());

   return result;
};

bool CSuspectRuleCheck :: DoesSingleStringMatchConstraint(const string& str, const CString_constraint* str_cons)
{
  bool rval = false;
  string tmp_match;
  CString_constraint tmp_cons;

  string this_str(str);
  if (!str_cons) return true;
  if (str.empty()) return false;
  if (IsStringConstraintEmpty(str_cons)) rval = true;
  else {
    if (str_cons->GetIgnore_weasel()) {
         this_str = SkipWeasel(str);
    }
    if (str_cons->GetIs_all_caps() && !CDiscRepUtil::IsAllCaps(this_str)) {
         rval = false;
    }
    else if (str_cons->GetIs_all_lower() 
                 && !CDiscRepUtil::IsAllLowerCase(this_str)) {
               rval = false;
    }
    else if (str_cons->GetIs_all_punct() 
               && !CDiscRepUtil::IsAllPunctuation(this_str)) {
               rval = false;
    }
    else if (!str_cons->CanGetMatch_text() ||str_cons->GetMatch_text().empty()){
        rval = true; 
    }
    else {
      strtmp = Blob2Str(*str_cons);
      Str2Blob(strtmp, tmp_cons);
      tmp_match = tmp_cons.GetMatch_text();
      if (str_cons->GetIgnore_weasel()) {
            tmp_cons.SetMatch_text(SkipWeasel(str_cons->GetMatch_text()));
      }
      if ((str_cons->GetMatch_location() != eString_location_inlist) 
                && str_cons->CanGetIgnore_words()){
          tmp_cons.SetMatch_text(tmp_match);
          rval = AdvancedStringMatch(str, &tmp_cons);
      }
      else {
        string search(this_str), pattern(tmp_cons.GetMatch_text());
        bool ig_space = str_cons->GetIgnore_space();
        bool ig_punct = str_cons->GetIgnore_punct();
        if ( (str_cons->GetMatch_location() != eString_location_inlist)
               && (ig_space || ig_punct)) {
           search = StripUnimportantCharacters(search, ig_space, ig_punct);
           pattern = StripUnimportantCharacters(pattern, ig_space, ig_punct);
        } 

        size_t pFound;
        pFound = (str_cons->GetCase_sensitive())?
                       search.find(pattern) : NStr::FindNoCase(search, pattern);
        switch (str_cons->GetMatch_location()) 
        {
          case eString_location_contains:
            if (string::npos == pFound) {
               rval = false;
            }
            else if (str_cons->GetWhole_word()) {
                rval = IsWholeWordMatch (search, pFound, pattern.size());
                while (!rval && pFound != string::npos) {
	           pFound = (str_cons->GetCase_sensitive()) ?
                              search.find(pattern, pFound+1):
                                NStr::FindNoCase(search, pattern, pFound+1);
                   rval = (pFound != string::npos)? 
                            IsWholeWordMatch (search, pFound, pattern.size()):
                              false;
                }
            }
            else {
                 rval = true;
            }
            break;
          case eString_location_starts:
            if (!pFound) {
              rval = (str_cons->GetWhole_word()) ?
                       IsWholeWordMatch (search, pFound, pattern.size()):
                         true;
            }
            break;
          case eString_location_ends:
            while (pFound != string::npos && !rval) {
              if ( (pFound + pattern.size()) == search.size()) {
                rval = (str_cons->GetWhole_word())? 
                        IsWholeWordMatch (search, pFound, pattern.size()): true;
                /* stop the search, we're at the end of the string */
                pFound = string::npos;
              }
              else {
  	        pFound = (str_cons->GetCase_sensitive()) ?
                             pFound = search.find(pattern, pFound+1):
                               NStr::FindNoCase(search, pattern, pFound+1);
              }
            }
            break;
          case eString_location_equals:
            if (str_cons->GetCase_sensitive() && (search==pattern) ) {
               rval= true; 
            }
            else if (!str_cons->GetCase_sensitive() 
                        && NStr::EqualNocase(search, pattern) ) {
                  rval = true;
            }
            break;
          case eString_location_inlist:
            pFound = (str_cons->GetCase_sensitive())?
                       pattern.find(search) : NStr::FindNoCase(pattern, search);
            if (pFound == string::npos) {
                  rval = false;
            }
            else {
              rval = IsWholeWordMatch(pattern, pFound, search.size(), true);
              while (!rval && pFound != string::npos) {
                pFound = (str_cons->GetCase_sensitive())?
                          CTempString(pattern).substr(pFound + 1).find(search):
                             NStr::FindNoCase(
                               CTempString(pattern).substr(pFound + 1), search);
                if (pFound != string::npos) {
                    rval = IsWholeWordMatch (pattern, pFound, str.size(), true);
                }
              }
            }
            if (!rval) {
              /* look for spans */
              rval = IsStringInSpanInList (search, pattern);
            }
            break;
        }
      }
    }
  }
  return rval;
};

bool CSuspectRuleCheck :: DoesStringMatchConstraint(const string& str, const CString_constraint* constraint)
{
  if (!constraint) return true;
  bool rval = DoesSingleStringMatchConstraint (str, constraint);
  if (constraint->GetNot_present()) return (!rval);
  else return rval;
};

bool CSuspectRuleCheck :: x_DoesStrContainPlural(const string& word, char last_letter, char second_to_last_letter, char next_letter)
{
   unsigned len = word.size();
   if (last_letter == 's') {
      if (len >= 5  && CTempString(word).substr(len-5) == "trans") return false; // not plural;
      else if (len > 3) {
        if (second_to_last_letter != 's' && second_to_last_letter != 'i'
                                         && second_to_last_letter != 'u'
                                         && next_letter == ',')
           return true;
      }
   }

   return false;
};

bool CSuspectRuleCheck :: StringMayContainPlural(const string& str)
{
  char last_letter, second_to_last_letter, next_letter;
  bool may_contain_plural = false;
  string word_skip = " ,";
  unsigned len;

  if (str.empty()) return false;
  arr.clear();
  arr = NStr::Tokenize(str, " ,", arr, NStr::eMergeDelims);
  if (arr.size() == 1) { // doesn't have ', ', or the last char is ', '
     len = arr[0].size();
     if (len == 1) return false;
     last_letter = arr[0][len-1];
     second_to_last_letter = arr[0][len-2]; 
     if (len == str.size()) next_letter = ',';
     else next_letter = str[len];
     may_contain_plural 
         = x_DoesStrContainPlural(arr[0], last_letter, second_to_last_letter, next_letter);
  }
  else {
    strtmp = str;
    size_t pos;
    ITERATE (vector <string>, it, arr) { 
      pos = strtmp.find(*it);
      len = (*it).size();
      if (len == 1) {
         strtmp = CTempString(strtmp).substr(pos+len);
         continue;
      }
      last_letter = (*it)[len-1];
      second_to_last_letter = (*it)[len-2];
      if (len == strtmp.size()) next_letter = ',';
      else next_letter = strtmp[pos+len];
      may_contain_plural 
         = x_DoesStrContainPlural(*it, last_letter, second_to_last_letter, next_letter);
      if (may_contain_plural) break;
      strtmp = CTempString(strtmp).substr(pos+len);
    }
  }
  arr.clear();
  return may_contain_plural;
};

char CSuspectRuleCheck :: GetClose(char bp)
{
  if (bp == '(') return ')';
  else if (bp == '[') return ']';
  else if (bp == '{') return '}';
  else return bp;
};

bool CSuspectRuleCheck :: SkipBracketOrParen(const unsigned& idx, string& start)
{
  bool rval = false;
  size_t ep, ns;

  if (idx > 2 && CTempString(start).substr(idx-3, 6) == "NAD(P)") {
     rval = true;
     start = CTempString(start).substr(idx + 3);
  } 
  else {
     unsigned len;
     ITERATE (vector <string>, it, thisInfo.skip_bracket_paren) {
       len = (*it).size();
       if (CTempString(start).substr(idx, len) == *it) {
         start = CTempString(start).substr(idx + len);
         rval = true;
         break;
       }
     }
     if (!rval) {
       ns = start.find(start[idx], idx+1);
       ep = start.find(GetClose(start[idx]), idx+1);
       if (ep != string::npos && (ns == string::npos || ns > ep)) {
         if (ep - idx < 5) {
           rval = true;
           start = CTempString(start).substr(ep+1);
         } 
         else if (ep - idx > 3 && CTempString(start).substr(ep - 3, 3) == "ing") {
           rval = true;
           start = CTempString(start).substr(ep + 1);
         }
       }
     }
  }
  return rval;
};

bool CSuspectRuleCheck :: ContainsNorMoreSetsOfBracketsOrParentheses(const string& search, const int& n)
{
  size_t idx, end;
  int num_found = 0;
  string open_bp("(["), sch_src(search);

  if (sch_src.empty()) return false;

  idx = sch_src.find_first_of(open_bp);
  while (idx != string::npos && num_found < n) {
     end = sch_src.find(GetClose(sch_src[idx]), idx);
     if (SkipBracketOrParen (idx, sch_src)) { // ignore it
         idx = sch_src.find_first_of(open_bp);
     }
     else if (end == string::npos) { // skip, doesn't close the bracket
         idx = sch_src.find_first_of(open_bp, idx+1);
     }
     else {
         idx = sch_src.find_first_of(open_bp, end);
         num_found++;
     }
  }
 
  if (num_found >= n) return true;
  else return false;
};

bool CSuspectRuleCheck :: PrecededByOkPrefix (const string& start_str)
{
  unsigned len_str = start_str.size();
  unsigned len_it;
  ITERATE (vector <string>, it, thisInfo.ok_num_prefix) {
    len_it = (*it).size();
    if (len_str >= len_it && (CTempString(start_str).substr(len_str-len_it) == *it)) 
          return true;
  }
  return false;
};

bool CSuspectRuleCheck :: InWordBeforeCytochromeOrCoenzyme(const string& start_str)
{
  if (start_str.empty()) return false;
  size_t pos = start_str.find_last_of(' ');
  string comp_str1, comp_str2;
  if (pos != string::npos) {
      strtmp = CTempString(start_str).substr(0, pos);
      pos = strtmp.find_last_not_of(' ');
      if (pos != string::npos) {
         unsigned len = strtmp.size();
         comp_str1 = CTempString(strtmp).substr(len-10);
         comp_str2 = CTempString(strtmp).substr(len-8);
         if ( (len >= 10  && NStr::EqualNocase(comp_str1, "cytochrome"))
                || (len >= 8 && NStr::EqualNocase(comp_str2, "coenzyme")) ) {
             return true;
         }
      }
  }
  return false;
};

bool CSuspectRuleCheck :: FollowedByFamily(string& after_str)
{
  size_t pos = after_str.find_first_of(' ');
  if (pos != string::npos) {
     after_str = CTempString(after_str).substr(pos+1);
     if (NStr::EqualNocase(after_str, "family")) {
           after_str = CTempString(after_str).substr(7);
           return true;
     }
  } 

  return false;
};

bool CSuspectRuleCheck :: ContainsThreeOrMoreNumbersTogether(const string& search)
{
  size_t p=0, p2;
  unsigned num_digits = 0;
  string sch_str = search;
  
  while (!sch_str.empty()) {
      p = sch_str.find_first_of(CDiscRepUtil::digit_str);
      if (p == string::npos) break;
      if (p && ( PrecededByOkPrefix(CTempString(sch_str).substr(0, p)) 
                  || InWordBeforeCytochromeOrCoenzyme (CTempString(sch_str).substr(0, p)) ) ) {
        p2 = sch_str.find_first_not_of(CDiscRepUtil::digit_str, p+1);
        if (p2 != string::npos) {
            sch_str = CTempString(sch_str).substr(p2);
            num_digits = 0;
        }
        else break;
      }
      else {
        num_digits ++;
        if (num_digits == 3) {
          sch_str = CTempString(sch_str).substr(p+1);
          if (FollowedByFamily(sch_str)) num_digits = 0;
          else return true;
        }
        if (p < sch_str.size() - 1) {
             sch_str = CTempString(sch_str).substr(p+1);
             if (!isdigit(sch_str[0])) num_digits = 0;
        }
        else break;
      }
  }
  return false;
};

bool CSuspectRuleCheck :: StringContainsUnderscore(const string& search)
{ 
  if (search.find('_') == string::npos) return false;

  arr.clear();
  arr = NStr::Tokenize(search, "_", arr);
  for (unsigned i=0; i< arr.size() - 1; i++) {
     strtmp = arr[i+1];
     if (FollowedByFamily(strtmp)) continue;   // strtmp was changed in the FollowedByFamily
     else if (arr[i].size() < 3 || search[arr[i].size()-1] == ' ') return true;
     else {
       strtmp = CTempString(arr[i]).substr(arr[i].size()-3);
       if ( (strtmp == "MFS" || strtmp == "TPR" || strtmp == "AAA")
                  && (isdigit(arr[i+1][0]) && !isdigit(arr[i+1][1])) )
          continue;
       else return true;
     }
  }

  arr.clear();
  return false;
};

bool CSuspectRuleCheck :: IsPrefixPlusNumbers(const string& prefix, const string& search)
{
  unsigned pattern_len;

  pattern_len = prefix.size();
  if (pattern_len > 0 && !NStr::EqualCase(search, 0, pattern_len, prefix)) {
       return false;
  }

  size_t digit_len 
           = search.find_first_not_of(CDiscRepUtil::digit_str, pattern_len);
  if (digit_len != string::npos && digit_len == search.size()) {
      return true;
  }
//  if (digit_len && (pattern_len + digit_len) == search.size()) return true;
  else return false;
};

bool CSuspectRuleCheck :: IsPropClose(const string& str, char open_p)
{
   if (str.empty()) return false;
   else if (str[str.size()-1] != open_p) return false;
   else return true;
};

bool CSuspectRuleCheck :: StringContainsUnbalancedParentheses(const string& search)
{
  size_t pos = 0;
  char ch_src;
  strtmp = kEmptyStr;
  string sch_src(search);
  while (!sch_src.empty()) {
    pos = sch_src.find_first_of("()[]");
    if (pos == string::npos) {
      if (strtmp.empty()) return false;
      else return true;
    }
    else {
       ch_src = sch_src[pos];
       if (ch_src == '(' || ch_src == '[') {
          strtmp += ch_src;
       }
       else if (sch_src[pos] == ')') {
            if (!IsPropClose(strtmp, '(')) return true;
            else strtmp = strtmp.substr(0, strtmp.size()-1);
       }
       else if (sch_src[pos] == ']') {
            if (!IsPropClose(strtmp, '[')) return true;
            else strtmp = strtmp.substr(0, strtmp.size()-1);
       }
    }
    if (pos < sch_src.size()-1) {
      sch_src = sch_src.substr(pos+1);
    }
    else sch_src = kEmptyStr;
  }
  
  if (strtmp.empty()) return false;
  else return true;
};

bool CSuspectRuleCheck :: ProductContainsTerm(const string& pattern, const string& search)
{
  /* don't bother searching for c-term or n-term if product name contains "domain" */
  if (NStr::FindNoCase(search, "domain") != string::npos) return false;

  size_t pos = NStr::FindNoCase(search, pattern);
  /* c-term and n-term must be either first word or separated from other word by space, 
    num, or punct */
  if (pos != string::npos && (!pos || !isalpha (search[pos-1]))) return true;
  else return false;
}

bool CSuspectRuleCheck :: MatchesSearchFunc(const string& str, const CSearch_func& func)
{
   switch (func.Which()){
      case CSearch_func::e_String_constraint:
         return DoesStringMatchConstraint(str, &(func.GetString_constraint()));
      case CSearch_func::e_Contains_plural:
         return StringMayContainPlural (str);
      case  CSearch_func::e_N_or_more_brackets_or_parentheses:
         return ContainsNorMoreSetsOfBracketsOrParentheses(str, 
                                  func.GetN_or_more_brackets_or_parentheses());
      case CSearch_func::e_Three_numbers:
         return ContainsThreeOrMoreNumbersTogether (str);
      case CSearch_func::e_Underscore:
         return StringContainsUnderscore (str);
      case CSearch_func::e_Prefix_and_numbers:
         return IsPrefixPlusNumbers (func.GetPrefix_and_numbers(), str);
      case CSearch_func::e_All_caps:
         return CDiscRepUtil::IsAllCaps (str);
      case CSearch_func::e_Unbalanced_paren:
         return StringContainsUnbalancedParentheses (str);
      case CSearch_func::e_Too_long:
         if (NStr::FindNoCase (str, "bifunctional") == string::npos 
                  && NStr::FindNoCase (str, "multifunctional") == string::npos
                  && str.size() > (unsigned) func.GetToo_long()) {
            return true;
         }
         else return false;
      case CSearch_func::e_Has_term:  
        return ProductContainsTerm(func.GetHas_term(), str);
      default: break;
  }
  return false; 
};

bool CSuspectRuleCheck :: IsStringConstraintEmpty(const CString_constraint* constraint)
{
   if (!constraint) return true;
   if (constraint->GetIs_all_caps() 
          || constraint->GetIs_all_lower()
          || constraint->GetIs_all_punct()) {
      return false;
   }
   else if (!constraint->CanGetMatch_text() 
                || constraint->GetMatch_text().empty()) {
        return true;
   }
   else {
      return false;
   }
};

bool CSuspectRuleCheck :: IsSearchFuncEmpty(const CSearch_func& func)
{
   switch (func.Which()) {
      case CSearch_func::e_String_constraint:
          return IsStringConstraintEmpty(&(func.GetString_constraint()));
      case  CSearch_func::e_Prefix_and_numbers:
          return (func.GetPrefix_and_numbers().empty());
      default: return false;
   }
   return false;
};

bool CSuspectRuleCheck :: MatchesSuspectProductRule(const string& str, const CSuspect_rule& rule)
{
   if (!IsSearchFuncEmpty(rule.GetFind())
            && !MatchesSearchFunc(str, rule.GetFind())) {
        return false;
   }
   else if ( rule.CanGetExcept() && !IsSearchFuncEmpty(rule.GetExcept())
                 && MatchesSearchFunc (str, rule.GetExcept())) {
        return false;
   }
   else {
      return true;
   }
};

bool CSuspectRuleCheck :: DoesObjectMatchStringConstraint(const CBioSource& biosrc, const CString_constraint& str_cons)
{
   vector <string> strs; 
   GetStringsFromObject(biosrc, strs);
   ITERATE (vector <string>, it, strs) {
      if (DoesSingleStringMatchConstraint(*it, &str_cons)) {
          strs.clear();
          return true;
      }
   }
   strs.clear();
   return false;
};

bool CSuspectRuleCheck :: DoesObjectMatchStringConstraint(const CCGPSetData& cgp, const CString_constraint& str_cons)
{
  // needed? if (scp == NULL) return TRUE;

  /* CDS-Gene-Prot set */
  bool all_match = true, any_match = false;
  vector <string> strs;
  if (cgp.gene) {
       m_bioseq_hl 
         = GetBioseqFromSeqLoc(cgp.gene->GetLocation(), *thisInfo.scope);
       GetStringsFromObject(*cgp.gene, strs);
       if (DoesObjectMatchStringConstraint ( *cgp.gene, strs, str_cons)) {
          any_match = true;
       }
       else {
          any_match = false;
       }
       strs.clear();
  }
  if (cgp.cds && (!any_match || all_match)) {
      m_bioseq_hl 
          = GetBioseqFromSeqLoc(cgp.cds->GetLocation(), *thisInfo.scope);
      strs.clear();
      GetStringsFromObject(*cgp.cds, strs);
      if (DoesObjectMatchStringConstraint( *cgp.cds, strs, str_cons)) {
         any_match = true;
      }
      else {
         all_match = false;
      }
      strs.clear();
  }
  if (cgp.mrna && (!any_match || all_match)) {
      m_bioseq_hl 
         = GetBioseqFromSeqLoc(cgp.mrna->GetLocation(), *thisInfo.scope);
      strs.clear();
      GetStringsFromObject(*cgp.mrna, strs);
      if (DoesObjectMatchStringConstraint( *cgp.mrna, strs, str_cons)) {
         any_match = true;
      }
      else {
         all_match = false;
      }
      strs.clear();
  }
  if (cgp.prot  && (!any_match || all_match)) {
      m_bioseq_hl = GetBioseqFromSeqLoc(cgp.prot->GetLocation(), *thisInfo.scope);
      strs.clear();
      GetStringsFromObject(*cgp.prot, strs);
      if (DoesObjectMatchStringConstraint( *cgp.prot, strs, str_cons)) any_match = true;
      else all_match = false;
      strs.clear();
  }
  if (!any_match || all_match) {
     ITERATE (vector <const CSeq_feat*>, it, cgp.mat_peptide_list) {
        m_bioseq_hl = GetBioseqFromSeqLoc((*it)->GetLocation(), *thisInfo.scope);
        strs.clear();
        GetStringsFromObject(**it, strs);
        if (DoesObjectMatchStringConstraint( **it, strs, str_cons)) any_match = true;
        else all_match = false;
        strs.clear();
        if (any_match && !all_match) break;
     }
  }

  if (str_cons.GetNot_present()) return all_match;
  else return any_match;
};

bool CSuspectRuleCheck :: DoesObjectMatchStringConstraint(const CSeq_feat& feat, const vector <string>& strs, const CString_constraint& str_cons)
{
   bool rval = false;
   ITERATE (vector <string>, it, strs) { 
       rval = DoesSingleStringMatchConstraint(*it, &str_cons); 
       if (rval) {
            break;
       }
   }
   if (!rval) {
     string str(kEmptyStr);
     switch (feat.GetData().Which()) {
       case CSeqFeatData::e_Cdregion: 
         {
            if (feat.CanGetProduct()) {
               CBioseq_Handle 
                     bioseq_hl = GetBioseqFromSeqLoc(feat.GetProduct(), *thisInfo.scope);
               if (bioseq_hl) {
                  CFeat_CI ci(bioseq_hl, SAnnotSelector(CSeqFeatData::eSubtype_prot)); 
                  if (ci) {
                     vector <string> arr;
                     GetStringsFromObject(ci->GetOriginalFeature(), arr);
                     ITERATE (vector <string>, it, arr) {
                         rval = DoesSingleStringMatchConstraint(*it, &str_cons);
                         if (rval) break;
                     }
                     arr.clear();
                  }
               }
            }
            break;
         }
       case CSeqFeatData::e_Rna:
         {
           if (feat.GetData().GetSubtype() == CSeqFeatData::eSubtype_rRNA) {
             CTestAndRepData :: GetSeqFeatLabel(feat, str);
             rval = DoesSingleStringMatchConstraint(str, &str_cons);
             if (!rval) {
               str = "tRNA-" + str;
               rval = DoesSingleStringMatchConstraint(str, &str_cons);
             }
           }
           break;
         }
       case CSeqFeatData::e_Imp:
         rval = DoesSingleStringMatchConstraint(feat.GetData().GetImp().GetKey(), &str_cons);
         break;
       default: break;
     }
   }
   if (str_cons.GetNot_present()) rval = !rval;
   return rval;
};

bool CSuspectRuleCheck :: IsLocationConstraintEmpty(const CLocation_constraint& loc_cons)
{
  if (loc_cons.GetStrand() != eStrand_constraint_any
          || loc_cons.GetSeq_type() != eSeqtype_constraint_any
          || loc_cons.GetPartial5() != ePartial_constraint_either
          || loc_cons.GetPartial3() != ePartial_constraint_either
          || loc_cons.GetLocation_type() != eLocation_type_constraint_any
          || (loc_cons.CanGetEnd5()
                && loc_cons.GetEnd5().Which() != CLocation_pos_constraint::e_not_set)
          || (loc_cons.CanGetEnd3()
                && loc_cons.GetEnd3().Which() != CLocation_pos_constraint::e_not_set)) {
    return false;
  }
  else return true;
};

bool CSuspectRuleCheck :: DoesStrandMatchConstraint(const CSeq_loc& loc, const CLocation_constraint& loc_cons)
{
  if (loc.Which() == CSeq_loc::e_not_set) return false;
  if (loc_cons.GetStrand() == eStrand_constraint_any) return true;

  if (loc.GetStrand() == eNa_strand_minus) {
      if (loc_cons.GetStrand() == eStrand_constraint_minus) return true;
      else return false;
  }
  else {
     if (loc_cons.GetStrand() == eStrand_constraint_plus) return true;
     else return false;
  }
};

bool CSuspectRuleCheck :: DoesBioseqMatchSequenceType(const ESeqtype_constraint& seq_type)
{
  if (seq_type == eSeqtype_constraint_any
        || (m_bioseq_hl.IsNa() && seq_type == eSeqtype_constraint_nuc)
        || (m_bioseq_hl.IsAa() && seq_type == eSeqtype_constraint_prot)) {
      return true;
  }
  else return false;
};

bool CSuspectRuleCheck :: DoesLocationMatchPartialnessConstraint(const CSeq_loc& loc, const CLocation_constraint& loc_cons)
{ 
  bool partial5 = loc.IsPartialStart(eExtreme_Biological);
  bool partial3 = loc.IsPartialStop(eExtreme_Biological);
  if ( (loc_cons.GetPartial5() == ePartial_constraint_partial && !partial5) 
         || (loc_cons.GetPartial5() == ePartial_constraint_complete && partial5) 
         || (loc_cons.GetPartial3() == ePartial_constraint_partial && !partial3)
         || (loc_cons.GetPartial3() == ePartial_constraint_complete && partial3) ) {
       return false;
    }
    else return true;
};

bool CSuspectRuleCheck :: DoesLocationMatchTypeConstraint(const CSeq_loc& seq_loc, const CLocation_constraint& loc_cons)
{ 
  bool has_null = false;
  int  num_intervals = 0;
  
  if (loc_cons.GetLocation_type() == eLocation_type_constraint_any) return true;
  else  
  {    
    for (CSeq_loc_CI sl_ci(seq_loc); sl_ci; ++ sl_ci) {
       if (sl_ci.GetEmbeddingSeq_loc().Which() == CSeq_loc::e_Null) has_null = true;
       else if (!sl_ci.IsEmpty()) num_intervals ++;
    }
    
    if (loc_cons.GetLocation_type() == eLocation_type_constraint_single_interval)
    {
      if (num_intervals == 1) return true;
    }
    else if (loc_cons.GetLocation_type() == eLocation_type_constraint_joined)
    {
      if (num_intervals > 1 && !has_null) return true;
    }
    else if (loc_cons.GetLocation_type() == eLocation_type_constraint_ordered)
    {
      if (num_intervals > 1 && has_null) return true;
    }
  }
  return false;
};

bool CSuspectRuleCheck :: DoesPositionMatchEndConstraint(int pos, const CLocation_pos_constraint& lp_cons)
{
   switch (lp_cons.Which())
   {
      case CLocation_pos_constraint:: e_Dist_from_end:
            if (pos != lp_cons.GetDist_from_end()) return false;
            break;
      case CLocation_pos_constraint:: e_Max_dist_from_end:
            if (pos > lp_cons.GetMax_dist_from_end()) return false;
            break;
      case CLocation_pos_constraint:: e_Min_dist_from_end:
            if (pos < lp_cons.GetMin_dist_from_end()) return false;
            break;
      default: break;
   }
   return true;
};

bool CSuspectRuleCheck :: DoesLocationMatchDistanceConstraint(const CSeq_loc& loc, const CLocation_constraint& loc_cons)
{
  if (!loc_cons.CanGetEnd5() && !loc_cons.CanGetEnd3()) return true;

  unsigned pos = loc.GetStop(eExtreme_Positional);
  int pos2 = (m_bioseq_hl.IsSetInst_Length() ? m_bioseq_hl.GetInst_Length():0) - pos - 1;

  if (loc.GetStrand() == eNa_strand_minus)
  {
    if (loc_cons.CanGetEnd5()) {
      if (m_bioseq_hl.GetCompleteBioseq().Empty()) return false;
      else {
        if (!DoesPositionMatchEndConstraint(pos2, loc_cons.GetEnd5())) return false;
      }
    }
    if (loc_cons.CanGetEnd3()) 
         return DoesPositionMatchEndConstraint(pos, loc_cons.GetEnd3());
  }
  else
  {
    if (loc_cons.CanGetEnd5())
      if (!DoesPositionMatchEndConstraint(pos, loc_cons.GetEnd5())) return false;
    if (loc_cons.CanGetEnd3()) {
      if (m_bioseq_hl.GetCompleteBioseq().Empty()) return false;
      return DoesPositionMatchEndConstraint(pos2, loc_cons.GetEnd3());
    }
  }
  return true;
};

bool CSuspectRuleCheck :: DoesFeatureMatchLocationConstraint(const CSeq_feat& feat, const CLocation_constraint& loc_cons)
{
  if (IsLocationConstraintEmpty (loc_cons)) return true;

  const CSeq_loc& feat_loc = feat.GetLocation();
  if (loc_cons.GetStrand() != eStrand_constraint_any) {
    if (!m_bioseq_hl) return false;
    else if (m_bioseq_hl.IsAa()) {
      const CSeq_feat* cds = sequence::GetCDSForProduct(m_bioseq_hl);
      if (!cds) return false;
      else if (!DoesStrandMatchConstraint (cds->GetLocation(), loc_cons))
        return false;
    }
    else if (!DoesStrandMatchConstraint (feat_loc, loc_cons))
        return false;
  }

  if (!DoesBioseqMatchSequenceType(loc_cons.GetSeq_type())) return false;

  if (!DoesLocationMatchPartialnessConstraint (feat_loc, loc_cons)) return false;

  if (!DoesLocationMatchTypeConstraint (feat_loc, loc_cons)) return false;

  if (!DoesLocationMatchDistanceConstraint(feat_loc, loc_cons)) return false;

  return true;
};

bool CSuspectRuleCheck :: IsSubsrcQual(ESource_qual src_qual)
{
   switch (src_qual) {
     case eSource_qual_cell_line:
     case eSource_qual_cell_type:
     case eSource_qual_chromosome:
     case eSource_qual_clone:
     case eSource_qual_clone_lib:
     case eSource_qual_collected_by:
     case eSource_qual_collection_date:
     case eSource_qual_country:
     case eSource_qual_dev_stage:
     case eSource_qual_endogenous_virus_name:
     case eSource_qual_environmental_sample:
     case eSource_qual_frequency:
     case eSource_qual_fwd_primer_name:
     case eSource_qual_fwd_primer_seq:
     case eSource_qual_genotype:
     case eSource_qual_germline:
     case eSource_qual_haplotype:
     case eSource_qual_identified_by:
     case eSource_qual_insertion_seq_name:
     case eSource_qual_isolation_source:
     case eSource_qual_lab_host:
     case eSource_qual_lat_lon:
     case eSource_qual_map:
     case eSource_qual_metagenomic:
     case eSource_qual_plasmid_name:
     case eSource_qual_plastid_name:
     case eSource_qual_pop_variant:
     case eSource_qual_rearranged:
     case eSource_qual_rev_primer_name:
     case eSource_qual_rev_primer_seq:
     case eSource_qual_segment:
     case eSource_qual_sex:
     case eSource_qual_subclone:
     case eSource_qual_subsource_note:
     case eSource_qual_tissue_lib :
     case eSource_qual_tissue_type:
     case eSource_qual_transgenic:
     case eSource_qual_transposon_name:
     case eSource_qual_mating_type:
     case eSource_qual_linkage_group:
     case eSource_qual_haplogroup:
     case eSource_qual_altitude:
        return true;
     default: return false;
   }
};

string CSuspectRuleCheck :: GetNotTextqualSrcQualValue(const CBioSource& biosrc, const CSource_qual_choice& src_qual, const CString_constraint* str_cons)
{
  string str(kEmptyStr);

  switch (src_qual.Which()) {
    case CSource_qual_choice:: e_Location:
      str = CBioSource::ENUM_METHOD_NAME(EGenome)()
             ->FindName((CBioSource::EGenome)biosrc.GetGenome(), false);
      if (str == "unknown") str = " "; // ????
      else if (str == "extrachrom") str = "extrachromosomal";
      if (str_cons && !DoesStringMatchConstraint (str, str_cons)) {
              str = kEmptyStr;
      }
      break;
    case CSource_qual_choice::e_Origin:
      str = CBioSource::ENUM_METHOD_NAME(EOrigin)()
                     ->FindName((CBioSource::EOrigin)biosrc.GetOrigin(), false);
      if (str_cons && !DoesStringMatchConstraint (str, str_cons)) {
           str = kEmptyStr;
      }
      break;
    case CSource_qual_choice::e_Gcode:
      if (biosrc.IsSetGcode() && biosrc.GetGcode()) {
          str = NStr::IntToString(biosrc.GetGcode());
      }
      break;
    case CSource_qual_choice::e_Mgcode:
      if (biosrc.IsSetMgcode() && biosrc.GetMgcode()) {
              str = NStr::IntToString(biosrc.GetMgcode());
      }
      break;
    default: break;
  }
  return str;
};

string CSuspectRuleCheck :: GetSrcQualValue4FieldType(const CBioSource& biosrc, const CSource_qual_choice& src_qual, const CString_constraint* str_cons)
{
   string str(kEmptyStr);
   switch (src_qual.Which()) {
     case CSource_qual_choice::e_Textqual:
       {
          ESource_qual text_qual= src_qual.GetTextqual();
          string qual_name = thisInfo.srcqual_names[text_qual];
          bool is_subsrc = IsSubsrcQual(text_qual);
          str = CTestAndRepData :: GetSrcQualValue( biosrc, qual_name, is_subsrc, str_cons);
       }
       break;
     case CSource_qual_choice::e_not_set: break;
     default:
         str = GetNotTextqualSrcQualValue(biosrc, src_qual, str_cons);
   }
   return str;
};

string CSuspectRuleCheck :: GetSequenceQualFromBioseq (const CMolinfo_field& mol_field)
{
  const CMolInfo* molinfo = GetMolInfo(m_bioseq_hl); // objmgr/util/sequence.hpp
  string str(kEmptyStr);
  switch (mol_field.Which()) {
    case CMolinfo_field::e_Molecule:
        str = CMolInfo::ENUM_METHOD_NAME(EBiomol)()->FindName(molinfo->GetBiomol(), true);
        break;
    case CMolinfo_field::e_Technique:
        str = CMolInfo::ENUM_METHOD_NAME(ETech)()->FindName(molinfo->GetTech(), true);
        break;
    case CMolinfo_field::e_Completedness:
        str = CMolInfo::ENUM_METHOD_NAME(ECompleteness)()
                                              ->FindName(molinfo->GetCompleteness(), true);
        break;
    case CMolinfo_field::e_Mol_class:
        if (m_bioseq_hl.IsSetInst_Mol()) str = thisInfo.mol_molname[m_bioseq_hl.GetInst_Mol()];
        break;
    case CMolinfo_field::e_Topology:
        if (m_bioseq_hl.IsSetInst_Topology())
                   str = CSeq_inst::ENUM_METHOD_NAME(ETopology)()
                                ->FindName(m_bioseq_hl.GetInst_Topology(), true);
        break;
    case CMolinfo_field::e_Strand:
        if (m_bioseq_hl.CanGetInst_Strand())
           str = thisInfo.strand_strname[m_bioseq_hl.GetInst_Strand()];
        break;
    default: break;
  }
  if (str == "unknown" || str == "not_set") str = kEmptyStr;
  return str;
};

string CSuspectRuleCheck :: GetDBLinkFieldFromUserObject(const CUser_object& user_obj, EDBLink_field_type dblink_tp, const CString_constraint& str_cons)
{
  string str(kEmptyStr);
  bool is_str_cons_empty = IsStringConstraintEmpty(&str_cons);
  if (user_obj.GetType().IsStr() && user_obj.GetType().GetStr() == "DBLink") {
    string field_name = thisInfo.dblink_names[dblink_tp];
    ITERATE (vector <CRef <CUser_field> >, it, user_obj.GetData()) {
       if ( (*it)->GetLabel().IsStr() && ((*it)->GetLabel().GetStr() == field_name) ) {
          if ( (*it)->GetData().IsStrs()) {
             ITERATE (vector <CStringUTF8>, sit, (*it)->GetData().GetStrs()) {
               str = CUtf8::AsSingleByteString(*sit, eEncoding_Ascii);
               if (is_str_cons_empty || DoesStringMatchConstraint (str, &str_cons)) break;
               else str = kEmptyStr;
             }
          }
          else if ( (*it)->GetData().IsInts() ) {
             ITERATE (vector <int>, iit, (*it)->GetData().GetInts()) {
               str = NStr::IntToString(*iit);
               if (is_str_cons_empty || DoesStringMatchConstraint (str, &str_cons)) break;
               else str = kEmptyStr;
             }
          }
          if (!str.empty()) break;
       }
    }
  }
  return str;
};

// GetFirstValNodeStringMatch
string CSuspectRuleCheck :: GetFirstStringMatch(const list <string>& strs, const CString_constraint* str_cons)
{
   ITERATE (list <string>, sit, strs) {
     if ( !(*sit).empty() && DoesStringMatchConstraint (*sit, str_cons))
       return (*sit);
   }
   return kEmptyStr;
};

string CSuspectRuleCheck :: GetFirstStringMatch(const vector <string>& strs, const CString_constraint* str_cons)
{
   ITERATE (vector <string>, sit, strs) {
     if ( !(*sit).empty() && DoesStringMatchConstraint (*sit, str_cons))
       return (*sit);
   }
   return kEmptyStr;
};

string CSuspectRuleCheck :: GetFieldValueForObjectEx (const CField_type& field_type, const CString_constraint& str_cons)
{
  string str(kEmptyStr);
  bool is_empty_str_cons = IsStringConstraintEmpty (&str_cons);
  switch (field_type.Which()) {
    case CField_type::e_Source_qual :
      {
         const CBioSource* biosrc = GetBioSource(m_bioseq_hl);
         if (biosrc) 
                str = GetSrcQualValue4FieldType (*biosrc, field_type.GetSource_qual(), 
                                                                                &str_cons);
         break;
      }
    case CField_type::e_Molinfo_field :
      str = GetSequenceQualFromBioseq (field_type.GetMolinfo_field());
      break;
    case CField_type::e_Dblink:
      for (CSeqdesc_CI seq_ci(m_bioseq_hl, CSeqdesc::e_User); seq_ci && str.empty(); ++seq_ci)
         str =GetDBLinkFieldFromUserObject(seq_ci->GetUser(),field_type.GetDblink(), str_cons);
      break;
    case CField_type::e_Misc:
      switch (field_type.GetMisc()) {
         case eMisc_field_genome_project_id:
              for (CSeqdesc_CI seq_ci(m_bioseq_hl, CSeqdesc::e_User); seq_ci && str.empty(); 
                                                                                    ++seq_ci) {
                  const CObject_id& type = seq_ci->GetUser().GetType();
                  if (type.IsStr() && type.GetStr() == "GenomeProjectsDB") {                 
                     ITERATE (vector <CRef <CUser_field> >, uit, seq_ci->GetUser().GetData()) {
                        const CObject_id& label = (*uit)->GetLabel();
                        if (label.IsStr() && label.GetStr()== "ProjectID" 
                                                         && (*uit)->GetData().IsInt()) {
                            str = NStr::IntToString( (*uit)->GetData().GetInt() );
                            if (is_empty_str_cons || DoesStringMatchConstraint (str,&str_cons))
                                      break;
                            else str = kEmptyStr;
                        }
                     }
                  }
              }
              break;
         case eMisc_field_comment_descriptor:
              for (CSeqdesc_CI seq_ci(m_bioseq_hl, CSeqdesc::e_Comment); seq_ci && str.empty();
                                                                                  ++seq_ci) {
                  str = seq_ci->GetComment();
                  if (!DoesStringMatchConstraint (str, &str_cons)) str = kEmptyStr;
              }
              break;
         case eMisc_field_defline:
              for (CSeqdesc_CI seq_ci(m_bioseq_hl, CSeqdesc::e_Title); seq_ci && str.empty();
                                                                                   ++seq_ci) {
                    str = seq_ci->GetTitle();
                    if (!DoesStringMatchConstraint (str, &str_cons)) str = kEmptyStr;
              }
              break;
         case eMisc_field_keyword:
              for (CSeqdesc_CI seq_ci(m_bioseq_hl, CSeqdesc::e_Genbank); seq_ci && str.empty();
                                                                                  ++seq_ci) {
                 if (seq_ci->GetGenbank().CanGetKeywords()) 
                      str = GetFirstStringMatch(seq_ci->GetGenbank().GetKeywords(), &str_cons);
              }
              break;
         default: break;
      }
      break;
    default: break;
  }
  return str;
};

string CSuspectRuleCheck :: GetDbxrefString (const vector <CRef <CDbtag> >& dbxref, const CString_constraint* str_cons)
{
  string str(kEmptyStr), label;
  ITERATE (vector <CRef <CDbtag> >, it, dbxref) {
     label = kEmptyStr;
     (*it)->GetLabel(&label);
     if (DoesStringMatchConstraint(label, str_cons))
       str += label + ";";
  }

  if (!str.empty()) str = str.substr(0, str.size()-1);
  return str;
};

string CSuspectRuleCheck :: GetCodeBreakString (const CSeq_feat& seq_feat)
{
  string str(kEmptyStr);
  if (m_bioseq_hl) {
     CFlatFileConfig cfg(CFlatFileConfig::eFormat_GenBank, CFlatFileConfig::eMode_Release );
     CFlatFileContext ffctx(cfg);
     CBioseqContext ctx(m_bioseq_hl, ffctx);
     const CCdregion& cd = seq_feat.GetData().GetCdregion();
     if (cd.CanGetCode_break()) {
        CFlatCodeBreakQVal fcb(cd.GetCode_break());
        vector <CRef <CFormatQual> > quals;
        fcb.Format(quals, "transl_except", ctx, 0);
        ITERATE (vector <CRef <CFormatQual> >, it, quals) {
          str += "/" + (*it)->GetName() + "=" + (*it)->GetValue() + "; ";
        }
     }
  };

  if (!str.empty()) str = str.substr(0, str.size()-2);
  return str;
};

string CSuspectRuleCheck :: GettRNACodonsRecognized (const CTrna_ext& trna, const CString_constraint* str_cons)
{
  string str(kEmptyStr);
  if (trna.CanGetCodon()) {
    ITERATE (list <int>, it, trna.GetCodon()) {  // should have only 6
       if ( (*it) < 64)
          str += CGen_code_table :: IndexToCodon(*it) + ", ";
    }
    if (!str.empty()) return (str.substr(0, str.size()-2));
  }
  return kEmptyStr;
  // str_cons not used????
};

string CSuspectRuleCheck :: GetIntervalString(const CSeq_interval& seq_int)
{
  bool partial5 = seq_int.IsPartialStart(eExtreme_Biological);
  bool partial3 = seq_int.IsPartialStop(eExtreme_Biological);
  string str;

  if (seq_int.CanGetStrand() && seq_int.GetStrand() == eNa_strand_minus) {
    str = (string)"complement(" + (partial3 ? "<" : "")
             + NStr::UIntToString(seq_int.GetStart(eExtreme_Positional))
             + ".." + (partial5 ? ">" : "")
             + NStr::UIntToString(seq_int.GetStop(eExtreme_Positional)) + ")";
  } else {
    str = (partial5 ? "<" : "")
             + NStr::UIntToString(seq_int.GetStart(eExtreme_Positional))
             + ".." + (partial3 ? ">" : "")
             + NStr::UIntToString(seq_int.GetStop(eExtreme_Positional));
  }
  return str;
};

string CSuspectRuleCheck :: GetAnticodonLocString (const CTrna_ext& trna)
{
  if (!trna.CanGetAnticodon()) return kEmptyStr;

  string str(kEmptyStr);
  if (trna.GetAnticodon().IsInt()) return(GetIntervalString (trna.GetAnticodon().GetInt()));
  else if (trna.GetAnticodon().IsMix()) {
    ITERATE (list <CRef <CSeq_loc> >, it, trna.GetAnticodon().GetMix().Get()) {
       if ( (*it)->IsInt()) str += GetIntervalString( (*it)->GetInt() ) + ", ";
       else return( "complex location" );
    }
    return (str.substr(0, str.size() - 2));
  }
  return kEmptyStr;
};

string CSuspectRuleCheck :: GetQualFromFeatureAnyType(const CSeq_feat& seq_feat, const CFeat_qual_choice& feat_qual, const CString_constraint* str_cons)
{
  string str(kEmptyStr);
  bool is_legal_qual = feat_qual.IsLegal_qual();
  bool is_illegal_qual = feat_qual.IsIllegal_qual();
  EFeat_qual_legal 
    legal_qual= feat_qual.IsLegal_qual() ? feat_qual.GetLegal_qual() : eFeat_qual_legal_allele;
  const CString_constraint* 
       illegal_qual = feat_qual.IsIllegal_qual() ?  &(feat_qual.GetIllegal_qual()) : 0;

  const CSeqFeatData& seq_fdt = seq_feat.GetData();
  // for gene fields 
  // GetGeneInfoForFeature (seq_feat, grp, gene);
  CGene_ref* gene_ref = 0;
  CConstRef <CSeq_feat> gene_feat(0);
  if (seq_feat.GetData().IsGene()) {
     gene_ref = const_cast<CGene_ref*>(&(seq_fdt.GetGene()));
     gene_feat = CConstRef <CSeq_feat> (&seq_feat);
  }
  else {
     gene_ref = const_cast<CGene_ref*>(seq_feat.GetGeneXref());
     if (!gene_ref) {
        gene_feat = GetBestOverlappingFeat(seq_feat.GetLocation(), CSeqFeatData::e_Gene, 
                                                         eOverlap_Contained, *thisInfo.scope);
        if (gene_feat.NotEmpty())
          gene_ref = const_cast<CGene_ref*>(&(gene_feat->GetData().GetGene()));
     }
     else if (gene_ref->IsSuppressed()) gene_ref = 0;
  }

  // for protein fields
  const CProt_ref* prot=0;
  CBioseq_Handle prot_hl;
  if (seq_fdt.IsProt()) prot = &(seq_fdt.GetProt());
  else if (seq_fdt.IsCdregion()) {
    prot = seq_feat.GetProtXref();
    if (!prot && seq_feat.CanGetProduct()) {
      prot_hl = GetBioseqFromSeqLoc(seq_feat.GetProduct(), *thisInfo.scope); 
      if (prot_hl) {
         const CSeq_feat* prot_f = GetPROTForProduct(prot_hl);
         if (prot_f) prot = &(prot_f->GetData().GetProt());
      }
    }
  }

/* fields common to all features */
/* note, also known as comment */
  if ( (is_legal_qual && legal_qual == eFeat_qual_legal_note)
      || (is_illegal_qual && DoesStringMatchConstraint("note", illegal_qual)))
  {
     if (seq_feat.CanGetComment()) str =  seq_feat.GetComment();
  }
  else if ((is_legal_qual && legal_qual == eFeat_qual_legal_db_xref)
          || (is_illegal_qual && DoesStringMatchConstraint ("db_xref", illegal_qual)))
  { /* db-xref */
    if (seq_feat.CanGetDbxref()) str = GetDbxrefString (seq_feat.GetDbxref(), str_cons);
    return str;
  }
  else if ((is_legal_qual && legal_qual == eFeat_qual_legal_exception)
          || (is_illegal_qual && DoesStringMatchConstraint ("exception", illegal_qual)))
  { /* exception */
     if (seq_feat.CanGetExcept_text()) str = seq_feat.GetExcept_text();
  }
  else if ((is_legal_qual && legal_qual == eFeat_qual_legal_evidence)
          || (is_illegal_qual && DoesStringMatchConstraint ("evidence", illegal_qual)))
  { /* evidence */
     if (seq_feat.CanGetExp_ev()) {
        if (seq_feat.GetExp_ev() == CSeq_feat::eExp_ev_experimental) str = "experimental";
        else str = "non-experimental";
     }
  }
  else if ((is_legal_qual && legal_qual == eFeat_qual_legal_citation)
          || (is_illegal_qual && DoesStringMatchConstraint ("citation", illegal_qual)))
  { /* citation */ // ####?????
//    str = GetCitationTextFromFeature (sfp, str_cons, batch_extra == NULL ? NULL : batch_extra->cit_list);
      if (seq_feat.CanGetCit()) seq_feat.GetCit().GetLabel(&str);
  }
  else if ((is_legal_qual && legal_qual == eFeat_qual_legal_location)
          || (is_illegal_qual && DoesStringMatchConstraint ("location", illegal_qual)))
  { /* location */
      return CTestAndRepData :: SeqLocPrintUseBestID (seq_feat.GetLocation());
  }
  else if ((is_legal_qual && legal_qual == eFeat_qual_legal_pseudo)
           || (is_illegal_qual && DoesStringMatchConstraint ("pseudogene", illegal_qual)))
  { /* pseudo */
     if (seq_feat.CanGetQual()) {
        str = GetFirstGBQualMatch(seq_feat.GetQual(), "pseudogene", 0, str_cons);
        if (str.empty() && seq_feat.CanGetPseudo() && seq_feat.GetPseudo())
             str = "unqualified";
     }
     return str;
  }
// fields common to some features
  else if ((is_legal_qual && legal_qual == eFeat_qual_legal_product) 
           || (is_illegal_qual && DoesStringMatchConstraint ("product", illegal_qual)))
  { // product
    if (prot) {
      if (prot->CanGetName()) return GetFirstStringMatch (prot->GetName(), str_cons);
    } 
    else if (seq_fdt.IsRna()) {
      str = CBioseqTestAndRepData :: GetRNAProductString (seq_feat);
    }
  }
  else { /* Gene fields */
    bool has_gene_ref = (gene_ref != 0);
    // locus
    if ((is_legal_qual && legal_qual == eFeat_qual_legal_gene)
           || (is_illegal_qual && DoesStringMatchConstraint ("locus", illegal_qual))) {
     if (has_gene_ref && gene_ref->CanGetLocus()) str = gene_ref->GetLocus();
    }
    else if ((is_legal_qual && legal_qual == eFeat_qual_legal_gene_description)
             || (is_illegal_qual && DoesStringMatchConstraint ("description", illegal_qual))) {
       /* description */
          if (has_gene_ref && gene_ref->CanGetDesc()) str = gene_ref->GetDesc();
    }
    else if ((is_legal_qual && legal_qual == eFeat_qual_legal_map)
               || (is_illegal_qual && DoesStringMatchConstraint ("map", illegal_qual))) {
          /* maploc */
           if (has_gene_ref && gene_ref->CanGetMaploc()) str = gene_ref->GetMaploc();
    }
    else if ( ((is_legal_qual && legal_qual == eFeat_qual_legal_allele)
                    || (is_illegal_qual && DoesStringMatchConstraint ("allele", illegal_qual)))
                 && (seq_fdt.GetSubtype() == CSeqFeatData::eSubtype_variation)) {
             /* allele */
                if (has_gene_ref && gene_ref->CanGetAllele()) str = gene_ref->GetAllele();
    }
    else if ((is_legal_qual && legal_qual == eFeat_qual_legal_locus_tag)
               || (is_illegal_qual && DoesStringMatchConstraint ("locus_tag", illegal_qual))) {
            /* locus_tag */
               if (has_gene_ref && gene_ref->CanGetLocus_tag()) str = gene_ref->GetLocus_tag();
    }
    else if ((is_legal_qual && legal_qual == eFeat_qual_legal_synonym)
                || (is_illegal_qual && DoesStringMatchConstraint ("synonym", illegal_qual))) {
              /* synonym */
              if (has_gene_ref && gene_ref->CanGetSyn()) 
                   return GetFirstStringMatch (gene_ref->GetSyn(), str_cons);
    }
    else if (is_legal_qual && legal_qual == eFeat_qual_legal_gene_comment) {
             /* gene comment */
            if (gene_feat.NotEmpty() && gene_feat->CanGetComment()) 
                    str = gene_feat->GetComment();
    }
    else { /* protein fields, note - product handled above */
       bool has_prot = (prot != 0);
       if ((is_legal_qual && legal_qual == eFeat_qual_legal_description)
              || (is_illegal_qual && DoesStringMatchConstraint("description",illegal_qual))) {
            /* description */
            if (has_prot && prot->CanGetDesc()) str = prot->GetDesc();
       }
       else if ((is_legal_qual && legal_qual == eFeat_qual_legal_ec_number)
                   ||(is_illegal_qual && DoesStringMatchConstraint("ec_number",illegal_qual))){
               /* ec_number */
               if (has_prot && prot->CanGetEc())
                     return GetFirstStringMatch(prot->GetEc(), str_cons);
       }
       else if ((is_legal_qual && legal_qual == eFeat_qual_legal_activity)
                   ||(is_illegal_qual && DoesStringMatchConstraint("activity",illegal_qual))) {
               /* activity */
               if (has_prot && prot->CanGetActivity()) 
                    return GetFirstStringMatch (prot->GetActivity(), str_cons); 
       }
       else { /* coding region fields */
           bool has_cd = seq_fdt.IsCdregion();
           /* transl_except */
           if (is_legal_qual && legal_qual == eFeat_qual_legal_transl_except) {
                  if (has_cd) return GetCodeBreakString (seq_feat);
           }
           else if (is_legal_qual && legal_qual == eFeat_qual_legal_transl_table) {
              /* transl_table */
              if ( has_cd) {
                 if (seq_fdt.GetCdregion().CanGetCode()) {
                     ITERATE (list < CRef <CGenetic_code::C_E> >, it,
                                                   seq_fdt.GetCdregion().GetCode().Get()) {
                         if ( (*it)->IsId() ) return NStr::IntToString( (*it)->GetId());
                     }
                 }
              }
           }
           else if (is_legal_qual && legal_qual == eFeat_qual_legal_translation) {
              /* translation */  // try CSeqTranslator
              if (has_cd && seq_feat.CanGetProduct()) {
                 if (prot_hl && prot_hl.CanGetInst_Length()) {
                   CSeqVector seq_vec = prot_hl.GetSeqVector(CBioseq_Handle::eCoding_Iupac);
                   seq_vec.GetSeqData(0, prot_hl.GetInst_Length(), str);
                   return str;
                 }
              } 
           }
           else { /* special RNA qualifiers */
              /* tRNA qualifiers */
              bool is_rna = seq_fdt.IsRna();
              bool has_ext = (is_rna && seq_fdt.GetRna().CanGetExt()) ? true : false;
              bool is_trna = (has_ext && seq_fdt.GetRna().GetExt().IsTRNA()) ? true : false; 
              bool is_rna_gen = (has_ext && seq_fdt.GetRna().GetExt().IsGen()) ? true : false;
              /* codon-recognized */
              if ((is_legal_qual && legal_qual == eFeat_qual_legal_codons_recognized)
                   ||(is_illegal_qual 
                             && DoesStringMatchConstraint("codon-recognized", illegal_qual))) {
                if (is_trna) 
                   return GettRNACodonsRecognized(
                                              seq_fdt.GetRna().GetExt().GetTRNA(), str_cons); 
                   // str_cons not used?????
              }
              else if ((is_legal_qual && legal_qual == eFeat_qual_legal_anticodon)
                         || (is_illegal_qual 
                                 && DoesStringMatchConstraint ("anticodon", illegal_qual))) {
                 /* anticodon */
                 if (is_trna)
                     return GetAnticodonLocString (seq_fdt.GetRna().GetExt().GetTRNA());
              }
              else if ((is_legal_qual && legal_qual == eFeat_qual_legal_tag_peptide)
                         || (is_illegal_qual 
                                && DoesStringMatchConstraint ("tag-peptide", illegal_qual))) {
                 /* tag-peptide */
                 if (is_rna_gen && seq_fdt.GetRna().GetExt().GetGen().CanGetQuals()) { 
                    ITERATE (list <CRef <CRNA_qual> >, it, 
                                     seq_fdt.GetRna().GetExt().GetGen().GetQuals().Get()) {
                       if ((*it)->GetQual() == "tag_peptide" 
                                  && !(str = (*it)->GetVal()).empty()
                                  && DoesStringMatchConstraint(str, str_cons))
                            break;
                       else str = kEmptyStr;
                    }
                 }
                 return str;
              }
              else if ((is_legal_qual && legal_qual == eFeat_qual_legal_ncRNA_class)
                         || (is_illegal_qual 
                                 && DoesStringMatchConstraint ("ncRNA_class", illegal_qual))) {
                 /* ncRNA_class */
                 if (is_rna_gen && seq_fdt.GetRna().GetExt().GetGen().CanGetClass()) {
                     str = seq_fdt.GetRna().GetExt().GetGen().GetClass();
                 }
              }
              else if (is_legal_qual && legal_qual == eFeat_qual_legal_codon_start) {
                /* codon-start */
                if (has_cd) {
                    const CCdregion& cd = seq_fdt.GetCdregion();
                    if (!cd.CanGetFrame()) str = "1";
                    else str = NStr::UIntToString( (unsigned)cd.GetFrame());
                }
              }
              else if (seq_fdt.IsRegion() && is_legal_qual 
                                                && legal_qual == eFeat_qual_legal_name) {
                 /* special region qualifiers */
                 str = seq_fdt.GetRegion();
              } 
              else if (is_legal_qual && seq_feat.CanGetQual()) { // actual GenBank qualifiers
                 string feat_qual_name = thisInfo.featquallegal_name[legal_qual];
                 bool has_subfield = thisInfo.featquallegal_subfield.find(legal_qual) 
                                                   != thisInfo.featquallegal_subfield.end();
                 unsigned subfield = has_subfield ? 
                                       thisInfo.featquallegal_subfield[legal_qual] : 0;
                 if (feat_qual_name != "name" || feat_qual_name != "location") {// gbqual > -1
                    return GetFirstGBQualMatch(seq_feat.GetQual(), feat_qual_name, 
                                                                           subfield, str_cons);
                 }
              }
              else {
                 //GetFirstGBQualMatchConstraintName:the arg qua_name=first->data.ptrval is 0?
                 ITERATE (vector <CRef <CGb_qual> >, it, seq_feat.GetQual()) {
                    str = (*it)->GetVal();
                    if (!str.empty() && DoesStringMatchConstraint(str, str_cons)) return str;
                    else str = kEmptyStr;
                 }
              }
           }
       }
    }
  }

  if (!str.empty() || DoesStringMatchConstraint(str, str_cons)) str = kEmptyStr;
  return str;
}; // GetQualFromFeatureAnyType


string CSuspectRuleCheck :: GetQualFromFeature(const CSeq_feat& seq_feat, const CFeature_field& feat_field, const CString_constraint* str_cons)
{
  EMacro_feature_type feat_field_type = feat_field.GetType();
  if (eMacro_feature_type_any != feat_field_type
            && seq_feat.GetData().GetSubtype() != thisInfo.feattype_featdef[feat_field_type])
         return (kEmptyStr);
  return (GetQualFromFeatureAnyType(seq_feat, feat_field.GetField(), str_cons));
};

bool CSuspectRuleCheck :: DoesObjectMatchFeatureFieldConstraint(const CSeq_feat& feat, const CFeature_field& feat_field, const CString_constraint& str_cons)
{
  if (IsStringConstraintEmpty (&str_cons)) return true;

  CString_constraint tmp_cons;
  strtmp = Blob2Str(str_cons);
  Str2Blob(strtmp, tmp_cons);
  tmp_cons.SetNot_present(false);
  string str = GetQualFromFeature (feat, feat_field, &tmp_cons);
  bool rval = str.empty() ? false : true;
  if (str_cons.GetNot_present()) rval = !rval;
  return rval;
};

bool CSuspectRuleCheck :: DoesFeatureMatchRnaType(const CSeq_feat& seq_feat, const CRna_feat_type& rna_type)
{
  bool rval = false;
  if (rna_type.IsAny()) return true;
  const CRNA_ref& rna_ref = seq_feat.GetData().GetRna();

  if (rna_ref.GetType() == thisInfo.rnafeattp_rnareftp[rna_type.Which()]) {
    switch (rna_type.Which()) {
      case CRna_feat_type::e_NcRNA:
        if (rna_type.GetNcRNA().empty()) rval = true;
        else if (rna_ref.CanGetExt() && rna_ref.GetExt().IsGen()
                     && rna_ref.GetExt().GetGen().CanGetClass()
                     && rna_ref.GetExt().GetGen().GetClass() == rna_type.GetNcRNA())
                rval = true;
        break;
      case CRna_feat_type::e_TmRNA:
        rval = true;
        break;
      case CRna_feat_type::e_MiscRNA:
        rval = true;
        break;
      default: rval = true;
    }
  }
  return rval;
};

string CSuspectRuleCheck :: GetRNAQualFromFeature(const CSeq_feat& seq_feat, const CRna_qual& rna_qual, const CString_constraint* str_cons)
{
   if (!seq_feat.GetData().IsRna() || !DoesFeatureMatchRnaType(seq_feat, rna_qual.GetType())) 
           return kEmptyStr;
   CFeat_qual_choice feat_qual;
   feat_qual.SetLegal_qual(thisInfo.rnafield_featquallegal[rna_qual.GetField()]);
   return GetQualFromFeatureAnyType (seq_feat, feat_qual, str_cons);
};
   
bool CSuspectRuleCheck :: DoesObjectMatchRnaQualConstraint (const CSeq_feat& seq_feat, const CRna_qual& rna_qual, const CString_constraint& str_cons)
{
  bool           rval = false;
  string         str;

  CString_constraint tmp_cons;
  if (IsStringConstraintEmpty (&str_cons)) return true;
  strtmp = Blob2Str(str_cons);
  Str2Blob(strtmp, tmp_cons);
  tmp_cons.SetNot_present(false);
  str = GetRNAQualFromFeature (seq_feat, rna_qual, &tmp_cons);
  if (!str.empty()) rval = true;
  if (str_cons.GetNot_present()) rval = !rval;
  return rval;
};

void CSuspectRuleCheck :: MakeFeatureField(CRef <CFeature_field>& f, CFeat_qual_choice& f_qual, EMacro_feature_type f_tp, EFeat_qual_legal legal_qual)
{
   f->SetType(f_tp);
   f_qual.SetLegal_qual(legal_qual);
};

CRef <CFeature_field> CSuspectRuleCheck :: FeatureFieldFromCDSGeneProtField (ECDSGeneProt_field cds_gene_prot_field)
{
  CRef <CFeature_field> f(new CFeature_field);
  CFeat_qual_choice f_qual;
  f->SetField(f_qual);

  switch (cds_gene_prot_field) {
    case eCDSGeneProt_field_cds_comment:
      MakeFeatureField(f, f_qual, eMacro_feature_type_cds, eFeat_qual_legal_note);
      break;
    case eCDSGeneProt_field_cds_inference:
      MakeFeatureField(f, f_qual, eMacro_feature_type_cds, eFeat_qual_legal_inference);
      break;
    case eCDSGeneProt_field_codon_start:
      MakeFeatureField(f, f_qual, eMacro_feature_type_cds, eFeat_qual_legal_codon_start);
      break;
    case eCDSGeneProt_field_gene_locus:
      MakeFeatureField(f, f_qual, eMacro_feature_type_gene, eFeat_qual_legal_gene);
      break;
    case eCDSGeneProt_field_gene_description:
      MakeFeatureField(f, f_qual, eMacro_feature_type_gene, eFeat_qual_legal_gene_description);
      break;
    case eCDSGeneProt_field_gene_comment:
      MakeFeatureField(f, f_qual, eMacro_feature_type_gene, eFeat_qual_legal_note);
      break;
    case eCDSGeneProt_field_gene_allele:
      MakeFeatureField(f, f_qual, eMacro_feature_type_gene, eFeat_qual_legal_allele);
      break;
    case eCDSGeneProt_field_gene_maploc:
      MakeFeatureField(f, f_qual, eMacro_feature_type_gene, eFeat_qual_legal_map);
      break;
    case eCDSGeneProt_field_gene_locus_tag:
      MakeFeatureField(f, f_qual, eMacro_feature_type_gene, eFeat_qual_legal_locus_tag);
      break;
    case eCDSGeneProt_field_gene_synonym:
      MakeFeatureField(f, f_qual, eMacro_feature_type_gene, eFeat_qual_legal_synonym);
      break;
    case eCDSGeneProt_field_gene_old_locus_tag:
      MakeFeatureField(f, f_qual, eMacro_feature_type_gene, eFeat_qual_legal_old_locus_tag);
      break;
    case eCDSGeneProt_field_gene_inference:
      MakeFeatureField(f, f_qual, eMacro_feature_type_gene, eFeat_qual_legal_inference);
      break;
    case eCDSGeneProt_field_mrna_product:
      MakeFeatureField(f, f_qual, eMacro_feature_type_mRNA, eFeat_qual_legal_product);
      break;
    case eCDSGeneProt_field_mrna_comment:
      MakeFeatureField(f, f_qual, eMacro_feature_type_mRNA, eFeat_qual_legal_note);
      break;
    case eCDSGeneProt_field_prot_name:
      MakeFeatureField(f, f_qual, eMacro_feature_type_prot, eFeat_qual_legal_product);
      break;
    case eCDSGeneProt_field_prot_description:
      MakeFeatureField(f, f_qual, eMacro_feature_type_prot, eFeat_qual_legal_description);
      break;
    case eCDSGeneProt_field_prot_ec_number:
      MakeFeatureField(f, f_qual, eMacro_feature_type_prot, eFeat_qual_legal_ec_number);
      break;
    case eCDSGeneProt_field_prot_activity:
      MakeFeatureField(f, f_qual, eMacro_feature_type_prot, eFeat_qual_legal_activity);
      break;
    case eCDSGeneProt_field_prot_comment:
      MakeFeatureField(f, f_qual, eMacro_feature_type_prot, eFeat_qual_legal_note);
      break;
    case eCDSGeneProt_field_mat_peptide_name:
      MakeFeatureField(f,f_qual, eMacro_feature_type_mat_peptide_aa, eFeat_qual_legal_product);
      break;
    case eCDSGeneProt_field_mat_peptide_description:
      MakeFeatureField(f, f_qual, eMacro_feature_type_mat_peptide_aa, 
                                                           eFeat_qual_legal_description);
      break;
    case eCDSGeneProt_field_mat_peptide_ec_number:
      MakeFeatureField(f,f_qual,eMacro_feature_type_mat_peptide_aa,eFeat_qual_legal_ec_number);
      break;
    case eCDSGeneProt_field_mat_peptide_activity:
      MakeFeatureField(f,f_qual,eMacro_feature_type_mat_peptide_aa, eFeat_qual_legal_activity);
      break;
    case eCDSGeneProt_field_mat_peptide_comment:
      MakeFeatureField(f, f_qual, eMacro_feature_type_mat_peptide_aa, eFeat_qual_legal_note);
      break;
    default: break;
  }
  return f;
}

bool CSuspectRuleCheck :: DoesObjectMatchFieldConstraint(const CSeq_feat& data, const CField_constraint& field_cons)
{
  const CString_constraint& str_cons = field_cons.GetString_constraint();
  if (IsStringConstraintEmpty (&str_cons)) return true;

  bool rval = false;
  string str(kEmptyStr);
  const CField_type& field_type = field_cons.GetField();
  switch (field_type.Which()) {
    case CField_type::e_Source_qual:
      {
        const CBioSource* biosrc = GetBioSource(m_bioseq_hl);
        if (biosrc)
            str = GetSrcQualValue4FieldType(*biosrc, field_type.GetSource_qual(), &str_cons);
        if (!str.empty()) rval = true;
      }
      break;
    case CField_type:: e_Feature_field:
      {
          const CFeature_field& feat_field = field_type.GetFeature_field();
          rval = DoesObjectMatchFeatureFieldConstraint(data, feat_field, str_cons);
      }
      break;
    case CField_type:: e_Rna_field:
      rval = DoesObjectMatchRnaQualConstraint (data, field_type.GetRna_field(), str_cons);
    case CField_type:: e_Cds_gene_prot:
      {
         CRef <CFeature_field> 
             feat_field = FeatureFieldFromCDSGeneProtField (field_type.GetCds_gene_prot());
         rval = DoesObjectMatchFeatureFieldConstraint (
                                    data, const_cast<CFeature_field&>(*feat_field), str_cons);
      }
      break;
    case CField_type:: e_Molinfo_field:
      if (m_bioseq_hl) {
        str = GetSequenceQualFromBioseq (field_type.GetMolinfo_field());
        if ( (str.empty() && str_cons.GetNot_present()) 
                || (!str.empty() && DoesStringMatchConstraint (str, &str_cons)) )
            rval = true;
      }
      break;
    case CField_type:: e_Misc:
    case CField_type:: e_Dblink:
      if (m_bioseq_hl) {
        str = GetFieldValueForObjectEx (field_type, str_cons);
        if (!str.empty()) rval = true;
      }
      break;

/* TODO LATER */ 
    case CField_type:: e_Pub:
      break;
    default: break;
  }
  return rval; 
};

bool CSuspectRuleCheck :: IsNonTextSourceQualPresent(const CBioSource& biosrc, ESource_qual srcqual)
{
  if (thisInfo.srcqual_orgmod.find(srcqual) != thisInfo.srcqual_orgmod.end()) {
     if (biosrc.IsSetOrgMod()) {
       ITERATE (list <CRef <COrgMod> >, it, biosrc.GetOrgname().GetMod()) {
          if ( (*it)->GetSubtype() == thisInfo.srcqual_orgmod[srcqual]) return true;
       }
     }
  }
  else if (thisInfo.srcqual_subsrc.find(srcqual) != thisInfo.srcqual_subsrc.end()) {
     if (biosrc.CanGetSubtype()) {
        ITERATE (list <CRef <CSubSource> >, it, biosrc.GetSubtype()) {
           if ( (*it)->GetSubtype() == thisInfo.srcqual_subsrc[srcqual] ) return true;
        }
     }
  }
  return false;
};

bool CSuspectRuleCheck :: IsSourceQualPresent (const CBioSource& biosrc, const CSource_qual_choice* src_choice)
{
  bool rval = false;
  string   str;

  if (!src_choice) return false;
  switch (src_choice->Which()) {
    case CSource_qual_choice::e_Textqual:
      {
         ESource_qual src_qual = src_choice->GetTextqual();
         // IsNonTextSourceQual (scp->data.intvalue)) {
         if ( src_qual == eSource_qual_transgenic || src_qual == eSource_qual_germline
                  || src_qual == eSource_qual_metagenomic
                  || src_qual == eSource_qual_environmental_sample
                  || src_qual == eSource_qual_rearranged ) {
              rval = IsNonTextSourceQualPresent (biosrc, src_qual);
         }
         else {
           str = GetSrcQualValue4FieldType(biosrc, *src_choice, 0);
           if (!str.empty()) rval = true;
         }
         break;
      }
    case CSource_qual_choice::e_Location:
      if (biosrc.GetGenome() != CBioSource::eGenome_unknown) rval = true;
      break;
    case CSource_qual_choice::e_Origin:
      if (biosrc.GetOrigin() != CBioSource::eOrigin_unknown) rval = true;
      break;
    default: break;
  }
  return rval;
};

bool CSuspectRuleCheck :: AllowSourceQualMulti(const CSource_qual_choice* src_cons)
{
   if (!src_cons) return false;
   if (src_cons->IsTextqual()) {
     ESource_qual src_qual = src_cons->GetTextqual();
     if ( src_qual == eSource_qual_culture_collection || src_qual == eSource_qual_bio_material
            || src_qual == eSource_qual_specimen_voucher || src_qual == eSource_qual_dbxref
            || src_qual == eSource_qual_fwd_primer_name
            || src_qual == eSource_qual_fwd_primer_seq
            || src_qual == eSource_qual_rev_primer_name
            || src_qual == eSource_qual_rev_primer_seq)
        return true;
   }
   return false;
};

bool CSuspectRuleCheck :: DoesBiosourceMatchConstraint (const CBioSource& biosrc, const CSource_constraint& src_cons)
{
  bool rval = false;
  string str1, str2;
  const CSource_qual_choice* 
            field1 = src_cons.CanGetField1() ? &src_cons.GetField1() : 0;
  const CSource_qual_choice* 
            field2 = src_cons.CanGetField2() ? &src_cons.GetField2() : 0;
  const CString_constraint* 
      str_cons = src_cons.CanGetConstraint() ? &src_cons.GetConstraint() : 0;
  if (IsStringConstraintEmpty(str_cons)) {
    /* looking for qual present */
    if (field1 && !field2) {
           rval = IsSourceQualPresent (biosrc, field1);
    }
    else if (field2 && !field1) {
         rval = IsSourceQualPresent (biosrc, field2);
    }
    else if (field1 && field2) { /* looking for quals to match */
      str1 = GetSrcQualValue4FieldType (biosrc, *field1);
      str2 = GetSrcQualValue4FieldType (biosrc, *field2);
      if (str1 == str2) {
         rval = true;
      }
    } 
    else {
       rval = true; /* nothing specified, automatic match */
    }
  } 
  else {
    if (field1 && !field2) {
      if ( AllowSourceQualMulti(field1) && str_cons->GetNot_present() ) {
          CString_constraint tmp_cons;
          strtmp = Blob2Str(*str_cons);
          Str2Blob(strtmp, tmp_cons);
          tmp_cons.SetNot_present(false);
          str1 = GetSrcQualValue4FieldType (biosrc, *field1, &tmp_cons);
          if (!str1.empty()) rval = false;
          else rval = true;
      } 
      else {
        str1 = GetSrcQualValue4FieldType (biosrc, *field1, str_cons);
        if (str1.empty()) {
          if (str_cons->GetNot_present()) {
            str1 = GetSrcQualValue4FieldType (biosrc, *field1, 0);
            if (str1.empty()) rval = true;
          }
        } 
        else rval = true;
      }
    } 
    else if (field2 && !field1) {
      str2 = GetSrcQualValue4FieldType (biosrc, *field2, str_cons);
      if (str2.empty()) {
        if (str_cons->GetNot_present()) {
          str2 = GetSrcQualValue4FieldType (biosrc, *field2);
          if (str2.empty()) rval = true;
        }
      } 
      else rval = true;
    }
    else if (field1 && field2 ) {
      str1 = GetSrcQualValue4FieldType (biosrc, *field1, str_cons);
      str2 = GetSrcQualValue4FieldType (biosrc, *field2, str_cons);
      if (str1 == str2) rval = true;
    } 
    else /* generic string constraint */
      rval = DoesObjectMatchStringConstraint (biosrc, *str_cons);
  }
  return rval;
};

CConstRef <CSeq_feat> CSuspectRuleCheck::AddProtFeatForCds(const CSeq_feat& cd_feat, const CBioseq_Handle& protbsp)
{
  CRef <CSeq_id> prot_id (new CSeq_id());
  prot_id->Assign(*(protbsp.GetId().front().GetSeqId()));
  CRef <CSeq_feat> prot_feat(new CSeq_feat);
  prot_feat->SetData().SetProt();
  prot_feat->SetLocation().SetInt().SetId(*prot_id);
  prot_feat->SetLocation().SetInt().SetFrom(0);
  prot_feat->SetLocation().SetInt().SetTo(protbsp.GetInst_Length() - 1);
  bool partial5 = cd_feat.GetLocation().IsPartialStart(eExtreme_Biological);
  bool partial3 = cd_feat.GetLocation().IsPartialStop(eExtreme_Biological);
  prot_feat->SetLocation().SetPartialStart(partial5, eExtreme_Biological);
  prot_feat->SetLocation().SetPartialStop(partial3, eExtreme_Biological);
  if (partial5 || partial3) prot_feat->SetPartial(true);
  else prot_feat->ResetPartial();
  return prot_feat;
};

// part of BuildCGPSetFromCodingRegion
void CSuspectRuleCheck :: GetProtFromCodingRegion (CRef <CCGPSetData>& cgp, const CSeq_feat& cd_feat)
{
  if (cd_feat.CanGetProduct()) {
    CBioseq_Handle prot_bhl = GetBioseqFromSeqLoc (cd_feat.GetProduct(), *thisInfo.scope);
    if (prot_bhl)
    {
      cgp->prot = GetPROTForProduct(prot_bhl);
      /* if there is no full-length protein feature, make one */
      if (cgp->prot.Empty())
      {
        cgp->prot = AddProtFeatForCds (cd_feat, prot_bhl);
// Why?       if (prot != NULL) ResynchCDSPartials (cds, NULL);
      }

      /* also add in mat_peptides from protein feature */
      for (CFeat_CI fci(prot_bhl, SAnnotSelector(CSeqFeatData::eSubtype_mat_peptide_aa).SetByProduct()); fci; ++fci)
          cgp->mat_peptide_list.push_back(fci->GetSeq_feat());
    }
  }
};

string CSuspectRuleCheck :: GetTwoFieldSubfield(const string& str, unsigned subfield)
{
  if (str.empty() || subfield > 2) return kEmptyStr;
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

string CSuspectRuleCheck :: GetFirstGBQualMatch (const vector <CRef <CGb_qual> >& quals, const string& qual_name, unsigned subfield, const CString_constraint* str_cons)
{
  string str(kEmptyStr);
  ITERATE (vector <CRef <CGb_qual> >, it, quals) {
     if (NStr::EqualNocase( (*it)->GetQual(), qual_name)) {
        str = (*it)->GetVal();
        if (subfield) {
            str = GetTwoFieldSubfield(str, subfield);
        }
        if (str.empty() || !DoesStringMatchConstraint (str, str_cons)) {
           str = kEmptyStr;
        }
        else break;
     }
  }
  return str;
}

string CSuspectRuleCheck :: GetFieldValueFromCGPSet (const CCGPSetData& c, ECDSGeneProt_field field, const CString_constraint* str_cons)
{
  string str(kEmptyStr);
  if (c.cds) {
    switch (field) {
      case eCDSGeneProt_field_cds_comment:
      case eCDSGeneProt_field_cds_inference:
      case eCDSGeneProt_field_codon_start:
        {
          CRef <CFeature_field> ffield = FeatureFieldFromCDSGeneProtField (field);
          if (c.cds) 
             str = GetQualFromFeature(*c.cds, const_cast<CFeature_field&>(*ffield), str_cons);
          break;
        }
      case eCDSGeneProt_field_gene_locus:
      case eCDSGeneProt_field_gene_inference:
        {
          CRef <CFeature_field> ffield = FeatureFieldFromCDSGeneProtField (field);
          if (c.gene) 
             str = GetQualFromFeature(*c.gene, const_cast<CFeature_field&>(*ffield), str_cons);
          break;
        }
      default: break;
    }
  }
  else if (c.gene) {
    const CGene_ref& g_ref = c.gene->GetData().GetGene();
    switch (field) {
      case eCDSGeneProt_field_gene_description:
        if (g_ref.CanGetDesc()) str = c.gene->GetData().GetGene().GetDesc();
        break;
      case eCDSGeneProt_field_gene_comment:
        if (c.gene->CanGetComment()) str = c.gene->GetComment();
        break;
      case eCDSGeneProt_field_gene_allele:
        if (g_ref.CanGetAllele()) str = c.gene->GetData().GetGene().GetAllele();
        break;
      case eCDSGeneProt_field_gene_maploc:
        if (g_ref.CanGetMaploc()) str = c.gene->GetData().GetGene().GetMaploc();
        break;
      case eCDSGeneProt_field_gene_locus_tag:
        if (g_ref.CanGetLocus_tag()) str = g_ref.GetLocus_tag();
        break;
      case eCDSGeneProt_field_gene_synonym:
        if (g_ref.CanGetSyn()) str = GetFirstStringMatch(g_ref.GetSyn(), str_cons);
        break;
      case eCDSGeneProt_field_gene_old_locus_tag:
        if (c.gene->CanGetQual()) 
              str = GetFirstGBQualMatch(c.gene->GetQual(), "old_locus_tag", 0, str_cons);
        break;
      default: break;
    }
  }
  else if (c.mrna) {
    const CRNA_ref& r_ref = c.mrna->GetData().GetRna();
    switch (field) {
      case eCDSGeneProt_field_mrna_product:
        if (r_ref.CanGetExt() && r_ref.GetExt().IsName())
            str = r_ref.GetExt().GetName();
        break; 
      case eCDSGeneProt_field_mrna_comment:
        if (c.mrna->CanGetComment()) str = c.mrna->GetComment();
        break;
      default: break;
    }
  }
  else if (!c.prot) {
    const CProt_ref& p_ref = c.prot->GetData().GetProt();
    switch (field) {
      case eCDSGeneProt_field_prot_name:
        if (p_ref.CanGetName()) str = GetFirstStringMatch(p_ref.GetName(), str_cons);
        break;
      case eCDSGeneProt_field_prot_description:
        if (p_ref.CanGetDesc()) str = p_ref.GetDesc();
        break;
      case eCDSGeneProt_field_prot_ec_number:
        if (p_ref.CanGetEc()) str = GetFirstStringMatch(p_ref.GetEc(), str_cons);
        break;
      case eCDSGeneProt_field_prot_activity:
        if (p_ref.CanGetActivity()) str = GetFirstStringMatch(p_ref.GetActivity(), str_cons);
        break;
      case eCDSGeneProt_field_prot_comment:
        if (c.prot->CanGetComment()) str = c.prot->GetComment();
        break;
      default: break;
    }
  }
  else if (!c.mat_peptide_list.empty()) {
    switch (field) {
      case eCDSGeneProt_field_mat_peptide_name:
        ITERATE (vector <const CSeq_feat*>, it, c.mat_peptide_list) {
           if ((*it)->GetData().GetProt().CanGetName()) {
              str = GetFirstStringMatch((*it)->GetData().GetProt().GetName(), str_cons);
              if (!str.empty()) break;
           }
        }
        break;
      case eCDSGeneProt_field_mat_peptide_description:
        ITERATE (vector <const CSeq_feat*>, it, c.mat_peptide_list) {
           if ((*it)->GetData().GetProt().CanGetDesc()) {
              str = (*it)->GetData().GetProt().GetDesc();
              if (!str.empty() && DoesStringMatchConstraint(str, str_cons)) break;
           }
        }
        break;
      case eCDSGeneProt_field_mat_peptide_ec_number:
        ITERATE (vector <const CSeq_feat*>, it, c.mat_peptide_list) {
           if ((*it)->GetData().GetProt().CanGetEc()) {
              str = GetFirstStringMatch((*it)->GetData().GetProt().GetEc(), str_cons);
              if (!str.empty()) break;
           }
        }
        break;
      case eCDSGeneProt_field_mat_peptide_activity:
        ITERATE (vector <const CSeq_feat*>, it, c.mat_peptide_list) {
           if ((*it)->GetData().GetProt().CanGetActivity()) {
              str = GetFirstStringMatch((*it)->GetData().GetProt().GetActivity(), str_cons);
              if (!str.empty()) break;
           }
        }
        break;
      case eCDSGeneProt_field_mat_peptide_comment:
        ITERATE (vector <const CSeq_feat*>, it, c.mat_peptide_list) {
           if ((*it)->CanGetComment()) {
              str = (*it)->GetComment();
              if (!str.empty() && DoesStringMatchConstraint(str, str_cons)) break;
           }
        }
        break;
      default: break;
    }
  }
  if (str.empty() || !DoesStringMatchConstraint(str, str_cons)) str = kEmptyStr;
  return str;
};


bool CSuspectRuleCheck :: CanGetFieldString(const CCGPSetData& c, const ECDSGeneProt_field field, const CString_constraint& str_cons)
{
   string str = GetFieldValueFromCGPSet (c, field, &str_cons);
   if (str.empty()) {
        if (str_cons.GetNot_present()) {
          str = GetFieldValueFromCGPSet (c, field, 0);
          if (str.empty()) return true;
        }
   }
   else return true;
   return false;
};

bool CSuspectRuleCheck :: DoesCGPSetMatchQualConstraint (const CCGPSetData& c, const CCDSGeneProt_qual_constraint& cgp_cons)
{
  const CString_constraint* 
      str_cons = cgp_cons.CanGetConstraint() ? &(cgp_cons.GetConstraint()) : 0; 
  bool has_field1 = cgp_cons.CanGetField1() && cgp_cons.GetField1().IsField();
  bool has_field2 = cgp_cons.CanGetField2() && cgp_cons.GetField2().IsField();
  string str(kEmptyStr), str1, str2;
  bool rval = false;
  if (!str_cons || IsStringConstraintEmpty (str_cons)) {
    /* looking for qual present */
    if (has_field1 && !has_field2) {
      str = GetFieldValueFromCGPSet (c, cgp_cons.GetField1().GetField());
      if (!str.empty()) rval = true;
    } 
    else if (has_field2 && !has_field1) {
      str = GetFieldValueFromCGPSet (c, cgp_cons.GetField2().GetField());
      if (str.empty()) rval = false;
    } 
    else if (has_field1 && has_field2) { /* looking for quals to match */
      str1 = GetFieldValueFromCGPSet (c, cgp_cons.GetField1().GetField());
      str2 = GetFieldValueFromCGPSet (c, cgp_cons.GetField2().GetField());
      if (str1 == str2) rval = true;
    } 
    else rval = true; /* nothing specified, automatic match */
  } 
  else {
    if (has_field1 && !has_field2) {
      rval = CanGetFieldString(c, cgp_cons.GetField1().GetField(), *str_cons);
    } 
    else if (has_field2 && !has_field1) {
      rval = CanGetFieldString(c, cgp_cons.GetField2().GetField(), *str_cons);
    } 
    else if (has_field1 && has_field2 ) {
      str1 = GetFieldValueFromCGPSet (c, cgp_cons.GetField1().GetField(), str_cons);
      str2 = GetFieldValueFromCGPSet (c, cgp_cons.GetField2().GetField(), str_cons);
      if (str1 == str2) rval = true;
    } 
    else {
      /* generic string constraint */
      rval = DoesObjectMatchStringConstraint (c, *str_cons);
    }
  }
  return rval;
};

bool CSuspectRuleCheck :: DoesFeatureMatchCGPQualConstraint (const CSeq_feat& feat, const CCDSGeneProt_qual_constraint& cons)
{
  CRef <CCGPSetData> cgp_set (new CCGPSetData);
  const CSeqFeatData& feat_dt = feat.GetData();
  if (feat_dt.IsCdregion() || feat_dt.IsProt()) {
    if (feat_dt.IsProt()) cgp_set->cds = sequence::GetCDSForProduct(m_bioseq_hl);
    else cgp_set->cds = &feat;
    cgp_set->gene =CTestAndRepData :: GetGeneForFeature(*(cgp_set->cds)).GetPointer(); // ??why return ref
    cgp_set->mrna = GetBestMrnaForCds(*(cgp_set->cds), *thisInfo.scope);

    GetProtFromCodingRegion(cgp_set, *(cgp_set->cds)); 
  } 
  else if (feat_dt.IsGene()) cgp_set->gene = &feat;
  else if (feat_dt.IsRna() && feat_dt.GetSubtype() == CSeqFeatData::eSubtype_mRNA) {
     // c = BuildCGPSetFrommRNA (feat);
     cgp_set->mrna = &feat;
     cgp_set->gene = CTestAndRepData::GetGeneForFeature(feat).GetPointer();
  }
  if (!cgp_set->cds && !cgp_set->gene && !cgp_set->mrna && cgp_set->prot.Empty() 
                                     && cgp_set->mat_peptide_list.empty()) 
      return false;

  bool rval = DoesCGPSetMatchQualConstraint (*cgp_set, cons);
  bool has_field1_dt = (cons.CanGetField1() && cons.GetField1().IsField());
  bool has_field2_dt = (cons.CanGetField2() && cons.GetField2().IsField());
  string str1, str2;
  const CString_constraint* 
      str_cons = cons.CanGetConstraint()? &(cons.GetConstraint()) : 0;
  if (rval && feat_dt.GetSubtype() == CSeqFeatData::eSubtype_mat_peptide_aa) {
    if (has_field1_dt) {
      str1 = GetFieldValue(*cgp_set, feat, cons.GetField1().GetField(), str_cons);
      if (str1.empty()) rval = false;
    }
    if (has_field2_dt) {
      str2 = GetFieldValue(*cgp_set, feat, cons.GetField2().GetField(), str_cons);
      if (str2.empty()) rval = false;
    }
    if (rval && cons.CanGetField1() && cons.CanGetField2() && str1 != str2) rval = false;
  }
  return rval;
};

const CSeq_feat* CSuspectRuleCheck :: GetmRNAforCDS (const CSeq_feat& cds)
{
  const CSeq_feat* mrna = 0;
  /* first, check for mRNA identified by feature xref */
  if (cds.CanGetXref()) {
     CBioseq_Handle bioseq_hl = GetBioseqFromSeqLoc(cds.GetLocation(), *thisInfo.scope);
     CTSE_Handle tse_hl = bioseq_hl.GetTSE_Handle();
     SAnnotSelector sel;
     sel.IncludeFeatSubtype(CSeqFeatData::eSubtype_mRNA);
     ITERATE (vector <CRef <CSeqFeatXref> >, it, cds.GetXref()) {  // ?? which _CI constructor?
        if ( (*it)->CanGetId()) {
          const CFeat_id& feat_id = (*it)->GetId();
          switch ( feat_id.Which()) {
            case CFeat_id::e_Gibb:
             {
               CFeat_CI feat_ci(tse_hl, sel, feat_id.GetGibb()); 
               if (feat_ci) mrna = &(feat_ci->GetOriginalFeature());
               break;
             }    
            case CFeat_id::e_Giim:
             {
               CFeat_CI feat_ci(tse_hl, sel, feat_id.GetGiim().GetId());
               if (feat_ci) mrna = &(feat_ci->GetOriginalFeature());
               break;
             }
            case CFeat_id::e_Local:
             {
               CFeat_CI feat_ci(tse_hl, sel, feat_id.GetLocal());
               if (feat_ci) mrna = &(feat_ci->GetOriginalFeature());
               break;
             }
            case CFeat_id::e_General:
             {
               CFeat_CI feat_ci(tse_hl, sel, feat_id.GetGeneral().GetTag());
               if (feat_ci) mrna = &(feat_ci->GetOriginalFeature());
               break;
             }
            default: break;
          }
        }
     }
  }
/*
  for (xref = cds->xref; xref != NULL && mrna == NULL; xref = xref->next) {
    if (xref->id.choice != 0) {
      mrna = SeqMgrGetFeatureByFeatID (cds->idx.entityID, NULL, NULL, xref, NULL);
      if (mrna != NULL && mrna->idx.subtype != FEATDEF_mRNA) {
        mrna = NULL;
      }
    }
  }
*/
  
  /* try by location if not by xref */
  if (!mrna) {
     //SeqMgrGetLocationSupersetmRNA (cds->location, &mcontext);
    mrna = GetBestOverlappingFeat(cds.GetLocation(), CSeqFeatData::eSubtype_mRNA, 
             eOverlap_Subset, *thisInfo.scope).GetPointer(); 
    if (!mrna) {
       // SeqMgrGetOverlappingmRNA (cds->location, &mcontext);
      mrna = GetBestOverlappingFeat(cds.GetLocation(), CSeqFeatData::eSubtype_mRNA,
                  eOverlap_Contained, *thisInfo.scope).GetPointer();
    }
  }

  if (!mrna) {
    //BioseqFindFromSeqLoc (cds->location);
    CBioseq_Handle mbsp = GetBioseqFromSeqLoc(cds.GetLocation(), *thisInfo.scope); 
    if (mbsp && CTestAndRepData :: IsmRNASequenceInGenProdSet(*(mbsp.GetCompleteBioseq()))) {
      // SeqMgrGetRNAgivenProduct(mbsp, &mcontext);
      mrna = GetmRNAForProduct(mbsp); 
    }
  }

  return mrna;
};

void CSuspectRuleCheck :: ListFeaturesInLocation (const CSeq_loc& seq_loc, CSeqFeatData::ESubtype subtype, vector <const CSeq_feat*> feat_list)
{
  unsigned loc_left = seq_loc.GetStart(eExtreme_Positional);
  unsigned loc_right = seq_loc.GetStop(eExtreme_Positional);
  SAnnotSelector sel;
  sel.IncludeFeatSubtype(subtype);
  for (CFeat_CI ci(m_bioseq_hl, sel); ci; ++ci) {
     const CSeq_feat& feat = ci->GetOriginalFeature();
     const CSeq_loc& feat_loc = feat.GetLocation();
     if (feat_loc.GetStart(eExtreme_Positional) > loc_right) break;
     if (feat_loc.GetStop(eExtreme_Positional) < loc_left) continue;
     sequence::ECompare loc_comp = Compare(feat_loc, seq_loc, thisInfo.scope);
     if (loc_comp == sequence::eContained) feat_list.push_back(&feat);
  }
};

bool CSuspectRuleCheck :: DoesFeatureMatchCGPPseudoConstraint (const CSeq_feat& seq_feat, const CCDSGeneProt_pseudo_constraint& cgp_p_cons)
{
  bool any_pseudo = false;
  const CSeqFeatData& sf_dt = seq_feat.GetData();
  const CSeq_loc& sf_loc = seq_feat.GetLocation();
  bool sf_is_pseudo = (seq_feat.CanGetPseudo() && seq_feat.GetPseudo());
  switch (cgp_p_cons.GetFeature()) {
    case eCDSGeneProt_feature_type_constraint_gene :
     { 
       if (sf_dt.IsGene()) {
         if (sf_is_pseudo) any_pseudo = true;
       } 
       else if (sf_dt.IsProt()) {
         const CSeq_feat* cds = sequence::GetCDSForProduct(m_bioseq_hl);
         if (cds) {
           CConstRef <CSeq_feat> gene = CTestAndRepData :: GetGeneForFeature (*cds);
           if (gene.NotEmpty() && gene->CanGetPseudo() && gene->GetPseudo()) any_pseudo = true;
         }
       } 
       else {
         CConstRef <CSeq_feat> gene = CTestAndRepData :: GetGeneForFeature (seq_feat);
         if (gene.NotEmpty() && gene->CanGetPseudo() && gene->GetPseudo()) any_pseudo = true;
       }
       break;
     }
    case eCDSGeneProt_feature_type_constraint_mRNA :
     {
       CConstRef <CSeq_feat> mrna(0);
       if (sf_dt.GetSubtype() == CSeqFeatData::eSubtype_mRNA) {
         if (sf_is_pseudo) any_pseudo = true;
       } 
       else if (sf_dt.IsProt()) {
         const CSeq_feat* cds = sequence::GetCDSForProduct(m_bioseq_hl);
         if (!cds) {
           mrna = GetBestMrnaForCds(*cds, *thisInfo.scope); //GetmRNAforCDS (cds); above
           if (mrna.NotEmpty() && mrna->CanGetPseudo() && mrna->GetPseudo()) any_pseudo = true;
         }
       } 
       else {
         mrna = GetBestMrnaForCds(seq_feat, *thisInfo.scope); // GetmRNAforCDS (sfp);
         if (mrna.NotEmpty() && mrna->CanGetPseudo() && mrna->GetPseudo()) any_pseudo = true;
       }
       break;
     }
    case eCDSGeneProt_feature_type_constraint_cds :
     {
       if (sf_dt.IsCdregion()) {
         if (sf_is_pseudo) any_pseudo = true;
       } 
       else if (sf_dt.IsProt()) {
         const CSeq_feat* cds = sequence::GetCDSForProduct(m_bioseq_hl);
         if (cds && cds->CanGetPseudo() && cds->GetPseudo()) any_pseudo = true;
       } 
       else {
         vector <const CSeq_feat*> feat_list;
         ListFeaturesInLocation(sf_loc,CSeqFeatData::eSubtype_cdregion,feat_list);
         ITERATE (vector <const CSeq_feat*>, it, feat_list) {
            if ( (*it)->CanGetPseudo() && (*it)->GetPseudo()) {
               any_pseudo = true; 
               break;
            }
         }
         feat_list.clear();
       }
       break;
     }
    case eCDSGeneProt_feature_type_constraint_prot :
     {
       if (sf_dt.GetSubtype() == CSeqFeatData::eSubtype_prot) {
         if (sf_is_pseudo) any_pseudo = true;
       } 
       else if (sf_dt.IsProt()) {
         const CSeq_feat* prot = GetPROTForProduct(m_bioseq_hl);
         if (prot && prot->CanGetPseudo() && prot->GetPseudo()) any_pseudo = true;
       } 
       else if (sf_dt.IsCdregion()) {
         if (seq_feat.CanGetProduct()) {
            const CSeq_feat* prot = GetPROTForProduct(
                                 GetBioseqFromSeqLoc(seq_feat.GetProduct(), *thisInfo.scope));
            if (prot && prot->CanGetPseudo() && prot->GetPseudo()) any_pseudo = true;
         }
       } 
       else {
         vector <const CSeq_feat*> feat_list;
         ListFeaturesInLocation(sf_loc,CSeqFeatData::eSubtype_cdregion,feat_list);
         ITERATE (vector <const CSeq_feat*>, it, feat_list) {
            if ( (*it)->CanGetProduct()) {
               const CSeq_feat* prot = GetPROTForProduct(
                              GetBioseqFromSeqLoc( (*it)->GetProduct(), *thisInfo.scope));
               if (prot && prot->CanGetPseudo() && prot->GetPseudo()) {
                  any_pseudo = true;
                  break;
               }
            }
         }
         feat_list.clear();
       }
       break;
     }
    case eCDSGeneProt_feature_type_constraint_mat_peptide :
      if (sf_dt.GetSubtype() == CSeqFeatData::eSubtype_mat_peptide_aa) {
        if (sf_is_pseudo) any_pseudo = true;
      } 
      break;
    default: break;
  }

  if ((any_pseudo && cgp_p_cons.GetIs_pseudo()) 
                    || (!any_pseudo && !cgp_p_cons.GetIs_pseudo())) 
      return true;
  else return false;
};

bool CSuspectRuleCheck :: IsSequenceConstraintEmpty (const CSequence_constraint& constraint)
{
  if (constraint.CanGetSeqtype() && !constraint.GetSeqtype().IsAny()) return false; // not-set?
  if (constraint.GetFeature() != eMacro_feature_type_any) return false;
  /* note - having a num_type_features not be null isn't enough to make the constraint 
     non-empty */
  if (constraint.CanGetId() && !IsStringConstraintEmpty (&(constraint.GetId()))) return false;
  if (constraint.CanGetNum_features()) return false;
  if (constraint.CanGetLength()) return false;
  if (constraint.GetStrandedness() != eFeature_strandedness_constraint_any) return false;
  return true;
};

bool CSuspectRuleCheck :: DoesValueMatchQuantityConstraint (int val, const CQuantity_constraint& quantity)
{

  switch (quantity.Which()) {
    case CQuantity_constraint::e_not_set: return true;
    case CQuantity_constraint::e_Equals:
            if (val != quantity.GetEquals()) return false;
            break;
    case CQuantity_constraint::e_Greater_than:
            if (val <= quantity.GetGreater_than()) return false;
            break;
    case CQuantity_constraint::e_Less_than:
            if (val >= quantity.GetLess_than()) return false;
  }
  return true;
};

bool CSuspectRuleCheck :: DoesFeatureCountMatchQuantityConstraint (CSeqFeatData::ESubtype featdef, const CQuantity_constraint& quantity)
{
  int num_features = 0;
  CQuantity_constraint::E_Choice choice = quantity.Which();
// what if featdef  = 0???  should be all features
  for (CFeat_CI ci(m_bioseq_hl, SAnnotSelector(featdef).SetByProduct()); ci; ++ci) {
    num_features++;
    /* note - break out of loop or return as soon as we know constraint
     * succeeds or passes - no need to iterate through all features
     */
    if (choice == CQuantity_constraint::e_not_set) return true;
    if (choice == CQuantity_constraint::e_Equals && num_features > quantity.GetEquals())
        return false;
    else if (choice == CQuantity_constraint::e_Greater_than
                       && num_features > quantity.GetGreater_than())
             break;
    else if (choice == CQuantity_constraint::e_Less_than
                 && num_features >= quantity.GetLess_than())
             return false;
  }
  if (choice == CQuantity_constraint::e_not_set) { // in case no feature
    return false;
  }
  else return (DoesValueMatchQuantityConstraint(num_features, quantity));
};

bool CSuspectRuleCheck :: DoesSequenceMatchSequenceConstraint (const CSequence_constraint& seq_cons)
{
  if (IsSequenceConstraintEmpty (seq_cons)) return true;

  if (seq_cons.CanGetSeqtype() && !seq_cons.GetSeqtype().IsAny()) {
    switch (seq_cons.GetSeqtype().Which()) {
      case CSequence_constraint_mol_type_constraint::e_Nucleotide :
        if (m_bioseq_hl.IsAa()) return false;
        break;
      case CSequence_constraint_mol_type_constraint::e_Dna :
        if (m_bioseq_hl.CanGetInst_Mol() && m_bioseq_hl.GetInst_Mol() != CSeq_inst::eMol_dna)
            return false;
        break;
      case CSequence_constraint_mol_type_constraint::e_Rna :
        if (m_bioseq_hl.CanGetInst_Mol() && m_bioseq_hl.GetInst_Mol() != CSeq_inst::eMol_rna)
             return false;
        if (seq_cons.GetSeqtype().GetRna() != eSequence_constraint_rnamol_any) {
           const CMolInfo* mol = GetMolInfo(m_bioseq_hl);
           if (!mol) return false;
           if ( thisInfo.scrna_mirna[seq_cons.GetSeqtype().GetRna()] != mol->GetBiomol())
              return false;
        }
        break;
      case CSequence_constraint_mol_type_constraint::e_Protein :
        if (!m_bioseq_hl.IsAa()) return false;
        break;
      default: break;
    }
  }

  if (seq_cons.GetFeature() != eMacro_feature_type_any) {
    if (seq_cons.CanGetNum_type_features() 
            && !DoesFeatureCountMatchQuantityConstraint (
                                  thisInfo.feattype_featdef[seq_cons.GetFeature()], 
                                  seq_cons.GetNum_type_features()))
         return false;
  }

  if (seq_cons.CanGetId() && !IsStringConstraintEmpty (&seq_cons.GetId()) 
                && !DoesSeqIDListMeetStringConstraint (m_bioseq_hl.GetId(), seq_cons.GetId()))
       return false;

  // what does it mean to SeqMgrGetNextFeature has featdefchoice = 0???
  if (seq_cons.CanGetNum_features() 
        && !DoesFeatureCountMatchQuantityConstraint (CSeqFeatData::eSubtype_bad, seq_cons.GetNum_features()))
          return false;

  if (m_bioseq_hl.CanGetInst_Length() && seq_cons.CanGetLength()
        && !DoesValueMatchQuantityConstraint((int)m_bioseq_hl.GetInst_Length(), 
                                                                      seq_cons.GetLength()))
      return false;

  if (!DoesSequenceMatchStrandednessConstraint(seq_cons.GetStrandedness()))
          return false;

  return true;
};

bool CSuspectRuleCheck :: DoesObjectMatchConstraint(const CSeq_feat& data, const vector <string>& strs, const CConstraint_choice& cons)
{
  switch (cons.Which()) {
    case CConstraint_choice::e_String :
       return DoesObjectMatchStringConstraint (data, strs, cons.GetString());
    case CConstraint_choice::e_Location :
       return DoesFeatureMatchLocationConstraint (data, cons.GetLocation());
    case CConstraint_choice::e_Field :
       return DoesObjectMatchFieldConstraint (data, cons.GetField());
    case CConstraint_choice::e_Source :
       if (data.GetData().IsBiosrc()) {
          return DoesBiosourceMatchConstraint ( data.GetData().GetBiosrc(), 
                                                cons.GetSource());
       }
       else {
          CBioseq_Handle 
             seq_hl = sequence::GetBioseqFromSeqLoc(data.GetLocation(), 
                                                    *thisInfo.scope);
          const CBioSource* src = GetBioSource(seq_hl);
          if (src) {
            return DoesBiosourceMatchConstraint(*src, cons.GetSource());
          }
          else return false;
       }
       break;
    case CConstraint_choice::e_Cdsgeneprot_qual :
       return DoesFeatureMatchCGPQualConstraint (data, 
                                                 cons.GetCdsgeneprot_qual());
/*
      if (choice == 0) {
        rval = DoesCGPSetMatchQualConstraint (data, cons->data.ptrvalue);
      } else if (choice == OBJ_SEQDESC) {
        rval = DoesSeqDescMatchCGPQualConstraint (data, cons->data.ptrvalue);
      } else if (choice == OBJ_SEQFEAT) {
        rval = DoesFeatureMatchCGPQualConstraint (data, cons->data.ptrvalue);
      } else if (choice == OBJ_BIOSEQ) {
        rval = DoesSequenceMatchCGPQualConstraint (data, cons->data.ptrvalue);
      } else {
        rval = FALSE;
      }
*/
    case CConstraint_choice::e_Cdsgeneprot_pseudo :
       return DoesFeatureMatchCGPPseudoConstraint (data, cons.GetCdsgeneprot_pseudo());
/*
      if (choice == 0) {
        rval = DoesCGPSetMatchPseudoConstraint (data, cons->data.ptrvalue);
      } else if (choice == OBJ_SEQFEAT) {
        rval = DoesFeatureMatchCGPPseudoConstraint (data, cons->data.ptrvalue);
      }
*/
    case CConstraint_choice::e_Sequence :
      {
        if (m_bioseq_hl) 
            return DoesSequenceMatchSequenceConstraint(cons.GetSequence());
//      rval = DoesObjectMatchSequenceConstraint (choice, data, cons->data.ptrvalue);
        break;
      }
    case CConstraint_choice::e_Pub:
      if (data.GetData().IsPub())
         return DoesPubMatchPublicationConstraint(data.GetData().GetPub(), cons.GetPub());
      break;
    case CConstraint_choice::e_Molinfo:
       return DoesObjectMatchMolinfoFieldConstraint (data, cons.GetMolinfo()); // use bioseq_hl
    case CConstraint_choice::e_Field_missing:
      if (GetConstraintFieldFromObject(data, cons.GetField_missing()).empty()) 
              return true; 
      else return false;
      // return DoesObjectMatchFieldMissingConstraint (data, cons.GetField_missing());
    case CConstraint_choice::e_Translation:
      /* must be coding region or protein feature */
      if (data.GetData().IsProt()) {
           const CSeq_feat* cds = sequence::GetCDSForProduct(m_bioseq_hl);
           if (cds)
              return DoesCodingRegionMatchTranslationConstraint (*cds, cons.GetTranslation());
      }
      else if (data.GetData().IsCdregion())
           return DoesCodingRegionMatchTranslationConstraint(data, cons.GetTranslation());
      // DoesObjectMatchTranslationConstraint (data, cons.GetTrandlation());
    default: break;
  }
  return true;
}

/*
void CSuspectRuleCheck :: GetFeatIdString(const CFeat_id& id, vector <string>& arr)
{
   switch (id.Which()) {
     case CFeat_id::e_Giim:
       {
         const CGiimport_id& giim = id.GetGiim();
         if (giim.CanGetDb()) arr.push_back(giim.GetDb());
         if (giim.CanGetRelease()) arr.push_back(giim.GetRelease());
       };
       break;
     case CFeat_id::e_Local:
        if (id.GetLocal().Is
     default: break;               
   }
};

void CSuspectRuleCheck :: GetStringsFromObject(const CSeq_feat& feat, vector <string>& arr)
{
   if (feat.CanGetId()) GetFeatIdString(feat.GetId(), arr);
   GetDataString(feat.GetData(), arr);
   if (feat.CanGetComment()) arr.push_back(feat.GetComment());
   if (feat.CanProduct()) GetSeqLocString(feat, arr);
   GetSeqLocString(feat, arr);
   if (feat.CanGetQual()) GetQualString(feat.GetQual(), arr);
   if (feat.CanGetTitle()) arr.push_back(feat.GetTitle());
   if (feat.CanGetExt()) GetExtString(feat.GetExt(), arr);
   if (feat.CanGetCit()) GetCitString(feat.GetCit(), arr);
   if (feat.CanGetExp_ev()) GetExpevString(feat.GetExp_ev(), arr);
   if (feat.CanGetDbxref()) GetDbxrefString(feat.GetDbxref(), arr);
   if (feat.CanGetExcept_text()) arr.push_back(feat.GetExcept_text());
   if (feat.CanGetIds()) 
      ITERATE (list <CRef <CFeat_id> >, it, feat.GetIds()) GetFeatIdString((**it), arr);
   if (feat.CanGetExts()) 
      ITERATE (list <CRef <CUser_object> >, it, feat.GetExts()) GetExtString(**it, arr);
   if (feat.CanGetSupport()) GetSupportString(feat.GetSupport(), arr);
};
*/

bool CSuspectRuleCheck :: DoesObjectMatchConstraintChoiceSet(const CSeq_feat& feat, const CConstraint_choice_set& c_set)
{
  vector <string> strs_in_feat;
  GetStringsFromObject(feat, strs_in_feat);
  ITERATE (list <CRef <CConstraint_choice> >, sit, c_set.Get()) {
     if (!DoesObjectMatchConstraint(feat, strs_in_feat, **sit)) {
           strs_in_feat.clear();
           return false;
     }
  }
  strs_in_feat.clear();
  return true;
};

bool CSuspectRuleCheck :: DoesStringMatchSuspectRule(const CBioseq_Handle& bioseq_hl, const string& str, const CSeq_feat& feat, const CSuspect_rule& rule)
{
  m_bioseq_hl = bioseq_hl;
  if (MatchesSuspectProductRule(str, rule)) {
    const CSeq_feat* feat_pnt = const_cast <CSeq_feat*>(&feat);
    /* we want to list the coding region, rather than the protein feature, if we can */
    if (feat.GetData().IsProt()) {
      const CSeq_feat* cds = sequence::GetCDSForProduct(m_bioseq_hl);
      if (cds) {
          feat_pnt = const_cast <CSeq_feat*>(cds);
      }
    }

    if (!rule.CanGetFeat_constraint()) {
         return true;
    }
    else {
        if (!feat_pnt) {
            return false;
        }
        else {
          return  DoesObjectMatchConstraintChoiceSet (*feat_pnt, 
                                                    rule.GetFeat_constraint());
        }
    }
  }

  return false;
};

END_NCBI_SCOPE
