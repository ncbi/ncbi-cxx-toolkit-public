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
 * Author:  Charlie Liu
 *
 * File Description: 
 *   part of CDTree app
 */

#include <ncbi_pch.hpp>
#include <algo/blast/api/blast_exception.hpp>
#include <objects/scoremat/Pssm.hpp>
#include <objects/scoremat/PssmFinalData.hpp>
#include <objects/scoremat/PssmIntermediateData.hpp>
#include <objects/scoremat/PssmWithParameters.hpp>
#include <objects/scoremat/PssmParameters.hpp>
#include <objects/scoremat/FormatRpsDbParameters.hpp>
#include <algo/blast/core/blast_def.h>
#include <algo/structure/cd_utils/structure_util.hpp>

/****************************************************************************/
// Copied verbatim from blast_psi_priv.c
void**
_PSIDeallocateMatrix(void** matrix, unsigned int ncols)
{
    unsigned int i = 0;

    if (!matrix)
        return NULL;

    for (i = 0; i < ncols; i++) {
        sfree(matrix[i]);
    }
    sfree(matrix);
    return NULL;
}
void**
_PSIAllocateMatrix(unsigned int ncols, unsigned int nrows, 
                   unsigned int data_type_sz)
{
    void** retval = NULL;
    unsigned int i = 0;

    retval = (void**) malloc(sizeof(void*) * ncols);
    if ( !retval ) {
        return NULL;
    }

    for (i = 0; i < ncols; i++) {
        retval[i] = (void*) calloc(nrows, data_type_sz);
        if ( !retval[i] ) {
            retval = _PSIDeallocateMatrix(retval, i);
            break;
        }
    }
    return retval;
}

/****************************************************************************/

BLAST_Matrix*
PssmToBLAST_Matrix(const ncbi::objects::CPssmWithParameters& pssm) 
{
    BLAST_Matrix* retval = (BLAST_Matrix*) calloc(1, sizeof(BLAST_Matrix));
    bool valid_data = false;

    if ( !pssm.GetPssm().CanGetFinalData()) {
        NCBI_THROW(ncbi::blast::CBlastException, eBadParameter, 
                   "Final data in PSSM unavailable");
    }
    
    retval->is_prot = pssm.GetPssm().GetIsProtein() ? TRUE : FALSE;
    retval->name = strdup(pssm.GetParams().GetRpsdbparams().GetMatrixName().c_str());
    // Columns and rows are swapped in C toolkit
    retval->rows = pssm.GetPssm().GetNumColumns() + 1;
    retval->columns = 
        pssm.GetPssm().GetNumRows();
    ASSERT(retval->columns == 26);

    // Calculate the maximum indices to which to iterate over when copying
    // matrix data
    unsigned int columns, rows;
    if (pssm.GetPssm().GetByRow() == false) {
        columns = retval->rows - 1;
        rows = retval->columns;
    } else {
        columns = retval->columns;
        rows = retval->rows - 1;
    }

    if (pssm.GetPssm().GetFinalData().CanGetScores()) {
        retval->matrix = (int**) _PSIAllocateMatrix(retval->rows,
                                                    retval->columns,
                                                    sizeof(int));
        ncbi::objects::CPssmFinalData::TScores::const_iterator itr =
            pssm.GetPssm().GetFinalData().GetScores().begin();
        for (unsigned int i = 0; i < columns; i++) {
            for (unsigned int j = 0; j < rows; j++) {
                retval->matrix[i][j] = *itr++;
            }
        }
        // Set the last column to BLAST_SCORE_MIN
        for (unsigned int i = 0; i < rows; i++) {
            retval->matrix[columns][i] = BLAST_SCORE_MIN;
        }
        valid_data = true;
    }

    if (pssm.GetPssm().CanGetIntermediateData() &&
        pssm.GetPssm().GetIntermediateData().CanGetFreqRatios()) {
        retval->posFreqs = (double**) _PSIAllocateMatrix(retval->rows,
                                                         retval->columns,
                                                         sizeof(double));
        ncbi::objects::CPssmIntermediateData::TFreqRatios::const_iterator itr =
            pssm.GetPssm().GetIntermediateData().GetFreqRatios().begin();
        for (unsigned int i = 0; i < columns; i++) {
            for (unsigned int j = 0; j < rows; j++) {
                retval->posFreqs[i][j] = *itr++;
            }
        }
        // Set the last column to BLAST_SCORE_MIN (?)
        for (unsigned int i = 0; i < rows; i++) {
            retval->posFreqs[columns][i] = BLAST_SCORE_MIN;
        }
        valid_data = true;
    }
    retval->karlinK = pssm.GetPssm().GetFinalData().GetKappa();
    retval->original_matrix = NULL;

    if ( !valid_data ) {
        NCBI_THROW(ncbi::blast::CBlastException, eBadParameter,
                   "PSSM is missing frequency ratios or scores");
    }

    return retval;
}



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
