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

#include <corelib/rwstream.hpp>

#include <connect/ncbi_conn_reader_writer.hpp>

#include <connect/services/grid_rw_impl.hpp>
#include <connect/services/error_codes.hpp>


#define NCBI_USE_ERRCODE_X   ConnServ_ReadWrite

BEGIN_NCBI_SCOPE

static const char s_JobOutputPrefixEmbedded[] = "D ";
static const char s_JobOutputPrefixNetCache[] = "K ";

#define JOB_OUTPUT_PREFIX_LEN 2

CStringOrWriter::CStringOrWriter(size_t max_data_size, string& data_ref, TWriterCreate writer_create) :
    m_MaxDataSize(max_data_size),
    m_Data(data_ref),
    m_WriterCreate(writer_create)
{
    m_Data = s_JobOutputPrefixEmbedded;
}

ERW_Result CStringOrWriter::Write(const void* buf, size_t count, size_t* bytes_written)
{
    if (m_Writer) return m_Writer->Write(buf, count, bytes_written);

    if (m_Data.size() + count <= m_MaxDataSize) {
        m_Data.append((const char*) buf, count);

        if (bytes_written) *bytes_written = count;
        return eRW_Success;
    }

    string key;
    unique_ptr<IEmbeddedStreamWriter> writer(m_WriterCreate(key));

    if (!writer) return eRW_Error;

    if (m_Data.size() > JOB_OUTPUT_PREFIX_LEN) {
        ERW_Result ret = writer->Write(
            m_Data.data() + JOB_OUTPUT_PREFIX_LEN,
            m_Data.size() - JOB_OUTPUT_PREFIX_LEN);

        if (ret != eRW_Success) return ret;
    }

    m_Data = s_JobOutputPrefixNetCache + key;

    m_Writer = move(writer);
    return m_Writer->Write(buf, count, bytes_written);
}

ERW_Result CStringOrWriter::Flush(void)
{
    return m_Writer ? m_Writer->Flush() : eRW_Success;
}

void CStringOrWriter::Close()
{
    if (m_Writer) m_Writer->Close();
}

void CStringOrWriter::Abort()
{
    if (m_Writer) m_Writer->Abort();
}

CStringOrWriter::TWriterCreate s_NetCacheWriterCreate(CNetCacheAPI api)
{
    return [api](string& key) mutable { return api.PutData(&key); };
}

CStringOrBlobStorageWriter::CStringOrBlobStorageWriter(size_t max_string_size,
        SNetCacheAPIImpl* storage, string& job_output_ref) :
    CStringOrWriter(max_string_size, job_output_ref, s_NetCacheWriterCreate(storage))
{
}

CNcbiOstream& SGridWrite::operator()(CNetCacheAPI nc_api, size_t embedded_max_size, string& data)
{
    writer.reset(new CStringOrBlobStorageWriter(embedded_max_size, nc_api, data));

    stream.reset(new CWStream(writer.get(), 0, 0, CRWStreambuf::fLeakExceptions));
    stream->exceptions(IOS_BASE::badbit | IOS_BASE::failbit);

    return *stream;
}

void SGridWrite::Reset(bool flush)
{
    if (flush && stream) stream->flush();
    stream.reset();

    if (writer) {
        writer->Close();
        writer.reset();
    }
}

////////////////////////////////////////////////////////////////////////////
//

CStringOrBlobStorageReader::EType CStringOrBlobStorageReader::x_GetDataType(
        string& data)
{
    if (NStr::CompareCase(data, 0, JOB_OUTPUT_PREFIX_LEN,
                s_JobOutputPrefixNetCache) == 0) {
        data.erase(0, JOB_OUTPUT_PREFIX_LEN);
        return eNetCache;
    }

    if (NStr::CompareCase(data, 0, JOB_OUTPUT_PREFIX_LEN,
                s_JobOutputPrefixEmbedded) == 0) {
        data.erase(0, JOB_OUTPUT_PREFIX_LEN);
        return eEmbedded;
    }

    return data.empty() ? eEmpty : eRaw;
}

CStringOrBlobStorageReader::CStringOrBlobStorageReader(const string& data_or_key,
        SNetCacheAPIImpl* storage, size_t* data_size) :
    m_Storage(storage), m_Data(data_or_key)
{
    switch (x_GetDataType(m_Data)) {
    case eNetCache:
        // If NetCache API is not provided, initialize it using info from key
        if (!m_Storage) {
            CNetCacheKey key(m_Data);
            string service(key.GetServiceName());

            if (service.empty()) {
                service = key.GetHost() + ":" + NStr::UIntToString(key.GetPort());
            }

            m_Storage = CNetCacheAPI(service, kEmptyStr);
            m_Storage.GetService().GetServerPool().StickToServer(
                    key.GetHost(), key.GetPort());
        }

        m_NetCacheReader.reset(m_Storage.GetReader(m_Data, data_size));
        return;

    case eEmbedded:
    case eEmpty:
        m_BytesToRead = m_Data.size();
        if (data_size != NULL)
            *data_size = m_BytesToRead;
        return;

    default:
        NCBI_THROW_FMT(CStringOrBlobStorageRWException, eInvalidFlag,
                "Unknown data type \"" <<
                m_Data.substr(0, JOB_OUTPUT_PREFIX_LEN) << '"');
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

CNcbiIstream& SGridRead::operator()(CNetCacheAPI nc_api, const string& data, size_t* data_size)
{
    auto reader = new CStringOrBlobStorageReader(data, nc_api, data_size);

    stream.reset(new CRStream(reader, 0, 0, CRWStreambuf::fOwnReader | CRWStreambuf::fLeakExceptions));
    stream->exceptions(IOS_BASE::badbit | IOS_BASE::failbit);

    return *stream;
}

void SGridRead::Reset()
{
    stream.reset();
}

END_NCBI_SCOPE
