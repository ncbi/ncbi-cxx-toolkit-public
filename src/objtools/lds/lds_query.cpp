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

#include <ncbi_pch.hpp>
#include <corelib/ncbistr.hpp>

#include <bdb/bdb_cursor.hpp>
#include <bdb/bdb_util.hpp>
#include <bdb/bdb_query.hpp>
#include <bdb/bdb_query_parser.hpp>

#include <objects/seqloc/Seq_id.hpp>
#include <objects/general/Object_id.hpp>

#include <objtools/lds/lds_query.hpp>
#include <objtools/lds/lds_set.hpp>
#include <objtools/lds/lds_expt.hpp>

#include <vector>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

/// Base class for sequence search functors.
///
/// @internal
class CLDS_FindSeqIdBase
{
public:
    CLDS_FindSeqIdBase(const vector<string>&  seqids,
                       CLDS_Set*              obj_ids)
    :
      m_SeqIds(seqids),
      m_ResultSet(obj_ids)
    {
        _ASSERT(obj_ids);
    }

    bool MatchSeqId(const CSeq_id& seq_id_db, const string& candidate_str)
    {
        CSeq_id seq_id(candidate_str);
        if (seq_id.Which() == CSeq_id::e_not_set) {
            seq_id.SetLocal().SetStr(candidate_str);
            if (seq_id.Which() == CSeq_id::e_not_set) {
                return false;
            }
        }
        if (seq_id.Match(seq_id_db)) {
            return true;
        }
        // Sequence does not match, lets try "force it local" strategy
        //
        if (seq_id.Which() != CSeq_id::e_Local) {
            if (candidate_str.find('|') == NPOS) {
                seq_id.SetLocal().SetStr(candidate_str);
                if (seq_id.Which() != CSeq_id::e_Local) {
                    return false;
                }
                if (seq_id.Match(seq_id_db)) {
                    return true;
                }
            }
        }
        return false;
    }

private:
    CLDS_FindSeqIdBase(const CLDS_FindSeqIdBase&);
    CLDS_FindSeqIdBase& operator=(const CLDS_FindSeqIdBase&);

protected:
    const vector<string>&   m_SeqIds;    // Search criteria
    CLDS_Set*               m_ResultSet; // Search result 
};

/// Functor used for scanning the Berkeley DB database.
/// This functor is driven by the BDB_iterate_file algorithm,
/// checks every object record to determine if it contains 
/// objects(molecules) satisfying the the given set of ids.
///
/// @internal
class CLDS_FindSeqIdFunctor : public CLDS_FindSeqIdBase
{
public:
    CLDS_FindSeqIdFunctor(SLDS_TablesCollection& db,
                          const vector<string>&  seqids,
                          CLDS_Set*              obj_ids)
    : CLDS_FindSeqIdBase(seqids, obj_ids),
      m_db(db)
    {
    }

    void operator()(SLDS_ObjectDB& dbf)
    {
        if (dbf.primary_seqid.IsEmpty())
            return;

        int object_id = dbf.object_id;
        int tse_id = dbf.TSE_object_id;

        string seq_id_str(dbf.primary_seqid);
        if (seq_id_str.empty())
            return;

        CSeq_id seq_id_db(seq_id_str);
        if (seq_id_db.Which() == CSeq_id::e_not_set) {
            seq_id_db.SetLocal().SetStr(seq_id_str);
            if (seq_id_db.Which() == CSeq_id::e_not_set) {
                return;
            }
        }

        // Check the seqids vector against the primary seq id
        //
        ITERATE (vector<string>, it, m_SeqIds) {
            if (MatchSeqId(seq_id_db, *it)) {
                m_ResultSet->insert(tse_id ? tse_id : object_id);
                return;
            }
        }

        // Primary seq id gave no hit. Scanning the supplemental list (attributes)
        //
/*
        m_db.object_attr_db.object_attr_id = object_id;
        if (m_db.object_attr_db.Fetch() != eBDB_Ok) {
            return;
        }
*/

        if (dbf.seq_ids.IsNull() || 
            dbf.seq_ids.IsEmpty()) {
            return;
        }

        string attr_seq_ids(dbf.seq_ids);
        vector<string> seq_id_arr;
        
        NStr::Tokenize(attr_seq_ids, " ", seq_id_arr, NStr::eMergeDelims);

        ITERATE (vector<string>, it, seq_id_arr) {
            CSeq_id seq_id_db(*it);

            if (seq_id_db.Which() == CSeq_id::e_not_set) {
                seq_id_db.SetLocal().SetStr(*it);
                if (seq_id_db.Which() == CSeq_id::e_not_set) {
                    continue;
                }
            }

            ITERATE (vector<string>, it2, m_SeqIds) {
                if (MatchSeqId(seq_id_db, *it2)) {
                    m_ResultSet->insert(tse_id ? tse_id : object_id);
                    return;
                }
            }
        }

    }
private:
    CLDS_FindSeqIdFunctor(const CLDS_FindSeqIdFunctor&);
    CLDS_FindSeqIdFunctor& operator=(const CLDS_FindSeqIdFunctor&);

private:
    SLDS_TablesCollection&  m_db;        // The LDS database
};

///
/// Functor used for scanning the SLDS_SeqId_List database.
///
/// @internal
class CLDS_FindSeqIdListFunctor : public CLDS_FindSeqIdBase
{
public:
    CLDS_FindSeqIdListFunctor(const vector<string>&  seqids,
                              CLDS_Set*  obj_ids)
    : CLDS_FindSeqIdBase(seqids, obj_ids)
    {
    }

    void operator()(SLDS_SeqId_List& dbf)
    {
        if (dbf.seq_id.IsEmpty())
            return;

        const char* str_id = dbf.seq_id;
        
        CSeq_id seq_id_db(str_id);
        if (seq_id_db.Which() == CSeq_id::e_not_set) {
            seq_id_db.SetLocal().SetStr((const char*)dbf.seq_id);
            if (seq_id_db.Which() == CSeq_id::e_not_set) {
                return;
            }
        }
        int object_id = dbf.object_id;

        ITERATE (vector<string>, it, m_SeqIds) {
            if (MatchSeqId(seq_id_db, *it)) {
                m_ResultSet->insert(object_id);
                return;
            }
        }
    }
};

inline 
string LDS_TypeMapSearch(const map<string, int>& type_map, int type)
{
    typedef map<string, int> TName2Id;
    ITERATE (TName2Id, it, type_map) {
        if (it->second == type) {
            return it->first;
        }
    }
    return kEmptyStr;
}

/// Query scanner functor for objects
///
/// @internal
class CLDS_IdTableScanner : public CBDB_FileScanner
{
public:

    CLDS_IdTableScanner(CBDB_File& dbf, CLDS_Set* rec_ids)
    : CBDB_FileScanner(dbf),
      m_ResultSet(*rec_ids)
    {}

    virtual EScanAction OnRecordFound()
    {
        int rowid = BDB_get_rowid(m_File);
        if (rowid) {
            m_ResultSet.insert(rowid);
        }
        return eContinue;
    }

protected:
    CLDS_Set&   m_ResultSet;
};


//////////////////////////////////////////////////////////////////
//
// CLDS_Query

bool CLDS_Query::FindFile(const string& path)
{
    CBDB_FileCursor cur(m_db.file_db);
    cur.SetCondition(CBDB_FileCursor::eFirst);
    while (cur.Fetch() == eBDB_Ok) {
        string fname(m_db.file_db.file_name);
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

void CLDS_Query::FindSeqIdList(const vector<string>& seqids, CLDS_Set* obj_ids)
{
    CLDS_FindSeqIdListFunctor search_func(seqids, obj_ids);
    BDB_iterate_file(m_db.seq_id_list, search_func);
}

void CLDS_Query::FindSequences(const string& query_str, CLDS_Set* obj_ids)
{
    _ASSERT(obj_ids);
    CLDS_IdTableScanner scanner(m_db.object_db, obj_ids);
    CBDB_Query    query;
    try {
        BDB_ParseQuery(query_str.c_str(), &query);
    } catch (CBDB_LibException&) {
        return; // ignore errors
    }

    scanner.Scan(query); 
}

CLDS_Query::SObjectDescr 
CLDS_Query::GetObjectDescr(const map<string, int>& type_map, 
                           int id,
                           bool trace_to_top)
{
    SObjectDescr descr;

    // Check objects
    //
    m_db.object_db.object_id = id;
    if (m_db.object_db.Fetch() == eBDB_Ok) {
        int tse_id = m_db.object_db.TSE_object_id;
        
        if (tse_id && trace_to_top) {
            // If non-top level entry, call recursively redirected to
            // the top level object
            return GetObjectDescr(type_map, tse_id, trace_to_top);
        }

        descr.id = id;
        descr.is_object = true;

        int object_type = m_db.object_db.object_type;
        descr.type_str = LDS_TypeMapSearch(type_map, object_type);

        int file_id = m_db.object_db.file_id;

        m_db.file_db.file_id = file_id;
        if (m_db.file_db.Fetch() != eBDB_Ok) {
            LDS_THROW(eRecordNotFound, "File record not found.");
        }

        descr.format = (CFormatGuess::EFormat)(int)m_db.file_db.format;
        descr.file_name = m_db.file_db.file_name;
        descr.offset = m_db.object_db.file_offset;
        descr.title = m_db.object_db.object_title;
/*
        m_db.object_attr_db.object_attr_id = id;
        if (m_db.object_attr_db.Fetch() == eBDB_Ok) {
            descr.title = m_db.object_attr_db.object_title;
        }
*/
        return descr;
    }

    // Check annotations
    //

    m_db.annot_db.annot_id = id;
    if (m_db.annot_db.Fetch() == eBDB_Ok) {
        int top_level_id = m_db.annot_db.top_level_id;
        
        if (top_level_id && trace_to_top) {
            // If non-top level entry, call recursively redirected to
            // the top level object
            return GetObjectDescr(type_map, top_level_id, trace_to_top);
        }

        descr.id = id;
        descr.is_object = false;

        int object_type = m_db.annot_db.annot_type;
        descr.type_str = LDS_TypeMapSearch(type_map, object_type);

        int file_id = m_db.annot_db.file_id;

        m_db.file_db.file_id = file_id;
        if (m_db.file_db.Fetch() != eBDB_Ok) {
            LDS_THROW(eRecordNotFound, "File record not found.");
        }

        descr.format = (CFormatGuess::EFormat)(int)m_db.file_db.format;
        descr.file_name = m_db.file_db.file_name;
        descr.offset = m_db.annot_db.file_offset;

        return descr;
    }

    descr.id = 0; // not found
    return descr;
}


END_SCOPE(objects)
END_NCBI_SCOPE

/*
 * ===========================================================================
 * $Log$
 * Revision 1.12  2004/05/21 21:42:55  gorelenk
 * Added PCH ncbi_pch.hpp
 *
 * Revision 1.11  2004/03/11 18:43:41  kuznets
 * + FindSequences
 *
 * Revision 1.10  2004/03/09 17:16:59  kuznets
 * Merge object attributes with objects
 *
 * Revision 1.9  2003/08/06 20:49:13  kuznets
 * SObjectDescr::title handled in CLDS_Query::GetObjectDescr
 *
 * Revision 1.8  2003/07/14 19:48:04  kuznets
 * Minor changes to improve debugging
 *
 * Revision 1.7  2003/07/10 20:09:53  kuznets
 * Implemented GetObjectDescr query. Searches both objects and annotations.
 *
 * Revision 1.6  2003/07/09 19:32:10  kuznets
 * Added query scanning sequence id list.
 *
 * Revision 1.5  2003/06/27 14:36:45  kuznets
 * Fixed compilation problem with GCC
 *
 * Revision 1.4  2003/06/24 18:32:39  kuznets
 * Code clean up. Improved sequence id comparison.
 *
 * Revision 1.3  2003/06/24 15:40:30  kuznets
 * Working on sequence id scan search. Improved recognition of local sequences.
 *
 * Revision 1.2  2003/06/20 19:56:41  kuznets
 * Implemented new function "FindSequences"
 *
 * Revision 1.1  2003/06/16 14:55:00  kuznets
 * lds splitted into "lds" and "lds_admin"
 *
 *
 * ===========================================================================
 */
