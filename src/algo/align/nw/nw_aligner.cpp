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


#include <algo/align/nw_aligner.hpp>
#include <corelib/ncbi_limits.h>
#include <objects/seqalign/Score.hpp>
#include <objects/general/Object_id.hpp>
#include <objects/seqalign/Dense_seg.hpp>
#include <objects/seqloc/Seq_id.hpp>
#include <serial/objostrasn.hpp>
#include <serial/serial.hpp>

BEGIN_NCBI_SCOPE


// Nucleotides: IUPACna coding
static const char nucleotides [] = 
{ 
    // base set
    'A', 'G', 'T', 'C',

    // extension: ambiguity characters
    'B', 'D', 'H', 'K', 
    'M', 'N', 'R', 'S', 
    'V', 'W', 'Y'
};


// Aminoacids: IUPACaa
static const char aminoacids [] = 
{ 'A', 'R', 'N', 'D', 'C',
  'Q', 'E', 'G', 'H', 'I',
  'L', 'K', 'M', 'F', 'P',
  'S', 'T', 'W', 'Y', 'V',
  'B', 'Z', 'X'
};


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


CNWAligner::CNWAligner()
    : m_Wm(GetDefaultWm()),
      m_Wms(GetDefaultWms()),
      m_Wg(GetDefaultWg()),
      m_Ws(GetDefaultWs()),
      m_esf_L1(false), m_esf_R1(false), m_esf_L2(false), m_esf_R2(false),
      m_MatrixType(eSMT_None),
      m_prg_callback(0),
      m_Seq1(0), m_SeqLen1(0),
      m_Seq2(0), m_SeqLen2(0),
      m_score(kInfMinus)
{
}


CNWAligner::CNWAligner( const char* seq1, size_t len1,
                        const char* seq2, size_t len2,
                        EScoringMatrixType matrix_type )
    throw(CAlgoAlignException)
    : m_Wm(GetDefaultWm()),
      m_Wms(GetDefaultWms()),
      m_Wg(GetDefaultWg()),
      m_Ws(GetDefaultWs()),
      m_esf_L1(false), m_esf_R1(false), m_esf_L2(false), m_esf_R2(false),
      m_MatrixType(matrix_type),
      m_prg_callback(0),
      m_Seq1(seq1), m_SeqLen1(len1),
      m_Seq2(seq2), m_SeqLen2(len2),
      m_score(kInfMinus)
{
    x_LoadScoringMatrix();
    SetSequences(seq1, len1, seq2, len2);
}


void CNWAligner::SetMatrixType(EScoringMatrixType matrix_type)
        throw(CAlgoAlignException)
{
    m_MatrixType = matrix_type;
    x_LoadScoringMatrix();
}


void CNWAligner::SetSequences(const char* seq1, size_t len1,
			      const char* seq2, size_t len2,
			      bool verify)
    throw(CAlgoAlignException)
{
    if(m_MatrixType == eSMT_None) {
        NCBI_THROW(
                   CAlgoAlignException,
                   eMatrixTypeNotSet,
                   "Scoring matrix type not set");
    }

    if(!seq1 && len1 || !seq2 && len2) {
        NCBI_THROW(
                   CAlgoAlignException,
                   eBadParameter,
                   "NULL sequence pointer(s) passed");
    }

    if(verify) {
        size_t iErrPos1 = x_CheckSequence(seq1, len1);
	if(iErrPos1 < len1) {
	    ostrstream oss;
	    oss << "The first sequence is inconsistent with the current "
		<< "scoring matrix type. Symbol " << seq1[iErrPos1] << " at "
		<< iErrPos1;
	    string message = CNcbiOstrstreamToString(oss);
	    NCBI_THROW(
		       CAlgoAlignException,
		       eInvalidCharacter,
		       message );
	}

	size_t iErrPos2 = x_CheckSequence(seq2, len2);
	if(iErrPos2 < len2) {
	    ostrstream oss;
	    oss << "The second sequence is inconsistent with the current "
		<< "scoring matrix type. Symbol " << seq2[iErrPos2] << " at "
		<< iErrPos2;
	    string message = CNcbiOstrstreamToString(oss);
	    NCBI_THROW (
			CAlgoAlignException,
			eInvalidCharacter,
			message );
	}
    }
    m_Seq1 = seq1;
    m_SeqLen1 = len1;
    m_Seq2 = seq2;
    m_SeqLen2 = len2;
}



void CNWAligner::SetEndSpaceFree(bool Left1, bool Right1,
                                 bool Left2, bool Right2)
{
    m_esf_L1 = Left1;
    m_esf_R1 = Right1;
    m_esf_L2 = Left2;
    m_esf_R2 = Right2;
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

CNWAligner::TScore CNWAligner::x_Align(const char* seg1, size_t len1,
                                       const char* seg2, size_t len2,
                                       vector<ETranscriptSymbol>* transcript)
{
    const size_t N1 = len1 + 1;
    const size_t N2 = len2 + 1;

    TScore* rowV    = new TScore [N2];
    TScore* rowF    = new TScore [N2];

    TScore* pV = rowV - 1;

    const char* seq1 = seg1 - 1;
    const char* seq2 = seg2 - 1;

    m_terminate = false;

    if(m_prg_callback) {
        m_prg_info.m_iter_total = N1*N2;
        m_prg_info.m_iter_done = 0;
        if(m_terminate = m_prg_callback(&m_prg_info)) {
	  return 0;
	}
    }

    bool bFreeGapLeft1  = m_esf_L1 && seg1 == m_Seq1;
    bool bFreeGapRight1 = m_esf_R1 && m_Seq1 + m_SeqLen1 - len1 == seg1;
    bool bFreeGapLeft2  = m_esf_L2 && seg2 == m_Seq2;
    bool bFreeGapRight2 = m_esf_R2 && m_Seq2 + m_SeqLen2 - len2 == seg2;

    TScore wgleft1   = bFreeGapLeft1? 0: m_Wg;
    TScore wsleft1   = bFreeGapLeft1? 0: m_Ws;
    TScore wg1 = m_Wg, ws1 = m_Ws;

    // index calculation: [i,j] = i*n2 + j
    unsigned char* backtrace_matrix = new unsigned char [N1*N2];

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
    TScore wgleft2   = bFreeGapLeft2? 0: m_Wg;
    TScore wsleft2   = bFreeGapLeft2? 0: m_Ws;
    TScore V  = rowV[N2 - 1];
    TScore V0 = wgleft2;
    TScore E, G, n0;
    unsigned char tracer;

    size_t i, j;
    for(i = 1;  i < N1 && !m_terminate;  ++i) {
        
        V = V0 += wsleft2;
        E = kInfMinus;
        backtrace_matrix[k++] = kMaskFc;
        char ci = seq1[i];

        if(i == N1 - 1 && bFreeGapRight1) {
                wg1 = ws1 = 0;
        }

        TScore wg2 = m_Wg, ws2 = m_Ws;

        for (j = 1; j < N2; ++j, ++k) {

            G = pV[j] + m_Matrix[ci][seq2[j]];
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
        x_DoBackTrace(backtrace_matrix, N1, N2, transcript);
    }

    delete[] backtrace_matrix;
    delete[] rowV;
    delete[] rowF;

    return V;
}


CNWAligner::TScore CNWAligner::Run()
{
    if(!x_CheckMemoryLimit()) {
        NCBI_THROW(
                   CAlgoAlignException,
                   eMemoryLimit,
                   "Memory limit exceeded");
    }

    if(m_MatrixType == eSMT_None) {

        NCBI_THROW(
                   CAlgoAlignException,
                   eMatrixTypeNotSet,
                   "Scoring matrix type not set");
    }

    m_score = x_Run();

    return m_score;
}


CNWAligner::TScore CNWAligner::x_Run()
{
    try {
        if(m_guides.size() == 0) {
	  m_score = x_Align(m_Seq1,m_SeqLen1, m_Seq2,m_SeqLen2, &m_Transcript);
	}
	else {
	    m_Transcript.clear();
	    // run the algorithm for every segment between hits
	    size_t guides_dim = m_guides.size() / 4;
	    size_t q1 = m_SeqLen1, q0, s1 = m_SeqLen2, s0;
	    vector<ETranscriptSymbol> trans;
	    for(size_t i = 4*guides_dim; i != 0; i -= 4) {
	      q0 = m_guides[i - 3] + 1;
	      s0 = m_guides[i - 1] + 1;
	      size_t dim_query = q1 - q0, dim_subj = s1 - s0;
	      x_Align(m_Seq1 + q0, dim_query, m_Seq2 + s0, dim_subj, &trans);
	      copy(trans.begin(), trans.end(), back_inserter(m_Transcript));
	      size_t dim_hit = m_guides[i - 3] - m_guides[i - 4] + 1;
	      for(size_t k = 0; k < dim_hit; ++k) {
                m_Transcript.push_back(eTS_Match);
	      }
	      q1 = m_guides[i - 4];
	      s1 = m_guides[i - 2];
	      trans.clear();
	    }
	    x_Align(m_Seq1, q1, m_Seq2, s1, &trans);
	    copy(trans.begin(), trans.end(), back_inserter(m_Transcript));
	    m_score = x_ScoreByTranscript();
	}
    }
    catch(bad_alloc& e) {
      
      NCBI_THROW(
		 CAlgoAlignException,
		 eMemoryLimit,
		 "Memory limit exceeded");
    }

    return m_score;
}


// perform backtrace step;
void CNWAligner::x_DoBackTrace(const unsigned char* backtrace,
                               size_t N1, size_t N2,
                               vector<ETranscriptSymbol>* transcript)
{
    transcript->clear();
    transcript->reserve(N1 + N2);

    size_t k = N1*N2 - 1;
    while (k != 0) {
        unsigned char Key = backtrace[k];
        if (Key & kMaskD) {
            transcript->push_back(eTS_Match);
            k -= N2 + 1;
        }
        else if (Key & kMaskE) {
            transcript->push_back(eTS_Insert); --k;
            while(k > 0 && (Key & kMaskEc)) {
                transcript->push_back(eTS_Insert);
                Key = backtrace[k--];
            }
        }
        else {
            transcript->push_back(eTS_Delete);
            k -= N2;
            while(k > 0 && (Key & kMaskFc)) {
                transcript->push_back(eTS_Delete);
                Key = backtrace[k];
                k -= N2;
            }
        }
    }
}


void  CNWAligner::SetGuides(const vector<size_t>& guides)
    throw (CAlgoAlignException)
{
    size_t dim = guides.size();
    const char* err = 0;
    if(dim % 4 == 0) {
        for(size_t i = 0; i < dim; i += 4) {

            if( guides[i] > guides[i+1] || guides[i+2] > guides[i+3] ) {
                err = "Guides should be specified in plus strand";
                break;
            }

            if(i > 4) {
                if(guides[i] <= guides[i-3] || guides[i+2] <= guides[i-2]){
                    err = "Guides coordinates must be sorted";
                    break;
                }
            }

            size_t dim1 = guides[i + 1] - guides[i];
            size_t dim2 = guides[i + 3] - guides[i + 2];
            if( dim1 != dim2) {
                err = "Guiding hits must have equal length on both sequences";
                break;
            }

            if(guides[i+1] >= m_SeqLen1 || guides[i+3] >= m_SeqLen2) {
                err = "One or more guiding hits are out of range";
                break;
            }
        }
    }
    else {
        err = "Guides vector must have a dimension multiple of four";
    }

    if(err) {
        NCBI_THROW( CAlgoAlignException, eBadParameter, err );
    }
    else {
        m_guides = guides;
    }
}


void CNWAligner::GetEndSpaceFree(bool* L1, bool* R1, bool* L2, bool* R2) const
{
    if(L1) *L1 = m_esf_L1;
    if(R1) *R1 = m_esf_R1;
    if(L2) *L2 = m_esf_L2;
    if(R2) *R2 = m_esf_R2;
}


// Return transcript as a readable string
void CNWAligner::GetTranscriptString(vector<char>* out) const
{
    const size_t dim = m_Transcript.size();   
    const size_t line_size = 100;
    const size_t lines_est = dim / line_size + 1;
    out->resize(dim + lines_est);
    size_t i1 = 0, i2 = 0, i = 0;

    for (int k = dim - 1; k >= 0;  --k) {

        ETranscriptSymbol c0 = m_Transcript[k];
        char c = 0;
        switch ( c0 ) {

            case eTS_Match: 
            case eTS_Replace: {
                c = (m_Seq1[i1++] == m_Seq2[i2++])? 'M': 'R';
            }
            break;

            case eTS_Insert: {
                c = 'I';
                ++i2;
            }
            break;

            case eTS_Delete: {
                c = 'D';
                ++i1;
            }
            break;

            case eTS_Intron: {
                c = '+';
                ++i2;
            }
            break;
        }

        (*out)[i++] = c;
        if((dim - k) % line_size == 0) {
            (*out)[i++] = '\n';
        }
    }
    if((*out)[i-1] != '\n') {
        (*out)[i] = '\n';
    }
    if(i < out->size()) {
        out->resize(i + 1);
    }
}


void CNWAligner::SetProgressCallback ( FProgressCallback prg_callback,
				       void* data )
{
    m_prg_callback = prg_callback;
    m_prg_info.m_data = data;
}


// Load pairwise scoring matrix
void CNWAligner::x_LoadScoringMatrix()
{
    switch(m_MatrixType) {

    case eSMT_Nucl: {
            char c1, c2;
            const size_t kNuclSize = sizeof nucleotides;
            for(size_t i = 0; i < kNuclSize; ++i) {
                c1 = nucleotides[i];
                for(size_t j = 0; j < i; ++j) {
                    c2 = nucleotides[j];
                    m_Matrix[c1][c2] = m_Matrix[c2][c1] =
                        m_Wms;
                }
                m_Matrix[c1][c1] = i < 4? m_Wm: m_Wms;
            }
        }
        break;

    case eSMT_Blosum62: {
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

    default: {

        NCBI_THROW(
                   CAlgoAlignException,
                   eMatrixTypeNotSet,
                   "Scoring matrix type not set");
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
    case eSMT_Nucl:
        pabc = nucleotides;
        abc_size = sizeof nucleotides;
        break;
    case eSMT_Blosum62:
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


bool CNWAligner::x_CheckMemoryLimit()
{
    size_t gdim = m_guides.size();
    if(gdim) {
        size_t dim1 = m_guides[0], dim2 = m_guides[2];
	const size_t elem_size = x_GetElemSize();
        if(double(dim1)*dim2*elem_size >= kMax_UInt) {
            return false;
        }
        for(size_t i = 4; i < gdim; i += 4) {
            size_t dim1 = m_guides[i] - m_guides[i-3] + 1;
            size_t dim2 = m_guides[i + 2] - m_guides[i-1] + 1;
            if(double(dim1)*dim2*elem_size >= kMax_UInt) {
                return false;
            }
        }
        dim1 = m_SeqLen1 - m_guides[gdim-3];
        dim2 = m_SeqLen2 - m_guides[gdim-1];
        if(double(dim1)*dim2*elem_size >= kMax_UInt) {
            return false;
        }

        return true;
    }
    else {
        return double(m_SeqLen1 + 1)*(m_SeqLen2 + 1) < kMax_UInt;
    }
}


CNWAligner::TScore CNWAligner::x_ScoreByTranscript() const
    throw (CAlgoAlignException)
{
    const size_t dim = m_Transcript.size();
    if(dim == 0) return 0;

    vector<ETranscriptSymbol> transcript (dim);
    for(size_t i = 0; i < dim; ++i) {
        transcript[i] = m_Transcript[dim - i - 1];
    }

    TScore score = 0;

    const char* p1 = m_Seq1;
    const char* p2 = m_Seq2;

    int state1;   // 0 = normal, 1 = gap, 2 = intron
    int state2;   // 0 = normal, 1 = gap

    switch(transcript[0]) {
    case eTS_Match:    state1 = state2 = 0; break;
    case eTS_Insert:   state1 = 1; state2 = 0; score += m_Wg; break;
    case eTS_Delete:   state1 = 0; state2 = 1; score += m_Wg; break;
    default: {
        NCBI_THROW(
                   CAlgoAlignException,
                   eInternal,
                   "Invalid transcript symbol");
        }
    }

    for(size_t i = 0; i < dim; ++i) {

        char c1 = m_Seq1? *p1: 'N';
        char c2 = m_Seq2? *p2: 'N';
        switch(transcript[i]) {

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

    return score;
}


size_t CNWAligner::GetLeftSeg(size_t* q0, size_t* q1,
                              size_t* s0, size_t* s1,
                              size_t min_size) const
{
    size_t trdim = m_Transcript.size();
    size_t cur = 0, maxseg = 0;
    const char* p1 = m_Seq1;
    const char* p2 = m_Seq2;
    size_t i0 = 0, j0 = 0, imax = 0, jmax = 0;

    for(int k = trdim - 1; k >= 0; --k) {

        switch(m_Transcript[k]) {

            case eTS_Insert: {
                ++p2;
                if(cur > maxseg) {
                    maxseg = cur;
                    imax = i0;
                    jmax = j0;
                    if(maxseg >= min_size) goto ret_point;
                }
                cur = 0;
            }
            break;

            case eTS_Delete: {
            ++p1;
            if(cur > maxseg) {
                maxseg = cur;
                imax = i0;
                jmax = j0;
                if(maxseg >= min_size) goto ret_point;
            }
            cur = 0;
            }
            break;

            case eTS_Match:
            case eTS_Replace: {
                if(*p1 == *p2) {
                    if(cur == 0) {
                        i0 = p1 - m_Seq1;
                        j0 = p2 - m_Seq2;
                    }
                    ++cur;
                }
                else {
                    if(cur > maxseg) {
                        maxseg = cur;
                        imax = i0;
                        jmax = j0;
                        if(maxseg >= min_size) goto ret_point;
                    }
                    cur = 0;
                }
                ++p1;
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

 ret_point:

    *q0 = imax; *s0 = jmax;
    *q1 = *q0 + maxseg - 1;
    *s1 = *s0 + maxseg - 1;

    return maxseg;
}


size_t CNWAligner::GetRightSeg(size_t* q0, size_t* q1,
                               size_t* s0, size_t* s1,
                               size_t min_size) const
{
    size_t trdim = m_Transcript.size();
    size_t cur = 0, maxseg = 0;
    const char* seq1_end = m_Seq1 + m_SeqLen1;
    const char* seq2_end = m_Seq2 + m_SeqLen2;
    const char* p1 = seq1_end - 1;
    const char* p2 = seq2_end - 1;
    size_t i0 = 0, j0 = 0, imax = 0, jmax = 0;

    for(size_t k = 0; k < trdim; ++k) {

        switch(m_Transcript[k]) {

            case eTS_Insert: {
                --p2;
                if(cur > maxseg) {
                    maxseg = cur;
                    imax = i0;
                    jmax = j0;
                    if(maxseg >= min_size) goto ret_point;
                }
                cur = 0;
            }
            break;

            case eTS_Delete: {
                --p1;
                if(cur > maxseg) {
                    maxseg = cur;
                    imax = i0;
                    jmax = j0;
                    if(maxseg >= min_size) goto ret_point;            }
                cur = 0;
            }
            break;

            case eTS_Match:
            case eTS_Replace: {
                if(*p1 == *p2) {
                    if(cur == 0) {
                        i0 = p1 - m_Seq1;
                        j0 = p2 - m_Seq2;
                    }
                    ++cur;
                }
                else {
                    if(cur > maxseg) {
                        maxseg = cur;
                        imax = i0;
                        jmax = j0;
                        if(maxseg >= min_size) goto ret_point;
                    }
                    cur = 0;
                }
                --p1;
                --p2;
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

 ret_point:

    *q1 = imax; *s1 = jmax;
    *q0 = imax - maxseg + 1;
    *s0 = jmax - maxseg + 1;

    return maxseg;
}


size_t CNWAligner::GetLongestSeg(size_t* q0, size_t* q1,
                                 size_t* s0, size_t* s1) const
{
    size_t trdim = m_Transcript.size();
    size_t cur = 0, maxseg = 0;
    const char* p1 = m_Seq1;
    const char* p2 = m_Seq2;
    size_t i0 = 0, j0 = 0, imax = 0, jmax = 0;

    for(int k = trdim-1; k >= 0; --k) {

        switch(m_Transcript[k]) {

            case eTS_Insert: {
                ++p2;
                if(cur > maxseg) {
                    maxseg = cur;
                    imax = i0;
                    jmax = j0;
                }
                cur = 0;
            }
            break;

            case eTS_Delete: {
            ++p1;
            if(cur > maxseg) {
                maxseg = cur;
                imax = i0;
                jmax = j0;
            }
            cur = 0;
            }
            break;

            case eTS_Match:
            case eTS_Replace: {
                if(*p1 == *p2) {
                    if(cur == 0) {
                        i0 = p1 - m_Seq1;
                        j0 = p2 - m_Seq2;
                    }
                    ++cur;
                }
                else {
                    if(cur > maxseg) {
                        maxseg = cur;
                        imax = i0;
                        jmax = j0;
                    }
                    cur = 0;
                }
                ++p1;
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

    *q0 = imax; *s0 = jmax;
    *q1 = *q0 + maxseg - 1;
    *s1 = *s0 + maxseg - 1;

    return maxseg;
}


END_NCBI_SCOPE


/*
 * ===========================================================================
 * $Log$
 * Revision 1.33  2003/09/02 22:38:18  kapustin
 * Move format functionality out of the class
 *
 * Revision 1.32  2003/07/09 15:11:22  kapustin
 * Update plain text output with seq ids and use inequalities instead of
 * binary operation to verify intron boundaries
 *
 * Revision 1.31  2003/06/26 20:39:39  kapustin
 * Rename formal parameters in SetEndSpaceFree() to avoid conflict with macro
 * under some configurations
 *
 * Revision 1.30  2003/06/17 17:20:44  kapustin
 * CNWAlignerException -> CAlgoAlignException
 *
 * Revision 1.29  2003/06/17 16:06:13  kapustin
 * Detect all variety of splices in Seq-Align formatter (restored from 1.27)
 *
 * Revision 1.28  2003/06/17 14:51:04  dicuccio
 * Fixed after algo/ rearragnement
 *
 * Revision 1.26  2003/06/02 14:04:49  kapustin
 * Progress indication-related updates
 *
 * Revision 1.25  2003/05/23 18:27:02  kapustin
 * Use weak comparisons in core recurrences. Adjust for new
 * transcript identifiers.
 *
 * Revision 1.24  2003/04/17 14:44:40  kapustin
 * A few changes to eliminate gcc warnings
 *
 * Revision 1.23  2003/04/14 19:00:55  kapustin
 * Add guide creation facility.  x_Run() -> x_Align()
 *
 * Revision 1.22  2003/04/10 19:15:16  kapustin
 * Introduce guiding hits approach
 *
 * Revision 1.21  2003/04/02 20:52:55  kapustin
 * Make FormatAsText virtual. Pass output string as a parameter.
 *
 * Revision 1.20  2003/03/31 15:32:05  kapustin
 * Calculate score independently from transcript
 *
 * Revision 1.19  2003/03/19 16:36:39  kapustin
 * Reverse memory limit condition
 *
 * Revision 1.18  2003/03/18 15:15:51  kapustin
 * Implement virtual memory checking function. Allow separate free end
 * gap specification
 *
 * Revision 1.17  2003/03/17 15:31:46  kapustin
 * Forced conversion to double in memory limit checking to avoid integer
 * overflow
 *
 * Revision 1.16  2003/03/14 19:18:50  kapustin
 * Add memory limit checking. Fix incorrect seq index references when
 * reporting about incorrect input seq character. Support all characters
 * within IUPACna coding
 *
 * Revision 1.15  2003/03/07 13:51:11  kapustin
 * Use zero-based indices to specify seq coordinates in ASN
 *
 * Revision 1.14  2003/03/05 20:13:48  kapustin
 * Simplify FormatAsText(). Fix FormatAsSeqAlign(). Convert sequence
 * alphabets to capitals
 *
 * Revision 1.13  2003/02/11 16:06:54  kapustin
 * Add end-space free alignment support
 *
 * Revision 1.12  2003/02/04 23:04:50  kapustin
 * Add progress callback support
 *
 * Revision 1.11  2003/01/31 02:55:48  lavr
 * User proper argument for ::time()
 *
 * Revision 1.10  2003/01/30 20:32:36  kapustin
 * Add EstiamteRunningTime()
 *
 * Revision 1.9  2003/01/28 12:39:03  kapustin
 * Implement ASN.1 and SeqAlign output
 *
 * Revision 1.8  2003/01/24 16:48:50  kapustin
 * Support more output formats - type 2 and gapped FastA
 *
 * Revision 1.7  2003/01/21 16:34:21  kapustin
 * Get rid of reverse() and reverse_copy() not supported by MSVC and/or ICC
 *
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
