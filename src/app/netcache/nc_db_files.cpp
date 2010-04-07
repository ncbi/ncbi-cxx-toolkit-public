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
#include <corelib/ncbifile.hpp>

#include "nc_memory.hpp"
#include "nc_db_files.hpp"
#include "nc_utils.hpp"
#include "nc_db_stat.hpp"
#include "error_codes.hpp"


BEGIN_NCBI_SCOPE

#define NCBI_USE_ERRCODE_X   NetCache_Storage


static const char* kNCBlobKeys_Table          = "NCB";
static const char* kNCBlobKeys_BlobIdCol      = "id";
static const char* kNCBlobKeys_KeyCol         = "key";
static const char* kNCBlobKeys_SubkeyCol      = "skey";
static const char* kNCBlobKeys_VersionCol     = "ver";

static const char* kNCBlobInfo_Table          = "NCI";
static const char* kNCBlobInfo_BlobIdCol      = "id";
static const char* kNCBlobInfo_CreateTimeCol  = "at";
static const char* kNCBlobInfo_DeadTimeCol    = "dt";
static const char* kNCBlobInfo_OwnerCol       = "own";
static const char* kNCBlobInfo_TTLCol         = "ttl";
static const char* kNCBlobInfo_SizeCol        = "sz";
static const char* kNCBlobInfo_CntReadsCol    = "rd";
static const char* kNCBlobInfo_PasswordCol    = "pw";

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
static const char* kNCDBIndex_VolumesCol      = "vlm";


CFastRWLock               CNCFileSystem::sm_FilesListLock;
CNCFileSystem::TFilesList CNCFileSystem::sm_FilesList;
CSpinLock                 CNCFileSystem::sm_EventsLock;
SNCFSEvent* volatile      CNCFileSystem::sm_EventsHead      = NULL;
SNCFSEvent*               CNCFileSystem::sm_EventsTail      = NULL;
CRef<CThread>             CNCFileSystem::sm_BGThread;
bool                      CNCFileSystem::sm_Stopped         = false;
bool volatile             CNCFileSystem::sm_BGWorking       = false;
CSemaphore                CNCFileSystem::sm_BGSleep        (0, 1000000);
bool                      CNCFileSystem::sm_DiskInitialized = false;
CAtomicCounter            CNCFileSystem::sm_WaitingOnAlert;
CSemaphore                CNCFileSystem::sm_OnAlertWaiter  (0, 1000000);



// Forward declarations
static int s_SQLITE_FS_Fullname(sqlite3_vfs*, const char*, int, char*);
static int s_SQLITE_FS_Access(sqlite3_vfs*, const char*, int, int*);
static int s_SQLITE_FS_Open(sqlite3_vfs*, const char*, sqlite3_file*, int, int*);
static int s_SQLITE_FS_Close(sqlite3_file*);
static int s_SQLITE_FS_Read(sqlite3_file*, void*, int, sqlite3_int64);
static int s_SQLITE_FS_Write(sqlite3_file*, const void*, int, sqlite3_int64);
static int s_SQLITE_FS_Delete(sqlite3_vfs*, const char*, int);
static int s_SQLITE_FS_Size(sqlite3_file*, sqlite3_int64*);
static int s_SQLITE_FS_Lock(sqlite3_file*, int);
static int s_SQLITE_FS_Unlock(sqlite3_file*, int);
static int s_SQLITE_FS_IsReserved(sqlite3_file*, int*);
static int s_SQLITE_FS_Random(sqlite3_vfs*, int, char*);
static int s_SQLITE_FS_CurTime(sqlite3_vfs*, double*);
static int s_SQLITE_FS_AboutDevice(sqlite3_file*);
static int s_SQLITE_FS_Sync(sqlite3_file*, int);

/// All methods of virtual file system exposed to SQLite
static sqlite3_vfs s_NCVirtualFS = {
    1,                      /* iVersion */
    sizeof(CNCFSVirtFile),  /* szOsFile */
    0,                      /* mxPathname */
    NULL,                   /* pNext */
    "NC_VirtualFS",         /* zName */
    NULL,                   /* pAppData */
    s_SQLITE_FS_Open,       /* xOpen */
    s_SQLITE_FS_Delete,     /* xDelete */
    s_SQLITE_FS_Access,     /* xAccess */
    s_SQLITE_FS_Fullname,   /* xFullPathname */
    NULL,                   /* xDlOpen */
    NULL,                   /* xDlError */
    NULL,                   /* xDlSym */
    NULL,                   /* xDlClose */
    s_SQLITE_FS_Random,     /* xRandomness */
    NULL,                   /* xSleep */
    s_SQLITE_FS_CurTime,    /* xCurrentTime */
    NULL                    /* xGetLastError */
};

/// All methods of operations with file exposed to SQLite
static sqlite3_io_methods s_NCVirtFileMethods = {
    1,                       /* iVersion */
    s_SQLITE_FS_Close,       /* xClose */
    s_SQLITE_FS_Read,        /* xRead */
    s_SQLITE_FS_Write,       /* xWrite */
    NULL,                    /* xTruncate */
    s_SQLITE_FS_Sync,        /* xSync */
    s_SQLITE_FS_Size,        /* xFileSize */
    s_SQLITE_FS_Lock,        /* xLock */
    s_SQLITE_FS_Unlock,      /* xUnlock */
    s_SQLITE_FS_IsReserved,  /* xCheckReservedLock */
    NULL,                    /* xFileControl */
    NULL,                    /* xSectorSize */
    s_SQLITE_FS_AboutDevice  /* xDeviceCharacteristics */
};

/// Get pointer to default SQLite's VFS
static inline sqlite3_vfs*
s_GetRealVFS(void)
{
    return static_cast<sqlite3_vfs*>(s_NCVirtualFS.pAppData);
}

/// Set pointer to default SQLite's VFS
static inline void
s_SetRealVFS(sqlite3_vfs* vfs)
{
    s_NCVirtualFS.pAppData   = vfs;
    s_NCVirtualFS.mxPathname = vfs->mxPathname;
}



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
               << kNCDBIndex_VolumesCol     << " int not null"
        << ")";
    stmt.SetSql(sql);
    stmt.Execute();

    try {
        // Try to upgrade from previous version of index database
        sql.Clear();
        sql << "alter table " << kNCDBIndex_Table
            << " add column " << kNCDBIndex_VolumesCol
                                            << " int not null default 0";
        stmt.SetSql(sql);
        stmt.Execute();
    }
    catch (CSQLITE_Exception& ex) {
        ERR_POST_X(15, Info << ex);
    }
}

void
CNCDBFile::CreateMetaDatabase(void)
{
    CQuickStrStream sql;
    CSQLITE_Statement stmt(this);
    CNCStatTimer timer(GetStat(), CNCDBStat::eDbOther);

    stmt.SetSql("BEGIN");
    stmt.Execute();

    sql.Clear();
    sql << "create table " << kNCBlobKeys_Table
        << "(" << kNCBlobKeys_BlobIdCol  << " integer primary key,"
               << kNCBlobKeys_KeyCol     << " varchar not null,";
    AddSubkeyColsDef(sql);
    sql << ")";
    stmt.SetSql(sql);
    stmt.Execute();

    sql.Clear();
    sql << "create table " << kNCBlobInfo_Table
        << "(" << kNCBlobInfo_BlobIdCol     << " integer primary key,"
               << kNCBlobInfo_CreateTimeCol << " int not null,"
               << kNCBlobInfo_DeadTimeCol   << " int not null,"
               << kNCBlobInfo_OwnerCol      << " varchar not null,"
               << kNCBlobInfo_TTLCol        << " int not null,"
               << kNCBlobInfo_SizeCol       << " int64 not null,"
               << kNCBlobInfo_CntReadsCol   << " int64 not null,"
               << kNCBlobInfo_PasswordCol   << " varchar not null,"
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

    stmt.SetSql("COMMIT");
    stmt.Execute();
}

void
CNCDBFile::AdjustMetaDatabase(void)
{
    try {
        // Try to upgrade from previous version of meta database
        CQuickStrStream sql;
        CSQLITE_Statement stmt(this);

        sql << "alter table " << kNCBlobInfo_Table
            << " add column " << kNCBlobInfo_CntReadsCol
                                            << " int64 not null default 0";
        stmt.SetSql(sql);
        stmt.Execute();

        sql.Clear();
        sql << "alter table " << kNCBlobInfo_Table
            << " add column " << kNCBlobInfo_PasswordCol
                                            << " varchar not null default ''";
        stmt.SetSql(sql);
        stmt.Execute();
    }
    catch (CSQLITE_Exception& ex) {
        ERR_POST_X(18, Info << ex);
    }
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
                       << kNCDBIndex_CreatedTimeCol << ","
                       << kNCDBIndex_VolumesCol
                << ")values(?1,?2,?3,?4,?5)";
            break;
        case eStmt_DeleteDBPart:
            sql << "delete from " << kNCDBIndex_Table
                << " where " << kNCDBIndex_DBIdCol << "=?1";
            break;
        case eStmt_GetBlobId:
        case eStmt_BlobFamilyExists:
        case eStmt_BlobExists:
        case eStmt_CreateBlobKey:
        case eStmt_GetBlobsList:
            GetSpecificStatement(typ, sql);
            break;
        case eStmt_WriteBlobInfo:
            sql << "insert or replace into " << kNCBlobInfo_Table
                << "(" << kNCBlobInfo_BlobIdCol     << ","
                       << kNCBlobInfo_CreateTimeCol << ","
                       << kNCBlobInfo_DeadTimeCol   << ","
                       << kNCBlobInfo_OwnerCol      << ","
                       << kNCBlobInfo_TTLCol        << ","
                       << kNCBlobInfo_SizeCol       << ","
                       << kNCBlobInfo_CntReadsCol   << ","
                       << kNCBlobInfo_PasswordCol
                << ")values(?1,?2,?3,?4,?5,?6,?7,?8)";
            break;
        case eStmt_SetBlobDeadTime:
            sql << "update " << kNCBlobInfo_Table
                <<   " set " << kNCBlobInfo_DeadTimeCol << "=?2"
                << " where " << kNCBlobInfo_BlobIdCol << "=?1";
            break;
        case eStmt_ReadBlobInfo:
            sql << "select " << kNCBlobInfo_CreateTimeCol << ","
                             << kNCBlobInfo_DeadTimeCol   << ","
                             << kNCBlobInfo_OwnerCol      << ","
                             << kNCBlobInfo_TTLCol        << ","
                             << kNCBlobInfo_SizeCol       << ","
                             << kNCBlobInfo_CntReadsCol   << ","
                             << kNCBlobInfo_PasswordCol
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

    part_info->last_rot_time = part_info->create_time = int(time(NULL));
    stmt->Bind(1, part_info->part_id);
    stmt->Bind(2, part_info->meta_file.data(), part_info->meta_file.size());
    stmt->Bind(3, part_info->data_file.data(), part_info->data_file.size());
    stmt->Bind(4, part_info->create_time);
    stmt->Bind(5, part_info->cnt_volumes);
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
                     << kNCDBIndex_VolumesCol
        <<  " from " << kNCDBIndex_Table
        << " order by " << kNCDBIndex_CreatedTimeCol;

    CSQLITE_Statement stmt(this, sql);
    while (stmt.Step()) {
        SNCDBPartInfo* info = new SNCDBPartInfo;
        info->part_id       = stmt.GetInt8  (0);
        info->meta_file     = stmt.GetString(1);
        info->data_file     = stmt.GetString(2);
        info->create_time   = stmt.GetInt   (3);
        info->last_rot_time = info->create_time;
        info->cnt_volumes   = stmt.GetInt   (4);

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

    stmt->Bind(1,  identity->key.data(), identity->key.size());
    BindBlobSubkey(stmt, *identity);
    stmt->Bind(20, dead_after);
    if (stmt->Step()) {
        identity->blob_id = stmt->GetInt8(0);
        return true;
    }
    return false;
}

bool
CNCDBFile::IsBlobFamilyExists(const string& key,
                              const string& subkey,
                              int           dead_after)
{
    CSQLITE_StatementLock stmt(x_GetStatement(eStmt_BlobFamilyExists));
    CNCStatTimer timer(GetStat(), CNCDBStat::eDbOther);

    stmt->Bind(1,  key.data(), key.size());
    BindBlobSubkey(stmt, subkey);
    stmt->Bind(20, dead_after);
    _VERIFY(stmt->Step());

    return stmt->GetInt8(0) > 0;
}

bool
CNCDBFile::IsBlobExists(const SNCBlobIdentity& identity,
                        int                    dead_after)
{
    CSQLITE_StatementLock stmt(x_GetStatement(eStmt_BlobExists));
    CNCStatTimer timer(GetStat(), CNCDBStat::eDbOther);

    // For now we don't allow for moved blob to keep its blob_id, so for exact
    // existence we need to check it too.
    stmt->Bind(1,  identity.blob_id);
    stmt->Bind(2,  identity.key.data(), identity.key.size());
    BindBlobSubkey(stmt, identity);
    stmt->Bind(20, dead_after);
    _VERIFY(stmt->Step());

    return stmt->GetInt8(0) > 0;
}

void
CNCDBFile::CreateBlobKey(const SNCBlobIdentity& identity)
{
    CSQLITE_StatementLock stmt(x_GetStatement(eStmt_CreateBlobKey));
    CNCStatTimer timer(GetStat(), CNCDBStat::eDbInfoWrite);

    stmt->Bind(1,  identity.blob_id);
    stmt->Bind(2,  identity.key.data(), identity.key.size());
    BindBlobSubkey(stmt, identity);

    stmt->Execute();
}

void
CNCDBFile::GetBlobsList(int&          dead_after,
                        TNCBlobId&    id_after,
                        int           dead_before,
                        int           max_count,
                        TNCBlobsList* blobs_list)
{
    _ASSERT(blobs_list);
    blobs_list->clear();

    CSQLITE_StatementLock stmt(x_GetStatement(eStmt_GetBlobsList));
    CNCStatTimer timer(GetStat(), CNCDBStat::eDbOther);

    stmt->Bind(1, dead_after);
    stmt->Bind(2, id_after);
    stmt->Bind(3, dead_before);
    stmt->Bind(4, max_count);
    while (stmt->Step()) {
        AutoPtr<SNCBlobIdentity> identity = new SNCBlobIdentity();
        identity->blob_id = id_after = stmt->GetInt8  (0);
        dead_after                   = stmt->GetInt   (1);
        identity->key                = stmt->GetString(2);
        ReadBlobSubkey(stmt, 3, identity.get());
        blobs_list->push_back(identity);
    }
}

void
CNCDBFile::WriteBlobInfo(SNCBlobInfo& blob_info, bool move_dead_time)
{
    int cur_time = int(time(NULL));
    if (blob_info.create_time == 0) {
        blob_info.create_time = cur_time;
    }
    if (move_dead_time  ||  blob_info.dead_time == 0) {
        blob_info.dead_time = cur_time + blob_info.ttl;
    }

    CSQLITE_StatementLock stmt(x_GetStatement(eStmt_WriteBlobInfo));
    CNCStatTimer timer(GetStat(), CNCDBStat::eDbInfoWrite);

    stmt->Bind(1, blob_info.blob_id);
    stmt->Bind(2, blob_info.create_time);
    stmt->Bind(3, blob_info.dead_time);
    stmt->Bind(4, blob_info.owner);
    stmt->Bind(5, blob_info.ttl);
    stmt->Bind(6, blob_info.size);
    stmt->Bind(7, blob_info.cnt_reads);
    stmt->Bind(8, blob_info.password.data(), blob_info.password.size());
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

bool
CNCDBFile::ReadBlobInfo(SNCBlobInfo* blob_info)
{
    _ASSERT(blob_info);

    CSQLITE_StatementLock stmt(x_GetStatement(eStmt_ReadBlobInfo));
    CNCStatTimer timer(GetStat(), CNCDBStat::eDbInfoRead);

    stmt->Bind(1, blob_info->blob_id);
    if (stmt->Step()) {
        blob_info->create_time = stmt->GetInt   (0);
        blob_info->dead_time   = stmt->GetInt   (1);
        blob_info->expired     = blob_info->dead_time <= int(time(NULL));
        blob_info->owner       = stmt->GetString(2);
        blob_info->ttl         = stmt->GetInt   (3);
        blob_info->size        = stmt->GetInt   (4);
        blob_info->cnt_reads   = stmt->GetInt   (5);
        blob_info->password    = stmt->GetString(6);
        return true;
    }
    return false;
}

void
CNCDBFile::GetChunkIds(TNCBlobId blob_id, TNCChunksList* id_list)
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

CNCDBFile::~CNCDBFile(void)
{
    m_Stmts.clear();
}

void
CNCDBFile::AddSubkeyColsDef(CQuickStrStream&)
{}

void
CNCDBFile::GetSpecificStatement(ENCStmtType, CQuickStrStream&)
{}

void
CNCDBFile::BindBlobSubkey(CSQLITE_Statement*, const SNCBlobIdentity&)
{}

void
CNCDBFile::BindBlobSubkey(CSQLITE_Statement*, const string&)
{}

void
CNCDBFile::ReadBlobSubkey(CSQLITE_Statement*, int, SNCBlobIdentity*)
{}


CNCDBIndexFile::~CNCDBIndexFile(void)
{}


CNCDBMetaFile::~CNCDBMetaFile(void)
{}


CNCDBMetaFile_ICache::~CNCDBMetaFile_ICache(void)
{}

void
CNCDBMetaFile_ICache::AddSubkeyColsDef(CQuickStrStream& sql)
{
    sql << kNCBlobKeys_SubkeyCol  << " varchar not null,"
        << kNCBlobKeys_VersionCol << " int not null,"
        // This index plays significant role at the start of NetCache.
        // And although start time seems very small and rare and at all
        // other times index only slows inserts it's still necessary
        // because without it starting time becomes intolerably bad.
        << "unique(" << kNCBlobKeys_KeyCol    << ","
                     << kNCBlobKeys_SubkeyCol << ","
                     << kNCBlobKeys_VersionCol
        // id should not be in the unique because we rely on it if we
        // need to change id for the blob when it is re-created.
        <<       ")";
}

void
CNCDBMetaFile_ICache::GetSpecificStatement(ENCStmtType      typ,
                                           CQuickStrStream& sql)
{
    switch (typ) {
    case eStmt_GetBlobId:
        sql << "select a." << kNCBlobKeys_BlobIdCol
            <<  " from " << kNCBlobKeys_Table << " a,"
                         << kNCBlobInfo_Table << " b"
            << " where a." << kNCBlobKeys_KeyCol     << "=?1"
            <<   " and a." << kNCBlobKeys_SubkeyCol  << "=?10"
            <<   " and a." << kNCBlobKeys_VersionCol << "=?11"
            <<   " and b." << kNCBlobInfo_BlobIdCol
            <<                             "=a." << kNCBlobKeys_BlobIdCol
            <<   " and b." << kNCBlobInfo_DeadTimeCol << ">=?20";
        break;
    case eStmt_BlobFamilyExists:
        sql << "select count(*)"
            <<  " from " << kNCBlobKeys_Table << " a,"
                         << kNCBlobInfo_Table << " b"
            << " where a." << kNCBlobKeys_KeyCol    << "=?1"
            <<   " and a." << kNCBlobKeys_SubkeyCol << "=?10"
            <<   " and b." << kNCBlobInfo_BlobIdCol
            <<                             "=a." << kNCBlobKeys_BlobIdCol
            <<   " and b." << kNCBlobInfo_DeadTimeCol << ">=?20";
        break;
    case eStmt_BlobExists:
        sql << "select count(*)"
            <<  " from " << kNCBlobKeys_Table << " a,"
                         << kNCBlobInfo_Table << " b"
            << " where a." << kNCBlobKeys_BlobIdCol  << "=?1"
            <<   " and a." << kNCBlobKeys_KeyCol     << "=?2"
            <<   " and a." << kNCBlobKeys_SubkeyCol  << "=?10"
            <<   " and a." << kNCBlobKeys_VersionCol << "=?11"
            <<   " and b." << kNCBlobInfo_BlobIdCol
            <<                             "=a." << kNCBlobKeys_BlobIdCol
            <<   " and b." << kNCBlobInfo_DeadTimeCol << ">=?20";
        break;
    case eStmt_CreateBlobKey:
        // Replace - in case if blob was recently deleted and now it's
        // created again when record in database is still exists.
        sql << "insert or replace into " << kNCBlobKeys_Table
            << "(" << kNCBlobKeys_BlobIdCol << ","
                   << kNCBlobKeys_KeyCol    << ","
                   << kNCBlobKeys_SubkeyCol << ","
                   << kNCBlobKeys_VersionCol
            << ")values(?1,?2,?10,?11)";
        break;
    case eStmt_GetBlobsList:
        sql << "select a." << kNCBlobInfo_BlobIdCol   << ","
            <<        "a." << kNCBlobInfo_DeadTimeCol << ","
            <<        "b." << kNCBlobKeys_KeyCol      << ","
            <<        "b." << kNCBlobKeys_SubkeyCol   << ","
            <<        "b." << kNCBlobKeys_VersionCol
            <<  " from " << kNCBlobInfo_Table << " a,"
                         << kNCBlobKeys_Table << " b"
            << " where a."  << kNCBlobInfo_DeadTimeCol  << ">=?1"
            <<   " and a."  << kNCBlobInfo_DeadTimeCol  << "<?3"
            <<   " and (a." << kNCBlobInfo_DeadTimeCol  << ">?1"
            <<        " or a." << kNCBlobInfo_BlobIdCol << ">?2)"
            <<   " and b." << kNCBlobKeys_BlobIdCol
            <<                             "=a." << kNCBlobInfo_BlobIdCol
            << " order by 2,1"
            << " limit ?4";
        break;
    default:
        NCBI_TROUBLE("Unknown statement type");
    }
}

void
CNCDBMetaFile_ICache::BindBlobSubkey(CSQLITE_Statement*     stmt,
                                     const SNCBlobIdentity& identity)
{
    stmt->Bind(10, identity.subkey.data(), identity.subkey.size());
    stmt->Bind(11, identity.version);
}

void
CNCDBMetaFile_ICache::BindBlobSubkey(CSQLITE_Statement* stmt,
                                     const string&      subkey)
{
    stmt->Bind(10, subkey.data(), subkey.size());
}

void
CNCDBMetaFile_ICache::ReadBlobSubkey(CSQLITE_Statement* stmt,
                                     int                col_num,
                                     SNCBlobIdentity*   identity)
{
    identity->subkey  = stmt->GetString(col_num);
    identity->version = stmt->GetInt   (col_num + 1);
}


CNCDBMetaFile_NCCache::~CNCDBMetaFile_NCCache(void)
{}

void
CNCDBMetaFile_NCCache::AddSubkeyColsDef(CQuickStrStream& sql)
{
    sql // This index plays significant role at the start of NetCache.
        // And although start time seems very small and rare and at all
        // other times index only slows inserts it's still necessary
        // because without it starting time becomes intolerably bad.
        << "unique(" << kNCBlobKeys_KeyCol
        // id should not be in the unique because we rely on it if we
        // need to change id for the blob when it is re-created.
        <<       ")";
}

void
CNCDBMetaFile_NCCache::GetSpecificStatement(ENCStmtType      typ,
                                            CQuickStrStream& sql)
{
    switch (typ) {
    case eStmt_GetBlobId:
        sql << "select a." << kNCBlobKeys_BlobIdCol
            <<  " from " << kNCBlobKeys_Table << " a,"
                         << kNCBlobInfo_Table << " b"
            << " where a." << kNCBlobKeys_KeyCol     << "=?1"
            <<   " and b." << kNCBlobInfo_BlobIdCol
            <<                             "=a." << kNCBlobKeys_BlobIdCol
            <<   " and b." << kNCBlobInfo_DeadTimeCol << ">=?20";
        break;
    case eStmt_BlobFamilyExists:
        sql << "select count(*)"
            <<  " from " << kNCBlobKeys_Table << " a,"
                         << kNCBlobInfo_Table << " b"
            << " where a." << kNCBlobKeys_KeyCol    << "=?1"
            <<   " and b." << kNCBlobInfo_BlobIdCol
            <<                             "=a." << kNCBlobKeys_BlobIdCol
            <<   " and b." << kNCBlobInfo_DeadTimeCol << ">=?20";
        break;
    case eStmt_BlobExists:
        sql << "select count(*)"
            <<  " from " << kNCBlobKeys_Table << " a,"
                         << kNCBlobInfo_Table << " b"
            << " where a." << kNCBlobKeys_BlobIdCol  << "=?1"
            <<   " and a." << kNCBlobKeys_KeyCol     << "=?2"
            <<   " and b." << kNCBlobInfo_BlobIdCol
            <<                             "=a." << kNCBlobKeys_BlobIdCol
            <<   " and b." << kNCBlobInfo_DeadTimeCol << ">=?20";
        break;
    case eStmt_CreateBlobKey:
        // Replace - in case if blob was recently deleted and now it's
        // created again when record in database is still exists.
        sql << "insert or replace into " << kNCBlobKeys_Table
            << "(" << kNCBlobKeys_BlobIdCol << ","
                   << kNCBlobKeys_KeyCol
            << ")values(?1,?2)";
        break;
    case eStmt_GetBlobsList:
        sql << "select a." << kNCBlobInfo_BlobIdCol   << ","
            <<        "a." << kNCBlobInfo_DeadTimeCol << ","
            <<        "b." << kNCBlobKeys_KeyCol
            <<  " from " << kNCBlobInfo_Table << " a,"
                         << kNCBlobKeys_Table << " b"
            << " where a."  << kNCBlobInfo_DeadTimeCol  << ">=?1"
            <<   " and a."  << kNCBlobInfo_DeadTimeCol  << "<?3"
            <<   " and (a." << kNCBlobInfo_DeadTimeCol  << ">?1"
            <<        " or a." << kNCBlobInfo_BlobIdCol << ">?2)"
            <<   " and b." << kNCBlobKeys_BlobIdCol
            <<                             "=a." << kNCBlobInfo_BlobIdCol
            << " order by 2,1"
            << " limit ?4";
        break;
    default:
        NCBI_TROUBLE("Unknown statement type");
    }
}


CNCDBDataFile::~CNCDBDataFile(void)
{}



CNCDBFilesPool::~CNCDBFilesPool(void)
{}


CNCDBFilesPool_ICache::~CNCDBFilesPool_ICache(void)
{}

void
CNCDBFilesPool_ICache::GetFile(CNCDBMetaFile** file_ptr)
{
    *file_ptr = m_Metas.GetObjPtr();
}


CNCDBFilesPool_NCCache::~CNCDBFilesPool_NCCache(void)
{}

void
CNCDBFilesPool_NCCache::GetFile(CNCDBMetaFile** file_ptr)
{
    *file_ptr = m_Metas.GetObjPtr();
}


inline int
CNCFileSystem::OpenRealFile(sqlite3_file** file,
                            const char*    name,
                            int            flags,
                            int*           out_flags)
{
    sqlite3_vfs* vfs = s_GetRealVFS();
    *file = reinterpret_cast<sqlite3_file*>(new char[vfs->szOsFile]);
    return vfs->xOpen(vfs, name, *file, flags, out_flags);
}

inline int
CNCFileSystem::CloseRealFile(sqlite3_file*& file)
{
    int result = SQLITE_OK;
    if (file) {
        if (file->pMethods) {
            result = file->pMethods->xClose(file);
        }
        delete[] file;
        file = NULL;
    }
    return result;
}


inline
CNCFSOpenFile::CNCFSOpenFile(const string& file_name, bool force_sync_io)
    : m_Name(file_name),
      m_RealFile(NULL),
      m_FirstPage(NULL),
      m_Flags(fOpened | (force_sync_io? fForcedSync: 0))
{
    if (force_sync_io)
        return;

    const int flags = SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE
                      | SQLITE_OPEN_MAIN_DB;
    int out_flags   = 0;
    int ret = CNCFileSystem::OpenRealFile(&m_RealFile, file_name.c_str(),
                                          flags, &out_flags);
    if (ret != SQLITE_OK) {
        ERR_POST(Fatal << "Error opening database file: " << ret);
    }
    sqlite3_int64 size;
    ret = m_RealFile->pMethods->xFileSize(m_RealFile, &size);
    if (ret != SQLITE_OK) {
        ERR_POST(Fatal << "Error reading database size: " << ret);
    }
    m_Size = size;

    m_FirstPage = new char[kNCSQLitePageSize];
    ret = m_RealFile->pMethods->xRead(m_RealFile, m_FirstPage,
                                      kNCSQLitePageSize, 0);
    if (ret != SQLITE_OK  &&  ret != SQLITE_IOERR_SHORT_READ) {
        ERR_POST(Fatal << "Error reading database: " << ret);
    }
}

inline
CNCFSOpenFile::~CNCFSOpenFile(void)
{
    CNCFileSystem::CloseRealFile(m_RealFile);
    delete[] m_FirstPage;
}

inline int
CNCFSOpenFile::ReadFirstPage(Int8 offset, int cnt, void* mem_ptr)
{
    _ASSERT(offset < kNCSQLitePageSize);

    if (offset < m_Size) {
        int copy_cnt = cnt;
        Int8 left_cnt = m_Size - offset;
        if (left_cnt < copy_cnt)
            copy_cnt = int(left_cnt);
        /*if (offset != 24 || cnt != 16) {
            LOG_POST("Reading first page " << cnt << " at " << offset);
        }*/
        memcpy(mem_ptr, &m_FirstPage[offset], copy_cnt);
        cnt -= copy_cnt;
        mem_ptr = static_cast<char*>(mem_ptr) + copy_cnt;
    }
    if (cnt == 0) {
        return SQLITE_OK;
    }
    else {
        memset(mem_ptr, 0, cnt);
        return SQLITE_IOERR_SHORT_READ;
    }
}

inline void
CNCFSOpenFile::WriteFirstPage(Int8 offset, const void** data, int cnt)
{
    _ASSERT(offset <= m_Size  &&  offset < kNCSQLitePageSize);
    _ASSERT(offset == 0  &&  cnt == kNCSQLitePageSize);

    if (m_Flags & fInitialized) {
        // Only 16 bytes at offset of 24 bytes in first page of the database
        // are constantly re-read and re-written by SQLite. Everything else is
        // initialized at the database creation, is not changed furthermore
        // and during work is stored in database cache. So we save time spent
        // in memcpy() and copy only these "magic" 16 bytes.
        memcpy(&m_FirstPage[24], static_cast<const char*>(*data) + 24, 16);
    }
    else {
        memcpy(&m_FirstPage[offset], *data, cnt);
    }
    *data = &m_FirstPage[offset];
}

inline void
CNCFSOpenFile::AdjustSize(Int8 size)
{
    // This method is called under SQLite's locking, so there shouldn't be
    // several parallel adjustments. So we don't need mutexes here.
    m_Size = max(m_Size, size);
}


void
CNCFileSystem::Initialize(void)
{
    s_SetRealVFS(CSQLITE_Global::GetDefaultVFS());
    CSQLITE_Global::RegisterCustomVFS(&s_NCVirtualFS);

    sm_WaitingOnAlert.Set(0);

    sm_BGThread = NewBGThread(&CNCFileSystem::x_DoBackgroundWork);
    sm_BGThread->Run();
}

void
CNCFileSystem::Finalize(void)
{
    sm_Stopped = true;
    try {
        sm_BGSleep.Post();
    }
    catch (CException&) {
        // Let's protect against accidental overflow of semaphore's counter.
    }
    sm_BGThread->Join();
}

inline CNCFSOpenFile*
CNCFileSystem::FindOpenFile(const string& name)
{
    CFastReadGuard guard(sm_FilesListLock);

    TFilesList::const_iterator it = sm_FilesList.find(name);
    return it == sm_FilesList.end()? NULL: it->second;
}

CNCFSOpenFile*
CNCFileSystem::OpenNewDBFile(const string& file_name,
                             bool          force_sync_io /* = false */)
{
    CNCFSOpenFile* file = FindOpenFile(file_name);
    if (!file) {
        file = new CNCFSOpenFile(file_name, force_sync_io);

        CFastWriteGuard guard(sm_FilesListLock);
        sm_FilesList[file_name] = file;
    }
    return file;
}

inline void
CNCFileSystem::x_RemoveFileFromList(CNCFSOpenFile* file)
{
    CFastWriteGuard guard(sm_FilesListLock);
    sm_FilesList.erase(file->m_Name);
}

inline void
CNCFileSystem::x_AddNewEvent(SNCFSEvent* event)
{
    sm_EventsLock.Lock();
    event->next = NULL;
    if (sm_EventsTail) {
        sm_EventsTail->next = event;
    }
    else {
        _ASSERT(!sm_EventsHead);
        sm_EventsHead = event;
    }
    sm_EventsTail = event;
    if (!sm_BGWorking) {
        try {
            sm_BGSleep.Post();
        }
        catch (CException&) {
            // For some reason this can happen sometimes (attempt to exceed
            // max_count) but we can safely ignore that.
        }
    }
    sm_EventsLock.Unlock();
}

void
CNCFileSystem::DeleteFileOnClose(const string& file_name)
{
    CNCFSOpenFile* file = FindOpenFile(file_name.c_str());
    bool need_delete = true;
    if (file) {
        _ASSERT(!file->IsForcedSync());
        if (file->IsFileOpen()) {
            file->m_Flags |= CNCFSOpenFile::fDeleteOnClose;
            need_delete = false;
        }
        else {
            x_RemoveFileFromList(file);
            delete file;
        }
    }
    if (need_delete) {
        CFile(file_name).Remove();
        // For the case of transferring of database from the previous
        // NetCache version.
        CFile(file_name + "-journal").Remove();
    }
}

void
CNCFileSystem::SetFileInitialized(const string& file_name)
{
    CNCFSOpenFile* file = FindOpenFile(file_name.c_str());
    if (!file) {
        file = OpenNewDBFile(file_name);
        file->SetFileOpen(false);
    }
    _ASSERT(file);
    file->m_Flags |= CNCFSOpenFile::fInitialized;
}

inline void
CNCFileSystem::CloseDBFile(CNCFSOpenFile* file)
{
    SNCFSEvent* event = new SNCFSEvent;
    event->type = SNCFSEvent::eClose;
    event->file = file;
    x_AddNewEvent(event);
}

inline void
CNCFileSystem::WriteToFile(CNCFSOpenFile* file,
                           Int8           offset,
                           const void*    data,
                           int            cnt)
{
    _ASSERT(offset >= kNCSQLitePageSize  ||  offset + cnt <= kNCSQLitePageSize);

    if (CNCMemManager::IsOnAlert()
        // Two conditions below will ensure that background writer is working
        // and will be able to wake us up.
        &&  sm_BGWorking  &&  sm_EventsHead != NULL)
    {
        sm_EventsLock.Lock();
        if (sm_BGWorking  &&  sm_EventsHead != NULL) {
            sm_WaitingOnAlert.Add(1);
            sm_EventsLock.Unlock();
            sm_OnAlertWaiter.Wait();
        }
        else {
            sm_EventsLock.Unlock();
        }
    }

    SNCFSEvent* event = new SNCFSEvent;
    event->type    = SNCFSEvent::eWrite;
    event->file    = file;
    event->offset  = offset;
    event->cnt     = cnt;
    if (offset < kNCSQLitePageSize) {
        file->WriteFirstPage(offset, &data, cnt);
    }
    else {
        CNCMemManager::LockDBPage(data);
    }
    event->data = data;
    x_AddNewEvent(event);
    file->AdjustSize(offset + cnt);
}

inline void
CNCFileSystem::x_DoWriteToFile(SNCFSEvent* event)
{
    CNCFSOpenFile* file = event->file;
    file->m_RealFile->pMethods->xWrite(file->m_RealFile,
                                       event->data, event->cnt, event->offset);
    if (event->offset >= kNCSQLitePageSize) {
        CNCMemManager::UnlockDBPage(event->data);
    }
}

inline void
CNCFileSystem::x_DoCloseFile(SNCFSEvent* event)
{
    CNCFSOpenFile* file = event->file;
    string file_name = file->m_Name;
    bool need_delete = (file->m_Flags & CNCFSOpenFile::fDeleteOnClose) != 0;

    x_RemoveFileFromList(file);
    delete file;
    if (need_delete) {
        CFile(file_name).Remove();
        // For the case of transferring of database from the previous
        // NetCache version.
        CFile(file_name + "-journal").Remove();
    }
}

void
CNCFileSystem::x_DoBackgroundWork(void)
{
    while (!sm_Stopped) {
        for (;;) {
            sm_EventsLock.Lock();
            SNCFSEvent* head = sm_EventsHead;
            sm_EventsHead = sm_EventsTail = NULL;
            sm_BGWorking = head != NULL;
            sm_EventsLock.Unlock();

            if (!head)
                break;
            while (head) {
                switch (head->type) {
                case SNCFSEvent::eWrite:
                    x_DoWriteToFile(head);
                    break;
                case SNCFSEvent::eClose:
                    x_DoCloseFile(head);
                    break;
                }
                SNCFSEvent* prev = head;
                head = head->next;
                delete prev;

                if (sm_WaitingOnAlert.Get() != 0) {
                    sm_WaitingOnAlert.Add(-1);
                    sm_OnAlertWaiter.Post();
                }
            }
        }
        sm_BGSleep.Wait();
    }
}


inline int
CNCFSVirtFile::Open(const char* name, int flags, int* out_flags)
{
    pMethods       = &s_NCVirtFileMethods;
    int read_flags = flags;
    if ((flags & SQLITE_OPEN_MAIN_JOURNAL) == 0) {
        m_WriteFile = CNCFileSystem::FindOpenFile(name);
        if (m_WriteFile) {
            m_WriteFile->SetFileOpen();
        }
        else {
            m_WriteFile = CNCFileSystem::OpenNewDBFile(name);
        }
        _ASSERT(m_WriteFile);
        if (m_WriteFile->IsForcedSync()) {
            m_WriteFile = NULL;
        }
        else {
            read_flags &= ~SQLITE_OPEN_CREATE;
        }
    }
    else {
        m_WriteFile = NULL;
    }
    return CNCFileSystem::OpenRealFile(&m_ReadFile, name,
                                       read_flags, out_flags);
}

inline int
CNCFSVirtFile::Close(void)
{
    if (m_WriteFile) {
        CNCFileSystem::CloseDBFile(m_WriteFile);
        m_WriteFile = NULL;
    }
    return CNCFileSystem::CloseRealFile(m_ReadFile);
}

inline int
CNCFSVirtFile::Read(Int8 offset, int cnt, void* mem_ptr)
{
    if (offset < kNCSQLitePageSize  &&  m_WriteFile) {
        _ASSERT(offset + cnt <= kNCSQLitePageSize);
        return m_WriteFile->ReadFirstPage(offset, cnt, mem_ptr);
    }
    return m_ReadFile->pMethods->xRead(m_ReadFile, mem_ptr, cnt, offset);
}

inline int
CNCFSVirtFile::Write(Int8 offset, const void* data, int cnt)
{
    if (m_WriteFile) {
        CNCFileSystem::WriteToFile(m_WriteFile, offset, data, cnt);
        return SQLITE_OK;
    }
    return m_ReadFile->pMethods->xWrite(m_ReadFile, data, cnt, offset);
}

inline Int8
CNCFSVirtFile::GetSize(void)
{
    if (m_WriteFile)
        return m_WriteFile->GetSize();

    sqlite3_int64 size;
    m_ReadFile->pMethods->xFileSize(m_ReadFile, &size);
    return size;
}

inline int
CNCFSVirtFile::Lock(int lock_type)
{
    if (m_WriteFile) {
        return SQLITE_OK;
    }
    return m_ReadFile->pMethods->xLock(m_ReadFile, lock_type);
}

inline int
CNCFSVirtFile::Unlock(int lock_type)
{
    if (m_WriteFile) {
        return SQLITE_OK;
    }
    return m_ReadFile->pMethods->xUnlock(m_ReadFile, lock_type);
}

inline bool
CNCFSVirtFile::IsReserved(void)
{
    int result;
    m_ReadFile->pMethods->xCheckReservedLock(m_ReadFile, &result);
    return result != 0;
}


/// Get fullname of some file
static int
s_SQLITE_FS_Fullname(sqlite3_vfs*, const char *name,
                     int cnt_out, char *fullname)
{
    sqlite3_vfs* vfs = s_GetRealVFS();
    return vfs->xFullPathname(vfs, name, cnt_out, fullname);
}

/// Check whether file exists on disk
static int
s_SQLITE_FS_Access(sqlite3_vfs*, const char *name, int flags, int *result)
{
    _ASSERT(flags == SQLITE_ACCESS_EXISTS);
    if (CNCFileSystem::IsDiskInitialized()) {
        *result = 0;
        return SQLITE_OK;
    }
    else {
        sqlite3_vfs* vfs = s_GetRealVFS();
        return vfs->xAccess(vfs, name, flags, result);
    }
}

/// Open file
static int
s_SQLITE_FS_Open(sqlite3_vfs*,
                 const char*   name,
                 sqlite3_file* file,
                 int           flags,
                 int*          out_flags)
{
    return static_cast<CNCFSVirtFile*>(file)->Open(name, flags, out_flags);
}

/// Close the file
static int
s_SQLITE_FS_Close(sqlite3_file* file)
{
    return static_cast<CNCFSVirtFile*>(file)->Close();
}

/// Read data from the file
static int
s_SQLITE_FS_Read(sqlite3_file* file,
                 void*         mem_ptr,
                 int           cnt,
                 sqlite3_int64 offset)
{
    return static_cast<CNCFSVirtFile*>(file)->Read(offset, cnt, mem_ptr);
}

/// Write data to the file
static int
s_SQLITE_FS_Write(sqlite3_file* file,
                  const void*   data,
                  int           cnt,
                  sqlite3_int64 offset)
{
    return static_cast<CNCFSVirtFile*>(file)->Write(offset, data, cnt);
}

/// Delete file (used only for journal)
static int
s_SQLITE_FS_Delete(sqlite3_vfs*, const char *name, int sync_dir)
{
    sqlite3_vfs* vfs = s_GetRealVFS();
    return vfs->xDelete(vfs, name, sync_dir);
}

/// Read size of the file
static int
s_SQLITE_FS_Size(sqlite3_file* file, sqlite3_int64 *size)
{
    *size = static_cast<CNCFSVirtFile*>(file)->GetSize();
    return SQLITE_OK;
}

/// Place a lock on the file
static int
s_SQLITE_FS_Lock(sqlite3_file* file, int lock_type)
{
    return static_cast<CNCFSVirtFile*>(file)->Lock(lock_type);
}

/// Release lock from the file
static int
s_SQLITE_FS_Unlock(sqlite3_file* file, int lock_type)
{
    return static_cast<CNCFSVirtFile*>(file)->Unlock(lock_type);
}

/// Check if file is locked with RESERVED lock
static int
s_SQLITE_FS_IsReserved(sqlite3_file* file, int* result)
{
    *result = static_cast<CNCFSVirtFile*>(file)->IsReserved();
    return SQLITE_OK;
}

/// Generate random data
static int
s_SQLITE_FS_Random(sqlite3_vfs*, int bytes_cnt, char *out_str)
{
    sqlite3_vfs* vfs = s_GetRealVFS();
    return vfs->xRandomness(vfs, bytes_cnt, out_str);
}

/// Get current OS time
static int
s_SQLITE_FS_CurTime(sqlite3_vfs*, double* time_ptr)
{
    sqlite3_vfs* vfs = s_GetRealVFS();
    return vfs->xCurrentTime(vfs, time_ptr);
}

/// Get information about device which VFS works with
static int
s_SQLITE_FS_AboutDevice(sqlite3_file*)
{
    return SQLITE_IOCAP_ATOMIC32K | SQLITE_IOCAP_SAFE_APPEND
                                  | SQLITE_IOCAP_SEQUENTIAL;
}

/// Synchronize the file with disk.
/// Although NetCache uses synchronous = OFF for all its operations SQLite
/// still needs this function for some reason.
static int
s_SQLITE_FS_Sync(sqlite3_file*, int)
{
    return SQLITE_OK;
}

END_NCBI_SCOPE
