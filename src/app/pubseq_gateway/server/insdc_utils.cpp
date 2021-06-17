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

#include <corelib/ncbistr.hpp>
#include <objects/seqloc/Seq_id.hpp>

#include "insdc_utils.hpp"

USING_NCBI_SCOPE;
USING_SCOPE(objects);



bool IsINSDCSeqIdType(CBioseqInfoRecord::TSeqIdType  seq_id_type)
{
    if (seq_id_type == CSeq_id_Base::e_Genbank ||   // 5
        seq_id_type == CSeq_id_Base::e_Embl ||      // 6
        seq_id_type == CSeq_id_Base::e_Ddbj ||      // 13
        seq_id_type == CSeq_id_Base::e_Tpg ||       // 16
        seq_id_type == CSeq_id_Base::e_Tpe ||       // 17
        seq_id_type == CSeq_id_Base::e_Tpd)         // 18
        return true;
    return false;
}


string GetBioseqRecordId(const CBioseqInfoRecord &  record)
{
    return record.GetAccession() + "." +
           to_string(record.GetVersion()) + "." +
           to_string(record.GetSeqIdType()) + "." +
           to_string(record.GetGI());
}


// The function makes a decision about what record from the vector should
// be taken in case of a secondary lookup without a sec_id_type when the
// original seq_id_type was INSDC and the primary lookup returned nothing.
SINSDCDecision DecideINSDC(const vector<CBioseqInfoRecord> &  records,
                           CBioseqInfoRecord::TVersion  version)
{
    SINSDCDecision      decision = {CRequestStatus::e404_NotFound, 0, ""};

    CBioseqInfoRecord::TVersion     current_version = -1;
    size_t                          conflict_index_1 = 0;
    size_t                          conflict_index_2 = 0;
    for (size_t  k = 0; k < records.size(); ++k) {
        if (!IsINSDCSeqIdType(records[k].GetSeqIdType()))
            continue;

        if (version != -1) {
            // Exact version has been requested
            if (records[k].GetVersion() == version) {
                if (decision.status != CRequestStatus::e404_NotFound) {
                    // Already found the requested version => data error
                    decision.status = CRequestStatus::e500_InternalServerError;
                    decision.message = "At least two bioseq info INSDC records "
                        "with the requested version " + to_string(version) +
                        " found during secondary lookup. First record: " +
                        GetBioseqRecordId(records[decision.index]) +
                        " Second record: " +
                        GetBioseqRecordId(records[k]);
                    break;
                }

                // First hit to the version
                decision.status = CRequestStatus::e200_Ok;
                decision.index = k;
            }
        } else {
            // Version was not requested; need to pick up the latest
            if (decision.status == CRequestStatus::e404_NotFound) {
                // First hit; pick the record
                decision.status = CRequestStatus::e200_Ok;
                decision.index = k;
                current_version = records[k].GetVersion();
                continue;
            }

            // not the first hit;
            auto    record_version = records[k].GetVersion();
            if (record_version < current_version) {
                continue;
            }
            if (record_version > current_version) {
                decision.status = CRequestStatus::e200_Ok;
                decision.index = k;
                current_version = record_version;
                conflict_index_1 = 0;
                conflict_index_2 = 0;
                continue;
            }

            // Version is the same as the currently picked. However it could
            // be two records with non max version
            conflict_index_1 = decision.index;
            conflict_index_2 = k;
        }
    }

    if (conflict_index_1 != conflict_index_2) {
        // This is a request without the version and there are more than one
        // record with the same max version
        decision.status = CRequestStatus::e500_InternalServerError;
        decision.message = "At least two bioseq info INSDC records "
            "with the same max version " +
            to_string(records[conflict_index_1].GetVersion()) +
            " found during secondary lookup. First record: " +
            GetBioseqRecordId(records[conflict_index_1]) +
            " Second record: " +
            GetBioseqRecordId(records[conflict_index_2]);
    }

    return decision;
}

