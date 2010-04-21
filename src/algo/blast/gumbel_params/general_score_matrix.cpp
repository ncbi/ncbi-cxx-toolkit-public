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

File name: general_score_matrix.cpp

Author: Greg Boratyn

Contents: Implementation of generalized score matrix

******************************************************************************/


#include <ncbi_pch.hpp>

#include <math.h>
#include <algo/blast/gumbel_params/general_score_matrix.hpp>


USING_NCBI_SCOPE;
USING_SCOPE(blast);

//CGeneralScoreMatrix

CGeneralScoreMatrix::CGeneralScoreMatrix(
                               CGeneralScoreMatrix::EScoreMatrixName type,
                               unsigned int sub_size)
{
    SNCBIPackedScoreMatrix matrix;
    SNCBIFullScoreMatrix full_matrix;

    switch (type) {
        
    case eBlosum45:
        matrix = NCBISM_Blosum45;
        break;
    case eBlosum62:
        matrix = NCBISM_Blosum62;
        break;
    case eBlosum80:
        matrix = NCBISM_Blosum80;
        break;
    case ePam30:
        matrix = NCBISM_Pam30;
        break;
    case ePam70:
        matrix = NCBISM_Pam70;
        break;
    case ePam250:
        matrix = NCBISM_Pam250;
        break;

    default:
        NCBI_THROW(CGeneralScoreMatrixException, eInvalid,
                   "Unrecognised standard scoring matrix name");
    }
    
    NCBISM_Unpack(&matrix, &full_matrix);
    
    m_NumResidues = min(strlen(matrix.symbols), (size_t)sub_size);
    m_ScoreMatrix = new int*[m_NumResidues];
    *m_ScoreMatrix = new int[m_NumResidues * m_NumResidues];
    for (unsigned i=1;i < m_NumResidues;i++) {
        m_ScoreMatrix[i] = &(m_ScoreMatrix[0][i * m_NumResidues]);
    }
    
    m_ResidueOrder = new char[m_NumResidues];
    strncpy(m_ResidueOrder, matrix.symbols, m_NumResidues);
    
    for (unsigned i=0;i < m_NumResidues;i++) {
         for (unsigned j=0;j < m_NumResidues;j++) {
            m_ScoreMatrix[i][j] = full_matrix.s[
                     (unsigned)m_ResidueOrder[i]][(unsigned)m_ResidueOrder[j]];
        } 
    }
}


CGeneralScoreMatrix::CGeneralScoreMatrix(const Int4** matrix, 
                                         unsigned int num_res,
                                         const char* order) 
    : m_NumResidues(num_res)
{
    if (order) {
        m_ResidueOrder=new char[m_NumResidues];    
        m_ResidueOrder = strncpy(m_ResidueOrder, order, num_res);
    }
    else {
        m_ResidueOrder = NULL;
    }
    m_ScoreMatrix = new Int4*[m_NumResidues];
    *m_ScoreMatrix = new Int4[m_NumResidues * m_NumResidues];
    for (unsigned i=1;i < m_NumResidues;i++) {
        m_ScoreMatrix[i] = &m_ScoreMatrix[0][i * m_NumResidues];
    }
    for (unsigned i=0;i < m_NumResidues;i++) {
        for (unsigned j=0;j < m_NumResidues;j++)
            m_ScoreMatrix[i][j] = matrix[i][j];
    }
}

CGeneralScoreMatrix::CGeneralScoreMatrix(const vector<Int4>& vals,
                                         const char* order)
    : m_NumResidues((unsigned int)sqrt((double)vals.size()))
{
    _ASSERT(vals.size() > 0);
    _ASSERT(m_NumResidues * m_NumResidues == vals.size());
   
    if (order) {
        m_ResidueOrder=new char[m_NumResidues];    
        m_ResidueOrder = strncpy(m_ResidueOrder, order, m_NumResidues);
    }
    else {
        m_ResidueOrder = NULL;
    }
    m_ScoreMatrix = new Int4*[m_NumResidues];
    *m_ScoreMatrix = new Int4[m_NumResidues * m_NumResidues];
    for (unsigned i=1;i < m_NumResidues;i++) {
        m_ScoreMatrix[i] = &m_ScoreMatrix[0][i * m_NumResidues];
    }
    for (unsigned i=0;i < vals.size();i++) {
        m_ScoreMatrix[0][i] = vals[i];
    }    
}

CGeneralScoreMatrix::CGeneralScoreMatrix(const CGeneralScoreMatrix& matrix)
{
    m_NumResidues = matrix.m_NumResidues;
    if (matrix.m_ResidueOrder) {
        m_ResidueOrder = new char[m_NumResidues];
        memcpy(m_ResidueOrder, matrix.GetResidueOrder(), m_NumResidues 
               * sizeof(char));
    }
    else {
        m_ResidueOrder = NULL;
    }

    m_ScoreMatrix = new Int4*[m_NumResidues];
    *m_ScoreMatrix = new Int4[m_NumResidues * m_NumResidues];
    for (unsigned i=1;i < m_NumResidues;i++) {
        m_ScoreMatrix[i] = &m_ScoreMatrix[0][i * m_NumResidues];
    }

    memcpy(*m_ScoreMatrix, *matrix.m_ScoreMatrix, m_NumResidues * m_NumResidues * sizeof(Int4));
}


CGeneralScoreMatrix::~CGeneralScoreMatrix()
{
    if (m_ResidueOrder)
        delete [] m_ResidueOrder;

    if (m_ScoreMatrix != NULL && *m_ScoreMatrix != NULL) {
        delete [] *m_ScoreMatrix;
        delete [] m_ScoreMatrix;
    }
}


Int4 CGeneralScoreMatrix::GetScore(Uint4 i, Uint4 j) const
{
    if (i >= m_NumResidues || j >= m_NumResidues) {
        NCBI_THROW(CGeneralScoreMatrixException, eIndexOutOfBounds,
                   "Score matrix index out of bounds");
    }

    return m_ScoreMatrix[i][j];
}

Int4 CGeneralScoreMatrix::GetScore(char a, char b) const
{
    if (!m_ResidueOrder) {
        NCBI_THROW(CGeneralScoreMatrixException, eNoResidueInfo,
                   "Score matrix does not contain residue order information");
    }

    unsigned i = 0, j = 0;
    while (i < m_NumResidues && m_ResidueOrder[i] != a) {
        i++;
    }
    while (j < m_NumResidues && m_ResidueOrder[j] != b) {
        j++;
    }

    if (i >= m_NumResidues) {
        char residue[2];
        residue[0] = a;
        residue[1] = 0;
        NCBI_THROW(CGeneralScoreMatrixException, eInvalidResidue,
                   (string)"Residue " + residue
                   + " not in score matrix alphabet");
    }

    if (j >= m_NumResidues) {
        char residue[2];
        residue[0] = b;
        residue[1] = 0;
        NCBI_THROW(CGeneralScoreMatrixException, eInvalidResidue,
                   (string)"Residue " + residue
                   + " not in score matrix alphabet");
    }

    return m_ScoreMatrix[i][j];
}

