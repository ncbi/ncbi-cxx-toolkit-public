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
* Authors:  Yuri Kapustin, Alexander Souvorov
*
* File Description:  CSplicedAligner16
*                   
* ===========================================================================
*
*/

#include <ncbi_pch.hpp>
#include "messages.hpp"
#include <algo/align/nw/nw_spliced_aligner16.hpp>
#include <algo/align/nw/align_exception.hpp>


BEGIN_NCBI_SCOPE


namespace {

    const unsigned char g_nwspl_donor[splice_type_count_16][2] = {
        {'G','T'}, {'G','C'}, {'A','T'}, {'?','?'}
    };

    /// 16-bit donor table
    const unsigned short g_nwspl_donor_16[splice_type_count_16] = {
        ('G' << 8) | 'T',   // GT
        ('G' << 8) | 'C',   // GC
        ('A' << 8) | 'T',   // AT
        ('?' << 8) | '?'    // ??
    };

/*
    const unsigned char g_nwspl_acceptor[splice_type_count_16][2] = {
        {'A','G'}, {'A','G'}, {'A','C'}, {'?','?'}
    };
*/
    const unsigned short g_nwspl_acceptor_16[splice_type_count_16] = {
        ('A' << 8) | 'G',  // AG
        ('A' << 8) | 'G',  // AG
        ('A' << 8) | 'C',  // AC
        ('?' << 8) | '?'   // ??
    };

    const unsigned char g_topidx (splice_type_count_16 - 1);
}


CSplicedAligner16::CSplicedAligner16()
{
    for(unsigned char st (0); st < splice_type_count_16; ++st) {
        m_Wi[st] = GetDefaultWi(st);
    }
}


CSplicedAligner16::CSplicedAligner16(const char* seq1, size_t len1,
                                     const char* seq2, size_t len2)
    : CSplicedAligner(seq1, len1, seq2, len2)
{
    for(unsigned char st (0); st < splice_type_count_16; ++st) {
        m_Wi[st] = GetDefaultWi(st);
    }
}


CSplicedAligner16::CSplicedAligner16(const string& seq1, const string& seq2)
    : CSplicedAligner(seq1, seq2)
{
    for(unsigned char st (0); st < splice_type_count_16; ++st) {
        m_Wi[st] = GetDefaultWi(st);
    }
}


CNWAligner::TScore CSplicedAligner16::GetDefaultWi(unsigned char splice_type)
{

   switch(splice_type) {
        case 0: return -8;  // GT/AG
        case 1: return -12; // GC/AG
        case 2: return -14; // AT/AC
        case 3: return -18; // ??/??
        default: {
            NCBI_THROW(CAlgoAlignException,
                       eInvalidSpliceTypeIndex,
                       g_msg_InvalidSpliceTypeIndex);
        }
    }
}

namespace {

    const Uint2 kMaskFc   (0x0001);
    const Uint2 kMaskEc   (0x0002);
    const Uint2 kMaskE    (0x0004);
    const Uint2 kMaskD    (0x0008);

    const Uint2 kMaskDnr0 (0x0010);
    const Uint2 kMaskDnr1 (0x0020);
    const Uint2 kMaskDnr2 (0x0040);
    const Uint2 kMaskDnr3 (0x0080);

    const Uint2 kMaskAcc0 (0x0100);
    const Uint2 kMaskAcc1 (0x0200);
    const Uint2 kMaskAcc2 (0x0400);
    const Uint2 kMaskAcc3 (0x0800);

    const Uint2 kMask_ZeroJump (0x1000);
}

// Evaluate dynamic programming matrix. Create transcript.
CNWAligner::TScore CSplicedAligner16::x_Align (SAlignInOut* data)
{
    if(m_terminate) {
        return 0;
    }

    /*
    // use the banded version if there is no space for introns
    const int len_dif (data->m_len2 - data->m_len1);
    if(data->m_len2 > 5 && data->m_len1 > 5 && len_dif < int (m_IntronMinSize) / 2)
    {
        const Uint1 where  (len_dif < 0? 0: 1);
        const size_t shift (abs(len_dif) / 2);
        const size_t band  (abs(len_dif) 
                            + 2 * ( max(data->m_len1, data->m_len2) / 20 + 1));
        SetShift(where, shift);
        SetBand(band);
        return CBandAligner::x_Align(data);
    }
    */

    const TScore cds_penalty_extra (-1);
    const size_t ibs (12);

    TScore V (0);

    const size_t N1 (data->m_len1 + 1);
    const size_t N2 (data->m_len2 + 1);

    if(m_prg_callback) {
        m_prg_info.m_iter_total = N1*N2;
        m_prg_info.m_iter_done = 0;
        if( (m_terminate = m_prg_callback(&m_prg_info)) ) {
	  return 0;
	}
    }


    vector<TScore> stl_rowV (N2), stl_rowF (N2);
    TScore * rowV (&stl_rowV.front());
    TScore * rowF (&stl_rowF.front());

    // index calculation: [i,j] = i*n2 + j
    SAllocator<Uint2> alloc_bm (N1*N2);
    Uint2* NCBI_RESTRICT backtrace_matrix (alloc_bm.GetPointer());

    const char * NCBI_RESTRICT seq1 (m_Seq1 + data->m_offset1 - 1);
    const char * NCBI_RESTRICT seq2 (m_Seq2 + data->m_offset2 - 1);

#ifdef NW_ALGO_ALIGN_SPLICED16_USE_SPLICED_SCORE_MATRIX
    const TScore (*sm) [NCBI_FSM_DIM] (
        reinterpret_cast<const TScore (*)[NCBI_FSM_DIM]> 
        (&m_ScoreMatrix.front()));
#endif

    const bool bFreeGapLeft1  (data->m_esf_L1 && data->m_offset1 == 0);
    const bool bFreeGapRight1 (data->m_esf_R1 &&
                               m_SeqLen1 == data->m_offset1 + data->m_len1);
    
    const bool bFreeGapLeft2  (data->m_esf_L2 && data->m_offset2 == 0);
    const bool bFreeGapRight2 (data->m_esf_R2 &&
                               m_SeqLen2 == data->m_offset2 + data->m_len2);

    const bool sw_left (bFreeGapLeft1 && bFreeGapLeft2);
    const bool sw_right (bFreeGapRight1 && bFreeGapRight2);

    TScore wgleft1 (bFreeGapLeft1? 0: m_Wg);
    TScore wsleft1 (bFreeGapLeft1? 0: m_Ws);
    TScore wg1 (wgleft1), ws1 (wsleft1);

    // recurrences
    TScore wgleft2 (bFreeGapLeft2? 0: m_Wg);
    TScore wsleft2 (bFreeGapLeft2? 0: m_Ws);
    TScore V_max, vAcc, global_max(numeric_limits<TScore>::min());
    TScore V0 (0);
    TScore E, G, n0;
    Uint2 tracer;

    size_t i_global_max (N1 - 1), j_global_max (N2 - 1);

    // store candidate donors
    size_t* jAllDonors_d[splice_type_count_16];
    TScore* vAllDonors_d[splice_type_count_16];
	
    size_t  jTail_d[splice_type_count_16];
    size_t  jHead_d[splice_type_count_16];
    TScore  vBestDonor_d[splice_type_count_16];
    size_t  jBestDonor_d[splice_type_count_16];
	
    // declare restrict aliases for arrays
    size_t** NCBI_RESTRICT jAllDonors = jAllDonors_d;
    TScore** NCBI_RESTRICT vAllDonors = vAllDonors_d;
    
    size_t* NCBI_RESTRICT jTail (jTail_d);
    size_t* NCBI_RESTRICT jHead (jHead_d);
    TScore* NCBI_RESTRICT vBestDonor (vBestDonor_d);
    size_t* NCBI_RESTRICT jBestDonor (jBestDonor_d);
	
    vector<size_t> stl_jAllDonors (splice_type_count_16 * N2);
    vector<TScore> stl_vAllDonors (splice_type_count_16 * N2);
    for(unsigned char st (0); st < splice_type_count_16; ++st) {
        jAllDonors[st] = &stl_jAllDonors[st*N2];
        vAllDonors[st] = &stl_vAllDonors[st*N2];
    }

    enum {
        eDnr0 = 1,  // GT
        eAcc0 = 2,  // AG
        eDnr1 = 4,  // GC
        eAcc1 = 8,  // AG
        eDnr2 = 16, // AT
        eAcc2 = 32, // AC
    };

    vector<Uint1> stl_splices (N2);

    // set up fake row and splices
    size_t k;
    for (k = 0; k < N2; ++k) {

        rowV[k] = rowF[k] = kInfMinus;

        Uint1 c (0);
        if(k >= 2) {
            const Uint2 v1 ((seq2[k-1] << 8) | seq2[k]);
            if(v1 == g_nwspl_acceptor_16[0]) c |= eAcc0;
            if(v1 == g_nwspl_acceptor_16[1]) c |= eAcc1;
            if(v1 == g_nwspl_acceptor_16[2]) c |= eAcc2;
        }

        if(k + 2 < N2) {
            const Uint2 v1 ((seq2[k+1] << 8) | seq2[k+2]);
            if(v1 == g_nwspl_donor_16[0]) c |= eDnr0;
            if(v1 == g_nwspl_donor_16[1]) c |= eDnr1;
            if(v1 == g_nwspl_donor_16[2]) c |= eDnr2;
        }

        stl_splices[k] = c;
    }
    k = 0;

    TScore * pV (rowV - 1);

    const Uint1 * NCBI_RESTRICT splices (& stl_splices.front());

    size_t cds_start (m_cds_start), cds_stop (m_cds_stop);
    if(cds_start < cds_stop) {
        cds_start -= data->m_offset1;
        cds_stop -= data->m_offset1;
    }

    for(size_t i (0), j(0); i < N1;  ++i, j = 0) {
       
        V = i > 0? (V0 += wsleft2) : 0;
        E = kInfMinus;
        backtrace_matrix[k++] = kMaskFc;
        Uint1 ci (i > 0? seq1[i]: 'N');
        
        if(ci == 'N') ci = 'z';

        jTail[0] = jTail[1] = jTail[2] = jTail[3] 
            = jHead[0] = jHead[1] = jHead[2] = jHead[3] = 0;
        vBestDonor[0] = vBestDonor[1] = vBestDonor[2] = vBestDonor[3] = kInfMinus;
        jBestDonor[0] = jBestDonor[1] = jBestDonor[2] = jBestDonor[3] = 0;

        TScore wg2 (m_Wg), ws2 (m_Ws);

        // detect donor candidate
        if (N2 > 2) {

#define NWSPL_DETECTDONOR(st_idx) \
            if(splices[j] & eDnr##st_idx ) \
            {  \
                 const size_t tl (jTail[st_idx]++); \
                 jAllDonors[st_idx][tl] = j; \
                 vAllDonors[st_idx][tl] = V; \
            } \

NWSPL_DETECTDONOR(0)
NWSPL_DETECTDONOR(1)
NWSPL_DETECTDONOR(2)

#undef DETECT_DONOR
        }

        // the first max value
        const size_t tl (jTail[g_topidx]++);
        jAllDonors[g_topidx][tl] = j;
        vAllDonors[g_topidx][tl] = V_max = V;

        if(cds_start <= i && i < cds_stop) {

            if(i != 0 || ! bFreeGapLeft1) {
                ws1 += cds_penalty_extra;
            }
            if(j != 0 || ! bFreeGapLeft2) {
                ws2 += cds_penalty_extra;
            }
        }

        if(i == N1 - 1 && bFreeGapRight1) {
            wg1 = ws1 = 0;
        }

        for (j = 1; j < N2; ++j, ++k) {
            			

#ifdef NW_ALGO_ALIGN_SPLICED16_USE_SPLICED_SCORE_MATRIX
            G = pV[j] + sm[ci][(unsigned char)seq2[j]];
#else
            G = pV[j] + (ci == seq2[j]? m_Wm: m_Wms);
#endif
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

            if(j == N2 - 1 && bFreeGapRight2) {
                wg2 = ws2 = 0;
            }

            n0 = rowV[j] + wg2;
            if(rowF[j] >= n0) {
                rowF[j] += ws2;
                tracer |= kMaskFc;
            }
            else {
                rowF[j] = n0 + ws2;
            }

            // evaluate the score (V)
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
            
/* TODO:
 *
 * As a performance improvement, exploit the fact that the scoring
 * is consistent among gaps and introns. That is, since the score of a splice
 * effectively caps the gap length, there is no need to check explicitly 
 * if an intron has the min length.
 *
 */

            // check for any donors just ripened (loop unroll)
#define NW_NDON_EVAL(st_idx) \
            { \
                const size_t jt (jTail[st_idx]); \
                const size_t jh (jHead[st_idx]); \
                if (jt > jh && j - jAllDonors[st_idx][jh] >= m_IntronMinSize) { \
                    const TScore x ((jAllDonors[st_idx][jh] - \
                                      jBestDonor[st_idx]) >> ibs); \
                    if (vAllDonors[st_idx][jh] + x > vBestDonor[st_idx]) {  \
                        vBestDonor[st_idx] = vAllDonors[st_idx][jh];  \
                        jBestDonor[st_idx] = jAllDonors[st_idx][jh]; \
                    } \
                    ++jHead[st_idx]; \
                } \
            } 

NW_NDON_EVAL(0)
NW_NDON_EVAL(1)
NW_NDON_EVAL(2)
NW_NDON_EVAL(3)

#undef NW_NDON_EVAL

            Uint2 acceptor (0);

            // check splice signal (loop unroll)
#define NW_SIG_EVAL(st_idx) \
            if(splices[j] & eAcc##st_idx) { \
                const size_t ilen (j - jBestDonor[st_idx]); \
                vAcc = vBestDonor[st_idx] + m_Wi[st_idx] - (ilen >> ibs); \
                if (vAcc > V) { \
                    V = vAcc; \
                    acceptor = kMaskAcc##st_idx;        \
                    backtrace_matrix[k - ilen] |= kMaskDnr##st_idx; \
                } \
            }
                    
NW_SIG_EVAL(0)
NW_SIG_EVAL(1)
NW_SIG_EVAL(2)

#undef NW_SIG_EVAL

            vAcc = vBestDonor[g_topidx] + m_Wi[g_topidx];
            if(vAcc > V) {
                V = vAcc;
                acceptor = kMaskAcc3;
                const size_t ilen (j - jBestDonor[g_topidx]);
                backtrace_matrix[k - ilen] |= kMaskDnr3;
            }

            if(sw_left && V < 0) {
                tracer |= kMask_ZeroJump;
                V = 0;
            }

            if(sw_right && V > global_max) {
                global_max = V;
                i_global_max = i;
                j_global_max = j;
            }

            if (acceptor) {
                tracer |= acceptor;
            }

            // detect donor candidate
#define NW_DON_EVAL(st_idx) \
            if(splices[j] & eDnr##st_idx ) { \
                const size_t ilen (j - jBestDonor[st_idx]); \
                const TScore x (ilen >> ibs); \
                if(V + x > vBestDonor[st_idx]) {  \
                    const size_t tl (jTail[st_idx]++); \
                    jAllDonors[st_idx][tl] = j; \
                    vAllDonors[st_idx][tl] = V; \
                } \
            }

NW_DON_EVAL(0)
NW_DON_EVAL(1)
NW_DON_EVAL(2)

#undef NW_DON_EVAL
            
            // detect new best value
            if(V > V_max) {
                const size_t tl (jTail[g_topidx]++);
                jAllDonors[g_topidx][tl] = j;
                vAllDonors[g_topidx][tl] = V_max = V;
            }

            backtrace_matrix[k] = tracer;
        } // for j

        pV[j] = V;

        if (i == 0) {
            V0 = wgleft2;
            wg1 = m_Wg;
            ws1 = m_Ws;
        }

        if(m_prg_callback) {
            m_prg_info.m_iter_done = k;
            if( (m_terminate = m_prg_callback(&m_prg_info)) ) {
                break;
            }
        }
    }

    try {
        if(!m_terminate) {
            x_DoBackTrace(backtrace_matrix, data, i_global_max, j_global_max);
        }
    }
    catch(exception&) { // GCC hack
        throw;
    }

    return CNWAligner::TScore(V);
}


void CSplicedAligner16::x_DoBackTrace (
    const Uint2* backtrace_matrix, CNWAligner::SAlignInOut* data,
    int i_global_max, int j_global_max)
{
    const size_t N1 (data->m_len1 + 1);
    const size_t N2 (data->m_len2 + 1);

    data->m_transcript.clear();
    data->m_transcript.reserve(N1 + N2);

    size_t k  (i_global_max * N2 + j_global_max);
    size_t i1 (data->m_offset1 + i_global_max - 1);
    size_t i2 (data->m_offset2 + j_global_max - 1);

    const size_t dim_slack_ins (N2 - 1 - j_global_max);
    data->m_transcript.insert(data->m_transcript.end(),
                              dim_slack_ins,
                              eTS_SlackInsert);

    const size_t dim_slack_del (N1 - 1 - i_global_max);
    data->m_transcript.insert(data->m_transcript.end(),
                              dim_slack_del,
                              eTS_SlackDelete);
    while (k != 0) {

        Uint2 Key (backtrace_matrix[k]);
        if(Key & kMask_ZeroJump) {

            const size_t dim_ins (i2 - data->m_offset2 + 1);
            data->m_transcript.insert(data->m_transcript.end(),
                                      dim_ins,
                                      eTS_SlackInsert);

            const size_t dim_del (i1 - data->m_offset1 + 1);
            data->m_transcript.insert(data->m_transcript.end(),
                                      dim_del,
                                      eTS_SlackDelete);
            break;
        }

        if(Key & 0x0F00) {

            size_t intron_length (1);
            const Uint2 donor ((Key & 0x0F00) >> 4);
            while(intron_length < m_IntronMinSize || (Key & donor) == 0) {
                Key = backtrace_matrix[--i2, --k];
                ++intron_length;
                data->m_transcript.push_back(eTS_Intron);
            }
            continue;
        }

        if(k == 0) continue;
        
        if (Key & kMaskD) {
            data->m_transcript.push_back(x_GetDiagTS(i1--, i2--));
            k -= N2 + 1;
        }
        else if (Key & kMaskE) {

            data->m_transcript.push_back(eTS_Insert);
            --k;
            --i2;
            while(k > 0 && (Key & kMaskEc)) {
                data->m_transcript.push_back(eTS_Insert);
                Key = backtrace_matrix[k--];
                --i2;
            }
        }
        else {

            data->m_transcript.push_back(eTS_Delete);
            k -= N2;
            --i1;

            while(k > 0 && (Key & kMaskFc)) {
                data->m_transcript.push_back(eTS_Delete);
                Key = backtrace_matrix[k];
                k -= N2;
                --i1;
            }
        }
    }
}


CNWAligner::TScore CSplicedAligner16::ScoreFromTranscript(
                       const TTranscript& transcript,
                       size_t start1, size_t start2) const 
{
    bool nucl_mode;
    if(start1 == kMax_UInt && start2 == kMax_UInt) {
        nucl_mode = true;
    }
    else if(start1 != kMax_UInt && start2 != kMax_UInt) {
        nucl_mode = false;
    }
    else {
        NCBI_THROW(CAlgoAlignException, eInternal, g_msg_InconsistentArguments);
    }

    const size_t dim (transcript.size());

    TScore score (0);

    const char* p1 (m_Seq1 + start1);
    const char* p2 (m_Seq2 + start2);

    const TNCBIScore (*sm) [NCBI_FSM_DIM] = m_ScoreMatrix.s;

    // skip any slack
    size_t i (0);
    for(; i < dim 
          && (transcript[i] == eTS_SlackDelete 
              || transcript[i] == eTS_SlackInsert); ++i);

    if(i == dim) {
        return score;
    }

    char state1;   // 0 = normal, 1 = gap, 2 = intron
    char state2;   // 0 = normal, 1 = gap

    switch(transcript[i]) {

    case eTS_Match:
    case eTS_Replace:
        state1 = state2 = 0;
        break;

    case eTS_Insert:
        state1 = 1; state2 = 0; score += m_Wg;
        break;

    case eTS_Intron:
        state1 = 0; state2 = 0;
        break; // intron flag set later

    case eTS_Delete:
        state1 = 0; state2 = 1; score += m_Wg;
        break;

    default:
        NCBI_THROW(CAlgoAlignException, eInternal, g_msg_InvalidTranscriptSymbol);
    }

    for(; i < dim; ++i) {

        ETranscriptSymbol ts (transcript[i]);
        switch(ts) {

        case eTS_SlackDelete:
        case eTS_SlackInsert:
            i = dim;
            break;

        case eTS_Match: 
        case eTS_Replace: {
            if(nucl_mode) {
                score += (ts == eTS_Match)? m_Wm: m_Wms;
            }
            else {
                unsigned char c1 = *p1;
                unsigned char c2 = *p2;
                score += sm[c1][c2];
                ++p1; ++p2;
            }
            state1 = state2 = 0;
        }
        break;

        case eTS_Insert: {
            if(state1 != 1) score += m_Wg;
            state1 = 1; state2 = 0;
            score += m_Ws;
            ++p2;
        }
        break;

        case eTS_Delete: {
            if(state2 != 1) score += m_Wg;
            state1 = 0; state2 = 1;
            score += m_Ws;
            ++p1;
        }
        break;

        case eTS_Intron: {

            if(state1 != 2) {
                if(nucl_mode) {
                    score += m_Wi[0]; // all introns assumed consensus
                }
                else {
                    for(unsigned char i = 0; i < splice_type_count_16; ++i) {

                        if( (*p2 == g_nwspl_donor[i][0] &&
                             *(p2 + 1) == g_nwspl_donor[i][1]) || i == g_topidx) {
                            
                            score += m_Wi[i];
                            break;
                        }
                    }
                }
            }
            state1 = 2; state2 = 0;
            ++p2;
        }
        break;

        default:
            NCBI_THROW(CAlgoAlignException, eInternal, g_msg_InvalidTranscriptSymbol);
        }
    }

    if(m_esf_R1) {
        size_t g = 0;
        for(int i = dim - 1; i >= 0; --i) {
            if(transcript[i] == eTS_Insert) ++g; else break;
        }
        if(g > 0) {
            score -= (m_Wg + g*m_Ws);
        }
    }

    if(m_esf_R2) {
        size_t g = 0;
        for(int i = dim - 1; i >= 0; --i) {
            if(transcript[i] == eTS_Delete) ++g; else break;
        }
        if(g > 0) {
            score -= (m_Wg + g*m_Ws);
        }
    }

    if(m_esf_L1) {
        size_t g = 0;
        for(size_t i = 0; i < dim; ++i) {
            if(transcript[i] == eTS_Insert) ++g; else break;
        }
        if(g > 0) {
            score -= (m_Wg + g*m_Ws);
        }
    }

    if(m_esf_L2) {
        size_t g = 0;
        for(size_t i = 0; i < dim; ++i) {
            if(transcript[i] == eTS_Delete) ++g; else break;
        }
        if(g > 0) {
            score -= (m_Wg + g*m_Ws);
        }
    }

    return score;
}




END_NCBI_SCOPE
