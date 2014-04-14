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
* Author:  Colleen Bollin
*
* File Description:
*   Generate unique definition lines for a set of sequences using organism
*   descriptions and feature clauses.
*/

#include <ncbi_pch.hpp>
#include <objtools/edit/autodef.hpp>
#include <corelib/ncbimisc.hpp>
#include <objmgr/seqdesc_ci.hpp>
#include <objmgr/bioseq_ci.hpp>
#include <objmgr/util/feature.hpp>
#include <objmgr/util/sequence.hpp>
#include <objmgr/util/seq_loc_util.hpp>

#include <objects/seq/Seq_descr.hpp>
#include <objects/seq/Seqdesc.hpp>
#include <objects/seq/Bioseq.hpp>
#include <objects/seqfeat/RNA_ref.hpp>
#include <objects/seqfeat/BioSource.hpp>
#include <objects/seqfeat/Org_ref.hpp>
#include <objects/taxon3/Taxon3_request.hpp>
#include <objects/taxon3/T3Request.hpp>
#include <objects/taxon3/SequenceOfInt.hpp>
#include <objects/taxon3/Taxon3_reply.hpp>
#include <objects/taxon3/T3Reply.hpp>
#include <objects/taxon3/taxon3.hpp>

#include <serial/iterator.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

            
CAutoDef::CAutoDef()
    : m_FeatureListType(eListAllFeatures),
      m_MiscFeatRule(eDelete),
      m_SpecifyNuclearProduct(false),
      m_ProductFlag(CBioSource::eGenome_unknown),
      m_AltSpliceFlag(false),
      m_SuppressAltSplicePhrase(false),
      m_SuppressLocusTags(false),
      m_GeneOppStrand(false),
      m_RemoveTransposonAndInsertionSequenceSubfeatures(false),
      m_KeepExons(false),
      m_KeepIntrons(false),
      m_KeepPromoters(false),
      m_KeepLTRs(false),
      m_Keep3UTRs(false),
      m_Keep5UTRs(false),
	  m_UseNcRNAComment(false),
      m_UseFakePromoters(false),
      m_Cancelled(false)
{
    m_SuppressedFeatures.clear();
}


CAutoDef::~CAutoDef()
{
}


void CAutoDef::AddSources (CSeq_entry_Handle se)
{
    
    // add sources to modifier combination groups
    CBioseq_CI seq_iter(se, CSeq_inst::eMol_na);
    for ( ; seq_iter; ++seq_iter ) {
        CSeqdesc_CI dit((*seq_iter), CSeqdesc::e_Source);
        if (dit) {
            string feature_clauses = x_GetFeatureClauses(*seq_iter);
            const CBioSource& bsrc = dit->GetSource();
            m_OrigModCombo.AddSource(bsrc, feature_clauses);
        }
    }

    // set default exclude_sp values
    m_OrigModCombo.SetExcludeSpOrgs (m_OrigModCombo.GetDefaultExcludeSp());
}


void CAutoDef::x_SortModifierListByRank(TModifierIndexVector &index_list, CAutoDefSourceDescription::TAvailableModifierVector &modifier_list) 
{
    unsigned int k, j, tmp;
    if (index_list.size() < 2) {
        return;
    }
    for (k = 0; k < index_list.size() - 1; k++) {
        for (j = k + 1; j < index_list.size(); j++) { 
            if (modifier_list[index_list[k]].GetRank() > modifier_list[index_list[j]].GetRank()) {
                 tmp = index_list[k];
                 index_list[k] = index_list[j];
                 index_list[j] = tmp;
             }
         }
     }
}


void CAutoDef::x_GetModifierIndexList(TModifierIndexVector &index_list, CAutoDefSourceDescription::TAvailableModifierVector &modifier_list)
{
    unsigned int k;
    TModifierIndexVector remaining_list;

    index_list.clear();
    remaining_list.clear();
    
    // note - required modifiers should be removed from the list
    
    // first, look for all_present and all_unique modifiers
    for (k = 0; k < modifier_list.size(); k++) {
        if (modifier_list[k].AllPresent() && modifier_list[k].AllUnique()) {
            index_list.push_back(k);
        } else if (modifier_list[k].AnyPresent()) {
            remaining_list.push_back(k);
        }
    }
    x_SortModifierListByRank(index_list, modifier_list);
    x_SortModifierListByRank(remaining_list, modifier_list);
    
    for (k = 0; k < remaining_list.size(); k++) {
        index_list.push_back(remaining_list[k]);
    }
}


bool CAutoDef::x_IsOrgModRequired(unsigned int mod_type)
{
    return false;
}


bool CAutoDef::x_IsSubSrcRequired(unsigned int mod_type)
{
    if (mod_type == CSubSource::eSubtype_endogenous_virus_name
        || mod_type == CSubSource::eSubtype_plasmid_name
        || mod_type == CSubSource::eSubtype_transgenic) {
        return true;
    } else {
        return false;
    }
}


unsigned int CAutoDef::GetNumAvailableModifiers()
{
    CAutoDefSourceDescription::TAvailableModifierVector modifier_list;
    modifier_list.clear();
    m_OrigModCombo.GetAvailableModifiers (modifier_list);

    unsigned int num_present = 0;
    for (unsigned int k = 0; k < modifier_list.size(); k++) {
        if (modifier_list[k].AnyPresent()) {
            num_present++;
        }
    }
    return num_present;    
}


struct SAutoDefModifierComboSort {
    bool operator()(const CAutoDefModifierCombo& s1,
                    const CAutoDefModifierCombo& s2) const
    {
        return (s1 < s2);
    }
};



CAutoDefModifierCombo * CAutoDef::FindBestModifierCombo()
{
    CAutoDefModifierCombo *best = NULL;
    TModifierComboVector  combo_list;

    combo_list.clear();
    combo_list.push_back (new CAutoDefModifierCombo(&m_OrigModCombo));


    TModifierComboVector tmp, add_list;
    TModifierComboVector::iterator it;
    CAutoDefSourceDescription::TModifierVector mod_list, mods_to_try;
    bool stop = false;
    unsigned int  k;

    mod_list.clear();

    if (combo_list[0]->GetMaxInGroup() == 1) {
        stop = true;
    }

    while (!stop) {
        stop = true;
        it = combo_list.begin();
        add_list.clear();
        while (it != combo_list.end()) {
            tmp = (*it)->ExpandByAnyPresent ();
            if (!tmp.empty()) {
                stop = false;
                for (k = 0; k < tmp.size(); k++) {
                    add_list.push_back (new CAutoDefModifierCombo(tmp[k]));
                }
                it = combo_list.erase (it);
            } else {
                it++;
            }
            tmp.clear();
        }
        for (k = 0; k < add_list.size(); k++) {
            combo_list.push_back (new CAutoDefModifierCombo(add_list[k]));
        }
        add_list.clear();
        std::sort (combo_list.begin(), combo_list.end(), SAutoDefModifierComboSort());
        if (combo_list[0]->GetMaxInGroup() == 1) {
            stop = true;
        }
    }

    ITERATE (CAutoDefSourceDescription::TModifierVector, it, combo_list[0]->GetModifiers()) {
        mod_list.push_back (CAutoDefSourceModifierInfo(*it));
    }

    best = combo_list[0];
    combo_list[0] = NULL;
    for (k = 1; k < combo_list.size(); k++) {
       delete combo_list[k];
       combo_list[k] = NULL;
    }
    return best;
}


CAutoDefModifierCombo* CAutoDef::GetAllModifierCombo()
{
    CAutoDefModifierCombo *newm = new CAutoDefModifierCombo(&m_OrigModCombo);
        
    // set all modifiers in combo
    CAutoDefSourceDescription::TAvailableModifierVector modifier_list;
    
    // first, get the list of modifiers that are available
    modifier_list.clear();
    newm->GetAvailableModifiers (modifier_list);
    
    // add any modifier not already in the combo to the combo
    for (unsigned int k = 0; k < modifier_list.size(); k++) {
        if (modifier_list[k].AnyPresent()) {
            if (modifier_list[k].IsOrgMod()) {
                COrgMod::ESubtype subtype = modifier_list[k].GetOrgModType();
                if (!newm->HasOrgMod(subtype)) {
                    newm->AddOrgMod(subtype);
                } 
            } else {
                CSubSource::ESubtype subtype = modifier_list[k].GetSubSourceType();
                if (!newm->HasSubSource(subtype)) {
                    newm->AddSubsource(subtype);
                }
            }
        }
    }
    return newm;
}


CAutoDefModifierCombo* CAutoDef::GetEmptyCombo()
{
    CAutoDefModifierCombo *newm = new CAutoDefModifierCombo(&m_OrigModCombo);
        
    return newm;
}


string CAutoDef::GetOneSourceDescription(CBioseq_Handle bh)
{
    CAutoDefModifierCombo *best = FindBestModifierCombo();
    if (best == NULL) {
        return "";
    }
    
    for (CSeqdesc_CI dit(bh, CSeqdesc::e_Source); dit;  ++dit) {
        const CBioSource& bsrc = dit->GetSource();
        return best->GetSourceDescriptionString(bsrc);
    }
    return "";
}


#if 0
CAutoDefParsedtRNAClause *CAutoDef::x_tRNAClauseFromNote(CBioseq_Handle bh, const CSeq_feat& cf, const CSeq_loc& mapped_loc, string comment, bool is_first, bool is_last) 
{
    string product_name = "";
    string gene_name = "";
    

    string::size_type pos = NStr::Find(comment, "(");
    if (pos == NCBI_NS_STD::string::npos) {
        return NULL;
    }
    product_name = comment.substr(0, pos);
    comment = comment.substr (pos + 1);
    pos = NStr::Find(comment, ")");
    if (pos == NCBI_NS_STD::string::npos) {
        return NULL;
    }
    gene_name = comment.substr (0, pos);

    NStr::TruncateSpacesInPlace(product_name);
    NStr::TruncateSpacesInPlace(gene_name);
    
    if (NStr::StartsWith (product_name, "tRNA-")) {
        /* tRNA name must start with "tRNA-" and be followed by one uppercase letter and
         * two lowercase letters.
         */
        if (product_name.length() < 8
            || !isalpha(product_name.c_str()[5]) || !isupper(product_name.c_str()[5])
            || !isalpha(product_name.c_str()[6]) || !islower(product_name.c_str()[6])
            || !isalpha(product_name.c_str()[7]) || !islower(product_name.c_str()[7])) {
            return NULL;
        }

        /* gene name must be in parentheses, start with letters "trn",
         * and end with one uppercase letter.
         */    
        if (gene_name.length() < 4
            || !NStr::StartsWith(gene_name, "trn" )
            || !isalpha(gene_name.c_str()[3])
            || !isupper(gene_name.c_str()[3])) {
            return NULL;
        }
    }
    if (NStr::IsBlank (product_name) || NStr::IsBlank (gene_name)) {
        return NULL;
    }
         
    return new CAutoDefParsedtRNAClause(bh, cf, mapped_loc, gene_name, product_name, is_first, is_last);

}        


const int kParsedTrnaGene = 1;
const int kParsedTrnaSpacer = 2;


vector<CAutoDefFeatureClause *> CAutoDef::x_GetIntergenicSpacerClauseList (string comment, CBioseq_Handle bh, const CSeq_feat& cf, const CSeq_loc& mapped_loc, bool suppress_locus_tags)
{
    string::size_type pos;
    vector<CAutoDefFeatureClause *> clause_list;

    clause_list.clear();
    pos = NStr::Find (comment, " and ");
    // insert comma for parsing
    if (pos != NCBI_NS_STD::string::npos && pos > 0 && !NStr::StartsWith (comment.substr(pos - 1), ",")) {
        comment = comment.substr(0, pos - 1) + "," + comment.substr(pos);
    }

    vector<string> parts;
    NStr::Tokenize(comment, ",", parts, NStr::eMergeDelims );
    if ( parts.empty() ) {
        return clause_list;
    }

    if (parts.size() == 1 && NStr::Find (parts[0], "intergenic spacer") != NCBI_NS_STD::string::npos) {
        clause_list.push_back (new CAutoDefIntergenicSpacerClause (bh, cf, mapped_loc, parts[0]));
        return clause_list;
    }
    
    bool bad_phrase = false;
    bool names_correct = true;
    int last_type = 0;


    CAutoDefParsedtRNAClause *gene = NULL;

    for (size_t j = 0; j < parts.size() && names_correct && !bad_phrase; j++) {
        NStr::TruncateSpacesInPlace(parts[j]);
        if (NStr::StartsWith (parts[j], "and ")) {
            parts[j] = parts[j].substr(4);
        }
        if (NStr::EndsWith (parts[j], " intergenic spacer")) {
            // must be a spacer
            string spacer_description = parts[j].substr(0, parts[j].length() - 18);
            CAutoDefFeatureClause *spacer = new CAutoDefParsedIntergenicSpacerClause(bh, cf, mapped_loc, spacer_description, j == 0, j == parts.size() - 1);
            if (spacer == NULL) {
                bad_phrase = true;
            } else {
                if (last_type == kParsedTrnaGene) {
                    // spacer names and gene names must agree
                    string gene_name = gene->GetGeneName();
                    string description = spacer->GetDescription();
                    if (!NStr::StartsWith (description, gene_name + "-")) {
                        names_correct = false;
                    }
                }
                spacer->SetSuppressLocusTag(suppress_locus_tags);
                clause_list.push_back (spacer);
                last_type = kParsedTrnaSpacer;
            }
        } else {
            gene = x_tRNAClauseFromNote(bh, cf, mapped_loc, parts[j], j == 0, j == parts.size() - 1);
            if (gene == NULL) {
                bad_phrase = true;
            } else {
                // must alternate between genes and spacers
                if (last_type == kParsedTrnaSpacer) {
                    // spacer names and gene names must agree
                    string gene_name = gene->GetGeneName();
                    if (!NStr::EndsWith (parts[j - 1], "-" + gene_name + " intergenic spacer")) {
                        names_correct = false;
                    }
                }
                gene->SetSuppressLocusTag(suppress_locus_tags);
                clause_list.push_back (gene);
                last_type = kParsedTrnaGene;
            }
        }
    }

    if (bad_phrase || !names_correct) {
        for (size_t i = 0; i < clause_list.size(); i++) {
            delete clause_list[i];
        }
        clause_list.clear();
    }
    return clause_list;
}
#endif

bool CAutoDef::x_AddIntergenicSpacerFeatures(CBioseq_Handle bh, const CSeq_feat& cf, const CSeq_loc& mapped_loc, CAutoDefFeatureClause_Base &main_clause, bool suppress_locus_tags)
{
    CSeqFeatData::ESubtype subtype = cf.GetData().GetSubtype();
    if ((subtype != CSeqFeatData::eSubtype_misc_feature && subtype != CSeqFeatData::eSubtype_otherRNA)
        || !cf.CanGetComment()) {
        return false;
    }
    string comment = cf.GetComment();

    // get rid of any leading or trailing spaces
    NStr::TruncateSpacesInPlace(comment);

    // ignore anything after the first semicolon
    string::size_type pos = NStr::Find(comment, ";");
    if (pos != NCBI_NS_STD::string::npos) {
        comment = comment.substr(0, pos);
    }

    bool is_region = false;
    
    // ignore "contains " at beginning of comment
    if (NStr::StartsWith(comment, "contains ")) {
        comment = comment.substr(9);
    } else if (NStr::StartsWith (comment, "may contain ")) {
        comment = comment.substr(12);
        is_region = true;
    }
    
    vector<CAutoDefFeatureClause *> clause_list = GetIntergenicSpacerClauseList (comment, bh, cf, mapped_loc, suppress_locus_tags);

    if (clause_list.size() > 0) {
        if (is_region) {
            main_clause.AddSubclause (new CAutoDefParsedRegionClause (bh, cf, mapped_loc, comment));
            for (size_t i = 0; i < clause_list.size(); i++) {
                delete (clause_list[i]);
            }
        } else {
            for (size_t i = 0; i < clause_list.size(); i++) {
                main_clause.AddSubclause (clause_list[i]);
            }
        }
        return true;
    } else {
        return false;
    }
}
    

// Some misc_RNA clauses have a comment that actually lists multiple
// features.  These functions create a clause for each element in the
// comment.
static string misc_words [] = {
  "internal transcribed spacer",
  "external transcribed spacer",
  "ribosomal RNA intergenic spacer",
  "ribosomal RNA",
  "intergenic spacer"
};

typedef enum {
  MISC_RNA_WORD_INTERNAL_SPACER = 0,
  MISC_RNA_WORD_EXTERNAL_SPACER,
  MISC_RNA_WORD_RNA_INTERGENIC_SPACER,
  MISC_RNA_WORD_RNA,
  MISC_RNA_WORD_INTERGENIC_SPACER,
  NUM_MISC_RNA_WORDS
} MiscWord;


bool CAutoDef::x_AddMiscRNAFeatures(CBioseq_Handle bh, const CSeq_feat& cf, const CSeq_loc& mapped_loc, CAutoDefFeatureClause_Base &main_clause, bool suppress_locus_tags)
{
    string comment = "";
    string::size_type pos;
    
    if (cf.GetData().Which() == CSeqFeatData::e_Rna) {
        comment = cf.GetNamedQual("product");
        if (NStr::IsBlank(comment)
            && cf.IsSetData()
            && cf.GetData().IsRna()
            && cf.GetData().GetRna().IsSetExt()) {
            if (cf.GetData().GetRna().GetExt().IsName()) {
                comment = cf.GetData().GetRna().GetExt().GetName();
            } else if (cf.GetData().GetRna().GetExt().IsGen()
                       && cf.GetData().GetRna().GetExt().GetGen().IsSetProduct()) {
                comment = cf.GetData().GetRna().GetExt().GetGen().GetProduct();
            }
        }
    }

    if ((NStr::Equal (comment, "misc_RNA") || NStr::IsBlank (comment)) && cf.CanGetComment()) {
        comment = cf.GetComment();
    }
    if (NStr::IsBlank(comment)) {
        return false;
    }
    
    pos = NStr::Find(comment, "spacer");
    if (pos == NCBI_NS_STD::string::npos) {
        return false;
    }

    vector<CAutoDefFeatureClause *> clause_list;
    size_t j;
    bool bad_phrase = false;
    bool is_region = false;

    if (NStr::StartsWith (comment, "contains ")) {
        comment = comment.substr(9);
    } else if (NStr::StartsWith (comment, "may contain ")) {
        comment = comment.substr(12);
        is_region = true;
    }
    
 
    clause_list.clear();
    pos = NStr::Find (comment, " and ");
    // insert comma for parsing
    if (pos != NCBI_NS_STD::string::npos && pos > 0 && !NStr::StartsWith (comment.substr(pos - 1), ",")) {
        comment = comment.substr(0, pos - 1) + "," + comment.substr(pos);
    }

    vector<string> parts;
    NStr::Tokenize(comment, ",", parts, NStr::eMergeDelims );
    if ( parts.empty() ) {
        return false;
    }

    vector<unsigned int> types;

    for (j = 0; j < parts.size() && !bad_phrase; j++) {
        // find first of the recognized words to occur in the string
        string::size_type first_word = NCBI_NS_STD::string::npos;
        unsigned int word_id = 0;
    
        for (unsigned int i = 0; i < NUM_MISC_RNA_WORDS && first_word == NCBI_NS_STD::string::npos; i++) {
            first_word = NStr::Find (parts[j], misc_words[i]);
            if (first_word != NCBI_NS_STD::string::npos) {
                word_id = i;
            }
        }
        if (first_word == NCBI_NS_STD::string::npos) {
            bad_phrase = true;
        } else {
            types.push_back (word_id);
        }
    }

    if (!bad_phrase) {
        if (is_region) {
            clause_list.push_back (new CAutoDefParsedRegionClause(bh, cf, mapped_loc, comment));
        } else {
            for (j = 0; j < parts.size(); j++) {
                // create a clause of the appropriate type
                CAutoDefParsedClause *new_clause = new CAutoDefParsedClause(bh, cf, mapped_loc, j == 0, j == parts.size() - 1);        
                string description = "";
                if (types[j] == MISC_RNA_WORD_INTERNAL_SPACER
                    || types[j] == MISC_RNA_WORD_EXTERNAL_SPACER
                    || types[j] == MISC_RNA_WORD_RNA_INTERGENIC_SPACER
                    || types[j] == MISC_RNA_WORD_INTERGENIC_SPACER) {
                    if (NStr::StartsWith (parts[j], misc_words[types[j]])) {
                        new_clause->SetTypewordFirst(true);
                        description = parts[j].substr(misc_words[types[j]].length());
                    } else {
                        new_clause->SetTypewordFirst(false);
                        description = parts[j].substr(0, NStr::Find(parts[j], misc_words[types[j]]));
                    }
                    new_clause->SetTypeword(misc_words[types[j]]);
                } else if (types[j] == MISC_RNA_WORD_RNA) {
                    description = parts[j];
                    new_clause->SetTypeword("gene");
                    new_clause->SetTypewordFirst(false);
                }
                NStr::TruncateSpacesInPlace(description);
                new_clause->SetDescription(description);
        
                new_clause->SetSuppressLocusTag(suppress_locus_tags);
                clause_list.push_back (new_clause);
            }
        }
    }

    if (clause_list.size() > 0) {
        for (j = 0; j < clause_list.size(); j++) {
            main_clause.AddSubclause(clause_list[j]);
        }
        return true;
    } else {
        return false;
    }
}

void CAutoDef::x_RemoveOptionalFeatures(CAutoDefFeatureClause_Base *main_clause, CBioseq_Handle bh)
{
    // remove optional features that have not been requested
    if (main_clause == NULL) {
        return;
    }
    
    // keep 5' UTRs only if lonely or requested
    if (!m_Keep5UTRs && !main_clause->IsFeatureTypeLonely(CSeqFeatData::eSubtype_5UTR)) {
        main_clause->RemoveFeaturesByType(CSeqFeatData::eSubtype_5UTR);
    }
    
    // keep 3' UTRs only if lonely or requested
    if (!m_Keep3UTRs && !main_clause->IsFeatureTypeLonely(CSeqFeatData::eSubtype_3UTR)) {
        main_clause->RemoveFeaturesByType(CSeqFeatData::eSubtype_3UTR);
    }
    
    // keep LTRs only if requested or lonely and not in parent
    if (!m_KeepLTRs) {
        if (main_clause->GetNumSubclauses() > 1 
            || main_clause->GetMainFeatureSubtype() != CSeqFeatData::eSubtype_LTR) {
            main_clause->RemoveFeaturesByType(CSeqFeatData::eSubtype_LTR);
        }
    }
           
    // keep promoters only if requested or lonely and not in mRNA
    if (!m_KeepPromoters) {
        if (!main_clause->IsFeatureTypeLonely(CSeqFeatData::eSubtype_promoter)) {
            main_clause->RemoveFeaturesByType(CSeqFeatData::eSubtype_promoter);
        } else {
            main_clause->RemoveFeaturesInmRNAsByType(CSeqFeatData::eSubtype_promoter);
        }
    }
    
    // keep introns only if requested or lonely and not in mRNA
    if (!m_KeepIntrons) {
        if (!main_clause->IsFeatureTypeLonely(CSeqFeatData::eSubtype_intron)) {
            main_clause->RemoveFeaturesByType(CSeqFeatData::eSubtype_intron);
        } else {
            main_clause->RemoveFeaturesInmRNAsByType(CSeqFeatData::eSubtype_intron);
        }
    }
    
    // keep exons only if requested or lonely or in mRNA or in partial CDS or on segment
    if (!m_KeepExons && !IsSegment(bh)) {
        if (main_clause->GetMainFeatureSubtype() != CSeqFeatData::eSubtype_exon) {
            main_clause->RemoveUnwantedExons();
        }
    }
    
    // only keep bioseq precursor RNAs if lonely
    if (!main_clause->IsBioseqPrecursorRNA()) {
        main_clause->RemoveBioseqPrecursorRNAs();
    }
    
    // delete subclauses at end, so that loneliness calculations will be correct
    main_clause->RemoveDeletedSubclauses();
}


bool CAutoDef::x_IsFeatureSuppressed(CSeqFeatData::ESubtype subtype)
{
    unsigned int feat_type = CSeqFeatData::GetTypeFromSubtype(subtype); 
    ITERATE(TFeatTypeItemSet, it, m_SuppressedFeatures)  {
        unsigned i_feat_type = it->GetType();
        if (i_feat_type == CSeqFeatData::e_not_set) {
            return true;
        } else if (feat_type != i_feat_type) {
            continue;
        }
        unsigned int i_subtype = it->GetSubtype();
        if (i_subtype == CSeqFeatData::eSubtype_any
            || i_subtype == subtype ) {
            return true;
        }
    }
    return false;
}


void CAutoDef::SuppressFeature(objects::CFeatListItem feat)
{
    m_SuppressedFeatures.insert(feat);
}


bool CAutoDef::IsSegment(CBioseq_Handle bh)
{
    CSeq_entry_Handle seh = bh.GetParentEntry();
    
    seh = seh.GetParentEntry();
    
    if (seh && seh.IsSet()) {
        CBioseq_set_Handle bsh = seh.GetSet();
        if (bsh.CanGetClass() && bsh.GetClass() == CBioseq_set::eClass_parts) {
            return true;
        }
    }
    return false;
}


void CAutoDef::GetMasterLocation(CBioseq_Handle &bh, CRange<TSeqPos>& range)
{
    CSeq_entry_Handle seh = bh.GetParentEntry();
    CBioseq_Handle    master = bh;
    unsigned int      start = 0, stop = bh.GetBioseqLength() - 1;
    unsigned int      offset = 0;
    
    seh = seh.GetParentEntry();
    
    if (seh && seh.IsSet()) {
        CBioseq_set_Handle bsh = seh.GetSet();
        if (bsh.CanGetClass() && bsh.GetClass() == CBioseq_set::eClass_parts) {
            seh = seh.GetParentEntry();
            if (seh.IsSet()) {
                bsh = seh.GetSet();
                if (bsh.CanGetClass() && bsh.GetClass() == CBioseq_set::eClass_segset) {
                    CBioseq_CI seq_iter(seh);
                    for ( ; seq_iter; ++seq_iter ) {
                        if (seq_iter->CanGetInst_Repr()) {
                            if (seq_iter->GetInst_Repr() == CSeq_inst::eRepr_seg) {
                                master = *seq_iter;
                            } else if (seq_iter->GetInst_Repr() == CSeq_inst::eRepr_raw) {
                                if (*seq_iter == bh) {
                                    start = offset;
                                    stop = offset + bh.GetBioseqLength() - 1;
                                } else {
                                    offset += seq_iter->GetBioseqLength();
                                }
                            }
                        }
                    }
                }
            }
        }
    }
    bh = master;
    range.SetFrom(start);
    range.SetTo(stop);
}


bool CAutoDef::x_Is5SList(CFeat_CI feat_ci)
{
    bool is_list = true;
    bool is_single = true;
    bool found_single = false;

    if (!feat_ci) {
        return false;
    }
    ++feat_ci;
    if (feat_ci) {
        is_single = false;
    }
    feat_ci.Rewind();
    
    while (feat_ci && is_list) {
        if (feat_ci->GetData().GetSubtype() == CSeqFeatData::eSubtype_rRNA) {
            if (!feat_ci->GetData().GetRna().IsSetExt()
                || !feat_ci->GetData().GetRna().GetExt().IsName()
                || !NStr::Equal(feat_ci->GetData().GetRna().GetExt().GetName(), "5S ribosomal RNA")) {
                is_list = false;
            }
        } else if (feat_ci->GetData().GetSubtype() == CSeqFeatData::eSubtype_misc_feature) {
            if (!feat_ci->IsSetComment()) {
                is_list = false;
            } else if (NStr::Equal(feat_ci->GetComment(), "contains 5S ribosomal RNA and nontranscribed spacer")) {
                found_single = true;
            } else if (!NStr::Equal(feat_ci->GetComment(), "nontranscribed spacer")) {
                is_list = false;
            } 
        } else {
            is_list = false;
        }
        ++feat_ci;
    }
    if (is_single && !found_single) {
        is_list = false;
    }
    feat_ci.Rewind();
    return is_list;
} 


string CAutoDef::x_GetFeatureClauses(CBioseq_Handle bh)
{
    CAutoDefFeatureClause_Base main_clause(m_SuppressLocusTags);
    CAutoDefFeatureClause *new_clause;
    CRange<TSeqPos> range;
    CBioseq_Handle master_bh = bh;
    
    GetMasterLocation(master_bh, range);

    // if no promoter, and fake promoters are requested, create one
    if (m_UseFakePromoters) {
        SAnnotSelector sel(CSeqFeatData::eSubtype_promoter);
        CFeat_CI promoter_ci (bh, sel);

        if (!promoter_ci) {
            CRef<CSeq_feat> fake_promoter(new CSeq_feat());
            CRef<CSeq_loc> fake_promoter_loc(new CSeq_loc());
            const CSeq_id* id = FindBestChoice(bh.GetBioseqCore()->GetId(), CSeq_id::BestRank);
            CRef <CSeq_id> new_id(new CSeq_id);
            new_id->Assign(*id);
            fake_promoter_loc->SetInt().SetId(*new_id);
            fake_promoter_loc->SetInt().SetFrom(0);
            fake_promoter_loc->SetInt().SetTo(bh.GetInst_Length() - 1);

            fake_promoter->SetLocation(*fake_promoter_loc);

            main_clause.AddSubclause (new CAutoDefFakePromoterClause (master_bh, 
                                                                      *fake_promoter,
                                                                      *fake_promoter_loc));
        }
    }

    // now create clauses for real features
    CFeat_CI feat_ci(master_bh);

    if (x_Is5SList(feat_ci)) {
        return "5S ribosomal RNA gene region";
    }

    while (feat_ci)
    { 
        const CSeq_feat& cf = feat_ci->GetOriginalFeature();
        const CSeq_loc& mapped_loc = feat_ci->GetMappedFeature().GetLocation();
        unsigned int subtype = cf.GetData().GetSubtype();
        unsigned int stop = mapped_loc.GetStop(eExtreme_Positional);
        // unless it's a gene, don't use it unless it ends in the sequence we're looking at
        if ((subtype == CSeqFeatData::eSubtype_gene 
             || subtype == CSeqFeatData::eSubtype_mRNA
             || subtype == CSeqFeatData::eSubtype_cdregion
             || (stop >= range.GetFrom() && stop <= range.GetTo()))
            && !x_IsFeatureSuppressed(cf.GetData().GetSubtype())) {

			// some clauses can be created differently just knowing the subtype
		    if (subtype == CSeqFeatData::eSubtype_ncRNA) {
				new_clause = new CAutoDefNcRNAClause(bh, cf, mapped_loc, m_UseNcRNAComment);
			} else {
				// others require more parsing
				new_clause = new CAutoDefFeatureClause(bh, cf, mapped_loc);
				subtype = new_clause->GetMainFeatureSubtype();
				if (new_clause->IsTransposon()) {
					delete new_clause;
					new_clause = new CAutoDefTransposonClause(bh, cf, mapped_loc);
				} else if (new_clause->IsSatelliteClause()) {
					delete new_clause;
					new_clause = new CAutoDefSatelliteClause(bh, cf, mapped_loc);
				} else if (subtype == CSeqFeatData::eSubtype_promoter) {
					delete new_clause;
					new_clause = new CAutoDefPromoterClause(bh, cf, mapped_loc);
				} else if (subtype == CSeqFeatData::eSubtype_otherRNA
						   || subtype == CSeqFeatData::eSubtype_misc_RNA
						   || subtype == CSeqFeatData::eSubtype_rRNA) {
					if (x_AddMiscRNAFeatures(bh, cf, mapped_loc, main_clause, m_SuppressLocusTags)) {
						delete new_clause;
						new_clause = NULL;
					}
        } else if (subtype == CSeqFeatData::eSubtype_misc_feature && x_AddMiscRNAFeatures(bh, cf, mapped_loc, main_clause, m_SuppressLocusTags)) {
  				delete new_clause;
	  			new_clause = NULL;
				} else if (new_clause->IsIntergenicSpacer()) {
					delete new_clause;
					x_AddIntergenicSpacerFeatures(bh, cf, mapped_loc, main_clause, m_SuppressLocusTags);
					new_clause = NULL;
				} else if (new_clause->IsGeneCluster()) {
					delete new_clause;
					new_clause = new CAutoDefGeneClusterClause(bh, cf, mapped_loc);
				} else if (new_clause->GetMainFeatureSubtype() == CSeqFeatData::eSubtype_misc_feature
						   && !NStr::Equal(new_clause->GetTypeword(), "control region")) {
					if (m_MiscFeatRule == eDelete
						|| (m_MiscFeatRule == eNoncodingProductFeat && !new_clause->IsNoncodingProductFeat())) {
						delete new_clause;
						new_clause = NULL;
					} else if (m_MiscFeatRule == eCommentFeat) {
						delete new_clause;
						new_clause = NULL;
						if (cf.CanGetComment() && ! NStr::IsBlank(cf.GetComment())) {
							new_clause = new CAutoDefMiscCommentClause(bh, cf, mapped_loc);
						}
					}            
				}
			}
        
            if (new_clause != NULL && new_clause->IsRecognizedFeature()) {
                new_clause->SetSuppressLocusTag(m_SuppressLocusTags);
                if (new_clause->GetMainFeatureSubtype() == CSeqFeatData::eSubtype_exon) {
                    new_clause->Label();
                }
                main_clause.AddSubclause(new_clause);
            } else if (new_clause != NULL) {            
                delete new_clause;
            }
        }
        ++feat_ci;
    }
    
    // Group alt-spliced exons first, so that they will be associated with the correct genes and mRNAs
    main_clause.GroupAltSplicedExons(bh);
    main_clause.RemoveDeletedSubclauses();
    
    // Add mRNAs to other clauses
    main_clause.GroupmRNAs();
    main_clause.RemoveDeletedSubclauses();
   
    // Add genes to clauses that need them for descriptions/products
    main_clause.GroupGenes();
        
    main_clause.GroupSegmentedCDSs();
    main_clause.RemoveDeletedSubclauses();
        
    // Group all features
    main_clause.GroupClauses(m_GeneOppStrand);
    main_clause.RemoveDeletedSubclauses();
    
    // now that features have been grouped, can expand lists of spliced exons
    main_clause.ExpandExonLists();
    
    // assign product names for features associated with genes that have products
    main_clause.AssignGeneProductNames(&main_clause);
    
    // reverse the order of clauses for minus-strand CDSfeatures
    main_clause.ReverseCDSClauseLists();
    
    main_clause.Label();
    main_clause.CountUnknownGenes();
    main_clause.RemoveDeletedSubclauses();
    
    // remove miscRNA and otherRNA features that are trans-spliced leaders
    main_clause.RemoveTransSplicedLeaders();
    main_clause.RemoveDeletedSubclauses();
    
    x_RemoveOptionalFeatures(&main_clause, bh);
    
    // if a gene is listed as part of another clause, they do not need
    // to be listed as there own clause
    main_clause.RemoveGenesMentionedElsewhere();
    main_clause.RemoveDeletedSubclauses();
    
    if (m_RemoveTransposonAndInsertionSequenceSubfeatures) {
        main_clause.SuppressTransposonAndInsertionSequenceSubfeatures();
    }

    // if this is a segment, remove genes, mRNAs, and CDSs
    // unless they end on this segment or have subclauses that need their protein/gene
    // information
    main_clause.RemoveNonSegmentClauses(range);
    
    main_clause.Label();

    if (!m_SuppressAltSplicePhrase) {
        main_clause.FindAltSplices();
        main_clause.RemoveDeletedSubclauses();
    } 
    
    main_clause.ConsolidateRepeatedClauses();
    main_clause.RemoveDeletedSubclauses();
    
    main_clause.GroupConsecutiveExons(bh);
    main_clause.RemoveDeletedSubclauses();
    
    main_clause.Label();

    return main_clause.ListClauses(true, false);
}


string OrganelleByGenome(unsigned int genome_val)
{
    string organelle = "";
    switch (genome_val) {
        case CBioSource::eGenome_macronuclear:
            organelle = "macronuclear";
            break;
        case CBioSource::eGenome_nucleomorph:
            organelle = "nucleomorph";
            break;
        case CBioSource::eGenome_mitochondrion:
            organelle = "mitochondrion";
            break;
        case CBioSource::eGenome_apicoplast:
            organelle = "apicoplast";
            break;
        case CBioSource::eGenome_chloroplast:
            organelle = "chloroplast";
            break;
        case CBioSource::eGenome_chromoplast:
            organelle = "chromoplast";
            break;
        case CBioSource::eGenome_kinetoplast:
            organelle = "kinetoplast";
            break;
        case CBioSource::eGenome_plastid:
            organelle = "plastid";
            break;
        case CBioSource::eGenome_cyanelle:
            organelle = "cyanelle";
            break;
        case CBioSource::eGenome_leucoplast:
            organelle = "leucoplast";
            break;
        case CBioSource::eGenome_proplastid:
            organelle = "proplastid";
            break;
        case CBioSource::eGenome_hydrogenosome:
            organelle = "hydrogenosome";
            break;
    }
    return organelle;
}


static unsigned int s_GetProductFlagFromCDSProductNames (CBioseq_Handle bh)
{
	unsigned int product_flag = CBioSource::eGenome_unknown;
	string::size_type pos;

	SAnnotSelector sel(CSeqFeatData::eSubtype_cdregion);
    CFeat_CI feat_ci(bh, sel);
	while (feat_ci && product_flag == CBioSource::eGenome_unknown) {
		string label;
        CConstRef<CSeq_feat> prot
            = sequence::GetBestOverlappingFeat(feat_ci->GetProduct(),
                                               CSeqFeatData::e_Prot,
                                               sequence::eOverlap_Simple,
                                               bh.GetScope());
        if (prot) {
            feature::GetLabel(*prot, &label, feature::fFGL_Content);
			if (NStr::Find (label, "macronuclear") != NCBI_NS_STD::string::npos) {
              product_flag = CBioSource::eGenome_macronuclear;
			} else if (NStr::Find (label, "nucleomorph") != NCBI_NS_STD::string::npos) {
              product_flag = CBioSource::eGenome_nucleomorph;
			} else if (NStr::Find (label, "mitochondrion") != NCBI_NS_STD::string::npos
				       || NStr::Find (label, "mitochondrial") != NCBI_NS_STD::string::npos) {
              product_flag = CBioSource::eGenome_mitochondrion;
			} else if (NStr::Find (label, "apicoplast") != NCBI_NS_STD::string::npos) {
              product_flag = CBioSource::eGenome_apicoplast;
			} else if (NStr::Find (label, "chloroplast") != NCBI_NS_STD::string::npos) {
              product_flag = CBioSource::eGenome_chloroplast;
			} else if (NStr::Find (label, "chromoplast") != NCBI_NS_STD::string::npos) {
              product_flag = CBioSource::eGenome_chromoplast;
			} else if (NStr::Find (label, "kinetoplast") != NCBI_NS_STD::string::npos) {
              product_flag = CBioSource::eGenome_kinetoplast;
			} else if (NStr::Find (label, "proplastid") != NCBI_NS_STD::string::npos) {
              product_flag = CBioSource::eGenome_proplastid;
			} else if ((pos = NStr::Find (label, "plastid")) != NCBI_NS_STD::string::npos
				       && (pos == 0 || isspace (label.c_str()[pos]))) {
              product_flag = CBioSource::eGenome_plastid;
			} else if (NStr::Find (label, "cyanelle") != NCBI_NS_STD::string::npos) {
              product_flag = CBioSource::eGenome_cyanelle;
			} else if (NStr::Find (label, "leucoplast") != NCBI_NS_STD::string::npos) {
              product_flag = CBioSource::eGenome_leucoplast;
			} else if (NStr::Find (label, "hydrogenosome") != NCBI_NS_STD::string::npos) {
              product_flag = CBioSource::eGenome_hydrogenosome;
			} 
        }
		++feat_ci;
	}
    return product_flag;
}


string CAutoDef::x_GetFeatureClauseProductEnding(const string& feature_clauses,
                                                 CBioseq_Handle bh)
{
    bool pluralize = false;
	unsigned int product_flag_to_use;
    
	if (m_SpecifyNuclearProduct) {
	    product_flag_to_use = s_GetProductFlagFromCDSProductNames (bh);
	} else {
		product_flag_to_use = m_ProductFlag;
	}
    if (NStr::Find(feature_clauses, "genes") != NCBI_NS_STD::string::npos) {
        pluralize = true;
    } else {
        string::size_type pos = NStr::Find(feature_clauses, "gene");
        if (pos != NCBI_NS_STD::string::npos
            && NStr::Find (feature_clauses, "gene", pos + 4) != NCBI_NS_STD::string::npos) {
            pluralize = true;
        }
    }

    unsigned int genome_val = CBioSource::eGenome_unknown;
    string genome_from_mods = "";
 
    for (CSeqdesc_CI dit(bh, CSeqdesc::e_Source); dit;  ++dit) {
        const CBioSource& bsrc = dit->GetSource();
        if (bsrc.CanGetGenome()) {
            genome_val = bsrc.GetGenome();
        }
        if (bsrc.CanGetSubtype()) {
            ITERATE (CBioSource::TSubtype, subSrcI, bsrc.GetSubtype()) {
                if ((*subSrcI)->GetSubtype() == CSubSource::eSubtype_other) {
                    string note = (*subSrcI)->GetName();
                    if (NStr::Equal(note, "macronuclear") || NStr::Equal(note, "micronuclear")) {
                        genome_from_mods = note;
                    }
                }
            }
        }
        break;
    }
    
    string ending = OrganelleByGenome(genome_val);
    if (NStr::Equal(ending, "mitochondrion")) {
        ending = "mitochondrial";
    }
    if (!NStr::IsBlank(ending)) {
        ending = "; " + ending;
    } else {
        ending = OrganelleByGenome(product_flag_to_use);
        if (NStr::IsBlank(ending)) {
            if (!NStr::IsBlank(genome_from_mods)) {
                ending = "; " + genome_from_mods;
            }
        } else {
            if (NStr::Equal(ending, "mitochondrion")) {
                ending = "mitochondrial";
            }
            if (pluralize) {
                ending = "; nuclear genes for " + ending + " products";
            } else {
                ending = "; nuclear gene for " +  ending + " product";
            }
        }
    } 
    return ending;
}


string CAutoDef::x_GetNonFeatureListEnding()
{
    string end = "";
    switch (m_FeatureListType)
    {
        case eCompleteSequence:
            end = ", complete sequence.";
            break;
        case eCompleteGenome:
            end = ", complete genome.";
            break;
        case ePartialSequence:
            end = ", partial sequence.";
            break;
        case ePartialGenome:
            end = ", partial genome.";
            break;
        case eSequence:
            end = " sequence.";
            break;
        default:
            break;
    }
    return end;
}


string CAutoDef::GetOneFeatureClauseList(CBioseq_Handle bh, unsigned int genome_val)
{
    string feature_clauses = "";
    if (m_FeatureListType == eListAllFeatures) {
        feature_clauses = " " + x_GetFeatureClauses(bh);
        string ending = x_GetFeatureClauseProductEnding(feature_clauses, bh);
        if (m_AltSpliceFlag) {
            if (NStr::IsBlank(ending)) {
                ending = "; alternatively spliced";
            } else {
                ending += ", alternatively spliced";
            }
        }
        feature_clauses += ending;
        if (NStr::IsBlank(feature_clauses)) {
            feature_clauses = ".";
        } else {
            feature_clauses += ".";
        }
    } else {
        string organelle = "";
        
        if (m_FeatureListType != eSequence
            || genome_val == CBioSource::eGenome_apicoplast
            || genome_val == CBioSource::eGenome_chloroplast
            || genome_val == CBioSource::eGenome_kinetoplast
            || genome_val == CBioSource::eGenome_leucoplast
            || genome_val == CBioSource::eGenome_mitochondrion
            || genome_val == CBioSource::eGenome_plastid) {
            organelle = OrganelleByGenome(genome_val);
        }
        if (!NStr::IsBlank(organelle)) {
            feature_clauses = " " + organelle;
        } else if (NStr::IsBlank(organelle) && m_FeatureListType == eSequence) {
            string biomol = "";
            CSeqdesc_CI mi(bh, CSeqdesc::e_Molinfo);
            if (mi && mi->GetMolinfo().IsSetBiomol()) {
                biomol = CMolInfo::GetBiomolName(mi->GetMolinfo().GetBiomol());
            }
            if (!NStr::IsBlank(biomol)) {
                feature_clauses = " " + biomol;
            }
        }

        feature_clauses += x_GetNonFeatureListEnding();
    }
    return feature_clauses;
}


string CAutoDef::GetOneDefLine(CAutoDefModifierCombo *mod_combo, CBioseq_Handle bh)
{
    // for protein sequences, use sequence::GetTitle
    if (bh.CanGetInst() && bh.GetInst().CanGetMol() && bh.GetInst().GetMol() == CSeq_inst::eMol_aa) {
        return sequence::CDeflineGenerator()
            .GenerateDefline(bh,
                             sequence::CDeflineGenerator::fIgnoreExisting |
                             sequence::CDeflineGenerator::fAllProteinNames);
    }
    string org_desc = "Unknown organism";
    unsigned int genome_val = CBioSource::eGenome_unknown;
    
    for (CSeqdesc_CI dit(bh, CSeqdesc::e_Source); dit;  ++dit) {
        const CBioSource& bsrc = dit->GetSource();
        org_desc = mod_combo->GetSourceDescriptionString(bsrc);
        if (bsrc.CanGetGenome()) {
            genome_val = bsrc.GetGenome();
        }
        break;
    }
    string feature_clauses = GetOneFeatureClauseList(bh, genome_val);
    
    return org_desc + feature_clauses;
}


string CAutoDef::GetDocsumOrgDescription(CSeq_entry_Handle se)
{
    string joined_org = "Mixed organisms";

    CRef<CT3Request> rq(new CT3Request());
    CBioseq_CI bi (se, CSeq_inst::eMol_na);
    while (bi) {
        CSeqdesc_CI desc_ci(*bi, CSeqdesc::e_Source);
        if (desc_ci && desc_ci->GetSource().IsSetOrg()) {
            int taxid = desc_ci->GetSource().GetOrg().GetTaxId();
            if (taxid > 0) {
                rq->SetJoin().Set().push_back(taxid);
            }
        }
        ++bi;
    }
    if (rq->IsJoin() && rq->GetJoin().Get().size() > 0) {
        CTaxon3_request request;
        request.SetRequest().push_back(rq);
        CTaxon3 taxon3;
        taxon3.Init();
        CRef<CTaxon3_reply> reply = taxon3.SendRequest(request);
        if (reply) {
            CTaxon3_reply::TReply::const_iterator reply_it = reply->GetReply().begin();
            while (reply_it != reply->GetReply().end()) {
                if ((*reply_it)->IsData() 
                    && (*reply_it)->GetData().GetOrg().IsSetTaxname()) {
                    joined_org = (*reply_it)->GetData().GetOrg().GetTaxname();
                    break;
                }
                ++reply_it;
            }
        }
    }

    return joined_org;
}


string CAutoDef::GetDocsumDefLine(CSeq_entry_Handle se)
{
    string org_desc = GetDocsumOrgDescription(se);

    string feature_clauses = "";
    CBioseq_CI bi(se, CSeq_inst::eMol_na);
    if (bi) {
        unsigned int genome_val = CBioSource::eGenome_unknown;
        CSeqdesc_CI di(*bi, CSeqdesc::e_Source);
        if (di && di->GetSource().IsSetGenome()) {
            genome_val = di->GetSource().GetGenome();
        }
        feature_clauses = GetOneFeatureClauseList(*bi, genome_val);
    }
    
    return org_desc + feature_clauses;
}


bool CAutoDef::NeedsDocsumDefline(const CBioseq_set& set)
{
    bool rval = false;
    if (set.IsSetClass()) {
        CBioseq_set::EClass set_class = set.GetClass();
        if (set_class == CBioseq_set::eClass_pop_set
              || set_class == CBioseq_set::eClass_phy_set
              || set_class == CBioseq_set::eClass_eco_set
              || set_class == CBioseq_set::eClass_mut_set) {
            rval = true;
        }
    }
    return rval;
}


void CAutoDef::GetAvailableModifiers(CAutoDef::TAvailableModifierSet &mod_set)
{    
    mod_set.clear();
    CAutoDefSourceDescription::TAvailableModifierVector modifier_list;
    modifier_list.clear();
    m_OrigModCombo.GetAvailableModifiers (modifier_list);
    for (unsigned int k = 0; k < modifier_list.size(); k++) {
        mod_set.insert(CAutoDefAvailableModifier(modifier_list[k]));
    }
}


END_SCOPE(objects)
END_NCBI_SCOPE
