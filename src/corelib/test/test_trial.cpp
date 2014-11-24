#include <ncbi_pch.hpp>
#include <corelib/ncbicfg.h>
#include <corelib/error_codes.hpp>
#include <corelib/ncbienv.hpp>
#include <corelib/ncbimisc.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbi_system.hpp>
#include <corelib/ncbi_safe_static.hpp>

#if 1

#include <cctype>

#define NCBI_DEFINE_CTYPE_FUNC(ncbi_name, name)           \
inline int ncbi_name(int c) { return name(c); }           \
inline int ncbi_name(char c) { return name(Uchar(c)); }   \
inline int ncbi_name(unsigned char c) { return name(c); } \
template<class C> inline int ncbi_name(C c)               \
{ return See_the_standard_on_proper_argument_type_for_ctype_functions(c); }

NCBI_DEFINE_CTYPE_FUNC(NCBI_isalpha, isalpha)
NCBI_DEFINE_CTYPE_FUNC(NCBI_isalnum, isalnum)
NCBI_DEFINE_CTYPE_FUNC(NCBI_isascii, isascii)
//NCBI_DEFINE_CTYPE_FUNC(NCBI_isblank, isblank)
NCBI_DEFINE_CTYPE_FUNC(NCBI_iscntrl, iscntrl)
NCBI_DEFINE_CTYPE_FUNC(NCBI_isdigit, isdigit)
NCBI_DEFINE_CTYPE_FUNC(NCBI_isgraph, isgraph)
NCBI_DEFINE_CTYPE_FUNC(NCBI_islower, islower)
NCBI_DEFINE_CTYPE_FUNC(NCBI_isprint, isprint)
NCBI_DEFINE_CTYPE_FUNC(NCBI_ispunct, ispunct)
NCBI_DEFINE_CTYPE_FUNC(NCBI_isspace, isspace)
NCBI_DEFINE_CTYPE_FUNC(NCBI_isupper, isupper)
NCBI_DEFINE_CTYPE_FUNC(NCBI_isxdigit, isxdigit)
NCBI_DEFINE_CTYPE_FUNC(NCBI_toascii, toascii)
NCBI_DEFINE_CTYPE_FUNC(NCBI_tolower, tolower)
NCBI_DEFINE_CTYPE_FUNC(NCBI_toupper, toupper)

#undef NCBI_DEFINE_CTYPE_FUNC

BEGIN_STD_NAMESPACE;

using ::NCBI_isalpha;
using ::NCBI_isalnum;
using ::NCBI_isascii;
//using ::NCBI_isblank;
using ::NCBI_iscntrl;
using ::NCBI_isdigit;
using ::NCBI_isgraph;
using ::NCBI_islower;
using ::NCBI_isprint;
using ::NCBI_ispunct;
using ::NCBI_isspace;
using ::NCBI_isupper;
using ::NCBI_isxdigit;
using ::NCBI_toascii;
using ::NCBI_tolower;
using ::NCBI_toupper;

END_STD_NAMESPACE;

#ifdef isalpha
# undef  isalpha
#endif
#define isalpha NCBI_isalpha

#ifdef isalnum
# undef  isalnum
#endif
#define isalnum NCBI_isalnum

#ifdef isascii
# undef  isascii
#endif
#define isascii NCBI_isascii

/*
#ifdef isblank
# undef  isblank
#endif
#define isblank NCBI_isblank
*/

#ifdef iscntrl
# undef  iscntrl
#endif
#define iscntrl NCBI_iscntrl

#ifdef isdigit
# undef  isdigit
#endif
#define isdigit NCBI_isdigit

#ifdef isgraph
# undef  isgraph
#endif
#define isgraph NCBI_isgraph

#ifdef islower
# undef  islower
#endif
#define islower NCBI_islower

#ifdef isprint
# undef  isprint
#endif
#define isprint NCBI_isprint

#ifdef ispunct
# undef  ispunct
#endif
#define ispunct NCBI_ispunct

#ifdef isspace
# undef  isspace
#endif
#define isspace NCBI_isspace

#ifdef isupper
# undef  isupper
#endif
#define isupper NCBI_isupper

#ifdef isxdigit
# undef  isxdigit
#endif
#define isxdigit NCBI_isxdigit

#ifdef toascii
# undef  toascii
#endif
#define toascii NCBI_toascii

#ifdef tolower
# undef  tolower
#endif
#define tolower NCBI_tolower

#ifdef toupper
# undef  toupper
#endif
#define toupper NCBI_toupper

#endif

#ifndef BOOST_TEST_NO_LIB
#  define BOOST_TEST_NO_LIB
#endif
#define BOOST_TEST_NO_MAIN
#if defined(__FreeBSD_cc_version)  &&  !defined(__FreeBSD_version)
#  define __FreeBSD_version __FreeBSD_cc_version
#endif
#include <corelib/test_boost.hpp>

#include <boost/preprocessor/cat.hpp>
#include <boost/preprocessor/tuple/elem.hpp>
#include <boost/preprocessor/tuple/eat.hpp>

// On Mac OS X, some corelib headers end up pulling in system headers
// that #define nil as a macro that ultimately expands to __null,
// breaking Boost's internal use of a struct nil.
#ifdef nil
#  undef nil
#endif
#ifdef NCBI_COMPILER_MSVC
#  pragma warning(push)
// 'class' : class has virtual functions, but destructor is not virtual
#  pragma warning(disable: 4265)
// 'operator/operation' : unsafe conversion from 'type of expression' to 'type required'
#  pragma warning(disable: 4191)
#endif

#include <boost/test/included/unit_test.hpp>
#include <boost/test/results_collector.hpp>
#include <boost/test/results_reporter.hpp>
#include <boost/test/test_observer.hpp>
#include <boost/test/unit_test_log.hpp>
#include <boost/test/unit_test_log_formatter.hpp>
#include <boost/test/output/plain_report_formatter.hpp>
#include <boost/test/output/xml_report_formatter.hpp>
#include <boost/test/output/compiler_log_formatter.hpp>
#include <boost/test/output/xml_log_formatter.hpp>
#include <boost/test/utils/xml_printer.hpp>
#include <boost/test/detail/global_typedef.hpp>
#include <boost/test/detail/unit_test_parameters.hpp>
#include <boost/test/debug.hpp>


#include <common/test_assert.h>  /* This header must go last */

USING_NCBI_SCOPE;

class CTestApplication : public CNcbiApplication
{
public:
    virtual int Run(void);
};


int CTestApplication::Run(void)
{
    NcbiCout << "isupper(int('A')) = " << isupper(int('A')) << NcbiEndl;
    NcbiCout << "isupper(Uchar('A')) = " << isupper(Uchar('A')) << NcbiEndl;
    NcbiCout << "isupper('A') = " << isupper('A') << NcbiEndl;
    _ASSERT(isupper(int('A')));
    _ASSERT(isupper(Uchar('A')));
    _ASSERT(isupper('A'));
    _ASSERT(std::isupper(int('A')));
    _ASSERT(std::isupper(Uchar('A')));
    _ASSERT(std::isupper('A'));
    _ASSERT(::isupper(int('A')));
    _ASSERT(::isupper(Uchar('A')));
    _ASSERT(::isupper('A'));
    _ASSERT(toupper('A') == 'A');
    _ASSERT(std::toupper('A') == 'A');
    _ASSERT(::toupper('A') == 'A');
    //_ASSERT(::toupper('A'=='A'));
    NcbiCout << "Passed" << NcbiEndl;
    return 0;
}


/////////////////////////////////////////////////////////////////////////////
//  MAIN

int main(int argc, const char* argv[]) 
{
    return CTestApplication().AppMain(argc, argv);
}
