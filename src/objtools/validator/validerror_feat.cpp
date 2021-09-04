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
#include <objtools/validator/validerror_feat.hpp>
#include <objtools/validator/single_feat_validator.hpp>
#include <objtools/validator/utilities.hpp>
#include <objtools/format/items/gene_finder.hpp>

#include <serial/serialbase.hpp>

#include <objmgr/bioseq_handle.hpp>
#include <objmgr/seq_entry_handle.hpp>
#include <objmgr/feat_ci.hpp>
#include <objmgr/seqdesc_ci.hpp>
#include <objmgr/seq_vector.hpp>
#include <objmgr/scope.hpp>
#include <objmgr/util/sequence.hpp>
#include <objmgr/util/feature.hpp>

#include <objects/seqfeat/Seq_feat.hpp>
#include <objects/seqfeat/BioSource.hpp>
#include <objects/seqfeat/Cdregion.hpp>
#include <objects/seqfeat/Code_break.hpp>
#include <objects/seqfeat/Gb_qual.hpp>
#include <objects/seqfeat/Genetic_code.hpp>
#include <objects/seqfeat/Genetic_code_table.hpp>
#include <objects/seqfeat/Imp_feat.hpp>
#include <objects/seqfeat/Org_ref.hpp>
#include <objects/seqfeat/Prot_ref.hpp>
#include <objects/seqfeat/RNA_ref.hpp>
#include <objects/seqfeat/SubSource.hpp>
#include <objects/seqfeat/Trna_ext.hpp>

#include <objects/seqloc/Seq_loc.hpp>
#include <objects/seqloc/Seq_interval.hpp>
#include <objects/seqloc/Seq_point.hpp>
#include <objects/seqloc/Textseq_id.hpp>

#include <objects/seqset/Seq_entry.hpp>
#include <objects/seqset/Bioseq_set.hpp>

#include <objects/seq/MolInfo.hpp>
#include <objects/seq/Bioseq.hpp>
#include <objects/seq/seqport_util.hpp>

#include <objects/pub/Pub.hpp>
#include <objects/pub/Pub_set.hpp>

#include <objects/general/Dbtag.hpp>

#include <objects/misc/sequence_macros.hpp>

#include <objtools/edit/cds_fix.hpp>

#include <util/static_set.hpp>
#include <util/sequtil/sequtil_convert.hpp>
#include <util/sgml_entity.hpp>

#include <algorithm>
#include <string>


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)
BEGIN_SCOPE(validator)
using namespace sequence;


// =============================================================================
//                                     Public
// =============================================================================


CValidError_feat::CValidError_feat(CValidError_imp& imp) :
    CValidError_base(imp)
{
}


CValidError_feat::~CValidError_feat()
{
}


void CValidError_feat::SetTSE(CSeq_entry_Handle seh)
{
    if (!m_TSE || m_Imp.ShouldSubdivide()) {
        m_GeneCache.Clear();
        m_SeqCache.Clear();
        m_TSE = seh;
    }
}


CBioseq_Handle CValidError_feat::x_GetCachedBsh(const CSeq_loc& loc)
{
    CBioseq_Handle empty;
    if (!m_TSE) {
        return empty;
    }
    return m_SeqCache.GetBioseqHandleFromLocation(
        m_Scope, loc, m_TSE.GetTSE_Handle());
}

void CValidError_feat::x_ValidateSeqFeatExceptXref(const CSeq_feat& feat)
{
    try {

//        ValidateSeqFeatData(feat.GetData(), feat);

        unique_ptr<CSingleFeatValidator> fval(FeatValidatorFactory(feat, *m_Scope, m_Imp));
        if (fval) {
            fval->Validate();
        }

    } catch (const exception& e) {
        PostErr(eDiag_Fatal, eErr_INTERNAL_Exception,
            string("Exception while validating feature. EXCEPTION: ") +
            e.what(), feat);
    }
}


void CValidError_feat::ValidateSeqFeat(
    const CSeq_feat& feat)
{
    x_ValidateSeqFeatExceptXref(feat);

    ValidateSeqFeatXref(feat);

}


bool CValidError_feat::GetTSACDSOnMinusStrandErrors(const CSeq_feat& feat, const CBioseq& seq)
{
    bool rval = false;
    // check for CDSonMinusStrandTranscribedRNA
    if (feat.IsSetData()
        && feat.GetData().IsCdregion()
        && feat.IsSetLocation()
        && feat.GetLocation().GetStrand() == eNa_strand_minus) {
        CBioseq_Handle bsh = m_Scope->GetBioseqHandle(seq);
        if ( bsh ) {
            CSeqdesc_CI di(bsh, CSeqdesc::e_Molinfo);
            if (di
                && di->GetMolinfo().IsSetTech()
                && di->GetMolinfo().GetTech() == CMolInfo::eTech_tsa
                && di->GetMolinfo().IsSetBiomol()
                && di->GetMolinfo().GetBiomol() == CMolInfo::eBiomol_transcribed_RNA) {
                PostErr(eDiag_Warning, eErr_SEQ_FEAT_CDSonMinusStrandTranscribedRNA,
                        "Coding region on TSA transcribed RNA should not be on the minus strand", feat);
                rval = true;
            }
        }
    }
    return rval;
}


void CValidError_feat::ValidateSeqFeatContext(const CSeq_feat& feat, const CBioseq& seq)
{
    CSeqFeatData::E_Choice ftype = feat.GetData().Which();

    if (seq.IsAa()) {
        // protein
        switch (ftype) {
            case CSeqFeatData::e_Cdregion:
            case CSeqFeatData::e_Rna:
            case CSeqFeatData::e_Rsite:
            case CSeqFeatData::e_Txinit:
                PostErr(eDiag_Error, eErr_SEQ_FEAT_InvalidFeatureForProtein,
                    "Invalid feature for a protein Bioseq.", feat);
                break;
            default:
                break;
        }
    } else {
        // nucleotide
        if (ftype == CSeqFeatData::e_Prot || ftype == CSeqFeatData::e_Psec_str) {
            PostErr(eDiag_Error, eErr_SEQ_FEAT_InvalidFeatureForNucleotide,
                    "Invalid feature for a nucleotide Bioseq.", feat);
        }
        if (feat.IsSetData() && feat.GetData().IsProt() && feat.GetData().GetProt().IsSetProcessed()) {
            CProt_ref::TProcessed processed = feat.GetData().GetProt().GetProcessed();
            if (processed == CProt_ref::eProcessed_mature
                || processed == CProt_ref::eProcessed_signal_peptide
                || processed == CProt_ref::eProcessed_transit_peptide
                || processed == CProt_ref::eProcessed_preprotein) {
                PostErr (m_Imp.IsRefSeq() ? eDiag_Error : eDiag_Warning,
                         eErr_SEQ_FEAT_InvalidForType,
                         "Peptide processing feature should be remapped to the appropriate protein bioseq",
                         feat);
            }
        }
    }

    // check for CDSonMinusStrandTranscribedRNA
    GetTSACDSOnMinusStrandErrors(feat, seq);
}


// =============================================================================
//                                     Private
// =============================================================================


// private member functions:

//#define TEST_LONGTIME

// in Ncbistdaa order
/*
Return values are:
 0: no problem - Accession is in proper format
-1: Accession did not start with a letter (or two letters)
-2: Accession did not contain five numbers (or six numbers after 2 letters)
-3: the original Accession number to be validated was NULL
-4: the original Accession number is too long (>16)
*/

static int ValidateAccessionFormat (string accession)
{
    if (CSeq_id::IdentifyAccession(accession) == CSeq_id::eAcc_unknown) {
        return -1;
    } else {
        return 0;
    }
}


bool CValidError_feat::GetPrefixAndAccessionFromInferenceAccession (string inf_accession, string &prefix, string &accession)
{
    size_t pos1 = NStr::Find (inf_accession, ":");
    size_t pos2 = NStr::Find (inf_accession, "|");
    size_t pos = string::npos;

    if (pos1 < pos2) {
      pos = pos1;
    } else {
      pos = pos2;
    }
    if (pos == string::npos) {
        return false;
    } else {
        prefix = inf_accession.substr(0, pos);
        NStr::TruncateSpacesInPlace (prefix);
        accession = inf_accession.substr(pos + 1);
        NStr::TruncateSpacesInPlace (accession);
        return true;
    }
}


bool s_IsSraPrefix (string str)

{
    if (str.length() < 3) {
        return false;
    }
    char ch = str.c_str()[0];
    if (ch != 'S' && ch != 'E' && ch != 'D') return false;
    ch = str.c_str()[1];
    if (ch != 'R') return false;
    ch = str.c_str()[2];
    if (ch != 'A' && ch != 'P' && ch != 'X' && ch != 'R' && ch != 'S' && ch != 'Z') return false;
    return true;
}


bool s_IsAllDigitsOrPeriods (string str)

{
    if (NStr::IsBlank(str) || NStr::StartsWith(str, ".") || NStr::EndsWith(str, ".")) {
        return false;
    }
    bool rval = true;
        ITERATE(string, it, str) {
        if (!isdigit(*it) && *it != '.') {
            rval = false;
            break;
        }
    }
    return rval;
}


CValidError_feat::EInferenceValidCode CValidError_feat::ValidateInferenceAccession (string accession, bool fetch_accession, bool is_similar_to)
{
    if (NStr::IsBlank (accession)) {
        return eInferenceValidCode_empty;
    }

    CValidError_feat::EInferenceValidCode rsult = eInferenceValidCode_valid;

    string prefix, remainder;
    if (GetPrefixAndAccessionFromInferenceAccession (accession, prefix, remainder)) {
        bool is_insd = false, is_refseq = false, is_blast = false;

        if (NStr::EqualNocase (prefix, "INSD")) {
            is_insd = true;
        } else if (NStr::EqualNocase (prefix, "RefSeq")) {
            is_refseq = true;
        } else if (NStr::StartsWith (prefix, "BLAST", NStr::eNocase)) {
            is_blast = true;
        }
        if (is_insd || is_refseq) {
            if (remainder.length() > 3) {
                if (remainder.c_str()[2] == '_') {
                    if (is_insd) {
                        rsult = eInferenceValidCode_bad_accession_type;
                    }
                } else {
                    if (is_refseq) {
                        rsult = eInferenceValidCode_bad_accession_type;
                    }
                }
            }
            if (s_IsSraPrefix (remainder) && s_IsAllDigitsOrPeriods(remainder.substr(3))) {
                // SRA
            } else if (NStr::StartsWith(remainder, "MAP_") && s_IsAllDigitsOrPeriods(remainder.substr(4))) {
            } else {
                string ver;
                int acc_code = ValidateAccessionFormat (remainder);
                if (acc_code == 0) {
                    //-5: missing version number
                    //-6: bad version number
                    size_t dot_pos = NStr::Find (remainder, ".");
                    if (dot_pos == string::npos || NStr::IsBlank(remainder.substr(dot_pos + 1))) {
                        acc_code = -5;
                    } else {
                        const string& cps = remainder.substr(dot_pos + 1);
                        const char *cp = cps.c_str();
                        while (*cp != 0 && isdigit (*cp)) {
                            ++cp;
                        }
                        if (*cp != 0) {
                            acc_code = -6;
                        }
                    }
                }

                if (acc_code == -5 || acc_code == -6) {
                    rsult = eInferenceValidCode_bad_accession_version;
                } else if (acc_code != 0) {
                    rsult = eInferenceValidCode_bad_accession;
                } else if (fetch_accession) {
                    // Test to see if accession is public
                    if (!IsSequenceFetchable(remainder)) {
                        rsult = eInferenceValidCode_accession_version_not_public;
                    }
                }
            }
        } else if (is_blast && is_similar_to) {
            rsult = eInferenceValidCode_bad_accession_type;
        } else if (is_similar_to) {
            if (CGb_qual::IsLegalInferenceDatabase(prefix)) {
                // recognized database
            } else {
                rsult = eInferenceValidCode_unrecognized_database;
            }
        }
        if (NStr::Find (remainder, " ") != string::npos) {
            rsult = eInferenceValidCode_spaces;
        }
    } else {
        rsult = eInferenceValidCode_single_field;
    }

    return rsult;
}


vector<string> CValidError_feat::GetAccessionsFromInferenceString (string inference, string &prefix, string &remainder, bool &same_species)
{
    vector<string> accessions;

    accessions.clear();
    CInferencePrefixList::GetPrefixAndRemainder (inference, prefix, remainder);
    if (NStr::IsBlank (prefix)) {
        return accessions;
    }

    same_species = false;

    if (NStr::StartsWith (remainder, "(same species)", NStr::eNocase)) {
        same_species = true;
        remainder = remainder.substr(14);
        NStr::TruncateSpacesInPlace (remainder);
    }

    if (NStr::StartsWith (remainder, ":")) {
        remainder = remainder.substr (1);
        NStr::TruncateSpacesInPlace (remainder);
    } else if (NStr::IsBlank (remainder)) {
        return accessions;
    } else {
        prefix = "";
    }

    if (NStr::IsBlank (remainder)) {
        return accessions;
    }

    if (NStr::Equal(prefix, "similar to sequence")
        || NStr::Equal(prefix, "similar to AA sequence")
        || NStr::Equal(prefix, "similar to DNA sequence")
        || NStr::Equal(prefix, "similar to RNA sequence")
        || NStr::Equal(prefix, "similar to RNA sequence, mRNA")
        || NStr::Equal(prefix, "similar to RNA sequence, EST")
        || NStr::Equal(prefix, "similar to RNA sequence, other RNA")) {
        NStr::Split(remainder, ",", accessions, 0);
    } else if (NStr::Equal(prefix, "alignment")) {
        NStr::Split(remainder, ",", accessions, 0);
    }
    return accessions;
}


CValidError_feat::EInferenceValidCode CValidError_feat::ValidateInference(string inference, bool fetch_accession)
{
    if (NStr::IsBlank (inference)) {
        return eInferenceValidCode_empty;
    }

    string prefix, remainder;
    bool same_species = false;

    vector<string> accessions = GetAccessionsFromInferenceString (inference, prefix, remainder, same_species);

    if (NStr::IsBlank (prefix)) {
        return eInferenceValidCode_bad_prefix;
    }

    if (NStr::IsBlank (remainder)) {
        return eInferenceValidCode_bad_body;
    }

    CValidError_feat::EInferenceValidCode rsult = eInferenceValidCode_valid;
    bool is_similar_to = NStr::StartsWith (prefix, "similar to");
    if (same_species && !is_similar_to) {
        rsult = eInferenceValidCode_same_species_misused;
    }

    if (rsult == eInferenceValidCode_valid) {
        for (size_t i = 0; i < accessions.size(); i++) {
            NStr::TruncateSpacesInPlace (accessions[i]);
            rsult = ValidateInferenceAccession (accessions[i], fetch_accession, is_similar_to);
            if (rsult != eInferenceValidCode_valid) {
                break;
            }
        }
    }
    if (rsult == eInferenceValidCode_valid) {
        int num_spaces = 0;
        FOR_EACH_CHAR_IN_STRING(str_itr, remainder) {
            const char& ch = *str_itr;
            if (ch == ' ') {
                num_spaces++;
            }
        }
        if (num_spaces > 3) {
            rsult = eInferenceValidCode_comment;
        } else if (num_spaces > 0){
            rsult = eInferenceValidCode_spaces;
        }
    }
    return rsult;
}


//LCOV_EXCL_START
//not used by asn_validate but may be needed by other applications
bool CValidError_feat::DoesCDSHaveShortIntrons(const CSeq_feat& feat)
{
    if (!feat.IsSetData() || !feat.GetData().IsCdregion()
        || !feat.IsSetLocation()) {
        return false;
    }

    const CSeq_loc& loc = feat.GetLocation();
    bool found_short = false;

    CSeq_loc_CI li(loc);

    TSeqPos last_start = li.GetRange().GetFrom();
    TSeqPos last_stop = li.GetRange().GetTo();
    CRef<CSeq_id> last_id(new CSeq_id());
    last_id->Assign(li.GetSeq_id());

    ++li;
    while (li && !found_short) {
        TSeqPos this_start = li.GetRange().GetFrom();
        TSeqPos this_stop = li.GetRange().GetTo();
        if (abs ((int)this_start - (int)last_stop) < 11 || abs ((int)this_stop - (int)last_start) < 11) {
            if (li.GetSeq_id().Equals(*last_id)) {
                // definitely same bioseq, definitely report
                found_short = true;
            } else if (m_Scope) {
                // only report if definitely on same bioseq
                CBioseq_Handle last_bsh = m_Scope->GetBioseqHandle(*last_id);
                if (last_bsh) {
                    for (auto id_it : last_bsh.GetId()) {
                        if (id_it.GetSeqId()->Equals(li.GetSeq_id())) {
                            found_short = true;
                            break;
                        }
                    }
                }
            }
        }
        last_start = this_start;
        last_stop = this_stop;
        last_id->Assign(li.GetSeq_id());
        ++li;
    }

    return found_short;
}


bool CValidError_feat::IsOverlappingGenePseudo(const CSeq_feat& feat, CScope *scope)
{
    const CGene_ref* grp = feat.GetGeneXref();
    if ( grp  && CSingleFeatValidator::s_IsPseudo(*grp)) {
        return true;
    }

    // check overlapping gene
    CConstRef<CSeq_feat> overlap = m_GeneCache.GetGeneFromCache(&feat, *m_Scope);
    if ( overlap ) {
        return CSingleFeatValidator::s_IsPseudo(*overlap);
    }

    return false;
}


bool CValidError_feat::IsIntronShort(const CSeq_feat& feat)
{
    if (!feat.IsSetData()
        || feat.GetData().GetSubtype() != CSeqFeatData::eSubtype_intron
        || !feat.IsSetLocation()
        || feat.IsSetPseudo()
        || IsOverlappingGenePseudo(feat, m_Scope)) {
        return false;
    }

    const CSeq_loc& loc = feat.GetLocation();
    bool is_short = false;

    if (! m_Imp.IsIndexerVersion()) {
        CBioseq_Handle bsh = x_GetCachedBsh(loc);
        if (!bsh || IsOrganelle(bsh)) return is_short;
    }

    if (GetLength(loc, m_Scope) < 11) {
        bool partial_left = loc.IsPartialStart(eExtreme_Positional);
        bool partial_right = loc.IsPartialStop(eExtreme_Positional);

        CBioseq_Handle bsh;
        if (partial_left && loc.GetStart(eExtreme_Positional) == 0) {
            // partial at beginning of sequence, ok
        } else if (partial_right &&
            (bsh = x_GetCachedBsh(loc)) &&
                   loc.GetStop(eExtreme_Positional) == (
                       bsh.GetBioseqLength() - 1))
        {
            // partial at end of sequence
        } else {
            is_short = true;
        }
    }
    return is_short;
}
//LCOV_EXCL_STOP


bool
FeaturePairIsTwoTypes
(const CSeq_feat& feat1,
 const CSeq_feat& feat2,
 CSeqFeatData::ESubtype subtype1,
 CSeqFeatData::ESubtype subtype2)
{
    if (!feat1.IsSetData() || !feat2.IsSetData()) {
        return false;
    } else if (feat1.GetData().GetSubtype() == subtype1 && feat2.GetData().GetSubtype() == subtype2) {
        return true;
    } else if (feat1.GetData().GetSubtype() == subtype2 && feat2.GetData().GetSubtype() == subtype1) {
        return true;
    } else {
        return false;
    }
}


bool GeneXrefConflicts(const CSeq_feat& feat, const CSeq_feat& gene)
{
    FOR_EACH_SEQFEATXREF_ON_SEQFEAT (it, feat) {
        string label;
        if ((*it)->IsSetData() && (*it)->GetData().IsGene()
             && !CSingleFeatValidator::s_GeneRefsAreEquivalent((*it)->GetData().GetGene(), gene.GetData().GetGene(), label)) {
            return true;
        }
    }
    return false;
}


// does feat have an xref to a feature other than the one specified by id with the same subtype
bool CValidError_feat::x_HasNonReciprocalXref
(const CSeq_feat& feat,
 const CFeat_id& id,
 CSeqFeatData::ESubtype subtype)
{
    if (!feat.IsSetXref()) {
        return false;
    }
    ITERATE(CSeq_feat::TXref, it, feat.GetXref()) {
        if ((*it)->IsSetId()) {
            if ((*it)->GetId().Equals(id)) {
                // match
            } else if ((*it)->GetId().IsLocal()) {
                const CTSE_Handle::TFeatureId& x_id = (*it)->GetId().GetLocal();
                CTSE_Handle::TSeq_feat_Handles far_feats = m_TSE.GetTSE_Handle().GetFeaturesWithId(subtype, x_id);
                if (!far_feats.empty()) {
                    return true;
                }
            }
        }
    }
    return false;
}



void CValidError_feat::ValidateOneFeatXrefPair(const CSeq_feat& feat, const CSeq_feat& far_feat, const CSeqFeatXref& xref)
{
    if (!feat.IsSetId()) {
        PostErr(eDiag_Warning, eErr_SEQ_FEAT_SeqFeatXrefNotReciprocal,
            "Cross-referenced feature does not link reciprocally",
            feat);
    } else if (far_feat.HasSeqFeatXref(feat.GetId())) {
        const bool is_cds_mrna = FeaturePairIsTwoTypes(feat, far_feat,
            CSeqFeatData::eSubtype_cdregion, CSeqFeatData::eSubtype_mRNA);
        const bool is_gene_mrna = FeaturePairIsTwoTypes(feat, far_feat,
            CSeqFeatData::eSubtype_gene, CSeqFeatData::eSubtype_mRNA);
        const bool is_gene_cdregion = FeaturePairIsTwoTypes(feat, far_feat,
            CSeqFeatData::eSubtype_gene, CSeqFeatData::eSubtype_cdregion);
        if (is_cds_mrna ||
            is_gene_mrna ||
            is_gene_cdregion) {
            if (feat.GetData().IsCdregion() && far_feat.GetData().GetSubtype() == CSeqFeatData::eSubtype_mRNA) {
                ECompare comp = Compare(feat.GetLocation(), far_feat.GetLocation(),
                    m_Scope, fCompareOverlapping);
                if ((comp != eContained) && (comp != eSame)) {
                    PostErr(eDiag_Warning, eErr_SEQ_FEAT_CDSmRNAXrefLocationProblem,
                        "CDS not contained within cross-referenced mRNA", feat);
                }
            }
            if (far_feat.GetData().GetSubtype() == CSeqFeatData::eSubtype_gene) {
                // make sure feature does not have conflicting gene xref
                if (GeneXrefConflicts(feat, far_feat)) {
                    PostErr(eDiag_Warning, eErr_SEQ_FEAT_SeqFeatXrefProblem,
                        "Feature gene xref does not match Feature ID cross-referenced gene feature",
                        feat);
                }
            }
        } else if (CSeqFeatData::ProhibitXref(feat.GetData().GetSubtype(), far_feat.GetData().GetSubtype())) {
            string label1 = feat.GetData().GetKey(CSeqFeatData::eVocabulary_genbank);
            string label2 = far_feat.GetData().GetKey(CSeqFeatData::eVocabulary_genbank);
            PostErr(eDiag_Error, eErr_SEQ_FEAT_SeqFeatXrefProblem,
                "Cross-references are not between CDS and mRNA pair or between a gene and a CDS or mRNA ("
                + label1 + "," + label2 + ")",
                feat);
        } else if (!CSeqFeatData::AllowXref(feat.GetData().GetSubtype(), far_feat.GetData().GetSubtype())) {
            string label1 = feat.GetData().GetKey(CSeqFeatData::eVocabulary_genbank);
            string label2 = far_feat.GetData().GetKey(CSeqFeatData::eVocabulary_genbank);
            PostErr(eDiag_Warning, eErr_SEQ_FEAT_SeqFeatXrefProblem,
                "Cross-references are not between CDS and mRNA pair or between a gene and a CDS or mRNA ("
                + label1 + "," + label2 + ")",
                feat);
        }
    } else if (x_HasNonReciprocalXref(far_feat, feat.GetId(), feat.GetData().GetSubtype())) {
        PostErr(eDiag_Warning, eErr_SEQ_FEAT_SeqFeatXrefNotReciprocal,
            "Cross-referenced feature does not link reciprocally",
            feat);
    } else {
        if (feat.GetData().IsCdregion() && far_feat.GetData().GetSubtype() == CSeqFeatData::eSubtype_mRNA) {
            ECompare comp = Compare(feat.GetLocation(), far_feat.GetLocation(),
                m_Scope, fCompareOverlapping);
            if ((comp != eContained) && (comp != eSame)) {
                PostErr(eDiag_Warning, eErr_SEQ_FEAT_CDSmRNAXrefLocationProblem,
                    "CDS not contained within cross-referenced mRNA", feat);
            }
        }
        if (far_feat.IsSetXref()) {
            PostErr(eDiag_Warning, eErr_SEQ_FEAT_SeqFeatXrefNotReciprocal,
                "Cross-referenced feature does not link reciprocally",
                feat);
        } else if (!far_feat.GetData().IsGene()) {
            PostErr(eDiag_Warning, eErr_SEQ_FEAT_SeqFeatXrefProblem,
                "Cross-referenced feature does not have its own cross-reference", feat);
        }
    }
}


bool s_HasId(const CSeq_feat& feat, const CSeqFeatXref::TId::TLocal& id)
{
    if (!feat.IsSetId() && feat.GetId().IsLocal()) {
        return false;
    }
    return feat.GetId().GetLocal().Equals(id);
}


void CValidError_feat::ValidateSeqFeatXref(const CSeq_feat& feat)
{
    if (!feat.IsSetXref()) {
        return;
    }
    for (auto it = feat.GetXref().begin(); it != feat.GetXref().end(); it++) {
        ValidateSeqFeatXref(**it, feat);
    }
}


void CValidError_feat::ValidateSeqFeatXref (const CSeqFeatXref& xref, const CSeq_feat& feat)
{
    if (!m_Imp.IsStandaloneAnnot() && !m_TSE) {
        return;
    }
    if (!xref.IsSetId() && !xref.IsSetData()) {
        PostErr (eDiag_Warning, eErr_SEQ_FEAT_SeqFeatXrefProblem,
                 "SeqFeatXref with no id or data field", feat);
    } else if (xref.IsSetId()) {
        if (xref.GetId().IsLocal()) {
            vector<CConstRef<CSeq_feat> > far_feats;
            if (m_Imp.IsStandaloneAnnot()) {
                for (auto it = m_Imp.GetSeqAnnot()->GetData().GetFtable().begin(); it != m_Imp.GetSeqAnnot()->GetData().GetFtable().end(); it++) {
                    if (s_HasId(**it, xref.GetId().GetLocal())) {
                        far_feats.push_back(*it);
                    }
                }
            } else {
                CTSE_Handle::TSeq_feat_Handles far_handles = m_TSE.GetTSE_Handle().GetFeaturesWithId(CSeqFeatData::eSubtype_any, xref.GetId().GetLocal());
                for (auto it = far_handles.begin(); it != far_handles.end(); it++) {
                    far_feats.push_back(it->GetSeq_feat());
                }
            }
            if (far_feats.empty()) {
                PostErr(eDiag_Warning, eErr_SEQ_FEAT_SeqFeatXrefFeatureMissing,
                    "Cross-referenced feature cannot be found",
                    feat);
            } else {
                for (auto ff = far_feats.begin(); ff != far_feats.end(); ff++) {
                    ValidateOneFeatXrefPair(feat, **ff, xref);
                    if (xref.IsSetData()) {
                        // Check that feature with ID matches data
                        if (xref.GetData().Which() != (*ff)->GetData().Which()) {
                            PostErr(eDiag_Error, eErr_SEQ_FEAT_SeqFeatXrefProblem,
                                "SeqFeatXref contains both id and data, data type conflicts with data on feature with id",
                                feat);
                        }
                    }
                }
            }
        } else {
            PostErr(eDiag_Warning, eErr_SEQ_FEAT_SeqFeatXrefFeatureMissing,
                "Cross-referenced feature cannot be found",
                feat);
        }
    }
    if (xref.IsSetData() && xref.GetData().IsGene() && feat.GetData().IsGene()) {
        PostErr (eDiag_Warning, eErr_SEQ_FEAT_UnnecessaryGeneXref,
                 "Gene feature has gene cross-reference",
                 feat);
    }
}


END_SCOPE(validator)
END_SCOPE(objects)
END_NCBI_SCOPE
