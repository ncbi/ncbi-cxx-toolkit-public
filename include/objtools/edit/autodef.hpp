#ifndef OBJTOOLS_EDIT___AUTODEF__HPP
#define OBJTOOLS_EDIT___AUTODEF__HPP

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
*   Creates unique definition lines for sequences in a set using organism
*   descriptions and feature clauses.
*/

#include <corelib/ncbistd.hpp>
#include <objects/seqfeat/BioSource.hpp>
#include <objects/seqfeat/Org_ref.hpp>
#include <objects/seqfeat/OrgMod.hpp>
#include <objects/seqfeat/SubSource.hpp>
#include <objects/seqfeat/OrgName.hpp>
#include <objects/seqfeat/Seq_feat.hpp>
#include <objects/seq/MolInfo.hpp>

#include <objmgr/bioseq_handle.hpp>
#include <objmgr/seq_entry_handle.hpp>
#include <objmgr/feat_ci.hpp>

#include <objtools/edit/autodef_available_modifier.hpp>
#include <objtools/edit/autodef_source_desc.hpp>
#include <objtools/edit/autodef_source_group.hpp>
#include <objtools/edit/autodef_mod_combo.hpp>
#include <objtools/edit/autodef_feature_clause.hpp>
#include <objtools/edit/autodef_options.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)    

class NCBI_XOBJEDIT_EXPORT CAutoDef
{
public:
    
    typedef set<CAutoDefAvailableModifier> TAvailableModifierSet;

    CAutoDef();
    ~CAutoDef();
 
    void AddSources(CSeq_entry_Handle se);
    void AddSources(CBioseq_Handle bh);
    CAutoDefModifierCombo* FindBestModifierCombo();
    CAutoDefModifierCombo* GetAllModifierCombo();
    CAutoDefModifierCombo* GetEmptyCombo();
    unsigned int GetNumAvailableModifiers();
    string GetOneSourceDescription(CBioseq_Handle bh);
    string GetOneFeatureClauseList(CBioseq_Handle bh, unsigned int genome_val);
    string GetOneDefLine(CAutoDefModifierCombo* mod_combo, CBioseq_Handle bh);
    string GetOneDefLine(CBioseq_Handle bh);
    static string GetDocsumOrgDescription(CSeq_entry_Handle se);
    string GetDocsumDefLine(CSeq_entry_Handle se);
    static bool NeedsDocsumDefline(const CBioseq_set& set);

    static bool RegenerateDefLines(CSeq_entry_Handle se);
    
    void SetOptionsObject(const CUser_object& user);
    CRef<CUser_object> GetOptionsObject() const { return m_Options.MakeUserObject(); }

    void SetFeatureListType(CAutoDefOptions::EFeatureListType feature_list_type);
    void SetMiscFeatRule(CAutoDefOptions::EMiscFeatRule misc_feat_rule);
    void SetProductFlag(CBioSource::EGenome product_flag);
	void SetSpecifyNuclearProduct (bool specify_nuclear_product);
    void SetAltSpliceFlag (bool alt_splice_flag);
    void SetSuppressLocusTags(bool suppress_locus_tags);
    void SetSuppressAllele(bool suppress_allele);
    void SetGeneClusterOppStrand(bool gene_opp_strand);
    void SetSuppressFeatureAltSplice (bool suppress_alt_splice);
    void SuppressMobileElementAndInsertionSequenceSubfeatures(bool suppress);
    void SetKeepExons(bool keep);
    void SetKeepIntrons(bool keep);
    void SetKeepPromoters(bool keep);
    void SetKeepLTRs(bool keep);
    void SetKeep3UTRs(bool keep);
    void SetKeep5UTRs(bool keep);
    void SetKeepuORFs(bool keep);
    void SetKeepOptionalMobileElements(bool keep);
    void SetKeepPrecursorRNA(bool keep);
    void SetKeepRepeatRegion(bool keep);
	void SetUseNcRNAComment (bool use_comment);
    void SetUseFakePromoters (bool use_fake);
    
    void SuppressFeature(objects::CFeatListItem feat); 
    void SuppressFeature(objects::CSeqFeatData::ESubtype subtype);
    
    typedef vector<CAutoDefModifierCombo *> TModifierComboVector;
    
    void GetAvailableModifiers(TAvailableModifierSet &mod_set);
    
    void Cancel() { m_Cancelled = true; }
    bool Cancelled() { return m_Cancelled; }
    
private:
    typedef vector<unsigned int> TModifierIndexVector;
    typedef vector<CSeq_entry_Handle> TSeqEntryHandleVector;

    CAutoDefModifierCombo m_OrigModCombo;

    CAutoDefOptions m_Options;

    // feature clause specifications
    bool         m_Cancelled;
    
    void x_SortModifierListByRank
        (TModifierIndexVector& index_list,
         CAutoDefSourceDescription::TAvailableModifierVector& modifier_list);
    void x_GetModifierIndexList
        (TModifierIndexVector& index_list,
         CAutoDefSourceDescription::TAvailableModifierVector& modifier_list);
    
    string x_GetNonFeatureListEnding();
    string x_GetFeatureClauses(CBioseq_Handle bh);
    string x_GetFeatureClauseProductEnding(const string& feature_clauses,
                                           CBioseq_Handle bh);
    bool x_AddIntergenicSpacerFeatures(CBioseq_Handle bh,
                                       const CSeq_feat& cf,
                                       const CSeq_loc& mapped_loc,
                                       CAutoDefFeatureClause_Base& main_clause);
    bool x_AddMiscRNAFeatures(CBioseq_Handle bh,
                              const CSeq_feat& cf,
                              const CSeq_loc& mapped_loc,
                              CAutoDefFeatureClause_Base& main_clause);
    bool x_AddtRNAAndOther(CBioseq_Handle bh,
                              const CSeq_feat& cf,
                              const CSeq_loc& mapped_loc,
                              CAutoDefFeatureClause_Base& main_clause);
                              
    void x_RemoveOptionalFeatures(CAutoDefFeatureClause_Base *main_clause, CBioseq_Handle bh);
                                  
    
    bool x_IsOrgModRequired(unsigned int mod_type);
    bool x_IsSubSrcRequired(unsigned int mod_type);
    
    bool x_IsFeatureSuppressed(CSeqFeatData::ESubtype subtype);
    
    void GetMasterLocation(CBioseq_Handle &bh, CRange<TSeqPos>& range);
    bool IsSegment(CBioseq_Handle bh);
    bool x_Is5SList(CFeat_CI feat_ci);

};  //  end of CAutoDef


inline
void CAutoDef::SetFeatureListType(CAutoDefOptions::EFeatureListType feature_list_type)
{
    m_Options.SetFeatureListType(feature_list_type);
}


inline
void CAutoDef::SetMiscFeatRule(CAutoDefOptions::EMiscFeatRule misc_feat_rule)
{
    m_Options.SetMiscFeatRule(misc_feat_rule);
}


inline
void CAutoDef::SetProductFlag(CBioSource::EGenome product_flag)
{
    m_Options.SetProductFlag(product_flag);
}


inline
void CAutoDef::SetSpecifyNuclearProduct (bool specify_nuclear_product)
{
    m_Options.SetSpecifyNuclearProduct(specify_nuclear_product);
}


inline
void CAutoDef::SetAltSpliceFlag (bool alt_splice_flag)
{
    m_Options.SetAltSpliceFlag(alt_splice_flag);
}


inline
void CAutoDef::SetSuppressLocusTags (bool suppress_locus_tags)
{
    m_Options.SetSuppressLocusTags(suppress_locus_tags);
}


inline
void CAutoDef::SetGeneClusterOppStrand (bool gene_opp_strand)
{
    m_Options.SetGeneClusterOppStrand(gene_opp_strand);
}


inline
void CAutoDef::SetSuppressFeatureAltSplice (bool suppress_alt_splice)
{
    m_Options.SetSuppressFeatureAltSplice(suppress_alt_splice);
}


inline
void CAutoDef::SuppressMobileElementAndInsertionSequenceSubfeatures(bool suppress)
{
    m_Options.SetSuppressMobileElementSubfeatures(suppress);
}


inline
void CAutoDef::SetKeepExons(bool keep)
{
    m_Options.SetKeepExons(keep);
}


inline
void CAutoDef::SetKeepIntrons(bool keep)
{
    m_Options.SetKeepIntrons(keep);
}


inline
void CAutoDef::SetKeepPromoters(bool keep)
{
    m_Options.SetKeepPromoters(keep);
}


inline
void CAutoDef::SetKeepLTRs(bool keep)
{
    m_Options.SetKeepLTRs(keep);
}


inline
void CAutoDef::SetKeep3UTRs(bool keep)
{
    m_Options.SetKeep3UTRs(keep);
}


inline
void CAutoDef::SetKeep5UTRs(bool keep)
{
    m_Options.SetKeep5UTRs(keep);
}


inline
void CAutoDef::SetKeepuORFs(bool keep)
{
    m_Options.SetKeepuORFs(keep);
}


inline
void CAutoDef::SetKeepOptionalMobileElements(bool keep)
{
    m_Options.SetKeepMobileElements(keep);
}

inline
void CAutoDef::SetKeepPrecursorRNA(bool keep)
{
    m_Options.SetKeepPrecursorRNA(keep);
}

inline
void CAutoDef::SetKeepRepeatRegion(bool keep)
{
    m_Options.SetKeepRepeatRegion(keep);
}

inline
void CAutoDef::SetUseNcRNAComment(bool use_comment)
{
    m_Options.SetUseNcRNAComment(use_comment);
}


inline
void CAutoDef::SetUseFakePromoters(bool use_fake)
{
    m_Options.SetUseFakePromoters(use_fake);
}



END_SCOPE(objects)
END_NCBI_SCOPE

#endif //OBJTOOLS_EDIT___AUTODEF__HPP
