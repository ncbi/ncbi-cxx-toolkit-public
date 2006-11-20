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
 *  Author: Christiam Camacho
 *
 */

/** @file blast_dynarray.h
 * Declarations for various dynamic array types
 */

#ifndef __BLAST_DYNARRAY__
#define __BLAST_DYNARRAY__

#include <algo/blast/core/blast_def.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Data structure to maintain a dynamically allocated array of Uint4 */
typedef struct SDynamicUint4Array {
    Uint4 num_used;      /**< number of elements used in this array */
    Uint4 num_allocated; /**< size of array below */
    Uint4* data;         /**< array of Uint4 */
} SDynamicUint4Array;

/** Initial number of queries allocated per chunk */
#define INIT_NUM_ELEMENTS 8

/** Allocate a dynamic array of Uint4 with the initial size of
 * INIT_NUM_ELEMENTS
 * @return NULL if out of memory otherwise, newly allocated structure
 */
NCBI_XBLAST_EXPORT SDynamicUint4Array* 
DynamicUint4ArrayNew();

/** Allocate a dynamic array of Uint4 with an initial size of specified as an
 * argument to the function
 * @param init_num_elements Number of elements initially allocated [in]
 * @return NULL if out of memory otherwise, newly allocated structure
 */
NCBI_XBLAST_EXPORT SDynamicUint4Array* 
DynamicUint4ArrayNewEx(Uint4 init_num_elements);

/** Deallocates a dynamic array structure 
 * @param arr data structure to free [in]
 * @return NULL
 */
NCBI_XBLAST_EXPORT SDynamicUint4Array* 
DynamicUint4ArrayFree(SDynamicUint4Array* arr);

/** Append a Uint4 to the dynamic array structure
 * @param arr data structure to manipulate [in]
 * @param element element to add [in]
 * @return 0 on success or BLASTERR_MEMORY if memory reallocation was needed
 * and this failed
 */
NCBI_XBLAST_EXPORT Int2
DynamicUint4Array_Append(SDynamicUint4Array* arr, Uint4 element);

/** Make a deep copy of the src dynamic array
 * @param src data structure to duplicate [in]
 * @return newly allocated data structure or NULL if out of memory
 */
NCBI_XBLAST_EXPORT SDynamicUint4Array*
DynamicUint4Array_Dup(const SDynamicUint4Array* src);

/** Make a shallow copy of the src dynamic array into the dest dynamic array
 * @param dest data structure to copy to. Its contents will be overwritten and
 * memory might be allocated if necessary [in|out]
 * @param src data structure to copy from [in]
 * @return newly allocated data structure or NULL if out of memory
 */
NCBI_XBLAST_EXPORT Int4
DynamicUint4Array_Copy(SDynamicUint4Array* dest,
                       const SDynamicUint4Array* src);

/** Compares dynamic arrays a and b for equality of its contents
 * @param a dynamic array to compare [in]
 * @param b dynamic array to compare [in]
 * @return TRUE if equal, FALSE otherwise
 */
NCBI_XBLAST_EXPORT Boolean
DynamicUint4Array_AreEqual(const SDynamicUint4Array* a,
                           const SDynamicUint4Array* b);

/** Data structure to maintain a dynamically allocated array of Int4 */
typedef struct SDynamicInt4Array {
    Uint4 num_used;      /**< number of elements used in this array */
    Uint4 num_allocated; /**< size of array below */
    Int4* data;          /**< array of Int4 */
} SDynamicInt4Array;

/** Allocate a dynamic array of Int4 with the initial size of
 * INIT_NUM_ELEMENTS
 * @return NULL if out of memory otherwise, newly allocated structure
 */
NCBI_XBLAST_EXPORT SDynamicInt4Array* 
DynamicInt4ArrayNew();

/** Deallocates a dynamic array structure 
 * @param arr data structure to free [in]
 * @return NULL
 */
NCBI_XBLAST_EXPORT SDynamicInt4Array* 
DynamicInt4ArrayFree(SDynamicInt4Array* arr);

/** Append a Int4 to the dynamic array structure
 * @param arr data structure to manipulate [in]
 * @param element element to add [in]
 * @return 0 on success or BLASTERR_MEMORY if memory reallocation was needed
 * and this failed
 */
NCBI_XBLAST_EXPORT Int2
DynamicInt4Array_Append(SDynamicInt4Array* arr, Int4 element);

#ifdef __cplusplus
}
#endif
#endif /* !__BLAST_DYNARRAY__ */
