#ifndef CLEANUP___CLEANUPP__HPP
#define CLEANUP___CLEANUPP__HPP

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
* Author:  Robert Smith
*
* File Description:
*   Implementation of Basic Cleanup of CSeq_entries.
*
*/
#include <corelib/ncbistd.hpp>
#include <corelib/ncbiobj.hpp>
#include <objects/seqfeat/Seq_feat.hpp>
#include <objects/seqfeat/Cdregion.hpp>
#include <objects/seqfeat/Org_ref.hpp>
#include <objects/seq/Seq_descr.hpp>
#include <objects/seq/MolInfo.hpp>
#include <objects/seq/Seqdesc.hpp>
#include <objmgr/scope.hpp>
#include <objtools/cleanup/cleanup_change.hpp>


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

class CSeq_entry;
class CSeq_submit;
class CBioseq;
class CBioseq_set;
class CSeq_annot;
class CSeq_feat;
class CSeqFeatData;
class CSeq_descr;
class CSeqdesc;
class CSeq_loc;
class CGene_ref;
class CProt_ref;
class CRNA_ref;
class CImp_feat;
class CGb_qual;
class CDbtag;
class CUser_field;
class CUser_object;
class CObject_id;
class CGB_block;
class CEMBL_block;
class CPubdesc;
class CPub_equiv;
class CPub;
class CCit_gen;
class CCit_sub;
class CCit_art;
class CCit_book;
class CCit_pat;
class CCit_let;
class CCit_proc;
class CAuth_list;
class CAuthor;
class CAffil;
class CPerson_id;
class CName_std;
class CBioSource;
class COrg_ref;
class COrgName;
class COrgMod;
class CSubSource;
class CMolInfo;
class CCdregion;

/// right now a slightly different cleanup is performed for EMBL/DDBJ and
/// SwissProt records. All other types are handled as GenBank records.
enum ECleanupMode
{
    eCleanup_NotSet,
    eCleanup_EMBL,
    eCleanup_DDBJ,
    eCleanup_SwissProt,
    eCleanup_GenBank
};


class CCleanup_imp
{
public:
    CCleanup_imp(CRef<CCleanupChange> changes, CRef<CScope> scope, Uint4 options = 0);
    virtual ~CCleanup_imp();
        
    /// Cleanup a Seq-entry. 
    void BasicCleanup(CSeq_entry& se);
    /// Cleanup a Seq-submit.
    void BasicCleanup(CSeq_submit& ss);
    /// Cleanup a Bioseq. 
    void BasicCleanup(CBioseq& bs);
    /// Cleanup a Bioseq_set.
    void BasicCleanup(CBioseq_set& bss);
    /// Cleanup a Seq-Annot. 
    void BasicCleanup(CSeq_annot& sa);
    /// Cleanup a Seq-feat.
    void BasicCleanup(CSeq_feat& sf);
    
    // using handles
    /// cleanup fields that object manager cares about. (like changing feature types.)

    /// Cleanup a Seq-entry. 
    void BasicCleanup(CSeq_entry_Handle& seh);
    /// Cleanup a Bioseq. 
    void BasicCleanup(const CBioseq_Handle& bsh);
    /// Cleanup a Bioseq_set.
    void BasicCleanup(CBioseq_set_Handle& bssh);
    /// Cleanup a Seq-Annot. 
    void BasicCleanup(CSeq_annot_Handle& sah);
    /// Cleanup a Seq-feat.
    void BasicCleanup(const CSeq_feat_Handle& sfh);
    
    //Extended Cleanup
    void ExtendedCleanup(CSeq_entry_Handle seh);
    void ExtendedCleanup(CSeq_submit& ss);
    void ExtendedCleanup(CBioseq_Handle bsh);
    void ExtendedCleanup(CBioseq_set_Handle bss);
    void ExtendedCleanup(CSeq_annot_Handle sa);
    
    void RenormalizeNucProtSets (CBioseq_set_Handle bsh);
    void MoveCodingRegionsToNucProtSets (CBioseq_set_Handle bss);

private:
    void Setup(const CSeq_entry& se);
    void Finish(CSeq_entry& se);
    
    void Setup(const CSeq_entry_Handle& seh);
    void Finish(CSeq_entry_Handle& seh);
    
    void ChangeMade(CCleanupChange::EChanges e);

    // Cleanup other sub-objects.
    void BasicCleanup(CSeq_descr& sdr);
    void BasicCleanup(CSeqdesc& sd);
    void BasicCleanup(CSeqFeatData& data);
    void BasicCleanup(CGene_ref& gene_ref);
    void BasicCleanup(CProt_ref& prot_ref);
    void BasicCleanup(CRNA_ref& rna_ref);
    void BasicCleanup(CImp_feat& imf);
    void BasicCleanup(CGb_qual& gbq);
    void BasicCleanup(CDbtag& dbtag);
    void BasicCleanup(CGB_block& gbb);
    void BasicCleanup(CEMBL_block& emb);
    void BasicCleanup(CPubdesc& pd);
    void BasicCleanup(CPub_equiv& pe);
    void BasicCleanup(CPub& pub, bool fix_initials);
    void BasicCleanup(CCit_gen& cg, bool fix_initials);
    void BasicCleanup(CCit_sub& cs, bool fix_initials);
    void BasicCleanup(CCit_art& ca, bool fix_initials);
    void BasicCleanup(CCit_book& cb, bool fix_initials);
    void BasicCleanup(CCit_pat&  cp, bool fix_initials);
    void BasicCleanup(CCit_let&  cl, bool fix_initials);
    void BasicCleanup(CCit_proc& cp, bool fix_initials);
    void BasicCleanup(CAuth_list& al, bool fix_initials);
    void BasicCleanup(CAuthor& au, bool fix_initials);
    void BasicCleanup(CAffil& af);
    void BasicCleanup(CPerson_id& pid, bool fix_initials);
    void BasicCleanup(CName_std& name, bool fix_initials);
    void BasicCleanup(CBioSource& bs);
    void BasicCleanup(COrg_ref& oref);
    void BasicCleanup(COrgName& on);
    void BasicCleanup(COrgMod& om);
    void BasicCleanup(CSubSource& ss);
    void BasicCleanup(CObject_id& oid);
    void BasicCleanup(CUser_field& field);
    void BasicCleanup(CUser_object& uo);
    void BasicCleanup(CSeq_loc& sl);
    void BasicCleanup(CSeq_interval& si);


    void BasicCleanup(CSeq_feat& feat, CSeqFeatData& data);
    bool BasicCleanup(CSeq_feat& feat, CGb_qual& qual);
    bool BasicCleanup(CGene_ref& gene, const CGb_qual& gb_qual);
    bool BasicCleanup(CSeq_feat& feat, CRNA_ref& rna, const CGb_qual& gb_qual);
    bool BasicCleanup(CProt_ref& rna, const CGb_qual& gb_qual);
    bool BasicCleanup(CSeq_feat& feat, CCdregion& cds, const CGb_qual& gb_qual);

    // cleaning up Seq_feat parts.
    void x_CleanupExcept_text(string& except_text);
    void x_CleanupRna(CSeq_feat& feat);
    void x_AddReplaceQual(CSeq_feat& feat, const string& str);
    void x_CombineSplitQual(string& val, string& new_val);

    // Gb_qual cleanup.
    void x_ExpandCombinedQuals(CSeq_feat::TQual& quals);
    void x_CleanupConsSplice(CGb_qual& gbq);
    bool x_CleanupRptUnit(CGb_qual& gbq);

    // Dbtag cleanup.
    void x_TagCleanup(CObject_id& tag);
    void x_DbCleanup(string& db);
    
    // pub axuiliary functions.
    void x_FlattenPubEquiv(CPub_equiv& pe);

    // CName_std cleanup 
    void x_FixEtAl(CName_std& name);
    void x_FixInitials(CName_std& name);
    void x_ExtractSuffixFromInitials(CName_std& name);
    void x_FixSuffix(CName_std& name);

    void x_OrgModToSubtype(CBioSource& bs);
    void x_SubtypeCleanup(CBioSource& bs);
    void x_ModToOrgMod(COrg_ref& oref);
    // cleanup strings in User objects and fields
    void x_CleanupUserString(string& str);

    bool x_ParseCodeBreak(const CSeq_feat& feat, CCdregion& cds, const string& str);

    // Extended Cleanup
    typedef void (CCleanup_imp::*RecurseDescriptor)(CSeq_descr& sdr, CSeq_descr::Tdata& remove_list);
    void x_RecurseForDescriptors (const CSeq_entry& se, RecurseDescriptor pmf);
    void x_RecurseForDescriptors (CBioseq_Handle bs, RecurseDescriptor pmf);
    void x_RecurseForDescriptors (CBioseq_set_Handle bs, RecurseDescriptor pmf);
    
    typedef bool (CCleanup_imp::*IsMergeCandidate)(const CSeqdesc& sd);
    typedef bool (CCleanup_imp::*Merge)(CSeqdesc& sd1, CSeqdesc& sd2);
    void x_RecurseDescriptorsForMerge(CSeq_descr& sdr, IsMergeCandidate is_can, Merge do_merge, CSeq_descr::Tdata& remove_list);
    void x_RecurseDescriptorsForMerge (CBioseq_Handle bs, IsMergeCandidate is_can, Merge do_merge);
    void x_RecurseDescriptorsForMerge (CBioseq_set_Handle bs, IsMergeCandidate is_can, Merge do_merge);

    typedef void (CCleanup_imp::*RecurseSeqAnnot)(CSeq_annot_Handle sah);
    void x_RecurseForSeqAnnots (const CSeq_entry& se, RecurseSeqAnnot pmf);
    void x_RecurseForSeqAnnots (CBioseq_Handle bs, RecurseSeqAnnot pmf);
    void x_RecurseForSeqAnnots (CBioseq_set_Handle bs, RecurseSeqAnnot pmf);
    
    void x_RemoveEmptyGenbankDesc(CSeq_descr& sdr, CSeq_descr::Tdata& remove_list);

    void x_CleanGenbankBlockStrings (CGB_block& block);
    void x_CleanGenbankBlockStrings (CSeq_descr& sdr, CSeq_descr::Tdata& remove_list);
    
    void x_RemoveEmptyFeatures (CSeq_annot_Handle sa);
    
    void x_RemoveMultipleTitles (CSeq_descr& sdr, CSeq_descr::Tdata& remove_list);
    void x_RemoveEmptyTitles (CSeq_descr& sdr, CSeq_descr::Tdata& remove_list);

    void x_MergeMultipleDates (CSeq_descr& sdr, CSeq_descr::Tdata& remove_list);

    void x_ExtendedCleanStrings (CSeqdesc& sd);
    void x_ExtendedCleanStrings (COrg_ref& org);
    void x_CleanOrgNameStrings (COrgName& on);
    void x_ExtendedCleanSubSourceList (CBioSource& bs);
    
    void x_SetSourceLineage(const CSeq_entry& se, string lineage);
    void x_SetSourceLineage(CSeq_entry_Handle seh, string lineage);
    void x_SetSourceLineage(CBioseq_Handle bh, string lineage);
    void x_SetSourceLineage(CBioseq_set_Handle bh, string lineage);
    
    void x_MergeAdjacentAnnots (CBioseq_Handle bs);
    void x_MergeAdjacentAnnots (CBioseq_set_Handle bss);
    
    void x_ConvertFullLenFeatureToDescriptor (CSeq_annot_Handle sa, CSeqFeatData::E_Choice choice);
    void x_ConvertFullLenPubFeatureToDescriptor (CSeq_annot_Handle sah);

    bool x_CorrectExceptText (string& except_text);
    void x_CorrectExceptText( CSeq_feat& feat);
    void x_CorrectExceptText (CSeq_annot_Handle sa);

    void x_MoveDbxrefs( CSeq_feat& feat);
    void x_MoveDbxrefs (CSeq_annot_Handle sa);
    
    bool x_CheckCodingRegionEnds (CSeq_feat& feat);
    void x_CheckCodingRegionEnds (CSeq_annot_Handle sah);
    
    void x_ExtendSingleGeneOnmRNA (CBioseq_set_Handle bssh);
    void x_ExtendSingleGeneOnmRNA (CBioseq_Handle bsh);
    
    void x_MoveFeaturesOnPartsSets (CSeq_annot_Handle sa);
    void x_RemovePseudoProducts (CSeq_annot_Handle sa);
    void x_RemoveGeneXref(CRef<CSeq_feat> feat);
    void x_RemoveUnnecessaryGeneXrefs(CSeq_annot_Handle sa);

    bool x_ChangeNoteQualToComment(CSeq_feat& feat);
    CCdregion::EFrame x_FrameFromSeqLoc(const CSeq_loc& loc);
    bool x_ImpFeatToCdRegion (CSeq_feat& feat);
    void x_ChangeImpFeatToCDS(CSeq_annot_Handle sa);
    void x_ChangeImpFeatToProt(CSeq_annot_Handle sa);
    
    void x_MoveGeneQuals(const CSeq_feat& orig_feat);
    void x_MoveGeneQuals (CSeq_annot_Handle sa);
    void x_MoveGeneQuals (CSeq_entry_Handle seh);
    void x_MoveGeneQuals (CBioseq_Handle bs);
    void x_MoveGeneQuals (CBioseq_set_Handle bs);
    void x_RemoveMarkedGeneXref(const CSeq_feat& orig_feat);
    void x_RemoveMarkedGeneXrefs (CSeq_annot_Handle sa);
    void x_RemoveMarkedGeneXrefs (CSeq_entry_Handle seh);
    void x_RemoveMarkedGeneXrefs (CBioseq_Handle bs);
    void x_RemoveMarkedGeneXrefs (CBioseq_set_Handle bs);
    
    void x_MoveCodingRegionsToNucProtSets (CSeq_entry_Handle seh, CSeq_annot_EditHandle parent_sah);

    void x_RemoveFeaturesBySubtype (const CSeq_entry& se, CSeqFeatData::ESubtype subtype);
    void x_RemoveFeaturesBySubtype (CBioseq_Handle bs, CSeqFeatData::ESubtype subtype);
    void x_RemoveFeaturesBySubtype (CBioseq_set_Handle bs, CSeqFeatData::ESubtype subtype);
    void x_RemoveImpSourceFeatures (CSeq_annot_Handle sa);    
    void x_RemoveSiteRefImpFeats(CSeq_annot_Handle sa);
    void x_StripProtXrefs(CSeq_annot_Handle sa);
    void x_ConvertUserObjectToAnticodon(CSeq_annot_Handle sa);
    void x_MoveMapQualsToGeneMaploc (CSeq_annot_Handle sa);

    void RemoveEmptyFeaturesDescriptorsAndAnnots (CBioseq_Handle bs);
    void RemoveEmptyFeaturesDescriptorsAndAnnots (CBioseq_set_Handle bs);
    
    
    void x_RemoveEmptyFeatureAnnots (CBioseq_Handle bs);
    void x_RemoveEmptyFeatureAnnots (CBioseq_set_Handle bs);
    
    void x_ExtendedCleanStrings (CSeq_descr& sdr);
    void x_ExtendedCleanStrings (CSeq_annot_Handle sah);
    void x_ExtendedCleanStrings (CSeq_feat& feat);
    void x_ExtendedCleanStrings (CGene_ref& gene_ref);
    void x_ExtendedCleanStrings (CProt_ref& prot_ref);
    void x_ExtendedCleanStrings (CRNA_ref& rna_ref);
    void x_ExtendedCleanStrings (CPubdesc& pd);
    void x_ExtendedCleanStrings (CImp_feat& imf);

    bool x_IsCitSubPub(const CSeqdesc& sd);
    bool x_CitSubsMatch(CSeqdesc& sd1, CSeqdesc& sd2);

    void x_RemoveCitGenPubDescriptors (CSeq_descr& sdr, CSeq_descr::Tdata& remove_list);
    void x_RemoveCitGenPubFeatures (CSeq_annot_Handle sa);
    
    CRef<CPub> x_MinimizePub (const CPub& pub);
    void x_ChangeCitationQualToCitationPub(CBioseq_Handle bs);
    bool x_ChangeCitSub (CPub& pub);
    void x_ChangeCitSub (CBioseq_Handle bh);
    void x_ChangeCitSub (CBioseq_set_Handle bh);
    void x_ChangeCitSub (CSeq_entry_Handle seh);
    void x_RemovePubMatch(const CSeq_entry& se, const CPubdesc& pd);
    void x_MergeAndMovePubs(CBioseq_set_Handle bsh);    
    void x_RemoveDuplicatePubsFromBioseqsInSet(CBioseq_set_Handle bsh);
    void x_MergeDuplicatePubsOnSet(CBioseq_set_Handle bsh);    
    void x_ConvertPubsToAsn4 (CSeq_entry_Handle seh);

    // BioSource Cleanup functions
    bool x_Merge (COrgName& on1, const COrgName& on2);
    bool x_Merge (COrg_ref::TDb& db1, const COrg_ref::TDb& db2);
    bool x_Merge (COrg_ref& org, const COrg_ref& add_org);
    bool x_Merge (CBioSource& src, const CBioSource& add_src);
    bool x_IsMergeableBioSource(const CSeqdesc& sd);  
    bool x_OkToMerge (const COrgName& on1, const COrgName& on2);
    bool x_OkToMerge (const COrg_ref::TDb& db1, const COrg_ref::TDb& db2);  
    bool x_OkToMerge (const COrg_ref& org1, const COrg_ref& org2);
    bool x_OkToMerge (const CBioSource& src1, const CBioSource& src2);
    bool x_OkToConvertToDescriptor (CBioseq_Handle bh, const CBioSource& src);    
    bool x_MergeDuplicateBioSources(CSeqdesc& sd1, CSeqdesc& sd2);
    
    bool x_IdenticalModifierLists (const list< CRef< CSubSource > >& mod_list1,
                                   const list< CRef< CSubSource > >& mod_list2);
    bool x_IdenticalModifierLists (const list< CRef< COrgMod > >& mod_list1,
                                   const list< CRef< COrgMod > >& mod_list2);
    bool x_Identical (const COrg_ref::TDb& db1, const COrg_ref::TDb& db2);
    bool x_Identical(const COrg_ref& org1, const COrg_ref& org2);
    bool x_Identical(const CBioSource& src1, const CBioSource& src2);
    bool x_IsBioSourceEmpty (const CBioSource& src);
    void x_Common (list< CRef< CSubSource > >& mod_list1,
                   const list< CRef< CSubSource > >& mod_list2,
                   bool flag_changes);
    void x_Common (list< CRef< COrgMod > >& mod_list1,
                   const list< CRef< COrgMod > >& mod_list2,
                   bool flag_changes);
    void x_Common (COrgName& db1, const COrgName& db2, bool flag_changes);                   
    void x_Common (COrg_ref::TDb& db1, const COrg_ref::TDb& db2, bool flag_changes);                   
    void x_Common(COrg_ref& host, const COrg_ref& guest, bool flag_changes);                                
    void x_Common(CBioSource& host, const CBioSource& guest, bool flag_changes);                                
    
    void x_MoveDescriptor (CBioseq_set_Handle bss, CBioseq_Handle bh, CSeqdesc::E_Choice choice);
    void x_MoveDescriptor (CBioseq_set_Handle dst, CBioseq_set_Handle src, CSeqdesc::E_Choice choice);
    bool x_PromoteMergeableSource (CBioseq_set_Handle dst, const CBioSource& biosrc);
    
    void x_FixSegSetSource (CBioseq_set_Handle segset, CBioseq_set_Handle parts, CBioseq_set_Handle *nuc_prot_parent = NULL);
    void x_FixSegSetSource (CBioseq_set_Handle bh, CBioseq_set_Handle *nuc_prot_parent = NULL);
    void x_FixNucProtSources (CBioseq_set_Handle bh);
    void x_FixSetSource (CBioseq_set_Handle bh);
    
    bool x_ConvertOrgDescToSourceDescriptor(CBioseq_set_Handle bh);
    bool x_ConvertOrgDescToSourceDescriptor(CBioseq_Handle bh);
    void x_ConvertQualifiersToOrgMods(CSeq_feat& sf);
    void x_ConvertQualifiersToSubSources (CSeq_feat& sf);
    void x_ConvertMiscQualifiersToBioSource (CSeq_feat& sf);
    bool x_ConvertOrgAndImpFeatToSource(CSeq_annot_Handle sa);
    bool x_ConvertOrgAndImpFeatToSource(CBioseq_set_Handle bh);
    bool x_ConvertOrgAndImpFeatToSource(CBioseq_Handle bh);

    void x_ExtendedCleanupBioSourceFeatures(CBioseq_Handle bh);
    void x_ExtendedCleanupBioSourceDescriptorsAndFeatures(CBioseq_set_Handle bss);
    void x_ExtendedCleanupBioSourceDescriptorsAndFeatures(CBioseq_Handle bh);

    bool x_RemovePIDXrefs (CSeq_feat& feat);
    bool x_FixPIDDbtag (CSeq_id& id);
    bool x_FixPIDDbtag (CSeq_loc& loc);
    bool x_FixPIDDbtag (CSeq_feat& feat);
    void x_FixProteinIDs (CSeq_annot_Handle sa);
    bool x_CheckConflictFlag (CSeq_feat& feat);
    void x_CheckConflictFlag (CSeq_annot_Handle sa);
    bool x_RemoveAsn2ffGeneratedComments (CSeq_feat& feat);
    void x_RemoveAsn2ffGeneratedComments (CSeq_annot_Handle sa);

    void x_CheckGenbankBlockTechKeywords(CGB_block& gb_block, CMolInfo::TTech tech);
    void x_ChangeGBDiv (CSeq_entry_Handle seh, string div);
    void x_ChangeGenBankBlocks(CSeq_entry_Handle seh);

    bool x_IsDescrSameForAllInPartsSet (CBioseq_set_Handle bss, CSeqdesc::E_Choice desctype, CSeq_descr::Tdata& desc_list);
    void x_RemoveDescrByType(CBioseq_Handle bh, CSeqdesc::E_Choice desctype);
    void x_RemoveDescrByType(CBioseq_set_Handle bh, CSeqdesc::E_Choice desctype, bool recurse = true);
    void x_RemoveDescrByType(const CSeq_entry& se, CSeqdesc::E_Choice desctype, bool recurse = true);
    void x_RemoveDescrForAllInSet (CBioseq_set_Handle bss, CSeqdesc::E_Choice desctype);
    void x_MoveIdenticalPartDescriptorsToSegSet (CBioseq_set_Handle segset, CBioseq_set_Handle parts, CSeqdesc::E_Choice desctype);
    bool x_SeqDescMatch (const CSeqdesc& d1, const CSeqdesc& d2);
    
    void x_RemoveNucProtSetTitle(CBioseq_set_EditHandle bsh, const CSeq_entry& se);
    void x_ExtractNucProtDescriptors(CBioseq_set_EditHandle bsh, const CSeq_entry& se, CSeqdesc::E_Choice desctype);

    void x_FixMissingSources (CBioseq_Handle bh);
    void x_FixMissingSources (CBioseq_set_Handle bh);
    void x_FixMissingSources (const CSeq_entry& se);

    void LoopToAsn3 (CSeq_entry_Handle seh);
    void LoopToAsn3 (CBioseq_set_Handle bh);
    void LoopToAsn3(CBioseq_Handle bh);
    void CheckSegSet (CBioseq_Handle bs);
    void CheckSegSet(CBioseq_set_Handle bss);
    void CheckNucProtSet (CBioseq_set_Handle bss);
    void x_StripOldDescriptorsAndFeatures (CBioseq_set_Handle bh, bool recurse = true);
    void x_StripOldDescriptorsAndFeatures (CBioseq_Handle bh);
    void x_FuseMolInfos (CBioseq::TDescr& desc_set, CSeq_descr::Tdata& desc_list);
    void x_FuseMolInfos (CBioseq_Handle bh);    
    void x_FuseMolInfos (CBioseq_set_Handle bh);
    void x_FuseMolInfos (CSeq_entry_Handle seh);
    void x_AddMissingProteinMolInfo(CSeq_entry_Handle seh);
    void x_GetGenBankTaxonomy(const CSeq_entry& se, string &taxonomy);
    void x_GetGenBankTaxonomy(CSeq_entry_Handle seh, string& taxonomy);
    void x_GetGenBankTaxonomy(CBioseq_Handle bs, string &taxonomy);
    void x_GetGenBankTaxonomy(CBioseq_set_Handle bss, string &taxonomy);

    void x_NormalizeMolInfo(CBioseq_set_Handle bh);
    void x_SetMolInfoTechForConflictCDS (CBioseq_Handle bh);

    // Prohibit copy constructor & assignment operator
    CCleanup_imp(const CCleanup_imp&);
    CCleanup_imp& operator= (const CCleanup_imp&);
    
    CRef<CCleanupChange>    m_Changes;
    Uint4                   m_Options;
    ECleanupMode            m_Mode;
    CRef<CScope>            m_Scope;
    
};


END_SCOPE(objects)
END_NCBI_SCOPE


/*
 * ===========================================================================
 *
 * $Log$
 * Revision 1.52  2006/12/28 19:51:50  bollin
 * Removed steps for handling obsolete descriptors mol-type, method, and modif.
 *
 * Revision 1.51  2006/12/28 13:56:03  bollin
 * Avoid creating empty Seqdescr sets.
 *
 * Revision 1.50  2006/12/27 19:27:11  bollin
 * Fixed bug in mechanism for extended cleanup of sets inside other sets.
 * Added step for cleaning up BioSource descriptors on WGS, pop, phy, mut, and
 * eco sets.
 *
 * Revision 1.49  2006/12/11 17:14:43  bollin
 * Made changes to ExtendedCleanup per the meetings and new document describing
 * the expected behavior for BioSource features and descriptors.  The behavior
 * for BioSource descriptors on GenBank, WGS, Mut, Pop, Phy, and Eco sets has
 * not been implemented yet because it has not yet been agreed upon.
 *
 * Revision 1.48  2006/10/24 12:15:22  bollin
 * Added more steps to LoopToAsn3, including steps for creating and combining
 * MolInfo descriptors and BioSource descriptors.
 *
 * Revision 1.47  2006/10/12 17:29:39  bollin
 * Corrected bugs that were falsely reporting changes made by ExtendedCleanup.
 *
 * Revision 1.46  2006/10/10 13:48:55  bollin
 * added steps to ExtendedCleanup to convert user object to anticodon and to
 * convert maps to genrefs
 *
 * Revision 1.45  2006/10/05 18:36:52  bollin
 * Added step to ExtendedCleanup to fuse MolInfo descriptors on the same Bioseq
 * or Bioseq-set.
 * Added step to ExtendedCleanup to convert protein xrefs on coding regions to
 * protein features on the product sequence for the coding regions.
 *
 * Revision 1.44  2006/10/05 15:15:03  ucko
 * Drop extraneous class name from x_MinimizePub's declaration.
 *
 * Revision 1.43  2006/10/04 14:17:47  bollin
 * Added step to ExtendedCleanup to move coding regions on nucleotide sequences
 * in nuc-prot sets to the nuc-prot set (was move_cds_ex in C Toolkit).
 * Replaced deprecated CSeq_feat_Handle.Replace calls with CSeq_feat_EditHandle.Replace calls.
 * Began implementing C++ version of LoopSeqEntryToAsn3.
 *
 * Revision 1.42  2006/09/27 15:42:03  bollin
 * Added step to ExtendedCleanup for moving gene quals to gene xrefs and removing
 * the gene xrefs from this step at the end of the process on features for which
 * there is an overlapping gene.
 *
 * Revision 1.41  2006/08/29 14:28:12  rsmith
 * + BasicCleanup(CEMBL_block)
 *
 * Revision 1.40  2006/08/03 18:13:46  rsmith
 * BasicCleanup(CSeq_loc)
 *
 * Revision 1.39  2006/08/03 12:05:31  bollin
 * added method to ExtendedCleanup for converting imp_feat coding regions to
 * real coding regions and for converting imp_feat protein features annotated
 * on the nucleotide sequence to real protein features annotated on the protein
 * sequence
 *
 * Revision 1.38  2006/07/26 19:37:04  rsmith
 * add cleanup w/Handles
 *
 * Revision 1.37  2006/07/26 17:12:41  bollin
 * added method to remove redundant genbank block information
 *
 * Revision 1.36  2006/07/25 20:07:13  bollin
 * added step to ExtendedCleanup to remove unnecessary gene xrefs
 *
 * Revision 1.35  2006/07/25 16:51:23  bollin
 * fixed bug in x_RemovePseudoProducts
 * implemented more efficient method for removing descriptors
 *
 * Revision 1.34  2006/07/25 14:36:47  bollin
 * added method to ExtendedCleanup to remove products on coding regions marked
 * as pseudo.
 *
 * Revision 1.33  2006/07/18 16:43:43  bollin
 * added x_RecurseDescriptorsForMerge and changed the ExtendedCleanup functions
 * for merging duplicate BioSources and equivalent CitSubs to use the new function
 *
 * Revision 1.32  2006/07/13 17:10:57  rsmith
 * report if changes made
 *
 * Revision 1.31  2006/07/11 14:38:28  bollin
 * aadded a step to ExtendedCleanup to extend the only gene found on the only
 * mRNA sequence in the set where there are zero or one coding region features
 * in the set so that the gene covers the entire mRNA sequence
 *
 * Revision 1.30  2006/07/10 19:01:57  bollin
 * added step to extend coding region to cover missing portion of a stop codon,
 * will also adjust the location of the overlapping gene if necessary.
 *
 * Revision 1.29  2006/07/05 17:26:11  bollin
 * cleared compiler error
 *
 * Revision 1.28  2006/07/05 16:43:34  bollin
 * added step to ExtendedCleanup to clean features and descriptors
 * and remove empty feature table seq-annots
 *
 * Revision 1.27  2006/07/03 18:45:00  bollin
 * changed methods in ExtendedCleanup for correcting exception text and moving
 * dbxrefs to use edit handles
 *
 * Revision 1.26  2006/07/03 17:48:04  bollin
 * changed RemoveEmptyFeatures method to use edit handles
 *
 * Revision 1.25  2006/07/03 17:02:32  bollin
 * converted steps for ExtendedCleanup that convert full length pub and source
 * features to descriptors to use edit handles
 *
 * Revision 1.24  2006/07/03 15:27:38  bollin
 * rewrote x_MergeAddjacentAnnots to use edit handles
 *
 * Revision 1.23  2006/07/03 14:51:11  bollin
 * corrected compiler errors
 *
 * Revision 1.22  2006/07/03 12:33:48  bollin
 * use edit handles instead of operating directly on the data
 * added step to renormalize nuc-prot sets to ExtendedCleanup
 *
 * Revision 1.21  2006/06/28 15:23:03  bollin
 * added step to move db_xref GenBank Qualifiers to real dbxrefs to Extended Cleanup
 *
 * Revision 1.20  2006/06/28 13:22:39  bollin
 * added step to merge duplicate biosources to ExtendedCleanup
 *
 * Revision 1.19  2006/06/27 18:43:02  bollin
 * added step for merging equivalent cit-sub publications to ExtendedCleanup
 *
 * Revision 1.18  2006/06/27 14:30:59  bollin
 * added step for correcting exception text to ExtendedCleanup
 *
 * Revision 1.17  2006/06/27 13:18:47  bollin
 * added steps for converting full length pubs and sources to descriptors to
 * Extended Cleanup
 *
 * Revision 1.16  2006/06/22 18:16:01  bollin
 * added step to merge adjacent Seq-Annots to ExtendedCleanup
 *
 * Revision 1.15  2006/06/22 17:29:15  bollin
 * added method to merge multiple create dates to Extended Cleanup
 *
 * Revision 1.14  2006/06/22 15:39:00  bollin
 * added step for removing multiple titles to Extended Cleanup
 *
 * Revision 1.13  2006/06/22 13:28:29  bollin
 * added step to remove empty features to ExtendedCleanup
 *
 * Revision 1.12  2006/06/21 17:21:28  bollin
 * added cleanup of GenbankBlock descriptor strings to ExtendedCleanup
 *
 * Revision 1.11  2006/06/21 14:12:59  bollin
 * added step for removing empty GenBank block descriptors to ExtendedCleanup
 *
 * Revision 1.10  2006/06/20 19:43:39  bollin
 * added MolInfoUpdate to ExtendedCleanup
 *
 * Revision 1.9  2006/05/17 17:39:36  bollin
 * added parsing and cleanup of anticodon qualifiers on tRNA features and
 * transl_except qualifiers on coding region features
 *
 * Revision 1.8  2006/04/18 14:32:36  rsmith
 * refactoring
 *
 * Revision 1.7  2006/04/17 17:03:12  rsmith
 * Refactoring. Get rid of cleanup-mode parameters.
 *
 * Revision 1.6  2006/04/05 14:01:22  dicuccio
 * Don't export internal classes
 *
 * Revision 1.5  2006/03/29 19:42:16  rsmith
 * Delete   x_CleanupExtName(CRNA_ref& rna_ref);
 * and x_CleanupExtTRNA(CRNA_ref& rna_ref);
 *
 * Revision 1.4  2006/03/29 16:38:14  rsmith
 * various implementation declaration changes.
 *
 * Revision 1.3  2006/03/23 18:31:40  rsmith
 * new BasicCleanup routines, particularly User Objects/Fields.
 *
 * Revision 1.2  2006/03/20 14:22:15  rsmith
 * add cleanup for CSeq_submit
 *
 * Revision 1.1  2006/03/14 20:21:50  rsmith
 * Move BasicCleanup functionality from objects to objtools/cleanup
 *
 *
 * ===========================================================================
 */

#endif  /* CLEANUP___CLEANUP__HPP */
