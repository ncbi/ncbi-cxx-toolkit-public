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
 * File Description:  Predict antigenic determinants as described by
 *                    Kolaskar and Tongaonkar, 1990
 *
 */


#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>
#include <objects/seqloc/Seq_interval.hpp>
#include <algo/sequence/antigenic.hpp>

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);


// Antigenic propensities by NCBIstdaa:
const double CAntigenic::sm_Pa_table[26] =
    {
        0,
        1.06383,  // A
        0.77624,  // B (min of D and N)
        1.41226,  // C
        0.86646,  // D
        0.85083,  // E
        1.09132,  // F
        0.87413,  // G
        1.10505,  // H
        1.15173,  // I
        0.93026,  // K
        1.25039,  // L
        0.82567,  // M
        0.77624,  // N
        1.06383,  // P
        1.01542,  // Q
        0.87254,  // R
        1.01219,  // S
        0.90884,  // T
        1.38428,  // V
        0.89290,  // W
        0.77624,  // X (min of standard 20)
        1.16148,  // Y
        0.85083,  // Z (min of E and Q)
        0,        // U (not known)
        0         // * (what's appropriate?)
    };


template<class Seq>
void x_PredictAGSites(const Seq& seq, CAntigenic::TLocVec& results,
                      int min_len)
{


    // First build vector giving local average of Pa (over 7 residues).
    // Along the way, calculate the average for the whole protein.
    
    vector<double> Pa(seq.size());
    double local_sum = 0, global_sum = 0;

    for (int i = 0;  i < 7;  i++) {
        local_sum += CAntigenic::sm_Pa_table[seq[i]];
        global_sum += CAntigenic::sm_Pa_table[seq[i]];
    }
    Pa[3] = local_sum / 7;
    
    for (unsigned int i = 4;  i < seq.size() - 3;  i++) {
        local_sum -= CAntigenic::sm_Pa_table[seq[i-4]];
        local_sum += CAntigenic::sm_Pa_table[seq[i+3]];
        global_sum += CAntigenic::sm_Pa_table[seq[i+3]];
        Pa[i] = local_sum / 7;
    }

    double global_mean = global_sum / seq.size();
    double thresh = min(global_mean, 1.0);

    // now look for runs of Pa >= thresh of length >= min_len

    int count = 0;
    int begin;

    // NOTE: we go one extra residue, in the knowledge that
    // its Pa entry will be zero, so it will end any run
    for (unsigned int i = 3;  i < seq.size() - 2;  i++) {
        if (Pa[i] >= thresh) {
            if (count == 0) {
                begin = i;  // the beginning of a run
            }
            count++;
        } else {
            // the end of a (possibly empty) run
            if (count >= min_len) {
                // an antigenic site
                int end = i - 1;
                
                CRef<objects::CSeq_loc> loc(new objects::CSeq_loc());
                loc->SetInt().SetFrom(begin);
                loc->SetInt().SetTo(end);
                results.push_back(loc);
            }
            count = 0;
        }
    }
}


void CAntigenic::PredictSites(const string& seq,
                              TLocVec& results,
                              unsigned int min_len)
{
    x_PredictAGSites(seq, results, min_len);
}


void CAntigenic::PredictSites(const vector<char>& seq,
                              TLocVec& results,
                              unsigned int min_len)
{
    x_PredictAGSites(seq, results, min_len);
}


void CAntigenic::PredictSites(const objects::CSeqVector& seq,
                              TLocVec& results,
                              unsigned int min_len)
{
    string seq_ncbistdaa;
    CSeqVector vec(seq);
    vec.SetNcbiCoding();
    vec.GetSeqData(0, vec.size(), seq_ncbistdaa);
    x_PredictAGSites(seq_ncbistdaa, results, min_len);
}


END_NCBI_SCOPE


/*
 * ===========================================================================
 * $Log$
 * Revision 1.4  2004/05/21 21:41:04  gorelenk
 * Added PCH ncbi_pch.hpp
 *
 * Revision 1.3  2003/09/10 16:20:40  ucko
 * Tweak to avoid undefined symbols with WorkShop.
 *
 * Revision 1.2  2003/09/09 16:09:38  dicuccio
 * Moved lookup table to implementation file
 *
 * Revision 1.1  2003/09/02 14:53:11  jcherry
 * Initial version
 *
 * ===========================================================================
 */
