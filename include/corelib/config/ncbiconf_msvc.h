/* //e/ncbi_cpp/msvc/inc/ncbiconf.h.  Generated automatically by configure.  */
/* config.h.in.  Generated automatically from configure.in by autoheader.  */

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
#define HOST "i386-pc-cygwin32"
#define HOST_CPU "i386"
#define HOST_VENDOR "pc"
#define HOST_OS "cygwin32"

/* Define if C++ namespaces are supported */
#define HAVE_NAMESPACE 1

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
#define SIZEOF_LONG_LONG 0

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
/* #undef HAVE_UNISTD_H */

/* Define if you have the <windows.h> header file.  */
#define HAVE_WINDOWS_H 1
