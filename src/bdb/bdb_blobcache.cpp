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
 * File Description:  BDB libarary BLOB cache implementation.
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbitime.hpp>
#include <corelib/ncbifile.hpp>
#include <corelib/ncbi_process.hpp>
#include <corelib/plugin_manager_impl.hpp>


#include <bdb/bdb_blobcache.hpp>
#include <bdb/bdb_cursor.hpp>
#include <bdb/bdb_trans.hpp>
#include <corelib/ncbimtx.hpp>

#include <corelib/ncbitime.hpp>


BEGIN_NCBI_SCOPE

// Mutex to sync cache requests coming from different threads
// All requests are protected with one mutex
DEFINE_STATIC_FAST_MUTEX(x_BDB_BLOB_CacheMutex);

// Mutex to sync int cache requests coming from different threads
// All requests are protected with one mutex
/*
DEFINE_STATIC_FAST_MUTEX(x_BDB_IntCacheMutex);
*/

static const unsigned int s_WriterBufferSize = 256 * 1024;

/*
static void s_MakeOverflowFileName(string& buf,
                                   const string& path, 
                                   const string& blob_key,
                                   int           version)
{
    buf = path + blob_key + '_' + NStr::IntToString(version) + ".ov_";
}
*/
		
static void s_MakeOverflowFileName(string& buf,
                                   const string& path, 
                                   const string& key,
                                   int           version,
                                   const string& subkey)
{
    buf = path + key + '_'
               + NStr::IntToString(version) + '_' + subkey + ".ov_";
}


struct SCacheDescr
{
    string    key;
    int       version;
    string    subkey;
    int       overflow;

    SCacheDescr(string x_key,
                int    x_version,
                string x_subkey,
                int    x_overflow)
    : key(x_key),
      version(x_version),
      subkey(x_subkey),
      overflow(x_overflow)
    {}

    SCacheDescr() {}
};


class CBDB_CacheIReader : public IReader
{
public:

    CBDB_CacheIReader(CBDB_BLobStream* blob_stream)
    : m_BlobStream(blob_stream),
      m_OverflowFile(0),
      m_Buffer(0)
    {}

    CBDB_CacheIReader(CNcbiIfstream* overflow_file)
    : m_BlobStream(0),
      m_OverflowFile(overflow_file),
      m_Buffer(0)
    {}

    CBDB_CacheIReader(unsigned char* buf, size_t buf_size)
    : m_BlobStream(0),
      m_OverflowFile(0),
      m_Buffer(buf),
      m_BufferPtr(buf),
      m_BufferSize(buf_size)
    {
    }

    virtual ~CBDB_CacheIReader()
    {
        delete m_BlobStream;
        delete m_OverflowFile;
        delete[] m_Buffer;
    }


    virtual ERW_Result Read(void*   buf, 
                            size_t  count,
                            size_t* bytes_read)
    {
        if (count == 0)
            return eRW_Success;

        // Check if BLOB is memory based...
        if (m_Buffer) {
            if (m_BufferSize == 0) {
                *bytes_read = 0;
                return eRW_Eof;
            }
            *bytes_read = min(count, m_BufferSize);
            ::memcpy(buf, m_BufferPtr, *bytes_read);
            m_BufferPtr += *bytes_read;
            m_BufferSize -= *bytes_read;
            return eRW_Success;
        }

        // Check if BLOB is file based...
        if (m_OverflowFile) {
            CFastMutexGuard guard(x_BDB_BLOB_CacheMutex);

            m_OverflowFile->read((char*)buf, count);
            *bytes_read = m_OverflowFile->gcount();
            if (*bytes_read == 0) {
                return eRW_Eof;
            }
            return eRW_Success;
        }
        
        // Reading from the BDB stream

        size_t br;

        {{

        CFastMutexGuard guard(x_BDB_BLOB_CacheMutex);
        m_BlobStream->Read(buf, count, &br);

        }}

        if (bytes_read)
            *bytes_read = br;
        
        if (br == 0) 
            return eRW_Eof;
        return eRW_Success;
    }

    virtual ERW_Result PendingCount(size_t* count)
    {
        if ( m_Buffer ) {
            *count = m_BufferSize;
            return eRW_Success;
        }
        else if ( m_OverflowFile ) {
            *count = m_OverflowFile->good()? 1: 0;
            return eRW_Success;
        }
        else if (m_BlobStream) {
            *count = m_BlobStream->PendingCount();
            return eRW_Success;
        }

        *count = 0;
        return eRW_Error;
    }


private:
    CBDB_CacheIReader(const CBDB_CacheIReader&);
    CBDB_CacheIReader& operator=(const CBDB_CacheIReader&);

private:
    CBDB_BLobStream* m_BlobStream;
    CNcbiIfstream*   m_OverflowFile;
    unsigned char*   m_Buffer;
    unsigned char*   m_BufferPtr;
    size_t           m_BufferSize;
};



class CBDB_CacheIWriter : public IWriter
{
public:
    CBDB_CacheIWriter(const char*         path,
                      const string&       blob_key,
                      int                 version,
                      const string&       subkey,
                      CBDB_BLobStream*    blob_stream,
                      SCacheDB&           blob_db,
                      SCache_AttrDB&      attr_db,
                      int                 stamp_subkey)
    : m_Path(path),
      m_BlobKey(blob_key),
      m_Version(version),
      m_SubKey(subkey),
      m_BlobStream(blob_stream),
      m_BlobDB(blob_db),
      m_AttrDB(attr_db),
      m_Buffer(0),
      m_BytesInBuffer(0),
      m_OverflowFile(0),
      m_StampSubKey(stamp_subkey)
    {
        m_Buffer = new unsigned char[s_WriterBufferSize];
    }

    virtual ~CBDB_CacheIWriter()
    {
        // Dumping the buffer
        CFastMutexGuard guard(x_BDB_BLOB_CacheMutex);

        CBDB_Transaction trans(*m_AttrDB.GetEnv());
        m_AttrDB.SetTransaction(&trans);
        m_BlobDB.SetTransaction(&trans);

        if (m_Buffer) {
            _TRACE("LC: Dumping BDB BLOB size=" << m_BytesInBuffer);
            m_BlobStream->SetTransaction(&trans);
            m_BlobStream->Write(m_Buffer, m_BytesInBuffer);
            m_BlobDB.Sync();
            delete[] m_Buffer;
        }
        delete m_BlobStream;


        m_AttrDB.key = m_BlobKey.c_str();
        m_AttrDB.version = m_Version;
        m_AttrDB.subkey = m_StampSubKey ? m_SubKey : "";
        m_AttrDB.overflow = 0;

        CTime time_stamp(CTime::eCurrent);
        m_AttrDB.time_stamp = (unsigned)time_stamp.GetTimeT();

        if (m_OverflowFile) {
            m_AttrDB.overflow = 1;
            delete m_OverflowFile;
        }
        m_AttrDB.UpdateInsert();
        m_AttrDB.Sync();

        trans.Commit();
        m_AttrDB.GetEnv()->TransactionCheckpoint();
    }

    virtual ERW_Result Write(const void* buf, size_t count,
                             size_t* bytes_written = 0)
    {
        *bytes_written = 0;
        if (count == 0)
            return eRW_Success;

        CFastMutexGuard guard(x_BDB_BLOB_CacheMutex);

        unsigned int new_buf_length = m_BytesInBuffer + count;

        if (m_Buffer) {
            // Filling the buffer while we can
            if (new_buf_length <= s_WriterBufferSize) {
                ::memcpy(m_Buffer + m_BytesInBuffer, buf, count);
                m_BytesInBuffer = new_buf_length;
                *bytes_written = count;
                return eRW_Success;
            } else {
                // Buffer overflow. Writing to file.
                OpenOverflowFile();
                if (m_OverflowFile) {
                    if (m_BytesInBuffer) {
                        m_OverflowFile->write((char*)m_Buffer, 
                                              m_BytesInBuffer);
                    }
                    delete[] m_Buffer;
                    m_Buffer = 0;
                    m_BytesInBuffer = 0;
                }
            }
        }

        if (m_OverflowFile) {
            m_OverflowFile->write((char*)buf, count);
            if ( m_OverflowFile->good() ) {
                *bytes_written = count;
                return eRW_Success;
            }
        }

        return eRW_Error;
    }

    /// Flush pending data (if any) down to output device.
    virtual ERW_Result Flush(void)
    {
        // Dumping the buffer
        CFastMutexGuard guard(x_BDB_BLOB_CacheMutex);

        CBDB_Transaction trans(*m_AttrDB.GetEnv());
        m_AttrDB.SetTransaction(&trans);
        m_BlobDB.SetTransaction(&trans);

        if (m_Buffer) {
            _TRACE("LC: Dumping BDB BLOB size=" << m_BytesInBuffer);
            m_BlobStream->SetTransaction(&trans);
            m_BlobStream->Write(m_Buffer, m_BytesInBuffer);
            delete[] m_Buffer;
            m_Buffer = 0;
            m_BytesInBuffer = 0;
        }
        m_BlobDB.Sync();
        if ( m_OverflowFile ) {
            m_OverflowFile->flush();
            if ( m_OverflowFile->bad() ) {
                return eRW_Error;
            }
        }

        trans.Commit();
        m_AttrDB.GetEnv()->TransactionCheckpoint();

        return eRW_Success;
    }
private:
    void OpenOverflowFile()
    {
        string path;
        s_MakeOverflowFileName(path, m_Path, m_BlobKey, m_Version, m_SubKey);
        _TRACE("LC: Making overflow file " << path);
        m_OverflowFile =
            new CNcbiOfstream(path.c_str(), 
                              IOS_BASE::out | 
                              IOS_BASE::trunc | 
                              IOS_BASE::binary);
        if (!m_OverflowFile->is_open()) {
            ERR_POST("LC Error:Cannot create overflow file " << path);
            delete m_OverflowFile;
            m_OverflowFile = 0;
        }
    }

private:
    CBDB_CacheIWriter(const CBDB_CacheIWriter&);
    CBDB_CacheIWriter& operator=(const CBDB_CacheIWriter&);

private:
    const char*           m_Path;
    string                m_BlobKey;
    int                   m_Version;
    string                m_SubKey;
    CBDB_BLobStream*      m_BlobStream;

    SCacheDB&             m_BlobDB;
    SCache_AttrDB&        m_AttrDB;

    unsigned char*        m_Buffer;
    unsigned int          m_BytesInBuffer;
    CNcbiOfstream*        m_OverflowFile;

    int                   m_StampSubKey;
};



CBDB_Cache::CBDB_Cache()
: m_PidGuard(0),
  m_Env(0),
  m_CacheDB(0),
  m_CacheAttrDB(0),
  m_VersionFlag(eDropOlder)
{
    m_TimeStampFlag = fTimeStampOnRead | 
                      fExpireLeastFrequentlyUsed |
                      fPurgeOnStartup;
}

CBDB_Cache::~CBDB_Cache()
{
    Close();
}

void CBDB_Cache::Open(const char* cache_path, 
                      const char* cache_name,
                      ELockMode lm, 
                      unsigned int cache_ram_size)
{
    {{
    
    CFastMutexGuard guard(x_BDB_BLOB_CacheMutex);

    Close();

    m_Path = CDirEntry::AddTrailingPathSeparator(cache_path);

    // Make sure our directory exists
    {{
        CDir dir(m_Path);
        if ( !dir.Exists() ) {
            dir.Create();
        }
    }}

    string lock_file = string("lcs_") + string(cache_name) + string(".pid");
    string lock_file_path = m_Path + lock_file;

    switch (lm)
    {
    case ePidLock:
        m_PidGuard = new CPIDGuard(lock_file_path, m_Path);
        break;
    case eNoLock:
        break;
    default:
        break;
    }

    m_Env = new CBDB_Env();

    // Check if bdb env. files are in place and try to join
    CDir dir(m_Path);
    CDir::TEntries fl = dir.GetEntries("__db.*", CDir::eIgnoreRecursive);
    if (fl.empty()) {
        if (cache_ram_size) {
            m_Env->SetCacheSize(cache_ram_size);
        }
        m_Env->OpenWithTrans(cache_path);
    } else {
        if (cache_ram_size) {
            m_Env->SetCacheSize(cache_ram_size);
        }

        try {
            m_Env->JoinEnv(cache_path, CBDB_Env::eThreaded);
            if (!m_Env->IsTransactional()) {
                LOG_POST(Warning << 
                         "LC: Warning: Joined non-transactional environment ");
            }
        } 
        catch (CBDB_Exception&)
        {
            m_Env->OpenWithTrans(cache_path);
        }
    }

    m_Env->SetDirectDB(true);
    m_Env->SetDirectLog(true);

    m_CacheDB = new SCacheDB();
    m_CacheAttrDB = new SCache_AttrDB();

    m_CacheDB->SetEnv(*m_Env);
    m_CacheAttrDB->SetEnv(*m_Env);

    m_CacheDB->SetPageSize(32 * 1024);

    string cache_db_name = 
       string("lcs_") + string(cache_name) + string(".db");
    string attr_db_name = 
       string("lcs_") + string(cache_name) + string("_attr") + string(".db");

    m_CacheDB->Open(cache_db_name.c_str(),    CBDB_RawFile::eReadWriteCreate);
    m_CacheAttrDB->Open(attr_db_name.c_str(), CBDB_RawFile::eReadWriteCreate);
    
    }}

    if (m_TimeStampFlag & fPurgeOnStartup) {
        Purge(GetTimeout());
    }
    m_Env->TransactionCheckpoint();
}


void CBDB_Cache::Close()
{
    delete m_PidGuard;    m_PidGuard = 0;
    delete m_CacheDB;     m_CacheDB = 0;
    delete m_CacheAttrDB; m_CacheAttrDB = 0;
    delete m_Env;         m_Env = 0;
}


void CBDB_Cache::SetTimeStampPolicy(TTimeStampFlags policy, 
                                    int             timeout)
{
    CFastMutexGuard guard(x_BDB_BLOB_CacheMutex);
    m_TimeStampFlag = policy;
    m_Timeout = timeout;
}

CBDB_Cache::TTimeStampFlags CBDB_Cache::GetTimeStampPolicy() const
{
    return m_TimeStampFlag;
}

int CBDB_Cache::GetTimeout()
{
    return m_Timeout;
}

void CBDB_Cache::SetVersionRetention(EKeepVersions policy)
{
    CFastMutexGuard guard(x_BDB_BLOB_CacheMutex);
    m_VersionFlag = policy;
}

CBDB_Cache::EKeepVersions CBDB_Cache::GetVersionRetention() const
{
    return m_VersionFlag;
}

void CBDB_Cache::Store(const string&  key,
                       int            version,
                       const string&  subkey,
                       const void*    data,
                       size_t         size)
{
    if (m_VersionFlag == eDropAll || m_VersionFlag == eDropOlder) {
        Purge(key, subkey, 0, m_VersionFlag);
    }

    CBDB_Transaction trans(*m_Env);
    m_CacheDB->SetTransaction(&trans);
    m_CacheAttrDB->SetTransaction(&trans);


    CFastMutexGuard guard(x_BDB_BLOB_CacheMutex);

    unsigned overflow = 0;

    if (size < s_WriterBufferSize) {  // inline BLOB

        m_CacheDB->key = key;
        m_CacheDB->version = version;
        m_CacheDB->subkey = subkey;

        m_CacheDB->Insert(data, size);

        overflow = 0;

    } else { // overflow BLOB
        string path;
        s_MakeOverflowFileName(path, m_Path, key, version, subkey);
        _TRACE("LC: Making overflow file " << path);
        CNcbiOfstream oveflow_file(path.c_str(), 
                                   IOS_BASE::out | 
                                   IOS_BASE::trunc | 
                                   IOS_BASE::binary);
        if (!oveflow_file.is_open()) {
            ERR_POST("LC Error:Cannot create overflow file " << path);
            return;
        }
        oveflow_file.write((char*)data, size);
        overflow = 1;
    }

    //
    // Update cache element's attributes
    //

    CTime time_stamp(CTime::eCurrent);

    m_CacheAttrDB->key = key;
    m_CacheAttrDB->version = version;
    m_CacheAttrDB->subkey = (m_TimeStampFlag & fTrackSubKey) ? subkey : "";
    m_CacheAttrDB->time_stamp = (unsigned)time_stamp.GetTimeT();
    m_CacheAttrDB->overflow = overflow;

    m_CacheAttrDB->UpdateInsert();

    trans.Commit();
    m_CacheAttrDB->GetEnv()->TransactionCheckpoint();
}


size_t CBDB_Cache::GetSize(const string&  key,
                           int            version,
                           const string&  subkey)
{
    CFastMutexGuard guard(x_BDB_BLOB_CacheMutex);

    m_CacheAttrDB->key = key;
    m_CacheAttrDB->version = version;
    m_CacheAttrDB->subkey = (m_TimeStampFlag & fTrackSubKey) ? subkey : "";

    EBDB_ErrCode ret = m_CacheAttrDB->Fetch();
    if (ret != eBDB_Ok) {
        return 0;
    }

    // check expiration here
    if (m_TimeStampFlag & fCheckExpirationAlways) {
        if (x_CheckTimestampExpired()) {
            return 0;
        }
    }

    int overflow = m_CacheAttrDB->overflow;

    if (overflow) {
        string path;
        s_MakeOverflowFileName(path, m_Path, key, version, subkey);
        CFile entry(path);

        if (entry.Exists()) {
            return (size_t) entry.GetLength();
        }
    }

    // Regular inline BLOB

    m_CacheDB->key = key;
    m_CacheDB->version = version;
    m_CacheDB->subkey = subkey;

    ret = m_CacheDB->Fetch();

    if (ret != eBDB_Ok) {
        return 0;
    }
    return m_CacheDB->LobSize();
}


bool CBDB_Cache::Read(const string& key, 
                      int           version, 
                      const string& subkey,
                      void*         buf, 
                      size_t        buf_size)
{
    CFastMutexGuard guard(x_BDB_BLOB_CacheMutex);

    m_CacheAttrDB->key = key;
    m_CacheAttrDB->version = version;
    m_CacheAttrDB->subkey = (m_TimeStampFlag & fTrackSubKey) ? subkey : "";

    EBDB_ErrCode ret = m_CacheAttrDB->Fetch();
    if (ret != eBDB_Ok) {
        return false;
    }

    // check expiration
    if (m_TimeStampFlag & fCheckExpirationAlways) {
        if (x_CheckTimestampExpired()) {
            return false;
        }
    }

    int overflow = m_CacheAttrDB->overflow;

    if (overflow) {
        string path;
        s_MakeOverflowFileName(path, m_Path, key, version, subkey);

        auto_ptr<CNcbiIfstream>
            overflow_file(new CNcbiIfstream(path.c_str(),
                                            IOS_BASE::in | IOS_BASE::binary));
        if (!overflow_file->is_open()) {
            return false;
        }
        overflow_file->read((char*)buf, buf_size);
        if (!*overflow_file) {
            return false;
        }
    }
    else {
        m_CacheDB->key = key;
        m_CacheDB->version = version;
        m_CacheDB->subkey = subkey;

        ret = m_CacheDB->Fetch();
        if (ret != eBDB_Ok) {
            return false;
        }
        ret = m_CacheDB->GetData(buf, buf_size);
        if (ret != eBDB_Ok) {
            return false;
        }
    }

    if ( m_TimeStampFlag & fTimeStampOnRead ) {
        x_UpdateAccessTime(key, version, subkey);
    }
    return true;

}


IReader* CBDB_Cache::GetReadStream(const string&  key, 
                                   int            version,
                                   const string&  subkey)
{
    CFastMutexGuard guard(x_BDB_BLOB_CacheMutex);

    m_CacheAttrDB->key = key;
    m_CacheAttrDB->version = version;
    m_CacheAttrDB->subkey = (m_TimeStampFlag & fTrackSubKey) ? subkey : "";

    EBDB_ErrCode ret = m_CacheAttrDB->Fetch();
    if (ret != eBDB_Ok) {
        return 0;
    }

    // check expiration
    if (m_TimeStampFlag & fCheckExpirationAlways) {
        if (x_CheckTimestampExpired()) {
            return 0;
        }
    }

    int overflow = m_CacheAttrDB->overflow;

    // Check if it's an overflow BLOB (external file)

    if (overflow) {
        string path;
        s_MakeOverflowFileName(path, m_Path, key, version, subkey);
        auto_ptr<CNcbiIfstream> 
            overflow_file(new CNcbiIfstream(path.c_str(),
                                            IOS_BASE::in | IOS_BASE::binary));
        if (!overflow_file->is_open()) {
            return 0;
        }
        if ( m_TimeStampFlag & fTimeStampOnRead ) {
            x_UpdateAccessTime(key, version, subkey);
        }
        return new CBDB_CacheIReader(overflow_file.release());

    }

    // Inline BLOB, reading from BDB storage

    m_CacheDB->key = key;
    m_CacheDB->version = version;
    m_CacheDB->subkey = subkey;
    
    ret = m_CacheDB->Fetch();
    if (ret != eBDB_Ok) {
        return 0;
    }

    size_t bsize = m_CacheDB->LobSize();

    unsigned char* buf = new unsigned char[bsize+1];

    ret = m_CacheDB->GetData(buf, bsize);
    if (ret != eBDB_Ok) {
        return 0;
    }

    if ( m_TimeStampFlag & fTimeStampOnRead ) {
        x_UpdateAccessTime(key, version, subkey);
    }
    return new CBDB_CacheIReader(buf, bsize);

/*
    CBDB_BLobStream* bstream = m_BlobDB.CreateStream();
    return new CBDB_BLOB_CacheIReader(bstream);
*/

}



IWriter* CBDB_Cache::GetWriteStream(const string&    key,
                                    int              version,
                                    const string&    subkey)
{
    if (m_VersionFlag == eDropAll || m_VersionFlag == eDropOlder) {
        Purge(key, subkey, 0, m_VersionFlag);
    }

    CFastMutexGuard guard(x_BDB_BLOB_CacheMutex);

    {
        CBDB_Transaction trans(*m_Env);
        m_CacheDB->SetTransaction(&trans);
        m_CacheAttrDB->SetTransaction(&trans);

        x_DropBlob(key.c_str(), version, subkey.c_str(), 1);
        trans.Commit();
    }

    m_CacheDB->key = key;
    m_CacheDB->version = version;
    m_CacheDB->subkey = subkey;

    CBDB_BLobStream* bstream = m_CacheDB->CreateStream();
    return 
        new CBDB_CacheIWriter(m_Path.c_str(), 
                              key, 
                              version,
                              subkey,
                              bstream, 
                              *m_CacheDB,
                              *m_CacheAttrDB,
                              m_TimeStampFlag & fTrackSubKey);

}


void CBDB_Cache::Remove(const string& key)
{
    CFastMutexGuard guard(x_BDB_BLOB_CacheMutex);

    vector<SCacheDescr>  cache_elements;

    // Search the records to delete

    {{

    CBDB_FileCursor cur(*m_CacheAttrDB);
    cur.SetCondition(CBDB_FileCursor::eEQ);

    cur.From << key;
    while (cur.Fetch() == eBDB_Ok) {
        int version = m_CacheAttrDB->version;
        const char* subkey = m_CacheAttrDB->subkey;

        int overflow = m_CacheAttrDB->overflow;

        cache_elements.push_back(SCacheDescr(key, version, subkey, overflow));
    }

    }}

    CBDB_Transaction trans(*m_Env);
    m_CacheDB->SetTransaction(&trans);
    m_CacheAttrDB->SetTransaction(&trans);

    // Now delete all objects

    ITERATE(vector<SCacheDescr>, it, cache_elements) {
        x_DropBlob(it->key.c_str(), 
                   it->version, 
                   it->subkey.c_str(), 
                   it->overflow);
    }

	trans.Commit();

    // Second pass scan if for some resons some cache elements are 
    // still in the database

    cache_elements.resize(0);

    {{

    CBDB_FileCursor cur(*m_CacheDB);
    cur.SetCondition(CBDB_FileCursor::eEQ);

    cur.From << key;
    while (cur.Fetch() == eBDB_Ok) {
        int version = m_CacheDB->version;
        const char* subkey = m_CacheDB->subkey;

        cache_elements.push_back(SCacheDescr(key, version, subkey, 0));
    }

    }}

    ITERATE(vector<SCacheDescr>, it, cache_elements) {
        x_DropBlob(it->key.c_str(), 
                   it->version, 
                   it->subkey.c_str(), 
                   it->overflow);
    }

    trans.Commit();
    m_CacheAttrDB->GetEnv()->TransactionCheckpoint();

}


time_t CBDB_Cache::GetAccessTime(const string&  key,
                                 int            version,
                                 const string&  subkey)
{
    _ASSERT(m_CacheAttrDB);

    CFastMutexGuard guard(x_BDB_BLOB_CacheMutex);

    m_CacheAttrDB->key = key;
    m_CacheAttrDB->version = version;
    m_CacheAttrDB->subkey = (m_TimeStampFlag & fTrackSubKey) ? subkey : "";

    EBDB_ErrCode ret = m_CacheAttrDB->Fetch();
    if (ret != eBDB_Ok) {
        return 0;
    }
    
    return (int) m_CacheAttrDB->time_stamp;
}


void CBDB_Cache::Purge(time_t           access_timeout,
                       EKeepVersions    keep_last_version)
{
    CFastMutexGuard guard(x_BDB_BLOB_CacheMutex);

    if (keep_last_version == eDropAll && access_timeout == 0) {
        x_TruncateDB();
        return;
    }

    // Search the database for obsolete cache entries

    vector<SCacheDescr> cache_entries;

    {{
    CBDB_FileCursor cur(*m_CacheAttrDB);
    cur.SetCondition(CBDB_FileCursor::eFirst);


    CTime time_stamp(CTime::eCurrent);
    time_t curr = (int)time_stamp.GetTimeT();
    int timeout = GetTimeout();

    while (cur.Fetch() == eBDB_Ok) {
        time_t db_time_stamp = m_CacheAttrDB->time_stamp;
        int version = m_CacheAttrDB->version;
        const char* key = m_CacheAttrDB->key;
        int overflow = m_CacheAttrDB->overflow;
        const char* subkey = m_CacheAttrDB->subkey;

        if (curr - timeout > db_time_stamp) {
            cache_entries.push_back(
                            SCacheDescr(key, version, subkey, overflow));
        }

    } // while
    }}

    CBDB_Transaction trans(*m_Env);
    m_CacheDB->SetTransaction(&trans);
    m_CacheAttrDB->SetTransaction(&trans);

    ITERATE(vector<SCacheDescr>, it, cache_entries) {
        x_DropBlob(it->key.c_str(), 
                   it->version, 
                   it->subkey.c_str(), 
                   it->overflow);
    }

    trans.Commit();

}


void CBDB_Cache::Purge(const string&    key,
                       const string&    subkey,
                       time_t           access_timeout,
                       EKeepVersions    keep_last_version)
{
    CFastMutexGuard guard(x_BDB_BLOB_CacheMutex);

    if (key.empty() || 
        (keep_last_version == eDropAll && access_timeout == 0)) {
        x_TruncateDB();
        return;
    }

    // Search the database for obsolete cache entries
    vector<SCacheDescr> cache_entries;


    {{

    CBDB_FileCursor cur(*m_CacheAttrDB);
    cur.SetCondition(CBDB_FileCursor::eEQ);

    cur.From << key;

    CTime time_stamp(CTime::eCurrent);
    time_t curr = (int)time_stamp.GetTimeT();
    int timeout = GetTimeout();

    while (cur.Fetch() == eBDB_Ok) {
        time_t db_time_stamp = m_CacheAttrDB->time_stamp;
        int version = m_CacheAttrDB->version;
        const char* x_key = m_CacheAttrDB->key;
        int overflow = m_CacheAttrDB->overflow;
        string x_subkey = (const char*) m_CacheAttrDB->subkey;

        if (subkey.empty()) {
            
        }

        if ( (curr - timeout > db_time_stamp) ||
            (subkey.empty() || (subkey == x_subkey)) 
           ) {
            cache_entries.push_back(
                            SCacheDescr(x_key, version, x_subkey, overflow));
        }

    } // while

    }}

    CBDB_Transaction trans(*m_Env);
    m_CacheDB->SetTransaction(&trans);
    m_CacheAttrDB->SetTransaction(&trans);

    ITERATE(vector<SCacheDescr>, it, cache_entries) {
        x_DropBlob(it->key.c_str(), 
                   it->version, 
                   it->subkey.c_str(), 
                   it->overflow);
    }
    
    trans.Commit();

}


bool CBDB_Cache::x_CheckTimestampExpired()
{
    int timeout = GetTimeout();
    int db_time_stamp = m_CacheAttrDB->time_stamp;
    if (timeout) {
        CTime time_stamp(CTime::eCurrent);
        time_t curr = (int)time_stamp.GetTimeT();
        if (curr - timeout > db_time_stamp) {
            _TRACE("local cache item expired:" 
                   << db_time_stamp << " curr=" << curr 
                   << " diff=" << curr - db_time_stamp);
            return true;
        }
    }
    return false;
}


void CBDB_Cache::x_UpdateAccessTime(const string&  key,
                                    int            version,
                                    const string&  subkey)
{
    CBDB_Transaction trans(*m_Env);
    m_CacheDB->SetTransaction(&trans);
    m_CacheAttrDB->SetTransaction(&trans);

    m_CacheAttrDB->key = key;
    m_CacheAttrDB->version = version;
    m_CacheAttrDB->subkey = (m_TimeStampFlag & fTrackSubKey) ? subkey : "";

    EBDB_ErrCode ret =m_CacheAttrDB->Fetch();
    if (ret != eBDB_Ok) {
        m_CacheAttrDB->overflow = 0;
    }

    CTime time_stamp(CTime::eCurrent);
    m_CacheAttrDB->time_stamp = (unsigned)time_stamp.GetTimeT();

    m_CacheAttrDB->UpdateInsert();

    trans.Commit();
}


void CBDB_Cache::x_TruncateDB()
{
    LOG_POST(Info << "CBDB_BLOB_Cache:: cache database truncated");
    m_CacheDB->Truncate();
    m_CacheAttrDB->Truncate();

    // Scan the directory, delete overflow BLOBs
    // TODO: remove overflow files only matching cache specific name
    //       signatures. Since several caches may live in the same
    //       directory we may delete some "extra" files
    CDir dir(m_Path);
    string ext;
    string ov_("ov_");
    if (dir.Exists()) {
        CDir::TEntries  content(dir.GetEntries());
        ITERATE(CDir::TEntries, it, content) {
            if (!(*it)->IsFile()) {
                if (ext == ov_) {
                    (*it)->Remove();
                }
            }
        }
    }
}


void CBDB_Cache::x_DropBlob(const char*    key,
                            int            version,
                            const char*    subkey,
                            int            overflow)
{
    _ASSERT(key);
    _ASSERT(subkey);

    if (overflow == 1) {
        string path;
        s_MakeOverflowFileName(path, m_Path, key, version, subkey);

        CDirEntry entry(path);
        if (entry.Exists()) {
            entry.Remove();
        }
    }
    m_CacheDB->key = key;
    m_CacheDB->version = version;
    m_CacheDB->subkey = subkey;

    m_CacheDB->Delete(CBDB_RawFile::eIgnoreError);

    m_CacheAttrDB->key = key;
    m_CacheAttrDB->version = version;
    m_CacheAttrDB->subkey = subkey;

    m_CacheAttrDB->Delete(CBDB_RawFile::eIgnoreError);

}



const char* kBDBCacheDriverName = "bdbcache";

/// Class factory for BDB implementation of ICache
///
/// @internal
///
class CBDB_CacheReaderCF : 
    public CSimpleClassFactoryImpl<ICache, CBDB_Cache>
{
public:
    typedef 
      CSimpleClassFactoryImpl<ICache, CBDB_Cache> TParent;
public:
    CBDB_CacheReaderCF() : TParent(kBDBCacheDriverName, 0)
    {
    }
    ~CBDB_CacheReaderCF()
    {
    }
};

void NCBI_BDB_ICacheEntryPoint(
     CPluginManager<ICache>::TDriverInfoList&   info_list,
     CPluginManager<ICache>::EEntryPointRequest method)
{
    CHostEntryPointImpl<CBDB_CacheReaderCF>::
       NCBI_EntryPointImpl(info_list, method);
}


CBDB_CacheHolder::CBDB_CacheHolder(ICache* blob_cache, ICache* id_cache) 
: m_BlobCache(blob_cache),
  m_IdCache(id_cache)
{}

CBDB_CacheHolder::~CBDB_CacheHolder()
{
    delete m_BlobCache;
    delete m_IdCache;
}

END_NCBI_SCOPE

/*
 * ===========================================================================
 * $Log$
 * Revision 1.54  2004/05/25 18:43:51  kuznets
 * Fixed bug in setting cache RAM size, added additional protection when joining
 * existing environment.
 *
 * Revision 1.53  2004/05/24 18:03:03  kuznets
 * CBDB_Cache::Open added parameter to specify RAM cache size
 *
 * Revision 1.52  2004/05/17 20:55:11  gorelenk
 * Added include of PCH ncbi_pch.hpp
 *
 * Revision 1.51  2004/04/28 16:58:56  kuznets
 * Fixed deadlock in CBDB_Cache::Remove
 *
 * Revision 1.50  2004/04/28 12:21:32  kuznets
 * Cleaned up dead code
 *
 * Revision 1.49  2004/04/28 12:11:22  kuznets
 * Replaced static string with char* (fix crash on Linux)
 *
 * Revision 1.48  2004/04/27 19:12:01  kuznets
 * Commented old cache implementation
 *
 * Revision 1.47  2004/03/26 14:54:21  kuznets
 * Transaction checkpoint after database creation
 *
 * Revision 1.46  2004/03/26 14:05:39  kuznets
 * Force transaction checkpoints and turn-off buffering
 *
 * Revision 1.45  2004/03/24 13:51:03  friedman
 * Fixed mutex comments
 *
 * Revision 1.44  2004/03/23 19:22:08  friedman
 * Replaced 'static CFastMutex' with DEFINE_STATIC_FAST_MUTEX
 *
 * Revision 1.43  2004/02/27 17:29:50  kuznets
 * +CBDB_CacheHolder
 *
 * Revision 1.42  2004/02/06 16:16:40  vasilche
 * Fixed delete m_Buffer -> delete[] m_Buffer.
 *
 * Revision 1.41  2004/02/02 21:24:29  vasilche
 * Fixed buffering of overflow file streams - open file in constructor.
 * Fixed processing of timestamps when fTimeStampOnRead flag is not set.
 *
 * Revision 1.40  2004/01/29 20:31:06  vasilche
 * Removed debug messages.
 *
 * Revision 1.39  2004/01/07 18:58:10  vasilche
 * Make message about joining to non-transactional environment a warning.
 *
 * Revision 1.38  2003/12/30 16:29:12  kuznets
 * Fixed a bug in overflow file naming.
 *
 * Revision 1.37  2003/12/29 18:47:20  kuznets
 * Cache opening changed to use free threaded environment.
 *
 * Revision 1.36  2003/12/29 17:08:15  kuznets
 * CBDB_CacheIWriter - using transactions to save BDB data
 *
 * Revision 1.35  2003/12/29 16:53:25  kuznets
 * Made Flush transactional
 *
 * Revision 1.34  2003/12/29 15:39:59  vasilche
 * Fixed subkey value in GetReadStream/GetWriteStream.
 *
 * Revision 1.33  2003/12/29 12:57:15  kuznets
 * Changes in Purge() method to make cursor loop lock independent
 * from the record delete function
 *
 * Revision 1.32  2003/12/16 13:45:11  kuznets
 * ICache implementation made transaction protected
 *
 * Revision 1.31  2003/12/08 16:13:15  kuznets
 * Added plugin mananger support
 *
 * Revision 1.30  2003/11/28 17:35:05  vasilche
 * Fixed new[]/delete discrepancy.
 *
 * Revision 1.29  2003/11/26 13:09:16  kuznets
 * Fixed bug in mutex locking
 *
 * Revision 1.28  2003/11/25 19:36:35  kuznets
 * + ICache implementation
 *
 * Revision 1.27  2003/11/06 14:20:38  kuznets
 * Warnings cleaned up
 *
 * Revision 1.26  2003/10/24 13:54:03  vasilche
 * Rolled back incorrect fix of PendingCount().
 *
 * Revision 1.25  2003/10/24 13:41:23  kuznets
 * Completed PendingCount implementaion
 *
 * Revision 1.24  2003/10/24 12:37:42  kuznets
 * Implemented cache locking using PID guard
 *
 * Revision 1.23  2003/10/23 13:46:38  vasilche
 * Implemented PendingCount() method.
 *
 * Revision 1.22  2003/10/22 19:08:29  vasilche
 * Added Flush() implementation.
 *
 * Revision 1.21  2003/10/21 12:11:27  kuznets
 * Fixed non-updated timestamp in Int cache.
 *
 * Revision 1.20  2003/10/20 20:44:20  vasilche
 * Added return true for overflow file read.
 *
 * Revision 1.19  2003/10/20 20:41:37  kuznets
 * Fixed bug in BlobCache::Read
 *
 * Revision 1.18  2003/10/20 20:35:33  kuznets
 * Blob cache Purge improved.
 *
 * Revision 1.17  2003/10/20 20:34:03  kuznets
 * Fixed bug with writing BLOB overflow attribute
 *
 * Revision 1.16  2003/10/20 20:15:30  kuznets
 * Fixed bug with expiration time retrieval
 *
 * Revision 1.15  2003/10/20 19:58:26  kuznets
 * Fixed bug in int cache expiration algorithm
 *
 * Revision 1.14  2003/10/20 17:53:03  kuznets
 * Dismissed blob cache entry overwrite protection.
 *
 * Revision 1.13  2003/10/20 16:34:20  kuznets
 * BLOB cache Store operation reimplemented to use external files.
 * BDB cache shared between tables by using common environment.
 * Overflow file limit set to 1M (was 2M)
 *
 * Revision 1.12  2003/10/17 14:11:41  kuznets
 * Implemented cached read from berkeley db BLOBs
 *
 * Revision 1.11  2003/10/16 19:29:18  kuznets
 * Added Int cache (AKA id resolution cache)
 *
 * Revision 1.10  2003/10/16 12:08:16  ucko
 * Address GCC 2.95 errors about missing sprintf declaration, and avoid
 * possible buffer overflows, by rewriting s_MakeOverflowFileName to use
 * C++ strings.
 *
 * Revision 1.9  2003/10/16 00:30:57  ucko
 * ios_base -> IOS_BASE (should fix GCC 2.95 build)
 *
 * Revision 1.8  2003/10/15 18:39:13  kuznets
 * Fixed minor incompatibility with the C++ language.
 *
 * Revision 1.7  2003/10/15 18:13:16  kuznets
 * Implemented new cache architecture based on combination of BDB tables
 * and plain files. Fixes the performance degradation in Berkeley DB
 * when it has to work with a lot of overflow pages.
 *
 * Revision 1.6  2003/10/06 16:24:19  kuznets
 * Fixed bug in Purge function
 * (truncated cache files with some parameters combination).
 *
 * Revision 1.5  2003/10/02 20:13:25  kuznets
 * Minor code cleanup
 *
 * Revision 1.4  2003/09/29 16:26:34  kuznets
 * Reflected ERW_Result rename + cleaned up 64-bit compilation
 *
 * Revision 1.3  2003/09/29 15:45:17  kuznets
 * Minor warning fixed
 *
 * Revision 1.2  2003/09/24 15:59:45  kuznets
 * Reflected changes in IReader/IWriter <util/reader_writer.hpp>
 *
 * Revision 1.1  2003/09/24 14:30:17  kuznets
 * Initial revision
 *
 *
 * ===========================================================================
 */
