#ifndef OBJTOOLS__PUBSEQ_GATEWAY__IPG__IPG_HUGE_REPORT_HELPER_HPP_
#define OBJTOOLS__PUBSEQ_GATEWAY__IPG__IPG_HUGE_REPORT_HELPER_HPP_
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
 *  IPG report huge groups helper
 *
 *****************************************************************************/

#include <corelib/ncbistd.hpp>

#include <atomic>

#include <objtools/pubseq_gateway/impl/cassandra/cass_driver.hpp>
#include <objtools/pubseq_gateway/impl/ipg/ipg_types.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(ipg)
USING_IDBLOB_SCOPE;

class CPubseqGatewayHugeIpgReportHelper
{
    CPubseqGatewayHugeIpgReportHelper() = default;
 public:
    static void PrepareHugeIpgConfigQuery(TIpg ipg, string const& keyspace, CCassQuery* query, CassConsistency consistency, bool async)
    {
        query->SetSQL("SELECT ipg, subgroups_status, subgroups_hash, subgroups FROM " + keyspace + ".ipg_report_config WHERE ipg = ?", 1);
        query->BindInt64(0, ipg);
        query->Query(consistency, async);
    }

    static void FetchHugeIpgConfigResult(SIpgSubgroupsConfig& config, CCassQuery* query, bool async)
    {
        query->NextRow();
        if (!query->IsEOF()) {
            config.ipg = query->FieldGetInt64Value(0);
            auto status_value = query->FieldGetInt32Value(1, 0);
            config.hash_type = static_cast<EIpgSubgroupHashType>(query->FieldGetInt32Value(2, 0));
            config.status = static_cast<EIpgSubgroupsStatus>(status_value);
            query->FieldGetContainerValue(3, back_inserter(config.subgroups));
        }
    }

    static void SetHugeIpgDisabled(bool value)
    {
        sm_HugeIpgDisabled = value;
    }

    static bool HugeIpgEnabled()
    {
        return !sm_HugeIpgDisabled;
    }
private:
    static atomic_bool sm_HugeIpgDisabled;
};

END_SCOPE(ipg)
END_NCBI_SCOPE

#endif  // OBJTOOLS__PUBSEQ_GATEWAY__IPG__IPG_HUGE_REPORT_HELPER_HPP_
