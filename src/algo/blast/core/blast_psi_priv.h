#ifndef ALGO_BLAST_CORE___BLAST_PSI_PRIV__H
#define ALGO_BLAST_CORE___BLAST_PSI_PRIV__H

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
 * Author:  Christiam Camacho
 *
 */

/** @file blast_psi_priv.h
 *  Private interface for Position Iterated BLAST API.
 */

/*#include <algo/blast/core/blast_psi.h>*/

#ifdef __cplusplus
extern "C" {
#endif

#define MASTER_INDEX 0

/** Generic 2 dimensional matrix allocator.
 * Allocates a ncols by nrows matrix with cells of size data_type_sz. Must be
 * freed using x_DeallocateMatrix
 * @param ncols number of columns in matrix
 * @param nrows number of rows in matrix
 * @param data_type_sz size of the data type (in bytes) to allocate for each
 * element in the matrix
 * @return pointer to allocated memory or NULL in case of failure
 */
void**
_PSIAllocateMatrix(unsigned int ncols, unsigned int nrows, 
                   unsigned int data_type_sz);

/** Generic 2 dimensional matrix deallocator.
 * Deallocates the memory allocated by x_AllocateMatrix
 * @param matrix matrix to deallocate
 * @param ncols number of columns in the matrix
 * @return NULL
 */
void**
_PSIDeallocateMatrix(void** matrix, unsigned int ncols);

/** Copies src matrix into dest matrix, both of which must be ncols by nrows 
 * matrices 
 * @param dest Destination matrix           [out]
 * @param src Source matrix                 [in]
 * @param ncols Number of columns to copy   [in]
 * @param ncows Number of rows to copy      [in]
 */
void
_PSICopyMatrix(double** dest, const double** src, 
               unsigned int ncols, unsigned int nrows);

#ifdef __cplusplus
}
#endif


/*
 * ===========================================================================
 *
 * $Log$
 * Revision 1.3  2004/05/06 14:01:40  camacho
 * + _PSICopyMatrix
 *
 * Revision 1.2  2004/04/07 21:43:47  camacho
 * Removed unneeded #include directive
 *
 * Revision 1.1  2004/04/07 19:11:17  camacho
 * Initial revision
 *
 *
 * ===========================================================================
 */

#endif /* !ALGO_BLAST_CORE__BLAST_PSI_PRIV__H */
