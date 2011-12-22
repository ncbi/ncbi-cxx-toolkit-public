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
#include <corelib/ncbi_system.hpp>
#include <corelib/ncbi_limits.h>
#include <corelib/ncbimtx.hpp>
#include <corelib/ncbitime.hpp>
#include <corelib/plugin_manager_store.hpp>
#include <corelib/ncbiexpt.hpp>

#include <connect/server_monitor.hpp>

#include <db.h>

#include <db/bdb/bdb_blobcache.hpp>
#include <db/bdb/bdb_cursor.hpp>
#include <db/bdb/bdb_trans.hpp>

#include <db/error_codes.hpp>

#include <util/cache/icache_cf.hpp>
#include <util/cache/icache_clean_thread.hpp>
#include <util/simple_buffer.hpp>

#include <time.h>
#include <math.h>


#define NCBI_USE_ERRCODE_X   Db_Bdb_BlobCache


BEGIN_NCBI_SCOPE


// ===========================================================================
//
// Various implementation notes:
//
// The code below uses mixed transaction model, data store transactions
// go in the default mode (sync or async) (configurable).
// Miscelaneous service transactions (like those from GC (Purge)) are
// explicitly async (for speed). This is not critical if we lose some of them.
//
//

const char* kBDBCacheStartedFileName = "__ncbi_cache_started__";


static void s_MakeOverflowFileName(string& buf,
                                   const string& path,
                                   const string& cache,
                                   const string& key,
                                   int           version,
                                   const string& subkey)
{
    buf = path + cache + '_' + key + '_'
               + NStr::IntToString(version) + '_' + subkey + ".ov_";
}


/// @internal
struct SCacheDescr
{
    string    key;
    int       version;
    string    subkey;
    int       overflow;
    unsigned  blob_id;

    SCacheDescr(string   x_key,
                int      x_version,
                string   x_subkey,
                int      x_overflow,
                unsigned x_blob_id)
    : key(x_key),
      version(x_version),
      subkey(x_subkey),
      overflow(x_overflow),
      blob_id(x_blob_id)
    {}

    SCacheDescr() {}
};


/// @internal
class CBDB_CacheIReader : public IReader
{
public:
    CBDB_CacheIReader(CBDB_Cache&                bdb_cache,
                      CNcbiIfstream*             overflow_file,
                      CBDB_Cache::TBlobLock&     blob_lock)
    : m_Cache(bdb_cache),
      m_OverflowFile(overflow_file),
      m_RawBuffer(0),
      m_BufferPtr(0),
      m_BlobLock(blob_lock.GetLockVector(), blob_lock.GetTimeout())
    {
        m_BlobLock.TakeFrom(blob_lock);
    }

    CBDB_CacheIReader(CBDB_Cache&                bdb_cache,
                      CBDB_RawFile::TBuffer*     raw_buffer,
                      CBDB_Cache::TBlobLock&     blob_lock)
    : m_Cache(bdb_cache),
      m_OverflowFile(0),
      m_RawBuffer(raw_buffer),
      m_BufferPtr(raw_buffer->data()),
      m_BufferSize(raw_buffer->size()),
      m_BlobLock(blob_lock.GetLockVector(), blob_lock.GetTimeout())
    {
        m_BlobLock.TakeFrom(blob_lock);
    }

    virtual ~CBDB_CacheIReader()
    {
        if ( m_RawBuffer ) {
            if ( m_BufferSize ) {
                ERR_POST("CBDB_CacheIReader: detected unread input "<<
                         m_BufferSize);
            }
            delete m_RawBuffer;
        }
        if ( m_OverflowFile ) {
            CNcbiStreampos pos = m_OverflowFile->tellg();
            m_OverflowFile->seekg(0, IOS_BASE::end);
            CNcbiStreampos end = m_OverflowFile->tellg();
            if ( pos != end ) {
                ERR_POST("CBDB_CacheIReader: detected unread input "<<
                         (end-pos)<<": "<<pos<<" of "<<end);
            }
            delete m_OverflowFile;
        }
    }


    virtual ERW_Result Read(void*   buf,
                            size_t  count,
                            size_t* bytes_read)
    {
        if (count == 0)
            return eRW_Success;

        // Check if BLOB is memory based...
        if (m_RawBuffer) {
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
            m_OverflowFile->read((char*)buf, count);
            *bytes_read = m_OverflowFile->gcount();
            if (*bytes_read == 0) {
                return eRW_Eof;
            }
            return eRW_Success;
        }

        return eRW_Success;
    }

    virtual ERW_Result PendingCount(size_t* count)
    {
        if ( m_RawBuffer ) {
            *count = m_BufferSize;
            return eRW_Success;
        }
        else if ( m_OverflowFile ) {
            *count = m_OverflowFile->good()? 1: 0;
            return eRW_Success;
        }
        *count = 0;
        return eRW_Error;
    }


private:
    CBDB_CacheIReader(const CBDB_CacheIReader&);
    CBDB_CacheIReader& operator=(const CBDB_CacheIReader&);

private:
    CBDB_Cache&                 m_Cache;
    CNcbiIfstream*              m_OverflowFile;
    CBDB_RawFile::TBuffer*      m_RawBuffer;
    unsigned char*              m_BufferPtr;
    size_t                      m_BufferSize;
    CBDB_Cache::TBlobLock       m_BlobLock;
};

/// Buffer resize strategy, to balance memory reallocs and heap
/// consumption
///
/// @internal
class CCacheBufferResizeStrategy
{
public:
    static size_t GetNewCapacity(size_t cur_capacity, size_t requested_size)
    {
        size_t reserve = requested_size / 2;
        if (reserve > 1 * 1024 * 1024) {
            reserve = 1 * 1024 * 1024;
        }
        return requested_size + reserve;
    }
};


/// @internal
class CBDB_CacheIWriter : public IWriter
{
public:
    typedef
        CSimpleBufferT<unsigned char, CCacheBufferResizeStrategy> TBuffer;

public:
    CBDB_CacheIWriter(CBDB_Cache&                bdb_cache,
                      const char*                path,
                      unsigned                   blob_id_ext,
                      const string&              blob_key,
                      int                        version,
                      const string&              subkey,
                      SCache_AttrDB&             attr_db,
                      unsigned int               ttl,
                      time_t                     request_time,
                      const string&              owner,
                      CBDB_Cache::TBlobLock&     blob_lock)
    : m_Cache(bdb_cache),
      m_Path(path),
      m_BlobIdExt(blob_id_ext),
      m_BlobKey(blob_key),
      m_Version(version),
      m_SubKey(subkey),
      m_AttrDB(attr_db),
      m_Buffer(0),
//      m_BytesInBuffer(0),
      m_OverflowFile(0),
      m_TTL(ttl),
      m_RequestTime(request_time),
      m_Flushed(false),
      m_BlobSize(0),
      m_Overflow(0),
      m_BlobStore(0),
      m_BlobUpdate(0),
      m_Owner(owner),
      m_BlobLock(blob_lock.GetLockVector(), blob_lock.GetTimeout())
    {
        //_TRACE("CBDB_CacheIWriter::CBDB_CacheIWriter point 1");
//        m_Buffer = new unsigned char[m_Cache.GetOverflowLimit()];
        m_Buffer.reserve(4 * 1024);
        m_BlobLock.TakeFrom(blob_lock);
        //_TRACE("CBDB_CacheIWriter::CBDB_CacheIWriter point 2");
    }

    virtual ~CBDB_CacheIWriter()
    {
        //_TRACE("CBDB_CacheIWriter::~CBDB_CacheIWriter point 1");
        try {
            //bool upd_statistics = false;

            // Dumping the buffer
            try {
                //if (m_Buffer.size()) {
                if (!m_OverflowFile  &&  !m_Flushed) {
                    _TRACE("CBDB_CacheIWriter::~CBDB_CacheIWriter point 2");
                    m_Cache.x_Store(m_BlobIdExt,
                                    m_BlobKey,
                                    m_Version,
                                    m_SubKey,
                                    m_Buffer.data(),
                                    m_Buffer.size(), // m_BytesInBuffer,
                                    m_TTL,
                                    m_Owner,
                                    false // do not lock blob
                                    );
                    //delete[] m_Buffer; m_Buffer = 0;
                }
            } catch (CBDB_Exception& ) {
                //_TRACE("CBDB_CacheIWriter::~CBDB_CacheIWriter point 3");
                m_Cache.KillBlob(m_BlobKey, m_Version, m_SubKey,
                                 1,
                                 0);
                throw;
            }
            //_TRACE("CBDB_CacheIWriter::~CBDB_CacheIWriter point 4");

            if (m_OverflowFile) {
                if (m_OverflowFile->is_open()) {
                    //_TRACE("CBDB_CacheIWriter::~CBDB_CacheIWriter point 5");
                    m_OverflowFile->close();
                    try {
                        m_Cache.RegisterOverflow(m_BlobKey, m_Version, m_SubKey,
                                                 m_TTL, m_Owner);
                    } catch (exception& ) {
                        m_Cache.KillBlob(m_BlobKey, m_Version, m_SubKey, 1, 0);
                        throw;
                    }
                    //_TRACE("CBDB_CacheIWriter::~CBDB_CacheIWriter point 6");
                    //upd_statistics = true;
                }
                delete m_OverflowFile; m_OverflowFile = 0;

                // statistics
                // FIXME: ideally statistics should not sit on the same main mutex
                if (/*upd_statistics && */m_Cache.IsSaveStatistics()) {
                    try {
                        CFastMutexGuard guard(m_Cache.m_DB_Lock);
                        m_Cache.m_Statistics.AddStore(m_Owner,
                                                      m_RequestTime,
                                                      m_BlobStore,
                                                      m_BlobUpdate,
                                                      m_BlobSize,
                                                      m_Overflow);
                    } catch (exception& ) {
                        // ignore non critical exceptions
                    }
                }
            }
            //_TRACE("CBDB_CacheIWriter::~CBDB_CacheIWriter point 7");

        } catch (exception & ex) {
            ERR_POST_X(1, "Exception in ~CBDB_CacheIWriter() : " << ex.what()
                          << " " << m_BlobKey);
            // final attempt to avoid leaks
//            delete[] m_Buffer;
            delete   m_OverflowFile;
        }
    }

    virtual ERW_Result Write(const void*  buf,
                             size_t       count,
                             size_t*      bytes_written = 0)
    {
        _ASSERT(m_Flushed == false);

        if (bytes_written)
            *bytes_written = 0;
        if (count == 0)
            return eRW_Success;
//      m_AttrUpdFlag = false;
        m_BlobSize += count;
        m_Flushed = false;

        // check BLOB size quota
        if (m_Cache.GetMaxBlobSize()) {
            if (m_BlobSize > m_Cache.GetMaxBlobSize()) {
                //delete m_Buffer; m_Buffer = 0;
                m_Buffer.clear();
                delete m_OverflowFile; m_OverflowFile = 0;
                m_Cache.KillBlob(m_BlobKey, m_Version, m_SubKey, 1, 0);
// FIXME:
                if (m_Cache.IsSaveStatistics()) {
                    CFastMutexGuard guard(m_Cache.m_DB_Lock);
                    m_Cache.m_Statistics.AddBlobQuotaError(m_Owner);
                }

                string msg("BLOB larger than allowed. size=");
                msg.append(NStr::UIntToString(m_BlobSize));
                msg.append(" quota=");
                msg.append(NStr::UIntToString(m_Cache.GetMaxBlobSize()));
                BDB_THROW(eQuotaLimit, msg);
            }
        }


//        if (m_Buffer.size()) {
        if (!m_OverflowFile) {
            unsigned int new_buf_length = m_Buffer.size() + count;
            if (new_buf_length <= m_Cache.GetOverflowLimit()) {
                size_t old_size = m_Buffer.size();
                m_Buffer.resize(old_size + count);
                ::memcpy(m_Buffer.data() + old_size, buf, count);
                //m_BytesInBuffer = new_buf_length;
                if (bytes_written) {
                    *bytes_written = count;
                }

                return eRW_Success;
            } else {
                // Buffer overflow. Writing to file.
                OpenOverflowFile();
                if (m_OverflowFile) {
                    if (m_Buffer.size()) {
                        try {
                            x_WriteOverflow((const char*)m_Buffer.data(),
                                             m_Buffer.size());
                        }
                        catch (exception& ) {
                            m_Buffer.clear();
                            //delete[] m_Buffer; m_Buffer = 0;
                            delete m_OverflowFile; m_OverflowFile = 0;
                            throw;
                        }
                    }
                    //delete[] m_Buffer; m_Buffer = 0; m_BytesInBuffer = 0;
                    m_Buffer.clear();
                }
            }

        }
        if (m_OverflowFile) {

            _ASSERT(m_Buffer.size() == 0);

            x_WriteOverflow((char*)buf, count);
            if (bytes_written) {
                *bytes_written = count;
            }
            return eRW_Success;
        }

        return eRW_Error;
    }

    /// Flush pending data (if any) down to output device.
    virtual ERW_Result Flush(void)
    {
        if (m_Flushed)
            return eRW_Success;

        m_Flushed = true;

        // Dumping the buffer

//        if (m_Buffer.size()) {
        if (!m_OverflowFile) {

            try {
                m_Cache.x_Store(m_BlobIdExt,
                                m_BlobKey,
                                m_Version,
                                m_SubKey,
                                m_Buffer.data(),
                                m_Buffer.size(), // m_BytesInBuffer,
                                m_TTL,
                                m_Owner,
                                false // do not lock blob
                                );
                //m_BlobLock.Unlock();
                //return eRW_Success;
            }
            catch (exception&) {
                m_Buffer.clear();
//              delete[] m_Buffer; m_Buffer = 0;
                throw;
            }

            //delete[] m_Buffer; m_Buffer = 0;
            //m_BytesInBuffer = 0;
            //m_Buffer.clear();
            //return eRW_Success;
        }

        if ( m_OverflowFile ) {

            _ASSERT(m_Buffer.size() == 0);

            try {
                m_OverflowFile->flush();
                if ( m_OverflowFile->bad() ) {
                    m_OverflowFile->close();
                    BDB_THROW(eOverflowFileIO,
                              "Error trying to flush an overflow file");
                }
/*
                m_Cache.RegisterOverflow(m_BlobKey, m_Version, m_SubKey,
                                         m_TTL, m_Owner);

                // FIXME:
                if (m_Cache.IsSaveStatistics()) {
                    CFastMutexGuard guard(m_Cache.m_DB_Lock);
                    try {
                        m_Cache.m_Statistics.AddStore(m_Owner,
                                                    m_RequestTime,
                                                    m_BlobStore,
                                                    m_BlobUpdate,
                                                    m_BlobSize,
                                                    m_Overflow);
                    } catch (exception&) {
                        // ignore
                    }
                }
*/
            } catch (CBDB_Exception& ) {
                m_Cache.KillBlob(m_BlobKey, m_Version, m_SubKey,
                                 1,
                                 0);
                throw;
            }
        }
        m_BlobLock.Unlock();
        return eRW_Success;
    }
private:
    void OpenOverflowFile()
    {
        s_MakeOverflowFileName(
            m_OverflowFilePath, m_Path, m_Cache.GetName(), m_BlobKey, m_Version, m_SubKey);

        _TRACE("LC: Making overflow file " << m_OverflowFilePath);
        m_OverflowFile =
            new CNcbiOfstream(m_OverflowFilePath.c_str(),
                              IOS_BASE::out |
                              IOS_BASE::trunc |
                              IOS_BASE::binary);
        if (!m_OverflowFile->is_open() || m_OverflowFile->bad()) {
            delete m_OverflowFile; m_OverflowFile = 0;
            string err = "LC: Cannot create overflow file ";
            err += m_OverflowFilePath;
            BDB_THROW(eCannotOpenOverflowFile, err);
        }
        m_Overflow = 1;
    }

    void x_WriteOverflow(const char* buf, streamsize count)
    {
        _ASSERT(m_OverflowFile);

        if (!m_OverflowFile->is_open()) {
            BDB_THROW(eOverflowFileIO,
                 "LC: Attempt to write to a non-open overflow file");
        }
        try {
            m_Cache.WriteOverflow(*m_OverflowFile,
                                   m_OverflowFilePath,
                                   buf, count);
        }
        catch (exception&) {
            // any error here is critical to the BLOB storage integrity
            // if file IO error happens we delete BLOB altogether
            m_Cache.KillBlob(m_BlobKey, m_Version, m_SubKey, 1, 0);
            throw;
        }
    }

private:
    CBDB_CacheIWriter(const CBDB_CacheIWriter&);
    CBDB_CacheIWriter& operator=(const CBDB_CacheIWriter&);

private:
    CBDB_Cache&           m_Cache;
    const char*           m_Path;
    unsigned              m_BlobIdExt;
    string                m_BlobKey;
    int                   m_Version;
    string                m_SubKey;
    SCache_AttrDB&        m_AttrDB;

    TBuffer               m_Buffer;
//    unsigned char*        m_Buffer;
//    unsigned int          m_BytesInBuffer;
    CNcbiOfstream*        m_OverflowFile;
    string                m_OverflowFilePath;

    int                   m_StampSubKey;
//  bool                  m_AttrUpdFlag; ///< Flags attributes are up to date
    CBDB_Cache::EWriteSyncMode m_WSync;
    unsigned int          m_TTL;
    time_t                m_RequestTime;
    bool                  m_Flushed;     ///< FALSE until Flush() called

    unsigned              m_BlobSize;    ///< Number of bytes written
    unsigned              m_Overflow;    ///< Overflow file created
    unsigned              m_BlobStore;
    unsigned              m_BlobUpdate;
    string                m_Owner;
    CBDB_Cache::TBlobLock m_BlobLock;
};


SBDB_CacheUnitStatistics::SBDB_CacheUnitStatistics()
{
    Init();
}

void SBDB_CacheUnitStatistics::Init()
{
    blobs_stored_total = blobs_updates_total =
        blobs_never_read_total = blobs_expl_deleted_total =
        blobs_purge_deleted_total = blobs_db =
        blobs_overflow_total = blobs_read_total = blob_size_max_total = 0;

    err_protocol = err_internal = err_communication =
        err_no_blob = err_blob_get = err_blob_put = err_blob_over_quota = 0;

    blobs_size_total = blobs_size_db = 0.0;
    InitHistorgam(&blob_size_hist);

    time_access.erase(time_access.begin(), time_access.end());
}

void SBDB_CacheUnitStatistics::InitHistorgam(TBlobSizeHistogram* hist)
{
    _ASSERT(hist);

    hist->clear();
    unsigned hist_value = 512;
    for (unsigned i = 0; i < 100; ++i) {
        (*hist)[hist_value] = 0;
        hist_value *= 2;
    }
    (*hist)[kMax_UInt] = 0;
}

void SBDB_CacheUnitStatistics::AddToHistogram(TBlobSizeHistogram* hist,
                                          unsigned            size)
{
    if (hist->empty()) {
        return;
    }
    TBlobSizeHistogram::iterator it = hist->upper_bound(size);
    if (it == hist->end()) {
        return;
    }
    ++(it->second);
}


/// Compute current hour of day
static
void s_GetDayHour(time_t time_in_secs, unsigned* day, unsigned* hour)
{
    *day = static_cast<unsigned>(time_in_secs / (24 * 60 * 60));
    unsigned secs_in_day = static_cast<unsigned>(time_in_secs % (24 * 60 * 60));
    *hour = secs_in_day / 3600;
}

void SBDB_CacheUnitStatistics::AddStore(time_t   tm,
                                        unsigned store,
                                        unsigned update,
                                        unsigned blob_size,
                                        unsigned overflow)
{
    blobs_stored_total += store;
    blobs_updates_total += update;
    blobs_size_total += blob_size;
    blobs_overflow_total += overflow;
    if (blob_size > blob_size_max_total) {
        blob_size_max_total = blob_size;
    }
    AddToHistogram(&blob_size_hist, blob_size);

    unsigned day, hour;
    s_GetDayHour(tm, &day, &hour);
    if (time_access.empty()) {
        time_access.push_back(SBDB_TimeAccessStatistics(day, hour, 1, 0));
        return;
    }
    SBDB_TimeAccessStatistics& ta_stat = time_access.back();
    if (ta_stat.day == day && ta_stat.hour == hour) {
        ++ta_stat.put_count;
    } else {
        time_access.push_back(SBDB_TimeAccessStatistics(day, hour, 1, 0));
        if (time_access.size() > 48) {
            time_access.pop_front();
        }
    }
}

void SBDB_CacheUnitStatistics::AddRead(time_t tm)
{
    ++blobs_read_total;

    unsigned day, hour;
    s_GetDayHour(tm, &day, &hour);
    if (time_access.empty()) {
        time_access.push_back(SBDB_TimeAccessStatistics(day, hour, 0, 1));
        return;
    }
    SBDB_TimeAccessStatistics& ta_stat = time_access.back();
    if (ta_stat.day == day && ta_stat.hour == hour) {
        ++ta_stat.get_count;
    } else {
        time_access.push_back(SBDB_TimeAccessStatistics(day, hour, 0, 1));
        if (time_access.size() > 48) {
            time_access.pop_front();
        }
    }
}

void SBDB_CacheUnitStatistics::x_AddErrGetPut(EErrGetPut operation)
{
    switch (operation) {
    case eErr_Put:
        ++err_blob_put;
        break;
    case eErr_Get:
        ++err_blob_get;
        break;
    case eErr_Unknown:
        break;
    default:
        _ASSERT(0);
        break;
    } // switch
}

void SBDB_CacheUnitStatistics::AddInternalError(EErrGetPut operation)
{
    ++err_internal;
    x_AddErrGetPut(operation);
}

void SBDB_CacheUnitStatistics::AddBlobQuotaError()
{
    ++err_blob_over_quota;
    x_AddErrGetPut(eErr_Put);
}

void SBDB_CacheUnitStatistics::AddProtocolError(EErrGetPut operation)
{
    ++err_protocol;
    x_AddErrGetPut(operation);
}

void SBDB_CacheUnitStatistics::AddNoBlobError(EErrGetPut operation)
{
    ++err_no_blob;
    x_AddErrGetPut(operation);
}

void SBDB_CacheUnitStatistics::AddCommError(EErrGetPut operation)
{
    ++err_communication;
    x_AddErrGetPut(operation);
}

void SBDB_CacheUnitStatistics::PrintStatistics(CNcbiOstream& out) const
{
    out
        << "Total number of blobs ever stored                  " << "\t" << blobs_stored_total   << "\n"
        << "Total number of overflow blobs (large size)        " << "\t" << blobs_overflow_total << "\n"
        << "Total number of blobs updates                      " << "\t" << blobs_updates_total    << "\n"
        << "Total number of blobs stored but never read        " << "\t" << blobs_never_read_total << "\n"
        << "Total number of reads                              " << "\t" << blobs_read_total << "\n"
        << "Total number of explicit deletes                   " << "\t" << blobs_expl_deleted_total << "\n"
        << "Total number of BLOBs deletes by garbage collector " << "\t" << blobs_purge_deleted_total << "\n"
        << "Total size of all BLOBs ever stored                " << "\t" << blob_size_max_total << "\n"

        << "Current database number of records(BLOBs)          " << "\t" << blobs_db << "\n"
        << "Current size of all BLOBs                          "  << "\t" << blobs_size_db << "\n"

        << "Number of NetCache protocol errors                 " << "\t" << err_protocol << "\n"
        << "Number of communication errors                     " << "\t" << err_communication << "\n"
        << "Number of NetCache server internal errors          " << "\t" << err_internal << "\n"
        << "Number of BLOB not found situations                " << "\t" << err_no_blob << "\n"
        << "Number of errors when getting BLOBs                " << "\t" << err_blob_get << "\n"
        << "Number of errors when storing BLOBs                " << "\t" << err_blob_put << "\n"
        << "Number of errors when BLOB is over the size limit  " << "\t" << err_blob_over_quota << "\n";

    out << "\n\n";

    if (!time_access.empty()) {
        out << "# Time access statistics:" << "\n" << "\n";
        out << "# Hour \t Puts \t Gets" << "\n";

        ITERATE(TTimeAccess, it, time_access) {
            const SBDB_TimeAccessStatistics& ta_stat = *it;
            out << ta_stat.hour << "\t"
                << ta_stat.put_count << "\t"
                << ta_stat.get_count << "\n";
        }
    }
    out << "\n\n";

    if (!blob_size_hist.empty()) {
        out << "# BLOB size histogram:" << "\n" << "\n";
        out << "# Size \t Count" << "\n";

        const TBlobSizeHistogram& hist = blob_size_hist;
        SBDB_CacheUnitStatistics::TBlobSizeHistogram::const_iterator hist_end =
            hist.end();

        ITERATE(SBDB_CacheUnitStatistics::TBlobSizeHistogram, it, hist) {
            if (it->second > 0) {
                hist_end = it;
            }
        }
        ITERATE(SBDB_CacheUnitStatistics::TBlobSizeHistogram, it, hist) {
            out << it->first << "\t" << it->second << "\n";
            if (it == hist_end) {
                break;
            }
        } // for
    }

}

void SBDB_CacheUnitStatistics::ConvertToRegistry(IRWRegistry* reg,
                                                 const string& sect_name_postfix) const
{
    string postfix(sect_name_postfix);
    postfix = NStr::Replace(postfix, " ", "_");

    // convert main parameters

    {{
    string sect_stat("bdb_stat");
     if (!postfix.empty()) {
        sect_stat.append("_");
        sect_stat.append(postfix);
    }

    reg->Set(sect_stat, "blobs_stored_total",
             NStr::UIntToString(blobs_stored_total), 0,
             "Total number of blobs ever stored");
    reg->Set(sect_stat, "blobs_overflow_total",
             NStr::UIntToString(blobs_overflow_total), 0,
             "Total number of overflow blobs (large size)");
    reg->Set(sect_stat, "blobs_updates_total",
             NStr::UIntToString(blobs_updates_total), 0,
             "Total number of blobs updates");
    reg->Set(sect_stat, "blobs_never_read_total",
             NStr::UIntToString(blobs_never_read_total), 0,
             "Total number of blobs stored but never read");
    reg->Set(sect_stat, "blobs_read_total",
             NStr::UIntToString(blobs_read_total), 0,
             "Total number of reads");
    reg->Set(sect_stat, "blobs_expl_deleted_total",
             NStr::UIntToString(blobs_expl_deleted_total), 0,
             "Total number of explicit deletes");
    reg->Set(sect_stat, "blobs_purge_deleted_total",
             NStr::UIntToString(blobs_purge_deleted_total), 0,
             "Total number of BLOBs deletes by garbage collector");
    reg->Set(sect_stat, "blobs_size_total",
             NStr::UIntToString(unsigned(blobs_size_total)), 0,
             "Total size of all BLOBs ever stored");
    reg->Set(sect_stat, "blob_size_max_total",
             NStr::UIntToString(blob_size_max_total), 0,
             "Size of the largest BLOB ever stored");


    reg->Set(sect_stat, "blobs_db",
             NStr::UIntToString(blobs_db), 0,
             "Current database number of records(BLOBs)");
    reg->Set(sect_stat, "blobs_size_db",
             NStr::UIntToString(unsigned(blobs_size_db)), 0,
             "Current size of all BLOBs");

    reg->Set(sect_stat, "err_protocol",
             NStr::UIntToString(err_protocol), 0,
             "Number of NetCache protocol errors");
    reg->Set(sect_stat, "err_communication",
             NStr::UIntToString(err_communication), 0,
             "Number of communication errors");
    reg->Set(sect_stat, "err_internal",
             NStr::UIntToString(err_internal), 0,
             "Number of NetCache server internal errors");
    reg->Set(sect_stat, "err_no_blob",
             NStr::UIntToString(err_no_blob), 0,
             "Number of BLOB not found situations");
    reg->Set(sect_stat, "err_blob_get",
             NStr::UIntToString(err_blob_get), 0,
             "Number of errors when getting BLOBs");
    reg->Set(sect_stat, "err_blob_put",
             NStr::UIntToString(err_blob_put), 0,
             "Number of errors when storing BLOBs");
    reg->Set(sect_stat, "err_blob_over_quota",
             NStr::UIntToString(err_blob_over_quota), 0,
             "Number of errors when BLOB is over the size limit");

    // convert time access statistics
    //
    if (!time_access.empty()) {

        string get_history;
        string put_history;

        ITERATE(TTimeAccess, it, time_access) {
            const SBDB_TimeAccessStatistics& ta_stat = *it;
            string hour = NStr::UIntToString(ta_stat.hour);
            string put_count = NStr::UIntToString(ta_stat.put_count);
            string get_count = NStr::UIntToString(ta_stat.get_count);

            string pair = hour;
            pair.append("=");
            pair.append(put_count);

            if (!put_history.empty()) {
                put_history.append(";");
            }
            put_history.append(pair);


            pair = hour;
            pair.append("=");
            pair.append(get_count);

            if (!get_history.empty()) {
                get_history.append(";");
            }
            get_history.append(pair);

        } // ITERATE

        reg->Set(sect_stat, "get_access",
             get_history, 0,
             "Read access by hours (hour=count)");
        reg->Set(sect_stat, "put_access",
             put_history, 0,
             "Write access by hours (hour=count)");

    }


    }}

    // convert historgam
    //
    if (!blob_size_hist.empty()) {
        string sect_hist("bdb_stat_hist");
        if (!postfix.empty()) {
            sect_hist.append("_");
            sect_hist.append(postfix);
        }

        const TBlobSizeHistogram& hist = blob_size_hist;
        SBDB_CacheUnitStatistics::TBlobSizeHistogram::const_iterator hist_end =
            hist.end();

        ITERATE(SBDB_CacheUnitStatistics::TBlobSizeHistogram, it, hist) {
            if (it->second > 0) {
                hist_end = it;
            }
        }
        ITERATE(SBDB_CacheUnitStatistics::TBlobSizeHistogram, it, hist) {
            string var_name = "size_";
            var_name += NStr::UIntToString(it->first);

            reg->Set(sect_hist, var_name, NStr::UIntToString(it->second));

            if (it == hist_end) {
                break;
            }
        } // for
    }

}




SBDB_CacheStatistics::SBDB_CacheStatistics()
{
}

void SBDB_CacheStatistics::Init()
{
    m_GlobalStat.Init();
    m_OwnerStatMap.erase(m_OwnerStatMap.begin(), m_OwnerStatMap.end());
}


void SBDB_CacheStatistics::AddStore(const string& client,
                                    time_t        tm,
                                    unsigned      store,
                                    unsigned      update,
                                    unsigned      blob_size,
                                    unsigned      overflow)
{
    m_GlobalStat.AddStore(tm, store, update, blob_size, overflow);
    if (!client.empty()) {
        m_OwnerStatMap[client].AddStore(tm, store, update, blob_size, overflow);
    }
}

void SBDB_CacheStatistics::AddRead(const string&  client, time_t tm)
{
    m_GlobalStat.AddRead(tm);
    if (!client.empty()) {
        m_OwnerStatMap[client].AddRead(tm);
    }
}

void SBDB_CacheStatistics::AddExplDelete(const string&  client)
{
    m_GlobalStat.AddExplDelete();
    if (!client.empty()) {
        m_OwnerStatMap[client].AddExplDelete();
    }
}

void SBDB_CacheStatistics::AddPurgeDelete(const string& client)
{
    m_GlobalStat.AddPurgeDelete();
    if (!client.empty()) {
        m_OwnerStatMap[client].AddPurgeDelete();
    }
}

void SBDB_CacheStatistics::AddNeverRead(const string&   client)
{
    m_GlobalStat.AddNeverRead();
    if (!client.empty()) {
        m_OwnerStatMap[client].AddNeverRead();
    }
}

void SBDB_CacheStatistics::AddBlobQuotaError(const string& client)
{
    m_GlobalStat.AddBlobQuotaError();
    if (!client.empty()) {
        m_OwnerStatMap[client].AddBlobQuotaError();
    }
}

void SBDB_CacheStatistics::AddInternalError(const string& client,
                     SBDB_CacheUnitStatistics::EErrGetPut operation)
{
    m_GlobalStat.AddInternalError(operation);
    if (!client.empty()) {
        m_OwnerStatMap[client].AddInternalError(operation);
    }
}

void SBDB_CacheStatistics::AddProtocolError(const string& client,
                     SBDB_CacheUnitStatistics::EErrGetPut operation)
{
    m_GlobalStat.AddProtocolError(operation);
    if (!client.empty()) {
        m_OwnerStatMap[client].AddProtocolError(operation);
    }
}

void SBDB_CacheStatistics::AddNoBlobError(const string& client,
                   SBDB_CacheUnitStatistics::EErrGetPut operation)
{
    m_GlobalStat.AddNoBlobError(operation);
    if (!client.empty()) {
        m_OwnerStatMap[client].AddNoBlobError(operation);
    }
}

void SBDB_CacheStatistics::AddCommError(const string& client,
                 SBDB_CacheUnitStatistics::EErrGetPut operation)
{
    m_GlobalStat.AddCommError(operation);
    if (!client.empty()) {
        m_OwnerStatMap[client].AddCommError(operation);
    }
}

void SBDB_CacheStatistics::ConvertToRegistry(IRWRegistry* reg) const
{
    m_GlobalStat.ConvertToRegistry(reg, kEmptyStr);
    ITERATE(TOwnerStatMap, it, m_OwnerStatMap) {
        const string& client = it->first;
        const SBDB_CacheUnitStatistics& stat = it->second;
        stat.ConvertToRegistry(reg, client);
    }
}

void SBDB_CacheStatistics::PrintStatistics(CNcbiOstream& out) const
{
    out << "## " << "\n"
        << "## Global statistics" << "\n"
        << "## " << "\n\n";
    m_GlobalStat.PrintStatistics(out);
    out << "\n\n";

    ITERATE(TOwnerStatMap, it, m_OwnerStatMap) {
        const string& client = it->first;
        const SBDB_CacheUnitStatistics& stat = it->second;

        out << "## " << "\n"
            << "## Owner statistics:" << client << "\n"
            << "## " << "\n\n";

        stat.PrintStatistics(out);
        out << "\n\n";
    }
}


CBDB_Cache::CBDB_Cache()
: m_PidGuard(0),
  m_ReadOnly(false),
  m_InitIfDirty(false),
  m_JoinedEnv(false),
  m_LockTimeout(20 * 1000),
  m_Env(0),
  m_Closed(true),
  m_BLOB_SplitStore(0),
  m_CacheAttrDB(0),
  m_CacheIdIDX(0),
  m_CacheAttrDB_RO1(0),
  m_CacheAttrDB_RO2(0),
  m_CacheIdIDX_RO(0),
  m_Timeout(0),
  m_MaxTimeout(0),
  m_VersionFlag(eDropOlder),
  m_WSync(eWriteNoSync),
  m_PurgeBatchSize(150),
  m_BatchSleep(0),
  m_PurgeStopSignal(0, 100), // purge stop semaphore max 100
  m_CleanLogOnPurge(0),
  m_PurgeCount(0),
  m_LogSizeMax(0),
  m_PurgeNowRunning(false),
  m_RunPurgeThread(false),
  m_PurgeThreadDelay(10),
  m_CheckPointInterval(24 * (1024 * 1024)),
  m_OverflowLimit(512 * 1024),
  m_MaxTTL_prolong(0),
  m_SaveStatistics(false),
  m_MaxBlobSize(0),
  m_RoundRobinVolumes(0),
  m_MempTrickle(10),
  m_Monitor(0),
  m_TimeLine(0)
{
    m_TimeStampFlag = fTimeStampOnRead |
                      fExpireLeastFrequentlyUsed |
                      fPurgeOnStartup;

}

CBDB_Cache::~CBDB_Cache()
{
    try {
        Close();
    } catch (exception& )
    {}
}

void CBDB_Cache::SetTTL_Prolongation(unsigned ttl_prolong)
{
    m_MaxTTL_prolong = ttl_prolong;
}

void CBDB_Cache::x_PidLock(ELockMode lm)
{
    string lock_file = string("lcs_") + m_Name + string(".pid");
    string lock_file_path = m_Path + lock_file;

    switch (lm)
    {
    case ePidLock:
        _ASSERT(m_PidGuard == 0);
        m_PidGuard = new CPIDGuard(lock_file_path, m_Path);
        break;
    case eNoLock:
        break;
    default:
        break;
    }
}

void CBDB_Cache::SetOverflowLimit(unsigned limit)
{
    _ASSERT(!IsOpen());
    m_OverflowLimit = limit;
}

void CBDB_Cache::Open(const string& cache_path,
                      const string& cache_name,
                      ELockMode     lm,
                      Uint8         cache_ram_size,
                      ETRansact     use_trans,
                      unsigned int  log_mem_size)
{
    {{
    m_NextExpTime = 0;
//    m_PurgeSkipCnt = 0;
    m_LastTimeLineCheck = 0;

    Close();
    m_Closed = false;

    CFastMutexGuard guard(m_DB_Lock);

    m_Path = CDirEntry::AddTrailingPathSeparator(cache_path);
    m_Name = cache_name;

    CDir dir(m_Path);
    CDir::TEntries fl = dir.GetEntries("__db.*", CDir::eIgnoreRecursive);
    CFile fl_clean(CDirEntry::MakePath(m_Path, kBDBCacheStartedFileName));

    static set<string> s_OpenedDirs;

    if ((!fl.empty()  ||  fl_clean.Exists())
        &&  s_OpenedDirs.count(m_Path) == 0)
    {
        if (m_InitIfDirty) {
            LOG_POST("Database was closed uncleanly. Removing directory "
                     << m_Path);
            dir.Remove();
        }
        else {
            // Opening db with recover flags is unreliable.
            LOG_POST("Database was closed uncleanly. Running recovery...");
            BDB_RecoverEnv(m_Path, false);
            fl_clean.Remove();
        }
    }


    // Make sure our directory exists
    if ( !dir.Exists() ) {
        dir.Create();
    }

    if (!fl_clean.Exists()) {
        CFileWriter writer(fl_clean.GetPath());
        string pid = NStr::NumericToString(CProcess::GetCurrentPid());
        writer.Write(pid.data(), pid.size());
        s_OpenedDirs.insert(m_Path);
    }

    m_Env = new CBDB_Env();

    string err_file = m_Path + "err" + string(cache_name) + ".log";
    m_Env->OpenErrFile(err_file.c_str());

    m_JoinedEnv = false;
    bool needs_recovery = false;

    if (!fl.empty()) {
        try {
            m_Env->JoinEnv(cache_path, CBDB_Env::eThreaded);
            if (m_Env->IsTransactional()) {
                LOG_POST_X(2, Info <<
                           "LC: '" << cache_name <<
                           "' Joined transactional environment ");
            } else {
                LOG_POST_X(3, Info <<
                           "LC: '" << cache_name <<
                           "' Warning: Joined non-transactional environment ");
            }
            m_JoinedEnv = true;
        }
        catch (CBDB_ErrnoException& err_ex)
        {
            if (err_ex.IsRecovery()) {
                LOG_POST_X(4, Warning <<
                           "LC: '" << cache_name <<
                           "'Warning: DB_ENV returned DB_RUNRECOVERY code."
                           " Running the recovery procedure.");
            }
            needs_recovery = true;
        }
        catch (CBDB_Exception&)
        {
            needs_recovery = true;
        }
    }

    if (!m_JoinedEnv) {
        if (log_mem_size == 0) {
            if (m_LogSizeMax >= (20 * 1024 * 1024)) {
                m_Env->SetLogFileMax(m_LogSizeMax);
            } else {
                m_Env->SetLogFileMax(200 * 1024 * 1024);
            }
            m_Env->SetLogBSize(1 * 1024 * 1024);
        } else {
            m_Env->SetLogInMemory(true);
            m_Env->SetLogBSize(log_mem_size);
        }
        if (!m_LogDir.empty()) {
            m_Env->SetLogDir(m_LogDir);
        }

        if (cache_ram_size) {
            // empirically if total cache size is large
            // you Berkeley DB works faster if you break it into several
            // files. The sweetspot seems to be around 250M per file.
            // (but growing number of files is not a good idea as well)
            // so here I'm dancing to set some reasonable number of caches
            //
            int cache_num = 1;
            if (cache_ram_size > (500 * 1024 * 1024)) {
                cache_num = (int)((cache_ram_size) / Uint8(250 * 1024 * 1024));
            }
            if (!cache_num) {
                cache_num = 1;  // paranoid check
            }
            if (cache_num > 10) { // too many files?
                cache_num = 10;
            }
            m_Env->SetCacheSize(cache_ram_size, cache_num);
        }

        unsigned checkpoint_KB = m_CheckPointInterval / 1024;
        if (checkpoint_KB == 0) {
            checkpoint_KB = 64;
        }
        m_Env->SetCheckPointKB(checkpoint_KB);

        m_Env->SetLogAutoRemove(true);

        x_PidLock(lm);
        switch (use_trans)
        {
        case eUseTrans:
            {
            CBDB_Env::TEnvOpenFlags env_flags = CBDB_Env::eThreaded;
            if (needs_recovery) {
                env_flags |= CBDB_Env::eRunRecovery;
            }
            m_Env->SetMaxLocks(50000);
            m_Env->OpenWithTrans(cache_path, env_flags);
            }
            break;
        case eNoTrans:
            LOG_POST_X(5, Info << "BDB_Cache: Creating locking environment");
            m_Env->OpenWithLocks(cache_path);
            break;
        default:
            _ASSERT(0);
        } // switch

        m_Env->SetLockTimeout(30 * 1000000); // 30 sec
        if (m_Env->IsTransactional()) {
            m_Env->SetTransactionTimeout(30 * 1000000); // 30 sec
        }

    }

    if (GetWriteSync() == eWriteSync) {
        m_Env->SetTransactionSync(CBDB_Transaction::eTransSync);
    } else {
        m_Env->SetTransactionSync(CBDB_Transaction::eTransASync);
    }

    m_BLOB_SplitStore =
        new TSplitStore(new CBDB_BlobDeMux_RoundRobin(m_RoundRobinVolumes));
    m_CacheAttrDB = new SCache_AttrDB();
    m_CacheAttrDB_RO1 = new SCache_AttrDB();
    m_CacheAttrDB_RO2 = new SCache_AttrDB();
    m_CacheIdIDX = new SCache_IdIDX();
    m_CacheIdIDX_RO = new SCache_IdIDX();

    m_BLOB_SplitStore->SetEnv(*m_Env);
    m_CacheAttrDB->SetEnv(*m_Env);
    m_CacheAttrDB_RO1->SetEnv(*m_Env);
    m_CacheAttrDB_RO2->SetEnv(*m_Env);
    m_CacheIdIDX->SetEnv(*m_Env);
    m_CacheIdIDX_RO->SetEnv(*m_Env);

    string cache_blob_db_name =
       string("lcs_") + string(cache_name) + string("_blob");
    string attr_db_name =
       string("lcs_") + string(cache_name) + string("_attr5") + string(".db");
    string id_idx_name =
       string("lcs_") + string(cache_name) + string("_id") + string(".idx");

    m_BLOB_SplitStore->Open(cache_blob_db_name, CBDB_RawFile::eReadWriteCreate);
    m_CacheAttrDB->Open(attr_db_name,        CBDB_RawFile::eReadWriteCreate);
    m_CacheAttrDB_RO1->Open(attr_db_name,    CBDB_RawFile::eReadOnly);
    m_CacheAttrDB_RO2->Open(attr_db_name,    CBDB_RawFile::eReadOnly);
    m_CacheIdIDX->Open(id_idx_name,          CBDB_RawFile::eReadWriteCreate);
    m_CacheIdIDX_RO->Open(id_idx_name,       CBDB_RawFile::eReadOnly);

    // try to open all databases
    //
    m_BLOB_SplitStore->OpenProjections();

    }}

    // Create timeline for BLOB expiration
    //

    {{
        unsigned timeline_precision = 5 * 60; // 5min default precision
        if (m_Timeout) {
            timeline_precision = m_Timeout / 10;
        }
        if (timeline_precision > (5 * 60)) { // 5 min max.
            timeline_precision = 5 * 60;
        }
        if (timeline_precision < (1 * 60)) {
            timeline_precision = 1 * 60;
        }
        m_TimeLine = new TTimeLine(timeline_precision, 0);
    }}


    // read cache attributes so we can adjust atomic counter
    //

    {{
    LOG_POST_X(6, Info << "Scanning cache content.");

    m_CacheAttrDB->SetTransaction(0);

    CBDB_FileCursor cur(*m_CacheAttrDB);
    cur.SetCondition(CBDB_FileCursor::eFirst);
    cur.InitMultiFetch(10 * 1024 * 1024);
    unsigned max_blob_id = 0;
    while (cur.Fetch() == eBDB_Ok) {
        unsigned blob_id = m_CacheAttrDB->blob_id;
        max_blob_id = max(max_blob_id, blob_id);

        unsigned coord[2];
        coord[0] = m_CacheAttrDB->volume_id;
        coord[1] = m_CacheAttrDB->split_id;

        m_BLOB_SplitStore->AssignCoordinates(blob_id, coord);

        // check BLOB id index integrity
        {{
            m_CacheIdIDX->blob_id = blob_id;
            if (m_CacheIdIDX->Fetch() == eBDB_Ok) {
            } else {
                // index not found... corruption?
                string key = m_CacheAttrDB->key.GetString();
                int version = m_CacheAttrDB->version;
                string subkey = m_CacheAttrDB->subkey.GetString();

                m_CacheIdIDX->blob_id = blob_id;
                m_CacheIdIDX->key = key;
                m_CacheIdIDX->version = version;
                m_CacheIdIDX->subkey = subkey;

                m_CacheIdIDX->Insert();
            }
        }}

    }
    m_BlobIdCounter.Set(max_blob_id);
    }}

    if (m_TimeStampFlag & fPurgeOnStartup) {
        unsigned batch_sleep = m_BatchSleep;
        unsigned batch_size = m_PurgeBatchSize;
        unsigned purge_thread_delay = m_PurgeThreadDelay;

        // setup parameters which favor fast Purge execution
        // (Open purge needs to be fast because all waiting for it)

        if (m_PurgeBatchSize < 2500) {
            m_PurgeBatchSize = 2500;
        }
        m_BatchSleep = 0;
        m_PurgeThreadDelay = 0;

        Purge(GetTimeout());

        // Restore parameters

        m_BatchSleep = batch_sleep;
        m_PurgeBatchSize = batch_size;
        m_PurgeThreadDelay = purge_thread_delay;
    }


    if (m_RunPurgeThread) {
# ifdef NCBI_THREADS
       LOG_POST_X(7, Info << "Starting cache cleaning thread.");
       m_PurgeThread.Reset(
           new CCacheCleanerThread(this, m_PurgeThreadDelay, 5));
       m_PurgeThread->Run();

        if (!m_JoinedEnv) {
            CBDB_Env::TBackgroundFlags flags =
                CBDB_Env::eBackground_MempTrickle  |
                CBDB_Env::eBackground_Checkpoint   |
                CBDB_Env::eBackground_DeadLockDetect;
            //_TRACE("Running background writer with delay " << m_PurgeThreadDelay);
            m_Env->RunBackgroundWriter(flags,
                                       m_CheckPointDelay,
                                       m_MempTrickle);
        }
# else
       LOG_POST_X(8, Warning <<
                  "Cannot run background thread in non-MT configuration.");
       m_Env->TransactionCheckpoint();
# endif
    }

    m_ReadOnly = false;

    LOG_POST_X(9, Info <<
               "LC: '" << cache_name <<
               "' Cache mount at: " << cache_path);

}

void CBDB_Cache::RunPurgeThread(unsigned purge_delay)
{
    m_RunPurgeThread = true;
    m_PurgeThreadDelay = purge_delay;
}

void CBDB_Cache::StopPurgeThread()
{
# ifdef NCBI_THREADS
    if (!m_PurgeThread.Empty()) {
        LOG_POST_X(10, Info << "Stopping cache cleaning thread...");
        StopPurge();
        m_PurgeThread->RequestStop();
        m_PurgeThread->Join();
        LOG_POST_X(11, Info << "Stopped.");
    }
# endif
}

void CBDB_Cache::OpenReadOnly(const string&  cache_path,
                              const string&  cache_name,
                              unsigned int   cache_ram_size)
{
    {{

    Close();

    CFastMutexGuard guard(m_DB_Lock);

    m_Path = CDirEntry::AddTrailingPathSeparator(cache_path);
    m_Name = cache_name;

    m_BLOB_SplitStore = new TSplitStore(new CBDB_BlobDeMux_RoundRobin(0));
    m_CacheAttrDB = new SCache_AttrDB();
/*
    if (cache_ram_size)
        m_CacheBLOB_DB->SetCacheSize(cache_ram_size);
*/
    string cache_blob_db_name =
       string("lcs_") + string(cache_name) + string("_blob");
    string attr_db_name =
       m_Path + string("lcs_") + string(cache_name) + string("_attr5")
              + string(".db");


    m_BLOB_SplitStore->Open(cache_blob_db_name,  CBDB_RawFile::eReadOnly);
    m_CacheAttrDB->Open(attr_db_name.c_str(),    CBDB_RawFile::eReadOnly);

    }}

    m_ReadOnly = true;

    LOG_POST_X(12, Info <<
               "LC: '" << cache_name <<
               "' Cache mount read-only at: " << cache_path);
}


void CBDB_Cache::Close()
{
    if (m_Closed) return;
    // We mark cache as closed early to prevent double close attempt if
    // exception fires before completion. We MUST not StopPurgeThread twice.
    m_Closed = true;
    StopPurgeThread();

    if (m_Env && !m_JoinedEnv) {
        m_Env->StopBackgroundWriterThread();
    }

    if (m_BLOB_SplitStore) {
        m_BLOB_SplitStore->Save();
    }

    delete m_PidGuard;        m_PidGuard = 0;

    delete m_CacheAttrDB_RO1; m_CacheAttrDB_RO1 = 0;
    delete m_CacheAttrDB_RO2; m_CacheAttrDB_RO2 = 0;
    delete m_CacheIdIDX_RO;   m_CacheIdIDX_RO = 0;

    delete m_BLOB_SplitStore; m_BLOB_SplitStore = 0;
    delete m_CacheAttrDB;     m_CacheAttrDB = 0;
    delete m_CacheIdIDX;      m_CacheIdIDX = 0;

    delete m_TimeLine;        m_TimeLine = 0;

    if (m_Env == 0) {
        return;
    }
    try {
        m_Env->ForceTransactionCheckpoint();
        CleanLog();

        if (m_Env->CheckRemove()) {
            LOG_POST_X(13, Info <<
                        "LC: '" << m_Name << "' Unmounted. BDB ENV deleted.");
        } else {
            LOG_POST_X(14, Info << "LC: '" << m_Name
                                << "' environment still in use.");
        }
    }
    catch (exception& ex) {
        LOG_POST_X(15, Warning << "LC: '" << m_Name
                         << "' Exception in Close() " << ex.what()
                         << " (ignored.)");
    }

    delete m_Env; m_Env = 0;

    CFile fl_clean(CDirEntry::MakePath(m_Path, kBDBCacheStartedFileName));
    fl_clean.Remove();
}

void CBDB_Cache::CleanLog()
{
    if (m_Env) {
        if (m_Env->IsTransactional()) {
            m_Env->CleanLog();
        }
    }
}

void CBDB_Cache::x_Close()
{
    delete m_PidGuard;    m_PidGuard = 0;
    delete m_CacheAttrDB; m_CacheAttrDB = 0;
    delete m_Env;         m_Env = 0;
}

ICache::TFlags CBDB_Cache::GetFlags()
{
    return (TFlags) 0;
}

void CBDB_Cache::SetFlags(ICache::TFlags flags)
{
}

void CBDB_Cache::SetTimeStampPolicy(TTimeStampFlags policy,
                                    unsigned int    timeout,
                                    unsigned int    max_timeout)
{
    CFastMutexGuard guard(m_DB_Lock);

    m_TimeStampFlag = policy;
    m_Timeout = timeout;

    if (max_timeout) {
        m_MaxTimeout = max_timeout > timeout ? max_timeout : timeout;
    } else {
        if (m_MaxTTL_prolong) {
            m_MaxTimeout = timeout * m_MaxTTL_prolong;
        } else {
            m_MaxTimeout = 0;
        }
    }
}

CBDB_Cache::TTimeStampFlags CBDB_Cache::GetTimeStampPolicy() const
{
    return m_TimeStampFlag;
}

int CBDB_Cache::GetTimeout() const
{
    return m_Timeout;
}

void CBDB_Cache::SetVersionRetention(EKeepVersions policy)
{
    CFastMutexGuard guard(m_DB_Lock);
    m_VersionFlag = policy;
}

CBDB_Cache::EKeepVersions CBDB_Cache::GetVersionRetention() const
{
    return m_VersionFlag;
}

void CBDB_Cache::GetStatistics(SBDB_CacheStatistics* cache_stat) const
{
    _ASSERT(cache_stat);

    CFastMutexGuard guard(m_DB_Lock);
    *cache_stat = m_Statistics;
}

void CBDB_Cache::InitStatistics()
{
    CFastMutexGuard guard(m_DB_Lock);
    m_Statistics.Init();
}

void CBDB_Cache::KillBlob(const string&  key,
                          int            version,
                          const string&  subkey,
                          int            overflow,
                          unsigned       blob_id)
{

    CBDB_Transaction trans(*m_Env,
                            CBDB_Transaction::eEnvDefault,
                            CBDB_Transaction::eNoAssociation);
    {{
        CFastMutexGuard guard(m_DB_Lock);
        m_BLOB_SplitStore->SetTransaction(&trans);
        x_DropBlob(key, version, subkey, overflow, blob_id, trans);
    }}
    trans.Commit();

}

void CBDB_Cache::DropBlob(const string&  key,
                          int            version,
                          const string&  subkey,
                          bool           for_update,
                          unsigned*      blob_id,
                          unsigned*      coord)
{
    _ASSERT(blob_id);
    _ASSERT(coord);

    int overflow = 0;

    {{

    CBDB_Transaction trans(*m_Env,
                            CBDB_Transaction::eEnvDefault,
                            CBDB_Transaction::eNoAssociation);

    {{

        CFastMutexGuard guard(m_DB_Lock);
        m_CacheAttrDB->SetTransaction(&trans);

        CBDB_FileCursor cur(*m_CacheAttrDB, trans,
                            CBDB_FileCursor::eReadModifyUpdate);
        cur.SetCondition(CBDB_FileCursor::eEQ);

        cur.From << key << version << subkey;

        if (cur.Fetch() == eBDB_Ok) {
            overflow = m_CacheAttrDB->overflow;
            *blob_id = m_CacheAttrDB->blob_id;

            coord[0] = m_CacheAttrDB->volume_id;
            coord[1] = m_CacheAttrDB->split_id;

            if (!for_update) {   // permanent BLOB removal
                string owner_name;
                m_CacheAttrDB->owner_name.ToString(owner_name);

                // FIXME:
                if (IsSaveStatistics()) {
                    m_Statistics.AddExplDelete(owner_name);
                    if (0 == m_CacheAttrDB->read_count) {
                        m_Statistics.AddNeverRead(owner_name);
                    }
                    x_UpdateOwnerStatOnDelete(owner_name,
                                              true);//explicit del
                }

                cur.Delete();
            }

        } else {
            *blob_id = 0;
            return;
        }
    }}

    // delete the split store BLOB
    if (!for_update) {   // permanent BLOB removal
        unsigned split_coord[2];
        EBDB_ErrCode ret =
            m_BLOB_SplitStore->GetCoordinates(*blob_id, split_coord);

        m_BLOB_SplitStore->SetTransaction(&trans);
        if (ret == eBDB_Ok) {
            if (coord[0] != split_coord[0] || coord[1] != split_coord[1]) {
                m_BLOB_SplitStore->Delete(*blob_id, split_coord,
                                          CBDB_RawFile::eThrowOnError);
            }
        }
        m_BLOB_SplitStore->Delete(*blob_id, coord,
                                  CBDB_RawFile::eThrowOnError);

    }

    trans.Commit();
    }}

    if (overflow) {
        x_DropOverflow(key.c_str(), version, subkey.c_str());
    }

}

void CBDB_Cache::RegisterOverflow(const string&  key,
                                  int            version,
                                  const string&  subkey,
                                  unsigned       time_to_live,
                                  const string&  owner)
{
    time_t curr = time(0);

    unsigned blob_id = 0;
    unsigned coord[2]={0,};

    {{
        CBDB_Transaction trans(*m_Env,
                            CBDB_Transaction::eEnvDefault,
                            CBDB_Transaction::eNoAssociation);

        {{
        CFastMutexGuard guard(m_DB_Lock);
        m_CacheAttrDB->SetTransaction(&trans);
        m_CacheIdIDX->SetTransaction(&trans);

        EBDB_ErrCode ret;
        {{
            CBDB_FileCursor cur(*m_CacheAttrDB,
                                trans,
                                CBDB_FileCursor::eReadModifyUpdate);
            cur.SetCondition(CBDB_FileCursor::eEQ);
            cur.From << key << version << subkey;

            ret = cur.Fetch();
            if (ret == eBDB_Ok) {
                m_CacheAttrDB->time_stamp = (unsigned)curr;
                m_CacheAttrDB->overflow = 1;
                m_CacheAttrDB->ttl = time_to_live;
                m_CacheAttrDB->max_time = (Uint4)ComputeMaxTime(curr);
                unsigned upd_count = m_CacheAttrDB->upd_count;
                ++upd_count;
                m_CacheAttrDB->upd_count = upd_count;
                m_CacheAttrDB->owner_name = owner;

                blob_id = m_CacheAttrDB->blob_id;
                coord[0] = m_CacheAttrDB->volume_id;
                coord[1] = m_CacheAttrDB->split_id;

                cur.Update();
            }
        }} // cursor

        if (ret != eBDB_Ok) { // record not found: re-registration
            unsigned blob_id = GetNextBlobId(false /*no locking*/);
            m_CacheAttrDB->key = key;
            m_CacheAttrDB->version = version;
            m_CacheAttrDB->subkey = subkey;
            m_CacheAttrDB->time_stamp = (unsigned)curr;
            m_CacheAttrDB->overflow = 1;
            m_CacheAttrDB->ttl = time_to_live;
            m_CacheAttrDB->max_time = (Uint4)ComputeMaxTime(curr);
            m_CacheAttrDB->upd_count  = 0;
            m_CacheAttrDB->read_count = 0;
            m_CacheAttrDB->owner_name = owner;
            m_CacheAttrDB->blob_id = blob_id;
            m_CacheAttrDB->volume_id = 0;
            m_CacheAttrDB->split_id = 0;

            ret = m_CacheAttrDB->Insert();
            if (ret != eBDB_Ok) {
                LOG_POST_X(16, Error << "Failed to insert BLOB attributes "
                               << key << " " << version << " " << subkey);
            } else {
                m_CacheIdIDX->blob_id = blob_id;
                m_CacheIdIDX->key = key;
                m_CacheIdIDX->version = version;
                m_CacheIdIDX->subkey = subkey;

                ret = m_CacheIdIDX->Insert();
                if (ret != eBDB_Ok) {
                    LOG_POST_X(17, Error << "Failed to insert BLOB id index "
                                   << key << " " << version << " " << subkey);
                }
            }


        }

        }} // m_DB_Lock

        trans.Commit();
    }} // trans

    if (blob_id) { // clean up the split store
        CBDB_Transaction trans(*m_Env,
                            CBDB_Transaction::eEnvDefault,
                            CBDB_Transaction::eNoAssociation);

        m_BLOB_SplitStore->SetTransaction(&trans);

        try {
            unsigned split_coord[2];
            EBDB_ErrCode ret =
                m_BLOB_SplitStore->GetCoordinates(blob_id, split_coord);
            if (ret == eBDB_Ok) {
                if (coord[0] != split_coord[0] || coord[1] != split_coord[1]) {
                    m_BLOB_SplitStore->Delete(blob_id, split_coord,
                                                CBDB_RawFile::eThrowOnError);
                }
            }
            m_BLOB_SplitStore->Delete(blob_id, coord,
                                    CBDB_RawFile::eThrowOnError);
        }
        catch (CBDB_Exception& ex) {
            LOG_POST_X(18, Error << "Cannot delete BLOB from split store "
                           << ex.what());
            throw;
        }
        trans.Commit();
    }
}

void CBDB_Cache::x_Store(unsigned       blob_id,
                         const string&  key,
                         int            version,
                         const string&  subkey,
                         const void*    data,
                         size_t         size,
                         unsigned int   time_to_live,
                         const string&  owner,
                         bool           do_blob_lock)
{
    if (IsReadOnly()) {
        return;
    }
    //_TRACE("CBDB_Cache::x_Store point 1 size=" << size);
    unsigned coord[2] = {0,};
    unsigned overflow = 0, old_overflow = 0;
    EBDB_ErrCode ret;

    time_t curr = time(0);
    int tz_delta = m_LocalTimer.GetLocalTimezone();

    // ----------------------------------------------------
    // check BLOB size quota
    // ----------------------------------------------------
    if (GetMaxBlobSize() && size > GetMaxBlobSize()) {
        // FIXME:
        if (IsSaveStatistics()) {
            CFastMutexGuard guard(m_DB_Lock);
            m_Statistics.AddBlobQuotaError(owner);
        }

        string msg("BLOB larger than allowed. size=");
        msg.append(NStr::SizetToString(size));
        msg.append(" quota=");
        msg.append(NStr::UIntToString(GetMaxBlobSize()));
        BDB_THROW(eQuotaLimit, msg);
    }


    // ----------------------------------------------------
    // Optional pre-cleaning
    // ----------------------------------------------------

    if (m_VersionFlag == eDropAll || m_VersionFlag == eDropOlder) {
        //_TRACE("CBDB_Cache::x_Store point 2");
        Purge(key, subkey, 0, m_VersionFlag);
    }

    TBlobLock blob_lock(m_LockVector, m_LockTimeout);

    // ----------------------------------------------------
    // BLOB check-in, read attributes, lock the id
    // ----------------------------------------------------

    EBlobCheckinRes check_res =
        BlobCheckIn(blob_id,
                    key, version, subkey,
                    eBlobCheckIn_Create,
                    blob_lock, do_blob_lock,
                    &coord[0], &coord[1],
                    &old_overflow);
    //_TRACE("CBDB_Cache::x_Store point 3");
    _ASSERT(check_res != EBlobCheckIn_NotFound);
    _ASSERT(blob_lock.GetId());

    blob_id = blob_lock.GetId();

    try {

        if (size > GetOverflowLimit()) {
            //_TRACE("CBDB_Cache::x_Store point 4");
            // ----------------------------------------------------
            // write overflow file
            // ----------------------------------------------------
            string path;
            s_MakeOverflowFileName(path, m_Path, GetName(), key, version, subkey);
            {{
                CNcbiOfstream oveflow_file(path.c_str(),
                                        IOS_BASE::out |
                                        IOS_BASE::trunc |
                                        IOS_BASE::binary);
                if (!oveflow_file.is_open() || oveflow_file.bad()) {
                    string err = "LC: Cannot create overflow file ";
                    err += path;
                    BDB_THROW(eCannotOpenOverflowFile, err);
                }
                WriteOverflow(oveflow_file, path, (const char*)data, size);
            }}
            overflow = 1;
        }

        // ----------------------------------------------------
        // Check if BLOB changed overflow location
        // ----------------------------------------------------

        if (overflow != old_overflow && check_res == eBlobCheckIn_Found) {
            if (overflow) { // BLOB goes from split to file
                //_TRACE("CBDB_Cache::x_Store point 5");
                // ----------------------------------------------------
                // Delete old split store content
                // ----------------------------------------------------
                CBDB_Transaction trans(*m_Env,
                                        CBDB_Transaction::eEnvDefault,
                                        CBDB_Transaction::eNoAssociation);
                m_BLOB_SplitStore->SetTransaction(&trans);

                try {
                    unsigned split_coord[2];
                    EBDB_ErrCode ret =
                        m_BLOB_SplitStore->GetCoordinates(blob_id, split_coord);
                    if (ret == eBDB_Ok) {
                        if (coord[0] != split_coord[0] || coord[1] != split_coord[1]) {
                            m_BLOB_SplitStore->Delete(blob_id, split_coord,
                                                      CBDB_RawFile::eThrowOnError);
                        }
                    }
                    m_BLOB_SplitStore->Delete(blob_id, coord,
                                              CBDB_RawFile::eThrowOnError);
                }
                catch (CBDB_Exception& ex) {
                    LOG_POST_X(19, Error << "Cannot delete BLOB from split store "
                                   << ex.what());
                    throw;
                }
                trans.Commit();
                coord[0] = coord[1] = 0;
            } else {
                //_TRACE("CBDB_Cache::x_Store point 6");
                // ----------------------------------------------------
                // Delete old overflow file
                // ----------------------------------------------------

                x_DropOverflow(key, version, subkey);
            }
        }

        // ----------------------------------------------------
        // Update the split store content
        // ----------------------------------------------------

        {{
            //_TRACE("CBDB_Cache::x_Store point 7");
            CBDB_Transaction trans(*m_Env,
                                    CBDB_Transaction::eEnvDefault,
                                    CBDB_Transaction::eNoAssociation);
            m_BLOB_SplitStore->SetTransaction(&trans);
            //_TRACE("CBDB_Cache::x_Store point 32");

            m_BLOB_SplitStore->UpdateInsert(blob_id,
                                            coord,
                                            data, size,
                                            coord);
            //_TRACE("CBDB_Cache::x_Store point 33");
            trans.Commit();
        }}


        // ----------------------------------------------------
        // Update BLOB attributes
        // ----------------------------------------------------
        if (m_MaxTimeout) {
            if (time_to_live > m_MaxTimeout) {
                time_to_live = m_MaxTimeout;
            }
        } else { // m_MaxTimeout == 0
            if (m_MaxTTL_prolong != 0 && m_Timeout != 0) {
                time_to_live = min(m_Timeout * m_MaxTTL_prolong, time_to_live);
            }
        }

        time_t ttl_max = ComputeMaxTime(curr);

        {{
            CBDB_Transaction trans(*m_Env,
                                CBDB_Transaction::eEnvDefault,
                                CBDB_Transaction::eNoAssociation);
            {{
                CFastMutexGuard guard(m_DB_Lock);
                m_CacheAttrDB->SetTransaction(&trans);

                CBDB_FileCursor cur(*m_CacheAttrDB,
                                    trans,
                                    CBDB_FileCursor::eReadModifyUpdate);
                cur.SetCondition(CBDB_FileCursor::eEQ);
                cur.From << key << version << subkey;
                ret = cur.Fetch();
                if (ret == eBDB_Ok) {
                    old_overflow = m_CacheAttrDB->overflow;

                    m_CacheAttrDB->time_stamp = (unsigned)curr;
                    m_CacheAttrDB->overflow = overflow;
                    m_CacheAttrDB->ttl = time_to_live;
                    m_CacheAttrDB->max_time = (Uint4)ttl_max;
                    unsigned upd_count = m_CacheAttrDB->upd_count;
                    m_CacheAttrDB->upd_count = ++upd_count;
                    m_CacheAttrDB->owner_name = owner;

                    // here is a paranoid check in case blob id ever changes
                    // this should never happen!
                    //
                    unsigned old_blob_id = m_CacheAttrDB->blob_id;
                    if (blob_id != old_blob_id) {
                        BDB_THROW(eRaceCondition,
                                  "BLOB id mutation detected!");
                    }
                    m_CacheAttrDB->volume_id = coord[0];
                    m_CacheAttrDB->split_id = coord[1];

                    ret = cur.Update();
                }
            }} // m_DB_Lock


            if (ret == eBDB_Ok) {
                trans.Commit();
            } else {
                // BLOB got deleted!
                string msg = "BLOB insert-delete race detected!";
                if (m_Monitor && m_Monitor->IsActive()) {
                    msg += "\n";
                    m_Monitor->Send(msg);
                }
                BDB_THROW(eRaceCondition, msg);
            }

        }} // trans

        {{
            time_t exp_time =
                x_ComputeExpTime((int)curr, time_to_live, GetTimeout());

            CFastMutexGuard guard(m_TimeLine_Lock);
            m_TimeLine->AddObject(exp_time, blob_id);
        }} // m_TimeLine_Lock


        // FIXME: locks, correct statistics, etc
        unsigned blob_updated = 0;
        unsigned blob_stored  = 0;

        if (check_res == eBlobCheckIn_Found) {
            blob_updated = 1;
        } else {
            blob_stored = 1;
        }

        if (IsSaveStatistics()) {
            CFastMutexGuard guard(m_DB_Lock);
            m_Statistics.AddStore(owner, curr - tz_delta,
                                  blob_stored, blob_updated, size, overflow);
        }

    }
    catch (exception)
    {
        // if things go wrong we do not want the database in an uncertain state, so
        // BLOB is getting killed here
        //

        KillBlob(key, version, subkey,
                 1, // try to delete overflow file too
                 0);
        throw;
    }
}


void CBDB_Cache::Store(unsigned       blob_id_ext,
                       const string&  key,
                       int            version,
                       const string&  subkey,
                       const void*    data,
                       size_t         size,
                       unsigned int   time_to_live,
                       const string&  owner)
{
    x_Store(blob_id_ext, key, version, subkey, data, size, time_to_live, owner,
            true // do blob is locking
            );
}


void CBDB_Cache::Store(const string&  key,
                       int            version,
                       const string&  subkey,
                       const void*    data,
                       size_t         size,
                       unsigned int   time_to_live,
                       const string&  owner)
{
    x_Store(0, key, version, subkey, data, size, time_to_live, owner,
            true // do blob is locking
            );
}


bool CBDB_Cache::GetSizeEx(const string&  key,
                           int            version,
                           const string&  subkey,
                           size_t*        size)
{
    size_t blob_size = 0;
    int overflow;
    unsigned int ttl, blob_id, volume_id, split_id;

    blob_id = GetBlobId(key, version, subkey);
    if (!blob_id) return false;
    TBlobLock blob_lock(m_LockVector, blob_id, m_LockTimeout);

    {{
        CFastMutexGuard guard(m_DB_Lock);
        m_CacheAttrDB->SetTransaction(0);

        bool rec_exists =
            x_RetrieveBlobAttributes(key, version, subkey,
                                    overflow, ttl,
                                    blob_id, volume_id, split_id);
        if (!rec_exists) return false;

        // check expiration here
        // joukovv 2006-06-24: expiration of WHAT? Apparently, not of a given
        // BLOB - no BLOB-specific parameters passed. Should we bother?
        if (m_TimeStampFlag & fCheckExpirationAlways) {
            if (x_CheckTimeStampExpired(*m_CacheAttrDB, time(0)))
                return false;
        }
        overflow = m_CacheAttrDB->overflow;
    }}

    if (overflow) {
        string path;
        s_MakeOverflowFileName(path, m_Path, GetName(), key, version, subkey);
        CFile entry(path);

        if (!entry.Exists())
            return false;
        blob_size = (size_t)entry.GetLength();
    } else {
        // Regular inline BLOB
        if (blob_id == 0) return false;

        unsigned coords[2];
        coords[0] = volume_id;
        coords[1] = split_id;

        m_BLOB_SplitStore->SetTransaction(0);

        EBDB_ErrCode ret =
            m_BLOB_SplitStore->BlobSize(blob_id, coords, &blob_size);
        if (ret != eBDB_Ok) return false;
    }
    if (size) *size = blob_size;
    return true;
}


size_t CBDB_Cache::GetSize(const string&  key,
                           int            version,
                           const string&  subkey)
{
    size_t size;
    return GetSizeEx(key, version, subkey, &size) ? size : 0;
}


void CBDB_Cache::GetBlobOwner(const string&  key,
                              int            version,
                              const string&  subkey,
                              string*        owner)
{
    _ASSERT(owner);

    CFastMutexGuard guard(m_DB_Lock);
    m_CacheAttrDB->SetTransaction(0);

    if (!x_FetchBlobAttributes(key, version, subkey)) {
        owner->erase();
        return;
    }

    m_CacheAttrDB->owner_name.ToString(*owner);
}


bool CBDB_Cache::HasBlobs(const string&  key,
                          const string&  subkey)
{
    time_t curr = time(0);

    CFastMutexGuard guard(m_DB_Lock);
    m_CacheAttrDB->SetTransaction(0);

    CBDB_FileCursor cur(*m_CacheAttrDB);

    // Simple case - key only. Just look it up
    if (subkey.empty()) {
        cur.SetCondition(CBDB_FileCursor::eEQ);
        cur.From << key;
        return cur.FetchFirst() == eBDB_Ok &&
               !x_CheckTimeStampExpired(*m_CacheAttrDB, curr);
    }

    // More complicated case with subkey
    int version = 0;
    for (;;) {
        // Get version for key
        cur.SetCondition(CBDB_FileCursor::eGE);
        cur.From << key;
        cur.From << version;
        if (cur.FetchFirst() != eBDB_Ok)
            return false;
        const char* found_key = m_CacheAttrDB->key;
        if ( key != found_key )
            return false;
        // We have version, now fetch subkey
        version = m_CacheAttrDB->version;
        cur.SetCondition(CBDB_FileCursor::eEQ);
        cur.From << key;
        cur.From << version;
        cur.From << subkey;
        if (cur.FetchFirst() == eBDB_Ok &&
            !x_CheckTimeStampExpired(*m_CacheAttrDB, curr))
            return true;
        ++version;
    }

    return false;

    /* I don't know why we need this extra check if we already have
       a record in CacheAttrDB. Moreover, for overflown blobs there is
       NO record in SplitStore DB. So I am commenting this out.
       joukovv 2007-11-27
    TBlobLock blob_lock(m_LockVector, blob_id, m_LockTimeout);

    _ASSERT(blob_id);
    unsigned split_coord[2];
    EBDB_ErrCode ret =
        m_BLOB_SplitStore->GetCoordinates(blob_id, split_coord);
    if (ret != eBDB_Ok) {
        return false;
    } */
}


bool CBDB_Cache::Read(const string& key,
                      int           version,
                      const string& subkey,
                      void*         buf,
                      size_t        buf_size)
{
    EBDB_ErrCode ret;

    time_t curr = time(0);
    int tz_delta = m_LocalTimer.GetLocalTimezone();

    int overflow = 0;
    unsigned volume_id = 0, split_id = 0;

    unsigned blob_id = GetBlobId(key, version, subkey);
    if (!blob_id) return false;
    TBlobLock blob_lock(m_LockVector, blob_id, m_LockTimeout);


    CBDB_Transaction trans(*m_Env,
                        CBDB_Transaction::eTransASync, // async!
                        CBDB_Transaction::eNoAssociation);


    {{
        CFastMutexGuard guard(m_DB_Lock);
        m_CacheAttrDB->SetTransaction(&trans);
        CBDB_FileCursor cur(*m_CacheAttrDB, trans,
                            CBDB_FileCursor::eReadModifyUpdate);
        cur.SetCondition(CBDB_FileCursor::eEQ);
        cur.From << key << version << subkey;
        ret = cur.Fetch();
        if (ret == eBDB_Ok) {
            if (m_TimeStampFlag & fCheckExpirationAlways) {
                if (x_CheckTimeStampExpired(*m_CacheAttrDB, curr)) {
                    return false;
                }
            }
            unsigned read_count = m_CacheAttrDB->read_count;
            m_CacheAttrDB->read_count = read_count + 1;

            unsigned max_time = m_CacheAttrDB->max_time;
            if (max_time == 0 || max_time >= (unsigned)curr) {
                if ( m_TimeStampFlag & fTimeStampOnRead ) {
                    m_CacheAttrDB->time_stamp = (unsigned)curr;
                }
            }

            blob_id = m_CacheAttrDB->blob_id;
            overflow = m_CacheAttrDB->overflow;
            volume_id = m_CacheAttrDB->volume_id;
            split_id = m_CacheAttrDB->split_id;

            ret = cur.Update();

        } else {
            return false;
        }
    }} // m_DB_Lock

    if (ret != eBDB_Ok) {
        return false;
    }
    _ASSERT(blob_id);
    trans.Commit();

    //  FIXME: locks, etc
    string owner_name;
    m_CacheAttrDB->owner_name.ToString(owner_name);
    if (IsSaveStatistics()) {
        CFastMutexGuard guard(m_DB_Lock);
        m_Statistics.AddRead(owner_name, curr - tz_delta);
    }

    // read the data


    if (overflow) {
        string path;
        s_MakeOverflowFileName(path, m_Path, GetName(),key, version, subkey);

        auto_ptr<CNcbiIfstream>
            overflow_file(new CNcbiIfstream(path.c_str(),
                                            IOS_BASE::in | IOS_BASE::binary));
        if (!overflow_file->is_open()) {
            // TODO: Kill the registration record
            return false;
        }
        overflow_file->read((char*)buf, buf_size);
        if (!*overflow_file) {
            return false;
        }
    }
    else {
        if (blob_id == 0) {
            return false;
        }
        m_BLOB_SplitStore->SetTransaction(0);

        unsigned coords[2];
        ret = m_BLOB_SplitStore->GetCoordinates(blob_id, coords);
        if (ret == eBDB_Ok) {
            if (coords[0] != volume_id ||
                coords[1] != split_id) {
                // TODO: restore de-mux mapping
            }
        } else {
            // TODO: restore de-mux mapping
        }

        coords[0] = volume_id;
        coords[1] = split_id;

        size_t blob_size;
        ret = m_BLOB_SplitStore->Fetch(blob_id, coords,
                                       &buf, buf_size,
                                       CBDB_RawFile::eReallocForbidden,
                                       &blob_size);

        if (ret != eBDB_Ok) {
            return false;
        }
    }
    return true;



/*

    {{
        CFastMutexGuard guard(m_DB_Lock);
        m_CacheAttrDB->SetTransaction(0);

        int overflow;
        unsigned int ttl, blob_id, volume_id, split_id;
        bool rec_exists =
            x_RetrieveBlobAttributes(key, version, subkey,
                                    overflow, ttl,
                                    blob_id, volume_id, split_id);
        if (!rec_exists) {
            return false;
        }

        // check expiration
        if (m_TimeStampFlag & fCheckExpirationAlways) {
            if (x_CheckTimeStampExpired()) {
                return false;
            }
        }

        m_CacheAttrDB->owner_name.ToString(m_TmpOwnerName);
        m_Statistics.AddRead(m_TmpOwnerName, curr - tz_delta);


        if (overflow) {
            string path;
            s_MakeOverflowFileName(path, m_Path, GetName(),key, version, subkey);

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

            if (blob_id == 0) {
                return false;
            }

            m_BLOB_SplitStore->SetTransaction(0);


            unsigned coords[2];
            ret = m_BLOB_SplitStore->GetCoordinates(blob_id, coords);
            if (ret == eBDB_Ok) {
                if (coords[0] != volume_id ||
                    coords[1] != split_id) {
                    // TO DO: restore de-mux mapping
                }
            } else {
                // TODO: restore de-mux mapping
            }

            coords[0] = volume_id;
            coords[1] = split_id;

            size_t blob_size;

            ret =
                m_BLOB_SplitStore->Fetch(blob_id, coords,
                                        &buf, buf_size,
                                        CBDB_RawFile::eReallocForbidden,
                                        &blob_size);

            if (ret != eBDB_Ok) {
                return false;
            }
        }



        if ( m_TimeStampFlag & fTimeStampOnRead ) {
            m_CacheAttrDB->SetTransaction(&trans);
            x_UpdateReadAccessTime(key, version, subkey, trans);
        }

    }} // m_DB_Lock

    trans.Commit();


    return true;
*/

}

IReader* CBDB_Cache::x_CreateOverflowReader(const string&  key,
                                            int            version,
                                            const string&  subkey,
                                            size_t&        file_length,
                                            TBlobLock&     blob_lock)
{
    string path;
    s_MakeOverflowFileName(path, m_Path, GetName(), key, version, subkey);
    auto_ptr<CNcbiIfstream>
        overflow_file(new CNcbiIfstream(path.c_str(),
                                        IOS_BASE::in | IOS_BASE::binary));
    if (!overflow_file->is_open()) {
        return 0;
    }
    CFile entry(path);
    file_length = (size_t) entry.GetLength();

    return new CBDB_CacheIReader(*this, overflow_file.release(), blob_lock);
}


IReader* CBDB_Cache::GetReadStream(const string&  key,
                                   int            version,
                                   const string&  subkey)
{
    EBDB_ErrCode ret;

    time_t curr = time(0);
    int tz_delta = m_LocalTimer.GetLocalTimezone();
    int overflow = 0;
    unsigned volume_id = 0, split_id = 0;

    unsigned blob_id = GetBlobId(key, version, subkey);
    if (!blob_id) return 0;
    TBlobLock blob_lock(m_LockVector, blob_id, m_LockTimeout);


    // TODO: unify read prolog code for all read functions

    CBDB_Transaction trans(*m_Env,
                        CBDB_Transaction::eTransASync, // async!
                        CBDB_Transaction::eNoAssociation);

    {{
        CFastMutexGuard guard(m_DB_Lock);
        {{ // TODO: nested scope here is nonsensical
            m_CacheAttrDB->SetTransaction(&trans);
            CBDB_FileCursor cur(*m_CacheAttrDB, trans,
                                CBDB_FileCursor::eReadModifyUpdate);
            cur.SetCondition(CBDB_FileCursor::eEQ);
            cur.From << key << version << subkey;
            ret = cur.Fetch();
            if (ret == eBDB_Ok) {
                if (m_TimeStampFlag & fCheckExpirationAlways) {
                    if (x_CheckTimeStampExpired(*m_CacheAttrDB, curr)) {
                        return false;
                    }
                }
                unsigned read_count = m_CacheAttrDB->read_count;
                m_CacheAttrDB->read_count = read_count + 1;

                unsigned max_time = m_CacheAttrDB->max_time;
                if (max_time == 0 || max_time >= (unsigned) curr) {
                    if ( m_TimeStampFlag & fTimeStampOnRead ) {
                        m_CacheAttrDB->time_stamp = (unsigned)curr;
                    }
                }

                blob_id = m_CacheAttrDB->blob_id;
                overflow = m_CacheAttrDB->overflow;
                volume_id = m_CacheAttrDB->volume_id;
                split_id = m_CacheAttrDB->split_id;

                ret = cur.Update();

            } else {
                return false;
            }
        }} // cursor
    }} // m_DB_Lock

    if (ret != eBDB_Ok) {
        return false;
    }
    _ASSERT(blob_id);
    trans.Commit();

    //  FIXME: locks, statistics, etc
    string owner_name;
    m_CacheAttrDB->owner_name.ToString(owner_name);
    if (IsSaveStatistics()) {
        CFastMutexGuard guard(m_DB_Lock);
        m_Statistics.AddRead(owner_name, curr - tz_delta);
    }

    if (overflow) {
        size_t bsize;
        auto_ptr<IReader> rd;
        rd.reset(
            x_CreateOverflowReader(key, version, subkey, bsize, blob_lock));
        return rd.release();
    }

    // Inline BLOB, reading from BDB storage
    if (blob_id == 0) {
        return 0;
    }

    unsigned coords[2];
    ret = m_BLOB_SplitStore->GetCoordinates(blob_id, coords);
    if (ret == eBDB_Ok) {
        if (coords[0] != volume_id ||
            coords[1] != split_id) {
            // TODO: restore de-mux mapping
        }
    } else {
        // TODO: restore de-mux mapping
    }

    coords[0] = volume_id;
    coords[1] = split_id;

    m_BLOB_SplitStore->SetTransaction(0);

    auto_ptr<CBDB_RawFile::TBuffer> buffer(new CBDB_RawFile::TBuffer(8 * 1024));
    ret = m_BLOB_SplitStore->ReadRealloc(blob_id, coords, *buffer);
    if (ret != eBDB_Ok) {
        return 0;
    }
    return new CBDB_CacheIReader(*this, buffer.release(), blob_lock);


/*
    {{
        CFastMutexGuard guard(m_DB_Lock);
        m_CacheAttrDB->SetTransaction(0);

        bool rec_exists =
            x_RetrieveBlobAttributes(key, version, subkey,
                                    overflow, ttl,
                                    blob_id, volume_id, split_id);
        if (!rec_exists) {
            return 0;
        }
        // check expiration
        if (m_TimeStampFlag & fCheckExpirationAlways) {
            if (x_CheckTimeStampExpired()) {
                return 0;
            }
        }

        m_CacheAttrDB->owner_name.ToString(m_TmpOwnerName);
        m_Statistics.AddRead(m_TmpOwnerName, curr - tz_delta);

        // Update the time stamp
        if ( m_TimeStampFlag & fTimeStampOnRead ) {
            CBDB_Transaction trans(*m_Env,
                                    CBDB_Transaction::eEnvDefault,
                                    CBDB_Transaction::eNoAssociation);
            m_CacheAttrDB->SetTransaction(&trans);

            x_UpdateReadAccessTime(key, version, subkey, trans);

            trans.Commit();
        }
    }} // m_DB_Lock



    // Check if it's an overflow BLOB (external file)
    {{
        size_t bsize;
        auto_ptr<IReader> rd;
        rd.reset(x_CreateOverflowReader(overflow, key, version, subkey, bsize));
        if (rd.get()) {
            if ( m_TimeStampFlag & fTimeStampOnRead ) {
                CBDB_Transaction trans(*m_Env,
                                        CBDB_Transaction::eEnvDefault,
                                        CBDB_Transaction::eNoAssociation);
                {{
                    CFastMutexGuard guard(m_DB_Lock);
                    x_UpdateReadAccessTime(key, version, subkey, trans);
                }} // m_DB_Lock
                trans.Commit();
            }
            return rd.release();
        }
    }}


    // Inline BLOB, reading from BDB storage
    if (blob_id == 0) {
        return 0;
    }

    unsigned coords[2];
    ret = m_BLOB_SplitStore->GetCoordinates(blob_id, coords);
    if (ret == eBDB_Ok) {
        if (coords[0] != volume_id ||
            coords[1] != split_id) {
            // TO DO: restore de-mux mapping
        }
    } else {
        // TODO: restore de-mux mapping
    }

    coords[0] = volume_id;
    coords[1] = split_id;

    m_BLOB_SplitStore->SetTransaction(0);

    auto_ptr<CBDB_RawFile::TBuffer> buffer(new CBDB_RawFile::TBuffer(4096));
    ret = m_BLOB_SplitStore->ReadRealloc(blob_id, coords, *buffer);
    if (ret != eBDB_Ok) {
        return 0;
    }
    return new CBDB_CacheIReader(*this, buffer.release());
*/
}

IReader* CBDB_Cache::GetReadStream(const string&  /* key */,
                                   const string&  /* subkey */,
                                   int*           /* version */,
                                   ICache::EBlobValidity* /* validity */)
{
    NCBI_USER_THROW("CBDB_Cache::GetReadStream("
        "key, subkey, &version, &validity) is not implemented");
}


void CBDB_Cache::SetBlobVersionAsValid(const string&  /* key */,
                                       const string&  /* subkey */,
                                       int            /* version */)
{
    NCBI_USER_THROW("CBDB_Cache::GetReadStream("
        "key, subkey, version) is not implemented");
}


void CBDB_Cache::GetBlobAccess(const string&     key,
                               int               version,
                               const string&     subkey,
                               SBlobAccessDescr* blob_descr)
{
    _ASSERT(blob_descr);

    blob_descr->reader.reset();
    blob_descr->blob_size = 0;
    blob_descr->blob_found = false;

    EBDB_ErrCode ret;

    time_t curr = time(0);
    int tz_delta = m_LocalTimer.GetLocalTimezone();
    int overflow = 0;
    unsigned volume_id = 0, split_id = 0;

    unsigned blob_id = GetBlobId(key, version, subkey);
    if (!blob_id) {
        //_TRACE("CBDB_Cache::GetBlobAccess return 1");
        return;
    }
    TBlobLock blob_lock(m_LockVector, blob_id, m_LockTimeout);


    // TODO: unify read prolog code for all read functions

    CBDB_Transaction trans(*m_Env,
                        CBDB_Transaction::eTransASync, // async!
                        CBDB_Transaction::eNoAssociation);

    {{
        CFastMutexGuard guard(m_DB_Lock);
        {{ // TODO: nested scope here is nonsensical
            m_CacheAttrDB->SetTransaction(&trans);

            CBDB_FileCursor cur(*m_CacheAttrDB, trans,
                                CBDB_FileCursor::eReadModifyUpdate);
            cur.SetCondition(CBDB_FileCursor::eEQ);
            cur.From << key << version << subkey;
            ret = cur.Fetch();
            if (ret == eBDB_Ok) {
                if (m_TimeStampFlag & fCheckExpirationAlways) {
                    if (x_CheckTimeStampExpired(*m_CacheAttrDB, curr)) {
                        //_TRACE("CBDB_Cache::GetBlobAccess return 2");
                        return;
                    }
                }
                unsigned read_count = m_CacheAttrDB->read_count;
                m_CacheAttrDB->read_count = read_count + 1;

                unsigned max_time = m_CacheAttrDB->max_time;
                if (max_time == 0 || max_time >= (unsigned) curr) {
                    if ( m_TimeStampFlag & fTimeStampOnRead ) {
                        m_CacheAttrDB->time_stamp = (unsigned)curr;
                    }
                }

                overflow = m_CacheAttrDB->overflow;
                volume_id = m_CacheAttrDB->volume_id;
                split_id = m_CacheAttrDB->split_id;

                ret = cur.Update();
            } else {
                //_TRACE("CBDB_Cache::GetBlobAccess return 3");
                return;
            }
        }} // cursor
    }} // m_DB_Lock

    if (ret != eBDB_Ok) {
        //_TRACE("CBDB_Cache::GetBlobAccess return 4");
        return;
    }
    _ASSERT(blob_id);
    trans.Commit();

    //  FIXME: locks, statistics, etc
    string owner_name;
    m_CacheAttrDB->owner_name.ToString(owner_name);
    if (IsSaveStatistics()) {
        CFastMutexGuard guard(m_DB_Lock);
        m_Statistics.AddRead(owner_name, curr - tz_delta);
    }


// Here we have a race(sic!) between overflow flag read/write and blob_id locking
// same is in all read functions!
// this race may affects coordinate mapping too (but it is auto-restored later)

    if (overflow) {
        blob_descr->reader.reset(
            x_CreateOverflowReader(key, version, subkey,
                                   blob_descr->blob_size, blob_lock));
        if (blob_descr->reader.get()) {
            blob_descr->blob_found = true;
            return;
        } else {
        }
    }

    // Inline BLOB, reading from BDB storage
    if (blob_id == 0) {
        //_TRACE("CBDB_Cache::GetBlobAccess return 5");
        return;
    }

    m_BLOB_SplitStore->SetTransaction(0);

    unsigned coords[2];
    ret = m_BLOB_SplitStore->GetCoordinates(blob_id, coords);
    if (ret == eBDB_Ok) {
        if (coords[0] != volume_id ||
            coords[1] != split_id) {
            // TO DO: restore de-mux mapping
        } else {
        // TODO: restore de-mux mapping
        }
    } else {
    }

    coords[0] = volume_id;
    coords[1] = split_id;

    if (blob_descr->buf && blob_descr->buf_size) {
        // use speculative read (hope provided buffer is sufficient)
        try {

            char** ptr = &blob_descr->buf;
            ret = m_BLOB_SplitStore->Fetch(blob_id, coords,
                                           (void**)ptr,
                                            blob_descr->buf_size,
                                            CBDB_RawFile::eReallocForbidden,
                                            &blob_descr->blob_size);
            if (ret == eBDB_Ok) {
                blob_descr->blob_found = true;
                return;
            } else {
            }
        } catch (CBDB_Exception&) {
        }
    }

    // speculative Fetch failed (or impossible), read it another way
    //
    // TODO: Add more realistic estimation of the BLOB size based on split
    //
    auto_ptr<CBDB_RawFile::TBuffer> buffer(
                        new CBDB_RawFile::TBuffer(8 * 1024));
    ret = m_BLOB_SplitStore->ReadRealloc(blob_id, coords, *buffer);
    if (ret != eBDB_Ok) {
        //_TRACE("CBDB_Cache::GetBlobAccess return 6");
        return;
    }
    blob_descr->blob_found = true;
    blob_descr->blob_size = buffer->size();
    blob_descr->reader.reset(
        new CBDB_CacheIReader(*this, buffer.release(), blob_lock));


/*

    EBDB_ErrCode ret;
    int overflow;
    unsigned int ttl, blob_id, volume_id, split_id;

    time_t curr = time(0);
    int tz_delta = m_LocalTimer.GetLocalTimezone();

    {{
        CBDB_Transaction trans(*m_Env,
                            CBDB_Transaction::eEnvDefault,
                            CBDB_Transaction::eNoAssociation);

        {{
        CFastMutexGuard guard(m_DB_Lock);
        m_CacheAttrDB->SetTransaction(0);

        bool rec_exists =
            x_RetrieveBlobAttributes(key, version, subkey,
                                    overflow, ttl,
                                    blob_id, volume_id, split_id);
        if (!rec_exists) {
            return;
        }
        // check expiration
        if (m_TimeStampFlag & fCheckExpirationAlways) {
            if (x_CheckTimeStampExpired()) {
                return;
            }
        }

        m_CacheAttrDB->owner_name.ToString(m_TmpOwnerName);
        m_Statistics.AddRead(m_TmpOwnerName, curr - tz_delta);

        // Update the time stamp
        if ( m_TimeStampFlag & fTimeStampOnRead ) {
            x_UpdateReadAccessTime(key, version, subkey, trans);
        }
        }} // m_DB_Lock

        trans.Commit();
    }}

    // Check if it's an overflow BLOB (external file)
    //
    blob_descr->reader.reset(
        x_CreateOverflowReader(overflow,
                               key, version, subkey,
                               blob_descr->blob_size));
    if (blob_descr->reader.get()) {
          blob_descr->blob_found = true;
          if ( m_TimeStampFlag & fTimeStampOnRead ) {
            CBDB_Transaction trans(*m_Env,
                                    CBDB_Transaction::eEnvDefault,
                                    CBDB_Transaction::eNoAssociation);
            {{
                CFastMutexGuard guard(m_DB_Lock);
                x_UpdateReadAccessTime(key, version, subkey, trans);
            }} // m_DB_Lock
            trans.Commit();
        }
        return;
    }


    // Inline BLOB, reading from BDB storage
    //
    if (blob_id == 0) {
        blob_descr->blob_found = false;
        return;
    }


    m_BLOB_SplitStore->SetTransaction(0);

    unsigned coords[2];
    ret = m_BLOB_SplitStore->GetCoordinates(blob_id, coords);
    if (ret == eBDB_Ok) {
        if (coords[0] != volume_id ||
            coords[1] != split_id) {
            // TO DO: restore de-mux mapping
        }
    } else {
        // TODO: restore de-mux mapping
    }

    coords[0] = volume_id;
    coords[1] = split_id;


    if (blob_descr->buf && blob_descr->buf_size) {
        // use speculative read (hope provided buffer is sufficient)
        try {

            char** ptr = &blob_descr->buf;
            ret = m_BLOB_SplitStore->Fetch(blob_id, coords,
                                           (void**)ptr,
                                            blob_descr->buf_size,
                                            CBDB_RawFile::eReallocForbidden,
                                            &blob_descr->blob_size);
            if (ret == eBDB_Ok) {
                blob_descr->blob_found = true;
                return;
            }
        } catch (CBDB_Exception&) {
        }
    }

    // speculative Fetch failed (or impossible), read it another way
    //
    // TODO: Add more realistic estimation of the BLOB size based on split
    //
    auto_ptr<CBDB_RawFile::TBuffer> buffer(new CBDB_RawFile::TBuffer(4096));
    ret = m_BLOB_SplitStore->ReadRealloc(blob_id, coords, *buffer);
    if (ret != eBDB_Ok) {
        blob_descr->blob_found = false;
        return;
    }
    blob_descr->blob_found = true;
    blob_descr->blob_size = buffer->size();
    blob_descr->reader.reset(
        new CBDB_CacheIReader(*this, buffer.release()));
*/
}

unsigned CBDB_Cache::GetBlobId(const string&  key,
                               int            version,
                               const string&  subkey)
{
    CFastMutexGuard guard(m_CARO1_Lock);

    m_CacheAttrDB_RO1->SetTransaction(0);

    m_CacheAttrDB_RO1->key = key;
    m_CacheAttrDB_RO1->version = version;
    m_CacheAttrDB_RO1->subkey = subkey;

    if (m_CacheAttrDB_RO1->Fetch() == eBDB_Ok) {
        return m_CacheAttrDB_RO1->blob_id;
    }
    return 0;
}


IWriter* CBDB_Cache::GetWriteStream(const string&    key,
                                    int              version,
                                    const string&    subkey,
                                    unsigned int     time_to_live,
                                    const string&    owner)
{
    return GetWriteStream(0, key, version, subkey, true /*lock id*/,
                          time_to_live, owner);
}


IWriter* CBDB_Cache::GetWriteStream(unsigned         blob_id_ext,
                                    const string&    key,
                                    int              version,
                                    const string&    subkey,
                                    bool             do_id_lock,
                                    unsigned int     time_to_live,
                                    const string&    owner)
{
    if (IsReadOnly()) {
        return 0;
    }

    //_TRACE("CBDB_Cache::GetWriteStream point 1");
    if (m_VersionFlag == eDropAll || m_VersionFlag == eDropOlder) {
        //_TRACE("CBDB_Cache::GetWriteStream point 2");
        Purge(key, subkey, 0, m_VersionFlag);
    }

    //_TRACE("CBDB_Cache::GetWriteStream point 3");
    if (m_MaxTimeout) {
        if (time_to_live > m_MaxTimeout) {
            time_to_live = m_MaxTimeout;
        }
    } else { // m_MaxTimeout == 0
        if (m_MaxTTL_prolong != 0 && m_Timeout != 0) {
            time_to_live = min(m_Timeout * m_MaxTTL_prolong, time_to_live);
        }
    }
    time_t curr = time(0);
    int tz_delta = m_LocalTimer.GetLocalTimezone();


    TBlobLock blob_lock(m_LockVector, m_LockTimeout);

    // ----------------------------------------------------
    // BLOB check-in, read attributes, lock the id
    // ----------------------------------------------------
    unsigned coord[2] = {0,};
    unsigned overflow;
    _VERIFY(
        BlobCheckIn(blob_id_ext,
                    key, version, subkey,
                    eBlobCheckIn_Create,
                    blob_lock, do_id_lock,
                    &coord[0], &coord[1],
                    &overflow)
            != EBlobCheckIn_NotFound);
    _ASSERT(blob_lock.GetId());


    return
        new CBDB_CacheIWriter(*this,
                              m_Path.c_str(),
                              blob_lock.GetId(),
                              key,
                              version,
                              subkey,
                              *m_CacheAttrDB,
                              time_to_live,
                              curr - tz_delta,
                              owner,
                              blob_lock);
}


void CBDB_Cache::Remove(const string& key)
{
    if (IsReadOnly()) {
        return;
    }

    vector<SCacheDescr>  cache_elements;

    // Search the records to delete

    {{
        CFastMutexGuard guard(m_DB_Lock);
        m_CacheAttrDB->SetTransaction(0);

        CBDB_FileCursor cur(*m_CacheAttrDB);
        cur.SetCondition(CBDB_FileCursor::eEQ);

        cur.From << key;
        while (cur.Fetch() == eBDB_Ok) {
            int version = m_CacheAttrDB->version;
            const char* subkey = m_CacheAttrDB->subkey;

            int overflow = m_CacheAttrDB->overflow;
            unsigned blob_id = m_CacheAttrDB->blob_id;

            cache_elements.push_back(
                SCacheDescr(key, version, subkey, overflow, blob_id));

            if (IsSaveStatistics()) {
                unsigned read_count = m_CacheAttrDB->read_count;
                string owner_name;
                m_CacheAttrDB->owner_name.ToString(owner_name);
                if (0 == read_count) {
                    m_Statistics.AddNeverRead(owner_name);
                }
                m_Statistics.AddExplDelete(owner_name);
                x_UpdateOwnerStatOnDelete(owner_name, true/*expl-delete*/);
            }
        }
    }} // m_DB_Lock

    {{
    CBDB_Transaction trans(*m_Env,
                        CBDB_Transaction::eEnvDefault,
                        CBDB_Transaction::eNoAssociation);

    // Now delete all objects

    ITERATE(vector<SCacheDescr>, it, cache_elements) {
        {{
            CFastMutexGuard guard(m_DB_Lock);
            m_BLOB_SplitStore->SetTransaction(&trans);

            x_DropBlob(it->key,
                       it->version,
                       it->subkey,
                       it->overflow,
                       it->blob_id,
                       trans);

        }} // m_DB_Lock

        {{
            CFastMutexGuard guard(m_TimeLine_Lock);
            m_TimeLine->RemoveObject(it->blob_id);
        }} // m_TimeLine_Lock

    }

    trans.Commit();
    }}

}

void CBDB_Cache::Remove(const string&    key,
                        int              version,
                        const string&    subkey)
{
    if (IsReadOnly()) {
        return;
    }

    // Search the records to delete

    vector<SCacheDescr>  cache_elements;

    {{
    CFastMutexGuard guard(m_DB_Lock);
    m_CacheAttrDB->SetTransaction(0);

    CBDB_FileCursor cur(*m_CacheAttrDB);
    cur.SetCondition(CBDB_FileCursor::eEQ);

    cur.From << key << version << subkey;
    while (cur.Fetch() == eBDB_Ok) {
        int overflow = m_CacheAttrDB->overflow;
        unsigned blob_id = m_CacheAttrDB->blob_id;

        cache_elements.push_back(
            SCacheDescr(key, version, subkey, overflow, blob_id));

        if (IsSaveStatistics()) {
            unsigned read_count = m_CacheAttrDB->read_count;
            string owner_name;
            m_CacheAttrDB->owner_name.ToString(owner_name);
            if (0 == read_count) {
                m_Statistics.AddNeverRead(owner_name);
            }
            m_Statistics.AddExplDelete(owner_name);
            x_UpdateOwnerStatOnDelete(owner_name, true/*expl-delete*/);
        }
    }

    }} // m_DB_Lock


    // TODO: add blob id locking here
    //
    CBDB_Transaction trans(*m_Env,
                            CBDB_Transaction::eEnvDefault,
                            CBDB_Transaction::eNoAssociation);

    ITERATE(vector<SCacheDescr>, it, cache_elements) {
        {{
            CFastMutexGuard guard(m_DB_Lock);
            m_BLOB_SplitStore->SetTransaction(&trans);

            x_DropBlob(it->key,
                       it->version,
                       it->subkey,
                       it->overflow,
                       it->blob_id,
                       trans);
        }} // m_DB_Lock

        {{
            CFastMutexGuard guard(m_TimeLine_Lock);
            m_TimeLine->RemoveObject(it->blob_id);
        }} // m_TimeLine_Lock
    }

    trans.Commit();
}


time_t CBDB_Cache::GetAccessTime(const string&  key,
                                 int            version,
                                 const string&  subkey)
{
    _ASSERT(m_CacheAttrDB);
    CFastMutexGuard guard(m_DB_Lock);
    m_CacheAttrDB->SetTransaction(0);

    m_CacheAttrDB->key = key;
    m_CacheAttrDB->version = version;
    m_CacheAttrDB->subkey = subkey;
        //(m_TimeStampFlag & fTrackSubKey) ? subkey : "";

    EBDB_ErrCode ret = m_CacheAttrDB->Fetch();
    if (ret != eBDB_Ok) {
        return 0;
    }

    return (int) m_CacheAttrDB->time_stamp;
}

void CBDB_Cache::SetPurgeBatchSize(unsigned batch_size)
{
    CFastMutexGuard guard(m_DB_Lock);
    m_PurgeBatchSize = batch_size;
}

unsigned CBDB_Cache::GetPurgeBatchSize() const
{
    CFastMutexGuard guard(m_DB_Lock);
    return m_PurgeBatchSize;
}

void CBDB_Cache::SetBatchSleep(unsigned sleep)
{
    CFastMutexGuard guard(m_DB_Lock);
    m_BatchSleep = sleep;
}

unsigned CBDB_Cache::GetBatchSleep() const
{
    CFastMutexGuard guard(m_DB_Lock);
    return m_BatchSleep;
}

void CBDB_Cache::StopPurge()
{
    CFastMutexGuard guard(m_DB_Lock);
    m_PurgeStopSignal.Post();
}


void CBDB_Cache::WriteOverflow(CNcbiOfstream& overflow_file,
                               const string&  overflow_file_path,
                               const char*    buf,
                               streamsize     count)
{
    overflow_file.write(buf, count);
    if (overflow_file.bad()) {

        overflow_file.close();

        string err("Overflow file IO error ");
        err += overflow_file_path;

        x_DropOverflow(overflow_file_path.c_str());

        BDB_THROW(eOverflowFileIO, err);
    }
}

void CBDB_Cache::AddToTimeLine(unsigned blob_id, time_t exp_time)
{
    {{
        CFastMutexGuard guard(m_TimeLine_Lock);
        m_TimeLine->AddObject(exp_time, blob_id);
    }}
}

/// @internal
///
/// Sets specified flag to FALSE on destruction
///
class CPurgeFlagGuard
{
public:
    CPurgeFlagGuard()
        : m_Flag(0)
    {}

    ~CPurgeFlagGuard()
    {
        if (m_Flag) {
            *m_Flag = false;
        }
    }

    void SetFlag(bool* flag)
    {
        m_Flag = flag;
        *m_Flag = true;
    }
private:
    bool*  m_Flag;
};


void CBDB_Cache::EvaluateTimeLine(bool* interrupted)
{
    time_t curr = time(0);

    if (m_LastTimeLineCheck) {
        if ((curr - m_LastTimeLineCheck) < (time_t)m_TimeLine->GetDiscrFactor()) {
            if (m_Monitor && m_Monitor->IsActive()) {
                string msg = "Purge: Timeline evaluation skiped ";
                msg += "(early wakeup for this precision) remains=";
                msg += NStr::ULongToString(
                        (unsigned long)(m_TimeLine->GetDiscrFactor() -
                                        (curr - m_LastTimeLineCheck)));
                msg += " precision=";
                msg += NStr::UIntToString(m_TimeLine->GetDiscrFactor());
                msg += "\n";
                m_Monitor->Send(msg);
            }

            return;
        }
    }
    m_LastTimeLineCheck = curr;

    TBitVector delete_candidates;
    {{
        CFastMutexGuard guard(m_TimeLine_Lock);
        m_TimeLine->ExtractObjects(curr, &delete_candidates);
        if (!delete_candidates.any()) {
            if (m_Monitor && m_Monitor->IsActive()) {
                string msg =
                    "Purge: Timeline evaluation exits "
                    "(no candidates) \n";
                m_Monitor->Send(msg);
            }

            return;
        }
    }}

    unsigned batch_size = m_PurgeBatchSize;
    if (batch_size < 10) {
        batch_size = 10;
    }

    unsigned candidates = delete_candidates.count();

    delete_candidates -= m_GC_Deleted;
    unsigned cleaned_candidates = delete_candidates.count();
    if (cleaned_candidates != candidates) {
        if (m_Monitor && m_Monitor->IsActive()) {
            string msg;
            msg = "Purge: Timeline total timeline candidates=";
            msg += NStr::UIntToString(candidates);
            msg += " cleaned_candidates=";
            msg += NStr::UIntToString(cleaned_candidates);
            msg += "\n";

            m_Monitor->Send(msg);
        }

        candidates = cleaned_candidates;
    }


    {{
    double id_trans_time = 0.0;  // id translation time
    double exp_check_time = 0.0; // expiration check time
    double del_time = 0.0;       // BLOB delete time
    unsigned deleted_cnt = 0;    // total number of deleted BLOBs

    vector<unsigned> blob_id_vect(batch_size);
    vector<SCacheDescr> blob_batch_vect(batch_size);
    vector<SCacheDescr> blob_exp_vect(batch_size);

    for (TBitVector::enumerator en = delete_candidates.first(); en.valid();) {

        // extract batch of blob ids out of the bit-vector
        //
        blob_id_vect.resize(0);
        blob_batch_vect.resize(0);
        blob_exp_vect.resize(0);
        for (unsigned i = 0; en.valid() && i < batch_size; ++i) {
            unsigned blob_id = *en;
            blob_id_vect.push_back(blob_id);
            ++en;
        } // for i

        // Translate IDs to keys
        //
        if (blob_id_vect.size()) {
            CStopWatch  sw(CStopWatch::eStart);
            vector<unsigned>::const_iterator it = blob_id_vect.begin();
            unsigned blob_id = *it;

            CFastMutexGuard guard(m_IDIDX_Lock_RO);
            m_CacheIdIDX_RO->SetTransaction(0);
            CBDB_FileCursor cur(*m_CacheIdIDX_RO);
            cur.SetCondition(CBDB_FileCursor::eGE);
            cur.From << blob_id;

            while (true) {
                if (cur.Fetch() == eBDB_Ok) {
                    unsigned bid = m_CacheIdIDX_RO->blob_id;
                    if (blob_id != bid) { // record has been deleted by now
                        if ((++it) == blob_id_vect.end())
                            break;
                        cur.SetCondition(CBDB_FileCursor::eGE);
                        cur.From << (blob_id = *it);

                    } else {
                        int version = m_CacheIdIDX_RO->version;
                        const char* key = m_CacheIdIDX_RO->key;
                        const char* subkey = m_CacheIdIDX_RO->subkey;
                        blob_batch_vect.push_back(
                            SCacheDescr(key, version, subkey, 0, blob_id));
                    }
                }
                ++it;
                if (it == blob_id_vect.end()) {
                    break;
                }
                // check if next blob can be get by fetch or we need to restart
                // the cursor
                //
                if ((blob_id + 1) == *it) {
                    ++blob_id;
                } else {
                    // cursor restart
                    //
                    cur.SetCondition(CBDB_FileCursor::eGE);
                    cur.From << (blob_id = *it);
                }
            } // while

            id_trans_time = sw.Elapsed();

        } // if

        // Check expiration of BLOB keys
        //
        if (blob_batch_vect.size()) {
            time_t curr = time(0); // initial timepoint
            CStopWatch  sw(CStopWatch::eStart);

            {{
            CFastMutexGuard guard(m_CARO2_Lock);

            m_CacheAttrDB_RO2->SetTransaction(0);
            CBDB_FileCursor cur(*m_CacheAttrDB_RO2);

            for (size_t i = 0; i < blob_batch_vect.size(); ++i) {
                SCacheDescr& blob_descr = blob_batch_vect[i];
                cur.SetCondition(CBDB_FileCursor::eEQ);
                cur.From << blob_descr.key
                         << blob_descr.version
                         << blob_descr.subkey;
                if (cur.Fetch() == eBDB_Ok) {
                    blob_descr.overflow = m_CacheAttrDB_RO2->overflow;
                    _ASSERT(blob_descr.blob_id == m_CacheAttrDB_RO2->blob_id);
                    blob_descr.blob_id = m_CacheAttrDB_RO2->blob_id;
                    time_t exp_time;
                    if (x_CheckTimeStampExpired(
                        *m_CacheAttrDB_RO2, curr, &exp_time)) {

                         blob_exp_vect.push_back(blob_descr);

                    } else {
                        {{
                            CFastMutexGuard guard(m_TimeLine_Lock);
                            m_TimeLine->AddObject(exp_time, blob_descr.blob_id);
                        }} // m_TimeLine_Lock
                    }
                }
            } // for
            }} // m_CARO2_Lock

            exp_check_time = sw.Elapsed();
        }

        unsigned deleted_batch_cnt = 0;
        // Delete the batch of expired BLOBs
        //
        if (blob_exp_vect.size()) {
            CStopWatch  sw(CStopWatch::eStart);
            for (size_t i = 0; i < blob_exp_vect.size(); ++i) {

                const SCacheDescr& blob_descr = blob_exp_vect[i];
                CBDB_Transaction trans(*m_Env,
                                       CBDB_Transaction::eTransASync, // async!
                                       CBDB_Transaction::eNoAssociation);

                if (m_Monitor && m_Monitor->IsActive()) {
                    string msg = GetFastLocalTime().AsString();
                    msg += " Purge: DELETING \"";
                    msg += blob_descr.key;
                    msg += "\"-";
                    msg += NStr::UIntToString(blob_descr.version);
                    msg += "-\"";
                    msg += blob_descr.subkey;
                    msg += "\"\n";
                    m_Monitor->Send(msg);
                }

                bool blob_deleted =
                    DropBlobWithExpCheck(blob_descr.key,
                                         blob_descr.version,
                                         blob_descr.subkey,
                                         trans);
                trans.Commit();
                deleted_batch_cnt += blob_deleted;
/*
                if (blob_deleted) {
                    if (m_Monitor && m_Monitor->IsActive()) {
                        string msg =
                            "Purge: Timeline DELETE blob =" + blob_descr.key
                            "\n";
                        m_Monitor->Send(msg);
                    }
                }
*/

            } // for i

            del_time = sw.Elapsed();
            deleted_cnt += deleted_batch_cnt;
        }

        if (m_Monitor && m_Monitor->IsActive()) {
            string msg;
            msg = "Purge: Timeline deleted_batch_cnt=";
            msg += NStr::UIntToString(deleted_batch_cnt);
            msg += " deleted_cnt=";
            msg += NStr::UIntToString(deleted_cnt);
            msg += " candidates=";
            msg += NStr::UIntToString(candidates);

            msg += " id_trans_time=";
            msg += NStr::DoubleToString(id_trans_time, 5);
            msg += " exp_check_time=";
            msg += NStr::DoubleToString(exp_check_time, 5);
            msg += " del_time=";
            msg += NStr::DoubleToString(del_time, 5);
            msg += "\n";

            m_Monitor->Send(msg);
        }

        m_GC_Deleted.optimize();

        // check for shutdown signal
        {{
            unsigned delay = m_BatchSleep * 1000000;
            bool scan_stop = m_PurgeStopSignal.TryWait(0, delay);
            if (scan_stop) {
                if (interrupted) {
                    *interrupted = true;
                    return;
                }
            }
        }}

    } // for en
    }}

/*
    for (; en.valid(); ++en) {
        unsigned blob_id = *en;
        try {
            CBDB_Transaction trans(*m_Env,
                                    CBDB_Transaction::eTransASync, // async!
                                    CBDB_Transaction::eNoAssociation);

            bool blob_deleted = DropBlobWithExpCheck(blob_id, trans);

            if (blob_deleted) {
                trans.Commit();
                ++deleted;
                if (m_Monitor && m_Monitor->IsActive()) {
                    string msg =
                        "Purge: Timeline DELETE blob id=" +
                        NStr::UIntToString(blob_id)+ "\n";
                    m_Monitor->Send(msg);
                }
            }

            // BLOB delete part of timeline can be quite expensive, lock
            // some resources needed by competing threads
            // so this code introduces delay to give way to others
            // ideally this delay should be adaptive dictated by current server load
            if (++cnt >= batch_size) {
                // get to nanoseconds as CSemaphore::TryWait() needs
                unsigned delay = m_BatchSleep * 1000000;
                bool scan_stop = m_PurgeStopSignal.TryWait(0, delay);
                if (scan_stop) {
                    if (interrupted) {
                        *interrupted = true;
                        return;
                    }
                }
                cnt = 0;
            }

        }
        catch (CBDB_ErrnoException& ex)
        {
            if (ex.IsRecovery()) {  // serious stuff! need to quit
                throw;
            }
            LOG_POST_X(20, Error
                <<  "Purge suppressed exception when deleting BLOB "
                << ex.what());
            if (m_Monitor && m_Monitor->IsActive()) {
                m_Monitor->Send(ex.what());
            }

        }
        catch(exception& ex) {
            LOG_POST_X(21, Error
                <<  "Purge suppressed exception when deleting BLOB "
                << ex.what());
            if (m_Monitor && m_Monitor->IsActive()) {
                m_Monitor->Send(ex.what());
            }
        }

    } // for en

    if (interrupted) {
        *interrupted = false;
    }

    if (m_Monitor && m_Monitor->IsActive()) {
        string msg =
            "Purge: Timeline deleted=" + NStr::UIntToString(deleted)
            + " out of candidates=" + NStr::UIntToString(candidates)
            + "\n";
        m_Monitor->Send(msg);
    }
*/
}

void CBDB_Cache::Purge(time_t           access_timeout,
                       EKeepVersions    keep_last_version)
{
    // Protect Purge form double (run from different threads)
    CPurgeFlagGuard pf_guard;
    {{
        CFastMutexGuard guard(m_DB_Lock);
        if (m_PurgeNowRunning)
            return;
        pf_guard.SetFlag(&m_PurgeNowRunning);
    }}


    if (IsReadOnly()) {
        return;
    }

    if (keep_last_version == eDropAll && access_timeout == 0) {
        CFastMutexGuard guard(m_DB_Lock);
        // TODO: SetTransaction here ??? Which one, zero?
        x_TruncateDB();
        return;
    }
purge_start:

    bool scan_interrupted = false;
    EvaluateTimeLine(&scan_interrupted);
    if (scan_interrupted) {
        return;
    }

    time_t gc_start = time(0); // time when we started GC scan

    if (m_NextExpTime) {

        // Mutiply to give new GC chance to kill first
        unsigned precision = m_TimeLine->GetDiscrFactor() * 3;

        //                                 [ precision ]
        //                                   |
        //                                   V
        // -----------*-------------------#<========>---------*-->> time
        //            *                   #                   ^
        //            *                   #                   *
        //           gc_start             { Next Expiration } *
        //            *                                       *
        //            *****************************************
        //
        // wait until current time goes after next projected expiration, plus
        // timeline precision, otherwise no need to do scanning
        //
        if (gc_start < (m_NextExpTime + (time_t)precision)) {
            if (m_Monitor && m_Monitor->IsActive()) {
                unsigned remains = (unsigned)((m_NextExpTime + precision) - gc_start);
                unsigned rc = 0;
                {{
                    CFastMutexGuard guard(m_CARO2_Lock);
                    m_CacheAttrDB_RO2->SetTransaction(0);
                    rc = m_CacheAttrDB_RO2->CountRecs();
                }}

                TSplitStore::TBitVector bv;
                m_BLOB_SplitStore->GetIdVector(&bv);
                unsigned split_store_blobs = bv.count();

                TBitVector timeline_blobs;
                {{
                    CFastMutexGuard guard(m_TimeLine_Lock);
                    m_TimeLine->EnumerateObjects(&timeline_blobs);
                }}
                unsigned tl_count = timeline_blobs.count();

                m_Monitor->Send(
                    "Purge: scan skipped(nothing todo) remains="
                    + NStr::UIntToString(remains)
                    + "s. precision=" + NStr::UIntToString(precision)
                    + "s. RecCount="  + NStr::UIntToString(rc)
                    + " SplitCount="+ NStr::UIntToString(split_store_blobs)
                    + " TimeLineCount="+ NStr::UIntToString(tl_count)
                    + "\n");
            }
            return;
        } else {
            m_NextExpTime = 0;
        }

/*
        if (m_PurgeSkipCnt >= 50) { // do not yeild Purge more than N times
            m_PurgeSkipCnt = 0;
        } else {
            // Check if we nothing to purge
            if (!(m_NextExpTime <= gc_start)) {
                ++m_PurgeSkipCnt;
                if (m_Monitor && m_Monitor->IsActive()) {
                    m_Monitor->Send("Purge: scan skipped(no delete candidates)\n");
                }
                return;
            }
        }
*/
    }

    if (m_Monitor && m_Monitor->IsActive()) {
        m_Monitor->Send("Purge: scan started.\n");
    }
    m_NextExpTime = 0;


    unsigned delay = 0;
    vector<SCacheDescr> cache_entries;
    cache_entries.reserve(1000);

    unsigned db_recs = 0;


    // Search the database for obsolete cache entries
    string first_key, last_key;
    string next_exp_key;

    for (bool flag = true; flag;) {
        unsigned batch_size = GetPurgeBatchSize();
        unsigned rec_cnt;
        {{
            time_t curr = time(0); // initial timepoint

            CFastMutexGuard guard(m_CARO2_Lock);
            // get to nanoseconds as CSemaphore::TryWait() needs
            delay = m_BatchSleep * 1000000;

            m_CacheAttrDB_RO2->SetTransaction(0);

            CBDB_FileCursor cur(*m_CacheAttrDB_RO2);
            cur.SetCondition(CBDB_FileCursor::eGE);
            cur.From << last_key;

            for (rec_cnt = 0; rec_cnt < batch_size; ++rec_cnt) {
                if (cur.Fetch() != eBDB_Ok) {
                    flag = false;
                    break;
                }

                int version = m_CacheAttrDB_RO2->version;
                const char* key = m_CacheAttrDB_RO2->key;
                last_key = key;
                int overflow = m_CacheAttrDB_RO2->overflow;
                const char* subkey = m_CacheAttrDB_RO2->subkey;
                unsigned blob_id = m_CacheAttrDB_RO2->blob_id;

                time_t exp_time;
                if (x_CheckTimeStampExpired(*m_CacheAttrDB_RO2, curr, &exp_time)) {

                    string owner_name;
                    m_CacheAttrDB_RO2->owner_name.ToString(owner_name);

                    // FIXME: statistics, locking, etc
                    if (IsSaveStatistics()) {
                        CFastMutexGuard guard(m_DB_Lock);
                        if (0 == m_CacheAttrDB_RO2->read_count) {
                            m_Statistics.AddNeverRead(owner_name);
                        }
                        m_Statistics.AddPurgeDelete(owner_name);
                        x_UpdateOwnerStatOnDelete(owner_name,
                                                  false//non-expl-delete
                                                  );
                    }

                    cache_entries.push_back(
                         SCacheDescr(key, version, subkey, overflow, blob_id));
                } else {
                    ++db_recs;

                    {{
                        CFastMutexGuard guard(m_TimeLine_Lock);
                        m_TimeLine->AddObject(exp_time, blob_id);
                    }} // m_TimeLine_Lock

                    if (m_NextExpTime) {
                        if (exp_time < m_NextExpTime) {
                            // compute fraction of the difference
                            // with sensitivity based on purge thread delay
                            // we need this in order not to move point of restart
                            // too agressively

                            double delta = fabs(double(m_NextExpTime - exp_time));
                            double fraction = fabs(double(delta / m_NextExpTime));
                            if (fraction > 0.1 && delta > m_PurgeThreadDelay) {
                                next_exp_key = key;
                                m_NextExpTime = exp_time;
                            }
                        }
                    } else {
                        m_NextExpTime = exp_time;
                        next_exp_key = key;
                    }


                }
                if (rec_cnt == 0) { // first record in the batch
                    first_key = last_key;
                } else
                if (rec_cnt == (batch_size - 1)) { // last record
                    // if batch stops in the same key we increase
                    // the batch size to avoid the infinite loop
                    // when new batch starts with the very same key
                    // and never comes to the end
                    //
                    if (first_key == last_key) {
                        batch_size += 10;
                    }
                }

            } // for i
        }} // m_CARO2_Lock

        if (m_Monitor && m_Monitor->IsActive()) {
            string msg("Purge: Inspected ");
            msg += NStr::UIntToString(rec_cnt);
            msg += " records.\n";

            m_Monitor->Send(msg);
        }


        // Delete BLOBs if there are deletion candidates
        if (cache_entries.size() > 0) {
            if (m_Monitor && m_Monitor->IsActive()) {
                m_Monitor->Send("Purge:Deleting expired BLOBs...\n");
            }

            for (size_t i = 0; i < cache_entries.size(); ++i) {
                try {
                    const SCacheDescr& it = cache_entries[i];

                    CBDB_Transaction trans(*m_Env,
                                            CBDB_Transaction::eTransASync, // async!
                                            CBDB_Transaction::eNoAssociation);

                    if (m_Monitor && m_Monitor->IsActive()) {
                        string msg = GetFastLocalTime().AsString();
                        msg += " Purge: DELETING \"";
                        msg += it.key;
                        msg += "\"-";
                        msg += NStr::UIntToString(it.version);
                        msg += "-\"";
                        msg += it.subkey;
                        msg += "\"\n";
                        m_Monitor->Send(msg);
                    }

                    {{
                        DropBlobWithExpCheck(it.key,
                                             it.version,
                                             it.subkey,
                                             trans);
                    }}

                    /*
                    {{
                        CFastMutexGuard guard(m_DB_Lock);
                        m_BLOB_SplitStore->SetTransaction(&trans);

                        x_DropBlob(it.key,
                                   it.version,
                                   it.subkey,
                                   it.overflow,
                                   it.blob_id,
                                   trans);

                    }} // m_DB_Lock
                    */
                    trans.Commit();
                }
                catch (CBDB_ErrnoException& ex)
                {
                    if (ex.IsRecovery()) {  // serious stuff! need to quit
                        throw;
                    }
                    LOG_POST_X(22, Error
                        <<  "Purge suppressed exception when deleting BLOB "
                        << ex.what());
                    if (m_Monitor && m_Monitor->IsActive()) {
                        m_Monitor->Send(ex.what());
                    }

                }
                catch(exception& ex) {
                    LOG_POST_X(23, Error
                        <<  "Purge suppressed exception when deleting BLOB "
                        << ex.what());
                    if (m_Monitor && m_Monitor->IsActive()) {
                        m_Monitor->Send(ex.what());
                    }
                }

            } // for i

            if (m_Monitor && m_Monitor->IsActive()) {
                string msg =
                    "Purge: deleted " +
                    NStr::SizetToString(cache_entries.size()) +
                    " BLOBs\n";
                m_Monitor->Send(msg);
            }

            cache_entries.resize(0);
        }

        bool purge_stop = m_PurgeStopSignal.TryWait(0, delay);
        if (purge_stop) {
            LOG_POST_X(24, Info << "BDB Cache: Stopping Purge execution.");
            return;
        }

        EvaluateTimeLine(&scan_interrupted);
        if (scan_interrupted) {
            LOG_POST_X(25, Warning << "BDB Cache: Stopping Purge execution.");
            return;
        }


    } // for flag


    ++m_PurgeCount;

    if (m_CleanLogOnPurge) {
        if ((m_PurgeCount % m_CleanLogOnPurge) == 0) {
            m_Env->CleanLog();
        }
    }

    if (IsSaveStatistics()) {
        CFastMutexGuard guard(m_DB_Lock);
        m_Statistics.GlobalStatistics().blobs_db = db_recs;
    }

    if ((m_PurgeCount % 200) == 0) {
        m_BLOB_SplitStore->Save();
        m_Env->ForceTransactionCheckpoint();
    }
    else
    if ((m_PurgeCount % 50) == 0) {
        m_BLOB_SplitStore->FreeUnusedMem();
        m_LockVector.FreeUnusedMem();
    }

    unsigned time_in_purge = 0;

    // check if we want to rescan the database right away
    if (m_PurgeThreadDelay) {
        time_t curr = time(0);
        // we spent more time in Purge than we planned to sleep
        // it means we have a lot of records and need to run GC
        // continuosly
        time_in_purge = (unsigned)curr - (unsigned)gc_start;
        if (time_in_purge >= m_PurgeThreadDelay ||
            (m_NextExpTime != 0 && (curr > m_NextExpTime))) {

            if (m_Monitor && m_Monitor->IsActive()) {
                m_Monitor->Send("Purge: scan restarted. time=" +
                                NStr::UIntToString(time_in_purge)+ "s.\n");
            }

            goto purge_start;
        }
    }

    if (m_Monitor && m_Monitor->IsActive()) {
        m_Monitor->Send("Purge: scan ended. Scan time=" +
                        NStr::UIntToString(time_in_purge)+ "s.\n");
    }

}


void CBDB_Cache::Purge(const string&    key,
                       const string&    subkey,
                       time_t           access_timeout,
                       EKeepVersions    keep_last_version)
{
    if (IsReadOnly()) {
        return;
    }

    if (key.empty() && keep_last_version == eDropAll && access_timeout == 0) {
        CFastMutexGuard guard(m_DB_Lock);
        // TODO: SetTransaction ??? see TODO above
        x_TruncateDB();
        return;
    }

    // Search the database for obsolete cache entries
    vector<SCacheDescr> cache_entries;

    unsigned recs_scanned = 0;

    {{
    CFastMutexGuard guard(m_DB_Lock);
    m_CacheAttrDB->SetTransaction(0);

    CBDB_FileCursor cur(*m_CacheAttrDB);
    cur.SetCondition(CBDB_FileCursor::eEQ);

    cur.From << key;

    //CTime time_stamp(CTime::eCurrent);
    time_t curr = time(0); // (int)time_stamp.GetTimeT();
    int timeout = GetTimeout();
    if (access_timeout && access_timeout < timeout) {
        timeout = (int)access_timeout;
    }

    while (cur.Fetch() == eBDB_Ok) {
        ++recs_scanned;

        unsigned    db_time_stamp = m_CacheAttrDB->time_stamp;
        int         version       = m_CacheAttrDB->version;
        const char* x_key         = m_CacheAttrDB->key;
        int         overflow      = m_CacheAttrDB->overflow;
        string      x_subkey      = (const char*) m_CacheAttrDB->subkey;
        unsigned    blob_id       = m_CacheAttrDB->blob_id;

        unsigned ttl = m_CacheAttrDB->ttl;
        unsigned to = timeout;

        if (ttl) {  // individual timeout
            if (m_MaxTimeout && ttl > m_MaxTimeout) {
                to = max((unsigned)timeout, (unsigned)m_MaxTimeout);
            } else {
                to = ttl;
            }
        }

        if (subkey.empty()) {
        }

        // check if timeout control has been requested and stored element
        // should not be removed
        if (access_timeout && (!(curr - to > db_time_stamp))) {
            continue;
        }

        if (subkey.empty() || (subkey == x_subkey)) {
            cache_entries.push_back(
                     SCacheDescr(x_key, version, x_subkey, overflow, blob_id));

            if (IsSaveStatistics()) {
                unsigned read_count = m_CacheAttrDB->read_count;
                string owner_name;
                m_CacheAttrDB->owner_name.ToString(owner_name);
                if (0 == read_count) {
                    m_Statistics.AddNeverRead(owner_name);
                }
                m_Statistics.AddPurgeDelete(owner_name);
                x_UpdateOwnerStatOnDelete(owner_name,
                                          false/*non-expl-delete*/);
            }


            continue;
        }
    } // while

    }} // m_DB_Lock


    ITERATE(vector<SCacheDescr>, it, cache_entries) {
        CBDB_Transaction trans(*m_Env,
                            CBDB_Transaction::eEnvDefault,
                            CBDB_Transaction::eNoAssociation);
        {{
            CFastMutexGuard guard(m_DB_Lock);
            m_BLOB_SplitStore->SetTransaction(&trans);
            x_DropBlob(it->key,
                       it->version,
                       it->subkey,
                       it->overflow,
                       it->blob_id,
                       trans);
        }} // m_DB_Lock

        trans.Commit();
    }
}

void CBDB_Cache::Verify(const string&  cache_path,
                        const string&  cache_name,
                        const string&  err_file,
                        bool           force_remove)
{
    Close();

    m_Path = CDirEntry::AddTrailingPathSeparator(cache_path);

    m_Env = new CBDB_Env();

    m_Env->SetCacheSize(10 * 1024 * 1024);
    m_Env->OpenErrFile(!err_file.empty() ? err_file : string("stderr"));
    try {
        m_Env->Open(cache_path, DB_INIT_MPOOL | DB_USE_ENVIRON);
    }
    catch (CBDB_Exception& /*ex*/)
    {
        m_Env->Open(cache_path,
                    DB_CREATE | DB_INIT_MPOOL | DB_PRIVATE | DB_USE_ENVIRON);
    }

    LOG_POST_X(26, "Cache location: " + string(cache_path));

    string cache_blob_db_name =
       string("lcs_") + string(cache_name) + string("_blob")+ string(".db");
    string attr_db_name =
       string("lcs_") + string(cache_name) + string("_attr5") + string(".db");

    m_CacheAttrDB = new SCache_AttrDB();
    m_CacheAttrDB->SetEnv(*m_Env);

    string bak = m_Path + attr_db_name + ".bak";
    FILE* fl = fopen(bak.c_str(), "wb");

    LOG_POST_X(27, "Running verification for: " + attr_db_name);
    m_CacheAttrDB->Verify(attr_db_name.c_str(), 0, fl);
    delete m_CacheAttrDB; m_CacheAttrDB = 0;

    fclose(fl);

    if (force_remove) {
        m_Env->ForceRemove();
    } else {
        bool deleted = m_Env->Remove();

        if (!deleted)
            LOG_POST_X(28, "Cannot delete the environment (it is busy by another process)");
    }
    delete m_Env; m_Env = 0;
}

void CBDB_Cache::x_PerformCheckPointNoLock(unsigned bytes_written)
{
    if (m_RunPurgeThread == false) {
        m_Env->TransactionCheckpoint();
    }
}

time_t
CBDB_Cache::x_ComputeExpTime(int time_stamp, unsigned ttl, int timeout)
{
    time_t exp_time;
    if (ttl) {  // individual timeout
        if (m_MaxTimeout && ttl > m_MaxTimeout) {
            timeout =
                (int) max((unsigned)timeout, (unsigned)m_MaxTimeout);
            ttl = timeout; // used in diagnostics only
        } else {
            timeout = ttl;
        }
    }

    // predicted job expiration time
    exp_time = time_stamp + timeout;
    return exp_time;
}

bool CBDB_Cache::x_CheckTimeStampExpired(SCache_AttrDB& attr_db,
                                         time_t  curr,
                                         time_t* exp_time)
{
    int timeout = GetTimeout();

    if (timeout) {

        int db_time_stamp = attr_db.time_stamp;
        unsigned int ttl = attr_db.ttl;

        if (ttl) {  // individual timeout
            if (m_MaxTimeout && ttl > m_MaxTimeout) {
                timeout =
                    (int) max((unsigned)timeout, (unsigned)m_MaxTimeout);
                ttl = timeout; // used in diagnostics only
            } else {
                timeout = ttl;
            }
        }

        // predicted job expiration time
        if (exp_time) {
            *exp_time = db_time_stamp + timeout;
        }

        if (curr - timeout > db_time_stamp) {
            return true;
        }
    }
    return false;
}


void CBDB_Cache::x_UpdateAccessTime(const string&  key,
                                    int            version,
                                    const string&  subkey,
                                    EBlobAccessType access_type,
                                    CBDB_Transaction& trans)
{
    if (IsReadOnly()) {
        return;
    }

    unsigned   timeout = (unsigned)time(0);
    {{
        CBDB_FileCursor cur(*m_CacheAttrDB, trans,
                            CBDB_FileCursor::eReadModifyUpdate);

        cur.SetCondition(CBDB_FileCursor::eEQ);
        cur.From << key << version;

        if (cur.Fetch() == eBDB_Ok) {
            unsigned old_ts = m_CacheAttrDB->time_stamp;
            if (old_ts < timeout) {
                unsigned max_time = m_CacheAttrDB->max_time;
                if (max_time == 0 || max_time >= timeout) {
                    m_CacheAttrDB->time_stamp = timeout;

                    switch (access_type) {
                    case eBlobRead:
                        {
                        unsigned read_count = m_CacheAttrDB->read_count;
                        ++read_count;
                        m_CacheAttrDB->read_count = read_count;
                        }
                        break;
                    case eBlobUpdate:
                        {
                        unsigned upd_count = m_CacheAttrDB->upd_count;
                        ++upd_count;
                        m_CacheAttrDB->upd_count = upd_count;
                        }
                        break;
                    case eBlobStore:
                        break;
                    default:
                        _ASSERT(0);
                    }

                    cur.Update();
                }
            }
        } // if

    }} // cursor
}

void CBDB_Cache::x_TruncateDB()
{
    if (IsReadOnly()) {
        return;
    }

    // TODO: optimization of store truncate
    {{
    CBDB_FileCursor cur(*m_CacheAttrDB);
    cur.SetCondition(CBDB_FileCursor::eFirst);
    while (cur.Fetch() == eBDB_Ok) {
        unsigned blob_id, volume_id, split_id;
        blob_id = m_CacheAttrDB->blob_id;
        volume_id = m_CacheAttrDB->volume_id;
        split_id = m_CacheAttrDB->split_id;

        if (blob_id) {
            unsigned coords[2] = { volume_id, split_id };
            m_BLOB_SplitStore->SetTransaction(0);
            m_BLOB_SplitStore->Delete(blob_id, coords);
        }
    } // while

    }}

    m_BLOB_SplitStore->Save();


    LOG_POST_X(29, Info << "CBDB_BLOB_Cache:: cache database truncated");
    m_CacheAttrDB->Truncate();

    CDir dir(m_Path);
    CMaskFileName mask;
    mask.Add(this->GetName() + "_*.ov_");

    string ext;
    string ov_(".ov_");
    if (dir.Exists()) {
        CDir::TEntries  content(dir.GetEntries(mask));
        ITERATE(CDir::TEntries, it, content) {
            if ( (*it)->IsFile() ) {
                ext = (*it)->GetExt();
                if (ext == ov_) {
                    (*it)->Remove();
                }
            }
        }
    }
}


bool CBDB_Cache::x_RetrieveBlobAttributes(const string&  key,
                                          int            version,
                                          const string&  subkey,
                                          int&           overflow,
                                          unsigned int&  ttl,
                                          unsigned int&  blob_id,
                                          unsigned int&  volume_id,
                                          unsigned int&  split_id)
{
    if (!x_FetchBlobAttributes(key, version, subkey)) return false;

    overflow  = m_CacheAttrDB->overflow;
    ttl       = m_CacheAttrDB->ttl;
    blob_id   = m_CacheAttrDB->blob_id;
    volume_id = m_CacheAttrDB->volume_id;
    split_id  = m_CacheAttrDB->split_id;

/*
    if (!(m_TimeStampFlag & fTrackSubKey)) {
        m_CacheAttrDB->subkey = "";

        EBDB_ErrCode err = m_CacheAttrDB->Fetch();
        if (err != eBDB_Ok) {
            return false;
        }
    }
*/
    return true;
}

bool CBDB_Cache::x_FetchBlobAttributes(const string&  key,
                                       int            version,
                                       const string&  subkey)
{
    m_CacheAttrDB->key = key;
    m_CacheAttrDB->version = version;
    m_CacheAttrDB->subkey = subkey;

    EBDB_ErrCode ret = m_CacheAttrDB->Fetch();
    if (ret != eBDB_Ok) {
        return false;
    }
    return true;
}



void CBDB_Cache::x_DropOverflow(const string&  key,
                                int            version,
                                const string&  subkey)
{
    string path;
    try {
        s_MakeOverflowFileName(path, m_Path, GetName(), key, version, subkey);
        x_DropOverflow(path);
    } catch (exception& ex) {
        ERR_POST_X(30, "Blob Store: Cannot remove file: " << path
                   << " " << ex.what());
    }
}

void CBDB_Cache::x_DropOverflow(const string&  file_path)
{
    try {
        CDirEntry entry(file_path);
        if (entry.Exists()) {
            entry.Remove();
        }
    } catch (exception& ex) {
        ERR_POST_X(31, "Blob Store: Cannot remove file: " << file_path
                   << " " << ex.what());
    }
}

unsigned CBDB_Cache::GetNextBlobId(bool lock_id)
{
    unsigned blob_id = m_BlobIdCounter.Add(1);
    if (blob_id >= kMax_UInt) {
        m_BlobIdCounter.Set(0);
        blob_id = m_BlobIdCounter.Add(1);
        m_GC_Deleted.clear();
    }
    if (lock_id) {
        bool locked = m_LockVector.TryLock(blob_id);
        if (!locked) {
            BDB_THROW(eInvalidOperation, "Cannot lock new BLOB ID");
        }
    }
    return blob_id;
}

bool CBDB_Cache::DropBlobWithExpCheck(unsigned           blob_id,
                                      CBDB_Transaction&  trans)
{
    string key, subkey;
    int version;
    EBDB_ErrCode ret;
    {{
        CFastMutexGuard guard(m_IDIDX_Lock_RO);

        m_CacheIdIDX_RO->blob_id = blob_id;
        ret = m_CacheIdIDX_RO->Fetch();
        if (ret == eBDB_Ok) {
            key = m_CacheIdIDX_RO->key;
            version = m_CacheIdIDX_RO->version;
            subkey = m_CacheIdIDX_RO->subkey;
        }
    }} // m_IDIDX_Lock_RO

    if (ret == eBDB_Ok) {
        return DropBlobWithExpCheck(key, version, subkey, trans);
    }
    return false;
}



bool CBDB_Cache::DropBlobWithExpCheck(const string&      key,
                                      int                version,
                                      const string&      subkey,
                                      CBDB_Transaction&  trans)
{
    if (IsReadOnly()) {
        return false;
    }

    time_t curr = time(0);

    unsigned coords[2] = {0,};
    unsigned split_coord[2] = {0,};
    int overflow;
    unsigned  blob_id;

    bool blob_expired = false;
    time_t exp_time;

    {{
        CFastMutexGuard guard(m_CARO2_Lock);

        m_CacheAttrDB_RO2->SetTransaction(0);

        m_CacheAttrDB_RO2->key = key;
        m_CacheAttrDB_RO2->version = version;
        m_CacheAttrDB_RO2->subkey = subkey;
        if (m_CacheAttrDB_RO2->Fetch() != eBDB_Ok) {
            return false;
        }

        if (x_CheckTimeStampExpired(*m_CacheAttrDB_RO2, curr, &exp_time)) {
            blob_expired = true;
            overflow  = m_CacheAttrDB_RO2->overflow;
            coords[0] = m_CacheAttrDB_RO2->volume_id;
            coords[1] = m_CacheAttrDB_RO2->split_id;
            blob_id   = m_CacheAttrDB_RO2->blob_id;
        } else {
            blob_expired = false;
            blob_id   = m_CacheAttrDB_RO2->blob_id;
        }

    }} // m_CARO2_Lock

    if (!blob_expired) {
        {{
            CFastMutexGuard guard(m_TimeLine_Lock);
            _ASSERT(exp_time);
            m_TimeLine->AddObject(exp_time, blob_id);
        }} // m_TimeLine_Lock
        return false;
    }

    // BLOB expired and needs to be deleted
    //


    // Delete the overflow file
    //
    if (overflow == 1) {
        x_DropOverflow(key, version, subkey);
    }

    // Important implementation note:
    // There is a temptation to delete BLOB split store first
    // and registration record second to minimize time registration record
    // stays locked. We should NOT do it in ONE transaction
    // because in other places order of access is different and this may
    // create DEADLOCK

    // Delete BLOB attributes and index
    //
    {{
        CFastMutexGuard guard(m_DB_Lock);
        m_CacheAttrDB->SetTransaction(&trans);

        m_CacheAttrDB->key     = key;
        m_CacheAttrDB->version = version;
        m_CacheAttrDB->subkey  = subkey;
        if (m_CacheAttrDB->Fetch() != eBDB_Ok) {
            return false;
        }
        m_CacheAttrDB->Delete(CBDB_RawFile::eThrowOnError);

        m_CacheIdIDX->SetTransaction(&trans);
        m_CacheIdIDX->blob_id = blob_id;
        m_CacheIdIDX->Delete(CBDB_RawFile::eThrowOnError);

    }} // m_DB_Lock

    // Delete split store
    //
    EBDB_ErrCode ret =
        m_BLOB_SplitStore->GetCoordinates(blob_id, split_coord);
    m_BLOB_SplitStore->SetTransaction(&trans);
    if (ret == eBDB_Ok) {
        if (split_coord[0] != coords[0] ||
            split_coord[1] != coords[1]) {
            // split coords un-sync: delete de-facto BLOB
            m_BLOB_SplitStore->Delete(blob_id,
                                      CBDB_RawFile::eThrowOnError);
        }
    }
    // Delete BLOB by known coordinates
    m_BLOB_SplitStore->Delete(blob_id, coords,
                              CBDB_RawFile::eThrowOnError);

    m_GC_Deleted.set(blob_id);

    return true;
}



void CBDB_Cache::x_DropBlob(const string&      key,
                            int                version,
                            const string&      subkey,
                            int                overflow,
                            unsigned           blob_id,
                            CBDB_Transaction&  trans)
{
    if (IsReadOnly()) {
        return;
    }

    if (overflow == 1) {
        x_DropOverflow(key, version, subkey);
    }

    unsigned coords[2] = {0,};
    unsigned split_coord[2] = {0,};

    if (blob_id) {
        bool delete_split = false;
        {{
        //m_CacheAttrDB->SetTransaction(0);

        CBDB_FileCursor cur(*m_CacheAttrDB);
        cur.SetCondition(CBDB_FileCursor::eEQ);
        cur.From << key << version << subkey;
        if (cur.Fetch() == eBDB_Ok) {
            coords[0] = m_CacheAttrDB->volume_id;
            coords[1] = m_CacheAttrDB->split_id;

            EBDB_ErrCode ret =
                m_BLOB_SplitStore->GetCoordinates(blob_id, split_coord);
            if (ret == eBDB_Ok) {
                if (split_coord[0] != coords[0] ||
                    split_coord[1] != coords[1]) {
                    delete_split = true;
                }
            }

        }
        }}

        // delete blob as pointed by de-mux splitter
        if (delete_split) {
            m_BLOB_SplitStore->Delete(blob_id,
                                      CBDB_RawFile::eThrowOnError);
        }
        // delete blob as accounted by meta-information
        m_BLOB_SplitStore->Delete(blob_id, coords,
                                  CBDB_RawFile::eThrowOnError);

    }
    m_CacheAttrDB->SetTransaction(&trans);

    m_CacheAttrDB->key = key;
    m_CacheAttrDB->version = version;
    m_CacheAttrDB->subkey = subkey;

    m_CacheAttrDB->Delete(CBDB_RawFile::eThrowOnError);

    m_CacheIdIDX->SetTransaction(&trans);

    m_CacheIdIDX->blob_id = blob_id;
    m_CacheIdIDX->Delete(CBDB_RawFile::eThrowOnError);
}

bool CBDB_Cache::IsLocked(unsigned blob_id)
{
    return m_LockVector.IsLocked(blob_id);
}

bool CBDB_Cache::IsLocked(const string&  key,
                          int            version,
                          const string&  subkey)
{
    unsigned blob_id = GetBlobId(key, version, subkey);
    if (!blob_id) return false;
    return IsLocked(blob_id);
}


CBDB_Cache::EBlobCheckinRes
CBDB_Cache::BlobCheckIn(unsigned         blob_id_ext,
                        const string&    key,
                        int              version,
                        const string&    subkey,
                        EBlobCheckinMode mode,
                        TBlobLock&       blob_lock,
                        bool             do_id_lock,
                        unsigned*        volume_id,
                        unsigned*        split_id,
                        unsigned*        overflow)
{
    _ASSERT(volume_id);
    _ASSERT(split_id);
    _ASSERT(overflow);

    EBDB_ErrCode ret;
    unsigned blob_id = 0;

    while (blob_id == 0) {
        {{
            CFastMutexGuard guard(m_DB_Lock);
            m_CacheAttrDB->SetTransaction(0);

            CBDB_FileCursor cur(*m_CacheAttrDB);
            cur.SetCondition(CBDB_FileCursor::eEQ);
            cur.From << key << version << subkey;

            ret = cur.Fetch();
            if (ret == eBDB_Ok) {
                blob_id = m_CacheAttrDB->blob_id;
                *volume_id = m_CacheAttrDB->volume_id;
                *split_id = m_CacheAttrDB->split_id;
                *overflow = m_CacheAttrDB->overflow;
            }
        }} // m_DB_Lock

        if (ret == eBDB_Ok) {
            _ASSERT(blob_id);
            if (do_id_lock) {
                blob_lock.Lock(blob_id);
            } else {
                blob_lock.SetId(blob_id);
            }
            return eBlobCheckIn_Found;
        }

        *overflow = *volume_id = *split_id = 0;

        switch (mode) {
        case eBlobCheckIn:
            break;
        case eBlobCheckIn_Create:
            {
            if (blob_id_ext == 0) {
                blob_id = GetNextBlobId(false/*do not lock*/);
            } else {
                blob_id = blob_id_ext;
            }

            CBDB_Transaction trans(*m_Env,
                                CBDB_Transaction::eTransASync,  // async!
                                CBDB_Transaction::eNoAssociation);

            // create registration record: use temp magic values
            // (and overide some later)
            //
            {{
                CFastMutexGuard guard(m_DB_Lock);
                m_CacheAttrDB->SetTransaction(&trans);

                m_CacheAttrDB->key = key;
                m_CacheAttrDB->version = version;
                m_CacheAttrDB->subkey = subkey;
                m_CacheAttrDB->time_stamp = Uint4(time(0)+1);
                m_CacheAttrDB->overflow = 0;
                m_CacheAttrDB->ttl = 77;
                m_CacheAttrDB->max_time = 77;
                m_CacheAttrDB->upd_count  = 0;
                m_CacheAttrDB->read_count = 0;
                m_CacheAttrDB->owner_name = "BDB_cache";
                m_CacheAttrDB->blob_id = blob_id;
                m_CacheAttrDB->volume_id = 0;
                m_CacheAttrDB->split_id = 0;

                ret = m_CacheAttrDB->Insert();

                if (ret == eBDB_Ok) {
                    m_CacheIdIDX->SetTransaction(&trans);

                    m_CacheIdIDX->blob_id = blob_id;
                    m_CacheIdIDX->key = key;
                    m_CacheIdIDX->version = version;
                    m_CacheIdIDX->subkey = subkey;

                    ret = m_CacheIdIDX->Insert();
                    if (ret == eBDB_Ok) {
                        trans.Commit();
                        if (do_id_lock) {
                            blob_lock.Lock(blob_id);
                        } else {
                            blob_lock.SetId(blob_id);
                        }
                        return eBlobCheckIn_Created;
                    } else {
                        BDB_THROW(eInvalidOperation,
                                  "Cannot update blob id index");
                    }
                }
            }} // m_DB_Lock

            // Insert failed (record exists) - re-read the record
            blob_id = 0;
            }
            break;
        default:
            _ASSERT(0);
        }
    } // while (!blob_id)

    return EBlobCheckIn_NotFound;
}



void CBDB_Cache::x_UpdateOwnerStatOnDelete(const string& owner,
                                           bool          expl_delete)
{
/*
    SBDB_CacheStatistics& st = m_OwnerStatistics[owner];

    const char* key = m_CacheAttrDB->key;
    int version = m_CacheAttrDB->version;
    const char* subkey = m_CacheAttrDB->subkey;

    unsigned upd_count = m_CacheAttrDB->upd_count;
    unsigned read_count = m_CacheAttrDB->read_count;
    int overflow = m_CacheAttrDB->overflow;

    size_t blob_size = x_GetBlobSize(key, version, subkey);

    if (!blob_size) {
        return;
    }

    ++st.blobs_stored_total;
    st.blobs_overflow_total += overflow;
    st.blobs_updates_total += upd_count;
    if (!read_count) {
        ++st.blobs_never_read_total;
    }
    st.blobs_read_total += read_count;
    st.blobs_expl_deleted_total += expl_delete;
    if (!expl_delete) {
        st.AddPurgeDelete();
    }
    st.blobs_size_total += blob_size;
    if (blob_size > st.blob_size_max_total) {
        st.blob_size_max_total = blob_size;
    }

    SBDB_CacheStatistics::AddToHistogram(&st.blob_size_hist, blob_size);
*/
}
/*
size_t CBDB_Cache::x_GetBlobSize(const char* key,
                                 int         version,
                                 const char* subkey)
{
    int overflow = m_CacheAttrDB->overflow;
    unsigned blob_id = m_CacheAttrDB->blob_id;
    if (overflow) {
        string path;
        s_MakeOverflowFileName(path, m_Path, GetName(), key, version, subkey);
        CFile entry(path);

        if (entry.Exists()) {
            return (size_t) entry.GetLength();
        }
    }

    // Regular inline BLOB

    if (blob_id == 0) {
        return 0;
    }

    m_CacheBLOB_DB->blob_id = blob_id;
    EBDB_ErrCode ret = m_CacheBLOB_DB->Fetch();

    if (ret != eBDB_Ok) {
        return 0;
    }
    return m_CacheBLOB_DB->LobSize();
}
*/

void CBDB_Cache::Lock()
{
    m_DB_Lock.Lock();
}

void CBDB_Cache::Unlock()
{
    m_DB_Lock.Unlock();
}



void CBDB_Cache::RegisterInternalError(
        SBDB_CacheUnitStatistics::EErrGetPut operation,
        const string&                        owner)
{
    if (IsSaveStatistics()) {
        CFastMutexGuard guard(m_DB_Lock);
        m_Statistics.AddInternalError(owner, operation);
    }
}

void CBDB_Cache::RegisterProtocolError(
                            SBDB_CacheUnitStatistics::EErrGetPut operation,
                            const string&                        owner)
{
    if (IsSaveStatistics()) {
        CFastMutexGuard guard(m_DB_Lock);
        m_Statistics.AddProtocolError(owner, operation);
    }
}

void CBDB_Cache::RegisterNoBlobError(
                             SBDB_CacheUnitStatistics::EErrGetPut operation,
                             const string&                        owner)
{
    if (IsSaveStatistics()) {
        CFastMutexGuard guard(m_DB_Lock);
        m_Statistics.AddNoBlobError(owner, operation);
    }
}

void CBDB_Cache::RegisterCommError(
                              SBDB_CacheUnitStatistics::EErrGetPut operation,
                              const string&                        owner)
{
    if (IsSaveStatistics()) {
        CFastMutexGuard guard(m_DB_Lock);
        m_Statistics.AddCommError(owner, operation);
    }
}








CBDB_Cache::CacheKey::CacheKey(const string& x_key,
                               int           x_version,
                               const string& x_subkey)
: key(x_key), version(x_version), subkey(x_subkey)
{}


bool
CBDB_Cache::CacheKey::operator < (const CBDB_Cache::CacheKey& cache_key) const
{
    int cmp = NStr::Compare(key, cache_key.key);
    if (cmp != 0)
        return cmp < 0;
    if (version != cache_key.version) return (version < cache_key.version);
    cmp = NStr::Compare(subkey, cache_key.subkey);
    if (cmp != 0)
        return cmp < 0;
    return false;
}


void BDB_Register_Cache(void)
{
    RegisterEntryPoint<ICache>(NCBI_BDB_ICacheEntryPoint);
}


const char* kBDBCacheDriverName = "bdb";

/// Class factory for BDB implementation of ICache
///
/// @internal
///
class CBDB_CacheReaderCF : public CICacheCF<CBDB_Cache>
{
public:
    typedef CICacheCF<CBDB_Cache> TParent;
public:
    CBDB_CacheReaderCF() : TParent(kBDBCacheDriverName, 0)
    {
    }
    ~CBDB_CacheReaderCF()
    {
    }

    virtual
    ICache* CreateInstance(
                   const string&    driver  = kEmptyStr,
                   CVersionInfo     version = NCBI_INTERFACE_VERSION(ICache),
                   const TPluginManagerParamTree* params = 0) const;

};

// List of parameters accepted by the CF

static const char* kCFParam_path               = "path";
static const char* kCFParam_name               = "name";
static const char* kCFParam_drop_if_dirty      = "drop_if_dirty";

static const char* kCFParam_lock               = "lock";
static const char* kCFParam_lock_default       = "no_lock";
static const char* kCFParam_lock_pid_lock      = "pid_lock";

static const char* kCFParam_mem_size           = "mem_size";
static const char* kCFParam_log_mem_size       = "log_mem_size";
static const char* kCFParam_read_only          = "read_only";
static const char* kCFParam_write_sync         = "write_sync";
static const char* kCFParam_use_transactions   = "use_transactions";
static const char* kCFParam_direct_db          = "direct_db";
static const char* kCFParam_direct_log         = "direct_log";
static const char* kCFParam_transaction_log_path = "transaction_log_path";

static const char* kCFParam_purge_batch_size   = "purge_batch_size";
static const char* kCFParam_purge_batch_sleep  = "purge_batch_sleep";
static const char* kCFParam_purge_clean_log    = "purge_clean_log";
static const char* kCFParam_purge_thread       = "purge_thread";
static const char* kCFParam_purge_thread_delay = "purge_thread_delay";
static const char* kCFParam_checkpoint_delay   = "checkpoint_delay";
static const char* kCFParam_checkpoint_bytes   = "checkpoint_bytes";
static const char* kCFParam_log_file_max       = "log_file_max";
static const char* kCFParam_overflow_limit     = "overflow_limit";
static const char* kCFParam_ttl_prolong        = "ttl_prolong";
static const char* kCFParam_max_blob_size      = "max_blob_size";
static const char* kCFParam_rr_volumes         = "rr_volumes";
static const char* kCFParam_memp_trickle       = "memp_trickle";
static const char* kCFParam_TAS_spins          = "tas_spins";


bool CBDB_Cache::SameCacheParams(const TCacheParams* params) const
{
    if ( !params ) {
        return false;
    }
    const TCacheParams* driver = params->FindNode("driver");
    if (!driver  ||  driver->GetValue().value != kBDBCacheDriverName) {
        return false;
    }
    const TCacheParams* driver_params = params->FindNode(kBDBCacheDriverName);
    if ( !driver_params ) {
        return false;
    }
    const TCacheParams* path = driver_params->FindNode(kCFParam_path);
    string str_path = path ?
        CDirEntry::AddTrailingPathSeparator(
        path->GetValue().value) : kEmptyStr;
    if (!path  || str_path != m_Path) {
        return false;
    }
    const TCacheParams* name = driver_params->FindNode(kCFParam_name);
    return name  &&  name->GetValue().value == m_Name;
}


ICache* CBDB_CacheReaderCF::CreateInstance(
           const string&                  driver,
           CVersionInfo                   version,
           const TPluginManagerParamTree* params) const
{
    auto_ptr<CBDB_Cache> drv;
    if (driver.empty() || driver == m_DriverName) {
        if (version.Match(NCBI_INTERFACE_VERSION(ICache))
                            != CVersionInfo::eNonCompatible) {
            drv.reset(new CBDB_Cache());
        }
    } else {
        return 0;
    }

    if (!params)
        return drv.release();

    // cache configuration

    const string& path =
        GetParam(params, kCFParam_path, true);
    string name =
        GetParam(params, kCFParam_name, false, "lcache");
    string locking =
        GetParam(params, kCFParam_lock, false, kCFParam_lock_default);

    CBDB_Cache::ELockMode lock = CBDB_Cache::eNoLock;
    if (NStr::CompareNocase(locking, kCFParam_lock_pid_lock) == 0) {
        lock = CBDB_Cache::ePidLock;
    }
    unsigned overflow_limit = (unsigned)
        GetParamDataSize(params, kCFParam_overflow_limit, false, 0);
    if (overflow_limit) {
        drv->SetOverflowLimit(overflow_limit);
    }

    drv->SetInitIfDirty(GetParamBool(params, kCFParam_drop_if_dirty, false, false));

    Uint8 mem_size =
        GetParamDataSize(params, kCFParam_mem_size, false, 0);

    Uint8 log_mem_size =
        GetParamDataSize(params, kCFParam_log_mem_size, false, 0);

    unsigned checkpoint_bytes = (unsigned)
        GetParamDataSize(params, kCFParam_checkpoint_bytes,
                         false, 24 * 1024 * 1024);
    drv->SetCheckpoint(checkpoint_bytes);

    unsigned checkpoint_delay =
        GetParamInt(params, kCFParam_checkpoint_delay, false, 15);
    drv->SetCheckpointDelay(checkpoint_delay);

    unsigned log_file_max = (unsigned)
        GetParamDataSize(params, kCFParam_log_file_max,
                         false, 200 * 1024 * 1024);
    drv->SetLogFileMax(log_file_max);

    string transaction_log_path =
        GetParam(params, kCFParam_transaction_log_path, false, path);
    if (transaction_log_path != path) {
        drv->SetLogDir(transaction_log_path);
    }


    bool ro =
        GetParamBool(params, kCFParam_read_only, false, false);

    bool w_sync =
        GetParamBool(params, kCFParam_write_sync, false, false);
    drv->SetWriteSync(w_sync ?
                        CBDB_Cache::eWriteSync : CBDB_Cache::eWriteNoSync);

    unsigned ttl_prolong =
        GetParamInt(params, kCFParam_ttl_prolong, false, 0);
    drv->SetTTL_Prolongation(ttl_prolong);

    unsigned max_blob_size =(unsigned)
        GetParamDataSize(params, kCFParam_max_blob_size, false, 0);
    drv->SetMaxBlobSize(max_blob_size);

    unsigned rr_volumes =
        GetParamInt(params, kCFParam_rr_volumes, false, 3);
    drv->SetRR_Volumes(rr_volumes);

    unsigned memp_trickle =
        GetParamInt(params, kCFParam_memp_trickle, false, 60);
    drv->SetMempTrickle(memp_trickle);

    ConfigureICache(drv.get(), params);

    bool use_trans =
        GetParamBool(params, kCFParam_use_transactions, false, true);

    unsigned batch_size =
        GetParamInt(params, kCFParam_purge_batch_size, false, 70);
    drv->SetPurgeBatchSize(batch_size);

    unsigned batch_sleep =
        GetParamInt(params, kCFParam_purge_batch_sleep, false, 0);
    drv->SetBatchSleep(batch_sleep);

    unsigned purge_clean_factor =
        GetParamInt(params, kCFParam_purge_clean_log, false, 0);
    drv->CleanLogOnPurge(purge_clean_factor);

    bool purge_thread =
        GetParamBool(params, kCFParam_purge_thread, false, false);
    unsigned purge_thread_delay =
        GetParamInt(params, kCFParam_purge_thread_delay, false, 30);

    if (purge_thread) {
        drv->RunPurgeThread(purge_thread_delay);
    }

    unsigned tas_spins =
        GetParamInt(params, kCFParam_TAS_spins, false, 200);

    if (ro) {
        drv->OpenReadOnly(path.c_str(), name.c_str(), (unsigned)mem_size);
    } else {
        drv->Open(path, name,
                  lock, mem_size,
                  use_trans ? CBDB_Cache::eUseTrans : CBDB_Cache::eNoTrans,
                  (unsigned)log_mem_size);
    }

    if (!drv->IsJoinedEnv()) {
        bool direct_db =
            GetParamBool(params, kCFParam_direct_db, false, false);
        bool direct_log =
            GetParamBool(params, kCFParam_direct_log, false, false);

        CBDB_Env* env = drv->GetEnv();
        env->SetDirectDB(direct_db);
        env->SetDirectLog(direct_log);
        if (tas_spins) {
            env->SetTasSpins(tas_spins);
        }
        env->SetLkDetect(CBDB_Env::eDeadLock_Default);
        env->SetMpMaxWrite(0, 0);
    }

    return drv.release();

}


void NCBI_BDB_ICacheEntryPoint(
     CPluginManager<ICache>::TDriverInfoList&   info_list,
     CPluginManager<ICache>::EEntryPointRequest method)
{
    CHostEntryPointImpl<CBDB_CacheReaderCF>::
       NCBI_EntryPointImpl(info_list, method);
}

void NCBI_EntryPoint_xcache_bdb(
     CPluginManager<ICache>::TDriverInfoList&   info_list,
     CPluginManager<ICache>::EEntryPointRequest method)
{
    NCBI_BDB_ICacheEntryPoint(info_list, method);
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


void BDB_ConfigureCache(CBDB_Cache&             bdb_cache,
                        const string&           path,
                        const string&           name,
                        unsigned                timeout,
                        ICache::TTimeStampFlags tflags)
{
    if (!tflags) {
        tflags =
            ICache::fTimeStampOnCreate         |
            ICache::fExpireLeastFrequentlyUsed |
            ICache::fPurgeOnStartup            |
//            ICache::fTrackSubKey               |
            ICache::fCheckExpirationAlways;
    }
    if (timeout == 0) {
        timeout = 24 * 60 * 60;
    }

    bdb_cache.SetTimeStampPolicy(tflags, timeout);
    bdb_cache.SetVersionRetention(ICache::eKeepAll);

    bdb_cache.Open(path.c_str(),
                   name.c_str(),
                   CBDB_Cache::eNoLock,
                   10 * 1024 * 1024,
                   CBDB_Cache::eUseTrans);

}


END_NCBI_SCOPE
