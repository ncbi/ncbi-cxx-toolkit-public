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
 * Authors:  Yuri Kapustin, Boris Kiryutin
 *
 * File Description:  CNWAligner implementation
 *                   
 * ===========================================================================
 *
 */


#include <algo/nw_aligner.hpp>
#include <corelib/ncbi_limits.h>

BEGIN_NCBI_SCOPE

static const char nucleotides [] = 
         { 'a', 'g', 't', 'c', 'n' };

static const char aminoacids [] = 
         { 'a', 'r', 'n', 'd', 'c', 'q', 'e', 'g', 'h', 'i', 'l', 'k', 'm',
           'f', 'p', 's', 't', 'w', 'y', 'v', 'b', 'z', 'x' };

static const size_t kBlosumSize = sizeof aminoacids;
static const char matrix_blosum62[kBlosumSize][kBlosumSize] = {
    /*       A,  R,  N,  D,  C,  Q,  E,  G,  H,  I,  L,  K,  M,  F,  P,  S,  T,  W,  Y,  V,  B,  Z,  X */
    /*A*/ {  4, -1, -2, -2,  0, -1, -1,  0, -2, -1, -1, -1, -1, -2, -1,  1,  0, -3, -2,  0, -2, -1,  0 },
    /*R*/ { -1,  5,  0, -2, -3,  1,  0, -2,  0, -3, -2,  2, -1, -3, -2, -1, -1, -3, -2, -3, -1,  0, -1 },
    /*N*/ { -2,  0,  6,  1, -3,  0,  0,  0,  1, -3, -3,  0, -2, -3, -2,  1,  0, -4, -2, -3,  3,  0, -1 },
    /*D*/ { -2, -2,  1,  6, -3,  0,  2, -1, -1, -3, -4, -1, -3, -3, -1,  0, -1, -4, -3, -3,  4,  1, -1 },
    /*C*/ {  0, -3, -3, -3,  9, -3, -4, -3, -3, -1, -1, -3, -1, -2, -3, -1, -1, -2, -2, -1, -3, -3, -2 },
    /*Q*/ { -1,  1,  0,  0, -3,  5,  2, -2,  0, -3, -2,  1,  0, -3, -1,  0, -1, -2, -1, -2,  0,  3, -1 },
    /*E*/ { -1,  0,  0,  2, -4,  2,  5, -2,  0, -3, -3,  1, -2, -3, -1,  0, -1, -3, -2, -2,  1,  4, -1 },
    /*G*/ {  0, -2,  0, -1, -3, -2, -2,  6, -2, -4, -4, -2, -3, -3, -2,  0, -2, -2, -3, -3, -1, -2, -1 },
    /*H*/ { -2,  0,  1, -1, -3,  0,  0, -2,  8, -3, -3, -1, -2, -1, -2, -1, -2, -2,  2, -3,  0,  0, -1 },
    /*I*/ { -1, -3, -3, -3, -1, -3, -3, -4, -3,  4,  2, -3,  1,  0, -3, -2, -1, -3, -1,  3, -3, -3, -1 },
    /*L*/ { -1, -2, -3, -4, -1, -2, -3, -4, -3,  2,  4, -2,  2,  0, -3, -2, -1, -2, -1,  1, -4, -3, -1 },
    /*K*/ { -1,  2,  0, -1, -3,  1,  1, -2, -1, -3, -2,  5, -1, -3, -1,  0, -1, -3, -2, -2,  0,  1, -1 },
    /*M*/ { -1, -1, -2, -3, -1,  0, -2, -3, -2,  1,  2, -1,  5,  0, -2, -1, -1, -1, -1,  1, -3, -1, -1 },
    /*F*/ { -2, -3, -3, -3, -2, -3, -3, -3, -1,  0,  0, -3,  0,  6, -4, -2, -2,  1,  3, -1, -3, -3, -1 },
    /*P*/ { -1, -2, -2, -1, -3, -1, -1, -2, -2, -3, -3, -1, -2, -4,  7, -1, -1, -4, -3, -2, -2, -1, -2 },
    /*S*/ {  1, -1,  1,  0, -1,  0,  0,  0, -1, -2, -2,  0, -1, -2, -1,  4,  1, -3, -2, -2,  0,  0,  0 },
    /*T*/ {  0, -1,  0, -1, -1, -1, -1, -2, -2, -1, -1, -1, -1, -2, -1,  1,  5, -2, -2,  0, -1, -1,  0 },
    /*W*/ { -3, -3, -4, -4, -2, -2, -3, -2, -2, -3, -2, -3, -1,  1, -4, -3, -2, 11,  2, -3, -4, -3, -2 },
    /*Y*/ { -2, -2, -2, -3, -2, -1, -2, -3,  2, -1, -1, -2, -1,  3, -3, -2, -2,  2,  7, -1, -3, -2, -1 },
    /*V*/ {  0, -3, -3, -3, -1, -2, -2, -3, -3,  3,  1, -2,  1, -1, -2, -2,  0, -3, -1,  4, -3, -2, -1 },
    /*B*/ { -2, -1,  3,  4, -3,  0,  1, -1,  0, -3, -4,  0, -3, -3, -2,  0, -1, -4, -3, -3,  4,  1, -1 },
    /*Z*/ { -1,  0,  0,  1, -3,  3,  4, -2,  0, -3, -3,  1, -1, -3, -1,  0, -1, -3, -2, -2,  1,  4, -1 },
    /*X*/ {  0, -1, -1, -1, -2, -1, -1, -1, -1, -1, -1, -1, -1, -1, -2,  0,  0, -2, -1, -1, -1, -1, -1 }
};


CNWAligner::CNWAligner(const char* seq1, size_t len1,
                       const char* seq2, size_t len2,
                       EScoringMatrixType matrix_type)
    throw(CNWAlignerException)
    : m_Wm(GetDefaultWm()),
      m_Wms(GetDefaultWms()),
      m_Wg(GetDefaultWg()),
      m_Ws(GetDefaultWs()),
      m_Seq1(seq1),  m_SeqLen1(len1),
      m_Seq2(seq2),  m_SeqLen2(len2),
      m_MatrixType(matrix_type)
{
    if(!seq1 || !seq2)
        NCBI_THROW(
                   CNWAlignerException,
                   eBadParameter,
                   "NULL sequence pointer(s) passed");

    if(!len1 || !len2)
        NCBI_THROW(
                   CNWAlignerException,
                   eBadParameter,
                   "Zero length specified for sequence(s)");

    size_t iErrPos1 = x_CheckSequence(seq1, len1);
    if(iErrPos1 < len1) {
        ostrstream oss;
        oss << "The first sequence is inconsistent with the current "
            << "scoring matrix type. Symbol " << seq1[iErrPos1] << " at "
            << iErrPos1;
        string message = CNcbiOstrstreamToString(oss);
        NCBI_THROW(
                   CNWAlignerException,
                   eInvalidCharacter,
                   message );
    }
    size_t iErrPos2 = x_CheckSequence(seq2, len2);
    if(iErrPos2 < len2) {
        ostrstream oss;
        oss << "The second sequence is inconsistent with the current "
            << "scoring matrix type. Symbol " << seq2[iErrPos2] << " at "
            << iErrPos1;
        string message = CNcbiOstrstreamToString(oss);
        NCBI_THROW(
                   CNWAlignerException,
                   eInvalidCharacter,
                   message );
    }
}


// evaluate score for each possible alignment;
// fill out backtrace matrix
// bit coding (four bits per value): D E Ec Fc
// D:  1 if diagonal; 0 - otherwise
// E:  1 if space in 1st sequence; 0 if space in 2nd sequence
// Ec: 1 if gap in 1st sequence was extended; 0 if it is was opened
// Fc: 1 if gap in 2nd sequence was extended; 0 if it is was opened
//

const unsigned char kMaskFc  = 0x0001;
const unsigned char kMaskEc  = 0x0002;
const unsigned char kMaskE   = 0x0004;
const unsigned char kMaskD   = 0x0008;

int CNWAligner::Run()
{
    x_LoadScoringMatrix();

    const int N1 = m_SeqLen1 + 1;
    const int N2 = m_SeqLen2 + 1;

    TScore* rowV    = new TScore [N2];
    TScore* rowF    = new TScore [N2];

    // index calculation: [i,j] = i*n2 + j
    unsigned char* backtrace_matrix = new unsigned char [N1*N2];

    TScore* pV = rowV - 1;

    const char* seq1 = m_Seq1 - 1;
    const char* seq2 = m_Seq2 - 1;

    // first row
    rowV[0] = m_Wg;
    int k;
    for (k = 1; k < N2; k++) {
        rowV[k] = pV[k] + m_Ws;
        rowF[k] = kInfMinus;
        backtrace_matrix[k] = kMaskE | kMaskEc;
    }
    rowV[0] = 0;

    // recurrences
    TScore V  = 0;
    TScore V0 = m_Wg;
    TScore E, G, n0;
    unsigned char tracer;

    int i, j;
    for(i = 1;  i < N1;  ++i) {
        
        V = V0 += m_Ws;
        E = kInfMinus;
        backtrace_matrix[k++] = kMaskFc;
        char ci = seq1[i];

        for (j = 1; j < N2; ++j, ++k) {

            G = pV[j] + m_Matrix[ci][seq2[j]];
            pV[j] = V;

            n0 = V + m_Wg;
            if(E > n0) {
                E += m_Ws;      // continue the gap
                tracer = kMaskEc;
            }
            else {
                E = n0 + m_Ws;  // open a new gap
                tracer = 0;
            }

            n0 = rowV[j] + m_Wg;
            if(rowF[j] > n0) {
                rowF[j] += m_Ws;
                tracer |= kMaskFc;
            }
            else {
                rowF[j] = n0 + m_Ws;
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
    }
    
    x_DoBackTrace(backtrace_matrix);

    delete[] backtrace_matrix;
    delete[] rowV;
    delete[] rowF;

    return V;
}


// perform backtrace step;
// create transcript in member variable

void CNWAligner::x_DoBackTrace(const unsigned char* backtrace)
{
    const size_t N1 = m_SeqLen1 + 1;
    const size_t N2 = m_SeqLen2 + 1;

    // backtrace
    m_Transcript.clear();
    m_Transcript.reserve(N1 + N2);

    size_t k = N1*N2 - 1;
    while (k != 0) {
        unsigned char Key = backtrace[k];
        if (Key & kMaskD) {
            m_Transcript.push_back( eMatch );
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


// creates formatted output of alignment;
// requires prior call to Run
string CNWAligner::Format(size_t line_width) const
{
    vector<char> v1, v2;

    size_t aln_size = x_ApplyTranscript(&v1, &v2);
    unsigned i1 = 0, i2 = 0;
    ostrstream ss;
    for (size_t i = 0;  i < aln_size; ) {
        ss << i << '\t' << i1 << ':' << i2 << endl;
        int i0 = i;
        for (size_t jPos = 0;  i < aln_size  &&  jPos < line_width;
             ++i, ++jPos) {
            char c = v1[i0 + jPos];
            ss << c;
            if(c != '-'  &&  c != '+')
                i1++;
        }
        ss << endl;

        string marker_line(line_width, ' ');
        i = i0;
        for (size_t jPos = 0;  i < aln_size  &&  jPos < line_width;
             ++i, ++jPos) {
            char c1 = v1[i0 + jPos];
            char c  = v2[i0 + jPos];
            ss << c;
            if(c != '-' && c != '+')
                i2++;
            if(c != c1  &&  c != '-'  &&  c1 != '-'  &&  c1 != '+')
                marker_line[jPos] = '^';
        }
        ss << endl << marker_line << endl;
    }

    return CNcbiOstrstreamToString(ss);
}


// Transform source sequences according to the transcript.
// Write the results to v1 and v2 leaving source sequences intact.
// Return alignment size.
size_t CNWAligner::x_ApplyTranscript(vector<char>* pv1, vector<char>* pv2)
    const
{
    vector<char>& v1 = *pv1;
    vector<char>& v2 = *pv2;

    vector<ETranscriptSymbol>::const_reverse_iterator
        ib = m_Transcript.rbegin(),
        ie = m_Transcript.rend(),
        ii;

    const char* iv1 = m_Seq1;
    const char* iv2 = m_Seq2;
    v1.clear();
    v2.clear();

    for (ii = ib;  ii != ie;  ii++) {
        ETranscriptSymbol ts = *ii;
        char c1, c2;
        switch ( ts ) {
        case eInsert:
            c1 = '-';
            c2 = *iv2++;
            break;
        case eDelete:
            c2 = '-';
            c1 = *iv1++;
            break;
        case eMatch:
        case eReplace:
            c1 = *iv1++;
            c2 = *iv2++;
            break;
        case eIntron:
            c1 = '+';
            c2 = *iv2++;
            break;
        default:
            c1 = c2 = '?';
            break;
        }
        v1.push_back(c1);
        v2.push_back(c2);
    }

    return v1.size();
}


// Return transcript as a readable string
string CNWAligner::GetTranscript() const
{
    string s;
    size_t size = m_Transcript.size();
    for (size_t i = 0;  i < size;  i++) {
        ETranscriptSymbol c0 = m_Transcript[i];
        char c;
        switch ( c0 ) {
        case eInsert:  c = 'I';  break;
        case eMatch:   c = 'M';  break;
        case eReplace: c = 'R';  break;
        case eDelete:  c = 'D';  break;
        case eIntron:  c = '+';  break;
        default:       c = '?';  break;
        }
        s += c;
    }
    reverse(s.begin(), s.end());
    return s;
}


// Load pairwise scoring matrix
void CNWAligner::x_LoadScoringMatrix()
{
    switch(m_MatrixType) {

    case eNucl: {
            char c1, c2;
            const size_t kNuclSize = sizeof nucleotides;
            for(size_t i = 0; i < kNuclSize; ++i) {
                c1 = nucleotides[i];
                for(size_t j = 0; j < i; ++j) {
                    c2 = nucleotides[j];
                    m_Matrix[c1][c2] = m_Matrix[c2][c1] =
                        m_Wms;
                }
                m_Matrix[c1][c1] = m_Wm;
            }
            m_Matrix['n']['n'] = m_Wms;
        }
        break;

    case eBlosum62: {
            char c1, c2;
            for(size_t i = 0; i < kBlosumSize; ++i) {
                c1 = aminoacids[i];
                for(size_t j = 0; j < i; ++j) {
                    c2 = aminoacids[j];
                    m_Matrix[c1][c2] = m_Matrix[c2][c1] =
                        matrix_blosum62[i][j];
                }
                m_Matrix[c1][c1] = matrix_blosum62[i][i];
            }
        }
        break;
    };
}

// Check that all characters in sequence are valid for 
// the current sequence type.
// Return an index to the first invalid character
// or len if all characters are valid.
size_t CNWAligner::x_CheckSequence(const char* seq, size_t len) const
{
    char Flags [256];
    memset(Flags, 0, sizeof Flags);

    const char* pabc = 0;
    size_t abc_size = 0;
    switch(m_MatrixType)
    {
    case eNucl:
        pabc = nucleotides;
        abc_size = sizeof nucleotides;
        break;
    case eBlosum62:
        pabc = aminoacids;
        abc_size = sizeof aminoacids;
        break;
    }
    
    size_t k;
    for(k = 0; k < abc_size; ++k) {
        Flags[pabc[k]] = 1;
    }

    for(k = 0; k < len; ++k) {
        if(Flags[seq[k]] == 0)
            break;
    }

    return k;
}


END_NCBI_SCOPE


/*
 * ===========================================================================
 * $Log$
 * Revision 1.6  2003/01/21 12:41:37  kapustin
 * Use class neg infinity constant to specify least possible score
 *
 * Revision 1.5  2003/01/08 15:42:59  kapustin
 * Fix initialization for the first column of the backtrace matrix
 *
 * Revision 1.4  2003/01/06 18:04:10  kapustin
 * CNWAligner: add sequence size verification
 *
 * Revision 1.3  2002/12/31 13:53:13  kapustin
 * CNWAligner::Format() -- use CNcbiOstrstreamToString
 *
 * Revision 1.2  2002/12/12 17:59:28  kapustin
 * Enable spliced alignments
 *
 * Revision 1.1  2002/12/06 17:41:21  ivanov
 * Initial revision
 *
 * ===========================================================================
 */
