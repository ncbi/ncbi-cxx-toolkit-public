/*  $Id$
 * ===========================================================================
 *
 *                            PUBLIC DOMAIN NOTICE
 *               National Center for Biotechnology Information
 *
 *   This software/database is a "United States Government Work" under the
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
 * File Description:
 *   Test program for transmission reader and writer
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbiargs.hpp>
#include <util/transmissionrw.hpp>
#include <stdio.h>
#include <stdlib.h>

#include <test/test_assert.h>  /* This header must go last */

USING_NCBI_SCOPE;

/// @internal
class CTestMemWriter : public IWriter
{
public:
    CTestMemWriter(unsigned char* buf, size_t buf_size)
    : m_Buf(buf),
      m_Ptr(buf),
      m_Capacity(buf_size)
    {}

    size_t SizeWritten() const { return m_Ptr - m_Buf; }

    virtual ERW_Result Write(const void* buf,
                             size_t      count,
                             size_t*     bytes_written = 0)
    {
        const unsigned char* src = (unsigned char*) buf;
        for (unsigned i = 0; i < count; ++i) {
            if (m_Capacity == 0)
                return eRW_Error;
            *m_Ptr++ = *src++;
            --m_Capacity;
            if (bytes_written) 
                *bytes_written = i+1;
        }
        return eRW_Success;
    }

    virtual ERW_Result Flush(void){ return eRW_Success; }


private:
    CTestMemWriter(const CTestMemWriter&);
    CTestMemWriter& operator=(const CTestMemWriter&);
private:
    unsigned char*  m_Buf;
    unsigned char*  m_Ptr;
    size_t          m_Capacity;
};

/// @internal
class CTestMemReader : public IReader
{
public:
    CTestMemReader(const unsigned char* buf, size_t length)
    : m_Buf(buf),
      m_Ptr(buf),
      m_Length(length)
    {}

    virtual ERW_Result Read(void*   buf,
                            size_t  count,
                            size_t* bytes_read = 0)
    {
        unsigned char* b = (unsigned char*)buf;
        if (m_Ptr - m_Buf == m_Length) {
            if (bytes_read)
                *bytes_read = 0;
            return eRW_Eof;
        }
        if (rand() % 3 == 0) {
            *b = *m_Ptr++;
            if (bytes_read) {
                *bytes_read = 1;
            }
            return eRW_Success;
        }

        size_t pending = m_Length - (m_Ptr - m_Buf);
        size_t to_read = ::min(count, pending);
        memcpy(buf, m_Ptr, to_read);
        if (bytes_read) {
            *bytes_read = to_read;
        }
        m_Ptr += to_read;
        return eRW_Success;
    }

    ERW_Result PendingCount(size_t* count)
    {
        return eRW_NotImplemented;
    }

private:
    CTestMemReader(const CTestMemReader&);
    CTestMemReader& operator=(const CTestMemReader&);
private:
    const unsigned char*   m_Buf;
    const unsigned char*   m_Ptr;
    size_t                 m_Length;
};


/// @internal
class CTestTransmission : public CNcbiApplication
{
public:
    void Init(void);
    int Run(void);
};


void CTestTransmission::Init(void)
{
    SetDiagPostLevel(eDiag_Warning);

    auto_ptr<CArgDescriptions> d(new CArgDescriptions);

    d->SetUsageContext("test_transmissionrw",
                       "test transmission reader/writer");
    SetupArgDescriptions(d.release());
}

static
void s_SaveTestData(IWriter* wrt, size_t size)
{
    char v = 0;
    for (size_t i = 0; i < size; ++i) {
        ERW_Result res = wrt->Write(&v, 1);
        ++v;
        assert(res == eRW_Success);
    }
}

static
void s_ReadCheck1(IReader*             rdr, 
                  const unsigned char* buf1,
                  unsigned char*       buf2,
                  size_t               size,
                  size_t               control_size)
{
    size_t read;
    size_t total_read = 0;
    unsigned char* ptr = buf2;
    ERW_Result res;
    while (1) {
        res = rdr->Read(ptr, size, &read);
        total_read += read;
        if (res == eRW_Eof)
            break;
        size -= read;
        ptr += read;
        assert(res == eRW_Success);
    }
    for (size_t i = 0; i < control_size; ++i) {
        unsigned char ch = (unsigned char)i;
        assert(ch == buf2[i]);
    }
}

int CTestTransmission::Run(void)
{
    NcbiCout << "Test IReader/IWriter transmission" << NcbiEndl;

    const int test_buf_size = 1024;
    unsigned char buf[test_buf_size  * 10];
    unsigned char buf2[test_buf_size * 10];

    // ---------------------------------------
    // write the test sequence

    ERW_Result res;
    size_t     sz;

    {{
    CTestMemWriter wrt(buf, sizeof(buf));
    s_SaveTestData(&wrt, test_buf_size/2);

    sz = wrt.SizeWritten();

    assert(sz == test_buf_size/2);
    }}


    // ---------------------------------------
    // read the whole test sequence

    {{
    CTestMemReader rdr(buf, sz);
    s_ReadCheck1(&rdr, buf, buf2, sz * 2, sz);
    }}

    // ---------------------------------------
    // read the test sequence char by char

    {{
    CTestMemReader rdr(buf, sz);
    for (size_t i = 0; i < sz * 2; ++i) {
        buf2[i] = 0;
        size_t read;
        res = rdr.Read(buf2 + i, 1, &read);
        if (res == eRW_Eof) {
            assert(i == sz);
            assert(read == 0);
            break;
        }
        assert(read == 1);
        assert(res == eRW_Success);
        assert (buf2[i] == buf[i]);
    }
    }}

    // ---------------------------------------
    // read the test sequence by 7 char

    {{
    CTestMemReader rdr(buf, sz);
    size_t read = 0;
    for (size_t i = 0; i < sz * 2; i+=read) {
        unsigned char buf3[10] = {0,};
        res = rdr.Read(buf3, 7, &read);
        if (res == eRW_Eof) {
            assert(read == 0);
            break;
        }
        assert(read <= 7);
        assert(res == eRW_Success);
        for (size_t j = 0; j < read; ++j) {
            assert(buf3[j] == (unsigned char)(i + j));
        }
    }
    }}


    // Transmission test


    memset(buf, 0, sizeof(buf));
    memset(buf2, 0, sizeof(buf2));



    {{
    CTestMemWriter wrt(buf, sizeof(buf));
    CTransmissionWriter twrt(&wrt);

    char b = 35;
    twrt.Write(&b, 1);

    CTestMemReader rdr(buf, sz);
    CTransmissionReader trdr(&rdr);

    char c = 0;
    trdr.Read(&c, 1);

    assert(b==c);


    }}

    size_t tsize = 3;
    size_t byte_size;

    {{
    CTestMemWriter wrt(buf, sizeof(buf));
    CTransmissionWriter twrt(&wrt);
    s_SaveTestData(&twrt, tsize);//test_buf_size/2);
    byte_size = wrt.SizeWritten();
    }}


    // ---------------------------------------
    // read the whole test sequence

    {{
    CTestMemReader rdr(buf, byte_size);
    CTransmissionReader trdr(&rdr);

    s_ReadCheck1(&trdr, buf, buf2, sz * 2, tsize);//test_buf_size/2);
    }}


    memset(buf, 0, sizeof(buf));
    memset(buf2, 0, sizeof(buf2));

    tsize = test_buf_size/2;
    {{
    CTestMemWriter wrt(buf, sizeof(buf));
    CTransmissionWriter twrt(&wrt);
    s_SaveTestData(&twrt, tsize);
    byte_size = wrt.SizeWritten();
    }}


    // ---------------------------------------
    // read the whole test sequence

    {{
    CTestMemReader rdr(buf, byte_size);
    CTransmissionReader trdr(&rdr);

    s_ReadCheck1(&trdr, buf, buf2, tsize * 2, tsize);
    }}

    // ---------------------------------------
    // read the test sequence char by char

    {{
    CTestMemReader rdr(buf, byte_size);
    CTransmissionReader trdr(&rdr);
    size_t i;
    for (i = 0; i < tsize * 2; ++i) {
        buf2[i] = 0;
        size_t read;
        res = trdr.Read(buf2 + i, 1, &read);
        if (res == eRW_Eof) {
            assert(i == sz);
            assert(read == 0);
            break;
        }
        assert(read == 1);
        assert(res == eRW_Success);
        unsigned char ch = (unsigned char) i;
        assert (buf2[i] == ch);
    }
    assert(i == tsize);
    }}


    // ---------------------------------------
    // read the test sequence by 7 char

    {{
    CTestMemReader rdr(buf, byte_size);
    CTransmissionReader trdr(&rdr);
    size_t read = 0;
    size_t total_read = 0;
    for (size_t i = 0; i < tsize * 2; i+=read) {
        unsigned char buf3[10] = {0,};
        res = trdr.Read(buf3, 7, &read);
        if (res == eRW_Eof) {
            assert(read == 0);
            break;
        }
        total_read += read;
        assert(read <= 7);
        assert(res == eRW_Success);
        for (size_t j = 0; j < read; ++j) {
            assert(buf3[j] == (unsigned char)(i + j));
        }
    }
    assert(total_read = tsize);
    }}


    NcbiCout << "OK" << NcbiEndl;

    return 0;
}

int main(int argc, const char* argv[])
{
    return CTestTransmission().AppMain(argc, argv);
}

/*
 * ===========================================================================
 * $Log$
 * Revision 1.1  2005/04/14 13:49:30  kuznets
 * Initial revision
 *
 * ===========================================================================
 */
