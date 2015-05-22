#define NCBI_TEST_APPLICATION
#include <ncbi_pch.hpp>
#include <corelib/ncbicfg.h>
#include <corelib/error_codes.hpp>
#include <corelib/ncbienv.hpp>
#include <corelib/ncbimisc.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbi_system.hpp>
#include <corelib/ncbi_safe_static.hpp>

#if !(defined(NCBI_STRICT_CTYPE_ARGS) && (defined(isalpha) || defined(NCBI_STRICT_CTYPE_ARGS_ACTIVE)))

#include <cctype>

#define NCBI_DEFINE_CTYPE_FUNC(name)                                    \
    inline int name(Uchar c) { return name(int(c)); }                   \
    inline int name(char c) { return name(Uchar(c)); }                  \
    template<class C> inline int name(C c)                              \
    { return See_the_standard_on_proper_argument_type_for_ctype_functions(c); }

NCBI_DEFINE_CTYPE_FUNC(isalpha)
NCBI_DEFINE_CTYPE_FUNC(isalnum)
NCBI_DEFINE_CTYPE_FUNC(iscntrl)
NCBI_DEFINE_CTYPE_FUNC(isdigit)
NCBI_DEFINE_CTYPE_FUNC(isgraph)
NCBI_DEFINE_CTYPE_FUNC(islower)
NCBI_DEFINE_CTYPE_FUNC(isprint)
NCBI_DEFINE_CTYPE_FUNC(ispunct)
NCBI_DEFINE_CTYPE_FUNC(isspace)
NCBI_DEFINE_CTYPE_FUNC(isupper)
NCBI_DEFINE_CTYPE_FUNC(isxdigit)
NCBI_DEFINE_CTYPE_FUNC(tolower)
NCBI_DEFINE_CTYPE_FUNC(toupper)
//NCBI_DEFINE_CTYPE_FUNC(isblank)
//NCBI_DEFINE_CTYPE_FUNC(isascii)
//NCBI_DEFINE_CTYPE_FUNC(toascii)

#undef NCBI_DEFINE_CTYPE_FUNC

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
#if 0 // should fail
    _ASSERT(::toupper('A'=='A'));
#endif
    TGi gi = 2;
    gi = 3;
    gi = INVALID_GI;
    gi = GI_CONST(192377853);
#ifdef NCBI_INT8_GI
    gi = GI_CONST(192377853343);
#endif
    NcbiCout << "GI = " << gi << NcbiEndl;
    NcbiCout << "Passed" << NcbiEndl;
    return 0;
}


/////////////////////////////////////////////////////////////////////////////
//  MAIN

int main(int argc, const char* argv[]) 
{
    return CTestApplication().AppMain(argc, argv);
}
