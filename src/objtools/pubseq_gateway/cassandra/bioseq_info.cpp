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

#include <ncbi_pch.hpp>
#include <objtools/pubseq_gateway/impl/cassandra/bioseq_info.hpp>

#define CANONICAL_SEQ_ID_CONSISTENCY    CassConsistency::CASS_CONSISTENCY_LOCAL_QUORUM
#define BIOSEQ_INFO_CONSISTENCY         CassConsistency::CASS_CONSISTENCY_LOCAL_QUORUM

BEGIN_IDBLOB_SCOPE


// NB: the field numbers must be in sync with the SQL statement, see
// FetchCanonicalSeqId(...)
static void
s_GetCSIValues(shared_ptr<CCassQuery> &  query,
               string &  accession,
               int &  version,
               int &  seq_id_type)
{
    accession = query->FieldGetStrValue(0);
    version = query->FieldGetInt32Value(1);
    seq_id_type = query->FieldGetInt32Value(2);
}


CRequestStatus::ECode
FetchCanonicalSeqId(shared_ptr<CCassConnection>  conn,
                    const string &  keyspace,
                    const string &  sec_seq_id,
                    int  sec_seq_id_type,
                    bool  sec_seq_id_type_provided,
                    string &  accession,
                    int &  version,
                    int &  seq_id_type)
{
    shared_ptr<CCassQuery>  query = conn->NewQuery();

    // NB: the sequence of the retrieved fields must be in sync with
    // s_GetCSIValues(...) function.
    if (sec_seq_id_type_provided) {
        query->SetSQL("SELECT accession, version, seq_id_type FROM " +
                      keyspace + ".SI2CSI WHERE "
                      "sec_seq_id = ? AND sec_seq_id_type = ?", 2);
        query->BindStr(0, sec_seq_id);
        query->BindInt32(1, sec_seq_id_type);
    } else {
        query->SetSQL("SELECT accession, version, seq_id_type FROM " +
                      keyspace + ".SI2CSI WHERE "
                      "sec_seq_id = ?", 1);
        query->BindStr(0, sec_seq_id);
    }

    query->Query(CANONICAL_SEQ_ID_CONSISTENCY, false, false);

    if (sec_seq_id_type_provided) {
        if (query->NextRow() == ar_dataready) {
            s_GetCSIValues(query, accession, version, seq_id_type);
            return CRequestStatus::e200_Ok;
        }
    } else {
        bool    found = false;
        while (query->NextRow() == ar_dataready) {
            if (found)
                return CRequestStatus::e300_MultipleChoices;
            s_GetCSIValues(query, accession, version, seq_id_type);
            found = true;
        }
    }
    return CRequestStatus::e404_NotFound;
}



// NB: the field numbers must be in sync with the SQL statement, see
// FetchBioseqInfo(...)
static void
s_GetBioseqValues(shared_ptr<CCassQuery> &  query,
                  SBioseqInfo &  bioseq_info)
{
    bioseq_info.m_Mol = query->FieldGetInt32Value(0);
    bioseq_info.m_Length = query->FieldGetInt32Value(1);
    bioseq_info.m_State = query->FieldGetInt32Value(2);
    bioseq_info.m_Sat = query->FieldGetInt32Value(3);
    bioseq_info.m_SatKey = query->FieldGetInt32Value(4);
    bioseq_info.m_TaxId =query->FieldGetInt32Value(5);
    bioseq_info.m_Hash = query->FieldGetInt32Value(6);
    query->FieldGetMapValue(7, bioseq_info.m_SeqIds);
}


CRequestStatus::ECode
FetchBioseqInfo(shared_ptr<CCassConnection>  conn,
                const string &  keyspace,
                bool  version_provided,
                bool  seq_id_type_provided,
                SBioseqInfo &  bioseq_info)
{
    // if version is not provided then the latest version has to be taken.
    // if id_type is not provided then the number of suitable records need to
    // be checked. It must be exactly 1.

    shared_ptr<CCassQuery>  query = conn->NewQuery();

    if (version_provided && seq_id_type_provided) {
        query->SetSQL("SELECT mol, "
                      "length, "
                      "state, "
                      "sat, "
                      "sat_key, "
                      "tax_id, "
                      "hash, "
                      "seq_ids FROM " +
                      keyspace + ".BIOSEQ_INFO WHERE "
                      "accession = ? AND version = ? AND seq_id_type = ?", 3);
        query->BindStr(0, bioseq_info.m_Accession);
        query->BindInt32(1, bioseq_info.m_Version);
        query->BindInt32(2, bioseq_info.m_SeqIdType);
    } else if (version_provided) {
        query->SetSQL("SELECT mol, "
                      "length, "
                      "state, "
                      "sat, "
                      "sat_key, "
                      "tax_id, "
                      "hash, "
                      "seq_ids, "
                      "seq_id_type FROM " +
                      keyspace + ".BIOSEQ_INFO WHERE "
                      "accession = ? AND version = ?", 2);
        query->BindStr(0, bioseq_info.m_Accession);
        query->BindInt32(1, bioseq_info.m_Version);
    } else {
        query->SetSQL("SELECT mol, "
                      "length, "
                      "state, "
                      "sat, "
                      "sat_key, "
                      "tax_id, "
                      "hash, "
                      "seq_ids, "
                      "version, "
                      "seq_id_type FROM " +
                      keyspace + ".BIOSEQ_INFO WHERE "
                      "accession = ?", 1);
        query->BindStr(0, bioseq_info.m_Accession);
    }


    query->Query(BIOSEQ_INFO_CONSISTENCY, false, false);

    // Case 1: all three fields are provided
    if (version_provided && seq_id_type_provided) {
        if (query->NextRow() == ar_dataready) {
            s_GetBioseqValues(query, bioseq_info);
            return CRequestStatus::e200_Ok;
        }
        return CRequestStatus::e404_NotFound;
    }

    // Case 2: accession and version are provided;
    //         check that there is exactly one record
    if (version_provided) {
        bool    found = false;
        while (query->NextRow() == ar_dataready) {
            if (!found) {
                s_GetBioseqValues(query, bioseq_info);
                found = true;
                continue;
            }
            return CRequestStatus::e300_MultipleChoices;
        }

        if (found)
            return CRequestStatus::e200_Ok;
        return CRequestStatus::e404_NotFound;
    }


    // Case 3: accession and seq_id_type are provided.
    // So the latest version has to be retrieved
    if (seq_id_type_provided) {
        bool    found = false;
        int     selected_version = INT_MIN;
        while (query->NextRow() == ar_dataready) {
            int     seq_id_type = query->FieldGetInt32Value(9);
            if (seq_id_type != bioseq_info.m_SeqIdType)
                continue;
            int     version = query->FieldGetInt32Value(8);
            if (!found || version > selected_version) {
                s_GetBioseqValues(query, bioseq_info);

                found = true;
                selected_version = version;
            }
        }

        if (found)
            return CRequestStatus::e200_Ok;
        return CRequestStatus::e404_NotFound;
    }

    // Case 4: only accession is provided;
    //         select the latest version;
    //         check that there is exactly one seq_it_type
    bool    found = false;
    int     selected_version = INT_MIN;
    int     selected_seq_id_type = INT_MIN;
    while (query->NextRow() == ar_dataready) {
        if (!found) {
            s_GetBioseqValues(query, bioseq_info);
            selected_version = query->FieldGetInt32Value(8);
            selected_seq_id_type = query->FieldGetInt32Value(9);
            found = true;
            continue;
        }

        int     seq_id_type = query->FieldGetInt32Value(9);
        if (selected_seq_id_type == seq_id_type) {
            // Take the latest version
            int     version = query->FieldGetInt32Value(8);
            if (version > selected_version) {
                s_GetBioseqValues(query, bioseq_info);
                selected_version = version;
            }
            continue;
        }
        return CRequestStatus::e300_MultipleChoices;
    }

    if (found)
        return CRequestStatus::e200_Ok;
    return CRequestStatus::e404_NotFound;
}



END_IDBLOB_SCOPE
