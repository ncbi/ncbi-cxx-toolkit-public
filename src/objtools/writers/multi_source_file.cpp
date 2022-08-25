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
* Authors:  Sergiy Gotvyanskyy
*
* File Description:
*   Asynchronous multi-source file writer
*
*/

#include <ncbi_pch.hpp>
#include <objtools/writers/multi_source_file.hpp>

#include <cassert>
#include <deque>
#include <mutex>
#include <condition_variable>
#include <streambuf>
#include <atomic>
#include <array>
#include <fstream>

//#include <iostream>

BEGIN_NCBI_SCOPE;
BEGIN_NAMESPACE(objects);

static constexpr size_t s_preallocated_buffers = 10;
static constexpr size_t s_default_max_writers  = 10;
static constexpr size_t s_default_buffer_size  = 64*1024;

class NCBI_XOBJWRITE_EXPORT CMultiSourceWriterImpl
{
public:
    using TBuffer = std::array<char, s_default_buffer_size>;

    CMultiSourceWriterImpl();
    ~CMultiSourceWriterImpl();
    void Open(const std::string& filename);
    void Open(std::ostream& o_stream);
    bool IsOpen() const;
    void Close();
    void Flush();

    void SetMaxWriters(size_t num) {
        m_max_writers = num;
    }
    CMultiSourceWriterImpl(const CMultiSourceWriterImpl&) = delete;
    CMultiSourceWriterImpl(CMultiSourceWriterImpl&&) = delete;

    std::shared_ptr<CMultiSourceOStreamBuf> NewStreamBuf();
    void FlushStreamBuf(CMultiSourceOStreamBuf* strbuf);
    std::unique_ptr<TBuffer> AllocateBuffer();
    void ReturnBuffer(std::unique_ptr<TBuffer>);
    void CloseStreamBuf(CMultiSourceOStreamBuf* strbuf);
private:
    std::deque<std::shared_ptr<CMultiSourceOStreamBuf>> m_writers;
    std::atomic<size_t> m_max_writers = s_default_max_writers;
    std::atomic<CMultiSourceOStreamBuf*> m_head;

    std::mutex m_mutex;
    std::condition_variable m_cv;

    std::mutex m_buffers_mutex;
    std::deque<std::unique_ptr<TBuffer>> m_reused_buffers;

    std::atomic<std::ostream*> m_ostream = nullptr;
    std::unique_ptr<std::ostream> m_own_stream;
};

class CMultiSourceOStreamBuf: public std::streambuf
{
public:
    using TBuffer = CMultiSourceWriterImpl::TBuffer;
    //friend class CMultiSourceWriterImpl;

    using _MyBase = std::streambuf;
    CMultiSourceOStreamBuf(CMultiSourceWriterImpl& parent);
    ~CMultiSourceOStreamBuf();
    void Close();
    void Dump(std::ostream& ostr);
protected:
    int_type overflow( int_type ch ) override;

private:
    CMultiSourceWriterImpl& m_parent;
    std::unique_ptr<TBuffer> m_buffer;
    std::atomic<int> m_leftover = -1;
};

CMultiSourceOStream::CMultiSourceOStream(CMultiSourceOStream&& _other) : _MyBase{std::move(_other)}, m_buf{_other.m_buf}
{ // note: std::ostream move constructor doesn't set rdbuf
    _other.m_buf = nullptr;
    _MyBase::set_rdbuf(m_buf.get());
    _other.set_rdbuf(nullptr);
}

CMultiSourceOStream& CMultiSourceOStream::operator=(CMultiSourceOStream&& _other)
{ // note: std::ostream move constructor doesn't set rdbuf

    if (&_other == this) return *this;

    close();

    _MyBase::operator=(std::move(_other));
    m_buf = std::move(_other.m_buf);

    _MyBase::set_rdbuf(m_buf.get());
    _other.set_rdbuf(nullptr);

    return *this;
}

CMultiSourceOStream::CMultiSourceOStream(std::shared_ptr<CMultiSourceOStreamBuf> buf) :
    _MyBase{buf.get()},
    m_buf{buf}
{}

void CMultiSourceOStream::close()
{
    if (m_buf) {
        _MyBase::set_rdbuf(nullptr);
        m_buf->Close();
        //std::cerr<< "CMultiSourceOStream::~CMultiSourceOStream() closing\n";
        m_buf.reset(); // this may not delete this because it's still hold in parent queue
    }
}

CMultiSourceOStream::~CMultiSourceOStream()
{
    //std::cerr<< "CMultiSourceOStream::~CMultiSourceOStream()\n";
    close();
}

CMultiSourceOStreamBuf::CMultiSourceOStreamBuf(CMultiSourceWriterImpl& parent): m_parent{parent} {}
CMultiSourceOStreamBuf::~CMultiSourceOStreamBuf()
{
    //std::cerr<< "CMultiSourceOStreamBuf::~CMultiSourceOStreamBuf()\n";
    m_parent.ReturnBuffer(std::move(m_buffer));
}

void CMultiSourceOStreamBuf::Close()
{
    if (m_leftover<0) {
        m_leftover = pptr() - pbase();
        m_parent.CloseStreamBuf(this);
    }
}

void CMultiSourceOStreamBuf::Dump(std::ostream& ostr)
{
    auto _pptr   = _MyBase::pptr();
    auto _pbase  = _MyBase::pbase();
    size_t size = _pptr - _pbase;
    if (size)
        ostr.write(_pbase, size);
}

std::ostream::int_type CMultiSourceOStreamBuf::overflow( int_type ch )
{
    if (m_buffer) {
        m_parent.FlushStreamBuf(this); // this can block & wait
    } else {
        m_buffer = m_parent.AllocateBuffer();
    }

    if (m_buffer) {
        auto head = m_buffer->data();
        setp(head, head + m_buffer->size());
        *head = ch;
        pbump(1);
        return ch;
    } else {
        setp(nullptr, nullptr);
        return _MyBase::overflow(ch);
    }
}

CMultiSourceWriterImpl::CMultiSourceWriterImpl() {}

CMultiSourceWriterImpl::~CMultiSourceWriterImpl()
{
    //std::cerr<< "CMultiSourceWriterImpl::~CMultiSourceWriterImpl()\n";
    Close();
}

void CMultiSourceWriterImpl::Open(const std::string& filename)
{
    auto new_stream = std::make_unique<std::ofstream>(filename);
    new_stream->exceptions( ios::failbit | ios::badbit );
    Open(*new_stream); // this may throw an exception
    m_own_stream = std::move(new_stream);
}


void CMultiSourceWriterImpl::Open(std::ostream& o_stream)
{
    std::ostream* expected = nullptr;
    if (!m_ostream.compare_exchange_strong(expected, &o_stream))
    {
        throw std::runtime_error("already open");
    }
}

bool CMultiSourceWriterImpl::IsOpen() const
{
    return m_ostream.load() != nullptr;
}

void CMultiSourceWriterImpl::Close()
{
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        m_cv.wait(lock, [this]
            {
                bool is_empty = m_writers.empty();
                return is_empty;
            });

        if (m_ostream) {
            m_ostream.load()->flush();
            m_ostream = nullptr;
        }
    }

    m_cv.notify_all();
}

void CMultiSourceWriterImpl::Flush()
{
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        //m_cv.wait(lock, [] { return true; });

        if (m_ostream) {
            m_ostream.load()->flush();
        }
    }

    //m_cv.notify_all();
}

std::shared_ptr<CMultiSourceOStreamBuf> CMultiSourceWriterImpl::NewStreamBuf()
{
    std::shared_ptr<CMultiSourceOStreamBuf> buf;
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        m_cv.wait(lock, [this]
            {
                return m_writers.size() < m_max_writers;
            });

        if (m_ostream == nullptr)
            throw runtime_error("CMultiSourceWriter is not open");

        buf = std::make_shared<CMultiSourceOStreamBuf>(*this);
        if (m_writers.empty()) {
            m_head = buf.get();
        }
        m_writers.push_back(buf);
    }

    m_cv.notify_all();

    return buf;
}

void CMultiSourceWriterImpl::ReturnBuffer(std::unique_ptr<CMultiSourceWriterImpl::TBuffer> buffer)
{
    std::unique_lock<std::mutex> lock(m_buffers_mutex);

    if (m_reused_buffers.size() < s_preallocated_buffers) {
        m_reused_buffers.push_back(std::move(buffer));
    }
}

std::unique_ptr<CMultiSourceWriterImpl::TBuffer> CMultiSourceWriterImpl::AllocateBuffer()
{
    std::unique_ptr<TBuffer> buffer;
    {
        std::unique_lock<std::mutex> lock(m_buffers_mutex);

        if (!m_reused_buffers.empty()) {
            buffer = std::move(m_reused_buffers.front());
            m_reused_buffers.pop_front();
        }
    }

    if (buffer)
        return buffer;
    else
        return make_unique<TBuffer>();
}

void CMultiSourceWriterImpl::FlushStreamBuf(CMultiSourceOStreamBuf* strbuf)
{
    //while (strbuf != m_head.load())
    // wait until we are ahead of all
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        m_cv.wait(lock, [this, strbuf]
            {
                bool is_head = strbuf == m_head.load();
                return is_head;
            });
    }
    strbuf->Dump(*m_ostream);

    // no need to notify
    // m_cv.notify_all();
}

void CMultiSourceWriterImpl::CloseStreamBuf(CMultiSourceOStreamBuf* strbuf)
{
    // TODO: if the streambuf is in the middle of queue, just leave it as is
    //if (strbuf == m_head.load())
    {
        {
        std::unique_lock<std::mutex> lock(m_mutex);
        m_cv.wait(lock, [this, strbuf]
            {
                bool is_head = strbuf == m_head.load();
                return is_head;
            });

        {
            strbuf->Dump(*m_ostream);
            m_writers.pop_front(); // this can destroy CMultiSourceOStreamBuf if it was closed in the middle of queue
            m_head = m_writers.empty() ? nullptr : m_writers.front().get();

            strbuf = m_head;
        }

        }
        m_cv.notify_all();
    }
}

CMultiSourceWriter::CMultiSourceWriter() {}
CMultiSourceWriter::~CMultiSourceWriter() {}

void CMultiSourceWriter::Open(const std::string& filename)
{
    x_ConstructImpl();
    m_impl->Open(filename);
}


void CMultiSourceWriter::Open(std::ostream& o_stream)
{
    x_ConstructImpl();
    m_impl->Open(o_stream);
}

void CMultiSourceWriter::Close()
{
    if (m_impl) m_impl->Close();
}

CMultiSourceOStream CMultiSourceWriter::NewStream()
{
    if (m_impl)
        return {m_impl->NewStreamBuf()};
    else
        throw std::runtime_error("CMultiSourceWriter is not configured");
}

std::unique_ptr<CMultiSourceOStream> CMultiSourceWriter::NewStreamPtr()
{
    if (m_impl)
        return std::make_unique<CMultiSourceOStream>(m_impl->NewStreamBuf());
    else
        throw std::runtime_error("CMultiSourceWriter is not configured");
}

void CMultiSourceWriter::Flush()
{
    if (m_impl) m_impl->Flush();
}

void CMultiSourceWriter::SetMaxWriters(size_t num)
{
    x_ConstructImpl();
    m_impl->SetMaxWriters(num);
}

bool CMultiSourceWriter::IsOpen() const
{
    if (!m_impl)
        return false;
    return m_impl->IsOpen();
}

void CMultiSourceWriter::x_ConstructImpl()
{
    if (!m_impl) {
        m_impl = make_unique<CMultiSourceWriterImpl>();
    }
}

END_NAMESPACE(objects);
END_NCBI_NAMESPACE;
