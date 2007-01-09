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
 * Authors:
 *   Dmitry Kazimirov - Initial revision.
 *
 * File Description:
 *   This file contains implementations of classes CChunkStreamReader and
 *   CChunkStreamWriter. The former serializes chunks of binary data to a byte
 *   stream and the latter deserializes these chunks back from the stream.
 */

#include <ncbi_pch.hpp>

#include <util/chunk_stream.hpp>

#include <stdio.h>
#include <string.h>
#include <assert.h>

BEGIN_NCBI_SCOPE

#define CHUNK_LENGTH_DELIM ':'
#define CONTROL_SYM_PREFIX '\\'

CChunkStreamReader::EStreamParsingEvent CChunkStreamReader::NextParsingEvent()
{
    if (m_BufferSize == 0)
        return eEndOfBuffer;

    unsigned digit;

    switch (m_State) {
    case eReadEscapedControlChar:
        m_ChunkPart = m_Buffer;
        ++m_Buffer;
        --m_BufferSize;
        m_State = eReadControlChars;
        return eControlSymbol;

    case eReadControlChars:
        // Check if the current character is a digit - all non-digit characters
        // are considered control symbols.
        if ((digit = (unsigned) *m_Buffer - '0') > 9) {
            if (*m_Buffer == CONTROL_SYM_PREFIX) {
                if (--m_BufferSize == 0) {
                    m_State = eReadEscapedControlChar;
                    return eEndOfBuffer;
                }
                ++m_Buffer;
            }
            m_ChunkPart = m_Buffer;
            ++m_Buffer;
            --m_BufferSize;
            return eControlSymbol;
        }

        // The current character is a digit - proceed with reading chunk
        // length.
        m_State = eReadChunkLength;
        m_LengthAcc = digit;
        if (--m_BufferSize == 0)
            return eEndOfBuffer;
        ++m_Buffer;

    case eReadChunkLength:
        while ((digit = (unsigned) *m_Buffer - '0') <= 9) {
            m_LengthAcc = m_LengthAcc * 10 + digit;

            if (--m_BufferSize == 0)
                return eEndOfBuffer;
            ++m_Buffer;
        }

        m_State = eReadChunk;

        if (*m_Buffer == CHUNK_LENGTH_DELIM) {
            if (--m_BufferSize == 0)
                return eEndOfBuffer;
            ++m_Buffer;
        }

    default: /* case eReadChunk: */
        m_ChunkPart = m_Buffer;

        if (m_BufferSize >= m_LengthAcc) {
            m_BufferSize -= m_LengthAcc;
            m_ChunkPartSize = m_LengthAcc;
            m_Buffer += m_LengthAcc;
            // The last part of the chunk has been read - get back to
            // reading control symbols.
            m_State = eReadControlChars;
            return eChunk;
        } else {
            m_ChunkPartSize = m_BufferSize;
            m_LengthAcc -= m_BufferSize;
            m_BufferSize = 0;
            return eChunkPart;
        }
    }
}

void CChunkStreamWriter::Reset(char* buffer,
    size_t buffer_size, size_t max_buffer_size)
{
    assert(buffer_size >= sizeof(m_InternalBuffer) - 1 &&
        max_buffer_size >= buffer_size &&
        "Buffer sizes must be consistent.");

    m_OutputBuffer = m_Buffer = buffer;
    m_BufferSize = buffer_size;
    m_InternalBufferSize = m_ChunkPartSize = m_OutputBufferSize = 0;
    m_MaxBufferSize = max_buffer_size;
}

bool CChunkStreamWriter::SendControlSymbol(char symbol)
{
    assert(m_OutputBuffer == m_Buffer && m_OutputBufferSize < m_BufferSize &&
        m_InternalBufferSize == 0 && m_ChunkPartSize == 0 &&
        "Must be in the state of filling the output buffer.");

    if ((unsigned) symbol - '0' <= 9 || symbol == CONTROL_SYM_PREFIX) {
        m_Buffer[m_OutputBufferSize] = CONTROL_SYM_PREFIX;
        if (++m_OutputBufferSize == m_BufferSize) {
            m_InternalBuffer[sizeof(m_InternalBuffer) - 1] = symbol;
            m_InternalBufferSize = 1;
            return false;
        }
    }

    m_Buffer[m_OutputBufferSize] = symbol;
    return ++m_OutputBufferSize < m_BufferSize;
}

bool CChunkStreamWriter::SendChunk(const char* chunk, size_t chunk_length)
{
    assert(m_OutputBuffer == m_Buffer && m_OutputBufferSize < m_BufferSize &&
        m_InternalBufferSize == 0 && m_ChunkPartSize == 0 &&
        "Must be in the state of filling the output buffer.");

    char* result = m_InternalBuffer + sizeof(m_InternalBuffer);

    if (chunk_length == 0 || (unsigned) *chunk - '0' <= 9 ||
        *chunk == CHUNK_LENGTH_DELIM)
        *--result = CHUNK_LENGTH_DELIM;

    size_t number = chunk_length;

    do
        *--result = number % 10 + '0';
    while (number /= 10);

    size_t string_len = m_InternalBuffer + sizeof(m_InternalBuffer) - result;
    size_t free_buf_size = m_BufferSize - m_OutputBufferSize;

    if (string_len < free_buf_size) {
        char* free_buffer = m_Buffer + m_OutputBufferSize;
        memcpy(free_buffer, result, string_len);
        free_buffer += string_len;
        free_buf_size -= string_len;
        if (chunk_length < free_buf_size) {
            memcpy(free_buffer, chunk, chunk_length);
            m_OutputBufferSize += string_len + chunk_length;
            return true;
        }
        memcpy(free_buffer, chunk, free_buf_size);
        m_ChunkPartSize = chunk_length - free_buf_size;
        m_ChunkPart = chunk + free_buf_size;
    } else {
        memcpy(m_Buffer + m_OutputBufferSize, result, free_buf_size);
        m_InternalBufferSize = string_len - free_buf_size;
        m_ChunkPartSize = chunk_length;
        m_ChunkPart = chunk;
    }
    m_OutputBufferSize = m_BufferSize;
    return false;
}

bool CChunkStreamWriter::NextOutputBuffer()
{
    if (m_InternalBufferSize > 0) {
        memcpy(m_Buffer, m_InternalBuffer + sizeof(m_InternalBuffer) -
            m_InternalBufferSize, m_InternalBufferSize);
        size_t free_buf_size = m_BufferSize - m_InternalBufferSize;
        if (m_ChunkPartSize < free_buf_size) {
            memcpy(m_Buffer + m_InternalBufferSize,
                m_ChunkPart, m_ChunkPartSize);
            m_OutputBufferSize = m_InternalBufferSize + m_ChunkPartSize;
            m_InternalBufferSize = m_ChunkPartSize = 0;
            return false;
        }
        memcpy(m_Buffer + m_InternalBufferSize, m_ChunkPart, free_buf_size);
        m_ChunkPartSize -= free_buf_size;
        m_ChunkPart += free_buf_size;
        m_OutputBufferSize = m_BufferSize;
        m_InternalBufferSize = 0;
    } else {
        if (m_ChunkPartSize >= m_MaxBufferSize)
            m_OutputBufferSize = m_MaxBufferSize;
        else if (m_ChunkPartSize >= m_BufferSize)
            m_OutputBufferSize = m_BufferSize;
        else {
            memcpy(m_Buffer, m_ChunkPart, m_ChunkPartSize);
            m_OutputBuffer = m_Buffer;
            m_OutputBufferSize = m_ChunkPartSize;
            m_ChunkPartSize = 0;
            return false;
        }
        m_OutputBuffer = m_ChunkPart;
        m_ChunkPart += m_OutputBufferSize;
        m_ChunkPartSize -= m_OutputBufferSize;
    }
    return true;
}

END_NCBI_SCOPE
