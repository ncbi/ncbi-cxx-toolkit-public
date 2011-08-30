#ifndef NETCACHE__NC_DB_FILES__HPP
#define NETCACHE__NC_DB_FILES__HPP
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
 */

#include <corelib/ncbithr.hpp>
#include <db/sqlite/sqlitewrapp.hpp>

#include "nc_db_info.hpp"
#include "nc_utils.hpp"


BEGIN_NCBI_SCOPE


/// Size of database page used in NetCache
static const int kNCSQLitePageSize = 32768;


class CNCDBStat;


/// Statement types used in CNCDBFile
enum ENCStmtType
{
    eStmt_CreateDBFile,      ///< Create new database part
    eStmt_DeleteDBFile,      ///< Delete info about old database part
    eStmt_GetBlobsList,      ///< Get part of list of blobs "alive" in
                             ///< the given time frame
    eStmt_WriteBlobInfo,     ///< Write all meta-info about blob
    eStmt_UpdateBlobInfo,    ///< Set blob's expiration time to given value
    eStmt_ReadBlobInfo,      ///< Get all meta-info about blob
    eStmt_DeleteBlobInfo,    ///<
    eStmt_GetChunkIds,       ///< Get ids of all chunks for given blob
    eStmt_CreateChunk,       ///< Create new blob chunk
    eStmt_WriteChunkData,    ///< Create new record with blob chunk data
    eStmt_ReadChunkData      ///< Read blob chunk data
};

/// Connection to one database file in NetCache storage.
/// Used for centralization of all only-database-related code. Actual using
/// of all methods can be only via derivative classes.
///
/// @sa CNCDBIndexFile, CNCDBMetaFile, CNCDBDataFile
class CNCDBFile : public CSQLITE_Connection
{
public:
    virtual ~CNCDBFile(void);

    ///
    ENCDBFileType GetType(void);
    ///
    void LockDB(void);
    ///
    void UnlockDB(void);
    ///
    Int8 GetFileSize(void);

    /// Create entire database structure for Index file
    void CreateDatabase(ENCDBFileType db_type);
    /// Delete the database file along with its journal file.
    /// This is sort of overriding of parent class' method for cleaning all
    /// prepared statements.
    void DeleteDatabase(void);

protected:
    /// Create connection to NetCache database file
    ///
    /// @param file_name
    ///   Name of the database file
    /// @param flags
    ///   Specific flags for operation (common flags are added - fExternalMT,
    ///   fSyncOff, fTempToMemory, fWritesSync)
    /// @param stat
    ///   Object for gathering database statistics
    CNCDBFile(CTempString     file_name,
              TOperationFlags flags,
              ENCDBFileType   file_type);

    /// Create new database part and save information about it.
    /// Creation time in given info structure is set to current time.
    void NewDBFile(ENCDBFileType file_type,
                   TNCDBFileId   file_id,
                   const string& file_name);
    /// Delete database part
    void DeleteDBFile(TNCDBFileId part_id);
    /// Read information about all database parts in order of their creation
    void GetAllDBFiles(TNCDBFilesMap* files_map);
    /// Clean index database removing information about all database parts.
    void DeleteAllDBFiles(void);

    Uint8 GetMaxSyncLogRecNo(void);
    void SetMaxSyncLogRecNo(Uint8 rec_no);

    /// Get last blob id used in the database
    TNCBlobId GetLastBlobId(void);
    /// Get last chunk id used in the database
    TNCChunkId GetLastChunkId(void);
    /// Read part of the full list of existing blobs in database. Only
    /// blobs live at the dead_after moment or after that and before
    /// dead_before moment are considered existing. Information about blobs
    /// is returned in the order of "dead time" with ties ordered by id.
    /// Method designed to be convenient for sequential calls until information
    /// about all necessary blobs is selected.
    ///
    /// @param dead_after
    ///   Starting moment of "alive period" caller is interested in (included
    ///   into the period). After method execution this will contain
    ///   "dead time" of the last selected blob so that it can be passed as
    ///   dead_after in next method call.
    /// @param id_after
    ///   Starting id for blobs caller is interested in. Parameter is used
    ///   only for breaking ties in first moment of "alive period", i.e. if
    ///   blob has "dead time" equal to dead_after then it will be returned
    ///   only if it has id strictly greater than id_after. After method
    ///   execution this will contain id of the last blob returned so that it
    ///   can be passed as id_after in next method call.
    /// @param dead_before
    ///   Ending moment of "alive period" caller is interested in (not
    ///   included into the period).
    /// @param max_count
    ///   Maximum number of ids method can return.
    /// @param blobs_list
    ///   Pointer to the list of blobs information to fill in. List is cleared
    ///   at the beginning of method execution.
    void GetBlobsList(int&          dead_after,
                      TNCBlobId&    id_after,
                      int           dead_before,
                      int           max_count,
                      TNCBlobsList* blobs_list);

    /// Read meta-information about blob - owner, ttl etc.
    ///
    /// @return
    ///   TRUE if meta-information was found in the database, FALSE otherwise
    bool ReadBlobInfo(SNCBlobVerData* blob_info);
    /// Write full meta-information for blob
    void WriteBlobInfo(const string& blob_key, const SNCBlobVerData* blob_info);
    ///
    void UpdateBlobInfo(const SNCBlobVerData* blob_info);
    ///
    void DeleteBlobInfo(TNCBlobId blob_id);
    ///
    //void DeleteAllBlobInfos(const string& min_key, const string& max_key);

    /// Get ids for all chunks of the given blob
    void GetChunkIds(TNCBlobId blob_id, TNCChunksList* id_list);
    /// Create record with association chunk id -> blob id
    void CreateChunk(TNCBlobId blob_id, TNCChunkId chunk_id);
    ///
    void WriteChunkData(TNCChunkId chunk_id, const CNCBlobBuffer* data);
    ///
    bool ReadChunkData(TNCChunkId chunk_id, CNCBlobBuffer* buffer);

private:
    CNCDBFile(const CNCDBFile&);
    CNCDBFile& operator= (const CNCDBFile&);

    ///
    void x_CreateIndexDatabase(void);
    ///
    void x_CreateMetaDatabase(void);
    ///
    void x_CreateDataDatabase(void);
    /// Create or get if already was created statement of given type.
    /// All created and prepared statements will be cached and re-used over
    /// the lifetime of database connection object.
    CSQLITE_Statement* x_GetStatement(ENCStmtType typ);


    typedef AutoPtr<CSQLITE_Statement>       TStatementPtr;
    typedef map<ENCStmtType, TStatementPtr>  TStmtMap;

    /// Map associating statement types to prepared statements
    TStmtMap        m_Stmts;
    ///
    ENCDBFileType   m_Type;
    ///
    CFastMutex      m_DBLock;
};


/// Connection to index database in NetCache storage. Database contains
/// information about storage parts containing actual blobs data.
class CNCDBIndexFile : public CNCDBFile
{
public:
    ///
    static ENCDBFileType GetClassType(void);

    /// Create connection to index database file
    ///
    /// @param file_name
    ///   Name of the database file
    /// @param stat
    ///   Object for gathering database statistics
    CNCDBIndexFile(const string& file_name);
    virtual ~CNCDBIndexFile(void);

    using CNCDBFile::NewDBFile;
    using CNCDBFile::DeleteDBFile;
    using CNCDBFile::GetAllDBFiles;
    using CNCDBFile::DeleteAllDBFiles;
    using CNCDBFile::GetMaxSyncLogRecNo;
    using CNCDBFile::SetMaxSyncLogRecNo;
};


/// Connection to the database file in NetCache storage containing all
/// meta-information about blobs and their chunks.
class CNCDBMetaFile : public CNCDBFile
{
public:
    ///
    static ENCDBFileType GetClassType(void);

    /// Create connection to database file with blobs' meta-information
    ///
    /// @param file_name
    ///   Name of the database file
    /// @param stat
    ///   Object for gathering database statistics
    CNCDBMetaFile(const string& file_name);
    virtual ~CNCDBMetaFile(void);

    using CNCDBFile::GetLastBlobId;
    using CNCDBFile::GetLastChunkId;
    using CNCDBFile::GetBlobsList;

    using CNCDBFile::ReadBlobInfo;
    using CNCDBFile::WriteBlobInfo;
    using CNCDBFile::UpdateBlobInfo;
    using CNCDBFile::DeleteBlobInfo;
    //using CNCDBFile::DeleteAllBlobInfos;

    using CNCDBFile::GetChunkIds;
    using CNCDBFile::CreateChunk;
};


/// Connection to the database file in NetCache storage containing all
/// actual blobs data.
class CNCDBDataFile : public CNCDBFile
{
public:
    ///
    static ENCDBFileType GetClassType(void);

    /// Create connection to database file with blobs' data
    ///
    /// @param file_name
    ///   Name of the database file
    /// @param stat
    ///   Object for gathering database statistics
    CNCDBDataFile(const string& file_name);
    virtual ~CNCDBDataFile(void);

    using CNCDBFile::WriteChunkData;
    using CNCDBFile::ReadChunkData;
};


class CNCFSOpenFile;
struct SNCFSEvent;

/// Class representing file system which NetCache works with.
/// This class along with CNCFS* ones together make virtual file system
/// exposed to SQLite. System implements somewhat dangerous mechanism of work:
/// all actual writings on disk are made in background thread and data for
/// writing is taken right from database cache memory which SQLite works with.
/// This can cause some temporary inconsistencies in the database when SQLite
/// makes several changes and several virtual writes of one page but because
/// of background queue latency only all changes together will make it to the
/// disk. And it can happen before some other changes which SQLite expects to
/// be on disk earlier. These inconsistencies can cause database corruption in
/// case of NetCache's or OS's crash. But they happen very-very rarely and
/// cost of losing one database part because of that is not so high, though
/// benefit in NetCache's performance from such work scheme is pretty big.
class CNCFileSystem
{
public:
    /// Initialize virtual file system
    static bool Initialize(void);
    static bool ReserveDiskSpace(void);
    /// Finalize virtual file system.
    /// Method will wait until all background writings to disk are finished.
    static void Finalize(void);
    /// Notify system that disk database was initialized and all SQLite's
    /// hot-journals that could exist were read and rollbacked. So from this
    /// point no new journals can appear (no other processes work with
    /// database). And this fact allows to make additional optimizations.
    static void SetDiskInitialized(void);
    /// Check if disk database was initialized.
    static bool IsDiskInitialized(void);

    static void EnableTimeThrottling(Uint8 server_id);

    /// Open new database file.
    /// Method is called automatically inside file system but can be called
    /// outside to notify that database file requires synchronous I/O because
    /// of extreme importance (as index file in NetCache's storage).
    static CNCFSOpenFile* OpenNewDBFile(const string& file_name,
                                        bool          force_sync_io = false);
    /// Close database file.
    /// File is closed in background so it's not allowed to do something with
    /// the physical file right after call to this method.
    /// Method is called automatically inside file system when last connection
    /// to database file is closed.
    static void CloseDBFile(CNCFSOpenFile* file);
    /// Notify file system that given file should be deleted after last
    /// connection to it is closed.
    static void DeleteFileOnClose(const string& file_name);
    /// Notify file system that given file is initialized and no significant
    /// changes will be made to the first page of the file in the future. This
    /// allows to do some good optimizations in file system work.
    static void SetFileInitialized(const string& file_name);
    ///
    static Int8 GetFileSize(const string& file_name);

    /// Get size of internal write queue
    static Uint4 GetQueueSize(void);

public:
    // For use only internally in nc_db_files.cpp

    /// Open real file for actual reading/writing using default SQLite's VFS.
    static int OpenRealFile(sqlite3_file** file,
                            const char*    name,
                            int            flags,
                            int*           out_flags);
    /// Close real file opened with OpenRealFile() and assign pointer to NULL.
    static int CloseRealFile(sqlite3_file*& file);
    /// Find already opened file by its name.
    static CNCFSOpenFile* FindOpenFile(const string& name);
    /// Schedule writing data to the file for execution in background thread.
    static void WriteToFile(CNCFSOpenFile* file,
                            Int8           offset,
                            const void*    data,
                            int            cnt);

private:
    /// This is a utility class, so no instantiation is allowed.
    CNCFileSystem(void);

    static void x_ReserveSpace(void);

    /// Add new event to the event queue.
    static void x_AddNewEvent(SNCFSEvent* event);
    /// Remove open file from the list of all open files
    static void x_RemoveFileFromList(CNCFSOpenFile* file);
    /// Method executing all background work for the file system
    static void x_DoBackgroundWork(void);
    /// Execute writing event - perform real writing to the file.
    static void x_DoWriteToFile(SNCFSEvent* event);
    /// Execute closing event - close file, remove it from memory and delete
    /// it on the disk if necessary.
    static void x_DoCloseFile(SNCFSEvent* event);


    typedef map<string, CNCFSOpenFile*>  TFilesList;


    /// Read/write lock to work with list of all open files
    static CFastRWLock          sm_FilesListLock;
    /// Head of the list of all open files
    static TFilesList           sm_FilesList;
    /// Mutex to work with write/close events queue
    static CSpinLock            sm_EventsLock;
    /// Head of write/close events queue
    static SNCFSEvent* volatile sm_EventsHead;
    /// Tail of write/close events queue
    static SNCFSEvent*          sm_EventsTail;
    /// Number of events in the queue
    static CAtomicCounter       sm_CntEvents;
    /// Background thread executing all actual writings
    static CRef<CThread>        sm_BGThread;
    /// Flag showing that file system is finalized and background thread
    /// should be stopped already.
    static bool                 sm_Stopped;
    /// Flag showing that background thread is in process of executing some
    /// write/close events. Value of FALSE in this flag means that background
    /// thread is waiting for new events on sm_BGSleep.
    static bool volatile        sm_BGWorking;
    /// Semaphore used in background thread for waiting for new events to
    /// arrive.
    static CSemaphore           sm_BGSleep;
    /// Flag showing that disk database was initialized
    static bool                 sm_DiskInitialized;
    /// Number of threads frozen and waiting while any disk write will happen
    /// and some memory will be freed.
    static Uint4                sm_WaitingOnAlert;
    /// Semaphore for freezing threads waiting for disk writes.
    static CSemaphore           sm_OnAlertWaiter;
};


//////////////////////////////////////////////////////////////////////////
// Classes only for internal use in nc_db_files.cpp
//////////////////////////////////////////////////////////////////////////

/// Class representing one open file in file system.
class CNCFSOpenFile
{
public:
    /// Open new file with given name and set flag of mandatory synchronous
    /// I/O to given value.
    CNCFSOpenFile(const string& file_name, bool force_sync_io);
    ~CNCFSOpenFile(void);

    /// Check if synchronous I/O is requested for the file.
    bool IsForcedSync(void);
    /// Get size of the file.
    Int8 GetSize(void);
    /// Check if file was really opened by SQLite (TRUE) or internally (FALSE)
    bool IsFileOpen(void);
    /// Set flag showing if file was really opened by SQLite
    void SetFileOpen(bool value = true);

    /// Read data from the very first page of the file.
    int  ReadFirstPage (Int8 offset, int cnt, void* mem_ptr);
    /// Write data to the very first page of the file.
    /// Second parameter (data) is changed inside the method to point to the
    /// persistent copy of the data inside this object.
    bool WriteFirstPage(const void** data, int* cnt);
    /// Adjust remembered size of the file so that it's not less than given
    /// value.
    void AdjustSize(Int8 size);
    /// Mark first page as clean
    void CleanFirstPage(void);

private:
    friend class CNCFileSystem;

    /// Prohibit copying of the object
    CNCFSOpenFile(const CNCFSOpenFile&);
    CNCFSOpenFile& operator= (const CNCFSOpenFile&);


    /// Name of the file
    string          m_Name;
    /// Real file in default SQLite's VFS used for writing
    sqlite3_file*   m_RealFile;
    /// Memory used to store very first page in the database - it's the only
    /// page that is read and written very frequently, it's not stored in
    /// database cache and thus can be read when some writes are pending.
    char*           m_FirstPage;
    /// Flag if first page is dirty
    bool volatile   m_FirstPageDirty;
    /// Size of the file.
    /// It could be not the same as size of file on disk but they will be
    /// equal when all writing events in queue are executed.
    Int8            m_Size;

    /// Flags of file operation
    enum EFlags {
        fForcedSync    = 1, ///< Synchronous I/O is requested
        fDeleteOnClose = 2, ///< File should be deleted from disk when it's
                            ///< closed
        fInitialized   = 4, ///< Initialization of the file schema is done and
                            ///< no significant changes to the first page will
                            ///< be made furthermore
        fOpened        = 8  ///< File was really opened by SQLite, not just
                            ///< internally to set some flags
    };
    /// Bit mask from EFlags
    typedef int  TFlags;

    /// Flags for the file
    TFlags          m_Flags;
};


/// Information about one event in file system's queue for background
/// execution.
struct SNCFSEvent
{
    /// Type of event
    enum EType {
        eWrite,
        eClose
    };

    /// Next event in the queue
    SNCFSEvent*     next;
    /// Type of this event
    EType           type;
    /// File which this event should be executed on
    CNCFSOpenFile*  file;
    /// Offset for writing
    Int8            offset;
    /// Data to be written
    const void*     data;
    /// Amount of data to be written
    int             cnt;
};


/// Virtual file in the file system opened by SQLite.
/// Class redirects all calls to a real file if it's for the file with
/// synchronous I/O requested or for the journal. For other files it redirects
/// all necessary calls to CNCFSOpenFile. Objects of this class are in
/// one-to-one relation with CNCFSOpenFile objects. The difference is only in
/// life time: for this class it's controlled by SQLite, for CNCFSOpenFile
/// it's controlled by CNCFileSystem.
class CNCFSVirtFile : public sqlite3_file
{
public:
    /// Open new database file
    int Open(const char* name, int flags, int* out_flags);
    /// Close this file
    int Close(void);
    /// Read data from the file
    int Read(Int8 offset, int cnt, void* mem_ptr);
    /// Write data to the file
    int Write(Int8 offset, const void* data, int cnt);
    /// Get size of the file
    Int8 GetSize(void);
    /// Place a lock on the file.
    /// Method is no-op for all files with asynchronous I/O because they're
    /// opened by SQLite only once (because shared cache mode is on) and no
    /// other process can use database.
    int Lock(int lock_type);
    /// Release lock on the file.
    /// The same comment about no-op in the method as in Lock() is applicable.
    int Unlock(int lock_type);
    /// Check if somebody locked this file with RESERVED lock.
    /// The same comment about no-op in the method as in Lock() is applicable.
    bool IsReserved(void);

private:
    /// File used for writing operations
    CNCFSOpenFile*  m_WriteFile;
    /// Real file used only for reading operations
    sqlite3_file*   m_ReadFile;
};


class CNCUint8Tls : public CNCTlsObject<CNCUint8Tls, Uint8>
{
public:
    static Uint8* CreateTlsObject(void);
    static void DeleteTlsObject(void* obj);
};




//////////////////////////////////////////////////////////////////////////
// Inline methods
//////////////////////////////////////////////////////////////////////////

inline Int8
CNCFSOpenFile::GetSize(void)
{
    return m_Size;
}


inline CNCFSOpenFile*
CNCFileSystem::FindOpenFile(const string& name)
{
    CFastReadGuard guard(sm_FilesListLock);

    TFilesList::const_iterator it = sm_FilesList.find(name);
    return it == sm_FilesList.end()? NULL: it->second;
}

inline Int8
CNCFileSystem::GetFileSize(const string& file_name)
{
    CNCFSOpenFile* file = FindOpenFile(file_name);
    return (file? file->GetSize(): 0);
}


inline
CNCDBFile::CNCDBFile(CTempString     file_name,
                     TOperationFlags flags,
                     ENCDBFileType   file_type)
    // If fSyncOff in here was changed to something else then behavior
    // in CNCFSVirtFile should be changed appropriately.
    : CSQLITE_Connection(file_name,
                         flags | fExternalMT   | fSyncOff
                               | fTempToMemory | fWritesSync),
      m_Type(file_type)
{
    SetCacheSize(kNCSQLitePageSize);
}

inline ENCDBFileType
CNCDBFile::GetType(void)
{
    return m_Type;
}

inline void
CNCDBFile::LockDB(void)
{
    m_DBLock.Lock();
}

inline void
CNCDBFile::UnlockDB(void)
{
    m_DBLock.Unlock();
}

inline Int8
CNCDBFile::GetFileSize(void)
{
    return CNCFileSystem::GetFileSize(GetFileName());
}

inline void
CNCDBFile::DeleteDatabase(void)
{
    m_Stmts.clear();
    CSQLITE_Connection::DeleteDatabase();
}


inline ENCDBFileType
CNCDBIndexFile::GetClassType(void)
{
    return eNCIndex;
}

inline
CNCDBIndexFile::CNCDBIndexFile(const string& file_name)
    : CNCDBFile(file_name, fJournalDelete | fVacuumOff, eNCIndex)
{}


inline ENCDBFileType
CNCDBMetaFile::GetClassType(void)
{
    return eNCMeta;
}

inline
CNCDBMetaFile::CNCDBMetaFile  (const string& file_name)
    : CNCDBFile(file_name, fJournalOff    | fVacuumOff, eNCMeta)
{}


inline ENCDBFileType
CNCDBDataFile::GetClassType(void)
{
    return eNCData;
}

inline
CNCDBDataFile::CNCDBDataFile  (const string& file_name)
    : CNCDBFile(file_name, fJournalOff    | fVacuumOff, eNCData)
{}



inline void
CNCFileSystem::SetDiskInitialized(void)
{
    sm_DiskInitialized = true;
}

inline bool
CNCFileSystem::IsDiskInitialized(void)
{
    return sm_DiskInitialized;
}


inline bool
CNCFSOpenFile::IsForcedSync(void)
{
    return (m_Flags & fForcedSync) != 0;
}

inline bool
CNCFSOpenFile::IsFileOpen(void)
{
    return (m_Flags & fOpened) != 0;
}

inline void
CNCFSOpenFile::SetFileOpen(bool value /* = true */)
{
    if (value)
        m_Flags |= fOpened;
    else
        m_Flags &= ~fOpened;
}

END_NCBI_SCOPE

#endif /* NETCACHE__NC_DB_FILES__HPP */
