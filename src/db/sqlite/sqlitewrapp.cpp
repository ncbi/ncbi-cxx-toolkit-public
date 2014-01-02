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
 * Authors:  Pavel Ivanov
 *
 * File Description:
 *   C++ wrapper for working with SQLite 3.6.14 or higher.
 */

#include <ncbi_pch.hpp>

#include <corelib/ncbidbg.hpp>
#include <corelib/ncbistr.hpp>
#include <corelib/ncbifile.hpp>

#include <db/sqlite/sqlitewrapp.hpp>

#include <db/error_codes.hpp>


BEGIN_NCBI_SCOPE


#define NCBI_USE_ERRCODE_X  Db_Sqlite


/// Default size of database pages (maximum allowed in SQLite)
const int kSQLITE_DefPageSize = 32768;



#define SQLITE_ERRMSG_STRM(handle, msg)  \
    msg << ": [" << sqlite3_errcode(handle) << "] " << sqlite3_errmsg(handle)

/// Convenient macro for throwing CSQLITE_Exception containing error message
/// from SQLite.
///
/// @param type
///   Enum value from CSQLITE_Exception
/// @param handle
///   Connection handle where error happened
/// @param msg
///   Additional explanation of the error
#define SQLITE_THROW(type, handle, msg)                          \
    do {                                                         \
        CNcbiOstrstream ostr;                                    \
        ostr << SQLITE_ERRMSG_STRM(handle, msg);                 \
        throw CSQLITE_Exception(DIAG_COMPILE_INFO, NULL, type,   \
                                CNcbiOstrstreamToString(ostr));  \
    } while (0)

/// Same as SQLITE_THROW but for logging errors in places where throwing
/// exceptions is impossible.
///
/// @param err_subcode
///   Error subcode for err_posting
/// @param handle
///   Connection handle where error happened
/// @param msg
///   Additional explanation of the error
#define SQLITE_LOG_ERROR(err_subcode, handle, msg)               \
    ERR_POST_X(err_subcode,                                      \
               SQLITE_ERRMSG_STRM(handle, msg))


#ifdef HAVE_SQLITE3ASYNC_H

/// Background thread for executing all asynchronous writes to database
/// if they exist.
class CSQLITE_AsyncWritesThread : public CThread
{
protected:
    virtual void* Main(void)
    {
        sqlite3async_run();
        return NULL;
    }
};

#endif  // HAVE_SQLITE3ASYNC_H

#ifdef HAVE_SQLITE3_UNLOCK_NOTIFY

/// Class helping make smart waits when database is locked by another thread
class CLockWaiter
{
public:
    CLockWaiter(void) : m_WaitSem(new CSemaphore(0, 1))
    {}
    /// Wait for the database to unlock
    void Wait(void)
    {
        m_WaitSem->Wait();
    }
    /// Signal the thread that waits inside Wait() method that database was
    /// unlocked
    void SignalUnlock(void)
    {
        m_WaitSem->Post();
    }

private:
    /// Semaphore to wait on. Made auto-pointer to allow this object to be
    /// used in stl containers.
    AutoPtr<CSemaphore> m_WaitSem;
};


/// Callback that will be called from sqlite3 when database is unlocked
static void
s_OnUnlockNotify(void** p_waiters, int n_waiters)
{
    for (int i = 0; i < n_waiters; ++i) {
        CLockWaiter* waiter = reinterpret_cast<CLockWaiter*>(p_waiters[i]);
        waiter->SignalUnlock();
    }
}


/// Set of all waiting helpers assigned to different sqlite3 connections.
/// This is needed to avoid construction/destruction of semaphores on each
/// wait for database to unlock.
static map<sqlite3*, CLockWaiter> s_LockWaiters;
/// Mutex for working with s_LockWaiters
DEFINE_STATIC_FAST_MUTEX(s_LockWaitersMutex);

#endif  // HAVE_SQLITE3_UNLOCK_NOTIFY


/// Unified handling of return codes from sqlite. Throw exception if
/// return code indicates some error, return SQLITE_BUSY if last operation
/// should be retried and return sqlite3_code itself if it is normal and
/// does not indicate any error.
static int
s_ProcessErrorCode(sqlite3*                    handle,
                   int                         sqlite3_code,
                   CSQLITE_Exception::EErrCode ex_code)
{
    switch (sqlite3_code) {
    case SQLITE_OK:
    case SQLITE_DONE:
    case SQLITE_ROW:
        break;
    case SQLITE_BUSY:
        SleepMilliSec(1);
        break;
#ifdef HAVE_SQLITE3_UNLOCK_NOTIFY
    case SQLITE_LOCKED:
    case SQLITE_LOCKED_SHAREDCACHE:
        CLockWaiter* waiter;
        int ret;
        {{
            CFastMutexGuard guard(s_LockWaitersMutex);
            waiter = &s_LockWaiters[handle];
        }}
        ret = sqlite3_unlock_notify(handle, &s_OnUnlockNotify, waiter);
        if (ret == SQLITE_LOCKED) {
            SQLITE_THROW(CSQLITE_Exception::eDeadLock, handle,
                         "Database is deadlocked");
        }
        else {
            waiter->Wait();
        }
        sqlite3_code = SQLITE_BUSY;
        break;
#endif  // HAVE_SQLITE3_UNLOCK_NOTIFY
    case SQLITE_CONSTRAINT:
        SQLITE_THROW(CSQLITE_Exception::eConstraint, handle,
                     "Constraint violation in statement");
    default:
        SQLITE_THROW(ex_code, handle, "Error from sqlite3");
    }

    return sqlite3_code;
}


/// Make call to sqlite3 function with automatic retry mechanism.
/// Result from call will be in sql_ret variable after macro.
///
/// @param call
///   Function along with its parameters to call
/// @param conn_handle
///   SQLite connection handle that will be used in call
/// @param ex_type
///   Enum value from CSQLITE_Exception which will be thrown in case of
///   an error
/// @param err_msg
///   Error explanation to throw in case of an error
/// @param retry
///   Additional function call to make between retries
#define SQLITE_SAFE_CALL_EX(call, conn_handle, ex_type, err_msg, retry)  \
    int sql_ret = SQLITE_ERROR;                                          \
    do {                                                                 \
        try {                                                            \
            sql_ret = call;                                              \
            sql_ret = s_ProcessErrorCode(conn_handle, sql_ret,           \
                                         CSQLITE_Exception::ex_type);    \
            if (sql_ret == SQLITE_BUSY) {                                \
                retry;                                                   \
            }                                                            \
        }                                                                \
        catch (CSQLITE_Exception& ex) {                                  \
            CNcbiOstrstream ostr;                                        \
            ostr << err_msg;                                             \
            NCBI_RETHROW_SAME(ex, CNcbiOstrstreamToString(ostr));        \
        }                                                                \
    }                                                                    \
    while (sql_ret == SQLITE_BUSY)

/// The same as SQLITE_SAFE_CALL_EX but without additional calls between
/// retries.
#define SQLITE_SAFE_CALL(sqlite_call, conn_handle, ex_type, err_msg)     \
    SQLITE_SAFE_CALL_EX((sqlite_call), conn_handle, ex_type, err_msg,    \
                        (void)0)



const char*
CSQLITE_Exception::GetErrCodeString(void) const
{
    switch (GetErrCode())
    {
    case eUnknown:      return "eUnknown";
    case eDBOpen:       return "eDBOpen";
    case eStmtPrepare:  return "eStmtPrepare";
    case eStmtFinalize: return "eStmtFinalize";
    case eStmtBind:     return "eStmtBind";
    case eStmtStep:     return "eStmtStep";
    case eStmtReset:    return "eStmtReset";
    case eBlobOpen:     return "eBlobOpen";
    case eBlobClose:    return "eBlobClose";
    case eBlobRead:     return "eBlobRead";
    case eBlobWrite:    return "eBlobWrite";
    default:            return CException::GetErrCodeString();
    }
}


#ifdef HAVE_SQLITE3ASYNC_H

/// Background thread for executing all asynchronous writings to databases
static CRef<CThread> s_AsyncWritesThread;

void
CSQLITE_Global::RunAsyncWritesThread(void)
{
    if (s_AsyncWritesThread.IsNull()) {
        s_AsyncWritesThread = new CSQLITE_AsyncWritesThread();
        s_AsyncWritesThread->Run();
    }
}

void
CSQLITE_Global::SetAsyncWritesFileLocking(bool lock_files)
{
    if (sqlite3async_control(SQLITEASYNC_LOCKFILES, int(lock_files))
        != SQLITE_OK)
    {
        ERR_POST_X(7, "File locking for asynchronous writing mode is not set "
                      "because of an error");
    }
}

#endif  // HAVE_SQLITE3ASYNC_H

void
CSQLITE_Global::Finalize(void)
{
#ifdef HAVE_SQLITE3ASYNC_H
    _VERIFY(sqlite3async_control(SQLITEASYNC_HALT, SQLITEASYNC_HALT_IDLE)
                                                                == SQLITE_OK);
    if (s_AsyncWritesThread.NotNull()) {
        s_AsyncWritesThread->Join();
    }
    sqlite3async_shutdown();
#endif

    _VERIFY(sqlite3_shutdown() == SQLITE_OK);
}

void
CSQLITE_Global::Initialize(void)
{
    _VERIFY(sqlite3_config(SQLITE_CONFIG_MEMSTATUS, 0) == SQLITE_OK);
    _VERIFY(sqlite3_initialize()                       == SQLITE_OK);
#ifdef HAVE_SQLITE3ASYNC_H
    _VERIFY(sqlite3async_initialize(NULL, 0)           == SQLITE_OK);
    _VERIFY(sqlite3async_control(SQLITEASYNC_HALT, SQLITEASYNC_HALT_NEVER)
                                                       == SQLITE_OK);
#endif
}

void
CSQLITE_Global::EnableSharedCache(bool enable /* = true */)
{
#if defined(NCBI_THREADS) && !defined(HAVE_SQLITE3_UNLOCK_NOTIFY)
    if (enable) {
        NCBI_THROW(CSQLITE_Exception, eBadCall,
                   "Cannot turn on shared cache because of lack of capabilities");
    }
#else
    if (sqlite3_enable_shared_cache(enable) != SQLITE_OK) {
        ERR_POST_X(9, "Setting for sharing cache is not set because of an error");
    }
#endif
}

void
CSQLITE_Global::SetCustomPageCache(sqlite3_pcache_methods* methods)
{
    int res = sqlite3_config(SQLITE_CONFIG_PCACHE, methods);
    if (res != SQLITE_OK) {
        NCBI_THROW_FMT(CSQLITE_Exception, eBadCall,
                       "Custom page cache is not set, err_code = " << res);
    }
}

void
CSQLITE_Global::SetCustomMallocFuncs(sqlite3_mem_methods* methods)
{
    int res = sqlite3_config(SQLITE_CONFIG_MALLOC, methods);
    if (res != SQLITE_OK) {
        NCBI_THROW_FMT(CSQLITE_Exception, eBadCall,
                       "Custom malloc functions are not set, err_code = " << res);
    }
}

sqlite3_vfs*
CSQLITE_Global::GetDefaultVFS(void)
{
    return sqlite3_vfs_find(NULL);
}

void
CSQLITE_Global::RegisterCustomVFS(sqlite3_vfs* vfs, bool set_default /*= true*/)
{
    int res = sqlite3_vfs_register(vfs, set_default);
    if (res != SQLITE_OK) {
        NCBI_THROW_FMT(CSQLITE_Exception, eBadCall,
                       "Custom VFS is not registered, err_code = " << res);
    }
}



CSQLITE_HandleFactory::CSQLITE_HandleFactory(CSQLITE_Connection* conn)
    : m_Conn(conn)
{}

sqlite3*
CSQLITE_HandleFactory::CreateObject(void)
{
    sqlite3* result = NULL;
    const char* vfs = NULL;
#ifdef HAVE_SQLITE3ASYNC_H
    if ((m_Conn->GetFlags() & CSQLITE_Connection::eAllWrites)
                                        == CSQLITE_Connection::fWritesAsync)
    {
        vfs = SQLITEASYNC_VFSNAME;
    }
#endif
    try {
        SQLITE_SAFE_CALL((sqlite3_open_v2(m_Conn->GetFileName().c_str(),
                                          &result,
                                          SQLITE_OPEN_READWRITE
                                                | SQLITE_OPEN_CREATE
                                                | SQLITE_OPEN_NOMUTEX,
                                          vfs)),
                         result, eDBOpen,
                         "Error opening database '"
                                          << m_Conn->GetFileName() << "'"
                        );
        _ASSERT(sql_ret == SQLITE_OK);
        m_Conn->SetupNewConnection(result);
        return result;
    }
    catch (CException&) {
        if (result)
            sqlite3_close(result);
        throw;
    }
}

void
CSQLITE_HandleFactory::DeleteObject(sqlite3* handle)
{
    int ret = sqlite3_close(handle);
    if (ret != SQLITE_OK) {
        SQLITE_LOG_ERROR(4, handle,
                         "Cannot close connection");

        while (ret == SQLITE_BUSY  ||  ret == SQLITE_LOCKED
#ifdef HAVE_SQLITE3_UNLOCK_NOTIFY
               ||  ret == SQLITE_LOCKED_SHAREDCACHE
#endif
              )
        {
            sqlite3_stmt *stmt;
            while( (stmt = sqlite3_next_stmt(handle, 0)) != NULL ) {
                ret = sqlite3_finalize(stmt);
                if (ret != SQLITE_OK) {
                    SQLITE_LOG_ERROR(1, handle,
                                     "Cannot finalize statement");
                }
            }

            ret = sqlite3_close(handle);
            if (ret != SQLITE_OK) {
                SQLITE_LOG_ERROR(5, handle,
                                 "Failed retry closing connection");
            }
        }
    }

    if (ret != SQLITE_OK) {
        SQLITE_LOG_ERROR(2, handle,
                         "Error closing database connection, "
                         "leaving it open");
    }
}



void
CSQLITE_Connection::x_CheckFlagsValidity(TOperationFlags flags,
                                         EOperationFlags mask)
{
    TOperationFlags fls = flags & mask;
    if (fls & (fls - 1)) {
        NCBI_THROW(CSQLITE_Exception, eWrongFlags,
                   "Incorrect flags in CSQLITE_Connection: 0x"
                   + NStr::IntToString(flags, 0, 16));
    }
}

inline void
CSQLITE_Connection::x_ExecuteSql(sqlite3* handle, CTempString sql)
{
    CSQLITE_Statement stmt(handle, sql);
    stmt.Execute();
}

CSQLITE_Connection::CSQLITE_Connection(CTempString     file_name,
                                       TOperationFlags flags/*=eDefaultFlags*/)
    : m_FileName(file_name),
      m_Flags(flags),
      m_PageSize(kSQLITE_DefPageSize),
      m_CacheSize(0),
      m_HandlePool(CSQLITE_HandleFactory(this))
{
    x_CheckFlagsValidity(flags, eAllMT);
    x_CheckFlagsValidity(flags, eAllVacuum);
    x_CheckFlagsValidity(flags, eAllJournal);
    x_CheckFlagsValidity(flags, eAllSync);
    x_CheckFlagsValidity(flags, eAllTemp);
    x_CheckFlagsValidity(flags, eAllWrites);
}

void
CSQLITE_Connection::SetupNewConnection(sqlite3* handle)
{
    sqlite3_extended_result_codes(handle, 1);

    if (m_Flags & fReadOnly) {
        // The database is read-only, do not execute any PRAGMA commands.
        return;
    }

    x_ExecuteSql(handle, "PRAGMA read_uncommitted = 1");
    x_ExecuteSql(handle, "PRAGMA count_changes = 0");
    x_ExecuteSql(handle, "PRAGMA legacy_file_format = OFF");
    x_ExecuteSql(handle, "PRAGMA page_size = "
                                            + NStr::IntToString(m_PageSize));
    x_ExecuteSql(handle, "PRAGMA cache_size = "
                                            + NStr::IntToString(m_CacheSize));

    switch (m_Flags & eAllTemp) {
    case fTempToMemory:
        x_ExecuteSql(handle, "PRAGMA temp_store = MEMORY");
        break;
    case fTempToFile:
        x_ExecuteSql(handle, "PRAGMA temp_store = FILE");
        break;
    default:
        // Evidently this will throw an exception
        x_CheckFlagsValidity(m_Flags, eAllTemp);
    }
    switch (m_Flags & eAllJournal) {
    case fJournalOff:
        x_ExecuteSql(handle, "PRAGMA journal_mode = OFF");
        break;
    case fJournalMemory:
        x_ExecuteSql(handle, "PRAGMA journal_mode = MEMORY");
        break;
    case fJournalPersist:
        x_ExecuteSql(handle, "PRAGMA journal_mode = PERSIST");
        break;
    case fJournalTruncate:
        x_ExecuteSql(handle, "PRAGMA journal_mode = TRUNCATE");
        break;
    case fJournalDelete:
        x_ExecuteSql(handle, "PRAGMA journal_mode = DELETE");
        break;
    default:
        // Evidently this will throw an exception
        x_CheckFlagsValidity(m_Flags, eAllJournal);
    }
    switch (m_Flags & eAllSync) {
    case fSyncOff:
        x_ExecuteSql(handle, "PRAGMA synchronous = OFF");
        break;
    case fSyncOn:
        x_ExecuteSql(handle, "PRAGMA synchronous = NORMAL");
        break;
    case fSyncFull:
        x_ExecuteSql(handle, "PRAGMA synchronous = FULL");
        break;
    default:
        // Evidently this will throw an exception
        x_CheckFlagsValidity(m_Flags, eAllSync);
    }
    switch (m_Flags & eAllVacuum) {
    case fVacuumOff:
        x_ExecuteSql(handle, "PRAGMA auto_vacuum = NONE");
        break;
    case fVacuumManual:
        x_ExecuteSql(handle, "PRAGMA auto_vacuum = INCREMENTAL");
        break;
    case fVacuumOn:
        x_ExecuteSql(handle, "PRAGMA auto_vacuum = FULL");
        break;
    default:
        // Evidently this will throw an exception
        x_CheckFlagsValidity(m_Flags, eAllSync);
    }
}

void
CSQLITE_Connection::SetFlags(TOperationFlags flags)
{
    if ((flags & eAllVacuum) != (m_Flags & eAllVacuum)) {
        NCBI_THROW(CSQLITE_Exception, eWrongFlags,
                   "Cannot change vacuuming flags after database creation");
    }

    m_Flags = flags;
    m_HandlePool.Clear();
}

CSQLITE_Statement*
CSQLITE_Connection::CreateVacuumStmt(size_t max_free_size)
{
    string sql("PRAGMA incremental_vacuum(");
    sql += NStr::UInt8ToString((max_free_size + kSQLITE_DefPageSize - 1)
                                  / kSQLITE_DefPageSize);
    sql += ")";

    return new CSQLITE_Statement(this, sql);
}

void
CSQLITE_Connection::DeleteDatabase(void)
{
    m_HandlePool.Clear();
    CFile(m_FileName).Remove();
    // For simplification this hard-coded name is taken from SQLite. If it
    // changed the name we would change it too.
    CFile(m_FileName + "-journal").Remove();
}



CSQLITE_Statement::~CSQLITE_Statement(void)
{
    try {
        x_Finalize();
    }
    STD_CATCH_ALL_X(3, "Error in x_Finalize()");

    if (m_ConnHandle  &&  m_Conn) {
        m_Conn->UnlockHandle(m_ConnHandle);
    }
}

void
CSQLITE_Statement::x_Prepare(CTempString sql)
{
    if (sql.empty()) {
        return;
    }

    if (!m_ConnHandle) {
        m_ConnHandle = m_Conn->LockHandle();
    }

    SQLITE_SAFE_CALL((sqlite3_prepare_v2(m_ConnHandle, sql.data(), int(sql.size()),
                                         &m_StmtHandle, NULL)),
                     m_ConnHandle, eStmtPrepare,
                     "Error preparing statement for \"" << sql << "\""
                    );
    _ASSERT(sql_ret == SQLITE_OK);
}

void
CSQLITE_Statement::x_Finalize(void)
{
    if (m_StmtHandle) {
        SQLITE_SAFE_CALL(sqlite3_finalize(m_StmtHandle),
                         m_ConnHandle, eStmtFinalize,
                         "Cannot finalize statement");
    }
}

#define STMT_BIND_IMPL(sql_type, str_type, index_and_val)           \
    do {                                                            \
        _ASSERT(m_StmtHandle);                                      \
        SQLITE_SAFE_CALL((NCBI_NAME2(sqlite3_bind_, sql_type)       \
                                   (m_StmtHandle, index_and_val)),  \
                         m_ConnHandle, eStmtBind,                   \
                         "Error binding " << #str_type              \
                            << " parameter N " << index             \
                        );                                          \
        _ASSERT(sql_ret == SQLITE_OK);                              \
    } while (0)                                                     \
/**/

#define COMMA ,

#define STMT_BIND_NO_VAL(sql_type, str_type, index)                 \
    STMT_BIND_IMPL(sql_type, str_type, index)

#define STMT_BIND(sql_type, str_type, index, val)                   \
    STMT_BIND_IMPL(sql_type, str_type, index COMMA val)

#define STMT_BIND3(sql_type, str_type, index, val1, val2, val3)     \
    STMT_BIND_IMPL(sql_type, str_type, index COMMA val1 COMMA val2 COMMA val3)


void
CSQLITE_Statement::Bind(int index, Int8 val)
{
    STMT_BIND(int64, Int8, index, val);
}

void
CSQLITE_Statement::Bind(int index, double val)
{
    STMT_BIND(double, double, index, val);
}

void
CSQLITE_Statement::Bind(int index, CTempString val)
{
    STMT_BIND3(text, string, index,
               val.data(), int(val.size()), SQLITE_TRANSIENT);
}

void
CSQLITE_Statement::Bind(int index, const char* data, size_t size)
{
    STMT_BIND3(text, static string, index, data, int(size), SQLITE_STATIC);
}

void
CSQLITE_Statement::Bind(int index, const void* data, size_t size)
{
    STMT_BIND3(blob, blob, index, data, int(size), SQLITE_STATIC);
}

void
CSQLITE_Statement::BindZeroedBlob(int index, size_t size)
{
    STMT_BIND(zeroblob, zeroed blob, index, int(size));
}

void
CSQLITE_Statement::BindNull(int index)
{
    STMT_BIND_NO_VAL(null, NULL, index);
}

bool
CSQLITE_Statement::Step(void)
{
    _ASSERT(m_StmtHandle);
    SQLITE_SAFE_CALL_EX(sqlite3_step(m_StmtHandle),
                        m_ConnHandle, eStmtStep,
                        "Error stepping through statement results",
                        sqlite3_reset(m_StmtHandle));
    _ASSERT(sql_ret == SQLITE_ROW  ||  sql_ret == SQLITE_DONE);
    return sql_ret == SQLITE_ROW;
}

void
CSQLITE_Statement::Reset(void)
{
    if (!m_StmtHandle) {
        return;
    }

    SQLITE_SAFE_CALL(sqlite3_reset(m_StmtHandle),
                     m_ConnHandle, eStmtStep,
                     "Error reseting statement");
    _ASSERT(sql_ret == SQLITE_OK);
}

void
CSQLITE_Statement::ClearBindings(void)
{
    if (!m_StmtHandle) {
        return;
    }

    SQLITE_SAFE_CALL(sqlite3_clear_bindings(m_StmtHandle),
                     m_ConnHandle, eStmtStep,
                     "Error clearing bindings");
    _ASSERT(sql_ret == SQLITE_OK);
}

CStringUTF8
CSQLITE_Statement::GetColumnName(int col_ind) const
{
    _ASSERT(m_StmtHandle);
    const char* result = sqlite3_column_name(m_StmtHandle, col_ind);
    if (!result) {
        SQLITE_THROW(CSQLITE_Exception::eUnknown, m_ConnHandle,
                     "Error requesting column name");
    }
    return CUtf8::AsUTF8(result, eEncoding_UTF8);
}

string
CSQLITE_Statement::GetString(int col_ind) const
{
    _ASSERT(m_StmtHandle);
    string result;
    const unsigned char* buf = sqlite3_column_text(m_StmtHandle, col_ind);
    size_t size = static_cast<size_t>(
                                sqlite3_column_bytes(m_StmtHandle, col_ind));
    result.append(reinterpret_cast<const char*>(buf), size);
    return result;
}

size_t
CSQLITE_Statement::GetBlobSize(int col_ind) const
{
    return (size_t)sqlite3_column_bytes(m_StmtHandle, col_ind);
}

size_t
CSQLITE_Statement::GetBlob(int col_ind, void* buffer, size_t size) const
{
    _ASSERT(m_StmtHandle);
    const void* buf = sqlite3_column_blob(m_StmtHandle, col_ind);
    size_t buf_size = static_cast<size_t>(
                                sqlite3_column_bytes(m_StmtHandle, col_ind));
    buf_size = min(buf_size, size);
    memcpy(buffer, buf, buf_size);
    return buf_size;
}


CSQLITE_StatementLock::~CSQLITE_StatementLock(void)
{
    try {
        m_Stmt->Reset();
    }
    catch (exception& ex) {
        ERR_POST("Error resetting statement: " << ex.what());
    }
}



/// Helper structure for opening the blob from CGuard<>
template <bool readwrite>
struct SSQLITE_BlobOpen
{
    void operator() (CSQLITE_Blob& blob)
    {
        blob.x_OpenBlob(readwrite);
    }
};

/// Helper structure for closing the blob from CGuard<>
struct SSQLITE_BlobClose
{
    void operator() (CSQLITE_Blob& blob)
    {
        blob.x_CloseBlob();
    }
};

/// Guard opening blob for read-only access at construction and closing it
/// at destruction.
typedef CGuard<CSQLITE_Blob,
               SSQLITE_BlobOpen<false>, SSQLITE_BlobClose>   TBlobReadGuard;
/// Guard opening blob for read-write access at construction and closing it
/// at destruction.
typedef CGuard<CSQLITE_Blob,
               SSQLITE_BlobOpen<true>,  SSQLITE_BlobClose>   TBlobWriteGuard;


#define BLOB_SAFE_CALL(blob_call, ex_type, err_msg)               \
    SQLITE_SAFE_CALL((blob_call), m_ConnHandle, ex_type,          \
                     err_msg << " " << m_Table << "." << m_Column \
                     << " where rowid = " << m_Rowid)


inline void
CSQLITE_Blob::x_Init(void)
{
    m_ConnHandle = NULL;
    m_BlobHandle = NULL;
    m_Size = 0;
    m_Position = 0;
}

CSQLITE_Blob::CSQLITE_Blob(CSQLITE_Connection* conn,
                           CTempString         table,
                           CTempString         column,
                           Int8                rowid)
    : m_Conn(conn),
      m_Database("main"),
      m_Table(table),
      m_Column(column),
      m_Rowid(rowid)
{
    x_Init();
}

CSQLITE_Blob::CSQLITE_Blob(CSQLITE_Connection* conn,
                           CTempString         db_name,
                           CTempString         table,
                           CTempString         column,
                           Int8                rowid)
    : m_Conn(conn),
      m_Database(db_name),
      m_Table(table),
      m_Column(column),
      m_Rowid(rowid)
{
    x_Init();
}

void
CSQLITE_Blob::x_OpenBlob(bool readwrite /* = false */)
{
    if (!m_ConnHandle) {
        m_ConnHandle = m_Conn->LockHandle();
    }

    BLOB_SAFE_CALL((sqlite3_blob_open(m_ConnHandle,
                                      m_Database.c_str(),
                                      m_Table.c_str(),
                                      m_Column.c_str(),
                                      m_Rowid,
                                      readwrite,
                                      &m_BlobHandle)),
                     eBlobOpen, "Error openning blob"
                  );
    _ASSERT(sql_ret == SQLITE_OK);

    if (m_Size == 0) {
        m_Size = static_cast<size_t>(sqlite3_blob_bytes(m_BlobHandle));
    }
}

void
CSQLITE_Blob::x_CloseBlob(void)
{
    if (!m_BlobHandle)
        return;

    BLOB_SAFE_CALL(sqlite3_blob_close(m_BlobHandle),
                   eBlobClose, "Error closing blob");
    _ASSERT(sql_ret == SQLITE_OK);
    m_BlobHandle = NULL;
}

size_t
CSQLITE_Blob::GetSize(void)
{
    if (m_Size == 0) {
        TBlobReadGuard guard(*this);

        // m_Size will be automatically initialized
    }

    return m_Size;
}

size_t
CSQLITE_Blob::Read(void* buffer, size_t size)
{
    TBlobReadGuard guard(*this);

    _ASSERT(m_Size >= m_Position);

    if (size > m_Size - m_Position) {
        size = m_Size - m_Position;
    }

    BLOB_SAFE_CALL((sqlite3_blob_read(m_BlobHandle,
                                      buffer,
                                      static_cast<int>(size),
                                      static_cast<int>(m_Position))),
                   eBlobRead,
                   "Error reading from position " << m_Position << " of blob"
                  );
    _ASSERT(sql_ret == SQLITE_OK);
    m_Position += size;

    return size;
}

void
CSQLITE_Blob::Write(const void* data, size_t size)
{
    if (size == 0)
        return;

    // A bit of a trick to avoid opening/closing blob just for reading its
    // size. Thus if size is 0 we open blob, automatically read size and then
    // decide what we're gonna do.
    if (m_Size == 0  ||  m_Position < m_Size) {
        TBlobWriteGuard guard(*this);

        if (m_Position < m_Size) {
            size_t to_write = min(m_Size - m_Position, size);
            BLOB_SAFE_CALL((sqlite3_blob_write(m_BlobHandle,
                                               data, int(to_write),
                                               int(m_Position))),
                           eBlobWrite,
                           "Error writing to position " << m_Position
                                << " " << to_write << " bytes for blob"
                          );
            _ASSERT(sql_ret == SQLITE_OK);

            data = static_cast<const char*>(data) + to_write;
            size -= to_write;
            m_Position += to_write;
        }
    }

    if (size != 0) {
        if (!m_AppendStmt.get()) {
            string sql("update ");
            sql += m_Database;
            sql += ".";
            sql += m_Table;
            sql += " set ";
            sql += m_Column;
            sql += "=";
            sql += m_Column;
            sql += "||?2 where rowid=?1";

            _ASSERT(m_ConnHandle);
            m_AppendStmt.reset(new CSQLITE_Statement(m_ConnHandle, sql));
        }
        try {
            m_AppendStmt->Bind(1, m_Rowid);
            m_AppendStmt->Bind(2, data, size);
            m_AppendStmt->Execute();
        }
        catch (...) {
            m_AppendStmt->Reset();
            throw;
        }
        m_AppendStmt->Reset();
    }
}

END_NCBI_SCOPE
