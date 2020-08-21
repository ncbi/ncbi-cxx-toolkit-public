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
 * Author:  Colleen Bollin
 *
 * File Description:
 *   detecting translation problems
 *   .......
 *
 */
#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>
#include <corelib/ncbistr.hpp>
#include <objtools/validator/translation_problems.hpp>
#include <objtools/validator/utilities.hpp>

#include <objmgr/seq_vector.hpp>
#include <objmgr/util/sequence.hpp>
#include <objects/seqfeat/Cdregion.hpp>
#include <objects/seqfeat/Code_break.hpp>
#include <objects/seqfeat/Gb_qual.hpp>
#include <objects/seqfeat/Genetic_code.hpp>
#include <util/sequtil/sequtil_convert.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)
BEGIN_SCOPE(validator)
using namespace sequence;


CCDSTranslationProblems::CCDSTranslationProblems()
{
    x_Reset();
}


void CCDSTranslationProblems::x_Reset()
{
    m_ProblemFlags = 0;
    m_RaggedLength = 0;
    m_TransLen = 0;
    m_ProtLen = 0;
    m_HasDashXStart = false;
    m_TranslStart = 0;
    m_InternalStopCodons = 0;
    m_TranslTerminalX = 0;
    m_ProdTerminalX = 0;
    m_UnableToTranslate = false;
    m_AltStart = false;
    m_UnparsedTranslExcept = false;
    m_NumNonsenseIntrons = 0;
    m_HasException = false;
}


void CCDSTranslationProblems::CalculateTranslationProblems
(const CSeq_feat& feat,
CBioseq_Handle loc_handle,
CBioseq_Handle prot_handle,
bool ignore_exceptions,
bool far_fetch_cds,
bool standalone_annot,
bool single_seq,
bool is_gpipe,
bool is_genomic,
bool is_refseq,
bool is_nt_or_ng_or_nw,
bool is_nc,
bool has_accession,
CScope* scope)
{
    x_Reset();
    // bail if not CDS
    if (!feat.GetData().IsCdregion()) {
        return;
    }

    // do not validate for pseudo gene
    if (feat.IsSetQual()) {
        for (auto it = feat.GetQual().begin(); it != feat.GetQual().end(); it++) {
             if ((*it)->IsSetQual() && NStr::EqualNocase((*it)->GetQual(), "pseudo")) {
                return;
            }
        }
    }

    bool has_errors = false, unclassified_except = false,
        mismatch_except = false, frameshift_except = false,
        rearrange_except = false, product_replaced = false,
        mixed_population = false, low_quality = false,
        report_errors = true, other_than_mismatch = false,
        rna_editing = false, transcript_or_proteomic = false;
    string farstr;

    if (!ignore_exceptions &&
        feat.IsSetExcept() && feat.GetExcept() &&
        feat.IsSetExcept_text()) {
        const string& except_text = feat.GetExcept_text();
        report_errors = ReportTranslationErrors(except_text);
        x_GetExceptionFlags(except_text,
            unclassified_except,
            mismatch_except,
            frameshift_except,
            rearrange_except,
            product_replaced,
            mixed_population,
            low_quality,
            rna_editing,
            transcript_or_proteomic);
    }

    m_HasException = !report_errors;

    // check frame
    m_ProblemFlags |= x_CheckCDSFrame(feat, scope);

    // check for unparsed transl_except
    if (feat.IsSetQual()) {
        for (auto it = feat.GetQual().begin(); it != feat.GetQual().end(); it++) {
            if ((*it)->IsSetQual() && NStr::Equal((*it)->GetQual(), "transl_except")) {
                m_UnparsedTranslExcept = true;
            }
        }
    }

    string transl_prot;   // translated protein
    bool got_stop = false;

    try {
        transl_prot = TranslateCodingRegionForValidation(feat, *scope, m_AltStart);
    } catch (CException&) {
        m_UnableToTranslate = true;
    }

    if (NStr::EndsWith(transl_prot, "*")) {
        got_stop = true;
    }

    if (HasBadStartCodon(feat.GetLocation(), transl_prot)) {
        m_TranslStart = transl_prot.c_str()[0];
        m_ProblemFlags |= eCDSTranslationProblem_BadStart;
    }

    bool no_product = true;

    if (!m_UnableToTranslate) {

        // check for code break not on a codon
        m_TranslExceptProblems = x_GetTranslExceptProblems(feat, loc_handle, scope, is_refseq);

        m_NumNonsenseIntrons = x_CountNonsenseIntrons(feat, scope);
        if (x_ProteinHasTooManyXs(transl_prot)) {
            m_ProblemFlags |= eCDSTranslationProblem_TooManyX;
        }

        m_InternalStopCodons = CountInternalStopCodons(transl_prot);
        if (m_InternalStopCodons >  5) {
            // stop checking if too many stop codons
            return;
        }

        // get protein sequence

        if (!prot_handle) {
            const CSeq_id* protid = 0;
            try {
                protid = &GetId(feat.GetProduct(), scope);
            } catch (CException&) {}
            if (protid && far_fetch_cds && feat.IsSetProduct()) {
                m_ProblemFlags |= eCDSTranslationProblem_UnableToFetch;
            } else if (protid && (!far_fetch_cds || feat.IsSetProduct())) {
                // don't report missing protein sequence
            } else if (!standalone_annot && transl_prot.length() > 6) {
                if (!is_nt_or_ng_or_nw && (!is_nc || !single_seq)) {
                    m_ProblemFlags |= eCDSTranslationProblem_NoProtein;
                }
            }
        }

        bool show_stop = true;

        if (prot_handle && prot_handle.IsAa()) {
            no_product = false;

            CSeqVector prot_vec = prot_handle.GetSeqVector();
            prot_vec.SetCoding(CSeq_data::e_Ncbieaa);


            CalculateEffectiveTranslationLengths(transl_prot, prot_vec, m_TransLen, m_ProtLen);

            if (m_TransLen == m_ProtLen || has_accession)  {                // could be identical
                if (prot_vec.size() > 0 && transl_prot.length() > 0 &&
                    prot_vec[0] != transl_prot[0]) {
                    bool no_beg, no_end;
                    FeatureHasEnds(feat, scope, no_beg, no_end);
                    if (feat.IsSetPartial() && feat.GetPartial() && (!no_beg) && (!no_end)) {
                        m_ProblemFlags |= eCDSTranslationProblem_ShouldStartPartial;
                    } else if (transl_prot[0] == '-' || transl_prot[0] == 'X') {
                        m_HasDashXStart = true;
                    }
                }
                m_Mismatches = x_GetTranslationMismatches(feat, prot_vec, transl_prot, has_accession);
            }

            if (feat.CanGetPartial() && feat.GetPartial() &&
                m_Mismatches.size() == 0) {
                bool no_beg, no_end;
                FeatureHasEnds(feat, scope, no_beg, no_end);
                if (!no_beg  && !no_end) {
                    if (report_errors) {
                        if (is_gpipe && is_genomic) {
                            // suppress in gpipe genomic
                        } else {
                            if (!got_stop) {
                                m_ProblemFlags |= eCDSTranslationProblem_ShouldBePartialButIsnt;
                            } else {
                                m_ProblemFlags |= eCDSTranslationProblem_ShouldNotBePartialButIs;
                            }
                        }
                    }
                    show_stop = false;
                }
            }

            if (!transl_prot.empty()) {
                // look for discrepancy in number of terminal Xs between product and translation
                m_TranslTerminalX = x_CountTerminalXs(transl_prot, (got_stop && (transl_prot.length() == prot_vec.size() + 1)));
                m_ProdTerminalX = x_CountTerminalXs(prot_vec);

            }

        }

        x_GetCdTransErrors(feat, prot_handle, show_stop, got_stop, scope);

    }

    if (x_JustifiesException()) {
        has_errors = true;
        other_than_mismatch = true;
    } else if (m_Mismatches.size() > 0) {
        has_errors = true;
    }

    if (!report_errors && !no_product) {
        if (!has_errors) {
            if (!frameshift_except && !rearrange_except && !mixed_population && !low_quality) {
                m_ProblemFlags |= eCDSTranslationProblem_UnnecessaryException;
            }
        } else if (unclassified_except && !other_than_mismatch) {
            if (m_Mismatches.size() * 50 <= m_ProtLen) {
                m_ProblemFlags |= eCDSTranslationProblem_ErroneousException;
            }
        } else if (product_replaced) {
            m_ProblemFlags |= eCDSTranslationProblem_UnqualifiedException;
        }
    }

}


void CCDSTranslationProblems::x_GetCdTransErrors(const CSeq_feat& feat, CBioseq_Handle product, bool show_stop, bool got_stop, CScope* scope)
{
    bool no_beg, no_end;
    FeatureHasEnds(feat, product ? &(product.GetScope()) : scope, no_beg, no_end);
    if (show_stop) {
        if (!got_stop  && !no_end) {
            m_ProblemFlags |= eCDSTranslationProblem_NoStop;
        } else if (got_stop  &&  no_end) {
            m_ProblemFlags |= eCDSTranslationProblem_StopPartial;
        } else if (got_stop  &&  !no_end) {
            m_RaggedLength = x_CheckForRaggedEnd(feat, scope);
        }
    }
}

bool CCDSTranslationProblems::x_IsThreeBaseNonsense(
    const CSeq_feat& feat,
    const CSeq_id& id,
    const CCdregion& cdr,
    TSeqPos start,
    TSeqPos stop,
    ENa_strand strand,
    CScope *scope
)

{
    bool nonsense_intron = false;
    CRef<CSeq_feat> tmp_cds (new CSeq_feat());

    tmp_cds->SetLocation().SetInt().SetFrom(start);
    tmp_cds->SetLocation().SetInt().SetTo(stop);
    tmp_cds->SetLocation().SetInt().SetStrand(strand);
    tmp_cds->SetLocation().SetInt().SetId().Assign(id);

    tmp_cds->SetLocation().SetPartialStart(true, eExtreme_Biological);
    tmp_cds->SetLocation().SetPartialStop(true, eExtreme_Biological);
    tmp_cds->SetData().SetCdregion();
    if ( cdr.IsSetCode()) {
        tmp_cds->SetData().SetCdregion().SetCode().Assign(cdr.GetCode());
    }

    bool alt_start = false;
    try {
        string transl_prot = TranslateCodingRegionForValidation(*tmp_cds, *scope, alt_start);

        if (NStr::Equal(transl_prot, "*")) {
            nonsense_intron = true;
        }
    } catch (CException&) {
    }
    return nonsense_intron;
}


size_t CCDSTranslationProblems::x_CountNonsenseIntrons(const CSeq_feat& feat, CScope* scope)
{
    TSeqPos last_start = 0, last_stop = 0, start, stop;

    if (!feat.IsSetData() && !feat.GetData().IsCdregion()) {
        return 0;
    }
    if (feat.IsSetExcept() || feat.IsSetExcept_text()) return 0;
    const CCdregion& cdr = feat.GetData().GetCdregion();
    if (cdr.IsSetCode_break()) return 0;

    size_t count = 0;
    const CSeq_loc& loc = feat.GetLocation();

    CSeq_loc_CI prev;
    for (CSeq_loc_CI curr(loc); curr; ++curr) {
        start = curr.GetRange().GetFrom();
        stop = curr.GetRange().GetTo();
        if (prev  &&  curr  && IsSameBioseq(curr.GetSeq_id(), prev.GetSeq_id(), scope)) {
            ENa_strand strand = curr.GetStrand();
            if (strand == eNa_strand_minus) {
                if (last_start - stop == 4) {
                    if (x_IsThreeBaseNonsense(feat, curr.GetSeq_id(), cdr, stop + 1, last_start - 1, strand, scope)) {
                        count++;
                    }
                }
            } else {
                if (start - last_stop == 4) {
                    if (x_IsThreeBaseNonsense(feat, curr.GetSeq_id(), cdr, last_stop + 1, start - 1, strand, scope)) {
                        count++;
                    }
                }
            }
        }
        last_start = start;
        last_stop = stop;
        prev = curr;
    }

    if (count > 0 && sequence::IsPseudo(feat, *scope)) return 0;
    else return count;
}


vector<CRef<CSeq_loc> > CCDSTranslationProblems::GetNonsenseIntrons(const CSeq_feat& feat, CScope& scope)
{
    vector<CRef<CSeq_loc> > intron_locs;

    TSeqPos last_start = 0, last_stop = 0, start, stop;

    if (!feat.IsSetData() && !feat.GetData().IsCdregion()) {
        return intron_locs;
    }
    if (feat.IsSetExcept() || feat.IsSetExcept_text()) return intron_locs;
    const CCdregion& cdr = feat.GetData().GetCdregion();
    if (cdr.IsSetCode_break()) return intron_locs;

    const CSeq_loc& loc = feat.GetLocation();

    CSeq_loc_CI prev;
    for (CSeq_loc_CI curr(loc); curr; ++curr) {
        start = curr.GetRange().GetFrom();
        stop = curr.GetRange().GetTo();
        if (prev  &&  curr  && IsSameBioseq(curr.GetSeq_id(), prev.GetSeq_id(), &scope)) {
            ENa_strand strand = curr.GetStrand();
            if (strand == eNa_strand_minus) {
                if (last_start - stop == 4) {
                    if (x_IsThreeBaseNonsense(feat, curr.GetSeq_id(), cdr, stop + 1, last_start - 1, strand, &scope)) {         
                        CRef<CSeq_id> id(new CSeq_id());
                        id->Assign(curr.GetSeq_id());
                        CRef<CSeq_loc> intron_loc(new CSeq_loc(*id, stop + 1, last_start - 1, strand));
                        intron_locs.push_back(intron_loc);
                    }
                }
            } else {
                if (start - last_stop == 4) {
                    if (x_IsThreeBaseNonsense(feat, curr.GetSeq_id(), cdr, last_stop + 1, start - 1, strand, &scope)) {
                        CRef<CSeq_id> id(new CSeq_id());
                        id->Assign(curr.GetSeq_id());
                        CRef<CSeq_loc> intron_loc(new CSeq_loc(*id, last_stop + 1, start - 1, strand));
                        intron_locs.push_back(intron_loc);
                    }
                }
            }
        }
        last_start = start;
        last_stop = stop;
        prev = curr;
    }

    if (intron_locs.size() > 0 && sequence::IsPseudo(feat, scope)) {
        intron_locs.clear();
    }
    return intron_locs;

}


bool CCDSTranslationProblems::x_ProteinHasTooManyXs(const string& transl_prot)
{
    size_t num_x = 0, num_nonx = 0;

    ITERATE(string, it, transl_prot) {
        if (*it == 'X') {
            num_x++;
        } else {
            num_nonx++;
        }
    }

    // report too many Xs
    if (num_x > num_nonx) {
        return true;
    } else {
        return false;
    }
}


void CCDSTranslationProblems::x_GetExceptionFlags
(const string& except_text,
 bool& unclassified_except,
 bool& mismatch_except,
 bool& frameshift_except,
 bool& rearrange_except,
 bool& product_replaced,
 bool& mixed_population,
 bool& low_quality,
 bool& rna_editing,
 bool& transcript_or_proteomic)
{
    if (NStr::FindNoCase(except_text, "unclassified translation discrepancy") != NPOS) {
        unclassified_except = true;
    }
    if (NStr::FindNoCase(except_text, "mismatches in translation") != NPOS) {
        mismatch_except = true;
    }
    if (NStr::FindNoCase(except_text, "artificial frameshift") != NPOS) {
        frameshift_except = true;
    }
    if (NStr::FindNoCase(except_text, "rearrangement required for product") != NPOS) {
        rearrange_except = true;
    }
    if (NStr::FindNoCase(except_text, "translated product replaced") != NPOS) {
        product_replaced = true;
    }
    if (NStr::FindNoCase(except_text, "heterogeneous population sequenced") != NPOS) {
        mixed_population = true;
    }
    if (NStr::FindNoCase(except_text, "low-quality sequence region") != NPOS) {
        low_quality = true;
    }
    if (NStr::FindNoCase (except_text, "RNA editing") != NPOS) {
        rna_editing = true;
    }
}


size_t CCDSTranslationProblems::x_CheckCDSFrame(const CSeq_feat& feat, CScope* scope)
{
    size_t rval = 0;

    const CCdregion& cdregion = feat.GetData().GetCdregion();
    const CSeq_loc& location = feat.GetLocation();
    unsigned int part_loc = SeqLocPartialCheck(location, scope);
    string comment_text = "";
    if (feat.IsSetComment()) {
        comment_text = feat.GetComment();
    }

    // check frame
    if (cdregion.IsSetFrame()
        && (cdregion.GetFrame() == CCdregion::eFrame_two || cdregion.GetFrame() == CCdregion::eFrame_three)) {
        // coding region should be 5' partial
        if (!(part_loc & eSeqlocPartial_Start)) {
            rval |= eCDSTranslationProblem_FrameNotPartial;
        } else if ((part_loc & eSeqlocPartial_Start) && !x_Is5AtEndSpliceSiteOrGap(location, *scope)) {
            if (s_PartialAtGapOrNs(scope, location, eSeqlocPartial_Nostart, true)
                || NStr::Find(comment_text, "coding region disrupted by sequencing gap") != string::npos) {
                // suppress
            } else {
                rval |= eCDSTranslationProblem_FrameNotConsensus;
            }
        }
    }
    return rval;
}


bool CCDSTranslationProblems::x_JustifiesException() const
{
    if (m_ProblemFlags & eCDSTranslationProblem_FrameNotPartial ||
        m_ProblemFlags & eCDSTranslationProblem_FrameNotConsensus ||
        m_ProblemFlags & eCDSTranslationProblem_NoStop ||
        m_ProblemFlags & eCDSTranslationProblem_StopPartial ||
        m_ProblemFlags & eCDSTranslationProblem_PastStop ||
        m_ProblemFlags & eCDSTranslationProblem_ShouldStartPartial ||
        m_ProblemFlags & eCDSTranslationProblem_BadStart ||
        m_ProblemFlags & eCDSTranslationProblem_NoProtein ||
        m_ProtLen != m_TransLen ||
        m_InternalStopCodons > 0 ||
        m_RaggedLength > 0 || m_HasDashXStart ||
        m_UnableToTranslate) {
        return true;
    } else if (x_JustifiesException(m_TranslExceptProblems)) {
        return true;
    } else {
        return false;
    }
}


size_t CCDSTranslationProblems::x_CountTerminalXs(const string& transl_prot, bool skip_stop)
{
    // look for discrepancy in number of terminal Xs between product and translation
    size_t transl_terminal_x = 0;
    size_t i = transl_prot.length() - 1;
    if (i > 0 && transl_prot[i] == '*' && skip_stop) {
        i--;
    }
    while (i > 0) {
        if (transl_prot[i] == 'X') {
            transl_terminal_x++;
        } else {
            break;
        }
        i--;
    }
    if (i == 0 && transl_prot[0] == 'X') {
        transl_terminal_x++;
    }
    return transl_terminal_x;
}

size_t CCDSTranslationProblems::x_CountTerminalXs(const CSeqVector& prot_vec)
{
    size_t prod_terminal_x = 0;
    TSeqPos prod_len = prot_vec.size() - 1;
    while (prod_len > 0 && prot_vec[prod_len] == 'X') {
        prod_terminal_x++;
        prod_len--;
    }
    if (prod_len == 0 && prot_vec[prod_len] == 'X') {
        prod_terminal_x++;
    }
    return prod_terminal_x;
}


CCDSTranslationProblems::TTranslationMismatches
CCDSTranslationProblems::x_GetTranslationMismatches(const CSeq_feat& feat, const CSeqVector& prot_vec, const string& transl_prot, bool has_accession)
{
    TTranslationMismatches mismatches;
    size_t prot_len;
    size_t len;

    CalculateEffectiveTranslationLengths(transl_prot, prot_vec, len, prot_len);

    if (len == prot_len || has_accession)  {                // could be identical
        if (len > prot_len) {
            len = prot_len;
        }
        for (TSeqPos i = 0; i < len; ++i) {
            CSeqVectorTypes::TResidue p_res = prot_vec[i];
            CSeqVectorTypes::TResidue t_res = transl_prot[i];

            if (t_res != p_res) {
                if (i == 0) {
                    bool no_beg, no_end;
                    FeatureHasEnds(feat, &(prot_vec.GetScope()), no_beg, no_end);
                    if (feat.IsSetPartial() && feat.GetPartial() && (!no_beg) && (!no_end)) {
                    } else if (t_res == '-') {
                    } else {
                        mismatches.push_back({ p_res, t_res, i });
                    }
                } else {
                    mismatches.push_back({ p_res, t_res, i });
                }
            }
        }
    }
    return mismatches;
}





static bool x_LeuCUGstart
(
    const CSeq_feat& feat
)

{
    if ( ! feat.IsSetExcept()) return false;
    if ( ! feat.IsSetExcept_text()) return false;
    const string& except_text = feat.GetExcept_text();
    if (NStr::FindNoCase(except_text, "translation initiation by tRNA-Leu at CUG codon") == NPOS) return false;
    if (feat.IsSetQual()) {
        for (auto it = feat.GetQual().begin(); it != feat.GetQual().end(); it++) {
            const CGb_qual& qual = **it;
            if (qual.IsSetQual() && NStr::Compare(qual.GetQual(), "experiment") == 0) return true;
        }
    }
    return false;
}


CCDSTranslationProblems::TTranslExceptProblems 
CCDSTranslationProblems::x_GetTranslExceptProblems
(const CSeq_feat& feat, CBioseq_Handle loc_handle, CScope* scope, bool is_refseq)
{
    TTranslExceptProblems problems;

    if (!feat.IsSetData() || !feat.GetData().IsCdregion() || !feat.GetData().GetCdregion().IsSetCode_break()) {
        return problems;
    }

    TSeqPos len = GetLength(feat.GetLocation(), scope);
    bool alt_start = false;

    // need to translate a version of the coding region without the code breaks,
    // to see if the code breaks are necessary
    const CCdregion& cdregion = feat.GetData().GetCdregion();
    CRef<CSeq_feat> tmp_cds(new CSeq_feat());
    tmp_cds->SetLocation().Assign(feat.GetLocation());
    tmp_cds->SetData().SetCdregion();
    if (cdregion.IsSetFrame()) {
        tmp_cds->SetData().SetCdregion().SetFrame(cdregion.GetFrame());
    }
    if (cdregion.IsSetCode()) {
        tmp_cds->SetData().SetCdregion().SetCode().Assign(cdregion.GetCode());
    }

    // now, will use tmp_cds to translate individual code breaks;
    tmp_cds->SetData().SetCdregion().ResetFrame();

    const CSeq_loc& loc = feat.GetLocation();

    for (auto cbr = cdregion.GetCode_break().begin(); cbr != cdregion.GetCode_break().end(); cbr++) {
        if (!(*cbr)->IsSetLoc()) {
            continue;
        }
        // if the code break is outside the coding region, skip
        ECompare comp = Compare((*cbr)->GetLoc(), loc,
            scope, fCompareOverlapping);
        if ((comp != eContained) && (comp != eSame)) {
            continue;
        }

        TSeqPos codon_length = GetLength((*cbr)->GetLoc(), scope);
        TSeqPos from = LocationOffset(loc, (*cbr)->GetLoc(),
            eOffset_FromStart, scope);

        TSeqPos from_end = LocationOffset(loc, (*cbr)->GetLoc(),
            eOffset_FromEnd, scope);

        TSeqPos to = from + codon_length - 1;

        // check for code break not on a codon
        if (codon_length == 3 ||
            ((codon_length == 1 || codon_length == 2) && to == len - 1)) {
            size_t start_pos;
            switch (cdregion.GetFrame()) {
            case CCdregion::eFrame_two:
                start_pos = 1;
                break;
            case CCdregion::eFrame_three:
                start_pos = 2;
                break;
            default:
                start_pos = 0;
                break;
            }
            if ((from % 3) != start_pos) {
                problems.push_back({ eTranslExceptPhase, 0, 0 });
            }
        }
        if ((*cbr)->IsSetAa() && (*cbr)->IsSetLoc()) {
            tmp_cds->SetLocation().Assign((*cbr)->GetLoc());
            tmp_cds->SetLocation().SetPartialStart(true, eExtreme_Biological);
            tmp_cds->SetLocation().SetPartialStop(true, eExtreme_Biological);
            string cb_trans = "";
            try {
                CSeqTranslator::Translate(*tmp_cds, *scope, cb_trans,
                    true,   // include stop codons
                    false,  // do not remove trailing X/B/Z
                    &alt_start);
            } catch (CException&) {
            }
            size_t prot_pos = from / 3;

            unsigned char ex = 0;
            vector<char> seqData;
            string str = "";
            bool not_set = false;

            switch ((*cbr)->GetAa().Which()) {
            case CCode_break::C_Aa::e_Ncbi8aa:
                str = (*cbr)->GetAa().GetNcbi8aa();
                CSeqConvert::Convert(str, CSeqUtil::e_Ncbi8aa, (TSeqPos)0, (TSeqPos)(str.size()), seqData, CSeqUtil::e_Ncbieaa);
                ex = seqData[0];
                break;
            case CCode_break::C_Aa::e_Ncbistdaa:
                str = (*cbr)->GetAa().GetNcbi8aa();
                CSeqConvert::Convert(str, CSeqUtil::e_Ncbistdaa, (TSeqPos)0, (TSeqPos)(str.size()), seqData, CSeqUtil::e_Ncbieaa);
                ex = seqData[0];
                break;
            case CCode_break::C_Aa::e_Ncbieaa:
                seqData.push_back((*cbr)->GetAa().GetNcbieaa());
                ex = seqData[0];
                break;
            default:
                // do nothing, code break wasn't actually set
                not_set = true;
                break;
            }

            if (!not_set) {
                string except_char = "";
                except_char += ex;

                //At the beginning of the CDS
                if (prot_pos == 0 && ex != 'M') {
                    if (prot_pos == 0 && ex == 'L' && x_LeuCUGstart(feat) && is_refseq) {
                        /* do not warn on explicitly documented unusual translation initiation at CUG without initiator tRNA-Met */
                    } else if ((!feat.IsSetPartial()) || (!feat.GetPartial())) {
                        problems.push_back({ eTranslExceptSuspicious, ex, prot_pos });
                    }
                }

                // Anywhere in CDS, where exception has no effect
                if (from_end > 0) {
                    if (from_end < 2 && NStr::Equal(except_char, "*")) {
                        // this is a necessary terminal transl_except
                    } else if (NStr::EqualNocase(cb_trans, except_char)) {
                        if (prot_pos == 0 && ex == 'L' && x_LeuCUGstart(feat) && is_refseq) {
                            /* do not warn on explicitly documented unusual translation initiation at CUG without initiator tRNA-Met */
                        } else {
                            problems.push_back({ eTranslExceptUnnecessary, ex, prot_pos });
                        }
                    }
                } else if (!NStr::Equal(except_char, "*"))
                {
                    if (NStr::Equal(cb_trans, except_char) ||
                        !loc.IsPartialStop(eExtreme_Biological))
                    {
                        const CGenetic_code  *gcode;
                        CBioseq_Handle       bsh;
                        CSeqVector           vec;
                        TSeqPos              from1;
                        string               p;
                        string               q;
                        bool                 altst;

                        vec = loc_handle.GetSeqVector(CBioseq_Handle::eCoding_Iupac);
                        altst = false;
                        from1 = (*cbr)->GetLoc().GetStart(eExtreme_Biological);
                        vec.GetSeqData(from1, from1 + 3, q);

                        if (cdregion.CanGetCode())
                            gcode = &cdregion.GetCode();
                        else
                            gcode = NULL;

                        CSeqTranslator::Translate(q, p,
                            CSeqTranslator::fIs5PrimePartial,
                            gcode, &altst);

                        if (!NStr::Equal(except_char, "*")) {
                            problems.push_back({ eTranslExceptUnexpected, ex, prot_pos });
                        }
                    }
                } else
                {
                    /*bsv
                    Stop codon here. Needs a check for abutting tRNA feature.
                    bsv*/
                }
            }
        }
    }
    return problems;
}


bool CCDSTranslationProblems::x_JustifiesException(const TTranslExceptProblems& problems)
{
    for (auto it = problems.begin(); it != problems.end(); it++) {
        if (it->problem == eTranslExceptPhase) {
            return true;
        }
    }
    return false;
}


bool CCDSTranslationProblems::x_Is5AtEndSpliceSiteOrGap(const CSeq_loc& loc, CScope& scope)
{
    CSeq_loc_CI loc_it(loc);
    if (!loc_it) {
        return false;
    }
    CConstRef<CSeq_loc> rng = loc_it.GetRangeAsSeq_loc();
    if (!rng) {
        return false;
    }

    TSeqPos end = rng->GetStart(eExtreme_Biological);
    const CBioseq_Handle & bsh = scope.GetBioseqHandle(*rng);
    if (!bsh) {
        return false;
    }

    ENa_strand strand = rng->GetStrand();
    if (strand == eNa_strand_minus) {
          TSeqPos seq_len = bsh.GetBioseqLength();
          if (end < seq_len - 1) {
              CSeqVector vec = bsh.GetSeqVector (CBioseq_Handle::eCoding_Iupac);
              if (vec.IsInGap(end + 1)) {
                  if (vec.IsInGap (end)) {
                      // not ok - location overlaps gap
                      return false;
                  } else {
                      // ok, location abuts gap
                  }
              } else if (end < seq_len - 2 && IsResidue (vec[end + 1]) && ConsistentWithC(vec[end + 1])
                         && IsResidue (vec[end + 2]) && ConsistentWithT(vec[end + 2])) {
                  // it's ok, it's abutting the reverse complement of AG
              } else {
                  return false;
              }
          } else {
              // it's ok, location endpoint is at the 3' end
          }
    } else {
          if (end > 0 && end < bsh.GetBioseqLength()) {
              CSeqVector vec = bsh.GetSeqVector (CBioseq_Handle::eCoding_Iupac);
              if (vec.IsInGap(end - 1)) {
                  if (vec.IsInGap (end)) {
                      // not ok - location overlaps gap
                      return false;
                  } else {
                      // ok, location abuts gap
                  }
              } else {
                  if (end > 1 && IsResidue (vec[end - 1]) && ConsistentWithG(vec[end - 1])
                      && IsResidue(vec[end - 2]) && ConsistentWithA(vec[end - 2])) {
                      //it's ok, it's abutting "AG"
                  } else {
                      return false;
                  }
              }
          } else {
              // it's ok, location endpoint is at the 5' end
          }
    }   
    return true;
}


int CCDSTranslationProblems::x_CheckForRaggedEnd(const CSeq_feat& feat, CScope* scope)
{
    int ragged = 0;
    if (!feat.IsSetData() || !feat.GetData().IsCdregion() || !feat.IsSetLocation()) {
        return 0;
    }
    unsigned int part_loc = SeqLocPartialCheck(feat.GetLocation(), scope);
    if (feat.IsSetProduct()) {
        unsigned int part_prod = SeqLocPartialCheck(feat.GetProduct(), scope);
        if ((part_loc & eSeqlocPartial_Stop) ||
            (part_prod & eSeqlocPartial_Stop)) {
            // not ragged
        } else {
            // complete stop, so check for ragged end
            ragged = x_CheckForRaggedEnd(feat.GetLocation(), feat.GetData().GetCdregion(), scope);
        }
    }
    return ragged;
}


int CCDSTranslationProblems::x_CheckForRaggedEnd
(const CSeq_loc& loc,
const CCdregion& cdregion,
CScope* scope)
{
    size_t len = GetLength(loc, scope);
    if (cdregion.GetFrame() > CCdregion::eFrame_one) {
        len -= cdregion.GetFrame() - 1;
    }

    int ragged = len % 3;
    if (ragged > 0) {
        len = GetLength(loc, scope);
        size_t last_pos = 0;

        CSeq_loc::TRange range = CSeq_loc::TRange::GetEmpty();
        if (cdregion.IsSetCode_break()) {
            for (auto cbr = cdregion.GetCode_break().begin(); cbr != cdregion.GetCode_break().end(); cbr++) {
                SRelLoc rl(loc, (*cbr)->GetLoc(), scope);
                ITERATE(SRelLoc::TRanges, rit, rl.m_Ranges) {
                    if ((*rit)->GetTo() > last_pos) {
                        last_pos = (*rit)->GetTo();
                    }
                }
            }
        }

        // allowing a partial codon at the end
        TSeqPos codon_length = range.GetLength();
        if ((codon_length == 0 || codon_length == 1) &&
            last_pos == len - 1) {
            ragged = 0;
        }
    }
    return ragged;
}


typedef enum {
    eMRNAExcept_Biological = 1,
    eMRNAExcept_RNAEditing = 2,
    eMRNAExcept_Unclassified = 4,
    eMRNAExcept_Mismatch = 8,
    eMRNAExcept_ProductReplaced = 16
} EMRNAException;
static const char* const sc_BypassMrnaTransCheckText[] = {
    "RNA editing",
    "adjusted for low-quality genome",
    "annotated by transcript or proteomic data",
    "artificial frameshift",
    "reasons given in citation",
    "transcribed product replaced",
    "unclassified transcription discrepancy",
};
typedef CStaticArraySet<const char*, PCase_CStr> TBypassMrnaTransCheckSet;
DEFINE_STATIC_ARRAY_MAP(TBypassMrnaTransCheckSet, sc_BypassMrnaTransCheck, sc_BypassMrnaTransCheckText);


size_t InterpretMrnaException(const string& except_text)
{
    size_t rval = 0;

    ITERATE(TBypassMrnaTransCheckSet, it, sc_BypassMrnaTransCheck) {
        if (NStr::FindNoCase(except_text, *it) != NPOS) {
            rval |= eMRNAExcept_Biological;
        }
    }
    if (NStr::FindNoCase(except_text, "RNA editing") != NPOS) {
        rval |= eMRNAExcept_RNAEditing;
    }
    if (NStr::FindNoCase(except_text, "unclassified transcription discrepancy") != NPOS) {
        rval |= eMRNAExcept_Unclassified;
    }
    if (NStr::FindNoCase(except_text, "mismatches in transcription") != NPOS) {
        rval |= eMRNAExcept_Mismatch;
    }
    if (NStr::FindNoCase(except_text, "transcribed product replaced") != NPOS) {
        rval |= eMRNAExcept_ProductReplaced;
    }
    return rval;
}

size_t GetMRNATranslationProblems
(const CSeq_feat& feat, 
 size_t& mismatches, 
 bool ignore_exceptions, 
 CBioseq_Handle nuc,
 CBioseq_Handle rna,
 bool far_fetch,
 bool is_gpipe,
 bool is_genomic,
 CScope* scope)
{
    size_t rval = 0;
    mismatches = 0;
    if (feat.CanGetPseudo() && feat.GetPseudo()) {
        return rval;
    }
    if (!feat.CanGetProduct()) {
        return rval;
    }

    size_t exception_flags = 0;

    if (!ignore_exceptions &&
        feat.CanGetExcept() && feat.GetExcept() &&
        feat.CanGetExcept_text()) {
        exception_flags = InterpretMrnaException(feat.GetExcept_text());
    }
    bool has_errors = false, other_than_mismatch = false;
    bool report_errors = !(exception_flags & eMRNAExcept_Biological) || (exception_flags & eMRNAExcept_Mismatch);

    CConstRef<CSeq_id> product_id;
    try {
        product_id.Reset(&GetId(feat.GetProduct(), scope));
    } catch (CException&) {
    }
    if (!product_id) {
        return rval;
    }

    if (!nuc) {
        if (exception_flags & eMRNAExcept_Unclassified) {
            rval |= eMRNAProblem_TransFail;
        }
        return rval;
    }

    size_t total = 0;

    // note - exception will be thrown when creating CSeqVector if wrong type set for bioseq seq-data
    try {
        if (nuc) {

            if (!rna) {
                if (far_fetch) {
                    rval |= eMRNAProblem_UnableToFetch;
                }
                return rval;
            }

            _ASSERT(nuc  &&  rna);

            CSeqVector nuc_vec(feat.GetLocation(), *scope,
                CBioseq_Handle::eCoding_Iupac);
            CSeqVector rna_vec(rna, CBioseq_Handle::eCoding_Iupac);

            TSeqPos nuc_len = nuc_vec.size();
            TSeqPos rna_len = rna_vec.size();

            if (nuc_len != rna_len) {
                has_errors = true;
                other_than_mismatch = true;
                if (nuc_len < rna_len) {
                    size_t count_a = 0, count_no_a = 0;
                    // count 'A's in the tail
                    for (CSeqVector_CI iter(rna_vec, nuc_len); iter; ++iter) {
                        if ((*iter == 'A') || (*iter == 'a')) {
                            ++count_a;
                        } else {
                            ++count_no_a;
                        }
                    }
                    if (count_a < (19 * count_no_a)) { // less then 5%
                        if (report_errors || (exception_flags & eMRNAExcept_RNAEditing)) {
                            rval |= eMRNAProblem_TranscriptLenLess;
                        }
                    } else if (count_a > 0 && count_no_a == 0) {
                        has_errors = true;
                        other_than_mismatch = true;
                        if (report_errors || (exception_flags & eMRNAExcept_RNAEditing)) {
                            if (is_gpipe && is_genomic) {
                                // suppress
                            } else {
                                rval |= eMRNAProblem_PolyATail100;
                            }
                        }
                    } else {
                        if (report_errors) {
                            rval |= eMRNAProblem_PolyATail95;
                        }
                    }
                    // allow base-by-base comparison on common length
                    rna_len = nuc_len = min(nuc_len, rna_len);

                } else {
                    if (report_errors) {
                        rval |= eMRNAProblem_TranscriptLenMore;
                    }
                }
            }

            if (rna_len == nuc_len && nuc_len > 0) {
                CSeqVector_CI nuc_ci(nuc_vec);
                CSeqVector_CI rna_ci(rna_vec);

                // compare content of common length
                while ((nuc_ci  &&  rna_ci) && (nuc_ci.GetPos() < nuc_len)) {
                    if (*nuc_ci != *rna_ci) {
                        ++mismatches;
                    }
                    ++nuc_ci;
                    ++rna_ci;
                    ++total;
                }
                if (mismatches > 0) {
                    has_errors = true;
                    if (report_errors  &&  !(exception_flags & eMRNAExcept_Mismatch)) {
                        rval |= eMRNAProblem_Mismatch;
                    }
                }
            }
        }
    } catch (CException& ex) {
        rval |= eMRNAProblem_TransFail;
    } catch (std::exception) {
    }

    if (!report_errors) {
        if (!has_errors) {
            rval |= eMRNAProblem_UnnecessaryException;
        } else if ((exception_flags & eMRNAExcept_Unclassified) && !other_than_mismatch) {
            if (mismatches * 50 <= total) {
                rval |= eMRNAProblem_ErroneousException;
            }
        } else if (exception_flags & eMRNAExcept_ProductReplaced) {
            rval |= eMRNAProblem_ProductReplaced;
        }
    }
    return rval;
}



END_SCOPE(validator)
END_SCOPE(objects)
END_NCBI_SCOPE
