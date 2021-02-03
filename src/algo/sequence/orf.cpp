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
 * Authors:  Mike DiCuccio
 *
 * File Description:
 *
 */


#include <ncbi_pch.hpp>
#include <algo/sequence/orf.hpp>
#include <objects/seqloc/Seq_interval.hpp>
#include <objects/seqloc/Na_strand.hpp>
#include <objects/seqfeat/Genetic_code_table.hpp>
#include <objects/seq/seqport_util.hpp>
#include <objects/general/Int_fuzz.hpp>
#include <objects/seqfeat/Cdregion.hpp>
#include <objects/seqfeat/Seq_feat.hpp>
#include <objects/seq/Seq_annot.hpp>
#include <algorithm>

#include <algo/sequence/consensus_splice.hpp>

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);

typedef vector<CRef<CSeq_interval>> TRangeVec;


char Complement(const char c)
{
    //note: N->N, S->S, W->W, U->A, T->A
    static const char* iupac_revcomp_table =
        "................................................................"
        ".TVGH..CD..M.KN...YSAABW.R.......tvgh..cd..m.kn...ysaabw.r......"
        "................................................................"
        "................................................................";
    return iupac_revcomp_table[static_cast<unsigned char>(c)];
}

bool IsGapOrN(const char c)
{
    return c == 'N' || c == 'n' || Complement(c) == '.';
}

// Find the lowest position, in-frame relative to \p from in [from, to_open)
// such that codon begining at that position is on a list of \p allowable_starts
// and there's no more than max_seq_gap consecutive gap-or-N bases between the 
// putative start and to_open. (gap is any char other than extended IUPAC residues)
//
// return to_open iff not found.
template<class TSeq>
set<TSeqPos> FindStarts(const TSeq& seq, 
                              TSeqPos from, TSeqPos to,
                              const vector<string>& allowable_starts)
{
    const TSeqPos inframe_to_open = to+1;

    set<TSeqPos> starts;
    starts.insert(inframe_to_open);

    for (TSeqPos pos = inframe_to_open - 3;
         pos >= from && pos < inframe_to_open;
         pos -= 3) 
    {
        ITERATE(vector<string>, it, allowable_starts) {
            if (   seq[pos + 0] == (*it)[0] 
                && seq[pos + 1] == (*it)[1] 
                && seq[pos + 2] == (*it)[2]) 
            {
                starts.insert(pos);
                break;
            }
        }
    }
    return starts;
}

void AddInterval(TRangeVec& intervals,
                    TSeqPos from, TSeqPos to,
                    bool from_fuzz=false, bool to_fuzz=false)
{
    intervals.emplace_back(new CSeq_interval);
    auto& interval = *intervals.back();
    interval.SetFrom(from);
    interval.SetTo(to);
    if (from_fuzz)
        interval.SetFuzz_from().SetLim(objects::CInt_fuzz::eLim_lt);
    if (to_fuzz)
        interval.SetFuzz_to().SetLim(objects::CInt_fuzz::eLim_gt);
}

/// Find all ORFs in forward orientation with
/// length in *base pairs* >= min_length_bp.
/// seq must be in iupac.
/// Returned range does not include the
/// stop codon.
template<class TSeq>
inline void FindForwardOrfs(const TSeq& seq, TRangeVec& ranges,
                            unsigned int min_length_bp,
                            int genetic_code,
                            const vector<string>& allowable_starts,
                            bool longest_orfs,
                            size_t max_seq_gap,
                            bool stop_to_stop)
{
    vector<vector<TSeqPos> > stops;
    stops.resize(3);
    const objects::CTrans_table& tbl = 
        objects::CGen_code_table::GetTransTable(genetic_code);
    int state = 0;
    for (unsigned int i = 0;  i < seq.size();  ++i) {
        if (IsGapOrN(seq[i])) {
            unsigned int j = i;
            while (++j < seq.size() && IsGapOrN(seq[j]))
                state = tbl.NextCodonState(state, seq[j]);
            if (j - i > max_seq_gap) {
                for (int f=0; f < 3; ++f) {
                    stops[f].push_back(i);
                    stops[f].push_back(j -1);
                }
            }
            i = j;
        } else {
            state = tbl.NextCodonState(state, seq[i]);
        }
        if (tbl.IsOrfStop(state)) {
            stops[(i - 2) % 3].push_back(i - 2);
        }
    }

    TSeqPos from, to;
    // for each reading frame, calculate the orfs
    for (int frame = 0;  frame < 3;  frame++) {

        stops[frame].push_back(seq.size());
        stops[frame].push_back(seq.size());

        bool gap_before = true;
        from = frame;
        for (unsigned int i = 0; i < stops[frame].size() -1;  i++) {
            TSeqPos from0 = from;
            TSeqPos stop = stops[frame][i];
            
            bool gap_after = (stop >= seq.size() || IsGapOrN(seq[stop]));

            if (stop >= min_length_bp + from) {

            to = ((stop - from) / 3) * 3 + from - 1; // cerr << from << " " << to << " " << stop << endl;
            _ASSERT( gap_after || to+1==stop );
            if (to +1 >= min_length_bp + from) {
                set<TSeqPos> starts; 
                if (!allowable_starts.empty()) {
                    starts = FindStarts(seq, 
                                        from, to, 
                                        allowable_starts);
                    from = *starts.begin();
                }
                if (to +1 >= min_length_bp + from) {
                    if (from != from0 && stop_to_stop) {
                        AddInterval(ranges, from0, to,
                                    true, gap_after);
                    }
                    if (!(stop_to_stop && from != from0 && longest_orfs)) {
                        AddInterval(ranges, from, to,
                                    !stop_to_stop && from < 3, gap_after);
                    }

                    if (!longest_orfs && !allowable_starts.empty()) {
                        starts.erase(starts.begin());
                        for (auto s: starts) {
                            from = s;
                            if (to +1 < min_length_bp + from)
                                break;
                            AddInterval(ranges, from, to,
                                        false, gap_after);
                        }
                    }
                } else // start found but too short
                    if (stop_to_stop) {
                        from = from0;
                        AddInterval(ranges, from, to,
                                    true, gap_after);
                    }
            }

            }
            if (gap_after) {
                ++i;
                stop = stops[frame][i] +3;
                from = ((stop - frame)/3)*3 + frame;
            } else {
                from = stop +3;
            }
            gap_before = gap_after;
        }
    }
}

/// Find all ORFs in both orientations that
/// are at least min_length_bp long (not including the stop).
/// Report results as Seq-locs.
/// seq must be in iupac.
template<class TSeq>
static void s_FindOrfs(const TSeq& seq, COrf::TLocVec& results,
                       unsigned int min_length_bp,
                       int genetic_code,
                       const vector<string>& allowable_starts_,
                       bool longest_orfs,
                       size_t max_seq_gap)
{
    if (seq.size() < 3) {
        return;
    }

    if (min_length_bp < 3) min_length_bp = 3;
    
    TRangeVec ranges;

    bool stop_to_stop = false;
    auto stop = find(allowable_starts_.begin(), allowable_starts_.end(), "STOP");
    vector<string> allowable_starts_2;
    if (stop != allowable_starts_.end()) {
        stop_to_stop = true;
        if (allowable_starts_.size() > 1) {
            allowable_starts_2 = allowable_starts_;
            allowable_starts_2.erase(allowable_starts_2.begin() + distance(allowable_starts_.begin(), stop));
        }
    }
    const vector<string>& allowable_starts = stop_to_stop ? allowable_starts_2 : allowable_starts_;

    // This code might be sped up by a factor of two
    // by use of a state machine that does all six frames
    // in a single pass.

    // find ORFs on the forward sequence and report them as-is
    FindForwardOrfs(seq, ranges, min_length_bp,
                    genetic_code, allowable_starts, longest_orfs, max_seq_gap, stop_to_stop);
    for (auto& interval: ranges) {
        CRef<objects::CSeq_loc> orf(new objects::CSeq_loc());
        if (!interval->IsPartialStop(eExtreme_Positional))
            interval->SetTo() += 3;
        orf->SetInt().Assign(*interval);
        orf->SetInt().SetStrand(objects::eNa_strand_plus);
        results.push_back(orf);
    }

    // find ORFs on the complement and munge the numbers
    ranges.clear();
    TSeq comp(seq);

    // compute the complement;
    // this should be replaced with new Seqport_util call
    reverse(comp.begin(), comp.end());
    NON_CONST_ITERATE (typename TSeq, i, comp) {
        *i = objects::CSeqportUtil
            ::GetIndexComplement(objects::eSeq_code_type_iupacna, *i);
    }

    FindForwardOrfs(comp, ranges, min_length_bp,
                    genetic_code, allowable_starts, longest_orfs, max_seq_gap, stop_to_stop);
    for (auto& interval: ranges) {
        CRef<objects::CSeq_loc> orf(new objects::CSeq_loc);
        if (!interval->IsPartialStop(eExtreme_Positional))
            interval->SetTo() += 3;
        unsigned int from = comp.size() - interval->GetTo() - 1;
        orf->SetInt().SetFrom(from);
        unsigned int to = comp.size() - interval->GetFrom() - 1;
        orf->SetInt().SetTo(to);
        orf->SetInt().SetStrand(objects::eNa_strand_minus);
        orf->SetPartialStart(interval->IsPartialStop(eExtreme_Positional), eExtreme_Positional);
        orf->SetPartialStop(interval->IsPartialStart(eExtreme_Positional), eExtreme_Positional);
        results.push_back(orf);
    }
}


vector<string> COrf::GetStartCodons(int genetic_code, bool include_atg, bool include_alt)
{
    const objects::CTrans_table& tbl = 
            objects::CGen_code_table::GetTransTable(genetic_code); 

    static const char* iupacs = "ACGTRYSWKMBDHVN";
    static const Uint1 k_num_iupacs = 15;

    vector<string> codons;
    for(Uint1 i1 = 0; i1 < k_num_iupacs; i1++) {
        char c1 = iupacs[i1];
        for(Uint1 i2 = 0; i2 < k_num_iupacs; i2++) {
            char c2 = iupacs[i2];
            for(Uint1 i3 = 0; i3 < k_num_iupacs; i3++) {
                char c3 = iupacs[i3];
                int state = tbl.SetCodonState(c1, c2, c3);

                if(   (include_atg && tbl.IsATGStart(state))
                   || (include_alt && tbl.IsAltStart(state)) ) 
                {
                    codons.resize(codons.size() + 1);
                    codons.back().resize(3);
                    codons.back()[0] = c1;
                    codons.back()[1] = c2;
                    codons.back()[2] = c3;
                }
            }
        }
    }

    return codons;
}


//
// find ORFs in a string
void COrf::FindOrfs(const string& seq_iupac,
                    TLocVec& results,
                    unsigned int min_length_bp,
                    int genetic_code,
                    const vector<string>& allowable_starts,
                    bool longest_orfs,
                    size_t max_seq_gap)
{
    s_FindOrfs(seq_iupac, results, min_length_bp,
               genetic_code, allowable_starts, longest_orfs, max_seq_gap);
}


//
// find ORFs in a vector<char>
void COrf::FindOrfs(const vector<char>& seq_iupac,
                    TLocVec& results,
                    unsigned int min_length_bp,
                    int genetic_code,
                    const vector<string>& allowable_starts,
                    bool longest_orfs,
                    size_t max_seq_gap)
{
    s_FindOrfs(seq_iupac, results, min_length_bp,
               genetic_code, allowable_starts, longest_orfs, max_seq_gap);
}


//
// find ORFs in a CSeqVector
void COrf::FindOrfs(const CSeqVector& orig_vec,
                    TLocVec& results,
                    unsigned int min_length_bp,
                    int genetic_code,
                    const vector<string>& allowable_starts,
                    bool longest_orfs,
                    size_t max_seq_gap)
{
    string seq_iupac;  // will contain ncbi8na
    CSeqVector vec(orig_vec);
    vec.SetCoding(CSeq_data::e_Iupacna);
    vec.GetSeqData(0, vec.size(), seq_iupac);
    s_FindOrfs(seq_iupac, results, min_length_bp,
               genetic_code, allowable_starts, longest_orfs, max_seq_gap);
}


void COrf::FindStrongKozakUOrfs(
                   const CSeqVector& seq,
                   TSeqPos cds_start,
                   TLocVec& overlap_results,
                   TLocVec& non_overlap_results,
                   unsigned int min_length_bp,
                   unsigned int non_overlap_min_length_bp,
                   int genetic_code,
                   size_t max_seq_gap)
{
    if (cds_start > seq.size()) {
        NCBI_THROW(CException, eUnknown,
                   "cds_start not within input CSeqVector");
    }

    if (cds_start <= 3) {
        /// 5' UTR is too short for there to possihly be a uORF
        return;
    }

    vector<string> start_codon(1, "ATG");
    TLocVec ORFs;
    FindOrfs(seq, ORFs, min_length_bp, genetic_code, start_codon, false, max_seq_gap);
    ITERATE (TLocVec, it, ORFs) {
        if ((*it)->GetStrand() == eNa_strand_minus) {
            /// We're only intersted in ORFs on the plus strand
            continue;
        }
        TSeqPos ORF_start = (*it)->GetStart(eExtreme_Biological),
                ORF_end = (*it)->GetStop(eExtreme_Biological);
        /// We're only intersted in uORFs, i.e. ORFs starting before CDS start;
        /// and only if they start after at least 3 bases and at least 5 bases
        /// before end, so there can be a Kozak signal
        /// overlapping uORFs count only if they're in a different frame;
        /// non-overlapping uORFs count only if long enough
        if (ORF_start < 3 || ORF_start >= cds_start ||
            ORF_start + 5 > seq.size() ||
            (ORF_end >= cds_start ? (cds_start - ORF_start) % 3 == 0
                           : ORF_end - ORF_start < non_overlap_min_length_bp))
        {
            continue;
        }
        string Kozak_signal;
        seq.GetSeqData(ORF_start - 3, ORF_start + 5, Kozak_signal);
        if ((Kozak_signal[0] == 'A' || Kozak_signal[0] == 'G') &&
            Kozak_signal[6] == 'G' && Kozak_signal[7] != 'T')
        {
            (ORF_end >= cds_start ? overlap_results : non_overlap_results)
                . push_back(*it);
        }
    }
}

// build an annot representing CDSs
CRef<CSeq_annot>
COrf::MakeCDSAnnot(const TLocVec& orfs, int genetic_code, CSeq_id* id)
{
    CRef<CSeq_annot> annot(new CSeq_annot());
    annot->SetData().SetFtable();  // in case there are zero orfs

    ITERATE (TLocVec, orf, orfs) {
        // create feature
        CRef<CSeq_feat> feat(new CSeq_feat());

        // confess the fact that it's just a computed ORF
        feat->SetExp_ev(CSeq_feat::eExp_ev_not_experimental);
        feat->SetData().SetCdregion().SetOrf(true);  // just an ORF
        // they're all frame 1 in this sense of 'frame'
        feat->SetData().SetCdregion().SetFrame(CCdregion::eFrame_one);
        feat->SetTitle("Open reading frame");

        // set up the location
        feat->SetLocation(const_cast<CSeq_loc&>(**orf));
        if (id) {
            feat->SetLocation().SetId(*id);
        }

        // save in annot
        annot->SetData().SetFtable().push_back(feat);
    }
    return annot;
}




END_NCBI_SCOPE
