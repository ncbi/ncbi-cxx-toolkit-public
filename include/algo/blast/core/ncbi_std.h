/* $Id$
 * ===========================================================================
 *
 *                            PUBLIC DOMAIN NOTICE
 *               National Center for Biotechnology Information
 *
 *  This software/database is a "United States Government Work" under the
 *  terms of the United States Copyright Act.  It was written as part of
 *  the author's offical duties as a United States Government employee and
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
 * Author: Ilya Dondoshansky
 *
 */

/** @file ncbi_std.h
 * Type and macro definitions from C toolkit that are not defined in C++ 
 * toolkit.
 */



#ifndef __NCBI_STD__
#define __NCBI_STD__

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <math.h>
#include <ctype.h>
#include <assert.h>

/* which toolkit are we using? */
#include "blast_toolkit.h"

#include <algo/blast/core/ncbi_math.h>

#ifdef __cplusplus
extern "C" {
#endif

/* For some reason, ICC claims a suitable __STDC_VERSION__ but then
   barfs on restrict. */
#ifdef __ICC
#define NCBI_RESTRICT __restrict
#elif __STDC_VERSION__ >= 199901
#define NCBI_RESTRICT restrict
#else
#define NCBI_RESTRICT
#endif

/* inlining support -- compiler dependent */
#if defined(__cplusplus)  ||  __STDC_VERSION__ >= 199901
/* C++ and C99 both guarantee "inline" */
#define NCBI_INLINE inline
#elif defined(__GNUC__)
/* So does GCC, normally, but it may be running with strict options
   that require the extra underscores */
#define NCBI_INLINE __inline__
#elif defined(_MSC_VER)  ||  defined(__sgi) || defined(HPUX)
/* MSVC and (older) MIPSpro always require leading underscores */
#define NCBI_INLINE __inline
#else
/* "inline" seems to work on our remaining in-house compilers
   (WorkShop, Compaq, ICC, MPW) */
#define NCBI_INLINE inline
#endif

#ifdef _MSC_VER
#define strcasecmp _stricmp
#define strdup _strdup
#define snprintf _snprintf
#endif

#ifndef _NCBISTD_ /* if we're not in the C toolkit... */
typedef Uint1 Boolean;
#ifndef TRUE
#define TRUE 1
#endif
#ifndef FALSE
#define FALSE 0
#endif
#endif

#ifndef ASSERT
#define ASSERT assert
#endif

#ifndef MIN
#define MIN(a,b)	((a)>(b)?(b):(a))
#endif

#ifndef MAX
#define MAX(a,b)	((a)>=(b)?(a):(b))
#endif

#ifndef ABS
#define ABS(a)	((a)>=0?(a):-(a))
#endif

#ifndef SIGN
#define SIGN(a)	((a)>0?1:((a)<0?-1:0))
#endif

/* low-level ANSI-style functions */

#ifndef _NCBISTD_ /* if we're not in the C toolkit ... */
#define UINT4_MAX     4294967295U
#define INT4_MAX    2147483647
#define INT4_MIN    (-2147483647-1)
#define NCBIMATH_LN2      0.69314718055994530941723212145818
#define LN2         (0.693147180559945)
#define INT2_MAX    32767
#define INT2_MIN    (-32768)

#ifndef DIM
#define DIM(A) (sizeof(A)/sizeof((A)[0]))
#endif

#define NULLB '\0'

#ifndef PATH_MAX
#define PATH_MAX 1024
#endif
#define DIRDELIMSTR        "/"

#else

#endif /* _NCBISTD_ */

extern void* BlastMemDup (const void *orig, size_t size);


/******************************************************************************/

/** A generic linked list node structure */
typedef struct ListNode {
	Uint1 choice;   /**< to pick a choice */
	void *ptr;              /**< attached data */
	struct ListNode *next;  /**< next in linked list */
} ListNode;

/** Create a new list node */
ListNode* ListNodeNew (ListNode* vnp);

/** Add a node to the list.
 * @param head Pointer to the start of the list. [in] [out]
 * @return New node
 */
ListNode* ListNodeAdd (ListNode** head);
/** Add a node to the list with a given choice and data pointer.
 * @param head Pointer to the start of the list [in] [out]
 * @param choice Choice value for the new node. [in]
 * @param value Data pointer for the new node. [in]
 * @return New node
 */
ListNode* ListNodeAddPointer (ListNode** head, Uint1 choice, void *value);
/** Free all list's nodes */
ListNode* ListNodeFree (ListNode* vnp);
/** Free only the attached data for all list's nodes */
ListNode* ListNodeFreeData (ListNode* vnp);
/** Sort the list, using a provided comparison function. */
ListNode* ListNodeSort (ListNode* list_to_sort, 
               int (*compar) (const void *, const void *));
/** Add a node to the list with a provided choice, and attached data 
 * pointing to a provided string.
 */
ListNode* ListNodeCopyStr (ListNode** head, Uint1 choice, char* str);
/** Count number of nodes in a list */
Int4 ListNodeLen (ListNode* vnp);

#ifdef __cplusplus
}
#endif
#endif /* !__NCBI_STD__ */
