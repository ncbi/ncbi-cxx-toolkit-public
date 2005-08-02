/* $Id$
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
 * Author:  Jason Papadopoulos
 *
 * File Description:  CPSSMAligner implementation
 *                   
 * ===========================================================================
 *
 */


#include <ncbi_pch.hpp>
#include "messages.hpp"
#include <algo/align/nw/nw_pssm_aligner.hpp>
#include <algo/align/nw/align_exception.hpp>


BEGIN_NCBI_SCOPE

// only NCBIstdaa alphabet supported
static const int kPSSM_ColumnSize = 26;


CPSSMAligner::CPSSMAligner()
    : CNWAligner(),
      m_Pssm1(0), m_Freq1(0),
      m_Seq2(0), m_Freq2(0),
      m_FreqScale(1),
      m_StartWg(GetDefaultWg()),
      m_StartWs(GetDefaultWs()),
      m_EndWg(GetDefaultWg()),
      m_EndWs(GetDefaultWs())
{
}


CPSSMAligner::CPSSMAligner(const TScore** pssm1, size_t len1,
                           const char* seq2, size_t len2)
    : CNWAligner(),
      m_Pssm1(pssm1), m_Freq1(0),
      m_Seq2(seq2), m_Freq2(0),
      m_FreqScale(1),
      m_StartWg(GetDefaultWg()),
      m_StartWs(GetDefaultWs()),
      m_EndWg(GetDefaultWg()),
      m_EndWs(GetDefaultWs())
{
    SetSequences(pssm1, len1, seq2, len2);
}


CPSSMAligner::CPSSMAligner(const double** freq1, size_t len1,
                           const double** freq2, size_t len2,
                           const SNCBIPackedScoreMatrix *scoremat,
                           const int scale)
    : CNWAligner(),
      m_Pssm1(0), m_Freq1(freq1),
      m_Seq2(0), m_Freq2(freq2),
      m_FreqScale(scale),
      m_StartWg(GetDefaultWg()),
      m_StartWs(GetDefaultWs()),
      m_EndWg(GetDefaultWg()),
      m_EndWs(GetDefaultWs())
{
    SetScoreMatrix(scoremat);
    SetSequences(freq1, len1, freq2, len2, scale);
}

void CPSSMAligner::SetSequences(const char* seq1, size_t len1,
                                const char* seq2, size_t len2,
                                bool verify)
{
    m_Pssm1 = 0;
    m_Freq1 = 0;
    m_Seq2 = 0;
    m_Freq2 = 0;
    CNWAligner::SetSequences(seq1, len1, seq2, len2, verify);
}


void CPSSMAligner::SetSequences(const TScore** pssm1, size_t len1,
                                const char* seq2, size_t len2,
                                bool verify)
{
    if(!pssm1 || !len1 || !seq2 || !len2) {
        NCBI_THROW(CAlgoAlignException, eBadParameter,
                   g_msg_NullParameter);
    }

    if(verify) {
        for (size_t i = 0; i < len2; i++) {
            if (seq2[i] < 0 || seq2[i] >= kPSSM_ColumnSize) {
                NCBI_THROW(CAlgoAlignException, eInvalidCharacter,
                           g_msg_InvalidSequenceChars);
            }
        }
    }
    m_Pssm1 = pssm1;
    m_Freq1 = 0;
    m_SeqLen1 = len1;
    m_Seq2 = seq2;
    m_Freq2 = 0;
    m_SeqLen2 = len2;
    CNWAligner::m_Seq1 = 0;
    CNWAligner::m_Seq2 = 0;
}


void CPSSMAligner::SetSequences(const double** freq1, size_t len1,
                                const double** freq2, size_t len2,
                                const int scale)
{
    if(!freq1 || !len1 || !freq2 || !len2) {
        NCBI_THROW(CAlgoAlignException, eBadParameter,
                   g_msg_NullParameter);
    }
    m_Pssm1 = 0;
    m_Freq1 = freq1;
    m_SeqLen1 = len1;
    m_Seq2 = 0;
    m_Freq2 = freq2;
    m_SeqLen2 = len2;
    m_FreqScale = scale;
    CNWAligner::m_Seq1 = 0;
    CNWAligner::m_Seq2 = 0;
}


void CPSSMAligner::SetScoreMatrix(const SNCBIPackedScoreMatrix *scoremat)
{
    // upacking the score matrix will automatically arrange
    // its entries in the correct order to support NCBIstdaa

    if(!scoremat) {
        NCBI_THROW(CAlgoAlignException, eBadParameter,
                   g_msg_NullParameter);
    }
    CNWAligner::SetScoreMatrix(scoremat);
}


CPSSMAligner::TScore CPSSMAligner::Run()
{
    if(!x_CheckMemoryLimit()) {
        NCBI_THROW(CAlgoAlignException, eMemoryLimit,
                   g_msg_OutOfSpace);
    }

    m_score = CNWAligner::x_Run();

    return m_score;
}


CNWAligner::ETranscriptSymbol CPSSMAligner::x_GetDiagTS(size_t i1, size_t i2) 
const
{
    if (m_Freq1 || m_Pssm1) {
        return eTS_Match; // makes no differences for profile alignments
    }
    else {
        return CNWAligner::x_GetDiagTS(i1, i2);
    }
}


CNWAligner::TScore CPSSMAligner::x_Align(SAlignInOut* data)
{
    if (m_Freq1 || m_Pssm1)
        return x_AlignProfile(data);      // profile-sequence, profile-profile
    else
        return CNWAligner::x_Align(data); // sequence-sequence
}


// evaluate score for each possible alignment;
// fill out backtrace matrix
// bit coding (four bits per value): D E Ec Fc
// D:  1 if diagonal; 0 - otherwise
// E:  1 if space in 1st sequence; 0 if space in 2nd sequence
// Ec: 1 if gap in 1st sequence was extended; 0 if it is was opened
// Fc: 1 if gap in 2nd sequence was extended; 0 if it is was opened
//

const unsigned char kMaskFc  = 0x01;
const unsigned char kMaskEc  = 0x02;
const unsigned char kMaskE   = 0x04;
const unsigned char kMaskD   = 0x08;

CNWAligner::TScore CPSSMAligner::x_AlignProfile(SAlignInOut* data)
{
    const size_t N1 = data->m_len1 + 1;
    const size_t N2 = data->m_len2 + 1;

    vector<TScore> stl_rowV (N2), stl_rowF(N2);

    TScore* rowV    = &stl_rowV[0];
    TScore* rowF    = &stl_rowF[0];

    TScore* pV = rowV - 1;

    const double** freq1_row = m_Freq1 + data->m_offset1 - 1;
    const double** freq2_row = m_Freq2 + data->m_offset2 - 1;
    const TNCBIScore (*sm) [NCBI_FSM_DIM] = m_ScoreMatrix.s;

    const TScore** pssm_row = m_Pssm1 + data->m_offset1 - 1;
    const char* seq2 = m_Seq2 + data->m_offset2 - 1;

    m_terminate = false;

    if(m_prg_callback) {
        m_prg_info.m_iter_total = N1*N2;
        m_prg_info.m_iter_done = 0;
        if(m_terminate = m_prg_callback(&m_prg_info)) {
	  return 0;
	}
    }

    TScore wg1L = m_Wg;
    TScore wg1R = m_Wg;
    TScore wg2L = m_Wg;
    TScore wg2R = m_Wg;

    TScore ws1L = m_Ws;
    TScore ws1R = m_Ws;
    TScore ws2L = m_Ws;
    TScore ws2R = m_Ws;

    if (data->m_offset1 == 0) {
        if (data->m_esf_L1) {
            wg1L = ws1L = 0;
        }
        else {
            wg1L = m_StartWg;
            ws1L = m_StartWs;
        }
    }

    if (m_SeqLen1 == data->m_offset1 + data->m_len1) {
        if (data->m_esf_R1) {
            wg1R = ws1R = 0;
        }
        else {
            wg1R = m_EndWg;
            ws1R = m_EndWs;
        }
    }

    if (data->m_offset2 == 0) {
        if (data->m_esf_L2) {
            wg2L = ws2L = 0;
        }
        else {
            wg2L = m_StartWg;
            ws2L = m_StartWs;
        }
    }

    if (m_SeqLen2 == data->m_offset2 + data->m_len2) {
        if (data->m_esf_R2) {
            wg2R = ws2R = 0;
        }
        else {
            wg2R = m_EndWg;
            ws2R = m_EndWs;
        }
    }

    TScore wgleft1   = wg1L;
    TScore wsleft1   = ws1L;
    TScore wg1 = m_Wg, ws1 = m_Ws;

    // index calculation: [i,j] = i*n2 + j
    vector<unsigned char> stl_bm (N1*N2);
    unsigned char* backtrace_matrix = &stl_bm[0];

    // first row
    size_t k;
    rowV[0] = wgleft1;
    for (k = 1; k < N2; k++) {
        rowV[k] = pV[k] + wsleft1;
        rowF[k] = kInfMinus;
        backtrace_matrix[k] = kMaskE | kMaskEc;
    }
    rowV[0] = 0;
	
    if(m_prg_callback) {
        m_prg_info.m_iter_done = k;
        m_terminate = m_prg_callback(&m_prg_info);
    }

    // recurrences
    TScore wgleft2   = wg2L;
    TScore wsleft2   = ws2L;
    TScore V  = rowV[N2 - 1];
    TScore V0 = wgleft2;
    TScore E, G, n0;
    unsigned char tracer;

    size_t i, j;
    for(i = 1;  i < N1 && !m_terminate;  ++i) {
        
        V = V0 += wsleft2;
        E = kInfMinus;
        backtrace_matrix[k++] = kMaskFc;

        if(i == N1 - 1) {
            wg1 = wg1R;
            ws1 = ws1R;
        }

        TScore wg2 = m_Wg, ws2 = m_Ws;

        for (j = 1; j < N2; ++j, ++k) {

            if (m_Pssm1) {
                G = pV[j] + pssm_row[i][(unsigned char)seq2[j]];
            }
            else {
                double accum = 0.0, sum = 0.0;
                double diff_freq1[kPSSM_ColumnSize];
                double diff_freq2[kPSSM_ColumnSize];

                // separate the residue frequencies into two components:
                // a component that is the same for both columns, and
                // a component that is different. The all-against-all
                // score computation only takes place on the components
                // that are different, so this will assign a higher score
                // to more similar frequency columns

                for (int m = 0; m < kPSSM_ColumnSize; m++) {
                    if (freq1_row[i][m] < freq2_row[j][m]) {
                        accum += freq1_row[i][m] * (double)sm[m][m];
                        diff_freq1[m] = 0.0;
                        diff_freq2[m] = freq2_row[j][m] - freq1_row[i][m];
                    }
                    else {
                        accum += freq2_row[j][m] * (double)sm[m][m];
                        diff_freq1[m] = freq1_row[i][m] - freq2_row[j][m];
                        diff_freq2[m] = 0.0;
                    }
                }

                for (int m = 0; m < kPSSM_ColumnSize; m++) {
                    sum += diff_freq1[m];
                }
                if (sum > 0) {
                    // normalize one column of differences

                    for (int m = 0; m < kPSSM_ColumnSize; m++)
                        diff_freq1[m] /= sum;

                    for (int m = 0; m < kPSSM_ColumnSize; m++) {
                        for (int n = 0; n < kPSSM_ColumnSize; n++) {
                            accum += diff_freq1[m] * 
                                    diff_freq2[n] * 
                                    (double)sm[m][n];
                        }
                    }
                }

                G = pV[j] + (TScore)(m_FreqScale * accum + 0.5);
            }

            pV[j] = V;

            n0 = V + wg1;
            if(E >= n0) {
                E += ws1;      // continue the gap
                tracer = kMaskEc;
            }
            else {
                E = n0 + ws1;  // open a new gap
                tracer = 0;
            }

            if(j == N2 - 1) {
                wg2 = wg2R;
                ws2 = ws2R;
            }
            n0 = rowV[j] + wg2;
            if(rowF[j] >= n0) {
                rowF[j] += ws2;
                tracer |= kMaskFc;
            }
            else {
                rowF[j] = n0 + ws2;
            }

            if (E >= rowF[j]) {
                if(E >= G) {
                    V = E;
                    tracer |= kMaskE;
                }
                else {
                    V = G;
                    tracer |= kMaskD;
                }
            } else {
                if(rowF[j] >= G) {
                    V = rowF[j];
                }
                else {
                    V = G;
                    tracer |= kMaskD;
                }
            }
            backtrace_matrix[k] = tracer;
        }

        pV[j] = V;

        if(m_prg_callback) {
            m_prg_info.m_iter_done = k;
            if(m_terminate = m_prg_callback(&m_prg_info)) {
                break;
            }
        }
       
    }

    if(!m_terminate) {
        x_DoBackTrace(backtrace_matrix, data);
    }
    return V;
}


// The present implementation works with full transcripts only,
// e.g. aligner.ScoreFromTranscript(aligner.GetTranscript(false));
//
CNWAligner::TScore CPSSMAligner::ScoreFromTranscript(
                       const TTranscript& transcript,
                       size_t start1, size_t start2) const
{
    if (m_Freq1 == 0 && m_Pssm1 == 0) {
        return CNWAligner::ScoreFromTranscript(transcript, start1, start2);
    }

    TScore score = 0;

    int state1 = 0;   // 0 = normal, 1 = gap
    int state2 = 0;   // 0 = normal, 1 = gap

    const TNCBIScore (*sm) [NCBI_FSM_DIM] = m_ScoreMatrix.s;
    size_t offset1 = start1;
    size_t offset2 = start2;

    const size_t dim = transcript.size();
    for(size_t i = 0; i < dim; ++i) {

        ETranscriptSymbol ts = transcript[i];
        switch(ts) {

        case eTS_Replace:
        case eTS_Match: {
            state1 = state2 = 0;
            if (m_Pssm1) {
                score += m_Pssm1[offset1][(unsigned char)m_Seq2[offset2]];
            }
            else {
                double accum = 0.0, sum = 0.0;
                double diff_freq1[kPSSM_ColumnSize];
                double diff_freq2[kPSSM_ColumnSize];

                for (int m = 0; m < kPSSM_ColumnSize; m++) {
                    if (m_Freq1[offset1][m] < m_Freq2[offset2][m]) {
                        accum += m_Freq1[offset1][m] * (double)sm[m][m];
                        diff_freq1[m] = 0.0;
                        diff_freq2[m] = m_Freq2[offset2][m] - 
                                        m_Freq1[offset1][m];
                    }
                    else {
                        accum += m_Freq2[offset2][m] * (double)sm[m][m];
                        diff_freq1[m] = m_Freq1[offset1][m] - 
                                        m_Freq2[offset2][m];
                        diff_freq2[m] = 0.0;
                    }
                }

                for (int m = 0; m < kPSSM_ColumnSize; m++) {
                    sum += diff_freq1[m];
                }
                if (sum > 0) {
                    for (int m = 0; m < kPSSM_ColumnSize; m++)
                        diff_freq1[m] /= sum;

                    for (int m = 0; m < kPSSM_ColumnSize; m++) {
                        for (int n = 0; n < kPSSM_ColumnSize; n++) {
                            accum += m_Freq1[offset1][m] * 
                                     m_Freq2[offset2][n] * 
                                    (double)sm[m][n];
                        }
                    }
                }

                score += (TScore)(m_FreqScale * accum + 0.5);
            }
            ++offset1; ++offset2;
        }
        break;

        case eTS_Insert: {
            if(state1 != 1) score += m_Wg;
            state1 = 1; state2 = 0;
            score += m_Ws;
            ++offset2;
        }
        break;

        case eTS_Delete: {
            if(state2 != 1) score += m_Wg;
            state1 = 0; state2 = 1;
            score += m_Ws;
            ++offset1;
        }
        break;
        
        default: {
        NCBI_THROW(CAlgoAlignException, eInternal,
                   g_msg_InvalidTranscriptSymbol);
        }
        }
    }

    size_t g = 0;
    for(size_t i = 0; i < dim; ++i) {
        if(transcript[i] == eTS_Insert) ++g; else break;
    }
    if(g > 0 && start1 == 0) {
        if (m_esf_L1 == true)
            score -= (m_Wg + g*m_Ws);
        else
            score -= ((m_Wg-m_StartWg) + g*(m_Ws-m_StartWs));
    }

    g = 0;
    for(size_t i = 0; i < dim; ++i) {
        if(transcript[i] == eTS_Delete) ++g; else break;
    }
    if(g > 0 && start2 == 0) {
        if(m_esf_L2 == true)
            score -= (m_Wg + g*m_Ws);
        else
            score -= ((m_Wg-m_StartWg) + g*(m_Ws-m_StartWs));
    }

    g = 0;
    for(int i = dim - 1; i >= 0; --i) {
        if(transcript[i] == eTS_Insert) ++g; else break;
    }
    if(g > 0) {
        if(m_esf_R1 == true)
            score -= (m_Wg + g*m_Ws);
        else
            score -= ((m_Wg-m_EndWg) + g*(m_Ws-m_EndWs));
    }

    g = 0;
    for(int i = dim - 1; i >= 0; --i) {
        if(transcript[i] == eTS_Delete) ++g; else break;
    }
    if(g > 0) {
        if(m_esf_R2 == true)
            score -= (m_Wg + g*m_Ws);
        else
            score -= ((m_Wg-m_EndWg) + g*(m_Ws-m_EndWs));
    }

    return score;
}

END_NCBI_SCOPE


/*
 * ===========================================================================
 * $Log$
 * Revision 1.6  2005/08/02 17:36:07  papadopo
 * 1. Make the aligner use different penalties for starting and
 *    ending gaps
 * 2. Switch to a different (greedy) function for assigning a score for
 *    aligning columns of residue frequencies
 *
 * Revision 1.5  2005/04/04 16:34:13  kapustin
 * Specify precise type of diags in raw alignment transcripts where feasible
 *
 * Revision 1.4  2004/12/27 19:42:41  papadopo
 * revert to two-sequences version of ScoreFromTranscript if aligning two sequences
 *
 * Revision 1.3  2004/12/16 22:42:22  kapustin
 * Move to algo/align/nw
 *
 * Revision 1.2  2004/11/29 14:37:15  kapustin
 * CNWAligner::GetTranscript now returns TTranscript and direction can be specified. x_ScoreByTanscript renamed to ScoreFromTranscript with two additional parameters to specify starting coordinates.
 *
 * Revision 1.1  2004/10/05 19:23:03  papadopo
 * Semantics identical to CNWAligner but allowing for one or both input
 * sequences to be represented as profiles
 *
 * ===========================================================================
 */
