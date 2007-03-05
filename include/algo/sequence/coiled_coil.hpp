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
 * File Description:  Prediction of coiled coil regions of proteins
 *                    according to Lupas et al., 1991 (PMID 2031185)
 *
 */


#ifndef ALGO_SEQUENCE___COILED_COIL__HPP
#define ALGO_SEQUENCE___COILED_COIL__HPP

#include <corelib/ncbistd.hpp>
#include <objects/seqloc/Seq_loc.hpp>
#include <objmgr/seq_vector.hpp>
#include <vector>
#include <algorithm>

BEGIN_NCBI_SCOPE

class NCBI_XALGOSEQ_EXPORT CCoiledCoil
{
public:
    /// For each sequence position, compute the best
    /// coiled coil score, and the corresponding frame,
    /// using a window of length win_len (article uses 28).
    /// For char containers, seq must be in ncbistdaa.
    static void ComputeScores(const string& seq, vector<double>& scores,
                              vector<unsigned int>& frames,
                              TSeqPos win_len = 28);

    static void ComputeScores(const vector<char>& seq, vector<double>& scores,
                              vector<unsigned int>& frames,
                              TSeqPos win_len = 28);

    static void ComputeScores(const objects::CSeqVector& seq,
                              vector<double>& scores,
                              vector<unsigned int>& frames,
                              TSeqPos win_len = 28);

    /// convert a score to a probability of being a coiled coil
    static double ScoreToProb(double score);
    /// score to probability for a whole vector
    static void ScoreToProb(const vector<double>& scores,
                            vector<double>& probs);

    /// predict CC regions from sequence;
    /// return top score
    static double PredictRegions(const vector<char>& seq,
                                 vector<CRef<objects::CSeq_loc> >& regions,
                                 vector<double>& max_scores,
                                 TSeqPos win_len = 28);

    static double PredictRegions(const string& seq,
                                 vector<CRef<objects::CSeq_loc> >& regions,
                                 vector<double>& max_scores,
                                 TSeqPos win_len = 28);

    static double PredictRegions(const objects::CSeqVector& seq,
                                 vector<CRef<objects::CSeq_loc> >& regions,
                                 vector<double>& max_scores,
                                 TSeqPos win_len = 28);

private:
    static void x_PredictRegions(const vector<double>& scores,
                                 vector<CRef<objects::CSeq_loc> >& regions,
                                 vector<double>& max_scores);

    template<class Seq>
    friend void CCoil_ComputeScores(const Seq& seq, vector<double>& scores,
                                    vector<unsigned int>& frames,
                                    TSeqPos win_len);

    template<class Seq>
    friend double
    CCoil_PredictRegions(const Seq& seq,
                         vector<CRef<objects::CSeq_loc> >& regions,
                         vector<double>& max_scores, TSeqPos win_len);

    static const double sm_Propensities[26][7];
};


END_NCBI_SCOPE

#endif  // ALGO_SEQUENCE___COILED_COIL__HPP
