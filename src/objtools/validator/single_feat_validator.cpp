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
 * Author:  Jonathan Kans, Clifford Clausen, Aaron Ucko......
 *
 * File Description:
 *   validation of Seq_feat
 *   .......
 *
 */
#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>
#include <corelib/ncbistr.hpp>
#include <objtools/validator/validatorp.hpp>
#include <objtools/validator/single_feat_validator.hpp>
#include <objtools/validator/validerror_feat.hpp>
#include <objtools/validator/utilities.hpp>
#include <objtools/validator/go_term_validation_and_cleanup.hpp>
#include <objtools/format/items/gene_finder.hpp>

#include <serial/serialbase.hpp>

#include <objmgr/seqdesc_ci.hpp>
#include <objects/seq/seqport_util.hpp>
#include <util/sequtil/sequtil_convert.hpp>
#include <util/sgml_entity.hpp>


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)
BEGIN_SCOPE(validator)
using namespace sequence;

CSingleFeatValidator::CSingleFeatValidator(const CSeq_feat& feat, CScope& scope, CValidError_imp& imp) 
    : m_Feat(feat), m_Scope(scope), m_Imp(imp), m_ProductIsFar(false)
{

}


void CSingleFeatValidator::Validate()
{
    if (!m_Feat.IsSetLocation()) {
        PostErr(eDiag_Critical, eErr_SEQ_FEAT_MissingLocation,
            "The feature is missing a location");
        return;
    }

    m_LocationBioseq = x_GetBioseqByLocation(m_Feat.GetLocation());
    bool lowerSev = false;
    if (m_Imp.IsRefSeq() && m_Feat.GetData().GetSubtype() == CSeqFeatData::eSubtype_variation) {
        if ( m_Feat.IsSetDbxref() ) {
            ITERATE( CSeq_feat::TDbxref, it, m_Feat.GetDbxref() ) {
                const CDbtag& dbtag = **it;
                if ( dbtag.GetDb() == "dbSNP" ) {
                    lowerSev = true;
                }
            }
        }
    }
    m_Imp.ValidateSeqLoc(m_Feat.GetLocation(), m_LocationBioseq,
        (m_Feat.GetData().IsGene() || !m_Imp.IsGpipe()),
        "Location", m_Feat, lowerSev);

    x_ValidateSeqFeatLoc();
    x_ValidateImpFeatLoc();

    if (m_Feat.IsSetProduct()) {
        m_ProductBioseq = x_GetFeatureProduct(m_ProductIsFar);
        if (m_ProductBioseq == m_LocationBioseq) {
            PostErr(eDiag_Error, eErr_SEQ_FEAT_SelfReferentialProduct, "Self-referential feature product");
        }
        x_ValidateSeqFeatProduct();
    }

    x_ValidateFeatPartialness();

    x_ValidateBothStrands();

    if (m_Feat.CanGetDbxref()) {
        m_Imp.ValidateDbxref(m_Feat.GetDbxref(), m_Feat);
        x_ValidateGeneId();
    }

    x_ValidateFeatComment();

    x_ValidateFeatCit();

    if (m_Feat.IsSetQual()) {
        for (auto it = m_Feat.GetQual().begin(); it != m_Feat.GetQual().end(); it++) {
            x_ValidateGbQual(**it);
        }
    }

    x_ValidateExtUserObject();

    if (m_Feat.IsSetExp_ev() && m_Feat.GetExp_ev() > 0 &&
        !x_HasNamedQual("inference") &&
        !x_HasNamedQual("experiment") &&
        !m_Imp.DoesAnyFeatLocHaveGI()) {
        PostErr(eDiag_Warning, eErr_SEQ_FEAT_InvalidInferenceValue,
            "Inference or experiment qualifier missing but obsolete experimental evidence qualifier set");
    }

    x_ValidateExcept();

    x_ValidateGbquals();
    x_ValidateImpFeatQuals();

    x_ValidateSeqFeatDataType();

    x_ValidateNonImpFeat();

    x_ValidateNonGene();

    x_CheckForNonAsciiCharacters();
}

void CSingleFeatValidator::PostErr(EDiagSev sv, EErrType et, const string& msg)
{
    m_Imp.PostErr(sv, et, msg, m_Feat);
}


CBioseq_Handle
CSingleFeatValidator::x_GetBioseqByLocation(const CSeq_loc& loc)
{
    if (loc.IsInt() || loc.IsWhole()) {
        return m_Scope.GetBioseqHandle(loc);
    }
    CBioseq_Handle rval;
    CConstRef<CSeq_id> prev(NULL);
    for (CSeq_loc_CI citer(loc); citer; ++citer) {
        const CSeq_id& this_id = citer.GetSeq_id();
        if (!prev || !prev->Equals(this_id)) {
            rval = m_Scope.GetBioseqHandle(this_id);
            if (rval) {
                break;
            }
            prev.Reset(&this_id);
        }
    }
    return rval;
}


void CSingleFeatValidator::x_ValidateSeqFeatProduct()
{
    if (!m_Feat.IsSetProduct()) {
        return;
    }
    const CSeq_id& sid = GetId(m_Feat.GetProduct(), &m_Scope);

    switch (sid.Which()) {
    case CSeq_id::e_Genbank:
    case CSeq_id::e_Embl:
    case CSeq_id::e_Ddbj:
    case CSeq_id::e_Tpg:
    case CSeq_id::e_Tpe:
    case CSeq_id::e_Tpd:
    {
        const CTextseq_id* tsid = sid.GetTextseq_Id();
        if (tsid != NULL) {
            if (!tsid->CanGetAccession() && tsid->CanGetName()) {
                if (ValidateAccessionString(tsid->GetName(), false) == eAccessionFormat_valid) {
                    PostErr(eDiag_Warning, eErr_SEQ_FEAT_BadProductSeqId,
                        "Feature product should not put an accession in the Textseq-id 'name' slot");
                } else {
                    PostErr(eDiag_Warning, eErr_SEQ_FEAT_BadProductSeqId,
                        "Feature product should not use "
                        "Textseq-id 'name' slot");
                }
            }
        }
    }
    break;

    default:
        break;
    }

    if (m_ProductBioseq) {
        m_Imp.ValidateSeqLoc(m_Feat.GetProduct(), m_ProductBioseq, true, "Product", m_Feat);

        for (auto id : m_ProductBioseq.GetCompleteBioseq()->GetId()) {
            if (id->Which() == sid.Which()) {
                // check to make sure capitalization is the same
                string from_seq = id->AsFastaString();
                string from_loc = sid.AsFastaString();
                if (!NStr::EqualCase(from_seq, from_loc) &&
                    NStr::EqualNocase(from_seq, from_loc)) {
                    PostErr(eDiag_Critical, eErr_SEQ_FEAT_BadProductSeqId,
                        "Capitalization change from product location on feature to product sequence");
                }
            }
            switch (id->Which()) {
            case CSeq_id::e_Genbank:
            case CSeq_id::e_Embl:
            case CSeq_id::e_Ddbj:
            case CSeq_id::e_Tpg:
            case CSeq_id::e_Tpe:
            case CSeq_id::e_Tpd:
            {
                const CTextseq_id* tsid = id->GetTextseq_Id();
                if (tsid != NULL) {
                    if (!tsid->IsSetAccession() && tsid->IsSetName()) {
                        if (ValidateAccessionString(tsid->GetName(), false) == eAccessionFormat_valid) {
                            PostErr(eDiag_Warning, eErr_SEQ_FEAT_BadProductSeqId,
                                "Protein bioseq has Textseq-id 'name' that "
                                "looks like it is derived from a nucleotide "
                                "accession");
                        } else {
                            PostErr(eDiag_Warning, eErr_SEQ_FEAT_BadProductSeqId,
                                "Protein bioseq has Textseq-id 'name' and no accession");
                        }
                    }
                }
            }
            break;
            default:
                break;
            }
        }
    }
}


bool CSingleFeatValidator::x_HasSeqLocBond(const CSeq_feat& feat)
{
    // check for bond locations - only allowable in bond feature and under special circumstances for het
    bool is_seqloc_bond = false;
    if (feat.IsSetData()) {
        if (feat.GetData().IsHet()) {
            // heterogen can have mix of bonds with just "a" point specified */
            for (CSeq_loc_CI it(feat.GetLocation()); it; ++it) {
                if (it.GetEmbeddingSeq_loc().IsBond()
                    && (!it.GetEmbeddingSeq_loc().GetBond().IsSetA()
                    || it.GetEmbeddingSeq_loc().GetBond().IsSetB())) {
                    is_seqloc_bond = true;
                    break;
                }
            }
        } else if (!feat.GetData().IsBond()) {
            for (CSeq_loc_CI it(feat.GetLocation()); it; ++it) {
                if (it.GetEmbeddingSeq_loc().IsBond()) {
                    is_seqloc_bond = true;
                    break;
                }
            }
        }
    } else {
        for (CSeq_loc_CI it(feat.GetLocation()); it; ++it) {
            if (it.GetEmbeddingSeq_loc().IsBond()) {
                is_seqloc_bond = true;
                break;
            }
        }
    }
    return is_seqloc_bond;
}


void CSingleFeatValidator::x_ValidateBothStrands()
{
    if (!m_Feat.IsSetLocation() || CSeqFeatData::AllowStrandBoth(m_Feat.GetData().GetSubtype())) {
        return;
    }
    bool both, both_rev;
    x_LocHasStrandBoth(m_Feat.GetLocation(), both, both_rev);
    if (both || both_rev) {
        string suffix = "";
        if (both && both_rev) {
            suffix = "(forward and reverse)";
        } else if (both) {
            suffix = "(forward)";
        } else if (both_rev) {
            suffix = "(reverse)";
        }

        string label = CSeqFeatData::SubtypeValueToName(m_Feat.GetData().GetSubtype());

        PostErr(eDiag_Error, eErr_SEQ_FEAT_BothStrands,
            label + " may not be on both " + suffix + " strands");
    }
}


void CSingleFeatValidator::x_LocHasStrandBoth(const CSeq_loc& loc, bool& both, bool& both_rev)
{
    both = false;
    both_rev = false;
    for (CSeq_loc_CI it(loc); it; ++it) {
        if (it.IsSetStrand()) {
            ENa_strand s = it.GetStrand();
            if (s == eNa_strand_both && !both) {
                both = true;
            } else if (s == eNa_strand_both_rev && !both_rev) {
                both_rev = true;
            }
        }
        if (both && both_rev) {
            break;
        }
    }
}


bool HasGeneIdXref(const CMappedFeat& sf, const CObject_id& tag, bool& has_parent_gene_id)
{
    has_parent_gene_id = false;
    if (!sf.IsSetDbxref()) {
        return false;
    }
    ITERATE(CSeq_feat::TDbxref, it, sf.GetDbxref()) {
        if ((*it)->IsSetDb() && NStr::EqualNocase((*it)->GetDb(), "GeneID")) {
            has_parent_gene_id = true;
            if ((*it)->IsSetTag() && (*it)->GetTag().Equals(tag)) {
                return true;
            }
        }
    }
    return false;
}


void CSingleFeatValidator::x_ValidateGeneId()
{
    if (!m_Feat.IsSetDbxref()) {
        return;
    }

    // no tse, no feat-handle
    auto tse = m_Imp.GetTSE_Handle();
    if (!tse) {
        return;
    }

    CRef<feature::CFeatTree> feat_tree(NULL);
    CMappedFeat mf = m_Scope.GetSeq_featHandle(m_Feat);
    ITERATE(CSeq_feat::TDbxref, it, m_Feat.GetDbxref()) {
        if ((*it)->IsSetDb() && NStr::EqualNocase((*it)->GetDb(), "GeneID") &&
            (*it)->IsSetTag()) {
            if (!feat_tree) {
                feat_tree = m_Imp.GetGeneCache().GetFeatTreeFromCache(m_Feat, m_Scope);
            }
            if (feat_tree) {
                CMappedFeat parent = feat_tree->GetParent(mf);
                while (parent) {
                    bool has_parent_gene_id = false;
                    if (!HasGeneIdXref(parent, (*it)->GetTag(), has_parent_gene_id)) {
                        if (has_parent_gene_id || 
                            parent.GetData().GetSubtype() == CSeqFeatData::eSubtype_gene) {
                            PostErr(eDiag_Error, eErr_SEQ_FEAT_GeneIdMismatch,
                                "GeneID mismatch");
                        }
                    }
                    parent = feat_tree->GetParent(parent);
                }
            }
        }
    }

}


void CSingleFeatValidator::x_ValidateFeatCit()
{
    if (!m_Feat.IsSetCit()) {
        return;
    }

    if (m_Feat.GetCit().IsPub()) {
        ITERATE(CPub_set::TPub, pi, m_Feat.GetCit().GetPub()) {
            if ((*pi)->IsEquiv()) {
                PostErr(eDiag_Warning, eErr_SEQ_FEAT_UnnecessaryCitPubEquiv,
                    "Citation on feature has unexpected internal Pub-equiv");
                return;
            }
        }
    }
}


const string kInferenceMessage[] = {
  "unknown error",
  "empty inference string",
  "bad inference prefix",
  "bad inference body",
  "single inference field",
  "spaces in inference",
  "possible comment in inference",
  "same species misused",
  "the value in the accession field is not legal. The only allowed value is accession.version, eg AF123456.1. Problem =",
  "bad inference accession version",
  "accession.version not public",
  "bad accession type",
  "unrecognized database",
};


void CSingleFeatValidator::x_ValidateGbQual(const CGb_qual& qual)
{
    if (!qual.IsSetQual()) {
        return;
    }
    /* first check for anything other than replace */
    if (!qual.IsSetVal() || NStr::IsBlank(qual.GetVal())) {
        if (NStr::EqualNocase(qual.GetQual(), "replace")) {
            /* ok for replace */
        } else {
            PostErr(eDiag_Warning, eErr_SEQ_FEAT_InvalidPunctuation,
                "Qualifier other than replace has just quotation marks");
            if (NStr::EqualNocase(qual.GetQual(), "EC_number")) {
                PostErr(eDiag_Warning, eErr_SEQ_FEAT_EcNumberEmpty, "EC number should not be empty");
            }
        }
        if (NStr::EqualNocase(qual.GetQual(), "inference")) {
            PostErr(eDiag_Warning, eErr_SEQ_FEAT_InvalidInferenceValue, 
                "Inference qualifier problem - empty inference string ()");
        } else if (NStr::EqualNocase(qual.GetQual(), "pseudogene")) {
            PostErr(eDiag_Warning, eErr_SEQ_FEAT_InvalidPseudoQualifier, "/pseudogene value should not be empty");
        }
    } else if (NStr::EqualNocase(qual.GetQual(), "EC_number")) {
        if (!CProt_ref::IsValidECNumberFormat(qual.GetVal())) {
            PostErr(eDiag_Warning, eErr_SEQ_FEAT_BadEcNumberFormat,
                qual.GetVal() + " is not in proper EC_number format");
        } else {
            string ec_number = qual.GetVal();
            CProt_ref::EECNumberStatus status = CProt_ref::GetECNumberStatus(ec_number);
            x_ReportECNumFileStatus();
            switch (status) {
            case CProt_ref::eEC_deleted:
                PostErr(eDiag_Warning, eErr_SEQ_FEAT_DeletedEcNumber,
                    "EC_number " + ec_number + " was deleted");
                break;
            case CProt_ref::eEC_replaced:
                PostErr(eDiag_Warning,
                    CProt_ref::IsECNumberSplit(ec_number) ? eErr_SEQ_FEAT_SplitEcNumber : eErr_SEQ_FEAT_ReplacedEcNumber,
                    "EC_number " + ec_number + " was replaced");
                break;
            case CProt_ref::eEC_unknown:
            {
                size_t pos = NStr::Find(ec_number, "n");
                if (pos == string::npos || !isdigit(ec_number.c_str()[pos + 1])) {
                    PostErr(eDiag_Warning, eErr_SEQ_FEAT_BadEcNumberValue,
                        ec_number + " is not a legal value for qualifier EC_number");
                } else {
                    PostErr(eDiag_Info, eErr_SEQ_FEAT_BadEcNumberValue,
                        ec_number + " is not a legal preliminary value for qualifier EC_number");
                }
            }
            break;
            default:
                break;
            }
        }
    } else if (NStr::EqualNocase(qual.GetQual(), "inference")) {
        /* TODO: Validate inference */
        string val = "";
        if (qual.IsSetVal()) {
            val = qual.GetVal();
        }
        CValidError_feat::EInferenceValidCode rsult = CValidError_feat::ValidateInference(val, m_Imp.ValidateInferenceAccessions());
        if (rsult > CValidError_feat::eInferenceValidCode_valid) {
            if (NStr::IsBlank(val)) {
                val = "?";
            }
            PostErr(eDiag_Warning, eErr_SEQ_FEAT_InvalidInferenceValue,
                "Inference qualifier problem - " + kInferenceMessage[(int)rsult] + " ("
                + val + ")");
        }
    } else if (NStr::EqualNocase(qual.GetQual(), "pseudogene")) {
        m_Imp.IncrementPseudogeneCount();
        if (!CGb_qual::IsValidPseudogeneValue(qual.GetVal())) {
            m_Imp.PostErr(eDiag_Warning, eErr_SEQ_FEAT_InvalidPseudoQualifier,
                "/pseudogene value should not be '" + qual.GetVal() + "'", m_Feat);
        }
    } else if (NStr::EqualNocase(qual.GetQual(), "number")) {
        bool has_space = false;
        bool has_char_after_space = false;
        ITERATE(string, it, qual.GetVal()) {
            if (isspace((unsigned char)(*it))) {
                has_space = true;
            } else if (has_space) {
                // non-space after space
                has_char_after_space = true;
                break;
            }
        }
        if (has_char_after_space) {
            PostErr(eDiag_Error, eErr_SEQ_FEAT_InvalidNumberQualifier,
                "Number qualifiers should not contain spaces");
        }
    }
    if (qual.IsSetVal() && ContainsSgml(qual.GetVal())) {
        PostErr(eDiag_Warning, eErr_GENERIC_SgmlPresentInText,
            "feature qualifier " + qual.GetVal() + " has SGML");
    }

}


void CSingleFeatValidator::x_ReportECNumFileStatus()
{
    static bool file_status_reported = false;

    if (!file_status_reported) {
        if (CProt_ref::GetECNumAmbiguousStatus() == CProt_ref::eECFile_not_found) {
            PostErr(eDiag_Warning, eErr_SEQ_FEAT_EcNumberDataMissing,
                "Unable to find EC number file 'ecnum_ambiguous.txt' in data directory");
        }
        if (CProt_ref::GetECNumDeletedStatus() == CProt_ref::eECFile_not_found) {
            PostErr(eDiag_Warning, eErr_SEQ_FEAT_EcNumberDataMissing,
                "Unable to find EC number file 'ecnum_deleted.txt' in data directory");
        }
        if (CProt_ref::GetECNumReplacedStatus() == CProt_ref::eECFile_not_found) {
            PostErr(eDiag_Warning, eErr_SEQ_FEAT_EcNumberDataMissing,
                "Unable to find EC number file 'ecnum_replaced.txt' in data directory");
        }
        if (CProt_ref::GetECNumSpecificStatus() == CProt_ref::eECFile_not_found) {
            PostErr(eDiag_Warning, eErr_SEQ_FEAT_EcNumberDataMissing,
                "Unable to find EC number file 'ecnum_specific.txt' in data directory");
        }
        file_status_reported = true;
    }
}


void CSingleFeatValidator::x_ValidateExtUserObject()
{
    vector<TGoTermError> errors = GetGoTermErrors(m_Feat);
    for (auto it : errors) {
        PostErr(it.first == eErr_SEQ_FEAT_DuplicateGeneOntologyTerm ? eDiag_Info : eDiag_Warning,
            it.first, it.second);
    }
}


bool CSingleFeatValidator::x_HasNamedQual(const string& qual_name)
{
    if (!m_Feat.IsSetQual()) {
        return false;
    }
    ITERATE(CSeq_feat::TQual, it, m_Feat.GetQual()) {
        if ((*it)->IsSetQual() && NStr::EqualNocase((*it)->GetQual(), qual_name)) {
            return true;
        }
    }
    return false;
}


void CSingleFeatValidator::x_ValidateFeatComment()
{
    if (!m_Feat.IsSetComment()) {
        return;
    }
    const string& comment = m_Feat.GetComment();
    if (m_Imp.IsSerialNumberInComment(comment)) {
        m_Imp.PostErr(eDiag_Info, eErr_SEQ_FEAT_SerialInComment,
            "Feature comment may refer to reference by serial number - "
            "attach reference specific comments to the reference "
            "REMARK instead.", m_Feat);
    }
    if (ContainsSgml(comment)) {
        m_Imp.PostErr(eDiag_Warning, eErr_GENERIC_SgmlPresentInText,
            "feature comment " + comment + " has SGML",
            m_Feat);
    }
}


void CSingleFeatValidator::x_ValidateFeatPartialness()
{
    unsigned int  partial_prod = eSeqlocPartial_Complete,
        partial_loc = eSeqlocPartial_Complete;

    bool is_partial = m_Feat.IsSetPartial() && m_Feat.GetPartial();
    partial_loc = SeqLocPartialCheck(m_Feat.GetLocation(), &m_Scope);

    if (m_ProductBioseq) {
        partial_prod = SeqLocPartialCheck(m_Feat.GetProduct(), &(m_ProductBioseq.GetScope()));
    }

    if ((partial_loc != eSeqlocPartial_Complete) ||
        (partial_prod != eSeqlocPartial_Complete) ||
        is_partial) {

        // a feature on a partial sequence should be partial -- it often isn't
        if (!is_partial &&
            partial_loc != eSeqlocPartial_Complete  &&
            m_Feat.IsSetLocation() &&
            m_Feat.GetLocation().IsWhole()) {
            PostErr(eDiag_Info, eErr_SEQ_FEAT_PartialProblem,
                "On partial Bioseq, SeqFeat.partial should be TRUE");
        }
        // a partial feature, with complete location, but partial product
        else if (is_partial                        &&
            partial_loc == eSeqlocPartial_Complete  &&
            m_Feat.IsSetProduct() &&
            m_Feat.GetProduct().IsWhole() &&
            partial_prod != eSeqlocPartial_Complete) {
            if (m_Imp.IsGenomic() && m_Imp.IsGpipe()) {
                // suppress in gpipe genomic
            } else {
                PostErr(eDiag_Warning, eErr_SEQ_FEAT_PartialProblem,
                    "When SeqFeat.product is a partial Bioseq, SeqFeat.location "
                    "should also be partial");
            }
        }
        // gene on segmented set is now 'order', should also be partial
        else if (m_Feat.GetData().IsGene() &&
            !is_partial                      &&
            partial_loc == eSeqlocPartial_Internal) {
            PostErr(eDiag_Warning, eErr_SEQ_FEAT_PartialProblem,
                "Gene of 'order' with otherwise complete location should "
                "have partial flag set");
        }
        // inconsistent combination of partial/complete product,location,partial flag - part 1
        else if (partial_prod == eSeqlocPartial_Complete  &&  m_Feat.IsSetProduct()) {
            // if not local bioseq product, lower severity
            EDiagSev sev = eDiag_Warning;
            bool is_far_fail = false;
            if (!m_ProductBioseq || m_ProductIsFar) {
                sev = eDiag_Info;
                if (!m_ProductBioseq && m_Imp.x_IsFarFetchFailure(m_Feat.GetProduct())) {
                    is_far_fail = true;
                }
            }

            string str("Inconsistent: Product= complete, Location= ");
            str += (partial_loc != eSeqlocPartial_Complete) ? "partial, " : "complete, ";
            str += "Feature.partial= ";
            str += is_partial ? "TRUE" : "FALSE";
            if (m_Imp.IsGenomic() && m_Imp.IsGpipe()) {
                // suppress for genomic gpipe
            } else if (is_far_fail) {
                m_Imp.SetFarFetchFailure();
            } else {
                PostErr(sev, eErr_SEQ_FEAT_PartialsInconsistent, str);
            }
        }
        // inconsistent combination of partial/complete product,location,partial flag - part 2
        else if (partial_loc == eSeqlocPartial_Complete || !is_partial) {
            string str("Inconsistent: ");
            if (m_Feat.IsSetProduct()) {
                str += "Product= ";
                str += (partial_prod != eSeqlocPartial_Complete) ? "partial, " : "complete, ";
            }
            str += "Location= ";
            str += (partial_loc != eSeqlocPartial_Complete) ? "partial, " : "complete, ";
            str += "Feature.partial= ";
            str += is_partial ? "TRUE" : "FALSE";
            if (m_Imp.IsGenomic() && m_Imp.IsGpipe()) {
                // suppress for genomic gpipe
            } else {
                PostErr(eDiag_Warning, eErr_SEQ_FEAT_PartialsInconsistent, str);
            }
        }
        // 5' or 3' partial location giving unclassified partial product
        else if ((((partial_loc & eSeqlocPartial_Start) != 0) ||
            ((partial_loc & eSeqlocPartial_Stop) != 0)) &&
            ((partial_prod & eSeqlocPartial_Other) != 0) &&
            is_partial) {
            PostErr(eDiag_Warning, eErr_SEQ_FEAT_PartialProblem,
                "5' or 3' partial location should not have unclassified"
                " partial in product molinfo descriptor");
        }

        // note - in analogous C Toolkit function there is additional code for ensuring
        // that partial intervals are partial at splice sites, gaps, or the ends of the
        // sequence.  This has been moved to CValidError_bioseq::ValidateFeatPartialInContext.
    }

}


void CSingleFeatValidator::x_ValidateSeqFeatLoc()
{
    if (x_HasSeqLocBond(m_Feat)) {
        PostErr(eDiag_Warning, eErr_SEQ_FEAT_ImproperBondLocation,
            "Bond location should only be on bond features");
    }

    // feature location should not be whole
    if (m_Feat.GetLocation().IsWhole()) {
        string prefix = "Feature";
        if (m_Feat.IsSetData()) {
            if (m_Feat.GetData().IsCdregion()) {
                prefix = "CDS";
            } else if (m_Feat.GetData().GetSubtype() == CSeqFeatData::eSubtype_mRNA) {
                prefix = "mRNA";
            }
        }
        PostErr(eDiag_Warning, eErr_SEQ_FEAT_WholeLocation, prefix + " may not have whole location");
    }

    if (m_LocationBioseq) {
        // look for mismatch in capitalization for IDs
        CNcbiOstrstream os;
        const CSeq_id *id = m_Feat.GetLocation().GetId();
        if (id) {
            id->WriteAsFasta(os);
            string loc_id = CNcbiOstrstreamToString(os);
            FOR_EACH_SEQID_ON_BIOSEQ(it, *(m_LocationBioseq.GetCompleteBioseq())) {
                if ((*it)->IsGi() || (*it)->IsGibbsq() || (*it)->IsGibbmt()) {
                    continue;
                }
                CNcbiOstrstream os2;
                (*it)->WriteAsFasta(os2);
                string bs_id = CNcbiOstrstreamToString(os2);
                if (NStr::EqualNocase(loc_id, bs_id) && !NStr::EqualCase(loc_id, bs_id)) {
                    PostErr(eDiag_Error, eErr_SEQ_FEAT_FeatureSeqIDCaseDifference,
                        "Sequence identifier in feature location differs in capitalization with identifier on Bioseq");
                }
            }
        }
        // look for protein features on the minus strand
        if (m_LocationBioseq.IsAa() && m_Feat.GetLocation().IsSetStrand() &&
            m_Feat.GetLocation().GetStrand() == eNa_strand_minus) {
            PostErr(eDiag_Warning, eErr_SEQ_FEAT_MinusStrandProtein,
                "Feature on protein indicates negative strand");
        }

        if (!m_Feat.GetData().IsImp()
            || !m_Feat.GetData().GetImp().IsSetKey()
            || !NStr::EqualNocase(m_Feat.GetData().GetImp().GetKey(), "gap")) {
            try {
                vector<TSeqPos> gap_starts;
                size_t rval = x_CalculateLocationGaps(m_LocationBioseq, m_Feat.GetLocation(), gap_starts);
                bool mostly_raw_ns = x_IsMostlyNs(m_Feat.GetLocation(), m_LocationBioseq);

                if ((rval & eLocationGapMostlyNs) || mostly_raw_ns) {
                    PostErr(eDiag_Info, eErr_SEQ_FEAT_FeatureIsMostlyNs,
                        "Feature contains more than 50% Ns");
                }
                for (auto gap_start : gap_starts) {
                    PostErr(eDiag_Warning, eErr_SEQ_FEAT_FeatureBeginsOrEndsInGap,
                        "Feature begins or ends in gap starting at " + NStr::NumericToString(gap_start + 1));
                }
                if (rval & eLocationGapContainedInGap &&
                    (!(rval & eLocationGapFeatureMatchesGap) || !x_AllowFeatureToMatchGapExactly())) {
                    PostErr(eDiag_Warning, eErr_SEQ_FEAT_FeatureInsideGap,
                        "Feature inside sequence gap");
                }
                if (m_Feat.GetData().IsCdregion() || m_Feat.GetData().IsRna()) {
                    if (rval & eLocationGapInternalIntervalEndpointInGap) {
                        PostErr(eDiag_Info, eErr_SEQ_FEAT_IntervalBeginsOrEndsInGap,
                            "Internal interval begins or ends in gap");
                    }
                    if (rval & eLocationGapCrossesUnknownGap) {
                        PostErr(eDiag_Warning, eErr_SEQ_FEAT_FeatureCrossesGap,
                            "Feature crosses gap of unknown length");
                    }
                }
            } catch (CException &e) {
                PostErr(eDiag_Fatal, eErr_INTERNAL_Exception,
                    string("Exception while checking for intervals in gaps. EXCEPTION: ") +
                    e.what());
            } catch (std::exception) {
            }
        }
    }

}


bool CSingleFeatValidator::x_AllowFeatureToMatchGapExactly()
{
    if (m_Feat.GetData().GetSubtype() == CSeqFeatData::eSubtype_misc_feature ||
        m_Feat.GetData().GetSubtype() == CSeqFeatData::eSubtype_assembly_gap) {
        return true;
    } else {
        return false;
    }
}


class CGapCache {
public:
    CGapCache(const CSeq_loc& loc, CBioseq_Handle bsh);
    ~CGapCache() {};
    bool IsUnknownGap(size_t offset);
    bool IsKnownGap(size_t offset);
    bool IsGap(size_t offset);

private:
    typedef enum {
        eGapType_unknown = 0,
        eGapType_known
    } EGapType;
    typedef map <size_t, EGapType> TGapTypeMap;
    TGapTypeMap m_Map;
    size_t m_NumUnknown;
    size_t m_NumKnown;
};

CGapCache::CGapCache(const CSeq_loc& loc, CBioseq_Handle bsh)
{
    TSeqPos start = loc.GetStart(eExtreme_Positional);
    TSeqPos stop = loc.GetStop(eExtreme_Positional);
    CRange<TSeqPos> range(start, stop);
    CSeqMap_CI map_iter(bsh, SSeqMapSelector(CSeqMap::fDefaultFlags, 100), range);
    TSeqPos pos = start;
    while (map_iter && pos <= stop) {
        TSeqPos map_end = map_iter.GetPosition() + map_iter.GetLength();
        if (map_iter.GetType() == CSeqMap::eSeqGap) {
            for (; pos < map_end && pos <= stop; pos++) {
                if (map_iter.IsUnknownLength()) {
                    m_Map[pos - start] = eGapType_unknown;
                    m_NumUnknown++;
                } else {
                    m_Map[pos - start] = eGapType_known;
                    m_NumKnown++;
                }
            }
        } else {
            pos = map_end;
        }
        ++map_iter;
    }
}

bool CGapCache::IsGap(size_t pos)
{
    if (m_Map.find(pos) != m_Map.end()) {
        return true;
    } else {
        return false;
    }
}


bool CGapCache::IsKnownGap(size_t pos)
{
    TGapTypeMap::iterator it = m_Map.find(pos);
    if (it == m_Map.end()) {
        return false;
    } else if (it->second == eGapType_known) {
        return true;
    } else {
        return false;
    }
}


bool CGapCache::IsUnknownGap(size_t pos)
{
    TGapTypeMap::iterator it = m_Map.find(pos);
    if (it == m_Map.end()) {
        return false;
    } else if (it->second == eGapType_unknown) {
        return true;
    } else {
        return false;
    }
}


static bool xf_IsDeltaLitOnly (CBioseq_Handle bsh)

{
    if ( bsh.IsSetInst_Ext() ) {
        const CBioseq_Handle::TInst_Ext& ext = bsh.GetInst_Ext();
        if ( ext.IsDelta() ) {
            ITERATE (CDelta_ext::Tdata, it, ext.GetDelta().Get()) {
                if ( (*it)->IsLoc() ) {
                    return false;
                }
            }
        }
    }
    return true;
}


size_t CSingleFeatValidator::x_CalculateLocationGaps(CBioseq_Handle bsh, const CSeq_loc& loc, vector<TSeqPos>& gap_starts)
{
    size_t rval = eLocationGapNoProblems;
    if (!bsh.IsNa() || !bsh.IsSetInst_Repr() || bsh.GetInst().GetRepr() != CSeq_inst::eRepr_delta) {
        return rval;
    }
    // look for features inside gaps, crossing unknown gaps, or starting or ending in gaps
    // ignore gap features for this
    int num_n = 0;
    int num_real = 0;
    int num_gap = 0;
    int num_unknown_gap = 0;
    bool first_in_gap = false, last_in_gap = false;
    bool local_first_gap = false, local_last_gap = false;
    bool startsOrEndsInGap = false;
    bool first = true;

    for (CSeq_loc_CI loc_it(loc); loc_it; ++loc_it) {
        CConstRef<CSeq_loc> this_loc = loc_it.GetRangeAsSeq_loc();
        CSeqVector vec = GetSequenceFromLoc(*this_loc, bsh.GetScope());
        if (!vec.empty()) {
            CBioseq_Handle ph;
            bool match = false;
            for (auto id_it : bsh.GetBioseqCore()->GetId()) {
                if (id_it->Equals(loc_it.GetSeq_id())) {
                    match = true;
                    break;
                }
            }
            if (match) {
                ph = bsh;
            } else {
                ph = bsh.GetScope().GetBioseqHandle(*this_loc);
            }
            try {
                CGapCache gap_cache(*this_loc, ph);
                string vec_data;
                vec.GetSeqData(0, vec.size(), vec_data);

                local_first_gap = false;
                local_last_gap = false;
                TSeqLength len = loc_it.GetRange().GetLength();
                ENa_strand strand = loc_it.GetStrand();

                size_t pos = 0;
                string::iterator it = vec_data.begin();
                while (it != vec_data.end() && pos < len) {
                    bool is_gap = false;
                    bool unknown_length = false;
                    if (strand == eNa_strand_minus) {
                        if (gap_cache.IsKnownGap(len - pos - 1)) {
                            is_gap = true;
                        } else if (gap_cache.IsUnknownGap(len - pos - 1)) {
                            is_gap = true;
                            unknown_length = true;
                        }
                    } else {
                        if (gap_cache.IsKnownGap(pos)) {
                            is_gap = true;
                        } else if (gap_cache.IsUnknownGap(pos)) {
                            is_gap = true;
                            unknown_length = true;
                        }

                    }
                    if (is_gap) {
                        if (pos == 0) {
                            local_first_gap = true;
                        } else if (pos == len - 1) {
                            local_last_gap = true;
                        }
                        if (unknown_length) {
                            num_unknown_gap++;
                        } else {
                            num_gap++;
                        }
                    } else if (*it == 'N') {
                        num_n++;
                    } else {
                        num_real++;
                    }
                    ++it;
                    ++pos;
                }
            } catch (CException& ex) {
                /*
                PostErr(eDiag_Fatal, eErr_INTERNAL_Exception,
                string("Exception while checking for intervals in gaps. EXCEPTION: ") +
                ex.what(), feat);
                */
            }
        }
        if (first) {
            first_in_gap = local_first_gap;
            first = false;
        }
        last_in_gap = local_last_gap;
        if (local_first_gap || local_last_gap) {
            startsOrEndsInGap = true;
        }
    }

    if (num_real == 0 && num_n == 0) {
        TSeqPos start = loc.GetStart(eExtreme_Positional);
        TSeqPos stop = loc.GetStop(eExtreme_Positional);
        if ((start == 0 || CSeqMap_CI(bsh, SSeqMapSelector(), start - 1).GetType() != CSeqMap::eSeqGap)
            && (stop == bsh.GetBioseqLength() - 1 || CSeqMap_CI(bsh, SSeqMapSelector(), stop + 1).GetType() != CSeqMap::eSeqGap)) {
            rval |= eLocationGapFeatureMatchesGap;
        }
    }


    if (num_gap == 0 && num_unknown_gap == 0 && num_n == 0) {
        // ignore features that do not cover any gap characters
    } else if (first_in_gap || last_in_gap) {
        if (num_real > 0) {
            TSeqPos gap_start = x_FindStartOfGap(bsh,
                first_in_gap ? loc.GetStart(eExtreme_Biological)
                : loc.GetStop(eExtreme_Biological), &(bsh.GetScope()));
            gap_starts.push_back(gap_start);
        } else {
            rval |= eLocationGapContainedInGap;
        }
    } else if (num_real == 0 && num_gap == 0 && num_unknown_gap == 0 && num_n >= 50) {
        rval |= eLocationGapContainedInGapOfNs;
    } else if (startsOrEndsInGap) {
        rval |= eLocationGapInternalIntervalEndpointInGap;
    } else if (num_unknown_gap > 0) {
        rval |= eLocationGapCrossesUnknownGap;
    }

    if (num_n > num_real && xf_IsDeltaLitOnly(bsh)) {
        rval |= eLocationGapMostlyNs;
    }

    return rval;
}


size_t CSingleFeatValidator::x_FindStartOfGap(CBioseq_Handle bsh, int pos, CScope* scope)
{
    if (!bsh || !bsh.IsNa() || !bsh.IsSetInst_Repr()
        || bsh.GetInst_Repr() != CSeq_inst::eRepr_delta
        || !bsh.GetInst().IsSetExt()
        || !bsh.GetInst().GetExt().IsDelta()) {
        return bsh.GetInst_Length();
    }
    size_t offset = 0;

    ITERATE(CSeq_inst::TExt::TDelta::Tdata, it, bsh.GetInst_Ext().GetDelta().Get()) {
        unsigned int len = 0;
        if ((*it)->IsLiteral()) {
            len = (*it)->GetLiteral().GetLength();
        } else if ((*it)->IsLoc()) {
            len = sequence::GetLength((*it)->GetLoc(), scope);
        }
        if ((unsigned int)pos >= offset && (unsigned int)pos < offset + len) {
            return offset;
        } else {
            offset += len;
        }
    }
    return offset;
}


bool CSingleFeatValidator::x_IsMostlyNs(const CSeq_loc& loc, CBioseq_Handle bsh)
{
    if (!bsh.IsNa() || !bsh.IsSetInst_Repr() || bsh.GetInst_Repr() != CSeq_inst::eRepr_raw) {
        return false;
    }
    int num_n = 0;
    int real_bases = 0;

    for (CSeq_loc_CI loc_it(loc); loc_it; ++loc_it) {
        CConstRef<CSeq_loc> this_loc = loc_it.GetRangeAsSeq_loc();
        CSeqVector vec = GetSequenceFromLoc(*this_loc, bsh.GetScope());
        if (!vec.empty()) {
            CBioseq_Handle ph;
            bool match = false;
            for (auto id_it : bsh.GetBioseqCore()->GetId()) {
                if (id_it->Equals(loc_it.GetSeq_id())) {
                    match = true;
                    break;
                }
            }
            if (match) {
                ph = bsh;
            } else {
                ph = bsh.GetScope().GetBioseqHandle(*this_loc);
            }
            TSeqPos offset = this_loc->GetStart(eExtreme_Positional);
            string vec_data;
            try {
                vec.GetSeqData(0, vec.size(), vec_data);

                int pos = 0;
                string::iterator it = vec_data.begin();
                while (it != vec_data.end()) {
                    if (*it == 'N') {
                        CSeqMap_CI map_iter(ph, SSeqMapSelector(), offset + pos);
                        if (map_iter.GetType() == CSeqMap::eSeqGap) {
                        } else {
                            num_n++;
                        }
                    } else {
                        if ((unsigned)(*it + 1) <= 256 && isalpha(*it)) {
                            real_bases++;
                        }
                    }
                    ++it;
                    ++pos;
                }
            } catch (CException) {
            } catch (std::exception) {
            }
        }
    }

    return (num_n > real_bases);
}


CBioseq_Handle CSingleFeatValidator::x_GetFeatureProduct(bool look_far, bool& is_far)
{
    CBioseq_Handle prot_handle;
    is_far = false;
    if (!m_Feat.IsSetProduct()) {
        return prot_handle;
    }
    const CSeq_id* protid = NULL;
    try {
        protid = &sequence::GetId(m_Feat.GetProduct(), &m_Scope);
    } catch (CException&) {}

    if (!protid) {
        return prot_handle;
    }

    // try "local" scope
    prot_handle = m_Scope.GetBioseqHandleFromTSE(*protid, m_LocationBioseq.GetTSE_Handle());
    if (!prot_handle) {
        prot_handle = m_Scope.GetBioseqHandleFromTSE(*protid, m_Imp.GetTSE_Handle());
    }
    if (!prot_handle  &&  look_far) {
        prot_handle = m_Scope.GetBioseqHandle(*protid);
        if (prot_handle) {
            is_far = true;
        }
    }

    return prot_handle;
}


CBioseq_Handle CSingleFeatValidator::x_GetFeatureProduct(bool& is_far)
{
    bool look_far = false;

    if (m_Feat.IsSetData()) {
        if (m_Feat.GetData().IsCdregion()) {
            look_far = m_Imp.IsFarFetchCDSproducts();
        } else if (m_Feat.GetData().IsRna()) {
            look_far = m_Imp.IsFarFetchMRNAproducts();
        } else {
            look_far = m_Imp.IsRemoteFetch();
        }
    }

    return x_GetFeatureProduct(look_far, is_far);
}


void CSingleFeatValidator::x_ValidateExcept()
{
    if (m_Feat.IsSetExcept_text() && !NStr::IsBlank(m_Feat.GetExcept_text()) &&
        (!m_Feat.IsSetExcept() || !m_Feat.GetExcept())) {
        PostErr(eDiag_Warning, eErr_SEQ_FEAT_MissingExceptionFlag,
            "Exception text is present, but exception flag is not set");
    } else if (m_Feat.IsSetExcept() && m_Feat.GetExcept() && 
        (!m_Feat.IsSetExcept_text() || NStr::IsBlank(m_Feat.GetExcept_text()))) {
        PostErr(eDiag_Warning, eErr_SEQ_FEAT_ExceptionMissingText,
            "Exception flag is set, but exception text is empty");
    }
    if (m_Feat.IsSetExcept_text() && !m_Feat.GetExcept_text().empty()) {
        x_ValidateExceptText(m_Feat.GetExcept_text());
    }
}


void CSingleFeatValidator::x_ValidateExceptText(const string& text)
{
    if (text.empty()) return;

    EDiagSev sev = eDiag_Error;
    bool found = false;

    string str;

    bool reasons_in_cit = false;
    bool annotated_by_transcript_or_proteomic = false;
    bool redundant_with_comment = false;
    bool refseq_except = false;
    vector<string> exceptions;
    NStr::Split(text, ",", exceptions, 0);
    ITERATE(vector<string>, it, exceptions) {
        found = false;
        str = NStr::TruncateSpaces(*it);
        if (NStr::IsBlank(*it)) {
            continue;
        }
        found = CSeq_feat::IsExceptionTextInLegalList(str, false);

        if (found) {
            if (NStr::EqualNocase(str, "reasons given in citation")) {
                reasons_in_cit = true;
            } else if (NStr::EqualNocase(str, "annotated by transcript or proteomic data")) {
                annotated_by_transcript_or_proteomic = true;
            }
        }
        if (!found) {
            if (m_LocationBioseq) {
                bool check_refseq = false;
                if (m_Imp.IsRefSeqConventions()) {
                    check_refseq = true;
                } else if (GetGenProdSetParent(m_LocationBioseq)) {
                    check_refseq = true;
                } else {
                    FOR_EACH_SEQID_ON_BIOSEQ(id_it, *(m_LocationBioseq.GetCompleteBioseq())) {
                        if ((*id_it)->IsOther()) {
                            check_refseq = true;
                            break;
                        }
                    }
                }

                if (check_refseq) {
                    if (CSeq_feat::IsExceptionTextRefSeqOnly(str)) {
                        found = true;
                        refseq_except = true;
                    }
                }
            }
        }
        if (!found) {
            // lower to warning for genomic refseq
            const CSeq_id *id = m_Feat.GetLocation().GetId();
            if ((id != NULL && IsNTNCNWACAccession(*id)) || 
                (m_LocationBioseq && IsNTNCNWACAccession(*(m_LocationBioseq.GetCompleteBioseq())))) {
                sev = eDiag_Warning;
            }
            PostErr(sev, eErr_SEQ_FEAT_ExceptionProblem,
                str + " is not a legal exception explanation");
        }
        if (m_Feat.IsSetComment() && NStr::Find(m_Feat.GetComment(), str) != string::npos) {
            if (!NStr::EqualNocase(str, "ribosomal slippage") &&
                !NStr::EqualNocase(str, "trans-splicing") &&
                !NStr::EqualNocase(str, "RNA editing") &&
                !NStr::EqualNocase(str, "artificial location")) {
                redundant_with_comment = true;
            } else if (NStr::EqualNocase(m_Feat.GetComment(), str)) {
                redundant_with_comment = true;
            }
        }
    }
    if (redundant_with_comment) {
        PostErr(eDiag_Warning, eErr_SEQ_FEAT_ExceptionProblem,
            "Exception explanation text is also found in feature comment");
    }
    if (refseq_except) {
        bool found_just_the_exception = CSeq_feat::IsExceptionTextRefSeqOnly(str);

        if (!found_just_the_exception) {
            PostErr(eDiag_Warning, eErr_SEQ_FEAT_ExceptionProblem,
                "Genome processing exception should not be combined with other explanations");
        }
    }

    if (reasons_in_cit && !m_Feat.IsSetCit()) {
        PostErr(eDiag_Warning, eErr_SEQ_FEAT_ExceptionProblem,
            "Reasons given in citation exception does not have the required citation");
    }
    if (annotated_by_transcript_or_proteomic) {
        bool has_inference = false;
        FOR_EACH_GBQUAL_ON_SEQFEAT(it, m_Feat) {
            if ((*it)->IsSetQual() && NStr::EqualNocase((*it)->GetQual(), "inference")) {
                has_inference = true;
                break;
            }
        }
        if (!has_inference) {
            PostErr(eDiag_Warning, eErr_SEQ_FEAT_ExceptionProblem,
                "Annotated by transcript or proteomic data exception does not have the required inference qualifier");
        }
    }
}


const string kOrigProteinId = "orig_protein_id";

bool CSingleFeatValidator::x_ReportOrigProteinId()
{
    if (!m_Feat.GetData().IsRna()) {
        return true;
    } else {
        return false;
    }
}


void CSingleFeatValidator::x_ValidateGbquals()
{
    if (!m_Feat.IsSetQual()) {
        return;
    }
    string key;
    bool is_imp = false;
    CSeqFeatData::ESubtype ftype = m_Feat.GetData().GetSubtype();

    if (m_Feat.IsSetData() && m_Feat.GetData().IsImp()) {
        is_imp = true;
        key = m_Feat.GetData().GetImp().GetKey();
        if (ftype == CSeqFeatData::eSubtype_imp && NStr::EqualNocase (key, "gene")) {
            ftype = CSeqFeatData::eSubtype_gene;
        } else if (ftype == CSeqFeatData::eSubtype_imp) { 
            ftype = CSeqFeatData::eSubtype_bad;
        } else if (ftype == CSeqFeatData::eSubtype_Imp_CDS 
                   || ftype == CSeqFeatData::eSubtype_site_ref
                   || ftype == CSeqFeatData::eSubtype_org) {
            ftype = CSeqFeatData::eSubtype_bad;
        }
    }
    else {
        key = m_Feat.GetData().GetKey();
        if (NStr::Equal (key, "Gene")) {
            key = "gene";
        }
    }

    for (auto gbq : m_Feat.GetQual()) {
        const string& qual_str = gbq->GetQual();

        if ( NStr::Equal (qual_str, "gsdb_id")) {
            continue;
        }
        auto gbqual_and_value = CSeqFeatData::GetQualifierTypeAndValue(qual_str);
        auto gbqual = gbqual_and_value.first;
        bool same_case = (gbqual == CSeqFeatData::eQual_bad) || NStr::EqualCase(gbqual_and_value.second, qual_str);

        if ( !same_case ) {
            PostErr(eDiag_Warning, eErr_SEQ_FEAT_IncorrectQualifierCapitalization,
                qual_str + " is improperly capitalized");
        }

        if ( gbqual == CSeqFeatData::eQual_bad ) {
            if (is_imp) {
                if (!gbq->IsSetQual() || NStr::IsBlank(gbq->GetQual())) {
                    PostErr(eDiag_Warning, eErr_SEQ_FEAT_UnknownImpFeatQual,
                        "NULL qualifier");
                }
                else {
                    PostErr(eDiag_Warning, eErr_SEQ_FEAT_UnknownImpFeatQual,
                        "Unknown qualifier " + qual_str);
                }
            } else if (NStr::Equal(qual_str, kOrigProteinId)) {
                if (x_ReportOrigProteinId()) {
                    PostErr(eDiag_Warning, eErr_SEQ_FEAT_UnknownFeatureQual, "Unknown qualifier " + kOrigProteinId);
                }
            } else {
                CSeqFeatData::E_Choice chs = m_Feat.GetData().Which();
                if (chs == CSeqFeatData::e_Gene) {
                    if (NStr::Equal(qual_str, "gen_map")
                        || NStr::Equal(qual_str, "cyt_map")
                        || NStr::Equal(qual_str, "rad_map")) {
                        continue;
                    }
                } else if (chs == CSeqFeatData::e_Cdregion) {
                    if (NStr::Equal(qual_str, "orig_transcript_id")) {
                        continue;
                    }
                } else if (chs == CSeqFeatData::e_Rna) {
                    if (NStr::Equal(qual_str, "orig_transcript_id")) {
                        continue;
                    }
                }
                PostErr(eDiag_Warning, eErr_SEQ_FEAT_UnknownFeatureQual, "Unknown qualifier " + qual_str);
            }
        } else {
            if ( ftype != CSeqFeatData::eSubtype_bad && !CSeqFeatData::IsLegalQualifier(ftype, gbqual) ) {
                PostErr(eDiag_Warning, 
                    is_imp ? eErr_SEQ_FEAT_WrongQualOnImpFeat : eErr_SEQ_FEAT_WrongQualOnFeature,
                    "Wrong qualifier " + qual_str + " for feature " + 
                    key);
            }

            if (gbq->IsSetVal() && !NStr::IsBlank(gbq->GetVal())) {
                // validate value of gbqual
                const string& val = gbq->GetVal();
                switch (gbqual) {

                    case CSeqFeatData::eQual_rpt_type:
                        if (!CGb_qual::IsValidRptTypeValue(val)) {
                            PostErr(eDiag_Error, eErr_SEQ_FEAT_InvalidQualifierValue,
                                val + " is not a legal value for qualifier " + qual_str);
                        }          
                        break;
                
                    case CSeqFeatData::eQual_rpt_unit:                
                        x_ValidateRptUnitVal (val, key);
                        break;

                    case CSeqFeatData::eQual_rpt_unit_seq:
                        x_ValidateRptUnitSeqVal (val, key);
                        break;

                    case CSeqFeatData::eQual_rpt_unit_range:
                        x_ValidateRptUnitRangeVal (val);
                        break;

                    case CSeqFeatData::eQual_label:
                        x_ValidateLabelVal (val);
                        break;
            
                    case CSeqFeatData::eQual_replace:
                        if (is_imp) {
                            x_ValidateReplaceQual(key, qual_str, val);
                        }
                        break;

                    case CSeqFeatData::eQual_mobile_element:
                    case CSeqFeatData::eQual_mobile_element_type:
                        if (is_imp && !CGb_qual::IsLegalMobileElementValue(val)) {
                            PostErr(eDiag_Warning, eErr_SEQ_FEAT_MobileElementInvalidQualifier,
                                  val + " is not a legal value for qualifier " + qual_str);
                        }
                        break;

                    case CSeqFeatData::eQual_compare:
                        x_ValidateCompareVal (val);
                        break;

                    case CSeqFeatData::eQual_standard_name:
                        if (is_imp && ftype == CSeqFeatData::eSubtype_misc_feature
                            && NStr::EqualCase (val, "Vector Contamination")) {
                            PostErr (eDiag_Warning, eErr_SEQ_FEAT_VectorContamination, 
                                     "Vector Contamination region should be trimmed from sequence");
                        }                
                        break;

                    case CSeqFeatData::eQual_product:
                        if (!is_imp) {
                            CSeqFeatData::E_Choice chs = m_Feat.GetData().Which();
                            if (chs == CSeqFeatData::e_Gene) {
                                PostErr(eDiag_Info, eErr_SEQ_FEAT_InvalidProductOnGene,
                                    "A product qualifier is not used on a gene feature");
                            }
                        }
                        break;

                    // for VR-825
                    case CSeqFeatData::eQual_locus_tag:
                        PostErr(eDiag_Warning, eErr_SEQ_FEAT_WrongQualOnFeature, 
                            "locus-tag values should be on genes");
                        break;
                    default:
                        break;
                } // end of switch statement
            }
        }
    }
}


void CSingleFeatValidator::x_ValidateRptUnitVal (const string& val, const string& key)
{
    bool /* found = false, */ multiple_rpt_unit = false;
    ITERATE(string, it, val) {
        if ( *it <= ' ' ) {
            /* found = true; */
        } else if ( *it == '('  ||  *it == ')'  ||
            *it == ','  ||  *it == '.'  ||
            isdigit((unsigned char)(*it)) ) {
            multiple_rpt_unit = true;
        }
    }
    /*
    if ( found || 
    (!multiple_rpt_unit && val.length() > 48) ) {
    error = true;
    }
    */
    if ( NStr::CompareNocase(key, "repeat_region") == 0  &&
         !multiple_rpt_unit ) {
        if (val.length() <= GetLength(m_Feat.GetLocation(), &m_Scope) ) {
            bool just_nuc_letters = true;
            static const string nuc_letters = "ACGTNacgtn";
            
            ITERATE(string, it, val) {
                if ( nuc_letters.find(*it) == NPOS ) {
                    just_nuc_letters = false;
                    break;
                }
            }
            
            if ( just_nuc_letters ) {
                CSeqVector vec = GetSequenceFromFeature(m_Feat, m_Scope);
                if ( !vec.empty() ) {
                    string vec_data;
                    vec.GetSeqData(0, vec.size(), vec_data);
                    if (NStr::FindNoCase (vec_data, val) == string::npos) {
                        PostErr(eDiag_Warning, eErr_SEQ_FEAT_RepeatSeqDoNotMatch,
                            "repeat_region /rpt_unit and underlying "
                            "sequence do not match");
                    }
                }
            }
        } else {
            PostErr(eDiag_Info, eErr_SEQ_FEAT_InvalidRepeatUnitLength,
                "Length of rpt_unit_seq is greater than feature length");
        }                            
    }
}


void CSingleFeatValidator::x_ValidateRptUnitSeqVal (const string& val, const string& key)
{
    // do validation common to rpt_unit
    x_ValidateRptUnitVal (val, key);

    // do the validation specific to rpt_unit_seq
    const char *cp = val.c_str();
    bool badchars = false;
    while (*cp != 0 && !badchars) {
        if (*cp < ' ') {
            badchars = true;
        } else if (*cp != '(' && *cp != ')' 
                   && !isdigit (*cp) && !isalpha (*cp) 
                   && *cp != ',' && *cp != ';') {
            badchars = true;
        }
        cp++;
    }
    if (badchars) {
        PostErr(eDiag_Warning, eErr_SEQ_FEAT_InvalidRptUnitSeqCharacters,
            "/rpt_unit_seq has illegal characters");
    }
}


static bool s_RptUnitIsBaseRange (string str, TSeqPos& from, TSeqPos& to)

{
    if (str.length() > 25) {
        return false;
    }
    SIZE_TYPE pos = NStr::Find (str, "..");
    if (pos == string::npos) {
        return false;
    }

    int tmp_from, tmp_to;
    try {
        tmp_from = NStr::StringToInt (str.substr(0, pos));
        from = tmp_from;
        tmp_to = NStr::StringToInt (str.substr (pos + 2));
        to = tmp_to;
      } catch (CException ) {
          return false;
      } catch (std::exception ) {
        return false;
    }
    if (tmp_from < 0 || tmp_to < 0) {
        return false;
    }
    return true;
}


void CSingleFeatValidator::x_ValidateRptUnitRangeVal (const string& val)
{
    TSeqPos from = kInvalidSeqPos, to = kInvalidSeqPos;
    if (!s_RptUnitIsBaseRange(val, from, to)) {
        PostErr(eDiag_Warning, eErr_SEQ_FEAT_InvalidRptUnitRange,
                 "/rpt_unit_range is not a base range");
    } else {
        CSeq_loc::TRange range = m_Feat.GetLocation().GetTotalRange();
        if (from - 1 < range.GetFrom() || from - 1> range.GetTo() || to - 1 < range.GetFrom() || to - 1 > range.GetTo()) {
            PostErr (eDiag_Warning, eErr_SEQ_FEAT_RptUnitRangeProblem,
                     "/rpt_unit_range is not within sequence length");   
        } else {
            bool nulls_between = false;
            for ( CTypeConstIterator<CSeq_loc> lit = ConstBegin(m_Feat.GetLocation()); lit; ++lit ) {
                if ( lit->Which() == CSeq_loc::e_Null ) {
                    nulls_between = true;
                }
            }
            if (nulls_between) {
                bool in_range = false;
                for ( CSeq_loc_CI it(m_Feat.GetLocation()); it; ++it ) {
                    range = it.GetEmbeddingSeq_loc().GetTotalRange();
                    if (from - 1 < range.GetFrom() || from - 1> range.GetTo() || to - 1 < range.GetFrom() || to - 1 > range.GetTo()) {
                    } else {
                        in_range = true;
                    }
                }
                if (! in_range) {
                    PostErr (eDiag_Warning, eErr_SEQ_FEAT_RptUnitRangeProblem,
                             "/rpt_unit_range is not within ordered intervals");   
                }
            }
        }
    }
}


void CSingleFeatValidator::x_ValidateLabelVal (const string& val)
{
    bool only_digits = true,
        has_spaces = false;
    
    ITERATE(string, it, val) {
        if ( isspace((unsigned char)(*it)) ) {
            has_spaces = true;
        }
        if ( !isdigit((unsigned char)(*it)) ) {
            only_digits = false;
        }
    }
    if (only_digits  ||  has_spaces) {
        PostErr (eDiag_Error, eErr_SEQ_FEAT_InvalidQualifierValue, "Illegal value for qualifier label");
    }
}


void CSingleFeatValidator::x_ValidateCompareVal (const string& val)
{
    if (!NStr::StartsWith (val, "(")) {
        EAccessionFormatError valid_accession = ValidateAccessionString (val, true);  
        if (valid_accession == eAccessionFormat_missing_version) {
            PostErr(eDiag_Error, eErr_SEQ_FEAT_InvalidCompareMissingVersion,
                     val + " accession missing version for qualifier compare");
        } else if (valid_accession == eAccessionFormat_bad_version) {
            PostErr (eDiag_Error, eErr_SEQ_FEAT_InvalidQualifierValue,
                     val + " accession has bad version for qualifier compare");
        } else if (valid_accession != eAccessionFormat_valid) {
            PostErr(eDiag_Error, eErr_SEQ_FEAT_InvalidCompareBadAccession,
                     val + " is not a legal accession for qualifier compare");
        } else if (m_Imp.IsINSDInSep() && NStr::Find (val, "_") != string::npos) {
            PostErr(eDiag_Error, eErr_SEQ_FEAT_InvalidCompareRefSeqAccession,
                     "RefSeq accession " + val + " cannot be used for qualifier compare");
        }
    }
}


static bool s_StringConsistsOf (string str, string consist)
{
    const char *src = str.c_str();
    const char *find = consist.c_str();
    bool rval = true;

    while (*src != 0 && rval) {
        if (strchr (find, *src) == NULL) {
            rval = false;
        }
        src++;
    }
    return rval;
}


void CSingleFeatValidator::x_ValidateReplaceQual(const string& key, const string& qual_str, const string& val)
{
    if (m_LocationBioseq) {
        if (m_LocationBioseq.IsNa()) {
            if (NStr::Equal(key, "variation")) {
                if (!s_StringConsistsOf (val, "acgtACGT")) {
                    PostErr(eDiag_Error, eErr_SEQ_FEAT_InvalidVariationReplace,
                                val + " is not a legal value for qualifier " + qual_str
                                + " - should only be composed of acgt unambiguous nucleotide bases");
                }
            } else if (!s_StringConsistsOf (val, "acgtmrwsykvhdbn")) {
                    PostErr (eDiag_Error, eErr_SEQ_FEAT_InvalidReplace,
                            val + " is not a legal value for qualifier " + qual_str
                            + " - should only be composed of acgtmrwsykvhdbn nucleotide bases");
            }
        } else if (m_LocationBioseq.IsAa()) {
            if (!s_StringConsistsOf (val, "acdefghiklmnpqrstuvwy*")) {
                PostErr(eDiag_Error, eErr_SEQ_FEAT_InvalidReplace,
                            val + " is not a legal value for qualifier " + qual_str
                            + " - should only be composed of acdefghiklmnpqrstuvwy* amino acids");
            }
        }

        // if no point in location with fuzz, info if text matches sequence
        bool has_fuzz = false;
        for( objects::CSeq_loc_CI it(m_Feat.GetLocation()); it && !has_fuzz; ++it) {
            if (it.IsPoint() && (it.GetFuzzFrom() != NULL || it.GetFuzzTo() != NULL)) {
                has_fuzz = true;
            }
        }
        if (!has_fuzz && val.length() == GetLength (m_Feat.GetLocation(), &m_Scope)) {
            try {
                CSeqVector nuc_vec(m_Feat.GetLocation(), m_Scope, CBioseq_Handle::eCoding_Iupac);
                string bases = "";
                nuc_vec.GetSeqData(0, nuc_vec.size(), bases);
                if (NStr::EqualNocase(val, bases)) {
                    PostErr(eDiag_Info, eErr_SEQ_FEAT_InvalidMatchingReplace,
                                "/replace already matches underlying sequence (" + val + ")");
                }
            } catch (CException ) {
            } catch (std::exception ) {
            }
        }
    }
}


void CSingleFeatValidator::ValidateCharactersInField (string value, string field_name)
{
    if (HasBadCharacter (value)) {
        PostErr (eDiag_Warning, eErr_SEQ_FEAT_BadInternalCharacter, 
                 field_name + " contains undesired character");
    }
    if (EndsWithBadCharacter (value)) {
        PostErr (eDiag_Warning, eErr_SEQ_FEAT_BadTrailingCharacter, 
                 field_name + " ends with undesired character");
    }
    if (NStr::EndsWith (value, "-")) {
        if (!m_Imp.IsGpipe() || !m_Imp.BioSourceKind().IsOrganismEukaryote() )
            PostErr (eDiag_Warning, eErr_SEQ_FEAT_BadTrailingHyphen, 
                field_name + " ends with hyphen");
    }
}


void CSingleFeatValidator::ValidateSplice(bool gene_pseudo, bool check_all)
{
    if (!m_LocationBioseq) {
        return;
    }

    CSpliceProblems splice_problems;
    splice_problems.CalculateSpliceProblems(m_Feat, check_all, gene_pseudo, m_LocationBioseq);

    if (splice_problems.AreErrorsUnexpected()) {
        string label = GetBioseqIdLabel(*(m_LocationBioseq.GetCompleteBioseq()), true);
        x_ReportSpliceProblems(splice_problems, label);
    }

    if (splice_problems.IsExceptionUnnecessary()) {
        PostErr(eDiag_Warning, eErr_SEQ_FEAT_UnnecessaryException,
            "feature has exception but passes splice site test");
    }
}


EDiagSev CSingleFeatValidator::x_SeverityForConsensusSplice(void)
{
    EDiagSev sev = eDiag_Warning;
    if (m_Imp.IsGpipe() && m_Imp.IsGenomic()) {
        sev = eDiag_Info;
    } else if ((m_Imp.IsGPS() || m_Imp.IsRefSeq()) && !m_Imp.ReportSpliceAsError()) {
        sev = eDiag_Warning;
    }
    return sev;
}


void CSingleFeatValidator::x_ReportDonorSpliceSiteReadErrors(const CSpliceProblems::TSpliceProblem& problem, const string& label)
{
    if (problem.first == CSpliceProblems::eSpliceSiteRead_BadSeq) {
        PostErr(eDiag_Warning, eErr_SEQ_FEAT_NotSpliceConsensusDonor,
            "Bad sequence at splice donor after exon ending at position "
            + NStr::IntToString(problem.second + 1) + " of " + label);
    } else if (problem.first == CSpliceProblems::eSpliceSiteRead_WrongNT) {
        PostErr(x_SeverityForConsensusSplice(), eErr_SEQ_FEAT_NotSpliceConsensusDonor,
            "Splice donor consensus (GT) not found after exon ending at position "
            + NStr::IntToString(problem.second + 1) + " of " + label);
    }

}


void CSingleFeatValidator::x_ReportAcceptorSpliceSiteReadErrors(const CSpliceProblems::TSpliceProblem& problem, const string& label)
{
    if (problem.first == CSpliceProblems::eSpliceSiteRead_BadSeq) {
        PostErr(x_SeverityForConsensusSplice(), eErr_SEQ_FEAT_NotSpliceConsensusAcceptor,
            "Bad sequence at splice acceptor before exon starting at position "
            + NStr::IntToString(problem.second + 1) + " of " + label);
    } else if (problem.first == CSpliceProblems::eSpliceSiteRead_WrongNT) {
        PostErr(x_SeverityForConsensusSplice(), eErr_SEQ_FEAT_NotSpliceConsensusAcceptor,
            "Splice acceptor consensus (AG) not found before exon starting at position "
            + NStr::IntToString(problem.second + 1) + " of " + label);
    }

}


void CSingleFeatValidator::x_ReportSpliceProblems
(const CSpliceProblems& problems, const string& label)
{
    const CSpliceProblems::TSpliceProblemList& donor_problems = problems.GetDonorProblems();
    for (auto it = donor_problems.begin(); it != donor_problems.end(); it++) {
        x_ReportDonorSpliceSiteReadErrors(*it, label);
    }
    const CSpliceProblems::TSpliceProblemList& acceptor_problems = problems.GetAcceptorProblems();    
    for (auto it = acceptor_problems.begin(); it != acceptor_problems.end(); it++) {
        x_ReportAcceptorSpliceSiteReadErrors(*it, label);
    }
}


bool CSingleFeatValidator::x_BioseqHasNmAccession (CBioseq_Handle bsh)
{
    if (bsh) {
        FOR_EACH_SEQID_ON_BIOSEQ (it, *(bsh.GetBioseqCore())) {
            if ((*it)->IsOther() && (*it)->GetOther().IsSetAccession() 
                && NStr::StartsWith ((*it)->GetOther().GetAccession(), "NM_")) {
                return true;
            }
        }
    }
    return false;
}


void CSingleFeatValidator::x_ValidateNonImpFeat ()
{
    if (m_Feat.GetData().IsImp()) {
        return;
    }
    string key = m_Feat.GetData().GetKey();

    CSeqFeatData::ESubtype subtype = m_Feat.GetData().GetSubtype();

    // look for mandatory qualifiers
    EDiagSev sev = eDiag_Warning;

    for (auto required : CSeqFeatData::GetMandatoryQualifiers(subtype))
    {
        bool found = false;
        if (m_Feat.IsSetQual()) {
            for (auto qual : m_Feat.GetQual()) {
                if (qual->IsSetQual() && CSeqFeatData::GetQualifierType(qual->GetQual()) == required) {
                    found = true;
                    break;
                }
            }
        }

        if (!found) {
            if (required == CSeqFeatData::eQual_citation) {
                if (m_Feat.IsSetCit()) {
                    found = true;
                } else if (m_Feat.IsSetComment() && NStr::EqualNocase (key, "conflict")) {
                    // RefSeq allows conflict with accession in comment instead of sfp->cit
                    FOR_EACH_SEQID_ON_BIOSEQ (it, *(m_LocationBioseq.GetCompleteBioseq())) {
                        if ((*it)->IsOther()) {
                            found = true;
                            break;
                        }
                    }
                }
            }
        }
        if (!found && (NStr::EqualNocase (key, "conflict") || NStr::EqualNocase (key, "old_sequence"))) {
            if (m_Feat.IsSetQual()) {
                for (auto qual : m_Feat.GetQual()) {
                    if (qual->IsSetQual() && NStr::EqualNocase(qual->GetQual(), "compare")
                        && qual->IsSetVal() && !NStr::IsBlank(qual->GetVal())) {
                        found = true;
                        break;
                    }
                }
            }
        }
        if (!found && required == CSeqFeatData::eQual_ncRNA_class) {
            sev = eDiag_Error;
            if (m_Feat.GetData().IsRna() && m_Feat.GetData().GetRna().IsSetExt()
                && m_Feat.GetData().GetRna().GetExt().IsGen()
                && m_Feat.GetData().GetRna().GetExt().GetGen().IsSetClass()
                && !NStr::IsBlank(m_Feat.GetData().GetRna().GetExt().GetGen().GetClass())) {
                found = true;
            }
        }

        if (!found) {
            PostErr(sev, eErr_SEQ_FEAT_MissingQualOnFeature,
                "Missing qualifier " + CSeqFeatData::GetQualifierAsString(required) +
                " for feature " + key);
        }
    }
}


static bool s_LocationStrandsIncompatible (const CSeq_loc& loc1, const CSeq_loc& loc2, CScope * scope)
{
    ENa_strand strand1 = loc1.GetStrand();
    ENa_strand strand2 = loc2.GetStrand();

    if (strand1 == strand2) {
        return false;
    }
    if ((strand1 == eNa_strand_unknown || strand1 == eNa_strand_plus) &&
        (strand2 == eNa_strand_unknown || strand2 == eNa_strand_plus)) {
            return false;
    }
    if (strand1 == eNa_strand_other) {
        ECompare comp = Compare(loc1, loc2, scope, fCompareOverlapping);
        if (comp == eContains) {
            return false;
        }
    } else if (strand2 == eNa_strand_other) {
        ECompare comp = Compare(loc1, loc2, scope, fCompareOverlapping);
        if (comp == eContained) {
            return false;
        }
    }

    return true;
}


void CSingleFeatValidator::x_ValidateGeneFeaturePair(const CSeq_feat& gene)
{
    bool bad_strand = s_LocationStrandsIncompatible(gene.GetLocation(), m_Feat.GetLocation(), &m_Scope);
    if (bad_strand) {
        PostErr(eDiag_Warning, eErr_SEQ_FEAT_GeneXrefStrandProblem,
                "Gene cross-reference is not on expected strand");
    }

}


bool CSingleFeatValidator::s_GeneRefsAreEquivalent (const CGene_ref& g1, const CGene_ref& g2, string& label)
{
    bool equivalent = false;
    if (g1.IsSetLocus_tag()
        && g2.IsSetLocus_tag()) {
        if (NStr::EqualNocase(g1.GetLocus_tag(),
                              g2.GetLocus_tag())) {
            label = g1.GetLocus_tag();
            equivalent = true;
        }
    } else if (g1.IsSetLocus()
               && g2.IsSetLocus()) {
        if (NStr::EqualNocase(g1.GetLocus(),
                              g2.GetLocus())) {
            label = g1.GetLocus();
            equivalent = true;
        }
    } else if (g1.IsSetSyn()
               && g2.IsSetSyn()) {
        if (NStr::EqualNocase (g1.GetSyn().front(),
                               g2.GetSyn().front())) {
            label = g1.GetSyn().front();
            equivalent = true;
        }
    }                    
    return equivalent;
}


// Check for redundant gene Xref
// Do not call if feat is gene
void CSingleFeatValidator::x_ValidateGeneXRef()
{
    if (m_Feat.IsSetData() && m_Feat.GetData().IsGene()) {
        return;
    }
    auto tse = m_Imp.GetTSE_Handle();
    if (!tse) {
        return;
    }

    // first, look for gene by feature id xref
    bool has_gene_id_xref = false;
    if (m_Feat.IsSetXref()) {
        ITERATE(CSeq_feat::TXref, xref, m_Feat.GetXref()) {
            if ((*xref)->IsSetId() && (*xref)->GetId().IsLocal()) {
                CTSE_Handle::TSeq_feat_Handles gene_feats =
                    tse.GetFeaturesWithId(CSeqFeatData::eSubtype_gene, (*xref)->GetId().GetLocal());
                if (gene_feats.size() > 0) {
                    has_gene_id_xref = true;
                    ITERATE(CTSE_Handle::TSeq_feat_Handles, gene, gene_feats) {
                        x_ValidateGeneFeaturePair(*(gene->GetSeq_feat()));
                    }
                }
            }
        }
    }
    if (has_gene_id_xref) {
        return;
    }

    // if we can't get the bioseq on which the gene is located, we can't check for 
    // overlapping/ambiguous/redundant conditions
    if (!m_LocationBioseq) {
        return;
    }

    const CGene_ref* gene_xref = m_Feat.GetGeneXref();

    size_t num_genes = 0;
    size_t max = 0;
    size_t num_trans_spliced = 0;
    bool equivalent = false;
    /*
    CFeat_CI gene_it(bsh, CSeqFeatData::e_Gene);
    */

    //CFeat_CI gene_it(*m_Scope, feat.GetLocation(), SAnnotSelector (CSeqFeatData::e_Gene));
    CFeat_CI gene_it(m_LocationBioseq, 
                     CRange<TSeqPos>(m_Feat.GetLocation().GetStart(eExtreme_Positional),
                                     m_Feat.GetLocation().GetStop(eExtreme_Positional)),
                     SAnnotSelector(CSeqFeatData::e_Gene));
    CFeat_CI prev_gene;
    string label = "?";
    size_t num_match_by_locus = 0;
    size_t num_match_by_locus_tag = 0;

    for ( ; gene_it; ++gene_it) {
        if (gene_xref && gene_xref->IsSetLocus() &&
            gene_it->GetData().GetGene().IsSetLocus() &&
            NStr::Equal(gene_xref->GetLocus(), gene_it->GetData().GetGene().GetLocus())) {
            num_match_by_locus++;
            x_ValidateGeneFeaturePair(*(gene_it->GetSeq_feat()));
        }
        if (gene_xref && gene_xref->IsSetLocus_tag() &&
            gene_it->GetData().GetGene().IsSetLocus_tag() &&
            NStr::Equal(gene_xref->GetLocus_tag(), gene_it->GetData().GetGene().GetLocus_tag())) {
            num_match_by_locus_tag++;
            x_ValidateGeneFeaturePair(*(gene_it->GetSeq_feat()));
            if ((!gene_xref->IsSetLocus() || NStr::IsBlank(gene_xref->GetLocus())) &&
                gene_it->GetData().GetGene().IsSetLocus() &&
                !NStr::IsBlank(gene_it->GetData().GetGene().GetLocus())) {
                PostErr(eDiag_Warning, eErr_SEQ_FEAT_GeneXrefWithoutLocus,
                    "Feature has Gene Xref with locus_tag but no locus, gene with locus_tag and locus exists");
            }
        }

        if (TestForOverlapEx (gene_it->GetLocation(), m_Feat.GetLocation(), 
          gene_it->GetLocation().IsInt() ? eOverlap_Contained : eOverlap_Subset, &m_Scope) >= 0) {
            size_t len = GetLength(gene_it->GetLocation(), &m_Scope);
            if (len < max || num_genes == 0) {
                num_genes = 1;
                max = len;
                num_trans_spliced = 0;
                if (gene_it->IsSetExcept() && gene_it->IsSetExcept_text() &&
                    NStr::FindNoCase (gene_it->GetExcept_text(), "trans-splicing") != string::npos) {
                    num_trans_spliced++;
                }
                equivalent = false;
                prev_gene = gene_it;
            } else if (len == max) {
                equivalent |= s_GeneRefsAreEquivalent(gene_it->GetData().GetGene(), prev_gene->GetData().GetGene(), label);
                num_genes++;
                if (gene_it->IsSetExcept() && gene_it->IsSetExcept_text() &&
                    NStr::FindNoCase (gene_it->GetExcept_text(), "trans-splicing") != string::npos) {
                    num_trans_spliced++;
                }
            }
        }
    }

    if ( gene_xref == 0) {
        // if there is no gene xref, then there should be 0 or 1 overlapping genes
        // so that mapping by overlap is unambiguous
        if (num_genes > 1 &&
            m_Feat.GetData().GetSubtype() != CSeqFeatData::eSubtype_repeat_region &&
            m_Feat.GetData().GetSubtype() != CSeqFeatData::eSubtype_mobile_element) {
            if (m_Imp.IsSmallGenomeSet() && num_genes == num_trans_spliced) {
                /* suppress for trans-spliced genes on small genome set */
            } else if (equivalent) {
                PostErr (eDiag_Warning, eErr_SEQ_FEAT_GeneXrefNeeded,
                         "Feature overlapped by "
                         + NStr::SizetToString(num_genes)
                         + " identical-length equivalent genes but has no cross-reference");
            } else {
                PostErr (eDiag_Warning, eErr_SEQ_FEAT_MissingGeneXref,
                         "Feature overlapped by "
                         + NStr::SizetToString(num_genes)
                         + " identical-length genes but has no cross-reference");
            }
        } else if (num_genes == 1 
                   && prev_gene->GetData().GetGene().IsSetAllele()
                   && !NStr::IsBlank(prev_gene->GetData().GetGene().GetAllele())) {
            const string& allele = prev_gene->GetData().GetGene().GetAllele();
            // overlapping gene should not conflict with allele qualifier
            FOR_EACH_GBQUAL_ON_FEATURE (qual_iter, m_Feat) {
                const CGb_qual& qual = **qual_iter;
                if ( qual.IsSetQual()  &&
                     NStr::Compare(qual.GetQual(), "allele") == 0 ) {
                    if ( qual.CanGetVal()  &&
                         NStr::CompareNocase(qual.GetVal(), allele) == 0 ) {
                        PostErr(eDiag_Warning, eErr_SEQ_FEAT_InvalidAlleleDuplicates,
                            "Redundant allele qualifier (" + allele + 
                            ") on gene and feature");
                    } else if (m_Feat.GetData().GetSubtype() != CSeqFeatData::eSubtype_variation) {
                        PostErr(eDiag_Warning, eErr_SEQ_FEAT_MismatchedAllele,
                            "Mismatched allele qualifier on gene (" + allele + 
                            ") and feature (" + qual.GetVal() +")");
                    }
                }
            }
        }
    } else if ( !gene_xref->IsSuppressed() ) {
        // we are counting features with gene xrefs
        m_Imp.IncrementGeneXrefCount();

        // make sure overlapping gene and gene xref do not conflict
        if (gene_xref->IsSetAllele() && !NStr::IsBlank(gene_xref->GetAllele())) {
            const string& allele = gene_xref->GetAllele();

            FOR_EACH_GBQUAL_ON_FEATURE (qual_iter, m_Feat) {
                const CGb_qual& qual = **qual_iter;
                if ( qual.CanGetQual()  &&
                     NStr::Compare(qual.GetQual(), "allele") == 0 ) {
                    if ( qual.CanGetVal()  &&
                         NStr::CompareNocase(qual.GetVal(), allele) == 0 ) {
                        PostErr(eDiag_Warning, eErr_SEQ_FEAT_InvalidAlleleDuplicates,
                            "Redundant allele qualifier (" + allele + 
                            ") on gene and feature");
                    } else if (m_Feat.GetData().GetSubtype() != CSeqFeatData::eSubtype_variation) {
                        PostErr(eDiag_Warning, eErr_SEQ_FEAT_MismatchedAllele,
                            "Mismatched allele qualifier on gene (" + allele + 
                            ") and feature (" + qual.GetVal() +")");
                    }
                }
            }
        }

        if (num_match_by_locus == 0 && num_match_by_locus_tag == 0) {
            // find gene on bioseq to match genexref    
            if ((gene_xref->IsSetLocus_tag() &&
                !NStr::IsBlank(gene_xref->GetLocus_tag())) ||
                (gene_xref->IsSetLocus() &&
                    !NStr::IsBlank(gene_xref->GetLocus()))) {
                CConstRef<CSeq_feat> gene = m_Imp.GetGeneCache().GetGeneFromCache(&m_Feat, m_Scope);
                if (!gene && m_LocationBioseq && m_LocationBioseq.IsAa()) {
                    const CSeq_feat* cds = GetCDSForProduct(m_LocationBioseq);
                    if (cds != 0) {
                        if (cds->IsSetLocation()) {
                            const CSeq_loc& loc = cds->GetLocation();
                            const CSeq_id* id = loc.GetId();
                            if (id != NULL) {
                                CBioseq_Handle nbsh = m_LocationBioseq.GetScope().GetBioseqHandle(*id);
                                if (nbsh) {
                                    gene = m_Imp.GetGeneCache().GetGeneFromCache(cds, m_Scope);
                                }
                            }
                        }
                    }
                }
                string label;
                if (gene && !CSingleFeatValidator::s_GeneRefsAreEquivalent(*gene_xref, gene->GetData().GetGene(), label)) {
                    gene.Reset(NULL);
                }
                if (gene_xref->IsSetLocus_tag() &&
                    !NStr::IsBlank(gene_xref->GetLocus_tag()) &&
                    !gene) {
                    PostErr(eDiag_Warning, eErr_SEQ_FEAT_GeneXrefWithoutGene,
                        "Feature has gene locus_tag cross-reference but no equivalent gene feature exists");
                } else if (gene_xref->IsSetLocus() &&
                    !NStr::IsBlank(gene_xref->GetLocus()) &&
                    !gene) {
                    PostErr(eDiag_Warning, eErr_SEQ_FEAT_GeneXrefWithoutGene,
                        "Feature has gene locus cross-reference but no equivalent gene feature exists");
                }
            }
        }
    }

}


void CSingleFeatValidator::x_ValidateNonGene()
{
    if (m_Feat.GetData().IsGene()) {
        return;
    }
    x_ValidateGeneXRef();

    if (m_Feat.IsSetQual()) {
        // check old locus tag on feature and overlapping gene
        for (auto it : m_Feat.GetQual()) {
            if (it->IsSetQual() && NStr::Equal(it->GetQual(), "old_locus_tag")
                && it->IsSetVal() && !NStr::IsBlank(it->GetVal())) {
                x_ValidateOldLocusTag(it->GetVal());
            }
        }
    }
}


bool CSingleFeatValidator::s_IsPseudo(const CGene_ref& ref)
{
    if (ref.IsSetPseudo() && ref.GetPseudo()) {
        return true;
    } else {
        return false;
    }
}


bool s_HasNamedQual(const CSeq_feat& feat, const string& qual)
{
    bool rval = false;
    if (feat.IsSetQual()) {
        for (auto it : feat.GetQual()) {
            if (it->IsSetQual() && NStr::EqualNocase(it->GetQual(), qual)) {
                rval = true;
                break;
            }
        }
    }
    return rval;
}


bool CSingleFeatValidator::s_IsPseudo(const CSeq_feat& feat)
{
    if (feat.IsSetPseudo() && feat.GetPseudo()) {
        return true;
    } else if (s_HasNamedQual(feat, "pseudogene")) {
        return true;
    } else if (feat.IsSetData() && feat.GetData().IsGene() &&
        s_IsPseudo(feat.GetData().GetGene())) {
        return true;
    } else {
        return false;
    }
}


void CSingleFeatValidator::x_ValidateOldLocusTag(const string& old_locus_tag)
{
    if (NStr::IsBlank(old_locus_tag)) {
        return;
    }
    bool pseudo = s_IsPseudo(m_Feat);
    const CGene_ref* grp = m_Feat.GetGeneXref();
    if ( !grp) {
        // check overlapping gene
        CConstRef<CSeq_feat> overlap = m_Imp.GetGeneCache().GetGeneFromCache(&m_Feat, m_Scope);
        if ( overlap ) {
            if (s_IsPseudo(*overlap)) {
                pseudo = true;
            }
            string gene_old_locus_tag;

            FOR_EACH_GBQUAL_ON_SEQFEAT (it, *overlap) {
                if ((*it)->IsSetQual() && NStr::Equal ((*it)->GetQual(), "old_locus_tag")
                    && (*it)->IsSetVal() && !NStr::IsBlank((*it)->GetVal())) {
                    gene_old_locus_tag = (*it)->GetVal();
                    break;
                }
            }
            if (!NStr::IsBlank (gene_old_locus_tag)
                && !NStr::EqualNocase (gene_old_locus_tag, old_locus_tag)) {
                PostErr (eDiag_Warning, eErr_SEQ_FEAT_OldLocusTagMismtach,
                            "Old locus tag on feature (" + old_locus_tag
                            + ") does not match that on gene (" + gene_old_locus_tag + ")");
            }
            grp = &(overlap->GetData().GetGene());
        }
    }
    if (grp && s_IsPseudo(*grp)) {
        pseudo = true;
    }
    if (grp == 0 || ! grp->IsSetLocus_tag() || NStr::IsBlank (grp->GetLocus_tag())) {
        if (! pseudo) {
            PostErr(eDiag_Error, eErr_SEQ_FEAT_OldLocusTagWithoutLocusTag,
                    "old_locus_tag without inherited locus_tag");
        }
    }
}


void CSingleFeatValidator::x_ValidateImpFeatLoc()
{
    if (!m_Feat.GetData().IsImp()) {
        return;
    }
    const string& key = m_Feat.GetData().GetImp().GetKey();
    // validate the feature's location
    if ( m_Feat.GetData().GetImp().IsSetLoc() ) {
        const string& imp_loc = m_Feat.GetData().GetImp().GetLoc();
        if ( imp_loc.find("one-of") != string::npos ) {
            PostErr(eDiag_Error, eErr_SEQ_FEAT_ImpFeatBadLoc,
                "ImpFeat loc " + imp_loc + 
                " has obsolete 'one-of' text for feature " + key);
        } else if ( m_Feat.GetLocation().IsInt() ) {
            const CSeq_interval& seq_int = m_Feat.GetLocation().GetInt();
            string temp_loc = NStr::IntToString(seq_int.GetFrom() + 1) +
                ".." + NStr::IntToString(seq_int.GetTo() + 1);
            if ( imp_loc != temp_loc ) {
                PostErr(eDiag_Error, eErr_SEQ_FEAT_ImpFeatBadLoc,
                    "ImpFeat loc " + imp_loc + " does not equal feature location " +
                    temp_loc + " for feature " + key);
            }
        }
    }

}


void CSingleFeatValidator::x_ValidateImpFeatQuals()
{
    if (!m_Feat.GetData().IsImp()) {
        return;
    }
    const string& key = m_Feat.GetData().GetImp().GetKey();

    // Make sure a feature has its mandatory qualifiers
    for (auto required : CSeqFeatData::GetMandatoryQualifiers(m_Feat.GetData().GetSubtype())) {
        bool found = false;
        if (m_Feat.IsSetQual()) {
            for (auto qual : m_Feat.GetQual()) {
                if (qual->IsSetQual() && CSeqFeatData::GetQualifierType(qual->GetQual()) == required) {
                    found = true;
                    break;
                }
            }
            if (!found && required == CSeqFeatData::eQual_citation) {
                if (m_Feat.IsSetCit()) {
                    found = true;
                }
                else if (m_Feat.IsSetComment() && !NStr::IsBlank(m_Feat.GetComment())) {
                    // RefSeq allows conflict with accession in comment instead of sfp->cit
                    if (m_LocationBioseq) {
                        FOR_EACH_SEQID_ON_BIOSEQ(it, *(m_LocationBioseq.GetCompleteBioseq())) {
                            if ((*it)->IsOther()) {
                                found = true;
                                break;
                            }
                        }
                    }
                }
                if (!found
                    && (NStr::EqualNocase(key, "conflict")
                        || NStr::EqualNocase(key, "old_sequence"))) {
                    // compare qualifier can now substitute for citation qualifier for conflict and old_sequence
                    FOR_EACH_GBQUAL_ON_FEATURE(qual, m_Feat) {
                        if ((*qual)->IsSetQual() && CSeqFeatData::GetQualifierType((*qual)->GetQual()) == CSeqFeatData::eQual_compare) {
                            found = true;
                            break;
                        }
                    }
                }
            }
        }
        if (!found) {
            PostErr(eDiag_Warning, eErr_SEQ_FEAT_MissingQualOnImpFeat,
                "Missing qualifier " + CSeqFeatData::GetQualifierAsString(required) +
                " for feature " + key);
        }
    }
}


void CSingleFeatValidator::x_ValidateSeqFeatDataType()
{
    switch ( m_Feat.GetData().Which () ) {
    case CSeqFeatData::e_Gene:
    case CSeqFeatData::e_Cdregion:
    case CSeqFeatData::e_Prot:
    case CSeqFeatData::e_Rna:
    case CSeqFeatData::e_Pub:
    case CSeqFeatData::e_Imp:
    case CSeqFeatData::e_Biosrc:
    case CSeqFeatData::e_Org:
    case CSeqFeatData::e_Region:
    case CSeqFeatData::e_Seq:
    case CSeqFeatData::e_Comment:
    case CSeqFeatData::e_Bond:
    case CSeqFeatData::e_Site:
    case CSeqFeatData::e_Rsite:
    case CSeqFeatData::e_User:
    case CSeqFeatData::e_Txinit:
    case CSeqFeatData::e_Num:
    case CSeqFeatData::e_Psec_str:
    case CSeqFeatData::e_Non_std_residue:
    case CSeqFeatData::e_Het:
    case CSeqFeatData::e_Clone:
    case CSeqFeatData::e_Variation:
        break;
    default:
        PostErr(eDiag_Error, eErr_SEQ_FEAT_InvalidType,
            "Invalid SeqFeat type [" + NStr::IntToString(m_Feat.GetData().Which()) + "]");
        break;
    }
}


bool CSingleFeatValidator::s_BioseqHasRefSeqThatStartsWithPrefix (CBioseq_Handle bsh, string prefix)
{
    bool rval = false;
    FOR_EACH_SEQID_ON_BIOSEQ (it, *(bsh.GetBioseqCore())) {
        if ((*it)->IsOther() && (*it)->GetTextseq_Id()->IsSetAccession()
            && NStr::StartsWith ((*it)->GetTextseq_Id()->GetAccession(), prefix)) {
            rval = true;
            break;
        }
    }
    return rval;
}


void CSingleFeatValidator::x_ReportPseudogeneConflict(CConstRef <CSeq_feat> gene)
{
    if (!m_Feat.IsSetData() ||
        (m_Feat.GetData().GetSubtype() != CSeqFeatData::eSubtype_mRNA &&
        m_Feat.GetData().GetSubtype() != CSeqFeatData::eSubtype_cdregion)) {
        return;
    }
    string sfp_pseudo = kEmptyStr;
    string gene_pseudo = kEmptyStr;
    bool has_sfp_pseudo = false;
    bool has_gene_pseudo = false;
    if (m_Feat.IsSetQual()) {
        for (auto it : m_Feat.GetQual()) {
            if (it->IsSetQual() &&
                NStr::EqualNocase(it->GetQual(), "pseudogene") &&
                it->IsSetVal()) {
                sfp_pseudo = it->GetVal();
                has_sfp_pseudo = true;
            }
        }
    }
    if (gene && gene->IsSetQual()) {
        for (auto it : gene->GetQual()) {
            if (it->IsSetQual() &&
                NStr::EqualNocase(it->GetQual(), "pseudogene") &&
                it->IsSetVal()) {
                gene_pseudo = it->GetVal();
                has_gene_pseudo = true;
            }
        }
    }

    if (!has_sfp_pseudo && !has_gene_pseudo) {
        return;
    } else if (!has_sfp_pseudo) {
        return;
    } else if (has_sfp_pseudo && !has_gene_pseudo) {
        string msg = m_Feat.GetData().IsCdregion() ? "CDS" : "mRNA";
        msg += " has pseudogene qualifier, gene does not";
        PostErr(eDiag_Warning, eErr_SEQ_FEAT_InconsistentPseudogeneValue,
                msg);
    } else if (!NStr::EqualNocase(sfp_pseudo, gene_pseudo)) {
        string msg = "Different pseudogene values on ";
        msg += m_Feat.GetData().IsCdregion() ? "CDS" : "mRNA";
        msg += " (" + sfp_pseudo + ") and gene (" + gene_pseudo + ")";
        PostErr(eDiag_Warning, eErr_SEQ_FEAT_InconsistentPseudogeneValue,
                msg);
    }
}


// grp is from gene xref or from overlapping gene
void CSingleFeatValidator::x_ValidateLocusTagGeneralMatch(CConstRef <CSeq_feat> gene)
{
    if (!m_Imp.IsLocusTagGeneralMatch()) {
        return;
    }
    if (!m_Feat.IsSetProduct()) {
        return;
    }

    CTempString locus_tag = kEmptyStr;
    // obtain the gene-ref from the feature or the overlapping gene
    const CGene_ref* grp = m_Feat.GetGeneXref();
    if (grp && grp->IsSuppressed()) {
        return;
    } else if (grp && grp->IsSetLocus_tag() &&
               !NStr::IsBlank(grp->GetLocus_tag())) {
        locus_tag = grp->GetLocus_tag();
    } else if (gene && gene->GetData().GetGene().IsSetLocus_tag() &&
               !NStr::IsBlank(gene->GetData().GetGene().GetLocus_tag())) {
        locus_tag = gene->GetData().GetGene().GetLocus_tag();
    } else {
        return;
    }

    if (!m_ProductBioseq) {
        return;
    }

    for (auto id : m_ProductBioseq.GetId()) {
        CConstRef<CSeq_id> seqid = id.GetSeqId();
        if (!seqid || !seqid->IsGeneral()) {
            continue;
        }
        const CDbtag& dbt = seqid->GetGeneral();
        if (!dbt.IsSetDb() || dbt.IsSkippable()) {
            continue;
        }

        if (dbt.IsSetTag() && dbt.GetTag().IsStr()) {
            SIZE_TYPE pos = dbt.GetTag().GetStr().find('-');
            string str = dbt.GetTag().GetStr().substr(0, pos);
            if (!NStr::EqualNocase(locus_tag, str)) {
                PostErr(eDiag_Error, eErr_SEQ_FEAT_LocusTagProductMismatch,
                    "Gene locus_tag does not match general ID of product");
            }
        }
    }
}


void CSingleFeatValidator::x_CheckForNonAsciiCharacters()
{
    CStdTypeConstIterator<string> it(m_Feat);

    for (; it; ++it) {
        const string& str = *it;
        FOR_EACH_CHAR_IN_STRING(c_it, str) {
            const char& ch = *c_it;
            unsigned char chu = ch;
            if (ch > 127 || (ch < 32 && ch != '\t' && ch != '\r' && ch != '\n')) {
                PostErr(eDiag_Fatal, eErr_GENERIC_NonAsciiAsn,
                    "Non-ASCII character '" + NStr::NumericToString(chu) + "' found in feature (" + str + ")");
                break;
            }
        }
    }
}

void CProtValidator::Validate()
{
    CSingleFeatValidator::Validate();

    x_CheckForEmpty();

    const CProt_ref& prot = m_Feat.GetData().GetProt();
    for (auto it : prot.GetName()) {        
        if (prot.IsSetEc() && !prot.IsSetProcessed()
            && (NStr::EqualCase (it, "Hypothetical protein")
                || NStr::EqualCase (it, "hypothetical protein")
                || NStr::EqualCase (it, "Unknown protein")
                || NStr::EqualCase (it, "unknown protein"))) {
            PostErr (eDiag_Warning, eErr_SEQ_FEAT_BadProteinName, 
                     "Unknown or hypothetical protein should not have EC number");
        }

    }

    if (prot.IsSetDesc() && ContainsSgml(prot.GetDesc())) {
        PostErr (eDiag_Warning, eErr_GENERIC_SgmlPresentInText, 
                 "protein description " + prot.GetDesc() + " has SGML");
    }

    if (prot.IsSetDesc() && m_Feat.IsSetComment() 
        && NStr::EqualCase(prot.GetDesc(), m_Feat.GetComment())) {
        PostErr (eDiag_Warning, eErr_SEQ_FEAT_RedundantFields, 
                 "Comment has same value as protein description");
    }

    if (m_Feat.IsSetComment() && HasECnumberPattern(m_Feat.GetComment())) {
        PostErr(eDiag_Warning, eErr_SEQ_FEAT_EcNumberInProteinComment,
                 "Apparent EC number in protein comment");
    }

    x_ReportUninformativeNames();

    // only look for EC numbers in first protein name
    // only look for brackets and hypothetical protein XP_ in first protein name
    if (prot.IsSetName() && prot.GetName().size() > 0) {
        if (HasECnumberPattern(prot.GetName().front())) {
            PostErr(eDiag_Warning, eErr_SEQ_FEAT_EcNumberInProteinName,
                "Apparent EC number in protein title");
        }
        x_ValidateProteinName(prot.GetName().front());
    }

    if ( prot.CanGetDb () ) {
        m_Imp.ValidateDbxref(prot.GetDb(), m_Feat);
    }
    if ( (!prot.IsSetName() || prot.GetName().empty()) && 
         (!prot.IsSetProcessed() 
          || (prot.GetProcessed() != CProt_ref::eProcessed_signal_peptide
              && prot.GetProcessed() !=  CProt_ref::eProcessed_transit_peptide))) {
        if (prot.IsSetDesc() && !NStr::IsBlank (prot.GetDesc())) {
            PostErr(eDiag_Warning, eErr_SEQ_FEAT_NoNameForProtein,
                "Protein feature has description but no name");
        } else if (prot.IsSetActivity() && !prot.GetActivity().empty()) {
            PostErr(eDiag_Warning, eErr_SEQ_FEAT_NoNameForProtein,
                "Protein feature has function but no name");
        } else if (prot.IsSetEc() && !prot.GetEc().empty()) {
            PostErr(eDiag_Warning, eErr_SEQ_FEAT_NoNameForProtein,
                "Protein feature has EC number but no name");
        } else {
            PostErr(eDiag_Warning, eErr_SEQ_FEAT_NoNameForProtein,
                "Protein feature has no name");
        }
    }

    x_ValidateECNumbers();

    x_ValidateMolinfoPartials();
}


void CProtValidator::x_CheckForEmpty()
{
    const CProt_ref& prot = m_Feat.GetData().GetProt();
    CProt_ref::EProcessed processed = CProt_ref::eProcessed_not_set;

    if ( prot.IsSetProcessed() ) {
        processed = prot.GetProcessed();
    }

    bool empty = true;
    if ( processed != CProt_ref::eProcessed_signal_peptide  &&
         processed != CProt_ref::eProcessed_transit_peptide ) {
        if ( prot.IsSetName()  &&
            !prot.GetName().empty()  &&
            !prot.GetName().front().empty() ) {
            empty = false;
        }
        if ( prot.CanGetDesc()  &&  !prot.GetDesc().empty() ) {
            empty = false;
        }
        if ( prot.CanGetEc()  &&  !prot.GetEc().empty() ) {
            empty = false;
        }
        if ( prot.CanGetActivity()  &&  !prot.GetActivity().empty() ) {
            empty = false;
        }
        if ( prot.CanGetDb()  &&  !prot.GetDb().empty() ) {
            empty = false;
        }

        if ( empty ) {
            PostErr(eDiag_Warning, eErr_SEQ_FEAT_ProtRefHasNoData,
                "There is a protein feature where all fields are empty");
        }
    }
}


// note - list bad protein names in lower case, as search term is converted to lower case
// before looking for exact match
static const char* const sc_BadProtNameText [] = {
  "'hypothetical protein",
  "alpha",
  "alternative",
  "alternatively spliced",
  "bacteriophage hypothetical protein",
  "beta",
  "cellular",
  "cnserved hypothetical protein",
  "conesrved hypothetical protein",
  "conserevd hypothetical protein",
  "conserved archaeal protein",
  "conserved domain protein",
  "conserved hypohetical protein",
  "conserved hypotehtical protein",
  "conserved hypotheical protein",
  "conserved hypothertical protein",
  "conserved hypothetcial protein",
  "conserved hypothetical",
  "conserved hypothetical exported protein",
  "conserved hypothetical integral membrane protein",
  "conserved hypothetical membrane protein",
  "conserved hypothetical phage protein",
  "conserved hypothetical prophage protein",
  "conserved hypothetical protein",
  "conserved hypothetical protein - phage associated",
  "conserved hypothetical protein fragment 3",
  "conserved hypothetical protein, fragment",
  "conserved hypothetical protein, putative",
  "conserved hypothetical protein, truncated",
  "conserved hypothetical protein, truncation",
  "conserved hypothetical protein.",
  "conserved hypothetical protein; possible membrane protein",
  "conserved hypothetical protein; putative membrane protein",
  "conserved hypothetical proteins",
  "conserved hypothetical protien",
  "conserved hypothetical transmembrane protein",
  "conserved hypotheticcal protein",
  "conserved hypthetical protein",
  "conserved in bacteria",
  "conserved membrane protein",
  "conserved protein",
  "conserved protein of unknown function",
  "conserved protein of unknown function ; putative membrane protein",
  "conserved unknown protein",
  "conservedhypothetical protein",
  "conserverd hypothetical protein",
  "conservered hypothetical protein",
  "consrved hypothetical protein",
  "converved hypothetical protein",
  "cytokine",
  "delta",
  "drosophila",
  "duplicated hypothetical protein",
  "epsilon",
  "gamma",
  "hla",
  "homeodomain",
  "homeodomain protein",
  "homolog",
  "hyopthetical protein",
  "hypotethical",
  "hypotheical protein",
  "hypothertical protein",
  "hypothetcical protein",
  "hypothetical",
  "hypothetical  protein",
  "hypothetical conserved protein",
  "hypothetical exported protein",
  "hypothetical novel protein",
  "hypothetical orf",
  "hypothetical phage protein",
  "hypothetical prophage protein",
  "hypothetical protein (fragment)",
  "hypothetical protein (multi-domain)",
  "hypothetical protein (phage associated)",
  "hypothetical protein - phage associated",
  "hypothetical protein fragment",
  "hypothetical protein fragment 1",
  "hypothetical protein predicted by genemark",
  "hypothetical protein predicted by glimmer",
  "hypothetical protein predicted by glimmer/critica",
  "hypothetical protein, conserved",
  "hypothetical protein, phage associated",
  "hypothetical protein, truncated",
  "hypothetical protein-putative conserved hypothetical protein",
  "hypothetical protein.",
  "hypothetical proteins",
  "hypothetical protien",
  "hypothetical transmembrane protein",
  "hypothetoical protein",
  "hypothteical protein",
  "identified by sequence similarity; putative; orf located~using blastx/framed",
  "identified by sequence similarity; putative; orf located~using blastx/glimmer/genemark",
  "ion channel",
  "membrane protein, putative",
  "mouse",
  "narrowly conserved hypothetical protein",
  "novel protein",
  "orf",
  "orf, conserved hypothetical protein",
  "orf, hypothetical",
  "orf, hypothetical protein",
  "orf, hypothetical, fragment",
  "orf, partial conserved hypothetical protein",
  "orf; hypothetical protein",
  "orf; unknown function",
  "partial",
  "partial cds, hypothetical",
  "partially conserved hypothetical protein",
  "phage hypothetical protein",
  "phage-related conserved hypothetical protein",
  "phage-related protein",
  "plasma",
  "possible hypothetical protein",
  "precursor",
  "predicted coding region",
  "predicted protein",
  "predicted protein (pseudogene)",
  "predicted protein family",
  "product uncharacterised protein family",
  "protein family",
  "protein of unknown function",
  "pseudogene",
  "putative",
  "putative conserved protein",
  "putative exported protein",
  "putative hypothetical protein",
  "putative membrane protein",
  "putative orf; unknown function",
  "putative phage protein",
  "putative protein",
  "rearranged",
  "repeats containing protein",
  "reserved",
  "ribosomal protein",
  "similar to",
  "small",
  "small hypothetical protein",
  "transmembrane protein",
  "trna",
  "trp repeat",
  "trp-repeat protein",
  "truncated conserved hypothetical protein",
  "truncated hypothetical protein",
  "uncharacterized conserved membrane protein",
  "uncharacterized conserved protein",
  "uncharacterized conserved secreted protein",
  "uncharacterized protein",
  "uncharacterized protein conserved in archaea",
  "uncharacterized protein conserved in bacteria",
  "unique hypothetical",
  "unique hypothetical protein",
  "unknown",
  "unknown cds",
  "unknown function",
  "unknown gene",
  "unknown protein",
  "unknown, conserved protein",
  "unknown, hypothetical",
  "unknown-related protein",
  "unknown; predicted coding region",
  "unnamed",
  "unnamed protein product",
  "very hypothetical protein"
};
typedef CStaticArraySet<const char*, PCase_CStr> TBadProtNameSet;
DEFINE_STATIC_ARRAY_MAP(TBadProtNameSet, sc_BadProtName, sc_BadProtNameText);


void CProtValidator::x_ReportUninformativeNames() 
{
    if (!m_Imp.IsRefSeq()) {
        return;
    }
    const CProt_ref& prot = m_Feat.GetData().GetProt();
    if (!prot.IsSetName()) {
        if (!prot.IsSetProcessed() ||
            (prot.GetProcessed() != CProt_ref::eProcessed_signal_peptide &&
             prot.GetProcessed() !=  CProt_ref::eProcessed_transit_peptide)) {
                PostErr (eDiag_Error, eErr_SEQ_FEAT_NoNameForProtein,
                         "Protein name is not set");
              }
        return;
    }
    for (auto it : m_Feat.GetData().GetProt().GetName()) {
        string search = it;
        search = NStr::ToLower(search);
        if (search.empty()) {
            PostErr (eDiag_Error, eErr_SEQ_FEAT_NoNameForProtein,
                     "Protein name is empty");
        } else if (sc_BadProtName.find (search.c_str()) != sc_BadProtName.end()
            || NStr::Find(search, "=") != string::npos
            || NStr::Find(search, "~") != string::npos
            || NStr::FindNoCase(search, "uniprot") != string::npos
            || NStr::FindNoCase(search, "uniprotkb") != string::npos
            || NStr::FindNoCase(search, "pmid") != string::npos
            || NStr::FindNoCase(search, "dbxref") != string::npos) {
            PostErr (eDiag_Warning, eErr_SEQ_FEAT_UndesiredProteinName, 
                     "Uninformative protein name '" + it + "'");
        }
    }
}


void CProtValidator::x_ValidateECNumbers() 
{
    if (!m_Feat.GetData().GetProt().IsSetEc()) {
        return;
    }
    for (auto it : m_Feat.GetData().GetProt().GetEc()) {
        if (NStr::IsBlank (it)) {
            PostErr(eDiag_Warning, eErr_SEQ_FEAT_EcNumberEmpty, "EC number should not be empty");
        } else if (!CProt_ref::IsValidECNumberFormat(it)) {
            PostErr(eDiag_Warning, eErr_SEQ_FEAT_BadEcNumberFormat,
                    (it) + " is not in proper EC_number format");
        } else {
            const string& ec_number = it;
            CProt_ref::EECNumberStatus status = CProt_ref::GetECNumberStatus (ec_number);
            x_ReportECNumFileStatus();
            switch (status) {
                case CProt_ref::eEC_deleted:
                    PostErr(eDiag_Warning, eErr_SEQ_FEAT_DeletedEcNumber,
                             "EC_number " + ec_number + " was deleted");
                    break;
                case CProt_ref::eEC_replaced:
                    PostErr (eDiag_Warning, 
                             CProt_ref::IsECNumberSplit(ec_number) ? eErr_SEQ_FEAT_SplitEcNumber : eErr_SEQ_FEAT_ReplacedEcNumber, 
                             "EC_number " + ec_number + " was transferred and is no longer valid");
                    break;
                case CProt_ref::eEC_unknown:
                  {
                      size_t pos = NStr::Find (ec_number, "n");
                      if (pos == string::npos || !isdigit (ec_number.c_str()[pos + 1])) {
                            PostErr (eDiag_Warning, eErr_SEQ_FEAT_BadEcNumberValue, 
                                   ec_number + " is not a legal value for qualifier EC_number");
                      } else {
                            PostErr (eDiag_Info, eErr_SEQ_FEAT_BadEcNumberValue, 
                                   ec_number + " is not a legal preliminary value for qualifier EC_number");
                      }
                  }
                    break;
                default:
                    break;
            }
        }
    }

}


void CProtValidator::x_ValidateProteinName(const string& prot_name)
{
    if (NStr::EndsWith(prot_name, "]")) {
        bool report_name = true;
        size_t pos = NStr::Find(prot_name, "[", NStr::eNocase, NStr::eReverseSearch);
        if (pos == string::npos) {
            // no disqualifying text
        } else if (prot_name.length() - pos < 5) {
            // no disqualifying text
        } else if (NStr::EqualCase(prot_name, pos, 4, "[NAD")) {
            report_name = false;
        }
        if (!m_Imp.IsEmbl() && !m_Imp.IsTPE()) {
            if (report_name) {
                PostErr(eDiag_Warning, eErr_SEQ_FEAT_ProteinNameEndsInBracket,
                    "Protein name ends with bracket and may contain organism name");
            }
        }
    }
    if (NStr::StartsWith(prot_name, "hypothetical protein XP_") && m_LocationBioseq) {
        for (auto id_it : m_LocationBioseq.GetCompleteBioseq()->GetId()) {
            if (id_it->IsOther()
                && id_it->GetOther().IsSetAccession()
                && !NStr::EqualNocase(id_it->GetOther().GetAccession(),
                prot_name.substr(21))) {
                PostErr(eDiag_Warning, eErr_SEQ_FEAT_HypotheticalProteinMismatch,
                    "Hypothetical protein reference does not match accession");
            }
        }
    }
    if (!m_Imp.IsRefSeq() && NStr::FindNoCase(prot_name, "RefSeq") != string::npos) {
        PostErr(eDiag_Error, eErr_SEQ_FEAT_RefSeqInText, "Protein name contains 'RefSeq'");
    }
    if (m_Feat.IsSetComment() && NStr::EqualCase(m_Feat.GetComment(), prot_name)) {
        PostErr(eDiag_Warning, eErr_SEQ_FEAT_RedundantFields,
            "Comment has same value as protein name");
    }

    if (s_StringHasPMID(prot_name)) {
        PostErr(eDiag_Warning, eErr_SEQ_FEAT_ProteinNameHasPMID,
            "Protein name has internal PMID");
    }

    if (m_Imp.DoRubiscoTest()) {
        if (NStr::FindCase(prot_name, "ribulose") != string::npos
            && NStr::FindCase(prot_name, "bisphosphate") != string::npos
            && NStr::FindCase(prot_name, "methyltransferase") == string::npos
            && NStr::FindCase(prot_name, "activase") == string::npos) {
            if (NStr::EqualNocase(prot_name, "ribulose-1,5-bisphosphate carboxylase/oxygenase")) {
                // allow standard name without large or small subunit designation - later need kingdom test
            } else if (!NStr::EqualNocase(prot_name, "ribulose-1,5-bisphosphate carboxylase/oxygenase large subunit")
                && !NStr::EqualNocase(prot_name, "ribulose-1,5-bisphosphate carboxylase/oxygenase small subunit")) {
                PostErr(eDiag_Warning, eErr_SEQ_FEAT_RubiscoProblem,
                    "Nonstandard ribulose bisphosphate protein name");
            }
        }
    }



    ValidateCharactersInField(prot_name, "Protein name");
    if (ContainsSgml(prot_name)) {
        PostErr(eDiag_Warning, eErr_GENERIC_SgmlPresentInText,
            "protein name " + prot_name + " has SGML");
    }

}


void CProtValidator::x_ValidateMolinfoPartials()
{
    if (!m_LocationBioseq) {
        return;
    }
    const CBioseq& pbioseq = *(m_LocationBioseq.GetCompleteBioseq());
    // if there is a coding region for this bioseq, this type of error 
    // will be handled there
    const CSeq_feat* cds = m_Imp.GetCDSGivenProduct(pbioseq);
    if (cds) return;
    CFeat_CI prot(m_LocationBioseq, CSeqFeatData::eSubtype_prot);
    if (! prot) return;
        
    CSeqdesc_CI mi_i(m_LocationBioseq, CSeqdesc::e_Molinfo);
    if (! mi_i) return;
    const CMolInfo& mi = mi_i->GetMolinfo();
    if (! mi.IsSetCompleteness()) return;
    int completeness = mi.GetCompleteness();
        
    const CSeq_loc& prot_loc = prot->GetLocation();
    bool prot_partial5 = prot_loc.IsPartialStart(eExtreme_Biological);
    bool prot_partial3 = prot_loc.IsPartialStop(eExtreme_Biological);
        
    bool conflict = false;
    if (completeness == CMolInfo::eCompleteness_partial && ((! prot_partial5) && (! prot_partial3))) {
        conflict = true;
    } else if (completeness == CMolInfo::eCompleteness_no_left && ((! prot_partial5) || prot_partial3)) {
        conflict = true;
    } else if (completeness == CMolInfo::eCompleteness_no_right && (prot_partial5 || (! prot_partial3))) {
        conflict = true;
    } else if (completeness == CMolInfo::eCompleteness_no_ends && ((! prot_partial5) || (! prot_partial3))) {
        conflict = true;
    } else if ((completeness < CMolInfo::eCompleteness_partial || completeness > CMolInfo::eCompleteness_no_ends) && (prot_partial5 || prot_partial3)) {
        conflict = true;
    }
        
    if (conflict) {
        PostErr (eDiag_Warning, eErr_SEQ_FEAT_PartialsInconsistent,
            "Molinfo completeness and protein feature partials conflict");
    }
}

void CRNAValidator::Validate()
{
    CSingleFeatValidator::Validate();

    const CRNA_ref& rna = m_Feat.GetData().GetRna();

    CRNA_ref::EType rna_type = CRNA_ref::eType_unknown;
    if (rna.IsSetType()) {
        rna_type = rna.GetType();
    }

    if (rna_type == CRNA_ref::eType_rRNA) {
        if (rna.CanGetExt() && rna.GetExt().IsName()) {
            const string& rna_name = rna.GetExt().GetName();
            ValidateCharactersInField (rna_name, "rRNA name");
            if (ContainsSgml(rna_name)) {
                PostErr (eDiag_Warning, eErr_GENERIC_SgmlPresentInText, 
                    "rRNA name " + rna_name + " has SGML");
            }
        }
    }

    x_ValidateTrnaData();
    x_ValidateTrnaType();

    bool feat_pseudo = s_IsPseudo(m_Feat);
    bool pseudo = feat_pseudo;
    if (!pseudo) {
        CConstRef<CSeq_feat> gene = m_Imp.GetGeneCache().GetGeneFromCache(&m_Feat, m_Scope);
        if (gene) {
            pseudo = s_IsPseudo(*gene);
        }
    }

    if (!pseudo) {
        x_ValidateRnaTrans();
    }

    x_ValidateRnaProduct(feat_pseudo, pseudo);

    if (rna_type == CRNA_ref::eType_rRNA
        || rna_type == CRNA_ref::eType_snRNA
        || rna_type == CRNA_ref::eType_scRNA
        || rna_type == CRNA_ref::eType_snoRNA) {
        if (!rna.IsSetExt() || !rna.GetExt().IsName() || NStr::IsBlank(rna.GetExt().GetName())) {
            if (!pseudo) {
                string rna_typename = CRNA_ref::GetRnaTypeName(rna_type);
                PostErr(eDiag_Warning, eErr_SEQ_FEAT_rRNADoesNotHaveProduct,
                         rna_typename + " has no name");
            }
        }
    }


    if ( rna_type == CRNA_ref::eType_unknown ) {
        PostErr(eDiag_Warning, eErr_SEQ_FEAT_RNAtype0,
            "RNA type 0 (unknown) not supported");
    }


}


void CRNAValidator::x_ValidateRnaProduct(bool feat_pseudo, bool pseudo)
{
    if (!m_Feat.IsSetProduct()) {
        return;
    }

    x_ValidateRnaProductType();

    if ((!m_Feat.IsSetExcept_text() 
         || NStr::FindNoCase (m_Feat.GetExcept_text(), "transcribed pseudogene") == string::npos)
        && !m_Imp.IsRefSeq()) {
        if (feat_pseudo) {
            PostErr (eDiag_Warning, eErr_SEQ_FEAT_PseudoRnaHasProduct,
                     "A pseudo RNA should not have a product");
        } else if (pseudo) {
            PostErr (eDiag_Warning, eErr_SEQ_FEAT_PseudoRnaViaGeneHasProduct,
                     "An RNA overlapped by a pseudogene should not have a product");
        }
    }

}


void CRNAValidator::x_ValidateRnaProductType()
{    
    if ( !m_Feat.GetData().GetRna().IsSetType() || !m_ProductBioseq ) {
        return;
    }
    CSeqdesc_CI di(m_ProductBioseq, CSeqdesc::e_Molinfo);
    if ( !di ) {
        return;
    }
    const CMolInfo& mol_info = di->GetMolinfo();
    if ( !mol_info.CanGetBiomol() ) {
        return;
    }
    int biomol = mol_info.GetBiomol();
    
    switch ( m_Feat.GetData().GetRna().GetType() ) {

    case CRNA_ref::eType_mRNA:
        if ( biomol == CMolInfo::eBiomol_mRNA ) {
            return;
        }        
        break;

    case CRNA_ref::eType_tRNA:
        if ( biomol == CMolInfo::eBiomol_tRNA ) {
            return;
        }
        break;

    case CRNA_ref::eType_rRNA:
        if ( biomol == CMolInfo::eBiomol_rRNA ) {
            return;
        }
        break;

    default:
        return;
    }

    PostErr(eDiag_Error, eErr_SEQ_FEAT_RnaProductMismatch,
        "Type of RNA does not match MolInfo of product Bioseq");
}


static bool s_IsBioseqPartial (CBioseq_Handle bsh)
{
    CSeqdesc_CI sd(bsh, CSeqdesc::e_Molinfo);
    if ( !sd ) {
        return false;
    }
    const CMolInfo& molinfo = sd->GetMolinfo();
    if (!molinfo.IsSetCompleteness ()) {
        return false;
    } 
    CMolInfo::TCompleteness completeness = molinfo.GetCompleteness();
    if (completeness == CMolInfo::eCompleteness_partial
        || completeness == CMolInfo::eCompleteness_no_ends 
        || completeness == CMolInfo::eCompleteness_no_left
        || completeness == CMolInfo::eCompleteness_no_right) {
        return true;
    } else {
        return false;
    }
}


void CRNAValidator::x_ValidateTrnaData()
{
    if (!m_Feat.GetData().GetRna().IsSetExt() || !m_Feat.GetData().GetRna().GetExt().IsTRNA()) {
        return;
    }
    if ( !m_Feat.GetData().GetRna().IsSetType() ||
          m_Feat.GetData().GetRna().GetType() != CRNA_ref::eType_tRNA ) {
        PostErr(eDiag_Warning, eErr_SEQ_FEAT_InvalidTRNAdata,
                "tRNA data structure on non-tRNA feature");
    }

    const CTrna_ext& trna = m_Feat.GetData().GetRna().GetExt ().GetTRNA ();
    if ( trna.CanGetAnticodon () ) {
        const CSeq_loc& anticodon = trna.GetAnticodon();
        size_t anticodon_len = GetLength(anticodon, &m_Scope);
        if ( anticodon_len != 3 ) {
            PostErr (eDiag_Warning, eErr_SEQ_FEAT_tRNArange,
                "Anticodon is not 3 bases in length");
        }
        ECompare comp = sequence::Compare(anticodon,
                                            m_Feat.GetLocation(),
                                            &m_Scope,
                                            sequence::fCompareOverlapping);
        if ( comp != eContained  &&  comp != eSame ) {
            PostErr (eDiag_Error, eErr_SEQ_FEAT_tRNArange,
                "Anticodon location not in tRNA");
        }
        x_ValidateAnticodon(anticodon);
    }
    x_ValidateTrnaCodons();

}


void CRNAValidator::x_ValidateTrnaType()
{
    if (!m_Feat.GetData().GetRna().IsSetType() ||
        m_Feat.GetData().GetRna().GetType() != CRNA_ref::eType_tRNA) {
        return;
    }
    const CRNA_ref& rna = m_Feat.GetData().GetRna();

    // check for unparsed qualifiers
    for (auto& gbqual : m_Feat.GetQual()) {
        if ( NStr::CompareNocase(gbqual->GetQual (), "anticodon") == 0 ) {
            PostErr(eDiag_Error, eErr_SEQ_FEAT_UnparsedtRNAAnticodon,
                "Unparsed anticodon qualifier in tRNA");
        } else if (NStr::CompareNocase (gbqual->GetQual (), "product") == 0 ) {
            if (NStr::CompareNocase (gbqual->GetVal (), "tRNA-fMet") != 0 &&
                NStr::CompareNocase (gbqual->GetVal (), "tRNA-iMet") != 0 &&
                NStr::CompareNocase (gbqual->GetVal (), "tRNA-Ile2") != 0) {
                PostErr(eDiag_Error, eErr_SEQ_FEAT_UnparsedtRNAProduct,
                    "Unparsed product qualifier in tRNA");
            }
        }
    }


    /* tRNA with string extension */
    if ( rna.IsSetExt()  &&  
            rna.GetExt().Which () == CRNA_ref::C_Ext::e_Name ) {
        PostErr(eDiag_Error, eErr_SEQ_FEAT_UnparsedtRNAProduct,
            "Unparsed product qualifier in tRNA");
    } else if (!rna.IsSetExt() || rna.GetExt().Which() == CRNA_ref::C_Ext::e_not_set ) {
        PostErr (eDiag_Warning, eErr_SEQ_FEAT_MissingTrnaAA,
            "Missing encoded amino acid qualifier in tRNA");
    }

    x_ValidateTrnaOverlap();
}


void CRNAValidator::x_ValidateAnticodon(const CSeq_loc& anticodon)
{
    bool ordered = true;
    bool adjacent = false;
    bool unmarked_strand = false;
    bool mixed_strand = false;

    CSeq_loc_CI prev;
    for (CSeq_loc_CI curr(anticodon); curr; ++curr) {
        bool chk = true;
        if (curr.GetEmbeddingSeq_loc().IsInt()) {
            chk = sequence::IsValid(curr.GetEmbeddingSeq_loc().GetInt(), &m_Scope);
        } else if (curr.GetEmbeddingSeq_loc().IsPnt()) {
            chk = sequence::IsValid(curr.GetEmbeddingSeq_loc().GetPnt(), &m_Scope);
        } else {
            continue;
        }

        if ( !chk ) {
            string lbl;
            curr.GetEmbeddingSeq_loc().GetLabel(&lbl);
            PostErr(eDiag_Critical, eErr_SEQ_FEAT_tRNArange,
                "Anticodon location [" + lbl + "] out of range");
        }

        if ( prev  &&  curr  &&
             IsSameBioseq(curr.GetSeq_id(), prev.GetSeq_id(), &m_Scope) ) {
            CSeq_loc_CI::TRange prev_range = prev.GetRange();
            CSeq_loc_CI::TRange curr_range = curr.GetRange();
            if ( ordered ) {
                if ( curr.GetStrand() == eNa_strand_minus ) {
                    if (prev_range.GetTo() < curr_range.GetTo()) {
                        ordered = false;
                    }
                    if (curr_range.GetTo() + 1 == prev_range.GetFrom()) {
                        adjacent = true;
                    }
                } else {
                    if (prev_range.GetTo() > curr_range.GetTo()) {
                        ordered = false;
                    }
                    if (prev_range.GetTo() + 1 == curr_range.GetFrom()) {
                        adjacent = true;
                    }
                }
            }
            ENa_strand curr_strand = curr.GetStrand();
            ENa_strand prev_strand = prev.GetStrand();
            if ( curr_range == prev_range  &&  curr_strand == prev_strand ) {
                PostErr(eDiag_Warning, eErr_SEQ_FEAT_DuplicateAnticodonInterval,
                    "Duplicate anticodon exons in location");
            }
            if ( curr_strand != prev_strand ) {
                if (curr_strand == eNa_strand_plus  &&  prev_strand == eNa_strand_unknown) {
                    unmarked_strand = true;
                } else if (curr_strand == eNa_strand_unknown  &&  prev_strand == eNa_strand_plus) {
                    unmarked_strand = true;
                } else {
                    mixed_strand = true;
                }
            }
        }
        prev = curr;
    }
    if (adjacent) {
        PostErr(eDiag_Warning, eErr_SEQ_FEAT_AbuttingIntervals,
            "Adjacent intervals in Anticodon");
    }

    ENa_strand loc_strand = m_Feat.GetLocation().GetStrand();
    ENa_strand ac_strand = anticodon.GetStrand();
    if (loc_strand == eNa_strand_minus && ac_strand != eNa_strand_minus) {
        PostErr (eDiag_Error, eErr_SEQ_FEAT_AnticodonStrandConflict, 
                 "Anticodon strand and tRNA strand do not match.");
    } else if (loc_strand != eNa_strand_minus && ac_strand == eNa_strand_minus) {
        PostErr (eDiag_Error, eErr_SEQ_FEAT_AnticodonStrandConflict, 
                 "Anticodon strand and tRNA strand do not match.");
    }

    // trans splicing exception turns off both mixed_strand and out_of_order messages
    bool trans_splice = false;
    if (m_Feat.CanGetExcept()  &&  m_Feat.GetExcept()  && m_Feat.CanGetExcept_text()) {
        if (NStr::FindNoCase(m_Feat.GetExcept_text(), "trans-splicing") != NPOS) {
            trans_splice = true;
        }
    }
    if (!trans_splice) {
        string loc_lbl = "";
        anticodon.GetLabel(&loc_lbl);
        if (mixed_strand) {
            PostErr(eDiag_Warning, eErr_SEQ_FEAT_AnticodonMixedStrand,
                "Mixed strands in Anticodon [" + loc_lbl + "]");
        }
        if (unmarked_strand) {
            PostErr(eDiag_Warning, eErr_SEQ_FEAT_AnticodonMixedStrand,
                "Mixed plus and unknown strands in Anticodon [" + loc_lbl + "]");
        }
        if (!ordered) {
            PostErr(eDiag_Warning, eErr_SEQ_FEAT_SeqLocOrder,
                "Intervals out of order in Anticodon [" + loc_lbl + "]");
        }
    }
}


int s_LegalNcbieaaValues[] = { 42, 65, 66, 67, 68, 69, 70, 71, 72, 73,
                               74, 75, 76, 77, 78, 79, 80, 81, 82, 83, 
                               84, 85, 86, 87, 88, 89, 90 };

static const char* kAANames[] = {
    "---", "Ala", "Asx", "Cys", "Asp", "Glu", "Phe", "Gly", "His", "Ile",
    "Lys", "Leu", "Met", "Asn", "Pro", "Gln", "Arg", "Ser", "Thr", 
    "Val", "Trp", "OTHER", "Tyr", "Glx", "Sec", "TERM", "Pyl", "Xle"
};


const char* GetAAName(unsigned char aa, bool is_ascii)
{
    try {
        if (is_ascii) {
            aa = CSeqportUtil::GetMapToIndex
                (CSeq_data::e_Ncbieaa, CSeq_data::e_Ncbistdaa, aa);
        }
        return (aa < sizeof(kAANames)/sizeof(*kAANames)) ? kAANames[aa] : "OTHER";
    } catch (CException ) {
        return "OTHER";
    } catch (std::exception ) {
        return "OTHER";
    }
}


static string GetGeneticCodeName (int gcode)
{
    const CGenetic_code_table& code_table = CGen_code_table::GetCodeTable();
    const list<CRef<CGenetic_code> >& codes = code_table.Get();

    for ( list<CRef<CGenetic_code> >::const_iterator code_it = codes.begin(), code_it_end = codes.end();  code_it != code_it_end;  ++code_it ) {
        if ((*code_it)->GetId() == gcode) {
            return (*code_it)->GetName();
        }
    }
    return "unknown";
}


void CRNAValidator::x_ValidateTrnaCodons()
{
    if (!m_Feat.IsSetData() || !m_Feat.GetData().IsRna() || 
        !m_Feat.GetData().GetRna().IsSetExt() || 
        !m_Feat.GetData().GetRna().GetExt().IsTRNA()) {
        return;
    }
    const CTrna_ext& trna = m_Feat.GetData().GetRna().GetExt().GetTRNA();

    if (!trna.IsSetAa()) {
        PostErr (eDiag_Error, eErr_SEQ_FEAT_BadTrnaAA, "Missing tRNA amino acid");
        return;
    }

    unsigned char aa = 0, orig_aa;
    vector<char> seqData;
    string str = "";
    
    switch (trna.GetAa().Which()) {
        case CTrna_ext::C_Aa::e_Iupacaa:
            str = trna.GetAa().GetIupacaa();
            CSeqConvert::Convert(str, CSeqUtil::e_Iupacaa, 0, str.size(), seqData, CSeqUtil::e_Ncbieaa);
            aa = seqData[0];
            break;
        case CTrna_ext::C_Aa::e_Ncbi8aa:
            str = trna.GetAa().GetNcbi8aa();
            CSeqConvert::Convert(str, CSeqUtil::e_Ncbi8aa, 0, str.size(), seqData, CSeqUtil::e_Ncbieaa);
            aa = seqData[0];
            break;
        case CTrna_ext::C_Aa::e_Ncbistdaa:
            str = trna.GetAa().GetNcbi8aa();
            CSeqConvert::Convert(str, CSeqUtil::e_Ncbistdaa, 0, str.size(), seqData, CSeqUtil::e_Ncbieaa);
            aa = seqData[0];
            break;
        case CTrna_ext::C_Aa::e_Ncbieaa:
            seqData.push_back(trna.GetAa().GetNcbieaa());
            aa = seqData[0];
            break;
        default:
            NCBI_THROW (CCoreException, eCore, "Unrecognized tRNA aa coding");
            break;
    }

    // make sure the amino acid is valid
    bool found = false;
    for ( unsigned int i = 0; i < sizeof (s_LegalNcbieaaValues) / sizeof (int); ++i ) {
        if ( aa == s_LegalNcbieaaValues[i] ) {
            found = true;
            break;
        }
    }
    orig_aa = aa;
    if ( !found ) {
        aa = ' ';
    }

    if (m_Feat.GetData().GetRna().IsSetType() &&
        m_Feat.GetData().GetRna().GetType() == CRNA_ref::eType_tRNA) {
        bool mustbemethionine = false;
        for (auto gbqual : m_Feat.GetQual()) {
            if (NStr::CompareNocase(gbqual->GetQual(), "product") == 0 &&
                (NStr::CompareNocase(gbqual->GetVal(), "tRNA-fMet") == 0 ||
                    NStr::CompareNocase(gbqual->GetVal(), "tRNA-iMet") == 0)) {
                mustbemethionine = true;
                break;
            }
        }
        if (mustbemethionine) {
            if (aa != 'M') {
                string aanm = GetAAName(aa, true);
                PostErr(eDiag_Error, eErr_SEQ_FEAT_InvalidQualifierValue,
                    "Initiation tRNA claims to be tRNA-" + aanm +
                    ", but should be tRNA-Met");
            }
        }
    }

    // Retrive the Genetic code id for the tRNA
    int gcode = 1;
    if ( m_LocationBioseq ) {
        // need only the closest biosoure.
        CSeqdesc_CI diter(m_LocationBioseq, CSeqdesc::e_Source);
        if ( diter ) {
            gcode = diter->GetSource().GetGenCode();
        }
    }
    
    const string& ncbieaa = CGen_code_table::GetNcbieaa(gcode);
    if ( ncbieaa.length() != 64 ) {
        return;
    }

    string codename = GetGeneticCodeName (gcode);
    char buf[2];
    buf[0] = aa;
    buf[1] = 0;
    string aaname = buf;
    aaname += "/";
    aaname += GetAAName (aa, true);
        
    EDiagSev sev = (aa == 'U' || aa == 'O') ? eDiag_Warning : eDiag_Error;

    bool modified_codon_recognition = false;
    bool rna_editing = false;
    if ( m_Feat.IsSetExcept_text() ) {
        string excpt_text = m_Feat.GetExcept_text();
        if ( NStr::FindNoCase(excpt_text, "modified codon recognition") != NPOS ) {
            modified_codon_recognition = true;
        }
        if ( NStr::FindNoCase(excpt_text, "RNA editing") != NPOS ) {
            rna_editing = true;
        }
    }

    vector<string> recognized_codon_values;
    vector<unsigned char> recognized_taa_values;

    ITERATE( CTrna_ext::TCodon, iter, trna.GetCodon() ) {
        if (*iter == 255) continue;
        // test that codon value is in range 0 - 63
        if ( *iter > 63 ) {
            PostErr(sev, eErr_SEQ_FEAT_BadTrnaCodon,
                "tRNA codon value " + NStr::IntToString(*iter) + 
                " is greater than maximum 63");
            continue;
        } else if (*iter < 0) {
            PostErr(sev, eErr_SEQ_FEAT_BadTrnaCodon,
                "tRNA codon value " + NStr::IntToString(*iter) +
                " is less than 0");
            continue;
        }

        if ( !modified_codon_recognition && !rna_editing ) {
            unsigned char taa = ncbieaa[*iter];
            string codon = CGen_code_table::IndexToCodon(*iter);
            recognized_codon_values.push_back (codon);
            recognized_taa_values.push_back (taa);

            if ( taa != aa ) {
                if ( (aa == 'U')  &&  (taa == '*')  &&  (*iter == 14) ) {
                    // selenocysteine normally uses TGA (14), so ignore without requiring exception in record
                    // TAG (11) is used for pyrrolysine in archaebacteria
                    // TAA (10) is not yet known to be used for an exceptional amino acid
                } else {
                    NStr::ReplaceInPlace (codon, "T", "U");

                    PostErr(sev, eErr_SEQ_FEAT_TrnaCodonWrong,
                      "Codon recognized by tRNA (" + codon + ") does not match amino acid (" 
                       + aaname + ") specified by genetic code ("
                       + NStr::IntToString (gcode) + "/" + codename + ")");
                }
            }
        }
    }

    // see if anticodon is compatible with codons recognized and amino acid
    string anticodon = "?";
    vector<string> codon_values;
    vector<unsigned char> taa_values;

    if (trna.IsSetAnticodon() && GetLength (trna.GetAnticodon(), &m_Scope) == 3) {
        try {
            anticodon = GetSequenceStringFromLoc(trna.GetAnticodon(), m_Scope);
            // get reverse complement sequence for location
            CRef<CSeq_loc> codon_loc(SeqLocRevCmpl(trna.GetAnticodon(), &m_Scope));
            string codon = GetSequenceStringFromLoc(*codon_loc, m_Scope);
            if (codon.length() > 3) {
                codon = codon.substr (0, 3);
            }

            // expand wobble base to known binding partners
            string wobble = "";


            char ch = anticodon.c_str()[0];
            switch (ch) {
                case 'A' :
                    wobble = "ACT";
                    break;
                case 'C' :
                    wobble = "G";
                    break;
                case 'G' :
                    wobble = "CT";
                    break;
                case 'T' :
                    wobble = "AG";
                    break;
                default :
                    break;
            }
            if (!NStr::IsBlank(wobble)) {
                string::iterator str_it = wobble.begin();
                while (str_it != wobble.end()) {
                    codon[2] = *str_it;
                    int index = CGen_code_table::CodonToIndex (codon);
                    if (index < 64 && index > -1) {
                        unsigned char taa = ncbieaa[index];
                        taa_values.push_back(taa);
                        codon_values.push_back(codon);
                    }
                    ++str_it;
                }
            }
            NStr::ReplaceInPlace (anticodon, "T", "U");
            if (anticodon.length() > 3) {
                anticodon = anticodon.substr(0, 3);
            }
            } catch (CException ) {
            } catch (std::exception ) {
        }

        if (codon_values.size() > 0) {
            bool ok = false;
            // check that codons predicted from anticodon can transfer indicated amino acid
            for (size_t i = 0; i < codon_values.size(); i++) {
                if (!NStr::IsBlank (codon_values[i]) && aa == taa_values[i]) {
                    ok = true;
                }
            }
            if (!ok) {
                if (aa == 'U' && NStr::Equal (anticodon, "UCA")) {
                    // ignore TGA codon for selenocysteine
                } else if (aa == 'O' && NStr::Equal (anticodon, "CUA")) {
                    // ignore TAG codon for pyrrolysine
                } else if (aa == 'I' && NStr::Equal (anticodon, "CAU")) {
                    // ignore ATG predicted codon for Ile2
                } else if (!m_Feat.IsSetExcept_text()
                          || (NStr::FindNoCase(m_Feat.GetExcept_text(), "modified codon recognition") == string::npos 
                              &&NStr::FindNoCase(m_Feat.GetExcept_text(), "RNA editing") == string::npos)) {
                    PostErr (eDiag_Warning, eErr_SEQ_FEAT_BadAnticodonAA,
                              "Codons predicted from anticodon (" + anticodon
                              + ") cannot produce amino acid (" + aaname + ")");
                }
            }

            // check that codons recognized match codons predicted from anticodon
            if (recognized_codon_values.size() > 0) {
                bool ok = false;
                for (size_t i = 0; i < codon_values.size() && !ok; i++) {
                    for (size_t j = 0; j < recognized_codon_values.size() && !ok; j++) {
                        if (NStr::Equal (codon_values[i], recognized_codon_values[j])) {
                            ok = true;
                        } else if ( NStr::Equal (codon_values[i], "ATG") && aa == 'I') {
                            // allow ATG recognized codon (pre-RNA-editing) for Ile2
                            ok = true;
                        }
                    }
                }
                if (!ok 
                    && (!m_Feat.IsSetExcept_text() 
                        || NStr::FindNoCase (m_Feat.GetExcept_text(), "RNA editing") == string::npos)) {
                    PostErr (eDiag_Warning, eErr_SEQ_FEAT_BadAnticodonCodon,
                             "Codon recognized cannot be produced from anticodon ("
                             + anticodon + ")");
                }
            }
        }
    }

    if (!m_Feat.IsSetPseudo() || !m_Feat.GetPseudo()) {
        if (orig_aa == 0 || orig_aa == 255) {
            PostErr (sev, eErr_SEQ_FEAT_BadTrnaAA, "Missing tRNA amino acid");
        } else {
            // verify that legal amino acid is indicated
            unsigned int idx;
            if (aa != '*') {
                idx = aa - 64;
            } else {
                idx = 25;
            }
            if (idx == 0 || idx >= 28) {
                PostErr (sev, eErr_SEQ_FEAT_BadTrnaAA, "Invalid tRNA amino acid");
            }
        }
    }
}


void CRNAValidator::x_ValidateTrnaOverlap()
{
    if (!m_Feat.GetData().GetRna().IsSetType() || 
        m_Feat.GetData().GetRna().GetType() != CRNA_ref::eType_tRNA) {
        return;
    }
    TFeatScores scores;
    GetOverlappingFeatures(m_Feat.GetLocation(),
                            CSeqFeatData::e_Rna,
                            CSeqFeatData::eSubtype_rRNA,
                            eOverlap_Interval,
                            scores, m_Scope);
    bool found_bad = false;
    for (auto it : scores) {
        CRef<CSeq_loc> intersection = it.second->GetLocation().Intersect(m_Feat.GetLocation(),
            0 /* flags*/,
            NULL /* synonym mapper */);
        if (intersection) {
            TSeqPos length = sequence::GetLength(*intersection, &m_Scope);
            if (length >= 5) {
                found_bad = true;
                break;
            }
        }
    }
    if (found_bad) {
        PostErr(eDiag_Warning, eErr_SEQ_FEAT_BadRRNAcomponentOverlapTRNA,
            "tRNA-rRNA overlap");
    }
}


void CRNAValidator::x_ValidateRnaTrans()
{
    size_t mismatches = 0;
    size_t problems = GetMRNATranslationProblems
        (m_Feat, mismatches, m_Imp.IgnoreExceptions(),
         m_LocationBioseq, m_ProductBioseq, 
         m_Imp.IsFarFetchMRNAproducts(), m_Imp.IsGpipe(),
         m_Imp.IsGenomic(), &m_Scope);
    x_ReportRNATranslationProblems(problems, mismatches);
}


void CRNAValidator::x_ReportRNATranslationProblems(size_t problems, size_t mismatches)
{
    if (problems & eMRNAProblem_TransFail) {
        PostErr(eDiag_Error, eErr_SEQ_FEAT_MrnaTransFail,
            "Unable to transcribe mRNA");
    }
    if (problems & eMRNAProblem_UnableToFetch) {
        const CSeq_id& product_id = GetId(m_Feat.GetProduct(), &m_Scope);
        string label = product_id.AsFastaString();
        PostErr(eDiag_Error, eErr_SEQ_FEAT_ProductFetchFailure,
            "Unable to fetch mRNA transcript '" + label + "'");
    }

    bool is_refseq = m_Imp.IsRefSeqConventions();
    if (m_LocationBioseq) {
        FOR_EACH_SEQID_ON_BIOSEQ(it, *(m_LocationBioseq.GetCompleteBioseq())) {
            if ((*it)->IsOther()) {
                is_refseq = true;
                break;
            }
        }
    }

    TSeqPos feat_len = sequence::GetLength(m_Feat.GetLocation(), &m_Scope);

    string farstr;
    EDiagSev sev = eDiag_Error;

    // if not local bioseq product, lower severity (with the exception of Refseq)
    if (m_ProductIsFar && !is_refseq) {
        sev = eDiag_Warning;
    }
    if (m_ProductIsFar) {
        farstr = "(far) ";
        if (m_Feat.IsSetPartial()
            && !s_IsBioseqPartial(m_ProductBioseq)
            && s_BioseqHasRefSeqThatStartsWithPrefix(m_ProductBioseq, "NM_")) {
            sev = eDiag_Warning;
        }
    }

    if (problems & eMRNAProblem_TranscriptLenLess) {
        PostErr(sev, eErr_SEQ_FEAT_TranscriptLen,
            "Transcript length [" + NStr::SizetToString(feat_len) +
            "] less than " + farstr + "product length [" +
            NStr::SizetToString(m_ProductBioseq.GetInst_Length()) + "], and tail < 95% polyA");
    }

    if (problems & eMRNAProblem_PolyATail100) {
        PostErr(eDiag_Info, eErr_SEQ_FEAT_PolyATail,
            "Transcript length [" + NStr::SizetToString(feat_len)
            + "] less than " + farstr + "product length ["
            + NStr::SizetToString(m_ProductBioseq.GetInst_Length()) + "], but tail is 100% polyA");
    }
    if (problems & eMRNAProblem_PolyATail95) {
        PostErr(eDiag_Info, eErr_SEQ_FEAT_PolyATail,
            "Transcript length [" + NStr::SizetToString(feat_len) +
            "] less than " + farstr + "product length [" +
            NStr::SizetToString(m_ProductBioseq.GetInst_Length()) + "], but tail >= 95% polyA");
    }
    if (problems & eMRNAProblem_TranscriptLenMore) {
        PostErr(sev, eErr_SEQ_FEAT_TranscriptLen,
            "Transcript length [" + NStr::IntToString(feat_len) + "] " +
            "greater than " + farstr + "product length [" +
            NStr::IntToString(m_ProductBioseq.GetInst_Length()) + "]");
    }
    if ((problems & eMRNAProblem_Mismatch) && mismatches > 0) {
        PostErr(sev, eErr_SEQ_FEAT_TranscriptMismatches,
            "There are " + NStr::SizetToString(mismatches) +
            " mismatches out of " + NStr::SizetToString(feat_len) +
            " bases between the transcript and " + farstr + "product sequence");
    }
    if (problems & eMRNAProblem_UnnecessaryException) {
        PostErr(eDiag_Warning, eErr_SEQ_FEAT_UnnecessaryException,
            "mRNA has exception but passes transcription test");
    }
    if (problems & eMRNAProblem_ErroneousException) {
        size_t total = min(feat_len, m_ProductBioseq.GetInst_Length());
        PostErr(eDiag_Warning, eErr_SEQ_FEAT_ErroneousException,
            "mRNA has unclassified exception but only difference is " + NStr::SizetToString(mismatches)
            + " mismatches out of " + NStr::SizetToString(total) + " bases");
    }
    if (problems & eMRNAProblem_ProductReplaced) {
        PostErr(eDiag_Warning, eErr_SEQ_FEAT_mRNAUnnecessaryException,
            "mRNA has transcribed product replaced exception");
    }
}


CMRNAValidator::CMRNAValidator(const CSeq_feat& feat, CScope& scope, CValidError_imp& imp) :
CRNAValidator(feat, scope, imp) 
{
    m_Gene = m_Imp.GetGeneCache().GetGeneFromCache(&feat, m_Scope);
    if (m_Gene) {
        m_GeneIsPseudo = s_IsPseudo(*m_Gene);
    } else {
        m_GeneIsPseudo = false;
    }
    m_FeatIsPseudo = s_IsPseudo(m_Feat);
}


void CMRNAValidator::Validate()
{
    CRNAValidator::Validate();

    x_ReportPseudogeneConflict(m_Gene);
    x_ValidateLocusTagGeneralMatch(m_Gene);

    x_ValidateMrna();

    if (!m_GeneIsPseudo && !m_FeatIsPseudo) {
        x_ValidateCommonMRNAProduct();
    }
    x_ValidateMrnaGene();
}


void CMRNAValidator::x_ValidateMrna()
{
    bool pseudo = m_GeneIsPseudo;
    if (!pseudo) {
        pseudo = s_IsPseudo(m_Feat);
    }
    ValidateSplice(pseudo, false);

    const CRNA_ref& rna = m_Feat.GetData().GetRna();

    if (m_Feat.IsSetQual()) {
        for (auto it : m_Feat.GetQual()) {
            const CGb_qual& qual = *it;
            if (qual.CanGetQual()) {
                const string& key = qual.GetQual();
                if (NStr::EqualNocase(key, "protein_id")) {
                    PostErr(eDiag_Warning, eErr_SEQ_FEAT_WrongQualOnFeature,
                        "protein_id should not be a gbqual on an mRNA feature");
                }
                else if (NStr::EqualNocase(key, "transcript_id")) {
                    PostErr(eDiag_Warning, eErr_SEQ_FEAT_WrongQualOnFeature,
                        "transcript_id should not be a gbqual on an mRNA feature");
                }
            }
        }
    }

    if (rna.IsSetExt() && rna.GetExt().IsName()) {
        const string& rna_name = rna.GetExt().GetName();
        if (NStr::StartsWith(rna_name, "transfer RNA ") &&
            (!NStr::EqualNocase(rna_name, "transfer RNA nucleotidyltransferase")) &&
            (!NStr::EqualNocase(rna_name, "transfer RNA methyltransferase"))) {
            PostErr(eDiag_Warning, eErr_SEQ_FEAT_tRNAmRNAmixup,
                "mRNA feature product indicates it should be a tRNA feature");
        }
        ValidateCharactersInField(rna_name, "mRNA name");
        if (ContainsSgml(rna_name)) {
            PostErr(eDiag_Warning, eErr_GENERIC_SgmlPresentInText,
                "mRNA name " + rna_name + " has SGML");
        }
    }
}


void CMRNAValidator::x_ValidateCommonMRNAProduct()
{
    if (!m_Feat.IsSetProduct()) {
        return;
    }
    if ( !m_ProductBioseq) {
        if (m_LocationBioseq) {
            CSeq_entry_Handle seh = m_LocationBioseq.GetTopLevelEntry();
            if (seh.IsSet() && seh.GetSet().IsSetClass()
                && (seh.GetSet().GetClass() == CBioseq_set::eClass_gen_prod_set
                    || seh.GetSet().GetClass() == CBioseq_set::eClass_other)) {
                PostErr(eDiag_Error, eErr_SEQ_FEAT_MissingMRNAproduct,
                    "Product Bioseq of mRNA feature is not "
                    "packaged in the record");
            }
        }
    } else {

        CConstRef<CSeq_feat> mrna = m_Imp.GetmRNAGivenProduct (*(m_ProductBioseq.GetCompleteBioseq()));
        if (mrna && mrna.GetPointer() != &m_Feat) {
            PostErr(eDiag_Critical, eErr_SEQ_FEAT_IdenticalMRNAtranscriptIDs,
                    "Identical transcript IDs found on multiple mRNAs");
        }
    }
}


static bool s_EqualGene_ref(const CGene_ref& genomic, const CGene_ref& mrna)
{
    bool locus = (!genomic.CanGetLocus()  &&  !mrna.CanGetLocus())  ||
        (genomic.CanGetLocus()  &&  mrna.CanGetLocus()  &&
        genomic.GetLocus() == mrna.GetLocus());
    bool allele = (!genomic.CanGetAllele()  &&  !mrna.CanGetAllele())  ||
        (genomic.CanGetAllele()  &&  mrna.CanGetAllele()  &&
        genomic.GetAllele() == mrna.GetAllele());
    bool desc = (!genomic.CanGetDesc()  &&  !mrna.CanGetDesc())  ||
        (genomic.CanGetDesc()  &&  mrna.CanGetDesc()  &&
        genomic.GetDesc() == mrna.GetDesc());
    bool locus_tag = (!genomic.CanGetLocus_tag()  &&  !mrna.CanGetLocus_tag())  ||
        (genomic.CanGetLocus_tag()  &&  mrna.CanGetLocus_tag()  &&
        genomic.GetLocus_tag() == mrna.GetLocus_tag());

    return locus  &&  allele  &&  desc  && locus_tag;
}


// check that there is no conflict between the gene on the genomic 
// and the gene on the mrna.
void CMRNAValidator::x_ValidateMrnaGene()
{
    if (!m_ProductBioseq) {
        return;
    }
    const CGene_ref* genomicgrp = NULL;
    if (m_Gene) {
        genomicgrp = &(m_Gene->GetData().GetGene());
    } else {
        genomicgrp = m_Feat.GetGeneXref();
    }
    if (!genomicgrp) {
        return;
    }
    CFeat_CI mrna_gene(m_ProductBioseq, CSeqFeatData::e_Gene);
    if ( mrna_gene ) {
        const CGene_ref& mrnagrp = mrna_gene->GetData().GetGene();
        if ( !s_EqualGene_ref(*genomicgrp, mrnagrp) ) {
            m_Imp.PostErr(eDiag_Warning, eErr_SEQ_FEAT_GenesInconsistent,
                "Gene on mRNA bioseq does not match gene on genomic bioseq",
                mrna_gene->GetOriginalFeature());
        }
    }

}


void CPubFeatValidator::Validate()
{
    CSingleFeatValidator::Validate();
    m_Imp.ValidatePubdesc(m_Feat.GetData().GetPub (), m_Feat);
}


void CSrcFeatValidator::Validate()
{
    CSingleFeatValidator::Validate();
    const CBioSource& bsrc = m_Feat.GetData().GetBiosrc();
    if ( bsrc.IsSetIs_focus() ) {
        PostErr(eDiag_Error, eErr_SEQ_FEAT_FocusOnBioSourceFeature,
            "Focus must be on BioSource descriptor, not BioSource feature.");
    }

    m_Imp.ValidateBioSource(bsrc, m_Feat);

    CSeqdesc_CI dbsrc_i(m_LocationBioseq, CSeqdesc::e_Source);
    if ( !dbsrc_i ) {
        return;
    }
    
    const COrg_ref& org = bsrc.GetOrg();           
    const CBioSource& dbsrc = dbsrc_i->GetSource();
    const COrg_ref& dorg = dbsrc.GetOrg(); 

    if ( org.CanGetTaxname()  &&  !org.GetTaxname().empty()  &&
            dorg.CanGetTaxname() ) {
        string taxname = org.GetTaxname();
        string dtaxname = dorg.GetTaxname();
        if ( NStr::CompareNocase(taxname, dtaxname) != 0 ) {
            if ( !dbsrc.IsSetIs_focus()  &&  !m_Imp.IsTransgenic(dbsrc) ) {
                PostErr(eDiag_Error, eErr_SEQ_DESCR_BioSourceNeedsFocus,
                    "BioSource descriptor must have focus or transgenic "
                    "when BioSource feature with different taxname is "
                    "present.");
            }
        }
    }
}


void CPolyASiteValidator::x_ValidateSeqFeatLoc()
{
    CSingleFeatValidator::x_ValidateSeqFeatLoc();
    CSeq_loc::TRange range = m_Feat.GetLocation().GetTotalRange();
    if ( range.GetFrom() != range.GetTo() ) {
        EDiagSev sev = eDiag_Warning;
        if (m_Imp.IsRefSeq()) {
            sev = eDiag_Error;
        }
        PostErr(sev, eErr_SEQ_FEAT_PolyAsiteNotPoint,
            "PolyA_site should be a single point");
    }

}


void CPolyASignalValidator::x_ValidateSeqFeatLoc()
{
    CSeq_loc::TRange range = m_Feat.GetLocation().GetTotalRange();
    if ( range.GetFrom() == range.GetTo() ) {
        EDiagSev sev = eDiag_Warning;
        if (m_Imp.IsRefSeq()) {
            sev = eDiag_Error;
        }
        PostErr (sev, eErr_SEQ_FEAT_PolyAsignalNotRange, "PolyA_signal should be a range");
    }
}


CPeptideValidator::CPeptideValidator(const CSeq_feat& feat, CScope& scope, CValidError_imp& imp) :
        CSingleFeatValidator(feat, scope, imp) 
{
    m_CDS = GetOverlappingCDS(m_Feat.GetLocation(), m_Scope);
}


void CPeptideValidator::Validate()
{
    CSingleFeatValidator::Validate();

    if (m_Imp.IsEmbl() || m_Imp.IsDdbj()) {
        PostErr(!m_CDS ? eDiag_Error : eDiag_Warning, eErr_SEQ_FEAT_PeptideFeatureLacksCDS,
            "sig/mat/transit_peptide feature cannot be associated with a "
            "protein product of a coding region feature");
    } else {
        PostErr(m_Imp.IsRefSeq() ? eDiag_Error : eDiag_Warning, eErr_SEQ_FEAT_PeptideFeatureLacksCDS,
            "Peptide processing feature should be converted to the "
            "appropriate protein feature subtype");
    }
    x_ValidatePeptideOnCodonBoundary();
}


void CPeptideValidator::x_ValidatePeptideOnCodonBoundary()
{
    if (!m_CDS) {
        return;
    }

    const string& key = m_Feat.GetData().GetImp().GetKey();

    feature::ELocationInFrame in_frame = feature::IsLocationInFrame(m_Scope.GetSeq_featHandle(*m_CDS), m_Feat.GetLocation());
    if (NStr::Equal(key, "sig_peptide") && in_frame == feature::eLocationInFrame_NotIn) {
        return;
    }
    switch (in_frame) {
        case feature::eLocationInFrame_NotIn:
            if (NStr::Equal(key, "sig_peptide")) {
                // ignore
            } else {
                PostErr(eDiag_Warning, eErr_SEQ_FEAT_PeptideFeatOutOfFrame,
                    "Start and stop of " + key + " are out of frame with CDS codons");
            }
            break;
        case feature::eLocationInFrame_BadStartAndStop:
            PostErr(eDiag_Warning, eErr_SEQ_FEAT_PeptideFeatOutOfFrame,
                "Start and stop of " + key + " are out of frame with CDS codons");
            break;
        case feature::eLocationInFrame_BadStart:
            PostErr(eDiag_Warning, eErr_SEQ_FEAT_PeptideFeatOutOfFrame, 
                "Start of " + key + " is out of frame with CDS codons");
            break;
        case feature::eLocationInFrame_BadStop:
            PostErr(eDiag_Warning, eErr_SEQ_FEAT_PeptideFeatOutOfFrame,
                "Stop of " + key + " is out of frame with CDS codons");
            break;
        case feature::eLocationInFrame_InFrame:
            break;
    }    
}


void CExonValidator::Validate()
{
    CSingleFeatValidator::Validate();
    bool feat_pseudo = s_IsPseudo(m_Feat);
    bool pseudo = feat_pseudo;
    if (!pseudo) {
        CConstRef<CSeq_feat> gene = m_Imp.GetGeneCache().GetGeneFromCache(&m_Feat, m_Scope);
        if (gene) {
            pseudo = s_IsPseudo(*gene);
        }
    }
    if (m_Imp.IsValidateExons()) {
        ValidateSplice(pseudo, true);
    }
}


void CIntronValidator::Validate()
{
    CSingleFeatValidator::Validate();
    bool feat_pseudo = s_IsPseudo(m_Feat);
    bool pseudo = feat_pseudo;
    if (!pseudo) {
        CConstRef<CSeq_feat> gene = m_Imp.GetGeneCache().GetGeneFromCache(&m_Feat, m_Scope);
        if (gene) {
            pseudo = s_IsPseudo(*gene);
        }
    }

    if (x_IsIntronShort(pseudo)) {
        PostErr (eDiag_Warning, eErr_SEQ_FEAT_ShortIntron,
                 "Introns should be at least 10 nt long");
    }

    if (m_Feat.IsSetExcept() && m_Feat.IsSetExcept_text()
        && NStr::FindNoCase (m_Feat.GetExcept_text(), "nonconsensus splice site") != string::npos) {
        return;
    }

    const CSeq_loc& loc = m_Feat.GetLocation();

    bool partial5 = loc.IsPartialStart(eExtreme_Biological);
    bool partial3 = loc.IsPartialStop(eExtreme_Biological);
    if (partial5 && partial3) {
        return;
    }

    // suppress if contained by rRNA - different consensus splice site
    TFeatScores scores;
    GetOverlappingFeatures(loc,
                           CSeqFeatData::e_Rna,
                           CSeqFeatData::eSubtype_rRNA,
                           eOverlap_Contained,
                           scores, m_Scope);
    if (scores.size() > 0) {
        return;
    }

    // suppress if contained by tRNA - different consensus splice site
    scores.clear();
    GetOverlappingFeatures(loc,
                           CSeqFeatData::e_Rna,
                           CSeqFeatData::eSubtype_tRNA,
                           eOverlap_Contained,
                           scores, m_Scope);
    if (scores.size() > 0) {
        return;
    }

    // skip if more than one bioseq
    if (!IsOneBioseq(loc, &m_Scope)) {
        return;
    }

    // skip if organelle
    if (!m_LocationBioseq || IsOrganelle(m_LocationBioseq)) {
        return;
    }

    TSeqPos seq_len = m_LocationBioseq.GetBioseqLength();
    string label;
    m_LocationBioseq.GetId().front().GetSeqId()->GetLabel(&label);
    CSeqVector vec = m_LocationBioseq.GetSeqVector (CBioseq_Handle::eCoding_Iupac);

    ENa_strand strand = loc.GetStrand();

    if (eNa_strand_minus != strand && eNa_strand_plus != strand) {
        strand = eNa_strand_plus;
    }

    bool donor_in_gap = false;
    bool acceptor_in_gap = false;

    TSeqPos end5 = loc.GetStart (eExtreme_Biological);
    if (vec.IsInGap(end5)) {
        donor_in_gap = true;
    }

    TSeqPos end3 = loc.GetStop (eExtreme_Biological);
    if (vec.IsInGap(end3)) {
        acceptor_in_gap = true;
    }
    
    if (!partial5 && !partial3) {    
        if (donor_in_gap && acceptor_in_gap) {
            return;
        }
    }

    Char donor[2];  // donor site signature
    Char acceptor[2];  // acceptor site signature
    bool donor_good = false;  // flag == "true" indicates that donor signature is in @donor
    bool acceptor_good = false;  // flag == "true" indicates that acceptor signature is in @acceptor

    // Read donor signature into @donor
    if (!partial5 && !donor_in_gap) {
        if (eNa_strand_minus == strand) {
            if (end5 > 0 && IsResidue (vec[end5 - 1]) && IsResidue (vec[end5])) {
                donor[0] = vec[end5 - 1];
                donor[1] = vec[end5];
                donor_good = true;
            }
        }
        else {
            if( end5 < seq_len - 1 && IsResidue (vec[end5]) && IsResidue (vec[end5 + 1])) {
                donor[0] = vec[end5];
                donor[1] = vec[end5 + 1];
                donor_good = true;
            }
        }
    }

    // Read acceptor signature into @acceptor
    if (!partial3 && !acceptor_in_gap) {
        if (eNa_strand_minus == strand) {
            if (end3 <  seq_len - 1 && IsResidue (vec[end3]) && IsResidue (vec[end3 + 1])) {
                acceptor[0] = vec[end3];
                acceptor[1] = vec[end3 + 1];
                acceptor_good = true;
            }
        }
        else {
            if (end3 > 0 && IsResidue (vec[end3 - 1]) && IsResidue (vec[end3])) {
                acceptor[0] = vec[end3 - 1];
                acceptor[1] = vec[end3];
                acceptor_good = true;
            }
        }
    }

    // Check intron's both ends.
    if (!partial5 && !partial3) {        
        if (donor_good && acceptor_good) {
            if (CheckIntronSpliceSites(strand, donor, acceptor)) {
                return;
            }
        }
    }

    // Check 5'-most
    if (!partial5) {
        if (!donor_in_gap) {
            bool not_found = true;
            
            if (donor_good) {
                if (CheckIntronDonor(strand, donor)) {
                    not_found = false;
                }
            }
            //
            if (not_found) {
                if ((strand == eNa_strand_minus && end5 == seq_len - 1) ||
                    (strand == eNa_strand_plus && end5 == 0)) {

                    PostErr(eDiag_Info, eErr_SEQ_FEAT_NotSpliceConsensusDonorTerminalIntron,
                            "Splice donor consensus (GT) not found at start of terminal intron, position "
                            + NStr::IntToString (end5 + 1) + " of " + label);
                }
                else {
                    PostErr(x_SeverityForConsensusSplice(), eErr_SEQ_FEAT_NotSpliceConsensusDonor,
                             "Splice donor consensus (GT) not found at start of intron, position "
                              + NStr::IntToString (end5 + 1) + " of " + label);
                }
            }
        }
    }

    // Check 3'-most
    if (!partial3) {
        if (!acceptor_in_gap) {
            bool not_found = true;
            
            if (acceptor_good) {
                if (CheckIntronAcceptor(strand, acceptor)) {
                    not_found = false;
                }
            }
            
            if (not_found) {
                if ((strand == eNa_strand_minus && end3 == 0) ||
                    (strand == eNa_strand_plus && end3 == seq_len - 1)) {
                    PostErr(eDiag_Info, eErr_SEQ_FEAT_NotSpliceConsensusAcceptorTerminalIntron,
                              "Splice acceptor consensus (AG) not found at end of terminal intron, position "
                              + NStr::IntToString (end3 + 1) + " of " + label + ", but at end of sequence");
                }
                else {
                    PostErr(x_SeverityForConsensusSplice(), eErr_SEQ_FEAT_NotSpliceConsensusAcceptor,
                             "Splice acceptor consensus (AG) not found at end of intron, position "
                              + NStr::IntToString (end3 + 1) + " of " + label);
               }
            }
        }
    }

}


bool CIntronValidator::x_IsIntronShort(bool pseudo)
{
    if (!m_Feat.IsSetData() 
        || m_Feat.GetData().GetSubtype() != CSeqFeatData::eSubtype_intron 
        || !m_Feat.IsSetLocation()
        || pseudo) {
        return false;
    }

    const CSeq_loc& loc = m_Feat.GetLocation();
    bool is_short = false;

    if (! m_Imp.IsIndexerVersion()) {
        if (!m_LocationBioseq || IsOrganelle(m_LocationBioseq)) return is_short;
    }

    if (GetLength(loc, &m_Scope) < 11) {
        bool partial_left = loc.IsPartialStart(eExtreme_Positional);
        bool partial_right = loc.IsPartialStop(eExtreme_Positional);
        
        CBioseq_Handle bsh;
        if (partial_left && loc.GetStart(eExtreme_Positional) == 0) {
            // partial at beginning of sequence, ok
        } else if (partial_right &&
            (m_LocationBioseq) &&
                   loc.GetStop(eExtreme_Positional) == (
                       m_LocationBioseq.GetBioseqLength() - 1))
        {
            // partial at end of sequence
        } else {
            is_short = true;
        }
    }
    return is_short;
}


void CMiscFeatValidator::Validate()
{
    CSingleFeatValidator::Validate();
    if ((!m_Feat.IsSetComment() || NStr::IsBlank (m_Feat.GetComment()))
            && (!m_Feat.IsSetQual() || m_Feat.GetQual().empty())
            && (!m_Feat.IsSetDbxref() || m_Feat.GetDbxref().empty())) {
        PostErr(eDiag_Warning, eErr_SEQ_FEAT_MiscFeatureNeedsNote,
                "A note or other qualifier is required for a misc_feature");
    }
    if (m_Feat.IsSetComment() && ! NStr::IsBlank (m_Feat.GetComment())) {
        if (NStr::FindWord(m_Feat.GetComment(), "cspA") != NPOS) {
            CConstRef<CSeq_feat> cds = GetBestOverlappingFeat(m_Feat.GetLocation(), CSeqFeatData::eSubtype_cdregion, eOverlap_Simple, m_Scope);
            if (cds) {
                string content_label;
                feature::GetLabel(*cds, &content_label, feature::fFGL_Content, &m_Scope);
                if (NStr::Equal(content_label, "cold-shock protein")) {
                    PostErr(eDiag_Error, eErr_SEQ_FEAT_ColdShockProteinProblem,
                            "cspA misc_feature overlapped by cold-shock protein CDS");
                }
            }
        }
    }

}


void CAssemblyGapValidator::Validate()
{
    CSingleFeatValidator::Validate();

    bool is_far_delta = false;
    if ( m_LocationBioseq.IsSetInst_Repr()) {
        CBioseq_Handle::TInst::TRepr repr = m_LocationBioseq.GetInst_Repr();
        if ( repr == CSeq_inst::eRepr_delta ) {
            is_far_delta = true;
            const CBioseq& seq = *(m_LocationBioseq.GetCompleteBioseq());
            const CSeq_inst& inst = seq.GetInst();
            ITERATE(CDelta_ext::Tdata, sg, inst.GetExt().GetDelta().Get()) {
                if ( !(*sg) ) continue;
                if (( (**sg).Which() == CDelta_seq::e_Loc )) continue;
                is_far_delta = false;
            }
        }
    }
    if (! is_far_delta) {
        PostErr(eDiag_Warning, eErr_SEQ_FEAT_AssemblyGapFeatureProblem,
                "An assembly_gap feature should only be on a contig record");
    }
    if (m_Feat.GetLocation().IsInt()) {
        TSeqPos from = m_Feat.GetLocation().GetInt().GetFrom();
        TSeqPos to = m_Feat.GetLocation().GetInt().GetTo();
        CSeqVector vec = m_LocationBioseq.GetSeqVector(CBioseq_Handle::eCoding_Iupac);
        string sequence;
        bool is5 = false;
        bool is3 = false;
        long int count = 0;
        vec.GetSeqData(from - 1, from, sequence);
        if (NStr::Equal (sequence, "N")) {
            is5 = true;
        }
        vec.GetSeqData(to + 1, to + 2, sequence);
        if (NStr::Equal (sequence, "N")) {
            is3 = true;
        }
        EDiagSev sv = eDiag_Warning;
        if (m_Imp.IsGenomeSubmission()) {
            sv = eDiag_Error;
        }
        if (is5 && is3) {
            PostErr(sv, eErr_SEQ_FEAT_AssemblyGapAdjacentToNs,
                    "Assembly_gap flanked by Ns on 5' and 3' sides");
        } else if (is5) {
            PostErr(sv, eErr_SEQ_FEAT_AssemblyGapAdjacentToNs,
                    "Assembly_gap flanked by Ns on 5' side");
        } else if (is3) {
            PostErr(sv, eErr_SEQ_FEAT_AssemblyGapAdjacentToNs,
                    "Assembly_gap flanked by Ns on 3' side");
        }
        vec.GetSeqData(from, to + 1, sequence);
        for (size_t i = 0; i < sequence.size(); i++) {
            if (sequence[i] != 'N') {
                count++;
            }
        }
        if (count > 0) {
            PostErr(eDiag_Error, eErr_SEQ_FEAT_AssemblyGapCoversSequence, "Assembly_gap extends into sequence");
        }
    }
}


void CGapFeatValidator::Validate()
{
    CSingleFeatValidator::Validate();
    int loc_len = GetLength (m_Feat.GetLocation(), &m_Scope);
    // look for estimated length qualifier
    FOR_EACH_GBQUAL_ON_SEQFEAT (it, m_Feat) {
        if ((*it)->IsSetQual() && NStr::EqualNocase ((*it)->GetQual(), "estimated_length")
            && (*it)->IsSetVal() && !NStr::EqualNocase ((*it)->GetVal(), "unknown")) {
            try {
                int estimated_length = NStr::StringToInt ((*it)->GetVal());
                if (estimated_length != loc_len) {
                    PostErr (eDiag_Error, eErr_SEQ_FEAT_GapFeatureProblem, 
                             "Gap feature estimated_length " + NStr::IntToString (estimated_length)
                             + " does not match " + NStr::IntToString (loc_len) + " feature length");
                    return;
                }
            } catch (CException ) {
            } catch (std::exception ) {
            }
        }
    }
    try {
        string s_data = GetSequenceStringFromLoc(m_Feat.GetLocation(), m_Scope);
        CSeqVector vec = GetSequenceFromFeature(m_Feat, m_Scope);
        if ( !vec.empty() ) {
            string vec_data;
            vec.GetSeqData(0, vec.size(), vec_data);
            int num_n = 0;
            int num_real = 0;
            unsigned int num_gap = 0;
            int pos = 0;
            string::iterator it = vec_data.begin();
            while (it != vec_data.end()) {
                if (*it == 'N') {
                    if (vec.IsInGap(pos)) {
                        // gap not N
                        num_gap++;
                    } else {
                        num_n++;
                    }
                } else if (*it != '-') {
                    num_real++;
                }
                ++it;
                ++pos;
            }
            if (num_real > 0 && num_n > 0) {
                PostErr (eDiag_Error, eErr_SEQ_FEAT_GapFeatureProblem, 
                         "Gap feature over " + NStr::IntToString (num_real)
                         + " real bases and " + NStr::IntToString (num_n)
                         + " Ns");
            } else if (num_real > 0) {
                PostErr (eDiag_Error, eErr_SEQ_FEAT_GapFeatureProblem, 
                         "Gap feature over " + NStr::IntToString (num_real)
                         + " real bases");
            } else if (num_n > 0) {
                PostErr (eDiag_Error, eErr_SEQ_FEAT_GapFeatureProblem, 
                         "Gap feature over " + NStr::IntToString (num_n)
                         + " Ns");
            } else if (num_gap != GetLength (m_Feat.GetLocation(), &m_Scope)) {
                PostErr (eDiag_Error, eErr_SEQ_FEAT_GapFeatureProblem, 
                         "Gap feature estimated_length " + NStr::IntToString (loc_len)
                         + " does not match " + NStr::IntToString (num_gap)
                         + " gap characters");
            }
        }

    } catch (CException ) {
    } catch (std::exception ) {
    }
}


void CImpFeatValidator::Validate()
{
    CSingleFeatValidator::Validate();
    CSeqFeatData::ESubtype subtype = m_Feat.GetData().GetSubtype();

    const string& key = m_Feat.GetData().GetImp().GetKey();
    if (NStr::IsBlank(key)) {
        PostErr(eDiag_Warning, eErr_SEQ_FEAT_UnknownImpFeatKey, 
                "NULL feature key");
        return;
    }

    if (subtype == CSeqFeatData::eSubtype_imp || subtype == CSeqFeatData::eSubtype_bad) {
        if (NStr::Equal(key, "mRNA")) {
            subtype = CSeqFeatData::eSubtype_mRNA;
        } else if (NStr::Equal(key, "tRNA")) {
            subtype = CSeqFeatData::eSubtype_tRNA;
        } else if (NStr::Equal(key, "tRNA")) {
            subtype = CSeqFeatData::eSubtype_tRNA;
        } else if (NStr::Equal(key, "rRNA")) {
            subtype = CSeqFeatData::eSubtype_rRNA;
        } else if (NStr::Equal(key, "snRNA")) {
            subtype = CSeqFeatData::eSubtype_snRNA;
        } else if (NStr::Equal(key, "scRNA")) {
            subtype = CSeqFeatData::eSubtype_scRNA;
        } else if (NStr::Equal(key, "snoRNA")) {
            subtype = CSeqFeatData::eSubtype_snoRNA;
        } else if (NStr::Equal(key, "misc_RNA")) {
            subtype = CSeqFeatData::eSubtype_misc_RNA;
        } else if (NStr::Equal(key, "precursor_RNA")) {
            subtype = CSeqFeatData::eSubtype_precursor_RNA;
        } else if (NStr::EqualNocase (key, "mat_peptide")) {
            subtype = CSeqFeatData::eSubtype_mat_peptide;
        } else if (NStr::EqualNocase (key, "propeptide")) {
            subtype = CSeqFeatData::eSubtype_propeptide;
        } else if (NStr::EqualNocase (key, "sig_peptide")) {
            subtype = CSeqFeatData::eSubtype_sig_peptide;
        } else if (NStr::EqualNocase (key, "transit_peptide")) {
            subtype = CSeqFeatData::eSubtype_transit_peptide;
        } else if (NStr::EqualNocase (key, "preprotein")
            || NStr::EqualNocase(key, "proprotein")) {
            subtype = CSeqFeatData::eSubtype_preprotein;
        } else if (NStr::EqualNocase (key, "virion")) {
            subtype = CSeqFeatData::eSubtype_virion;
        } else if (NStr::EqualNocase(key, "mutation")) {
            subtype = CSeqFeatData::eSubtype_mutation;
        } else if (NStr::EqualNocase(key, "allele")) {
            subtype = CSeqFeatData::eSubtype_allele;
        } else if (NStr::EqualNocase (key, "CDS")) {
            subtype = CSeqFeatData::eSubtype_Imp_CDS;
        } else if (NStr::EqualNocase(key, "Import")) {
            PostErr(eDiag_Error, eErr_SEQ_FEAT_UnknownImpFeatKey,
                    "Feature key Import is no longer legal");
            return;
        }
    }

    switch ( subtype ) {

    case CSeqFeatData::eSubtype_bad:
    case CSeqFeatData::eSubtype_site_ref:
    case CSeqFeatData::eSubtype_gene:
        PostErr(eDiag_Warning, eErr_SEQ_FEAT_UnknownImpFeatKey, 
            "Unknown feature key " + key);
        break;

    case CSeqFeatData::eSubtype_virion:
    case CSeqFeatData::eSubtype_mutation:
    case CSeqFeatData::eSubtype_allele:
        PostErr(eDiag_Error, eErr_SEQ_FEAT_UnknownImpFeatKey,
            "Feature key " + key + " is no longer legal");
        break;


    case CSeqFeatData::eSubtype_preprotein:
        if (m_Imp.IsEmbl() || m_Imp.IsDdbj()) {
            const CSeq_loc& loc = m_Feat.GetLocation();
            CConstRef<CSeq_feat> cds = GetOverlappingCDS(loc, m_Scope);
            PostErr(!cds ? eDiag_Error : eDiag_Warning, eErr_SEQ_FEAT_PeptideFeatureLacksCDS,
                "Pre/pro protein feature cannot be associated with a "
                "protein product of a coding region feature");
        } else {
            PostErr(m_Imp.IsRefSeq() ? eDiag_Error : eDiag_Warning, eErr_SEQ_FEAT_PeptideFeatureLacksCDS,
                "Peptide processing feature should be converted to the "
                "appropriate protein feature subtype");
        }
        break;
        
    case CSeqFeatData::eSubtype_mRNA:
    case CSeqFeatData::eSubtype_tRNA:
    case CSeqFeatData::eSubtype_rRNA:
    case CSeqFeatData::eSubtype_snRNA:
    case CSeqFeatData::eSubtype_scRNA:
    case CSeqFeatData::eSubtype_snoRNA:
    case CSeqFeatData::eSubtype_misc_RNA:
    case CSeqFeatData::eSubtype_precursor_RNA:
    // !!! what about other RNA types (preRNA, otherRNA)?
        PostErr(eDiag_Error, eErr_SEQ_FEAT_InvalidRNAFeature,
              "RNA feature should be converted to the appropriate RNA feature "
              "subtype, location should be converted manually");
        break;

    case CSeqFeatData::eSubtype_Imp_CDS:
        {
            // impfeat CDS must be pseudo; fail if not
            bool pseudo = sequence::IsPseudo(m_Feat, m_Scope);
            if ( !pseudo ) {
                PostErr(eDiag_Info, eErr_SEQ_FEAT_ImpCDSnotPseudo,
                    "ImpFeat CDS should be pseudo");
            }

            FOR_EACH_GBQUAL_ON_FEATURE (gbqual, m_Feat) {
                if ( NStr::CompareNocase( (*gbqual)->GetQual(), "translation") == 0 ) {
                    PostErr(eDiag_Error, eErr_SEQ_FEAT_ImpCDShasTranslation,
                        "ImpFeat CDS with /translation found");
                }
            }
        }
        break;
    case CSeqFeatData::eSubtype_imp:
        PostErr(eDiag_Warning, eErr_SEQ_FEAT_UnknownImpFeatKey,
            "Unknown feature key " + key);
        break;
    case CSeqFeatData::eSubtype_repeat_region:
        if ((!m_Feat.IsSetComment() || NStr::IsBlank (m_Feat.GetComment()))
            && (!m_Feat.IsSetDbxref() || m_Feat.GetDbxref().empty())) {
            if (!m_Feat.IsSetQual() || m_Feat.GetQual().empty()) {
               PostErr(eDiag_Warning, eErr_SEQ_FEAT_RepeatRegionNeedsNote,
                        "repeat_region has no qualifiers");
            } else if ( ! m_Imp.IsGenomeSubmission() ) {
                bool okay = false;
                FOR_EACH_GBQUAL_ON_FEATURE (gbqual, m_Feat) {
                    if ( ! NStr::EqualNocase((*gbqual)->GetQual(), "rpt_type") ) {
                        okay = true;
                        break;
                    }
                    const string& val = (*gbqual)->GetVal();
                    if ( ! NStr::Equal (val, "other") ) {
                        okay = true;
                        break;
                    }
                }
                if ( ! okay ) {
                   PostErr(eDiag_Warning, eErr_SEQ_FEAT_RepeatRegionNeedsNote,
                            "repeat_region has no qualifiers except rpt_type other");
                }
            }
         }
        break;
    case CSeqFeatData::eSubtype_regulatory:
        {
            vector<string> valid_types = CSeqFeatData::GetRegulatoryClassList();
            FOR_EACH_GBQUAL_ON_FEATURE (gbqual, m_Feat) {
                if ( NStr::CompareNocase( (*gbqual)->GetQual(), "regulatory_class") != 0 ) continue;
                const string& val = (*gbqual)->GetVal();
                bool missing = true;
                FOR_EACH_STRING_IN_VECTOR (itr, valid_types) {
                    string str = *itr;
                    if ( NStr::Equal (str, val) ) {
                        missing = false;
                    }
                }
                if ( missing ) {
                    if ( NStr::Equal (val, "other") && !m_Feat.IsSetComment() ) {
                        PostErr(eDiag_Error, eErr_SEQ_FEAT_RegulatoryClassOtherNeedsNote,
                            "The regulatory_class 'other' is missing the required /note");
                    }
                }
            }
        }
        break;
    case CSeqFeatData::eSubtype_misc_recomb:
        {
            const CGb_qual::TLegalRecombinationClassSet recomb_values = CGb_qual::GetSetOfLegalRecombinationClassValues();
            FOR_EACH_GBQUAL_ON_FEATURE (gbqual, m_Feat) {
                if ( NStr::CompareNocase( (*gbqual)->GetQual(), "recombination_class") != 0 ) continue;
                const string& val = (*gbqual)->GetVal();
               if ( recomb_values.find(val.c_str()) == recomb_values.end() ) {
                    if ( NStr::Equal (val, "other")) {
                        if (!m_Feat.IsSetComment()) {
                            PostErr(eDiag_Error, eErr_SEQ_FEAT_RecombinationClassOtherNeedsNote,
                                "The recombination_class 'other' is missing the required /note");
                        }
                    } else {
                        // Removed per VR-770. FlatFile will automatically
                        // display the unrecognized recombination_class value
                        // in the note and list the recombination_class as other
//                        PostErr(eDiag_Info, eErr_SEQ_FEAT_InvalidQualifierValue,
//                            "'" + val + "' is not a legal value for recombination_class", feat);
                    }
                }
            }
        }
        break;
    default:
        break;
    }// end of switch statement      
}


CSingleFeatValidator* FeatValidatorFactory(const CSeq_feat& feat, CScope& scope, CValidError_imp& imp)
{
    if (!feat.IsSetData()) {
        return new CSingleFeatValidator(feat, scope, imp);
    } else if (feat.GetData().IsCdregion()) {
        return new CCdregionValidator(feat, scope, imp);
    } else if (feat.GetData().IsGene()) {
        return new CGeneValidator(feat, scope, imp);
    } else if (feat.GetData().IsProt()) {
        return new CProtValidator(feat, scope, imp);
    } else if (feat.GetData().GetSubtype() == CSeqFeatData::eSubtype_mRNA) {
        return new CMRNAValidator(feat, scope, imp);
    } else if (feat.GetData().IsRna()) {
        return new CRNAValidator(feat, scope, imp);
    } else if (feat.GetData().IsPub()) {
        return new CPubFeatValidator(feat, scope, imp);
    } else if (feat.GetData().IsBiosrc()) {
        return new CSrcFeatValidator(feat, scope, imp);
    } else if (feat.GetData().GetSubtype() == CSeqFeatData::eSubtype_exon) {
        return new CExonValidator(feat, scope, imp);
    } else if (feat.GetData().GetSubtype() == CSeqFeatData::eSubtype_intron) {
        return new CIntronValidator(feat, scope, imp);
    } else if (feat.GetData().GetSubtype() == CSeqFeatData::eSubtype_misc_feature) {
        return new CMiscFeatValidator(feat, scope, imp);
    } else if (feat.GetData().GetSubtype() == CSeqFeatData::eSubtype_assembly_gap) {
        return new CAssemblyGapValidator(feat, scope, imp);
    } else if (feat.GetData().GetSubtype() == CSeqFeatData::eSubtype_polyA_site) {
        return new CPolyASiteValidator(feat, scope, imp);
    }
    else if (feat.GetData().GetSubtype() == CSeqFeatData::eSubtype_polyA_signal) {
        return new CPolyASignalValidator(feat, scope, imp);
    } else if (feat.GetData().IsImp()) {
        CSeqFeatData::ESubtype subtype = feat.GetData().GetSubtype();
        switch (subtype) {
            case CSeqFeatData::eSubtype_mat_peptide:
            case CSeqFeatData::eSubtype_propeptide:
            case CSeqFeatData::eSubtype_sig_peptide:
            case CSeqFeatData::eSubtype_transit_peptide:
                return new CPeptideValidator(feat, scope, imp);
                break;
            case CSeqFeatData::eSubtype_gap:
                return new CGapFeatValidator(feat, scope, imp);
                break;
            default:
                return new CImpFeatValidator(feat, scope, imp);
                break;
        }
    } else {
        return new CSingleFeatValidator(feat, scope, imp);
    }
}




END_SCOPE(validator)
END_SCOPE(objects)
END_NCBI_SCOPE
