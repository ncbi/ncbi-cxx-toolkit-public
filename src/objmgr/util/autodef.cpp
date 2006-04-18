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
#include <corelib/ncbimisc.hpp>
#include <objmgr/util/autodef.hpp>
#include <objmgr/seqdesc_ci.hpp>
#include <objmgr/bioseq_ci.hpp>
#include <objmgr/feat_ci.hpp>
#include <objmgr/util/feature.hpp>
#include <objmgr/util/sequence.hpp>

#include <objects/seq/Seq_descr.hpp>
#include <objects/seq/Seqdesc.hpp>
#include <objects/seq/Bioseq.hpp>

#include <serial/iterator.hpp>

#include <objtools/format/context.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

            
CAutoDef::CAutoDef()
    : m_SuppressAltSplicePhrase(false),
      m_FeatureListType(eListAllFeatures),
      m_ProductFlag(CBioSource::eGenome_unknown),
      m_AltSpliceFlag(false)
{
    m_ComboList.clear();
}


CAutoDef::~CAutoDef()
{
}

void CAutoDef::AddSources (CSeq_entry_Handle se)
{

    if (m_ComboList.size() == 0) {
        m_ComboList.push_back(new CAutoDefModifierCombo());
    }
    
    // add sources to modifier combination groups
    CBioseq_CI seq_iter(se, CSeq_inst::eMol_na);
    for ( ; seq_iter; ++seq_iter ) {
        for (CSeqdesc_CI dit((*seq_iter), CSeqdesc::e_Source); dit;  ++dit) {
            const CBioSource& bsrc = dit->GetSource();
            for (unsigned int k = 0; k < m_ComboList.size(); k++) {
                m_ComboList[k]->AddSource(bsrc);
           }
        }
    }
}


void CAutoDef::AddSources (CBioseq_Handle bh) 
{
    for (CSeqdesc_CI dit(bh, CSeqdesc::e_Source); dit;  ++dit) {
        const CBioSource& bsrc = dit->GetSource();
        for (unsigned int k = 0; k < m_ComboList.size(); k++) {
             m_ComboList[k]->AddSource(bsrc);
        }
    }
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


CAutoDefModifierCombo * CAutoDef::FindBestModifierCombo()
{
    _ASSERT(m_ComboList.size() > 0);
    CAutoDefSourceDescription::TAvailableModifierVector modifier_list;
    CAutoDefModifierCombo *best = NULL;
    
    // first, get the list of modifiers that are available
    modifier_list.clear();
    m_ComboList[0]->GetAvailableModifiers (modifier_list);
    // later, need to find a way to specify required modifiers
    // these should be added first to the master combo
    
    if (m_ComboList[0]->AllUnique()) {
        best = m_ComboList[0];
        return best;
    }
    
    // find the order in which we should try the modifiers
    TModifierIndexVector index_list;
    index_list.clear();
    x_GetModifierIndexList(index_list, modifier_list);
    
    // copy the original combo and add a modifier
    unsigned int start_index = 0;
    unsigned int num_to_expand = 1;
    unsigned int next_num_to_expand = 0;
    while (best == NULL && num_to_expand + start_index < m_ComboList.size() && num_to_expand > 0) {
        next_num_to_expand = 0;
        for (unsigned int j = start_index; j < start_index + num_to_expand && best == NULL; j++) {
            for (unsigned int k = 0; k < index_list.size() && best == NULL; k++) {
                CAutoDefModifierCombo *newm = new CAutoDefModifierCombo(m_ComboList[j]);
                if (modifier_list[index_list[k]].IsOrgMod()) {
                    newm->AddOrgMod(modifier_list[index_list[k]].GetOrgModType());
                } else {
                    newm->AddSubsource(modifier_list[index_list[k]].GetSubSourceType());
                }
                if (newm->AllUnique()) {
                    best = newm;
                } else if (newm->GetNumGroups() > m_ComboList[j]->GetNumGroups()) {
                    m_ComboList.push_back(newm);
                    next_num_to_expand ++;
                } else {
                    delete newm;
                }
            }
        }
        start_index += num_to_expand;
        num_to_expand = next_num_to_expand;
    }
    
    return best;
}   


string CAutoDef::x_GetSourceDescriptionString (CAutoDefModifierCombo *mod_combo, const CBioSource& bsrc) 
{
    unsigned int k;
    string       source_description = "";
    
    /* start with tax name */
    source_description += bsrc.GetOrg().GetTaxname();
    
    if (mod_combo == NULL) {
        return source_description;
    }

    if (bsrc.CanGetSubtype()) {
        for (k = 0; k < mod_combo->GetNumSubSources(); k++) {
            ITERATE (CBioSource::TSubtype, subSrcI, bsrc.GetSubtype()) {
                if ((*subSrcI)->GetSubtype() == mod_combo->GetSubSource(k)) {
                    source_description += (*subSrcI)->GetName();
                }
            }
        }
    }
    if (bsrc.CanGetOrg() && bsrc.GetOrg().CanGetOrgname() && bsrc.GetOrg().GetOrgname().CanGetMod()) {
        for (k = 0; k < mod_combo->GetNumOrgMods(); k++) {
            ITERATE (COrgName::TMod, modI, bsrc.GetOrg().GetOrgname().GetMod()) {
                if ((*modI)->GetSubtype() == mod_combo->GetOrgMod(k)) {
                    source_description += (*modI)->GetSubname();
                }
            }
        }
    }
    
    return source_description;
}


string CAutoDef::GetOneSourceDescription(CBioseq_Handle bh)
{
    CAutoDefModifierCombo *best = FindBestModifierCombo();
    if (best == NULL) {
        return "";
    }
    
    for (CSeqdesc_CI dit(bh, CSeqdesc::e_Source); dit;  ++dit) {
        const CBioSource& bsrc = dit->GetSource();
        return x_GetSourceDescriptionString(best, bsrc);
    }
    return "";
}



CAutoDefParsedtRNAClause *CAutoDef::x_tRNAClauseFromNote(CBioseq_Handle bh, const CSeq_feat& cf, string &comment, bool is_first) 
{
    string product_name = "";
    string gene_name = "";
    
    /* tRNA name must start with "tRNA-" and be followed by one uppercase letter and
     * two lowercase letters.
     */
    if (!NStr::StartsWith(comment, "tRNA-")
        || comment.length() < 8
        || !isalpha(comment.c_str()[5]) || !isupper(comment.c_str()[5])
        || !isalpha(comment.c_str()[6]) || !islower(comment.c_str()[6])
        || !isalpha(comment.c_str()[7]) || !islower(comment.c_str()[7])) {
        return NULL;
    }
    product_name = comment.substr(0, 8);
    comment = comment.substr(8);
    NStr::TruncateSpacesInPlace(comment);

    /* gene name must be in parentheses, start with letters "trn",
     * and end with one uppercase letter.
     */    
    if (comment.length() < 6
        || !NStr::StartsWith(comment, "(trn" )
        || !isalpha(comment.c_str()[4])
        || !isupper(comment.c_str()[4])
        || comment.c_str()[5] != ')' ) {
        return NULL;
    }
    gene_name = comment.substr(1, 4);
    comment = comment.substr(6);
    NStr::TruncateSpacesInPlace(comment);
    
    // if the comment ends or the next item is a semicolon, this is the last feature
    bool is_last = NStr::IsBlank(comment) || NStr::StartsWith(comment, ";");
     
    return new CAutoDefParsedtRNAClause(bh, cf, gene_name, product_name, is_first, is_last);

}        


bool CAutoDef::x_AddIntergenicSpacerFeatures(CBioseq_Handle bh, const CSeq_feat& cf, CAutoDefFeatureClause_Base &main_clause, bool suppress_locus_tags)
{
    CSeqFeatData::ESubtype subtype = cf.GetData().GetSubtype();
    if ((subtype != CSeqFeatData::eSubtype_misc_feature && subtype != CSeqFeatData::eSubtype_otherRNA)
        || !cf.CanGetComment()) {
        return false;
    }
    string comment = cf.GetComment();
    unsigned int pos = NStr::Find(comment, "intergenic spacer");
    if (pos == NCBI_NS_STD::string::npos) {
        return false;
    }
    
    // ignore "contains " at beginning of comment
    if (NStr::StartsWith(comment, "contains ")) {
        comment = comment.substr(9);
    }
    
    // get rid of any leading or trailing spaces
    NStr::TruncateSpacesInPlace(comment);
    
    CAutoDefParsedtRNAClause *before = x_tRNAClauseFromNote(bh, cf, comment, true);
    CAutoDefParsedtRNAClause *after = NULL;
    CAutoDefFeatureClause *spacer = NULL;

    pos = NStr::Find(comment, "intergenic spacer");
    if (pos == NCBI_NS_STD::string::npos) {
        delete before;
        delete spacer;
        return false;
    }
    unsigned int start_after;
    start_after = NStr::Find(comment, ",", pos);
    if (start_after == NCBI_NS_STD::string::npos) {
        start_after = NStr::Find(comment, " and ", pos);
        if (start_after != NCBI_NS_STD::string::npos) {
            start_after += 5;
        }
    } else {
        start_after += 1;
    }
    if (start_after != NCBI_NS_STD::string::npos) {    
        comment = comment.substr(start_after);
        NStr::TruncateSpacesInPlace(comment);
        after = x_tRNAClauseFromNote(bh, cf, comment, false);
    }
    
    string description = "";
    if (before != NULL && after != NULL) {
        description = before->GetGeneName() + "-" + after->GetGeneName();
        spacer = new CAutoDefParsedIntergenicSpacerClause(bh, cf, description, before == NULL, after == NULL);
    } else {
        spacer = new CAutoDefIntergenicSpacerClause(bh, cf);
    }
    
    if (before != NULL) {
        before->SetSuppressLocusTag(suppress_locus_tags);
        main_clause.AddSubclause(before);
    }
    if (spacer != NULL) {
        spacer->SetSuppressLocusTag(suppress_locus_tags);
        main_clause.AddSubclause(spacer);
    }
    if (after != NULL) {
        after->SetSuppressLocusTag(suppress_locus_tags);
        main_clause.AddSubclause(after);
    }
    return true;
}
    

string CAutoDef::x_GetFeatureClauses(CBioseq_Handle bh, bool suppress_locus_tags, bool gene_cluster_opp_strand)
{
    CAutoDefFeatureClause_Base main_clause(suppress_locus_tags);
    CAutoDefFeatureClause *new_clause;
    
    CFeat_CI feat_ci(bh);
    while (feat_ci)
    {
        const CSeq_feat& cf = feat_ci->GetOriginalFeature();
        new_clause = new CAutoDefFeatureClause(bh, cf);
        if (new_clause->IsTransposon()) {
            delete new_clause;
            new_clause = new CAutoDefTransposonClause(bh, cf);
        } else if (new_clause->IsSatelliteClause()) {
            delete new_clause;
            new_clause = new CAutoDefSatelliteClause(bh, cf);
        } else if (new_clause->GetMainFeatureSubtype() == CSeqFeatData::eSubtype_promoter) {
            delete new_clause;
            new_clause = new CAutoDefPromoterClause(bh, cf);
        } else if (new_clause->IsIntergenicSpacer()) {
            delete new_clause;
            x_AddIntergenicSpacerFeatures(bh, cf, main_clause, suppress_locus_tags);
            new_clause = NULL;
        } else if (new_clause->IsGeneCluster()) {
            delete new_clause;
            new_clause = new CAutoDefGeneClusterClause(bh, cf);
        }
        
        if (new_clause != NULL && new_clause->IsRecognizedFeature()) {
            new_clause->SetSuppressLocusTag(suppress_locus_tags);
            main_clause.AddSubclause(new_clause);
        } else if (new_clause != NULL) {            
            delete new_clause;
        }
        ++feat_ci;
    }
    
    // First, need to remove suppressed features
    // TODO
    
    // Add mRNAs to other clauses
    main_clause.GroupmRNAs();
    main_clause.RemoveDeletedSubclauses();
   
    // Add genes to clauses that need them for descriptions/products
    main_clause.GroupGenes();
    
    
    
    // Group all features
    main_clause.GroupClauses(gene_cluster_opp_strand);
    main_clause.RemoveDeletedSubclauses();
    
    main_clause.Label();
    main_clause.CountUnknownGenes();
    main_clause.RemoveDeletedSubclauses();
    
    // if a gene is listed as part of another clause, they do not need
    // to be listed as there own clause
    main_clause.RemoveGenesMentionedElsewhere();
    main_clause.RemoveDeletedSubclauses();

    main_clause.Label();

    if (!m_SuppressAltSplicePhrase) {
        main_clause.FindAltSplices();
        main_clause.RemoveDeletedSubclauses();
    } 
    
    main_clause.ConsolidateRepeatedClauses();
    main_clause.RemoveDeletedSubclauses();
    

#if 0
  if (feature_requests->feature_list_type == DEFLINE_USE_FEATURES
      && (! isSegment || (seg_feature_list != NULL && *seg_feature_list != NULL)))
  {
    /* remove features that indexer has chosen to suppress before they are grouped
     * with other features or used to determine loneliness etc.
     */
    RemoveSuppressedFeatures (feature_list, feature_requests->suppressed_feature_list);
  
    GroupmRNAs (feature_list, bsp, feature_requests->suppress_locus_tags);

    /* genes are added to other clauses */
    GroupGenes (feature_list, feature_requests->suppress_locus_tags);

    if (! feature_requests->suppress_alt_splice_phrase)
    {
      /* find alt-spliced CDSs */
      FindAltSplices (*feature_list, bsp, feature_requests->suppress_locus_tags);
    }

    GroupAltSplicedExons (feature_list, bsp, TRUE);
    
    if (!isSegment)
    {
       /* group CDSs that have the same name and are under the same gene together */
      GroupSegmentedCDSs (feature_list, bsp, TRUE, feature_requests->suppress_locus_tags);
    }

    /* now group clauses */
    GroupAllClauses ( feature_list, gene_cluster_opp_strand, bsp );

    ExpandAltSplicedExons (*feature_list, bsp, feature_requests->suppress_locus_tags);

    FindGeneProducts (*feature_list, bsp, feature_requests->suppress_locus_tags);

    if (seg_feature_list != NULL && *seg_feature_list != NULL)
    {
      tmp_feat_list = NULL; 
      ExtractSegmentClauses ( *seg_feature_list, *feature_list, &tmp_feat_list);
      FreeListElement (*feature_list);
      *feature_list = tmp_feat_list;
    }
   
    /* remove exons and other unwanted features */
    RemoveUnwantedFeatures (feature_list, bsp, isSegment, feature_requests);

    RemoveGenesMentionedElsewhere (feature_list, *feature_list, TRUE,
                                   feature_requests->suppress_locus_tags);

    if (feature_requests->remove_subfeatures)
    {
      DeleteSubfeatures (feature_list, TRUE);
    }

    DeleteOperonAndGeneClusterSubfeatures (feature_list, TRUE);

    CountUnknownGenes (feature_list, bsp, feature_requests->suppress_locus_tags);

    if (feature_requests->misc_feat_parse_rule == 1)
    {
      RenameMiscFeats (*feature_list, molecule_type);
    }
    else
    {
      RemoveUnwantedMiscFeats (feature_list, TRUE);
    }

    ReplaceRNAClauses (feature_list, bsp, feature_requests->suppress_locus_tags);

    /* take any exons on the minus strand */
    /* and reverse their order within the clause */
    ReverseClauses (feature_list, IsExon);

    RenameExonSequences ( feature_list, bsp, TRUE);

    LabelClauses (*feature_list, molecule_type, bsp, 
                  feature_requests->suppress_locus_tags);

    /* parse lists of tRNA and intergenic spacer clauses in misc_feat notes */
    /* need to do this after LabelClauses, since LabelClauses labels intergenic
     * spacers with more relaxed restrictions.  The labels from LabelClauses
     * for intergenic spacers are the default values.
     */
    ReplaceIntergenicSpacerClauses (feature_list, bsp, feature_requests->suppress_locus_tags);

#endif    
    
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

string CAutoDef::x_GetFeatureClauseProductEnding(string feature_clauses, CBioseq_Handle bh)
{
    bool pluralize = false;
    
    if (NStr::Find(feature_clauses, "genes") != NCBI_NS_STD::string::npos) {
        pluralize = true;
    } else {
        unsigned int pos = NStr::Find(feature_clauses, "gene");
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
    if (!NStr::IsBlank(ending)) {
        ending = "; " + ending;
    } else {
        ending = OrganelleByGenome(m_ProductFlag);
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


string CAutoDef::GetOneDefLine(CAutoDefModifierCombo *mod_combo, CBioseq_Handle bh)
{
    // for protein sequences, use sequence::GetTitle
    if (bh.CanGetInst() && bh.GetInst().CanGetMol() && bh.GetInst().GetMol() == CSeq_inst::eMol_aa) {
        sequence::TGetTitleFlags flags = sequence::fGetTitle_Reconstruct | sequence::fGetTitle_Organism | sequence::fGetTitle_AllProteins;
        return sequence::GetTitle(bh, flags);
    }
    string org_desc = "Unknown organism";
    unsigned int genome_val = CBioSource::eGenome_unknown;
    
    for (CSeqdesc_CI dit(bh, CSeqdesc::e_Source); dit;  ++dit) {
        const CBioSource& bsrc = dit->GetSource();
        org_desc = x_GetSourceDescriptionString(mod_combo, bsrc);
        if (bsrc.CanGetGenome()) {
            genome_val = bsrc.GetGenome();
        }
        break;
    }
    string feature_clauses = "";
    if (m_FeatureListType == eListAllFeatures) {
        feature_clauses = " " + x_GetFeatureClauses(bh, true, true);
        feature_clauses += x_GetFeatureClauseProductEnding(feature_clauses, bh);
        if (m_AltSpliceFlag) {
            feature_clauses += ", alternatively spliced";
        }
        feature_clauses += ".";
    } else if (m_FeatureListType == eCompleteSequence) {
        feature_clauses = ", complete sequence";
    } else if (m_FeatureListType == eCompleteGenome) {
        string organelle = OrganelleByGenome(genome_val);
        if (!NStr::IsBlank(organelle)) {
            feature_clauses = " " + organelle;
        }
        feature_clauses += ", complete genome";
    }
    
    return org_desc + feature_clauses;
}


END_SCOPE(objects)
END_NCBI_SCOPE

/*
* ===========================================================================
* $Log$
* Revision 1.4  2006/04/18 14:50:36  bollin
* set defline options as member variables for CAutoDef class
*
* Revision 1.3  2006/04/17 17:42:21  ucko
* Drop extraneous and disconcerting inclusion of gui headers.
*
* Revision 1.2  2006/04/17 17:39:37  ucko
* Fix capitalization of header filenames.
*
* Revision 1.1  2006/04/17 16:25:05  bollin
* files for automatically generating definition lines, using a combination
* of modifiers to make definition lines unique within a set and listing the
* relevant features on the sequence.
*
*
* ===========================================================================
*/

