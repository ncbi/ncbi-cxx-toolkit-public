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

#include <connect/services/grid_rw_impl.hpp>
#include <connect/services/error_codes.hpp>

#include <array>
#include <deque>
#include <list>
#include <memory>
#include <utility>


#define NCBI_USE_ERRCODE_X   ConnServ_ReadWrite

BEGIN_NCBI_SCOPE

static const char s_JobOutputPrefixEmbedded[] = "D ";
static const char s_JobOutputPrefixNetCache[] = "K ";

#define JOB_OUTPUT_PREFIX_LEN 2

struct SMultiWriter : IEmbeddedStreamWriter
{
    SMultiWriter(initializer_list<IEmbeddedStreamWriter*> init);
    ERW_Result Write(const void* buf, size_t count, size_t* bytes_written = 0) override;
    ERW_Result Flush() override;

    void Close() override;
    void Abort() override;

private:
    using TBuffer = deque<char>;
    using TWriter = unique_ptr<IEmbeddedStreamWriter>;
    using TWriterPos = size_t;
    using TWriterInfo = pair<TWriter, TWriterPos>;
    using TWriters = list<TWriterInfo>;

    template <class TAction>
    void Do(TAction action);

    TBuffer m_Buffer;
    TWriters m_Writers;
};

SMultiWriter::SMultiWriter(initializer_list<IEmbeddedStreamWriter*> init)
{
    for (auto p : init) {
        m_Writers.emplace_back(TWriter(p), 0);
    }
}

template <class TAction>
void SMultiWriter::Do(TAction action)
{
    auto i = m_Writers.begin();

    while (i != m_Writers.end()) {
        try {
            if (action(i)) {
                ++i;
                continue;
            }
        }
        catch (...) {
        }

        i = m_Writers.erase(i);
    }
}

ERW_Result SMultiWriter::Write(const void* buf, size_t count, size_t* bytes_written)
{
    if (bytes_written) *bytes_written = count;
    if (!count) return eRW_Success;

    m_Buffer.insert(m_Buffer.end(), static_cast<const char*>(buf), static_cast<const char*>(buf) + count);
    bool all_failed = true;

    Do([&](TWriters::iterator i) {
        array<char, 1024 * 1024> buf;

        for (;;) {
            auto start = m_Buffer.cbegin() + i->second;
            const size_t max = distance(start, m_Buffer.cend());
            size_t to_write = min(buf.size(), max);

            if (!to_write) return true;

            copy_n(start, to_write, buf.begin());

            char* data = buf.data();
            size_t written = 0;

            while (to_write) {
                switch (i->first->Write(data, to_write, &written)) {
                case eRW_Success:
                    all_failed = false;
                    data += written;
                    to_write -= written;
                    i->second += written;
                    continue;

                case eRW_Timeout:
                    return true;

                default:
                    return false;
                }
            }
        }
    });

    return all_failed ? eRW_Error : eRW_Success;
}

ERW_Result SMultiWriter::Flush()
{
    Do([](TWriters::iterator i) {
        return i->first->Flush() != eRW_Error;
    });

    return m_Writers.empty() ? eRW_Error : eRW_Success;
}

void SMultiWriter::Close()
{
    Do([](TWriters::iterator i) {
        i->first->Close();
        return true;
    });
}

void SMultiWriter::Abort()
{
    Do([](TWriters::iterator i) {
        i->first->Abort();
        return true;
    });
}

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
