/* $Id$
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
 * Author:  Jonathan Kans, Clifford Clausen, Aaron Ucko.......
 *
 * File Description:
 *   Validates CSeq_entries and CSeq_submits
 *
 */
#include <serial/objostr.hpp>
#include <serial/serial.hpp>
#include <serial/iterator.hpp>
#include <serial/enumvalues.hpp>

#include <objects/objmgr/scope.hpp>
#include <objects/objmgr/seq_vector.hpp>
#include <objects/objmgr/feat_ci.hpp>
#include <objects/objmgr/desc_ci.hpp>
#include <objects/objmgr/seqdesc_ci.hpp>

#include <objects/general/Int_fuzz.hpp>
#include <objects/general/Dbtag.hpp>
#include <objects/general/User_object.hpp>
#include <objects/general/Object_id.hpp>
#include <objects/general/Date.hpp>

#include <objects/pub/Pub.hpp>

#include <objects/seqset/Seq_entry.hpp>
#include <objects/seqset/Bioseq_set.hpp>

#include <objects/seq/seq__.hpp>
#include <objects/seq/seqport_util.hpp>
#include <objects/seq/gencode.hpp>

#include <objects/seqalign/Seq_align.hpp>

#include <objects/seqblock/GB_block.hpp>
#include <objects/seqblock/EMBL_block.hpp>

#include <objects/seqfeat/Gene_ref.hpp>
#include <objects/seqfeat/Genetic_code.hpp>
#include <objects/seqfeat/Seq_feat.hpp>
#include <objects/seqfeat/SeqFeatData.hpp>
#include <objects/seqfeat/BioSource.hpp>
#include <objects/seqfeat/Org_ref.hpp>
#include <objects/seqfeat/Cdregion.hpp>
#include <objects/seqfeat/SubSource.hpp>
#include <objects/seqfeat/OrgName.hpp>
#include <objects/seqfeat/OrgMod.hpp>

#include <objects/seqloc/Seq_loc.hpp>
#include <objects/seqloc/Textseq_id.hpp>
#include <objects/seqloc/Seq_interval.hpp>
#include <objects/seqloc/Seq_point.hpp>

#include <objects/seqres/Seq_graph.hpp>

#include <objects/submit/Seq_submit.hpp>
#include <objects/submit/Submit_block.hpp>

#include <objects/util/sequence.hpp>
#include <objects/util/feature.hpp>
#include <objects/util/validate.hpp>

#include <algorithm>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)
BEGIN_SCOPE(validator)
using namespace sequence;


// validator internal error types
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


// *********************** Debug only -- remove before release **********

template<class T>
void display_object(const T& obj)
{
    auto_ptr<CObjectOStream> os (CObjectOStream::Open(eSerial_AsnText, cout));
    *os << obj;
    cout << endl;
}


// *********************** CValidErrItem implementation ********************

CValidErrItem::CValidErrItem
(EDiagSev sev,
 unsigned int         ei,
 const string&        msg,
 const CSerialObject& obj)
  : m_Severity (sev),
    m_ErrIndex (ei),
    m_Message (msg),
    m_Object (&obj, obj.GetThisTypeInfo ())

{
}

CValidErrItem::~CValidErrItem()
{
}


EDiagSev CValidErrItem::GetSeverity() const
{
    return m_Severity;
}


const string& CValidErrItem::GetErrCode (void) const
{
    if (m_ErrIndex <= eErr_UNKNOWN) {
        return sm_Terse [m_ErrIndex];
    }
    return sm_Terse [eErr_UNKNOWN];
}

const string& CValidErrItem::GetMessage (void) const
{
    return m_Message;
}

const string& CValidErrItem::GetVerbose (void) const
{
    if (m_ErrIndex <= eErr_UNKNOWN) {
        return sm_Verbose [m_ErrIndex];
    }
    return sm_Verbose [eErr_UNKNOWN];
}

const CConstObjectInfo& CValidErrItem::GetObject (void) const
{
    return m_Object;
}

// ************************ CValidError_CI implementation **************
CValidError_CI::CValidError_CI(void) :
    m_ValidError(0),
    m_ErrCodeFilter(eErr_ALL), // eErr_UNKNOWN
    m_MinSeverity(eDiag_Info),
    m_MaxSeverity(eDiag_Critical)
{
}

CValidError_CI::CValidError_CI
(const CValidError& ve,
 string             errcode,
 EDiagSev           minsev,
 EDiagSev           maxsev) :
    m_ValidError(&ve),
    m_ErrIter(ve.m_ErrItems.begin()),
    m_MinSeverity(minsev),
    m_MaxSeverity(maxsev)
{
    if (errcode.empty()) {
        m_ErrCodeFilter = eErr_ALL;
    }

    for (unsigned int i = eErr_ALL; i < eErr_UNKNOWN; i++) {
        if (errcode == CValidErrItem::sm_Terse[i]) {
            m_ErrCodeFilter = i;
            return;
        }
    }
    m_ErrCodeFilter = eErr_ALL; // eErr_UNKNOWN
}


CValidError_CI::CValidError_CI(const CValidError_CI& iter) :
    m_ValidError(iter.m_ValidError),
    m_ErrIter(iter.m_ErrIter),
    m_ErrCodeFilter(iter.m_ErrCodeFilter),
    m_MinSeverity(iter.m_MinSeverity),
    m_MaxSeverity(iter.m_MaxSeverity)
{
}


CValidError_CI::~CValidError_CI(void)
{
}


CValidError_CI& CValidError_CI::operator= (const CValidError_CI& iter)
{
    if (this == &iter) {
        return *this;
    }

    m_ValidError = iter.m_ValidError;
    m_ErrIter = iter.m_ErrIter;
    m_ErrCodeFilter = iter.m_ErrCodeFilter;
    m_MinSeverity = iter.m_MinSeverity;
    m_MaxSeverity = iter.m_MaxSeverity;
    return *this;
}


CValidError_CI& CValidError_CI::operator++ (void)
{
    ++m_ErrIter;
    for (; m_ErrIter != m_ValidError->m_ErrItems.end(); ++m_ErrIter) {
        if ((m_ErrCodeFilter == eErr_ALL  ||
            (**m_ErrIter).GetErrCode() == CValidErrItem::sm_Terse[m_ErrCodeFilter])  &&
            (**m_ErrIter).GetSeverity() <= m_MaxSeverity  &&
            (**m_ErrIter).GetSeverity() >= m_MinSeverity) {
            break;
        }
    }
    return *this;
}


CValidError_CI::operator bool (void) const
{
    return m_ErrIter != m_ValidError->m_ErrItems.end();
}


const CValidErrItem& CValidError_CI::operator* (void) const
{
    return **m_ErrIter;
}


// ************************** CValidError_impl declaration **************
class CValidError_impl
{
public:
    //ctors
    CValidError_impl(CObjectManager&, TErrs&, unsigned int options = 0);
    virtual ~CValidError_impl(void);

    void Validate(const CSeq_entry& se, const CCit_sub* cs = 0);
    void Validate(const CSeq_submit& ss);

private:
    // Prohibit copy constructor & assignment operator
    CValidError_impl(const CValidError_impl&);
    CValidError_impl& operator= (const CValidError_impl&);

    CObjectManager* const m_ObjMgr;
    CRef<CScope> m_Scope;

    void SetScope(const CSeq_entry& se);

    // Top level Seq_entry, set by SetScope
    const CSeq_entry*   m_SeqEntry;
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

    // Validation methods
    void ValidateDescrChain(const CSeq_descr& descr);
    void ValidateSeqHist(const CSeq_hist& hist);
    void ValidateSeqGraph(const CSeq_graph& graph);
    void ValidateSeqAlign(const CSeq_align& align);
    void ValidateSeqFeat(const CSeq_feat& feat);
    void ValidateSeqAnnot(const CSeq_annot& annot);
    void ValidateSeqSet(const CBioseq_set& set);
    void ValidateBioseq(const CBioseq& seq);
    void ValidateSeqDesc(const CSeqdesc& desc);
    void ValidateSeqDescContext(const CBioseq& seq);
    void ValidateSeqIds(const CBioseq& seq);
    void ValidateSeqLoc(const CSeq_loc& loc, const CBioseq& seq,
        char* prefix);
    void ValidateInst(const CBioseq& seq);
    bool ValidateRepr(const CSeq_inst& inst, const CBioseq& seq);
    void ValidateSeqLen(const CBioseq& seq);
    void ValidateSeqParts(const CBioseq& seq);
    void ValidateProteinTitle(const CBioseq& seq);
    void ValidateBioseqContext(const CBioseq& seq);
    void ValidateMultiIntervalGene (const CBioseq& seq);
    void ValidateSeqFeatContext(const CBioseq& seq);
    void ValidateDupOrOverlapFeats(const CBioseq& seq);
    void ValidateCollidingGeneNames(const CBioseq& seq);
    void ValidateRawConst(const CBioseq& seq);
    void ValidateSegRef(const CBioseq& seq);
    void ValidateDelta(const CBioseq& seq);
    void ValidateBioSource(const CBioSource& bsrc, const CSerialObject& obj);
    void ValidatePubdesc(const CPubdesc& pub, const CSerialObject& obj);
    void ValidateDbxref(const CDbtag& xref, const CSerialObject& obj);
    void ValidateSourceQualTags(void);

    bool IsCountryValid(const string &str);

    typedef const CSeq_feat& TFeat;
    typedef const CBioseq& TBioseq;
    typedef const CBioseq_set& TSet;
    typedef const CSeqdesc& TDesc;

    // Posts errors.
    void ValidErr(EDiagSev sv, EErrType et, const string& msg,
        const CSerialObject& obj);
    void ValidErr(EDiagSev sv, EErrType et, const string& msg, TDesc ds);
    void ValidErr(EDiagSev sv, EErrType et, const string& msg, TFeat ft);
    void ValidErr(EDiagSev sv, EErrType et, const string& msg, TBioseq sq);
    void ValidErr(EDiagSev sv, EErrType et, const string& msg, TBioseq sq,
        TDesc ds);
    void ValidErr(EDiagSev sv, EErrType et, const string& msg, TSet set);
    void ValidErr(EDiagSev sv, EErrType et, const string& msg, TSet set, 
        TDesc ds);

    // legal dbxref database strings
    static const char * legalDbXrefs [];
    static const char * legalRefSeqDbXrefs [];

    // legal country strings
    static const string countrycodes [];
};

//********************** CValidError_impl implementation ******************

CValidError_impl::CValidError_impl
(CObjectManager&     objmgr,
 TErrs&              errs,
 unsigned int        options)
    : m_ObjMgr(&objmgr),
      m_Scope(0),
      m_SeqEntry(0),
      m_Errors(&errs),
      m_NonASCII((options & CValidError::eVal_non_ascii) != 0),
      m_SuppressContext((options & CValidError::eVal_no_context) != 0),
      m_ValidateAlignments((options & CValidError::eVal_val_align) != 0),
      m_ValidateExons((options & CValidError::eVal_val_exons) != 0),
      m_SpliceErr((options & CValidError::eVal_splice_err) != 0),
      m_OvlPepErr((options & CValidError::eVal_ovl_pep_err) != 0),
      m_RequireTaxonID((options & CValidError::eVal_need_taxid) != 0),
      m_RequireISOJTA((options & CValidError::eVal_need_isojta) != 0),
      m_NoPubs(false),
      m_NoBioSource(false),
      m_IsGPS(false),
      m_IsGED(false),
      m_IsPDB(false),
      m_IsTPA(false),
      m_IsPatent(false),
      m_IsRefSeq(false),
      m_IsNC(false),
      m_IsNG(false),
      m_IsNM(false),
      m_IsNP(false),
      m_IsNR(false),
      m_IsNS(false),
      m_IsNT(false),
      m_IsNW(false),
      m_IsXR(false),
      m_IsGI(false)
{
}


CValidError_impl::~CValidError_impl()
{
}


inline
const CSeq_id& s_GetId(const CBioseq& seq)
{
    if (!seq.GetId().empty()) {
        return **seq.GetId().begin();
    } else {
        THROW1_TRACE(runtime_error, "Bioseq has no Seq-id");
    }
}


inline
const CTextseq_id& s_GetTextseq_id(const CSeq_id& id)
{
    switch (id.Which()) {
    case CSeq_id::e_Genbank:
        return id.GetGenbank();
    case CSeq_id::e_Embl:
        return id.GetEmbl();
    case CSeq_id::e_Pir:
        return id.GetPir();
    case CSeq_id::e_Swissprot:
        return id.GetSwissprot();
    case CSeq_id::e_Other:
        return id.GetOther();
    case CSeq_id::e_Ddbj:
        return id.GetDdbj();
    case CSeq_id::e_Prf:
        return id.GetPrf();
    case CSeq_id::e_Tpg:
        return id.GetTpg();
    case CSeq_id::e_Tpe:
        return id.GetTpe();
    case CSeq_id::e_Tpd:
        return id.GetTpd();
    default:
        THROW1_TRACE(runtime_error, "Type not handled");
    }
}


inline
const string& s_GetAccession(const CSeq_id& id)
{
    const CTextseq_id& tid = s_GetTextseq_id(id);
    if (tid.IsSetAccession()) {
        return tid.GetAccession();
    } else {
        THROW1_TRACE(runtime_error, "Accession not set");
    }
}


inline
const string& s_GetName(const CSeq_id& id)
{
    const CTextseq_id& tid = s_GetTextseq_id(id);
    if (tid.IsSetName()) {
        return tid.GetName();
    } else {
        THROW1_TRACE(runtime_error, "Name not set");
    }
}


inline
static bool s_isNa(const CSeq_inst& inst)
{
    const CSeq_inst::EMol& mol = inst.GetMol();
    if (mol == CSeq_inst::eMol_dna  ||  mol == CSeq_inst::eMol_rna  ||
        mol == CSeq_inst::eMol_na)
    {
        return true;
    } else {
        return false;
    }
}


inline
static bool s_isNa(const CBioseq& seq)
{
    return s_isNa(seq.GetInst());
}


inline
static bool s_isAa(const CSeq_inst& inst)
{
    return inst.GetMol() == CSeq_inst::eMol_aa;
}


inline
static bool s_isAa(const CBioseq& seq)
{
    return s_isAa(seq.GetInst());
}


inline
static TSeqPos s_GetDataLen(const CSeq_inst& inst)
{
    if (!inst.IsSetSeq_data()) {
        return 0;
    }

    const CSeq_data& seqdata = inst.GetSeq_data();
    switch (seqdata.Which()) {
    case CSeq_data::e_not_set:
        return 0;
    case CSeq_data::e_Iupacna:
        return seqdata.GetIupacna().Get().size();
    case CSeq_data::e_Iupacaa:
        return seqdata.GetIupacaa().Get().size();
    case CSeq_data::e_Ncbi2na:
        return seqdata.GetNcbi2na().Get().size();
    case CSeq_data::e_Ncbi4na:
        return seqdata.GetNcbi4na().Get().size();
    case CSeq_data::e_Ncbi8na:
        return seqdata.GetNcbi8na().Get().size();
    case CSeq_data::e_Ncbipna:
        return seqdata.GetNcbipna().Get().size();
    case CSeq_data::e_Ncbi8aa:
        return seqdata.GetNcbi8aa().Get().size();
    case CSeq_data::e_Ncbieaa:
        return seqdata.GetNcbieaa().Get().size();
    case CSeq_data::e_Ncbipaa:
        return seqdata.GetNcbipaa().Get().size();
    case CSeq_data::e_Ncbistdaa:
        return seqdata.GetNcbistdaa().Get().size();
    default:
        return 0;
    }
}


inline
static bool s_IsGEDSeqEmbedded(const CSeq_entry& se)
{
    for (CTypeConstIterator<CBioseq> seq(ConstBegin(se)); seq; ++seq) {
        iterate (CBioseq::TId, id, seq->GetId()) {
            switch ((**id).Which()) {
            case CSeq_id::e_Genbank:
            case CSeq_id::e_Embl:
            case CSeq_id::e_Ddbj:
            case CSeq_id::e_Tpg:
            case CSeq_id::e_Tpe:
            case CSeq_id::e_Tpd:
                return true;
            default:
                break;
            }
        }
    }
    return false;
}


inline
static bool s_IsIdIn(const CSeq_id& id, const CBioseq& seq)
{
    iterate (CBioseq::TId, it, seq.GetId()) {
        if (id.Match(**it)) {
            return true;
        }
    }
    return false;
}


// TO BE REPLACED
inline
static const CSeq_feat* s_GetCDSForProduct
(const CBioseq& seq,
 CScope* const  scope)
{
     // Get id for CBioseq
     const CSeq_id* id = seq.GetFirstId();
 
     // Return null poiner if no id
     if (!id) {
         return 0;
     }
 
     CSeq_loc loc;
     loc.SetWhole(*id);
     CFeat_CI fi(*scope, loc, CSeqFeatData::e_Cdregion);
     return &(*fi);
}


// TO BE REPLACED
// Check if CdRegion required but not found
static bool s_CdError(const CBioseq& seq, CScope* scope)
{
    const CSeq_id* sid = seq.GetFirstId();
     if (s_isAa(seq)  &&  sid) {
         const CSeq_entry* parent = seq.GetParentEntry();
         const CSeq_entry* grand_parent;
         if (parent) {
             grand_parent = parent->GetParentEntry();
         }
         if (grand_parent  &&  grand_parent->IsSet()) {
             const CBioseq_set& set = grand_parent->GetSet();
             if (set.IsSetClass()  &&
                 set.GetClass() == CBioseq_set::eClass_nuc_prot) {
                 CTypeConstIterator<CSeq_feat> ft(ConstBegin(set));
                 bool isFoundCd = false;
                 for (; ft; ++ft) {
                     if (ft->IsSetProduct()  &&
                         ft->GetData().Which() == CSeqFeatData::e_Cdregion) {
                         const CSeq_loc& loc = ft->GetProduct();
                         const CSeq_id& lid = GetId(loc, scope);
                         if (IsSameBioseq(*sid, lid, scope)) {
                             isFoundCd = true;
                             break;
                         }
                     }
                 }
                 return !isFoundCd;
             }
         }
     }
     return false;
}


inline
static bool s_SameAnnotDesc(const CAnnot_descr& d1, const CAnnot_descr& d2)
{
    const list< CRef<CAnnotdesc> >& ad1 = d1.Get();
    const list< CRef<CAnnotdesc> >& ad2 = d2.Get();

    if (ad1.size() != ad2.size()) {
        return false;
    }

    list< CRef<CAnnotdesc> >::const_iterator i1 = ad1.begin();
    list< CRef<CAnnotdesc> >::const_iterator i2 = ad2.begin();
    for (; i1 != ad1.end(); ++i1, ++i2) {
        if ((**i1).Which() != (**i2).Which()) {
            return false;
        }
        switch ((**i1).Which()) {
        case CAnnotdesc::e_Name:
            if ((**i1).GetName() != (**i2).GetName()) {
                return false;
            }
            break;
        case CAnnotdesc::e_Title:
            if ((**i1).GetTitle() != (**i2).GetTitle()) {
                return false;
            }
            break;
        default:
            return false;
        }
    }
    return true;
}


inline
static bool s_IsSameComment(const CSeq_feat& f1, const CSeq_feat& f2)
{
    if (f1.IsSetComment()  &&  !f2.IsSetComment()) {
        return false;
    } else if (!f1.IsSetComment()  &&  f2.IsSetComment()) {
        return false;
    } else if (!f1.IsSetComment()  && !f2.IsSetComment()) {
        return true;
    } else {
        return f1.GetComment() == f2.GetComment();
    }
}


inline
static bool s_IsDifferentFrame(const CSeq_feat& f1, const CSeq_feat& f2)
{
    if (!f1.GetData().IsCdregion()  ||  !f2.GetData().IsCdregion()) {
        return false;
    }

    if (!f1.GetData().GetCdregion().IsSetFrame()  ||
        !f2.GetData().GetCdregion().IsSetFrame()) {
            return false;
    }

    return f1.GetData().GetCdregion().GetFrame() !=
        f2.GetData().GetCdregion().GetFrame();
}


inline
static bool s_IsWarn(CSeqFeatData::ESubtype sub_type)
{
    switch (sub_type) {
    case CSeqFeatData::eSubtype_pub:
    case CSeqFeatData::eSubtype_region:
    case CSeqFeatData::eSubtype_misc_feature:
    case CSeqFeatData::eSubtype_STS:
    case CSeqFeatData::eSubtype_variation:
        return true;
    default:
        return false;
    }
}


// Finds the first COrg_ref up the CSeq_entry tree from seq and
// return taxname, if found
inline
static string s_GetTaxname(const CBioseq& seq)
{
    const CSeq_entry* se = seq.GetParentEntry();
    for (; se; se->GetParentEntry()) {
        CTypeConstIterator<COrg_ref> ref(ConstBegin(*se));
        if (ref) {
            if (ref->IsSetTaxname()) {
                return ref->GetTaxname();
            }
            return "";
        }
    }
    return "";
}


inline
static bool s_IsDifferentDbxref(const CSeq_feat& f1, const CSeq_feat& f2)
{
    if (!f1.IsSetDbxref()  ||  !f2.IsSetDbxref()) {
        return false;
    }

    const list< CRef<CDbtag> >& list1 = f1.GetDbxref();
    const list< CRef<CDbtag> >& list2 = f2.GetDbxref();
   
    if (list1.empty()  ||  list2.empty()) {
        return false;
    } else if (list1.size() != list2.size()) {
        return true;
    }

    list< CRef<CDbtag> >::const_iterator it1 = list1.begin();
    list< CRef<CDbtag> >::const_iterator it2 = list2.begin();
    for (; it1 != list1.end(); ++it1, ++it2) {
        if ((**it1).GetDb() != (**it2).GetDb()) {
            return true;
        }
        string str1 =
            (**it1).GetTag().IsStr() ? (**it1).GetTag().GetStr() : "";
        string str2 =
            (**it2).GetTag().IsStr() ? (**it2).GetTag().GetStr() : "";
        if (str1.empty()  &&  str2.empty()) {
            if (!(**it1).GetTag().IsId()  &&  !(**it2).GetTag().IsId()) {
                continue;
            } else if ((**it1).GetTag().IsId()  &&  (**it2).GetTag().IsId()) {
                if ((**it1).GetTag().GetId() != (**it2).GetTag().GetId()) {
                    return true;
                }
            } else {
                return true;
            }
        }
    }
    return false;
}


// Returns true if seq derived from translation ending in "*" or
// seq is 3' partial (i.e. the right of the sequence is incomplete)
inline
static bool s_SuppressTrailingXMsg(const CBioseq& seq, CScope* const scope)
{

    // Look for the Cdregion feature used to create this aa product
    // Use the Cdregion to translate the associated na sequence
    // and check if translation has a '*' at the end. If it does.
    // message about 'X' at the end of this aa product sequence is suppressed
    const CSeq_feat* fi = s_GetCDSForProduct(seq, scope);
    if (fi) {
    
        // Get CCdregion 
        CTypeConstIterator<CCdregion> cdr(ConstBegin(*fi));
        
        // Get location on source sequence
        const CSeq_loc& loc = (*fi).GetLocation();

        // Get CSeq_id of source sequence
        const CSeq_id& id = GetId(loc);

        // Get CBioseq_Handle for source sequence
        CBioseq_Handle hnd = scope->GetBioseqHandle(id);

        // Translate na CSeq_data
        string prot;        
        CCdregion_translate::TranslateCdregion(prot, hnd, loc, *cdr);
        
        if (!NStr::CompareCase(prot.substr(prot.size()-1, 1), "*")) {
            return true;
        }
        return false;
    }

    // Get CMolInfo for seq and determine if completeness is
    // "eCompleteness_no_right or eCompleteness_no_ends. If so
    // suppress message about "X" at end of aa sequence is suppressed
    CTypeConstIterator<CMolInfo> mi = ConstBegin(seq);
    if (mi  &&  mi->IsSetCompleteness()) {
        if (mi->GetCompleteness() == CMolInfo::eCompleteness_no_right  ||
          mi->GetCompleteness() == CMolInfo::eCompleteness_no_ends) {
            return true;
        }
    }
    return false;
}


// TO BE REPLACED
// Called by s_GetGeneLabel
inline
static const CGene_ref* s_GetGeneRef(const CSeq_feat& cds)
{
    CTypeConstIterator<CGene_ref> grf = ConstBegin(cds);
    if (grf) {
        return &(*grf);
    } else {
        return 0;
    }
}


// TO BE REPLACED
// Get Seq-feat with SeqFeatData of type Gene-ref that best
// contains location of cds -- used to create a label
// for error message
inline
static const CSeq_feat* s_GetContainingGene
(const CSeq_feat* cds,
CScope*           scope)
{
    if (!cds) {
        return 0;
    }

    // Iterate through Seq-feats of type Gene-ref that overlap cds location
    // and pick the best one -- the one containing cds location
    // that least differs from cds location.
    CFeat_CI fi(*scope, cds->GetLocation(), CSeqFeatData::e_Gene);
    TSeqPos best_diff = kMax_UInt;
    TSeqPos diff;
    const CSeq_feat* best = 0;
    for (; fi; ++fi) {
        try {
            TSeqPos cds_left = GetStart(cds->GetLocation(), scope);
            TSeqPos fi_left = GetStart(fi->GetLocation(), scope);
            if (fi_left > cds_left) {
                continue;
            }
            TSeqPos cds_right = GetLength(cds->GetLocation(), scope)
                + cds_left - 1;
            TSeqPos fi_right = GetLength(fi->GetLocation(), scope) +
                fi_left - 1;
            if (fi_right < cds_right) {
                continue;
            }
            diff = cds_left - fi_left + fi_right - cds_right;
            if (diff < best_diff) {
                best_diff = diff;
                best = &(*fi);
            }
        } catch(const CNoLength&) {}
    }
    return best;
}


// TO BE REPLACED
// Gets a label for the gene sequence that produces the sequence seq.
inline
static void s_GetGeneLabel(const CBioseq& seq, string* lbl, CScope* scope)
{
    if (!lbl) {
        return;
    }

    // Get the cdregion that created this product
    const CSeq_feat* cds = s_GetCDSForProduct(seq, scope);
    const CGene_ref* grf;
    const CSeq_feat* gene;
    grf = cds ? s_GetGeneRef(*cds) : 0;
    if (!grf  &&  cds) {
        gene = s_GetContainingGene(cds, scope);
        if (gene) {
            if (!(grf = s_GetGeneRef(*gene))) {
                (*lbl) = "gene?";
                return;
            }
        }
    }
    if (grf->IsSetLocus()) {
        (*lbl) = grf->GetLocus();
    } else if (grf->IsSetDesc()) {
        (*lbl) = grf->GetDesc();
    } else if (grf->IsSetSyn()  &&  !grf->GetSyn().empty()) {
        (*lbl) = *grf->GetSyn().begin();
    }
    if (lbl->empty()) {
        (*lbl) = "gene?";
    }
    return;
}


// Gets the label for a sequence -- used to get the label for a protein
inline
static void s_GetSeqLabel
(const CBioseq& seq,
 string*        lbl,
 CScope*        scope,
 char*          default_label = "prot?")
{
    if (!lbl) {
        return;
    }
    // Get id for CBioseq
    const CSeq_id* id = (*seq.GetId().begin()).GetPointer();

    // Return if no id
    if (!id) {
        (*lbl) = default_label;
        return;
    }

    // Get the protein label.
    CBioseq_Handle hnd = scope->GetBioseqHandle(*id);
    (*lbl) = GetTitle(hnd);
    if (lbl->empty()) {
        (*lbl) = default_label;
    }
    return;
}


inline
static bool s_GetLocFromSeq(const CBioseq& seq, CSeq_loc* loc)
{
    if (!seq.GetInst().IsSetExt()  ||  !seq.GetInst().GetExt().IsSeg()) {
        return false;
    }

    CSeq_loc_mix& mix = loc->SetMix();
    iterate (list< CRef<CSeq_loc> >, it,
        seq.GetInst().GetExt().GetSeg().Get()) {
        mix.Set().push_back(*it);
    }
    return true;
}


// If seq has extension data, wraps it in a Seq-loc of type mix.
enum ESeqlocPartial {
    eSeqlocPartial_Complete   =   0,
    eSeqlocPartial_Start      =   1,
    eSeqlocPartial_Stop       =   2,
    eSeqlocPartial_Internal   =   4,
    eSeqlocPartial_Other      =   8,
    eSeqlocPartial_Nostart    =  16,
    eSeqlocPartial_Nostop     =  32,
    eSeqlocPartial_Nointernal =  64,
    eSeqlocPartial_Limwrong   = 128,
    eSeqlocPartial_Haderror   = 256
};


static unsigned int s_GetSeqlocPartialInfo(const CSeq_loc& loc, CScope* scope)
{
    unsigned int retval = 0;
    if (!scope) {
        return retval;
    }

    // Find first and last Seq-loc
    const CSeq_loc *first = 0, *last = 0;
    CTypeConstIterator<CSeq_loc> i1(ConstBegin(loc));
    for (++i1; i1; ++i1) {
        if (!first) {
            first = &(*i1);
        }
        last = &(*i1);
    }
    if (!first) {
        return retval;
    }

    CTypeConstIterator<CSeq_loc> i2(ConstBegin(loc));
    for (++i2; i2; ++i2) {
        switch ((*i2).Which()) {
        case CSeq_loc::e_Null:
            if (&(*i2) == first) {
                retval |= eSeqlocPartial_Start;
            } else if (&(*i2) == last) {
                retval |= eSeqlocPartial_Stop;
            } else {
                retval |= eSeqlocPartial_Internal;
            }
            break;
        case CSeq_loc::e_Int:
            if ((*i2).GetInt().IsSetFuzz_from()) {
                const CInt_fuzz& fuzz = (*i2).GetInt().GetFuzz_from();
                if (fuzz.Which() == CInt_fuzz::e_Lim) {
                    CInt_fuzz::ELim lim = fuzz.GetLim();
                    if (lim == CInt_fuzz::eLim_gt) {
                        retval |= eSeqlocPartial_Limwrong;
                    } else if (lim == CInt_fuzz::eLim_lt  ||
                        lim == CInt_fuzz::eLim_unk) {
                        if ((*i2).GetInt().IsSetStrand()  &&
                            (*i2).GetInt().GetStrand() == eNa_strand_minus) {
                            if (&(*i2) == last) {
                                retval |= eSeqlocPartial_Stop;
                            } else {
                                retval |= eSeqlocPartial_Internal;
                            }
                            if ((*i2).GetInt().GetFrom() != 0) {
                                if (&(*i2) == last) {
                                    retval |= eSeqlocPartial_Nostop;
                                } else {
                                    retval |= eSeqlocPartial_Nointernal;
                                }
                            }
                        } else {
                            if (&(*i2) == first) {
                                retval |= eSeqlocPartial_Start;
                            } else {
                                retval |= eSeqlocPartial_Internal;
                            }
                            if ((*i2).GetInt().GetFrom() != 0) {
                                if (&(*i2) == first) {
                                    retval |= eSeqlocPartial_Nostart;
                                } else {
                                    retval |= eSeqlocPartial_Nointernal;
                                }
                            }
                        }
                    }
                }
            }

            if ((*i2).GetInt().IsSetFuzz_to()) {
                const CInt_fuzz& fuzz = (*i2).GetInt().GetFuzz_to();
                CInt_fuzz::ELim lim = fuzz.GetLim();
                if (lim == CInt_fuzz::eLim_lt) {
                    retval |= eSeqlocPartial_Limwrong;
                } else if (lim == CInt_fuzz::eLim_gt  ||
                    lim == CInt_fuzz::eLim_unk) {
                    CBioseq_Handle hnd =
                        scope->GetBioseqHandle((*i2).GetInt().GetId());
                    CBioseq_Handle::TBioseqCore bc = hnd.GetBioseqCore();
                    bool miss_end = false;
                    const CSeq_interval& itv = (*i2).GetInt();
                    if (itv.GetTo() != bc->GetInst().GetLength() - 1) {
                        miss_end = true;
                    }
                    if (itv.IsSetStrand()  &&
                        itv.GetStrand() == eNa_strand_minus) {
                        if (&(*i2) == first) {
                            retval |= eSeqlocPartial_Start;
                        } else {
                            retval |= eSeqlocPartial_Internal;
                        }
                        if (miss_end) {
                            if (&(*i2) == last) {
                                retval |= eSeqlocPartial_Nostart;
                            } else {
                                retval |= eSeqlocPartial_Nointernal;
                            }
                        }
                    } else {
                        if (&(*i2) == last) {
                            retval |= eSeqlocPartial_Stop;
                        } else {
                            retval |= eSeqlocPartial_Internal;
                        }
                        if (miss_end) {
                            if (&(*i2) == last) {
                                retval |= eSeqlocPartial_Nostop;
                            } else {
                                retval |= eSeqlocPartial_Nointernal;
                            }
                        }
                    }
                }
            }
            break;
        case CSeq_loc::e_Pnt:
            if ((*i2).GetPnt().IsSetFuzz()) {
                const CInt_fuzz& fuzz = (*i2).GetPnt().GetFuzz();
                if (fuzz.Which() == CInt_fuzz::e_Lim) {
                    CInt_fuzz::ELim lim = fuzz.GetLim();
                    if (lim == CInt_fuzz::eLim_gt  ||
                        lim == CInt_fuzz::eLim_lt  ||
                        lim == CInt_fuzz::eLim_unk) {
                        if (&(*i2) == first) {
                            retval |= eSeqlocPartial_Start;
                        } else if (&(*i2) == last) {
                            retval |= eSeqlocPartial_Stop;
                        } else {
                            retval |= eSeqlocPartial_Internal;
                        }
                    }
                }
            }
            break;
        case CSeq_loc::e_Packed_pnt:
            if ((*i2).GetPacked_pnt().IsSetFuzz()) {
                const CInt_fuzz& fuzz = (*i2).GetPacked_pnt().GetFuzz();
                if (fuzz.Which() == CInt_fuzz::e_Lim) {
                    CInt_fuzz::ELim lim = fuzz.GetLim();
                    if (lim == CInt_fuzz::eLim_gt  ||
                        lim == CInt_fuzz::eLim_lt  ||
                        lim == CInt_fuzz::eLim_unk) {
                        if (&(*i2) == first) {
                            retval |= eSeqlocPartial_Start;
                        } else if (&(*i2) == last) {
                            retval |= eSeqlocPartial_Stop;
                        } else {
                            retval |= eSeqlocPartial_Internal;
                        }
                    }
                }
            }
            break;
        case CSeq_loc::e_Whole:
        {
            // Get the Bioseq referred to by Whole
            CBioseq_Handle hnd = scope->GetBioseqHandle((*i2).GetWhole());
            CBioseq_Handle::TBioseqCore bc = hnd.GetBioseqCore();
            if (!(*bc).IsSetDescr()) {
                // If no CSeq_descr, nothing can be done
                break;
            }
            // First try to loop through CMolInfo
            CTypeConstIterator<CMolInfo> mi(ConstBegin((*bc).GetDescr()));
            bool found_molinfo = mi ? true : false;
            for (; mi; ++mi) {
                if (!(*mi).IsSetCompleteness()) {
                    continue;
                }
                switch ((*mi).GetCompleteness()) {
                case CMolInfo::eCompleteness_no_left:
                    if (&(*i2) == first) {
                        retval |= eSeqlocPartial_Start;
                    } else {
                        retval |= eSeqlocPartial_Internal;
                    }
                    break;
                case CMolInfo::eCompleteness_no_right:
                    if (&(*i2) == last) {
                        retval |= eSeqlocPartial_Stop;
                    } else {
                        retval |= eSeqlocPartial_Internal;
                    }
                    break;
                case CMolInfo::eCompleteness_partial:
                    retval |= eSeqlocPartial_Other;
                    break;
                case CMolInfo::eCompleteness_no_ends:
                    retval |= eSeqlocPartial_Start;
                    retval |= eSeqlocPartial_Stop;
                    break;
                default:
                    break;
                }
            }
            if (found_molinfo) {
                break;
            }
            break;
        }
        default:
            break;
        }
    }
    return retval;
}


// TO BE REPLACED
static bool s_SeqLocMixedStrands(const CBioseq& seq, const CSeq_loc& loc)
{
    return true;
}



// Returns true if an object of type T is embedded in in object of type K,
// else false
template <class T, class K>
static bool s_AnyObj(const K& obj)
{
    CTypeConstIterator<T> i(ConstBegin(obj));
    return i ? true : false;
}


// Returns true if first CBioseq in se has CSeq_id of type C.
template <CSeq_id::E_Choice C>
static bool s_IsType(const CSeq_entry& se)
{
    // Get first CBioseq
    CTypeConstIterator<CBioseq> seq(ConstBegin(se));
    if (seq) {
    list< CRef<CSeq_id> >::const_iterator id = seq->GetId().begin();
        // Loop thru CSeq_ids of seq & return true if C type id found
        for (;id != seq->GetId().end(); ++id)
        {
            if ((*id)->Which() == C) {
                return true;
            }
        }
    }
    return false;
}


inline
static bool s_NotPeptideException(const CSeq_feat& ft)
{
    if (!ft.IsSetExcept_text()) {
        return true;
    }

    if (ft.GetExcept_text().find("alternative processing") != string::npos) {
        return false;
    }

    if (ft.GetExcept_text().find("alternate processing") != string::npos) {
        return false;
    }

    return true;
}


void CValidError_impl::ValidErr
(EDiagSev sv,
 EErrType et,
 const string&  msg,
 const CSerialObject& obj)
{
    const CSeqdesc* desc = dynamic_cast < const CSeqdesc* > (&obj);
    if (desc != 0) {
        ValidErr (sv, et, msg, *desc);
        return;
    }
    const CSeq_feat* feat = dynamic_cast < const CSeq_feat* > (&obj);
    if (feat != 0) {
        ValidErr (sv, et, msg, *feat);
        return;
    }
    const CBioseq* seq = dynamic_cast < const CBioseq* > (&obj);
    if (seq != 0) {
        ValidErr (sv, et, msg, *seq);
        return;
    }
    const CBioseq_set* set = dynamic_cast < const CBioseq_set* > (&obj);
    if (set != 0) {
        ValidErr (sv, et, msg, *set);
        return;
    }
}


void CValidError_impl::ValidErr
(EDiagSev sv,
 EErrType et,
 const string&   message,
 TDesc    ds)
{
    // Append Descriptor label
    string msg(message + " DESCRIPTOR: ");
    ds.GetLabel (&msg, CSeqdesc::eBoth);

    m_Errors->push_back (new CValidErrItem (sv, et, msg, ds));
}


void CValidError_impl::ValidErr
(EDiagSev sv,
 EErrType et,
 const string&   message,
 TFeat    ft)
{
    // Add feature part of label
    string msg(message + " FEATURE: ");
    feature::GetLabel(ft, &msg, feature::eBoth, m_Scope.GetPointer());

    // Add feature location part of label
    string loc_label;
    if (m_SuppressContext) {
        CSeq_loc loc;
        SerialAssign(loc, ft.GetLocation());
        ChangeSeqLocId(&loc, false, m_Scope.GetPointer());
        loc.GetLabel(&loc_label);
    } else {
        ft.GetLocation().GetLabel(&loc_label);
    }
    if (loc_label.size() > 800) {
        loc_label = loc_label.substr(0, 797) + "...";
    }
    if (!loc_label.empty()) {
        loc_label = string("[") + loc_label + "]";
        msg += loc_label;
    }

    // Append label for bioseq of feature location
    if (!m_SuppressContext) {
        try {
            const CSeq_id& id = GetId(ft.GetLocation(), m_Scope.GetPointer());
            CBioseq_Handle hnd = m_Scope.GetPointer()->GetBioseqHandle(id);
            CBioseq_Handle::TBioseqCore bc = hnd.GetBioseqCore();
            msg += "[";
            bc->GetLabel(&msg, CBioseq::eBoth);
            msg += "]";
        } catch (...){};
    }

    // Append label for product of feature
    loc_label.erase();
    if (ft.IsSetProduct()) {
        if (m_SuppressContext) {
            CSeq_loc loc;
            SerialAssign(loc, ft.GetProduct());
            ChangeSeqLocId(&loc, false, m_Scope.GetPointer());
            loc.GetLabel(&loc_label);
        } else {
            ft.GetProduct().GetLabel(&loc_label);
        }
        if (loc_label.size() > 800) {
            loc_label = loc_label.substr(0, 797) + "...";
        }
        if (!loc_label.empty()) {
            loc_label = string("[") + loc_label + "]";
            msg += loc_label;
        }
    }
    m_Errors->push_back (new CValidErrItem (sv, et, msg, ft));
}


void CValidError_impl::ValidErr
(EDiagSev sv,
 EErrType et,
 const string&   message,
 TBioseq  sq)
{
    // Append bioseq label
    string msg(message + " BIOSEQ: ");
    if (m_SuppressContext) {
        sq.GetLabel(&msg, CBioseq::eContent, true);
    } else {
        sq.GetLabel(&msg, CBioseq::eBoth, false);
    }
    m_Errors->push_back (new CValidErrItem (sv, et, msg, sq));
}


void CValidError_impl::ValidErr
(EDiagSev sv,
 EErrType et,
 const string&   message,
 TBioseq  sq,
 TDesc    ds)
{
    // Append Descriptor label
    string msg(message + " DESCRIPTOR: ");
    ds.GetLabel(&msg, CSeqdesc::eBoth);
    ValidErr(sv, et, msg, sq);
}


void CValidError_impl::ValidErr
(EDiagSev      sv,
 EErrType      et,
 const string& message,
 TSet          set)
{
    // Append Bioseq_set label
    string msg(message + " BIOSEQ-SET: ");
    if (m_SuppressContext) {
        set.GetLabel(&msg, CBioseq_set::eContent);
    } else {
        set.GetLabel(&msg, CBioseq_set::eBoth);
    }
    m_Errors->push_back (new CValidErrItem (sv, et, msg, set));
}


void CValidError_impl::ValidErr
(EDiagSev        sv,
 EErrType        et,
 const string&   message,
 TSet            set,
 TDesc           ds)
{
    // Append Descriptor label
    string msg(message + " DESCRIPTOR: ");
    ds.GetLabel(&msg, CSeqdesc::eBoth);
    ValidErr(sv, et, msg, set);
}


void CValidError_impl::SetScope(const CSeq_entry& se)
{
    m_SeqEntry = &se;
    m_Scope.Reset(new CScope(*m_ObjMgr));
    m_Scope->AddTopLevelSeqEntry(*const_cast<CSeq_entry*>(&se));
    m_Scope->AddDefaults();
}


void CValidError_impl::ValidateDescrChain(const CSeq_descr& descr)
{
    int numSources = 0;
    int numTitles = 0;
    const CSeqdesc * lastsource = 0;
    const CSeqdesc * lasttitle = 0;

    for (CTypeConstIterator <CSeqdesc> dt (descr); dt; ++dt) {
        switch (dt->Which ()) {
            case CSeqdesc::e_Title:
                numTitles++;
                lasttitle = &(*dt);
                break;
            case CSeqdesc::e_Source:
                numSources++;
                lastsource = &(*dt);
                break;
            default:
                break;
        }
    }
    if (numSources > 1) {
        ValidErr(eDiag_Error, eErr_SEQ_DESCR_MultipleBioSources,
            "Multiple BioSource blocks", *lastsource);
    }
    if (numTitles > 1) {
        ValidErr(eDiag_Error, eErr_SEQ_DESCR_MultipleTitles,
            "Multiple Title blocks", *lasttitle);
    }
}


void CValidError_impl::ValidateSeqDescContext(const CBioseq& seq)
{
    bool prf_found = false;
    const CDate* create_date_prev = 0;

    for (CTypeConstIterator<CSeqdesc> dt(ConstBegin(seq)); dt; ++dt) {
        switch (dt->Which()) {
        case CSeqdesc::e_Prf:
            if (prf_found) {
                ValidErr(eDiag_Error, eErr_SEQ_DESCR_Inconsistent,
                    "Multiple PRF blocks", *dt);
            } else {
                prf_found = true;
            }
            break;
        case CSeqdesc::e_Create_date:
            if (create_date_prev) {
                if (create_date_prev->Compare(dt->GetCreate_date()) !=
                    CDate::eCompare_same) {
                    string str_date_prev;
                    string str_date;
                    create_date_prev->GetDate(&str_date_prev);
                    dt->GetCreate_date().GetDate(&str_date);
                    ValidErr(eDiag_Warning, eErr_SEQ_DESCR_Inconsistent,
                        "Inconsistent create_dates " + str_date +
                        " and " + str_date_prev, *dt);
                }
            } else {
                create_date_prev = &dt->GetCreate_date();
            }

            break;
        default:
            break;
        }
    }
}


void CValidError_impl::ValidateSeqIds
(const CBioseq& seq)
{

    // Ensure that CBioseq has at least one CSeq_id
    if (!seq.GetId().size()) {
        ValidErr(eDiag_Critical, eErr_SEQ_INST_NoIdOnBioseq,
                 "No ids on a Bioseq", seq);
    }

    // Loop thru CSeq_ids for this CBioseq. Determine if seq has
    // gi, NT, or NC. Check that the same CSeq_id not included more
    // than once.
    iterate (CBioseq::TId, i, seq.GetId()) {

        // Check that no two CSeq_ids for same CBioseq are same type
        list< CRef < CSeq_id > >::const_iterator j;
        for (j = i, ++j; j != seq.GetId().end(); ++j) {
            if ((**i).Compare(**j) != CSeq_id::e_DIFF) {
                CNcbiOstrstream os;
                os << "Conflicting ids on a Bioseq: ("
                           << (**i).DumpAsFasta() << " - "
                           << (**j).DumpAsFasta() << ")";
                ValidErr(eDiag_Error, eErr_SEQ_INST_ConflictingIdsOnBioseq,
                         os.str(), seq);
            }
        }
    }


    // Loop thru CSeq_ids to check formatting
    unsigned int gi_count = 0;
    unsigned int accn_count = 0;
    iterate (CBioseq::TId, k, seq.GetId()) {
        switch ((**k).Which()) {
        case CSeq_id::e_Tpg:
        case CSeq_id::e_Tpe:
        case CSeq_id::e_Tpd:
        case CSeq_id::e_Genbank:
        case CSeq_id::e_Embl:
        case CSeq_id::e_Ddbj:
            try {
                const string& acc = s_GetAccession(**k);
                unsigned int numDigits = 0;
                unsigned int numLetters = 0;
                bool letterAfterDigit = false;
                bool badIDchars = false;
                iterate (string, s, acc) {
                    if (isupper(*s)) {
                        numLetters++;
                        if (numDigits > 0) {
                            letterAfterDigit = true;
                        }
                    } else if (isdigit(*s)) {
                        numDigits++;
                    } else {
                        badIDchars = true;
                    }
                }
                if (letterAfterDigit || badIDchars) {
                    ValidErr(eDiag_Error, eErr_SEQ_INST_BadSeqIdFormat,
                             "Bad accession: " + acc, seq);
                } else if (numLetters == 1 && numDigits == 5 && s_isNa(seq)) {
                } else if (numLetters == 2 && numDigits == 6 && s_isNa(seq)) {
                } else if (numLetters == 3 && numDigits == 5 && s_isAa(seq)) {
                } else if (numLetters == 2 && numDigits == 6 && s_isAa(seq) &&
                    seq.GetInst().GetRepr() == CSeq_inst:: eRepr_seg) {
                } else if (numLetters == 4  &&  numDigits == 8  &&
                    ((**k).IsGenbank()  ||  (**k).IsEmbl()  ||
                    (**k).IsDdbj())) {
                } else {
                    ValidErr(eDiag_Error, eErr_SEQ_INST_BadSeqIdFormat,
                             "Bad accession: " + acc, seq);
                }

                // Check for secondary conflicts
                if (!acc.empty()  &&  seq.GetFirstId()) {
                    CDesc_CI ds(m_Scope->GetBioseqHandle(*seq.GetFirstId()));
                    CSeqdesc_CI sd(ds);
                    for (; sd; ++sd) {
                        if (sd->IsGenbank()) {
                            const CGB_block& gb = sd->GetGenbank();
                            if (gb.IsSetExtra_accessions()) {
                                iterate(list<string>, a,
                                    gb.GetExtra_accessions()) {
                                    if (!NStr::CompareNocase(acc, *a)) {
                                        // If the same post error
                                        ValidErr(eDiag_Error,
                                            eErr_SEQ_INST_BadSecondaryAccn,
                                            acc + " used for both primary and"
                                            " secondary accession", seq);
                                    }
                                }
                            }
                        }
                        if (sd->IsEmbl()) {
                            const CEMBL_block& eb = sd->GetEmbl();
                            if (eb.IsSetExtra_acc()) {
                                iterate(list<string>, a, eb.GetExtra_acc()) {
                                    if (!NStr::CompareNocase(acc, *a)) {
                                        // If the same post error
                                        ValidErr(eDiag_Error,
                                            eErr_SEQ_INST_BadSecondaryAccn,
                                            acc + " used for both primary and"
                                            " secondary accession", seq);
                                    }
                                }
                            }
                        }
                    }
                }
            } catch (const runtime_error&) {}
            // Fall thru
        case CSeq_id::e_Other:
            try{
                const string& name = s_GetName(**k);
                iterate (string, s, name) {
                    if (isspace(*s)) {
                        ValidErr(eDiag_Critical, eErr_SEQ_INST_SeqIdNameHasSpace,
                                 "Seq-id.name " + name + " should be a single "
                                 "word without any spaces", seq);
                        break;
                    }
                }
            } catch (const runtime_error&) {}
            // Fall thru
        case CSeq_id::e_Pir:
        case CSeq_id::e_Swissprot:
        case CSeq_id::e_Prf:
        {
            const CTextseq_id& ti = s_GetTextseq_id(**k);
            if (s_isNa(seq)  &&  (!ti.IsSetAccession() ||
                ti.GetAccession().empty())) {
                if (seq.GetInst().GetRepr() != CSeq_inst::eRepr_seg  ||
                    m_IsGI) {
                    if (!(**k).IsDdbj()  ||
                        seq.GetInst().GetRepr() != CSeq_inst::eRepr_seg) {
                        CNcbiOstrstream os;
                        os << "Missing accession for " << (**k).DumpAsFasta();
                        ValidErr(eDiag_Error, eErr_SEQ_INST_BadSeqIdFormat,
                            string(os.str()), seq);
                    }
                }
            }
            accn_count++;
            break;
        }
        case CSeq_id::e_Patent:
            break;
        case CSeq_id::e_Pdb:
            break;
        case CSeq_id::e_Gi:
            if ((**k).GetGi() == 0) {
                ValidErr(eDiag_Error, eErr_SEQ_INST_ZeroGiNumber,
                         "Invalid GI number", seq);
            }
            gi_count++;
            break;
        case CSeq_id::e_General:
            break;
        default:
            break;
        }
    }

    // Check that a sequence with a gi number has exactly one accession
    if (gi_count > 0  &&  accn_count == 0) {
        ValidErr(eDiag_Error, eErr_SEQ_INST_GiWithoutAccession,
            "No accession on sequence with gi number", seq);
    }
    if (gi_count > 0  &&  accn_count > 1) {
        ValidErr(eDiag_Error, eErr_SEQ_INST_MultipleAccessions,
            "Multiple accessions on sequence with gi number", seq);
    }

    // C toolkit ensures that there is exactly one CBioseq for a CSeq_id
    // Not done here because object manager will not allow
    // the same Seq-id on multiple Bioseqs

}


void CValidError_impl::ValidateInst
(const CBioseq& seq)
{

    const CSeq_inst& inst = seq.GetInst();

    // Check representation
    if (!ValidateRepr(inst, seq)) {
        return;
    }

    // Check molecule, topology, and strand
    const CSeq_inst::EMol& mol = inst.GetMol();
    switch (mol) {
        case CSeq_inst::eMol_na:
            ValidErr(eDiag_Error, eErr_SEQ_INST_MolNuclAcid,
                     "Bioseq.mol is type na", seq);
            break;
        case CSeq_inst::eMol_aa:
            if (inst.IsSetTopology()  &&
              inst.GetTopology() != CSeq_inst::eTopology_linear)
            {
                ValidErr(eDiag_Error, eErr_SEQ_INST_CircularProtein,
                         "Non-linear topology set on protein", seq);
            }
            if (inst.IsSetStrand()  &&
              inst.GetStrand() != CSeq_inst::eStrand_ss)
            {
                ValidErr(eDiag_Error, eErr_SEQ_INST_DSProtein,
                         "Protein not single stranded", seq);
            }
            break;
        case CSeq_inst::eMol_not_set:
            ValidErr(eDiag_Error, eErr_SEQ_INST_MolNotSet, "Bioseq.mol not set",
                seq);
            break;
        case CSeq_inst::eMol_other:
            ValidErr(eDiag_Error, eErr_SEQ_INST_MolOther,
                     "Bioseq.mol is type other", seq);
            break;
        default:
            break;
    }

    CSeq_inst::ERepr rp = seq.GetInst().GetRepr();

    if (rp == CSeq_inst::eRepr_raw  ||  rp == CSeq_inst::eRepr_const) {    
        // Validate raw and constructed sequences
        ValidateRawConst(seq);
    }

    if (rp == CSeq_inst::eRepr_seg  ||  rp == CSeq_inst::eRepr_ref) {
        // Validate segmented and reference sequences
        ValidateSegRef(seq);
    }

    if (rp == CSeq_inst::eRepr_delta) {
        // Validate delta sequences
        ValidateDelta(seq);
    }

    if (rp == CSeq_inst::eRepr_seg  &&  seq.GetInst().IsSetExt()  &&
        seq.GetInst().GetExt().IsSeg()) {
        // Validate part of segmented sequence
        ValidateSeqParts(seq);
    }

    if (s_isAa(seq)) {
        // Validate protein title (amino acids only)
        ValidateProteinTitle(seq);
    }

    // Validate sequence length
    ValidateSeqLen(seq);
}


void CValidError_impl::ValidateBioseq(const CBioseq& seq)
{
    // Validate CSeq_ids
    ValidateSeqIds(seq);

    // Validate the inst data
    ValidateInst(seq);

    // Validate the context
    ValidateBioseqContext(seq);
}


void CValidError_impl::ValidateSeqDesc(const CSeqdesc& desc)
{
    // switch on type, e.g., call ValidateBioSource or ValidatePubdesc
    switch (desc.Which ()) {
        case CSeqdesc::e_not_set:
            break;
        case CSeqdesc::e_Mol_type:
            break;
        case CSeqdesc::e_Modif:
            break;
        case CSeqdesc::e_Method:
            break;
        case CSeqdesc::e_Name:
            break;
        case CSeqdesc::e_Title:
            break;
        case CSeqdesc::e_Org:
            break;
        case CSeqdesc::e_Comment:
            break;
        case CSeqdesc::e_Num:
            break;
        case CSeqdesc::e_Maploc:
            break;
        case CSeqdesc::e_Pir:
            break;
        case CSeqdesc::e_Genbank:
            break;
        case CSeqdesc::e_Pub:
        {
            const CPubdesc& pub = desc.GetPub ();
            ValidatePubdesc (pub, desc);
            break;
        }
        case CSeqdesc::e_Region:
            break;
        case CSeqdesc::e_User:
        {
            const CUser_object& usr = desc.GetUser ();
            const CObject_id& oi = usr.GetType();
            if (!oi.IsStr()) {
                break;
            }
            if (!NStr::CompareNocase(oi.GetStr(), "TpaAssembly")) {
                try {
                    ValidErr(eDiag_Error, eErr_SEQ_DESCR_InvalidForType,
                        "Non-TPA record should not have TpaAssembly object", desc);
                } catch (const runtime_error&) {
                    return;
                }
            }
            break;
        }
        case CSeqdesc::e_Sp:
            break;
        case CSeqdesc::e_Dbxref:
            break;
        case CSeqdesc::e_Embl:
            break;
        case CSeqdesc::e_Create_date:
            break;
        case CSeqdesc::e_Update_date:
            break;
        case CSeqdesc::e_Prf:
            break;
        case CSeqdesc::e_Pdb:
            break;
        case CSeqdesc::e_Het:
            break;
        case CSeqdesc::e_Source:
        {
            const CBioSource& src = desc.GetSource ();
            ValidateBioSource (src, desc);
            break;
        }
        case CSeqdesc::e_Molinfo:
            break;
        default:
            break;
    }
}


void CValidError_impl::ValidateSeqSet(const CBioseq_set& seqset)
{

}


void CValidError_impl::ValidateSeqAnnot(const CSeq_annot& annot)
{

}


void CValidError_impl::ValidateSeqAlign(const CSeq_align& align)
{

}


void CValidError_impl::ValidateSeqGraph(const CSeq_graph& graph)
{

}


void CValidError_impl::ValidateSeqFeatContext(const CBioseq& seq)
{

}


void CValidError_impl::ValidateSeqFeat(const CSeq_feat& feat)
{
    switch (feat.GetData ().Which ()) {
        case CSeqFeatData::e_Gene:
            break;
        case CSeqFeatData::e_Cdregion:
            break;
        case CSeqFeatData::e_Prot:
            break;
        case CSeqFeatData::e_Rna:
            break;
        case CSeqFeatData::e_Pub:
            // Validate CPubdesc
            ValidatePubdesc(feat.GetData ().GetPub (), feat);
            break;
        case CSeqFeatData::e_Biosrc:
            // Validate CBioSource
            ValidateBioSource(feat.GetData ().GetBiosrc (), feat);
            break;
        default:
            break;
    }
    if (feat.IsSetDbxref ()) {
        iterate (CSeq_feat::TDbxref, db, feat.GetDbxref ()) {
            ValidateDbxref (**db, feat);
        }
    }
}


void CValidError_impl::ValidateSeqLoc
(const CSeq_loc& loc,
 const CBioseq&  seq,
 char*           prefix)
{
    bool circular = false;
    if (seq.GetInst().IsSetTopology()) {
        circular =
            seq.GetInst().GetTopology() == CSeq_inst::eTopology_circular;
    }

    CTypeConstIterator<CSeq_loc> lit = ConstBegin(loc);
    bool ordered = true, adjacent = false, chk = true,
        unmarked_strand = false, mixed_strand = false;
    const CSeq_id* id_cur = 0, *id_prv = 0;
    const CSeq_interval *int_cur = 0, *int_prv = 0;
    ENa_strand strand_cur = eNa_strand_unknown,
        strand_prv = eNa_strand_unknown;
    for (; lit; ++lit) {
        switch (lit->Which()) {
        case CSeq_loc::e_Int:
            int_cur = &lit->GetInt();
            strand_cur = int_cur->IsSetStrand() ?
                int_cur->GetStrand() : eNa_strand_unknown;
            id_cur = &int_cur->GetId();
            chk = IsValid(*int_cur, m_Scope);
            if (chk  &&  int_prv  && ordered  &&
                !circular  && id_prv) {
                if (IsSameBioseq(*id_prv, *id_cur, m_Scope)) {
                    if (strand_cur == eNa_strand_minus) {
                        if (int_prv->GetTo() < int_cur->GetTo()) {
                            ordered = false;
                        }
                        if (int_cur->GetTo() + 1 == int_prv->GetFrom()) {
                            adjacent = true;
                        }
                    } else {
                        if (int_prv->GetTo() > int_cur->GetTo()) {
                            ordered = false;
                        }
                        if (int_prv->GetTo() + 1 == int_cur->GetFrom()) {
                            adjacent = true;
                        }
                    }
                }
            }
            if (int_prv) {
                if (IsSameBioseq(int_prv->GetId(), int_cur->GetId(), m_Scope)){
                    if (strand_prv == strand_cur  &&
                        int_prv->GetFrom() == int_cur->GetFrom()  &&
                        int_prv->GetTo() == int_cur->GetTo()) {
                            ValidErr(eDiag_Error,
                                eErr_SEQ_FEAT_DuplicateInterval,
                                "Duplicate exons in location", seq);
                    }
                }
            }
            int_prv = int_cur;
            break;
        case CSeq_loc::e_Pnt:
            strand_cur = lit->GetPnt().IsSetStrand() ?
                lit->GetPnt().GetStrand() : eNa_strand_unknown;
            id_cur = &lit->GetPnt().GetId();
            chk = IsValid(lit->GetPnt(), m_Scope);
            int_prv = 0;
            break;
        case CSeq_loc::e_Packed_pnt:
            strand_cur = lit->GetPacked_pnt().IsSetStrand() ?
                lit->GetPacked_pnt().GetStrand() : eNa_strand_unknown;
            id_cur = &lit->GetPacked_pnt().GetId();
            chk = IsValid(lit->GetPacked_pnt(), m_Scope);
            int_prv = 0;
            break;
        case CSeq_loc::e_Null:
            break;
        default:
            strand_cur = eNa_strand_other;
            id_cur = 0;
            int_prv = 0;
            break;
        }
        if (!chk) {
            string lbl;
            lit->GetLabel(&lbl);
            ValidErr(eDiag_Critical, eErr_SEQ_FEAT_Range,
                string(prefix) + ": Seq-loc " + lbl + " out of range", seq);
        }

        if (lit->Which() != CSeq_loc::e_Null) {
            if (strand_prv != eNa_strand_other  &&
                strand_cur != eNa_strand_other) {
                if (id_cur  &&  id_prv  &&
                    IsSameBioseq(*id_cur, *id_prv, m_Scope)) {
                    if (strand_prv != strand_cur) {
                        if ((strand_prv == eNa_strand_plus  &&
                            strand_cur == eNa_strand_unknown)  ||
                            (strand_prv == eNa_strand_unknown  &&
                            strand_cur == eNa_strand_plus)) {
                            unmarked_strand = true;
                        } else {
                            mixed_strand = true;
                        }
                    }
                }
            }
            strand_prv = strand_cur;
            id_prv = id_cur;
        }
    }

    string loc_lbl;
    if (adjacent) {
        loc.GetLabel(&loc_lbl);
        ValidErr(eDiag_Error, eErr_SEQ_FEAT_AbuttingIntervals,
            string (prefix) + ": Adjacent intervals in SeqLoc [" +
            loc_lbl + "]", seq);
    }
    if (mixed_strand  ||  unmarked_strand  ||  !ordered) {
        if (loc_lbl.empty()) {
            loc.GetLabel(&loc_lbl);
        }
        if (mixed_strand) {
            ValidErr(eDiag_Error, eErr_SEQ_FEAT_MixedStrand,
                string(prefix) + ": Mixed strands in SeqLoc [" +
                loc_lbl + "]", seq);
        } else if (unmarked_strand) {
            ValidErr(eDiag_Warning, eErr_SEQ_FEAT_MixedStrand,
                string(prefix) + ": Mixed plus and unknown strands in SeqLoc "
                " [" + loc_lbl + "]", seq);
        }
        if (!ordered) {
            ValidErr(eDiag_Error, eErr_SEQ_FEAT_SeqLocOrder,
                string(prefix) + ": Intervals out of order in SeqLoc [" +
                loc_lbl + "]", seq);
        }
        return;
    }

    if (seq.GetInst().GetRepr() != CSeq_inst::eRepr_seg) {
        return;
    }

    // Check for intervals out of order on segmented Bioseq
    if (BadSeqLocSortOrder(seq, loc, m_Scope)) {
        if (loc_lbl.empty()) {
            loc.GetLabel(&loc_lbl);
        }
        ValidErr(eDiag_Error, eErr_SEQ_FEAT_SeqLocOrder,
            string(prefix) + "Intervals out of order in SeqLoc [" +
            loc_lbl + "]", seq);
    }

    // Check for mixed strand on segmented Bioseq
    if (s_SeqLocMixedStrands(seq, loc)) {
        if (loc_lbl.empty()) {
            loc.GetLabel(&loc_lbl);
        }
        ValidErr(eDiag_Error, eErr_SEQ_FEAT_MixedStrand,
            string(prefix) + ": Mixed strands in SeqLoc [" +
            loc_lbl + "]", seq);
    }

}


bool CValidError_impl::ValidateRepr
(const CSeq_inst& inst,
 const CBioseq&   seq)
{
    bool rtn = true;
    const CEnumeratedTypeValues* tv = CSeq_inst::GetTypeInfo_enum_ERepr();
    const string& rpr = tv->FindName(inst.GetRepr(), true);
    const string err0 = "Bioseq-ext not allowed on " + rpr + " Bioseq";
    const string err1 = "Missing or incorrect Bioseq-ext on " + rpr + " Bioseq";
    const string err2 = "Missing Seq-data on " + rpr + " Bioseq";
    const string err3 = "Seq-data not allowed on " + rpr + " Bioseq";
    switch (inst.GetRepr()) {
    case CSeq_inst::eRepr_virtual:
        if (inst.IsSetExt()) {
            ValidErr(eDiag_Error, eErr_SEQ_INST_ExtNotAllowed, err0, seq);
            rtn = false;
        }
        if (inst.IsSetSeq_data()) {
            ValidErr(eDiag_Error, eErr_SEQ_INST_SeqDataNotAllowed, err3, seq);
            rtn = false;
        }
        break;
    case CSeq_inst::eRepr_map:
        if (!inst.IsSetExt() || !inst.GetExt().IsMap()) {
            ValidErr(eDiag_Error, eErr_SEQ_INST_ExtBadOrMissing, err1, seq);
            rtn = false;
        }
        if (inst.IsSetSeq_data()) {
            ValidErr(eDiag_Error, eErr_SEQ_INST_SeqDataNotAllowed, err3, seq);
            rtn = false;
        }
        break;
    case CSeq_inst::eRepr_ref:
        if (!inst.IsSetExt() || !inst.GetExt().IsRef() ) {
            ValidErr(eDiag_Error, eErr_SEQ_INST_ExtBadOrMissing, err1, seq);
            rtn = false;
        }
        if (inst.IsSetSeq_data()) {
            ValidErr(eDiag_Error, eErr_SEQ_INST_SeqDataNotAllowed, err3, seq);
            rtn = false;
        }
        break;
    case CSeq_inst::eRepr_seg:
        if (!inst.IsSetExt() || !inst.GetExt().IsSeg() ) {
            ValidErr(eDiag_Error, eErr_SEQ_INST_ExtBadOrMissing, err1, seq);
            rtn = false;
        }
        if (inst.IsSetSeq_data()) {
            ValidErr(eDiag_Error, eErr_SEQ_INST_SeqDataNotAllowed, err3, seq);
            rtn = false;
        }
        break;
    case CSeq_inst::eRepr_raw:
    case CSeq_inst::eRepr_const:
        if (inst.IsSetExt()) {
            ValidErr(eDiag_Error, eErr_SEQ_INST_ExtNotAllowed, err0, seq);
            rtn = false;
        }
        if (!inst.IsSetSeq_data() ||
          inst.GetSeq_data().Which() == CSeq_data::e_not_set)
        {
            ValidErr(eDiag_Error, eErr_SEQ_INST_SeqDataNotFound, err2, seq);
            rtn = false;
        }
        break;
    case CSeq_inst::eRepr_delta:
        if (!inst.IsSetExt() || !inst.GetExt().IsDelta() ) {
            ValidErr(eDiag_Error, eErr_SEQ_INST_ExtBadOrMissing, err1, seq);
            rtn = false;
        }
        if (inst.IsSetSeq_data()) {
            ValidErr(eDiag_Error, eErr_SEQ_INST_SeqDataNotAllowed, err3, seq);
            rtn = false;
        }
        break;
    default:
        ValidErr(
            eDiag_Critical, eErr_SEQ_INST_ReprInvalid,
            "Invalid Bioseq->repr = " +
            NStr::IntToString(static_cast<int>(inst.GetRepr())), seq);
        rtn = false;
    }
    return rtn;
}


void CValidError_impl::Validate(const CSeq_entry& se, const CCit_sub* cs)
{
    // Check that CSeq_entry has data
    if (se.Which() == CSeq_entry::e_not_set) {
        ERR_POST(Warning << "Seq_entry not set");
        return;
    }

    // Set scope to CSeq_entry
    SetScope(se);

    // Get first CBioseq object pointer for ValidErr below.
    CTypeConstIterator<CBioseq> seq(ConstBegin(se));
    if (!seq) {
        ERR_POST("No Bioseq anywhere on this Seq-entry");
        return;
    }

    // If m_NonASCII is true, then this flag was set by the caller
    // of validate to indicate that a non ascii character had been
    // read from a file being used to create a CSeq_entry, that the
    // error had been corrected, but that the error needs to be reported
    // by Validate. Note, Validate is not doing anything other than
    // reporting an error if m_NonASCII is true;
    if (m_NonASCII) {
        ValidErr (eDiag_Critical, eErr_GENERIC_NonAsciiAsn,
                  "Non-ascii chars in input ASN.1 strings", *seq);
        // Only report the error once
        m_NonASCII = false;
    }

    // If no Pubs/BioSource in CSeq_entry, post only one error
    m_NoPubs = !s_AnyObj<CPub, CSeq_entry>(se);
    m_NoBioSource = !s_AnyObj<CBioSource, CSeq_entry>(se);

    // Look for genomic product set
    for (CTypeConstIterator <CBioseq_set> si (se); si; ++si) {
        if (si->IsSetClass ()) {
            if (si->GetClass () == CBioseq_set::eClass_gen_prod_set) {
                m_IsGPS = true;
            }
        }
    }

    // Examine all Seq-ids on Bioseqs
    for (CTypeConstIterator <CBioseq> bi (se); bi; ++bi) {
        iterate (CBioseq::TId, id, bi->GetId()) {
            CSeq_id::E_Choice typ = (**id).Which();
            switch (typ) {
                case CSeq_id::e_not_set:
                    break;
                case CSeq_id::e_Local:
                    break;
                case CSeq_id::e_Gibbsq:
                    break;
                case CSeq_id::e_Gibbmt:
                    break;
                case CSeq_id::e_Giim:
                    break;
                case CSeq_id::e_Genbank:
                    m_IsGED = true;
                    break;
                case CSeq_id::e_Embl:
                    m_IsGED = true;
                    break;
                case CSeq_id::e_Pir:
                    break;
                case CSeq_id::e_Swissprot:
                    break;
                case CSeq_id::e_Patent:
                    m_IsPatent = true;
                    break;
                case CSeq_id::e_Other:
                    m_IsRefSeq = true;
                    // and do RefSeq subclasses up front as well
                    if ((**id).GetOther().IsSetAccession()) {
                        string acc = (**id).GetOther().GetAccession().substr(0, 3);
                        if (acc == "NC_") {
                            m_IsNC = true;
                        } else if (acc == "NG_") {
                            m_IsNG = true;
                        } else if (acc == "NM_") {
                            m_IsNM = true;
                        } else if (acc == "NP_") {
                            m_IsNP = true;
                        } else if (acc == "NR_") {
                            m_IsNR = true;
                        } else if (acc == "NS_") {
                            m_IsNS = true;
                        } else if (acc == "NT_") {
                            m_IsNT = true;
                        } else if (acc == "NW_") {
                            m_IsNW = true;
                        } else if (acc == "XR_") {
                            m_IsXR = true;
                        }
                    }
                    break;
                case CSeq_id::e_General:
                    if (!NStr::CompareCase((**id).GetGeneral().GetDb(), "BankIt")) {
                        m_IsTPA = true;
                    }
                    break;
                case CSeq_id::e_Gi:
                    m_IsGI = true;
                    break;
                case CSeq_id::e_Ddbj:
                    m_IsGED = true;
                    break;
                case CSeq_id::e_Prf:
                    break;
                case CSeq_id::e_Pdb:
                    m_IsPDB = true;
                    break;
                case CSeq_id::e_Tpg:
                    m_IsTPA = true;
                    break;
                case CSeq_id::e_Tpe:
                    m_IsTPA = true;
                    break;
                case CSeq_id::e_Tpd:
                    m_IsTPA = true;
                    break;
                default:
                    break;
            }
        }
    }

    if (m_NoPubs  &&  !m_IsGPS  &&  !m_IsRefSeq  &&  !cs) {
        string msg = "No publications anywhere on this entire record";
        ValidErr(eDiag_Error, eErr_SEQ_DESCR_NoPubFound, msg, *seq);
    }

    if(m_NoBioSource  &&  !m_IsPatent  &&  !m_IsPDB) {
        string msg = "No organism name anywhere on this entire record";
        ValidErr(eDiag_Error, eErr_SEQ_DESCR_NoOrgFound, msg, *seq);
    }

    // Iterate thru components of record and validate each
    for (CTypeConstIterator <CSeq_feat> fi (se); fi; ++fi) {
        ValidateSeqFeat (*fi);
    }

    for (CTypeConstIterator <CSeqdesc> di (se); di; ++di) {
        ValidateSeqDesc (*di);
    }

    for (CTypeConstIterator <CBioseq> bi (se); bi; ++bi) {
        ValidateBioseq (*bi);
    }

    for (CTypeConstIterator <CBioseq_set> si (se); si; ++si) {
        ValidateSeqSet (*si);
    }

    for (CTypeConstIterator <CSeq_align> ai (se); ai; ++ai) {
        ValidateSeqAlign (*ai);
    }

    for (CTypeConstIterator <CSeq_graph> gi (se); gi; ++gi) {
        ValidateSeqGraph (*gi);
    }

    for (CTypeConstIterator <CSeq_annot> ni (se); ni; ++ni) {
        ValidateSeqAnnot (*ni);
    }

    for (CTypeConstIterator <CSeq_descr> ei (se); ei; ++ei) {
        ValidateDescrChain (*ei);
    }
}


void CValidError_impl::Validate(const CSeq_submit& ss)
{
    // Check that ss is type e_Entrys
    if (ss.GetData().Which() != CSeq_submit::C_Data::e_Entrys) {
        return;
    }

    // Get CCit_sub pointer
    const CCit_sub* cs = &ss.GetSub().GetCit();

    // Just loop thru CSeq_entrys
    list< CRef< CSeq_entry > >::const_iterator i;
    i = ss.GetData().GetEntrys().begin();
    for (; i != ss.GetData().GetEntrys().end(); ++i) {
        const CSeq_entry* se = (*i).GetPointer();
        Validate(*se, cs);
    }
}


void CValidError_impl::ValidateSeqLen(const CBioseq& seq)
{

    const CSeq_inst& inst = seq.GetInst();

    TSeqPos len = inst.IsSetLength() ? inst.GetLength() : 0;
    if (s_isAa(seq)) {
        if (len <= 3  &&  !m_IsPDB) {
            ValidErr(eDiag_Warning, eErr_SEQ_INST_ShortSeq, "Sequence only " +
                NStr::IntToString(len) + " residue(s) long", seq);
        }
    } else {
        if (len <= 10  &&  !m_IsPDB) {
            ValidErr(eDiag_Warning, eErr_SEQ_INST_ShortSeq, "Sequence only " +
                NStr::IntToString(len) + " residue(s) long", seq);
        }
    }

    if (len <= 350000) {
        return;
    }

    CTypeConstIterator<CMolInfo> mi = ConstBegin(seq);
    if (inst.GetRepr() == CSeq_inst::eRepr_delta) {
        if (mi  &&  mi->IsSetTech()  &&  m_IsGED) {
            CMolInfo::ETech tech = static_cast<CMolInfo::ETech>(mi->GetTech());
            if (tech == CMolInfo::eTech_htgs_0  ||
                tech == CMolInfo::eTech_htgs_1  ||
                tech == CMolInfo::eTech_htgs_2)
            {
                ValidErr(eDiag_Warning, eErr_SEQ_INST_LongHtgsSequence,
                    "Phase 0, 1 or 2 HTGS sequence exceeds 350kbp limit",
                    seq);
            } else if (tech == CMolInfo::eTech_htgs_3) {
                ValidErr(eDiag_Warning, eErr_SEQ_INST_SequenceExceeds350kbp,
                    "Phase 3 HTGS sequence exceeds 350kbp limit", seq);
            } else if (tech == CMolInfo::eTech_wgs) {
                ValidErr(eDiag_Warning, eErr_SEQ_INST_SequenceExceeds350kbp,
                    "WGS sequence exceeds 350kbp limit", seq);
            } else {
                TSeqPos len = 0;
                bool litHasData = false;
                CTypeConstIterator<CSeq_literal> lit(ConstBegin(seq));
                for (; lit; ++lit) {
                    if (lit->IsSetSeq_data()) {
                        litHasData = true;
                    }
                    len += lit->GetLength();
                }
                if (len > 350000  && litHasData) {
                    ValidErr(eDiag_Critical, eErr_SEQ_INST_LongLiteralSequence,
                        "Length of sequence literals exceeds 350kbp limit",
                        seq);
                }
            }
        }

    } else if (inst.GetRepr() == CSeq_inst::eRepr_raw) {
        if (mi  &&  mi->IsSetTech()) {
            CMolInfo::ETech tech = static_cast<CMolInfo::ETech>(mi->GetTech());
            if (tech == CMolInfo::eTech_htgs_0  ||
                tech == CMolInfo::eTech_htgs_1  ||
                tech == CMolInfo::eTech_htgs_2)
            {
                ValidErr(eDiag_Warning, eErr_SEQ_INST_LongHtgsSequence,
                    "Phase 0, 1 or 2 HTGS sequence exceeds 350kbp limit",
                    seq);
            } else if (tech == CMolInfo::eTech_htgs_3) {
                ValidErr(eDiag_Warning, eErr_SEQ_INST_SequenceExceeds350kbp,
                    "Phase 3 HTGS sequence exceeds 350kbp limit", seq);
            } else if (tech == CMolInfo::eTech_wgs) {
                ValidErr(eDiag_Warning, eErr_SEQ_INST_SequenceExceeds350kbp,
                    "WGS sequence exceeds 350kbp limit", seq);
            } else {
                ValidErr (eDiag_Warning, eErr_SEQ_INST_SequenceExceeds350kbp,
                    "Length of sequence exceeds 350kbp limit", seq);
            }
        }
    }
}


// Assumes that seq is segmented and has Seq-ext data
void CValidError_impl::ValidateSeqParts(const CBioseq& seq)
{
    // Get parent CSeq_entry of seq
    const CSeq_entry* parent = seq.GetParentEntry();
    if (!parent) {
        return;
    }

    // Need to get seq-set containing parent and then find the next
    // CSeq_entry in the set. This CSeq_entry should be a CBioseq_set
    // of class parts.
    const CSeq_entry* grand_parent = parent->GetParentEntry();
    if (!grand_parent  ||  !grand_parent->IsSet()) {
        return;
    }
    const CBioseq_set& set = grand_parent->GetSet();
    const list< CRef<CSeq_entry> >& seq_set = set.GetSeq_set();

    // Loop through seq_set looking for parent. When found, the next
    // CSeq_entry should have the parts
    const CSeq_entry* se = 0;
    iterate (list< CRef<CSeq_entry> >, it, seq_set) {
        if (parent == &(**it)) {
            ++it;
            se = &(**it);
            break;
        }
    }
    if (!se) {
        return;
    }

    // Check that se is a CBioseq_set of class parts
    if (!se->IsSet()  ||  !se->GetSet().IsSetClass()  ||
        se->GetSet().GetClass() != CBioseq_set::eClass_parts) {
        return;
    }

    // Get iterator for CSeq_entries in se
    CTypeConstIterator<CSeq_entry> se_parts(ConstBegin(*se));

    // Now, simultaneously loop through the parts of se and CSeq_locs of seq's
    // CSseq-ext. If don't compare, post error
    const list< CRef<CSeq_loc> > locs = seq.GetInst().GetExt().GetSeg().Get();
    iterate (list< CRef<CSeq_loc> >, lit, locs) {
        if ((**lit).Which() == CSeq_loc::e_Null) {
            continue;
        }
        if (!se_parts) {
            ValidErr(eDiag_Error, eErr_SEQ_INST_PartsOutOfOrder,
                "Parts set does not contain enough Bioseqs", seq);
        }
        if (se_parts->Which() == CSeq_entry::e_Seq) {
            try {
                const CBioseq& part = se_parts->GetSeq();
                const CSeq_id& id = GetId(**lit, m_Scope.GetPointer());
                if (!s_IsIdIn(id, part)) {
                    ValidErr(eDiag_Error, eErr_SEQ_INST_PartsOutOfOrder,
                        "Segmented bioseq seq_ext does not correspond to parts"
                        "packaging order", seq);
                }
            } catch (const CNotUnique&) {
                ERR_POST("Seq-loc not for unique sequence");
                return;
            } catch (...) {
                ERR_POST("Unknown error");
                return;
            }
        } else {
            ValidErr(eDiag_Error, eErr_SEQ_INST_PartsOutOfOrder,
                "Parts set component is not Bioseq", seq);
            return;
        }
        ++se_parts;
    }
    if (se_parts) {
        ValidErr(eDiag_Error, eErr_SEQ_INST_PartsOutOfOrder,
            "Parts set contains too many Bioseqs", seq);
    }
}


// Assumes seq is an amino acid sequence
void CValidError_impl::ValidateProteinTitle(const CBioseq& seq)
{
    const CSeq_id* id = seq.GetFirstId();
    if (!id) {
        return;
    }

    CBioseq_Handle hnd = m_Scope.GetPointer()->GetBioseqHandle(*id);
    string title_no_recon = GetTitle(hnd);
    string title_recon = GetTitle(hnd, fGetTitle_Reconstruct);
    if (title_no_recon != title_recon) {
        ValidErr(eDiag_Warning, eErr_SEQ_DESCR_InconsistentProteinTitle,
            "Instantiated protein title does not match automatically "
            "generated title", seq);
    }
}


void CValidError_impl::ValidateBioseqContext(const CBioseq& seq)
{
    // Get Molinfo
    CTypeConstIterator<CMolInfo> mi(ConstBegin(seq));

    if (mi->IsSetTech()) {
        switch (mi->GetTech()) {
        case CMolInfo::eTech_sts:
        case CMolInfo::eTech_survey:
        case CMolInfo::eTech_wgs:
        case CMolInfo::eTech_htgs_0:
        case CMolInfo::eTech_htgs_1:
        case CMolInfo::eTech_htgs_2:
        case CMolInfo::eTech_htgs_3:
            if (mi->GetTech() == CMolInfo::eTech_sts  &&
                seq.GetInst().GetMol() == CSeq_inst::eMol_rna  &&
                mi->IsSetBiomol()  &&
                mi->GetBiomol() == CMolInfo::eBiomol_mRNA) {
                // Ok
            } else if (mi->IsSetBiomol()  &&
                mi->GetBiomol() != CMolInfo::eBiomol_genomic) {
                ValidErr(eDiag_Error, eErr_SEQ_INST_ConflictingBiomolTech,
                    "HTGS/STS/GSS/WGS sequence should be genomic", seq);
            } else if (seq.GetInst().GetMol() != CSeq_inst::eMol_dna  &&
                seq.GetInst().GetMol() != CSeq_inst::eMol_na) {
                    ValidErr(eDiag_Error, eErr_SEQ_INST_ConflictingBiomolTech,
                        "HTGS/STS/GSS/WGS sequence should not be RNA", seq);
            }
            break;
        default:
            break;
        }
    }

    // Check that proteins in nuc_prot set have a CdRegion
    if (s_CdError(seq, m_Scope.GetPointer())) {
        ValidErr(eDiag_Error, eErr_SEQ_PKG_NoCdRegionPtr,
            "No CdRegion in nuc-prot set points to this protein", seq);
    }

    // Check that gene on non-segmented sequence does not have
    // multiple intervals
    ValidateMultiIntervalGene(seq);

    ValidateSeqFeatContext(seq);

    // Check for duplicate features and overlapping peptide features.
    ValidateDupOrOverlapFeats(seq);

    // Check for colliding gene names
    ValidateCollidingGeneNames(seq);

    ValidateSeqDescContext(seq);
}


void CValidError_impl::ValidateMultiIntervalGene(const CBioseq& seq)
{
    // Create a loc for iterator
    CSeq_loc loc;
    CSeq_id& lid = loc.SetWhole();
    const CSeq_id* sid = seq.GetFirstId();
    if (!sid) {
        return;
    }
    SerialAssign(lid, *sid);

    // Loop through features on gene
    for (CFeat_CI fit(*m_Scope.GetPointer(), loc, CSeqFeatData::e_Gene);
        fit; ++fit) {
    }
}


// Functional used by multimap below for sorting
class CRangeCmp
{
public:
    bool operator()(const CRange<TSeqPos>& r1, const CRange<TSeqPos>& r2)
    {
        if (r1.GetFrom() < r2.GetFrom()) {
            return true;
        } else if (r1.GetFrom() > r2.GetFrom()) {
            return false;
        } else if (r1.GetTo() < r2.GetTo()) {
            return true;
        } else {
            return false;
        }
    }
};


void CValidError_impl::ValidateDupOrOverlapFeats(const CBioseq& seq)
{
}


class CNoCaseCompare
{
public:
    bool operator ()(const string& s1, const string& s2) const
    {
        if (NStr::CompareNocase(s1, s2) < 0) {
            return true;
        } else {
            return false;
        }
    }
};


void CValidError_impl::ValidateCollidingGeneNames(const CBioseq& seq)
{
    /*
    // Loop through features and insert into multimap sorted by
    // feature label--case insensitive
    typedef multimap<string, const CSeq_feat*, CNoCaseCompare> TSmap;
    TSmap label_map;
    string label;
    for (CTypeConstIterator<CSeq_feat> ft(ConstBegin(seq)); ft; ++ft) {
        label.erase();
        feature::GetLabel(*ft, &label, feature::eContent, m_Scope.GetPointer());
        label_map.insert(TSmap::value_type(label, &*ft));
    }

    // Iterate through multimap and compare labels
    bool first = true;
    const string* plabel;
    iterate (TSmap, it, label_map) {
        if (first) {
            first = false;
            plabel = &(it->first);
            continue;
        }
        if (NStr::CompareCase(*plabel, it->first)) {
            ValidErr(eDiag_Warning, eErr_SEQ_FEAT_CollidingGeneNames,
                "Colliding names in gene features", *it->second);
        } else if (NStr::CompareNocase(*plabel, it->first)) {
            ValidErr(eDiag_Warning, eErr_SEQ_FEAT_CollidingGeneNames,
                "Colliding names (with different capitalization) in gene"
                "features", *it->second);
        }
        plabel = &(it->first);
    }
    */
}


// Assumes that seq is eRepr_raw or eRepr_inst
void CValidError_impl::ValidateRawConst(const CBioseq& seq)
{
    const CSeq_inst& inst = seq.GetInst();
    const CEnumeratedTypeValues* tv = CSeq_inst::GetTypeInfo_enum_ERepr();
    const string& rpr = tv->FindName(inst.GetRepr(), true);

    if (inst.IsSetFuzz()) {
        ValidErr(eDiag_Error, eErr_SEQ_INST_FuzzyLen,
            "Fuzzy length on " + rpr + "Bioseq", seq);
    }

    if (!inst.IsSetLength()  ||  inst.GetLength() == 0) {
        string len = inst.IsSetLength() ?
            NStr::IntToString(inst.GetLength()) : "0";
        ValidErr(eDiag_Error, eErr_SEQ_INST_InvalidLen,
                 "Invalid Bioseq length [" + len + "]", seq);
    }

    CSeq_data::E_Choice seqtyp = inst.IsSetSeq_data() ?
        inst.GetSeq_data().Which() : CSeq_data::e_not_set;
    switch (seqtyp) {
    case CSeq_data::e_Iupacna:
    case CSeq_data::e_Ncbi2na:
    case CSeq_data::e_Ncbi4na:
    case CSeq_data::e_Ncbi8na:
    case CSeq_data::e_Ncbipna:
        if (s_isAa(inst)) {
            ValidErr(eDiag_Critical, eErr_SEQ_INST_InvalidAlphabet,
                     "Using a nucleic acid alphabet on a protein sequence",
                     seq);
            return;
        }
        break;
    case CSeq_data::e_Iupacaa:
    case CSeq_data::e_Ncbi8aa:
    case CSeq_data::e_Ncbieaa:
    case CSeq_data::e_Ncbipaa:
    case CSeq_data::e_Ncbistdaa:
        if (s_isNa(inst)) {
            ValidErr(eDiag_Critical, eErr_SEQ_INST_InvalidAlphabet,
                     "Using a protein alphabet on a nucleic acid",
                     seq);
            return;
        }
        break;
    default:
        ValidErr(eDiag_Critical, eErr_SEQ_INST_InvalidAlphabet,
                 "Sequence alphabet not set",
                 seq);
        return;
    }

    bool check_alphabet = false;
    unsigned int factor = 1;
    switch (seqtyp) {
    case CSeq_data::e_Iupacaa:
    case CSeq_data::e_Iupacna:
    case CSeq_data::e_Ncbieaa:
    case CSeq_data::e_Ncbistdaa:
        check_alphabet = true;
        break;
    case CSeq_data::e_Ncbi8na:
    case CSeq_data::e_Ncbi8aa:
        break;
    case CSeq_data::e_Ncbi4na:
        factor = 2;
        break;
    case CSeq_data::e_Ncbi2na:
        factor = 4;
        break;
    case CSeq_data::e_Ncbipna:
        factor = 5;
        break;
    case CSeq_data::e_Ncbipaa:
        factor = 30;
        break;
    default:
        // Logically, should not occur
        ValidErr(eDiag_Critical, eErr_SEQ_INST_InvalidAlphabet,
                 "Sequence alphabet not set",
                 seq);
        return;
    }
    TSeqPos calc_len = inst.IsSetLength() ? inst.GetLength() : 0;
    string s_len = NStr::IntToString(calc_len);
    switch (seqtyp) {
    case CSeq_data::e_Ncbipna:
    case CSeq_data::e_Ncbipaa:
        calc_len *= factor;
        break;
    default:
        if (calc_len % factor) {
            calc_len += factor;
        }
        calc_len /= factor;
    }
    TSeqPos data_len = s_GetDataLen(inst);
    string s_data_len = NStr::IntToString(data_len);
    if (calc_len > data_len) {
        ValidErr(eDiag_Critical, eErr_SEQ_INST_SeqDataLenWrong,
                 "Bioseq.seq_data too short [" + s_data_len +
                 "] for given length [" + s_len + "]", seq);
        return;
    } else if (calc_len < data_len) {
        ValidErr(eDiag_Critical, eErr_SEQ_INST_SeqDataLenWrong,
                 "Bioseq.seq_data is larger [" + s_data_len +
                 "] than given length [" + s_len + "]", seq);
    }

    unsigned char termination;
    unsigned int trailingX = 0;
    unsigned int terminations = 0;
    if (check_alphabet) {

        switch (seqtyp) {
        case CSeq_data::e_Iupacaa:
        case CSeq_data::e_Ncbieaa:
            termination = '*';
            break;
        case CSeq_data::e_Ncbistdaa:
            termination = 25;
            break;
        default:
            termination = '\0';
        }

        const CSeq_id* id = (*seq.GetId().begin()).GetPointer();
        CSeqVector sv = m_Scope->GetBioseqHandle(*id).GetSeqVector();

        unsigned int bad_cnt = 0;
        for (TSeqPos pos = 0; pos < data_len; pos++) {
            if (sv[pos] > 250) {
                if (++bad_cnt > 10) {
                    ValidErr(eDiag_Critical, eErr_SEQ_INST_InvalidResidue,
                        "More than 10 invalid residues. Checking stopped",
                        seq);
                    return;
                } else {
                    if (seqtyp == CSeq_data::e_Ncbistdaa) {
                        ValidErr(eDiag_Critical, eErr_SEQ_INST_InvalidResidue,
                            "Invalid residue [" +
                            NStr::IntToString((int)sv[pos]) +
                            "] in position [" +
                            NStr::IntToString((int)pos) + "]",
                            seq);
                    } else {
                        ValidErr(eDiag_Critical, eErr_SEQ_INST_InvalidResidue,
                            "Invalid residue [" +
                            NStr::IntToString((int)sv[pos]) +
                            "] in position [" +
                            NStr::IntToString((int)pos) + "]",
                            seq);
                    }
                }
            } else if (sv[pos] == termination) {
                terminations++;
                trailingX = 0;
            } else if (sv[pos] == 'X') {
                trailingX++;
            } else {
                trailingX = 0;
            }
        }

        if (trailingX > 0 && !s_SuppressTrailingXMsg(seq, m_Scope)) {
            // Suppress if cds ends in "*" or 3' partial
            ValidErr(eDiag_Warning, eErr_SEQ_INST_TrailingX,
                "Sequence ends in " +
                NStr::IntToString(trailingX) + " trailing X(s)",
                seq);
        }

        if (terminations  && seqtyp != CSeq_data::e_Iupacna) {
            // Post error indicating terminations found in protein sequence
            // First get gene label
            string glbl;
            s_GetGeneLabel(seq, &glbl, m_Scope);
            string plbl;
            s_GetSeqLabel(seq, &plbl, m_Scope);
            ValidErr(eDiag_Error, eErr_SEQ_INST_StopInProtein,
                NStr::IntToString(terminations) +
                " termination symbols in protein sequence (" +
                glbl + string(" - ") + plbl + ")", seq);
            if (!bad_cnt) {
                return;
            }
        }
        return;
    }
}


// Assumes seq is eRepr_seg or eRepr_ref
void CValidError_impl::ValidateSegRef(const CBioseq& seq)
{
    const CSeq_inst& inst = seq.GetInst();

    // Validate extension data -- wrap in CSeq_loc_mix for convenience
    CSeq_loc loc;
    if (s_GetLocFromSeq(seq, &loc)) {
        ValidateSeqLoc(loc, seq, "Segmented Bioseq");
    }

    // Validate Length
    try {
        TSeqPos loclen = GetLength(loc, m_Scope);
        TSeqPos seqlen = inst.IsSetLength() ? inst.GetLength() : 0;
        if (seqlen > loclen) {
            ValidErr(eDiag_Critical, eErr_SEQ_INST_SeqDataLenWrong,
                "Bioseq.seq_data too short [" + NStr::IntToString(loclen) +
                "] for given length [" + NStr::IntToString(seqlen) + "]",
                seq);
        } else if (seqlen < loclen) {
            ValidErr(eDiag_Critical, eErr_SEQ_INST_SeqDataLenWrong,
                "Bioseq.seq_data is larger [" + NStr::IntToString(loclen) +
                "] than given length [" + NStr::IntToString(seqlen) + "]",
                seq);
        }
    } catch (const CNoLength&) {
        ERR_POST(Critical << "Unable to calculate length: ");
    }

    // Check for multiple references to the same Bioseq
    if (inst.IsSetExt()  &&  inst.GetExt().IsSeg()) {
        const list< CRef<CSeq_loc> >& locs = inst.GetExt().GetSeg().Get();
        iterate(list< CRef<CSeq_loc> >, i1, locs) {
           if (!IsOneBioseq(**i1, m_Scope)) {
                continue;
            }
            const CSeq_id& id1 = GetId(**i1);
            list< CRef<CSeq_loc> >::const_iterator i2 = i1;
            for (++i2; i2 != locs.end(); ++i2) {
                if (!IsOneBioseq(**i2, m_Scope)) {
                    continue;
                }
                const CSeq_id& id2 = GetId(**i2);
                if (IsSameBioseq(id1, id2, m_Scope)) {
                    CNcbiOstrstream os;
                    os << id1.DumpAsFasta();
                    string sid(os.str());
                    if ((**i1).IsWhole()  &&  (**i2).IsWhole()) {
                        ValidErr(eDiag_Error,
                            eErr_SEQ_INST_DuplicateSegmentReferences,
                            "Segmented sequence has multiple references to " +
                            sid, seq);
                    } else {
                        ValidErr(eDiag_Warning,
                            eErr_SEQ_INST_DuplicateSegmentReferences,
                            "Segmented sequence has multiple references to " +
                            sid + " that are not e_Whole", seq);
                    }
                }
            }
        }
    }

    // Check that partial sequence info on sequence segments is consistent with
    // partial sequence info on sequence -- aa  sequences only
    unsigned int partial = s_GetSeqlocPartialInfo(loc, m_Scope);
    if (partial  &&  s_isAa(seq)) {
        bool got_partial = false;
        CTypeConstIterator<CSeqdesc> sd(ConstBegin(seq.GetDescr()));
        for (; sd; ++sd) {
            if (!(*sd).IsModif()) {
                continue;
            }
            iterate(list< EGIBB_mod >, md, (*sd).GetModif()) {
                switch (*md) {
                case eGIBB_mod_partial:
                    got_partial = true;
                    break;
                case eGIBB_mod_no_left:
                    if (partial & eSeqlocPartial_Start) {
                        ValidErr(eDiag_Error, eErr_SEQ_INST_PartialInconsistent,
                            "GIBB-mod no-left inconsistent with segmented "
                            "SeqLoc", seq);
                    }
                    got_partial = true;
                    break;
                case eGIBB_mod_no_right:
                    if (partial & eSeqlocPartial_Stop) {
                        ValidErr(eDiag_Error, eErr_SEQ_INST_PartialInconsistent,
                            "GIBB-mod no-right inconsistene with segmented "
                            "SeqLoc", seq);
                    }
                    got_partial = true;
                    break;
                default:
                    break;
                }
            }
        }
        if (!got_partial) {
            ValidErr(eDiag_Error, eErr_SEQ_INST_PartialInconsistent,
                "Partial segmented sequence without GIBB-mod", seq);
        }
    }
}


// Assumes seq is a delta sequence
void CValidError_impl::ValidateDelta(const CBioseq& seq)
{
    const CSeq_inst& inst = seq.GetInst();

    if (!inst.IsSetExt()  ||  !inst.GetExt().IsDelta()  ||
        inst.GetExt().GetDelta().Get().empty()) {
        ValidErr(eDiag_Error, eErr_SEQ_INST_SeqDataLenWrong,
            "No CDelta_ext data for delta Bioseq", seq);
    }

    TSeqPos len = 0;
    iterate(list< CRef<CDelta_seq> >, sg, inst.GetExt().GetDelta().Get()) {
        switch ((**sg).Which()) {
        case CDelta_seq::e_Loc:
            try {
                len += GetLength((**sg).GetLoc(), m_Scope);
            } catch (const CNoLength&) {
                ValidErr(eDiag_Error, eErr_SEQ_INST_SeqDataLenWrong,
                    "No length for Seq-loc of delta seq-ext", seq);
            }
            break;
        case CDelta_seq::e_Literal:
        {
            // The C toolkit code checks for valid alphabet here
            // The C++ object serializaton will not load if invalid alphabet
            // so no check needed here

            // Check for invalid residues
            if (!(**sg).GetLiteral().IsSetSeq_data()) {
                break;
            }
            const CSeq_data& data = (**sg).GetLiteral().GetSeq_data();
            vector<TSeqPos> badIdx;
            CSeqportUtil::Validate(data, &badIdx);
            const string* ss = 0;
            switch (data.Which()) {
            case CSeq_data::e_Iupacaa:
                ss = &data.GetIupacaa().Get();
                break;
            case CSeq_data::e_Iupacna:
                ss = &data.GetIupacna().Get();
                break;
            case CSeq_data::e_Ncbieaa:
                ss = &data.GetNcbieaa().Get();
                break;
            case CSeq_data::e_Ncbistdaa:
            {
                const vector<char>& c = data.GetNcbistdaa().Get();
                iterate (vector<TSeqPos>, ci, badIdx) {
                    ValidErr(eDiag_Error, eErr_SEQ_INST_InvalidResidue,
                        "Invalid residue [" +
                        NStr::IntToString((int)c[*ci]) + "] in position " +
                        NStr::IntToString(*ci), seq);
                }
                break;
            }
            default:
                break;
            }
            if (ss) {
                iterate (vector<TSeqPos>, it, badIdx) {
                    ValidErr(eDiag_Error, eErr_SEQ_INST_InvalidResidue,
                        "Invalid residue [" +
                        ss->substr(*it, 1) + "] in position " +
                        NStr::IntToString(*it), seq);
                }
            }
            len += (**sg).GetLiteral().GetLength();
            break;
        }
        default:
            ValidErr(eDiag_Error, eErr_SEQ_INST_ExtNotAllowed,
                "CDelta_seq::Which() is e_not_set", seq);
        }
    }
    if (inst.GetLength() > len) {
        ValidErr(eDiag_Critical, eErr_SEQ_INST_SeqDataLenWrong,
            "Bioseq.seq_data too short [" + NStr::IntToString(len) +
            "] for given length [" + NStr::IntToString(inst.GetLength()) +
            "]", seq);
    } else if (inst.GetLength() < len) {
        ValidErr(eDiag_Critical, eErr_SEQ_INST_SeqDataLenWrong,
            "Bisoeq.seq_data is larger [" + NStr::IntToString(len) +
            "] than given length [" + NStr::IntToString(inst.GetLength()) +
            "]", seq);
    }

    // Get CMolInfo and tech used for validating technique and gap positioning
    CTypeConstIterator<CMolInfo> mi(ConstBegin(seq.GetDescr()));
    int tech = mi->IsSetTech() ? mi->GetTech() : 0;

    // Validate technique
    if (mi  &&  !m_IsNT  &&  !m_IsNC  &&  !m_IsGPS) {
        if (tech != CMolInfo::eTech_unknown   &&
            tech != CMolInfo::eTech_standard  &&
            tech != CMolInfo::eTech_wgs       &&
            tech != CMolInfo::eTech_htgs_0    &&
            tech != CMolInfo::eTech_htgs_1    &&
            tech != CMolInfo::eTech_htgs_2    &&
            tech != CMolInfo::eTech_htgs_3      ) {
            const CEnumeratedTypeValues* tv =
                CMolInfo::GetTypeInfo_enum_ETech();
            const string& stech = tv->FindName(mi->GetTech(), true);
            ValidErr(eDiag_Error, eErr_SEQ_INST_BadDeltaSeq,
                "Delta seq technique should not be " + stech, seq);
        }
    }

    // Validate positioning of gaps
    CTypeConstIterator<CDelta_seq> ds(ConstBegin(inst));
    if (ds  &&  (*ds).IsLiteral()  &&  !(*ds).GetLiteral().IsSetSeq_data()) {
        ValidErr(eDiag_Error, eErr_SEQ_INST_BadDeltaSeq,
            "First delta seq component is a gap", seq);
    }
    bool last_is_gap = false;
    unsigned int num_adjacent_gaps = 0;
    unsigned int num_gaps = 0;
    for (++ds; ds; ++ds) {
        if ((*ds).IsLiteral()) {
            if (!(*ds).GetLiteral().IsSetSeq_data()) {
                if (last_is_gap) {
                    num_adjacent_gaps++;
                }
                last_is_gap = true;
                num_gaps++;
            } else {
                last_is_gap = false;
            }
        } else {
            last_is_gap = false;
        }
    }
    if (num_adjacent_gaps > 1) {
        ValidErr(eDiag_Error, eErr_SEQ_INST_BadDeltaSeq,
            "There is (are) " + NStr::IntToString(num_adjacent_gaps) +
            " adjacent gap(s) in delta seq", seq);
    }
    if (last_is_gap) {
        ValidErr(eDiag_Error, eErr_SEQ_INST_BadDeltaSeq,
            "Last delta seq component is a gap", seq);
    }
    if (num_gaps == 0  &&  mi) {
        if (tech == CMolInfo::eTech_htgs_1  ||
            tech == CMolInfo::eTech_htgs_2) {
            ValidErr(eDiag_Warning, eErr_SEQ_INST_BadDeltaSeq,
                "HTGS 1 or 2 delta seq has no gaps", seq);
        }
    }
}


void CValidError_impl::ValidatePubdesc
(const CPubdesc&      pub,
 const CSerialObject& obj)
{
}


const char * CValidError_impl::legalDbXrefs [] = {
  "PIDe", "PIDd", "PIDg", "PID",
  "ATCC",
  "ATCC(in host)",
  "ATCC(dna)",
  "BDGP_EST",
  "BDGP_INS",
  "CDD",
  "CK",
  "COG",
  "dbEST",
  "dbSNP",
  "dbSTS",
  "ENSEMBL",
  "ESTLIB",
  "FANTOM_DB",
  "FLYBASE",
  "GABI",
  "GDB",
  "GeneID",
  "GI",
  "GO",
  "IFO",
  "IMGT/LIGM",
  "IMGT/HLA",
  "InterimID",
  "ISFinder",
  "JCM",
  "LocusID",
  "MaizeDB",
  "MGD",
  "MGI",
  "MIM",
  "niaEST",
  "PIR",
  "PSEUDO",
  "RATMAP",
  "RiceGenes",
  "REMTREMBL",
  "RGD",
  "RZPD",
  "SGD",
  "SoyBase",
  "SPTREMBL",
  "SWISS-PROT",
  "taxon",
  "UniGene",
  "UniSTS",
  "WormBase",
    0  // to indicate that there is no more data
};

const char * CValidError_impl::legalRefSeqDbXrefs [] = {
  "GenBank",
  "EMBL",
  "DDBJ",
    0  // to indicate that there is no more data
};


void CValidError_impl::ValidateDbxref
(const CDbtag&        xref,
 const CSerialObject& obj)
{
    const string& str = xref.GetDb ();
    for (size_t i = 0; legalDbXrefs [i];  i++) {
        if (NStr::Compare (str, legalDbXrefs [i]) == 0) {
            return;
        }
    }
    if (m_IsRefSeq) {
        for (size_t i = 0; legalDbXrefs [i];  i++) {
            if (NStr::Compare (str, legalRefSeqDbXrefs [i]) == 0) {
                return;
            }
        }
    }
    ValidErr (eDiag_Warning, eErr_SEQ_FEAT_IllegalDbXref,
             "Illegal db_xref type " + str, obj);
}


// ************************** Official country names  ***********************

const string CValidError_impl::countrycodes [] = {
  "Afghanistan",
  "Albania",
  "Algeria",
  "American Samoa",
  "Andorra",
  "Angola",
  "Anguilla",
  "Antarctica",
  "Antigua and Barbuda",
  "Argentina",
  "Armenia",
  "Aruba",
  "Ashmore and Cartier Islands",
  "Australia",
  "Austria",
  "Azerbaijan",
  "Bahamas",
  "Bahrain",
  "Baker Island",
  "Bangladesh",
  "Barbados",
  "Bassas da India",
  "Belarus",
  "Belgium",
  "Belize",
  "Benin",
  "Bermuda",
  "Bhutan",
  "Bolivia",
  "Bosnia and Herzegovina",
  "Botswana",
  "Bouvet Island",
  "Brazil",
  "British Virgin Islands",
  "Brunei",
  "Bulgaria",
  "Burkina Faso",
  "Burundi",
  "Cambodia",
  "Cameroon",
  "Canada",
  "Cape Verde",
  "Cayman Islands",
  "Central African Republic",
  "Chad",
  "Chile",
  "China",
  "Christmas Island",
  "Clipperton Island",
  "Cocos Islands",
  "Colombia",
  "Comoros",
  "Cook Islands",
  "Coral Sea Islands",
  "Costa Rica",
  "Cote d'Ivoire",
  "Croatia",
  "Cuba",
  "Cyprus",
  "Czech Republic",
  "Democratic Republic of the Congo",
  "Denmark",
  "Djibouti",
  "Dominica",
  "Dominican Republic",
  "Ecuador",
  "Egypt",
  "El Salvador",
  "Equatorial Guinea",
  "Eritrea",
  "Estonia",
  "Ethiopia",
  "Europa Island",
  "Falkland Islands (Islas Malvinas)",
  "Faroe Islands",
  "Fiji",
  "Finland",
  "France",
  "French Guiana",
  "French Polynesia",
  "French Southern and Antarctic Lands",
  "Gabon",
  "Gambia",
  "Gaza Strip",
  "Georgia",
  "Germany",
  "Ghana",
  "Gibraltar",
  "Glorioso Islands",
  "Greece",
  "Greenland",
  "Grenada",
  "Guadeloupe",
  "Guam",
  "Guatemala",
  "Guernsey",
  "Guinea",
  "Guinea-Bissau",
  "Guyana",
  "Haiti",
  "Heard Island and McDonald Islands",
  "Honduras",
  "Hong Kong",
  "Howland Island",
  "Hungary",
  "Iceland",
  "India",
  "Indonesia",
  "Iran",
  "Iraq",
  "Ireland",
  "Isle of Man",
  "Israel",
  "Italy",
  "Jamaica",
  "Jan Mayen",
  "Japan",
  "Jarvis Island",
  "Jersey",
  "Johnston Atoll",
  "Jordan",
  "Juan de Nova Island",
  "Kazakhstan",
  "Kenya",
  "Kingman Reef",
  "Kiribati",
  "Kuwait",
  "Kyrgyzstan",
  "Laos",
  "Latvia",
  "Lebanon",
  "Lesotho",
  "Liberia",
  "Libya",
  "Liechtenstein",
  "Lithuania",
  "Luxembourg",
  "Macau",
  "Macedonia",
  "Madagascar",
  "Malawi",
  "Malaysia",
  "Maldives",
  "Mali",
  "Malta",
  "Marshall Islands",
  "Martinique",
  "Mauritania",
  "Mauritius",
  "Mayotte",
  "Mexico",
  "Micronesia",
  "Midway Islands",
  "Moldova",
  "Monaco",
  "Mongolia",
  "Montserrat",
  "Morocco",
  "Mozambique",
  "Myanmar",
  "Namibia",
  "Nauru",
  "Navassa Island",
  "Nepal",
  "Netherlands",
  "Netherlands Antilles",
  "New Caledonia",
  "New Zealand",
  "Nicaragua",
  "Niger",
  "Nigeria",
  "Niue",
  "Norfolk Island",
  "North Korea",
  "Northern Mariana Islands",
  "Norway",
  "Oman",
  "Pakistan",
  "Palau",
  "Palmyra Atoll",
  "Panama",
  "Papua New Guinea",
  "Paracel Islands",
  "Paraguay",
  "Peru",
  "Philippines",
  "Pitcairn Islands",
  "Poland",
  "Portugal",
  "Puerto Rico",
  "Qatar",
  "Republic of the Congo",
  "Reunion",
  "Romania",
  "Russia",
  "Rwanda",
  "Saint Helena",
  "Saint Kitts and Nevis",
  "Saint Lucia",
  "Saint Pierre and Miquelon",
  "Saint Vincent and the Grenadines",
  "Samoa",
  "San Marino",
  "Sao Tome and Principe",
  "Saudi Arabia",
  "Senegal",
  "Seychelles",
  "Sierra Leone",
  "Singapore",
  "Slovakia",
  "Slovenia",
  "Solomon Islands",
  "Somalia",
  "South Africa",
  "South Georgia and the South Sandwich Islands",
  "South Korea",
  "Spain",
  "Spratly Islands",
  "Sri Lanka",
  "Sudan",
  "Suriname",
  "Svalbard",
  "Swaziland",
  "Sweden",
  "Switzerland",
  "Syria",
  "Taiwan",
  "Tajikistan",
  "Tanzania",
  "Thailand",
  "Togo",
  "Tokelau",
  "Tonga",
  "Trinidad and Tobago",
  "Tromelin Island",
  "Tunisia",
  "Turkey",
  "Turkmenistan",
  "Turks and Caicos Islands",
  "Tuvalu",
  "Uganda",
  "Ukraine",
  "United Arab Emirates",
  "United Kingdom",
  "Uruguay",
  "USA",
  "Uzbekistan",
  "Vanuatu",
  "Venezuela",
  "Viet Nam",
  "Virgin Islands",
  "Wake Island",
  "Wallis and Futuna",
  "West Bank",
  "Western Sahara",
  "Yemen",
  "Yugoslavia",
  "Zambia",
  "Zimbabwe"
};

bool CValidError_impl::IsCountryValid(const string &str) {
  if ( str.empty() ) {
    return false;
  }

  string country_name = str;
  string::size_type pos = str.find(':');
  if ( pos != string::npos ) {
    country_name = str.substr(0, pos);
  }

  const string *begin = countrycodes;
  const string *end = &(countrycodes[sizeof (countrycodes) / sizeof (string)]);
  if ( find (begin, end, country_name) == end ) {
    return false;
  }

  return true;
}

void CValidError_impl::ValidateSourceQualTags(void) {
/*
  if (vsp->sourceQualTags == NULL || StringHasNoText (str)) return;
  state = 0;
  ptr = str;
  ch = *ptr;
  while (ch != '\0') {
    matches = NULL;
    state = TextFsaNext (vsp->sourceQualTags, state, ch, &matches);
    if (matches != NULL) {
      hit = (CharPtr) matches->data.ptrvalue;
      if (StringHasNoText (hit)) {
        hit = "?";
      }
      ValidErr (vsp, SEV_WARNING, ERR_SEQ_DESCR_StructuredSourceNote,
                "Source note has structured tag %s", hit);
    }
    ptr++;
    ch = *ptr;
  }
*/
}


void CValidError_impl::ValidateBioSource
(const CBioSource&    bsrc,
 const CSerialObject& obj)
{
	const COrg_ref &orgref = bsrc.GetOrg();
  
	// Rule: Organism must have a name.
	if ( orgref.GetTaxname().empty() && orgref.GetCommon().empty() ) {
		ValidErr(eDiag_Error, eErr_SEQ_DESCR_NoOrgFound,
			     "No organism name has been applied to this Bioseq.", obj);
	}

	// Rule: validate legal locations.
	if ( bsrc.GetGenome() == CBioSource_Base::eGenome_transposon  ||
		 bsrc.GetGenome() == CBioSource_Base::eGenome_insertion_seq ) {
		ValidErr(eDiag_Warning, eErr_SEQ_DESCR_ObsoleteSourceLocation,
			     "Transposon and insertion sequence are no longer legal locations.", obj);
	}

	int chrom_count = 0;
	bool chrom_conflict = false;
	CSubSource *chromosome = 0;
	string countryname;
	iterate( CBioSource::TSubtype, ssit, bsrc.GetSubtype() ) {
		switch ( (**ssit).GetSubtype() ) {

		case CSubSource::eSubtype_country:
			countryname = (**ssit).GetName();
			if ( !IsCountryValid(countryname) ) {
				if ( countryname.empty() ) {
					countryname = "?";
				}
				ValidErr(eDiag_Warning, eErr_SEQ_DESCR_BadCountryCode,
						 "Bad country name " + countryname, obj);
			}
			break;

		case CSubSource::eSubtype_chromosome:
			++chrom_count;
			if ( chromosome != 0 ) {
				if ( NStr::CompareNocase((**ssit).GetName(), chromosome->GetName()) != 0) {
					chrom_conflict = true;
				}          
			} else {
				chromosome = ssit->GetPointer();
			}
			break;

		case CSubSource::eSubtype_transposon_name:
		case CSubSource::eSubtype_insertion_seq_name:
			ValidErr(eDiag_Warning, eErr_SEQ_DESCR_ObsoleteSourceQual,
					 "Transposon name and insertion sequence name are no longer legal qualifiers.", obj);
		break;

		case 0:
			ValidErr(eDiag_Warning, eErr_SEQ_DESCR_BadSubSource,
               "Unknown subsource subtype 0.", obj);
			break;
    
		case CSubSource::eSubtype_other:
			//ValidateSourceQualTags 
			break;
		}
	}
	if ( chrom_count > 1 ) {
		string msg = chrom_conflict ? "Multiple conflicting chromosome qualifiers" :
									  "Multiple identical chromosome qualifiers";
		ValidErr(eDiag_Warning, eErr_SEQ_DESCR_MultipleChromosomes, msg, obj);
	}

	const COrgName &orgname = orgref.GetOrgname();
	const string &lineage = orgname.GetLineage();
	if ( lineage.empty() ) {
		ValidErr(eDiag_Error, eErr_SEQ_DESCR_MissingLineage, 
			     "No lineage for this BioSource.", obj);
	} else {
		if ( bsrc.GetGenome() == CBioSource_Base::eGenome_kinetoplast ) {
			if ( lineage.find("Kinetoplastida") == string::npos ) {
				ValidErr(eDiag_Warning, eErr_SEQ_DESCR_BadOrganelle, 
						 "Only Kinetoplastida have kinetoplasts", obj);
			}
		} 
		if ( bsrc.GetGenome() == CBioSource_Base::eGenome_nucleomorph ) {
			if ( lineage.find("Chlorarchniophyta") == string::npos  &&
				lineage.find("Cryptophyta") == string::npos) {
				ValidErr(eDiag_Warning, eErr_SEQ_DESCR_BadOrganelle, 
						 "Only Chlorarchniophyta and Cryptophyta have nucleomorphs", obj);
			}
		}
	}

	iterate ( COrgName::TMod, omit, orgname.GetMod() ) {
		int subtype = (**omit).GetSubtype();

		if ( (subtype == 0) || (subtype == 1) ) {
			ValidErr(eDiag_Warning, eErr_SEQ_DESCR_BadOrgMod, 
				     "Unknown orgmod subtype " + subtype, obj);
		}
		if ( subtype == COrgMod::eSubtype_variety ) {
			if ( NStr::CompareNocase( orgname.GetDiv(), "PLN" ) != 0 ) {
				ValidErr(eDiag_Warning, eErr_SEQ_DESCR_BadOrgMod, 
						 "Orgmod variety should only be in plants or fungi", obj);
			}
		}
		if ( subtype == COrgMod::eSubtype_other ) {
			//ValidateSourceQualTags 
		}
	}

    if (orgref.IsSetDb ()) {
        iterate (COrg_ref::TDb, db, orgref.GetDb ()) {
            ValidateDbxref (**db, obj);
        }
    }
}


//************************** CValidError implementation **********************

CValidError::CValidError
(CObjectManager&   objmgr,
 const CSeq_entry& se,
 unsigned int      options)
{
    CValidError_impl val_impl(objmgr, m_ErrItems, options);
    val_impl.Validate(se);
}


CValidError::CValidError
(CObjectManager&    objmgr,
 const CSeq_submit& ss,
 unsigned int       options)
{

    CValidError_impl val_impl(objmgr, m_ErrItems, options);
    val_impl.Validate(ss);
}


CValidError::~CValidError()
{
}


//************************** miscellaneous arrays **********************

// External terse error type explanation
const string CValidErrItem::sm_Terse [] = {
    "UNKNOWN",

    "SEQ_INST_ExtNotAllowed",
    "SEQ_INST_ExtBadOrMissing",
    "SEQ_INST_SeqDataNotFound",
    "SEQ_INST_SeqDataNotAllowed",
    "SEQ_INST_ReprInvalid",
    "SEQ_INST_CircularProtein",
    "SEQ_INST_DSProtein",
    "SEQ_INST_MolNotSet",
    "SEQ_INST_MolOther",
    "SEQ_INST_FuzzyLen",
    "SEQ_INST_InvalidLen",
    "SEQ_INST_InvalidAlphabet",
    "SEQ_INST_SeqDataLenWrong",
    "SEQ_INST_SeqPortFail",
    "SEQ_INST_InvalidResidue",
    "SEQ_INST_StopInProtein",
    "SEQ_INST_PartialInconsistent",
    "SEQ_INST_ShortSeq",
    "SEQ_INST_NoIdOnBioseq",
    "SEQ_INST_BadDeltaSeq",
    "SEQ_INST_LongHtgsSequence",
    "SEQ_INST_LongLiteralSequence",
    "SEQ_INST_SequenceExceeds350kbp",
    "SEQ_INST_ConflictingIdsOnBioseq",
    "SEQ_INST_MolNuclAcid",
    "SEQ_INST_ConflictingBiomolTech",
    "SEQ_INST_SeqIdNameHasSpace",
    "SEQ_INST_IdOnMultipleBioseqs",
    "SEQ_INST_DuplicateSegmentReferences",
    "SEQ_INST_TrailingX",
    "SEQ_INST_BadSeqIdFormat",
    "SEQ_INST_PartsOutOfOrder",
    "SEQ_INST_BadSecondaryAccn",
    "SEQ_INST_ZeroGiNumber",
    "SEQ_INST_RnaDnaConflict",
    "SEQ_INST_HistoryGiCollision",
    "SEQ_INST_GiWithoutAccession",
    "SEQ_INST_MultipleAccessions",

    "SEQ_DESCR_BioSourceMissing",
    "SEQ_DESCR_InvalidForType",
    "SEQ_DESCR_FileOpenCollision",
    "SEQ_DESCR_Unknown",
    "SEQ_DESCR_NoPubFound",
    "SEQ_DESCR_NoOrgFound",
    "SEQ_DESCR_MultipleBioSources",
    "SEQ_DESCR_NoMolInfoFound",
    "SEQ_DESCR_BadCountryCode",
    "SEQ_DESCR_NoTaxonID",
    "SEQ_DESCR_InconsistentBioSources",
    "SEQ_DESCR_MissingLineage",
    "SEQ_DESCR_SerialInComment",
    "SEQ_DESCR_BioSourceNeedsFocus",
    "SEQ_DESCR_BadOrganelle",
    "SEQ_DESCR_MultipleChromosomes",
    "SEQ_DESCR_BadSubSource",
    "SEQ_DESCR_BadOrgMod",
    "SEQ_DESCR_InconsistentProteinTitle",
    "SEQ_DESCR_Inconsistent",
    "SEQ_DESCR_ObsoleteSourceLocation",
    "SEQ_DESCR_ObsoleteSourceQual",
    "SEQ_DESCR_StructuredSourceNote",
    "SEQ_DESCR_MultipleTitles",

    "GENERIC_NonAsciiAsn",
    "GENERIC_Spell",
    "GENERIC_AuthorListHasEtAl",
    "GENERIC_MissingPubInfo",
    "GENERIC_UnnecessaryPubEquiv",
    "GENERIC_BadPageNumbering",

    "SEQ_PKG_NoCdRegionPtr",
    "SEQ_PKG_NucProtProblem",
    "SEQ_PKG_SegSetProblem",
    "SEQ_PKG_EmptySet",
    "SEQ_PKG_NucProtNotSegSet",
    "SEQ_PKG_SegSetNotParts",
    "SEQ_PKG_SegSetMixedBioseqs",
    "SEQ_PKG_PartsSetMixedBioseqs",
    "SEQ_PKG_PartsSetHasSets",
    "SEQ_PKG_FeaturePackagingProblem",
    "SEQ_PKG_GenomicProductPackagingProblem",
    "SEQ_PKG_InconsistentMolInfoBiomols",

    "SEQ_FEAT_InvalidForType",
    "SEQ_FEAT_PartialProblem",
    "SEQ_FEAT_InvalidType",
    "SEQ_FEAT_Range",
    "SEQ_FEAT_MixedStrand",
    "SEQ_FEAT_SeqLocOrder",
    "SEQ_FEAT_CdTransFail",
    "SEQ_FEAT_StartCodon",
    "SEQ_FEAT_InternalStop",
    "SEQ_FEAT_NoProtein",
    "SEQ_FEAT_MisMatchAA",
    "SEQ_FEAT_TransLen",
    "SEQ_FEAT_NoStop",
    "SEQ_FEAT_TranslExcept",
    "SEQ_FEAT_NoProtRefFound",
    "SEQ_FEAT_NotSpliceConsensus",
    "SEQ_FEAT_OrfCdsHasProduct",
    "SEQ_FEAT_GeneRefHasNoData",
    "SEQ_FEAT_ExceptInconsistent",
    "SEQ_FEAT_ProtRefHasNoData",
    "SEQ_FEAT_GenCodeMismatch",
    "SEQ_FEAT_RNAtype0",
    "SEQ_FEAT_UnknownImpFeatKey",
    "SEQ_FEAT_UnknownImpFeatQual",
    "SEQ_FEAT_WrongQualOnImpFeat",
    "SEQ_FEAT_MissingQualOnImpFeat",
    "SEQ_FEAT_PsuedoCdsHasProduct",
    "SEQ_FEAT_IllegalDbXref",
    "SEQ_FEAT_FarLocation",
    "SEQ_FEAT_DuplicateFeat",
    "SEQ_FEAT_UnnecessaryGeneXref",
    "SEQ_FEAT_TranslExceptPhase",
    "SEQ_FEAT_TrnaCodonWrong",
    "SEQ_FEAT_BothStrands",
    "SEQ_FEAT_CDSgeneRange",
    "SEQ_FEAT_CDSmRNArange",
    "SEQ_FEAT_OverlappingPeptideFeat",
    "SEQ_FEAT_SerialInComment",
    "SEQ_FEAT_MultipleCDSproducts",
    "SEQ_FEAT_FocusOnBioSourceFeature",
    "SEQ_FEAT_PeptideFeatOutOfFrame",
    "SEQ_FEAT_InvalidQualifierValue",
    "SEQ_FEAT_MultipleMRNAproducts",
    "SEQ_FEAT_mRNAgeneRange",
    "SEQ_FEAT_TranscriptLen",
    "SEQ_FEAT_TranscriptMismatches",
    "SEQ_FEAT_CDSproductPackagingProblem",
    "SEQ_FEAT_DuplicateInterval",
    "SEQ_FEAT_PolyAsiteNotPoint",
    "SEQ_FEAT_ImpFeatBadLoc",
    "SEQ_FEAT_LocOnSegmentedBioseq",
    "SEQ_FEAT_UnnecessaryCitPubEquiv",
    "SEQ_FEAT_ImpCDShasTranslation",
    "SEQ_FEAT_ImpCDSnotPseudo",
    "SEQ_FEAT_MissingMRNAproduct",
    "SEQ_FEAT_AbuttingIntervals",
    "SEQ_FEAT_CollidingGeneNames",
    "SEQ_FEAT_MultiIntervalGene",
    "SEQ_FEAT_FeatContentDup",

    "SEQ_ALIGN_SeqIdProblem",
    "SEQ_ALIGN_StrandRev",
    "SEQ_ALIGN_DensegLenStart",
    "SEQ_ALIGN_StartLessthanZero",
    "SEQ_ALIGN_StartMorethanBiolen",
    "SEQ_ALIGN_EndLessthanZero",
    "SEQ_ALIGN_EndMorethanBiolen",
    "SEQ_ALIGN_LenLessthanZero",
    "SEQ_ALIGN_LenMorethanBiolen",
    "SEQ_ALIGN_SumLenStart",
    "SEQ_ALIGN_AlignDimSeqIdNotMatch",
    "SEQ_ALIGN_SegsDimSeqIdNotMatch",
    "SEQ_ALIGN_FastaLike",
    "SEQ_ALIGN_NullSegs",
    "SEQ_ALIGN_SegmentGap",
    "SEQ_ALIGN_SegsDimOne",
    "SEQ_ALIGN_AlignDimOne",
    "SEQ_ALIGN_Segtype",
    "SEQ_ALIGN_BlastAligns",

    "SEQ_GRAPH_GraphMin",
    "SEQ_GRAPH_GraphMax",
    "SEQ_GRAPH_GraphBelow",
    "SEQ_GRAPH_GraphAbove",
    "SEQ_GRAPH_GraphByteLen",
    "SEQ_GRAPH_GraphOutOfOrder",
    "SEQ_GRAPH_GraphBioseqLen",
    "SEQ_GRAPH_GraphSeqLitLen",
    "SEQ_GRAPH_GraphSeqLocLen",
    "SEQ_GRAPH_GraphStartPhase",
    "SEQ_GRAPH_GraphStopPhase",
    "SEQ_GRAPH_GraphDiffNumber",
    "SEQ_GRAPH_GraphACGTScore",
    "SEQ_GRAPH_GraphNScore",
    "SEQ_GRAPH_GraphGapScore",
    "SEQ_GRAPH_GraphOverlap",

    "UNKONWN"
};


// External verbose error type explanation
const string CValidErrItem::sm_Verbose [] = {
"UNKNOWN",

/* SEQ_INST */

"A Bioseq 'extension' is used for special classes of Bioseq. This class \
of Bioseq should not have one but it does. This is probably a software \
error.",

"This class of Bioseq requires an 'extension' but it is missing or of \
the wrong type. This is probably a software error.",

"No actual sequence data was found on this Bioseq. This is probably a \
software problem.",

"The wrong type of sequence data was found on this Bioseq. This is \
probably a software problem.",

"This Bioseq has an invalid representation class. This is probably a \
software error.",

"This protein Bioseq is represented as circular. Circular topology is \
normally used only for certain DNA molecules, for example, plasmids.",

"This protein Bioseq has strandedness indicated. Strandedness is \
normally a property only of DNA sequences. Please unset the \
strandedness.",

"It is not clear whether this sequence is nucleic acid or protein. \
Please set the appropriate molecule type (Bioseq.mol).",

"Most sequences are either nucleic acid or protein. However, the \
molecule type (Bioseq.mol) is set to 'other'. It should probably be set \
to nucleic acid or a protein.",
"This sequence is marked as having an uncertain length, but the length \
is known exactly.",

"The length indicated for this sequence is invalid. This is probably a \
software error.",

"This Bioseq has an invalid alphabet (e.g. protein codes on a nucleic \
acid or vice versa). This is probably a software error.",

"The length of this Bioseq does not agree with the length of the actual \
data. This is probably a software error.",

"Something is very wrong with this entry. The validator cannot open a \
SeqPort on the Bioseq. Further testing cannot be done.",

"Invalid residue codes were found in this Bioseq.",

"Stop codon symbols were found in this protein Bioseq.",

"This segmented sequence is described as complete or incomplete in \
several places, but these settings are inconsistent.",

"This Bioseq is unusually short (less than 4 amino acids or less than 11 \
nucleic acids). GenBank does not usually accept such short sequences.",

"No SeqIds were found on this Bioseq. This is probably a software \
error.",

"Delta sequences should only be HTGS-1 or HTGS-2.",

"HTGS-1 or HTGS-2 sequences must be < 350 KB in length.",

"Delta literals must be < 350 KB in length.",

"Individual sequences must be < 350 KB in length, unless they represent \
a single gene.",

"Two SeqIds of the same class was found on this Bioseq. This is probably \
a software error.",

"The specific type of this nucleic acid (DNA or RNA) is not set.",

"HTGS/STS/GSS records should be genomic DNA. There is a conflict between \
the technique and expected molecule type.",

"The Seq-id.name field should be a single word without any whitespace. \
This should be fixed by the database staff.",

"There are multiple occurrences of the same Seq-id in this record. \
Sequence identifiers must be unique within a record.",

"The segmented sequence refers multiple times to the same Seq-id. This \
may be due to a software error. Please consult with the database staff \
to fix this record.",

"The protein sequence ends with one or more X (unknown) amino acids.",

"A nucleotide sequence identifier should be 1 letter plus 5 digits or 2 \
letters plus 6 digits, and a protein sequence identifer should be 3 \
letters plus 5 digits.",

"The parts inside a segmented set should correspond to the seq_ext of \
the segmented bioseq. A difference will affect how the flatfile is \
displayed.",

"A secondary accession usually indicates a record replaced or subsumed \
by the current record. In this case, the current accession and \
secondary are the same.",

"GI numbers are assigned to sequences by NCBI's sequence tracking \
database. 0 is not a legal value for a gi number.",

"The MolInfo biomol field is inconsistent with the Bioseq molecule type \
field.",

"The Bioseq history gi refers to this Bioseq, not to its predecessor or \
successor.",

"The Bioseq has a gi identifier but no GenBank/EMBL/DDBJ accession \
identifier.",

"The Bioseq has a gi identifier and more than one GenBank/EMBL/DDBJ \
accession identifier.",

/* SEQ_DESCR */

"The biological source of this sequence has not been described \
correctly. A Bioseq must have a BioSource descriptor that covers the \
entire molecule. Additional BioSource features may also be added to \
recombinant molecules, natural or otherwise, to designate the parts of \
the molecule. Please add the source information.",

"This descriptor cannot be used with this Bioseq. A descriptor placed at \
the BioseqSet level applies to all of the Bioseqs in the set. Please \
make sure the descriptor is consistent with every sequence to which it \
applies.",

"FileOpen is unable to find a local file. This is normal, and can be \
ignored.",

"An unknown or 'other' modifier was used.",

"No publications were found in this entry which refer to this Bioseq. If \
a publication descriptor is added to a BioseqSet, it will apply to all \
of the Bioseqs in the set. A publication feature should be used if the \
publication applies only to a subregion of a sequence.",

"This entry does not specify the organism that was the source of the \
sequence. Please name the organism.",

"There are multiple BioSource or OrgRef descriptors in the same chain \
with the same taxonomic name. Their information should be combined into \
a single BioSource descriptor.",

"This sequence does not have a Mol-info descriptor applying to it. This \
indicates genomic vs. message, sequencing technique, and whether the \
sequence is incomplete.",

"The country code (up to the first colon) is not on the approved list of \
countries.",

"The BioSource is missing a taxonID database identifier. This will be \
inserted by the automated taxonomy lookup called by Clean Up Record.",

"This population study has BioSource descriptors with different \
taxonomic names. All members of a population study should be from the \
same organism.",

"A BioSource should have a taxonomic lineage, which can be obtained from \
the taxonomy network server.",

"Comments that refer to the conclusions of a specific reference should \
not be cited by a serial number inside brackets (e.g., [3]), but should \
instead be attached as a REMARK on the reference itself.",

"Focus must be set on a BioSource descriptor in records where there is a \
BioSource feature with a different organism name.",

"Note that only Kinetoplastida have kinetoplasts, and that only \
Chlorarchniophyta and Cryptophyta have nucleomorphs.",

"There are multiple chromosome qualifiers on this Bioseq. With the \
exception of some pseudoautosomal genes, this is likely to be a \
biological annotation error.",

"Unassigned SubSource subtype.",

"Unassigned OrgMod subtype.",

"An instantiated protein title descriptor should normally be the same as \
the automatically generated title. This may be a curated exception, or \
it may be out of synch with the current annotation.",

"There are two descriptors of the same type which are inconsistent with \
each other. Please make them consistent.",

"There is a source location that is no longer legal for use in GenBank \
records.",

"There is a source qualifier that is no longer legal for use in GenBank \
records.",

"The name of a structured source field is present as text in a note. \
The data should probably be put into the appropriate field instead.",

"There are multiple title descriptors in the same chain.",

/* SEQ_GENERIC */

"There is a non-ASCII type character in this entry.",

"There is a potentially misspelled word in this entry.",

"The author list contains et al, which should be replaced with the \
remaining author names.",

"The publication is missing essential information, such as title or \
authors.",

"A nested Pub-equiv is not normally expected in a publication. This may \
prevent proper display of all publication information.",

"The publication page numbering is suspect.",

/* SEQ_PKG */

"A protein is found in this entry, but the coding region has not been \
described. Please add a CdRegion feature to the nucleotide Bioseq.",

"Both DNA and protein sequences were expected, but one of the two seems \
to be missing. Perhaps this is the wrong package to use.",

"A segmented sequence was expected, but it was not found. Perhaps this \
is the wrong package to use.",

"No Bioseqs were found in this BioseqSet. Is that what was intended?",

"A nuc-prot set should not contain any other BioseqSet except segset.",

"A segset should not contain any other BioseqSet except parts.",

"A segset should not contain both nucleotide and protein Bioseqs.",

"A parts set should not contain both nucleotide and protein Bioseqs.",

"A parts set should not contain BioseqSets.",

"A feature should be packaged on its bioseq, or on a set containing the \
Bioseq.",

"The product of an mRNA feature in a genomic product set should point to \
a cDNA Bioseq packaged in the set, perhaps within a nuc-prot set. \
RefSeq records may however be referenced remotely.",

"Mol-info.biomol is inconsistent within a segset or parts set.",

/* SEQ_FEAT */

"This feature type is illegal on this type of Bioseq.",

"There are several places in an entry where a sequence can be described \
as either partial or complete. In this entry, these settings are \
inconsistent. Make sure that the location and product Seq-locs, the \
Bioseqs, and the SeqFeat partial flag all agree in describing this \
SeqFeat as partial or complete.",

"A feature with an invalid type has been detected. This is most likely a \
software problem.",

"The coordinates describing the location of a feature do not fall within \
the sequence itself. A feature location or a product Seq-loc is out of \
range of the Bioseq it points to.",

"Mixed strands (plus and minus) have been found in the same location. \
While this is biologically possible, it is very unusual. Please check \
that this is really what you mean.",

"This location has intervals that are out of order. While whis is \
biologically possible, it is very unusual. Please check that this is \
really what you mean.",

"A fundamental error occurred in software while attempting to translate \
this coding region. It is either a software problem or sever data \
corruption.",

"An illegal start codon was used. Some possible explanations are: (1) \
the wrong genetic code may have been selected; (2) the wrong reading \
frame may be in use; or (3) the coding region may be incomplete at the \
5' end, in which case a partial location should be indicated.",

"Internal stop codons are found in the protein sequence. Some possible \
explanations are: (1) the wrong genetic code may have been selected; (2) \
the wrong reading frame may be in use; (3) the coding region may be \
incomplete at the 5' end, in which case a partial location should be \
indicated; or (4) the CdRegion feature location is incorrect.",

"Normally a protein sequence is supplied. This sequence can then be \
compared with the translation of the coding region. In this entry, no \
protein Bioseq was found, and the comparison could not be made.",

"The protein sequence that was supplied is not identical to the \
translation of the coding region. Mismatching amino acids are found \
between these two sequences.",

"The protein sequence that was supplied is not the same length as the \
translation of the coding region. Please determine why they are \
different.",

"A coding region that is complete should have a stop codon at the 3'end. \
 A stop codon was not found on this sequence, although one was \
expected.",

"An unparsed \transl_except qualifier was found. This indicates a parser \
problem.",

"The name and description of the protein is missing from this entry. \
Every protein Bioseq must have one full-length Prot-ref feature to \
provide this information.",

"Splice junctions typically have GT as the first two bases of the intron \
(splice donor) and AG as the last two bases of the intron (splice \
acceptor). This intron does not conform to that pattern.",

"A coding region flagged as orf has a protein product. There should be \
no protein product bioseq on an orf.",

"A gene feature exists with no locus name or other fields filled in.",

"A coding region has an exception gbqual but the excpt flag is not \
set.",

"A protein feature exists with no name or other fields filled in.",

"The genetic code stored in the BioSource is different than that for \
this CDS.",

"RNA type 0 (unknown RNA) should be type 255 (other).",

"An import feature has an unrecognized key.",

"An import feature has an unrecognized qualifier.",

"This qualifier is not legal for this feature.",

"An essential qualifier for this feature is missing.",

"A coding region flagged as pseudo has a protein product. There should \
be no protein product bioseq on a pseudo CDS.",

"The database in a cross-reference is not on the list of officially \
recognized database abbreviations.",

"The location has a reference to a bioseq that is not packaged in this \
record.",

"The intervals on this feature are identical to another feature of the \
same type, but the label or comment are different.",

"This feature has a gene xref that is identical to the overlapping gene. \
This is redundant, and probably should be removed.",

"A \transl_except qualifier was not on a codon boundary.",

"The tRNA codon recognized does not code for the indicated amino acid \
using the specified genetic code.",

"Feature location indicates that it is on both strands. This is not \
biologically possible for this kind of feature. Please indicate the \
correct strand (plus or minus) for this feature.",

"A CDS is overlapped by a gene feature, but is not completely contained \
by it. This may be an annotation error.",

"A CDS is overlapped by an mRNA feature, but the mRNA does not cover all \
intervals (i.e., exons) on the CDS. This may be an annotation error.",

"The intervals on this processed protein feature overlap another protein \
feature. This may be caused by errors in originally annotating these \
features on DNA coordinates, where start or stop positions do not occur \
in between codon boundaries. These then appear as errors when the \
features are converted to protein coordinates by mapping through the \
CDS.",

"Comments that refer to the conclusions of a specific reference should \
not be cited by a serial number inside brackets (e.g., [3]), but should \
instead be attached as a REMARK on the reference itself.",

"More than one CDS feature points to the same protein product. This can \
happen with viral long terminal repeats (LTRs), but GenBank policy is to \
have each equivalent CDS point to a separately accessioned protein \
Bioseq.",

"The /focus flag is only appropriate on BioSource descriptors, not \
BioSource features.",

"The start or stop positions of this processed peptide feature do not \
occur in between codon boundaries. This may incorrectly overlap other \
peptides when the features are converted to protein coordinates by \
mapping through the CDS.",

"The value of this qualifier is constrained to a particular vocabulary \
of style. This value does not conform to those constraints. Please see \
the feature table documentation",

"for more information.",

"More than one mRNA feature points to the same cDNA product. This is an \
error in the genomic product set. Each mRNA feature should have a \
unique product Bioseq.",

"An mRNA is overlapped by a gene feature, but is not completely \
contained by it. This may be an annotation error.",

"The mRNA sequence that was supplied is not the same length as the \
transcription of the mRNA feature. Please determine why they are \
different.",

"The mRNA sequence and the transcription of the mRNA feature are \
different. If the number is large, it may indicate incorrect intron/exon \
boundaries.",

"The nucleotide location and protein product of the CDS are not packaged \
together in the same nuc-prot set. This may be an error in the software \
used to create",

"the record.",

"The location has identical adjacent intervals, e.g., a duplicate exon \
reference.",

"A polyA_site should be at a single nucleotide position.",

"An import feature loc field does not equal the feature location. This \
should be corrected, and then the loc field should be cleared.",

"Feature locations traditionally go on the individual parts of a \
segmented bioseq, not on the segmented sequence itself. These features \
are invisible in asn2ff reports, and are now being flagged for \
correction.",

"A set of citations on a feature should not normally have a nested \
Pub-equiv construct. This may prevent proper matching to the correct \
publication.",

"A CDS that has known translation errors cannot have a /translation \
qualifier.",

"A CDS that has known translation errors must be marked as pseudo to \
suppress the",

"translation.",

"The mRNA feature points to a cDNA product that is not packaged in the \
record. This is an error in the genomic product set.",

"The start of one interval is next to the stop of another. A single \
interval may be desirable in this case.",

"Two gene features should not have the same name.",

"A gene feature on a single Bioseq should have a single interval \
spanning everything considered to be under that gene.",

"The intervals on this feature are identical to another feature of the \
same type, and the label and comment are also identical. This is likely \
to be an error in annotating the record. Note that GenBank format \
suppresses duplicate features, so use of Graphic view is recommended.",

/* SEQ_ALIGN */

"The seqence referenced by an alignment SeqID is not packaged in the \
record.",

"Please contact the sequence database for further help with this \
error.",

"Please contact the sequence database for further help with this \
error.",

"Please contact the sequence database for further help with this \
error.",

"Please contact the sequence database for further help with this \
error.",

"Please contact the sequence database for further help with this \
error.",

"Please contact the sequence database for further help with this \
error.",

"Please contact the sequence database for further help with this \
error.",

"Please contact the sequence database for further help with this \
error.",

"Please contact the sequence database for further help with this \
error.",

"Please contact the sequence database for further help with this \
error.",

"Please contact the sequence database for further help with this \
error.",

"Please contact the sequence database for further help with this \
error.",

"Please contact the sequence database for further help with this \
error.",

"Please contact the sequence database for further help with this \
error.",

"Please contact the sequence database for further help with this \
error.",

"Please contact the sequence database for further help with this \
error.",

"Please contact the sequence database for further help with this \
error.",

"BLAST alignments are not desired in records submitted to the sequence \
database.",

/* SEQ_GRAPH */

"The graph minimum value is outside of the 0-100 range.",

"The graph maximum value is outside of the 0-100 range.",

"Some quality scores are below the stated graph minimum value.",

"Some quality scores are above the stated graph maximum value.",

"The number of bytes in the quality graph does not correspond to the",

"stated length of the graph.",

"The quality graphs are not packaged in order - may be due to an old \
fa2htgs bug.",

"The length of the quality graph does not correspond to the length of \
the Bioseq.",

"The length of the quality graph does not correspond to the length of \
the delta Bioseq literal component.",

"The length of the quality graph does not correspond to the length of \
the delta Bioseq location component.",

"The quality graph does not start or stop on a sequence segment \
boundary.",

"The quality graph does not start or stop on a sequence segment \
boundary.",

"The number quality graph does not equal the number of sequence \
segments.",

"Quality score values for known bases should be above 0.",

"Quality score values for unknown bases should not be above 0.",

"Gap positions should not have quality scores above 0.",

"Quality graphs overlap - may be due to an old fa2htgs bug.",

"UNKNOWN"
};


END_SCOPE(validator)
END_SCOPE(objects)
END_NCBI_SCOPE



/*
* ===========================================================================
* $Log$
* Revision 1.12  2002/10/14 20:00:58  kans
* added multiple titles check, moved big string arrays to end
*
* Revision 1.11  2002/10/14 15:06:43  ucko
* ValidateSeqDesc: Put braces around the cases with local variables, to
* fix compilation on (at least) GCC.
*
* Revision 1.10  2002/10/11 21:36:48  kans
* flatten organization, iterating within main Validate function, and added ValidateBiosource (MS)
*
* Revision 1.9  2002/10/11 13:52:36  clausen
* QA fixes & changed Seq-entry navigation
*
* Revision 1.8  2002/10/08 19:51:37  kans
* ValidateBioSource and ValidatePubdesc take CSerialObject
*
* Revision 1.7  2002/10/08 14:35:42  clausen
* Added calls to ValidateBiosource() & ValidatePub()
*
* Revision 1.6  2002/10/07 18:14:22  clausen
* Fixed error in CNoCaseCompare that prevent compile on Mac
*
* Revision 1.5  2002/10/07 17:11:16  ucko
* Fix usage of ERR_POST (caught by KCC)
*
* Revision 1.4  2002/10/04 14:39:41  ucko
* Use TSmap::value_type's constructor directly rather than via make_pair,
* as the latter fails under WorkShop.
*
* Revision 1.3  2002/10/04 14:27:37  ivanov
* Fixed s_IsDifferentDbxref() -- MSVC don't like identifier names lst1
* and lst2 in this function (weird compiler :).
* Fixed some warnings.
*
* Revision 1.2  2002/10/03 18:40:12  clausen
* Removed extra whitespace
*
* Revision 1.1  2002/10/03 16:29:32  clausen
* First version
*
* ===========================================================================
*/
