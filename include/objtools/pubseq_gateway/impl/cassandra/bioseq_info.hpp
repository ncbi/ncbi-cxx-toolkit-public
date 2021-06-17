#ifndef BIOSEQ_INFO__HPP
#define BIOSEQ_INFO__HPP

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
 * Synchronous retrieving data from bioseq. tables
 *
 */

#include <corelib/ncbistd.hpp>
#include <corelib/request_status.hpp>
#include <objtools/pubseq_gateway/impl/cassandra/cass_driver.hpp>
#include "IdCassScope.hpp"

BEGIN_IDBLOB_SCOPE

USING_NCBI_SCOPE;


struct SBioseqInfo
{
    SBioseqInfo() :
        m_Version(0), m_SeqIdType(0)
    {}

    // Sequence of fields and the order are inherited from the DB
    // https://docs.datastax.com/en/cql/3.3/cql/cql_reference/cql_data_types_c.html
    string                          m_Accession;
    int16_t                         m_Version;
    int16_t                         m_SeqIdType;

    int64_t                         m_DateChanged;
    int32_t                         m_Hash;
    int64_t                         m_IdSync;
    int32_t                         m_Length;
    int8_t                          m_Mol;
    int16_t                         m_Sat;
    int32_t                         m_SatKey;
    set<tuple<int16_t, string>>     m_SeqIds;
    int8_t                          m_SeqState;
    int8_t                          m_State;
    int32_t                         m_TaxId;
};


// e200_Ok: exactly one record found
// e404_NotFound: no records found
// e300_MultipleChoices: more than one record found
CRequestStatus::ECode
FetchCanonicalSeqId(shared_ptr<CCassConnection>  conn,
                    const string &  keyspace,
                    const string &  sec_seq_id,
                    int16_t  sec_seq_id_type,
                    bool  sec_seq_id_type_provided,
                    string &  accession,                // output
                    int16_t &  version,                 // output
                    int16_t &  seq_id_type);            // output

// e200_Ok: exactly one record found
// e404_NotFound: no records found
// e300_MultipleChoices: more than one record found
CRequestStatus::ECode
FetchBioseqInfo(shared_ptr<CCassConnection>  conn,
                     const string &  keyspace,
                     bool  version_provided,
                     bool  seq_id_type_provided,
                     SBioseqInfo &  bioseq_info);

END_IDBLOB_SCOPE

#endif
