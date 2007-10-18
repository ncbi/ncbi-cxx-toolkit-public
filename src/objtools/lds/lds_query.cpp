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
#include <corelib/ncbistre.hpp>

#include <bdb/bdb_cursor.hpp>
#include <bdb/bdb_util.hpp>
#include <bdb/bdb_query.hpp>
#include <bdb/bdb_query_parser.hpp>

#include <objects/seqloc/Seq_id.hpp>
#include <objects/general/Object_id.hpp>

#include <objtools/lds/lds_query.hpp>
#include <objtools/lds/lds_set.hpp>
#include <objtools/lds/lds_expt.hpp>
#include <objtools/error_codes.hpp>

#include <vector>


#define NCBI_USE_ERRCODE_X   Objtools_LDS_Query

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
        if (candidate_str.empty())
            return false;
        CRef<CSeq_id> seq_id;
        try {
            seq_id.Reset(new CSeq_id(candidate_str));
        } catch (CSeqIdException&) {
            seq_id.Reset(new CSeq_id(CSeq_id::e_Local, candidate_str));
        }
        if (seq_id->Match(seq_id_db)) {
            return true;
        }
        // Sequence does not match, let's try "force it local" strategy
        //
        if (seq_id->Which() != CSeq_id::e_Local) {
            if (candidate_str.find('|') == NPOS) {
                seq_id->SetLocal().SetStr(candidate_str);
                if ( !seq_id->IsLocal() ) {
                    return false;
                }
                if (seq_id->Match(seq_id_db)) {
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
///
class CLDS_FindSeqIdFunctor : public CLDS_FindSeqIdBase
{
public:
    CLDS_FindSeqIdFunctor(SLDS_TablesCollection& db,
                          const vector<string>&  seqids,
                          CLDS_Set*              obj_ids)
    : CLDS_FindSeqIdBase(seqids, obj_ids),
      m_db(db)
    {}

    void operator()(SLDS_ObjectDB& dbf)
    {
        if (dbf.primary_seqid.IsEmpty())
            return;

        int object_id = dbf.object_id;
        int tse_id = dbf.TSE_object_id;

        string seq_id_str(dbf.primary_seqid);
        if (seq_id_str.empty())
            return;

        CRef<CSeq_id> seq_id_db;
        try {
            seq_id_db.Reset(new CSeq_id(seq_id_str));
        } catch (CSeqIdException&) {
            seq_id_db.Reset(new CSeq_id(CSeq_id::e_Local, seq_id_str));
        }

        // Check the seqids vector against the primary seq id
        //
        ITERATE (vector<string>, it, m_SeqIds) {
            const string& s = *it;
            if (s.empty()) { 
                continue;
            }
            if (MatchSeqId(*seq_id_db, s)) {
                m_ResultSet->set(tse_id ? tse_id : object_id);
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
            const string& s = *it;
            if (s.empty()) {
                continue;
            }
            CRef<CSeq_id> seq_id_db;
            try {
                seq_id_db.Reset(new CSeq_id(s));
            } catch (CSeqIdException&) {
                seq_id_db.Reset(new CSeq_id(CSeq_id::e_Local, s));
            }

            ITERATE (vector<string>, it2, m_SeqIds) {
                if (MatchSeqId(*seq_id_db, *it2)) {
                    m_ResultSet->set(tse_id ? tse_id : object_id);
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
        
        CRef<CSeq_id> seq_id_db;
        try {
            seq_id_db.Reset(new CSeq_id(str_id));
        } catch (CSeqIdException&) {
            seq_id_db.Reset(new CSeq_id(CSeq_id::e_Local, str_id));
        }
        int object_id = dbf.object_id;

        ITERATE (vector<string>, it, m_SeqIds) {
            if (MatchSeqId(*seq_id_db, *it)) {
                m_ResultSet->set(object_id);
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
            m_ResultSet.set(rowid);
        }
        return eContinue;
    }

protected:
    CLDS_Set&   m_ResultSet;
};


//////////////////////////////////////////////////////////////////
//
// CLDS_Query

CLDS_Query::CLDS_Query(CLDS_Database& db)
: m_DataBase(db),
  m_db(db.GetTables())
{}


bool CLDS_Query::FindFile(const string& path)
{
    {{
    CBDB_FileCursor cur(m_db.file_filename_idx);
    cur.SetCondition(CBDB_FileCursor::eEQ);
    cur.From << path;
    if (cur.Fetch() == eBDB_Ok) {
        m_db.file_db.file_id = m_db.file_filename_idx.file_id;
        m_db.file_db.Fetch();
        return true;
    }
    }}

/*
    CBDB_FileCursor cur(m_db.file_db);
    cur.SetCondition(CBDB_FileCursor::eFirst);
    while (cur.Fetch() == eBDB_Ok) {
        string fname(m_db.file_db.file_name);
        if (fname == path) {
            return true;
        }
    }
*/
    return false;
}

void CLDS_Query::FindSequence(const string&   seqid, 
                              CLDS_Set*       obj_ids,
                              CLDS_Set*       cand_ids,
                              CSeq_id*        tmp_seqid,
                              SLDS_SeqIdBase* sbase)
{
    auto_ptr<CLDS_Set> tmp_set;

    if (cand_ids == 0) {
        tmp_set.reset(cand_ids = (new CLDS_Set));
    }

    CRef<CSeq_id> local_seq_id;
}


CLDS_Query::CSequenceFinder::CSequenceFinder(CLDS_Query& query)
: m_Query(query),
  m_CurInt_idx(m_Query.m_db.obj_seqid_int_idx),
  m_CurTxt_idx(m_Query.m_db.obj_seqid_txt_idx),
  m_SeqId(new CSeq_id),
  m_CandidateSet(bm::BM_GAP)
{
    m_CurInt_idx.SetCondition(CBDB_FileCursor::eEQ);
    m_CurTxt_idx.SetCondition(CBDB_FileCursor::eEQ);
}


CLDS_Query::CSequenceFinder::~CSequenceFinder()
{
}


void CLDS_Query::CSequenceFinder::Find(const string&   seqid, 
                                       CLDS_Set*       obj_ids)
{
    m_CandidateSet.clear();
    Screen(seqid);

    if (m_CandidateSet.any()) {
        vector<string> seqids(1);
        seqids.push_back(seqid);
        
        FindInCandidates(seqids, obj_ids);
    }
}

void CLDS_Query::CSequenceFinder::Screen(const string&  seqid)
{
    bool can_convert = LDS_GetSequenceBase(seqid, &m_SBase, &*m_SeqId);
    if (can_convert) {
        Screen(m_SBase);
    }
}

void CLDS_Query::CSequenceFinder::Screen(const SLDS_SeqIdBase& sbase)
{
    m_Query.ScreenSequence(sbase, &m_CandidateSet, 
                           m_CurInt_idx, m_CurTxt_idx);
}

void CLDS_Query::CSequenceFinder::FindInCandidates(
                                        const vector<string>& seqids, 
                                        CLDS_Set*             obj_ids)
{
    if (m_CandidateSet.any()) {
        CLDS_FindSeqIdFunctor search_func(m_Query.m_db, seqids, obj_ids);

        CLDS_Set::enumerator en(m_CandidateSet.first());
        CLDS_Set::enumerator en_end(m_CandidateSet.end());

        BDB_iterate_file(m_Query.m_db.object_db, en, en_end, search_func);
    }
}

void CLDS_Query::CSequenceFinder::FindInCandidates(const string& seqid, 
                                                   CLDS_Set*     obj_ids)
{
    if (m_CandidateSet.any()) {
        vector<string> seqids(1); // ugly and slow... 
        seqids.push_back(seqid);

        FindInCandidates(seqids, obj_ids);
    }
}


void CLDS_Query::FindSequences(const vector<string>& seqids, 
                               CLDS_Set*             obj_ids)
{
    CSequenceFinder sfinder(*this);

    ITERATE(vector<string>, it, seqids) {
        const string& seqid = *it;
        sfinder.Screen(seqid);
    }

    sfinder.FindInCandidates(seqids, obj_ids);
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


void CLDS_Query::ScreenSequence(const SLDS_SeqIdBase& sbase, 
                                CLDS_Set*             obj_ids)
{
    CBDB_FileCursor cur_int_idx(m_db.obj_seqid_int_idx);
    cur_int_idx.SetCondition(CBDB_FileCursor::eEQ);

    CBDB_FileCursor cur_txt_idx(m_db.obj_seqid_txt_idx);
    cur_txt_idx.SetCondition(CBDB_FileCursor::eEQ);

    ScreenSequence(sbase, obj_ids, cur_int_idx, cur_txt_idx);
}

void CLDS_Query::ScreenSequence(const SLDS_SeqIdBase&    sbase,
                                CLDS_Set*                obj_ids,
                                CBDB_FileCursor&         cur_int_idx,
                                CBDB_FileCursor&         cur_txt_idx)
{
    _ASSERT(obj_ids);

    int rec_id = 0;
    if (sbase.int_id) {
        cur_int_idx.SetCondition(CBDB_FileCursor::eEQ);
        cur_int_idx.From << sbase.int_id;
        while (cur_int_idx.Fetch() == eBDB_Ok) {
            rec_id = m_db.obj_seqid_int_idx.row_id;
            if (rec_id) {
                obj_ids->set(rec_id);
            }
        }
    } else if (!sbase.str_id.empty()) {
        cur_txt_idx.SetCondition(CBDB_FileCursor::eEQ);
        cur_txt_idx.From << sbase.str_id;
        while (cur_txt_idx.Fetch() == eBDB_Ok) {
            rec_id = m_db.obj_seqid_txt_idx.row_id;
            if (rec_id) {
                obj_ids->set(rec_id);
            }
        }
    }
}

CLDS_Query::SObjectDescr 
CLDS_Query::GetObjectDescr(int  id,
                           bool trace_to_top)
{ 
    return GetObjectDescr(m_DataBase.GetObjTypeMap(), id, trace_to_top); 
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

        x_FillDescrObj(&descr, type_map);
        return descr;
    }

    // Check annotations
    //

    m_db.annot_db.annot_id = id;
    if (m_db.annot_db.Fetch() == eBDB_Ok) {
        int top_level_id = m_db.annot_db.TSE_object_id;
        
        if (top_level_id && trace_to_top) {
            // If non-top level entry, call recursively redirected to
            // the top level object
            return GetObjectDescr(type_map, top_level_id, trace_to_top);
        }

        x_FillDescrAnnot(&descr, type_map);
        return descr;
    }

    descr.id = 0; // not found
    return descr;
}

CLDS_Query::SObjectDescr 
CLDS_Query::GetTopSeqEntry(const map<string, int>& type_map, 
                           int id)
{
    SObjectDescr descr;
    int parent_id, tse_id, candidate_id = 0;

    m_db.object_db.object_id = id;
    if (m_db.object_db.Fetch() == eBDB_Ok) {
        parent_id = m_db.object_db.parent_object_id;
        tse_id = m_db.object_db.TSE_object_id;
    } else { // record not found

        // try annotations
        m_db.annot_db.annot_id = id;
        if (m_db.annot_db.Fetch() == eBDB_Ok) {
            parent_id = m_db.annot_db.parent_object_id;
            tse_id = m_db.annot_db.TSE_object_id;
        } else {
            descr.id = -1;
            descr.is_object = false;
            return descr;
        }
    }

    if (parent_id == 0) { // top level         
        x_FillDescrObj(&descr, type_map);
        return descr;
    }

    // scan parents
    while (parent_id) {

        // check objects
        m_db.object_db.object_id = parent_id;
        if (m_db.object_db.Fetch() == eBDB_Ok) {
            int object_type = m_db.object_db.object_type;
            descr.type_str = LDS_TypeMapSearch(type_map, object_type);
            parent_id = m_db.object_db.parent_object_id;

            if (descr.type_str == "Seq-entry") {
                // remember this object as the top possible seq-entry
                candidate_id = m_db.object_db.object_id;
                if (parent_id == 0) {
                    // we found "the true TSE"
                    x_FillDescrObj(&descr, type_map);
                    return descr;
                }
            }
        } else {
            // try annotation
            m_db.annot_db.annot_id = id;
            if (m_db.annot_db.Fetch() == eBDB_Ok) {
                // annotation cannot be the TSE, so just go to
                // the next parent
                parent_id = m_db.annot_db.parent_object_id;
            } else {
                LDS_THROW(eRecordNotFound, 
                          "Local database cannot trace parent object.");
            }
        }
    } // while

    return GetObjectDescr(type_map, 
                          candidate_id ? candidate_id : tse_id, 
                          false /*dont trace to top*/);

}


void 
CLDS_Query::ReportDuplicateObjectSeqId(const string& seqid, 
                                       int           old_rec_id,
                                       int           new_rec_id)
{
    SObjectDescr old_rec = GetObjectDescr(old_rec_id);
    SObjectDescr new_rec = GetObjectDescr(new_rec_id);

    string err_msg = 
        "Duplicate sequence id '" + seqid + "'.";
    err_msg.append(" Conflicting files: " + old_rec.file_name);
    err_msg.append("  " + new_rec.file_name);

    ERR_POST_X(1, err_msg);

    LDS_THROW(eDuplicateId, err_msg);
}


void CLDS_Query::x_FillDescrObj(SObjectDescr* descr,
                                const map<string, int>& type_map)
{
    descr->id = m_db.object_db.object_id;
    descr->is_object = true;
    int object_type = m_db.object_db.object_type;
    descr->type_str = LDS_TypeMapSearch(type_map, object_type);

    int file_id = m_db.object_db.file_id;
    m_db.file_db.file_id = file_id;
    if (m_db.file_db.Fetch() != eBDB_Ok) {
        LDS_THROW(eRecordNotFound, "File record not found.");
    }
    descr->format = (CFormatGuess::EFormat)(int)m_db.file_db.format;
    descr->file_name = m_db.file_db.file_name;
    Int8 pos = m_db.object_db.file_pos;
    descr->pos = NcbiInt8ToStreampos(pos);
    descr->title = m_db.object_db.object_title;
/*
        m_db.object_attr_db.object_attr_id = id;
        if (m_db.object_attr_db.Fetch() == eBDB_Ok) {
            descr.title = m_db.object_attr_db.object_title;
        }
*/
}

void CLDS_Query::x_FillDescrAnnot(SObjectDescr* descr,
                                  const map<string, int>& type_map)
{
    descr->id = m_db.annot_db.annot_id;
    descr->is_object = false;

    int object_type = m_db.annot_db.annot_type;
    descr->type_str = LDS_TypeMapSearch(type_map, object_type);

    int file_id = m_db.annot_db.file_id;

    m_db.file_db.file_id = file_id;
    if (m_db.file_db.Fetch() != eBDB_Ok) {
        LDS_THROW(eRecordNotFound, "File record not found.");
    }

    descr->format    = (CFormatGuess::EFormat)(int)m_db.file_db.format;
    descr->file_name = m_db.file_db.file_name;
    Int8 pos = m_db.annot_db.file_pos;
    descr->pos = NcbiInt8ToStreampos(pos);
}


END_SCOPE(objects)
END_NCBI_SCOPE
