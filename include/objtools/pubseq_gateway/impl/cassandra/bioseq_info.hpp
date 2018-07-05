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
#include <objtools/pubseq_gateway/impl/cassandra/cass_driver.hpp>
#include "IdCassScope.hpp"

BEGIN_IDBLOB_SCOPE

USING_NCBI_SCOPE;


struct SBioseqInfo
{
    string              m_Accession;
    int                 m_Version;
    int                 m_IdType;

    int                 m_Mol;
    int                 m_Length;
    int                 m_State;
    int                 m_Sat;
    int                 m_SatKey;
    int                 m_TaxId;
    int                 m_Hash;
    map<int, string>    m_SeqIds;
};


// true => fetch succeeded
// false => not found
bool FetchCanonicalSeqId(shared_ptr<CCassConnection>  conn,
                         const string &  keyspace,
                         const string &  seq_id,
                         int  seq_id_type,
                         string &  accession,
                         int &  version,
                         int &  id_type);

// true => fetch succeeded
// false => not found
bool FetchBioseqInfo(shared_ptr<CCassConnection>  conn,
                     const string &  keyspace,
                     SBioseqInfo &  bioseq_info);

END_IDBLOB_SCOPE

#endif
