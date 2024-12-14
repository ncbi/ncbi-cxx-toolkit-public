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
 * Author:  Justin Foley, Vitaly Stakhovsky, NCBI
 *
 * File Description:
 *   Unit tests for the field handlers.
 *
 * ===========================================================================
 */

#include <ncbi_pch.hpp>


#include <corelib/ncbi_system.hpp>

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
#include <objtools/edit/remote_updater.hpp>
#include <objtools/logging/listener.hpp>
#include <objtools/edit/eutils_updater.hpp>
#include <objects/seq/Seqdesc.hpp>

#include <common/test_assert.h>  /* This header must go last */


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)
USING_SCOPE(edit);


template <EPubmedError error_val>
class CPubmedUpdater_THROW : public CEUtilsUpdater
{
public:
    CPubmedUpdater_THROW() :
        CEUtilsUpdater(CEUtilsUpdater::ENormalize::On)
    {
    }

    CRef<CPub> GetPubmedEntry(TEntrezId pmid, EPubmedError* perr = nullptr) override
    {
        if (perr) {
            *perr = error_val;
        }
        return {};
    }
};

static CRef<CSeqdesc> s_CreateDescriptor(void)
{
    auto pDesc = Ref(new CSeqdesc());
    auto pPub = Ref(new CPub());
    pPub->SetPmid(CPubMedId(ENTREZ_ID_CONST(1234)));
    pDesc->SetPub().SetPub().Set().push_back(pPub);
    return pDesc;
}


class CCheckMsg {
public:
    CCheckMsg(const string& msg) :
        m_Expected(msg) {}

    bool operator()(const CException& e) {
        return e.GetMsg() == m_Expected;
    }

private:
    string m_Expected;
};

template <EPubmedError error_val>
static void s_SetPubmedClient(CRemoteUpdater& updater)
{
    updater.SetPubmedClient(new CPubmedUpdater_THROW<error_val>());
}


BOOST_AUTO_TEST_CASE(Test_RW_1130)
{
    auto pDesc = s_CreateDescriptor();
    CRemoteUpdater updater(nullptr);

    BOOST_CHECK_NO_THROW(updater.UpdatePubReferences(*pDesc));

    {
        CRemoteUpdater updater(nullptr);
        updater.SetPubmedClient(new CEUtilsUpdater());
        BOOST_CHECK_NO_THROW(updater.UpdatePubReferences(*pDesc));
    }

    {
        s_SetPubmedClient<EPubmedError::cannot_connect_pmdb>(updater);
        string expectedMsg = "Failed to retrieve publication for PMID 1234. "
            "3 attempts made. "
            "Pubmed error: cannot-connect-pmdb";
        BOOST_CHECK_EXCEPTION(updater.UpdatePubReferences(*pDesc),
                CException,
                CCheckMsg(expectedMsg));
    }

    {
        CRemoteUpdater updater(nullptr);
        s_SetPubmedClient<EPubmedError::cannot_connect_pmdb>(updater);

        string expectedMsg = "Failed to retrieve publication for PMID 1234. "
            "3 attempts made. "
            "Pubmed error: cannot-connect-pmdb";
        BOOST_CHECK_EXCEPTION(updater.UpdatePubReferences(*pDesc),
                CException,
                CCheckMsg(expectedMsg));
    }

    updater.SetMaxMlaAttempts(4);
    s_SetPubmedClient<EPubmedError::cannot_connect_searchbackend_pmdb>(updater);
    {
        string expectedMsg = "Failed to retrieve publication for PMID 1234. "
            "4 attempts made. "
            "Pubmed error: cannot-connect-searchbackend-pmdb";
        BOOST_CHECK_EXCEPTION(updater.UpdatePubReferences(*pDesc),
                CException,
                CCheckMsg(expectedMsg));
    }

    {
        s_SetPubmedClient<EPubmedError::not_found>(updater);
        string expectedMsg = "Failed to retrieve publication for PMID 1234. "
            "Pubmed error: not-found";
        BOOST_CHECK_EXCEPTION(updater.UpdatePubReferences(*pDesc),
                CException,
                CCheckMsg(expectedMsg));

        BOOST_CHECK_THROW(updater.UpdatePubReferences(*pDesc), CException);
    }

    {
        CObjtoolsListener messageListener;
        CRemoteUpdater updater(&messageListener);
        s_SetPubmedClient<EPubmedError::not_found>(updater);
        BOOST_CHECK_NO_THROW(updater.UpdatePubReferences(*pDesc));
    }
}


static void check_pubmed_error(EPubmedError err, string msg)
{
    CNcbiOstrstream oss;
    oss << err;
    BOOST_CHECK_EQUAL(msg, CNcbiOstrstreamToString(oss));
}

BOOST_AUTO_TEST_CASE(Test_PubmedError)
{
    check_pubmed_error(EPubmedError::not_found, "not-found");
    check_pubmed_error(EPubmedError::operational_error, "operational-error");
    check_pubmed_error(EPubmedError::citation_not_found, "citation-not-found");
    check_pubmed_error(EPubmedError::citation_ambiguous, "citation-ambiguous");
}


END_SCOPE(objects)
END_NCBI_SCOPE

