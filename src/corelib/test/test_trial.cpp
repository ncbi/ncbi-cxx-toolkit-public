/*  $Id$
 * ===========================================================================
 *
 *                            PUBLIC DOMAIN NOTICE
 *               National Center for Biotechnology Information
 *
 *  This software/database is a "United States Government Work" under the
 *  terms of the United States Copyright Act.  It was written as part of
 *  the author's official duties as a United States Government employee and
 *  thus cannot be copyrighted.  This software/database is freely available
 *  to the public for use. The National Library of Medicine and the U.S.
 *  Government have not placed any restriction on its use or reproduction.
 *
 *  Although all reasonable efforts have been taken to ensure the accuracy
 *  and reliability of the software and data, the NLM and the U.S.
 *  Government do not and cannot warrant the performance or results that
 *  may be obtained by using this software or data. The NLM and the U.S.
 *  Government disclaim all warranties, express or implied, including
 *  warranties of performance, merchantability or fitness for any particular
 *  purpose.
 *
 *  Please cite the author in any work or product based on this material.
 *
 * ===========================================================================
 *
 * Author:  Eugene Vasilchenko
 *
 * File Description:
 *   Test miscellaneous functionalities
 *
 */


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
#include <boost/test/unit_test_log.hpp>
#include <boost/test/unit_test_log_formatter.hpp>
#include <boost/test/output/plain_report_formatter.hpp>
#include <boost/test/output/xml_report_formatter.hpp>
#include <boost/test/output/compiler_log_formatter.hpp>
#include <boost/test/output/xml_log_formatter.hpp>
#include <boost/test/utils/xml_printer.hpp>
#include <boost/test/detail/global_typedef.hpp>
#include <boost/test/debug.hpp>

#if BOOST_VERSION >= 105900
#  include <boost/test/tree/observer.hpp>
#  include <boost/test/unit_test_parameters.hpp>
#else
#  include <boost/test/test_observer.hpp>
#  include <boost/test/detail/unit_test_parameters.hpp>
#endif

#include <common/test_assert.h>  /* This header must go last */

USING_NCBI_SCOPE;

class CTestApplication : public CNcbiApplication
{
public:
    virtual int Run(void);
};

namespace foo {
class CClassWithFlags
{
public:
    enum EFlags {
        fDefault = 0,
        fFlag1 = 1<<0,
        fFlag2 = 1<<1,
        fFlag3 = 1<<2,
        fMask12 = 0|fFlag1|fFlag2
    };
    DECLARE_SAFE_FLAGS_TYPE(EFlags, TFlags);
    enum EOtherFlags {
        fOtherDefault = 0,
        fOtherFlag1 = 1<<0,
        fOtherFlag2 = 1<<1,
        fOtherFlag3 = 1<<2,
        fOtherMask12 = fOtherFlag1|fOtherFlag2
    };
    DECLARE_SAFE_FLAGS_TYPE(EOtherFlags, TOtherFlags);
    enum EOtherEnum {
        eValue1,
        eValue2,
        eValue3 = eValue1 + eValue2
    };

#ifdef NCBI_ENABLE_SAFE_FLAGS
    static int Foo(TFlags flags = fDefault)
        {
            if ( flags == fDefault ) {
                cout << "Foo()" << endl;
            }
            else {
                cout << "Foo("<<flags<<")" << endl;
            }
            return flags.get() | 1;
        }
    static int Foo(int flags)
        {
            cout << "Foo(int = " << flags << ")" << endl;
            return 0;
        }
#else
    static int Foo(int flags = fDefault)
        {
            cout << "Foo(int = " << flags << ")" << endl;
            return 0;
        }
#endif
    static int Foo(EFlags flags)
        {
            return Foo(TFlags(flags));
        }
    static int Foo2(TFlags flags)
        {
            return Foo(flags);
        }
};
DECLARE_SAFE_FLAGS(CClassWithFlags::EFlags);
template<int>
class COtherClass
{
public:
    enum EOtherEnum {
        eValue1,
        eValue2,
        eValue3 = eValue1 + eValue2
    };
};
}
void TestSafeFlags()
{
    // safe-flags test
    foo::CClassWithFlags::Foo();
    foo::CClassWithFlags::Foo(~foo::CClassWithFlags::fFlag2);
    using namespace foo;
    CClassWithFlags::TFlags ff = CClassWithFlags::fFlag1;
#ifdef NCBI_ENABLE_SAFE_FLAGS
# define SAFE_ASSERT(v) _ASSERT(v)
#else
    // test compilation only
# define SAFE_ASSERT(v) (void)(v)
#endif
    SAFE_ASSERT(CClassWithFlags::Foo(CClassWithFlags::fFlag1 | CClassWithFlags::fFlag3));
    SAFE_ASSERT(CClassWithFlags::Foo(CClassWithFlags::fMask12&CClassWithFlags::fFlag2));
    SAFE_ASSERT(CClassWithFlags::Foo(CClassWithFlags::fMask12^CClassWithFlags::fFlag2));
    SAFE_ASSERT(CClassWithFlags::Foo(CClassWithFlags::EFlags(COtherClass<1>::eValue3)));
    SAFE_ASSERT(CClassWithFlags::Foo(ff | CClassWithFlags::fFlag3));
    SAFE_ASSERT(CClassWithFlags::Foo(CClassWithFlags::fMask12 & ff));
    SAFE_ASSERT(CClassWithFlags::Foo(ff ^ ff));
    SAFE_ASSERT(CClassWithFlags::Foo(~ff));
    SAFE_ASSERT(CClassWithFlags::Foo2(~ff|CClassWithFlags::fFlag2));
    SAFE_ASSERT(CClassWithFlags::Foo2(CClassWithFlags::fFlag2));
    SAFE_ASSERT(!CClassWithFlags::Foo(CClassWithFlags::eValue1));
    SAFE_ASSERT(ff);
    SAFE_ASSERT(ff != 0);
    SAFE_ASSERT((ff & CClassWithFlags::fFlag2) == 0);
    SAFE_ASSERT(ff == CClassWithFlags::fFlag1);
    SAFE_ASSERT(ff != CClassWithFlags::fFlag2);
    SAFE_ASSERT(ff != CClassWithFlags::fOtherFlag2);
#undef SAFE_ASSERT
    // all below operations should cause compilation error
    //CClassWithFlags::Foo(CClassWithFlags::fOtherFlag1 | CClassWithFlags::fOtherFlag3);
    CClassWithFlags::Foo(COtherClass<1>::eValue3);
}

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
    TGi gi = 2;
    gi = 3;
    gi = INVALID_GI;
    gi = GI_CONST(192377853);
#ifdef NCBI_INT8_GI
    gi = GI_CONST(192377853343);
#endif
    NcbiCout << "GI = " << gi << NcbiEndl;
    TestSafeFlags();
    NcbiCout << "Passed" << NcbiEndl;
    return 0;
}


/////////////////////////////////////////////////////////////////////////////
//  MAIN

int main(int argc, const char* argv[]) 
{
    return CTestApplication().AppMain(argc, argv);
}
