#ifndef DBAPI___BLOBCACHE__HPP
#define DBAPI___BLOBCACHE__HPP

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
 * Author: Anatoliy Kuznetsov
 *
 * File Description:  DBAPI based ICache interface
 *
 */

/// @file bdb_blobcache.hpp
/// ICache interface implementation on top of Berkeley DB

#include <corelib/ncbiexpt.hpp>
#include <util/cache/icache.hpp>
#include <dbapi/dbapi.hpp>

BEGIN_NCBI_SCOPE

/** @addtogroup DbAPI
 *
 * @{
 */


/// DBAPI ICache exception 

class NCBI_DBAPI_EXPORT CDBAPI_ICacheException : public CException
{
public:
    enum EErrCode {
        eInvalidDirectory,
        eStreamClosed,
        eCannotCreateBLOB,
        eCannotReadBLOB,
        eTempFileIOError
    };

    virtual const char* GetErrCodeString(void) const
    {
        switch (GetErrCode())
        {
        case eInvalidDirectory:  return "eInvalidDirectory";
        case eStreamClosed:      return "eStreamClosed";
        case eCannotCreateBLOB:  return "eCannotCreateBLOB";
        case eCannotReadBLOB:    return "eCannotReadBLOB";
        case eTempFileIOError:   return "eTempFileIOError";
        default:                 return  CException::GetErrCodeString();
        }
    }

    NCBI_EXCEPTION_DEFAULT(CDBAPI_ICacheException, CException);
};






/// DBAPI cache implementation.
///
/// Class implements ICache (BLOB cache) interface using DBAPI (RDBMS)
/// This implementation stores BLOBs in the RDBMS and intended to play
/// role of an enterprise wide BLOB storage
///
/// (tested with MSSQL)

class NCBI_DBAPI_EXPORT CDBAPI_Cache : public ICache
{
public:
    CDBAPI_Cache();
    virtual ~CDBAPI_Cache();

    /// Open cache. Does not take IConnection owbership.
    /// Connection should be already logged into the destination server
    /// 
    /// Implementation uses temporary files to keep local BLOBs.
    /// If you temp directory is not specified system default is used
    ///
    /// @param conn
    ///    Established database connection
    /// @param temp_dir
    ///    Directory name to keep temp files 
    ///    (auto created if does not exist)
    /// @param temp_prefix
    ///    Temp file prefix
    void Open(IConnection*  conn,
              const string& temp_dir = kEmptyStr,
              const string& temp_prefix = kEmptyStr);

    IConnection* GetConnection() { return m_Conn; }
    
    /// @return Size of the intermidiate BLOB memory buffer
    unsigned GetMemBufferSize() const { return m_MemBufferSize; }
    
    /// Set size of the intermidiate BLOB memory buffer
    void SetMemBufferSize(unsigned int buf_size);
    

    // ICache interface 

    virtual void SetTimeStampPolicy(TTimeStampFlags policy, int timeout);
    virtual TTimeStampFlags GetTimeStampPolicy() const;
    virtual int GetTimeout() const;
    virtual void SetVersionRetention(EKeepVersions policy);
    virtual EKeepVersions GetVersionRetention() const;
    virtual void Store(const string&  key,
                       int            version,
                       const string&  subkey,
                       const void*    data,
                       size_t         size);
    virtual size_t GetSize(const string&  key,
                           int            version,
                           const string&  subkey);

    virtual bool Read(const string& key, 
                      int           version, 
                      const string& subkey,
                      void*         buf, 
                      size_t        buf_size);

    virtual IReader* GetReadStream(const string&  key, 
                                   int            version,
                                   const string&  subkey);

    /// Specifics of this IWriter implementation is that IWriter::Flush here
    /// cannot be called twice, because it finalises transaction
    /// Also you cannot call Write after Flush...
    /// All this is because MSSQL (and Sybase) wants to know exact 
    /// BLOB size before writing it to the database
    /// Effectively IWriter::Flush in this case works as "Close"...
    ///
    virtual IWriter* GetWriteStream(const string&    key,
                                    int              version,
                                    const string&    subkey);

    virtual void Remove(const string& key);

    virtual void Remove(const string&    key,
                        int              version,
                        const string&    subkey);

    virtual time_t GetAccessTime(const string&  key,
                                 int            version,
                                 const string&  subkey);

    virtual void Purge(time_t           access_timeout,
                       EKeepVersions    keep_last_version = eDropAll);

    virtual void Purge(const string&    key,
                       const string&    subkey,
                       time_t           access_timeout,
                       EKeepVersions    keep_last_version = eDropAll);
private:

    /// Update BLOB storage, return TRUE if it was updated,
    /// FALSE if there is no record to update
    /// If data == 0 or size == 0 method adds an empty BLOB
    /// (or updates to make it empty)
    bool x_UpdateBlob(IStatement&    stmt,
                      const string&  key,
                      int            version,
                      const string&  subkey,
                      const void*    data,
                      size_t         size);

    /// Update access time attributes for the BLOB
    static
    void x_UpdateAccessTime(IStatement&    stmt,
                            const string&  key,
                            int            version,
                            const string&  subkey,
                            int            timestamp_flag);

    /// Return FALSE if timestamp cannot be retrived
    bool x_RetrieveTimeStamp(IStatement&    stmt,
                             const string&  key,
                             int            version,
                             const string&  subkey,
                             int&           timestamp);

    bool x_CheckTimestampExpired(int timestamp) const;

    void x_TruncateDB();

    /// Delete all BLOBs without corresponding attribute records
    void x_CleanOrphantBlobs(IStatement&    stmt);

private:
    CDBAPI_Cache(const CDBAPI_Cache&);
    CDBAPI_Cache& operator=(const CDBAPI_Cache&);

public:
    static
    void UpdateAccessTime(IStatement&    stmt,
                          const string&  key,
                          int            version,
                          const string&  subkey,
                          int            timestamp_flag)
    {
        x_UpdateAccessTime(stmt, key, version, subkey, timestamp_flag);
    }

private:
    IConnection*            m_Conn;         ///< db connection
    TTimeStampFlags         m_TimeStampFlag;///< Time stamp flag
    int                     m_Timeout;      ///< Timeout expiration policy
    EKeepVersions           m_VersionFlag;  ///< Version retention policy
    string                  m_TempDir;      ///< Directory for temp files
    string                  m_TempPrefix;   ///< Temp prefix
    unsigned int            m_MemBufferSize;///< Size of temp. buffer for BLOBs
};

/* @} */


END_NCBI_SCOPE

/*
 * ===========================================================================
 * $Log$
 * Revision 1.3  2004/07/21 19:04:33  kuznets
 * Added functions to change the size of the memory buffer
 *
 * Revision 1.2  2004/07/19 16:11:51  kuznets
 * + Remove for key,version,subkey
 *
 * Revision 1.1  2004/07/19 14:57:58  kuznets
 * Initial revision
 *
 *
 * ===========================================================================
 */

#endif
