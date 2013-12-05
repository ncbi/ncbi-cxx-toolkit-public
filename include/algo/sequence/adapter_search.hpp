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

#ifndef ALGO_SEQUENCE___ADAPTE_SEARCH__HPP
#define ALGO_SEQUENCE___ADAPTE_SEARCH__HPP

#include <corelib/ncbitype.h>
#include <corelib/ncbistl.hpp>

#include <vector>
#include <set>
#include <map>

BEGIN_NCBI_SCOPE

class NCBI_XALGOSEQ_EXPORT NAdapterSearch // used as namespace
{
private:
    /// Will use 24-bit 12-mers as words
    static const size_t MER_LENGTH = 12;
    typedef Uint4 TWord; 
    typedef vector<TWord> TWords;    

    typedef Uint4 TCount;
    typedef vector<TCount> TCounts; 

    /// Translate IUPAC sequence to a sequence of overlapping words
    /// (A,C,G,T,*) <-> (0,1,2,3,0)
    static void s_Translate(
            const char* const iupac_na, 
            size_t len, 
            bool apply_revcomp, 
            TWords& words);

    /// Convert word to IUPAC string [ACGT]{MER_LENGTH}
    static string s_AsIUPAC(
            TWord w, 
            size_t mer_size = MER_LENGTH);

    /// Convert sequence of overlapping words to an IUPAC string 
    /// (inverse of Translate)
    static string s_AsIUPAC(
            const TWords& words,
            size_t mer_size = MER_LENGTH);

    /// Complexity is a value between 0 (for a homopolymer) and 1
    /// (max complexity). Average for all words is 0.984375
    /// Based on sum of squares of trinucleotide counts
    static double s_GetWordComplexity(TWord word);

    /// With threshold of 0.9, 0.7% of all words are classified as such
    static bool s_IsLowComplexity(TWord word)
    {
        return s_GetWordComplexity(word) < 0.9;
    }

    static bool s_IsHomopolymer(TWord w)
    {
        return w == 0 || w == 0x555555 || w == 0xAAAAAA || w == 0xFFFFFF;
    }

    NAdapterSearch()
    {}

    NAdapterSearch& operator=(const NAdapterSearch& other)
    {
        return *this;
    }

public:
    ///////////////////////////////////////////////////////////////////////
    //
    class IAdapterDetector
    {
    public:
        /// An adapter sequence is presumed to occur at least
        /// min_support times in the input, and is overrepresented by 
        /// a factor of at least min_init_factor relative to most frequent
        /// biological words.
        /// The seed word will be extended as long as it remains frequent enough
        /// and does not drop off too sharply relative to adjacent word.
        struct SParams
        {
            size_t min_support;
            size_t top_n;
            float  min_init_factor;
            float  min_ext_factor_adj;
            float  min_ext_factor_top;

            SParams() 
              : min_support(100)
              , top_n(1000) 
              , min_init_factor(10)
              , min_ext_factor_adj(0.5)
              , min_ext_factor_top(0.2)
            {}

            // An originating word must be overrepresented by a factor of
            // at least min_init_factor relative to background, and satisfy
            // absolute minimum of min_support
            bool HaveInitialSupport(size_t top_candidate_sup,
                                    size_t non_candidate_sup) const
            {
                return top_candidate_sup > non_candidate_sup * min_init_factor
                    && top_candidate_sup > min_support;
            }

            // Will extend sequence as long as support is within
            // factor of min_ext_factor_top from the originating seed, and
            // factor of min_ext_factor_adj from the neighbor
            // (i.e. support does not drop too sharply)
            bool HaveContinuedSupport(size_t top_sup,
                                      size_t prev_sup,
                                      size_t candidate_sup) const
            {
                return candidate_sup > top_sup  * min_ext_factor_top
                    && candidate_sup > prev_sup * min_ext_factor_adj;
            }
        };

        SParams& SetParams() 
        {
            return m_params;
        }

        const SParams& GetParams() const
        {
            return m_params;
        }

        /// Add sequence of a spot from an SRA run (single or paired-end read)
        virtual void AddExemplar(const char* seq, size_t len) = 0;

        /// returns IUPAC seq of the inferred adapter seq; empty if not found.
        virtual string InferAdapterSeq() const = 0;

        virtual ~IAdapterDetector() 
        {}

    protected:
        SParams m_params;
    };


    /// This class assembles adapter sequence based on distribution of word counts
    class CUnpairedAdapterDetector : public IAdapterDetector
    {
    public:
        CUnpairedAdapterDetector()
            : m_counts(1 << (MER_LENGTH*2), 0)
        {}

        virtual void AddExemplar(const char* seq, size_t len);

        /// returns IUPAC seq of the inferred adapter seq; empty if not found.
        virtual string InferAdapterSeq() const;

    private:
        /// Find a word that is likely adapter-specific, if any.
        TWord x_FindAdapterSeed() const;

        /// if right,  return most frequent word whose 11-prefix = 11-suffix of w;
        /// otherwise, return most frequent word whose 11-suffix = 11-prefix of w
        TWord x_GetAdjacent(TWord w, bool right) const;

        /// Extend the seed one way, as long as it satisfies extension thresholds.
        void x_ExtendSeed(TWords& words, size_t top_count, bool right) const;

    private:
        TCounts m_counts; //counts of words in the seq; not position-specific.
    };


    class CPairedEndAdapterDetector : public IAdapterDetector
    {
    public:

        /// CConsensusPattern calculates most frequent pattern from a 
        /// set of (noisy) exemplars based on distribution of 10-mer 
        /// frequencies at every position of the pattern.
        ///
        ///  inputs    | output
        ///  ----------|---------
        ///  ACGTACGT  |
        ///  ATATATAT  | ACGTACGT
        ///  ACGTACGT  |
        ///  CGCGCGCG  |
        class CConsensusPattern
        {
        public:
            static const size_t NMERS10 = 1048576; //total count of possible 10-mers: 4^10

            CConsensusPattern(size_t max_pattern_len)
               : m_len(max_pattern_len)
               , m_counts(max_pattern_len * NMERS10, 0)
            {}

            /// Add Exemplar sequence translated to overlapping 12-mers
            void AddExemplar(
                    TWords::const_iterator begin, 
                    TWords::const_iterator end);

            /// Return IUPAC string of most common sequence
            string InferConsensus(const SParams& params) const;

        private:
            TCount x_GetCount(size_t pos, TWord word) const
            {
                return m_counts[pos * NMERS10 + word];
            }

            void x_IncrCount(size_t pos, TWord word) 
            {
                m_counts[pos * NMERS10 + word] += 1;
            }

            /// Return most frequent word @ pos+1 following word@pos. (note: 10-mers here)
            TWord x_NextWord(size_t pos, TWord word) const;

            size_t m_len;
            vector<TCount> m_counts; //rectangular array of 10-mer word counts at given position.
                                     //size: length of pattern * count of 10-mers (4^10)
        };

    public:
        CPairedEndAdapterDetector(size_t max_len = 100)
          : m_cons5(max_len)
          , m_cons3(max_len)
        {}

        /// Add examplar paired-end spot
        /// seq = fwd_read + rev_read; |fwd_read|=|rev_read|=len/2
        /// Reads are in original orientation ->...<- (as in fastq-dump)
        virtual void AddExemplar(const char* seq, size_t len);

        /// The returned string contains '-'-delimited pair of
        /// IUPAC strings for 5' and 3' adapter respectively
        virtual string InferAdapterSeq() const
        {
            return m_cons5.InferConsensus(m_params) + "-"
                 + m_cons3.InferConsensus(m_params);
        }

    private:
        /// Given a paired-end read in consistent orientations with a fragment-overlap
        /// overlap, compute the putative positions of the adapter starts
        static pair<size_t, size_t> s_FindAdapterStartPos(
                const TWords& fwd_read, 
                const TWords& rev_read);

        CConsensusPattern m_cons5;
        CConsensusPattern m_cons3;
    };


    /// Find ungapped alignment of queries to a subject
    /// The subject sequence is presumed to be fairly short (~100 bases, few kb max)
    class CSimpleUngappedAligner
    {
    public:
        typedef Int2 TPos;

        CSimpleUngappedAligner()
        {}

        /// IUPAC sequence of target sequence(s)
        /// Multiple sequences may be concatenated and '-'-delimited
        /// e.g. if we want to match to seq or its rev-comp,
        /// the combined seq is seq+"-"+rev_comp(seq).
        ///
        void Init(const char* seq, size_t len);

        /// Represents single ungapped alignment
        struct SMatch
        {
            TPos first;  // query-pos
            TPos second; // subject-pos
            TPos len;
            
            SMatch() : first(NULLPOS), second(NULLPOS), len(0)
            {}
        };

        /// Return best ungapped alignment of at least MER_LENGTH
        SMatch FindSingleBest(const char* query, size_t len) const;

        const string& GetSeq() const
        {
            return m_seq;
        }

        typedef pair<TPos, TPos> TRange;
        TRange GetSeqRange(TPos pos) const;

    private:

        typedef vector<TPos> TPositions; 
        
        typedef pair<TPositions::const_iterator, 
                     TPositions::const_iterator> TPosRange;
        typedef map<TWord, TPosRange > TMapIndex;

        typedef vector<TRange> TRanges;

        string m_seq; // sequence of '-'-delimited subseqs
        TRanges m_subseq_ranges; // stores (begin, end) of each subseq

        // m_vec_index: word -> starting position in the target seq.
        // If uninitialized, m_vec_index will contain NULLPOS
        // If unique, value will be in vec_index;
        // If non-unique, m_vec_index will contain MULTIPOS;
        //   values will be in m_positions, and m_map_index
        //   will contain w -> (begin, end) into m_positions
        TPositions m_vec_index; // length: 2^bitsize(word)
        TPositions m_positions; // length: count of all non-unique words
        TMapIndex m_map_index;  // length: count of distinct non-unique words

        // Special values in vec-index.
        // MULTIPOS indicates that the multiple pos-values
        // for this word reside in the map-index.
        // Note: not using "static const TPos NULLPOS = -1; etc" 
        // due to compiler-specific issues
        enum {
            NULLPOS  = -1,
            MULTIPOS = -2
        };

   private:
        /// Create a match in diag/antidiag coordspace for a matched word.
        SMatch x_CreateMatch(TPos q_start, TPos s_start) const;

        /// Ungapped extension (match is in normal coordinates)
        void x_Extend(
                SMatch& match, 
                const char* query_seq, 
                const size_t query_len,
                const bool direction, //true iff forward
                const int match_score = 3,
                const int mismatch_score = -2,
                const int dropoff = 5) const;

        static bool s_Merge(SMatch& m, const SMatch& m2);

        /// Return better of the two matches
        /// (tradeoff between length and coverage)
        /// Positions are presumed to be are in diag/antidiag space
        const SMatch& x_GetBetterOf(const SMatch& a, const SMatch& b) const;

        //Permute all mismatches in a 24-bit 12-mer:
        // - Allow maximum of two mismatches.
        // - 3-prefix and suffix are required to match.
        static void s_PermuteMismatches(TWord w, TWords& words);

        /// Replace a letter in a 2-bit-coding word
        static TWord s_Put(TWord w, size_t pos, Uint1 letter)
        {
            return (w & ~(3 << (pos*2))) // zero-out target frame;
                  | (letter << (pos*2)); // put the letter in there
        }

        /// Helpers for indexing
        typedef pair<TWord, TPos> TWordAndPos;
        typedef set<TWordAndPos> TCoordSet; // Will use set as multimap
                                            // enforcing unique keyvals
        static void s_IndexWord(
                TWord w, 
                TPos pos, 
                TPositions& vec_index, 
                TCoordSet& coordset);

        static void s_CoordSetToMapIndex(
                const TCoordSet& coordset,
                TMapIndex& map_index,
                TPositions& positions);

    };
};

END_NCBI_SCOPE
#endif
