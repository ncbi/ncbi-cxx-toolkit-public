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

USING_NCBI_SCOPE;


ssize_t  SelectBioseqInfoRecord(const vector<CBioseqInfoRecord>&  records)
{
    if (records.empty())
        return -1;

    if (records.size() == 1)
        return 0;

    ssize_t                             index = -1;
    CBioseqInfoRecord::TVersion         version = -1;
    CBioseqInfoRecord::TDateChanged     date_changed = 0;

    // First try: select among those which have seq_state as alive
    // Pick the max version record (ans max date changed if more than one with
    // max version)
    for (size_t  k = 0; k < records.size(); ++k) {
        if (records[k].GetSeqState() != ncbi::psg::retrieval::SEQ_STATE_LIVE)
            continue;

        if (index == -1) {
            // Not found yet
            index = k;
            version = records[k].GetVersion();
            date_changed = records[k].GetDateChanged();
            continue;
        }

        // Something has already been picked
        if (records[k].GetVersion() > version) {
            index = k;
            version = records[k].GetVersion();
            date_changed = records[k].GetDateChanged();
        } else {
            if (records[k].GetVersion() == version) {
                if (records[k].GetDateChanged() > date_changed) {
                    index = k;
                    date_changed = records[k].GetDateChanged();
                }
            }
        }
    }

    if (index != -1)
        return index;

    // Second try: all records are not SEQ_STATE_ALIVE so select basing on
    // the max version (and max date changed if more than one with the max
    // version)
    index = 0;
    version = records[0].GetVersion();
    date_changed = records[0].GetDateChanged();
    for (size_t  k = 0; k < records.size(); ++k) {
        if (records[k].GetVersion() > version) {
            index = k;
            version = records[k].GetVersion();
            date_changed = records[k].GetDateChanged();
        } else {
            if (records[k].GetVersion() == version) {
                if (records[k].GetDateChanged() > date_changed) {
                    index = k;
                    date_changed = records[k].GetDateChanged();
                }
            }
        }
    }

    return index;
}

