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

const unsigned char g_nwspl_acceptor[splice_type_count_16][2] = {
    {'A','G'}, {'A','G'}, {'A','C'}, {'?','?'}
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
// [11-8] donors (bitwise OR for multiple types)
//        1000     ??    (??/??) - arbitrary pair
//        0100     AT    (AT/AC)
//        0010     GC    (GC/AG)
//        0001     GT    (GT/AG)
// [7-5]  acceptor type
//        100      ?? (??/??)
//        011      AC (AT/AC)
//        010      AG (GC/AG)
//        001      AG (GT/AG)
//        000      no acceptor
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
    TScore V = 0;

    const size_t N1 = data->m_len1 + 1;
    const size_t N2 = data->m_len2 + 1;

    vector<TScore> stl_rowV (N2), stl_rowF (N2);
    TScore* rowV    = &stl_rowV[0];
    TScore* rowF    = &stl_rowF[0];

    // index calculation: [i,j] = i*n2 + j
    vector<Uint2> stl_bm (N1*N2);
    Uint2* backtrace_matrix = &stl_bm[0];
    TScore* pV = rowV - 1;

    const char* seq1 = m_Seq1 + data->m_offset1 - 1;
    const char* seq2 = m_Seq2 + data->m_offset2 - 1;

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
    size_t* jAllDonors [splice_type_count_16];
    TScore* vAllDonors [splice_type_count_16];
    vector<size_t> stl_jAllDonors (splice_type_count_16 * N2);
    vector<TScore> stl_vAllDonors (splice_type_count_16 * N2);
    for(unsigned char st = 0; st < splice_type_count_16; ++st) {
        jAllDonors[st] = &stl_jAllDonors[st*N2];
        vAllDonors[st] = &stl_vAllDonors[st*N2];
    }
    size_t  jTail[splice_type_count_16], jHead[splice_type_count_16];
    TScore  vBestDonor [splice_type_count_16];
    size_t  jBestDonor [splice_type_count_16];

    // fake row
    rowV[0] = kInfMinus;
    size_t k;
    for (k = 0; k < N2; k++) {
        rowV[k] = rowF[k] = kInfMinus;
    }
    k = 0;

    size_t i, j = 0, k0;
    unsigned char ci;
    for(i = 0;  i < N1;  ++i, j = 0) {
       
        V = i > 0? (V0 += wsleft2) : 0;
        E = kInfMinus;
        k0 = k;
        backtrace_matrix[k++] = kMaskFc;
        ci = i > 0? seq1[i]: 'N';

        for(unsigned char st = 0; st < splice_type_count_16; ++st) {
            jTail[st] = jHead[st] = 0;
            vBestDonor[st] = kInfMinus;
        }

        if(i == N1 - 1 && bFreeGapRight1) {
                wg1 = ws1 = 0;
        }

        TScore wg2 = m_Wg, ws2 = m_Ws;
            
        // detect donor candidate
        if(N2 > 2) {
	  for(unsigned char st = 0; st < g_topidx; ++st) {
                if(seq2[1] == g_nwspl_donor[st][0] &&
                   seq2[2] == g_nwspl_donor[st][1]) {

                    jAllDonors[st][jTail[st]] = j;
                    vAllDonors[st][jTail[st]] = V;
                    ++(jTail[st]);
                }
            }
        }
        // the first max value
        jAllDonors[g_topidx][jTail[g_topidx]] = j;
        vAllDonors[g_topidx][jTail[g_topidx]] = V_max = V;
        ++(jTail[3]);

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

            // find out if there are new donors
            for(unsigned char st = 0; st < splice_type_count_16; ++st) {

                if(jTail[st] > jHead[st])  {
                    if(j - jAllDonors[st][jHead[st]] >= m_IntronMinSize) {
                        if(vAllDonors[st][jHead[st]] > vBestDonor[st]) {
                            vBestDonor[st] = vAllDonors[st][jHead[st]];
                            jBestDonor[st] = jAllDonors[st][jHead[st]];
                        }
                        ++(jHead[st]);
                    }
                }
            }
                
            // check splice signal
            size_t dnr_pos = 0;
            Uint2 tracer_dnr = 0xFFFF;
            Uint2 tracer_acc = 0;
	    for(unsigned char st = 0; st < splice_type_count_16; ++st) {
                if(seq2[j-1] == g_nwspl_acceptor[st][0] &&
                   seq2[j] == g_nwspl_acceptor[st][1] &&
                   vBestDonor[st] > kInfMinus || st == g_topidx) {

                    vAcc = vBestDonor[st] + m_Wi[st];
                    if(vAcc > V) {
                        V = vAcc;
                        tracer_acc = (st+1) << 4;
                        dnr_pos = k0 + jBestDonor[st];
                        tracer_dnr = 0x0080 << st;
                    }
                }
            }

            if(tracer_dnr != 0xFFFF) {
                backtrace_matrix[dnr_pos] |= tracer_dnr;
                tracer |= tracer_acc;
            }

            backtrace_matrix[k] = tracer;

            // detect donor candidate
            if(j < N2 - 2) {
                for(unsigned char st = 0; st < g_topidx; ++st) {
                    
                    if( seq2[j+1] == g_nwspl_donor[st][0] &&
                        seq2[j+2] == g_nwspl_donor[st][1] &&
                        V > vBestDonor[st]  ) {
                        
                        jAllDonors[st][jTail[st]] = j;
                        vAllDonors[st][jTail[st]] = V;
                        ++(jTail[st]);
                    }
                }
            }
            
            // detect new best value
            if(V > V_max) {
                jAllDonors[g_topidx][jTail[g_topidx]] = j;
                vAllDonors[g_topidx][jTail[g_topidx]] = V_max = V;
                ++(jTail[3]);
            }
        }

        pV[j] = V;

        if(i == 0) {
            V0 = wgleft2;
            wg1 = m_Wg;
            ws1 = m_Ws;
        }

    }

    try {
        x_DoBackTrace(backtrace_matrix, data);
    }
    catch(exception&) { // GCC hack
        throw;
    }
    
    return V;
}


// perform backtrace step;
void CSplicedAligner16::x_DoBackTrace (const Uint2* backtrace_matrix,
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
        while(Key & 0x0070) {  // acceptor

            unsigned char acc_type = (Key & 0x0070) >> 4;
            Uint2 dnr_mask = 0x0040 << acc_type;
            ETranscriptSymbol ets = eTS_Intron;
            do {
                data->m_transcript.push_back(ets);
                Key = backtrace_matrix[--k];
                --i2;
            }
            while(!(Key & dnr_mask));

            if(Key & 0x0070) {
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



/*
 * ===========================================================================
 * $Log$
 * Revision 1.20  2005/04/04 16:34:13  kapustin
 * Specify precise type of diags in raw alignment transcripts where feasible
 *
 * Revision 1.19  2005/03/16 15:48:26  jcherry
 * Allow use of std::string for specifying sequences
 *
 * Revision 1.18  2004/12/16 22:42:22  kapustin
 * Move to algo/align/nw
 *
 * Revision 1.17  2004/12/06 22:13:36  kapustin
 * File header update
 *
 * Revision 1.16  2004/11/29 14:37:15  kapustin
 * CNWAligner::GetTranscript now returns TTranscript and direction can be specified. x_ScoreByTanscript renamed to ScoreFromTranscript with two additional parameters to specify starting coordinates.
 *
 * Revision 1.15  2004/08/31 16:17:21  papadopo
 * make SAlignInOut work with sequence offsets rather than char pointers
 *
 * Revision 1.14  2004/06/29 20:51:21  kapustin
 * Support simultaneous segment computing
 *
 * Revision 1.13  2004/05/21 21:41:02  gorelenk
 * Added PCH ncbi_pch.hpp
 *
 * Revision 1.12  2004/05/18 21:43:40  kapustin
 * Code cleanup
 *
 * Revision 1.11  2004/05/17 14:50:56  kapustin
 * Add/remove/rearrange some includes and object declarations
 *
 * Revision 1.10  2004/04/23 14:39:47  kapustin
 * Add Splign library and other changes
 *
 * Revision 1.8  2003/11/20 17:54:23  kapustin
 * Alternative conventional splice penalty adjusted
 *
 * Revision 1.7  2003/10/31 19:40:13  kapustin
 * Get rid of some WS and GCC complains
 *
 * Revision 1.6  2003/10/27 21:00:17  kapustin
 * Set intron penalty defaults differently for 16- and 32-bit versions
 * according to the expected quality of sequences those variants are
 * supposed to be used with.
 *
 * Revision 1.5  2003/10/14 19:29:24  kapustin
 * Dismiss static keyword as a local-to-compilation-unit flag. Use longer
 * name since unnamed namespaces are not everywhere supported
 *
 * Revision 1.4  2003/09/30 19:50:04  kapustin
 * Make use of standard score matrix interface
 *
 * Revision 1.3  2003/09/26 14:43:18  kapustin
 * Remove exception specifications
 *
 * Revision 1.2  2003/09/04 16:07:38  kapustin
 * Use STL vectors for exception-safe dynamic arrays and matrices
 *
 * Revision 1.1  2003/09/02 22:34:49  kapustin
 * Initial revision
 *
 * ===========================================================================
 */
