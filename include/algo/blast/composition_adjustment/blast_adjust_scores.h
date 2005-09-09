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

File name: blast_adjust_scores.h

Authors: E. Michael Gertz, Alejandro Schaffer, Yi-Kuo Yu
******************************************************************************/

#ifndef __BLAST_ADJUST_SCORES__
#define __BLAST_ADJUST_SCORES__

#ifdef __cplusplus
extern "C" {
#endif

typedef struct Blast_CompositionWorkspace {
    int flag;             /* determines which of the optimization
                             problems are solved */
    double ** mat_b;
    double ** score_old;
    double ** mat_final;
    double ** score_final;

    double RE_final;       /* the relative entropy used, either
                              re_o_implicit or re_o_newcontext */
    double RE_o_implicit;  /* used for eRelEntropyOldMatrixOldContext
                              mode */

    double * first_seq_freq;          /* freq vector of first seq */
    double * second_seq_freq;         /* freq. vector for the second. */
    double * first_standard_freq;     /* background freq vector of first
                                         seq using matrix */
    double * second_standard_freq;    /* background freq vector for
                                                   the second. */
    double * first_seq_freq_wpseudo;  /* freq vector of first seq
                                         w/pseudocounts */
    double * second_seq_freq_wpseudo; /* freq. vector for the
                                         second seq w/pseudocounts */
} Blast_CompositionWorkspace;

Blast_CompositionWorkspace * Blast_CompositionWorkspaceNew();
void Blast_CompositionWorkspaceFree(Blast_CompositionWorkspace ** NRrecord);
int Blast_CompositionWorkspaceInit(Blast_CompositionWorkspace * NRrecord,
                                   const char *matrixName);
double 
Blast_AdjustComposition(const char *matrixName,
                        int length1, int length2,
                        const double *probArray1, 
                        const double *probArray2,
                        int pseudocounts, double specifiedRE, 
                        Blast_CompositionWorkspace * NRrecord);

#ifdef __cplusplus
}
#endif

#endif
