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


#include <algo/sequence/orf.hpp>
#include <objects/seqloc/Seq_interval.hpp>
#include <objects/seqloc/Na_strand.hpp>
#include <objects/seqfeat/Genetic_code_table.hpp>
#include <objects/seq/seqport_util.hpp>
#include <algorithm>


BEGIN_NCBI_SCOPE
USING_SCOPE(objects);


/// Find all ORFs in forward orientation with
/// length in *base pairs* >= min_length_bp.
/// seq must be in iupac.
template<class Seq>
void x_FindForwardOrfs(const Seq& seq, COrf::TRangeVec& ranges,
                       unsigned int min_length_bp = 3,
                       int genetic_code = 1)
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
            if (to - from + 1 >= min_length_bp) {
                ranges.push_back(COrf::TRange(from, to));
            }
            continue;  // we're done for this reading frame
        }
    
        // deal specially with first ORF
        from = frame;
        to = stops[frame].front() - 1;
        if (to - from + 1 >= min_length_bp) {
            ranges.push_back(COrf::TRange(from, to));
        }

        for (unsigned int i = 0;  i < stops[frame].size() - 1;  i++) {
            from = stops[frame][i] + 3;
            to = stops[frame][i + 1] - 1;
            if (to - from + 1 >= min_length_bp) {
                ranges.push_back(COrf::TRange(from, to));
            }
        }
    
        // deal specially with last ORF
        from = stops[frame].back() + 3;
        // 'to' should be the largest index within the
        // sequence length that gives an orf length
        // divisible by 3
        to = ((seq.size() - from) / 3) * 3 + from - 1;
        if (to - from + 1 >= min_length_bp) {
            ranges.push_back(COrf::TRange(from, to));
        }
    }
}


/// Find all ORFs in both orientations that
/// are at least min_length_bp long.
/// Report results as Seq-locs.
/// seq must be in iupac.
template<class Seq>
void x_FindOrfs(const Seq& seq, COrf::TLocVec& results,
                unsigned int min_length_bp = 3,
                int genetic_code = 1)
{
    COrf::TRangeVec ranges;

    // This code might be sped up by a factor of two
    // by use of a state machine that does all sixs frames
    // in a single pass.

    // find ORFs on the forward sequence and report them as-is
    x_FindForwardOrfs(seq, ranges, min_length_bp, genetic_code);
    ITERATE (COrf::TRangeVec, iter, ranges) {
        CRef<objects::CSeq_loc> orf(new objects::CSeq_loc());
        orf->SetInt().SetFrom  (iter->GetFrom());
        orf->SetInt().SetTo    (iter->GetTo());
        orf->SetInt().SetStrand(objects::eNa_strand_plus);
        results.push_back(orf);
    }

    // find ORFs on the complement and munge the numbers
    ranges.clear();
    Seq comp(seq);

    // compute the complement;
    // this should be replaced with new Seqport_util call
    reverse(comp.begin(), comp.end());
    NON_CONST_ITERATE (Seq, iter, comp) {
        *iter =
            objects::CSeqportUtil::GetIndexComplement(objects::eSeq_code_type_iupacna,
                                                      *iter);
    }

    x_FindForwardOrfs(comp, ranges, min_length_bp, genetic_code);
    ITERATE (COrf::TRangeVec, iter, ranges) {
        CRef<objects::CSeq_loc> orf(new objects::CSeq_loc);
        orf->SetInt().SetFrom  (comp.size() - iter->GetTo() - 1);
        orf->SetInt().SetTo    (comp.size() - iter->GetFrom() - 1);
        orf->SetInt().SetStrand(objects::eNa_strand_minus);
        results.push_back(orf);
    }
}


//
// find ORFs in a string
void COrf::FindOrfs(const string& seq_iupac,
                    TLocVec& results,
                    unsigned int min_length_bp,
                    int genetic_code)
{
    x_FindOrfs(seq_iupac, results, min_length_bp, genetic_code);
}


//
// find ORFs in a vector<char>
void COrf::FindOrfs(const vector<char>& seq_iupac,
                    TLocVec& results,
                    unsigned int min_length_bp,
                    int genetic_code)
{
    x_FindOrfs(seq_iupac, results, min_length_bp, genetic_code);
}


//
// find ORFs in a CSeqVector
void COrf::FindOrfs(const CSeqVector& orig_vec,
                    TLocVec& results,
                    unsigned int min_length_bp,
                    int genetic_code)
{
    string seq_iupac;  // will contain ncbi8na
    CSeqVector vec(orig_vec);
    vec.SetCoding(CSeq_data::e_Iupacna);
    vec.GetSeqData(0, vec.size(), seq_iupac);
    x_FindOrfs(seq_iupac, results, min_length_bp, genetic_code);
}


END_NCBI_SCOPE

/*
 * ===========================================================================
 * $Log$
 * Revision 1.1  2003/08/21 12:00:53  dicuccio
 * Added private implementation file for COrf
 *
 * ===========================================================================
 */
