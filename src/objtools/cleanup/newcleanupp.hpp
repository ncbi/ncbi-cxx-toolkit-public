#ifndef NEWCLEANUP__HPP
#define NEWCLEANUP__HPP

/*
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
 * Author: Robert Smith, Jonathan Kans, Michael Kornbluh
 *
 * File Description:
 *   Basic and Extended Cleanup of CSeq_entries.
 *
 * ===========================================================================
 */

#include <stack>

#include <objects/seqfeat/OrgMod.hpp>
#include <objects/seqfeat/SeqFeatData.hpp>
#include <objects/seqfeat/Seq_feat.hpp>
#include <objects/seq/MolInfo.hpp>
#include <objects/seqblock/GB_block.hpp>

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
class CSeq_loc_mix;
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
class CMolInfo;
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
class CId_pat;
class CCit_proc;
class CCit_jour;
class CPubMedId;
class CAuth_list;
class CAuthor;
class CAffil;
class CPerson_id;
class CName_std;
class CBioSource;
class COrg_ref;
class COrgName;
class CSubSource;
class CMolInfo;
class CCdregion;
class CDate;
class CDate_std;
class CImprint;
class CSubmit_block;
class CSeq_align;
class CDense_diag;
class CDense_seg;
class CStd_seg;
class CMedline_entry;
class CPub_set;
class CTrna_ext;
class CPCRPrimerSet;
class CPCRReactionSet;

class CSeq_entry_Handle;
class CBioseq_Handle;
class CBioseq_set_Handle;
class CSeq_annot_Handle;
class CSeq_feat_Handle;

class CObjectManager;
class CScope;

class CGBQualCleanup;

class NCBI_CLEANUP_EXPORT CNewCleanup_imp
{
public:
    static const int NCBI_CLEANUP_VERSION = 1;

    // Constructor
    CNewCleanup_imp(CRef<CCleanupChange> changes, CScope& scope, Uint4 options = 0);

    // Destructor
    virtual ~CNewCleanup_imp();

    /// Main methods


    void BasicCleanupSeqEntry(
        CSeq_entry& se);

    void BasicCleanupSeqSubmit(
        CSeq_submit& ss);

    void BasicCleanupSubmitblock(CSubmit_block& sb);

    void BasicCleanupSeqAnnot(
        CSeq_annot& sa);

    void BasicCleanupBioseqSet(
        CBioseq_set& bss);

    void BasicCleanupSeqFeat(
        CSeq_feat& sf);

    void BasicCleanupBioSource(
        CBioSource& src);

    void BasicCleanupSeqEntryHandle(
        CSeq_entry_Handle& seh);

    void BasicCleanupBioseqHandle(
        const CBioseq_Handle& bsh);

    void BasicCleanupBioseqSetHandle(
        CBioseq_set_Handle& bssh);

    void BasicCleanupSeqAnnotHandle(
        CSeq_annot_Handle& sah);

    void BasicCleanupSeqFeatHandle(
        CSeq_feat_Handle& sfh);

    void BasicCleanup(CPubdesc& pd, bool strip_serial);

    void BasicCleanup(CSeqdesc& desc);

    /// Extended Cleanup methods

    void ExtendedCleanupSeqEntry(
        CSeq_entry& se);

    void ExtendedCleanupSeqSubmit(
        CSeq_submit& ss);

    void ExtendedCleanupSeqAnnot(
        CSeq_annot& sa);

    void ExtendedCleanupSeqEntryHandle(
        CSeq_entry_Handle& seh);

    void ExtendedCleanup(CSeq_entry_Handle& seh) { ExtendedCleanupSeqEntryHandle(seh); }

    void ExtendedCleanup(CBioSource& biosrc);

    static bool ShouldRemoveAnnot(const CSeq_annot& annot);

    static void AddNcbiCleanupObject(CSeq_descr& descr);


    // many more methods and variables ...
    // Note that these are not private. This is to facilitate testing
    void ChangeMade(CCleanupChange::EChanges e);

    void EnteringEntry(CSeq_entry& se);

    void SetGeneticCode(CBioseq& bs);

    void SetGeneticCode(const CBioseq_Handle& bsh);

    void SubmitblockBC(CSubmit_block& sb);

    void SetClass(const CBioseq_set_Handle& bssh);
    void ProtSeqBC(const CBioseq_Handle& bsh);

    bool BasicCleanupSeqIds(const CBioseq_Handle& bsh);
    bool SeqIdBC(CSeq_id& seq_id);


    void GBblockOriginBC(string& str);
    void GBblockBC(CGB_block& gbk);
    void EMBLblockBC(CEMBL_block& emb);

    void BiosourceFeatBC(CBioSource& biosrc, CSeq_feat& seqfeat);
    void BiosourceBC(CBioSource& bsc);
    void OrgrefModBC(string& str);
    void OrgrefBC(COrg_ref& org);
    void x_MovedNamedValuesInStrain(COrgName& orgname);
    void x_MovedNamedValuesInStrain(COrgName& orgname, COrgMod::ESubtype stype, const string& prefix);
    void OrgnameBC(COrgName& onm, COrg_ref& org_ref);
    void OrgmodBC(COrgMod& omd);

    void DbtagBC(CDbtag& dbt);

    void PubdescBC(CPubdesc& pub);
    void PubSetBC(CPub_set& pub_set);

    void ImpFeatBC(CSeq_feat& sf);

    void SiteFeatBC(const CSeqFeatData::ESite& site, CSeq_feat& sf);

    void SeqLocBC(CSeq_loc& loc);
    void ConvertSeqLocWholeToInt(CSeq_loc& loc);

    void SeqfeatBC(CSeq_feat& sf);

    void GBQualBC(CGb_qual& gbq);
    void Except_textBC(string& except_text);

    void GenerefBC(CGene_ref& gr);
    void ProtNameBC(std::string& str);
    void ProtActivityBC(std::string& str);
    void ProtrefBC(CProt_ref& pr);
    void RnarefBC(CRNA_ref& rr);
    void RnarefGenBC(CRNA_ref& rr);

    void GeneFeatBC(CGene_ref& gr, CSeq_feat& sf);
    void ProtFeatfBC(CProt_ref& pr, CSeq_feat& sf);
    void PostProtFeatfBC(CProt_ref& pr);
    void RnaFeatBC(CRNA_ref& rr, CSeq_feat& sf);
    void CdregionFeatBC(CCdregion& cds, CSeq_feat& seqfeat);

    static bool x_IsCommentRedundantWithEC(const CSeq_feat& seqfeat, CScope& scope);

    void RemoveZeroLengthLiterals(CDelta_ext& delta_ext);

    void UserObjectBC(CUser_object& user_object);

    void PCRReactionSetBC(CPCRReactionSet& pcr_reaction_set);
    void SubSourceListBC(CBioSource& biosrc);

    bool MolInfoBC(CMolInfo& molinfo);
    void CreateMissingMolInfo(const CBioseq_Handle& bsh);

    static bool IsInternalTranscribedSpacer(const string& name);
    static bool TranslateITSName(string& in_out_name);


    // Extended Cleanup functions
    void        BioSourceEC(CBioSource& biosrc);
    void        AddProteinTitles(CBioseq& seq);
    void        ProtRefEC(CProt_ref& pr);
    void        CdRegionEC(CSeq_feat& sf);
    bool        x_FixParentPartials(const CSeq_feat& sf, CSeq_feat& parent);
    void        ExtendProteinFeatureOnProteinSeq(const CBioseq_Handle& bsh);
    void        ExtendSingleGeneOnMrna(const CBioseq_Handle& bsh);
    static bool IsSyntheticConstruct(const CBioSource& src);

    void MoveDbxrefs(CSeq_feat& sf);
    void MoveStandardName(CSeq_feat& sf);
    void ResynchProteinPartials(CSeq_feat& feat);
    void ResynchPeptidePartials(CBioseq& seq);
    void x_SetPartialsForProtein(CBioseq& prot, bool partial5, bool partial3, bool feat_partial);
    void RemoveBadProteinTitle(CBioseq& seq);
    void MoveCitationQuals(const CBioseq_Handle& bsh);
    void x_RemoveUnseenTitles(CSeq_descr& seq_descr);
    void KeepLatestDateDesc(CSeq_descr& seq_descr);
    void x_SingleSeqSetToSeq(CBioseq_set& set);
    void x_MergeDupBioSources(CSeq_descr& seq_descr);

    // void XxxxxxBC (Cxxxxx& xxx);

    // Prohibit copy constructor & assignment operator
    CNewCleanup_imp(const CNewCleanup_imp&);
    CNewCleanup_imp& operator=(const CNewCleanup_imp&);

private:
    void x_ExtendFeatureToCoverSequence(CSeq_feat_Handle fh, TSeqPos seq_length);

    // data structures used for post-processing

    // recorded by x_NotePubdescOrAnnotPubs
    typedef std::map<TEntrezId, TEntrezId>    TMuidToPmidMap;
    TMuidToPmidMap                            m_MuidToPmidMap;
    // recorded by x_RememberMuidThatMightBeConvertibleToPmid
    typedef std::vector<CRef<CPub>>           TMuidPubContainer;
    TMuidPubContainer                         m_MuidPubContainer;
    // m_OldLabelToPubMap and m_PubToNewPubLabelMap work together.
    // They supply "old_label -> node" and "node -> new_label", respectively,
    // so together we can get a mapping of "old_label -> new_label".
    // m_OldLabelToPubMap is a multimap because a node's address may change as we do our cleaning, and
    // at least one should remain so we can make the "old_label -> new_label" connection.
    typedef std::multimap<string, CRef<CPub>> TOldLabelToPubMap;
    TOldLabelToPubMap                         m_OldLabelToPubMap;
    map<string, string>                       m_OldLabelToNewLabel;
    // remember label changes
    typedef std::map<CRef<CPub>, string>      TPubToNewPubLabelMap;
    TPubToNewPubLabelMap                      m_PubToNewPubLabelMap;
    // remember all Seq-feat CPubs so we remember to change them later
    typedef std::vector<CRef<CPub>>           TSeqFeatCitPubContainer;
    TSeqFeatCitPubContainer                   m_SeqFeatCitPubContainer;
    // note all Pubdesc/annot cit-gen labels
    typedef std::vector<string>               TPubdescCitGenLabelVec;
    TPubdescCitGenLabelVec                    m_PubdescCitGenLabelVec;

    void x_ConvertGoQualifiers(CSeq_feat& sf);
    void x_CleanSeqFeatQuals(CSeq_feat& sf);

    void x_AddNcbiCleanupObject(CSeq_entry& seq_entry);


    // for rpt_unit and replace GenBank qualifiers
    bool        x_CleanupRptUnit(CGb_qual& gbq);
    bool        x_CleanupRptUnitRange(string& val);
    static bool x_IsBaseRange(const string& val);
    static bool x_IsDotBaseRange(const string& val);
    static bool x_IsHyphenBaseRange(const string& val);


    void x_ChangeTransposonToMobileElement(CGb_qual& gbq);
    void x_ChangeInsertionSeqToMobileElement(CGb_qual& gbq);

    void x_AddEnvSamplOrMetagenomic(CBioSource& biosrc);
    void x_CleanupOldName(COrg_ref& org);
    void x_CleanupOrgModNoteEC(COrg_ref& org);


    void x_AddReplaceQual(CSeq_feat& feat, const string& str);

    void x_SeqPointBC(CSeq_point& seq_point);
    void x_SeqIntervalBC(CSeq_interval& seq_interval);
    void x_BothStrandBC(CSeq_interval& seq_interval);
    void x_SplitDbtag(CDbtag& dbt, vector<CRef<CDbtag>>& out_new_dbtags);

    void x_OrgnameModBC(COrgName& orgname, const string& org_ref_common);

public:
    void AddToComment(CSeq_feat& feat, const string& comment);
    void SeqFeatTRNABC(CTrna_ext& tRNA);
    void ExpandCombinedQuals(CSeq_feat::TQual& quals);
    bool DateStdBC(CDate_std& date);
    void BothStrandBC(CSeq_loc& loc);

    // modernize PCR Primer
    void ModernizePCRPrimers(CBioSource& biosrc);
    void CleanupOrgModAndSubSourceOther(COrgName& orgname, CBioSource& biosrc);
    void SubSourceBC(CSubSource& subsrc);
    void OrgModBC(COrgMod& orgmod);

    void FixUnsetMolFromBiomol(CMolInfo::TBiomol biomol, const CBioseq_Handle& bsh);

    void AddPartialToProteinTitle(const CBioseq_Handle& bsh);
    void CleanupECNumberList(CProt_ref::TEc& ec_num_list);
    // e.g. if ends with ",..", turn into "..."
    void FixUpEllipsis(string& str);
    void RemoveFlankingQuotes(string& val);
    void CleanupECNumber(string& ec_num);

private:
    string x_ExtractSatelliteFromComment(string& comment);

    void x_RRNANameBC(string& name);

    void x_CleanupECNumberListEC(CProt_ref::TEc& ec_num_list);


    void x_MoveCdregionXrefsToProt(CSeq_feat& seqfeat);
    bool x_InGpsGenomic(const CSeq_feat& seqfeat);

    void x_AddNonCopiedQual(
        vector<CRef<CGb_qual>>& out_quals,
        const char*             qual,
        const char*             val);

    void x_MoveSeqdescOrgToSourceOrg( CSeqdesc &seqdesc );
    void x_MoveCDSFromNucAnnotToSetAnnot(CBioseq_set& set);

public:
    void GBQualToOrgRef(COrg_ref& org, CSeq_feat& seqfeat);
    // string cleanup funcs
    void CleanupStringMarkChanged(std::string& str);
    void CleanupStringJunkMarkChanged(std::string& str);
    void ConvertDoubleQuotesMarkChanged(std::string& str);
    bool CompressSpaces(string& str);
    void CompressStringSpacesMarkChanged(std::string& str);
    void StripSpacesMarkChanged(std::string& str);
    void RemoveSpacesBetweenTildesMarkChanged(std::string& str);
    void TrimInternalSemicolonsMarkChanged(std::string& str);

    void PostSeqFeat(CSeq_feat& seq_feat);
    void PostOrgRef(COrg_ref& org);
    void PostBiosource(CBioSource& biosrc);
    void MoveSeqfeatOrgToSourceOrg(CSeq_feat &seqfeat);
    void CopyGBBlockDivToOrgnameDiv(const CSeq_entry_Handle& entry_handle);
    void AuthListBCWithFixInitials(CAuth_list& al);

private:
    bool x_TruncateSpacesMarkChanged(std::string& str);
    void x_TranslateITSNameAndFlag(string& in_out_name);

    void x_PCRPrimerSetBC(CPCRPrimerSet& primer_set);

    // After we've traversed the hierarchy of objects, there may be some
    // processing that can only be done after the traversal is complete.
    // This function does that processing.
    void x_PostProcessing(void);

    void xUpdateSeqfeatCitPub(const CSeq_entry_Handle& seh);
    void xUpdateSeqfeatCitPub(const CBioseq_set_Handle& bssh);
    void xUpdateSeqfeatCitPub(const CBioseq_Handle& bsh);
    void xUpdateSeqfeatCitPub(const CSeq_annot_Handle& annotHandle);
    bool xUpdateSeqfeatCitGenLabel(CPub& pub);

    // after cleaning bioseq and bioseq-set, need to clear empty descriptors
    void x_ClearEmptyDescr(CBioseq_set& bioseq_set);
    void x_ClearEmptyDescr(CBioseq& bioseq);

public:
    void ClearEmptyDescr(const CBioseq_Handle& bsh);
    void ClearEmptyDescr(const CBioseq_set_Handle& bssh);
    // removes single-strandedness from non-viral nucleotide sequences
    bool RemoveSingleStrand(const CBioseq_Handle& bsh);
    void LabelMapAppend(const vector<string>& old_labels, const vector<string>& new_labels);

    // functions that prepare for post-processing while traversing
    void NotePubdescOrAnnotPubs(const CPub_equiv& pub_equiv);
    void RememberPubOldLabel(CPub& pub);
    void RememberMuidThatMightBeConvertibleToPmid(CPub& pub);

    void RememberSeqFeatCitPubs(CPub& pub);
    void DecodeXMLMarkChanged(std::string& str);

private:
    void x_NotePubdescOrAnnotPubs_RecursionHelper(
        const CPub_equiv& pub_equiv, TEntrezId& muid, TEntrezId& pmid);
    void AddMolInfo(CBioseq_set& set, const CMolInfo& mol);
    void AddMolInfo(CBioseq& seq, const CMolInfo& mol);

    void x_SortSeqDescs(CSeq_entry& seq_entry);

    void x_FixStructuredCommentKeywords(CSeq_descr& descr);

    void x_RemoveDupBioSource(CBioseq_set& bioseq_set);
    void x_RemoveDupBioSource(CSeq_entry& se, const CBioSource& src);

    void x_RemoveDupPubs(CSeq_descr& descr);

    void x_RemoveProtDescThatDupsProtName(CProt_ref& prot);
    void x_RemoveRedundantComment(CGene_ref& gene, CSeq_feat& seq_feat);

    void        x_RemoveEmptyUserObject(CSeq_descr& seq_descr);
    void        x_SetMolInfoTechFromGenBankBlock(CSeq_descr& seq_descr, CGB_block& block);
    void        x_SetMolInfoTechFromGenBankBlock(CSeq_descr& seq_descr);
    static bool x_CleanGenbankKeywords(CGB_block& blk, CMolInfo::TTech tech);
    void        x_CleanupGenbankBlock(CBioseq& seq);
    void        x_CleanupGenbankBlock(CBioseq_set& set);
    void        x_CleanupGenbankBlock(CSeq_descr& seq_descr);
    void        x_CleanupGenbankBlock(CGB_block& block, bool is_patent, CConstRef<CBioSource> biosrc, CMolInfo::TTech tech);
    static bool x_CanRemoveGenbankBlockSource(const string& src, const CBioSource& biosrc);
    void        x_RescueMolInfo(CBioseq& seq);
    void        x_RemoveOldDescriptors(CSeq_descr& seq_descr);
    void        x_RemoveEmptyDescriptors(CSeq_descr& seq_descr);
    void        x_RemoveEmptyFeatures(CSeq_annot& seq_annot);
    void        x_RemoveEmptyFeatureTables(CBioseq_set& bioseq_set);

public:
    void ExceptTextEC(string& except_text);
    void tRNACodonEC(CSeq_feat& seq_feat);
    void RemoveEmptyFeatures(const CSeq_annot_Handle& sah);
    void RemoveEmptyFeatureTables(const CBioseq_Handle& bsh);
    void MergeAdjacentFeatureTables(CBioseq& bioseq);
    void MergeAdjacentFeatureTables(const CBioseq_Handle& bsh);

private:
    void x_MergeAdjacentFeatureTables(const list<CRef<CSeq_annot>> & annot_list);
    void x_MergeAdjacentFeatureTables(CBioseq_set & bioseq_set);
    bool x_CleanEmptyFeature(CSeq_feat& feat);
    bool x_ShouldRemoveEmptyFeature(const CSeq_feat& feat );
    bool x_CleanEmptyGene(CGene_ref& gene);
    bool x_ShouldRemoveEmptyGene(const CGene_ref& gene, const CSeq_feat& feat);
    bool x_CleanEmptyProt(CProt_ref& prot);
    bool x_ShouldRemoveEmptyProt(const CProt_ref& prot );
    static bool x_IsPubContentBad(const CPubdesc& pub, bool strict);
    static bool x_IsPubContentBad(const CPub& pub);
    static bool x_IsPubContentBad(const CId_pat& pat);
    bool        x_ShouldRemoveEmptyPub(const CPubdesc& pubdesc);
    static bool x_IsGenbankBlockEmpty(const CGB_block& gbk);

public:
    void BondEC(CSeq_feat& feat);
    void RemoveOldFeatures(const CBioseq_Handle & bsh);
private:
    void x_BioseqSetEC(CBioseq_set& bioseq_set);
    void x_ChangePopToPhy(CBioseq_set& bioseq_set);
    void x_CollapseSet(CBioseq_set& set);
    void x_RemovePopPhyBioSource(CBioseq_set& set);
    void x_RemovePopPhyBioSource(CBioseq_set& set, const COrg_ref& org);
    void x_RemovePopPhyBioSource(CBioseq& seq, const COrg_ref& org);
    void x_RemovePopPhyMolInfo(CBioseq_set& set);
    void x_BioseqSetNucProtEC(CBioseq_set& bioseq_set);
    void x_BioseqSetGenBankEC(CBioseq_set& bioseq_set);
    void x_RemoveNestedGenBankSet(CBioseq_set& bioseq_set);
    void x_RemoveNestedNucProtSet(CBioseq_set& bioseq_set);
    void x_MoveNpDBlinks(CBioseq_set& bioseq_set);
    void x_MoveNpSrc(CBioseq_set& bioseq_set);
    void x_MoveNpSrc(CRef<CSeqdesc>& srcdesc, CSeq_descr& descr);
    void x_MoveNPTitle(CBioseq_set& set);

    void x_MoveNpPub(CBioseq_set& bioseq_set);
    void x_MoveNpPub(CBioseq_set& np_set, CSeq_descr& descr);
    void x_MovePopPhyMutPub(CBioseq_set& bioseq_set);
    void x_RemovePub(CSeq_entry& se, const CPubdesc& pub);

    bool x_IsDBLinkUserObj(const CSeqdesc& desc);

    bool x_FixMiscRNA(CSeq_feat& feat);
    void x_ModernizeRNAFeat(CSeq_feat& feat);

    void x_ExtendedCleanupExtra(CSeq_entry_Handle seh);

    CGBQualCleanup& x_GetGBQualCleanup();

    // variables used for the whole cleaning process
    /// If set, holds all the cleanup changes that have occurred so far
    CRef<CCleanupChange> m_Changes;
    /// See CCleanup::EValidOptions
    Uint4                m_Options;
    /// For simplicity, the same object manager is used wherever possible
    CRef<CObjectManager> m_Objmgr;
    /// For simplicity, the same CScope is used wherever possible
    CRef<CScope>         m_Scope;
    /// Set via m_Options when we're in gpipe cleanup mode
    bool                 m_IsGpipe{ false };
    /// Set via m_Options to synchronize Cdregion genetic codes with BioSource
    bool                 m_SyncGenCodes{ false };

    //// Used to bypass x_CleanSeqFeatQuals for flatfile generator with inference in record.
    bool m_HasInferenceQuals{ false };

    void SetGlobalFlags(const CSeq_entry& se, bool reset = true);
    void SetGlobalFlags(const CSeq_submit& ss);
    void SetGlobalFlags(const CBioseq& bs, bool reset = true);
    void SetGlobalFlags(const CBioseq_set& set, bool reset = true);
    void ResetGlobalFlags()
    {
        m_StripSerial  = true;
        m_IsEmblOrDdbj = false;
    }
    /// set if references should NOT have serial numbers
    /// under this entry.
    bool m_StripSerial{ true };
    /// tells if any Seq-id on any Bioseq in the blob being cleaned is embl or ddbj.
    bool m_IsEmblOrDdbj{ false };

    bool m_KeepTopNestedSet{ false };
    bool m_KeepSingleSeqSet{ false };

    unique_ptr<CGBQualCleanup> m_GBQualCleanup;

    friend class CAutogeneratedExtendedCleanup;
};

END_SCOPE(objects)
END_NCBI_SCOPE

#endif /* NEWCLEANUP__HPP */
