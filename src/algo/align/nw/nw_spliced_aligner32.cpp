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


#include <algo/align/nw_spliced_aligner32.hpp>
#include <corelib/ncbi_limits.h>

BEGIN_NCBI_SCOPE

static unsigned char donor[splice_type_count_32][2] = {
    {'G','T'}, // donor type 1 in CDonorAcceptorMatrix coding
    {'G','C'}, // 2
    {'A','T'}  // 3
};

static unsigned char acceptor[splice_type_count_32][2] = {
    {'A','G'}, // 1
    {'A','G'}, // 2
    {'A','C'}  // 3
};


// Transcript coding
// (lower bits contain jump value for dels and introns)
const Uint4 kTypeD = 0x00000000; // match or mismatch
const Uint4 kTypeE = 0x10000000; // del in seq 1
const Uint4 kTypeF = 0x20000000; // del in seq 2
const Uint4 kTypeI = 0x30000000; // intron


// Donor-acceptor static object used
// to figure out what donors and aceptors
// could be transformed to any given na pair
// by mutating either of its characters (but not both).
// donor types:      bits 5-7
// acceptor types:   bits 1-3

class CDonorAcceptorMatrix {
public:
    CDonorAcceptorMatrix() {
        memset(m_matrix, 0, sizeof m_matrix);
        m_matrix['A']['A'] = 0x47; //       3 ; 1, 2, 3
        m_matrix['A']['G'] = 0x47; //       3 ; 1, 2, 3
        m_matrix['A']['T'] = 0x57; // 1,    3 ; 1, 2, 3
        m_matrix['A']['C'] = 0x67; //    2, 3 ; 1, 2, 3
        m_matrix['G']['A'] = 0x33; // 1, 2    ;
        m_matrix['G']['G'] = 0x33; // 1, 2    ; 1, 2
        m_matrix['G']['T'] = 0x70; // 1, 2, 3 ;
        m_matrix['G']['C'] = 0x34; // 1, 2    ;       3
        m_matrix['T']['A'] = 0x00; //         ;
        m_matrix['T']['G'] = 0x03; //         ; 1, 2
        m_matrix['T']['T'] = 0x50; // 1,    3 ;
        m_matrix['T']['C'] = 0x24; //    2    ;       3
        m_matrix['C']['A'] = 0x00; //         ;
        m_matrix['C']['G'] = 0x03; //         ; 1, 2
        m_matrix['C']['T'] = 0x50; // 1,    3 ;
        m_matrix['C']['C'] = 0x24; //    2    ;       3
    }
    const Uint1* GetMatrix() const {
        return m_matrix[0];
    }
private:
    Uint1 m_matrix [256][256];
};

static CDonorAcceptorMatrix g_dnr_acc_matrix;


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
    throw(CAlgoAlignException)
    : CSplicedAligner(seq1, len1, seq2, len2),
      m_Wd1(GetDefaultWd1()),
      m_Wd2(GetDefaultWd2())
{
    for(unsigned char st = 0; st < splice_type_count_32; ++st) {
        m_Wi[st] = GetDefaultWi(st);
    }
}


CNWAligner::TScore CSplicedAligner32::GetDefaultWi(unsigned char splice_type)
    throw(CAlgoAlignException)
{
    return CSplicedAligner::GetDefaultWi(splice_type);
}


// Evaluate dynamic programming matrix. Create transcript.
CNWAligner::TScore CSplicedAligner32::x_Align (
                         const char* seg1, size_t len1,
                         const char* seg2, size_t len2,
                         vector<ETranscriptSymbol>* transcript )
{
    const size_t N1 = len1 + 1;
    const size_t N2 = len2 + 1;

    TScore* rowV    = new TScore [N2];
    TScore* rowF    = new TScore [N2];

    // index calculation: [i,j] = i*n2 + j
    Uint4* backtrace_matrix = new Uint4 [N1*N2];

    TScore* pV = rowV - 1;

    const char* seq1   = seg1 - 1;
    const char* seq2   = seg2 - 1;

    bool bFreeGapLeft1  = m_esf_L1 && seg1 == m_Seq1;
    bool bFreeGapRight1 = m_esf_R1 && m_Seq1 + m_SeqLen1 - len1 == seg1;
    bool bFreeGapLeft2  = m_esf_L2 && seg2 == m_Seq2;
    bool bFreeGapRight2 = m_esf_R2 && m_Seq2 + m_SeqLen2 - len2 == seg2;

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
    for(unsigned char st = 0; st < splice_type_count_32; ++st) {
        jAllDonors[st] = new size_t [N2];
        vAllDonors[st] = new TScore [N2];
    }
    size_t  jTail[splice_type_count_32], jHead[splice_type_count_32];
    TScore  vBestDonor   [splice_type_count_32];
    size_t  jBestDonor   [splice_type_count_32];

    // donor/acceptor matrix
    const Uint1 * dnr_acc_matrix = g_dnr_acc_matrix.GetMatrix();

    // fake row
    rowV[0] = kInfMinus;
    size_t k;
    for (k = 0; k < N2; k++) {
        rowV[k] = rowF[k] = kInfMinus;
    }
    k = 0;

    size_t i, j = 0, k0;
    char ci;
    for(i = 0;  i < N1;  ++i, j = 0) {
       
        V = i > 0? (V0 += wsleft2) : 0;
        E = kInfMinus;
        k0 = k;
        backtrace_matrix[k++] = kTypeF | 1;
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
            char c1 = seq2[1], c2 = seq2[2];
            Uint1 dnr_type = 0xF0 & dnr_acc_matrix[(size_t(c1)<<8)|c2];

            for(Uint1 st = 0; st < splice_type_count_32; ++st ) {
                jAllDonors[st][jTail[st]] = j;
                if(dnr_type & (0x10 << st)) {
                    vAllDonors[st][jTail[st]] = 
                        ( c1 == donor[st][0] && c2 == donor[st][1] ) ?
                        V: (V + m_Wd1);
                }
                else { // both chars distorted
                    vAllDonors[st][jTail[st]] = V + m_Wd2;
                }
                ++(jTail[st]);
            }
        }

        for (j = 1; j < N2; ++j, ++k) {
            
            G = pV[j] + m_Matrix[ci][seq2[j]];
            pV[j] = V;

            n0 = V + wg1;
            if(E >= n0) {
                E += ws1;      // continue the gap
            }
            else {
                E = n0 + ws1;  // open a new gap
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
            }

            // evaluate the score (V)
            if (E >= rowF[j]) {
                if(E >= G) {
                    V = E;
                    type = kTypeE;
                }
                else {
                    V = G;
                    type = kTypeD;
                }
            } else {
                if(rowF[j] >= G) {
                    V = rowF[j];
                    type = kTypeF;
                }
                else {
                    V = G;
                    type = kTypeD;
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
            size_t dnr_pos = 0;
            char c1 = seq2[j-1], c2 = seq2[j];
            Uint1 acc_mask = 0x0F & dnr_acc_matrix[(size_t(c1)<<8)|c2];
            for(Uint1 st = 0; st < splice_type_count_32; ++st ) {
                if(acc_mask & (0x01 << st)) {
                    TScore vAcc = vBestDonor[st] + m_Wi[st];
                    if( c1 != acceptor[st][0] || c2 != acceptor[st][1] ) {
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
            
            if(dnr_pos > 0) {
                type = kTypeI | dnr_pos;
            }

            backtrace_matrix[k] = type;

            // detect donor candidates
            if(j < N2 - 2) {
                char c1 = seq2[j+1], c2 = seq2[j+2];
                Uint1 dnr_mask = 0xF0 & dnr_acc_matrix[(size_t(c1)<<8)|c2];
                for(Uint1 st = 0; st < splice_type_count_32; ++st ) {
                    if( dnr_mask & (0x10 << st) ) {
                        if( c1 == donor[st][0] && c2 == donor[st][1] ) {
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

    for(unsigned char st = 0; st < splice_type_count_32; ++st) {
        delete[] jAllDonors[st];
        delete[] vAllDonors[st];
    }
    delete[] rowV;
    delete[] rowF;

    x_DoBackTrace(backtrace_matrix, N1, N2, transcript);

    delete[] backtrace_matrix;

    return V;
}



// perform backtrace step;
void CSplicedAligner32::x_DoBackTrace ( const Uint4* backtrace_matrix,
                                         size_t N1, size_t N2,
                                         vector<ETranscriptSymbol>* transcript)
{
    transcript->clear();
    transcript->reserve(N1 + N2);

    const Uint4 mask_jump = 0x0FFFFFFF;
    const Uint4 mask_type = ~mask_jump;

    size_t k = N1*N2 - 1;

    while (k != 0) {
        Uint4 Key = backtrace_matrix[k];
        Uint4 type = Key & mask_type;

        if(type == kTypeI) {  // intron
            for(size_t k2 = (Key & mask_jump); k != k2; --k) {
                transcript->push_back(eTS_Intron);
            }
        }
        else if (type == kTypeD) {
            transcript->push_back(eTS_Match); // could be eReplace actually
            k -= N2 + 1;
        }
        else if (type == kTypeE) {
            transcript->push_back(eTS_Insert);
            --k;
        }
        else {
            transcript->push_back(eTS_Delete);
            k -= N2;
        }
    } // while
}



CNWAligner::TScore CSplicedAligner32::x_ScoreByTranscript() const
    throw (CAlgoAlignException)
{
    const size_t dim = m_Transcript.size();
    if(dim == 0) return 0;

    TScore score = 0;

    const char* p1 = m_Seq1;
    const char* p2 = m_Seq2;

    char state1;   // 0 = normal, 1 = gap, 2 = intron
    char state2;   // 0 = normal, 1 = gap

    switch( m_Transcript[dim - 1] ) {

    case eTS_Match:
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
        NCBI_THROW(
                   CAlgoAlignException,
                   eInternal,
                   "Invalid transcript symbol");
        }
    }

    for(int i = dim - 1; i >= 0; --i) {

        char c1 = m_Seq1? *p1: 0;
        char c2 = m_Seq2? *p2: 0;
        switch(m_Transcript[i]) {

        case eTS_Match: {
            state1 = state2 = 0;
            score += m_Matrix[c1][c2];
            ++p1; ++p2;
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
                for(unsigned char i = 0; i < splice_type_count_32; ++i) {
                    if(*p2 == donor[i][0] && *(p2 + 1) == donor[i][1]) {
                        score += m_Wi[i];
                        break;
                    }
                }
            }
            state1 = 2; state2 = 0;
            ++p2;
        }
        break;

        default: {
        NCBI_THROW(
                   CAlgoAlignException,
                   eInternal,
                   "Invalid transcript symbol");
        }
        }
    }

    if(m_esf_L1) {
        size_t g = 0;
        for(int i = dim - 1; i >= 0; --i) {
            if(m_Transcript[i] == eTS_Insert) ++g; else break;
        }
        if(g > 0) {
            score -= (m_Wg + g*m_Ws);
        }
    }

    if(m_esf_L2) {
        size_t g = 0;
        for(int i = dim - 1; i >= 0; --i) {
            if(m_Transcript[i] == eTS_Delete) ++g; else break;
        }
        if(g > 0) {
            score -= (m_Wg + g*m_Ws);
        }
    }

    if(m_esf_R1) {
        size_t g = 0;
        for(size_t i = 0; i < dim; ++i) {
            if(m_Transcript[i] == eTS_Insert) ++g; else break;
        }
        if(g > 0) {
            score -= (m_Wg + g*m_Ws);
        }
    }

    if(m_esf_R2) {
        size_t g = 0;
        for(size_t i = 0; i < dim; ++i) {
            if(m_Transcript[i] == eTS_Delete) ++g; else break;
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
 * Revision 1.1  2003/09/02 22:34:49  kapustin
 * Initial revision
 *
 * ===========================================================================
 */
