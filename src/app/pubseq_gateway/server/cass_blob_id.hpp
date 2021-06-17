#ifndef CASS_BLOB_ID__HPP
#define CASS_BLOB_ID__HPP

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
 * File Description: Cassandra processors blob id
 *
 */

#include <objtools/pubseq_gateway/impl/cassandra/bioseq_info/record.hpp>
#include <string>

USING_IDBLOB_SCOPE;


// Cassandra blob identifier consists of two integers: sat and sat key.
// The blob sat eventually needs to be resolved to a sat name.
struct SCass_BlobId
{
    CBioseqInfoRecord::TSat     m_Sat;
    CBioseqInfoRecord::TSatKey  m_SatKey;

    // Resolved sat
    // The resolved sat appears later in the process
    string                      m_Keyspace;

    SCass_BlobId() :
        m_Sat(-1), m_SatKey(-1)
    {}

    SCass_BlobId(int  sat, int  sat_key) :
        m_Sat(sat), m_SatKey(sat_key)
    {}

    SCass_BlobId(const string &  blob_id);

    SCass_BlobId(const SCass_BlobId &) = default;
    SCass_BlobId(SCass_BlobId &&) = default;
    SCass_BlobId &  operator=(const SCass_BlobId &) = default;
    SCass_BlobId &  operator=(SCass_BlobId &&) = default;

    bool MapSatToKeyspace(void);

    string ToString(void) const
    {
        return to_string(m_Sat) + '.' + to_string(m_SatKey);
    }

    bool IsValid(void) const
    {
        return m_Sat >= 0 && m_SatKey >= 0;
    }

    bool operator < (const SCass_BlobId &  other) const
    {
        if (m_Sat == other.m_Sat)
            return m_SatKey < other.m_SatKey;
        return m_Sat < other.m_Sat;
    }

    bool operator == (const SCass_BlobId &  other) const
    {
        return m_Sat == other.m_Sat && m_SatKey == other.m_SatKey;
    }

    bool operator != (const SCass_BlobId &  other) const
    {
        return !this->operator==(other);
    }
};


#endif  // CASS_BLOB_ID__HPP

