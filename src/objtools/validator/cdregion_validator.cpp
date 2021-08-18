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
#include <objtools/validator/utilities.hpp>
#include <objtools/format/items/gene_finder.hpp>

#include <serial/serialbase.hpp>

#include <objmgr/bioseq_handle.hpp>
#include <objmgr/seq_entry_handle.hpp>
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
#include <objects/seqfeat/Prot_ref.hpp>

#include <objects/seq/MolInfo.hpp>
#include <objects/seq/Bioseq.hpp>
#include <objects/seq/seqport_util.hpp>

#include <objtools/edit/cds_fix.hpp>

#include <string>


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)
BEGIN_SCOPE(validator)
using namespace sequence;


CCdregionValidator::CCdregionValidator(const CSeq_feat& feat, CScope& scope, CValidError_imp& imp) :
CSingleFeatValidator(feat, scope, imp) {
    m_Gene = m_Imp.GetGeneCache().GetGeneFromCache(&feat, m_Scope);
    if (m_Gene) {
        m_GeneIsPseudo = s_IsPseudo(*m_Gene);
    } else {
        m_GeneIsPseudo = false;
    }
}


void CCdregionValidator::x_ValidateFeatComment()
{
    if (!m_Feat.IsSetComment()) {
        return;
    }
    CSingleFeatValidator::x_ValidateFeatComment();
    const string& comment = m_Feat.GetComment();
    if (NStr::Find(comment, "ambiguity in stop codon") != NPOS
        && !edit::DoesCodingRegionHaveTerminalCodeBreak(m_Feat.GetData().GetCdregion())) {
        CRef<CSeq_loc> stop_codon_loc = edit::GetLastCodonLoc(m_Feat, m_Scope);
        if (stop_codon_loc) {
            TSeqPos len = sequence::GetLength(*stop_codon_loc, &m_Scope);
            CSeqVector vec(*stop_codon_loc, m_Scope, CBioseq_Handle::eCoding_Iupac);
            string seq_string;
            vec.GetSeqData(0, len - 1, seq_string);
            bool found_ambig = false;
            string::iterator it = seq_string.begin();
            while (it != seq_string.end() && !found_ambig) {
                if (*it != 'A' && *it != 'T' && *it != 'C' && *it != 'G' && *it != 'U') {
                    found_ambig = true;
                }
                ++it;
            }
            if (!found_ambig) {
                m_Imp.PostErr(eDiag_Error, eErr_SEQ_FEAT_BadCDScomment,
                    "Feature comment indicates ambiguity in stop codon "
                    "but no ambiguities are present in stop codon.", m_Feat);
            }
        }
    }

    // look for EC number in comment
    if (HasECnumberPattern(m_Feat.GetComment())) {
        // suppress if protein has EC numbers
        bool suppress = false;
        if (m_ProductBioseq) {
            CFeat_CI prot_feat(m_ProductBioseq, SAnnotSelector(CSeqFeatData::eSubtype_prot));
            if (prot_feat && prot_feat->GetData().GetProt().IsSetEc()) {
                suppress = true;
            }
        }
        if (!suppress) {
            PostErr(eDiag_Info, eErr_SEQ_FEAT_EcNumberInCDSComment,
                "Apparent EC number in CDS comment");
        }
    }

}


void CCdregionValidator::x_ValidateExceptText(const string& text)
{
    CSingleFeatValidator::x_ValidateExceptText(text);
    if (m_Feat.GetData().GetCdregion().IsSetCode_break() &&
        NStr::FindNoCase(text, "RNA editing") != NPOS) {
        PostErr(eDiag_Warning, eErr_SEQ_FEAT_TranslExceptAndRnaEditing,
            "CDS has both RNA editing /exception and /transl_except qualifiers");
    }
}


#define FOR_EACH_SEQID_ON_BIOSEQ_HANDLE(Itr, Var) \
ITERATE (CBioseq_Handle::TId, Itr, Var.GetId())

void s_LocIdType(CBioseq_Handle bsh,
    bool& is_nt, bool& is_ng, bool& is_nw, bool& is_nc)
{
    is_nt = is_ng = is_nw = is_nc = false;
    if (bsh) {
        FOR_EACH_SEQID_ON_BIOSEQ_HANDLE(it, bsh) {
            CSeq_id_Handle sid = *it;
            switch (sid.Which()) {
            case NCBI_SEQID(Embl):
            case NCBI_SEQID(Ddbj):
            case NCBI_SEQID(Other):
            case NCBI_SEQID(Genbank):
            {
                CSeq_id::EAccessionInfo info = sid.IdentifyAccession();
                is_nt |= (info == CSeq_id::eAcc_refseq_contig);
                is_ng |= (info == CSeq_id::eAcc_refseq_genomic);
                is_nw |= (info == CSeq_id::eAcc_refseq_wgs_intermed);
                is_nc |= (info == CSeq_id::eAcc_refseq_chromosome);
                break;
            }
            default:
                break;
            }
        }
    }
}


void s_LocIdType(const CSeq_loc& loc, CScope& scope, const CSeq_entry& tse,
    bool& is_nt, bool& is_ng, bool& is_nw, bool& is_nc)
{
    is_nt = is_ng = is_nw = is_nc = false;
    if (!IsOneBioseq(loc, &scope)) {
        return;
    }
    const CSeq_id& id = GetId(loc, &scope);
    try {
        CBioseq_Handle bsh = scope.GetBioseqHandleFromTSE(id, tse);
        if (bsh) {
            s_LocIdType(bsh, is_nt, is_ng, is_nw, is_nc);
        }
    } catch (CException&) {
    }
}


void CCdregionValidator::x_ValidateTrans()
{
    CCDSTranslationProblems problems;
    bool is_nt, is_ng, is_nw, is_nc;
    s_LocIdType(m_LocationBioseq, is_nt, is_ng, is_nw, is_nc);

    problems.CalculateTranslationProblems(m_Feat,
                                    m_LocationBioseq,
                                    m_ProductBioseq,
                                    m_Imp.IgnoreExceptions(),
                                    m_Imp.IsFarFetchCDSproducts(),
                                    m_Imp.IsStandaloneAnnot(),
                                    m_Imp.IsStandaloneAnnot() ? false : m_Imp.GetTSE().IsSeq(),
                                    m_Imp.IsGpipe(),
                                    m_Imp.IsGenomic(),
                                    m_Imp.IsRefSeq(),
                                    (is_nt||is_ng||is_nw),
                                    is_nc,
                                    (m_Imp.IsRefSeq() || m_Imp.IsGED() || m_Imp.IsTPE()),
                                    &m_Scope);
    if (!problems.UnableToTranslate() && !problems.HasException()) {
        x_ValidateCodebreak();
    }

    if (problems.GetTranslationProblemFlags() & CCDSTranslationProblems::eCDSTranslationProblem_UnableToFetch) {
        if (m_Imp.x_IsFarFetchFailure(m_Feat.GetProduct())) {
            m_Imp.SetFarFetchFailure();
        }
    }

    x_ReportTranslationProblems(problems);
}


int GetGcodeForName(const string& code_name)
{
    const CGenetic_code_table& tbl = CGen_code_table::GetCodeTable();
    ITERATE(CGenetic_code_table::Tdata, it, tbl.Get()) {
        if (NStr::EqualNocase((*it)->GetName(), code_name)) {
            return (*it)->GetId();
        }
    }
    return 255;
}


int GetGcodeForInternalStopErrors(const CCdregion& cdr)
{
    int gc = 0;
    if (cdr.IsSetCode()) {
        ITERATE(CCdregion::TCode::Tdata, it, cdr.GetCode().Get()) {
            if ((*it)->IsId()) {
                gc = (*it)->GetId();
            } else if ((*it)->IsName()) {
                gc = GetGcodeForName((*it)->GetName());
            }
            if (gc != 0) break;
        }
    }
    return gc;
}


string GetInternalStopErrorMessage(const CSeq_feat& feat, size_t internal_stop_count, bool bad_start, char transl_start)
{
    int gc = GetGcodeForInternalStopErrors(feat.GetData().GetCdregion());
    string gccode = NStr::IntToString(gc);

    string error_message;
    if (bad_start) {
        bool got_dash = transl_start == '-';
        string codon_desc = got_dash ? "illegal" : "ambiguous";
        error_message = NStr::SizetToString(internal_stop_count) +
            " internal stops (and " + codon_desc + " start codon). Genetic code [" + gccode + "]";
    } else {
        error_message = NStr::SizetToString(internal_stop_count) +
            " internal stops. Genetic code [" + gccode + "]";
    }
    return error_message;
}


string GetInternalStopErrorMessage(const CSeq_feat& feat, const string& transl_prot)
{
    size_t internal_stop_count = CountInternalStopCodons(transl_prot);

    int gc = GetGcodeForInternalStopErrors(feat.GetData().GetCdregion());
    string gccode = NStr::IntToString(gc);

    string error_message;
    if (HasBadStartCodon(feat.GetLocation(), transl_prot)) {
        bool got_dash = transl_prot[0] == '-';
        string codon_desc = got_dash ? "illegal" : "ambiguous";
        error_message = NStr::SizetToString(internal_stop_count) +
            " internal stops (and " + codon_desc + " start codon). Genetic code [" + gccode + "]";
    } else {
        error_message = NStr::SizetToString(internal_stop_count) +
            " internal stops. Genetic code [" + gccode + "]";
    }
    return error_message;
}


string GetStartCodonErrorMessage(const CSeq_feat& feat, const char first_char, size_t internal_stop_count)
{
    bool got_dash = first_char == '-';
    string codon_desc = got_dash ? "Illegal" : "Ambiguous";
    string p_word = got_dash ? "Probably" : "Possibly";

    int gc = GetGcodeForInternalStopErrors(feat.GetData().GetCdregion());
    string gccode = NStr::IntToString(gc);

    string error_message;

    if (internal_stop_count > 0) {
        error_message = codon_desc + " start codon (and " +
            NStr::SizetToString(internal_stop_count) +
            " internal stops). " + p_word + " wrong genetic code [" +
            gccode + "]";
    } else {
        error_message = codon_desc + " start codon used. Wrong genetic code [" +
            gccode + "] or protein should be partial";
    }
    return error_message;
}


string GetStartCodonErrorMessage(const CSeq_feat& feat, const string& transl_prot)
{
    size_t internal_stop_count = CountInternalStopCodons(transl_prot);

    return GetStartCodonErrorMessage(feat, transl_prot[0], internal_stop_count);
}


void CCdregionValidator::x_ReportTranslationProblems(const CCDSTranslationProblems& problems)
{
    size_t problem_flags = problems.GetTranslationProblemFlags();
    if (problem_flags & CCDSTranslationProblems::eCDSTranslationProblem_UnableToFetch) {
        string label;
        const CSeq_id* protid = &GetId(m_Feat.GetProduct(), &m_Scope);
        protid->GetLabel(&label);
        EDiagSev sev = eDiag_Error;
        if (protid->IsGeneral() && protid->GetGeneral().IsSetDb() &&
            (NStr::EqualNocase(protid->GetGeneral().GetDb(), "ti") ||
            NStr::EqualNocase(protid->GetGeneral().GetDb(), "SRA"))) {
            sev = eDiag_Warning;
        }
        PostErr(sev, eErr_SEQ_FEAT_ProductFetchFailure,
            "Unable to fetch CDS product '" + label + "'");
    }

    if (!problems.HasException() && (problem_flags & CCDSTranslationProblems::eCDSTranslationProblem_NoProtein)) {
        bool is_nt, is_ng, is_nw, is_nc;
        s_LocIdType(m_Feat.GetLocation(), m_Scope, m_Imp.GetTSE(), is_nt, is_ng, is_nw, is_nc);
        EDiagSev sev = eDiag_Error;
        if (IsDeltaOrFarSeg(m_Feat.GetLocation(), &m_Scope)) {
            sev = eDiag_Warning;
        }
        if (is_nc) {
            sev = eDiag_Warning;
        }
        PostErr(sev, eErr_SEQ_FEAT_NoProtein,
            "No protein Bioseq given");
    }

    bool unclassified_except = false;
    if (m_Feat.IsSetExcept_text() && NStr::FindNoCase(m_Feat.GetExcept_text(), "unclassified translation discrepancy") != NPOS) {
        unclassified_except = true;
    }

    x_ReportTranslExceptProblems(problems.GetTranslExceptProblems(), problems.HasException());

    if (!problems.HasException() && problems.HasUnparsedTranslExcept()) {
        if (problems.GetInternalStopCodons() == 0 && problems.GetTranslationMismatches().size() == 0) {
            PostErr(eDiag_Warning, eErr_SEQ_FEAT_TranslExcept,
                "Unparsed transl_except qual (but protein is okay). Skipped");
        } else {
            PostErr(eDiag_Warning, eErr_SEQ_FEAT_TranslExcept,
                "Unparsed transl_except qual. Skipped");
        }
    }


    for (size_t i = 0; i < problems.GetNumNonsenseIntrons(); i++) {
        EDiagSev sev = eDiag_Critical;
        if (m_Imp.IsEmbl() || m_Imp.IsDdbj()) {
            sev = eDiag_Error;
        }
        PostErr(sev, eErr_SEQ_FEAT_IntronIsStopCodon, "Triplet intron encodes stop codon");
    }

    if (problem_flags & CCDSTranslationProblems::eCDSTranslationProblem_TooManyX) {
        PostErr(eDiag_Info, eErr_SEQ_FEAT_CDShasTooManyXs, "CDS translation consists of more than 50% X residues");
    }

    if (problems.UnableToTranslate()) {
        if (!problems.HasException()) {
            PostErr(eDiag_Error, eErr_SEQ_FEAT_CdTransFail,
                "Unable to translate");
        }
    }

    if (!problems.UnableToTranslate() && !problems.AltStart() &&
        m_Feat.IsSetExcept() && m_Feat.IsSetExcept_text() &&
        NStr::Find(m_Feat.GetExcept_text(), "alternative start codon") != string::npos &&
        x_BioseqHasNmAccession(m_LocationBioseq)) {

        PostErr(eDiag_Warning, eErr_SEQ_FEAT_AltStartCodonException,
                "Unnecessary alternative start codon exception");
    }

    if ((!problems.HasException() || unclassified_except) && problems.GetInternalStopCodons() > 0) {
        if (unclassified_except && m_Imp.IsGpipe()) {
            // suppress if gpipe genomic
        } else {
            EDiagSev stop_sev = unclassified_except ? eDiag_Warning : eDiag_Error;
            if (!m_Imp.IsRefSeq() && m_Imp.IsGI() && m_Imp.IsGED()) {
                stop_sev = eDiag_Critical;
            }

            PostErr(stop_sev, eErr_SEQ_FEAT_InternalStop,
                GetInternalStopErrorMessage(m_Feat, problems.GetInternalStopCodons(),
                                            (problem_flags & CCDSTranslationProblems::eCDSTranslationProblem_BadStart),
                                            problems.GetTranslStartCharacter()));
        }
    }

    if (!problems.HasException()) {

        if (!unclassified_except && (problem_flags & CCDSTranslationProblems::eCDSTranslationProblem_BadStart)) {
            string start_err_msg = GetStartCodonErrorMessage(m_Feat, problems.GetTranslStartCharacter(), problems.GetInternalStopCodons());
            PostErr(eDiag_Error, eErr_SEQ_FEAT_StartCodon,
                start_err_msg);
        }

        if (problem_flags & CCDSTranslationProblems::eCDSTranslationProblem_FrameNotPartial) {
            PostErr(eDiag_Error, eErr_SEQ_FEAT_SuspiciousFrame,
                "Suspicious CDS location - reading frame > 1 but not 5' partial");
        }

        if (problem_flags & CCDSTranslationProblems::eCDSTranslationProblem_FrameNotConsensus) {
            EDiagSev sev = eDiag_Warning;
            if (x_BioseqHasNmAccession(m_LocationBioseq))
            {
                sev = eDiag_Error;
            }
            PostErr(sev, eErr_SEQ_FEAT_SuspiciousFrame,
                "Suspicious CDS location - reading frame > 1 and not at consensus splice site");
        }

        if (problem_flags & CCDSTranslationProblems::eCDSTranslationProblem_NoStop) {
            PostErr(eDiag_Error, eErr_SEQ_FEAT_NoStop,
                "Missing stop codon");
        }
        if (problem_flags & CCDSTranslationProblems::eCDSTranslationProblem_StopPartial) {
            PostErr(eDiag_Error, eErr_SEQ_FEAT_PartialProblemHasStop,
                "Got stop codon, but 3'end is labeled partial");
        }
        if (problem_flags & CCDSTranslationProblems::eCDSTranslationProblem_ShouldStartPartial) {
            PostErr(eDiag_Error, eErr_SEQ_FEAT_PartialProblem,
                "Start of location should probably be partial");
        }
        if (problems.GetRaggedLength() > 0)  {
            PostErr(eDiag_Error, eErr_SEQ_FEAT_TransLen,
                "Coding region extends " + NStr::IntToString(problems.GetRaggedLength()) +
                " base(s) past stop codon");
        }
    }

    if (!problems.UnableToTranslate() && problems.GetProtLen() > 1.2 * problems.GetTransLen()) {
        if ((!m_Feat.IsSetExcept_text()) || NStr::Find(m_Feat.GetExcept_text(), "annotated by transcript or proteomic data") == string::npos) {
            string msg = "Protein product length [" + NStr::SizetToString(problems.GetProtLen()) +
                "] is more than 120% of the ";
            if (m_ProductIsFar) {
                msg += "(far) ";
            }
            msg += "translation length [" + NStr::SizetToString(problems.GetTransLen()) + "]";
            PostErr(eDiag_Warning, eErr_SEQ_FEAT_ProductLength, msg);
        }
    }


    bool rna_editing = false;
    if (m_Feat.IsSetExcept_text() && NStr::FindNoCase(m_Feat.GetExcept_text(), "RNA editing") != NPOS) {
        rna_editing = true;
    }
    if (problems.GetProtLen() != problems.GetTransLen() &&
        (!problems.HasException() ||
         (rna_editing &&
         (problems.GetProtLen() < problems.GetTransLen() - 1 || problems.GetProtLen() > problems.GetTransLen())))) {
        string msg = "Given protein length [" + NStr::SizetToString(problems.GetProtLen()) +
            "] does not match ";
        if (m_ProductIsFar) {
            msg += "(far) ";
        }
        msg += "translation length [" +
            NStr::SizetToString(problems.GetTransLen()) + "]";

        if (rna_editing) {
            msg += " (RNA editing present)";
        }
        PostErr(rna_editing ? eDiag_Warning : eDiag_Error,
            eErr_SEQ_FEAT_TransLen, msg);
    }

    bool mismatch_except = false;
    if (m_Feat.IsSetExcept_text() && NStr::FindNoCase(m_Feat.GetExcept_text(), "mismatches in translation") != NPOS) {
        mismatch_except = true;
    }

    if (!problems.HasException() && !mismatch_except) {
        x_ReportTranslationMismatches(problems.GetTranslationMismatches());
    }

    if (problems.GetTranslTerminalX() != problems.GetProdTerminalX()) {
        PostErr(eDiag_Warning, eErr_SEQ_FEAT_TerminalXDiscrepancy,
            "Terminal X count for CDS translation (" + NStr::SizetToString(problems.GetTranslTerminalX())
            + ") and protein product sequence (" + NStr::SizetToString(problems.GetProdTerminalX())
            + ") are not equal");
    }

    if (problem_flags & CCDSTranslationProblems::eCDSTranslationProblem_ShouldBePartialButIsnt) {
        PostErr(eDiag_Error, eErr_SEQ_FEAT_PartialProblem,
            "End of location should probably be partial");
    }
    if (problem_flags & CCDSTranslationProblems::eCDSTranslationProblem_ShouldNotBePartialButIs) {
        PostErr(eDiag_Error, eErr_SEQ_FEAT_PartialProblem,
            "This SeqFeat should not be partial");
    }

    if (problem_flags & CCDSTranslationProblems::eCDSTranslationProblem_UnnecessaryException) {
        PostErr(eDiag_Warning, eErr_SEQ_FEAT_UnnecessaryException,
            "CDS has exception but passes translation test");
    }

    if (problem_flags & CCDSTranslationProblems::eCDSTranslationProblem_ErroneousException) {
        PostErr(eDiag_Warning, eErr_SEQ_FEAT_ErroneousException,
            "CDS has unclassified exception but only difference is "
            + NStr::SizetToString(problems.GetTranslationMismatches().size()) + " mismatches out of "
            + NStr::SizetToString(problems.GetProtLen()) + " residues");
    }

    if (problem_flags & CCDSTranslationProblems::eCDSTranslationProblem_UnqualifiedException) {
        PostErr(eDiag_Warning, eErr_SEQ_FEAT_UnnecessaryException,
            "CDS has unnecessary translated product replaced exception");
    }

}


string CCdregionValidator::MapToNTCoords(TSeqPos pos)
{
    string result;

    CSeq_point pnt;
    pnt.SetPoint(pos);
    pnt.SetStrand( GetStrand(m_Feat.GetProduct(), &m_Scope) );

    try {
        pnt.SetId().Assign(GetId(m_Feat.GetProduct(), &m_Scope));
    } catch (CObjmgrUtilException) {}

    CSeq_loc tmp;
    tmp.SetPnt(pnt);
    CRef<CSeq_loc> loc = ProductToSource(m_Feat, tmp, 0, &m_Scope);

    result = GetValidatorLocationLabel(*loc, m_Scope);

    return result;
}


void CCdregionValidator::x_ReportTranslationMismatches(const CCDSTranslationProblems::TTranslationMismatches& mismatches)
{
    string nuclocstr;

    size_t num_mismatches = mismatches.size();

    if (num_mismatches > 10) {
        // report total number of mismatches and the details of the
        // first and last.
        nuclocstr = MapToNTCoords(mismatches.front().pos);
        string msg =
            NStr::SizetToString(mismatches.size()) + " mismatches found.  " +
            "First mismatch at " + NStr::IntToString(mismatches.front().pos + 1) +
            ", residue in protein [";
        msg += mismatches.front().prot_res;
        msg += "] != translation [";
        msg += mismatches.front().transl_res;
        msg += "]";
        if (!nuclocstr.empty()) {
            msg += " at " + nuclocstr;
        }
        nuclocstr = MapToNTCoords(mismatches.back().pos);
        msg +=
            ".  Last mismatch at " + NStr::IntToString(mismatches.back().pos + 1) +
            ", residue in protein [";
        msg += mismatches.back().prot_res;
        msg += "] != translation [";
        msg += mismatches.back().transl_res;
        msg += "]";
        if (!nuclocstr.empty()) {
            msg += " at " + nuclocstr;
        }
        int gc = 0;
        if (m_Feat.GetData().IsCdregion() && m_Feat.GetData().GetCdregion().IsSetCode()) {
            // We assume that the id is set for all Genetic_code
            gc = m_Feat.GetData().GetCdregion().GetCode().GetId();
        }
        string gccode = NStr::IntToString(gc);

        msg += ".  Genetic code [" + gccode + "]";
        PostErr(eDiag_Error, eErr_SEQ_FEAT_MisMatchAA, msg);
    } else {
        // report individual mismatches
        for (size_t i = 0; i < mismatches.size(); ++i) {
            nuclocstr = MapToNTCoords(mismatches[i].pos);
            if (mismatches[i].pos == 0 && mismatches[i].transl_res == '-') {
                // skip - dash is expected to differ
                num_mismatches--;
            } else {
                EDiagSev sev = eDiag_Error;
                if (mismatches[i].prot_res == 'X' &&
                    (mismatches[i].transl_res == 'B' || mismatches[i].transl_res == 'Z' || mismatches[i].transl_res == 'J')) {
                    sev = eDiag_Warning;
                }
                string msg;
                if (m_ProductIsFar) {
                    msg += "(far) ";
                }
                msg += "Residue " + NStr::IntToString(mismatches[i].pos + 1) +
                    " in protein [";
                msg += mismatches[i].prot_res;
                msg += "] != translation [";
                msg += mismatches[i].transl_res;
                msg += "]";
                if (!nuclocstr.empty()) {
                    msg += " at " + nuclocstr;
                }
                PostErr(sev, eErr_SEQ_FEAT_MisMatchAA, msg);
            }
        }
    }
}


void CCdregionValidator::x_ReportTranslExceptProblems(const CCDSTranslationProblems::TTranslExceptProblems& problems, bool has_exception)
{
    for (auto it = problems.begin(); it != problems.end(); it++) {
        string msg;
        switch (it->problem) {
        case CCDSTranslationProblems::eTranslExceptPhase:
            if (!has_exception) {
                PostErr(eDiag_Warning, eErr_SEQ_FEAT_TranslExceptPhase,
                    "transl_except qual out of frame.");
            }
            break;
        case CCDSTranslationProblems::eTranslExceptSuspicious:
            msg = "Suspicious transl_except ";
            msg += it->ex;
            msg += " at first codon of complete CDS";
            PostErr(eDiag_Warning, eErr_SEQ_FEAT_TranslExcept, msg);
            break;
        case CCDSTranslationProblems::eTranslExceptUnnecessary:
            msg = "Unnecessary transl_except ";
            msg += it->ex;
            msg += " at position ";
            msg += NStr::SizetToString(it->prot_pos + 1);
            PostErr(eDiag_Warning, eErr_SEQ_FEAT_UnnecessaryTranslExcept,
                    msg);
            break;
        case CCDSTranslationProblems::eTranslExceptUnexpected:
            msg = "Unexpected transl_except ";
            msg += it->ex;
            msg += +" at position " + NStr::SizetToString(it->prot_pos + 1)
                + " just past end of protein";

            PostErr(eDiag_Warning, eErr_SEQ_FEAT_UnnecessaryTranslExcept,
                    msg);
            break;
        }
    }
}


void CCdregionValidator::x_ValidateCodebreak()
{
    const CCdregion& cds = m_Feat.GetData().GetCdregion();
    const CSeq_loc& feat_loc = m_Feat.GetLocation();
    const CCode_break* prev_cbr = 0;

    FOR_EACH_CODEBREAK_ON_CDREGION (it, cds) {
        const CCode_break& cbr = **it;
        const CSeq_loc& cbr_loc = cbr.GetLoc();
        ECompare comp = Compare(cbr_loc, feat_loc, &m_Scope, fCompareOverlapping);
        if ( ((comp != eContained) && (comp != eSame)) || cbr_loc.IsNull() || cbr_loc.IsEmpty()) {
            PostErr (eDiag_Critical, eErr_SEQ_FEAT_CDSrange,
                "Code-break location not in coding region");
        } else if (m_Feat.IsSetProduct()) {
            if (cbr_loc.GetStop(eExtreme_Biological) == feat_loc.GetStop(eExtreme_Biological)) {
                // terminal exception - don't bother checking, can't be mapped
            } else {
                if (SeqLocCheck(cbr_loc, &m_Scope) == eSeqLocCheck_error) {
                    string lbl = GetValidatorLocationLabel(cbr_loc, m_Scope);
                    PostErr(eDiag_Critical, eErr_SEQ_FEAT_CDSrange,
                        "Code-break: SeqLoc [" + lbl + "] out of range");
                } else {
                    int frame = 0;
                    CRef<CSeq_loc> p_loc = SourceToProduct(m_Feat, cbr_loc, fS2P_AllowTer, &m_Scope, &frame);
                    if (!p_loc || p_loc->IsNull() || frame != 1) {
                        PostErr(eDiag_Critical, eErr_SEQ_FEAT_CDSrange,
                            "Code-break location not in coding region - may be frame problem");
                    }
                }
            }
        }
        if (cbr_loc.IsPartialStart(eExtreme_Biological) ||
            cbr_loc.IsPartialStop(eExtreme_Biological)) {
            PostErr(eDiag_Error, eErr_SEQ_FEAT_TranslExceptIsPartial,
                   "Translation exception locations should not be partial");
        }
        if ( prev_cbr != 0 ) {
            if ( Compare(cbr_loc, prev_cbr->GetLoc(), &m_Scope, fCompareOverlapping) == eSame ) {
                string msg = "Multiple code-breaks at same location ";
                string str = GetValidatorLocationLabel (cbr_loc, m_Scope);
                if ( !str.empty() ) {
                    msg += "[" + str + "]";
                }
                PostErr(eDiag_Error, eErr_SEQ_FEAT_DuplicateTranslExcept,
                   msg);
            }
        }
        prev_cbr = &cbr;
    }
}


void CCdregionValidator::Validate()
{
    CSingleFeatValidator::Validate();

    bool feat_is_pseudo = s_IsPseudo(m_Feat);
    bool pseudo = feat_is_pseudo || m_GeneIsPseudo;

    x_ValidateQuals();
    x_ValidateGeneticCode();

    const CCdregion& cdregion = m_Feat.GetData().GetCdregion();
    if (cdregion.IsSetOrf() && cdregion.GetOrf() &&
        m_Feat.IsSetProduct()) {
        PostErr(eDiag_Warning, eErr_SEQ_FEAT_OrfCdsHasProduct,
            "An ORF coding region should not have a product");
    }

    if (pseudo) {
        if (m_Feat.IsSetProduct()) {
            if (feat_is_pseudo) {
                PostErr(eDiag_Error, eErr_SEQ_FEAT_PseudoCdsHasProduct,
                    "A pseudo coding region should not have a product");
            } else if (m_GeneIsPseudo) {
                PostErr(eDiag_Error, eErr_SEQ_FEAT_PseudoCdsViaGeneHasProduct,
                    "A coding region overlapped by a pseudogene should not have a product");
            } else {
                PostErr(eDiag_Error, eErr_SEQ_FEAT_PseudoCdsHasProduct,
                    "A pseudo coding region should not have a product");
            }
        }
    } else {
        ReportShortIntrons();
        x_ValidateProductId();
        x_ValidateCommonProduct();
    }

    x_ValidateBadMRNAOverlap();
    x_ValidateFarProducts();
    x_ValidateCDSPeptides();
    x_ValidateCDSPartial();

    if (x_IsProductMisplaced()) {
        if (m_Imp.IsSmallGenomeSet()) {
            PostErr(eDiag_Warning, eErr_SEQ_FEAT_CDSproductPackagingProblem,
                "Protein product not packaged in nuc-prot set with nucleotide in small genome set");
        } else {
            PostErr(eDiag_Error, eErr_SEQ_FEAT_CDSproductPackagingProblem,
                "Protein product not packaged in nuc-prot set with nucleotide");
        }
    }

    bool conflict = cdregion.IsSetConflict()  &&  cdregion.GetConflict();
    if ( !pseudo  &&  !conflict ) {
        x_ValidateTrans();
        ValidateSplice(false, false);
    }

    if (conflict) {
        x_ValidateConflict();
    }

    x_ReportPseudogeneConflict(m_Gene);
    x_ValidateLocusTagGeneralMatch(m_Gene);

    x_ValidateProductPartials();
    x_ValidateParentPartialness();
}


void CCdregionValidator::x_ValidateQuals()
{

    FOR_EACH_GBQUAL_ON_FEATURE(it, m_Feat) {
        const CGb_qual& qual = **it;
        if (qual.CanGetQual()) {
            const string& key = qual.GetQual();
            if (NStr::EqualNocase(key, "exception")) {
                if (!m_Feat.IsSetExcept()) {
                    PostErr(eDiag_Warning, eErr_SEQ_FEAT_MissingExceptionFlag,
                        "Exception flag should be set in coding region");
                }
            } else if (NStr::EqualNocase(key, "codon")) {
                PostErr(eDiag_Error, eErr_SEQ_FEAT_CodonQualifierUsed,
                    "Use the proper genetic code, if available, "
                    "or set transl_excepts on specific codons");
            } else if (NStr::EqualNocase(key, "protein_id")) {
                PostErr(eDiag_Warning, eErr_SEQ_FEAT_WrongQualOnFeature,
                    "protein_id should not be a gbqual on a CDS feature");
            } else if (NStr::EqualNocase(key, "gene_synonym")) {
                PostErr(eDiag_Warning, eErr_SEQ_FEAT_WrongQualOnCDS,
                    "gene_synonym should not be a gbqual on a CDS feature");
            } else if (NStr::EqualNocase(key, "transcript_id")) {
                PostErr(eDiag_Warning, eErr_SEQ_FEAT_WrongQualOnFeature,
                    "transcript_id should not be a gbqual on a CDS feature");
            } else if (NStr::EqualNocase(key, "codon_start")) {
                const CCdregion& cdregion = m_Feat.GetData().GetCdregion();
                if (cdregion.IsSetFrame() && cdregion.GetFrame() != CCdregion::eFrame_not_set) {
                    PostErr(eDiag_Warning, eErr_SEQ_FEAT_WrongQualOnFeature,
                        "conflicting codon_start values");
                } else {
                    PostErr(eDiag_Warning, eErr_SEQ_FEAT_InvalidCodonStart,
                        "codon_start value should be 1, 2, or 3");
                }
            }
        }
    }
}


bool CCdregionValidator::x_ReportOrigProteinId()
{
    if (!m_GeneIsPseudo && !s_IsPseudo(m_Feat)) {
        return true;
    } else {
        return false;
    }
}


const string s_PlastidTxt[] = {
  "",
  "",
  "chloroplast",
  "chromoplast",
  "",
  "",
  "plastid",
  "",
  "",
  "",
  "",
  "",
  "cyanelle",
  "",
  "",
  "",
  "apicoplast",
  "leucoplast",
  "proplastid",
  ""
};


bool CCdregionValidator::IsPlastid(int genome)
{
    if ( genome == CBioSource::eGenome_chloroplast  ||
         genome == CBioSource::eGenome_chromoplast  ||
         genome == CBioSource::eGenome_plastid      ||
         genome == CBioSource::eGenome_cyanelle     ||
         genome == CBioSource::eGenome_apicoplast   ||
         genome == CBioSource::eGenome_leucoplast   ||
         genome == CBioSource::eGenome_proplastid   ||
         genome == CBioSource::eGenome_chromatophore ) {
        return true;
    }

    return false;
}


static bool IsGeneticCodeValid(int gcode)
{
    bool ret = false;
    if (gcode > 0) {

        try {
            const CTrans_table& tbl = CGen_code_table::GetTransTable(gcode);
            (void)tbl;  // suppress unused-variable warning
            ret = true;
        }
        catch (CException&) {
        }
    }

    return ret;
}


static int s_GetStrictGenCode(const CBioSource& src)
{
    int gencode = 0;

    try {
      CBioSource::TGenome genome = src.IsSetGenome() ? src.GetGenome() : CBioSource::eGenome_unknown;

        if ( src.IsSetOrg()  &&  src.GetOrg().IsSetOrgname() ) {
            const COrgName& orn = src.GetOrg().GetOrgname();

            switch ( genome ) {
                case CBioSource::eGenome_kinetoplast:
                case CBioSource::eGenome_mitochondrion:
                case CBioSource::eGenome_hydrogenosome:
                    // bacteria and plant organelle code
                    if (orn.IsSetMgcode()) {
                        gencode = orn.GetMgcode();
                    }
                    break;
                case CBioSource::eGenome_chloroplast:
                case CBioSource::eGenome_chromoplast:
                case CBioSource::eGenome_plastid:
                case CBioSource::eGenome_cyanelle:
                case CBioSource::eGenome_apicoplast:
                case CBioSource::eGenome_leucoplast:
                case CBioSource::eGenome_proplastid:
                    if (orn.IsSetPgcode() && orn.GetPgcode() != 0) {
                        gencode = orn.GetPgcode();
                    } else {
                        // bacteria and plant plastids are code 11.
                        gencode = 11;
                    }
                    break;
                default:
                    if (orn.IsSetGcode()) {
                        gencode = orn.GetGcode();
                    }
                    break;
            }
        }
    } catch (CException ) {
    } catch (std::exception ) {
    }
    return gencode;
}


void CCdregionValidator::x_ValidateGeneticCode()
{
    if (!m_LocationBioseq) {
        return;
    }
    int cdsgencode = 0;

    const CCdregion& cdregion = m_Feat.GetData().GetCdregion();

    if (cdregion.CanGetCode()) {
        cdsgencode = cdregion.GetCode().GetId();

        if (!IsGeneticCodeValid(cdsgencode)) {
            PostErr(eDiag_Error, eErr_SEQ_FEAT_GenCodeInvalid,
                "A coding region contains invalid genetic code [" + NStr::IntToString(cdsgencode) + "]");
        }
    }

    CSeqdesc_CI diter(m_LocationBioseq, CSeqdesc::e_Source);
    if (diter) {
        const CBioSource& src = diter->GetSource();
        int biopgencode = s_GetStrictGenCode(src);

        if (biopgencode != cdsgencode
            && (!m_Feat.IsSetExcept()
            || !m_Feat.IsSetExcept_text()
            || NStr::Find(m_Feat.GetExcept_text(), "genetic code exception") == string::npos)) {
            int genome = 0;

            if (src.CanGetGenome()) {
                genome = src.GetGenome();
            }

            if (IsPlastid(genome)) {
                PostErr(eDiag_Warning, eErr_SEQ_FEAT_GenCodeMismatch,
                    "Genetic code conflict between CDS (code " +
                    NStr::IntToString(cdsgencode) +
                    ") and BioSource.genome biological context (" +
                    s_PlastidTxt[genome] + ") (uses code 11)");
            } else {
                PostErr(eDiag_Warning, eErr_SEQ_FEAT_GenCodeMismatch,
                    "Genetic code conflict between CDS (code " +
                    NStr::IntToString(cdsgencode) +
                    ") and BioSource (code " +
                    NStr::IntToString(biopgencode) + ")");
            }
        }
    }
}


void CCdregionValidator::x_ValidateSeqFeatLoc()
{
    CSingleFeatValidator::x_ValidateSeqFeatLoc();

    // for coding regions, internal exons should not be 15 or less bp long
    int num_short_exons = 0;
    string message;
    CSeq_loc_CI it(m_Feat.GetLocation());
    if (it) {
        // note - do not want to warn for first or last exon
        ++it;
        size_t prev_len = 16;
        size_t prev_start = 0;
        size_t prev_stop = 0;
        while (it) {
            if (prev_len <= 15) {
                num_short_exons++;
                if (!message.empty()) {
                    message += ", ";
                }
                message += NStr::NumericToString(prev_start + 1)
                    + "-" + NStr::NumericToString(prev_stop + 1);
            }
            prev_len = it.GetRange().GetLength();
            prev_start = it.GetRange().GetFrom();
            prev_stop = it.GetRange().GetTo();
            ++it;
        }
    }
    if (num_short_exons > 1) {
        PostErr(eDiag_Warning, eErr_SEQ_FEAT_ShortExon,
            "Coding region has multiple internal exons that are too short at positions " + message);
    } else if (num_short_exons == 1) {
        PostErr(eDiag_Warning, eErr_SEQ_FEAT_ShortExon,
            "Internal coding region exon is too short at position " + message);
    }
}


void CCdregionValidator::x_ValidateBadMRNAOverlap()
{
    if (x_HasGoodParent()) {
        return;
    }

    const CSeq_loc& loc = m_Feat.GetLocation();

    CConstRef<CSeq_feat> mrna = GetBestOverlappingFeat(
        loc,
        CSeqFeatData::eSubtype_mRNA,
        eOverlap_Simple,
        m_Scope);
    if (!mrna) {
        return;
    }

    mrna = GetBestOverlappingFeat(
        loc,
        CSeqFeatData::eSubtype_mRNA,
        eOverlap_CheckIntRev,
        m_Scope);
    if (mrna) {
        return;
    }

    mrna = GetBestOverlappingFeat(
        loc,
        CSeqFeatData::eSubtype_mRNA,
        eOverlap_Interval,
        m_Scope);
    if (!mrna) {
        return;
    }

    bool pseudo = s_IsPseudo(m_Feat) || m_GeneIsPseudo;

    EErrType err_type = eErr_SEQ_FEAT_CDSmRNArange;
    if (pseudo) {
        err_type = eErr_SEQ_FEAT_PseudoCDSmRNArange;
    }

    mrna = GetBestOverlappingFeat(
        loc,
        CSeqFeatData::eSubtype_mRNA,
        eOverlap_SubsetRev,
        m_Scope);

    EDiagSev sev = eDiag_Warning;
    if (pseudo) {
        sev = eDiag_Info;
    }
    if (mrna) {
        // ribosomal slippage exception suppresses CDSmRNArange warning
        bool supress = false;

        if (m_Feat.CanGetExcept_text()) {
            const CSeq_feat::TExcept_text& text = m_Feat.GetExcept_text();
            if (NStr::FindNoCase(text, "ribosomal slippage") != NPOS
                || NStr::FindNoCase(text, "trans-splicing") != NPOS) {
                supress = true;
            }
        }
        if (!supress) {
            PostErr(sev, err_type,
                "mRNA contains CDS but internal intron-exon boundaries "
                "do not match");
        }
    } else {
        PostErr(sev, err_type,
            "mRNA overlaps or contains CDS but does not completely "
            "contain intervals");
    }
}


bool CCdregionValidator::x_HasGoodParent()
{
    static const CSeqFeatData::ESubtype parent_types[] = {
        CSeqFeatData::eSubtype_C_region,
        CSeqFeatData::eSubtype_D_segment,
        CSeqFeatData::eSubtype_J_segment,
        CSeqFeatData::eSubtype_V_segment
    };
    size_t num_parent_types = sizeof(parent_types) / sizeof(CSeqFeatData::ESubtype);
    CRef<feature::CFeatTree> feat_tree = m_Imp.GetGeneCache().GetFeatTreeFromCache(m_Feat, m_Scope);
    if (!feat_tree) {
        return false;
    }
    CSeq_feat_Handle fh;
    try {
        // will fail if location is bad
        fh = m_Scope.GetSeq_featHandle(m_Feat);
    } catch (CException&) {
        return false;
    }

    for (size_t i = 0; i < num_parent_types; i++) {
        CMappedFeat parent = feat_tree->GetParent(fh, parent_types[i]);
        if (parent) {
            sequence::ECompare cmp = sequence::Compare(m_Feat.GetLocation(),
                                                       parent.GetLocation(),
                                                       &m_Scope,
                                                       sequence::fCompareOverlapping);
            if (cmp == sequence::eContained || cmp == sequence::eSame) {
                return true;
            }
        }
    }
    return false;
}


// VR-619
// for an mRNA / CDS pair where both have far products
// (which is only true for genomic RefSeqs with instantiated mRNA products),
// please check that the pair found by CFeatTree corresponds to the nuc-prot pair in ID
// (i.e.the CDS product is annotated on the mRNA product).
void CCdregionValidator::x_ValidateFarProducts()
{
    // if coding region doesn't have a far product, nothing to check
    if (!m_ProductIsFar) {
        return;
    }
    // no point if not far-fetching
    if (!m_Imp.IsRemoteFetch()) {
        return;
    }
    if (!m_Feat.GetData().IsCdregion() || !m_Feat.IsSetProduct()) {
        return;
    }
    if (!m_Imp.IsRefSeq()) {
        return;
    }
    const CSeq_id * cds_sid = m_Feat.GetProduct().GetId();
    if (!cds_sid) {
        return;
    }
    CRef<feature::CFeatTree> feat_tree = m_Imp.GetGeneCache().GetFeatTreeFromCache(m_Feat, m_Scope);
    if (!feat_tree) {
        return;
    }
    CSeq_feat_Handle fh = m_Scope.GetSeq_featHandle(m_Feat);
    if (!fh) {
        return;
    }
    CMappedFeat mrna = feat_tree->GetParent(fh, CSeqFeatData::eSubtype_mRNA);
    if (!mrna || !mrna.IsSetProduct()) {
        // no mRNA or no mRNA product
        return;
    }
    const CSeq_id * mrna_sid = mrna.GetProduct().GetId();
    if (!mrna_sid) {
        return;
    }
    CBioseq_Handle mrna_prod = m_Scope.GetBioseqHandleFromTSE(*mrna_sid, m_LocationBioseq.GetTSE_Handle());
    if (mrna_prod) {
        // mRNA product is not far
        return;
    }
    mrna_prod = m_Scope.GetBioseqHandle(*mrna_sid);
    if (!mrna_prod) {
        // can't be fetched, will be reported elsewhere
        return;
    }
    CSeq_entry_Handle far_mrna_nps =
        mrna_prod.GetExactComplexityLevel(CBioseq_set::eClass_nuc_prot);
    if (!far_mrna_nps) {
        PostErr(eDiag_Error, eErr_SEQ_FEAT_CDSmRNAmismatch, "no Far mRNA nuc-prot-set");
    } else {
        CBioseq_Handle cds_prod = m_Scope.GetBioseqHandleFromTSE(*cds_sid, far_mrna_nps);
        if (!cds_prod) {
            PostErr(eDiag_Error, eErr_SEQ_FEAT_CDSmRNAmismatch, "Far CDS product and far mRNA product are not packaged together");
            m_Imp.PostErr(eDiag_Error, eErr_SEQ_FEAT_CDSmRNAmismatch, "Far CDS product and far mRNA product are not packaged together", *(mrna.GetSeq_feat()));
        }
    }
}


void CCdregionValidator::x_ValidateCDSPeptides()
{
    try {
    if (!m_Feat.GetData().IsCdregion()  ||  !m_Feat.CanGetProduct()) {
        return;
    }

    CBioseq_Handle prot = m_Scope.GetBioseqHandle(m_Feat.GetProduct());
    if (!prot) {
        return;
    }
    CBioseq_Handle nuc = m_Scope.GetBioseqHandle(m_Feat.GetLocation());
    if (!nuc) {
        return;
    }
    // check for self-referential CDS feature
    if (nuc == prot) {
        return;
    }

    const CGene_ref* cds_ref = 0;

    // map from cds product to nucleotide
    const string prev = GetDiagFilter(eDiagFilter_Post);
    SetDiagFilter(eDiagFilter_All, "!(1305.28,31)");
    CSeq_loc_Mapper prot_to_cds(m_Feat, CSeq_loc_Mapper::eProductToLocation, &m_Scope);
    SetDiagFilter(eDiagFilter_All, prev.c_str());

    for (CFeat_CI it(prot, CSeqFeatData::e_Prot); it; ++it) {
        CSeq_feat_Handle curr = it->GetSeq_feat_Handle();
        CSeqFeatData::ESubtype subtype = curr.GetFeatSubtype();

        if (subtype != CSeqFeatData::eSubtype_preprotein &&
            subtype != CSeqFeatData::eSubtype_mat_peptide_aa &&
            subtype != CSeqFeatData::eSubtype_sig_peptide_aa &&
            subtype != CSeqFeatData::eSubtype_transit_peptide_aa &&
            subtype != CSeqFeatData::eSubtype_propeptide_aa) {
            continue;
        }

        // see if already has gene xref
        if (curr.GetGeneXref()) {
            continue;
        }

        if (! cds_ref) {
            // wait until first mat_peptide found to avoid expensive computation on CDS /gene qualifier
            CConstRef<CSeq_feat> cgene = sequence::GetBestOverlappingFeat (m_Feat.GetLocation(), CSeqFeatData::eSubtype_gene, sequence::eOverlap_SubsetRev, m_Scope);
            if (cgene && cgene->CanGetData() && cgene->GetData().IsGene()) {
                const CGene_ref& cgref = cgene->GetData().GetGene();
                cds_ref = &cgref;
            } else {
                // if CDS does not have overlapping gene, bail out of function
                return;
            }
        }

        const CSeq_loc& loc = curr.GetLocation();
        // map prot location to nuc location
        CRef<CSeq_loc> nloc(prot_to_cds.Map(loc));
        if (! nloc) {
            continue;
        }

        const CGene_ref* pep_ref = 0;
        CConstRef<CSeq_feat> pgene = sequence::GetBestOverlappingFeat (*nloc, CSeqFeatData::eSubtype_gene, sequence::eOverlap_SubsetRev, m_Scope);
        if (pgene && pgene->CanGetData() && pgene->GetData().IsGene()) {
            const CGene_ref& pgref = pgene->GetData().GetGene();
            pep_ref = &pgref;
        }

        if (! cds_ref || ! pep_ref) {
            continue;
        }
        if (cds_ref->IsSetLocus_tag() && pep_ref->IsSetLocus_tag()) {
            if (cds_ref->GetLocus_tag() == pep_ref->GetLocus_tag()) {
               continue;
            }
        } else if (cds_ref->IsSetLocus() && pep_ref->IsSetLocus()) {
            if (cds_ref->GetLocus() == pep_ref->GetLocus()) {
                continue;
            }
        }

        if (pgene) {

            const CSeq_loc& gloc = pgene->GetLocation();

            if (sequence::Compare(*nloc, gloc, nullptr /* scope */, sequence::fCompareOverlapping) == sequence::eSame) {

                PostErr(eDiag_Warning, eErr_SEQ_FEAT_GeneOnNucPositionOfPeptide,
                        "Peptide under CDS matches small Gene");
            }
        }
    }
    } catch (CException) {
    }
}


void CCdregionValidator::x_ValidateCDSPartial()
{
    if (!m_ProductBioseq || x_BypassCDSPartialTest()) {
        return;
    }

    CSeqdesc_CI sd(m_ProductBioseq, CSeqdesc::e_Molinfo);
    if (!sd) {
        return;
    }
    const CMolInfo& molinfo = sd->GetMolinfo();

    const CSeq_loc& loc = m_Feat.GetLocation();
    bool partial5 = loc.IsPartialStart(eExtreme_Biological);
    bool partial3 = loc.IsPartialStop(eExtreme_Biological);

    if (molinfo.CanGetCompleteness()) {
        switch (molinfo.GetCompleteness()) {
        case CMolInfo::eCompleteness_unknown:
            break;

        case CMolInfo::eCompleteness_complete:
            if (partial5 || partial3) {
                PostErr(eDiag_Error, eErr_SEQ_FEAT_PartialProblem,
                    "CDS is partial but protein is complete");
            }
            break;

        case CMolInfo::eCompleteness_partial:
            break;

        case CMolInfo::eCompleteness_no_left:
            if (!partial5) {
                PostErr(eDiag_Error, eErr_SEQ_FEAT_PartialProblem,
                    "CDS is 5' complete but protein is NH2 partial");
            }
            if (partial3) {
                EDiagSev sev = eDiag_Error;
                if (x_CDS3primePartialTest())
                {
                    sev = eDiag_Warning;
                }
                PostErr(sev, eErr_SEQ_FEAT_PartialProblem,
                    "CDS is 3' partial but protein is NH2 partial");
            }
            break;

        case CMolInfo::eCompleteness_no_right:
            if (!partial3) {
                PostErr(eDiag_Error, eErr_SEQ_FEAT_PartialProblem,
                    "CDS is 3' complete but protein is CO2 partial");
            }
            if (partial5) {
                EDiagSev sev = eDiag_Error;
                if (x_CDS5primePartialTest())
                {
                    sev = eDiag_Warning;
                }
                PostErr(sev, eErr_SEQ_FEAT_PartialProblem,
                    "CDS is 5' partial but protein is CO2 partial");
            }
            break;

        case CMolInfo::eCompleteness_no_ends:
            if (partial5 && partial3) {
            } else if (partial5) {
                EDiagSev sev = eDiag_Error;
                if (x_CDS5primePartialTest())
                {
                    sev = eDiag_Warning;
                }
                PostErr(sev, eErr_SEQ_FEAT_PartialProblem,
                    "CDS is 5' partial but protein has neither end");
            } else if (partial3) {
                EDiagSev sev = eDiag_Error;
                if (x_CDS3primePartialTest()) {
                    sev = eDiag_Warning;
                }

                PostErr(sev, eErr_SEQ_FEAT_PartialProblem,
                    "CDS is 3' partial but protein has neither end");
            } else {
                PostErr(eDiag_Error, eErr_SEQ_FEAT_PartialProblem,
                    "CDS is complete but protein has neither end");
            }
            break;

        case CMolInfo::eCompleteness_has_left:
            break;

        case CMolInfo::eCompleteness_has_right:
            break;

        case CMolInfo::eCompleteness_other:
            break;

        default:
            break;
        }
    }
}


static const char* const sc_BypassCdsPartialCheckText[] = {
    "RNA editing",
    "annotated by transcript or proteomic data",
    "artificial frameshift",
    "mismatches in translation",
    "rearrangement required for product",
    "reasons given in citation",
    "translated product replaced",
    "unclassified translation discrepancy"
};
typedef CStaticArraySet<const char*, PCase_CStr> TBypassCdsPartialCheckSet;
DEFINE_STATIC_ARRAY_MAP(TBypassCdsPartialCheckSet, sc_BypassCdsPartialCheck, sc_BypassCdsPartialCheckText);

bool CCdregionValidator::x_BypassCDSPartialTest() const
{
    if (m_Feat.CanGetExcept() && m_Feat.GetExcept() && m_Feat.CanGetExcept_text()) {
        const string& except_text = m_Feat.GetExcept_text();
        ITERATE(TBypassCdsPartialCheckSet, it, sc_BypassCdsPartialCheck) {
            if (NStr::FindNoCase(except_text, *it) != NPOS) {
                return true;  // biological exception
            }
        }
    }
    return false;
}


bool CCdregionValidator::x_CDS3primePartialTest() const
{
    CSeq_loc_CI last;
    for (CSeq_loc_CI sl_iter(m_Feat.GetLocation()); sl_iter; ++sl_iter) {
        last = sl_iter;
    }

    if (last) {
        if (last.GetStrand() == eNa_strand_minus) {
            if (last.GetRange().GetFrom() == 0) {
                return true;
            }
        } else {
            if (!m_LocationBioseq) {
                return false;
            }
            if (last.GetRange().GetTo() == m_LocationBioseq.GetInst_Length() - 1) {
                return true;
            }
        }
    }
    return false;
}


bool CCdregionValidator::x_CDS5primePartialTest() const
{
    CSeq_loc_CI first(m_Feat.GetLocation());

    if (first) {
        if (first.GetStrand() == eNa_strand_minus) {
            if (!m_LocationBioseq) {
                return false;
            }
            if (first.GetRange().GetTo() == m_LocationBioseq.GetInst_Length() - 1) {
                return true;
            }
        } else {
            if (first.GetRange().GetFrom() == 0) {
                return true;
            }
        }
    }
    return false;
}


bool CCdregionValidator::x_IsProductMisplaced() const
{
    // don't calculate if no product or if ORF flag is set
    if (!m_Feat.IsSetProduct() ||
        m_Feat.GetData().GetCdregion().IsSetOrf()) {
        return false;
    }
    // don't calculate if feature is pseudo
    if (s_IsPseudo(m_Feat) || m_GeneIsPseudo) {
        return false;
    }
    if (!m_ProductBioseq) {
        return false;
    } else if (m_ProductIsFar) {
        if (m_Imp.RequireLocalProduct(m_Feat.GetProduct().GetId())) {
            return true;
        } else {
            return false;
        }
    }

    bool found_match = false;

    CSeq_entry_Handle prod_nps =
        m_ProductBioseq.GetExactComplexityLevel(CBioseq_set::eClass_nuc_prot);
    if (!prod_nps) {
        return true;
    }

    for (CSeq_loc_CI loc_i(m_Feat.GetLocation()); loc_i; ++loc_i) {
        const CSeq_id& sid = loc_i.GetSeq_id();
        if (sid.IsOther() && sid.GetOther().IsSetAccession() && NStr::StartsWith(sid.GetOther().GetAccession(), "NT_")) {
            return false;
        }
        CBioseq_Handle nuc = m_Scope.GetBioseqHandle(loc_i.GetSeq_id());
        if (nuc) {
            if (s_BioseqHasRefSeqThatStartsWithPrefix(nuc, "NT_")) {
                // we don't report this for NT records
                return false;
            }
            CSeq_entry_Handle wgs = nuc.GetExactComplexityLevel(CBioseq_set::eClass_gen_prod_set);
            if (wgs) {
                // we don't report this for gen-prod-sets
                return false;
            }

            CSeq_entry_Handle nuc_nps =
                nuc.GetExactComplexityLevel(CBioseq_set::eClass_nuc_prot);

            if (prod_nps == nuc_nps) {
                found_match = true;
                break;
            }
        }
    }
    return !found_match;
}


void CCdregionValidator::x_AddToIntronList(vector<CCdregionValidator::TShortIntron>& shortlist, TSeqPos last_start, TSeqPos last_stop, TSeqPos this_start, TSeqPos this_stop)
{
    if (abs ((int)this_start - (int)last_stop) < 11) {
        shortlist.push_back(TShortIntron(last_stop, this_start));
    } else if (abs ((int)this_stop - (int)last_start) < 11) {
        shortlist.push_back(TShortIntron(last_start, this_stop));
    }
}


vector<CCdregionValidator::TShortIntron> CCdregionValidator::x_GetShortIntrons(const CSeq_loc& loc, CScope* scope)
{
    vector<CCdregionValidator::TShortIntron> shortlist;

    CSeq_loc_CI li(loc);

    TSeqPos last_start = li.GetRange().GetFrom();
    TSeqPos last_stop = li.GetRange().GetTo();
    CRef<CSeq_id> last_id(new CSeq_id());
    last_id->Assign(li.GetSeq_id());

    ++li;
    while (li) {
        TSeqPos this_start = li.GetRange().GetFrom();
        TSeqPos this_stop = li.GetRange().GetTo();
        if (abs ((int)this_start - (int)last_stop) < 11 || abs ((int)this_stop - (int)last_start) < 11) {
            if (li.GetSeq_id().Equals(*last_id)) {
                // definitely same bioseq, definitely report
                x_AddToIntronList(shortlist, last_start, last_stop, this_start, this_stop);
            } else if (scope) {
                // only report if definitely on same bioseq
                CBioseq_Handle last_bsh = scope->GetBioseqHandle(*last_id);
                if (last_bsh) {
                    for (auto id_it : last_bsh.GetId()) {
                        if (id_it.GetSeqId()->Equals(li.GetSeq_id())) {
                             x_AddToIntronList(shortlist, last_start, last_stop, this_start, this_stop);
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
    return shortlist;
}


string CCdregionValidator::x_FormatIntronInterval(const TShortIntron& interval)
{
    return NStr::NumericToString(interval.first + 1) + "-"
        + NStr::NumericToString(interval.second + 1);
}


void CCdregionValidator::ReportShortIntrons()
{
    if (m_Feat.IsSetExcept()) {
        return;
    }

    string message;

    vector<TShortIntron> shortlist = x_GetShortIntrons(m_Feat.GetLocation(), &m_Scope);
    if (shortlist.size() == 0) {
        return;
    }

    // only report if no nonsense introns
    vector<CRef<CSeq_loc> > nonsense_introns = CCDSTranslationProblems::GetNonsenseIntrons(m_Feat, m_Scope);
    if (nonsense_introns.size() > 0) {
        return;
    }

    if (shortlist.size() == 1) {
        message = x_FormatIntronInterval(shortlist.front());
    } else if (shortlist.size() == 2) {
        message = x_FormatIntronInterval(shortlist.front())
            + " and " +
            x_FormatIntronInterval(shortlist.back());
    } else {
        for (size_t i = 0; i < shortlist.size() - 2; i++) {
            message += x_FormatIntronInterval(shortlist[i]) + ", ";
        }
        message += " and " + x_FormatIntronInterval(shortlist.back());
    }
    PostErr(eDiag_Warning, eErr_SEQ_FEAT_ShortIntron,
                "Introns at positions " + message + " should be at least 10 nt long");
}


// non-pseudo CDS must have product
void CCdregionValidator::x_ValidateProductId()
{
    // bail if product exists
    if ( m_Feat.IsSetProduct() ) {
        return;
    }
    // bail if location has just stop
    if ( m_Feat.IsSetLocation() ) {
        const CSeq_loc& loc = m_Feat.GetLocation();
        if ( loc.IsPartialStart(eExtreme_Biological)  &&  !loc.IsPartialStop(eExtreme_Biological) ) {
            if ( GetLength(loc, &m_Scope) <= 5 ) {
                return;
            }
        }
    }
    // supress in case of the appropriate exception
    if ( m_Feat.IsSetExcept()  &&  m_Feat.IsSetExcept_text()  &&
         !NStr::IsBlank(m_Feat.GetExcept_text()) ) {
        if ( NStr::Find(m_Feat.GetExcept_text(),
            "rearrangement required for product") != NPOS ) {
           return;
        }
    }

    // non-pseudo CDS must have /product
    PostErr(eDiag_Error, eErr_SEQ_FEAT_MissingCDSproduct,
        "Expected CDS product absent");
}


void CCdregionValidator::x_ValidateConflict()
{
    if (!m_ProductBioseq) {
        return;
    }
    // translate the coding region
    string transl_prot;
    try {
        CSeqTranslator::Translate(m_Feat, m_Scope, transl_prot,
                                  false,   // do not include stop codons
                                  false);  // do not remove trailing X/B/Z

    } catch ( const runtime_error& ) {
    }

    CSeqVector prot_vec = m_ProductBioseq.GetSeqVector(CBioseq_Handle::eCoding_Iupac);
    prot_vec.SetCoding(CSeq_data::e_Ncbieaa);

    string prot_seq;
    prot_vec.GetSeqData(0, prot_vec.size(), prot_seq);

    if ( transl_prot.empty()  ||  prot_seq.empty()  ||  NStr::Equal(transl_prot, prot_seq) ) {
        PostErr(eDiag_Error, eErr_SEQ_FEAT_BadConflictFlag,
                "Coding region conflict flag should not be set");
    } else {
        PostErr(eDiag_Warning, eErr_SEQ_FEAT_ConflictFlagSet,
                "Coding region conflict flag is set");
    }
}


void CCdregionValidator::x_ValidateCommonProduct()
{
    if ( !m_Feat.IsSetProduct() ) {
        return;
    }

    const CCdregion& cdr = m_Feat.GetData().GetCdregion();
    if ( cdr.CanGetOrf() ) {
        return;
    }

    if ( !m_ProductBioseq  || m_ProductIsFar) {
        const CSeq_id* sid = 0;
        try {
            sid = &(GetId(m_Feat.GetProduct(), &m_Scope));
        } catch (const CObjmgrUtilException&) {}
        if (m_Imp.RequireLocalProduct(sid)) {
            PostErr(eDiag_Warning, eErr_SEQ_FEAT_MissingCDSproduct,
                "Unable to find product Bioseq from CDS feature");
        }
        return;
    }

    const CSeq_feat* sfp = GetCDSForProduct(m_ProductBioseq);
    if ( sfp == 0 ) {
        return;
    }

    if ( &m_Feat != sfp ) {
        // if genomic product set, with one cds on contig and one on cdna,
        // do not report.
        if ( m_Imp.IsGPS() ) {
            // feature packaging test will do final contig vs. cdna check
            CBioseq_Handle sfh = m_Scope.GetBioseqHandle(sfp->GetLocation());
            if ( m_LocationBioseq != sfh ) {
                return;
            }
        }
        PostErr(eDiag_Critical, eErr_SEQ_FEAT_MultipleCDSproducts,
            "Same product Bioseq from multiple CDS features");
    }
}


void CCdregionValidator::x_ValidateProductPartials()
{
    if (!m_ProductBioseq || !m_LocationBioseq) {
        return;
    }

    if (m_LocationBioseq.GetTopLevelEntry() != m_ProductBioseq.GetTopLevelEntry()) {
        return;
    }
    CFeat_CI prot(m_ProductBioseq, CSeqFeatData::eSubtype_prot);
    if (!prot) {
        return;
    }
    if (!PartialsSame(m_Feat.GetLocation(), prot->GetLocation())) {
        PostErr(eDiag_Warning, eErr_SEQ_FEAT_PartialsInconsistentCDSProtein,
            "Coding region and protein feature partials conflict");
    }
}


bool CCdregionValidator::x_CheckPosNOrGap(TSeqPos pos, const CSeqVector& vec)
{
    if (vec.IsInGap(pos) || vec[pos] == 'N') {
        return true;
    } else {
        return false;
    }
}


void CCdregionValidator::x_ValidateParentPartialness(const CSeq_loc& parent_loc, const string& parent_name)
{
    if (!m_LocationBioseq) {
        return;
    }

    bool check_gaps = false;
    if (m_LocationBioseq.IsSetInst() && m_LocationBioseq.GetInst().IsSetRepr() &&
        m_LocationBioseq.GetInst().GetRepr() == CSeq_inst::eRepr_delta) {
        check_gaps = true;
    }

    bool has_abutting_gap = false;
    bool is_minus_strand = m_Feat.GetLocation().IsSetStrand() && m_Feat.GetLocation().GetStrand() == eNa_strand_minus;

    if (m_Feat.GetLocation().IsPartialStart(eExtreme_Biological) && !parent_loc.IsPartialStart(eExtreme_Biological)) {

        if (check_gaps) {
            CSeqVector seq_vec(m_LocationBioseq);
            TSeqPos start = m_Feat.GetLocation().GetStart(eExtreme_Biological),
                    pos = is_minus_strand ? start + 1 : start - 1;

            if (pos < m_LocationBioseq.GetBioseqLength()) {
                has_abutting_gap = x_CheckPosNOrGap(pos, seq_vec);
            }
        }

        if (!has_abutting_gap) {
            EDiagSev sev = eDiag_Warning;
            CConstRef <CSeq_feat> gene = m_Gene;
            if (gene && gene->GetData().GetGene().IsSetLocus()) {
                string locus = gene->GetData().GetGene().GetLocus();
                if ( NStr::EqualNocase (locus, "orf1ab") ) {
                    sev = eDiag_Info;
                }
            }
            PostErr(sev, eErr_SEQ_FEAT_PartialProblemMismatch5Prime, parent_name + " should not be 5' complete if coding region is 5' partial");
        }
    }
    if (m_Feat.GetLocation().IsPartialStop(eExtreme_Biological) && !parent_loc.IsPartialStop(eExtreme_Biological)) {

        if (check_gaps) {

            CSeqVector seq_vec(m_LocationBioseq);
            TSeqPos stop = m_Feat.GetLocation().GetStop(eExtreme_Biological),
                    pos = is_minus_strand ? stop - 1 : stop + 1;

            if (pos < m_LocationBioseq.GetBioseqLength()) {
                has_abutting_gap = x_CheckPosNOrGap(pos, seq_vec);
            }
        }

        if (!has_abutting_gap) {
            EDiagSev sev = eDiag_Warning;
            CConstRef <CSeq_feat> gene = m_Gene;
            if (gene && gene->GetData().GetGene().IsSetLocus()) {
                string locus = gene->GetData().GetGene().GetLocus();
                if ( NStr::EqualNocase (locus, "orf1ab") ) {
                    sev = eDiag_Info;
                }
            }
            PostErr(sev, eErr_SEQ_FEAT_PartialProblemMismatch3Prime, parent_name + " should not be 3' complete if coding region is 3' partial");
        }
    }
}


void CCdregionValidator::x_ValidateParentPartialness()
{
    if (!m_Gene) {
        return;
    }
    x_ValidateParentPartialness(m_Gene->GetLocation(), "gene");

    CConstRef<CSeq_feat> mrna = GetmRNAforCDS(m_Feat, m_Scope);
    if (mrna) {
        TFeatScores contained_mrna;
        GetOverlappingFeatures(m_Gene->GetLocation(), CSeqFeatData::e_Rna,
            CSeqFeatData::eSubtype_mRNA, eOverlap_Contains, contained_mrna, m_Scope);
        if (contained_mrna.size() == 1) {
            // messy for alternate splicing, so only check if there is only one
            x_ValidateParentPartialness(mrna->GetLocation(), "mRNA");
        }
    }
}


END_SCOPE(validator)
END_SCOPE(objects)
END_NCBI_SCOPE
