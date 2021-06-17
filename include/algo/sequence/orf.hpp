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
 * Authors:  Josh Cherry
 *
 * File Description:  code for finding and representing ORFs
 *
 */


#include <corelib/ncbistd.hpp>
#include <objects/seqloc/Seq_loc.hpp>
#include <objmgr/seq_vector.hpp>
#include <objects/seqloc/Seq_id.hpp>
#include <math.h>


BEGIN_NCBI_SCOPE


//
// class COrf implements a simple open reading frame search.
//
// This class reports, as a series of seq-locs, all the possible open
// reading frames in a given sequence.  The sequence can be represented
// as either 
/// This class provides functions for finding all the ORFs
/// of a specified minimum length in a DNA sequence.

class NCBI_XALGOSEQ_EXPORT COrf
{
public:
    typedef vector< CRef<objects::CSeq_loc> > TLocVec;

    static const size_t k_default_max_seq_gap = 30;

    /// Find ORFs in both orientations. Circularity is ignored.
    /// Report results as Seq-locs (without seq-id set). Partial ORFs are trimmed to frame.
    /// seq must be in iupac.
    
    /// allowable_starts may contain "STOP" for stop-to-stop ORFs
    /// If
    /// - allowable_starts empty (the default) or just "STOP", longest_orfs = any - finds stop-to-stop ORFs, only ORFs near edges are marked partial
    /// - allowable_starts not empty, no "STOP", longest_orfs = false - find all proper or partial (if no start) ORFs
    /// - allowable_starts not empty, no "STOP", longest_orfs = true - find longest proper or partial (if no start) ORFs
    /// - allowable_starts not empty, plus "STOP", longest_orfs = false - find all proper or partial ORFs and stop-to-stop ORFs 
    /// - allowable_starts not empty, plus "STOP", longest_orfs = true - find partial ORFs or stop-to-stop ORFs, only those without proper start are marked partial
    
    /// Do not allow more than max_seq_gap consecutive N-or-gap bases in an ORF,
    /// longer gaps split the sequence, resulting in no spanning ORFs and possibly partial ORFs before and after the gap

    /// ORFs below min_length_bp long are not reported
    
    static void FindOrfs(const string& seq,
                         TLocVec& results,
                         unsigned int min_length_bp = 3,
                         int genetic_code = 1,
                         const vector<string>& allowable_starts = vector<string>(),
                         bool longest_orfs = true,
                         size_t max_seq_gap = k_default_max_seq_gap);

    static void FindOrfs(const vector<char>& seq,
                         TLocVec& results,
                         unsigned int min_length_bp = 3,
                         int genetic_code = 1,
                         const vector<string>& allowable_starts = vector<string>(),
                         bool longest_orfs = true,
                         size_t max_seq_gap = k_default_max_seq_gap);

    static void FindOrfs(const objects::CSeqVector& seq,
                         TLocVec& results,
                         unsigned int min_length_bp = 3,
                         int genetic_code = 1,
                         const vector<string>& allowable_starts = vector<string>(),
                         bool longest_orfs = true,
                         size_t max_seq_gap = k_default_max_seq_gap);

    /// Create vector of allowable_starts by genetic-code.
    /// satisfying CTrans_table::IsATGStart() and/or CTrans_table::IsAltStart()
    static vector<string> GetStartCodons(int genetic_code, bool include_atg, bool include_alt);

    /// Specifically find ORFS with a strong Kozak signal that are upstream of
    /// cds_start. Separately report uORFS overlapping cds start and uORFs of
    /// sufficiantly length that don't overlap cds start
    static void FindStrongKozakUOrfs(
                         const objects::CSeqVector& seq,
                         TSeqPos cds_start,
                         TLocVec& overlap_results,
                         TLocVec& non_overlap_results,
                         unsigned int min_length_bp = 3,
                         unsigned int non_overlap_min_length_bp = 105,
                         int genetic_code = 1,
                         size_t max_seq_gap = k_default_max_seq_gap);

    /**
    /// This version returns an annot full of CDS features.
    /// Optionally takes a CSeq_id (by CRef) for use in
    /// the feature table; otherwise ids are left unset.
    template<class Seq>
    static CRef<objects::CSeq_annot>
    FindOrfs(const Seq& seq,
             unsigned int min_length_bp = 3,
             int genetic_code = 1,
             CRef<objects::CSeq_id> id = CRef<objects::CSeq_id>())
    {
        // place to store orfs
        TLocVec orfs;
        FindOrfs(seq, orfs, min_length_bp, genetic_code);
        return MakeCDSAnnot(orfs, genetic_code, id);
    }
    **/

    /// Build an annot full of CDS features from CSeq_loc's.
    /// Optionally takes a CSeq_id (by CRef) for use in
    /// the feature table; otherwise ids are left unset.
    static CRef<objects::CSeq_annot>
    MakeCDSAnnot(const TLocVec& orfs, int genetic_code = 1,
                 objects::CSeq_id* id = NULL);



    /// Calculate identity to Kozak PWM for vertebrates
    template<typename TSeqType> //must support operator[], yielding iupacna residue, 
                                //e.g. CSeqVector, char*, or string
    static double GetKozakIdentity(const TSeqType& iupacna_seq, size_t seq_len, size_t start_codon_pos)
    {
        //log-odds position-weight matrix for +/-6 positions around the putative start-codon
        //The start-codon itself is "masked-out" with uniform distribution (will contribute 0 to log-odds),
        //PWM is precomputed based set of refseq mRNAs, and will differ slightly depending on which set is used
        //Note that the PWM for non-vertebrates is entirely different.
        //This one based on 'srcdb_refseq_known[prop] AND biomol_mRNA[prop] AND "vertebrates"[porgn:__txid7742]'
        //
        //Each value is log(p1)-log(1-p1)-log(p2)+log(1-p2)
        //where p1 is probability of finding this nucleotide at this position in TIS context and 
        //p2 is probability of finding this nucleotide at this position in non-TIS context := 0.25

#if 0
        //based on ccds subset
        static const double pwm[15][4] = {
            {-0.22, -0.09, 0.45, -0.34},
            {-0.31, 0.26, 0.18, -0.27},
            {-0.02, 0.43, 0.00, -0.75},
            {0.64, -0.98, 0.39, -1.38},
            {0.14, 0.43, -0.22, -0.66},
            {-0.33, 0.61, 0.12, -1.19},
             { 0.00,  0.00,  0.00,  0.00}, //
             { 0.00,  0.00,  0.00,  0.00}, // start codon
             { 0.00,  0.00,  0.00,  0.00}, //
            {-0.12, -0.56, 0.69, -0.60},
            {0.10, 0.47, -0.34, -0.55},
            {-0.50, 0.05, 0.40, -0.15},
            {-0.05, 0.00, 0.32, -0.40},
            {0.05, 0.23, -0.16, -0.18},
            {-0.37, 0.25, 0.29, -0.36}
        };

#else
        static const double pwm[15][4] = {  
            // A      C      G      T
             {-0.21, -0.23,  0.64, -0.36},
             {-0.37,  0.33,  0.18, -0.22},
             { 0.08,  0.60, -0.09, -0.87},
             { 1.16, -1.35,  0.48, -1.76},
             { 0.33,  0.51, -0.36, -0.72},
             {-0.27,  0.86,  0.12, -1.30},
             { 0.00,  0.00,  0.00,  0.00}, //
             { 0.00,  0.00,  0.00,  0.00}, // start codon
             { 0.00,  0.00,  0.00,  0.00}, //
             {-0.16, -0.74,  1.08, -0.66},
             { 0.09,  0.76, -0.46, -0.70},
             {-0.59, -0.03,  0.56, -0.11},
             {-0.04, -0.08,  0.48, -0.47},
             { 0.12,  0.30, -0.26, -0.21},
             {-0.44,  0.31,  0.35, -0.37}
        };   
#endif
        static const char* alphabet = "ACGT";
        double log_odds(0);
        for(size_t i = 0; i < 15; i++) {
            TSignedSeqPos seq_pos = start_codon_pos - 6 + i; 
            char nuc = (seq_pos >= 0 && seq_pos < static_cast<long>(seq_len)) 
                     ? iupacna_seq[seq_pos] : 'N'; 
            size_t nuc_ord = find(alphabet, alphabet + 4, nuc) - alphabet;
            if(nuc_ord >= 4) { 
                continue;
            }    
            log_odds += pwm[i][nuc_ord];
        }    
        double odds = exp(log_odds);
        return odds / (1.0 + odds); 
    }
};

END_NCBI_SCOPE
