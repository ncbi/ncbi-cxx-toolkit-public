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
* File Description:  CNWAlignerMrna2Dna
*                   
* ===========================================================================
*
*/


#include <algo/nw_aligner_mrna2dna.hpp>
#include <corelib/ncbi_limits.h>

BEGIN_NCBI_SCOPE

static unsigned char donor[splice_type_count][2] = {
    {'G','T'}, {'G','C'}, {'A','T'}
};

static unsigned char acceptor[splice_type_count][2] = {
    {'A','G'}, {'A','G'}, {'A','C'}
};

/*

static unsigned char donor[splice_type_count][2] = {
    {'G','T'}
};

static unsigned char acceptor[splice_type_count][2] = {
    {'A','G'}
};

*/

CNWAlignerMrna2Dna::CNWAlignerMrna2Dna(const char* seq1, size_t len1,
                                       const char* seq2, size_t len2)
    throw(CNWAlignerException)
    : CNWAligner(seq1, len1, seq2, len2, eNucl),
    m_IntronMinSize(GetDefaultIntronMinSize())
{
    for(unsigned char st = 0; st < splice_type_count; ++st) {
        m_Wi[st] = GetDefaultWi(st);
    }
}

CNWAligner::TScore CNWAlignerMrna2Dna::GetDefaultWi(unsigned char splice_type)
    throw(CNWAlignerException)
{
    switch(splice_type) {
    case 0: case 1: case 2: default:
        return -10;
    }

    NCBI_THROW(CNWAlignerException,
               eInvalidSpliceTypeIndex,
               "Invalid splice type index");
}


// evaluate score for each possible alignment;
// fill out backtrace matrix
// bit coding (eight bits per value): D E Ec Fc Acc Dnr
// D:    1 if diagonal; 0 - otherwise
// E:    1 if space in 1st sequence; 0 if space in 2nd sequence
// Ec:   1 if gap in 1st sequence was extended; 0 if it is was opened
// Fc:   1 if gap in 2nd sequence was extended; 0 if it is was opened
// The four remaining higher bits indicate splice type, if any:
// Dnr  Acc
// 00   00     no dnr/acc
// 01   01     GT/AG
// 10   10     GC/AG
// 11   11     AT/AC 

const unsigned char kMaskFc    = 0x01;
const unsigned char kMaskEc    = 0x02;
const unsigned char kMaskE     = 0x04;
const unsigned char kMaskD     = 0x08;

CNWAligner::TScore CNWAlignerMrna2Dna::Run()
{
    x_LoadScoringMatrix();

    const size_t N1 = m_SeqLen1 + 1;
    const size_t N2 = m_SeqLen2 + 1;

    TScore* rowV    = new TScore [N2];
    TScore* rowF    = new TScore [N2];

    // index calculation: [i,j] = i*n2 + j
    unsigned char* backtrace_matrix = new unsigned char [N1*N2];

    TScore* pV = rowV - 1;

    const char* seq1   = m_Seq1 - 1;
    const char* seq2   = m_Seq2 - 1;

    bool bFreeGapLeft1  = m_esf_L1;
    bool bFreeGapRight1 = m_esf_R1;
    bool bFreeGapLeft2  = m_esf_L2;
    bool bFreeGapRight2 = m_esf_R2;

    TScore wgleft1   = bFreeGapLeft1? 0: m_Wg;
    TScore wsleft1   = bFreeGapLeft1? 0: m_Ws;
    TScore wg1 = m_Wg, ws1 = m_Ws;

    // first row
    rowV[0] = wgleft1;
    size_t k;
    for (k = 1; k < N2; k++) {
        rowV[k] = pV[k] + wsleft1;
        rowF[k] = kInfMinus;
        backtrace_matrix[k] = kMaskE | kMaskEc;
    }
    rowV[0] = 0;

    // recurrences
    TScore wgleft2   = bFreeGapLeft2? 0: m_Wg;
    TScore wsleft2   = bFreeGapLeft2? 0: m_Ws;
    TScore V  = 0;
    TScore V0 = wgleft2;
    TScore E, G, n0;
    unsigned char tracer;

    size_t* jAllDonors [splice_type_count];
    TScore* vAllDonors [splice_type_count];
    for(unsigned char st = 0; st < splice_type_count; ++st) {
        jAllDonors[st] = new size_t [N2];
        vAllDonors[st] = new TScore [N2];
    }
    size_t  jTail[splice_type_count], jHead[splice_type_count];
    TScore  vBestDonor [splice_type_count];
    size_t  jBestDonor [splice_type_count];

    size_t i, j, k0;
    char ci;
    for(i = 1;  i < N1;  ++i) {
        
        V = V0 += wsleft2;
        E = kInfMinus;
        k0 = k;
        backtrace_matrix[k++] = kMaskFc;
        ci = seq1[i];

        for(unsigned char st = 0; st < splice_type_count; ++st) {
            jTail[st] = jHead[st] = 0;
            vBestDonor[st] = kInfMinus;
        }

        if(i == N1 - 1 && bFreeGapRight1) {
                wg1 = ws1 = 0;
        }

        TScore wg2 = m_Wg, ws2 = m_Ws;

        for (j = 1; j < N2; ++j, ++k) {
            
            G = pV[j] + m_Matrix[ci][seq2[j]];
            pV[j] = V;

            n0 = V + wg1;
            if(E > n0) {
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
            if(rowF[j] > n0) {
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
            for(unsigned char st = 0; st < splice_type_count; ++st) {

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
            unsigned char value = 0xFF;
            unsigned char tracer_splice = 0;
            for(unsigned char st = 0; st < splice_type_count; ++st) {

                if(seq2[j-1] == acceptor[st][0] && seq2[j] == acceptor[st][1]
                   && i < N1 - 1 && vBestDonor[st] > kInfMinus) {
                    TScore vAcc = vBestDonor[st] + m_Wi[st];
                    if(vAcc > V) {
                        V = vAcc;
                        tracer_splice = (st+1) << 4;
                        dnr_pos = k0 + jBestDonor[st];
                        value = (st+1) << 6;
                    }
                }
            }
            if(value != 0xFF) {
                backtrace_matrix[dnr_pos] |= value;
            }
            
            backtrace_matrix[k] = tracer | tracer_splice;
            
            // detect donor candidate
            for(unsigned char st = 0; st < splice_type_count; ++st) {
                if( j < N2 - 2 && i < N1 - 1 &&
                    seq2[j+1] == donor[st][0] && seq2[j+2] == donor[st][1] ) {
                    jAllDonors[st][jTail[st]] = j;
                    vAllDonors[st][jTail[st]] = V;
                    ++(jTail[st]);
                }
            }
        }

        pV[j] = V;

    }

    for(unsigned char st = 0; st < splice_type_count; ++st) {
        delete[] jAllDonors[st];
        delete[] vAllDonors[st];
    }
    delete[] rowV;
    delete[] rowF;

    x_DoBackTrace(backtrace_matrix);

    delete[] backtrace_matrix;

    return V;
}


// perform backtrace step;
// create transcript in member variable
void CNWAlignerMrna2Dna::x_DoBackTrace(const unsigned char* backtrace)
{
    const size_t N1 = m_SeqLen1 + 1;
    const size_t N2 = m_SeqLen2 + 1;

    // backtrace
    m_Transcript.clear();
    m_Transcript.reserve(N1 + N2);

    size_t k = N1*N2 - 1;
    while (k != 0) {
        unsigned char Key = backtrace[k];

        while(Key & 0x30) {  // detect acceptor
            unsigned char acc_type = (Key & 0x30) >> 4, dnr_type = ~acc_type;
            for( ; acc_type != dnr_type; dnr_type = (Key & 0xC0) >> 6 ) {
                Key = 0; // make sure we enter the loop
                while(!(Key & 0xC0)) {
                    m_Transcript.push_back(eIntron);
                    Key = backtrace[--k];
                }
            }
            if(Key & 0x30) {
                NCBI_THROW(CNWAlignerException,
                           eInternal,
                           "Adjacent splices encountered during backtrace");
            }
        }
        
        if (Key & kMaskD) {
            m_Transcript.push_back(eMatch);  // or eReplace
            k -= N2 + 1;
        }
        else if (Key & kMaskE) {
            m_Transcript.push_back(eInsert); --k;
            while(k > 0 && (Key & kMaskEc)) {
                m_Transcript.push_back(eInsert);
                Key = backtrace[k--];
            }
        }
        else {
            m_Transcript.push_back(eDelete);
            k -= N2;
            while(k > 0 && (Key & kMaskFc)) {
                m_Transcript.push_back(eDelete);
                Key = backtrace[k];
                k -= N2;
            }
        }
    }
}


CNWAligner::TScore CNWAlignerMrna2Dna::x_ScoreByTranscript() const
    throw (CNWAlignerException)
{
    const size_t dim = m_Transcript.size();
    vector<ETranscriptSymbol> transcript (dim);
    for(size_t i = 0; i < dim; ++i) {
        transcript[i] = m_Transcript[dim - i - 1];
    }

    TScore score = 0;

    const char* p1 = m_Seq1;
    const char* p2 = m_Seq2;

    char state1;   // 0 = normal, 1 = gap, 2 = intron
    char state2;   // 0 = normal, 1 = gap

    switch(transcript[0]) {
    case eMatch:    state1 = state2 = 0; break;
    case eInsert:   state1 = 1; state2 = 0; score += m_Wg; break;
    case eDelete:   state1 = 0; state2 = 1; score += m_Wg; break;
    default: {
        NCBI_THROW(
                   CNWAlignerException,
                   eInternal,
                   "Invalid transcript symbol");
        }
    }

    TScore L1 = 0, R1 = 0, L2 = 0, R2 = 0;
    bool   bL1 = false, bR1 = false, bL2 = false, bR2 = false;
    
    for(size_t i = 0; i < dim; ++i) {

        char c1 = *p1;
        char c2 = *p2;
        switch(transcript[i]) {

        case eMatch: {
            state1 = state2 = 0;
            score += m_Matrix[c1][c2];
            ++p1; ++p2;
        }
        break;

        case eInsert: {
            if(state1 != 1) score += m_Wg;
            state1 = 1; state2 = 0;
            score += m_Ws;
            ++p2;
        }
        break;

        case eDelete: {
            if(state2 != 1) score += m_Wg;
            state1 = 0; state2 = 1;
            score += m_Ws;
            ++p1;
        }
        break;

        case eIntron: {
            if(state1 != 2) {
                for(unsigned char i = 0; i < splice_type_count; ++i) {
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
                   CNWAlignerException,
                   eInternal,
                   "Invalid transcript symbol");
        }
        }
    }

    if(m_esf_L1) {
        size_t g = 0;
        for(size_t i = 0; i < dim; ++i) {
            if(transcript[i] == eInsert) ++g; else break;
        }
        if(g > 0) {
            score -= (m_Wg + g*m_Ws);
        }
    }

    if(m_esf_L2) {
        size_t g = 0;
        for(size_t i = 0; i < dim; ++i) {
            if(transcript[i] == eDelete) ++g; else break;
        }
        if(g > 0) {
            score -= (m_Wg + g*m_Ws);
        }
    }

    if(m_esf_R1) {
        size_t g = 0;
        for(int i = dim - 1; i >= 0; --i) {
            if(transcript[i] == eInsert) ++g; else break;
        }
        if(g > 0) {
            score -= (m_Wg + g*m_Ws);
        }
    }

    if(m_esf_R2) {
        size_t g = 0;
        for(int i = dim - 1; i >= 0; --i) {
            if(transcript[i] == eDelete) ++g; else break;
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
 * Revision 1.11  2003/03/31 15:32:05  kapustin
 * Calculate score independently from transcript
 *
 * Revision 1.10  2003/03/25 22:06:01  kapustin
 * Support non-canonical splice signals
 *
 * Revision 1.9  2003/03/18 15:15:51  kapustin
 * Implement virtual memory checking function. Allow separate free end
 * gap specification
 *
 * Revision 1.8  2003/03/17 13:42:24  kapustin
 * Make donor/acceptor characters uppercase
 *
 * Revision 1.7  2003/02/11 16:06:54  kapustin
 * Add end-space free alignment support
 *
 * Revision 1.6  2003/01/30 20:32:06  kapustin
 * Call x_LoadScoringMatrix() from the base class constructor.
 *
 * Revision 1.5  2003/01/21 12:41:38  kapustin
 * Use class neg infinity constant to specify least possible score
 *
 * Revision 1.4  2003/01/08 15:42:59  kapustin
 * Fix initialization for the first column of the backtrace matrix
 *
 * Revision 1.3  2002/12/24 18:29:06  kapustin
 * Remove sequence size verification since a part of Dna could be submitted
 *
 * Revision 1.2  2002/12/17 21:50:05  kapustin
 * Remove unnecesary seq type parameter from the constructor
 *
 * Revision 1.1  2002/12/12 17:57:41  kapustin
 * Initial revision
 *
 * ===========================================================================
 */
