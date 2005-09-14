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

File name: nlm_linear_algebra.h

Author: E. Michael Gertz

Contents: Declarations for several linear algebra routines

******************************************************************************/

#ifndef __NLM_LINEAR_ALGEBRA__
#define __NLM_LINEAR_ALGEBRA__
 
#include <algo/blast/core/blast_export.h>

#ifdef __cplusplus
extern "C" {
#endif

NCBI_XBLAST_EXPORT
double ** Nlm_DenseMatrixNew(int nrows, int ncols);

NCBI_XBLAST_EXPORT
double ** Nlm_LtriangMatrixNew(int n);

NCBI_XBLAST_EXPORT
void Nlm_DenseMatrixFree(double *** mat);

NCBI_XBLAST_EXPORT
void Nlm_FactorLtriangPosDef(double ** A, int n);

NCBI_XBLAST_EXPORT
void Nlm_SolveLtriangPosDef(double x[], int n, double ** L);

NCBI_XBLAST_EXPORT
double Nlm_EuclideanNorm(const double v[], int n);

NCBI_XBLAST_EXPORT
void Nlm_AddVectors(double y[], int n, double alpha,
                    const double x[]);

NCBI_XBLAST_EXPORT
double Nlm_StepBound(const double x[], int n,
                     const double step_x[], double max);

#ifdef __cplusplus
}
#endif

#endif
