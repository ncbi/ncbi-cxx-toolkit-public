#ifndef OBJMGR_UTIL___AUTODEF_FEATURE_CLAUSE__HPP
#define OBJMGR_UTIL___AUTODEF_FEATURE_CLAUSE__HPP

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

#include <objmgr/util/autodef_feature_clause_base.hpp>
#include <objmgr/bioseq_handle.hpp>
#include <objects/seq/MolInfo.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)
    
class NCBI_XOBJEDIT_EXPORT CAutoDefFeatureClause : public CAutoDefFeatureClause_Base
{
public:    
    CAutoDefFeatureClause(CBioseq_Handle bh, const CSeq_feat &main_feat, const CSeq_loc &mapped_loc, const CAutoDefOptions& opts);
    ~CAutoDefFeatureClause();

    enum EClauseType {
        eDefault = 0,
        eEndogenousVirusRepeatRegion
    };
    virtual EClauseType GetClauseType() const;
    
    virtual void Label(bool suppress_allele);
    virtual CSeqFeatData::ESubtype  GetMainFeatureSubtype() const;
    
    virtual bool IsRecognizedFeature() const;
    
    virtual bool IsMobileElement() const;
    virtual bool IsInsertionSequence() const;
    virtual bool IsControlRegion() const;
    virtual bool IsEndogenousVirusSourceFeature() const;
    virtual bool IsGeneCluster() const;
    virtual bool IsNoncodingProductFeat() const;
    virtual bool IsSatelliteClause() const;
    virtual bool IsPromoter() const;
    virtual bool IsLTR() const;
    static bool IsSatellite(const CSeq_feat& feat);
    static bool IsPromoter(const CSeq_feat& feat);
    static bool IsGeneCluster (const CSeq_feat& feat);
    static bool IsControlRegion (const CSeq_feat& feat);
    static bool IsLTR(const CSeq_feat& feat);

    
    // functions for comparing locations
    virtual sequence::ECompare CompareLocation(const CSeq_loc& loc) const;
    virtual void AddToLocation(CRef<CSeq_loc> loc, bool also_set_partials = true);
    virtual bool SameStrand(const CSeq_loc& loc) const;
    virtual bool IsPartial() const;
    virtual CRef<CSeq_loc> GetLocation() const;

    // functions for grouping
    virtual bool AddmRNA (CAutoDefFeatureClause_Base *mRNAClause);
    virtual bool AddGene (CAutoDefFeatureClause_Base *gene_clause, bool suppress_allele);

    virtual bool OkToGroupUnderByType(const CAutoDefFeatureClause_Base *parent_clause) const;
    virtual bool OkToGroupUnderByLocation(const CAutoDefFeatureClause_Base *parent_clause, bool gene_cluster_opp_strand) const;
    virtual CAutoDefFeatureClause_Base *FindBestParentClause(CAutoDefFeatureClause_Base * subclause, bool gene_cluster_opp_strand);
    virtual void ReverseCDSClauseLists();
    
    virtual bool ShouldRemoveExons() const;
    virtual bool IsExonWithNumber() const;
    
    virtual bool IsBioseqPrecursorRNA() const;

    static bool IsPseudo(const CSeq_feat& f);
    bool DoesmRNAProductNameMatch(const string& mrna_product) const;

protected:
    CAutoDefFeatureClause();

    bool x_GetGenericInterval (string &interval, bool suppress_allele);
    void x_GetOperonSubfeatures(string &interval);

    virtual bool x_IsPseudo();

    void x_TypewordFromSequence();
   
    CConstRef<CSeq_feat> m_pMainFeat;
    CRef<CSeq_loc> m_ClauseLocation;
    CMolInfo::TBiomol m_Biomol;

protected:
    CBioseq_Handle m_BH;
    
    bool x_GetFeatureTypeWord(string &typeword);
    bool x_ShowTypewordFirst(string typeword);
    virtual bool x_GetProductName(string &product_name);
    bool x_GetDescription(string &description);
    void x_SetBiomol();
    
    bool x_GetNoncodingProductFeatProduct (string &product) const;
    bool x_FindNoncodingFeatureKeywordProduct (string comment, string keyword, string &product_name) const;
    string x_GetGeneName(const CGene_ref& gref, bool suppress_locus_tag) const;
    bool x_GetExonDescription(string &description);
       
};


class NCBI_XOBJEDIT_EXPORT CAutoDefGeneClause : public CAutoDefFeatureClause
{
public:
    CAutoDefGeneClause(CBioseq_Handle bh, const CSeq_feat &main_feat, const CSeq_loc &mapped_loc, const CAutoDefOptions& opts);
    ~CAutoDefGeneClause() {}
    bool GetSuppressLocusTag() { return m_Opts.GetSuppressLocusTags(); }

protected:
    virtual bool x_IsPseudo();
    virtual bool x_GetProductName(string &product_name);
};

class NCBI_XOBJEDIT_EXPORT CAutoDefNcRNAClause : public CAutoDefFeatureClause
{
public:    
    CAutoDefNcRNAClause(CBioseq_Handle bh, const CSeq_feat &main_feat, const CSeq_loc &mapped_loc, const CAutoDefOptions& opts);
    ~CAutoDefNcRNAClause();

protected:
	virtual bool x_GetProductName(string &product_name);
	bool m_UseComment;
};


class NCBI_XOBJEDIT_EXPORT CAutoDefMobileElementClause : public CAutoDefFeatureClause
{
public:    
    CAutoDefMobileElementClause(CBioseq_Handle bh, const CSeq_feat &main_feat, const CSeq_loc &mapped_loc, const CAutoDefOptions& opts);
    ~CAutoDefMobileElementClause();
  
    virtual void Label(bool suppress_allele);
    virtual bool IsMobileElement() const { return true; }
    bool IsOptional();
};


class NCBI_XOBJEDIT_EXPORT CAutoDefSatelliteClause : public CAutoDefFeatureClause
{
public:    
    CAutoDefSatelliteClause(CBioseq_Handle bh, const CSeq_feat &main_feat, const CSeq_loc &mapped_loc, const CAutoDefOptions& opts);
    ~CAutoDefSatelliteClause();
  
    virtual void Label(bool suppress_allele);
    virtual bool IsSatelliteClause() const { return true; }
};


class NCBI_XOBJEDIT_EXPORT CAutoDefPromoterClause : public CAutoDefFeatureClause
{
public:    
    CAutoDefPromoterClause(CBioseq_Handle bh, const CSeq_feat &main_feat, const CSeq_loc &mapped_loc, const CAutoDefOptions& opts);
    ~CAutoDefPromoterClause();
  
    virtual void Label(bool suppress_allele);
    virtual bool IsPromoter() const { return true; }
};


class NCBI_XOBJEDIT_EXPORT CAutoDefIntergenicSpacerClause : public CAutoDefFeatureClause
{
public:    
    CAutoDefIntergenicSpacerClause(CBioseq_Handle bh, const CSeq_feat &main_feat, const CSeq_loc &mapped_loc, const CAutoDefOptions& opts);
    CAutoDefIntergenicSpacerClause(CBioseq_Handle bh, const CSeq_feat &main_feat, const CSeq_loc &mapped_loc, string comment, const CAutoDefOptions& opts);
    ~CAutoDefIntergenicSpacerClause();
  
    virtual void Label(bool suppress_allele);
protected:
    void InitWithString (string comment, bool suppress_allele);
};

class NCBI_XOBJEDIT_EXPORT CAutoDefParsedClause : public CAutoDefFeatureClause
{
public:
    CAutoDefParsedClause(CBioseq_Handle bh, const CSeq_feat &main_feat, const CSeq_loc &mapped_loc, bool is_first, bool is_last, const CAutoDefOptions& opts);
    ~CAutoDefParsedClause();
    void SetTypeword(string typeword) { m_Typeword = typeword; m_TypewordChosen = true; }
    void SetDescription(string description) { m_Description = description; m_DescriptionChosen = true; }
    void SetTypewordFirst(bool typeword_first) { m_ShowTypewordFirst = typeword_first; }
    virtual bool IsRecognizedFeature() const { return true; }
    void SetMiscRNAWord(const string& phrase);
    
protected:
};

class NCBI_XOBJEDIT_EXPORT CAutoDefParsedtRNAClause : public CAutoDefParsedClause
{
public:
    CAutoDefParsedtRNAClause(CBioseq_Handle bh, const CSeq_feat &main_feat, const CSeq_loc &mapped_loc, string gene_name, string product_name, bool is_first, bool is_last, const CAutoDefOptions& opts);
    ~CAutoDefParsedtRNAClause();

    virtual CSeqFeatData::ESubtype  GetMainFeatureSubtype() const { return CSeqFeatData::eSubtype_tRNA; }
    static bool ParseString(string comment, string& gene_name, string& product_name);
};

class NCBI_XOBJEDIT_EXPORT CAutoDefParsedIntergenicSpacerClause : public CAutoDefIntergenicSpacerClause
{
public:
    CAutoDefParsedIntergenicSpacerClause(CBioseq_Handle bh, const CSeq_feat &main_feat, const CSeq_loc &mapped_loc, const string& description, bool is_first, bool is_last, const CAutoDefOptions& opts);
    ~CAutoDefParsedIntergenicSpacerClause();

    void MakeRegion() { if (!NStr::EndsWith(m_Typeword, "region")) m_Typeword += " region"; }
};


class NCBI_XOBJEDIT_EXPORT CAutoDefGeneClusterClause : public CAutoDefFeatureClause
{
public:    
    CAutoDefGeneClusterClause(CBioseq_Handle bh, const CSeq_feat &main_feat, const CSeq_loc &mapped_loc, const CAutoDefOptions& opts);
    ~CAutoDefGeneClusterClause();
  
    virtual void Label(bool suppress_allele);
    virtual bool IsGeneCluster() const { return true; }
};


class NCBI_XOBJEDIT_EXPORT CAutoDefMiscCommentClause : public CAutoDefFeatureClause
{
public:    
    CAutoDefMiscCommentClause(CBioseq_Handle bh, const CSeq_feat &main_feat, const CSeq_loc &mapped_loc, const CAutoDefOptions& opts);
    ~CAutoDefMiscCommentClause();
  
    virtual void Label(bool suppress_allele);
    virtual bool IsRecognizedFeature() const { return true; }
};


class NCBI_XOBJEDIT_EXPORT CAutoDefParsedRegionClause : public CAutoDefFeatureClause
{
public:
    CAutoDefParsedRegionClause(CBioseq_Handle bh, const CSeq_feat &main_feat, const CSeq_loc &mapped_loc, string product, const CAutoDefOptions& opts);
    ~CAutoDefParsedRegionClause();

    virtual void Label(bool suppress_allele);
    virtual bool IsRecognizedFeature() const { return true; }
};


class NCBI_XOBJEDIT_EXPORT CAutoDefFakePromoterClause : public CAutoDefFeatureClause
{
public:
    CAutoDefFakePromoterClause(CBioseq_Handle bh, const CSeq_feat &main_feat, const CSeq_loc &mapped_loc, const CAutoDefOptions& opts);
    ~CAutoDefFakePromoterClause();

    virtual void Label(bool suppress_allele);
    virtual bool IsPromoter() const { return true; }
    virtual CSeqFeatData::ESubtype  GetMainFeatureSubtype() const { return CSeqFeatData::eSubtype_promoter; };
    
    virtual bool IsRecognizedFeature() const { return false; };
    
    virtual bool IsMobileElement() const { return false; };
    virtual bool IsInsertionSequence() const { return false; };
    virtual bool IsControlRegion() const { return false; };
    virtual bool IsEndogenousVirusSourceFeature() const { return false; };
    virtual bool IsGeneCluster() const { return false; };
    virtual bool IsNoncodingProductFeat() const { return false; };
    virtual bool IsSatelliteClause() const { return false; };

    virtual bool OkToGroupUnderByLocation(const CAutoDefFeatureClause_Base *parent_clause, bool gene_cluster_opp_strand) const;
    virtual bool OkToGroupUnderByType(const CAutoDefFeatureClause_Base *parent_clause) const;

};


class NCBI_XOBJEDIT_EXPORT CAutoDefPromoterAnd5UTRClause : public CAutoDefFeatureClause
{
public:
    CAutoDefPromoterAnd5UTRClause(CBioseq_Handle bh, const CSeq_feat &main_feat, const CSeq_loc &mapped_loc, const CAutoDefOptions& opts);
    ~CAutoDefPromoterAnd5UTRClause() {};

    virtual void Label(bool suppress_allele);
    virtual bool IsPromoter() const { return true; }
    virtual CSeqFeatData::ESubtype  GetMainFeatureSubtype() const { return CSeqFeatData::eSubtype_promoter; };

    virtual bool IsRecognizedFeature() const { return true; };

    static bool IsPromoterAnd5UTR(const CSeq_feat& feat);
};


CAutoDefParsedtRNAClause *s_tRNAClauseFromNote(CBioseq_Handle bh, const CSeq_feat& cf, const CSeq_loc& mapped_loc, string comment, bool is_first, bool is_last, const CAutoDefOptions& opts);


vector<CRef<CAutoDefFeatureClause > > FeatureClauseFactory(CBioseq_Handle bh, const CSeq_feat& cf, const CSeq_loc& mapped_loc, const CAutoDefOptions& opts, bool is_single_misc_feat);


END_SCOPE(objects)
END_NCBI_SCOPE

#endif //OBJMGR_UTIL___AUTODEF_FEATURE_CLAUSE__HPP
