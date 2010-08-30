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
 * Author: Aleksey Grichenko
 *
 * File Description:  LDS v.2 database access.
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbifile.hpp>
#include <db/sqlite/sqlitewrapp.hpp>
#include <objtools/error_codes.hpp>
#include <objtools/lds2/lds2_expt.hpp>
#include <objtools/lds2/lds2_db.hpp>


#define NCBI_USE_ERRCODE_X Objtools_LDS2

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


CLDS2_Database::CLDS2_Database(const string& db_file)
    : m_DbFile(db_file),
      m_DbFlags(CSQLITE_Connection::eDefaultFlags)
{
}


CLDS2_Database::~CLDS2_Database(void)
{
}


DEFINE_STATIC_FAST_MUTEX(sx_LDS2_DB_Lock);
#define LDS2_DB_GUARD() CFastMutexGuard guard(sx_LDS2_DB_Lock)


CSQLITE_Connection& CLDS2_Database::x_GetConn(void) const
{
    if ( !m_Conn.get() ) {
        LDS2_DB_GUARD();
        if ( !m_Conn.get() ) {
            if ( m_DbFile.empty() ) {
                LDS2_THROW(eInvalidDbFile, "Empty database file name.");
            }
            m_Conn.reset(new CSQLITE_Connection(m_DbFile,
                CSQLITE_Connection::eDefaultFlags |
                CSQLITE_Connection::fVacuumManual |
                CSQLITE_Connection::fJournalMemory |
                CSQLITE_Connection::fSyncOff
                ));
        }
    }
    return *m_Conn;
}


// Create db queries
// NOTE: The version of sqlite3 used by the toolkit does not support
// foreign keys. For this reason triggers are used to cascade
// deletions.
const char* kLDS2_CreateDB[] = {
    // files
    "create table `file` (" \
    "`file_id` integer primary key on conflict abort autoincrement," \
    "`file_name` varchar(2048) not null unique on conflict abort," \
    "`file_format` integer not null," \
    "`file_handler` varchar(100)," \
    "`file_size` integer(8)," \
    "`file_time` integer(8)," \
    "`file_crc` integer(4));",
    // index files by name, prevents duplicate names
    "create unique index `idx_filename` on `file` (`file_name`);",

    // chunks
    "create table `chunk` (" \
    "`file_id` integer(8) not null," \
    "`raw_pos` integer(8) not null," \
    "`stream_pos` integer(8) not null," \
    "`data_size` inteter not null," \
    "`data` blob default null);",
    // index by stream_pos
    "create index `idx_stream_pos` on `chunk` (`stream_pos`);",

    // seq-id vs lds-id
    "create table `seq_id` (" \
    "`lds_id` integer primary key on conflict abort autoincrement," \
    "`txt_id` varchar(100) not null," \
    "`int_id` integer(8) default null);",
    // index for reverse lookup, prevents duplicate ids
    "create unique index `idx_int_id` on `seq_id` (`int_id`);",
    "create unique index `idx_txt_id` on `seq_id` (`txt_id`);",

    // bioseqs by blob
    "create table `bioseq` (" \
    "`bioseq_id` integer primary key on conflict abort autoincrement," \
    "`blob_id` integer(8) not null);",
    // reverse index
    "create index `idx_blob_id` on `bioseq` (`blob_id`);",

    // bioseqs by lds_id
    "create table `bioseq_id` (" \
    "`bioseq_id` integer(8) not null," \
    "`lds_id` integer(8) not null);",
    // index by bioseq_id
    "create index `idx_bioseq_id` on `bioseq_id` (`bioseq_id`);",
    // index by lds_id
    "create index `idx_bioseq_lds_id` on `bioseq_id` (`lds_id`);",

    // blobs
    "create table `blob` (" \
    "`blob_id` integer primary key on conflict abort autoincrement," \
    "`blob_type` integer not null," \
    "`file_id` integer(8) not null," \
    "`file_pos` integer(8));",
    // index by file_id
    "create index `idx_blob_file_id` on `blob` (`file_id`);",

    // annotations
    "create table `annot` (" \
    "`annot_id` integer primary key on conflict abort autoincrement," \
    "`annot_type` integer not null," \
    "`blob_id` integer(8) not null);",
    // index by blob_id
    "create index `idx_annot_blob_id` on `annot` (`blob_id`);",

    // annotations by lds_id
    "create table `annot_id` (" \
    "`annot_id` integer(8) not null," \
    "`lds_id` integer(8) not null," \
    "`external` boolean);",
    // index by annot_id
    "create index `idx_annot_id` on `annot_id` (`annot_id`);",
    // index by lds_id
    "create index `idx_annot_lds_id` on `annot_id` (`lds_id`);",
    // index 'external' column
    "create index `idx_external` on `annot_id` (`external`);",

    // delete files - cascade to blobls and chunks
    "create trigger `delete_file` before delete on `file` begin " \
    "delete from `blob` where `blob`.`file_id`=`old`.`file_id`;" \
    "delete from `chunk` where `chunk`.`file_id`=`old`.`file_id`;" \
    "end;",
    // delete bioseqs - cascade to bioseq_id
    "create trigger `delete_bioseq` before delete on `bioseq` begin " \
    "delete from `bioseq_id` where `bioseq_id`.`bioseq_id`=`old`.`bioseq_id`;" \
    "end;",
    // delete blobs - cascade to bioseq and annot
    "create trigger `delete_blob` before delete on `blob` begin " \
    "delete from `bioseq` where `bioseq`.`blob_id`=`old`.`blob_id`;" \
    "delete from `annot` where `annot`.`blob_id`=`old`.`blob_id`;" \
    "end;",
    // delete annotations - cascade to annot_id
    "create trigger `delete_annot` before delete on `annot` begin " \
    "delete from `annot_id` where `annot_id`.`annot_id`=`old`.`annot_id`;" \
    "end;"
};


void CLDS2_Database::Create(void)
{
    LOG_POST_X(1, Info << "LDS2: Creating database " <<  m_DbFile);

    // Close the connection if any
    m_Conn.reset();

    // Delete the file
    CFile dbf(m_DbFile);
    if ( dbf.Exists() ) {
        dbf.Remove();
    }

    // Create new db file and open connection
    CSQLITE_Connection& conn = x_GetConn();

    // Create tables:

    for (size_t i = 0; i < sizeof(kLDS2_CreateDB)/sizeof(kLDS2_CreateDB[0]); i++) {
        conn.ExecuteSql(kLDS2_CreateDB[i]);
    }
}


void CLDS2_Database::SetSQLiteFlags(int flags)
{
    m_DbFlags = flags;
    m_Conn.reset();
}


void CLDS2_Database::Open(void)
{
    x_GetConn();
}


void CLDS2_Database::GetFileNames(TStringSet& files) const
{
    CSQLITE_Connection& conn = x_GetConn();
    CSQLITE_Statement st(&conn, "select `file_name` from `file`;");
    while ( st.Step() ) {
        files.insert(st.GetString(0));
    }
}


SLDS2_File CLDS2_Database::GetFileInfo(const string& file_name) const
{
    SLDS2_File info(file_name);

    CSQLITE_Connection& conn = x_GetConn();
    CSQLITE_Statement st(&conn,
        "select `file_id`, `file_format`, `file_handler`, `file_size`, " \
        "`file_time`, `file_crc` from `file` where `file_name`=?1;");
    st.Bind(1, file_name);
    if ( st.Step() ) {
        info.id = st.GetInt8(0);
        info.format = SLDS2_File::TFormat(st.GetInt(1));
        info.handler = st.GetString(2);
        info.size = st.GetInt8(3);
        info.time = st.GetInt8(4);
        info.crc = st.GetInt(5);
    }
    return info;
}


void CLDS2_Database::AddFile(SLDS2_File& info)
{
    LOG_POST_X(2, Info << "LDS2: Adding file " << info.name);
    CSQLITE_Connection& conn = x_GetConn();
    CSQLITE_Statement st(&conn,
        "insert into `file` " \
        "(`file_name`, `file_format`, `file_handler`, `file_size`, " \
        "`file_time`, `file_crc`) values (?1, ?2, ?3, ?4, ?5, ?6);");
    st.Bind(1, info.name);
    st.Bind(2, info.format);
    st.Bind(3, info.handler);
    st.Bind(4, info.size);
    st.Bind(5, info.time);
    st.Bind(6, info.crc);
    st.Execute();
    info.id = st.GetLastInsertedRowid();
}


void CLDS2_Database::UpdateFile(SLDS2_File& info)
{
    LOG_POST_X(3, Info << "LDS2: Updating file " << info.name);
    DeleteFile(info.id);
    AddFile(info);
}


void CLDS2_Database::DeleteFile(const string& file_name)
{
    LOG_POST_X(4, Info << "LDS2: Deleting file " << file_name);
    CSQLITE_Connection& conn = x_GetConn();
    CSQLITE_Statement st(&conn, "delete from `file` where `file_name`=?1;");
    st.Bind(1, file_name);
    st.Execute();
}


void CLDS2_Database::DeleteFile(Int8 file_id)
{
    LOG_POST_X(4, Info << "LDS2: Deleting file " << file_id);
    CSQLITE_Connection& conn = x_GetConn();
    CSQLITE_Statement st(&conn, "delete from `file` where `file_id`=?1;");
    st.Bind(1, file_id);
    st.Execute();
}


Int8 CLDS2_Database::x_GetLdsSeqId(const CSeq_id_Handle& id)
{
    CSQLITE_Connection& conn = x_GetConn();
    CSQLITE_Statement st(&conn);
    if ( id.IsGi() ) {
        // Try to use integer index
        st.SetSql(
            "select `lds_id` from `seq_id` where `int_id`=?1;");
        st.Bind(1, id.GetGi());
    }
    else {
        // Use text index
        st.SetSql(
            "select `lds_id` from `seq_id` where `txt_id`=?1;");
        st.Bind(1, id.AsString());
    }
    if ( st.Step() ) {
        return st.GetInt8(0);
    }

    // Id not in the database yet -- add new entry.
    st.SetSql(
        "insert into `seq_id` (`txt_id`, `int_id`) " \
        "values (?1, ?2);");
    st.Bind(1, id.AsString());
    if ( id.IsGi() ) {
        st.Bind(2, id.GetGi());
    }
    st.Execute();
    return st.GetLastInsertedRowid();
}


Int8 CLDS2_Database::AddBlob(Int8                   file_id,
                             SLDS2_Blob::EBlobType  blob_type,
                             Int8                   file_pos)
{
    CSQLITE_Connection& conn = x_GetConn();
    CSQLITE_Statement st(&conn,
        "insert into `blob` " \
        "(`blob_type`, `file_id`, `file_pos`) " \
        "values (?1, ?2, ?3);");
    st.Bind(1, blob_type);
    st.Bind(2, file_id);
    st.Bind(3, file_pos);
    st.Execute();
    Int8 blob_id = st.GetLastInsertedRowid();
    return blob_id;
}


typedef vector<Int8> TLdsIds;


Int8 CLDS2_Database::AddBioseq(Int8 blob_id, const TSeqIdSet& ids)
{
    // Convert ids to lds-ids
    TLdsIds lds_ids;
    ITERATE(TSeqIdSet, id, ids) {
        Int8 lds_id = x_GetLdsSeqId(*id);
        lds_ids.push_back(lds_id);
    }

    CSQLITE_Connection& conn = x_GetConn();
    CSQLITE_Statement st(&conn);

    // Create new bioseq
    st.SetSql("insert into `bioseq` (`blob_id`) values (?1);");
    st.Bind(1, blob_id);
    st.Execute();
    Int8 bioseq_id = st.GetLastInsertedRowid();

    // Link bioseq to its seq-ids
    st.SetSql(
        "insert into `bioseq_id` " \
        "(`bioseq_id`, `lds_id`) " \
        "values (?1, ?2);");
    ITERATE(TLdsIds, lds_id, lds_ids) {
        st.Bind(1, bioseq_id);
        st.Bind(2, *lds_id);
        st.Execute();
        st.Reset();
    }

    return bioseq_id;
}


Int8 CLDS2_Database::AddAnnot(Int8                      blob_id,
                              SLDS2_Annot::EAnnotType   annot_type,
                              const TAnnotRefSet&       refs)
{
    // Convert ids to lds-ids
    typedef pair<Int8, bool> TLdsAnnotRef;
    typedef vector<TLdsAnnotRef> TLdsAnnotRefSet;

    TLdsAnnotRefSet lds_refs;
    ITERATE(TAnnotRefSet, ref, refs) {
        Int8 lds_id = x_GetLdsSeqId(ref->first);
        lds_refs.push_back(TLdsAnnotRef(lds_id, ref->second));
    }

    CSQLITE_Connection& conn = x_GetConn();
    CSQLITE_Statement st(&conn);

    // Create new bioseq
    st.SetSql(
        "insert into `annot` " \
        "(`annot_type`, `blob_id`) " \
        "values (?1, ?2);");
    st.Bind(1, annot_type);
    st.Bind(2, blob_id);
    st.Execute();
    Int8 annot_id = st.GetLastInsertedRowid();

    // Link annot to its seq-ids
    st.SetSql(
        "insert into `annot_id` " \
        "(`annot_id`, `lds_id`, `external`) " \
        "values (?1, ?2, ?3);");
    ITERATE(TLdsAnnotRefSet, ref, lds_refs) {
        st.Bind(1, annot_id);
        st.Bind(2, ref->first);
        st.Bind(3, ref->second);
        st.Execute();
        st.Reset();
    }

    return annot_id;
}


Int8 CLDS2_Database::GetBioseqId(const CSeq_id_Handle& idh) const
{
    CSQLITE_Connection& conn = x_GetConn();
    CSQLITE_Statement st(&conn);
    if ( idh.IsGi() ) {
        st.SetSql(
            "select `bioseq_id`.`bioseq_id` from `seq_id` " \
            "inner join `bioseq_id` using(`lds_id`) " \
            "where `seq_id`.`int_id`=?1;");
        st.Bind(1, idh.GetGi());
    }
    else {
        st.SetSql(
            "select `bioseq_id`.`bioseq_id` from `seq_id` " \
            "inner join `bioseq_id` using(`lds_id`) " \
            "where `seq_id`.`txt_id`=?1;");
        st.Bind(1, idh.AsString());
    }
    Int8 bioseq_id = 0;
    if ( !st.Step() )
    {
        return 0;
    }
    bioseq_id = st.GetInt8(0);
    // Reset on conflict
    if ( st.Step() ) {
        bioseq_id = -1;
    }
    return bioseq_id;
}


void CLDS2_Database::GetSynonyms(const CSeq_id_Handle& idh, TLdsIdSet& ids)
{
    Int8 bioseq_id = GetBioseqId(idh);
    if (bioseq_id <= 0) {
        // No such id or conflict
        return;
    }
    CSQLITE_Connection& conn = x_GetConn();
    CSQLITE_Statement st(&conn,
        "select distinct `lds_id` from `bioseq_id` " \
        "where `bioseq_id`=?1;");
    st.Bind(1, bioseq_id);
    while ( st.Step() ) {
        ids.insert(st.GetInt8(0));
    }
}


void CLDS2_Database::x_InitGetBioseqsSql(const CSeq_id_Handle& idh,
                                         CSQLITE_Statement&    st) const
{
    string sql =
        "select distinct `blob_id`, `blob_type`, `file_id`, `file_pos` " \
        "from `blob` " \
        "inner join `bioseq` using(`blob_id`) " \
        "inner join `bioseq_id` using(`bioseq_id`) " \
        "inner join `seq_id` using(`lds_id`) " \
        "where ";
    if ( idh.IsGi() ) {
        st.SetSql(sql + "`seq_id`.`int_id`=?1;");
        st.Bind(1, idh.GetGi());
    }
    else {
        st.SetSql(sql + "`seq_id`.`txt_id`=?1;");
        st.Bind(1, idh.AsString());
    }
}


SLDS2_Blob CLDS2_Database::GetBlobInfo(const CSeq_id_Handle& idh)
{
    CSQLITE_Connection& conn = x_GetConn();
    CSQLITE_Statement st(&conn);
    x_InitGetBioseqsSql(idh, st);
    if ( !st.Step() ) {
        return SLDS2_Blob();
    }
    SLDS2_Blob info;
    info.id = st.GetInt8(0);
    info.type = SLDS2_Blob::EBlobType(st.GetInt(1));
    info.file_id = st.GetInt8(2);
    info.file_pos = st.GetInt8(3);
    if ( st.Step() ) {
        // Conflict - return empty blob
        return SLDS2_Blob();
    }
    return info;
}


SLDS2_Blob CLDS2_Database::GetBlobInfo(Int8 blob_id)
{
    SLDS2_Blob info;
    if (blob_id <= 0) {
        return info;
    }
    CSQLITE_Connection& conn = x_GetConn();
    CSQLITE_Statement st(&conn,
        "select distinct `blob_id`, `blob_type`, `file_id`, `file_pos` " \
        "from `blob` where `blob_id`=?1;");
    st.Bind(1, blob_id);
    if ( !st.Step() ) {
        return info;
    }
    info.id = st.GetInt8(0);
    info.type = SLDS2_Blob::EBlobType(st.GetInt(1));
    info.file_id = st.GetInt8(2);
    info.file_pos = st.GetInt8(3);
    _ASSERT( !st.Step() );
    return info;
}


SLDS2_File CLDS2_Database::GetFileInfo(Int8 file_id)
{
    SLDS2_File info;
    if (file_id <= 0) {
        return info;
    }
    CSQLITE_Connection& conn = x_GetConn();
    CSQLITE_Statement st(&conn,
        "select `file_name`, `file_format`, `file_handler`, `file_size`, " \
        "`file_time`, `file_crc` from `file` where `file_id`=?1;");
    st.Bind(1, file_id);
    if ( st.Step() ) {
        info.id = file_id;
        info.name = st.GetString(0);
        info.format = SLDS2_File::TFormat(st.GetInt(1));
        info.handler = st.GetString(2);
        info.size = st.GetInt8(3);
        info.time = st.GetInt8(4);
        info.crc = st.GetInt(5);
    }
    return info;
}


void CLDS2_Database::GetBioseqBlobs(const CSeq_id_Handle& idh,
                                    TBlobSet&             blobs)
{
    CSQLITE_Connection& conn = x_GetConn();
    CSQLITE_Statement st(&conn);
    x_InitGetBioseqsSql(idh, st);
    while ( st.Step() ) {
        SLDS2_Blob info;
        info.id = st.GetInt8(0);
        info.type = SLDS2_Blob::EBlobType(st.GetInt(1));
        info.file_id = st.GetInt8(2);
        info.file_pos = st.GetInt8(3);
        blobs.push_back(info);
    }
}


void CLDS2_Database::GetAnnotBlobs(const CSeq_id_Handle& idh,
                                   TAnnotChoice          choice,
                                   TBlobSet&             blobs)
{
    CSQLITE_Connection& conn = x_GetConn();
    CSQLITE_Statement st(&conn);

    string sql =
        "select distinct `blob_id`, `blob_type`, `file_id`, `file_pos` " \
        "from `blob` " \
        "inner join `annot` using(`blob_id`) " \
        "inner join `annot_id` using(`annot_id`) " \
        "inner join `seq_id` using(`lds_id`) " \
        "where ";

    if (choice != fAnnot_All) {
        sql += "`annot_id`.`external`=?2 and ";
    }

    if ( idh.IsGi() ) {
        st.SetSql(sql + "`seq_id`.`int_id`=?1;");
        st.Bind(1, idh.GetGi());
    }
    else {
        st.SetSql(sql + "`seq_id`.`txt_id`=?1;");
        st.Bind(1, idh.AsString());
    }

    if (choice != fAnnot_All) {
        st.Bind(2, (choice & fAnnot_External) != 0);
    }

    while ( st.Step() ) {
        SLDS2_Blob info;
        info.id = st.GetInt8(0);
        info.type = SLDS2_Blob::EBlobType(st.GetInt(1));
        info.file_id = st.GetInt8(2);
        info.file_pos = st.GetInt8(3);
        blobs.push_back(info);
    }
}


void CLDS2_Database::AddChunk(const SLDS2_File&  file_info,
                              const SLDS2_Chunk& chunk_info)
{
    CSQLITE_Connection& conn = x_GetConn();
    CSQLITE_Statement st(&conn,
        "insert into `chunk` " \
        "(`file_id`, `raw_pos`, `stream_pos`, `data_size`, `data`) " \
        "values (?1, ?2, ?3, ?4, ?5);");
    st.Bind(1, file_info.id);
    st.Bind(2, chunk_info.raw_pos);
    st.Bind(3, chunk_info.stream_pos);
    st.Bind(4, chunk_info.data_size);
    st.Bind(5, chunk_info.data, chunk_info.data_size);
    st.Execute();
}


bool CLDS2_Database::FindChunk(const SLDS2_File& file_info,
                               SLDS2_Chunk&      chunk_info,
                               Int8              stream_pos)
{
    CSQLITE_Connection& conn = x_GetConn();
    CSQLITE_Statement st(&conn,
        "select `raw_pos`, `stream_pos`, `data_size`, `data` from `chunk` " \
        "where `file_id`=?1 and `stream_pos`<=?2 order by `stream_pos` desc " \
        "limit 1;");
    st.Bind(1, file_info.id);
    st.Bind(2, stream_pos);
    if ( !st.Step() ) return false;
    chunk_info.raw_pos = st.GetInt8(0);
    chunk_info.stream_pos = st.GetInt8(1);
    chunk_info.DeleteData(); // just in case someone reuses the same object
    chunk_info.data_size = st.GetInt(2);
    if ( chunk_info.data_size ) {
        chunk_info.InitData(chunk_info.data_size);
        chunk_info.data_size =
            st.GetBlob(3, chunk_info.data, chunk_info.data_size);
    }
    return true;
}


void CLDS2_Database::Analyze(void)
{
    CSQLITE_Connection& conn = x_GetConn();
    CSQLITE_Statement st(&conn, "analyze;");
    st.Execute();
}


END_SCOPE(objects)
END_NCBI_SCOPE
