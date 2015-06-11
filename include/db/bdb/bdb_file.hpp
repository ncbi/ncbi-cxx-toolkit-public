#ifndef BDB_FILE_HPP__
#define BDB_FILE_HPP__

/* $Id$
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
 * Author:  Anatoliy Kuznetsov
 *
 * File Description: Berkeley DB File classes.
 *
 */

/// @file bdb_file.hpp
/// BDB File management.

#include <corelib/ncbistre.hpp>
#include <util/itransaction.hpp>
#include <util/compress/compress.hpp>
#include <db/bdb/bdb_types.hpp>
#include <stdio.h>


BEGIN_NCBI_SCOPE

/** @addtogroup BDB_Files
 *
 * @{
 */


/// BDB Return codes
///


enum EBDB_ErrCode {
    eBDB_Ok,
    eBDB_NotFound,
    eBDB_KeyDup,
    eBDB_KeyEmpty,
    eBDB_MultiRowEnd
};


class CBDB_Env;
class CBDB_Transaction;


/// Raw file class wraps up basic Berkeley DB operations.
///
class NCBI_BDB_EXPORT CBDB_RawFile : public ITransactional
{
public:
    static const char kDefaultDatabase[];  // = "_table"

    /// BDB file open mode
    enum EOpenMode {
        eReadWrite,
        eReadOnly,
        eCreate,         //!< implies 'eReadWrite' too
        eReadWriteCreate //!< read-write, create if it doesn't exist
    };

    /// Berkeley DB database type
    enum EDBType {
        eBtree,
        eQueue,
        eHash
    };

    /// BLOB read mode, controld data buffer reallocation when
    /// there is not enough space in buffer
    ///
    enum EReallocMode {
        eReallocAllowed,
        eReallocForbidden
    };

    /// Control key duplicates in Btree
    enum EDuplicateKeys {
        eDuplicatesDisable,
        eDuplicatesEnable
    };

    enum EIgnoreError {
        eIgnoreError,
        eThrowOnError
    };

    /// BerkeleyDB compaction methods and flags
    enum ECompact {

        //< Do not attempt to free any pages from the underlying file
        eCompactNoFree,

        //< Free only pages that already exist as free pages in the file
        eCompactFreeExisting,

        //< Return all free pages to the file system after compaction
        eCompactFreeAll
    };

    /// typedef for raw buffer operations
    typedef CSimpleBuffer TBuffer;

public:
    CBDB_RawFile(EDuplicateKeys dup_keys = eDuplicatesDisable,
                 EDBType        db_type  = eBtree);
    virtual ~CBDB_RawFile();

    /// Associate file with environment. Should be called before
    /// file opening.
    void SetEnv(CBDB_Env& env);

    /// Get pointer on file environment
    /// Return NULL if no environment has been set
    CBDB_Env* GetEnv() { return m_Env; }

    /// Open file with specified access mode
    void Open(const string& filename,
              EOpenMode     open_mode,
              bool          support_dirty_read = false,
              unsigned      rec_len = 0);
    /// Open file with specified filename and database name.
    /// (Berkeley DB supports having several database tables in one file.)
    void Open(const string& filename,
              const string& database,
              EOpenMode   open_mode,
              bool        support_dirty_read = false,
              unsigned    rec_len = 0);
    /// Attach class to external BerkeleyDB file instance.
    /// Note: Should be already open.
    void Attach(CBDB_RawFile& bdb_file);
    /// Close file
    void Close();
    /// Reopen database file. (Should be already open).
    void Reopen(EOpenMode open_mode,
                bool      support_dirty_read = false,
                unsigned  rec_len = 0);

    /// Remove the database specified by the filename and database arguments
    void Remove(const string& filename, const string& database = kEmptyStr);
    /// Empty the database. Return number of records removed.
    unsigned int Truncate();
    /// Workaround for truncate of large databases. Executes out of transaction,
    /// and as such cannot be rolled back/
    unsigned int SafeTruncate();
    /// Rename a database.  NOTE: This cannot be called on an opened file.
    void Rename(const string& fname,
                const string& old_name,
                const string& new_name);

    /// Compact the database.  The target fill percent per page can be
    /// supplied, to allow for known expansion
    void Compact(ECompact compact_type = eCompactNoFree,
                 int target_fill_pct = 0);

    /// Extended version of compact
    /// This version performs iterative compacting and uses a callback
    /// to request an exit
    typedef bool (*FContinueCompact)(void);
    void CompactEx(FContinueCompact compact_callback,
                   ECompact compact_type = eCompactNoFree,
                   int target_fill_pct = 0);

    // Set Berkeley DB page size value. By default OS default is used.
    void SetPageSize(unsigned int page_size);

    // Get Berkeley DB page size value. By default OS default is used.
    unsigned int GetPageSize();

    /// Set Berkeley DB memory cache size for the file (default is 256K).
    void SetCacheSize(unsigned int cache_size);

    /// Turn OFF reverse splitting
    void RevSplitOff();

    /// Disable BTREE comparison override
    void DisableCmpOverride() { m_CmpOverride = false; }

    /// Set the priority for this database's pages in the buffer cache
    /// This is generally a temporary advisement, and works only if an
    /// environment is used.
    enum ECachePriority {
        eCache_Lowest,
        eCache_Low,
        eCache_Default,
        eCache_High,
        eCache_Highest
    };
    void SetCachePriority(ECachePriority priority);

    const string& FileName() const;
    const string& Database() const;

    /// Set comparison function. Default implementation installs bdb_types based
    /// function. Can be overloaded for some specific cases.
    virtual void SetCmp(DB*) = 0;

    /// Set hash function. Default implementation installs bdb_types based
    /// function. Can be overloaded for some specific cases.
    virtual void SetHash(DB*);

    /// Return TRUE if the file is open
    bool IsOpen() const;

    // Return TRUE if the file is attached to some other BDB file
    bool IsAttached() const;

    /// Return TRUE if the if the underlying database files were created
    /// on an architecture of the different byte order
    bool IsByteSwapped() const { return m_ByteSwapped; }

    /// Return TRUE if file can contain duplicate keys
    bool DuplicatesAllowed() const { return m_DuplicateKeys == eDuplicatesEnable; }

    /// Return the key duplicate mode value
    EDuplicateKeys GetDupKeysMode() const { return m_DuplicateKeys; }

    /// Return file name
    const string& GetFileName() const { return m_FileName; }

    /// Return the file open mode
    EOpenMode GetOpenMode() const { return m_OpenMode; }

    /// Flush any cached information to disk
    void Sync();

    /// Compute database statistic, return number of records.
    /// (Can be time consuming)
    unsigned CountRecs(bool bFast = false);

    /// Print database statistics
    void PrintStat(CNcbiOstream & out);

    // ITransactional:

    virtual void SetTransaction(ITransaction* trans);
    virtual void RemoveTransaction(ITransaction* trans);
    virtual ITransaction* GetTransaction();

    /// Get current transaction
    CBDB_Transaction* GetBDBTransaction() { return m_Trans; }

    /// Get record length
    /// Works for fixed length record DBs only (Queue)
    unsigned GetRecLen() const;

    /// Set hash table density (fill factor)
    void SetHashFillFactor(unsigned h_ffactor);

    /// Set an estimate of hash table final size
    void SetHashNelem(unsigned h_nelem);

    /// Disable hash method override
    /// (Berkeley DB will use it's own default hashing method)
    void DisableHashOverride() { m_CmpOverride = false; }

    /// Set the minimum number of keys per page (BTREE access methods only)
    void SetBtreeMinKeysPerPage(unsigned int keys_per_page);
    unsigned int GetBtreeMinKeysPerPage();


    /// Set record compressor
    ///
    /// Record compression should only be used if we do not
    /// use partial record storage and retrieval.
    ///
    void SetCompressor(ICompression* compressor,
                       EOwnership    own = eTakeOwnership);

private:
    /// forbidden
    CBDB_RawFile(const CBDB_RawFile&);
    CBDB_RawFile& operator= (const CBDB_RawFile&);

protected:
    void x_Open(const char* filename, const char* database,
                EOpenMode open_mode,
                bool      support_dirty_read,
                unsigned  rec_len);
    void x_Create(const char* filename, const char* database);

    void x_Close(EIgnoreError close_mode);

    /// Create m_DB member, set page, cache parameters
    ///
    /// @param rec_len
    ///    record length (must be non zero for Queue type)
    void x_CreateDB(unsigned rec_len);


    /// Set current transaction
    void x_SetTransaction(CBDB_Transaction* trans);

    void x_RemoveTransaction(CBDB_Transaction* trans);

    /// Get transaction handler.
    ///
    /// Function returns NULL if no transaction has been set.
    ///
    /// @sa SetTransaction
    DB_TXN* GetTxn();

    /// Create DB cursor
    DBC* CreateCursor(CBDB_Transaction* trans = 0,
                      unsigned int      flags = 0) const;


    /// Internal override for DB->get(...)
    /// This method overrides destination buffer and uses compressor:
    /// Should only be used with DB_DBT_USERMEM flag.
    ///
    int x_DB_Fetch(DBT *key,
                   DBT *data,
                   unsigned flags);


    /// Internal override for DBC->c_get(...)
    /// This method overrides destination buffer and uses compressor:
    /// Should only be used with DB_DBT_USERMEM flag.
    ///
    int x_DBC_Fetch(DBC* dbc,
                    DBT *key,
                    DBT *data,
                    unsigned flags);

    /// Override for DB->put(...)
    /// Handles compression.
    ///
    int x_DB_Put(DBT *key,
                 DBT *data,
                 unsigned flags);

    /// Override for DBC->c_put(...)
    /// Handles compression.
    ///
    int x_DB_CPut(DBC* dbc,
                  DBT *key,
                  DBT *data,
                  unsigned flags);


    int x_FetchBufferDecompress(DBT *data, void* usr_data);

    /// Set byte order swapping. Can be overloaded on derived classes
    /// @note When overloading DO call parent::x_SetByteSwapped
    virtual void x_SetByteSwapped(bool bswp);


protected:
    EDBType           m_DB_Type;
    DB*               m_DB;
    DBT*              m_DBT_Key;
    DBT*              m_DBT_Data;
    CBDB_Env*         m_Env;
    CBDB_Transaction* m_Trans;
    int               m_TransAssociation;
    unsigned          m_RecLen;
    unsigned          m_H_ffactor;
    unsigned          m_H_nelem;
    unsigned          m_BT_minkey;

    AutoPtr<ICompression> m_Compressor;    ///< Record compressor
    TBuffer           m_CompressBuffer;

private:
    bool             m_DB_Attached;    //!< TRUE if m_DB doesn't belong here
    bool             m_ByteSwapped;    //!< TRUE if file created on a diff.arch.
    bool             m_RevSplitOff;    //!< TRUE if reverse splitting is off
    bool             m_CmpOverride;    //!< TRUE - NCBI BDB sets its own cmp
    string           m_FileName;       //!< filename
    string           m_Database;       //!< db name in file (optional)
    unsigned         m_PageSize;
    unsigned         m_CacheSize;
    EDuplicateKeys   m_DuplicateKeys;
    EOpenMode        m_OpenMode;

    static const int kOpenFileMask;

    friend class CBDB_FileCursor;
};


/// Multirow buffer for reading many rows in one call
///

class NCBI_BDB_EXPORT CBDB_MultiRowBuffer
{
public:
    CBDB_MultiRowBuffer(size_t buf_size);
    ~CBDB_MultiRowBuffer();

    /// Get data buffer pointer from last cursor read
    const void* GetLastDataPtr() const { return m_LastData; }
    /// Get BLOB length from last cursor read
    size_t      GetLastDataLen() const { return m_LastDataLen; }
protected:
    void  InitDBT();
    void  MultipleInit();
private:
    CBDB_MultiRowBuffer(const CBDB_MultiRowBuffer&);
    CBDB_MultiRowBuffer& operator=(const CBDB_MultiRowBuffer&);
protected:
    DBT*     m_Data_DBT;  ///< Temp DBT for multiple fetch
    void*    m_Buf;       ///< Multiple row buffer
    size_t   m_BufSize;   ///< buffer size
    void*    m_BufPtr;    ///< current buffer position
    void*    m_LastKey;   ///< Last key pointer returned by DB_MULTIPLE_KEY_NEXT
    void*    m_LastData;  ///< Last data pointer returned by DB_MULTIPLE_KEY_NEXT
    size_t   m_LastKeyLen;
    size_t   m_LastDataLen;

friend class CBDB_File;
};



/// Berkeley DB file class.
/// Implements primary key and fields functionality.
///

class NCBI_BDB_EXPORT CBDB_File : public CBDB_RawFile
{
public:
    CBDB_File(EDuplicateKeys dup_keys = eDuplicatesDisable,
              EDBType        db_type  = eBtree);

    /// Open file with specified access mode
    void Open(const string& filename,
              EOpenMode     open_mode,
              bool          support_dirty_read = false,
              unsigned      rec_len = 0);

    /// Open file with specified filename and database name.
    /// (Berkeley DB supports having several database tables in one file.)
    void Open(const string& filename,
              const string& database,
              EOpenMode     open_mode,
              bool          support_dirty_read = false,
              unsigned      rec_len = 0);
    /// Reopen the db file
    void Reopen(EOpenMode open_mode, bool support_dirty_read = false);

    /// Attach external Berkeley DB file.
    /// Note: Should be already open.
    void Attach(CBDB_File& db_file);

    /// Fetches the record corresponding to the current key value.
    EBDB_ErrCode Fetch() { return x_Fetch(0); }

    /// Fetche the record corresponding to the current key value.
    /// Acquire write lock instead of read lock when doing the retrieval.
    /// Meaningful only in the presence of transactions.
    EBDB_ErrCode FetchForUpdate();

    enum EAfterWrite {
        eKeepData,    //!< Keep the inserted data for a while
        eDiscardData  //!< Invalidate the inserted data immediately after write
    };

    /// Insert new record
    EBDB_ErrCode Insert(EAfterWrite write_flag = eDiscardData);

    /// Append record to the queue
    /// (works only for DB_QUEUE database type)
    ///
    /// @return record number (auto increment)
    unsigned Append(EAfterWrite write_flag = eDiscardData);

    /// Delete record corresponding to the current key value.
    EBDB_ErrCode Delete(EIgnoreError on_error=eThrowOnError);

    /// Update record corresponding to the current key value. If record does not exist
    /// it will be inserted.
    EBDB_ErrCode UpdateInsert(EAfterWrite write_flag = eDiscardData);


    void BindKey (const char* field_name,
                  CBDB_Field* key_field,
                  size_t buf_size = 0);

    void BindData(const char* field_name,
                  CBDB_Field* data_field,
                  size_t buf_size = 0,
                  ENullable is_null = eNullable);

    /// Create the same fieldset as in dbf and bind them to the current file
    void DuplicateStructure(const CBDB_File& dbf);

    /// Get Buffer manager for key section of the file
    const CBDB_BufferManager* GetKeyBuffer() const { return m_KeyBuf.get(); }

    /// Get Buffer manager for data section of the file
    const CBDB_BufferManager* GetDataBuffer() const { return m_DataBuf.get(); }

    /// Get Buffer manager for key section of the file
    CBDB_BufferManager* GetKeyBuffer() { return m_KeyBuf.get(); }

    /// Get Buffer manager for data section of the file
    CBDB_BufferManager* GetDataBuffer() { return m_DataBuf.get(); }

    /// Sets maximum number of key fields participating in comparison
    /// Should be less than total number of key fields
    void SetFieldCompareLimit(unsigned int n_fields);

    /// Create new copy of m_DBT_Key.
    /// Caller is responsible for proper deletion. See also: DestroyDBT_Clone
    DBT* CloneDBT_Key();

    /// Free the DBT structure created by CloneDBT_Key.
    static void DestroyDBT_Clone(DBT* dbt);

    /// Set C-str detection
    void SetLegacyStringsCheck(bool value);

    /// CBDB_File keeps data in two buffers (key buffer and data buffer).
    /// TUnifiedFieldIndex is used to address fields in a non-ambigiuos manner.
    /// Negative index addresses fields in the key buffer, positive - data buffer
    /// Numbers are 1 based, 0 - means non-existing field
    typedef int TUnifiedFieldIndex;

    /// Get field index by name.
    /// @param
    ///   name field name to find (case insensitive)
    /// @return
    ///   Field index (0 if not found)
    /// @sa TUnifiedFieldIndex
    TUnifiedFieldIndex GetFieldIdx(const string& name) const;

    /// Return field by field index
    /// @param
    ///    idx field index
    /// @return field reference
    const CBDB_Field& GetField(TUnifiedFieldIndex idx) const;
    CBDB_Field& GetField(TUnifiedFieldIndex idx);

    /// Fields deletion is managed by the class when own_fields is TRUE
    void SetFieldOwnership(bool own_fields);

    /// Return fields ownership flag
    bool IsOwnFields() const { return m_OwnFields; }

    /// Copy record (fields) from another BDB file
    /// (MUST have the same structure)
    void CopyFrom(const CBDB_File& dbf);

    /// Run database verification (DB->verify)
    void Verify(const char* filename, const char* database, FILE* backup);

    /// Turn ON prefix compression.
    /// Should be turned on for string based keys. (not LString)
    void EnablePrefixCompression() { m_PrefixCompress = true; }

protected:
    /// Unpack internal record buffers
    void Discard();

    /// Set comparison function. Default implementation installs bdb_types based
    /// function. Can be overloaded for some specific cases.
    virtual void SetCmp(DB*);

    /// Read DB cursor
    EBDB_ErrCode ReadCursor(DBC* dbc, unsigned int bdb_flag);

    /// Read DB cursor (BLOB)
    EBDB_ErrCode ReadCursor(DBC* dbc, unsigned int bdb_flag,
                            void**       buf,
                            size_t       buf_size,
                            EReallocMode allow_realloc);
    /// Read DB cursor (BLOB)
    EBDB_ErrCode ReadCursor(DBC* dbc, unsigned int bdb_flag,
                            TBuffer* buf);


    /// Multiple-row read into a buffer
    /// Buffer is to be traversed using DB_MULTIPLE_KEY_NEXT (BerkeleyDB)
    ///
    /// @param multirow_only
    ///     Fetch only in multirow buffer, method returns eBDB_MultiRowEnd
    ///     when the buffer is over
    EBDB_ErrCode ReadCursor(DBC*                  dbc,
                            unsigned int          bdb_flag,
                            CBDB_MultiRowBuffer*  multirow_buf,
                            bool                  multirow_only);


    /// Write DB cursor
    EBDB_ErrCode WriteCursor(DBC* dbc, unsigned int bdb_flag,
                             EAfterWrite write_flag);

    /// Write BLOB to DB cursor
    EBDB_ErrCode WriteCursor(const void* data,
                             size_t      size,
                             DBC* dbc, unsigned int bdb_flag,
                             EAfterWrite write_flag);

    /// Delete DB cursor
    EBDB_ErrCode DeleteCursor(DBC* dbc, EIgnoreError);

    /// Check if all NOT NULL fields are assigned.
    /// Throw an exception if constraint check failed.
    void CheckNullDataConstraint() const;

    /// Function disables processing of m_DBT_data.
    /// This function can be used when creating custom BDB file
    /// data structures (BLOB storage, etc.) Caller takes full
    /// responsibility for filling m_DBT_Data with correct values.
    void DisableDataBufProcessing() { m_DataBufDisabled = true; }

    /// Disable NULL/not NULL in data fields
    /// (Performance tweak, call before BindData)
    void DisableNull() { m_DisabledNull = true; }

    /// Disable packing of variable length fields in the data buffer
    /// (Call after BindData)
    void DisableDataPacking();

    /// Wrapper around get operation.
    EBDB_ErrCode x_Fetch(unsigned int flags);

    virtual void x_SetByteSwapped(bool bswp);

private:
    /// forbidden
    CBDB_File(const CBDB_File&);
    CBDB_File& operator= (const CBDB_File&);

    /// Record reading prolog function
    void x_StartRead();
    /// Record reading epilog function
    void x_EndRead();

    EBDB_ErrCode x_Write(unsigned int flags, EAfterWrite write_flag,
                         DBC * dbc = 0);

    void x_CheckConstructBuffers();

    void x_ConstructKeyBuf();
    void x_ConstructDataBuf();

    static int x_CompareShim(DB* db, const DBT* dbt1, const DBT* dbt2,
                             size_t* locp);

private:
    auto_ptr<CBDB_BufferManager>   m_KeyBuf;
    auto_ptr<CBDB_BufferManager>   m_DataBuf;
    bool                           m_BufsAttached;
    bool                           m_BufsCreated;
    bool                           m_DataBufDisabled;
    bool                           m_LegacyString;
    bool                           m_OwnFields;
    bool                           m_DisabledNull;
    bool                           m_PrefixCompress; //!< TRUE if prefix compression ON

    friend class CBDB_FileCursor;
};



/// Berkeley DB file class optimized to work with
/// tables having int as the primary key.
///

class NCBI_BDB_EXPORT CBDB_IdFile : public CBDB_File
{
public:
    CBDB_FieldInt4  IdKey;

public:
    CBDB_IdFile();
    virtual void SetCmp(DB* db);
};



/* @} */


/////////////////////////////////////////////////////////////////////////////
//  IMPLEMENTATION of INLINE functions
/////////////////////////////////////////////////////////////////////////////


/// Make field index in CBDB_File format
///
/// @internal
inline
CBDB_File::TUnifiedFieldIndex BDB_GetUFieldIdx(int fidx, bool key)
{
    _ASSERT(fidx >= 0);
    ++fidx;
    return key ? (-fidx) : (fidx);
}


/////////////////////////////////////////////////////////////////////////////
//  CBDB_RawFile::
//


inline
void CBDB_RawFile::Open(
        const string& filename, EOpenMode open_mode,
        bool support_dirty_read,
        unsigned rec_len)
{
    Open(filename, kEmptyStr, open_mode, support_dirty_read, rec_len);
}

inline
unsigned CBDB_RawFile::GetRecLen() const
{
    _ASSERT(m_DB_Type == eQueue);
    return m_RecLen;
}

inline
const string& CBDB_RawFile::FileName() const
{
    return m_FileName;
}


inline
const string& CBDB_RawFile::Database() const
{
    return m_Database;
}


inline
bool CBDB_RawFile::IsOpen() const
{
    return !m_FileName.empty();
}

inline
bool CBDB_RawFile::IsAttached() const
{
    return m_DB_Attached;
}


/////////////////////////////////////////////////////////////////////////////
//
//  CBDB_File::
//



inline
void CBDB_File::Open(
  const string& filename, EOpenMode open_mode,
  bool support_dirty_read, unsigned rec_len)
{
    Open(filename, "", open_mode, support_dirty_read, rec_len);
}


inline
void CBDB_File::CheckNullDataConstraint() const
{
    if ( !m_DisabledNull && m_DataBuf.get() )
        m_DataBuf->CheckNullConstraint();
}

inline
void CBDB_File::SetFieldCompareLimit(unsigned int n_fields)
{
    m_KeyBuf->SetFieldCompareLimit(n_fields);
}


END_NCBI_SCOPE

#endif
