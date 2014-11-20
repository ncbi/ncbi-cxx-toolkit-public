#include <ncbi_pch.hpp>
#include <corelib/ncbicfg.h>
#include <corelib/error_codes.hpp>
#include <corelib/ncbienv.hpp>
#include <corelib/ncbimisc.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbi_system.hpp>
#include <corelib/ncbi_safe_static.hpp>

BEGIN_STD_NAMESPACE;

#define NCBI_CTYPEFAKEBODY \
  { return See_the_standard_on_proper_argument_type_for_ctype_macros(c); }

inline int NCBI_isalpha(unsigned char c) { return isalpha(c); }
inline int NCBI_isalpha(int           c) { return isalpha(c); }
template<class C>
inline int NCBI_isalpha(C c) NCBI_CTYPEFAKEBODY

#ifdef isalpha
# undef  isalpha
#endif
#define isalpha NCBI_isalpha

inline int NCBI_isalnum(unsigned char c) { return isalnum(c); }
inline int NCBI_isalnum(int           c) { return isalnum(c); }
template<class C>
inline int NCBI_isalnum(C c) NCBI_CTYPEFAKEBODY
#ifdef isalnum
# undef  isalnum
#endif
#define isalnum NCBI_isalnum

inline int NCBI_isascii(unsigned char c) { return isascii(c); }
inline int NCBI_isascii(int           c) { return isascii(c); }
template<class C>
inline int NCBI_isascii(C c) NCBI_CTYPEFAKEBODY
#ifdef isascii
# undef  isascii
#endif
#define isascii NCBI_isascii

inline int NCBI_isblank(unsigned char c) { return isblank(c); }
inline int NCBI_isblank(int           c) { return isblank(c); }
template<class C>
inline int NCBI_isblank(C c) NCBI_CTYPEFAKEBODY
#ifdef isblank
# undef  isblank
#endif
#define isblank NCBI_isblank

inline int NCBI_iscntrl(unsigned char c) { return iscntrl(c); }
inline int NCBI_iscntrl(int           c) { return iscntrl(c); }
template<class C>
inline int NCBI_iscntrl(C c) NCBI_CTYPEFAKEBODY
#ifdef iscntrl
# undef  iscntrl
#endif
#define iscntrl NCBI_iscntrl

inline int NCBI_isdigit(unsigned char c) { return isdigit(c); }
inline int NCBI_isdigit(int           c) { return isdigit(c); }
template<class C>
inline int NCBI_isdigit(C c) NCBI_CTYPEFAKEBODY
#ifdef isdigit
# undef  isdigit
#endif
#define isdigit NCBI_isdigit

inline int NCBI_isgraph(unsigned char c) { return isgraph(c); }
inline int NCBI_isgraph(int           c) { return isgraph(c); }
template<class C>
inline int NCBI_isgraph(C c) NCBI_CTYPEFAKEBODY
#ifdef isgraph
# undef  isgraph
#endif
#define isgraph NCBI_isgraph

inline int NCBI_islower(unsigned char c) { return islower(c); }
inline int NCBI_islower(int           c) { return islower(c); }
template<class C>
inline int NCBI_islower(C c) NCBI_CTYPEFAKEBODY
#ifdef islower
# undef  islower
#endif
#define islower NCBI_islower

inline int NCBI_isprint(unsigned char c) { return isprint(c); }
inline int NCBI_isprint(int           c) { return isprint(c); }
template<class C>
inline int NCBI_isprint(C c) NCBI_CTYPEFAKEBODY
#ifdef isprint
# undef  isprint
#endif
#define isprint NCBI_isprint

inline int NCBI_ispunct(unsigned char c) { return ispunct(c); }
inline int NCBI_ispunct(int           c) { return ispunct(c); }
template<class C>
inline int NCBI_ispunct(C c) NCBI_CTYPEFAKEBODY
#ifdef ispunct
# undef  ispunct
#endif
#define ispunct NCBI_ispunct

inline int NCBI_isspace(unsigned char c) { return isspace(c); }
inline int NCBI_isspace(int           c) { return isspace(c); }
template<class C>
inline int NCBI_isspace(C c) NCBI_CTYPEFAKEBODY
#ifdef isspace
# undef  isspace
#endif
#define isspace NCBI_isspace

inline int NCBI_isupper(unsigned char c) { return isupper(c); }
inline int NCBI_isupper(int           c) { return isupper(c); }
template<class C>
inline int NCBI_isupper(C c) NCBI_CTYPEFAKEBODY
#ifdef isupper
# undef  isupper
#endif
#define isupper NCBI_isupper

inline int NCBI_isxdigit(unsigned char c) { return isxdigit(c); }
inline int NCBI_isxdigit(int           c) { return isxdigit(c); }
template<class C>
inline int NCBI_isxdigit(C c) NCBI_CTYPEFAKEBODY
#ifdef isxdigit
# undef  isxdigit
#endif
#define isxdigit NCBI_isxdigit

inline int NCBI_toascii(unsigned char c) { return toascii(c); }
inline int NCBI_toascii(int           c) { return toascii(c); }
template<class C>
inline int NCBI_toascii(C c) NCBI_CTYPEFAKEBODY
#ifdef toascii
# undef  toascii
#endif
#define toascii NCBI_toascii

inline int NCBI_tolower(unsigned char c) { return tolower(c); }
inline int NCBI_tolower(int           c) { return tolower(c); }
template<class C>
inline int NCBI_tolower(C c) NCBI_CTYPEFAKEBODY
#ifdef tolower
# undef  tolower
#endif
#define tolower NCBI_tolower

inline int NCBI_toupper(unsigned char c) { return toupper(c); }
inline int NCBI_toupper(int           c) { return toupper(c); }
template<class C>
inline int NCBI_toupper(C c) NCBI_CTYPEFAKEBODY
#ifdef toupper
# undef  toupper
#endif
#define toupper NCBI_toupper

#undef NCBI_CTYPEFAKEBODY

END_STD_NAMESPACE;

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
    NcbiCout << "Passed" << NcbiEndl;
    return 0;
}


/////////////////////////////////////////////////////////////////////////////
//  MAIN

int main(int argc, const char* argv[]) 
{
    return CTestApplication().AppMain(argc, argv);
}
