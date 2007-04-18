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

#include <db.h>

#include <bdb/bdb_blobcache.hpp>
#include <bdb/bdb_cursor.hpp>
#include <bdb/bdb_trans.hpp>

#include <corelib/ncbimtx.hpp>
#include <corelib/ncbitime.hpp>
#include <time.h>

#include <util/cache/icache_cf.hpp>
#include <util/cache/icache_clean_thread.hpp>
#include <corelib/plugin_manager_store.hpp>

BEGIN_NCBI_SCOPE

		
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

#define BDB_TRAN_SYNC \
    (m_WSync == CBDB_Cache::eWriteSync ? \
     CBDB_Transaction::eTransSync : CBDB_Transaction::eTransASync)

/// @internal
class CBDB_CacheIReader : public IReader
{
public:

    CBDB_CacheIReader(CBDB_Cache&                bdb_cache,
                      CBDB_BLobStream*           blob_stream,
                      CBDB_Cache::EWriteSyncMode wsync)
    : m_Cache(bdb_cache),
      m_BlobStream(blob_stream),
      m_OverflowFile(0),
      m_Buffer(0),
      m_WSync(wsync)
    {}

    CBDB_CacheIReader(CBDB_Cache&                bdb_cache,
                      CNcbiIfstream*             overflow_file,
                      CBDB_Cache::EWriteSyncMode wsync)
    : m_Cache(bdb_cache),
      m_BlobStream(0),
      m_OverflowFile(overflow_file),
      m_Buffer(0),
      m_WSync(wsync)
    {}

    CBDB_CacheIReader(CBDB_Cache&                bdb_cache,
                      unsigned char*             buf,
                      size_t                     buf_size,
                      CBDB_Cache::EWriteSyncMode wsync)
    : m_Cache(bdb_cache),
      m_BlobStream(0),
      m_OverflowFile(0),
      m_Buffer(buf),
      m_BufferPtr(buf),
      m_BufferSize(buf_size),
      m_WSync(wsync)
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
            CFastMutexGuard guard(m_Cache.m_DB_Lock);

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

        CFastMutexGuard guard(m_Cache.m_DB_Lock);
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
    CBDB_Cache&                 m_Cache;
    CBDB_BLobStream*            m_BlobStream;
    CNcbiIfstream*              m_OverflowFile;
    unsigned char*              m_Buffer;
    unsigned char*              m_BufferPtr;
    size_t                      m_BufferSize;
    CBDB_Cache::EWriteSyncMode  m_WSync;
};


/// @internal
class CBDB_CacheIWriter : public IWriter
{
public:
    CBDB_CacheIWriter(CBDB_Cache&                bdb_cache,
                      const char*                path,
                      const string&              blob_key,
                      int                        version,
                      const string&              subkey,
                      SCache_AttrDB&             attr_db,
                      int                        stamp_subkey,
                      CBDB_Cache::EWriteSyncMode wsync,
                      unsigned int               ttl,
                      time_t                     request_time,
                      const string&              owner)
    : m_Cache(bdb_cache),
      m_Path(path),
      m_BlobKey(blob_key),
      m_Version(version),
      m_SubKey(subkey),
      m_AttrDB(attr_db),
      m_Buffer(0),
      m_BytesInBuffer(0),
      m_OverflowFile(0),
      m_StampSubKey(stamp_subkey),
	  m_AttrUpdFlag(false),
      m_WSync(wsync),
      m_TTL(ttl),
      m_RequestTime(request_time),
      m_Flushed(false),
      m_BlobSize(0),
      m_Overflow(0),
      m_BlobStore(0),
      m_BlobUpdate(0),
      m_Owner(owner)
    {
        m_Buffer = new unsigned char[m_Cache.GetOverflowLimit()];
    }

    virtual ~CBDB_CacheIWriter()
    {
        try {
            bool upd_statistics = false;
		    if (!m_AttrUpdFlag || m_Buffer != 0) {

			    // Dumping the buffer
                try {
                    if (m_Buffer) {
                        m_Cache.Store(m_BlobKey,
                                      m_Version,
                                      m_SubKey,
                                      m_Buffer,
                                      m_BytesInBuffer,
                                      m_TTL,
                                      m_Owner);
				        delete[] m_Buffer; m_Buffer = 0;
                    }
                } catch (CBDB_Exception& ) {
                    m_Cache.KillBlob(m_BlobKey.c_str(), 
                                     m_Version, 
                                     m_SubKey.c_str(), 1,
                                     0);
                    throw;
                }
		    }

            if (m_OverflowFile) {
                if (m_OverflowFile->is_open()) {
                    m_OverflowFile->close();
			        CFastMutexGuard guard(m_Cache.m_DB_Lock);
                    CBDB_Cache::CCacheTransaction trans(m_Cache);
                    try {
                        x_UpdateAttributes(trans);
                    } catch (exception& ) {
                        m_Cache.KillBlob(m_BlobKey.c_str(),
                                         m_Version,
                                         m_SubKey.c_str(), 1, 0);
                        throw;
                    }
                    upd_statistics = true;
                }
                delete m_OverflowFile; m_OverflowFile = 0;
            }

            // statistics
            //
            if (upd_statistics) {
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

        } catch (exception & ex) {
            ERR_POST("Exception in ~CBDB_CacheIWriter() : " << ex.what()
                     << " " << m_BlobKey);
            // final attempt to avoid leaks
            delete[] m_Buffer;
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
		m_AttrUpdFlag = false;
        m_BlobSize += count;

        if (m_Buffer) {
            unsigned int new_buf_length = m_BytesInBuffer + count;
            if (new_buf_length <= m_Cache.GetOverflowLimit()) {
                ::memcpy(m_Buffer + m_BytesInBuffer, buf, count);
                m_BytesInBuffer = new_buf_length;
                if (bytes_written) {
                    *bytes_written = count;
                }

                return eRW_Success;
            } else {
                // Buffer overflow. Writing to file.
                OpenOverflowFile();
                if (m_OverflowFile) {
                    if (m_BytesInBuffer) {
                        try {
                            x_WriteOverflow((const char*)m_Buffer,
                                             m_BytesInBuffer);
                        } 
                        catch (exception& ) {
                            delete[] m_Buffer; m_Buffer = 0;
                            delete m_OverflowFile; m_OverflowFile = 0;
                            throw;
                        }
                    }
                    delete[] m_Buffer; m_Buffer = 0; m_BytesInBuffer = 0;
                }
            }

        }
        if (m_OverflowFile) {

            _ASSERT(m_Buffer == 0);

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

        if (m_Buffer) {

            _ASSERT(m_OverflowFile == 0);

            try {
                m_Cache.Store(m_BlobKey,
                                m_Version,
                                m_SubKey,
                                m_Buffer,
                                m_BytesInBuffer,
                                m_TTL,
                                m_Owner);
                return eRW_Success;
            } 
            catch (exception&) {
    			delete[] m_Buffer; m_Buffer = 0;
                throw;
            }

			delete[] m_Buffer; m_Buffer = 0;
            m_BytesInBuffer = 0;
            return eRW_Success;
        }

        if ( m_OverflowFile ) {

            _ASSERT(m_Buffer == 0);

            try {
                m_OverflowFile->flush();
                if ( m_OverflowFile->bad() ) {
                    m_OverflowFile->close();
                    BDB_THROW(eOverflowFileIO, 
                              "Error trying to flush an overflow file");
                }

                CFastMutexGuard guard(m_Cache.m_DB_Lock);
                CBDB_Cache::CCacheTransaction trans(m_Cache);
		        x_UpdateAttributes(trans);
                trans.Commit();

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

            } catch (CBDB_Exception& ) {
                m_Cache.KillBlob(m_BlobKey.c_str(),
                                 m_Version,
                                 m_SubKey.c_str(), 1,
                                 0);
                throw;
            }
        }
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
            m_Cache.KillBlob(
                m_BlobKey.c_str(), m_Version, m_SubKey.c_str(), 1, 0);
            throw;
        }
    }

	void x_UpdateAttributes(CBDB_Transaction& trans)
	{
        time_t curr = time(0);

        CBDB_FileCursor cur(m_AttrDB, trans, 
                            CBDB_FileCursor::eReadModifyUpdate);
        cur.SetCondition(CBDB_FileCursor::eEQ);
        cur.From << m_BlobKey << m_Version << m_SubKey;

        if (cur.Fetch() == eBDB_Ok) {
            m_AttrDB.time_stamp = (unsigned) curr;

            unsigned upd_count = m_AttrDB.upd_count;
            ++upd_count;
            m_AttrDB.upd_count = upd_count;

            m_AttrDB.ttl = m_TTL;
            m_AttrDB.max_time =
                curr + (m_Cache.GetTimeout() * m_Cache.GetTTL_Prolongation());
            m_AttrDB.owner_name = m_Owner;
            m_AttrDB.overflow = (m_OverflowFile != 0);

            cur.Update();

            m_BlobUpdate = 1;

        } else {
            m_AttrDB.key = m_BlobKey.c_str();
            m_AttrDB.version = m_Version;
		    m_AttrDB.subkey = m_SubKey;

            m_AttrDB.time_stamp = (unsigned) curr;

            if (m_OverflowFile) {
                m_AttrDB.overflow = 1;
                delete m_OverflowFile;
                m_OverflowFile = 0;
            } else {
                m_AttrDB.overflow = 0;
            }
            m_AttrDB.ttl = m_TTL;
            m_AttrDB.max_time =
                curr + (m_Cache.GetTimeout() * m_Cache.GetTTL_Prolongation());
            m_AttrDB.read_count = 0;
            m_AttrDB.upd_count = 0;
            m_AttrDB.owner_name = m_Owner;

            m_AttrDB.UpdateInsert();

            m_BlobStore = 1;
        }

		// Time stamp the key with empty subkey
		if (!m_StampSubKey && !m_SubKey.empty()) {
			m_AttrDB.key = m_BlobKey.c_str();
			m_AttrDB.version = m_Version;
			m_AttrDB.subkey = "";
			m_AttrDB.overflow = 0;
            m_AttrDB.ttl = m_TTL;

			m_AttrDB.time_stamp = (unsigned) curr;
            m_AttrDB.max_time =
                curr + (m_Cache.GetTimeout() * m_Cache.GetTTL_Prolongation());
	        m_AttrDB.UpdateInsert();
		}

        if (m_WSync == CBDB_Cache::eWriteSync) {
    		m_AttrDB.Sync();
        }

		m_AttrUpdFlag = true;
	}

private:
    CBDB_CacheIWriter(const CBDB_CacheIWriter&);
    CBDB_CacheIWriter& operator=(const CBDB_CacheIWriter&);

private:
    CBDB_Cache&           m_Cache;
    const char*           m_Path;
    string                m_BlobKey;
    int                   m_Version;
    string                m_SubKey;
    SCache_AttrDB&        m_AttrDB;

    unsigned char*        m_Buffer;
    unsigned int          m_BytesInBuffer;
    CNcbiOfstream*        m_OverflowFile;
    string                m_OverflowFilePath;

    int                   m_StampSubKey;
	bool                  m_AttrUpdFlag; ///< Falgs attributes are up to date
    CBDB_Cache::EWriteSyncMode m_WSync;
    unsigned int          m_TTL;
    time_t                m_RequestTime;
    bool                  m_Flushed;     ///< FALSE until Flush() called

    unsigned              m_BlobSize;    ///< Number of bytes written
    unsigned              m_Overflow;    ///< Overflow file created
    unsigned              m_BlobStore;
    unsigned              m_BlobUpdate;
    string                m_Owner;
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
        err_no_blob = err_blob_get = err_blob_put = 0;

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
    *day = time_in_secs / (24 * 60 * 60);
    unsigned secs_in_day = time_in_secs % (24 * 60 * 60);
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
             NStr::UIntToString(err_blob_get), 0,
             "Number of errors when storing BLOBs");

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

        SBDB_CacheUnitStatistics::TBlobSizeHistogram::const_iterator it = 
            hist.begin();

        for (; it != hist.end(); ++it) {
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




CBDB_Cache::CBDB_Cache()
: m_PidGuard(0),
  m_ReadOnly(false),
  m_JoinedEnv(false),
  m_Env(0),
  m_CacheBLOB_DB(0),
  m_CacheAttrDB(0),
  m_Timeout(0),
  m_MaxTimeout(0),
  m_VersionFlag(eDropOlder),
  m_PageSizeHint(eLarge),
  m_WSync(eWriteNoSync),
  m_PurgeBatchSize(150),
  m_BatchSleep(0),
  m_PurgeStopSignal(0, 100), // purge stop semaphore max 100
  m_BytesWritten(0),
  m_CleanLogOnPurge(0),
  m_PurgeCount(0),
  m_LogSizeMax(0),
  m_PurgeNowRunning(false),
  m_RunPurgeThread(false),
  m_PurgeThreadDelay(10),
  m_CheckPointInterval(24 * (1024 * 1024)),
  m_OverflowLimit(512 * 1024),
  m_MaxTTL_prolong(0)
//  m_CollectOwnerStat(true)
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
                      unsigned int  cache_ram_size,
                      ETRansact     use_trans,
                      unsigned int  log_mem_size)
{
    {{

    Close();

    CFastMutexGuard guard(m_DB_Lock);

    m_Path = CDirEntry::AddTrailingPathSeparator(cache_path);
    m_Name = cache_name;

    // Make sure our directory exists
    {{
        CDir dir(m_Path);
        if ( !dir.Exists() ) {
            dir.Create();
        }
    }}

    CDir dir(m_Path);
    CDir::TEntries fl = dir.GetEntries("__db.*", CDir::eIgnoreRecursive);


    m_BytesWritten = 0;
    m_Env = new CBDB_Env();

    string err_file = m_Path + "err" + string(cache_name) + ".log";
    m_Env->OpenErrFile(err_file.c_str());

    m_JoinedEnv = false;
    bool needs_recovery = false;

    if (!fl.empty()) {
        try {
            m_Env->JoinEnv(cache_path, CBDB_Env::eThreaded);
            if (m_Env->IsTransactional()) {
                LOG_POST(Info <<
                         "LC: '" << cache_name <<
                         "' Joined transactional environment ");
            } else {
                LOG_POST(Info <<
                         "LC: '" << cache_name <<
                         "' Warning: Joined non-transactional environment ");
            }
            m_JoinedEnv = true;
        }
        catch (CBDB_ErrnoException& err_ex)
        {
            if (err_ex.IsRecovery()) {
                LOG_POST(Warning <<
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
        if (cache_ram_size) {
            m_Env->SetCacheSize(cache_ram_size);
        }
        x_PidLock(lm);
        switch (use_trans)
        {
        case eUseTrans:
            {
            CBDB_Env::TEnvOpenFlags env_flags = CBDB_Env::eThreaded;
            if (needs_recovery) {
                env_flags |= CBDB_Env::eRunRecovery;
            }
            m_Env->OpenWithTrans(cache_path, env_flags);
            }
            break;
        case eNoTrans:
            LOG_POST(Info << "BDB_Cache: Creating locking environment");
            m_Env->OpenWithLocks(cache_path);
            break;
        default:
            _ASSERT(0);
        } // switch

        m_Env->SetLogAutoRemove(true);

        m_Env->SetLockTimeout(30 * 1000000); // 30 sec
        if (m_Env->IsTransactional()) {
            m_Env->SetTransactionTimeout(30 * 1000000); // 30 sec
        }

    }


    m_CacheBLOB_DB = new SCacheBLOB_DB();
    m_CacheAttrDB = new SCache_AttrDB();

    m_CacheBLOB_DB->SetEnv(*m_Env);
    m_CacheAttrDB->SetEnv(*m_Env);

    switch (m_PageSizeHint)
    {
    case eLarge:
        m_CacheBLOB_DB->SetPageSize(32 * 1024);
        break;
    case eSmall:
        // nothing to do
        break;
    default:
        _ASSERT(0);
    } // switch

    string cache_blob_db_name =
       string("lcs_") + string(cache_name) + string("_blob")+ string(".db");
    string attr_db_name =
       string("lcs_") + string(cache_name) + string("_attr4") + string(".db");

    m_CacheBLOB_DB->Open(cache_blob_db_name, CBDB_RawFile::eReadWriteCreate);
    m_CacheAttrDB->Open(attr_db_name,        CBDB_RawFile::eReadWriteCreate);

    }}

    // read cache attributes so we can adjust atomic counter
    //

    {{
    LOG_POST(Info << "Scanning cache content.");
    CBDB_FileCursor cur(*m_CacheAttrDB);
    cur.SetCondition(CBDB_FileCursor::eFirst);
    cur.InitMultiFetch(10 * 1024 * 1024);
    unsigned max_blob_id = 0;
    while (cur.Fetch() == eBDB_Ok) {
        unsigned blob_id = m_CacheAttrDB->blob_id;
        max_blob_id = max(max_blob_id, blob_id);
    }
    m_BlobIdCounter.Set(max_blob_id);
    }}

    if (m_TimeStampFlag & fPurgeOnStartup) {
        unsigned batch_sleep = m_BatchSleep;
        unsigned batch_size = m_PurgeBatchSize;

        // setup parameters which favor fast Purge execution
        // (Open purge needs to be fast because all waiting for it)

        if (m_PurgeBatchSize < 2500) {
            m_PurgeBatchSize = 2500;
        }
        m_BatchSleep = 0;

        Purge(GetTimeout());

        // Restore parameters

        m_BatchSleep = batch_sleep;
        m_PurgeBatchSize = batch_size;
    }


    if (m_RunPurgeThread) {
# ifdef NCBI_THREADS
       LOG_POST(Info << "Starting cache cleaning thread.");
       m_PurgeThread.Reset(
           new CCacheCleanerThread(this, m_PurgeThreadDelay, 5));
       m_PurgeThread->Run();
# else
        LOG_POST(Warning <<
                 "Cannot run background thread in non-MT configuration.");
# endif
    }


    m_Env->TransactionCheckpoint();

    m_ReadOnly = false;


    LOG_POST(Info <<
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
        LOG_POST(Info << "Stopping cache cleaning thread...");
        StopPurge();
        m_PurgeThread->RequestStop();
        m_PurgeThread->Join();
        LOG_POST(Info << "Stopped.");
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

    m_CacheBLOB_DB = new SCacheBLOB_DB();
    m_CacheAttrDB = new SCache_AttrDB();

    if (cache_ram_size)
        m_CacheBLOB_DB->SetCacheSize(cache_ram_size);

    string cache_blob_db_name =
       string("lcs_") + string(cache_name) + string("_blob")+ string(".db");
    string attr_db_name =
       m_Path + string("lcs_") + string(cache_name) + string("_attr4")
	          + string(".db");


    m_CacheBLOB_DB->Open(cache_blob_db_name,  CBDB_RawFile::eReadOnly);
    m_CacheAttrDB->Open(attr_db_name.c_str(), CBDB_RawFile::eReadOnly);

    }}

    m_ReadOnly = true;

    LOG_POST(Info <<
             "LC: '" << cache_name <<
             "' Cache mount read-only at: " << cache_path);
}


void CBDB_Cache::Close()
{
    StopPurgeThread();

    delete m_PidGuard;    m_PidGuard = 0;
    delete m_CacheBLOB_DB; m_CacheBLOB_DB = 0;
    delete m_CacheAttrDB; m_CacheAttrDB = 0;

    try {
        if (m_Env) {
            m_Env->TransactionCheckpoint();
            CleanLog();

            if (m_Env->CheckRemove()) {
                LOG_POST(Info    <<
                         "LC: '" << m_Name << "' Unmounted. BDB ENV deleted.");
            } else {
                LOG_POST(Warning << "LC: '" << m_Name
                                 << "' environment still in use.");

            }
        }
    }
    catch (exception& ex) {
        LOG_POST(Warning << "LC: '" << m_Name
                         << "' Exception in Close() " << ex.what()
                         << " (ignored.)");
    }

    delete m_Env;         m_Env = 0;
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

void CBDB_Cache::KillBlobNoLock(const char*    key,
                                int            version,
                                const char*    subkey,
                                int            overflow,
                                unsigned       blob_id)
{
    CCacheTransaction trans(*this);
    x_DropBlob(key, version, subkey, overflow, blob_id);
    trans.Commit();
}

void CBDB_Cache::KillBlob(const char*    key,
                          int            version,
                          const char*    subkey,
                          int            overflow,
                          unsigned       blob_id)
{
    CFastMutexGuard guard(m_DB_Lock);
    KillBlobNoLock(key, version, subkey, overflow, blob_id);
}

void CBDB_Cache::DropBlob(const string&  key,
                          int            version,
                          const string&  subkey,
                          bool           for_update,
                          unsigned*      blob_id)
{
    _ASSERT(blob_id);

    int overflow = 0;

    {{
    CFastMutexGuard guard(m_DB_Lock);

    CCacheTransaction trans(*this);


    {{
        CBDB_FileCursor cur(*m_CacheAttrDB, trans,
                            CBDB_FileCursor::eReadModifyUpdate);
        cur.SetCondition(CBDB_FileCursor::eEQ);

        cur.From << key << version << subkey;

        if (cur.Fetch() == eBDB_Ok) {
            overflow = m_CacheAttrDB->overflow;
            *blob_id = m_CacheAttrDB->blob_id;

            if (!for_update) {   // permanent BLOB removal
                unsigned read_count = m_CacheAttrDB->read_count;
                m_CacheAttrDB->owner_name.ToString(m_TmpOwnerName);

                m_Statistics.AddExplDelete(m_TmpOwnerName);
                if (0 == read_count) {
                    m_Statistics.AddNeverRead(m_TmpOwnerName);
                }
                x_UpdateOwnerStatOnDelete(m_TmpOwnerName, 
                                          true/*explicit del*/);
                cur.Delete();
            }
            m_CacheBLOB_DB->blob_id = *blob_id;
            m_CacheBLOB_DB->Delete(CBDB_RawFile::eIgnoreError);

        } else {
            *blob_id = 0;
            return;
        }
    }}
    trans.Commit();
    }}

    if (overflow) {
        x_DropOverflow(key.c_str(), version, subkey.c_str());
    }

}


void CBDB_Cache::Store(const string&  key,
                       int            version,
                       const string&  subkey,
                       const void*    data,
                       size_t         size,
                       unsigned int   time_to_live,
                       const string&  owner)
{
    if (IsReadOnly()) {
        return;
    }

    unsigned blob_id = 0;
    if (m_VersionFlag == eDropAll || m_VersionFlag == eDropOlder) {
        Purge(key, subkey, 0, m_VersionFlag);
    } else {
        DropBlob(key, version, subkey, true /*update*/, &blob_id);
    }
    unsigned overflow = 0, old_overflow = 0;

    time_t curr = time(0);
    int tz_delta = m_LocalTimer.GetLocalTimezone();

    {{
    CFastMutexGuard guard(m_DB_Lock);

    CCacheTransaction trans(*this);

    if (size < GetOverflowLimit()) {  // inline BLOB

        if (blob_id == 0) {
            EBDB_ErrCode ret = eBDB_KeyDup;
            do {
                blob_id = m_BlobIdCounter.Add(1);
                if (blob_id >= kMax_UInt) {
                    m_BlobIdCounter.Set(0);
                    continue;
                }
                m_CacheBLOB_DB->blob_id = blob_id;
                ret = m_CacheBLOB_DB->Insert(data, size);
            } while (ret == eBDB_KeyDup);
        } else {
            m_CacheBLOB_DB->blob_id = blob_id;
            /*EBDB_ErrCode ret = */ m_CacheBLOB_DB->UpdateInsert(data, size);
        }

        overflow = 0;

    } else { // overflow BLOB
        string path;
        s_MakeOverflowFileName(path, m_Path, GetName(), key, version, subkey);
        _TRACE("LC: Making overflow file " << path);
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
        overflow = 1;
    }

    //
    // Update cache element's attributes
    //

    if (m_MaxTimeout) {
        if (time_to_live > m_MaxTimeout) {
            time_to_live = m_MaxTimeout;
        }
    } else { // m_MaxTimeout == 0
        if (m_MaxTTL_prolong != 0 && m_Timeout != 0) {
            time_to_live = min(m_Timeout * m_MaxTTL_prolong, time_to_live);
        }
    }

    unsigned blob_updated = 0;
    unsigned blob_stored  = 0;
    EBlobAccessType access_type = eBlobStore;

    {{
    CBDB_FileCursor cur(*m_CacheAttrDB, 
                        trans, 
                        CBDB_FileCursor::eReadModifyUpdate);
    cur.SetCondition(CBDB_FileCursor::eEQ);
    cur.From << key << version << subkey;

    if (cur.Fetch() == eBDB_Ok) {
        old_overflow = m_CacheAttrDB->overflow;

        m_CacheAttrDB->time_stamp = (unsigned)curr;
        m_CacheAttrDB->overflow = overflow;
        m_CacheAttrDB->ttl = time_to_live;
        m_CacheAttrDB->max_time = curr + (GetTimeout() * GetTTL_Prolongation());
        unsigned upd_count = m_CacheAttrDB->upd_count;
        ++upd_count;
        m_CacheAttrDB->upd_count = upd_count;
        m_CacheAttrDB->owner_name = owner;
        m_CacheAttrDB->blob_id = blob_id;

        cur.Update();

        blob_updated = 1;
        access_type = eBlobUpdate;
    } else {
        m_CacheAttrDB->key = key;
        m_CacheAttrDB->version = version;
	    m_CacheAttrDB->subkey = subkey;
        m_CacheAttrDB->time_stamp = (unsigned)curr;
        m_CacheAttrDB->overflow = overflow;
        m_CacheAttrDB->ttl = time_to_live;
        m_CacheAttrDB->max_time = curr + (GetTimeout() * GetTTL_Prolongation());
        m_CacheAttrDB->upd_count  = 0;
        m_CacheAttrDB->read_count = 0;
        m_CacheAttrDB->owner_name = owner;
        m_CacheAttrDB->blob_id = blob_id;

        m_CacheAttrDB->Insert();

        blob_stored = 1;
    }

    }}

    trans.Commit();

    m_Statistics.AddStore(owner, curr - tz_delta, 
                          blob_stored, blob_updated, size, overflow);

	if (!(m_TimeStampFlag & fTrackSubKey)) {
        if (!subkey.empty()) {
            CCacheTransaction transact(*this);
		    x_UpdateAccessTime_NonTrans(key, version, subkey, access_type);
            transact.Commit();
        }
	}

    if (overflow == 0) { // inline BLOB
        x_PerformCheckPointNoLock(size);
    }

    }}


    // check if BLOB shrinked in size but was an overflow
    // and we need to delete the overflow file
    //
    if (overflow != old_overflow) {
        if (old_overflow != 0) {
            x_DropOverflow(key.c_str(), version, subkey.c_str());
        }
    }

}


size_t CBDB_Cache::GetSize(const string&  key,
                           int            version,
                           const string&  subkey)
{
    CFastMutexGuard guard(m_DB_Lock);

	int overflow;
    unsigned int ttl, blob_id;
	bool rec_exists =
        x_RetrieveBlobAttributes(key, version, subkey, 
                                 &overflow, &ttl, &blob_id);
	if (!rec_exists) {
		return 0;
	}

    // check expiration here
    if (m_TimeStampFlag & fCheckExpirationAlways) {
        if (x_CheckTimestampExpired()) {
            return 0;
        }
    }

    return x_GetBlobSize(key.c_str(), version, subkey.c_str());
}

void CBDB_Cache::GetBlobOwner(const string&  key,
                              int            version,
                              const string&  subkey,
                              string*        owner)
{
    _ASSERT(owner);

    CFastMutexGuard guard(m_DB_Lock);

    bool ret = x_FetchBlobAttributes(key, version, subkey);
    if (ret == false) {
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

    CBDB_FileCursor cur(*m_CacheAttrDB);
    cur.SetCondition(CBDB_FileCursor::eGE);
    cur.From << key;
    if (!subkey.empty()) {
        cur.From << subkey;
    }
    if (cur.FetchFirst() != eBDB_Ok) {
        return false;
    }

    if (x_CheckTimestampExpired(curr)) {
        return false;
    }

    return true;
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


    CFastMutexGuard guard(m_DB_Lock);

	int overflow;
    unsigned int ttl, blob_id;
	bool rec_exists =
        x_RetrieveBlobAttributes(key, version, subkey, 
                                 &overflow, &ttl, &blob_id);
	if (!rec_exists) {
		return false;
	}

    // check expiration
    if (m_TimeStampFlag & fCheckExpirationAlways) {
        if (x_CheckTimestampExpired()) {
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

        m_CacheBLOB_DB->blob_id = blob_id;
        ret = m_CacheBLOB_DB->Fetch();
        if (ret != eBDB_Ok) {
            return false;
        }
        ret = m_CacheBLOB_DB->GetData(buf, buf_size);
        if (ret != eBDB_Ok) {
            return false;
        }
    }



    if ( m_TimeStampFlag & fTimeStampOnRead ) {
        x_UpdateReadAccessTime(key, version, subkey);
    }
    return true;

}

IReader* CBDB_Cache::x_CreateOverflowReader(int            overflow,
                                            const string&  key,
                                            int            version,
                                            const string&  subkey,
                                            size_t&        file_length)
{
    if (overflow) {
        string path;
        s_MakeOverflowFileName(path, m_Path, GetName(), key, version, subkey);
        auto_ptr<CNcbiIfstream>
            overflow_file(new CNcbiIfstream(path.c_str(),
                                            IOS_BASE::in | IOS_BASE::binary));
        if (!overflow_file->is_open()) {
            return 0;
        }
        if ( m_TimeStampFlag & fTimeStampOnRead ) {
            x_UpdateReadAccessTime(key, version, subkey);
        }
        CFile entry(path);
        file_length = (size_t) entry.GetLength();

        return new CBDB_CacheIReader(*this, overflow_file.release(), m_WSync);
    }
    return 0;
}


IReader* CBDB_Cache::GetReadStream(const string&  key,
                                   int            version,
                                   const string&  subkey)
{
	EBDB_ErrCode ret;

    time_t curr = time(0);
    int tz_delta = m_LocalTimer.GetLocalTimezone();

    CFastMutexGuard guard(m_DB_Lock);

	int overflow;
    unsigned int ttl, blob_id;
	bool rec_exists =
        x_RetrieveBlobAttributes(key, version, subkey, 
                                 &overflow, &ttl, &blob_id);
	if (!rec_exists) {
		return 0;
	}
    // check expiration
    if (m_TimeStampFlag & fCheckExpirationAlways) {
        if (x_CheckTimestampExpired()) {
            return 0;
        }
    }
    size_t bsize;

    m_CacheAttrDB->owner_name.ToString(m_TmpOwnerName);
    m_Statistics.AddRead(m_TmpOwnerName, curr - tz_delta);

    // Check if it's an overflow BLOB (external file)
    {{
    IReader *rd = x_CreateOverflowReader(overflow, key, version, subkey, bsize);
    if (rd)
        return rd;
    }}
    // Inline BLOB, reading from BDB storage

    if (blob_id == 0) {
        return 0;
    }
    m_CacheBLOB_DB->blob_id = blob_id;
    ret = m_CacheBLOB_DB->Fetch();
    if (ret != eBDB_Ok) {
        return 0;
    }
    bsize = m_CacheBLOB_DB->LobSize();
    unsigned char* buf = new unsigned char[bsize+1];

    ret = m_CacheBLOB_DB->GetData(buf, bsize);
    if (ret != eBDB_Ok) {
        return 0;
    }

    if ( m_TimeStampFlag & fTimeStampOnRead ) {
        x_UpdateReadAccessTime(key, version, subkey);
    }
    return new CBDB_CacheIReader(*this, buf, bsize, m_WSync);
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

    CFastMutexGuard guard(m_DB_Lock);

	int overflow;
    unsigned int ttl, blob_id;
	bool rec_exists =
        x_RetrieveBlobAttributes(key, version, subkey, 
                                 &overflow, &ttl, &blob_id);
	if (!rec_exists) {
		return;
	}
    // check expiration
    if (m_TimeStampFlag & fCheckExpirationAlways) {
        if (x_CheckTimestampExpired()) {
            return;
        }
    }

    m_CacheAttrDB->owner_name.ToString(m_TmpOwnerName);
    m_Statistics.AddRead(m_TmpOwnerName, curr - tz_delta);

    // Check if it's an overflow BLOB (external file)
    blob_descr->reader.reset(
        x_CreateOverflowReader(overflow,
                               key, version, subkey,
                               blob_descr->blob_size));
    if (blob_descr->reader.get()) {
        blob_descr->blob_found = true;
        return;
    }

    // Inline BLOB, reading from BDB storage

    if (blob_id == 0) {
        blob_descr->blob_found = false;
        return;
    }

    m_CacheBLOB_DB->blob_id = blob_id;    

    if (blob_descr->buf && blob_descr->buf_size) {
        // use speculative read (hope provided buffer is sufficient)
        try {
            char** ptr = &blob_descr->buf;
            ret = m_CacheBLOB_DB->Fetch((void**)ptr,
                                        blob_descr->buf_size,
                                        CBDB_RawFile::eReallocForbidden);
            if (ret == eBDB_Ok) {
                blob_descr->blob_found = true;
                blob_descr->blob_size = m_CacheBLOB_DB->LobSize();
                return;
            }
        } catch (CBDB_Exception&) {
        }
    } 
    // speculative Fetch failed (or impossible), read it another way
    ret = m_CacheBLOB_DB->Fetch();

    if (ret != eBDB_Ok) {
        blob_descr->blob_found = false;
        blob_descr->blob_size = 0;
        return;
    }
    blob_descr->blob_found = true;
    blob_descr->blob_size = m_CacheBLOB_DB->LobSize();

    // read the BLOB into a custom in-memory buffer
    CBDB_BLobStream* bstream = m_CacheBLOB_DB->CreateStream();
    blob_descr->reader.reset(new CBDB_CacheIReader(*this, bstream, m_WSync));

}


IWriter* CBDB_Cache::GetWriteStream(const string&    key,
                                    int              version,
                                    const string&    subkey,
                                    unsigned int     time_to_live,
                                    const string&    owner)
{
    if (IsReadOnly()) {
        return 0;
    }

    if (m_VersionFlag == eDropAll || m_VersionFlag == eDropOlder) {
        Purge(key, subkey, 0, m_VersionFlag);
    }
    unsigned blob_id;


    DropBlob(key, version, subkey, true /*update*/, &blob_id);

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

    return
        new CBDB_CacheIWriter(*this,
                              m_Path.c_str(),
                              key,
                              version,
                              subkey,
                              *m_CacheAttrDB,
                              m_TimeStampFlag & fTrackSubKey,
                              m_WSync,
                              time_to_live,
                              curr - tz_delta,
                              owner);
}


void CBDB_Cache::Remove(const string& key)
{
    if (IsReadOnly()) {
        return;
    }

    CFastMutexGuard guard(m_DB_Lock);

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
        unsigned blob_id = m_CacheAttrDB->blob_id;

        cache_elements.push_back(
            SCacheDescr(key, version, subkey, overflow, blob_id));

        unsigned read_count = m_CacheAttrDB->read_count;
        m_CacheAttrDB->owner_name.ToString(m_TmpOwnerName);
        if (0 == read_count) {
            m_Statistics.AddNeverRead(m_TmpOwnerName);
        }
        m_Statistics.AddExplDelete(m_TmpOwnerName);
        x_UpdateOwnerStatOnDelete(m_TmpOwnerName, true/*expl-delete*/);
    }

    }}

    {{
    CCacheTransaction trans(*this);

    // Now delete all objects

    ITERATE(vector<SCacheDescr>, it, cache_elements) {
        x_DropBlob(it->key.c_str(),
                   it->version,
                   it->subkey.c_str(),
                   it->overflow,
                   it->blob_id);
    }

	trans.Commit();
    }}

    // Second pass scan if for some resons some cache elements are
    // still in the database
/*
    cache_elements.resize(0);
    CCacheTransaction trans(*this);

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
*/
}

void CBDB_Cache::Remove(const string&    key,
                        int              version,
                        const string&    subkey)
{
    if (IsReadOnly()) {
        return;
    }
    CFastMutexGuard guard(m_DB_Lock);

    // Search the records to delete

    vector<SCacheDescr>  cache_elements;

    {{

    CBDB_FileCursor cur(*m_CacheAttrDB);
    cur.SetCondition(CBDB_FileCursor::eEQ);

    cur.From << key << version << subkey;
    while (cur.Fetch() == eBDB_Ok) {
        int overflow = m_CacheAttrDB->overflow;
        unsigned blob_id = m_CacheAttrDB->blob_id;

        cache_elements.push_back(
            SCacheDescr(key, version, subkey, overflow, blob_id));

        unsigned read_count = m_CacheAttrDB->read_count;
        m_CacheAttrDB->owner_name.ToString(m_TmpOwnerName);
        if (0 == read_count) {
            m_Statistics.AddNeverRead(m_TmpOwnerName);
        }
        m_Statistics.AddExplDelete(m_TmpOwnerName);
        x_UpdateOwnerStatOnDelete(m_TmpOwnerName, true/*expl-delete*/);
    }

    }}



    CBDB_Cache::CCacheTransaction trans(*this);
    ITERATE(vector<SCacheDescr>, it, cache_elements) {
        x_DropBlob(it->key.c_str(),
                   it->version,
                   it->subkey.c_str(),
                   it->overflow,
                   it->blob_id);
    }

	trans.Commit();
}


time_t CBDB_Cache::GetAccessTime(const string&  key,
                                 int            version,
                                 const string&  subkey)
{
    _ASSERT(m_CacheAttrDB);
    CFastMutexGuard guard(m_DB_Lock);

    m_CacheAttrDB->key = key;
    m_CacheAttrDB->version = version;
    m_CacheAttrDB->subkey = (m_TimeStampFlag & fTrackSubKey) ? subkey : "";

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
    unsigned ret = m_PurgeBatchSize;
    return ret;
}

void CBDB_Cache::SetBatchSleep(unsigned sleep)
{
    CFastMutexGuard guard(m_DB_Lock);
    m_BatchSleep = sleep;
}

unsigned CBDB_Cache::GetBatchSleep() const
{
    CFastMutexGuard guard(m_DB_Lock);
    unsigned ret = m_BatchSleep;
    return ret;
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
        x_TruncateDB();
        return;
    }

    unsigned delay = 0;
    unsigned bytes_written = 0;
    vector<SCacheDescr> cache_entries;
    cache_entries.reserve(1000);

    unsigned db_recs = 0;

    // Search the database for obsolete cache entries
    string first_key, last_key;
    for (bool flag = true; flag;) {
        unsigned batch_size = GetPurgeBatchSize();
        {{
            CFastMutexGuard guard(m_DB_Lock);
            delay = m_BatchSleep;

            CBDB_FileCursor cur(*m_CacheAttrDB);
            cur.SetCondition(CBDB_FileCursor::eGE);
            cur.From << last_key;

            time_t curr = time(0);

            for (unsigned i = 0; i < batch_size; ++i) {
                if (cur.Fetch() != eBDB_Ok) {
                    flag = false;
                    break;
                }

                int version = m_CacheAttrDB->version;
                const char* key = m_CacheAttrDB->key;
                last_key = key;
                int overflow = m_CacheAttrDB->overflow;
                const char* subkey = m_CacheAttrDB->subkey;
                unsigned blob_id = m_CacheAttrDB->blob_id;

                if (x_CheckTimestampExpired(curr)) {

                    unsigned read_count = m_CacheAttrDB->read_count;
                    m_CacheAttrDB->owner_name.ToString(m_TmpOwnerName);
                    if (0 == read_count) {
                        m_Statistics.AddNeverRead(m_TmpOwnerName);
                    }
                    m_Statistics.AddPurgeDelete(m_TmpOwnerName);
                    x_UpdateOwnerStatOnDelete(m_TmpOwnerName, 
                                              false/*non-expl-delete*/);

                    cache_entries.push_back(
                         SCacheDescr(key, version, subkey, overflow, blob_id));
                } else {
                    ++db_recs;
                }
                if (i == 0) { // first record in the batch
                    first_key = last_key;
                } else
                if (i == (batch_size - 1)) { // last record
                    // if batch stops in the same key we increase
                    // the batch size to avoid the infinite loop
                    // when new batch starts with the very same key
                    // and never comes to the end
                    if (first_key == last_key) {
                        batch_size += 10;
                    }
                }

            } // for i

            bool purge_stop = m_PurgeStopSignal.TryWait(0, delay);
            if (purge_stop) {
                LOG_POST(Warning << "BDB Cache: Stopping Purge execution.");
                return;
            }
            bytes_written = m_BytesWritten;
        }}

        if (bytes_written > m_CheckPointInterval) {
            m_Env->TransactionCheckpoint();
            {{
            CFastMutexGuard guard(m_DB_Lock);
            m_BytesWritten = 0;
            }}
        } else {
            bool purge_stop = m_PurgeStopSignal.TryWait(0, delay);
            if (purge_stop) {
                LOG_POST(Warning << "BDB Cache: Stopping Purge execution.");
                return;
            }
        }


    } // for flag


    // Delete BLOBs


    for (unsigned i = 0; i < cache_entries.size(); ) {
        unsigned batch_size = GetPurgeBatchSize();
        batch_size = batch_size / 2;
        if (batch_size == 0) {
            batch_size = 2;
        }

        {{
        CFastMutexGuard guard(m_DB_Lock);

        CCacheTransaction trans(*this);
        for (unsigned j = 0;
             (j < batch_size) && (i < cache_entries.size());
             ++i,++j) {
                 const SCacheDescr& it = cache_entries[i];
                 x_DropBlob(it.key.c_str(),
                            it.version,
                            it.subkey.c_str(),
                            it.overflow,
                            it.blob_id);

        } // for j
        trans.Commit();
        delay = m_BatchSleep;

        bool purge_stop = m_PurgeStopSignal.TryWait(0, 0);
        if (purge_stop) {
            LOG_POST(Warning << "BDB Cache: Stopping Purge execution.");
            return;
        }

        }}

        // temporarily unlock the mutex to let other threads go
        if (delay) {
            bool purge_stop = m_PurgeStopSignal.TryWait(0, delay);
            if (purge_stop) {
                LOG_POST(Warning << "BDB Cache: Stopping Purge execution.");
                return;
            }
        }

    } // for i

    ++m_PurgeCount;
    if (m_CleanLogOnPurge) {
        if ((m_PurgeCount % m_CleanLogOnPurge) == 0) {
            m_Env->CleanLog();
        }
    }


    {{
    CFastMutexGuard guard(m_DB_Lock);
    m_Statistics.GlobalStatistics().blobs_db = db_recs;
    }}

}


void CBDB_Cache::Purge(const string&    key,
                       const string&    subkey,
                       time_t           access_timeout,
                       EKeepVersions    keep_last_version)
{
    if (IsReadOnly()) {
        return;
    }


    CFastMutexGuard guard(m_DB_Lock);

    if (key.empty() && keep_last_version == eDropAll && access_timeout == 0) {
        x_TruncateDB();
        return;
    }

    // Search the database for obsolete cache entries
    vector<SCacheDescr> cache_entries;


    {{

    CBDB_FileCursor cur(*m_CacheAttrDB);
    cur.SetCondition(CBDB_FileCursor::eEQ);

    cur.From << key;

    //CTime time_stamp(CTime::eCurrent);
    time_t curr = time(0); // (int)time_stamp.GetTimeT();
    int timeout = GetTimeout();
    if (access_timeout && access_timeout < timeout) {
        timeout = access_timeout;
    }

    while (cur.Fetch() == eBDB_Ok) {
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

            unsigned read_count = m_CacheAttrDB->read_count;
            m_CacheAttrDB->owner_name.ToString(m_TmpOwnerName);
            if (0 == read_count) {
                m_Statistics.AddNeverRead(m_TmpOwnerName);
            }
            m_Statistics.AddPurgeDelete(m_TmpOwnerName);
            x_UpdateOwnerStatOnDelete(m_TmpOwnerName, 
                                      false/*non-expl-delete*/);
            

            continue;
        }
    } // while

    }}

    CCacheTransaction trans(*this);
    ITERATE(vector<SCacheDescr>, it, cache_entries) {
        x_DropBlob(it->key.c_str(),
                   it->version,
                   it->subkey.c_str(),
                   it->overflow,
                   it->blob_id);
    }

    trans.Commit();

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

    LOG_POST("Cache location: " + string(cache_path));

    string cache_blob_db_name =
       string("lcs_") + string(cache_name) + string("_blob")+ string(".db");
    string attr_db_name =
       string("lcs_") + string(cache_name) + string("_attr4") + string(".db");

    m_CacheBLOB_DB = new SCacheBLOB_DB();
    m_CacheBLOB_DB->SetEnv(*m_Env);

    LOG_POST("Running verification for: " + cache_blob_db_name);
    m_CacheBLOB_DB->Verify(cache_blob_db_name.c_str(), 0, 0);

    m_CacheAttrDB = new SCache_AttrDB();
    m_CacheAttrDB->SetEnv(*m_Env);

    string bak = m_Path + attr_db_name + ".bak";
    FILE* fl = fopen(bak.c_str(), "wb");

    LOG_POST("Running verification for: " + attr_db_name);
    m_CacheAttrDB->Verify(attr_db_name.c_str(), 0, fl);
    delete m_CacheAttrDB; m_CacheAttrDB = 0;

    fclose(fl);

    if (force_remove) {
        m_Env->ForceRemove();
    } else {
        bool deleted = m_Env->Remove();

        if (!deleted)
            LOG_POST("Cannot delete the environment (it is busy by another process)");
    }
    delete m_Env; m_Env = 0;
}

void CBDB_Cache::x_PerformCheckPointNoLock(unsigned bytes_written)
{
    m_BytesWritten += bytes_written;

    // if purge is running (in the background) it works as a checkpoint-er
    // so we don't need to call TransactionCheckpoint

    if ((m_RunPurgeThread == false) &&
         (m_BytesWritten > m_CheckPointInterval)) {

        m_Env->TransactionCheckpoint();
        m_BytesWritten = 0;
    }

}

bool CBDB_Cache::x_CheckTimestampExpired(time_t  curr)
{
    int timeout = GetTimeout();

    if (timeout) {

        int db_time_stamp = m_CacheAttrDB->time_stamp;
        unsigned int ttl = m_CacheAttrDB->ttl;

        if (ttl) {  // individual timeout
            if (m_MaxTimeout && ttl > m_MaxTimeout) {
                timeout =
                    (int) max((unsigned)timeout, (unsigned)m_MaxTimeout);
                ttl = timeout; // used in diagnostics only
            } else {
                timeout = ttl;
            }
        }

        if (curr - timeout > db_time_stamp) {
/*
            string key = m_CacheAttrDB->key;
            LOG_POST("local cache item expired:" << key << " "
                     << db_time_stamp << " curr=" << curr
                     << " diff=" << curr - db_time_stamp
                     << " ttl=" << ttl);
*/
            return true;
        }
    }
    return false;

}


bool CBDB_Cache::x_CheckTimestampExpired()
{
    time_t curr = time(0);
    return x_CheckTimestampExpired(curr);
}

void CBDB_Cache::x_UpdateReadAccessTime(const string&  key,
                                        int            version,
                                        const string&  subkey)
{
    x_UpdateAccessTime(key, version, subkey, eBlobRead);
}



void CBDB_Cache::x_UpdateAccessTime(const string&  key,
                                    int            version,
                                    const string&  subkey,
                                    EBlobAccessType access_type)
{
    if (IsReadOnly()) {
        return;
    }

    CCacheTransaction trans(*this);

	x_UpdateAccessTime_NonTrans(key, version, subkey, access_type);

    trans.Commit();
}




void CBDB_Cache::x_UpdateAccessTime_NonTrans(const string&  key,
                                             int            version,
                                             const string&  subkey,
                                             EBlobAccessType access_type)
{
    if (IsReadOnly()) {
        return;
    }
    time_t curr = time(0);
    x_UpdateAccessTime_NonTrans(key,
                                version,
                                subkey,
                                (unsigned)curr,
                                access_type);
}

void CBDB_Cache::x_UpdateAccessTime_NonTrans(const string&  key,
                                             int            version,
                                             const string&  subkey,
                                             unsigned       timeout,
                                             EBlobAccessType access_type)
{
    int track_sk = (m_TimeStampFlag & fTrackSubKey);
    bool updated = false;
    {{
        CBDB_FileCursor cur(*m_CacheAttrDB,
                             CBDB_FileCursor::eReadModifyUpdate);

        cur.SetCondition(CBDB_FileCursor::eEQ);
        cur.From << key
                 << version;

        // if we are not tracking subkeys we blindly update ALL subkeys
        // not allowing them to expire, otherwise just one exactl subkey
        if (track_sk)
            cur.From << subkey;

        while (cur.Fetch() == eBDB_Ok) {
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
                updated = true;
            }

            if (track_sk) {
                break;
            }

        } // while

    }}

    if (updated)
        return;

    // record not found: create
    const string& sk = track_sk ? subkey : kEmptyStr;

    m_CacheAttrDB->key = key;
    m_CacheAttrDB->version = version;
    m_CacheAttrDB->subkey = sk;
    m_CacheAttrDB->overflow = 0;
    m_CacheAttrDB->time_stamp = timeout;
    m_CacheAttrDB->ttl = 0;
    m_CacheAttrDB->upd_count = 0;
    m_CacheAttrDB->read_count = 0;
    m_CacheAttrDB->blob_id = 0;

    m_CacheAttrDB->Insert();
}


void CBDB_Cache::x_TruncateDB()
{
    if (IsReadOnly()) {
        return;
    }

    LOG_POST(Info << "CBDB_BLOB_Cache:: cache database truncated");
    m_CacheBLOB_DB->Truncate();
    m_CacheAttrDB->Truncate();

    // Scan the directory, delete overflow BLOBs
    // TODO: remove overflow files only matching cache specific name
    //       signatures. Since several caches may live in the same
    //       directory we may delete some "extra" files
    CDir dir(m_Path);
    string ext;
    string ov_(".ov_");
    if (dir.Exists()) {
        CDir::TEntries  content(dir.GetEntries());
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
										  int*           overflow,
                                          unsigned int*  ttl,
                                          unsigned int*  blob_id)
{
    _ASSERT(blob_id);

    bool ret = x_FetchBlobAttributes(key, version, subkey);
    if (ret == false) {
        return ret;
    }

	*overflow = m_CacheAttrDB->overflow;
    *ttl      = m_CacheAttrDB->ttl;
    *blob_id  = m_CacheAttrDB->blob_id;

	if (!(m_TimeStampFlag & fTrackSubKey)) {
	    m_CacheAttrDB->subkey = "";

		EBDB_ErrCode err = m_CacheAttrDB->Fetch();
		if (err != eBDB_Ok) {
			return false;
		}
	}
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



void CBDB_Cache::x_DropOverflow(const char*    key,
                                int            version,
                                const char*    subkey)
{
    string path;
    try {
        s_MakeOverflowFileName(path, m_Path, GetName(), key, version, subkey);
        x_DropOverflow(path);
    } catch (exception& ex) {  
        ERR_POST("Blob Store: Cannot remove file: " << path 
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
        ERR_POST("Blob Store: Cannot remove file: " << file_path 
                 << " " << ex.what());
    }
}

void CBDB_Cache::x_DropBlob(const char*    key,
                            int            version,
                            const char*    subkey,
                            int            overflow,
                            unsigned       blob_id)
{
    _ASSERT(key);
    _ASSERT(subkey);

    if (IsReadOnly()) {
        return;
    }

    if (overflow == 1) {
        x_DropOverflow(key, version, subkey);
    }

    // delete blob from m_CacheDB even if it is marked as "overflow"
    // old data maybe result of a BLOB grow from regular to overlow
    //

    if (blob_id) {
        m_CacheBLOB_DB->blob_id = blob_id;
        m_CacheBLOB_DB->Delete(CBDB_RawFile::eIgnoreError);
    }

    m_CacheAttrDB->key = key;
    m_CacheAttrDB->version = version;
    m_CacheAttrDB->subkey = subkey;

    m_CacheAttrDB->Delete(CBDB_RawFile::eIgnoreError);
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
    CFastMutexGuard guard(m_DB_Lock);

    m_Statistics.AddInternalError(owner, operation);
}

void CBDB_Cache::RegisterProtocolError(
                            SBDB_CacheUnitStatistics::EErrGetPut operation,
                            const string&                        owner)
{
    CFastMutexGuard guard(m_DB_Lock);

    m_Statistics.AddProtocolError(owner, operation);
}

void CBDB_Cache::RegisterNoBlobError(
                             SBDB_CacheUnitStatistics::EErrGetPut operation,
                             const string&                        owner)
{
    CFastMutexGuard guard(m_DB_Lock);
    m_Statistics.AddNoBlobError(owner, operation);
}

void CBDB_Cache::RegisterCommError(
                              SBDB_CacheUnitStatistics::EErrGetPut operation, 
                              const string&                        owner)
{
    CFastMutexGuard guard(m_DB_Lock);
    m_Statistics.AddCommError(owner, operation);
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

static const string kCFParam_path           = "path";
static const string kCFParam_name           = "name";
static const string kCFParam_page_size      = "page_size";

static const string kCFParam_lock           = "lock";
static const string kCFParam_lock_default   = "no_lock";
static const string kCFParam_lock_pid_lock  = "pid_lock";

static const string kCFParam_mem_size          = "mem_size";
static const string kCFParam_log_mem_size      = "log_mem_size";
static const string kCFParam_read_only         = "read_only";
static const string kCFParam_write_sync        = "write_sync";
static const string kCFParam_use_transactions  = "use_transactions";
static const string kCFParam_direct_db         = "direct_db";
static const string kCFParam_direct_log        = "direct_log";

static const string kCFParam_purge_batch_size  = "purge_batch_size";
static const string kCFParam_purge_batch_sleep  = "purge_batch_sleep";
static const string kCFParam_purge_clean_log    = "purge_clean_log";
static const string kCFParam_purge_thread       = "purge_thread";
static const string kCFParam_purge_thread_delay = "purge_thread_delay";
static const string kCFParam_checkpoint_bytes   = "checkpoint_bytes";
static const string kCFParam_log_file_max       = "log_file_max";
static const string kCFParam_overflow_limit     = "overflow_limit";
static const string kCFParam_ttl_prolong        = "ttl_prolong";
static const string kCFParam_owner_stat         = "owner_stat";




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
    const TCacheParams* path = params->FindNode(kCFParam_path);
    string str_path = path ?
        CDirEntry::AddTrailingPathSeparator(
        path->GetValue().value) : kEmptyStr;
    if (!path  || str_path != m_Path) {
        return false;
    }
    const TCacheParams* name = params->FindNode(kCFParam_name);
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
    const string& page_size =
        GetParam(params, kCFParam_page_size, false);

    CBDB_Cache::ELockMode lock = CBDB_Cache::eNoLock;
    if (NStr::CompareNocase(locking, kCFParam_lock_pid_lock) == 0) {
        lock = CBDB_Cache::ePidLock;
    }
    unsigned overflow_limit =
        GetParamDataSize(params, kCFParam_overflow_limit, false, 0);
    if (overflow_limit) {
        drv->SetOverflowLimit(overflow_limit);
    }

    unsigned mem_size =
        GetParamDataSize(params, kCFParam_mem_size, false, 0);

    unsigned log_mem_size =
        GetParamDataSize(params, kCFParam_log_mem_size, false, 0);

    if (!page_size.empty()) {
        if (NStr::CompareNocase(page_size, "small") == 0) {
            drv->SetPageSize(CBDB_Cache::eSmall);
        } // any other variant makes deafult (large) page size
    }

    unsigned checkpoint_bytes =
        GetParamDataSize(params, kCFParam_checkpoint_bytes,
                         false, 24 * 1024 * 1024);
    drv->SetCheckpoint(checkpoint_bytes);

    unsigned log_file_max =
        GetParamDataSize(params, kCFParam_log_file_max,
                         false, 200 * 1024 * 1024);
    drv->SetLogFileMax(log_file_max);


    bool ro =
        GetParamBool(params, kCFParam_read_only, false, false);

    bool w_sync =
        GetParamBool(params, kCFParam_write_sync, false, false);
    drv->SetWriteSync(w_sync ?
                        CBDB_Cache::eWriteSync : CBDB_Cache::eWriteNoSync);

    unsigned ttl_prolong =
        GetParamInt(params, kCFParam_ttl_prolong, false, 0);
    drv->SetTTL_Prolongation(ttl_prolong);


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
/*
    bool owner_stat =
        GetParamBool(params, kCFParam_owner_stat, false, false);
    drv->SetOwnerStat(owner_stat);
*/


    if (ro) {
        drv->OpenReadOnly(path.c_str(), name.c_str(), mem_size);
    } else {
        drv->Open(path, name,
                  lock, mem_size,
                  use_trans ? CBDB_Cache::eUseTrans : CBDB_Cache::eNoTrans,
                  log_mem_size);
    }

    if (!drv->IsJoinedEnv()) {
        bool direct_db =
            GetParamBool(params, kCFParam_direct_db, false, false);
        bool direct_log =
            GetParamBool(params, kCFParam_direct_log, false, false);

        CBDB_Env* env = drv->GetEnv();
        env->SetDirectDB(direct_db);
        env->SetDirectLog(direct_log);
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
            ICache::fTrackSubKey               |
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
