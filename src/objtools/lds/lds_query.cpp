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
 * Author: Anatoliy Kuznetsov
 *
 * File Description:  Different query functions to LDS database.
 *
 */

#include <corelib/ncbistr.hpp>

#include <bdb/bdb_cursor.hpp>
#include <bdb/bdb_util.hpp>

#include <objects/seqloc/Seq_id.hpp>

#include <objtools/lds/lds_query.hpp>
#include <objtools/lds/lds_set.hpp>

#include <vector>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


//////////////////////////////////////////////////////////////////
//
// Class Functor used for scanning the Berkeley DB database.
// This functor is driven by the BDB_iterate_file algorithm,
// checks every object record to determine if it contains 
// objects(molecules) satisfying the the given set of ids.
//

class CLDS_FindSeqIdFunctor
{
public:
    CLDS_FindSeqIdFunctor(SLDS_TablesCollection& db,
                          const vector<string>&  seqids,
                          CLDS_Set*              obj_ids)
    : m_db(db),
      m_SeqIds(seqids),
      m_ResultSet(obj_ids)
    {
        _ASSERT(obj_ids);
    }

    void operator()(SLDS_ObjectDB& dbf)
    {
        if (dbf.primary_seqid.IsNull())
            return;

        int object_id = dbf.object_id;
        int tse_id = dbf.TSE_object_id;

        string seq_id_str = dbf.primary_seqid;
        if (seq_id_str.empty())
            return;
        CSeq_id seq_id_db(seq_id_str);

        // Check the seqids vector against the primary seq id
        //
        vector<string>::const_iterator it;
        for (it = m_SeqIds.begin(); it != m_SeqIds.end(); ++it) {
            const CSeq_id seq_id(*it);

            if (seq_id.Which() == CSeq_id::e_not_set)
                continue;

            if (seq_id.Match(seq_id_db)) {
                m_ResultSet->insert(tse_id ? tse_id : object_id);
                return;
            }            
        } // for

        // Primary seq id gave no hit. Scanning the supplemental list (attributes)
        //
        m_db.object_attr_db.object_attr_id = object_id;
        if (m_db.object_attr_db.Fetch() != eBDB_Ok) {
            return;
        }

        if (m_db.object_attr_db.seq_ids.IsNull() || 
            m_db.object_attr_db.seq_ids.IsEmpty()) {
            return;
        }

        string attr_seq_ids = m_db.object_attr_db.seq_ids;
        vector<string> seq_id_arr;
        
        NStr::Tokenize(attr_seq_ids, " ", seq_id_arr, NStr::eMergeDelims);
        for (it = seq_id_arr.begin(); it != seq_id_arr.end(); ++it) {
            const CSeq_id seq_id_db(*it);

            if (seq_id_db.Which() == CSeq_id::e_not_set)
                continue;

            vector<string>::const_iterator it2;
            for (it2 = m_SeqIds.begin(); it2 != m_SeqIds.end(); ++it2) {
                const CSeq_id seq_id(*it2);

                if (seq_id.Which() == CSeq_id::e_not_set)
                    continue;
                
                if (seq_id.Match(seq_id_db)) {
                    m_ResultSet->insert(tse_id ? tse_id : object_id);
                    return;
                }

            } // for
            
        } // for

    }

private:
    CLDS_FindSeqIdFunctor(const CLDS_FindSeqIdFunctor&);
    CLDS_FindSeqIdFunctor& operator=(const CLDS_FindSeqIdFunctor&);

private:
    SLDS_TablesCollection&  m_db;        // The LDS database
    const vector<string>&   m_SeqIds;    // Seq-Ids to search for
    CLDS_Set*               m_ResultSet; // Search result 
};


//////////////////////////////////////////////////////////////////
//
// CLDS_Query

bool CLDS_Query::FindFile(const string& path)
{
    CBDB_FileCursor cur(m_db.file_db);
    cur.SetCondition(CBDB_FileCursor::eFirst);
    while (cur.Fetch() == eBDB_Ok) {
        string fname = m_db.file_db.file_name;
        if (fname == path) {
            return true;
        }
    }
    return false;
}

void CLDS_Query::FindSequences(const vector<string>& seqids, 
                               CLDS_Set* obj_ids)
{
    CLDS_FindSeqIdFunctor search_func(m_db, seqids, obj_ids);
    BDB_iterate_file(m_db.object_db, search_func);
}

END_SCOPE(objects)
END_NCBI_SCOPE

/*
 * ===========================================================================
 * $Log$
 * Revision 1.2  2003/06/20 19:56:41  kuznets
 * Implemented new function "FindSequences"
 *
 * Revision 1.1  2003/06/16 14:55:00  kuznets
 * lds splitted into "lds" and "lds_admin"
 *
 *
 * ===========================================================================
 */
