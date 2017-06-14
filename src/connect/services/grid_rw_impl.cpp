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

#include "grid_rw.hpp"

#include <corelib/rwstream.hpp>

#include <connect/ncbi_conn_reader_writer.hpp>

#include <connect/services/grid_rw_impl.hpp>
#include <connect/services/error_codes.hpp>

#include <util/transmissionrw.hpp>

#include <array>
#include <list>
#include <utility>
#include <sstream>
#include <thread>
#include <vector>


#define NCBI_USE_ERRCODE_X   ConnServ_ReadWrite

BEGIN_NCBI_SCOPE

static const char s_JobOutputPrefixEmbedded[] = "D ";
static const char s_JobOutputPrefixNetCache[] = "K ";

#define JOB_OUTPUT_PREFIX_LEN 2

struct SDataDirectWriter : IEmbeddedStreamWriter
{
    SDataDirectWriter(const CNetScheduleJob& job);

    ERW_Result Write(const void* buf, size_t count, size_t* bytes_written = 0) override;
    ERW_Result Flush() override;

    void Close() override;
    void Abort() override;

private:
    bool Connect();

    enum { Created, Connected, Off } m_State;
    CListeningSocket m_ListeningSocket;
    CSocket m_OutputSocket;
    unique_ptr<CTransmissionWriter> m_TransmissionWriter;
};

SDataDirectWriter::SDataDirectWriter(const CNetScheduleJob& job) :
    m_State(Off)
{
    if (m_ListeningSocket.Listen(0) != eIO_Success) return;

    auto local_host = CSocketAPI::GetLocalHostAddress();
    auto local_port = m_ListeningSocket.GetPort(eNH_HostByteOrder);

    auto other_host = job.submitter.first;
    auto other_port = job.submitter.second;

    if (other_host.empty() || !other_port) return;

    ostringstream oss;
    oss << "job_key=" << job.job_id << "&job_status=ReadyToWrite&worker_node_host=" << local_host <<
        "&worker_node_port=" << local_port;

    CDatagramSocket udp_socket;
    auto buf = oss.str();

    if (udp_socket.Send(buf.data(), buf.size(), other_host, other_port) != eIO_Success) return;

    m_State = Created;
}

bool SDataDirectWriter::Connect()
{
    STimeout timeout{ 0, 1 };

    if (m_ListeningSocket.Accept(m_OutputSocket, &timeout) != eIO_Success) return false;

    m_ListeningSocket.Close();
    m_OutputSocket.SetCork();
    m_OutputSocket.DisableOSSendDelay();

    auto socket_writer = new CSocketReaderWriter(&m_OutputSocket, eNoOwnership, eIO_WritePlain);
    m_TransmissionWriter.reset(new CTransmissionWriter(socket_writer, eTakeOwnership));
    m_State = Connected;
    return true;
}

ERW_Result SDataDirectWriter::Write(const void* buf, size_t count, size_t* bytes_written)
{
    if (m_State == Off) return eRW_Timeout;

    if ((m_State != Connected) && !Connect()) return eRW_Timeout;

    if (m_TransmissionWriter->Write(buf, count, bytes_written) == eRW_Success) return eRW_Success;

    Abort();
    return eRW_Error;
}

ERW_Result SDataDirectWriter::Flush()
{
    if (m_State != Connected) return eRW_Success;

    if (m_TransmissionWriter->Flush() == eRW_Success) return eRW_Success;

    Abort();
    return eRW_Error;
}

void SDataDirectWriter::Close()
{
    if (m_State != Connected) return;

    m_TransmissionWriter->SetSendEof(CTransmissionWriter::eSendEofPacket);
    Abort();
}

void SDataDirectWriter::Abort()
{
    if (m_State != Connected) return;

    m_TransmissionWriter->Close();
    m_OutputSocket.SetCork(false);

    m_TransmissionWriter.reset();
}

struct SMultiWriter : IEmbeddedStreamWriter
{
    SMultiWriter(initializer_list<IEmbeddedStreamWriter*> init);
    ERW_Result Write(const void* buf, size_t count, size_t* bytes_written = 0) override;
    ERW_Result Flush() override;

    void Close() override;
    void Abort() override;

private:
    using TBuffer = vector<char>;
    using TWriter = unique_ptr<IEmbeddedStreamWriter>;
    using TWriterPos = size_t;
    using TWriterInfo = pair<TWriter, TWriterPos>;
    using TWriters = list<TWriterInfo>;

    template <class TAction>
    ERW_Result Do(bool all_writers, TAction action);

    ERW_Result WriteImpl(TWriters::iterator i);

    TBuffer m_Buffer;
    TWriters m_Writers;
};

SMultiWriter::SMultiWriter(initializer_list<IEmbeddedStreamWriter*> init)
{
    m_Buffer.reserve(16 * 1024 * 1024);

    for (auto p : init) {
        m_Writers.emplace_back(TWriter(p), 0);
    }
}

template <class TAction>
ERW_Result SMultiWriter::Do(bool all_writers, TAction action)
{
    bool any_success = false;
    auto i = m_Writers.begin();

    while (i != m_Writers.end()) {
        try {
            switch (action(i)) {
            case eRW_Success:
                if (!all_writers) return eRW_Success;

                any_success = true;
                /* FALL THROUGH */

            case eRW_Timeout:
                ++i;
                continue;

            default:
                break;
            }
        }
        catch (...) {
        }

        i = m_Writers.erase(i);
    }

    return m_Writers.empty() ? eRW_Error : (any_success ? eRW_Success : eRW_Timeout);
}

ERW_Result SMultiWriter::Write(const void* buf, size_t count, size_t* bytes_written)
{
    if (bytes_written) *bytes_written = count;
    if (!count) return eRW_Success;

    m_Buffer.insert(m_Buffer.end(), static_cast<const char*>(buf), static_cast<const char*>(buf) + count);

    return Do(false, [&](TWriters::iterator i) {
        return WriteImpl(i);
    });
}

ERW_Result SMultiWriter::WriteImpl(TWriters::iterator i)
{
    array<char, 1024 * 1024> buf;

    for (;;) {
        auto start = m_Buffer.cbegin() + i->second;
        const size_t max = distance(start, m_Buffer.cend());
        size_t to_write = min(buf.size(), max);

        if (!to_write) return eRW_Success;

        copy_n(start, to_write, buf.begin());

        char* data = buf.data();
        size_t written = 0;

        while (to_write) {
        auto result = i->first->Write(data, to_write, &written);

            switch (result) {
            case eRW_Success:
                data += written;
                to_write -= written;
                i->second += written;
                continue;

            default:
                return result;
            }
        }
    }
}

ERW_Result SMultiWriter::Flush()
{
    return Do(false, [](TWriters::iterator i) {
        return i->first->Flush();
    });
}

void SMultiWriter::Close()
{
    Do(true, [&](TWriters::iterator i) {
        WriteImpl(i);
        i->first->Close();
        return eRW_Success;
    });
}

void SMultiWriter::Abort()
{
    Do(true, [](TWriters::iterator i) {
        i->first->Abort();
        return eRW_Success;
    });
}

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

CNcbiOstream& SGridWrite::operator()(CNetCacheAPI nc_api, size_t embedded_max_size, bool direct_output, CNetScheduleJob& job)
{
    if (!direct_output) return operator()(nc_api, embedded_max_size, job.output);

    using TWriter = unique_ptr<IEmbeddedStreamWriter>;

    auto l = [nc_api, &job](string& key) mutable {
        TWriter direct_writer(new SDataDirectWriter(job));
        TWriter nc_writer(nc_api.PutData(&key));
        TWriter multi_writer(new SMultiWriter{direct_writer.get(), nc_writer.get()});

        direct_writer.release();
        nc_writer.release();
        return multi_writer.release();
    };

    writer.reset(new CStringOrWriter(embedded_max_size, job.output, l));

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

#ifdef NCBI_THREADS
SOutputCopy::SOutputCopy(IReader& reader, CNcbiOstream& writer)
{
    d.reserve(16 * 1024 * 1024);
    thread t(&SOutputCopy::Reader, this, ref(reader));
    Writer(writer);
    t.join();
}

void SOutputCopy::Reader(IReader& reader)
{
    array<char, 1024 * 1024> buf;
    bool eof = false;
    char* data = buf.data();
    size_t to_read = buf.size();

    auto copy = [&](unique_lock<mutex>& lock) {
        size_t read = buf.size() - to_read;
        d.insert(d.end(), buf.cbegin(), buf.cbegin() + read);

        data = buf.data();
        to_read = buf.size();

        if (done) eof = true;

        done = eof;
        lock.unlock();
        cv.notify_one();
    };

    while (!eof) {
        if (to_read) {
            ERW_Result status = eRW_Error;
            size_t bytes_read = 0;

            try {
                status = reader.Read(data, to_read, &bytes_read);
            }
            catch (...) {
            }

            switch (status) {
                case eRW_Success:
                    break;

                case eRW_Eof:
                    eof = true;
                    break;

                default:
                    eof = true;
                    bytes_read = 0;
                    break;
            }

            data += bytes_read;
            to_read -= bytes_read;
        }

        if (eof || !to_read) {
            unique_lock<mutex> lock(m);
            copy(lock);
        } else {
            unique_lock<mutex> lock(m, try_to_lock);
            if (lock.owns_lock()) copy(lock);
        }
    }
}

void SOutputCopy::Writer(CNcbiOstream& writer)
{
    array<char, 1024 * 1024> buf;
    bool eof = false;
    size_t copied = 0;

    while (!eof) {
        char* data = buf.data();
        size_t to_write = 0;

        {
            unique_lock<mutex> lock(m);

            if (!writer.good()) {
                done = true;
                return;
            }

            cv.wait(lock, [&]{ return d.size() > copied || done; });

            to_write = min(buf.size(), d.size() - copied);

            if (to_write) {
                copy_n(d.cbegin() + copied, to_write, buf.begin());
                copied += to_write;
            }

            if (d.size() == copied) eof = done;
        }

        try {
            writer.write(data, to_write);
        }
        catch (...) {
            writer.setstate(ios_base::badbit);
        }
    }
}
#endif

END_NCBI_SCOPE
