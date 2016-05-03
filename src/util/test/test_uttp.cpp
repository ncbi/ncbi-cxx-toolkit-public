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
 * Authors:
 *   Dmitry Kazimirov
 *
 * File Description:
 *   Test program for testing Untyped Tree Transfer Protocol (UTTP)
 *   implementation.
 *
 */

#include <ncbi_pch.hpp>

#include <util/uttp.hpp>

#include <corelib/ncbiapp.hpp>
#include <corelib/ncbiargs.hpp>

#include <corelib/test_boost.hpp>

#include <stdio.h>

#include <common/test_assert.h>  /* This header must go last */

USING_NCBI_SCOPE;

struct SChunkData {
    const char* chunk;
    bool to_be_continued;
    char control_symbol;
};

class CUTTPReaderTest
{
public:
    bool Test(size_t buffer_size)
#if defined(__llvm__)  &&  !defined(__clang__)
        __attribute__((noinline))
#endif
        ;

private:
    bool x_TestReaderChunkPart();
    bool x_TestReaderInput(const char* buffer, size_t buffer_size);

    CUTTPReader m_Reader;
    const SChunkData* m_ReaderTestChunk;
    const char* m_ReaderTestChunkPart;
};

class CUTTPWriterTest
{
public:
    bool Test(size_t buffer_size);

private:
    bool x_TestWriterOutput();

    CUTTPWriter m_Writer;
    const char* m_WriterTestStream;
};

static const SChunkData s_TestChunkSequence[] =
{
    {"", false, 0},
    {"cd", false, 0},
    {NULL, false, ';'},
    {"ls", false, 0},
    {NULL, false, ';'},
    {"mkdir", false, 0},
    {"test", false, 0},
    {NULL, false, ';'},
    {"cd", false, 0},
    {"test", false, 0},
    {NULL, false, ';'},
    {"cp", false, 0},
    {"~/test.cpp", false, 0},
    {".", false, 0},
    {NULL, false, ';'},
    {"g++", false, 0},
    {"-Wall", false, 0},
    {"-o", false, 0},
    {"test", false, 0},
    {"test.cpp", false, 0},
    {NULL, false, ';'},
    {"./test", false, 0},
    {NULL, false, ';'},
    {"cd", false, 0},
    {"..", false, 0},
    {NULL, false, ';'},
    {"rm", false, 0},
    {"-fr", false, 0},
    {"test", false, 0},
    {NULL, false, ';'},
    {"update", true, 0},
    {"db", false, 0},
    {NULL, false, ';'}
};

static const char s_TestStream[] =
    "0 "
    "2 cd;"
    "2 ls;"
    "5 mkdir4 test;"
    "2 cd4 test;"
    "2 cp10 ~/test.cpp1 .;"
    "3 g++5 -Wall2 -o4 test8 test.cpp;"
    "6 ./test;"
    "2 cd2 ..;"
    "2 rm3 -fr4 test;"
    "6+update"
    "2 db;";

#define IN_READER_TEST "CUTTPReader test: "

bool CUTTPReaderTest::x_TestReaderChunkPart()
{
    if (m_ReaderTestChunk->control_symbol) {
        ERR_POST(IN_READER_TEST
            "got a chunk part while expected a control symbol");
        return false;
    }

    if (m_ReaderTestChunkPart == NULL)
        m_ReaderTestChunkPart = m_ReaderTestChunk->chunk;

    const char* chunk_part = m_Reader.GetChunkPart();
    size_t chunk_part_size = m_Reader.GetChunkPartSize();

    if (memcmp(m_ReaderTestChunkPart, chunk_part, chunk_part_size) != 0) {
        ERR_POST(IN_READER_TEST "chunk part mismatch: [" <<
            std::string(m_ReaderTestChunkPart, chunk_part_size) << "] vs [" <<
            std::string(chunk_part, chunk_part_size) << "]");
        return false;
    }

    m_ReaderTestChunkPart += chunk_part_size;

    return true;
}

bool CUTTPReaderTest::x_TestReaderInput(
    const char* buffer, size_t buffer_size)
{
    m_Reader.SetNewBuffer(buffer, buffer_size);

    for (;;)
        switch (m_Reader.GetNextEvent()) {
        case CUTTPReader::eChunkPart:
            if (!x_TestReaderChunkPart())
                return false;

            if (*m_ReaderTestChunkPart == 0 &&
                m_ReaderTestChunk->to_be_continued) {
                ++m_ReaderTestChunk;
                m_ReaderTestChunkPart = NULL;
            }
            break;

        case CUTTPReader::eChunk:
            if (!x_TestReaderChunkPart())
                return false;

            if (*m_ReaderTestChunkPart != 0) {
                ERR_POST(IN_READER_TEST "unexpected end of chunk ..." <<
                    std::string(m_Reader.GetChunkPart(),
                        m_Reader.GetChunkPartSize()));
                return false;
            }

            ++m_ReaderTestChunk;
            m_ReaderTestChunkPart = NULL;
            break;

        case CUTTPReader::eControlSymbol:
            if (!m_ReaderTestChunk->control_symbol) {
                ERR_POST(IN_READER_TEST "unexpected control symbol '" <<
                    m_Reader.GetControlSymbol() << '\'');
                return false;
            }

            if (m_ReaderTestChunk->control_symbol != *m_Reader.GetChunkPart()) {
                ERR_POST(IN_READER_TEST "control symbol mismatch: '" <<
                    m_ReaderTestChunk->control_symbol << "' vs '" <<
                    *m_Reader.GetChunkPart() << '\'');
                return false;
            }
            ++m_ReaderTestChunk;
            break;

        case CUTTPReader::eEndOfBuffer:
            return true;

        default: /* case CUTTPReader::eFormatError: */
            ERR_POST(IN_READER_TEST "stream format error at offset " <<
                m_Reader.GetOffset());
            return false;
        }
}

bool CUTTPReaderTest::Test(size_t buffer_size)
{
    m_Reader.Reset();
    m_ReaderTestChunk = s_TestChunkSequence;
    m_ReaderTestChunkPart = NULL;

    const char* buffer = s_TestStream;
    size_t stream_length = sizeof(s_TestStream) - 1;

    do {
        if (!x_TestReaderInput(buffer, buffer_size))
            return false;
        buffer += buffer_size;
    } while ((stream_length -= buffer_size) > buffer_size);

    return x_TestReaderInput(buffer, stream_length);
}

#define IN_WRITER_TEST "CUTTPWriter test: "

bool CUTTPWriterTest::x_TestWriterOutput()
{
    const char* buffer;
    size_t buffer_size;

    m_Writer.GetOutputBuffer(&buffer, &buffer_size);

    size_t pos = m_WriterTestStream - s_TestStream;

    if (pos + buffer_size > sizeof(s_TestStream) - 1) {
        ERR_POST(IN_WRITER_TEST "output buffer overflow: " <<
            pos + buffer_size << " > " << sizeof(s_TestStream) - 1);
        return false;
    }

    if (memcmp(buffer, m_WriterTestStream, buffer_size) != 0) {
        ERR_POST(IN_WRITER_TEST "mismatch at pos " << pos << ": [" <<
            std::string(m_WriterTestStream, buffer_size) << "] vs [" <<
            std::string(buffer, buffer_size) << "]");
        return false;
    }

    m_WriterTestStream += buffer_size;

    return true;
}

bool CUTTPWriterTest::Test(size_t buffer_size)
{
    char buffer[sizeof(s_TestStream) - 1];

    m_Writer.Reset(buffer, buffer_size);
    m_WriterTestStream = s_TestStream;

    size_t counter = sizeof(s_TestChunkSequence) / sizeof(*s_TestChunkSequence);

    const SChunkData* test_seq = s_TestChunkSequence;

    do {
        if (!(test_seq->control_symbol ?
            m_Writer.SendControlSymbol(test_seq->control_symbol) :
            m_Writer.SendChunk(test_seq->chunk, strlen(test_seq->chunk),
                test_seq->to_be_continued)))
            do
                if (!x_TestWriterOutput())
                    return false;
            while (m_Writer.NextOutputBuffer());

        ++test_seq;
    } while (--counter);

    return x_TestWriterOutput();
}

BOOST_AUTO_TEST_CASE(TestUTTPWriter)
{
    CUTTPWriterTest writer_test;

    size_t buffer_size = (sizeof(Int8) >> 1) * 5 + 1;

    do
        BOOST_CHECK(writer_test.Test(buffer_size));
    while (++buffer_size < sizeof(s_TestStream) - 1);
}

BOOST_AUTO_TEST_CASE(TestUTTPReader)
{
    CUTTPReaderTest reader_test;

    size_t buffer_size = sizeof(s_TestStream) - 1;

    do
        BOOST_CHECK(reader_test.Test(buffer_size));
    while (--buffer_size > 0);
}
