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
#include <corelib/test_boost.hpp>
#undef init_unit_test_suite

#include <boost/test/results_reporter.hpp>
#include <boost/test/output/plain_report_formatter.hpp>
#include <boost/test/output/xml_report_formatter.hpp>
#include <boost/test/detail/unit_test_parameters.hpp>

#include <list>
#include <string>

#include <common/test_assert.h>  /* This header must go last */


#define NCBI_USE_ERRCODE_X  Corelib_TestBoost


/// Prototype of global user-defined initialization function
extern boost::unit_test::test_suite*
NcbiInitUnitTestSuite( int argc, char* argv[] );


BEGIN_NCBI_SCOPE

static list<TNcbiBoostInitFunc>* s_BoostInitFuncs = NULL;


void
RegisterNcbiBoostInit(TNcbiBoostInitFunc func)
{
    if (! s_BoostInitFuncs) {
        s_BoostInitFuncs = new list<TNcbiBoostInitFunc>();
    }
    s_BoostInitFuncs->push_back(func);
}



// Some shortening typedefs
typedef boost::unit_test::results_reporter::format  TBoostFormatter;
typedef boost::unit_test::test_unit                 TBoostTestUnit;


/// Reporter for embedding in Boost framework and adding non-standard
/// information to detailed report given by Boost.
class CNcbiBoostReporter : public TBoostFormatter
{
public:
    /// Create reporter tuned for printing report of specific format
    ///
    /// @param format
    ///   Format of the report
    CNcbiBoostReporter(boost::unit_test::output_format format);
    virtual ~CNcbiBoostReporter(void);

    /// Add new test marked as disabled
    ///
    /// @param test_name
    ///   Name of the disabled test
    /// @param reason
    ///   Reason of test disabling. It will be printed in the report.
    void AddDisabledTest(const string& test_name, const string& reason);

    // TBoostFormatter interface
    virtual
    void results_report_start(std::ostream& ostr);
    virtual
    void results_report_finish(std::ostream& ostr);
    virtual
    void test_unit_report_start(TBoostTestUnit const& tu, std::ostream& ostr);
    virtual
    void test_unit_report_finish(TBoostTestUnit const& tu, std::ostream& ostr);
    virtual
    void do_confirmation_report(TBoostTestUnit const& tu, std::ostream& ostr);

private:
    typedef list< pair<string, string> >  TDisabledList;

    /// If reporter already printed info about disabled tests or not
    bool                       m_DisabledPrinted;
    /// Standard reporter from Boost for particular report format
    auto_ptr<TBoostFormatter>  m_Upper;
    /// If report is XML or not
    bool                       m_IsXML;
    /// List of disabled tests
    TDisabledList              m_DisabledList;
};


CNcbiBoostReporter::CNcbiBoostReporter(boost::unit_test::output_format format)
    : m_DisabledPrinted(false)
{
    if (format == boost::unit_test::XML) {
        m_IsXML = true;
        m_Upper.reset(new boost::unit_test::output::xml_report_formatter());
    }
    else {
        m_IsXML = false;
        m_Upper.reset(new boost::unit_test::output::plain_report_formatter());
    }
}

CNcbiBoostReporter::~CNcbiBoostReporter(void)
{
}

void
CNcbiBoostReporter::AddDisabledTest(const string&  test_name,
                                    const string&  reason)
{
    m_DisabledList.push_back(make_pair(test_name, reason));
}

void
CNcbiBoostReporter::results_report_start(std::ostream& ostr)
{
    m_Upper->results_report_start(ostr);
}

void
CNcbiBoostReporter::results_report_finish(std::ostream& ostr)
{
    m_Upper->results_report_finish(ostr);
}

void
CNcbiBoostReporter::test_unit_report_start(TBoostTestUnit const&  tu,
                                           std::ostream&          ostr)
{
    if (! m_DisabledPrinted) {
        ITERATE(TDisabledList, it, m_DisabledList) {
            if (m_IsXML) {
                ostr << "<TestCase name=\"" << it->first << "\" result=\"disabled\"";
                if (! it->second.empty()) {
                    ostr << " reason=\"" << it->second << "\"";
                }
                ostr << "/>";
            }
            else {
                ostr << "Test case \"" << it->first << "\" disabled";
                if (! it->second.empty()) {
                    ostr << " because " << it->second;
                }
                ostr << '\n';
            }
        }

        m_DisabledPrinted = true;
    }

    m_Upper->test_unit_report_start(tu, ostr);
}

void
CNcbiBoostReporter::test_unit_report_finish(TBoostTestUnit const&  tu,
                                            std::ostream&          ostr)
{
    m_Upper->test_unit_report_finish(tu, ostr);
}

void
CNcbiBoostReporter::do_confirmation_report(TBoostTestUnit const&  tu,
                                           std::ostream&          ostr)
{
    m_Upper->do_confirmation_report(tu, ostr);
}



/// The singleton object for reporter
CNcbiBoostReporter* s_NcbiReporter = NULL;


void
NcbiBoostTestDisable(CTempString test_name, CTempString reason)
{
    // reporter must be already created by init_unit_test_suite()
    assert(s_NcbiReporter);

    s_NcbiReporter->AddDisabledTest(test_name, reason);
}


END_NCBI_SCOPE


// Global initialization function called from Boost framework
boost::unit_test::test_suite*
init_unit_test_suite( int argc, char* argv[] )
{
    // This function should not be called yet
    assert(! NCBI_NS_NCBI::s_NcbiReporter);

    boost::unit_test::output_format
                format = boost::unit_test::runtime_config::report_format();

    NCBI_NS_NCBI::CNcbiEnvironment env;
    std::string is_autobuild = env.Get("NCBI_AUTOMATED_BUILD");
    if (! is_autobuild.empty()) {
        format = boost::unit_test::XML;
        boost::unit_test::results_reporter::set_level(
                                           boost::unit_test::DETAILED_REPORT);
    }

    NCBI_NS_NCBI::s_NcbiReporter =
                                new NCBI_NS_NCBI::CNcbiBoostReporter(format);

    boost::unit_test::results_reporter
                            ::set_format(NCBI_NS_NCBI::s_NcbiReporter);


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
