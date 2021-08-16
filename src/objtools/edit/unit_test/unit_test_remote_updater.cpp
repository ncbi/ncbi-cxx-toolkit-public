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
* Author:  Justin Foley, NCBI
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
#include <objects/mla/mla_client.hpp>
#include <objects/seq/Seqdesc.hpp>

#include <common/test_assert.h>  /* This header must go last */


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)
USING_SCOPE(edit);

template<EError_val MLAError_val>
class CMLAClient_THROW : public CMLAClient
{
public:
    virtual ~CMLAClient_THROW(){};

    using TReply = CMla_back;
    CRef<CPub> AskGetpubpmid
        (const CPubMedId& req, TReply* reply=0) override;

};


template<EError_val MLAError_val>
CRef<CPub> CMLAClient_THROW<MLAError_val>::AskGetpubpmid(const CPubMedId& req, CMla_back* reply)
{
    if (reply) {
        reply->SetError(MLAError_val);
    }

    CNcbiOstrstream oss;
    oss << MLAError_val;
    NCBI_THROW(CException, eUnknown, CNcbiOstrstreamToString(oss));
}


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


BOOST_AUTO_TEST_CASE(Test_RW_1130)
{
    CRef<CMLAClient> pMLAClient(new CMLAClient_THROW<eError_val_cannot_connect_pmdb>());
    auto pDesc = s_CreateDescriptor();
    CRemoteUpdater updater;
    {
        updater.SetMLAClient(*pMLAClient);
        string expectedMsg = "Failed to retrieve publication for PMID 1234. "
            "3 attempts made. "
            "CMLAClient : cannot-connect-pmdb";
        BOOST_CHECK_EXCEPTION(updater.UpdatePubReferences(*pDesc),
                CException,
                CCheckMsg(expectedMsg));
    }


    updater.SetMaxMlaAttempts(4);
    pMLAClient.Reset(new CMLAClient_THROW<eError_val_cannot_connect_searchbackend_pmdb>());
    updater.SetMLAClient(*pMLAClient);
    {
        string expectedMsg = "Failed to retrieve publication for PMID 1234. "
        "4 attempts made. "
         "CMLAClient : cannot-connect-searchbackend-pmdb";
        BOOST_CHECK_EXCEPTION(updater.UpdatePubReferences(*pDesc),
                CException,
                CCheckMsg(expectedMsg));
    }


    pMLAClient.Reset(new CMLAClient_THROW<eError_val_not_found>());
    {
        updater.SetMLAClient(*pMLAClient);
        string expectedMsg = "Failed to retrieve publication for PMID 1234. "
            "CMLAClient : not-found";
        BOOST_CHECK_EXCEPTION(updater.UpdatePubReferences(*pDesc),
                CException,
                CCheckMsg(expectedMsg));

        BOOST_CHECK_THROW(updater.UpdatePubReferences(*pDesc), CException);
    }


    {
        CRemoteUpdater updater(nullptr);
        updater.SetMLAClient(*pMLAClient);
        string expectedMsg = "Failed to retrieve publication for PMID 1234. "
            "CMLAClient : not-found";
        BOOST_CHECK_EXCEPTION(updater.UpdatePubReferences(*pDesc),
                CException,
                CCheckMsg(expectedMsg));

        BOOST_CHECK_THROW(updater.UpdatePubReferences(*pDesc), CException);
    }


    {
        CObjtoolsListener messageListener;
        CRemoteUpdater updater(&messageListener);
        updater.SetMLAClient(*pMLAClient);
        BOOST_CHECK_NO_THROW(updater.UpdatePubReferences(*pDesc));
    }
}


END_SCOPE(objects)
END_NCBI_SCOPE

