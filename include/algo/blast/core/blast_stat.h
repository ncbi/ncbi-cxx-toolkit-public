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

File name: blastkar.h

Author: Tom Madden

Contents: definitions and prototypes used by blastkar.c to calculate BLAST
	statistics.

******************************************************************************/

/* $Revision$ 
* $Log$
* Revision 1.12  2003/07/31 14:31:37  camacho
* Replaced Char for char
*
* Revision 1.11  2003/07/31 14:19:18  camacho
* Replaced FloatHi for double
*
* Revision 1.10  2003/07/31 00:32:35  camacho
* Eliminated Ptr notation
*
* Revision 1.9  2003/07/30 22:01:08  dondosha
* Added some comments
*
* Revision 1.8  2003/07/30 21:52:57  camacho
* Follow conventional structure definition
*
* Revision 1.7  2003/07/30 19:43:07  camacho
* Remove PNTRs
*
* Revision 1.6  2003/07/30 18:41:25  dondosha
* Changed ValNode to ListNode
*
* Revision 1.5  2003/07/24 22:37:28  dondosha
* Removed some unused function parameters
*
* Revision 1.4  2003/07/24 21:31:12  dondosha
* Changed to calls to BlastConstructErrorMessage to API from blast_message.h
*
* Revision 1.3  2003/07/24 19:38:14  dondosha
* Removed LIBCALL, LIBCALLBACK, PROTO macros from declarations
*
* Revision 1.2  2003/07/24 17:37:52  dondosha
* Removed MakeBlastScore function that is dependent on objalign.h
*
* Revision 1.1  2003/07/24 15:18:03  dondosha
* Copy of blastkar.h from ncbitools library, stripped of dependency on ncbiobj
*
* Revision 6.31  2003/02/27 19:07:56  madden
* Add functions PrintMatrixMessage and PrintAllowedValuesMessage
*
* Revision 6.30  2003/02/26 18:23:50  madden
* Add functions BlastKarlinkGapBlkFill and BlastKarlinReportAllowedValues, call from BlastKarlinBlkGappedCalcEx
*
* Revision 6.29  2002/12/04 13:28:37  madden
* Add effective length parameters
*
* Revision 6.28  2002/09/03 14:21:50  camacho
* Changed type of karlinK from double to double
*
* Revision 6.27  2002/08/29 13:58:08  camacho
* Added field to store K parameter associated with posMatrix
*
* Revision 6.26  2002/03/18 21:31:56  madden
* Added comments
*
* Revision 6.25  2000/12/28 16:23:24  madden
* Function getAlphaBeta from AS
*
* Revision 6.24  2000/12/26 17:46:20  madden
* Add function BlastKarlinGetMatrixValuesEx2 to return alpha and beta
*
* Revision 6.23  2000/11/24 22:07:33  shavirin
* Added new function BlastResFreqFree().
*
* Revision 6.22  2000/09/27 21:27:15  dondosha
* Added original_matrix member to BLAST_Matrix structure
*
* Revision 6.21  2000/09/12 16:03:51  dondosha
* Added functions to create and destroy matrix used in txalign
*
* Revision 6.20  2000/08/31 15:45:07  dondosha
* Added function BlastUnevenGapSumE for sum evalue computation with different gap size on two sequences
*
* Revision 6.19  2000/08/23 18:50:02  madden
* Changes to support decline-to-align checking
*
* Revision 6.18  2000/08/04 15:49:26  sicotte
* added BlastScoreBlkMatCreateEx(reward,penalty) and BlastKarlinGetDefaultMatrixValues as external functions
*
* Revision 6.17  2000/07/07 21:20:08  vakatov
* Get all "#include" out of the 'extern "C" { }' scope!
*
* Revision 6.16  2000/05/26 17:29:55  shavirin
* Added array of pos frequencies into BLAST_Matrix structure and it's
* handling.
*
* Revision 6.15  2000/04/17 20:41:37  madden
* Added BLAST_MatrixFetch
*
* Revision 6.14  1999/12/22 21:06:35  shavirin
* Added new function BlastPSIMaxScoreGet().
*
* Revision 6.13  1999/09/16 17:38:42  madden
* Add posFreqs for position-specific frequencies
*
* Revision 6.12  1999/03/17 16:49:11  madden
* Removed comment within comment
*
* Revision 6.11  1998/12/31 18:17:05  madden
* Added strand option
*
 * Revision 6.10  1998/09/11 19:02:07  madden
 * Added paramC
 *
 * Revision 6.9  1998/07/17 15:39:58  madden
 * Changes for Effective search space.
 *
 * Revision 6.8  1998/04/24 19:27:55  madden
 * Added BlastKarlinBlkStandardCalcEx for ideal KarlinBlk
 *
 * Revision 6.7  1998/04/10 15:05:49  madden
 * Added pref_flags return by value to BlastKarlinGetMatrixValues
 *
 * Revision 6.6  1998/03/09 17:15:04  madden
 * Added BlastKarlinGetMatrixValues
 *
 * Revision 6.5  1998/02/26 22:34:35  madden
 * Changes for 16 bit windows
 *
 * Revision 6.4  1998/02/06 18:28:07  madden
 * Added functions to produce pseudo-scores from p and e values
 *
 * Revision 6.3  1997/11/14 17:14:57  madden
 * Added Karlin parameter to matrix structure
 *
 * Revision 6.2  1997/11/07 00:48:10  madden
 * Added defintitions and functions for BLAST_Matrix
 *
 * Revision 6.1  1997/09/22 17:36:26  madden
 * MACROS for position-specific matrices from Andy Neuwald
 *
 * Revision 6.0  1997/08/25 18:52:41  madden
 * Revision changed to 6.0
 *
 * Revision 1.16  1997/08/19 18:19:49  madden
 * BLAST_Score is Int4, not long
 *
 * Revision 1.15  1997/07/14 15:31:07  madden
 * Changed call to BlastKarlinBlkGappedCalc
 *
 * Revision 1.14  1997/05/01 15:53:16  madden
 * Addition of extra KarlinBlk's for psi-blast
 *
 * Revision 1.13  1997/01/22  17:46:30  madden
 * Added BLAST_ScorePtr* posMatrix.
 *
 * Revision 1.12  1996/12/16  14:35:48  madden
 * Removed gapped_calculation Boolean.
 *
 * Revision 1.11  1996/12/10  17:30:59  madden
 * Changed statistics for gapped blastp
 *
 * Revision 1.10  1996/12/03  19:13:47  madden
 * Made BlastRes functions non-static.
 *
 * Revision 1.9  1996/11/26  19:55:25  madden
 * Added check for matrices in standard places.
 *
 * Revision 1.8  1996/11/18  14:49:26  madden
 * Rewrote BlastScoreSetAmbigRes to take multiple ambig. chars.
 *
 * Revision 1.7  1996/11/08  21:45:03  madden
 * Added BLASTNA_SEQ_CODE define.
 *
 * Revision 1.6  1996/10/03  20:49:29  madden
 * Added function BlastKarlinBlkStandardCalc to calculate standard
 * Karlin parameters for blastx and tblastx.
 *
 * Revision 1.5  1996/10/02  21:43:31  madden
 * Replaced kbp and sfp with arrays of same.
 *
 * Revision 1.4  1996/09/30  21:56:12  madden
 * Replaced query alphabet of ncbi2na with blastna alphabet.
 *
 * Revision 1.3  1996/09/10  19:40:35  madden
 * Added function BlastScoreBlkMatCreate for blastn matrices.
 *
 * Revision 1.2  1996/08/22  20:38:11  madden
 * Changed all doubles and floats to double.
 *
 * Revision 1.1  1996/08/05  19:47:42  madden
 * Initial revision
 *
 * Revision 1.19  1996/06/21  15:23:54  madden
 * Corelibed BlastSumP.
 *
 * Revision 1.18  1996/06/21  15:14:55  madden
 * Made some functions static, added LIBCALL to others.
 *
 * Revision 1.17  1996/06/04  15:34:55  madden
 * *** empty log message ***
 *
 * Revision 1.16  1996/05/22  20:38:05  madden
 * *** empty log message ***
 *
 * Revision 1.15  1996/05/20  21:18:51  madden
 * Added BLASTMatrixStructure.
 *
 * Revision 1.14  1996/05/16  19:50:15  madden
 * Added documentation block.
 *
 * Revision 1.13  1996/04/24  12:53:12  madden
 * *** empty log message ***
 *
 * Revision 1.12  1996/04/16  15:34:10  madden
 * renamed variable in BlastLargeGapSumE.
 *
 * Revision 1.11  1996/03/25  16:34:19  madden
 * Changes to mimic old statistics.
 *
 * Revision 1.10  1996/02/15  23:20:35  madden
 * Added query length to a number of calls.
 *
 * Revision 1.9  1996/01/17  23:19:13  madden
 * Added BlastScoreSetAmbigRes.
 *
 * Revision 1.8  1996/01/17  17:01:19  madden
 * Fixed BlastSmallGapSumE  and BlastLargeGapSumE.
 *
 * Revision 1.7  1996/01/17  13:46:55  madden
 * Added prototype for BlastKarlinPtoE.
 *
 * Revision 1.6  1996/01/06  18:58:28  madden
 * Added prototype for BlastSumP.
 *
 * Revision 1.5  1995/12/30  18:39:27  madden
 * added q_frame and s_frame to KarlinBlkPtr.
 *
 * Revision 1.4  1995/12/26  23:05:19  madden
 * *** empty log message ***
 *
 * Revision 1.3  1995/12/26  20:28:10  madden
 * Replaced BLAST_ScoreMat wiht BLAST_ScorePtr*.
 *
 * Revision 1.2  1995/12/26  14:26:22  madden
 * *** empty log message ***
 *
 * Revision 1.1  1995/12/21  23:07:35  madden
 * Initial revision
 *
 *
 * */
#ifndef __BLASTKAR__
#define __BLASTKAR__

#include <blast_def.h>
#include <blast_message.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
	Defines for the matrix 'preferences' (as specified by S. Altschul).
*/
#define BLAST_MATRIX_NOMINAL 0
#define BLAST_MATRIX_PREFERRED 1
#define BLAST_MATRIX_BEST 2


/****************************************************************************
For more accuracy in the calculation of K, set K_SUMLIMIT to 0.00001.
For high speed in the calculation of K, use a K_SUMLIMIT of 0.001
Note:  statistical significance is often not greatly affected by the value
of K, so high accuracy is generally unwarranted.
*****************************************************************************/
/* K_SUMLIMIT_DEFAULT == sumlimit used in BlastKarlinLHtoK() */
#define BLAST_KARLIN_K_SUMLIMIT_DEFAULT 0.01

/* LAMBDA_ACCURACY_DEFAULT == accuracy to which Lambda should be calc'd */
#define BLAST_KARLIN_LAMBDA_ACCURACY_DEFAULT    (1.e-5)

/* LAMBDA_ITER_DEFAULT == no. of iterations in LambdaBis = ln(accuracy)/ln(2)*/
#define BLAST_KARLIN_LAMBDA_ITER_DEFAULT        17

/* Initial guess for the value of Lambda in BlastKarlinLambdaNR */
#define BLAST_KARLIN_LAMBDA0_DEFAULT    0.5

#define BLAST_KARLIN_K_ITER_MAX 100
#define BLAST_SUMP_EPSILON_DEFAULT 0.002 /* accuracy for SumP calculations */

/* 
	Where are the BLAST matrices located?
*/
#define BLASTMAT_DIR "/usr/ncbi/blast/matrix"

#define BLASTNA_SIZE 16
#define BLASTAA_SIZE 26
/* Identifies the blastna alphabet, for use in blast only. */
#define BLASTNA_SEQ_CODE 99
#define BLASTAA_SEQ_CODE 11 /* == Seq_code_ncbistdaa */
#define NCBI4NA_SEQ_CODE 4  /* == Seq_code_ncbi4na */	

#define PSI_ALPHABET_SIZE  26 /* For PSI Blast this is the only 26 */

/*
	Maps the ncbi4na alphabet to blastna, an alphabet that blastn uses
	as the first four characters have the same representation as in
	ncbi2na.
*/
extern Uint1 ncbi4na_to_blastna[BLASTNA_SIZE];
extern Uint1 blastna_to_ncbi4na[BLASTNA_SIZE];
	
/*************************************************************************
	Structure to the Karlin-Blk parameters.

	This structure was (more or less) copied from the old
	karlin.h.
**************************************************************************/

typedef struct BLAST_KarlinBlk {
		double	Lambda; /* Lambda value used in statistics */
		double	K, logK; /* K value used in statistics */
		double	H; /* H value used in statistics */
		/* "real" values are ones actually found, may be replaced by above */
		double	Lambda_real, K_real, logK_real, H_real;
		Int4 q_frame, s_frame; /* reading frame for query and subject.*/
		double	paramC;	/* for use in seed. */
	} BLAST_KarlinBlk;




/********************************************************************

	Structures relating to scoring or the BLAST_ScoreBlk

********************************************************************/

/* BLAST_Score must be a signed datatype */
typedef Int4    BLAST_Score,* BLAST_ScorePtr;

/*
SCORE_MIN is (-2**31 + 1)/2 because it has been observed more than once that
a compiler did not properly calculate the quantity (-2**31)/2.  The list
of compilers which failed this operation have at least at some time included:
NeXT and a version of AIX/370's MetaWare High C R2.1r.
For this reason, SCORE_MIN is not simply defined to be LONG_MIN/2.
*/
#define BLAST_SCORE_MIN	INT2_MIN
#define BLAST_SCORE_MAX	INT2_MAX


#if defined(OS_DOS) || defined(OS_MAC)
#define BLAST_SCORE_1MIN (-100)
#define BLAST_SCORE_1MAX ( 100)
#else
#define BLAST_SCORE_1MIN (-10000)
#define BLAST_SCORE_1MAX ( 1000)
#endif
#define BLAST_SCORE_RANGE_MAX	(BLAST_SCORE_1MAX - BLAST_SCORE_1MIN)

typedef struct BLAST_ScoreFreq {
    BLAST_Score	score_min, score_max;
    BLAST_Score	obs_min, obs_max;
    double	score_avg;
    double* sprob0,* sprob;
} BLAST_ScoreFreq;

#define BLAST_MATRIX_SIZE 32

typedef struct BLASTMatrixStructure {
    BLAST_ScorePtr matrix[BLAST_MATRIX_SIZE];
    BLAST_Score long_matrix[BLAST_MATRIX_SIZE*BLAST_MATRIX_SIZE];
} BLASTMatrixStructure;

typedef struct BLAST_ScoreBlk {
	Boolean		protein_alphabet; /* TRUE if alphabet_code is for a 
protein alphabet (e.g., ncbistdaa etc.), FALSE for nt. alphabets. */
	Uint1		alphabet_code;	/* NCBI alphabet code. */
	Int2 		alphabet_size;  /* size of alphabet. */
	Int2 		alphabet_start;  /* numerical value of 1st letter. */
	BLASTMatrixStructure* matrix_struct;	/* Holds info about matrix. */
	BLAST_ScorePtr* matrix;  /* Substitution matrix */
	BLAST_ScorePtr* posMatrix;  /* Sub matrix for position depend BLAST. */
    double karlinK; /* Karlin-Altschul parameter associated with posMatrix */
	Int2		mat_dim1, mat_dim2;	/* dimensions of matrix. */
	BLAST_ScorePtr	maxscore; /* Max. score for each letter */
	BLAST_Score	loscore, hiscore; /* Min. & max. substitution scores */
	BLAST_Score	penalty, reward; /* penalty and reward for blastn. */
	Boolean		read_in_matrix; /* If TRUE, matrix is read in, otherwise
					produce one from penalty and reward above. */
	BLAST_ScoreFreq** sfp;	/* score frequencies. */
	double **posFreqs; /*matrix of position specific frequencies*/
	/* kbp & kbp_gap are ptrs that should be set to kbp_std, kbp_psi, etc. */
	BLAST_KarlinBlk** kbp; 	/* Karlin-Altschul parameters. */
	BLAST_KarlinBlk** kbp_gap; /* K-A parameters for gapped alignments. */
	/* Below are the Karlin-Altschul parameters for non-position based ('std')
	and position based ('psi') searches. */
	BLAST_KarlinBlk **kbp_std,	
                    **kbp_psi,	
                    **kbp_gap_std,
                    **kbp_gap_psi;
	BLAST_KarlinBlk* 	kbp_ideal;	/* Ideal values (for query with average database composition). */
	Int2 number_of_contexts;	/* Used by sfp and kbp, how large are these*/
	Int2		matid;		/* id number of matrix. */
	char* 	name;		/* name of matrix. */
	Uint1* 	ambiguous_res;	/* Array of ambiguous res. (e.g, 'X', 'N')*/
	Int2		ambig_size,	/* size of array above. */
			ambig_occupy;	/* How many occupied? */
	ListNode*	comments;	/* Comments about matrix. */
/**** Andy's modification ****/
	Int4    	query_length;   /* the length of the query. */
/**** end Andy's modification ****/
	Int4	length_adjustment; /* length to trim query/db sequence by. */
	Int4	effective_query_length; /* shortened query length. */
	Int8	effective_db_length;	/* trimmed db length */
	Int8	effective_search_sp;	/* product of above two */
} BLAST_ScoreBlk;

/* Used for communicating between BLAST and other applications. */
typedef struct BLAST_Matrix {
    Boolean is_prot;	/* Matrix is for proteins */
    char* name;		/* Name of Matrix (i.e., BLOSUM62). */
    /* Position-specific BLAST rows and columns are different, otherwise they are the
    alphabet length. */
    Int4	rows,		/* query length + 1 for PSSM. */
        columns;	/* alphabet size in all cases (26). */
    Int4** matrix;
    double ** posFreqs;
    double karlinK;
    Int4** original_matrix;
} BLAST_Matrix;

typedef struct BLAST_ResComp {
    Uint1	alphabet_code;
    Int4*	comp; 	/* composition of alphabet, array starts at beginning of alphabet. */
    Int4*   comp0;	/* Same array as above, starts at zero. */
} BLAST_ResComp;

typedef struct BLAST_ResFreq {
    Uint1		alphabet_code;
    double* prob;	/* probs, (possible) non-zero offset. */
    double* prob0; /* probs, zero offset. */
} BLAST_ResFreq;

BLAST_ScoreBlk* BLAST_ScoreBlkNew (Uint1 alphabet, Int2 number_of_contexts);

BLAST_ScoreBlk* BLAST_ScoreBlkDestruct (BLAST_ScoreBlk* sbp);

Int2 BlastScoreSetAmbigRes (BLAST_ScoreBlk* sbp, char ambiguous_res);


Int2 BlastScoreBlkFill (BLAST_ScoreBlk* sbp, char* string, Int4 length, Int2 context_number);
 
/** This function fills in the BLAST_ScoreBlk structure.  
 * Tasks are:
 *	-read in the matrix
 *	-set maxscore
 * @param sbp Scoring block [in] [out]
 * @param matrix_path Full path to the matrix in the directory structure [in]
*/
Int2 BlastScoreBlkMatFill (BLAST_ScoreBlk* sbp, char* matrix);
BLAST_ScorePtr* BlastScoreBlkMatCreateEx(BLAST_ScorePtr* matrix,BLAST_Score penalty, BLAST_Score reward);
 
Int2 BlastScoreBlkMatRead (BLAST_ScoreBlk* sbp, FILE *fp);
 
Int2 BlastScoreBlkMaxScoreSet (BLAST_ScoreBlk* sbp);
BLAST_ScorePtr BlastPSIMaxScoreGet(BLAST_ScorePtr* posMatrix, 
                                   Int4 start, Int4 length);

BLAST_ResComp* BlastResCompNew (BLAST_ScoreBlk* sbp);

BLAST_ResComp* BlastResCompDestruct (BLAST_ResComp* rcp);

Int2 BlastResCompStr (BLAST_ScoreBlk* sbp, BLAST_ResComp* rcp, char* str, Int4 length);

/* 
Produces a Karlin Block, and parameters, with standard protein frequencies.
*/
Int2 BlastKarlinBlkStandardCalc (BLAST_ScoreBlk* sbp, Int2 context_start, Int2 context_end);
BLAST_KarlinBlk* BlastKarlinBlkStandardCalcEx (BLAST_ScoreBlk* sbp);



/*
	Functions taken from the OLD karlin.c
*/

BLAST_KarlinBlk* BlastKarlinBlkCreate (void);

BLAST_KarlinBlk* BlastKarlinBlkDestruct (BLAST_KarlinBlk*);

Int2 BlastKarlinBlkCalc (BLAST_KarlinBlk* kbp, BLAST_ScoreFreq* sfp);

Int2 BlastKarlinBlkGappedCalc (BLAST_KarlinBlk* kbp, Int4 gap_open, Int4 gap_extend, char* matrix_name, Blast_Message** error_return);

Int2 BlastKarlinBlkGappedCalcEx (BLAST_KarlinBlk* kbp, Int4 gap_open, Int4 gap_extend, Int4 decline_align, char* matrix_name, Blast_Message** error_return);


/*
        Attempts to fill KarlinBlk for given gap opening, extensions etc.
        Will return non-zero status if that fails.

        return values:  -1 if matrix_name is NULL;
                        1 if matrix not found
                        2 if matrix found, but open, extend etc. values not supported.
*/
Int2 BlastKarlinkGapBlkFill(BLAST_KarlinBlk* kbp, Int4 gap_open, Int4 gap_extend, Int4 decline_align, char* matrix_name);

/* Prints a messages about the allowed matrices, BlastKarlinkGapBlkFill should return 1 before this is called. */
char* PrintMatrixMessage(const char *matrix);

/* Prints a messages about the allowed open etc values for the given matrix, 
BlastKarlinkGapBlkFill should return 2 before this is called. */
char* PrintAllowedValuesMessage(const char *matrix, Int4 gap_open, Int4 gap_extend, Int4 decline_align);

Int2 BlastKarlinReportAllowedValues(const char *matrix_name, Blast_Message** error_return);


double BlastKarlinLHtoK (BLAST_ScoreFreq* sfp, double lambda, double H);

double BlastKarlinLambdaBis (BLAST_ScoreFreq* sfp);

double BlastKarlinLambdaNR (BLAST_ScoreFreq* sfp);

double BlastKarlinLtoH (BLAST_ScoreFreq* sfp, double  lambda);

BLAST_Score BlastKarlinEtoS (double  E, BLAST_KarlinBlk* kbp, double  qlen, double  dblen);

BLAST_Score BlastKarlinEtoS_simple (double  E, BLAST_KarlinBlk* kbp, double searchsp); 

double BlastKarlinPtoE (double p);

double BlastKarlinEtoP (double x);

double BlastKarlinStoP (BLAST_Score S, BLAST_KarlinBlk* kbp, double  qlen, double  dblen);

double BlastKarlinStoP_simple (BLAST_Score S, BLAST_KarlinBlk* kbp, double  searchsp);

double BlastKarlinStoE (BLAST_Score S, BLAST_KarlinBlk* kbp, double  qlen, double  dblen);

double BlastKarlinStoE_simple (BLAST_Score S, BLAST_KarlinBlk* kbp, double  searchsp);

Int2 BlastCutoffs (BLAST_ScorePtr S, double* E, BLAST_KarlinBlk* kbp, double qlen, double dblen, Boolean dodecay);


Int2 BlastCutoffs_simple (BLAST_ScorePtr S, double* E, BLAST_KarlinBlk* kbp, double search_sp, Boolean dodecay);
double BlastKarlinStoLen (BLAST_KarlinBlk* kbp, BLAST_Score S);

/* SumP function. Called by BlastSmallGapSumE and BlastLargeGapSumE. */
double BlastSumP (Int4 r, double s);

/* Functions to calculate SumE (for large and small gaps). */
double BlastSmallGapSumE (BLAST_KarlinBlk* kbp, Int4 gap, double gap_prob, double gap_decay_rate, Int2 num,  double xsum, Int4 query_length, Int4 subject_length);

double BlastUnevenGapSumE (BLAST_KarlinBlk* kbp, Int4 p_gap, Int4 n_gap, double gap_prob, double gap_decay_rate, Int2 num,  double xsum, Int4 query_length, Int4 subject_length);

double BlastLargeGapSumE (BLAST_KarlinBlk* kbp, double gap_prob, double gap_decay_rate, Int2 num,  double xsum, Int4 query_length, Int4 subject_length);

/* Used to produce random sequences. */
char*  BlastRepresentativeResidues (Int2 length);

Int2 BlastResFreqNormalize (BLAST_ScoreBlk* sbp, BLAST_ResFreq* rfp, double norm);

BLAST_ResFreq* BlastResFreqNew (BLAST_ScoreBlk* sbp);
void BlastResFreqFree (BLAST_ResFreq* rfp);

BLAST_ResFreq* BlastResFreqDestruct (BLAST_ResFreq* rfp);

Int2 BlastResFreqString (BLAST_ScoreBlk* sbp, BLAST_ResFreq* rfp, char* string, Int4 length);

Int2 BlastResFreqStdComp (BLAST_ScoreBlk* sbp, BLAST_ResFreq* rfp);

Int2 BlastResFreqResComp (BLAST_ScoreBlk* sbp, BLAST_ResFreq* rfp, BLAST_ResComp* rcp);

Int2 BlastResFreqClr (BLAST_ScoreBlk* sbp, BLAST_ResFreq* rfp);

BLAST_ScoreFreq* BlastScoreFreqNew (BLAST_Score score_min, BLAST_Score score_max);

BLAST_ScoreFreq* BlastScoreFreqDestruct (BLAST_ScoreFreq* sfp);

BLAST_Matrix* BLAST_MatrixDestruct (BLAST_Matrix* blast_matrix);

BLAST_Matrix* BLAST_MatrixFill (BLAST_ScoreBlk* sbp);

BLAST_Matrix* BLAST_MatrixFetch (char* matrix_name);


Int2 BlastGetStdAlphabet (Uint1 alphabet_code, Uint1* residues, Uint4 residues_size);
/*
Functions used to convert between Stephen's pseudo scores
and E or p-values.
*/
Int2 ConvertPtoPseudoS (double p, double n);
Int2 ConvertEtoPseudoS (double E, double searchsp);
double ConvertPseudoStoE (Int2 s, double n);

/*
Obtains arrays of the allowed opening and extension penalties for gapped BLAST for
the given matrix.  Also obtains arrays of Lambda, K, and H.  The pref_flags field is
used for display purposes, with the defines: BLAST_MATRIX_NOMINAL, BLAST_MATRIX_PREFERRED, and
BLAST_MATRIX_BEST.

Any of these fields that
are not required should be set to NULL.  The Int2 return value is the length of the
arrays.
*/

Int2 BlastKarlinGetMatrixValues (char* matrix, Int4** open, Int4** extension, double** lambda, double** K, double** H, Int4** pref_flags);

Int2 BlastKarlinGetMatrixValuesEx (char* matrix, Int4** open, Int4** extension, Int4** decline_align, double** lambda, double** K, double** H, Int4** pref_flags);

Int2 BlastKarlinGetMatrixValuesEx2 (char* matrix, Int4** open, Int4** extension, Int4** decline_align, double** lambda, double** K, double** H, double** alpha, double** beta, Int4** pref_flags);

void getAlphaBeta (char* matrixName, double *alpha,
double *beta, Boolean gapped, Int4 gap_open, Int4 gap_extend);

Int2 BlastKarlinGetDefaultMatrixValues (char* matrix, Int4* open, Int4* extension, double* lambda, double* K, double* H);

Int4** BlastMatrixToTxMatrix (BLAST_Matrix* matrix);
Int4** TxMatrixDestruct (Int4** txmatrix); 

#ifdef __cplusplus
}
#endif
#endif /* !__BLASTKAR__ */
