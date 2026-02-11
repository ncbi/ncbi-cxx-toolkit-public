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
*   Unit test suite to check FetchReport operation
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>

#include <atomic>
#include <condition_variable>
#include <mutex>

#include <gtest/gtest.h>

#include <objtools/pubseq_gateway/impl/cassandra/cass_driver.hpp>
#include <objtools/pubseq_gateway/impl/cassandra/cass_factory.hpp>
#include <objtools/pubseq_gateway/impl/ipg/fetch_ipg_report.hpp>
#include <objtools/pubseq_gateway/impl/ipg/ipg_huge_report_helper.hpp>

#include "../ccm_bridge.hpp"

namespace {

USING_NCBI_SCOPE;
USING_SCOPE(ipg);

class CPubseqGatewayFetchIpgReportTest
    : public testing::Test
{
 protected:
    static void SetUpTestCase() {
        auto r = CCCMBridge::GetConfigRegistry("config_fetch.ini");
        ASSERT_FALSE(r == nullptr);
        m_Factory = CCassConnectionFactory::s_Create();
        m_Factory->LoadConfig(*r, "CASSANDRA");
        m_Connection = m_Factory->CreateInstance();
        m_Connection->Connect();
    }

    static void TearDownTestCase() {
        m_Connection->Close();
        m_Connection = nullptr;
        m_Factory = nullptr;
    }

    static shared_ptr<CCassConnectionFactory> m_Factory;
    static shared_ptr<CCassConnection> m_Connection;

    string m_KeyspaceName{"test_fetch"};
};

shared_ptr<CCassConnectionFactory> CPubseqGatewayFetchIpgReportTest::m_Factory(nullptr);
shared_ptr<CCassConnection> CPubseqGatewayFetchIpgReportTest::m_Connection(nullptr);

static auto error_function = [](CRequestStatus::ECode status, int code, EDiagSev severity, const string & message) {
    EXPECT_TRUE(false) << "Error callback called during the test (status - "
        << status << ", code - " << code << ", message - '" << message << "')";
};

static auto wait_function = [](CPubseqGatewayFetchIpgReport& task)
{
    // Callback definition
    class CTaskOnDataCallback : public CCassDataCallbackReceiver
    {
     public:
       CTaskOnDataCallback(atomic_bool* called_flag, condition_variable* waiters)
           : m_CalledFlag(called_flag)
           , m_Waiters(waiters)
       {}
       void OnData() override
       {
           if (m_CalledFlag)
               *m_CalledFlag = true;
           if (m_Waiters)
               m_Waiters->notify_all();
       }
     private:
       atomic_bool* m_CalledFlag{nullptr};
       condition_variable* m_Waiters{nullptr};
    };

    atomic_bool on_data_called{false};
    condition_variable wait_condition;
    mutex wait_mutex;
    auto on_data = make_shared<CTaskOnDataCallback>(&on_data_called, &wait_condition);

    task.SetDataReadyCB(on_data);
    while (!task.Wait()) {
        unique_lock<mutex> _(wait_mutex);
        auto predicate = [&on_data_called]() -> bool {
            return on_data_called;
        };
        while (!wait_condition.wait_for(_, chrono::seconds(1), predicate)) {
            LOG_POST(Warning << "Waiting for reply");
        }
        on_data_called = false;
    }
};

TEST_F(CPubseqGatewayFetchIpgReportTest, FetchReportWithHugeStatusNew) {
    int call_count{0};
    auto consume = [&call_count](vector<CIpgStorageReportEntry> && page, bool is_last) -> bool
    {
        ++call_count;
        if (call_count == 1) {
            EXPECT_EQ(2UL, page.size());
            if (page.size() == 2) {
                EXPECT_EQ("EGB0688235.1", page[0].GetAccession());
                EXPECT_EQ("AAVJDK010000003.1", page[0].GetNucAccession());
                EXPECT_EQ("TonB system transport protein ExbD", page[0].GetProductName());
                EXPECT_EQ(page[0].GetCreated().AsString("Y-M-D h:m:s.l"), "2020-08-11 10:20:00.000");
                EXPECT_TRUE(page[0].GetCreated().IsLocalTime());
            }
        }
        else {
            EXPECT_TRUE(page.empty());
            EXPECT_TRUE(is_last);
            EXPECT_EQ(2, call_count);
        }
        return true;
    };
    CPubseqGatewayFetchIpgReportRequest request;
    request.SetIpg(216356);
    CPubseqGatewayFetchIpgReport task(
        m_Connection, m_KeyspaceName, request,
        consume, error_function, true
    );
    task.SetConsumeCallback(consume);
    task.SetConsistency(CassConsistency::CASS_CONSISTENCY_ALL);
    wait_function(task);
    EXPECT_EQ(2, call_count);
}

TEST_F(CPubseqGatewayFetchIpgReportTest, NormalWithFilter) {
    int count{0};
    auto consume = [&count](vector<CIpgStorageReportEntry> && page, bool is_last) -> bool
    {
        ++count;
        if (count == 1) {
            EXPECT_EQ(1UL, page.size());
            if (page.size() == 1) {
                EXPECT_EQ("EGB0709986.1", page[0].GetAccession());
                EXPECT_EQ("AAVJDV010000007.1", page[0].GetNucAccession());
                EXPECT_EQ(page[0].GetCreated().AsString("Y-M-D h:m:s.l"), "2020-08-11 10:21:00.000");
                EXPECT_TRUE(page[0].GetCreated().IsLocalTime());
            }
        }
        else {
            EXPECT_TRUE(page.empty());
            EXPECT_TRUE(is_last);
            EXPECT_EQ(2, count);
        }
        return true;
    };
    CPubseqGatewayFetchIpgReportRequest request;
    request.SetIpg(216356);
    request.SetProtein("EGB0709986.1");
    request.SetNucleotide("AAVJDV010000007.1");
    CPubseqGatewayFetchIpgReport task(
        m_Connection, m_KeyspaceName, request,
        consume, error_function, true
    );
    wait_function(task);
    EXPECT_EQ(2, count);
}

TEST_F(CPubseqGatewayFetchIpgReportTest, NormalWithFilterByAccession) {
    int count{0};
    auto consume = [&count](vector<CIpgStorageReportEntry> && page, bool is_last) -> bool
    {
        ++count;
        if (count == 1) {
            EXPECT_EQ(1UL, page.size());
            if (page.size() == 1) {
                EXPECT_EQ("EGB0709986.1", page[0].GetAccession());
                EXPECT_EQ("AAVJDV010000007.1", page[0].GetNucAccession());
            }
        }
        else {
            EXPECT_TRUE(page.empty());
            EXPECT_TRUE(is_last);
            EXPECT_EQ(2, count);
        }
        return true;
    };
    CPubseqGatewayFetchIpgReportRequest request;
    request.SetIpg(0);
    request.SetProtein("EGB0709986.1");
    request.SetNucleotide("AAVJDV010000007.1");
    EXPECT_FALSE(request.HasIpg());
    EXPECT_FALSE(request.HasResolvedIpg());
    EXPECT_FALSE(request.HasIpgToFetchData());
    CPubseqGatewayFetchIpgReport task(
        m_Connection, m_KeyspaceName, request,
        consume, error_function, true
    );
    wait_function(task);
    EXPECT_EQ(2, count);
    EXPECT_FALSE(task.GetRequest().HasIpg());
    EXPECT_TRUE(task.GetRequest().HasResolvedIpg());
    EXPECT_TRUE(task.GetRequest().HasIpgToFetchData());
    EXPECT_EQ(0, task.GetRequest().GetIpg());
    EXPECT_EQ(216356, task.GetRequest().GetResolvedIpg());
    EXPECT_EQ(216356, task.GetRequest().GetIpgToFetchData());
}

TEST_F(CPubseqGatewayFetchIpgReportTest, HugeEmpty) {
    int count{0};
    auto consume = [&count](vector<CIpgStorageReportEntry> && page, bool is_last) -> bool
    {
        ++count;
        EXPECT_TRUE(page.empty());
        EXPECT_TRUE(is_last);
        return true;
    };
    CPubseqGatewayFetchIpgReportRequest request;
    request.SetIpg(1111);
    CPubseqGatewayFetchIpgReport task(
        m_Connection, m_KeyspaceName, request,
        consume, error_function, true
    );
    wait_function(task);
    EXPECT_EQ(1, count);
}

TEST_F(CPubseqGatewayFetchIpgReportTest, Huge) {
    int count{0};
    map<string, CIpgStorageReportEntry> report;
    auto consume = [&count, &report](vector<CIpgStorageReportEntry> && page, bool is_last) -> bool
    {
        ++count;
        if (is_last) {
            EXPECT_TRUE(page.empty());
        }
        else {
            EXPECT_EQ(1UL, page.size());
            for (auto && item : page) {
                report[item.GetAccession()] = std::move(item);
            }
        }
        return true;
    };
    CPubseqGatewayFetchIpgReportRequest request;
    request.SetIpg(642300);
    CPubseqGatewayFetchIpgReport task(
        m_Connection, m_KeyspaceName, request,
        consume, error_function, true
    );
    task.SetPageSize(1);
    wait_function(task);
    EXPECT_EQ(4, count);
    ASSERT_EQ(3UL, report.size());
    ASSERT_FALSE(report.find("EGB0689183.1") == report.end());
    ASSERT_FALSE(report.find("EGB0689184.1") == report.end());
    ASSERT_FALSE(report.find("EGB0689185.1") == report.end());
    EXPECT_EQ("PRJNA248792", report["EGB0689183.1"].GetBioProject());
    EXPECT_EQ("970289", report["EGB0689184.1"].GetStrain());
    EXPECT_EQ("glutathione ABC transporter permease GsiD", report["EGB0689185.1"].GetProductName());
}

TEST_F(CPubseqGatewayFetchIpgReportTest, HugeWithFilter) {
    int count{0};
    map<string, CIpgStorageReportEntry> report;
    auto consume = [&count, &report](vector<CIpgStorageReportEntry> && page, bool is_last) -> bool
    {
        ++count;
        if (is_last) {
            EXPECT_TRUE(page.empty());
        }
        else {
            EXPECT_EQ(1UL, page.size());
            for (auto && item : page) {
                report[item.GetAccession()] = std::move(item);
            }
        }
        return true;
    };
    CPubseqGatewayFetchIpgReportRequest request;
    request.SetIpg(642300);
    request.SetProtein("EGB0689183.1");
    request.SetNucleotide("");
    CPubseqGatewayFetchIpgReport task(
        m_Connection, m_KeyspaceName, request,
        consume, error_function, true
    );
    wait_function(task);
    EXPECT_EQ(2, count);
    ASSERT_EQ(1UL, report.size());
    ASSERT_FALSE(report.find("EGB0689183.1") == report.end());
    EXPECT_EQ("PRJNA248792", report["EGB0689183.1"].GetBioProject());
    EXPECT_EQ("970289", report["EGB0689183.1"].GetStrain());
    EXPECT_EQ("glutathione ABC transporter permease GsiD", report["EGB0689183.1"].GetProductName());
}

TEST_F(CPubseqGatewayFetchIpgReportTest, HugeDisabled) {
    CPubseqGatewayHugeIpgReportHelper::SetHugeIpgDisabled(true);
    struct SEnableHugeIpg {
        ~SEnableHugeIpg() {
            CPubseqGatewayHugeIpgReportHelper::SetHugeIpgDisabled(false);
        }
    };
    SEnableHugeIpg enable_action;
    {
        int count{0};
        map<string, CIpgStorageReportEntry> report;
        auto consume = [&count, &report](vector<CIpgStorageReportEntry> && page, bool is_last) -> bool
        {
            ++count;
            if (is_last) {
                EXPECT_TRUE(page.empty());
            }
            else {
                EXPECT_EQ(1UL, page.size());
                for (auto && item : page) {
                    report[item.GetAccession()] = std::move(item);
                }
            }
            return true;
        };
        CPubseqGatewayFetchIpgReportRequest request;
        request.SetIpg(2222);
        CPubseqGatewayFetchIpgReport task(
            m_Connection, m_KeyspaceName, request,
            consume, error_function, true
        );
        wait_function(task);
        EXPECT_EQ(2, count);
        ASSERT_EQ(1UL, report.size());
        ASSERT_FALSE(report.find("EGB0689184.1") == report.end());
    }
}

TEST_F(CPubseqGatewayFetchIpgReportTest, TimeTMsToCTimeConversion) {
    auto check_fn = [](int64_t time_ms, string const& time_formatted) {
        CTime converted;
        CTime created(time_ms / 1000);
        created.SetMilliSecond(time_ms % 1000);
        created.ToLocalTime();
        CPubseqGatewayFetchIpgReport::ConvertTimeTMsToCTimeLocal(time_ms, converted);
        EXPECT_EQ(created, converted);
        EXPECT_EQ(converted.AsString("Y-M-D h:m:s.l"), time_formatted);
    };
    check_fn(1738299600234, "2025-01-31 00:00:00.234");
    check_fn(1665821123000, "2022-10-15 04:05:23.000");
    check_fn(0, "1969-12-31 19:00:00.000");
    bool should_throw{false};
    try {
        check_fn(1665821123000000, "2022-10-15 04:05:23.000");
    }
    catch(CException const& e) {
        should_throw = true;
    }
    EXPECT_TRUE(should_throw);
}

TEST_F(CPubseqGatewayFetchIpgReportTest, TimeTMsToCTimeConversionTiming) {
    CStopWatch sw;
    sw.Start();
    unsigned long iterations = 1'000'000;

    for (unsigned long i = 0; i < iterations; i++) {
        CTime created(1738299600);
        created.ToLocalTime();
        EXPECT_TRUE(created.IsLocalTime());
    }
    auto created_elapsed = sw.Elapsed();
    sw.Reset();
    sw.Start();
    for (unsigned long i = 0; i < iterations; i++) {
        CTime converted;
        CPubseqGatewayFetchIpgReport::ConvertTimeTMsToCTimeLocal(1738299600, converted);
        EXPECT_TRUE(converted.IsLocalTime());
    }
    auto converted_elapsed = sw.Elapsed();
    // If CTime is fixed, this will start failing.
    // CPubseqGatewayFetchIpgReport::ConvertTimeTMsToCTimeLocal needs to be reverted in that case
    EXPECT_GT(created_elapsed, 1.5 * converted_elapsed);
    cout << "Created time: " << NStr::DoubleToString(created_elapsed)
         << " Converted time: " << NStr::DoubleToString(converted_elapsed) << "\n";
}

}  // namespace
