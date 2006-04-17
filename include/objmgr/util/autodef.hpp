#ifndef OBJMGR_UTIL_AUTODEF__HPP
#define OBJMGR_UTIL_AUTODEF__HPP

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
#include <objects/seqfeat/biosource.hpp>
#include <objects/seqfeat/Org_ref.hpp>
#include <objects/seqfeat/orgmod.hpp>
#include <objects/seqfeat/subsource.hpp>
#include <objects/seqfeat/orgname.hpp>
#include <objects/seqfeat/seq_feat.hpp>
#include <objects/seq/molinfo.hpp>

#include <objmgr/bioseq_handle.hpp>
#include <objmgr/seq_entry_handle.hpp>

#include <objmgr/util/autodef_available_modifier.hpp>
#include <objmgr/util/autodef_source_desc.hpp>
#include <objmgr/util/autodef_source_group.hpp>
#include <objmgr/util/autodef_mod_combo.hpp>
#include <objmgr/util/autodef_feature_clause.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)    

class NCBI_XOBJUTIL_EXPORT CAutoDef
{
public:
    enum EFeatureListType {
        eListAllFeatures = 0,
        eCompleteSequence,
        eCompleteGenome
    };

    CAutoDef();
    ~CAutoDef();
 
    void AddSources (CSeq_entry_Handle se);
    void AddSources (CBioseq_Handle bh);
    CAutoDefModifierCombo *FindBestModifierCombo();
    string GetOneSourceDescription(CBioseq_Handle bh);
    void CAutoDef::DoAutoDef();
    
    string GetOneDefLine(CAutoDefModifierCombo *mod_combo, CBioseq_Handle bh, 
                         EFeatureListType feature_list_type = eListAllFeatures, 
                         unsigned int product_flag = CBioSource::eGenome_unknown, 
                         bool alt_splice_flag = false);
    
    typedef vector<CAutoDefModifierCombo *> TModifierComboVector;
    
private:
    typedef vector<unsigned int> TModifierIndexVector;
    typedef vector<CSeq_entry_Handle> TSeqEntryHandleVector;

    TModifierComboVector  m_ComboList;
    bool m_SuppressAltSplicePhrase;
    
    void x_SortModifierListByRank(TModifierIndexVector &index_list, CAutoDefSourceDescription::TAvailableModifierVector &modifier_list);
    void x_GetModifierIndexList(TModifierIndexVector &index_list, CAutoDefSourceDescription::TAvailableModifierVector &modifier_list);

    string CAutoDef::x_GetSourceDescriptionString (CAutoDefModifierCombo *mod_combo, const CBioSource& bsrc);
    
    string x_GetFeatureClauses(CBioseq_Handle bh, bool suppress_locus_tags, bool gene_cluster_opp_strand);
    string x_GetFeatureClauseProductEnding(string feature_clauses, CBioseq_Handle bh, unsigned int product_flag);
    bool x_AddIntergenicSpacerFeatures(CBioseq_Handle bh, const CSeq_feat& cf, CAutoDefFeatureClause_Base &main_clause, bool suppress_locus_tags);
    CAutoDefParsedtRNAClause *x_tRNAClauseFromNote(CBioseq_Handle bh, const CSeq_feat& cf, string &comment, bool is_first);




};  //  end of CAutoDef


END_SCOPE(objects)
END_NCBI_SCOPE

/*
* ===========================================================================
* $Log$
* Revision 1.1  2006/04/17 16:25:01  bollin
* files for automatically generating definition lines, using a combination
* of modifiers to make definition lines unique within a set and listing the
* relevant features on the sequence.
*
*
* ===========================================================================
*/

#endif //OBJMGR_UTIL_AUTODEF__HPP
