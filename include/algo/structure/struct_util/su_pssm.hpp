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
* Authors:  Paul Thiessen
*
* File Description:
*      new C++ PSSM construction
*
* ===========================================================================
*/

#ifndef STRUCT_UTIL_PSSM__HPP
#define STRUCT_UTIL_PSSM__HPP

#include <corelib/ncbistd.hpp>
#include <corelib/ncbistre.hpp>


BEGIN_SCOPE(struct_util)

// BLAST_Matrix structure (from blastkar.h, edited), brought in here to avoid any C-toolkit dependency;
// will use this structure for PSSM scoring since it's more efficient than accessing the more complex
// scoremat.asn objects (plus I already have code that uses this... ;))
class NCBI_STRUCTUTIL_EXPORT BLAST_Matrix
{
public:
    // data, parallels original BLAST_Matrix structure
    bool is_prot;    /* Matrix is for proteins */
    char *name;       /* Name of Matrix (i.e., BLOSUM62). */
    /* Position-specific BLAST rows and columns are different, otherwise they are the alphabet length. */
    int rows,       /* query length + 1 for PSSM. */
        columns;    /* alphabet size in all cases (28). */
    int **matrix;
//    double **posFreqs;
    double karlinK;
//    int **original_matrix;

    BLAST_Matrix(int nRows, int nColumns);
    ~BLAST_Matrix(void);
};

class BlockMultipleAlignment;

NCBI_STRUCTUTIL_EXPORT 
BLAST_Matrix * CreateBlastMatrix(const BlockMultipleAlignment *bma);

// utility functions
extern 
NCBI_STRUCTUTIL_EXPORT int GetPSSMScoreOfCharWithAverageOfBZ(const BLAST_Matrix *matrix, unsigned int pssmIndex, char resChar);
extern 
NCBI_STRUCTUTIL_EXPORT unsigned char LookupNCBIStdaaNumberFromCharacter(char r);
extern 
NCBI_STRUCTUTIL_EXPORT char LookupCharacterFromNCBIStdaaNumber(unsigned char n);

END_SCOPE(struct_util)

#endif // STRUCT_UTIL_PSSM__HPP
