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
 *  Retrieval part of IPG storage library for integration with PubseqGateway
 *
 *****************************************************************************/

#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>
#include <corelib/ncbitime.hpp>

#include <objtools/pubseq_gateway/impl/ipg/fetch_ipg_report.hpp>
#include <objtools/pubseq_gateway/impl/ipg/ipg_huge_report_helper.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(ipg)

BEGIN_SCOPE()

inline static constexpr array<pair<const char*, const char *>, 22> ipg_report_fields{{
    {"ipg", "ipg"},
    {"accession", "accession"},
    {"assembly", "assembly"},
    {"product_name", "product_name"},
    {"nuc_accession", "nuc_accession"},
    {"length", "length"},
    {"div", "div"},
    {"src_db", "src_db"},
    {"src_refseq", "src_refseq"},
    {"taxid", "taxid"},
    {"cds", "cds"},
    {"strain", "strain"},
    {"gb_state", "gb_state"},
    {"weights", "weights"},
    {"bioproject", "bioproject"},
    {"compartment", "compartment"},
    {"updated", "updated"},
    {"created", "created"},
    {"flags", "flags"},
    {"pubmedids", "pubmedids"},
    {"def_line", "def_line"},
    {"write_time", "writetime(gb_state)"},
}};

constexpr int FieldIndex(const char * field_name)
{
    int i{0};
    for (const auto& field : ipg_report_fields) {
        if (field.first == field_name) {
            return i;
        }
        ++i;
    }
    return -1;
}

string GetSelectFieldList()
{
    string field_list;
    for (auto const& item : ipg_report_fields) {
        field_list.append(item.second).append(", ");
    }
    if (!field_list.empty()) {
        field_list.resize(field_list.size() - 2);
    }
    return field_list;
}

void FillCTimeLocalByTimeTMs(int64_t time_ms, CTime& t)
{
    using namespace std::chrono;
    system_clock::time_point tp{milliseconds{time_ms}};
    std::time_t tt = system_clock::to_time_t(tp);
    std::tm tm{};
    localtime_r(&tt, &tm);
    t.SetTimeTM(tm);
    t.SetMilliSecond(time_ms % 1000);
}

void PopulateEntry(shared_ptr<CCassQuery>& query, CIpgStorageReportEntry& entry)
{
    entry.SetIpg(query->FieldGetInt64Value(FieldIndex("ipg")));
    entry
        .SetAccession(query->FieldGetStrValue(FieldIndex("accession")))
        .SetNucAccession(query->FieldGetStrValue(FieldIndex("nuc_accession")))
        .SetGbState( query->FieldIsNull(FieldIndex("gb_state")) ?
            NCBI_gb_state_eWGSGenBankMissing :
            query->FieldGetInt32Value(FieldIndex("gb_state"))
        );

    entry.SetAssembly(query->FieldGetStrValueDef(FieldIndex("assembly"), ""));
    entry.SetProductName(query->FieldGetStrValueDef(FieldIndex("product_name"), ""));
    entry.SetDiv(query->FieldGetStrValueDef(FieldIndex("div"), ""));
    entry.SetStrain(query->FieldGetStrValueDef(FieldIndex("strain"), ""));
    entry.SetBioProject(query->FieldGetStrValueDef(FieldIndex("bioproject"), ""));
    entry.SetGenome(
        query->FieldIsNull(FieldIndex("compartment")) ?
        objects::CBioSource::eGenome_unknown :
        static_cast<objects::CBioSource::EGenome>(query->FieldGetInt32Value(FieldIndex("compartment")))
    );

    entry.SetLength(query->FieldGetInt32Value(FieldIndex("length"), 0));
    entry.SetSrcDb(query->FieldGetInt32Value(FieldIndex("src_db"), 0));
    entry.SetTaxid(TAX_ID_FROM(int, query->FieldGetInt32Value(FieldIndex("taxid"), 0)));
    entry.SetFlags(query->FieldGetInt32Value(FieldIndex("flags"), 0));
    int64_t writetime_mcs = query->FieldGetInt64Value(FieldIndex("write_time"));
    CTime writetime;
    FillCTimeLocalByTimeTMs(writetime_mcs/1000, writetime);
    entry.SetWriteTime(writetime);

    if (!query->FieldIsNull(FieldIndex("updated"))) {
        CTime updated;
        FillCTimeLocalByTimeTMs(query->FieldGetInt64Value(FieldIndex("updated")), updated);
        entry.SetUpdated(updated);
    } else {
        entry.SetUpdated(CTime{});
    }

    if (!query->FieldIsNull(FieldIndex("created"))) {
        CTime created;
        FillCTimeLocalByTimeTMs(query->FieldGetInt64Value(FieldIndex("created")), created);
        entry.SetCreated(created);
    } else {
        entry.SetCreated(CTime{});
    }

    if (!query->FieldIsNull(FieldIndex("src_refseq"))) {
        list<string> src_refseq;
        query->FieldGetContainerValue(FieldIndex("src_refseq"), back_inserter(src_refseq));
        entry.SetRefseq(std::move(src_refseq));
    }

    if (!query->FieldIsNull(FieldIndex("weights"))) {
        TIpgWeights weights;
        query->FieldGetContainerValue(FieldIndex("weights"), back_inserter(weights));
        entry.SetWeights(std::move(weights));
    }
    if (!query->FieldIsNull(FieldIndex("pubmedids"))) {
        TPubMedIds ids;
        query->FieldGetContainerValue(FieldIndex("pubmedids"), inserter(ids, ids.end()));
        entry.SetPubMedIds(std::move(ids));
    }

    if (!query->FieldIsNull(FieldIndex("cds"))) {
        entry.SetCds(SIpgCds(
            query->FieldGetTupleValue<tuple<TCdsValue, TCdsValue, TCdsValue>>(FieldIndex("cds"))
        ));
    }

    entry.SetDefLine(query->FieldGetStrValueDef(FieldIndex("def_line"), ""));
}

END_SCOPE()

CPubseqGatewayFetchIpgReport::CPubseqGatewayFetchIpgReport(
    shared_ptr<CCassConnection> connection,
    const string & keyspace,
    CPubseqGatewayFetchIpgReportRequest const& request,
    CPubseqGatewayIpgReportConsumeCallback consume_callback,
    TDataErrorCallback data_error_cb,
    bool async
)
    : CCassBlobWaiter(std::move(connection), keyspace, async, std::move(data_error_cb))
    , m_Request(request)
    , m_ConsumeCallback(std::move(consume_callback))
{}

const size_t CPubseqGatewayFetchIpgReport::kReadBufferReserveDefault = 512;

CPubseqGatewayFetchIpgReport::~CPubseqGatewayFetchIpgReport() = default;

void CPubseqGatewayFetchIpgReport::SetConsumeCallback(CPubseqGatewayIpgReportConsumeCallback  callback)
{
    m_ConsumeCallback = std::move(callback);
}

void CPubseqGatewayFetchIpgReport::SetConsistency(CassConsistency value)
{
    m_Consistency = value;
}

void CPubseqGatewayFetchIpgReport::SetPageSize(unsigned int value)
{
    m_PageSize = value;
}

void CPubseqGatewayFetchIpgReport::ConvertTimeTMsToCTimeLocal(int64_t time_ms, CTime& t)
{
    FillCTimeLocalByTimeTMs(time_ms, t);
}

void CPubseqGatewayFetchIpgReport::SetDataReadyCB(shared_ptr<CCassDataCallbackReceiver> callback)
{
    if (callback && m_State != eInit) {
        NCBI_THROW(CCassandraException, eSeqFailed,
           "CPubseqGatewayFetchIpgReport: DataReadyCB can't be assigned "
           "after the loading process has started");
    }
    CCassBlobWaiter::SetDataReadyCB3(callback);
}

void CPubseqGatewayFetchIpgReport::Wait1()
{
    using THugeHelper = CPubseqGatewayHugeIpgReportHelper;
    bool restarted;
    do {
        restarted = false;
        switch(m_State) {
            case eError:
            case eDone:
                return;
            case eInit:
                m_Container.reserve(kReadBufferReserveDefault);
                m_QueryArr.resize(1);
                m_QueryArr[0] = {ProduceQuery(), 0};
                // If accession is provided instead of IPG, resolve IPG, then return
                // to the 'new task' state.
                if (!m_Request.HasIpgToFetchData() && m_Request.HasProtein()) {
                    m_QueryArr[0].query->SetSQL("SELECT ipg FROM " + GetKeySpace() + ".accession_to_ipg WHERE accession = ?", 1);
                    m_QueryArr[0].query->BindStr(0, m_Request.GetProtein());
                    SetupQueryCB3(m_QueryArr[0].query);
                    m_QueryArr[0].query->Query(m_Consistency, m_Async);
                    m_State = eTaskAccessionResolutionStarted;
                } else if (THugeHelper::HugeIpgEnabled()) {
                    SetupQueryCB3(m_QueryArr[0].query);
                    THugeHelper::PrepareHugeIpgConfigQuery(m_Request.GetIpgToFetchData(), GetKeySpace(), m_QueryArr[0].query.get(), m_Consistency, m_Async);
                    m_State = eTaskCheckHugeIpgStarted;
                }
                else {
                    restarted = true;
                    m_State = eTaskFetchReport;
                }
            break;

            case eTaskAccessionResolutionStarted:
                if (CheckReady(m_QueryArr[0])) {
                    m_QueryArr[0].query->NextRow();
                    if (!m_QueryArr[0].query->IsEOF()) {
                        m_Request.SetResolvedIpg(m_QueryArr[0].query->FieldGetInt64Value(0));
                    }
                    if (m_Request.HasIpgToFetchData()) {
                        restarted = true;
                        m_State = eInit;
                    }
                    else {
                        restarted = true;
                        m_State = eTaskCleanup;
                    }
                }
            break;
            case eTaskCheckHugeIpgStarted:
                if (CheckReady(m_QueryArr[0])) {
                    THugeHelper::FetchHugeIpgConfigResult(m_Subgroups, m_QueryArr[0].query.get(), m_Async);
                    if (m_SubgroupsStatusOverride != EIpgSubgroupsStatus::eUndefined) {
                        m_Subgroups.status = m_SubgroupsStatusOverride;
                    }
                    if (m_Subgroups.IsReadable()) {
                        m_SubgroupItr = m_Subgroups.subgroups.cbegin();
                        m_State = eTaskFetchHugeReport;
                    }
                    else {
                        m_State = eTaskFetchReport;
                    }
                    restarted = true;
                }
            break;

            case eTaskFetchReport:
            {
                m_QueryArr[0] = {ProduceQuery(), 0};
                size_t args = 1;
                string protein_suffix;
                if (m_Request.HasProtein()) {
                    args = 2;
                    protein_suffix = " AND accession = ?";
                    if (m_Request.HasNucleotide()) {
                        args = 3;
                        protein_suffix += " AND nuc_accession = ?";
                    }
                }
                m_QueryArr[0].query->SetSQL("SELECT " + GetSelectFieldList()
                    + " FROM " + GetKeySpace() + ".ipg_report WHERE ipg = ?" + protein_suffix, args);
                m_QueryArr[0].query->BindInt64(0, m_Request.GetIpgToFetchData());
                if (m_Request.HasProtein()) {
                    m_QueryArr[0].query->BindStr(1, m_Request.GetProtein());
                    if (m_Request.HasNucleotide()) {
                        m_QueryArr[0].query->BindStr(2, m_Request.GetNucleotide());
                    }
                }
                SetupQueryCB3(m_QueryArr[0].query);
                m_QueryArr[0].query->Query(m_Consistency, m_Async, true, m_PageSize);
                m_LastAccession.clear();
                m_LastNucAccession.clear();
                m_State = eTaskFetchReportStarted;
            }
            break;

            case eTaskFetchHugeReport:
                if (m_SubgroupItr == m_Subgroups.subgroups.cend()) {
                    restarted = true;
                    m_State = eTaskCleanup;
                }
                else {
                    size_t args = 2;
                    string protein_suffix;
                    if (m_Request.HasProtein()) {
                        args = 3;
                        protein_suffix = " AND accession = ?";
                        if (m_Request.HasNucleotide()) {
                            args = 4;
                            protein_suffix += " AND nuc_accession = ?";
                        }
                    }
                    m_QueryArr[0] = {ProduceQuery(), 0};
                    m_QueryArr[0].query->SetSQL("SELECT " + GetSelectFieldList()
                        + " FROM " + GetKeySpace() + ".ipg_report_huge WHERE ipg = ? and subgroup = ?" + protein_suffix, args);
                    m_QueryArr[0].query->BindInt64(0, m_Request.GetIpgToFetchData());
                    m_QueryArr[0].query->BindInt32(1, *m_SubgroupItr);
                    if (m_Request.HasProtein()) {
                        m_QueryArr[0].query->BindStr(2, m_Request.GetProtein());
                        if (m_Request.HasNucleotide()) {
                            m_QueryArr[0].query->BindStr(3, m_Request.GetNucleotide());
                        }
                    }
                    SetupQueryCB3(m_QueryArr[0].query);
                    m_QueryArr[0].query->Query(m_Consistency, m_Async, true, m_PageSize);
                    m_LastAccession.clear();
                    m_LastNucAccession.clear();
                    m_State = eTaskFetchReportStarted;
                }
            break;

            case eTaskFetchReportStarted:
                if (CheckReady(m_QueryArr[0])) {
                    while (m_QueryArr[0].query->NextRow() == ar_dataready) {
                        m_Container.resize(m_Container.size() + 1);
                        auto& new_item = *rbegin(m_Container);
                        PopulateEntry(m_QueryArr[0].query, new_item);
                        if (!new_item.IsValid()) {
                            m_Container.resize(m_Container.size() - 1);
                        }
                    }
                    // If query has been restarted we need to filter out whatever we already returned to client
                    if (!m_LastAccession.empty() || !m_LastNucAccession.empty()) {
                        m_Container.erase(
                            remove_if(begin(m_Container), end(m_Container),
                                [this](CIpgStorageReportEntry const& e) -> bool
                                {
                                    return e.GetAccession() < m_LastAccession
                                        || (e.GetAccession() == m_LastAccession && e.GetNucAccession() <= m_LastNucAccession);
                                }),
                            end(m_Container)
                        );
                    }
                    bool do_next{true};
                    if (!m_Container.empty()) {
                        m_LastAccession = rbegin(m_Container)->GetAccession();
                        m_LastNucAccession = rbegin(m_Container)->GetNucAccession();
                        do_next = m_ConsumeCallback(std::move(m_Container), false);
                        m_Container.clear();
                    }
                    if (!do_next) {
                        restarted = true;
                        m_State = eTaskCleanup;
                    }
                    else if (m_QueryArr[0].query->IsEOF()) {
                        if (m_Subgroups.IsReadable()) {
                            ++m_SubgroupItr;
                            m_State = eTaskFetchHugeReport;
                        }
                        else {
                            m_State = eTaskCleanup;
                        }
                        restarted = true;
                    }
                }
            break;
            case eTaskCleanup:
                if (m_State != eError) {
                    m_ConsumeCallback({}, true);
                }
                CloseAll();
                m_State = eDone;
            break;

            default: { // LCOV_EXCL_LINE
                char msg[1024]; // LCOV_EXCL_LINE
                string keyspace = GetKeySpace(); // LCOV_EXCL_LINE
                snprintf(msg, sizeof(msg), // LCOV_EXCL_LINE
                    "Failed to fetch blob (key=%s.%d) unexpected state (%d)",
                    keyspace.c_str(), GetKey(), static_cast<int>(m_State));
                Error(CRequestStatus::e502_BadGateway, // LCOV_EXCL_LINE
                    CCassandraException::eQueryFailed,
                    eDiag_Error, msg);
            }
        }
    } while(restarted);
}

END_SCOPE(ipg)
END_NCBI_SCOPE
