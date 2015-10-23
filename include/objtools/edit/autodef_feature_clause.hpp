#ifndef OBJTOOLS_EDIT___AUTODEF_FEATURE_CLAUSE__HPP
#define OBJTOOLS_EDIT___AUTODEF_FEATURE_CLAUSE__HPP

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

#include <objtools/edit/autodef_feature_clause_base.hpp>
#include <objmgr/bioseq_handle.hpp>
#include <objects/seq/MolInfo.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)
    
class NCBI_XOBJEDIT_EXPORT CAutoDefFeatureClause : public CAutoDefFeatureClause_Base
{
public:    
    CAutoDefFeatureClause(CBioseq_Handle bh, const CSeq_feat &main_feat, const CSeq_loc &mapped_loc);
    ~CAutoDefFeatureClause();

    enum EClauseType {
        eDefault = 0,
        eEndogenousVirusRepeatRegion
    };
    virtual EClauseType GetClauseType();
    
    virtual void Label(bool suppress_allele);
    virtual CSeqFeatData::ESubtype  GetMainFeatureSubtype() const;
    
    virtual bool IsRecognizedFeature();
    
    virtual bool IsMobileElement();
    virtual bool IsInsertionSequence();
    virtual bool IsControlRegion();
    virtual bool IsEndogenousVirusSourceFeature();
    virtual bool IsGeneCluster();
    virtual bool IsNoncodingProductFeat();
    virtual bool IsSatelliteClause();
    virtual bool IsPromoter();
    static bool IsSatellite(const CSeq_feat& feat);
    static bool IsPromoter(const CSeq_feat& feat);
    static bool IsGeneCluster (const CSeq_feat& feat);
    static bool IsControlRegion (const CSeq_feat& feat);

    
    // functions for comparing locations
    virtual sequence::ECompare CompareLocation(const CSeq_loc& loc);
    virtual void AddToLocation(CRef<CSeq_loc> loc, bool also_set_partials = true);
    virtual bool SameStrand(const CSeq_loc& loc);
    virtual bool IsPartial();
    virtual CRef<CSeq_loc> GetLocation();

    // functions for grouping
    virtual bool AddmRNA (CAutoDefFeatureClause_Base *mRNAClause);
    virtual bool AddGene (CAutoDefFeatureClause_Base *gene_clause, bool suppress_allele);

    virtual bool OkToGroupUnderByType(CAutoDefFeatureClause_Base *parent_clause);
    virtual bool OkToGroupUnderByLocation(CAutoDefFeatureClause_Base *parent_clause, bool gene_cluster_opp_strand);
    virtual CAutoDefFeatureClause_Base *FindBestParentClause(CAutoDefFeatureClause_Base * subclause, bool gene_cluster_opp_strand);
    virtual void ReverseCDSClauseLists();
    
    virtual bool ShouldRemoveExons();
    
    virtual bool IsBioseqPrecursorRNA();

protected:
    CAutoDefFeatureClause();

    bool x_GetGenericInterval (string &interval, bool suppress_allele);
    virtual bool x_IsPseudo();
   
    const CSeq_feat& m_MainFeat;
    CRef<CSeq_loc> m_ClauseLocation;
    CMolInfo::TBiomol m_Biomol;

protected:
    CBioseq_Handle m_BH;
    
    bool x_GetFeatureTypeWord(string &typeword);
    bool x_ShowTypewordFirst(string typeword);
    virtual bool x_GetProductName(string &product_name);
    bool x_GetDescription(string &description);
    void x_SetBiomol();
    
    bool x_GetNoncodingProductFeatProduct (string &product);
    bool x_FindNoncodingFeatureKeywordProduct (string comment, string keyword, string &product_name);
    string x_GetGeneName(const CGene_ref& gref, bool suppress_locus_tag) const;
    bool x_GetExonDescription(string &description);
       
};


class NCBI_XOBJEDIT_EXPORT CAutoDefGeneClause : public CAutoDefFeatureClause
{
public:
    CAutoDefGeneClause(CBioseq_Handle bh, const CSeq_feat &main_feat, const CSeq_loc &mapped_loc, bool suppress_locus_tag);
    ~CAutoDefGeneClause() {}
    bool GetSuppressLocusTag() { return m_SuppressLocusTag; }

protected:
    virtual bool x_IsPseudo();
    virtual bool x_GetProductName(string &product_name);
    bool m_SuppressLocusTag;
};

class NCBI_XOBJEDIT_EXPORT CAutoDefNcRNAClause : public CAutoDefFeatureClause
{
public:    
    CAutoDefNcRNAClause(CBioseq_Handle bh, const CSeq_feat &main_feat, const CSeq_loc &mapped_loc, bool use_comment);
    ~CAutoDefNcRNAClause();

protected:
	virtual bool x_GetProductName(string &product_name);
	bool m_UseComment;
};


class NCBI_XOBJEDIT_EXPORT CAutoDefMobileElementClause : public CAutoDefFeatureClause
{
public:    
    CAutoDefMobileElementClause(CBioseq_Handle bh, const CSeq_feat &main_feat, const CSeq_loc &mapped_loc);
    ~CAutoDefMobileElementClause();
  
    virtual void Label(bool suppress_allele);
    virtual bool IsMobileElement() { return true; }
    bool IsOptional();
};


class NCBI_XOBJEDIT_EXPORT CAutoDefSatelliteClause : public CAutoDefFeatureClause
{
public:    
    CAutoDefSatelliteClause(CBioseq_Handle bh, const CSeq_feat &main_feat, const CSeq_loc &mapped_loc);
    ~CAutoDefSatelliteClause();
  
    virtual void Label(bool suppress_allele);
    virtual bool IsSatelliteClause() { return true; }
};


class NCBI_XOBJEDIT_EXPORT CAutoDefPromoterClause : public CAutoDefFeatureClause
{
public:    
    CAutoDefPromoterClause(CBioseq_Handle bh, const CSeq_feat &main_feat, const CSeq_loc &mapped_loc);
    ~CAutoDefPromoterClause();
  
    virtual void Label(bool suppress_allele);
    virtual bool IsPromoter() { return true; }
};


class NCBI_XOBJEDIT_EXPORT CAutoDefIntergenicSpacerClause : public CAutoDefFeatureClause
{
public:    
    CAutoDefIntergenicSpacerClause(CBioseq_Handle bh, const CSeq_feat &main_feat, const CSeq_loc &mapped_loc);
    CAutoDefIntergenicSpacerClause(CBioseq_Handle bh, const CSeq_feat &main_feat, const CSeq_loc &mapped_loc, string comment);
    ~CAutoDefIntergenicSpacerClause();
  
    virtual void Label(bool suppress_allele);
protected:
    void InitWithString (string comment, bool suppress_allele);
};

class NCBI_XOBJEDIT_EXPORT CAutoDefParsedClause : public CAutoDefFeatureClause
{
public:
    CAutoDefParsedClause(CBioseq_Handle bh, const CSeq_feat &main_feat, const CSeq_loc &mapped_loc, bool is_first, bool is_last);
    ~CAutoDefParsedClause();
    void SetTypeword(string typeword) { m_Typeword = typeword; m_TypewordChosen = true; }
    void SetDescription(string description) { m_Description = description; m_DescriptionChosen = true; }
    void SetTypewordFirst(bool typeword_first) { m_ShowTypewordFirst = typeword_first; }
    virtual bool IsRecognizedFeature() { return true; }
    void SetMiscRNAWord(const string& phrase);
    
protected:
};

class NCBI_XOBJEDIT_EXPORT CAutoDefParsedtRNAClause : public CAutoDefParsedClause
{
public:
    CAutoDefParsedtRNAClause(CBioseq_Handle bh, const CSeq_feat &main_feat, const CSeq_loc &mapped_loc, string gene_name, string product_name, bool is_first, bool is_last);
    ~CAutoDefParsedtRNAClause();

    virtual CSeqFeatData::ESubtype  GetMainFeatureSubtype() const { return CSeqFeatData::eSubtype_tRNA; }
    static bool ParseString(string comment, string& gene_name, string& product_name);
};

class NCBI_XOBJEDIT_EXPORT CAutoDefParsedIntergenicSpacerClause : public CAutoDefIntergenicSpacerClause
{
public:
    CAutoDefParsedIntergenicSpacerClause(CBioseq_Handle bh, const CSeq_feat &main_feat, const CSeq_loc &mapped_loc, const string& description, bool is_first, bool is_last);
    ~CAutoDefParsedIntergenicSpacerClause();

    void MakeRegion() { if (!NStr::EndsWith(m_Typeword, "region")) m_Typeword += " region"; }
};


class NCBI_XOBJEDIT_EXPORT CAutoDefGeneClusterClause : public CAutoDefFeatureClause
{
public:    
    CAutoDefGeneClusterClause(CBioseq_Handle bh, const CSeq_feat &main_feat, const CSeq_loc &mapped_loc);
    ~CAutoDefGeneClusterClause();
  
    virtual void Label(bool suppress_allele);
    virtual bool IsGeneCluster() { return true; }
};


class NCBI_XOBJEDIT_EXPORT CAutoDefMiscCommentClause : public CAutoDefFeatureClause
{
public:    
    CAutoDefMiscCommentClause(CBioseq_Handle bh, const CSeq_feat &main_feat, const CSeq_loc &mapped_loc);
    ~CAutoDefMiscCommentClause();
  
    virtual void Label(bool suppress_allele);
    virtual bool IsRecognizedFeature() { return true; }
};


class NCBI_XOBJEDIT_EXPORT CAutoDefParsedRegionClause : public CAutoDefFeatureClause
{
public:
    CAutoDefParsedRegionClause(CBioseq_Handle bh, const CSeq_feat &main_feat, const CSeq_loc &mapped_loc, string product);
    ~CAutoDefParsedRegionClause();

    virtual void Label(bool suppress_allele);
    virtual bool IsRecognizedFeature() { return true; }
};


class NCBI_XOBJEDIT_EXPORT CAutoDefFakePromoterClause : public CAutoDefFeatureClause
{
public:
    CAutoDefFakePromoterClause(CBioseq_Handle bh, const CSeq_feat &main_feat, const CSeq_loc &mapped_loc);
    ~CAutoDefFakePromoterClause();

    virtual void Label(bool suppress_allele);
    virtual bool IsPromoter() { return true; }
    virtual CSeqFeatData::ESubtype  GetMainFeatureSubtype() const { return CSeqFeatData::eSubtype_promoter; };
    
    virtual bool IsRecognizedFeature() { return false; };
    
    virtual bool IsMobileElement() { return false; };
    virtual bool IsInsertionSequence() { return false; };
    virtual bool IsControlRegion() { return false; };
    virtual bool IsEndogenousVirusSourceFeature() { return false; };
    virtual bool IsGeneCluster() { return false; };
    virtual bool IsNoncodingProductFeat() { return false; };
    virtual bool IsSatelliteClause() { return false; };

    virtual bool OkToGroupUnderByLocation(CAutoDefFeatureClause_Base *parent_clause, bool gene_cluster_opp_strand);
    virtual bool OkToGroupUnderByType(CAutoDefFeatureClause_Base *parent_clause);

};


CAutoDefParsedtRNAClause *s_tRNAClauseFromNote(CBioseq_Handle bh, const CSeq_feat& cf, const CSeq_loc& mapped_loc, string comment, bool is_first, bool is_last);


END_SCOPE(objects)
END_NCBI_SCOPE

#endif //OBJTOOLS_EDIT___AUTODEF_FEATURE_CLAUSE__HPP
