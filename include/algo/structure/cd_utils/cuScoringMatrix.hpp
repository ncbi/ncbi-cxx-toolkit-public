#ifndef CU_SCORING_MATRIX__HPP
#define CU_SCORING_MATRIX__HPP

/*  $Id$
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
* Authors:  Chris Lanczycki (w/ some code from Paul Thiessen's conservation_colorer)
*
* File Description:
*      Header to define alignment scoring matrices and their parameters,
*      and the class to encapsulate them.  Uses C++ toolkit to access
*      centrally defined scoring matrices.
*
* ===========================================================================
*/

#include <corelib/ncbistd.hpp>  // must come before C-toolkit stuff
#include <corelib/ncbistl.hpp>
#include <util/tables/raw_scoremat.h>

#include <map>
#include <vector>
#include <ncbierr.h>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(cd_utils)

//====================  Scoring Matrix Definitions  ====================//
//  Unless otherwise noted, matrices are obtained from API defined in
//  include/util/tables/raw_scoremat.h
//
//======================================================================//

//  For each matrix use <matrix>NAME as given in blastool.c, BLASTOptionsSetGapParams 
//  for standard scoring matrices.  Otherwise, cannot use to pass to Blast options.

const int NUMBER_OF_SCORING_MATRIX_TYPES = 6;
enum EScoreMatrixType {
    eInvalidMatrixType   = 0,
    eBlosum45  = 1,
    eBlosum62  = 2,
    eBlosum80  = 3,
    ePam30     = 4,
    ePam70     = 5,
    ePam250    = 6
};
const EScoreMatrixType GLOBAL_DEFAULT_SCORE_MATRIX = eBlosum62;

string GetScoringMatrixName(EScoreMatrixType type);

#define INVALIDSIZE 0
#define INVALIDNAME "InvalidMatrix"

//====================  BLOSUM 45 Matrix Definition  ====================//
//
#define BLOSUM45SIZE 24
#define BLOSUM45NAME "BLOSUM45"

//====================  BLOSUM 62 Matrix Definition  ====================//
//
#define BLOSUM62SIZE 24
#define BLOSUM62NAME "BLOSUM62"

//====================  BLOSUM 80 Matrix Definition  ====================//
//
#define BLOSUM80SIZE 24
#define BLOSUM80NAME "BLOSUM80"

//====================  PAM 30 Matrix Definition  ====================//
//
#define PAM30SIZE 24
#define PAM30NAME "PAM30"

//====================  PAM 70 Matrix Definition  ====================//
//
#define PAM70SIZE 24
#define PAM70NAME "PAM70"


//====================  PAM 250 Matrix Definition  ====================//
//  Not a standard matrix in the NCBI Toolkit.
//  (copied from http://www.cmbi.kun.nl/gvteach/aainfo/pam250.shtml;
//   confirmed from http://www.es.embnet.org/Services/ftp/databases/matrices/PAM250)
//
#define PAM250SIZE 24
#define PAM250NAME "PAM250"

//
//====================  End Matrix Definitions  ====================//
//

inline char ScreenResidueCharacter(char original)
{
    char ch = toupper(original);
    switch (ch) {
    case 'A': case 'R': case 'N': case 'D': case 'C':
    case 'Q': case 'E': case 'G': case 'H': case 'I':
    case 'L': case 'K': case 'M': case 'F': case 'P':
    case 'S': case 'T': case 'W': case 'Y': case 'V':
    case 'B': case 'Z':
        break;
    default:
        ch = 'X'; // make all but natural aa's just 'X'
    }
    return ch;
}

class ScoreMatrix {

//    typedef std::map < char, std::map < char, int > > ScoreMap;

public:

    ScoreMatrix(EScoreMatrixType type);
    ~ScoreMatrix(){
        m_alphabet = NULL;
    }

    EScoreMatrixType GetType() const {
        return m_type;
    }

    string GetName() const {
        return m_name;
    }
    inline int GetScore(char i, char j) {
        return m_scoreMatrix.s[(unsigned)ScreenResidueCharacter(i)][(unsigned)ScreenResidueCharacter(j)];
    }
    bool IsValid() const {
        return (m_type != eInvalidMatrixType);
    }


private:

    void initialize(EScoreMatrixType type);

    EScoreMatrixType     m_type;
    string               m_name;
    int                  m_numLetters;
    const char*          m_alphabet;
    SNCBIFullScoreMatrix m_scoreMatrix;
//    ScoreMap        m_scoreMap;

};

END_SCOPE(cd_utils)
END_NCBI_SCOPE

#endif   //  CDT_SCORING_MATRIX__HPP

/*
 * ===========================================================================
 *
 * $Log$
 * Revision 1.1  2005/04/19 14:28:01  lanczyck
 * initial version under algo/structure
 *
 *
 * ===========================================================================
 */
