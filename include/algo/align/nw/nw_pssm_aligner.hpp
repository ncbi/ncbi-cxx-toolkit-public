#ifndef ALGO___NW_PSSM_ALIGNER__HPP
#define ALGO___NW_PSSM_ALIGNER__HPP

/* $Id$
* ===========================================================================
*
*                            public DOMAIN NOTICE                          
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
* File Description:
*   CPSSMAligner class definition
*
*   CPSSMAligner encapsulates a generic global (Needleman-Wunsch)
*   alignment algorithm with affine gap penalty model and position-
*   specific scoring for one or both input sequences.
*
*/

#include <corelib/ncbistd.hpp>
#include <corelib/ncbiobj.hpp>
#include <corelib/ncbi_limits.hpp>
#include <util/tables/raw_scoremat.h>
#include <algo/align/nw/nw_aligner.hpp>

#include <vector>
#include <string>


/** @addtogroup AlgoAlignRoot
 *
 * @{
 */


BEGIN_NCBI_SCOPE


// Needleman Wunsch algorithm with position-specific scoring
//

class NCBI_XALGOALIGN_EXPORT CPSSMAligner: public CNWAligner
{
public:
    // ctors
    CPSSMAligner();

    CPSSMAligner(const CNWAligner::TScore** pssm1, size_t len1,
                 const char* seq2, size_t len2);

    CPSSMAligner(const double** freq1, size_t len1,
                 const double** freq2, size_t len2,
                 const SNCBIPackedScoreMatrix* scoremat,
                 const int scale = 1);

    virtual ~CPSSMAligner(void) {}

    // Compute the alignment
    virtual CNWAligner::TScore Run(void);

    // Setters
    void SetSequences(const char* seq1, size_t len1,
                      const char* seq2, size_t len2,
                      bool verify = true);

    void SetSequences(const CNWAligner::TScore** pssm1, size_t len1,
                      const char* seq2, size_t len2,
                      bool verify = true);

    void SetSequences(const double** freq1, size_t len1,
                      const double** freq2, size_t len2,
                      const int scale = 1);

    void SetScoreMatrix(const SNCBIPackedScoreMatrix* scoremat);

    void SetFreqScale(const int scale) {m_FreqScale = scale;}

    void SetWg  (TScore value)   // gap opening
    { 
        m_StartWg = m_Wg  = m_EndWg = value; 
    }
    void SetWs  (TScore value)   // gap extension
    { 
        m_StartWs = m_Ws  = m_EndWs = value; 
    }
    void SetStartWg(TScore value)  { m_StartWg = value; }   // gap opening
    void SetStartWs(TScore value)  { m_StartWs = value; }   // gap extension
    void SetEndWg(TScore value)    { m_EndWg = value; }   // gap opening
    void SetEndWs(TScore value)    { m_EndWs = value; }   // gap extension

    // Getters
    const CNWAligner::TScore** GetPssm1() const {return m_Pssm1;}
    const char* GetSeq1() const                 {return m_Seq1;}
    const double** GetFreq1() const             {return m_Freq1;}
    const double** GetFreq2() const             {return m_Freq2;}
    int GetFreqScale() const                    {return m_FreqScale;}

    TScore GetStartWg() const { return m_StartWg; }
    TScore GetStartWs() const { return m_StartWs; }
    TScore GetEndWg() const   { return m_EndWg; }
    TScore GetEndWs() const   { return m_EndWs; }
    SNCBIFullScoreMatrix& GetMatrix() { return m_ScoreMatrix; }

    virtual TScore ScoreFromTranscript(const TTranscript& transcript,
                                       size_t start1 = 0,
                                       size_t start2 = 0) const;

protected:

    // only NCBIstdaa alphabet supported
    static const int kPSSM_ColumnSize = 28;

    // Source sequences
    const TScore** m_Pssm1;
    const double** m_Freq1;

    const char*    m_Seq2;
    const double** m_Freq2;

    // scale factor for position frequencies
    int                        m_FreqScale;

    TScore   m_StartWg;// gap opening penalty for initial gaps
    TScore   m_StartWs;// gap extension penalty for initial gaps
    TScore   m_EndWg;  // gap opening penalty for terminal gaps
    TScore   m_EndWs;  // gap extension penalty for terminal gaps

    // core dynamic programming
    virtual TScore x_Align (SAlignInOut* data);
    TScore x_AlignProfile (SAlignInOut* data);
    TScore x_AlignPSSM (SAlignInOut* data);

    // retrieve transcript symbol for a one-character diag
    virtual ETranscriptSymbol x_GetDiagTS(size_t i1, size_t i2) const;

    double m_DScoreMatrix[kPSSM_ColumnSize][kPSSM_ColumnSize];
};


END_NCBI_SCOPE


/* @} */

#endif  /* ALGO___NW_PSSM_ALIGNER__HPP */
