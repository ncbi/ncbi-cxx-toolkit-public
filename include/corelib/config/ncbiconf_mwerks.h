/* $Id$
 * By Jonathan Kans, NCBI (kans@ncbi.nlm.nih.gov)
 *
 * NOTE:  Unlike its UNIX and NT counterparts, this configuration header
 *        is manually maintained in order to keep it in-sync with the
 *        "configure"-generated configuration headers.
 */

/* Hard-coded;  must always be defined in this(pro-Mac) header */
#define NCBI_OS_MAC 1

/* Define if type char is unsigned and you are not using gcc.  */
#ifndef __CHAR_UNSIGNED__
/* #undef __CHAR_UNSIGNED__ */
#endif

/* Define to empty if the keyword does not work.  */
/* #undef const */

/* Define to `unsigned' if <sys/types.h> doesn't define.  */
/* #undef size_t */

/* Define if you have the ANSI C header files.  */
#define STDC_HEADERS 1

/* Define if your processor stores words with the most significant
   byte first (like Motorola and SPARC, unlike Intel and VAX).  */
/* #undef WORDS_BIGENDIAN */

/* Host info */
#define HOST "PowerPC-Apple-MacOS"
#define HOST_CPU "PowerPC"
#define HOST_VENDOR "Apple"
#define HOST_OS "MacOS"

/* Define if C++ namespaces are not supported */
/* #undef HAVE_NO_NAMESPACE */

/* Define if C++ namespace std:: is used */
/* #undef HAVE_NO_STD */

/* Does not give enough support to the in-class template functions */
/* #undef NO_INCLASS_TMPL */

/* Can use exception specifications("throw(...)" after func. proto) */
#define NCBI_USE_THROW_SPEC 1

/* Non-standard basic_string::compare() -- most probably, from <bastring> */
/* #undef NCBI_OBSOLETE_STR_COMPARE */

/* "auto_ptr" template class is not implemented in <memory> */
#define HAVE_NO_AUTO_PTR 1

/* Fast-CGI library is available */
/* #undef HAVE_LIBFASTCGI */

/* New C++ streams dont have ios_base:: */
/* #undef HAVE_NO_IOS_BASE */

/* This is good for the GNU C/C++ compiler on Solaris */ 
/* #undef __EXTENSIONS__ */

/* This is good for the EGCS C/C++ compiler on Linux(e.g. proto for putenv) */
/* #undef _SVID_SOURCE */

/* The number of bytes in a __int64.  */
#define SIZEOF___INT64 8

/* The number of bytes in a char.  */
#define SIZEOF_CHAR 1

/* The number of bytes in a double.  */
#define SIZEOF_DOUBLE 8

/* The number of bytes in a int.  */
#define SIZEOF_INT 4

/* The number of bytes in a long.  */
#define SIZEOF_LONG 4

/* The number of bytes in a long double.  */
#define SIZEOF_LONG_DOUBLE 8

/* The number of bytes in a long long.  */
#define SIZEOF_LONG_LONG 8

/* The number of bytes in a short.  */
#define SIZEOF_SHORT 2

/* The number of bytes in a void*.  */
#define SIZEOF_VOIDP 4

/* Define if you have the <fstream> header file.  */
#define HAVE_FSTREAM 1

/* Define if you have the <fstream.h> header file.  */
#define HAVE_FSTREAM_H 1

/* Define if you have the <iostream> header file.  */
#define HAVE_IOSTREAM 1

/* Define if you have the <iostream.h> header file.  */
#define HAVE_IOSTREAM_H 1

/* Define if you have the <limits> header file.  */
#define HAVE_LIMITS 1

/* Define if you have the <string> header file.  */
#define HAVE_STRING 1

/* Define if you have the <strstrea.h> header file.  */
#define HAVE_STRSTREA_H 1

/* Define if you have the <strstream> header file.  */
#define HAVE_STRSTREAM 1

/* Define if you have the <strstream.h> header file.  */
/* #undef HAVE_STRSTREAM_H */

/* Define if you have the <unistd.h> header file.  */
#define HAVE_UNISTD_H 1

/* Define if you have the <windows.h> header file.  */
/* #undef HAVE_WINDOWS_H */
