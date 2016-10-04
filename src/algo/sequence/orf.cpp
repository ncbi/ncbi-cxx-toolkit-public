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
inline TSeqPos FindFirstStart(const TSeq& seq, 
                              TSeqPos from, TSeqPos to_open,
                              const vector<string>& allowable_starts,
                              size_t max_seq_gap)
{
    const TSeqPos inframe_to_open = from + ((to_open - from)/3 * 3);
    size_t gap_length(0);

    TSeqPos best_start_pos = to_open; //a.k.a not-found

    for (TSeqPos pos = inframe_to_open - 3;
         pos >= from && pos < inframe_to_open;
         pos -= 3) 
    {
        bool is_gap = IsGapOrN(seq[pos]) 
                   && IsGapOrN(seq[pos + 1])
                   && IsGapOrN(seq[pos + 2]);

        gap_length = (gap_length + 3) * is_gap; 
        // note: this value may slightly differ from the true
        // one, since we are considering bases in triplets.

        if(gap_length > max_seq_gap) {
            break;
        }

        ITERATE(vector<string>, it, allowable_starts) {
            if (   seq[pos + 0] == (*it)[0] 
                && seq[pos + 1] == (*it)[1] 
                && seq[pos + 2] == (*it)[2]) 
            {
                best_start_pos = pos;
            }
        }
    }
    return best_start_pos;
}


/// Find all ORFs in forward orientation with
/// length in *base pairs* >= min_length_bp.
/// seq must be in iupac.
/// Returned range does not include the
/// stop codon.
template<class TSeq>
inline void FindForwardOrfs(const TSeq& seq, COrf::TRangeVec& ranges,
                            unsigned int min_length_bp,
                            int genetic_code,
                            const vector<string>& allowable_starts,
                            bool longest_orfs,
                            size_t max_seq_gap)
{

    vector<vector<TSeqPos> > stops;
    stops.resize(3);
    const objects::CTrans_table& tbl = 
        objects::CGen_code_table::GetTransTable(genetic_code);
    int state = 0;
    for (unsigned int i = 0;  i < seq.size() - 2;  i += 3) {
        for (int pos = 0;  pos < 3;  pos++) {
            state = tbl.NextCodonState(state, seq[i + pos]);
            if (tbl.IsOrfStop(state)) {
                stops[(i + pos - 2) % 3].push_back(i + pos - 2);
            }
        }
    }
    

    TSeqPos from, to;
    // for each reading frame, calculate the orfs
    for (int frame = 0;  frame < 3;  frame++) {

        if (stops[frame].empty()) {
            // no stops in this frame; the whole sequence,
            // minus some scraps at the ends, is one ORF
            from = frame;
            // 'to' should be the largest index within the
            // sequence length that gives an ORF length
            // divisible by 3
            to = ((seq.size() - from) / 3) * 3 + from - 1;
            if (!allowable_starts.empty()) {
                from = FindFirstStart(seq, 
                                      from, to, 
                                      allowable_starts, 
                                      max_seq_gap);
            }
            if (to - from + 1 >= min_length_bp) {
                ranges.push_back(COrf::TRange(from, to));

                if (!longest_orfs && !allowable_starts.empty()) {
                    for (from += 3; from < to; from += 3) {
                        from = FindFirstStart(seq, 
                                              from, to, 
                                              allowable_starts, 
                                              max_seq_gap);
                        if (to - from + 1 < min_length_bp)
                            break;
                        ranges.push_back(COrf::TRange(from, to));
                    }
                }
            }
            continue;  // we're done for this reading frame
        }
    
        // deal specially with first ORF
        from = frame;
        to = stops[frame].front() - 1;
        if (to - from + 1 >= min_length_bp) {
            if (!allowable_starts.empty()) {
                from = FindFirstStart(seq, 
                                      from, to, 
                                      allowable_starts, 
                                      max_seq_gap);
            }
            if (from < to && to - from + 1 >= min_length_bp) {
                ranges.push_back(COrf::TRange(from, to));

                if (!longest_orfs && !allowable_starts.empty()) {
                    for (from += 3; from < to; from += 3) {
                        from = FindFirstStart(seq, 
                                              from, to, 
                                              allowable_starts, 
                                              max_seq_gap);
                        if (to - from + 1 < min_length_bp)
                            break;
                        ranges.push_back(COrf::TRange(from, to));
                    }
                }
            }
        }

        for (unsigned int i = 0;  i < stops[frame].size() - 1;  i++) {
            from = stops[frame][i] + 3;
            to = stops[frame][i + 1] - 1;
            if (to - from + 1 >= min_length_bp) {
                if (!allowable_starts.empty()) {
                    from = FindFirstStart(seq, 
                                          from, to, 
                                          allowable_starts, 
                                          max_seq_gap);
                    if (from >= to || to - from + 1 < min_length_bp) {
                        continue;
                    }
                }
                ranges.push_back(COrf::TRange(from, to));

                if (!longest_orfs && !allowable_starts.empty()) {
                    for (from += 3; from < to; from += 3) {
                        from = FindFirstStart(seq, 
                                              from, to, 
                                              allowable_starts, 
                                              max_seq_gap);
                        if (to - from + 1 < min_length_bp)
                            break;
                        ranges.push_back(COrf::TRange(from, to));
                    }
                }
            }
        }
    
        // deal specially with last ORF
        from = stops[frame].back() + 3;
        // 'to' should be the largest index within the
        // sequence length that gives an orf length
        // divisible by 3
        to = ((seq.size() - from) / 3) * 3 + from - 1;
        if (to - from + 1 >= min_length_bp) {
            if (!allowable_starts.empty()) {
                from = FindFirstStart(seq, from, to, allowable_starts, max_seq_gap);
                if (from >= to || to - from + 1 < min_length_bp) {
                    continue;
                }
            }
            ranges.push_back(COrf::TRange(from, to));

            if (!longest_orfs && !allowable_starts.empty()) {
                for (from += 3; from < to; from += 3) {
                    from = FindFirstStart(seq, from, to, allowable_starts, max_seq_gap);
                    if (to - from + 1 < min_length_bp)
                        break;
                    ranges.push_back(COrf::TRange(from, to));
                }
            }
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
                       const vector<string>& allowable_starts,
                       bool longest_orfs,
                       size_t max_seq_gap)
{
    if (seq.size() < 30) {
        return;
    }
    COrf::TRangeVec ranges;

    // This code might be sped up by a factor of two
    // by use of a state machine that does all six frames
    // in a single pass.

    // find ORFs on the forward sequence and report them as-is
    FindForwardOrfs(seq, ranges, min_length_bp,
                    genetic_code, allowable_starts, longest_orfs, max_seq_gap);
    ITERATE (COrf::TRangeVec, iter, ranges) {
        CRef<objects::CSeq_loc> orf(new objects::CSeq_loc());
        orf->SetInt().SetFrom(iter->GetFrom());
        if (iter->GetFrom() < 3) {
            // "beginning" of ORF at beginning of sequence
            orf->SetInt().SetFuzz_from().SetLim(objects::CInt_fuzz::eLim_lt);
        }
        unsigned int to = iter->GetTo();
        if (to + 3 >= seq.size()) {
            // "end" of ORF is really end of sequence
            orf->SetInt().SetFuzz_to().SetLim(objects::CInt_fuzz::eLim_gt);
        } else {
            // ORF was ended by a stop, rather than end of sequence
            to += 3;
        }
        orf->SetInt().SetTo(to);
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
                    genetic_code, allowable_starts, longest_orfs, max_seq_gap);
    ITERATE (COrf::TRangeVec, iter, ranges) {
        CRef<objects::CSeq_loc> orf(new objects::CSeq_loc);
        unsigned int from = comp.size() - iter->GetTo() - 1;
        if (from < 3) {
            // "end" of ORF is beginning of sequence
            orf->SetInt().SetFuzz_from().SetLim(objects::CInt_fuzz::eLim_lt);
        } else {
            // ORF was ended by a stop, rather than beginning of sequence
            from -= 3;
        }
        orf->SetInt().SetFrom(from);
        unsigned int to = comp.size() - iter->GetFrom() - 1;
        if (to + 3 >= comp.size()) {
            // "beginning" of ORF is really end of sequence
            orf->SetInt().SetFuzz_to().SetLim(objects::CInt_fuzz::eLim_gt);
        }
        orf->SetInt().SetTo(to);
        orf->SetInt().SetStrand(objects::eNa_strand_minus);
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
