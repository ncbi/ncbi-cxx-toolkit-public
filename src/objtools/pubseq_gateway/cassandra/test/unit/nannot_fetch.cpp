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
#include <mutex>
#include <condition_variable>
#include <string>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include <corelib/ncbireg.hpp>

#include <serial/serial.hpp>
#include <serial/objistr.hpp>
#include <serial/objostr.hpp>

#include <objects/seqsplit/ID2S_Seq_annot_Info.hpp>

#include <objtools/pubseq_gateway/impl/cassandra/nannot_task/fetch.hpp>
#include <objtools/pubseq_gateway/impl/cassandra/cass_driver.hpp>
#include <objtools/pubseq_gateway/impl/cassandra/cass_factory.hpp>

#include "test_environment.hpp"

namespace {

USING_NCBI_SCOPE;
USING_IDBLOB_SCOPE;
USING_SCOPE(objects);

class CNAnnotTaskFetchTest
    : public testing::Test
{
public:
    CNAnnotTaskFetchTest() = default;

protected:
    static void SetUpTestCase() {
        const string config_section = "TEST";
        CNcbiRegistry r;
        r.Set(config_section, "service", string(m_TestClusterName), IRegistry::fPersistent);
        sm_Env.Get().SetUp(&r, config_section);
    }

    static void TearDownTestCase() {
        sm_Env.Get().TearDown();
    }

    static const char* m_TestClusterName;
    static CSafeStatic<STestEnvironment> sm_Env;

    string m_KeyspaceName{"nannotg3"};
    string m_FrozenKeyspaceName{"psg_test_sat_41"};
    string m_NewNannotSchemaKeyspace{"psg_test_sat_52"};
};

class CCassNAnnotTaskFetchWithTimeout : public CCassNAnnotTaskFetch
{
public:
    CCassNAnnotTaskFetchWithTimeout(
        shared_ptr<CCassConnection> connection,
        const string & keyspace,
        string accession,
        int16_t version,
        int16_t seq_id_type,
        TNAnnotConsumeCallback consume_callback,
        TDataErrorCallback data_error_cb
    )
        : CCassNAnnotTaskFetch(std::move(connection),
            keyspace, accession, version, seq_id_type,
                               std::move(consume_callback), std::move(data_error_cb)
        )
        , m_CallCount(nullptr)
        , m_RestartTrigger(0)
        , m_TestRestartDone(false)
    {
        m_PageSize = 1;
    }

    CCassNAnnotTaskFetchWithTimeout(
        shared_ptr<CCassConnection> connection,
        const string & keyspace,
        string accession,
        int16_t version,
        int16_t seq_id_type,
        const vector<string> & annot_names,
        TNAnnotConsumeCallback consume_callback,
        TDataErrorCallback data_error_cb
    )
        : CCassNAnnotTaskFetch(std::move(connection),
            keyspace, accession, version, seq_id_type, annot_names,
                               std::move(consume_callback), std::move(data_error_cb)
        )
        , m_CallCount(nullptr)
        , m_RestartTrigger(0)
        , m_TestRestartDone(false)
    {
        m_PageSize = 1;
    }

    virtual void Wait1() override
    {
        bool restartable_state = m_State != EBlobWaiterState::eInit
            && m_State != EBlobWaiterState::eDone
            && m_State != EBlobWaiterState::eError;
        if (restartable_state && !m_TestRestartDone
            && m_RestartTrigger != 0
            && m_CallCount
            && *m_CallCount == m_RestartTrigger
        ) {
            ++m_RestartCounter;
            m_State = EBlobWaiterState::eInit;
            m_TestRestartDone = true;
        }
        // cout << "Wait1 called " << (m_CallCount ? NStr::NumericToString(*m_CallCount) : " null ") << endl;
        CCassNAnnotTaskFetch::Wait1();
    }

    void SetCallCount(size_t* value)
    {
        m_CallCount = value;
    }

    void SetRestartTriggerCount(size_t value)
    {
        m_RestartTrigger = value;
    }

    bool RestartTriggered() const
    {
        return m_TestRestartDone;
    }

    bool HasErrorState() const
    {
        return m_State == EBlobWaiterState::eError;
    }

 private:
    size_t* m_CallCount;
    size_t m_RestartTrigger;
    bool m_TestRestartDone;
};

string gGetSeqAnnotInfoString(CNAnnotRecord* record)
{
    istringstream ss(record->GetSeqAnnotInfo());
    unique_ptr<CObjectIStream> obj_strm(CObjectIStream::Open(eSerial_AsnBinary, ss));
    ostringstream oss;
    unique_ptr<CObjectOStream> out(
        CObjectOStream::Open(
            eSerial_AsnText, oss, eNoOwnership,
            fSerial_AsnText_NoIndentation | fSerial_AsnText_NoEol
        )
    );
    while (!obj_strm->EndOfData()) {
        CID2S_Seq_annot_Info annot_info;
        (*obj_strm) >> annot_info;
        *out << annot_info;
    }
    return oss.str();
}

const char* CNAnnotTaskFetchTest::m_TestClusterName = "ID_CASS_TEST";
CSafeStatic<STestEnvironment> CNAnnotTaskFetchTest::sm_Env;

TEST_F(CNAnnotTaskFetchTest, SingleRetrievalVector) {
    size_t call_count = 0;
    CCassNAnnotTaskFetch fetch(
        sm_Env.Get().connection, m_KeyspaceName,
        "NW_017889270", 1, 10, vector<string>({"NA000122202.1"}),
        [&call_count](CNAnnotRecord && entry, bool last) -> bool {
            EXPECT_TRUE(call_count == 0 || (last && call_count == 1));
            ++call_count;
            if (!last) {
                EXPECT_EQ(entry.GetAnnotName(), "NA000122202.1");
                EXPECT_EQ(entry.GetStart(), 9853);
                EXPECT_EQ(entry.GetStop(), 9858);
                EXPECT_EQ(entry.GetState(), CNAnnotRecord::eStateAlive);
            }
            return true;
        },
        [](CRequestStatus::ECode status, int code, EDiagSev severity, const string & message) {
            EXPECT_TRUE(false) << "Error callback called during the test (status - "
                << status << ", code - " << code << ", message - '" << message << "')";
        }
    );
    bool done = fetch.Wait();
    while (!done) {
        usleep(100);
        done = fetch.Wait();
    }
}

TEST_F(CNAnnotTaskFetchTest, SingleRetrievalTempString) {
    size_t call_count = 0;
    string naccession = "NA000122202.1";
    vector<CTempString> annot_names;
    annot_names.push_back(CTempString(naccession));
    CCassNAnnotTaskFetch fetch(
        sm_Env.Get().connection, m_KeyspaceName,
        "NW_017889270", 1, 10, annot_names,
        [&call_count](CNAnnotRecord && entry, bool last) -> bool {
            EXPECT_TRUE(call_count == 0 || (last && call_count == 1));
            ++call_count;
            if (!last) {
                EXPECT_EQ(entry.GetAnnotName(), "NA000122202.1");
                EXPECT_EQ(entry.GetStart(), 9853);
                EXPECT_EQ(entry.GetStop(), 9858);
                EXPECT_EQ(entry.GetState(), CNAnnotRecord::eStateAlive);
            }
            return true;
        },
        [](CRequestStatus::ECode status, int code, EDiagSev severity, const string & message) {
            EXPECT_TRUE(false) << "Error callback called during the test (status - "
                << status << ", code - " << code << ", message - '" << message << "')";
        }
    );
    bool done = fetch.Wait();
    while (!done) {
        usleep(100);
        done = fetch.Wait();
    }
}

TEST_F(CNAnnotTaskFetchTest, MultipleRetrieval) {
    size_t call_count = 0;
    CCassNAnnotTaskFetch fetch(
        sm_Env.Get().connection, m_KeyspaceName,
        "NW_017889270", 1, 10,
        [&call_count](CNAnnotRecord && entry, bool last) -> bool {
            switch(++call_count) {
                case 1:
                    EXPECT_EQ(entry.GetAnnotName(), "NA000122202.1");
                    EXPECT_EQ(entry.GetStart(), 9853);
                    EXPECT_EQ(entry.GetStop(), 9858);
                    EXPECT_EQ(entry.GetSatKey(), 24715753);
                    EXPECT_EQ(entry.GetState(), CNAnnotRecord::eStateAlive);
                    EXPECT_FALSE(last);
                    return true;
                case 2:
                    EXPECT_EQ(entry.GetAnnotName(), "NA000122203.1");
                    EXPECT_EQ(entry.GetStart(), 2506);
                    EXPECT_EQ(entry.GetStop(), 8119);
                    EXPECT_EQ(entry.GetSatKey(), 24734212);
                    EXPECT_EQ(entry.GetState(), CNAnnotRecord::eStateAlive);
                    EXPECT_FALSE(last);
                    return true;
                case 3:
                    EXPECT_EQ(entry.GetAnnotName(), "NA000122204.1");
                    EXPECT_EQ(entry.GetStart(), 640);
                    EXPECT_EQ(entry.GetStop(), 5865);
                    EXPECT_EQ(entry.GetSatKey(), 24754284);
                    EXPECT_EQ(entry.GetState(), CNAnnotRecord::eStateAlive);
                    EXPECT_FALSE(last);
                    return true;
                case 4:
                    EXPECT_EQ(entry.GetAnnotName(), "NA000122205.1");
                    EXPECT_EQ(entry.GetStart(), 992);
                    EXPECT_EQ(entry.GetStop(), 1445);
                    EXPECT_EQ(entry.GetSatKey(), 24773337);
                    EXPECT_EQ(entry.GetState(), CNAnnotRecord::eStateAlive);
                    EXPECT_FALSE(last);
                    return false;
                case 5:
                    EXPECT_TRUE(last);
                    return false;
                default:
                    EXPECT_TRUE(false) << "Callback should not be called " << call_count << " times.";
            }
            return true;
        },
        [](CRequestStatus::ECode status, int code, EDiagSev severity, const string & message) {
            EXPECT_TRUE(false) << "Error callback called during the test (status - "
                << status << ", code - " << code << ", message - '" << message << "')";
        }
    );
    bool done = fetch.Wait();
    while (!done) {
        usleep(100);
        done = fetch.Wait();
    }
}

TEST_F(CNAnnotTaskFetchTest, MultipleRetrievalWithTimeout) {
    size_t call_count = 0;
    CCassNAnnotTaskFetchWithTimeout fetch(
        sm_Env.Get().connection, m_KeyspaceName,
        "NW_017889270", 1, 10,
        [&call_count](CNAnnotRecord && entry, bool last) -> bool {
            switch(++call_count) {
                case 1:
                    EXPECT_EQ(entry.GetAnnotName(), "NA000122202.1");
                    EXPECT_EQ(entry.GetStart(), 9853);
                    EXPECT_EQ(entry.GetStop(), 9858);
                    EXPECT_EQ(entry.GetSatKey(), 24715753);
                    EXPECT_EQ(entry.GetState(), CNAnnotRecord::eStateAlive);
                    EXPECT_FALSE(last);
                    return true;
                case 2:
                    EXPECT_EQ(entry.GetAnnotName(), "NA000122203.1");
                    EXPECT_EQ(entry.GetStart(), 2506);
                    EXPECT_EQ(entry.GetStop(), 8119);
                    EXPECT_EQ(entry.GetSatKey(), 24734212);
                    EXPECT_EQ(entry.GetState(), CNAnnotRecord::eStateAlive);
                    EXPECT_FALSE(last);
                    return true;
                case 3:
                    EXPECT_EQ(entry.GetAnnotName(), "NA000122204.1");
                    EXPECT_EQ(entry.GetStart(), 640);
                    EXPECT_EQ(entry.GetStop(), 5865);
                    EXPECT_EQ(entry.GetSatKey(), 24754284);
                    EXPECT_EQ(entry.GetState(), CNAnnotRecord::eStateAlive);
                    EXPECT_FALSE(last);
                    return true;
                case 4:
                    EXPECT_EQ(entry.GetAnnotName(), "NA000122205.1");
                    EXPECT_EQ(entry.GetStart(), 992);
                    EXPECT_EQ(entry.GetStop(), 1445);
                    EXPECT_EQ(entry.GetSatKey(), 24773337);
                    EXPECT_EQ(entry.GetState(), CNAnnotRecord::eStateAlive);
                    EXPECT_FALSE(last);
                    return false;
                case 5:
                    EXPECT_TRUE(last);
                    return false;
                default:
                    EXPECT_TRUE(false) << "Callback should not be called " << call_count << " times.";
            }
            return true;
        },
        [](CRequestStatus::ECode status, int code, EDiagSev severity, const string & message) {
            EXPECT_TRUE(false) << "Error callback called during the test (status - "
                << status << ", code - " << code << ", message - '" << message << "')";
        }
    );
    fetch.SetCallCount(&call_count);
    fetch.SetRestartTriggerCount(3);
    bool done = fetch.Wait();
    while (!done) {
        usleep(1000);
        done = fetch.Wait();
    }
    fetch.SetCallCount(nullptr);
    EXPECT_TRUE(fetch.RestartTriggered());
    EXPECT_FALSE(fetch.Cancelled());
    EXPECT_FALSE(fetch.HasErrorState());
}

TEST_F(CNAnnotTaskFetchTest, ListRetrievalWithTimeout) {
    size_t call_count = 0;
    CCassNAnnotTaskFetchWithTimeout fetch(
        sm_Env.Get().connection, m_KeyspaceName,
        "NW_017889270", 1, 10, vector<string>({"NA000122203.1", "NA000122202.1", "NA000122204.1"}),
        [&call_count](CNAnnotRecord && entry, bool last) -> bool {
            switch(++call_count) {
                case 1:
                    EXPECT_EQ(entry.GetAnnotName(), "NA000122202.1");
                    EXPECT_EQ(entry.GetStart(), 9853);
                    EXPECT_EQ(entry.GetStop(), 9858);
                    EXPECT_EQ(entry.GetSatKey(), 24715753);
                    EXPECT_EQ(entry.GetState(), CNAnnotRecord::eStateAlive);
                    EXPECT_FALSE(last);
                    return true;
                case 2:
                    EXPECT_EQ(entry.GetAnnotName(), "NA000122203.1");
                    EXPECT_EQ(entry.GetStart(), 2506);
                    EXPECT_EQ(entry.GetStop(), 8119);
                    EXPECT_EQ(entry.GetSatKey(), 24734212);
                    EXPECT_EQ(entry.GetState(), CNAnnotRecord::eStateAlive);
                    EXPECT_FALSE(last);
                    return true;
                case 3:
                    EXPECT_EQ(entry.GetAnnotName(), "NA000122204.1");
                    EXPECT_EQ(entry.GetStart(), 640);
                    EXPECT_EQ(entry.GetStop(), 5865);
                    EXPECT_EQ(entry.GetSatKey(), 24754284);
                    EXPECT_EQ(entry.GetState(), CNAnnotRecord::eStateAlive);
                    EXPECT_FALSE(last);
                    return true;
                case 4:
                    EXPECT_TRUE(last);
                    return false;
                default:
                    EXPECT_TRUE(false) << "Callback should not be called " << call_count << " times.";
            }
            return true;
        },
        [](CRequestStatus::ECode status, int code, EDiagSev severity, const string & message) {
            EXPECT_TRUE(false) << "Error callback called during the test (status - "
                << status << ", code - " << code << ", message - '" << message << "')";
        }
    );
    fetch.SetCallCount(&call_count);
    fetch.SetRestartTriggerCount(2);
    bool done = fetch.Wait();
    while (!done) {
        usleep(1000);
        done = fetch.Wait();
    }
    fetch.SetCallCount(nullptr);
    EXPECT_TRUE(fetch.RestartTriggered());
    EXPECT_FALSE(fetch.Cancelled());
    EXPECT_FALSE(fetch.HasErrorState());
}

TEST_F(CNAnnotTaskFetchTest, ListRetrievalWithTimeoutOnEOF) {
    size_t call_count = 0;
    CCassNAnnotTaskFetchWithTimeout fetch(
        sm_Env.Get().connection, m_KeyspaceName,
        "NW_017889270", 1, 10, vector<string>({"NA000122203.1", "NA000122202.1", "NA000122204.1"}),
        [&call_count](CNAnnotRecord && entry, bool last) -> bool {
            switch(++call_count) {
                case 1:
                    EXPECT_EQ(entry.GetAnnotName(), "NA000122202.1");
                    EXPECT_EQ(entry.GetStart(), 9853);
                    EXPECT_EQ(entry.GetStop(), 9858);
                    EXPECT_EQ(entry.GetSatKey(), 24715753);
                    EXPECT_EQ(entry.GetState(), CNAnnotRecord::eStateAlive);
                    EXPECT_FALSE(last);
                    return true;
                case 2:
                    EXPECT_EQ(entry.GetAnnotName(), "NA000122203.1");
                    EXPECT_EQ(entry.GetStart(), 2506);
                    EXPECT_EQ(entry.GetStop(), 8119);
                    EXPECT_EQ(entry.GetSatKey(), 24734212);
                    EXPECT_EQ(entry.GetState(), CNAnnotRecord::eStateAlive);
                    EXPECT_FALSE(last);
                    return true;
                case 3:
                    EXPECT_EQ(entry.GetAnnotName(), "NA000122204.1");
                    EXPECT_EQ(entry.GetStart(), 640);
                    EXPECT_EQ(entry.GetStop(), 5865);
                    EXPECT_EQ(entry.GetSatKey(), 24754284);
                    EXPECT_EQ(entry.GetState(), CNAnnotRecord::eStateAlive);
                    EXPECT_FALSE(last);
                    return true;
                case 4:
                    EXPECT_TRUE(last);
                    return false;
                default:
                    EXPECT_TRUE(false) << "Callback should not be called " << call_count << " times.";
            }
            return true;
        },
        [](CRequestStatus::ECode status, int code, EDiagSev severity, const string & message) {
            EXPECT_TRUE(false) << "Error callback called during the test (status - "
                << status << ", code - " << code << ", message - '" << message << "')";
        }
    );
    fetch.SetCallCount(&call_count);
    fetch.SetRestartTriggerCount(3);
    bool done = fetch.Wait();
    while (!done) {
        usleep(1000);
        done = fetch.Wait();
    }
    fetch.SetCallCount(nullptr);
    EXPECT_TRUE(fetch.RestartTriggered());
    fetch.SetConsumeCallback(nullptr);
    EXPECT_TRUE(fetch.Wait());
    EXPECT_FALSE(fetch.Cancelled());
    EXPECT_FALSE(fetch.HasErrorState());
}

TEST_F(CNAnnotTaskFetchTest, ListRetrievalWithCancel) {
    size_t call_count = 0;
    CCassNAnnotTaskFetchWithTimeout fetch(
        sm_Env.Get().connection, m_KeyspaceName,
        "NW_017889270", 1, 10, vector<string>({"NA000122203.1", "NA000122202.1", "NA000122204.1"}),
        [&call_count](CNAnnotRecord && entry, bool last) -> bool {
            switch(++call_count) {
                case 1:
                    EXPECT_EQ(entry.GetAnnotName(), "NA000122202.1");
                    EXPECT_EQ(entry.GetStart(), 9853);
                    EXPECT_EQ(entry.GetStop(), 9858);
                    EXPECT_EQ(entry.GetSatKey(), 24715753);
                    EXPECT_EQ(entry.GetState(), CNAnnotRecord::eStateAlive);
                    EXPECT_FALSE(last);
                    return true;
                case 2:
                    EXPECT_EQ(entry.GetAnnotName(), "NA000122203.1");
                    EXPECT_EQ(entry.GetStart(), 2506);
                    EXPECT_EQ(entry.GetStop(), 8119);
                    EXPECT_EQ(entry.GetSatKey(), 24734212);
                    EXPECT_EQ(entry.GetState(), CNAnnotRecord::eStateAlive);
                    EXPECT_FALSE(last);
                    return true;
                default:
                    EXPECT_TRUE(false) << "Callback should not be called " << call_count << " times.";
            }
            return true;
        },
        [](CRequestStatus::ECode status, int code, EDiagSev severity, const string & message) {
            EXPECT_TRUE(false) << "Error callback called during the test (status - "
                << status << ", code - " << code << ", message - '" << message << "')";
        }
    );
    bool done = fetch.Wait();
    bool cancelled = false;
    while (!done) {
        usleep(1000);
        done = fetch.Wait();
        if (call_count == 2 && !cancelled) {
            fetch.Cancel();
            cancelled = true;
        }
    }
    EXPECT_FALSE(fetch.RestartTriggered());
    fetch.SetConsumeCallback(nullptr);
    EXPECT_TRUE(fetch.Wait());
    EXPECT_TRUE(fetch.Cancelled());
    EXPECT_TRUE(fetch.HasErrorState());
}

TEST_F(CNAnnotTaskFetchTest, RetrievalWithSeqAnnotInfo) {
    size_t call_count = 0;
    CCassNAnnotTaskFetch fetch(
        sm_Env.Get().connection, m_KeyspaceName,
        "NW_017889735", 1, 10, vector<string>({"NA000122202.1"}),
        [&call_count](CNAnnotRecord && entry, bool last) -> bool {
            switch(++call_count) {
                case 1:
                    EXPECT_EQ(entry.GetAnnotName(), "NA000122202.1");
                    EXPECT_EQ(entry.GetStart(), 6616);
                    EXPECT_EQ(entry.GetStop(), 41589);
                    EXPECT_EQ(entry.GetSatKey(), 24715365);
                    EXPECT_EQ(entry.GetAnnotInfoModified(), 1658874804940);
                    EXPECT_FALSE(entry.GetSeqAnnotInfo().empty());
                    EXPECT_EQ(gGetSeqAnnotInfoString(&entry),
                        "ID2S-Seq-annot-Info ::= {name \"NA000122202.1\",feat {{type 13}},seq-loc gi-interval {gi 1142972004,start 6616,length 34974}}\n"
                        "ID2S-Seq-annot-Info ::= {name \"NA000122202.1@@10\",graph NULL,seq-loc gi-interval {gi 1142972004,start 6610,length 34980}}\n"
                        "ID2S-Seq-annot-Info ::= {name \"NA000122202.1@@100\",graph NULL,seq-loc gi-interval {gi 1142972004,start 6610,length 35000}}\n");
                    EXPECT_EQ(entry.GetState(), CNAnnotRecord::eStateAlive);
                    EXPECT_FALSE(last);
                    return true;
                case 2:
                    EXPECT_TRUE(last);
                    return false;
                default:
                    EXPECT_TRUE(false) << "Callback should not be called " << call_count << " times.";
            }
            return true;
        },
        [](CRequestStatus::ECode status, int code, EDiagSev severity, const string & message) {
            EXPECT_TRUE(false) << "Error callback called during the test (status - "
                << status << ", code - " << code << ", message - '" << message << "')";
        }
    );
    bool done = fetch.Wait();
    while (!done) {
        usleep(1000);
        done = fetch.Wait();
    }
    fetch.SetConsumeCallback(nullptr);
    EXPECT_EQ(call_count, 2UL);
}

TEST_F(CNAnnotTaskFetchTest, DISABLED_RetrievalWithSeqAnnotInfo2) {
    size_t call_count = 0;
    CCassNAnnotTaskFetch fetch(
        sm_Env.Get().connection, m_KeyspaceName,
        "NW_020201082", 1, 10, vector<string>({"NA000156740.1"}),
        [&call_count](CNAnnotRecord && entry, bool last) -> bool {
            switch(++call_count) {
                case 1:
                    EXPECT_EQ(entry.GetAnnotName(), "NA000156740.1");
                    EXPECT_EQ(entry.GetStart(), 14153);
                    EXPECT_EQ(entry.GetStop(), 77991);
                    EXPECT_EQ(entry.GetSatKey(), 26395559);
                    EXPECT_EQ(entry.GetAnnotInfoModified(), 1652726171060);
                    EXPECT_FALSE(entry.GetSeqAnnotInfo().empty());
                    EXPECT_EQ(gGetSeqAnnotInfoString(&entry),
                        "ID2S-Seq-annot-Info ::= {name \"NA000156740.1\",feat {{type 8,subtypes {56}}},seq-loc gi-interval {gi 1385009444,start 14153,length 63839}}\n"
                        "ID2S-Seq-annot-Info ::= {name \"NA000156740.1@@10\",graph NULL,seq-loc gi-interval {gi 1385009444,start 14150,length 63850}}\n"
                        "ID2S-Seq-annot-Info ::= {name \"NA000156740.1@@100\",graph NULL,seq-loc gi-interval {gi 1385009444,start 14150,length 63900}}\n"
                    );
                    EXPECT_EQ(entry.GetState(), CNAnnotRecord::eStateAlive);
                    EXPECT_FALSE(last);
                    return true;
                case 2:
                    EXPECT_TRUE(last);
                    return false;
                default:
                    EXPECT_TRUE(false) << "Callback should not be called " << call_count << " times.";
            }
            return true;
        },
        [](CRequestStatus::ECode status, int code, EDiagSev severity, const string & message) {
            EXPECT_TRUE(false) << "Error callback called during the test (status - "
                << status << ", code - " << code << ", message - '" << message << "')";
        }
    );
    bool done = fetch.Wait();
    while (!done) {
        usleep(1000);
        done = fetch.Wait();
    }
    fetch.SetConsumeCallback(nullptr);
    EXPECT_EQ(call_count, 2UL);
}

TEST_F(CNAnnotTaskFetchTest, RetrievalWithDeadRecords) {
    size_t call_count = 0;
    CCassNAnnotTaskFetch fetch(
        sm_Env.Get().connection, m_FrozenKeyspaceName,
        "NW_019824444", 1, 10,
        [&call_count](CNAnnotRecord && entry, bool last) -> bool {
            switch(++call_count) {
                case 1:
                    EXPECT_EQ(entry.GetAnnotName(), "NA000888889.1");
                    EXPECT_EQ(entry.GetStart(), 777);
                    EXPECT_EQ(entry.GetStop(), 1234);
                    EXPECT_EQ(entry.GetSatKey(), 222222222);
                    EXPECT_EQ(entry.GetState(), CNAnnotRecord::eStateAlive);
                    EXPECT_FALSE(last);
                    return true;
                case 2:
                    EXPECT_TRUE(last);
                    return false;
                default:
                    EXPECT_TRUE(false) << "Callback should not be called " << call_count << " times.";
            }
            return true;
        },
        [](CRequestStatus::ECode status, int code, EDiagSev severity, const string & message) {
            EXPECT_TRUE(false) << "Error callback called during the test (status - "
                << status << ", code - " << code << ", message - '" << message << "')";
        }
    );
    bool done = fetch.Wait();
    while (!done) {
        usleep(100);
        done = fetch.Wait();
    }
}

TEST_F(CNAnnotTaskFetchTest, RetrievalWithSatKeyInPrimary) {
    size_t call_count = 0;
    CCassNAnnotTaskFetch fetch(
        sm_Env.Get().connection, m_NewNannotSchemaKeyspace,
        "NC_062543", 1, 10, vector<string>({"NA000353807.1"}),
        [&call_count](CNAnnotRecord && entry, bool last) -> bool {
            switch(++call_count) {
                case 1:
                    EXPECT_EQ(entry.GetAnnotName(), "NA000353807.1");
                    EXPECT_EQ(entry.GetStart(), 447);
                    EXPECT_EQ(entry.GetStop(), 12505360);
                    EXPECT_EQ(entry.GetSatKey(), 8754056);
                    EXPECT_EQ(entry.GetState(), CNAnnotRecord::eStateAlive);
                    EXPECT_FALSE(last);
                    return true;
                case 2:
                    EXPECT_EQ(entry.GetAnnotName(), "NA000353807.1");
                    EXPECT_EQ(entry.GetStart(), 11751284);
                    EXPECT_EQ(entry.GetStop(), 16972309);
                    EXPECT_EQ(entry.GetSatKey(), 8754057);
                    EXPECT_EQ(entry.GetState(), CNAnnotRecord::eStateAlive);
                    EXPECT_FALSE(last);
                    return true;
                case 3:
                    EXPECT_TRUE(last);
                    return false;
                default:
                    EXPECT_TRUE(false) << "Callback should not be called " << call_count << " times.";
            }
            return true;
        },
        [](CRequestStatus::ECode status, int code, EDiagSev severity, const string & message) {
            EXPECT_TRUE(false) << "Error callback called during the test (status - "
                << status << ", code - " << code << ", message - '" << message << "')";
        }
    );
    bool done = fetch.Wait();
    while (!done) {
        usleep(100);
        done = fetch.Wait();
    }
}

TEST_F(CNAnnotTaskFetchTest, RetrievalWithShortTimeoutAndRetry)
{
    bool fetch_finished{false}, timeout_called{false};
    bool false_negative{false};
    auto timeout_function =
        [&timeout_called, &fetch_finished]
        (CRequestStatus::ECode status, int code, EDiagSev severity, const string & message)
        {
            timeout_called = true;
            EXPECT_EQ(CRequestStatus::e502_BadGateway, status);
            EXPECT_EQ(CCassandraException::eQueryTimeout, code);
            EXPECT_EQ(eDiag_Error, severity);
            fetch_finished = true;
        };
    auto fetch_callback =
        [&fetch_finished, &false_negative]
        (CNAnnotRecord &&, bool last) -> bool {
            if (last) {
                fetch_finished = true;
                false_negative = true;
                cout << "LAST NANNOT RECEIVED: TEST RESULT IS FALSE NEGATIVE" << endl;
            }
            return true;
        };

    auto old_retry_timeout = sm_Env.Get().connection->QryTimeoutRetryMs();
    sm_Env.Get().connection->SetQueryTimeoutRetry(1);
    CCassNAnnotTaskFetch fetch(
        sm_Env.Get().connection, "psg_test_sat_41", "LS480640", 104, 6,
        fetch_callback, timeout_function
    );
    fetch.SetQueryTimeout(std::chrono::milliseconds(1));
    fetch.SetMaxRetries(2);

    atomic_bool has_events{false};
    mutex wait_mutex;
    condition_variable wait_condition;

    struct STestCallbackReceiver
        : public CCassDataCallbackReceiver
    {
        void OnData() override {
            {
                unique_lock<mutex> wait_lck(*cb_wait_mutex);
                *cb_has_events = true;
            }
            ++times_called;
            cb_wait_condition->notify_all();
        }

        atomic_bool* cb_has_events{nullptr};
        mutex* cb_wait_mutex{nullptr};
        condition_variable* cb_wait_condition{nullptr};
        atomic_int times_called{0};
    };

    auto event_callback = make_shared<STestCallbackReceiver>();
    event_callback->cb_has_events = &has_events;
    event_callback->cb_wait_mutex = &wait_mutex;
    event_callback->cb_wait_condition = &wait_condition;
    fetch.SetDataReadyCB(event_callback);

    auto wait_function =
    [&has_events, &wait_condition, &wait_mutex, &fetch_finished, &fetch, &false_negative]() {
        while (!fetch_finished) {
            unique_lock<mutex> wait_lck(wait_mutex);
            auto predicate = [&has_events]() -> bool
            {
                return has_events;
            };
            while (!wait_condition.wait_for(wait_lck, chrono::seconds(1), predicate));
            if (has_events) {
                has_events = false;
                if (fetch.Wait()) {
                    fetch_finished = true;
                    EXPECT_TRUE(fetch.HasError() || false_negative) << "Blob request should be in failed state";
                }
            }
        }
    };

    EXPECT_FALSE(fetch.Wait()) << "Fetch should not finish on Init()";
    wait_function();

    sm_Env.Get().connection->SetQueryTimeoutRetry(old_retry_timeout);
    EXPECT_TRUE(timeout_called || false_negative) << "Timeout should happen";
    EXPECT_TRUE(fetch_finished) << "Fetch should finish";
    if (!false_negative) {
        EXPECT_EQ(2, event_callback->times_called)
            << "Error and Data callbacks should be called 2 times in total (1 for timeout and 1 for timeout after restart)";
        EXPECT_EQ(fetch.GetRestartCounterDebug(), 1UL);
    }
    event_callback.reset();
}

}  // namespace
