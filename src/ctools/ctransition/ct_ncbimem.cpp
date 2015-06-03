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
* File Name:  ncbimem.c
*
* Author:  Gish, Kans, Ostell, Schuler
*
* Version Creation Date:   6/4/91
*
* C Tolkit Revision: 6.30
*
* File Description:
*   	portable memory handlers for Mac, PC, Unix
*
* ==========================================================================
*/

#include <ncbi_pch.hpp>
#include <corelib/ncbifile.hpp>
#include <ctools/ctransition/ncbimem.hpp>
#include <ctools/ctransition/ncbistr.hpp>
#include <ctools/ctransition/ncbierr.hpp>

BEGIN_CTRANSITION_SCOPE


#ifdef OS_UNIX
#ifndef OS_UNIX_BEOS
/*#define USE_SETHEAPLIMIT*/
#undef USE_SETHEAPLIMIT
#endif
#endif

/* ! -- disable */
#undef USE_SETHEAPLIMIT

#ifdef USE_SETHEAPLIMIT
//#include <ncbithr.h>
#endif

/*#include <ncbiwin.h>*/

/* Used for UNIX memory-mapping. */
#ifdef MMAP_AVAIL
#  ifdef OS_UNIX_LINUX
/*   MADV_*** constants are not defined on Linux otherwise */
/*#    define __USE_BSD*/
#  endif
#  include <sys/mman.h>
#  ifdef OS_UNIX_AIX
#    include <fcntl.h>
#  else
#    include <sys/fcntl.h>
#  endif
#  ifndef MAP_FAILED
#    define MAP_FAILED ((void *) -1)
#  endif
#endif

#ifdef USE_SETHEAPLIMIT
#include <sys/resource.h>
#endif



short g_bBadPtr;

static const char * _msgMemory  = "Ran out of memory";


NLM_EXTERN void* Nlm_CallocViaMalloc(size_t n_elem, size_t item_size)
{
  size_t size = n_elem * item_size;
  void*  ptr = Nlm_Malloc(size);
  if ( ptr )
    Nlm_MemSet(ptr, 0, size);
  return ptr;
}



#ifdef USE_SETHEAPLIMIT
static size_t s_SetHeapLimit_Curr = 0;
static size_t s_SetHeapLimit_Add  = 0;
static size_t s_SetHeapLimit_Max  = 0;
static TNlmMutex s_SetHeapLimit_Mutex;

NLM_EXTERN Nlm_Boolean Nlm_SetHeapLimit(size_t curr, size_t add, size_t max)
{
  Nlm_Boolean ok = FALSE;
  struct rlimit rl;

  if (NlmMutexLockEx(&s_SetHeapLimit_Mutex) != 0)
    return FALSE;

  if ( curr ) {
    rl.rlim_cur = curr;
    rl.rlim_max = RLIM_INFINITY;
  }
  else {
    rl.rlim_cur = rl.rlim_max = RLIM_INFINITY;
  }

  if (setrlimit(RLIMIT_DATA, &rl) == 0) {
    s_SetHeapLimit_Curr = curr;
    s_SetHeapLimit_Add  = add;
    s_SetHeapLimit_Max  = max;
    ok = TRUE;
  }

  NlmMutexUnlock(s_SetHeapLimit_Mutex);
  return ok;
}

#else
NLM_EXTERN Nlm_Boolean Nlm_SetHeapLimit(size_t curr, size_t add, size_t max) {
  return FALSE;
}
#endif /* USE_SETHEAPLIMIT */


typedef enum {
  eA_Malloc,
  eA_Calloc,
  eA_Realloc
} EAllocator;

/****************************************************************************
 *
 * s_MemAllocator(ptr, size, flags, allocator)
 *   ptr       -- origin pointer(for eA_Realloc only)
 *   size      -- number of bytes to allocate
 *   flags     -- any of the following bits may be set
 *      MGET_CLEAR     clear to zeros
 *      MGET_ERRPOST   post error on allocaion failure
 *   allocator -- method
 *
 * It is a generic routine for:
 *   Nlm_MemGet(size, flags)
 *   Nlm_MemNew(size)
 *   Nlm_MemMore(ptr, size)
 *   Nlm_MemExtend(ptr, size, oldsize)
 *
 ****************************************************************************/
 
static Nlm_Boolean post_mssg_for_mem_fail = FALSE;

extern void Nlm_SetMemFailFlag (Nlm_Boolean val);
extern void Nlm_SetMemFailFlag (Nlm_Boolean val)

{
  post_mssg_for_mem_fail = val;
}

static void* s_MemAllocator(void *ptr, size_t size,
                            unsigned int flags, EAllocator allocator)
{
  void *x_ptr = 0;

  switch ( allocator ) {
  case eA_Malloc:
    if ( !size )
      return 0;
    x_ptr = Nlm_Malloc(size);
    break;
  case eA_Calloc:
    if ( !size )
      return 0;
    x_ptr = Nlm_Calloc(size, 1);
    break;
  case eA_Realloc:
    if ( !ptr ) {
      if (flags & MGET_ERRPOST)
        ErrPostEx(SEV_WARNING, E_Programmer, 0, "Attempt to realloc NULL");
      return 0;
    }
    if ( !size )
      return Nlm_MemFree(ptr);

    x_ptr = Nlm_Realloc(ptr, size);
    break;
  }

#ifdef USE_SETHEAPLIMIT
  if (!x_ptr  &&  s_SetHeapLimit_Curr) {
    NlmMutexLock(s_SetHeapLimit_Mutex);

    while (s_SetHeapLimit_Curr < s_SetHeapLimit_Max) {
      struct rlimit rl;
      size_t x_curr = s_SetHeapLimit_Curr + s_SetHeapLimit_Add;
      if (x_curr > s_SetHeapLimit_Max)
        x_curr = s_SetHeapLimit_Max;

      if (flags & MGET_ERRPOST) {
        ErrPostEx(SEV_WARNING, E_NoMemory, 0,
                  "Trying to allocate %ld bytes;  "
                  "adjusting max.avail. heap size from %ld to %ld",
                  (long)size, (long)s_SetHeapLimit_Curr, (long)x_curr);
      }

      rl.rlim_cur = x_curr;
      rl.rlim_max = RLIM_INFINITY;
      if (setrlimit(RLIMIT_DATA, &rl) != 0)
        break;

      s_SetHeapLimit_Curr = x_curr;

      switch ( allocator ) {
      case eA_Malloc:
        x_ptr = Nlm_Malloc(size);
        break;
      case eA_Calloc:
        x_ptr = Nlm_Calloc(size, 1);
        break;
      case eA_Realloc:
        x_ptr = Nlm_Realloc(ptr, size);
        break;
      }
      if ( x_ptr )
        break;
    }

    NlmMutexUnlock(s_SetHeapLimit_Mutex);
  }
#endif /* USE_SETHEAPLIMIT */

  if ( x_ptr ) {
    if (flags & MGET_CLEAR)
      memset(x_ptr, 0, size);
  }
  else if (flags & MGET_ERRPOST) {
    if (post_mssg_for_mem_fail) {
      Nlm_Message (MSG_OK, "Failed to allocate %ld bytes", (long)size);
    }
    ErrPostEx(SEV_FATAL, E_NoMemory, 0,
              "Failed to allocate %ld bytes", (long)size);
  }

  return x_ptr;
}


NLM_EXTERN void* LIBCALL Nlm_MemGet(size_t size, unsigned int flags)
{
  return s_MemAllocator(0, size, flags, eA_Malloc);
}

NLM_EXTERN void* LIBCALL Nlm_MemNew(size_t size)
{
  return s_MemAllocator(0, size, MGET_ERRPOST, eA_Calloc);
}

NLM_EXTERN void* LIBCALL Nlm_MemMore(void *ptr, size_t size)
{
  return s_MemAllocator(ptr, size, MGET_ERRPOST, eA_Realloc);
}

NLM_EXTERN void* LIBCALL Nlm_MemExtend(void *ptr, size_t size, size_t oldsize)
{
	void *x_ptr = s_MemAllocator(ptr, size, MGET_ERRPOST, eA_Realloc);
	if (x_ptr  &&  size > oldsize)
		memset((char*)x_ptr + oldsize, 0, size - oldsize);

	return x_ptr;
}


/*****************************************************************************
*
*   Nlm_MemFree(ptr)
*   	frees allocated memory
*
*****************************************************************************/

NLM_EXTERN void * LIBCALL  Nlm_MemFree (void *ptr)
{
	if (ptr != NULL)
		Nlm_Free (ptr);

    return NULL;
}


#if defined(_DEBUG)  &&  defined(OS_MSWIN)
NLM_EXTERN void * LIBCALL  Nlm_MemFreeTrace (void *ptr, const char *module,
			const char *filename, int linenum)
{
	if (ptr != NULL)
	{
		Nlm_Free(ptr);
		if (g_bBadPtr)
		{
			ErrPostEx(SEV_WARNING,E_Programmer,0,
			          "MemFree: attempt to free invalid pointer");
		}
	}
	return NULL;
}
#endif


/*****************************************************************************
*
*    void Nlm_MemCopy(Pointer to, Pointer from, Uint4 bytes)
*       WARNING: no check on overlapping regions
*
*****************************************************************************/

NLM_EXTERN void * LIBCALL  Nlm_MemCopy (void *dst, const void *src, size_t bytes)
{
    return (dst&&src) ? Nlm_MemCpy (dst, src, bytes) : NULL;
}

/*****************************************************************************
*
*    void Nlm_MemDup (Pointer orig, Uint4 bytes)
*       Duplicate the region of memory pointed to by 'orig' for 'size' length
*
*****************************************************************************/

NLM_EXTERN void * LIBCALL  Nlm_MemDup (const void *orig, size_t size)
{
	Nlm_VoidPtr	copy;

	if (orig == NULL || size == 0)
		return NULL;

	if ((copy = Nlm_Malloc (size)) == NULL)
		ErrPostEx(SEV_FATAL,E_NoMemory,0,_msgMemory);

	Nlm_MemCpy(copy, orig, size);
		return copy;
}

/*****************************************************************************
*
*    void Nlm_MemMove (Pointer to, Pointer from, Uint4 bytes)
*       This code will work on overlapping regions
*
*****************************************************************************/

NLM_EXTERN void * LIBCALL  Nlm_MemMove (void * dst, const void *src, size_t bytes)
{
	register char *dest = (char*) dst;
	register const char *sorc = (const char*) src;

	if (dest > sorc) {
		sorc += bytes;
		dest += bytes;
		while (bytes-- != 0) {
			*--dest = *--sorc;
		}
		} else {
		while (bytes-- != 0) {
			*dest++ = *sorc++;
		}
	}
	return dst;
}

/*****************************************************************************
*
*   void Nlm_MemFill(to, value, bytes)
*   	set a block of memory to a value
*
*****************************************************************************/

NLM_EXTERN void * LIBCALL  Nlm_MemFill (void *buf, int value, size_t bytes)
{
    return  buf ? Nlm_MemSet (buf, value, bytes) : NULL;
}


/*****************************************************************************
*
*   void Nlm_MemSearch(Pointer where, where_size, Pointer what, what_size)
*   	search a position one block of data into another
*
*****************************************************************************/

NLM_EXTERN size_t LIBCALL Nlm_MemSearch(const void* where, size_t where_size,
                                        const void* what, size_t what_size)
{
	size_t i, rbound, pos;
	
	rbound = where_size - what_size;
	pos = (size_t)-1;
    i = 0;
	if (where_size  &&  what_size  &&  where_size >= what_size) {
		while ((i <= rbound)  &&  (pos == (size_t)-1)) {
			if (memcmp((char*)where + i, what, what_size)==0)
                pos = i;
            else
                i++;
		}
	}
	return pos;
}


#if defined(OS_MAC) || defined(OS_UNIX_DARWIN) || defined(OS_MSWIN) || defined(MSC_VIRT)
/***** Handle functions are for Macintosh and Windows only *****/
/***** or Microsoft virtual memory manager ****/

static char * _msgNullHnd = "NULL handle passed as an argument";

#ifdef MSC_VIRT
Nlm_Boolean wrote_to_handle;   /* used by ncbibs write routines */
#endif

/*****************************************************************************
*
*   Nlm_HandGet(size, clear_out)
*   	returns handle to allocated memory
*       if (clear_out) clear memory to 0
*
*****************************************************************************/

NLM_EXTERN Nlm_Handle LIBCALL  Nlm_HandGet (size_t size, Nlm_Boolean clear_out)

{
    Nlm_VoidPtr ptr;
    Nlm_Handle  hnd;

    if (size == 0) return NULL;

#if defined(OS_MAC) || defined(OS_UNIX_DARWIN)
    hnd = (Nlm_Handle) NewHandle (size);
#endif

#ifdef OS_MSWIN
#ifdef _DLL
#ifdef WIN32
    hnd = (Nlm_Handle) HeapAlloc(GetProcessHeap(), 0, size);
#else
    hnd = (Nlm_Handle) GlobalAlloc (GMEM_MOVEABLE, size);
#endif
#else
    hnd = (Nlm_Handle) malloc (size);
#endif
#endif

#ifdef MSC_VIRT
	hnd = (Nlm_Handle) _vmalloc ((unsigned long)size);
#endif

    if (hnd == NULL)
    	ErrPostEx(SEV_FATAL,E_NoMemory,0,_msgMemory);

    else if (clear_out)	{
#ifdef MSC_VIRT
		wrote_to_handle = TRUE;
#endif
        if ((ptr = HandLock (hnd)) != NULL)
            Nlm_MemSet (ptr, 0, size);
        HandUnlock (hnd);
    }

    return  hnd;
}


/*****************************************************************************
*
*   Nlm_HandNew(size)
*
*****************************************************************************/

NLM_EXTERN Nlm_Handle LIBCALL  Nlm_HandNew (size_t size)

{
    Nlm_Handle  hnd;

    if (size == 0)  return NULL;

    if ((hnd = HandGet (size, TRUE)) == NULL)
    	ErrPostEx(SEV_FATAL,E_NoMemory,0,_msgMemory);

    return hnd;
}

/*****************************************************************************
*
*   Nlm_HandMore(hnd, size)
*
*****************************************************************************/

NLM_EXTERN Nlm_Handle LIBCALL  Nlm_HandMore (Nlm_Handle hnd, size_t size)

{
    Nlm_Handle  hnd2;

	if (size == 0) {
		Nlm_HandFree (hnd);
		return NULL;
	}

    if (hnd == NULL) {
    	ErrPostEx(SEV_WARNING,E_Programmer,0,"HandMore: %s", _msgNullHnd);
		return NULL;
    }

#if defined(OS_MAC) || defined(OS_UNIX_DARWIN)
    SetHandleSize ((Handle)hnd, (Size)size);
    hnd2 = hnd;
    if (MemError() != noErr)
        hnd2 = NULL;
#endif

#ifdef OS_MSWIN
#ifdef _DLL
#ifdef WIN32
    hnd2 = (Nlm_Handle) HeapReAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, hnd, size);
#else
    hnd2 = (Nlm_Handle) GlobalReAlloc ((HANDLE) hnd, size, GHND);
#endif
#else
    hnd2 = (Nlm_Handle) realloc (hnd, size);
#endif
#endif

#ifdef MSC_VIRT
	hnd2 = (Nlm_Handle) _vrealloc ((_vmhnd_t)hnd, (unsigned long)size);
#endif

    if (hnd2 == NULL)
    	ErrPostEx(SEV_FATAL,E_NoMemory,0,_msgMemory);

    return  hnd2;
}


/*****************************************************************************
*
*   Nlm_HandFree (hnd)
*
*****************************************************************************/

NLM_EXTERN Nlm_Handle LIBCALL  Nlm_HandFree (Nlm_Handle hnd)
{
#ifdef MSC_VIRT
	_vmhnd_t x;
#endif

    if (hnd) {

#if defined(OS_MAC) || defined(OS_UNIX_DARWIN)
        DisposeHandle ((Handle) hnd);
#endif

#ifdef OS_MSWIN
#ifdef _DLL
#ifdef WIN32
        HeapFree(GetProcessHeap(), 0, (HANDLE) hnd);
#else
        GlobalFree ((HANDLE) hnd);
#endif
#else
        free (hnd);
#endif
#endif

#ifdef MSC_VIRT
		x = (_vmhnd_t)hnd;
		while (_vlockcnt(x))
			_vunlock(x, _VM_CLEAN);
		_vfree(x);
#endif
    }
    else
    	ErrPostEx(SEV_WARNING,E_Programmer,0,"HandFree: %s", _msgNullHnd);

    return NULL;
}


/*****************************************************************************
*
*   Nlm_HandLock (hnd)
*
*****************************************************************************/

NLM_EXTERN Nlm_VoidPtr LIBCALL  Nlm_HandLock (Nlm_Handle hnd)
{
    Nlm_VoidPtr ptr;

    if (hnd == NULL) {
    	ErrPostEx(SEV_WARNING,E_Programmer,0,"HandLock: %s", _msgNullHnd);
        return NULL;
    }

#if defined(OS_MAC) || defined(OS_UNIX_DARWIN)
    HLock ((Handle) hnd);
    ptr = *((Handle) hnd);
#endif

#ifdef OS_MSWIN
#ifdef _DLL
#ifdef WIN32
    ptr = hnd;
#else
    ptr = GlobalLock ((HANDLE) hnd);
#endif
#else
    ptr = hnd;
#endif
#endif

#ifdef MSC_VIRT
	ptr = _vlock((_vmhnd_t) hnd);
#endif

    return  ptr;
}

/*****************************************************************************
*
*   Nlm_HandUnlock(hnd)
*
*****************************************************************************/

NLM_EXTERN Nlm_VoidPtr LIBCALL  Nlm_HandUnlock (Nlm_Handle hnd)
{
#ifdef MSC_VIRT
	int dirty = _VM_CLEAN;
#endif

    if (hnd == NULL)
    	ErrPostEx(SEV_WARNING,E_Programmer,0,"HandUnlock: %s", _msgNullHnd);
    else {
#if defined(OS_MAC) || defined(OS_UNIX_DARWIN)
        HUnlock ((Handle) hnd);
#endif

#ifdef OS_MSWIN
#ifdef _DLL
#ifdef WIN32
        /* nothing */
#else
        GlobalUnlock ((HANDLE) hnd);
#endif
#else
        /* nothing */
#endif
#endif

#ifdef MSC_VIRT
		if (wrote_to_handle == TRUE)
			dirty = _VM_DIRTY;
		_vunlock ((_vmhnd_t) hnd, dirty);  /* always assume dirty */
		wrote_to_handle = FALSE;
#endif
    }

    return NULL;
}

#endif /* Mac or Win */



#ifdef _WINDLL
/*****************************************************************************
*
*   Windows DLL-specific functions (shared memory)
*
*   dll_Malloc
*   dll_Calloc	(not yet)
*   dll_Realloc	(not yet)
*   dll_Free
*
*****************************************************************************/


void * dll_Malloc (size_t bytes)
{
	HGLOBAL hMem;
	void *pMem;

	if (bytes >0 && (hMem = GlobalAlloc(GMEM_DDESHARE,bytes)))
	{
		if (pMem = GlobalLock(hMem))
			return pMem;
		else
			GlobalFree(hMem);
	}

	TRACE("dll_Malloc(%ld) failed\n",bytes);
	return NULL;
}

void   dll_Free (void *pMem)
{
	HGLOBAL hMem = GlobalHandle(pMem);
	GlobalUnlock(hMem);
	GlobalFree(hMem);
}
#endif /* _WINDLL */


/*********************************************************************
*	Function to test whether memory-mapping is available.
*
*	returns TRUE if it is supported by NCBI routines.
*********************************************************************/

NLM_EXTERN Nlm_Boolean Nlm_MemMapAvailable(void)
{
#if defined(MMAP_AVAIL) || defined(WIN32)
  return TRUE;
#else
  return FALSE;
#endif
}


NLM_EXTERN Nlm_MemMapPtr Nlm_MemMapInit(const Nlm_Char PNTR name)
{
  Nlm_MemMapPtr mem_mapp;
  if (!Nlm_MemMapAvailable()  ||  !name  ||  !*name  ||
      (mem_mapp = (Nlm_MemMapPtr)Nlm_MemNew(sizeof(Nlm_MemMap))) == NULL)
    return NULL;

  for (;;) {{ /* (quasi-TRY block) */
    if ((mem_mapp->file_size = NCBI_NS_NCBI::CFile(name).GetLength()) < 0)
      break;

    if (mem_mapp->file_size == 0) /* Special case */
      return mem_mapp;
	
#ifdef WIN32
    {{
      char x_name[MAX_PATH], *str;
      Nlm_StringNCpy_0(x_name, name, sizeof(x_name));
      for (str = x_name;  *str;  str++)
        if (*str == '\\')
          *str = '/';  /* name of a file-mapping object cannot contain '\' */

      if ( !(mem_mapp->hMap =
             OpenFileMapping(FILE_MAP_READ, FALSE, x_name)) )
        { /* If failed to attach to an existing file-mapping object then
           * create a new one(based on the specified file) */
          HANDLE hFile= CreateFile(name, GENERIC_READ, FILE_SHARE_READ, NULL,
                                   OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
          if (hFile == INVALID_HANDLE_VALUE)
            break;

          mem_mapp->hMap = CreateFileMapping(hFile, NULL, PAGE_READONLY,
                                             0, 0, x_name);
          CloseHandle( hFile );
          if ( !mem_mapp->hMap )
            break;
        }

      if ( !(mem_mapp->mmp_begin = (Nlm_CharPtr)
             MapViewOfFile(mem_mapp->hMap, FILE_MAP_READ,
                           0, 0, mem_mapp->file_size)) ) {
        CloseHandle( mem_mapp->hMap );
        break;
      }
    }}

#elif defined(MMAP_AVAIL)
    {{  /* UNIX memory mapping. */
      int fd = open(name, O_RDONLY);
      if (fd < 0)
        break;

      mem_mapp->mmp_begin = (Nlm_CharPtr) mmap(NULL, mem_mapp->file_size,
                                               PROT_READ, MAP_PRIVATE, fd, 0);
      close(fd);
      if ((void*) mem_mapp->mmp_begin == (void*) MAP_FAILED)
        break;
    }}
#endif

    /* Success */
    return mem_mapp;
  }}

  /* Error;  cleanup */
  Nlm_MemFree(mem_mapp->mmp_begin);
  return NULL;
}


NLM_EXTERN void Nlm_MemMapFini(Nlm_MemMapPtr mem_mapp)
{
  if ( !mem_mapp )
    return;

#ifdef WIN32
  UnmapViewOfFile( mem_mapp->mmp_begin );
  if ( mem_mapp->hMap )
    CloseHandle( mem_mapp->hMap );
#elif defined(MMAP_AVAIL)
  munmap(mem_mapp->mmp_begin, mem_mapp->file_size);
#endif

  Nlm_MemFree( mem_mapp->mmp_begin );
}


NLM_EXTERN Nlm_Boolean Nlm_MemMapAdvise(void* addr, size_t len, EMemMapAdvise advise)
{
#if defined(HAVE_MADVISE) && defined(MADV_NORMAL)
  int adv;
  if (!addr || !len) {
    return FALSE;
  }
  switch (advise) {
	case eMMA_Random:
	  adv = MADV_RANDOM;     break;
	case eMMA_Sequential:
      adv = MADV_SEQUENTIAL; break;
	case eMMA_WillNeed:
	  adv = MADV_WILLNEED;   break;
	case eMMA_DontNeed:
	  adv = MADV_DONTNEED;   break;
	default:
	  adv = MADV_NORMAL;
  }
  /* Conversion type of "addr" to char* -- Sun Solaris fix */
  return madvise((char*)addr, len, adv) == 0;
#else
  return TRUE;
#endif
}


NLM_EXTERN Nlm_Boolean Nlm_MemMapAdvisePtr(Nlm_MemMapPtr ptr, EMemMapAdvise advise)
{
#if defined(HAVE_MADVISE) && defined(MADV_NORMAL)
  return ptr ? Nlm_MemMapAdvise(ptr->mmp_begin, ptr->file_size, advise) : FALSE;
#else
  return TRUE;
#endif
}


END_CTRANSITION_SCOPE
