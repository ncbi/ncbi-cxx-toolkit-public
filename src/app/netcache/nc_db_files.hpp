#ifndef NETCACHE_NC_DB_FILES__HPP
#define NETCACHE_NC_DB_FILES__HPP
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
    eStmt_CreateDBPart,      ///< Create new database part
    eStmt_DeleteDBPart,      ///< Delete info about old database part
    eStmt_GetBlobId,         ///< Get id of blob by key, subkey, version
    eStmt_BlobFamilyExists,  ///< Check if blob with given key, subkey exists
    eStmt_BlobExists,        ///< Check if blob with given key, subkey, version
                             ///< and id exists
    eStmt_CreateBlobKey,     ///< Create record about id and key, subkey,
                             ///< version of the blob
    eStmt_GetBlobsList,      ///< Get part of list of blobs "alive" in
                             ///< the given time frame
    eStmt_WriteBlobInfo,     ///< Write all meta-info about blob
    eStmt_SetBlobDeadTime,   ///< Set blob's expiration time to given value
    eStmt_ReadBlobInfo,      ///< Get all meta-info about blob
    eStmt_GetChunkIds,       ///< Get ids of all chunks for given blob
    eStmt_CreateChunk,       ///< Create new blob chunk
    eStmt_DeleteLastChunks,  ///< Delete last chunks for the blob
    eStmt_CreateChunkData,   ///< Create new record with blob chunk data
    eStmt_WriteChunkData,    ///< Write blob chunk data into existing chunk
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
              CNCDBStat*      stat);
    virtual ~CNCDBFile(void);

    /// Get object gathering database statistics
    CNCDBStat* GetStat(void) const;

    /// Create entire database structure for Index file
    void CreateIndexDatabase(void);
    /// Create entire database structure for Meta file
    void CreateMetaDatabase(void);
    /// Create entire database structure for Data file
    void CreateDataDatabase(void);
    /// Convert database structure for Meta file from previous version
    void AdjustMetaDatabase(void);

    /// Create new database part and save information about it.
    /// Creation time in given info structure is set to current time.
    void CreateDBPart(SNCDBPartInfo* part_info);
    /// Delete database part
    void DeleteDBPart(TNCDBPartId part_id);
    /// Read information about all database parts in order of their creation
    void GetAllDBParts(TNCDBPartsList* parts_list);
    /// Clean index database removing information about all database parts.
    void DeleteAllDBParts(void);

    /// Get last blob id used in the database
    TNCBlobId GetLastBlobId(void);
    /// Get id of existent blob with given key, subkey and version. Only blobs
    /// alive at the dead_after moment or after that are considered existing.
    /// Id of the blob is read right into the identity structure.
    ///
    /// @return
    ///   TRUE if blob exists and its id was successfully read from database,
    ///   FALSE if blob doesn't exist.
    bool ReadBlobId(SNCBlobIdentity* identity, int dead_after);
    /// Check if blob with given key and subkey exists (more than one blob
    /// with given key and subkey can exist). Only blobs alive at
    /// the dead_after moment or after that are considered existing.
    bool IsBlobFamilyExists(const string& key,
                            const string& subkey,
                            int           dead_after);
    /// Check if blob with full given identification (id, key, subkey and
    /// version) exists. Only blobs alive at the dead_after moment or after
    /// that are considered existing.
    bool IsBlobExists(const SNCBlobIdentity& identity, int dead_after);
    /// Create record for blob's id, key, subkey and version.
    void CreateBlobKey(const SNCBlobIdentity& identity);
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

    /// Write full meta-information for blob
    void WriteBlobInfo(SNCBlobInfo& blob_info, bool move_dead_time);
    /// Change "dead time" for the blob, i.e. when blob will expire and after
    /// what moment it will be considered not existing.
    void SetBlobDeadTime(TNCBlobId blob_id, int dead_time);
    /// Read meta-information about blob - owner, ttl etc.
    ///
    /// @return
    ///   TRUE if meta-information was found in the database, FALSE otherwise
    bool ReadBlobInfo(SNCBlobInfo* blob_info);

    /// Get ids for all chunks of the given blob
    void GetChunkIds(TNCBlobId blob_id, TNCChunksList* id_list);
    /// Create record with association chunk id -> blob id
    void CreateChunk(TNCBlobId blob_id, TNCChunkId chunk_id);
    /// Delete ids of last chunks for the blob
    ///
    /// @param blob_id
    ///   Id of the blob which chunks should be deleted
    /// @param min_chunk_id
    ///   Minimum id of chunks to be deleted - all chunks with ids equal or
    ///   greater than this id will be deleted.
    void DeleteLastChunks(TNCBlobId blob_id, TNCChunkId min_chunk_id);
    /// Create new chunk value record with given data
    ///
    /// @param data
    ///   Data to write to the chunk
    /// @return
    ///   Id of the created chunk data
    TNCChunkId CreateChunkValue(const TNCBlobBuffer& data);
    /// Write value of blob chunk
    ///
    /// @param chunk_id
    ///   Id of the chunk to write
    /// @param data
    ///   Data to write to the chunk
    void WriteChunkValue(TNCChunkId chunk_id, const TNCBlobBuffer& data);
    /// Read blob chunk value
    ///
    /// @param chunk_id
    ///   Id of the chunk to read value from
    /// @param buffer
    ///   Buffer for the data to read to
    /// @return
    ///   TRUE if chunk was found in the databse, FALSE otherwise (meaning
    ///   that database was corrupted in some way)
    bool ReadChunkValue(TNCChunkId chunk_id, TNCBlobBuffer* buffer);

protected:
    /// Add to the given CREATE TABLE statement definition of subkey and
    /// version columns and index for all key columns.
    /// Nothing is done in the base implementation here.
    virtual void AddSubkeyColsDef(CQuickStrStream& sql);
    /// Create sql for the statement which behavior depends on the file type
    /// (for pure NetCache or for ICache).
    /// Nothing is done in the base implementation here.
    virtual void GetSpecificStatement(ENCStmtType typ, CQuickStrStream& sql);
    /// Bind values of subkey and version to the statement if necessary.
    /// Nothing is done in the base implementation here.
    virtual void BindBlobSubkey(CSQLITE_Statement*     stmt,
                                const SNCBlobIdentity& identity);
    /// Bind value of subkey to the statement if necessary.
    /// Nothing is done in the base implementation here.
    virtual void BindBlobSubkey(CSQLITE_Statement*     stmt,
                                const string&          subkey);
    /// Read from result set returned by given statement values of subkey and
    /// version starting from column with number col_num.
    /// Nothing is done in the base implementation here.
    virtual void ReadBlobSubkey(CSQLITE_Statement*     stmt,
                                int                    col_num,
                                SNCBlobIdentity*       identity);

private:
    CNCDBFile(const CNCDBFile&);
    CNCDBFile& operator= (const CNCDBFile&);

    /// Create or get if already was created statement of given type.
    /// All created and prepared statements will be cached and re-used over
    /// the lifetime of database connection object.
    CSQLITE_Statement* x_GetStatement(ENCStmtType typ);


    typedef AutoPtr<CSQLITE_Statement>       TStatementPtr;
    typedef map<ENCStmtType, TStatementPtr>  TStmtMap;

    /// Map associating statement types to prepared statements
    TStmtMap     m_Stmts;
    /// Object for gathering database statistics
    CNCDBStat*  m_Stat;
};


/// Connection to index database in NetCache storage. Database contains
/// information about storage parts containing actual blobs data.
class CNCDBIndexFile : public CNCDBFile
{
public:
    /// Create connection to index database file
    ///
    /// @param file_name
    ///   Name of the database file
    /// @param stat
    ///   Object for gathering database statistics
    CNCDBIndexFile(const string& file_name, CNCDBStat* stat);
    virtual ~CNCDBIndexFile(void);

    /// Create entire database
    void CreateDatabase(void);

    using CNCDBFile::CreateDBPart;
    using CNCDBFile::DeleteDBPart;
    using CNCDBFile::GetAllDBParts;
    using CNCDBFile::DeleteAllDBParts;
};


/// Connection to the database file in NetCache storage containing all
/// meta-information about blobs and their chunks.
class CNCDBMetaFile : public CNCDBFile
{
public:
    virtual ~CNCDBMetaFile(void);

    /// Create entire database
    void CreateDatabase(void);
    /// Check if file contains information about any existing blobs.
    /// Only blobs alive at the dead_after moment or after that are considered
    /// existing.
    bool HasLiveBlobs(int dead_after);
    /// Check if file contains information about any blobs (existing or not
    /// existing).
    bool HasAnyBlobs(void);

    using CNCDBFile::AdjustMetaDatabase;

    using CNCDBFile::GetLastBlobId;
    using CNCDBFile::ReadBlobId;
    using CNCDBFile::IsBlobFamilyExists;
    using CNCDBFile::IsBlobExists;
    using CNCDBFile::CreateBlobKey;
    using CNCDBFile::GetBlobsList;

    using CNCDBFile::WriteBlobInfo;
    using CNCDBFile::SetBlobDeadTime;
    using CNCDBFile::ReadBlobInfo;

    using CNCDBFile::GetChunkIds;
    using CNCDBFile::CreateChunk;
    using CNCDBFile::DeleteLastChunks;

protected:
    /// Create connection to database file with blobs' meta-information
    ///
    /// @param file_name
    ///   Name of the database file
    /// @param stat
    ///   Object for gathering database statistics
    CNCDBMetaFile(const string& file_name, CNCDBStat* stat);
};


/// Connection to the database file in NetCache storage containing all
/// meta-information about blobs and their chunks. Class is specific for
/// ICache blobs storage.
class CNCDBMetaFile_ICache : public CNCDBMetaFile
{
public:
    /// Create connection to database file with blobs' meta-information - file
    /// specific for ICache blobs storage.
    ///
    /// @param file_name
    ///   Name of the database file
    /// @param stat
    ///   Object for gathering database statistics
    CNCDBMetaFile_ICache(const string& file_name, CNCDBStat* stat);
    virtual ~CNCDBMetaFile_ICache(void);

private:
    /// Add to the given CREATE TABLE statement definition of subkey and
    /// version columns and index for all key columns.
    virtual void AddSubkeyColsDef(CQuickStrStream& sql);
    /// Create sql for the statement which behavior depends on the file type
    /// (for pure NetCache or for ICache).
    virtual void GetSpecificStatement(ENCStmtType typ, CQuickStrStream& sql);
    /// Bind values of subkey and version to the statement if necessary.
    virtual void BindBlobSubkey(CSQLITE_Statement*     stmt,
                                const SNCBlobIdentity& identity);
    /// Bind value of subkey to the statement if necessary.
    virtual void BindBlobSubkey(CSQLITE_Statement*     stmt,
                                const string&          subkey);
    /// Read from result set returned by given statement values of subkey and
    /// version starting from column with number col_num.
    virtual void ReadBlobSubkey(CSQLITE_Statement*     stmt,
                                int                    col_num,
                                SNCBlobIdentity*       identity);
};


/// Connection to the database file in NetCache storage containing all
/// meta-information about blobs and their chunks. Class is specific for
/// pure NetCache blobs storage.
class CNCDBMetaFile_NCCache : public CNCDBMetaFile
{
public:
    /// Create connection to database file with blobs' meta-information - file
    /// specific for pure NetCache blobs storage.
    ///
    /// @param file_name
    ///   Name of the database file
    /// @param stat
    ///   Object for gathering database statistics
    CNCDBMetaFile_NCCache(const string& file_name, CNCDBStat* stat);
    virtual ~CNCDBMetaFile_NCCache(void);

private:
    /// Add to the given CREATE TABLE statement definition of and index for
    /// key column.
    virtual void AddSubkeyColsDef(CQuickStrStream& sql);
    /// Create sql for the statement which behavior depends on the file type
    virtual void GetSpecificStatement(ENCStmtType typ, CQuickStrStream& sql);
};


/// Connection to the database file in NetCache storage containing all
/// actual blobs data.
class CNCDBDataFile : public CNCDBFile
{
public:
    /// Create connection to database file with blobs' data
    ///
    /// @param file_name
    ///   Name of the database file
    /// @param stat
    ///   Object for gathering database statistics
    CNCDBDataFile(const string& file_name, CNCDBStat* stat);
    virtual ~CNCDBDataFile(void);

    /// Create entire database
    void CreateDatabase(void);

    using CNCDBFile::CreateChunkValue;
    using CNCDBFile::WriteChunkValue;
    using CNCDBFile::ReadChunkValue;
};


/// Factory class for creating connections to database files.
/// Class creates one connection for each thread and deletes them all in
/// destructor.
///
/// @param TFile
///   Type of file connection to create by this factory.
template <class TFile>
class CNCDBFileObjFactory : public CNCTlsObject<CNCDBFileObjFactory<TFile>,
                                                TFile>
{
    typedef CNCTlsObject<CNCDBFileObjFactory<TFile>, TFile>  TBase;

public:
    /// Both parameters of constructor are passed unchanged to file
    /// connection constructor.
    ///
    /// @param file_name
    ///   Name of database file all objects will connect to
    /// @param stat
    ///   Object collecting database statistics
    CNCDBFileObjFactory(const string& file_name, CNCDBStat* stat);
    /// Destructor cleans up all file objects that it created in all threads.
    ~CNCDBFileObjFactory(void);

    /// Create new object.
    /// Part of the interface required by CNCTlsObject.
    TFile* CreateTlsObject(void);
    /// Delete object.
    /// Part of the interface required by CNCTlsObject.
    static void DeleteTlsObject(void* obj_ptr);

    /// Get name of the file all connections will be created to.
    const string& GetFileName(void) const;

private:
    /// Name of database file all objects will connect to
    string       m_FileName;
    /// Object for collecting database statistics
    CNCDBStat*   m_Stat;
    /// Mutex for creation of new file objects (actually for work with
    /// m_AllFiles).
    CSpinLock    m_CreateLock;
    /// List of all file objects created by this factory.
    list<TFile*> m_AllFiles;
};

/// Factories for database files with meta-information and with actual data
typedef CNCDBFileObjFactory<CNCDBMetaFile_ICache>  TNCMetaFileFactory_ICache;
typedef CNCDBFileObjFactory<CNCDBMetaFile_NCCache> TNCMetaFileFactory_NCCache;
typedef CNCDBFileObjFactory<CNCDBDataFile>         TNCDataFileFactory;


/// Central pool of connections to database files belonging to one database
/// part. Class created for convenience of implementation of different
/// template methods and classes. Class is abstract, specific versions
/// (*_ICache or *_NCCache) should be used.
class CNCDBFilesPool
{
public:
    virtual ~CNCDBFilesPool(void);

    /// Get file of necessary type from pool.
    /// Method cannot be made as simple template because GCC 3.0.4 refuses to
    /// compile calls to such template method.
    virtual void GetFile(CNCDBMetaFile** file_ptr) = 0;
    void         GetFile(CNCDBDataFile** file_ptr);
    /// Return file to the pool
    void ReturnFile     (CNCDBMetaFile*  file);
    void ReturnFile     (CNCDBDataFile*  file);

    /// Get name of files this pool is for
    const string& GetMetaFileName(void) const;
    const string& GetDataFileName(void) const;

protected:
    /// Create database part's file pool
    ///
    /// @param meta_name
    ///   Name of meta file in database part
    /// @param data_name
    ///   Name of data file in database part
    /// @param stat
    ///   Object for gathering database statistics
    CNCDBFilesPool(const string& meta_name,
                   const string& data_name,
                   CNCDBStat*    stat);

private:
    CNCDBFilesPool(const CNCDBFilesPool&);
    CNCDBFilesPool& operator= (const CNCDBFilesPool&);

    /// Factory for per-thread data file connections
    TNCDataFileFactory m_Datas;
    /// Name of meta file that will be used by this pool
    string             m_MetaName;
};

/// Central pool of connections to database files for ICache-related storage.
class CNCDBFilesPool_ICache : public CNCDBFilesPool
{
public:
    /// Create database part's file pool
    ///
    /// @param meta_name
    ///   Name of meta file in database part
    /// @param data_name
    ///   Name of data file in database part
    /// @param stat
    ///   Object for gathering database statistics
    CNCDBFilesPool_ICache(const string& meta_name,
                          const string& data_name,
                          CNCDBStat*    stat);
    virtual ~CNCDBFilesPool_ICache(void);
    /// Get meta file from pool.
    virtual void GetFile(CNCDBMetaFile** file_ptr);

private:
    /// Factory for per-thread meta file connections
    TNCMetaFileFactory_ICache m_Metas;
};

/// Central pool of connections to database files for pure NetCache-related
/// storage.
class CNCDBFilesPool_NCCache : public CNCDBFilesPool
{
public:
    /// Create database part's file pool
    ///
    /// @param meta_name
    ///   Name of meta file in database part
    /// @param data_name
    ///   Name of data file in database part
    /// @param stat
    ///   Object for gathering database statistics
    CNCDBFilesPool_NCCache(const string& meta_name,
                           const string& data_name,
                           CNCDBStat*    stat);
    virtual ~CNCDBFilesPool_NCCache(void);
    /// Get meta file from pool.
    virtual void GetFile(CNCDBMetaFile** file_ptr);

private:
    /// Factory for per-thread meta file connections
    TNCMetaFileFactory_NCCache m_Metas;
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
    static void Initialize(void);
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
    static CAtomicCounter       sm_WaitingOnAlert;
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
    void WriteFirstPage(Int8 offset, const void** data, int cnt);
    /// Adjust remembered size of the file so that it's not less than given
    /// value.
    void AdjustSize(Int8 size);

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



//////////////////////////////////////////////////////////////////////////
// Inline methods
//////////////////////////////////////////////////////////////////////////

inline
CNCDBFile::CNCDBFile(CTempString     file_name,
                     TOperationFlags flags,
                     CNCDBStat*      stat)
    // If fSyncOff in here was changed to something else then behavior
    // in CNCFSVirtFile should have been changed appropriately.
    : CSQLITE_Connection(file_name,
                         flags | fExternalMT   | fSyncOff
                               | fTempToMemory | fWritesSync),
      m_Stat(stat)
{
    _ASSERT(stat);
    SetCacheSize(kNCSQLitePageSize);
}

inline CNCDBStat*
CNCDBFile::GetStat(void) const
{
    return m_Stat;
}

inline void
CNCDBFile::DeleteDatabase(void)
{
    m_Stmts.clear();
    CSQLITE_Connection::DeleteDatabase();
}


inline
CNCDBIndexFile::CNCDBIndexFile(const string& file_name, CNCDBStat* stat)
    : CNCDBFile(file_name, fJournalDelete | fVacuumOn, stat)
{}

inline void
CNCDBIndexFile::CreateDatabase(void)
{
    CreateIndexDatabase();
}


inline
CNCDBMetaFile::CNCDBMetaFile(const string& file_name, CNCDBStat* stat)
    : CNCDBFile(file_name, fJournalOff | fVacuumManual, stat)
{}

inline void
CNCDBMetaFile::CreateDatabase(void)
{
    CreateMetaDatabase();
}

inline bool
CNCDBMetaFile::HasLiveBlobs(int dead_after)
{
    TNCBlobId id_after = 0;
    TNCBlobsList blobs_list;
    GetBlobsList(dead_after, id_after, numeric_limits<int>::max(),
                 1, &blobs_list);
    return !blobs_list.empty();
}

inline bool
CNCDBMetaFile::HasAnyBlobs(void)
{
    int dead_after = 0;
    TNCBlobId id_after = 0;
    TNCBlobsList blobs_list;
    GetBlobsList(dead_after, id_after, numeric_limits<int>::max(),
                 1, &blobs_list);
    return !blobs_list.empty();
}


inline
CNCDBMetaFile_ICache::CNCDBMetaFile_ICache(const string& file_name,
                                           CNCDBStat*    stat)
    : CNCDBMetaFile(file_name, stat)
{}


inline
CNCDBMetaFile_NCCache::CNCDBMetaFile_NCCache(const string& file_name,
                                             CNCDBStat*    stat)
    : CNCDBMetaFile(file_name, stat)
{}


inline
CNCDBDataFile::CNCDBDataFile(const string& file_name, CNCDBStat* stat)
    : CNCDBFile(file_name, fJournalOff | fVacuumManual, stat)
{}

inline void
CNCDBDataFile::CreateDatabase(void)
{
    CreateDataDatabase();
}



template <class TFile>
inline
CNCDBFileObjFactory<TFile>::CNCDBFileObjFactory(const string& file_name,
                                                CNCDBStat*    stat)
    : m_FileName(file_name),
      m_Stat(stat)
{
    TBase::Initialize();
}

template <class TFile>
inline
CNCDBFileObjFactory<TFile>::~CNCDBFileObjFactory(void)
{
    ITERATE(typename list<TFile*>, it, m_AllFiles) {
        delete *it;
    }
    m_AllFiles.clear();
    TBase::Finalize();
}

template <class TFile>
inline TFile*
CNCDBFileObjFactory<TFile>::CreateTlsObject(void)
{
    _ASSERT(!m_FileName.empty()  &&  m_Stat);
    // Make it a unique object for each thread for now. If it changes then
    // CNCDBFilesPool::GetFile and CNCDBFilesPool::ReturnFile should be
    // changed too.
    TFile* file = new TFile(m_FileName, m_Stat);

    m_CreateLock.Lock();
    m_AllFiles.push_back(file);
    m_CreateLock.Unlock();

    return file;
}

template <class TFile>
inline void
CNCDBFileObjFactory<TFile>::DeleteTlsObject(void*)
{
    // Nothing to do now because it's static
}

template <class TFile>
inline const string&
CNCDBFileObjFactory<TFile>::GetFileName(void) const
{
    return m_FileName;
}



inline
CNCDBFilesPool::CNCDBFilesPool(const string& meta_name,
                               const string& data_name,
                               CNCDBStat*    stat)
    : m_Datas(data_name, stat),
      m_MetaName(meta_name)
{}

inline void
CNCDBFilesPool::GetFile(CNCDBDataFile** file_ptr)
{
    *file_ptr = m_Datas.GetObjPtr();
}

inline void
CNCDBFilesPool::ReturnFile(CNCDBMetaFile*)
{
    // Nothing to be done for now until file objects are re-used for different
    // threads.
}

inline void
CNCDBFilesPool::ReturnFile(CNCDBDataFile*)
{
    // Nothing to be done for now until file objects are re-used for different
    // threads.
}

inline const string&
CNCDBFilesPool::GetMetaFileName(void) const
{
    return m_MetaName;
}

inline const string&
CNCDBFilesPool::GetDataFileName(void) const
{
    return m_Datas.GetFileName();
}


inline
CNCDBFilesPool_ICache::CNCDBFilesPool_ICache(const string& meta_name,
                                             const string& data_name,
                                             CNCDBStat*    stat)
    : CNCDBFilesPool(meta_name, data_name, stat),
      m_Metas(meta_name, stat)
{}


inline
CNCDBFilesPool_NCCache::CNCDBFilesPool_NCCache(const string& meta_name,
                                               const string& data_name,
                                               CNCDBStat*    stat)
    : CNCDBFilesPool(meta_name, data_name, stat),
      m_Metas(meta_name, stat)
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

inline Int8
CNCFSOpenFile::GetSize(void)
{
    return m_Size;
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

#endif /* NETCACHE_NC_DB_FILES__HPP */
