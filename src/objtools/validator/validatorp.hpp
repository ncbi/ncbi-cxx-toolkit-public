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

#ifndef VALIDATOR___VALIDATORP__HPP
#define VALIDATOR___VALIDATORP__HPP

#include <corelib/ncbistd.hpp>

#include <objmgr/scope.hpp>
#include <objmgr/feat_ci.hpp>  // for CMappedFeat
#include <objects/seqset/Bioseq_set.hpp>
#include <objects/seq/GIBB_mol.hpp>
#include <util/strsearch.hpp>
#include <objects/seqfeat/Seq_feat.hpp>
#include <objects/seqalign/Seq_align.hpp>
#include <objects/seqalign/Std_seg.hpp>
#include <objects/seqalign/Packed_seg.hpp>

#include <objtools/validator/validator.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

class CSeq_entry;
class CCit_sub;
class CCit_art;
class CCit_gen;
class CSeq_feat;
class CBioseq;
class CSeqdesc;
class CSeq_annot;
class CTrna_ext;
class CProt_ref;
class CSeq_loc;
class CFeat_CI;
class CPub_set;
class CAuthor;
class CTitle;
class CDesc_CI;
class CMolInfo;
class CUser_object;
class CSeqdesc_CI;
class CSeq_graph;
class CMappedGraph;
class CDense_diag;
class CDense_seg;
class CSeq_align_set;
class CPubdesc;
class CBioSource;
class COrg_ref;
class CByte_graph;
class CDelta_seq;
class CSeqFeatData;
class CGene_ref;
class CCdregion;
class CRNA_ref;
class CImp_feat;
class CSeq_literal;
class CBioseq_Handle;


BEGIN_SCOPE(validator)


// ===========================  Internal error types  ==========================

#define ERR_CODE_BEGIN(x) x##BEGIN
#define ERR_CODE_END(x) x##END

enum EErrType {
    eErr_ALL = 0,

    ERR_CODE_BEGIN(SEQ_INST),
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
    eErr_SEQ_INST_HistAssemblyMissing,
    eErr_SEQ_INST_TerminalNs,
    eErr_SEQ_INST_UnexpectedIdentifierChange,
    eErr_SEQ_INST_InternalNsInSeqLit,
    eErr_SEQ_INST_SeqLitGapLength0,
    eErr_SEQ_INST_TpaAssmeblyProblem,
    eErr_SEQ_INST_SeqLocLength,
    eErr_SEQ_INST_MissingGaps,
    ERR_CODE_END(SEQ_INST),

    ERR_CODE_BEGIN(SEQ_DESCR),
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
    eErr_SEQ_DESCR_Obsolete,
    eErr_SEQ_DESCR_UnnecessaryBioSourceFocus,
    eErr_SEQ_DESCR_RefGeneTrackingWithoutStatus,
    eErr_SEQ_DESCR_UnwantedCompleteFlag,
    eErr_SEQ_DESCR_CollidingPublications,
    eErr_SEQ_DESCR_TransgenicProblem,
    ERR_CODE_END(SEQ_DESCR),

    ERR_CODE_BEGIN(GENERIC),
    eErr_GENERIC_NonAsciiAsn,
    eErr_GENERIC_Spell,
    eErr_GENERIC_AuthorListHasEtAl,
    eErr_GENERIC_MissingPubInfo,
    eErr_GENERIC_UnnecessaryPubEquiv,
    eErr_GENERIC_BadPageNumbering,
    eErr_GENERIC_MedlineEntryPub,
    ERR_CODE_END(GENERIC),

    ERR_CODE_BEGIN(SEQ_PKG),
    eErr_SEQ_PKG_BEGIN,
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
    eErr_SEQ_PKG_GraphPackagingProblem,
    ERR_CODE_END(SEQ_PKG),

    ERR_CODE_BEGIN(SEQ_FEAT),
    eErr_SEQ_FEAT_InvalidForType,
    eErr_SEQ_FEAT_PartialProblem,
    eErr_SEQ_FEAT_PartialsInconsistent,
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
    eErr_SEQ_FEAT_BadTrnaAA,
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
    eErr_SEQ_FEAT_CollidingLocusTags,
    eErr_SEQ_FEAT_MultiIntervalGene,
    eErr_SEQ_FEAT_FeatContentDup,
    eErr_SEQ_FEAT_BadProductSeqId,
    eErr_SEQ_FEAT_RnaProductMismatch,
    eErr_SEQ_FEAT_DifferntIdTypesInSeqLoc,
    eErr_SEQ_FEAT_MissingCDSproduct,
    eErr_SEQ_FEAT_MissingLocation,
    eErr_SEQ_FEAT_OnlyGeneXrefs,
    eErr_SEQ_FEAT_UTRdoesNotAbutCDS,
    eErr_SEQ_FEAT_MultipleCdsOnMrna,
    eErr_SEQ_FEAT_BadConflictFlag,
    eErr_SEQ_FEAT_ConflictFlagSet,
    eErr_SEQ_FEAT_LocusTagProblem,
    eErr_SEQ_FEAT_AltStartCodon,
    eErr_SEQ_FEAT_GenesInconsistent,
    eErr_SEQ_FEAT_DuplicateTranslExcept,
    eErr_SEQ_FEAT_TranslExceptAndRnaEditing,
    eErr_SEQ_FEAT_NoNameForProtein,
    ERR_CODE_END(SEQ_FEAT),

    ERR_CODE_BEGIN(SEQ_ALIGN),
    eErr_SEQ_ALIGN_SeqIdProblem,
    eErr_SEQ_ALIGN_StrandRev,
    eErr_SEQ_ALIGN_DensegLenStart,
    eErr_SEQ_ALIGN_StartMorethanBiolen,
    eErr_SEQ_ALIGN_EndMorethanBiolen,
    eErr_SEQ_ALIGN_LenMorethanBiolen,
    eErr_SEQ_ALIGN_SumLenStart,
    eErr_SEQ_ALIGN_SegsDimMismatch,
    eErr_SEQ_ALIGN_SegsNumsegMismatch,
    eErr_SEQ_ALIGN_SegsStartsMismatch,
    eErr_SEQ_ALIGN_SegsPresentMismatch,
    eErr_SEQ_ALIGN_SegsPresentStartsMismatch,
    eErr_SEQ_ALIGN_SegsPresentStrandsMismatch,
    eErr_SEQ_ALIGN_FastaLike,
    eErr_SEQ_ALIGN_SegmentGap,
    eErr_SEQ_ALIGN_SegsInvalidDim,
    eErr_SEQ_ALIGN_Segtype,
    eErr_SEQ_ALIGN_BlastAligns,
    ERR_CODE_END(SEQ_ALIGN),

    ERR_CODE_BEGIN(SEQ_GRAPH),
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
    ERR_CODE_END(SEQ_GRAPH),

    ERR_CODE_BEGIN(INTERNAL),
    eErr_Internal_Exception,
    ERR_CODE_END(INTERNAL),

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
    CValidError_imp(CObjectManager& objmgr, CValidError* errors, 
        Uint4 options = 0);
    virtual ~CValidError_imp(void);

    // Validation methods
    bool Validate(const CSeq_entry& se, const CCit_sub* cs = 0,
        CScope* scope = 0);
    void Validate(const CSeq_submit& ss, CScope* scope = 0);
    void Validate(const CSeq_annot& sa, CScope* scope = 0);

    void SetProgressCallback(CValidator::TProgressCallback callback,
        void* user_data);
public:
    // interface to be used by the various validation classes

    // typedefs:
    typedef const CSeq_feat& TFeat;
    typedef const CBioseq& TBioseq;
    typedef const CBioseq_set& TSet;
    typedef const CSeqdesc& TDesc;
    typedef const CSeq_annot& TAnnot;
    typedef const CSeq_graph& TGraph;
    typedef const CSeq_align& TAlign;
    typedef const CSeq_entry& TEntry;
    typedef const CSeq_feat::TDbxref TDbtags;
    typedef map < const CSeq_feat*, const CSeq_annot* >& TFeatAnnotMap;

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
    void PostErr(EDiagSev sv, EErrType et, const string& msg, TGraph graph);
    void PostErr(EDiagSev sv, EErrType et, const string& msg, TBioseq sq,
        TGraph graph);
    void PostErr(EDiagSev sv, EErrType et, const string& msg, TAlign align);
    void PostErr(EDiagSev sv, EErrType et, const string& msg, TEntry entry);

    // General use validation methods
    void ValidatePubdesc(const CPubdesc& pub, const CSerialObject& obj);
    void ValidateBioSource(const CBioSource& bsrc, const CSerialObject& obj);
    void ValidateSeqLoc(const CSeq_loc& loc, const CBioseq_Handle& seq,
        const string& prefix, const CSerialObject& obj);
    void ValidateSeqLocIds(const CSeq_loc& loc, const CSerialObject& obj);
    void ValidateDbxref(const CDbtag& xref, const CSerialObject& obj,
        bool biosource = false);
    void ValidateDbxref(TDbtags& xref_list, const CSerialObject& obj,
        bool biosource = false);
    void ValidateCitSub(const CCit_sub& cs, const CSerialObject& obj);
        
    // getters
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
    inline bool IsValidateIdSet(void) const { return m_ValidateIdSet; }
    inline bool IsRemoteFetch(void) const { return m_RemoteFetch; }

    // !!! DEBUG {
    inline bool AvoidPerfBottlenecks() const { return m_PerfBottlenecks; }
    // }

    // flags calculated by examining data in record
    inline bool IsStandaloneAnnot(void) const { return m_IsStandaloneAnnot; }
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
    inline bool IsCuratedRefSeq(void) const;
    
    const CSeq_entry& GetTSE(void) { return *m_TSE; }

    TFeatAnnotMap GetFeatAnnotMap(void);

    void AddBioseqWithNoPub(const CBioseq& seq);
    void AddBioseqWithNoBiosource(const CBioseq& seq);
    void AddBioseqWithNoMolinfo(const CBioseq& seq);
    void AddProtWithoutFullRef(const CBioseq_Handle& seq);
    void ReportMissingPubs(const CSeq_entry& se, const CCit_sub* cs);
    void ReportMissingBiosource(const CSeq_entry& se);
    void ReportProtWithoutFullRef(void);
    void ReportBioseqsWithNoMolinfo(void);

    bool IsNucAcc(const string& acc);
    bool IsFarLocation(const CSeq_loc& loc);
    CConstRef<CSeq_feat> GetCDSGivenProduct(const CBioseq& seq);
    const CSeq_entry* GetAncestor(const CBioseq& seq, CBioseq_set::EClass clss);
    bool IsSerialNumberInComment(const string& comment);

    bool CheckSeqVector(const CSeqVector& vec);
    bool IsSequenceAvaliable(const CSeqVector& vec);

    bool IsTransgenic(const CBioSource& bsrc);

private:
    // Prohibit copy constructor & assignment operator
    CValidError_imp(const CValidError_imp&);
    CValidError_imp& operator= (const CValidError_imp&);

    void Setup(const CSeq_entry& se, CScope* scope);
    void Setup(const CSeq_annot& sa, CScope* scope);
    void SetScope(const CSeq_entry& se);
    void SetScope(const CSeq_annot& sa);

    void InitializeSourceQualTags();
    void ValidateSourceQualTags(const string& str, const CSerialObject& obj);

    bool IsMixedStrands(const CSeq_loc& loc);

    void ValidatePubGen(const CCit_gen& gen, const CSerialObject& obj);
    void ValidatePubArticle(const CCit_art& art, int uid, const CSerialObject& obj);
    void ValidateEtAl(const CPubdesc& pubdesc, const CSerialObject& obj);
    
    bool HasName(const list< CRef< CAuthor > >& authors);
    bool HasTitle(const CTitle& title);
    bool HasIsoJTA(const CTitle& title);

    CRef<CObjectManager>    m_ObjMgr;
    CRef<CScope>            m_Scope;
    CConstRef<CSeq_entry>   m_TSE;

    // error repoitory
    CValidError*       m_ErrRepository;

    // flags derived from options parameter
    bool m_NonASCII;            // User sets if Non ASCII char found
    bool m_SuppressContext;     // Include context in errors if true
    bool m_ValidateAlignments;  // Validate Alignments if true
    bool m_ValidateExons;       // Check exon feature splice sites
    bool m_SpliceErr;           // Bad splice site error if true, else warn
    bool m_OvlPepErr;           // Peptide overlap error if true, else warn
    bool m_RequireTaxonID;      // BioSource requires taxonID dbxref
    bool m_RequireISOJTA;       // Journal requires ISO JTA
    bool m_ValidateIdSet;       // validate update against ID set in database
    bool m_RemoteFetch;         // Remote fetch enabled?

    // !!! DEBUG {
    bool m_PerfBottlenecks;         // Skip suspected performance bottlenecks
    // }

    // flags calculated by examining data in record
    bool m_IsStandaloneAnnot;
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
    vector< CConstRef<CSeq_id> >    m_InitialSeqIds;
    // prot bioseqs without a full reference (reporting cds feature)
    vector< CConstRef<CSeq_feat> >  m_ProtWithNoFullRef;
    // Bioseqs without pubs (should be considered only if m_NoPubs is false)
    vector< CConstRef<CBioseq> >    m_BioseqWithNoPubs;
    // Bioseqs without source (should be considered only if m_NoSource is false)
    vector< CConstRef<CBioseq> >    m_BioseqWithNoSource;
    // Bioseqs without MolInfo
    vector< CConstRef<CBioseq> >    m_BioseqWithNoMolinfo;

    // legal dbxref database strings
    static const string legalDbXrefs[];
    static const string legalRefSeqDbXrefs[];

    // source qulalifiers prefixes
    static const string sm_SourceQualPrefixes[];
    static auto_ptr<CTextFsa> m_SourceQualTags;

    CValidator::TProgressCallback m_PrgCallback;
    CValidator::CProgressInfo     m_PrgInfo;
    SIZE_TYPE   m_NumAlign;
    SIZE_TYPE   m_NumAnnot;
    SIZE_TYPE   m_NumBioseq;
    SIZE_TYPE   m_NumBioseq_set;
    SIZE_TYPE   m_NumDesc;
    SIZE_TYPE   m_NumDescr;
    SIZE_TYPE   m_NumFeat;
    SIZE_TYPE   m_NumGraph;
};


// =============================================================================
//                         Specific validation classes
// =============================================================================



class CValidError_base
{
protected:
    // typedefs:
    typedef CValidError_imp::TFeat TFeat;
    typedef CValidError_imp::TBioseq TBioseq;
    typedef CValidError_imp::TSet TSet;
    typedef CValidError_imp::TDesc TDesc;
    typedef CValidError_imp::TAnnot TAnnot;
    typedef CValidError_imp::TGraph TGraph;
    typedef CValidError_imp::TAlign TAlign;
    typedef CValidError_imp::TEntry TEntry;
    typedef CValidError_imp::TDbtags TDbtags;

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
    void PostErr(EDiagSev sv, EErrType et, const string& msg, TGraph graph);
    void PostErr(EDiagSev sv, EErrType et, const string& msg, TBioseq sq,
        TGraph graph);
    void PostErr(EDiagSev sv, EErrType et, const string& msg, TAlign align);
    void PostErr(EDiagSev sv, EErrType et, const string& msg, TEntry entry);

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
    void ValidateHistory(const CBioseq& seq);

    size_t GetTpaWithHistory(void)    const { return m_TpaWithHistory;    }
    size_t GetTpaWithoutHistory(void) const { return m_TpaWithoutHistory; }

private:
    typedef multimap<string, const CSeq_feat*, PNocase> TStrFeatMap;
    typedef vector<CMappedFeat>                         TMappedFeatVec;

    static const size_t scm_AdjacentNsThreshold; // = 80
    
    void ValidateSeqLen(const CBioseq& seq);
    void ValidateSegRef(const CBioseq& seq);
    void ValidateDelta(const CBioseq& seq);
    bool ValidateRepr(const CSeq_inst& inst, const CBioseq& seq);
    void ValidateSeqParts(const CBioseq& seq);
    void ValidateProteinTitle(const CBioseq& seq);
    void ValidateRawConst(const CBioseq& seq);
    void ValidateNs(const CBioseq& seq);
    
    void ValidateMultiIntervalGene (const CBioseq& seq);
    void ValidateSeqFeatContext(const CBioseq& seq);
    void ValidateDupOrOverlapFeats(const CBioseq& seq);
    void ValidateCollidingGenes(const CBioseq& seq);
    void x_CompareStrings(const TStrFeatMap& str_feat_map, const string& type,
        EErrType err, EDiagSev sev);
    void x_ValidateCompletness(const CBioseq& seq, const CMolInfo& mi);
    void x_ValidateAbuttingUTR(const CBioseq_Handle& seq);
    void x_ValidateAbuttingCDSGroup(const TMappedFeatVec& cds_group, bool minus);

    void ValidateSeqDescContext(const CBioseq& seq);
    void ValidateMolInfoContext(const CMolInfo& minfo, int& seq_biomol,
        const CBioseq& seq, const CSeqdesc& desc);
    void ValidateMolTypeContext(const EGIBB_mol& gibb, EGIBB_mol& seq_biomol,
        const CBioseq& seq, const CSeqdesc& desc);
    void ValidateUpdateDateContext(const CDate& update,const CDate& create,
        const CBioseq& seq, const CSeqdesc& desc);
    void ValidateOrgContext(const CSeqdesc_CI& iter, const COrg_ref& this_org,
        const COrg_ref& org, const CBioseq& seq, const CSeqdesc& desc);

    void ValidateGraphsOnBioseq(const CBioseq& seq);
    void ValidateByteGraphOnBioseq(const CSeq_graph& graph, const CBioseq& seq);
    void ValidateGraphOnDeltaBioseq(const CBioseq& seq, bool& validate_values);
    void ValidateGraphValues(const CSeq_graph& graph, const CBioseq& seq);
    void ValidateMinValues(const CByte_graph& bg);
    void ValidateMaxValues(const CByte_graph& bg);
    void ValidatemRNABioseqContext(const CBioseq_Handle& seq);
    bool GetLitLength(const CDelta_seq& delta, TSeqPos& len);
    bool IsSuportedGraphType(const CSeq_graph& graph) const;
    SIZE_TYPE GetSeqLen(const CBioseq& seq);

    void ValidateSecondaryAccConflict(const string& primary_acc,
        const CBioseq& seq, int choice);
    void ValidateIDSetAgainstDb(const CBioseq& seq);
    void x_ValidateMultiplePubs(const CBioseq_Handle& bsh);

    void CheckForPubOnBioseq(const CBioseq& seq);
    void CheckSoureDescriptor(const CBioseq_Handle& bsh);
    void CheckForMolinfoOnBioseq(const CBioseq& seq);
    void CheckTpaHistory(const CBioseq& seq);

    TSeqPos GetDataLen(const CSeq_inst& inst);
    bool CdError(const CBioseq_Handle& bsh);
    bool IsMrna(const CBioseq_Handle& bsh);
    bool IsPrerna(const CBioseq_Handle& bsh);
    size_t NumOfIntervals(const CSeq_loc& loc);
    bool LocOnSeg(const CBioseq& seq, const CSeq_loc& loc);
    bool NotPeptideException(const CFeat_CI& curr, const CFeat_CI& prev);
    bool IsSameSeqAnnot(const CFeat_CI& fi1, const CFeat_CI& fi2);
    bool IsSameSeqAnnotDesc(const CFeat_CI& fi1, const CFeat_CI& fi2);
    bool IsIdIn(const CSeq_id& id, const CBioseq& seq);
    bool SuppressTrailingXMsg(const CBioseq& seq);
    bool GetLocFromSeq(const CBioseq& seq, CSeq_loc* loc);
    bool IsDifferentDbxrefs(const TDbtags& dbxref1,
                            const TDbtags& dbxref2);
    bool IsHistAssemblyMissing(const CBioseq& seq);
    bool IsFlybaseDbxrefs(const TDbtags& dbxrefs);
    bool GraphsOnBioseq(const CBioseq& seq) const;
    bool IsOtherDNA(const CBioseq& seq) const;
    bool IsSynthetic(const CBioseq& seq) const;
    bool x_IsArtificial(const CBioseq& seq) const;
    bool x_IsActiveFin(const CBioseq& seq) const;
    bool x_IsMicroRNA(const CBioseq& seq) const;
    bool x_IsDeltaLitOnly(const CSeq_inst& inst) const;
    
    size_t x_CountAdjacentNs(const CSeq_literal& lit);

    // data
    size_t m_TpaWithHistory;
    size_t m_TpaWithoutHistory;
};


// =============================  Validate SeqFeat  ============================


class CValidError_feat : private CValidError_base
{
public:
    CValidError_feat(CValidError_imp& imp);
    virtual ~CValidError_feat(void);

    void ValidateSeqFeat(const CSeq_feat& feat);

    size_t GetNumGenes    (void) const { return m_NumGenes; }
    size_t GetNumGeneXrefs(void) const { return m_NumGeneXrefs; }

private:
    void ValidateSeqFeatData(const CSeqFeatData& data, const CSeq_feat& feat);
    void ValidateSeqFeatProduct(const CSeq_loc& prod, const CSeq_feat& feat);
    void ValidateGene(const CGene_ref& gene, const CSeq_feat& feat);
    void ValidateGeneXRef(const CSeq_feat& feat);
    void ValidateOperon(const CSeq_feat& feat);

    void ValidateCdregion(const CCdregion& cdregion, const CSeq_feat& obj);
    void ValidateCdTrans(const CSeq_feat& feat);
    void ValidateCdsProductId(const CSeq_feat& feat);
    void ValidateCdConflict(const CCdregion& cdregion, const CSeq_feat& feat);
    void ReportCdTransErrors(const CSeq_feat& feat,
        bool show_stop, bool got_stop, bool no_end, int ragged);
    void ValidateSplice(const CSeq_feat& feat, bool check_all);
    void ValidateBothStrands(const CSeq_feat& feat);
    void ValidateCommonCDSProduct(const CSeq_feat& feat);
    void ValidateBadMRNAOverlap(const CSeq_feat& feat);
    void ValidateBadGeneOverlap(const CSeq_feat& feat);
    void ValidateCDSPartial(const CSeq_feat& feat);
    void ValidateCodeBreakNotOnCodon(const CSeq_feat& feat,const CSeq_loc& loc,
                                     const CCdregion& cdregion);
    void x_ValidateCdregionCodebreak(const CCdregion& cds, const CSeq_feat& feat);

    void ValidateProt(const CProt_ref& prot, const CSerialObject& obj);

    void ValidateRna(const CRNA_ref& rna, const CSeq_feat& feat);
    void ValidateAnticodon(const CSeq_loc& anticodon, const CSeq_feat& feat);
    void ValidateTrnaCodons(const CTrna_ext& trna, const CSeq_feat& feat);
    void ValidateMrnaTrans(const CSeq_feat& feat);
    void ValidateCommonMRNAProduct(const CSeq_feat& feat);
    void ValidateRnaProductType(const CRNA_ref& rna, const CSeq_feat& feat);

    void ValidateImp(const CImp_feat& imp, const CSeq_feat& feat);
    void ValidateImpGbquals(const CImp_feat& imp, const CSeq_feat& feat);
    void ValidatePeptideOnCodonBoundry(const CSeq_feat& feat, 
        const string& key);

    void ValidateFeatPartialness(const CSeq_feat& feat);
    void ValidateExcept(const CSeq_feat& feat);
    void ValidateExceptText(const string& text, const CSeq_feat& feat);

    void ValidateFeatCit(const CPub_set& cit, const CSeq_feat& feat);
    void ValidateFeatComment(const string& comment, const CSeq_feat& feat);
    void ValidateFeatBioSource(const CBioSource& bsrc, const CSeq_feat& feat);

    bool IsPlastid(int genome);
    bool IsOverlappingGenePseudo(const CSeq_feat& feat);
    unsigned char Residue(unsigned char res);
    int  CheckForRaggedEnd(const CSeq_loc&, const CCdregion& cdr);
    bool SuppressCheck(const string& except_text);
    string MapToNTCoords(const CSeq_feat& feat, const CSeq_loc& product,
        TSeqPos pos);

    bool IsPartialAtSpliceSite(const CSeq_loc& loc, unsigned int errtype);
    bool IsSameAsCDS(const CSeq_feat& feat);
    bool IsCDDFeat(const CSeq_feat& feat) const;

    // data
    size_t m_NumGenes;
    size_t m_NumGeneXrefs;
};


// =============================  Validate SeqDesc  ============================


class CValidError_desc : private CValidError_base
{
public:
    CValidError_desc(CValidError_imp& imp);
    virtual ~CValidError_desc(void);

    void ValidateSeqDesc(const CSeqdesc& desc);

private:

    void ValidateComment(const string& comment, const CSeqdesc& desc);
    void ValidateUser(const CUser_object& usr, const CSeqdesc& desc);
    void ValidateMolInfo(const CMolInfo& minfo, const CSeqdesc& desc);
};


// ============================  Validate SeqAlign  ============================


class CValidError_align : private CValidError_base
{
public:
    CValidError_align(CValidError_imp& imp);
    virtual ~CValidError_align(void);

    void ValidateSeqAlign(const CSeq_align& align);

private:
    typedef CSeq_align::C_Segs::TDendiag    TDendiag;
    typedef CSeq_align::C_Segs::TDenseg     TDenseg;
    typedef CSeq_align::C_Segs::TPacked     TPacked;
    typedef CSeq_align::C_Segs::TStd        TStd;
    typedef CSeq_align::C_Segs::TDisc       TDisc;

    void x_ValidateDendiag(const TDendiag& dendiags, const CSeq_align& align);
    void x_ValidateDenseg(const TDenseg& denseg, const CSeq_align& align);
    void x_ValidateStd(const TStd& stdsegs, const CSeq_align& align);
    void x_ValidatePacked(const TPacked& packed, const CSeq_align& align);
    size_t x_CountBits(const CPacked_seg::TPresent& present);

    // Check if dimension is valid
    template <typename T>
    bool x_ValidateDim(T& obj, const CSeq_align& align, size_t part = 0);

    // Check if the  strand is consistent in SeqAlignment of global 
    // or partial type
    void x_ValidateStrand(const TDenseg& denseg, const CSeq_align& align);
    void x_ValidateStrand(const TPacked& packed, const CSeq_align& align);
    void x_ValidateStrand(const TStd& std_segs, const CSeq_align& align);

    // Check if an alignment is FASTA-like. 
    // Alignment is FASTA-like if all gaps are at the end with dimensions > 2.
    void x_ValidateFastaLike(const TDenseg& denseg, const CSeq_align& align);
    void x_ValidateFastaLike(const TPacked& packed, const CSeq_align& align);
    void x_ValidateFastaLike(const TStd& std_segs, const CSeq_align& align);

    // Check if there is a gap for all sequences in a segment.
    void x_ValidateSegmentGap(const TDenseg& denseg, const CSeq_align& align);
    void x_ValidateSegmentGap(const TPacked& packed, const CSeq_align& align);
    void x_ValidateSegmentGap(const TStd& std_segs, const CSeq_align& align);

    // Validate SeqId in sequence alignment.
    void x_ValidateSeqId(const CSeq_align& align);
    void x_GetIds(const CSeq_align& align, vector< CRef< CSeq_id > >& ids);

    // Check segment length, start and end point in Dense_seg, Dense_diag 
    // and Std_seg
    void x_ValidateSeqLength(const TDenseg& denseg, const CSeq_align& align);
    void x_ValidateSeqLength(const TPacked& packed, const CSeq_align& align);
    void x_ValidateSeqLength(const TStd& std_segs, const CSeq_align& align);
    void x_ValidateSeqLength(const CDense_diag& dendiag, size_t dendiag_num,
        const CSeq_align& align);
};


// ============================  Validate SeqGraph  ============================


class CValidError_graph : private CValidError_base
{
public:
    CValidError_graph(CValidError_imp& imp);
    virtual ~CValidError_graph(void);

    void ValidateSeqGraph(const CSeq_graph& graph);

    SIZE_TYPE GetNumMisplacedGraphs(void) const { return m_NumMisplaced; }

private:
    bool x_IsMisplaced(const CSeq_graph& graph);

    SIZE_TYPE   m_NumMisplaced;
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
* Revision 1.72  2004/08/03 13:40:26  shomrat
* + eErr_GENERIC_MedlineEntryPub
*
* Revision 1.71  2004/07/29 17:09:45  shomrat
* + eErr_SEQ_DESCR_TransgenicProblem
*
* Revision 1.70  2004/07/29 16:08:51  shomrat
* Separated error message from offending object's description; Added error group
*
* Revision 1.69  2004/07/07 16:04:33  shomrat
* + ValidateAnticodon
*
* Revision 1.68  2004/07/07 13:27:17  shomrat
* + SEQ_FEAT_NoNameForProtein
*
* Revision 1.67  2004/06/25 15:58:27  shomrat
* + eErr_SEQ_INST_MissingGaps
*
* Revision 1.66  2004/06/25 14:57:23  shomrat
* +eErr_SEQ_FEAT_DuplicateTranslExcept,eErr_SEQ_FEAT_TranslExceptAndRnaEditing
*
* Revision 1.65  2004/06/17 17:03:29  shomrat
* Added CollidingPublications check
*
* Revision 1.64  2004/05/10 13:56:22  shomrat
* Use CSeq_feat_Handle instead of FeatAnnotMap
*
* Revision 1.63  2004/05/10 13:20:11  shomrat
* + methods for validation of abutting UTRs
*
* Revision 1.62  2004/04/27 18:40:50  shomrat
* ProtWithoutFullRef report CDS feature instead of protein bioseq
*
* Revision 1.61  2004/04/23 16:26:35  shomrat
* Use sequence::GetNucleotideParent() instead of GetNucGivenProt()
*
* Revision 1.60  2004/03/25 18:32:26  shomrat
* + ValidatemRNABioseqContext()
*
* Revision 1.59  2004/03/19 14:48:14  shomrat
* + SEQ_FEAT_PartialsInconsistent
*
* Revision 1.58  2004/03/10 21:23:09  shomrat
* + SEQ_DESCR_UnwantedCompleteFlag
*
* Revision 1.57  2004/03/01 18:39:21  shomrat
* Added alternative start codon error
*
* Revision 1.56  2004/02/25 15:52:51  shomrat
* Added CollidingLocusTags error
*
* Revision 1.55  2004/01/16 20:08:14  shomrat
* Added LocusTagProblem error
*
* Revision 1.54  2004/01/12 20:24:42  shomrat
* Added ValidateCitSub
*
* Revision 1.53  2003/12/17 19:15:19  shomrat
* declarations for graph packaging problem test
*
* Revision 1.52  2003/12/16 17:34:48  shomrat
* Added SEQ_INST_SeqLocLength
*
* Revision 1.51  2003/12/16 16:18:40  shomrat
* Added ValidateCdConflict
*
* Revision 1.50  2003/11/14 15:55:48  shomrat
* added TPA history check
*
* Revision 1.49  2003/11/12 20:30:24  shomrat
* added error code for multiple cds on mRNA bioseq
*
* Revision 1.48  2003/10/27 17:00:37  shomrat
* changed signature of ValidatePubArticle
*
* Revision 1.47  2003/10/27 14:53:57  shomrat
* added eErr_SEQ_FEAT_UTRdoesNotAbutCDS
*
* Revision 1.46  2003/10/27 14:14:11  shomrat
* added eErr_SEQ_INST_SeqLitGapLength0
*
* Revision 1.45  2003/10/24 17:56:07  shomrat
* added ValidateOperon
*
* Revision 1.44  2003/10/20 18:30:43  shomrat
* added x_IsArtificial
*
* Revision 1.43  2003/10/20 16:08:55  shomrat
* genes and gene xrefs counters
*
* Revision 1.42  2003/10/13 18:44:02  shomrat
* Added error code for bad tRNA amino acid
*
* Revision 1.41  2003/10/01 22:42:45  ucko
* Actually declare CValidError_feat::IsCDDFeat....
*
* Revision 1.40  2003/09/23 13:25:55  shomrat
* Added IsDeltaSeqOnly test
*
* Revision 1.39  2003/09/22 20:23:33  shomrat
* Added x_IsMicroRNA
*
* Revision 1.38  2003/09/03 18:25:09  shomrat
* added SEQ_DESCR_RefGeneTrackingWithoutStatus
*
* Revision 1.37  2003/08/06 15:05:42  shomrat
* Added test for adjacent Ns in Seq-literal
*
* Revision 1.36  2003/07/24 20:16:18  vasilche
* Fixed typedefs for dbxref: list<CRef<CDbtag>> -> vector<CRef<CDbtag>>
*
* Revision 1.35  2003/07/21 21:18:19  shomrat
* Added eErr_SEQ_FEAT_MissingLocation
*
* Revision 1.34  2003/07/15 18:26:43  shomrat
* Added x_IsActiveFin
*
* Revision 1.33  2003/07/02 21:03:38  shomrat
* Added ValidateCDSPartial
*
* Revision 1.32  2003/06/27 18:53:20  shomrat
* Changed ValidateDbxref definition
*
* Revision 1.31  2003/06/19 15:34:09  shomrat
* Added IsOtherDNA and IsSynthetic
*
* Revision 1.30  2003/06/02 16:06:43  dicuccio
* Rearranged src/objects/ subtree.  This includes the following shifts:
*     - src/objects/asn2asn --> arc/app/asn2asn
*     - src/objects/testmedline --> src/objects/ncbimime/test
*     - src/objects/objmgr --> src/objmgr
*     - src/objects/util --> src/objmgr/util
*     - src/objects/alnmgr --> src/objtools/alnmgr
*     - src/objects/flat --> src/objtools/flat
*     - src/objects/validator --> src/objtools/validator
*     - src/objects/cddalignview --> src/objtools/cddalignview
* In addition, libseq now includes six of the objects/seq... libs, and libmmdb
* replaces the three libmmdb? libs.
*
* Revision 1.29  2003/05/28 16:23:45  shomrat
* Minor corrections.
*
* Revision 1.28  2003/05/05 15:35:45  shomrat
* Added ValidateCdsProductId
*
* Revision 1.27  2003/04/29 14:55:09  shomrat
* Added SeqAlign validation
*
* Revision 1.26  2003/04/24 16:16:00  vasilche
* Added missing includes and forward class declarations.
*
* Revision 1.25  2003/04/15 14:53:32  shomrat
* Added a progress callback mechanism
*
* Revision 1.24  2003/04/10 19:25:07  shomrat
* Added ValidateMolTypeContext
*
* Revision 1.23  2003/04/07 14:56:09  shomrat
* Added Seq-loc ids validation
*
* Revision 1.22  2003/04/04 18:39:17  shomrat
* Added Internal Exception error and tests for the availabilty of the sequence
*
* Revision 1.21  2003/03/31 14:41:21  shomrat
* $id: -> $id$
*
* Revision 1.20  2003/03/28 16:28:53  shomrat
* Added Seq-graph tests related methods
*
* Revision 1.19  2003/03/21 21:12:17  shomrat
* Added eErr_SEQ_DESCR_UnnecessaryBioSourceFocus; Change to signature of ValidateOrgContext
*
* Revision 1.18  2003/03/21 19:37:04  shomrat
* Added IsNucAcc and IsFlybaseDbxrefs
*
* Revision 1.17  2003/03/21 16:21:46  shomrat
* Added ValidateIDSetAgainstDb
*
* Revision 1.16  2003/03/20 18:55:28  shomrat
* Added validation of standalone Seq-annot objects
*
* Revision 1.15  2003/02/24 20:16:41  shomrat
* Holds refernce to the CValidError object, and not the internal TErrs
*
* Revision 1.14  2003/02/14 21:46:20  shomrat
* Added methods to check Bioseqs with no MolInfo
*
* Revision 1.13  2003/02/12 17:40:35  shomrat
* Added function for SEQ_DESCR checks
*
* Revision 1.12  2003/02/07 21:09:43  shomrat
* Added eErr_SEQ_INST_TerminalNs; Added helper methods
*
* Revision 1.11  2003/02/03 20:18:14  shomrat
* Added flag to skip performance bottlenecks (for testing)
*
* Revision 1.10  2003/02/03 17:08:48  shomrat
* Changed return value for GetCDSGivenProduct
*
* Revision 1.9  2003/01/29 21:55:19  shomrat
* Added check for et al
*
* Revision 1.8  2003/01/24 20:39:40  shomrat
* Added history validation for bioseq
*
* Revision 1.7  2003/01/21 20:20:43  shomrat
* Added private methods to facilitate the PubDesc Validation
*
* Revision 1.6  2003/01/10 16:31:23  shomrat
* Added private functions to CValidError_feat
*
* Revision 1.5  2003/01/08 18:35:31  shomrat
* Added mapping features to their enclosing annotation
*
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
