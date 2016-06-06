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
* Author:  Aaron Ucko
*
* File Description:
*   Wrappers for LAPACK routines of interest.
*
* ===========================================================================
*/

#include <misc/lapackwrapp/ncbi_lapack.h>
#include <stdlib.h>
#include <string.h>

#include <ncbiconf.h>

#ifdef HAVE_LAPACKE_H
#  define HAVE_LAPACK_CONFIG_H 1
#  include <lapacke.h>
#elif defined(HAVE_LAPACKE_LAPACKE_H)
#  define HAVE_LAPACK_CONFIG_H 1
#  include <lapacke/lapacke.h>
#elif defined(HAVE_ACCELERATE_ACCELERATE_H)
#  include <Accelerate/Accelerate.h>
#elif defined(HAVE___CLPK_INTEGER)
#  include <clapack.h>
#else
typedef int                     TLapackInt;
typedef int                     TLapackLogical;
typedef struct { float  r, i; } TLapackComplexFloat;
typedef struct { double r, i; } TLapackComplexDouble;
#  ifdef HAVE_CLAPACK_H
#    define complex       TLapackComplexFloat
#    define doublecomplex TLapackComplexDouble
#    define integer       TLapackInt
#    define logical       TLapackLogical
#    define VOID          void
#    include <clapack.h>
#    undef complex
#    undef doublecomplex
#    undef integer
#    undef logical
#    undef VOID
#  endif
#endif

#if defined(LAPACK_malloc)
typedef lapack_int            TLapackInt;
typedef lapack_logical        TLapackLogical;
typedef lapack_complex_float  TLapackComplexFloat;
typedef lapack_complex_double TLapackComplexDouble;
#elif defined(HAVE___CLPK_INTEGER) 
typedef __CLPK_integer       TLapackInt;
typedef __CLPK_logical       TLapackLogical;
typedef __CLPK_complex       TLapackComplexFloat;
typedef __CLPK_doublecomplex TLapackComplexDouble;
#endif

/* Fall back as necessary to supplying our own prototypes to work around
 * https://bugzilla.redhat.com/show_bug.cgi?id=1165538 or the like. */
#if !defined(LAPACK_COL_MAJOR)  &&  !defined(__CLAPACK_H)
TLapackInt dsyev_(char* jobz, char* uplo, TLapackInt* n,
                  double* a, TLapackInt* lda, double* w,
                  double* work, TLapackInt* lwork, TLapackInt* info);
TLapackInt dgels_(char* trans, TLapackInt* m, TLapackInt* n,
                  TLapackInt* nrhs, double* a, TLapackInt* lda,
                  double* b, TLapackInt* ldb,
                  double* work, TLapackInt* lwork, TLapackInt* info);
TLapackInt dgesvd_(char* jobu, char* jobvt, TLapackInt* m, TLapackInt* n,
                   double* a, TLapackInt* lda, double* s,
                   double* u, TLapackInt* ldu, double* vt, TLapackInt* ldvt,
                   double* work, TLapackInt* lwork, TLapackInt* info);
#endif

static unsigned int s_Min(unsigned int x, unsigned int y)
{
    return x < y ? x : y;
}

int NCBI_SymmetricEigens(enum EEigensWanted jobz_in,
                         enum EMatrixTriangle uplo_in, unsigned int n_in,
                         double* a, unsigned int lda_in, double* w)
{
    double     optimal_work_size;
    char       jobz = jobz_in, uplo = uplo_in;
    TLapackInt n = n_in, lda = lda_in, lwork = -1, info = 0;
    dsyev_(&jobz, &uplo, &n, a, &lda, w, &optimal_work_size, &lwork, &info);
    if (info == 0) {
        double* work;
        lwork = optimal_work_size;
        work  = malloc(lwork * sizeof(double));
        if (work == NULL) {
            return kNcbiLapackMemoryError;
        }
        dsyev_(&jobz, &uplo, &n, a, &lda, w, work, &lwork, &info);
        free(work);
    }
    return info;
}

int NCBI_LinearSolution(enum EMaybeTransposed trans_in,
                        unsigned int r, unsigned int c, unsigned int nrhs_in,
                        double* a, unsigned int lda_in,
                        double* b, unsigned int ldb_in)
{
    double     ows;
    char       trans = trans_in;
    TLapackInt m = r, n = c, nrhs = nrhs_in, lda = lda_in, ldb = ldb_in,
               lwork = -1, info = 0;
    dgels_(&trans, &m, &n, &nrhs, a, &lda, b, &ldb, &ows, &lwork, &info);
    if (info == 0) {
        double* work;
        lwork = ows;
        work  = malloc(lwork * sizeof(double));
        if (work == NULL) {
            return kNcbiLapackMemoryError;
        }
        dgels_(&trans, &m, &n, &nrhs, a, &lda, b, &ldb, work, &lwork, &info);
        free(work);
    }
    return info;
}

int NCBI_SingularValueDecomposition(enum ESingularVectorDetailsWanted jobu_in,
                                    enum ESingularVectorDetailsWanted jobvt_in,
                                    unsigned int r, unsigned int c,
                                    double* a, unsigned int lda_in, double* s,
                                    double* u, unsigned int ldu_in,
                                    double* vt, unsigned int ldvt_in,
                                    double* superb)
{
    double     optimal_work_size;
    char       jobu = jobu_in, jobvt = jobvt_in;
    TLapackInt m = r, n = c, lda = lda_in, ldu = ldu_in, ldvt = ldvt_in,
               lwork = -1, info = 0;
    dgesvd_(&jobu, &jobvt, &m, &n, a, &lda, s, u, &ldu, vt, &ldvt,
            &optimal_work_size, &lwork, &info);
    if (info == 0) {
        double* work;
        lwork = optimal_work_size;
        work  = malloc(lwork * sizeof(double));
        if (work == NULL) {
            return kNcbiLapackMemoryError;
        }
        dgesvd_(&jobu, &jobvt, &m, &n, a, &lda, s, u, &ldu, vt, &ldvt,
                work, &lwork, &info);
        if (superb != NULL /* && info > 0 */) {
            memcpy(superb, work + 1, (s_Min(r, c) - 1) * sizeof(double));
        }
        free(work);
    }
    return info;
}
