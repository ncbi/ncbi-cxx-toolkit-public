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

#include <bdb/bdb_file.hpp>


BEGIN_NCBI_SCOPE

/** @addtogroup BDB_BLOB
 *
 * @{
 */


class CBDB_BLobStream;

/// Berkeley DB BLOB File class. 
///
/// The basic BLOB file. Key part of the file consists of one or more 
/// fields. Data part is one binary object(BLOB).

class NCBI_BDB_EXPORT CBDB_BLobFile : public CBDB_File
{
public:

    CBDB_BLobFile();

    /// Insert BLOB into the database
    ///
    /// Before calling this function you should assign the key fields.
    /// @param data BLOB data
    /// @param size data size in bytes
    EBDB_ErrCode Insert(const void* data, size_t size);

    /// Fetche the record corresponding to the current key value.
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

};

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


END_NCBI_SCOPE



/*
 * ===========================================================================
 * $Log$
 * Revision 1.15  2004/07/14 20:00:30  rotmistr
 * InsertUpdate added
 *
 * Revision 1.14  2003/12/29 17:06:04  kuznets
 * +CBDB_BlobStream::SetTransaction()
 *
 * Revision 1.13  2003/12/29 16:51:21  kuznets
 * Added transaction support for BLOB stream
 *
 * Revision 1.12  2003/10/24 13:39:59  kuznets
 * + PendingCount for BlobStream
 *
 * Revision 1.11  2003/09/29 16:43:40  kuznets
 * Returned back CBDB_LobFile::SetCmp function
 * (was removed before out of good intentions)
 *
 * Revision 1.10  2003/09/26 20:54:37  kuznets
 * Documentaion change
 *
 * Revision 1.9  2003/09/26 18:48:05  kuznets
 * Doxigenification of comments
 *
 * Revision 1.8  2003/09/17 18:17:15  kuznets
 * +CBDB_BLobStream class
 *
 * Revision 1.7  2003/07/02 17:53:59  kuznets
 * Eliminated direct dependency from <db.h>
 *
 * Revision 1.6  2003/06/27 18:57:16  dicuccio
 * Uninlined strerror() adaptor.  Changed to use #include<> instead of #include ""
 *
 * Revision 1.5  2003/06/10 20:07:27  kuznets
 * Fixed header files not to repeat information from the README file
 *
 * Revision 1.4  2003/06/03 18:50:09  kuznets
 * Added dll export/import specifications
 *
 * Revision 1.3  2003/05/05 20:14:41  kuznets
 * Added CBDB_BLobFile, CBDB_File changed to support more flexible data record
 * management.
 *
 * Revision 1.2  2003/04/29 16:48:31  kuznets
 * Fixed minor warnings in Sun Workshop compiler
 *
 * Revision 1.1  2003/04/24 16:31:16  kuznets
 * Initial revision
 *
 * ===========================================================================
 */

#endif
