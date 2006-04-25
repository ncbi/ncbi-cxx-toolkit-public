#ifndef OBJMGR_UTIL_AUTODEF_FEATURE_CLAUSE__HPP
#define OBJMGR_UTIL_AUTODEF_FEATURE_CLAUSE__HPP

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
    
class NCBI_XOBJUTIL_EXPORT CAutoDefFeatureClause : public CAutoDefFeatureClause_Base
{
public:    
    CAutoDefFeatureClause(CBioseq_Handle bh, const CSeq_feat &main_feat);
    ~CAutoDefFeatureClause();
    
    virtual void Label();
    virtual CSeqFeatData::ESubtype  GetMainFeatureSubtype();
    
    virtual bool IsRecognizedFeature();
    
    virtual bool IsTransposon();
    virtual bool IsInsertionSequence();
    virtual bool IsControlRegion();
    virtual bool IsEndogenousVirusSourceFeature();
    virtual bool IsGeneCluster();
    virtual bool IsNoncodingProductFeat();
    virtual bool IsIntergenicSpacer();
    virtual bool IsSatelliteClause();
    
    // functions for comparing locations
    virtual sequence::ECompare CompareLocation(const CSeq_loc& loc);
    virtual void AddToLocation(CRef<CSeq_loc> loc);
    virtual bool SameStrand(const CSeq_loc& loc);
    virtual bool IsPartial();
    virtual CRef<CSeq_loc> GetLocation();

    // functions for grouping
    virtual bool AddmRNA (CAutoDefFeatureClause_Base *mRNAClause);
    virtual bool AddGene (CAutoDefFeatureClause_Base *gene_clause);

    virtual bool OkToGroupUnderByType(CAutoDefFeatureClause_Base *parent_clause);
    virtual bool OkToGroupUnderByLocation(CAutoDefFeatureClause_Base *parent_clause, bool gene_cluster_opp_strand);
    virtual CAutoDefFeatureClause_Base *FindBestParentClause(CAutoDefFeatureClause_Base * subclause, bool gene_cluster_opp_strand);
    virtual void ReverseCDSClauseLists();
    
    virtual bool ShouldRemoveExons();
    
    virtual bool IsBioseqPrecursorRNA();

protected:
    CAutoDefFeatureClause();

    bool x_GetGenericInterval (string &interval);
   
    const CSeq_feat& m_MainFeat;
    CRef<CSeq_loc> m_ClauseLocation;
    CMolInfo::TBiomol m_Biomol;

private:
    CBioseq_Handle m_BH;
    
    bool x_GetFeatureTypeWord(string &typeword);
    bool x_ShowTypewordFirst(string typeword);
    bool x_GetProductName(string &product_name);
    bool x_GetDescription(string &description);
    void x_SetBiomol();
    
    bool x_GetNoncodingProductFeatProduct (string &product);
    bool x_FindNoncodingFeatureKeywordProduct (string comment, string keyword, string &product_name);
    string x_GetGeneName(const CGene_ref& gref);
    bool x_MatchGene(const CGene_ref& gref);
    bool x_GetExonDescription(string &description);
       
};

class NCBI_XOBJUTIL_EXPORT CAutoDefTransposonClause : public CAutoDefFeatureClause
{
public:    
    CAutoDefTransposonClause(CBioseq_Handle bh, const CSeq_feat &main_feat);
    ~CAutoDefTransposonClause();
  
    virtual void Label();  
    virtual bool IsTransposon() { return true; }
};


class NCBI_XOBJUTIL_EXPORT CAutoDefSatelliteClause : public CAutoDefFeatureClause
{
public:    
    CAutoDefSatelliteClause(CBioseq_Handle bh, const CSeq_feat &main_feat);
    ~CAutoDefSatelliteClause();
  
    virtual void Label();  
    virtual bool IsSatelliteClause() { return true; }
};


class NCBI_XOBJUTIL_EXPORT CAutoDefPromoterClause : public CAutoDefFeatureClause
{
public:    
    CAutoDefPromoterClause(CBioseq_Handle bh, const CSeq_feat &main_feat);
    ~CAutoDefPromoterClause();
  
    virtual void Label();  
};


class NCBI_XOBJUTIL_EXPORT CAutoDefIntergenicSpacerClause : public CAutoDefFeatureClause
{
public:    
    CAutoDefIntergenicSpacerClause(CBioseq_Handle bh, const CSeq_feat &main_feat);
    ~CAutoDefIntergenicSpacerClause();
  
    virtual void Label();
    virtual bool IsIntergenicSpacer() { return true; }
};

class NCBI_XOBJUTIL_EXPORT CAutoDefParsedClause : public CAutoDefFeatureClause
{
public:
    CAutoDefParsedClause(CBioseq_Handle bh, const CSeq_feat &main_feat, bool is_first, bool is_last);
    ~CAutoDefParsedClause();
    void SetTypeword(string typeword) { m_Typeword = typeword; m_TypewordChosen = true; }
    void SetDescription(string description) { m_Description = description; m_DescriptionChosen = true; }
    void SetTypewordFirst(bool typeword_first) { m_ShowTypewordFirst = typeword_first; }
    virtual bool IsRecognizedFeature() { return true; }
    
protected:
};

class NCBI_XOBJUTIL_EXPORT CAutoDefParsedtRNAClause : public CAutoDefParsedClause
{
public:
    CAutoDefParsedtRNAClause(CBioseq_Handle bh, const CSeq_feat &main_feat, string gene_name, string product_name, bool is_first, bool is_last);
    ~CAutoDefParsedtRNAClause();

    virtual CSeqFeatData::ESubtype  GetMainFeatureSubtype() { return CSeqFeatData::eSubtype_tRNA; }
};

class NCBI_XOBJUTIL_EXPORT CAutoDefParsedIntergenicSpacerClause : public CAutoDefIntergenicSpacerClause
{
public:
    CAutoDefParsedIntergenicSpacerClause(CBioseq_Handle bh, const CSeq_feat &main_feat, string description, bool is_first, bool is_last);
    ~CAutoDefParsedIntergenicSpacerClause();
};


class NCBI_XOBJUTIL_EXPORT CAutoDefGeneClusterClause : public CAutoDefFeatureClause
{
public:    
    CAutoDefGeneClusterClause(CBioseq_Handle bh, const CSeq_feat &main_feat);
    ~CAutoDefGeneClusterClause();
  
    virtual void Label();
    virtual bool IsGeneCluster() { return true; }
};


class NCBI_XOBJUTIL_EXPORT CAutoDefMiscCommentClause : public CAutoDefFeatureClause
{
public:    
    CAutoDefMiscCommentClause(CBioseq_Handle bh, const CSeq_feat &main_feat);
    ~CAutoDefMiscCommentClause();
  
    virtual void Label();
    virtual bool IsRecognizedFeature() { return true; }
};


END_SCOPE(objects)
END_NCBI_SCOPE

/*
* ===========================================================================
* $Log$
* Revision 1.4  2006/04/25 13:36:28  bollin
* added misc_feat processing and removal of unwanted features
*
* Revision 1.3  2006/04/18 16:54:22  bollin
* added support for parsing misc_RNA features
*
* Revision 1.2  2006/04/17 17:39:36  ucko
* Fix capitalization of header filenames.
*
* Revision 1.1  2006/04/17 16:25:01  bollin
* files for automatically generating definition lines, using a combination
* of modifiers to make definition lines unique within a set and listing the
* relevant features on the sequence.
*
*
* ===========================================================================
*/

#endif //OBJMGR_UTIL_AUTODEF_FEATURE_CLAUSE__HPP
