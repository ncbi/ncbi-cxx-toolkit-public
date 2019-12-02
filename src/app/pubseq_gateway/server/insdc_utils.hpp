#ifndef PUBSEQ_GATEWAY_INSDC_UTILS__HPP
#define PUBSEQ_GATEWAY_INSDC_UTILS__HPP

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

#include "pubseq_gateway_types.hpp"

USING_NCBI_SCOPE;
USING_IDBLOB_SCOPE;

#include <string>
#include <vector>
using namespace std;


struct SINSDCDecision
{
    CRequestStatus::ECode   status;     // 200, 404, 500
    size_t                  index;      // If 200 then the vector index to take
    string                  message;    // In case of errors
};


bool IsINSDCSeqIdType(CBioseqInfoRecord::TSeqIdType  seq_id_type);
string GetBioseqRecordId(const CBioseqInfoRecord &  record);

// version is from the lookup request
// if version is -1 => the lookup was without version
SINSDCDecision DecideINSDC(const vector<CBioseqInfoRecord>&  records,
                           CBioseqInfoRecord::TVersion  version);

#endif
