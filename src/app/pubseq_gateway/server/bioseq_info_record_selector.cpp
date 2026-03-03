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

USING_NCBI_SCOPE;


ssize_t  SelectBioseqInfoRecord(const vector<CBioseqInfoRecord>&  records)
{
    static auto app = CPubseqGatewayApp::GetInstance();

    if (records.empty())
        return -1;

    if (records.size() == 1)
        return 0;

    ssize_t                             index = -1;


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

    TGi max_gi = ZERO_GI;
    CBioseqInfoRecord::TVersion max_version = -1;
    size_t k = -1;
    CBioseqInfoRecord::TSeqIdType  current_seq_id_type;

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
            index = k;
            max_gi = gi;
            max_version = records[k].GetVersion();
        } else if (gi == max_gi) {
            auto version = records[k].GetVersion();
            if (version > max_version) {
                index = k;
                max_version = version;
            }
        }
    }

    if (!seq_id_types_compatible) {
        // The seq id types are not compatible between multiple records
        // Potentially could be:
        // - produced log message
        // - engage an alert
        // - tick a counter
        return -1;
    }

    return index;
}

