/* $Id$
 * By Jonathan Kans, NCBI (kans@ncbi.nlm.nih.gov)
 *
 * NOTE:  Unlike its UNIX counterpart, this configuration header
 *        is manually maintained in order to keep it in-sync with the
 *        "configure"-generated configuration headers.
 */

#define NCBI_CXX_TOOLKIT  1

#ifdef __MWERKS__
#  define NCBI_COMPILER_METROWERKS 1
#endif

#ifdef __MACH__
#  define NCBI_OS_DARWIN 1
#  define NCBI_OS_UNIX 1
#  define NCBI_OS      "MAC OSX"
#  define HOST         "PowerPC-Apple-MacOSX"
#  define HOST_OS      "MacOSX"
#  define _MT           1
   /* fix for /usr/include/ctype.h */
#  define _USE_CTYPE_INLINE_ 1
#else
#  define NCBI_OS_MAC  1
#  define NCBI_OS      "MAC"
#  define HOST         "PowerPC-Apple-MacOS"
#  define HOST_OS      "MacOS"
#endif

#ifdef NCBI_COMPILER_MW_MSL
#  undef NCBI_COMPILER_MW_MSL
#endif

#if defined(NCBI_OS_DARWIN) && defined(NCBI_COMPILER_METROWERKS)
#  if _MSL_USING_MW_C_HEADERS
#    define NCBI_COMPILER_MW_MSL
#  endif
#endif

#define HOST_CPU     "PowerPC"
#define HOST_VENDOR  "Apple"

#define NCBI_USE_THROW_SPEC 1
#define STACK_GROWS_DOWN    1

#define SIZEOF_CHAR         1
#define SIZEOF_DOUBLE       8
#define SIZEOF_INT          4
#define SIZEOF_LONG         4
#define SIZEOF_LONG_DOUBLE  8
#define SIZEOF_LONG_LONG    8
#define SIZEOF_SHORT        2
#define SIZEOF_SIZE_T       4
#define SIZEOF_VOIDP        4
#define NCBI_PLATFORM_BITS  32

#define STDC_HEADERS  1

/* Define to 1 if your processor stores words with the most significant byte
   first (like Motorola and SPARC, unlike Intel and VAX). */
#define WORDS_BIGENDIAN 1

/* Define to 1 if you have the <arpa/inet.h> header file. */
#define HAVE_ARPA_INET_H 1

/* Define to 1 if Berkeley DB libraries are available. */
#define HAVE_BERKELEY_DB 1

/* Define to 1 if you have the <dlfcn.h> header file. */
#define HAVE_DLFCN_H 1

/* Define to 1 if FLTK is available. */
#define HAVE_FLTK 1

/* Define to 1 if you have the <fstream> header file. */
#define HAVE_FSTREAM 1

/* Define to 1 if you have the <fstream.h> header file. */
#define HAVE_FSTREAM_H 1

/* Define to 1 if you have the `getpagesize' function. */
#define HAVE_GETPAGESIZE 1

/* Define to 1 if you have the <inttypes.h> header file. */
#define HAVE_INTTYPES_H 1

/* Define to 1 if you have the <iostream> header file. */
#define HAVE_IOSTREAM 1

/* Define to 1 if you have the <iostream.h> header file. */
#define HAVE_IOSTREAM_H 1

/* Define to 1 if you have `ios(_base)::register_callback'. */
#define HAVE_IOS_REGISTER_CALLBACK 1

/* Define to 1 if you have `ios(_base)::xalloc'. */
#define HAVE_IOS_XALLOC 1

/* Define to 1 if CRYPT is available, either in its own library or as part of
   the standard libraries. */
#define HAVE_LIBCRYPT 1

/* Define to 1 if DL is available, either in its own library or as part of the
   standard libraries. */
#define HAVE_LIBDL 1

/* Define to 1 if you have the <limits> header file. */
#define HAVE_LIMITS 1

/* Define to 1 if you have the `localtime_r' function. */
#define HAVE_LOCALTIME_R 1

/* Define to 1 if you have the <memory.h> header file. */
#define HAVE_MEMORY_H 1

/* Define to 1 if you have the <netinet/in.h> header file. */
#define HAVE_NETINET_IN_H 1

/* Define to 1 if you have OpenGL (-lGL). */
#define HAVE_OPENGL 1

/* Define to 1 if you have the `pthread_setconcurrency' function. */
#define HAVE_PTHREAD_SETCONCURRENCY 1

/* Define to 1 if you have the `sched_yield' function. */
#define HAVE_SCHED_YIELD 1

/* Define to 1 if `sin_len' is a member of `struct sockaddr_in'. */
#define HAVE_SIN_LEN 1

/* Define to 1 if you have the <stdint.h> header file. */
#define HAVE_STDINT_H 1

/* Define to 1 if you have the <stdlib.h> header file. */
#define HAVE_STDLIB_H 1

/* Define to 1 if you have the `strcasecmp' function. */
#define HAVE_STRCASECMP 1

/* Define to 1 if you have the `strdup' function. */
#define HAVE_STRDUP 1

/* Define to 1 if you have the <strings.h> header file. */
#define HAVE_STRINGS_H 1

/* Define to 1 if you have the <string.h> header file. */
#define HAVE_STRING_H 1

/* Define to 1 if you have the <strstream> header file. */
#define HAVE_STRSTREAM 1

/* Define to 1 if you have the <strstream.h> header file. */
#define HAVE_STRSTREAM_H 1

/* Define to 1 if you have the <sys/socket.h> header file. */
#define HAVE_SYS_SOCKET_H 1

/* Define to 1 if you have the <sys/sockio.h> header file. */
#define HAVE_SYS_SOCKIO_H 1

/* Define to 1 if you have the <sys/stat.h> header file. */
#define HAVE_SYS_STAT_H 1

/* Define to 1 if you have the <sys/types.h> header file. */
#define HAVE_SYS_TYPES_H 1

/* Define to 1 if you have the `timegm' function. */
#define HAVE_TIMEGM 1

/* Define to 1 if you have the <unistd.h> header file. */
#define HAVE_UNISTD_H 1

/* Define to 1 if you have the `vasprintf' function. */
#define HAVE_VASPRINTF 1

