/* It is the NCBI C++ Toolkit */
#undef NCBI_CXX_TOOLKIT

/* Host info */
#undef HOST
#undef HOST_CPU
#undef HOST_VENDOR
#undef HOST_OS

/* Platform info */
#undef NCBI_OS
#undef NCBI_OS_UNIX
#undef NCBI_OS_MSWIN
#undef NCBI_OS_MAC

/* Compiler name and version */
#undef NCBI_COMPILER_GCC
#undef NCBI_COMPILER_KCC
#undef NCBI_COMPILER_ICC
#undef NCBI_COMPILER_WORKSHOP
#undef NCBI_COMPILER_MIPSPRO
#undef NCBI_COMPILER_MSVC
#undef NCBI_COMPILER_UNKNOWN
#undef NCBI_COMPILER_VERSION

/* <sys/sockio.h> */
#undef HAVE_SYS_SOCKIO_H

/* typedef socklen_t */
#undef HAVE_SOCKLEN_T

/* gethostbyname_r() */
#undef HAVE_GETHOSTBYNAME_R

/* gethostbyaddr_r() */
#undef HAVE_GETHOSTBYADDR_R

/* localtime_r() */
#undef HAVE_LOCALTIME_R

/* pthread_setconcurrency() */
#undef HAVE_PTHREAD_SETCONCURRENCY

/* strdup() */
#undef HAVE_STRDUP

/* sysinfo() -- Linux-like, with 1 arg */
#undef HAVE_SYSINFO_1

/* sysmp() */
#undef HAVE_SYSMP

/* strcasecmp() */
#undef HAVE_STRCASECMP

/* Microsoft C++ specific */
#undef SIZEOF___INT64

/* Does not give enough support to the in-class template functions */
#undef NO_INCLASS_TMPL

/* Can use exception specifications("throw(...)" after func. proto) */
#undef NCBI_USE_THROW_SPEC

/* Non-standard basic_string::compare() -- most probably, from <bastring> */
#undef NCBI_OBSOLETE_STR_COMPARE

/* "auto_ptr" template class is not implemented in <memory> */
#undef HAVE_NO_AUTO_PTR

/* "min"/"max" templates are not implemented in <algorithm> */
#undef HAVE_NO_MINMAX_TEMPLATE

/* Stack direction */
#undef STACK_GROWS_UP
#undef STACK_GROWS_DOWN

/* SYBASE libraries are available */
#undef HAVE_LIBSYBASE

/* KSTAT library is available */
#undef HAVE_LIBKSTAT

/* RPCSVC library is available */
#undef HAVE_LIBRPCSVC

/* NCBI C Toolkit libs are available */
#undef HAVE_NCBI_C

/* NCBI C Toolkit libs are available */
#undef HAVE_WXWINDOWS

/* Fast-CGI library is available */
#undef HAVE_LIBFASTCGI

/* NCBI SSS DB library is available */
#undef HAVE_LIBSSSDB

/* NCBI SSS UTILS library is available */
#undef HAVE_LIBSSSUTILS

/* NCBI GEO DB library is available */
#undef HAVE_LIBGEODB

/* SP library is available */
#undef HAVE_LIBSP

/* NCBI PubMed libraries are available */
#undef HAVE_LIBPUBMED

/* New C++ streams do not have ios_base:: */
#undef HAVE_NO_IOS_BASE

/* Have union semun */
#undef HAVE_SEMUN

/* There is no "std::char_traits" type defined */
#undef HAVE_NO_CHAR_TRAITS

/* Architecture size */
#undef NCBI_PLATFORM_BITS
