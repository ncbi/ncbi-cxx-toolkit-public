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

#include <ncbi_pch.hpp>
#include <algo/structure/cd_utils/cuScoringMatrix.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(cd_utils)

string GetScoringMatrixName(EScoreMatrixType type) {

	string name;
	switch (type) {
    case eBlosum45:
        name = BLOSUM45NAME;
		break;
    case eBlosum62:
        name = BLOSUM62NAME;
		break;
    case eBlosum80:
        name = BLOSUM80NAME;
		break;
    case ePam30:
        name = PAM30NAME;
		break;
    case ePam70:
        name = PAM70NAME;
		break;
    case ePam250:
        name = PAM250NAME;
		break;
    case eInvalidMatrixType:
    default:
        name = INVALIDNAME;
		break;
    }
	return name;
}

ScoreMatrix::ScoreMatrix(EScoreMatrixType type) {

    initialize(type);
}

void ScoreMatrix::initialize(EScoreMatrixType type) {

    SNCBIPackedScoreMatrix matrix;

    m_type = type;
    switch (m_type) {

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
    case eInvalidMatrixType:
    default:
        m_numLetters = INVALIDSIZE;
        m_alphabet = NULL;
		return;
		break;
    }

    m_name = GetScoringMatrixName(type);
    if (type != eInvalidMatrixType) {
        m_alphabet = matrix.symbols;
        m_numLetters = strlen(m_alphabet);
        NCBISM_Unpack(&matrix, &m_scoreMatrix);
    }

}

END_SCOPE(cd_utils)
END_NCBI_SCOPE

/*
 * ===========================================================================
 *
 * $Log$
 * Revision 1.1  2005/04/19 14:27:18  lanczyck
 * initial version under algo/structure
 *
 *
 * ===========================================================================
 */

