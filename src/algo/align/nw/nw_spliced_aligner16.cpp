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

const unsigned char g_nwspl_acceptor[splice_type_count_16][2] = {
    {'A','G'}, {'A','G'}, {'A','C'}, {'?','?'}
};

const unsigned short g_nwspl_acceptor_16[splice_type_count_16] = {
    ('A' << 8) | 'G',  // AG
    ('A' << 8) | 'G',  // AG
    ('A' << 8) | 'C',  // AC
    ('?' << 8) | '?'   // ??
};


const unsigned char g_topidx = splice_type_count_16 - 1;


CSplicedAligner16::CSplicedAligner16()
{
    for(unsigned char st = 0; st < splice_type_count_16; ++st) {
        m_Wi[st] = GetDefaultWi(st);
    }
}


CSplicedAligner16::CSplicedAligner16(const char* seq1, size_t len1,
                                     const char* seq2, size_t len2)
    : CSplicedAligner(seq1, len1, seq2, len2)
{
    for(unsigned char st = 0; st < splice_type_count_16; ++st) {
        m_Wi[st] = GetDefaultWi(st);
    }
}


CSplicedAligner16::CSplicedAligner16(const string& seq1, const string& seq2)
    : CSplicedAligner(seq1, seq2)
{
    for(unsigned char st = 0; st < splice_type_count_16; ++st) {
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


// Bit coding (eleven bits per value) for backtrace.
// --------------------------------------------------
// [24-5] intron length, if non-zero
// [4]    Fc:      1 if gap in 2nd sequence was extended; 0 if it is was opened
// [3]    Ec:      1 if gap in 1st sequence was extended; 0 if it is was opened
// [2]    E:       1 if space in 1st sequence; 0 if space in 2nd sequence
// [1]    D:       1 if diagonal; 0 - otherwise

const unsigned char kMaskFc       = 0x01;
const unsigned char kMaskEc       = 0x02;
const unsigned char kMaskE        = 0x04;
const unsigned char kMaskD        = 0x08;

// Evaluate dynamic programming matrix. Create transcript.
CNWAligner::TScore CSplicedAligner16::x_Align (SAlignInOut* data)
{
    /*
    // use the banded version if there is no space for introns
    const int len_dif (data->m_len2 - data->m_len1);
    if(data->m_len2 > 5 && data->m_len1 > 5 
       && len_dif < 2 * int (m_IntronMinSize) / 3)
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

    // redefine TScore as a floating-point type for this procedure only
    typedef double TScore;
    const TScore cds_penalty_extra = -2e-6;

    TScore V (0);

    const size_t N1 = data->m_len1 + 1;
    const size_t N2 = data->m_len2 + 1;
    vector<TScore> stl_rowV (N2), stl_rowF (N2);
    TScore* rowV    = &stl_rowV[0];
    TScore* rowF    = &stl_rowF[0];

    // index calculation: [i,j] = i*n2 + j
    SAllocator<Uint2> alloc_bm (N1*N2);
    Uint2* NCBI_RESTRICT backtrace_matrix (alloc_bm.GetPointer());

    SAllocator<Uint1> alloc_bm_ext (N1*N2);
    Uint1* NCBI_RESTRICT backtrace_matrix_ext (alloc_bm_ext.GetPointer());

    const char* NCBI_RESTRICT seq1 = m_Seq1 + data->m_offset1 - 1;
    const char* NCBI_RESTRICT seq2 = m_Seq2 + data->m_offset2 - 1;

    const TNCBIScore (*sm) [NCBI_FSM_DIM] = m_ScoreMatrix.s;

    bool bFreeGapLeft1  = data->m_esf_L1 && data->m_offset1 == 0;
    bool bFreeGapRight1 = data->m_esf_R1 &&
                          m_SeqLen1 == data->m_offset1 + data->m_len1;

    bool bFreeGapLeft2  = data->m_esf_L2 && data->m_offset2 == 0;
    bool bFreeGapRight2 = data->m_esf_R2 &&
                          m_SeqLen2 == data->m_offset2 + data->m_len2;

    TScore wgleft1   = bFreeGapLeft1? 0: m_Wg;
    TScore wsleft1   = bFreeGapLeft1? 0: m_Ws;
    TScore wg1 = wgleft1, ws1 = wsleft1;

    // recurrences
    TScore wgleft2   = bFreeGapLeft2? 0: m_Wg;
    TScore wsleft2   = bFreeGapLeft2? 0: m_Ws;
    TScore V_max, vAcc;
    TScore V0 = 0;
    TScore E, G, n0;
    Uint2 tracer;

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
    
    size_t* NCBI_RESTRICT jTail = jTail_d;
    size_t* NCBI_RESTRICT jHead = jHead_d;
    TScore* NCBI_RESTRICT vBestDonor = vBestDonor_d;
    size_t* NCBI_RESTRICT jBestDonor = jBestDonor_d;
	
    vector<size_t> stl_jAllDonors (splice_type_count_16 * N2);
    vector<TScore> stl_vAllDonors (splice_type_count_16 * N2);
    for(unsigned char st = 0; st < splice_type_count_16; ++st) {
        jAllDonors[st] = &stl_jAllDonors[st*N2];
        vAllDonors[st] = &stl_vAllDonors[st*N2];
    }		

    // fake row
    rowV[0] = kInfMinus;
    size_t k;
    for (k = 0; k < N2; k++) {
        rowV[k] = rowF[k] = kInfMinus;
    }
    k = 0;

    TScore* pV = rowV - 1;

    size_t i, j = 0, k0;
    unsigned char ci;

    size_t cds_start = m_cds_start, cds_stop = m_cds_stop;
    if(cds_start < cds_stop) {
        cds_start -= data->m_offset1;
        cds_stop -= data->m_offset1;
    }

    for(i = 0;  i < N1;  ++i, j = 0) {
       
        V = i > 0? (V0 += wsleft2) : 0;
        E = kInfMinus;
        k0 = k;
        backtrace_matrix[k++] = kMaskFc;
        ci = i > 0? seq1[i]: 'N';

        for(unsigned char st = 0; st < splice_type_count_16; ++st) {
            jTail[st] = jHead[st] = 0;
            vBestDonor[st] = kInfMinus;
            jBestDonor[st] = kMax_UInt;
        }

        TScore wg2 = m_Wg, ws2 = m_Ws;
            
        // detect donor candidate
        if (N2 > 2) {
            unsigned short v1 = (seq2[1] << 8) | seq2[2];
            for(unsigned char st = 0; st < g_topidx; ++st) {
                if (v1 == g_nwspl_donor_16[st]) {
                    size_t tl = jTail[st]++;
                    jAllDonors[st][tl] = j;
                    vAllDonors[st][tl] = V;
                }
            }
        }
        // the first max value
        {{
        size_t tl = jTail[g_topidx]++;
        jAllDonors[g_topidx][tl] = j;
        vAllDonors[g_topidx][tl] = V_max = V;
        }}

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
            			
            G = pV[j] + sm[ci][(unsigned char)seq2[j]];
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

            // find out if there are new donors (loop unroll)
            {{
#define NW_NDON_EVAL(st_idx) \
                { \
                size_t jt = jTail[st_idx]; \
                size_t jh = jHead[st_idx]; \
                if ((jt > jh) && \
                    (j - jAllDonors[st_idx][jh] >= m_IntronMinSize))  { \
                                                                        \
                    if (vAllDonors[st_idx][jh] > vBestDonor[st_idx]) {  \
                        vBestDonor[st_idx] = vAllDonors[st_idx][jh];    \
                        jBestDonor[st_idx] = jAllDonors[st_idx][jh];    \
                    } \
		    ++jHead[st_idx]; \
                } \
                } 
            NW_NDON_EVAL(0)
            NW_NDON_EVAL(1)
            NW_NDON_EVAL(2)
            NW_NDON_EVAL(3)
#undef NW_NDON_EVAL
            }}
                
            // check splice signal
            //                       (loop unroll)
            size_t intron_length = kMax_UInt;
            {{
                TScore v_best_d;
                if(j >= m_IntronMinSize) {
                    unsigned short v1 = (seq2[j-1] << 8) | seq2[j];

#define NW_SIG_EVAL(st_idx) \
                    v_best_d = vBestDonor[st_idx]; \
                    if ((v1 == g_nwspl_acceptor_16[st_idx]) && \
                        (v_best_d > kInfMinus)) \
                    { \
                        vAcc = v_best_d + m_Wi[st_idx]; \
                        if (vAcc > V) { \
                            V = vAcc; intron_length = j - jBestDonor[st_idx]; \
                        } \
                    } 
                    
                    NW_SIG_EVAL(0)
                    NW_SIG_EVAL(1)
                    NW_SIG_EVAL(2)
                }

                // iteration 3 (last element) is a bit different...
                v_best_d = vBestDonor[3];
                if (v_best_d > kInfMinus) {
                    vAcc = v_best_d + m_Wi[3];
                    if(vAcc > V) {
                        V = vAcc; intron_length = j - jBestDonor[3];
                    }
                }
            #undef NW_SIG_EVAL
            }}

            if (intron_length != kMax_UInt) {
                if(intron_length > 1048575) {
                    // no space to record introns longer than 2^20
                    NCBI_THROW(CAlgoAlignException,
                               eIntronTooLong,
                               g_msg_IntronTooLong);
                }

                backtrace_matrix_ext[k] = (0xFF000 & intron_length) >> 12;
                tracer |= (0x00FFF & intron_length) << 4;
            }

            backtrace_matrix[k] = tracer;

            // detect donor candidate
            if(j + 2 + m_IntronMinSize < N2) {
                unsigned short v1 = (seq2[j+1] << 8) | seq2[j+2];
#define NW_DON_EVAL(st_idx) \
                    if( (v1 == g_nwspl_donor_16[st_idx]) &&  \
                        (V > vBestDonor[st_idx])){  \
                        size_t tl = jTail[st_idx]++; \
                        jAllDonors[st_idx][tl] = j; \
                        vAllDonors[st_idx][tl] = V; \
                    } \

                NW_DON_EVAL(0)
                NW_DON_EVAL(1)
                NW_DON_EVAL(2)
#undef NW_DON_EVAL
            }
            
            // detect new best value
            if(V > V_max) {
                size_t tl = jTail[g_topidx]++;
                jAllDonors[g_topidx][tl] = j;
                vAllDonors[g_topidx][tl] = V_max = V;
            }
        
        } // for j

        pV[j] = V;

        if (i == 0) {
            V0 = wgleft2;
            wg1 = m_Wg;
            ws1 = m_Ws;
        }

    }

    try {
        x_DoBackTrace(backtrace_matrix, backtrace_matrix_ext, data);
    }
    catch(exception&) { // GCC hack
        throw;
    }
    
    return CNWAligner::TScore(V);
}


// perform backtrace step;
void CSplicedAligner16::x_DoBackTrace (
    const Uint2* backtrace_matrix,
    const Uint1* backtrace_matrix_ext,
    CNWAligner::SAlignInOut* data)
{
    const size_t N1 = data->m_len1 + 1;
    const size_t N2 = data->m_len2 + 1;

    data->m_transcript.clear();
    data->m_transcript.reserve(N1 + N2);

    size_t k = N1*N2 - 1;
    size_t i1 = data->m_offset1 + data->m_len1 - 1;
    size_t i2 = data->m_offset2 + data->m_len2 - 1;

    while (k != 0) {

        Uint2 Key = backtrace_matrix[k];
        Uint2 Key_ext = backtrace_matrix_ext[k];

        if(Key_ext > 0 || Key & 0xFFF0) {

            const size_t hi8 = Key_ext << 12;
            const size_t lo12 = (Key & 0xFFF0) >> 4;
            const size_t intron_length = hi8 | lo12;
            data->m_transcript.insert(data->m_transcript.end(),
                                      intron_length,
                                      eTS_Intron);
            k -= intron_length;
            i2 -= intron_length;

            if(intron_length < m_IntronMinSize) {
                
                NCBI_THROW(CAlgoAlignException,
                           eInternal,
                           "Min intron length violated");
            }

            Key_ext = backtrace_matrix_ext[k];
            Key = backtrace_matrix[k];

            if(Key_ext > 0 || Key & 0xFFF0) {
                NCBI_THROW(CAlgoAlignException,
                           eInternal,
                           "Adjacent splices encountered during backtrace");
            }
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
        NCBI_THROW(CAlgoAlignException, eInternal,
                   g_msg_InconsistentArguments);
    }

    const size_t dim = transcript.size();

    TScore score = 0;

    const char* p1 = m_Seq1 + start1;
    const char* p2 = m_Seq2 + start2;

    const TNCBIScore (*sm) [NCBI_FSM_DIM] = m_ScoreMatrix.s;

    char state1;   // 0 = normal, 1 = gap, 2 = intron
    char state2;   // 0 = normal, 1 = gap

    switch( transcript[0] ) {

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

    default: {
        NCBI_THROW(CAlgoAlignException, eInternal,
                   g_msg_InvalidTranscriptSymbol);
        }
    }

    for(size_t i = 0; i < dim; ++i) {

        ETranscriptSymbol ts = transcript[i];
        switch(ts) {

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

                        if(*p2 == g_nwspl_donor[i][0] &&
                           *(p2 + 1) == g_nwspl_donor[i][1] || i == g_topidx) {
                            
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

        default: {
        NCBI_THROW(CAlgoAlignException, eInternal,
                   g_msg_InvalidTranscriptSymbol);
        }
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
