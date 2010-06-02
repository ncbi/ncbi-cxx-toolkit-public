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
#include "nc_stat.hpp"
#include "error_codes.hpp"


BEGIN_NCBI_SCOPE

#define NCBI_USE_ERRCODE_X   NetCache_Storage

/*
static const char* kNCBlobKeys_Table          = "NCB";
static const char* kNCBlobKeys_BlobIdCol      = "id";
static const char* kNCBlobKeys_KeyCol         = "key";
static const char* kNCBlobKeys_SubkeyCol      = "skey";
static const char* kNCBlobKeys_VersionCol     = "ver";
*/
static const char* kNCBlobInfo_Table          = "NCF";
static const char* kNCBlobInfo_BlobIdCol      = "id";
static const char* kNCBlobInfo_KeyCol         = "k";
static const char* kNCBlobInfo_CreateTimeCol  = "ct";
static const char* kNCBlobInfo_DeadTimeCol    = "dt";
static const char* kNCBlobInfo_TTLCol         = "tl";
static const char* kNCBlobInfo_SizeCol        = "sz";
static const char* kNCBlobInfo_CntReadsCol    = "rd";
static const char* kNCBlobInfo_PasswordCol    = "pw";
static const char* kNCBlobInfo_DataIdCol      = "di";
static const char* kNCBlobInfo_OldStyleCol    = "os";
static const char* kNCBlobInfo_GenerationCol  = "g";
/*
static const char* kNCOldBlobInfo_Table         = "NCI";
static const char* kNCOldBlobInfo_BlobIdCol     = "id";
static const char* kNCOldBlobInfo_CreateTimeCol = "at";
static const char* kNCOldBlobInfo_DeadTimeCol   = "dt";
static const char* kNCOldBlobInfo_OwnerCol      = "own";
static const char* kNCOldBlobInfo_TTLCol        = "ttl";
static const char* kNCOldBlobInfo_SizeCol       = "sz";
static const char* kNCOldBlobInfo_CntReadsCol   = "rd";
static const char* kNCOldBlobInfo_PasswordCol   = "pw";
*/
static const char* kNCBlobChunks_Table        = "NCC";
static const char* kNCBlobChunks_ChunkIdCol   = "id";
static const char* kNCBlobChunks_BlobIdCol    = "bid";

static const char* kNCBlobData_Table          = "NCD";
static const char* kNCBlobData_ChunkIdCol     = "id";
static const char* kNCBlobData_BlobCol        = "blob";

static const char* kNCDBIndex_Table           = "NCX";
static const char* kNCDBIndex_FileIdCol       = "id";
static const char* kNCDBIndex_FileNameCol     = "nm";
static const char* kNCDBIndex_FileTypeCol     = "tp";
static const char* kNCDBIndex_CreatedTimeCol  = "tm";
/*
static const char* kNCOldDBIndex_Table          = "NCN";
static const char* kNCOldDBIndex_DBIdCol        = "id";
static const char* kNCOldDBIndex_MetaNameCol    = "met";
static const char* kNCOldDBIndex_DataNameCol    = "dat";
static const char* kNCOldDBIndex_CreatedTimeCol = "tm";
static const char* kNCOldDBIndex_VolumesCol     = "vlm";
*/

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
extern "C" {

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

};  // extern "C"

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



inline void
CNCDBFile::x_CreateIndexDatabase(void)
{
    CQuickStrStream sql;
    CSQLITE_Statement  stmt(this);

    sql.Clear();
    sql << "create table if not exists " << kNCDBIndex_Table
        << "(" << kNCDBIndex_FileIdCol      << " integer primary key,"
               << kNCDBIndex_FileNameCol    << " varchar not null,"
               << kNCDBIndex_FileTypeCol    << " int not null,"
               << kNCDBIndex_CreatedTimeCol << " int not null"
        << ")";
    stmt.SetSql(sql);
    stmt.Execute();
}

inline void
CNCDBFile::x_CreateMetaDatabase(void)
{
    CQuickStrStream sql;
    CSQLITE_Statement stmt(this);

    stmt.SetSql("BEGIN");
    stmt.Execute();

    sql.Clear();
    sql << "create table " << kNCBlobInfo_Table
        << "(" << kNCBlobInfo_BlobIdCol     << " integer primary key,"
               << kNCBlobInfo_KeyCol        << " varchar not null,"
               << kNCBlobInfo_DataIdCol     << " int not null,"
               << kNCBlobInfo_CreateTimeCol << " int not null,"
               << kNCBlobInfo_DeadTimeCol   << " int not null,"
               << kNCBlobInfo_TTLCol        << " int not null,"
               << kNCBlobInfo_SizeCol       << " int64 not null,"
               << kNCBlobInfo_PasswordCol   << " varchar,"
               << kNCBlobInfo_OldStyleCol   << " int not null default 0,"
               << kNCBlobInfo_GenerationCol << " int not null,"
               << kNCBlobInfo_CntReadsCol   << " int not null,"
               << "unique(" << kNCBlobInfo_DeadTimeCol << ","
                            << kNCBlobInfo_BlobIdCol
               <<       "),"
               << "unique(" << kNCBlobInfo_KeyCol << ")"
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

inline void
CNCDBFile::x_CreateDataDatabase(void)
{
    CQuickStrStream sql;
    CSQLITE_Statement  stmt(this);

    sql.Clear();
    sql << "create table " << kNCBlobData_Table
        << "(" << kNCBlobData_ChunkIdCol << " integer primary key,"
               << kNCBlobData_BlobCol    << " blob not null"
           ")";
    stmt.SetSql(sql);
    stmt.Execute();
}

void
CNCDBFile::CreateDatabase(ENCDBFileType db_type)
{
    switch (db_type) {
    case eNCIndex:
        x_CreateIndexDatabase();
        break;
    case eNCMeta:
        x_CreateMetaDatabase();
        break;
    case eNCData:
        x_CreateDataDatabase();
        break;
    default:
        _ASSERT(false);
    }
}

CSQLITE_Statement*
CNCDBFile::x_GetStatement(ENCStmtType typ)
{
    TStatementPtr& stmt = m_Stmts[typ];
    if (!stmt) {
        CQuickStrStream sql;

        switch (typ) {
        case eStmt_CreateDBFile:
            sql << "insert into " << kNCDBIndex_Table
                << "(" << kNCDBIndex_FileIdCol      << ","
                       << kNCDBIndex_FileNameCol    << ","
                       << kNCDBIndex_FileTypeCol    << ","
                       << kNCDBIndex_CreatedTimeCol
                << ")values(?1,?2,?3,?4)";
            break;
        case eStmt_DeleteDBFile:
            sql << "delete from " << kNCDBIndex_Table
                << " where " << kNCDBIndex_FileIdCol << "=?1";
            break;
        case eStmt_BlobFamilyExists:
            sql << "select count(*)"
                <<  " from " << kNCBlobInfo_Table
                << " where " << kNCBlobInfo_KeyCol    << ">=?1"
                <<   " and " << kNCBlobInfo_KeyCol    << "<?2"
                <<   " and " << kNCBlobInfo_DeadTimeCol << ">=?3";
            break;
        case eStmt_GetBlobsList:
            sql << "select " << kNCBlobInfo_BlobIdCol   << ","
                             << kNCBlobInfo_DataIdCol   << ","
                             << kNCBlobInfo_DeadTimeCol << ","
                             << kNCBlobInfo_KeyCol      << ","
                             << kNCBlobInfo_SizeCol     << ","
                             << kNCBlobInfo_CntReadsCol
                <<  " from " << kNCBlobInfo_Table
                << " where "  << kNCBlobInfo_DeadTimeCol  << ">=?1"
                <<   " and "  << kNCBlobInfo_DeadTimeCol  << "<?3"
                <<   " and (" << kNCBlobInfo_DeadTimeCol  << ">?1"
                <<        " or " << kNCBlobInfo_BlobIdCol << ">?2)"
                << " order by 3,1"
                << " limit ?4";
            break;
        case eStmt_GetBlobShortInfo:
            sql << "select " << kNCBlobInfo_BlobIdCol   << ","
                             << kNCBlobInfo_DataIdCol   << ","
                             << kNCBlobInfo_SizeCol     << ","
                             << kNCBlobInfo_CntReadsCol
                <<  " from " << kNCBlobInfo_Table
                << " where " << kNCBlobInfo_KeyCol      << "=?1"
                <<   " and " << kNCBlobInfo_DeadTimeCol << ">=?2";
            break;
        case eStmt_WriteBlobInfo:
            sql << "insert or replace into " << kNCBlobInfo_Table
                << "(" << kNCBlobInfo_BlobIdCol     << ","
                       << kNCBlobInfo_DataIdCol     << ","
                       << kNCBlobInfo_KeyCol        << ","
                       << kNCBlobInfo_CreateTimeCol << ","
                       << kNCBlobInfo_DeadTimeCol   << ","
                       << kNCBlobInfo_TTLCol        << ","
                       << kNCBlobInfo_SizeCol       << ","
                       << kNCBlobInfo_CntReadsCol   << ","
                       << kNCBlobInfo_PasswordCol   << ","
                       << kNCBlobInfo_GenerationCol
                << ")values(?1,?2,?3,?4,?5,?6,?7,?8,?9,?10)";
            break;
        case eStmt_UpdateBlobInfo:
            sql << "update " << kNCBlobInfo_Table
                <<   " set " << kNCBlobInfo_DeadTimeCol << "=?2,"
                             << kNCBlobInfo_CntReadsCol << "=?3"
                << " where " << kNCBlobInfo_BlobIdCol << "=?1";
            break;
        case eStmt_ReadBlobInfo:
            sql << "select " << kNCBlobInfo_DataIdCol     << ","
                             << kNCBlobInfo_CreateTimeCol << ","
                             << kNCBlobInfo_DeadTimeCol   << ","
                             << kNCBlobInfo_TTLCol        << ","
                             << kNCBlobInfo_SizeCol       << ","
                             << kNCBlobInfo_OldStyleCol   << ","
                             << kNCBlobInfo_CntReadsCol   << ","
                             << kNCBlobInfo_PasswordCol   << ","
                             << kNCBlobInfo_GenerationCol
                <<  " from " << kNCBlobInfo_Table
                << " where " << kNCBlobInfo_BlobIdCol << "=?1";
            break;
        case eStmt_DeleteBlobInfo:
            sql << "delete from " << kNCBlobInfo_Table
                << " where " << kNCBlobInfo_BlobIdCol << "=?1";
            break;
        case eStmt_GetChunkIds:
            sql << "select id from " << kNCBlobChunks_Table
                << " where " << kNCBlobChunks_BlobIdCol << "=?1"
                << " order by id";
            break;
        case eStmt_CreateChunk:
            sql << "insert into " << kNCBlobChunks_Table
                << "(" << kNCBlobChunks_ChunkIdCol << ","
                       << kNCBlobChunks_BlobIdCol
                << ")values(?1,?2)";
            break;
        case eStmt_WriteChunkData:
            sql << "insert into " << kNCBlobData_Table
                << "(" << kNCBlobData_ChunkIdCol << ","
                       << kNCBlobData_BlobCol
                << ")values(?1,?2)";
            break;
        case eStmt_ReadChunkData:
            sql << "select " << kNCBlobData_BlobCol
                <<  " from " << kNCBlobData_Table
                << " where " << kNCBlobData_ChunkIdCol << "=?1";
            break;
        default:
            NCBI_TROUBLE("Unknown statement type");
        }

        stmt = new CSQLITE_Statement(this, sql);
    }
    return stmt.get();
}

void
CNCDBFile::NewDBFile(ENCDBFileType  file_type,
                     TNCDBFileId    file_id,
                     const string&  file_name)
{
    CSQLITE_StatementLock stmt(x_GetStatement(eStmt_CreateDBFile));

    stmt->Bind(1, file_id);
    stmt->Bind(2, file_name.data(), file_name.size());
    stmt->Bind(3, file_type);
    stmt->Bind(4, int(time(NULL)));
    stmt->Execute();
}

void
CNCDBFile::DeleteDBFile(TNCDBFileId file_id)
{
    CSQLITE_StatementLock stmt(x_GetStatement(eStmt_DeleteDBFile));

    stmt->Bind(1, file_id);
    stmt->Execute();
}

void
CNCDBFile::GetAllDBFiles(TNCDBFilesMap* files_map)
{
    _ASSERT(files_map);

    CQuickStrStream sql;
    sql << "select " << kNCDBIndex_FileIdCol      << ","
                     << kNCDBIndex_FileTypeCol    << ","
                     << kNCDBIndex_FileNameCol    << ","
                     << kNCDBIndex_CreatedTimeCol
        <<  " from " << kNCDBIndex_Table
        << " order by " << kNCDBIndex_CreatedTimeCol;

    CSQLITE_Statement stmt(this, sql);
    while (stmt.Step()) {
        TNCDBFileId    file_id   = stmt.GetInt(0);
        ENCDBFileType  file_type = ENCDBFileType(stmt.GetInt(1));
        SNCDBFileInfo& info      = (*files_map)[file_id];
        info.file_id = file_id;
        info.file_name   = stmt.GetString(2);
        info.create_time = stmt.GetInt   (3);
        switch (file_type) {
        case eNCMeta:
            info.file_obj = new CNCDBMetaFile(info.file_name);
            break;
        case eNCData:
            info.file_obj = new CNCDBDataFile(info.file_name);
            break;
        default:
            _ASSERT(false);
        }
    }
}

void
CNCDBFile::DeleteAllDBFiles(void)
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
    sql << "select max(" << kNCBlobInfo_BlobIdCol << ")"
        << " from " << kNCBlobInfo_Table;

    CSQLITE_Statement stmt(this, sql);
    _VERIFY(stmt.Step());
    return stmt.GetInt(0);
}

bool
CNCDBFile::IsBlobFamilyExists(const string& key_first,
                              const string& key_last,
                              int           dead_after)
{
    CSQLITE_StatementLock stmt(x_GetStatement(eStmt_BlobFamilyExists));

    stmt->Bind(1, key_first.data(), key_first.size());
    stmt->Bind(2, key_last.data(),  key_last.size());
    stmt->Bind(3, dead_after);
    _VERIFY(stmt->Step());

    return stmt->GetInt8(0) > 0;
}

void
CNCDBFile::GetBlobsList(int&            dead_after,
                        TNCBlobId&      id_after,
                        int             dead_before,
                        int             max_count,
                        TNCBlobsList*   blobs_list)
{
    _ASSERT(blobs_list);
    blobs_list->clear();

    CSQLITE_StatementLock stmt(x_GetStatement(eStmt_GetBlobsList));

    stmt->Bind(1, dead_after);
    stmt->Bind(2, id_after);
    stmt->Bind(3, dead_before);
    stmt->Bind(4, max_count);
    while (stmt->Step()) {
        SNCBlobShortInfo info;
        info.blob_id = id_after = stmt->GetInt   (0);
        info.data_id            = stmt->GetInt   (1);
        dead_after              = stmt->GetInt   (2);
        info.key                = stmt->GetString(3);
        info.size               = stmt->GetInt8  (4);
        info.cnt_reads          = stmt->GetInt8  (5);
        if (info.data_id != 0  &&  !info.key.empty()) {
            blobs_list->push_back(info);
        }
    }
}

void
CNCDBFile::WriteBlobInfo(const string& blob_key, const SNCBlobVerData* blob_info)
{
    CSQLITE_StatementLock stmt(x_GetStatement(eStmt_WriteBlobInfo));

    stmt->Bind(1,  blob_info->coords.blob_id);
    stmt->Bind(2,  blob_info->coords.data_id);
    stmt->Bind(3,  blob_key.data(), blob_key.size());
    stmt->Bind(4,  blob_info->create_time);
    stmt->Bind(5,  blob_info->dead_time);
    stmt->Bind(6,  blob_info->ttl);
    stmt->Bind(7,  blob_info->size);
    stmt->Bind(8,  blob_info->cnt_reads);
    stmt->Bind(9,  blob_info->password.data(), blob_info->password.size());
    stmt->Bind(10, blob_info->generation);
    stmt->Execute();
}

void
CNCDBFile::UpdateBlobInfo(const SNCBlobVerData* blob_info)
{
    CSQLITE_StatementLock stmt(x_GetStatement(eStmt_UpdateBlobInfo));

    stmt->Bind(1, blob_info->coords.blob_id);
    stmt->Bind(2, blob_info->dead_time);
    stmt->Bind(3, blob_info->cnt_reads);
    stmt->Execute();
}

bool
CNCDBFile::ReadBlobInfo(SNCBlobVerData* blob_info)
{
    _ASSERT(blob_info);

    CSQLITE_StatementLock stmt(x_GetStatement(eStmt_ReadBlobInfo));

    stmt->Bind(1, blob_info->coords.blob_id);
    if (stmt->Step()) {
        blob_info->coords.data_id = stmt->GetInt (0);
        blob_info->create_time    = stmt->GetInt8(1);
        blob_info->dead_time      = stmt->GetInt (2);
        blob_info->old_dead_time  = blob_info->dead_time;
        blob_info->ttl            = stmt->GetInt (3);
        blob_info->size           = stmt->GetInt8(4);
        blob_info->disk_size      = blob_info->size;
        blob_info->old_style      = stmt->GetInt (5) != 0;
        blob_info->cnt_reads      = stmt->GetInt8(6);
        blob_info->password       = stmt->GetString(7);
        blob_info->generation     = Uint8(stmt->GetInt8(8));
        return true;
    }
    return false;
}

void
CNCDBFile::DeleteBlobInfo(TNCBlobId blob_id)
{
    CSQLITE_StatementLock stmt(x_GetStatement(eStmt_DeleteBlobInfo));

    stmt->Bind(1, blob_id);
    stmt->Execute();
}

void
CNCDBFile::DeleteAllBlobInfos(const string& min_key, const string& max_key)
{
    CQuickStrStream sql;
    sql << "delete from " << kNCBlobInfo_Table
        << " where " << kNCBlobInfo_KeyCol << ">=?1"
        <<   " and " << kNCBlobInfo_KeyCol << "<?2";
    CSQLITE_Statement stmt(this, sql);
    stmt.Bind(1, min_key.data(), min_key.size());
    stmt.Bind(2, max_key.data(), max_key.size());
    _VERIFY(!stmt.Step());
}

bool
CNCDBFile::GetBlobShortInfo(const string&     blob_key,
                            int               dead_after,
                            SNCBlobShortInfo* blob_info)
{
    CSQLITE_StatementLock stmt(x_GetStatement(eStmt_GetBlobShortInfo));

    stmt->Bind(1, blob_key.data(), blob_key.size());
    stmt->Bind(2, dead_after);
    if (stmt->Step()) {
        blob_info->blob_id   = stmt->GetInt (0);
        blob_info->data_id   = stmt->GetInt (1);
        blob_info->size      = stmt->GetInt8(2);
        blob_info->cnt_reads = stmt->GetInt8(3);
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

    stmt->Bind(1, blob_id);
    while (stmt->Step()) {
        id_list->push_back(stmt->GetInt8(0));
    }
}

void
CNCDBFile::CreateChunk(TNCBlobId blob_id, TNCChunkId chunk_id)
{
    CSQLITE_StatementLock stmt(x_GetStatement(eStmt_CreateChunk));

    stmt->Bind(1, chunk_id);
    stmt->Bind(2, blob_id);
    stmt->Execute();
}

void
CNCDBFile::WriteChunkData(TNCChunkId chunk_id, const CNCBlobBuffer* data)
{
    CSQLITE_StatementLock stmt(x_GetStatement(eStmt_WriteChunkData));

    stmt->Bind(1, chunk_id);
    stmt->Bind(2, static_cast<const void*>(data->GetData()), data->GetSize());
    stmt->Execute();
}

bool
CNCDBFile::ReadChunkData(TNCChunkId chunk_id, CNCBlobBuffer* buffer)
{
    CSQLITE_StatementLock stmt(x_GetStatement(eStmt_ReadChunkData));

    stmt->Bind(1, chunk_id);
    if (stmt->Step()) {
        size_t n_read = stmt->GetBlob(0, buffer->GetData(), kNCMaxBlobChunkSize);
        buffer->Resize(n_read);
        return true;
    }
    return false;
}

CNCDBFile::~CNCDBFile(void)
{
    m_Stmts.clear();
}


CNCDBIndexFile::~CNCDBIndexFile(void)
{}


CNCDBMetaFile::~CNCDBMetaFile(void)
{}


CNCDBDataFile::~CNCDBDataFile(void)
{}



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
    // several parallel adjustments, and we don't need any mutexes.
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
        CNCMemManager::SetDBPageDirty(data);
        CNCMemManager::LockDBPage(data);
    }
    event->data = data;
    x_AddNewEvent(event);
    file->AdjustSize(offset + cnt);
}

inline void
CNCFileSystem::x_DoWriteToFile(SNCFSEvent* event)
{
    if (event->offset >= kNCSQLitePageSize) {
        if (!CNCMemManager::IsDBPageDirty(event->data)) {
            CNCMemManager::UnlockDBPage(event->data);
            return;
        }
        CNCMemManager::SetDBPageClean(event->data);
    }
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


extern "C" {

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

};  // extern "C"

END_NCBI_SCOPE
