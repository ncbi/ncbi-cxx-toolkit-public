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
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbidbg.hpp>
#include <corelib/ncbimisc.hpp>
#include <algo/sequence/adapter_search.hpp>

#include <cmath>
#include <algorithm>
#include <vector>
#include <set>
#include <queue>

USING_NCBI_SCOPE;

string NAdapterSearch::s_AsIUPAC(
        TWord w, 
        size_t mer_size)
{
    string s("");
    s.resize(mer_size);
    static const char* alphabet = "ACGT";
    for(size_t i = 0; i < mer_size; i++) {
        s[mer_size - 1 - i] = alphabet[w & 3];
        w >>= 2;
    }
    return s;
}

string NAdapterSearch::s_AsIUPAC(
        const TWords& words, 
        size_t mer_size)
{
    if(words.size() == 0) {
        return "";
    }

    string iupac;
    iupac.resize(words.size() - 1);
    static const char* alphabet = "ACGT";
    for(size_t i = 0; i < words.size() - 1; i++) {
        iupac[i] = alphabet[words[i] >> (mer_size * 2 - 2)];
    }
    iupac += s_AsIUPAC(words.back(), mer_size);
    return iupac;
}


double NAdapterSearch::s_GetWordComplexity(TWord word)
{
    vector<Uint1> counts(64, 0);
    Uint8 w2 = word | (word << (MER_LENGTH * 2));
    for(size_t i = 0; i < MER_LENGTH; i++) {
        counts[w2 & 0x3F] += 1; //counting trinucleotides (6 bits each)
        w2 >>= 2;
    }
    size_t sumsq(0);
    for(size_t i = 0; i < 64; i++) {
        size_t c = counts[i];
        sumsq += c*c;
    }
    return (144 - sumsq)/132.0; //normalize to [0..1]
}



void NAdapterSearch::s_Translate(
        const char* const seq,
        size_t seq_size, 
        bool revcomp, TWords& words)
{
    if(seq_size < MER_LENGTH) {
        words.resize(0);
        return;
    } else {
        words.resize(seq_size - (MER_LENGTH - 1));
    }

    //(A,C,G,T,*) -> (0, 1, 2, 3, 0)
    static const Uint1 char2letter[] = {
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 1, 0, 0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 
        0, 0, 0, 0, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 1, 0, 0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 
        0, 0, 0, 0, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
    };

    // mask of set bits; spanning MER_LENGTH-1 letters
    TWord mask = (1 << ((MER_LENGTH - 1) * 2)) - 1;

    if(!revcomp) {
        TWord w0 = 0;
        for(size_t i = 0; i < MER_LENGTH; i++) {
            Uint1 c = char2letter[static_cast<int>(seq[i])];
            w0 = (w0 << 2) | c;
        }
        words[0] = w0;
        
        int k = MER_LENGTH - 1;
        for(size_t i = 1; i < words.size(); i++) {
            Uint1 c = char2letter[static_cast<int>(seq[i + k])];
            words[i] = ((words[i - 1] & mask) << 2) | c;
        }
    } else {
        TWord w0 = 0;
        for(size_t i = 0; i < MER_LENGTH; i++) {
            Uint1 c = 3 - char2letter[static_cast<int>(seq[seq_size - 1 - i])];
            w0 = (w0 << 2) | c;
        }
        words[0] = w0;

        int k = seq_size - MER_LENGTH;
        for(size_t i = 1; i < words.size(); i++) {
            Uint1 c = 3 - char2letter[static_cast<int>(seq[k - i])];
            words[i] = ((words[i - 1] & mask) << 2) | c;
        }
    }
}


void NAdapterSearch
   ::CPairedEndAdapterDetector
   ::CConsensusPattern
   ::AddExemplar(
        TWords::const_iterator begin, 
        TWords::const_iterator end)
{
    size_t words_size = end - begin;
    size_t len = min(words_size, m_len);

    // Store position-specific 10-mer counts; 
    // (10-mer is in upper bits of 12-mer)
    for(size_t i = 0; i < len; i++) {
        x_IncrCount(i, *(begin + i) >> 4);
    }

    // Can get two more 10-mers out of last 12-mer
    if(words_size > 0 && words_size < m_len) {
        size_t len = min<size_t>(2, m_len - words_size);
        TWord w = *(begin + words_size);
        for(size_t i = 0; i < len; i++) {
            size_t pos = words_size + i;
            TWord w2 = (w >> (2 - i*2)) & 0xFFFFF;
            x_IncrCount(pos, w2);
        }
    }
}


NAdapterSearch::TWord NAdapterSearch
     ::CPairedEndAdapterDetector
     ::CConsensusPattern
     ::x_NextWord(size_t pos, TWord prev_word) const
{
    TWord best_word = 0;
    size_t best_count = 0;
    for(Uint1 letter = 0; letter < 4; letter++) {
        TWord word = ((prev_word << 2) & 0xFFFFF) | letter;
        TCount count = x_GetCount(pos + 1, word);
        if(count > best_count) {
            best_count = count;
            best_word = word;
        }
    }
    return best_word;
}


string NAdapterSearch
     ::CPairedEndAdapterDetector
     ::CConsensusPattern
     ::InferConsensus(const SParams& params) const
{
    // Find the best (most-frequent) and 2nd-best word at position 0
    size_t top_sup(0);
    size_t secondbest_sup(0);
    size_t best_word(0);
    for(size_t word = 0; word < NMERS10; word++) {
        size_t sup = m_counts[word];
        if(sup > top_sup && !NAdapterSearch::s_IsLowComplexity(word)) {
            secondbest_sup = top_sup;
            top_sup = sup;
            best_word = word;
        }
    }

    if(!params.HaveInitialSupport(top_sup, secondbest_sup)) {
        return "";
    } 

    TWords words(m_len);
    words[0] = best_word;

    ERR_POST(Info 
          << "Seed: " 
          << s_AsIUPAC(best_word,10) 
          << "; overrepresentation: " 
          << top_sup 
          << "/" 
          << secondbest_sup);

    // Extend the sequence from the starting word
    for(size_t i = 1; i < words.size(); i++) {
        TWord last_word = words[i - 1];
        TCount last_sup = x_GetCount(i - 1, last_word);

        TWord curr_word = x_NextWord(i - 1, last_word);
        TCount curr_sup = x_GetCount(i,     curr_word);

        if(params.HaveContinuedSupport(top_sup, last_sup, curr_sup)) {
            words[i] = curr_word;
        } else {
            words.resize(i);
            break;
        }
    }

    return s_AsIUPAC(words, 10);
}

///////////////////////////////////////////////////////////////////////

pair<size_t, size_t> NAdapterSearch
    ::CPairedEndAdapterDetector
    ::s_FindAdapterStartPos(
        const TWords& fwd_read, 
        const TWords& rev_read)
{
    return pair<size_t, size_t>(
        find(fwd_read.begin(), fwd_read.end(), rev_read.back()) 
            - fwd_read.begin() + MER_LENGTH,

        find(rev_read.begin(), rev_read.end(), fwd_read.front()) 
            - rev_read.begin());
}


void NAdapterSearch
    ::CPairedEndAdapterDetector
    ::AddExemplar(
        const char* spot,
        size_t spot_len)
{
    size_t len = spot_len / 2;
    const char* fwd_read = spot;
    const char* rev_read = spot+len;

    TWords fwd_words;
    TWords rev_words;
    NAdapterSearch::s_Translate(fwd_read, len, false, fwd_words);
    NAdapterSearch::s_Translate(rev_read, len, true,  rev_words); 
        // note: translating in rev_read reverse-orientation,
        // such that translations are in consistent orientation
        // so we can check for overlap

    pair<size_t, size_t> pos = s_FindAdapterStartPos(fwd_words, rev_words);

    bool is_consistent = (pos.first + pos.second == len); 
        // this is a likely true-positive rather than a random match

    size_t adapter_len = len - pos.first; // remaining portion of the read
    if(is_consistent && adapter_len >= MER_LENGTH) {
        m_cons5.AddExemplar(fwd_words.begin() + pos.first, fwd_words.end());

        // We translated R-read in rev-comp above for the purpose of 
        // detecting self-overlap within a spot, but here we'll need to do it 
        // again without rev-comp for the purpose of consensus-finding. 
        // We don't need to translate the whole read, just the adapter part.
        TWords words2;
        s_Translate(rev_read + pos.first, adapter_len, false, words2);
        m_cons3.AddExemplar(words2.begin(), words2.end());
    }
}

///////////////////////////////////////////////////////////////////////////

void NAdapterSearch::CUnpairedAdapterDetector::AddExemplar(
        const char* seq,
        size_t len)
{
    TWords words;
    s_Translate(seq, len, false, words);
    ITERATE(TWords, it, words) {
        m_counts[*it] += 1;
    }        
}


string NAdapterSearch::CUnpairedAdapterDetector::InferAdapterSeq() const
{
    TWord seed = x_FindAdapterSeed();
    if(!seed) {
        return "";  
    }

    // subsequent words will need to satisfy minimum support
    // relative to the originating seed.
    size_t top_count = m_counts[seed];

    // Get sequence of overlapping words starting at the seed
    TWords words;
    words.push_back(seed);
    x_ExtendSeed(words, top_count, false);  // extend left
    reverse(words.begin(), words.end());    // fix the order left->right
    x_ExtendSeed(words, top_count, true);   // extend right

    // The non-biological sequence following a read is often a homopolymer 
    // (usually A+), so statistically it looks like part of the adapter.
    // We'll truncate the homopolymer suffix if last word is a homopolymer

    string seq = s_AsIUPAC(words);
    if(NAdapterSearch::s_IsHomopolymer(words.back())) {
        char c = seq[seq.size() - 1]; 
        while(seq.size() > 0 && seq[seq.size() - 1] == c) {
            seq.resize(seq.size() - 1); 
            words.pop_back();
        }   
    }   
    return seq;
}


NAdapterSearch::TWord NAdapterSearch
    ::CUnpairedAdapterDetector
    ::x_FindAdapterSeed() const
{
    typedef pair<TCount, TWord> TPair; 
    typedef priority_queue< TPair, vector<TPair>, greater<TPair> > TTopWords;
    TTopWords top_words;
    
    // Collect most frequent words. The count must be large enough such that 
    // that most of these words are biological (not read-specific). The idea
    // is to check whether most frequent word (potentially adapter-specific)
    // is overrepresented relative to biological seqs
    for(TWord w = 0; w < m_counts.size(); w++) {
        Uint8 count = m_counts[w];
        if(count > m_params.min_support && !NAdapterSearch::s_IsLowComplexity(w)) {
            top_words.push(TPair(count, w));
            while(top_words.size() > m_params.top_n) {
                top_words.pop();
            }
        }
    }

    // Calculate geometric mean of counts
    double sumlogs(0);
    TWord word(0); // last word and occ will be the most frequent one
    TCount sup(0);
    size_t n = top_words.size();
    for(; !top_words.empty(); top_words.pop()) {
        sup = top_words.top().first;
        word = top_words.top().second;
        sumlogs += log(sup + 1.0);
    }

    double gmean = n == 0 ? 0 : exp(sumlogs / n) - 1;

    ERR_POST(Info 
          << "Seed: "
          << s_AsIUPAC(word) 
          << "; overrepresentation: " 
          << sup 
          << "/" 
          << static_cast<size_t>(gmean));

    return m_params.HaveInitialSupport(sup, gmean) ? word : 0;
}


NAdapterSearch::TWord NAdapterSearch::CUnpairedAdapterDetector::x_GetAdjacent(
        TWord w, 
        bool right) const
{
    TCount best_count(0);
    TWord best_word(0);

    for(Uint1 letter = 0; letter < 4; letter++) {
        TWord w2 = right ? ((w << 2) & 0xFFFFFF) | letter
                         :  (w >> 2)             | (letter << 22);

        TCount count = m_counts[w2];
        if(count > best_count) {
            best_count = count;
            best_word = w2;
        }
    }
    return best_word;
}


void NAdapterSearch::CUnpairedAdapterDetector::x_ExtendSeed(
        TWords& words,
        size_t top_sup,
        bool right) const
{
    while(true) {
        TWord prev_word = words.back();
        TWord curr_word = x_GetAdjacent(prev_word, right);
        TCount prev_sup = m_counts[prev_word];
        TCount curr_sup = m_counts[curr_word];

        if(!m_params.HaveContinuedSupport(top_sup, prev_sup, curr_sup)
           || find(words.begin(), words.end(), curr_word) != words.end()) 
        {
            // Stop extending if support is too low, or
            // this word is already in the sequence (to prevent loops)
            break;
        } else {
            words.push_back(curr_word);
        }
    }
}


///////////////////////////////////////////////////////////////////////

struct SOpLess_Second
{
    bool operator()(const NAdapterSearch::CSimpleUngappedAligner::TRange& a, 
                    const NAdapterSearch::CSimpleUngappedAligner::TRange& b) const
    {
        return a.second < b.second;
    }
};

NAdapterSearch::CSimpleUngappedAligner::TRange
  NAdapterSearch
::CSimpleUngappedAligner
::GetSeqRange(TPos pos) const
{
    TRanges::const_iterator it = lower_bound(
            m_subseq_ranges.begin(),
            m_subseq_ranges.end(),
            TRange(pos, pos),
            SOpLess_Second());

    return it == m_subseq_ranges.end() ? TRange(NULLPOS, NULLPOS) : *it;
}

const NAdapterSearch::CSimpleUngappedAligner::SMatch& 
  NAdapterSearch
::CSimpleUngappedAligner
::x_GetBetterOf(const SMatch& a, const SMatch& b) const
{
    if(a.len > 0 && b.len == 0) {
        return a;
    } else if(a.len == 0 && b.len > 0) {
        return b;
    } else {
        TRange ar = GetSeqRange((a.second - a.first) / 2);
        TRange br = GetSeqRange((b.second - b.first) / 2);

        // calculate unaligned length 
        // note ::len is doubled on antidiag
        TPos a_un = (ar.second - ar.first) - a.len/2;
        TPos b_un = (br.second - br.first) - b.len/2;

#if 0
        LOG_POST((a.second + a.first) / 2 
                << " " << (a.second - a.first) / 2 
                << " " << a.len 
                << " " << a_un 
                << " " << m_seq.substr(ar.first, ar.second - ar.first));
        LOG_POST((b.second + b.first) / 2 
                << " " << (b.second - b.first) / 2 
                << " " << b.len 
                << " " << b_un 
                << " " << m_seq.substr(br.first, br.second - br.first));
        LOG_POST("\n");
#endif
       
        // score is proportional to the length, and penalized
        // proportonally to the log of unaligned length 
        double a_score = a.len - log(1.0 + a_un)*5;
        double b_score = b.len - log(1.0 + b_un)*5;
        return a_score < b_score ? b : a;
    }
}


void NAdapterSearch::CSimpleUngappedAligner::s_IndexWord(
        TWord word,
        TPos pos,
        TPositions& vec_index, 
        TCoordSet& coordset)
{
    TWords approx_words;
    s_PermuteMismatches(word, approx_words);

    ITERATE(TWords, it2, approx_words) {
        TWord w = *it2;

        // store word->pos in vec-index;
        // if encountered multiplicity, move
        // the values to coordset and mark as
        // MULTIPOS in vec-index

        TPos& pos_ref = vec_index[w];
        if(pos_ref == pos || pos_ref == NULLPOS) {
            pos_ref = pos;
        } else {
            if(pos_ref != MULTIPOS) {
                coordset.insert(TCoordSet::value_type(w, pos_ref));
                pos_ref = MULTIPOS;
            }
            coordset.insert(TCoordSet::value_type(w, pos));
        }
    }
}


void NAdapterSearch::CSimpleUngappedAligner::s_CoordSetToMapIndex(
        const TCoordSet& coordset,
        TMapIndex& map_index,
        TPositions& positions)
{
     map_index.clear();
     positions.resize(coordset.size());
     TPositions::iterator dest = positions.begin();

     ITERATE(TCoordSet, it, coordset) {
        TWord w = it->first;
        TPos pos = it->second;
       
        *dest = pos; 
        if(map_index.find(w) == map_index.end()) {
            map_index[w].first = dest;
        }
        dest++;
        map_index[w].second = dest;
     }
}


void NAdapterSearch::CSimpleUngappedAligner::Init(const char* seq, size_t len)
{
    m_seq.resize(len);
    m_seq.assign(seq, len);

    m_subseq_ranges.clear();

    m_vec_index.resize(1 << (MER_LENGTH*2));
    fill(m_vec_index.begin(), m_vec_index.end(), (TPos)NULLPOS);

    m_positions.clear();
    m_map_index.clear();

    // process each '-'-delimited substring.
    TCoordSet coordset;
    const char* endend = seq+len;
    for(const char *begin = seq,    *end = find(begin, endend, '-');
                    begin < endend;
                    begin = end + 1, end = find(begin, endend, '-'))
    {
        TRange r(begin - seq, end - seq);
        m_subseq_ranges.push_back(r);

        TWords words;
        s_Translate(begin, r.second - r.first, false, words);

        for(size_t i = 0; i < words.size(); i++) {
            TPos pos = r.first + i;
            s_IndexWord(words[i], pos, m_vec_index, coordset);
        }
    }

    s_CoordSetToMapIndex(coordset, m_map_index, m_positions);
}


void NAdapterSearch::CSimpleUngappedAligner::s_PermuteMismatches(
        TWord w, 
        TWords& words)
{
    words.resize(240); // nCr(6,2)*4^2
    TWords::iterator dest = words.begin();
    for(size_t i1 = 3; i1 < 9; i1++) {

        for(Uint1 c1 = 0; c1 < 4; c1++) {
            TWord w1 = s_Put(w, i1, c1);

            for(size_t i2 = i1+1; i2 < 9; i2++) {

                for(Uint1 c2 = 0; c2 < 4; c2++) {
                    *(dest++) = s_Put(w1, i2, c2);
                }
            }
        }
    }
}


bool NAdapterSearch::CSimpleUngappedAligner::s_Merge(
        SMatch& m1, 
        const SMatch& m2)
{
    if(m1.first == NULLPOS) {
        m1 = m2;
        return true;
    } else if(m1.first == m2.first
           && m1.second + m1.len + 2 >= m2.second)
    {
        // on same diag and [almost-]overlapping on antidiag 
        // - extend the length of m1 to the end of m2
        m1.len = m2.len + m2.second - m1.second;
        return true;
    } else {
        return false;
    }
}


NAdapterSearch::CSimpleUngappedAligner::SMatch 
NAdapterSearch::CSimpleUngappedAligner::x_CreateMatch(
        TPos q_pos,
        TPos s_pos) const
{
    SMatch m;
    m.first  = q_pos - s_pos;
    m.second = q_pos + s_pos;
    m.len = MER_LENGTH * 2; // lengths are doubled on antidiag
    return m;
}


void NAdapterSearch::CSimpleUngappedAligner::x_Extend(
        SMatch& seg, 
        const char* q_seq, 
        const size_t query_len,
        const bool direction, //true iff forward
        const int match_score,
        const int mismatch_score,
        const int dropoff_score) const
{   
    int delta_pos = direction ? 1 : -1; 

    // Create a point at the first or last position of
    // the initial segment, depending on direction
    typedef pair<TPos, TPos> TPoint;
    TPoint p(seg.first  + (direction ? seg.len - 1 : 0),
             seg.second + (direction ? seg.len - 1 : 0)); 

    Int8 score(0), max_score(0);
    TPoint best_p = p;

    // bounds are [min, max)
    TRange q_bounds(0, query_len);
    TRange s_bounds = GetSeqRange(seg.second);

    // the initial point is apriori matched, so advance
    p.first  += delta_pos;
    p.second += delta_pos;
    while(score + dropoff_score > max_score
          && p.first  >= q_bounds.first 
          && p.first  <  q_bounds.second 
          && p.second >= s_bounds.first
          && p.second <  s_bounds.second)
    {   
        score += (q_seq[p.first] == m_seq[p.second]) ? match_score 
                                                     : mismatch_score;
#if 0
        LOG_POST(direction 
                 << " " << q_seq[p.first] 
                 << " " << m_seq[p.second] 
                 << " " << score 
                 << " " << best_p.first);
#endif
        
        if(score > max_score) {
            max_score = score;
            best_p = p;
        }   

        p.first  += delta_pos;
        p.second += delta_pos;
    }   

    if(direction) {
        // starting coordinates are the same; length increased
        seg.len = best_p.first - seg.first + 1;
    } else {
        // starting coordinates are at best_p; 
        // length is distance to the starting coords + original length
        SMatch seg2;
        seg2.first  = best_p.first;
        seg2.second = best_p.second;
        seg2.len    = seg.len + seg.first - best_p.first;
        seg = seg2;
    }   
} 


NAdapterSearch::CSimpleUngappedAligner::SMatch 
NAdapterSearch::CSimpleUngappedAligner::FindSingleBest(
        const char* query,
        size_t len) const
{
    SMatch best_match;
 
    typedef map<TPos, SMatch> TMatches;
    TMatches matches; //diag -> match

    TWords words;
    s_Translate(query, len, false, words);

    for(size_t i = 0; i < words.size(); i++) {
        TWord w = words[i];
       
        TPosRange r(m_vec_index.begin() + w,
                    m_vec_index.begin() + w + 1);

        if(*r.first == NULLPOS) {
            continue;
        } else if(*r.first == MULTIPOS) {
            r = m_map_index.find(w)->second;
        }

        // For each matched position in db for this word
        // create a Match and try to merge it with existing
        // overlapping Match on this diag, if any. If can't merge,
        // we'll never be able to merge anything into the old mMatch
        // because following matches on this diag will be even
        // farther on the antidiag, so we can drop the old match
        // (but keep it it is best so far), and replace it with the
        // new one.
        for(TPositions::const_iterator it = r.first; it != r.second; ++it) {
            SMatch new_match = x_CreateMatch(i, *it);
            SMatch& old_match = matches[new_match.first];

            if(!s_Merge(old_match, new_match)) {
                best_match = x_GetBetterOf(best_match, old_match);
                old_match = new_match;
            }
        }
    }

    ITERATE(TMatches, it, matches) {
        best_match = x_GetBetterOf(best_match, it->second);
    }

    // Convert (diag,antidiag) representation to (query,target)
    SMatch match;
    match.len            = best_match.len / 2;
    match.first          = (best_match.first + best_match.second) / 2;
    match.second         = best_match.second - match.first;

    // Do ungapped extension both ways.
    x_Extend(match, query, len, true);
    x_Extend(match, query, len, false);

    return match;
}

