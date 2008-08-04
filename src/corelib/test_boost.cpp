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
#define NCBI_NO_TEST_PREPARE_ARG_DESCRS
#include <corelib/test_boost.hpp>
#undef init_unit_test_suite

#include <boost/test/results_collector.hpp>
#include <boost/test/results_reporter.hpp>
#include <boost/test/test_observer.hpp>
#include <boost/test/framework.hpp>
#include <boost/test/output/plain_report_formatter.hpp>
#include <boost/test/output/xml_report_formatter.hpp>
#include <boost/test/utils/xml_printer.hpp>
#include <boost/test/detail/global_typedef.hpp>
#include <boost/test/detail/unit_test_parameters.hpp>

#include <list>
#include <set>
#include <string>

#include <common/test_assert.h>  /* This header must go last */


#define NCBI_USE_ERRCODE_X  Corelib_TestBoost


namespace but = boost::unit_test;

/// Prototype of global user-defined initialization function
extern but::test_suite*
NcbiInitUnitTestSuite( int argc, char* argv[] );


BEGIN_NCBI_SCOPE


/// Prototype of global user-defined function for preparing command line
/// arguments description.
extern CArgDescriptions* NcbiTestPrepareArgDescrs(void);


typedef but::results_reporter::format   TBoostFormatter;
typedef set<but::test_unit*>            TUnitsSet;
typedef map<but::test_unit*, TUnitsSet> TUnitToManyMap;


/// Special observer to embed in Boost.Test framework to initialize test
/// dependencies before they started execution.
class CNcbiTestsInitializer : public but::test_observer
{
public:
    virtual ~CNcbiTestsInitializer(void) {}

    /// Method called before execution of all tests
    virtual void test_start(but::counter_t /* test_cases_amount */);
};


/// Reporter for embedding in Boost framework and adding non-standard
/// information to detailed report given by Boost.
class CNcbiBoostReporter : public TBoostFormatter
{
public:
    CNcbiBoostReporter(void);

    /// Setup reporter tuned for printing report of specific format
    ///
    /// @param format
    ///   Format of the report
    void SetOutputFormat(but::output_format format);

    // TBoostFormatter interface
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
    AutoPtr<TBoostFormatter>  m_Upper;
    /// If report is XML or not
    bool                      m_IsXML;
    /// Current indentation level in plain text report
    int                       m_Indent;
};


/// Class which will disable all tests if it will be passed to
/// but::traverse_test_tree() function and enable all tests after that
class CNcbiTestDisabler : public but::test_tree_visitor
{
public:
    virtual void ~CNcbiTestDisabler(void) {}

    virtual void visit(but::test_case const& test);

    /// Enable all tests that were traversed previously in
    /// but::traverse_test_tree()
    void ReEnableAll(void);

private:
    list<but::test_case*> m_AllTests;
};


/// Application for all unit tests
class CBoostTestApplication : public CNcbiApplication
{
public:
    CBoostTestApplication(void);

    virtual void Init  (void);
    virtual int  Run   (void);
    virtual int  DryRun(void);

    /// Add user initialization functions
    void AddBoostInitFunc(TNcbiBoostInitFunc func);

    /// Add dependency for test unit
    void AddTestDependsOn(but::test_unit* tu, but::test_unit* dep_tu);

    /// Set test as disabled by user
    void SetTestDisabled(but::test_unit* tu);

    /// Initialize this application, main test suite and all test framework
    but::test_suite* InitTestFramework(int argc, char* argv[]);

    /// Initialize already prepared test suite before running tests
    void InitTestsBeforeRun(void);

    /// Enable all necessary tests after execution but before printing report
    void ReEnableAllTests(void);

    /// Get string representation of result of test execution
    string GetTestResultString(but::test_unit tu);

private:
    /// If Run() was called or not
    ///
    /// @sa Run()
    bool m_RunCalled;
    /// If tests were launched with --do_not_run parameter
    bool m_TestListMode;
    /// List of all initialization functions created by user
    list<TNcbiBoostInitFunc> m_BoostInitFuncs;
    /// List of all disabled tests
    TUnitsSet                m_DisabledTests;
    /// List of all dependencies for each test having dependencies
    TUnitToManyMap           m_TestDeps;
    /// Object for disabling and reenabling all tests
    CNcbiTestDisabler        m_TestDisabler;
    /// Initializer to make test dependencies
    CNcbiTestsInitializer    m_Initializer;
    /// Boost reporter - must be pointer because Boost.Test calls free() on it
    CNcbiBoostReporter*      m_Reporter;
    /// Output stream for Boost.Test report
    ofstream                 m_ReportOut;
};


void
CNcbiTestDisabler::visit(but::test_case const& test)
{
    m_AllTests.push_back(const_cast<but::test_case*>(&test));
    test.p_enabled.set(false);
}

inline void
CNcbiTestDisabler::ReEnableAll(void)
{
    ITERATE(list<but::test_case*>, it, m_AllTests) {
        (*it)->p_enabled.set(true);
    }
}


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
CBoostTestApplication::CBoostTestApplication(void)
    : m_RunCalled   (false),
      m_TestListMode(false)
{
    m_Reporter = new CNcbiBoostReporter();
}

/// Application for unit tests
static CBoostTestApplication&
s_GetTestApp(void)
{
    static CBoostTestApplication s_TestApp;

    return s_TestApp;
}

void
CBoostTestApplication::Init(void)
{
    CArgDescriptions* descrs = NcbiTestPrepareArgDescrs();
    if (descrs) {
        descrs->AddFlag("dryrun",
                        "Do not actually run tests, "
                        "just print list of all available tests.");
        SetupArgDescriptions(descrs);
    }
    else {
        // Now it's not conforming to parameter type but conforming to
        // the implemented behavior
        DisableArgDescriptions(true);
    }
}

int
CBoostTestApplication::Run(void)
{
    m_RunCalled = true;
    return 0;
}

int
CBoostTestApplication::DryRun(void)
{
    m_TestListMode = true;
    return 0;
}

inline void
CBoostTestApplication::AddBoostInitFunc(TNcbiBoostInitFunc func)
{
    m_BoostInitFuncs.push_back(func);
}

inline void
CBoostTestApplication::AddTestDependsOn(but::test_unit* tu,
                                        but::test_unit* dep_tu)
{
    m_TestDeps[tu].insert(dep_tu);
}

inline void
CBoostTestApplication::SetTestDisabled(but::test_unit* tu)
{
    tu->p_enabled.set(false);
    m_DisabledTests.insert(tu);
}

inline void
CBoostTestApplication::InitTestsBeforeRun(void)
{
    if (m_TestListMode) {
        but::traverse_test_tree(but::framework::master_test_suite(),
                                m_TestDisabler);
    }
    else {
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
}

inline void
CBoostTestApplication::ReEnableAllTests(void)
{
    m_TestDisabler.ReEnableAll();

    ITERATE(TUnitsSet, it, m_DisabledTests) {
        (*it)->p_enabled.set(true);
    }
}

inline string
CBoostTestApplication::GetTestResultString(but::test_unit tu)
{
    string result;
    but::test_results const& tr = but::results_collector.results( tu.p_id );

    if( tr.passed() )
        result = "passed";
    else if( tr.p_skipped ) {
        if (m_DisabledTests.count(const_cast<but::test_unit*>(&tu)) != 0)
            result = "disabled";
        else
            result = "skipped";
    }
    else if( tr.p_aborted )
        result = "aborted";
    else
        result = "failed";

    return result;
}

inline but::test_suite*
CBoostTestApplication::InitTestFramework(int argc, char* argv[])
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
    but::framework::register_observer(m_Initializer);

    // TODO: change this functionality to use -dryrun parameter instead of
    //       --do_not_run
    for (int i = 0; i < argc; ++i) {
        if (NStr::CompareCase(argv[i], "--do_not_run") == 0) {
            m_TestListMode = true;
            but::results_reporter::set_level(but::DETAILED_REPORT);

            for (int j = i + 1; j < argc; ++j) {
                argv[j - 1] = argv[j];
            }
            --argc;
        }
    }

    if (AppMain(argc, argv) != 0 || !m_RunCalled) {
        return NULL;
    }


    ITERATE(list<TNcbiBoostInitFunc>, it, m_BoostInitFuncs) {
        try {
            (*it)();
        }
        catch (exception& e) {
            ERR_POST_X(1,
                       "Exception in unit tests initialization function: "
                       << e.what());
            return NULL;
        }
    }

    return ::NcbiInitUnitTestSuite(argc, argv);
}


void
CNcbiTestsInitializer::test_start(but::counter_t /* test_cases_amount */)
{
    s_GetTestApp().InitTestsBeforeRun();
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
    string descr = s_GetTestApp().GetTestResultString(tu);

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
RegisterNcbiBoostInit(TNcbiBoostInitFunc func)
{
    s_GetTestApp().AddBoostInitFunc(func);
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


END_NCBI_SCOPE


/// Global initialization function called from Boost framework
but::test_suite*
init_unit_test_suite(int argc, char* argv[])
{
    return NCBI_NS_NCBI::s_GetTestApp().InitTestFramework(argc, argv);
}
