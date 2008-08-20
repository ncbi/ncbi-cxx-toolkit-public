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
#include <corelib/error_codes.hpp>
#include <corelib/ncbienv.hpp>
#include <corelib/ncbimisc.hpp>
#include <corelib/ncbiapp.hpp>

#define NCBI_BOOST_NO_AUTO_TEST_MAIN
#include <corelib/test_boost.hpp>
#undef init_unit_test_suite

#include <boost/preprocessor/cat.hpp>
#include <boost/preprocessor/tuple/elem.hpp>
#include <boost/preprocessor/tuple/eat.hpp>

#include <boost/test/results_collector.hpp>
#include <boost/test/results_reporter.hpp>
#include <boost/test/test_observer.hpp>
#include <boost/test/framework.hpp>
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

#include <list>
#include <vector>
#include <set>
#include <map>
#include <string>


#define NCBI_USE_ERRCODE_X  Corelib_TestBoost


namespace but = boost::unit_test;

/// Prototype of global user-defined initialization function
extern but::test_suite*
NcbiInitUnitTestSuite( int argc, char* argv[] );


BEGIN_NCBI_SCOPE

const char* kTestConfigSectionName = "UNITTESTS_DISABLE";
const char* kTestConfigGlobalValue = "GLOBAL";

#define DUMMY_TEST_FUNCTION_NAME  DummyTestFunction
const char* kDummyTestCaseName    = BOOST_STRINGIZE(DUMMY_TEST_FUNCTION_NAME);

const char* kTestResultPassed      = "passed";
const char* kTestResultFailed      = "failed";
const char* kTestResultAborted     = "aborted";
const char* kTestResultSkipped     = "skipped";
const char* kTestResultDisabled    = "disabled";


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
    virtual
    void log_exception    (ostream& ostr, but::log_checkpoint_data const& lcd,
                                          but::const_string explanation);
    virtual
    void log_entry_start  (ostream& ostr, but::log_entry_data const& led,
                                          log_entry_types let);
    virtual
    void log_entry_value  (ostream& ostr, but::const_string value);
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
class CNcbiTestsInitializer : public but::test_observer
{
public:
    virtual ~CNcbiTestsInitializer(void) {}

    /// Method called before execution of all tests
    virtual void test_start(but::counter_t /* test_cases_amount */);

    /// Method called after execution of all tests
    virtual void test_finish(void);
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
                            TElemsList::iterator  it_right);

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

    void SetGloballyDisabled(void);

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

    /// Get number of actually executed tests
    int GetRanTestsCount(void);

    /// Get string representation of result of test execution
    string GetTestResultString(but::test_unit* tu);

    /// Get pointer to empty test case added to Boost for internal purposes
    but::test_case* GetDummyTest(void);

    /// Check if user initialization functions failed
    bool IsInitFailed(void);

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
    bool x_CalcConfigValue(const string& value_name);


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
    /// List of all dependencies for each test having dependencies
    TUnitToManyMap            m_TestDeps;
    /// Initializer to make test dependencies
    CNcbiTestsInitializer     m_Initializer;
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
                                         TElemsList::iterator  it_right)
{
    TElemsList::iterator it_left  = find(m_Children.begin(), m_Children.end(),
                                         leftElem);
    _ASSERT(it_left != m_Children.end());

    if (it_left < it_right)
        return;

    m_OrderChanged = true;
    m_Children.erase(it_left);
    it_right = m_Children.insert(it_right, leftElem);

    ITERATE(TElemsSet, it, leftElem->m_MustLeft) {
        x_EnsureChildOrder(*it, it_right);
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

    TElemsList::iterator it_right = find(m_Children.begin(), m_Children.end(),
                                         rightElem);
    _ASSERT(it_right != m_Children.end());

    x_EnsureChildOrder(leftElem, it_right);
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


inline
CNcbiTestApplication::CNcbiTestApplication(void)
    : m_RunCalled(false),
      m_RunMode  (0),
      m_DummyTest(NULL)
{
    m_Reporter = new CNcbiBoostReporter();
    m_Logger   = new CNcbiBoostLogger();

    // Do not show warning about inaccessible configuration file
    SetDiagFilter(eDiagFilter_Post, "!(106.11)");
}

/// Application for unit tests
static CNcbiTestApplication&
s_GetTestApp(void)
{
    static CNcbiTestApplication s_TestApp;

    return s_TestApp;
}

void
CNcbiTestApplication::Init(void)
{
    if (m_UserFuncs[eTestUserFuncCmdLine].empty()) {
        // No user function - no parameter parsing.
        // Now this call is not conforming to parameter type but conforming to
        // the implemented behavior.
        DisableArgDescriptions(true);
    }
    else {
        m_ArgDescrs = new CArgDescriptions();
        m_ArgDescrs->AddFlag("dryrun",
                             "Do not actually run tests, "
                             "just print list of all available tests.");
        x_CallUserFuncs(eTestUserFuncCmdLine);
        SetupArgDescriptions(m_ArgDescrs.release());
    }
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
    m_RunMode |= fTestList;
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
    tu->p_enabled.set(false);
    m_DisabledTests.insert(tu);
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
    const string& test_name = tu->p_name.get();
    if (test_name == kDummyTestCaseName)
        return;
    m_AllTests[x_GetTrimmedTestName(test_name)] = tu;
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
#define IS_FLAG_DEFINED_I(flag)                               \
    (BOOST_PP_CAT(IS_FLAG_DEFINED_II_, flag) (), false)
#define IS_FLAG_DEFINED_II_()                                 \
    BOOST_PP_NIL, true) BOOST_PP_TUPLE_EAT(2) (BOOST_PP_NIL
#define IS_FLAG_DEFINED_II_1()                                \
    BOOST_PP_NIL, true) BOOST_PP_TUPLE_EAT(2) (BOOST_PP_NIL

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

    m_IniParser->AddSymbol("BUILD_Dll",          IS_FLAG_DEFINED(DLL_BUILD));
    m_IniParser->AddSymbol("BUILD_Static",      !IS_FLAG_DEFINED(DLL_BUILD));
}

inline bool
CNcbiTestApplication::x_CalcConfigValue(const string& value_name)
{
    const IRegistry& registry = s_GetTestApp().GetConfig();
    const string& value = registry.Get(kTestConfigSectionName, value_name);
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
    registry.EnumerateEntries(kTestConfigSectionName, &reg_entries);

    // Disable tests ...
    ITERATE(list<string>, it, reg_entries) {
        const string& test_name = *it;

        if (test_name == kTestConfigGlobalValue) {
            if (x_CalcConfigValue(test_name)) {
                SetGloballyDisabled();
            }
            continue;
        }

        but::test_unit* tu = GetTestUnit(test_name);
        if (tu) {
            if (x_CalcConfigValue(test_name))
            {
                SetTestDisabled(tu);
            }
        }
        else {
            ERR_POST_X(2, Warning << "Invalid test case name: '"
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
    // they were disabled in NcbiInitUnitTestSuite()
    ITERATE(TUnitsSet, it, m_DisabledTests) {
        (*it)->p_enabled.set(true);
    }
}

inline string
CNcbiTestApplication::GetTestResultString(but::test_unit* tu)
{
    string result;
    but::test_results const& tr = but::results_collector.results(tu->p_id);

    if (m_DisabledTests.count(tu) != 0  ||  (m_RunMode & fDisabled))
        result = kTestResultDisabled;
    else if( tr.p_aborted )
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

inline void
CNcbiTestApplication::x_SetupBoostReporters(void)
{
    but::output_format format = but::runtime_config::report_format();

    CNcbiEnvironment env;
    string is_autobuild = env.Get("NCBI_AUTOMATED_BUILD");
    if (! is_autobuild.empty()) {
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

bool
CNcbiTestApplication::x_CallUserFuncs(ETestUserFuncType func_type)
{
    ITERATE(TUserFuncsList, it, m_UserFuncs[func_type]) {
        try {
            (*it)();
        }
        catch (exception& e) {
            ERR_POST_X(1, "Exception in unit tests user function: "
                          << e.what());
            return false;
        }
    }

    return true;
}

inline but::test_unit*
CNcbiTestApplication::GetTestUnit(CTempString test_name)
{
    TStringToUnitMap::iterator it = m_AllTests.find(
                                            x_GetTrimmedTestName(test_name));
    if (it != m_AllTests.end())
        return it->second;

    return NULL;
}

inline void
CNcbiTestApplication::x_CollectAllTests(void)
{
    if (m_AllTests.size() > 1)
        return;

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

inline but::test_suite*
CNcbiTestApplication::InitTestFramework(int argc, char* argv[])
{
    // Do not detect memory leaks using msvcrt - this information is useless
    boost::debug::detect_memory_leaks(false);
    boost::debug::break_memory_alloc(0);

    x_SetupBoostReporters();
    but::framework::register_observer(m_Initializer);

    // TODO: change this functionality to use only -dryrun parameter
    for (int i = 0; i < argc; ++i) {
        if (NStr::CompareCase(argv[i], "--do_not_run") == 0) {
            m_RunMode |= fTestList;
            but::results_reporter::set_level(but::DETAILED_REPORT);

            for (int j = i + 1; j < argc; ++j) {
                argv[j - 1] = argv[j];
            }
            --argc;
        }
    }

    but::test_suite* global_suite = NULL;

    x_CollectAllTests();
    if (AppMain(argc, argv) == 0 && m_RunCalled) {
        global_suite = ::NcbiInitUnitTestSuite(argc, argv);

        if (global_suite)
            but::framework::master_test_suite().add(global_suite);

        // Call should be double to support tests with automatic registration
        // and without it at the same time.
        x_CollectAllTests();

        but::traverse_test_tree(but::framework::master_test_suite(),
                                m_TreeBuilder);

        // We do not read configuration if particular tests were given in
        // command line
        if (x_CallUserFuncs(eTestUserFuncDeps)
            &&  (!but::runtime_config::test_to_run().empty()
                 ||  x_ReadConfiguration()))
        {
            if (x_GetEnabledTestsCount() == 0) {
                SetGloballyDisabled();
                x_AddDummyTest();
            }

            return NULL;
        }
    }

    // This path we'll be if something've gone wrong
    if (global_suite) {
        but::framework::master_test_suite().remove(global_suite->p_id);
    }
    else {
        x_EnableAllTests(false);
    }

    return NULL;
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
CNcbiTestsInitializer::test_start(but::counter_t /* test_cases_amount */)
{
    s_GetTestApp().InitTestsBeforeRun();
}

void
CNcbiTestsInitializer::test_finish(void)
{
    s_GetTestApp().FiniTestsAfterRun();
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
             << " test cases." << endl;
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

void
CNcbiBoostLogger::log_exception(ostream& ostr, but::log_checkpoint_data const& lcd,
                                but::const_string explanation)
{
    m_Upper->log_exception(ostr, lcd, explanation);
}

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


/// Global initialization function called from Boost framework
but::test_suite*
init_unit_test_suite(int argc, char* argv[])
{
    return NCBI_NS_NCBI::s_GetTestApp().InitTestFramework(argc, argv);
}
