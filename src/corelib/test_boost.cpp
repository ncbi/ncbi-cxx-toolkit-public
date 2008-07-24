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

#define NCBI_BOOST_NO_AUTO_TEST_MAIN
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


// Some shortening typedefs
typedef but::results_reporter::format  TBoostFormatter;
typedef but::test_unit                 TBoostTestUnit;


static list<TNcbiBoostInitFunc>*        s_BoostInitFuncs = NULL;

typedef set<TBoostTestUnit*>            TUnitsSet;
typedef map<TBoostTestUnit*, TUnitsSet> TUnitToManyMap;
static  TUnitsSet                       s_DisabledTests;
static  TUnitToManyMap                  s_TestDeps;


void
RegisterNcbiBoostInit(TNcbiBoostInitFunc func)
{
    if (! s_BoostInitFuncs) {
        s_BoostInitFuncs = new list<TNcbiBoostInitFunc>();
    }
    s_BoostInitFuncs->push_back(func);
}

void
NcbiTestDependsOn(TBoostTestUnit* tu, TBoostTestUnit* dep_tu)
{
    s_TestDeps[tu].insert(dep_tu);
}

void
NcbiTestDisable(TBoostTestUnit* tu)
{
    tu->p_enabled.set(false);
    s_DisabledTests.insert(tu);
}


/// Special observer to embed in Boost.Test framework to initialize test
/// dependencies before they started execution.
class CNcbiTestsInitializer : public but::test_observer
{
public:
    /// Method called before execution of all tests
    virtual void test_start(but::counter_t /* test_cases_amount */);
};


void
CNcbiTestsInitializer::test_start(but::counter_t /* test_cases_amount */)
{
    ITERATE(TUnitToManyMap, it, s_TestDeps) {
        TBoostTestUnit* test = it->first;
        if (!s_DisabledTests.count(test) && !test->p_enabled) {
            continue;
        }

        ITERATE(TUnitsSet, dep_it, it->second) {
            TBoostTestUnit* dep_test = *dep_it;
            if (!s_DisabledTests.count(dep_test) && !dep_test->p_enabled) {
                continue;
            }

            test->depends_on(dep_test);
        }
    }
}


/// Reporter for embedding in Boost framework and adding non-standard
/// information to detailed report given by Boost.
class CNcbiBoostReporter : public TBoostFormatter
{
public:
    /// Create reporter tuned for printing report of specific format
    ///
    /// @param format
    ///   Format of the report
    CNcbiBoostReporter(but::output_format format);
    virtual ~CNcbiBoostReporter(void);

    // TBoostFormatter interface
    virtual
    void results_report_start(std::ostream& ostr);
    virtual
    void results_report_finish(std::ostream& ostr);
    virtual
    void test_unit_report_start(TBoostTestUnit const& tu, std::ostream& ostr);
    virtual
    void test_unit_report_finish(TBoostTestUnit const& tu,std::ostream& ostr);
    virtual
    void do_confirmation_report(TBoostTestUnit const& tu, std::ostream& ostr);

private:
    /// Standard reporter from Boost for particular report format
    auto_ptr<TBoostFormatter>  m_Upper;
    /// If report is XML or not
    bool                       m_IsXML;
    /// Current indentation level in plain text report
    int                        m_Indent;
};


CNcbiBoostReporter::CNcbiBoostReporter(but::output_format format)
{
    if (format == but::XML) {
        m_IsXML = true;
        m_Upper.reset(new but::output::xml_report_formatter());
    }
    else {
        m_IsXML = false;
        m_Upper.reset(new but::output::plain_report_formatter());
    }
}

CNcbiBoostReporter::~CNcbiBoostReporter(void)
{
}

void
CNcbiBoostReporter::results_report_start(std::ostream& ostr)
{
    m_Indent = 0;
    ITERATE(TUnitsSet, it, s_DisabledTests) {
        (*it)->p_enabled.set(true);
    }

    m_Upper->results_report_start(ostr);
}

void
CNcbiBoostReporter::results_report_finish(std::ostream& ostr)
{
    m_Upper->results_report_finish(ostr);
    if (m_IsXML) {
        ostr << endl;
    }
}

void
CNcbiBoostReporter::test_unit_report_start(TBoostTestUnit const&  tu,
                                           std::ostream&          ostr)
{
    but::test_results const& tr = but::results_collector.results( tu.p_id );

    string descr;

    if( tr.passed() )
        descr = "passed";
    else if( tr.p_skipped ) {
        if (s_DisabledTests.count(const_cast<TBoostTestUnit*>(&tu)) != 0)
            descr = "disabled";
        else
            descr = "skipped";
    }
    else if( tr.p_aborted )
        descr = "aborted";
    else
        descr = "failed";

    if (m_IsXML) {
        ostr << '<' << (tu.p_type == but::tut_case ? "TestCase" : "TestSuite") 
             << " name"     << but::attr_value() << tu.p_name.get()
             << " result"   << but::attr_value() << descr;

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
CNcbiBoostReporter::test_unit_report_finish(TBoostTestUnit const&  tu,
                                            std::ostream&          ostr)
{
    m_Indent -= 2;
    m_Upper->test_unit_report_finish(tu, ostr);
}

void
CNcbiBoostReporter::do_confirmation_report(TBoostTestUnit const&  tu,
                                           std::ostream&          ostr)
{
    m_Upper->do_confirmation_report(tu, ostr);
}



/// The singleton object for reporter
static CNcbiBoostReporter*   s_NcbiReporter = NULL;
static CNcbiTestsInitializer s_NcbiInitializer;


END_NCBI_SCOPE


NCBI_NS_NCBI::AutoPtr<std::ostream> s_ReportOut;

// Global initialization function called from Boost framework
but::test_suite*
init_unit_test_suite( int argc, char* argv[] )
{
    // This function should not be called yet
    assert(! NCBI_NS_NCBI::s_NcbiReporter);

    but::output_format format = but::runtime_config::report_format();

    NCBI_NS_NCBI::CNcbiEnvironment env;
    std::string is_autobuild = env.Get("NCBI_AUTOMATED_BUILD");
    if (! is_autobuild.empty()) {
        format = but::XML;
        but::results_reporter::set_level(but::DETAILED_REPORT);

        std::string boost_rep = env.Get("NCBI_BOOST_REPORT_FILE");
        if (! boost_rep.empty()) {
            s_ReportOut = new std::ofstream(boost_rep.c_str());
            if (s_ReportOut->good()) {
                but::results_reporter::set_stream(*s_ReportOut);
            }
            else {
                ERR_POST("Error opening Boost.Test report file '"
                         + boost_rep + "'");
            }
        }
    }

    NCBI_NS_NCBI::s_NcbiReporter
                               = new NCBI_NS_NCBI::CNcbiBoostReporter(format);

    but::results_reporter::set_format(NCBI_NS_NCBI::s_NcbiReporter);

    but::framework::register_observer(NCBI_NS_NCBI::s_NcbiInitializer);


    if (NCBI_NS_NCBI::s_BoostInitFuncs) {
        ITERATE(std::list<NCBI_NS_NCBI::TNcbiBoostInitFunc>, it,
                                            (*NCBI_NS_NCBI::s_BoostInitFuncs))
        {
            try {
                (*it)();
            }
            catch (std::exception& e) {
                ERR_POST_X(1, "Exception in unit tests initialization function: "
                              << e.what());
                return NULL;
            }
        }

        delete NCBI_NS_NCBI::s_BoostInitFuncs;
        NCBI_NS_NCBI::s_BoostInitFuncs = NULL;
    }

    return NcbiInitUnitTestSuite(argc, argv);
}
