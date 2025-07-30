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
 * Author:  Jonathan Kans, Clifford Clausen, Aaron Ucko, Mati Shomrat, ....
 *
 * File Description:
 *   Implementation of private parts of the validator
 *   .......
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>
#include <corelib/ncbistr.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbi_safe_static.hpp>
#include <objmgr/object_manager.hpp>

#include <objtools/validator/validatorp.hpp>
#include <objtools/validator/validerror_desc.hpp>
#include <objtools/validator/validerror_descr.hpp>
#include <objtools/validator/validerror_annot.hpp>
#include <objtools/validator/validerror_bioseq.hpp>
#include <objtools/validator/validerror_bioseqset.hpp>
#include <objtools/validator/utilities.hpp>
#include <objtools/validator/validator_barcode.hpp>
#include <objtools/validator/validator_context.hpp>
#include <objtools/cleanup/cleanup.hpp>

#include <serial/iterator.hpp>
#include <serial/enumvalues.hpp>

#include <objects/general/Dbtag.hpp>
#include <objects/general/Person_id.hpp>
#include <objects/general/Name_std.hpp>

#include <objects/seqalign/Seq_align.hpp>

#include <objects/seqset/Bioseq_set.hpp>
#include <objects/seqset/Seq_entry.hpp>

#include <objects/seq/Bioseq.hpp>
#include <objects/seq/Seq_annot.hpp>
#include <objects/seq/Seqdesc.hpp>
#include <objects/seq/Seq_descr.hpp>
#include <objects/seq/Pubdesc.hpp>
#include <objects/seq/MolInfo.hpp>
#include <objects/seqfeat/BioSource.hpp>
#include <objects/seqfeat/OrgMod.hpp>
#include <objects/seqfeat/OrgName.hpp>
#include <objects/seqfeat/Org_ref.hpp>
#include <objects/seqfeat/Seq_feat.hpp>
#include <objects/seqfeat/SubSource.hpp>

#include <objects/seqloc/Seq_loc.hpp>
#include <objects/seqloc/Seq_interval.hpp>
#include <objects/seqloc/Seq_point.hpp>
#include <objects/seqloc/Textseq_id.hpp>

#include <objects/seqres/Seq_graph.hpp>

#include <objects/submit/Seq_submit.hpp>
#include <objects/submit/Submit_block.hpp>

#include <objmgr/bioseq_ci.hpp>
#include <objmgr/seqdesc_ci.hpp>
#include <objmgr/graph_ci.hpp>
#include <objmgr/seq_annot_ci.hpp>
#include <objmgr/util/feature.hpp>
#include <objmgr/util/sequence.hpp>

#include <objmgr/feat_ci.hpp>
#include <objmgr/align_ci.hpp>
#include <objmgr/seq_vector.hpp>
#include <objmgr/scope.hpp>

#include <objects/pub/Pub.hpp>
#include <objects/pub/Pub_equiv.hpp>

#include <objects/biblio/Author.hpp>
#include <objects/biblio/Auth_list.hpp>
#include <objects/biblio/Cit_art.hpp>
#include <objects/biblio/Cit_book.hpp>
#include <objects/biblio/Cit_gen.hpp>
#include <objects/biblio/Cit_jour.hpp>
#include <objects/biblio/Cit_let.hpp>
#include <objects/biblio/Cit_proc.hpp>
#include <objects/biblio/Cit_sub.hpp>
#include <objects/biblio/PubMedId.hpp>
#include <objects/biblio/PubStatus.hpp>
#include <objects/biblio/Title.hpp>
#include <objects/biblio/Imprint.hpp>
#include <objects/biblio/Affil.hpp>
#include <objects/misc/sequence_macros.hpp>
#include <objects/taxon3/itaxon3.hpp>
#include <objects/taxon3/taxon3.hpp>
#include <objects/taxon3/Taxon3_reply.hpp>

#include <objects/valid/Comment_set.hpp>
#include <objects/valid/Comment_rule.hpp>
#include <objects/valid/Field_set.hpp>
#include <objects/valid/Field_rule.hpp>
#include <objects/valid/Dependent_field_set.hpp>
#include <objects/valid/Dependent_field_rule.hpp>

#include <objtools/error_codes.hpp>
#include <objtools/validator/validerror_format.hpp>
#include <objtools/validator/utilities.hpp>
#include <objtools/validator/validator_context.hpp>
#include <objtools/edit/seq_entry_edit.hpp>
#include <objtools/edit/huge_asn_reader.hpp>
#include <util/sgml_entity.hpp>
#include <util/line_reader.hpp>
#include <util/util_misc.hpp>
#include <util/static_set.hpp>
#include <algorithm>

#include <serial/iterator.hpp>

#define NCBI_USE_ERRCODE_X   Objtools_Validator


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)
BEGIN_SCOPE(validator)
using namespace sequence;

namespace {
    // avoid creating a PQuickStringLess for every comparison
    PQuickStringLess s_QuickStringLess;
};


// =============================================================================
//                            CValidError_imp Public
// =============================================================================

const CSeqFeatData::E_Choice CCacheImpl::kAnyFeatType =
    static_cast<CSeqFeatData::E_Choice>(CSeqFeatData::e_not_set - 1);
const CSeqFeatData::ESubtype CCacheImpl::kAnyFeatSubtype =
    static_cast<CSeqFeatData::ESubtype>(CSeqFeatData::eSubtype_bad - 1);
const CCacheImpl::TFeatValue CCacheImpl::kEmptyFeatValue;

const CBioseq_Handle CCacheImpl::kEmptyBioseqHandle;
const CTSE_Handle CCacheImpl::kEmptyTSEHandle;
const CBioseq_Handle CCacheImpl::kAnyBioseq;

CValidError_imp::CValidError_imp
(CObjectManager&    objmgr,
 shared_ptr<SValidatorContext> pContext,
 IValidError*       errs,
 Uint4              options) :
    m_ObjMgr{&objmgr},
    m_ErrRepository{errs},
    m_pContext{pContext}
{
    x_Init(options, pContext->CumulativeInferenceCount, pContext->NotJustLocalOrGeneral, pContext->IsRefSeq);
}

void CValidError_imp::x_Init(Uint4 options, size_t initialInferenceCount, bool notJustLocalOrGeneral, bool hasRefSeq)
{
    SetOptions(options);
    Reset(initialInferenceCount, notJustLocalOrGeneral, hasRefSeq);

    InitializeSourceQualTags();
}

// Destructor
CValidError_imp::~CValidError_imp()
{
}


CValidError_imp::TSuppressed& CValidError_imp::SetSuppressed()
{
    return m_SuppressedErrors;
}

SValidatorContext& CValidError_imp::SetContext()
{
  //  if (!m_pContext) {
  //      m_pContext = make_shared<SValidatorContext>();
  //  }
    _ASSERT(m_pContext);
    return *m_pContext;
}


const SValidatorContext& CValidError_imp::GetContext() const
{
    _ASSERT(m_pContext);
    return *m_pContext;
}


bool CValidError_imp::IsHugeFileMode() const
{
    const auto& context = GetContext();
    return context.PreprocessHugeFile ||
           context.PostprocessHugeFile;
}


bool CValidError_imp::IsHugeSet(const CBioseq_set& bioseqSet) const
{
    if (bioseqSet.IsSetClass()) {
        return IsHugeSet(bioseqSet.GetClass());
    }
    return false;
}


bool CValidError_imp::IsHugeSet(CBioseq_set::TClass setClass) const
{
    return edit::CHugeAsnReader::IsHugeSet(setClass);
}


bool CValidError_imp::IsFarSequence(const CSeq_id& id) // const
{
    if (IsHugeFileMode() && GetContext().IsIdInBlob) {
        return !GetContext().IsIdInBlob(id);
    }

    _ASSERT(m_Scope);
    if (GetBioseqHandleFromTSE(id)) {
        return false;
    }
    return true;
}


CBioseq_Handle CValidError_imp::GetBioseqHandleFromTSE(const CSeq_id& id)
{
    if (m_Scope) {
        return m_Scope->GetBioseqHandleFromTSE(id, GetTSE_Handle());
    }
    return CBioseq_Handle();
}


CBioseq_Handle CValidError_imp::GetLocalBioseqHandle(const CSeq_id& id)
{
    if (!IsHugeFileMode()) {
        return GetBioseqHandleFromTSE(id);
    }
    // Huge-file mode
    if (!IsFarSequence(id)) {
        return m_Scope->GetBioseqHandle(id);
    }
    return CBioseq_Handle();
}


void CValidError_imp::SetOptions(Uint4 options)
{
    m_NonASCII = (options & CValidator::eVal_non_ascii) != 0;
    m_SuppressContext = (options & CValidator::eVal_no_context) != 0;
    m_ValidateAlignments = (options & CValidator::eVal_val_align) != 0;
    m_ValidateExons = (options & CValidator::eVal_val_exons) != 0;
    m_OvlPepErr = (options & CValidator::eVal_ovl_pep_err) != 0;
    m_RequireISOJTA = (options & CValidator::eVal_need_isojta) != 0;
    m_ValidateIdSet = (options & CValidator::eVal_validate_id_set) != 0;
    m_RemoteFetch = (options & CValidator::eVal_remote_fetch) != 0;
    m_FarFetchMRNAproducts = (options & CValidator::eVal_far_fetch_mrna_products) != 0;
    m_FarFetchCDSproducts = (options & CValidator::eVal_far_fetch_cds_products) != 0;
    m_LocusTagGeneralMatch = (options & CValidator::eVal_locus_tag_general_match) != 0;
    m_DoRubiscoText = (options & CValidator::eVal_do_rubisco_test) != 0;
    m_IndexerVersion = (options & CValidator::eVal_indexer_version) != 0;
    m_UseEntrez = (options & CValidator::eVal_use_entrez) != 0;
    m_DoTaxLookup = (options & CValidator::eVal_do_tax_lookup) != 0;
    m_DoBarcodeTests = (options & CValidator::eVal_do_barcode_tests) != 0;
    m_RefSeqConventions = (options & CValidator::eVal_refseq_conventions) != 0;
    m_SeqSubmitParent = (options & CValidator::eVal_seqsubmit_parent) != 0;
    m_ValidateInferenceAccessions = (options & CValidator::eVal_inference_accns) != 0;
    m_IgnoreExceptions = (options & CValidator::eVal_ignore_exceptions) != 0;
    m_ReportSpliceAsError = (options & CValidator::eVal_report_splice_as_error) != 0;
    m_LatLonCheckState = (options & CValidator::eVal_latlon_check_state) != 0;
    m_LatLonIgnoreWater = (options & CValidator::eVal_latlon_ignore_water) != 0;
    m_genomeSubmission = (options & CValidator::eVal_genome_submission) != 0;
    m_CollectLocusTags = (options & CValidator::eVal_collect_locus_tags) != 0;
    m_GenerateGoldenFile = (options & CValidator::eVal_generate_golden_file) != 0;
    m_CompareVDJCtoCDS = (options & CValidator::eVal_compare_vdjc_to_cds) != 0;
    m_IgnoreInferences = (options & CValidator::eVal_ignore_inferences) != 0;
    m_ForceInferences = (options & CValidator::eVal_force_inferences) != 0;
    m_NewStrainValidation = (options & CValidator::eVal_new_strain_validation) != 0;
}


//LCOV_EXCL_START
//not used by asnvalidate
void CValidError_imp::SetErrorRepository(IValidError* errors)
{
    m_ErrRepository = errors;
}
//LCOV_EXCL_STOP


void CValidError_imp::Reset(size_t prevCumulativeInferenceCount, bool notJustLocalOrGeneral, bool hasRefSeq)
{
    m_Scope = nullptr;
    m_TSE = nullptr;
    m_IsStandaloneAnnot = false;
    m_SeqAnnot.Reset();

    m_pEntryInfo.reset(new CValidatorEntryInfo());

    m_CumulativeInferenceCount = prevCumulativeInferenceCount;
    m_NotJustLocalOrGeneral = notJustLocalOrGeneral;
    m_HasRefSeq = hasRefSeq;

    m_IsNC = false;
    m_IsNG = false;
    m_IsNM = false;
    m_IsNP = false;
    m_IsNR = false;
    m_IsNZ = false;
    m_IsNS = false;
    m_IsNT = false;
    m_IsNW = false;
    m_IsWP = false;
    m_IsXR = false;

    m_PrgCallback = nullptr;
    m_NumAlign = 0;
    m_NumAnnot = 0;
    m_NumBioseq = 0;
    m_NumBioseq_set = 0;
    m_NumTopSetSiblings = 0;
    m_NumDesc = 0;
    m_NumDescr = 0;
    m_NumFeat = 0;
    m_NumGraph = 0;
    m_NumMisplacedFeatures = 0;
    m_NumSmallGenomeSetMisplaced = 0;
    m_NumMisplacedGraphs = 0;
    m_NumGenes = 0;
    m_NumGeneXrefs = 0;
    m_NumTpaWithHistory = 0;
    m_NumTpaWithoutHistory = 0;
    m_NumPseudo = 0;
    m_NumPseudogene = 0;
    m_FarFetchFailure = false;
    m_IsTbl2Asn = false;

    SetSuppressed().clear();
}

bool CValidError_imp::x_IsSuppressed(CValidErrItem::TErrIndex errType) const
{
    return (m_SuppressedErrors.find(errType) != m_SuppressedErrors.end());
}

// Error post methods
void CValidError_imp::PostErr
(EDiagSev sv,
 EErrType et,
 const string&  msg,
 const CSerialObject& obj)
{
    if (x_IsSuppressed(et)) {
        return;
    }

    const CTypeInfo* type_info = obj.GetThisTypeInfo();
    if (type_info == CSeqdesc::GetTypeInfo()) {
        const CSeqdesc* desc = dynamic_cast < const CSeqdesc* > (&obj);
        ERR_POST_X(1, Warning << "Seqdesc validation error using default context.");
        PostErr (sv, et, msg, GetTSE(), *desc);
    } else if (type_info == CSeq_feat::GetTypeInfo()) {
        const CSeq_feat* feat = dynamic_cast < const CSeq_feat* > (&obj);
        PostErr (sv, et, msg, *feat);
    } else if (type_info == CBioseq::GetTypeInfo()) {
        const CBioseq* seq = dynamic_cast < const CBioseq* > (&obj);
        PostErr (sv, et, msg, *seq);
    } else if (type_info == CBioseq_set::GetTypeInfo()) {
        const CBioseq_set* set = dynamic_cast < const CBioseq_set* > (&obj);
        PostErr (sv, et, msg, *set);
    } else if (type_info == CSeq_annot::GetTypeInfo()) {
        const CSeq_annot* annot = dynamic_cast < const CSeq_annot* > (&obj);
        PostErr (sv, et, msg, *annot);
    } else if (type_info == CSeq_graph::GetTypeInfo()) {
        const CSeq_graph* graph = dynamic_cast < const CSeq_graph* > (&obj);
        PostErr (sv, et, msg, *graph);
    } else if (type_info == CSeq_align::GetTypeInfo()) {
        const CSeq_align* align = dynamic_cast < const CSeq_align* > (&obj);
        PostErr (sv, et, msg, *align);
    } else if (type_info == CSeq_entry::GetTypeInfo()) {
        const CSeq_entry* entry = dynamic_cast < const CSeq_entry* > (&obj);
        PostErr (sv, et, msg, *entry);
    } else if (type_info == CBioSource::GetTypeInfo()) {
        const CBioSource* src = dynamic_cast < const CBioSource* > (&obj);
        PostErr (sv, et, msg, *src);
    } else if (type_info == COrg_ref::GetTypeInfo()) {
        const COrg_ref* org = dynamic_cast < const COrg_ref* > (&obj);
        PostErr (sv, et, msg, *org);
    } else if (type_info == CPubdesc::GetTypeInfo()) {
        const CPubdesc* pd = dynamic_cast < const CPubdesc* > (&obj);
        PostErr (sv, et, msg, *pd);
    } else if (type_info == CSeq_submit::GetTypeInfo()) {
        const CSeq_submit* ss = dynamic_cast < const CSeq_submit* > (&obj);
        PostErr (sv, et, msg, *ss);
    } else {
        ERR_POST_X(1, Warning << "Unknown data type in PostErr.");
    }
}


/*
void CValidError_imp::PostErr
(EDiagSev       sv,
 EErrType       et,
 const string&  msg,
 TDesc          ds)
{
    // Append Descriptor label
    string desc = "DESCRIPTOR: ";
    ds.GetLabel (&desc, CSeqdesc::eBoth);
    desc += ", NO Descriptor Context";
    m_ErrRepository->AddValidErrItem(sv, et, msg, desc, ds, *m_Scope);
}
*/

static const EErrType sc_ValidGenomeRaise[] = {
    eErr_SEQ_INST_ShortSeq,
    eErr_SEQ_INST_ConflictingBiomolTech,
    eErr_SEQ_INST_DuplicateSegmentReferences,
    eErr_SEQ_INST_BadSeqIdFormat,
    eErr_SEQ_INST_TerminalNs,
    eErr_SEQ_INST_UnexpectedIdentifierChange,
    eErr_SEQ_INST_TpaAssemblyProblem,
    eErr_SEQ_INST_SeqLocLength,
    eErr_SEQ_INST_CompleteTitleProblem,
    eErr_SEQ_INST_BadHTGSeq,
    eErr_SEQ_INST_OverlappingDeltaRange,
    eErr_SEQ_INST_InternalNsInSeqRaw,
    eErr_SEQ_INST_FarFetchFailure,
    eErr_SEQ_INST_InternalGapsInSeqRaw,
    eErr_SEQ_INST_HighNContentStretch,
    eErr_SEQ_INST_UnknownLengthGapNot100,
    eErr_SEQ_INST_CompleteGenomeHasGaps,
    eErr_SEQ_DESCR_BioSourceMissing,
    eErr_SEQ_DESCR_InvalidForType,
    eErr_SEQ_DESCR_InconsistentBioSources,
    eErr_SEQ_DESCR_BadOrganelleLocation,
    eErr_SEQ_DESCR_MultipleChromosomes,
    eErr_SEQ_DESCR_BadOrgMod,
    eErr_SEQ_DESCR_Inconsistent,
    eErr_SEQ_DESCR_ObsoleteSourceLocation,
    eErr_SEQ_DESCR_ObsoleteSourceQual,
    eErr_SEQ_DESCR_UnwantedCompleteFlag,
    eErr_SEQ_DESCR_CollidingPublications,
    eErr_SEQ_DESCR_TransgenicProblem,
    eErr_SEQ_DESCR_BioSourceInconsistency,
    eErr_SEQ_DESCR_BadCollectionDate,
    eErr_SEQ_DESCR_BadPCRPrimerSequence,
    eErr_SEQ_DESCR_BioSourceOnProtein,
    eErr_SEQ_DESCR_BioSourceDbTagConflict,
    eErr_SEQ_DESCR_DuplicatePCRPrimerSequence,
    eErr_SEQ_DESCR_MultipleNames,
    eErr_SEQ_DESCR_LatLonRange,
    eErr_SEQ_DESCR_LatLonValue,
    eErr_SEQ_DESCR_LatLonCountry,
    eErr_SEQ_DESCR_BadCollectionCode,
    eErr_SEQ_DESCR_IncorrectlyFormattedVoucherID,
    eErr_SEQ_DESCR_MultipleSourceQualifiers,
    eErr_SEQ_DESCR_IdenticalInstitutionCode,
    eErr_SEQ_DESCR_WrongVoucherType,
    eErr_SEQ_DESCR_BadKeyword,
    eErr_SEQ_DESCR_BioSourceNeedsChromosome,
    eErr_SEQ_DESCR_MolInfoConflictsWithBioSource,
    eErr_SEQ_DESCR_TaxonomyIsSpeciesProblem,
    eErr_SEQ_DESCR_BadAltitude,
    eErr_SEQ_DESCR_DBLinkMissingUserObject,
    eErr_GENERIC_UnnecessaryPubEquiv,
    eErr_GENERIC_CollidingSerialNumbers,
    eErr_GENERIC_PublicationInconsistency,
    eErr_GENERIC_SgmlPresentInText,
    eErr_GENERIC_MissingPubRequirement,
    eErr_SEQ_PKG_EmptySet,
    eErr_SEQ_PKG_FeaturePackagingProblem,
    eErr_SEQ_PKG_GenomicProductPackagingProblem,
    eErr_SEQ_PKG_ArchaicFeatureLocation,
    eErr_SEQ_PKG_ArchaicFeatureProduct,
    eErr_SEQ_PKG_InternalGenBankSet,
    eErr_SEQ_PKG_BioseqSetClassNotSet,
    eErr_SEQ_PKG_MissingSetTitle,
    eErr_SEQ_PKG_NucProtSetHasTitle,
    eErr_SEQ_PKG_ComponentMissingTitle,
    eErr_SEQ_PKG_SingleItemSet,
    eErr_SEQ_PKG_MisplacedMolInfo,
    eErr_SEQ_PKG_ImproperlyNestedSets,
    eErr_SEQ_PKG_SeqSubmitWithWgsSet,
    eErr_SEQ_PKG_InconsistentMoltypeSet,
    eErr_SEQ_FEAT_Range,
    eErr_SEQ_FEAT_MixedStrand,
    eErr_SEQ_FEAT_SeqLocOrder,
    eErr_SEQ_FEAT_TransLen,
    eErr_SEQ_FEAT_TranslExcept,
    eErr_SEQ_FEAT_OrfCdsHasProduct,
    eErr_SEQ_FEAT_GeneRefHasNoData,
    eErr_SEQ_FEAT_ProtRefHasNoData,
    eErr_SEQ_FEAT_RNAtype0,
    eErr_SEQ_FEAT_UnknownImpFeatKey,
    eErr_SEQ_FEAT_UnknownImpFeatQual,
    eErr_SEQ_FEAT_WrongQualOnImpFeat,
    eErr_SEQ_FEAT_MissingQualOnImpFeat,
    eErr_SEQ_FEAT_IllegalDbXref,
    eErr_SEQ_FEAT_FarLocation,
    eErr_SEQ_FEAT_TranslExceptPhase,
    eErr_SEQ_FEAT_PeptideFeatOutOfFrame,
    eErr_SEQ_FEAT_InvalidQualifierValue,
    eErr_SEQ_FEAT_CDSproductPackagingProblem,
    eErr_SEQ_FEAT_DuplicateExonInterval,
    eErr_SEQ_FEAT_DuplicateAnticodonInterval,
    eErr_SEQ_FEAT_AbuttingIntervals,
    eErr_SEQ_FEAT_MissingCDSproduct,
    eErr_SEQ_FEAT_OnlyGeneXrefs,
    eErr_SEQ_FEAT_UTRdoesNotAbutCDS,
    eErr_SEQ_FEAT_ConflictFlagSet,
    eErr_SEQ_FEAT_LocusTagProblem,
    eErr_SEQ_FEAT_GenesInconsistent,
    eErr_SEQ_FEAT_TranslExceptAndRnaEditing,
    eErr_SEQ_FEAT_NoNameForProtein,
    eErr_SEQ_FEAT_MissingGeneXref,
    eErr_SEQ_FEAT_FeatureCitationProblem,
    eErr_SEQ_FEAT_WrongQualOnFeature,
    eErr_SEQ_FEAT_UnknownFeatureQual,
    eErr_SEQ_FEAT_BadCharInAuthorName,
    eErr_SEQ_FEAT_CDSwithMultipleMRNAs,
    eErr_SEQ_FEAT_MultipleEquivBioSources,
    eErr_SEQ_FEAT_MultipleEquivPublications,
    eErr_SEQ_FEAT_BadFullLengthFeature,
    eErr_SEQ_FEAT_RedundantFields,
    eErr_SEQ_FEAT_CDSwithNoMRNAOverlap,
    eErr_SEQ_FEAT_FeatureProductInconsistency,
    eErr_SEQ_FEAT_ImproperBondLocation,
    eErr_SEQ_FEAT_GeneXrefWithoutGene,
    eErr_SEQ_FEAT_MissingTrnaAA,
    eErr_SEQ_FEAT_OldLocusTagMismtach,
    eErr_SEQ_FEAT_InvalidInferenceValue,
    eErr_SEQ_FEAT_HypotheticalProteinMismatch,
    eErr_SEQ_FEAT_WholeLocation,
    eErr_SEQ_FEAT_BadEcNumberFormat,
    eErr_SEQ_FEAT_EcNumberProblem,
    eErr_SEQ_FEAT_VectorContamination,
    eErr_SEQ_FEAT_MinusStrandProtein,
    eErr_SEQ_FEAT_BadProteinName,
    eErr_SEQ_FEAT_GeneXrefWithoutLocus,
    eErr_SEQ_FEAT_CDShasTooManyXs,
    eErr_SEQ_FEAT_TerminalXDiscrepancy,
    eErr_SEQ_FEAT_UnnecessaryTranslExcept,
    eErr_SEQ_FEAT_FeatureInsideGap,
    eErr_SEQ_FEAT_BadAnticodonAA,
    eErr_SEQ_FEAT_BadAnticodonCodon,
    eErr_SEQ_FEAT_FeatureBeginsOrEndsInGap,
    eErr_SEQ_FEAT_GeneOntologyTermMissingGOID,
    eErr_SEQ_FEAT_PseudoRnaHasProduct,
    eErr_SEQ_FEAT_PseudoRnaViaGeneHasProduct,
    eErr_SEQ_FEAT_BadRRNAcomponentOrder,
    eErr_SEQ_FEAT_BadRRNAcomponentOverlap,
    eErr_SEQ_FEAT_MultipleProtRefs,
    eErr_SEQ_FEAT_BadInternalCharacter,
    eErr_SEQ_FEAT_BadTrailingCharacter,
    eErr_SEQ_FEAT_BadTrailingHyphen,
    eErr_SEQ_FEAT_BadCharInAuthorLastName,
    eErr_SEQ_FEAT_GeneXrefNeeded,
    eErr_SEQ_FEAT_ProteinNameHasPMID,
    eErr_SEQ_FEAT_BadGeneOntologyFormat,
    eErr_SEQ_FEAT_InconsistentGeneOntologyTermAndId,
    eErr_SEQ_FEAT_ShortIntron,
    eErr_SEQ_FEAT_GeneXrefStrandProblem,
    eErr_SEQ_FEAT_CDSmRNAXrefLocationProblem,
    eErr_SEQ_FEAT_LocusCollidesWithLocusTag,
    eErr_SEQ_FEAT_RptUnitRangeProblem,
    eErr_SEQ_FEAT_InconsistentRRNAstrands,
    eErr_SEQ_FEAT_CDSrange,
    eErr_SEQ_GRAPH_GraphAbove,
    eErr_SEQ_GRAPH_GraphOutOfOrder,
    eErr_SEQ_GRAPH_GraphSeqLocLen,
    eErr_SEQ_GRAPH_GraphBioseqId
};

DEFINE_STATIC_ARRAY_MAP(CStaticArraySet<EErrType>, sc_GenomeRaiseArray, sc_ValidGenomeRaise);

static const EErrType sc_ValidGenomeRaiseExceptEmblDdbj[] = {
    eErr_SEQ_INST_CompleteTitleProblem,
    eErr_SEQ_INST_CompleteGenomeHasGaps,
    eErr_SEQ_FEAT_MiscFeatureNeedsNote,
    eErr_SEQ_FEAT_RepeatRegionNeedsNote
};

DEFINE_STATIC_ARRAY_MAP(CStaticArraySet<EErrType>, sc_GenomeRaiseExceptEmblDdbjArray, sc_ValidGenomeRaiseExceptEmblDdbj);


static const EErrType sc_ValidGenomeRaiseExceptEmblDdbjRefSeq[] = {
    eErr_SEQ_DESCR_BadInstitutionCode
};

DEFINE_STATIC_ARRAY_MAP(CStaticArraySet<EErrType>, sc_GenomeRaiseExceptEmblDdbjRefSeqArray, sc_ValidGenomeRaiseExceptEmblDdbjRefSeq);


bool CValidError_imp::RaiseGenomeSeverity(
    EErrType et
)

{
    if (sc_GenomeRaiseExceptEmblDdbjRefSeqArray.find(et) != sc_GenomeRaiseExceptEmblDdbjRefSeqArray.end()) {
        if (IsEmbl() || IsDdbj() || IsRefSeq()) {
            return false;
        } else {
            return true;
        }
    }
    if (sc_GenomeRaiseExceptEmblDdbjArray.find(et) != sc_GenomeRaiseExceptEmblDdbjArray.end()) {
        if (IsEmbl() || IsDdbj()) {
            return false;
        } else {
            return true;
        }
    }
    if (sc_GenomeRaiseArray.find (et) != sc_GenomeRaiseArray.end()) {
        return true;
    }
    return false;
}

void CValidError_imp::PostErr
(EDiagSev       sv,
 EErrType       et,
 const string&  msg,
 TFeat          ft)
{

    if (x_IsSuppressed(et)) {
        return;
    }

    CRef<CValidErrItem> item(new CValidErrItem());

    // Adjust severity
    if (m_genomeSubmission && RaiseGenomeSeverity(et) && sv < eDiag_Error) {
        sv = eDiag_Error;
    }

    item->SetSev(sv);
    item->SetErrIndex(et);
    item->SetMsg(msg);
    item->SetObject(ft);

    if (GenerateGoldenFile()) {
        m_ErrRepository->AddValidErrItem(item);
        return;
    }

    string content_label = CValidErrorFormat::GetFeatureContentLabel(ft, m_Scope);
    item->SetObj_content(content_label);

    string feature_id = CValidErrorFormat::GetFeatureIdLabel(ft);
    if (!NStr::IsBlank(feature_id)) {
        item->SetFeatureId(feature_id);
    }

    string bioseq_label = CValidErrorFormat::GetFeatureBioseqLabel(ft, m_Scope, m_SuppressContext);
    if (!NStr::IsBlank(bioseq_label)) {
        item->SetBioseq(bioseq_label);
    }

    // Calculate sequence offset
    TSeqPos offset = 0;
    string location;
    if (ft.IsSetLocation()) {
        offset = ft.GetLocation().GetStart(eExtreme_Positional);
        string loc_label = CValidErrorFormat::GetFeatureLocationLabel(ft, m_Scope, m_SuppressContext);
        if (!NStr::IsBlank(loc_label)) {
            item->SetLocation(loc_label);
        }
        item->SetSeqOffset(offset);
    }


    string product_label = CValidErrorFormat::GetFeatureProductLocLabel(ft, m_Scope, m_SuppressContext);
    if (!NStr::IsBlank(product_label)) {
        item->SetProduct_loc(product_label);
    }

    int version = 0;
    string accession;
    if (m_Scope) {
        accession = GetAccessionFromObjects(&ft, nullptr, *m_Scope, &version);
    }
    item->SetAccession(accession);
    if (version > 0) {
        item->SetAccnver(accession + "." + NStr::IntToString(version));
        item->SetVersion(version);
    } else {
        item->SetAccnver(accession);
    }

    if (ft.IsSetData()) {
        if (ft.GetData().IsGene()) {
            if (ft.GetData().GetGene().IsSetLocus_tag() &&
                !NStr::IsBlank(ft.GetData().GetGene().GetLocus_tag())) {
                item->SetLocus_tag(ft.GetData().GetGene().GetLocus_tag());
            }
        } else {
            if (m_CollectLocusTags) {
                // TODO: this should be part of post-processing
                CConstRef<CSeq_feat> gene = GetGeneCache().GetGeneFromCache(&ft, *m_Scope);
                if (gene && gene->GetData().GetGene().IsSetLocus_tag() &&
                    !NStr::IsBlank(gene->GetData().GetGene().GetLocus_tag())) {
                    item->SetLocus_tag(gene->GetData().GetGene().GetLocus_tag());
                }
            }
        }
    }

    item->SetFeatureObjDescFromFields();
    m_ErrRepository->AddValidErrItem(item);
}


void CValidError_imp::PostErr
(EDiagSev       sv,
 EErrType       et,
 const string&  msg,
 TBioseq        sq)
{
    if (x_IsSuppressed(et)) {
        return;
    }

    // Adjust severity
    if (m_genomeSubmission && sv < eDiag_Error && RaiseGenomeSeverity(et)) {
        sv = eDiag_Error;
    }

    if (GenerateGoldenFile()) {
        m_ErrRepository->AddValidErrItem(sv, et, msg);
        return;
    }

    // Append bioseq label
    string desc;
    AppendBioseqLabel(desc, sq, m_SuppressContext);
    int version = 0;
    const string& accession = GetAccessionFromBioseq(sq, &version);
    // GetAccessionFromObjects(&sq, nullptr, *m_Scope, &version);
    x_AddValidErrItem(sv, et, msg, desc, sq, accession, version);
}


void CValidError_imp::PostErr
(EDiagSev      sv,
 EErrType      et,
 const string& msg,
 TSet          st)
{
    if (x_IsSuppressed(et)) {
        return;
    }

    // Adjust severity
    if (m_genomeSubmission && RaiseGenomeSeverity(et) && sv < eDiag_Error) {
        sv = eDiag_Error;
    }

    if (GenerateGoldenFile()) {
        m_ErrRepository->AddValidErrItem(sv, et, msg);
        return;
    }

    // Append Bioseq_set label

    const auto isSetClass = st.IsSetClass();

    if (isSetClass && GetContext().PreprocessHugeFile) {
        if (auto setClass = st.GetClass(); IsHugeSet(setClass)) {
            string desc =
            CValidErrorFormat::GetBioseqSetLabel(GetContext().HugeSetId, setClass, m_SuppressContext);
            x_AddValidErrItem(sv, et, msg, desc, st, GetContext().HugeSetId, 0);
            return;
        }
    }

    int version = 0;
    const string& accession = GetAccessionFromBioseqSet(st, &version);
    //string desc = CValidErrorFormat::GetBioseqSetLabel(st, m_SuppressContext);
    string desc = CValidErrorFormat::GetBioseqSetLabel(accession,
            isSetClass ? st.GetClass() : CBioseq_set::eClass_not_set,
            isSetClass ? m_SuppressContext : true);
    x_AddValidErrItem(sv, et, msg, desc, st, accession, version);
}


void CValidError_imp::PostErr
(EDiagSev       sv,
 EErrType       et,
 const string&  msg,
 TEntry         ctx,
 TDesc          ds)
{
    if (x_IsSuppressed(et)) {
        return;
    }

    // Adjust severity
    if (m_genomeSubmission && RaiseGenomeSeverity(et) && sv < eDiag_Error) {
        sv = eDiag_Error;
    }

    if (GenerateGoldenFile()) {
        m_ErrRepository->AddValidErrItem(sv, et, msg);
        return;
    }


    if (GetContext().PreprocessHugeFile &&
        ctx.IsSet() && ctx.GetSet().IsSetClass()) {
        if (auto setClass = ctx.GetSet().GetClass(); IsHugeSet(setClass)) {
            string desc{"DESCRIPTOR: "};
            desc += CValidErrorFormat::GetDescriptorContent(ds) + " ";
            desc += "BIOSEQ-SET: ";
            if (!m_SuppressContext) {
                if (setClass == CBioseq_set::eClass_genbank) {
                    desc += "genbank: ";
                }
                else {
                    desc += "wgs-set: ";
                }
            }
            desc += GetContext().HugeSetId;
            m_ErrRepository->AddValidErrItem(sv, et, msg, desc, ds, GetContext().HugeSetId, 0);
            return;
        }
    }

    // Append Descriptor label
    string desc = CValidErrorFormat::GetDescriptorLabel(ds, ctx, m_Scope, m_SuppressContext);
    int version = 0;
    const string& accession = GetAccessionFromObjects(&ds, &ctx, *m_Scope, &version);
    m_ErrRepository->AddValidErrItem(sv, et, msg, desc, ds, ctx, accession, version);
}


//void CValidError_imp::PostErr
//(EDiagSev       sv,
// EErrType       et,
// const string&  msg,
// TBioseq        sq,
// TDesc          ds)
//{
//    // Append Descriptor label
//    string desc("DESCRIPTOR: ");
//    ds.GetLabel(&desc, CSeqdesc::eBoth);
//
//    s_AppendBioseqLabel(desc, sq, m_SuppressContext);
//    m_ErrRepository->AddValidErrItem(sv, et, msg, desc, ds, *m_Scope);
//    //PostErr(sv, et, msg, sq);
//}


//void CValidError_imp::PostErr
//(EDiagSev        sv,
// EErrType        et,
// const string&   msg,
// TSet            st,
// TDesc           ds)
//{
//    // Append Descriptor label
//    string desc =  " DESCRIPTOR: ";
//    ds.GetLabel(&desc, CSeqdesc::eBoth);
//    s_AppendSetLabel(desc, st, m_SuppressContext);
//    m_ErrRepository->AddValidErrItem(sv, et, msg, desc, st, *m_Scope);
//
//}


void CValidError_imp::PostErr
(EDiagSev       sv,
 EErrType       et,
 const string&  msg,
 TAnnot         an)
{
    if (x_IsSuppressed(et)) {
        return;
    }

    // Adjust severity
    if (m_genomeSubmission && RaiseGenomeSeverity(et) && sv < eDiag_Error) {
        sv = eDiag_Error;
    }

    if (GenerateGoldenFile()) {
        m_ErrRepository->AddValidErrItem(sv, et, msg);
        return;
    }

    // Append Annotation label
    string desc = "ANNOTATION: ";

    // !!! need to decide on the message

    int version = 0;
    const string& accession = GetAccessionFromObjects(&an, nullptr, *m_Scope, &version);
    x_AddValidErrItem(sv, et, msg, desc, an, accession, version);
}


void CValidError_imp::PostErr
(EDiagSev       sv,
 EErrType       et,
 const string&  msg,
 TGraph         graph)
{

    if (x_IsSuppressed(et)) {
        return;
    }

    // Adjust severity
    if (m_genomeSubmission && RaiseGenomeSeverity(et) && sv < eDiag_Error) {
        sv = eDiag_Error;
    }

    if (GenerateGoldenFile()) {
        m_ErrRepository->AddValidErrItem(sv, et, msg);
        return;
    }

    // Append Graph label
    string desc = "GRAPH: ";
    if (graph.IsSetTitle()) {
        desc += graph.GetTitle();
    } else {
        desc += "<Unnamed>";
    }
    desc += " ";
    graph.GetLoc().GetLabel(&desc);

    int version = 0;
    const string& accession = GetAccessionFromObjects(&graph, nullptr, *m_Scope, &version);
    x_AddValidErrItem(sv, et, msg, desc, graph, accession, version);
}


void CValidError_imp::PostErr
(EDiagSev       sv,
 EErrType       et,
 const string&  msg,
 TBioseq        sq,
 TGraph         graph)
{

    if (x_IsSuppressed(et)) {
        return;
    }

    // Adjust severity
    if (m_genomeSubmission && RaiseGenomeSeverity(et) && sv < eDiag_Error) {
        sv = eDiag_Error;
    }

    if (GenerateGoldenFile()) {
        m_ErrRepository->AddValidErrItem(sv, et, msg);
        return;
    }

    // Append Graph label
    string desc("GRAPH: ");
    if ( graph.IsSetTitle() ) {
        desc += graph.GetTitle();
    } else {
        desc += "<Unnamed>";
    }
    desc += " ";
    graph.GetLoc().GetLabel(&desc);
    AppendBioseqLabel(desc, sq, m_SuppressContext);
    int version = 0;
    const string& accession = GetAccessionFromObjects(&graph, nullptr, *m_Scope, &version);
    x_AddValidErrItem(sv, et, msg, desc, graph, accession, version);
}


void CValidError_imp::PostErr
(EDiagSev      sv,
 EErrType      et,
 const string& msg,
 TAlign        align)
{

    if (x_IsSuppressed(et)) {
        return;
    }

    // Adjust severity
    if (m_genomeSubmission && RaiseGenomeSeverity(et) && sv < eDiag_Error) {
        sv = eDiag_Error;
    }

    if (GenerateGoldenFile()) {
        m_ErrRepository->AddValidErrItem(sv, et, msg);
        return;
    }

    CConstRef<CSeq_id> id = GetReportableSeqIdForAlignment(align, *m_Scope);
    if (id) {
        CBioseq_Handle bsh = m_Scope->GetBioseqHandle(*id);
        if (bsh) {
            PostErr(sv, et, msg, *(bsh.GetCompleteBioseq()));
            return;
        }
    }

    // Can't get bioseq for reporting, use other Alignment label
    string desc = "ALIGNMENT: ";
    if (align.IsSetType()) {
        desc += align.ENUM_METHOD_NAME(EType)()->FindName(align.GetType(), true);
    }
    try {
        CSeq_align::TDim dim = align.GetDim();
        desc += ", dim=" + NStr::NumericToString(dim);
    } catch ( const CUnassignedMember& ) {
        desc += ", dim=UNASSIGNED";
    }

    if (align.IsSetSegs()) {
        desc += " SEGS: ";
        desc += align.GetSegs().SelectionName(align.GetSegs().Which());
    }

    int version = 0;
    const string& accession = GetAccessionFromObjects(&align, nullptr, *m_Scope, &version);
    x_AddValidErrItem(sv, et, msg, desc, align, accession, version);
}


void CValidError_imp::PostErr
(EDiagSev      sv,
 EErrType      et,
 const string& msg,
 TEntry        entry)
{
    if (x_IsSuppressed(et)) {
        return;
    }

    // Adjust severity
    if (m_genomeSubmission && RaiseGenomeSeverity(et) && sv < eDiag_Error) {
        sv = eDiag_Error;
    }

    if (GenerateGoldenFile()) {
        m_ErrRepository->AddValidErrItem(sv, et, msg);
        return;
    }

    if (entry.IsSeq()) {
        PostErr(sv, et, msg, entry.GetSeq());
    } else if (entry.IsSet()) {
        PostErr(sv, et, msg, entry.GetSet());
    } else {
        string desc = "SEQ-ENTRY: ";
        entry.GetLabel(&desc, CSeq_entry::eContent);

        int version = 0;
        const string& accession = GetAccessionFromObjects(&entry, nullptr, *m_Scope, &version);
        x_AddValidErrItem(sv, et, msg, desc, entry, accession, version);
    }
}


void CValidError_imp::PostErr
(EDiagSev      sv,
 EErrType      et,
 const string& msg,
 const CBioSource& src)
{

    if (x_IsSuppressed(et)) {
        return;
    }

    // Adjust severity
    if (m_genomeSubmission && RaiseGenomeSeverity(et) && sv < eDiag_Error) {
        sv = eDiag_Error;
    }

    if (GenerateGoldenFile()) {
        m_ErrRepository->AddValidErrItem(sv, et, msg);
        return;
    }

    string desc = "BioSource: ";
    x_AddValidErrItem(sv, et, msg, desc, src, "", 0);
}


void CValidError_imp::PostErr
(EDiagSev      sv,
 EErrType      et,
 const string& msg,
 const COrg_ref& org)
{

    if (x_IsSuppressed(et)) {
        return;
    }

    // Adjust severity
    if (m_genomeSubmission && RaiseGenomeSeverity(et) && sv < eDiag_Error) {
        sv = eDiag_Error;
    }

    if (GenerateGoldenFile()) {
        m_ErrRepository->AddValidErrItem(sv, et, msg);
        return;
    }

    string desc = "Org-ref: ";
    x_AddValidErrItem(sv, et, msg, desc, org, "", 0);
}


void CValidError_imp::PostErr
(EDiagSev      sv,
 EErrType      et,
 const string& msg,
 const CPubdesc& pd)
{
    if (x_IsSuppressed(et)) {
        return;
    }

    // Adjust severity
    if (m_genomeSubmission && RaiseGenomeSeverity(et) && sv < eDiag_Error) {
        sv = eDiag_Error;
    }

    if (GenerateGoldenFile()) {
        m_ErrRepository->AddValidErrItem(sv, et, msg);
        return;
    }

    string desc = "Pubdesc: ";
    x_AddValidErrItem(sv, et, msg, desc, pd, "", 0);
}


void CValidError_imp::PostErr
(EDiagSev      sv,
 EErrType      et,
 const string& msg,
 const CSeq_submit& ss)
{
    if (x_IsSuppressed(et)) {
        return;
    }

    // Adjust severity
    if (m_genomeSubmission && RaiseGenomeSeverity(et) && sv < eDiag_Error) {
        sv = eDiag_Error;
    }

    if (GenerateGoldenFile()) {
        m_ErrRepository->AddValidErrItem(sv, et, msg);
        return;
    }

    string desc = "Seq-submit: ";
    x_AddValidErrItem(sv, et, msg, desc, ss, "", 0);
}


void CValidError_imp::x_AddValidErrItem(
        EDiagSev sev,
        EErrType type,
        const string& msg,
        const string& desc,
        const CSerialObject& obj,
        const string& accession,
        const int version)
{
    if (IsHugeFileMode()) {
        m_ErrRepository->AddValidErrItem(sev, type, msg, desc, accession, version);
        return;
    }
    m_ErrRepository->AddValidErrItem(sev, type, msg, desc, obj, accession, version);
}


void CValidError_imp::PostObjErr
(EDiagSev sv,
 EErrType et,
 const string&  msg,
 const CSerialObject& obj,
 const CSeq_entry *ctx)
{
    if (!ctx) {
        PostErr (sv, et, msg, obj);
    } else if (obj.GetThisTypeInfo() == CSeqdesc::GetTypeInfo()) {
        PostErr(sv, et, msg, *ctx, *(dynamic_cast <const CSeqdesc*> (&obj)));
    } else {
        PostErr(sv, et, msg, obj);
    }

}


void CValidError_imp::PostBadDateError
(EDiagSev             sv,
 const string&        msg,
 int                  flags,
 const CSerialObject& obj,
 const CSeq_entry *ctx)
{
    string reasons = GetDateErrorDescription(flags);

    NStr::TruncateSpacesInPlace (reasons);
    reasons = msg + " - " + reasons;

    PostObjErr (sv, eErr_GENERIC_BadDate, reasons, obj, ctx);
}


bool CValidError_imp::Validate
(const CSeq_entry& se,
 const CCit_sub* cs,
 CScope* scope)
{
    CSeq_entry_Handle seh;
    try {
        seh = scope->GetSeq_entryHandle(se);
    } catch (const CException& ) { ; }
    if (! seh) {
        seh = scope->AddTopLevelSeqEntry(se);
        if (!seh) {
            return false;
        }
    }

    return Validate(seh, cs);
}

static bool s_IsPhage(const COrg_ref& org)
{
    if (org.IsSetDivision() && NStr::Equal(org.GetDivision(), "PHG")) {
        return true;
    } else {
        return false;
    }
}


void CValidError_imp::ValidateMultipleTaxIds(const CSeq_entry_Handle& seh)
{
    bool has_mult = false;
    int first_id = 0;
    int phage_id = 0;

    for (CBioseq_CI bi(seh); bi; ++bi) {
        for (CSeqdesc_CI desc_ci(*bi, CSeqdesc::e_Source);
            desc_ci && !has_mult;
            ++desc_ci) {
            if (desc_ci->GetSource().IsSetOrg()) {
                const COrg_ref& org = desc_ci->GetSource().GetOrg();
                if (org.IsSetDb()) {
                    ITERATE(COrg_ref::TDb, it, org.GetDb()) {
                        if ((*it)->IsSetDb() && NStr::EqualNocase((*it)->GetDb(), "taxon") &&
                            (*it)->IsSetTag() && (*it)->GetTag().IsId()) {
                            int this_id = (*it)->GetTag().GetId();
                            if (this_id > 0) {
                                if (s_IsPhage(org)) {
                                    phage_id = this_id;
                                } else if (first_id == 0) {
                                    first_id = this_id;
                                } else if (first_id != this_id) {
                                    has_mult = true;
                                }
                            }
                        }
                    }
                }
            }
        }
    }
    if (has_mult || (phage_id > 0 && first_id > 0)) {
        PostErr(has_mult ? eDiag_Error : eDiag_Warning, eErr_SEQ_DESCR_MultipleTaxonIDs,
            "There are multiple taxonIDs in this RefSeq record.",
            *m_TSE);
    }
}


const CValidatorEntryInfo& CValidError_imp::GetEntryInfo() const
{
    return *m_pEntryInfo;
}


CValidatorEntryInfo& CValidError_imp::x_SetEntryInfo()
{
    if (!m_pEntryInfo) {
        m_pEntryInfo.reset(new CValidatorEntryInfo());
    }

    return *m_pEntryInfo;
}


bool CValidError_imp::Validate
(const CSeq_entry_Handle& seh,
 const CCit_sub* cs)
{
    _ASSERT(seh);

    if ( m_PrgCallback ) {
        m_PrgInfo.m_State = CValidator::CProgressInfo::eState_Initializing;
        if ( m_PrgCallback(&m_PrgInfo) ) {
            return false;
        }
    }

    // Check that CSeq_entry has data
    if (seh.Which() == CSeq_entry::e_not_set) {
        ERR_POST_X(2, Warning << "Seq_entry not set");
        return false;
    }

    Setup(seh);

    // Seq-submit has submission citationTest_Descr_LatLonValue
    if (cs) {
        x_SetEntryInfo().SetNoPubs(false);
        x_SetEntryInfo().SetSeqSubmit();
    }

    // Get first CBioseq object pointer for PostErr below.
    CTypeConstIterator<CBioseq> seq(ConstBegin(*m_TSE));
    if (!seq) {
        PostErr(eDiag_Error, eErr_SEQ_PKG_NoBioseqFound,
                  "No Bioseqs in this entire record.", seh.GetCompleteSeq_entry()->GetSet());
        return true;
    }

    // If m_NonASCII is true, then this flag was set by the caller
    // of validate to indicate that a non ascii character had been
    // read from a file being used to create a CSeq_entry, that the
    // error had been corrected, but that the error needs to be reported
    // by Validate. Note, Validate is not doing anything other than
    // reporting an error if m_NonASCII is true;
    if (m_NonASCII) {
        PostErr(eDiag_Fatal, eErr_GENERIC_NonAsciiAsn,
                  "Non-ascii chars in input ASN.1 strings", *seq);
        // Only report the error once
        m_NonASCII = false;
    }

    // Iterate thru components of record and validate each

    // also want to know if we have gi
    bool has_gi = false;
    // also want to know if there are any nucleotide sequences
    bool has_nucleotide_sequence = false;

    for (CBioseq_CI bi(GetTSEH(), CSeq_inst::eMol_not_set, CBioseq_CI::eLevel_All);
         bi && (!IsINSDInSep() || !has_gi || !has_nucleotide_sequence);
         ++bi) {
        FOR_EACH_SEQID_ON_BIOSEQ (it, *(bi->GetCompleteBioseq())) {
            if ((*it)->IsGi()) {
                has_gi = true;
            }
        }
        if (bi->IsSetInst_Mol() && bi->IsNa()) {
            has_nucleotide_sequence = true;
        }
    }

    if (IsINSDInSep() && IsRefSeq()) {
        // NOTE: We use m_IsRefSeq to indicate the actual presence of RefSeq IDs in
        // the record, rather than IsRefSeq(), which indicates *either* RefSeq IDs are
        // present *OR* the refseq flag has been used
        PostErr (eDiag_Error, eErr_SEQ_PKG_INSDRefSeqPackaging,
                 "INSD and RefSeq records should not be present in the same set", *m_TSE);
    }

#if 0
    // disabled for now
    // look for long IDs that would collide if truncated at 30 characters
    vector<string> id_strings;
    for (CBioseq_CI bi(GetTSEH(), CSeq_inst::eMol_not_set, CBioseq_CI::eLevel_All);
         bi;
         ++bi) {
        FOR_EACH_SEQID_ON_BIOSEQ (it, *(bi->GetCompleteBioseq())) {
            if (!IsNCBIFILESeqId(**it)) {
                string label;
                (*it)->GetLabel(&label);
                id_strings.push_back(label);
            }
        }
    }
    stable_sort (id_strings.begin(), id_strings.end());
    for (vector<string>::iterator id_str_it = id_strings.begin();
         id_str_it != id_strings.end();
         ++id_str_it) {
        string pattern = (*id_str_it).substr(0, 30);
        string first_id = *id_str_it;
        vector<string>::iterator cmp_it = id_str_it;
        ++cmp_it;
        while (cmp_it != id_strings.end() && NStr::StartsWith(*cmp_it, pattern)) {
            CRef<CSeq_id> id(new CSeq_id(*cmp_it));
            CBioseq_Handle bsh = m_Scope->GetBioseqHandle(*id);
            PostErr (eDiag_Warning, eErr_SEQ_INST_BadSeqIdFormat,
                     "First 30 characters of " + first_id + " and " +
                     *cmp_it + " are identical", *(bsh.GetCompleteBioseq()));
            ++id_str_it;
            ++cmp_it;
        }
    }
#endif

    // look for colliding feature IDs
    vector < int > feature_ids;
    for (CFeat_CI fi(GetTSEH()); fi; ++fi) {
        const CSeq_feat& sf = fi->GetOriginalFeature();
        if (sf.IsSetId() && sf.GetId().IsLocal() && sf.GetId().GetLocal().IsId()) {
            feature_ids.push_back(sf.GetId().GetLocal().GetId());
        }
    }

    if (feature_ids.size() > 0) {
        const CTSE_Handle& tse = seh.GetTSE_Handle ();
        stable_sort (feature_ids.begin(), feature_ids.end());
        vector <int>::iterator it = feature_ids.begin();
        int id = *it;
        ++it;
        while (it != feature_ids.end()) {
            if (*it == id) {
                vector<CSeq_feat_Handle> handles = tse.GetFeaturesWithId(CSeqFeatData::e_not_set, id);
                ITERATE( vector<CSeq_feat_Handle>, feat_it, handles ) {
                    PostErr (eDiag_Critical, eErr_SEQ_FEAT_CollidingFeatureIDs,
                             "Colliding feature ID " + NStr::NumericToString (id), *(feat_it->GetSeq_feat()));
                }
                while (it != feature_ids.end() && *it == id) {
                    ++it;
                }
                if (it != feature_ids.end()) {
                    id = *it;
                    ++it;
                }
            } else {
                id = *it;
                ++it;
            }
        }
    }

    // look for mixed gps and non-gps sets
    bool has_nongps = false;
    bool has_gps = false;

    for (CTypeConstIterator<CBioseq_set> si(*m_TSE); si && (!has_nongps || !has_gps); ++si) {
        if (si->IsSetClass()) {
            if (si->GetClass() == CBioseq_set::eClass_mut_set
                || si->GetClass() == CBioseq_set::eClass_pop_set
                || si->GetClass() == CBioseq_set::eClass_phy_set
                || si->GetClass() == CBioseq_set::eClass_eco_set
                || si->GetClass() == CBioseq_set::eClass_wgs_set
                || si->GetClass() == CBioseq_set::eClass_small_genome_set) {
                has_nongps = true;
            } else if (si->GetClass() == CBioseq_set::eClass_gen_prod_set) {
                has_gps = true;
            }
        }
    }

    if (has_nongps && has_gps) {
        PostErr(eDiag_Error, eErr_SEQ_PKG_GPSnonGPSPackaging,
            "Genomic product set and mut/pop/phy/eco set records should not be present in the same set",
            *m_TSE);
    }

    // count inference accessions - if there are too many, WAS temporarily disable inference checking
    // now disable inference checking for rest of this validator run
    bool old_inference_acc_check = m_ValidateInferenceAccessions;
    if (m_CumulativeInferenceCount >= InferenceAccessionCutoff && ! m_ForceInferences) {
        m_IgnoreInferences = true;
    }
    if (! m_IgnoreInferences) {
        CFeat_CI feat_inf(seh);
        while (feat_inf && ! m_IgnoreInferences) {
            FOR_EACH_GBQUAL_ON_FEATURE (qual, *feat_inf) {
                if (! m_IgnoreInferences && (*qual)->IsSetQual() && (*qual)->IsSetVal() && NStr::Equal((*qual)->GetQual(), "inference")) {
                    if (! GetContext().PreprocessHugeFile) {
                        m_CumulativeInferenceCount++;
                    }
                    if (m_CumulativeInferenceCount >= InferenceAccessionCutoff && ! m_ForceInferences) {
                        // disable inference checking for remainder of run
                        m_IgnoreInferences = true;

                        // warn about too many inferences
                        PostErr (eDiag_Info, eErr_SEQ_FEAT_TooManyInferenceAccessions,
                                 "Skipping validation of remaining /inference qualifiers",
                                 *m_TSE);
                    }

                    if (m_ValidateInferenceAccessions && ! m_IgnoreInferences) {
                        string prefix, remainder;
                        bool same_species;
                        size_t num_accessions = 0;
                        vector<string> accessions = CValidError_feat::GetAccessionsFromInferenceString ((*qual)->GetVal(), prefix, remainder, same_species);
                        for (size_t i = 0; i < accessions.size(); i++) {
                            NStr::TruncateSpacesInPlace (accessions[i]);
                            string acc_prefix, accession;
                            if (CValidError_feat::GetPrefixAndAccessionFromInferenceAccession (accessions[i], acc_prefix, accession)) {
                                num_accessions++;
                                if (! GetContext().PreprocessHugeFile) {
                                    m_CumulativeInferenceCount++;
                                }
                            }
                        }
                        if (num_accessions > 0) {
                            if (! GetContext().PreprocessHugeFile) {
                                m_CumulativeInferenceCount += num_accessions;
                            }
                            if (m_CumulativeInferenceCount >= InferenceAccessionCutoff && ! m_ForceInferences) {
                                // disable inference checking for remainder of run
                                m_IgnoreInferences = true;

                                // warn about too many inferences
                                PostErr (eDiag_Info, eErr_SEQ_FEAT_TooManyInferenceAccessions,
                                         "Skipping validation of remaining /inference qualifiers",
                                         *m_TSE);
                            }
                        }
                    }
                }
            }
            ++feat_inf;
        }
    }

    // validate the main data
    if (seh.IsSeq()) {
        const CBioseq& seq2 = seh.GetCompleteSeq_entry()->GetSeq();
        CValidError_bioseq bioseq_validator(*this);
        try {
            bioseq_validator.ValidateBioseq(seq2);
        } catch ( const exception& e ) {
            PostErr(eDiag_Fatal, eErr_INTERNAL_Exception,
                string("Exception while validating bioseq. EXCEPTION: ") +
                e.what(), seq2);
            return true;
        }
    } else if (seh.IsSet()) {
        const CBioseq_set& set = seh.GetCompleteSeq_entry()->GetSet();
        CValidError_bioseqset bioseqset_validator(*this);

        try {
            bioseqset_validator.ValidateBioseqSet(set);

        } catch ( const exception& e ) {
            PostErr(eDiag_Fatal, eErr_INTERNAL_Exception,
                string("Exception while validating bioseq set. EXCEPTION: ") +
                e.what(), set);
            return true;
        }
    }

    // put flag for validating inference accessions back to original value
    m_ValidateInferenceAccessions = old_inference_acc_check;

    // validation from data collected during previous step

    if (!GetContext().PreprocessHugeFile) {
        if ( m_NumTpaWithHistory > 0  &&
             m_NumTpaWithoutHistory > 0 ) {
            PostErr(eDiag_Error, eErr_SEQ_INST_TpaAssemblyProblem,
                "There are " +
                NStr::SizetToString(m_NumTpaWithHistory) +
                " TPAs with history and " +
                NStr::SizetToString(m_NumTpaWithoutHistory) +
                " without history in this record.", *seq);
        }
        if ( m_NumTpaWithoutHistory > 0 && has_gi) {
            PostErr (eDiag_Warning, eErr_SEQ_INST_TpaAssemblyProblem,
                "There are " +
                NStr::SizetToString(m_NumTpaWithoutHistory) +
                " TPAs without history in this record, but the record has a gi number assignment.", *m_TSE);
        }
    }

    if (IsIndexerVersion() && DoesAnyProteinHaveGeneralID() && !IsRefSeq() && has_nucleotide_sequence) {
        call_once(SetContext().ProteinHaveGeneralIDOnceFlag,
            [](CValidError_imp* imp, CSeq_entry_Handle seh2) {
                imp->PostErr (eDiag_Info, eErr_SEQ_INST_ProteinsHaveGeneralID,
                    "INDEXER_ONLY - Protein bioseqs have general seq-id.",
                    *(seh2.GetCompleteSeq_entry()));
            }, this, seh);
    }

    ReportMissingPubs(*m_TSE, cs);
    ReportMissingBiosource(*m_TSE);

    if (m_NumMisplacedFeatures > 1) {
        PostErr (eDiag_Critical, eErr_SEQ_PKG_FeaturePackagingProblem,
                 "There are " + NStr::SizetToString (m_NumMisplacedFeatures) + " mispackaged features in this record.",
                 *(seh.GetCompleteSeq_entry()));
    } else if (m_NumMisplacedFeatures == 1) {
        PostErr (eDiag_Critical, eErr_SEQ_PKG_FeaturePackagingProblem,
                 "There is 1 mispackaged feature in this record.",
                 *(seh.GetCompleteSeq_entry()));
    }
    if (m_NumSmallGenomeSetMisplaced > 1) {
        PostErr (eDiag_Warning, eErr_SEQ_PKG_FeaturePackagingProblem,
                 "There are " + NStr::SizetToString (m_NumSmallGenomeSetMisplaced) + " mispackaged features in this small genome set record.",
                 *(seh.GetCompleteSeq_entry()));
    } else if (m_NumSmallGenomeSetMisplaced == 1) {
        PostErr (eDiag_Warning, eErr_SEQ_PKG_FeaturePackagingProblem,
                 "There is 1 mispackaged feature in this small genome set record.",
                 *(seh.GetCompleteSeq_entry()));
    }
    if ( !GetContext().PreprocessHugeFile ) {
        if ( m_NumGenes == 0 && m_NumGeneXrefs > 0 ) {
        PostErr(eDiag_Warning, eErr_SEQ_FEAT_OnlyGeneXrefs,
            "There are " + NStr::SizetToString(m_NumGeneXrefs) +
            " gene xrefs and no gene features in this record.", *m_TSE);
        }
    }
    ValidateCitations (seh);


    if ( m_NumMisplacedGraphs > 0 ) {
        string num = NStr::SizetToString(m_NumMisplacedGraphs);
        PostErr(eDiag_Critical, eErr_SEQ_PKG_GraphPackagingProblem,
            string("There ") + ((m_NumMisplacedGraphs > 1) ? "are " : "is ") + num +
            " mispackaged graph" + ((m_NumMisplacedGraphs > 1) ? "s" : "") + " in this record.",
            *m_TSE);
    }

    if ( IsRefSeq() && ! IsWP() ) {
        ValidateMultipleTaxIds(seh);
    }


    FindEmbeddedScript(*(seh.GetCompleteSeq_entry()));
    FindNonAsciiText(*(seh.GetCompleteSeq_entry()));
    if (!GetContext().PreprocessHugeFile) {
        FindCollidingSerialNumbers(*(seh.GetCompleteSeq_entry()));
    }

    if (m_FarFetchFailure) {
        PostErr(eDiag_Warning, eErr_SEQ_INST_FarFetchFailure,
                "Far fetch failures caused some validator tests to be bypassed",
                *m_TSE);
    }

    if (m_DoTaxLookup) {
        ValidateTaxonomy(*(seh.GetCompleteSeq_entry()));
    }

    // validate cit-sub
    if (cs) {
        auto pEntry = seh.GetCompleteSeq_entry();
        if (IsHugeFileMode()) {
            call_once(SetContext().CitSubOnceFlag,
                      [this, &cs, &pEntry]() { ValidateCitSub(*cs, *pEntry, pEntry); });
        } else {
            ValidateCitSub(*cs, *pEntry, pEntry);
        }
    }

    // optional barcode tests
    if (m_DoBarcodeTests) {
        x_DoBarcodeTests(seh);
    }
    return true;
}


void CValidError_imp::ValidateSubmitBlock(const CSubmit_block& block, const CSeq_submit& ss)
{
    if (block.IsSetHup() && block.GetHup() && block.IsSetReldate() &&
        IsDateInPast(block.GetReldate())) {
        PostErr(eDiag_Warning, eErr_GENERIC_PastReleaseDate,
            "Record release date has already passed", ss);
    }

    if (block.IsSetContact() && block.GetContact().IsSetContact()) {
        const CAuthor& author = block.GetContact().GetContact();
        if (author.IsSetAffil() && author.GetAffil().IsStd()) {
            ValidateAffil(author.GetAffil().GetStd(), ss, nullptr);
        }
        const CPerson_id& pid = author.GetName();
        if (pid.IsName()) {
            const CName_std& nstd = pid.GetName();
            string first = "";
            string last = "";
            if (nstd.IsSetLast()) {
                last = nstd.GetLast();
                if (IsBadSubmissionLastName(last)) {
                    PostErr(eDiag_Error, eErr_GENERIC_BadSubmissionAuthorName,
                            "Bad last name '" + last + "'", ss);
                }
            }
            if (nstd.IsSetFirst()) {
                first = nstd.GetFirst();
                if (IsBadSubmissionFirstName(first)) {
                    PostErr(eDiag_Error, eErr_GENERIC_BadSubmissionAuthorName,
                            "Bad first name '" + first + "'", ss);
                }
            }
            if (first != "" && last != "" && NStr::EqualNocase(last, "last") && NStr::EqualNocase(first, "first")) {
                PostErr(eDiag_Error, eErr_GENERIC_BadSubmissionAuthorName,
                        "Bad first and last name", ss);
            }
        }
    }
    if (block.IsSetCit()) {
        const CCit_sub& sub = block.GetCit();
        if (sub.IsSetAuthors()) {
            const CAuth_list& auth_list = sub.GetAuthors();
            const CAuth_list::TNames& names = auth_list.GetNames();
            if (names.IsStd()) {
                ITERATE ( CAuth_list::C_Names::TStd, name, names.GetStd() ) {
                    if ( (*name)->GetName().IsName() ) {
                        const CName_std& nstd = (*name)->GetName().GetName();
                        string first = "";
                        string last = "";
                        if (nstd.IsSetLast()) {
                            last = nstd.GetLast();
                            if (IsBadSubmissionLastName(last)) {
                                PostErr(eDiag_Error, eErr_GENERIC_BadSubmissionAuthorName,
                                        "Bad last name '" + last + "'", ss);
                            }
                        }
                        if (nstd.IsSetFirst()) {
                            first = nstd.GetFirst();
                            if (IsBadSubmissionFirstName(first)) {
                                PostErr(eDiag_Error, eErr_GENERIC_BadSubmissionAuthorName,
                                        "Bad first name '" + first + "'", ss);
                            }
                        }
                        if (first != "" && last != "" && NStr::EqualNocase(last, "last") && NStr::EqualNocase(first, "first")) {
                            PostErr(eDiag_Error, eErr_GENERIC_BadSubmissionAuthorName,
                                    "Bad first and last name", ss);
                        }
                    }
                }
            }
        }
    }
}


void CValidError_imp::Validate(
    const CSeq_submit& ss, CScope* scope)
{
    // Check that ss is type e_Entrys
    if ( ss.GetData().Which() != CSeq_submit::C_Data::e_Entrys ) {
        return;
    }

    x_SetEntryInfo().SetSeqSubmit();
    if (ss.IsSetSub()) {
        if (IsHugeFileMode()) {
            call_once(SetContext().SubmitBlockOnceFlag,
                [this, &ss](){ ValidateSubmitBlock(ss.GetSub(), ss); });
        }
        else {
            ValidateSubmitBlock(ss.GetSub(), ss);
        }
    }

    // Get CCit_sub pointer
    const CCit_sub* cs = &ss.GetSub().GetCit();

    if (ss.IsSetSub() && ss.GetSub().IsSetTool() && NStr::StartsWith(ss.GetSub().GetTool(), "Geneious")) {
        x_SetEntryInfo().SetGeneious();
    }

    // Just loop thru CSeq_entrys
    FOR_EACH_SEQENTRY_ON_SEQSUBMIT (se_itr, ss) {
        const CSeq_entry& se = **se_itr;
        if(se.IsSet())
        {
            const CBioseq_set &set = se.GetSet();
            if(set.IsSetClass() &&
               set.GetClass() == CBioseq_set::eClass_wgs_set)
            {
                if (IsHugeFileMode() && IsHugeSet(CBioseq_set::eClass_wgs_set)) {
                    CSeq_entry_Handle seh;
                    seh = scope->GetSeq_entryHandle(se);
                    Setup(seh);
                    call_once(SetContext().WgsSetInSeqSubmitOnceFlag,
                            [this, seh]() {
                                PostErr(eDiag_Warning, eErr_SEQ_PKG_SeqSubmitWithWgsSet,
                                "File was created as a wgs-set, but should be a batch submission instead.",
                                seh.GetCompleteSeq_entry()->GetSet());
                            });
                } else {
                    CSeq_entry_Handle seh;
                    seh = scope->GetSeq_entryHandle(se);
                    Setup(seh);
                    PostErr(eDiag_Warning, eErr_SEQ_PKG_SeqSubmitWithWgsSet,
                            "File was created as a wgs-set, but should be a batch submission instead.",
                            seh.GetCompleteSeq_entry()->GetSet());
                }
            }
        }
        Validate (se, cs, scope);
    }
}


void CValidError_imp::Validate(
    const CSeq_annot_Handle& sah)
{
    Setup(sah);

    // Iterate thru components of record and validate each

    CValidError_annot annot_validator(*this);
    annot_validator.ValidateSeqAnnot(sah);

    switch (sah.Which()) {
    case CSeq_annot::TData::e_Ftable:
    {
        CValidError_feat feat_validator(*this);
        for (CFeat_CI fi (sah); fi; ++fi) {
            const CSeq_feat& sf = fi->GetOriginalFeature();
            feat_validator.ValidateSeqFeat(sf);
        }
    }
        break;

    case CSeq_annot::TData::e_Align:
    {
        if (IsValidateAlignments()) {
            CValidError_align align_validator(*this);
            int order = 1;
            for (CAlign_CI ai(sah); ai; ++ai) {
                const CSeq_align& sa = ai.GetOriginalSeq_align();
                align_validator.ValidateSeqAlign(sa, order++);
            }
        }
    }
        break;

    case CSeq_annot::TData::e_Graph:
    {
        CValidError_graph graph_validator(*this);
        // for (CTypeConstIterator <CSeq_graph> gi (sa); gi; ++gi) {
        for (CGraph_CI gi(sah); gi; ++gi) {
            const CSeq_graph& sg = gi->GetOriginalGraph();
            graph_validator.ValidateSeqGraph(sg);
        }
    }
        break;
    default:
        break;
    }
    FindEmbeddedScript(*(sah.GetCompleteSeq_annot()));
    FindNonAsciiText(*(sah.GetCompleteSeq_annot()));
    FindCollidingSerialNumbers(*(sah.GetCompleteSeq_annot()));
}


void CValidError_imp::Validate(const CSeq_feat& feat, CScope* scope)
{
    // automatically restores m_Scope to its old value when we leave
    // the function
    CScopeRestorer scopeRestorer( m_Scope );

    if( scope ) {
        m_Scope.Reset(scope);
    }
    if (!m_Scope) {
        // set up a temporary local scope if there is no scope set already
        m_Scope.Reset(new CScope(*m_ObjMgr));
    }

    CValidError_feat feat_validator(*this);
    feat_validator.SetScope(*m_Scope);
    CSeq_entry_Handle empty;
    feat_validator.SetTSE(empty);
    feat_validator.ValidateSeqFeat(feat);
    if (feat.IsSetData() && feat.GetData().IsBiosrc()) {
        const CBioSource& src = feat.GetData().GetBiosrc();
        if (src.IsSetOrg()) {
            ValidateTaxonomy (src.GetOrg(), src.IsSetGenome() ? src.GetGenome() : CBioSource::eGenome_unknown);
        }
    }
    FindEmbeddedScript(feat);
    FindNonAsciiText(feat);
    FindCollidingSerialNumbers(feat);
}


void CValidError_imp::Validate(const CBioSource& src, CScope* scope)
{
    // automatically restores m_Scope to its old value when we leave
    // the function
    CScopeRestorer scopeRestorer( m_Scope );

    if( scope ) {
        m_Scope.Reset(scope);
    }
    if (!m_Scope) {
        // set up a temporary local scope if there is no scope set already
        m_Scope.Reset(new CScope(*m_ObjMgr));
    }

    ValidateBioSource(src, src);
    if (src.IsSetOrg()) {
        ValidateTaxonomy (src.GetOrg(), src.IsSetGenome() ? src.GetGenome() : CBioSource::eGenome_unknown);
    }
    FindEmbeddedScript(src);
    FindNonAsciiText(src);
    FindCollidingSerialNumbers(src);
}


void CValidError_imp::Validate(const CPubdesc& pubdesc, CScope* scope)
{
    // automatically restores m_Scope to its old value when we leave
    // the function
    CScopeRestorer scopeRestorer( m_Scope );

    if( scope ) {
        m_Scope.Reset(scope);
    }
    if (!m_Scope) {
        // set up a temporary local scope if there is no scope set already
        m_Scope.Reset(new CScope(*m_ObjMgr));
    }

    ValidatePubdesc(pubdesc, pubdesc);
    FindEmbeddedScript(pubdesc);
    FindNonAsciiText(pubdesc);
    FindCollidingSerialNumbers(pubdesc);
}

void CValidError_imp::Validate(const CSeqdesc& desc, const CSeq_entry& ctx)
{
    CValidError_desc seqdesc_validator(*this);
    m_Scope.Reset(new CScope(*m_ObjMgr));
    m_Scope->AddTopLevelSeqEntry(ctx);
    seqdesc_validator.ValidateSeqDesc(desc,ctx);
}


void CValidError_imp::SetProgressCallback
(CValidator::TProgressCallback callback,
 void* user_data)
{
    m_PrgCallback = callback;
    m_PrgInfo.m_UserData = user_data;
}


void CValidError_imp::ValidateDbxref
(const CDbtag& xref,
 const CSerialObject& obj,
 bool biosource,
 const CSeq_entry *ctx)
{
    bool refseq_or_gps = IsRefSeq() || IsGPS();
    CValidator::TDbxrefValidFlags flags = CValidator::IsValidDbxref(xref, biosource,
        refseq_or_gps);

    const string& db = xref.IsSetDb() ? xref.GetDb() : kEmptyStr;

    if (flags & CValidator::eTagHasSgml) {
        PostObjErr(eDiag_Warning, eErr_GENERIC_SgmlPresentInText,
            "dbxref value " + xref.GetTag().GetStr() + " has SGML",
            obj, ctx);
    }
    if (flags & CValidator::eContainsSpace) {
        PostObjErr(eDiag_Warning, eErr_SEQ_FEAT_IllegalDbXref,
                   "dbxref value " + xref.GetTag().GetStr() + " contains space character",
                   obj, ctx);
    }
    if (flags & CValidator::eDbHasSgml) {
        PostObjErr(eDiag_Warning, eErr_GENERIC_SgmlPresentInText,
            "dbxref database " + db + " has SGML",
            obj, ctx);
    }

    bool isStr = false;
    string dbv;
    if (xref.IsSetTag() && xref.GetTag().IsStr()) {
        dbv = xref.GetTag().GetStr();
        isStr = true;
    } else if (xref.IsSetTag() && xref.GetTag().IsId()) {
        dbv = NStr::NumericToString(xref.GetTag().GetId());
    }

    if (flags & CValidator::eUnrecognized) {
        PostObjErr(eDiag_Warning, eErr_SEQ_FEAT_IllegalDbXref,
            "Illegal db_xref type " + db + " (" + dbv + ")", obj, ctx);
    }
    if (flags & CValidator::eBadCapitalization) {
        // capitalization is bad
        bool refseq_db = false, src_db = false;
        string correct_caps;
        xref.GetDBFlags(refseq_db, src_db, correct_caps);
        string message = "Illegal db_xref type " + db + " (" + dbv + "), legal capitalization is " + correct_caps;
        if (flags & CValidator::eNotForSource) {
            message += ", but should not be used on an OrgRef";
        } else if (flags & CValidator::eOnlyForSource) {
            message += ", but should only be used on an OrgRef";
        }

        PostObjErr(eDiag_Warning, eErr_SEQ_FEAT_IllegalDbXref, message, obj, ctx);
    } else {
        if (flags & CValidator::eOnlyForRefSeq) {
            if (flags & CValidator::eNotForSource) {
                PostObjErr(eDiag_Warning, eErr_SEQ_FEAT_IllegalDbXref,
                    "RefSeq-specific db_xref type " + db + " (" + dbv + ") should not be used on a non-RefSeq OrgRef",
                    obj, ctx);
            } else {
                PostObjErr(eDiag_Warning, eErr_SEQ_FEAT_IllegalDbXref,
                    "db_xref type " + db + " (" + dbv + ") is only legal for RefSeq",
                    obj, ctx);
            }
        } else if (flags & CValidator::eNotForSource) {
            if (flags & CValidator::eRefSeqNotForSource) {
                PostObjErr(eDiag_Warning, eErr_SEQ_FEAT_IllegalDbXref,
                    "RefSeq-specific db_xref type " + db + " (" + dbv + ") should not be used on an OrgRef",
                    obj, ctx);
            } else {
                PostObjErr(eDiag_Warning, eErr_SEQ_FEAT_IllegalDbXref,
                    "db_xref type " + db + " (" + dbv + ") should not be used on an OrgRef",
                    obj, ctx);
            }
        } else if (flags & CValidator::eOnlyForSource) {
            PostObjErr(eDiag_Warning, eErr_SEQ_FEAT_IllegalDbXref,
                "db_xref type " + db + " (" + dbv + ") should only be used on an OrgRef",
                obj, ctx);
        }
    }

    if (isStr && db == "GeneID") {
        PostObjErr(eDiag_Error, eErr_SEQ_FEAT_IllegalDbXref,
            "db_xref type " + db + " (" + dbv + ") is required to be an integer",
            obj, ctx);
    }
}


void CValidError_imp::ValidateDbxref
(TDbtags& xref_list,
 const CSerialObject& obj,
 bool biosource,
 const CSeq_entry *ctx)
{
    string last_db;

    ITERATE( TDbtags, xref, xref_list) {
        if (biosource
            && (*xref)->IsSetDb()) {
            if (!NStr::IsBlank(last_db)
                && NStr::EqualNocase((*xref)->GetDb(), last_db)) {
                PostObjErr (eDiag_Warning, eErr_SEQ_DESCR_BioSourceDbTagConflict,
                            "BioSource uses db " + last_db + " multiple times",
                            obj, ctx);
            }
            last_db = (*xref)->GetDb();
        }
        ValidateDbxref(**xref, obj, biosource, ctx);
    }
}


void CValidError_imp::x_CheckPackedInt
(const CPacked_seqint& packed_int,
  SLocCheck& lc,
 const CSerialObject& /*obj*/)
{
    ITERATE(CPacked_seqint::Tdata, it, packed_int.Get()) {
        lc.int_cur = (*it);
        lc.chk &= x_CheckSeqInt(lc.id_cur, lc.int_cur, lc.strand_cur);

        x_CheckForStrandChange(lc);

        lc.id_prv = lc.id_cur;
        lc.strand_prv = lc.strand_cur;
        lc.int_prv = lc.int_cur;
    }
}


bool CValidError_imp::x_CheckSeqInt(
    CConstRef<CSeq_id>& id_cur,
    const CSeq_interval* int_cur,
    ENa_strand& strand_cur)
{
    strand_cur = int_cur->IsSetStrand() ?
        int_cur->GetStrand() : eNa_strand_unknown;
    id_cur = &int_cur->GetId();
    bool chk = IsValid(*int_cur, m_Scope);
    return chk;
}


void CValidError_imp::x_ReportInvalidFuzz(const CPacked_seqint& packed_int, const CSerialObject& obj)
{
    ITERATE(CPacked_seqint::Tdata, it, packed_int.Get()) {
        x_ReportInvalidFuzz(**it, obj);
    }
}


static const char* kSpaceLeftFirst   = "Should not specify 'space to left' at first position of non-circular sequence";
static const char* kSpaceRightLast   = "Should not specify 'space to right' at last position of non-circular sequence";
static const char* kSpaceLeftCircle  = "Should not specify 'circle to left' except at first position of circular sequence";
static const char* kSpaceRightCircle = "Should not specify 'circle to right' except at last position of circular sequence";

void CValidError_imp::x_ReportInvalidFuzz(const CSeq_interval& interval, const CSerialObject& obj)
{
    CInt_fuzz::ELim fuzz_from = CInt_fuzz::eLim_unk;
    CInt_fuzz::ELim fuzz_to = CInt_fuzz::eLim_unk;
    bool has_fuzz_from = false;
    bool has_fuzz_to = false;

    if (interval.IsSetFuzz_from() && interval.GetFuzz_from().IsLim()) {
        fuzz_from = interval.GetFuzz_from().GetLim();
        has_fuzz_from = true;
    }
    if (interval.IsSetFuzz_to() && interval.GetFuzz_to().IsLim()) {
        fuzz_to = interval.GetFuzz_to().GetLim();
        has_fuzz_to = true;
    }
    if (! has_fuzz_from && ! has_fuzz_to) {
        return;
    }

    // check for invalid fuzz on both ends of Interval
    if (has_fuzz_from && has_fuzz_to && fuzz_from == fuzz_to) {
        if (fuzz_from == CInt_fuzz::eLim_tl) {
            PostErr(eDiag_Error,
                eErr_SEQ_FEAT_InvalidFuzz,
                "Should not specify 'space to left' for both ends of interval", obj);
        }
        else if (fuzz_from == CInt_fuzz::eLim_tr) {
            PostErr(eDiag_Error,
                eErr_SEQ_FEAT_InvalidFuzz,
                "Should not specify 'space to right' for both ends of interval", obj);
        }
        else if (fuzz_from == CInt_fuzz::eLim_circle) {
            PostErr(eDiag_Error,
                eErr_SEQ_FEAT_InvalidFuzz,
                "Should not specify 'origin of circle' for both ends of interval", obj);
        }
    }

    CBioseq_Handle bsh = m_Scope->GetBioseqHandle(interval.GetId());
    if (! bsh) {
        return;
    }

    CSeq_inst::ETopology top = CSeq_inst::eTopology_not_set;
    if (bsh.IsSetInst_Topology()) {
        top = bsh.GetInst_Topology();
    }

    if (top != CSeq_inst::eTopology_circular) {

        // VR-15
        // look for space to left at beginning of sequence or space to right at end
        if (fuzz_from == CInt_fuzz::eLim_tl && interval.IsSetFrom() && interval.GetFrom() == 0) {
            PostErr(eDiag_Error, eErr_SEQ_FEAT_InvalidFuzz, kSpaceLeftFirst, obj);
        }
        if (fuzz_to == CInt_fuzz::eLim_tr && interval.IsSetTo() && interval.GetTo() == bsh.GetBioseqLength() - 1) {
            PostErr(eDiag_Error, eErr_SEQ_FEAT_InvalidFuzz, kSpaceRightLast, obj);
        }

    } else if (fuzz_from == CInt_fuzz::eLim_circle || fuzz_to == CInt_fuzz::eLim_circle) {

        if (obj.GetThisTypeInfo() == CSeq_feat::GetTypeInfo()) {
            const CSeq_feat* sfp = dynamic_cast<const CSeq_feat*>(&obj);
            if (sfp && sfp->IsSetExcept() && sfp->CanGetExcept_text() && NStr::FindNoCase(sfp->GetExcept_text(), "ribosomal slippage") != NPOS) {
                return;
            }
        }

        // VR-832
        if (fuzz_from == CInt_fuzz::eLim_circle && interval.IsSetFrom() && interval.GetFrom() != 0) {
            PostErr(eDiag_Error, eErr_SEQ_FEAT_InvalidFuzz, kSpaceLeftCircle, obj);
        }
        if (fuzz_to == CInt_fuzz::eLim_circle && interval.IsSetTo() && interval.GetTo() != bsh.GetBioseqLength() - 1) {
            PostErr(eDiag_Error, eErr_SEQ_FEAT_InvalidFuzz, kSpaceRightCircle, obj);
        }
    }
}


void CValidError_imp::x_ReportInvalidFuzz(const CSeq_point& point, const CSerialObject& obj)
{
    // VR-15
    if (!point.IsSetFuzz() || !point.GetFuzz().IsLim() ||
        (point.GetFuzz().GetLim() != CInt_fuzz::eLim_tl && point.GetFuzz().GetLim() != CInt_fuzz::eLim_tr) ||
        !point.IsSetId() || !point.IsSetPoint()) {
        return;
    }
    CBioseq_Handle bsh = m_Scope->GetBioseqHandle(point.GetId());
    if (!bsh) {
        return;
    }
    if (bsh.IsSetInst_Topology() && bsh.GetInst_Topology() == CSeq_inst::eTopology_circular) {
        return;
    }
    if (point.GetPoint() == 0 && point.GetFuzz().GetLim() == CInt_fuzz::eLim_tl) {
        PostErr(eDiag_Error, eErr_SEQ_FEAT_InvalidFuzz, kSpaceLeftFirst, obj);
    }
    if (point.GetPoint() == bsh.GetBioseqLength() - 1) {
        PostErr(eDiag_Error, eErr_SEQ_FEAT_InvalidFuzz, kSpaceRightLast, obj);
    }
}


void CValidError_imp::x_ReportInvalidFuzz(const CSeq_loc& loc, const CSerialObject& obj)
{
    CTypeConstIterator<CSeq_loc> lit = ConstBegin(loc);
    for (; lit; ++lit) {
        CSeq_loc::E_Choice loc_choice = lit->Which();
        switch (loc_choice) {
        case CSeq_loc::e_Int:
            x_ReportInvalidFuzz(lit->GetInt(), obj);
            break;
        case CSeq_loc::e_Packed_int:
            x_ReportInvalidFuzz(lit->GetPacked_int(), obj);
            break;
        case CSeq_loc::e_Pnt:
            x_ReportInvalidFuzz(lit->GetPnt(), obj);
            break;
        default:
            break;
        }
    }
}


unsigned int s_CountMix(const CSeq_loc& loc)
{
    unsigned int num_mix = 0;
    CTypeConstIterator<CSeq_loc> lit = ConstBegin(loc);
    for (; lit; ++lit) {
        if (lit->IsMix()) {
            num_mix++;
        }
    }
    return num_mix;
}


void CValidError_imp::x_InitLocCheck(SLocCheck& lc, const string& prefix)
{
    lc.chk = true;
    lc.unmarked_strand = false;
    lc.mixed_strand = false;
    lc.has_other = false;
    lc.has_not_other = false;
    lc.id_cur = nullptr;
    lc.id_prv = nullptr;
    lc.int_cur = nullptr;
    lc.int_prv = nullptr;
    lc.strand_cur = eNa_strand_unknown;
    lc.strand_prv = eNa_strand_unknown;
    lc.prefix = prefix;
}

void CValidError_imp::x_CheckForStrandChange(SLocCheck& lc)
{
    if (lc.strand_prv != eNa_strand_other  &&
        lc.strand_cur != eNa_strand_other) {
        if (lc.id_cur  &&  lc.id_prv  &&
            IsSameBioseq(*lc.id_cur, *lc.id_prv, m_Scope)) {
            if (lc.strand_prv != lc.strand_cur) {
                if ((lc.strand_prv == eNa_strand_plus  &&
                    lc.strand_cur == eNa_strand_unknown)  ||
                    (lc.strand_prv == eNa_strand_unknown  &&
                    lc.strand_cur == eNa_strand_plus)) {
                    lc.unmarked_strand = true;
                } else {
                    lc.mixed_strand = true;
                }
            }
        }
    }
    if (lc.strand_cur == eNa_strand_other) {
        lc.has_other = true;
    } else if (lc.strand_cur == eNa_strand_minus || lc.strand_cur == eNa_strand_plus) {
        lc.has_not_other = true;
    }

}

void CValidError_imp::x_CheckLoc(const CSeq_loc& loc, const CSerialObject& obj, SLocCheck& lc, bool lowerSev)
{
    try {
        switch (loc.Which()) {
        case CSeq_loc::e_Int:
            lc.int_cur = &loc.GetInt();
            lc.chk = x_CheckSeqInt(lc.id_cur, lc.int_cur, lc.strand_cur);
            if (lc.strand_cur == eNa_strand_other) {
                lc.has_other = true;
            }
            if ((!lc.chk) && lowerSev) {
                TSeqPos length = GetLength(loc.GetInt().GetId(), m_Scope);
                TSeqPos fr = loc.GetInt().GetFrom();
                TSeqPos to = loc.GetInt().GetTo();
                if (fr < length && to >= length) {
                    // RefSeq variation feature with dbSNP xref and interval flanking the length is ERROR
                } else {
                    // otherwise keep severity at REJECT
                    lowerSev = false;
                }
            }
            break;
        case CSeq_loc::e_Pnt:
            lc.strand_cur = loc.GetPnt().IsSetStrand() ?
                loc.GetPnt().GetStrand() : eNa_strand_unknown;
            if (lc.strand_cur == eNa_strand_other) {
                lc.has_other = true;
            }
            lc.id_cur = &loc.GetPnt().GetId();
            lc.chk = IsValid(loc.GetPnt(), m_Scope);
            lc.int_prv = nullptr;
            break;
        case CSeq_loc::e_Packed_pnt:
            lc.strand_cur = loc.GetPacked_pnt().IsSetStrand() ?
                loc.GetPacked_pnt().GetStrand() : eNa_strand_unknown;
            if (lc.strand_cur == eNa_strand_other) {
                lc.has_other = true;
            }
            lc.id_cur = &loc.GetPacked_pnt().GetId();
            lc.chk = IsValid(loc.GetPacked_pnt(), m_Scope);
            lc.int_prv = nullptr;
            break;
        case CSeq_loc::e_Packed_int:
            x_CheckPackedInt(loc.GetPacked_int(), lc, obj);
            break;
        case CSeq_loc::e_Null:
            break;
        case CSeq_loc::e_Mix:
            for (auto l : loc.GetMix().Get()) {
                x_CheckLoc(*l, obj, lc, lowerSev);
                x_CheckForStrandChange(lc);
            }
            break;
        default:
            lc.strand_cur = eNa_strand_other;
            lc.id_cur = nullptr;
            lc.int_prv = nullptr;
            break;
        }
        if (!lc.chk) {
            string lbl = GetValidatorLocationLabel (loc, *m_Scope);
            EDiagSev sev = eDiag_Critical;
            if (lowerSev) {
                sev = eDiag_Error;
            }
            PostErr(sev, eErr_SEQ_FEAT_Range,
                lc.prefix + ": SeqLoc [" + lbl + "] out of range", obj);
        }

        if (loc.Which() != CSeq_loc::e_Null) {
            x_CheckForStrandChange(lc);

            lc.strand_prv = lc.strand_cur;
            lc.id_prv = lc.id_cur;
        }
    } catch( const exception& e ) {
        string label = GetValidatorLocationLabel(loc, *m_Scope);
        PostErr(eDiag_Error, eErr_INTERNAL_Exception,
            "Exception caught while validating location " +
            label + ". Exception: " + e.what(), obj);

        lc.strand_cur = eNa_strand_other;
        lc.id_cur = nullptr;
        lc.int_prv = nullptr;
    }
}

void CValidError_imp::ValidateSeqLoc
(const CSeq_loc& loc,
 const CBioseq_Handle&  seq,
 bool  report_abutting,
 const string&   prefix,
 const CSerialObject& obj,
 bool lowerSev)
{
    SLocCheck lc;

    x_InitLocCheck(lc, prefix);

    x_CheckLoc(loc, obj, lc, lowerSev);

    if (lc.has_other && lc.has_not_other) {
        string label = GetValidatorLocationLabel(loc, *m_Scope);
        PostErr(IsSmallGenomeSet() ? eDiag_Warning : eDiag_Error, eErr_SEQ_FEAT_MixedStrand,
            prefix + ": Inconsistent use of other strand SeqLoc [" + label + "]", obj);
    } else if (lc.has_other && NStr::Equal(prefix, "Location")) {
         PostErr(eDiag_Warning,
            eErr_SEQ_FEAT_StrandOther,
            "Strand 'other' in location", obj);
   }

    x_ReportInvalidFuzz(loc, obj);

    if (m_Scope && CValidator::DoesSeqLocContainDuplicateIntervals(loc, *m_Scope)) {
        PostErr(eDiag_Error,
            eErr_SEQ_FEAT_DuplicateExonInterval,
            "Duplicate exons in location", obj);
    }

    if (s_CountMix(loc) > 1) {
        string label;
        loc.GetLabel(&label);
        PostErr (eDiag_Error, eErr_SEQ_FEAT_NestedSeqLocMix,
            prefix + ": SeqLoc [" + label + "] has nested SEQLOC_MIX elements",
                 obj);
    }

    // Warn if different parts of a seq-loc refer to the same bioseq using
    // differnt id types (i.e. gi and accession)
    ValidateSeqLocIds(loc, obj);

    bool trans_splice = false;
    bool circular_rna = false;
    bool exception = false;
    const CSeq_feat* sfp = nullptr;
    if (obj.GetThisTypeInfo() == CSeq_feat::GetTypeInfo()) {
        sfp = dynamic_cast<const CSeq_feat*>(&obj);
    }
    if (sfp) {
        // primer_bind intervals MAY be in on opposite strands
        if ( sfp->GetData().GetSubtype() == CSeqFeatData::eSubtype_primer_bind ) {
            lc.mixed_strand = false;
            lc.unmarked_strand = false;
        }

        exception = sfp->IsSetExcept() ?  sfp->GetExcept() : false;
        if (exception  &&  sfp->CanGetExcept_text()) {
            if (NStr::FindNoCase(sfp->GetExcept_text(), "trans-splicing") != NPOS) {
                // trans splicing exception turns off both mixed_strand and
                // out_of_order messages
                trans_splice = true;
            } else if (NStr::FindNoCase(sfp->GetExcept_text(), "circular RNA") != NPOS) {
                // circular RNA exception turns off out_of_order message
                circular_rna = true;
            }
        }
    }

    string loc_lbl;
    if (report_abutting && (!sfp || !CSeqFeatData::AllowAdjacentIntervals(sfp->GetData().GetSubtype())) &&
         (m_Scope && CValidator::DoesSeqLocContainAdjacentIntervals(loc, *m_Scope))) {
        loc_lbl = GetValidatorLocationLabel(loc, *m_Scope);

        EDiagSev sev = exception ? eDiag_Warning : eDiag_Error;
        PostErr(sev, eErr_SEQ_FEAT_AbuttingIntervals,
            prefix + ": Adjacent intervals in SeqLoc [" +
            loc_lbl + "]", obj);
    }

    if (trans_splice && !NStr::Equal(prefix, "Product")) {
        CSeq_loc_CI li(loc);
        ++li;
        if (!li) {
            PostErr(eDiag_Warning, eErr_SEQ_FEAT_BadTranssplicedInterval, "Trans-spliced feature should have multiple intervals", obj);
        }
        return;
    }

    bool ordered = true;
    bool circular = false;
    if ( seq  &&
         seq.IsSetInst() && seq.GetInst().IsSetTopology() &&
         seq.GetInst().GetTopology() == CSeq_inst::eTopology_circular ) {
        circular = true;
    }
    try {
        if (m_Scope && (!sfp || CSeqFeatData::RequireLocationIntervalsInBiologicalOrder(sfp->GetData().GetSubtype())) && !circular) {
            ordered = CValidator::IsSeqLocCorrectlyOrdered(loc, *m_Scope);
        }
    } catch ( const CException& ex) {
        string label;
        loc.GetLabel(&label);
        PostErr(eDiag_Error, eErr_INTERNAL_Exception,
            "Exception caught while validating location " +
            label + ". Exception: " + ex.what(), obj);
    }

    if (lc.mixed_strand || lc.unmarked_strand || !ordered) {
        if (loc_lbl.empty()) {
            loc_lbl = GetValidatorLocationLabel(loc, *m_Scope);
        }
        if (lc.mixed_strand) {
            if (IsSmallGenomeSet()) {
                PostErr(eDiag_Warning, eErr_SEQ_FEAT_GenomeSetMixedStrand,
                    prefix + ": Mixed strands in SeqLoc ["
                    + loc_lbl + "] in small genome set - set trans-splicing exception if appropriate", obj);
            } else {
                EDiagSev sev = eDiag_Error;
                if (IsGeneious() || (sfp && sequence::IsPseudo(*sfp, *m_Scope))) {
                    sev = eDiag_Warning;
                }
                PostErr(sev, eErr_SEQ_FEAT_MixedStrand,
                    prefix + ": Mixed strands in SeqLoc ["
                    + loc_lbl + "]", obj);
            }
        } else if (lc.unmarked_strand) {
            PostErr(eDiag_Warning, eErr_SEQ_FEAT_MixedStrand,
                prefix + ": Mixed plus and unknown strands in SeqLoc ["
                + loc_lbl + "]", obj);
        }
        if (!ordered && !circular_rna) {
            if (IsSmallGenomeSet()) {
                PostErr(eDiag_Warning, eErr_SEQ_FEAT_SeqLocOrder,
                    prefix + ": Intervals out of order in SeqLoc [" +
                    loc_lbl + "]", obj);
            } else {
                PostErr(eDiag_Error, eErr_SEQ_FEAT_SeqLocOrder,
                    prefix + ": Intervals out of order in SeqLoc [" +
                    loc_lbl + "]", obj);
            }
        }
        return;
    }

    if ( seq  &&
         seq.IsSetInst_Repr()  &&
         seq.GetInst_Repr() != CSeq_inst::eRepr_seg ) {
        return;
    }

    // Check for intervals out of order on segmented Bioseq
    if ( seq  &&  BadSeqLocSortOrder(seq, loc) && !circular_rna ) {
        if (loc_lbl.empty()) {
            loc.GetLabel(&loc_lbl);
        }
        PostErr(eDiag_Error, eErr_SEQ_FEAT_SeqLocOrder,
            prefix + "Intervals out of order in SeqLoc [" +
            loc_lbl + "]", obj);
    }

    // Check for mixed strand on segmented Bioseq
    if ( IsMixedStrands(loc) ) {
        if (loc_lbl.empty()) {
            loc.GetLabel(&loc_lbl);
        }
        PostErr(eDiag_Error, eErr_SEQ_FEAT_MixedStrand,
            prefix + ": Mixed strands in SeqLoc [" +
            loc_lbl + "]", obj);
    }
}


void CValidError_imp::AddBioseqWithNoBiosource(const CBioseq& seq)
{
    if (!SeqIsPatent(seq)) {
        m_BioseqWithNoSource.push_back(CConstRef<CBioseq>(&seq));
    }
}


void CValidError_imp::AddProtWithoutFullRef(const CBioseq_Handle& seq)
{
    if (!SeqIsPatent (seq)) {
        PostErr (eDiag_Error, eErr_SEQ_FEAT_MissingProteinName,
                 "The product name is missing from this protein.", *(seq.GetCompleteBioseq()));
    }
}


bool CValidError_imp::IsWGSIntermediate(const CBioseq& seq)
{
    bool wgs = false;

    FOR_EACH_DESCRIPTOR_ON_BIOSEQ (it, seq) {
        if ((*it)->IsMolinfo() && (*it)->GetMolinfo().IsSetTech()
            && (*it)->GetMolinfo().GetTech() == CMolInfo::eTech_wgs) {
            wgs = true;
            break;
        }
    }
    if (!wgs) {
        return false;
    }

    bool is_other = false;
    bool has_gi = false;

    FOR_EACH_SEQID_ON_BIOSEQ (it, seq) {
        if ((*it)->IsOther()) {
            is_other = true;
            break;
        } else if ((*it)->IsGi()) {
            has_gi = true;
            break;
        }
    }
    if (!is_other || has_gi) {
        return false;
    }

    return true;
}


bool CValidError_imp::IsTSAIntermediate(const CBioseq& seq)
{
    bool tsa = false;

    FOR_EACH_DESCRIPTOR_ON_BIOSEQ (it, seq) {
        if ((*it)->IsMolinfo() && (*it)->GetMolinfo().IsSetTech()
            && (*it)->GetMolinfo().GetTech() == CMolInfo::eTech_tsa) {
            tsa = true;
            break;
        }
    }
    if (!tsa) {
        return false;
    }

    bool is_other = false;
    bool has_gi = false;

    FOR_EACH_SEQID_ON_BIOSEQ (it, seq) {
        if ((*it)->IsOther()) {
            is_other = true;
            break;
        } else if ((*it)->IsGi()) {
            has_gi = true;
            break;
        }
    }
    if (!is_other || has_gi) {
        return false;
    }

    return true;
}


void CValidError_imp::ReportMissingBiosource(const CSeq_entry& se)
{
   if (GetContext().PreprocessHugeFile) {
        if (m_pEntryInfo->IsNoBioSource() && !GetContext().IsPatent && !GetContext().IsPDB) {
            return;
        }
   }
   else if (m_pEntryInfo->IsNoBioSource()  &&  !m_pEntryInfo->IsPatent()  &&  !m_pEntryInfo->IsPDB()) {
       PostErr(eDiag_Error, eErr_SEQ_DESCR_NoSourceDescriptor,
               "No source information included on this record.", se);

       if (!GetContext().PostprocessHugeFile) {
           return;
       }
   }

    size_t num_no_source = m_BioseqWithNoSource.size();

    for ( size_t i = 0; i < num_no_source; ++i ) {
        PostErr(eDiag_Fatal, eErr_SEQ_DESCR_NoOrgFound,
                "No organism name included in the source. Other qualifiers may exist.",
                *(m_BioseqWithNoSource[i]));
    }
}


CConstRef<CSeq_feat> CValidError_imp::GetCDSGivenProduct(const CBioseq& seq)
{
    CConstRef<CSeq_feat> feat;

    CBioseq_Handle bsh = m_Scope->GetBioseqHandle(seq);

    if ( bsh ) {
        if ( IsNT()  &&  m_TSE ) {
            // In case of a NT bioseq limit the search to features packaged on the
            // NT (we assume features have been pulled from the segments to the NT).
            SAnnotSelector sel(CSeqFeatData::e_Cdregion);
            sel.SetByProduct()
                .SetLimitTSE(m_Scope->GetSeq_entryHandle(*m_TSE));
            CFeat_CI fi(bsh, sel);
            if ( fi ) {
                // return the first one (should be the one packaged on the
                // nuc-prot set).
                feat.Reset(&(fi->GetOriginalFeature()));
            }
        } else {
            SAnnotSelector sel(CSeqFeatData::e_Cdregion);
            sel.SetByProduct();
            CFeat_CI fi(bsh, sel);
            if ( fi ) {
                // return the first one (should be the one packaged on the
                // nuc-prot set).
                feat.Reset(&(fi->GetOriginalFeature()));
            }
        }
    }

    return feat;
}


CConstRef<CSeq_feat> CValidError_imp::GetmRNAGivenProduct(const CBioseq& seq)
{
    CBioseq_Handle bsh = m_Scope->GetBioseqHandle(seq);
    return GetmRNAGivenProduct(bsh);
}


CConstRef<CSeq_feat> CValidError_imp::GetmRNAGivenProduct(const CBioseq_Handle& bsh)
{
    CConstRef<CSeq_feat> feat;
    if ( bsh ) {
        // In case of a NT bioseq limit the search to features packaged on the
        // NT (we assume features have been pulled from the segments to the NT).
        CSeq_entry_Handle limit;
        if ( IsNT()  &&  m_TSE ) {
            limit = m_Scope->GetSeq_entryHandle(*m_TSE);
        }

        if (limit) {
            SAnnotSelector sel(CSeqFeatData::eSubtype_mRNA);
            sel.SetByProduct() .SetLimitTSE(limit);
            CFeat_CI fi(bsh, sel);
            if ( fi ) {
                // return the first one (should be the one packaged on the
                // nuc-prot set).
                feat.Reset(&(fi->GetOriginalFeature()));
            }
        } else {
            SAnnotSelector sel(CSeqFeatData::eSubtype_mRNA);
            sel.SetByProduct();
            CFeat_CI fi(bsh, sel);
            if ( fi ) {
                // return the first one (should be the one packaged on the
                // nuc-prot set).
                feat.Reset(&(fi->GetOriginalFeature()));
            }
        }
    }

    return feat;
}


const CSeq_entry* CValidError_imp::GetAncestor
(const CBioseq& seq,
 CBioseq_set::EClass clss)
{
    const CSeq_entry* parent = nullptr;
    for ( parent = seq.GetParentEntry();
          parent;
          parent = parent->GetParentEntry() ) {
        if ( parent->IsSet() ) {
            const CBioseq_set& set = parent->GetSet();
            if ( set.IsSetClass()  &&  set.GetClass() == clss ) {
                break;
            }
        }
    }
    return parent;
}


bool CValidError_imp::IsSerialNumberInComment(const string& comment)
{
    size_t pos = comment.find('[', 0);
    while ( pos != string::npos ) {
        ++pos;
        bool okay = true;
        if ( isdigit((unsigned char) comment[pos]) ) {
            // skip if first character after bracket is 0
            if (comment[pos] == '0') {
                okay = false;
            }
            while ( isdigit((unsigned char) comment[pos]) ) {
                ++pos;
            }
            if ( comment[pos] == ']' && okay ) {
                return true;
            }
        }

        pos = comment.find('[', pos);
    }
    return false;
}


bool CValidError_imp::RequireLocalProduct(const CSeq_id* sid) const
{
        // okay to have far RefSeq product, but only if genomic product set
    if ( sid && sid->IsOther() ) {
        if ( IsGPS() ) {
            return false;
        }
    }
    // or just a bioseq
    if ( GetTSE().IsSeq() ) {
        return false;
    }

    // or in a standalone Seq-annot
    if (IsStandaloneAnnot() ) {
        return false;
    }
    return true;
}


static void s_CollectPubDescriptorLabels (const CSeq_entry& se,
                                          vector<TEntrezId>& pmids, vector<TEntrezId>& muids, vector<int>& serials,
                                          vector<string>& published_labels, vector<string>& unpublished_labels)
{
    FOR_EACH_SEQDESC_ON_SEQENTRY (it, se) {
        if ((*it)->IsPub()) {
            CCleanup::GetPubdescLabels ((*it)->GetPub(), pmids, muids, serials, published_labels, unpublished_labels);
        }
    }

    if (se.IsSet()) {
        FOR_EACH_SEQENTRY_ON_SEQSET (it, se.GetSet()) {
            s_CollectPubDescriptorLabels (**it, pmids, muids, serials, published_labels, unpublished_labels);
        }
    }
}


void CValidError_imp::ValidateCitations (const CSeq_entry_Handle& seh)
{
    vector<TEntrezId> pmids;
    vector<TEntrezId> muids;
    vector<int> serials;
    vector<string> published_labels;
    vector<string> unpublished_labels;

    // collect labels for pubs on record
    s_CollectPubDescriptorLabels (*(seh.GetCompleteSeq_entry()), pmids, muids, serials, published_labels, unpublished_labels);

    CFeat_CI feat (seh, SAnnotSelector(CSeqFeatData::e_Pub));
    while (feat) {
        CCleanup::GetPubdescLabels (feat->GetData().GetPub(), pmids, muids, serials, published_labels, unpublished_labels);
        ++feat;
    }

    // now examine citations to determine whether they match a pub on the record
    CFeat_CI f (seh);
    while (f) {
        if (f->IsSetCit() && f->GetCit().IsPub()) {
            ITERATE (CPub_set::TPub, cit_it, f->GetCit().GetPub()) {
                bool found = false;

                if ((*cit_it)->IsPmid()) {
                    vector<TEntrezId>::iterator it = pmids.begin();
                    while (it != pmids.end() && !found) {
                        if (*it == (*cit_it)->GetPmid()) {
                            found = true;
                        }
                        ++it;
                    }
                    if (!found) {
                        PostErr (eDiag_Warning, eErr_SEQ_FEAT_FeatureCitationProblem,
                                 "Citation on feature refers to uid ["
                                 + NStr::NumericToString((*cit_it)->GetPmid().Get())
                                 + "] not on a publication in the record",
                                 f->GetOriginalFeature());
                    }
                } else if ((*cit_it)->IsMuid()) {
                    vector<TEntrezId>::iterator it = muids.begin();
                    while (it != muids.end() && !found) {
                        if (*it == (*cit_it)->GetMuid()) {
                            found = true;
                        }
                        ++it;
                    }
                    if (!found) {
                        PostErr (eDiag_Warning, eErr_SEQ_FEAT_FeatureCitationProblem,
                                 "Citation on feature refers to uid ["
                                 + NStr::NumericToString((*cit_it)->GetMuid())
                                 + "] not on a publication in the record",
                                 f->GetOriginalFeature());
                    }
                } else if ((*cit_it)->IsEquiv()) {
                    continue;
                } else {
                    string label;
                    (*cit_it)->GetLabel(&label, CPub::eContent, CPub::fLabel_Unique);

                    if (NStr::EndsWith (label, ">")) {
                        label = label.substr(0, label.length() - 2);
                    }
                    if(NStr::EndsWith (label, "|")) {
                        label = label.substr(0, label.length() - 1);
                    }
                    if (NStr::EndsWith (label, "  ")) {
                        label = label.substr(0, label.length() - 1);
                    }
                    size_t len = label.length();
                    vector<string>::iterator unpub_it = unpublished_labels.begin();
                    while (unpub_it != unpublished_labels.end() && !found) {
                        size_t it_len =(*unpub_it).length();
                        if (NStr::EqualNocase (*unpub_it, 0, it_len > len ? len : it_len, label)) {
                            found = true;
                        }
                        ++unpub_it;
                    }
                    vector<string>::iterator pub_it = published_labels.begin();

                    while (pub_it != published_labels.end() && !found) {
                        size_t it_len =(*pub_it).length();
                        if (NStr::EqualNocase (*pub_it, 0, it_len > len ? len : it_len, label)) {
                            PostErr (eDiag_Warning, eErr_SEQ_FEAT_FeatureCitationProblem,
                                     "Citation on feature needs to be updated to published uid",
                                     f->GetOriginalFeature());
                            found = true;
                        }
                        ++pub_it;
                    }
                    if (!found) {
                        PostErr (eDiag_Warning, eErr_SEQ_FEAT_FeatureCitationProblem,
                                 "Citation on feature refers to a publication not in the record",
                                 f->GetOriginalFeature());
                    }
                }
            }
        }
        ++f;
    }
}


// =============================================================================
//                                  Private
// =============================================================================



void CValidError_imp::FindNonAsciiText (const CSerialObject& obj)
{
    CStdTypeConstIterator<string> it(obj);
    for( ; it; ++it) {
        const string& str = *it;
        FOR_EACH_CHAR_IN_STRING(c_it, str) {
            const char& ch = *c_it;
            unsigned char chu = ch;
            if (ch > 127 || (ch < 32 && ch != '\t' && ch != '\r' && ch != '\n')) {
                PostErr (eDiag_Fatal, eErr_GENERIC_NonAsciiAsn,
                         "Non-ASCII character '" + NStr::NumericToString(chu) + "' found in item", obj);
                break;
            }
        }
    }
}


void CValidError_imp::FindEmbeddedScript (const CSerialObject& obj)
{
    class CScriptTagTextFsm : public CTextFsm<int>
    {
    public:
        CScriptTagTextFsm() {
            const char * script_tags[] = {
                "<script", "<object", "<applet", "<embed", "<form",
                "javascript:", "vbscript:"};
            ITERATE_0_IDX(idx, ArraySize(script_tags)) {
                AddWord(script_tags[idx], true);
            }
            Prime();
        }

        // Returns true if the given string matches any of the strings
        // in the fsm anywhere.
        bool DoesStrHaveFsmHits(const string &str) {
            int state = GetInitialState();
            ITERATE(string, str_it, str) {
                state = GetNextState(state, *str_it);
                if( IsMatchFound(state) ) {
                    return true;
                }
            }
            return false;
        }
    };
    static CSafeStatic<CScriptTagTextFsm> s_ScriptTagFsm;

    CStdTypeConstIterator<string> it(obj);
    for( ; it; ++it) {
        if (s_ScriptTagFsm->DoesStrHaveFsmHits(*it)) {
            PostErr (eDiag_Error, eErr_GENERIC_EmbeddedScript,
                     "Script tag found in item", obj);
            return;
        }
    }
}


bool CValidError_imp::IsMixedStrands(const CSeq_loc& loc)
{
    if ( SeqLocCheck(loc, m_Scope) == eSeqLocCheck_warning ) {
        return false;
    }

    CSeq_loc_CI curr(loc);
    if ( !curr ) {
        return false;
    }
    CSeq_loc_CI prev = curr;
    ++curr;

    while ( curr ) {
        ENa_strand curr_strand = curr.GetStrand();
        ENa_strand prev_strand = prev.GetStrand();

        if ( (prev_strand == eNa_strand_minus  &&
              curr_strand != eNa_strand_minus)   ||
             (prev_strand != eNa_strand_minus  &&
              curr_strand == eNa_strand_minus) ) {
            return true;
        }

        prev = curr;
        ++curr;
    }

    return false;
}


static bool s_SeqLocHasGI (const CSeq_loc& loc)
{
    bool rval = false;

    for ( CSeq_loc_CI it(loc); it && !rval; ++it ) {
        if (it.GetSeq_id().IsGi()) {
            rval = true;
        }
    }
    return rval;
}


void CValidError_imp::SetTSE(const CSeq_entry_Handle& seh)
{
    m_TSEH = seh;
    m_TSE = m_TSEH.GetCompleteSeq_entry();
    m_GeneCache.Clear();
}


bool s_IsGoodTopSetClass(CBioseq_set::EClass set_class)
{
    if (set_class == CBioseq_set::eClass_gen_prod_set || set_class == CBioseq_set::eClass_small_genome_set) {
        return true;
    } else {
        return false;
    }
}


size_t s_CountTopSetSiblings(const CSeq_entry& se)
{
    if (se.IsSeq()) {
        return 1;
    } else if (!se.IsSet()) {
        return 0;
    }
    if (se.GetSet().IsSetClass()) {
        if (se.GetSet().GetClass() == CBioseq_set::eClass_nuc_prot ||
            s_IsGoodTopSetClass(se.GetSet().GetClass())) {
            return 1;
        }
    }
    size_t count = 0;
    if (se.GetSet().IsSetSeq_set()) {
        for (auto it = se.GetSet().GetSeq_set().begin(); it != se.GetSet().GetSeq_set().end(); it++) {
            count += s_CountTopSetSiblings(**it);
        }
    }
    return count;
}


void CValidError_imp::Setup(const CSeq_entry_Handle& seh)
{
    // "Save" the Seq-entry
    SetTSE(seh);

    m_NumTopSetSiblings = s_CountTopSetSiblings(*(seh.GetCompleteSeq_entry()));
    m_Scope.Reset(&m_TSEH.GetScope());

    // If no Pubs/BioSource in CSeq_entry, post only one error
    if (GetContext().PreprocessHugeFile) {
        x_SetEntryInfo().SetNoPubs(GetContext().NoPubsFound);
        x_SetEntryInfo().SetNoCitSubPubs(GetContext().NoCitSubsFound);
        x_SetEntryInfo().SetNoBioSource(GetContext().NoBioSource);
    } else {
        CTypeConstIterator<CPub> pub(ConstBegin(*m_TSE));
        x_SetEntryInfo().SetNoPubs(!pub);
        while (pub && !pub->IsSub()) {
            ++pub;
        }
        x_SetEntryInfo().SetNoCitSubPubs(!pub);
        CTypeConstIterator<CBioSource> src(ConstBegin(*m_TSE));
        x_SetEntryInfo().SetNoBioSource(!src);
    }


    // Look for genomic product set
    for (CTypeConstIterator <CBioseq_set> si (*m_TSE); si; ++si) {
        if (si->IsSetClass ()) {
            if (si->GetClass () == CBioseq_set::eClass_gen_prod_set) {
                x_SetEntryInfo().SetGPS();
            }
            if (si->GetClass () == CBioseq_set::eClass_small_genome_set) {
                x_SetEntryInfo().SetSmallGenomeSet();
            }
        }
    }

    // Examine all Seq-ids on Bioseqs
    for (CTypeConstIterator <CBioseq> bi (*m_TSE); bi; ++bi) {
        FOR_EACH_SEQID_ON_BIOSEQ (sid_itr, *bi) {
            const CSeq_id& sid = **sid_itr;
            const CTextseq_id* tsid = sid.GetTextseq_Id();
            CSeq_id::E_Choice typ = sid.Which();
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
                x_SetEntryInfo().SetINSDInSep();
                x_SetEntryInfo().SetGenbank();
                x_SetEntryInfo().SetGED();
                break;
            case CSeq_id::e_Embl:
                x_SetEntryInfo().SetINSDInSep();
                x_SetEntryInfo().SetGED();
                x_SetEntryInfo().SetEmbl();
                break;
            case CSeq_id::e_Pir:
                break;
            case CSeq_id::e_Swissprot:
                break;
            case CSeq_id::e_Patent:
                x_SetEntryInfo().SetPatent();
                break;
            case CSeq_id::e_Other:
                x_SetEntryInfo().SetRefSeq();
                // and do RefSeq subclasses up front as well
                if (sid.GetOther().IsSetAccession()) {
                    string acc = sid.GetOther().GetAccession().substr(0, 3);
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
                        } else if (acc == "NZ_") {
                        m_IsNZ = true;
                    } else if (acc == "NS_") {
                        m_IsNS = true;
                    } else if (acc == "NT_") {
                        m_IsNT = true;
                    } else if (acc == "NW_") {
                        m_IsNW = true;
                    } else if (acc == "WP_") {
                        m_IsWP = true;
                    } else if (acc == "XR_") {
                        m_IsXR = true;
                    }
                }
                break;
            case CSeq_id::e_General:
                if ((*bi).IsAa() && !sid.GetGeneral().IsSkippable()) {
                    x_SetEntryInfo().SetProteinHasGeneralID();
                }
                break;
            case CSeq_id::e_Gi:
                x_SetEntryInfo().SetGI();
                x_SetEntryInfo().SetGiOrAccnVer();
                break;
            case CSeq_id::e_Ddbj:
                x_SetEntryInfo().SetINSDInSep();
                x_SetEntryInfo().SetGED();
                x_SetEntryInfo().SetDdbj();
                break;
            case CSeq_id::e_Prf:
                break;
            case CSeq_id::e_Pdb:
                x_SetEntryInfo().SetPDB();
                break;
            case CSeq_id::e_Tpg:
                x_SetEntryInfo().SetINSDInSep();
                break;
            case CSeq_id::e_Tpe:
                x_SetEntryInfo().SetTPE();
                x_SetEntryInfo().SetINSDInSep();
                break;
            case CSeq_id::e_Tpd:
                x_SetEntryInfo().SetINSDInSep();
                break;
            case CSeq_id::e_Gpipe:
                x_SetEntryInfo().SetGpipe();
                break;
            default:
                break;
            }
            if ( tsid && tsid->IsSetAccession() && tsid->IsSetVersion() && tsid->GetVersion() >= 1 ) {
                x_SetEntryInfo().SetGiOrAccnVer();
            }
            if (typ != CSeq_id::e_Local && typ != CSeq_id::e_General) {
                x_SetEntryInfo().SetLocalGeneralOnly(false);
            }
        }
    }

    // search all source descriptors for genomic source
    for (CSeqdesc_CI desc_ci (seh, CSeqdesc::e_Source);
         desc_ci && !m_pEntryInfo->IsGenomic();
         ++desc_ci) {
         if (desc_ci->GetSource().IsSetGenome()
             && desc_ci->GetSource().GetGenome() == CBioSource::eGenome_genomic) {
             x_SetEntryInfo().SetGenomic();
         }
    }

    // search genome build and annotation pipeline user object descriptors
    for (CSeqdesc_CI desc_ci (seh, CSeqdesc::e_User);
         desc_ci && !m_pEntryInfo->IsGpipe();
         ++desc_ci) {
         if ( desc_ci->GetUser().IsSetType() ) {
             const CUser_object& obj = desc_ci->GetUser();
             const CObject_id& oi = obj.GetType();
             if ( ! oi.IsStr() ) continue;
             if ( NStr::CompareNocase(oi.GetStr(), "GenomeBuild") == 0 ) {
                 x_SetEntryInfo().SetGpipe();
             } else if ( NStr::CompareNocase(oi.GetStr(), "StructuredComment") == 0 ) {
                 ITERATE (CUser_object::TData, field, obj.GetData()) {
                     if ((*field)->IsSetLabel() && (*field)->GetLabel().IsStr()) {
                         if (NStr::EqualNocase((*field)->GetLabel().GetStr(), "Annotation Pipeline")) {
                             if (NStr::EqualNocase((*field)->GetData().GetStr(), "NCBI eukaryotic genome annotation pipeline")) {
                                 x_SetEntryInfo().SetGpipe();
                             }
                         }
                     }
                 }
             }
         }
    }

    // examine features for location gi, product gi, and locus tag
    for (CFeat_CI feat_ci (seh);
         feat_ci && (!DoesAnyFeatLocHaveGI() || !DoesAnyProductLocHaveGI() || !DoesAnyGeneHaveLocusTag());
         ++feat_ci) {
        if (s_SeqLocHasGI(feat_ci->GetLocation())) {
            x_SetEntryInfo().SetFeatLocHasGI();
        }
        if (feat_ci->IsSetProduct() && s_SeqLocHasGI(feat_ci->GetProduct())) {
            x_SetEntryInfo().SetProductLocHasGI();
        }
        if (feat_ci->IsSetData() && feat_ci->GetData().IsGene()
            && feat_ci->GetData().GetGene().IsSetLocus_tag()
            && !NStr::IsBlank (feat_ci->GetData().GetGene().GetLocus_tag())) {
            x_SetEntryInfo().SetGeneHasLocusTag();
        }
    }

    if ( m_PrgCallback ) {
        m_NumAlign = 0;
        for (CTypeConstIterator<CSeq_align> i(*m_TSE); i; ++i) {
            m_NumAlign++;
        }
        m_NumAnnot = 0;
        for (CTypeConstIterator<CSeq_annot> i(*m_TSE); i; ++i) {
            m_NumAnnot++;
        }
        m_NumBioseq = 0;
        for (CTypeConstIterator<CBioseq> i(*m_TSE); i; ++i) {
            m_NumBioseq++;
        }
        m_NumBioseq_set = 0;
        for (CTypeConstIterator<CBioseq_set> i(*m_TSE); i; ++i) {
            m_NumBioseq_set++;
        }
        m_NumDesc = 0;
        for (CTypeConstIterator<CSeqdesc> i(*m_TSE); i; ++i) {
            m_NumDesc++;
        }
        m_NumDescr = 0;
        for (CTypeConstIterator<CSeq_descr> i(*m_TSE); i; ++i) {
            m_NumDescr++;
        }
        m_NumFeat = 0;
        for (CTypeConstIterator<CSeq_feat> i(*m_TSE); i; ++i) {
            m_NumFeat++;
        }
        m_NumGraph = 0;
        for (CTypeConstIterator<CSeq_graph> i(*m_TSE); i; ++i) {
            m_NumGraph++;
        }
        m_PrgInfo.m_Total = m_NumAlign + m_NumAnnot + m_NumBioseq +
            m_NumBioseq_set + m_NumDesc + m_NumDescr + m_NumFeat +
            m_NumGraph;
    }

    if (CNcbiApplication::Instance()->GetProgramDisplayName() == "table2asn") {
        m_IsTbl2Asn = true;
    }
}


void CValidError_imp::SetScope(const CSeq_entry& se)
{
    m_Scope.Reset(new CScope(*m_ObjMgr));
    m_Scope->AddTopLevelSeqEntry(*const_cast<CSeq_entry*>(&se));
    m_Scope->AddDefaults();
}


void CValidError_imp::Setup(const CSeq_annot_Handle& sah)
{
    m_IsStandaloneAnnot = true;
    if (! m_Scope) {
        m_Scope.Reset(& sah.GetScope());
    }
    m_SeqAnnot = sah.GetCompleteSeq_annot();
    m_TSE.Reset(new CSeq_entry); // set a dummy Seq-entry
    m_TSEH = m_Scope->AddTopLevelSeqEntry(*m_TSE);
}


CSeq_entry_Handle CValidError_imp::Setup(const CBioseq& seq)
{
    m_Scope.Reset(new CScope(*m_ObjMgr));
    CRef<CSeq_entry> tmp_entry(new CSeq_entry());
    tmp_entry->SetSeq().Assign(seq);
    m_TSE.Reset(tmp_entry);
    m_TSEH = m_Scope->AddTopLevelSeqEntry(*m_TSE);
    Setup(m_TSEH);
    return m_TSEH;
}


void CValidError_imp::ValidateSeqLocIds
(const CSeq_loc& loc,
 const CSerialObject& obj)
{
    for ( CSeq_loc_CI lit(loc); lit; ++lit ) {
        const CSeq_id& id1 = lit.GetSeq_id();
        CSeq_loc_CI  lit2 = lit;
        for ( ++lit2; lit2; ++lit2 ) {
            const CSeq_id& id2 = lit2.GetSeq_id();
            if ( IsSameBioseq(id1, id2, m_Scope)  &&  !id1.Match(id2) ) {
                PostErr(eDiag_Warning,
                    eErr_SEQ_FEAT_DifferntIdTypesInSeqLoc,
                    "Two ids refer to the same bioseq but are of "
                    "different type", obj);
            }
        }
        if (IsTemporary(id1)) {
            PostErr(eDiag_Critical, eErr_SEQ_INST_BadSeqIdFormat,
                "Feature locations should not use Seq-ids that will be stripped during ID load", obj);
        }
    }
    if (validator::BadMultipleSequenceLocation(loc, *m_Scope)) {
        PostErr(eDiag_Error, eErr_SEQ_FEAT_BadLocation,
            "Feature location intervals should all be on the same sequence", obj);
    }
}


bool CValidError_imp::IsInOrganelleSmallGenomeSet(const CSeq_id& id, CScope& scope)
{
    return validator::IsInOrganelleSmallGenomeSet(id, scope);
}


// all ids in a location should point to the same sequence, unless the sequences are
// in an organelle small genome set
bool CValidError_imp::BadMultipleSequenceLocation(const CSeq_loc& loc, CScope& scope)
{
    return validator::BadMultipleSequenceLocation(loc, scope);
}


bool CValidError_imp::x_IsFarFetchFailure (const CSeq_loc& loc)
{
    if (!IsFarFetchMRNAproducts() && !IsFarFetchCDSproducts()
        && IsFarLocation(loc, GetTSEH())) {
        return true;
    } else {
        return false;
    }
}


//LCOV_EXCL_START
// not used by asnvalidate, used by external programs
bool CValidError_imp::GetTSANStretchErrors(const CSeq_entry_Handle& se)
{
    bool rval = false;
    Setup(se);
    CValidError_bioseq bioseq_validator(*this);
    CBioseq_CI bi(se, CSeq_inst::eMol_na);
    while (bi) {
        rval |= bioseq_validator.GetTSANStretchErrors(*(bi->GetCompleteBioseq()));
        ++bi;
    }
    return rval;
}


bool CValidError_imp::GetTSANStretchErrors(const CBioseq& seq)
{
    CSeq_entry_Handle seh = Setup(seq);
    CValidError_bioseq bioseq_validator(*this);
    return bioseq_validator.GetTSANStretchErrors(*(seh.GetSeq().GetCompleteBioseq()));
}


bool CValidError_imp::GetTSACDSOnMinusStrandErrors (const CSeq_entry_Handle& se)
{
    bool rval = false;
    Setup(se);
    CValidError_feat feat_validator(*this);
    CFeat_CI fi(se);
    while (fi) {
        CBioseq_Handle bsh = se.GetScope().GetBioseqHandle(fi->GetLocation());
        if (bsh) {
            rval |= feat_validator.GetTSACDSOnMinusStrandErrors(*(fi->GetSeq_feat()), *(bsh.GetCompleteBioseq()));
        }
        ++fi;
    }

    return rval;
}


bool CValidError_imp::GetTSACDSOnMinusStrandErrors (const CSeq_feat& f, const CBioseq& seq)
{
    CSeq_entry_Handle seh = Setup(seq);
    CValidError_feat feat_validator(*this);
    return feat_validator.GetTSACDSOnMinusStrandErrors(f, *(seh.GetSeq().GetCompleteBioseq()));
}


bool CValidError_imp::GetTSAConflictingBiomolTechErrors (const CSeq_entry_Handle& se)
{
    bool rval = false;
    Setup(se);
    CValidError_bioseq bioseq_validator(*this);
    CBioseq_CI bi(se, CSeq_inst::eMol_na);
    while (bi) {
        rval |= bioseq_validator.GetTSAConflictingBiomolTechErrors(*(bi->GetCompleteBioseq()));
        ++bi;
    }
    return rval;
}


bool CValidError_imp::GetTSAConflictingBiomolTechErrors (const CBioseq& seq)
{
    CSeq_entry_Handle seh = Setup(seq);
    CValidError_bioseq bioseq_validator(*this);
    return bioseq_validator.GetTSAConflictingBiomolTechErrors(*(seh.GetSeq().GetCompleteBioseq()));
}
//LCOV_EXCL_STOP

const string kTooShort = "Too Short";
const string kMissingPrimers = "Missing Primers";
const string kMissingCountry = "Missing Country";
const string kMissingVoucher = "Missing Voucher";
const string kBadCollectionDate = "Bad Collection Date";
const string kTooManyNs = "Too Many Ns";
const string kMissingOrderAssignment = "Missing Order Assignment";
const string kLowTrace = "Low Trace";
const string kFrameShift = "Frame Shift";
const string kStructuredVoucher = "Structured Voucher";

#define ADD_BARCODE_ERR(TestName) \
    PostErr(eDiag_Warning, eErr_GENERIC_Barcode##TestName, k##TestName, sq); \
    if (!msg.empty()) { \
        msg += ","; \
    } \
    msg += k##TestName;

void CValidError_imp::x_DoBarcodeTests(CSeq_entry_Handle seh)
{
    TBarcodeResults results = GetBarcodeValues(seh);
    for (auto r : results) {
        const CBioseq& sq = *(r.bsh.GetCompleteBioseq());
        if (BarcodeTestFails(r)){
            string msg;
            if (r.length) {
                ADD_BARCODE_ERR(TooShort)
            }
            if (r.primers) {
                ADD_BARCODE_ERR(MissingPrimers)
            }
            if (r.country) {
                ADD_BARCODE_ERR(MissingCountry)
            }
            if (r.voucher) {
                ADD_BARCODE_ERR(MissingVoucher)
            }
            if (!r.percent_n.empty()) {
                PostErr(eDiag_Warning, eErr_GENERIC_BarcodeTooManyNs, kTooManyNs + ":" + r.percent_n, sq);
                if (!msg.empty()) {
                    msg += ",";
                }
                msg += kTooManyNs + ":" + r.percent_n;
            }
            if (r.collection_date) {
                ADD_BARCODE_ERR(BadCollectionDate)
            }
            if (r.order_assignment) {
                ADD_BARCODE_ERR(MissingOrderAssignment)
            }
            if (r.low_trace) {
                ADD_BARCODE_ERR(LowTrace)
            }
            if (r.frame_shift) {
                ADD_BARCODE_ERR(FrameShift)
            }
            if (!r.structured_voucher) {
                ADD_BARCODE_ERR(StructuredVoucher)
            }
            PostErr(eDiag_Info, eErr_GENERIC_BarcodeTestFails, "FAIL (" + msg + ")", sq);
        } else {
            PostErr(eDiag_Info, eErr_GENERIC_BarcodeTestPasses, "PASS", sq);
        }
    }
}


bool CValidError_imp::IsNoPubs() const { return GetEntryInfo().IsNoPubs(); }
bool CValidError_imp::IsNoCitSubPubs() const { return GetEntryInfo().IsNoCitSubPubs(); }
bool CValidError_imp::IsNoBioSource() const { return GetEntryInfo().IsNoBioSource(); }
bool CValidError_imp::IsGPS() const { return GetEntryInfo().IsGPS(); }
bool CValidError_imp::IsGED() const { return GetEntryInfo().IsGED(); }
bool CValidError_imp::IsPDB() const { return GetEntryInfo().IsPDB(); }
bool CValidError_imp::IsPatent() const { return GetEntryInfo().IsPatent(); }
bool CValidError_imp::IsRefSeq() const { return GetEntryInfo().IsRefSeq() || m_RefSeqConventions || GetContext().IsRefSeq; }
bool CValidError_imp::IsEmbl() const { return GetEntryInfo().IsEmbl(); }
bool CValidError_imp::IsDdbj() const { return GetEntryInfo().IsDdbj(); }
bool CValidError_imp::IsTPE() const { return GetEntryInfo().IsTPE(); }
bool CValidError_imp::IsNC() const { return m_IsNC; }
bool CValidError_imp::IsNG() const { return m_IsNG; }
bool CValidError_imp::IsNM() const { return m_IsNM; }
bool CValidError_imp::IsNP() const { return m_IsNP; }
bool CValidError_imp::IsNR() const { return m_IsNR; }
bool CValidError_imp::IsNS() const { return m_IsNS; }
bool CValidError_imp::IsNT() const { return m_IsNT; }
bool CValidError_imp::IsNW() const { return m_IsNW; }
bool CValidError_imp::IsNZ() const { return m_IsNZ; }
bool CValidError_imp::IsWP() const { return m_IsWP; }
bool CValidError_imp::IsXR() const { return m_IsXR; }
bool CValidError_imp::IsGI() const { return GetEntryInfo().IsGI(); }
bool CValidError_imp::IsGenbank() const { return GetEntryInfo().IsGenbank(); }
bool CValidError_imp::IsGpipe() const { return GetEntryInfo().IsGpipe(); }
bool CValidError_imp::IsLocalGeneralOnly() const { return GetEntryInfo().IsLocalGeneralOnly(); }
bool CValidError_imp::HasGiOrAccnVer() const { return GetEntryInfo().HasGiOrAccnVer(); }
bool CValidError_imp::IsGenomic() const { return GetEntryInfo().IsGenomic(); }
bool CValidError_imp::IsSeqSubmit() const { return GetEntryInfo().IsSeqSubmit(); }
bool CValidError_imp::IsSmallGenomeSet() const { return GetEntryInfo().IsSmallGenomeSet(); }
bool CValidError_imp::DoesAnyFeatLocHaveGI() const { return GetEntryInfo().DoesAnyFeatLocHaveGI(); }
bool CValidError_imp::DoesAnyProductLocHaveGI() const { return GetEntryInfo().DoesAnyProductLocHaveGI(); }
bool CValidError_imp::DoesAnyGeneHaveLocusTag() const { return GetEntryInfo().DoesAnyGeneHaveLocusTag(); }
bool CValidError_imp::DoesAnyProteinHaveGeneralID() const { return GetEntryInfo().DoesAnyProteinHaveGeneralID(); }
bool CValidError_imp::IsINSDInSep() const { return GetEntryInfo().IsINSDInSep(); }
bool CValidError_imp::IsGeneious() const { return GetEntryInfo().IsGeneious(); }
const CBioSourceKind& CValidError_imp::BioSourceKind() const { return m_biosource_kind; }



// =============================================================================
//                         CValidError_base Implementation
// =============================================================================


CValidError_base::CValidError_base(CValidError_imp& imp) :
    m_Imp(imp), m_Scope(imp.GetScope())
{
}


CValidError_base::~CValidError_base()
{
}


void CValidError_base::PostErr
(EDiagSev sv,
 EErrType et,
 const string& msg,
 const CSerialObject& obj)
{
    m_Imp.PostErr(sv, et, msg, obj);
}


//void CValidError_base::PostErr
//(EDiagSev sv,
// EErrType et,
// const string& msg,
// TDesc ds)
//{
//    m_Imp.PostErr(sv, et, msg, ds);
//}


void CValidError_base::PostErr
(EDiagSev sv,
 EErrType et,
 const string& msg,
 const CSeq_feat& ft)
{
    m_Imp.PostErr(sv, et, msg, ft);
}


void CValidError_base::PostErr
(EDiagSev sv,
 EErrType et,
 const string& msg,
 const CBioseq& sq)
{
    m_Imp.PostErr(sv, et, msg, sq);
}


void CValidError_base::PostErr
(EDiagSev sv,
 EErrType et,
 const string& msg,
 const CSeq_entry& ctx,
 const CSeqdesc& ds)
{
    m_Imp.PostErr(sv, et, msg, ctx, ds);
}


void CValidError_base::PostErr
(EDiagSev sv,
 EErrType et,
 const string& msg,
 const CBioseq_set& set)
{
    m_Imp.PostErr(sv, et, msg, set);
}


void CValidError_base::PostErr
(EDiagSev sv,
 EErrType et,
 const string& msg,
 const CSeq_annot& annot)
{
    m_Imp.PostErr(sv, et, msg, annot);
}

void CValidError_base::PostErr
(EDiagSev sv,
 EErrType et,
 const string& msg,
 const CSeq_graph& graph)
{
    m_Imp.PostErr(sv, et, msg, graph);
}


void CValidError_base::PostErr
(EDiagSev sv,
 EErrType et,
 const string& msg,
 const CBioseq& sq,
 const CSeq_graph& graph)
{
    m_Imp.PostErr(sv, et, msg, sq, graph);
}


void CValidError_base::PostErr
(EDiagSev sv,
 EErrType et,
 const string& msg,
 const CSeq_align& align)
{
    m_Imp.PostErr(sv, et, msg, align);
}


void CValidError_base::PostErr
(EDiagSev sv,
 EErrType et,
 const string& msg,
 const CSeq_entry& entry)
{
    m_Imp.PostErr(sv, et, msg, entry);
}

CCacheImpl&
CValidError_base::GetCache()
{
    return m_Imp.GetCache();
}


bool s_HasTopSetSiblings(CSeq_entry_Handle seh)
{
    CSeq_entry_Handle parent = seh.GetParentEntry();
    if (!parent || !parent.IsSet()) {
        return false;
    }
    CConstRef<CBioseq_set> pset = parent.GetSet().GetCompleteBioseq_set();
    if (!pset) {
        return false;
    }
    if (pset->IsSetSeq_set() && pset->GetSeq_set().size() > 10) {
        return true;
    } else {
        return s_HasTopSetSiblings(parent);
    }
}


CSeq_entry_Handle CValidError_base::GetAppropriateXrefParent(CSeq_entry_Handle seh)
{
    CSeq_entry_Handle appropriate_parent;

    CSeq_entry_Handle np;
    CSeq_entry_Handle gps;
    if (seh.IsSet() && seh.GetSet().IsSetClass()) {
        if (seh.GetSet().GetClass() == CBioseq_set::eClass_nuc_prot) {
            np = seh;
        } else if (s_IsGoodTopSetClass(seh.GetSet().GetClass())) {
            gps = seh;
        }
    } else if (seh.IsSeq()) {
        CSeq_entry_Handle p = seh.GetParentEntry();
        if (p && p.IsSet() && p.GetSet().IsSetClass()) {
            if (p.GetSet().GetClass() == CBioseq_set::eClass_nuc_prot) {
                np = p;
            } else if (s_IsGoodTopSetClass(p.GetSet().GetClass())) {
                gps = p;
            }
        }
    }
    if (gps) {
        appropriate_parent = gps;
    } else if (np) {
        CSeq_entry_Handle gp = np.GetParentEntry();
        if (gp && gp.IsSet() && gp.GetSet().IsSetClass() &&
            s_IsGoodTopSetClass(gp.GetSet().GetClass())) {
            appropriate_parent = gp;
        } else {
            appropriate_parent = np;
        }
    } else {
        appropriate_parent = seh;
    }
    return appropriate_parent;
}


const CCacheImpl::CPubdescInfo &
CCacheImpl::GetPubdescToInfo(
    CConstRef<CPubdesc> pub)
{
    // first, try to receive from cache
    CCacheImpl::TPubdescCache::const_iterator find_iter =
        m_pubdescCache.find(pub);
    if( find_iter != m_pubdescCache.end() ) {
        return *find_iter->second;
    }

    CRef<CPubdescInfo> pInfo(new CPubdescInfo);
    CCleanup::GetPubdescLabels(
        *pub, pInfo->m_pmids, pInfo->m_muids,
        pInfo->m_serials, pInfo->m_published_labels,
        pInfo->m_unpublished_labels);
    m_pubdescCache[pub] = pInfo;
    return *pInfo;
}

bool
CCacheImpl::SFeatKey::operator<(
    const SFeatKey & rhs) const
{
    if( feat_type != rhs.feat_type ) {
        return feat_type < rhs.feat_type;
    } else if( feat_subtype != rhs.feat_subtype ) {
        return feat_subtype < rhs.feat_subtype;
     } else {
        return bioseq_h < rhs.bioseq_h;
    }
}

bool
CCacheImpl::SFeatKey::operator==(
    const SFeatKey & rhs) const
{
    return (feat_type == rhs.feat_type) &&
        (feat_subtype == rhs.feat_subtype) && (bioseq_h == rhs.bioseq_h);
}

const CCacheImpl::TFeatValue &
CCacheImpl::GetFeatFromCache(
    const CCacheImpl::SFeatKey & featKey)
{
    // check common case where already in the cache
    TFeatCache::iterator find_iter = m_featCache.find(featKey);
    if( find_iter != m_featCache.end() ) {
        return find_iter->second;
    }

    // check if bioseq already processed, but had no entry requested above
    SFeatKey bioseq_check_key(
        kAnyFeatType, kAnyFeatSubtype, featKey.bioseq_h );
    TFeatCache::const_iterator bioseq_find_iter =
        m_featCache.find(bioseq_check_key);
    if( bioseq_find_iter != m_featCache.end() ) {
        // bioseq was already processed,
        // it just happened to not have an entry here
        return kEmptyFeatValue;
    }

    // bioseq never added to cache, so calculate that now

    // to avoid expensive constructions of CFeat_CI's,
    // we iterate through all the seqs on
    // the bioseq and load them into the cache.
    CFeat_CI feat_ci(featKey.bioseq_h);
    for( ; feat_ci; ++feat_ci ) {
        SFeatKey inner_feat_key(
            feat_ci->GetFeatType(), feat_ci->GetFeatSubtype(), featKey.bioseq_h);

        m_featCache[inner_feat_key].push_back(*feat_ci);

        // also add "don't care" entries for partial searches
        // (e.g. if caller just wants to search on type but not on
        // subtype they can set subtype to kAnyFeatSubtype)
        SFeatKey any_type_key = inner_feat_key;
        any_type_key.feat_type = kAnyFeatType;
        m_featCache[any_type_key].push_back(*feat_ci);

        SFeatKey any_subtype_key = inner_feat_key;
        any_subtype_key.feat_subtype = kAnyFeatSubtype;
        m_featCache[any_subtype_key].push_back(*feat_ci);

        // for when the caller wants all feats on a bioseq
        SFeatKey any_type_or_subtype_key = inner_feat_key;
        any_type_or_subtype_key.feat_type = kAnyFeatType;
        any_type_or_subtype_key.feat_subtype = kAnyFeatSubtype;
        m_featCache[any_type_or_subtype_key].push_back(*feat_ci);
    }

    // in case a bioseq has no features, we add a dummy key just to
    // remember that so we don't use CFeat_CI again on the same bioseq
    m_featCache[bioseq_check_key]; // gets default val

    return m_featCache[featKey];
}

AutoPtr<CCacheImpl::TFeatValue>
CCacheImpl::GetFeatFromCacheMulti(
        const vector<SFeatKey> &featKeys)
{
    if( featKeys.empty() ) {
        return new TFeatValue;
    }

    // all featKeys must have the same bioseq
    const CBioseq_Handle & bioseq_h = featKeys[0].bioseq_h;
    ITERATE(vector<SFeatKey>, feat_it, featKeys) {
        if( feat_it->bioseq_h != bioseq_h ) {
            throw runtime_error("GetFeatFromCacheMulti must be called with only 1 bioseq in its args");
        }
    }

    // set prevents dups
    set<TFeatValue::value_type> set_of_feats;

    // combine the answers from every key into the set
    ITERATE(vector<SFeatKey>, key_it, featKeys  ) {
        const TFeatValue & feat_value = GetFeatFromCache(*key_it);
        copy(BEGIN_COMMA_END(feat_value), inserter(
                 set_of_feats, set_of_feats.begin()));
    }

    // go through every feature on the bioseq and remember any that match what's in the set
    // (The purpose of this step is to return the feats in the same
    // order they were on the original bioseq.  In the future, we may
    // consider adding a flag to avoid sorting for time purposes).
    AutoPtr<TFeatValue> answer(new TFeatValue);
    SFeatKey all_feats_key(
        kAnyFeatType, kAnyFeatSubtype, bioseq_h);
    const TFeatValue & all_feats_vec = GetFeatFromCache(all_feats_key);
    ITERATE(TFeatValue, feat_it, all_feats_vec) {
        if( set_of_feats.find(*feat_it) != set_of_feats.end() ) {
            answer->push_back(*feat_it);
        }
    }

    return answer;
}


//LCOV_EXCL_START
//not used
bool
CCacheImpl::SFeatStrKey::operator<(const SFeatStrKey & rhs) const
{
    if( m_eFeatKeyStr != rhs.m_eFeatKeyStr ) {
        return m_eFeatKeyStr < rhs.m_eFeatKeyStr;
    }
    if( m_bioseq != rhs.m_bioseq ) {
        return m_bioseq < rhs.m_bioseq;
    }
    return s_QuickStringLess(m_feat_str, rhs.m_feat_str);
}


bool
CCacheImpl::SFeatStrKey::operator==(const SFeatStrKey & rhs) const
{
    if( m_eFeatKeyStr != rhs.m_eFeatKeyStr ) {
        return false;
    }
    if( m_bioseq != rhs.m_bioseq ) {
        return false;
    }
    return (m_feat_str == rhs.m_feat_str);
}


const CCacheImpl::TFeatValue &
CCacheImpl::GetFeatStrKeyToFeats(
    const SFeatStrKey & feat_str_key, const CTSE_Handle & tse_arg)
{
    const CBioseq_Handle & search_bsh = feat_str_key.m_bioseq;

    // caller must give us something to work with
    _ASSERT(search_bsh || tse_arg);

    const CTSE_Handle & tse = (tse_arg ? tse_arg : search_bsh.GetTSE_Handle());

    // load cache if empty
    if( m_featStrKeyToFeatsCache.empty() ) {
        // (for now just indexes genes, but more may be added in the future)
        SAnnotSelector sel(CSeqFeatData::e_Gene);
        AutoPtr<CFeat_CI> p_gene_ci;
        // if we have TSE, get all features on it; otherwise, just get
        // the features from the bioseq
        if( tse ) {
            p_gene_ci.reset(new CFeat_CI(tse, sel));
        } else {
            p_gene_ci.reset(new CFeat_CI(search_bsh, sel));
        }
        CFeat_CI & gene_ci = *p_gene_ci; // for convenience

        for( ; gene_ci; ++gene_ci ) {
            CBioseq_Handle bsh = tse.GetScope().GetBioseqHandle(gene_ci->GetLocation());
            string label;
            const CGene_ref & gene_ref = gene_ci->GetData().GetGene();

            // for each one, add an entry for using given Bioseq and the
            // kAnyBioseq (so users can search on any bioseq)
            gene_ref.GetLabel(&label);
            SFeatStrKey label_key(eFeatKeyStr_Label, bsh, label);
            m_featStrKeyToFeatsCache[label_key].push_back(*gene_ci);
            if( bsh ) {
                label_key.m_bioseq = kAnyBioseq;
                m_featStrKeyToFeatsCache[label_key].push_back(*gene_ci);
            }

            const string & locus_tag = (
                gene_ref.IsSetLocus_tag() ? gene_ref.GetLocus_tag() :
                kEmptyStr);
            SFeatStrKey locus_tag_key(eFeatKeyStr_LocusTag, bsh, locus_tag);
            m_featStrKeyToFeatsCache[locus_tag_key].push_back(*gene_ci);
            if( bsh ) {
                locus_tag_key.m_bioseq = kAnyBioseq;
                m_featStrKeyToFeatsCache[locus_tag_key].push_back(*gene_ci);
            }
        }
    }

    // get from cache, if possible
    TFeatStrKeyToFeatsCache::const_iterator find_iter =
        m_featStrKeyToFeatsCache.find(feat_str_key);
    if( find_iter != m_featStrKeyToFeatsCache.end() ) {
        return find_iter->second;
    } else {
        // nothing found
        return kEmptyFeatValue;
    }
}


const CCacheImpl::TFeatToBioseqValue &
CCacheImpl::GetBioseqsOfFeatCache(
    const CCacheImpl::TFeatToBioseqKey & feat_to_bioseq_key,
    const CTSE_Handle & tse)
{
    // load cache if empty
    if( m_featToBioseqCache.empty() ) {
        CBioseq_CI bioseq_ci(tse);
        for( ; bioseq_ci; ++bioseq_ci ) {
            CFeat_CI feat_ci(*bioseq_ci);
            for( ;  feat_ci; ++feat_ci ) {
                m_featToBioseqCache[*feat_ci].insert(*bioseq_ci);
            }
        }
    }

    // we're being given the map to a feature, so we should've loaded
    // at least one feature when we loaded the cache
    _ASSERT( ! m_featToBioseqCache.empty() );

    // load from the cache
    TFeatToBioseqCache::const_iterator find_iter =
        m_featToBioseqCache.find(feat_to_bioseq_key);
    if( find_iter != m_featToBioseqCache.end() ) {
        return find_iter->second;
    } else {
        static CSafeStatic<TFeatToBioseqValue> kEmptyFeatToBioseqCache;
        return kEmptyFeatToBioseqCache.Get();
    }
}
//LCOV_EXCL_STOP

const CCacheImpl::TIdToBioseqValue &
CCacheImpl::GetIdToBioseq(
    const CCacheImpl::TIdToBioseqKey & key,
    const CTSE_Handle & tse)
{
    _ASSERT(tse);

    // load cache if empty
    if( m_IdToBioseqCache.empty() ) {
        CBioseq_CI bioseq_ci(tse);
        for( ; bioseq_ci; ++bioseq_ci ) {
            const CBioseq_Handle::TId & ids = bioseq_ci->GetId();
            ITERATE(CBioseq_Handle::TId, id_it, ids) {
                m_IdToBioseqCache[id_it->GetSeqId()] = *bioseq_ci;
            }
        }
    }

    // there should be at least one Bioseq otherwise there wouldn't
    // be anything to validate.
    _ASSERT(! m_IdToBioseqCache.empty());

    TIdToBioseqCache::const_iterator find_iter = m_IdToBioseqCache.find(key);
    if( find_iter != m_IdToBioseqCache.end() ) {
        return find_iter->second;
    } else {
        static CSafeStatic<TIdToBioseqValue> s_EmptyResult;
        return s_EmptyResult.Get();
    }
}

CBioseq_Handle
CCacheImpl::GetBioseqHandleFromLocation(
    CScope *scope, const CSeq_loc& loc, const CTSE_Handle & tse)
{
    _ASSERT(scope || tse);
    if( ! tse  || (!tse.GetTopLevelEntry().IsSet() && !tse.GetTopLevelEntry().IsSeq())) {
        // fall back on old style
        return BioseqHandleFromLocation(scope, loc);
    }

    for ( CSeq_loc_CI citer (loc); citer; ++citer) {
        CConstRef<CSeq_id> id(&citer.GetSeq_id());
        const TIdToBioseqValue & bioseq = GetIdToBioseq(id, tse);
        if( bioseq ) {
            return bioseq;
        }
    }

    // nothing found, so fall back on old style if possible
    if( scope ) {
        return BioseqHandleFromLocation(scope, loc);
    } else {
        return kEmptyBioseqHandle;
    }
}


void CCacheImpl::Clear()
{
    m_pubdescCache.clear();
    m_featCache.clear();
    m_featStrKeyToFeatsCache.clear();
    m_featToBioseqCache.clear();
    m_IdToBioseqCache.clear();
}


END_SCOPE(validator)
END_SCOPE(objects)
END_NCBI_SCOPE
