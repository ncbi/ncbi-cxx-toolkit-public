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
 * Author:  Christiam Camacho
 *
 */

/** @file blast_psi_priv.c
 * Defintions for functions in private interface for Position Iterated BLAST 
 * API.
 */

static char const rcsid[] =
    "$Id$";

#include "blast_psi_priv.h"
#include <algo/blast/core/blast_def.h>

void**
_PSIAllocateMatrix(unsigned int ncols, unsigned int nrows, 
                 unsigned int data_type_sz)
{
    void** retval = NULL;
    unsigned int i = 0;

    if ( !(retval = (void**) malloc(sizeof(void*) * ncols)))
        return NULL;

    for (i = 0; i < ncols; i++) {
        if ( !(retval[i] = (void*) calloc(nrows, data_type_sz))) {
            retval = _PSIDeallocateMatrix(retval, i);
            break;
        }
    }
    return retval;
}

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

void
_PSICopyMatrix(double** dest, const double** src, 
               unsigned int ncols, unsigned int nrows)
{
    unsigned int i = 0;
    unsigned int j = 0;

    for (i = 0; i < ncols; i++) {
        for (j = 0; j < nrows; j++) {
            dest[i][j] = src[i][j];
        }
    }
}

/*
 * ===========================================================================
 *
 * $Log$
 * Revision 1.4  2004/05/19 14:52:02  camacho
 * 1. Added doxygen tags to enable doxygen processing of algo/blast/core
 * 2. Standardized copyright, CVS $Id string, $Log and rcsid formatting and i
 *    location
 * 3. Added use of @todo doxygen keyword
 *
 * Revision 1.3  2004/05/06 14:01:40  camacho
 * + _PSICopyMatrix
 *
 * Revision 1.2  2004/04/07 22:08:37  kans
 * needed to include blast_def.h for sfree prototype
 *
 * Revision 1.1  2004/04/07 19:11:17  camacho
 * Initial revision
 *
 *
 * ===========================================================================
 */
