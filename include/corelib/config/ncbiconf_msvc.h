/* $Id$
 * By Denis Vakatov, NCBI (vakatov@ncbi.nlm.nih.gov)
 *
 * MS-Win 32/64, MSVC++ 6.0/.NET
 *
 * NOTE:  Unlike its UNIX counterpart, this configuration header
 *        is manually maintained in order to keep it in-sync with the
 *        "configure"-generated configuration headers.
 */


/*
 *  Standard Toolkit/MSVC properties
 */

#define NCBI_CXX_TOOLKIT  1

#define NCBI_OS        "MSWIN"
#define NCBI_OS_MSWIN  1

#define NCBI_COMPILER_MSVC    1

#define HOST         "i386-pc-win32"
#define HOST_CPU     "i386"
#define HOST_VENDOR  "pc"
#define HOST_OS      "win32"

#define HAVE_STRDUP                1
#define HAVE_STRICMP               1
#define NCBI_USE_THROW_SPEC        1
#define HAVE_NO_AUTO_PTR           1
#define HAVE_NO_MINMAX_TEMPLATE    1
#define STACK_GROWS_DOWN           1
#define HAVE_IOS_REGISTER_CALLBACK 1
#define HAVE_IOS_XALLOC            1

#define SIZEOF___INT64      8
#define SIZEOF_CHAR         1
#define SIZEOF_DOUBLE       8
#define SIZEOF_INT          4
#define SIZEOF_LONG         4
#define SIZEOF_LONG_DOUBLE  8
#define SIZEOF_LONG_LONG    0
#define SIZEOF_SHORT        2
#define SIZEOF_SIZE_T       4
#define SIZEOF_VOIDP        4
#define NCBI_PLATFORM_BITS  32

#define STDC_HEADERS  1

#define HAVE_FSTREAM     1
#define HAVE_FSTREAM_H   1
#define HAVE_IOSTREAM    1
#define HAVE_IOSTREAM_H  1
#define HAVE_LIMITS      1
#define HAVE_STRSTREA_H  1
#define HAVE_STRSTREAM   1
#define HAVE_SYS_STAT_H  1
#define HAVE_SYS_TYPES_H 1
#define HAVE_VSNPRINTF   1
#define vsnprintf        _vsnprintf
#define HAVE_WINDOWS_H   1
#define HAVE_WSTRING     1

#ifdef _WIN64
typedef __int64 ssize_t;
#else
typedef   int   ssize_t;
#endif


/*
 *  Site localization
 */


/* PROJECT_TREE_BUILDER-generated site localization
 */
#include "ncbiconf_msvc_site.h"


/* Default site localization (for MSVC6)
 */
#if (_MSC_VER < 1300)
#  if !defined(CORELIB_CONFIG___NCBICONF_MSVC_SITE__H)
#    define HAVE_OPENGL      1
#    define HAVE_FLTK        1

#    define HAVE_ODBC        1
#    define HAVE_ODBCSS_H    1

/* optional parameters */
/*#    define HAVE_PUBSEQ_OS   1*/
/*#    define HAVE_LIBZ        1*/
/*#    define HAVE_LIBBZ2      1*/
#    define HAVE_LIBJPEG     1
#    define HAVE_LIBTIFF     1
#    define HAVE_LIBPNG      1
#    define HAVE_LIBGIF      1
#  endif
#endif
