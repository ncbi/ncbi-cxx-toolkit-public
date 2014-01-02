#ifndef DB_SQLITE_SQLITEWRAPP__HPP
#define DB_SQLITE_SQLITEWRAPP__HPP
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
 *   Convenient wrappers for SQLite-related objects and most commonly used
 *   functions. Wrappers require SQLite 3.6.14 or higher with async vfs
 *   extension. All wrappers are written in assumption that no one else calls
 *   any sqlite3* functions except the wrapper itself.
 */

#include <corelib/ncbiexpt.hpp>
#include <corelib/ncbimisc.hpp>
#include <corelib/ncbistr.hpp>
#include <corelib/ncbimtx.hpp>
#include <corelib/ncbi_system.hpp>
#include <corelib/obj_pool.hpp>

#include <sqlite3.h>
#ifdef HAVE_SQLITE3ASYNC_H
# include <sqlite3async.h>
#endif

#include <deque>


BEGIN_NCBI_SCOPE


class CSQLITE_Exception;
class CSQLITE_Connection;
class CSQLITE_Statement;
class CSQLITE_Blob;


/// Utility class for some global-purpose functions tuning SQLite as a whole
/// as opposed to tuning connection-by-connection.
class CSQLITE_Global
{
public:
    /// Install non-default cache for all SQLite databases. Can be useful
    /// if default policy of working with cache is inappropriate for your
    /// application. For more details on sqlite3_pcache_methods look into
    /// comments before it in sqlite3.h.
    /// Method can be called only before SQLite is initialized with
    /// Initialize() method.
    static void SetCustomPageCache(sqlite3_pcache_methods* methods);
    /// Install non-default memory management for SQLite. For more details on
    /// sqlite3_mem_methods look into comments before it in sqlite3.h.
    /// Method can be called only before SQLite is initialized with
    /// Initialize() method.
    static void SetCustomMallocFuncs(sqlite3_mem_methods* methods);

    /// Initialization of SQLite and tuning some default parameters.
    /// For these SQLite wrappers to work properly this method should be
    /// called before any work with other classes.
    static void Initialize(void);
    /// Finish all SQLite operations. Method should be called at the end of
    /// application execution. After it has been called any further attempts
    /// to use wrapper classes will likely cause application crash.
    static void Finalize(void);

    /// Enable use of the same cache by different connections to the same
    /// database file. Setting can be changed at any time and will affect all
    /// connections opened after changing.
    static void EnableSharedCache(bool enable = true);
    /// Get default virtual file system installed in SQLite.
    /// Method can be called only after SQLite is initialized with
    /// Initialize() method.
    static sqlite3_vfs* GetDefaultVFS(void);
    /// Register new virtual file system in SQLite.
    /// If set_default is TRUE then all connections that will be opened after
    /// call to this method will use this virtual file system. When
    /// set_default is FALSE then connections will have to state explicitly
    /// if they want to use this new vfs (although for now this functionality
    /// is not yet implemented in CSQLITE_Connection).
    /// Method can be called only after SQLite is initialized with
    /// Initialize() method.
    static void RegisterCustomVFS(sqlite3_vfs* vfs, bool set_default = true);

#ifdef HAVE_SQLITE3ASYNC_H
    /// Setup the option for asynchronous file system to do the actual locking
    /// of database files on disk. If lock_files is TRUE then files will be
    /// actually locked on hard disk, if it is FALSE files will not be locked
    /// and so consistency of access to databases opened with asynchronous
    /// writing flag set will be maintained only inside current process.
    /// Method should be called only when there's no connections opened.
    static void SetAsyncWritesFileLocking(bool lock_files);
    /// Launch background thread doing all asynchronous writes to databases.
    /// Method is no-op if background thread has already launched.
    static void RunAsyncWritesThread(void);
#endif  // HAVE_SQLITE3ASYNC_H

private:
    CSQLITE_Global(void);
    CSQLITE_Global(CSQLITE_Global&);
    CSQLITE_Global& operator= (CSQLITE_Global&);
};


/// Object factory creating sqlite3* handles.
/// For internal use in CSQLITE_Connection's internal pool.
class CSQLITE_HandleFactory
{
public:
    CSQLITE_HandleFactory(CSQLITE_Connection* conn);

    /// Create new database handle
    sqlite3* CreateObject(void);

    /// Destroy database handle
    void DeleteObject(sqlite3* handle);

private:
    /// Connection object this factory is bound to
    CSQLITE_Connection* m_Conn;
};


/// Connection to SQLite database.
class CSQLITE_Connection
{
public:
    /// Flags controlling specifics of database connection operation.
    /// Only one flag can be used in each group of flags (separated by
    /// comments).
    enum EOperationFlags {
        // Mode of operation in multi-threaded environment
        fInternalMT = 0x00,  ///< Object and all statements and blobs created
                             ///< on top of it should support internal
                             ///< synchronization between different threads
        fExternalMT = 0x01,  ///< Object and all statements and blobs created
                             ///< on top of it will not be used from different
                             ///< threads simultaneously, thus performance is
                             ///< optimized, object creates only one low-level
                             ///< SQLite connection
        /// Default value for multi-threading group of flags
        fDefaultMT  = fInternalMT,
        eAllMT      = fInternalMT + fExternalMT,

        // Mode of vacuuming and shrinking file size
        fVacuumOn      = 0x00,  ///< Vacuuming is on, database file has always
                                ///< the minimum size possible
        fVacuumOff     = 0x02,  ///< Vacuuming is off, database file can only
                                ///< grow
        fVacuumManual  = 0x04,  ///< Vacuuming is only by request
        /// Default value for vacuuming group of flags
        fDefaultVacuum = fVacuumOn,
        eAllVacuum     = fVacuumOn + fVacuumOff + fVacuumManual,

        // Mode of journaling of all transactions
        fJournalDelete   = 0x00,  ///< Journal on disk, journal file is
                                  ///< deleted after each transaction
        fJournalTruncate = 0x08,  ///< Journal on disk, size of journal file
                                  ///< is nullified after each transaction
        fJournalPersist  = 0x10,  ///< Journal on disk, journal file can only
                                  ///< grow, never shrinks
        fJournalMemory   = 0x20,  ///< Journaling is entirely in-memory
        fJournalOff      = 0x40,  ///< Journaling is completely off (not
                                  ///< recommended - transactions cannot be
                                  ///< rollbacked unless they consist of just
                                  ///< one simple operation)
        /// Default value for journaling group of flags
        fDefaultJournal  = fJournalDelete,
        eAllJournal      = fJournalDelete + fJournalTruncate + fJournalPersist
                           + fJournalMemory + fJournalOff,

        // Mode of reliable synchronization with disk database file
        fSyncFull    = 0x000,  ///< Full synchronization, database cannot be
                               ///< corrupted
        fSyncOn      = 0x080,  ///< Synchronization is on, there is a slight
                               ///< chance of database corruption when
                               ///< OS crashes
        fSyncOff     = 0x100,  ///< Synchronization is off, database can be
                               ///< corrupted on OS crash or power outage
        /// Default value for synchronization group of flags
        fDefaultSync = fSyncFull,
        eAllSync     = fSyncOff + fSyncOn + fSyncFull,

        /// Mode of storing temporary data
        fTempToMemory = 0x000,  ///< Store all temporary tables in memory
        fTempToFile   = 0x200,  ///< Use actual disk file for temporary
                                ///< storage
        /// Default value for "temporary" group of flags
        fDefaultTemp  = fTempToMemory,
        eAllTemp      = fTempToMemory + fTempToFile,

        /// Mode of doing all writes to the database
        fWritesSync    = 0x000,  ///< All writes are synchronous - when update
                                 ///< has executed data is already in file
                                 ///< (though @sa fSyncOff)
#ifdef HAVE_SQLITE3ASYNC_H
        fWritesAsync   = 0x400,  ///< All writes are asynchronous - updates
                                 ///< return as quick as they can, actual
                                 ///< writes to database happen in background
                                 ///< thread (@sa
                                 ///< CSQLITE_Global::RunAsyncWritesThread)
#endif
        /// Default value for writes mode group of flags
        fDefaultWrites = fWritesSync,
        eAllWrites     = fWritesSync
#ifdef HAVE_SQLITE3ASYNC_H
                         + fWritesAsync
#endif
                         ,

        fReadOnly      = 0x8000, ///< The DB is read-only, ignore all flags
                                 ///< and do not execute any PRAGMA commands.

        /// Default value of all flags
        eDefaultFlags = fDefaultMT + fDefaultVacuum + fDefaultJournal
                        + fDefaultSync + fDefaultTemp + fDefaultWrites
    };
    /// Bit mask of EOperationFlags
    typedef int TOperationFlags;

    /// Connect to SQLite database.
    /// NOTE: Vacuuming flags have any value here only when new database is
    ///       created. When old database is opened with some data in it
    ///       then vacuuming flags are no-op, flags used at creation are in
    ///       effect.
    /// NOTE: Connection should be destroyed after all statements or blobs
    ///       created on top of it. destroying statements after connection
    ///       was destroyed will result in segmentation fault.
    ///
    /// @param file_name
    ///   File name of SQLite database. If it doesn't exist it will be
    ///   created. If directory for file doesn't exist exception will be
    ///   thrown. If file name is empty connection to temporary file will be
    ///   open. This file will be deleted after connection is closed. If file
    ///   name is ":memory:" then connection to in-memory database will be
    ///   open. In both cases (temporary file and in-memory database) flag
    ///   fExternalMT should be set (it is not checked but you will get
    ///   malfunctioning otherwise).
    /// @param flags
    ///   Flags for object operations
    CSQLITE_Connection(CTempString     file_name,
                       TOperationFlags flags = eDefaultFlags);

    /// Get database file name for the connection
    const string&   GetFileName(void) const;
    /// Get flags controlling database connection operation
    TOperationFlags GetFlags   (void) const;
    /// Change flags controlling database connection operation.
    /// NOTE: Vacuuming flags cannot be changed here. New flags will be
    ///       applied only to newly created low-level SQLite connections.
    ///       Thus in fInternalMT mode it is recommended to destroy all
    ///       CSQLITE_Statement and CSQLITE_Blob objects before changing
    ///       flags. In fExternalMT mode this cleaning of statements is
    ///       mandatory - you will get memory and other resources leak
    ///       otherwise.
    void SetFlags(TOperationFlags flags);
    /// Set page size for the database (in bytes). Setting has any value only
    /// if database is empty (has no tables) and no statements are created
    /// yet. Page size should be power of 2 and be less than or equal to
    /// 32768. If page size is not set default value is used.
    void SetPageSize(unsigned int size);
    /// Get page size used in the database (in bytes)
    unsigned int GetPageSize(void);
    /// Set recommended size of the database cache (number of pages). If value
    /// is not set or set to 0 then default SQLite value is used.
    void SetCacheSize(unsigned int size);
    /// Get recommended size of the database cache. If cache size was not set
    /// by SetCacheSize() method then this method returns 0, though actually
    /// SQLite uses some default value.
    unsigned int GetCacheSize(void);

    /// Try to shrink database and free up to max_free_size bytes of disk
    /// space. Method is no-op when vacuum mode is other than fVacuumManual.
    ///
    /// @sa EOperationFlags
    void Vacuum(size_t max_free_size);
    /// Create statement for executing manual vacuuming.
    /// Caller is responsible for deleting returned statement object.
    ///
    /// @sa Vacuum
    CSQLITE_Statement* CreateVacuumStmt(size_t max_free_size);

    /// Execute sql statement without returning any results.
    void ExecuteSql(CTempString sql);
    /// Delete database under this connection. Method makes sure that journal
    /// file is deleted along with database file itself. All CSQLITE_Statement
    /// and CSQLITE_Blob objects on this connection should be deleted before
    /// call to this method. Any use of connection after call to this method
    /// will cause the database file to be created again.
    void DeleteDatabase(void);

public:
    // For internal use only

    /// Lock and return low-level connection handle
    sqlite3* LockHandle(void);
    /// Unlock unused low-level connection handle. This handle will be used
    /// later by some other statement. Essentially no-op in eExternalMT mode.
    ///
    /// @sa EMultiThreadMode
    void UnlockHandle(sqlite3* handle);
    /// Setup newly created low-level connection handle.
    /// Executes all necessary pragmas according to operation flags set.
    void SetupNewConnection(sqlite3* handle);

private:
    CSQLITE_Connection(const CSQLITE_Connection&);
    CSQLITE_Connection& operator=(const CSQLITE_Connection&);

    /// Check that only one flag in specific group of flags is given
    void x_CheckFlagsValidity(TOperationFlags flags, EOperationFlags mask);
    /// Execute sql statement on given low-level connection handle
    void x_ExecuteSql(sqlite3* handle, CTempString sql);


    typedef CObjPool<sqlite3, CSQLITE_HandleFactory>  THandlePool;

    /// File name of SQLite database
    string           m_FileName;
    /// Flags for object operation
    TOperationFlags  m_Flags;
    /// Page size inside the database
    unsigned int     m_PageSize;
    /// Recommended size of cache for the database
    unsigned int     m_CacheSize;
    /// Pool of low-level database connections
    THandlePool      m_HandlePool;
};


/// SQL statement executing on SQLite database
class CSQLITE_Statement
{
public:
    /// Create and prepare statement for given database connection.
    /// If sql is empty nothing is prepared in the constructor.
    CSQLITE_Statement(CSQLITE_Connection* conn, CTempString sql = kEmptyStr);
    /// Create and prepare statement for particular low-level connection.
    /// If sql is empty nothing is prepared in the constructor.
    CSQLITE_Statement(sqlite3* conn_handle, CTempString sql = kEmptyStr);
    ~CSQLITE_Statement();

    /// Change sql text for the object and prepare new statement
    void SetSql(CTempString sql);

    /// Bind integer value to parameter index.
    /// Parameters' numbers start from 1.
    void Bind          (int index, int           val);
    /// Bind unsigned integer value to parameter index.
    /// Parameters' numbers start from 1.
    void Bind          (int index, unsigned int  val);
    /// Bind integer value to parameter index.
    /// Parameters' numbers start from 1.
    void Bind          (int index, long          val);
    /// Bind unsigned integer value to parameter index.
    /// Parameters' numbers start from 1.
    void Bind          (int index, unsigned long val);
#if !NCBI_INT8_IS_LONG
    /// Bind 64-bit integer value to parameter index.
    /// Parameters' numbers start from 1.
    void Bind          (int index, Int8          val);
    /// Bind unsigned 64-bit integer value to parameter index.
    /// Parameters' numbers start from 1.
    void Bind          (int index, Uint8         val);
#endif
    /// Bind double value to parameter index.
    /// Parameters' numbers start from 1.
    void Bind          (int index, double        val);
    /// Bind text value to parameter index.
    /// Parameters' numbers start from 1. Value of parameter is copied inside
    /// the method so it may disappear after call.
    void Bind          (int index, CTempString   val);
    /// Bind text value to parameter index.
    /// Parameters' numbers start from 1. Value of parameter is not copied,
    /// pointer is remembered instead. So given value should exist until
    /// statement is executed.
    void Bind          (int index, const char*   data, size_t size);
    /// Bind blob value to parameter index.
    /// Parameters' numbers start from 1. Value of parameter is not copied,
    /// pointer is remembered instead. So given value should exist until
    /// statement is executed.
    void Bind          (int index, const void*   data, size_t size);
    /// Bind blob value of given size containing only zeros to parameter
    /// index. Parameters' numbers start from 1.
    void BindZeroedBlob(int index, size_t        size);
    /// Bind null value to parameter index.
    void BindNull      (int index);

    /// Execute statement without returning any result
    void Execute(void);
    /// Step through results of the statement.
    /// If statement wasn't executed yet it starts executing. If statement
    /// already returned some rows then it moves to the next row. Be aware
    /// that when statement begins executing until it returns all rows or
    /// is reseted the database is locked so that nobody else can write to it.
    /// So it's recommended to step through all results as quick as possible.
    ///
    /// @return
    ///   TRUE if new row is available in the result.
    ///   FALSE if no more rows are available and thus statement finished
    ///   executing and released all locks held.
    bool Step(void);
    /// Reset the statement to release all locks and to be ready to execute
    /// again.
    void Reset(void);
    /// Reset all bindings to the statement to their initial NULL values
    void ClearBindings(void);

    /// Get number of columns in result set. Method should be executed only
    /// after Step() returned TRUE, otherwise returned value is undefined.
    int GetColumnsCount(void) const;
    /// Get name of column at index col_ind in result set. The leftmost column
    /// of result set has the index 0. Method should be executed only after
    /// Step() returned TRUE, otherwise returned value is undefined.
    CStringUTF8 GetColumnName(int col_ind) const;

    /// Get integer value from column col_ind in current row. The leftmost
    /// column of result set has index 0. Method should be executed only
    /// after Step() returned TRUE, otherwise returned value is undefined.
    int    GetInt   (int col_ind) const;
    /// Get 64-bit integer value from column col_ind in current row.
    /// The leftmost column of result set has the index 0. Method should be
    /// executed only after Step() returned TRUE, otherwise returned value is
    /// undefined.
    Int8   GetInt8  (int col_ind) const;
    /// Get double value from column col_ind in current row. The leftmost
    /// column of result set has the index 0. Method should be executed only
    /// after Step() returned TRUE, otherwise returned value is undefined.
    double GetDouble(int col_ind) const;
    /// Get text value from column col_ind in current row. The leftmost
    /// column of result set has the index 0. Method should be executed only
    /// after Step() returned TRUE, otherwise returned value is undefined.
    string GetString(int col_ind) const;
    /// Get size of blob value from column col_ind in current row. The leftmost
    /// column of result set has the index 0. Method should be executed only
    /// after Step() returned TRUE, otherwise returned value is undefined.
    size_t GetBlobSize(int col_ind) const;
    /// Read blob value from column col_ind in current row. The leftmost
    /// column of result set has the index 0. Method should be executed only
    /// after Step() returned TRUE, otherwise returned value is undefined.
    ///
    /// @param col_ind
    ///   Index of column to read (left-most is 0)
    /// @param buffer
    ///   Pointer to buffer where to write the data
    /// @param size
    ///   Size of buffer available for writing
    /// @return
    ///   Number of bytes read from result set and put into buffer
    size_t GetBlob  (int col_ind, void* buffer, size_t size) const;

    /// Get rowid of the row inserted in last statement execution.
    /// If connection is working in eExternalMT mode then method should be
    /// called after statement execution and before execution of any other
    /// statement in the same connection. If last executed statement was not
    /// insert then 0 is returned.
    Int8 GetLastInsertedRowid(void) const;
    /// Get number of rows changed during last insert, delete or update
    /// statement. Number does not include rows changed by triggers or by
    /// any other indirect means. If called when no insert, update or delete
    /// statement was executed result is undefined.
    int  GetChangedRowsCount (void) const;

private:
    CSQLITE_Statement(const CSQLITE_Statement&);
    CSQLITE_Statement& operator=(const CSQLITE_Statement&);

    /// Prepare new statement if it's not empty
    void x_Prepare(CTempString sql);
    /// Finalize current statement
    void x_Finalize(void);


    /// Connection this statement belongs to
    CSQLITE_Connection* m_Conn;
    /// Low-level connection handle provided for this statement
    sqlite3*            m_ConnHandle;
    /// Low-level statement handle
    sqlite3_stmt*       m_StmtHandle;
};


/// "Scoped" statement activity object.
/// Object binds to some statement and ensures that by the end of the scope
/// statement will be reseted and all database locks will be released. Besides
/// that object acts as smart pointer to the statement.
class CSQLITE_StatementLock
{
public:
    /// Bind activity control to the given statement
    CSQLITE_StatementLock(CSQLITE_Statement* stmt);

    ~CSQLITE_StatementLock(void);

    /// Smart pointer's transformation
    CSQLITE_Statement& operator*  (void);

    /// Smart pointer's transformation
    CSQLITE_Statement* operator-> (void);

    /// Smart pointer's transformation
    operator CSQLITE_Statement*   (void);

private:
    CSQLITE_StatementLock(const CSQLITE_StatementLock&);
    CSQLITE_StatementLock& operator= (const CSQLITE_StatementLock&);

    /// Statement this object is bound to
    CSQLITE_Statement* m_Stmt;
};


// Structures for internal use only
template <bool readwrite> struct SSQLITE_BlobOpen;
struct SSQLITE_BlobClose;

/// Object reading and writing blob directly (mostly without executing select
/// or update statements), can read and write blob incrementally.
/// Class assumes that from the moment of its creation till deletion nobody
/// else writes to the blob and/or changes its size.
class CSQLITE_Blob
{
public:
    /// Create object reading and writing blob
    /// Identified row with blob should exist in database, exception is
    /// thrown otherwise.
    ///
    /// @param conn
    ///   Connection which object will work over
    /// @param table
    ///   Table name where blob is located
    /// @param column
    ///   Column name where blob is located
    /// @param rowid
    ///   Rowid of the row where blob is located
    CSQLITE_Blob(CSQLITE_Connection* conn,
                 CTempString         table,
                 CTempString         column,
                 Int8                rowid);
    /// Create object reading and writing blob
    /// Identified row with blob should exist in database, exception is
    /// thrown otherwise.
    ///
    /// @param conn
    ///   Connection which object will work over
    /// @param db_name
    ///   Database name where blob is located
    /// @param table
    ///   Table name where blob is located
    /// @param column
    ///   Column name where blob is located
    /// @param rowid
    ///   Rowid of the row where blob is located
    CSQLITE_Blob(CSQLITE_Connection* conn,
                 CTempString         db_name,
                 CTempString         table,
                 CTempString         column,
                 Int8                rowid);
    ~CSQLITE_Blob(void);

    /// Set current position inside the blob to desired value
    void   ResetPosition(size_t position = 0);
    /// Get current position inside the blob
    size_t GetPosition  (void);
    /// Get size of the blob
    size_t GetSize(void);

    /// Read from blob at current position
    ///
    /// @param buffer
    ///   Pointer to buffer where to write read data
    /// @param size
    ///   Size of buffer available for writing
    /// @return
    ///   Number of bytes read from blob. If current position is beyond end
    ///   of the blob then 0 is returned and buffer is unchanged.
    size_t Read   (void*       buffer, size_t size);
    /// Write to blob at current position
    ///
    /// @param data
    ///   Pointer to data to write
    /// @param size
    ///   Number of bytes to write
    void   Write  (const void* data,   size_t size);
    /// Set statement to use when appending to existing blob if necessary.
    /// Statement should be of the form
    /// "update ... set ... = ...||?2 where rowid=?1"
    /// If not set object creates its own statement on the first append.
    /// Created statement is destroyed together with this blob object.
    /// Statement set by this method is not destroyed with blob object.
    /// This method can be useful if one wants to cache statement between
    /// different blob objects creation and deletion to not prepare it with
    /// each blob object.
    void SetAppendStatement(CSQLITE_Statement* stmt);

private:
    CSQLITE_Blob(const CSQLITE_Blob&);
    CSQLITE_Blob& operator= (const CSQLITE_Blob&);

    /// Initialize the object
    void x_Init     (void);
    /// Open low-level blob object
    ///
    /// @param readwrite
    ///   TRUE if there's need for write access, FALSE if read-only access is
    ///   sufficient
    void x_OpenBlob (bool readwrite = false);
    /// Close low-level blob object
    void x_CloseBlob(void);

    template <bool readwrite> friend struct SSQLITE_BlobOpen;
    friend struct SSQLITE_BlobClose;


    /// Connection this blob object belongs to
    CSQLITE_Connection*        m_Conn;
    /// Low-level connection handle provided for this blob object
    sqlite3*                   m_ConnHandle;
    /// Statement used for appending to existing blob value
    AutoPtr<CSQLITE_Statement> m_AppendStmt;
    /// Database name for blob
    string                     m_Database;
    /// Table name for blob
    string                     m_Table;
    /// Column name for blob
    string                     m_Column;
    /// Rowid for the row where blob is located
    Int8                       m_Rowid;
    /// Low-level handle of the blob
    sqlite3_blob*              m_BlobHandle;
    /// Size of the blob
    size_t                     m_Size;
    /// Current position inside the blob
    size_t                     m_Position;
};


/// Exception thrown from all SQLite-related objects and functions
class CSQLITE_Exception : public CException
{
public:
    enum EErrCode {
        eUnknown,       ///< Unknown error
        eWrongFlags,    ///< Incorrect set of flags in connection constructor
        eDBOpen,        ///< Error during database opening
        eStmtPrepare,   ///< Error preparing statement
        eStmtFinalize,  ///< Error finalizing statement
        eStmtBind,      ///< Error binding statement parameters
        eStmtStep,      ///< Error stepping through the statement
        eStmtReset,     ///< Error reseting statement
        eBlobOpen,      ///< Error opening blob object
        eBlobClose,     ///< Error closing blob object
        eBlobRead,      ///< Error reading directly from blob
        eBlobWrite,     ///< Error writing directly to blob
        eConstraint,    ///< Constraint violation during statement execution
        eDeadLock,      ///< SQLite detected possible deadlock between
                        ///< different threads
        eBadCall        ///< Method called when there's no enough capabilities
                        ///< to finish it successfully
    };
    virtual const char* GetErrCodeString(void) const;
    NCBI_EXCEPTION_DEFAULT(CSQLITE_Exception, CException);
};



//////////////////////////////////////////////////////////////////////////
// Inline functions
//////////////////////////////////////////////////////////////////////////

inline
CSQLITE_Statement::CSQLITE_Statement(CSQLITE_Connection* conn,
                                     CTempString         sql)
    : m_Conn(conn),
      m_ConnHandle(NULL),
      m_StmtHandle(NULL)
{
    x_Prepare(sql);
}

inline
CSQLITE_Statement::CSQLITE_Statement(sqlite3*    conn_handle,
                                     CTempString sql)
    : m_Conn(NULL),
      m_ConnHandle(conn_handle),
      m_StmtHandle(NULL)
{
    x_Prepare(sql);
}

inline void
CSQLITE_Statement::SetSql(CTempString sql)
{
    x_Finalize();
    x_Prepare(sql);
}

inline void
CSQLITE_Statement::Execute(void)
{
    while (Step())
    {}
}

inline void
CSQLITE_Statement::Bind(int index, Uint8 val)
{
    // SQLite doesn't understand unsigned types anyway
    Bind(index, static_cast<Int8>(val));
}

#if !NCBI_INT8_IS_LONG

inline void
CSQLITE_Statement::Bind(int index, long val)
{
    Bind(index, static_cast<Int8>(val));
}

inline void
CSQLITE_Statement::Bind(int index, unsigned long val)
{
    Bind(index, static_cast<Uint8>(val));
}

#endif

inline void
CSQLITE_Statement::Bind(int index, int val)
{
    Bind(index, static_cast<long>(val));
}

inline void
CSQLITE_Statement::Bind(int index, unsigned int val)
{
    Bind(index, static_cast<unsigned long>(val));
}

inline int
CSQLITE_Statement::GetColumnsCount(void) const
{
    _ASSERT(m_StmtHandle);
    return sqlite3_column_count(m_StmtHandle);
}

inline int
CSQLITE_Statement::GetInt(int col_ind) const
{
    _ASSERT(m_StmtHandle);
    return sqlite3_column_int(m_StmtHandle, col_ind);
}

inline Int8
CSQLITE_Statement::GetInt8(int col_ind) const
{
    _ASSERT(m_StmtHandle);
    return sqlite3_column_int64(m_StmtHandle, col_ind);
}

inline double
CSQLITE_Statement::GetDouble(int col_ind) const
{
    _ASSERT(m_StmtHandle);
    return sqlite3_column_double(m_StmtHandle, col_ind);
}

inline Int8
CSQLITE_Statement::GetLastInsertedRowid(void) const
{
    _ASSERT(m_ConnHandle);
    return sqlite3_last_insert_rowid(m_ConnHandle);
}

inline int
CSQLITE_Statement::GetChangedRowsCount(void) const
{
    _ASSERT(m_ConnHandle);
    return sqlite3_changes(m_ConnHandle);
}


inline const string&
CSQLITE_Connection::GetFileName(void) const
{
    return m_FileName;
}

inline CSQLITE_Connection::TOperationFlags
CSQLITE_Connection::GetFlags(void) const
{
    return m_Flags;
}

inline unsigned int
CSQLITE_Connection::GetPageSize(void)
{
    return m_PageSize;
}

inline void
CSQLITE_Connection::SetPageSize(unsigned int size)
{
    m_PageSize = size;
    m_HandlePool.Clear();
}

inline unsigned int
CSQLITE_Connection::GetCacheSize(void)
{
    return m_CacheSize;
}

inline void
CSQLITE_Connection::SetCacheSize(unsigned int size)
{
    m_CacheSize = size;
    m_HandlePool.Clear();
}

inline sqlite3*
CSQLITE_Connection::LockHandle(void)
{
    sqlite3* result = m_HandlePool.Get();
    if ((m_Flags & eAllMT) == fExternalMT) {
        m_HandlePool.Return(result);
    }
    return result;
}

inline void
CSQLITE_Connection::UnlockHandle(sqlite3* handle)
{
    if ((m_Flags & eAllMT) == fInternalMT) {
        m_HandlePool.Return(handle);
    }
}

inline void
CSQLITE_Connection::ExecuteSql(CTempString sql)
{
    CSQLITE_Statement stmt(this, sql);
    stmt.Execute();
}

inline void
CSQLITE_Connection::Vacuum(size_t max_free_size)
{
    AutoPtr<CSQLITE_Statement> stmt(CreateVacuumStmt(max_free_size));
    stmt->Execute();
}


inline
CSQLITE_StatementLock::CSQLITE_StatementLock(CSQLITE_Statement* stmt)
    : m_Stmt(stmt)
{
    _ASSERT(stmt);
}

inline CSQLITE_Statement&
CSQLITE_StatementLock::operator* (void)
{
    return *m_Stmt;
}

inline CSQLITE_Statement*
CSQLITE_StatementLock::operator-> (void)
{
    return m_Stmt;
}

inline
CSQLITE_StatementLock::operator CSQLITE_Statement* (void)
{
    return m_Stmt;
}


inline
CSQLITE_Blob::~CSQLITE_Blob(void)
{
    x_CloseBlob();
    if (m_ConnHandle) {
        m_Conn->UnlockHandle(m_ConnHandle);
    }
}

inline size_t
CSQLITE_Blob::GetPosition(void)
{
    return m_Position;
}

inline void
CSQLITE_Blob::ResetPosition(size_t position /* = 0 */)
{
    m_Position = position;
}

inline void
CSQLITE_Blob::SetAppendStatement(CSQLITE_Statement* stmt)
{
    m_AppendStmt.reset(stmt, eNoOwnership);
}

END_NCBI_SCOPE

#endif /* DB_SQLITE_SQLITEWRAPP__HPP */
