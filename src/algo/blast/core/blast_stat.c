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
 * Author: Tom Madden
 *
 */

/** @file blast_stat.c
 * Functions to calculate BLAST probabilities etc.
 * Detailed Contents:
 *
 * - allocate and deallocate structures used by BLAST to calculate
 * probabilities etc.
 *
 * - calculate residue frequencies for query and "average" database.
 *
 * - read in matrix or load it from memory.
 *
 *  - calculate sum-p from a collection of HSP's, for both the case
 *   of a "small" gap and a "large" gap, when give a total score and the
 *   number of HSP's.
 *
 * - calculate expect values for p-values.
 *
 * - calculate pseuod-scores from p-values.
 *
 */

#ifndef SKIP_DOXYGEN_PROCESSING
static char const rcsid[] = 
    "$Id$";
#endif /* SKIP_DOXYGEN_PROCESSING */

#include <algo/blast/core/blast_stat.h>
#include <algo/blast/core/blast_util.h>
#include <util/tables/raw_scoremat.h>
#include <algo/blast/core/blast_encoding.h>
#include "blast_psi_priv.h"

#define BLAST_SCORE_RANGE_MAX   (BLAST_SCORE_MAX - BLAST_SCORE_MIN) /**< maximum allowed range of BLAST scores. */

/****************************************************************************
For more accuracy in the calculation of K, set K_SUMLIMIT to 0.00001.
For high speed in the calculation of K, use a K_SUMLIMIT of 0.001
Note:  statistical significance is often not greatly affected by the value
of K, so high accuracy is generally unwarranted.
*****************************************************************************/
#define BLAST_KARLIN_K_SUMLIMIT_DEFAULT 0.0001 /**< K_SUMLIMIT_DEFAULT == sumlimit used in BlastKarlinLHtoK() */

#define BLAST_KARLIN_LAMBDA_ACCURACY_DEFAULT    (1.e-5) /**< LAMBDA_ACCURACY_DEFAULT == accuracy to which Lambda should be calc'd */

#define BLAST_KARLIN_LAMBDA_ITER_DEFAULT        17 /**< LAMBDA_ITER_DEFAULT == no. of iterations in LambdaBis = ln(accuracy)/ln(2)*/

#define BLAST_KARLIN_LAMBDA0_DEFAULT    0.5 /**< Initial guess for the value of Lambda in BlastKarlinLambdaNR */

#define BLAST_KARLIN_K_ITER_MAX 100 /**< upper limit on iterations for BlastKarlinLHtoK */

/** Number of statistical parameters in each row of the precomputed tables. */
#define BLAST_NUM_STAT_VALUES 8

/** Holds values (gap-opening, extension, etc.) for a matrix. */
typedef double array_of_8[BLAST_NUM_STAT_VALUES];

/** Used to temporarily store matrix values for retrieval. */
typedef struct MatrixInfo {
   char*    name;       /**< name of matrix (e.g., BLOSUM90). */
   array_of_8  *values;    /**< The values (gap-opening, extension etc.). */
   Int4     *prefs;        /**< Preferences for display. */
   Int4     max_number_values;   /**< number of values (e.g., BLOSUM90_VALUES_MAX). */
} MatrixInfo;


/**************************************************************************************

How the statistical parameters for the matrices are stored:
-----------------------------------------------------------
They parameters are stored in a two-dimensional array double (i.e., 
doubles, which has as it's first dimensions the number of different 
gap existence and extension combinations and as it's second dimension 8.
The eight different columns specify:

1.) gap existence penalty (INT2_MAX denotes infinite).
2.) gap extension penalty (INT2_MAX denotes infinite).
3.) decline to align penalty (INT2_MAX denotes infinite).
4.) Lambda
5.) K
6.) H
7.) alpha
8.) beta

(Items 4-8 are explained in:
Altschul SF, Bundschuh R, Olsen R, Hwa T.
The estimation of statistical parameters for local alignment score distributions.
Nucleic Acids Res. 2001 Jan 15;29(2):351-61.).

Take BLOSUM45 (below) as an example.  Currently (5/17/02) there are
14 different allowed combinations (specified by "#define BLOSUM45_VALUES_MAX 14").
The first row in the array "blosum45_values" has INT2_MAX (i.e., 32767) for gap 
existence, extension, and decline-to-align penalties.  For all practical purposes
this value is large enough to be infinite, so the alignments will be ungapped.
BLAST may also use this value (INT2_MAX) as a signal to skip gapping, so a
different value should not be used if the intent is to have gapless extensions.
The next row is for the gap existence penalty 13 and the extension penalty 3.
The decline-to-align penalty is only supported in a few cases, so it is normally
set to INT2_MAX.


How to add a new matrix to blast_stat.c:
--------------------------------------
To add a new matrix to blast_stat.c it is necessary to complete 
four steps.  As an example consider adding the matrix
called TESTMATRIX

1.) add a define specifying how many different existence and extensions
penalties are allowed, so it would be necessary to add the line:

#define TESTMATRIX_VALUES_MAX 14

if 14 values were to be allowed.

2.) add a two-dimensional array to contain the statistical parameters:

static array_of_8 testmatrix_values[TESTMATRIX_VALUES_MAX] ={ ...

3.) add a "prefs" array that should hint about the "optimal" 
gap existence and extension penalties:

static Int4 testmatrix_prefs[TESTMATRIX_VALUES_MAX] = {
BLAST_MATRIX_NOMINAL,
...
};

4.) Go to the function BlastLoadMatrixValues (in this file) and
add two lines before the return at the end of the function: 

        matrix_info = MatrixInfoNew("TESTMATRIX", testmatrix_values, testmatrix_prefs, TESTMATRIX_VALUES_MAX);
        ListNodeAddPointer(&retval, 0, matrix_info);



***************************************************************************************/


   
   

#define BLOSUM45_VALUES_MAX 14 /**< Number of different combinations supported for BLOSUM45. */
static array_of_8 blosum45_values[BLOSUM45_VALUES_MAX] = {
    {(double) INT2_MAX, (double) INT2_MAX, (double) INT2_MAX, 0.2291, 0.0924, 0.2514, 0.9113, -5.7},
    {13, 3, (double) INT2_MAX, 0.207, 0.049, 0.14, 1.5, -22},
    {12, 3, (double) INT2_MAX, 0.199, 0.039, 0.11, 1.8, -34},
    {11, 3, (double) INT2_MAX, 0.190, 0.031, 0.095, 2.0, -38},
    {10, 3, (double) INT2_MAX, 0.179, 0.023, 0.075, 2.4, -51},
    {16, 2, (double) INT2_MAX, 0.210, 0.051, 0.14, 1.5, -24},
    {15, 2, (double) INT2_MAX, 0.203, 0.041, 0.12, 1.7, -31},
    {14, 2, (double) INT2_MAX, 0.195, 0.032, 0.10, 1.9, -36},
    {13, 2, (double) INT2_MAX, 0.185, 0.024, 0.084, 2.2, -45},
    {12, 2, (double) INT2_MAX, 0.171, 0.016, 0.061, 2.8, -65},
    {19, 1, (double) INT2_MAX, 0.205, 0.040, 0.11, 1.9, -43},
    {18, 1, (double) INT2_MAX, 0.198, 0.032, 0.10, 2.0, -43},
    {17, 1, (double) INT2_MAX, 0.189, 0.024, 0.079, 2.4, -57},
    {16, 1, (double) INT2_MAX, 0.176, 0.016, 0.063, 2.8, -67},
};  /**< Supported values (gap-existence, extension, etc.) for BLOSUM45. */

static Int4 blosum45_prefs[BLOSUM45_VALUES_MAX] = {
BLAST_MATRIX_NOMINAL,
BLAST_MATRIX_NOMINAL,
BLAST_MATRIX_NOMINAL,
BLAST_MATRIX_NOMINAL,
BLAST_MATRIX_NOMINAL,
BLAST_MATRIX_NOMINAL,
BLAST_MATRIX_NOMINAL,
BLAST_MATRIX_BEST,
BLAST_MATRIX_NOMINAL,
BLAST_MATRIX_NOMINAL,
BLAST_MATRIX_NOMINAL,
BLAST_MATRIX_NOMINAL,
BLAST_MATRIX_NOMINAL,
BLAST_MATRIX_NOMINAL
};  /**< Quality values for BLOSUM45 matrix, each element corresponds to same element number in array blosum45_values */


#define BLOSUM50_VALUES_MAX 16 /**< Number of different combinations supported for BLOSUM50. */
static array_of_8 blosum50_values[BLOSUM50_VALUES_MAX] = {
    {(double) INT2_MAX, (double) INT2_MAX, (double) INT2_MAX, 0.2318, 0.112, 0.3362, 0.6895, -4.0},
    {13, 3, (double) INT2_MAX, 0.212, 0.063, 0.19, 1.1, -16},
    {12, 3, (double) INT2_MAX, 0.206, 0.055, 0.17, 1.2, -18},
    {11, 3, (double) INT2_MAX, 0.197, 0.042, 0.14, 1.4, -25},
    {10, 3, (double) INT2_MAX, 0.186, 0.031, 0.11, 1.7, -34},
    {9, 3, (double) INT2_MAX, 0.172, 0.022, 0.082, 2.1, -48},
    {16, 2, (double) INT2_MAX, 0.215, 0.066, 0.20, 1.05, -15},
    {15, 2, (double) INT2_MAX, 0.210, 0.058, 0.17, 1.2, -20},
    {14, 2, (double) INT2_MAX, 0.202, 0.045, 0.14, 1.4, -27},
    {13, 2, (double) INT2_MAX, 0.193, 0.035, 0.12, 1.6, -32},
    {12, 2, (double) INT2_MAX, 0.181, 0.025, 0.095, 1.9, -41},
    {19, 1, (double) INT2_MAX, 0.212, 0.057, 0.18, 1.2, -21},
    {18, 1, (double) INT2_MAX, 0.207, 0.050, 0.15, 1.4, -28},
    {17, 1, (double) INT2_MAX, 0.198, 0.037, 0.12, 1.6, -33},
    {16, 1, (double) INT2_MAX, 0.186, 0.025, 0.10, 1.9, -42},
    {15, 1, (double) INT2_MAX, 0.171, 0.015, 0.063, 2.7, -76},
};  /**< Supported values (gap-existence, extension, etc.) for BLOSUM50. */

static Int4 blosum50_prefs[BLOSUM50_VALUES_MAX] = {
BLAST_MATRIX_NOMINAL,
BLAST_MATRIX_NOMINAL,
BLAST_MATRIX_NOMINAL,
BLAST_MATRIX_NOMINAL,
BLAST_MATRIX_NOMINAL,
BLAST_MATRIX_NOMINAL,
BLAST_MATRIX_NOMINAL,
BLAST_MATRIX_NOMINAL,
BLAST_MATRIX_NOMINAL,
BLAST_MATRIX_BEST,
BLAST_MATRIX_NOMINAL,
BLAST_MATRIX_NOMINAL,
BLAST_MATRIX_NOMINAL,
BLAST_MATRIX_NOMINAL,
BLAST_MATRIX_NOMINAL,
BLAST_MATRIX_NOMINAL
};  /**< Quality values for BLOSUM50 matrix, each element corresponds to same element number in array blosum50_values */

#define BLOSUM62_VALUES_MAX 12 /**< Number of different combinations supported for BLOSUM62. */
static array_of_8 blosum62_values[BLOSUM62_VALUES_MAX] = {
    {(double) INT2_MAX, (double) INT2_MAX, (double) INT2_MAX, 0.3176, 0.134, 0.4012, 0.7916, -3.2},
    {11, 2, (double) INT2_MAX, 0.297, 0.082, 0.27, 1.1, -10},
    {10, 2, (double) INT2_MAX, 0.291, 0.075, 0.23, 1.3, -15},
    {9, 2, (double) INT2_MAX, 0.279, 0.058, 0.19, 1.5, -19},
    {8, 2, (double) INT2_MAX, 0.264, 0.045, 0.15, 1.8, -26},
    {7, 2, (double) INT2_MAX, 0.239, 0.027, 0.10, 2.5, -46},
    {6, 2, (double) INT2_MAX, 0.201, 0.012, 0.061, 3.3, -58},
    {13, 1, (double) INT2_MAX, 0.292, 0.071, 0.23, 1.2, -11},
    {12, 1, (double) INT2_MAX, 0.283, 0.059, 0.19, 1.5, -19},
    {11, 1, (double) INT2_MAX, 0.267, 0.041, 0.14, 1.9, -30},
    {10, 1, (double) INT2_MAX, 0.243, 0.024, 0.10, 2.5, -44},
    {9, 1, (double) INT2_MAX, 0.206, 0.010, 0.052, 4.0, -87},
}; /**< Supported values (gap-existence, extension, etc.) for BLOSUM62. */

static Int4 blosum62_prefs[BLOSUM62_VALUES_MAX] = {
    BLAST_MATRIX_NOMINAL,
    BLAST_MATRIX_NOMINAL,
    BLAST_MATRIX_NOMINAL,
    BLAST_MATRIX_NOMINAL,
    BLAST_MATRIX_NOMINAL,
    BLAST_MATRIX_NOMINAL,
    BLAST_MATRIX_NOMINAL,
    BLAST_MATRIX_NOMINAL,
    BLAST_MATRIX_NOMINAL,
    BLAST_MATRIX_BEST,
    BLAST_MATRIX_NOMINAL,
    BLAST_MATRIX_NOMINAL,
};  /**< Quality values for BLOSUM62 matrix, each element corresponds to same element number in array blosum62_values */


#define BLOSUM80_VALUES_MAX 10 /**< Number of different combinations supported for BLOSUM80. */
static array_of_8 blosum80_values[BLOSUM80_VALUES_MAX] = {
    {(double) INT2_MAX, (double) INT2_MAX, (double) INT2_MAX, 0.3430, 0.177, 0.6568, 0.5222, -1.6},
    {25, 2, (double) INT2_MAX, 0.342, 0.17, 0.66, 0.52, -1.6},
    {13, 2, (double) INT2_MAX, 0.336, 0.15, 0.57, 0.59, -3},
    {9, 2, (double) INT2_MAX, 0.319, 0.11, 0.42, 0.76, -6},
    {8, 2, (double) INT2_MAX, 0.308, 0.090, 0.35, 0.89, -9},
    {7, 2, (double) INT2_MAX, 0.293, 0.070, 0.27, 1.1, -14},
    {6, 2, (double) INT2_MAX, 0.268, 0.045, 0.19, 1.4, -19},
    {11, 1, (double) INT2_MAX, 0.314, 0.095, 0.35, 0.90, -9},
    {10, 1, (double) INT2_MAX, 0.299, 0.071, 0.27, 1.1, -14},
    {9, 1, (double) INT2_MAX, 0.279, 0.048, 0.20, 1.4, -19},
}; /**< Supported values (gap-existence, extension, etc.) for BLOSUM80. */

static Int4 blosum80_prefs[BLOSUM80_VALUES_MAX] = {
    BLAST_MATRIX_NOMINAL,
    BLAST_MATRIX_NOMINAL,
    BLAST_MATRIX_NOMINAL,
    BLAST_MATRIX_NOMINAL,
    BLAST_MATRIX_NOMINAL,
    BLAST_MATRIX_NOMINAL,
    BLAST_MATRIX_NOMINAL,
    BLAST_MATRIX_NOMINAL,
    BLAST_MATRIX_BEST,
    BLAST_MATRIX_NOMINAL
};  /**< Quality values for BLOSUM80 matrix, each element corresponds to same element number in array blosum80_values */

#define BLOSUM90_VALUES_MAX 8 /**< Number of different combinations supported for BLOSUM90. */
static array_of_8 blosum90_values[BLOSUM90_VALUES_MAX] = {
    {(double) INT2_MAX, (double) INT2_MAX, (double) INT2_MAX, 0.3346, 0.190, 0.7547, 0.4434, -1.4},
    {9, 2, (double) INT2_MAX, 0.310, 0.12, 0.46, 0.67, -6},
    {8, 2, (double) INT2_MAX, 0.300, 0.099, 0.39, 0.76, -7},
    {7, 2, (double) INT2_MAX, 0.283, 0.072, 0.30, 0.93, -11},
    {6, 2, (double) INT2_MAX, 0.259, 0.048, 0.22, 1.2, -16},
    {11, 1, (double) INT2_MAX, 0.302, 0.093, 0.39, 0.78, -8},
    {10, 1, (double) INT2_MAX, 0.290, 0.075, 0.28, 1.04, -15},
    {9, 1, (double) INT2_MAX, 0.265, 0.044, 0.20, 1.3, -19},
};  /**< Supported values (gap-existence, extension, etc.) for BLOSUM90. */

static Int4 blosum90_prefs[BLOSUM90_VALUES_MAX] = {
   BLAST_MATRIX_NOMINAL,
   BLAST_MATRIX_NOMINAL,
   BLAST_MATRIX_NOMINAL,
   BLAST_MATRIX_NOMINAL,
   BLAST_MATRIX_NOMINAL,
   BLAST_MATRIX_NOMINAL,
   BLAST_MATRIX_BEST,
   BLAST_MATRIX_NOMINAL
};  /**< Quality values for BLOSUM90 matrix, each element corresponds to same element number in array blosum90_values */

#define PAM250_VALUES_MAX 16 /**< Number of different combinations supported for PAM250. */
static array_of_8 pam250_values[PAM250_VALUES_MAX] = {
    {(double) INT2_MAX, (double) INT2_MAX, (double) INT2_MAX, 0.2252, 0.0868, 0.2223, 0.98, -5.0},
    {15, 3, (double) INT2_MAX, 0.205, 0.049, 0.13, 1.6, -23},
    {14, 3, (double) INT2_MAX, 0.200, 0.043, 0.12, 1.7, -26},
    {13, 3, (double) INT2_MAX, 0.194, 0.036, 0.10, 1.9, -31},
    {12, 3, (double) INT2_MAX, 0.186, 0.029, 0.085, 2.2, -41},
    {11, 3, (double) INT2_MAX, 0.174, 0.020, 0.070, 2.5, -48},
    {17, 2, (double) INT2_MAX, 0.204, 0.047, 0.12, 1.7, -28},
    {16, 2, (double) INT2_MAX, 0.198, 0.038, 0.11, 1.8, -29},
    {15, 2, (double) INT2_MAX, 0.191, 0.031, 0.087, 2.2, -44},
    {14, 2, (double) INT2_MAX, 0.182, 0.024, 0.073, 2.5, -53},
    {13, 2, (double) INT2_MAX, 0.171, 0.017, 0.059, 2.9, -64},
    {21, 1, (double) INT2_MAX, 0.205, 0.045, 0.11, 1.8, -34},
    {20, 1, (double) INT2_MAX, 0.199, 0.037, 0.10, 1.9, -35},
    {19, 1, (double) INT2_MAX, 0.192, 0.029, 0.083, 2.3, -52},
    {18, 1, (double) INT2_MAX, 0.183, 0.021, 0.070, 2.6, -60},
    {17, 1, (double) INT2_MAX, 0.171, 0.014, 0.052, 3.3, -86},
}; /**< Supported values (gap-existence, extension, etc.) for PAM250. */

static Int4 pam250_prefs[PAM250_VALUES_MAX] = {
BLAST_MATRIX_NOMINAL,
BLAST_MATRIX_NOMINAL,
BLAST_MATRIX_NOMINAL,
BLAST_MATRIX_NOMINAL,
BLAST_MATRIX_NOMINAL,
BLAST_MATRIX_NOMINAL,
BLAST_MATRIX_NOMINAL,
BLAST_MATRIX_NOMINAL,
BLAST_MATRIX_NOMINAL,
BLAST_MATRIX_BEST,
BLAST_MATRIX_NOMINAL,
BLAST_MATRIX_NOMINAL,
BLAST_MATRIX_NOMINAL,
BLAST_MATRIX_NOMINAL,
BLAST_MATRIX_NOMINAL,
BLAST_MATRIX_NOMINAL
};  /**< Quality values for PAM250 matrix, each element corresponds to same element number in array pam250_values */

#define PAM30_VALUES_MAX 7 /**< Number of different combinations supported for PAM30. */
static array_of_8 pam30_values[PAM30_VALUES_MAX] = {
    {(double) INT2_MAX, (double) INT2_MAX, (double) INT2_MAX, 0.3400, 0.283, 1.754, 0.1938, -0.3},
    {7, 2, (double) INT2_MAX, 0.305, 0.15, 0.87, 0.35, -3},
    {6, 2, (double) INT2_MAX, 0.287, 0.11, 0.68, 0.42, -4},
    {5, 2, (double) INT2_MAX, 0.264, 0.079, 0.45, 0.59, -7},
    {10, 1, (double) INT2_MAX, 0.309, 0.15, 0.88, 0.35, -3},
    {9, 1, (double) INT2_MAX, 0.294, 0.11, 0.61, 0.48, -6},
    {8, 1, (double) INT2_MAX, 0.270, 0.072, 0.40, 0.68, -10},
}; /**< Supported values (gap-existence, extension, etc.) for PAM30. */

static Int4 pam30_prefs[PAM30_VALUES_MAX] = {
BLAST_MATRIX_NOMINAL,
BLAST_MATRIX_NOMINAL,
BLAST_MATRIX_NOMINAL,
BLAST_MATRIX_NOMINAL,
BLAST_MATRIX_NOMINAL,
BLAST_MATRIX_BEST,
BLAST_MATRIX_NOMINAL,
};  /**< Quality values for PAM30 matrix, each element corresponds to same element number in array pam30_values */


#define PAM70_VALUES_MAX 7 /**< Number of different combinations supported for PAM70. */
static array_of_8 pam70_values[PAM70_VALUES_MAX] = {
    {(double) INT2_MAX, (double) INT2_MAX, (double) INT2_MAX, 0.3345, 0.229, 1.029, 0.3250,   -0.7},
    {8, 2, (double) INT2_MAX, 0.301, 0.12, 0.54, 0.56, -5},
    {7, 2, (double) INT2_MAX, 0.286, 0.093, 0.43, 0.67, -7},
    {6, 2, (double) INT2_MAX, 0.264, 0.064, 0.29, 0.90, -12},
    {11, 1, (double) INT2_MAX, 0.305, 0.12, 0.52, 0.59, -6},
    {10, 1, (double) INT2_MAX, 0.291, 0.091, 0.41, 0.71, -9},
    {9, 1, (double) INT2_MAX, 0.270, 0.060, 0.28, 0.97, -14},
}; /**< Supported values (gap-existence, extension, etc.) for PAM70. */

static Int4 pam70_prefs[PAM70_VALUES_MAX] = {
BLAST_MATRIX_NOMINAL,
BLAST_MATRIX_NOMINAL,
BLAST_MATRIX_NOMINAL,
BLAST_MATRIX_NOMINAL,
BLAST_MATRIX_NOMINAL,
BLAST_MATRIX_BEST,
BLAST_MATRIX_NOMINAL
};  /**< Quality values for PAM70 matrix, each element corresponds to same element number in array pam70_values */



#define BLOSUM62_20_VALUES_MAX 65 /**< Number of different combinations supported for BLOSUM62 with 1/20 bit scaling. */
static array_of_8 blosum62_20_values[BLOSUM62_20_VALUES_MAX] = {
    {(double) INT2_MAX, (double) INT2_MAX, (double) INT2_MAX, 0.03391, 0.125, 0.4544, 0.07462, -3.2},
    {100, 12, (double) INT2_MAX, 0.0300, 0.056, 0.21, 0.14, -15},
    {95, 12, (double) INT2_MAX, 0.0291, 0.047, 0.18, 0.16, -20},
    {90, 12, (double) INT2_MAX, 0.0280, 0.038, 0.15, 0.19, -28},
    {85, 12, (double) INT2_MAX, 0.0267, 0.030, 0.13, 0.21, -31},
    {80, 12, (double) INT2_MAX, 0.0250, 0.021, 0.10, 0.25, -39},
    {105, 11, (double) INT2_MAX, 0.0301, 0.056, 0.22, 0.14, -16},
    {100, 11, (double) INT2_MAX, 0.0294, 0.049, 0.20, 0.15, -17},
    {95, 11, (double) INT2_MAX, 0.0285, 0.042, 0.16, 0.18, -25},
    {90, 11, (double) INT2_MAX, 0.0271, 0.031, 0.14, 0.20, -28},
    {85, 11, (double) INT2_MAX, 0.0256, 0.023, 0.10, 0.26, -46},
    {115, 10, (double) INT2_MAX, 0.0308, 0.062, 0.22, 0.14, -20},
    {110, 10, (double) INT2_MAX, 0.0302, 0.056, 0.19, 0.16, -26},
    {105, 10, (double) INT2_MAX, 0.0296, 0.050, 0.17, 0.17, -27},
    {100, 10, (double) INT2_MAX, 0.0286, 0.041, 0.15, 0.19, -32},
    {95, 10, (double) INT2_MAX, 0.0272, 0.030, 0.13, 0.21, -35},
    {90, 10, (double) INT2_MAX, 0.0257, 0.022, 0.11, 0.24, -40},
    {85, 10, (double) INT2_MAX, 0.0242, 0.017, 0.083, 0.29, -51},
    {115, 9, (double) INT2_MAX, 0.0306, 0.061, 0.24, 0.13, -14},
    {110, 9, (double) INT2_MAX, 0.0299, 0.053, 0.19, 0.16, -23},
    {105, 9, (double) INT2_MAX, 0.0289, 0.043, 0.17, 0.17, -23},
    {100, 9, (double) INT2_MAX, 0.0279, 0.036, 0.14, 0.20, -31},
    {95, 9, (double) INT2_MAX, 0.0266, 0.028, 0.12, 0.23, -37},
    {120, 8, (double) INT2_MAX, 0.0307, 0.062, 0.22, 0.14, -18},
    {115, 8, (double) INT2_MAX, 0.0300, 0.053, 0.20, 0.15, -19},
    {110, 8, (double) INT2_MAX, 0.0292, 0.046, 0.17, 0.17, -23},
    {105, 8, (double) INT2_MAX, 0.0280, 0.035, 0.14, 0.20, -31},
    {100, 8, (double) INT2_MAX, 0.0266, 0.026, 0.12, 0.23, -37},
    {125, 7, (double) INT2_MAX, 0.0306, 0.058, 0.22, 0.14, -18},
    {120, 7, (double) INT2_MAX, 0.0300, 0.052, 0.19, 0.16, -23},
    {115, 7, (double) INT2_MAX, 0.0292, 0.044, 0.17, 0.17, -24},
    {110, 7, (double) INT2_MAX, 0.0279, 0.032, 0.14, 0.20, -31},
    {105, 7, (double) INT2_MAX, 0.0267, 0.026, 0.11, 0.24, -41},
    {120,10,5, 0.0298, 0.049, 0.19, 0.16, -21},
    {115,10,5, 0.0290, 0.042, 0.16, 0.18, -25},
    {110,10,5, 0.0279, 0.033, 0.13, 0.21, -32},
    {105,10,5, 0.0264, 0.024, 0.10, 0.26, -46},
    {100,10,5, 0.0250, 0.018, 0.081, 0.31, -56},
    {125,10,4, 0.0301, 0.053, 0.18, 0.17, -25},
    {120,10,4, 0.0292, 0.043, 0.15, 0.20, -33},
    {115,10,4, 0.0282, 0.035, 0.13, 0.22, -36},
    {110,10,4, 0.0270, 0.027, 0.11, 0.25, -41},
    {105,10,4, 0.0254, 0.020, 0.079, 0.32, -60},
    {130,10,3, 0.0300, 0.051, 0.17, 0.18, -27},
    {125,10,3, 0.0290, 0.040, 0.13, 0.22, -38},
    {120,10,3, 0.0278, 0.030, 0.11, 0.25, -44},
    {115,10,3, 0.0267, 0.025, 0.092, 0.29, -52},
    {110,10,3, 0.0252, 0.018, 0.070, 0.36, -70},
    {135,10,2, 0.0292, 0.040, 0.13, 0.22, -35},
    {130,10,2, 0.0283, 0.034, 0.10, 0.28, -51},
    {125,10,2, 0.0269, 0.024, 0.077, 0.35, -71},
    {120,10,2, 0.0253, 0.017, 0.059, 0.43, -90},
    {115,10,2, 0.0234, 0.011, 0.043, 0.55, -121},
    {100,14,3, 0.0258, 0.023, 0.087, 0.33, -59},
    {105,13,3, 0.0263, 0.024, 0.085, 0.31, -57},
    {110,12,3, 0.0271, 0.028, 0.093, 0.29, -54},
    {115,11,3, 0.0275, 0.030, 0.10, 0.27, -49},
    {125,9,3, 0.0283, 0.034, 0.12, 0.23, -38},
    {130,8,3, 0.0287, 0.037, 0.12, 0.23, -40},
    {125,7,3, 0.0287, 0.036, 0.12, 0.24, -44},
    {140,6,3, 0.0285, 0.033, 0.12, 0.23, -40},
    {105,14,3, 0.0270, 0.028, 0.10, 0.27, -46},
    {110,13,3, 0.0279, 0.034, 0.10, 0.27, -50},
    {115,12,3, 0.0282, 0.035, 0.12, 0.24, -42},
    {120,11,3, 0.0286, 0.037, 0.12, 0.24, -44},
}; /**< Supported values (gap-existence, extension, etc.) for BLOSUM62_20. */

static Int4 blosum62_20_prefs[BLOSUM62_20_VALUES_MAX] = {
BLAST_MATRIX_NOMINAL,
BLAST_MATRIX_NOMINAL,
BLAST_MATRIX_NOMINAL,
BLAST_MATRIX_NOMINAL,
BLAST_MATRIX_NOMINAL,
BLAST_MATRIX_NOMINAL,
BLAST_MATRIX_NOMINAL,
BLAST_MATRIX_NOMINAL,
BLAST_MATRIX_NOMINAL,
BLAST_MATRIX_NOMINAL,
BLAST_MATRIX_NOMINAL,
BLAST_MATRIX_NOMINAL,
BLAST_MATRIX_NOMINAL,
BLAST_MATRIX_NOMINAL,
BLAST_MATRIX_NOMINAL,
BLAST_MATRIX_NOMINAL,
BLAST_MATRIX_NOMINAL,
BLAST_MATRIX_NOMINAL,
BLAST_MATRIX_NOMINAL,
BLAST_MATRIX_NOMINAL,
BLAST_MATRIX_NOMINAL,
BLAST_MATRIX_NOMINAL,
BLAST_MATRIX_NOMINAL,
BLAST_MATRIX_NOMINAL,
BLAST_MATRIX_NOMINAL,
BLAST_MATRIX_NOMINAL,
BLAST_MATRIX_NOMINAL,
BLAST_MATRIX_NOMINAL,
BLAST_MATRIX_NOMINAL,
BLAST_MATRIX_NOMINAL,
BLAST_MATRIX_NOMINAL,
BLAST_MATRIX_NOMINAL,
BLAST_MATRIX_NOMINAL,
BLAST_MATRIX_NOMINAL,
BLAST_MATRIX_NOMINAL,
BLAST_MATRIX_NOMINAL,
BLAST_MATRIX_NOMINAL,
BLAST_MATRIX_NOMINAL,
BLAST_MATRIX_NOMINAL,
BLAST_MATRIX_NOMINAL,
BLAST_MATRIX_NOMINAL,
BLAST_MATRIX_NOMINAL,
BLAST_MATRIX_NOMINAL,
BLAST_MATRIX_NOMINAL,
BLAST_MATRIX_NOMINAL,
BLAST_MATRIX_BEST,
BLAST_MATRIX_NOMINAL,
BLAST_MATRIX_NOMINAL,
BLAST_MATRIX_NOMINAL,
BLAST_MATRIX_NOMINAL,
BLAST_MATRIX_NOMINAL,
BLAST_MATRIX_NOMINAL,
BLAST_MATRIX_NOMINAL,
BLAST_MATRIX_NOMINAL,
BLAST_MATRIX_NOMINAL,
BLAST_MATRIX_NOMINAL,
BLAST_MATRIX_NOMINAL,
BLAST_MATRIX_NOMINAL,
BLAST_MATRIX_NOMINAL,
BLAST_MATRIX_NOMINAL,
BLAST_MATRIX_NOMINAL,
BLAST_MATRIX_NOMINAL,
BLAST_MATRIX_NOMINAL,
BLAST_MATRIX_NOMINAL,
BLAST_MATRIX_NOMINAL
};  /**< Quality values for BLOSUM62_20 matrix, each element corresponds to same element number in array blosum62_20_values */

/** Supported substitution and gap costs with corresponding quality values
 * for nucleotide sequence comparisons.
 * NB: the values 0 and 0 for the gap costs are treated as the defaults used for
 * the greedy gapped extension, i.e. 
 * gap opening = 0, 
 * gap extension = 1/2 match - mismatch.
 * 
 * The fields are:
 * 
 * 1. Gap opening cost,
 * 2. Gap extension cost,
 * 3. Lambda,
 * 4. K,
 * 5. H,
 * 6. Alpha,
 * 7. Beta,
 * 8. Theta
 */

/** Karlin-Altschul parameter values for substitution scores 1 and -4. */
static const array_of_8 blastn_values_1_4[] = {
    { 0, 0, 1.383, 0.738, 1.36, 1.02,  0, 100 },
    { 1, 2,  1.36,  0.67,  1.2,  1.1,  0,  98 }, 
    { 0, 2,  1.26,  0.43, 0.90,  1.4, -1,  91 },
    { 2, 1,  1.35,  0.61,  1.1,  1.2, -1,  98 },
    { 1, 1,  1.22,  0.35, 0.72,  1.7, -3,  88 }
};

/** Karlin-Altschul parameter values for substitution scores 2 and -7. 
 * These parameters can only be applied to even scores. Any odd score must be
 * rounded down to the nearest even number before calculating the e-value.
 */
static const array_of_8 blastn_values_2_7[] = {
    { 0, 0,  0.69, 0.73, 1.34, 0.515,  0, 100 },
    { 2, 4,  0.68, 0.67,  1.2,  0.55,  0,  99 }, 
    { 0, 4,  0.63, 0.43, 0.90,   0.7, -1,  91 },
    { 4, 2, 0.675, 0.62,  1.1,   0.6, -1,  98 },
    { 2, 2,  0.61, 0.35, 0.72,   1.7, -3,  88 }
};

/** Karlin-Altschul parameter values for substitution scores 1 and -3. */
static const array_of_8 blastn_values_1_3[] = {
    { 0, 0, 1.374, 0.711, 1.31, 1.05,  0, 100 },
    { 2, 2,  1.37,  0.70,  1.2,  1.1,  0,  99 },
    { 1, 2,  1.35,  0.64,  1.1,  1.2, -1,  98 },
    { 0, 2,  1.25,  0.42, 0.83,  1.5, -2,  91 },
    { 2, 1,  1.34,  0.60,  1.1,  1.2, -1,  97 },
    { 1, 1,  1.21,  0.34, 0.71,  1.7, -2,  88 }
};

/** Karlin-Altschul parameter values for substitution scores 2 and -5.
 * These parameters can only be applied to even scores. Any odd score must be
 * rounded down to the nearest even number before calculating the e-value.
 */
static const array_of_8 blastn_values_2_5[] = {
    { 0, 0, 0.675, 0.65,  1.1,  0.6, -1, 99 },
    { 2, 4,  0.67, 0.59,  1.1,  0.6, -1, 98 },
    { 0, 4,  0.62, 0.39, 0.78,  0.8, -2, 91 },
    { 4, 2,  0.67, 0.61,  1.0, 0.65, -2, 98 },
    { 2, 2,  0.56, 0.32, 0.59, 0.95, -4, 82 }
};

/** Karlin-Altschul parameter values for substitution scores 1 and -2. */
static const array_of_8 blastn_values_1_2[] = {
    { 0, 0, 1.28, 0.46, 0.85, 1.5, -2, 96 },
    { 2, 2, 1.33, 0.62,  1.1, 1.2,  0, 99 },
    { 1, 2, 1.30, 0.52, 0.93, 1.4, -2, 97 }, 
    { 0, 2, 1.19, 0.34, 0.66, 1.8, -3, 89 },
    { 3, 1, 1.32, 0.57,  1.0, 1.3, -1, 99 }, 
    { 2, 1, 1.29, 0.49, 0.92, 1.4, -1, 96 }, 
    { 1, 1, 1.14, 0.26, 0.52, 2.2, -5, 85 }
};

/** Karlin-Altschul parameter values for substitution scores 2 and -3.
 * These parameters can only be applied to even scores. Any odd score must be
 * rounded down to the nearest even number before calculating the e-value.
 */
static const array_of_8 blastn_values_2_3[] = {
    { 0, 0,  0.55, 0.21, 0.46,  1.2, -5, 87 },
    { 4, 4,  0.63, 0.42, 0.84, 0.75, -2, 99 },
    { 2, 4, 0.615, 0.37, 0.72, 0.85, -3, 97 },
    { 0, 4,  0.55, 0.21, 0.46,  1.2, -5, 87 },
    { 3, 3, 0.615, 0.37, 0.68,  0.9, -3, 97 },
    { 6, 2,  0.63, 0.42, 0.84, 0.75, -2, 99 },
    { 5, 2, 0.625, 0.41, 0.78,  0.8, -2, 99 },
    { 4, 2,  0.61, 0.35, 0.68,  0.9, -3, 96 },
    { 2, 2, 0.515, 0.14, 0.33, 1.55, -9, 81 }
};

/** Karlin-Altschul parameter values for substitution scores 4 and -5. */
static const array_of_8 blastn_values_4_5[] = {
    { 0, 0, 0.22, 0.061, 0.22, 1.0, -15, 74 },
    { 6, 5, 0.28,  0.21, 0.47, 0.6 , -7, 93 },
    { 5, 5, 0.27,  0.17, 0.39, 0.7,  -9, 90 },
    { 4, 5, 0.25,  0.10, 0.31, 0.8, -10, 83 },
    { 3, 5, 0.23, 0.065, 0.25, 0.9, -11, 76 }
};

/** Karlin-Altschul parameter values for substitution scores 1 and -1. */
static const array_of_8 blastn_values_1_1[] = {
    { 3,  2, 1.09,  0.31, 0.55, 2.0,  -2, 99 },
    { 2,  2, 1.07,  0.27, 0.49, 2.2,  -3, 97 }, 
    { 1,  2, 1.02,  0.21, 0.36, 2.8,  -6, 92 }, 
    { 0,  2, 0.80, 0.064, 0.17, 4.8, -16, 72 },
    { 4,  1, 1.08,  0.28, 0.54, 2.0,  -2, 98 }, 
    { 3,  1, 1.06,  0.25, 0.46, 2.3,  -4, 96 }, 
    { 2,  1, 0.99,  0.17, 0.30, 3.3, -10, 90 }
};

/** Karlin-Altschul parameter values for substitution scores 5 and -4. */
static const array_of_8 blastn_values_5_4[] = {
    { 10, 6, 0.163, 0.068, 0.16, 1.0, -19, 85 },
    {  8, 6, 0.146, 0.039, 0.11, 1.3, -29, 76 }
};

/** Deallocates SBlastScoreMatrix structure
 * @param matrix structure to deallocate [in]
 * @return NULL
 */
static SBlastScoreMatrix*
SBlastScoreMatrixFree(SBlastScoreMatrix* matrix)
{
    if ( !matrix ) {
        return NULL;
    }

    if (matrix->data) {
        matrix->data = (int**) _PSIDeallocateMatrix((void**) matrix->data, 
                                                    matrix->ncols);
    }

    sfree(matrix);
    return NULL;
}

/** Allocates a new SBlastScoreMatrix structure of the specified dimensions.
 * @param ncols number of columns [in]
 * @param nrows number of rows [in]
 * @return NULL in case of memory allocation failure, else new
 * SBlastScoreMatrix structure
 */
static SBlastScoreMatrix*
SBlastScoreMatrixNew(size_t ncols, size_t nrows)
{
    SBlastScoreMatrix* retval = NULL;

    retval = (SBlastScoreMatrix*) calloc(1, sizeof(SBlastScoreMatrix));
    if ( !retval ) {
        return SBlastScoreMatrixFree(retval);
    }

    retval->data = (int**) _PSIAllocateMatrix(ncols, nrows, sizeof(int));
    if ( !retval->data ) {
        return SBlastScoreMatrixFree(retval);
    }
    retval->ncols = ncols;
    retval->nrows = nrows;
    return retval;
}

SPsiBlastScoreMatrix*
SPsiBlastScoreMatrixFree(SPsiBlastScoreMatrix* matrix)
{
    if ( !matrix ) {
        return NULL;
    }

    if (matrix->freq_ratios) {
        matrix->freq_ratios = (double**) _PSIDeallocateMatrix((void**)
                                                   matrix->freq_ratios,
                                                   matrix->pssm->ncols);
    }

    matrix->pssm = SBlastScoreMatrixFree(matrix->pssm);
    matrix->kbp = Blast_KarlinBlkFree(matrix->kbp);
    sfree(matrix);
    return NULL;
}

SPsiBlastScoreMatrix*
SPsiBlastScoreMatrixNew(size_t ncols)
{
    SPsiBlastScoreMatrix* retval = NULL;

    retval = (SPsiBlastScoreMatrix*) calloc(1, sizeof(SPsiBlastScoreMatrix));
    if ( !retval ) {
        return SPsiBlastScoreMatrixFree(retval);
    }

    retval->pssm = SBlastScoreMatrixNew(ncols, BLASTAA_SIZE);

    if ( !retval->pssm ) {
        return SPsiBlastScoreMatrixFree(retval);
    }

    retval->freq_ratios = (double**) _PSIAllocateMatrix(ncols, BLASTAA_SIZE,
                                                        sizeof(double));
    if ( !retval->freq_ratios ) {
        return SPsiBlastScoreMatrixFree(retval);
    }

    retval->kbp = Blast_KarlinBlkNew();
    if ( !retval->kbp ) {
        return SPsiBlastScoreMatrixFree(retval);
    }

    return retval;
}

/*
   Allocates memory for the BlastScoreBlk*.
*/

BlastScoreBlk*
BlastScoreBlkNew(Uint1 alphabet, Int4 number_of_contexts)

{
   BlastScoreBlk* sbp;

   sbp = (BlastScoreBlk*) calloc(1, sizeof(BlastScoreBlk));

   if ( !sbp ) {
        return NULL;
    }

    sbp->alphabet_code = alphabet;
    if (alphabet != BLASTNA_SEQ_CODE) {
        sbp->alphabet_size = BLASTAA_SIZE;
    } else {
        sbp->alphabet_size = BLASTNA_SIZE;
    }

    /* set the alphabet type (protein or not). */
    switch (alphabet) {
    case BLASTAA_SEQ_CODE:
        sbp->protein_alphabet = TRUE;
        break;
    case BLASTNA_SEQ_CODE:
        sbp->protein_alphabet = FALSE;
        break;
    default:
        break;
    }

    sbp->matrix = SBlastScoreMatrixNew(sbp->alphabet_size, sbp->alphabet_size);
    if (sbp->matrix == NULL) {
        return BlastScoreBlkFree(sbp);
    }
    sbp->scale_factor = 1.0;
    sbp->number_of_contexts = number_of_contexts;
    sbp->sfp = (Blast_ScoreFreq**) 
     calloc(sbp->number_of_contexts, sizeof(Blast_ScoreFreq*));
    sbp->kbp_std = (Blast_KarlinBlk**)
     calloc(sbp->number_of_contexts, sizeof(Blast_KarlinBlk*));
    sbp->kbp_gap_std = (Blast_KarlinBlk**)
     calloc(sbp->number_of_contexts, sizeof(Blast_KarlinBlk*));
    sbp->kbp_psi = (Blast_KarlinBlk**)
     calloc(sbp->number_of_contexts, sizeof(Blast_KarlinBlk*));
    sbp->kbp_gap_psi = (Blast_KarlinBlk**)
     calloc(sbp->number_of_contexts, sizeof(Blast_KarlinBlk*));

   return sbp;
}

Blast_ScoreFreq*
Blast_ScoreFreqFree(Blast_ScoreFreq* sfp)
{
   if (sfp == NULL)
      return NULL;

   if (sfp->sprob0 != NULL)
      sfree(sfp->sprob0);
   sfree(sfp);
   return sfp;
}

/*
   Deallocates the Karlin Block.
*/
Blast_KarlinBlk*
Blast_KarlinBlkFree(Blast_KarlinBlk* kbp)

{
   sfree(kbp);

   return kbp;
}

BlastScoreBlk*
BlastScoreBlkFree(BlastScoreBlk* sbp)

{
    Int4 index;
    if (sbp == NULL)
        return NULL;
    
    for (index=0; index<sbp->number_of_contexts; index++) {
        if (sbp->sfp)
            sbp->sfp[index] = Blast_ScoreFreqFree(sbp->sfp[index]);
        if (sbp->kbp_std)
            sbp->kbp_std[index] = Blast_KarlinBlkFree(sbp->kbp_std[index]);
        if (sbp->kbp_gap_std)
            sbp->kbp_gap_std[index] = Blast_KarlinBlkFree(sbp->kbp_gap_std[index]);
        if (sbp->kbp_psi)
            sbp->kbp_psi[index] = Blast_KarlinBlkFree(sbp->kbp_psi[index]);
        if (sbp->kbp_gap_psi)
            sbp->kbp_gap_psi[index] = Blast_KarlinBlkFree(sbp->kbp_gap_psi[index]);
    }
    if (sbp->kbp_ideal)
        sbp->kbp_ideal = Blast_KarlinBlkFree(sbp->kbp_ideal);
    sfree(sbp->sfp);
    sfree(sbp->kbp_std);
    sfree(sbp->kbp_psi);
    sfree(sbp->kbp_gap_std);
    sfree(sbp->kbp_gap_psi);
    sbp->matrix = SBlastScoreMatrixFree(sbp->matrix);
    sbp->comments = ListNodeFreeData(sbp->comments);
    sfree(sbp->name);
    sbp->psi_matrix = SPsiBlastScoreMatrixFree(sbp->psi_matrix);
    sfree(sbp->ambiguous_res);
    sfree(sbp);
    
    return NULL;
}

/* 
   Set the ambiguous residue (e.g, 'N', 'X') in the BlastScoreBlk*.
   Convert from ncbieaa to sbp->alphabet_code (i.e., ncbistdaa) first.
*/
Int2
BLAST_ScoreSetAmbigRes(BlastScoreBlk* sbp, char ambiguous_res)

{
   Int2 index;
   Uint1* ambig_buffer;

   if (sbp == NULL)
      return 1;

   if (sbp->ambig_occupy >= sbp->ambig_size)
   {
      sbp->ambig_size += 5;
      ambig_buffer = (Uint1 *) calloc(sbp->ambig_size, sizeof(Uint1));
      for (index=0; index<sbp->ambig_occupy; index++)
      {
         ambig_buffer[index] = sbp->ambiguous_res[index];
      }
      sfree(sbp->ambiguous_res);
      sbp->ambiguous_res = ambig_buffer;
   }

   if (sbp->alphabet_code == BLASTAA_SEQ_CODE)
   {
      sbp->ambiguous_res[sbp->ambig_occupy] = 
         AMINOACID_TO_NCBISTDAA[toupper((unsigned char) ambiguous_res)];
   }
   else {
      if (sbp->alphabet_code == BLASTNA_SEQ_CODE)
         sbp->ambiguous_res[sbp->ambig_occupy] = 
            IUPACNA_TO_BLASTNA[toupper((unsigned char) ambiguous_res)];
      else if (sbp->alphabet_code == NCBI4NA_SEQ_CODE)
         sbp->ambiguous_res[sbp->ambig_occupy] = 
            IUPACNA_TO_NCBI4NA[toupper((unsigned char) ambiguous_res)];
   }
   (sbp->ambig_occupy)++;
   

   return 0;
}

/** Fill in the matrix for blastn using the penaly and rewards
 * The query sequence alphabet is blastna, the subject sequence
 * is ncbi2na.  The alphabet blastna is defined in blast_stat.h
 * and the first four elements of blastna are identical to ncbi2na.
 * @param sbp the BlastScoreBlk on which reward, penalty, and matrix will be set [in|out]
 * @return zero on success.
*/

Int2 BlastScoreBlkNuclMatrixCreate(BlastScoreBlk* sbp)
{
    Int2  index1, index2, degen;
    Int2 degeneracy[BLASTNA_SIZE+1];
    Int4 reward; /* reward for match of bases. */
    Int4 penalty; /* cost for mismatch of bases. */
    Int4** matrix; /* matrix to be populated. */
    /* How many of the first bases are ambiguous (four, of course). */
    const int k_number_non_ambig_bp = 4; 

    ASSERT(sbp);
    ASSERT(sbp->alphabet_size == BLASTNA_SIZE);
    ASSERT(sbp->matrix);
    ASSERT(sbp->matrix->ncols == BLASTNA_SIZE);
    ASSERT(sbp->matrix->nrows == BLASTNA_SIZE);

    reward = sbp->reward;
    penalty = sbp->penalty;
    matrix = sbp->matrix->data;

    for (index1 = 0; index1<BLASTNA_SIZE; index1++)
        for (index2 = 0; index2<BLASTNA_SIZE; index2++)
            matrix[index1][index2] = 0;

    /* In blastna the 1st four bases are A, C, G, and T, exactly as it is 
       ncbi2na. */
    /* ncbi4na gives them the value 1, 2, 4, and 8.  */
    /* Set the first four bases to degen. one */
    for (index1=0; index1<k_number_non_ambig_bp; index1++)
        degeneracy[index1] = 1;

    for (index1=k_number_non_ambig_bp; index1<BLASTNA_SIZE; index1++) {
        degen=0;
        for (index2=0; index2<k_number_non_ambig_bp; index2++) /* ncbi2na */
        {
            if (BLASTNA_TO_NCBI4NA[index1] & BLASTNA_TO_NCBI4NA[index2])
                degen++;
        }
        degeneracy[index1] = degen;
    }


    for (index1=0; index1<BLASTNA_SIZE; index1++) {
        for (index2=index1; index2<BLASTNA_SIZE; index2++) {
            if (BLASTNA_TO_NCBI4NA[index1] & BLASTNA_TO_NCBI4NA[index2]) { 
                /* round up for positive scores, down for negatives. */
                matrix[index1][index2] = 
                    BLAST_Nint( (double) ((degeneracy[index2]-1)*penalty + 
                                          reward))/degeneracy[index2];
                if (index1 != index2)
                {
                      matrix[index2][index1] = matrix[index1][index2];
                }
            }
            else
            {
                matrix[index1][index2] = penalty;
                matrix[index2][index1] = penalty;
            }
        }
    }

    /* The value of 15 is a gap, which is a sentinel between strands in 
    the ungapped extension algorithm */
    for (index1=0; index1<BLASTNA_SIZE; index1++)
        matrix[BLASTNA_SIZE-1][index1] = INT4_MIN / 2;
    for (index1=0; index1<BLASTNA_SIZE; index1++)
        matrix[index1][BLASTNA_SIZE-1] = INT4_MIN / 2;

    return 0;
}

/** Read in the matrix from the FILE *fp.
 * This function ASSUMES that the matrices are in the ncbistdaa
 * @param sbp the BlastScoreBlk with the matrix to be populated [in|out]
 * @param fp the file pointer to read from [in]
 * @return zero on success
*/

static Int2
BlastScoreBlkProteinMatrixRead(BlastScoreBlk* sbp, FILE *fp)
{
    char buf[512+3];
    char temp[512];
    char*   cp,* lp;
    char    ch;
    Int4 ** matrix;
    Int4 *  m;
    Int4 score;
    Uint4   a1cnt = 0, a2cnt = 0;
    char    a1chars[BLASTAA_SIZE], a2chars[BLASTAA_SIZE];
    long lineno = 0;
    double  xscore;
    register int  index1, index2;
    int x_index, u_index;
    const char kCommentChar = '#';
    const char* kTokenStr = " \t\n\r";
    
    ASSERT(sbp->alphabet_size == BLASTAA_SIZE);
    ASSERT(sbp->matrix);
    ASSERT(sbp->matrix->ncols == BLASTAA_SIZE);
    ASSERT(sbp->matrix->nrows == BLASTAA_SIZE);

    matrix = sbp->matrix->data;  
    
    if (sbp->alphabet_code != BLASTNA_SEQ_CODE) {
        for (index1 = 0; index1 < sbp->alphabet_size; index1++)
            for (index2 = 0; index2 < sbp->alphabet_size; index2++)
                matrix[index1][index2] = BLAST_SCORE_MIN;
    }
    
    /* Read the residue names for the second alphabet */
    while (fgets(buf, sizeof(buf), fp) != NULL) {
        ++lineno;
        if (strchr(buf, '\n') == NULL) {
            return 2;
        }

        if (buf[0] == kCommentChar) {
            /* save the comment line in a linked list */
            *strchr(buf, '\n') = NULLB;
            ListNodeCopyStr(&sbp->comments, 0, buf+1);
            continue;
        }
        if ((cp = strchr(buf, kCommentChar)) != NULL)
            *cp = NULLB;
        lp = (char*)strtok(buf, kTokenStr);
        if (lp == NULL) /* skip blank lines */
            continue;
        while (lp != NULL) {
           if (sbp->alphabet_code == BLASTAA_SEQ_CODE)
              ch = AMINOACID_TO_NCBISTDAA[toupper((unsigned char)(*lp))];
           else if (sbp->alphabet_code == BLASTNA_SEQ_CODE) {
              ch = IUPACNA_TO_BLASTNA[toupper((unsigned char)(*lp))];
           } else {
              ch = *lp;
           }
            a2chars[a2cnt++] = ch;
            lp = (char*)strtok(NULL, kTokenStr);
        }
        
        break; /* Exit loop after reading one line. */
    }
    
    if (a2cnt <= 1) { 
        return 2;
    }

    while (fgets(buf, sizeof(buf), fp) != NULL)  {
        ++lineno;
        if ((cp = strchr(buf, '\n')) == NULL) {
            return 2;
        }
        if ((cp = strchr(buf, kCommentChar)) != NULL)
            *cp = NULLB;
        if ((lp = (char*)strtok(buf, kTokenStr)) == NULL)
            continue;
        ch = *lp;
        cp = (char*) lp;
        if ((cp = strtok(NULL, kTokenStr)) == NULL) {
            return 2;
        }
        if (a1cnt >= DIM(a1chars)) {
            return 2;
        }

        if (sbp->alphabet_code == BLASTAA_SEQ_CODE) {
           ch = AMINOACID_TO_NCBISTDAA[toupper((unsigned char) ch)];
        } else {
            if (sbp->alphabet_code == BLASTNA_SEQ_CODE) {
                ch = IUPACNA_TO_BLASTNA[toupper((unsigned char) ch)];
            }
        }
        a1chars[a1cnt++] = ch;
        m = &matrix[(int)ch][0];
        index2 = 0;
        while (cp != NULL) {
            if (index2 >= (int) a2cnt) {
                return 2;
            }
            strcpy(temp, cp);
            
            if (strcasecmp(temp, "na") == 0)  {
                score = BLAST_SCORE_MIN;
            } else  {
                if (sscanf(temp, "%lg", &xscore) != 1) {
                    return 2;
                }
            /*xscore = MAX(xscore, BLAST_SCORE_1MIN);*/
                if (xscore > BLAST_SCORE_MAX || xscore < BLAST_SCORE_MIN) {
                    return 2;
                }
                xscore += (xscore >= 0. ? 0.5 : -0.5);
                score = (Int4)xscore;
            }
            
            m[(int)a2chars[index2++]] = score;
            
            cp = strtok(NULL, kTokenStr);
        }
    }
    
    if (a1cnt <= 1) {
        return 2;
    }
    
    /* Use the X scores for selenocysteines; if this is not done
       then U will never align to a non-gap residue */
    x_index = AMINOACID_TO_NCBISTDAA['X'];
    u_index = AMINOACID_TO_NCBISTDAA['U'];
    for (index1 = 0; index1 < sbp->alphabet_size; index1++) {
        matrix[u_index][index1] = matrix[x_index][index1];
        matrix[index1][u_index] = matrix[index1][x_index];
    }

    return 0;
}

/** Sets maximum and minimum scores on the BlastScoreBlk for a 
 * given matrix
 * @param sbp the BlastScoreBlk on which loscore and hiscore 
 *   will be set [in|out]
 * @return zero on success
 */

static Int2
BlastScoreBlkMaxScoreSet(BlastScoreBlk* sbp)
{
    Int4 score;
    Int4 ** matrix; 
    Int2 index1, index2;

    sbp->loscore = BLAST_SCORE_MAX;
    sbp->hiscore = BLAST_SCORE_MIN;
    matrix = sbp->matrix->data;
    for (index1=0; index1<sbp->alphabet_size; index1++)
    {
      for (index2=0; index2<sbp->alphabet_size; index2++)
      {
         score = matrix[index1][index2];
         if (score <= BLAST_SCORE_MIN || score >= BLAST_SCORE_MAX)
            continue;
         if (sbp->loscore > score)
            sbp->loscore = score;
         if (sbp->hiscore < score)
            sbp->hiscore = score;
      }
    }
    /* If the lo/hi-scores are BLAST_SCORE_MIN/BLAST_SCORE_MAX, (i.e., for
    gaps), then use other scores. */

    if (sbp->loscore < BLAST_SCORE_MIN)
      sbp->loscore = BLAST_SCORE_MIN;
    if (sbp->hiscore > BLAST_SCORE_MAX)
      sbp->hiscore = BLAST_SCORE_MAX;

    return 0;
}

/** Sets sbp->matrix->data field using sbp->name field using
 * the matrices in the toolkit (util/tables/raw_scoremat.h).
 * @param sbp the object containing matrix and name [in|out]
 * @return 0 on success, 1 if matrix could not be loaded 
 */
static Int2
BlastScoreBlkProteinMatrixLoad(BlastScoreBlk* sbp)
{
    Int2 status = 0;
    SNCBIPackedScoreMatrix* psm;
    Int4** matrix = NULL;
    int i, j;   /* loop indices */
    int x_index, u_index;

    ASSERT(sbp);
    ASSERT(sbp->alphabet_size == BLASTAA_SIZE);
    ASSERT(sbp->matrix);
    ASSERT(sbp->matrix->ncols == BLASTAA_SIZE);
    ASSERT(sbp->matrix->nrows == BLASTAA_SIZE);

    matrix = sbp->matrix->data;

    if (strcasecmp(sbp->name, "BLOSUM62") == 0) {
        psm = (SNCBIPackedScoreMatrix*) &NCBISM_Blosum62;
    } else if (strcasecmp(sbp->name, "BLOSUM45") == 0) {
        psm = (SNCBIPackedScoreMatrix*) &NCBISM_Blosum45;
    } else if (strcasecmp(sbp->name, "BLOSUM80") == 0) {
        psm = (SNCBIPackedScoreMatrix*) &NCBISM_Blosum80;
    } else if (strcasecmp(sbp->name, "PAM30") == 0) {
        psm = (SNCBIPackedScoreMatrix*) &NCBISM_Pam30;
    } else if (strcasecmp(sbp->name, "PAM70") == 0) {
        psm = (SNCBIPackedScoreMatrix*) &NCBISM_Pam70;
    } else if (strcasecmp(sbp->name, "PAM250") == 0) {
        psm = (SNCBIPackedScoreMatrix*) &NCBISM_Pam250;
    } else {
        return 1;
    }

    /* Initialize with BLAST_SCORE_MIN */
    for (i = 0; i < sbp->alphabet_size; i++) {
        for (j = 0; j < sbp->alphabet_size; j++) {
            matrix[i][j] = BLAST_SCORE_MIN;
        }
    }

    for (i = 0; i < sbp->alphabet_size; i++) {
        for (j = 0; j < sbp->alphabet_size; j++) {
            /* skip selenocysteine and gap */
            if (i == AMINOACID_TO_NCBISTDAA['U'] || 
                i == AMINOACID_TO_NCBISTDAA['-'] ||
                j == AMINOACID_TO_NCBISTDAA['U'] || 
                j == AMINOACID_TO_NCBISTDAA['-']) {
                continue;
            }
            matrix[i][j] = NCBISM_GetScore((const SNCBIPackedScoreMatrix*) psm,
                                           i, j);
        }
    }

    /* Use the X scores for selenocysteines; if this is not done
       then U will never align to a non-gap residue */
    x_index = AMINOACID_TO_NCBISTDAA['X'];
    u_index = AMINOACID_TO_NCBISTDAA['U'];
    for (i = 0; i < sbp->alphabet_size; i++) {
        matrix[u_index][i] = matrix[x_index][i];
        matrix[i][u_index] = matrix[i][x_index];
    }

    return status;
}

Int2
Blast_ScoreBlkMatrixFill(BlastScoreBlk* sbp, char* matrix_path)
{
    Int2 status = 0;
    
    /* For nucleotide case we first create a default matrix, based on the 
       match and mismatch scores. */
    if (sbp->alphabet_code == BLASTNA_SEQ_CODE) {
       if ( (status=BlastScoreBlkNuclMatrixCreate(sbp)) != 0)
          return status;
    }

    if (sbp->read_in_matrix) {
        if (matrix_path && *matrix_path != NULLB) {

            FILE *fp = NULL;
            char* full_matrix_path = NULL;
            int path_len = strlen(matrix_path);
            int buflen = path_len + strlen(sbp->name);

            full_matrix_path = (char*) malloc((buflen + 1) * sizeof(char));
            if (!full_matrix_path) {
                return -1;
            }
            strncpy(full_matrix_path, matrix_path, buflen);
            strncat(full_matrix_path, sbp->name, buflen - path_len);

            if ( (fp=fopen(full_matrix_path, "r")) == NULL) {
               return -1;
            }
            sfree(full_matrix_path);

            if ( (status=BlastScoreBlkProteinMatrixRead(sbp, fp)) != 0) {
               fclose(fp);
               return status;
            }
            fclose(fp);

        } else {
            if ( (status = BlastScoreBlkProteinMatrixLoad(sbp)) !=0) {
                return status;
            }
        }
    }

    if ( (status=BlastScoreBlkMaxScoreSet(sbp)) != 0)
         return status;
    
    return status;
}

Blast_ResFreq*
Blast_ResFreqFree(Blast_ResFreq* rfp)
{
   if (rfp == NULL)
      return NULL;

   if (rfp->prob0 != NULL)
      sfree(rfp->prob0);

   sfree(rfp);

   return rfp;
}

/*
   Allocates the Blast_ResFreq* and then fills in the frequencies
   in the probabilities.
*/ 

Blast_ResFreq*
Blast_ResFreqNew(const BlastScoreBlk* sbp)
{
   Blast_ResFreq* rfp;

   if (sbp == NULL)
   {
      return NULL;
   }

   rfp = (Blast_ResFreq*) calloc(1, sizeof(Blast_ResFreq));
   if (rfp == NULL)
      return NULL;

   rfp->alphabet_code = sbp->alphabet_code;

   rfp->prob0 = (double*) calloc(sbp->alphabet_size, sizeof(double));
   if (rfp->prob0 == NULL) 
   {
      rfp = Blast_ResFreqFree(rfp);
      return rfp;
   }
   rfp->prob = rfp->prob0 - sbp->alphabet_start;

   return rfp;
}

/** Records probability of letter appearing in sequence.
*/
typedef struct BLAST_LetterProb {
      char  ch; /**< residue */
      double   p;  /**< probability of residue. */
   } BLAST_LetterProb;

#if 0

/* Unused for right now, but do not remove */

/*  M. O. Dayhoff amino acid background frequencies   */
static BLAST_LetterProb Dayhoff_prob[] = {
      { 'A', 87.13 },
      { 'C', 33.47 },
      { 'D', 46.87 },
      { 'E', 49.53 },
      { 'F', 39.77 },
      { 'G', 88.61 },
      { 'H', 33.62 },
      { 'I', 36.89 },
      { 'K', 80.48 },
      { 'L', 85.36 },
      { 'M', 14.75 },
      { 'N', 40.43 },
      { 'P', 50.68 },
      { 'Q', 38.26 },
      { 'R', 40.90 },
      { 'S', 69.58 },
      { 'T', 58.54 },
      { 'V', 64.72 },
      { 'W', 10.49 },
      { 'Y', 29.92 }
   };

/* Stephen Altschul amino acid background frequencies */
static BLAST_LetterProb Altschul_prob[] = {
      { 'A', 81.00 },
      { 'C', 15.00 },
      { 'D', 54.00 },
      { 'E', 61.00 },
      { 'F', 40.00 },
      { 'G', 68.00 },
      { 'H', 22.00 },
      { 'I', 57.00 },
      { 'K', 56.00 },
      { 'L', 93.00 },
      { 'M', 25.00 },
      { 'N', 45.00 },
      { 'P', 49.00 },
      { 'Q', 39.00 },
      { 'R', 57.00 },
      { 'S', 68.00 },
      { 'T', 58.00 },
      { 'V', 67.00 },
      { 'W', 13.00 },
      { 'Y', 32.00 }
   };
#endif

/* amino acid background frequencies from Robinson and Robinson */
static BLAST_LetterProb Robinson_prob[] = {
      { 'A', 78.05 },
      { 'C', 19.25 },
      { 'D', 53.64 },
      { 'E', 62.95 },
      { 'F', 38.56 },
      { 'G', 73.77 },
      { 'H', 21.99 },
      { 'I', 51.42 },
      { 'K', 57.44 },
      { 'L', 90.19 },
      { 'M', 22.43 },
      { 'N', 44.87 },
      { 'P', 52.03 },
      { 'Q', 42.64 },
      { 'R', 51.29 },
      { 'S', 71.20 },
      { 'T', 58.41 },
      { 'V', 64.41 },
      { 'W', 13.30 },
      { 'Y', 32.16 }
   }; /**< amino acid background frequencies from Robinson and Robinson */

#define STD_AMINO_ACID_FREQS Robinson_prob /**< points to the standard amino acid frequencies to use. */

static BLAST_LetterProb nt_prob[] = {
      { 'A', 25.00 },
      { 'C', 25.00 },
      { 'G', 25.00 },
      { 'T', 25.00 }
   }; /**< nucleotide probabilities (25% each letter) */

/** Normalizes all the residue frequencies and then normalizes them to "norm".
 * If "norm" is one, then they will all sum to one.
 * @param sbp needed for alphabet information [in]
 * @param rfp array of residue frequencies to be normalized [in|out]
 * @param norm value to normalize to [in]
 * @return zero on success, 1 otherwise
*/
static Int2
Blast_ResFreqNormalize(const BlastScoreBlk* sbp, Blast_ResFreq* rfp, double norm)
{
   Int2  alphabet_stop, index;
   double   sum = 0., p;

   if (norm == 0.)
      return 1;

   alphabet_stop = sbp->alphabet_start + sbp->alphabet_size;
   for (index=sbp->alphabet_start; index<alphabet_stop; index++)
   {
      p = rfp->prob[index];
      if (p < 0.)
         return 1;
      sum += p;
   }
   if (sum <= 0.)
      return 0;

   for (index=sbp->alphabet_start; index<alphabet_stop; index++)
   {
      rfp->prob[index] /= sum;
      rfp->prob[index] *= norm;
   }
   return 0;
}

Int2
Blast_GetStdAlphabet(Uint1 alphabet_code, Uint1* residues, Uint4 residues_size)
{
   Int2 index;

   if (residues_size < DIM(STD_AMINO_ACID_FREQS))
      return -2;

   for (index=0; index<(int)DIM(STD_AMINO_ACID_FREQS); index++) 
   {
      if (alphabet_code == BLASTAA_SEQ_CODE)
      {
         residues[index] = 
            AMINOACID_TO_NCBISTDAA[toupper((unsigned char) STD_AMINO_ACID_FREQS[index].ch)];
      }
      else
      {
         residues[index] = STD_AMINO_ACID_FREQS[index].ch;
      }
   }

   return index;
}

Int2
Blast_ResFreqStdComp(const BlastScoreBlk* sbp, Blast_ResFreq* rfp)
{
        Uint4 index;

   if (sbp->protein_alphabet == TRUE)
   {
           Int2 retval;
           Uint1* residues = (Uint1*) calloc(DIM(STD_AMINO_ACID_FREQS), sizeof(Uint1));
      retval = Blast_GetStdAlphabet(sbp->alphabet_code, residues, DIM(STD_AMINO_ACID_FREQS));
      if (retval < 1)
         return retval;

      for (index=0; index<DIM(STD_AMINO_ACID_FREQS); index++) 
      {
         rfp->prob[residues[index]] = STD_AMINO_ACID_FREQS[index].p;
      }
      sfree(residues);
   }
   else
   {  /* beginning of blastna and ncbi2na are the same. */
      /* Only blastna used  for nucleotides. */
      for (index=0; index<DIM(nt_prob); index++) 
      {
         rfp->prob[index] = nt_prob[index].p;
      }
   }

   Blast_ResFreqNormalize(sbp, rfp, 1.0);

   return 0;
}

/** 
Intermediate structure to store the composition of a sequence
*/
typedef struct Blast_ResComp {
    Uint1   alphabet_code; /**< indicates alphabet. */
    Int4*   comp;    /**< store composition of a string. */
    Int4*   comp0;   /**< Same array as above, starts at zero. */
} Blast_ResComp;

/** Deallocates Blast_ResComp structure and 
 * associated arrays.
 * @param rcp the object to be freed [in|out]
 * @return NULL
 */
static Blast_ResComp*
BlastResCompDestruct(Blast_ResComp* rcp)
{
   if (rcp == NULL)
      return NULL;

   if (rcp->comp0 != NULL)
      sfree(rcp->comp0);

   sfree(rcp);
   return NULL;
}

/** Allocated the Blast_ResComp* for a given alphabet.  Only the
 *  alphabets ncbistdaa and ncbi4na should be used by BLAST.
 * @param sbp contains alphabet code and size.
 * @return pointer to Blast_ResComp, corectly initialized.
*/
static Blast_ResComp*
BlastResCompNew(const BlastScoreBlk* sbp)
{
   Blast_ResComp* rcp;

   rcp = (Blast_ResComp*) calloc(1, sizeof(Blast_ResComp));
   if (rcp == NULL)
      return NULL;

   rcp->alphabet_code = sbp->alphabet_code;

/* comp0 has zero offset, comp starts at 0, only one 
array is allocated.  */
   rcp->comp0 = (Int4*) calloc(sbp->alphabet_size, sizeof(Int4));
   if (rcp->comp0 == NULL) 
   {
      rcp = BlastResCompDestruct(rcp);
      return rcp;
   }

   rcp->comp = rcp->comp0 - sbp->alphabet_start;

   return rcp;
}

/** Store the composition of a (query) string.  
 * @param sbp needed for alphabet information [in]
 * @param rcp object to be filled in [in|out]
 * @param str sequence to have composition calculated for [in]
 * @param length length of sequence [in]
 * @return zero on success, 1 otherwise.
*/
static Int2
BlastResCompStr(const BlastScoreBlk* sbp, Blast_ResComp* rcp, char* str, Int4 length)
{
   char* lp,* lpmax;
   Int2 index;
        Uint1 mask;

   if (sbp == NULL || rcp == NULL || str == NULL)
      return 1;

   if (rcp->alphabet_code != sbp->alphabet_code)
      return 1;

        /* For megablast, check only the first 4 bits of the sequence values */
        mask = (sbp->protein_alphabet ? 0xff : 0x0f);

/* comp0 starts at zero and extends for "num", comp is the same array, but 
"start_at" offset. */
   for (index=0; index<(sbp->alphabet_size); index++)
      rcp->comp0[index] = 0;

   for (lp = str, lpmax = lp+length; lp < lpmax; lp++)
   {
      ++rcp->comp[(int)(*lp & mask)];
   }

   /* Don't count ambig. residues. */
   for (index=0; index<sbp->ambig_occupy; index++)
   {
      rcp->comp[sbp->ambiguous_res[index]] = 0;
   }

   return 0;
}

/** Sets prob elements of Blast_ResFreq to zero
 * @param sbp needed for alphabet information [in]
 * @param rfp contains elements to be zeroed [in|out]
 * @return zero on success.
 */
static Int2
Blast_ResFreqClr(const BlastScoreBlk* sbp, Blast_ResFreq* rfp)
{
   Int2  alphabet_max, index;
 
   alphabet_max = sbp->alphabet_start + sbp->alphabet_size;
   for (index=sbp->alphabet_start; index<alphabet_max; index++)
                rfp->prob[index] = 0.0;
 
        return 0;
}

/** Calculate the residue frequencies associated with the provided ResComp
 *  This function takes into account the composition of a given sequence
 *  (expressed through rcp) rather than just doing it for a standard distribution.
 * @param sbp contains alphabet information [in]
 * @param rfp object to be filled in [in|out]
 * @param rcp object with composition information [in]
 * @return zero on success, 1 on failure
*/
static Int2
Blast_ResFreqResComp(const BlastScoreBlk* sbp, Blast_ResFreq* rfp, 
                     const Blast_ResComp* rcp)
{
   Int2  alphabet_max, index;
   double   sum = 0.;

   if (rfp == NULL || rcp == NULL)
      return 1;

   if (rfp->alphabet_code != rcp->alphabet_code)
      return 1;

   alphabet_max = sbp->alphabet_start + sbp->alphabet_size;
   for (index=sbp->alphabet_start; index<alphabet_max; index++)
      sum += rcp->comp[index];

   if (sum == 0.) {
      Blast_ResFreqClr(sbp, rfp);
      return 0;
   }

   for (index=sbp->alphabet_start; index<alphabet_max; index++)
      rfp->prob[index] = rcp->comp[index] / sum;

   return 0;
}

/** Fills in residue frequences for a given sequence.
 * @param sbp needed for alphabet information [in]
 * @param rfp object to be populated [in|out]
 * @param string sequence for calculation [in]
 * @param length length of above sequence [in]
 */
static Int2
Blast_ResFreqString(const BlastScoreBlk* sbp, Blast_ResFreq* rfp, char* string, Int4 length)
{
   Blast_ResComp* rcp;
   
   rcp = BlastResCompNew(sbp);

   BlastResCompStr(sbp, rcp, string, length);

   Blast_ResFreqResComp(sbp, rfp, rcp);

   rcp = BlastResCompDestruct(rcp);

   return 0;
}

/** Check that the lo and hi score are within the allowed ranges
 * @param lo the lowest permitted value [in]
 * @param hi the highest permitted value [in]
 * @return zero on success, 1 otherwise
 */

static Int2
BlastScoreChk(Int4 lo, Int4 hi)
{
   if (lo >= 0 || hi <= 0 ||
         lo < BLAST_SCORE_MIN || hi > BLAST_SCORE_MAX)
      return 1;

   if (hi - lo > BLAST_SCORE_RANGE_MAX)
      return 1;

   return 0;
}

Blast_ScoreFreq*
Blast_ScoreFreqNew(Int4 score_min, Int4 score_max)
{
   Blast_ScoreFreq*  sfp;
   Int4  range;

   if (BlastScoreChk(score_min, score_max) != 0)
      return NULL;

   sfp = (Blast_ScoreFreq*) calloc(1, sizeof(Blast_ScoreFreq));
   if (sfp == NULL)
      return NULL;

   range = score_max - score_min + 1;
   sfp->sprob = (double*) calloc(range, sizeof(double));
   if (sfp->sprob == NULL) 
   {
      Blast_ScoreFreqFree(sfp);
      return NULL;
   }

   sfp->sprob0 = sfp->sprob;
   sfp->sprob -= score_min;        /* center around 0 */
   sfp->score_min = score_min;
   sfp->score_max = score_max;
   sfp->obs_min = sfp->obs_max = 0;
   sfp->score_avg = 0.0;
   return sfp;
}

/** Calculates the score frequencies.
 *
 * @param sbp object with scoring information [in]
 * @param sfp object to hold frequency information [in|out]
 * @param rfp1 letter frequencies for first sequence (query) [in]
 * @param rfp2 letter frequencies for second sequence (database) [in]
 * @return zero on success
 */
static Int2
BlastScoreFreqCalc(const BlastScoreBlk* sbp, Blast_ScoreFreq* sfp, Blast_ResFreq* rfp1, Blast_ResFreq* rfp2)
{
   Int4 **  matrix;
   Int4  score, obs_min, obs_max;
   double      score_sum, score_avg;
   Int2     alphabet_start, alphabet_end, index1, index2;

   if (sbp == NULL || sfp == NULL)
      return 1;

   if (sbp->loscore < sfp->score_min || sbp->hiscore > sfp->score_max)
      return 1;

   for (score = sfp->score_min; score <= sfp->score_max; score++)
      sfp->sprob[score] = 0.0;

   matrix = sbp->matrix->data;

   alphabet_start = sbp->alphabet_start;
   alphabet_end = alphabet_start + sbp->alphabet_size;
   for (index1=alphabet_start; index1<alphabet_end; index1++)
   {
      for (index2=alphabet_start; index2<alphabet_end; index2++)
      {
         score = matrix[index1][index2];
         if (score >= sbp->loscore) 
         {
            sfp->sprob[score] += rfp1->prob[index1] * rfp2->prob[index2];
         }
      }
   }

   score_sum = 0.;
   obs_min = obs_max = BLAST_SCORE_MIN;
   for (score = sfp->score_min; score <= sfp->score_max; score++)
   {
      if (sfp->sprob[score] > 0.) 
      {
         score_sum += sfp->sprob[score];
         obs_max = score;
         if (obs_min == BLAST_SCORE_MIN)
            obs_min = score;
      }
   }
   sfp->obs_min = obs_min;
   sfp->obs_max = obs_max;

   score_avg = 0.0;
   if (score_sum > 0.0001 || score_sum < -0.0001)
   {
      for (score = obs_min; score <= obs_max; score++) 
      {
         sfp->sprob[score] /= score_sum;
         score_avg += score * sfp->sprob[score];
      }
   }
   sfp->score_avg = score_avg;

   return 0;
}



/** The following procedure computes K. The input includes Lambda, H,
 *  and an array of probabilities for each score.
 *  There are distinct closed form for three cases:
 *  1. high score is 1 low score is -1
 *  2. high score is 1 low score is not -1
 *  3. low score is -1, high score is not 1
 *
 * Otherwise, in most cases the value is computed as:
 * -exp(-2.0*outerSum) / ((H/lambda)*(exp(-lambda) - 1)
 * The last term (exp(-lambda) - 1) can be computed in two different
 * ways depending on whether lambda is small or not.
 * outerSum is a sum of the terms
 * innerSum/j, where j is denoted by iterCounter in the code.
 * The sum is truncated when the new term innersum/j i sufficiently small.
 * innerSum is a weighted sum of the probabilities of
 * of achieving a total score i in a gapless alignment,
 * which we denote by P(i,j).
 * of exactly j characters. innerSum(j) has two parts
 * Sum over i < 0  P(i,j)exp(-i * lambda) +
 * Sum over i >=0  P(i,j)
 * The terms P(i,j) are computed by dynamic programming.
 * An earlier version was flawed in that ignored the special case 1
 * and tried to replace the tail of the computation of outerSum
 * by a geometric series, but the base of the geometric series
 * was not accurately estimated in some cases.
 *
 * @param sfp object holding scoring frequency information [in]
 * @param lambda a Karlin-Altschul parameter [in]
 * @param H a Karlin-Altschul parameter [in]
 * @return K, another Karlin-Altschul parameter
 */

static double
BlastKarlinLHtoK(Blast_ScoreFreq* sfp, double lambda, double H)
{
    /*The next array stores the probabilities of getting each possible
      score in an alignment of fixed length; the array is shifted
      during part of the computation, so that
      entry 0 is for score 0.  */
    double         *alignmentScoreProbabilities = NULL;
    Int4            low;    /* Lowest score (must be negative) */
    Int4            high;   /* Highest score (must be positive) */
    Int4            range;  /* range of scores, computed as high - low*/
    double          K;      /* local copy of K  to return*/
    int             i;   /*loop index*/
    int             iterCounter; /*counter on iterations*/
    Int4            divisor; /*candidate divisor of all scores with
                               non-zero probabilities*/
    /*highest and lowest possible alignment scores for current length*/
    Int4            lowAlignmentScore, highAlignmentScore;
    Int4            first, last; /*loop indices for dynamic program*/
    register double innerSum;
    double          oldsum, oldsum2;  /* values of innerSum on previous
                                         iterations*/
    double          outerSum;        /* holds sum over j of (innerSum
                                        for iteration j/j)*/

    double          score_avg; /*average score*/
    /*first term to use in the closed form for the case where
      high == 1 or low == -1, but not both*/
    double          firstTermClosedForm;  /*usually store H/lambda*/
    int             iterlimit; /*upper limit on iterations*/
    double          sumlimit; /*lower limit on contributions
                                to sum over scores*/

    /*array of score probabilities reindexed so that low is at index 0*/
    double         *probArrayStartLow;

    /*pointers used in dynamic program*/
    double         *ptrP, *ptr1, *ptr2, *ptr1e;
    double          expMinusLambda; /*e^^(-Lambda) */

    if (lambda <= 0. || H <= 0.) {
        /* Theory dictates that H and lambda must be positive, so
         * return -1 to indicate an error */
        return -1.;
    }

    /*Karlin-Altschul theory works only if the expected score
      is negative*/
    if (sfp->score_avg >= 0.0) {
        return -1.;
    }

    low   = sfp->obs_min;
    high  = sfp->obs_max;
    range = high - low;

    probArrayStartLow = &sfp->sprob[low];
    /* Look for the greatest common divisor ("delta" in Appendix of PNAS 87 of
       Karlin&Altschul (1990) */
    for (i = 1, divisor = -low; i <= range && divisor > 1; ++i) {
        if (probArrayStartLow[i] != 0.0)
            divisor = BLAST_Gcd(divisor, i);
    }

    high   /= divisor;
    low    /= divisor;
    lambda *= divisor;

    range = high - low;

    firstTermClosedForm = H/lambda;
    expMinusLambda      = exp((double) -lambda);

    if (low == -1 && high == 1) {
        K = (sfp->sprob[low*divisor] - sfp->sprob[high*divisor]) *
            (sfp->sprob[low*divisor] - sfp->sprob[high*divisor]) / sfp->sprob[low*divisor];
        return(K);
    }

    if (low == -1 || high == 1) {
        if (high != 1) {
            score_avg = sfp->score_avg / divisor;
            firstTermClosedForm
                = (score_avg * score_avg) / firstTermClosedForm;
        }
        return firstTermClosedForm * (1.0 - expMinusLambda);
    }

    sumlimit  = BLAST_KARLIN_K_SUMLIMIT_DEFAULT;
    iterlimit = BLAST_KARLIN_K_ITER_MAX;

    alignmentScoreProbabilities =
        (double *)calloc((iterlimit*range + 1), sizeof(*alignmentScoreProbabilities));
    if (alignmentScoreProbabilities == NULL)
        return -1.;

    outerSum = 0.;
    lowAlignmentScore = highAlignmentScore = 0;
    alignmentScoreProbabilities[0] = innerSum = oldsum = oldsum2 = 1.;

    for (iterCounter = 0;
         ((iterCounter < iterlimit) && (innerSum > sumlimit));
         outerSum += innerSum /= ++iterCounter) {
        first = last = range;
        lowAlignmentScore  += low;
        highAlignmentScore += high;
        /*dynamic program to compute P(i,j)*/
        for (ptrP = alignmentScoreProbabilities +
                 (highAlignmentScore-lowAlignmentScore);
             ptrP >= alignmentScoreProbabilities;
             *ptrP-- =innerSum) {
            ptr1  = ptrP - first;
            ptr1e = ptrP - last;
            ptr2  = probArrayStartLow + first;
            for (innerSum = 0.; ptr1 >= ptr1e; )
                innerSum += *ptr1--  *  *ptr2++;
            if (first)
                --first;
            if (ptrP - alignmentScoreProbabilities <= range)
                --last;
        }
        /* Horner's rule */
        innerSum = *++ptrP;
        for( i = lowAlignmentScore + 1; i < 0; i++ ) {
            innerSum = *++ptrP + innerSum * expMinusLambda;
        }
        innerSum *= expMinusLambda;

        for (; i <= highAlignmentScore; ++i)
            innerSum += *++ptrP;
        oldsum2 = oldsum;
        oldsum  = innerSum;
    }

#ifdef ADD_GEOMETRIC_TERMS_TO_K
    /*old code assumed that the later terms in sum were
      asymptotically comparable to those of a geometric
      progression, and tried to speed up convergence by
      guessing the estimated ratio between sucessive terms
      and using the explicit terms of a geometric progression
      to speed up convergence. However, the assumption does not
      always hold, and convergenece of the above code is fast
      enough in practice*/
    /* Terms of geometric progression added for correction */
    {
        double     ratio;  /* fraction used to generate the
                                   geometric progression */

        ratio = oldsum / oldsum2;
        if (ratio >= (1.0 - sumlimit*0.001)) {
            K = -1.;
            if (alignmentScoreProbabilities != NULL)
                sfree(alignmentScoreProbabilities);
            return K;
        }
        sumlimit *= 0.01;
        while (innerSum > sumlimit) {
            oldsum   *= ratio;
            outerSum += innerSum = oldsum / ++iterCounter;
        }
    }
#endif

    K = -exp((double)-2.0*outerSum) /
             (firstTermClosedForm*BLAST_Expm1(-(double)lambda));

    if (alignmentScoreProbabilities != NULL)
        sfree(alignmentScoreProbabilities);

    return K;
}


/**
 * Find positive solution to sum_{i=low}^{high} exp(i lambda) = 1.
 * 
 * @param probs probabilities of a score occurring 
 * @param d the gcd of the possible scores. This equals 1 if the scores
 * are not a lattice
 * @param low the lowest possible score
 * @param high the highest possible score
 * @param lambda0 an initial value for lambda
 * @param tolx the tolerance to which lambda must be computed
 * @param itmax the maximum number of times the function may be
 * evaluated
 * @param maxNewton the maximum permissible number of Newton
 * iteration. After that the computation will proceed by bisection.
 * @param itn a pointer to an integer that will receive the actually
 * number of iterations performed.
 *
 * Let phi(lambda) =  sum_{i=low}^{high} exp(i lambda) - 1. Then phi(lambda)
 * may be written
 *
 *     phi(lamdba) = exp(u lambda) p( exp(-lambda) )
 *
 * where p(x) is a polynomial that has exactly two zeros, one at x = 1
 * and one at y = exp(-lamdba). It is simpler, more numerically
 * efficient and stable to apply Newton's method to p(x) than to
 * phi(lambda).
 *
 * We define a safeguarded Newton iteration as follows. Let the
 * initial interval of uncertainty be [0,1]. If p'(x) >= 0, we bisect
 * the interval. Otherwise we try a Newton step. If the Newton iterate
 * lies in the current interval of uncertainty and it reduces the
 * value of | p(x) | by at least 10%, we accept the new
 * point. Otherwise, we bisect the current interval of uncertainty.
 * It is clear that this method converges to a zero of p(x).  Since
 * p'(x) > 0 in an interval containing x = 1, the method cannot
 * converge to x = 1 and therefore converges to the only other zero,
 * y.
 */

static double 
NlmKarlinLambdaNR( double* probs, Int4 d, Int4 low, Int4 high, double lambda0, double tolx,
                            Int4 itmax, Int4 maxNewton, Int4 * itn ) 
{
  Int4 k;
  double x0, x, a = 0, b = 1;
  double f = 4;  /* Larger than any possible value of the poly in [0,1] */
  Int4 isNewton = 0; /* we haven't yet taken a Newton step. */

  assert( d > 0 );

   x0 = exp( -lambda0 );
  x = ( 0 < x0 && x0 < 1 ) ? x0 : .5;
  
  for( k = 0; k < itmax; k++ ) { /* all iteration indices k */
    Int4 i;
    double g, fold = f;
    Int4 wasNewton = isNewton; /* If true, then the previous step was a */
                              /* Newton step */
    isNewton  = 0;            /* Assume that this step is not */
    
    /* Horner's rule for evaluating a polynomial and its derivative */
    g = 0;
    f = probs[low];
    for( i = low + d; i < 0; i += d ) {
      g = x * g + f;
      f = f * x + probs[i];
    }
    g = x * g + f;
    f = f * x + probs[0] - 1;
    for( i = d; i <= high; i += d ) {
      g = x * g + f;
      f = f * x + probs[i];
    }
    /* End Horner's rule */

    if( f > 0 ) {
      a = x; /* move the left endpoint */
    } else if( f < 0 ) { 
      b = x; /* move the right endpoint */
    } else { /* f == 0 */
      break; /* x is an exact solution */
    }
    if( b - a < 2 * a * ( 1 - b ) * tolx ) {
      /* The midpoint of the interval converged */
      x = (a + b) / 2; break;
    }

    if( k >= maxNewton ||
        /* If convergence of Newton's method appears to be failing; or */
            ( wasNewton && fabs( f ) > .9 * fabs(fold) ) ||  
        /* if the previous iteration was a Newton step but didn't decrease 
         * f sufficiently; or */
        g >= 0 
        /* if a Newton step will move us away from the desired solution */
        ) { /* then */
      /* bisect */
      x = (a + b)/2;
    } else {
      /* try a Newton step */
      double p = - f/g;
      double y = x + p;
      if( y <= a || y >= b ) { /* The proposed iterate is not in (a,b) */
        x = (a + b)/2;
      } else { /* The proposed iterate is in (a,b). Accept it. */
        isNewton = 1;
        x = y;
        if( fabs( p ) < tolx * x * (1-x) ) break; /* Converged */
      } /* else the proposed iterate is in (a,b) */
    } /* else try a Newton step. */ 
  } /* end for all iteration indices k */
   *itn = k; 
  return -log(x)/d;
}


double
Blast_KarlinLambdaNR(Blast_ScoreFreq* sfp, double initialLambdaGuess)
{
   Int4  low;        /* Lowest score (must be negative)  */
   Int4  high;       /* Highest score (must be positive) */
   Int4     itn;
   Int4  i, d;
   double*  sprob;
   double   returnValue;

   low = sfp->obs_min;
   high = sfp->obs_max;
   if (sfp->score_avg >= 0.) {   /* Expected score must be negative */
      return -1.0;
   }
   if (BlastScoreChk(low, high) != 0) return -1.;
   
   sprob = sfp->sprob;
   /* Find greatest common divisor of all scores */
   for (i = 1, d = -low; i <= high-low && d > 1; ++i) {
      if (sprob[i+low] != 0.0) {
         d = BLAST_Gcd(d, i);
      }
   }
   returnValue =
      NlmKarlinLambdaNR( sprob, d, low, high,
                           initialLambdaGuess,
                           BLAST_KARLIN_LAMBDA_ACCURACY_DEFAULT,
                     20, 20 + BLAST_KARLIN_LAMBDA_ITER_DEFAULT, &itn );


   return returnValue;
}

/** Calculate H, the relative entropy of the p's and q's
 *
 * @param sfp object containing scoring frequency information [in]
 * @param lambda a Karlin-Altschul parameter [in]
 * @return H, a Karlin-Altschul parameter
 */
static double 
BlastKarlinLtoH(Blast_ScoreFreq* sfp, double lambda)
{
   Int4  score;
   double   H, etonlam, sum, scale;

   double *probs = sfp->sprob;
   Int4 low   = sfp->obs_min,  high  = sfp->obs_max;

   if (lambda < 0.) {
      return -1.;
   }
   if (BlastScoreChk(low, high) != 0) return -1.;

   etonlam = exp( - lambda );
  sum = low * probs[low];
  for( score = low + 1; score <= high; score++ ) {
    sum = score * probs[score] + etonlam * sum;
  }

  scale = BLAST_Powi( etonlam, high );
  if( scale > 0.0 ) {
    H = lambda * sum/scale;
  } else { /* Underflow of exp( -lambda * high ) */
    H = lambda * exp( lambda * high + log(sum) );
  }
   return H;
}

/**************** Statistical Significance Parameter Subroutine ****************

    Version 1.0     February 2, 1990
    Version 1.2     July 6,     1990

    Program by:     Stephen Altschul

    Address:        National Center for Biotechnology Information
                    National Library of Medicine
                    National Institutes of Health
                    Bethesda, MD  20894

    Internet:       altschul@ncbi.nlm.nih.gov

See:  Karlin, S. & Altschul, S.F. "Methods for Assessing the Statistical
    Significance of Molecular Sequence Features by Using General Scoring
    Schemes,"  Proc. Natl. Acad. Sci. USA 87 (1990), 2264-2268.

    Computes the parameters lambda and K for use in calculating the
    statistical significance of high-scoring segments or subalignments.

    The scoring scheme must be integer valued.  A positive score must be
    possible, but the expected (mean) score must be negative.

    A program that calls this routine must provide the value of the lowest
    possible score, the value of the greatest possible score, and a pointer
    to an array of probabilities for the occurrence of all scores between
    these two extreme scores.  For example, if score -2 occurs with
    probability 0.7, score 0 occurs with probability 0.1, and score 3
    occurs with probability 0.2, then the subroutine must be called with
    low = -2, high = 3, and pr pointing to the array of values
    { 0.7, 0.0, 0.1, 0.0, 0.0, 0.2 }.  The calling program must also provide
    pointers to lambda and K; the subroutine will then calculate the values
    of these two parameters.  In this example, lambda=0.330 and K=0.154.

    The parameters lambda and K can be used as follows.  Suppose we are
    given a length N random sequence of independent letters.  Associated
    with each letter is a score, and the probabilities of the letters
    determine the probability for each score.  Let S be the aggregate score
    of the highest scoring contiguous segment of this sequence.  Then if N
    is sufficiently large (greater than 100), the following bound on the
    probability that S is greater than or equal to x applies:

            P( S >= x )   <=   1 - exp [ - KN exp ( - lambda * x ) ].

    In other words, the p-value for this segment can be written as
    1-exp[-KN*exp(-lambda*S)].

    This formula can be applied to pairwise sequence comparison by assigning
    scores to pairs of letters (e.g. amino acids), and by replacing N in the
    formula with N*M, where N and M are the lengths of the two sequences
    being compared.

    In addition, letting y = KN*exp(-lambda*S), the p-value for finding m
    distinct segments all with score >= S is given by:

                           2             m-1           -y
            1 - [ 1 + y + y /2! + ... + y   /(m-1)! ] e

    Notice that for m=1 this formula reduces to 1-exp(-y), which is the same
    as the previous formula.

*******************************************************************************/
Int2
Blast_KarlinBlkUngappedCalc(Blast_KarlinBlk* kbp, Blast_ScoreFreq* sfp)
{
   

   if (kbp == NULL || sfp == NULL)
      return 1;

   /* Calculate the parameter Lambda */

   kbp->Lambda = Blast_KarlinLambdaNR(sfp, BLAST_KARLIN_LAMBDA0_DEFAULT);
   if (kbp->Lambda < 0.)
      goto ErrExit;


   /* Calculate H */

   kbp->H = BlastKarlinLtoH(sfp, kbp->Lambda);
   if (kbp->H < 0.)
      goto ErrExit;


   /* Calculate K and log(K) */

   kbp->K = BlastKarlinLHtoK(sfp, kbp->Lambda, kbp->H);
   if (kbp->K < 0.)
      goto ErrExit;
   kbp->logK = log(kbp->K);

   /* Normal return */
   return 0;

ErrExit:
   kbp->Lambda = kbp->H = kbp->K = -1.;
   kbp->logK = HUGE_VAL;
   return 1;
}

Int2
Blast_ScoreBlkKbpUngappedCalc(EBlastProgramType program, 
                              BlastScoreBlk* sbp, Uint1* query, 
                              const BlastQueryInfo* query_info)
{
   Int4 context; /* loop variable. */
   Boolean query_valid = FALSE;
   Blast_ResFreq* rfp,* stdrfp;
   Int2 status = 0;
   Boolean check_ideal = 
      (program == eBlastTypeBlastx || program == eBlastTypeTblastx ||
       program == eBlastTypeRpsTblastn);

   /* Ideal Karlin block is filled unconditionally. */
   status = Blast_ScoreBlkKbpIdealCalc(sbp);
   if (status)
      return status;

   stdrfp = Blast_ResFreqNew(sbp);
   Blast_ResFreqStdComp(sbp, stdrfp);
   rfp = Blast_ResFreqNew(sbp);

   for (context = query_info->first_context;
        context <= query_info->last_context; ++context) {
      
      Int4 context_offset;
      Int4 query_length;
      Uint1 *buffer;              /* holds sequence */
      Blast_KarlinBlk* kbp;
      
      /* For each query, check if forward strand is present */
      if ((query_length = query_info->contexts[context].query_length) < 0)
         continue;
      
      context_offset = query_info->contexts[context].query_offset;
      buffer = &query[context_offset];
      
      Blast_ResFreqString(sbp, rfp, (char*)buffer, query_length);
      sbp->sfp[context] = Blast_ScoreFreqNew(sbp->loscore, sbp->hiscore);
      BlastScoreFreqCalc(sbp, sbp->sfp[context], rfp, stdrfp);
      sbp->kbp_std[context] = kbp = Blast_KarlinBlkNew();
      status = Blast_KarlinBlkUngappedCalc(kbp, sbp->sfp[context]);
      if (status) {
         continue;
      }
      /* For searches with translated queries, check whether ideal values
         should be substituted instead of calculated values, so a more 
         conservative (smaller) Lambda is used. */
      if (check_ideal && kbp->Lambda >= sbp->kbp_ideal->Lambda)
         Blast_KarlinBlkCopy(kbp, sbp->kbp_ideal);

      sbp->kbp_psi[context] = Blast_KarlinBlkNew();
      if ((status = Blast_KarlinBlkUngappedCalc(sbp->kbp_psi[context], 
                                                sbp->sfp[context])) == 0)
         query_valid = TRUE;
   }

   rfp = Blast_ResFreqFree(rfp);
   stdrfp = Blast_ResFreqFree(stdrfp);

   if (query_valid)
      status = 0;
   else 
      return status;

   /* Set ungapped Blast_KarlinBlk* alias */
   sbp->kbp = (program == eBlastTypePsiBlast) ? sbp->kbp_psi : sbp->kbp_std;

   return status;
}

Int2
Blast_ScoreBlkKbpIdealCalc(BlastScoreBlk* sbp)

{
   Blast_ResFreq* stdrfp = NULL;
   Blast_ScoreFreq* sfp = NULL;
    Int2 status = 0;

    if ( !sbp ) {
        return (status = 1);
    }

   stdrfp = Blast_ResFreqNew(sbp);
   Blast_ResFreqStdComp(sbp, stdrfp);
   sfp = Blast_ScoreFreqNew(sbp->loscore, sbp->hiscore);
   BlastScoreFreqCalc(sbp, sfp, stdrfp, stdrfp);
   sbp->kbp_ideal = Blast_KarlinBlkNew();
   Blast_KarlinBlkUngappedCalc(sbp->kbp_ideal, sfp);

   stdrfp = Blast_ResFreqFree(stdrfp);
   sfp = Blast_ScoreFreqFree(sfp);

    return status;
}

/*
   Creates the Karlin Block.
*/

Blast_KarlinBlk*
Blast_KarlinBlkNew(void)

{
   Blast_KarlinBlk* kbp;

   kbp = (Blast_KarlinBlk*) calloc(1, sizeof(Blast_KarlinBlk));

   return kbp;
}

Int2 Blast_KarlinBlkCopy(Blast_KarlinBlk* kbp_to, Blast_KarlinBlk* kbp_from)
{
   if (!kbp_to || !kbp_from)
      return -1;

   kbp_to->Lambda = kbp_from->Lambda;
   kbp_to->K = kbp_from->K;
   kbp_to->logK = kbp_from->logK;
   kbp_to->H = kbp_from->H;
   kbp_to->paramC = kbp_from->paramC;
   return 0;
}

/** Deallocates MatrixInfo as well as name string.
 * @param matrix_info the object to be deallocated [in]
 * @return NULL pointer
 */

static MatrixInfo*
MatrixInfoDestruct(MatrixInfo* matrix_info)

{
   if (matrix_info == NULL)
      return NULL;

   sfree(matrix_info->name);
   sfree(matrix_info);
   return NULL;
}

/** Allocates New MatrixInfo*
 * @param name name of matrix [in]
 * @param values array contains information about a matrix [in]
 * @param prefs contains information on a which values are preferred [in]
 * @param max_number size of those arrays [in]
 * @return pointer to the allocated MatrixInfo
 */

static MatrixInfo*
MatrixInfoNew(const char* name, array_of_8 *values, Int4* prefs, Int4 max_number)

{
   MatrixInfo* matrix_info;

   matrix_info = (MatrixInfo*) calloc(1, sizeof(MatrixInfo));
   matrix_info->name = strdup(name);
   matrix_info->values = values;
   matrix_info->prefs = prefs;
   matrix_info->max_number_values = max_number;

   return matrix_info;
}

/** Free linked list of MatrixValues and all associated data
 * @param vnp linked list of MatrixValues [in]
 * @return NULL pointer
 */
static ListNode*
BlastMatrixValuesDestruct(ListNode* vnp)

{
   ListNode* head;

   head = vnp;
   while (vnp)
   {
      MatrixInfoDestruct((MatrixInfo*) vnp->ptr);
      vnp = vnp->next;
   }

   head = ListNodeFree(head);

   return head;
}

/** Loads all the matrix values, returns a ListNode* chain that contains
 *  MatrixInfo*'s.
 * @return list of MatrixInfos.
 *
*/
static ListNode* 
BlastLoadMatrixValues (void)

{
   MatrixInfo* matrix_info;
   ListNode* retval=NULL;

   matrix_info = MatrixInfoNew("BLOSUM80", blosum80_values, blosum80_prefs, BLOSUM80_VALUES_MAX);
   ListNodeAddPointer(&retval, 0, matrix_info);

   matrix_info = MatrixInfoNew("BLOSUM62", blosum62_values, blosum62_prefs, BLOSUM62_VALUES_MAX);
   ListNodeAddPointer(&retval, 0, matrix_info);

   matrix_info = MatrixInfoNew("BLOSUM50", blosum50_values, blosum50_prefs, BLOSUM50_VALUES_MAX);
   ListNodeAddPointer(&retval, 0, matrix_info);

   matrix_info = MatrixInfoNew("BLOSUM45", blosum45_values, blosum45_prefs, BLOSUM45_VALUES_MAX);
   ListNodeAddPointer(&retval, 0, matrix_info);

   matrix_info = MatrixInfoNew("PAM250", pam250_values, pam250_prefs, PAM250_VALUES_MAX);
   ListNodeAddPointer(&retval, 0, matrix_info);

   matrix_info = MatrixInfoNew("BLOSUM62_20", blosum62_20_values, blosum62_20_prefs, BLOSUM62_20_VALUES_MAX);
   ListNodeAddPointer(&retval, 0, matrix_info);

   matrix_info = MatrixInfoNew("BLOSUM90", blosum90_values, blosum90_prefs, BLOSUM90_VALUES_MAX);
   ListNodeAddPointer(&retval, 0, matrix_info);

   matrix_info = MatrixInfoNew("PAM30", pam30_values, pam30_prefs, PAM30_VALUES_MAX);
   ListNodeAddPointer(&retval, 0, matrix_info);

   matrix_info = MatrixInfoNew("PAM70", pam70_values, pam70_prefs, PAM70_VALUES_MAX);
   ListNodeAddPointer(&retval, 0, matrix_info);

   return retval;
}

/** Obtains arrays of the allowed opening and extension penalties for gapped BLAST for
 * the given matrix.  Also obtains arrays of Lambda, K, and H.  Any of these fields that
 * are not required should be set to NULL.  The Int2 return value is the length of the arrays.
 * @param matrix name of the matrix [in]
 * @param open gap existence parameter [in|out]
 * @param extension cost to extend a gap by one letter [in|out]
 * @param decline_align cost to decline to extend an alignment [in|out]
 * @param lambda Karlin-Altschul parameter [in|out]
 * @param K Karlin-Altschul parameter [in|out]
 * @param H Karlin-Altschul parameter [in|out]
 * @param alpha Karlin-Altschul parameter [in|out]
 * @param beta Karlin-Altschul parameter [in|out]
 * @param pref_flags describes preferred values [in|out]
 * @return maximum number of values (length of arrays).
*/

static Int2
Blast_GetMatrixValues(const char* matrix, Int4** open, Int4** extension, Int4** decline_align, double** lambda, double** K, double** H, double** alpha, double** beta, Int4** pref_flags)

{
   array_of_8 *values = NULL;
   Boolean found_matrix=FALSE;
   Int4 index, max_number_values=0;
   Int4* open_array=NULL,* extension_array=NULL,* decline_align_array=NULL,* pref_flags_array=NULL,* prefs=NULL;
   double* lambda_array=NULL,* K_array=NULL,* H_array=NULL,* alpha_array=NULL,* beta_array=NULL;
   MatrixInfo* matrix_info;
   ListNode* vnp,* head;

   if (matrix == NULL)
      return 0;

   vnp = head = BlastLoadMatrixValues();

   while (vnp)
   {
      matrix_info = vnp->ptr;
      if (strcasecmp(matrix_info->name, matrix) == 0)
      {
         values = matrix_info->values;
         max_number_values = matrix_info->max_number_values;
         prefs = matrix_info->prefs;
         found_matrix = TRUE;
         break;
      }
      vnp = vnp->next;
   }

   if (found_matrix)
   {
      if (open)
         *open = open_array = (Int4 *) calloc(max_number_values, sizeof(Int4));
      if (extension)
         *extension = extension_array = 
            (Int4 *) calloc(max_number_values, sizeof(Int4));
      if (decline_align)
         *decline_align = decline_align_array = 
            (Int4 *) calloc(max_number_values, sizeof(Int4));
      if (lambda)
         *lambda = lambda_array = 
            (double*) calloc(max_number_values, sizeof(double));
      if (K)
         *K = K_array = (double*) calloc(max_number_values, sizeof(double));
      if (H)
         *H = H_array = (double*) calloc(max_number_values, sizeof(double));
      if (alpha)
         *alpha = alpha_array = (double*) calloc(max_number_values, sizeof(double));
      if (beta)
         *beta = beta_array = (double*) calloc(max_number_values, sizeof(double));
      if (pref_flags)
         *pref_flags = pref_flags_array = 
            (Int4 *) calloc(max_number_values, sizeof(Int4));

      for (index=0; index<max_number_values; index++)
      {
         if (open)
            open_array[index] = (Int4) values[index][0];
         if (extension)
            extension_array[index] = (Int4) values[index][1];
         if (decline_align)
            decline_align_array[index] = (Int4) values[index][2];
         if (lambda)
            lambda_array[index] = values[index][3];
         if (K)
            K_array[index] = values[index][4];
         if (H)
            H_array[index] = values[index][5];
         if (alpha)
            alpha_array[index] = values[index][6];
         if (beta)
            beta_array[index] = values[index][7];
         if (pref_flags)
            pref_flags_array[index] = prefs[index];
      }
   }

   BlastMatrixValuesDestruct(head);

   return max_number_values;
}

/*Extract the alpha and beta settings for this matrixName, and these
  gap open and gap extension costs*/
void BLAST_GetAlphaBeta(const char* matrixName, double *alpha,
                        double *beta, Boolean gapped, Int4 gap_open, 
                        Int4 gap_extend, const Blast_KarlinBlk* kbp_ungapped)
{
   Int4* gapOpen_arr,* gapExtend_arr,* pref_flags;
   double* alpha_arr,* beta_arr;
   Int2 num_values;
   Int4 i; /*loop index*/

   num_values = Blast_GetMatrixValues(matrixName, &gapOpen_arr, 
     &gapExtend_arr, NULL, NULL, NULL, NULL,  &alpha_arr, &beta_arr, 
     &pref_flags);

   if (gapped) {
     if ((0 == gap_open) && (0 == gap_extend)) {
       for(i = 1; i < num_values; i++) {
    if(pref_flags[i]==BLAST_MATRIX_BEST) {
      (*alpha) = alpha_arr[i];
      (*beta) = beta_arr[i];
      break;
    }
       }
     }
     else {
       for(i = 1; i < num_values; i++) {
    if ((gapOpen_arr[i] == gap_open) &&
        (gapExtend_arr[i] == gap_extend)) {
      (*alpha) = alpha_arr[i];
      (*beta) = beta_arr[i];
      break;
    }
       }
     }
   }
   else if (num_values > 0) {
     (*alpha) = alpha_arr[0];
     (*beta) = beta_arr[0];
   } else {
       *alpha = kbp_ungapped->Lambda / kbp_ungapped->H;
       *beta = 0;
   }

   sfree(gapOpen_arr);
   sfree(gapExtend_arr);
   sfree(pref_flags);
   sfree(alpha_arr);
   sfree(beta_arr);
}

Int2 BLAST_GetGapExistenceExtendParams(EBlastProgramType program_number,
                                       const char* matrixName, 
                                       Int4* gap_existence, 
                                       Int4* gap_extension)
{
   if (Blast_QueryIsNucleotide(program_number) == TRUE && Blast_QueryIsTranslated(program_number) == FALSE)
   {
       (*gap_existence) = BLAST_GAP_OPEN_NUCL;
       (*gap_extension) = BLAST_GAP_EXTN_NUCL;
   }
   else
   {
       Int4* gapOpen_arr,* gapExtend_arr,* pref_flags;
       Int4 i; /*loop index*/
       Int2 num_values = Blast_GetMatrixValues(matrixName, &gapOpen_arr, 
         &gapExtend_arr, NULL, NULL, NULL, NULL,  NULL, NULL,
         &pref_flags);

       if (num_values <= 0)
         return -1;
   
       for(i = 1; i < num_values; i++) {
           if(pref_flags[i]==BLAST_MATRIX_BEST) {
             (*gap_existence) = gapOpen_arr[i];
             (*gap_extension) = gapExtend_arr[i];
             break;
           }
       }
   
       sfree(gapOpen_arr);
       sfree(gapExtend_arr);
       sfree(pref_flags);
   }

   return 0;
}

/** Fills in error_return with strings describing the allowed values.
 * @param matrix_name name of the matrix [in]
 * @param error_return object to be filled in [in|out]
 * @return zero on success.
 */
static Int2
BlastKarlinReportAllowedValues(const char *matrix_name, 
   Blast_Message** error_return)
{
   array_of_8 *values = NULL;
   Boolean found_matrix=FALSE;
   Int4 max_number_values=0;
   ListNode* vnp,* head;
   MatrixInfo* matrix_info;

   vnp = head = BlastLoadMatrixValues();
   while (vnp)
   {
           matrix_info = vnp->ptr;
      if (strcasecmp(matrix_info->name, matrix_name) == 0)
      {
         values = matrix_info->values;
         max_number_values = matrix_info->max_number_values;
         found_matrix = TRUE;
         break;
      }
      vnp = vnp->next;
   }

   if (found_matrix)
   {
                Int4 index;
           char buffer[256];
      for (index=0; index<max_number_values; index++)
      {
         if (BLAST_Nint(values[index][2]) == INT2_MAX)
            sprintf(buffer, "Gap existence and extension values of %ld and %ld are supported", (long) BLAST_Nint(values[index][0]), (long) BLAST_Nint(values[index][1]));
         else
            sprintf(buffer, "Gap existence, extension and decline-to-align values of %ld, %ld and %ld are supported", (long) BLAST_Nint(values[index][0]), (long) BLAST_Nint(values[index][1]), (long) BLAST_Nint(values[index][2]));
         Blast_MessageWrite(error_return, eBlastSevError, 0, 0, buffer);
      }
   }

   BlastMatrixValuesDestruct(head);

   return 0;
}

/*
   Supplies lambda, H, and K values, as calcualted by Stephen Altschul 
   in Methods in Enzy. (vol 266, page 474).

   if kbp is NULL, then a validation is perfomed.
*/
Int2
Blast_KarlinBlkGappedCalc(Blast_KarlinBlk* kbp, Int4 gap_open, Int4 gap_extend, Int4 decline_align, const char* matrix_name, Blast_Message** error_return)

{
   char buffer[256];
   Int2 status = 
      Blast_KarlinBlkGappedLoadFromTables(kbp, gap_open, gap_extend, 
                                          decline_align, matrix_name);

   if (status && error_return)
   {
      if (status == 1)
      {
         MatrixInfo* matrix_info;
         ListNode* vnp,* head;      

         vnp = head = BlastLoadMatrixValues();

         sprintf(buffer, "%s is not a supported matrix", matrix_name);
         Blast_MessageWrite(error_return, eBlastSevError, 0, 0, buffer);

         while (vnp)
         {
            matrix_info = vnp->ptr;
            sprintf(buffer, "%s is a supported matrix", matrix_info->name);
            Blast_MessageWrite(error_return, eBlastSevError, 0, 0, buffer);
            vnp = vnp->next;
         }

         BlastMatrixValuesDestruct(head);
      }
      else if (status == 2)
      {
         if (decline_align == INT2_MAX)
            sprintf(buffer, "Gap existence and extension values of %ld and %ld not supported for %s", (long) gap_open, (long) gap_extend, matrix_name);
         else
            sprintf(buffer, "Gap existence, extension and decline-to-align values of %ld, %ld and %ld not supported for %s", (long) gap_open, (long) gap_extend, (long) decline_align, matrix_name);
         Blast_MessageWrite(error_return, eBlastSevError, 0, 0, buffer);
         BlastKarlinReportAllowedValues(matrix_name, error_return);
      }
   }

   return status;
}

/*
   Attempts to fill KarlinBlk for given gap opening, extensions etc.  
   Will return non-zero status if that fails.

   return values: -1 if matrix_name is NULL;
         1 if matrix not found
         2 if matrix found, but open, extend etc. values not supported.
*/
Int2
Blast_KarlinBlkGappedLoadFromTables(Blast_KarlinBlk* kbp, Int4 gap_open, 
                                    Int4 gap_extend, Int4 decline_align, 
                                    const char* matrix_name)
{
   Boolean found_matrix=FALSE;
   array_of_8 *values;
   Int2 status=0;
   Int4 max_number_values=0;
   MatrixInfo* matrix_info;
   ListNode* vnp,* head;
   
   if (matrix_name == NULL)
      return -1;

   values = NULL;

   vnp = head = BlastLoadMatrixValues();
   while (vnp)
   {
      matrix_info = vnp->ptr;
      if (strcasecmp(matrix_info->name, matrix_name) == 0)
      {
         values = matrix_info->values;
         max_number_values = matrix_info->max_number_values;
         found_matrix = TRUE;
         break;
      }
      vnp = vnp->next;
   }


   if (found_matrix)
   {
                Boolean found_values=FALSE;
           Int4 index;
      for (index=0; index<max_number_values; index++)
      {
         if (BLAST_Nint(values[index][0]) == gap_open &&
            BLAST_Nint(values[index][1]) == gap_extend &&
            (BLAST_Nint(values[index][2]) == INT2_MAX || BLAST_Nint(values[index][2]) == decline_align))
         {
            if (kbp)
            {
               kbp->Lambda = values[index][3];
               kbp->K = values[index][4];
               kbp->logK = log(kbp->K);
               kbp->H = values[index][5];
            }
            found_values = TRUE;
            break;
         }
      }

      if (found_values == TRUE)
      {
         status = 0;
      }
      else
      {
         status = 2;
      }
   }
   else
   {
      status = 1;
   }

   BlastMatrixValuesDestruct(head);

   return status;
}

char*
BLAST_PrintMatrixMessage(const char *matrix_name)
{
   char* buffer= (char *) calloc(1024, sizeof(char));
   char* ptr;
   MatrixInfo* matrix_info;
        ListNode* vnp,* head;

   ptr = buffer;
        sprintf(ptr, "%s is not a supported matrix, supported matrices are:\n", matrix_name);

   ptr += strlen(ptr);

        vnp = head = BlastLoadMatrixValues();

        while (vnp)
        {
         matrix_info = vnp->ptr;
         sprintf(ptr, "%s \n", matrix_info->name);
      ptr += strlen(ptr);
      vnp = vnp->next;
        }
        BlastMatrixValuesDestruct(head);

   return buffer;
}

char*
BLAST_PrintAllowedValues(const char *matrix_name, Int4 gap_open, Int4 gap_extend, Int4 decline_align) 
{
   array_of_8 *values = NULL;
   Boolean found_matrix=FALSE;
   char* buffer,* ptr;
   Int4 index, max_number_values=0;
   MatrixInfo* matrix_info;
   ListNode* vnp,* head;

   ptr = buffer = (char *) calloc(2048, sizeof(char));

        if (decline_align == INT2_MAX)
              sprintf(ptr, "Gap existence and extension values of %ld and %ld not supported for %s\nsupported values are:\n", 
      (long) gap_open, (long) gap_extend, matrix_name);
        else
               sprintf(ptr, "Gap existence, extension and decline-to-align values of %ld, %ld and %ld not supported for %s\n supported values are:\n", 
      (long) gap_open, (long) gap_extend, (long) decline_align, matrix_name);

   ptr += strlen(ptr);

   vnp = head = BlastLoadMatrixValues();
   while (vnp)
   {
      matrix_info = vnp->ptr;
      if (strcasecmp(matrix_info->name, matrix_name) == 0)
      {
         values = matrix_info->values;
         max_number_values = matrix_info->max_number_values;
         found_matrix = TRUE;
         break;
      }
      vnp = vnp->next;
   }

   if (found_matrix)
   {
      for (index=0; index<max_number_values; index++)
      {
         if (BLAST_Nint(values[index][2]) == INT2_MAX)
            sprintf(ptr, "%ld, %ld\n", (long) BLAST_Nint(values[index][0]), (long) BLAST_Nint(values[index][1]));
         else
            sprintf(ptr, "%ld, %ld, %ld\n", (long) BLAST_Nint(values[index][0]), (long) BLAST_Nint(values[index][1]), (long) BLAST_Nint(values[index][2]));
         ptr += strlen(ptr);
      }
   }

   BlastMatrixValuesDestruct(head);

   return buffer;
}

/** Returns the array of values corresponding to the given match/mismatch
 * scores, the number of supported gap cost combinations and thresholds for 
 * the gap costs, beyond which the ungapped statistics can be applied.
 * @param reward Match reward score [in]
 * @param penalty Mismatch penalty score [in]
 * @param array_size Number of supported combinations for this match/mismatch
 *                   pair [out]
 * @param gap_open_max Gap opening cost threshold for infinite gap costs [in]
 * @param gap_extend_max Gap extension cost threshold for infinite gap costs [in]
 * @param error_return Pointer to error message [in] [out]
 * @return Corresponding array of values.
 */
static const array_of_8 *
s_GetNuclValuesArray(Int4 reward, Int4 penalty, Int4* array_size,
                     Int4* gap_open_max, Int4* gap_extend_max,
                     Blast_Message** error_return)
{
    const array_of_8 * kValues = NULL;

    if (reward == 1 && penalty == -4) {
        kValues = blastn_values_1_4;
        *array_size = sizeof(blastn_values_1_4)/sizeof(array_of_8);
        *gap_open_max = 2;
        *gap_extend_max = 2;
    } else if (reward == 2 && penalty == -7) {
        kValues = blastn_values_2_7;
        *array_size = sizeof(blastn_values_2_7)/sizeof(array_of_8);
        *gap_open_max = 4;
        *gap_extend_max = 4;
    } else if (reward == 1 && penalty == -3) { 
        kValues = blastn_values_1_3;
        *array_size = sizeof(blastn_values_1_3)/sizeof(array_of_8);
        *gap_open_max = 2;
        *gap_extend_max = 2;
    } else if (reward == 2 && penalty == -5) {
        kValues = blastn_values_2_5;
        *array_size = sizeof(blastn_values_2_5)/sizeof(array_of_8);
        *gap_open_max = 4;
        *gap_extend_max = 4;
    } else if (reward == 1 && penalty == -2) {
        kValues = blastn_values_1_2;
        *array_size = sizeof(blastn_values_1_2)/sizeof(array_of_8);
        *gap_open_max = 2;
        *gap_extend_max = 2;
    } else if (reward == 2 && penalty == -3) {
        kValues = blastn_values_2_3;
        *array_size = sizeof(blastn_values_2_3)/sizeof(array_of_8);
        *gap_open_max = 6;
        *gap_extend_max = 4;
    } else if (reward == 1 && penalty == -1) {
        kValues = blastn_values_1_1;
        *array_size = sizeof(blastn_values_1_1)/sizeof(array_of_8);
        *gap_open_max = 4;
        *gap_extend_max = 2;
    } else if (reward == 4 && penalty == -5) {
        kValues = blastn_values_4_5;
        *array_size = sizeof(blastn_values_4_5)/sizeof(array_of_8);
        *gap_open_max = 12;
        *gap_extend_max = 8;
    } else if (reward == 5 && penalty == -4) {
        kValues = blastn_values_5_4;
        *array_size = sizeof(blastn_values_5_4)/sizeof(array_of_8);
        *gap_open_max = 25;
        *gap_extend_max = 10;
    } else if (error_return) {
        char buffer[256];
        /* Unsupported reward-penalty */
        sprintf(buffer, "Substitution scores %d and %d are not supported", 
                reward, penalty);
        Blast_MessageWrite(error_return, eBlastSevError, 0, 0, buffer);
    }

    return kValues;
}
   
Int2
Blast_KarlinBlkNuclGappedCalc(Blast_KarlinBlk* kbp, Int4 gap_open, 
                              Int4 gap_extend, Int4 reward, Int4 penalty,
                              Blast_KarlinBlk* kbp_ungap,
                              Blast_Message** error_return)
{
    const int kGapOpenIndex = 0;
    const int kGapExtIndex = 1;
    const int kLambdaIndex = 2;
    const int kKIndex = 3;
    const int kHIndex = 4;
    int num_combinations = 0;
    int index;
    int gap_open_max, gap_extend_max;

    const array_of_8 *kValues = 
        s_GetNuclValuesArray(reward, penalty, &num_combinations, 
                             &gap_open_max, &gap_extend_max, error_return);

    ASSERT(kbp && kbp_ungap);

#ifndef NEW_BLASTN_STAT
    Blast_KarlinBlkCopy(kbp, kbp_ungap);
    return 0;
#endif

    if (!kValues)
        return 1;

    /* Try to find the table entry corresponding to input gap costs. */
    for (index = 0; index < num_combinations; ++index) {
        if (kValues[index][kGapOpenIndex] == gap_open &&
            kValues[index][kGapExtIndex] == gap_extend) {
            kbp->Lambda = kValues[index][kLambdaIndex];
            kbp->K = kValues[index][kKIndex];
            kbp->logK = log(kbp->K);
            kbp->H = kValues[index][kHIndex];
            break;
        }
    }
    
    /* If gap costs are not found in the table, check if they belong to the
       infinite domain, where ungapped values of the parameters can be used. */
    if (index == num_combinations) {
        /* If gap costs are larger than maximal provided in tables, copy
           the values from the ungapped Karlin block. */
        if (gap_open >= gap_open_max && gap_extend >= gap_extend_max) {
            Blast_KarlinBlkCopy(kbp, kbp_ungap);
        } else if (error_return) {
            char buffer[256];
            /* Unsupported gap costs combination. */
            sprintf(buffer, "Gap existence and extension values %d and %d "
                    "are not supported for substitution scores %d and %d", 
                    gap_open, gap_extend, reward, penalty);
            Blast_MessageWrite(error_return, eBlastSevError, 0, 0, buffer);
            return 1;
        }
    }

    return 0;
}

/** Returns the beta statistical parameter value, given the nucleotide 
 * substitution scores.
 * @param reward Match reward score [in]
 * @param penalty Mismatch penalty score [in]
 * @return The value of the beta parameter.
 */
static double s_GetUngappedBeta(Int4 reward, Int4 penalty)
{
    double beta = 0;
    if ((reward == 1 && penalty == -1) || 
        (reward == 2 && penalty == -3))
        beta = -2;
    
    return beta;
}

Int2 Blast_GetNuclAlphaBeta(Int4 reward, Int4 penalty, Int4 gap_open, 
                            Int4 gap_extend, Blast_KarlinBlk* kbp,
                            Boolean gapped_calculation,
                            double *alpha, double *beta)
{
    const int kGapOpenIndex = 0;
    const int kGapExtIndex = 1;
    const int kAlphaIndex = 5;
    const int kBetaIndex = 6;
    Int4 num_combinations = 0;
    Int4 gap_open_max = 0, gap_extend_max = 0;
    Int4 index = 0;
    const array_of_8 *kValues = 
        s_GetNuclValuesArray(reward, penalty, &num_combinations, 
                             &gap_open_max, &gap_extend_max, NULL);;
    
    ASSERT(alpha && beta && kbp);

#ifndef NEW_BLASTN_STAT    
    *alpha = kbp->Lambda/kbp->H;
    *beta = 0;
    return 0;
#endif

    /* For ungapped search return ungapped values of alpha and beta. */
    if (gapped_calculation && kValues) {
        for (index = 0; index < num_combinations; ++index) {
            if (kValues[index][kGapOpenIndex] == gap_open && 
                kValues[index][kGapExtIndex] == gap_extend) {
                *alpha = kValues[index][kAlphaIndex];
                *beta = kValues[index][kBetaIndex];
                return 0;
            }
        }
    }

    /* If input values not found in tables, or if this is an ungapped search,
       return the ungapped values of alpha and beta. */
    *alpha = kbp->Lambda/kbp->H;
    *beta = s_GetUngappedBeta(reward, penalty);

    return 0;
}

/** Calculates score from expect value and search space.
 * @param E expect value [in]
 * @param kbp contains Karlin-Altschul parameters [in]
 * @param searchsp query times database size [in]
 * @return score
 */
static Int4
BlastKarlinEtoS_simple(double E, /* Expect value */
   const Blast_KarlinBlk*  kbp,
   Int8  searchsp)   /* size of search space */
{

   double   Lambda, K, H; /* parameters for Karlin statistics */
   Int4  S;
/* Smallest float that might not cause a floating point exception in
   S = (Int4) (ceil( log((double)(K * searchsp / E)) / Lambda )); below.  */
   const double kSmallFloat = 1.0e-297;

   Lambda = kbp->Lambda;
   K = kbp->K;
   H = kbp->H;
   if (Lambda < 0. || K < 0. || H < 0.0) 
   {
      return BLAST_SCORE_MIN;
   }

   E = MAX(E, kSmallFloat);

   S = (Int4) (ceil( log((double)(K * searchsp / E)) / Lambda ));
   return S;
}

/* Compute a divisor used to weight the evalue of a collection of
 * "nsegs" distinct alignments.  These divisors are used to compensate
 * for the effect of choosing the best among multiple collections of
 * alignments.  See
 *
 * Stephen F. Altschul. Evaluating the statitical significance of
 * multiple distinct local alignments. In Suhai, editior, Theoretical
 * and Computational Methods in Genome Research, pages 1-14. Plenum
 * Press, New York, 1997.
 *
 * The "decayrate" parameter of this routine is a value in the
 * interval (0,1). Typical values of decayrate are .1 and .5. */

double
BLAST_GapDecayDivisor(double decayrate, unsigned nsegs )
{
    return (1. - decayrate) * BLAST_Powi(decayrate, nsegs - 1);
}


/*
   BlastCutoffs
   Calculate the cutoff score, S, and the highest expected score.
*/
Int2
BLAST_Cutoffs(Int4 *S, /* cutoff score */
   double* E, /* expected no. of HSPs scoring at or above S */
   Blast_KarlinBlk* kbp,
   Int8 searchsp, /* size of search space. */
   Boolean dodecay,  /* TRUE ==> use gapdecay feature */
   double gap_decay_rate)
{
   Int4  s = *S, es;
   double   e = *E, esave;
   Boolean     s_changed = FALSE;

   if (kbp->Lambda == -1. || kbp->K == -1. || kbp->H == -1.)
      return 1;

   /*
   Calculate a cutoff score, S, from the Expected
   (or desired) number of reported HSPs, E.
   */
   es = 1;
   esave = e;
   if (e > 0.) 
   {
        if (dodecay) {
            /* Invert the adjustment to the e-value that will be applied
             * to compensate for the effect of choosing the best among
             * multiple alignments */
            if( gap_decay_rate > 0 && gap_decay_rate < 1 ) {
                e *= BLAST_GapDecayDivisor(gap_decay_rate, 1);
            }
        }
        es = BlastKarlinEtoS_simple(e, kbp, searchsp);
   }
   /*
   Pick the larger cutoff score between the user's choice
   and that calculated from the value of E.
   */
   if (es > s) {
      s_changed = TRUE;
      *S = s = es;
   }

   /*
      Re-calculate E from the cutoff score, if E going in was too high
   */
   if (esave <= 0. || !s_changed) 
   {
      e = BLAST_KarlinStoE_simple(s, kbp, searchsp);
      if (dodecay) {
            /* Weight the e-value to compensate for the effect of
             * choosing the best of more than one collection of
             * distinct alignments */
            if( gap_decay_rate > 0 && gap_decay_rate < 1 ) {
                e /= BLAST_GapDecayDivisor(gap_decay_rate, 1);
            }
        }
      *E = e;
   }

   return 0;
}

/*
   BlastKarlinStoE() -- given a score, return the associated Expect value

   Error return value is -1.
*/
double
BLAST_KarlinStoE_simple(Int4 S,
      Blast_KarlinBlk* kbp,
      Int8  searchsp)   /* size of search space. */
{
   double   Lambda, K, H; /* parameters for Karlin statistics */

   Lambda = kbp->Lambda;
   K = kbp->K;
   H = kbp->H;
   if (Lambda < 0. || K < 0. || H < 0.) {
      return -1.;
   }

   return (double) searchsp * exp((double)(-Lambda * S) + kbp->logK);
}

/** BlastKarlinPtoE -- convert a P-value to an Expect value
 * When using BlastKarlinPtoE in the context of a database search,
 * the returned E-value should be multiplied by the effective
 * length of the database and divided by the effective lnegth of
 * the subject.
 * @param p the P-value to be converted [in]
 * @return the corresponding expect value.
*/
static double
BlastKarlinPtoE(double p)
{
        if (p < 0. || p > 1.0) 
   {
                return INT4_MIN;
        }

   if (p == 1)
      return INT4_MAX;

        return -BLAST_Log1p(-p);
}

/** Internal data structure used by Romberg integration callbacks. */
typedef struct SRombergCbackArgs {
   int num_hsps;          /**< number of HSPs */
   int num_hsps_minus_2;  /**< number of HSPs minus 2 */
   double adj1;              /**< Nat log of r**(r-2)/((r-1)! (r-2)!) */
   double adj2;              /**< adj1 - score */
   double sdvir;             /**< score divided by number of HSPs. */
   double epsilon;           /**< convergence criteria for Romberg integration. */
} SRombergCbackArgs;


/** Callback for the Romberg integration function.
 *  This is the second of the double integrals that  
 *  BlastSumPCalc calculates This is eqn. 4 of Karlin 
 *  and Altschul, PNAS USA, 90, 5873-5877 (1993).
 * 
 * @param x variable to integrate over [in]
 * @param vp pointer to parameters [in]
 * @return value of integrand
 */  
static double
s_OuterIntegralCback(double x, void* vp)
{
   SRombergCbackArgs* callback_args = (SRombergCbackArgs*) vp;
   double y = exp(x - callback_args->sdvir);

   if (y == HUGE_VAL)
      return 0.;

   if (callback_args->num_hsps_minus_2 == 0)
      return exp(callback_args->adj2 - y);
   if (x == 0.)
      return 0.;
   return exp((callback_args->num_hsps_minus_2)*log(x) + callback_args->adj2 - y);
}

/** Callback for the Romberg integration function.
 *  This is the first of the double integrals that  
 *  BlastSumPCalc calculates.  This is the integral 
 *  described in the paragraph after eqn. 4 of Karlin 
 *  and Altschul, PNAS USA, 90, 5873-5877 (1993).
 * 
 * @param s variable to integrate over [in]
 * @param vp pointer to parameters [in]
 * @return value of integrand
 */  
static double
s_InnerIntegralCback(double s, void* vp)
{
   double   mx;
   SRombergCbackArgs* callback_args = (SRombergCbackArgs*) vp;
   
   callback_args->adj2 = callback_args->adj1 - s;
   callback_args->sdvir = s / callback_args->num_hsps;
   mx = (s > 0. ? callback_args->sdvir + 3. : 3.);
   return BLAST_RombergIntegrate(s_OuterIntegralCback, vp, 0., mx, callback_args->epsilon, 0, 1);
}

/**
 *
 *   Evaluate the following double integral, where r = number of segments
 *
 *   and s = the adjusted score in nats:
 *
 *                   (r-2)         oo           oo
 *    Prob(r,s) =   r              -            -   (r-2)
 *                -------------   |   exp(-y)  |   x   exp(-exp(x - y/r)) dx dy
 *                (r-1)! (r-2)!  U            U
 *                               s            0
 * @param r number of segments
 * @param s adjusted score in nats
 * @return P value
 */
static double
s_BlastSumPCalc(int r, double s)
{
   int      r1, itmin;
   double   t, d;
   double   mean, stddev, stddev4;
   double   logr;
   SRombergCbackArgs callback_args;
   const double kSumpEpsilon = 0.002;

   if (r == 1) {
      if (s > 8.)
         return exp(-s);
      return -BLAST_Expm1(-exp(-s));
   }
   if (r < 1)
      return 0.;

   if (r < 8) {
      if (s <= -2.3*r)
         return 1.;
   }
   else if (r < 15) {
         if (s <= -2.5*r)
            return 1.;
   }
   else if (r < 27) {
         if (s <= -3.0*r)
            return 1.;
   }
   else if (r < 51) {
         if (s <= -3.4*r)
            return 1.;
   }
   else if (r < 101) {
         if (s <= -4.0*r)
            return 1.;
   }

   /* stddev in the limit of infinite r, but quite good for even small r */
   stddev = sqrt(r);
   stddev4 = 4.*stddev;
   r1 = r - 1;

   if (r > 100) {
      /* Calculate lower bound on the mean using inequality log(r) <= r */
      double est_mean = -r * r1;
      if (s <= est_mean - stddev4)
         return 1.;
   }

   /* mean is rather close to the mode, and the mean is readily calculated */
   /* mean in the limit of infinite r, but quite good for even small r */
   logr = log(r);
   mean = r * (1. - logr) - 0.5;
   if (s <= mean - stddev4)
      return 1.;

   if (s >= mean) {
      t = s + 6.*stddev;
      itmin = 1;
   }
   else {
      t = mean + 6.*stddev;
      itmin = 2;
   }

   memset((void *)&callback_args, 0, sizeof(callback_args));
   callback_args.num_hsps = r;
   callback_args.num_hsps_minus_2 = r - 2;
   callback_args.adj1 = callback_args.num_hsps_minus_2*logr - BLAST_LnGammaInt(r1) - BLAST_LnGammaInt(r);
   callback_args.epsilon = kSumpEpsilon;

   do {
      d = BLAST_RombergIntegrate(s_InnerIntegralCback, &callback_args, s, t, callback_args.epsilon, 0, itmin);
      if (d == HUGE_VAL)
         return d;
   } while (s < mean && d < 0.4 && itmin++ < 4);

   return (d < 1. ? d : 1.);
}

/** Estimate the Sum P-value by calculation or interpolation, as appropriate.
 * Approx. 2-1/2 digits accuracy minimum throughout the range of r, s.
 * @param r number of segments [in]
 * @param s total score (in nats), adjusted by -r*log(KN) [in]
 * @return p-value 
*/
static double
s_BlastSumP(Int4 r, double s)
{
    static const double  kTab2[] = { /* table for r == 2 */
         0.01669,  0.0249,   0.03683,  0.05390,  0.07794,  0.1111,   0.1559,   0.2146,   
         0.2890,   0.3794,   0.4836,   0.5965,   0.7092,   0.8114,   0.8931,   0.9490,   
         0.9806,   0.9944,   0.9989
         };  
    static const double  kTab3[] = { /* table for r == 3 */
         0.9806,   0.9944,   0.9989,   0.0001682,0.0002542,0.0003829,0.0005745,0.0008587,
         0.001278, 0.001893, 0.002789, 0.004088, 0.005958, 0.008627, 0.01240,  0.01770,  
         0.02505,  0.03514,  0.04880,  0.06704,  0.09103,  0.1220,   0.1612,   0.2097,   
         0.2682,   0.3368,   0.4145,   0.4994,   0.5881,   0.6765,   0.7596,   0.8326,   
         0.8922,   0.9367,   0.9667,   0.9846,   0.9939,   0.9980
         };
   static const double  kTab4[] = { /* table for r == 4 */
         2.658e-07,4.064e-07,6.203e-07,9.450e-07,1.437e-06,2.181e-06,3.302e-06,4.990e-06,
         7.524e-06,1.132e-05,1.698e-05,2.541e-05,3.791e-05,5.641e-05,8.368e-05,0.0001237,
         0.0001823,0.0002677,0.0003915,0.0005704,0.0008275,0.001195, 0.001718, 0.002457,
         0.003494, 0.004942, 0.006948, 0.009702, 0.01346,  0.01853,  0.02532,  0.03431,
         0.04607,  0.06128,  0.08068,  0.1051,   0.1352,   0.1719,   0.2157,   0.2669,
         0.3254,   0.3906,   0.4612,   0.5355,   0.6110,   0.6849,   0.7544,   0.8168,
         0.8699,   0.9127,   0.9451,   0.9679,   0.9827,   0.9915,   0.9963
         };
   const double* kTable[] = { kTab2, kTab3, kTab4 }; 
   const int kTabsize[] = { DIM(kTab2)-1, DIM(kTab3)-1, DIM(kTab4)-1 };  /* sizes of tab2, tab3, tab4 */

   if (r == 1)
      return -BLAST_Expm1(-exp(-s));

   if (r <= 4) {
      Int4     r1;
      double   a;

      if (r < 1)
         return 0.;
      r1 = r - 1;
      if (s >= r*r+r1) {
         a = BLAST_LnGammaInt(r+1);
         return r * exp(r1*log(s)-s-a-a);
      }
      if (s > -2*r) {
         Int4 i, r2;
         /* interpolate */
         i = (Int4) (a = s+s+(4*r));
         a -= i;
         i = kTabsize[r2 = r - 2] - i;
         return a*kTable[r2][i-1] + (1.-a)*kTable[r2][i];
      }
      return 1.;
   }

   return s_BlastSumPCalc(r, s);
}

/*
    Calculates the e-value for alignments with "small" gaps (typically
    under fifty residues/basepairs) following ideas of Stephen Altschul's.
*/

double
BLAST_SmallGapSumE(
    Int4 starting_points,      /* the number of starting points
                                 * permitted between adjacent
                                 * alignments;
                                 * max_overlap + max_gap + 1 */
    Int2 num,                   /* the number of distinct alignments in this
                                 * collection */
    double xsum,                /* the sum of the scores of these
                                 * alignments, each weighted normalized
                                 * using an appropriate value of
                                 * Lambda and logK */
    Int4 query_length,          /* the effective len of the query seq */
    Int4 subject_length,        /* the effective len of the database seq */
    Int8 searchsp_eff,          /* the effective size of the search space */
    double weight_divisor)      /* a divisor used to weight the e-value
                                 * when multiple collections of alignments
                                 * are being considered by the calling
                                 * routine */
{

    double sum_e;               /* The e-value of this set of alignments */

    if(num == 1) {
        sum_e = searchsp_eff * exp(-xsum);
    } else {
        double pair_search_space;  /* The effective size of the search
                                 * space, for this query-subject pair */
        double sum_p;           /* The p-value of this set of alignments */
      
        pair_search_space = (double)subject_length * (double)query_length;

        xsum -=
            log(pair_search_space) + 2 * (num-1)*log((double)starting_points);

        xsum -= BLAST_LnFactorial((double) num);

        sum_p = s_BlastSumP(num, xsum);
        sum_e = BlastKarlinPtoE(sum_p) *
            ((double) searchsp_eff / (double) pair_search_space);
    }
    if( weight_divisor == 0.0 || (sum_e /= weight_divisor) > INT4_MAX ) {
        sum_e = INT4_MAX;
    }

    return sum_e;
}

/** Calculates the e-value of a collection multiple distinct
 * alignments with asymmetric gaps between the alignments. The gaps
 * in one (protein) sequence are typically small (like in
 * BLAST_SmallGapSumE) gap an the gaps in the other (translated DNA)
 * sequence are possibly large (up to 4000 bp.)  This routine is used
 * for linking HSPs representing exons in the DNA sequence that are
 * separated by introns.
 * @param query_start_points  the number of starting points in
 *                            the query sequence permitted
 *                            between adjacent alignments [in]
 * @param subject_start_points the number of starting points in
 *                             the subject sequence permitted
 *                             between adjacent alignments [in]
 * @param num The number of distinct alignments in this collection [in]
 * @param xsum The sum of the scores of these alignments, each normalized
 *                     using an appropriate value of Lambda and logK [in]
 * @param query_length The effective len of the query seq [in]
 * @param subject_length The effective len of the database seq [in]
 * @param searchsp_eff effective size of the search space [in]
 * @param weight_divisor A divisor used to weight the e-value when multiple 
 *                       collections of alignments are being considered by 
 *                       the calling routine [in]
 * @return Resulting e-value of a combined set.
 */
double
BLAST_UnevenGapSumE(Int4 query_start_points, Int4 subject_start_points,
                    Int2 num, double xsum,
                    Int4 query_length, Int4 subject_length,
                    Int8 searchsp_eff,
                    double weight_divisor)
{
    double sum_e;          /* The e-value of this set of alignments */

    if(num == 1) {
        sum_e = searchsp_eff * exp(-xsum);
    } else {
        double sum_p;          /* The p-value of this set of alignments */

        double pair_search_space;  /* The effective size of the search
                                 * space, for this query-subject pair */
        pair_search_space = (double)subject_length*(double)query_length;

        xsum -= log(pair_search_space) +
            (num-1)*(log((double) query_start_points) +
                     log((double) subject_start_points));
        xsum -= BLAST_LnFactorial((double) num);

        sum_p = s_BlastSumP(num, xsum);
        sum_e = BlastKarlinPtoE(sum_p) *
            ((double) searchsp_eff / (double) pair_search_space);
    }
    if( weight_divisor == 0.0 || (sum_e /= weight_divisor) > INT4_MAX ) {
        sum_e = INT4_MAX;
    }

    return sum_e;
}


/*
    Calculates the e-value if a collection of distinct alignments with
    arbitrarily large gaps between the alignments
*/

double
BLAST_LargeGapSumE(
    Int2 num,                   /* the number of distinct alignments in this
                                 * collection */
    double      xsum,           /* the sum of the scores of these
                                 * alignments each, normalized using an
                                 * appropriate value of Lambda and
                                 * logK */
    Int4 query_length,          /* the effective len of the query seq */
    Int4 subject_length,        /* the effective len of the database seq */
    Int8 searchsp_eff,          /* the effective size of the search space */
    double weight_divisor)      /* a divisor used to weight the e-value
                                 * when multiple collections of alignments
                                 * are being considered by the calling
                                 * routine */
{
    double sum_p;               /* The p-value of this set of alignments */
    double sum_e;               /* The e-value of this set of alignments */

/* The next two variables are for compatability with Warren's code. */
    double lcl_subject_length;     /* query_length as a float */
    double lcl_query_length;       /* subject_length as a float */

    lcl_query_length = (double) query_length;
    lcl_subject_length = (double) subject_length;

    if( num == 1 ) {
      sum_e = searchsp_eff * exp(-xsum);
    } else {
      xsum -= num*log(lcl_subject_length*lcl_query_length)
        - BLAST_LnFactorial((double) num);
      
      sum_p = s_BlastSumP(num, xsum);
      
      sum_e = BlastKarlinPtoE(sum_p) *
          ((double) searchsp_eff / (lcl_query_length * lcl_subject_length));
    }
    if( weight_divisor == 0.0 || (sum_e /= weight_divisor) > INT4_MAX ) {
        sum_e = INT4_MAX;
    }

    return sum_e;
}

/* Please see comment in blast_stat.h */
void 
Blast_FillResidueProbability(const Uint1* sequence, Int4 length, double * resProb)
{
    Int4 frequency[BLASTAA_SIZE];  /*frequency of each letter*/
    Int4 i;                             /*index*/
    Int4 denominator;                   /*length not including X's*/

    denominator = length;
    for(i = 0; i < BLASTAA_SIZE; i++)
        frequency[i] = 0;

    for(i = 0; i < length; i++) {
        if (sequence[i] != AMINOACID_TO_NCBISTDAA['X'])
            frequency[sequence[i]]++;
        else
            denominator--;
    }

    for(i = 0; i < BLASTAA_SIZE; i++) {
        if (frequency[i] == 0)
            resProb[i] = 0.0;
        else
            resProb[i] = ((double) frequency[i]) /((double) denominator);
    }
}

/*------------------- RPS BLAST functions --------------------*/

/** Gets the ungapped lambda calculated for the matrix in question given
 * standard residue composition for both query and subject sequences.
 * @param matrixName name of amino acid substitution matrix [in]
 * @return lambda ungapped or 0.0 if matrix is not supported
 */
static double
RPSfindUngappedLambda(const char *matrixName)
{
    double* lambda_array = NULL;
    int num_lambdas = Blast_GetMatrixValues(matrixName, NULL, NULL, NULL,
                                            &lambda_array, NULL, NULL, NULL,
                                            NULL, NULL);
    if (num_lambdas > 0) {
        double retval = lambda_array[0];
        sfree(lambda_array);
        return retval;
    } else {
        return 0.0;
    }
}

/** 
 *  the routine RPSFillScores computes the probability of each score weighted
 *       by the probability of each query residue and fills those probabilities
 *       into scoreArray and puts scoreArray as a field in
 *       that in the structure that is returned
 *  for indexing convenience the field storing scoreArray points to the
 *       entry for score 0, so that referring to the -k index corresponds to
 *       score -k 
 *FIXME: This can be replaced by _PSIComputeScoreProbabilities??
 * @param matrix a position-specific score matrix with matrixLength positions [in]
 * @param matrixLength number of positions in the pssm (arg above) [in]
 * @param queryProbArray an array containing the probability of occurrence
 *       of each residue in the query [in]
 * @param scoreArray an array of probabilities for each score that is
 *       to be used as a field in return_sfp
 * @param return_sfp a structure to be filled in and returned [in|out]
 * @param range the size of scoreArray and is an upper bound on the
 *       difference between maximum score and minimum score in the matrix [in]
*/

static void
RPSFillScores(Int4 **matrix, Int4 matrixLength, 
              double *queryProbArray, double *scoreArray,  
              Blast_ScoreFreq* return_sfp, Int4 range)
{
    Int4 minScore, maxScore;    /*observed minimum and maximum scores */
    Int4 i,j;                   /* indices */
    double recipLength;        /* 1/matrix length as a double */

    minScore = maxScore = 0;

    for (i = 0; i < matrixLength; i++) {
        for (j = 0 ; j < BLASTAA_SIZE; j++) {
            if (j == AMINOACID_TO_NCBISTDAA['X'])
                continue;
            if ((matrix[i][j] > BLAST_SCORE_MIN) && 
                (matrix[i][j] < minScore))
                minScore = matrix[i][j];
            if (matrix[i][j] > maxScore)
                maxScore = matrix[i][j];
        }
    }

    return_sfp->obs_min = minScore;
    return_sfp->obs_max = maxScore;
    for (i = 0; i < range; i++)
        scoreArray[i] = 0.0;

    return_sfp->sprob = &(scoreArray[-minScore]); /*center around 0*/
    recipLength = 1.0 / (double) matrixLength;
    for(i = 0; i < matrixLength; i++) {
        for (j = 0; j < BLASTAA_SIZE; j++) {
            if (j == AMINOACID_TO_NCBISTDAA['X'])
                continue;
            if(matrix[i][j] >= minScore)
                return_sfp->sprob[matrix[i][j]] += recipLength * 
                                                queryProbArray[j];
        }
    }

    return_sfp->score_avg = 0;
    for(i = minScore; i <= maxScore; i++)
        return_sfp->score_avg += i * return_sfp->sprob[i];
}

Int4 **
RPSRescalePssm(double scalingFactor, Int4 rps_query_length, 
               const Uint1* rps_query_seq, Int4 db_seq_length, 
               Int4 **posMatrix, const char *matrix_name)
{
    double *scoreArray;         /*array of score probabilities*/
    double *resProb;            /*array of probabilities for each residue*/
    Blast_ScoreFreq * return_sfp;/*score frequency pointers to compute lambda*/
    Int4* * returnMatrix;        /*the PSSM to return */
    double initialUngappedLambda; 
    double scaledInitialUngappedLambda; 
    double correctUngappedLambda;
    double finalLambda;
    double temp;               /*intermediate variable for adjusting matrix*/
    Int4 index, inner_index; 

    resProb = (double *)malloc(BLASTAA_SIZE * sizeof(double));
    scoreArray = (double *)malloc(BLAST_SCORE_RANGE_MAX * sizeof(double));
    return_sfp = (Blast_ScoreFreq *)malloc(sizeof(Blast_ScoreFreq));

    Blast_FillResidueProbability(rps_query_seq, rps_query_length, resProb);

    RPSFillScores(posMatrix, db_seq_length, resProb, scoreArray, 
                 return_sfp, BLAST_SCORE_RANGE_MAX);

    initialUngappedLambda = RPSfindUngappedLambda(matrix_name);
    ASSERT(initialUngappedLambda > 0.0);
    scaledInitialUngappedLambda = initialUngappedLambda / scalingFactor;
    correctUngappedLambda = Blast_KarlinLambdaNR(return_sfp, 
                                              scaledInitialUngappedLambda);
    sfree(resProb);
    sfree(scoreArray);
    sfree(return_sfp);
    if(correctUngappedLambda == -1.0)
        return NULL;

    finalLambda = correctUngappedLambda/scaledInitialUngappedLambda;

    returnMatrix = (Int4 **)_PSIAllocateMatrix(db_seq_length,
                                               BLASTAA_SIZE,
                                               sizeof(Int4));

    for (index = 0; index < db_seq_length; index++) {
        for (inner_index = 0; inner_index < BLASTAA_SIZE; inner_index++) {
            if (posMatrix[index][inner_index] <= BLAST_SCORE_MIN || 
                inner_index == AMINOACID_TO_NCBISTDAA['X']) {
                returnMatrix[index][inner_index] = 
                    posMatrix[index][inner_index];
            }
            else {
               temp = ((double)(posMatrix[index][inner_index])) * finalLambda;
               returnMatrix[index][inner_index] = BLAST_Nint(temp);
           }
        }
    }

    return returnMatrix;
}

/** 
 * Computes the adjustment to the lengths of the query and database sequences
 * that is used to compensate for edge effects when computing evalues. 
 *
 * The length adjustment is an integer-valued approximation to the fixed
 * point of the function
 *
 *    f(ell) = beta + 
 *               (alpha/lambda) * (log K + log((m - ell)*(n - N ell)))
 *
 * where m is the query length n is the length of the database and N is the
 * number of sequences in the database. The values beta, alpha, lambda and
 * K are statistical, Karlin-Altschul parameters.
 * 
 * The value of the length adjustment computed by this routine, A, 
 * will always be an integer smaller than the fixed point of
 * f(ell). Usually, it will be the largest such integer.  However, the
 * computed length adjustment, A, will also be so small that 
 *
 *    K * (m - A) * (n - N * A) > MAX(m,n).
 *
 * Moreover, an iterative method is used to compute A, and under
 * unusual circumstances the iterative method may not converge. 
 *
 * @param K      the statistical parameter K
 * @param logK   the natural logarithm of K
 * @param alpha_d_lambda    the ratio of the statistical parameters 
 *                          alpha and lambda (for ungapped alignments, the
 *                          value 1/H should be used)
 * @param beta              the statistical parameter beta (for ungapped
 *                          alignments, beta == 0)
 * @param query_length      the length of the query sequence
 * @param db_length         the length of the database
 * @param db_num_seqs       the number of sequences in the database
 * @param length_adjustment the computed value of the length adjustment [out]
 *
 * @return   0 if length_adjustment is known to be the largest integer less
 *           than the fixed point of f(ell); 1 otherwise.
 */
Int4
BLAST_ComputeLengthAdjustment(double K,
                             double logK,
                             double alpha_d_lambda,
                             double beta,
                             Int4 query_length,
                             Int8 db_length,
                             Int4 db_num_seqs,
                             Int4 * length_adjustment)
{
    Int4 i;                     /* iteration index */
    const Int4 kMaxIterations = 20;     /* maximum allowed iterations */
    double m = (double) query_length;
    double n = (double) db_length;
    double N = (double) db_num_seqs;

    double ell;                 /* A float value of the length adjustment */
    double ss;                  /* effective size of the search space */
    double ell_min = 0, ell_max;   /* At each iteration i,
                                         * ell_min <= ell <= ell_max. */
    Boolean converged    = FALSE;       /* True if the iteration converged */
    double ell_next = 0;        /* Value the variable ell takes at iteration
                                 * i + 1 */
    /* Choose ell_max to be the largest nonnegative value that satisfies
     *
     *    K * (m - ell) * (n - N * ell) > MAX(m,n)
     *
     * Use quadratic formula: 2 c /( - b + sqrt( b*b - 4 * a * c )) */
    { /* scope of a, mb, and c, the coefficients in the quadratic formula
       * (the variable mb is -b) */
        double a  = N;
        double mb = m * N + n;
        double c  = n * m - MAX(m, n) / K;

        if(c < 0) {
            *length_adjustment = 0;
            return 1;
        } else {
            ell_max = 2 * c / (mb + sqrt(mb * mb - 4 * a * c));
        }
    } /* end scope of a, mb and c */

    for(i = 1; i <= kMaxIterations; i++) {      /* for all iteration indices */
        double ell_bar;         /* proposed next value of ell */
        ell      = ell_next;
        ss       = (m - ell) * (n - N * ell);
        ell_bar  = alpha_d_lambda * (logK + log(ss)) + beta;
        if(ell_bar >= ell) { /* ell is no bigger than the true fixed point */
            ell_min = ell;
            if(ell_bar - ell_min <= 1.0) {
                converged = TRUE;
                break;
            }
            if(ell_min == ell_max) { /* There are no more points to check */
                break;
            }
        } else { /* else ell is greater than the true fixed point */
            ell_max = ell;
        }
        if(ell_min <= ell_bar && ell_bar <= ell_max) { 
          /* ell_bar is in range. Accept it */
            ell_next = ell_bar;
        } else { /* else ell_bar is not in range. Reject it */
            ell_next = (i == 1) ? ell_max : (ell_min + ell_max) / 2;
        }
    } /* end for all iteration indices */
    if(converged) { /* the iteration converged */
        /* If ell_fixed is the (unknown) true fixed point, then we
         * wish to set (*length_adjustment) to floor(ell_fixed).  We
         * assume that floor(ell_min) = floor(ell_fixed) */
        *length_adjustment = (Int4) ell_min;
        /* But verify that ceil(ell_min) != floor(ell_fixed) */
        ell = ceil(ell_min);
        if( ell <= ell_max ) {
          ss = (m - ell) * (n - N * ell);
          if(alpha_d_lambda * (logK + log(ss)) + beta >= ell) {
            /* ceil(ell_min) == floor(ell_fixed) */
            *length_adjustment = (Int4) ell;
          }
        }
    } else { /* else the iteration did not converge. */
        /* Use the best value seen so far */
        *length_adjustment = (Int4) ell_min;
    }

    return converged ? 0 : 1;
}

/*
 * ===========================================================================
 *
 * $Log$
 * Revision 1.125  2005/08/30 15:42:58  madden
 * BLAST_GetGapExistenceExtendParams now takes program_number as an argument so it can properly identify blastn queries
 *
 * Revision 1.124  2005/08/29 13:52:05  madden
 * Add BLAST_GetGapExistenceExtendParams
 *
 * Revision 1.123  2005/08/19 17:56:18  dondosha
 * Removed unnecessary redefinition of HUGE_VAL
 *
 * Revision 1.122  2005/08/15 20:41:44  dondosha
 * New blastn statistics can be enabled with a -DNEW_BLASTN_STAT only with a compilation flag
 *
 * Revision 1.121  2005/08/15 16:11:43  dondosha
 * Use precomputed statistical parameters for blastn
 *
 * Revision 1.120  2005/08/09 14:16:03  dondosha
 * From A. Schaffer: added comments to clarify usage of BlastKarlinPtoE
 *
 * Revision 1.119  2005/08/01 17:30:31  dondosha
 * Added functions to return Lambda, K, alpha, beta for nucleotide search, given substitution and gap scores
 *
 * Revision 1.118  2005/07/12 22:57:03  bealer
 * - Change "BlastQueryInfo*" to "const BlastQueryInfo*" in several places.
 *
 * Revision 1.117  2005/06/20 13:09:36  madden
 * Rename BlastSeverity enums in line with C++ tookit convention
 *
 * Revision 1.116  2005/06/03 16:22:37  lavr
 * Explicit (unsigned char) casts in ctype routines
 *
 * Revision 1.115  2005/04/27 17:20:15  papadopo
 * copy X scores to U scores when building score matrix
 *
 * Revision 1.114  2005/02/14 14:09:00  camacho
 * Replaced SBLASTMatrixStructure by SBlastScoreMatrix.
 * Added SPsiBlastScoreMatrix structure to support PSSMs.
 * Removed obsolete fields from the BlastScoreBlk.
 * Renamed RPSCalculatePSSM to RPSRescalePssm.
 * Renamed functions to read/load protein scoring matrices.
 *
 * Revision 1.113  2005/01/31 16:59:19  dondosha
 * Bug fix in BlastKarlinLHtoK when penalty == -reward
 *
 * Revision 1.112  2005/01/28 13:50:43  madden
 * Fix typos in doxygen comments
 *
 * Revision 1.111  2005/01/27 13:59:17  madden
 * Minor doxygen fix
 *
 * Revision 1.110  2005/01/27 13:17:16  madden
 * 1.) moved #define BLAST_SUMP_EPSILON_DEFAULT to a const variable within s_BlastSumPCalc.
 * 2.) made static arrays tab[234] etc. const static arrays within one function.
 * 3.) introduced structure SRombergCbackArgs to replace confusing use of #defines starting with ARG_
 * 4.) renamed Romberg callbacks f and g to s_OuterIntegralCback and s_InnerIntegralCback
 * 5.) cleaned up and removed extra variables from s_BlastSumPCalc
 * 6.) renamed some static functions to start with s_ per C++ toolkit naming convention.
 * 7.) added doxygen comments
 *
 * Revision 1.109  2005/01/25 17:30:24  madden
 * Doxygen fixes
 *
 * Revision 1.108  2004/12/09 16:13:33  dondosha
 * Cast Uint1* to char* in call to Blast_ResFreqString, to remove SunOS compiler warning
 *
 * Revision 1.107  2004/12/09 16:05:36  dondosha
 * Added return on success in Blast_KarlinBlkCopy
 *
 * Revision 1.106  2004/12/09 15:56:55  dondosha
 * Return type of Blast_KarlinBlkCopy changed to Int2: check for null arguments; removed no-longer-relevant comments; removed all tabs
 *
 * Revision 1.105  2004/12/09 15:22:56  dondosha
 * Renamed some functions dealing with BlastScoreBlk and Blast_KarlinBlk structures
 *
 * Revision 1.104  2004/11/30 18:34:28  camacho
 * Rename Blast_KarlinBlkStandardCalc to Blast_ScoreBlkChooseUngappedOrIdealKbp
 *
 * Revision 1.103  2004/11/30 15:27:16  camacho
 * Rename Blast_ResFreqDestruct to Blast_ResFreqFree
 *
 * Revision 1.102  2004/11/29 13:53:23  camacho
 * Renamed Blast_ScoreFreq structure free function
 *
 * Revision 1.101  2004/11/24 16:02:51  dondosha
 * Added and/or fixed doxygen comments
 *
 * Revision 1.100  2004/11/23 21:47:56  camacho
 * Changed signature of Blast_ScoreBlkKbpIdealCalc so that it is used only
 * to initialize kbp_ideal field of BlastScoreBlk.
 * Rename Blast_KarlinBlk* allocation/deallocation structures
 * to follow standard naming conventions.
 *
 * Revision 1.99  2004/11/15 16:34:45  dondosha
 * Changed constants names in accordance with C++ toolkit guidelines
 *
 * Revision 1.98  2004/11/02 17:56:48  camacho
 * Add DOXYGEN_SKIP_PROCESSING to guard rcsid string
 *
 * Revision 1.97  2004/10/05 21:32:55  camacho
 * Return 1 to indicate error in BlastScoreBlkMatrixLoad
 *
 * Revision 1.96  2004/10/04 13:38:53  camacho
 * Do not use hard coded constants in RPSfindUngappedLambda
 *
 * Revision 1.95  2004/10/01 14:52:05  camacho
 * Add PAM250 to list of matrices to load from tables library
 *
 * Revision 1.94  2004/10/01 13:58:59  camacho
 * Remove extra PSSM column for RPS-BLAST
 *
 * Revision 1.93  2004/09/29 20:39:59  papadopo
 * Retrieve the ungapped lambda from the actual score matrix underlying an RPS search, not just that of BLOSUM62
 *
 * Revision 1.92  2004/09/28 16:23:15  papadopo
 * From Michael Gertz:
 * 1. Pass the effective size of the search space into
 *    BLAST_SmallGapSumE, BLAST_LargeGapSumE and BLAST_UnevenGapSumE.
 *    The routines use this value in a simplified formula to compute the
 *    e-value of singleton sets.
 * 2. Caused all routines for calculating the significance of multiple
 *    distinct alignments (BLAST_SmallGapSumE, BLAST_LargeGapSumE and
 *    BLAST_UnevenGapSumE) to use
 *
 *        sum_{i in linked_set} (\lambda_i s_i - \ln K_i)
 *
 *    as the weighted sum score, where (\lambda_i, K_i) are taken from
 *    the appropriate query context.
 *
 * Revision 1.91  2004/08/13 17:44:27  dondosha
 * Doxygenized comments for BLAST_UnevenGapSumE
 *
 * Revision 1.90  2004/08/03 21:06:15  dondosha
 * Removed unused variable
 *
 * Revision 1.89  2004/08/03 20:12:57  dondosha
 * Renamed BlastScoreBlkMatCreate to BlastScoreBlkNuclMatrixCreate and made it public
 *
 * Revision 1.88  2004/07/16 14:00:20  camacho
 * documentation fixes
 *
 * Revision 1.87  2004/07/15 14:50:34  madden
 * Doxygen fix
 *
 * Revision 1.86  2004/07/14 18:04:52  camacho
 * Add const type qualifier to BlastScoreBlk in BlastScoreFreqCalc & Blast_ScoreBlkKbpIdealCalc
 *
 * Revision 1.85  2004/06/21 12:52:05  camacho
 * Replace PSI_ALPHABET_SIZE for BLASTAA_SIZE
 *
 * Revision 1.84  2004/06/18 14:01:32  madden
 * Doxygen fixes, made some input params const, do not set deprecated sbp->maxscore
 *
 * Revision 1.83  2004/06/16 19:34:26  madden
 * Doxygen fixes, made some params const
 *
 * Revision 1.82  2004/06/10 13:21:24  madden
 * Rename RPSFillResidueProbability to Blast_FillResidueProbability, made public.
 * Removed usage of BLAST_SCORE_1MIN/MAX, simply use BLAST_SCORE_MIN/MAX instead
 * Removed useless defines DIMOFP0 and DIMOFP0_MAX
 * Moved over some defines from blast_stat.h
 *
 * Revision 1.81  2004/06/08 15:05:05  madden
 * Doxygen fixes
 *
 * Revision 1.80  2004/06/07 20:03:34  coulouri
 * use floating point constants for comparisons with floating point variables
 *
 * Revision 1.79  2004/06/07 14:44:01  madden
 * Doxygen fixes
 *
 * Revision 1.78  2004/06/07 14:20:41  dondosha
 * Set matrix dimensions to 26 when matrix is read from a file, to make it the same as when matrix is loaded from a library
 *
 * Revision 1.77  2004/05/24 15:09:40  camacho
 * Fixed conflict
 *
 * Revision 1.76  2004/05/24 13:26:27  madden
 * Fix PC compiler warnings
 *
 * Revision 1.75  2004/05/20 16:29:30  madden
 * Make searchsp an Int8 consistent with rest of blast
 *
 * Revision 1.74  2004/05/19 15:34:38  dondosha
 * Moved Blast_ResComp definition from header file
 *
 * Revision 1.73  2004/05/19 14:52:03  camacho
 * 1. Added doxygen tags to enable doxygen processing of algo/blast/core
 * 2. Standardized copyright, CVS $Id string, $Log and rcsid formatting and i
 *    location
 * 3. Added use of @todo doxygen keyword
 *
 * Revision 1.72  2004/05/17 10:37:38  camacho
 * Rename BLAST_ScoreFreq, BLASTMatrixStructure and BLAST_ResComp to avoid conflicts with C toolkit
 *
 * Revision 1.71  2004/05/07 15:23:47  papadopo
 * add initialization of scale factor to ScoreBlkNew
 *
 * Revision 1.70  2004/05/06 15:59:29  camacho
 * Made Blast_KarlinBlkCalc non-static
 *
 * Revision 1.69  2004/05/06 15:05:13  camacho
 * Fix to previous commit
 *
 * Revision 1.68  2004/05/06 14:44:27  camacho
 * Made Blast_ScoreFreqFree non-static
 *
 * Revision 1.67  2004/05/05 21:16:24  camacho
 * Make Blast_GetStdAlphabet and Blast_ScoreFreqNew non-static
 *
 * Revision 1.66  2004/05/04 13:00:02  madden
 * Change BlastKarlinBlkStandardCalcEx to more descriptive Blast_ScoreBlkKbpIdealCalc, make public
 *
 * Revision 1.65  2004/04/30 14:39:44  papadopo
 * 1. Remove unneeded #defines
 * 2. use BLAST_SCORE_RANGE_MAX during RPS PSSM creation instead of
 *    (possibly incompatible) RPS_SCORE_MAX
 * 3. return NULL instead of FALSE on an error
 *
 * Revision 1.64  2004/04/30 12:58:49  camacho
 * Replace RPSKarlinLambdaNR by Blast_KarlinLambdaNR
 *
 * Revision 1.63  2004/04/29 20:32:38  papadopo
 * remove RPS_SCORE_MIN, since it turned out to be a workaround for a bug that has since been fixed
 *
 * Revision 1.62  2004/04/29 19:58:03  camacho
 * Use generic matrix allocator/deallocator from blast_psi_priv.h
 *
 * Revision 1.61  2004/04/28 14:40:23  madden
 * Changes from Mike Gertz:
 * - I created the new routine BLAST_GapDecayDivisor that computes a
 *   divisor used to weight the evalue of a collection of distinct
 *   alignments.
 * - I removed  BLAST_GapDecay and BLAST_GapDecayInverse which had become
 *   redundant.
 * - I modified the BLAST_Cutoffs routine so that it uses the value
 *   returned by BLAST_GapDecayDivisor to weight evalues.
 * - I modified BLAST_SmallGapSumE, BLAST_LargeGapSumE and
 *   BLAST_UnevenGapSumE no longer refer to the gap_prob parameter.
 *   Replaced the gap_decay_rate parameter of each of these routines with
 *   a weight_divisor parameter.  Added documentation.
 *
 * Revision 1.60  2004/04/23 19:06:33  camacho
 * Do NOT use lowercase names for #defines
 *
 * Revision 1.59  2004/04/23 13:49:20  madden
 * Cleaned up ifndef in BlastKarlinLHtoK
 *
 * Revision 1.58  2004/04/23 13:21:25  madden
 * Rewrote BlastKarlinLHtoK to do the following and more:
 * 1. fix a bug whereby the wrong formula was used when high score == 1
 *    and low score == -1;
 * 2. fix a methodological error of truncating the first sum
 *    and trying to make it converge quickly by adding terms
 *    of a geometric progression, even though the geometric progression
 *    estimate is not correct in all cases;
 *    the old adjustment code is left in for historical purposes but
 *    #ifdef'd out
 * 3. Eliminate the Boolean bi_modal_score variable.  The old test that
 *    set the value of bi_modal_score would frequently fail to choose the
 *    correct value due to rounding error.
 * 4. changed numerous local variable names to make them more meaningful;
 * 5. added substantial comments to explain what the procedure
 *    is doing and what each variable represents
 *
 * Revision 1.57  2004/04/19 12:58:18  madden
 * Changed BLAST_KarlinBlk to Blast_KarlinBlk to avoid conflict with blastkar.h structure, renamed some functions to start with Blast_Karlin, made Blast_KarlinBlkFree public
 *
 * Revision 1.56  2004/04/12 18:57:31  madden
 * Rename BLAST_ResFreq to Blast_ResFreq, make Blast_ResFreqNew, Blast_ResFreqFree, and Blast_ResFreqStdComp non-static
 *
 * Revision 1.55  2004/04/08 13:53:10  papadopo
 * fix doxygen warning
 *
 * Revision 1.54  2004/04/07 03:06:16  camacho
 * Added blast_encoding.[hc], refactoring blast_stat.[hc]
 *
v * Revision 1.53  2004/04/05 18:53:35  madden
 * Set dimensions if matrix from memory
 *
 * Revision 1.52  2004/04/01 14:14:02  lavr
 * Spell "occurred", "occurrence", and "occurring"
 *
 * Revision 1.51  2004/03/31 17:50:09  papadopo
 * Mike Gertz' changes for length adjustment calculations
 *
 * Revision 1.50  2004/03/11 18:52:41  camacho
 * Remove THREADS_IMPLEMENTED
 *
 * Revision 1.49  2004/03/10 18:00:06  camacho
 * Remove outdated references to blastkar
 *
 * Revision 1.48  2004/03/05 17:52:33  papadopo
 * Allow 32-bit context numbers for queries
 *
 * Revision 1.47  2004/03/04 21:07:51  papadopo
 * add RPS BLAST functionality
 *
 * Revision 1.46  2004/02/19 21:16:48  dondosha
 * Use enum type for severity argument in Blast_MessageWrite
 *
 * Revision 1.45  2003/12/05 16:03:57  camacho
 * Remove compiler warnings
 *
 * Revision 1.44  2003/11/28 22:39:11  camacho
 * + static keyword to BlastKarlinLtoH
 *
 * Revision 1.43  2003/11/28 15:03:48  camacho
 * Added static keyword to BlastKarlinLtoH
 *
 * Revision 1.42  2003/11/26 19:12:13  madden
 * code to simplify some routines and use NlmKarlinLambdaNR in place of BlastKarlinLambdaBis (following Mike Gertzs changes to blastkar.c )
 *
 * Revision 1.41  2003/11/24 23:18:32  dondosha
 * Added gap_decay_rate argument to BLAST_Cutoffs; removed BLAST_Cutoffs_simple
 *
 * Revision 1.40  2003/11/19 15:17:42  dondosha
 * Removed unused members from Karlin block structure
 *
 * Revision 1.39  2003/10/16 15:55:22  coulouri
 * fix uninitialized variables
 *
 * Revision 1.38  2003/10/16 15:52:08  coulouri
 * fix uninitialized variables
 *
 * Revision 1.37  2003/10/15 16:59:43  coulouri
 * type correctness fixes
 *
 * Revision 1.36  2003/10/02 22:08:34  dondosha
 * Corrections for one-strand translated searches
 *
 * Revision 1.35  2003/09/26 19:01:59  madden
 * Prefix ncbimath functions with BLAST_
 *
 * Revision 1.34  2003/09/09 14:21:39  coulouri
 * change blastkar.h to blast_stat.h
 *
 * Revision 1.33  2003/09/02 21:12:07  camacho
 * Fix small memory leak
 *
 * Revision 1.32  2003/08/26 15:23:51  dondosha
 * Rolled back previous change as it is not necessary any more
 *
 * Revision 1.31  2003/08/25 22:29:07  dondosha
 * Default matrix loading is defined only in C++ toolkit
 *
 * Revision 1.30  2003/08/25 18:05:41  dondosha
 * Moved assert statement after variables declarations
 *
 * Revision 1.29  2003/08/25 16:23:33  camacho
 * +Loading protein scoring matrices from utils/tables
 *
 * Revision 1.28  2003/08/11 15:01:59  dondosha
 * Added algo/blast/core to all #included headers
 *
 * Revision 1.27  2003/08/01 17:27:04  dondosha
 * Renamed external functions to avoid collisions with ncbitool library; made other functions static
 *
 * Revision 1.26  2003/07/31 18:48:49  dondosha
 * Use Int4 instead of BLAST_Score
 *
 * Revision 1.25  2003/07/31 17:48:06  madden
 * Remove call to FileLength
 *
 * Revision 1.24  2003/07/31 14:31:41  camacho
 * Replaced Char for char
 *
 * Revision 1.23  2003/07/31 14:19:28  camacho
 * Replaced FloatHi for double
 *
 * Revision 1.22  2003/07/31 00:32:37  camacho
 * Eliminated Ptr notation
 *
 * Revision 1.21  2003/07/30 22:08:09  dondosha
 * Process of finding path to the matrix is moved out of the blast library
 *
 * Revision 1.20  2003/07/30 21:52:41  camacho
 * Follow conventional structure definition
 *
 * Revision 1.19  2003/07/30 19:39:14  camacho
 * Remove PNTRs
 *
 * Revision 1.18  2003/07/30 17:58:25  dondosha
 * Changed ValNode to ListNode
 *
 * Revision 1.17  2003/07/30 17:15:00  dondosha
 * Minor fixes for very strict compiler warnings
 *
 * Revision 1.16  2003/07/30 17:06:40  camacho
 * Removed old cvs log
 *
 * Revision 1.15  2003/07/30 16:32:02  madden
 * Use ansi functions when possible
 *
 * Revision 1.14  2003/07/30 15:29:37  madden
 * Removed MemSets
 *
 * Revision 1.13  2003/07/29 14:42:31  coulouri
 * use strdup() instead of StringSave()
 *
 * Revision 1.12  2003/07/28 19:04:15  camacho
 * Replaced all MemNews for calloc
 *
 * Revision 1.11  2003/07/28 03:41:49  camacho
 * Use f{open,close,gets} instead of File{Open,Close,Gets}
 *
 * Revision 1.10  2003/07/25 21:12:28  coulouri
 * remove constructions of the form "return sfree();" and "a=sfree(a);"
 *
 * Revision 1.9  2003/07/25 18:58:43  camacho
 * Avoid using StrUpper and StringHasNoText
 *
 * Revision 1.8  2003/07/25 17:25:43  coulouri
 * in progres:
 *  * use malloc/calloc/realloc instead of Malloc/Calloc/Realloc
 *  * add sfree() macro and __sfree() helper function to util.[ch]
 *  * use sfree() instead of MemFree()
 *
 * Revision 1.7  2003/07/24 22:37:33  dondosha
 * Removed some unused function parameters
 *
 * Revision 1.6  2003/07/24 22:01:44  camacho
 * Removed unused variables
 *
 * Revision 1.5  2003/07/24 21:31:06  dondosha
 * Changed to calls to BlastConstructErrorMessage to API from blast_message.h
 *
 * Revision 1.4  2003/07/24 20:38:30  dondosha
 * Removed LIBCALL etc. macros
 *
 * Revision 1.3  2003/07/24 17:37:46  dondosha
 * Removed MakeBlastScore function that is dependent on objalign.h
 *
 * Revision 1.2  2003/07/24 15:50:49  dondosha
 * Commented out mutex operations
 *
 * Revision 1.1  2003/07/24 15:18:09  dondosha
 * Copy of blastkar.h from ncbitools library, stripped of dependency on ncbiobj
 *
 * ===========================================================================
 */
