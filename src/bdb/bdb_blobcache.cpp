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

#include <corelib/ncbitime.hpp>
#include <bdb/bdb_blobcache.hpp>
#include <bdb/bdb_cursor.hpp>
#include <corelib/ncbimtx.hpp>


BEGIN_NCBI_SCOPE

// Mutex to sync cache requests coming from different threads
// All requests are protected with one mutex
static CFastMutex x_BDB_BLOB_CacheMutex;


class CBDB_BLOB_CacheIReader : public IReader
{
public:

    CBDB_BLOB_CacheIReader(CBDB_BLobStream* blob_stream)
    : m_BlobStream(blob_stream)
    {}

    virtual ~CBDB_BLOB_CacheIReader()
    {
        delete m_BlobStream;
    }

    virtual ERW_Result Read(void*   buf, 
                            size_t  count,
                            size_t* bytes_read)
    {
        if (count == 0)
            return eRW_Success;
        
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

    virtual ERW_Result PendingCount(size_t* /*count*/)
    {
        return eRW_NotImplemented;
    }


private:
    CBDB_BLOB_CacheIReader(const CBDB_BLOB_CacheIReader&);
    CBDB_BLOB_CacheIReader& operator=(const CBDB_BLOB_CacheIReader&);

private:
    CBDB_BLobStream* m_BlobStream;
};


class CBDB_BLOB_CacheIWriter : public IWriter
{
public:

    CBDB_BLOB_CacheIWriter(CBDB_BLobStream* blob_stream)
    : m_BlobStream(blob_stream)
    {}

    virtual ~CBDB_BLOB_CacheIWriter()
    {
        delete m_BlobStream;
    }

    virtual ERW_Result Write(const void* buf, size_t count,
                             size_t* bytes_written = 0)
    {
        if (count == 0)
            return eRW_Success;

        {{
        CFastMutexGuard guard(x_BDB_BLOB_CacheMutex);
        m_BlobStream->Write(buf, count);
        }}

        if (bytes_written)
            *bytes_written = count;

        return eRW_Success;
    }

private:
    CBDB_BLOB_CacheIWriter(const CBDB_BLOB_CacheIWriter&);
    CBDB_BLOB_CacheIWriter& operator=(const CBDB_BLOB_CacheIWriter&);

private:
    CBDB_BLobStream* m_BlobStream;
};





CBDB_BLOB_Cache::CBDB_BLOB_Cache()
{
}

CBDB_BLOB_Cache::~CBDB_BLOB_Cache()
{
}

void CBDB_BLOB_Cache::Open(const char* path)
{
    CFastMutexGuard guard(x_BDB_BLOB_CacheMutex);

    m_Env.OpenWithLocks(path);
    m_BlobDB.SetEnv(m_Env);
    m_TimeDB.SetEnv(m_Env);

    m_BlobDB.Open("lc_blob.db", CBDB_RawFile::eReadWriteCreate);
    m_TimeDB.Open("lc_time.db", CBDB_RawFile::eReadWriteCreate);
}

void CBDB_BLOB_Cache::Store(const string& key,
                           int           version,
                           const void*   data,
                           size_t        size,
                           EKeepVersions keep_versions)
{
    {{

    CFastMutexGuard guard(x_BDB_BLOB_CacheMutex);

    m_BlobDB.key = key;
    m_BlobDB.version = version;

    if (m_BlobDB.Fetch() == eBDB_Ok) {   // BLOB exists, nothing to do
        return;
    }

    }}

    if (keep_versions == eDropAll || keep_versions == eDropOlder) {
        Purge(key, 0, keep_versions);
    }


    CFastMutexGuard guard(x_BDB_BLOB_CacheMutex);

    m_BlobDB.key = key;
    m_BlobDB.version = version;

    m_BlobDB.Insert(data, size);

    x_UpdateAccessTime(key, version);

    m_TimeDB.key = key;
    m_TimeDB.version = version;

    CTime time_stamp(CTime::eCurrent);
    m_TimeDB.time_stamp = (unsigned)time_stamp.GetTimeT();

    m_TimeDB.UpdateInsert();
}


size_t CBDB_BLOB_Cache::GetSize(const string& key,
                               int           version)
{
    CFastMutexGuard guard(x_BDB_BLOB_CacheMutex);

    m_BlobDB.key = key;
    m_BlobDB.version = version;
    EBDB_ErrCode ret = m_BlobDB.Fetch();

    if (ret != eBDB_Ok) {
        return 0;
    }
    return m_BlobDB.LobSize();
}


bool CBDB_BLOB_Cache::Read(const string& key, 
                          int           version, 
                          void*         buf, 
                          size_t        buf_size)
{
    CFastMutexGuard guard(x_BDB_BLOB_CacheMutex);

    m_BlobDB.key = key;
    m_BlobDB.version = version;
    EBDB_ErrCode ret = m_BlobDB.Fetch();
    if (ret != eBDB_Ok) {
        return false;
    }
    ret = m_BlobDB.GetData(buf, buf_size);
    if (ret != eBDB_Ok) {
        return false;
    }

    x_UpdateAccessTime(key, version);

    return true;
}


void CBDB_BLOB_Cache::Remove(const string& key)
{
    Purge(key, 0, eDropAll);
}


void CBDB_BLOB_Cache::x_UpdateAccessTime(const string&  key,
                                         int            version)
{
    m_TimeDB.key = key;
    m_TimeDB.version = version;

    CTime time_stamp(CTime::eCurrent);
    m_TimeDB.time_stamp = (unsigned)time_stamp.GetTimeT();

    m_TimeDB.UpdateInsert();
}


time_t CBDB_BLOB_Cache::GetAccessTime(const string& key,
                                      int           version)
{
    CFastMutexGuard guard(x_BDB_BLOB_CacheMutex);

    m_TimeDB.key = key;
    m_TimeDB.version = version;

    EBDB_ErrCode ret = m_TimeDB.Fetch();
    if (ret == eBDB_Ok) {
        time_t t = m_TimeDB.time_stamp;
        return t;
    }

    return 0;
}

void CBDB_BLOB_Cache::Purge(time_t           access_time,
                            EKeepVersions    keep_last_version)
{
    CFastMutexGuard guard(x_BDB_BLOB_CacheMutex);

    if (keep_last_version == eDropAll && access_time == 0) {
        LOG_POST(Info << "CBDB_BLOB_Cache:: cache truncated");
        m_BlobDB.Truncate();
        m_TimeDB.Truncate();
        return;
    }

    // Search the records

    CBDB_FileCursor cur(m_TimeDB);
    cur.SetCondition(CBDB_FileCursor::eFirst);

    while (cur.Fetch() == eBDB_Ok) {
        time_t t = m_TimeDB.time_stamp;
        int version = m_TimeDB.version;
        const char* key = m_TimeDB.key;

        if (t < access_time) {
            x_DropBLOB(key, version);
        }
    }
}

void CBDB_BLOB_Cache::Purge(const string&    key,
                            time_t           access_time,
                            EKeepVersions    keep_last_version)
{
    CFastMutexGuard guard(x_BDB_BLOB_CacheMutex);

    // Search the records

    CBDB_FileCursor cur(m_TimeDB);
    cur.SetCondition(CBDB_FileCursor::eEQ);

    cur.From << key;

    while (cur.Fetch() == eBDB_Ok) {
        time_t t = m_TimeDB.time_stamp;

        const char* key_str = m_TimeDB.key;
        int version = m_TimeDB.version;

        if (access_time == 0 || 
            keep_last_version == eDropAll ||
            t < access_time) {
            x_DropBLOB(key_str, version);
        }
    }    
}


IReader* CBDB_BLOB_Cache::GetReadStream(const string& key, 
                                        int   version)
{
    CFastMutexGuard guard(x_BDB_BLOB_CacheMutex);

    m_BlobDB.key = key;
    m_BlobDB.version = version;
    
    EBDB_ErrCode ret = m_BlobDB.Fetch();
    if (ret != eBDB_Ok) {
        return 0;
    }
    CBDB_BLobStream* bstream = m_BlobDB.CreateStream();
    return new CBDB_BLOB_CacheIReader(bstream);

}

IWriter* CBDB_BLOB_Cache::GetWriteStream(const string&    key, 
                                         int              version,
                                         EKeepVersions    keep_versions)
{
    if (keep_versions == eDropAll || keep_versions == eDropOlder) {
        Purge(key, 0, keep_versions);
    }

    CFastMutexGuard guard(x_BDB_BLOB_CacheMutex);

    m_BlobDB.key = key;
    m_BlobDB.version = version;

    CBDB_BLobStream* bstream = m_BlobDB.CreateStream();
    return new CBDB_BLOB_CacheIWriter(bstream);
}

void CBDB_BLOB_Cache::x_DropBLOB(const char*    key,
                                 int            version)
{
    m_BlobDB.key = key;
    m_BlobDB.version = version;

    m_BlobDB.Delete();
}


END_NCBI_SCOPE

/*
 * ===========================================================================
 * $Log$
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
