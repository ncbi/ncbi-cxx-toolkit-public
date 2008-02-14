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
class CMedline_entry;
class CPubMedId;
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
    void ExtendedCleanup(const CSeq_submit& ss);
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
    void BasicCleanup(CMedline_entry& ml, bool fix_initials);
    void BasicCleanup(CPubMedId& pm, bool fix_initials);
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
    void x_ChangeTransposonToMobileElement(CGb_qual& gbq);
    void x_ChangeInsertionSeqToMobileElement(CGb_qual& gbq);

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
    void x_ActOnDescriptors (CBioseq_set_Handle bss, RecurseDescriptor pmf);
    
    typedef bool (CCleanup_imp::*IsMergeCandidate)(const CSeqdesc& sd);
    typedef bool (CCleanup_imp::*Merge)(CSeqdesc& sd1, CSeqdesc& sd2);
    void x_RecurseDescriptorsForMerge(CSeq_descr& sdr, IsMergeCandidate is_can, Merge do_merge, CSeq_descr::Tdata& remove_list);
    void x_RecurseDescriptorsForMerge (CBioseq_Handle bs, IsMergeCandidate is_can, Merge do_merge);
    void x_RecurseDescriptorsForMerge (CBioseq_set_Handle bs, IsMergeCandidate is_can, Merge do_merge);
    void x_ActOnDescriptorsForMerge (CBioseq_set_Handle bh, IsMergeCandidate is_can, Merge do_merge);    

    typedef void (CCleanup_imp::*RecurseSeqAnnot)(CSeq_annot_Handle sah);
    void x_RecurseForSeqAnnots (const CSeq_entry& se, RecurseSeqAnnot pmf);
    void x_RecurseForSeqAnnots (CBioseq_Handle bs, RecurseSeqAnnot pmf);
    void x_RecurseForSeqAnnots (CBioseq_set_Handle bs, RecurseSeqAnnot pmf);
    void x_ActOnSeqAnnots (CBioseq_set_Handle bss, RecurseSeqAnnot pmf);
    
    void x_RemoveEmptyGenbankDesc(CSeq_descr& sdr, CSeq_descr::Tdata& remove_list);

    void x_CleanGenbankBlockStrings (CGB_block& block);
    void x_CleanGenbankBlockStrings (CSeq_descr& sdr, CSeq_descr::Tdata& remove_list);
    
    void x_RemoveEmptyFeatures (CSeq_annot_Handle sa);
    
    void x_RemoveMultipleTitles (CSeq_descr& sdr, CSeq_descr::Tdata& remove_list);
    void x_RemoveEmptyTitles (CSeq_descr& sdr, CSeq_descr::Tdata& remove_list);

    void x_MergeMultipleDates (CSeq_descr& sdr, CSeq_descr::Tdata& remove_list);

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

    void x_MoveDbxrefs( CSeq_feat& feat);
    void x_MoveDbxrefs (CSeq_annot_Handle sa);
    
    bool x_CheckCodingRegionEnds (CSeq_feat_Handle ofh);
    void x_CheckCodingRegionEnds (CSeq_annot_Handle sah);
    
    void x_ExtendSingleGeneOnmRNA (CBioseq_set_Handle bssh);
    bool x_ExtendSingleGeneOnmRNA (CBioseq_Handle bsh, bool is_master_seq);
    
    void x_MoveFeaturesOnPartsToCorrectSeqAnnots(CBioseq_set_Handle bsh);

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
    
    bool x_IsCitSubPub(const CSeqdesc& sd);
    bool x_CitSubsMatch(CSeqdesc& sd1, CSeqdesc& sd2);

    CRef<CPub> x_MinimizePub (const CPub& pub);
    void x_ChangeCitationQualToCitationPub(CBioseq_Handle bs);
    bool x_ChangeCitSub (CPub& pub);
    void x_ChangeCitSub (CBioseq_Handle bh);
    void x_ChangeCitSub (CBioseq_set_Handle bh);
    void x_ChangeCitSub (CSeq_entry_Handle seh);
    void x_RemovePubMatch(const CSeq_entry& se, const CPubdesc& pd);
    void x_MergeAndMovePubs(CBioseq_set_Handle bsh);    
    void x_RemoveDuplicatePubsFromBioseqsInSet(CBioseq_set_Handle bsh);
    void x_MergeDuplicatePubs(CBioseq_set_Handle bsh);    
    void x_MergeDuplicatePubs(CBioseq_Handle bsh);    
    
    bool x_RemoveEmptyPubs (CPubdesc& pubdesc);
    void x_RemoveEmptyPubs (CSeq_annot_Handle sa);
    void x_RemoveEmptyPubs(CSeq_descr& sdr, CSeq_descr::Tdata& remove_list);
    void x_ExtendedCleanupPubs (CBioseq_set_Handle bss);
    void x_ExtendedCleanupPubs (CBioseq_Handle bss);
    void x_ExtendedCleanupPubs (CSeq_annot_Handle sa);


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
    
    void x_ConvertOrgDescToSourceDescriptor(CBioseq_set_Handle bh);
    void x_ConvertOrgDescToSourceDescriptor(CBioseq_Handle bh);
    void x_ConvertOrgFeatToSource(CSeq_annot_Handle sa);
    void x_ConvertOrgFeatToSource(CBioseq_set_Handle bh);
    void x_ConvertOrgFeatToSource(CBioseq_Handle bh);

    void x_ExtendedCleanupBioSourceFeatures(CBioseq_Handle bh);
    void x_ExtendedCleanupBioSourceDescriptorsAndFeatures(CBioseq_set_Handle bss);
    void x_ExtendedCleanupBioSourceDescriptorsAndFeatures(CBioseq_Handle bh);

    // MolInfo Cleanup Functions
    void x_FuseMolInfos (CBioseq::TDescr& desc_set, CSeq_descr::Tdata& desc_list);
    void x_FuseMolInfos (CBioseq_Handle bh);    
    void x_FuseMolInfos (CBioseq_set_Handle bh);
    void x_FuseMolInfos (CSeq_entry_Handle seh);
    void x_AddMissingProteinMolInfo(CSeq_entry_Handle seh);

    void x_NormalizeMolInfo(CBioseq_set_Handle bh);
    void x_SetMolInfoTechForConflictCDS (CBioseq_Handle bh);
    void x_ExtendedCleanupMolInfoDescriptors(CBioseq_Handle bh);
    void x_ExtendedCleanupMolInfoDescriptors(CBioseq_set_Handle bh);

    // Other Cleanup Functions    
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

    void LoopToAsn3 (CSeq_entry_Handle seh);
    void LoopToAsn3 (CBioseq_set_Handle bh);
    void LoopToAsn3(CBioseq_Handle bh);
    void CheckNucProtSet (CBioseq_set_Handle bss);

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

#endif  /* CLEANUP___CLEANUP__HPP */
