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


#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>
#include <objects/seqloc/Seq_interval.hpp>
#include <algo/sequence/coiled_coil.hpp>
#include "math.h"

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);


const double CCoiledCoil::sm_Propensities[26][7] = 
    {
        {0,     0,     0,     0,     0,     0,     0    },
        {1.297, 1.551, 1.084, 2.612, 0.377, 1.248, 0.877}, // A
        {0.030, 1.475, 1.534, 0.039, 0.663, 1.620, 1.448}, // B (min of D, N)
        {0.824, 0.022, 0.308, 0.152, 0.180, 0.156, 0.044}, // C
        {0.030, 2.352, 2.268, 0.237, 0.663, 1.620, 1.448}, // D
        {0.262, 3.496, 3.108, 0.998, 5.685, 2.494, 3.048}, // E
        {0.531, 0.076, 0.403, 0.662, 0.189, 0.106, 0.013}, // F
        {0.045, 0.275, 0.578, 0.216, 0.211, 0.426, 0.156}, // G
        {0.347, 0.275, 0.679, 0.395, 0.294, 0.579, 0.213}, // H
        {2.597, 0.098, 0.345, 0.894, 0.514, 0.471, 0.431}, // I
        {1.375, 2.639, 1.763, 0.191, 1.815, 1.961, 2.795}, // K
        {3.167, 0.297, 0.398, 3.902, 0.585, 0.501, 0.483}, // L
        {2.240, 0.370, 0.480, 1.409, 0.541, 0.772, 0.663}, // M
        {0.835, 1.475, 1.534, 0.039, 1.722, 2.456, 2.280}, // N
        {0,     0.008, 0,     0.013, 0,     0,     0    }, // P
        {0.179, 2.114, 1.778, 0.631, 2.550, 1.578, 2.526}, // Q
        {0.659, 1.163, 1.210, 0.031, 1.358, 1.937, 1.798}, // R
        {0.382, 0.583, 1.052, 0.419, 0.525, 0.916, 0.628}, // S
        {0.169, 0.702, 0.955, 0.654, 0.791, 0.843, 0.647}, // T
        {1.665, 0.403, 0.386, 0.949, 0.211, 0.342, 0.360}, // V
        {0.240, 0,     0,     0.456, 0.019, 0,     0    }, // W
        {0,     0.008, 0,     0.013, 0,     0,     0    }, // X (min of 20)
        {1.417, 0.090, 0.122, 1.659, 0.190, 0.130, 0.155}, // Y
        {0.179, 2.114, 1.778, 0.631, 2.550, 1.578, 2.526}, // Z (min of E, Q)
        {0,     0,     0,     0,     0,     0,     0    }, // U (not known)
        {0,     0,     0,     0,     0,     0,     0    }  // *
    };


template<class Seq>
void CCoil_ComputeScores(const Seq& seq, vector<double>& scores,
                         vector<unsigned int>& frames, TSeqPos win_len)
{

    vector<vector<double> > prelim_scores(7);
    for (unsigned int frame = 0;  frame < 7;  frame++) {
        prelim_scores[frame].resize(seq.size());
    }

    // calculate 'preliminary' scores (one per frame per window position;
    // score is recorded in position corresponing to the start of the window)
    for (unsigned int frame = 0;  frame < 7;  frame++) {
        for (TSeqPos start = 0;  start < seq.size() - win_len + 1;  start++) {
            double prod;
            if (start > 0  &&
                CCoiledCoil::sm_Propensities[seq[start - 1]]
                    [(start - 1 + frame) % 7] != 0) {
                // compute product using last result, avoiding repeating mults
                prod /= CCoiledCoil::sm_Propensities[seq[start - 1]]
                    [(start - 1 + frame) % 7];
                prod *= CCoiledCoil::sm_Propensities[seq[start + win_len - 1]]
                    [(start + win_len - 1 + frame) % 7];
            } else {
                // compute product from scratch
                prod = 1;
                for (TSeqPos i = start;  i < start + win_len;  i++) {
                    prod *= CCoiledCoil::sm_Propensities[seq[i]]
                        [(i + frame) % 7];
                }
            }
            prelim_scores[frame][start] = pow(prod, 1.0 / win_len);
        }
    }

    // now find max among all frames at each window position
    vector<double> w_score(seq.size());
    vector<unsigned int> w_frame(seq.size());
    for (TSeqPos start = 0;  start < seq.size() - win_len + 1;  start++) {
        double max_score = 0;
        unsigned int max_frame = 0;
        for (unsigned int frame = 0;  frame < 7;  frame++) {
            if (prelim_scores[frame][start] > max_score) {
                max_score = prelim_scores[frame][start];
                max_frame = frame;
            }
        }
        w_score[start] = max_score;
        w_frame[start] = max_frame;
    }
    

    // now find, for each position, the max of the scores for the
    // win_len or fewer windows to which it belongs
    scores.resize(seq.size());
    frames.resize(seq.size());
    for (TSeqPos pos = 0;  pos < seq.size();  pos++) {
        // if possible, compute the max the easy way, without iterating
        if (pos > 0 && (pos < win_len
                        || w_score[pos - win_len] != scores[pos - 1])) {
            if (scores[pos - 1] > w_score[pos]) {
                scores[pos] = scores[pos - 1];
                frames[pos] = frames[pos - 1];
            } else {
                scores[pos] = w_score[pos];
                frames[pos] = w_frame[pos];
            }
        } else {
            // compute the max from scratch

            double max_score = 0;
            unsigned int max_frame = 0;
        
            for (TSeqPos win = pos < win_len ? 0 : (pos - win_len + 1);
                 win <= pos;
                 win++) {
                if (w_score[win] > max_score) {
                    max_score = w_score[win];
                    max_frame = w_frame[win];
                }
            }
            scores[pos] = max_score;
            frames[pos] = max_frame;
        }
    }
}


void CCoiledCoil::ComputeScores(const string& seq, vector<double>& scores,
                                vector<unsigned int>& frames,
                                TSeqPos win_len)
{
    CCoil_ComputeScores(seq, scores, frames, win_len);
}


void CCoiledCoil::ComputeScores(const vector<char>& seq, vector<double>& scores,
                                vector<unsigned int>& frames,
                                TSeqPos win_len)
{
    CCoil_ComputeScores(seq, scores, frames, win_len);
}


void CCoiledCoil::ComputeScores(const CSeqVector& seq,
                                vector<double>& scores,
                                vector<unsigned int>& frames,
                                TSeqPos win_len)
{
    string seq_ncbistdaa;
    CSeqVector vec(seq);
    vec.SetNcbiCoding();
    vec.GetSeqData(0, vec.size(), seq_ncbistdaa);
    CCoil_ComputeScores(seq_ncbistdaa, scores, frames, win_len);
}


double CCoiledCoil::ScoreToProb(double score)
{
    // neglect factor of 1/sqrt(2*Pi); cancels in ratio
    // (note that it's given as 1/(2*Pi) in the paper)
    double gcc = 1 / 0.24 * exp(-0.5 * pow((score - 1.63) / 0.24, 2));
    double gg  = 1 / 0.20 * exp(-0.5 * pow((score - 0.77) / 0.20, 2));

    return gcc / (30 * gg + gcc);
}


void CCoiledCoil::ScoreToProb(const vector<double>& scores,
                              vector<double>& probs)
{
    probs.resize(scores.size());
    for (unsigned int i = 0;  i < scores.size();  i++) {
        probs[i] = ScoreToProb(scores[i]);
    }
}


// find runs where ScoreToProb(score) >= 0.5
void CCoiledCoil::x_PredictRegions(const vector<double>& scores,
                                   vector<CRef<CSeq_loc> >& regions,
                                   vector<double>& max_scores)
{
    bool in_a_run = 0;
    TSeqPos begin, end;
    double max_score;  // max score for the run
    for (unsigned int i = 0;  i < scores.size();  i++) {
        if (CCoiledCoil::ScoreToProb(scores[i]) >= 0.5) {
            if (!in_a_run) {
                in_a_run = true;
                begin = i;
                max_score = scores[i];
            } else {
                if (scores[i] > max_score) {
                    max_score = scores[i];
                }
            }
        } else {
            if (in_a_run) {  // the end of a run
                in_a_run = false;
                end = i - 1;
                CRef<CSeq_loc> region(new CSeq_loc());
                region->SetInt().SetFrom(begin);
                region->SetInt().SetTo  (end);
                regions.push_back(region);
                max_scores.push_back(max_score);
            }
        }
    }
    // deal with case where run went to end of sequence
    if (in_a_run) {
        end = scores.size() - 1;
        CRef<CSeq_loc> region(new CSeq_loc());
        region->SetInt().SetFrom(begin);
        region->SetInt().SetTo  (end);
        regions.push_back(region);
        max_scores.push_back(max_score);
    }
}


template<class Seq>
double CCoil_PredictRegions(const Seq& seq,
                            vector<CRef<objects::CSeq_loc> >& regions,
                            vector<double>& max_scores,
                            TSeqPos win_len)
{
    vector<double> scores;
    vector<unsigned int> frames;
    CCoiledCoil::ComputeScores(seq, scores, frames, win_len);
    CCoiledCoil::x_PredictRegions(scores, regions, max_scores);
    return *max_element(scores.begin(), scores.end());
}


double CCoiledCoil::PredictRegions(const string& seq,
                                   vector<CRef<objects::CSeq_loc> >& regions,
                                   vector<double>& max_scores,
                                   TSeqPos win_len)
{
    return CCoil_PredictRegions(seq, regions, max_scores, win_len);
}


double CCoiledCoil::PredictRegions(const vector<char>& seq,
                                   vector<CRef<objects::CSeq_loc> >& regions,
                                   vector<double>& max_scores,
                                   TSeqPos win_len)
{
    return CCoil_PredictRegions(seq, regions, max_scores, win_len);
}


double CCoiledCoil::PredictRegions(const objects::CSeqVector& seq,
                                   vector<CRef<objects::CSeq_loc> >& regions,
                                   vector<double>& max_scores,
                                   TSeqPos win_len)
{
    return CCoil_PredictRegions(seq, regions, max_scores, win_len);
}



END_NCBI_SCOPE


/*
 * ===========================================================================
 * $Log$
 * Revision 1.4  2004/05/21 21:41:04  gorelenk
 * Added PCH ncbi_pch.hpp
 *
 * Revision 1.3  2003/09/09 18:30:55  ucko
 * Fixes for WorkShop, which (still) doesn't let templates access
 * anything file-static.
 *
 * Revision 1.2  2003/09/09 16:10:20  dicuccio
 * Fxes for MSVC.  Moved templated functions into implementation file to avoid
 * naming / export conflicts.  Moved lookup table to implementation file.
 *
 * Revision 1.1  2003/09/08 16:15:12  jcherry
 * Initial version
 *
 * ===========================================================================
 */
