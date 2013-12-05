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

#include <ncbi_pch.hpp>
#include <dbapi/cache/dbapi_blob_cache.hpp>
#include <dbapi/error_codes.hpp>

#include <corelib/ncbistr.hpp>
#include <corelib/ncbimtx.hpp>
#include <corelib/ncbitime.hpp>
#include <corelib/ncbifile.hpp>


#define NCBI_USE_ERRCODE_X   Dbapi_ICache


BEGIN_NCBI_SCOPE

static const unsigned int kWriterBufferSize = 1024 * 1024;


/// Add BLOB key specific where condition
static
void s_MakeKeyCondition(const string&  key,
                        int            version,
                        const string&  subkey,
                        string*        out_str)
{
    *out_str += " cache_key = '";
    *out_str += key;
    *out_str += "'";
    *out_str += " AND ";
    *out_str += " version = ";
    *out_str += NStr::IntToString(version);
    *out_str += " AND ";
    *out_str += " subkey = '";
    *out_str += subkey;
    *out_str += "'";
}

/// Add BLOB VALUES(...) statement
static
void s_MakeValueList(const string&  key,
                     int            version,
                     const string&  subkey,
                     string*        out_str)
{
    *out_str += "'"; *out_str += key; *out_str += "'";
    *out_str += ", ";
    *out_str += NStr::IntToString(version);
    *out_str += ", ";
    *out_str += "'"; *out_str += subkey; *out_str += "'";
}


// Mutex to sync cache requests coming from different threads
// All requests are protected with one mutex
DEFINE_STATIC_FAST_MUTEX(x_DBAPI_BLOB_CacheMutex);

/// Guard class, closes cursor on destruction
/// @internal
class CDBAPI_CursorGuard
{
public:
    CDBAPI_CursorGuard(ICursor* cur) : m_Cur(cur) {}
    ~CDBAPI_CursorGuard()
    {
        try {
            if (m_Cur)
                m_Cur->Close();
        } catch(...) {
        }
    }

    void Reset(ICursor* cur)
    {
        Close();
        m_Cur = cur;
    }

    void Close()
    {
        if (m_Cur) {
            ICursor* cur = m_Cur;
            m_Cur = 0;
            cur->Close();
        }
    }

private:
    CDBAPI_CursorGuard(const CDBAPI_CursorGuard&);
    CDBAPI_CursorGuard& operator=(const CDBAPI_CursorGuard&);

private:
    ICursor*   m_Cur;
};

/// Transaction Guard class
/// @internal
class CDBAPI_TransGuard
{
public:
    CDBAPI_TransGuard(IStatement* stmt)
     : m_Stmt(stmt)
    {
        m_Stmt->ExecuteUpdate("BEGIN TRANSACTION");
    }
    CDBAPI_TransGuard(const CDBAPI_TransGuard& tg)
    {
        Commit();
        m_Stmt = tg.m_Stmt;
        tg.Forget();
    }

    CDBAPI_TransGuard& operator=(const CDBAPI_TransGuard& tg)
    {
        m_Stmt = tg.m_Stmt;
        tg.Forget();
        return *this;
    }

    ~CDBAPI_TransGuard()
    {
        if (m_Stmt) {
            try {
                m_Stmt->ExecuteUpdate("ROLLBACK TRANSACTION");
            }
            catch (...) {} // ignore all troubles
        }
    }

    void Forget() const { m_Stmt = 0; }

    void Commit()
    {
        if (m_Stmt) {
            m_Stmt->ExecuteUpdate("COMMIT TRANSACTION");
            m_Stmt = 0;
        }
    }
private:
    mutable IStatement*   m_Stmt;
};



/// @internal
class CDBAPI_CacheIReader : public IReader
{
public:

    CDBAPI_CacheIReader(IConnection*             conn,
                        const string&            key,
                        int                      version,
                        const string&            subkey,
                        unsigned                 buf_size = kWriterBufferSize)
    : m_GoodStateFlag(true),
      m_Conn(conn),
      m_Key(key),
      m_Version(version),
      m_SubKey(subkey),
      m_Buffer(0),
      m_BytesInBuffer(0),
      m_BlobSize(0),
      m_ReadPos(0),
      m_MemBufferSize(buf_size)
    {
        string sel_blob_sql =
            "SELECT datalength(\"data\"), data FROM dbo.cache_data WHERE ";
        s_MakeKeyCondition(key, version, subkey, &sel_blob_sql);

        ICursor* cur = m_Conn->GetCursor("sel_cur", sel_blob_sql, 1);
        CDBAPI_CursorGuard cg(cur);
        IResultSet *rs = cur->Open();

        while (rs->Next()) {
            const CVariant& v = rs->GetVariant(1);
            if (!v.IsNull()) {
                m_BlobSize = v.GetInt4();
            } else {
                NCBI_THROW(CDBAPI_ICacheException,
                           eCannotReadBLOB,
                           "BLOB data is NULL for query " + sel_blob_sql);
            }
            if (m_BlobSize) {
                if (m_BlobSize <= m_MemBufferSize) {
                    m_Buffer = new unsigned char[m_BlobSize];
                    m_BytesInBuffer = (unsigned)rs->Read(m_Buffer, m_BlobSize);
                } else { // use temp file instead
                    m_TmpFile.reset(
                        CFile::CreateTmpFileEx(m_TempDir, m_TempPrefix));
                    for (unsigned i = 0; i < m_BlobSize;) {
                        char buf[1024];
                        size_t br = rs->Read(buf, sizeof(buf));
                        m_TmpFile->write(buf, br);
                        if (m_TmpFile->bad()) {
                            NCBI_THROW(CDBAPI_ICacheException,
                                    eTempFileIOError,
                                    "Temp file write error");
                        }
                        i += br;
                        _ASSERT(br);
                    } // for
                    m_TmpFile->seekg(0, IOS_BASE::beg);
                }
                m_ReadPos = 0;
            }
        } // while

    }


    ~CDBAPI_CacheIReader()
    {
        delete m_Buffer;
    }

    void SetTemps(const string& temp_dir, const string temp_prefix)
    {
        m_TempDir = temp_dir;
        m_TempPrefix = temp_prefix;
    }

    virtual ERW_Result Read(void*   buf,
                            size_t  count,
                            size_t* bytes_read)
    {
        CFastMutexGuard guard(x_DBAPI_BLOB_CacheMutex);

        if (m_Buffer) {
            _ASSERT(m_BlobSize >= m_ReadPos);

            size_t bytes_left = m_BlobSize - m_ReadPos;
            if (!bytes_left) {
                *bytes_read = 0;
                return eRW_Eof;
            }
            *bytes_read = min(count, bytes_left);
            ::memcpy(buf, m_Buffer + m_ReadPos, *bytes_read);
        } else {
            if (&*m_TmpFile) {
                m_TmpFile->read((char*)buf, count);
                *bytes_read = (size_t)m_TmpFile->gcount();
                if (*bytes_read == 0) {
                    return eRW_Eof;
                }
            }
        }
        m_ReadPos += *bytes_read;

        return eRW_Success;
    }

    virtual ERW_Result PendingCount(size_t* count)
    {
        *count = m_BlobSize - m_ReadPos;
        return eRW_Success;
    }

private:
    auto_ptr<fstream>     m_TmpFile;
    string                m_TempDir;
    string                m_TempPrefix;
    bool                  m_GoodStateFlag; //!< Stream is in the good state

    IConnection*          m_Conn;

    string                m_Key;
    int                   m_Version;
    string                m_SubKey;

    unsigned char*        m_Buffer;
    unsigned int          m_BytesInBuffer;
    unsigned int          m_BlobSize;
    unsigned int          m_ReadPos;
    unsigned int          m_MemBufferSize;
};


/// @internal
class CDBAPI_CacheIWriter : public IWriter
{
public:
    CDBAPI_CacheIWriter(CDBAPI_Cache*            cache,
                      const string&            key,
                      int                      version,
                      const string&            subkey,
                      unsigned int             buffer_size = kWriterBufferSize)
    : m_Cache(cache),
      m_TmpFile(0),
      m_GoodStateFlag(true),
      m_Flushed(false),
      m_Conn(cache->GetConnection()),
      m_Key(key),
      m_Version(version),
      m_SubKey(subkey),
      m_BytesInBuffer(0),
      m_MemBufferSize(buffer_size)
    {
        m_Buffer = new unsigned char[m_MemBufferSize];
    }

    ~CDBAPI_CacheIWriter()
    {
        CFastMutexGuard guard(x_DBAPI_BLOB_CacheMutex);

        try {
            if (!m_Flushed) {
                x_Flush();
            }
        } catch(exception& ex) {
            LOG_POST_X(1, ex.what());
        }

        delete [] m_Buffer;
    }

    void SetTemps(const string& temp_dir, const string temp_prefix)
    {
        m_TempDir = temp_dir;
        m_TempPrefix = temp_prefix;
    }

    virtual ERW_Result Write(const void* buf,
                             size_t      count,
                             size_t*     bytes_written = 0)
    {
        if (!m_GoodStateFlag)
            return eRW_Error;

        if (m_Flushed) {
            if (bytes_written)
                *bytes_written = 0;
            NCBI_THROW(CDBAPI_ICacheException, eStreamClosed,
                "Cannot call IWriter::Write after Flush");
        }

        unsigned int new_buf_length = m_BytesInBuffer + count;

        if (m_Buffer) {
            // Filling the buffer while we can
            if (new_buf_length <= m_MemBufferSize) {
                ::memcpy(m_Buffer + m_BytesInBuffer, buf, count);
                m_BytesInBuffer = new_buf_length;
                *bytes_written = count;
                return eRW_Success;
            } else {  // Buffer overflow. Writing to tmp file.
                _ASSERT(m_TmpFile.get() == 0);
                 m_TmpFile.reset(
                     CFile::CreateTmpFileEx(m_TempDir, m_TempPrefix));

                 if (m_BytesInBuffer) { // save the remains
                     m_TmpFile->write((char*)m_Buffer, m_BytesInBuffer);
                 }
                delete[] m_Buffer; m_Buffer = 0; m_BytesInBuffer = 0;
            }
        }

        if (&*m_TmpFile) {
            m_TmpFile->write((char*)buf, count);
            if ( m_TmpFile->good() ) {
                *bytes_written = count;
                return eRW_Success;
            }
        }
        m_GoodStateFlag = false;
        return eRW_Error;
    }

    virtual ERW_Result Flush(void)
    {
        if (m_Flushed) {
            return eRW_Success;
        }
        return x_Flush();
    }
private:

    ERW_Result x_SaveBlob(ostream& out)
    {
        if (m_Buffer) {
            _ASSERT(m_TmpFile.get() == 0);
            out.write((char*)m_Buffer, m_BytesInBuffer);
        }

        if (&*m_TmpFile) {
            _ASSERT(m_Buffer == 0);

            m_TmpFile->seekg(0, IOS_BASE::beg);
            char buf[1024];
            while (true) {
                m_TmpFile->read(buf, sizeof(buf));
                size_t bytes_read = (size_t)m_TmpFile->gcount();
                if (bytes_read == 0)
                    break;
                out.write(buf, bytes_read);
                if (out.bad()) {
                    m_GoodStateFlag = false;
                    return eRW_Error;
                }
            }
        }
        out.flush();
        return eRW_Success;
    }

    ERW_Result x_SaveBlob(ICursor& cur)
    {
        ERW_Result ret = eRW_Error;
        if (m_Buffer) {
            ostream& out = cur.GetBlobOStream(1, m_BytesInBuffer);
            ret = x_SaveBlob(out);
        }
        if (&*m_TmpFile) {
            CT_OFF_TYPE total_bytes = m_TmpFile->tellg() - CT_POS_TYPE(0);
            ostream& out = cur.GetBlobOStream(1, (size_t)total_bytes);
            ret = x_SaveBlob(out);
        }
        return ret;
    }

    ERW_Result x_Flush(void)
    {
        if (m_Flushed) {
            NCBI_THROW(CDBAPI_ICacheException, eStreamClosed,
                "Cannot call IWriter::Write after Flush");
        }
        if (!m_GoodStateFlag)
            return eRW_Error;

        m_Flushed = true;

        CFastMutexGuard guard(x_DBAPI_BLOB_CacheMutex);

        IStatement* stmt = m_Conn->GetStatement();
        CDBAPI_TransGuard tg(stmt);

        //
        // Create an update cursor here, build an output stream on it
        //
        string upd_blob_sql =
            "SELECT data FROM dbo.cache_data WHERE ";
        s_MakeKeyCondition(m_Key, m_Version, m_SubKey, &upd_blob_sql);
        upd_blob_sql += " FOR UPDATE";

        ICursor* cur(m_Conn->GetCursor("wrt_upd_cur", upd_blob_sql, 1));
        CDBAPI_CursorGuard cg(cur);
        IResultSet *rs = cur->Open();

        while (rs->Next()) {
            ERW_Result ret = x_SaveBlob(*cur);

            cg.Close();
            CDBAPI_Cache::UpdateAccessTime(
                            *stmt,
                            m_Key,
                            m_Version,
                            m_SubKey,
                            m_Cache->GetTimeStampPolicy());

            if (ret == eRW_Success) {
                tg.Commit();
            }
            return ret;
        } // while
        cg.Close();

        string ins_blob_sql =
            "INSERT INTO dbo.cache_data (cache_key, version, subkey, data) "
            "VALUES( ";
        {  // BLOB does not exist, INSERT required
            s_MakeValueList(m_Key, m_Version, m_SubKey, &ins_blob_sql);
            // it should be NULL here but it gives an error with FTDS :(
            // if porting to normal RDBMS it may need attention
            ins_blob_sql += ", ' ')";
//            ins_blob_sql += ", NULL)";
            stmt->ExecuteUpdate(ins_blob_sql);

            cg.Reset(cur = m_Conn->GetCursor("wrt_upd_cur", upd_blob_sql, 1));
            rs = cur->Open();
            while (rs->Next()) {
                ERW_Result ret = x_SaveBlob(*cur);

                cg.Close();
                CDBAPI_Cache::UpdateAccessTime(
                                *stmt,
                                m_Key,
                                m_Version,
                                m_SubKey,
                                m_Cache->GetTimeStampPolicy());

                if (ret == eRW_Success) {
                    tg.Commit();
                }
                return ret;
            } // while
        }

        NCBI_THROW(CDBAPI_ICacheException, eCannotCreateBLOB,
                   "BLOB INSERT failed.  Failed query: " + ins_blob_sql);
        return eRW_Success;
    }

private:
    CDBAPI_Cache*         m_Cache;
    auto_ptr<fstream>     m_TmpFile;
    string                m_TempDir;
    string                m_TempPrefix;
    bool                  m_GoodStateFlag; //!< Stream is in the good state
    bool                  m_Flushed;       //!< Flush been called flag

    IConnection*          m_Conn;

    string                m_Key;
    int                   m_Version;
    string                m_SubKey;

    unsigned char*        m_Buffer;
    unsigned int          m_BytesInBuffer;
    unsigned int          m_MemBufferSize;
};




///////////////////////////////////////////////////////////////////////////////
const char* CDBAPI_ICacheException::GetErrCodeString(void) const
{
    switch (GetErrCode())
    {
    case eCannotInitCache:   return "eCannotInitCache";
    case eConnectionError:   return "eConnectionError";
    case eInvalidDirectory:  return "eInvalidDirectory";
    case eStreamClosed:      return "eStreamClosed";
    case eCannotCreateBLOB:  return "eCannotCreateBLOB";
    case eCannotReadBLOB:    return "eCannotReadBLOB";
    case eTempFileIOError:   return "eTempFileIOError";
    case eNotImplemented:    return "eNotImplemented";
    default:                 return  CException::GetErrCodeString();
    }
}

///////////////////////////////////////////////////////////////////////////////
CDBAPI_Cache::CDBAPI_Cache()
: m_Conn(0),
  m_OwnConnection(false),
  m_Timeout(0),
  m_MaxTimeout(0),
  m_MemBufferSize(kWriterBufferSize)
{
}

CDBAPI_Cache::~CDBAPI_Cache()
{
    if (m_Conn && m_OwnConnection) {
        delete m_Conn;
    }
}

void CDBAPI_Cache::SetMemBufferSize(unsigned int buf_size)
{
    const unsigned min_buffer = 256 * 1024;
    m_MemBufferSize = (buf_size < min_buffer) ? min_buffer : buf_size;
}

void CDBAPI_Cache::Open(IConnection* conn,
                        const string& temp_dir,
                        const string& temp_prefix)
{
    m_Conn = conn;
    m_OwnConnection = false;
    m_TempDir = temp_dir;
    m_TempPrefix = temp_prefix;

    if (!m_TempDir.empty()) {
        CDir  tmp_dir(m_TempDir);

        if (!tmp_dir.Exists()) {
            if (!tmp_dir.Create()) {
                NCBI_THROW(CDBAPI_ICacheException, eInvalidDirectory,
                        "Cannot create directory:" + m_TempDir);
            }
        }
    }

}

void CDBAPI_Cache::Open(const string& driver,
                        const string& server,
                        const string& database,
                        const string& login,
                        const string& password,
                        const string& temp_dir,
                        const string& temp_prefix)
{
    CDriverManager &db_drv_man = CDriverManager::GetInstance();
    IDataSource* ds = db_drv_man.CreateDs(driver);
    auto_ptr<IConnection> conn(ds->CreateConnection());  // TODO: Add ownership flag
    if (conn.get() == 0) {
        NCBI_THROW(CDBAPI_ICacheException, eConnectionError,
                "Cannot create connection");
    }
    conn->Connect(login, password, server, database);

    Open(conn.get(), temp_dir, temp_prefix);

    m_Conn = conn.release();
    m_OwnConnection = true;
}


ICache::TFlags CDBAPI_Cache::GetFlags()
{
    return (TFlags) 0;
}

void CDBAPI_Cache::SetFlags(ICache::TFlags flags)
{
}

void CDBAPI_Cache::SetTimeStampPolicy(TTimeStampFlags policy,
                                      unsigned int    timeout,
                                      unsigned int    max_timeout)
{
    CFastMutexGuard guard(x_DBAPI_BLOB_CacheMutex);
    m_TimeStampFlag = policy;
    m_Timeout = timeout;
    if (max_timeout) {
        m_MaxTimeout = max_timeout > timeout ? max_timeout : timeout;
    } else {
        m_MaxTimeout = 0;
    }
}

CDBAPI_Cache::TTimeStampFlags CDBAPI_Cache::GetTimeStampPolicy() const
{
    return m_TimeStampFlag;
}

int CDBAPI_Cache::GetTimeout() const
{
    return m_Timeout;
}

void CDBAPI_Cache::SetVersionRetention(EKeepVersions policy)
{
    CFastMutexGuard guard(x_DBAPI_BLOB_CacheMutex);
    m_VersionFlag = policy;
}

CDBAPI_Cache::EKeepVersions CDBAPI_Cache::GetVersionRetention() const
{
    return m_VersionFlag;
}

void CDBAPI_Cache::Store(const string&  key,
                         int            version,
                         const string&  subkey,
                         const void*    data,
                         size_t         size,
                         unsigned int   /*time_to_live*/,
                         const string&  /*owner = kEmptyStr*/)
{
    if (m_VersionFlag == eDropAll || m_VersionFlag == eDropOlder) {
        Purge(key, subkey, 0, m_VersionFlag);
    }

    CFastMutexGuard guard(x_DBAPI_BLOB_CacheMutex);

    IStatement* stmt = m_Conn->GetStatement();
    CDBAPI_TransGuard tg(stmt);

    bool any_rec = x_UpdateBlob(*stmt, key, version, subkey, data, size);

    if (!any_rec) { // BLOB not found. insert.
        string ins_blob_sql =
            "INSERT INTO dbo.cache_data (cache_key, version, subkey, data) "
            "VALUES( ";
        s_MakeValueList(key, version, subkey, &ins_blob_sql);
        // it should be NULL here but it gives an error with FTDS :(
        // if porting to normal RDBMS it may need attention
        ins_blob_sql += ", ' ')";


        stmt->ExecuteUpdate(ins_blob_sql);
        x_UpdateBlob(*stmt, key, version, subkey, data, size);

    }
    x_UpdateAccessTime(*stmt, key, version, subkey, m_TimeStampFlag);

    tg.Commit();
}



size_t CDBAPI_Cache::GetSize(const string&  key,
                             int            version,
                             const string&  subkey)
{
    CFastMutexGuard guard(x_DBAPI_BLOB_CacheMutex);

    int timestamp;
    IStatement* stmt = m_Conn->GetStatement();

    bool attr_exists =
        x_RetrieveTimeStamp(*stmt, key, version, subkey, timestamp);

    if (!attr_exists) {
        return 0;
    }
    // check expiration here
    if (m_TimeStampFlag & fCheckExpirationAlways) {
        if (x_CheckTimestampExpired(timestamp)) {
            return 0;
        }
    }
    // get blob information
    string sel_blob_sql = "SELECT datalength(\"data\") FROM dbo.cache_data WHERE ";
    s_MakeKeyCondition(key, version, subkey, &sel_blob_sql);

    ICursor* cur=m_Conn->GetCursor("sel_cur", sel_blob_sql, 1);
    CDBAPI_CursorGuard cg(cur);
    IResultSet *rs = cur->Open();

    size_t blob_size = 0;
    while (rs->Next()) {
        const CVariant& v = rs->GetVariant(1);
        if (v.IsNull()) {
            blob_size = 0;
        } else {
            blob_size = v.GetInt4();
        }
    }
    return blob_size;
}

bool CDBAPI_Cache::HasBlobs(const string&  key,
                            const string&  subkey)
{
    _ASSERT(0); // TODO: implement this!
    return false;
}


bool CDBAPI_Cache::Read(const string& key,
                        int           version,
                        const string& subkey,
                        void*         buf,
                        size_t        buf_size)
{
    CFastMutexGuard guard(x_DBAPI_BLOB_CacheMutex);

    int timestamp;
    IStatement* stmt=m_Conn->GetStatement();

    bool attr_exists =
        x_RetrieveTimeStamp(*stmt, key, version, subkey, timestamp);

    if (!attr_exists) {
        return 0;
    }
    // check expiration here
    if (m_TimeStampFlag & fCheckExpirationAlways) {
        if (x_CheckTimestampExpired(timestamp)) {
            return 0;
        }
    }
    // get blob information
    string sel_blob_sql =
        "SELECT datalength(\"data\"), data FROM dbo.cache_data WHERE ";
    s_MakeKeyCondition(key, version, subkey, &sel_blob_sql);

    ICursor* cur = m_Conn->GetCursor("sel_cur", sel_blob_sql, 1);
    CDBAPI_CursorGuard cg(cur);
    IResultSet *rs = cur->Open();

    while (rs->Next()) {
        const CVariant& v = rs->GetVariant(1);
        size_t blob_size = 0;
        if (!v.IsNull()) {
            blob_size = v.GetInt4();
        }
        if (blob_size) {
            blob_size = min(blob_size, buf_size);
            istream& in = rs->GetBlobIStream();
            in.read((char*)buf, blob_size);

            if ( m_TimeStampFlag & fTimeStampOnRead ) {
                CDBAPI_TransGuard tg(stmt);
                x_UpdateAccessTime(*stmt,
                                   key, version, subkey,
                                   m_TimeStampFlag);
                tg.Commit();
            }
            return true;
        }
    }

    return false;
}

IReader* CDBAPI_Cache::GetReadStream(const string&  key,
                                     int            version,
                                     const string&  subkey)
{
    CFastMutexGuard guard(x_DBAPI_BLOB_CacheMutex);

    int timestamp;
    IStatement* stmt = m_Conn->GetStatement();

    bool attr_exists =
        x_RetrieveTimeStamp(*stmt, key, version, subkey, timestamp);

    if (!attr_exists) {
        return 0;
    }
    // check expiration here
    if (m_TimeStampFlag & fCheckExpirationAlways) {
        if (x_CheckTimestampExpired(timestamp)) {
            return 0;
        }
    }

    auto_ptr<CDBAPI_CacheIReader> rdr;
    try {
        rdr.reset(new CDBAPI_CacheIReader(m_Conn,
                                          key, version, subkey,
                                          GetMemBufferSize()));
    } catch (CDBAPI_ICacheException&) {
        return 0;
    }

    if ( m_TimeStampFlag & fTimeStampOnRead ) {
        CDBAPI_TransGuard tg(stmt);
        x_UpdateAccessTime(*stmt, key, version, subkey, m_TimeStampFlag);
        tg.Commit();
    }

    return rdr.release();
}


IReader* CDBAPI_Cache::GetReadStream(const string&  /* key */,
                                     const string&  /* subkey */,
                                     int*           /* version */,
                                     ICache::EBlobVersionValidity* /* validity */)
{
    NCBI_THROW(CDBAPI_ICacheException, eNotImplemented,
        "CDBAPI_Cache::GetReadStream(key, subkey, &version, &validity) "
        "is not implemented");
}


void CDBAPI_Cache::SetBlobVersionAsCurrent(const string&  /* key */,
                                         const string&  /* subkey */,
                                         int            /* version */)
{
    NCBI_THROW(CDBAPI_ICacheException, eNotImplemented,
        "CDBAPI_Cache::SetBlobVersionAsCurrent(key, subkey, version) "
        "is not implemented");
}


void CDBAPI_Cache::GetBlobAccess(const string&     /* key */,
                                 int               /* version */,
                                 const string&     /* subkey */,
                                 SBlobAccessDescr*  blob_descr)
{
    _ASSERT(0); // Not implemented yet
    blob_descr->blob_size = 0;
}



IWriter* CDBAPI_Cache::GetWriteStream(const string&    key,
                                      int              version,
                                      const string&    subkey,
                                      unsigned int    /*time_to_live*/,
                                      const string&   /*owner*/)
{
    if (m_VersionFlag == eDropAll || m_VersionFlag == eDropOlder) {
        Purge(key, subkey, 0, m_VersionFlag);
    }

    auto_ptr<CDBAPI_CacheIWriter> wrt(
        new CDBAPI_CacheIWriter(this, key, version, subkey, GetMemBufferSize()));
    wrt->SetTemps(m_TempDir, m_TempPrefix);

    return wrt.release();
}

void CDBAPI_Cache::Remove(const string& key)
{
    CFastMutexGuard guard(x_DBAPI_BLOB_CacheMutex);

    string del_blob_sql =
        "DELETE FROM dbo.cache_data WHERE cache_key = '";
    del_blob_sql += key;
    del_blob_sql += "'";

    IStatement* stmt = m_Conn->GetStatement();
    CDBAPI_TransGuard tg(stmt);

    stmt->ExecuteUpdate(del_blob_sql);

    del_blob_sql =
        "DELETE FROM dbo.cache_attr WHERE cache_key = '";
    del_blob_sql += key;
    del_blob_sql += "'";

    stmt->ExecuteUpdate(del_blob_sql);

    tg.Commit();
}

void CDBAPI_Cache::Remove(const string&    key,
                          int              version,
                          const string&    subkey)
{
    CFastMutexGuard guard(x_DBAPI_BLOB_CacheMutex);

    string del_blob_sql =
        "DELETE FROM dbo.cache_data WHERE ";
    s_MakeKeyCondition(key, version, subkey, &del_blob_sql);
    IStatement* stmt = m_Conn->GetStatement();
    CDBAPI_TransGuard tg(stmt);

    stmt->ExecuteUpdate(del_blob_sql);

    del_blob_sql =
        "DELETE FROM dbo.cache_attr WHERE ";
    s_MakeKeyCondition(key, version, subkey, &del_blob_sql);

    stmt->ExecuteUpdate(del_blob_sql);

    tg.Commit();
}


time_t CDBAPI_Cache::GetAccessTime(const string&  key,
                                   int            version,
                                   const string&  subkey)
{
    CFastMutexGuard guard(x_DBAPI_BLOB_CacheMutex);

    IStatement* stmt = m_Conn->GetStatement();
    int timestamp;
    bool rec_exsits =
        x_RetrieveTimeStamp(*stmt, key, version, subkey, timestamp);

    if (rec_exsits)
        return (time_t)timestamp;
    return 0;
}

void CDBAPI_Cache::Purge(time_t           access_timeout,
                         EKeepVersions    keep_last_version)
{
    CFastMutexGuard guard(x_DBAPI_BLOB_CacheMutex);

    if (keep_last_version == eDropAll && access_timeout == 0) {
        x_TruncateDB();
        return;
    }
    CTime time_stamp(CTime::eCurrent);
    time_t curr = time_stamp.GetTimeT();
    int timeout = GetTimeout();
    curr -= timeout;

    IStatement* stmt = m_Conn->GetStatement();
    {{
    CDBAPI_TransGuard tg(stmt);

    string del_sql =
        "DELETE FROM dbo.cache_attr WHERE ";
    del_sql += " cache_timestamp < ";
    del_sql += NStr::NumericToString(curr);

    stmt->ExecuteUpdate(del_sql);

    tg.Commit();
    }}

    // Now we have all wrong attributes deleted (and commited)
    // we iterate all dangling BLOBs and drop them one by one
    // in separate transactions
    // The rationel is to give RDBMS a chance to clean the
    // transaction log after each BLOB.

    x_CleanOrphantBlobs(*stmt);

}

void CDBAPI_Cache::Purge(const string&    key,
                         const string&    subkey,
                         time_t           access_timeout,
                         EKeepVersions    keep_last_version)
{
    if (key.empty() && subkey.empty()) {
        Purge(access_timeout, keep_last_version);
        return;
    }

    CFastMutexGuard guard(x_DBAPI_BLOB_CacheMutex);

    if (key.empty() ||
        (keep_last_version == eDropAll && access_timeout == 0)) {
        x_TruncateDB();
        return;
    }

    CTime time_stamp(CTime::eCurrent);
    time_t curr = time_stamp.GetTimeT();
    int timeout = GetTimeout();
    curr -= timeout;

    IStatement* stmt = m_Conn->GetStatement();
    {{
    CDBAPI_TransGuard tg(stmt);

    string del_sql =
        "DELETE FROM dbo.cache_attr WHERE ";
    del_sql += " cache_timestamp < ";
    del_sql += NStr::NumericToString(curr);
    if (!key.empty()) {
        del_sql += " AND cache_key = '";
        del_sql += key;
        del_sql += "'";
    }

    if (!subkey.empty()) {
        del_sql += " AND subkey = '";
        del_sql += subkey;
        del_sql += "'";
    }

    stmt->ExecuteUpdate(del_sql);

    tg.Commit();
    }}

    x_CleanOrphantBlobs(*stmt);

}

/// @internal
struct SDBAPI_CacheDescr
{
    string    key;
    int       version;
    string    subkey;

    SDBAPI_CacheDescr(string x_key,
                      int    x_version,
                      string x_subkey)
    : key(x_key), version(x_version), subkey(x_subkey)
    {}

    SDBAPI_CacheDescr() {}
};

void CDBAPI_Cache::x_CleanOrphantBlobs(IStatement& stmt)
{
    string sel_sql =
        "SELECT cd.cache_key, cd.version, cd.subkey "
        "FROM cache_data cd "
        "LEFT OUTER JOIN cache_attr ca "
        "ON (cd.cache_key = ca.cache_key AND "
            "cd.version = ca.version AND "
            "cd.subkey = ca.subkey) "
        "WHERE "
            "ca.cache_key IS NULL AND "
            "ca.version   IS NULL AND "
            "ca.cache_key IS NULL";

    vector<SDBAPI_CacheDescr> blist;

    // MSSQL cannot execute two statements at the same time
    // (connection is busy), here we fetch the BLOB list first
    // and then delete all in a separate transaction

    {{
    ICursor* cur = m_Conn->GetCursor("sel_cur", sel_sql, 1);
    CDBAPI_CursorGuard cg(cur);
    IResultSet *rs = cur->Open();

    string key, subkey;

    while (rs->Next()) {
        const CVariant& v1 = rs->GetVariant(1);
        key = v1.IsNull()? kEmptyStr : v1.GetString();

        const CVariant& v2 = rs->GetVariant(2);
        int version = v2.IsNull()? 0 : v2.GetInt4();

        const CVariant& v3 = rs->GetVariant(3);
        subkey = v3.IsNull()? kEmptyStr : v3.GetString();

        blist.push_back(SDBAPI_CacheDescr(key, version, subkey));
    } // while
    }}

    string del_sql = "DELETE FROM dbo.cache_data WHERE ";
    ITERATE(vector<SDBAPI_CacheDescr>, it, blist) {
        {{
            CDBAPI_TransGuard tg(&stmt);
            string del_cond;
            s_MakeKeyCondition(it->key, it->version, it->subkey, &del_cond);

            string sql = del_sql + del_cond;
            stmt.ExecuteUpdate(sql);

            tg.Commit();
        }}
    }
}


void CDBAPI_Cache::x_TruncateDB()
{
    IStatement* stmt = m_Conn->GetStatement();

    // First delete cache attributes, then delete the cache BLOB database

    {{
    CDBAPI_TransGuard tg(stmt);
    stmt->ExecuteUpdate("DELETE FROM dbo.cache_attr");
    tg.Commit();
    }}

    {{
    CDBAPI_TransGuard tg(stmt);
    stmt->ExecuteUpdate("DELETE FROM dbo.cache_blob");
    tg.Commit();
    }}
}

bool CDBAPI_Cache::x_CheckTimestampExpired(int timestamp) const
{
    int timeout = GetTimeout();
    if (timeout) {
        CTime time_stamp(CTime::eCurrent);
        time_t curr = (int)time_stamp.GetTimeT();
        if (curr - timeout > timestamp) {
            return true;
        }
    }
    return false;
}


bool CDBAPI_Cache::x_RetrieveTimeStamp(IStatement&   /* stmt */,
                                      const string&  key,
                                      int            version,
                                      const string&  subkey,
                                      int&           timestamp)
{
    bool any_rec = false;
    string subk = (m_TimeStampFlag & fTrackSubKey) ? subkey : kEmptyStr;

    string sel_blob_sql = "SELECT cache_timestamp FROM dbo.cache_attr WHERE ";
    s_MakeKeyCondition(key, version, subk, &sel_blob_sql);
    ICursor* cur = m_Conn->GetCursor("attr_cur", sel_blob_sql, 1);
    CDBAPI_CursorGuard cg(cur);
    IResultSet *rs = cur->Open();

    while (rs->Next()) {
        const CVariant& v = rs->GetVariant(1);
        timestamp = v.GetInt4();
        any_rec = true;
        break;
    }
    return any_rec;
}


bool CDBAPI_Cache::x_UpdateBlob(IStatement&    stmt,
                                const string&  key,
                                int            version,
                                const string&  subkey,
                                const void*    data,
                                size_t         size)
{
    bool any_rec = false;

    // Request to create an empty BLOB
    if (size == 0 || data == 0) {
        string upd_sql =
            "UPDATE dbo.cache_data SET data = NULL WHERE ";
        s_MakeKeyCondition(key, version, subkey, &upd_sql);
        stmt.ExecuteUpdate(upd_sql);
        int rows = stmt.GetRowCount();
        if (rows <= 0) {
            string ins_blob_sql =
                "INSERT INTO dbo.cache_data (cache_key, version, subkey, data) "
                "VALUES( ";
            s_MakeValueList(key, version, subkey, &ins_blob_sql);
            // it should be NULL here but it gives an error with FTDS :(
            // if porting to normal RDBMS it may need attention
            ins_blob_sql += ", ' ')";
//            ins_blob_sql += ", NULL)";
            stmt.ExecuteUpdate(ins_blob_sql);
            // rows = stmt.GetRowCount();
        }
        return true;
    }

    string upd_blob_sql = "SELECT \"data\" FROM dbo.cache_data WHERE ";
    s_MakeKeyCondition(key, version, subkey, &upd_blob_sql);
    upd_blob_sql += " FOR UPDATE";

    ICursor* cur(m_Conn->GetCursor("upd_cur", upd_blob_sql, 1));
    CDBAPI_CursorGuard cg(cur);
    IResultSet *rs = cur->Open();

    while (rs->Next()) {
        ostream& out = cur->GetBlobOStream(1, size);
        out.write((const char*)data, size);
        out.flush();
        any_rec = true;
        break;
    }

    return any_rec;
}

void CDBAPI_Cache::x_UpdateAccessTime(IStatement&    stmt,
                                      const string&  key,
                                      int            version,
                                      const string&  subkey,
                                      int            timestamp_flag)
{
    CTime time_stamp(CTime::eCurrent);
    int stamp = (int)time_stamp.GetTimeT();
    string str_stamp = NStr::IntToString(stamp);

    string upd_attr_sql = "UPDATE dbo.cache_attr SET cache_timestamp = ";
    upd_attr_sql += str_stamp;
    upd_attr_sql += " WHERE";

    string subk = (timestamp_flag & fTrackSubKey) ? subkey : kEmptyStr;
    string key_cond;
    s_MakeKeyCondition(key, version, subk, &key_cond);
    upd_attr_sql += key_cond;

    stmt.ExecuteUpdate(upd_attr_sql);
    int rows = stmt.GetRowCount();

    if (rows <= 0) {  // No attribute record...
        string ins_attr_sql =
            "INSERT INTO dbo.cache_attr (cache_key, version, subkey, cache_timestamp) "
            "VALUES( ";
        s_MakeValueList(key, version, subk, &ins_attr_sql);
        ins_attr_sql += ", ";
        ins_attr_sql += str_stamp;
        ins_attr_sql += ")";
        stmt.ExecuteUpdate(ins_attr_sql);
    }
}


bool CDBAPI_Cache::SameCacheParams(const TCacheParams* params) const
{
    // Never share dbapi cache
    return false;
}

void CDBAPI_Cache::GetBlobOwner(const string&  key,
                                int            version,
                                const string&  subkey,
                                string*        owner)
{
    _ASSERT(owner);
    owner->erase(); // not supported in this implementation
}

END_NCBI_SCOPE
