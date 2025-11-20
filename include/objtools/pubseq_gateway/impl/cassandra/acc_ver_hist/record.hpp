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
 *  Blob storage: blob status history record
 *
 *****************************************************************************/

#ifndef OBJTOOLS__PUBSEQ_GATEWAY__IMPL__CASSANDRA__ACC_VER_HISTORY__RECORD_HPP
#define OBJTOOLS__PUBSEQ_GATEWAY__IMPL__CASSANDRA__ACC_VER_HISTORY__RECORD_HPP

#include <corelib/ncbistd.hpp>

#include <sstream>
#include <memory>
#include <string>

#include <corelib/request_status.hpp>
#include <corelib/ncbidiag.hpp>

#include <objtools/pubseq_gateway/impl/cassandra/cass_blob_op.hpp>
#include <objtools/pubseq_gateway/impl/cassandra/IdCassScope.hpp>

BEGIN_IDBLOB_SCOPE
USING_NCBI_SCOPE;

//:::::::
struct SAccVerHistRec
{
  //SBioseqInfoKeyPK bi_pk;  // PK
    int64_t gi;
    string  accession;
    int16_t version;
    int16_t seq_id_type = 0;
  
    int64_t date = 0;        // VAL  put it at this position for a better alignment
    int32_t sat_key = 0;     // VAL TSatKey
    int16_t sat = 0;         // VAL TSatId
    int64_t chain = 0;       // TBiSiGi

    SAccVerHistRec() :
        gi( 0), accession( ""), version( 0), seq_id_type( 0), date( 0),
        sat_key( 0), sat( 0), chain( 0)
    {}
    
    SAccVerHistRec& operator=(const SAccVerHistRec& rec)
    {
        gi          = rec.gi;
        accession   = rec.accession;
        version     = rec.version;
        seq_id_type = rec.seq_id_type;

        date      = rec.date;
      
        sat_key = rec.sat_key;
        sat     = rec.sat;
        chain   = rec.chain;
        return (* this);
    }
};

using TAccVerHistConsumeCallback = function<bool( SAccVerHistRec &&, bool last)>;

END_IDBLOB_SCOPE

#endif  // OBJTOOLS__PUBSEQ_GATEWAY__IMPL__CASSANDRA__ACC_VER_HISTORY__RECORD_HPP
