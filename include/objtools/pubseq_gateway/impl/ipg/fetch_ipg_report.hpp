#ifndef OBJTOOLS__PUBSEQ_GATEWAY__IPG__FETCH_IPG_REPORT_HPP_
#define OBJTOOLS__PUBSEQ_GATEWAY__IPG__FETCH_IPG_REPORT_HPP_
/*****************************************************************************
 *  $Id$
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
 *  IPG storage library
 *
 *****************************************************************************/

#include <corelib/ncbistd.hpp>

#include <optional>

#include <objtools/pubseq_gateway/impl/cassandra/cass_blob_op.hpp>
#include <objtools/pubseq_gateway/impl/ipg/ipg_types.hpp>
#include <objtools/pubseq_gateway/impl/ipg/ipg_report_entry.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(ipg)
USING_IDBLOB_SCOPE;

class CPubseqGatewayFetchIpgReportRequest
{
public:
    CPubseqGatewayFetchIpgReportRequest() = default;
    CPubseqGatewayFetchIpgReportRequest(CPubseqGatewayFetchIpgReportRequest const&) = default;
    CPubseqGatewayFetchIpgReportRequest(CPubseqGatewayFetchIpgReportRequest &&) = default;
    CPubseqGatewayFetchIpgReportRequest& operator=(CPubseqGatewayFetchIpgReportRequest const&) = default;
    CPubseqGatewayFetchIpgReportRequest& operator=(CPubseqGatewayFetchIpgReportRequest &&) = default;

    CPubseqGatewayFetchIpgReportRequest& SetProtein(string const& accession)
    {
        m_ProteinAccession = accession;
        return *this;
    }

    CPubseqGatewayFetchIpgReportRequest& SetNucleotide(string const& accession)
    {
        m_NucAccession = accession;
        return *this;
    }

    CPubseqGatewayFetchIpgReportRequest& SetIpg(TIpg value)
    {
        if (value > 0) {
            m_Ipg = value;
        }
        else {
            m_Ipg = nullopt;
        }
        return *this;
    }

    CPubseqGatewayFetchIpgReportRequest& SetResolvedIpg(TIpg value)
    {
        m_ResolvedIpg = value;
        return *this;
    }

    TIpg GetIpg() const
    {
        if (m_Ipg) {
            return m_Ipg.value();
        }
        return TIpg();
    }

    string GetProtein() const
    {
        if (m_ProteinAccession) {
            return m_ProteinAccession.value();
        }
        return {};
    }

    string GetNucleotide() const
    {
        if (m_NucAccession) {
            return m_NucAccession.value();
        }
        return {};
    }

    TIpg GetResolvedIpg() const
    {
        if (m_ResolvedIpg) {
            return m_ResolvedIpg.value();
        }
        return TIpg();
    }

    TIpg GetIpgToFetchData() const
    {
        if (m_Ipg) {
            return m_Ipg.value();
        }
        if (m_ResolvedIpg) {
            return m_ResolvedIpg.value();
        }
        return TIpg();
    }

    bool HasIpg() const
    {
        return m_Ipg.has_value();
    }

    bool HasProtein() const
    {
        return m_ProteinAccession.has_value();
    }

    bool HasNucleotide() const
    {
        return m_NucAccession.has_value();
    }

    bool HasResolvedIpg() const
    {
        return m_ResolvedIpg.has_value();
    }

    TIpg HasIpgToFetchData() const
    {
        return m_Ipg.has_value() || m_ResolvedIpg.has_value();
    }

private:
    optional<TIpg> m_Ipg;
    optional<string> m_ProteinAccession;
    optional<string> m_NucAccession;
    optional<TIpg> m_ResolvedIpg;
};

class CPubseqGatewayFetchIpgReport
    : public CCassBlobWaiter
{
    enum ETaskState {
        eInit = CCassBlobWaiter::eInit,
        eTaskAccessionResolutionStarted,
        eTaskCheckHugeIpgStarted,
        eTaskFetchHugeReport,
        eTaskFetchReport,
        eTaskFetchReportStarted,
        eTaskCleanup,
        eDone = CCassBlobWaiter::eDone,
        eError = CCassBlobWaiter::eError
    };

    const static size_t kReadBufferReserveDefault;

public:
    CPubseqGatewayFetchIpgReport(
        shared_ptr<CCassConnection> connection,
        const string & keyspace,
        CPubseqGatewayFetchIpgReportRequest const& request,
        CPubseqGatewayIpgReportConsumeCallback consume_callback,
        TDataErrorCallback data_error_cb,
        bool async = true
    );
    virtual ~CPubseqGatewayFetchIpgReport();

    CPubseqGatewayFetchIpgReport(const CPubseqGatewayFetchIpgReport&) = delete;
    CPubseqGatewayFetchIpgReport& operator=(const CPubseqGatewayFetchIpgReport&) = delete;

    CPubseqGatewayFetchIpgReport(CPubseqGatewayFetchIpgReport&&) = delete;
    CPubseqGatewayFetchIpgReport& operator=(CPubseqGatewayFetchIpgReport&&) = delete;

    void SetDataReadyCB(shared_ptr<CCassDataCallbackReceiver> callback);
    void SetConsumeCallback(CPubseqGatewayIpgReportConsumeCallback callback);

    void SetConsistency(CassConsistency value);
    void SetPageSize(unsigned int value);

    unsigned int GetPageSize() const
    {
        return m_PageSize;
    }

    CPubseqGatewayFetchIpgReportRequest GetRequest() const
    {
        return m_Request;
    }

    // For testing only
    SIpgSubgroupsConfig GetSubgroupsConfig() const
    {
        return m_Subgroups;
    }

    // For testing only
    void SetSubgrupsStatusOverride(EIpgSubgroupsStatus value)
    {
        m_SubgroupsStatusOverride = value;
    }

    // For testing only
    static void ConvertTimeTMsToCTimeLocal(int64_t time_ms, CTime& t);

private:
    void Wait1() override;

    CPubseqGatewayFetchIpgReportRequest m_Request;
    CPubseqGatewayIpgReportConsumeCallback m_ConsumeCallback;
    SIpgSubgroupsConfig m_Subgroups;
    EIpgSubgroupsStatus m_SubgroupsStatusOverride{EIpgSubgroupsStatus::eUndefined};
    SIpgSubgroupsConfig::TCIterator m_SubgroupItr{};

    vector<CIpgStorageReportEntry> m_Container;
    string m_LastAccession;
    string m_LastNucAccession;

    CassConsistency m_Consistency{CassConsistency::CASS_CONSISTENCY_LOCAL_QUORUM};
    unsigned int m_PageSize{CCassQuery::DEFAULT_PAGE_SIZE};
};

END_SCOPE(ipg)
END_NCBI_SCOPE

#endif  // OBJTOOLS__PUBSEQ_GATEWAY__IPG__FETCH_IPG_REPORT_HPP_
