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
 * File Description:
 *
 */

#include <ncbi_pch.hpp>

#include <objtools/pubseq_gateway/protobuf/psg_protobuf.pb.h>
#include "bioseq_info_record_selector.hpp"
#include "insdc_utils.hpp"
#include "pubseq_gateway.hpp"
#include "pubseq_gateway_convert_utils.hpp"

USING_NCBI_SCOPE;


SPSGS_BioseqSelectionResult
SelectBioseqInfoRecord(const vector<CBioseqInfoRecord>&  records,
                       bool  is_cache)
{
    if (records.empty())
        return {CRequestStatus::e404_NotFound, -1, ""};

    if (records.size() == 1)
        return {CRequestStatus::e200_Ok, 0, ""};

    SPSGS_BioseqSelectionResult ret = {CRequestStatus::e404_NotFound, -1, ""};
    static auto                 app = CPubseqGatewayApp::GetInstance();


    // CXX-14074, ID-8744 : Always pick the record with latest date_changed, regardless of its
    // version number or state. 
    // ID-8796 : date_changed cannot be used as a criterion because it depends on GiFlags.status_change.
    // Correct order of priorities:
    // 1a. Live (state = 10), not HUP sat
    // 1b. Dead (state != 10), not HUP sat
    // 1c. all HUP sat
    // 2. Largest gi;
    // 3. Largest version.
    vector<size_t> live_idx_v;
    vector<size_t> dead_idx_v;
    vector<size_t> hup_idx_v;
    for (size_t  k = 0; k < records.size(); ++k) {
        bool    is_secure_sat = app->IsSecureSat(records[k].GetSat());
        if (records[k].GetSeqState() == ncbi::psg::retrieval::SEQ_STATE_LIVE &&
            is_secure_sat == false) {
            live_idx_v.push_back(k);
        } else if (is_secure_sat == false) {
            dead_idx_v.push_back(k);
        } else {
            hup_idx_v.push_back(k);
        }
    }

    vector<size_t>* good_idx_v_ptr = NULL;
    if (!live_idx_v.empty()) good_idx_v_ptr = &live_idx_v;
    else if (!dead_idx_v.empty()) good_idx_v_ptr = &dead_idx_v;
    else good_idx_v_ptr = &hup_idx_v;

    // It is necessary to check that all the records from the good vector are
    // of compatible seq id type
    CBioseqInfoRecord::TSeqIdType   expected_seq_id_type = records[(*good_idx_v_ptr)[0]].GetSeqIdType();
    bool                            is_expected_insdc_type = IsINSDCSeqIdType(expected_seq_id_type);
    bool                            seq_id_types_compatible = true;

    TGi                             max_gi = ZERO_GI;
    CBioseqInfoRecord::TVersion     max_version = -1;
    size_t                          k = -1;
    CBioseqInfoRecord::TSeqIdType   current_seq_id_type;

    for (size_t i = 0; i < good_idx_v_ptr->size(); ++i) {
        k = (*good_idx_v_ptr)[i];
        current_seq_id_type = records[k].GetSeqIdType();

        if (is_expected_insdc_type) {
            if (!IsINSDCSeqIdType(current_seq_id_type)) {
                seq_id_types_compatible = false;
                break;
            }
        } else {
            if (current_seq_id_type != expected_seq_id_type) {
                seq_id_types_compatible = false;
                break;
            }
        }

        TGi gi = GI_FROM(idblob::CBioseqInfoRecord::TGI, records[k].GetGI());
        if (gi > max_gi) {
            ret.index = k;
            ret.status = CRequestStatus::e200_Ok;
            max_gi = gi;
            max_version = records[k].GetVersion();
        } else if (gi == max_gi) {
            auto version = records[k].GetVersion();
            if (version > max_version) {
                ret.index = k;
                ret.status = CRequestStatus::e200_Ok;
                max_version = version;
            }
        }
    }

    if (!seq_id_types_compatible) {
        // The seq id types are not compatible between multiple records
        ret.index = -1;
        ret.status = CRequestStatus::e300_MultipleChoices;

        bool    need_comma = false;

        ret.message = "Cannot unambiguously pick bioseq info record between: ";
        ret.ambiguity_json = "[";
        for (size_t i = 0; i < good_idx_v_ptr->size(); ++i) {
            if (need_comma) {
                ret.message += ", ";
                ret.ambiguity_json += ", ";
            }
            need_comma = true;

            size_t  record_index = (*good_idx_v_ptr)[i];
            ret.message += ToBioseqIndentification(records[record_index]);
            ret.ambiguity_json += ToJsonString(records[record_index],
                                               SPSGS_ResolveRequest::fPSGS_AllBioseqFields);
        }
        ret.ambiguity_json += "]";

        auto    app = CPubseqGatewayApp::GetInstance();
        // - produced log message
        // - engage an alert
        // - tick a counter
        if (is_cache) {
            PSG_WARNING("Cache lookup: " + ret.message);
            app->GetAlerts().Register(ePSGS_BioseqInfoCacheLookupIncompatibleRecords,
                                      "Cache lookup: " + ret.message);

            // It is safe to use nullptr instead of a processor pointer because
            // it is not a per-processor counter
            app->GetCounters().Increment(nullptr,
                                         CPSGSCounters::ePSGS_BioseqInfoCacheLookupAmbiguity);
        } else {
            PSG_WARNING("Cassandra lookup: " + ret.message);
            app->GetAlerts().Register(ePSGS_BioseqInfoDBLookupIncompatibleRecords,
                                      "Cassandra lookup: " + ret.message);

            // It is safe to use nullptr instead of a processor pointer because
            // it is not a per-processor counter
            app->GetCounters().Increment(nullptr,
                                         CPSGSCounters::ePSGS_BioseqInfoDBLookupAmbiguity);
        }
    }

    return ret;
}

