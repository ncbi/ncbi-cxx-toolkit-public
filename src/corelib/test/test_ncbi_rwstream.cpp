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
#include <stdlib.h>                // atoi() & rand()

#include <common/test_assert.h>  /* This header must go last */


BEGIN_NCBI_SCOPE


const size_t kHugeBufsize  = 4 << 20; // 4MB
const size_t kReadBufsize  = 3000;
const size_t kWriteBufsize = 5000;
const size_t kMaxIOSize    = 7000;


class CMyReader : public virtual IReader {
public:
    CMyReader(const unsigned char* base, size_t size)
        : m_Base(base), m_Size(size), m_Pos(0), m_Eof(false) { }

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
        : m_Base(base), m_Size(size), m_Pos(0), m_Err(false) { }

    virtual ERW_Result Write(const void* buf, size_t count,
                             size_t*     bytes_written = 0);

    virtual ERW_Result Flush(void) { return m_Err ? eRW_Error : eRW_Success; }

    size_t GetPosition(void) const { return m_Pos;  }
    size_t GetSize(void)     const { return m_Size; }

    virtual ~CMyWriter() { /*noop*/ }

protected:
    unsigned char* m_Base;
    size_t         m_Size;
    size_t         m_Pos;
    bool           m_Err;
};


class CMyReaderWriter : public IReaderWriter,
                        public CMyReader,
                        public CMyWriter
{
public:
    CMyReaderWriter(unsigned char* base, size_t size)
        : CMyReader(base, 0), CMyWriter(base, size)
    { }

    virtual ERW_Result Read(void* buf, size_t count,
                            size_t* bytes_read = 0);

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
    if (!m_Eof  &&  !count) {
        n_read = 0;
        result = eRW_Success;
    } else if (m_Pos < m_Size) {
        n_read = m_Size - m_Pos;
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
            result = n_read < count ? eRW_Eof : eRW_Success;
    } else {
        n_read = 0;
        if (!m_Eof) {
            m_Eof = true;
            result = eRW_Eof;
        } else
            result = eRW_Error;
    }
    if ( bytes_read )
        *bytes_read = n_read;
    ERR_POST(Info << "Read  @"
             << setw(8) << m_Pos << '/'
             << setw(7) << m_Size << ": "
             << setw(5) << n_read << "/%"[n_read != count] << left
             << setw(5) << count  << " byte" << " s"[n_read != 1]
             << " -> " << g_RW_ResultToString(result));
    m_Pos += n_read;
    return result;
}


ERW_Result CMyReader::PendingCount(size_t* count)
{
    if (m_Eof) {
        *count = 0;
        return eRW_Error;
    } else {
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
    if (!m_Err  &&  !count) {
        n_written = 0;
        result = eRW_Success;
    } else if (m_Pos < m_Size) {
        n_written = m_Size - m_Pos;
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
    ERR_POST(Info << "Write @"
             << setw(8) << m_Pos << '/'
             << setw(7) << m_Size << ": "
             << setw(5) << n_written << "/%"[n_written != count] << left
             << setw(5) << count     << " byte" << " s"[n_written != 1]
             << " -> " << g_RW_ResultToString(result));
    m_Pos += n_written;
    return result;
}


ERW_Result CMyReaderWriter::Read(void* buf, size_t count,
                                 size_t* bytes_read)
{
    ERW_Result result = CMyReader::Read(buf, count, bytes_read);
    if (result == eRW_Eof  &&  !m_Err)
        result  = eRW_Error;
    return result;
}


ERW_Result CMyReaderWriter::Write(const void* buf, size_t count,
                                  size_t* bytes_written)
{
    size_t x_written;
    ERW_Result result = CMyWriter::Write(buf, count, &x_written);
    if (x_written) {
        CMyReader::m_Size = CMyWriter::m_Pos;
        m_Eof = false;
    }
    if ( bytes_written )
        *bytes_written = x_written;
    return result;
}


ERW_Result CMyReaderWriter::Flush(void)
{
    ERR_POST(Info << "Flush @"
             << setw(8) << CMyReader::m_Pos << '/'
             << setw(7) << CMyWriter::m_Pos);
    return CMyWriter::Flush();
}


END_NCBI_SCOPE


int main(int argc, char* argv[])
{
    USING_NCBI_SCOPE;

    SetDiagTrace(eDT_Enable);
    SetDiagPostLevel(eDiag_Info);
    SetDiagPostAllFlags((SetDiagPostAllFlags(eDPF_Default) & ~eDPF_All)
                        | eDPF_DateTime    | eDPF_Severity
                        | eDPF_OmitInfoSev | eDPF_ErrorID);
    SetDiagTraceAllFlags(SetDiagPostAllFlags(eDPF_Default));

    ERR_POST(Info << "Testing NCBI CRWStream API");

    int seed;
    if (argc == 2) {
        seed = atoi(argv[1]);
        ERR_POST(Info << "Reusing SEED " << seed);
    } else {
        seed = (int(CProcess::GetCurrentPid()) ^
                int(CTime(CTime::eCurrent).GetTimeT()));
        ERR_POST(Info << "Using SEED "   << seed);
    }
    srand(seed);

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

    ERR_POST(Info << "Pumping data with random I/O");

    CMyReader* rd = new CMyReader(hugedata,                kHugeBufsize);
    CMyWriter* wr = new CMyWriter(hugedata + kHugeBufsize, kHugeBufsize); 
    CRStream is(rd, kReadBufsize,  0, CRWStreambuf::fOwnReader);
    CWStream os(wr, kWriteBufsize, 0, CRWStreambuf::fOwnWriter);

    char* buf = new char[kMaxIOSize];

    size_t n_in = 0, n_out = 0;
    do {
        size_t x_in = rand() % kMaxIOSize + 1;
        ERR_POST(Info
                 << "Read:  " << setw(8) << x_in);
        is.read(buf, x_in);
        if (!(x_in = (size_t) is.gcount()))
            break;
        n_in += x_in;
        size_t x_out = 0;
        while (x_out < x_in) {
            size_t xx_out = rand() % (x_in - x_out) + 1;
            ERR_POST(Info
                     << "Write: " << setw(8) << xx_out << '/'
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
             << "Read:  " << setw(8) << n_in  << '/' << kHugeBufsize
             << ";  position: " << rd->GetPosition() << '/' << rd->GetSize());
    ERR_POST(Info
             << "Write: " << setw(8) << n_out << '/' << kHugeBufsize
             << ";  position: " << wr->GetPosition() << '/' << wr->GetSize());

    _ASSERT(kHugeBufsize == n_in           &&
            kHugeBufsize == rd->GetSize()  &&
            kHugeBufsize == rd->GetPosition());
    _ASSERT(kHugeBufsize == n_out          &&
            kHugeBufsize == wr->GetSize()  &&
            kHugeBufsize == wr->GetPosition());

    ERR_POST(Info << "Comparing original with collected data");

    for (size_t n = 0;  n < kHugeBufsize;  n++) {
        if (hugedata[n] != hugedata[n + kHugeBufsize])
            ERR_POST(Fatal << "Mismatch @ " << n);
    }

    ERR_POST(Info << "Checking tied I/O");

    buf = (char*) hugedata + kHugeBufsize;
    memset(buf, '\xFF', kHugeBufsize);

    CMyReaderWriter* rw = new CMyReaderWriter(hugedata + kHugeBufsize,
                                              kHugeBufsize);
    CRWStream io(rw, 2*(kReadBufsize + kWriteBufsize),
                 0, CRWStreambuf::fOwnReader);

    n_out = n_in = 0;
    do {
        if ((rand() % 10 == 2  ||  n_out == n_in)  &&  n_out < kHugeBufsize) {
            size_t x_out = rand() % kMaxIOSize + 1;
            if (x_out + n_out > kHugeBufsize)
                x_out = kHugeBufsize - n_out;
            ERR_POST(Info
                     << "Write: " << setw(8) << x_out);
            if (!io.write(buf - kHugeBufsize + n_out, x_out))
                break;
            n_out += x_out;
        }
        if (rand() % 10 == 4  &&  n_out > n_in) {
            size_t x_in = (rand() & 1
                           ? n_out - n_in
                           : rand() % (n_out - n_in) + 1);
            ERR_POST(Info
                     << "Read:  " << setw(8) << x_in);
            if (!io.read(buf + kHugeBufsize + n_in, x_in))
                break;
            n_in += x_in;
        }
        if (n_out >= kHugeBufsize  &&  n_in >= kHugeBufsize)
            break;
    } while (io.good());
    // io.flush(); // not needed as everything should have been read out

    ERR_POST(Info
             << "Read:  " << setw(8) << n_in  << '/' << kHugeBufsize << ";  "
             << "position: " << rw->GetRPosition() << '/' << rw->GetRSize());
    ERR_POST(Info
             << "Write: " << setw(8) << n_out << '/' << kHugeBufsize << ";  "
             << "position: " << rw->GetWPosition() << '/' << rw->GetWSize());

    _ASSERT(kHugeBufsize == n_in            &&
            kHugeBufsize == rw->GetRSize()  &&
            kHugeBufsize == rw->GetRPosition());
    _ASSERT(kHugeBufsize == n_out           &&
            kHugeBufsize == rw->GetWSize()  &&
            kHugeBufsize == rw->GetWPosition());

    ERR_POST(Info << "Comparing original with collected data");

    for (size_t n = 0;  n < kHugeBufsize;  n++) {
        if (hugedata[n] != hugedata[n + kHugeBufsize]  ||
            hugedata[n] != hugedata[n + kHugeBufsize*2]) {
            ERR_POST(Fatal << "Mismatch @ " << n);
        }
    }

    ERR_POST(Info << "Test completed successfully");

    delete[] hugedata;
    return 0;
}
