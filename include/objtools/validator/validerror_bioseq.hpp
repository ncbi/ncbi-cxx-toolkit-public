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
 *`
 * Author:  Jonathan Kans, Clifford Clausen, Aaron Ucko......
 *
 * File Description:
 *   Privae classes and definition for the validator
 *   .......
 *
 */

#ifndef VALIDATOR___VALIDERROR_BIOSEQ__HPP
#define VALIDATOR___VALIDERROR_BIOSEQ__HPP

#include <corelib/ncbistd.hpp>
#include <corelib/ncbi_autoinit.hpp>

#include <objmgr/scope.hpp>
#include <objmgr/feat_ci.hpp>  // for CMappedFeat
#include <objmgr/util/seq_loc_util.hpp>

#include <objtools/validator/validator.hpp>
#include <objtools/validator/gene_cache.hpp>
#include <objtools/validator/validerror_imp.hpp>
#include <objtools/validator/validerror_base.hpp>
#include <objtools/validator/validerror_feat.hpp>
#include <objtools/validator/validerror_annot.hpp>
#include <objtools/validator/validerror_descr.hpp>


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

//class CSeq_entry;
//class CCit_sub;
//class CCit_art;
//class CCit_gen;
class CSeq_feat;
class CBioseq;
class CSeqdesc;
class CSeq_annot;
class CTrna_ext;
class CProt_ref;
class CSeq_loc;
class CAuth_list;
class CTitle;
class CMolInfo;
class CUser_object;
class CSeqdesc_CI;
class CBioSource;
class COrg_ref;
class CGene_ref;
class CCdregion;
class CRNA_ref;
class CImp_feat;
class CSeq_literal;
class CBioseq_Handle;
class CSeq_feat_Handle;

BEGIN_SCOPE(validator)

class CTaxValidationAndCleanup;
class CGeneCache;
class CValidError_base;
class CValidError_annot;

// =============================================================================
//                            Caching classes
// =============================================================================

// for convenience
typedef CValidator::CCache CCache;


// =============================================================================
//                            Validation classes                          
// =============================================================================
class CValidError_desc;
class CValidError_descr;



// =============================  Validate Bioseq  =============================

// internal structures
class CCdsMatchInfo;
class CMrnaMatchInfo;

typedef map<const CSeq_feat *, CRef<CMrnaMatchInfo> > TmRNAList;

class CValidError_bioseq : private CValidError_base
{
public:
    CValidError_bioseq(CValidError_imp& imp);
    virtual ~CValidError_bioseq(void);

    void ValidateBioseq(const CBioseq& seq);
    void ValidateSeqIds(const CBioseq& seq);
    void ValidateSeqId(const CSeq_id& id, const CBioseq& ctx);
    void ValidateInst(const CBioseq& seq);
    void ValidateBioseqContext(const CBioseq& seq);
    void ValidateHistory(const CBioseq& seq);

    bool GetTSANStretchErrors(const CBioseq& seq);
    bool GetTSAConflictingBiomolTechErrors(const CBioseq& seq);

    static bool IsSelfReferential(const CBioseq& seq);
    static bool IsAllNs(const CSeqVector& vec);
    static int PctNs(CBioseq_Handle bsh);

    static bool IsMaster(const CBioseq& seq);
    static bool IsWGSMaster(const CBioseq& seq, CScope& scope);
    static bool IsWGSMaster(const CSeq_entry& entry);
    static bool IsWGS(const CBioseq& seq);
    static bool IsWGS(CBioseq_Handle bsh);
    static bool IsWGS(const CSeq_entry& entry);
    static bool IsWGSAccession(const CSeq_id& id);
    static bool IsWGSAccession(const CBioseq& seq);
    static bool IsTSAAccession(const CSeq_id& id);
    static bool IsTSAAccession(const CBioseq& seq);
    static bool IsWp(CBioseq_Handle bsh);
    static bool IsEmblOrDdbj(const CBioseq& seq);
    static bool IsGenbank(const CBioseq& seq);
    static bool IsRefSeq(const CBioseq& seq);
    static bool IsPdb(const CBioseq& seq);
    static bool IsPartial(const CBioseq& seq, CScope& scope);

    // DBLink user object counters
    int m_dblink_count;
    int m_taa_count;
    int m_bs_count;
    int m_as_count;
    int m_pdb_count;
    int m_sra_count;
    int m_bp_count;
    int m_unknown_count;

private:
    typedef multimap<string, const CSeq_feat*, PNocase> TStrFeatMap;
    typedef vector<CMappedFeat>                         TMappedFeatVec;
 
    void x_SetupCommonFlags (CBioseq_Handle bsh);
   
    void ValidateSeqLen(const CBioseq& seq);
    void ValidateSegRef(const CBioseq& seq);
    void ValidateDelta(const CBioseq& seq);
    static bool x_IgnoreEndGap(CBioseq_Handle bsh, CSeq_gap::TType gap_type);
    void ValidateSeqGap(const CSeq_gap& gap, const CBioseq& seq);
    void ValidateDeltaLoc(const CSeq_loc& loc, const CBioseq& seq, TSeqPos& len);
    bool ValidateRepr(const CSeq_inst& inst, const CBioseq& seq);
    void ValidateSeqParts(const CBioseq& seq);
    void x_ValidateTitle(const CBioseq& seq);
    void x_ValidateBarcode(const CBioseq& seq);
    void ValidateRawConst(const CBioseq& seq);
    void x_CalculateNsStretchAndTotal(const CBioseq& seq, TSeqPos& num_ns, TSeqPos& max_stretch, bool& n5, bool& n3);
    void ValidateNsAndGaps(const CBioseq& seq);
    void ReportBadAssemblyGap (const CBioseq& seq);
    static bool HasBadWGSGap(const CBioseq& seq);
    void ReportBadWGSGap(const CBioseq& seq);
    void ReportBadTSAGap(const CBioseq& seq);
    void ReportBadGenomeGap(const CBioseq& seq);
    void ValidateWGSMaster(CBioseq_Handle bsh);
    
    void ValidateMultipleGeneOverlap (const CBioseq_Handle& bsh);
    void ValidateBadGeneOverlap(const CSeq_feat& feat);
    void x_ReportGeneOverlapError(const CSeq_feat& feat, const string& gene_label);
    void x_ReportImproperPartial(const CSeq_feat& feat);
    void x_ReportInternalPartial(const CSeq_feat& feat);
    bool x_PartialAdjacentToIntron(const CSeq_loc& loc);
    void ValidateFeatPartialInContext (const CMappedFeat& feat, bool is_complete);
    void x_ReportStartStopPartialProblem(int partial_type, bool at_splice_or_gap, bool abuts_n, const CSeq_feat& feat);

    bool x_IsPartialAtSpliceSiteOrGap (const CSeq_loc& loc, unsigned int tag, bool& bad_seq, bool& is_gap, bool& abuts_n);
    bool x_MatchesOverlappingFeaturePartial (const CMappedFeat& feat, unsigned int partial_type);
    bool x_IsSameAsCDS(const CMappedFeat& feat);
    void ValidateSeqFeatContext(const CBioseq& seq, bool is_complete);
    static bool x_HasPGAPStructuredComment(CBioseq_Handle bsh);
    EDiagSev x_DupFeatSeverity (const CSeq_feat& curr, const CSeq_feat& prev, bool viral, bool htgs, bool same_annot, bool same_label);
    bool x_ReportDupOverlapFeaturePair (const CSeq_feat_Handle & f1, const CSeq_feat_Handle & f2, bool fruit_fly, bool viral, bool htgs);
    bool x_SuppressDicistronic(const CSeq_feat_Handle & f1, const CSeq_feat_Handle & f2, bool fruit_fly);
    void x_ReportOverlappingPeptidePair (CSeq_feat_Handle f1, CSeq_feat_Handle f2, const CBioseq& bioseq, bool& reported_last_peptide);
    void ValidateDupOrOverlapFeats(const CBioseq& seq);
    void ValidateTwintrons(const CBioseq& seq);
    void ValidateCollidingGenes(const CBioseq& seq);
    void ValidateCompleteGenome(const CBioseq& seq);
    void x_CompareStrings(const TStrFeatMap& str_feat_map, const string& type);
    void x_ValidateCompletness(const CBioseq& seq, const CMolInfo& mi);
    void x_ReportSuspiciousUseOfComplete(const CBioseq& seq, EDiagSev sev);
    void x_ValidateAbuttingUTR(const CBioseq_Handle& seq);
    bool x_IsRangeGap (const CBioseq_Handle& seq, int start, int stop);
    void x_ValidateAbuttingRNA(const CBioseq_Handle& seq);
    void x_ValidateGeneCDSmRNACounts (const CBioseq_Handle& seq);
    void x_ValidateCDSmRNAmatch(const CBioseq_Handle& seq, int numgene, int numcds, int nummrna);
    void x_ValidateCDSVDJCmatch(const CBioseq_Handle& seq, int numcds, int numcrgn, int numvseg, int numdseg, int numjseg);
    void x_ValidateCDSagainstVDJC(const CBioseq_Handle& seq);
    void x_CheckForMultiplemRNAs(CCdsMatchInfo& cds_match, const TmRNAList& unmatched_mrnas);
    void x_CheckMrnaProteinLink(const CCdsMatchInfo& cds_match);
    void x_CheckOrigProteinAndTranscriptIds(const CCdsMatchInfo& cds_match);
    void x_TranscriptIDsMatch(const string& protein_id, const CSeq_feat& cds);
    unsigned int x_IdXrefsNotReciprocal (const CSeq_feat &cds, const CSeq_feat &mrna);
    bool x_IdXrefsAreReciprocal (const CSeq_feat &cds, const CSeq_feat &mrna);

    void ValidateSeqDescContext(const CBioseq& seq);
    void CheckForMissingChromosome(CBioseq_Handle bsh);
    void CheckForMultipleStructuredComments(const CBioseq& seq);
    void x_CheckForMultipleComments(CBioseq_Handle bsh);
     void ValidateGBBlock (const CGB_block& gbblock, const CBioseq& seq, const CSeqdesc& desc);
    void ValidateMolInfoContext(const CMolInfo& minfo, int& seq_biomol, int& tech, int& completeness,
        const CBioseq& seq, const CSeqdesc& desc);
    void x_ValidateMolInfoForBioSource(
        const CBioSource& src,
        const CMolInfo& minfo,
        const CSeqdesc& desc
    );
    void x_CheckSingleStrandedRNAViruses(
        const CBioSource& source,
        const string& lineage,
        const string& stranded_mol,
        const CMolInfo::TBiomol biomol,
        const CBioseq_Handle& bsh,
        const CSerialObject& obj,
        const CSeq_entry    *ctx
    );

    // for conflicts between lineage and molecule type
    typedef enum {
        eStrandedMoltype_unknown = 0,
        eStrandedMoltype_ssRNA = 1,
        eStrandedMoltype_dsRNA = 2,
        eStrandedMoltype_ssDNA = 4,
        eStrandedMoltype_dsDNA = 8
    } EStrandedMoltype;
    static string s_GetStrandedMolStringFromLineage(const string& lineage);

    void x_ReportLineageConflictWithMol(
        const string& lineage,
        const string& stranded_mol,
        const CMolInfo::TBiomol biomol,
        CSeq_inst::EMol mol,
        const CSerialObject& obj,
        const CSeq_entry *ctx
    );
    void ValidateMolTypeContext(const EGIBB_mol& gibb, EGIBB_mol& seq_biomol,
        const CBioseq& seq, const CSeqdesc& desc);
    void ValidateUpdateDateContext(const CDate& update,const CDate& create,
        const CBioseq& seq, const CSeqdesc& desc);
    void ValidateOrgContext(const CSeqdesc_CI& iter, const COrg_ref& this_org,
        const COrg_ref& org, const CBioseq& seq, const CSeqdesc& desc);
    void ReportModifInconsistentError (int new_mod, int& old_mod, const CSeqdesc& desc, const CSeq_entry& ctx);
    void ValidateModifDescriptors (const CBioseq& seq);
    void ValidateMoltypeDescriptors (const CBioseq& seq);

    void ValidateSecondaryAccConflict(const string& primary_acc,
        const CBioseq& seq, int choice);
    void ValidateIDSetAgainstDb(const CBioseq& seq);
    void x_ValidateSourceFeatures(const CBioseq_Handle& bsh);
    void x_ValidateOverlappingRNAFeatures(const CBioseq_Handle& bsh);
    void x_ValidatePubFeatures(const CBioseq_Handle& bsh);
    void x_ReportDuplicatePubLabels (const CBioseq& seq, const vector<CTempString>& labels);
    void x_ValidateMultiplePubs(
        const CBioseq_Handle& bsh);

    void CheckForPubOnBioseq(const CBioseq& seq);
    void CheckSourceDescriptor(const CBioseq_Handle& bsh);
    static bool x_ParentAndComponentLocationsDiffer(CBioseq_Handle bsh, CBioSource::TGenome parent_location);
    static size_t x_BadMetazoanMitochondrialLength(const CBioSource& src, const CSeq_inst& inst);
    void CheckForMolinfoOnBioseq(const CBioseq& seq);
    void CheckTpaHistory(const CBioseq& seq);

    size_t GetDataLen(const CSeq_inst& inst);
    bool CdError(const CBioseq_Handle& bsh);
    bool IsMrna(const CBioseq_Handle& bsh);
    size_t NumOfIntervals(const CSeq_loc& loc);
    //bool NotPeptideException(const CFeat_CI& curr, const CFeat_CI& prev);
    //bool IsSameSeqAnnot(const CFeat_CI& fi1, const CFeat_CI& fi2);
    bool IsIdIn(const CSeq_id& id, const CBioseq& seq);
    bool SuppressTrailingXMsg(const CBioseq& seq);
    CRef<CSeq_loc> GetLocFromSeq(const CBioseq& seq);
    bool IsHistAssemblyMissing(const CBioseq& seq);
    bool IsFlybaseDbxrefs(const TDbtags& dbxrefs);
    bool GraphsOnBioseq(const CBioseq& seq) const;
    bool IsSynthetic(const CBioseq& seq) const;
    bool x_IsArtificial(const CBioseq& seq) const;
    bool x_IsActiveFin(const CBioseq& seq) const;
    bool x_IsMicroRNA(const CBioseq& seq) const;
    bool x_IsDeltaLitOnly(const CSeq_inst& inst) const;
    bool x_ShowBioProjectWarning(const CBioseq& seq);
    bool x_HasCitSub(CBioseq_Handle bsh) const;
    static bool x_HasCitSub(const CPub_equiv& pub);
    static bool x_HasCitSub(const CPub& pub);

    void ValidateCDSUTR(const CBioseq& seq);
    bool x_ReportUTRPair(const CSeq_feat& utr5, const CSeq_feat& utr3);

    size_t x_CountAdjacentNs(const CSeq_literal& lit);
    void x_CheckGeneralIDs(const CBioseq& seq);

    static bool x_HasGap(const CBioseq& seq);

    //internal validators
    CValidError_annot m_AnnotValidator;
    CValidError_descr m_DescrValidator;
    CValidError_feat  m_FeatValidator;

    // BioseqHandle for bioseq currently being validated - to cut down on overhead
    CBioseq_Handle m_CurrentHandle;

    // feature iterator for genes on bioseq - to cut down on overhead
    // (This class does *not* own this)
    const CCacheImpl::TFeatValue * m_GeneIt;

    // feature iterator for all features on bioseq (again, trying to cut down on overhead
    // (This class does *not* own this)
    const CCacheImpl::TFeatValue * m_AllFeatIt;

    bool m_splicing_not_expected;
    bool m_report_missing_chromosome;
    bool m_report_short_seq;
    bool m_is_bact_or_arch;
    bool m_is_plasmid;
    bool m_is_chromosome;
};



END_SCOPE(validator)
END_SCOPE(objects)
END_NCBI_SCOPE

#endif  /* VALIDATOR___VALIDERROR_BIOSEQ__HPP */
