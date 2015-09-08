#ifndef OBJTOOLS_EDIT___AUTODEF_OPTIONS__HPP
#define OBJTOOLS_EDIT___AUTODEF_OPTIONS__HPP

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
#include <objects/seqfeat/Seq_feat.hpp>
#include <objects/general/User_object.hpp>
#include <objtools/edit/autodef_available_modifier.hpp>
#include <objtools/edit/autodef_source_desc.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)    

class NCBI_XOBJEDIT_EXPORT CAutoDefOptions
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


    CAutoDefOptions();
    ~CAutoDefOptions();

    CRef<CUser_object> MakeUserObject();
    void InitFromUserObject(const CUser_object& obj);
    

private:

    bool    m_UseLabels;
    bool    m_LeaveParenthetical;
    bool    m_DoNotApplyToSp;
    bool    m_DoNotApplyToNr;
    bool    m_DoNotApplyToCf;
    bool    m_DoNotApplyToAff;
    bool    m_IncludeCountryText;
    bool    m_KeepAfterSemicolon;
    unsigned int m_MaxMods;
    unsigned int m_HIVRule;
    bool         m_NeedHIVRule;

    unsigned int m_FeatureListType;
    unsigned int m_MiscFeatRule;
    unsigned int m_ProductFlag;
    bool m_SpecifyNuclearProduct;
    bool m_AltSpliceFlag;
    bool m_SuppressLocusTags;
    bool m_GeneClusterOppStrand;
    bool m_SuppressFeatureAltSplice;
    bool m_SuppressMobileElementSubfeatures;
    bool m_KeepExons;
    bool m_KeepIntrons;
    bool m_KeepPromoters;
    bool m_UseFakePromoters;
    bool m_KeepLTRs;
    bool m_Keep3UTRs;
    bool m_Keep5UTRs;
    bool m_UseNcRNAComment;

    set<objects::CFeatListItem> m_SuppressedFeatures;

    objects::CAutoDefSourceDescription::TAvailableModifierVector m_ModifierList;
};



END_SCOPE(objects)
END_NCBI_SCOPE

#endif //OBJTOOLS_EDIT___AUTODEF_OPTIONS__HPP
