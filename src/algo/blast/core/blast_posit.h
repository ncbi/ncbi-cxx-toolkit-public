#ifndef ALGO_BLAST_CORE___BLAST_POSIT__H
#define ALGO_BLAST_CORE___BLAST_POSIT__H

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
 * Author:  Alejandro Schaffer (ported by Christiam Camacho)
 *
 */

/** @file blast_posit.h
 * Port of posit.h structures and impalaScaling for implementing composition
 * based statistics for PSI-BLAST.
 */

#include <algo/blast/core/blast_stat.h>
#include "matrix_freq_ratios.h"

#ifdef __cplusplus
extern "C" {
#endif

/** number of real aminoacids (i.e.: does not include U, X, B, etc) */
#define PRO_TRUE_ALPHABET_SIZE 20
/** range of scores in a matrix */
#define kScoreMatrixScoreRange 10000

/** positions of true characters in protein alphabet*/
static Int4 trueCharPositions[PRO_TRUE_ALPHABET_SIZE] =
  {1,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,22};


/** Structure used to pass data into the scaling routines */
typedef struct Kappa_posSearchItems {
    int**               posMatrix;
    int**               posPrivateMatrix;       /* this is only an alias */
    double**            posFreqs;               /* this is only an alias */
    SFreqRatios*        stdFreqRatios;
    unsigned int        queryLength;
} Kappa_posSearchItems;

/** Structure used to pass data into the scaling routines */
typedef struct Kappa_compactSearchItems {
    Uint1*              query;                  /* this is only an alias */
    int                 qlength;
    Boolean             gapped_calculation;
    int                 alphabetSize;
    int**               matrix;                 /* this is only an alias */
    double              lambda;
    Blast_KarlinBlk**   kbp_std;                /* this is only an alias */
    Blast_KarlinBlk**   kbp_psi;                /* this is only an alias */
    Blast_KarlinBlk**   kbp_gap_std;            /* this is only an alias */
    Blast_KarlinBlk**   kbp_gap_psi;            /* this is only an alias */
    double              lambda_ideal;
    double              K_ideal;
    double*             standardProb;

} Kappa_compactSearchItems;

Kappa_posSearchItems*
Kappa_posSearchItemsNew(unsigned int queryLenth,
                        const char* matrix_name,
                        int** posPrivateMatrix,
                        double** posFreqs);

Kappa_posSearchItems*
Kappa_posSearchItemsFree(Kappa_posSearchItems* posSearchItems);

Kappa_compactSearchItems*
Kappa_compactSearchItemsNew(Uint1* query, unsigned int queryLength, 
                            int** standardSubstitutionMatrix, 
                            BlastScoreBlk* sbp);

Kappa_compactSearchItems*
Kappa_compactSearchItemsFree(Kappa_compactSearchItems* compactSearchItems);

/** Copied from posit2.c 
 * @return 0 on success, 1 on failure
 */
int Kappa_impalaScaling(Kappa_posSearchItems* posSearch,
                        Kappa_compactSearchItems* compactSearch,
                        double scalingFactor,
                        Boolean doBinarySearch,
                        BlastScoreBlk* sbp);

#ifdef __cplusplus
}
#endif

/*
 * ===========================================================================
 *
 * $Log$
 * Revision 1.1  2005/02/14 14:05:53  camacho
 * Initial revision
 *
 *
 * ===========================================================================
 */

#endif /* !ALGO_BLAST_CORE__BLAST_POSIT__H */

