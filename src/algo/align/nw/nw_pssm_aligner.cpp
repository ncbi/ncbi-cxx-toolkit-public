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
#include <math.h>
#include "messages.hpp"
#include <algo/align/nw/nw_pssm_aligner.hpp>
#include <algo/align/nw/align_exception.hpp>


BEGIN_NCBI_SCOPE

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

    // no penalty for aligning gaps with each other
    m_ScoreMatrix.s[0][0] = 0;

    for (int i = 0; i < kPSSM_ColumnSize; i++) {
        for (int j = 0; j < kPSSM_ColumnSize; j++) {
            m_DScoreMatrix[i][j] = (double)m_ScoreMatrix.s[i][j];
        }
    }
}


CPSSMAligner::TScore CPSSMAligner::Run()
{
    if(!x_CheckMemoryLimit()) {
        NCBI_THROW(CAlgoAlignException, eMemoryLimit, g_msg_HitSpaceLimit);
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
    if (m_Freq1)
        return x_AlignProfile(data);      // profile-profile
    else if (m_Pssm1)
        return x_AlignPSSM(data);      // PSSM-sequence
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

CNWAligner::TScore CPSSMAligner::x_AlignPSSM(SAlignInOut* data)
{
    const size_t N1 = data->m_len1 + 1;
    const size_t N2 = data->m_len2 + 1;

    vector<TScore> stl_rowV (N2), stl_rowF(N2);

    TScore* rowV    = &stl_rowV[0];
    TScore* rowF    = &stl_rowF[0];

    TScore* pV = rowV - 1;

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
    CBacktraceMatrix4 backtrace_matrix (N1 * N2);
    backtrace_matrix.SetAt(0, 0);

    // first row
    size_t k;
    rowV[0] = wgleft1;
    for (k = 1; k < N2; k++) {
        rowV[k] = pV[k] + wsleft1;
        rowF[k] = kInfMinus;
        backtrace_matrix.SetAt(k, kMaskE | kMaskEc);
    }
    backtrace_matrix.Purge(k);
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
        backtrace_matrix.SetAt(k++, kMaskFc);

        if(i == N1 - 1) {
            wg1 = wg1R;
            ws1 = ws1R;
        }

        TScore wg2 = m_Wg, ws2 = m_Ws;

        for (j = 1; j < N2; ++j, ++k) {

            G = pV[j] + pssm_row[i][(unsigned char)seq2[j]];

            pV[j] = V;

            n0 = V + wg1;
            if(E >= n0) {
                E += ws1;
                tracer = kMaskEc;
            }
            else {
                E = n0 + ws1;
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
            backtrace_matrix.SetAt(k, tracer);
        }

        pV[j] = V;

        if(m_prg_callback) {
            m_prg_info.m_iter_done = k;
            if(m_terminate = m_prg_callback(&m_prg_info)) {
                break;
            }
        }  
    }
    backtrace_matrix.Purge(k);

    if(!m_terminate) {
        x_DoBackTrace(backtrace_matrix, data);
    }
    return V;
}


CNWAligner::TScore CPSSMAligner::x_AlignProfile(SAlignInOut* data)
{
    const size_t N1 = data->m_len1 + 1;
    const size_t N2 = data->m_len2 + 1;

    vector<double> stl_rowV (N2), stl_rowF(N2);

    double* rowV    = &stl_rowV[0];
    double* rowF    = &stl_rowF[0];

    double* pV = rowV - 1;

    const double** freq1_row = m_Freq1 + data->m_offset1 - 1;
    const double** freq2_row = m_Freq2 + data->m_offset2 - 1;

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
    CBacktraceMatrix4 backtrace_matrix (N1 * N2);

    // first row
    size_t k = 1;
    if (N2 > 1) {
        rowV[0] = wgleft1 * (1.0 - freq2_row[1][0]);
        for (k = 1; k < N2; k++) {
            rowV[k] = pV[k] + wsleft1;
            rowF[k] = kInfMinus;
            backtrace_matrix.SetAt(k, kMaskE | kMaskEc);
        }
        backtrace_matrix.Purge(k);
    }
    rowV[0] = 0;
	
    if(m_prg_callback) {
        m_prg_info.m_iter_done = k;
        m_terminate = m_prg_callback(&m_prg_info);
    }

    // recurrences
    TScore wgleft2   = wg2L;
    TScore wsleft2   = ws2L;
    double V  = rowV[N2 - 1];
    double V0 = 0;
    double E, G, n0;
    unsigned char tracer;

    if (N1 > 1)
        V0 = wgleft2 * (1.0 - freq1_row[1][0]);

    size_t i, j;
    for(i = 1;  i < N1 && !m_terminate;  ++i) {
        
        V = V0 += wsleft2;
        E = kInfMinus;
        backtrace_matrix.SetAt(k++, kMaskFc);

        if(i == N1 - 1) {
            wg1 = wg1R;
            ws1 = ws1R;
        }

        TScore wg2 = m_Wg, ws2 = m_Ws;

        for (j = 1; j < N2; ++j, ++k) {

            if(j == N2 - 1) {
                wg2 = wg2R;
                ws2 = ws2R;
            }
            const double *profile1 = freq1_row[i];
            const double *profile2 = freq2_row[j];
            const double scaled_wg1 = wg1 * (1.0 - profile2[0]);
            const double scaled_ws1 = ws1;
            const double scaled_wg2 = wg2 * (1.0 - profile1[0]);
            const double scaled_ws2 = ws2;
            
            double accum = 0.0, sum = 0.0;
            int num_zeros1 = 0, num_zeros2 = 0;
            double diff_freq1[kPSSM_ColumnSize];
            double diff_freq2[kPSSM_ColumnSize];

            // separate the residue frequencies into two components:
            // a component that is the same for both columns, and
            // a component that is different. The all-against-all
            // score computation only takes place on the components
            // that are different, so this will assign a higher score
            // to more similar frequency columns
            //
            // Begin by separating out the common portion of each
            // profile

            for (int m = 1; m < kPSSM_ColumnSize; m++) {
                if (profile1[m] < profile2[m]) {
                    accum += profile1[m] * m_DScoreMatrix[m][m];
                    diff_freq1[m] = 0.0;
                    diff_freq2[m] = profile2[m] - profile1[m];
                    num_zeros1++;
                }
                else {
                    accum += profile2[m] * m_DScoreMatrix[m][m];
                    diff_freq1[m] = profile1[m] - profile2[m];
                    diff_freq2[m] = 0.0;
                    num_zeros2++;
                }
            }

            // normalize difference for profile with smaller gap
            if (profile1[0] <= profile2[0]) {
                for (int m = 1; m < kPSSM_ColumnSize; m++)
                    sum += diff_freq1[m];
            } else {
                for (int m = 1; m < kPSSM_ColumnSize; m++)
                    sum += diff_freq2[m];
            }

            if (sum > 0) {
                sum = 1.0 / sum;
                if (profile1[0] <= profile2[0]) {
                    for (int m = 1; m < kPSSM_ColumnSize; m++)
                        diff_freq1[m] *= sum;
                } else {
                    for (int m = 1; m < kPSSM_ColumnSize; m++)
                        diff_freq2[m] *= sum;
                }

                // Add in the cross terms (not counting gaps).
                // Note that the following assumes a symmetric
                // score matrix

                if (num_zeros1 > num_zeros2) {
                    for (int m = 1; m < kPSSM_ColumnSize; m++) {
                        if (diff_freq1[m] > 0) {
                            sum = 0.0;
                            double *matrix_row = m_DScoreMatrix[m];
                            for (int n = 1; n < kPSSM_ColumnSize; n++) {
                                sum += diff_freq2[n] * matrix_row[n];
                            }
                            accum += diff_freq1[m] * sum;
                        }
                    }
                } else {
                    for (int m = 1; m < kPSSM_ColumnSize; m++) {
                        if (diff_freq2[m] > 0) {
                            sum = 0.0;
                            double *matrix_row = m_DScoreMatrix[m];
                            for (int n = 1; n < kPSSM_ColumnSize; n++) {
                                sum += diff_freq1[n] * matrix_row[n];
                            }
                            accum += diff_freq2[m] * sum;
                        }
                    }
                }
            }

            G = pV[j] + accum * m_FreqScale +
                            profile1[0] * m_Ws * (1-profile2[0]) +
                            profile2[0] * m_Ws * (1-profile1[0]);

            pV[j] = V;

            n0 = V + scaled_wg1;
            if(E >= n0) {
                E += scaled_ws1;      // continue the gap
                tracer = kMaskEc;
            }
            else {
                E = n0 + scaled_ws1;  // open a new gap
                tracer = 0;
            }

            n0 = rowV[j] + scaled_wg2;
            if(rowF[j] >= n0) {
                rowF[j] += scaled_ws2;
                tracer |= kMaskFc;
            }
            else {
                rowF[j] = n0 + scaled_ws2;
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
            backtrace_matrix.SetAt(k, tracer);
        }

        pV[j] = V;

        if(m_prg_callback) {
            m_prg_info.m_iter_done = k;
            if(m_terminate = m_prg_callback(&m_prg_info)) {
                break;
            }
        }
    }
    backtrace_matrix.Purge(k);

    if(!m_terminate) {
        x_DoBackTrace(backtrace_matrix, data);
    }
    return (TScore)(V + 0.5);
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
    int offset1 = -1;
    int offset2 = -1;

    const size_t dim = transcript.size();

    if (m_Pssm1) {                         // PSSM-sequence score
        for(size_t i = 0; i < dim; ++i) {

            TScore wg = 0, ws = 0;

            if (offset1 < 0) {
                if (!m_esf_L1) {
                    wg = m_StartWg; ws = m_StartWs;
                }
            } else if (offset2 < 0) {
                if (!m_esf_L2) {
                    wg = m_StartWg; ws = m_StartWs;
                }
            } else if (offset1 == (int)m_SeqLen1 - 1) {
                if (!m_esf_R1) {
                    wg = m_EndWg; ws = m_EndWs;
                }
            } else if (offset2 == (int)m_SeqLen2 - 1) {
                if (!m_esf_R2) {
                    wg = m_EndWg; ws = m_EndWs;
                }
            } else {
                wg = m_Wg; ws = m_Ws;
            }

            ETranscriptSymbol ts = transcript[i];
            switch(ts) {
    
            case eTS_Replace:
            case eTS_Match: {
                ++offset1; ++offset2;
                state1 = state2 = 0;
                score += m_Pssm1[offset1][(unsigned char)m_Seq2[offset2]];
            }
            break;
    
            case eTS_Insert: {
                ++offset2;
                if(state1 != 1) score += wg;
                state1 = 1; state2 = 0;
                score += ws;
            }
            break;
    
            case eTS_Delete: {
                ++offset1;
                if(state2 != 1) score += wg;
                state1 = 0; state2 = 1;
                score += ws;
            }
            break;
            
            default: {
            NCBI_THROW(CAlgoAlignException, eInternal,
                       g_msg_InvalidTranscriptSymbol);
            }
            }
        }
    } else {                                 // profile-profile score
        double dscore = 0.0;

        for(size_t i = 0; i < dim; ++i) {
    
            TScore wg1 = 0, ws1 = 0;
            TScore wg2 = 0, ws2 = 0;

            if (offset1 < 0) {
                if (!m_esf_L1) {
                    wg1 = m_StartWg; ws1 = m_StartWs;
                }
            } else if (offset1 == (int)m_SeqLen1 - 1) {
                if (!m_esf_R1) {
                    wg1 = m_EndWg; ws1 = m_EndWs;
                }
            } else {
                wg1 = m_Wg; ws1 = m_Ws;
            }

            if (offset2 < 0) {
                if (!m_esf_L2) {
                    wg2 = m_StartWg; ws2 = m_StartWs;
                }
            } else if (offset2 == (int)m_SeqLen2 - 1) {
                if (!m_esf_R2) {
                    wg2 = m_EndWg; ws2 = m_EndWs;
                }
            } else {
                wg2 = m_Wg; ws2 = m_Ws;
            }

            ETranscriptSymbol ts = transcript[i];
            switch(ts) {
    
            case eTS_Replace:
            case eTS_Match: {
                state1 = state2 = 0;
                ++offset1; ++offset2;
                double accum = 0.0, sum = 0.0;
                double diff_freq1[kPSSM_ColumnSize];
                double diff_freq2[kPSSM_ColumnSize];

                for (int m = 1; m < kPSSM_ColumnSize; m++) {
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

                if (m_Freq1[offset1][0] <=  m_Freq2[offset2][0]) {
                    for (int m = 1; m < kPSSM_ColumnSize; m++)
                        sum += diff_freq1[m];
                } else {
                    for (int m = 1; m < kPSSM_ColumnSize; m++)
                        sum += diff_freq2[m];
                }

                if (sum > 0) {
                    if (m_Freq1[offset1][0] <=  m_Freq2[offset2][0]) {
                        for (int m = 1; m < kPSSM_ColumnSize; m++)
                            diff_freq1[m] /= sum;
                    } else {
                        for (int m = 1; m < kPSSM_ColumnSize; m++)
                            diff_freq2[m] /= sum;
                    }

                    for (int m = 1; m < kPSSM_ColumnSize; m++) {
                        for (int n = 1; n < kPSSM_ColumnSize; n++) {
                            accum += diff_freq1[m] * 
                                     diff_freq2[n] * 
                                    (double)sm[m][n];
                        }
                    }
                }
                dscore += accum * m_FreqScale +
                        m_Freq1[offset1][0] * m_Ws * (1-m_Freq2[offset2][0]) +
                        m_Freq2[offset2][0] * m_Ws * (1-m_Freq1[offset1][0]);
            }
            break;
    
            case eTS_Insert: {
                ++offset2;
                if(state1 != 1) dscore += wg1 * (1.0 - m_Freq2[offset2][0]);
                state1 = 1; state2 = 0;
                dscore += ws1;
            }
            break;
    
            case eTS_Delete: {
                ++offset1;
                if(state2 != 1) dscore += wg2 * (1.0 - m_Freq1[offset1][0]);
                state1 = 0; state2 = 1;
                dscore += ws2;
            }
            break;
            
            default: {
            NCBI_THROW(CAlgoAlignException, eInternal,
                       g_msg_InvalidTranscriptSymbol);
            }
            }
        }
        score = (TScore)(dscore + 0.5);
    }

    return score;
}

END_NCBI_SCOPE
