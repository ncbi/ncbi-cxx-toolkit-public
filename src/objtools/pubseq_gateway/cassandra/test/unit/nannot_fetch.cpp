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
* Author:  Dmitrii Saprykin, NCBI
*
* File Description:
*   Unit test suite to check CCassNAnnotTaskFetch task
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>

#include <memory>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include <corelib/ncbireg.hpp>

#include <objtools/pubseq_gateway/impl/cassandra/nannot_task/fetch.hpp>
#include <objtools/pubseq_gateway/impl/cassandra/cass_driver.hpp>
#include <objtools/pubseq_gateway/impl/cassandra/cass_factory.hpp>

namespace {

USING_NCBI_SCOPE;
USING_IDBLOB_SCOPE;

class CNAnnotTaskFetchTest
    : public testing::Test
{
 public:
    CNAnnotTaskFetchTest()
     : m_KeyspaceName("nannotg2")
     , m_Timeout(10000)
    {}

 protected:
    static void SetUpTestCase() {
        const string config_section = "TEST";
        CNcbiRegistry r;
        r.Set(config_section, "service", string(m_TestClusterName), IRegistry::fPersistent);
        m_Factory = CCassConnectionFactory::s_Create();
        m_Factory->LoadConfig(r, config_section);
        m_Connection = m_Factory->CreateInstance();
        m_Connection->Connect();
    }

    static void TearDownTestCase() {
        m_Connection->Close();
        m_Connection = nullptr;
        m_Factory = nullptr;
    }

    static const char* m_TestClusterName;
    static shared_ptr<CCassConnectionFactory> m_Factory;
    static shared_ptr<CCassConnection> m_Connection;

    string m_KeyspaceName;
    unsigned int m_Timeout;
};

const char* CNAnnotTaskFetchTest::m_TestClusterName = "ID_CASS_TEST";
shared_ptr<CCassConnectionFactory> CNAnnotTaskFetchTest::m_Factory(nullptr);
shared_ptr<CCassConnection> CNAnnotTaskFetchTest::m_Connection(nullptr);

TEST_F(CNAnnotTaskFetchTest, ListRetrieval) {
    size_t call_count = 0;
    CCassNAnnotTaskFetch fetch(
        m_Timeout, 0, m_Connection, m_KeyspaceName,
        "NW_017889270", 1, 10, vector<string>({"NA000122202.1"}),
        [&call_count](CNAnnotRecord && entry, bool last) -> bool {
            EXPECT_TRUE(call_count == 0 || (last && call_count == 1));
            ++call_count;
            if (!last) {
                EXPECT_EQ(entry.GetAnnotName(), "NA000122202.1");
                EXPECT_EQ(entry.GetStart(), 9853);
                EXPECT_EQ(entry.GetStop(), 9858);
            }
            return true;
        },
        [](CRequestStatus::ECode status, int code, EDiagSev severity, const string & message) {
            cout << "Error callback called: " << status << " " << code << " " << message << endl;
        }
    );
    bool done = fetch.Wait();
    while (!done) {
        done = fetch.Wait();
    }
}

TEST_F(CNAnnotTaskFetchTest, ListRetrievalTempString) {
    size_t call_count = 0;
    string naccession = "NA000122202.1";
    vector<CTempString> annot_names;
    annot_names.push_back(CTempString(naccession));
    CCassNAnnotTaskFetch fetch(
        m_Timeout, 0, m_Connection, m_KeyspaceName,
        "NW_017889270", 1, 10, annot_names,
        [&call_count](CNAnnotRecord && entry, bool last) -> bool {
            EXPECT_TRUE(call_count == 0 || (last && call_count == 1));
            ++call_count;
            if (!last) {
                EXPECT_EQ(entry.GetAnnotName(), "NA000122202.1");
                EXPECT_EQ(entry.GetStart(), 9853);
                EXPECT_EQ(entry.GetStop(), 9858);
            }
            return true;
        },
        [](CRequestStatus::ECode status, int code, EDiagSev severity, const string & message) {
            cout << "Error callback called: " << status << " " << code << " " << message << endl;
        }
    );
    bool done = fetch.Wait();
    while (!done) {
        done = fetch.Wait();
    }
}
}  // namespace
