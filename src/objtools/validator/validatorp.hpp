/*  $Id:
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
 * Author:  Jonathan Kans, Clifford Clausen, Aaron Ucko......
 *
 * File Description:
 *   Privae classes and definition for the validator
 *   .......
 *
 */

#ifndef VALIDATOR___VALIDATORP__HPP
#define VALIDATOR___VALIDATORP__HPP

#include <corelib/ncbistd.hpp>

#include <objects/objmgr/scope.hpp>
#include <objects/seqset/Bioseq_set.hpp>
#include <util/strsearch.hpp>

#include <objects/validator/validator.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

class CSeq_entry;
class CCit_sub;
class CSeq_feat;
class CBioseq;
class CSeqdesc;
class CSeq_annot;
class CTrna_ext;
class CProt_ref;
class CSeq_loc;
class CFeat_CI;

BEGIN_SCOPE(validator)


// ===========================  Internal error types  ==========================

enum EErrType {
    eErr_ALL = 0,

    eErr_SEQ_INST_ExtNotAllowed,
    eErr_SEQ_INST_ExtBadOrMissing,
    eErr_SEQ_INST_SeqDataNotFound,
    eErr_SEQ_INST_SeqDataNotAllowed,
    eErr_SEQ_INST_ReprInvalid,
    eErr_SEQ_INST_CircularProtein,
    eErr_SEQ_INST_DSProtein,
    eErr_SEQ_INST_MolNotSet,
    eErr_SEQ_INST_MolOther,
    eErr_SEQ_INST_FuzzyLen,
    eErr_SEQ_INST_InvalidLen,
    eErr_SEQ_INST_InvalidAlphabet,
    eErr_SEQ_INST_SeqDataLenWrong,
    eErr_SEQ_INST_SeqPortFail,
    eErr_SEQ_INST_InvalidResidue,
    eErr_SEQ_INST_StopInProtein,
    eErr_SEQ_INST_PartialInconsistent,
    eErr_SEQ_INST_ShortSeq,
    eErr_SEQ_INST_NoIdOnBioseq,
    eErr_SEQ_INST_BadDeltaSeq,
    eErr_SEQ_INST_LongHtgsSequence,
    eErr_SEQ_INST_LongLiteralSequence,
    eErr_SEQ_INST_SequenceExceeds350kbp,
    eErr_SEQ_INST_ConflictingIdsOnBioseq,
    eErr_SEQ_INST_MolNuclAcid,
    eErr_SEQ_INST_ConflictingBiomolTech,
    eErr_SEQ_INST_SeqIdNameHasSpace,
    eErr_SEQ_INST_IdOnMultipleBioseqs,
    eErr_SEQ_INST_DuplicateSegmentReferences,
    eErr_SEQ_INST_TrailingX,
    eErr_SEQ_INST_BadSeqIdFormat,
    eErr_SEQ_INST_PartsOutOfOrder,
    eErr_SEQ_INST_BadSecondaryAccn,
    eErr_SEQ_INST_ZeroGiNumber,
    eErr_SEQ_INST_RnaDnaConflict,
    eErr_SEQ_INST_HistoryGiCollision,
    eErr_SEQ_INST_GiWithoutAccession,
    eErr_SEQ_INST_MultipleAccessions,

    eErr_SEQ_DESCR_BioSourceMissing,
    eErr_SEQ_DESCR_InvalidForType,
    eErr_SEQ_DESCR_FileOpenCollision,
    eErr_SEQ_DESCR_Unknown,
    eErr_SEQ_DESCR_NoPubFound,
    eErr_SEQ_DESCR_NoOrgFound,
    eErr_SEQ_DESCR_MultipleBioSources,
    eErr_SEQ_DESCR_NoMolInfoFound,
    eErr_SEQ_DESCR_BadCountryCode,
    eErr_SEQ_DESCR_NoTaxonID,
    eErr_SEQ_DESCR_InconsistentBioSources,
    eErr_SEQ_DESCR_MissingLineage,
    eErr_SEQ_DESCR_SerialInComment,
    eErr_SEQ_DESCR_BioSourceNeedsFocus,
    eErr_SEQ_DESCR_BadOrganelle,
    eErr_SEQ_DESCR_MultipleChromosomes,
    eErr_SEQ_DESCR_BadSubSource,
    eErr_SEQ_DESCR_BadOrgMod,
    eErr_SEQ_DESCR_InconsistentProteinTitle,
    eErr_SEQ_DESCR_Inconsistent,
    eErr_SEQ_DESCR_ObsoleteSourceLocation,
    eErr_SEQ_DESCR_ObsoleteSourceQual,
    eErr_SEQ_DESCR_StructuredSourceNote,
    eErr_SEQ_DESCR_MultipleTitles,

    eErr_GENERIC_NonAsciiAsn,
    eErr_GENERIC_Spell,
    eErr_GENERIC_AuthorListHasEtAl,
    eErr_GENERIC_MissingPubInfo,
    eErr_GENERIC_UnnecessaryPubEquiv,
    eErr_GENERIC_BadPageNumbering,

    eErr_SEQ_PKG_NoCdRegionPtr,
    eErr_SEQ_PKG_NucProtProblem,
    eErr_SEQ_PKG_SegSetProblem,
    eErr_SEQ_PKG_EmptySet,
    eErr_SEQ_PKG_NucProtNotSegSet,
    eErr_SEQ_PKG_SegSetNotParts,
    eErr_SEQ_PKG_SegSetMixedBioseqs,
    eErr_SEQ_PKG_PartsSetMixedBioseqs,
    eErr_SEQ_PKG_PartsSetHasSets,
    eErr_SEQ_PKG_FeaturePackagingProblem,
    eErr_SEQ_PKG_GenomicProductPackagingProblem,
    eErr_SEQ_PKG_InconsistentMolInfoBiomols,

    eErr_SEQ_FEAT_InvalidForType,
    eErr_SEQ_FEAT_PartialProblem,
    eErr_SEQ_FEAT_InvalidType,
    eErr_SEQ_FEAT_Range,
    eErr_SEQ_FEAT_MixedStrand,
    eErr_SEQ_FEAT_SeqLocOrder,
    eErr_SEQ_FEAT_CdTransFail,
    eErr_SEQ_FEAT_StartCodon,
    eErr_SEQ_FEAT_InternalStop,
    eErr_SEQ_FEAT_NoProtein,
    eErr_SEQ_FEAT_MisMatchAA,
    eErr_SEQ_FEAT_TransLen,
    eErr_SEQ_FEAT_NoStop,
    eErr_SEQ_FEAT_TranslExcept,
    eErr_SEQ_FEAT_NoProtRefFound,
    eErr_SEQ_FEAT_NotSpliceConsensus,
    eErr_SEQ_FEAT_OrfCdsHasProduct,
    eErr_SEQ_FEAT_GeneRefHasNoData,
    eErr_SEQ_FEAT_ExceptInconsistent,
    eErr_SEQ_FEAT_ProtRefHasNoData,
    eErr_SEQ_FEAT_GenCodeMismatch,
    eErr_SEQ_FEAT_RNAtype0,
    eErr_SEQ_FEAT_UnknownImpFeatKey,
    eErr_SEQ_FEAT_UnknownImpFeatQual,
    eErr_SEQ_FEAT_WrongQualOnImpFeat,
    eErr_SEQ_FEAT_MissingQualOnImpFeat,
    eErr_SEQ_FEAT_PsuedoCdsHasProduct,
    eErr_SEQ_FEAT_IllegalDbXref,
    eErr_SEQ_FEAT_FarLocation,
    eErr_SEQ_FEAT_DuplicateFeat,
    eErr_SEQ_FEAT_UnnecessaryGeneXref,
    eErr_SEQ_FEAT_TranslExceptPhase,
    eErr_SEQ_FEAT_TrnaCodonWrong,
    eErr_SEQ_FEAT_BothStrands,
    eErr_SEQ_FEAT_CDSgeneRange,
    eErr_SEQ_FEAT_CDSmRNArange,
    eErr_SEQ_FEAT_OverlappingPeptideFeat,
    eErr_SEQ_FEAT_SerialInComment,
    eErr_SEQ_FEAT_MultipleCDSproducts,
    eErr_SEQ_FEAT_FocusOnBioSourceFeature,
    eErr_SEQ_FEAT_PeptideFeatOutOfFrame,
    eErr_SEQ_FEAT_InvalidQualifierValue,
    eErr_SEQ_FEAT_MultipleMRNAproducts,
    eErr_SEQ_FEAT_mRNAgeneRange,
    eErr_SEQ_FEAT_TranscriptLen,
    eErr_SEQ_FEAT_TranscriptMismatches,
    eErr_SEQ_FEAT_CDSproductPackagingProblem,
    eErr_SEQ_FEAT_DuplicateInterval,
    eErr_SEQ_FEAT_PolyAsiteNotPoint,
    eErr_SEQ_FEAT_ImpFeatBadLoc,
    eErr_SEQ_FEAT_LocOnSegmentedBioseq,
    eErr_SEQ_FEAT_UnnecessaryCitPubEquiv,
    eErr_SEQ_FEAT_ImpCDShasTranslation,
    eErr_SEQ_FEAT_ImpCDSnotPseudo,
    eErr_SEQ_FEAT_MissingMRNAproduct,
    eErr_SEQ_FEAT_AbuttingIntervals,
    eErr_SEQ_FEAT_CollidingGeneNames,
    eErr_SEQ_FEAT_MultiIntervalGene,
    eErr_SEQ_FEAT_FeatContentDup,
    eErr_SEQ_FEAT_BadProductSeqId,

    eErr_SEQ_ALIGN_SeqIdProblem,
    eErr_SEQ_ALIGN_StrandRev,
    eErr_SEQ_ALIGN_DensegLenStart,
    eErr_SEQ_ALIGN_StartLessthanZero,
    eErr_SEQ_ALIGN_StartMorethanBiolen,
    eErr_SEQ_ALIGN_EndLessthanZero,
    eErr_SEQ_ALIGN_EndMorethanBiolen,
    eErr_SEQ_ALIGN_LenLessthanZero,
    eErr_SEQ_ALIGN_LenMorethanBiolen,
    eErr_SEQ_ALIGN_SumLenStart,
    eErr_SEQ_ALIGN_AlignDimSeqIdNotMatch,
    eErr_SEQ_ALIGN_SegsDimSeqIdNotMatch,
    eErr_SEQ_ALIGN_FastaLike,
    eErr_SEQ_ALIGN_NullSegs,
    eErr_SEQ_ALIGN_SegmentGap,
    eErr_SEQ_ALIGN_SegsDimOne,
    eErr_SEQ_ALIGN_AlignDimOne,
    eErr_SEQ_ALIGN_Segtype,
    eErr_SEQ_ALIGN_BlastAligns,

    eErr_SEQ_GRAPH_GraphMin,
    eErr_SEQ_GRAPH_GraphMax,
    eErr_SEQ_GRAPH_GraphBelow,
    eErr_SEQ_GRAPH_GraphAbove,
    eErr_SEQ_GRAPH_GraphByteLen,
    eErr_SEQ_GRAPH_GraphOutOfOrder,
    eErr_SEQ_GRAPH_GraphBioseqLen,
    eErr_SEQ_GRAPH_GraphSeqLitLen,
    eErr_SEQ_GRAPH_GraphSeqLocLen,
    eErr_SEQ_GRAPH_GraphStartPhase,
    eErr_SEQ_GRAPH_GraphStopPhase,
    eErr_SEQ_GRAPH_GraphDiffNumber,
    eErr_SEQ_GRAPH_GraphACGTScore,
    eErr_SEQ_GRAPH_GraphNScore,
    eErr_SEQ_GRAPH_GraphGapScore,
    eErr_SEQ_GRAPH_GraphOverlap,

    eErr_UNKNOWN
};


// =============================================================================
//                            Validation classes                          
// =============================================================================

// ===========================  Central Validation  ==========================

// CValidError_imp provides the entry point to the validation process.
// It calls upon the various validation classes to perform validation of
// each part.
// The class holds all the data for the validation process. 
class CValidError_imp
{
public:
    // Interface to be used by the CValidError class

    // Constructor & Destructor
    CValidError_imp(CObjectManager&, TErrs&, unsigned int options = 0);
    virtual ~CValidError_imp(void);

    // Validation methods
    void Validate(const CSeq_entry& se, const CCit_sub* cs = 0);
    void Validate(const CSeq_submit& ss);

public:
    // interface to be used by the various validation classes

    // typedefs:
    typedef const CSeq_feat& TFeat;
    typedef const CBioseq& TBioseq;
    typedef const CBioseq_set& TSet;
    typedef const CSeqdesc& TDesc;
    typedef const CSeq_annot& TAnnot;
    typedef const list< CRef< CDbtag > >&TDbtags;

    // Posts errors.
    void PostErr(EDiagSev sv, EErrType et, const string& msg,
        const CSerialObject& obj);
    void PostErr(EDiagSev sv, EErrType et, const string& msg, TDesc ds);
    void PostErr(EDiagSev sv, EErrType et, const string& msg, TFeat ft);
    void PostErr(EDiagSev sv, EErrType et, const string& msg, TBioseq sq);
    void PostErr(EDiagSev sv, EErrType et, const string& msg, TBioseq sq,
        TDesc ds);
    void PostErr(EDiagSev sv, EErrType et, const string& msg, TSet set);
    void PostErr(EDiagSev sv, EErrType et, const string& msg, TSet set, 
        TDesc ds);
    void PostErr(EDiagSev sv, EErrType et, const string& msg, TAnnot annot);

    // General use validation methods
    void ValidatePubdesc(const CPubdesc& pub, const CSerialObject& obj);
    void ValidateBioSource(const CBioSource& bsrc, const CSerialObject& obj);
    void ValidateSeqLoc(const CSeq_loc& loc, const CBioseq& seq,
        const string& prefix);
    void ValidateDbxref(const CDbtag& xref, const CSerialObject& obj);
    void ValidateDbxref(TDbtags xref_list, const CSerialObject& obj);

    // getters
    inline CObjectManager* GetObjMgr(void)  { return m_ObjMgr; }
    inline CScope* GetScope(void) { return m_Scope; }

    // flags derived from options parameter
    inline bool IsNonASCII(void) const { return m_NonASCII; }
    inline bool IsSuppressContext(void) const { return m_SuppressContext; }
    inline bool IsValidateAlignments(void) const { return m_ValidateAlignments; }
    inline bool IsValidateExons(void) const { return m_ValidateExons; }
    inline bool IsSpliceErr(void) const { return m_SpliceErr; }
    inline bool IsOvlPepErr(void) const { return m_OvlPepErr; }
    inline bool IsRequireTaxonID(void) const { return m_RequireTaxonID; }
    inline bool IsRequireISOJTA(void) const { return m_RequireISOJTA; }

    // flags calculated by examining data in record
    inline bool IsNoPubs(void) const { return m_NoPubs; }
    inline bool IsNoBioSource(void) const { return m_NoBioSource; }
    inline bool IsGPS(void) const { return m_IsGPS; }
    inline bool IsGED(void) const { return m_IsGED; }
    inline bool IsPDB(void) const { return m_IsPDB; }
    inline bool IsTPA(void) const { return m_IsTPA; }
    inline bool IsPatent(void) const { return m_IsPatent; }
    inline bool IsRefSeq(void) const { return m_IsRefSeq; }
    inline bool IsNC(void) const { return m_IsNC; }
    inline bool IsNG(void) const { return m_IsNG; }
    inline bool IsNM(void) const { return m_IsNM; }
    inline bool IsNP(void) const { return m_IsNP; }
    inline bool IsNR(void) const { return m_IsNR; }
    inline bool IsNS(void) const { return m_IsNS; }
    inline bool IsNT(void) const { return m_IsNT; }
    inline bool IsNW(void) const { return m_IsNW; }
    inline bool IsXR(void) const { return m_IsXR; }
    inline bool IsGI(void) const { return m_IsGI; }

    void AddBioseqWithNoPub(const CBioseq& seq);
    void AddBioseqWithNoBiosource(const CBioseq& seq);
    void AddProtWithoutFullRef(const CBioseq& seq);
    void ReportMissingPubs(const CBioseq& seq, const CCit_sub* cs);
    void ReportMissingBiosource(const CBioseq& seq);
    void ReportProtWithoutFullRef(void);

    bool IsFarLocation(const CSeq_loc& loc) const;
    const CSeq_feat* GetCDSGivenProduct(const CBioseq& seq);
    const CSeq_entry* GetAncestor(const CBioseq& seq, CBioseq_set::EClass clss);

private:
    // Prohibit copy constructor & assignment operator
    CValidError_imp(const CValidError_imp&);
    CValidError_imp& operator= (const CValidError_imp&);

    void Setup(const CSeq_entry& se);
    void SetScope(const CSeq_entry& se);

    void InitializeSourceQualTags();
    void ValidateSourceQualTags(const string& str, const CSerialObject& obj);

    bool IsMixedStrands(const CBioseq& seq, const CSeq_loc& loc);


    CObjectManager* const m_ObjMgr;
    CRef<CScope> m_Scope;

    // error repoitory
    TErrs*              m_Errors;

    // flags derived from options parameter
    bool m_NonASCII;                // User sets if Non ASCII char found
    bool m_SuppressContext;         // Include context in errors if true
    bool m_ValidateAlignments;      // Validate Alignments if true
    bool m_ValidateExons;           // Check exon feature splice sites
    bool m_SpliceErr;               // Bad splice site error if true, else warn
    bool m_OvlPepErr;               // Peptide overlap error if true, else warn
    bool m_RequireTaxonID;          // BioSource requires taxonID dbxref
    bool m_RequireISOJTA;           // Journal requires ISO JTA

    // flags calculated by examining data in record
    bool m_NoPubs;                  // Suppress no pub error if true
    bool m_NoBioSource;             // Suppress no organism error if true
    bool m_IsGPS;
    bool m_IsGED;
    bool m_IsPDB;
    bool m_IsTPA;
    bool m_IsPatent;
    bool m_IsRefSeq;
    bool m_IsNC;
    bool m_IsNG;
    bool m_IsNM;
    bool m_IsNP;
    bool m_IsNR;
    bool m_IsNS;
    bool m_IsNT;
    bool m_IsNW;
    bool m_IsXR;
    bool m_IsGI;

    // seq ids contained within the orignal seq entry. 
    // (used to check for far location)
    set< CConstRef<CSeq_id> >    m_InitialSeqIds;
    // prot bioseqs without a full reference
    vector< CConstRef<CBioseq> > m_ProtWithNoFullRef;
    // Bioseqs without pubs (should be considered only if m_NoPubs is false)
    vector< CConstRef<CBioseq> > m_BioseqWithNoPubs;
    // Bioseqs without source (should be considered only if m_NoSource is false)
    vector< CConstRef<CBioseq> > m_BioseqWithNoSource;

    // legal dbxref database strings
    static const string legalDbXrefs[];
    static const string legalRefSeqDbXrefs[];

    // source qulalifiers prefixes
    static const string sm_SourceQualPrefixes[];
    CTextFsa m_SourceQualTags;
};


// =============================================================================
//                         Specific validation classes
// =============================================================================



class CValidError_base
{
protected:
    // typedefs:
    typedef const CSeq_feat& TFeat;
    typedef const CBioseq& TBioseq;
    typedef const CBioseq_set& TSet;
    typedef const CSeqdesc& TDesc;
    typedef const CSeq_annot& TAnnot;
    typedef const list< CRef< CDbtag > >&TDbtags;

    CValidError_base(CValidError_imp& imp);
    virtual ~CValidError_base();

    void PostErr(EDiagSev sv, EErrType et, const string& msg,
        const CSerialObject& obj);
    void PostErr(EDiagSev sv, EErrType et, const string& msg, TDesc ds);
    void PostErr(EDiagSev sv, EErrType et, const string& msg, TFeat ft);
    void PostErr(EDiagSev sv, EErrType et, const string& msg, TBioseq sq);
    void PostErr(EDiagSev sv, EErrType et, const string& msg, TBioseq sq,
        TDesc ds);
    void PostErr(EDiagSev sv, EErrType et, const string& msg, TSet set);
    void PostErr(EDiagSev sv, EErrType et, const string& msg, TSet set, 
        TDesc ds);
    void PostErr(EDiagSev sv, EErrType et, const string& msg, TAnnot annot);

    CValidError_imp& m_Imp;
    CScope* m_Scope;
};


// ===========================  Validate Bioseq_set  ===========================


class CValidError_bioseqset : private CValidError_base
{
public:
    CValidError_bioseqset(CValidError_imp& imp);
    virtual ~CValidError_bioseqset(void);

    void ValidateBioseqSet(const CBioseq_set& seqset);

private:

    void ValidateNucProtSet(const CBioseq_set& seqset, int nuccnt, int protcnt);
    void ValidateSegSet(const CBioseq_set& seqset, int segcnt);
    void ValidatePartsSet(const CBioseq_set& seqset);
    void ValidatePopSet(const CBioseq_set& seqset);
    void ValidateGenProdSet(const CBioseq_set& seqset);

    bool IsMrnaProductInGPS(const CBioseq& seq); 
};


// =============================  Validate Bioseq  =============================


class CValidError_bioseq : private CValidError_base
{
public:
    CValidError_bioseq(CValidError_imp& imp);
    virtual ~CValidError_bioseq(void);

    void ValidateSeqIds(const CBioseq& seq);
    void ValidateInst(const CBioseq& seq);
    void ValidateBioseqContext(const CBioseq& seq);

private:
    
    void ValidateSeqLen(const CBioseq& seq);
    void ValidateSegRef(const CBioseq& seq);
    void ValidateDelta(const CBioseq& seq);
    bool ValidateRepr(const CSeq_inst& inst, const CBioseq& seq);
    void ValidateSeqParts(const CBioseq& seq);
    void ValidateProteinTitle(const CBioseq& seq);
    void ValidateRawConst(const CBioseq& seq);
    
    void ValidateMultiIntervalGene (const CBioseq& seq);
    void ValidateSeqFeatContext(const CBioseq& seq);
    void ValidateDupOrOverlapFeats(const CBioseq& seq);
    void ValidateCollidingGeneNames(const CBioseq& seq);
    void ValidateSeqDescContext(const CBioseq& seq);

    void CheckForPubOnBioseq(const CBioseq& seq);
    void CheckForBiosourceOnBioseq(const CBioseq& seq);
    
    TSeqPos GetDataLen(const CSeq_inst& inst);
    bool CdError(const CBioseq& seq);
    bool IsMrna(const CBioseq_Handle& bsh);
    bool IsPrerna(const CBioseq_Handle& bsh);
    size_t NumOfIntervals(const CSeq_loc& loc);
    bool LocOnSeg(const CBioseq& seq, const CSeq_loc& loc);
    bool NotPeptideException(const CFeat_CI& curr, const CFeat_CI& prev);
    bool IsEqualSeqAnnot(const CFeat_CI& fi1, const CFeat_CI& fi2);
    bool IsEqualSeqAnnotDesc(const CFeat_CI& fi1, const CFeat_CI& fi2);
    bool IsIdIn(const CSeq_id& id, const CBioseq& seq);
    bool SuppressTrailingXMsg(const CBioseq& seq);
    bool GetLocFromSeq(const CBioseq& seq, CSeq_loc* loc);
    bool IsDifferentDbxrefs(const list< CRef< CDbtag > >& dbxref1,
        const list< CRef< CDbtag > >& dbxref2);

    const CBioseq* GetNucGivenProt(const CBioseq& prot);
};


// =============================  Validate SeqFeat  ============================


class CValidError_feat : private CValidError_base
{
public:
    CValidError_feat(CValidError_imp& imp);
    virtual ~CValidError_feat(void);

    void ValidateSeqFeat(const CSeq_feat& feat);

private:
    void ValidateGene(const CGene_ref& gene, const CSerialObject& obj);

    void ValidateCdregion(const CCdregion& cdregion, const CSeq_feat& obj);
    void ValidateCdTrans(const CSeq_feat& feat);
    void ReportCdTransErrors(const CSeq_feat& feat,
        bool show_stop, bool got_stop, bool no_end, int ragged);
    void ValidateSplice(const CSeq_feat& feat, bool check_all);
    void ValidateBothStrands(const CSeq_feat& feat);
    void ValidateCommonCDSProduct(const CSeq_feat& feat);
    void ValidateBadMRNAOverlap(const CSeq_feat& feat);
    void ValidateBadGeneOverlap(const CSeq_feat& feat);
    void ValidateCodeBreakNotOnCodon(const CSeq_feat& feat,const CSeq_loc& loc,
                                     const CCdregion& cdregion);

    void ValidateProt(const CProt_ref& prot, const CSerialObject& obj);

    void ValidateRna(const CRNA_ref& rna, const CSeq_feat& feat);
    void ValidateTrnaCodons(const CTrna_ext& trna, const CSeq_feat& feat);
    void ValidateMrnaTrans(const CSeq_feat& feat);
    void ValidateCommonMRNAProduct(const CSeq_feat& feat);

    void ValidateImp(const CImp_feat& imp, const CSeq_feat& feat);
    void ValidateImpGbquals(const CImp_feat& imp, const CSeq_feat& feat);
    void ValidatePeptideOnCodonBoundry(const CSeq_feat& feat, 
        const string& key);

    void ValidateFeatPartialness(const CSeq_feat& feat);
    void ValidateExcept(const CSeq_feat& feat);
    void ValidateExceptText(const string& text, const CSeq_feat& feat);
    
    bool SerialNumberInComment(const string& comment);
    bool IsPlastid(int genome);
    bool IsOverlappingGenePseudo(const CSeq_feat& feat);
    bool IsResidue(unsigned char res);
    unsigned char Residue(unsigned char res);
    int  CheckForRaggedEnd(const CSeq_loc&, const CCdregion& cdr);
    bool SuppressCheck(const string& except_text);
    string MapToNTCoords(const CSeq_feat& feat, const CSeq_loc& product,
        TSeqPos pos);

    void ValidateGeneXRef(const CSeq_feat& feat);
    
    bool IsPartialAtSpliceSite(const CSeq_loc& loc, unsigned int errtype);
};


// =============================  Validate SeqDesc  ============================


class CValidError_desc : private CValidError_base
{
public:
    CValidError_desc(CValidError_imp& imp);
    virtual ~CValidError_desc(void);

    void ValidateSeqDesc(const CSeqdesc& desc);

private:
};


// ============================  Validate SeqAlign  ============================


class CValidError_align : private CValidError_base
{
public:
    CValidError_align(CValidError_imp& imp);
    virtual ~CValidError_align(void);

    void ValidateSeqAlign(const CSeq_align& bsset);

private:
};


// ============================  Validate SeqGraph  ============================


class CValidError_graph : private CValidError_base
{
public:
    CValidError_graph(CValidError_imp& imp);
    virtual ~CValidError_graph(void);

    void ValidateSeqGraph(const CSeq_graph& graph);

private:
};


// ============================  Validate SeqAnnot  ============================


class CValidError_annot : private CValidError_base
{
public:
    CValidError_annot(CValidError_imp& imp);
    virtual ~CValidError_annot(void);

    void ValidateSeqAnnot(const CSeq_annot& annot);
private:
};


// ============================  Validate SeqDescr  ============================


class CValidError_descr : private CValidError_base
{
public:
    CValidError_descr(CValidError_imp& imp);
    virtual ~CValidError_descr(void);

    void ValidateSeqDescr(const CSeq_descr& descr);
private:
};


END_SCOPE(validator)
END_SCOPE(objects)
END_NCBI_SCOPE


/*
* ===========================================================================
*
* $Log$
* Revision 1.4  2003/01/02 21:57:21  shomrat
* Add eErr_SEQ_FEAT_BadProductSeqId error; Chenges in (mostly private) interfaces of validation classes
*
* Revision 1.3  2002/12/24 16:52:15  shomrat
* Changes to include directives
*
* Revision 1.2  2002/12/23 20:09:02  shomrat
* Added static functions as private members; Added base class for specific validation classes
*
*
* ===========================================================================
*/

#endif  /* VALIDATOR___VALIDATORP__HPP */
