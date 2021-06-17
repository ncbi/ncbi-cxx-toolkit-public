#ifndef BDB___BLOB_HPP__
#define BDB___BLOB_HPP__

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
 * File Description: BDB library BLOB support.
 *
 */

/// @file bdb_blob.hpp
/// BDB library BLOB support.

#include <corelib/reader_writer.hpp>
#include <db/bdb/bdb_file.hpp>


BEGIN_NCBI_SCOPE

/** @addtogroup BDB_BLOB
 *
 * @{
 */


class CBDB_BLobStream;
class CBDB_BlobReaderWriter;

/// Berkeley DB BLOB File class.
///
/// The basic BLOB file. Key part of the file consists of one or more
/// fields. Data part is one binary object(BLOB).

class NCBI_BDB_EXPORT CBDB_BLobFile : public CBDB_File
{
public:

    CBDB_BLobFile(EDuplicateKeys dup_keys = eDuplicatesDisable,
                  EDBType        db_type  = eBtree);


    /// Insert BLOB into the database
    ///
    /// Before calling this function you should assign the key fields.
    /// @param data BLOB data
    /// @param size data size in bytes
    EBDB_ErrCode Insert(const void* data, size_t size);

    /// Insert BLOB
    EBDB_ErrCode Insert(const TBuffer& buf);

    unsigned Append(const void* data, size_t size);

    /// Insert or update BLOB
    ///
    /// Before calling this function you should assign the key fields.
    /// @param data BLOB data
    /// @param size data size in bytes
    EBDB_ErrCode UpdateInsert(const void* data, size_t size);

    /// Insert or update BLOB
    EBDB_ErrCode UpdateInsert(const TBuffer& buf);


    /// Fetch the record corresponding to the current key value.
    ///
    /// Key fields should be assigned before calling this function.
    /// This call actually translates into a BerkeleyDB call, so the target page
    /// will be read from the disk into the BerkeleyDB internal cache.
    /// You can call LobSize to get BLOB data size, allocate the target
    /// buffer and then call GetData (two phase BLOB fetch).
    /// If you know the data size upfront parameterized Fetch is a
    /// better alternative.
    EBDB_ErrCode Fetch();

    /// Retrieve BLOB data
    ///
    /// Fetch LOB record directly into the provided '*buf'.
    /// If size of the LOB is greater than 'buf_size', then
    /// if reallocation is allowed -- '*buf' will be reallocated
    /// to fit the BLOB size; otherwise it throws an exception.
    /// @param buf pointer on buffer pointer
    /// @param buf_size buffer size
    /// @param allow_realloc when "eReallocAllowed" Berkeley DB reallocates
    /// the buffer to allow successful fetch
    EBDB_ErrCode Fetch(void**       buf,
                       size_t       buf_size,
                       EReallocMode allow_realloc);

    /// Get LOB size. Becomes available right after successfull Fetch.
    size_t LobSize() const;

    /// Get LOB size. Becomes available right after successfull Fetch.
    size_t BlobSize() const { return LobSize(); }

    /// Read BLOB into vector.
    /// If BLOB does not fit, method resizes the vector to accomodate.
    ///
    EBDB_ErrCode ReadRealloc(TBuffer& buffer);

    /// Copy LOB data into the 'buf'.
    /// Throw an exception if buffer size 'size' is less than LOB size.
    ///
    /// @param buf destination data buffer
    /// @param size data buffer size
    /// @sa LobSize
    EBDB_ErrCode GetData(void* buf, size_t size);

    /// Creates stream like object to retrieve or write BLOB by chunks.
    /// Caller is responsible for deletion.
    CBDB_BLobStream* CreateStream();

    /// Creates stream like object to retrieve or write BLOB by chunks.
    /// Caller is responsible for deletion.
    CBDB_BlobReaderWriter* CreateReaderWriter();

    /// Creates stream like object to read BLOB by chunks.
    /// Return NULL if not found
    IReader* CreateReader();

private:
    /// forbidden
    CBDB_BLobFile(const CBDB_BLobFile&);
    CBDB_BLobFile& operator=(const CBDB_BLobFile&);
};

/// Variant of BLOB storage for integer key database
///
class NCBI_BDB_EXPORT CBDB_IdBlobFile : public CBDB_BLobFile
{
public:
    CBDB_FieldUint4        id;  ///< ID key
    Uint4 GetUid() const;
public:
    CBDB_IdBlobFile(EDuplicateKeys dup_keys = eDuplicatesDisable,
                    EDBType        db_type  = eBtree);

    /// Hash for this type returns id (key);
    virtual void SetHash(DB*);
};

/// Stream style BDB BLOB reader
///
/// Class wraps partial data access functionality of Berkeley DB.
/// All I/O directly calls  corresponding Berkeley DB methods
/// without any buffering. For better performance it's advised to read or
/// write data in chunks of substantial size.
///
class NCBI_BDB_EXPORT CBDB_BlobReaderWriter : public IReaderWriter
{
protected:
    CBDB_BlobReaderWriter(DB* db, DBT* dbt_key, size_t blob_size, DB_TXN* txn);
public:
    ~CBDB_BlobReaderWriter();

    /// Set current transaction
    void SetTransaction(CBDB_Transaction* trans);


    virtual ERW_Result Read(void*   buf,
                            size_t  count,
                            size_t* bytes_read);
    virtual ERW_Result PendingCount(size_t* count);

    virtual ERW_Result Write(const void* buf,
                             size_t      count,
                             size_t*     bytes_written);

    virtual ERW_Result Flush(void);

private:
    CBDB_BlobReaderWriter(const CBDB_BlobReaderWriter&);
    CBDB_BlobReaderWriter& operator=(const CBDB_BlobReaderWriter&);
    friend class CBDB_BLobFile;
private:
    DB*        m_DB;
    DBT*       m_DBT_Key;
    DBT*       m_DBT_Data;
    DB_TXN*    m_Txn;
    unsigned   m_Pos;
    size_t     m_BlobSize;
};


class CBDB_BlobWriter;

/// Berkeley DB BLOB File stream.
///
/// Class wraps partial data read/write functionality of Berkeley DB.
/// Both Read and Write functions of the class directly call
/// corresponding Berkeley DB methods without any buffering. For performance
/// reasons it's advised to read or write data in chunks of substantial size.

class NCBI_BDB_EXPORT CBDB_BLobStream
{
protected:
    CBDB_BLobStream(DB* db, DBT* dbt_key, size_t blob_size, DB_TXN* txn);
public:

    ~CBDB_BLobStream();

    /// Set current transaction for BLOB stream
    void SetTransaction(CBDB_Transaction* trans);

    /// Read data from BLOB
    void Read(void *buf, size_t buf_size, size_t *bytes_read);
    /// Write data into BLOB
    void Write(const void* buf, size_t buf_size);

    /// Return how much bytes we can read from the blob
    size_t PendingCount() const { return m_BlobSize > 0 ? (m_BlobSize - m_Pos) : 0; }

private:
    CBDB_BLobStream(const CBDB_BLobStream&);
    CBDB_BLobStream& operator=(const CBDB_BLobStream&);
private:
    DB*        m_DB;
    DBT*       m_DBT_Key;
    DBT*       m_DBT_Data;
    DB_TXN*    m_Txn;
    unsigned   m_Pos;
    size_t     m_BlobSize;
private:
    friend class CBDB_BLobFile;
};



/// Berkeley DB Large Object File class.
/// Implements simple BLOB storage based on single unsigned integer key

class NCBI_BDB_EXPORT CBDB_LobFile : public CBDB_RawFile
{
public:
    CBDB_LobFile();

    /// Insert BLOB data into the database, does nothing if key exists
    ///
    /// @param lob_id insertion key
    /// @param data buffer pointer
    /// @param size data size in bytes
    EBDB_ErrCode Insert(unsigned int lob_id, const void* data, size_t size);

    /// Insert or Update BLOB data into the database
    ///
    /// @param lob_id insertion key
    /// @param data buffer pointer
    /// @param size data size in bytes
    EBDB_ErrCode InsertUpdate(unsigned int lob_id, const void* data, size_t size);

    /// Fetch LOB record. On success, LOB size becomes available
    /// (see LobSize()), and the value can be obtained using GetData().
    ///
    /// <pre>
    /// Typical usage for this function is:
    /// 1. Call Fetch()
    /// 2. Allocate LobSize() chunk of memory
    /// 3. Use GetData() to retrive lob value
    /// </pre>
    EBDB_ErrCode Fetch(unsigned int lob_id);

    /// Fetch LOB record directly into the provided '*buf'.
    /// If size of the LOB is greater than 'buf_size', then
    /// if reallocation is allowed -- '*buf' will be reallocated
    /// to fit the LOB size; otherwise, a exception will be thrown.
    EBDB_ErrCode Fetch(unsigned int lob_id,
                       void**       buf,
                       size_t       buf_size,
                       EReallocMode allow_realloc);

    /// Get LOB size. Becomes available right after successfull Fetch.
    size_t LobSize() const;

    /// Copy LOB data into the 'buf'.
    /// Throw an exception if buffer size 'size' is less than LOB size.
    EBDB_ErrCode GetData(void* buf, size_t size);

    unsigned int GetKey() const { return m_LobKey; }

    /// Comparison function for unsigned int key
    virtual void SetCmp(DB*);

private:
    EBDB_ErrCode x_Put(unsigned int lob_id, const void* data, size_t size, bool can_update);

    unsigned int  m_LobKey;
};


/* @} */


/////////////////////////////////////////////////////////////////////////////
//  IMPLEMENTATION of INLINE functions
/////////////////////////////////////////////////////////////////////////////



/////////////////////////////////////////////////////////////////////////////
//  CBDB_LobFile::
//


inline EBDB_ErrCode CBDB_LobFile::Fetch(unsigned int lob_id)
{
    return Fetch(lob_id, 0, 0, eReallocForbidden);
}

/////////////////////////////////////////////////////////////////////////////
//  CBDB_BLobFile::
//

inline EBDB_ErrCode CBDB_BLobFile::Insert(const TBuffer& buf)
{
    return Insert(&buf[0], buf.size());
}

inline EBDB_ErrCode CBDB_BLobFile::UpdateInsert(const TBuffer& buf)
{
    return UpdateInsert(&buf[0], buf.size());
}


END_NCBI_SCOPE



#endif
