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
 * File Description:
 *   Implementation of special reporter for Boost.Test framework and utility
 *   functions for embedding it into the Boost.
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbicfg.h>
#include <corelib/error_codes.hpp>
#include <corelib/ncbienv.hpp>
#include <corelib/ncbimisc.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbi_system.hpp>
#include <corelib/ncbi_safe_static.hpp>

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

#ifdef NCBI_COMPILER_MSVC
#  pragma warning(pop)
#endif

#include <list>
#include <vector>
#include <set>
#include <map>
#include <string>


#define NCBI_USE_ERRCODE_X  Corelib_TestBoost


namespace but = boost::unit_test;


BEGIN_NCBI_SCOPE

const char* kTestsDisableSectionName = "UNITTESTS_DISABLE";
const char* kTestsToFixSectionName = "UNITTESTS_TOFIX";
const char* kTestsTimeoutSectionName = "UNITTESTS_TIMEOUT_MULT";
const char* kTestConfigGlobalValue = "GLOBAL";

#define DUMMY_TEST_FUNCTION_NAME  DummyTestFunction
const char* kDummyTestCaseName    = BOOST_STRINGIZE(DUMMY_TEST_FUNCTION_NAME);

const char* kTestResultPassed      = "passed";
const char* kTestResultFailed      = "failed";
const char* kTestResultTimeout     = "timeout";
const char* kTestResultAborted     = "aborted";
const char* kTestResultSkipped     = "skipped";
const char* kTestResultDisabled    = "disabled";
const char* kTestResultToFix       = "tofix";


typedef but::results_reporter::format   TBoostRepFormatter;
typedef but::unit_test_log_formatter    TBoostLogFormatter;
typedef set<but::test_unit*>            TUnitsSet;
typedef map<but::test_unit*, TUnitsSet> TUnitToManyMap;
typedef map<string, but::test_unit*>    TStringToUnitMap;


/// Reporter for embedding in Boost framework and adding non-standard
/// information to detailed report given by Boost.
class CNcbiBoostReporter : public TBoostRepFormatter
{
public:
    CNcbiBoostReporter(void);
    virtual ~CNcbiBoostReporter(void) {}

    /// Setup reporter tuned for printing report of specific format
    ///
    /// @param format
    ///   Format of the report
    void SetOutputFormat(but::output_format format);

    // TBoostRepFormatter interface
    virtual
    void results_report_start   (ostream& ostr);
    virtual
    void results_report_finish  (ostream& ostr);
    virtual
    void test_unit_report_start (but::test_unit const& tu, ostream& ostr);
    virtual
    void test_unit_report_finish(but::test_unit const& tu, ostream& ostr);
    virtual
    void do_confirmation_report (but::test_unit const& tu, ostream& ostr);

private:
    /// Standard reporter from Boost for particular report format
    AutoPtr<TBoostRepFormatter>  m_Upper;
    /// If report is XML or not
    bool                         m_IsXML;
    /// Current indentation level in plain text report
    int                          m_Indent;
};


/// Logger for embedding in Boost framework and adding non-standard
/// information to logging given by Boost.
class CNcbiBoostLogger : public TBoostLogFormatter
{
public:
    CNcbiBoostLogger(void);
    virtual ~CNcbiBoostLogger(void) {}

    /// Setup logger tuned for printing log of specific format
    ///
    /// @param format
    ///   Format of the report
    void SetOutputFormat(but::output_format format);

    // TBoostLogFormatter interface
    virtual
    void log_start        (ostream& ostr, but::counter_t test_cases_amount);
    virtual
    void log_finish       (ostream& ostr);
    virtual
    void log_build_info   (ostream& ostr);
    virtual
    void test_unit_start  (ostream& ostr, but::test_unit const& tu);
    virtual
    void test_unit_finish (ostream& ostr, but::test_unit const& tu,
                                          unsigned long elapsed);
    virtual
    void test_unit_skipped(ostream& ostr, but::test_unit const& tu);
#if BOOST_VERSION >= 104200
    virtual
    void log_exception    (ostream& ostr, but::log_checkpoint_data const& lcd,
                                          boost::execution_exception const& ex);
    // Next line is necessary for compiling with ICC and Boost 1.41.0 and up
    using TBoostLogFormatter::log_exception;
#else
    virtual
    void log_exception    (ostream& ostr, but::log_checkpoint_data const& lcd,
                                          but::const_string explanation);
#endif
    virtual
    void log_entry_start  (ostream& ostr, but::log_entry_data const& led,
                                          log_entry_types let);
    virtual
    void log_entry_value  (ostream& ostr, but::const_string value);
    // Next line is necessary for compiling with ICC and Boost 1.41.0 and up
    using TBoostLogFormatter::log_entry_value;
    virtual
    void log_entry_finish (ostream& ostr);

private:
    /// Standard logger from Boost for particular report format
    AutoPtr<TBoostLogFormatter>  m_Upper;
    /// If report is XML or not
    bool                         m_IsXML;
};


/// Special observer to embed in Boost.Test framework to initialize test
/// dependencies before they started execution.
class CNcbiTestsObserver : public but::test_observer
{
public:
    virtual ~CNcbiTestsObserver(void) {}

    /// Method called before execution of all tests
    virtual void test_start(but::counter_t /* test_cases_amount */);

    /// Method called after execution of all tests
    virtual void test_finish(void);

    /// Method called before execution of each unit
    virtual void test_unit_start(but::test_unit const& tu);

    /// Method called after execution of each unit
    virtual void test_unit_finish(but::test_unit const& tu,
                                  unsigned long         elapsed);

    /// Method called when some exception was caught during execution of unit
    virtual void exception_caught(boost::execution_exception const& ex);

    virtual void test_unit_aborted(but::test_unit const& tu);
    virtual void assertion_result(bool passed);
};


/// Class that can walk through all tree of tests and register them inside
/// CNcbiTestApplication.
class CNcbiTestsCollector : public but::test_tree_visitor
{
public:
    virtual ~CNcbiTestsCollector(void) {}

    virtual void visit           (but::test_case  const& test );
    virtual bool test_suite_start(but::test_suite const& suite);
};


/// Element of tests tree. Used to make proper order between units to ensure
/// that dependencies are executed earlier than dependents.
class CNcbiTestTreeElement
{
public:
    /// Element represents one test unit
    CNcbiTestTreeElement(but::test_unit* tu);
    /// In destructor class destroys all its children
    ~CNcbiTestTreeElement(void);

    /// Get unit represented by the element
    but::test_unit* GetTestUnit(void);

    /// Add child element. Class acquires ownership on the child element and
    /// destroys it at the end of work.
    void AddChild(CNcbiTestTreeElement* element);

    /// Get parent element in tests tree. If this element represents master
    /// test suite then return NULL.
    CNcbiTestTreeElement* GetParent(void);

    /// Ensure good dependency of this element on "from" element. If
    /// dependency is not fulfilled well then ensure that "from" element will
    /// stand earlier in tests tree. Correct order is made in internal
    /// structures only. To make it in Boost tests tree you need to call
    /// FixUnitsOrder().
    ///
    /// @sa  FixUnitsOrder()
    void EnsureDep(CNcbiTestTreeElement* from);

    /// Fix order of unit tests in the subtree rooted in this element. Any
    /// action is taken only if during calls to EnsureDep() some wrong order
    /// was found.
    ///
    /// @sa EnsureDep()
    void FixUnitsOrder(void);

private:
    /// Prohibit
    CNcbiTestTreeElement(const CNcbiTestTreeElement&);
    CNcbiTestTreeElement& operator= (const CNcbiTestTreeElement&);

    typedef vector<CNcbiTestTreeElement*> TElemsList;
    typedef set   <CNcbiTestTreeElement*> TElemsSet;

    /// Ensure that leftElem and rightElem (or element pointed by it_right
    /// inside m_Children) are in that very order: leftElem first, rightElem
    /// after that. leftElem and rightElem should be children of this element.
    void x_EnsureChildOrder(CNcbiTestTreeElement* leftElem,
                            CNcbiTestTreeElement* rightElem);
    void x_EnsureChildOrder(CNcbiTestTreeElement* leftElem,
                            size_t                idx_right);

    /// Add leftElem (rightElem) in the list of elements that should be
    /// "lefter" ("righter") in the tests tree.
    void x_AddToMustLeft(CNcbiTestTreeElement* elem,
                         CNcbiTestTreeElement* leftElem);
    void x_AddToMustRight(CNcbiTestTreeElement* elem,
                          CNcbiTestTreeElement* rightElem);


    /// Parent element in tests tree
    CNcbiTestTreeElement* m_Parent;
    /// Unit represented by the element
    but::test_unit*       m_TestUnit;
    /// If order of children was changed during checking dependencies
    bool                  m_OrderChanged;
    /// Children of the element in tests tree
    TElemsList            m_Children;
    /// Elements that should be "on the left" from this element in tests tree
    /// (should have less index in the parent's list of children).
    TElemsSet             m_MustLeft;
    /// Elements that should be "on the right" from this element in tests tree
    /// (should have greater index in the parent's list of children).
    TElemsSet             m_MustRight;
};


/// Class for traversing all Boost tests tree and building tree structure in
/// our own accessible manner.
class CNcbiTestsTreeBuilder : public but::test_tree_visitor
{
public:
    CNcbiTestsTreeBuilder(void);
    virtual ~CNcbiTestsTreeBuilder(void);

    virtual void visit            (but::test_case  const& test );
    virtual bool test_suite_start (but::test_suite const& suite);
    virtual void test_suite_finish(but::test_suite const& suite);

    /// Ensure good dependency of the tu test unit on tu_from test unit. If
    /// dependency is not fulfilled well then ensure that tu_from element will
    /// stand earlier in tests tree. Correct order is made in internal
    /// structures only. To make it in Boost tests tree you need to call
    /// FixUnitsOrder().
    ///
    /// @sa  FixUnitsOrder()
    void EnsureDep(but::test_unit* tu, but::test_unit* tu_from);

    /// Fix order of unit tests in the whole tree of tests. Any action is
    /// taken only if during calls to EnsureDep() some wrong order was found.
    ///
    /// @sa EnsureDep()
    void FixUnitsOrder(void);

private:
    typedef map<but::test_unit*, CNcbiTestTreeElement*> TUnitToElemMap;

    /// Root element of the tests tree
    CNcbiTestTreeElement* m_RootElem;
    /// Element in tests tree representing started but not yet finished test
    /// suite, i.e. all test cases that will be visited now will for sure be
    /// from this test suite.
    CNcbiTestTreeElement* m_CurElem;
    /// Overall map of relations between test units and their representatives
    /// in elements tree.
    TUnitToElemMap        m_AllUnits;
};


/// Application for all unit tests
class CNcbiTestApplication : public CNcbiApplication
{
public:
    CNcbiTestApplication(void);
    ~CNcbiTestApplication(void);

    virtual void Init  (void);
    virtual int  Run   (void);
    virtual int  DryRun(void);

    /// Add user function
    void AddUserFunction(TNcbiTestUserFunction func,
                         ETestUserFuncType     func_type);
    /// Add dependency for test unit
    void AddTestDependsOn(but::test_unit* tu, but::test_unit* dep_tu);
    /// Set test as disabled by user
    void SetTestDisabled(but::test_unit* tu);
    /// Set flag that all tests globally disabled
    void SetGloballyDisabled(void);
    /// Set flag that all tests globally skipped
    void SetGloballySkipped(void);

    /// Initialize this application, main test suite and all test framework
    but::test_suite* InitTestFramework(int argc, char* argv[]);
    /// Get object with argument descriptions.
    /// Return NULL if it is not right time to fill in descriptions.
    CArgDescriptions* GetArgDescrs(void);
    /// Get parser evaluating configuration conditions.
    /// Return NULL if it is not right time to deal with the parser.
    CExprParser* GetIniParser(void);

    /// Save test unit in the collection of all tests.
    void CollectTestUnit(but::test_unit* tu);
    /// Get pointer to test case or test suite by its name.
    but::test_unit* GetTestUnit(CTempString test_name);
    /// Initialize already prepared test suite before running tests
    void InitTestsBeforeRun(void);
    /// Finalize test suite after running tests
    void FiniTestsAfterRun(void);
    /// Enable all necessary tests after execution but before printing report
    void ReEnableAllTests(void);
    /// Check the correct setting for unit timeout and check overall
    /// test timeout.
    void AdjustTestTimeout(but::test_unit* tu);
    /// Mark test case as failed due to hit of the timeout
    void SetTestTimedOut(but::test_case* tc);
    /// Register the fact of test failure
    void SetTestErrored(but::test_case* tc);
    /// Check if given test is marked as requiring fixing in the future
    bool IsTestToFix(const but::test_unit* tu);

    /// Get number of actually executed tests
    int GetRanTestsCount(void);
    /// Get number of tests that were failed but are marked to be fixed
    int GetToFixTestsCount(void);
    /// Get string representation of result of test execution
    string GetTestResultString(but::test_unit* tu);
    /// Get pointer to empty test case added to Boost for internal purposes
    but::test_case* GetDummyTest(void);
    /// Check if user initialization functions failed
    bool IsInitFailed(void);

    /// Check if there were any test errors
    bool HasTestErrors(void);
    /// Check if there were any timeouted tests
    bool HasTestTimeouts(void);

private:
    typedef list<TNcbiTestUserFunction> TUserFuncsList;


    /// Setup our own reporter for Boost.Test
    void x_SetupBoostReporters(void);
    /// Call all user functions. Return TRUE if functions execution is
    /// successful and FALSE if come function thrown exception.
    bool x_CallUserFuncs(ETestUserFuncType func_type);
    /// Ensure that all dependencies stand earlier in tests tree than their
    /// dependents.
    void x_EnsureAllDeps(void);
    /// Set up real Boost.Test dependencies based on ones made by
    /// AddTestDependsOn().
    ///
    /// @sa AddTestDependsOn()
    void x_ActualizeDeps(void);
    /// Enable / disable tests based on application configuration file
    bool x_ReadConfiguration(void);
    /// Get number of tests which Boost will execute
    int x_GetEnabledTestsCount(void);
    /// Add empty test necesary for internal purposes
    void x_AddDummyTest(void);
    /// Initialize common for all tests parser variables
    /// (OS*, COMPILER* and DLL_BUILD)
    void x_InitCommonParserVars(void);
    /// Apply standard trimmings to test name and return resultant test name
    /// which will identify test inside the framework.
    string x_GetTrimmedTestName(const string& test_name);
    /// Enable / disable all tests known to application
    void x_EnableAllTests(bool enable);
    /// Collect names and pointers to all tests existing in master test suite
    void x_CollectAllTests();
    /// Calculate the value from configuration file
    bool x_CalcConfigValue(const string& value);


    /// Mode of running testing application
    enum ERunMode {
        fTestList   = 0x1,  ///< Only tests list is requested
        fDisabled   = 0x2,  ///< All tests are disabled in configuration file
        fInitFailed = 0x4   ///< Initialization user functions failed
    };
    typedef unsigned int TRunMode;


    /// If Run() was called or not
    ///
    /// @sa Run()
    bool                      m_RunCalled;
    /// Mode of running the application
    TRunMode                  m_RunMode;
    /// Lists of all user-defined functions
    TUserFuncsList            m_UserFuncs[eTestUserFuncLast
                                            - eTestUserFuncFirst + 1];
    /// Argument descriptions to be passed to SetArgDescriptions().
    /// Value is not null only during NCBITEST_INIT_CMDLINE() function
    AutoPtr<CArgDescriptions> m_ArgDescrs;
    /// Parser to evaluate expressions in configuration file.
    /// Value is not null only during NCBITEST_INIT_VARIABLES() function
    AutoPtr<CExprParser>      m_IniParser;
    /// List of all test units mapped to their names.
    TStringToUnitMap          m_AllTests;
    /// List of all disabled tests
    TUnitsSet                 m_DisabledTests;
    /// List of all tests which result is a timeout
    TUnitsSet                 m_TimedOutTests;
    /// List of all tests marked as in need of fixing in the future
    TUnitsSet                 m_ToFixTests;
    /// List of all dependencies for each test having dependencies
    TUnitToManyMap            m_TestDeps;
    /// Observer to make test dependencies and look for unit's timeouts
    CNcbiTestsObserver        m_Observer;
    /// Boost reporter - must be pointer because Boost.Test calls free() on it
    CNcbiBoostReporter*       m_Reporter;
    /// Boost logger - must be pointer because Boost.Test calls free() on it
    CNcbiBoostLogger*         m_Logger;
    /// Output stream for Boost.Test report
    ofstream                  m_ReportOut;
    /// Builder of internal accessible from library tests tree
    CNcbiTestsTreeBuilder     m_TreeBuilder;
    /// Empty test case added to Boost for internal perposes
    but::test_case*           m_DummyTest;
    /// Timeout for the whole test
    double                    m_Timeout;
    /// String representation for whole test timeout (real value taken from
    /// CHECK_TIMEOUT in Makefile).
    string                    m_TimeoutStr;
    /// Multiplicator for timeouts
    double                    m_TimeMult;
    /// Timer measuring elapsed time for the whole test
    CStopWatch                m_Timer;
    /// Timeout that was set in currently executing unit before adjustment
    ///
    /// @sa AdjustTestTimeout()
    unsigned int              m_CurUnitTimeout;
    /// Flag showing if there were some test errors
    bool                      m_HasTestErrors;
    /// Flag showing if there were some timeouted tests
    bool                      m_HasTestTimeouts;
};


inline
CNcbiBoostReporter::CNcbiBoostReporter()
    : m_IsXML(false)
{}

inline void
CNcbiBoostReporter::SetOutputFormat(but::output_format format)
{
    if (format == but::XML) {
        m_IsXML = true;
        m_Upper = new but::output::xml_report_formatter();
    }
    else {
        m_IsXML = false;
        m_Upper = new but::output::plain_report_formatter();
    }
}


inline
CNcbiBoostLogger::CNcbiBoostLogger(void)
: m_IsXML(false)
{}

inline void
CNcbiBoostLogger::SetOutputFormat(but::output_format format)
{
    if (format == but::XML) {
        m_IsXML = true;
        m_Upper = new but::output::xml_log_formatter();
    }
    else {
        m_IsXML = false;
        m_Upper = new but::output::compiler_log_formatter();
    }
}


inline
CNcbiTestTreeElement::CNcbiTestTreeElement(but::test_unit* tu)
    : m_Parent      (NULL),
      m_TestUnit    (tu),
      m_OrderChanged(false)
{}

CNcbiTestTreeElement::~CNcbiTestTreeElement(void)
{
    ITERATE(TElemsList, it, m_Children) {
        delete *it;
    }
}

inline void
CNcbiTestTreeElement::AddChild(CNcbiTestTreeElement* element)
{
    m_Children.push_back(element);
    element->m_Parent = this;
}

void
CNcbiTestTreeElement::x_EnsureChildOrder(CNcbiTestTreeElement* leftElem,
                                         size_t                idx_right)
{
    size_t idx_left = 0;
    for (; idx_left < m_Children.size(); ++ idx_left) {
        if (m_Children[idx_left] == leftElem)
            break;
    }
    _ASSERT(idx_left < m_Children.size());

    if (idx_left < idx_right)
        return;

    m_OrderChanged = true;
    m_Children.erase(m_Children.begin() + idx_left);
    m_Children.insert(m_Children.begin() + idx_right, leftElem);

    ITERATE(TElemsSet, it, leftElem->m_MustLeft) {
        x_EnsureChildOrder(*it, idx_right);
        // If order is changed in the above call then leftElem will move to
        // the right and we need to change our index.
        while (m_Children[idx_right] != leftElem)
            ++idx_right;
    }
}

void
CNcbiTestTreeElement::x_AddToMustLeft(CNcbiTestTreeElement* elem,
                                      CNcbiTestTreeElement* leftElem)
{
    if (elem == leftElem) {
        NCBI_THROW(CCoreException, eCore,
                   FORMAT("Circular dependency found: '"
                          << elem->m_TestUnit->p_name.get()
                          << "' must depend on itself."));
    }

    elem->m_MustLeft.insert(leftElem);

    ITERATE(TElemsSet, it, elem->m_MustRight) {
        x_AddToMustLeft(*it, leftElem);
    }
}

void
CNcbiTestTreeElement::x_AddToMustRight(CNcbiTestTreeElement* elem,
                                       CNcbiTestTreeElement* rightElem)
{
    if (elem == rightElem) {
        NCBI_THROW(CCoreException, eCore,
                   FORMAT("Circular dependency found: '"
                          << elem->m_TestUnit->p_name.get()
                          << "' must depend on itself."));
    }

    elem->m_MustRight.insert(rightElem);

    ITERATE(TElemsSet, it, elem->m_MustLeft) {
        x_AddToMustRight(*it, rightElem);
    }
}

inline void
CNcbiTestTreeElement::x_EnsureChildOrder(CNcbiTestTreeElement* leftElem,
                                         CNcbiTestTreeElement* rightElem)
{
    x_AddToMustLeft(rightElem, leftElem);
    x_AddToMustRight(leftElem, rightElem);

    size_t idx_right = 0;
    for (; idx_right < m_Children.size(); ++idx_right) {
        if (m_Children[idx_right] == rightElem)
            break;
    }
    _ASSERT(idx_right < m_Children.size());

    x_EnsureChildOrder(leftElem, idx_right);
}

void
CNcbiTestTreeElement::EnsureDep(CNcbiTestTreeElement* from)
{
    TElemsList parents;

    CNcbiTestTreeElement* parElem = this;
    if (m_TestUnit->p_type != but::tut_suite) {
        parElem = m_Parent;
    }
    do {
        parents.push_back(parElem);
        parElem = parElem->m_Parent;
    }
    while (parElem != NULL);

    parElem = from;
    CNcbiTestTreeElement* fromElem = from;
    do {
        TElemsList::iterator it = find(parents.begin(), parents.end(), parElem);
        if (it != parents.end()) {
            break;
        }
        fromElem = parElem;
        parElem  = parElem->m_Parent;
    }
    while (parElem != NULL);
    _ASSERT(parElem);

    if (parElem == this) {
        NCBI_THROW(CCoreException, eCore,
                   FORMAT("Error in unit tests setup: dependency of '"
                          << m_TestUnit->p_name.get() << "' from '"
                          << from->m_TestUnit->p_name.get()
                          << "' can never be implemented."));
    }

    CNcbiTestTreeElement* toElem = this;
    while (toElem->m_Parent != parElem) {
        toElem = toElem->m_Parent;
    }

    parElem->x_EnsureChildOrder(fromElem, toElem);
}

void
CNcbiTestTreeElement::FixUnitsOrder(void)
{
    if (m_OrderChanged) {
        but::test_suite* suite = static_cast<but::test_suite*>(m_TestUnit);
        ITERATE(TElemsList, it, m_Children) {
            suite->remove((*it)->m_TestUnit->p_id);
        }
        ITERATE(TElemsList, it, m_Children) {
            suite->add((*it)->m_TestUnit);
        }
    }

    ITERATE(TElemsList, it, m_Children) {
        (*it)->FixUnitsOrder();
    }
}

inline but::test_unit*
CNcbiTestTreeElement::GetTestUnit(void)
{
    return m_TestUnit;
}

inline CNcbiTestTreeElement*
CNcbiTestTreeElement::GetParent(void)
{
    return m_Parent;
}


CNcbiTestsTreeBuilder::CNcbiTestsTreeBuilder(void)
    : m_RootElem(NULL),
      m_CurElem (NULL)
{}

CNcbiTestsTreeBuilder::~CNcbiTestsTreeBuilder(void)
{
    delete m_RootElem;
}

bool
CNcbiTestsTreeBuilder::test_suite_start(but::test_suite const& suite)
{
    but::test_suite* nc_suite = const_cast<but::test_suite*>(&suite);
    if (m_RootElem) {
        CNcbiTestTreeElement* next_elem = new CNcbiTestTreeElement(nc_suite);
        m_CurElem->AddChild(next_elem);
        m_CurElem = next_elem;
    }
    else {
        m_RootElem = new CNcbiTestTreeElement(nc_suite);
        m_CurElem  = m_RootElem;
    }

    m_AllUnits[nc_suite] = m_CurElem;

    return true;
}

void
CNcbiTestsTreeBuilder::test_suite_finish(but::test_suite const& suite)
{
    _ASSERT(m_CurElem->GetTestUnit()
                            == &static_cast<const but::test_unit&>(suite));
    m_CurElem = m_CurElem->GetParent();
}

void
CNcbiTestsTreeBuilder::visit(but::test_case const& test)
{
    but::test_case* nc_test = const_cast<but::test_case*>(&test);
    CNcbiTestTreeElement* elem = new CNcbiTestTreeElement(nc_test);
    m_CurElem->AddChild(elem);
    m_AllUnits[nc_test] = elem;
}

inline void
CNcbiTestsTreeBuilder::EnsureDep(but::test_unit* tu, but::test_unit* tu_from)
{
    CNcbiTestTreeElement* elem = m_AllUnits[tu];
    CNcbiTestTreeElement* elem_from = m_AllUnits[tu_from];
    _ASSERT(elem  &&  elem_from);

    elem->EnsureDep(elem_from);
}

inline void
CNcbiTestsTreeBuilder::FixUnitsOrder(void)
{
    m_RootElem->FixUnitsOrder();
}


static CNcbiTestApplication* s_TestApp = NULL;


inline
CNcbiTestApplication::CNcbiTestApplication(void)
    : m_RunCalled(false),
      m_RunMode  (0),
      m_DummyTest(NULL),
      m_Timeout  (0),
      m_TimeMult (1),
      m_Timer    (CStopWatch::eStart),
      m_HasTestErrors(false),
      m_HasTestTimeouts(false)
{
    m_Reporter = new CNcbiBoostReporter();
    m_Logger   = new CNcbiBoostLogger();

    // Do not show warning about inaccessible configuration file
    SetDiagFilter(eDiagFilter_Post, "!(106.11)");
}

CNcbiTestApplication::~CNcbiTestApplication(void)
{
    if (m_ReportOut.good())
        but::results_reporter::set_stream(cerr);
}

/// Application for unit tests
static CNcbiTestApplication&
s_GetTestApp(void)
{
    if (!s_TestApp)
        s_TestApp = new CNcbiTestApplication();
    return *s_TestApp;
}

void
CNcbiTestApplication::Init(void)
{
    m_ArgDescrs = new CArgDescriptions();
    m_ArgDescrs->AddFlag("-help",
         "Print test framework related command line arguments");
#ifndef NCBI_COMPILER_WORKSHOP
    m_ArgDescrs->AddOptionalKey("-run_test", "Filter",
         "Allows to filter which test units to run",
         CArgDescriptions::eString, CArgDescriptions::fMandatorySeparator);
#endif
    m_ArgDescrs->AddFlag("dryrun",
                         "Do not actually run tests, "
                         "just print list of all available tests.");
    m_ArgDescrs->SetUsageContext(GetArguments().GetProgramBasename(),
                                 "NCBI unit test");
    if (!m_UserFuncs[eTestUserFuncCmdLine].empty())
        x_CallUserFuncs(eTestUserFuncCmdLine);
    SetupArgDescriptions(m_ArgDescrs.release());
}

int
CNcbiTestApplication::Run(void)
{
    m_RunCalled = true;
    return 0;
}

int
CNcbiTestApplication::DryRun(void)
{
    m_RunCalled = true;
    m_RunMode |= fTestList;
    but::results_reporter::set_level(but::DETAILED_REPORT);
    return 0;
}

inline void
CNcbiTestApplication::AddUserFunction(TNcbiTestUserFunction func,
                                      ETestUserFuncType     func_type)
{
    m_UserFuncs[func_type].push_back(func);
}

inline void
CNcbiTestApplication::AddTestDependsOn(but::test_unit* tu,
                                       but::test_unit* dep_tu)
{
    m_TestDeps[tu].insert(dep_tu);
}

inline void
CNcbiTestApplication::SetTestDisabled(but::test_unit* tu)
{
    if (but::runtime_config::test_to_run().empty()) {
        tu->p_enabled.set(false);
        m_DisabledTests.insert(tu);
    }
}

inline CArgDescriptions*
CNcbiTestApplication::GetArgDescrs(void)
{
    return m_ArgDescrs.get();
}

inline CExprParser*
CNcbiTestApplication::GetIniParser(void)
{
    return m_IniParser.get();
}

inline but::test_case*
CNcbiTestApplication::GetDummyTest(void)
{
    return m_DummyTest;
}

inline bool
CNcbiTestApplication::IsInitFailed(void)
{
    return (m_RunMode & fInitFailed) != 0;
}

string
CNcbiTestApplication::x_GetTrimmedTestName(const string& test_name)
{
    string new_name = test_name;
    SIZE_TYPE pos = NStr::FindCase(new_name, "::", 0, new_name.size(),
                                   NStr::eLast);
    if (pos != NPOS) {
        new_name = new_name.substr(pos + 2);
    }

    if(NStr::StartsWith(new_name, "test_", NStr::eNocase)) {
        new_name = new_name.substr(5);
    }
    else if(NStr::StartsWith(new_name, "test", NStr::eNocase)) {
        new_name = new_name.substr(4);
    }

    return new_name;
}

inline void
CNcbiTestApplication::CollectTestUnit(but::test_unit* tu)
{
    const string unit_name = x_GetTrimmedTestName(tu->p_name.get());
    if (unit_name == kDummyTestCaseName)
        return;
    string test_name(unit_name);
    int index = 0;
    for (;;) {
        but::test_unit*& tu_val = m_AllTests[test_name];
        if (!tu_val) {
            tu_val = tu;
            if (test_name != unit_name) {
                LOG_POST_X(3, Info << "Duplicate name found: '" << unit_name
                                   << "' - renamed to '" << test_name << "'");
                tu->p_name.set(test_name);
            }
            break;
        }
        test_name = unit_name;
        test_name += "_";
        test_name += NStr::IntToString(++index);
    }
}

inline void
CNcbiTestApplication::x_EnsureAllDeps(void)
{
    ITERATE(TUnitToManyMap, it, m_TestDeps) {
        but::test_unit* test = it->first;
        ITERATE(TUnitsSet, dep_it, it->second) {
            but::test_unit* dep_test = *dep_it;
            m_TreeBuilder.EnsureDep(test, dep_test);
        }
    }

    m_TreeBuilder.FixUnitsOrder();
}

inline void
CNcbiTestApplication::x_ActualizeDeps(void)
{
    ITERATE(TUnitToManyMap, it, m_TestDeps) {
        but::test_unit* test = it->first;
        if (!m_DisabledTests.count(test) && !test->p_enabled) {
            continue;
        }

        ITERATE(TUnitsSet, dep_it, it->second) {
            but::test_unit* dep_test = *dep_it;
            if (!m_DisabledTests.count(dep_test) && !dep_test->p_enabled) {
                continue;
            }

            test->depends_on(dep_test);
        }
    }
}

/// Helper macro to check if NCBI preprocessor flag was defined empty or
/// equal to 1.
/// Macro expands to true if flag was defined empty or equal to 1 and to false
/// if it was defined to something else or wasn't defined at all.
#define IS_FLAG_DEFINED(flag)                                 \
    BOOST_PP_TUPLE_ELEM(2, 1, IS_FLAG_DEFINED_I(BOOST_PP_CAT(NCBI_, flag)))
#define IS_VAR_DEFINED(var)                                   \
    BOOST_PP_TUPLE_ELEM(2, 1, IS_FLAG_DEFINED_I(var))
#define IS_FLAG_DEFINED_I(flag)                               \
    (BOOST_PP_CAT(IS_FLAG_DEFINED_II_, flag) (), false)
#define IS_FLAG_DEFINED_II_()                                 \
    BOOST_PP_NIL, true) BOOST_PP_TUPLE_EAT(2) (BOOST_PP_NIL
#define IS_FLAG_DEFINED_II_1()                                \
    BOOST_PP_NIL, true) BOOST_PP_TUPLE_EAT(2) (BOOST_PP_NIL

/// List of features that will be converted to unittest variables
/// (checking testsuite environment variable $FEATURES).
/// If you would like to add some new veriables here, please
/// see Unix configure utility and Project Tree Builder for full
/// list of supported values.
/// @note
///    All non alphanumeric charecters in the names replaced with "_" symbol.
static const char* s_NcbiFeatures[] = {
    // Features 
    "AIX",
    "BSD",
    "CompaqCompiler",
    "Cygwin",
    "CygwinMT",
    "DLL",
    "DLL_BUILD",
    "Darwin",
    "GCC",
    "ICC",
    "IRIX",
    "KCC",
    "Linux",
    "MIPSpro",
    "MSVC",
    "MSWin",
    "MT",
    "MacOS",
    "Ncbi_JNI",            // Ncbi-JNI
    "OSF",
    "PubSeqOS",
    "SRAT_internal",       // SRAT-internal
    "Solaris",
    "VisualAge",
    "WinMain",
    "WorkShop",
    "XCODE",
    "in_house_resources",  // in-house-resources
    "unix",

    // Packages
    "BZ2",
    "BerkeleyDB",
    "BerkeleyDB__",        // BerkeleyDB++
    "Boost_Regex",         // Boost.Regex
    "Boost_Spirit",        // Boost.Spirit
    "Boost_Test",          // Boost.Test
    "Boost_Test_Included", // Boost.Test.Included
    "Boost_Threads",       // Boost.Threads
    "C_Toolkit",           // C-Toolkit
    "CPPUNIT",
    "C_ncbi",
    "DBLib",
    "EXPAT",
    "FLTK",
    "FUSE",
    "Fast_CGI",            // Fast-CGI
    "FreeTDS",
    "FreeType",
    "GIF",
    "GLUT",
    "GNUTLS",
    "HDF5",
    "ICU",
    "JPEG",
    "LIBXML",
    "LIBXSLT",
    "LZO",
    "LocalBZ2",
    "LocalMSGMAIL2",
    "LocalNCBILS",
    "LocalPCRE",
    "LocalSSS",
    "LocalZ",
    "MAGIC",
    "MESA",
    "MUPARSER",
    "MySQL",
    "NCBILS2",
    "ODBC",
    "OECHEM",
    "OPENSSL",
    "ORBacus",
    "OpenGL",
    "PCRE",
    "PNG",
    "PYTHON",
    "PYTHON23",
    "PYTHON24",
    "PYTHON25",
    "SABLOT",
    "SGE",
    "SP",
    "SQLITE",
    "SQLITE3",
    "SQLITE3ASYNC",
    "SSSDB",
    "SSSUTILS",
    "Sybase",
    "SybaseCTLIB",
    "SybaseDBLIB",
    "TIFF",
    "UNGIF",
    "UUID",
    "XPM",
    "Xalan",
    "Xerces",
    "Z",
    "wx2_8",               // wx2.8
    "wxWidgets",
    "wxWindows",

    // Projects
    "algo",
    "app",
    "bdb",
    "cgi",
    "connext",
    "ctools",
    "dbapi",
    "gbench",
    "gui",
    "local_lbsm",
    "ncbi_crypt",
    "objects",
    "serial"
};


inline void
CNcbiTestApplication::x_InitCommonParserVars(void)
{
    m_IniParser->AddSymbol("COMPILER_Compaq",    IS_FLAG_DEFINED(COMPILER_COMPAQ));
    m_IniParser->AddSymbol("COMPILER_GCC",       IS_FLAG_DEFINED(COMPILER_GCC));
    m_IniParser->AddSymbol("COMPILER_ICC",       IS_FLAG_DEFINED(COMPILER_ICC));
    m_IniParser->AddSymbol("COMPILER_KCC",       IS_FLAG_DEFINED(COMPILER_KCC));
    m_IniParser->AddSymbol("COMPILER_MipsPro",   IS_FLAG_DEFINED(COMPILER_MIPSPRO));
    m_IniParser->AddSymbol("COMPILER_MSVC",      IS_FLAG_DEFINED(COMPILER_MSVC));
    m_IniParser->AddSymbol("COMPILER_VisualAge", IS_FLAG_DEFINED(COMPILER_VISUALAGE));
    m_IniParser->AddSymbol("COMPILER_WorkShop",  IS_FLAG_DEFINED(COMPILER_WORKSHOP));

    m_IniParser->AddSymbol("OS_AIX",             IS_FLAG_DEFINED(OS_AIX));
    m_IniParser->AddSymbol("OS_BSD",             IS_FLAG_DEFINED(OS_BSD));
    m_IniParser->AddSymbol("OS_Cygwin",          IS_FLAG_DEFINED(OS_CYGWIN));
    m_IniParser->AddSymbol("OS_MacOSX",          IS_FLAG_DEFINED(OS_DARWIN));
    m_IniParser->AddSymbol("OS_Irix",            IS_FLAG_DEFINED(OS_IRIX));
    m_IniParser->AddSymbol("OS_Linux",           IS_FLAG_DEFINED(OS_LINUX));
    m_IniParser->AddSymbol("OS_MacOS",           IS_FLAG_DEFINED(OS_MAC));
    m_IniParser->AddSymbol("OS_Windows",         IS_FLAG_DEFINED(OS_MSWIN));
    m_IniParser->AddSymbol("OS_Tru64",           IS_FLAG_DEFINED(OS_OSF1));
    m_IniParser->AddSymbol("OS_Solaris",         IS_FLAG_DEFINED(OS_SOLARIS));
    m_IniParser->AddSymbol("OS_Unix",            IS_FLAG_DEFINED(OS_UNIX));

    m_IniParser->AddSymbol("PLATFORM_Bits32",    NCBI_PLATFORM_BITS == 32);
    m_IniParser->AddSymbol("PLATFORM_Bits64",    NCBI_PLATFORM_BITS == 64);

    m_IniParser->AddSymbol("PLATFORM_BigEndian",     IS_VAR_DEFINED(WORDS_BIGENDIAN));
    m_IniParser->AddSymbol("PLATFORM_LittleEndian", !IS_VAR_DEFINED(WORDS_BIGENDIAN));

    m_IniParser->AddSymbol("BUILD_Dll",          IS_FLAG_DEFINED(DLL_BUILD));
    m_IniParser->AddSymbol("BUILD_Static",      !IS_FLAG_DEFINED(DLL_BUILD));

    m_IniParser->AddSymbol("BUILD_Debug",        IS_VAR_DEFINED(_DEBUG));
    m_IniParser->AddSymbol("BUILD_Release",     !IS_VAR_DEFINED(_DEBUG));


    // Add variables based on features available in the build

    string features_str = NCBI_GetBuildFeatures();
    if (features_str.empty()) {
        return;
    }
    // Split $FEATURES to tokens
    list<string> features_list;
    NStr::Split(features_str, " ", features_list);
    // Convert list<> to set<> to speed up a search
    typedef set<string> TFeatures;
    TFeatures features;
    // For all features
    ITERATE(list<string>, it, features_list) {
        // Replace all non alphanumeric characters in the names with "_".
        // Ignore negative features (with first "-" characters)
        string f = *it;
        if (f[0] != '-') {
            NON_CONST_ITERATE (string, fit, f) {
                if (!isalnum((unsigned char)(*fit))) {
                    *fit = '_';
                }
            }
            // Add feature name
            features.insert(f);
        }
    }
    // Add FEATURE_* variables
    for (size_t i = 0; i < sizeof(s_NcbiFeatures) / sizeof(s_NcbiFeatures[0]); i++) {
        string name("FEATURE_");
        name += s_NcbiFeatures[i];
        TFeatures::const_iterator it = features.find(s_NcbiFeatures[i]);
        bool found = (it != features.end());
        m_IniParser->AddSymbol(name.c_str(), found);
    }
}

inline bool
CNcbiTestApplication::x_CalcConfigValue(const string& value)
{
    m_IniParser->Parse(value.c_str());
    const CExprValue& expr_res = m_IniParser->GetResult();

    if (expr_res.GetType() == CExprValue::eBOOL  &&  !expr_res.GetBool())
        return false;

    return true;
}


void
DUMMY_TEST_FUNCTION_NAME(void)
{
    if (s_GetTestApp().IsInitFailed()) {
        but::results_collector.test_unit_aborted(
                                        *s_GetTestApp().GetDummyTest());
    }
}


void
CNcbiTestApplication::SetGloballyDisabled(void)
{
    m_RunMode |= fDisabled;

    // This should certainly go to the output. So we can use only printf,
    // nothing else.
    printf("All tests are disabled in current configuration.\n"
           " (for autobuild scripts: NCBI_UNITTEST_DISABLED)\n");
}

void
CNcbiTestApplication::SetGloballySkipped(void)
{
    m_RunMode |= fDisabled;

    // This should certainly go to the output. So we can use only printf,
    // nothing else.
    printf("Tests cannot be executed in current configuration "
                                                    "and will be skipped.\n"
           " (for autobuild scripts: NCBI_UNITTEST_SKIPPED)\n");
}

inline void
CNcbiTestApplication::x_AddDummyTest(void)
{
    if (!m_DummyTest) {
        m_DummyTest = BOOST_TEST_CASE(&DUMMY_TEST_FUNCTION_NAME);
        but::framework::master_test_suite().add(m_DummyTest);
    }
}

inline bool
CNcbiTestApplication::x_ReadConfiguration(void)
{
    m_IniParser = new CExprParser(CExprParser::eDenyAutoVar);
    x_InitCommonParserVars();
    if (!x_CallUserFuncs(eTestUserFuncVars))
        return false;

    const IRegistry& registry = s_GetTestApp().GetConfig();
    list<string> reg_entries;
    registry.EnumerateEntries(kTestsDisableSectionName, &reg_entries);

    // Disable tests ...
    ITERATE(list<string>, it, reg_entries) {
        const string& test_name = *it;
        string reg_value = registry.Get(kTestsDisableSectionName, test_name);

        if (test_name == kTestConfigGlobalValue) {
            if (x_CalcConfigValue(reg_value)) {
                SetGloballyDisabled();
            }
            continue;
        }

        but::test_unit* tu = GetTestUnit(test_name);
        if (tu) {
            if (x_CalcConfigValue(reg_value)) {
                SetTestDisabled(tu);
            }
        }
        else {
            ERR_POST_X(2, Warning << "Invalid test case name: '"
                                  << test_name << "'");
        }
    }

    reg_entries.clear();
    registry.EnumerateEntries(kTestsToFixSectionName, &reg_entries);
    // Put tests into "to-fix" list
    ITERATE(list<string>, it, reg_entries) {
        const string& test_name = *it;
        string reg_value = registry.Get(kTestsToFixSectionName, test_name);

        but::test_unit* tu = GetTestUnit(test_name);
        if (tu) {
            if (x_CalcConfigValue(reg_value)) {
                m_ToFixTests.insert(tu);
            }
        }
        else {
            ERR_POST_X(4, Warning << "Invalid test case name: '"
                                  << test_name << "'");
        }
    }

    reg_entries.clear();
    registry.EnumerateEntries(kTestsTimeoutSectionName, &reg_entries);
    // Adjust timeouts of test units
    ITERATE(list<string>, it, reg_entries) {
        const string& test_name = *it;
        string reg_value = registry.Get(kTestsTimeoutSectionName, test_name);

        but::test_unit* tu = GetTestUnit(test_name);
        if (tu) {
            list<CTempString> koef_lst;
            NStr::Split(reg_value, ";", koef_lst);
            ITERATE(list<CTempString>, it_koef, koef_lst) {
                CTempString koef_str, koef_cond;
                if (NStr::SplitInTwo(*it_koef, ":", koef_str, koef_cond)) {
                    if (x_CalcConfigValue(koef_cond)) {
                        double koef = NStr::StringToDouble(koef_str,
                                                NStr::fAllowLeadingSpaces
                                                | NStr::fAllowTrailingSpaces);
                        tu->p_timeout.set(Uint4(tu->p_timeout.get() * koef));
                        break;
                    }
                }
                else {
                    ERR_POST_X(6, "Bad format of TIMEOUT_MULT string: '"
                                  << reg_value << "'");
                    break;
                }
            }
        }
        else {
            ERR_POST_X(5, Warning << "Invalid test case name: '"
                                  << test_name << "'");
        }
    }

    return true;
}

void
CNcbiTestApplication::x_EnableAllTests(bool enable)
{
    ITERATE(TStringToUnitMap, it, m_AllTests) {
        but::test_unit* tu = it->second;
        if (tu->p_type == but::tut_case) {
            tu->p_enabled.set(enable);

            /*
            For full correctness this functionality should exist but it
            can't be made now. So if test suite will be disabled by user
            then it will not be possible to get list of tests inside this
            suite to be included in the report.

            if (enable  &&  tu->p_type == but::tut_suite) {
                but::results_collector.results(tu->p_id).p_skipped = false;
            }
            */
        }
    }
}

inline void
CNcbiTestApplication::InitTestsBeforeRun(void)
{
    bool need_run = !(m_RunMode & (fTestList + fDisabled));
    if (need_run  &&  !x_CallUserFuncs(eTestUserFuncInit)) {
        m_RunMode |= fInitFailed;
        need_run = false;
    }
    // fDisabled property can be changed in initialization functions
    if (m_RunMode & fDisabled)
        need_run = false;

    if (need_run) {
        x_EnsureAllDeps();
        x_ActualizeDeps();
    }
    else {
        x_EnableAllTests(false);

        if (m_RunMode & fInitFailed) {
            x_AddDummyTest();
        }
    }
}

inline void
CNcbiTestApplication::FiniTestsAfterRun(void)
{
    x_CallUserFuncs(eTestUserFuncFini);
}

inline void
CNcbiTestApplication::ReEnableAllTests(void)
{
    x_EnableAllTests(true);

    // Disabled tests can accidentally become not included in full list if
    // they were disabled in initialization
    ITERATE(TUnitsSet, it, m_DisabledTests) {
        (*it)->p_enabled.set(true);
    }
}

inline void
CNcbiTestApplication::SetTestTimedOut(but::test_case* tc)
{
    // If equal then it's real timeout, if not then it's just this unit hit
    // the whole test timeout.
    if (tc->p_timeout.get() == m_CurUnitTimeout) {
        m_TimedOutTests.insert(tc);
    }
    m_HasTestTimeouts = true;
}

inline void
CNcbiTestApplication::SetTestErrored(but::test_case* tc)
{
    if (m_TimedOutTests.find(tc) == m_TimedOutTests.end())
        m_HasTestErrors = true;
}

void
CNcbiTestApplication::AdjustTestTimeout(but::test_unit* tu)
{
    m_CurUnitTimeout = tu->p_timeout.get();
    unsigned int new_timeout = (unsigned int)(m_CurUnitTimeout * m_TimeMult);

    if (m_Timeout != 0) {
        double elapsed = m_Timer.Elapsed();
        if (m_Timeout <= elapsed) {
            CNcbiEnvironment env;
            printf("Maximum execution time of %s seconds is exceeded",
                   m_TimeoutStr.c_str());
            throw but::test_being_aborted();
        }
        new_timeout = (unsigned int)(m_Timeout - elapsed);
    }
    if (m_CurUnitTimeout == 0  ||  m_CurUnitTimeout > new_timeout) {
        tu->p_timeout.set(new_timeout);
    }
}

string
CNcbiTestApplication::GetTestResultString(but::test_unit* tu)
{
    string result;
    const but::test_results& tr = but::results_collector.results(tu->p_id);

    if (m_DisabledTests.count(tu) != 0  ||  (m_RunMode & fDisabled))
        result = kTestResultDisabled;
    else if (m_TimedOutTests.count(tu) != 0)
        result = kTestResultTimeout;
    else if (!tr.passed()  &&  m_ToFixTests.find(tu) != m_ToFixTests.end())
        result = kTestResultToFix;
    else if (tr.p_aborted)
        result = kTestResultAborted;
    else if (tr.p_assertions_failed.get() > tr.p_expected_failures.get()
             ||  tr.p_test_cases_failed.get()
                        + tr.p_test_cases_aborted.get() > 0)
    {
        result = kTestResultFailed;
    }
    else if ((m_RunMode & fTestList)  ||  tr.p_skipped)
        result = kTestResultSkipped;
    else if( tr.passed() )
        result = kTestResultPassed;
    else
        result = kTestResultFailed;

    return result;
}

int
CNcbiTestApplication::GetRanTestsCount(void)
{
    int result = 0;
    ITERATE(TStringToUnitMap, it, m_AllTests) {
        but::test_unit* tu = it->second;
        if (tu->p_type.get() != but::tut_case)
            continue;

        string str = GetTestResultString(tu);
        if (str != kTestResultDisabled  &&  str != kTestResultSkipped)
            ++result;
    }
    return result;
}

int
CNcbiTestApplication::GetToFixTestsCount(void)
{
    int result = 0;
    ITERATE(TUnitsSet, it, m_ToFixTests) {
        if (!but::results_collector.results((*it)->p_id).passed())
            ++result;
    }
    return result;
}

inline bool
CNcbiTestApplication::IsTestToFix(const but::test_unit* tu)
{
    return m_ToFixTests.find(const_cast<but::test_unit*>(tu))
                                                        != m_ToFixTests.end();
}

inline void
CNcbiTestApplication::x_SetupBoostReporters(void)
{
    but::output_format format = but::runtime_config::report_format();

    CNcbiEnvironment env;
    string is_autobuild = env.Get("NCBI_AUTOMATED_BUILD");
    if (! is_autobuild.empty()) {
        // There shouldn't be any message box in the automated build mode
        SuppressSystemMessageBox(fSuppress_All);

        format = but::XML;
        but::results_reporter::set_level(but::DETAILED_REPORT);

        string boost_rep = env.Get("NCBI_BOOST_REPORT_FILE");
        if (! boost_rep.empty()) {
            m_ReportOut.open(boost_rep.c_str());
            if (m_ReportOut.good()) {
                but::results_reporter::set_stream(m_ReportOut);
            }
            else {
                ERR_POST("Error opening Boost.Test report file '"
                         << boost_rep << "'");
            }
        }
    }

    m_Reporter->SetOutputFormat(format);
    but::results_reporter::set_format(m_Reporter);

    m_Logger->SetOutputFormat(but::runtime_config::log_format());
    but::unit_test_log.set_formatter(m_Logger);
}

static const char*
s_GetUserFuncName(ETestUserFuncType func_type)
{
    switch (func_type) {
    case eTestUserFuncInit:
        return "NCBITEST_AUTO_INIT()";
    case eTestUserFuncFini:
        return "NCBITEST_AUTO_FINI()";
    case eTestUserFuncCmdLine:
        return "NCBITEST_INIT_CMDLINE()";
    case eTestUserFuncVars:
        return "NCBITEST_INIT_VARIABLES()";
    case eTestUserFuncDeps:
        return "NCBITEST_INIT_TREE()";
    default:
        return NULL;
    }
}

bool
CNcbiTestApplication::x_CallUserFuncs(ETestUserFuncType func_type)
{
    ITERATE(TUserFuncsList, it, m_UserFuncs[func_type]) {
        try {
            (*it)();
        }
        catch (CException& e) {
            ERR_POST_X(1, "Exception in " << s_GetUserFuncName(func_type) << ": " << e);
            throw;
            //return false;
        }
        catch (exception& e) {
            ERR_POST_X(1, "Exception in " << s_GetUserFuncName(func_type) << ": " << e.what());
            throw;
            //return false;
        }
    }

    return true;
}

inline but::test_unit*
CNcbiTestApplication::GetTestUnit(CTempString test_name)
{
    TStringToUnitMap::iterator it = m_AllTests.find(
                                            x_GetTrimmedTestName(test_name));
    if (it == m_AllTests.end()) {
        NCBI_THROW(CCoreException, eInvalidArg,
                   "Test unit '" + (string)test_name + "' not found.");
    }

    return it->second;
}

inline void
CNcbiTestApplication::x_CollectAllTests(void)
{
    m_AllTests.clear();
    CNcbiTestsCollector collector;
    but::traverse_test_tree(but::framework::master_test_suite(), collector);
}

inline int
CNcbiTestApplication::x_GetEnabledTestsCount(void)
{
    but::test_case_counter tcc;
    but::traverse_test_tree(but::framework::master_test_suite(), tcc);
    return tcc.p_count;
}

but::test_suite*
CNcbiTestApplication::InitTestFramework(int argc, char* argv[])
{
    // Do not detect memory leaks using msvcrt - this information is useless
    boost::debug::detect_memory_leaks(false);
    boost::debug::break_memory_alloc(0);

    x_SetupBoostReporters();
    but::framework::register_observer(m_Observer);

    // TODO: change this functionality to use only -dryrun parameter
    for (int i = 1; i < argc; ++i) {
        if (NStr::CompareCase(argv[i], "--do_not_run") == 0) {
            m_RunMode |= fTestList;
            but::results_reporter::set_level(but::DETAILED_REPORT);

            for (int j = i + 1; j < argc; ++j) {
                argv[j - 1] = argv[j];
            }
            --argc;
        }
    }

    CNcbiEnvironment env;
    m_TimeoutStr = env.Get("NCBI_CHECK_TIMEOUT");
    if (!m_TimeoutStr.empty()) {
        m_Timeout = NStr::StringToDouble(m_TimeoutStr, NStr::fConvErr_NoThrow);
    }
    if (m_Timeout == 0) {
        m_Timer.Stop();
    }
    else {
        m_Timeout = min(max(0.0, m_Timeout - 3), 0.9 * m_Timeout);
    }
    m_TimeMult = NCBI_GetCheckTimeoutMult();

    if (AppMain(argc, argv) == 0 && m_RunCalled) {
        x_CollectAllTests();

        but::traverse_test_tree(but::framework::master_test_suite(),
                                m_TreeBuilder);

        // We do not read configuration if particular tests were given in
        // command line
        if (x_CallUserFuncs(eTestUserFuncDeps)
            &&  (!but::runtime_config::test_to_run().empty()
                 ||  x_ReadConfiguration()))
        {
            // Call should be doubled to support manual adding of
            // test cases inside NCBITEST_INIT_TREE().
            x_CollectAllTests();
            if (x_GetEnabledTestsCount() == 0) {
                SetGloballyDisabled();
                x_AddDummyTest();
            }
#ifdef NCBI_COMPILER_WORKSHOP
            else if (!but::runtime_config::test_to_run().empty()) {
                printf("Parameter --run_test is not supported in current configuration\n");
                x_EnableAllTests(false);
                x_AddDummyTest();
            }
#endif

            return NULL;
        }
    }

    // This path we'll be if something have gone wrong
    x_CollectAllTests();
    x_EnableAllTests(false);

    return NULL;
}

inline bool
CNcbiTestApplication::HasTestErrors(void)
{
    return m_HasTestErrors;
}

inline bool
CNcbiTestApplication::HasTestTimeouts(void)
{
    return m_HasTestTimeouts;
}

void
CNcbiTestsCollector::visit(but::test_case const& test)
{
    s_GetTestApp().CollectTestUnit(const_cast<but::test_case*>(&test));
}

bool
CNcbiTestsCollector::test_suite_start(but::test_suite const& suite)
{
    s_GetTestApp().CollectTestUnit(const_cast<but::test_suite*>(&suite));
    return true;
}


void
CNcbiTestsObserver::test_start(but::counter_t /* test_cases_amount */)
{
    s_GetTestApp().InitTestsBeforeRun();
}

void
CNcbiTestsObserver::test_finish(void)
{
    s_GetTestApp().FiniTestsAfterRun();
}

void
CNcbiTestsObserver::test_unit_start(but::test_unit const& tu)
{
    s_GetTestApp().AdjustTestTimeout(const_cast<but::test_unit*>(&tu));
}

void
CNcbiTestsObserver::test_unit_finish(but::test_unit const& tu,
                                     unsigned long         elapsed)
{
    unsigned long timeout = tu.p_timeout.get();
    // elapsed comes in microseconds
    if (timeout != 0  &&  timeout < elapsed / 1000000) {
        boost::execution_exception ex(
               boost::execution_exception::timeout_error, "Timeout exceeded"
               NCBI_BOOST_LOCATION());
        but::framework::exception_caught(ex);
    }

    but::test_results& tr = but::s_rc_impl().m_results_store[tu.p_id];
    if (!tr.passed()  &&  s_GetTestApp().IsTestToFix(&tu)) {
        static_cast<but::readwrite_property<bool>& >(
            static_cast<but::class_property<bool>& >(
                                            tr.p_skipped)).set(true);
        static_cast<but::readwrite_property<but::counter_t>& >(
            static_cast<but::class_property<but::counter_t>& >(
                                            tr.p_assertions_failed)).set(0);
    }
}

void
CNcbiTestsObserver::exception_caught(boost::execution_exception const& ex)
{
    if (ex.code() == boost::execution_exception::timeout_error) {
        s_GetTestApp().SetTestTimedOut(const_cast<but::test_case*>(
                                       &but::framework::current_test_case()));
    }
    else {
        s_GetTestApp().SetTestErrored(const_cast<but::test_case*>(
                                      &but::framework::current_test_case()));
    }
}

void
CNcbiTestsObserver::test_unit_aborted(but::test_unit const& tu)
{
    s_GetTestApp().SetTestErrored((but::test_case*)&tu);
}

void
CNcbiTestsObserver::assertion_result(bool passed)
{
    if (!passed) {
        s_GetTestApp().SetTestErrored(const_cast<but::test_case*>(
                                      &but::framework::current_test_case()));
    }
}


void
CNcbiBoostReporter::results_report_start(ostream& ostr)
{
    m_Indent = 0;
    s_GetTestApp().ReEnableAllTests();

    m_Upper->results_report_start(ostr);
}

void
CNcbiBoostReporter::results_report_finish(ostream& ostr)
{
    m_Upper->results_report_finish(ostr);
    if (m_IsXML) {
        ostr << endl;
    }
}

void
CNcbiBoostReporter::test_unit_report_start(but::test_unit const&  tu,
                                           ostream&               ostr)
{
    if (tu.p_name.get() == kDummyTestCaseName)
        return;

    string descr = s_GetTestApp().GetTestResultString(
                                        const_cast<but::test_unit*>(&tu));

    if (m_IsXML) {
        ostr << '<' << (tu.p_type == but::tut_case ? "TestCase" : "TestSuite")
             << " name"   << but::attr_value() << tu.p_name.get()
             << " result" << but::attr_value() << descr;

        ostr << '>';
    }
    else {
        ostr << std::setw( m_Indent ) << ""
            << "Test " << (tu.p_type == but::tut_case ? "case " : "suite " )
            << "\"" << tu.p_name << "\" " << descr;

        ostr << '\n';
        m_Indent += 2;
    }
}

void
CNcbiBoostReporter::test_unit_report_finish(but::test_unit const&  tu,
                                            std::ostream&          ostr)
{
    if (tu.p_name.get() == kDummyTestCaseName)
        return;

    m_Indent -= 2;
    m_Upper->test_unit_report_finish(tu, ostr);
}

void
CNcbiBoostReporter::do_confirmation_report(but::test_unit const&  tu,
                                           std::ostream&          ostr)
{
    m_Upper->do_confirmation_report(tu, ostr);
}


void
CNcbiBoostLogger::log_start(ostream& ostr, but::counter_t test_cases_amount)
{
    m_Upper->log_start(ostr, test_cases_amount);
}

void
CNcbiBoostLogger::log_finish(ostream& ostr)
{
    m_Upper->log_finish(ostr);
    if (!m_IsXML) {
        ostr << "Executed " << s_GetTestApp().GetRanTestsCount()
             << " test cases";
        int to_fix = s_GetTestApp().GetToFixTestsCount();
        if (to_fix != 0) {
            ostr << " (" << to_fix << " to fix)";
        }
        ostr << "." << endl;
    }
}

void
CNcbiBoostLogger::log_build_info(ostream& ostr)
{
    m_Upper->log_build_info(ostr);
}

void
CNcbiBoostLogger::test_unit_start(ostream& ostr, but::test_unit const& tu)
{
    m_Upper->test_unit_start(ostr, tu);
}

void
CNcbiBoostLogger::test_unit_finish(ostream& ostr, but::test_unit const& tu,
                                   unsigned long elapsed)
{
    m_Upper->test_unit_finish(ostr, tu, elapsed);
}

void
CNcbiBoostLogger::test_unit_skipped(ostream& ostr, but::test_unit const& tu)
{
    m_Upper->test_unit_skipped(ostr, tu);
}

#if BOOST_VERSION >= 104200
void
CNcbiBoostLogger::log_exception(ostream& ostr, but::log_checkpoint_data const& lcd,
                                boost::execution_exception const& ex)
{
    m_Upper->log_exception(ostr, lcd, ex);
}
#else
void
CNcbiBoostLogger::log_exception(ostream& ostr, but::log_checkpoint_data const& lcd,
                                but::const_string explanation)
{
    m_Upper->log_exception(ostr, lcd, explanation);
}
#endif

void
CNcbiBoostLogger::log_entry_start(ostream& ostr, but::log_entry_data const& led,
                                  log_entry_types let)
{
    m_Upper->log_entry_start(ostr, led, let);
}

void
CNcbiBoostLogger::log_entry_value(ostream& ostr, but::const_string value)
{
    m_Upper->log_entry_value(ostr, value);
}

void
CNcbiBoostLogger::log_entry_finish(ostream& ostr)
{
    m_Upper->log_entry_finish(ostr);
}


void
RegisterNcbiTestUserFunc(TNcbiTestUserFunction func,
                         ETestUserFuncType     func_type)
{
    s_GetTestApp().AddUserFunction(func, func_type);
}

void
NcbiTestDependsOn(but::test_unit* tu, but::test_unit* dep_tu)
{
    s_GetTestApp().AddTestDependsOn(tu, dep_tu);
}

void
NcbiTestDisable(but::test_unit* tu)
{
    s_GetTestApp().SetTestDisabled(tu);
}

void
NcbiTestSetGlobalDisabled(void)
{
    s_GetTestApp().SetGloballyDisabled();
}

void
NcbiTestSetGlobalSkipped(void)
{
    s_GetTestApp().SetGloballySkipped();
}

CExprParser*
NcbiTestGetIniParser(void)
{
    return s_GetTestApp().GetIniParser();
}

CArgDescriptions*
NcbiTestGetArgDescrs(void)
{
    return s_GetTestApp().GetArgDescrs();
}

but::test_unit*
NcbiTestGetUnit(CTempString test_name)
{
    return s_GetTestApp().GetTestUnit(test_name);
}


END_NCBI_SCOPE


using namespace but;

/// Global initialization function called from Boost framework
test_suite*
init_unit_test_suite(int argc, char* argv[])
{
    return NCBI_NS_NCBI::s_GetTestApp().InitTestFramework(argc, argv);
}

// This main() is mostly a copy from Boost's unit_test_main.ipp
int
main(int argc, char* argv[])
{
    int result = boost::exit_success;

    try {
        framework::init( &init_unit_test_suite, argc, argv );

        if( !runtime_config::test_to_run().is_empty() ) {
            test_case_filter filter( runtime_config::test_to_run() );

            traverse_test_tree( framework::master_test_suite().p_id, filter );
        }

        framework::run();

        // Let's try to make report in case of any error after all catches.
        //results_reporter::make_report();

        if (!runtime_config::no_result_code()) {
            result = results_collector.results( framework::master_test_suite().p_id ).result_code();
            if (!NCBI_NS_NCBI::s_GetTestApp().HasTestErrors()
                &&  NCBI_NS_NCBI::s_GetTestApp().HasTestTimeouts())
            {
                // This should certainly go to the output. So we can use only
                // printf, nothing else.
                printf("There were no test failures, only timeouts.\n"
                       " (for autobuild scripts: NCBI_UNITTEST_TIMEOUTS_BUT_NO_ERRORS)\n");
            }
        }
    }
#if BOOST_VERSION >= 104200
    catch( framework::nothing_to_test const& ) {
        result = boost::exit_success;
    }
#endif
    catch( framework::internal_error const& ex ) {
        results_reporter::get_stream() << "Boost.Test framework internal error: " << ex.what() << std::endl;
        
        result = boost::exit_exception_failure;
    }
    catch( framework::setup_error const& ex ) {
        results_reporter::get_stream() << "Test setup error: " << ex.what() << std::endl;
        
        result = boost::exit_exception_failure;
    }
    catch( std::exception const& ex ) {
        results_reporter::get_stream() << "Test framework error: " << ex.what() << std::endl;

        result = boost::exit_exception_failure;
    }
    catch( ... ) {
        results_reporter::get_stream() << "Boost.Test framework internal error: unknown reason" << std::endl;
        
        result = boost::exit_exception_failure;
    }

    results_reporter::make_report();

    delete NCBI_NS_NCBI::s_TestApp;
    NCBI_NS_NCBI::GetDiagContext().SetExitCode(result);
    return result;
}
