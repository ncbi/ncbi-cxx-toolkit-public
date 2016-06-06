#ifndef MISC_LAPACKWRAPP___NCBI_LAPACK__H
#define MISC_LAPACKWRAPP___NCBI_LAPACK__H

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
 */

/** @file ncbi_lapack.hpp
 *  Wrappers for LAPACK routines of interest.
 */

/** @addtogroup Miscellaneous
 *
 * @{
 */

#ifdef __cplusplus
extern "C" {
#endif

static const int kNcbiLapackMemoryError = -999;
    
enum EEigensWanted {
    eEigenValues  = 'N',
    eEigenVectors = 'V'
};

enum EMatrixTriangle {
    eUpperTriangle = 'U',
    eLowerTriangle = 'L'
};

enum EMaybeTransposed {
    eTransposed    = 'T',
    eNotTransposed = 'N'
};

enum ESingularVectorDetailsWanted {
    eSVD_All       = 'A', /**< Full orthogonal matrix, in a dedicated arg. */
    eSVD_Singular  = 'S', /**< Only the singular vectors, in a dedicated arg. */
    eSVD_Overwrite = 'O', /**< Only the singular vectors, overwriting A. */
    eSVD_None      = 'N'  /**< None. */
};
    
/**
 * Compute the eigenvalues, and optionally also the eigenvectors, of a
 * symmetric matrix.  (Wraps the LAPACK routine DSYEV.)
 * @param jobz
 *   Whether to compute eigenvectors, or just eigenvalues.
 * @param uplo
 *   Which triangle of the matrix to consult.
 * @param n
 *   The order of the matrix.
 * @param a
 *   On entry, the matrix A to analyze, stored in COLUMN-MAJOR order
 *   (Fortran-style).  On successful exit in eEigenVectors mode, contains the
 *   orthonormal eigenvectors of the original matrix.  On exit in all other
 *   cases, undefined.
 * @param lda
 *   The leading dimension of A (the spacing between the start of A's columns,
 *   which need not be contiguous, just uniformly spaced and non-overlapping).
 * @param w
 *   On successful exit, contains the eigenvalues in ascending order.
 * @return
 *   0 on success, -N if the Nth argument had a bad value,
 *   kNcbiLapackMemoryError if unable to allocate temporary memory, +N if N
 *   off-diagonal elements of an intermediate tridiagonal form did not
 *   converge to zero.
 * @sa http://www.netlib.org/lapack/explore-html/d2/d8a/group__double_s_yeigen.html#ga442c43fca5493590f8f26cf42fed4044
 */
int NCBI_SymmetricEigens(enum EEigensWanted jobz, enum EMatrixTriangle uplo,
                         unsigned int n, double* a, unsigned int lda,
                         double* w);

/**
 * Find the least-squares solution of an overdetermined linear system, or the
 * minimum-norm solution of an undetermined linear system.  (Wraps the LAPACK
 * routine DGELS.)
 * @param trans
 *   Whether to work with matrix A or its transpose.
 * @param r
 *   The number of rows in matrix A.
 * @param c
 *   The number of columns in matrix A.
 * @param nrhs
 *   The number of right-hand sides (columns in matrix B).
 * @param a
 *   On entry, the matrix A, stored in COLUMN-MAJOR order (Fortran-style);
 *   expected to have full rank.  On successful exit, details of the QR or LQ
 *   factorization of A.
 * @param lda
 *   The leading dimension of A (the spacing between the start of A's columns,
 *   which need not be contiguous, just uniformly spaced and non-overlapping).
 * @param b
 *   On entry, the matrix B of right-hand side vectors, stored in COLUMN-MAJOR
 *   order (Fortran-style).  On successful exit, the corresponding solution
 *   vectors.
 * @param ldb
 *   The leading dimension of B (the spacing between the start of B's columns,
 *   which need not be contiguous, just uniformly spaced and non-overlapping).
 * @return
 *   0 on success, -N if the Nth argument had a bad value,
 *   kNcbiLapackMemoryError if unable to allocate temporary memory, +N if the
 *   Nth diagonal element of the triangular factor of A is zero.
 * @sa http://www.netlib.org/lapack/explore-html/d7/d3b/group__double_g_esolve.html#ga225c8efde208eaf246882df48e590eac
 */
int NCBI_LinearSolution(enum EMaybeTransposed trans,
                        unsigned int r, unsigned int c, unsigned int nrhs,
                        double* a, unsigned int lda,
                        double* b, unsigned int ldb);

/**
 * Compute the singular value decomposition of a matrix.  (Wraps the LAPACK
 * routine DGESVD.)
 * @param jobu
 *   How much of the left orthogonal matrix to return.
 * @param jobvt
 *   How much of the right orthogonal matrix to return.
 * @param r
 *   The number of rows in matrix A.
 * @param c
 *   The number of columns in matrix A.
 * @param a
 *   On entry, the matrix A, stored in COLUMN-MAJOR order (Fortran-style).  On
 *   successful exit when either jobu or jobvt is eSVD_Overwrite, either the
 *   left singular vectors in column-major order or the right singular vectors
 *   in row-major order, respectively.  On exit in all other cases, undefined.
 * @param lda
 *   The leading dimension of A (the spacing between the start of A's columns,
 *   which need not be contiguous, just uniformly spaced and non-overlapping).
 * @param s
 *   On success, receives the singular values of A, in descending order.
 * @param u
 *   Depending on jobu, may receive the left singular vectors in column-major
 *   order or the full left orthogonal matrix, or be left alone altogether.
 * @param ldu
 *   The leading dimension of U.  Must be positive even if no output to U is
 *   requested.
 * @param vt
 *   Depending on jobu, may receive the right singular vectors in row-major
 *   order or the full right orthogonal matrix, or be left alone altogether.
 * @param ldvt
 *   The leading dimension of VT.  Must be positive even if no output to VT is
 *   requested.
 * @param superb
 *   If the algorithm didn't converge, receives (unless NULL!) the unconverged
 *   superdiagonal elements of an upper bidiagonal matrix B whose diagonal
 *   is in S (not necessarily sorted).
 * @note jobu and jobvt cannot both be eSVD_Overwrite!
 * @return 0 on success, -N if the Nth argument had an illegal value,
 *   kNcbiLapackMemoryError if unable to allocate temporary memory, +N if N
 *   superdiagonals of an intermediate bidiagonal form didn't converge to 0.
 * @sa http://www.netlib.org/lapack/explore-html/d1/d7e/group__double_g_esing.html#ga84fdf22a62b12ff364621e4713ce02f2
 */
int NCBI_SingularValueDecomposition(enum ESingularVectorDetailsWanted jobu,
                                    enum ESingularVectorDetailsWanted jobvt,
                                    unsigned int r, unsigned int c,
                                    double* a, unsigned int lda, double* s,
                                    double* u, unsigned int ldu,
                                    double* vt, unsigned int ldvt,
                                    double* superb);

#ifdef __cplusplus
}
#endif

/* @} */

#endif  /* MISC_LAPACKWRAPP___NCBI_LAPACK__HPP */
