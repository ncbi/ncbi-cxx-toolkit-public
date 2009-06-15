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

#include <db/sqlite/sqlitewrapp.hpp>

#include "nc_db_info.hpp"


BEGIN_NCBI_SCOPE


class CNCDB_Stat;


/// Statement types used in CNCDB_File
enum ENCStmtType
{
    eStmt_CreateDBPartRow,   ///< Create info about new database part
    eStmt_UpdDBPartBlobId,   ///< Update value of minimum blob id in database
                             ///< part
    eStmt_UpdDBPartCreated,  ///< Update time when database was created to the
                             ///< current time
    eStmt_DeleteDBPartRow,   ///< Delete info about old database part
    eStmt_GetBlobId,         ///< Get id of blob by key
    eStmt_GetKeyIds,         ///< Get ids of blobs with given key
    eStmt_BlobExists,        ///< Check if blob with given key exists
    eStmt_CreateBlobKey,     ///< Create record about id and key of the blob
    eStmt_ReadBlobKey,       ///< Get key by blob id
    eStmt_DeleteBlobKey,     ///< Delete record about id and key of the blob
    eStmt_GetBlobIdsList,    ///< Get part of the full list of blob ids
    eStmt_WriteBlobInfo,     ///< Write all info about blob (without key info)
    eStmt_UpdateDeadTime,    ///< 
    eStmt_ReadBlobInfo,      ///< Get all info about blob (without key info)
    eStmt_DeleteBlobInfo,    ///< Delete record with info about blob
    eStmt_GetExpiredIds,     ///< Get ids of expired blobs
    eStmt_GetChunkIds,       ///< Get ids of all chunks for given blob
    eStmt_CreateChunk,       ///< Create new blob chunk
    eStmt_DeleteLastChunks,  ///< Delete last chunks for the blob
    eStmt_GetDataIdsList,    ///< Get ids of chunk data rows existing
                             ///< in database
    eStmt_CreateChunkData,   ///< Create new row with blob chunk data
    eStmt_WriteChunkData     ///< Write blob chunk data into existing chunk
};

/// Connection to one database file in NetCache storage.
/// Used for centralization of all only-database-related code. Actual using
/// of all methods can be only via derivative classes.
///
/// @sa CNCDB_IndexFile, CNCDB_MetaFile, CNCDB_DataFile, CNCDB_MemoryFile
class CNCDB_File : public CSQLITE_Connection
{
public:
    void DeleteDatabase(void);

protected:
    /// Create connection to NetCache database file
    CNCDB_File(CTempString     file_name,
               TOperationFlags flags,
               CNCDB_Stat*     stat);
    ~CNCDB_File(void);

    /// Get object gathering database statistics
    CNCDB_Stat* GetStat(void) const;

    /// Create entire database structure for Index file
    void CreateIndexDatabase(void);
    /// Create entire database structure for Meta file
    void CreateMetaDatabase(void);
    /// Create entire database structure for Data file
    void CreateDataDatabase(void);

    void CreateDBPartRow(SNCDBPartInfo* part_info);
    void UpdateDBPartBlobId(TNCDBPartId part_id, TNCBlobId blob_id);
    void UpdateDBPartCreated(SNCDBPartInfo* part_info);
    void DeleteDBPartRow(TNCDBPartId part_id);
    void GetAllDBParts(TNCDBPartsList* parts_list);
    void DeleteAllDBParts(void);

    /// Get last blob id used in the database
    TNCBlobId GetLastBlobId(void);
    /// Get blob id for given key, subkey and version
    bool ReadBlobId(SNCBlobIdentity* identity, int dead_time);
    /// Get ids of all blobs with given key
    void FillBlobIds(const string& key, int dead_time, TNCIdsList* id_list);
    /// Check if blob with given key and subkey exists
    bool IsBlobExists(const string& key, const string& subkey, int dead_time);
    /// Create record for the blob, return TRUE if it is successful.
    /// If record cannot be created because another one with the same key
    /// exists then return FALSE. If record cannot be created by another
    /// reason then CSQLITE_Exception is thrown.
    void CreateBlobKey(const SNCBlobIdentity& identity);
    /// Read information about key, subkey and version of the blob
    ///
    /// @return
    ///   TRUE if information was successfully read, FALSE if there's no such
    ///   blob id in database.
    bool ReadBlobKey(SNCBlobIdentity* identity, int dead_time);
    /// Delete record for the blob key
    void DeleteBlobKey(TNCBlobId blob_id);
    /// Read full list of blob ids in database
    void GetBlobIdsList(TNCBlobId&  start_id,
                        int&        start_dead_time,
                        int         end_dead_time,
                        int         max_count,
                        TNCIdsList* id_list);
    /// Write full meta-information for blob
    void WriteBlobInfo(const SNCBlobInfo& blob_info);
    /// 
    void UpdateBlobDeadTime(TNCBlobId blob_id, int dead_time);
    /// Read meta-information about blob - owner, ttl etc.
    ///
    /// @return
    ///   TRUE if information was successfully read, FALSE if there's no
    ///   record with information about this blob in database.
    bool ReadBlobInfo(SNCBlobInfo* blob_info);
    /// Delete record for the blob info
    void DeleteBlobInfo(TNCBlobId blob_id);
    /// Get ids of all expired blobs
    ///
    /// @param max_count
    ///   Maximum number of ids needed
    /// @param id_list
    ///   Pointer to ids list to fill
    void GetExpiredBlobIds(int max_count, TNCIdsList* id_list);

    /// Get ids for all chunks of the given blob
    void GetChunkIds(TNCBlobId blob_id, TNCIdsList* id_list);
    /// Create row with association chunk id - blob id
    void CreateChunkRow(TNCBlobId blob_id, TNCChunkId chunk_id);
    /// Delete ids of last chunks for the blob
    void DeleteLastChunkRows(TNCBlobId blob_id, TNCChunkId min_chunk_id);
    ///
    void GetChunkDataIdsList(int         start_from,
                             int         max_count,
                             TNCIdsList* id_list);
    /// Create new chunk value record with given data
    TNCChunkId CreateChunkValue(const TNCBlobBuffer& chunk_data);
    /// Write value of blob chunk with given id and data
    void WriteChunkValue(TNCChunkId chunk_id, const TNCBlobBuffer& chunk_data);
    /// Read blob chunk with given id into given buffer.
    void ReadChunkValue(TNCChunkId chunk_id, TNCBlobBuffer* chunk_data);

private:
    /// Create or get if already was created statement of given type
    CSQLITE_Statement* x_GetStatement(ENCStmtType typ);


    typedef AutoPtr<CSQLITE_Statement>                          TStatementPtr;
    typedef map<ENCStmtType, TStatementPtr/*, less<ENCStmtType>,
                CNCSmallDataAllocator<pair<const ENCStmtType,
                                           TStatementPtr> >*/ >   TStmtMap;

    /// Map of statement types to prepared statements
    TStmtMap                    m_Stmts;
    /// Storage object this connection created for
    CNCDB_Stat*                 m_Stat;
};


class CNCDB_IndexFile : public CNCDB_File
{
public:
    CNCDB_IndexFile(const string& file_name, CNCDB_Stat* stat);

    /// Create entire database
    void CreateDatabase(void);

    void CreateDBPartRow(SNCDBPartInfo* part_info);
    void UpdateDBPartBlobId(TNCDBPartId part_id, TNCBlobId blob_id);
    void UpdateDBPartCreated(SNCDBPartInfo* part_info);
    void DeleteDBPartRow(TNCDBPartId part_id);
    void GetAllDBParts(TNCDBPartsList* parts_list);
    void DeleteAllDBParts(void);
};


/// On-disk database with all meta-information about blobs and their chunks
class CNCDB_MetaFile : public CNCDB_File
{
public:
    CNCDB_MetaFile(const string& file_name, CNCDB_Stat* stat);

    /// Create entire database
    void CreateDatabase(void);
    /// Check if file does not contain information about any blobs
    bool IsEmpty(int dead_time);

    /// Get last blob id used in the database
    TNCBlobId GetLastBlobId(void);
    /// Get blob id for given key, subkey and version
    bool ReadBlobId(SNCBlobIdentity* identity, int dead_time);
    /// Get ids of all blobs with given key
    void FillBlobIds(const string& key, int dead_time, TNCIdsList* id_list);
    /// Check if blob with given key and subkey exists
    bool IsBlobExists(const string& key, const string& subkey, int dead_time);
    /// Create record for the blob
    void CreateBlobKey(const SNCBlobIdentity& identitys);
    /// Read information about key, subkey and version of the blob
    ///
    /// @return
    ///   TRUE if information was successfully read, FALSE if there's no such
    ///   blob id in database.
    bool ReadBlobKey(SNCBlobIdentity* identity, int dead_time);
    /// Delete record for the blob key
    void DeleteBlobKey(TNCBlobId blob_id);
    /// Read full list of blob ids in database
    void GetBlobIdsList(TNCBlobId&  start_id,
                        int&        start_dead_time,
                        int         end_dead_time,
                        int         max_count,
                        TNCIdsList* id_list);
    /// Write full meta-information for blob
    void WriteBlobInfo(const SNCBlobInfo& blob_info);
    /// 
    void UpdateBlobDeadTime(TNCBlobId blob_id, int dead_time);
    /// Read meta-information about blob - owner, ttl etc.
    ///
    /// @return
    ///   TRUE if information was successfully read, FALSE if there's no
    ///   record with information about this blob in database.
    bool ReadBlobInfo(SNCBlobInfo* blob_info);
    /// Delete record for the blob info
    void DeleteBlobInfo(TNCBlobId blob_id);
    /// Get ids of all expired blobs
    ///
    /// @param id_list
    ///   Pointer to ids list to fill
    /// @param max_count
    ///   Maximum number of ids needed
    void GetExpiredBlobIds(int max_count, TNCIdsList* id_list);

    /// Get ids for all chunks of the given blob
    void GetChunkIds(TNCBlobId blob_id, TNCIdsList* id_list);
    /// Create row with association chunk id - blob id
    void CreateChunkRow(TNCBlobId blob_id, TNCChunkId chunk_id);
    /// Delete ids of last chunks for the blob
    void DeleteLastChunkRows(TNCBlobId blob_id, TNCChunkId min_chunk_id);
};


class CNCDB_DataFile : public CNCDB_File
{
public:
    CNCDB_DataFile(const string& file_name, CNCDB_Stat* stat);

    /// Create entire database
    void CreateDatabase(void);
    ///
    bool IsEmpty(void);

    /// Create new chunk value record with given data
    TNCChunkId CreateChunkValue(const TNCBlobBuffer& chunk_data);
    /// Write value of blob chunk with given id and data
    void WriteChunkValue(TNCChunkId           chunk_id,
                         const TNCBlobBuffer& chunk_data);
    /// Read blob chunk with given id into given buffer.
    void ReadChunkValue(TNCChunkId chunk_id, TNCBlobBuffer* chunk_data);
};



inline
CNCDB_File::CNCDB_File(CTempString     file_name,
                       TOperationFlags flags,
                       CNCDB_Stat*     stat)
    : CSQLITE_Connection(file_name,
                         flags | fExternalMT   | fSyncOff
                               | fTempToMemory | fWritesSync),
      m_Stat(stat)
{
    _ASSERT(stat);
    SetCacheSize(2048);
}

inline
CNCDB_File::~CNCDB_File(void)
{
    m_Stmts.clear();
}

inline CNCDB_Stat*
CNCDB_File::GetStat(void) const
{
    return m_Stat;
}

inline void
CNCDB_File::DeleteDatabase(void)
{
    m_Stmts.clear();
    CSQLITE_Connection::DeleteDatabase();
}


inline
CNCDB_IndexFile::CNCDB_IndexFile(const string& file_name, CNCDB_Stat* stat)
    : CNCDB_File(file_name, fJournalDelete | fVacuumOn, stat)
{}

inline void
CNCDB_IndexFile::CreateDatabase(void)
{
    CreateIndexDatabase();
}

inline void
CNCDB_IndexFile::CreateDBPartRow(SNCDBPartInfo* part_info)
{
    CNCDB_File::CreateDBPartRow(part_info);
}

inline void
CNCDB_IndexFile::UpdateDBPartBlobId(TNCDBPartId part_id, TNCBlobId blob_id)
{
    CNCDB_File::UpdateDBPartBlobId(part_id, blob_id);
}

inline void
CNCDB_IndexFile::UpdateDBPartCreated(SNCDBPartInfo* part_info)
{
    CNCDB_File::UpdateDBPartCreated(part_info);
}

inline void
CNCDB_IndexFile::DeleteDBPartRow(TNCDBPartId part_id)
{
    CNCDB_File::DeleteDBPartRow(part_id);
}

inline void
CNCDB_IndexFile::GetAllDBParts(TNCDBPartsList* parts_list)
{
    CNCDB_File::GetAllDBParts(parts_list);
}

inline void
CNCDB_IndexFile::DeleteAllDBParts(void)
{
    CNCDB_File::DeleteAllDBParts();
}


inline
CNCDB_MetaFile::CNCDB_MetaFile(const string& file_name, CNCDB_Stat* stat)
    : CNCDB_File(file_name, fJournalPersist | fVacuumManual, stat)
{}

inline void
CNCDB_MetaFile::CreateDatabase(void)
{
    CreateMetaDatabase();
}

inline TNCBlobId
CNCDB_MetaFile::GetLastBlobId(void)
{
    return CNCDB_File::GetLastBlobId();
}

inline bool
CNCDB_MetaFile::ReadBlobId(SNCBlobIdentity* identity, int dead_time)
{
    return CNCDB_File::ReadBlobId(identity, dead_time);
}

inline void
CNCDB_MetaFile::FillBlobIds(const string& key,
                            int           dead_time,
                            TNCIdsList*   id_list)
{
    CNCDB_File::FillBlobIds(key, dead_time, id_list);
}

inline bool
CNCDB_MetaFile::IsBlobExists(const string& key,
                             const string& subkey,
                             int           dead_time)
{
    return CNCDB_File::IsBlobExists(key, subkey, dead_time);
}

inline void
CNCDB_MetaFile::CreateBlobKey(const SNCBlobIdentity& identity)
{
    CNCDB_File::CreateBlobKey(identity);
}

inline bool
CNCDB_MetaFile::ReadBlobKey(SNCBlobIdentity* identity, int dead_time)
{
    return CNCDB_File::ReadBlobKey(identity, dead_time);
}

inline void
CNCDB_MetaFile::DeleteBlobKey(TNCBlobId blob_id)
{
    CNCDB_File::DeleteBlobKey(blob_id);
}

inline void
CNCDB_MetaFile::GetBlobIdsList(TNCBlobId&  start_id,
                               int&        start_dead_time,
                               int         end_dead_time,
                               int         max_count,
                               TNCIdsList* id_list)
{
    CNCDB_File::GetBlobIdsList(start_id, start_dead_time, end_dead_time,
                               max_count, id_list);
}

inline bool
CNCDB_MetaFile::IsEmpty(int dead_time)
{
    TNCBlobId last_id = 0;
    TNCIdsList id_list;
    GetBlobIdsList(last_id, dead_time, numeric_limits<int>::max(),
                   1, &id_list);
    return id_list.empty();
}

inline void
CNCDB_MetaFile::WriteBlobInfo(const SNCBlobInfo& blob_info)
{
    CNCDB_File::WriteBlobInfo(blob_info);
}

inline void
CNCDB_MetaFile::UpdateBlobDeadTime(TNCBlobId blob_id, int dead_time)
{
    CNCDB_File::UpdateBlobDeadTime(blob_id, dead_time);
}

inline bool
CNCDB_MetaFile::ReadBlobInfo(SNCBlobInfo* blob_info)
{
    return CNCDB_File::ReadBlobInfo(blob_info);
}

inline void
CNCDB_MetaFile::DeleteBlobInfo(TNCBlobId blob_id)
{
    CNCDB_File::DeleteBlobInfo(blob_id);
}

inline void
CNCDB_MetaFile::GetExpiredBlobIds(int max_count, TNCIdsList* id_list)
{
    CNCDB_File::GetExpiredBlobIds(max_count, id_list);
}

inline void
CNCDB_MetaFile::GetChunkIds(TNCBlobId blob_id, TNCIdsList* id_list)
{
    CNCDB_File::GetChunkIds(blob_id, id_list);
}

inline void
CNCDB_MetaFile::CreateChunkRow(TNCBlobId blob_id, TNCChunkId chunk_id)
{
    CNCDB_File::CreateChunkRow(blob_id, chunk_id);
}

inline void
CNCDB_MetaFile::DeleteLastChunkRows(TNCBlobId  blob_id,
                                    TNCChunkId min_chunk_id)
{
    CNCDB_File::DeleteLastChunkRows(blob_id, min_chunk_id);
}


inline
CNCDB_DataFile::CNCDB_DataFile(const string& file_name, CNCDB_Stat* stat)
    : CNCDB_File(file_name, fJournalPersist | fVacuumManual, stat)
{}

inline void
CNCDB_DataFile::CreateDatabase(void)
{
    CreateDataDatabase();
}

inline bool
CNCDB_DataFile::IsEmpty(void)
{
    TNCIdsList id_list;
    GetChunkDataIdsList(1, 1, &id_list);
    return id_list.empty();
}

inline TNCChunkId
CNCDB_DataFile::CreateChunkValue(const TNCBlobBuffer& chunk_data)
{
    return CNCDB_File::CreateChunkValue(chunk_data);
}

inline void
CNCDB_DataFile::WriteChunkValue(TNCChunkId           chunk_id,
                                const TNCBlobBuffer& chunk_data)
{
    CNCDB_File::WriteChunkValue(chunk_id, chunk_data);
}

inline void
CNCDB_DataFile::ReadChunkValue(TNCChunkId chunk_id, TNCBlobBuffer* chunk_data)
{
    CNCDB_File::ReadChunkValue(chunk_id, chunk_data);
}

END_NCBI_SCOPE

#endif /* NETCACHE_NC_DB_FILES__HPP */
