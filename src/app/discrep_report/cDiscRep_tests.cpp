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
#include <serial/enumvalues.hpp>
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
#include <objects/seqfeat/Feat_id.hpp>
#include <objects/seqfeat/SeqFeatXref.hpp>
#include <objects/seqfeat/PCRReaction.hpp>
#include <objects/seqfeat/PCRPrimer.hpp>
#include <objects/seqfeat/PCRReactionSet.hpp>
#include <objects/seqfeat/PCRPrimerSet.hpp>
#include <objects/seqfeat/RNA_gen.hpp>
#include <objects/seqfeat/RNA_ref.hpp>
#include <objects/seqblock/GB_block.hpp>
#include <objects/macro/Feat_qual_legal_.hpp>
#include <objects/macro/Feature_field.hpp>
#include <objects/macro/Search_func.hpp>
#include <objects/macro/Constraint_choice_set.hpp>
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

#include <algo/blast/core/ncbi_std.h>

#include "hDiscRep_app.hpp"
#include "hDiscRep_config.hpp"
#include "hDiscRep_tests.hpp"
#include "hchecking_class.hpp"

#include "hUtilib.hpp"

USING_NCBI_SCOPE;
using namespace objects;
using namespace sequence;
using namespace DiscRepNmSpc;
using namespace feature;

vector <bool>   CRuleProperties :: srch_func_empty;
vector <bool>   CRuleProperties :: except_empty;

static CDiscRepInfo thisInfo;
static CRef <CRuleProperties> rule_prop;
static string strtmp(kEmptyStr);
static CDiscTestInfo thisTest;

// ini. CDiscTestInfo
bool CDiscTestInfo :: is_ANNOT_run = false;
bool CDiscTestInfo :: is_BASES_N_run = false;
bool CDiscTestInfo :: is_BIOSRC_run = false;
bool CDiscTestInfo :: is_DESC_user_run = false;
bool CDiscTestInfo :: is_FEAT_DESC_biosrc_run = false;
bool CDiscTestInfo :: is_MOLINFO_run = false;
bool CDiscTestInfo :: is_MRNA_run = false;
bool CDiscTestInfo :: is_SHORT_run = false;

typedef map <string, CRef < GeneralDiscSubDt > > Str2SubDt;
typedef map <int, vector <string> > Int2Strs;

static Str2SubDt GENE_PRODUCT_CONFLICT_cds;
static set <string> DUP_LOCUS_TAGS_adj_genes;
static vector <CRef < GeneralDiscSubDt > > INCONSISTENT_BIOSOURCE_biosrc;
static vector < CRef < CClickableItem > > DUPLICATE_GENE_LOCUS_locuss;
static vector < CRef < GeneralDiscSubDt > > SUSPECT_PROD_NAMES_feat;
static unsigned DISC_QUALITY_SCORES_graph=0;
static Int2Strs PROJECT_ID_prot;
static Int2Strs PROJECT_ID_nuc;
static vector <CRef < GeneralDiscSubDt > > DISC_FEATURE_COUNT_list;
static vector <string> DISC_FEATURE_COUNT_protfeat_prot_list;
static vector <string> DISC_FEATURE_COUNT_protfeat_nul_list;
static Str2Strs JOINED_FEATURES_sfs;

typedef map <string, int> Str2Int;


// CRuleProperties
bool CRuleProperties :: IsSearchFuncEmpty (const CSearch_func& func)
{
  if (func.IsString_constraint()) {
     const CString_constraint& str_cons = func.GetString_constraint();
     if (str_cons.GetIs_all_lower() || str_cons.GetIs_all_lower() || str_cons.GetIs_all_punct())
           return false;
     else if (!(str_cons.CanGetMatch_text()) || str_cons.GetMatch_text().empty()) return true;
     else return false;
  }
  else if (func.IsPrefix_and_numbers()) {
      if (func.GetPrefix_and_numbers().empty()) return true;
      else return false;
  }
  else return false;
};



// CBioseq
bool CBioseqTestAndRepData :: IsUnknown(const string& known_items, const unsigned idx)
{
   if (known_items.find("|" + NStr::UIntToString(idx) + "|") == string::npos) return true;
   else return false;
};


// equal = IsPesudo(sfp) in C
bool CBioseqTestAndRepData :: IsPseudoSeqFeatOrXrefGene(const CSeq_feat* seq_feat)
{
   bool rvl;
   if (seq_feat->CanGetPseudo()) rvl = seq_feat->GetPseudo();
   else {
     const CSeqFeatData& seq_feat_dt = seq_feat->GetData();
     if (seq_feat_dt.IsGene()) rvl = seq_feat_dt.GetGene().GetPseudo();
     else {
        CConstRef <CSeq_feat> gene_olp= GetBestOverlappingFeat(seq_feat->GetLocation(),
                                                             CSeqFeatData::e_Gene,
                                                             eOverlap_Contained,
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
             if (rna_str.empty()|| rna_str== "ncRNA"|| rna_str== "tmRNA"||rna_str== "misc_RNA")
                 rna_str = seq_feat.GetNamedQual("product");
             break;
       case CRNA_ref::C_Ext::e_TRNA:
              GetSeqFeatLabel(seq_feat, &rna_str);
              break;
       case CRNA_ref::C_Ext::e_Gen:
              if (ext.GetGen().CanGetProduct()) rna_str = ext.GetGen().GetProduct();
       default: break;
     }
  }
  return rna_str;
};


bool CBioseqTestAndRepData :: DoesStringContainPhrase(const string& str, const string& phrase, bool case_sensitive, bool whole_word)
{
  size_t pos;
  if (case_sensitive) {
    if ( (pos = str.find(phrase)) == string::npos) return false;
  }
  else if ( (pos = NStr::FindNoCase(str, phrase)) == string::npos) return false;

  if (whole_word) {
      while (pos != string::npos) {
        strtmp = str.substr(pos);
        if ( (!pos || !isalnum(str[pos-1])) 
                 && (strtmp.size() == phrase.size() || !isalnum(strtmp[phrase.size()])))
             return true;
        else {
          if (case_sensitive) pos = str.substr(pos + 1).find(phrase);
          else pos = NStr::FindNoCase(str.substr(pos + 1), phrase);
        }
      }
  } 
  else return true;
};



bool CBioseq_DISC_GENE_PARTIAL_CONFLICT :: Is5EndInUTRList(const unsigned& start)
{
   ITERATE (vector <const CSeq_feat*>, it, utr5_feat) {
     if (start == (*it)->GetLocation().GetStart(eExtreme_Positional)) return true;
   }
   return false;
};


bool CBioseq_DISC_GENE_PARTIAL_CONFLICT :: Is3EndInUTRList(const unsigned& stop)
{
  ITERATE (vector <const CSeq_feat*>, it, utr3_feat) {
     if (stop == (*it)->GetLocation().GetStop(eExtreme_Positional)) return true;
  }
  return false;
};


void CBioseq_DISC_GENE_PARTIAL_CONFLICT :: ReportPartialConflictsForFeatureType(vector <const CSeq_feat*>& seq_feats, string label, bool check_for_utrs)
{
  CRef <CSeq_loc> gene_loc, feat_loc;
  ENa_strand feat_strand, gene_strand;
  unsigned feat_start, feat_stop, gene_start, gene_stop;
  bool conflict5, conflict3, feat_partial5, feat_partial3, gene_partial5, gene_partial3;
  ITERATE (vector <const CSeq_feat*>, it, seq_feats) {
    CConstRef <CSeq_feat> gene_feat (GetGeneForFeature(**it));
    if (gene_feat.NotEmpty()) {
      feat_loc = Seq_loc_Merge((*it)->GetLocation(), 
                     CSeq_loc::fMerge_Overlapping | CSeq_loc::fSort, thisInfo.scope);
      gene_loc = Seq_loc_Merge(gene_feat->GetLocation(),
                      CSeq_loc::fMerge_Overlapping | CSeq_loc::fSort, thisInfo.scope); 
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
      if (feat_loc->IsInt()) {
         feat_partial5 = feat_loc->GetInt().IsPartialStart(eExtreme_Biological);
         feat_partial3 = feat_loc->GetInt().IsPartialStop(eExtreme_Biological);
      }
      if (gene_loc->IsInt()) {
         gene_partial5 = gene_loc->GetInt().IsPartialStart(eExtreme_Biological);
         gene_partial3 = gene_loc->GetInt().IsPartialStop(eExtreme_Biological);
      }

      conflict5 = false;
      if ( (feat_partial5 && !gene_partial5) || (!feat_partial5 && gene_partial5) ) {
        if (feat_start == gene_start) conflict5 = true;
        else if (check_for_utrs && !Is5EndInUTRList(gene_start)) conflict5 = true;
      }

      conflict3 = false;
      if ( (feat_partial3 && !gene_partial3) || (!feat_partial3 && gene_partial3) ) {
        if (feat_stop == gene_stop) conflict3 = true;
        else if (check_for_utrs && !Is3EndInUTRList(gene_stop)) conflict3 = true;
      }

      strtmp = label;
      if (conflict5 || conflict3) {
        if (conflict5 && conflict3) 
             strtmp += " feature partialness conflicts with gene on both ends.";
        else if (conflict5) 
              strtmp += " feature partialness conflicts with gene on 5' end.";
        else strtmp += " feature partialness conflicts with gene on 3' end.";

        label += "$" + strtmp;
        thisInfo.test_item_list[GetName()].push_back(label + "#" + GetDiscItemText(**it));
        thisInfo.test_item_list[GetName()].push_back(label + "#" +GetDiscItemText(*gene_feat));
      }
    }
  }
};


bool CBioseqTestAndRepData :: IsMrnaSequence()
{
  ITERATE (vector <const CSeqdesc*>, it, bioseq_molinfo) {
     if ( (*it)->GetMolinfo().GetBiomol() == CMolInfo :: eBiomol_mRNA) return true;
  }
  return false;
};


void CBioseq_DISC_GENE_PARTIAL_CONFLICT :: TestOnObj(const CBioseq& bioseq)
{
   if (bioseq.IsNa()) return;
   // ReportPartialConflictsForFeatureType (bsp, 0, FEATDEF_intron, "intron")
   ReportPartialConflictsForFeatureType (intron_feat, (string)"intron");
   ReportPartialConflictsForFeatureType (exon_feat, (string)"exon");
   ReportPartialConflictsForFeatureType (promoter_feat, (string)"promoter");
   ReportPartialConflictsForFeatureType (rna_feat, (string)"RNA");
   ReportPartialConflictsForFeatureType (utr3_feat, (string)"3' URT");
   ReportPartialConflictsForFeatureType (utr5_feat, (string)"5' URT");
   ReportPartialConflictsForFeatureType (utr5_feat, (string)"5' URT");
   if (!IsEukaryotic(bioseq) || IsMrnaSequence ()) 
       ReportPartialConflictsForFeatureType (cd_feat, (string)"coding region");
   ReportPartialConflictsForFeatureType (miscfeat_feat, (string)"misc_feature");
};



void CBioseq_DISC_GENE_PARTIAL_CONFLICT :: GetReport(CRef <CClickableItem>& c_item)
{
  Str2Strs label2lists, sublabel2items;
  GetTestItemList(c_item->item_list, label2lists);
  c_item->item_list.clear();
  unsigned cnt;
  ITERATE (Str2Strs, it, label2lists) {
    AddSubcategories(c_item, GetName(), it->second, it->first + " location conflicts ", 
                                    it->first + " locations conflict ", false, e_OtherComment,
                                      "with partialness of overlapping gene.", true);
    
    GetTestItemList(it->second, sublabel2items, "#");
    cnt = c_item->subcategories.size();  
     c_item->subcategories[cnt-1]->item_list.clear();
    ITERATE (Str2Strs, jt, sublabel2items) {
      CRef <CClickableItem> c_sub2 (new CClickableItem);
      c_sub2->setting_name = GetName();
      c_sub2->item_list.insert(c_sub2->item_list.end(), jt->second.begin(), jt->second.end());
      c_sub2->description = jt->first;
      c_item->subcategories[cnt-1]->item_list.insert(
          c_item->subcategories[cnt-1]->item_list.end(), jt->second.begin(), jt->second.end());
      c_item->item_list.insert(c_item->item_list.end(), jt->second.begin(), jt->second.end());
    }
    sublabel2items.clear();
  }

  cnt = c_item->item_list.size();
  c_item->description 
       = GetOtherComment(cnt, "feature that is", "features that are") 
           + " not coding regions or misc_features " 
           + (cnt == 1? "conflicts" : "conflict") + " with partialness of overlapping gene.";
};


void CBioseq_SHORT_PROT_SEQUENCES :: TestOnObj(const CBioseq& bioseq)
{
  if (!bioseq.IsAa() || bioseq.GetLength() >= 50) return;
  CConstRef <CBioseq_set> bioseq_set = bioseq.GetParentSet();
  if (bioseq_set.NotEmpty() && bioseq_set->GetClass() == CBioseq_set::eClass_parts) return;

  ITERATE (vector <const CSeqdesc*>, it, bioseq_molinfo) {
    if ( (*it)->GetMolinfo().GetCompleteness() != CMolInfo::eCompleteness_complete) continue;
    else thisInfo.test_item_list[GetName()].push_back(GetDiscItemText(bioseq));
  }
};


void CBioseq_SHORT_PROT_SEQUENCES :: GetReport(CRef <CClickableItem>& c_item)
{
   c_item->description 
            = GetIsComment(c_item->item_list.size(), "protein sequence") + "shorter than 50 aa.";
};



bool CBioseq_RRNA_NAME_CONFLICTS :: NameNotStandard(const string& nm)
{
  unsigned i=0;
  bool is_equal = false, is_no_case_equal = false;
  ITERATE (vector <string>, it, thisInfo.rrna_standard_name) {
    if ( (*it) == nm ) { is_equal = true; break;}
    else if ( NStr::EqualNocase((*it), nm) ) {
        is_no_case_equal = true; break;
    }
    else i++;
  }

  vector <string> arr;
  string nm_new;
  if (is_equal) return false;
  else if (is_no_case_equal) {
    if (!DoesStringContainPhrase(nm, "RNA")) return true;
    arr = NStr::Tokenize(nm, " ", arr); 
    NON_CONST_ITERATE (vector <string>, it, arr) 
     if (*it != "RNA" && isalpha( (*it)[0] ) && isupper( (*it)[0] ))
       (*it)[0] = tolower( (*it)[0]);
    nm_new = NStr::Join(arr, " ");
    if (nm_new == thisInfo.rrna_standard_name[i]) return false;
    else return true; 
  }
  else return TRUE;
};




void CBioseq_RRNA_NAME_CONFLICTS :: TestOnObj(const CBioseq& bioseq)
{
  ITERATE (vector <const CSeq_feat*>, it, rrna_feat) {
     const CRNA_ref& rrna = (*it)->GetData().GetRna();
     if ( rrna.CanGetExt() && rrna.GetExt().IsName() 
                                               && NameNotStandard(rrna.GetExt().GetName())) {
        thisInfo.test_item_list[GetName()].push_back(GetDiscItemText(**it));
     }
  }
};


void CBioseq_RRNA_NAME_CONFLICTS :: GetReport(CRef <CClickableItem>& c_item)
{
   c_item->description 
        = GetIsComment(c_item->item_list.size(), "rRNA product name") + "not standard.";
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
                seq_feat_hl = thisInfo.scope->GetTSE_Handle(seq_entry).GetFeatureWithId(
                                                       CSeqFeatData::eSubtype_mRNA, local_id); 
            if (seq_feat_hl) return (seq_feat_hl.GetSeq_feat());
          }
        }
      }
    }
  }

  /* try by location if not by xref */
  CConstRef <CSeq_feat> mRNA = GetBestOverlappingFeat(cd_feat.GetLocation(),
                                  CSeqFeatData::eSubtype_mRNA,
                                  eOverlap_Subset,
                                  *thisInfo.scope);
  if (mRNA.Empty())
      mRNA = CConstRef <CSeq_feat> (GetBestOverlappingFeat(cd_feat.GetLocation(),
                                                           CSeqFeatData::eSubtype_mRNA,
                                                           eOverlap_Contained,
                                                           *thisInfo.scope));
  if (mRNA.Empty()) {
      CConstRef <CBioseq> 
          bioseq = 
             GetBioseqFromSeqLoc(cd_feat.GetLocation(), *thisInfo.scope).GetCompleteBioseq();
      if (bioseq.NotEmpty() && IsmRNASequenceInGenProdSet(*bioseq) )
          mRNA =CConstRef <CSeq_feat> (sequence :: GetmRNAForProduct(*bioseq, thisInfo.scope));
  }

  return mRNA;
}



CConstRef <CProt_ref> CBioseqTestAndRepData :: GetProtRefForFeature(const CSeq_feat& seq_feat)
{
  CConstRef <CProt_ref> prot_ref;
  if (seq_feat.GetData().IsProt()) 
       prot_ref = CConstRef <CProt_ref> (&(seq_feat.GetData().GetProt()));
  else if (seq_feat.GetData().IsCdregion()) {
    if (seq_feat.CanGetXref()) {
      ITERATE (vector <CRef <CSeqFeatXref> >, it, seq_feat.GetXref()) {
        if ( (*it)->CanGetData() && (*it)->GetData().IsProt())
          prot_ref = CConstRef <CProt_ref> (&( (*it)->GetData().GetProt() ));
      }
    }

    if (seq_feat.CanGetProduct()) {
        CBioseq_Handle bioseq_h = GetBioseqFromSeqLoc(seq_feat.GetProduct(), *thisInfo.scope);
        for (CFeat_CI prot_ci(bioseq_h, CSeqFeatData::e_Prot); prot_ci; ++prot_ci) {
          prot_ref 
              = CConstRef <CProt_ref> (&(prot_ci->GetOriginalFeature().GetData().GetProt())); 
          if (prot_ref->GetProcessed() == CProt_ref :: eProcessed_not_set) break;
	}
    }
  }

  return prot_ref;
};

/*
GetGBQualFromFeatQual(EFeat_qual_legal feat_legal_dt, int gb_qual, int& subfield)
{
};

string GetFirstGBQualMatch()

string GetFirstGBQualMatchConstraintName(
*/

string CBioseqTestAndRepData :: GetFieldValueForObject(const CSeq_feat& seq_feat, const CFeat_qual_choice& feat_qual)
{
  string ret_str(kEmptyStr);

  // for protein fields
  CConstRef <CProt_ref> prot = GetProtRefForFeature (seq_feat);

  /* fields common to some features */
  /* product */
  if (ret_str.empty()
      && ((feat_qual.IsLegal_qual() && feat_qual.GetLegal_qual() == eFeat_qual_legal_product) 
//          || (feat_qual.IsIllegal_qual() && DoesStringMatchConstraint ("product", field->data.ptrvalue))
))
  {
    if (prot.NotEmpty()) {
      if (prot->CanGetName()) {
        ret_str = *(prot->GetName().begin()); //str = GetFirstValNodeStringMatch (prp->name, scp);
      }
    } 
    else if (seq_feat.GetData().IsRna()) {
      ret_str = CBioseqTestAndRepData :: GetRNAProductString (seq_feat);
   //   str = GetRNAProductString (seq_feat, scp);
    }
  }

  /* actual GenBank qualifiers */
/*
  if (ret_str.empty()) {
    if (feat_qual.IsLegal_qual()) {
      gbqual = GetGBQualFromFeatQual (field->data.intvalue, &subfield);
      if (gbqual > -1) 
        str = GetFirstGBQualMatch (sfp->qual, ParFlat_GBQual_names [gbqual].name, subfield, scp);
    } else {
      str = GetFirstGBQualMatchConstraintName (sfp->qual, field->data.ptrvalue, scp);
    }
  }
*/
  return ret_str;
}


CSeqFeatData::ESubtype CBioseqTestAndRepData :: GetFeatdefFromFeatureType(EMacro_feature_type feat_field_type)
{
   switch (feat_field_type) {
     case eMacro_feature_type_any:
         return (CSeqFeatData :: eSubtype_any); break;
     default: return (CSeqFeatData :: eSubtype_bad);
   }
};


string CBioseqTestAndRepData :: GetQualFromFeature(const CSeq_feat& seq_feat, const CField_type& field_type)
{
  if (field_type.IsFeature_field()) {
     const CFeature_field& feat_field = field_type.GetFeature_field();
     EMacro_feature_type feat_field_type = feat_field.GetType();
     if (eMacro_feature_type_any == feat_field_type
             || seq_feat.GetData().GetSubtype() == GetFeatdefFromFeatureType(feat_field_type))
        return (GetFieldValueForObject(seq_feat, feat_field.GetField()));
  }
  return (kEmptyStr);
};


bool CBioseqTestAndRepData :: IsLocationOrganelle(const CBioSource::EGenome& genome)
{
    if (genome == CBioSource :: eGenome_chloroplast
      || genome == CBioSource :: eGenome_chromoplast
      || genome == CBioSource :: eGenome_kinetoplast
      || genome == CBioSource :: eGenome_mitochondrion
      || genome == CBioSource :: eGenome_cyanelle
      || genome == CBioSource :: eGenome_nucleomorph
      || genome == CBioSource :: eGenome_apicoplast
      || genome == CBioSource :: eGenome_leucoplast
      || genome == CBioSource :: eGenome_proplastid
      || genome == CBioSource :: eGenome_hydrogenosome
      || genome == CBioSource :: eGenome_plastid
      || genome == CBioSource :: eGenome_chromatophore) {
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
  else if (mRNA_prod.substr(0, pos_m) != feat_prod.substr(0, pos_f)) return false;

  if ( feat_prod.substr(pos_f, kCDSVariant.size())
                                 == mRNA_prod.substr(pos_m, kmRNAVariant.size()))
       return true;
  else return false;
};



void CBioseqTestAndRepData :: TestOnMRna(const CBioseq& bioseq)
{
  bool has_bad_molinfo = false, has_bad_biosrc = false, has_qual_ids = false;
  if (bioseq.GetInst().GetMol() != CSeq_inst::eMol_dna || !IsEukaryotic(bioseq)) return;
  ITERATE (vector <const CSeqdesc*>, it, bioseq_biosrc) {
    if (IsLocationOrganelle( (CBioSource::EGenome)(*it)->GetSource().GetGenome())) {
           has_bad_biosrc = true; break;
    }
  }
  if (has_bad_biosrc) return;
  
  ITERATE (vector <const CSeqdesc*>, it, bioseq_molinfo) {
    if ( (*it)->GetMolinfo().GetBiomol() == CMolInfo::eBiomol_genomic) {
         has_bad_molinfo = true; break;
    }
  }
  if (has_bad_molinfo) return;
 
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

  ITERATE (vector <const CSeq_feat*>, it, cd_feat) {
    if (IsPseudoSeqFeatOrXrefGene(*it)) continue;
    CConstRef <CSeq_feat> mRNA = GetmRNAforCDS(**it, *seq_entry);
    if (mRNA.Empty())
      thisInfo.test_item_list[GetName_no_mrna()].push_back(GetDiscItemText(**it));
    else {
      feat_prod = GetQualFromFeature(**it, *field_type);
      mRNA_prod = GetQualFromFeature(*mRNA, *field_type);
      if (feat_prod != mRNA_prod && !ProductsMatchForRefSeq(feat_prod, mRNA_prod))
         thisInfo.test_item_list[GetName_no_mrna()].push_back(GetDiscItemText(**it));
      if (!has_qual_ids) {
         if (!(mRNA->GetNamedQual("orig_protein_id").empty()) 
                                     && !(mRNA->GetNamedQual("orig_transcript_id").empty())) 
           has_qual_ids = true;
      }
    }     
  }

  strtmp = has_qual_ids ? "yes" : "no";
  thisInfo.test_item_list[GetName_pid_tid()].push_back(strtmp);
  thisTest.is_MRNA_run = true;
};


void CBioseq_DISC_CDS_WITHOUT_MRNA :: TestOnObj(const CBioseq& bioseq)
{
   if (!thisTest.is_MRNA_run) TestOnMRna(bioseq);
};


void CBioseq_DISC_CDS_WITHOUT_MRNA :: GetReport(CRef <CClickableItem>& c_item)
{
  c_item->description 
     = GetOtherComment(c_item->item_list.size(), "coding region does", "coding regions do")
        + " not have an mRNA.";
};


void CBioseq_MRNA_SHOULD_HAVE_PROTEIN_TRANSCRIPT_IDS :: TestOnObj(const CBioseq& bioseq)
{
  if (!thisTest.is_MRNA_run) TestOnMRna(bioseq);
};


void CBioseq_MRNA_SHOULD_HAVE_PROTEIN_TRANSCRIPT_IDS :: GetReport(CRef <CClickableItem>& c_item)
{
  bool has_ids = false;
  ITERATE (vector <string>, it, c_item->item_list) 
    if ( (*it).find("yes") != string::npos) {
       has_ids = true;
       break;
    }
  if (!has_ids) {
    c_item->item_list.clear();
    c_item->description = "no protein_id and transcript_id present.";
  }
};


void CBioseqTestAndRepData :: TestOnMolinfo(const CBioseq& bioseq)
{
   if (!bioseq.IsNa()) return;
   string desc;
   ITERATE (vector <const CSeqdesc*>, it, bioseq_molinfo) {
     desc = GetDiscItemText(bioseq);
     if ( (*it)->GetMolinfo().GetBiomol() != CMolInfo :: eBiomol_mRNA)
       thisInfo.test_item_list[GetName_mrna()].push_back(desc);
     if ( (*it)->GetMolinfo().GetTech() != CMolInfo :: eTech_tsa)
       thisInfo.test_item_list[GetName_tsa()].push_back(desc);
   }

   thisTest.is_MOLINFO_run = true;
};



void CBioseq_MOLTYPE_NOT_MRNA :: TestOnObj(const CBioseq& bioseq)
{
  if (!thisTest.is_MOLINFO_run) TestOnMolinfo(bioseq);
};

void CBioseq_MOLTYPE_NOT_MRNA :: GetReport(CRef <CClickableItem>& c_item)
{
   c_item->description
      = GetIsComment(c_item->item_list.size(), "molecule type") + "not set as mRNA.";
};


void CBioseq_TECHNIQUE_NOT_TSA :: TestOnObj(const CBioseq& bioseq)
{
  if (!thisTest.is_MOLINFO_run) TestOnMolinfo(bioseq);
};


void CBioseq_TECHNIQUE_NOT_TSA :: GetReport(CRef <CClickableItem>& c_item)
{
  c_item->description
      = GetIsComment(c_item->item_list.size(), "technique") + "not set as TSA.";
};



void CBioseq_GENE_PRODUCT_CONFLICT :: TestOnObj(const CBioseq& bioseq)
{  
   string gene_label(kEmptyStr), prod_nm, desc;
   ITERATE (vector <const CSeq_feat*>, it, cd_feat) {
     gene_label = kEmptyStr;
     // GetGeneLabel
     CGene_ref* gene = const_cast <CGene_ref*> ( (*it)->GetGeneXref());
     if (!gene) {
        CConstRef<CSeq_feat> g_olp= 
                                GetBestOverlappingFeat( (*it)->GetLocation(),
                                                   CSeqFeatData::e_Gene,
                                                   eOverlap_Contained,
                                                   *thisInfo.scope);
        if (g_olp.NotEmpty()) 
            gene = const_cast< CGene_ref* > (&(g_olp->GetData().GetGene()));
     } 
     if (gene && gene->CanGetLocus()) gene_label = gene->SetLocus();

     if (!(gene_label.empty())) {
       prod_nm = GetProdNmForCD(**it);
       desc = GetDiscItemText(**it);
       Str2SubDt::iterator it = GENE_PRODUCT_CONFLICT_cds.find(gene_label);
       if (it == GENE_PRODUCT_CONFLICT_cds.end())
           GENE_PRODUCT_CONFLICT_cds[gene_label] 
                = CRef < GeneralDiscSubDt > (new GeneralDiscSubDt(desc, prod_nm));
       else {
          if (it->second->str != prod_nm) { // conflict
            if ( !( it->second->sf0_added )) {
                thisInfo.test_item_list[GetName()].push_back(it->second->obj_text[0]);
                it->second->sf0_added = true;
            }
            thisInfo.test_item_list[GetName()].push_back(desc);
            it->second->obj_text.push_back(strtmp);
          }
       }
     }
   }
};


void CBioseq_GENE_PRODUCT_CONFLICT :: GetReport(CRef <CClickableItem>& c_item)
{
  c_item->description = GetHasComment(c_item->item_list.size(), "coding region")
      + "the same gene name as another coding region but a different product.";

  ITERATE ( Str2SubDt, it, GENE_PRODUCT_CONFLICT_cds) {
    if ( it->second->obj_text.size() > 1) {
      CRef <CClickableItem> c_sub (new CClickableItem);
      c_sub->setting_name = GetName();
      c_sub->item_list = it->second->obj_text;
      strtmp = (it->second->str.empty())? kEmptyStr: ("(" + it->second->str + ") ");
      c_sub->description = GetHasComment(c_sub->item_list.size(), "coding region")
         + "the same gene name " + strtmp
         + (string)"as another coding region but a different product.";
      c_item->subcategories.push_back(c_sub);
    }
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
        NCBI_THROW(CException, eUnknown, "GetTrnaProductString: can't get from.");
        break;
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
        str = ext.GetName();  break;
    case CRNA_ref::C_Ext::e_TRNA:
        if (ext.GetTRNA().CanGetAa()) str = GetTrnaProductString(ext.GetTRNA()); 
        break;
    case CRNA_ref::C_Ext::e_Gen:
        if (ext.GetGen().CanGetProduct())
          str = ext.GetGen().GetProduct(); break;
    default: break;
  }

  return str;
};


void CBioseq_TEST_UNUSUAL_MISC_RNA :: TestOnObj(const CBioseq& bioseq)
{
   string product;
   ITERATE (vector <const CSeq_feat*>, it, otherRna_feat) {
      const CRNA_ref& rna_ref = (*it)->GetData().GetRna();
      if ( rna_ref.CanGetExt() && rna_ref.GetExt().Which() != CRNA_ref::C_Ext::e_not_set ) {
         product = GetRnaRefProductString( (*it)->GetData().GetRna());
         if (product.find("ITS") == string::npos 
                          && product.find("internal transcribed spacer") == string::npos) { 
            thisInfo.test_item_list[GetName()].push_back(GetDiscItemText(**it));
            break;
         }
      }
   }
};


void CBioseq_TEST_UNUSUAL_MISC_RNA :: GetReport(CRef <CClickableItem>& c_item)
{
  c_item->description
    = GetOtherComment(c_item->item_list.size(), "unexpected misc_RNA feature", "unexpected misc_RNA features")
       + " found. misc_RNAs are unusual in a genome, consider using ncRNA, misc_binding, or misc_feature as appropriate.";
};



int CBioseq_DISC_PARTIAL_PROBLEMS :: DistanceToUpstreamGap(const unsigned& pos, const CBioseq& bioseq) 
{
   int offset = 0, last_gap = -1;
   ITERATE (list <CRef <CDelta_seq> >, it, bioseq.GetInst().GetExt().GetDelta().Get()) {
     if ( (*it)->IsLoc()) offset += (*it)->GetLoc().GetTotalRange().GetLength(); 
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


int CBioseq_DISC_PARTIAL_PROBLEMS :: DistanceToDownstreamGap (const int& pos, const CBioseq& bioseq)
{
  int offset = 0;

  if (pos < 0) return -1;

  ITERATE (list <CRef <CDelta_seq> >, it, bioseq.GetInst().GetExt().GetDelta().Get()) {
     if ( (*it)->IsLoc()) offset += (*it)->GetLoc().GetTotalRange().GetLength();
     else if ((*it)->IsLiteral()) {
            const CSeq_literal& seq_lit = (*it)->GetLiteral();
            if ( (!seq_lit.CanGetSeq_data() || seq_lit.GetSeq_data().IsGap()) //IsDeltaSeqGap()
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
    if (seq_loc.IsInt()) {
      partial5 = seq_loc.GetInt().IsPartialStart(eExtreme_Biological);
      partial3 = seq_loc.GetInt().IsPartialStop(eExtreme_Biological);
      if (strand == eNa_strand_minus) {
        partialL = partial3;
        partialR = partial5;
      }
      else {
        partialL = partial5;
        partialR = partial3;
      }
      if ( (partialL && CouldExtendLeft(bioseq, seq_loc.GetTotalRange().GetFrom()))
            || (partialR && CouldExtendRight(bioseq,(int)seq_loc.GetTotalRange().GetTo())))
        thisInfo.test_item_list[GetName()].push_back(GetDiscItemText(**it));
    }
  }
};


void CBioseq_DISC_PARTIAL_PROBLEMS :: GetReport(CRef <CClickableItem>& c_item)
{
  c_item->description 
    = GetHasComment(c_item->item_list.size(), "feature") 
        + "partial ends that do not abut the end of the sequence or a gap, but could be extended by 3 or fewer nucleotides to do so"; 
};



bool CBioseq_DISC_SUSPICIOUS_NOTE_TEXT :: HasSuspiciousStr(const string& str, string& sus_str)
{
  sus_str = kEmptyStr;
  ITERATE (vector <string>, it, thisInfo.suspicious_notes) {
    if (NStr::FindNoCase(str, *it) != string::npos) { sus_str = *it; return true;}
  }
  return false;
};


void CBioseq_DISC_SUSPICIOUS_NOTE_TEXT :: TestOnObj(const CBioseq& bioseq)
{
  string sus_str, sus_str2, desc;
  ITERATE (vector <const CSeq_feat*>, it, gene_feat) {
    desc = GetDiscItemText(**it);
    if ( (*it)->CanGetComment() && HasSuspiciousStr((*it)->GetComment(), sus_str))
        thisInfo.test_item_list[GetName()].push_back(sus_str + "$" + desc);
    if ( (*it)->GetData().GetGene().CanGetDesc()
                  && HasSuspiciousStr((*it)->GetData().GetGene().GetDesc(), sus_str2)
                  && sus_str != sus_str2) 
        thisInfo.test_item_list[GetName()].push_back(sus_str + "$" + desc);
  }

  ITERATE (vector <const CSeq_feat*>, it, cd_feat) {
    if ( (*it)->CanGetComment() && HasSuspiciousStr((*it)->GetComment(), sus_str))
          thisInfo.test_item_list[GetName()].push_back(sus_str + "$" + GetDiscItemText(**it));
  }
  
  ITERATE (vector <const CSeq_feat*>, it, prot_feat) {
    if ( (*it)->GetData().GetSubtype() != CSeqFeatData::eSubtype_prot) continue; // FEATDEF_PROT
    if ( (*it)->GetData().GetProt().CanGetDesc() 
                    && HasSuspiciousStr((*it)->GetData().GetProt().GetDesc(), sus_str))
          thisInfo.test_item_list[GetName()].push_back(sus_str + "$" + GetDiscItemText(**it));

  }
  
  ITERATE (vector <const CSeq_feat*>, it, miscfeat_feat) {
    if ( (*it)->CanGetComment() && HasSuspiciousStr((*it)->GetComment(), sus_str) )
          thisInfo.test_item_list[GetName()].push_back(sus_str + "$" + GetDiscItemText(**it));
  }
};



void CBioseq_DISC_SUSPICIOUS_NOTE_TEXT :: GetReport(CRef <CClickableItem>& c_item)
{
  Str2Strs sus_str2list;
  GetTestItemList(c_item->item_list, sus_str2list);
  c_item->item_list.clear();
  ITERATE (Str2Strs, it, sus_str2list) {
    CRef <CClickableItem> c_sub (new CClickableItem);
    AddSubcategories(c_item, GetName(), it->second, "note text conains ","note texts contain ",
                                                              true, e_OtherComment, it->first);
  }

  c_item->description 
     = GetOtherComment(c_item->item_list.size(), "note text contains", "note texts contain")
           + " suspicious phrases";
};


void CBioseqTestAndRepData :: TestOnShortContigSequence(const CBioseq& bioseq)
{
  string desc;
  unsigned len = bioseq.GetLength();
  if (!bioseq.IsAa()) {
     desc = GetDiscItemText(bioseq);
     if (len < 200)
       thisInfo.test_item_list[GetName_contig()].push_back(desc);
     CConstRef <CBioseq_set> bioseq_set = bioseq.GetParentSet();
     if (bioseq_set.Empty() || bioseq_set->GetClass() != CBioseq_set::eClass_parts) {
       if (len < 200) thisInfo.test_item_list[GetName_200seq()].push_back(desc);
       else if (len < 50) thisInfo.test_item_list[GetName_50seq()].push_back(desc);
     } 
  }

  thisTest.is_SHORT_run = true;
}



void CBioseq_SHORT_CONTIG :: TestOnObj(const CBioseq& bioseq)
{
   if (!thisTest.is_SHORT_run) TestOnShortContigSequence(bioseq);
/*
   if (bioseq.IsNa() && bioseq.GetLength() < 200)
      thisInfo.test_item_list[GetName()].push_back(GetDiscItemText(bioseq));
*/
};


void CBioseq_SHORT_CONTIG :: GetReport(CRef <CClickableItem>& c_item)
{
   c_item->description 
       = GetIsComment(c_item->item_list.size(), "contig") + "shorter than 200 nt.";
};


void CBioseq_SHORT_SEQUENCES :: TestOnObj(const CBioseq& bioseq)
{
   if (!thisTest.is_SHORT_run) TestOnShortContigSequence(bioseq);
};


void CBioseq_SHORT_SEQUENCES :: GetReport(CRef <CClickableItem>& c_item)
{
  c_item->description
    = GetIsComment(c_item->item_list.size(), "sequence") + "shorter than 50 nt.";
};



void CBioseq_SHORT_SEQUENCES_200 :: TestOnObj(const CBioseq& bioseq)
{
   if (!thisTest.is_SHORT_run) TestOnShortContigSequence(bioseq);
};

void CBioseq_SHORT_SEQUENCES_200 :: GetReport(CRef <CClickableItem>& c_item)
{
    c_item->description
    = GetIsComment(c_item->item_list.size(), "sequence") + "shorter than 200 nt.";
};



void CBioseq_HYPOTHETICAL_CDS_HAVING_GENE_NAME :: TestOnObj(const CBioseq& bioseq)
{
  SAnnotSelector sel_seqfeat;
  sel_seqfeat.IncludeFeatType(CSeqFeatData::e_Prot);
  string locus;
  ITERATE (vector <const CSeq_feat*>, it, cd_feat) {
/*
    const CGene_ref* gene = (*it)->GetGeneXref();
    if (gene && gene->IsSuppressed()) continue;
    else if (gene) {
       // look for overlap_gene = SeqMgrGetGeneByLocusTag (bsp, grp->locus_tag, &fcontext);
       // overlap_gene = SeqMgrGetFeatureByLabel (bsp, grp->locus, SEQFEAT_GENE, 0, &fcontext);
    }
    else {
         CConstRef <CSeq_feat> gene_olp= GetBestOverlappingFeat((*it)->GetLocation(),
                                                             CSeqFeatData::e_Gene,
                                                             eOverlap_Contained,
                                                             *thisInfo.scope);
         if (gene_olp.Empty() || !gene_olp->GetData().GetGene().CanGetLocus()
                 || gene_olp->GetData().GetGene().GetLocus().empty()) continue;
    }
*/
    CConstRef <CSeq_feat> gene_feat (GetGeneForFeature(**it));
    if (gene_feat.Empty() || !(gene_feat->GetData().GetGene().CanGetLocus())
          || !(gene_feat->GetData().GetGene().GetLocus().empty()) ) continue;   // no gene name
    if ((*it)->CanGetProduct()) {
      CBioseq_Handle bioseq_prot = GetBioseqFromSeqLoc((*it)->GetProduct(),*thisInfo.scope);
      CFeat_CI feat_it(bioseq_prot, sel_seqfeat);
      if (!feat_it) continue;
      const CProt_ref& prot_ref = feat_it->GetOriginalFeature().GetData().GetProt();
      if (prot_ref.CanGetName()) {
          ITERATE (list <string>, jt, prot_ref.GetName()) {
            if (NStr::FindNoCase(*jt, "hypothetical protein") != string::npos)
                thisInfo.test_item_list[GetName()].push_back(GetDiscItemText(**it));
          }
      }
    }
  }
};


void CBioseq_HYPOTHETICAL_CDS_HAVING_GENE_NAME :: GetReport(CRef <CClickableItem>& c_item)
{
   c_item->description 
          = GetHasComment(c_item->item_list.size(), "hypothetical coding region")
                              + "a gene name";
};



void CBioseqTestAndRepData :: TestOverlapping_ed_Feats(const vector <const CSeq_feat*>& feat, const string& setting_name, bool isGene, bool isOverlapped)
{
   string overlapping_ed_feats("|");
   unsigned i, j;
   for (i=0; (int)i< (int)(feat.size()-1); i++) {
      if (isOverlapped && !IsUnknown(overlapping_ed_feats, i)) continue;
      for (j=i+1; j< feat.size(); j++) {
           if (!isOverlapped && !IsUnknown(overlapping_ed_feats, j)) continue;
           const CSeq_loc& loc_i = feat[i]->GetLocation();
           const CSeq_loc& loc_j = feat[j]->GetLocation();

           // all feats more distant than the loc_j will have no chance of overlapping
           if (loc_i.GetStop(eExtreme_Positional) < loc_j.GetStart(eExtreme_Positional))
               break;

           if (isGene && loc_i.GetStrand() != loc_j.GetStrand()) continue;
           sequence::ECompare ovlp = Compare(loc_i, loc_j, thisInfo.scope);
           if (isOverlapped) {
               if (ovlp == sequence::eContained || ovlp == sequence::eSame) {
                  thisInfo.test_item_list[setting_name].push_back(
                                                            GetDiscItemText(*feat[i]));
                  overlapping_ed_feats += NStr::UIntToString(i) + "|";
                  break;
               }
               else if (ovlp == sequence::eContains 
                                          && IsUnknown(overlapping_ed_feats, j)) {
                    thisInfo.test_item_list[setting_name].push_back(
                                                            GetDiscItemText(*feat[j]));
                    overlapping_ed_feats += NStr::UIntToString(j) + "|";
               }
           }
           else if (ovlp != sequence :: eNoOverlap) {
               if (IsUnknown(overlapping_ed_feats, i)) {
                  thisInfo.test_item_list[setting_name].push_back(
                                                          GetDiscItemText(*feat[i]));
                  overlapping_ed_feats += NStr::UIntToString(i) + "|";
               }
               thisInfo.test_item_list[setting_name].push_back(
                                                            GetDiscItemText(*feat[j]));
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
  c_item->description 
     = GetOtherComment(c_item->item_list.size(), "rRNA feature overlaps", "rRNA features overlap")
      + " another rRNA feature.";
};


void CBioseq_OVERLAPPING_GENES :: TestOnObj(const CBioseq& bioseq)
{
    TestOverlapping_ed_Feats(gene_feat, GetName(), true, false);
};



void CBioseq_OVERLAPPING_GENES :: GetReport(CRef <CClickableItem>& c_item)
{
    c_item->description 
      = GetOtherComment(c_item->item_list.size(), "gene overlaps", "genes overlap")
                           + " another gene on the same strand.";
};


void CBioseq_FIND_OVERLAPPED_GENES :: TestOnObj(const CBioseq& bioseq)
{
    TestOverlapping_ed_Feats(gene_feat, GetName());
};



void CBioseq_FIND_OVERLAPPED_GENES :: GetReport(CRef <CClickableItem>& c_item)
{
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
    CSeqVector seq_vec = bioseq_h.GetSeqVector(CBioseq_Handle::eCoding_Iupac);
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
         if (start > -1) { /* if we are already in an interval, see if we should continue to be */
           len = i - start;
           if ( ((float)num_ns / (float)len) < min_pct) {
              /* is the interval long enough to qualify? */
              if (len >= min_length) {
                 thisInfo.test_item_list[GetName()].push_back(GetDiscItemText(bioseq));
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
    if (!found_interval && start > -1 && len >= min_length)
         thisInfo.test_item_list[GetName()].push_back(GetDiscItemText(bioseq));
};


void CBioseq_TEST_LOW_QUALITY_REGION :: GetReport(CRef <CClickableItem>& c_item)
{
  c_item->description 
    = GetOtherComment(c_item->item_list.size(), "sequence contains", "sequeces contain") 
       + " low quality region";
};



bool CBioseq_TEST_BAD_GENE_NAME :: GeneNameHas4Numbers(const string& locus)
{
  bool num_digits=0;
  for (unsigned i=0; i< locus.size() && num_digits < 4; i++)
    if (isdigit(locus[i])) num_digits++;
    else num_digits = 0;

  if (num_digits >=4) return true;
  else return false; 
};


void CBioseq_TEST_BAD_GENE_NAME :: TestOnObj(const CBioseq& bioseq)
{
  string desc, locus;
  bool is_euka;
  ITERATE (vector <const CSeq_feat*>, it, gene_feat) {  
    const CSeqFeatData& seq_feat_dt = (*it)->GetData();
    if (seq_feat_dt.GetGene().CanGetLocus()) {
      locus = seq_feat_dt.GetGene().GetLocus();
      if (!locus.empty()) {
        desc = GetDiscItemText(**it); 
        if (locus.size() > 10)
           thisInfo.test_item_list[GetName()].push_back("more than 10 characters$" + desc);
        ITERATE (vector <string>, jt, thisInfo.bad_gene_names_contained)
          if (NStr::FindNoCase(locus, *jt) != string::npos)
              thisInfo.test_item_list[GetName()].push_back(*jt + "$" + desc);
        if (GeneNameHas4Numbers(locus))
           thisInfo.test_item_list[GetName()].push_back("4 or more consecutive numbers$" + desc);
        is_euka = false;
 
        // bad_bacterial_gene_name
        ITERATE (vector <const CSeqdesc*>, jt, bioseq_biosrc)   // bioseq_biosrc used only once
          if (HasLineage((*jt)->GetSource(), "Eukaryota")) {is_euka = true; break;}
        if (!is_euka && (!isalpha(locus[0]) || !islower(locus[0]))) 
           thisInfo.test_item_list[GetName()].push_back("bad$" + desc);      
      }
    }
  }
};


void CBioseq_TEST_BAD_GENE_NAME :: GetReport(CRef <CClickableItem>& c_item)
{
  c_item->item_list.clear();
  Str2Strs bad_name_genes;
  vector <string> bad_bacterial, rep_arr;
  ITERATE (vector <string>, it, thisInfo.test_item_list[GetName()]) {
    rep_arr = NStr::Tokenize(*it, "$", rep_arr);
    if (rep_arr[0] == "bad") c_item->item_list.push_back(*it);
    else bad_name_genes[rep_arr[0]].push_back(rep_arr[1]);
    rep_arr.clear();
  }

  bool has_bad_bacterial = false;
  if (!c_item->item_list.empty()) {
    c_item->setting_name = GetName_bad();
    has_bad_bacterial = true;
    if (!bad_name_genes.empty()) 
         c_item = CRef <CClickableItem> (new CClickableItem);
  } 
  if (!bad_name_genes.empty()) {
    c_item->setting_name = GetName();
  
    if (bad_name_genes.size() == 1) {
       c_item->item_list = bad_name_genes.begin()->second;
       c_item->description
         = GetOtherComment(c_item->item_list.size(), "gene contains ", "genes contain ") 
            + bad_name_genes.begin()->first;
    }
    else {
      ITERATE (Str2Strs, it, bad_name_genes) {
        CRef <CClickableItem> c_sub (new CClickableItem);
        c_sub->setting_name = c_item->setting_name;
        c_sub->item_list = it->second;
        c_sub->description 
          =GetOtherComment(c_sub->item_list.size(),"gene contains ", "genes contain ") +it->first;
        c_item->subcategories.push_back(c_sub);
        c_item->item_list.insert(c_item->item_list.end(), c_sub->item_list.begin(), 
                                                                         c_sub->item_list.end());
      }
      c_item->description
        = GetOtherComment(c_item->item_list.size(), "gene contains", "genes contain") 
          + " suspect phrase or characters";
    }
    if (has_bad_bacterial) thisInfo.disc_report_data.push_back(c_item);
  }
};



void CBioseq_DISC_SHORT_RRNA :: TestOnObj(const CBioseq& bioseq)
{
  string rrna_name;
  unsigned len;
  ITERATE (vector <const CSeq_feat*>, it, rrna_feat) {
    if ((*it)->CanGetPartial() && (*it)->GetPartial()) continue; 
    len = (*it)->GetLocation().GetTotalRange().GetLength();
    rrna_name = GetRNAProductString(**it);
    ITERATE (Str2UInt, jt, thisInfo.rRNATerms) {
      if (NStr::FindNoCase(rrna_name, jt->first) != string::npos && len < jt->second) {
           thisInfo.test_item_list[GetName()].push_back(GetDiscItemText(**it));
           break;
      }
    }
  }
};



void CBioseq_DISC_SHORT_RRNA :: GetReport(CRef <CClickableItem>& c_item)
{
   c_item->description 
      = GetIsComment(c_item->item_list.size(), "rRNA feature") + "too short.";
};



bool CBioseq_DISC_BAD_GENE_STRAND :: StrandOk(ENa_strand strand1, ENa_strand strand2)
{
  if (strand1 == eNa_strand_minus && strand2 != eNa_strand_minus) return false;
  else if (strand1 != eNa_strand_minus && strand2 == eNa_strand_minus) return false;
  else return true;
};



bool CBioseq_DISC_BAD_GENE_STRAND :: AreIntervalStrandsOk(const CSeq_loc& g_loc, const CSeq_loc& f_loc) 
{
  bool   found_match;
  bool   found_bad = FALSE;
  ENa_strand     feat_strand, gene_strand;

  for (CSeq_loc_CI f_loc_it(f_loc); f_loc_it && !found_bad; ++ f_loc_it) {
     found_match = false;
     for (CSeq_loc_CI g_loc_it(g_loc); g_loc_it && !found_match; ++ g_loc_it) {
        sequence::ECompare ovlp = Compare(f_loc_it.GetEmbeddingSeq_loc(), 
                                        g_loc_it.GetEmbeddingSeq_loc(), thisInfo.scope);
        if (ovlp == sequence::eContained || ovlp == sequence::eSame) {
          found_match = true;
          feat_strand = f_loc_it.GetStrand();
          gene_strand = g_loc_it.GetStrand();
          if (!StrandOk(feat_strand, gene_strand)) found_bad = true;
        }
     }
  }

  return !found_bad;
};



void CBioseq_DISC_BAD_GENE_STRAND :: TestOnObj(const CBioseq& bioseq)
{
  if (bioseq.IsAa()) return;
  bool is_error, g_mixed_strand;

  unsigned f_left, f_right, g_left, g_right;
  ITERATE (vector <const CSeq_feat*>, it, gene_feat) {
    const CSeq_loc& g_loc = (*it)->GetLocation();
    ITERATE (vector <const CSeq_feat*>, jt, all_feat) {
       const CSeq_loc& f_loc = (*jt)->GetLocation();
       const CSeqFeatData& seq_feat_dt = (*jt)->GetData();
       if ( seq_feat_dt.IsGene() 
              || seq_feat_dt.GetSubtype() == CSeqFeatData::eSubtype_primer_bind) continue;
       f_left = f_loc.GetStart(eExtreme_Positional);
       f_right = f_loc.GetStop(eExtreme_Positional);
       g_left = g_loc.GetStart(eExtreme_Positional);
       g_right = g_loc.GetStop(eExtreme_Positional);
       if (f_left == g_left || f_right == g_right) {
          g_mixed_strand = is_error = false;
          if (g_loc.IsMix()) {
             bool has_strand_plus = false, has_strand_minus = false;
             ITERATE (list <CRef <CSeq_loc> >, it, g_loc.GetMix().Get()) {
                if ( (*it)->GetStrand() == eNa_strand_plus) has_strand_plus = true;
                else if ( (*it)->GetStrand() == eNa_strand_minus) has_strand_minus = true;
                if (has_strand_plus && has_strand_minus) break; 
             }
             if (has_strand_plus && has_strand_minus) g_mixed_strand = true;
          }
          if (g_mixed_strand) is_error = !AreIntervalStrandsOk(g_loc, f_loc);
          else if (!StrandOk(f_loc.GetStrand(), g_loc.GetStrand())) is_error = true;
          if (is_error)
            thisInfo.test_item_list[GetName()].push_back(
                                         GetDiscItemText(**it)+"$" + GetDiscItemText(**jt));
       } 
       if (f_left > g_right) break;
    }
  }
};


void CBioseq_DISC_BAD_GENE_STRAND :: GetReport(CRef <CClickableItem>& c_item)
{
   vector <string> rep_arr;
   c_item->item_list.clear();
   ITERATE (vector <string>, it, thisInfo.test_item_list[GetName()]) {
     rep_arr = NStr::Tokenize(*it, "$", rep_arr);
     CRef <CClickableItem> c_sub (new CClickableItem);
     c_sub->setting_name = GetName();
     c_sub->item_list.insert(c_sub->item_list.end(), rep_arr.begin(), rep_arr.end());
     c_sub->description = "Gene and feature strands conflict";
     c_item->subcategories.push_back(c_sub); 
     c_item->item_list.insert(c_item->item_list.end(), rep_arr.begin(), rep_arr.end());
     rep_arr.clear(); 
   }

   c_item->description 
      = GetOtherComment(c_item->item_list.size()/2, 
                           "feature location conflicts", "feature locations conflict") 
          + "with gene location strands";
};



bool CBioseq_DISC_SHORT_INTRON :: PosIsAt5End(unsigned pos, CConstRef <CBioseq>& bioseq)
{
  unsigned seq_end = 0;

  if (!pos) return true;
  else {
    const CSeq_inst& seq_inst = bioseq->GetInst();
    if (seq_inst.GetRepr() != CSeq_inst::eRepr_seg) return false;
    else {
      if (seq_inst.CanGetExt()) {
         const CSeq_ext& seq_ext = seq_inst.GetExt();
         if (seq_ext.Which() != CSeq_ext :: e_Seg) return false;
         else {
           ITERATE (list <CRef <CSeq_loc> >, it, seq_ext.GetSeg().Get()) {
             seq_end += (*it)->GetTotalRange().GetLength();
             if (pos == seq_end) return true;
           }
        }
      }
    }
  }
  return false;
};



bool CBioseq_DISC_SHORT_INTRON :: PosIsAt3End(const unsigned pos, CConstRef <CBioseq>& bioseq)
{
  unsigned seq_end = 0;

  if (pos == bioseq->GetLength() - 1) return true;
  else {
   const CSeq_inst& seq_inst = bioseq->GetInst();
   if (seq_inst.GetRepr() != CSeq_inst::eRepr_seg) return false;
   else {
     if (seq_inst.CanGetExt()) {
        const CSeq_ext& seq_ext = seq_inst.GetExt();
        if (seq_ext.Which() != CSeq_ext :: e_Seg) return false;
        else {
          ITERATE (list <CRef <CSeq_loc> >, it, seq_ext.GetSeg().Get()) {
            seq_end += (*it)->GetTotalRange().GetLength();
            if (pos == seq_end -1) return true;
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
  bool partial5, partial3, excpt;
  unsigned start, stop;
  string desc_txt;
  ITERATE (vector <const CSeq_feat*>, it, intron_feat) {
    if (IsPseudoSeqFeatOrXrefGene(*it)) continue;
    const CSeq_loc& seq_loc = (*it)->GetLocation();
    if (seq_loc.GetTotalRange().GetLength() >= 11) continue;
    strand = seq_loc.GetStrand();
    partial5 = partial3 = false;
    if (seq_loc.IsInt()) {
       start = seq_loc.GetTotalRange().GetFrom();
       stop = seq_loc.GetTotalRange().GetTo();
       partial5 = seq_loc.GetInt().IsPartialStart(eExtreme_Biological);
       partial3 = seq_loc.GetInt().IsPartialStop(eExtreme_Biological);
       //CBioseq_Handle bioseq_h = GetBioseqFromSeqLoc(seq_loc, *thisInfo.scope);
       CConstRef <CBioseq> 
         bioseq_new = GetBioseqFromSeqLoc(seq_loc, *thisInfo.scope).GetCompleteBioseq();
       if ( (*it)->CanGetExcept() && (*it)->GetExcept()) excpt = true;
       else excpt = false;
       desc_txt = (excpt)? GetDiscItemText(**it) : "except$" + GetDiscItemText(**it);      
       if (bioseq_new.Empty()) thisInfo.test_item_list[GetName()].push_back(desc_txt);
       else if ( (partial5 && strand != eNa_strand_minus && PosIsAt5End(start, bioseq_new))
             || (partial3 && strand == eNa_strand_minus && PosIsAt5End (stop, bioseq_new) )
             || (partial5 && strand == eNa_strand_minus && PosIsAt3End (start, bioseq_new))
             || (partial3 && strand != eNa_strand_minus && PosIsAt3End (stop, bioseq_new)) )
           continue;
       else thisInfo.test_item_list[GetName()].push_back(desc_txt);
    } 
  }  

  bool found_short;
  unsigned last_start, last_stop;
  ITERATE (vector <const CSeq_feat*>, it, cd_feat) {
    found_short = false;
    if ( (*it)->CanGetExcept() && (*it)->GetExcept() ) continue;
    CSeq_loc_CI loc_it( (*it)->GetLocation() );
    last_start = loc_it.GetRange().GetFrom();
    last_stop = loc_it.GetRange().GetTo();
    for (++loc_it; loc_it; ++loc_it) {
      start = loc_it.GetRange().GetFrom();
      stop = loc_it.GetRange().GetTo();
      if (ABS((int)(start - last_stop)) < 11 || ABS ((int)(stop - last_start)) < 11) {
               found_short = true; break;
      }
      last_start = start;
      last_stop = stop; 
    }
    if (found_short) 
         thisInfo.test_item_list[GetName()].push_back(GetDiscItemText(**it));
  }
};



void CBioseq_DISC_SHORT_INTRON :: GetReport(CRef <CClickableItem>& c_item)
{
  vector <string> rep_arr;
  vector <string> with_exception;
  bool any_no_exception = false;
  c_item->item_list.clear();
  ITERATE (vector <string>, it, thisInfo.test_item_list[GetName()]) {
    rep_arr = NStr::Tokenize(*it, "$", rep_arr);
    if (rep_arr.size() == 1) {
          any_no_exception = true;
          c_item->item_list.push_back(*it);
    }
    else {
      with_exception.push_back(rep_arr[1]);
      c_item->item_list.push_back(rep_arr[1]);
    }
    rep_arr.clear(); 
  }

  if (any_no_exception) {
     c_item->description= GetIsComment(c_item->item_list.size(), "intron") + "shorter than 11 nt.";
     if (!with_exception.empty()) {
       CRef <CClickableItem> c_sub (new CClickableItem);
       c_sub->setting_name = GetName();
       c_sub->item_list = with_exception;
       c_sub->description = GetIsComment(c_sub->item_list.size(), "intron")
                                       + "shorter than 11 nt and have an exception";
       c_item->subcategories.push_back(c_sub);
     }
  }
  else {
    c_item->description= GetIsComment(c_item->item_list.size(), "intron")
                                       + "shorter than 11 nt and have an exception";
  }
};



bool CBioseqTestAndRepData :: IsDeltaSeqWithFarpointers(const CBioseq& bioseq)
{
  bool rval = false;

  if (bioseq.GetInst().GetRepr() != CSeq_inst :: eRepr_delta) return false;
  if (bioseq.GetInst().CanGetExt()) {
    ITERATE (list <CRef <CDelta_seq> >, it, bioseq.GetInst().GetExt().GetDelta().Get()) 
       if ( (*it)->IsLoc()) {rval = true; break;}
  }
  return rval;
};




void CBioseqTestAndRepData :: TestOnBasesN(const CBioseq& bioseq)
{
    if (IsDeltaSeqWithFarpointers (bioseq)) return;
   
    unsigned cnt_a, cnt_t, cnt_g, cnt_c, cnt_n, tot_n, cnt_non_nt;
    cnt_a = cnt_t = cnt_g = cnt_c = cnt_n = tot_n = cnt_non_nt = 0;
    unsigned start_n, i=0;
    string n10_intvls(kEmptyStr), n14_intvls(kEmptyStr);
    CBioseq_Handle bioseq_h = thisInfo.scope->GetBioseqHandle(bioseq);
    CSeqVector seq_vec = bioseq_h.GetSeqVector(CBioseq_Handle::eCoding_Iupac);
    for (CSeqVector_CI it =seq_vec.begin(); it; ++it, i++) {
// if ( it.IsInGap()) cerr << "is in gap " << (*it) << endl;// gaps represented as 'N's
      if ( (*it) == 'N') {
        if (!cnt_n) start_n = i+1;
        cnt_n++;
        tot_n++;
      }
      else {
         if (cnt_n >= 10) 
            n10_intvls += ", " + NStr::UIntToString(start_n) + "-" + NStr::UIntToString(i);
         if (cnt_n >= 14) 
            n14_intvls += ", " + NStr::UIntToString(start_n) + "-" + NStr::UIntToString(i);
         cnt_n = 0;
         if (!cnt_a && (*it) == 'A') cnt_a++;
         else if (!cnt_t && (*it) == 'T') cnt_t++;
         else if (!cnt_c && (*it) == 'C') cnt_c++;
         else if (!cnt_g && (*it) == 'G') cnt_g++;
         else cnt_non_nt++;
      }
    }
    // last count
    if (cnt_n >= 10)   
          n10_intvls += ", " + NStr::UIntToString(start_n) + "-" + NStr::UIntToString(i);
    if (cnt_n >= 14)
          n14_intvls += ", " + NStr::UIntToString(start_n) + "-" + NStr::UIntToString(i);
   
    const CSeq_id& new_seq_id =
                       BioseqToBestSeqId(*(bioseq_h.GetCompleteBioseq()), CSeq_id::e_Genbank);
    string id_str = new_seq_id.AsFastaString();
    id_str = id_str.substr(id_str.find("|")+1);

    string desc = GetDiscItemText(bioseq);
    if (!n10_intvls.empty())  
       thisInfo.test_item_list[GetName_n10()].push_back(
                                   desc + "$" + id_str + "#" + n10_intvls.substr(2));
    if (!n14_intvls.empty())   // just for test, should be in the TSA report          
       thisInfo.test_item_list[GetName_n14()].push_back(
                                  desc + "$" + id_str + "#" + n14_intvls.substr(2));
    if (!cnt_a) thisInfo.test_item_list[GetName_0()].push_back("A$" + desc);
    if (!cnt_t) thisInfo.test_item_list[GetName_0()].push_back("T$" + desc);
    if (!cnt_c) thisInfo.test_item_list[GetName_0()].push_back("C$" + desc);
    if (!cnt_g) thisInfo.test_item_list[GetName_0()].push_back("G$" + desc);
    if (tot_n && (float)tot_n/(float)bioseq.GetLength() > 0.05)
          thisInfo.test_item_list[GetName_5perc()].push_back(desc);
    if (tot_n && (float)tot_n/(float)bioseq.GetLength() > 0.10)
          thisInfo.test_item_list[GetName_10perc()].push_back(desc);
    if (cnt_non_nt) thisInfo.test_item_list[GetName_non_nt()].push_back(desc);

    thisTest.is_BASES_N_run = true;
};


void CBioseqTestAndRepData :: AddNsReport(CRef <CClickableItem>& c_item, bool is_n10)
{
   Str2Strs desc2intvls;
   GetTestItemList(c_item->item_list, desc2intvls);
   c_item->item_list.clear();

   vector <string> rep_dt;
   ITERATE (Str2Strs, it, desc2intvls) {
     c_item->item_list.push_back(it->first);
     CRef <CClickableItem> c_sub (new CClickableItem);
     c_sub->setting_name = GetName();
     c_sub->item_list.push_back(it->first);
     rep_dt = NStr::Tokenize(it->second[0], "#", rep_dt);
     c_sub->description =
               rep_dt[0] + " has runs of Ns at the following locations:\n" + rep_dt[1];
     c_item->subcategories.push_back(c_sub);
     rep_dt.clear();
   }
   c_item->description = GetHasComment(c_item->item_list.size(), "sequence") 
                           + "runs of " + (is_n10 ? "10":"14") + " or more Ns.";
};



void CBioseq_N_RUNS :: TestOnObj(const CBioseq& bioseq)
{
   if (!thisTest.is_BASES_N_run) TestOnBasesN(bioseq);
};
 

void CBioseq_N_RUNS :: GetReport(CRef <CClickableItem>& c_item) 
{
      AddNsReport(c_item);
};


void CBioseq_N_RUNS_14 :: TestOnObj(const CBioseq& bioseq)
{
   if (!thisTest.is_BASES_N_run) TestOnBasesN(bioseq);
};


void CBioseq_N_RUNS_14 :: GetReport(CRef <CClickableItem>& c_item)
{
     AddNsReport(c_item, false);
};


void CBioseq_ZERO_BASECOUNT :: TestOnObj(const CBioseq& bioseq)
{
   if (!thisTest.is_BASES_N_run) TestOnBasesN(bioseq);
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
     c_sub->description 
        = GetHasComment(c_sub->item_list.size(), "sequence") + "no " + it->first + "s.";
     c_item->subcategories.push_back(c_sub);
     c_item->item_list.insert(c_item->item_list.end(), it->second.begin(), it->second.end());
   }
};


void CBioseq_DISC_PERCENT_N :: TestOnObj(const CBioseq& bioseq)
{
   if (!thisTest.is_BASES_N_run) TestOnBasesN(bioseq);
};



void CBioseq_DISC_PERCENT_N :: GetReport(CRef <CClickableItem>& c_item)
{
  c_item->description = GetHasComment(c_item->item_list.size(), "sequence") + "> 5% Ns.";
};



void CBioseq_DISC_10_PERCENTN :: TestOnObj(const CBioseq& bioseq)
{
   if (!thisTest.is_BASES_N_run) TestOnBasesN(bioseq);
};


void CBioseq_DISC_10_PERCENTN :: GetReport(CRef <CClickableItem>& c_item)
{
  c_item->description = GetHasComment(c_item->item_list.size(), "sequence") + "> 10% Ns.";
};


void CBioseq_TEST_UNUSUAL_NT :: TestOnObj(const CBioseq& bioseq)
{
   if (!thisTest.is_BASES_N_run) TestOnBasesN(bioseq);
};


void CBioseq_TEST_UNUSUAL_NT :: GetReport(CRef <CClickableItem>& c_item)
{
   c_item->description 
     = GetOtherComment(c_item->item_list.size(), "sequence contains", "sequences contain")
          + " nucleotides that are not ATCG or N.";
};



void CBioseq_RNA_NO_PRODUCT :: TestOnObj(const CBioseq& bioseq)
{
  string pseudo;
  ITERATE (vector <const CSeq_feat*>, it, rna_feat) {
     CSeqFeatData :: ESubtype subtype = (*it)->GetData().GetSubtype();
     if (IsPseudoSeqFeatOrXrefGene(*it)) pseudo = "pseudo";
     else pseudo = "not pseudo";
     if (subtype == CSeqFeatData :: eSubtype_otherRNA) {
        if ((*it)->CanGetComment()) {
          strtmp = (*it)->GetComment();
          if ((NStr::FindNoCase(strtmp.substr(0, 9), "contains ") != string::npos)
              || (NStr::FindNoCase(strtmp.substr(0, 11), "may contain") != string::npos))
             continue;
        }
     }
     else if (subtype == CSeqFeatData :: eSubtype_tmRNA) continue;
     
     if (GetRNAProductString(**it).empty())
        thisInfo.test_item_list[GetName()].push_back(pseudo + "$" +GetDiscItemText(**it));
  }
};


void CBioseq_RNA_NO_PRODUCT :: GetReport(CRef <CClickableItem>& c_item)
{
  Str2Strs sub_ls;
  vector <string> rep_arr;
  string setting_name = GetName();
  ITERATE (vector <string>, it, thisInfo.test_item_list[setting_name]) {
    rep_arr = NStr::Tokenize(*it, "$", rep_arr);
    sub_ls[rep_arr[0]].push_back(rep_arr[1]); 
    rep_arr.clear();
  }

  unsigned cnt;
  if (sub_ls.size() == 1) {
    c_item->item_list.clear();
    c_item->item_list = sub_ls.begin()->second;
    cnt = c_item->item_list.size();
    c_item->description = GetHasComment(cnt, "RNA feature") 
           + "no product and " + ( (cnt>1) ? "are " : "is ") + sub_ls.begin()->first + ".";
  }
  else {
    ITERATE (Str2Strs, it, sub_ls) {
      CRef <CClickableItem> c_sub (new CClickableItem);
      c_sub->setting_name = setting_name;
      c_sub->item_list = it->second;
      cnt = c_sub->item_list.size();
      c_sub->description = GetHasComment(cnt, "RNA feature") 
             + "no product and " + ( (cnt >1) ? "are " : "is ") + it->first + "."; 
      c_item->subcategories.push_back(c_sub);
    }
  }
};



void CBioseq_EC_NUMBER_ON_HYPOTHETICAL_PROTEIN :: TestOnObj(const CBioseq& bioseq)
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



void CBioseq_EC_NUMBER_ON_HYPOTHETICAL_PROTEIN :: GetReport(CRef <CClickableItem>& c_item)
{
  c_item->description 
     = GetHasComment(c_item->item_list.size(), "protein feature")
       + "an EC number and a protein name of 'unknown protein' or 'hypothetical protein'";
};



void CBioseq_JOINED_FEATURES :: TestOnObj(const CBioseq& bioseq)
{
  bool excpt;
  string excpt_txt(kEmptyStr);
  string desc;
  if (IsEukaryotic(bioseq)) return;
  CBioseq_Handle bioseq_h = thisInfo.scope->GetBioseqHandle(bioseq);
  for (CFeat_CI it(bioseq_h); it; ++it) {    // use all_feat instead
    if (it->GetLocation().IsMix() || it->GetLocation().IsPacked_int()) {
       const CSeq_feat& seq_feat = it->GetOriginalFeature();
       if (seq_feat.CanGetExcept()) {
           if (seq_feat.GetExcept()) excpt = true;
           else excpt = false;
           if (seq_feat.CanGetExcept_text()) excpt_txt = seq_feat.GetExcept_text();
           excpt_txt = excpt_txt.empty()? "blank" : excpt_txt; 
       }
       else excpt = false;     
       desc = GetDiscItemText(seq_feat);
       if (excpt) JOINED_FEATURES_sfs[excpt_txt].push_back(desc);
       else JOINED_FEATURES_sfs["no"].push_back(desc);
       thisInfo.test_item_list[GetName()].push_back(desc);
    }
  }
};



void CBioseq_JOINED_FEATURES :: GetReport(CRef <CClickableItem>& c_item)
{
   string cmt_txt;
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
};




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


bool CBioseq_FEATURE_LOCATION_CONFLICT :: StrandOk(const int& strand1, const int& strand2)
{
  if (strand1 == (int)eNa_strand_minus && strand2 != (int)eNa_strand_minus)
       return false;
  else if (strand1 != (int)eNa_strand_minus && strand2 == eNa_strand_minus) return false;
  else return true;
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

    if (!StrandOk(gene_strand, feat_strand)) return false;
    
    gene_start = g_loc_it.GetRange().GetFrom(); 
    gene_stop = g_loc_it.GetRange().GetTo();
    feat_start = f_loc_it.GetRange().GetFrom();
    feat_stop = f_loc_it.GetRange().GetTo();

    if (gene_strand ==  eNa_strand_minus) {
      if (gene_stop != feat_stop) return false;
      while ( (gene_start != feat_start) && f_loc_it) {
        while (++f_loc_it) {
          feat_strand = f_loc_it.GetStrand();
          if (!StrandOk (gene_strand, feat_strand)) return false;
          feat_start = f_loc_it.GetRange().GetFrom();
          if (feat_start < gene_start) return false;
          else if (feat_start == gene_start) break;
        }
      }
    }
    else {
      if (gene_start != feat_start) return false;
      while (gene_stop != feat_stop && f_loc_it) {
        while (++f_loc_it) {
          feat_strand = f_loc_it.GetStrand();
          if (!StrandOk (gene_strand, feat_strand)) return false;
          feat_stop = f_loc_it.GetRange().GetTo();
          if (feat_stop > gene_stop) return false;
          else if (feat_stop == gene_stop)  break;
        }
      }
    }
    if (!f_loc_it) return false;
    ++g_loc_it;
    ++f_loc_it;
  }

  if (g_loc_it || f_loc_it) return false;
  else return true;
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
                || (feat_strand != eNa_strand_minus && gene_strand == eNa_strand_minus))
         return false;
    else if (gene_left == feat_left && gene_right == feat_right) return true; 
    else if ((gene_strand == eNa_strand_minus && gene_left == feat_left)
                    || (gene_strand != eNa_strand_minus && gene_right == feat_right)) {
       ITERATE (vector <const CSeq_feat*>, it, rbs_feat) {
         const CSeq_loc& rbs_loc = (*it)->GetLocation();
         ENa_strand rbs_strand = rbs_loc.GetStrand();
         int rbs_left = rbs_loc.GetStart(eExtreme_Biological);
         int rbs_right = rbs_loc.GetStop(eExtreme_Biological);
         if (rbs_strand != gene_strand) continue;
         if (rbs_strand == eNa_strand_minus) {
            if (rbs_right == gene_right && rbs_left >= feat_right) return true;
         }
         else if (rbs_left == gene_left && rbs_right <= feat_left) return true;
       } 
    }
    else return false;
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
   if (g1_maploc != g2_maploc) return false;

   string g1_locus_tag = gene1.CanGetLocus_tag() ? gene1.GetLocus_tag() : kEmptyStr;
   string g2_locus_tag = gene2.CanGetLocus_tag() ? gene2.GetLocus_tag() : kEmptyStr;
   if (g1_locus_tag != g2_locus_tag) return false;

   unsigned i;
   bool g1_getdb = gene1.CanGetDb() ? true : false;
   bool g2_getdb = gene1.CanGetDb() ? true : false;
   if (g1_getdb && g2_getdb) {
      const vector <CRef <CDbtag> >& g1_db = gene1.GetDb(), g2_db = gene2.GetDb();
      if (g1_db.size() != g2_db.size()) return false;
      for (i=0; i< g1_db.size(); i++) {
        if (g1_db[i]->GetDb() != g2_db[i]->GetDb()) return false;
      }
      
   }
   else if ( g1_getdb || g2_getdb ) return false; 
 
   bool g1_getsyn = gene1.CanGetSyn() ? true : false;
   bool g2_getsyn = gene2.CanGetSyn() ? true : false;
   if (g1_getsyn && g2_getsyn) {
      const list <string>& g1_syn = gene1.GetSyn(), g2_syn = gene2.GetSyn();
      if (g1_syn.size() != g2_syn.size()) return false;
      list <string> ::const_iterator it, jt;
      for (it=g1_syn.begin(), jt = g2_syn.begin(); it != g1_syn.end(); it++, jt++) 
        if (*it != *jt) return false;
   }
   else if (g1_getsyn || g2_getsyn) return false;

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
             if (IsGeneLocationOk(*it, *jt)) found_match = true;
             else if ( (*it)->GetData().GetSubtype()) {
                thisInfo.test_item_list[GetName()].push_back(
                                                      feat_type +"$"+GetDiscItemText(**it));
                thisInfo.test_item_list[GetName()].push_back( GetDiscItemText(**jt));
             }
          }
        }
        if (!found_match)
            thisInfo.test_item_list[GetName()].push_back(
                                          feat_type + "_missing$" + GetDiscItemText(**it));
     }
     else {
         CConstRef <CSeq_feat> gene_olp= GetBestOverlappingFeat(feat_loc,
                                                             CSeqFeatData::e_Gene,
                                                             eOverlap_Contained,
                                                             *thisInfo.scope);

//cerr << "start stop " << feat_loc.GetStart(eExtreme_Biological) << "  " 
  //<< feat_loc.GetStop(eExtreme_Biological) <<  endl;
         if (gene_olp.NotEmpty() && !IsGeneLocationOk(*it, gene_olp.GetPointer())) {
            thisInfo.test_item_list[GetName()].push_back(feat_type +"$"+GetDiscItemText(**it));
            thisInfo.test_item_list[GetName()].push_back(
                                                      GetDiscItemText(*gene_olp.GetPointer()));
         }
     }
   }
};



void CBioseq_FEATURE_LOCATION_CONFLICT :: TestOnObj(const CBioseq& bioseq)
{
  if (bioseq.IsAa()) return;
  if (!IsEukaryotic(bioseq))
           CheckFeatureTypeForLocationDiscrepancies(cd_feat, "Coding region");        
  CheckFeatureTypeForLocationDiscrepancies(rna_feat, "RNA feature");
};


void CBioseq_FEATURE_LOCATION_CONFLICT :: GetReport(CRef <CClickableItem>& c_item)
{
  vector <string> rep_arr;
  c_item->item_list.clear();
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
  c_item->description 
        = GetHasComment(cnt, "feature") + "inconsistent gene locations";
};




bool CBioseq_LOCUS_TAGS :: IsLocationDirSub(const CSeq_loc& seq_location)  // not actually used
{   
   const CSeq_id* seq_id  = seq_location.GetId();
   if (!seq_id) return false;
   if (seq_id->Which() == CSeq_id::e_Other) return false;
   CBioseq_Handle bioseq_h = thisInfo.scope->GetBioseqHandle(*seq_id);
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
             (mol.GetCompleteness() == CMolInfo::eCompleteness_complete) ? true : is_complete;
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


void CBioseq_LOCUS_TAGS :: TestOnObj(const CBioseq& bioseq) 
{
  string locus_tag, locus_tag_adj;
  string test_desc, test_desc_adj;
  ITERATE (vector <const CSeq_feat*>, jt, gene_feat) {
     const CGene_ref& gene = (*jt)->GetData().GetGene();
     if (gene.GetPseudo()) continue;  
     test_desc = GetDiscItemText(**jt);
     if (gene.CanGetLocus_tag()) {
        locus_tag = gene.GetLocus_tag();
        if (!locus_tag.empty()) {
           thisInfo.test_item_list[GetName()].push_back( locus_tag + "$" + test_desc);
           // if adjacent gene has the same locus;
           if ((jt+1) != gene_feat.end()) {
             const CGene_ref& gene_adj = (*(jt+1))->GetData().GetGene();
             if (gene_adj.CanGetLocus_tag() && !gene_adj.GetPseudo() 
                                                        && !gene_adj.GetLocus_tag().empty() ) {
                locus_tag_adj = gene_adj.GetLocus_tag();
                if (locus_tag_adj == locus_tag 
                   && ( (*jt)->GetLocation().GetStop(eExtreme_Positional) 
                                   < (*(jt+1))->GetLocation().GetStart(eExtreme_Positional))){
                   test_desc_adj = GetDiscItemText( **(jt+1) );
                   if (DUP_LOCUS_TAGS_adj_genes.find(test_desc) 
                                                           == DUP_LOCUS_TAGS_adj_genes.end())
                       DUP_LOCUS_TAGS_adj_genes.insert(test_desc);
                   if (DUP_LOCUS_TAGS_adj_genes.find(test_desc_adj)
                                                            ==DUP_LOCUS_TAGS_adj_genes.end())
                       DUP_LOCUS_TAGS_adj_genes.insert(test_desc_adj);
                }
             }
           }
        }
        else thisInfo.test_item_list[GetName_missing()].push_back(test_desc);
     }
     else thisInfo.test_item_list[GetName_missing()].push_back(test_desc);
  }
};


void CBioseq_LOCUS_TAGS :: GetReport(CRef <CClickableItem>& c_item)
{
   bool used_citem1 = false;
   // MISSING_LOCUS_TAGS
   if (thisInfo.test_item_list.find(GetName_missing()) != thisInfo.test_item_list.end()) {
     c_item->setting_name = GetName_missing();
     c_item->item_list = thisInfo.test_item_list[GetName_missing()];
     c_item->description = GetHasComment(c_item->item_list.size(), "gene") + "no locus tags.";
     thisInfo.disc_report_data.push_back(c_item);
     used_citem1 = true;
   }

// DUPLICATE_LOCUS_TAGS && Dup_prefix && bad_tag_feats;
   Str2Strs tag2dups, prefix2dups;
   vector <string> dt_arr, bad_tag_feats;
   size_t pos;
   unsigned i;
   bool bad_tag;
   dt_arr.clear();
   ITERATE (vector <string>, it, thisInfo.test_item_list[GetName()]) {
     bad_tag = false;
     dt_arr = NStr::Tokenize(*it, "$", dt_arr);
     tag2dups[dt_arr[0]].push_back(dt_arr[1]);
     if ( (pos = dt_arr[0].find("_")) != string::npos)
          prefix2dups[dt_arr[0].substr(0, pos)].push_back(dt_arr[1]);
     else bad_tag = true;
     if (!isalpha(dt_arr[0][0])) bad_tag = true;
     else {
        dt_arr[0] = dt_arr[0].substr(0, pos) + dt_arr[0].substr(pos+1);
        for (i=0; (i< dt_arr[0].size()) && !bad_tag; i++)
             if ( !isalnum(dt_arr[0][i]) ) bad_tag = true;
     }
     if (bad_tag) bad_tag_feats.push_back(dt_arr[1]);
     dt_arr.clear();
   }

// DUPLICATE_LOCUS_TAGS
   unsigned dup_cnt = 0;
   string dup_setting_name(GetName_dup());
   ITERATE (Str2Strs, it, tag2dups) {
     if (it->second.size() > 1) {
       CRef <CClickableItem> c_sub (new CClickableItem);
       c_sub->item_list = it->second;
       c_sub->setting_name = dup_setting_name;
       c_sub->description
              = GetHasComment(c_sub->item_list.size(), "gene") + "locus tag " + it->first;
       if (thisInfo.report == "Discrepancy") thisInfo.disc_report_data.push_back(c_sub);
       else {
          if (used_citem1) c_item = CRef <CClickableItem> (new CClickableItem);
          c_item->setting_name = dup_setting_name;
          c_item->subcategories.push_back(c_sub);
       }
       dup_cnt += it->second.size();
     }
   }

   if (!DUP_LOCUS_TAGS_adj_genes.empty()) {
       CRef <CClickableItem> c_sub (new CClickableItem);
       c_sub->setting_name = dup_setting_name;
       c_sub->item_list.insert(c_sub->item_list.end(), 
                            DUP_LOCUS_TAGS_adj_genes.begin(), DUP_LOCUS_TAGS_adj_genes.end());
       c_sub->description = GetIsComment(c_sub->item_list.size(), "gene") 
                               + "adjacent to another gene with the same locus tag.";
       if (thisInfo.report == "Discrepancy") thisInfo.disc_report_data.push_back(c_sub);
       else {
            c_item->subcategories.push_back(c_sub);
            if (used_citem1) thisInfo.disc_report_data.push_back(c_item);
            used_citem1 = true;
       }
   }

// inconsistent locus tags: 
   ITERATE (Str2Strs, it, prefix2dups) {
      GetProperCItem(c_item, &used_citem1);
      c_item->setting_name = GetName_incons();
      c_item->item_list = it->second;
      c_item->description 
          = GetHasComment(c_item->item_list.size(),"feature") +"locus tag prefix " + it->first;
   } 

// bad formats: BAD_LOCUS_TAG_FORMAT
   if (!bad_tag_feats.empty()) {
     GetProperCItem(c_item, &used_citem1);
     c_item->setting_name = GetName_badtag();
     c_item->item_list = bad_tag_feats;
     c_item->description 
           = GetIsComment(c_item->item_list.size(), "locus tag") + "incorrectly formatted.";
   }
};


void CBioseq_DISC_FEATURE_COUNT :: TestOnObj(const CBioseq& bioseq)
{
   CBioseq_Handle bioseq_hl = thisInfo.scope->GetBioseqHandle(bioseq);
   SAnnotSelector sel;
   sel.ExcludeFeatSubtype(CSeqFeatData::eSubtype_prot);
   
   string bioseq_text = GetDiscItemText(bioseq);
   CRef <GeneralDiscSubDt> bioseq_feat_count (new GeneralDiscSubDt(kEmptyStr, bioseq_text));

   Str2Int :: iterator it;
   for (CFeat_CI feat_it(bioseq_hl, sel); feat_it; ++feat_it) {  // use non_prot_feat instead
      const CSeqFeatData& seq_feat_dt = (feat_it->GetOriginalFeature()).GetData();
      strtmp = seq_feat_dt.GetKey(CSeqFeatData::eVocabulary_genbank);
      it = bioseq_feat_count->feat_count_list.find(strtmp);
      if (it == bioseq_feat_count->feat_count_list.end())  
                bioseq_feat_count->feat_count_list[strtmp] = 1;
      else it->second ++;
   }
 
   if ( !(bioseq_feat_count->feat_count_list.empty()) ) {
     if (bioseq.IsAa()) bioseq_feat_count->isAa = true;
     else bioseq_feat_count->isAa = false;
     DISC_FEATURE_COUNT_list.push_back(bioseq_feat_count);
     if (thisInfo.test_item_list.find(GetName()) == thisInfo.test_item_list.end())
           thisInfo.test_item_list[GetName()].push_back("Feaure Counts"); 
   }
   else {
      if (bioseq.IsAa()) DISC_FEATURE_COUNT_protfeat_prot_list.push_back(bioseq_text);
      else DISC_FEATURE_COUNT_protfeat_nul_list.push_back(bioseq_text);
   }
};


void CBioseq_DISC_FEATURE_COUNT :: GetReport(CRef <CClickableItem>& c_item)
{
   Str2Int feat_bioseqcnt;

   // get all feattype
   ITERATE (vector <CRef <GeneralDiscSubDt> >, it, DISC_FEATURE_COUNT_list) {
      ITERATE (Str2Int, jt, (*it)->feat_count_list) {
         if (feat_bioseqcnt.find( jt->first ) == feat_bioseqcnt.end()) 
                  feat_bioseqcnt[jt->first] = 1;
         else feat_bioseqcnt[jt->first]++;
      }    
   }

   // missing feature
   NON_CONST_ITERATE (vector <CRef <GeneralDiscSubDt> >, it, DISC_FEATURE_COUNT_list) {
     ITERATE (Str2Int, jt, feat_bioseqcnt) {
       if ( (*it)->feat_count_list.find(jt->first) == (*it)->feat_count_list.end()) {
         if ( (jt->first == CSeqFeatData::SelectionName(CSeqFeatData::e_Prot)
                 && (*it)->isAa)
              || (jt->first != CSeqFeatData::SelectionName(CSeqFeatData::e_Prot)
                    && !((*it)->isAa)))
          (*it)->feat_count_list[jt->first] = 0;   
       }
     }
   }

   // subcategories
   unsigned current_num = 0, i, j;
   string current_feat(kEmptyStr);
   typedef map <int, vector <string>, std::greater<int> > Int2Strs;
   Int2Strs featnum_bioseqs;
   unsigned total_feat;
   Str2Int :: iterator jt;
   Int2Strs :: iterator jjt;
   for (i=0; i< DISC_FEATURE_COUNT_list.size(); i++) {
     ITERATE (Str2Int, it, DISC_FEATURE_COUNT_list[i]->feat_count_list) {
        if (it->second >= 0) {
           featnum_bioseqs.clear();
           current_feat = it->first;
           total_feat = current_num = it->second;
           featnum_bioseqs[current_num].push_back(DISC_FEATURE_COUNT_list[i]->str);//bioseq

           for (j = i+1; j < DISC_FEATURE_COUNT_list.size(); j++) {
              jt = DISC_FEATURE_COUNT_list[j]->feat_count_list.find(current_feat);
              if (jt != (DISC_FEATURE_COUNT_list[j]->feat_count_list).end()) {
                jjt = featnum_bioseqs.find(jt->second);
                if (jjt != featnum_bioseqs.end())
                      jjt->second.push_back(DISC_FEATURE_COUNT_list[j]->str); 
                else 
                 featnum_bioseqs[jt->second].push_back(DISC_FEATURE_COUNT_list[j]->str);
                total_feat += jt->second;
                if (jt->second >0) jt->second *= -1;
                else jt->second = -1;
              } 
           } 

           if (current_feat == CSeqFeatData::SelectionName(CSeqFeatData::e_Prot))
               ITERATE (vector <string>, p_it, DISC_FEATURE_COUNT_protfeat_prot_list)
                 featnum_bioseqs[0].push_back(*p_it);
           else ITERATE (vector <string>, p_it, DISC_FEATURE_COUNT_protfeat_nul_list)
                 featnum_bioseqs[0].push_back(*p_it);

           CRef <CClickableItem> c_type_sub (new CClickableItem);  // type_list
           c_type_sub->description = current_feat + ": " + NStr::UIntToString(total_feat)
                                 + ((total_feat > 1)? " present" : " presents")
                                 + ((featnum_bioseqs.size() > 1)? " (inconsistent)" : kEmptyStr);
           c_type_sub->setting_name = GetName();

           ITERATE (Int2Strs, kt, featnum_bioseqs) {
              CRef <CClickableItem> c_sub (new CClickableItem);
              c_sub->setting_name = GetName();
              c_sub->item_list = kt->second;
              c_sub->description = GetHasComment(c_sub->item_list.size(), "bioseq")
                                     + NStr::IntToString(kt->first) + " " + current_feat
                                     + ( (kt->first > 1)? " features" : " feature");
              c_type_sub->subcategories.push_back(c_sub);
           } 
  
           c_item->subcategories.push_back(c_type_sub);
        }
     }
    
   } 
   c_item->description = "Feature Counts";
   c_item->item_list.clear();

   DISC_FEATURE_COUNT_protfeat_prot_list.clear();
   DISC_FEATURE_COUNT_protfeat_nul_list.clear();
   DISC_FEATURE_COUNT_list.clear();
};


void CSeqEntryTestAndRepData :: TestOnBiosrc(const CSeq_entry& seq_entry)
{
   string desc;
   ITERATE (vector <const CSeq_feat*>, it, biosrc_feat) {
     const CBioSource& biosrc = (*it)->GetData().GetBiosrc();
     desc = GetDiscItemText(**it);
     if (CSeqEntry_ONCALLER_MULTISRC :: HasMulSrc( biosrc ))
        thisInfo.test_item_list[GetName_multi()].push_back(desc);
     else if (IsBacterialIsolate(biosrc))
               thisInfo.test_item_list[GetName_iso()].push_back(desc); 
   };

   unsigned i=0;
   ITERATE (vector <const CSeqdesc*>, it, biosrc_seqdesc) {
     const CBioSource& biosrc = (*it)->GetSource();
     desc = GetDiscItemText( **it, *(biosrc_seqdesc_seqentry[i]));
     if (CSeqEntry_ONCALLER_MULTISRC :: HasMulSrc( biosrc ))
        thisInfo.test_item_list[GetName_multi()].push_back(desc);
     else 
       if (IsBacterialIsolate(biosrc)) 
                 thisInfo.test_item_list[GetName_iso()].push_back(desc);
     i++;
   };

  thisTest.is_BIOSRC_run = true;
};


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



void CSeqEntry_DISC_BACTERIA_SHOULD_NOT_HAVE_ISOLATE :: TestOnObj(const CSeq_entry& seq_entry)
{
   if (!thisTest.is_BIOSRC_run) TestOnBiosrc(seq_entry);
};


void CSeqEntry_DISC_BACTERIA_SHOULD_NOT_HAVE_ISOLATE :: GetReport(CRef <CClickableItem>& c_item)
{
   c_item->description = GetHasComment(c_item->item_list.size(), "bacterial biosrouce")
                           + "isolate.";
};



bool CSeqEntry_ONCALLER_MULTISRC :: HasMulSrc(const CBioSource& biosrc)
{
  if (biosrc.IsSetOrgMod()) {
    ITERATE (list <CRef <COrgMod> >, it, biosrc.GetOrgname().GetMod()) {
      const int subtype = (*it)->GetSubtype();
      const string& subname = (*it)->GetSubname();
      if ( (subtype == COrgMod :: eSubtype_strain || subtype == COrgMod :: eSubtype_isolate)
                     && ( subname.find(",") != string::npos || subname.find(";") != string::npos) )
         return true;
    }
    return false;
  }
  else return false;
};


void CSeqEntry_ONCALLER_MULTISRC :: TestOnObj(const CSeq_entry& seq_entry)
{
   if (!thisTest.is_BIOSRC_run) TestOnBiosrc(seq_entry);
};


void CSeqEntry_ONCALLER_MULTISRC :: GetReport(CRef <CClickableItem>& c_item) 
{
   c_item->description = GetHasComment(c_item->item_list.size(), "organism") 
                                      + "commma or semicolon in strain or isolate.";
};


bool CSeqEntry_INCONSISTENT_BIOSOURCE :: SynonymsMatch(const COrg_ref& org1, const COrg_ref& org2)
{
     bool has_syn1 = org1.CanGetSyn(), has_syn2 = org2.CanGetSyn();

     if ( !has_syn1 && !has_syn2 ) return true;
     if ( has_syn1 && has_syn2 ) {
        list <string>& syn1 = const_cast<COrg_ref&>(org1).SetSyn();
        list <string>& syn2 = const_cast<COrg_ref&>(org2).SetSyn();

        if (syn1.size() != syn2.size()) return false;

        syn1.sort();
        syn2.sort();

        list <string> :: const_iterator it, jt;
        for (it = syn1.begin(), jt = syn2.begin(); it != syn1.end(); it++, jt++) {
             if ( (*it) != (*jt)) return false;
        }
        return true;
     }
     else return false;

};  // SynonymsMatch



bool CSeqEntry_INCONSISTENT_BIOSOURCE :: DbtagMatch(const COrg_ref& org1, const COrg_ref& org2)
{
    bool has_db1 = org1.CanGetDb(), has_db2 = org2.CanGetDb();

    if ( !has_db1 && !has_db2) return true;
    else if ( has_db1 && has_db2)  {
      const vector < CRef < CDbtag > >& db1 = org1.GetDb();
      const vector < CRef < CDbtag > >& db2 = org2.GetDb();

      if (db1.size() != db2.size()) return false;
      for (unsigned i=0; i< db1.size(); i++) {   // already sorted
           if ( !db1[i]->Match(*db2[i]) ) return false;
      };
      return true;
    }
    else return false;
}; // DbtagMatch



bool CSeqEntry_INCONSISTENT_BIOSOURCE :: OrgModSetMatch(const COrgName& nm1, const COrgName& nm2) 
{
  bool has_mod1 = nm1.CanGetMod(), has_mod2 = nm2.CanGetMod();

  if (!has_mod1 && !has_mod2) return true;
  if (has_mod1 && has_mod2) {
     const list < CRef <COrgMod> >& mod1= nm1.GetMod();
     const list < CRef <COrgMod> >& mod2 = nm2.GetMod();

     if (mod1.size() != mod2.size()) return false;
     list < CRef < COrgMod > > ::const_iterator it, jt;
     // already sorted
     for (it = mod1.begin(), jt = mod2.begin(); it != mod1.end(); it++, jt++) {
       string att_i = (*it)->CanGetAttrib()? (*it)->GetAttrib() : kEmptyStr;
       string att_j = (*jt)->CanGetAttrib()? (*jt)->GetAttrib(): kEmptyStr;
       if ( (*it)->GetSubtype() != (*jt)->GetSubtype()
                  || (*it)->GetSubname() != (*jt)->GetSubname() || att_i != att_j)
            return false;
     }
     return true;
  }
  else return false;
};  // OrgModSetMatch



bool CSeqEntry_INCONSISTENT_BIOSOURCE :: OrgNameMatch(const COrg_ref& org1, const COrg_ref& org2)
{
   bool has_orgnm1 = org1.CanGetOrgname();
   bool has_orgnm2 = org2.CanGetOrgname();
   
   if (!has_orgnm1 && !has_orgnm2) return true;
   else if (has_orgnm1 && has_orgnm2) {
     const COrgName& nm1 = org1.GetOrgname();
     const COrgName& nm2 = org2.GetOrgname();

     COrgName::C_Name::E_Choice choc1 = nm1.CanGetName()? 
                                          nm1.GetName().Which(): COrgName::C_Name::e_not_set;
     COrgName::C_Name::E_Choice choc2 = nm2.CanGetName()? 
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

     if (choc1 != choc2 || gcode1 != gcode2 || mgcode1 != mgcode2 || att1 != att2 
               || lin1 != lin2 || div1 != div2 || !OrgModSetMatch(nm1, nm2)) 
           return false;
     else return true;
   }
   else return false;
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
  bool has_subtype1 = biosrc1.CanGetSubtype();
  bool has_subtype2 = biosrc2.CanGetSubtype();

  if (!has_subtype1 && !has_subtype2) return true;
  else if (has_subtype1 && has_subtype2) {
    const list < CRef < CSubSource > >& subsrc1 = biosrc1.GetSubtype();
    const list < CRef < CSubSource > >& subsrc2 = biosrc2.GetSubtype();

    if (subsrc1.size() != subsrc2.size()) return false;
    list < CRef < CSubSource > >::const_iterator it, jt;
    // already sorted
    for (it = subsrc1.begin(), jt = subsrc2.begin(); it != subsrc1.end(); it++, jt++) { 
         if ( (*it)->GetSubtype() != (*jt)->GetSubtype() 
              || (*it)->GetName() != (*jt)->GetName()  
              || ( (*it)->CanGetAttrib()? (*it)->GetAttrib(): kEmptyStr )
                   != ( (*jt)->CanGetAttrib()? (*jt)->GetAttrib(): kEmptyStr ) ) {
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
          || !SubSourceSetMatch(biosrc1, biosrc2) )
      return false;

  int genome1 = biosrc1.GetGenome();
  int genome2 = biosrc2.GetGenome();
  if (genome1 == genome2) return true;
  if ( (genome1 = CBioSource::eGenome_unknown && genome2 == CBioSource::eGenome_genomic)
        || (genome1 == CBioSource::eGenome_genomic && genome2 == CBioSource::eGenome_unknown) )
       return true;
 
  else return false;
};



string CSeqEntry_INCONSISTENT_BIOSOURCE :: OrgModSetDifferences(const COrgName& nm1, const COrgName& nm2) 
{
  bool has_mod1 = nm1.CanGetMod(), has_mod2 = nm2.CanGetMod();
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

   if (!has_orgnm1 && !has_orgnm2) return(kEmptyStr);
   else if (has_orgnm1 && has_orgnm2) {
     string org_nm_diff_str(kEmptyStr);
     const COrgName& nm1 = org1.GetOrgname();
     const COrgName& nm2 = org2.GetOrgname();

     COrgName::C_Name::E_Choice choc1 = nm1.CanGetName()?
                                          nm1.GetName().Which(): COrgName::C_Name :: e_not_set;
     COrgName::C_Name::E_Choice choc2 = nm2.CanGetName()?
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

     if (choc1 != choc2) org_nm_diff_str = "orgname choices differ, ";
     if (gcode1 != gcode2) org_nm_diff_str += "genetic codes differ, ";
     if (mgcode1 != mgcode2) org_nm_diff_str += "mitochondrial genetic codes differ, ";
     if (att1 != att2) org_nm_diff_str += "attributes differ, ";
     if (lin1 != lin2) org_nm_diff_str += "lineages differ, ";
     if (div1 != div2) org_nm_diff_str += "divisions differ, ";
     string tmp = OrgModSetDifferences(nm1, nm2);
     if (!tmp.empty()) org_nm_diff_str += tmp + ", ";
     org_nm_diff_str = org_nm_diff_str.substr(0, org_nm_diff_str.size()-2);
     return (org_nm_diff_str); 
   }
   else return ("One Orgname is missing, ");
}



string CSeqEntry_INCONSISTENT_BIOSOURCE :: DescribeOrgRefDifferences(const COrg_ref& org1, const COrg_ref& org2)
{
  string org_ref_diff_str(kEmptyStr);

  string tax1 = org1.CanGetTaxname()? org1.GetTaxname() : kEmptyStr;
  string tax2 = org2.CanGetTaxname()? org2.GetTaxname() : kEmptyStr;
  string comm1 = org1.CanGetCommon()? org1.GetCommon() : kEmptyStr;
  string comm2 = org2.CanGetCommon()? org2.GetCommon() : kEmptyStr;

  if ( tax1 != tax2 ) org_ref_diff_str = "taxnames differ, ";
  if ( comm1 != comm2 ) org_ref_diff_str += "common names differ, ";
  if ( !SynonymsMatch(org1, org2) ) org_ref_diff_str += "synonyms differ, ";
  if ( !DbtagMatch(org1, org2)) org_ref_diff_str += "dbxrefs differ, ";
  string tmp = DescribeOrgNameDifferences(org1, org2);
  if (!tmp.empty()) org_ref_diff_str += tmp + ", ";
  org_ref_diff_str = org_ref_diff_str.substr(0, org_ref_diff_str.size()-2);
  return (org_ref_diff_str);
}



string CSeqEntry_INCONSISTENT_BIOSOURCE :: DescribeBioSourceDifferences(const CBioSource& biosrc1, const CBioSource& biosrc2)
{
    string diff_str(kEmptyStr);
    if (biosrc1.GetOrigin() != biosrc2.GetOrigin()) diff_str = "origins differ, ";
    if (biosrc1.CanGetIs_focus() != biosrc2.CanGetIs_focus()) diff_str += "focus differs, ";
    int genome1 = biosrc1.GetGenome();
    int genome2 = biosrc2.GetGenome();
    if (genome1 != genome2
        && !(genome1 = CBioSource::eGenome_unknown && genome2 == CBioSource::eGenome_genomic)
        && !(genome1 == CBioSource::eGenome_genomic && genome2 ==CBioSource::eGenome_unknown) )
       diff_str += "locations differ, ";
    if (!SubSourceSetMatch(biosrc1, biosrc2)) diff_str += "subsource qualifiers differ, ";
    string tmp = DescribeOrgRefDifferences (biosrc1.GetOrg(), biosrc2.GetOrg());
    if (!tmp.empty()) diff_str += tmp + ", ";
    diff_str = diff_str.substr(0, diff_str.size()-2);
    return (diff_str);
};




void CBioseqTestAndRepData :: TestProteinID(const CBioseq& bioseq)
{
   bool has_pid;
   ITERATE (list <CRef <CSeq_id> >, it, bioseq.GetId()) {
     if ((*it)->IsGeneral()) {
       const CDbtag& dbtag = (*it)->GetGeneral();
       if (!dbtag.IsSkippable()) {
          has_pid = true;
          strtmp = dbtag.GetDb();
          if (!strtmp.empty())
                thisInfo.test_item_list[GetName_prefix()].push_back(
                                             dbtag.GetDb() + "$" + GetDiscItemText(bioseq));
          break;
       }
       else has_pid = false; 
     }
   }

   if (!has_pid) 
       thisInfo.test_item_list[GetName_pid()].push_back(GetDiscItemText(bioseq));
};


void CBioseqTestAndRepData :: TestOnAnnotation(const CBioseq& bioseq)
{
  if (all_feat.empty()) {
    string desc = GetDiscItemText(bioseq);
    thisInfo.test_item_list[GetName_no()].push_back(desc);
    if (bioseq.IsNa() && bioseq.GetLength() >= 5000)
      thisInfo.test_item_list[GetName_long_na()].push_back(desc); 
  }
  thisTest.is_ANNOT_run = true;
};


void CBioseq_NO_ANNOTATION :: TestOnObj(const CBioseq& bioseq)
{
  if (!thisTest.is_ANNOT_run) TestOnAnnotation(bioseq);
} ;


void CBioseq_NO_ANNOTATION :: GetReport(CRef <CClickableItem>& c_item)
{
   c_item->description = GetHasComment(c_item->item_list.size(), "bioseq") + "no features";
};


void CBioseq_DISC_LONG_NO_ANNOTATION :: TestOnObj(const CBioseq& bioseq) 
{
  if (!thisTest.is_ANNOT_run) TestOnAnnotation(bioseq);
};


void CBioseq_DISC_LONG_NO_ANNOTATION :: GetReport(CRef <CClickableItem>& c_item)
{
  unsigned len = c_item->item_list.size();
  c_item->description = GetIsComment(len, "bioseq") + "longer than 5000nt and " 
                               + ( (len == 1)? "has" : "have") + " no features.";
};



void CBioseq_MISSING_PROTEIN_ID :: TestOnObj(const CBioseq& bioseq)
{
   if (thisInfo.test_item_list.find(GetName_prefix()) == thisInfo.test_item_list.end())
     TestProteinID(bioseq);
};


void CBioseq_MISSING_PROTEIN_ID :: GetReport(CRef <CClickableItem>& c_item)
{
  unsigned cnt = c_item->item_list.size();
  c_item->description = GetHasComment(cnt, "protein") 
                         + "invalid " + ( (cnt > 1)? "IDs." : "ID." ); 
};


//not tested yet
void CBioseq_INCONSISTENT_PROTEIN_ID_PREFIX :: TestOnObj(const CBioseq& bioseq)
{
   if (thisInfo.test_item_list.find(GetName_pid()) == thisInfo.test_item_list.end())
     TestProteinID(bioseq);
};


void CBioseq_INCONSISTENT_PROTEIN_ID_PREFIX :: MakeRep(const Str2Strs& item_map, const string& desc1, const string& desc2) 
{
   string setting_name = GetName();
   ITERATE (Str2Strs, it, item_map) {
     if (it->second.size() > 1) {
       CRef <CClickableItem> c_item (new CClickableItem);
       c_item->setting_name = setting_name;
       c_item->item_list = it->second;
       c_item->description 
          = GetHasComment(c_item->item_list.size(), desc1) + desc2 + it->first + ".";
       thisInfo.disc_report_data.push_back(c_item);
     }
   }
};


void CBioseq_INCONSISTENT_PROTEIN_ID_PREFIX :: GetReport(CRef <CClickableItem>& c_item)
{
   Str2Strs db2list, prefix2list;
   GetTestItemList(c_item->item_list, db2list);

   GetTestItemList(c_item->item_list, prefix2list);
   c_item->item_list.clear();

   // ReportInconsistentGlobalDiscrepancyStrings
   if (db2list.size() > 1) MakeRep(db2list, "sequence", "protein ID prefix ");

   // ReportInconsistentGlobalDiscrepancyPrefixes
   if (prefix2list.size() > 1) MakeRep(prefix2list, "feature", "locus tag prefix ");
};




void CBioseq_TEST_DEFLINE_PRESENT :: TestOnObj(const CBioseq& bioseq)
{
  CConstRef <CSeqdesc> seqdesc_title = bioseq.GetClosestDescriptor(CSeqdesc::e_Title);
  if (seqdesc_title.NotEmpty()) {
     thisInfo.test_item_list[GetName()].push_back(GetDiscItemText(bioseq));
     ITERATE(vector < CRef < CTestAndRepData > >, it, CRepConfig :: tests_on_Bioseq_na) {
       if ( (*it)->GetName() == GetName()) {
             CRepConfig :: tests_4_once.push_back(*it);
             break;
       }
     }
  } 
};


void CBioseq_TEST_DEFLINE_PRESENT :: GetReport(CRef <CClickableItem>& c_item)
{
   c_item->description = "Bioseq has definition line";
}



void CSeqEntry_MISSING_GENOMEASSEMBLY_COMMENTS :: TestOnObj(const CSeq_entry& seq_entry)
{
  unsigned i=0; 
  ITERATE (vector <const CSeqdesc*>, it, user_seqdesc) {
    const CUser_object& user_obj = (*it)->GetUser();
    if (user_obj.GetType().IsStr() 
         && user_obj.GetType().GetStr() == "StructuredComment"
         && user_obj.HasField("StructuredCommentPrefix")
         && user_obj.GetField("StructuredCommentPrefix").GetData().IsStr()) {
      if (user_obj.GetField("StructuredCommentPrefix").GetData().GetStr() 
                                                        != "##Genome-Assembly-Data-START##") {
           if (user_seqdesc_seqentry[i]->IsSeq() && user_seqdesc_seqentry[i]->GetSeq().IsNa())
              thisInfo.test_item_list[GetName()].push_back(GetDiscItemText(
                                                          user_seqdesc_seqentry[i]->GetSeq()));
           else AddBioseqsOfSetToReport(user_seqdesc_seqentry[i]->GetSet(), GetName(), true);
      }
    }
    i++;
  }
}; 


void CSeqEntry_MISSING_GENOMEASSEMBLY_COMMENTS :: GetReport(CRef <CClickableItem>& c_item)
{
   c_item->description = GetIsComment(c_item->item_list.size(), "bioseq")
                            + "missing GenomeAssembly structured comments.";
};



void CSeqEntry_TEST_HAS_PROJECT_ID :: GroupAllBioseqs(const CBioseq_set& bioseq_set, const int& id)
{
  string desc;
  ITERATE (list < CRef < CSeq_entry > >, it, bioseq_set.GetSeq_set()) {
     if ((*it)->IsSeq()) {
         const CBioseq& bioseq = (*it)->GetSeq();
         desc = GetDiscItemText(bioseq);
         if ( bioseq.IsAa() ) {
              PROJECT_ID_prot[id].push_back(desc);
              thisInfo.test_item_list[GetName()].push_back("prot$" + desc);
         }
         else {
              PROJECT_ID_nuc[id].push_back(desc);
              thisInfo.test_item_list[GetName()].push_back("nuc$" + desc);
         }
     }
     else GroupAllBioseqs((*it)->GetSet(), id);
  }
};



void CSeqEntry_TEST_HAS_PROJECT_ID :: TestOnObj(const CSeq_entry& seq_entry)
{
  unsigned i=0;
  int id;
  string desc;
  ITERATE (vector <const CSeqdesc*>, it, user_seqdesc) {
    const CUser_object& user_obj = (*it)->GetUser();
    if (user_obj.GetType().IsStr()
         && user_obj.GetType().GetStr() == "GenomeProjectsDB"
         && user_obj.HasField("ProjectID")
         && user_obj.GetField("ProjectID").GetData().IsInt()) {
      id = user_obj.GetField("ProjectID").GetData().GetInt();
      if (user_seqdesc_seqentry[i]->IsSeq()) {
          const CBioseq& bioseq = user_seqdesc_seqentry[i]->GetSeq();
          desc = GetDiscItemText(bioseq);
          if ( bioseq.IsAa() ) {
              PROJECT_ID_prot[id].push_back(desc);
              thisInfo.test_item_list[GetName()].push_back("prot$" + desc); 
          }
          else {
              PROJECT_ID_nuc[id].push_back(desc);
              thisInfo.test_item_list[GetName()].push_back("nuc$" + desc); 
          }
      }
      else GroupAllBioseqs(user_seqdesc_seqentry[i]->GetSet(), id);
    }     
    i++; 
  }   
}; 



void CSeqEntry_TEST_HAS_PROJECT_ID :: GetReport(CRef <CClickableItem>& c_item)
{
   unsigned prot_cnt = 0, nuc_cnt = 0, prot_id1 = 0, nuc_id1 = 0;
   Str2Strs mol2list;
   GetTestItemList(c_item->item_list, mol2list);
   c_item->item_list.clear();
   ITERATE (Str2Strs, it, mol2list) {
     if (it->first == "prot") prot_cnt = it->second.size();
     else nuc_cnt = it->second.size();
   }

   string tot_add_desc, prot_add_desc, nuc_add_desc;
   if (PROJECT_ID_prot.size() <= 1 && PROJECT_ID_nuc.size() <= 1) {
      if (prot_cnt)  prot_id1 = PROJECT_ID_prot.begin()->first;
      if (nuc_cnt) nuc_id1 = PROJECT_ID_nuc.begin()->first;
      if ( (prot_id1 == nuc_id1) ||  !(prot_id1 * nuc_id1)) tot_add_desc = "(all same)";
      else tot_add_desc = " (some different)";
   }

   if (prot_cnt && PROJECT_ID_prot.size() > 1) prot_add_desc = "(some different)";
   else prot_add_desc = "(all same)";

   if (nuc_cnt && PROJECT_ID_nuc.size() > 1) nuc_add_desc = "(some different)";
   else nuc_add_desc = "(all same)";

   if (prot_cnt && !nuc_cnt) {
       c_item->description = GetOtherComment(c_item->item_list.size(),
                                 "protein sequence has project ID ",
                                 "protein sequences have project IDs " + tot_add_desc);
       c_item->item_list = mol2list.begin()->second;
   }
   else if (!prot_cnt && nuc_cnt) {
       c_item->description = GetOtherComment(c_item->item_list.size(),
                                 "nucleotide sequence has project ID ",
                                 "nucleotide sequences have project IDs " + tot_add_desc);
       c_item->item_list = mol2list.begin()->second;
   }
   else {
       c_item->description = GetOtherComment(prot_cnt + nuc_cnt, "sequence has project ID ",
                                "sequencs have project IDs " + tot_add_desc);
       AddSubcategories(c_item, GetName(), mol2list["nuc"],
               "nucleotide sequence has project ID", "nucleotide sequences have project IDs", 
               false, e_OtherComment);
       AddSubcategories(c_item, GetName(), mol2list["prot"], "protein sequence has project ID",
                              "protein sequences have project IDs", false, e_OtherComment);

       //c_item->expanded = true;  what does the expanded work for?
   }

   PROJECT_ID_prot.clear();
};



void CBioseq_DISC_QUALITY_SCORES :: TestOnObj(const CBioseq& bioseq)
{
  if (bioseq.IsNa() && bioseq.CanGetAnnot()) {
    unsigned cnt = 0;
    ITERATE (list <CRef <CSeq_annot> >, it, bioseq.GetAnnot()) {
       if ( (*it)->GetData().IsGraph() ) { DISC_QUALITY_SCORES_graph |= 1; break; }
       cnt ++;
    }

    if (cnt == bioseq.GetAnnot().size()) DISC_QUALITY_SCORES_graph |= 2; 
  }
};


void CBioseq_DISC_QUALITY_SCORES ::  GetReport(CRef <CClickableItem>& c_item)
{
  if (DISC_QUALITY_SCORES_graph == 1) 
    c_item->description = "Quality scores are present on all sequences.";
  else if (DISC_QUALITY_SCORES_graph == 2) 
    c_item->description = "Quality scores are missing on all sequences.";
  else if (DISC_QUALITY_SCORES_graph == 3) 
    c_item->description = "Quality scores are missing on some sequences.";
};



void CBioseq_RNA_CDS_OVERLAP :: TestOnObj(const CBioseq& bioseq)
{
  string text_i, text_j, subcat_tp;
  ENa_strand strand_i, strand_j;
  ITERATE (vector <const CSeq_feat*>, it, rna_not_mrna_feat) {
    ITERATE (vector <const CSeq_feat*>, jt, cd_feat) {
      const CSeq_loc& loc_i = (*it)->GetLocation();
      const CSeq_loc& loc_j = (*jt)->GetLocation();
      sequence::ECompare ovlp_com = Compare(loc_j, loc_i, thisInfo.scope);
      subcat_tp = kEmptyStr;
      switch (ovlp_com) {
        case eSame: subcat_tp = "extrc$"; break;
        case eContained: subcat_tp = "cds_in_rna$"; break;
        case eContains: subcat_tp = "rna_in_cds$"; break;
        case eOverlap: {
           strand_i = loc_i.GetStrand();
           strand_j = loc_j.GetStrand();
           if ((strand_i == eNa_strand_minus && strand_j != eNa_strand_minus)
                       || (strand_i != eNa_strand_minus && strand_j == eNa_strand_minus))
                subcat_tp = "overlap_opp_strand$";
           else subcat_tp = "overlap_same_strand$";
           break;
        }
        default:;
      }
      if (!subcat_tp.empty()) {
          thisInfo.test_item_list[GetName()].push_back(subcat_tp + GetDiscItemText(**jt));
          thisInfo.test_item_list[GetName()].push_back(subcat_tp + GetDiscItemText(**it));
      }
    }
  }
}; //  CBioseq_RNA_CDS_OVERLAP :: TestOnObj



void CBioseq_RNA_CDS_OVERLAP :: GetReport(CRef <CClickableItem>& c_item)
{
   Str2Strs subcat2list;
   GetTestItemList(c_item->item_list, subcat2list);
   c_item->item_list.clear();

   string desc1, desc2, desc3;
   CRef <CClickableItem> ovp_subcat (new CClickableItem);
   ITERATE (Str2Strs, it, subcat2list) {
     if (it->first.find("overlap") != string::npos) {
       if (it->first == "exact") {
         desc1 = "coding region exactly matches";
         desc2 = "coding regions exactly match";
         desc3 = " an RNA location";
       }
       else if (it->first == "cds_in_rna") {
         desc1 = "coding region is";
         desc2 = "coding regions are";
         desc3 = " completely contained in RNA location";
       }
       else if (it->first == "rna_in_cds" ) {
         desc1 = "coding region completely contains";
         desc2 = "coding regions completely contain";
         desc3 = " RNAs";
       }
       AddSubcategories(c_item, GetName(), it->second, desc1, desc2, true, 
                                                              e_OtherComment, desc3, true); 
     }
     else {
       desc1 = "coding region overlaps";
       desc2 = "coding regions overlap";
       if (it->first == "overlap_opp_strand") 
             desc3 = " RNAs on the opposite strand (no containment)";
       else desc3 = " RNAs on the same strand (no containment)";
       AddSubcategories(ovp_subcat, GetName(), it->second, desc1, desc2, true, 
                                                               e_OtherComment, desc3, true); 
     } 
   }
   if (!ovp_subcat->item_list.empty()) {
      ovp_subcat->setting_name = GetName();
      ovp_subcat->description 
         = GetOtherComment(ovp_subcat->item_list.size(), "coding region exactly matches",
                             "coding regions exactly match") 
             + " an RNA location";
      c_item->item_list.insert(
         c_item->item_list.end(), ovp_subcat->item_list.begin(), ovp_subcat->item_list.end());
      c_item->subcategories.push_back(ovp_subcat);
   }
    
   c_item->description 
        = GetOtherComment(c_item->item_list.size()/2, "coding region overlapps", 
                           "coding regions overlap") 
           + " RNA features";
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
     if (seq_feat->CanGetComment()) {
         const string& comm = seq_feat->GetComment();
         if ( string::npos != NStr::FindNoCase(comm, "overlap")
              || string::npos != NStr::FindNoCase(comm, "frameshift")
              || string::npos != NStr::FindNoCase(comm, "frame shift")
              || string::npos != NStr::FindNoCase(comm, "extend") ) 
         thisInfo.test_item_list[GetName()].push_back("comment$" + desc);
     }
     else thisInfo.test_item_list[GetName()].push_back("no_comment$" + desc);
}; // AddToDiscRep



bool CBioseq_OVERLAPPING_CDS :: HasNoSuppressionWords(const CSeq_feat* seq_feat)
{
  string prod_nm;
  if (seq_feat->GetData().GetSubtype() == CSeqFeatData::eSubtype_cdregion) {
     prod_nm = GetProdNmForCD(*seq_feat);
     if ( IsWholeWord(prod_nm, "ABC")
            || string::npos != NStr::FindNoCase(prod_nm, "transposon")
            || string::npos != NStr::FindNoCase(prod_nm, "transposase"))
          return false;
     else return true;
  }
  else return false;
}; // HasNoSuppressionWords()



void CBioseq_OVERLAPPING_CDS :: TestOnObj(const CBioseq& bioseq)
{
  bool sf_i_processed = false;
  int i, j;
  for (i=0; (int)i< (int)(cd_feat.size() - 1); i++) {
    sf_i_processed = false;
    for (j = i+1; j < (int)cd_feat.size(); j++) {
      const CSeq_loc& loc_i = cd_feat[i]->GetLocation();
      const CSeq_loc& loc_j = cd_feat[j]->GetLocation();
      if (loc_i.GetStop(eExtreme_Positional) < loc_j.GetStart(eExtreme_Positional)) break;
      if (!OverlappingProdNmSimilar(GetProdNmForCD(*(cd_feat[i])), 
                                    GetProdNmForCD(*(cd_feat[j])))) 
          continue;
      ENa_strand strand1 = loc_i.GetStrand();
      ENa_strand strand2 = loc_j.GetStrand();
      if ((strand1 == eNa_strand_minus && strand2 != eNa_strand_minus)
          || (strand1 != eNa_strand_minus && strand2 == eNa_strand_minus)) continue;
      if ( eNoOverlap != Compare(loc_i, loc_j, thisInfo.scope)) {
          if (!sf_i_processed) {
              if ( HasNoSuppressionWords(cd_feat[i])) AddToDiscRep(cd_feat[i]);
              sf_i_processed = true;
          }
          if ( HasNoSuppressionWords(cd_feat[j]) ) AddToDiscRep(cd_feat[j]);
      }
    }  
  }
}


void CBioseq_OVERLAPPING_CDS :: GetReport(CRef <CClickableItem>& c_item)
{
   Str2Strs subcat2list;
   string desc1, desc2, desc3;
   GetTestItemList(c_item->item_list, subcat2list);
   c_item->item_list.clear();
   ITERATE (Str2Strs, it, subcat2list) {
     if (it->first == "comment") {
       desc1 = "coding region overlapps";
       desc2 = "coding regions overlap";
       desc3 = " another coding region with a similar or identical name that do not have the appropriate note text";
     }
     else {
       desc1 = "coding region overlapps";
       desc2 = "coding regions overlap";
       desc3 = " another coding region with a similar or identical name but have the appropriate note text";
     }
     AddSubcategories(c_item, GetName(), it->second, desc1, desc2, true, e_OtherComment,desc3);
   }
   c_item->description = 
     GetOtherComment(c_item->item_list.size(), "coding region overlapps", 
                                                                 "coding regions overlap")
     + " another coding region with a similar or identical name.";
}; // OVERLAPPING_CDS




void CBioseq_EXTRA_MISSING_GENES :: TestOnObj(const CBioseq& bioseq)
{
  vector <int> super_idx;
  unsigned super_cnt = gene_feat.size();
  unsigned i;
  for (i=0; i< super_cnt; i++) super_idx.push_back(1);
  vector <const CSeq_feat*> superfluous_genes;
  set <const CSeq_feat*> features_without_genes;
  ITERATE ( vector < const CSeq_feat* > , it, mix_feat) {
     const CGene_ref* xref_gene = (*it)->GetGeneXref();
     if (xref_gene && xref_gene->IsSuppressed() 
                && features_without_genes.find(*it) == features_without_genes.end())
          features_without_genes.insert(*it);
     else {
        if (xref_gene) {
           if (super_cnt) {
             i = 0;
             ITERATE (vector <const CSeq_feat*>, jt, gene_feat) {
               if (super_idx[i] &&
                      GeneRefMatchForSuperfluousCheck((*jt)->GetData().GetGene(), xref_gene)) {
                   super_cnt --;
                   super_idx[i] = 0;
               }
               i++;
             } 
           }
        }
        else {
          // GetBest... can't deal with the mix strand now, 
          // so for File1fordiscrep.sqn, it needs to check the EXTRA_GENES
          // and MISSING_GENES after the bug is fixed.
          CConstRef < CSeq_feat> 
             gene_olp= GetBestOverlappingFeat( (*it)->GetLocation(), 
                                               CSeqFeatData::e_Gene, 
                                               eOverlap_Contained, 
                                               *thisInfo.scope);

          if (gene_olp.Empty() && features_without_genes.find(*it) == features_without_genes.end()) 
               features_without_genes.insert(*it);
          else if ( super_cnt && ((*it)->GetData().IsCdregion() || (*it)->GetData().IsRna()) ){
             i=0;
             ITERATE (vector <const CSeq_feat*>, jt, gene_feat) {
               if (super_idx[i] && (*jt == gene_olp.GetPointer())) {
                    super_idx[i] = 0;
                    super_cnt --;
               }
               i++;
             } 
          }
        }
     }
  }

  ITERATE (set <const CSeq_feat* >, it, features_without_genes) 
     thisInfo.test_item_list[GetName()].push_back(GetName_missing() + "$" + GetDiscItemText(*(*it)));

//GetPseudoAndNonPseudoGeneList;
  if (!super_cnt) return;
  vector < CSeq_feat* > pseudo_ls, frameshift_ls, non_frameshift_lframeshift_ls, non_frameshift_ls;
  i = 0;
  string desc, nm_desc;
  ITERATE (vector <const CSeq_feat* >, it, gene_feat) {
     if (super_idx[i++]) {
       desc = GetDiscItemText(**it);
       thisInfo.test_item_list[GetName()].push_back(GetName_extra() + "$" + desc);
       nm_desc = kEmptyStr;
       const CSeqFeatData& seq_feat_dt = (*it)->GetData();
       if ((*it)->CanGetPseudo() && (*it)->GetPseudo())
              nm_desc = GetName_extra_pseudodp() + "$" + desc;
       else if ( seq_feat_dt.GetGene().GetPseudo() ) 
                 nm_desc = GetName_extra_pseudodp() + "$" + desc;
       else {
             //GetFrameshiftAndNonFrameshiftGeneList
             if ((*it)->CanGetComment()) {
               const string& comment = (*it)->GetComment();
               if (string::npos != NStr::FindNoCase(comment, "frameshift")
                    || string::npos!= NStr::FindNoCase(comment, "frame shift"))
                   nm_desc = GetName_extra_frameshift() + "$" + desc;
               else nm_desc = GetName_extra_nonshift() + "$" + desc;
             } 
       }
       if (!nm_desc.empty()) thisInfo.test_item_list[GetName()].push_back(nm_desc);
     }
  }
};  // EXTRA_MISSING_GENES :: TestOnObj



bool CBioseq_EXTRA_MISSING_GENES :: GeneRefMatchForSuperfluousCheck (const CGene_ref& gene, const CGene_ref* g_xref)
{
  const string& locus1 = (gene.CanGetLocus()) ? gene.GetLocus() : kEmptyStr;
  const string& locus_tag1 = (gene.CanGetLocus_tag()) ? gene.GetLocus_tag() : kEmptyStr;

  const string& locus2 = (g_xref->CanGetLocus()) ? g_xref->GetLocus() : kEmptyStr;
  const string& locus_tag2 = (g_xref->CanGetLocus_tag()) ? g_xref->GetLocus_tag() : kEmptyStr;

  if (locus1 != locus2 || locus_tag1 != locus_tag2 
          || (gene.GetPseudo() != g_xref->GetPseudo()) ) return false;
  else {
    const string& allele1 = (gene.CanGetAllele()) ? gene.GetAllele(): kEmptyStr;
    const string& allele2 = (g_xref->CanGetAllele()) ? g_xref->GetAllele(): kEmptyStr;
    if (allele1.empty() || allele2.empty()) return true;
    else if (allele1 != allele2) return false;
    else {
      const string& desc1 = (gene.CanGetDesc())? gene.GetDesc() : kEmptyStr;
      const string& desc2 = (g_xref->CanGetDesc())? g_xref->GetDesc() : kEmptyStr;
      if (desc1.empty() || desc2.empty()) return true;
      else if (desc1 != desc2) return false;
      else {
        const string& maploc1 = (gene.CanGetMaploc())? gene.GetMaploc(): kEmptyStr;
        const string& maploc2= (g_xref->CanGetMaploc())? g_xref->GetMaploc(): kEmptyStr; 
        if (!maploc1.empty() && !maploc2.empty() && maploc1 != maploc2) 
             return false; 
        else return true;
      }      
    } 
  }
};


void CBioseq_EXTRA_MISSING_GENES :: GetReport(CRef <CClickableItem>& c_item)
{
    bool has_missing = false;
    Str2Strs setting2list;
    string desc1(kEmptyStr), desc2(kEmptyStr);
    GetTestItemList(c_item->item_list, setting2list);
    if (setting2list.find(GetName_missing()) != setting2list.end()) has_missing = true;
    c_item->item_list.clear();
  
    CRef <CClickableItem> c_extra;
    if (has_missing) c_extra = CRef <CClickableItem> (new CClickableItem);
    else c_extra = c_item;
    ITERATE (Str2Strs, it, setting2list) {
      if (it->first == GetName_missing()) {
        c_item->setting_name = GetName_missing();
        c_item->item_list = it->second;
        c_item->description = GetHasComment(c_item->item_list.size(), "feature") + "no gene"; 
      }
      else {
        if (it->first.find("pseudodp") != string::npos) {
          desc1 = "pseudo gene feature"; 
          desc2 = "not associated with a CDS or RNA feature.";
          AddSubcategories(c_extra,  GetName(), it->second, desc1, desc2);
        }
        else if (it->first.find("frameshift") != string::npos) {
            desc1 = "non-pseudo gene feature";
            desc2
              ="not associated with a CDS or RNA feature and have frameshift in the comment.";
            AddSubcategories(c_extra,  GetName(), it->second, desc1, desc2);
        }
        else if (it->first.find("nonshift") != string::npos) {
            desc1 = "non-pseudo gene feature";
            strtmp = (it->second.size() > 1) ? "do" : "does";
            desc2 = "not associated with a CDS or RNA feature and " 
                              + strtmp + " not have frameshift in the comment.";
            AddSubcategories(c_extra,  GetName(), it->second, desc1, desc2);
        }
        else c_extra->item_list.insert(c_extra->item_list.begin(), 
                                                      it->second.begin(), it->second.end());
      }
    }

    c_extra->setting_name = GetName_extra();
    c_extra->description = GetIsComment(c_extra->item_list.size(), "gene feature")
                          + "not associated with a CDS or RNA feature.";
    if (has_missing && !c_extra->item_list.empty()) thisInfo.disc_report_data.push_back(c_extra);
}; // EXTRA_MISSING_GENES



void CBioseq_DISC_COUNT_NUCLEOTIDES :: TestOnObj(const CBioseq& bioseq)
{
   if (bioseq.IsNa())
        thisInfo.test_item_list[GetName()].push_back(GetDiscItemText(bioseq));

}; // DISC_COUNT_NUCLEOTIDES :: TestOnObj


void CBioseq_DISC_COUNT_NUCLEOTIDES :: GetReport(CRef <CClickableItem>& c_item)
{
    c_item->setting_name = GetName();
    c_item->item_list = thisInfo.test_item_list[GetName()];
    c_item->description = GetIsComment(c_item->item_list.size(), "nucleotide Bioseq")
                           + "present.";
}; // DISC_COUNT_NUCLEOTIDES :: GetReport()



CRef <GeneralDiscSubDt> CBioseq_DUPLICATE_GENE_LOCUS :: AddLocus(const CSeq_feat& seq_feat)
{
    CRef < GeneralDiscSubDt > gene_locus
              (new GeneralDiscSubDt (GetDiscItemText(seq_feat), kEmptyStr));
    return (gene_locus);
};


void CBioseq_DUPLICATE_GENE_LOCUS :: TestOnObj(const CBioseq& bioseq)
{
  Str2SubDt locus_list;
  Str2SubDt :: iterator it;
  ITERATE (vector <const CSeq_feat*>, jt, gene_feat) {
     if ((*jt)->GetData().GetGene().CanGetLocus()) {
        string this_locus = (*jt)->GetData().GetGene().GetLocus();
        if (locus_list.empty()) locus_list[this_locus] = AddLocus(**jt);
        else {
          it = locus_list.find(this_locus);
          if (locus_list.end() != it) { // duplicates
              if ( !(it->second->sf0_added) ) {
                   thisInfo.test_item_list[GetName()].push_back(it->second->obj_text[0]);
                   it->second->sf0_added = true;
              }
              strtmp = GetDiscItemText(**jt);
              thisInfo.test_item_list[GetName()].push_back(strtmp);
              it->second->obj_text.push_back(strtmp);
          }
          else locus_list[this_locus] = AddLocus(**jt);
        }
     }
  }   
  
  if (!locus_list.empty()) {

      const CSeq_id& seq_id = BioseqToBestSeqId(bioseq, CSeq_id::e_Genbank);
      string location = seq_id.AsFastaString();
      CRef < CClickableItem > c_ls_item (new CClickableItem);

      unsigned cnt=0;
      ITERATE (Str2SubDt, it, locus_list) {
         if (it->second->obj_text.size() > 1) {
            CRef < CClickableItem > c_item (new CClickableItem);
            c_item->item_list = it->second->obj_text;            
            cnt += it->second->obj_text.size();
            c_item->description = GetHasComment(c_item->item_list.size(), "gene")
                                   + "locus " + it->second->str;
            c_item->setting_name = "DUPLICATE_GENE_LOCUS";
            c_ls_item->subcategories.push_back(c_item);
         }
      }
      c_ls_item->item_list.clear();
      c_ls_item->item_list.reserve(cnt);
      ITERATE (Str2SubDt, it, locus_list) {
        if (it->second->obj_text.size() > 1) {
           c_ls_item->item_list.insert(c_ls_item->item_list.end(), 
                     it->second->obj_text.begin(), it->second->obj_text.end()); 
        }
      };
      c_ls_item->setting_name = GetName();
      c_ls_item->description = GetHasComment(c_ls_item->item_list.size(), "gene")
                                   + "the same locus as another gene on " + location;

      DUPLICATE_GENE_LOCUS_locuss.push_back(c_ls_item);
  }
};


void CBioseq_DUPLICATE_GENE_LOCUS :: GetReport(CRef <CClickableItem>& c_item)
{
   c_item->description = GetHasComment(c_item->item_list.size(), "gene")
                          + "the same locus as another gene on the same Bioseq.";
}; // DUPLICATE_GENE_LOCUS


/*
bool CBioseq_SUSPECT_PRODUCT_NAMES :: IsSearchFuncEmpty(const CSearch_func& func)
{
  if (func.IsString_constraint()) {
     const CString_constraint& str_cons = func.GetString_constraint();
     if (str_cons.GetIs_all_lower() || str_cons.GetIs_all_lower() || str_conf.GetIs_all_punct())
           return false;
     else if (!(str_cons.CanGetMatch_text()) return true;
     else return false;
  }
  else if (func.IsPrefix_and numbers()) {
      if (func.GetPrefix_and_numbers().empty()) return true;
      else return false;
  }
  else return false;
};



string CBioseq_SUSPECT_PRODUCT_NAMES :: SkipWeasel(string& str)
{
  vector <string> words;
  words = NStr::Tokenize(str, " ", words);
  
  vector <string> weasels;
  weasels.reserve(11);
  weasels.push_back("candidate");
  weasels.push_back("hypothetical");
  weasels.push_back("novel");
  weasels.push_back("possible");
  weasels.push_back("potential");
  weasels.push_back("predicted");
  weasels.push_back("probable");
  weasels.push_back("putative");
  weasels.push_back("uncharacterized");
  weasels.push_back("unique");

  NON_CONST_ITERATE (vector <string>, it, words) {
     if (!(*it).empty()) {
         ITERATE (vector <string>, jt, weasels) {
            if ( !(*it).empty() && NStr::FindNoCase(*it, *jt)) {
                *it = kEmptyStr;
                break;
            }
         }
     }
  }
 
**
  str = kEmptyStr;
  ITERATE (vector <string>, it, words) {
    if (!(*it).empty()) str += (*it) + " ";
  }
  return(NStr::TruncateSpaces(str));
**
  return (NStr::Join(words, " "));
};



bool CBioseq_SUSPECT_PRODUCT_NAMES :: DoesStringMatchConstraint(string& str, const CString_constrain& str_cons)
{
  if (str_cons.GetIgnore_weasel()) str = SkipWeasel(str);

  if (str_cons.GetIs_all_caps() && !IsAllCaps(str)) return false;
  if (str_cons.GetIs_all_lower && !IsAllLowerCase(str)) return false;
  if (str_cons.GetIs_all_punct && !IsAllPunctuation(str)) return false;
  if (!(str_conf.CanGetMatch_text())) return true; 

  string tmp_match = str_conf.GetMatch_text();
  if (str_cons.GetIgnore_weasel) {
    str_conf.SetMatch_text(SkipWeasel (scp->match_text));
  }

  if ((str_conf.GetMatch_location != eString_location_inlist) 
                                                      && !str_conf.CanGetIgnore_words()){
    str_conf.SetMatch_text(tmp_match);
    return AdvancedStringMatch(str, scp);
  }

  string search, pattern;
  if (str_conf.GetMatch_location() != eString_location_inlist 
           && (str_conf.GetIgnore_space() || str_cons.GetIgnore_punct())) {
    search = str;
    StripUnimportantCharacters (search, scp->ignore_space, scp->ignore_punct);
    pattern = str_confs.GetMatch_text();
    StripUnimportantCharacters (pattern, scp->ignore_space, scp->ignore_punct);
  } else {
    search = str;
    pattern = scp->match_text;
  }

  bool rval;
  if (str_conf.GetCase_sensitive()) pFound = search.find(pattern);
  else pFound = NStr::FindNoCase(search, pattern);
  switch (str_conf.GetMatch_location()) 
  {
    case eString_location_contains:
      if (string::npos == pFound) rval = FALSE;
      else if (str_conf.GetWhole_word()) 
      {
          rval = IsWholeWordMatch (search, pFound, pattern.size());
          while (!rval && pFound != string::npos) 
          {
	     if (str_conf.GetCase_sensitive()) pFound = search.substr(pFound+1).find(patten);
	     else pFound = NStr::FindNoCase(search.substr(pFound+1), pattern);
             if (pFound != string::npos)
                 rval = IsWholeWordMatch (search, pFound, pattern.size());
          }
      }
      else rval = TRUE;
      break;
    case eString_location_starts:
      if (pFound == search)
      {
        if (str_conf.GetWhole_word()) 
            rval = IsWholeWordMatch (search, pFound, pattern.size());
        else rval = TRUE;
      }
      break;
    case eString_location_ends:
      while (pFound != string::npos && !rval) {
  	    char_after = *(pFound + StringLen (pattern));
        if ( (pFound + pattern.size()) == search.size())
        {
          if (str_conf.GetWhole_word()) 
              rval = IsWholeWordMatch (search, pFound, pattern.size());
          else rval = true;
          * stop the search, we're at the end of the string *
          pFound = string::npos;
        }
        else
        {
	   if (str_conf.GetCase_sensitive()) pFound = search.substr(pFound+1).find(pattern);
	   else pFound = NStr::FindNoCase(search.substr(pFound + 1), pattern);
        }
      }
      break;
    case eString_location_equals:
      if (str_conf.GetCase_sensitive() && (search==pattern) ) rval= true; 
      else if (!str_conf.GetCase_sensitive() 
                  && !NStr::CompareNocase(search, pattern) ) rval = true;
      break;
    case eString_location_inlist:
      if (str_conf.GetCase_sensitive()) pFound = pattern.find(search);
      else pFound = NStr::FinNoCase(pattern, search);
      if (pFound == string::npos) rval = FALSE;
      else
      {
        rval = IsWholeWordMatchEx (pattern, pFound, search.size(), true);
        while (!rval && pFound != string::npos) 
        {
          if (str_conf.GetCase_sensitive())
               pFound = pattern.substr(pFound + 1).find(search);
          else pFound = NStr::FindNoCase(pattern.substr(pFound + 1), search);
          if (pFound != string::npos)
              rval = IsWholeWordMatchEx (pattern, pFound, str.size(), TRUE);
        }
      }
      if (!rval) {
        * look for spans *
        rval = IsStringInSpanInList (search, pattern);
      }
      break;
  }

  str_conf.SetMatch_text(tmp_match);
  return rval;
};
*/



/*
void CBioseq_SUSPECT_PRODUCT_NAMES :: MatchesSearchFunc(const string& str, const CSearch_func& func)
{
    if (func.IsString_constrain()) {
        bool rval = DoesSingleStringMatchConstraint(str, func.GetString_constrain());
        rval = (func.GetString_constrain().GetNot_present()) ? !rval : rval;
        return rval;
    }
    else if (func.IsContains_plural()) return (StringMayContainPlural (str));
    else if (func.IsN_or_more_brackets_or_parentheses())
           return( ContainsNorMoreSetsOfBracketsOrParentheses (str, search->data.intvalue));
    else if (func.IsThree_numbers()) return(ContainsThreeOrMoreNumbersTogether (str));
    else if (func.IsUnderscore()) return(StringContainsUnderscore (str));
    else if (func.IsPrefix_and_numbers()) 
              return( IsPrefixPlusNumbers (func.Getprefix_and_number(), str));
    else if (func.IsAll_caps()) return (IsAllCaps (str));
    else if (func.IsUnbalanced_paren()) return (StringContainsUnbalancedParentheses (str));
    else if (func.IsToo_long()) {
      if ((string::npos != str.fin("bifunctional")) 
             && (string::npos == str.find("multifunctional"))
             && (str.size() > func.GetToo_long()) ) 
          return true;
      else return false;
    }
    else if (func.IsHas_term()) return (ProductContainsTerm (search->data.ptrvalue, str));
    else return false;
};
*/




/*
string CBioseq_SUSPECT_PRODUCT_NAMES :: SummarizeStringConstraint(const CString_constraint& constraint)
{

   return("not set yet");
};




string CBioseq_SUSPECT_PRODUCT_NAMES :: SummarizeSuspectRule(const CSuspect_rule& rule)
{
   string summ; 

   if (rule.CanGetDescription()) return (rule.GetDescription());
   else {
     const CSearch_func& func = rule.GetFind();
     if (func.IsString_constraint()) 
            summ = SummarizeStringConstraint(func.GetString_constraint());
     else if (func.IsContains_plural()) 
             summ = "May contain plural";
     else if (func.IsN_or_more_brackets_or_parentheses())
             summ = "Contains " 
                     + NStr::IntToString(func.GetN_or_more_brackets_or_parentheses())
                     + " or more brackets or parentheses";
     else if (func.IsThree_numbers()) summ = "Three or more numbers together";
     else if (func.IsUnderscore()) summ = "Contains underscore";
     else if (func.IsPrefix_and_numbers())
              summ = "Is '" + func.GetPrefix_and_numbers() + "' followed by numbers";
     else if (func.IsAll_caps()) summ = "Is all capital letters";
     else if (func.IsUnbalanced_paren()) summ = "Contains unbalanced brackets or parentheses";
     else if (func.IsToo_long()) 
              summ = "Is longer than " + NStr::IntToString(func.GetToo_long()) + " characters";
     else if (func.IsHas_term())
              summ = "Contains '" + func.GetHas_term() + "'";
     else summ = "Unknown search function";
   } 
   return summ;
}




void CBioseq_SUSPECT_PRODUCT_NAMES :: DoesStringMatchConstraint(vector <string>& prot_nm, vector <bool>& match_ls, const CString_constraint& str_cst)
{
   if ( (!(str_cst.CanGetMatch_text()) || str_cst.GetMatch_text().empty()) ) {

       NON_CONST_ITERATE(vector <bool>, it, match_ls) *(it) = true;
       return;
   }

   unsigned i=0, cnt = 0;
   ITERATE(vector <string>, it, prot_nm) {
       if ((*it).empty()) { match_ls[i++] = false; cnt++;};
   }
   if (cnt == prot_nm.size()) return;

   if (str_cst.GetIgnore_weasel()) 
         NON_CONST_ITERATE (vector <string>, it, prot_nm)
             *it = SkipWeasel(*it);
   
   i=0;
   if (str_cst.GetIs_all_caps()) 
     NON_CONST_ITERATE (vector <bool>, it, match_ls)
        if (!(*it)) i++;
        else if (!IsAllCaps(prot_nm[i++])) { (*it) = false; cnt++;};
   if (cnt == prot_nm.size()) return;

   i = 0;
   if (str_cst.GetIs_all_lower()) 
       NON_CONST_ITERATE (vector <bool>, it, match_ls) 
          if (!(*it)) i++;
          else if (!IsAllLowerCase(prot_nm[i++])) { (*it) = false; cnt++; }
   if (cnt == prot_nm.size()) return;

   if (!(str_cst.CanGetMatch_text())) {
       NON_CONST_ITERATE (vector <bool>, it, match_ls) {
            (*it) = true;
       }
       return;
   }

   if (str_cst.GetMatch_location() !=eString_location_inlist 
                 && !(str_cst.CanGetIgnore_words())){
//         AdVancedStringMatch(prot_nm, str_cst);
         return;
   } 

   string tmp_match = str_cst.GetMatch_text();
   if (str_cst.GetIgnore_weasel()) {
       strtmp = SkipWeasel(str_cst.GetMatch_text());
       str_cst.SetMatch_text("!!!");
   }
   
   string pattern( str_cst.GetMatch_text() ); 
   if (eString_location_inlist != str_cst.GetMatch_Location()
        && ( str_cst.GetIgnore_space() || str_cst.GetIgnore_punct()) ) {
      StripUnimportantCharacters(prot_nm, str_cst.GetIgnore_space(), str_cst.GetIgnore_punct());
   }

   vector <size_t> pFound;
   pFound.reserve(prot_nm.size());
   i=0;
   if (str_cst.GetMatch_location != eString_location_inlist) {
     if (str_cst.GetCase_sensitive()) {
         NON_CONST_ITERATE (vector <size_t>, it, pFound) {
           if (match_ls[i] < 0) (*it) = prot_nm[i].find(pattern);
           else (*it) = string::npos; 
           i++;
         }
     }
     else {
         NON_CONST_ITERATE (vector <size_t>, it, pFound) {
           if (match_ls[i] < 0) (*it) = NStr::FindNoCase(prot_nm[i], pattern);
           else (*it) = string::npos;
           i++;
         }
     }
   }
   else {
     if (str_cst.GetCase_sensitive()) {
         NON_CONST_ITERATE (vector <size_t>, it, pFound) {
           if (match_ls[i]) (*it) = pattern.find(prot_nm[i]);
           else (*it) = string::npos;
           i++;
         }
     }
     else {
         NON_CONST_ITERATE (vector <size_t>, it, pFound) {
           if (match_ls[i]) (*it) = NStr::FindNoCase(pattern, prot_nm[i]);
           else (*it) = string::npos;
           i++;
         }
     }

   }
 
   i=0;
   bool if_match;
   switch (str_cst.GetMatch_location()) {
      case eString_location_contains:
         NON_CONST_ITERATE (vector <int>, it, match_ls) {
            if (*it < 0) {
                if (string::npos == pFound[i])  {
                      (*it) = 0;
                      cnt++;
                }
            }
            i++;
         }
         if (cnt < prot_nm.size()) {
             if (str_cst.GetWhole_word()) {
                i=0;
                bool if_found, if_match;
                NON_CONST_ITERATE (vector <int>, it, match_ls) {
                  if (*it < 0) {
                    strtmp = prot_nm[i];
                    if_found = pFound[i];
                    if_match = IsWholeWordMatch(strtmp, if_found, pattern.size());
                    while (string::npos != if_found && !if_match) {
                        if (str_cst.GetCase_sensitive()) if_found = strtmp.find(pattern);
                        else if_found = NStr::FinNoCase(strtmp, pattern);
                    } 
                    *it = if_match ? 1 : 0; 
                  }
                  i++;
                }
             };
             else NON_CONST_ITERATE (vector <int>, it, match_ls) if (*it < 0) *it = 1;
          }
          break;
      case eString_location_starts:
          NON_CONST_ITERATE (vector <bool>, it, match_ls) {
             if (*it) {
                if (!pFound[i]) {
                   if (str_cst.GetWhole_word())
                        *it = true; //IsWholeWordMatch(prot_nm[i], pFound[i], pattern.size());
                   else *it = true;
                }
             }
             i++;
          }; 
          break;
      case eString_location_ends:
          i = 0;
          string char_after;
          bool if_found;
          NON_CONST_ITERATE(vector <bool>, it, match_ls) {
            if (*it < 0) {
              bool rval = false;
              if_found = pFound[i]; 
              while (string::npos != if_found && !rval) {
                 if ( (if_found + pattern.size()) > prot_nm.size()) {
                   if (str_cst.GetWhole_word())
                      match_ls[i] = (IsWholeWordMatch(prot_nm, if_found, pattern.size()))? 1: 0;
                   else match_ls[i] = 1;
                   if_found = string::npos;
                 }
                 else {
                   if (str_cst.GetCase_sensitive())
                        if_found = prot_nm.substr(if_found+1).find(pattern);
                   else if_found = NStr::FindNoCase(pro_nm.substr(if_found+1), pattern);
                 }
              }
           }
           i++;  
          };
          break;
      case eString_location_equals:
          if (str_cst.GetCase_sensitive()) {
             NON_CONST_ITERATE (vector <bool>, it, match_ls) {
                if (*it) {
                   if (prot_nm[i] == pattern) *it = true;
                   else *it = false;
                }
                i++;
             }
          }
          else {
             NON_CONST_ITERATE (vector <bool>, it, match_ls) {
                if (*it) {
                    if (!NStr::CompareNocase(prot_nm[i], pattern)) *it = true;
                    else *it = false;
                }
                i++;
             }
          }
          break;
      case eString_location_inlist:
          i=0; 
          NON_CONST_ITERATE (vector <bool>, it, match_ls) {
            if (*it) {
               if (pFound[i] == string::npos) {
                   *it = false;
                   cnt ++;
               }
            } 
          }
          if (cnt < prot_nm.size()) {
             i = 0;
             NON_CONST_ITERATE (vector <bool>, it, match_ls) {
                if (*it) {
                   strtmp = prot_nm[i];
                   is_found = pFound[i];
                   is_match = IsWholeWordMatch(pattern, is_found, strtmp.size())
                   while (!is_match && string::npos != is_found) {
                      if (str_cst.GetCase_sensitive()) 
                            is_found = pattern.substr(is_found+1,strtmp);
                      else is_found = NStr::FindNoCase(pattern.substr(is_found+1), strtmp);
                      if (string::npos != is_found)
                        is_match = IsWholeWordMatch(pattern, is_found, strtmp.size());
                   }
                  if (!is_match) {
                      match_ls = IsStringInSpanInList(strtmp, pattern);
                  };
                }
                i++; 
             }
          }
          break;
   }

   str_cst.SetMatch_text(tmp_match);

   if (str_cst.GetNot_present()) 
        NON_CONST_ITERATE (vector <bool>, it, match_ls) *it = !(*it);
};






void CBioseq_SUSPECT_PRODUCT_NAMES :: MatchesSrchFunc(vector <string>& prot_nm, vector <bool>& match_ls, const CSearch_func& func)
{
  if (func.IsString_constraint())
        DoesStringMatchConstraint(prot_nm, func->GetString_constraint());
  
};



void CBioseq_SUSPECT_PRODUCT_NAMES :: DoesObjectMatchStringConstraint (vector <CSeq_feat*> seq_feats, vector <bool> match_ls, const CString_constraint& str_cst)
{
};


void CBioseq_SUSPECT_PRODUCT_NAMES :: DoesObjectMatchConstraint(const CRef < CConstraint_choice>& cst, vector <const CSeq_feat*> seq_feats, vector <bool> match_ls) 
{
  if (cst->IsString()) {
      DoesObjectMatchStringConstraint(seq_feats, match_ls, cst->GetString());
  }
};



void CBioseq_SUSPECT_PRODUCT_NAMES :: DoesObjectMatchConstraintChoiceSet(const CConstraint_choice_set& feat_cst, vector <const CSeq_feat*>& seq_feats, vector <bool>& match_ls)
{
   const list < CRef < CConstraint_choice > >& cst_ls = feat_cst.Get();
   ITERATE (CConstraint_choice_set::Tdata, it, cst_ls) {
      DoesObjectMatchConstraint(*it, seq_feats, match_ls);
   }
};






void CBioseq_SUSPECT_PRODUCT_NAMES :: TestOnObj(const CBioseq& bioseq)  // with rules
{
  const CSuspect_rule_set::Tdata& rules = thisInfo.suspect_rules->Get();
  vector < vector <CSeq_feat*> > feature_ls;

  unsigned k=0;
  CSeq_feat* seq_feat;
  vector <string> prot_nm;
  vector <const CSeq_feat*> seq_feat_ls;

  ITERATE (vector <const CSeq_feat*>, it, prot_feat) {
     seq_feat = const_cast <CSeq_feat*> (*it);
     const CProt_ref& prot = seq_feat->GetData().GetProt();
     if (prot.CanGetName()) {
       if ((*it)->GetData().GetSubtype() == CSeqFeatData::eSubtype_cdregion) {
          const CSeq_feat* cds = GetCDSForProduct(bioseq, thisInfo.scope);
          if (cds) seq_feat = const_cast <CSeq_feat*> (cds);
       }
       prot_nm.push_back(*(prot.GetName().begin()));
       seq_feat_ls.push_back(const_cast <const CSeq_feat*>(seq_feat));
     }
  }
 
  vector <bool> match_ls;
  match_ls.reserve(prot_nm.size());

  k = 0; 
  ITERATE (CSuspect_rule_set::Tdata, it, rules) {
     match_ls.clear();
     if (!(rule_prop->srch_func_empty[k])) MatchesSrchFunc(prot_nm, match_ls, (*it)->GetFind());
     else if (!(rule_prop->except_empty[k])) MatchesSrchFunc(prot_nm, match_ls,(*it)->GetExcept());
     else NON_CONST_ITERATE (vector <bool>, jt, match_ls) *jt = true;
     
     if ( (*it)->CanGetFeat_constraint() )
         DoesObjectMatchConstraintChoiceSet((*it)->GetFeat_constraint(), seq_feat_ls, match_ls);

     //ITERATE (match_ls) -> suspect_prod_name_feat[k]
  }
};

*/




/*  previous version
void CBioseq_SUSPECT_PRODUCT_NAMES :: TestOnObj(const CBioseq& bioseq)  // with rules
{
  const CSuspect_rule_set::Tdata& rules = thisInfo.suspect_rules->Get();
  vector < vector <CSeq_feat*> > feature_ls; 
  
  unsigned k;
  CSeq_feat* seq_feat;
  ITERATE (vector <const CSeq_feat*>, it, prot_feat) {
    seq_feat = const_cast <CSeq_feat*> (*it);
    const CProt_ref& prot = seq_feat->GetData().GetProt();
    if (prot.CanGetName()) {
       if ((*it)->GetData().GetSubtype() == CSeqFeatData::eSubtype_cdregion) {
          const CSeq_feat* cds = GetCDSForProduct(bioseq, thisInfo.scope);
          if (cds) seq_feat = const_cast <CSeq_feat*> (cds);
       }
       k = 0;
       ITERATE (CSuspect_rule_set::Tdata, jt, rules) {
         if (DoesStringMatchSuspectRule ( prot.GetName(), seq_feat, *jt, k)) {
            strtmp = GetDiscItemText(*seq_feat);
            thisInfo.test_item_list[GetName()].push_back(strtmp);  // master_list
            if (!SUSPECT_PROD_NAMES_feat[k]) {
               CRef <GeneralDiscSubDt> feat (new GeneralDiscSubDt (strtmp, 
                                                           SummarizeSuspectRule(*(*jt)))); 
               SUSPECT_PROD_NAMES_feat[k] = feat;
            }
            else SUSPECT_PROD_NAMES_feat[k]->seq_feat_text.push_back(strtmp);
         }
         k++;
       }
    }
  }
};
*/



/*
void CBioseq_SUSPECT_PRODUCT_NAMES :: GetReport(CRef <CClickableItem>& c_item)
{
   c_item->setting_name = GetName();
   c_item->item_list = thisInfo.test_item_list[GetName()];

   typedef map <EFix_type, CRef < CClickableItem > > RuleTp2Citem;
   RuleTp2Citem rule_tp_cate;
   map <EFix_type, vector < unsigned > > rule_feat_idx;

   unsigned i = 0;
   ITERATE (CSuspect_rule_set::Tdata, it, thisInfo.suspect_rules->Get()) {
      CRef <CClickableItem> c_feat (new CClickableItem);

      EFix_type rule_tp = (*it)->GetRule_type();
      switch ( rule_tp ) {
         case eFix_type_typo:
                   strtmp = "DISC_PRODUCT_NAME_TYPO"; break;
         case eFix_type_quickfix:
                   strtmp = "DISC_PRODUCT_NAME_QUICKFIX"; break;
         default:
                   strtmp = GetName();
      };        
      c_feat->setting_name = strtmp;
      c_feat->item_list = SUSPECT_PROD_NAMES_feat[i]->seq_feat_text;
      c_feat->description = SUSPECT_PROD_NAMES_feat[i]->str;
**
      if ((*it)->replace) {
...
      }
**
      if (rule_tp_cate.end() == rule_tp_cate.find(rule_tp)) {
          CRef < CClickableItem > c_rule (new CClickableItem);
          c_rule->setting_name = GetName();
          c_rule->description = "!!!";
          c_rule->item_list.insert(c_rule->item_list.end(), 
                        c_feat->item_list.begin(), c_feat->item_list.end());
          c_rule->expanded = true;
          c_rule->subcategories.push_back(c_feat);
      }
      else  {
          rule_tp_cate[rule_tp]->subcategories.push_back(c_feat);
          rule_tp_cate[rule_tp]->item_list.insert( rule_tp_cate[rule_tp]->item_list.end(), 
                       c_feat->item_list.begin(), c_feat->item_list.end());
      }
      rule_feat_idx[rule_tp].push_back(i);
   }

   unsigned tot_cnt;
   NON_CONST_ITERATE ( RuleTp2Citem, it, rule_tp_cate) {
      tot_cnt = 0;
      ITERATE ( vector < unsigned >, jt, rule_feat_idx[it->first])
            tot_cnt += SUSPECT_PROD_NAMES_feat[*jt]->seq_feat_text.size();
      it->second->item_list.reserve (tot_cnt);
      ITERATE ( vector < unsigned >, jt, rule_feat_idx[it->first]) 
           it->second->item_list.insert(it->second->item_list.end(),
                     SUSPECT_PROD_NAMES_feat[*jt]->seq_feat_text.begin(), 
                     SUSPECT_PROD_NAMES_feat[*jt]->seq_feat_text.end());    
      c_item->subcategories.push_back(it->second);
   }   

   c_item->expanded = true;
};
*/


// CSeqFeat


// CBioseq_set
void CBioseqSet_DISC_NONWGS_SETS_PRESENT :: TestOnObj(const CBioseq_set& bioseq_set)
{
   CBioseq_set::EClass e_cls = bioseq_set.GetClass();
   if ( e_cls == CBioseq_set::eClass_eco_set
        || e_cls == CBioseq_set :: eClass_mut_set
        || e_cls == CBioseq_set :: eClass_phy_set
        || e_cls == CBioseq_set :: eClass_pop_set) 
      thisInfo.test_item_list[GetName()].push_back(GetDiscItemText(bioseq_set));
};


void CBioseqSet_DISC_NONWGS_SETS_PRESENT :: GetReport(CRef <CClickableItem>& c_item)
{
   c_item->description 
        = GetIsComment(c_item->item_list.size(), "set") + "of type eco, mut, phy or pop.";
};



// CSeqEntryTestAndRepData
CConstRef <CCit_sub> CSeqEntry_DISC_CITSUB_AFFIL_DUP_TEXT :: CitSubFromPubEquiv(const list <CRef <CPub> >& pubs)
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
bool CSeqEntry_DISC_CITSUB_AFFIL_DUP_TEXT :: AffilStreetEndsWith(const string& street, const string& end_str)
{
  unsigned street_sz = street.size();
  unsigned end_sz = end_str.size();
  unsigned diff_sz = street_sz - end_sz;
  unsigned uni_sz = uni_str.size();
  if (!end_sz || street_sz < end_sz) return false;
  else if (NStr::EqualNocase(street, diff_sz, street_sz-1, end_str)
            && ( (street_sz == end_sz 
                        || isspace(street[diff_sz-1]) || ispunct(street[diff_sz-1])))) {
     if (diff_sz >= uni_sz 
            && NStr::EqualNocase(street, diff_sz - uni_sz -1, diff_sz-1, uni_str))
          return false;
     else return true;
  }
  else return false;
};


bool CSeqEntry_DISC_CITSUB_AFFIL_DUP_TEXT :: AffilStreetContainsDuplicateText(const CAffil& affil)
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


void CSeqEntry_DISC_CITSUB_AFFIL_DUP_TEXT :: TestOnObj(const CSeq_entry& seq_entry)
{
   ITERATE (vector <const CSeq_feat*>, it, pub_feat) {
     CConstRef <CCit_sub> cit_sub (
                   CitSubFromPubEquiv((*it)->GetData().GetPub().GetPub().Get()));
     if (cit_sub.NotEmpty() && cit_sub->GetAuthors().CanGetAffil()) {
       const CAffil& affil = cit_sub->GetAuthors().GetAffil();
       if (affil.IsStd() && affil.GetStd().CanGetStreet() 
              && !(affil.GetStd().GetStreet().empty())
              && AffilStreetContainsDuplicateText(cit_sub->GetAuthors().GetAffil())) {
          thisInfo.test_item_list[GetName()].push_back(GetDiscItemText(**it));
          break;
       }
     }
   }

   unsigned i=0;
   ITERATE (vector <const CSeqdesc*>, it, pub_seqdesc) {
     CConstRef <CCit_sub> cit_sub (
                  CitSubFromPubEquiv( (*it)->GetPub().GetPub().Get()));
     if (cit_sub.NotEmpty() && cit_sub->GetAuthors().CanGetAffil()) {
       const CAffil& affil = cit_sub->GetAuthors().GetAffil();
       if (affil.IsStd() && affil.GetStd().CanGetStreet()
              && !(affil.GetStd().GetStreet().empty())
              && AffilStreetContainsDuplicateText(cit_sub->GetAuthors().GetAffil())) {
          thisInfo.test_item_list[GetName()].push_back(
                         GetDiscItemText(**it, *(pub_seqdesc_seqentry[i])));
          break;
       }
     }
     i++;
   }
  
   CSubmit_block this_submit_blk;
   string desc;
   if (thisInfo.submit_block.NotEmpty()
        && AffilStreetContainsDuplicateText(
                   thisInfo.submit_block->GetCit().GetAuthors().GetAffil())) {
        if (seq_entry.IsSeq()) 
           desc = GetDiscItemText(seq_entry.GetSeq());
        else desc = GetDiscItemText(seq_entry.GetSet());
        size_t pos = desc.find(": ");
        desc = desc.substr(0, pos + 2) + "Cit-sub for " + desc.substr(pos+3);
        thisInfo.test_item_list[GetName()].push_back(desc);
   }
};

void CSeqEntry_DISC_CITSUB_AFFIL_DUP_TEXT :: GetReport(CRef <CClickableItem>& c_item)
{
   c_item->description
     = GetHasComment(c_item->item_list.size(), "Cit-sub pub") + "duplicate affil text.";
};



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



void CSeqEntryTestAndRepData :: AddBioseqsInSeqentryToReport(const unsigned& i, const string& setting_name)
{
  if (user_seqdesc_seqentry[i]->IsSeq())
    thisInfo.test_item_list[setting_name].push_back(
                            GetDiscItemText( user_seqdesc_seqentry[i]->GetSeq() ));
  else AddBioseqsOfSetToReport(user_seqdesc_seqentry[i]->GetSet(), GetName(), true, true);
};



void CSeqEntryTestAndRepData :: TestOnDesc_User()
{
  string type, desc;
  unsigned i=0;
  ITERATE (vector <const CSeqdesc*>, it, user_seqdesc) {
    const CUser_object& user_obj = (*it)->GetUser();

    // MISSING_PROJECT
    if (user_obj.GetType().IsStr()) {
        type = user_obj.GetType().GetStr(); 
        if (type != "GenomeProjectsDB" 
             && (type != "DBLink" || !user_obj.HasField("BioProject"))) { 
            AddBioseqsInSeqentryToReport(i, GetName_missing());
        }
    }
    i++;
  }

  thisTest.is_DESC_user_run = true;
};



void CSeqEntry_MISSING_PROJECT :: TestOnObj(const CSeq_entry& seq_entry)
{
  if (!thisTest.is_DESC_user_run) TestOnDesc_User();
};


void CSeqEntry_MISSING_PROJECT :: GetReport(CRef <CClickableItem>& c_item)
{
  RmvRedundancy(c_item->item_list);   // all CSeqEntry_Feat_desc tests need this. 

  c_item->description
    = GetOtherComment(c_item->item_list.size(), "sequence does", "sequences do")
       + "not include project.";
};



bool CSeqEntryTestAndRepData :: HasMoreOrSpecNames(const CBioSource& biosrc, CSubSource::ESubtype subtype, vector <string>& submit_text, bool check_mul_nm_only)
{
   string name;
   vector <string> name_ls;
   if (biosrc.CanGetSubtype()) {
     ITERATE (list <CRef <CSubSource> >, it, biosrc.GetSubtype()) {
       if ( (*it)->GetSubtype() == subtype) {
          name = (*it)->GetName();
          name_ls = NStr::Tokenize(name, ",;", name_ls, NStr::eMergeDelims);
          if (name_ls.size() >= 3) return true;
          else if (!check_mul_nm_only) {
            ITERATE (vector <string>, jt, thisInfo.spec_words_biosrc) 
              if (NStr::FindNoCase(name, *jt) != string::npos) return true;
          }
          if (!check_mul_nm_only) {
              ITERATE (vector <string>, it, submit_text)
                  if ( (*it) == name) return true;
          }
       }
     }
   } 
   return false;
};


void CSeqEntryTestAndRepData :: GetSubmitText(const CAuth_list& authors, vector <string>& submit_text)
{
   if (authors.CanGetAffil()) {
     const CAffil& affil = authors.GetAffil();
     if (affil.IsStd()) {
        if (affil.GetStd().CanGetAffil()) {
             strtmp = affil.GetStd().GetAffil();
             if (!strtmp.empty()) submit_text.push_back(strtmp);
        }
        if (affil.GetStd().CanGetDiv()) {
             strtmp = affil.GetStd().GetDiv();
             if (!strtmp.empty()) submit_text.push_back(strtmp);
        }
     }
   }
};



void CSeqEntryTestAndRepData :: FindSpecSubmitText(vector <string>& submit_text)
{
  string inst(kEmptyStr), dept(kEmptyStr);
  if (thisInfo.submit_block.NotEmpty()) {
    GetSubmitText(thisInfo.submit_block->GetCit().GetAuthors(), submit_text);
  }

  ITERATE (vector <const CSeq_feat*>, it, pub_feat) {
     ITERATE (list < CRef < CPub > >, jt, (*it)->GetData().GetPub().GetPub().Get()) 
       if ( (*jt)->IsSetAuthors()) GetSubmitText((*jt)->GetAuthors(), submit_text);
  }
 
  ITERATE (vector <const CSeqdesc*>, it, pub_seqdesc) {
     ITERATE (list < CRef < CPub > >, jt, (*it)->GetPub().GetPub().Get())
       if ( (*jt)->IsSetAuthors()) GetSubmitText((*jt)->GetAuthors(), submit_text);
  }
};



void CSeqEntryTestAndRepData :: TestOnFeatDesc_Biosrc()
{
   string desc;
   vector <string> submit_text;
   FindSpecSubmitText(submit_text);

   vector <const CSeq_feat*> iden_more_nm_ls, iden_spec_wd_ls;
   ITERATE (vector <const CSeq_feat*>, it, biosrc_feat) {
     desc = GetDiscItemText(**it);
     const CBioSource& biosrc = (*it)->GetData().GetBiosrc();

     // MORE_OR_SPEC_NAMES_IDENTIFIED_BY
     if ( HasMoreOrSpecNames( biosrc, CSubSource::eSubtype_identified_by, submit_text))
        thisInfo.test_item_list[GetName_iden_nm()].push_back(desc);

     // MORE_NAMES_COLLECTED_BY
     if ( HasMoreOrSpecNames( biosrc, CSubSource::eSubtype_collected_by, submit_text, true))
        thisInfo.test_item_list[GetName_col_nm()].push_back(desc);
   }

   unsigned i=0;
   ITERATE (vector <const CSeqdesc*>, it, biosrc_seqdesc) {
     desc = GetDiscItemText(**it, *(biosrc_seqdesc_seqentry[i])); 
     const CBioSource& biosrc = (*it)->GetSource();
     if ( HasMoreOrSpecNames( biosrc, CSubSource::eSubtype_identified_by, submit_text ))
         thisInfo.test_item_list[GetName_iden_nm()].push_back(desc);
     if ( HasMoreOrSpecNames( biosrc, CSubSource::eSubtype_collected_by, submit_text, true ))
         thisInfo.test_item_list[GetName_col_nm()].push_back(desc);
     i++;
   }

   thisTest.is_FEAT_DESC_biosrc_run = true;
};



void CSeqEntry_MORE_NAMES_COLLECTED_BY :: TestOnObj(const CSeq_entry& seq_entry)
{
   if (!thisTest.is_FEAT_DESC_biosrc_run) TestOnFeatDesc_Biosrc();
};



void CSeqEntry_MORE_NAMES_COLLECTED_BY :: GetReport(CRef <CClickableItem>& c_item)
{
  c_item->description = GetHasComment(c_item->item_list.size(), "biosource")
                          + "3 or more names in collected-by.";
};


void CSeqEntry_MORE_OR_SPEC_NAMES_IDENTIFIED_BY :: TestOnObj(const CSeq_entry& seq_entry)
{
  if (!thisTest.is_FEAT_DESC_biosrc_run) TestOnFeatDesc_Biosrc();
};



void CSeqEntry_MORE_OR_SPEC_NAMES_IDENTIFIED_BY :: GetReport(CRef <CClickableItem>& c_item)
{
   c_item->description = GetHasComment(c_item->item_list.size(), "biosource") 
                          + "3 or more names or suspect text in identified-by.";
};



bool CSeqEntry_DISC_REQUIRED_STRAIN :: IsMissingRequiredStrain(const CBioSource& biosrc) 
{
  if (!HasLineage(biosrc, "Bacteria")) return false;
  if ( biosrc.IsSetOrgname() && biosrc.GetOrgname().CanGetMod()) { 
     ITERATE (list <CRef <COrgMod> >, it, biosrc.GetOrgname().GetMod()) 
       if ( (*it)->GetSubtype() == COrgMod :: eSubtype_strain) return false;

     return true;
  }
  else return false;
};

void CSeqEntry_DISC_REQUIRED_STRAIN  :: TestOnObj(const CSeq_entry& seq_entry)
{
  ITERATE (vector <const CSeq_feat*>, it, biosrc_feat)
    if (IsMissingRequiredStrain( (*it)->GetData().GetBiosrc() )) 
      thisInfo.test_item_list[GetName()].push_back(GetDiscItemText(**it));

  unsigned i=0;
  ITERATE (vector <const CSeqdesc*>, it, biosrc_seqdesc) {
    if (IsMissingRequiredStrain( (*it)->GetSource())) 
      thisInfo.test_item_list[GetName()].push_back(
                         GetDiscItemText(**it, *(biosrc_seqdesc_seqentry[i])));
    i++;
  }
};



void CSeqEntry_DISC_REQUIRED_STRAIN :: GetReport(CRef <CClickableItem>& c_item)
{
  c_item->description = GetIsComment(c_item->item_list.size(), "biosource") 
                                                         + "missing required strain value";
};



bool CSeqEntry_DIVISION_CODE_CONFLICTS :: HasDivCode(const CBioSource& biosrc, string& div_code)
{
   if (biosrc.IsSetDivision() && !(div_code= biosrc.GetDivision()).empty()) return true;
   else return false;
};


void CSeqEntry_DIVISION_CODE_CONFLICTS :: TestOnObj(const CSeq_entry& seq_entry)
{
   string div_code;
   ITERATE (vector <const CSeq_feat*>, it, biosrc_feat) {
     const CBioSource& biosrc = (*it)->GetData().GetBiosrc();
     if (HasDivCode(biosrc, div_code))
        thisInfo.test_item_list[GetName()].push_back(div_code + "$" + GetDiscItemText(**it));
   }

   unsigned i=0;
   ITERATE (vector <const CSeqdesc*>, it, biosrc_seqdesc) {
     const CBioSource& biosrc = (*it)->GetSource();
     if (HasDivCode(biosrc, div_code)) 
        thisInfo.test_item_list[GetName()].push_back(
                          div_code + "$" + GetDiscItemText(**it, *(biosrc_seqdesc_seqentry[i])));
     i++;
   } 
};




void CSeqEntry_DIVISION_CODE_CONFLICTS  :: GetReport(CRef <CClickableItem>& c_item)
{
   Str2Strs div2item;
   GetTestItemList(c_item->item_list, div2item);
   c_item->item_list.clear();
 
   if (div2item.size() > 1) {
     ITERATE (Str2Strs, it, div2item) {
       AddSubcategories(c_item, GetName(), it->second, "bioseq", "division code " + it->first, 
                                                                               false, e_HasComment); 
     }
     c_item->description = "Division code conflicts found.";
   }
};



void CSeqEntry_ONCALLER_BIOPROJECT_ID :: TestOnObj(const CSeq_entry& seq_entry)
{
  unsigned i=0; 
  ITERATE (vector <const CSeqdesc*>, it, user_seqdesc) {
    const CUser_object& user_obj = (*it)->GetUser();
    if (user_obj.GetType().IsStr() 
         && user_obj.GetType().GetStr() == "DBLink"
         && user_obj.HasField("BioProject")
         && user_obj.GetField("BioProject").GetData().IsStrs()) {
         const vector <string>& ids = user_obj.GetField("BioProject").GetData().GetStrs();
         if (!ids.empty() && !ids[0].empty()) {
           if (user_seqdesc_seqentry[i]->IsSeq())
              thisInfo.test_item_list[GetName()].push_back(GetDiscItemText(
                                                          user_seqdesc_seqentry[i]->GetSeq()));
           else AddBioseqsOfSetToReport(user_seqdesc_seqentry[i]->GetSet(), GetName());
         }
    }
    i++;
  }
};



void CSeqEntry_ONCALLER_BIOPROJECT_ID :: GetReport(CRef <CClickableItem>& c_item)
{
  c_item->description 
     = GetOtherComment(c_item->item_list.size(), "sequence contains", "sequences contain") 
        + " BioProject IDs.";
};



void CSeqEntry_INCONSISTENT_BIOSOURCE :: AddAllBioseqToList(const CBioseq_set& bioseq_set, CRef <GeneralDiscSubDt>& item_list, const string& biosrc_txt)
{
  ITERATE (list < CRef < CSeq_entry > >, it, bioseq_set.GetSeq_set()) {
     if ((*it)->IsSeq()) { 
         if ((*it)->GetSeq().IsNa()) {
              item_list->obj_text.push_back( biosrc_txt);
              item_list->obj_text.push_back( GetDiscItemText((*it)->GetSeq()));
         }
     }
     else AddAllBioseqToList( (*it)->GetSet(), item_list, biosrc_txt);
  }
};


bool CSeqEntry_INCONSISTENT_BIOSOURCE :: HasNaInSet(const CBioseq_set& bioseq_set)
{
  bool rvl = false;
  ITERATE (list < CRef < CSeq_entry > >, it, bioseq_set.GetSeq_set()) {
     if ((*it)->IsSeq()) {
        if ( (*it)->GetSeq().IsNa()) return true;
        else return false;
     }
     else rvl = HasNaInSet( (*it)->GetSet() );
  }

  return rvl;
};



CRef <GeneralDiscSubDt> CSeqEntry_INCONSISTENT_BIOSOURCE :: AddSeqEntry(const CSeqdesc& seqdesc, const CSeq_entry& seq_entry)
{
cerr << "seqdesc " << Blob2Str(seqdesc, eSerial_AsnText) << endl;
    CRef < GeneralDiscSubDt > new_biosrc_ls (new GeneralDiscSubDt (kEmptyStr, kEmptyStr));
    new_biosrc_ls->obj_text.clear();

    string biosrc_txt =  GetDiscItemText(seqdesc, seq_entry);
    if (seq_entry.IsSeq()) {
            new_biosrc_ls->obj_text.push_back(biosrc_txt);
            new_biosrc_ls->obj_text.push_back(GetDiscItemText(seq_entry.GetSeq()));
    }
    else AddAllBioseqToList(seq_entry.GetSet(), new_biosrc_ls, biosrc_txt);

    new_biosrc_ls->biosrc = &(seqdesc.GetSource());
    return (new_biosrc_ls);
};



void CSeqEntry_INCONSISTENT_BIOSOURCE :: TestOnObj(const CSeq_entry& seq_entry)
{
   bool found, has_na, is_seq = false;;
   unsigned i=0;
   string biosrc_txt;
   ITERATE (vector <const CSeqdesc*>, it, biosrc_seqdesc) {
      found = has_na = is_seq = false;
      const CSeq_entry* this_seq = biosrc_seqdesc_seqentry[i];
      if (this_seq->IsSeq() && this_seq->GetSeq().IsNa()) {
                has_na = true;
                is_seq = true;
      }
      else has_na = HasNaInSet(this_seq->GetSet());
      if (!has_na) continue;
      NON_CONST_ITERATE(vector <CRef < GeneralDiscSubDt > >,jt,INCONSISTENT_BIOSOURCE_biosrc) {
         if ( BioSourceMatch((*it)->GetSource(), *((*jt)->biosrc)) ) {
            biosrc_txt = GetDiscItemText(**it, *this_seq);
            if (is_seq) {
                  (*jt)->obj_text.push_back(biosrc_txt);
                  (*jt)->obj_text.push_back( GetDiscItemText(this_seq->GetSeq()) );
            }
            else {
              const CBioseq_set& bioseq_set = this_seq->GetSet();
              AddAllBioseqToList(bioseq_set, *jt, biosrc_txt);
            }
            found = true;
            break;
         }
     }
     if (!found) {
          INCONSISTENT_BIOSOURCE_biosrc.push_back(AddSeqEntry(**it, *this_seq));
          if (thisInfo.test_item_list.find(GetName()) == thisInfo.test_item_list.end()
                                   && INCONSISTENT_BIOSOURCE_biosrc.size() > 1)
             thisInfo.test_item_list[GetName()].push_back("inconsistance");
     }
     i++;
   }
};


void CSeqEntry_INCONSISTENT_BIOSOURCE :: GetReport(CRef <CClickableItem>& c_item)
{
   string diff =
            (INCONSISTENT_BIOSOURCE_biosrc.size() > 1)?
              " (" + DescribeBioSourceDifferences(*(INCONSISTENT_BIOSOURCE_biosrc[0]->biosrc),
                                           *(INCONSISTENT_BIOSOURCE_biosrc[1]->biosrc)) + ")."
              : ".";
   c_item->item_list.clear();
   ITERATE (vector <CRef < GeneralDiscSubDt > >, it, INCONSISTENT_BIOSOURCE_biosrc) {
     CRef <CClickableItem> sub_c_item (new CClickableItem);
     sub_c_item->setting_name = GetName();
     sub_c_item->item_list = (*it)->obj_text;
     c_item->item_list.insert(
                 c_item->item_list.end(), (*it)->obj_text.begin(), (*it)->obj_text.end());
     
     sub_c_item->description = GetHasComment(sub_c_item->item_list.size()/2, "contig")
                                 + "identical sources that do not match another contig source.";
     c_item->subcategories.push_back(sub_c_item);
   }
   c_item->description =
       GetOtherComment(c_item->item_list.size()/2, "inconsistent contig source.",
                                                         "inconsistent contig sources") + diff;

   INCONSISTENT_BIOSOURCE_biosrc.clear();
};


void CSeqEntry_DISC_SOURCE_QUALS :: GetQual2SrcIdx(const CBioSource& biosrc, Str2Ints& qual2src_idx, const unsigned& i)
{
    bool fwd_nm, fwd_seq, rev_nm, rev_seq; 
    if ( biosrc.IsSetTaxname() ) qual2src_idx["taxname"].push_back(i);
    if ( biosrc.GetOrg().GetTaxId() ) qual2src_idx["taxid"].push_back(i);

    // add subtypes & orgmods
    if ( biosrc.CanGetSubtype() ) {
       ITERATE (list <CRef <CSubSource> >, jt, biosrc.GetSubtype()) {
         qual2src_idx[ (*jt)->GetSubtypeName((*jt)->GetSubtype()) + "$subsrc" ].push_back(i);
       }
    }
    if ( biosrc.IsSetOrgname() && biosrc.GetOrgname().CanGetMod() ) {
       ITERATE (list <CRef <COrgMod> >, jt, biosrc.GetOrgname().GetMod() ) {
          strtmp = (*jt)->GetSubtypeName((*jt)->GetSubtype());
          if (strtmp != "old-name" 
               && strtmp != "old-lineage"
               && strtmp != "gb-acronym" 
               && strtmp != "gb-anamorph" 
               && strtmp != "gb-synonym")
          qual2src_idx[ (*jt)->GetSubtypeName((*jt)->GetSubtype())].push_back(i);
       }
    }

    // add PCR primers
    if ( biosrc.CanGetPcr_primers() ) {
       fwd_nm = fwd_seq = rev_nm = rev_seq = false;
       ITERATE (list <CRef <CPCRReaction> >, jt, biosrc.GetPcr_primers().Get()) {
          if ( !fwd_nm && !fwd_seq && (*jt)->CanGetForward() ) {
            ITERATE (list <CRef <CPCRPrimer> >, kt, (*jt)->GetForward().Get()) {
               if ( !fwd_nm && (*kt)->CanGetName() ) {
                  strtmp = (*kt)->GetName();
                  if (!strtmp.empty()) {
                      qual2src_idx["fwd_primer_name"].push_back(i);
                      fwd_nm = true;
                  }
               }
               if ( !fwd_seq && (*kt)->CanGetSeq() ) {
                  strtmp = (*kt)->GetSeq();
                  if (!strtmp.empty()) {
                     fwd_seq = true;
                     qual2src_idx["fwd_primer_seq"].push_back(i);
                  }
               }
               if (fwd_nm && fwd_seq) break;
            }
          }
          if ( !rev_nm && !rev_seq && (*jt)->CanGetReverse() ) {
            ITERATE (list <CRef <CPCRPrimer> >, kt, (*jt)->GetReverse().Get()) {
               if (!rev_nm && (*kt)->CanGetName() ) {
                  strtmp = (*kt)->GetName();
                  if (!strtmp.empty()) {
                      rev_nm = true;
                      qual2src_idx["rev_primer_name"].push_back(i);
                  }
               }
               if (!rev_seq && (*kt)->CanGetSeq() ) {
                  strtmp = (*kt)->GetSeq();
                  if (!strtmp.empty()) {
                      rev_seq = true;
                      qual2src_idx["rev_primer_seq"].push_back(i);
                  }
               }
               if (rev_nm && rev_seq) break;
            }
          }
          if (fwd_nm && fwd_seq && rev_nm && rev_seq) break;
       }
    }

    // genomic
    if (biosrc.GetGenome() != CBioSource::eGenome_unknown) 
              qual2src_idx["location"].push_back(i); 
} // GetQual2SrcIdx


void CSeqEntry_DISC_SOURCE_QUALS :: TestOnObj(const CSeq_entry& seq_entry)
{
  unsigned i=0;
  ITERATE ( vector <const CSeq_feat*>, it, biosrc_feat) 
     GetQual2SrcIdx((*it)->GetData().GetBiosrc(), qual2src_idx_feat, i++);
  i=0;
  ITERATE ( vector <const CSeqdesc*>, it, biosrc_seqdesc)
    GetQual2SrcIdx((*it)->GetSource(), qual2src_idx_seqdesc, i++);

  if (!qual2src_idx_feat.empty() || !qual2src_idx_seqdesc.empty())
      thisInfo.test_item_list[GetName()].push_back(GetName());   // temp
};


string CSeqEntry_DISC_SOURCE_QUALS :: GetSubSrcValue(const CBioSource& biosrc, const string& type_name)
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
           return ("TRUE");
        }
        else return (strtmp);
     }
  }
  return (kEmptyStr);
};


void CSeqEntry_DISC_SOURCE_QUALS :: GetMultiSubSrcVlus(const CBioSource& biosrc, const string& type_name, vector <string>& multi_vlus)
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



string CSeqEntry_DISC_SOURCE_QUALS :: GetOrgModValue(const CBioSource& biosrc, const string& type_name)
{
   ITERATE (list <CRef <COrgMod> >, it, biosrc.GetOrgname().GetMod() )
      if ((*it)->GetSubtypeName((*it)->GetSubtype()) == type_name) 
                return ((*it)->GetSubname());
   return (kEmptyStr);
};



void CSeqEntry_DISC_SOURCE_QUALS :: GetMultiOrgModVlus(const CBioSource& biosrc, const string& type_name, vector <string>& multi_vlus)
{
   ITERATE (list <CRef <COrgMod> >, it, biosrc.GetOrgname().GetMod() )
      if ((*it)->GetSubtypeName((*it)->GetSubtype()) == type_name) 
          multi_vlus.push_back((*it)->GetSubname());
};



void CSeqEntry_DISC_SOURCE_QUALS :: GetMultiPrimerVlus(const CBioSource& biosrc, const string& qual_name, vector <string>& multi_vlus)
{
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



string CSeqEntry_DISC_SOURCE_QUALS :: GetSrcQualValue(const string& qual_name, const int& cur_idx, bool is_subsrc)
{
 const CBioSource& biosrc = biosrc_seqdesc[cur_idx]->GetSource();
 string ret_str(kEmptyStr);
 // DoesStringMatchConstraint missing
 if (is_subsrc) ret_str = GetSubSrcValue(biosrc, qual_name);
 else if (qual_name == "location") ret_str = biosrc.GetOrganelleByGenome(biosrc.GetGenome());
 else if (qual_name == "taxname") ret_str = biosrc.GetTaxname();
 else if (qual_name == "common_name") ret_str = biosrc.GetCommon();
 else if (qual_name == "lineage") ret_str = biosrc.GetLineage();
 else if (qual_name == "div") ret_str = biosrc.GetDivision();
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
 else ret_str = GetOrgModValue(biosrc, qual_name);
 
 return (ret_str);
};


void CSeqEntry_DISC_SOURCE_QUALS :: GetMultiQualVlus(const string& qual_name, const CBioSource& biosrc, vector <string>& multi_vlus, bool is_subsrc)
{
   ITERATE (vector <string>, it, thisInfo.no_multi_qual) if (qual_name == *it) return;

   if (is_subsrc) GetMultiSubSrcVlus(biosrc, qual_name, multi_vlus);
   if (qual_name == "fwd_primer_name" || qual_name == "fwd_primer_seq"
             || qual_name == "rev_primer_name" || qual_name == "rev_primer_seq" ) 
      GetMultiPrimerVlus(biosrc, qual_name, multi_vlus);
   else GetMultiOrgModVlus(biosrc, qual_name, multi_vlus);
}



void CSeqEntry_DISC_SOURCE_QUALS :: CheckForMultiQual(const string& qual_name, const CBioSource& biosrc, eMultiQual& multi_type, bool is_subsrc)
{
   vector <string> multi_qual_vlus;
   Str2Int  vlu_cnt;
   GetMultiQualVlus(qual_name, biosrc, multi_qual_vlus, is_subsrc);
   if (multi_qual_vlus.size() <= 1) multi_type = e_not_multi;
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


void CSeqEntry_DISC_SOURCE_QUALS :: GetQualDistribute(const Str2Ints& qual2src_idx, bool isFromFeat)
{
  int pre_idx, cur_idx, i;
  string qual_name, src_qual_vlu, biosrc_txt;
  bool is_subsrc;
  size_t pos;

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
       if (pre_idx +1 != cur_idx) {  // have missing
          if (qual_name != "location") {
             if (isFromFeat) 
                 for ( i = pre_idx+1; i < cur_idx; i++) 
                      qual_nm2qual_vlus[qual_name].missing_item.push_back(
                                                           GetDiscItemText(*(biosrc_feat[i])));
             else for (i = pre_idx+1; i < cur_idx; i++)
                      qual_nm2qual_vlus[qual_name].missing_item.push_back(
                          GetDiscItemText(*(biosrc_seqdesc[i]),*(biosrc_seqdesc_seqentry[i])));
          }
          else {
            if (isFromFeat)
                 for (i = pre_idx+1; i < cur_idx; i++) 
                     qual_nm2qual_vlus[qual_name].qual_vlu2src["genomic"].push_back(
                                                       GetDiscItemText(*(biosrc_feat[i])));
            else for (i = pre_idx+1; i < cur_idx; i++) 
                   qual_nm2qual_vlus[qual_name].qual_vlu2src["genomic"].push_back(
                        GetDiscItemText(*(biosrc_seqdesc[i]), *(biosrc_seqdesc_seqentry[i])));
          }
       }
       src_qual_vlu= GetSrcQualValue(qual_name, cur_idx, is_subsrc); 
       biosrc_txt = 
         (isFromFeat ? GetDiscItemText(*(biosrc_feat[cur_idx]))
                     : GetDiscItemText(*(biosrc_seqdesc[cur_idx]), 
                                                  *(biosrc_seqdesc_seqentry[cur_idx])));
       qual_nm2qual_vlus[qual_name].qual_vlu2src[src_qual_vlu].push_back(biosrc_txt);

       // have multiple qualifiers? 
       eMultiQual multi_type;
       if (isFromFeat) 
          CheckForMultiQual(qual_name, biosrc_feat[cur_idx]->GetData().GetBiosrc(), 
                                                                       multi_type, is_subsrc);
       else CheckForMultiQual(qual_name, biosrc_seqdesc[cur_idx]->GetSource(), 
                                                                       multi_type,is_subsrc);

       switch (multi_type) {
          case e_same:     qual_nm2qual_vlus[qual_name].multi_same.push_back(biosrc_txt);
                           break;
          case e_dup:      qual_nm2qual_vlus[qual_name].multi_dup.push_back(biosrc_txt); break;
          case e_all_dif:  qual_nm2qual_vlus[qual_name].multi_all_dif.push_back(biosrc_txt); 
                           break;
          default: break;
       }

       // biosrc_val_qualnm:
       if (biosrc2qualvlu_nm.find(biosrc_txt) == biosrc2qualvlu_nm.end()) {
          Str2Strs vlu_nm;
          vlu_nm[src_qual_vlu].push_back(qual_name);
	  biosrc2qualvlu_nm[biosrc_txt] = vlu_nm;
       }
       else {
         Str2Strs& vlu_nm = biosrc2qualvlu_nm[biosrc_txt];
         vlu_nm[src_qual_vlu].push_back(qual_name);
       }

       pre_idx = cur_idx;
     }

     if (qual_name != "location") {
         if (isFromFeat)
           for (i = cur_idx +1; i < (int)biosrc_feat.size(); i++)
              qual_nm2qual_vlus[qual_name].missing_item.push_back(
                                                          GetDiscItemText(*(biosrc_feat[i])));
         else for (i = cur_idx +1; i < (int)biosrc_seqdesc.size(); i++)
                  qual_nm2qual_vlus[qual_name].missing_item.push_back(
                         GetDiscItemText(*(biosrc_seqdesc[i]), *(biosrc_seqdesc_seqentry[i])));
     }
     else {
        if (isFromFeat) 
           for (i = cur_idx +1; i < (int)biosrc_feat.size(); i++)
              qual_nm2qual_vlus[qual_name].qual_vlu2src["genomic"].push_back(
                                                          GetDiscItemText(*(biosrc_feat[i])));
        else for (i = cur_idx +1; i < (int)biosrc_seqdesc.size(); i++)
                  qual_nm2qual_vlus[qual_name].qual_vlu2src["genomic"].push_back(
                         GetDiscItemText(*(biosrc_seqdesc[i]), *(biosrc_seqdesc_seqentry[i])));
     }
  }
}


CRef <CClickableItem> CSeqEntry_DISC_SOURCE_QUALS :: MultiItem(const string& qual_name, const vector <string>& multi_list, const string& ext_desc)
{
    CRef <CClickableItem> c_sub (new CClickableItem);
    c_sub->setting_name = GetName();
    c_sub->description = "  " + GetHasComment(multi_list.size(), "source") + "multiple " 
                         + qual_name + " qualifiers" + ext_desc;
    if (isOncaller) c_sub->item_list = multi_list;
    return (c_sub);
}


void CSeqEntry_DISC_SOURCE_QUALS :: GetReport(CRef <CClickableItem>& c_item)
{
   GetQualDistribute(qual2src_idx_feat, true);
   GetQualDistribute(qual2src_idx_seqdesc);

   bool first_c_item = false, all_same, all_unique, all_present;
   bool multi_same, multi_dup, multi_all_dif;
   string qual_name;
   unsigned multi_type_cnt;
   CRef <CClickableItem> c_main;

   if (isOncaller) {
     c_main = c_item;
     c_main->setting_name = GetName();
     c_main->description = "Source Qualifer Report";
     c_main->item_list.clear();
   }
   else first_c_item = true;

   ITERATE (Str2QualVlus, it, qual_nm2qual_vlus) {
     if (!first_c_item) c_item = CRef <CClickableItem> (new CClickableItem);
     qual_name = it->first;
     if (it->second.qual_vlu2src.size() == 1 
           && it->second.qual_vlu2src.begin()->second.size() > 1) all_same = true;
     else {
        all_same = false;
        all_unique = true;
        ITERATE (Str2Strs, jt, it->second.qual_vlu2src) {
          if ( jt->second.size() > 1) {
            all_unique = false; break;
          }
        }
     }

     all_present = (it->second.missing_item.empty()) ? true : false;

     multi_same = (it->second.multi_same.empty()) ? false : true;
     multi_dup = (it->second.multi_dup.empty()) ? false : true;
     multi_all_dif= (it->second.multi_all_dif.empty()) ? false : true;

     multi_type_cnt = 0;
     multi_type_cnt += multi_same ? 1 : 0;
     multi_type_cnt += multi_dup ? 1 : 0;
     multi_type_cnt += multi_all_dif? 1 : 0;

     c_item->setting_name = GetName();
     if (qual_name == "note") qual_name += "-subsrc";
     else if (qual_name == "nat-host") qual_name = "host";
     c_item->description = qual_name;
     if (all_present) {
         c_item->description += " (all present, ";
         if (all_same) {
            c_item->description += "all same";

            CRef <CClickableItem> c_sub (new CClickableItem);
            c_sub->setting_name = GetName();
            c_sub->description 
                = GetHasComment( (it->second.qual_vlu2src.begin()->second).size(), "source")
                   + " '" + it->second.qual_vlu2src.begin()->first + "' for " + qual_name;
            c_item->subcategories.push_back(c_sub);
            if (isOncaller) c_sub->item_list = it->second.qual_vlu2src.begin()->second;
         }
         else if (all_unique) {
                 c_item->description += "all_unique";
                 if (isOncaller) {
                    ITERATE (Str2Strs, jt, it->second.qual_vlu2src)
                       c_item->item_list.push_back((jt->second)[0]);
                 }
         }
         else {
           c_item->description += "some duplicate";
           unsigned uni_cnt=0;
           vector <string> uni_sub;
           ITERATE (Str2Strs, jt, it->second.qual_vlu2src) {
             unsigned sz = (jt->second).size();
             if (sz > 1) {
               CRef <CClickableItem> c_sub (new CClickableItem);
               c_sub->setting_name = GetName();
               c_sub->description =
                  GetHasComment(sz, "source") + "'" + jt->first + "' for " + qual_name;
               c_item->subcategories.push_back(c_sub);
               if (isOncaller) c_sub->item_list = jt->second; 
             }
             else  {
               uni_cnt ++;
               if (isOncaller) uni_sub.push_back((jt->second)[0]);
             }
           }
           if (uni_cnt) {
             CRef <CClickableItem> c_sub (new CClickableItem);
             c_sub->setting_name = GetName();
             c_sub->description = GetHasComment(uni_cnt, "source") + "unique "
                                    + (uni_cnt >1 ? "values" : "value") + " for " + qual_name;
             c_item->subcategories.push_back(c_sub);
             if (isOncaller) c_sub->item_list = uni_sub;
          }
         }
     }
     else {
        c_item->description += " (some missing, ";
        CRef <CClickableItem> c_sub (new CClickableItem);
        c_sub->setting_name = GetName();
        c_sub->description = 
             GetIsComment(it->second.missing_item.size(), "source") + "missing " + qual_name;
        c_item->subcategories.push_back(c_sub);
        if (isOncaller) c_sub->item_list = it->second.missing_item;

        if (all_same) {
            c_item->description += "all same";
            CRef <CClickableItem> c_sub (new CClickableItem);
            c_sub->setting_name = GetName();
            c_sub->description 
                = GetHasComment( (it->second.qual_vlu2src.begin()->second).size(), "source")
                   + " '" + it->second.qual_vlu2src.begin()->first + "' for " + qual_name;
            c_item->subcategories.push_back(c_sub);
            if (isOncaller) c_sub->item_list = it->second.qual_vlu2src.begin()->second;
        }
        else if (all_unique) {
           c_item->description += "all unique"; 
           CRef <CClickableItem> c_sub (new CClickableItem);
           unsigned uni_cnt = 0;
           ITERATE (Str2Strs, jt, it->second.qual_vlu2src) 
              if (jt->second.size() == 1) {
                 uni_cnt ++;
                 if (isOncaller) c_sub->item_list.push_back((jt->second)[0]);
              }
           c_sub->setting_name = GetName();
           c_sub->description = GetHasComment(uni_cnt, "source") + "unique " 
                                    + (uni_cnt >1 ? "values" : "value") + " for " + qual_name;
           c_item->subcategories.push_back(c_sub);
        }
        else {
           c_item->description += "some duplicate";
           unsigned uni_cnt=0, sz;
           vector <string> uni_sub;
           ITERATE (Str2Strs, jt, it->second.qual_vlu2src) {
             sz = (jt->second).size();
             if (sz > 1) {
               CRef <CClickableItem> c_sub (new CClickableItem);
               c_sub->setting_name = GetName();
               c_sub->description =
                  GetHasComment(sz, "source") + "'" + jt->first + "' for " + qual_name;
               c_item->subcategories.push_back(c_sub);
               if (isOncaller) c_sub->item_list = jt->second;
             }
             else  {
                 uni_cnt ++;
                 uni_sub.push_back((jt->second)[0]);
             }
           }
           if (uni_cnt) {
             CRef <CClickableItem> c_sub (new CClickableItem);
             c_sub->setting_name = GetName();
             c_sub->description = GetHasComment(uni_cnt, "source") + "unique "
                                    + (uni_cnt >1 ? "values" : "value") + " for " + qual_name;
             c_item->subcategories.push_back(c_sub);
             if (isOncaller) c_sub->item_list = uni_sub;
          }
        }
     }

     //multiple
     unsigned sz = 
        it->second.multi_same.size() + it->second.multi_dup.size() + it->second.multi_all_dif.size();
     string ext_desc;
     if ( multi_type_cnt) {
          c_item->description += ", some multi";
          CRef <CClickableItem> c_sub (new CClickableItem);
          c_sub->setting_name = GetName();
          if (multi_type_cnt == 1) 
              ext_desc = multi_same ? ", same_value" 
                                       : (multi_dup ? ", some dupplicates" : kEmptyStr);
          else {
              ext_desc = kEmptyStr;
              if (multi_same) {
                 c_sub->subcategories.push_back(MultiItem(
                                          qual_name, it->second.multi_same, ", same value"));
              }
              if (multi_dup) 
                 c_sub->subcategories.push_back(MultiItem(
                                       qual_name, it->second.multi_dup, ", some duplicates"));
              if (multi_all_dif) 
                 c_sub->subcategories.push_back(MultiItem(
                                                 qual_name, it->second.multi_all_dif, kEmptyStr));
          }
          c_sub->description = 
                 GetHasComment(sz, "source") + "multiple " + qual_name + " qualifiers" + ext_desc;
          c_item->subcategories.push_back(c_sub);
     }
     
     c_item->description += ")";
     if (isOncaller) c_main->subcategories.push_back(c_item);
     else {
        thisInfo.disc_report_data.push_back(c_item);
        if (first_c_item) {
           c_item->item_list.clear();
           first_c_item = false;
        }
     }
   }
   if (isOncaller) thisInfo.disc_report_data.push_back(c_main);

 // biosrc has 2 qual with same value?
    string biosrc_nm, quals;
    unsigned cnt = 0;
    vector < CRef <CClickableItem> > sub;
    ITERATE (Str2MapStr2Strs, it, biosrc2qualvlu_nm) {
         biosrc_nm = it->first;
         ITERATE (Str2Strs, jt, it->second) {
           if (jt->second.size() > 1) {
              cnt ++;
              quals = NStr::Join(jt->second, ", ");
              CRef <CClickableItem> c_sub (new CClickableItem);
              c_sub->setting_name = GetName();
              c_sub->description = 
                "BioSource has value '" + jt->first + "' for these qualifiers: " + quals;
              sub.push_back(c_sub);
              if (isOncaller) c_sub->item_list.push_back(biosrc_nm);
           }
         }
    }
 
    if (!sub.empty()) {
         c_item = CRef <CClickableItem> (new CClickableItem);
         c_item->setting_name = GetName();
         c_item->description = GetHasComment(cnt, "source") + "two or more qualifiers with the same value";
         c_item->item_list.push_back(biosrc_nm);
         c_item->subcategories = sub;
         thisInfo.disc_report_data.push_back(c_item);
    }

// clean
};





void CSeqEntry_DISC_BACTERIAL_TAX_STRAIN_MISMATCH :: BacterialTaxShouldEndWithStrain(const CBioSource& biosrc, const CSeq_feat* seq_feat, const CSeqdesc* seqdesc, const CSeq_entry* seq_entry)
{
    if ( HasLineage(biosrc, "Bacteria") && biosrc.GetOrg().CanGetTaxname()) {
      string taxname = biosrc.GetOrg().GetTaxname();
      if (taxname.empty()) return;
      string subname;
      ITERATE (list <CRef <COrgMod> >, it, biosrc.GetOrgname().GetMod()) {
        if ( (*it)->GetSubtype() == COrgMod::eSubtype_strain) {
           subname = (*it)->GetSubname();
           size_t pos = taxname.find(subname);
           if (pos == string::npos || subname.size() != (taxname.size() - pos + 1)) {
             thisInfo.test_item_list[GetName()].push_back(
                (seq_feat? GetDiscItemText(*seq_feat) : GetDiscItemText(*seqdesc,*seq_entry)));
           }
        }
      }
    }
};


void CSeqEntry_DISC_BACTERIAL_TAX_STRAIN_MISMATCH :: TestOnObj(const CSeq_entry& seq_entry)
{
   ITERATE (vector <const CSeq_feat*>, it, biosrc_orgmod_feat)
     BacterialTaxShouldEndWithStrain((*it)->GetData().GetBiosrc(), *it);

   unsigned i=0;
   ITERATE (vector <const CSeqdesc*>, it, biosrc_orgmod_seqdesc)
     BacterialTaxShouldEndWithStrain((*it)->GetSource(), 0, *it, biosrc_orgmod_seqdesc_seqentry[i++]);
};



void CSeqEntry_DISC_BACTERIAL_TAX_STRAIN_MISMATCH :: GetReport(CRef <CClickableItem>& c_item)
{
   c_item->description = GetHasComment(c_item->item_list.size(), "biosource") + "tax name/strain mismatch.";
};



void  CSeqEntry_ONCALLER_DEFLINE_ON_SET :: TestOnObj(const CSeq_entry& seq_entry)
{
    unsigned i=0; 
    ITERATE ( vector <const CSeqdesc*>, it, title_seqdesc)
       thisInfo.test_item_list[GetName()].push_back( 
                                    GetDiscItemText(**it, *(title_seqdesc_seqentry[i++])) );
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

   if (thisInfo.expand_defline_on_set == "true" || thisInfo.expand_defline_on_set == "TRUE")
     c_item->expanded = true;
};



void CSeqEntry_ONCALLER_STRAIN_CULTURE_COLLECTION_MISMATCH :: BiosrcHasConflictingStrainAndCultureCollectionValues(const CBioSource& biosrc, const CSeq_feat* seq_feat, const CSeqdesc* seqdesc, const CSeq_entry* seq_entry)
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
               extra_str = "        " + (*it)->GetSubtypeName( (*it)->GetSubtype(), 
                                                     COrgMod::eVocabulary_insdc ) + ": "
                                + (*it)->GetSubname() + "\n        " 
                                + (*jt)->GetSubtypeName( (*jt)->GetSubtype(),
                                                     COrgMod::eVocabulary_insdc ) + ": "
                                + (*jt)->GetSubname();
               has_conflict = true;
            }
         } 
       };
       if (has_match) break; 
     } 
   }
   if (has_conflict && !has_match)
       thisInfo.test_item_list[GetName()].push_back(
                (seq_feat? GetDiscItemText(*seq_feat):GetDiscItemText(*seqdesc,*seq_entry)) 
                 + "\n" + extra_str);
};



void CSeqEntry_ONCALLER_STRAIN_CULTURE_COLLECTION_MISMATCH :: TestOnObj(const CSeq_entry& seq_entry)
{
   ITERATE (vector <const CSeq_feat*>, it, biosrc_orgmod_feat) 
     BiosrcHasConflictingStrainAndCultureCollectionValues((*it)->GetData().GetBiosrc(), *it);

   unsigned i=0;
   ITERATE (vector <const CSeqdesc*>, it, biosrc_orgmod_seqdesc)
     BiosrcHasConflictingStrainAndCultureCollectionValues((*it)->GetSource(), 0, *it, 
                                                        biosrc_orgmod_seqdesc_seqentry[i++]);
};



void CSeqEntry_ONCALLER_STRAIN_CULTURE_COLLECTION_MISMATCH :: GetReport(CRef <CClickableItem>& c_item)
{
   c_item->description = GetHasComment(c_item->item_list.size(), "organism") 
                          + "conflicting strain and culture-collection values";
};



bool CSeqEntryTestAndRepData :: HasConflict(const list <CRef <COrgMod> >& mods, const string& subname_rest, const COrgMod::ESubtype& check_type, const string& check_head)
{
  size_t pos;
  unsigned h_size = check_head.size();
  ITERATE (list <CRef <COrgMod> >, it, mods) {
     strtmp = (*it)->GetSubname();
     if ( (*it)->GetSubtype() == check_type && strtmp.substr(0, h_size) == check_head ) {
        if ( (pos = strtmp.find(';')) != string::npos) {
           if ( strtmp.substr(h_size, pos-h_size) == subname_rest ) return false;  // have to test
        }
        else if (strtmp.substr(h_size) == subname_rest) return false;
     }
  }
  return true;
}



bool CSeqEntryTestAndRepData :: IsStrainInCultureCollectionForBioSource(const CBioSource& biosrc, const string& strain_head, const string& culture_head) 
{
  COrgMod::ESubtype check_type;
  string check_head(kEmptyStr), subname_rest(kEmptyStr);
  const list < CRef < COrgMod > >& mods = biosrc.GetOrgname().GetMod();
  ITERATE (list <CRef <COrgMod> >, it, mods) {
    if ( (*it)->GetSubtype() == COrgMod::eSubtype_strain 
                            && (*it)->GetSubname().substr(0, strain_head.size()) == strain_head) {
       check_type = COrgMod::eSubtype_culture_collection;
       check_head = culture_head;
       subname_rest = (*it)->GetSubname().substr(strain_head.size());
    }
    else if ( (*it)->GetSubtype() == COrgMod::eSubtype_culture_collection
                                   && (*it)->GetSubname().substr(0, culture_head.size()) == culture_head) {
       check_type = COrgMod::eSubtype_strain;
       check_head = strain_head;
       subname_rest = (*it)->GetSubname().substr(culture_head.size());
    }

    if (!check_head.empty()&& !subname_rest.empty() && HasConflict(mods, subname_rest, check_type, check_head) ) 
         return true;
  }

  return false; 
};


void CSeqEntryTestAndRepData :: CheckBioSourceWithStrainCulture(const string& strain_head, const string& culture_head,
const string& test_setting_name)
{
   ITERATE (vector <const CSeq_feat*>, it, biosrc_orgmod_feat) {
     if (IsStrainInCultureCollectionForBioSource((*it)->GetData().GetBiosrc(), strain_head, culture_head))
        thisInfo.test_item_list[test_setting_name].push_back( GetDiscItemText(**it) );
   }

   unsigned i=0;
   ITERATE (vector <const CSeqdesc*>, it, biosrc_orgmod_seqdesc) {
      if (IsStrainInCultureCollectionForBioSource((*it)->GetSource(), strain_head, culture_head))
        thisInfo.test_item_list[test_setting_name].push_back(
                               GetDiscItemText(**it, *(biosrc_orgmod_seqdesc_seqentry[i])));
      i++;
   }
};


void CSeqEntry_DUP_DISC_CBS_CULTURE_CONFLICT :: TestOnObj(const CSeq_entry& seq_entry)
{
   CheckBioSourceWithStrainCulture("CBS ", "CBS:", GetName());
};



void CSeqEntry_DUP_DISC_CBS_CULTURE_CONFLICT :: GetReport(CRef <CClickableItem>& c_item)
{
   c_item->description = GetHasComment(c_item->item_list.size(), "biosource")
                          + "conflicting CBS strain and culture collection values.";
};


void CSeqEntry_DUP_DISC_ATCC_CULTURE_CONFLICT :: TestOnObj(const CSeq_entry& seq_entry)
{
   CheckBioSourceWithStrainCulture("ATCC ", "ATCC:", GetName());
};



void CSeqEntry_DUP_DISC_ATCC_CULTURE_CONFLICT :: GetReport(CRef <CClickableItem>& c_item)
{
   c_item->description = GetHasComment(c_item->item_list.size(), "biosource")
                          + "conflicting ATCC strain and culture collection values.";
};



void CSeqEntry_DISC_FEATURE_COUNT :: TestOnObj(const CSeq_entry& seq_entry)
{  
   typedef map <string, int > Str2Int;
   Str2Int feat_count_list;
   Str2Int :: iterator it;
   CSeq_entry_Handle seq_entry_hl = thisInfo.scope->GetSeq_entryHandle(seq_entry);
   SAnnotSelector sel;
   sel.ExcludeFeatSubtype(CSeqFeatData::eSubtype_prot);
   for (CFeat_CI feat_it(seq_entry_hl, sel); feat_it; ++feat_it) { // rey non_prot_feat
      const CSeq_feat& seq_feat = (feat_it->GetOriginalFeature());
      const CSeqFeatData& seq_feat_dt = seq_feat.GetData();
      strtmp = seq_feat_dt.GetKey(CSeqFeatData::eVocabulary_genbank);
      it = feat_count_list.find(strtmp);
      if (it == feat_count_list.end())  feat_count_list[strtmp] = 1;
      else it->second ++;
/*
          CBioseq_Handle bioseq_h = GetBioseqFromSeqLoc(seq_feat.GetLocation(),
                                                      *thisInfo.scope);
          CConstRef <CBioseq> bioseq = bioseq_h.GetCompleteBioseq();
          if (bioseq.NotEmpty()) 
              feat_bioseq_list[seq_feat_dt.which()].push_back(bioseq);
*/
   }

   ITERATE (Str2Int, it, feat_count_list) 
     thisInfo.test_item_list[GetName()+"$" + it->first].push_back(NStr::UIntToString(it->second));
};



void CSeqEntry_DISC_FEATURE_COUNT :: GetReport(CRef <CClickableItem>& c_item)
{
   size_t pos;
   unsigned cnt;
   strtmp = GetName() + "$";
   ITERATE (Str2Strs, it, thisInfo.test_item_list) {
     if ( it->first.find(strtmp) != string::npos) {
        pos = it->first.find("$");
        c_item->setting_name = GetName();
        c_item->item_list.clear();
        cnt = NStr::StringToUInt(((it->second)[0]));
        c_item->description = 
             it->first.substr(pos+1) + ": " + NStr::UIntToString(cnt) + ((cnt>1)? " present" : " presents");
        thisInfo.disc_report_data.push_back(c_item);
        c_item = CRef <CClickableItem> (new CClickableItem);
     }
   }
};



void CSeqEntry_ONCALLER_COMMENT_PRESENT :: TestOnObj(const CSeq_entry& seq_entry)
{
   unsigned i=0;
   ITERATE (vector <const CSeqdesc*>, it, comm_seqdesc) {
     if (all_same && comm_seqdesc[0]->GetComment() != (*it)->GetComment()) {
          all_same  = false; 
     } 
     thisInfo.test_item_list[GetName()].push_back(GetDiscItemText(**it, *(comm_seqdesc_seqentry[i++])));
   }
};


void CSeqEntry_ONCALLER_COMMENT_PRESENT :: GetReport(CRef <CClickableItem>& c_item)
{
    c_item->description = GetOtherComment(c_item->item_list.size(), 
                                          "comment descriptor was found", 
                                          "comment descriptors were found") 
        + ((c_item->item_list.size() > 1 && all_same)? " (all same)." : " (some different). ");
};



bool CSeqEntry_DISC_CHECK_AUTH_CAPS :: IsNameCapitalizationOk(const string& name)
{
  if (name.empty()) return (true);

  vector <string> short_nms;
  short_nms.reserve(12);
  short_nms.push_back("de la");
  short_nms.push_back("del");
  short_nms.push_back("de");
  short_nms.push_back("da");
  short_nms.push_back("du");
  short_nms.push_back("dos");
  short_nms.push_back("la");
  short_nms.push_back("le");
  short_nms.push_back("van");
  short_nms.push_back("von");
  short_nms.push_back("der");
  short_nms.push_back("den");
  short_nms.push_back("di");

  unsigned    i;
  bool need_cap = true, rval = true, found;
  bool needed_lower = false, found_lower = false;

  size_t pos;
  pos = 0;
  while (pos < name.size() && rval) {
    if (isalpha (name[pos])) {
      if (!need_cap) {
          needed_lower = true;
          if (!isupper(name[pos])) found_lower = true;
      }

      /* check to see if this is a short name */
      if (need_cap && !isupper (name[pos])) {
        if (!pos || name[pos-1] == ' ') {
          found = false;
          for (i = 0; i < short_nms.size(); i++) {
            if (name.size() > short_nms.size()
                 && string::npos != (pos = name.find(short_nms[i]))
                 && !pos
                 && name[short_nms[i].size()] == ' ') {
                found = true;
                pos += short_nms[i].size() - 1;   // in order to set need_cap correctly
                break;
            }
          }

          if (!found) rval = false;
        } 
        else rval = false;
      }

      need_cap = false;
    } 
    else need_cap = true;
    pos++;
  }
  if (needed_lower && !found_lower) rval = false;

  return rval;

}; //IsNameCapitalizationOK


bool CSeqEntry_DISC_CHECK_AUTH_CAPS :: IsAuthorInitialsCapitalizationOk(const string& nm_init)
{
   strtmp = nm_init;
   size_t pos = 0;
   while (pos != nm_init.size()) {
      if (isalpha(nm_init[pos]) && !isupper(nm_init[pos])) return false;
      else pos ++;
   }
   return true;
};  // IsAuthorInitialsCapitalizationOk



bool CSeqEntry_DISC_CHECK_AUTH_CAPS :: NameIsBad(CRef <CAuthor> nm_std) 
{
   const CPerson_id& pid = nm_std->GetName();
   if (pid.IsName()) {
       const CName_std& name_std = pid.GetName();
       if ( !CSeqEntry_DISC_CHECK_AUTH_CAPS ::IsNameCapitalizationOk(name_std.GetLast()) ) 
              return true;
       else if ( name_std.CanGetFirst() && 
                 !CSeqEntry_DISC_CHECK_AUTH_CAPS ::IsNameCapitalizationOk(name_std.GetFirst())) 
              return true;
       else if ( name_std.CanGetInitials() && 
                   !CSeqEntry_DISC_CHECK_AUTH_CAPS ::
                                   IsAuthorInitialsCapitalizationOk(name_std.GetInitials())) 
               return true;
   }

   return false;
}

bool CSeqEntry_DISC_CHECK_AUTH_CAPS :: HasBadAuthorName(CAuth_list& auths)
{
  CRef <CAuthor> w_auth (new CAuthor);
  CRef <CAuth_list::C_Names> w_names (new CAuth_list::C_Names);

  CAuth_list::C_Names& names = auths.SetNames();
  if (names.IsStd()) {
    list < CRef <CAuthor> >& nm_stds = names.SetStd();
    nm_stds.remove_if(not1(ptr_fun(CSeqEntry_DISC_CHECK_AUTH_CAPS :: NameIsBad)));

    if (nm_stds.empty()) return false;
    else return true;
  }
  return false;
 
}; // HasBadAuthorName 



bool CSeqEntry_DISC_CHECK_AUTH_CAPS :: AreBadAuthCapsInPubdesc(CPubdesc& pubdesc)
{
  bool isBad = false;
  CRef <CAuth_list> w_auth_ls (new CAuth_list);
  CRef <CPub> w_pub (new CPub);

  list < CRef < CPub > > pubs = pubdesc.SetPub().Set();
  NON_CONST_ITERATE (CPub_equiv::Tdata, it, pubs) {
   if (isBad) break;

   if ( (*it)->IsPmid() || (*it)->IsMuid()) continue;
   if ( (*it)->IsGen()) {
      if ((*it)->SetGen().IsSetAuthors()) 
              isBad = HasBadAuthorName((*it)->SetGen().SetAuthors());
   }
   else if ( (*it)->IsSub()) {
          isBad = HasBadAuthorName( (*it)->SetSub().SetAuthors());
   }
   else if ( (*it)->IsArticle()) {
      CCit_art& cit_art = (*it)->SetArticle();
      if ( cit_art.IsSetAuthors()) isBad = HasBadAuthorName(cit_art.SetAuthors());
      if (cit_art.SetFrom().IsBook()) 
          isBad = HasBadAuthorName(cit_art.SetFrom().SetBook().SetAuthors());
      else if (cit_art.SetFrom().IsProc())
            isBad = HasBadAuthorName(cit_art.SetFrom().SetProc().SetBook().SetAuthors());
   }
   else if ( (*it)->IsBook()) isBad = HasBadAuthorName( (*it)->SetBook().SetAuthors());
   else if ( (*it)->IsMan()) {
           isBad = HasBadAuthorName( (*it)->SetMan().SetCit().SetAuthors());
           
   }
   else if ( (*it)->IsPatent()) {
         CCit_pat& patent = (*it)->SetPatent();
         isBad = HasBadAuthorName( patent.SetAuthors());
         isBad = HasBadAuthorName( patent.SetApplicants());
         isBad = HasBadAuthorName( patent.SetAssignees());
   } 
  }

  return isBad;

}; //AreBadAuthCapsInPubdesc



bool CSeqEntry_DISC_CHECK_AUTH_CAPS :: AreAuthCapsOkInSubmitBlock( CSubmit_block& submit_block)
{
   return ( !(HasBadAuthorName(submit_block.SetCit().SetAuthors())) );
 
}; // AreAuthCapsOkInSubmitBlock()


void CSeqEntry_DISC_CHECK_AUTH_CAPS :: TestOnObj(const CSeq_entry& seq_entry)
{
   CSeqdesc this_seqdesc;
   CSeq_feat this_seqfeat;
   ITERATE (vector < const CSeq_feat* >, it, pub_feat) {
      this_seqfeat.Reset();
      this_seqfeat.Assign(**it);
      this_seqdesc.SetPub(this_seqfeat.SetData().SetPub());
      if (AreBadAuthCapsInPubdesc(this_seqdesc.SetPub())) {
         thisInfo.test_item_list[GetName()].push_back(GetDiscItemText(const_cast<const CSeq_feat&>(this_seqfeat)));
      }
   }

   unsigned i = 0;
   ITERATE (vector < const CSeqdesc* >, it, pub_seqdesc) {
      this_seqdesc.Reset();
      this_seqdesc.Assign(**it);
      if (AreBadAuthCapsInPubdesc(this_seqdesc.SetPub())) {
          thisInfo.test_item_list[GetName()].push_back(
                        GetDiscItemText(const_cast <const CSeqdesc&>(this_seqdesc), *(pub_seqdesc_seqentry[i++])));
      }
   }

   CSubmit_block this_submit_blk;
   if (thisInfo.submit_block.NotEmpty()) { 
       this_submit_blk.Assign(*(thisInfo.submit_block));
       if ( !AreAuthCapsOkInSubmitBlock(this_submit_blk) ) {
         if (seq_entry.IsSeq())
            thisInfo.test_item_list[GetName()].push_back(GetDiscItemText(seq_entry.GetSeq()));
         else if (seq_entry.IsSet()) {
            thisInfo.test_item_list[GetName()].push_back(GetDiscItemText(seq_entry.GetSet()));
         }
       }
   }
};


void CSeqEntry_DISC_CHECK_AUTH_CAPS :: GetReport(CRef <CClickableItem>& c_item)
{
   c_item->description = GetHasComment(c_item->item_list.size(), "pub") + 
                                                      "incorrect author capitalization.";
};
