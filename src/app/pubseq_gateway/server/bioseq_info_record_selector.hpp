#ifndef BIOSEQ_INFO_RECORD_SELECTOR__HPP
#define BIOSEQ_INFO_RECORD_SELECTOR__HPP

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

#include <corelib/request_status.hpp>
#include <corelib/ncbidiag.hpp>
#include <objtools/pubseq_gateway/impl/cassandra/bioseq_info/record.hpp>

USING_NCBI_SCOPE;
USING_IDBLOB_SCOPE;

#include <string>
#include <vector>
using namespace std;


struct SPSGS_BioseqSelectionResult
{
    CRequestStatus::ECode   status;     // 200, 404, 300, 500
    ssize_t                 index;      // If 200 then the vector index to take
    string                  message;    // In case of errors or ambiguity

    string                  ambiguity_json; // in case of 300 this field stores
                                            // a json list of dictionaries with
                                            // the full bioseq info records
};


// Can return statuses 200, 300 and 404
SPSGS_BioseqSelectionResult
SelectBioseqInfoRecord(const vector<CBioseqInfoRecord>&  records,
                       bool  is_cache);

#endif
