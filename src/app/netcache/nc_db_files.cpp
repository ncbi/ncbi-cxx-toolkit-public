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
 * Author: Pavel Ivanov
 */

#include <ncbi_pch.hpp>

#include "nc_db_files.hpp"
#include "nc_utils.hpp"
#include "nc_db_stat.hpp"


BEGIN_NCBI_SCOPE


static const char* kNCBlobKeys_Table          = "NCB";
static const char* kNCBlobKeys_BlobIdCol      = "id";
static const char* kNCBlobKeys_KeyCol         = "key";
static const char* kNCBlobKeys_SubkeyCol      = "skey";
static const char* kNCBlobKeys_VersionCol     = "ver";

static const char* kNCBlobInfo_Table          = "NCI";
static const char* kNCBlobInfo_BlobIdCol      = "id";
static const char* kNCBlobInfo_AccessTimeCol  = "at";
static const char* kNCBlobInfo_DeadTimeCol    = "dt";
static const char* kNCBlobInfo_OwnerCol       = "own";
static const char* kNCBlobInfo_TTLCol         = "ttl";
static const char* kNCBlobInfo_SizeCol        = "sz";

static const char* kNCBlobChunks_Table        = "NCC";
static const char* kNCBlobChunks_ChunkIdCol   = "id";
static const char* kNCBlobChunks_BlobIdCol    = "bid";

static const char* kNCBlobData_Table          = "NCD";
static const char* kNCBlobData_ChunkIdCol     = "id";
static const char* kNCBlobData_BlobCol        = "blob";

static const char* kNCDBIndex_Table           = "NCN";
static const char* kNCDBIndex_DBIdCol         = "id";
static const char* kNCDBIndex_MetaNameCol     = "met";
static const char* kNCDBIndex_DataNameCol     = "dat";
static const char* kNCDBIndex_CreatedTimeCol  = "tm";
static const char* kNCDBIndex_MinBlobIdCol    = "bid";



void
CNCDBFile::CreateIndexDatabase(void)
{
    CQuickStrStream sql;
    CSQLITE_Statement  stmt(this);

    sql.Clear();
    sql << "create table if not exists " << kNCDBIndex_Table
        << "(" << kNCDBIndex_DBIdCol        << " integer primary key,"
               << kNCDBIndex_MetaNameCol    << " varchar not null,"
               << kNCDBIndex_DataNameCol    << " varchar not null,"
               << kNCDBIndex_CreatedTimeCol << " int not null,"
               << kNCDBIndex_MinBlobIdCol   << " int64 not null default 0"
        << ")";
    stmt.SetSql(sql);
    stmt.Execute();
}

void
CNCDBFile::CreateMetaDatabase(void)
{
    CQuickStrStream sql;
    CSQLITE_Statement stmt(this);
    CNCStatTimer timer(GetStat(), CNCDBStat::eDbOther);

    sql.Clear();
    sql << "create table " << kNCBlobKeys_Table
        << "(" << kNCBlobKeys_BlobIdCol  << " integer primary key,"
               << kNCBlobKeys_KeyCol     << " varchar not null,"
               << kNCBlobKeys_SubkeyCol  << " varchar not null,"
               << kNCBlobKeys_VersionCol << " int not null,"
               << "unique(" << kNCBlobKeys_KeyCol    << ","
                            << kNCBlobKeys_SubkeyCol << ","
                            << kNCBlobKeys_VersionCol /* << ","
               // id should not be in the unique because we rely on it if we
               // need to change id for the blob when it is re-created.
                            << kNCBlobKeys_BlobIdCol */
               <<       ")"
        << ")";
    stmt.SetSql(sql);
    stmt.Execute();

    sql.Clear();
    sql << "create table " << kNCBlobInfo_Table
        << "(" << kNCBlobInfo_BlobIdCol     << " integer primary key,"
               << kNCBlobInfo_AccessTimeCol << " int not null,"
               << kNCBlobInfo_DeadTimeCol   << " int not null,"
               << kNCBlobInfo_OwnerCol      << " varchar not null,"
               << kNCBlobInfo_TTLCol        << " int not null,"
               << kNCBlobInfo_SizeCol       << " int64 not null,"
               << "unique(" << kNCBlobInfo_DeadTimeCol << ","
                            << kNCBlobInfo_BlobIdCol
               <<       ")"
        << ")";
    stmt.SetSql(sql);
    stmt.Execute();

    sql.Clear();
    sql << "create table " << kNCBlobChunks_Table
        << "(" << kNCBlobChunks_ChunkIdCol << " integer primary key,"
               << kNCBlobChunks_BlobIdCol  << " integer not null,"
               << "unique(" << kNCBlobChunks_BlobIdCol << ","
                            << kNCBlobChunks_ChunkIdCol
               <<       ")"
        << ")";
    stmt.SetSql(sql);
    stmt.Execute();
}

void
CNCDBFile::CreateDataDatabase(void)
{
    CQuickStrStream sql;
    CSQLITE_Statement  stmt(this);
    CNCStatTimer timer(GetStat(), CNCDBStat::eDbOther);

    sql.Clear();
    sql << "create table " << kNCBlobData_Table
        << "(" << kNCBlobData_ChunkIdCol << " integer primary key,"
               << kNCBlobData_BlobCol    << " blob not null"
           ")";
    stmt.SetSql(sql);
    stmt.Execute();
}

CSQLITE_Statement*
CNCDBFile::x_GetStatement(ENCStmtType typ)
{
    TStatementPtr& stmt = m_Stmts[typ];
    if (!stmt) {
        CQuickStrStream sql;

        switch (typ) {
        case eStmt_CreateDBPart:
            sql << "insert into " << kNCDBIndex_Table
                << "(" << kNCDBIndex_DBIdCol        << ","
                       << kNCDBIndex_MetaNameCol    << ","
                       << kNCDBIndex_DataNameCol    << ","
                       << kNCDBIndex_CreatedTimeCol
                << ")values(?1,?2,?3,?4)";
            break;
        case eStmt_SetDBPartBlobId:
            sql << "update " << kNCDBIndex_Table
                <<   " set " << kNCDBIndex_MinBlobIdCol << "=?2"
                << " where " << kNCDBIndex_DBIdCol << "=?1";
            break;
        case eStmt_SetDBPartCreated:
            sql << "update " << kNCDBIndex_Table
                <<   " set " << kNCDBIndex_CreatedTimeCol << "=?2"
                << " where " << kNCDBIndex_DBIdCol << "=?1";
            break;
        case eStmt_DeleteDBPart:
            sql << "delete from " << kNCDBIndex_Table
                << " where " << kNCDBIndex_DBIdCol << "=?1";
            break;
        case eStmt_GetBlobId:
            sql << "select a." << kNCBlobKeys_BlobIdCol
                <<  " from " << kNCBlobKeys_Table << " a,"
                             << kNCBlobInfo_Table << " b"
                << " where a." << kNCBlobKeys_KeyCol     << "=?1"
                <<   " and a." << kNCBlobKeys_SubkeyCol  << "=?2"
                <<   " and a." << kNCBlobKeys_VersionCol << "=?3"
                <<   " and b." << kNCBlobInfo_BlobIdCol
                <<                             "=a." << kNCBlobKeys_BlobIdCol
                <<   " and b." << kNCBlobInfo_DeadTimeCol << ">=?4";
            break;
        case eStmt_GetKeyIds:
            sql << "select a." << kNCBlobKeys_BlobIdCol
                <<  " from " << kNCBlobKeys_Table << " a,"
                             << kNCBlobInfo_Table << " b"
                << " where a." << kNCBlobKeys_KeyCol << "=?1"
                <<   " and b." << kNCBlobInfo_BlobIdCol
                <<                             "=a." << kNCBlobKeys_BlobIdCol
                <<   " and b." << kNCBlobInfo_DeadTimeCol << ">=?2";
            break;
        case eStmt_BlobExists:
            sql << "select count(*)"
                <<  " from " << kNCBlobKeys_Table << " a,"
                             << kNCBlobInfo_Table << " b"
                << " where a." << kNCBlobKeys_KeyCol    << "=?1"
                <<   " and a." << kNCBlobKeys_SubkeyCol << "=?2"
                <<   " and b." << kNCBlobInfo_BlobIdCol
                <<                             "=a." << kNCBlobKeys_BlobIdCol
                <<   " and b." << kNCBlobInfo_DeadTimeCol << ">=?3";
            break;
        case eStmt_CreateBlobKey:
            // Replace - in case if blob was recently deleted and now it's
            // created again when record in database is still exists.
            sql << "insert or replace into " << kNCBlobKeys_Table
                << "(" << kNCBlobKeys_BlobIdCol << ","
                       << kNCBlobKeys_KeyCol    << ","
                       << kNCBlobKeys_SubkeyCol << ","
                       << kNCBlobKeys_VersionCol
                << ")values(?1,?2,?3,?4)";
            break;
        case eStmt_ReadBlobKey:
            sql << "select a." << kNCBlobKeys_KeyCol    << ","
                <<        "a." << kNCBlobKeys_SubkeyCol << ","
                <<        "a." << kNCBlobKeys_VersionCol
                <<  " from " << kNCBlobKeys_Table << " a,"
                             << kNCBlobInfo_Table << " b"
                << " where a." << kNCBlobKeys_BlobIdCol << "=?1"
                <<   " and b." << kNCBlobInfo_BlobIdCol
                <<                             "=a." << kNCBlobKeys_BlobIdCol
                <<   " and b." << kNCBlobInfo_DeadTimeCol << ">=?2";
            break;
        case eStmt_GetBlobIdsList:
            sql << "select " << kNCBlobInfo_BlobIdCol << ","
                             << kNCBlobInfo_DeadTimeCol
                <<  " from " << kNCBlobInfo_Table
                << " where " << kNCBlobInfo_DeadTimeCol << ">=?1"
                <<   " and " << kNCBlobInfo_DeadTimeCol << "<?3"
                <<   " and (" << kNCBlobInfo_DeadTimeCol << ">?1"
                <<        " or " << kNCBlobInfo_BlobIdCol << ">?2)"
                << " order by 2,1"
                << " limit ?4";
            break;
        case eStmt_WriteBlobInfo:
            sql << "insert or replace into " << kNCBlobInfo_Table
                << "(" << kNCBlobInfo_BlobIdCol     << ","
                       << kNCBlobInfo_AccessTimeCol << ","
                       << kNCBlobInfo_DeadTimeCol   << ","
                       << kNCBlobInfo_OwnerCol      << ","
                       << kNCBlobInfo_TTLCol        << ","
                       << kNCBlobInfo_SizeCol
                << ")values(?1,?2,?3,?4,?5,?6)";
            break;
        case eStmt_SetBlobDeadTime:
            sql << "update " << kNCBlobInfo_Table
                <<   " set " << kNCBlobInfo_DeadTimeCol << "=?2"
                << " where " << kNCBlobInfo_BlobIdCol << "=?1";
            break;
        case eStmt_ReadBlobInfo:
            sql << "select " << kNCBlobInfo_AccessTimeCol << ","
                             << kNCBlobInfo_DeadTimeCol   << ","
                             << kNCBlobInfo_OwnerCol      << ","
                             << kNCBlobInfo_TTLCol        << ","
                             << kNCBlobInfo_SizeCol
                <<  " from " << kNCBlobInfo_Table
                << " where " << kNCBlobInfo_BlobIdCol << "=?1";
            break;
        case eStmt_GetChunkIds:
            sql << "select id from " << kNCBlobChunks_Table
                << " where " << kNCBlobChunks_BlobIdCol << "=?1"
                << " order by id";
            break;
        case eStmt_CreateChunk:
            sql << "insert or replace into " << kNCBlobChunks_Table
                << "(" << kNCBlobChunks_ChunkIdCol << ","
                       << kNCBlobChunks_BlobIdCol
                << ")values(?1,?2)";
            break;
        case eStmt_DeleteLastChunks:
            sql << "delete from " << kNCBlobChunks_Table
                << " where " << kNCBlobChunks_BlobIdCol  << "=?1"
                <<   " and " << kNCBlobChunks_ChunkIdCol << ">=?2";
            break;
        case eStmt_CreateChunkData:
            sql << "insert into " << kNCBlobData_Table
                << "(" << kNCBlobData_BlobCol
                << ")values(?1)";
            break;
        case eStmt_WriteChunkData:
            sql << "update " << kNCBlobData_Table
                <<   " set " << kNCBlobData_BlobCol    << "=?2"
                << " where " << kNCBlobData_ChunkIdCol << "=?1";
            break;
        case eStmt_ReadChunkData:
            sql << "select " << kNCBlobData_BlobCol
                <<  " from " << kNCBlobData_Table
                << " where " << kNCBlobData_ChunkIdCol << "=?1";
            break;
        default:
            NCBI_TROUBLE("Unknown statement type");
        }

        {{
            CNCStatTimer timer(GetStat(), CNCDBStat::eDbOther);
            stmt = new CSQLITE_Statement(this, sql);
        }}
    }
    return stmt.get();
}

void
CNCDBFile::CreateDBPart(SNCDBPartInfo* part_info)
{
    CSQLITE_StatementLock stmt(x_GetStatement(eStmt_CreateDBPart));
    CNCStatTimer timer(GetStat(), CNCDBStat::eDbOther);

    part_info->create_time = int(time(NULL));
    stmt->Bind(1, part_info->part_id);
    stmt->Bind(2, part_info->meta_file.data(), part_info->meta_file.size());
    stmt->Bind(3, part_info->data_file.data(), part_info->data_file.size());
    stmt->Bind(4, part_info->create_time);
    stmt->Execute();
}

void
CNCDBFile::SetDBPartMinBlobId(TNCDBPartId part_id, TNCBlobId blob_id)
{
    CSQLITE_StatementLock stmt(x_GetStatement(eStmt_SetDBPartBlobId));
    CNCStatTimer timer(GetStat(), CNCDBStat::eDbOther);

    stmt->Bind(1, part_id);
    stmt->Bind(2, blob_id);
    stmt->Execute();
}

void
CNCDBFile::UpdateDBPartCreated(SNCDBPartInfo* part_info)
{
    CSQLITE_StatementLock stmt(x_GetStatement(eStmt_SetDBPartCreated));
    CNCStatTimer timer(GetStat(), CNCDBStat::eDbOther);

    part_info->create_time = int(time(NULL));
    stmt->Bind(1, part_info->part_id);
    stmt->Bind(2, part_info->create_time);
    stmt->Execute();
}

void
CNCDBFile::DeleteDBPart(TNCDBPartId part_id)
{
    CSQLITE_StatementLock stmt(x_GetStatement(eStmt_DeleteDBPart));
    CNCStatTimer timer(GetStat(), CNCDBStat::eDbOther);

    stmt->Bind(1, part_id);
    stmt->Execute();
}

void
CNCDBFile::GetAllDBParts(TNCDBPartsList* parts_list)
{
    _ASSERT(parts_list);

    CQuickStrStream sql;
    sql << "select " << kNCDBIndex_DBIdCol        << ","
                     << kNCDBIndex_MetaNameCol    << ","
                     << kNCDBIndex_DataNameCol    << ","
                     << kNCDBIndex_CreatedTimeCol << ","
                     << kNCDBIndex_MinBlobIdCol
        <<  " from " << kNCDBIndex_Table
        << " order by " << kNCDBIndex_CreatedTimeCol;

    CSQLITE_Statement stmt(this, sql);
    while (stmt.Step()) {
        SNCDBPartInfo info;
        info.part_id     = stmt.GetInt8  (0);
        info.meta_file   = stmt.GetString(1);
        info.data_file   = stmt.GetString(2);
        info.create_time = stmt.GetInt   (3);
        info.min_blob_id = stmt.GetInt8  (4);

        parts_list->push_back(info);
    }
}

void
CNCDBFile::DeleteAllDBParts(void)
{
    CQuickStrStream sql;
    sql << "delete from " << kNCDBIndex_Table;

    CSQLITE_Statement stmt(this, sql);
    stmt.Execute();
}

TNCBlobId
CNCDBFile::GetLastBlobId(void)
{
    CQuickStrStream sql;
    sql << "select max(" << kNCBlobKeys_BlobIdCol << ")"
        << " from " << kNCBlobKeys_Table;

    CSQLITE_Statement stmt(this, sql);
    _VERIFY(stmt.Step());
    return stmt.GetInt8(0);
}

bool
CNCDBFile::ReadBlobId(SNCBlobIdentity* identity, int dead_after)
{
    CSQLITE_StatementLock stmt(x_GetStatement(eStmt_GetBlobId));
    CNCStatTimer timer(GetStat(), CNCDBStat::eDbInfoRead);

    stmt->Bind(1, identity->key.data(), identity->key.size());
    stmt->Bind(2, identity->subkey.data(), identity->subkey.size());
    stmt->Bind(3, identity->version);
    stmt->Bind(4, dead_after);
    if (stmt->Step()) {
        identity->blob_id = stmt->GetInt8(0);
        return true;
    }
    return false;
}

void
CNCDBFile::FillBlobIds(const string& key,
                       int           dead_after,
                       TNCIdsList*   id_list)
{
    _ASSERT(id_list);

    CSQLITE_StatementLock stmt(x_GetStatement(eStmt_GetKeyIds));
    CNCStatTimer timer(GetStat(), CNCDBStat::eDbInfoRead);

    stmt->Bind(1, key.data(), key.size());
    stmt->Bind(2, dead_after);
    while (stmt->Step()) {
        id_list->push_back(stmt->GetInt8(0));
    }
}

bool
CNCDBFile::IsBlobExists(const string& key,
                        const string& subkey,
                        int           dead_after)
{
    CSQLITE_StatementLock stmt(x_GetStatement(eStmt_BlobExists));
    CNCStatTimer timer(GetStat(), CNCDBStat::eDbOther);

    stmt->Bind(1, key.data(), key.size());
    stmt->Bind(2, subkey.data(), subkey.size());
    stmt->Bind(3, dead_after);
    _VERIFY(stmt->Step());

    return stmt->GetInt8(0) > 0;
}

void
CNCDBFile::CreateBlobKey(const SNCBlobIdentity& identity)
{
    CSQLITE_StatementLock stmt(x_GetStatement(eStmt_CreateBlobKey));
    CNCStatTimer timer(GetStat(), CNCDBStat::eDbInfoWrite);

    stmt->Bind(1, identity.blob_id);
    stmt->Bind(2, identity.key   .data(), identity.key   .size());
    stmt->Bind(3, identity.subkey.data(), identity.subkey.size());
    stmt->Bind(4, identity.version);

    stmt->Execute();
}

bool
CNCDBFile::ReadBlobKey(SNCBlobIdentity* identity, int dead_after)
{
    CSQLITE_StatementLock stmt(x_GetStatement(eStmt_ReadBlobKey));
    CNCStatTimer timer(GetStat(), CNCDBStat::eDbInfoRead);

    stmt->Bind(1, identity->blob_id);
    stmt->Bind(2, dead_after);
    if (stmt->Step()) {
        identity->key     = stmt->GetString(0);
        identity->subkey  = stmt->GetString(1);
        identity->version = stmt->GetInt   (2);
        return true;
    }
    return false;
}

void
CNCDBFile::GetBlobIdsList(int&        dead_after,
                          TNCBlobId&  id_after,
                          int         dead_before,
                          int         max_count,
                          TNCIdsList* id_list)
{
    _ASSERT(id_list);
    id_list->clear();

    CSQLITE_StatementLock stmt(x_GetStatement(eStmt_GetBlobIdsList));
    CNCStatTimer timer(GetStat(), CNCDBStat::eDbOther);

    stmt->Bind(1, dead_after);
    stmt->Bind(2, id_after);
    stmt->Bind(3, dead_before);
    stmt->Bind(4, max_count);
    while (stmt->Step()) {
        id_after = stmt->GetInt8(0);
        id_list->push_back(id_after);
        dead_after = stmt->GetInt(1);
    }
}

void
CNCDBFile::WriteBlobInfo(const SNCBlobInfo& blob_info)
{
    int cur_time = int(time(NULL));
    CSQLITE_StatementLock stmt(x_GetStatement(eStmt_WriteBlobInfo));
    CNCStatTimer timer(GetStat(), CNCDBStat::eDbInfoWrite);

    stmt->Bind(1, blob_info.blob_id);
    stmt->Bind(2, cur_time);
    stmt->Bind(3, cur_time + blob_info.ttl);
    stmt->Bind(4, blob_info.owner);
    stmt->Bind(5, blob_info.ttl);
    stmt->Bind(6, blob_info.size);
    stmt->Execute();
}

void
CNCDBFile::SetBlobDeadTime(TNCBlobId blob_id, int dead_time)
{
    CSQLITE_StatementLock stmt(x_GetStatement(eStmt_SetBlobDeadTime));
    CNCStatTimer timer(GetStat(), CNCDBStat::eDbInfoWrite);

    stmt->Bind(1, blob_id);
    stmt->Bind(2, dead_time);
    stmt->Execute();
}

void
CNCDBFile::ReadBlobInfo(SNCBlobInfo* blob_info)
{
    _ASSERT(blob_info);

    CSQLITE_StatementLock stmt(x_GetStatement(eStmt_ReadBlobInfo));
    CNCStatTimer timer(GetStat(), CNCDBStat::eDbInfoRead);

    stmt->Bind(1, blob_info->blob_id);
    _VERIFY(stmt->Step());
    blob_info->access_time = stmt->GetInt   (0);
    blob_info->expired     = stmt->GetInt   (1) <= int(time(NULL));
    blob_info->owner       = stmt->GetString(2);
    blob_info->ttl         = stmt->GetInt   (3);
    blob_info->size        = stmt->GetInt   (4);
}

void
CNCDBFile::GetChunkIds(TNCBlobId blob_id, TNCIdsList* id_list)
{
    _ASSERT(id_list);
    id_list->clear();

    CSQLITE_StatementLock stmt(x_GetStatement(eStmt_GetChunkIds));
    CNCStatTimer timer(GetStat(), CNCDBStat::eDbInfoRead);

    stmt->Bind(1, blob_id);
    while (stmt->Step()) {
        id_list->push_back(stmt->GetInt8(0));
    }
}

void
CNCDBFile::CreateChunk(TNCBlobId blob_id, TNCChunkId chunk_id)
{
    CSQLITE_StatementLock stmt(x_GetStatement(eStmt_CreateChunk));
    CNCStatTimer timer(GetStat(), CNCDBStat::eDbInfoWrite);

    stmt->Bind(1, chunk_id);
    stmt->Bind(2, blob_id);
    stmt->Execute();
}

void
CNCDBFile::DeleteLastChunks(TNCBlobId blob_id, TNCChunkId min_chunk_id)
{
    CSQLITE_StatementLock stmt(x_GetStatement(eStmt_DeleteLastChunks));
    CNCStatTimer timer(GetStat(), CNCDBStat::eDbInfoWrite);

    stmt->Bind(1, blob_id);
    stmt->Bind(2, min_chunk_id);
    stmt->Execute();

    GetStat()->AddBlobTruncate();
}

TNCChunkId
CNCDBFile::CreateChunkValue(const TNCBlobBuffer& data)
{
    CSQLITE_StatementLock stmt(x_GetStatement(eStmt_CreateChunkData));
    CNCStatTimer timer(GetStat(), CNCDBStat::eDbChunkWrite);

    stmt->Bind(1, static_cast<const void*>(data.data()), data.size());
    stmt->Execute();
    timer.SetChunkSize(data.size());
    return stmt->GetLastInsertedRowid();
}

void
CNCDBFile::WriteChunkValue(TNCChunkId           chunk_id,
                           const TNCBlobBuffer& data)
{
    CSQLITE_StatementLock stmt(x_GetStatement(eStmt_WriteChunkData));
    CNCStatTimer timer(GetStat(), CNCDBStat::eDbChunkWrite);

    stmt->Bind(1, chunk_id);
    stmt->Bind(2, static_cast<const void*>(data.data()), data.size());
    stmt->Execute();
    timer.SetChunkSize(data.size());
}

bool
CNCDBFile::ReadChunkValue(TNCChunkId chunk_id, TNCBlobBuffer* buffer)
{
    _ASSERT(buffer->size() == 0);

    CSQLITE_StatementLock stmt(x_GetStatement(eStmt_ReadChunkData));
    CNCStatTimer timer(GetStat(), CNCDBStat::eDbChunkRead);

    stmt->Bind(1, chunk_id);
    if (stmt->Step()) {
        size_t n_read = stmt->GetBlob(0, buffer->data(), buffer->capacity());
        buffer->resize(n_read);
        timer.SetChunkSize(n_read);
        return true;
    }
    return false;
}

END_NCBI_SCOPE
