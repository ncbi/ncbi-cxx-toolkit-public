/* src/config.h.  Normally generated from config.h.in by configure.  */
/* src/config.h.in.  Generated from configure.ac by autoheader.  */

/* PCRE2 is written in Standard C, but there are a few non-standard things it
can cope with, allowing it to run on SunOS4 and other "close to standard"
systems.

In environments that support the GNU autotools, config.h.in is converted into
config.h by the "configure" script. In environments that use CMake,
config-cmake.in is converted into config.h. If you are going to build PCRE2 "by
hand" without using "configure" or CMake, you should copy the distributed
config.h.generic to config.h, and edit the macro definitions to be the way you
need them. You must then add -DHAVE_CONFIG_H to all of your compile commands,
so that config.h is included at the start of every source.

Alternatively, you can avoid editing by using -D on the compiler command line
to set the macro values. In this case, you do not have to set -DHAVE_CONFIG_H,
but if you do, default values will be taken from config.h for non-boolean
macros that are not defined on the command line.

Boolean macros such as HAVE_STDLIB_H and SUPPORT_PCRE2_8 should either be
defined (conventionally to 1) for TRUE, and not defined at all for FALSE. All
such macros are listed as a commented #undef in config.h.generic. Macros such
as MATCH_LIMIT, whose actual value is relevant, have defaults defined, but are
surrounded by #ifndef/#endif lines so that the value can be overridden by -D.

PCRE2 uses memmove() if HAVE_MEMMOVE is defined; otherwise it uses bcopy() if
HAVE_BCOPY is defined. If your system has neither bcopy() nor memmove(), make
sure both macros are undefined; an emulation function will then be used. */

/* The C++ Toolkit's own configuration header makes a solid starting point. */
#include <ncbiconf.h>

/* By default, the \R escape sequence matches any Unicode line ending
   character or sequence of characters. If BSR_ANYCRLF is defined (to any
   value), this is changed so that backslash-R matches only CR, LF, or CRLF.
   The build-time default can be overridden by the user of PCRE2 at runtime.
   */
/* #undef BSR_ANYCRLF */

/* Define to any value to disable the use of the z and t modifiers in
   formatting settings such as %zu or %td (this is rarely needed). */
/* #undef DISABLE_PERCENT_ZT */

/* Out of the question: EBCDIC, EBCDIC_NL25 */

/* Define this if your compiler supports __attribute__((uninitialized)) */
#if NCBI_HAS_C_ATTRIBUTE(uninitialized)
#  define HAVE_ATTRIBUTE_UNINITIALIZED 1
#endif

/* Handled via ncbiconf.h: HAVE_BCOPY, HAVE_BZLIB_H, HAVE_DIRENT_H,
   HAVE_DLFCN_H */

/* Left off for simplicity (and unused by library): HAVE_*READLINE*_H */

/* Handled via ncbiconf.h: HAVE_INTTYPES_H, HAVE_LIMITS_H, HAVE_MEMFD_CREATE,
   HAVE_MEMMOVE */

/* Out of the question: HAVE_MINIX_CONFIG_H */

/* Handled via ncbiconf.h: HAVE_MKOSTEMP */

#ifdef HAVE_PTHREAD_MUTEX
#  define HAVE_PTHREAD 1
#endif

/* Unused by library or test: PTHREAD_PRIO_INHERIT. */

/* Handled via ncbiconf.h: HAVE_REALPATH, HAVE_SECURE_GETENV, HAVE_STDINT_H,
   HAVE_STDIO_H, HAVE_STDLIB_H, HAVE_STRERROR, HAVE_STRING(S)_H,
   HAVE_SYS_STAT_H, HAVE_SYS_TYPES_H, HAVE_SYS_WAIT_H, HAVE_UNISTD_H */

#ifdef HAVE_ATTRIBUTE_VISIBILITY_DEFAULT
#  define HAVE_VISIBILITY 1
#endif

/* Handled via ncbiconf.h: HAVE_WCHAR_H, HAVE_WINDOWS_H */

#ifdef HAVE_LIBZ
#  define HAVE_ZLIB_H 1
#endif

/* This limits the amount of memory that may be used while matching a pattern.
   It applies to both pcre2_match() and pcre2_dfa_match(). It does not apply
   to JIT matching. The value is in kibibytes (units of 1024 bytes). */
#ifndef HEAP_LIMIT
#define HEAP_LIMIT 20000000
#endif

/* The value of LINK_SIZE determines the number of bytes used to store links
   as offsets within the compiled regex. The default is 2, which allows for
   compiled patterns up to 65535 code units long. This covers the vast
   majority of cases. However, PCRE2 can also be compiled to use 3 or 4 bytes
   instead. This allows for longer patterns in extreme cases. */
#ifndef LINK_SIZE
#define LINK_SIZE 2
#endif

/* Define to the sub-directory where libtool stores uninstalled libraries. */
/* This is ignored unless you are using libtool. */
#ifndef LT_OBJDIR
#define LT_OBJDIR ".libs/"
#endif

/* The value of MATCH_LIMIT determines the default number of times the
   pcre2_match() function can record a backtrack position during a single
   matching attempt. The value is also used to limit a loop counter in
   pcre2_dfa_match(). There is a runtime interface for setting a different
   limit. The limit exists in order to catch runaway regular expressions that
   take forever to determine that they do not match. The default is set very
   large so that it does not accidentally catch legitimate cases. */
#ifndef MATCH_LIMIT
#define MATCH_LIMIT 10000000
#endif

/* The above limit applies to all backtracks, whether or not they are nested.
   In some environments it is desirable to limit the nesting of backtracking
   (that is, the depth of tree that is searched) more strictly, in order to
   restrict the maximum amount of heap memory that is used. The value of
   MATCH_LIMIT_DEPTH provides this facility. To have any useful effect, it
   must be less than the value of MATCH_LIMIT. The default is to use the same
   value as MATCH_LIMIT. There is a runtime method for setting a different
   limit. In the case of pcre2_dfa_match(), this limit controls the depth of
   the internal nested function calls that are used for pattern recursions,
   lookarounds, and atomic groups. */
#ifndef MATCH_LIMIT_DEPTH
#define MATCH_LIMIT_DEPTH MATCH_LIMIT
#endif

/* This limit is parameterized just in case anybody ever wants to change it.
   Care must be taken if it is increased, because it guards against integer
   overflow caused by enormously large patterns. */
#ifndef MAX_NAME_COUNT
#define MAX_NAME_COUNT 10000
#endif

/* This limit is parameterized just in case anybody ever wants to change it.
   Care must be taken if it is increased, because it guards against integer
   overflow caused by enormously large patterns. */
#ifndef MAX_NAME_SIZE
#define MAX_NAME_SIZE 128
#endif

/* The value of MAX_VARLOOKBEHIND specifies the default maximum length, in
   characters, for a variable-length lookbehind assertion. */
#ifndef MAX_VARLOOKBEHIND
#define MAX_VARLOOKBEHIND 255
#endif

/* Defining NEVER_BACKSLASH_C locks out the use of \C in all patterns. */
/* #undef NEVER_BACKSLASH_C */

/* The value of NEWLINE_DEFAULT determines the default newline character
   sequence. PCRE2 client programs can override this by selecting other values
   at run time. The valid values are 1 (CR), 2 (LF), 3 (CRLF), 4 (ANY), 5
   (ANYCRLF), and 6 (NUL). */
#ifndef NEWLINE_DEFAULT
#define NEWLINE_DEFAULT 2
#endif

/* Name of package */
#define PACKAGE "pcre2"

/* Define to the address where bug reports for this package should be sent. */
#define PACKAGE_BUGREPORT ""

/* Define to the full name of this package. */
#define PACKAGE_NAME "PCRE2"

/* Define to the full name and version of this package. */
#define PACKAGE_STRING "PCRE2 10.44"

/* Define to the one symbol short name of this package. */
#define PACKAGE_TARNAME "pcre2"

/* Define to the home page for this package. */
#define PACKAGE_URL ""

/* Define to the version of this package. */
#define PACKAGE_VERSION "10.44"

/* The value of PARENS_NEST_LIMIT specifies the maximum depth of nested
   parentheses (of any kind) in a pattern. This limits the amount of system
   stack that is used while compiling a pattern. */
#ifndef PARENS_NEST_LIMIT
#define PARENS_NEST_LIMIT 250
#endif

/* The value of PCRE2GREP_BUFSIZE is the starting size of the buffer used by
   pcre2grep to hold parts of the file it is searching. The buffer will be
   expanded up to PCRE2GREP_MAX_BUFSIZE if necessary, for files containing
   very long lines. The actual amount of memory used by pcre2grep is three
   times this number, because it allows for the buffering of "before" and
   "after" lines. */
#ifndef PCRE2GREP_BUFSIZE
#define PCRE2GREP_BUFSIZE 20480
#endif

/* The value of PCRE2GREP_MAX_BUFSIZE specifies the maximum size of the buffer
   used by pcre2grep to hold parts of the file it is searching. The actual
   amount of memory used by pcre2grep is three times this number, because it
   allows for the buffering of "before" and "after" lines. */
#ifndef PCRE2GREP_MAX_BUFSIZE
#define PCRE2GREP_MAX_BUFSIZE 1048576
#endif

/* Define to any value to include debugging code. */
/* #undef PCRE2_DEBUG */

/* to make a symbol visible */
#define PCRE2_EXPORT

/* If you are compiling for a system other than a Unix-like system or
   Win32, and it needs some magic to be inserted before the definition
   of a function that is exported by the library, define this macro to
   contain the relevant magic. If you do not define this macro, a suitable
   __declspec value is used for Windows systems; in other environments
   a compiler relevant "extern" is used with any "visibility" related
   attributes from PCRE2_EXPORT included.
   This macro apears at the start of every exported function that is part
   of the external API. It does not appear on functions that are "external"
   in the C sense, but which are internal to the library. */
/* #undef PCRE2_EXP_DEFN */

/* Define to any value if linking statically (TODO: make nice with Libtool) */
/* #undef PCRE2_STATIC */

/* Define to necessary symbol if this constant uses a non-standard name on
   your system. */
/* #undef PTHREAD_CREATE_JOINABLE */

/* Define to any non-zero number to enable support for SELinux compatible
   executable memory allocator in JIT. Note that this will have no effect
   unless SUPPORT_JIT is also defined. */
/* #undef SLJIT_PROT_EXECUTABLE_ALLOCATOR */

/* Handled via ncbiconf.h: STDC_HEADERS */

/* Define to any value to enable differential fuzzing support. */
/* #undef SUPPORT_DIFF_FUZZ */

/* Define to any value to enable support for Just-In-Time compiling. */
#define SUPPORT_JIT 1

/* Define to any value to allow pcre2grep to be linked with libbz2, so that it
   is able to handle .bz2 files. */
#ifdef HAVE_LIBBZ2
#  define SUPPORT_LIBBZ2 1
#endif

/* Define to any value to allow pcre2test to be linked with libedit. */
/* #undef SUPPORT_LIBEDIT */

/* Define to any value to allow pcre2test to be linked with libreadline. */
/* #undef SUPPORT_LIBREADLINE */

/* Define to any value to allow pcre2grep to be linked with libz, so that it
   is able to handle .gz files. */
#ifdef HAVE_LIBZ
#  define SUPPORT_LIBZ 1
#endif

/* Define to any value to enable callout script support in pcre2grep. */
#define SUPPORT_PCRE2GREP_CALLOUT 1

/* Define to any value to enable fork support in pcre2grep callout scripts.
   This will have no effect unless SUPPORT_PCRE2GREP_CALLOUT is also defined.
   */
#define SUPPORT_PCRE2GREP_CALLOUT_FORK 1

/* Define to any value to enable JIT support in pcre2grep. Note that this will
   have no effect unless SUPPORT_JIT is also defined. */
#define SUPPORT_PCRE2GREP_JIT 1

/* Define to any value to enable the 16 bit PCRE2 library. */
/* #undef SUPPORT_PCRE2_16 */

/* Define to any value to enable the 32 bit PCRE2 library. */
/* #undef SUPPORT_PCRE2_32 */

/* Define to any value to enable the 8 bit PCRE2 library. */
#define SUPPORT_PCRE2_8 1

/* Define to any value to enable support for Unicode and UTF encoding. This
   will work even in an EBCDIC environment, but it is incompatible with the
   EBCDIC macro. That is, PCRE2 can support *either* EBCDIC code *or*
   ASCII/Unicode, but not both at once. */
#define SUPPORT_UNICODE 1

/* Define to any value for valgrind support to find invalid memory reads. */
#ifdef HAVE_VALGRIND_MEMCHECK_H
#  define SUPPORT_VALGRIND 1
#endif

/* Enable extensions on AIX, Interix, z/OS.  */
#ifndef _ALL_SOURCE
# define _ALL_SOURCE 1
#endif
/* Enable general extensions on macOS.  */
#ifndef _DARWIN_C_SOURCE
# define _DARWIN_C_SOURCE 1
#endif
/* Enable general extensions on Solaris.  */
#ifndef __EXTENSIONS__
# define __EXTENSIONS__ 1
#endif
/* Enable GNU extensions on systems that have them.  */
#ifndef _GNU_SOURCE
# define _GNU_SOURCE 1
#endif
/* Enable X/Open compliant socket functions that do not require linking
   with -lxnet on HP-UX 11.11.  */
#ifndef _HPUX_ALT_XOPEN_SOCKET_API
# define _HPUX_ALT_XOPEN_SOCKET_API 1
#endif
/* Identify the host operating system as Minix.
   This macro does not affect the system headers' behavior.
   A future release of Autoconf may stop defining this macro.  */
#ifndef _MINIX
/* # undef _MINIX */
#endif
/* Enable general extensions on NetBSD.
   Enable NetBSD compatibility extensions on Minix.  */
#ifndef _NETBSD_SOURCE
# define _NETBSD_SOURCE 1
#endif
/* Enable OpenBSD compatibility extensions on NetBSD.
   Oddly enough, this does nothing on OpenBSD.  */
#ifndef _OPENBSD_SOURCE
# define _OPENBSD_SOURCE 1
#endif
/* Define to 1 if needed for POSIX-compatible behavior.  */
#ifndef _POSIX_SOURCE
/* # undef _POSIX_SOURCE */
#endif
/* Define to 2 if needed for POSIX-compatible behavior.  */
#ifndef _POSIX_1_SOURCE
/* # undef _POSIX_1_SOURCE */
#endif
/* Enable POSIX-compatible threading on Solaris.  */
#ifndef _POSIX_PTHREAD_SEMANTICS
# define _POSIX_PTHREAD_SEMANTICS 1
#endif
/* Enable extensions specified by ISO/IEC TS 18661-5:2014.  */
#ifndef __STDC_WANT_IEC_60559_ATTRIBS_EXT__
# define __STDC_WANT_IEC_60559_ATTRIBS_EXT__ 1
#endif
/* Enable extensions specified by ISO/IEC TS 18661-1:2014.  */
#ifndef __STDC_WANT_IEC_60559_BFP_EXT__
# define __STDC_WANT_IEC_60559_BFP_EXT__ 1
#endif
/* Enable extensions specified by ISO/IEC TS 18661-2:2015.  */
#ifndef __STDC_WANT_IEC_60559_DFP_EXT__
# define __STDC_WANT_IEC_60559_DFP_EXT__ 1
#endif
/* Enable extensions specified by C23 Annex F.  */
#ifndef __STDC_WANT_IEC_60559_EXT__
# define __STDC_WANT_IEC_60559_EXT__ 1
#endif
/* Enable extensions specified by ISO/IEC TS 18661-4:2015.  */
#ifndef __STDC_WANT_IEC_60559_FUNCS_EXT__
# define __STDC_WANT_IEC_60559_FUNCS_EXT__ 1
#endif
/* Enable extensions specified by C23 Annex H and ISO/IEC TS 18661-3:2015.  */
#ifndef __STDC_WANT_IEC_60559_TYPES_EXT__
# define __STDC_WANT_IEC_60559_TYPES_EXT__ 1
#endif
/* Enable extensions specified by ISO/IEC TR 24731-2:2010.  */
#ifndef __STDC_WANT_LIB_EXT2__
# define __STDC_WANT_LIB_EXT2__ 1
#endif
/* Enable extensions specified by ISO/IEC 24747:2009.  */
#ifndef __STDC_WANT_MATH_SPEC_FUNCS__
# define __STDC_WANT_MATH_SPEC_FUNCS__ 1
#endif
/* Enable extensions on HP NonStop.  */
#ifndef _TANDEM_SOURCE
# define _TANDEM_SOURCE 1
#endif
/* Enable X/Open extensions.  Define to 500 only if necessary
   to make mbstate_t available.  */
#ifndef _XOPEN_SOURCE
/* # undef _XOPEN_SOURCE */
#endif

/* Version number of package */
#define VERSION "10.44"

/* Number of bits in a file offset, on hosts where this is settable. */
/* #undef _FILE_OFFSET_BITS */

/* Define to 1 on platforms where this makes off_t a 64-bit type. */
/* #undef _LARGE_FILES */

/* Number of bits in time_t, on hosts where this is settable. */
/* #undef _TIME_BITS */

/* Define to 1 on platforms where this makes time_t a 64-bit type. */
/* #undef __MINGW_USE_VC2005_COMPAT */

/* Define to empty if 'const' does not conform to ANSI C. */
/* #undef const */

/* Define to the type of a signed integer type of width exactly 64 bits if
   such a type exists and the standard includes do not define it. */
/* #undef int64_t */

/* Define to 'unsigned int' if <sys/types.h> does not define. */
/* #undef size_t */
