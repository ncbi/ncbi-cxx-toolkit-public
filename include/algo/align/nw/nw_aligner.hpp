#ifndef UTIL___NW_ALIGNER__HPP
#define UTIL___NW_ALIGNER__HPP

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
* Author:  Yuri Kapustin
*
* File Description:
*   CNWAligner class definition
*
*   CNWAligner encapsulates the global alignment algorithm
*   featuring affine gap penalty model.
*   For a description of the algorithm, see
*
*   Dan Gusfeld, "Algorithms on Strings, Trees and Sequences", 
*   11.8.6. Affine (and constant) gap weights
*
*/

#include <corelib/ncbistd.hpp>
#include <vector>
#include <string>


BEGIN_NCBI_SCOPE


// Exceptions
//

class CNWAlignerException : public CException 
{
public:
    enum EErrCode {
        eBadParameter,
        eInvalidSequence
    };
    virtual const char* GetErrCodeString(void) const {
        switch ( GetErrCode() ) {
        case eBadParameter:
            return "One or more parameters passed are invalid";
        case eInvalidSequence:
            return "Sequence contains one or more invalid characters";
        default:
            return CException::GetErrCodeString();
        }
    }
    NCBI_EXCEPTION_DEFAULT(CNWAlignerException, CException);
};


// NW algorithm encapsulation
//

class CNWAligner
{
public:
    typedef int TScore;

    // ctors
    enum EScoringMatrixType {
        eNucl,
        eBlosum62
    };

    CNWAligner(const char* seq1, size_t len1,
               const char* seq2, size_t len2,
               EScoringMatrixType matrix_type)
        throw(CNWAlignerException);

    virtual ~CNWAligner() {}

    // Run generic Needleman-Wunsch algorithm, return the alignment's score
    virtual TScore Run();

    // Create human-readable representation of the alignment
    string Format(size_t line_width = 50) const;

    // Retrieve transcript string
    string GetTranscript() const;

    // Setters
    void SetWm  (TScore value)  { m_Wm  = value; }
    void SetWms (TScore value)  { m_Wms = value; }
    void SetWg  (TScore value)  { m_Wg  = value; }
    void SetWs  (TScore value)  { m_Ws  = value; }

    // Getters
    static TScore GetDefaultWm  () { return  1; }
    static TScore GetDefaultWms () { return -3; }
    static TScore GetDefaultWg  () { return -5; }
    static TScore GetDefaultWs  () { return -2; }

private:
    // Bonuses and penalties
    TScore   m_Wm;   // match bonus (eNucl)
    TScore   m_Wms;  // mismatch penalty (eNucl)
    TScore   m_Wg;   // gap opening penalty
    TScore   m_Ws;   // gap extension penalty

    // Pairwise scoring matrix
    TScore   m_Matrix [256][256];
    void     x_LoadMatrix();

    // Source sequences and their lengths
    const char*           m_Seq1;
    size_t                m_SeqLen1;
    const char*           m_Seq2;
    size_t                m_SeqLen2;
    EScoringMatrixType    m_MatrixType;

    size_t x_CheckSequence(const char* seq, size_t len) const;

    // Transcript and backtrace
    enum ETranscriptSymbol {
        eNone = 0,
        eInsert,
        eDelete,
        eMatch,
        eReplace,
        eIntron
    };
    vector<ETranscriptSymbol> m_Transcript;

    void   x_DoBackTrace(const unsigned char* backtrace_matrix);
    size_t x_ApplyTranscript(vector<char>* seq1_transformed,
                             vector<char>* seq2_transformed) const;
};


END_NCBI_SCOPE


/*
 * ===========================================================================
 * $Log$
 * Revision 1.2  2002/12/09 15:44:40  kapustin
 * exception forward declaration removed
 *
 * Revision 1.1  2002/12/06 17:40:12  ivanov
 * Initial revision
 *
 * ===========================================================================
 */

#endif  /* UTIL___NW_ALIGNER__HPP */
