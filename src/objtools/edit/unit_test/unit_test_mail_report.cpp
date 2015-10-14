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
* Author:  Colleen Bollin, NCBI
*
* File Description:
*   Unit tests for the field handlers.
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>

#include "unit_test_mail_report.hpp"
#include <corelib/ncbi_system.hpp>
#include <objects/seqtable/SeqTable_column.hpp>
#include <objects/seqtable/SeqTable_multi_data.hpp>
#include <objmgr/object_manager.hpp>
#include <objmgr/scope.hpp>

// This macro should be defined before inclusion of test_boost.hpp in all
// "*.cpp" files inside executable except one. It is like function main() for
// non-Boost.Test executables is defined only in one *.cpp file - other files
// should not include it. If NCBI_BOOST_NO_AUTO_TEST_MAIN will not be defined
// then test_boost.hpp will define such "main()" function for tests.
//
// Usually if your unit tests contain only one *.cpp file you should not
// care about this macro at all.
//
//#define NCBI_BOOST_NO_AUTO_TEST_MAIN


// This header must be included before all Boost.Test headers if there are any
#include <corelib/test_boost.hpp>

#include <corelib/ncbiapp.hpp>

#include <objtools/unit_test_util/unit_test_util.hpp>
#include <objtools/edit/mail_report.hpp>

#include <common/test_assert.h>  /* This header must go last */


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)





NCBITEST_INIT_TREE()
{
    if ( !CNcbiApplication::Instance()->GetConfig().HasEntry("NCBI", "Data") ) {
    }
}

static bool s_debugMode = false;

NCBITEST_INIT_CMDLINE(arg_desc)
{
    // Here we make descriptions of command line parameters that we are
    // going to use.

    arg_desc->AddFlag( "debug_mode",
        "Debugging mode writes errors seen for each test" );
}

NCBITEST_AUTO_INIT()
{
    // initialization function body

    const CArgs& args = CNcbiApplication::Instance()->GetArgs();
    if (args["debug_mode"]) {
        s_debugMode = true;
    }
}




BOOST_AUTO_TEST_CASE(Test_MailReport)
{
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodSeq();
    STANDARD_SETUP

    CRef<CSeq_table> table = edit::MakeMailReportPreReport(seh);

    unit_test_util::SetTaxname(entry, "Name1");
    edit::MakeMailReportPostReport(*table, scope);

    BOOST_CHECK_EQUAL(table->GetColumns()[1]->GetData().GetString()[0], "Sebaea microphylla");
    BOOST_CHECK_EQUAL(table->GetColumns()[2]->GetData().GetInt()[0], 0);
    BOOST_CHECK_EQUAL(table->GetColumns()[3]->GetData().GetString()[0], "Name1");
    BOOST_CHECK_EQUAL(table->GetColumns()[4]->GetData().GetInt()[0], 592768);
    BOOST_CHECK_EQUAL(table->GetColumns()[5]->GetData().GetInt()[0], 0);

    unit_test_util::SetTech(entry, CMolInfo::eTech_barcode);
    table = edit::MakeMailReportPreReport(seh);
    unit_test_util::SetTaxname(entry, "Name2");
    edit::MakeMailReportPostReport(*table, scope);

    BOOST_CHECK_EQUAL(table->GetColumns()[1]->GetData().GetString()[0], "Name1");
    BOOST_CHECK_EQUAL(table->GetColumns()[2]->GetData().GetInt()[0], 1);
    BOOST_CHECK_EQUAL(table->GetColumns()[3]->GetData().GetString()[0], "Name2");
    BOOST_CHECK_EQUAL(table->GetColumns()[4]->GetData().GetInt()[0], 592768);
    BOOST_CHECK_EQUAL(table->GetColumns()[5]->GetData().GetInt()[0], 0);

}


END_SCOPE(objects)
END_NCBI_SCOPE

