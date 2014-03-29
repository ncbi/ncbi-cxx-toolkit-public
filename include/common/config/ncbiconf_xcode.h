/* $Id$
 * By Vlad Lebedev, NCBI (lebedev@ncbi.nlm.nih.gov)
 *
 * Mac OS X - xCode Build
 *
 * NOTE:  Unlike its UNIX counterpart, this configuration header
 *        is manually maintained in order to keep it in-sync with the
 *        "configure"-generated configuration headers.
 */

#include <AvailabilityMacros.h>

/* This is the NCBI C++ Toolkit. */
#define NCBI_CXX_TOOLKIT 1

/* Operating system name */
#define NCBI_OS "Mac OS X"

/* Define to 1 on AIX. */
/* #undef NCBI_OS_AIX */

/* Define to 1 on *BSD. */
/* #undef NCBI_OS_BSD */

/* Define to 1 on Cygwin. */
/* #undef NCBI_OS_CYGWIN */

/* Define to 1 on Mac OS X. */
#define NCBI_OS_DARWIN 1

/* Define to 1 on IRIX. */
/* #undef NCBI_OS_IRIX */

/* Define to 1 on Linux. */
/* #undef NCBI_OS_LINUX */

/* Define to 1 on Windows. */
/* #undef NCBI_OS_MSWIN */

/* Define to 1 on Tru64 Unix. */
/* #undef NCBI_OS_OSF1 */

/* Define to 1 on Solaris. */
/* #undef NCBI_OS_SOLARIS */

/* Define to 1 on Unix. */
#define NCBI_OS_UNIX 1

/* Compiler name */
#define NCBI_COMPILER "GCC"

/* Compiler name */
/* #undef NCBI_COMPILER_COMPAQ */

/* Compiler name */
#define NCBI_COMPILER_GCC 1

/* Compiler name */
/* #undef NCBI_COMPILER_ICC */

/* Compiler name */
/* #undef NCBI_COMPILER_KCC */

/* Compiler name */
/* #undef NCBI_COMPILER_MIPSPRO */

/* Compiler name */
/* #undef NCBI_COMPILER_MSVC */

/* Compiler name */
/* #undef NCBI_COMPILER_UNKNOWN */

/* Compiler version as three-digit integer */
#define NCBI_COMPILER_VERSION (__GNUC__ * 100 + \
                               __GNUC_MINOR__ * 10 + \
                               __GNUC_PATCHLEVEL__)

/* Compiler name */
/* #undef NCBI_COMPILER_VISUALAGE */

/* Compiler name */
/* #undef NCBI_COMPILER_WORKSHOP */

/* Full GNU-style system type */
#define HOST "i686-apple-darwin10.8.0"

/* CPU type only */
#define HOST_CPU "i686"

/* System OS only */
#define HOST_OS "darwin10.8.0"

/* System vendor only */
#define HOST_VENDOR "apple"


/* Define to 1 if the plugin manager should load DLLs by default. */
#define NCBI_PLUGIN_AUTO_LOAD 1

/* Define to 1 if building dynamic libraries by default. */
#define NCBI_DLL_BUILD 1

/* Define to 1 if building dynamic libraries at all (albeit not necessarily by
   default). */
#ifdef NCBI_DLL_BUILD
#  define NCBI_DLL_SUPPORT 1
#endif


/* Define to 1 if necessary to get FIONBIO (e.g., on Solaris) */
/* #undef BSD_COMP */

/* Define to 1 if you have the <arpa/inet.h> header file. */
#define HAVE_ARPA_INET_H 1

/* Define to 1 if you have the `asprintf' function. */
#define HAVE_ASPRINTF 1

/* Define to 1 if you have the `atoll' function. */
#define HAVE_ATOLL 1

/* Define to 1 if you have the <atomic.h> header file. */
/* #undef HAVE_ATOMIC_H */

/* Define to 1 if your C compiler supports __attribute__((destructor)) */
#define HAVE_ATTRIBUTE_DESTRUCTOR 1

/* Define to 1 if your compiler supports
   __attribute__((visibility("default"))) */
#define HAVE_ATTRIBUTE_VISIBILITY_DEFAULT 1

/* Define to 1 if you have the `basename' function. */
#define HAVE_BASENAME 1

/* Define to 1 if the Berkeley `db_cxx' library is available. */
/* #undef HAVE_BERKELEY_DB_CXX */

/* Define to 1 if the `Boost.Regex' library is available. */
/* #undef HAVE_BOOST_REGEX */

/* Define to 1 if the `Boost.Spirit' headers are available. */
/* #undef HAVE_BOOST_SPIRIT */

/* Define to 1 if the `Boost.Threads' library is available. */
/* #undef HAVE_BOOST_THREAD */

/* Define to 1 if the preprocessor supports GNU-style variadic macros. */
#define HAVE_CPP_GNU_VARARGS 1

/* Define to 1 if the preprocessor supports C99-style variadic macros. */
#define HAVE_CPP_STD_VARARGS 1

/* Define to 1 if you have the <dlfcn.h> header file. */
#define HAVE_DLFCN_H 1

/* Define to 1 if you don't have `vprintf' but do have `_doprnt.' */
/* #undef HAVE_DOPRNT */

/* Define to 1 if you have the `erf' function. */
#define HAVE_ERF 1

/* Define to 1 if you have the <errno.h> header file. */
#define HAVE_ERRNO_H 1

/* Define to 1 if you have the `euidaccess' function. */
/* #undef HAVE_EUIDACCESS */

/* Define to 1 if you have the `freehostent' function. */
#define HAVE_FREEHOSTENT 1

/* Define to 1 if FreeType is available. */
/* #undef HAVE_FREETYPE */

/* Define to 1 if you have the `fseeko' function. */
#define HAVE_FSEEKO 1

/* Define to 1 if you have the <fstream> header file. */
#define HAVE_FSTREAM 1

/* Define to 1 if you have the <fstream.h> header file. */
#define HAVE_FSTREAM_H 1

/* Define to 1 if your localtime_r returns a int. */
/* #undef HAVE_FUNC_LOCALTIME_R_INT */

/* Define to 1 if your localtime_r returns a struct tm*. */
#define HAVE_FUNC_LOCALTIME_R_TM 1

/* Define to 1 if you have the `getaddrinfo' function. */
#define HAVE_GETADDRINFO 1

/* If you have the `gethostbyaddr_r' function, define to the number of
   arguments it takes (normally 7 or 8). */
/* #undef HAVE_GETHOSTBYADDR_R */

/* If you have the `gethostbyname_r' function, define to the number of
   arguments it takes (normally 5 or 6). */
/* #undef HAVE_GETHOSTBYNAME_R */

/* Define to 1 if you have the `gethostent_r' function. */
/* #undef HAVE_GETHOSTENT_R */

/* Define to 1 if you have the `getipnodebyaddr' function. */
#define HAVE_GETIPNODEBYADDR 1

/* Define to 1 if you have the `getipnodebyname' function. */
#define HAVE_GETIPNODEBYNAME 1

/* Define to 1 if you have the `getloadavg' function. */
#define HAVE_GETLOADAVG 1

/* Define to 1 if you have the `getlogin_r' function */
#define HAVE_GETLOGIN_R 1

/* Define to 1 if you have the `getnameinfo' function. */
#define HAVE_GETNAMEINFO 1

/* Define to 1 if you have the `getpagesize' function. */
#define HAVE_GETPAGESIZE 1

/* Define to 1 if you have the `getpwuid' function. */
#define HAVE_GETPWUID 1

/* Define to 1 if you have the `getrusage' function. */
#define HAVE_GETRUSAGE 1

/* If you have the `getservbyname_r' function, define to the number of
   arguments it takes (normally 5 or 6). */
/* #undef HAVE_GETSERVBYNAME_R */

/* Define to 1 if you have the `gettimeofday' function. */
#define HAVE_GETTIMEOFDAY 1

/* Define to 1 if you have the `getuid' function. */
#define HAVE_GETUID 1

/* Define to 1 if you have the <ieeefp.h> header file. */
/* #undef HAVE_IEEEFP_H */

/* Define to 1 if you have the `inet_ntoa_r' function. */
/* #undef HAVE_INET_NTOA_R */

/* Define to 1 if you have the `inet_ntop' function. */
#define HAVE_INET_NTOP 1

/* Define to 1 if the system has the type `intptr_t'. */
#define HAVE_INTPTR_T 1

/* Define to 1 if you have the <inttypes.h> header file. */
#define HAVE_INTTYPES_H 1

/* Define to 1 if you have the <iostream> header file. */
#define HAVE_IOSTREAM 1

/* Define to 1 if you have the <iostream.h> header file. */
#define HAVE_IOSTREAM_H 1

/* Define to 1 if you have `ios(_base)::register_callback'. */
#define HAVE_IOS_REGISTER_CALLBACK 1

/* Define to 1 if you have the `lchown' function. */
/* #undef HAVE_LCHOWN */

/* Define to 1 if libbz2 is available. */
#define HAVE_LIBBZ2 1

/* Define to 1 if CRYPT is available, either in its own library or as part of
   the standard libraries. */
#define HAVE_LIBCRYPT 1

/* Define to 1 if DEMANGLE is available, either in its own library or as part
   of the standard libraries. */
/* #undef HAVE_LIBDEMANGLE */

/* Define to 1 if DL is available, either in its own library or as part of the
   standard libraries. */
#define HAVE_LIBDL 1

/* Define to 1 if libexpat is available. */
/* #undef HAVE_LIBEXPAT */

/* Define to 1 if FreeTDS libraries are available. */
/* #undef HAVE_LIBFTDS */

/* Define to 1 if you have libglut. */
/* #undef HAVE_LIBGLUT */

/* Define to 1 if libgnutls is available. */
/* #undef HAVE_LIBGNUTLS */

/* Define to 1 if ICONV is available, either in its own library or as part of
   the standard libraries. */
#define HAVE_LIBICONV 1

/* Define to 1 if libgssapi_krb5 is available. */
#define HAVE_LIBKRB5 1

/* Define to 1 if KSTAT is available, either in its own library or as part of
   the standard libraries. */
/* #undef HAVE_LIBKSTAT */

/* Define to 1 if liblzo2 is available. */
/* #undef HAVE_LIBLZO */

/* Define to 1 if liboechem is available. */
/* #undef HAVE_LIBOECHEM */

/* Define to 1 if you have libOSMesa. */
/* #undef HAVE_LIBOSMESA */

/* Define to 1 if libpcre is available. */
/* #undef HAVE_LIBPCRE */

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

/* Define to 1 if the NCBI SSS DB library is available. */
/* #undef HAVE_LIBSSSDB */

/* Define to 1 if the NCBI SSS UTILS library is available. */
/* #undef HAVE_LIBSSSUTILS */

/* Define to 1 if SYBASE DBLib is available. */
/* #undef HAVE_LIBSYBDB */

/* Define to 1 if libungif is available. */
/* #undef HAVE_LIBUNGIF */

/* Define to 1 if libXpm is available. */
/* #undef HAVE_LIBXPM */

/* Define to 1 if libz is available. */
#define HAVE_LIBZ 1

/* Define to 1 if you have the <limits> header file. */
#define HAVE_LIMITS 1

/* Define to 1 if you have the `localtime_r' function. */
#define HAVE_LOCALTIME_R 1

/* Define to 1 if local LBSM support is available. */
/* #undef HAVE_LOCAL_LBSM */

/* Define to 1 if you have the `lutimes' function. */
/* #undef HAVE_LUTIMES */

/* Define to 1 if you have the `madvise' function. */
#define HAVE_MADVISE 1

/* Define to 1 if you have the <malloc.h> header file. */
/* #undef HAVE_MALLOC_H */

/* Define to 1 if you have the <memory.h> header file. */
#define HAVE_MEMORY_H 1

/* Define to 1 if you have the `nanosleep' function. */
#define HAVE_NANOSLEEP 1

/* Define to 1 if the NCBI C toolkit is available. */
/* #undef HAVE_NCBI_C */

/* Define to 1 if the real version of ncbi_crypt support is available. */
/* #undef HAVE_NCBI_CRYPT */

/* Define to 1 if you have the <netdb.h> header file. */
#define HAVE_NETDB_H 1

/* Define to 1 if you have the <netinet/in.h> header file. */
#define HAVE_NETINET_IN_H 1

/* Define to 1 if you have the <netinet/tcp.h> header file. */
#define HAVE_NETINET_TCP_H 1

/* Define to 1 if `auto_ptr<T>' is missing or broken. */
/* #undef HAVE_NO_AUTO_PTR */

/* Define to 1 if `std::char_traits' is missing. */
/* #undef HAVE_NO_CHAR_TRAITS */

/* Define to 1 if new C++ streams lack `ios_base::'. */
/* #undef HAVE_NO_IOS_BASE */

/* Define to 1 if `min'/`max' templates are not implemented. */
/* #undef HAVE_NO_MINMAX_TEMPLATE */

/* Define to 1 if the ORBacus CORBA package is available. */
/* #undef HAVE_ORBACUS */

/* Define to 1 if you have the <paths.h> header file. */
#define HAVE_PATHS_H 1

/* Define to 1 if you have the <poll.h> header file. */
#define HAVE_POLL_H 1

/* Define to 1 if you have the `pthread_atfork' function. */
/* #undef HAVE_PTHREAD_ATFORK */

/* Define to 1 if pthread mutexes are available. */
#define HAVE_PTHREAD_MUTEX 1

/* Define to 1 if you have the `pthread_setconcurrency' function. */
#define HAVE_PTHREAD_SETCONCURRENCY 1

/* Define to 1 if Python 2.3 libraries are available. */
/* #undef HAVE_PYTHON23 */

/* Define to 1 if Python 2.4 libraries are available. */
/* #undef HAVE_PYTHON24 */

/* Define to 1 if Python 2.5 libraries are available. */
/* #undef HAVE_PYTHON25 */

/* Define to 1 if you have the `readpassphrase' function. */
#define HAVE_READPASSPHRASE 1

/* Define to 1 if your C compiler supports some variant of the C99 `restrict'
   keyword. */
#define HAVE_RESTRICT_C 1

/* Define to 1 if your C++ compiler supports some variant of the C99
   `restrict' keyword. */
#define HAVE_RESTRICT_CXX 1

/* Define to 1 if you have the `sched_yield' function. */
#define HAVE_SCHED_YIELD 1

/* Define to 1 if you have the `select' function. */
#define HAVE_SELECT 1

/* Define to 1 if you have `union semun'. */
#define HAVE_SEMUN 1

/* Define to 1 if you have the <signal.h> header file. */
#define HAVE_SIGNAL_H 1

/* Define to 1 if `sin_len' is a member of `struct sockaddr_in'. */
#define HAVE_SIN_LEN 1

/* Define to 1 if the system has the type `socklen_t'. */
#define HAVE_SOCKLEN_T 1

/* Define to 1 if you have the `SQLGetPrivateProfileString' function. */
/* #undef HAVE_SQLGETPRIVATEPROFILESTRING */

/* Define to 1 if the system has the type `SQLLEN'. */
#define HAVE_SQLLEN 1

/* Define to 1 if you have the `statfs' function. */
#define HAVE_STATFS 1

/* Define to 1 if you have the `statvfs' function. */
#define HAVE_STATVFS 1

/* Define to 1 if you have the <stdint.h> header file. */
#define HAVE_STDINT_H 1

/* Define to 1 if you have the <stdlib.h> header file. */
#define HAVE_STDLIB_H 1

/* Define to 1 if you have the `strcasecmp' function. */
#define HAVE_STRCASECMP 1

/* Define to 1 if strcasecmp treats letters as lowercase. */
#define HAVE_STRCASECMP_LC 1

/* Define to 1 if you have the `strdup' function. */
#define HAVE_STRDUP 1

/* Define to 1 if you have the <strings.h> header file. */
#define HAVE_STRINGS_H 1

/* Define to 1 if you have the <string.h> header file. */
#define HAVE_STRING_H 1

/* Define to 1 if you have the `strlcat' function. */
#define HAVE_STRLCAT 1

/* Define to 1 if you have the `strlcpy' function. */
#define HAVE_STRLCPY 1

/* Define to 1 if you have the `strndup' function. */
/* #undef HAVE_STRNDUP */

/* Define to 1 if you have the <strstream> header file. */
#define HAVE_STRSTREAM 1

/* Define to 1 if you have the <strstream.h> header file. */
/* #undef HAVE_STRSTREAM_H */

/* Define to 1 if you have the <strstrea.h> header file. */
/* #undef HAVE_STRSTREA_H */

/* Define to 1 if you have the `strtok_r' function. */
#define HAVE_STRTOK_R 1

/* Define to 1 if `tm_zone' is member of `struct tm'. */
#define HAVE_STRUCT_TM_TM_ZONE 1

/* Define to 1 if `__tm_zone' is member of `struct tm'. */
/* #undef HAVE_STRUCT_TM___TM_ZONE */

/* Define to 1 if SYBASE has reentrant libraries. */
/* #undef HAVE_SYBASE_REENTRANT */

/* Define to 1 if Linux-like 1-arg sysinfo exists. */
/* #undef HAVE_SYSINFO_1 */

/* Define to 1 if you have the `sysmp' function. */
/* #undef HAVE_SYSMP */

/* Define to 1 if you have SysV semaphores. */
#define HAVE_SYSV_SEMAPHORES 1

/* Define to 1 if you have the <sys/ioctl.h> header file. */
#define HAVE_SYS_IOCTL_H 1

/* Define to 1 if you have the <sys/mount.h> header file. */
#define HAVE_SYS_MOUNT_H 1

/* Define to 1 if you have the <sys/select.h> header file. */
#define HAVE_SYS_SELECT_H 1

/* Define to 1 if you have the <sys/socket.h> header file. */
#define HAVE_SYS_SOCKET_H 1

/* Define to 1 if you have the <sys/sockio.h> header file. */
#define HAVE_SYS_SOCKIO_H 1

/* Define to 1 if you have the <sys/statvfs.h> header file. */
//#define HAVE_SYS_STATVFS_H 1

/* Define to 1 if you have the <sys/stat.h> header file. */
#define HAVE_SYS_STAT_H 1

/* Define to 1 if you have the <sys/sysinfo.h> header file. */
/* #undef HAVE_SYS_SYSINFO_H */

/* Define to 1 if you have the <sys/time.h> header file. */
#define HAVE_SYS_TIME_H 1

/* Define to 1 if you have the <sys/types.h> header file. */
#define HAVE_SYS_TYPES_H 1

/* Define to 1 if you have the <sys/vfs.h> header file. */
/* #undef HAVE_SYS_VFS_H */

/* Define to 1 if you have the `timegm' function. */
#define HAVE_TIMEGM 1

/* Define to 1 if the system has the type `uintptr_t'. */
#define HAVE_UINTPTR_T 1

/* Define to 1 if your system permits reading integers from unaligned
   addresses. */
#define HAVE_UNALIGNED_READS 1

/* Define to 1 if you have the <unistd.h> header file. */
#define HAVE_UNISTD_H 1

/* Define to 1 if you have the `usleep' function. */
#define HAVE_USLEEP 1

/* Define to 1 if you have the `utimes' function. */
#define HAVE_UTIMES 1

/* Define to 1 if you have the `vasprintf' function. */
#define HAVE_VASPRINTF 1

/* Define to 1 if you have the `vprintf' function. */
#define HAVE_VPRINTF 1

/* Define to 1 if you have the `vsnprintf' function. */
#define HAVE_VSNPRINTF 1

/* Define to 1 if you have the <wchar.h> header file. */
#define HAVE_WCHAR_H 1

/* Define to 1 if you have the <windows.h> header file. */
/* #undef HAVE_WINDOWS_H */

/* Define to 1 if you have the `writev' function. */
#define HAVE_WRITEV 1

/* Define to 1 if the system has the type `wstring'. */
#define HAVE_WSTRING 1

/* Define to 1 if wxWidgets is available. */
/* #undef HAVE_WXWIDGETS */

/* Define as const if the declaration of iconv() needs const. */
#if MAC_OS_X_VERSION_MIN_REQUIRED >= 1050 /* MAC_OS_X_VERSION_10_5 */
#  define ICONV_CONST
#else
#  define ICONV_CONST const
#endif

/* Define to 0xffffffff if your operating system doesn't. */
/* #undef INADDR_NONE */

/* Define to 1 when building binaries for public release. */
/* #undef NCBI_BIN_RELEASE */

/* Define to whatever syntax, if any, your compiler supports for marking
   functions as deprecated. */
#define NCBI_DEPRECATED __attribute__((__deprecated__))


/* Define to whatever syntax your compiler supports for marking functions as
   to be inlined even if they might not otherwise be. */
#define NCBI_FORCEINLINE inline __attribute__((always_inline))

/* Rename DBLIB symbols in FTDS to avoid name clash with Sybase DBLIB. */
#define NCBI_FTDS_RENAME_SYBDB 1

/* If you have the `getpwuid_r' function, define to the number of arguments it
   takes (normally 4 or 5). */
#define NCBI_HAVE_GETPWUID_R 5

/* If you have the `readdir_r' function, define to the number of arguments it
   takes (normally 2 or 3). */
#define NCBI_HAVE_READDIR_R 3

/* Define to whatever syntax, if any, your compiler supports for marking
   functions that never return. */
#define NCBI_NORETURN __attribute__((__noreturn__))

/* Define to 1 if `string::compare()' is non-standard. */
/* #undef NCBI_OBSOLETE_STR_COMPARE */

/* Define to whatever syntax, if any, your C compiler supports for marking
   pointers as restricted in the C99 sense. */
#define NCBI_RESTRICT_C __restrict__

/* Define to whatever syntax, if any, your C++ compiler supports for marking
   pointers as restricted in the C99 sense. */
#define NCBI_RESTRICT_CXX __restrict__

/* Define to 1 if SQLColAttribute's last argument is an SQLLEN * */
/* #undef NCBI_SQLCOLATTRIBUTE_SQLLEN */

/* Define to whatever syntax your compiler supports for declaring thread-local
   variables, or leave undefined if it doesn't. */
/* #undef NCBI_TLS_VAR */

/* Define to 1 if building universal (multi-architecture) binaries. */
/* (Not necessarily the case, but we shouldn't hardcode anything
 *  PowerPC-specific either.) */
#define NCBI_UNIVERSAL_BUILD 1

/* Define to 1 if prototypes can use exception specifications. */
#define NCBI_USE_THROW_SPEC 1

/* Define to 1 if the BSD-style netdb interface is reentrant. */
/* #undef NETDB_REENTRANT */

/* Define as the return type of signal handlers (`int' or `void'). */
#define RETSIGTYPE void

/* Define to 1 if the `select' function updates its timeout when interrupted
   by a signal. */
/* #undef SELECT_UPDATES_TIMEOUT */

/* Define to 1 if the stack grows down. */
#define STACK_GROWS_DOWN 1

/* Define to 1 if the stack grows up. */
/* #undef STACK_GROWS_UP */

/* Define to 1 if you have the ANSI C header files. */
#define STDC_HEADERS 1

/* Define to 1 if you can safely include both <sys/time.h> and <time.h>. */
#define TIME_WITH_SYS_TIME 1

/* Define to 1 if the X Window System is missing or not being used. */
/* #undef X_DISPLAY_MISSING */

/* Enable GNU extensions on systems that have them.  */
#ifndef _GNU_SOURCE
# define _GNU_SOURCE 1
#endif

/* Define to empty if `const' does not conform to ANSI C. */
/* #undef const */

/* Define to `unsigned' if <sys/types.h> does not define. */
/* #undef size_t */

/*
 *  Site localization
 */
#include <common/config/ncbiconf_xcode_site.h>
