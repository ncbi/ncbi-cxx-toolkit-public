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

/* to silence an Apple header warning in dlfcn.h */
#define DLOPEN_NO_WARN  1

/* Define to 1 if your processor stores words with the most significant byte
   first (like Motorola and SPARC, unlike Intel and VAX). */
#define WORDS_BIGENDIAN 1

/* Define to 1 if you have the <arpa/inet.h> header file. */
#define HAVE_ARPA_INET_H 1

/* Define to 1 if you have the `atoll' function. */
#define HAVE_ATOLL 1

/* Define to 1 if Berkeley DB libraries are available. */
#define HAVE_BERKELEY_DB 1

/* Define to 1 if the preprocessor supports GNU-style variadic macros. */
#define HAVE_CPP_GNU_VARARGS 1

/* Define to 1 if the preprocessor supports C99-style variadic macros. */
#define HAVE_CPP_STD_VARARGS 1

/* Define to 1 if you have the <dlfcn.h> header file. */
#define HAVE_DLFCN_H 1

/* Define to 1 if you don't have `vprintf' but do have `_doprnt.' */
/* #undef HAVE_DOPRNT */

/* Define to 1 if you have the `FCGX_Accept_r' function. */
/* #undef HAVE_FCGX_ACCEPT_R */

/* Define to 1 if FLTK is available. */
#define HAVE_FLTK 1

/* Define to 1 if you have the <fstream> header file. */
#define HAVE_FSTREAM 1

/* Define to 1 if you have the <fstream.h> header file. */
#define HAVE_FSTREAM_H 1

/* Define to 1 if you have the `getaddrinfo' function. */
#define HAVE_GETADDRINFO 1

/* If you have the `gethostbyaddr_r' function, define to the number of
   arguments it takes (normally 7 or 8). */
/* #undef HAVE_GETHOSTBYADDR_R */

/* If you have the `gethostbyname_r' function, define to the number of
   arguments it takes (normally 5 or 6). */
/* #undef HAVE_GETHOSTBYNAME_R */

/* Define to 1 if you have the `getloadavg' function. */
/* #undef HAVE_GETLOADAVG */

/* Define to 1 if you have the `getnameinfo' function. */
#define HAVE_GETNAMEINFO 1

/* Define to 1 if you have the `getpagesize' function. */
#define HAVE_GETPAGESIZE 1

/* If you have the `getservbyname_r' function, define to the number of
   arguments it takes (normally 5 or 6). */
/* #undef HAVE_GETSERVBYNAME_R */

/* Define to 1 if you have the <ieeefp.h> header file. */
/* #undef HAVE_IEEEFP_H */

/* Define to 1 if you have the <inttypes.h> header file. */
#define HAVE_INTTYPES_H 1

/* Define to 1 if you the system headers define the type intptr_t */
#define HAVE_INTPTR_T 1

/* Define to 1 if you have the <iostream> header file. */
#define HAVE_IOSTREAM 1

/* Define to 1 if you have the <iostream.h> header file. */
#define HAVE_IOSTREAM_H 1

/* Define to 1 if you have `ios(_base)::register_callback'. */
#define HAVE_IOS_REGISTER_CALLBACK 1

/* Define to 1 if libbz2 is available. */
#define HAVE_LIBBZ2 1

/* Define to 1 if CRYPT is available, either in its own library or as part of
   the standard libraries. */
#define HAVE_LIBCRYPT 1

/* Define to 1 if DL is available, either in its own library or as part of the
   standard libraries. */
#define HAVE_LIBDL 1

/* Define to 1 if libexpat is available. */
/* #undef HAVE_LIBEXPAT */

/* Define to 1 if FastCGI libraries are available. */
/* #undef HAVE_LIBFASTCGI */

/* Define to 1 if FreeTDS libraries are available. */
/* #undef HAVE_LIBFTDS */

/* Define to 1 if NCBI GEO DB library is available. */
/* #undef HAVE_LIBGEODB */

/* Define to 1 if libgif is available. */
/* #undef HAVE_LIBGIF */

/* Define to 1 if you have libglut. */
/* #undef HAVE_LIBGLUT */

/* Define to 1 if ICONV is available, either in its own library or as part of
   the standard libraries. */
#define HAVE_LIBICONV 1

/* Define to 1 if libjpeg is available. */
/* #undef HAVE_LIBJPEG */

/* Define to 1 if KSTAT is available, either in its own library or as part of
   the standard libraries. */
/* #undef HAVE_LIBKSTAT */

/* Define to 1 if liboechem is available. */
/* #undef HAVE_LIBOECHEM */

/* Define to 1 if you have libOSMesa. */
/* #undef HAVE_LIBOSMESA */

/* Define to 1 if libpcre is available. */
#define HAVE_LIBPCRE 1

/* Define to 1 if libpng is available. */
#define HAVE_LIBPNG 1

/* Define to 1 if NCBI PubMed libraries are available. */
/* #undef HAVE_LIBPUBMED */

/* Define to 1 if RPCSVC is available, either in its own library or as part of
   the standard libraries. */
/* #undef HAVE_LIBRPCSVC */

/* Define to 1 if RT is available, either in its own library or as part of the
   standard libraries. */
/* #undef HAVE_LIBRT */

/* Define to 1 if libsablot is available. */
/* #undef HAVE_LIBSABLOT */

/* Define to 1 if the SP SGML library is available. */
/* #undef HAVE_LIBSP */

/* Define to 1 if libsqlite is available. */
#define HAVE_LIBSQLITE 1

/* Define to 1 if the NCBI SSS DB library is available. */
/* #undef HAVE_LIBSSSDB */

/* Define to 1 if the NCBI SSS UTILS library is available. */
/* #undef HAVE_LIBSSSUTILS */

/* Define to 1 if SYBASE libraries are available. */
/* #undef HAVE_LIBSYBASE */

/* Define to 1 if SYBASE DBLib is available. */
/* #undef HAVE_LIBSYBDB */

/* Define to 1 if libtiff is available. */
#define HAVE_LIBTIFF 1

/* Define to 1 if libungif is available. */
/* #undef HAVE_LIBUNGIF */

/* Define to 1 if libxml2 is available. */
/* #undef HAVE_LIBXML */

/* Define to 1 if libXpm is available. */
/* #undef HAVE_LIBXPM */

/* Define to 1 if libz is available. */
#define HAVE_LIBZ 1

/* Define to 1 if you have the <limits> header file. */
#define HAVE_LIMITS 1

/* Define to 1 if you have the `localtime_r' function. */
#define HAVE_LOCALTIME_R 1

/* Define to 1 if local LBSM support is available. */
#define HAVE_LOCAL_LBSM 1

/* Define to 1 if you have the <malloc.h> header file. */
/* #undef HAVE_MALLOC_H */

/* Define to 1 if you have the <memory.h> header file. */
#define HAVE_MEMORY_H 1

/* Define to 1 if MySQL is available. */
/* #undef HAVE_MYSQL */

/* Define to 1 if the NCBI C toolkit is available. */
/* #undef HAVE_NCBI_C */

/* Define to 1 if you have the <netinet/in.h> header file. */
#define HAVE_NETINET_IN_H 1

/* Define to 1 if `auto_ptr<T>' is missing or broken. */
/* #undef HAVE_NO_AUTO_PTR */

/* Define to 1 if `std::char_traits' is missing. */
/* #undef HAVE_NO_CHAR_TRAITS */

/* Define to 1 if new C++ streams lack `ios_base::'. */
/* #undef HAVE_NO_IOS_BASE */

/* Define to 1 if `min'/`max' templates are not implemented. */
/* #undef HAVE_NO_MINMAX_TEMPLATE */

/* Define to 1 if ODBC libraries are available. */
/* #undef HAVE_ODBC */

/* Define to 1 if you have the <odbcss.h> header file. */
/* #undef HAVE_ODBCSS_H */

/* Define to 1 if you have OpenGL (-lGL). */
#define HAVE_OPENGL 1

/* Define to 1 if the ORBacus CORBA package is available. */
/* #undef HAVE_ORBACUS */

/* Define to 1 if you have the <poll.h> header file. */
#define HAVE_POLL_H 1

/* Define to 1 if you have the `pthread_atfork' function. */
/* #undef HAVE_PTHREAD_ATFORK */

/* Define to 1 if you have the `pthread_setconcurrency' function. */
#define HAVE_PTHREAD_SETCONCURRENCY 1

/* Define to 1 if the PUBSEQ service is available. */
/* #undef HAVE_PUBSEQ_OS */

/* If you have the `readdir_r' function, define to the number of arguments it
   takes (normally 2 or 3). */
#define HAVE_READDIR_R 3

/* Define to 1 if you have the `sched_yield' function. */
#define HAVE_SCHED_YIELD 1

/* Define to 1 if you have `union semun'. */
#define HAVE_SEMUN 1

/* Define to 1 if `sin_len' is a member of `struct sockaddr_in'. */
#define HAVE_SIN_LEN 1

/* Define to 1 if the system has the type `socklen_t'. */
#define HAVE_SOCKLEN_T 1

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

/* Define to 1 if you have the <strstrea.h> header file. */
/* #undef HAVE_STRSTREA_H */

/* Define to 1 if SYBASE has reentrant libraries. */
/* #undef HAVE_SYBASE_REENTRANT */

/* Define to 1 if Linux-like 1-arg sysinfo exists. */
/* #undef HAVE_SYSINFO_1 */

/* Define to 1 if you have the `sysmp' function. */
/* #undef HAVE_SYSMP */

/* Define to 1 if you have SysV semaphores. */
#define HAVE_SYSV_SEMAPHORES 1

/* Define to 1 if you have the <sys/socket.h> header file. */
#define HAVE_SYS_SOCKET_H 1

/* Define to 1 if you have the <sys/sockio.h> header file. */
#define HAVE_SYS_SOCKIO_H 1

/* Define to 1 if you have the <sys/stat.h> header file. */
#define HAVE_SYS_STAT_H 1

/* Define to 1 if you have the <sys/sysinfo.h> header file. */
/* #undef HAVE_SYS_SYSINFO_H */

/* Define to 1 if you have the <sys/types.h> header file. */
#define HAVE_SYS_TYPES_H 1

/* Define to 1 if you have the `timegm' function. */
#define HAVE_TIMEGM 1

/* Define to 1 if you have the <unistd.h> header file. */
#define HAVE_UNISTD_H 1

/* Define to 1 if you have the `vasprintf' function. */
#define HAVE_VASPRINTF 1

/* Define to 1 if you have the `vprintf' function. */
#define HAVE_VPRINTF 1

/* Define to 1 if you have the `vsnprintf' function. */
#define HAVE_VSNPRINTF 1

/* Define to 1 if you have the <windows.h> header file. */
/* #undef HAVE_WINDOWS_H */

/* Define to 1 if the system has the type `wstring'. */
#define HAVE_WSTRING 1

/* Define to 1 if wxWindows is available. */
/* #undef HAVE_WXWINDOWS */

/* Define to 0xffffffff if your operating system doesn't. */
/* #undef INADDR_NONE */


/* This is the NCBI C++ Toolkit. */
#define NCBI_CXX_TOOLKIT 1

/* Define to 1 if `string::compare()' is non-standard. */
#define NCBI_OBSOLETE_STR_COMPARE 1


/* Define to the architecture size. */
#define NCBI_PLATFORM_BITS 32

/* Define to 1 if prototypes can use exception specifications. */
#define NCBI_USE_THROW_SPEC 1

/* Define to the address where bug reports for this package should be sent. */
#define PACKAGE_BUGREPORT "cpp-core@ncbi.nlm.nih.gov"

/* Define to the full name of this package. */
#define PACKAGE_NAME "ncbi-tools++"

/* Define to the full name and version of this package. */
#define PACKAGE_STRING "ncbi-tools++ 0.0"

/* Define to the one symbol short name of this package. */
#define PACKAGE_TARNAME "ncbi-tools--"

/* Define to the version of this package. */
#define PACKAGE_VERSION "0.0"


/* Define to 1 if the stack grows down. */
#define STACK_GROWS_DOWN 1

/* Define to 1 if the stack grows up. */
/* #undef STACK_GROWS_UP */

/* Define to 1 if you have the ANSI C header files. */
#define STDC_HEADERS 1

/* Define to 1 if using a local copy of bzlib. */
/* #undef USE_LOCAL_BZLIB */

/* Define to 1 if using a local copy of PCRE. */
/* #undef USE_LOCAL_PCRE */

/* Define to 1 if your processor stores words with the most significant byte
   first (like Motorola and SPARC, unlike Intel and VAX). */
#define WORDS_BIGENDIAN 1

/* Define to 1 if the X Window System is missing or not being used. */
/* #undef X_DISPLAY_MISSING */

/* Define to 1 if type `char' is unsigned and you are not using gcc.  */
#ifndef __CHAR_UNSIGNED__
/* # undef __CHAR_UNSIGNED__ */
#endif

/* Define to empty if `const' does not conform to ANSI C. */
/* #undef const */

/* Define to `unsigned' if <sys/types.h> does not define. */
/* #undef size_t */
