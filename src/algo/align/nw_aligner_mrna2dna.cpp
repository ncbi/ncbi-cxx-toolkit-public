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

CNWAlignerMrna2Dna::CNWAlignerMrna2Dna(const char* seq1, size_t len1,
                                       const char* seq2, size_t len2)
    throw(CNWAlignerException)
    : CNWAligner(seq1, len1, seq2, len2, eNucl), 
    m_Wi(GetDefaultWi()),  
    m_IntronMinSize(GetDefaultIntronMinSize())
{
    // the shorter sequence is assumed to be mRna
    if(len2 < len1) {
        NCBI_THROW(
                   CNWAlignerException,
                   eIncorrectSequenceOrder,
                   "mRna must be the first of two sequences");
    }
}


// evaluate score for each possible alignment;
// fill out backtrace matrix
// bit coding (six bits per value): D E Ec Fc Acc Dnr
// D:    1 if diagonal; 0 - otherwise
// E:    1 if space in 1st sequence; 0 if space in 2nd sequence
// Ec:   1 if gap in 1st sequence was extended; 0 if it is was opened
// Fc:   1 if gap in 2nd sequence was extended; 0 if it is was opened
// Acc:  1 if acceptor; 0 - otherwise
// Dnr:  1 if the best donor (so far from left to right); 0 otherwise

const unsigned char kMaskFc    = 1;
const unsigned char kMaskEc    = kMaskFc << 1;
const unsigned char kMaskE     = kMaskFc << 2;
const unsigned char kMaskD     = kMaskFc << 3;
const unsigned char kMaskAcc   = kMaskFc << 4;
const unsigned char kMaskDnr   = kMaskFc << 5;

int CNWAlignerMrna2Dna::Run()
{
    x_LoadMatrix();

    const int N1 = m_SeqLen1 + 1;
    const int N2 = m_SeqLen2 + 1;

    TScore* rowV    = new TScore [N2];
    TScore* rowF    = new TScore [N2];

    // index calculation: [i,j] = i*n2 + j
    unsigned char* backtrace_matrix = new unsigned char [N1*N2];

    TScore* pV = rowV - 1;

    const char* seq1   = m_Seq1 - 1;
    const char* seq2   = m_Seq2 - 1;

    bool bFreeGapLeft1  = true;
    bool bFreeGapRight1 = true;
    bool bFreeGapLeft2  = true;
    bool bFreeGapRight2 = true;

    TScore wgleft1   = bFreeGapLeft1? 0: m_Wg;
    TScore wsleft1   = bFreeGapLeft1? 0: m_Ws;
    TScore wg1 = m_Wg, ws1 = m_Ws;

    // first row
    rowV[0] = wgleft1;
    int k;
    for (k = 1; k < N2; k++) {
        rowV[k] = pV[k] + wsleft1;
        rowF[k] = kMin_Int;
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

    int*    jAllDonors = new int [N2];
    TScore* vAllDonors = new TScore [N2];
    int     jTail, jHead;
    TScore  vBestDonor;

    int i, j, k0;
    char ci;
    for(i = 1;  i < N1;  ++i) {
        
        V = V0 += wsleft2;
        E = kMin_Int;
        k0 = k;
        backtrace_matrix[++k] = kMaskFc;
        ci = seq1[i];

        jTail = jHead = 0;
        vBestDonor = kMin_Int;

        if(i == N1 - 1) {
            if(bFreeGapRight1) {
                wg1 = ws1 = 0;
            }
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

            // check the best donor
            if(jTail > jHead)  {
                if(j - jAllDonors[jHead] >= m_IntronMinSize) {
                    if(vAllDonors[jHead] > vBestDonor) {
                        vBestDonor = vAllDonors[jHead];
                        backtrace_matrix[k0 + jAllDonors[jHead]] |= kMaskDnr;
                    }
                    ++jHead;
                }
            }

            if(seq2[j-1] == 'a' && seq2[j] == 'g' // acceptor
               && i < N1 - 1 && vBestDonor > kMin_Int) {
                TScore vAcc = vBestDonor + m_Wi;
                if(vAcc > V) {
                    V = vAcc;
                    tracer = kMaskAcc;
                }
            }

            backtrace_matrix[k] = tracer;
            
            // detect donor candidate
            if( j < N2 - 2 && i < N1 - 1 &&
                seq2[j+1] == 'g' && seq2[j+2] == 't' )  {

                jAllDonors[jTail] = j;
                vAllDonors[jTail++] = V;
            }
            
        }

        pV[j] = V;
    }
    
    x_DoBackTrace(backtrace_matrix);

    delete[] vAllDonors;
    delete[] jAllDonors;
    delete[] backtrace_matrix;
    delete[] rowV;
    delete[] rowF;

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

        if(Key & kMaskAcc) {  // insert intron
            while(!(Key & kMaskDnr)) {
                m_Transcript.push_back(eIntron);
                Key = backtrace[--k];
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


END_NCBI_SCOPE



/*
 * ===========================================================================
 * $Log$
 * Revision 1.2  2002/12/17 21:50:05  kapustin
 * Remove unnecesary seq type parameter from the constructor
 *
 * Revision 1.1  2002/12/12 17:57:41  kapustin
 * Initial revision
 *
 * ===========================================================================
 */
