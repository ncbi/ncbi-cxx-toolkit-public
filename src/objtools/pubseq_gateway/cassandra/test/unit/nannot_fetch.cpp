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
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include <corelib/ncbireg.hpp>

#include <serial/serial.hpp>
#include <serial/iterator.hpp>
#include <serial/objistr.hpp>
#include <serial/objostr.hpp>

#include <objects/seqsplit/ID2S_Seq_annot_Info.hpp>

#include <objtools/pubseq_gateway/impl/cassandra/nannot_task/fetch.hpp>
#include <objtools/pubseq_gateway/impl/cassandra/cass_driver.hpp>
#include <objtools/pubseq_gateway/impl/cassandra/cass_factory.hpp>

namespace {

USING_NCBI_SCOPE;
USING_IDBLOB_SCOPE;
USING_SCOPE(objects);

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

class CCassNAnnotTaskFetchWithTimeout : public CCassNAnnotTaskFetch
{
 public:
    CCassNAnnotTaskFetchWithTimeout(
        unsigned int timeout_ms,
        unsigned int max_retries,
        shared_ptr<CCassConnection> connection,
        const string & keyspace,
        string accession,
        int16_t version,
        int16_t seq_id_type,
        TNAnnotConsumeCallback consume_callback,
        TDataErrorCallback data_error_cb
    )
        : CCassNAnnotTaskFetch(timeout_ms, max_retries, connection,
            keyspace, accession, version, seq_id_type,
            move(consume_callback), move(data_error_cb)
        )
        , m_CallCount(nullptr)
        , m_RestartTrigger(0)
        , m_TestRestartDone(false)
    {
        m_PageSize = 1;
    }

    CCassNAnnotTaskFetchWithTimeout(
        unsigned int timeout_ms,
        unsigned int max_retries,
        shared_ptr<CCassConnection> connection,
        const string & keyspace,
        string accession,
        int16_t version,
        int16_t seq_id_type,
        const vector<string> & annot_names,
        TNAnnotConsumeCallback consume_callback,
        TDataErrorCallback data_error_cb
    )
        : CCassNAnnotTaskFetch(timeout_ms, max_retries, connection,
            keyspace, accession, version, seq_id_type, annot_names,
            move(consume_callback), move(data_error_cb)
        )
        , m_CallCount(nullptr)
        , m_RestartTrigger(0)
        , m_TestRestartDone(false)
    {
        m_PageSize = 1;
    }

    virtual void Wait1(void) override
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
shared_ptr<CCassConnectionFactory> CNAnnotTaskFetchTest::m_Factory(nullptr);
shared_ptr<CCassConnection> CNAnnotTaskFetchTest::m_Connection(nullptr);

TEST_F(CNAnnotTaskFetchTest, SingleRetrievalVector) {
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
        m_Timeout, 0, m_Connection, m_KeyspaceName,
        "NW_017889270", 1, 10,
        [&call_count](CNAnnotRecord && entry, bool last) -> bool {
            switch(++call_count) {
                case 1:
                    EXPECT_EQ(entry.GetAnnotName(), "NA000122202.1");
                    EXPECT_EQ(entry.GetStart(), 9853);
                    EXPECT_EQ(entry.GetStop(), 9858);
                    EXPECT_EQ(entry.GetSatKey(), 888);
                    EXPECT_FALSE(last);
                    return true;
                case 2:
                    EXPECT_EQ(entry.GetAnnotName(), "NA000122203.1");
                    EXPECT_EQ(entry.GetStart(), 2506);
                    EXPECT_EQ(entry.GetStop(), 8119);
                    EXPECT_EQ(entry.GetSatKey(), 19347);
                    EXPECT_FALSE(last);
                    return true;
                case 3:
                    EXPECT_EQ(entry.GetAnnotName(), "NA000122204.1");
                    EXPECT_EQ(entry.GetStart(), 640);
                    EXPECT_EQ(entry.GetStop(), 5865);
                    EXPECT_EQ(entry.GetSatKey(), 39419);
                    EXPECT_FALSE(last);
                    return true;
                case 4:
                    EXPECT_EQ(entry.GetAnnotName(), "NA000122205.1");
                    EXPECT_EQ(entry.GetStart(), 992);
                    EXPECT_EQ(entry.GetStop(), 1445);
                    EXPECT_EQ(entry.GetSatKey(), 58472);
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
        m_Timeout, 0, m_Connection, m_KeyspaceName,
        "NW_017889270", 1, 10,
        [&call_count](CNAnnotRecord && entry, bool last) -> bool {
            switch(++call_count) {
                case 1:
                    EXPECT_EQ(entry.GetAnnotName(), "NA000122202.1");
                    EXPECT_EQ(entry.GetStart(), 9853);
                    EXPECT_EQ(entry.GetStop(), 9858);
                    EXPECT_EQ(entry.GetSatKey(), 888);
                    EXPECT_FALSE(last);
                    return true;
                case 2:
                    EXPECT_EQ(entry.GetAnnotName(), "NA000122203.1");
                    EXPECT_EQ(entry.GetStart(), 2506);
                    EXPECT_EQ(entry.GetStop(), 8119);
                    EXPECT_EQ(entry.GetSatKey(), 19347);
                    EXPECT_FALSE(last);
                    return true;
                case 3:
                    EXPECT_EQ(entry.GetAnnotName(), "NA000122204.1");
                    EXPECT_EQ(entry.GetStart(), 640);
                    EXPECT_EQ(entry.GetStop(), 5865);
                    EXPECT_EQ(entry.GetSatKey(), 39419);
                    EXPECT_FALSE(last);
                    return true;
                case 4:
                    EXPECT_EQ(entry.GetAnnotName(), "NA000122205.1");
                    EXPECT_EQ(entry.GetStart(), 992);
                    EXPECT_EQ(entry.GetStop(), 1445);
                    EXPECT_EQ(entry.GetSatKey(), 58472);
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
        m_Timeout, 0, m_Connection, m_KeyspaceName,
        "NW_017889270", 1, 10, vector<string>({"NA000122203.1", "NA000122202.1", "NA000122204.1"}),
        [&call_count](CNAnnotRecord && entry, bool last) -> bool {
            switch(++call_count) {
                case 1:
                    EXPECT_EQ(entry.GetAnnotName(), "NA000122202.1");
                    EXPECT_EQ(entry.GetStart(), 9853);
                    EXPECT_EQ(entry.GetStop(), 9858);
                    EXPECT_EQ(entry.GetSatKey(), 888);
                    EXPECT_FALSE(last);
                    return true;
                case 2:
                    EXPECT_EQ(entry.GetAnnotName(), "NA000122203.1");
                    EXPECT_EQ(entry.GetStart(), 2506);
                    EXPECT_EQ(entry.GetStop(), 8119);
                    EXPECT_EQ(entry.GetSatKey(), 19347);
                    EXPECT_FALSE(last);
                    return true;
                case 3:
                    EXPECT_EQ(entry.GetAnnotName(), "NA000122204.1");
                    EXPECT_EQ(entry.GetStart(), 640);
                    EXPECT_EQ(entry.GetStop(), 5865);
                    EXPECT_EQ(entry.GetSatKey(), 39419);
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
        m_Timeout, 0, m_Connection, m_KeyspaceName,
        "NW_017889270", 1, 10, vector<string>({"NA000122203.1", "NA000122202.1", "NA000122204.1"}),
        [&call_count](CNAnnotRecord && entry, bool last) -> bool {
            switch(++call_count) {
                case 1:
                    EXPECT_EQ(entry.GetAnnotName(), "NA000122202.1");
                    EXPECT_EQ(entry.GetStart(), 9853);
                    EXPECT_EQ(entry.GetStop(), 9858);
                    EXPECT_EQ(entry.GetSatKey(), 888);
                    EXPECT_FALSE(last);
                    return true;
                case 2:
                    EXPECT_EQ(entry.GetAnnotName(), "NA000122203.1");
                    EXPECT_EQ(entry.GetStart(), 2506);
                    EXPECT_EQ(entry.GetStop(), 8119);
                    EXPECT_EQ(entry.GetSatKey(), 19347);
                    EXPECT_FALSE(last);
                    return true;
                case 3:
                    EXPECT_EQ(entry.GetAnnotName(), "NA000122204.1");
                    EXPECT_EQ(entry.GetStart(), 640);
                    EXPECT_EQ(entry.GetStop(), 5865);
                    EXPECT_EQ(entry.GetSatKey(), 39419);
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
        m_Timeout, 0, m_Connection, m_KeyspaceName,
        "NW_017889270", 1, 10, vector<string>({"NA000122203.1", "NA000122202.1", "NA000122204.1"}),
        [&call_count](CNAnnotRecord && entry, bool last) -> bool {
            switch(++call_count) {
                case 1:
                    EXPECT_EQ(entry.GetAnnotName(), "NA000122202.1");
                    EXPECT_EQ(entry.GetStart(), 9853);
                    EXPECT_EQ(entry.GetStop(), 9858);
                    EXPECT_EQ(entry.GetSatKey(), 888);
                    EXPECT_FALSE(last);
                    return true;
                case 2:
                    EXPECT_EQ(entry.GetAnnotName(), "NA000122203.1");
                    EXPECT_EQ(entry.GetStart(), 2506);
                    EXPECT_EQ(entry.GetStop(), 8119);
                    EXPECT_EQ(entry.GetSatKey(), 19347);
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
        m_Timeout, 0, m_Connection, m_KeyspaceName,
        "NW_017889735", 1, 10, vector<string>({"NA000122202.1"}),
        [&call_count](CNAnnotRecord && entry, bool last) -> bool {
            switch(++call_count) {
                case 1:
                    EXPECT_EQ(entry.GetAnnotName(), "NA000122202.1");
                    EXPECT_EQ(entry.GetStart(), 6616);
                    EXPECT_EQ(entry.GetStop(), 41589);
                    EXPECT_EQ(entry.GetSatKey(), 500);
                    EXPECT_EQ(entry.GetAnnotInfoModified(), 1567710575270);
                    EXPECT_FALSE(entry.GetSeqAnnotInfo().empty());
                    EXPECT_EQ(gGetSeqAnnotInfoString(&entry),
                        "ID2S-Seq-annot-Info ::= {name \"NA000122202.1\",feat {{type 13}},seq-loc gi-interval {gi 1142972004,start 6616,length 34974}}\n"
                        "ID2S-Seq-annot-Info ::= {name \"NA000122202.1@@10\",graph NULL,seq-loc gi-interval {gi 1142972004,start 6610,length 34980}}\n"
                        "ID2S-Seq-annot-Info ::= {name \"NA000122202.1@@100\",graph NULL,seq-loc gi-interval {gi 1142972004,start 6610,length 35000}}\n");
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

TEST_F(CNAnnotTaskFetchTest, RetrievalWithSeqAnnotInfo2) {
    size_t call_count = 0;
    CCassNAnnotTaskFetch fetch(
        m_Timeout, 0, m_Connection, m_KeyspaceName,
        "NW_020201082", 1, 10, vector<string>({"NA000156740.1"}),
        [&call_count](CNAnnotRecord && entry, bool last) -> bool {
            switch(++call_count) {
                case 1:
                    EXPECT_EQ(entry.GetAnnotName(), "NA000156740.1");
                    EXPECT_EQ(entry.GetStart(), 14153);
                    EXPECT_EQ(entry.GetStop(), 77991);
                    EXPECT_EQ(entry.GetSatKey(), 26395559);
                    EXPECT_EQ(entry.GetAnnotInfoModified(), 1567919439853);
                    EXPECT_FALSE(entry.GetSeqAnnotInfo().empty());
                    EXPECT_EQ(gGetSeqAnnotInfoString(&entry),
                        "ID2S-Seq-annot-Info ::= {name \"NA000156740.1\",feat {{type 8,subtypes {56}}},seq-loc gi-interval {gi 1385009444,start 14153,length 63839}}\n"
                        "ID2S-Seq-annot-Info ::= {name \"NA000156740.1@@10\",graph NULL,seq-loc gi-interval {gi 1385009444,start 14150,length 63850}}\n"
                        "ID2S-Seq-annot-Info ::= {name \"NA000156740.1@@100\",graph NULL,seq-loc gi-interval {gi 1385009444,start 14150,length 63900}}\n"
                    );
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

}  // namespace
