#ifndef PUBSEQ_GATEWAY_CONVERT_UTILS__HPP
#define PUBSEQ_GATEWAY_CONVERT_UTILS__HPP

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
 * Authors: Sergey Satskiy
 *
 * File Description: Various format conversion utilities
 *
 *
 */

#include <objtools/pubseq_gateway/impl/cassandra/blob_record.hpp>
#include <objtools/pubseq_gateway/impl/cassandra/bioseq_info/record.hpp>
#include <objtools/pubseq_gateway/impl/cassandra/nannot/record.hpp>
#include <objtools/pubseq_gateway/impl/cassandra/si2csi/record.hpp>
#include <objtools/pubseq_gateway/impl/cassandra/request.hpp>
#include <objtools/pubseq_gateway/impl/cassandra/nannot_task/fetch.hpp>
#include <objtools/pubseq_gateway/impl/cassandra/blob_task/load_blob.hpp>
#include <objtools/pubseq_gateway/impl/cassandra/blob_task/fetch_split_history.hpp>
#include <objtools/pubseq_gateway/impl/cassandra/status_history/get_public_comment.hpp>
#include <objtools/pubseq_gateway/impl/cassandra/acc_ver_hist/record.hpp>
#include <objtools/pubseq_gateway/impl/cassandra/acc_ver_hist/tasks.hpp>
#include <objtools/pubseq_gateway/impl/ipg/fetch_ipg_report.hpp>

#include "pubseq_gateway_types.hpp"
#include "pubseq_gateway_utils.hpp"
#include "http_connection.hpp"


USING_NCBI_SCOPE;
USING_IDBLOB_SCOPE;
using namespace ipg;

#include <string>
using namespace std;


string ToBioseqProtobuf(const CBioseqInfoRecord &  bioseq_info);
string ToJsonString(const CBioseqInfoRecord &  bioseq_info,
                    SPSGS_ResolveRequest::TPSGS_BioseqIncludeData  include_data_flags,
                    const string &  custom_blob_id = "");
string ToJsonString(const CBlobRecord &  blob);
string ToJsonString(const CNAnnotRecord &  annot_record, int32_t  sat,
                    const string &  custom_blob_id = "");
string ToJsonString(const map<string, int> &  per_na_results);
string ToJsonString(const SAccVerHistRec &  history_record);

string ToJsonString(const CBioseqInfoFetchRequest &  request);
string ToJsonString(const CSi2CsiFetchRequest &  request);
string ToJsonString(const CBlobFetchRequest &  request);

string ToJsonString(const CSI2CSIRecord &  record);

string ToJsonString(const CCassBlobTaskLoadBlob &  request);
string ToJsonString(const CCassBlobTaskFetchSplitHistory &  request);
string ToJsonString(const CCassNAnnotTaskFetch &  request);
string ToJsonString(const CCassStatusHistoryTaskGetPublicComment &  request);
string ToJsonString(const CCassAccVerHistoryTaskFetch &  request);
string ToJsonString(const CPubseqGatewayFetchIpgReportRequest &  request);
string ToJsonString(const CIpgStorageReportEntry &  ipg_entry);

string ToJsonString(const SConnectionRunTimeProperties &  conn_props);

#endif
