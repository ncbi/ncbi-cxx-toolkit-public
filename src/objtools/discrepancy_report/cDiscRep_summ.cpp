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
 *   summization for suspect product rules
 *
 * $Log:$
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>
/*
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbienv.hpp>
#include <corelib/ncbiargs.hpp>
#include <connect/ncbi_core_cxx.hpp>
*/

#include <objects/macro/CDSGenePr_pseudo_constrain.hpp>
#include <objects/macro/CDSGenePr_constraint_field.hpp>
#include <objects/macro/CDSGenePro_qual_constraint.hpp>
#include <objects/macro/Field_type.hpp>
#include <objects/macro/Field_constraint.hpp>
#include <objects/macro/Fix_type_.hpp>
#include <objects/macro/Location_pos_constraint.hpp>
#include <objects/macro/Match_type_constraint_.hpp>
#include <objects/macro/Misc_field_.hpp>
#include <objects/macro/Molinfo_field_constraint.hpp>
#include <objects/macro/Publication_constraint.hpp>
#include <objects/macro/Replace_rule.hpp>
#include <objects/macro/Seque_const_mol_type_const.hpp>
#include <objects/macro/Sequence_constraint.hpp>
#include <objects/macro/Simple_replace.hpp>
#include <objects/macro/Source_constraint.hpp>
#include <objects/macro/String_constraint_set.hpp>
#include <objects/macro/Translation_constraint.hpp>
#include <objects/macro/Word_substitution.hpp>
#include <objects/macro/Word_substitution_set.hpp>
#include <objects/seq/MolInfo.hpp>
#include <objmgr/util/sequence.hpp>

#include <objtools/discrepancy_report/hDiscRep_config.hpp>
#include <objtools/discrepancy_report/hDiscRep_tests.hpp>
#include <objtools/discrepancy_report/hUtilib.hpp>
#include <objtools/discrepancy_report/hDiscRep_summ.hpp>

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);
USING_SCOPE(DiscRepNmSpc);

static CSuspectRuleCheck rule_check;
static string strtmp;
static CDiscRepInfo thisInfo;

string CSummarizeSusProdRule :: SummarizeSearchFunc (const CSearch_func& func, bool short_version)
{
  string summ;
  switch (func.Which()) {
      case CSearch_func::e_String_constraint:
         summ = SummarizeStringConstraintEx (func.GetString_constraint(), short_version);
         break;
      case CSearch_func::e_Contains_plural:
         summ = "May contain plural";
         break;
      case CSearch_func::e_N_or_more_brackets_or_parentheses:
         summ
            = "Contains " + NStr::IntToString(func.GetN_or_more_brackets_or_parentheses())
              + " or more brackets or parentheses";
         break;
      case CSearch_func::e_Three_numbers:
         summ = "Three or more numbers together";
         break;
      case CSearch_func::e_Underscore:
         summ = "Contains underscore";
         break;
      case CSearch_func::e_Prefix_and_numbers:
         summ = "Is '" + func.GetPrefix_and_numbers() + "' followed by numbers";
         break;
      case CSearch_func::e_All_caps:
         summ = "Is all capital letters";
         break;
      case CSearch_func::e_Unbalanced_paren:
         summ = "Contains unbalanced brackets or parentheses";
         break;
      case CSearch_func::e_Too_long:
         summ = "Is longer than " + NStr::IntToString(func.GetToo_long()) + " characters";
         break;
      case CSearch_func::e_Has_term:
         strtmp = "'" + func.GetHas_term() + "'";
         if (short_version) summ = "Contains " + strtmp;
         else summ = "Contains " + strtmp 
                    + " at start or separated from other letters by numbers, spaces, or punctuation, but does not also contain 'domain'";
         break;
      default:
         summ = "Unknown search function";
         break;
  }
  return summ;
}

string CSummarizeSusProdRule :: SummarizePartialnessForLocationConstraint (const CLocation_constraint& loc_cons)
{
  EPartial_constraint partial5 = loc_cons.GetPartial5();
  EPartial_constraint partial3 = loc_cons.GetPartial3();
  if (partial5 == ePartial_constraint_either && partial3 == ePartial_constraint_either)
       return kEmptyStr;
  if (partial5 == ePartial_constraint_either) {
    if (partial3 == ePartial_constraint_partial) return "that are 3' partial";
    else return "that are 3' complete";
  }
  else if (partial3 == ePartial_constraint_either) {
    if (partial5 == ePartial_constraint_partial) return "that are 5' partial";
    else return "that are 5' complete";
  }
  else if (partial5 == ePartial_constraint_partial && partial3 == ePartial_constraint_partial)
              return "that are partial on both ends";
  else if (partial5 == ePartial_constraint_complete && partial3 == ePartial_constraint_complete)
              return "that are complete on both ends";
  else if (partial5 == ePartial_constraint_complete && partial3 == ePartial_constraint_partial)
              return "that are 5' complete and 3' partial";
  else if (partial5 == ePartial_constraint_partial && partial3 == ePartial_constraint_complete)
              return "that are 5' partial and 3' complete";
  else return kEmptyStr;
}

static const char* distance_words[] = { NULL, "exactly", "no more than", "no less than" };
string CSummarizeSusProdRule :: SummarizeEndDistance (const CLocation_pos_constraint& lp_cons, const string& end_name)
{
  if (lp_cons.Which() == CLocation_pos_constraint::e_not_set) return kEmptyStr;
  int dist;
  switch (lp_cons.Which()) {
     case CLocation_pos_constraint :: e_Dist_from_end:
              dist = lp_cons.GetDist_from_end(); break;
     case CLocation_pos_constraint :: e_Max_dist_from_end:
              dist = lp_cons.GetMax_dist_from_end(); break;
     case CLocation_pos_constraint :: e_Min_dist_from_end:
              dist = lp_cons.GetMin_dist_from_end(); break;
     default: break;
  }

  return("with " + end_name + distance_words[(unsigned)lp_cons.Which()] 
                                                           + NStr::IntToString(dist));
}

string CSummarizeSusProdRule :: SummarizeLocationConstraint (const CLocation_constraint& loc_cons)
{
  if (rule_check.IsLocationConstraintEmpty (loc_cons)) return kEmptyStr;

  string partial, location_type, dist5(kEmptyStr), dist3(kEmptyStr), seq_word(kEmptyStr);
  partial = SummarizePartialnessForLocationConstraint (loc_cons);
  location_type = thisInfo.loctype_names[loc_cons.GetLocation_type()];
  dist5 = loc_cons.CanGetEnd5() ? SummarizeEndDistance (loc_cons.GetEnd5(), "5' end") : dist5;
  dist3 = loc_cons.CanGetEnd3() ? SummarizeEndDistance (loc_cons.GetEnd3(), "3' end") : dist3;

  switch (loc_cons.GetSeq_type()) {
     case eSeqtype_constraint_nuc:
             seq_word = "nucleotide sequences"; break;
     case eSeqtype_constraint_prot:
             seq_word = "protein sequences"; break;
     default: break;
  }

  string strand_word(kEmptyStr);
  switch (loc_cons.GetStrand()) {
     case eStrand_constraint_plus:
           strand_word = " on plus strands"; break;
     case eStrand_constraint_minus: 
           strand_word = " on minus strands"; break;
     default: break;
  }

  string str("only objects");
  if (strand_word.empty() && !seq_word.empty()) str += " on " + seq_word;
  else if (!strand_word.empty()) {
    str += strand_word;
    if (!seq_word.empty()) str += " of " + seq_word;
  };
  if (!partial.empty()) str += " " + partial;
  if (!location_type.empty()) str += " " + location_type;
  if (!dist5.empty()) str += " " + dist5;
  if (!dist3.empty()) str += " " + dist3;
    
  return str;
};

string CSummarizeSusProdRule :: GetStringLocationPhrase (EString_location match_location, bool not_present)
{
  if (not_present) return (thisInfo.matloc_notpresent_names[match_location]);
  else return (thisInfo.matloc_names[match_location]);
}

string CSummarizeSusProdRule :: SummarizeWordSubstitution (const CWord_substitution& word)
{
  if (word.CanGetSynonyms() || word.GetSynonyms().empty()) return kEmptyStr;

  string syns = "'" + NStr::Join(word.GetSynonyms(), ", '") + "'";
  size_t pos = syns.find_last_of(',');
  syns = syns.substr(0, pos+1) + " and" + CTempString(syns).substr(pos+2);
  
  string summ 
        = "allow '" + (word.CanGetWord()? word.GetWord() : "") + "' to be replace by " + syns;
  if (word.GetCase_sensitive()) summ += ", case-sensitive";
  if (word.GetWhole_word()) summ += ", whole word";
  return summ;
}

string CSummarizeSusProdRule :: SummarizeStringConstraintEx(const CString_constraint& str_cons, bool short_version)
{
  if (rule_check.IsStringConstraintEmpty (&str_cons)) return kEmptyStr;

  string str;
  if (str_cons.CanGetMatch_text()) {
    string loc_word = 
             GetStringLocationPhrase (str_cons.GetMatch_location(), str_cons.GetNot_present());
    if (loc_word.empty()) return kEmptyStr;

    string sub_words;
    if (!short_version) {
      if (str_cons.CanGetIgnore_words()) {
         ITERATE (list <CRef <CWord_substitution> >, it, str_cons.GetIgnore_words().Get())
             sub_words += SummarizeWordSubstitution(**it) + ", ";    
         if (!sub_words.empty()) sub_words = sub_words.substr(0, sub_words.size()-2);
      }
    }

    bool has_extra = false, has_start_para = false;
    str = loc_word + " '" + str_cons.GetMatch_text() + "'";
    if (!short_version) {
       if (str_cons.GetCase_sensitive() || str_cons.GetWhole_word() 
                         || str_cons.GetIgnore_space() || str_cons.GetIgnore_punct()) {
          has_start_para = true;
          str += " (";
       }
     
       if (str_cons.GetCase_sensitive()) {
           str += "case-sensitive";
           has_extra = true;
       }
       if (str_cons.GetWhole_word()) {
          if (has_extra) str += ", ";
          str += "whole word";
          has_extra = true;
       }
       if (str_cons.GetIgnore_space()) {
           if (has_extra) str += ", ";
           str += "ignore spaces";
           has_extra = true;
       }
       if (str_cons.GetIgnore_punct()) {
           if (has_extra) str += ", ";
           str += "ignore punctuation";
           has_extra = true;
       }
       if (has_start_para) str += ")";

       // Colleen: I think the weasel comes after the parenthesized portion.
       if (str_cons.GetIgnore_weasel()) {
           if (has_extra) str += ", ";
           str += "ignore 'putative' synonyms";
           has_extra = true;
       }

       if (!sub_words.empty()) str += ", " + sub_words;
    }
  }
  strtmp = kEmptyStr;
  if (str_cons.GetIs_all_caps()) strtmp = ", all letters are uppercase";
  if (str_cons.GetIs_all_lower()) strtmp += ", all letters are lowercase";
  if (str_cons.GetIs_all_punct()) strtmp += ", all characters are punctuation";
  return (str + strtmp);
};


string CSummarizeSusProdRule :: SummarizeSourceQual(const CSource_qual_choice& src_qual)
{
  string summ;
  switch (src_qual.Which()) {
    case CSource_qual_choice::e_Textqual:
      summ = thisInfo.srcqual_names[src_qual.GetTextqual()];
      break;
    case CSource_qual_choice::e_Location:
      summ = "location " + thisInfo.srcloc_names[src_qual.GetLocation()]; 
      break;
    case CSource_qual_choice::e_Origin:
      summ = "origin " + thisInfo.srcori_names[src_qual.GetOrigin()];
      break;
    default: break;
  }
  return summ;
};

string CSummarizeSusProdRule :: SummarizeSourceConstraint (const CSource_constraint& src_cons)
{
  bool has_field1 = src_cons.CanGetField1();
  bool has_field2 = src_cons.CanGetField2();
  string field1, field2, strcons_str;
  strcons_str = field1 = field2 = kEmptyStr;
  if (src_cons.CanGetConstraint()) 
       strcons_str = SummarizeStringConstraintEx (src_cons.GetConstraint(), false);
  if (has_field1) field1 = SummarizeSourceQual (src_cons.GetField1());
  if (has_field2) field2 = SummarizeSourceQual (src_cons.GetField2());

  int type_constraint = -1;
  string summ, intro;
  if (src_cons.CanGetType_constraint()) type_constraint = (int)src_cons.GetType_constraint();
  if (!has_field1 && !has_field2 && strcons_str.empty()) {
    if (type_constraint == (int)eObject_type_constraint_feature) 
         summ = "where source is a feature";
    else if (type_constraint == (int)eObject_type_constraint_descriptor)
               summ = "where source is a descriptor";
  } 
  else { 
    if (type_constraint == (int)eObject_type_constraint_any) intro = "where source";
    else if (type_constraint == (int)eObject_type_constraint_feature) 
             intro = "where source feature";
    else if (type_constraint == (int)eObject_type_constraint_descriptor) 
              intro = "where source descriptor";
    else return kEmptyStr;

    if (strcons_str.empty()) {
      if (!has_field1 && !has_field2) {
        if (type_constraint == (int)eObject_type_constraint_feature) 
             summ = "where source is a feature";
        else if (type_constraint == (int)eObject_type_constraint_descriptor)
                 summ = "where source is a descriptor";
      } 
      else if (has_field1 && has_field2) 
               summ = intro + " " + field1 + " matches " + field2;
      else if (has_field1) summ = intro + " " + field1 + " is present";
      else if (has_field2) summ = intro + " " + field1 + " is present";
    } 
    else {
      if (!has_field1 && !has_field2) summ = intro + " text " + strcons_str;
      else if (has_field1 && has_field2) 
         summ =intro +" " +field1 +" matches " + field2 + " and " + field1 + " " + strcons_str;
      else if (has_field1) summ = intro + " " + field1 + " " + strcons_str;
      else if (has_field2) summ = intro + " " + field2 + " " + strcons_str;
    }
  }
  return summ;
};

string CSummarizeSusProdRule :: SummarizeCDSGeneProtQualConstraint (const CCDSGeneProt_qual_constraint& cgp_cons)
{
  string strcons_str, field1, field2, summ(kEmptyStr);
  strcons_str = field1 = field2 = kEmptyStr;
  if (cgp_cons.CanGetConstraint())
        strcons_str = SummarizeStringConstraintEx (cgp_cons.GetConstraint(), false);
  bool has_field1 = cgp_cons.CanGetField1();
  bool has_field2 = cgp_cons.CanGetField2();

  //CDSGeneProtNameFromField (cgp_cons.GetField1().GetField());
  if (has_field1 && cgp_cons.GetField1().IsField())
        field1 = thisInfo.cgp_field_name[cgp_cons.GetField1().GetField()];
  if (has_field2 && cgp_cons.GetField2().IsField())
        field2 = thisInfo.cgp_field_name[cgp_cons.GetField2().GetField()];

  if (strcons_str.empty()) {
    if (!field1.empty() && !field2.empty()) summ = "where " + field1 + " matches " + field2;
    else if (!field1.empty()) summ = "where " + field1 + " is present";
    else if (!field2.empty()) summ = "where " + field2 + " is present";
  }
  else {
    if (field1.empty() && field2.empty()) summ = "where CDS-gene-prot text " + strcons_str;
    else if (!field1.empty() && !field2.empty())
              summ = "where " + field1 + " matches " + field2 + " and " + field1 + " " + strcons_str;
    else if (!field1.empty()) summ = "where " + field1 + " " + strcons_str;
    else if (!field2.empty()) summ = "where " + field2 + " " + strcons_str;
  }

  return summ;
}

string CSummarizeSusProdRule :: SummarizeCDSGeneProtPseudoConstraint (const CCDSGeneProt_pseudo_constraint& cgp_pcons)
{
  string pseudo_feat = thisInfo.cgp_feat_name[cgp_pcons.GetFeature()];
  if (!pseudo_feat.empty()) {
      if (cgp_pcons.GetIs_pseudo()) return("where " + pseudo_feat + " is pseudo");
      else return ("where " + pseudo_feat + " is not pseudo");
  }
  return kEmptyStr;
}

const char* s_QuantityWords [] = { "exactly ", "more than ", "less than " };
bool CSummarizeSusProdRule :: HasQuantity(const CQuantity_constraint& v, CQuantity_constraint::E_Choice& e_val, int& num)
{
  e_val = v.Which();
  if (e_val == CQuantity_constraint::e_not_set) return false;
  num = (e_val == CQuantity_constraint::e_Equals) ?
          v.GetEquals() : ((e_val == CQuantity_constraint::e_Greater_than) ?
                             v.GetGreater_than() : v.GetLess_than() );
  return true;
};

string CSummarizeSusProdRule :: SummarizeFeatureQuantity (const CQuantity_constraint& v, const string* feature_name)
{
  int num;
  CQuantity_constraint::E_Choice e_val;
  if (!HasQuantity(v, e_val, num)) return kEmptyStr;
  string summ = (string)"sequence has " + s_QuantityWords[(int)e_val - 1] 
                  + NStr::IntToString(num);
  if (feature_name && !(*feature_name).empty()) summ += " " + *feature_name;
  summ += (string)" feature" + (num > 1? "s": "");
  return summ;
}

string CSummarizeSusProdRule :: SummarizeSequenceLength (const CQuantity_constraint& v)
{
  CQuantity_constraint::E_Choice e_val;
  int num;
  if (!HasQuantity(v, e_val, num)) return kEmptyStr;
  
  return ((string)"sequence is " + s_QuantityWords[(int)e_val - 1] 
                                                + NStr::IntToString(num) + " in length");
}
string CSummarizeSusProdRule :: SummarizeSequenceConstraint (const CSequence_constraint& seq_cons)
{
  string summ, seq_word;
  bool has_seqtype = seq_cons.CanGetSeqtype();
  if (rule_check.IsSequenceConstraintEmpty (seq_cons)) 
            summ = "Missing sequence constraint";
  else {
    string seq_word, featpresent, feat_type_quantity, id, feat_quantity;
    string length_quantity, strandedness;
    if (has_seqtype && !seq_cons.GetSeqtype().IsAny()) {
      switch (seq_cons.GetSeqtype().Which()) {
        case CSequence_constraint_mol_type_constraint::e_Nucleotide:
              seq_word = "nucleotide";
              break;
        case CSequence_constraint_mol_type_constraint::e_Dna:
              seq_word = "DNA";
              break;
        case CSequence_constraint_mol_type_constraint::e_Rna:
              if (seq_cons.GetSeqtype().GetRna() == eSequence_constraint_rnamol_any) 
                   seq_word = "RNA";
              else seq_word = thisInfo.scrna_mirna[seq_cons.GetSeqtype().GetRna()];
              break;
        case CSequence_constraint_mol_type_constraint::e_Protein:
              seq_word = "protein";
              break;
        default: break;
      }
    }

    if (seq_cons.GetFeature() != eMacro_feature_type_any) {
      featpresent = thisInfo.feattype_name[seq_cons.GetFeature()]; //GetFeatureNameFromFeatureType
      if (seq_cons.CanGetNum_type_features()) 
        feat_type_quantity =
                  SummarizeFeatureQuantity(seq_cons.GetNum_type_features(), &featpresent);
    }

    if (seq_cons.CanGetId() && !rule_check.IsStringConstraintEmpty (&seq_cons.GetId()))
           id = SummarizeStringConstraintEx (seq_cons.GetId(), false);
 
    if (seq_cons.CanGetNum_features())
          feat_quantity = SummarizeFeatureQuantity (seq_cons.GetNum_features());

    if (seq_cons.CanGetLength()) 
          length_quantity = SummarizeSequenceLength (seq_cons.GetLength());

    if (seq_cons.GetStrandedness() > eFeature_strandedness_constraint_any)
         strandedness = thisInfo.feat_strandedness[seq_cons.GetStrandedness()];

    if (seq_word.empty() && featpresent.empty() && feat_type_quantity.empty() && id.empty()
                 && feat_quantity.empty() && length_quantity.empty() && strandedness.empty() )
         summ = "missing sequence constraint";
    else {
      if (!seq_word.empty()) summ = "where sequence type is " + seq_word;
      string summ2 = "where ";
      bool first = seq_word.empty();
      if (!featpresent.empty()) {
        if (!first) summ2 +=  " and ";
        summ2 += featpresent + " is present";
        first = false;
      }
      if (!feat_type_quantity.empty()) {
        if (!first) summ2 += " and ";
        else first = true;
        summ2 += feat_type_quantity;
      }
      if (!id.empty()) {
        if (first) summ2 = "where ";
        else summ2 += " and ";
        summ2 += "sequence ID " + id;
        first = false;
      }
      if (!feat_quantity.empty()) {
        if (!first) summ2 += " and ";
        summ2 += feat_quantity;
        first = false;
      }
      if (!length_quantity.empty()) {
        if (!first) summ2 += " and ";
        summ2 += length_quantity;
        first = false;
      }
      if (!strandedness.empty()) {
        if (!first) summ2 += " and ";
        summ2 += strandedness;
      }        
      if (!summ.empty()) summ += summ2; 
      else summ = summ2;
    }
  }
  return summ;
}

string CSummarizeSusProdRule :: SummarizePubFieldConstraint (const CPub_field_constraint& field)
{
  strtmp = SummarizeStringConstraintEx (field.GetConstraint(), false);
  string label = thisInfo.pubfield_label[field.GetField()];
  return (label + " " + strtmp);
};

string CSummarizeSusProdRule :: SummarizePubFieldSpecialConstraint(const CPub_field_special_constraint& field)
{
   string label = thisInfo.pubfield_label[field.GetField()];
   if (field.GetConstraint().Which() != CPub_field_special_constraint_type::e_not_set)
              strtmp = thisInfo.spe_pubfield_label[field.GetConstraint().Which()]; 
   else strtmp = kEmptyStr;
   return (label + " " + strtmp);
}

string CSummarizeSusProdRule :: SummarizePublicationConstraint (const CPublication_constraint& pub_cons)
{
  if (rule_check.IsPublicationConstraintEmpty (pub_cons)) return kEmptyStr;

  EPub_type type = pub_cons.GetType();
  string type_str = (type == ePub_type_any)?  kEmptyStr 
                       : "pub is " + ENUM_METHOD_NAME(EPub_type)()->FindName(type, true);


  string field = pub_cons.CanGetField() ? SummarizePubFieldConstraint(pub_cons.GetField())
                   : kEmptyStr;
  string special = pub_cons.CanGetSpecial_field()?
                 SummarizePubFieldSpecialConstraint(pub_cons.GetSpecial_field()) : kEmptyStr;

  if (type_str.empty() && field.empty() && special.empty()) return kEmptyStr;
  string summ("where ");
  bool first = true;
  if (!type_str.empty()) {
    summ += type;
    first = false;
  }
  if (!field.empty()) {
    if (!first) summ += " and ";
    summ += field;
    first = false;
  }
  if (!special.empty()) {
    if (!first) summ += " and ";
    summ += special;
    first = false;
  }

  return summ;
};

string CSummarizeSusProdRule :: FeatureFieldLabel (const string& feature_name, const CFeat_qual_choice& field)
{
  string label;
  if (field.Which() == CFeat_qual_choice::e_not_set) return ("missing field");
  else if (field.IsLegal_qual()) 
       label = feature_name + " " + thisInfo.featqual_leg_name[field.GetLegal_qual()];
  else if (field.IsIllegal_qual()) label = "constrained field on " + feature_name;
  else label = "illegal field value";
  return label;
};

string CSummarizeSusProdRule :: GetSequenceQualName (const CMolinfo_field& field)
{
  string fieldname("invalid field"), val("invalid value");
  if (field.Which() == CMolinfo_field::e_not_set) return kEmptyStr;
  switch (field.Which()) {
    case CMolinfo_field::e_Molecule:
      fieldname = "molecule";
      val = thisInfo.moltp_name[field.GetMolecule()];
      break;
    case CMolinfo_field::e_Technique:
      fieldname = "technique";
      val = thisInfo.techtp_name[field.GetTechnique()];
      break;
    case CMolinfo_field::e_Completedness:
      fieldname = "completeness";
      val = thisInfo.compl_names[field.GetCompletedness()];
      break;
    case CMolinfo_field::e_Mol_class:
      fieldname = "class";
      val = thisInfo.molclass_names[field.GetMol_class()];
      break;
    case CMolinfo_field::e_Topology:
      fieldname = "topology";
      val = thisInfo.topo_names[field.GetTopology()];
      break;
    case CMolinfo_field::e_Strand:
      fieldname = "strand";
      val = thisInfo.strand_names[field.GetStrand()];
      break;
    default: break;
  }
  if (val.empty() || val == "unknown") val = "invalid value";
  return (fieldname + " " + val);
}

string CSummarizeSusProdRule :: SummarizeRnaQual (const CRna_qual& rq)
{
  string qualname = thisInfo.rnafield_names[rq.GetField()];
  if (qualname.empty()) return kEmptyStr;

  string rnatypename = thisInfo.rnatype_names[(int)rq.GetType().Which()];

  string s;
  if (rnatypename.empty()) s = "RNA " + qualname;
  else s = rnatypename + " " + qualname;
  return s; 
};

string CSummarizeSusProdRule :: SummarizeStructuredCommentField (const CStructured_comment_field& field)
{
  string summ;
  if (field.IsDatabase()) summ = "structured comment database";
  else if (field.IsNamed()) {
    strtmp = field.GetNamed();
    summ = "structured comment field " +  (strtmp.empty()? "" : strtmp);
  }

  return summ;
}

string CSummarizeSusProdRule :: SummarizeFieldType (const CField_type& vnp)
{
  string label, str;
  if (vnp.Which() == CField_type::e_not_set) return ("missing field");
  else {
    switch (vnp.Which()) {
      case CField_type::e_Source_qual:
        str = SummarizeSourceQual (vnp.GetSource_qual());
        break;
      case CField_type::e_Feature_field:
        {
          const CFeature_field& ff = vnp.GetFeature_field();
        if (ff.GetField().Which() == CFeat_qual_choice::e_not_set) str = "missing field";
        else {
            label = thisInfo.feattype_name[ff.GetType()];
            if (label.empty()) str = "Unknown feature";
            else str = FeatureFieldLabel (label, ff.GetField());
        }
        break;
        }
      case CField_type::e_Cds_gene_prot:
	str = thisInfo.cgp_field_name[vnp.GetCds_gene_prot()];
        if (str.empty()) str = "Invalid CDS-Gene-Prot Field";
        break;
      case CField_type::e_Molinfo_field:
        str = GetSequenceQualName (vnp.GetMolinfo_field());
        if (str.empty()) str = "Invalid Sequence Qual Field";
        break;
      case CField_type::e_Pub:
        str = thisInfo.pubfield_label[vnp.GetPub()];
        if (str.empty()) str = "Invalid field type";
        break;
      case CField_type::e_Rna_field:
        str = SummarizeRnaQual (vnp.GetRna_field());
        break;
      case CField_type::e_Struc_comment_field:
        str = SummarizeStructuredCommentField (vnp.GetStruc_comment_field());
        break;
      case CField_type::e_Dblink:
        str = thisInfo.dblink_names[(int)vnp.GetDblink()];
        break;
      case CField_type::e_Misc:
        str = thisInfo.miscfield_names[(int)vnp.GetMisc()];
        break;
      default:
        str = "Invalid field type";
        break;
    }
  }
  return str;
}

string  CSummarizeSusProdRule :: SummarizeMissingFieldConstraint (const CField_type& field_tp)
{
  string label = SummarizeFieldType (field_tp);

  if (!label.empty()) return ("where " + label + " is missing");
  else return kEmptyStr;
}

string CSummarizeSusProdRule :: SummarizeFieldConstraint (const CField_constraint& field_cons)
{
  if (rule_check.IsStringConstraintEmpty (&(field_cons.GetString_constraint()))) 
             return kEmptyStr;

  string summ = SummarizeStringConstraintEx (field_cons.GetString_constraint(), false);
  string label = SummarizeFieldType (field_cons.GetField());

  if (!summ.empty() && !label.empty()) return ("where " + label + " " + summ);
  else return kEmptyStr;
}

string CSummarizeSusProdRule :: SummarizeMolinfoFieldConstraint (const CMolinfo_field_constraint& mol_cons)
{
  string label = GetSequenceQualName (mol_cons.GetField());
  if (label.empty()) return kEmptyStr;
  size_t cp = label.find(' ');
  if (cp == string::npos) return kEmptyStr;

  string rval = "where " + label.substr(0, cp) + (mol_cons.GetIs_not()? "is not " : "is ")
          + CTempString(label).substr(cp+1);

  return rval;
};

void CSummarizeSusProdRule :: GetStringConstraintPhrase(const list <CRef <CString_constraint> >& str_cons_set, string& phrases)
{
  string strcons_str;
  phrases.clear();
  ITERATE (list <CRef <CString_constraint> >, it, str_cons_set) {
     strcons_str.clear();
     strcons_str = SummarizeStringConstraintEx(**it, false);
     if (!strcons_str.empty()) phrases += strcons_str + ", ";
  }
  if (phrases.empty()) phrases = phrases.substr(0, phrases.size()-2);
  return;
}

string CSummarizeSusProdRule :: SummarizeTranslationMismatches (const CQuantity_constraint& v)
{
  CQuantity_constraint::E_Choice e_val;
  int num;
  if (!HasQuantity(v, e_val, num)) return kEmptyStr;
  string summ = (string)"there are " + s_QuantityWords[(int)(e_val)-1] + " " 
                  + NStr::IntToString(num)
                  + " mismatches between the actual and translated protein sequences";
  return summ;
}

string CSummarizeSusProdRule :: SummarizeTranslationConstraint (const CTranslation_constraint& tras_cons)
{
  // IsTranslationConstraintEmpty(tras_cons)) return kEmptyStr;
  bool all_strcons_empty = true;
  ITERATE (list <CRef <CString_constraint> >, ait, tras_cons.GetActual_strings().Get()) {
     if (!rule_check.IsStringConstraintEmpty((*ait).GetPointer())) {
             all_strcons_empty = false; break;
     }
  }
  if (all_strcons_empty) {
    ITERATE (list <CRef <CString_constraint> >, tit, tras_cons.GetTransl_strings().Get()) {
       if (!rule_check.IsStringConstraintEmpty((*tit).GetPointer())) {
             all_strcons_empty = false; break;
       }  
    }
  }
  if (all_strcons_empty && !tras_cons.CanGetNum_mismatches()
         && tras_cons.GetInternal_stops() == eMatch_type_constraint_dont_care)
    return kEmptyStr;

  string rval, actual_phrases, transl_phrases;
  GetStringConstraintPhrase(tras_cons.GetActual_strings(), actual_phrases); 
  GetStringConstraintPhrase(tras_cons.GetTransl_strings(), transl_phrases); 

  string mismatch = tras_cons.CanGetNum_mismatches() ? 
                    SummarizeTranslationMismatches(tras_cons.GetNum_mismatches()) : kEmptyStr;
  EMatch_type_constraint internal_stops =  tras_cons.GetInternal_stops();
    
  if (!actual_phrases.empty()) rval = "where actual sequence " + actual_phrases;

  if (!transl_phrases.empty()) {
    if (!rval.empty()) {
      if (!mismatch.empty() || internal_stops != eMatch_type_constraint_dont_care) 
           rval += ", ";
      else rval += " and ";
    }
    rval += "where translated sequence " + transl_phrases;
  }

  if (!mismatch.empty()) {
    if (!rval.empty()) {
      if (internal_stops != eMatch_type_constraint_dont_care) rval += ", ";
      else rval +=  " and ";
    }
    rval += mismatch;
  }

  if (internal_stops == eMatch_type_constraint_yes) rval += " and sequence has internal stops";
  else if (internal_stops == eMatch_type_constraint_no) 
    rval += " and sequence has no internal stops";

  return rval;
};

string CSummarizeSusProdRule :: SummarizeConstraint(const CConstraint_choice& cons_choice)
{
  string phrase;
  switch (cons_choice.Which()) {
    case CConstraint_choice::e_String:
      phrase = SummarizeStringConstraintEx (cons_choice.GetString(), false);
      if (!phrase.empty()) phrase = "where object text " + phrase;
      break;
    case CConstraint_choice::e_Location:
      phrase = SummarizeLocationConstraint (cons_choice.GetLocation());
      break;
    case CConstraint_choice::e_Source:
      phrase = SummarizeSourceConstraint (cons_choice.GetSource());
      break;
    case CConstraint_choice::e_Cdsgeneprot_qual:
      phrase = SummarizeCDSGeneProtQualConstraint (cons_choice.GetCdsgeneprot_qual());
      break;
    case CConstraint_choice::e_Cdsgeneprot_pseudo:
      phrase = SummarizeCDSGeneProtPseudoConstraint (cons_choice.GetCdsgeneprot_pseudo());
      break;
    case CConstraint_choice::e_Sequence:
      phrase = SummarizeSequenceConstraint (cons_choice.GetSequence());
      break;
    case CConstraint_choice::e_Pub:
      phrase = SummarizePublicationConstraint (cons_choice.GetPub());
      break;
    case CConstraint_choice::e_Field:
      phrase = SummarizeFieldConstraint (cons_choice.GetField());
      break;
    case CConstraint_choice::e_Molinfo:
      phrase = SummarizeMolinfoFieldConstraint (cons_choice.GetMolinfo());
      break;
    case CConstraint_choice::e_Field_missing:
      phrase = SummarizeMissingFieldConstraint (cons_choice.GetField_missing());
      break;
    case CConstraint_choice::e_Translation:
      phrase = SummarizeTranslationConstraint (cons_choice.GetTranslation());
      break;
    default: break;
  }
  return phrase;
};

string CSummarizeSusProdRule :: SummarizeConstraintSet (const CConstraint_choice_set& cons_set)
{
  string summ;
  ITERATE (list <CRef <CConstraint_choice> >, it, cons_set.Get()) {
     strtmp = SummarizeConstraint(**it);
     if (!strtmp.empty()) summ += strtmp + " and ";
  }
  summ = summ.substr(0, summ.size() - 5);
  return summ;
}

string CSummarizeSusProdRule :: SummarizeReplaceFunc (const CReplace_func& replace, bool short_version)
{
  string summ;
  switch (replace.Which()) {
    case CReplace_func::e_Simple_replace:
      {
        const CSimple_replace& simple = replace.GetSimple_replace();
        summ = (string)"Replace " + (simple.GetWhole_string()? "entire name with " : "with ")
                + (simple.CanGetReplace()? "''" : "'" + simple.GetReplace() + "'");
        if (simple.GetWeasel_to_putative() && !short_version)
             summ += ", retain and normalize 'putative' synonym";
        break;
      }
    case CReplace_func::e_Haem_replace:
      summ = "Replace '" + replace.GetHaem_replace() 
                                      + "' with 'heme' if whole word, 'hem' otherwise";
      break;
    default:
      summ = "Unknown replacement function";
      break;
  }
  return summ;
}

string CSummarizeSusProdRule :: SummarizeReplaceRule (const CReplace_rule& replace)
{
  string summ = SummarizeReplaceFunc (replace.GetReplace_func());
  if (replace.GetMove_to_note()) summ += ", move original to note";
  return summ;
}

extern const char* fix_type_names[];
string CSummarizeSusProdRule :: SummarizeSuspectRuleEx(const CSuspect_rule& rule, bool short_version)
{
  string fixtp = (!short_version && rule.GetRule_type() != eFix_type_none) ?
                   fix_type_names[rule.GetRule_type()] : kEmptyStr;

  string rule_desc = rule.CanGetDescription() ? rule.GetDescription() : kEmptyStr;
  if (short_version && !rule_desc.empty()) {
      if (fixtp.empty()) return rule_desc;
      else return (rule_desc + " (" + fixtp + ")");
  }

  string find  = SummarizeSearchFunc (rule.GetFind(), short_version);
  string except = (rule.CanGetExcept() && !rule_check.IsSearchFuncEmpty(rule.GetExcept()))?
                    SummarizeSearchFunc(rule.GetExcept(), short_version) : kEmptyStr;
  string feat_constraint = (!short_version && rule.CanGetFeat_constraint()) ?  
                            SummarizeConstraintSet (rule.GetFeat_constraint()) : kEmptyStr;
  string replace 
          = (!short_version && rule.GetRule_type() == eFix_type_typo && rule.CanGetReplace())?
             SummarizeReplaceRule (rule.GetReplace()) : kEmptyStr;

  string summ = find;
  if (!except.empty()) summ += " but not " + except;
  if (!feat_constraint.empty()) summ += ", " + feat_constraint;
  if (!replace.empty()) summ += ", " + replace;
  if (!fixtp.empty()) summ += " (" + fixtp + ")";
  if (!rule_desc.empty()) summ += " Description: " + rule_desc;
  return summ;
};

END_NCBI_SCOPE
