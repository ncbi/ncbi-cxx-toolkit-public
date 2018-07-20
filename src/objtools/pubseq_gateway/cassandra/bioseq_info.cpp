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


CRequestStatus::ECode
FetchCanonicalSeqId(shared_ptr<CCassConnection>  conn,
                    const string &  keyspace,
                    const string &  seq_id,
                    int  seq_id_type,
                    bool  seq_id_type_provided,
                    string &  accession,
                    int &  version,
                    int &  id_type)
{
    shared_ptr<CCassQuery>  query = conn->NewQuery();

    if (seq_id_type_provided) {
        query->SetSQL("SELECT accession, version, id_type FROM " +
                      keyspace + ".SI2CSI WHERE "
                      "seq_id = ? AND seq_id_type = ?", 2);
        query->BindStr(0, seq_id);
        query->BindInt32(1, seq_id_type);
    } else {
        query->SetSQL("SELECT accession, version, id_type FROM " +
                      keyspace + ".SI2CSI WHERE "
                      "seq_id = ?", 1);
        query->BindStr(0, seq_id);
    }

    query->Query(CANONICAL_SEQ_ID_CONSISTENCY, false, false);

    if (seq_id_type_provided) {
        if (query->NextRow() == ar_dataready) {
            accession = query->FieldGetStrValue(0);
            version = query->FieldGetInt32Value(1);
            id_type = query->FieldGetInt32Value(2);
            return CRequestStatus::e200_Ok;
        }
    } else {
        bool    found = false;
        while (query->NextRow() == ar_dataready) {
            if (found)
                return CRequestStatus::e300_MultipleChoices;
            accession = query->FieldGetStrValue(0);
            version = query->FieldGetInt32Value(1);
            id_type = query->FieldGetInt32Value(2);
            found = true;
        }
    }
    return CRequestStatus::e404_NotFound;
}


CRequestStatus::ECode
FetchBioseqInfo(shared_ptr<CCassConnection>  conn,
                const string &  keyspace,
                bool  version_provided,
                bool  id_type_provided,
                SBioseqInfo &  bioseq_info)
{
    // if version is not provided then the latest version has to be taken.
    // if id_type is not provided then the number of suitable records need to
    // be checked. It must be exactly 1.

    shared_ptr<CCassQuery>  query = conn->NewQuery();

    if (version_provided && id_type_provided) {
        query->SetSQL("SELECT mol, "
                      "length, "
                      "state, "
                      "sat, "
                      "sat_key, "
                      "tax_id, "
                      "hash, "
                      "seq_ids FROM " +
                      keyspace + ".BIOSEQ_INFO WHERE "
                      "accession = ? AND version = ? AND id_type = ?", 3);
        query->BindStr(0, bioseq_info.m_Accession);
        query->BindInt32(1, bioseq_info.m_Version);
        query->BindInt32(2, bioseq_info.m_IdType);
    } else if (version_provided) {
        query->SetSQL("SELECT mol, "
                      "length, "
                      "state, "
                      "sat, "
                      "sat_key, "
                      "tax_id, "
                      "hash, "
                      "seq_ids FROM " +
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
                      "id_type FROM " +
                      keyspace + ".BIOSEQ_INFO WHERE "
                      "accession = ?", 1);
        query->BindStr(0, bioseq_info.m_Accession);
    }


#if 0
    if (latest_version) {
    } else {
    }

    query->Query(BIOSEQ_INFO_CONSISTENCY, false, false);

    bool    found = false;
    if (latest_version) {
        int     selected_version = INT_MIN;
        while (query->NextRow() == ar_dataready) {
            int     id_type = query->FieldGetInt32Value(9);
            if (id_type != bioseq_info.m_IdType)
                continue;
            int     version = query->FieldGetInt32Value(8);
            if (!found || version > selected_version) {
                bioseq_info.m_Mol = query->FieldGetInt32Value(0);
                bioseq_info.m_Length = query->FieldGetInt32Value(1);
                bioseq_info.m_State = query->FieldGetInt32Value(2);
                bioseq_info.m_Sat = query->FieldGetInt32Value(3);
                bioseq_info.m_SatKey = query->FieldGetInt32Value(4);
                bioseq_info.m_TaxId =query->FieldGetInt32Value(5);
                bioseq_info.m_Hash = query->FieldGetInt32Value(6);
                query->FieldGetMapValue(7, bioseq_info.m_SeqIds);

                found = true;
                selected_version = version;
            }
        }
    } else {
        if (query->NextRow() == ar_dataready) {
            bioseq_info.m_Mol = query->FieldGetInt32Value(0);
            bioseq_info.m_Length = query->FieldGetInt32Value(1);
            bioseq_info.m_State = query->FieldGetInt32Value(2);
            bioseq_info.m_Sat = query->FieldGetInt32Value(3);
            bioseq_info.m_SatKey = query->FieldGetInt32Value(4);
            bioseq_info.m_TaxId =query->FieldGetInt32Value(5);
            bioseq_info.m_Hash = query->FieldGetInt32Value(6);
            query->FieldGetMapValue(7, bioseq_info.m_SeqIds);
            return true;
        }
    }
    return found;
#endif
    return CRequestStatus::e404_NotFound;
}



END_IDBLOB_SCOPE
