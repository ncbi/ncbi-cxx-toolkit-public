#ifndef _NCBIMEM_
#define _NCBIMEM_

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
* File Name:  ncbimem.h
*
* Author:  Gish, Kans, Ostell, Schuler
*
* Version Creation Date:   1/1/91
*
* C Tolkit Revision: 6.12
*
* File Description:
*   	prototypes for ncbi memory functions
*
* ==========================================================================
*/

#include <corelib/ncbistd.hpp> 
#include <ctools/ctransition/ncbistd.hpp>

/** @addtogroup CToolsBridge
 *
 * @{
 */

BEGIN_CTRANSITION_SCOPE


NLM_EXTERN void*  LIBCALL Nlm_MemNew(size_t size);
NLM_EXTERN void*  LIBCALL Nlm_MemGet(size_t size, unsigned int flags);
NLM_EXTERN void*  LIBCALL Nlm_MemMore(void* ptr, size_t size);
NLM_EXTERN void*  LIBCALL Nlm_MemExtend(void* ptr, size_t size, size_t oldsize);
NLM_EXTERN void*  LIBCALL Nlm_MemFree(void* ptr);
NLM_EXTERN void*  LIBCALL Nlm_MemCopy(void* dst, const void* src, size_t bytes);
NLM_EXTERN void*  LIBCALL Nlm_MemMove(void* dst, const void* src, size_t bytes);
NLM_EXTERN void*  LIBCALL Nlm_MemFill(void* ptr, int value, size_t bytes);
NLM_EXTERN void*  LIBCALL Nlm_MemDup(const void* orig, size_t size);
NLM_EXTERN size_t LIBCALL Nlm_MemSearch(const void* where, size_t where_size,
                                        const void* what, size_t what_size);

#if 0
#if defined(OS_MAC) || defined(OS_UNIX_DARWIN) || defined(OS_MSWIN) || defined(MSC_VIRT)
NLM_EXTERN Nlm_Handle  LIBCALL Nlm_HandNew(size_t size);
NLM_EXTERN Nlm_Handle  LIBCALL Nlm_HandGet(size_t size, Nlm_Boolean clear_out);
NLM_EXTERN Nlm_Handle  LIBCALL Nlm_HandMore(Nlm_Handle hnd, size_t size);
NLM_EXTERN Nlm_Handle  LIBCALL Nlm_HandFree(Nlm_Handle hnd);
NLM_EXTERN void* LIBCALL Nlm_HandLock(Nlm_Handle hnd);
NLM_EXTERN void* LIBCALL Nlm_HandUnlock(Nlm_Handle hnd);
#endif
#endif /*0*/



/* [UNIX only] set the limit for the heap size and its increase policy for
 *  NCBI memory allocation functions:
 *    MemGet, MemNew, MemMore, MemExtend
 *  when the heap size reaches "curr", it ussues a warning, then it increases
 *  "curr" by "add" bytes -- unless "curr" has already reached "max".
 *  in the latter case, program ussues a FATAL_ERROR error message and
 *  the NCBI allocation function returns NULL
 *  NOTE: specifying "curr" == 0 switch off the heap restriction algorithm;
 *        and it is off by default(if Nlm_SetHeapLimit has not been called)
 */
NLM_EXTERN Nlm_Boolean Nlm_SetHeapLimit(size_t curr, size_t add, size_t max);


/* Do Nlm_Calloc by {Nlm_Malloc + Nlm_MemSet} rather than native {calloc}
 */
NLM_EXTERN void* Nlm_CallocViaMalloc(size_t n_elem, size_t item_size);



/* ========= MACROS ======== */


/* low-level ANSI-style functions */
#define Nlm_Malloc  malloc
#define Nlm_Calloc  calloc
#define Nlm_Realloc realloc
#define Nlm_Free    free
#define Nlm_MemSet  memset
#define Nlm_MemCpy  memcpy
#define Nlm_MemChr  memchr
#define Nlm_MemCmp  memcmp


#ifdef OS_UNIX_SOL
/* Kludge for Solaris("calloc()" sometimes fails in MT aplications) */
#undef Nlm_Calloc
#define Nlm_Calloc Nlm_CallocViaMalloc
#endif


#define Malloc  Nlm_Malloc
#define Calloc  Nlm_Calloc
#define Realloc Nlm_Realloc
#define Free    Nlm_Free
#define MemSet  Nlm_MemSet
#define MemCpy  Nlm_MemCpy
#define MemChr  Nlm_MemChr
#define MemCmp  Nlm_MemCmp
#define MemSearch  Nlm_MemSearch

/*** High-level NCBI functions ***/

/* Fake handle functions with pointer functions */

#if !(defined(OS_MAC) || defined(OS_UNIX_DARWIN) || defined(OS_MSWIN) || defined(MSC_VIRT) )
#define Nlm_HandNew(a)    Nlm_MemNew(a)
#define Nlm_HandGet(a,b)  Nlm_MemGet(a,b)
#define Nlm_HandMore(a,b) Nlm_MemMore(a,b)
#define Nlm_HandFree(a)   Nlm_MemFree(a)
#define Nlm_HandLock(a)   (a)
#define Nlm_HandUnlock(a) NULL
#endif

/* Pointer functions */
#define MemNew(x)     Nlm_MemGet(x,MGET_CLEAR|MGET_ERRPOST)
#define MemGet(x,y)   Nlm_MemGet(x,(unsigned int)(y))
#define MemFree       Nlm_MemFree
#define MemMore       Nlm_MemMore
#define MemExtend     Nlm_MemExtend
#define MemCopy       Nlm_MemCopy
#define MemMove       Nlm_MemMove
#define MemFill       Nlm_MemFill
#define MemDup        Nlm_MemDup

#define HandNew     Nlm_HandNew
#define HandGet     Nlm_HandGet
#define HandMore    Nlm_HandMore
#define HandFree    Nlm_HandFree
#define HandLock    Nlm_HandLock
#define HandUnlock  Nlm_HandUnlock

#if (defined(OS_UNIX_SYSV) || defined(OS_UNIX_SUN) || defined(OS_UNIX_OSF1) || defined(OS_UNIX_LINUX) || defined(OS_UNIX_AIX) || defined(OS_UNIX_DARWIN)) && !defined(OS_UNIX_ULTRIX) || defined(OS_UNIX_FREEBSD)
#ifndef IBM_DISABLE_MMAP
#define MMAP_AVAIL
#endif
#endif



#if defined(_DEBUG)  &&  defined(OS_MSWIN)
NLM_EXTERN void* LIBCALL Nlm_MemFreeTrace (void* , const char*, const char*, int);
#undef MemFree
#define MemFree(_ptr_)  Nlm_MemFreeTrace(_ptr_,THIS_MODULE,THIS_FILE,__LINE__)
#endif


#ifdef _WINDLL
NLM_EXTERN void* dll_Malloc(size_t bytes);
NLM_EXTERN void  dll_Free  (void*  pMem);
#else
#define dll_Malloc(x)	(void*) Nlm_Malloc(x)
#define dll_Free(x)	   Nlm_Free((void*) (x))
#endif


/* flags for MemGet */
#define MGET_CLEAR    0x0001
#define MGET_ERRPOST  0x0004


/* obsolete */
#define MG_CLEAR    MGET_CLEAR
#define MG_MAXALLOC 0x0002
#define MG_ERRPOST  MGET_ERRPOST



/****************************************************************************
 * Memory mapping
 */

/* This structure is allocated and filled by Nlm_MemMapInit.
   The Nlm_Handle's are used by WIN32, "file_size" is used by
   UNIX memory mapping when the the files are unmapped. */
typedef struct _nlm_mem_map
{
#ifdef WIN32
  Nlm_Handle hMap;
#endif
  Nlm_Int8    file_size;
  Nlm_CharPtr mmp_begin;
} Nlm_MemMap, PNTR Nlm_MemMapPtr;

/* Determine if memory-mapping is supported by the NCBI toolkit
 */
NLM_EXTERN Nlm_Boolean Nlm_MemMapAvailable(void);


/* Initializes the memory mapping on file "name"
 * Return NULL on error
 * NOTE:  return non-NULL zero-filled structure if the file has zero length
 */
NLM_EXTERN Nlm_MemMapPtr Nlm_MemMapInit(const Nlm_Char PNTR name);


/* Close the memory mapping
 */
NLM_EXTERN void Nlm_MemMapFini(Nlm_MemMapPtr mem_mapp);


/* Advises the VM system that the a certain region of user mapped memory 
   will be accessed following a type of pattern. The VM system uses this 
   information to optimize work wih mapped memory.
 */
typedef enum {
  eMMA_Normal,		/* No further special threatment     */
  eMMA_Random,		/* Expect random page references     */
  eMMA_Sequential,	/* Expect sequential page references */
  eMMA_WillNeed,	/* Will need these pages             */
  eMMA_DontNeed		/* Don't need these pages            */
} EMemMapAdvise;

Nlm_Boolean Nlm_MemMapAdvise(void* addr, size_t len, EMemMapAdvise advise);
Nlm_Boolean Nlm_MemMapAdvisePtr(Nlm_MemMapPtr ptr, EMemMapAdvise advise);


#endif /* _NCBIMEM_ */


END_CTRANSITION_SCOPE


/* @} */
