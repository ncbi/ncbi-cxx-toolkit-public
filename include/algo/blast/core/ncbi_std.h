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
* ===========================================================================*/

/*****************************************************************************

File name: ncbi_std.h

Author: Ilya Dondoshansky

Contents: Type and macro definitions from C toolkit that are not defined in 
          C++ toolkit.

Detailed Contents: 

******************************************************************************
 * $Revision$
 * */
#ifndef __NCBI_STD__
#define __NCBI_STD__

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef _MSC_VER
#include <strings.h>
#endif

#include <math.h>
#include <ctype.h>
#include <assert.h>

#ifndef NCBI_C_TOOLKIT
#include <corelib/ncbitype.h>
#include <algo/blast/core/ncbi_math.h>
#else
#include <ncbistd.h>
#include <ncbimath.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

#ifdef _MSC_VER
#define NCBI_INLINE __inline
#elif defined(__sgi)  &&  !defined(__GNUC__)  &&  !defined(__cplusplus)
#define NCBI_INLINE
#else
#define NCBI_INLINE inline
#endif

#ifdef _MSC_VER
#define strcasecmp _stricmp
#endif
#ifdef _MSC_VER
#define strdup _strdup
#endif

#ifndef NCBI_C_TOOLKIT
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

#ifndef NCBI_C_TOOLKIT
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

#endif /* NCBI_C_TOOLKIT */

#define LnGammaInt Nlm_LnGammaInt
 
extern void* MemDup (const void *orig, size_t size);


/******************************************************************************/

/** A generic linked list node structure */
typedef struct ListNode {
	Uint1 choice;   /* to pick a choice */
	void *ptr;              /* attached data */
	struct ListNode *next;  /* next in linked list */
} ListNode;

ListNode* ListNodeNew (ListNode* vnp);
ListNode* ListNodeAdd (ListNode** head);
ListNode* ListNodeAddPointer (ListNode** head, Uint1 choice, void *value);
ListNode* ListNodeFree (ListNode* vnp);
ListNode* ListNodeFreeData (ListNode* vnp);
ListNode* ListNodeSort (ListNode* list_to_sort, 
               int (*compar) (const void *, const void *));
ListNode* ListNodeCopyStr (ListNode** head, Uint1 choice, char* str);
Int4 ListNodeLen (ListNode* vnp);

#ifdef __cplusplus
}
#endif
#endif /* !__NCBI_STD__ */
