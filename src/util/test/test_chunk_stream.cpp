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
 * Author: Dmitry Kazimirov
 *
 * File Description:
 *   Test program for testing 
 *
 */

#include <ncbi_pch.hpp>

#include <util/chunk_stream.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbiargs.hpp>
#include <stdio.h>

#include <test/test_assert.h>  /* This header must go last */

USING_NCBI_SCOPE;

class CChunkStreamTestApp : public CNcbiApplication
{
// Construction
public:
    CChunkStreamTestApp();

// Overridden
public:
    virtual int Run();

// Implementation
private:
    struct SChunkData {
        char* chunk;
        bool to_be_continued;
        bool is_control_symbol;
        char control_symbol;
    };

    static const SChunkData sm_TestChunkSequence[];
    static const char sm_TestStream[];

    bool TestReaderChunkPart();
    bool TestReaderInput(const char* buffer, size_t buffer_size);
    bool TestReader(size_t buffer_size);

    bool TestWriterOutput();
    bool TestWriter(size_t buffer_size);

    char m_Buffer[1024];

    CChunkStreamReader m_Reader;
    const SChunkData* m_ReaderTestChunk;
    const char* m_ReaderTestChunkPart;

    CChunkStreamWriter m_Writer;
    const char* m_WriterTestStream;
};

const CChunkStreamTestApp::SChunkData
    CChunkStreamTestApp::sm_TestChunkSequence[] =
{
    {"cd", false, false, 0},
    {NULL, false, true, '*'},
    {"ls", false, false, 0},
    {NULL, false, true, '*'},
    {"mkdir", false, false, 0},
    {"test", false, false, 0},
    {NULL, false, true, '*'},
    {"cd", false, false, 0},
    {"test", false, false, 0},
    {NULL, false, true, '*'},
    {"cp", false, false, 0},
    {"~/test.cpp", false, false, 0},
    {".", false, false, 0},
    {NULL, false, true, '*'},
    {"g++", false, false, 0},
    {"-Wall", false, false, 0},
    {"-o", false, false, 0},
    {"test", false, false, 0},
    {"test.cpp", false, false, 0},
    {NULL, false, true, '*'},
    {"./test", false, false, 0},
    {NULL, false, true, '*'},
    {"cd", false, false, 0},
    {"..", false, false, 0},
    {NULL, false, true, '*'},
    {"rm", false, false, 0},
    {"-fr", false, false, 0},
    {"test", false, false, 0},
    {NULL, false, true, '*'},
    {"update", true, false, 0},
    {NULL, false, true, '?'},
    {"db", false, false, 0},
    {NULL, false, true, '*'}
};

const char CChunkStreamTestApp::sm_TestStream[] =
    "2 cd*"
    "2 ls*"
    "5 mkdir4 test*"
    "2 cd4 test*"
    "2 cp10 ~/test.cpp1 .*"
    "3 g++5 -Wall2 -o4 test8 test.cpp*"
    "6 ./test*"
    "2 cd2 ..*"
    "2 rm3 -fr4 test*"
    "6+update?"
    "2 db*";

inline CChunkStreamTestApp::CChunkStreamTestApp() :
    m_Writer(m_Buffer, (sizeof(size_t) >> 1) * 5)
{
}

#define IN_READER_TEST "CChunkStreamReader test: "

bool CChunkStreamTestApp::TestReaderChunkPart()
{
    if (m_ReaderTestChunk->is_control_symbol) {
        fprintf(stderr, IN_READER_TEST
            "got a chunk part while expected a control symbol\n");
        return false;
    }

    if (m_ReaderTestChunkPart == NULL)
        m_ReaderTestChunkPart = m_ReaderTestChunk->chunk;

    const char* chunk_part = m_Reader.GetChunkPart();
    size_t chunk_part_size = m_Reader.GetChunkPartSize();

    if (memcmp(m_ReaderTestChunkPart, chunk_part, chunk_part_size) != 0) {
        fprintf(stderr, IN_READER_TEST
            "chunk part mismatch: [%.*s] vs [%.*s]\n", chunk_part_size,
            m_ReaderTestChunkPart, chunk_part_size, chunk_part);
        return false;
    }

    m_ReaderTestChunkPart += chunk_part_size;

    return true;
}

bool CChunkStreamTestApp::TestReaderInput(
    const char* buffer, size_t buffer_size)
{
    m_Reader.SetNewBuffer(buffer, buffer_size);

    for (;;)
        switch (m_Reader.NextParsingEvent()) {
        case CChunkStreamReader::eChunkPart:
            if (!TestReaderChunkPart())
                return false;

            if (*m_ReaderTestChunkPart == 0 &&
                m_ReaderTestChunk->to_be_continued) {
                ++m_ReaderTestChunk;
                m_ReaderTestChunkPart = NULL;
            }
            break;

        case CChunkStreamReader::eChunk:
            if (!TestReaderChunkPart())
                return false;

            if (*m_ReaderTestChunkPart != 0) {
                fprintf(stderr, IN_READER_TEST
                    "unexpected end of chunk ...%.*s\n",
                    m_Reader.GetChunkPartSize(),
                    m_Reader.GetChunkPart());
                return false;
            }

            ++m_ReaderTestChunk;
            m_ReaderTestChunkPart = NULL;
            break;

        case CChunkStreamReader::eControlSymbol:
            if (!m_ReaderTestChunk->is_control_symbol) {
                fprintf(stderr, IN_READER_TEST
                    "unexpected control symbol '%c'\n",
                    *m_Reader.GetChunkPart());
                return false;
            }

            if (m_ReaderTestChunk->control_symbol != *m_Reader.GetChunkPart()) {
                fprintf(stderr, IN_READER_TEST
                    "control symbol mismatch: '%c' vs '%c'\n",
                    m_ReaderTestChunk->control_symbol,
                    *m_Reader.GetChunkPart());
                return false;
            }
            ++m_ReaderTestChunk;
            break;

        default: /* case CChunkStreamReader::eEndOfBuffer: */
            return true;
        }
}

bool CChunkStreamTestApp::TestReader(size_t buffer_size)
{
    m_Reader.Reset();
    m_ReaderTestChunk = sm_TestChunkSequence;
    m_ReaderTestChunkPart = NULL;

    try {
        const char* buffer = sm_TestStream;
        size_t stream_length = sizeof(sm_TestStream) - 1;

        do {
            if (!TestReaderInput(buffer, buffer_size))
                return false;
            buffer += buffer_size;
        } while ((stream_length -= buffer_size) > buffer_size);

        return TestReaderInput(buffer, stream_length);
    } catch (CChunkStreamFormatException& e) {
        fprintf(stderr, IN_READER_TEST "%s\n", e.what());
        return false;
    }
}

#define IN_WRITER_TEST "CChunkStreamWriter test: "

bool CChunkStreamTestApp::TestWriterOutput()
{
    const char* buffer;
    size_t buffer_size;

    m_Writer.GetOutputBuffer(&buffer, &buffer_size);

    size_t pos = m_WriterTestStream - sm_TestStream;

    if (pos + buffer_size > sizeof(sm_TestStream)) {
        fprintf(stderr, IN_WRITER_TEST "output buffer overflow: %u > %u.\n",
            pos + buffer_size, sizeof(sm_TestStream));
        return false;
    }

    if (memcmp(buffer, m_WriterTestStream, buffer_size) != 0) {
        fprintf(stderr, IN_WRITER_TEST "mismatch at pos %u: [%.*s] vs [%.*s]\n",
            pos, buffer_size, m_WriterTestStream, buffer_size, buffer);
        return false;
    }

    m_WriterTestStream += buffer_size;

    return true;
}

bool CChunkStreamTestApp::TestWriter(size_t buffer_size)
{
    m_Writer.Reset(m_Buffer, buffer_size);
    m_WriterTestStream = sm_TestStream;

    int counter = sizeof(sm_TestChunkSequence) /
        sizeof(*sm_TestChunkSequence);

    const SChunkData* test_seq = sm_TestChunkSequence;

    do {
        if (!(test_seq->is_control_symbol ?
            m_Writer.SendControlSymbol(test_seq->control_symbol) :
            m_Writer.SendChunk(test_seq->chunk, strlen(test_seq->chunk),
                test_seq->to_be_continued)))
            do
                if (!TestWriterOutput())
                    return false;
            while (m_Writer.NextOutputBuffer());

        ++test_seq;
    } while (--counter);

    return TestWriterOutput();
}

int CChunkStreamTestApp::Run(void)
{
    return TestReader(8) && TestWriter(16) ? 0 : 1;
}

int main(int argc, const char* argv[])
{
    return CChunkStreamTestApp().AppMain(argc, argv);
}
