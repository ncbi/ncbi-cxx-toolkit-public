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
 * Author:  Justin Foley
 *
 * File Description:
 *   GFF3 alignment reader utils
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>
#include <objects/seqalign/Dense_seg.hpp>
#include <objects/seqalign/Seq_align.hpp>
#include <objects/seqalign/Product_pos.hpp>
#include <objects/seqalign/Spliced_seg.hpp>
#include <objects/seqalign/Spliced_exon.hpp>
#include <objects/seqalign/Spliced_exon_chunk.hpp>
#include <objects/seqalign/Seq_align_set.hpp>
#include <objects/seqalign/Score.hpp>
#include <objtools/readers/gff2_data.hpp>
#include <objtools/readers/reader_base.hpp>
#include "gff3_align_utils.hpp"


BEGIN_NCBI_SCOPE
BEGIN_objects_SCOPE

static bool s_GetStartsOnMinusStrand(TSeqPos offset,
        const vector<string>& gapParts,
        const bool isTarget,
        vector<int>& starts)
{
    starts.clear();
    const size_t gapCount = gapParts.size();

    for (size_t i=0; i<gapCount; ++i) {
        char changeType = gapParts[i][0];
        int changeSize = NStr::StringToInt(gapParts[i].substr(1));
        switch (changeType) {
        default:
            return false;

        case 'M':
            starts.push_back(offset+1-changeSize);
            offset -= changeSize;
            break;

        case 'I':
            if (isTarget) {
                starts.push_back(offset+1-changeSize);
                offset -= changeSize;
            } else {
                starts.push_back(-1);
            }
            break;

        case 'D':
            if (isTarget) {
                starts.push_back(-1);
            } else {
                starts.push_back(offset+1-changeSize);
                offset -= changeSize;
            }
            break;
        }
    }
    return true;
}


static bool s_GetStartsOnPlusStrand(TSeqPos offset,
        const vector<string>& gapParts,
        const bool isTarget,
        vector<int>& starts) // CGff3AlignUtils
{
    starts.clear();

    for (const auto& gap_part : gapParts) {
        char changeType = gap_part[0];
        int changeSize = NStr::StringToInt(gap_part.substr(1));
        switch (changeType) {
        default:
            return false;

        case 'M':
            starts.push_back(offset);
            offset += changeSize;
            break;

        case 'I':
            if (isTarget) {
                starts.push_back(offset);
                offset += changeSize;
            } else {
                starts.push_back(-1);
            }
            break;

        case 'D':
            if (isTarget) {
                starts.push_back(-1);
            } else {
                starts.push_back(offset);
                offset += changeSize;
            }
            break;
        }
    }
    return true;
}



static bool s_SetDensegStarts(const vector<string>& gapParts,
                                   const ENa_strand identStrand,
                                   const ENa_strand targetStrand,
                                   const TSeqPos targetStart,
                                   const TSeqPos targetEnd,
                                   const CGff2Record& gff,
                                   CSeq_align::C_Segs::TDenseg& denseg)
//  ----------------------------------------------------------------------------
{

    const size_t gapCount = gapParts.size();

    const bool isTarget = true;
    vector<int> targetStarts;
    if (targetStrand == eNa_strand_minus) {
        if( !s_GetStartsOnMinusStrand(targetEnd,
            gapParts,
            isTarget,
            targetStarts)) {
            return false;
        }
    }
    else {
        if (!s_GetStartsOnPlusStrand(targetStart,
            gapParts,
            isTarget,
            targetStarts)) {
            return false;
        }
    }

    vector<int> identStarts;
    const bool isIdent = !isTarget;

    if (identStrand == eNa_strand_minus) {

        if ( !s_GetStartsOnMinusStrand(
            static_cast<TSeqPos>(gff.SeqStop()),
            gapParts,
            isIdent,
            identStarts)) {
            return false;
        }
    }
    else {
        if ( !s_GetStartsOnPlusStrand(
            static_cast<TSeqPos>(gff.SeqStart()),
            gapParts,
            isIdent,
            identStarts)) {
            return false;
        }
    }

    for (size_t i=0; i<gapCount; ++i) {
        denseg.SetStarts().push_back(targetStarts[i]);
        denseg.SetStarts().push_back(identStarts[i]);
    }

    return true;
}



static bool s_GetTargetParts(const CGff2Record& gff, vector<string>& targetParts)
//  ----------------------------------------------------------------------------
{
    string targetInfo;
    if (!gff.GetAttribute("Target", targetInfo)) {
        return false;
    }

    NStr::Split(targetInfo, " ", targetParts);
    if (targetParts.size() != 4) {
        return false;
    }

    return true;
}



static bool s_AlignmentSetSpliced_seg(
    const CGff2Record& gff,
    CReaderBase::SeqIdResolver& SeqIdResolve,
    CRef<CSeq_align> pAlign)
{
    vector<string> targetParts;
    if (!s_GetTargetParts(gff, targetParts)) {
        return false;
    }

    CSeq_align::TSegs& segs = pAlign->SetSegs();

    auto& spliced_seg = segs.SetSpliced();

    const string& type = gff.Type();
    if (type == "translated_nucleotide_match") {
        spliced_seg.SetProduct_type(CSpliced_seg::eProduct_type_protein);
    }
    else {
        spliced_seg.SetProduct_type(CSpliced_seg::eProduct_type_transcript);
    }
    CRef<CSeq_id> product_id = SeqIdResolve(targetParts[0], 0, true);
    spliced_seg.SetProduct_id(*product_id);

    CRef<CSeq_id> genomic_id = SeqIdResolve(gff.Id(), 0, true);
    spliced_seg.SetGenomic_id(*genomic_id);

    if (targetParts[3] == "+") {
        spliced_seg.SetProduct_strand(eNa_strand_plus);
    }
    else
    if (targetParts[3] == "-") {
        spliced_seg.SetProduct_strand(eNa_strand_minus);
    }

    if (gff.IsSetStrand()) {
        ENa_strand ident_strand = gff.Strand();
        spliced_seg.SetGenomic_strand(ident_strand);
    }

    CRef<CSpliced_exon> exon(new CSpliced_exon());
    exon->SetProduct_start().SetNucpos(NStr::StringToInt(targetParts[1])-1);
    exon->SetProduct_end().SetNucpos(NStr::StringToInt(targetParts[2])-1);

    exon->SetGenomic_start(static_cast<TSeqPos>(gff.SeqStart()));
    exon->SetGenomic_end(static_cast<TSeqPos>(gff.SeqStop()));

    string gapInfo;
    vector<string> gapParts;
    if (gff.GetAttribute("Gap", gapInfo)) {
        NStr::Split(gapInfo, " ", gapParts);
    }
    else {
        gapParts.push_back(string("M") + NStr::NumericToString(gff.SeqStop()-gff.SeqStart()+1));
    }

    for (const auto& gap_part : gapParts) {
        CRef<CSpliced_exon_chunk> chunk(new CSpliced_exon_chunk());
        char changeType = gap_part[0];
        int changeSize = NStr::StringToInt(gap_part.substr(1));
        switch (changeType) {
        default:
            return false;

        case 'M':
            chunk->SetMatch(changeSize);
            break;

        case 'I':
            chunk->SetProduct_ins(changeSize);
            break;

        case 'D':
            chunk->SetGenomic_ins(changeSize);
            break;

        }
        exon->SetParts().push_back(chunk);
    }

    spliced_seg.SetExons().push_back(exon);
    return true;
}


static bool s_AlignmentSetDenseg(
    const CGff2Record& gff,
    CReaderBase::SeqIdResolver& SeqIdResolve,
    CRef<CSeq_align> pAlign)
{
    vector<string> targetParts;
    if (!s_GetTargetParts(gff, targetParts)) {
        return false;
    }

    //strands
    ENa_strand targetStrand = eNa_strand_plus;
    if (targetParts[3] == "-") {
        targetStrand = eNa_strand_minus;
    }
    ENa_strand identStrand = eNa_strand_plus;
    if (gff.IsSetStrand()) {
        identStrand = gff.Strand();
    }


    string gapInfo;
    vector<string> gapParts;
    if (gff.GetAttribute("Gap", gapInfo)) {
        NStr::Split(gapInfo, " ", gapParts);
    }
    else {
        gapParts.push_back(string("M") + NStr::NumericToString(gff.SeqStop()-gff.SeqStart()+1));
    }

    int gapCount = static_cast<int>(gapParts.size());

    //meta
    CSeq_align::TSegs& segs = pAlign->SetSegs();
    CSeq_align::C_Segs::TDenseg& denseg = segs.SetDenseg();
    denseg.SetDim(2);
    denseg.SetNumseg(gapCount);

    //ids
    denseg.SetIds().push_back(
        SeqIdResolve(targetParts[0], 0, true));
    denseg.SetIds().push_back(
        SeqIdResolve(gff.Id(), 0, true));

    const TSeqPos targetStart = NStr::StringToInt(targetParts[1])-1;
    const TSeqPos targetEnd   = NStr::StringToInt(targetParts[2])-1;

    if (!s_SetDensegStarts(gapParts,
                          identStrand,
                          targetStrand,
                          targetStart,
                          targetEnd,
                          gff,
                          denseg)) {
        return false;
    }

    //lengths
    for (int i=0; i < gapCount; ++i) {
        denseg.SetLens().push_back(NStr::StringToInt(CTempString(gapParts[i],1,string::npos)));
    }

    for (int i=0; i < gapCount; ++i) {
        denseg.SetStrands().push_back(targetStrand);
        denseg.SetStrands().push_back(identStrand);
    }
    return true;
}


bool SGff3AlignUtils::SetSegment(
    const CGff2Record& gff,
    CReaderBase::SeqIdResolver& SeqIdResolve,
    CRef<CSeq_align> pAlign)
{
    const string& type = gff.Type();

    if (type == "cDNA_match" ||
        type == "EST_match"  ||
        type == "translated_nucleotide_match") {
        return s_AlignmentSetSpliced_seg(gff, SeqIdResolve, pAlign);
    }

    return s_AlignmentSetDenseg(gff, SeqIdResolve, pAlign);
}


bool SGff3AlignUtils::SetScore(
    const CGff2Record& gff,
    CRef<CSeq_align> pAlign)
{
    if (gff.IsSetScore()) {
        pAlign->SetNamedScore(CSeq_align::eScore_Score,
            int(gff.Score()));
    }

    string extraScore;

    const string intScores[] = {
        //official
        "score",
        "align_length",
        "num_ident",
        "num_positives",
        "num_negatives",
        "num_mismatch",
        "num_gap",

        //picked up from real data files
        "common_component",
        "filter_score",
        "for_remapping",
        "merge_aligner",
        "rank",
        "reciprocity",
        "batch_id",
        "align_id",
    };

    const size_t intCount(sizeof(intScores)/sizeof(string));
    for (size_t i=0; i < intCount; ++i) {
        if (gff.GetAttribute(intScores[i], extraScore)) {
            pAlign->SetNamedScore(
                intScores[i], int(NStr::StringToDouble(extraScore)));
        }
    }

    const string realScores[] = {
        //official
        "bit_score",
        "e_value",
        "pct_identity_gap",
        "pct_identity_ungap",
        "pct_identity_gapopen_only",
        "pct_coverage",
        "sum_e",
        "comp_adjustment_method",
        "pct_coverage_hiqual",

        //picked up from real data files
        "inversion_merge_alignmer",
        "expansion",
    };

    const size_t realCount(sizeof(realScores)/sizeof(string));
    for (size_t i=0; i < realCount; ++i) {
        if (gff.GetAttribute(realScores[i], extraScore)) {
            pAlign->SetNamedScore(
                realScores[i], NStr::StringToDouble(extraScore));
        }
    }

    return true;
}

class CScoreUtils {
public:
    using TScoreValueMap = map<string, CRef<CScore::TValue>>;

    static bool MergeAlignments(
            const list<CRef<CSeq_align>>& alignment_list,
            CRef<CSeq_align>& merged);
private:
    static void x_ProcessAlignmentScores(const CSeq_align& alignments,
            map<string,TSeqPos>& summed_scores,
            TScoreValueMap& common_scores);

    static void x_GetAlignmentScores(const CSeq_align& alignment,
            TScoreValueMap& score_values);

    static void x_InitializeScoreSums(const TScoreValueMap& score_values,
            map<string,TSeqPos>& summed_scores);

    static void x_FindMatchingScores(const TScoreValueMap& scores_1,
                              const TScoreValueMap& scores_2,
                              set<string>& matching_scores);

};


void CScoreUtils::x_GetAlignmentScores(const CSeq_align& alignment,
                                        TScoreValueMap& score_values)
{
    // Start with empty scores
    score_values.clear();

    if (!alignment.IsSetScore()) {
        return;
    }

    for (const CRef<CScore>& score : alignment.GetScore()) {

        if (!score->IsSetId() ||
            !score->GetId().IsStr() ||
            !score->IsSetValue()) {
            continue;
        }
        const string name = score->GetId().GetStr();
        const CScore::TValue& value = score->GetValue();
        score_values[name] = Ref(new CScore::TValue());
        score_values[name]->Assign(value);
    }
}


void CScoreUtils::x_InitializeScoreSums(const TScoreValueMap& score_values,
        map<string,TSeqPos>& summed_scores)
{
    const list<string> score_names {"num_ident", "num_mismatch"};

    for (const string& score_name : score_names) {
        if (score_values.find(score_name) != score_values.end()) {
            summed_scores[score_name] = score_values.at(score_name)->GetInt();
        }
    }
}


static bool s_CompareValues(const CScore::TValue& score_val1,
                     const CScore::TValue& score_val2)
{

    if (score_val1.IsInt() &&
        score_val2.IsInt() &&
        score_val1.GetInt() == score_val2.GetInt()) {
        return true;
    }

    if (score_val1.IsReal() &&
        score_val2.IsReal() &&
        score_val1.GetReal() == score_val2.GetReal()) {
        return true;
    }

    return false;
}


void CScoreUtils::x_FindMatchingScores(const TScoreValueMap& scores_1,
                                       const TScoreValueMap& scores_2,
                                       set<string>& matching_scores)
{
    matching_scores.clear();

    for (const auto& score1 : scores_1) {
        const string& name = score1.first;
        const CScore::TValue& value = *(score1.second);

        const auto& it = scores_2.find(name);
        if (it != scores_2.end() &&
            s_CompareValues(value, *(it->second))) {
            matching_scores.insert(name);
        }
    }
}


void CScoreUtils::x_ProcessAlignmentScores(const CSeq_align& alignment,
    map<string, TSeqPos>& summed_scores,
    TScoreValueMap& common_scores)
{
    const list<string> summed_score_names {"num_ident", "num_mismatch"};

    TScoreValueMap new_scores;
    x_GetAlignmentScores(alignment, new_scores);

    for (const string& score_name : summed_score_names) {
        if (new_scores.find(score_name) == new_scores.end()) {
            summed_scores.erase(score_name);
        } else if (summed_scores.find(score_name) != summed_scores.end()) {
            summed_scores[score_name] += new_scores[score_name]->GetInt();
            new_scores.erase(score_name);
        }
    }

    set<string> matching_score_names;
    x_FindMatchingScores(common_scores,
        new_scores,
        matching_score_names);

    common_scores.clear();
    for (string score_name : matching_score_names) {
        common_scores[score_name] = Ref(new CScore::TValue());
        common_scores[score_name]->Assign(*new_scores[score_name]);
    }
}


bool CScoreUtils::MergeAlignments(
        const list<CRef<CSeq_align>>& alignment_list,
        CRef<CSeq_align>& processed)
{
    if (alignment_list.empty()) {
        return false;
    }

    if (alignment_list.size() == 1) {
        processed = alignment_list.front();
        return true;
    }

    map<string, TSeqPos> summed_scores;
    const list<string> summed_score_names {"num_ident", "num_mismatch"};

    // Factor out identical scores
    list<CRef<CSeq_align>>::const_iterator align_it = alignment_list.begin();
    TScoreValueMap score_values;
    x_GetAlignmentScores(**align_it, score_values);

    x_InitializeScoreSums(score_values,
        summed_scores);
    ++align_it;

    while (align_it != alignment_list.end() &&
           !score_values.empty()) {

        x_ProcessAlignmentScores(**align_it, summed_scores, score_values);
        ++align_it;
    }
    // At this point, the score_values map should contain the scores that
    // do not change over the rows

    const auto first_alignment = alignment_list.front();
    if (first_alignment->IsSetSegs() &&
        first_alignment->GetSegs().IsSpliced()) {

        processed->SetType(CSeq_align::eType_global);

        if (first_alignment->IsSetDim()) {
            processed->SetDim(first_alignment->GetDim());
        }

        for (auto& kv : summed_scores) {
            auto score = Ref(new CScore());
            score->SetId().SetStr(kv.first);
            score->SetValue().SetInt(kv.second);
            processed->SetScore().push_back(score);
        }

        for (auto& kv : score_values) {
            auto score = Ref(new CScore());
            score->SetId().SetStr(kv.first);
            score->SetValue().Assign(*(kv.second));
            processed->SetScore().push_back(score);
        }

        CRef<CSpliced_seg> spliced = Ref(new CSpliced_seg());
        spliced->Assign(first_alignment->GetSegs().GetSpliced());
        processed->SetSegs().SetSpliced(*spliced);

        auto align_it = alignment_list.cbegin();
        ++align_it;

        while(align_it != alignment_list.end()) {
            const auto& spliced_seg = (*align_it)->GetSegs().GetSpliced();
            if (spliced_seg.IsSetExons()) {
                for (auto exon : spliced_seg.GetExons()) {
                    processed->SetSegs().SetSpliced().SetExons().push_back(exon);
                }
            }
            ++align_it;
        }
        return true;
    }


    processed->SetType(CSeq_align::eType_disc);

    for (auto& kv : summed_scores) {
        auto score = Ref(new CScore());
        score->SetId().SetStr(kv.first);
        score->SetValue().SetInt(kv.second);
        processed->SetScore().push_back(score);
    }

    for (auto& kv : score_values) {
        auto score = Ref(new CScore());
        score->SetId().SetStr(kv.first);
        score->SetValue().Assign(*(kv.second));
        processed->SetScore().push_back(score);
    }

    for (auto current : alignment_list) {
        auto new_align = Ref(new CSeq_align());
        new_align->Assign(*current);
        new_align->ResetScore();

        for (CRef<CScore> score : current->GetScore()) {
            const string& score_name = score->GetId().GetStr();
            if (score_values.find(score_name) == score_values.end()) {
                new_align->SetScore().push_back(score);
            }
        }
        processed->SetSegs().SetDisc().Set().push_back(new_align);
    }

    return true;
}


bool SGff3AlignUtils::MergeAlignments(
        const list<CRef<CSeq_align>>& alignment_list,
        CRef<CSeq_align>& merged) {

    return CScoreUtils::MergeAlignments(alignment_list, merged);
}

END_objects_SCOPE
END_NCBI_SCOPE

