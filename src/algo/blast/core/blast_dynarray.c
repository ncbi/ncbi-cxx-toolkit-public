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
 * Author: Christiam Camacho
 *
 */

/** @file blast_dynarray.c
 * Definitions for various dynamic array types
 */


#ifndef SKIP_DOXYGEN_PROCESSING
static char const rcsid[] = 
    "$Id$";
#endif /* SKIP_DOXYGEN_PROCESSING */

#include "blast_dynarray.h"
#include <algo/blast/core/blast_util.h>
#include <algo/blast/core/blast_message.h>  /* for error codes */

SDynamicUint4Array* DynamicUint4ArrayFree(SDynamicUint4Array* arr)
{
    if ( !arr ) {
        return NULL;
    }
    if (arr->data) {
        sfree(arr->data);
    }
    sfree(arr);
    return NULL;
}

SDynamicUint4Array* DynamicUint4ArrayNew()
{
    return DynamicUint4ArrayNewEx(INIT_NUM_ELEMENTS);
}

SDynamicUint4Array* DynamicUint4ArrayNewEx(Uint4 init_num_elements)
{
    SDynamicUint4Array* retval = 
        (SDynamicUint4Array*) calloc(1, sizeof(SDynamicUint4Array));
    if ( !retval ) {
        return NULL;
    }
    retval->data = (Uint4*) calloc(init_num_elements, sizeof(Uint4));
    if ( !retval->data ) {
        return DynamicUint4ArrayFree(retval);
    }
    retval->num_allocated = init_num_elements;

    return retval;
}

/** Grow a dynamic array of Uint4 elements
 * @param arr Structure of array data [in][out]
 * @return zero on success
 */
static Int2
s_DynamicUint4Array_ReallocIfNecessary(SDynamicUint4Array* arr)
{
    ASSERT(arr);

    if (arr->num_used+1 > arr->num_allocated) {
        /* we need more room for elements */
        arr->num_allocated *= 2;
        arr->data = (Uint4*) realloc(arr->data, 
                                     arr->num_allocated * sizeof(Uint4));
        if ( !arr->data ) {
            return BLASTERR_MEMORY;
        }
    }
    return 0;
}

Int2
DynamicUint4Array_Append(SDynamicUint4Array* arr, Uint4 element)
{
    Int2 retval = 0;
    ASSERT(arr);

    if ( (retval = s_DynamicUint4Array_ReallocIfNecessary(arr)) != 0) {
        return retval;
    }
    arr->data[arr->num_used++] = element;
    return retval;
}

SDynamicUint4Array*
DynamicUint4Array_Dup(const SDynamicUint4Array* src)
{
    SDynamicUint4Array* retval = NULL;

    if ( !src ) {
        return retval;
    }

    retval = DynamicUint4ArrayNewEx(src->num_allocated);
    memcpy((void*) retval->data, (void*) src->data, 
           sizeof(*src->data) * src->num_used);
    return retval;
}

Int4
DynamicUint4Array_Copy(SDynamicUint4Array* dest,
                       const SDynamicUint4Array* src)
{
    Uint4 i = 0;        /* index into arrays */

    if (dest->num_allocated < src->num_allocated) {
        dest->num_allocated = src->num_allocated;
        dest->data = (Uint4*) realloc(dest->data, 
                                      dest->num_allocated * sizeof(Uint4));
        if ( !dest->data ) {
            return -1;
        }
    }

    for (i = 0; i < src->num_used; i++) {
        dest->data[i] = src->data[i];
    }
    dest->num_used = src->num_used;
    return 0;
}

Boolean
DynamicUint4Array_AreEqual(const SDynamicUint4Array* a,
                           const SDynamicUint4Array* b)
{
    Uint4 i = 0;        /* index into arrays */

    if (a->num_used != b->num_used) {
        return FALSE;
    }

    for (i = 0; i < a->num_used; i++) {
        if (a->data[i] != b->data[i]) {
            return FALSE;
        }
    }

    return TRUE;
}

SDynamicInt4Array* DynamicInt4ArrayFree(SDynamicInt4Array* arr)
{
    if ( !arr ) {
        return NULL;
    }
    if (arr->data) {
        sfree(arr->data);
    }
    sfree(arr);
    return NULL;
}

SDynamicInt4Array* DynamicInt4ArrayNew()
{
    SDynamicInt4Array* retval = 
        (SDynamicInt4Array*) calloc(1, sizeof(SDynamicInt4Array));
    if ( !retval ) {
        return NULL;
    }
    retval->data = (Int4*) calloc(INIT_NUM_ELEMENTS, sizeof(Int4));
    if ( !retval->data ) {
        return DynamicInt4ArrayFree(retval);
    }
    retval->num_allocated = INIT_NUM_ELEMENTS;

    return retval;
}

/** Grow a dynamic array of Int4 elements
 * @param arr Structure of array data [in][out]
 * @return zero on success
 */
static Int2
s_DynamicInt4Array_ReallocIfNecessary(SDynamicInt4Array* arr)
{
    ASSERT(arr);

    if (arr->num_used+1 > arr->num_allocated) {
        /* we need more room for elements */
        arr->num_allocated *= 2;
        arr->data = (Int4*) realloc(arr->data, 
                                     arr->num_allocated * sizeof(Int4));
        if ( !arr->data ) {
            return BLASTERR_MEMORY;
        }
    }
    return 0;
}

Int2
DynamicInt4Array_Append(SDynamicInt4Array* arr, Int4 element)
{
    Int2 retval = 0;
    ASSERT(arr);

    if ( (retval = s_DynamicInt4Array_ReallocIfNecessary(arr)) != 0) {
        return retval;
    }
    arr->data[arr->num_used++] = element;
    return retval;
}
