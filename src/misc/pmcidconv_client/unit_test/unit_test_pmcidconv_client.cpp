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
*   Sample unit tests file for the mainstream test developing.
*
* This file represents basic most common usage of Ncbi.Test framework (which
* is based on Boost.Test framework. For more advanced techniques look into
* another sample - unit_test_alt_sample.cpp.
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>

#include <corelib/ncbi_system.hpp>
#include <corelib/ncbiapp.hpp>
#include <objects/seq/Seqdesc.hpp>
#include <objtools/unit_test_util/unit_test_util.hpp>

#include <objects/seqfeat/BioSource.hpp>
#include <objects/seqfeat/Org_ref.hpp>
#include <objects/seqfeat/OrgName.hpp>
#include <objects/seqfeat/OrgMod.hpp>
#include <objects/seqfeat/SubSource.hpp>
#include <objects/general/Dbtag.hpp>
#include <common/ncbi_export.h>

#include <misc/pmcidconv_client/pmcidconv_client.hpp>



// This header must be included before all Boost.Test headers if there are any
#include <corelib/test_boost.hpp>

USING_NCBI_SCOPE;

// Needed under windows for some reason.
#ifdef BOOST_NO_EXCEPTIONS

namespace boost
{
    void throw_exception( std::exception const & e ) {
        throw e;
    }
}

#endif


BOOST_AUTO_TEST_CASE(TEST_First)
{
    vector<string> query_ids;
    CPMCIDSearch::TResults results;

    // PMC
    query_ids.push_back("PMC1193645");    
    CPMCIDSearch srch;
    BOOST_CHECK_EQUAL(srch.DoPMCIDSearch(query_ids, results), true);
    BOOST_CHECK_EQUAL(results.size(), query_ids.size());
    BOOST_CHECK_EQUAL(results[0], 14699080);

    //DOI
    query_ids.clear();
    query_ids.push_back("10.1111/j.1475-6773.2011.01339.x");
    BOOST_CHECK_EQUAL(srch.DoPMCIDSearch(query_ids, results), true);
    BOOST_CHECK_EQUAL(results.size(), query_ids.size());
    BOOST_CHECK_EQUAL(results[0], 22091849);

    // list of PMCS

    query_ids.clear();
    for (size_t i = 0; i < 250; i++) {
        string id = "PMC" + NStr::NumericToString(i + 1193645);
        query_ids.push_back(id);
    }
    BOOST_CHECK_EQUAL(srch.DoPMCIDSearch(query_ids, results), true);
    BOOST_CHECK_EQUAL(results.size(), query_ids.size());

}



