/* $Id$
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
 * Author:  Anton Lavrentiev
 *
 * File Description:
 *   Test CRWstream interface.
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbidiag.hpp>
#include <corelib/ncbitime.hpp>
#include <corelib/ncbi_process.hpp>
#include <corelib/rwstream.hpp>
#include <corelib/ncbi_test.hpp>

#include <stdlib.h>              // rand()

#include <common/test_assert.h>  // This header must go last


BEGIN_NCBI_SCOPE


const size_t kHugeBufsize  = 4 << 20; // 4MB
const size_t kReadBufsize  = 3000;
const size_t kWriteBufsize = 5000;
const size_t kMaxIOSize    = 7000;


class CMyReader : public virtual IReader {
public:
    CMyReader(const unsigned char* base, size_t size)
        : m_Base(base), m_Size(size), m_Pos(0), m_Eof(!size)
    { }

    virtual ERW_Result Read(void* buf, size_t count,
                            size_t* bytes_read = 0);

    virtual ERW_Result PendingCount(size_t* count);

    size_t GetPosition(void) const { return m_Pos;  }
    size_t GetSize(void)     const { return m_Size; }

    virtual ~CMyReader() { /*noop*/ }

protected:
    const unsigned char* m_Base;
    size_t               m_Size;
    size_t               m_Pos;
    bool                 m_Eof;
};


class CMyWriter : public virtual IWriter {
public:
    CMyWriter(unsigned char* base, size_t size)
        : m_Base(base), m_Size(size), m_Pos(0), m_Err(false)
    { }

    virtual ERW_Result Write(const void* buf, size_t count,
                             size_t*     bytes_written = 0);

    virtual ERW_Result Flush(void) { return m_Err ? eRW_Error : eRW_Success; }

    size_t GetPosition(void) const { return m_Pos;  }
    size_t GetSize(void)     const { return m_Size; }

    virtual ~CMyWriter() { /*noop*/ }

protected:
    unsigned char*       m_Base;
    size_t               m_Size;
    size_t               m_Pos;
    bool                 m_Err;
};


class CMyReaderWriter : public IReaderWriter,
                        public CMyReader,
                        public CMyWriter
{
public:
    CMyReaderWriter(unsigned char* base, size_t size)
        : CMyReader(base, 0), CMyWriter(base, size)
    { }

    virtual ERW_Result Write(const void* buf, size_t count,
                             size_t*     bytes_written = 0);

    virtual ERW_Result Flush(void);

    size_t GetRPosition(void) const { return CMyReader::GetPosition(); }
    size_t GetWPosition(void) const { return CMyWriter::GetPosition(); }
    size_t GetRSize(void)     const { return CMyReader::GetSize();     }
    size_t GetWSize(void)     const { return CMyWriter::GetSize();     }
};


ERW_Result CMyReader::Read(void* buf, size_t count,
                           size_t* bytes_read)
{
    ERW_Result result;
    size_t     n_read;

    _ASSERT(!count  ||  buf);
    if (m_Eof) {
        n_read = 0;
        result = eRW_Error;
    } else if (!count) {
        n_read = 0;
        result = eRW_Success;
    } else if (m_Pos < m_Size) {
        n_read = m_Size - m_Pos;
        assert(n_read > 0);
        if (n_read > count)
            n_read = count;
        size_t x_read = ((rand() & 7) == 3
                         ? rand() % n_read + 1
                         : n_read);
        memcpy(buf, m_Base + m_Pos, x_read);
        if (n_read > x_read) {
            n_read = x_read;
            result = eRW_Success;
        } else
            result = n_read < count ? eRW_Timeout : eRW_Success;
    } else {
        n_read = 0;
        m_Eof = true;
        result = eRW_Eof;
    }
    if ( bytes_read )
        *bytes_read = n_read;
    else if (n_read < count)
        result = eRW_Error;
    ERR_POST(Info << "Read  @"
             << setw(8) << m_Pos << " / "
             << setw(7) << m_Size << ": "
             << setw(5) << n_read << "/%"[n_read != count] << left
             << setw(5) << count  << " byte" << " s"[n_read != 1]
             << " -> " << g_RW_ResultToString(result));
    m_Pos += n_read;
    return result;
}


static bool s_IfPendingCount = false;


ERW_Result CMyReader::PendingCount(size_t* count)
{
    if (m_Eof) {
        *count = 0;
        return eRW_Error;
    } else {
        s_IfPendingCount = true;
        *count = m_Size - m_Pos;
        return eRW_Success;
    }
}


ERW_Result CMyWriter::Write(const void* buf, size_t count,
                            size_t* bytes_written)
{
    ERW_Result result;
    size_t  n_written;

    _ASSERT(!count  ||  buf);
    if (m_Err) {
        n_written = 0;
        result = eRW_Error;
    } else if (!count) {
        n_written = 0;
        result = eRW_Success;
    } else if (m_Pos < m_Size) {
        n_written = m_Size - m_Pos;
        assert(n_written > 0);
        if (n_written > count)
            n_written = count;
        size_t x_written = ((rand() & 7) == 5
                            ? rand() % n_written + 1
                            : n_written);
        memcpy(m_Base + m_Pos, buf, x_written);
        if (n_written > x_written) {
            n_written = x_written;
            result = eRW_Success;
        } else if (n_written < count) {
            m_Err  = true;
            result = eRW_Error;
        } else
            result = eRW_Success;
    } else {
        n_written = 0;
        m_Err  = true;
        result = eRW_Error;
    }
    if ( bytes_written )
        *bytes_written = n_written;
    else if (n_written < count)
        result = eRW_Error;
    ERR_POST(Info << "Write @"
             << setw(8) << m_Pos << " / "
             << setw(7) << m_Size << ": "
             << setw(5) << n_written << "/%"[n_written != count] << left
             << setw(5) << count     << " byte" << " s"[n_written != 1]
             << " -> " << g_RW_ResultToString(result));
    m_Pos += n_written;
    return result;
}


ERW_Result CMyReaderWriter::Write(const void* buf, size_t count,
                                  size_t* bytes_written)
{
    size_t x_written = 0;
    ERW_Result result = CMyWriter::Write(buf, count, &x_written);
    if (x_written) {
        CMyReader::m_Size = CMyWriter::m_Pos;
        m_Eof = false;
    }
    if ( bytes_written )
        *bytes_written = x_written;
    else if (x_written < count)
        result = eRW_Error;
    return result;
}


ERW_Result CMyReaderWriter::Flush(void)
{
    ERR_POST(Info << "Flush @"
             << setw(8) << CMyReader::m_Pos << " / "
             << setw(7) << CMyWriter::m_Pos);
    return CMyWriter::Flush();
}


END_NCBI_SCOPE


int main(int argc, char* argv[])
{
    USING_NCBI_SCOPE;

#ifdef NCBI_OS_MSWIN
    static char x_buf[4096];
    cerr.rdbuf()->pubsetbuf(x_buf, sizeof(x_buf));
    cerr.unsetf(ios_base::unitbuf);
#endif /*NCBI_OS_MSWIN*/

    SetDiagTrace(eDT_Enable);
    SetDiagPostLevel(eDiag_Info);
    SetDiagPostAllFlags((SetDiagPostAllFlags(eDPF_Default) & ~eDPF_All)
                        | eDPF_DateTime    | eDPF_Severity
                        | eDPF_OmitInfoSev | eDPF_ErrorID);
    SetDiagTraceAllFlags(SetDiagPostAllFlags(eDPF_Default));

    ERR_POST(Info << "Testing NCBI CRWStream API");

    CNcbiTest::SetRandomSeed();
    unsigned char* hugedata = new unsigned char[kHugeBufsize * 3];
    ERR_POST(Info << "Generating data: " << kHugeBufsize << " random bytes");

    for (size_t n = 0;  n < kHugeBufsize;  n += 2) {
        hugedata[n]                      = (unsigned char)(rand() & 0xFF);
        hugedata[n + 1]                  = (unsigned char)(rand() & 0xFF);
        hugedata[n + kHugeBufsize]       = (unsigned char) 0xDE;
        hugedata[n + kHugeBufsize   + 1] = (unsigned char) 0xAD;
        hugedata[n + kHugeBufsize*2]     = (unsigned char) 0xBE;
        hugedata[n + kHugeBufsize*2 + 1] = (unsigned char) 0xEF;
    }

    const size_t kIOSize = kHugeBufsize - rand() % kMaxIOSize;

    ERR_POST(Info << "Random I/O size for this test: " << kIOSize);

    ERR_POST(Info << "Reading the data and storing it with random I/O");

    CMyReader* rd = new CMyReader(hugedata,                kIOSize);
    CMyWriter* wr = new CMyWriter(hugedata + kHugeBufsize, kIOSize); 
    CRStream is(rd, kReadBufsize,  0, CRWStreambuf::fOwnReader);
    CWStream os(wr, kWriteBufsize, 0, CRWStreambuf::fOwnWriter);

    char* buf = new char[kMaxIOSize];

    size_t n_in = 0, n_out = 0;
    do {
        s_IfPendingCount = false;
        size_t     x_in = rand() % kMaxIOSize + 1;
        streamsize x_inavail = is.rdbuf()->in_avail();
        if (x_inavail < 0)
            x_inavail = 0;
        if (s_IfPendingCount  ||  !x_inavail) {
            ERR_POST(Info
                     << "Read:  " << setw(8) << x_in);
        } else {
            ERR_POST(Info
                     << "Read:  " << setw(8) << x_in
                     << " [ " << (size_t) x_inavail << " ]");
        }
        is.read(buf, x_in);
        _ASSERT(!is.bad());
        if (!(x_in = (size_t) is.gcount()))
            break;
        n_in += x_in;
        size_t x_out = 0;
        while (x_out < x_in) {
            size_t xx_out = rand() % (x_in - x_out) + 1;
            ERR_POST(Info
                     << "Write: " << setw(8) << xx_out << " / "
                     << x_out << '+' << (x_in - x_out));
            if (!os.write(buf + x_out, xx_out))
                break;
            x_out += xx_out;
        }
        if (x_out < x_in)
            break;
        n_out += x_out;
    } while (is.good());
    os.flush();

    delete[] buf;

    ERR_POST(Info
             << "Read:  " << setw(8) << n_in  << " / " << kIOSize
             << ";  position: " << rd->GetPosition() << '/' << rd->GetSize());
    ERR_POST(Info
             << "Write: " << setw(8) << n_out << " / " << kIOSize
             << ";  position: " << wr->GetPosition() << '/' << wr->GetSize());

    _ASSERT(kIOSize == n_in           &&
            kIOSize == rd->GetSize()  &&
            kIOSize == rd->GetPosition());
    _ASSERT(kIOSize == n_out          &&
            kIOSize == wr->GetSize()  &&
            kIOSize == wr->GetPosition());

    ERR_POST(Info << "Comparing the original with the collected data");

    for (size_t n = 0;  n < kIOSize;  ++n) {
        if (hugedata[n] != hugedata[n + kHugeBufsize])
            ERR_FATAL("Mismatch @ " << n);
    }

    ERR_POST(Info << "Checking tied I/O by reading the data after writing it");

    buf = (char*) hugedata + kHugeBufsize;
    memset(buf, '\xFF', kHugeBufsize);

    CMyReaderWriter* rw = new CMyReaderWriter(hugedata + kHugeBufsize,kIOSize);
    CRWStream io(rw, 2 * (kReadBufsize + kWriteBufsize),
                 0, CRWStreambuf::fOwnWriter/*for fun*/);

    n_out = n_in = 0;
    do {
        if ((rand() % 10 == 2  ||  n_out == n_in)  &&  n_out < kIOSize) {
            size_t x_out = rand() % kMaxIOSize + 1;
            if (x_out + n_out > kIOSize)
                x_out = kIOSize - n_out;
            {
                ERR_POST(Info
                         << "Write: " << setw(8) << x_out);
            }
            if (!io.write(buf - kHugeBufsize + n_out, x_out))
                break;
            n_out += x_out;
        }
        if (rand() % 10 == 4  &&  n_out > n_in) {
            s_IfPendingCount = false;
            size_t     x_in = (rand() & 1
                               ? n_out - n_in
                               : rand() % (n_out - n_in) + 1);
            streamsize x_inavail = io.rdbuf()->in_avail();
            if (x_inavail < 0)
                x_inavail = 0;
            if (s_IfPendingCount  ||  !x_inavail) {
                ERR_POST(Info
                         << "Read:  " << setw(8) << x_in);
            } else { 
                ERR_POST(Info
                         << "Read:  " << setw(8) << x_in
                         << " [ " << (size_t) x_inavail << " ]");
            }
            if (!io.read(buf + kHugeBufsize + n_in, x_in))
                break;
            n_in += x_in;
        }
        if (n_out >= kIOSize  &&  n_in >= kIOSize)
            break;
    } while (io.good());
    // io.flush(); // not needed as everything should have been read out

    ERR_POST(Info
             << "Read:  " << setw(8) << n_in  << " / " << kIOSize << ";  "
             << "position: " << rw->GetRPosition() << '/' << rw->GetRSize());
    ERR_POST(Info
             << "Write: " << setw(8) << n_out << " / " << kIOSize << ";  "
             << "position: " << rw->GetWPosition() << '/' << rw->GetWSize());

    _ASSERT(kIOSize == n_in            &&
            kIOSize == rw->GetRSize()  &&
            kIOSize == rw->GetRPosition());
    _ASSERT(kIOSize == n_out           &&
            kIOSize == rw->GetWSize()  &&
            kIOSize == rw->GetWPosition());

    ERR_POST(Info << "Comparing the original with the collected data");

    for (size_t n = 0;  n < kIOSize;  ++n) {
        if (hugedata[n] != hugedata[n + kHugeBufsize]  ||
            hugedata[n] != hugedata[n + kHugeBufsize*2]) {
            ERR_FATAL("Mismatch @ " << n);
        }
    }

    ERR_POST(Info << "Test completed successfully");

    delete[] hugedata;
    return 0;
}
