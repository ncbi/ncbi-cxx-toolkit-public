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

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)    

class NCBI_XOBJEDIT_EXPORT CAutoDef
{
public:
    enum EFeatureListType {
        eListAllFeatures = 0,
        eCompleteSequence,
        eCompleteGenome,
        ePartialSequence,
        ePartialGenome,
        eSequence
    };
    
    enum EMiscFeatRule {
        eDelete = 0,
        eNoncodingProductFeat,
        eCommentFeat
    };

    typedef set<objects::CFeatListItem>  TFeatTypeItemSet;
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
    static string GetDocsumOrgDescription(CSeq_entry_Handle se);
    string GetDocsumDefLine(CSeq_entry_Handle se);
    static bool NeedsDocsumDefline(const CBioseq_set& set);

    void DoAutoDef();
    
    void SetFeatureListType(unsigned int feature_list_type);
    void SetMiscFeatRule(unsigned int misc_feat_rule);
    void SetProductFlag (unsigned int product_flag);
	void SetSpecifyNuclearProduct (bool specify_nuclear_product);
    void SetAltSpliceFlag (bool alt_splice_flag);
    void SetSuppressLocusTags(bool suppress_locus_tags);
    void SetGeneClusterOppStrand(bool gene_opp_strand);
    void SetSuppressFeatureAltSplice (bool suppress_alt_splice);
    void SuppressTransposonAndInsertionSequenceSubfeatures(bool suppress);
    void SetKeepExons(bool keep);
    void SetKeepIntrons(bool keep);
    void SetKeepPromoters(bool keep);
    void SetKeepLTRs(bool keep);
    void SetKeep3UTRs(bool keep);
    void SetKeep5UTRs(bool keep);
	void SetUseNcRNAComment (bool use_comment);
    void SetUseFakePromoters (bool use_fake);
    
    void SuppressFeature(objects::CFeatListItem feat);    
    
    typedef vector<CAutoDefModifierCombo *> TModifierComboVector;
    
    void GetAvailableModifiers(TAvailableModifierSet &mod_set);
    
    void Cancel() { m_Cancelled = true; }
    bool Cancelled() { return m_Cancelled; }
    
private:
    typedef vector<unsigned int> TModifierIndexVector;
    typedef vector<CSeq_entry_Handle> TSeqEntryHandleVector;

    CAutoDefModifierCombo m_OrigModCombo;

    TFeatTypeItemSet m_SuppressedFeatures;
    
    // feature clause specifications
    unsigned int m_FeatureListType;
    unsigned int m_MiscFeatRule;
    bool         m_SpecifyNuclearProduct;
    unsigned int m_ProductFlag;
    bool         m_AltSpliceFlag;
    bool         m_SuppressAltSplicePhrase;
    bool         m_SuppressLocusTags;
    bool         m_GeneOppStrand;
    bool         m_RemoveTransposonAndInsertionSequenceSubfeatures;
    bool         m_KeepExons;
    bool         m_KeepIntrons;
    bool         m_KeepPromoters;
    bool         m_KeepLTRs;
    bool         m_Keep3UTRs;
    bool         m_Keep5UTRs;
	bool         m_UseNcRNAComment;
    bool         m_UseFakePromoters;
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
    vector<CAutoDefFeatureClause *> x_GetIntergenicSpacerClauseList
        (string comment, CBioseq_Handle bh, const CSeq_feat& cf,
         const CSeq_loc& mapped_loc, bool suppress_locus_tags);
    bool x_AddIntergenicSpacerFeatures(CBioseq_Handle bh,
                                       const CSeq_feat& cf,
                                       const CSeq_loc& mapped_loc,
                                       CAutoDefFeatureClause_Base& main_clause,
                                       bool suppress_locus_tags);
    CAutoDefParsedtRNAClause* x_tRNAClauseFromNote(CBioseq_Handle bh,
                                                   const CSeq_feat& cf,
                                                   const CSeq_loc& mapped_loc,
                                                   string comment,
                                                   bool is_first,
                                                   bool is_last);
    bool x_AddMiscRNAFeatures(CBioseq_Handle bh,
                              const CSeq_feat& cf,
                              const CSeq_loc& mapped_loc,
                              CAutoDefFeatureClause_Base& main_clause,
                              bool suppress_locus_tags);
                              
    void x_RemoveOptionalFeatures(CAutoDefFeatureClause_Base *main_clause, CBioseq_Handle bh);
                                  
    
    bool x_IsOrgModRequired(unsigned int mod_type);
    bool x_IsSubSrcRequired(unsigned int mod_type);
    
    bool x_IsFeatureSuppressed(CSeqFeatData::ESubtype subtype);
    
    void GetMasterLocation(CBioseq_Handle &bh, CRange<TSeqPos>& range);
    bool IsSegment(CBioseq_Handle bh);
    bool x_Is5SList(CFeat_CI feat_ci);

};  //  end of CAutoDef


inline
void CAutoDef::SetFeatureListType(unsigned int feature_list_type)
{
    m_FeatureListType = feature_list_type;
}


inline
void CAutoDef::SetMiscFeatRule(unsigned int misc_feat_rule)
{
    m_MiscFeatRule = misc_feat_rule;
}


inline
void CAutoDef::SetProductFlag(unsigned int product_flag)
{
    m_SpecifyNuclearProduct = false;
    m_ProductFlag = product_flag;
}


inline
void CAutoDef::SetSpecifyNuclearProduct (bool specify_nuclear_product)
{
    m_SpecifyNuclearProduct = specify_nuclear_product;
	m_ProductFlag = CBioSource::eGenome_unknown;
}


inline
void CAutoDef::SetAltSpliceFlag (bool alt_splice_flag)
{
    m_AltSpliceFlag = alt_splice_flag;
}


inline
void CAutoDef::SetSuppressLocusTags (bool suppress_locus_tags)
{
    m_SuppressLocusTags = suppress_locus_tags;
}


inline
void CAutoDef::SetGeneClusterOppStrand (bool gene_opp_strand)
{
    m_GeneOppStrand = gene_opp_strand;
}


inline
void CAutoDef::SetSuppressFeatureAltSplice (bool suppress_alt_splice)
{
    m_SuppressAltSplicePhrase = suppress_alt_splice;
}


inline
void CAutoDef::SuppressTransposonAndInsertionSequenceSubfeatures(bool suppress)
{
    m_RemoveTransposonAndInsertionSequenceSubfeatures = suppress;
}


inline
void CAutoDef::SetKeepExons(bool keep)
{
    m_KeepExons = keep;
}


inline
void CAutoDef::SetKeepIntrons(bool keep)
{
    m_KeepIntrons = keep;
}


inline
void CAutoDef::SetKeepPromoters(bool keep)
{
    m_KeepPromoters = keep;
}


inline
void CAutoDef::SetKeepLTRs(bool keep)
{
    m_KeepLTRs = keep;
}


inline
void CAutoDef::SetKeep3UTRs(bool keep)
{
    m_Keep3UTRs = keep;
}


inline
void CAutoDef::SetKeep5UTRs(bool keep)
{
    m_Keep5UTRs = keep;
}


inline
void CAutoDef::SetUseNcRNAComment(bool use_comment)
{
    m_UseNcRNAComment = use_comment;
}


inline
void CAutoDef::SetUseFakePromoters(bool use_fake)
{
    m_UseFakePromoters = use_fake;
    m_KeepPromoters = true;
}



END_SCOPE(objects)
END_NCBI_SCOPE

#endif //OBJTOOLS_EDIT___AUTODEF__HPP
