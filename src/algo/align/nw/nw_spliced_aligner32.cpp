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
* Authors:  Yuri Kapustin
*
* File Description:  CSplicedAligner32
*                   
* ===========================================================================
*
*/


#include <ncbi_pch.hpp>
#include "messages.hpp"
#include <algo/align/nw/nw_spliced_aligner32.hpp>
#include <algo/align/nw/align_exception.hpp>

BEGIN_NCBI_SCOPE

const unsigned char g_nwspl32_donor[splice_type_count_32][2] = {
    {'G','T'}, // donor type 1 in CDonorAcceptorMatrix coding
    {'G','C'}, // 2
    {'A','T'}  // 3
};

const unsigned char g_nwspl32_acceptor[splice_type_count_32][2] = {
    {'A','G'}, // 1
    {'A','G'}, // 2
    {'A','C'}  // 3
};


// Transcript coding
// (lower bits contain jump value for gaps and introns)
const Uint4 kTypeDiag   = 0x00000000;  // single match or mismatch
const Uint4 kTypeGap    = 0x40000000;  // gap - any type
const Uint4 kTypeIntron = 0x80000000;  // intron


// Donor-acceptor static object used
// to figure out what donors and aceptors
// could be transformed to any given na pair
// by mutating either of its characters (but not both).
// donor types:      bits 5-7
// acceptor types:   bits 1-3

class CDonorAcceptorMatrix {

public:

    CDonorAcceptorMatrix() {
        memset(m_Matrix, 0, sizeof m_Matrix);
        x_Set('A', 'A', 0x47); //       3 ; 1, 2, 3
        x_Set('A', 'G', 0x47); //       3 ; 1, 2, 3
        x_Set('A', 'T', 0x57); // 1,    3 ; 1, 2, 3
        x_Set('A', 'C', 0x67); //    2, 3 ; 1, 2, 3
        x_Set('G', 'A', 0x33); // 1, 2    ;
        x_Set('G', 'G', 0x33); // 1, 2    ; 1, 2
        x_Set('G', 'T', 0x70); // 1, 2, 3 ;
        x_Set('G', 'C', 0x34); // 1, 2    ;       3
        x_Set('T', 'A', 0x00); //         ;
        x_Set('T', 'G', 0x03); //         ; 1, 2
        x_Set('T', 'T', 0x50); // 1,    3 ;
        x_Set('T', 'C', 0x24); //    2    ;       3
        x_Set('C', 'A', 0x00); //         ;
        x_Set('C', 'G', 0x03); //         ; 1, 2
        x_Set('C', 'T', 0x50); // 1,    3 ;
        x_Set('C', 'C', 0x24); //    2    ;       3
    }

    const Uint1* GetMatrix() const {
        return m_Matrix[0];
    }

private:

    void x_Set(char c1, char c2, Uint1 val) {
        m_Matrix[(unsigned char)c1][(unsigned char)c2] = val;
    }

    Uint1 m_Matrix [256][256];

};


namespace {
    CDonorAcceptorMatrix g_dnr_acc_matrix;
}

CSplicedAligner32::CSplicedAligner32():
      m_Wd1(GetDefaultWd1()),
      m_Wd2(GetDefaultWd2())
{
    for(unsigned char st = 0; st < splice_type_count_32; ++st) {
        m_Wi[st] = GetDefaultWi(st);
    }
}

CSplicedAligner32::CSplicedAligner32(const char* seq1, size_t len1,
                                     const char* seq2, size_t len2)
    : CSplicedAligner(seq1, len1, seq2, len2),
      m_Wd1(GetDefaultWd1()),
      m_Wd2(GetDefaultWd2())
{
    for(unsigned char st = 0; st < splice_type_count_32; ++st) {
        m_Wi[st] = GetDefaultWi(st);
    }
}


CSplicedAligner32::CSplicedAligner32(const string& seq1, const string& seq2)
    : CSplicedAligner(seq1, seq2),
      m_Wd1(GetDefaultWd1()),
      m_Wd2(GetDefaultWd2())
{
    for(unsigned char st = 0; st < splice_type_count_32; ++st) {
        m_Wi[st] = GetDefaultWi(st);
    }
}


CNWAligner::TScore CSplicedAligner32::GetDefaultWi(unsigned char splice_type)
{
    switch(splice_type) {
        case 0: return -15; // GT/AG
        case 1: return -18; // GC/AG
        case 2: return -21; // AT/AC
        default: {
            NCBI_THROW(CAlgoAlignException,
                       eInvalidSpliceTypeIndex,
                       g_msg_InvalidSpliceTypeIndex);
        }
    }
}


// Evaluate dynamic programming matrix. Create transcript.
CNWAligner::TScore CSplicedAligner32::x_Align (SAlignInOut* data)
{
    const size_t N1 = data->m_len1 + 1;
    const size_t N2 = data->m_len2 + 1;

    vector<TScore> stl_rowV (N2), stl_rowF (N2);
    TScore* rowV    = &stl_rowV[0];
    TScore* rowF    = &stl_rowF[0];

    // index calculation: [i,j] = i*n2 + j
    vector<Uint4> stl_bm (N1*N2);
    Uint4* backtrace_matrix = &stl_bm[0];

    TScore* pV = rowV - 1;

    const char* seq1   = m_Seq1 + data->m_offset1 - 1;
    const char* seq2   = m_Seq2 + data->m_offset2 - 1;

    const TNCBIScore (*sm) [NCBI_FSM_DIM] = m_ScoreMatrix.s;

    bool bFreeGapLeft1  = data->m_esf_L1 && data->m_offset1 == 0;
    bool bFreeGapRight1 = data->m_esf_R1 &&
                          m_SeqLen1 == data->m_offset1 + data->m_len1;

    bool bFreeGapLeft2  = data->m_esf_L2 && data->m_offset1 == 0;
    bool bFreeGapRight2 = data->m_esf_R2 &&
                          m_SeqLen2 == data->m_offset2 + data->m_len2;

    TScore wgleft1   = bFreeGapLeft1? 0: m_Wg;
    TScore wsleft1   = bFreeGapLeft1? 0: m_Ws;
    TScore wg1 = wgleft1, ws1 = wsleft1;

    // recurrences
    TScore wgleft2   = bFreeGapLeft2? 0: m_Wg;
    TScore wsleft2   = bFreeGapLeft2? 0: m_Ws;
    TScore V  = 0;
    TScore V0 = 0;
    TScore E, G, n0;
    Uint4 type;

    // store candidate donors
    size_t* jAllDonors [splice_type_count_32];
    TScore* vAllDonors [splice_type_count_32];
    vector<size_t> stl_jAllDonors (splice_type_count_32 * N2);
    vector<TScore> stl_vAllDonors (splice_type_count_32 * N2);
    for(unsigned char st = 0; st < splice_type_count_32; ++st) {
        jAllDonors[st] = &stl_jAllDonors[st*N2];
        vAllDonors[st] = &stl_vAllDonors[st*N2];
    }
    size_t  jTail[splice_type_count_32], jHead[splice_type_count_32];
    TScore  vBestDonor   [splice_type_count_32];
    size_t  jBestDonor   [splice_type_count_32];

    // place to store gap opening starts
    size_t ins_start;
    vector<size_t> stl_del_start(N2);
    size_t* del_start = &stl_del_start[0];

    // donor/acceptor matrix
    const Uint1 * dnr_acc_matrix = g_dnr_acc_matrix.GetMatrix();

    // fake row (above lambda)
    rowV[0] = kInfMinus;
    size_t k;
    for (k = 0; k < N2; k++) {
        rowV[k] = rowF[k] = kInfMinus;
	del_start[k] = k;
    }
    k = 0;

    size_t i, j = 0, k0;
    unsigned char ci;
    for(i = 0;  i < N1;  ++i, j = 0) {

        V = i > 0? (V0 += wsleft2) : 0;
        E = kInfMinus;
        ins_start = k0 = k;
        backtrace_matrix[k++] = kTypeGap; // | del_start[0]
        ci = i > 0? seq1[i]: 'N';

        for(unsigned char st = 0; st < splice_type_count_32; ++st) {
            jTail[st] = jHead[st] = 0;
            vBestDonor[st] = kInfMinus;
        }

        if(i == N1 - 1 && bFreeGapRight1) {
                wg1 = ws1 = 0;
        }

        TScore wg2 = m_Wg, ws2 = m_Ws;
            
        // detect donor candidate
        if(N2 > 2) {
            unsigned char d1 = seq2[1], d2 = seq2[2];
            Uint1 dnr_type = 0xF0 & dnr_acc_matrix[(size_t(d1)<<8)|d2];

            for(Uint1 st = 0; st < splice_type_count_32; ++st ) {
                jAllDonors[st][jTail[st]] = j;
                if(dnr_type & (0x10 << st)) {
                    vAllDonors[st][jTail[st]] = 
                        ( d1 == g_nwspl32_donor[st][0] &&
                          d2 == g_nwspl32_donor[st][1] ) ? V: (V + m_Wd1);
                }
                else { // both chars distorted
                    vAllDonors[st][jTail[st]] = V + m_Wd2;
                }
                ++(jTail[st]);
            }
        }

        for (j = 1; j < N2; ++j, ++k) {
            
            G = pV[j] + sm[ci][(unsigned char)seq2[j]];
            pV[j] = V;

            n0 = V + wg1;
            if(E >= n0) {
                E += ws1;      // continue the gap
            }
            else {
                E = n0 + ws1;  // open a new gap
		ins_start = k-1;
            }

            if(j == N2 - 1 && bFreeGapRight2) {
                wg2 = ws2 = 0;
            }
            n0 = rowV[j] + wg2;
            if(rowF[j] >= n0) {
                rowF[j] += ws2;
            }
            else {
                rowF[j] = n0 + ws2;
                del_start[j] = k-N2;
            }

            // evaluate the score (V)
            if (E >= rowF[j]) {
                if(E >= G) {
                    V = E;
                    type = kTypeGap | ins_start;
                }
                else {
                    V = G;
                    type = kTypeDiag;
                }
            } else {
                if(rowF[j] >= G) {
                    V = rowF[j];
                    type = kTypeGap | del_start[j];
                }
                else {
                    V = G;
                    type = kTypeDiag;
                }
            }

            // find out if there are new donors
            for(unsigned char st = 0; st < splice_type_count_32; ++st) {

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
            Uint4 dnr_pos = kMax_UI4;
            unsigned char c1 = seq2[j-1], c2 = seq2[j];
            Uint1 acc_mask = 0x0F & dnr_acc_matrix[(size_t(c1)<<8)|c2];
            for(Uint1 st = 0; st < splice_type_count_32; ++st ) {
                if(acc_mask & (0x01 << st)) {
                    TScore vAcc = vBestDonor[st] + m_Wi[st];
                    if( c1 != g_nwspl32_acceptor[st][0] ||
                        c2 != g_nwspl32_acceptor[st][1] ) {

                        vAcc += m_Wd1;
                    }
                    if(vAcc > V) {
                        V = vAcc;
                        dnr_pos = k0 + jBestDonor[st];
                    }
                }
                else {   // try arbitrary splice
                    TScore vAcc = vBestDonor[st] + m_Wi[st] + m_Wd2;
                    if(vAcc > V) {
                        V = vAcc;
                        dnr_pos = k0 + jBestDonor[st];
                    }
                }
            }
            
            if(dnr_pos != kMax_UI4) {
                type = kTypeIntron | dnr_pos;
            }

            backtrace_matrix[k] = type;

            // detect donor candidates
            if(j < N2 - 2) {
                unsigned char d1 = seq2[j+1], d2 = seq2[j+2];
                Uint1 dnr_mask = 0xF0 & dnr_acc_matrix[(size_t(d1)<<8)|d2];
                for(Uint1 st = 0; st < splice_type_count_32; ++st ) {
                    if( dnr_mask & (0x10 << st) ) {
                        if( d1 == g_nwspl32_donor[st][0] &&
                            d2 == g_nwspl32_donor[st][1] ) {

                            if(V > vBestDonor[st]) {
                                jAllDonors[st][jTail[st]] = j;
                                vAllDonors[st][jTail[st]] = V;
                                ++(jTail[st]);
                            }
                        } else {
                            TScore v = V + m_Wd1;
                            if(v > vBestDonor[st]) {
                                jAllDonors[st][jTail[st]] = j;
                                vAllDonors[st][jTail[st]] = v;
                                ++(jTail[st]);
                            }
                        }
                    }
                    else { // both chars distorted
                        TScore v = V + m_Wd2;
                        if(v > vBestDonor[st]) {
                            jAllDonors[st][jTail[st]] = j;
                            vAllDonors[st][jTail[st]] = v;
                            ++(jTail[st]);
                        }
                    }
                }
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
void CSplicedAligner32::x_DoBackTrace (const Uint4* backtrace_matrix,
                                       CNWAligner::SAlignInOut* data) 
{
    const size_t N1 = data->m_len1 + 1;
    const size_t N2 = data->m_len2 + 1;
    
    data->m_transcript.clear();
    data->m_transcript.reserve(N1 + N2);
    
    size_t k = N1*N2 - 1;
    size_t i1 = data->m_offset1 + data->m_len1 - 1;
    size_t i2 = data->m_offset2 + data->m_len2 - 1;
    
    const Uint4 mask_jump = 0x3FFFFFFF;
    const Uint4 mask_type = ~mask_jump;
    
    while (k != 0) {
        
        Uint4 Key = backtrace_matrix[k];
        Uint4 type = Key & mask_type;
        
	if(type == kTypeDiag) {
	    data->m_transcript.push_back(x_GetDiagTS(i1--, i2--));
	    k -= N2 + 1;
	}
	else {
            
            Uint4 k2 = (Key & mask_jump);
            if(k2 >= k) {
	        NCBI_THROW(CAlgoAlignException, eInternal,
                           g_msg_InvalidBacktraceData);
            }
            
            ETranscriptSymbol ts;
            Uint4 decr;
            if(type == kTypeIntron) {
                ts = eTS_Intron;
                decr = 1;
            }
            else {
                Uint4 kdel = k / N2, k2del = k2 / N2;
                Uint4 kmod = k % N2, k2mod = k2 % N2;
                if(kdel == k2del) {
                    ts = eTS_Insert;
                    decr = 1;
                }
                else if(kmod == k2mod) {
                    ts = eTS_Delete;
                    decr = N2;
                }
                else {
                    // ts = eTS_DiagSpace (not yet supported)
                    NCBI_THROW(CAlgoAlignException, eInternal,
                               g_msg_InvalidBacktraceData);
                }
            }
            
            for(; k > k2; k -= decr) {
                data->m_transcript.push_back(ts);
                if(decr == 1) {
                    --i2;
                }
                else {
                    --i1;
                }
            }
            if(k != k2) {
                NCBI_THROW(CAlgoAlignException, eInternal,
                           g_msg_InvalidBacktraceData);
            }
	}
    }
}



CNWAligner::TScore CSplicedAligner32::ScoreFromTranscript(
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

        case eTS_Replace:
        case eTS_Match: {
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
                    score += m_Wi[0];
                }
                else {
                    for(unsigned char k = 0; k < splice_type_count_32; ++k) {
                        if(*p2 == g_nwspl32_donor[k][0] && 
                           *(p2 + 1) == g_nwspl32_donor[k][1]) {
                            
                            score += m_Wi[k];
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
 * Revision 1.22  2005/04/04 16:34:13  kapustin
 * Specify precise type of diags in raw alignment transcripts where feasible
 *
 * Revision 1.21  2005/03/16 15:48:26  jcherry
 * Allow use of std::string for specifying sequences
 *
 * Revision 1.20  2005/03/02 14:26:16  kapustin
 * A few tweaks to mute GCC and MSVC warnings
 *
 * Revision 1.19  2004/12/16 22:42:22  kapustin
 * Move to algo/align/nw
 *
 * Revision 1.18  2004/11/29 14:37:15  kapustin
 * CNWAligner::GetTranscript now returns TTranscript and direction can be specified. x_ScoreByTanscript renamed to ScoreFromTranscript with two additional parameters to specify starting coordinates.
 *
 * Revision 1.17  2004/08/31 16:17:21  papadopo
 * make SAlignInOut work with sequence offsets rather than char pointers
 *
 * Revision 1.16  2004/06/29 20:51:21  kapustin
 * Support simultaneous segment computing
 *
 * Revision 1.15  2004/05/21 21:41:02  gorelenk
 * Added PCH ncbi_pch.hpp
 *
 * Revision 1.14  2004/05/18 21:43:40  kapustin
 * Code cleanup
 *
 * Revision 1.13  2004/05/17 14:50:57  kapustin
 * Add/remove/rearrange some includes and object declarations
 *
 * Revision 1.12  2004/04/23 14:39:47  kapustin
 * Add Splign library and other changes
 *
 * Revision 1.10  2003/11/20 17:54:23  kapustin
 * Alternative conventional splice penalty adjusted
 *
 * Revision 1.9  2003/10/31 19:40:13  kapustin
 * Get rid of some WS and GCC complains
 *
 * Revision 1.8  2003/10/27 21:00:17  kapustin
 * Set intron penalty defaults differently for 16- and 32-bit versions according to the expected quality of sequences those variants are supposed to be used with.
 *
 * Revision 1.7  2003/10/14 19:31:52  kapustin
 * Use one flag for all gap types
 *
 * Revision 1.6  2003/09/30 19:50:04  kapustin
 * Make use of standard score matrix interface
 *
 * Revision 1.5  2003/09/26 14:43:18  kapustin
 * Remove exception specifications
 *
 * Revision 1.4  2003/09/10 20:25:21  kapustin
 * Use unsigned char for score matrix index
 *
 * Revision 1.3  2003/09/10 19:13:10  kapustin
 * Change backtrace type constants to allow larger jump values
 *
 * Revision 1.2  2003/09/04 16:07:38  kapustin
 * Use STL vectors for exception-safe dynamic arrays and matrices
 *
 * Revision 1.1  2003/09/02 22:34:49  kapustin
 * Initial revision
 *
 * ===========================================================================
 */
