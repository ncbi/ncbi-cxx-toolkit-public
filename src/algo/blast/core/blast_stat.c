static char const rcsid[] = "$Id$";

/* ===========================================================================
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
* ===========================================================================*/
/*****************************************************************************

File name: blastkar.c

Author: Tom Madden

Contents: Functions to calculate BLAST probabilities etc.

Detailed Contents:

	- allocate and deallocate structures used by BLAST to calculate
	probabilities etc.

	- calculate residue frequencies for query and "average" database.

	- read in matrix.

        - calculate sum-p from a collection of HSP's, for both the case
	of a "small" gap and a "large" gap, when give a total score and the
	number of HSP's.

	- calculate expect values for p-values.

	- calculate pseuod-scores from p-values.

****************************************************************************** 
 * $Revision$
 * $Log$
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
 * */
#include <algo/blast/core/blast_stat.h>
#include <algo/blast/core/blast_util.h>
#include <util/tables/raw_scoremat.h>

/* OSF1 apparently doesn't like this. */
#if defined(HUGE_VAL) && !defined(OS_UNIX_OSF1)
#define BLASTKAR_HUGE_VAL HUGE_VAL
#else
#define BLASTKAR_HUGE_VAL 1.e30
#endif


/* Allocates and Deallocates the two-dimensional matrix. */
static BLASTMatrixStructure* BlastMatrixAllocate (Int2 alphabet_size);

/* performs sump calculation, used by BlastSumPStd */
static double BlastSumPCalc (int r, double s);

#define COMMENT_CHR	'#'
#define TOKSTR	" \t\n\r"
#define BLAST_MAX_ALPHABET 40 /* ncbistdaa is only 26, this should be enough */

/*
	How many of the first bases are not ambiguous
	(it's four, of course).
*/
#define NUMBER_NON_AMBIG_BP 4

/** Translates between ncbi4na and blastna. The first four elements
 *	of this array match ncbi2na.
 */
Uint1 NCBI4NA_TO_BLASTNA[] = {
15,/* Gap, 0 */
0, /* A,   1 */
1, /* C,   2 */
6, /* M,   3 */
2, /* G,   4 */
4, /* R,   5 */
9, /* S,   6 */
13, /* V,   7 */
3, /* T,   8 */
8, /* W,   9 */
 5, /* Y,  10 */
 12, /* H,  11 */
 7, /* K,  12 */
 11, /* D,  13 */
 10, /* B,  14 */
 14  /* N,  15 */
};

static Uint1 blastna_to_ncbi4na[] = {
         	 1, /* A, 0 */
				 2, /* C, 1 */
				 4, /* G, 2 */
				 8, /* T, 3 */
				 5, /* R, 4 */
				10, /* Y, 5 */
				 3, /* M, 6 */
				12, /* K, 7 */
				 9, /* W, 8 */
				 6, /* S, 9 */
				14, /* B, 10 */
				13, /* D, 11 */
				11, /* H, 12 */
				 7, /* V, 13 */
				15, /* N, 14 */
				 0  /* Gap, 15 */
};

static Uint1 iupacna_to_blastna[128]={
15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,
15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,
15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,
15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,
15, 0,10, 1,11,15,15, 2,12,15,15, 7,15, 6,14,15,
15,15, 4, 9, 3,15,13, 8,15, 5,15,15,15,15,15,15,
15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,
15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15};

static Uint1 iupacna_to_ncbi4na[128]={
 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
 0, 1,14, 2,13, 0, 0, 4,11, 0, 0,12, 0, 3,15, 0,
 0, 0, 5, 6, 8, 0, 7, 9, 0,10, 0, 0, 0, 0, 0, 0,
 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

static Uint1 ncbieaa_to_ncbistdaa[128]={
 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,25, 0, 0, 0, 0, 0,
 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 0,10,11,12,13, 0,
14,15,16,17,18,24,19,20,21,22,23, 0, 0, 0, 0, 0,
 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

static Uint1 ncbistdaa_to_ncbieaa[BLASTAA_SIZE]={
'-','A','B','C','D','E','F','G','H','I','K','L','M',
'N','P','Q','R','S','T','V','W','X','Y','Z','U','*'};

/* Used in BlastKarlinBlkGappedCalc */
typedef double array_of_8[8];

/* Used to temporarily store matrix values for retrieval. */

typedef struct MatrixInfo {
	char*		name;			/* name of matrix (e.g., BLOSUM90). */
	array_of_8 	*values;		/* The values (below). */
	Int4		*prefs;			/* Preferences for display. */
	Int4		max_number_values;	/* number of values (e.g., BLOSUM90_VALUES_MAX). */
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


How to add a new matrix to blastkar.c:
--------------------------------------
To add a new matrix to blastkar.c it is necessary to complete 
four steps.  As an example consider adding the matrix
called TESTMATRIX

1.) add a define specifying how many different existence and extensions
penalties are allowed, so it would be necessary to add the line:

#define TESTMATRIX_VALUES_MAX 14

if 14 values were to be allowed.

2.) add a two-dimensional array to contain the statistical parameters:

static double testmatrix_values[TESTMATRIX_VALUES_MAX][8] ={ ...

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


	
	

#define BLOSUM45_VALUES_MAX 14
static double  blosum45_values[BLOSUM45_VALUES_MAX][8] = {
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
};

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
};


#define BLOSUM50_VALUES_MAX 16
static double  blosum50_values[BLOSUM50_VALUES_MAX][8] = {
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
};

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
};

#define BLOSUM62_VALUES_MAX 12
static double  blosum62_values[BLOSUM62_VALUES_MAX][8] = {
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
};

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
};


#define BLOSUM80_VALUES_MAX 10
static double  blosum80_values[BLOSUM80_VALUES_MAX][8] = {
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
};

static Int4 blosum80_prefs[BLOSUM80_VALUES_MAX] = {
    BLAST_MATRIX_NOMINAL,
    BLAST_MATRIX_NOMINAL,
    BLAST_MATRIX_NOMINAL,
    BLAST_MATRIX_NOMINAL,
    BLAST_MATRIX_NOMINAL,
    BLAST_MATRIX_NOMINAL,
    BLAST_MATRIX_NOMINAL,
    BLAST_MATRIX_BEST,
    BLAST_MATRIX_NOMINAL
};

#define BLOSUM90_VALUES_MAX 8
static double  blosum90_values[BLOSUM90_VALUES_MAX][8] = {
    {(double) INT2_MAX, (double) INT2_MAX, (double) INT2_MAX, 0.3346, 0.190, 0.7547, 0.4434, -1.4},
    {9, 2, (double) INT2_MAX, 0.310, 0.12, 0.46, 0.67, -6},
    {8, 2, (double) INT2_MAX, 0.300, 0.099, 0.39, 0.76, -7},
    {7, 2, (double) INT2_MAX, 0.283, 0.072, 0.30, 0.93, -11},
    {6, 2, (double) INT2_MAX, 0.259, 0.048, 0.22, 1.2, -16},
    {11, 1, (double) INT2_MAX, 0.302, 0.093, 0.39, 0.78, -8},
    {10, 1, (double) INT2_MAX, 0.290, 0.075, 0.28, 1.04, -15},
    {9, 1, (double) INT2_MAX, 0.265, 0.044, 0.20, 1.3, -19},
};

static Int4 blosum90_prefs[BLOSUM90_VALUES_MAX] = {
	BLAST_MATRIX_NOMINAL,
	BLAST_MATRIX_NOMINAL,
	BLAST_MATRIX_NOMINAL,
	BLAST_MATRIX_NOMINAL,
	BLAST_MATRIX_NOMINAL,
	BLAST_MATRIX_NOMINAL,
	BLAST_MATRIX_BEST,
	BLAST_MATRIX_NOMINAL
};

#define PAM250_VALUES_MAX 16
static double  pam250_values[PAM250_VALUES_MAX][8] = {
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
};

static Int4 pam250_prefs[PAM250_VALUES_MAX] = {
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
BLAST_MATRIX_NOMINAL
};

#define PAM30_VALUES_MAX 7
static double  pam30_values[PAM30_VALUES_MAX][8] = {
    {(double) INT2_MAX, (double) INT2_MAX, (double) INT2_MAX, 0.3400, 0.283, 1.754, 0.1938, -0.3},
    {7, 2, (double) INT2_MAX, 0.305, 0.15, 0.87, 0.35, -3},
    {6, 2, (double) INT2_MAX, 0.287, 0.11, 0.68, 0.42, -4},
    {5, 2, (double) INT2_MAX, 0.264, 0.079, 0.45, 0.59, -7},
    {10, 1, (double) INT2_MAX, 0.309, 0.15, 0.88, 0.35, -3},
    {9, 1, (double) INT2_MAX, 0.294, 0.11, 0.61, 0.48, -6},
    {8, 1, (double) INT2_MAX, 0.270, 0.072, 0.40, 0.68, -10},
};

static Int4 pam30_prefs[PAM30_VALUES_MAX] = {
BLAST_MATRIX_NOMINAL,
BLAST_MATRIX_NOMINAL,
BLAST_MATRIX_NOMINAL,
BLAST_MATRIX_NOMINAL,
BLAST_MATRIX_NOMINAL,
BLAST_MATRIX_BEST,
BLAST_MATRIX_NOMINAL,
};


#define PAM70_VALUES_MAX 7
static double  pam70_values[PAM70_VALUES_MAX][8] = {
    {(double) INT2_MAX, (double) INT2_MAX, (double) INT2_MAX, 0.3345, 0.229, 1.029, 0.3250,   -0.7},
    {8, 2, (double) INT2_MAX, 0.301, 0.12, 0.54, 0.56, -5},
    {7, 2, (double) INT2_MAX, 0.286, 0.093, 0.43, 0.67, -7},
    {6, 2, (double) INT2_MAX, 0.264, 0.064, 0.29, 0.90, -12},
    {11, 1, (double) INT2_MAX, 0.305, 0.12, 0.52, 0.59, -6},
    {10, 1, (double) INT2_MAX, 0.291, 0.091, 0.41, 0.71, -9},
    {9, 1, (double) INT2_MAX, 0.270, 0.060, 0.28, 0.97, -14},
};

static Int4 pam70_prefs[PAM70_VALUES_MAX] = {
BLAST_MATRIX_NOMINAL,
BLAST_MATRIX_NOMINAL,
BLAST_MATRIX_NOMINAL,
BLAST_MATRIX_NOMINAL,
BLAST_MATRIX_NOMINAL,
BLAST_MATRIX_BEST,
BLAST_MATRIX_NOMINAL
};



#define BLOSUM62_20_VALUES_MAX 65
static double  blosum62_20_values[BLOSUM62_20_VALUES_MAX][8] = {
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
};

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
};

/*
	Allocates memory for the BlastScoreBlk*.
*/

BlastScoreBlk*
BlastScoreBlkNew(Uint1 alphabet, Int2 number_of_contexts)

{
	BlastScoreBlk* sbp;

	sbp = (BlastScoreBlk*) calloc(1, sizeof(BlastScoreBlk));

	if (sbp != NULL)
	{
		sbp->alphabet_code = alphabet;
		if (alphabet != BLASTNA_SEQ_CODE)
			sbp->alphabet_size = BLASTAA_SIZE;
		else
			sbp->alphabet_size = BLASTNA_SIZE;

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

		sbp->matrix_struct = BlastMatrixAllocate(sbp->alphabet_size);
		if (sbp->matrix_struct == NULL)
		{
			sbp = BlastScoreBlkFree(sbp);
			return sbp;
		}
		sbp->matrix = sbp->matrix_struct->matrix;
		sbp->maxscore = (Int4 *) calloc(BLAST_MATRIX_SIZE, sizeof(Int4));
		sbp->number_of_contexts = number_of_contexts;
		sbp->sfp = (BLAST_ScoreFreq**) 
         calloc(sbp->number_of_contexts, sizeof(BLAST_ScoreFreq*));
		sbp->kbp_std = (BLAST_KarlinBlk**)
         calloc(sbp->number_of_contexts, sizeof(BLAST_KarlinBlk*));
		sbp->kbp_gap_std = (BLAST_KarlinBlk**)
         calloc(sbp->number_of_contexts, sizeof(BLAST_KarlinBlk*));
		sbp->kbp_psi = (BLAST_KarlinBlk**)
         calloc(sbp->number_of_contexts, sizeof(BLAST_KarlinBlk*));
		sbp->kbp_gap_psi = (BLAST_KarlinBlk**)
         calloc(sbp->number_of_contexts, sizeof(BLAST_KarlinBlk*));
	}

	return sbp;
}

static BLAST_ScoreFreq*
BlastScoreFreqDestruct(BLAST_ScoreFreq* sfp)
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
static BLAST_KarlinBlk*
BlastKarlinBlkDestruct(BLAST_KarlinBlk* kbp)

{
	sfree(kbp);

	return kbp;
}

static BLASTMatrixStructure*
BlastMatrixDestruct(BLASTMatrixStructure* matrix_struct)

{

	if (matrix_struct == NULL)
		return NULL;

	sfree(matrix_struct);

	return matrix_struct;
}

BlastScoreBlk*
BlastScoreBlkFree(BlastScoreBlk* sbp)

{
    Int4 index, rows;
    if (sbp == NULL)
        return NULL;
    
    for (index=0; index<sbp->number_of_contexts; index++) {
        if (sbp->sfp)
            sbp->sfp[index] = BlastScoreFreqDestruct(sbp->sfp[index]);
        if (sbp->kbp_std)
            sbp->kbp_std[index] = BlastKarlinBlkDestruct(sbp->kbp_std[index]);
        if (sbp->kbp_gap_std)
            sbp->kbp_gap_std[index] = BlastKarlinBlkDestruct(sbp->kbp_gap_std[index]);
        if (sbp->kbp_psi)
            sbp->kbp_psi[index] = BlastKarlinBlkDestruct(sbp->kbp_psi[index]);
        if (sbp->kbp_gap_psi)
            sbp->kbp_gap_psi[index] = BlastKarlinBlkDestruct(sbp->kbp_gap_psi[index]);
    }
    if (sbp->kbp_ideal)
        sbp->kbp_ideal = BlastKarlinBlkDestruct(sbp->kbp_ideal);
    sfree(sbp->sfp);
    sfree(sbp->kbp_std);
    sfree(sbp->kbp_psi);
    sfree(sbp->kbp_gap_std);
    sfree(sbp->kbp_gap_psi);
    sbp->matrix_struct = BlastMatrixDestruct(sbp->matrix_struct);
    sfree(sbp->maxscore);
    sbp->comments = ListNodeFreeData(sbp->comments);
    sfree(sbp->name);
    sfree(sbp->ambiguous_res);
    
    /* Removing posMatrix and posFreqs if any */
    rows = sbp->query_length + 1;
    if(sbp->posMatrix != NULL) {
        for (index=0; index < rows; index++) {
            sfree(sbp->posMatrix[index]);
        }
        sfree(sbp->posMatrix);
    }
    
    if(sbp->posFreqs != NULL) {
        for (index = 0; index < rows; index++) {
            sfree(sbp->posFreqs[index]);
        }
        sfree(sbp->posFreqs);
    }
    
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
         ncbieaa_to_ncbistdaa[toupper(ambiguous_res)];
	}
	else {
      if (sbp->alphabet_code == BLASTNA_SEQ_CODE)
         sbp->ambiguous_res[sbp->ambig_occupy] = 
            iupacna_to_blastna[toupper(ambiguous_res)];
      else if (sbp->alphabet_code == NCBI4NA_SEQ_CODE)
         sbp->ambiguous_res[sbp->ambig_occupy] = 
            iupacna_to_ncbi4na[toupper(ambiguous_res)];
   }
	(sbp->ambig_occupy)++;
	

	return 0;
}

/* 
	Fill in the matrix for blastn using the penaly and rewards

	The query sequence alphabet is blastna, the subject sequence
	is ncbi2na.  The alphabet blastna is defined in blastkar.h
	and the first four elements of blastna are identical to ncbi2na.

	The query is in the first index, the subject is the second.
        if matrix==NULL, it is allocated and returned.
*/

static Int4 **BlastScoreBlkMatCreateEx(Int4 **matrix,Int4 penalty, 
                                       Int4 reward)
{

	Int2	index1, index2, degen;
	Int2 degeneracy[BLASTNA_SIZE+1];
	
        if(!matrix) {
            BLASTMatrixStructure* matrix_struct;
            matrix_struct =BlastMatrixAllocate((Int2) BLASTNA_SIZE);
            matrix = matrix_struct->matrix;
        }
	for (index1 = 0; index1<BLASTNA_SIZE; index1++) /* blastna */
		for (index2 = 0; index2<BLASTNA_SIZE; index2++) /* blastna */
			matrix[index1][index2] = 0;

	/* In blastna the 1st four bases are A, C, G, and T, exactly as it is ncbi2na. */
	/* ncbi4na gives them the value 1, 2, 4, and 8.  */

	/* Set the first four bases to degen. one */
	for (index1=0; index1<NUMBER_NON_AMBIG_BP; index1++)
		degeneracy[index1] = 1;

	for (index1=NUMBER_NON_AMBIG_BP; index1<BLASTNA_SIZE; index1++) /* blastna */
	{
		degen=0;
		for (index2=0; index2<NUMBER_NON_AMBIG_BP; index2++) /* ncbi2na */
		{
			if (blastna_to_ncbi4na[index1] & blastna_to_ncbi4na[index2])
				degen++;
		}
		degeneracy[index1] = degen;
	}


	for (index1=0; index1<BLASTNA_SIZE; index1++) /* blastna */
	{
		for (index2=index1; index2<BLASTNA_SIZE; index2++) /* blastna */
		{
			if (blastna_to_ncbi4na[index1] & blastna_to_ncbi4na[index2])
			{ /* round up for positive scores, down for negatives. */
				matrix[index1][index2] = BLAST_Nint( (double) ((degeneracy[index2]-1)*penalty + reward))/degeneracy[index2];
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
	for (index1=0; index1<BLASTNA_SIZE; index1++) /* blastna */
           matrix[BLASTNA_SIZE-1][index1] = INT4_MIN / 2;
	for (index1=0; index1<BLASTNA_SIZE; index1++) /* blastna */
           matrix[index1][BLASTNA_SIZE-1] = INT4_MIN / 2;

	return matrix;
}
/* 
	Fill in the matrix for blastn using the penaly and rewards on
	the BlastScoreBlk*.

	The query sequence alphabet is blastna, the subject sequence
	is ncbi2na.  The alphabet blastna is defined in blastkar.h
	and the first four elements of blastna are identical to ncbi2na.

	The query is in the first index, the subject is the second.
*/
static Int2 BlastScoreBlkMatCreate(BlastScoreBlk* sbp)
{
   sbp->matrix = BlastScoreBlkMatCreateEx(sbp->matrix,sbp->penalty, sbp->reward);
	sbp->mat_dim1 = BLASTNA_SIZE;
	sbp->mat_dim2 = BLASTNA_SIZE;

	return 0;
}

/* 
	Read in the matrix from the FILE *fp.

	This function ASSUMES that the matrices are in the ncbistdaa
	format.  BLAST should be able to use any alphabet, though it
	is expected that ncbistdaa will be used.
*/

static Int2
BlastScoreBlkMatRead(BlastScoreBlk* sbp, FILE *fp)
{
    char	buf[512+3];
    char	temp[512];
    char*	cp,* lp;
    char		ch;
    Int4 **	matrix;
    Int4 *	m;
    Int4	score;
    Uint4	a1cnt = 0, a2cnt = 0;
    char    a1chars[BLAST_MAX_ALPHABET], a2chars[BLAST_MAX_ALPHABET];
    long	lineno = 0;
    double	xscore;
    register int	index1, index2;
    Int2 status;
#ifdef THREADS_IMPLEMENTED
    static TNlmMutex read_matrix_mutex;
    NlmMutexInit(&read_matrix_mutex);
    NlmMutexLock(read_matrix_mutex);
#endif
    
    matrix = sbp->matrix;	
    
    if (sbp->alphabet_code != BLASTNA_SEQ_CODE) {
        for (index1 = 0; index1 < sbp->alphabet_size; index1++)
            for (index2 = 0; index2 < sbp->alphabet_size; index2++)
                matrix[index1][index2] = BLAST_SCORE_MIN;
    } else {
        /* Fill-in all the defaults ambiguity and normal codes */
        status=BlastScoreBlkMatCreate(sbp); 
        if(status != 0)
	{
#ifdef THREADS_IMPLEMENTED
        	NlmMutexUnlock(read_matrix_mutex); 
#endif
        	return status;
	}
    }
    
    /* Read the residue names for the second alphabet */
    while (fgets(buf, sizeof(buf), fp) != NULL) {
        ++lineno;
        if (strchr(buf, '\n') == NULL) {
#ifdef THREADS_IMPLEMENTED
            NlmMutexUnlock(read_matrix_mutex); 
#endif
            return 2;
        }

        if (buf[0] == COMMENT_CHR) {
            /* save the comment line in a linked list */
            *strchr(buf, '\n') = NULLB;
            ListNodeCopyStr(&sbp->comments, 0, buf+1);
            continue;
        }
        if ((cp = strchr(buf, COMMENT_CHR)) != NULL)
            *cp = NULLB;
        lp = (char*)strtok(buf, TOKSTR);
        if (lp == NULL) /* skip blank lines */
            continue;
        while (lp != NULL) {
           if (sbp->alphabet_code == BLASTAA_SEQ_CODE)
              ch = ncbieaa_to_ncbistdaa[toupper(*lp)];
           else if (sbp->alphabet_code == BLASTNA_SEQ_CODE) {
              ch = iupacna_to_blastna[toupper(*lp)];
           } else {
              ch = *lp;
           }
            a2chars[a2cnt++] = ch;
            lp = (char*)strtok(NULL, TOKSTR);
        }
        
        break;	/* Exit loop after reading one line. */
    }
    
    if (a2cnt <= 1) { 
#ifdef THREADS_IMPLEMENTED
        NlmMutexUnlock(read_matrix_mutex); 
#endif
        return 2;
    }

    if (sbp->alphabet_code != BLASTNA_SEQ_CODE) {
        sbp->mat_dim2 = a2cnt;
    }
    while (fgets(buf, sizeof(buf), fp) != NULL)  {
        ++lineno;
        if ((cp = strchr(buf, '\n')) == NULL) {
#ifdef THREADS_IMPLEMENTED
            NlmMutexUnlock(read_matrix_mutex); 
#endif
            return 2;
        }
        if ((cp = strchr(buf, COMMENT_CHR)) != NULL)
            *cp = NULLB;
        if ((lp = (char*)strtok(buf, TOKSTR)) == NULL)
            continue;
        ch = *lp;
        cp = (char*) lp;
        if ((cp = strtok(NULL, TOKSTR)) == NULL) {
#ifdef THREADS_IMPLEMENTED
            NlmMutexUnlock(read_matrix_mutex); 
#endif
            return 2;
        }
        if (a1cnt >= DIM(a1chars)) {
#ifdef THREADS_IMPLEMENTED
            NlmMutexUnlock(read_matrix_mutex); 
#endif
            return 2;
        }

        if (sbp->alphabet_code == BLASTAA_SEQ_CODE) {
           ch = ncbieaa_to_ncbistdaa[toupper(ch)];
        } else {
            if (sbp->alphabet_code == BLASTNA_SEQ_CODE) {
                ch = iupacna_to_blastna[toupper(ch)];
            }
        }
        a1chars[a1cnt++] = ch;
        m = &matrix[(int)ch][0];
        index2 = 0;
        while (cp != NULL) {
            if (index2 >= (int) a2cnt) {
#ifdef THREADS_IMPLEMENTED
                NlmMutexUnlock(read_matrix_mutex); 
#endif
                return 2;
            }
            strcpy(temp, cp);
            
            if (strcasecmp(temp, "na") == 0)  {
                score = BLAST_SCORE_1MIN;
            } else  {
                if (sscanf(temp, "%lg", &xscore) != 1) {
#ifdef THREADS_IMPLEMENTED
                    NlmMutexUnlock(read_matrix_mutex); 
#endif
                    return 2;
                }
				/*xscore = MAX(xscore, BLAST_SCORE_1MIN);*/
                if (xscore > BLAST_SCORE_1MAX || xscore < BLAST_SCORE_1MIN) {
#ifdef THREADS_IMPLEMENTED
                    NlmMutexUnlock(read_matrix_mutex); 
#endif
                    return 2;
                }
                xscore += (xscore >= 0. ? 0.5 : -0.5);
                score = (Int4)xscore;
            }
            
            m[(int)a2chars[index2++]] = score;
            
            cp = strtok(NULL, TOKSTR);
        }
    }
    
    if (a1cnt <= 1) {
#ifdef THREADS_IMPLEMENTED
        NlmMutexUnlock(read_matrix_mutex); 
#endif
        return 2;
    }
    
    if (sbp->alphabet_code != BLASTNA_SEQ_CODE) {
        sbp->mat_dim1 = a1cnt;
    }
    
#ifdef THREADS_IMPLEMENTED
    NlmMutexUnlock(read_matrix_mutex); 
#endif
    return 0;
}

static Int2
BlastScoreBlkMaxScoreSet(BlastScoreBlk* sbp)
{
	Int4 score, maxscore;
	Int4 ** matrix; 
	Int2 index1, index2;

	sbp->loscore = BLAST_SCORE_1MAX;
        sbp->hiscore = BLAST_SCORE_1MIN;
	matrix = sbp->matrix;
	for (index1=0; index1<sbp->alphabet_size; index1++)
	{
		maxscore=BLAST_SCORE_MIN;
		for (index2=0; index2<sbp->alphabet_size; index2++)
		{
			score = matrix[index1][index2];
			if (score <= BLAST_SCORE_MIN || score >= BLAST_SCORE_MAX)
				continue;
			if (score > maxscore)
			{
				maxscore = score;
			}
			if (sbp->loscore > score)
				sbp->loscore = score;
			if (sbp->hiscore < score)
				sbp->hiscore = score;
		}
		sbp->maxscore[index1] = maxscore;
	}
/* If the lo/hi-scores are BLAST_SCORE_MIN/BLAST_SCORE_MAX, (i.e., for
gaps), then use other scores. */

	if (sbp->loscore < BLAST_SCORE_1MIN)
		sbp->loscore = BLAST_SCORE_1MIN;
	if (sbp->hiscore > BLAST_SCORE_1MAX)
		sbp->hiscore = BLAST_SCORE_1MAX;

	return 0;
}

Int2
BlastScoreBlkMatrixLoad(BlastScoreBlk* sbp)
{
    Int2 status = 0;
    SNCBIPackedScoreMatrix* psm;
    Int4** matrix = sbp->matrix;
    int i, j;   /* loop indices */

    ASSERT(sbp);

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
    } else {
        return -1;
    }

    /* Initialize with BLAST_SCORE_MIN */
    for (i = 0; i < sbp->alphabet_size; i++) {
        for (j = 0; j < sbp->alphabet_size; j++) {
            matrix[i][j] = BLAST_SCORE_MIN;
        }
    }

    for (i = 0; i < sbp->alphabet_size; i++) {
        for (j = 0; j < sbp->alphabet_size; j++) {
            if (i == 24 || i == GAP_CHAR ||
                j == 24 || j == GAP_CHAR) {  /* skip selenocysteine and gap */
                continue;
            }
            matrix[i][j] = NCBISM_GetScore((const SNCBIPackedScoreMatrix*) psm,
                                           i, j);
        }
    }
    return status;
}

Int2
BLAST_ScoreBlkMatFill(BlastScoreBlk* sbp, char* matrix_path)
{
    Int2 status = 0;
    
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

            if ( (status=BlastScoreBlkMatRead(sbp, fp)) != 0) {
               fclose(fp);
               return status;
            }
            fclose(fp);

        } else {
            if ( (status = BlastScoreBlkMatrixLoad(sbp)) !=0) {
                return status;
            }
        }
    } else {
       /* Nucleotide BLAST only! */
       if ( (status=BlastScoreBlkMatCreate(sbp)) != 0)
          return status;
    }
    
    if ( (status=BlastScoreBlkMaxScoreSet(sbp)) != 0)
       return status;

    return status;
}

static BLAST_ResFreq*
BlastResFreqDestruct(BLAST_ResFreq* rfp)
{
	if (rfp == NULL)
		return NULL;

	if (rfp->prob0 != NULL)
		sfree(rfp->prob0);

	sfree(rfp);

	return rfp;
}

/*
	Allocates the BLAST_ResFreq* and then fills in the frequencies
	in the probabilities.
*/ 

static BLAST_ResFreq*
BlastResFreqNew(BlastScoreBlk* sbp)
{
	BLAST_ResFreq*	rfp;

	if (sbp == NULL)
	{
		return NULL;
	}

	rfp = (BLAST_ResFreq*) calloc(1, sizeof(BLAST_ResFreq));
	if (rfp == NULL)
		return NULL;

	rfp->alphabet_code = sbp->alphabet_code;

	rfp->prob0 = (double*) calloc(sbp->alphabet_size, sizeof(double));
	if (rfp->prob0 == NULL) 
	{
		rfp = BlastResFreqDestruct(rfp);
		return rfp;
	}
	rfp->prob = rfp->prob0 - sbp->alphabet_start;

	return rfp;
}

typedef struct BLAST_LetterProb {
		char	ch;
		double	p;
	} BLAST_LetterProb;

/*  M. O. Dayhoff amino acid background frequencies   */
static BLAST_LetterProb	Dayhoff_prob[] = {
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
	};

#define STD_AMINO_ACID_FREQS Robinson_prob

static BLAST_LetterProb	nt_prob[] = {
		{ 'A', 25.00 },
		{ 'C', 25.00 },
		{ 'G', 25.00 },
		{ 'T', 25.00 }
	};

/*
	Normalize the frequencies to "norm".
*/
static Int2
BlastResFreqNormalize(BlastScoreBlk* sbp, BLAST_ResFreq* rfp, double norm)
{
	Int2	alphabet_stop, index;
	double	sum = 0., p;

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

/*
	Fills a buffer with the 'standard' alphabet (given by 
	STD_AMINO_ACID_FREQS[index].ch).

	Return value is the number of residues in alphabet.  
	Negative returns upon error.
*/

static Int2
BlastGetStdAlphabet (Uint1 alphabet_code, Uint1* residues, Uint4 residues_size)

{
	Int2 index;

	if (residues_size < DIM(STD_AMINO_ACID_FREQS))
		return -2;

	for (index=0; index<(int)DIM(STD_AMINO_ACID_FREQS); index++) 
	{
		if (alphabet_code == BLASTAA_SEQ_CODE)
		{
	 		residues[index] = 
            ncbieaa_to_ncbistdaa[toupper(STD_AMINO_ACID_FREQS[index].ch)];
		}
		else
		{
			residues[index] = STD_AMINO_ACID_FREQS[index].ch;
		}
	}

	return index;
}

static Int2
BlastResFreqStdComp(BlastScoreBlk* sbp, BLAST_ResFreq* rfp)
{
	Int2 retval;
   Uint4 index;
	Uint1* residues;

	if (sbp->protein_alphabet == TRUE)
	{
		residues = (Uint1*) calloc(DIM(STD_AMINO_ACID_FREQS), sizeof(Uint1));
		retval = BlastGetStdAlphabet(sbp->alphabet_code, residues, DIM(STD_AMINO_ACID_FREQS));
		if (retval < 1)
			return retval;

		for (index=0; index<DIM(STD_AMINO_ACID_FREQS); index++) 
		{
			rfp->prob[residues[index]] = STD_AMINO_ACID_FREQS[index].p;
		}
		sfree(residues);
	}
	else
	{	/* beginning of blastna and ncbi2na are the same. */
		/* Only blastna used  for nucleotides. */
		for (index=0; index<DIM(nt_prob); index++) 
		{
			rfp->prob[index] = nt_prob[index].p;
		}
	}

	BlastResFreqNormalize(sbp, rfp, 1.0);

	return 0;
}

static BLAST_ResComp*
BlastResCompDestruct(BLAST_ResComp* rcp)
{
	if (rcp == NULL)
		return NULL;

	if (rcp->comp0 != NULL)
		sfree(rcp->comp0);

	sfree(rcp);
	return NULL;
}

/* 
	Allocated the BLAST_ResComp* for a given alphabet.  Only the
	alphabets ncbistdaa and ncbi4na should be used by BLAST.
*/
static BLAST_ResComp*
BlastResCompNew(BlastScoreBlk* sbp)
{
	BLAST_ResComp*	rcp;

	rcp = (BLAST_ResComp*) calloc(1, sizeof(BLAST_ResComp));
	if (rcp == NULL)
		return NULL;

	rcp->alphabet_code = sbp->alphabet_code;

/* comp0 has zero offset, comp starts at 0, only one 
array is allocated.  */
	rcp->comp0 = (Int4*) calloc(BLAST_MATRIX_SIZE, sizeof(Int4));
	if (rcp->comp0 == NULL) 
	{
		rcp = BlastResCompDestruct(rcp);
		return rcp;
	}

	rcp->comp = rcp->comp0 - sbp->alphabet_start;

	return rcp;
}

/*
	Store the composition of a (query) string.  
*/
static Int2
BlastResCompStr(BlastScoreBlk* sbp, BLAST_ResComp* rcp, char* str, Int4 length)
{
	char*	lp,* lpmax;
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

static Int2
BlastResFreqClr(BlastScoreBlk* sbp, BLAST_ResFreq* rfp)
{
	Int2	alphabet_max, index;
 
	alphabet_max = sbp->alphabet_start + sbp->alphabet_size;
	for (index=sbp->alphabet_start; index<alphabet_max; index++)
                rfp->prob[index] = 0.0;
 
        return 0;
}

/*
	Calculate the residue frequencies associated with the provided ResComp
*/
static Int2
BlastResFreqResComp(BlastScoreBlk* sbp, BLAST_ResFreq* rfp, BLAST_ResComp* rcp)
{
	Int2	alphabet_max, index;
	double	sum = 0.;

	if (rfp == NULL || rcp == NULL)
		return 1;

	if (rfp->alphabet_code != rcp->alphabet_code)
		return 1;

	alphabet_max = sbp->alphabet_start + sbp->alphabet_size;
	for (index=sbp->alphabet_start; index<alphabet_max; index++)
		sum += rcp->comp[index];

	if (sum == 0.) {
		BlastResFreqClr(sbp, rfp);
		return 0;
	}

	for (index=sbp->alphabet_start; index<alphabet_max; index++)
		rfp->prob[index] = rcp->comp[index] / sum;

	return 0;
}

static Int2
BlastResFreqString(BlastScoreBlk* sbp, BLAST_ResFreq* rfp, char* string, Int4 length)
{
	BLAST_ResComp* rcp;
	
	rcp = BlastResCompNew(sbp);

	BlastResCompStr(sbp, rcp, string, length);

	BlastResFreqResComp(sbp, rfp, rcp);

	rcp = BlastResCompDestruct(rcp);

	return 0;
}

static Int2
BlastScoreChk(Int4 lo, Int4 hi)
{
	if (lo >= 0 || hi <= 0 ||
			lo < BLAST_SCORE_1MIN || hi > BLAST_SCORE_1MAX)
		return 1;

	if (hi - lo > BLAST_SCORE_RANGE_MAX)
		return 1;

	return 0;
}

static BLAST_ScoreFreq*
BlastScoreFreqNew(Int4 score_min, Int4 score_max)
{
	BLAST_ScoreFreq*	sfp;
	Int4	range;

	if (BlastScoreChk(score_min, score_max) != 0)
		return NULL;

	sfp = (BLAST_ScoreFreq*) calloc(1, sizeof(BLAST_ScoreFreq));
	if (sfp == NULL)
		return NULL;

	range = score_max - score_min + 1;
	sfp->sprob = (double*) calloc(range, sizeof(double));
	if (sfp->sprob == NULL) 
	{
		BlastScoreFreqDestruct(sfp);
		return NULL;
	}

	sfp->sprob0 = sfp->sprob;
	sfp->sprob -= score_min;
	sfp->score_min = score_min;
	sfp->score_max = score_max;
	sfp->obs_min = sfp->obs_max = 0;
	sfp->score_avg = 0.0;
	return sfp;
}

static Int2
BlastScoreFreqCalc(BlastScoreBlk* sbp, BLAST_ScoreFreq* sfp, BLAST_ResFreq* rfp1, BLAST_ResFreq* rfp2)
{
	Int4 **	matrix;
	Int4	score, obs_min, obs_max;
	double		score_sum, score_avg;
	Int2 		alphabet_start, alphabet_end, index1, index2;

	if (sbp == NULL || sfp == NULL)
		return 1;

	if (sbp->loscore < sfp->score_min || sbp->hiscore > sfp->score_max)
		return 1;

	for (score = sfp->score_min; score <= sfp->score_max; score++)
		sfp->sprob[score] = 0.0;

	matrix = sbp->matrix;

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

#define DIMOFP0	(iter*range + 1)
#define DIMOFP0_MAX (BLAST_KARLIN_K_ITER_MAX*BLAST_SCORE_RANGE_MAX+1)

static double
BlastKarlinLHtoK(BLAST_ScoreFreq* sfp, double	lambda, double H)
{
#ifndef BLAST_KARLIN_STACKP
	double* P0 = NULL;
#else
	double	P0 [DIMOFP0_MAX];
#endif
	Int4	low;	/* Lowest score (must be negative) */
	Int4	high;	/* Highest score (must be positive) */
	double	K;			/* local copy of K */
	double	ratio;
	int		i, j;
	Int4	range, lo, hi, first, last, d;
	register double	sum;
	double	Sum, av, oldsum, oldsum2, score_avg;
	int		iter;
	double	sumlimit;
	double* p,* ptrP,* ptr1,* ptr2,* ptr1e;
	double	etolami, etolam;
        Boolean         bi_modal_score = FALSE;

	if (lambda <= 0. || H <= 0.) {
		return -1.;
	}

	if (sfp->score_avg >= 0.0) {
		return -1.;
	}

	low = sfp->obs_min;
	high = sfp->obs_max;
	range = high - low;
	p = &sfp->sprob[low];

        /* Look for the greatest common divisor ("delta" in Appendix of PNAS 87 of
           Karlin&Altschul (1990) */
    	for (i = 1, d = -low; i <= range && d > 1; ++i)
           if (p[i])
              d = BLAST_Gcd(d, i);
        
        high /= d;
        low /= d;
        lambda *= d;

	range = high - low;

	av = H/lambda;
	etolam = exp((double)lambda);

	if (low == -1 || high == 1) {
           if (high == 1)
              K = av;
           else {
              score_avg = sfp->score_avg / d;
              K = (score_avg * score_avg) / av;
           }
           return K * (1.0 - 1./etolam);
	}

	sumlimit = BLAST_KARLIN_K_SUMLIMIT_DEFAULT;

	iter = BLAST_KARLIN_K_ITER_MAX;

	if (DIMOFP0 > DIMOFP0_MAX) {
		return -1.;
	}
#ifndef BLAST_KARLIN_STACKP
	P0 = (double*)calloc(DIMOFP0 , sizeof(*P0));
	if (P0 == NULL)
		return -1.;
#else
	memset((char*)P0, 0, DIMOFP0*sizeof(P0[0]));
#endif

	Sum = 0.;
	lo = hi = 0;
	P0[0] = sum = oldsum = oldsum2 = 1.;

        if (p[0] + p[range*d] == 1.) {
           /* There are only two scores (e.g. DNA comparison matrix */
           bi_modal_score = TRUE;
           sumlimit *= 0.01;
        }

        for (j = 0; j < iter && sum > sumlimit; Sum += sum /= ++j) {
           first = last = range;
           lo += low;
           hi += high;
           for (ptrP = P0+(hi-lo); ptrP >= P0; *ptrP-- =sum) {
              ptr1 = ptrP - first;
              ptr1e = ptrP - last;
              ptr2 = p + first;
              for (sum = 0.; ptr1 >= ptr1e; )
                 sum += *ptr1--  *  *ptr2++;
              if (first)
                 --first;
              if (ptrP - P0 <= range)
                 --last;
           }
           etolami = BLAST_Powi((double)etolam, lo - 1);
           for (sum = 0., i = lo; i != 0; ++i) {
              etolami *= etolam;
              sum += *++ptrP * etolami;
           }
           for (; i <= hi; ++i)
              sum += *++ptrP;
           oldsum2 = oldsum;
           oldsum = sum;
        }
        
        if (!bi_modal_score) {
           /* Terms of geometric progression added for correction */
           ratio = oldsum / oldsum2;
           if (ratio >= (1.0 - sumlimit*0.001)) {
              K = -1.;
              goto CleanUp;
           }
           sumlimit *= 0.01;
           while (sum > sumlimit) {
              oldsum *= ratio;
              Sum += sum = oldsum / ++j;
           }
        }

	if (etolam > 0.05) 
	{
           etolami = 1 / etolam;
           K = exp((double)-2.0*Sum) / (av*(1.0 - etolami));
	}
	else
           K = -exp((double)-2.0*Sum) / (av*BLAST_Expm1(-(double)lambda));

CleanUp:
#ifndef BLAST_KARLIN_K_STACKP
	if (P0 != NULL)
		sfree(P0);
#endif
	return K;
}

/******************* Fast Lambda Calculation Subroutine ************************
	Version 1.0	May 16, 1991
	Program by:	Stephen Altschul

	Uses Newton-Raphson method (fast) to solve for Lambda, given an initial
	guess (lambda0) obtained perhaps by the bisection method.
*******************************************************************************/

/*
	BlastKarlinLambdaBis

	Calculate Lambda using the bisection method (slow).
*/
static double
BlastKarlinLambdaBis(BLAST_ScoreFreq* sfp)
{
	register double* sprob;
	double	lambda, up, newval;
	Int4	i, low, high, d;
	int		j;
	register double	sum, x0, x1;

	if (sfp->score_avg >= 0.) {
		return -1.;
	}
	low = sfp->obs_min;
	high = sfp->obs_max;
	if (BlastScoreChk(low, high) != 0)
		return -1.;

	sprob = sfp->sprob;

        /* Find greatest common divisor of all scores */
    	for (i = 1, d = -low; i <= high-low && d > 1; ++i) {
           if (sprob[i+low] != 0)
              d = BLAST_Gcd(d, i);
        }

        high = high / d;
        low = low / d;

	up = BLAST_KARLIN_LAMBDA0_DEFAULT;
	for (lambda=0.; ; ) {
		up *= 2;
		x0 = exp((double)up);
		x1 = BLAST_Powi((double)x0, low - 1);
		if (x1 > 0.) {
			for (sum=0., i=low; i<=high; ++i)
				sum += sprob[i*d] * (x1 *= x0);
		}
		else {
			for (sum=0., i=low; i<=high; ++i)
				sum += sprob[i*d] * exp(up * i);
		}
		if (sum >= 1.0)
			break;
		lambda = up;
	}

	for (j=0; j<BLAST_KARLIN_LAMBDA_ITER_DEFAULT; ++j) {
		newval = (lambda + up) / 2.;
		x0 = exp((double)newval);
		x1 = BLAST_Powi((double)x0, low - 1);
		if (x1 > 0.) {
			for (sum=0., i=low; i<=high; ++i)
				sum += sprob[i*d] * (x1 *= x0);
		}
		else {
			for (sum=0., i=low; i<=high; ++i)
				sum += sprob[i*d] * exp(newval * i);
		}
		if (sum > 1.0)
			up = newval;
		else
			lambda = newval;
	}
	return (lambda + up) / (2. * d);
}

static double
BlastKarlinLambdaNR(BLAST_ScoreFreq* sfp)
{
	Int4	low;			/* Lowest score (must be negative)  */
	Int4	high;			/* Highest score (must be positive) */
	int		j;
	Int4	i, d;
	double*	sprob;
	double	lambda0, sum, slope, temp, x0, x1, amt;

	low = sfp->obs_min;
	high = sfp->obs_max;
	if (sfp->score_avg >= 0.) {	/* Expected score must be negative */
		return -1.0;
	}
	if (BlastScoreChk(low, high) != 0)
		return -1.;

	lambda0 = BLAST_KARLIN_LAMBDA0_DEFAULT;

	sprob = sfp->sprob;
        /* Find greatest common divisor of all scores */
    	for (i = 1, d = -low; i <= high-low && d > 1; ++i) {
           if (sprob[i+low] != 0)
              d = BLAST_Gcd(d, i);
        }

        high = high / d;
        low = low / d;
	/* Calculate lambda */

	for (j=0; j<20; ++j) { /* limit of 20 should never be close-approached */
		sum = -1.0;
		slope = 0.0;
		if (lambda0 < 0.01)
			break;
		x0 = exp((double)lambda0);
		x1 = BLAST_Powi((double)x0, low - 1);
		if (x1 == 0.)
			break;
		for (i=low; i<=high; i++) {
			sum += (temp = sprob[i*d] * (x1 *= x0));
			slope += temp * i;
		}
		lambda0 -= (amt = sum/slope);
		if (ABS(amt/lambda0) < BLAST_KARLIN_LAMBDA_ACCURACY_DEFAULT) {
			/*
			Does it appear that we may be on the verge of converging
			to the ever-present, zero-valued solution?
			*/
			if (lambda0 > BLAST_KARLIN_LAMBDA_ACCURACY_DEFAULT)
				return lambda0 / d;
			break;
		}
	}
	return BlastKarlinLambdaBis(sfp);
}

/*
	BlastKarlinLtoH

	Calculate H, the relative entropy of the p's and q's
*/
static double
BlastKarlinLtoH(BLAST_ScoreFreq* sfp, double	lambda)
{
	Int4	score;
	double	av, etolam, etolami;

	if (lambda < 0.) {
		return -1.;
	}
	if (BlastScoreChk(sfp->obs_min, sfp->obs_max) != 0)
		return -1.;

	etolam = exp((double)lambda);
	etolami = BLAST_Powi((double)etolam, sfp->obs_min - 1);
	if (etolami > 0.) 
	{
	    av = 0.0;
	    for (score=sfp->obs_min; score<=sfp->obs_max; score++)
   			av += sfp->sprob[score] * score * (etolami *= etolam);
	}
	else 
	{
	    av = 0.0;
	    for (score=sfp->obs_min; score<=sfp->obs_max; score++)
   			av += sfp->sprob[score] * score * exp(lambda * score);
	}

    	return lambda * av;
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
    to an array of probabilities for the occurence of all scores between
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
static Int2
BlastKarlinBlkCalc(BLAST_KarlinBlk* kbp, BLAST_ScoreFreq* sfp)
{
	

	if (kbp == NULL || sfp == NULL)
		return 1;

	/* Calculate the parameter Lambda */

	kbp->Lambda_real = kbp->Lambda = BlastKarlinLambdaNR(sfp);
	if (kbp->Lambda < 0.)
		goto ErrExit;


	/* Calculate H */

	kbp->H_real = kbp->H = BlastKarlinLtoH(sfp, kbp->Lambda);
	if (kbp->H < 0.)
		goto ErrExit;


	/* Calculate K and log(K) */

	kbp->K_real = kbp->K = BlastKarlinLHtoK(sfp, kbp->Lambda, kbp->H);
	if (kbp->K < 0.)
		goto ErrExit;
	kbp->logK_real = kbp->logK = log(kbp->K);

	/* Normal return */
	return 0;

ErrExit:
	kbp->Lambda = kbp->H = kbp->K
		= kbp->Lambda_real = kbp->H_real = kbp->K_real = -1.;
#ifdef BLASTKAR_HUGE_VAL
	kbp->logK_real = kbp->logK = BLASTKAR_HUGE_VAL;
#else
	kbp->logK_real = kbp->logK = 1.e30;
#endif
	return 1;
}

/*
  Calculate the Karlin parameters.  This function should be called once
  for each context, or frame translated.
  
  The rfp and stdrfp are calculated for each context, this should be
  fixed. 
*/

Int2
BLAST_ScoreBlkFill(BlastScoreBlk* sbp, char* query, Int4 query_length, Int2 context_number)
{
	BLAST_ResFreq* rfp,* stdrfp;
	Int2 retval=0;

	rfp = BlastResFreqNew(sbp);
	stdrfp = BlastResFreqNew(sbp);
	BlastResFreqStdComp(sbp, stdrfp);
	BlastResFreqString(sbp, rfp, query, query_length);
	sbp->sfp[context_number] = BlastScoreFreqNew(sbp->loscore, sbp->hiscore);
	BlastScoreFreqCalc(sbp, sbp->sfp[context_number], rfp, stdrfp);
	sbp->kbp_std[context_number] = BLAST_KarlinBlkCreate();
	retval = BlastKarlinBlkCalc(sbp->kbp_std[context_number], sbp->sfp[context_number]);
	if (retval)
	{
		rfp = BlastResFreqDestruct(rfp);
		stdrfp = BlastResFreqDestruct(stdrfp);
		return retval;
	}
	sbp->kbp_psi[context_number] = BLAST_KarlinBlkCreate();
	retval = BlastKarlinBlkCalc(sbp->kbp_psi[context_number], sbp->sfp[context_number]);
	rfp = BlastResFreqDestruct(rfp);
	stdrfp = BlastResFreqDestruct(stdrfp);

	return retval;
}

/*
	Calculates the standard Karlin parameters.  This is used
	if the query is translated and the calculated (real) Karlin
	parameters are bad, as they're calculated for non-coding regions.
*/

static BLAST_KarlinBlk*
BlastKarlinBlkStandardCalcEx(BlastScoreBlk* sbp)

{
	BLAST_KarlinBlk* kbp_ideal;
	BLAST_ResFreq* stdrfp;
	BLAST_ScoreFreq* sfp;

	stdrfp = BlastResFreqNew(sbp);
	BlastResFreqStdComp(sbp, stdrfp);
	sfp = BlastScoreFreqNew(sbp->loscore, sbp->hiscore);
	BlastScoreFreqCalc(sbp, sfp, stdrfp, stdrfp);
	kbp_ideal = BLAST_KarlinBlkCreate();
	BlastKarlinBlkCalc(kbp_ideal, sfp);
	stdrfp = BlastResFreqDestruct(stdrfp);

	sfp = BlastScoreFreqDestruct(sfp);

	return kbp_ideal;
}

Int2
BLAST_KarlinBlkStandardCalc(BlastScoreBlk* sbp, Int2 context_start, Int2 context_end)
{

	BLAST_KarlinBlk* kbp_ideal,* kbp;
	Int2 index;

	kbp_ideal = BlastKarlinBlkStandardCalcEx(sbp);
/* Replace the calculated values with ideal ones for blastx, tblastx. */
	for (index=context_start; index<=context_end; index++)
	{
		kbp = sbp->kbp[index];	
		if (kbp->Lambda >= kbp_ideal->Lambda)
		{
			kbp->Lambda = kbp_ideal->Lambda;
			kbp->K = kbp_ideal->K;
			kbp->logK = kbp_ideal->logK;
			kbp->H = kbp_ideal->H;
		}
	}
	kbp_ideal = BlastKarlinBlkDestruct(kbp_ideal);

	return 0;
}

/*
	Creates the Karlin Block.
*/

BLAST_KarlinBlk*
BLAST_KarlinBlkCreate(void)

{
	BLAST_KarlinBlk* kbp;

	kbp = (BLAST_KarlinBlk*) calloc(1, sizeof(BLAST_KarlinBlk));

	return kbp;
}

static BLASTMatrixStructure*
BlastMatrixAllocate(Int2 alphabet_size)

{
	BLASTMatrixStructure* matrix_struct;
	Int2 index;

	if (alphabet_size <= 0 || alphabet_size >= BLAST_MATRIX_SIZE)
		return NULL;

	matrix_struct =	(BLASTMatrixStructure*) calloc(1, sizeof(BLASTMatrixStructure));

	if (matrix_struct == NULL)
		return NULL;

	for (index=0; index<BLAST_MATRIX_SIZE-1; index++)
	{
		matrix_struct->matrix[index] = matrix_struct->long_matrix + index*BLAST_MATRIX_SIZE;
	}

	return matrix_struct;
}

static void BlastResFreqFree(BLAST_ResFreq* rfp)
{
    sfree(rfp->prob0);
    sfree(rfp);

    return;
}

/*
	Deallocates MatrixInfo*
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

/*
	Makes New MatrixInfo*
*/

static MatrixInfo*
MatrixInfoNew(char* name, array_of_8 *values, Int4* prefs, Int4 max_number)

{
	MatrixInfo* matrix_info;

	matrix_info = (MatrixInfo*) calloc(1, sizeof(MatrixInfo));
	matrix_info->name = strdup(name);
	matrix_info->values = values;
	matrix_info->prefs = prefs;
	matrix_info->max_number_values = max_number;

	return matrix_info;
}

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

/*
	ListNode* BlastLoadMatrixValues (void)
	
	Loads all the matrix values, returns a ListNode* chain that contains
	MatrixInfo*'s.

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

/*
Int2
BlastKarlinGetMatrixValuesEx2(char* matrix, Int4* open, Int4* extension, Int4* decline_align, double* lambda, double* K, double* H)
	
Obtains arrays of the allowed opening and extension penalties for gapped BLAST for
the given matrix.  Also obtains arrays of Lambda, K, and H.  Any of these fields that
are not required should be set to NULL.  The Int2 return value is the length of the
arrays.
*/

static Int2
BlastKarlinGetMatrixValuesEx2(char* matrix, Int4** open, Int4** extension, Int4** decline_align, double** lambda, double** K, double** H, double** alpha, double** beta, Int4** pref_flags)

{
	array_of_8 *values;
	Boolean found_matrix=FALSE;
	Int4 index, max_number_values=0;
	Int4* open_array=NULL,* extension_array=NULL,* decline_align_array=NULL,* pref_flags_array=NULL,* prefs;
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
				open_array[index] = values[index][0];
			if (extension)
				extension_array[index] = values[index][1];
			if (decline_align)
				decline_align_array[index] = values[index][2];
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

/*
Int2
BlastKarlinGetMatrixValues(char* matrix, Int4* open, Int4* extension, double* lambda, double* K, double* H)
	
Obtains arrays of the allowed opening and extension penalties for gapped BLAST for
the given matrix.  Also obtains arrays of Lambda, K, and H.  Any of these fields that
are not required should be set to NULL.  The Int2 return value is the length of the
arrays.
*/

static Int2
BlastKarlinGetMatrixValues(char* matrix, Int4** open, Int4** extension, double** lambda, double** K, double** H, Int4** pref_flags)

{
	return BlastKarlinGetMatrixValuesEx2(matrix, open, extension, NULL, lambda, K, H, NULL, NULL, pref_flags);

}

/*Extract the alpha and beta settings for this matrixName, and these
  gap open and gap extension costs*/
void BLAST_GetAlphaBeta(char* matrixName, double *alpha,
double *beta, Boolean gapped, Int4 gap_open, Int4 gap_extend)
{
   Int4* gapOpen_arr,* gapExtend_arr,* pref_flags;
   double* alpha_arr,* beta_arr;
   Int2 num_values;
   Int4 i; /*loop index*/

   num_values = BlastKarlinGetMatrixValuesEx2(matrixName, &gapOpen_arr, 
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
   }

   sfree(gapOpen_arr);
   sfree(gapExtend_arr);
   sfree(pref_flags);
   sfree(alpha_arr);
   sfree(beta_arr);
}

static Int2
BlastKarlinReportAllowedValues(const char *matrix_name, 
   Blast_Message** error_return)
{
	array_of_8 *values;
	Boolean found_matrix=FALSE;
	char buffer[256];
	Int4 index, max_number_values=0;
	MatrixInfo* matrix_info;
	ListNode* vnp,* head;

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
				sprintf(buffer, "Gap existence and extension values of %ld and %ld are supported", (long) BLAST_Nint(values[index][0]), (long) BLAST_Nint(values[index][1]));
			else
				sprintf(buffer, "Gap existence, extension and decline-to-align values of %ld, %ld and %ld are supported", (long) BLAST_Nint(values[index][0]), (long) BLAST_Nint(values[index][1]), (long) BLAST_Nint(values[index][2]));
			Blast_MessageWrite(error_return, 2, 0, 0, buffer);
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
BLAST_KarlinBlkGappedCalc(BLAST_KarlinBlk* kbp, Int4 gap_open, Int4 gap_extend, Int4 decline_align, char* matrix_name, Blast_Message** error_return)

{
	char buffer[256];
	Int2 status = BLAST_KarlinkGapBlkFill(kbp, gap_open, gap_extend, decline_align, matrix_name);

	if (status && error_return)
	{
		if (status == 1)
		{
			MatrixInfo* matrix_info;
			ListNode* vnp,* head; 		

			vnp = head = BlastLoadMatrixValues();

			sprintf(buffer, "%s is not a supported matrix", matrix_name);
			Blast_MessageWrite(error_return, 2, 0, 0, buffer);

			while (vnp)
			{
				matrix_info = vnp->ptr;
				sprintf(buffer, "%s is a supported matrix", matrix_info->name);
            Blast_MessageWrite(error_return, 2, 0, 0, buffer);
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
			Blast_MessageWrite(error_return, 2, 0, 0, buffer);
			BlastKarlinReportAllowedValues(matrix_name, error_return);
		}
	}

	return status;
}

/*
	Attempts to fill KarlinBlk for given gap opening, extensions etc.  
	Will return non-zero status if that fails.

	return values:	-1 if matrix_name is NULL;
			1 if matrix not found
			2 if matrix found, but open, extend etc. values not supported.
*/
Int2
BLAST_KarlinkGapBlkFill(BLAST_KarlinBlk* kbp, Int4 gap_open, Int4 gap_extend, Int4 decline_align, char* matrix_name)
{
	Boolean found_matrix=FALSE, found_values=FALSE;
	array_of_8 *values;
	Int2 status=0;
	Int4 index, max_number_values=0;
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
		for (index=0; index<max_number_values; index++)
		{
			if (BLAST_Nint(values[index][0]) == gap_open &&
				BLAST_Nint(values[index][1]) == gap_extend &&
				(BLAST_Nint(values[index][2]) == INT2_MAX || BLAST_Nint(values[index][2]) == decline_align))
			{
				if (kbp)
				{
					kbp->Lambda_real = kbp->Lambda = values[index][3];
					kbp->K_real = kbp->K = values[index][4];
					kbp->logK_real = kbp->logK = log(kbp->K);
					kbp->H_real = kbp->H = values[index][5];
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
	array_of_8 *values;
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
	
static double
BlastGapDecayInverse(double pvalue, unsigned nsegs, double decayrate)
{
	if (decayrate <= 0. || decayrate >= 1. || nsegs == 0)
		return pvalue;

	return pvalue * (1. - decayrate) * BLAST_Powi(decayrate, nsegs - 1);
}

static double
BlastGapDecay(double pvalue, unsigned nsegs, double decayrate)
{
	if (decayrate <= 0. || decayrate >= 1. || nsegs == 0)
		return pvalue;

	return pvalue / ((1. - decayrate) * BLAST_Powi(decayrate, nsegs - 1));
}

/*
	BlastCutoffs

	Calculate the cutoff score, S, and the highest expected score.

	WRG (later modified by TLM).
*/
Int2
BLAST_Cutoffs(Int4 *S, /* cutoff score */
	double* E, /* expected no. of HSPs scoring at or above S */
	BLAST_KarlinBlk* kbp,
	double qlen, /* length of query sequence */
	double dblen, /* length of database or database sequence */
	Boolean dodecay) /* TRUE ==> use gapdecay feature */
{
	return BLAST_Cutoffs_simple(S, E, kbp, qlen*dblen, dodecay);
}

/* Smallest float that might not cause a floating point exception in
	S = (Int4) (ceil( log((double)(K * searchsp / E)) / Lambda ));
below.
*/
#define BLASTKAR_SMALL_FLOAT 1.0e-297
static Int4
BlastKarlinEtoS_simple(double	E,	/* Expect value */
	BLAST_KarlinBlk*	kbp,
	double	searchsp)	/* size of search space */
{

	double	Lambda, K, H; /* parameters for Karlin statistics */
	Int4	S;

	Lambda = kbp->Lambda;
	K = kbp->K;
	H = kbp->H;
	if (Lambda < 0. || K < 0. || H < 0.) 
	{
		return BLAST_SCORE_MIN;
	}

	E = MAX(E, BLASTKAR_SMALL_FLOAT);

	S = (Int4) (ceil( log((double)(K * searchsp / E)) / Lambda ));
	return S;
}

Int2
BLAST_Cutoffs_simple(Int4 *S, /* cutoff score */
	double* E, /* expected no. of HSPs scoring at or above S */
	BLAST_KarlinBlk* kbp,
	double searchsp, /* size of search space. */
	Boolean dodecay) /* TRUE ==> use gapdecay feature */
{
	Int4	s = *S, es;
	double	e = *E, esave;
	Boolean		s_changed = FALSE;

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
		if (dodecay)
			e = BlastGapDecayInverse(e, 1, 0.5);
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
		if (dodecay)
			e = BlastGapDecay(e, 1, 0.5);
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
		BLAST_KarlinBlk* kbp,
		double	searchsp)	/* size of search space. */
{
	double	Lambda, K, H; /* parameters for Karlin statistics */

	Lambda = kbp->Lambda;
	K = kbp->K;
	H = kbp->H;
	if (Lambda < 0. || K < 0. || H < 0.) {
		return -1.;
	}

	return searchsp * exp((double)(-Lambda * S) + kbp->logK);
}

/*
BlastKarlinPtoE -- convert a P-value to an Expect value
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

static double	tab2[] = { /* table for r == 2 */
0.01669,  0.0249,   0.03683,  0.05390,  0.07794,  0.1111,   0.1559,   0.2146,   
0.2890,   0.3794,   0.4836,   0.5965,   0.7092,   0.8114,   0.8931,   0.9490,   
0.9806,   0.9944,   0.9989
		};

static double	tab3[] = { /* table for r == 3 */
0.9806,   0.9944,   0.9989,   0.0001682,0.0002542,0.0003829,0.0005745,0.0008587,
0.001278, 0.001893, 0.002789, 0.004088, 0.005958, 0.008627, 0.01240,  0.01770,  
0.02505,  0.03514,  0.04880,  0.06704,  0.09103,  0.1220,   0.1612,   0.2097,   
0.2682,   0.3368,   0.4145,   0.4994,   0.5881,   0.6765,   0.7596,   0.8326,   
0.8922,   0.9367,   0.9667,   0.9846,   0.9939,   0.9980
		};

static double	tab4[] = { /* table for r == 4 */
2.658e-07,4.064e-07,6.203e-07,9.450e-07,1.437e-06,2.181e-06,3.302e-06,4.990e-06,
7.524e-06,1.132e-05,1.698e-05,2.541e-05,3.791e-05,5.641e-05,8.368e-05,0.0001237,
0.0001823,0.0002677,0.0003915,0.0005704,0.0008275,0.001195, 0.001718, 0.002457,
0.003494, 0.004942, 0.006948, 0.009702, 0.01346,  0.01853,  0.02532,  0.03431,
0.04607,  0.06128,  0.08068,  0.1051,   0.1352,   0.1719,   0.2157,   0.2669,
0.3254,   0.3906,   0.4612,   0.5355,   0.6110,   0.6849,   0.7544,   0.8168,
0.8699,   0.9127,   0.9451,   0.9679,   0.9827,   0.9915,   0.9963
		};

static double* table[] = { tab2, tab3, tab4 };
static short tabsize[] = { DIM(tab2)-1, DIM(tab3)-1, DIM(tab4)-1 };

static double f (double,void*);
static double g (double,void*);


/*
    Estimate the Sum P-value by calculation or interpolation, as appropriate.
	Approx. 2-1/2 digits accuracy minimum throughout the range of r, s.
	r = number of segments
	s = total score (in nats), adjusted by -r*log(KN)
*/
static double
BlastSumP(Int4 r, double s)
{
	Int4		i, r1, r2;
	double	a;

	if (r == 1)
		return -BLAST_Expm1(-exp(-s));

	if (r <= 4) {
		if (r < 1)
			return 0.;
		r1 = r - 1;
		if (s >= r*r+r1) {
			a = BLAST_LnGammaInt(r+1);
			return r * exp(r1*log(s)-s-a-a);
		}
		if (s > -2*r) {
			/* interpolate */
			i = (Int4) (a = s+s+(4*r));
			a -= i;
			i = tabsize[r2 = r - 2] - i;
			return a*table[r2][i-1] + (1.-a)*table[r2][i];
		}
		return 1.;
	}

	return BlastSumPCalc(r, s);
}

/*
    BlastSumPCalc

    Evaluate the following double integral, where r = number of segments
    and s = the adjusted score in nats:

                    (r-2)         oo           oo
     Prob(r,s) =   r              -            -   (r-2)
                 -------------   |   exp(-y)  |   x   exp(-exp(x - y/r)) dx dy
                 (r-1)! (r-2)!  U            U
                                s            0
*/
static double
BlastSumPCalc(int r, double s)
{
	int		r1, itmin;
	double	t, d, epsilon;
	double	est_mean, mean, stddev, stddev4;
	double	xr, xr1, xr2, logr;
	double	args[6];

	epsilon = BLAST_SUMP_EPSILON_DEFAULT; /* accuracy for SumP calcs. */

	if (r == 1) {
		if (s > 8.)
			return exp(-s);
		return -BLAST_Expm1(-exp(-s));
	}
	if (r < 1)
		return 0.;

	xr = r;

	if (r < 8) {
		if (s <= -2.3*xr)
			return 1.;
	}
	else if (r < 15) {
			if (s <= -2.5*xr)
				return 1.;
	}
	else if (r < 27) {
			if (s <= -3.0*xr)
				return 1.;
	}
	else if (r < 51) {
			if (s <= -3.4*xr)
				return 1.;
	}
	else if (r < 101) {
			if (s <= -4.0*xr)
				return 1.;
	}

	/* stddev in the limit of infinite r, but quite good for even small r */
	stddev = sqrt(xr);
	stddev4 = 4.*stddev;
	xr1 = r1 = r - 1;

	if (r > 100) {
		/* Calculate lower bound on the mean using inequality log(r) <= r */
		est_mean = -xr * xr1;
		if (s <= est_mean - stddev4)
			return 1.;
	}

	/* mean is rather close to the mode, and the mean is readily calculated */
	/* mean in the limit of infinite r, but quite good for even small r */
	logr = log(xr);
	mean = xr * (1. - logr) - 0.5;
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

#define ARG_R args[0]
#define ARG_R2 args[1]
#define ARG_ADJ1 args[2]
#define ARG_ADJ2 args[3]
#define ARG_SDIVR args[4]
#define ARG_EPS args[5]

	ARG_R = xr;
	ARG_R2 = xr2 = r - 2;
	ARG_ADJ1 = xr2*logr - BLAST_LnGammaInt(r1) - BLAST_LnGammaInt(r);
	ARG_EPS = epsilon;

	do {
		d = BLAST_RombergIntegrate(g, args, s, t, epsilon, 0, itmin);
#ifdef BLASTKAR_HUGE_VAL
		if (d == BLASTKAR_HUGE_VAL)
			return d;
#endif
	} while (s < mean && d < 0.4 && itmin++ < 4);

	return (d < 1. ? d : 1.);
}

static double
g(double	s, void*	vp)
{
	register double*	args = vp;
	double	mx;
	
	ARG_ADJ2 = ARG_ADJ1 - s;
	ARG_SDIVR = s / ARG_R;	/* = s / r */
	mx = (s > 0. ? ARG_SDIVR + 3. : 3.);
	return BLAST_RombergIntegrate(f, vp, 0., mx, ARG_EPS, 0, 1);
}

static double
f(double	x, void*	vp)
{
	register double*	args = vp;
	register double	y;

	y = exp(x - ARG_SDIVR);
#ifdef BLASTKAR_HUGE_VAL
	if (y == BLASTKAR_HUGE_VAL)
		return 0.;
#endif
	if (ARG_R2 == 0.)
		return exp(ARG_ADJ2 - y);
	if (x == 0.)
		return 0.;
	return exp(ARG_R2*log(x) + ARG_ADJ2 - y);
}

/*
	Calculates the p-value for alignments with "small" gaps (typically
	under fifty residues/basepairs) following ideas of Stephen Altschul's.
	"gap" gives the size of this gap, "gap_prob" is the probability
	of this model of gapping being correct (it's thought that gap_prob
	will generally be 0.5).  "num" is the number of HSP's involved, "sum" 
	is the "raw" sum-score of these HSP's. "subject_len" is the (effective)
	length of the database sequence, "query_length" is the (effective) 
	length of the query sequence.  min_length_one specifies whether one
	or 1/K will be used as the minimum expected length.

*/

double
BLAST_SmallGapSumE(BLAST_KarlinBlk* kbp, Int4 gap, double gap_prob, double gap_decay_rate, Int2 num, double score_prime, Int4 query_length, Int4 subject_length)

{

	double sum_p, sum_e;
		
	score_prime -= kbp->logK + log((double)subject_length*(double)query_length) + (num-1)*(kbp->logK + 2*log((double)gap));
	score_prime -= BLAST_LnFactorial((double) num); 

	sum_p = BlastSumP(num, score_prime);

	sum_e = BlastKarlinPtoE(sum_p);

	sum_e = sum_e/((1.0-gap_decay_rate)*BLAST_Powi(gap_decay_rate, (num-1)));

	if (num > 1)
	{
		if (gap_prob == 0.0)
			sum_e = INT4_MAX;
		else
			sum_e = sum_e/gap_prob;
	}

	return sum_e;
}

/*
	Calculates the p-value for alignments with asymmetric gaps, typically 
        a small (like in BlastSmallGapSumE) gap for one (protein) sequence and
        a possibly large (up to 4000 bp) gap in the other (translated DNA) 
        sequence. Used for linking HSPs representing exons in the DNA sequence
        that are separated by introns.
*/

double
BLAST_UnevenGapSumE(BLAST_KarlinBlk* kbp, Int4 p_gap, Int4 n_gap, double gap_prob, double gap_decay_rate, Int2 num, double score_prime, Int4 query_length, Int4 subject_length)

{

	double sum_p, sum_e;
		
	score_prime -= kbp->logK + log((double)subject_length*(double)query_length) + (num-1)*(kbp->logK + log((double)p_gap) + log((double)n_gap));
	score_prime -= BLAST_LnFactorial((double) num); 

	sum_p = BlastSumP(num, score_prime);

	sum_e = BlastKarlinPtoE(sum_p);

	sum_e = sum_e/((1.0-gap_decay_rate)*BLAST_Powi(gap_decay_rate, (num-1)));

	if (num > 1)
	{
		if (gap_prob == 0.0)
			sum_e = INT4_MAX;
		else
			sum_e = sum_e/gap_prob;
	}

	return sum_e;
}

/*
	Calculates the p-values for alignments with "large" gaps (i.e., 
	infinite) followings an idea of Stephen Altschul's.
*/

double
BLAST_LargeGapSumE(BLAST_KarlinBlk* kbp, double gap_prob, double gap_decay_rate, Int2 num, double score_prime, Int4 query_length, Int4 subject_length)

{

	double sum_p, sum_e;
/* The next two variables are for compatability with Warren's code. */
	double lcl_subject_length, lcl_query_length;

        lcl_query_length = (double) query_length;
        lcl_subject_length = (double) subject_length;

	score_prime -= num*(kbp->logK + log(lcl_subject_length*lcl_query_length)) 
	    - BLAST_LnFactorial((double) num); 

	sum_p = BlastSumP(num, score_prime);

	sum_e = BlastKarlinPtoE(sum_p);

	sum_e = sum_e/((1.0-gap_decay_rate)*BLAST_Powi(gap_decay_rate, (num-1)));

	if (num > 1)
	{
		if (gap_prob == 1.0)
			sum_e = INT4_MAX;
		else
			sum_e = sum_e/(1.0 - gap_prob);
	}

	return sum_e;
}


