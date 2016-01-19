#ifndef CORELIB___TEST_BOOST__HPP
#define CORELIB___TEST_BOOST__HPP

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
 * Author:  Pavel Ivanov
 *
 */

/// @file test_boost.hpp
///   Utility stuff for more convenient using of Boost.Test library.
///
/// This header must be included before any Boost.Test header
/// (if you have any).

#ifdef BOOST_CHECK
#  error "test_boost.hpp should be included before any Boost.Test header"
#endif


#include <corelib/ncbistd.hpp>
#include <corelib/expr.hpp>
#include <corelib/ncbiargs.hpp>


// Keep Boost's inclusion of <limits> from breaking under old WorkShop versions.
#if defined(numeric_limits)  &&  defined(NCBI_NUMERIC_LIMITS)
#  undef numeric_limits
#endif

// BOOST_AUTO_TEST_MAIN should not be defined - it is in test_boost library
#ifdef BOOST_AUTO_TEST_MAIN
#  undef BOOST_AUTO_TEST_MAIN
#endif

#ifdef NCBI_COMPILER_MSVC
#  pragma warning(push)
// 'class' : class has virtual functions, but destructor is not virtual
#  pragma warning(disable: 4265)
#endif

#include <boost/version.hpp>
#include <boost/test/auto_unit_test.hpp>
#include <boost/test/floating_point_comparison.hpp>
#include <boost/test/framework.hpp>
#include <boost/test/execution_monitor.hpp>
#include <boost/test/parameterized_test.hpp>

#include <boost/preprocessor/tuple/rem.hpp>
#include <boost/preprocessor/repeat.hpp>
#include <boost/preprocessor/array/elem.hpp>
#include <boost/preprocessor/arithmetic/inc.hpp>

#include <common/boost_skew_guard.hpp>

#ifdef NCBI_COMPILER_MSVC
#  pragma warning(pop)
#endif


// Redefine some Boost macros to make them more comfortable and fit them into
// the framework.
#undef BOOST_CHECK_THROW_IMPL
#undef BOOST_CHECK_NO_THROW_IMPL
#ifdef BOOST_FIXTURE_TEST_CASE_WITH_DECOR
#  undef BOOST_FIXTURE_TEST_CASE_WITH_DECOR
#else
#  undef BOOST_FIXTURE_TEST_CASE
#endif
#undef BOOST_PARAM_TEST_CASE

#if BOOST_VERSION >= 105900
#  if BOOST_VERSION >= 106000
#    define BOOST_CHECK_THROW_IMPL( S, E, P, postfix, TL )              \
do {                                                                    \
    try {                                                               \
        BOOST_TEST_PASSPOINT();                                         \
        S;                                                              \
        BOOST_TEST_TOOL_IMPL( 2, false, "exception " BOOST_STRINGIZE(E) \
                              " expected but not raised",               \
                              TL, CHECK_MSG, _ );                       \
    } catch( E const& ex ) {                                            \
        ::boost::unit_test::ut_detail::ignore_unused_variable_warning( ex ); \
        BOOST_TEST_TOOL_IMPL( 2, P,                                     \
                              "exception \"" BOOST_STRINGIZE( E )       \
                              "\" raised as expected" postfix,          \
                              TL, CHECK_MSG, _  );                      \
    } catch (...) {                                                     \
        BOOST_TEST_TOOL_IMPL( 2, false,                                 \
                              "an unexpected exception was thrown by "  \
                              BOOST_STRINGIZE( S ),                     \
                              TL, CHECK_MSG, _ );                       \
    }                                                                   \
} while( ::boost::test_tools::tt_detail::dummy_cond() )                 \
/**/
#  else
#    define BOOST_CHECK_THROW_IMPL( S, E, P, prefix, TL )               \
do {                                                                    \
    try {                                                               \
        BOOST_TEST_PASSPOINT();                                         \
        S;                                                              \
        BOOST_TEST_TOOL_IMPL( 2, false, "exception " BOOST_STRINGIZE(E) \
                              " is expected",                           \
                              TL, CHECK_MSG, _ );                       \
    } catch( E const& ex ) {                                            \
        ::boost::unit_test::ut_detail::ignore_unused_variable_warning( ex ); \
        BOOST_TEST_TOOL_IMPL( 2, P, prefix BOOST_STRINGIZE( E ) " is caught", \
                              TL, CHECK_MSG, _  );                      \
    } catch (...) {                                                     \
        BOOST_TEST_TOOL_IMPL( 2, false,                                 \
                              "an unexpected exception was thrown by "  \
                              BOOST_STRINGIZE( S ),                     \
                              TL, CHECK_MSG, _ );                       \
    }                                                                   \
} while( ::boost::test_tools::tt_detail::dummy_cond() )                 \
/**/
#  endif

#  define BOOST_CHECK_NO_THROW_IMPL( S, TL )                            \
do {                                                                    \
    try {                                                               \
        S;                                                              \
        BOOST_TEST_TOOL_IMPL( 2, true, "no exceptions thrown by "       \
                              BOOST_STRINGIZE( S ),                     \
                              TL, CHECK_MSG, _ );                       \
    } catch (std::exception& ex) {                                      \
        BOOST_TEST_TOOL_IMPL( 2, false, "an std::exception was thrown by " \
                              BOOST_STRINGIZE( S ) " : " << ex.what(),  \
                              TL, CHECK_MSG, _ );                       \
    } catch( ... ) {                                                    \
        BOOST_TEST_TOOL_IMPL( 2, false, "a nonstandard exception thrown by " \
                              BOOST_STRINGIZE( S ),                     \
                              TL, CHECK_MSG, _ );                       \
    }                                                                   \
} while( ::boost::test_tools::tt_detail::dummy_cond() )                 \
/**/
#else
#  define BOOST_CHECK_THROW_IMPL( S, E, P, prefix, TL )                  \
try {                                                                    \
    BOOST_TEST_PASSPOINT();                                              \
    S;                                                                   \
    BOOST_CHECK_IMPL( false, "exception " BOOST_STRINGIZE( E )           \
                             " is expected", TL, CHECK_MSG ); }          \
catch( E const& ex ) {                                                   \
    boost::unit_test::ut_detail::ignore_unused_variable_warning( ex );   \
    BOOST_CHECK_IMPL( P, prefix BOOST_STRINGIZE( E ) " is caught",       \
                      TL, CHECK_MSG );                                   \
}                                                                        \
catch (...) {                                                            \
    BOOST_CHECK_IMPL(false, "an unexpected exception was thrown by "     \
                            BOOST_STRINGIZE( S ),                        \
                     TL, CHECK_MSG);                                     \
}                                                                        \
/**/

#  define BOOST_CHECK_NO_THROW_IMPL( S, TL )                                 \
try {                                                                        \
    S;                                                                       \
    BOOST_CHECK_IMPL( true, "no exceptions thrown by " BOOST_STRINGIZE( S ), \
                      TL, CHECK_MSG );                                       \
}                                                                            \
catch (std::exception& ex) {                                                 \
    BOOST_CHECK_IMPL( false, "an std::exception was thrown by "              \
                             BOOST_STRINGIZE( S ) " : " << ex.what(),        \
                      TL, CHECK_MSG);                                        \
}                                                                            \
catch( ... ) {                                                               \
    BOOST_CHECK_IMPL( false, "a nonstandard exception thrown by "            \
                             BOOST_STRINGIZE( S ),                           \
                      TL, CHECK_MSG );                                       \
}                                                                            \
/**/
#endif

#if BOOST_VERSION >= 104200
#  define NCBI_BOOST_LOCATION()  , boost::execution_exception::location()
#else
#  define NCBI_BOOST_LOCATION()
#endif

#ifdef BOOST_FIXTURE_TEST_CASE_NO_DECOR
#  define NCBI_BOOST_DECORATOR_ARG \
    , boost::unit_test::decorator::collector::instance()
#else
#  define NCBI_BOOST_DECORATOR_ARG
#endif

#ifdef BOOST_FIXTURE_TEST_CASE_NO_DECOR
#  define BOOST_FIXTURE_TEST_CASE_WITH_DECOR( test_name, F, decorators ) \
struct test_name : public F { void test_method(); };                    \
                                                                        \
static void BOOST_AUTO_TC_INVOKER( test_name )()                        \
{                                                                       \
    BOOST_TEST_CHECKPOINT('"' << #test_name << "\" fixture entry.");    \
    test_name t;                                                        \
    BOOST_TEST_CHECKPOINT('"' << #test_name << "\" entry.");            \
    try {                                                               \
        t.test_method();                                                \
    }                                                                   \
    catch (NCBI_NS_NCBI::CException& ex) {                              \
        ERR_POST("Uncaught exception in \""                             \
                 << boost::unit_test                                    \
                         ::framework::current_test_case().p_name        \
                 << "\"" << ex);                                        \
        throw boost::execution_exception(                               \
                boost::execution_exception::cpp_exception_error, ""     \
                NCBI_BOOST_LOCATION() );                                \
    }                                                                   \
    BOOST_TEST_CHECKPOINT('"' << #test_name << "\" exit.");             \
}                                                                       \
                                                                        \
struct BOOST_AUTO_TC_UNIQUE_ID( test_name ) {};                         \
                                                                        \
static ::NCBI_NS_NCBI::SNcbiTestRegistrar                               \
BOOST_JOIN( BOOST_JOIN( test_name, _registrar ), __LINE__ ) (           \
    boost::unit_test::make_test_case(                                   \
        &BOOST_AUTO_TC_INVOKER( test_name ), #test_name,                \
        __FILE__, __LINE__ ),                                           \
    ::NCBI_NS_NCBI::SNcbiTestTCTimeout<                                 \
        BOOST_AUTO_TC_UNIQUE_ID( test_name )>::instance()->value(),     \
    decorators );                                                       \
                                                                        \
void test_name::test_method()                                           \
/**/
#else
#  define BOOST_FIXTURE_TEST_CASE( test_name, F )                       \
struct test_name : public F { void test_method(); };                    \
                                                                        \
static void BOOST_AUTO_TC_INVOKER( test_name )()                        \
{                                                                       \
    test_name t;                                                        \
    try {                                                               \
        t.test_method();                                                \
    }                                                                   \
    catch (NCBI_NS_NCBI::CException& ex) {                              \
        ERR_POST("Uncaught exception in \""                             \
                 << boost::unit_test                                    \
                         ::framework::current_test_case().p_name        \
                 << "\"" << ex);                                        \
        throw boost::execution_exception(                               \
                boost::execution_exception::cpp_exception_error, ""     \
                NCBI_BOOST_LOCATION() );                                \
    }                                                                   \
}                                                                       \
                                                                        \
struct BOOST_AUTO_TC_UNIQUE_ID( test_name ) {};                         \
                                                                        \
static ::NCBI_NS_NCBI::SNcbiTestRegistrar                               \
BOOST_JOIN( BOOST_JOIN( test_name, _registrar ), __LINE__ ) (           \
    boost::unit_test::make_test_case(                                   \
        &BOOST_AUTO_TC_INVOKER( test_name ), #test_name ),              \
    boost::unit_test::ut_detail::auto_tc_exp_fail<                      \
        BOOST_AUTO_TC_UNIQUE_ID( test_name )>::instance()->value(),     \
    ::NCBI_NS_NCBI::SNcbiTestTCTimeout<                                 \
        BOOST_AUTO_TC_UNIQUE_ID( test_name )>::instance()->value() );   \
                                                                        \
void test_name::test_method()                                           \
/**/
#endif

#define BOOST_PARAM_TEST_CASE( function, begin, end )                       \
    ::NCBI_NS_NCBI::NcbiTestGenTestCases( function,                         \
                                          BOOST_TEST_STRINGIZE( function ), \
                                          (begin), (end) )                  \
/**/

/// Set timeout value for the test case created using auto-registration
/// facility.
#define BOOST_AUTO_TEST_CASE_TIMEOUT(test_name, n)                      \
struct BOOST_AUTO_TC_UNIQUE_ID( test_name );                            \
                                                                        \
static struct BOOST_JOIN( test_name, _timeout_spec )                    \
: ::NCBI_NS_NCBI::                                                      \
  SNcbiTestTCTimeout<BOOST_AUTO_TC_UNIQUE_ID( test_name ) >             \
{                                                                       \
    BOOST_JOIN( test_name, _timeout_spec )()                            \
    : ::NCBI_NS_NCBI::                                                  \
      SNcbiTestTCTimeout<BOOST_AUTO_TC_UNIQUE_ID( test_name ) >( n )    \
    {}                                                                  \
} BOOST_JOIN( test_name, _timeout_spec_inst );                          \
/**/

/// Automatic registration of the set of test cases based on some function
/// accepting one parameter. Set of parameters used to call that function is
/// taken from iterator 'begin' which is incremented until it reaches 'end'.
///
/// @sa BOOST_PARAM_TEST_CASE
#define BOOST_AUTO_PARAM_TEST_CASE( function, begin, end )               \
    BOOST_AUTO_TU_REGISTRAR(function) (                                  \
                            BOOST_PARAM_TEST_CASE(function, begin, end)  \
                            NCBI_BOOST_DECORATOR_ARG)                    \
/**/

#define BOOST_TIMEOUT(M)                                        \
    do {                                                        \
        throw boost::execution_exception(                       \
                boost::execution_exception::timeout_error, M    \
                NCBI_BOOST_LOCATION());                         \
    } while (0)                                                 \
/**/



#if BOOST_VERSION >= 105900

#  define NCBITEST_CHECK_IMPL_EX(frwd_type, P, check_descr, TL, CT, ARGS)    \
    BOOST_CHECK_NO_THROW_IMPL(                                               \
        BOOST_TEST_TOOL_IMPL(frwd_type, P, check_descr, TL, CT, ARGS), TL)

#  define NCBITEST_CHECK_IMPL(P, check_descr, TL, CT)                        \
    NCBITEST_CHECK_IMPL_EX(2, P, check_descr, TL, CT, _)

#  define NCBITEST_CHECK_WITH_ARGS_IMPL(P, check_descr, TL, CT, ARGS)        \
    NCBITEST_CHECK_IMPL_EX(0, ::boost::test_tools::tt_detail::P(),           \
                           check_descr, TL, CT, ARGS)

#else

#  define NCBITEST_CHECK_IMPL(P, check_descr, TL, CT)                        \
    BOOST_CHECK_NO_THROW_IMPL(BOOST_CHECK_IMPL(P, check_descr, TL, CT), TL)

#  define NCBITEST_CHECK_WITH_ARGS_IMPL(P, check_descr, TL, CT, ARGS)        \
    BOOST_CHECK_NO_THROW_IMPL(BOOST_CHECK_WITH_ARGS_IMPL(                    \
    ::boost::test_tools::tt_detail::P(), check_descr, TL, CT, ARGS), TL)

#endif



// Several analogs to BOOST_* macros that make simultaneous checking of
// NO_THROW and some other condition
#define NCBITEST_WARN(P)      NCBITEST_CHECK_IMPL( (P), BOOST_TEST_STRINGIZE( P ), WARN,    CHECK_PRED )
#define NCBITEST_CHECK(P)     NCBITEST_CHECK_IMPL( (P), BOOST_TEST_STRINGIZE( P ), CHECK,   CHECK_PRED )
#define NCBITEST_REQUIRE(P)   NCBITEST_CHECK_IMPL( (P), BOOST_TEST_STRINGIZE( P ), REQUIRE, CHECK_PRED )


#define NCBITEST_WARN_MESSAGE( P, M )    NCBITEST_CHECK_IMPL( (P), M, WARN,    CHECK_MSG )
#define NCBITEST_CHECK_MESSAGE( P, M )   NCBITEST_CHECK_IMPL( (P), M, CHECK,   CHECK_MSG )
#define NCBITEST_REQUIRE_MESSAGE( P, M ) NCBITEST_CHECK_IMPL( (P), M, REQUIRE, CHECK_MSG )


#define NCBITEST_WARN_EQUAL( L, R ) \
    NCBITEST_CHECK_WITH_ARGS_IMPL( equal_impl_frwd, "", WARN,    CHECK_EQUAL, (L)(R) )
#define NCBITEST_CHECK_EQUAL( L, R ) \
    NCBITEST_CHECK_WITH_ARGS_IMPL( equal_impl_frwd, "", CHECK,   CHECK_EQUAL, (L)(R) )
#define NCBITEST_REQUIRE_EQUAL( L, R ) \
    NCBITEST_CHECK_WITH_ARGS_IMPL( equal_impl_frwd, "", REQUIRE, CHECK_EQUAL, (L)(R) )


#define NCBITEST_WARN_NE( L, R ) \
    NCBITEST_CHECK_WITH_ARGS_IMPL( ne_impl, "", WARN,    CHECK_NE, (L)(R) )
#define NCBITEST_CHECK_NE( L, R ) \
    NCBITEST_CHECK_WITH_ARGS_IMPL( ne_impl, "", CHECK,   CHECK_NE, (L)(R) )
#define NCBITEST_REQUIRE_NE( L, R ) \
    NCBITEST_CHECK_WITH_ARGS_IMPL( ne_impl, "", REQUIRE, CHECK_NE, (L)(R) )


// Ensure Clang analysis tools (notably scan-build) recognize that a failed
// {BOOST,NCBITEST}_REQUIRE* call will bail.
#ifdef __clang_analyzer__
#  undef  BOOST_REQUIRE
#  define BOOST_REQUIRE(P)               _ALWAYS_ASSERT(P)
#  undef  BOOST_REQUIRE_MESSAGE
#  define BOOST_REQUIRE_MESSAGE(P, M)    _ALWAYS_ASSERT((FORMAT(M), (P)))
#  undef  BOOST_REQUIRE_EQUAL
#  define BOOST_REQUIRE_EQUAL(L, R)      _ALWAYS_ASSERT((L) == (R))
#  undef  BOOST_REQUIRE_NE
#  define BOOST_REQUIRE_NE(L, R)         _ALWAYS_ASSERT((L) != (R))
#  undef  BOOST_REQUIRE_NO_THROW
#  define BOOST_REQUIRE_NO_THROW(S) try { S; } catch (...) { _ALWAYS_TROUBLE; }
#  undef  BOOST_REQUIRE_THROW
#  define BOOST_REQUIRE_THROW(S, E) \
    do { try { S; } catch (E const&) { break; } _ALWAYS_TROUBLE; } while(false)
#  undef  NCBITEST_REQUIRE
#  define NCBITEST_REQUIRE(P) BOOST_REQUIRE_NO_THROW(_ALWAYS_ASSERT(P))
#  undef  NCBITEST_REQUIRE_MESSAGE
#  define NCBITEST_REQUIRE_MESSAGE(P, M) NCBITEST_REQUIRE((FORMAT(M), (P)))
#  undef  NCBITEST_REQUIRE_EQUAL
#  define NCBITEST_REQUIRE_EQUAL(L, R)   NCBITEST_REQUIRE((L) == (R))
#  undef  NCBITEST_REQUIRE_NE
#  define NCBITEST_REQUIRE_NE(L, R)      NCBITEST_REQUIRE((L) != (R))
// Used but left as is: BOOST_REQUIRE_CLOSE, BOOST_REQUIRE_EQUAL_COLLECTIONS.
// Irrelevant: BOOST_REQUIRE_CTX (custom, based on BOOST_REQUIRE),
//             BOOST_REQUIRE_CUTPOINT (custom, based on throw).
// Used only in BOOST_CHECK_... form: GE, GT, LT, SMALL.
#endif


/** @addtogroup Tests
 *
 * @{
 */


BEGIN_NCBI_SCOPE


/// Macro for introducing function initializing argument descriptions for
/// tests. This function will be called before CNcbiApplication will parse
/// command line arguments. So it will parse command line using descriptions
/// set by this function. Also test framework will react correctly on such
/// arguments as -h, -help or -dryrun (the last will just print list of unit
/// tests without actually executing them). The parameter var_name is a name
/// for variable of type CArgDescriptions* that can be used inside function
/// to set up argument descriptions. Usage of this macro is like this:<pre>
/// NCBITEST_INIT_CMDLINE(my_args)
/// {
///     my_args->SetUsageContext(...);
///     my_args->AddPositional(...);
/// }
/// </pre>
///
#define NCBITEST_INIT_CMDLINE(var_name)                     \
    NCBITEST_AUTOREG_PARAMFUNC(eTestUserFuncCmdLine,        \
                               CArgDescriptions* var_name,  \
                               NcbiTestGetArgDescrs)


/// Macro for introducing initialization function which will be called before
/// tests execution and only if tests will be executed (if there's no command
/// line parameter -dryrun or --do_not_test) even if only select number of
/// tests will be executed (if command line parameter --run_test=... were
/// given). If any of these initialization functions will throw an exception
/// then tests will not be executed. The usage of this macro:<pre>
/// NCBITEST_AUTO_INIT()
/// {
///     // initialization function body
/// }
/// </pre>
/// Arbitrary number of initialization functions can be defined. They all will
/// be called before tests but the order of these callings is not defined.
///
/// @sa NCBITEST_AUTO_FINI
///
#define NCBITEST_AUTO_INIT()  NCBITEST_AUTOREG_FUNCTION(eTestUserFuncInit)


/// Macro for introducing finalization function which will be called after
/// actual tests execution even if only select number of tests will be
/// executed (if command line parameter --run_test=... were given). The usage
/// of this macro:<pre>
/// NCBITEST_AUTO_FINI()
/// {
///     // finalization function body
/// }
/// </pre>
/// Arbitrary number of finalization functions can be defined. They all will
/// be called after tests are executed but the order of these callings is not
/// defined.
///
/// @sa NCBITEST_AUTO_INIT
///
#define NCBITEST_AUTO_FINI()  NCBITEST_AUTOREG_FUNCTION(eTestUserFuncFini)


/// Macro for introducing function which should initialize configuration
/// conditions parser. This parser will be used to evaluate conditions for
/// running tests written in configuration file. So you should set values for
/// all variables that you want to participate in those expressions. Test
/// framework automatically adds all OS*, COMPILER* and DLL_BUILD variables
/// with the values of analogous NCBI_OS*, NCBI_COMPILER* and NCBI_DLL_BUILD
/// macros. The usage of this macro:<pre>
/// NCBITEST_INIT_VARIABLES(my_parser)
/// {
///    my_parser->AddSymbol("var_name1", value_expr1);
///    my_parser->AddSymbol("var_name2", value_expr2);
/// }
/// </pre>
/// Arbitrary number of such functions can be defined.
///
#define NCBITEST_INIT_VARIABLES(var_name)              \
    NCBITEST_AUTOREG_PARAMFUNC(eTestUserFuncVars,      \
                               CExprParser* var_name,  \
                               NcbiTestGetIniParser)


/// Macro for introducing function which should initialize dependencies
/// between test units and some hard coded (not taken from configuration file)
/// tests disablings. All function job can be done by using NCBITEST_DISABLE,
/// NCBITEST_DEPENDS_ON and NCBITEST_DEPENDS_ON_N macros in conjunction with
/// some conditional statements maybe. The usage of this macro:<pre>
/// NCBITEST_INIT_TREE()
/// {
///     NCBITEST_DISABLE(test_name11);
///
///     NCBITEST_DEPENDS_ON(test_name22, test_name1);
///     NCBITEST_DEPENDS_ON_N(test_name33, N, (test_name1, ..., test_nameN));
/// }
/// </pre>
/// Arbitrary number of such functions can be defined.
///
/// @sa NCBITEST_DISABLE, NCBITEST_DEPENDS_ON, NCBITEST_DEPENDS_ON_N
///
#define NCBITEST_INIT_TREE()  NCBITEST_AUTOREG_FUNCTION(eTestUserFuncDeps)


/// Unconditionally disable test case. To be used inside function introduced
/// by NCBITEST_INIT_TREE.
///
/// @param test_name
///   Name of the test as a bare text without quotes. Name can exclude test_
///   prefix if function name includes one and class prefix if it is class
///   member test case.
///
/// @sa NCBITEST_INIT_TREE
///
#define NCBITEST_DISABLE(test_name)                               \
    NcbiTestDisable(NcbiTestGetUnit(BOOST_STRINGIZE(test_name)))


/// Add dependency between test test_name and dep_name. This dependency means
/// if test dep_name is failed during execution or was disabled by any reason
/// then test test_name will not be executed (will be skipped).
/// To be used inside function introduced by NCBITEST_INIT_TREE.
///
/// @param test_name
///   Name of the test as a bare text without quotes. Name can exclude test_
///   prefix if function name includes one and class prefix if it is class
///   member test case.
/// @param dep_name
///   Name of the test to depend on. Name can be given with the same
///   assumptions as test_name.
///
/// @sa NCBITEST_INIT_TREE, NCBI_TEST_DEPENDS_ON_N
///
#define NCBITEST_DEPENDS_ON(test_name, dep_name)                    \
    NcbiTestDependsOn(NcbiTestGetUnit(BOOST_STRINGIZE(test_name)),  \
                      NcbiTestGetUnit(BOOST_STRINGIZE(dep_name)))


/// Add dependency between test test_name and several other tests which names
/// given in the list dep_names_array. This dependency means if any of the
/// tests in list dep_names_array is failed during execution or was disabled
/// by any reason then test test_name will not be executed (will be skipped).
/// To be used inside function introduced by NCBITEST_INIT_TREE. Macro is
/// equivalent to use NCBI_TEST_DEPENDS_ON several times for each test in
/// dep_names_array.
///
/// @param test_name
///   Name of the test as a bare text without quotes. Name can exclude test_
///   prefix if function name includes one and class prefix if it is class
///   member test case.
/// @param N
///   Number of tests in dep_names_array
/// @param dep_names_array
///   Names of tests to depend on. Every name can be given with the same
///   assumptions as test_name. Array should be given enclosed in parenthesis
///   like (test_name1, ..., test_nameN) and should include exactly N elements
///   or preprocessor error will occur during compilation.
///
/// @sa NCBITEST_INIT_TREE, NCBI_TEST_DEPENDS_ON
///
#define NCBITEST_DEPENDS_ON_N(test_name, N, dep_names_array)        \
    BOOST_PP_REPEAT(N, NCBITEST_DEPENDS_ON_N_IMPL,                  \
                    (BOOST_PP_INC(N), (test_name,                   \
                        BOOST_PP_TUPLE_REM(N) dep_names_array)))    \
    (void)0


/// Set of macros to manually add test cases that cannot be created using
/// BOOST_AUTO_TEST_CASE. To create such test cases you should have a function
/// (that can accept up to 3 parameters) and use one of macros below inside
/// NCBITEST_INIT_TREE() function. All function parameters are passed by value.
///
/// @sa NCBITEST_INIT_TREE, BOOST_AUTO_PARAM_TEST_CASE
#define NCBITEST_ADD_TEST_CASE(function)                            \
    boost::unit_test::framework::master_test_suite().add(           \
        boost::unit_test::make_test_case(                           \
                            boost::bind(function),                  \
                            BOOST_TEST_STRINGIZE(function)          \
                                        )               )
#define NCBITEST_ADD_TEST_CASE1(function, param1)                   \
    boost::unit_test::framework::master_test_suite().add(           \
        boost::unit_test::make_test_case(                           \
                            boost::bind(function, (param1)),        \
                            BOOST_TEST_STRINGIZE(function)          \
                                        )               )
#define NCBITEST_ADD_TEST_CASE2(function, param1, param2)               \
    boost::unit_test::framework::master_test_suite().add(               \
        boost::unit_test::make_test_case(                               \
                            boost::bind(function, (param1), (param2)),  \
                            BOOST_TEST_STRINGIZE(function)              \
                                        )               )
#define NCBITEST_ADD_TEST_CASE3(function, param1, param2, param3)                \
    boost::unit_test::framework::master_test_suite().add(                        \
        boost::unit_test::make_test_case(                                        \
                            boost::bind(function, (param1), (param2), (param3)), \
                            BOOST_TEST_STRINGIZE(function)                       \
                                        )               )


/// Disable execution of all tests in current configuration. Call to the
/// function is equivalent to setting GLOBAL = true in ini file.
/// Globally disabled tests are shown as DIS by check scripts
/// (called via make check).
/// Function should be called only from NCBITEST_AUTO_INIT() or
/// NCBITEST_INIT_TREE() functions.
///
/// @sa NCBITEST_AUTO_INIT, NCBITEST_INIT_TREE
///
void NcbiTestSetGlobalDisabled(void);


/// Skip execution of all tests in current configuration.
/// Globally skipped tests are shown as SKP by check scripts
/// (called via make check).
/// Function should be called only from NCBITEST_AUTO_INIT() or
/// NCBITEST_INIT_TREE() functions.
///
/// @sa NCBITEST_AUTO_INIT, NCBITEST_INIT_TREE
///
void NcbiTestSetGlobalSkipped(void);


//////////////////////////////////////////////////////////////////////////
// All API from this line below is for internal use only and is not
// intended for use by any users. All this stuff is used by end-user
// macros defined above.
//////////////////////////////////////////////////////////////////////////


/// Helper macro to implement NCBI_TEST_DEPENDS_ON_N.
#define NCBITEST_DEPENDS_ON_N_IMPL(z, n, names_array)               \
    NCBITEST_DEPENDS_ON(BOOST_PP_ARRAY_ELEM(0, names_array),        \
    BOOST_PP_ARRAY_ELEM(BOOST_PP_INC(n), names_array));


/// Mark test case/suite as dependent on another test case/suite.
/// If dependency test case didn't executed successfully for any reason then
/// dependent test will not be executed. This rule has one exception: if test
/// is requested to execute in command line via parameter "--run_test" and
/// dependency was not requested to execute, requested test will be executed
/// anyways.
///
/// @param tu
///   Test case/suite that should be marked as dependent
/// @param dep_tu
///   Test case/suite that will be "parent" for tu
void NcbiTestDependsOn(boost::unit_test::test_unit* tu,
                       boost::unit_test::test_unit* dep_tu);

/// Disable test unit.
/// Disabled test unit will not be executed (as if p_enabled is set to false)
/// but it will be reported in final Boost.Test report as disabled (as opposed
/// to setting p_enabled to false when test does not appear in final
/// Boost.Test report).
void NcbiTestDisable(boost::unit_test::test_unit* tu);


/// Type of user-defined function which will be automatically registered
/// in test framework
typedef void (*TNcbiTestUserFunction)(void);

/// Types of functions that user can define
enum ETestUserFuncType {
    eTestUserFuncInit,
    eTestUserFuncFini,
    eTestUserFuncCmdLine,
    eTestUserFuncVars,
    eTestUserFuncDeps,
    eTestUserFuncFirst = eTestUserFuncInit,
    eTestUserFuncLast  = eTestUserFuncDeps
};

/// Registrar of all user-defined functions
void RegisterNcbiTestUserFunc(TNcbiTestUserFunction func,
                              ETestUserFuncType     func_type);

/// Class for implementing automatic registration of user functions
struct SNcbiTestUserFuncReg
{
    SNcbiTestUserFuncReg(TNcbiTestUserFunction func,
                         ETestUserFuncType     func_type)
    {
        RegisterNcbiTestUserFunc(func, func_type);
    }
};

/// Get pointer to parser which will be used for evaluating conditions written
/// in configuration file
CExprParser* NcbiTestGetIniParser(void);

/// Get ArgDescriptions object which will be passed to application for parsing
/// command line arguments.
CArgDescriptions* NcbiTestGetArgDescrs(void);

/// Get pointer to test unit by its name which can be partial, i.e. without
/// class prefix and/or test_ prefix if any. Throws an exception in case of
/// name of non-existent test
boost::unit_test::test_unit* NcbiTestGetUnit(CTempString test_name);


/// Helper macros for unique identifiers
#define NCBITEST_AUTOREG_FUNC(type)  \
                      BOOST_JOIN(BOOST_JOIN(Ncbi_, type),       __LINE__)
#define NCBITEST_AUTOREG_OBJ     BOOST_JOIN(NcbiTestAutoObj,    __LINE__)
#define NCBITEST_AUTOREG_HELPER  BOOST_JOIN(NcbiTestAutoHelper, __LINE__)

#define NCBITEST_AUTOREG_FUNCTION(type)                                    \
static void NCBITEST_AUTOREG_FUNC(type)(void);                             \
static ::NCBI_NS_NCBI::SNcbiTestUserFuncReg                                \
NCBITEST_AUTOREG_OBJ(&NCBITEST_AUTOREG_FUNC(type), ::NCBI_NS_NCBI::type);  \
static void NCBITEST_AUTOREG_FUNC(type)(void)

#define NCBITEST_AUTOREG_PARAMFUNC(type, param_decl, param_func)       \
static void NCBITEST_AUTOREG_FUNC(type)(::NCBI_NS_NCBI::param_decl);   \
static void NCBITEST_AUTOREG_HELPER(void)                              \
{                                                                      \
    NCBITEST_AUTOREG_FUNC(type)(::NCBI_NS_NCBI::param_func());         \
}                                                                      \
static ::NCBI_NS_NCBI::SNcbiTestUserFuncReg                            \
NCBITEST_AUTOREG_OBJ(&NCBITEST_AUTOREG_HELPER, ::NCBI_NS_NCBI::type);  \
static void NCBITEST_AUTOREG_FUNC(type)(::NCBI_NS_NCBI::param_decl)

/// Extension auto-registrar from Boost.Test that can automatically set the
/// timeout for unit.
struct SNcbiTestRegistrar
    : public boost::unit_test::ut_detail::auto_test_unit_registrar
{
    typedef boost::unit_test::ut_detail::auto_test_unit_registrar TParent;

    SNcbiTestRegistrar(boost::unit_test::test_case* tc,
                       boost::unit_test::counter_t  exp_fail,
                       unsigned int                 timeout)
        : TParent(tc NCBI_BOOST_DECORATOR_ARG, exp_fail)
    {
        tc->p_timeout.set(timeout);
    }

#ifdef BOOST_FIXTURE_TEST_CASE_WITH_DECOR
    SNcbiTestRegistrar(boost::unit_test::test_case*            tc,
                       unsigned int                            timeout,
                       boost::unit_test::decorator::collector& decorator)
        : TParent(tc, decorator)
    {
        tc->p_timeout.set(timeout);
    }
#endif

    SNcbiTestRegistrar(boost::unit_test::test_case* tc,
                       boost::unit_test::counter_t  exp_fail)
        : TParent(tc NCBI_BOOST_DECORATOR_ARG, exp_fail)
    {}

#ifndef BOOST_FIXTURE_TEST_CASE_WITH_DECOR
    explicit
    SNcbiTestRegistrar(boost::unit_test::const_string ts_name)
        : TParent(ts_name)
    {}
#endif

    explicit
    SNcbiTestRegistrar(boost::unit_test::test_unit_generator const& tc_gen)
        : TParent(tc_gen NCBI_BOOST_DECORATOR_ARG)
    {}

    explicit
    SNcbiTestRegistrar(int n)
        : TParent(n)
    {}
};


/// Copy of auto_tc_exp_fail from Boost.Test to store the value of timeout
/// for each test.
template<typename T>
struct SNcbiTestTCTimeout
{
    SNcbiTestTCTimeout() : m_value(0) {}

    explicit SNcbiTestTCTimeout(unsigned int v)
        : m_value( v )
    {
        instance() = this;
    }

    static SNcbiTestTCTimeout*& instance()
    {
        static SNcbiTestTCTimeout  inst;
        static SNcbiTestTCTimeout* inst_ptr = &inst;

        return inst_ptr;
    }

    unsigned int value() const { return m_value; }

private:
    // Data members
    unsigned    m_value;
};


/// Special generator of test cases for function accepting one parameter.
/// Generator differs from the one provided in Boost.Test in names assigned to
/// generated test cases. NCBI.Test library requires all test names to be
/// unique.
template<typename ParamType, typename ParamIter>
class CNcbiTestParamTestCaseGenerator
    : public boost::unit_test::test_unit_generator
{
public:
#if BOOST_VERSION >= 105900
    typedef boost::function<void (ParamType)> TTestFunc;
#else
    typedef boost::unit_test::callback1<ParamType> TTestFunc;
#endif

    CNcbiTestParamTestCaseGenerator(TTestFunc const&               test_func,
                                    boost::unit_test::const_string name,
                                    ParamIter                      par_begin,
                                    ParamIter                      par_end)
        : m_TestFunc(test_func),
          m_Name(boost::unit_test::ut_detail::normalize_test_case_name(name)),
          m_ParBegin(par_begin),
          m_ParEnd(par_end),
          m_CaseIndex(0)
    {
        m_Name += "_";
    }

    virtual ~CNcbiTestParamTestCaseGenerator() {}

    virtual boost::unit_test::test_unit* next() const
    {
        if( m_ParBegin == m_ParEnd )
            return NULL;

        string this_name(m_Name);
        this_name += NStr::IntToString(++m_CaseIndex);
#if BOOST_VERSION >= 105900
        boost::unit_test::test_unit* res
            = new boost::unit_test::test_case
                (this_name, boost::bind(m_TestFunc, *m_ParBegin));
#else
        boost::unit_test::ut_detail::test_func_with_bound_param<ParamType>
                                    bound_test_func( m_TestFunc, *m_ParBegin );
        boost::unit_test::test_unit* res
                  = new boost::unit_test::test_case(this_name, bound_test_func);
#endif
        ++m_ParBegin;

        return res;
    }

private:
    // Data members
    TTestFunc         m_TestFunc;
    string            m_Name;
    mutable ParamIter m_ParBegin;
    ParamIter         m_ParEnd;
    mutable int       m_CaseIndex;
};


/// Helper functions to be used in BOOST_PARAM_TEST_CASE macro to create
/// special test case generator.
template<typename ParamType, typename ParamIter>
inline CNcbiTestParamTestCaseGenerator<ParamType, ParamIter>
NcbiTestGenTestCases(typename CNcbiTestParamTestCaseGenerator<ParamType, ParamIter>::TTestFunc const& test_func,
                     boost::unit_test::const_string                name,
                     ParamIter                                     par_begin,
                     ParamIter                                     par_end)
{
    return CNcbiTestParamTestCaseGenerator<ParamType, ParamIter>(
                                        test_func, name, par_begin, par_end);
}

template<typename ParamType, typename ParamIter>
inline CNcbiTestParamTestCaseGenerator<
                    typename boost::remove_const<
                            typename boost::remove_reference<ParamType>::type
                                                >::type, ParamIter>
NcbiTestGenTestCases(void (*test_func)(ParamType),
                     boost::unit_test::const_string name,
                     ParamIter                      par_begin,
                     ParamIter                      par_end )
{
    typedef typename boost::remove_const<
                        typename boost::remove_reference<ParamType>::type
                                        >::type             param_value_type;
    return CNcbiTestParamTestCaseGenerator<param_value_type, ParamIter>(
                                        test_func, name, par_begin, par_end);
}


END_NCBI_SCOPE


/* @} */

#endif  /* CORELIB___TEST_BOOST__HPP */
