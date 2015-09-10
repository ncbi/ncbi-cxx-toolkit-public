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
 *   Government have not placed any restriction on its use or reproduction.
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
 * Authors:  Maxim Didenko, Dmitry Kazimirov
 *
 * File Description:
 *
 */

#include <ncbi_pch.hpp>

#include <connect/services/grid_rw_impl.hpp>
#include <connect/services/error_codes.hpp>


#define NCBI_USE_ERRCODE_X   ConnServ_ReadWrite

BEGIN_NCBI_SCOPE

static const char s_JobOutputPrefixEmbedded[] = "D ";
static const char s_JobOutputPrefixNetCache[] = "K ";

#define JOB_OUTPUT_PREFIX_LEN 2

CStringOrBlobStorageWriter::CStringOrBlobStorageWriter(size_t max_string_size,
        SNetCacheAPIImpl* storage, string& job_output_ref) :
    m_Storage(storage), m_Data(job_output_ref), m_MaxBuffSize(max_string_size)
{
    job_output_ref = s_JobOutputPrefixEmbedded;
}

ERW_Result CStringOrBlobStorageWriter::Write(const void* buf,
                                             size_t      count,
                                             size_t*     bytes_written)
{
    if (m_NetCacheWriter.get())
        return m_NetCacheWriter->Write(buf, count, bytes_written);

    if (m_Data.size() + count <= m_MaxBuffSize) {
        m_Data.append((const char*) buf, count);

        if (bytes_written != NULL)
            *bytes_written = count;
        return eRW_Success;
    }

    string key;
    m_NetCacheWriter.reset(m_Storage.PutData(&key));

    if (m_Data.size() > JOB_OUTPUT_PREFIX_LEN) {
        ERW_Result ret = m_NetCacheWriter->Write(
            m_Data.data() + JOB_OUTPUT_PREFIX_LEN,
            m_Data.size() - JOB_OUTPUT_PREFIX_LEN);

        if (ret != eRW_Success) {
            m_NetCacheWriter.reset(NULL);
            return ret;
        }
    }

    m_Data = s_JobOutputPrefixNetCache + key;

    return m_NetCacheWriter->Write(buf, count, bytes_written);
}

ERW_Result CStringOrBlobStorageWriter::Flush(void)
{
    return m_NetCacheWriter.get() ? m_NetCacheWriter->Flush() : eRW_Success;
}

void CStringOrBlobStorageWriter::Close()
{
    if (m_NetCacheWriter.get())
        m_NetCacheWriter->Close();
}

void CStringOrBlobStorageWriter::Abort()
{
    if (m_NetCacheWriter.get())
        m_NetCacheWriter->Abort();
}


////////////////////////////////////////////////////////////////////////////
//

CStringOrBlobStorageReader::CStringOrBlobStorageReader(const string& data_or_key,
        SNetCacheAPIImpl* storage, size_t* data_size) :
    m_Storage(storage), m_Data(data_or_key)
{
    if (NStr::CompareCase(data_or_key, 0, JOB_OUTPUT_PREFIX_LEN,
            s_JobOutputPrefixNetCache) == 0) {

        // If NetCache API is not provided, initialize it using info from key
        if (!m_Storage) {
            CNetCacheKey key(data_or_key.substr(JOB_OUTPUT_PREFIX_LEN));
            string service(key.GetServiceName());

            if (service.empty()) {
                service = key.GetHost() + ":" + NStr::UIntToString(key.GetPort());
            }

            m_Storage = CNetCacheAPI(service, kEmptyStr);
            m_Storage.GetService().GetServerPool().StickToServer(
                    key.GetHost(), key.GetPort());
        }

        m_NetCacheReader.reset(m_Storage.GetReader(
            data_or_key.data() + JOB_OUTPUT_PREFIX_LEN, data_size));
    } else if (NStr::CompareCase(data_or_key, 0,
            JOB_OUTPUT_PREFIX_LEN, s_JobOutputPrefixEmbedded) == 0) {
        m_BytesToRead = data_or_key.size() - JOB_OUTPUT_PREFIX_LEN;
        if (data_size != NULL)
            *data_size = m_BytesToRead;
    } else if (data_or_key.empty()) {
        m_BytesToRead = 0;
        if (data_size != NULL)
            *data_size = 0;
    } else {
        NCBI_THROW_FMT(CStringOrBlobStorageRWException, eInvalidFlag,
            "Unknown data type \"" <<
                string(data_or_key.begin(), data_or_key.begin() +
                    (data_or_key.size() < JOB_OUTPUT_PREFIX_LEN ?
                    data_or_key.size() : JOB_OUTPUT_PREFIX_LEN)) << '"');
    }
}

ERW_Result CStringOrBlobStorageReader::Read(void*   buf,
                                            size_t  count,
                                            size_t* bytes_read)
{
    if (m_NetCacheReader.get())
        return m_NetCacheReader->Read(buf, count, bytes_read);

    if (m_BytesToRead == 0) {
        if (bytes_read != NULL)
            *bytes_read = 0;
        return eRW_Eof;
    }

    if (count > m_BytesToRead)
        count = m_BytesToRead;
    memcpy(buf, &*(m_Data.end() - m_BytesToRead), count);
    m_BytesToRead -= count;
    if (bytes_read != NULL)
        *bytes_read = count;
    return eRW_Success;
}

ERW_Result CStringOrBlobStorageReader::PendingCount(size_t* count)
{
    if (m_NetCacheReader.get())
        return m_NetCacheReader->PendingCount(count);

    *count = m_BytesToRead;
    return eRW_Success;
}

END_NCBI_SCOPE
