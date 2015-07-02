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
 * Authors:  Christiam Camacho
 *
 * File Description:
 *
 */

#include <ncbi_pch.hpp>
#include <algo/sequence/util.hpp>
#include <objects/seq/seq__.hpp>
#include <objects/seqloc/seqloc__.hpp>
#include <objmgr/seq_vector.hpp>
#include <objects/seq/seqport_util.hpp>

#include <math.h>

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);

CRef<objects::CBioseq>
SeqLocToBioseq(const objects::CSeq_loc& loc, objects::CScope& scope)
{
    CRef<CBioseq> bioseq;
    if ( !loc.GetId() ) {
        return bioseq;
    }

    // Build a Seq-entry for the query Seq-loc
    CBioseq_Handle handle = scope.GetBioseqHandle(*loc.GetId());
    if( !handle ){
        return bioseq;
    }

    bioseq.Reset( new CBioseq() );

    // add an ID for our sequence
    CRef<CSeq_id> id(new CSeq_id());
    id->Assign(*handle.GetSeqId());
    bioseq->SetId().push_back(id);

    // a title
    CRef<CSeqdesc> title( new CSeqdesc() );
    string title_str;
    id -> GetLabel(&title_str );
    title_str += ": ";
    loc.GetLabel( &title_str );
    title->SetTitle( title_str );
    bioseq->SetDescr().Set().push_back( title );

    ///
    /// create the seq-inst
    /// we can play some games here
    ///
    CSeq_inst& inst = bioseq->SetInst();

    if (handle.IsAa()) {
        inst.SetMol(CSeq_inst::eMol_aa);
    } else {
        inst.SetMol(CSeq_inst::eMol_na);
    }

    bool process_whole = false;
    if (loc.IsWhole()) {
        process_whole = true;
    } else if (loc.IsInt()) {
        TSeqRange range = loc.GetTotalRange();
        if (range.GetFrom() == 0  &&  range.GetTo() == handle.GetBioseqLength() - 1) {
            /// it's whole
            process_whole = true;
        }
    }

    /// BLAST now handles delta-seqs correctly, so we can submit this
    /// as a delta-seq
    if (process_whole) {
        /// just encode the whole sequence
        CSeqVector vec(loc, scope, CBioseq_Handle::eCoding_Iupac);
        string seq_string;
        vec.GetSeqData(0, vec.size(), seq_string);

        inst.SetRepr(CSeq_inst::eRepr_raw);
        inst.SetLength(seq_string.size());
        if (vec.IsProtein()) {
            inst.SetMol(CSeq_inst::eMol_aa);
            inst.SetSeq_data().SetIupacaa().Set().swap(seq_string);
        } else {
            inst.SetMol(CSeq_inst::eMol_na);
            inst.SetSeq_data().SetIupacna().Set().swap(seq_string);
            CSeqportUtil::Pack(&inst.SetSeq_data());
        }
    } else {
        inst.SetRepr(CSeq_inst::eRepr_delta);
        inst.SetLength(handle.GetBioseqLength());
        CDelta_ext& ext = inst.SetExt().SetDelta();

        ///
        /// create a delta sequence
        ///

        //const CSeq_id& id = sequence::GetId(loc, &scope);
        //ENa_strand strand = sequence::GetStrand(loc, &scope);
        TSeqRange range = loc.GetTotalRange();

        /// first segment: gap out to initial start of seq-loc
        if (range.GetFrom() != 0) {
            ext.AddLiteral(range.GetFrom());
        }

        CSeq_loc_CI loc_iter(loc);
        if (loc_iter) {
            TSeqRange  prev   = loc_iter.GetRange();
            ENa_strand strand = loc_iter.GetStrand();

            do {
                /// encode a literal for the included bases
                CRef<CSeq_loc> sl =
                    handle.GetRangeSeq_loc(prev.GetFrom(), prev.GetTo(), strand);

                CSeqVector vec(*sl, scope, CBioseq_Handle::eCoding_Iupac);
                string seq_string;
                vec.GetSeqData(0, vec.size(), seq_string);

                ext.AddLiteral(seq_string,
                    (vec.IsProtein() ? CSeq_inst::eMol_aa : CSeq_inst::eMol_na));

                /// skip to the next segment
                /// we may need to include a gap
                ++loc_iter;
                if (loc_iter) {
                    TSeqRange next = loc_iter.GetRange();
                    ext.AddLiteral(next.GetFrom() - prev.GetTo());
                    prev = next;
                    strand = loc_iter.GetStrand();
                }
            }
            while (loc_iter);

            /// gap at the end
            if (prev.GetTo() < handle.GetBioseqLength() - 1) {
                ext.AddLiteral(handle.GetBioseqLength() - prev.GetTo() - 1);
            }
        }
    }

    return bioseq;
}


//////////////////////////////////////////////////////////////////////////////
///
/// Sequence Entropy Calculation
///
/// This is a straightforward computation of Shannon entropy for all tetramers
/// observed in a given sequence.  This was tuned to work specifically with DNA
/// sequence.  While it should also apply for proteins, the denominator is much
/// different in that case (20^4 instead of 4^4).
///
///
/// The function has been heavily optimized to use caching of partial results.
/// For large sequence sets, this caching makes a huge difference.  The
/// notional normative function we are trying to reproduce is included inline
/// below for reference.

#if 0
///
/// This is the normative copy of the function
///
double ComputeNormalizedEntropy(const CTempString& sequence,
                               size_t word_size)
{
    typedef map<CTempString, double> TCounts;
    TCounts counts;
    double total = sequence.size() - word_size;
    for (size_t i = word_size;  i < sequence.size();  ++i) {
        CTempString t(sequence, i - word_size, word_size);
        TCounts::iterator it =
            counts.insert(TCounts::value_type(t, 0)).first;
        it->second += 1;
    }

    NON_CONST_ITERATE (TCounts, it, counts) {
        it->second /= total;
    }

    double entropy = 0;
    ITERATE (TCounts, it, counts) {
        entropy += it->second * log(it->second);
    }
    double denom = pow(4, word_size);
    denom = min(denom, total);
    entropy = -entropy / log(denom);
    return max<double>(0.0, entropy);
}
#endif


CEntropyCalculator::CEntropyCalculator(size_t sequence_size, size_t word_size)
    : m_WordSize(word_size)
    , m_NumWords(sequence_size - word_size)
    , m_Denom(log(min((double)m_NumWords,
                      pow(4.0, (int)word_size))))
{
    if (word_size > sequence_size) {
        NCBI_THROW(CException, eUnknown,
                   "entropy is undefined when the sequence size is "
                   "smaller than the word size");
    }

    m_Words.resize(m_NumWords);
    m_EntropyValues.resize(m_NumWords+1, -1.);

    /// Special case for count 0; don't calculate, to avoid calling log(0)
    m_EntropyValues[0] = 0;
}

double CEntropyCalculator::ComputeEntropy(const CTempString& sequence)
{
    NCBI_ASSERT(sequence.size() - m_WordSize == m_NumWords,
                "Sequence of wrong length");

    for (size_t i = 0;  i < m_NumWords;  ++i) {
        m_Words[i].assign(sequence, i, m_WordSize);
    }
    sort(m_Words.begin(), m_Words.end());

    double entropy = 0;
    for (size_t i = 0, count = 1; i < m_NumWords; ++i, ++count) {
        if (i == m_NumWords-1 || m_Words[i] != m_Words[i+1]) {
            entropy += x_Entropy(count);
            count = 0;
        }
    }
    return max<double>(0.0, entropy);
}

vector<double> CEntropyCalculator::
ComputeSlidingWindowEntropy(const CTempString& sequence)
{
    NCBI_ASSERT(sequence.size() - m_WordSize >= m_NumWords,
                "Sequence too short");

    size_t window = m_NumWords + m_WordSize;
    TCounts counts;

    for (size_t i = 0;  i < m_NumWords;  ++i) {
        CTempString t(sequence, i, m_WordSize);
        TCounts::iterator it =
            counts.insert(TCounts::value_type(t, 0)).first;
        ++it->second;
    }

    /// The first half-window all has the same entropy, calculated over the
    /// beginning of the sequence
    vector<double> results(window/2+1, x_Entropy(counts));

    for (size_t pos = 0; pos < sequence.size()-window; ++pos) {
        CTempString removed_word(sequence, pos, m_WordSize);
        --counts[removed_word];
        CTempString added_word(sequence, pos+m_NumWords, m_WordSize);
        TCounts::iterator it =
            counts.insert(TCounts::value_type(added_word, 0)).first;
        ++it->second;
        results.push_back(x_Entropy(counts));
    }

    /// last half-window again has the same entropy
    results.resize(sequence.size(), x_Entropy(counts));
    return results;
}

double CEntropyCalculator::x_Entropy(size_t count)
{
    double &entropy_value = m_EntropyValues[count];
    if (entropy_value < 0) {
        /// Lazy evaluation of entropy value
        double fraction = (double)count / m_NumWords;
        entropy_value = -fraction * log(fraction) / m_Denom;
    }
    return entropy_value;
}

double CEntropyCalculator::x_Entropy(const TCounts &counts)
{
    double entropy = 0;
    ITERATE (TCounts, word_it, counts) {
        entropy += x_Entropy(word_it->second);
    }
    return max<double>(0.0, entropy);
}

double ComputeNormalizedEntropy(const CTempString& sequence,
                               size_t word_size)
{
    return CEntropyCalculator(sequence.size(), word_size)
               .ComputeEntropy(sequence);
}


END_NCBI_SCOPE
