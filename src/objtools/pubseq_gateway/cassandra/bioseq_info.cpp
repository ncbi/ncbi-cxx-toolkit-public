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
               int16_t &  version,
               int16_t &  seq_id_type)
{
    accession = query->FieldGetStrValue(0);
    version = query->FieldGetInt16Value(1);
    seq_id_type = query->FieldGetInt16Value(2);
}


CRequestStatus::ECode
FetchCanonicalSeqId(shared_ptr<CCassConnection>  conn,
                    const string &  keyspace,
                    const string &  sec_seq_id,
                    int16_t  sec_seq_id_type,
                    bool  sec_seq_id_type_provided,
                    string &  accession,
                    int16_t &  version,
                    int16_t &  seq_id_type)
{
    static const string     s_Select = "SELECT accession, version, seq_id_type FROM ";
    static const string     s_Where_2 = ".SI2CSI WHERE sec_seq_id = ? AND sec_seq_id_type = ?";
    static const string     s_Where_1 = ".SI2CSI WHERE sec_seq_id = ?";
    shared_ptr<CCassQuery>  query = conn->NewQuery();

    // NB: the sequence of the retrieved fields must be in sync with
    // s_GetCSIValues(...) function.
    string      sql = s_Select;
    sql.append(keyspace);
    if (sec_seq_id_type_provided) {
        sql.append(s_Where_2);
        query->SetSQL(sql, 2);
        query->BindStr(0, sec_seq_id);
        query->BindInt16(1, sec_seq_id_type);
    } else {
        sql.append(s_Where_1);
        query->SetSQL(sql, 1);
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
        if (found)
            return CRequestStatus::e200_Ok;
    }
    return CRequestStatus::e404_NotFound;
}

// Select field numbers; must be in sync with the select statemens
static const int    fnDateChanged = 0;
static const int    fnHash = 1;
static const int    fnIdSync = 2;
static const int    fnLength = 3;
static const int    fnMol = 4;
static const int    fnSat = 5;
static const int    fnSatKey = 6;
static const int    fnSeqIds = 7;
static const int    fnSeqState = 8;
static const int    fnState = 9;
static const int    fnTaxId = 10;
static const int    fnSeqIdType = 11;
static const int    fnVersion = 12;


// NB: the field numbers must be in sync with the SQL statement, see
// FetchBioseqInfo(...)
static void
s_GetBioseqValues(shared_ptr<CCassQuery> &  query,
                  SBioseqInfo &  bioseq_info)
{
    bioseq_info.m_DateChanged = query->FieldGetInt64Value(fnDateChanged);
    bioseq_info.m_Hash = query->FieldGetInt32Value(fnHash);
    bioseq_info.m_IdSync = query->FieldGetInt64Value(fnIdSync);
    bioseq_info.m_Length = query->FieldGetInt32Value(fnLength);
    bioseq_info.m_Mol = query->FieldGetInt8Value(fnMol);
    bioseq_info.m_Sat = query->FieldGetInt16Value(fnSat);
    bioseq_info.m_SatKey = query->FieldGetInt32Value(fnSatKey);
    query->FieldGetContainerValue(fnSeqIds,
                                  inserter(bioseq_info.m_SeqIds,
                                           bioseq_info.m_SeqIds.end()));
    bioseq_info.m_SeqState = query->FieldGetInt8Value(fnSeqState);
    bioseq_info.m_State = query->FieldGetInt8Value(fnState);
    bioseq_info.m_TaxId =query->FieldGetInt32Value(fnTaxId);
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
        query->SetSQL("SELECT "
                      "date_changed, "
                      "hash, "
                      "id_sync, "
                      "length, "
                      "mol, "
                      "sat, "
                      "sat_key, "
                      "seq_ids, "
                      "seq_state, "
                      "state, "
                      "tax_id "
                      "FROM " +
                      keyspace + ".BIOSEQ_INFO WHERE "
                      "accession = ? AND version = ? AND seq_id_type = ?", 3);
        query->BindStr(0, bioseq_info.m_Accession);
        query->BindInt16(1, bioseq_info.m_Version);
        query->BindInt16(2, bioseq_info.m_SeqIdType);
    } else if (version_provided) {
        query->SetSQL("SELECT "
                      "date_changed, "
                      "hash, "
                      "id_sync, "
                      "length, "
                      "mol, "
                      "sat, "
                      "sat_key, "
                      "seq_ids, "
                      "seq_state, "
                      "state, "
                      "tax_id, "
                      "seq_id_type "
                      "FROM " +
                      keyspace + ".BIOSEQ_INFO WHERE "
                      "accession = ? AND version = ?", 2);
        query->BindStr(0, bioseq_info.m_Accession);
        query->BindInt16(1, bioseq_info.m_Version);
    } else {
        query->SetSQL("SELECT "
                      "date_changed, "
                      "hash, "
                      "id_sync, "
                      "length, "
                      "mol, "
                      "sat, "
                      "sat_key, "
                      "seq_ids, "
                      "seq_state, "
                      "state, "
                      "tax_id, "
                      "seq_id_type, "
                      "version "
                      "FROM " +
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
        int     selected_seq_id_type = INT_MIN;
        while (query->NextRow() == ar_dataready) {
            if (!found) {
                s_GetBioseqValues(query, bioseq_info);
                selected_seq_id_type = query->FieldGetInt16Value(fnSeqIdType);
                found = true;
                continue;
            }
            return CRequestStatus::e300_MultipleChoices;
        }

        if (found) {
            bioseq_info.m_SeqIdType = selected_seq_id_type;
            return CRequestStatus::e200_Ok;
        }
        return CRequestStatus::e404_NotFound;
    }


    // Case 3: accession and seq_id_type are provided.
    // So the latest version has to be retrieved
    if (seq_id_type_provided) {
        bool    found = false;
        int     selected_version = INT_MIN;
        while (query->NextRow() == ar_dataready) {
            int     seq_id_type = query->FieldGetInt16Value(fnSeqIdType);
            if (seq_id_type != bioseq_info.m_SeqIdType)
                continue;
            int     version = query->FieldGetInt16Value(fnVersion);
            if (!found || version > selected_version) {
                s_GetBioseqValues(query, bioseq_info);

                found = true;
                selected_version = version;
            }
        }

        if (found) {
            bioseq_info.m_Version = selected_version;
            return CRequestStatus::e200_Ok;
        }
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
            selected_version = query->FieldGetInt16Value(fnVersion);
            selected_seq_id_type = query->FieldGetInt16Value(fnSeqIdType);
            found = true;
            continue;
        }

        int     seq_id_type = query->FieldGetInt16Value(fnSeqIdType);
        if (selected_seq_id_type == seq_id_type) {
            // Take the latest version
            int     version = query->FieldGetInt16Value(fnVersion);
            if (version > selected_version) {
                s_GetBioseqValues(query, bioseq_info);
                selected_version = version;
            }
            continue;
        }
        return CRequestStatus::e300_MultipleChoices;
    }

    if (found) {
        bioseq_info.m_Version = selected_version;
        bioseq_info.m_SeqIdType = selected_seq_id_type;
        return CRequestStatus::e200_Ok;
    }
    return CRequestStatus::e404_NotFound;
}



END_IDBLOB_SCOPE
