#ifndef OBJTOOLS__PUBSEQ_GATEWAY__CACHE__RESPONSE_HPP_
#define OBJTOOLS__PUBSEQ_GATEWAY__CACHE__RESPONSE_HPP_

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
 * Authors: Dmitrii Saprykin
 *
 * Response structures for PSG cache
 *
 */

#include <string>
#include <vector>

#include <corelib/ncbistd.hpp>

#include <objtools/pubseq_gateway/impl/cassandra/IdCassScope.hpp>
#include <objtools/pubseq_gateway/impl/cassandra/bioseq_info/record.hpp>
#include <objtools/pubseq_gateway/impl/cassandra/si2csi/record.hpp>
#include <objtools/pubseq_gateway/impl/cassandra/blob_record.hpp>

BEGIN_IDBLOB_SCOPE

struct SBioseqInfoCacheRecord {
    string accession;
    CBioseqInfoRecord::TVersion version = 0;
    CBioseqInfoRecord::TSeqIdType seq_id_type = 0;
    CBioseqInfoRecord::TGI gi = 0;
    string data;
};

struct SBlobPropCacheRecord {
    int32_t sat;
    CBlobRecord::TSatKey sat_key = 0;
    CBlobRecord::TTimestamp last_modified = 0;
    string data;
};

struct SSi2CsiCacheRecord {
    CSI2CSIRecord::TSecSeqId sec_seqid;
    CSI2CSIRecord::TSecSeqIdType sec_seqid_type = 0;
    string data;
};

using TBioseqInfoCacheResponse = vector<SBioseqInfoCacheRecord>;
using TBlobPropCacheResponse = vector<SBlobPropCacheRecord>;
using TSi2CsiCacheResponse = vector<SSi2CsiCacheRecord>;

END_IDBLOB_SCOPE

#endif  // OBJTOOLS__PUBSEQ_GATEWAY__CACHE__RESPONSE_HPP_
