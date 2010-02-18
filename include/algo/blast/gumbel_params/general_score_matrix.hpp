#ifndef ALGO_BLAST_GUMBEL_PARAMS__GENERAL_SCORE_MATRIX___HPP
#define ALGO_BLAST_GUMBEL_PARAMS__GENERAL_SCORE_MATRIX___HPP

/* $Id$
* ===========================================================================
*
*                            PUBLIC DOMAIN NOTICE
*               National Center for Biotechnology Information
*
*  This software/database is a "United States Government Work" under the
*  terms of the United States Copyright Act.  It was written as part of
*  the author's offical duties as a United States Government employee and
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
* ===========================================================================*/

/*****************************************************************************

File name: general_score_matrix.hpp

Author: Greg Boratyn

Contents: Generalized score matrix that can be set to any values.
          Initialization to any of the standard score matrices is also 
          available.

******************************************************************************/


#include <corelib/ncbiobj.hpp>
#include <util/tables/raw_scoremat.h>
#include <vector>


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(blast)

/// Score matrix that can take any value
class CGeneralScoreMatrix : public CObject
{
public:
    /// Names of standard scoring matrices
    ///
    enum EScoreMatrixName {
        eBlosum45,
        eBlosum62,
        eBlosum80,
        ePam30,
        ePam70,
        ePam250
    };

public:
    /// Creates a standard score matrix
    /// @param type Standard score matrix type [in]
    /// @param sub_size Number of rows and columns to copy [in]
    ///
    CGeneralScoreMatrix(EScoreMatrixName type, 
                        unsigned int sub_size = 25);

    /// Creates user defined score matrix. Matrix and alphabet are copied.
    /// @param matrix Matrix values [in]
    /// @param size Number of alphabet elements [in]
    /// @param alphabet Alphabel symbols [in]
    ///
    CGeneralScoreMatrix(const Int4** matrix, unsigned int size,
                        const char* alphabet = NULL);

    /// Creates score matrix from values in a given vector
    /// @param vals Values for the score matrix as consecutive rows [in]
    /// @param alphabet Order of residues for the score matrix or NULL
    ///
    CGeneralScoreMatrix(const vector<Int4>& vals, const char* alphabet = NULL);

    /// Copy constructor
    /// @param matrix Score matrix
    CGeneralScoreMatrix(const CGeneralScoreMatrix& matrix);

    /// Destructor
    ~CGeneralScoreMatrix();

    /// Access matrix values
    /// @return Pointer to matrix values
    ///
    const Int4** GetMatrix(void) const {return (const Int4**)m_ScoreMatrix;}

    /// Get order of residues in the score matrix
    /// @return Pointer to alphabet symbols
    ///
    const char* GetResidueOrder(void) const {return m_ResidueOrder;}

    /// Get number of residues
    /// @return Number of residues
    ///
    unsigned int GetNumResidues(void) const {return m_NumResidues;}

    /// Get score matrix entry for at specified position. Throws exception
    /// if index out of bounds
    /// @param i Matrix row number [in]
    /// @param j Matrix column number [in]
    /// @return Score matrix value at position i, j
    ///
    Int4 GetScore(Uint4 i, Uint4 j) const;

    /// Return score matrix entry for a given pair of residues. Throws 
    /// exception for illegal residues
    /// @param a Residue 1 [in]
    /// @param b Residue 2 [in]
    /// @return Score matrix value for a given pair of residues
    ///
    Int4 GetScore(char a, char b) const;

private:
    /// Forbid assignment operator
    CGeneralScoreMatrix& operator=(const CGeneralScoreMatrix&);

protected:
    Int4** m_ScoreMatrix;
    char* m_ResidueOrder;
    unsigned int m_NumResidues;
};


/// Exceptions for CGeneralScoreMatrix
class CGeneralScoreMatrixException : public CException
{
public:
    enum EErrCode {
        eInvalid,
        eIndexOutOfBounds,
        eInvalidResidue,
        eNoResidueInfo
    };

    virtual const char* GetErrCodeString(void) const {
        switch (GetErrCode()) {
        case eInvalid : return "eInvalid";
        case eIndexOutOfBounds : return "eIndexOutOfBounds";
        case eInvalidResidue : return "eInvalidResidue";
        case eNoResidueInfo : return "eNoResidueInfo";
        }
        return "UnknownError";
    }

    NCBI_EXCEPTION_DEFAULT(CGeneralScoreMatrixException, CException);
};


END_SCOPE(blast)
END_NCBI_SCOPE

#endif // ALGO_BLAST_GUMBEL_PARAMS__GENERAL_SCORE_MATRIX___HPP


