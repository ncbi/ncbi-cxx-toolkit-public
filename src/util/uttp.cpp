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
 *   Dmitry Kazimirov
 *
 * File Description:
 *   This file contains implementations of classes CUTTPReader and
 *   CUTTPWriter. They implement Untyped Tree Transfer Protocol - UTTP.
 */

#include <ncbi_pch.hpp>

#include <util/uttp.hpp>

#include <stdio.h>
#include <string.h>
#include <corelib/ncbidbg.hpp>

BEGIN_NCBI_SCOPE

CUTTPReader::EStreamParsingEvent CUTTPReader::GetNextEvent()
{
    if (m_BufferSize == 0)
        return eEndOfBuffer;

    unsigned digit;

    switch (m_State) {
    case eReadControlChars:
        // This block will consume one character either way.
        ++m_Offset;

        // Check if the current character is a digit.
        if ((digit = (unsigned) *m_Buffer - '0') > 9) {
            // All non-digit characters are considered control symbols.
            m_ChunkPart = m_Buffer;
            ++m_Buffer;
            --m_BufferSize;
            return eControlSymbol;
        }

        // The current character is a digit - proceed with reading chunk
        // length.
        m_State = eReadNumber;
        m_LengthAcc = digit;
        if (--m_BufferSize == 0)
            return eEndOfBuffer;
        ++m_Buffer;
        /* FALL THROUGH */

    case eReadNumber:
        while ((digit = (unsigned) *m_Buffer - '0') <= 9) {
            m_LengthAcc = m_LengthAcc * 10 + digit;

            ++m_Offset;
            if (--m_BufferSize == 0)
                return eEndOfBuffer;
            ++m_Buffer;
        }
        switch (*m_Buffer) {
        case '+':
            m_ChunkContinued = true;
            break;
        case ' ':
            m_ChunkContinued = false;
            break;
        case '-':
            m_LengthAcc = -m_LengthAcc;
            /* FALL THROUGH */
        case '=':
            ++m_Offset;
            ++m_Buffer;
            --m_BufferSize;
            return eNumber;
        default:
            m_ChunkPart = m_Buffer;
            m_ChunkPartSize = (size_t) m_LengthAcc;
            m_State = eReadControlChars;
            return eFormatError;
        }

        m_State = eReadChunk;
        ++m_Offset;
        if (--m_BufferSize == 0)
            return eEndOfBuffer;
        ++m_Buffer;

    default: /* case eReadChunk: */
        m_ChunkPart = m_Buffer;

        if (m_BufferSize >= (size_t) m_LengthAcc) {
            m_ChunkPartSize = (size_t) m_LengthAcc;
            m_BufferSize -= m_ChunkPartSize;
            m_Buffer += m_ChunkPartSize;
            m_Offset += (off_t) m_ChunkPartSize;
            // The last part of the chunk has been read - get back to
            // reading control symbols.
            m_State = eReadControlChars;
            return m_ChunkContinued ? eChunkPart : eChunk;
        } else {
            m_ChunkPartSize = m_BufferSize;
            m_Offset += (off_t) m_BufferSize;
            m_LengthAcc -= m_BufferSize;
            m_BufferSize = 0;
            return eChunkPart;
        }
    }
}

void CUTTPWriter::Reset(char* buffer,
    size_t buffer_size, size_t max_buffer_size)
{
    _ASSERT(buffer_size >= sizeof(m_InternalBuffer) - 1 &&
        max_buffer_size >= buffer_size &&
        "Buffer sizes must be consistent.");

    m_OutputBuffer = m_Buffer = buffer;
    m_BufferSize = buffer_size;
    m_InternalBufferSize = m_ChunkPartSize = m_OutputBufferSize = 0;
    m_MaxBufferSize = max_buffer_size;
}

bool CUTTPWriter::SendControlSymbol(char symbol)
{
    _ASSERT(m_OutputBuffer == m_Buffer && m_OutputBufferSize < m_BufferSize &&
        m_InternalBufferSize == 0 && m_ChunkPartSize == 0 &&
        "Must be in the state of filling the output buffer.");
    _ASSERT((symbol < '0' || symbol > '9') &&
        "Control symbol cannot be a digit.");

    m_Buffer[m_OutputBufferSize] = symbol;
    return ++m_OutputBufferSize < m_BufferSize;
}

bool CUTTPWriter::SendChunk(const char* chunk,
    size_t chunk_length, bool to_be_continued)
{
    _ASSERT(m_OutputBuffer == m_Buffer && m_OutputBufferSize < m_BufferSize &&
        m_InternalBufferSize == 0 && m_ChunkPartSize == 0 &&
        "Must be in the state of filling the output buffer.");

    char* result = m_InternalBuffer + sizeof(m_InternalBuffer) - 1;

    *result = to_be_continued ? '+' : ' ';

    Uint8 number = chunk_length;

    do
        *--result = char(number % 10) + '0';
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

bool CUTTPWriter::SendNumber(Int8 number)
{
    _ASSERT(m_OutputBuffer == m_Buffer && m_OutputBufferSize < m_BufferSize &&
        m_InternalBufferSize == 0 && m_ChunkPartSize == 0 &&
        "Must be in the state of filling the output buffer.");

    char* result = m_InternalBuffer + sizeof(m_InternalBuffer) - 1;

    if (number >= 0)
        *result = '=';
    else {
        *result = '-';
        number = -number;
    }

    do
        *--result = char(number % 10) + '0';
    while (number /= 10);

    size_t string_len = m_InternalBuffer + sizeof(m_InternalBuffer) - result;
    size_t free_buf_size = m_BufferSize - m_OutputBufferSize;

    if (string_len < free_buf_size) {
        memcpy(m_Buffer + m_OutputBufferSize, result, string_len);
        m_OutputBufferSize += string_len;
        return true;
    }
    memcpy(m_Buffer + m_OutputBufferSize, result, free_buf_size);
    m_InternalBufferSize = string_len - free_buf_size;
    m_ChunkPartSize = 0;
    m_OutputBufferSize = m_BufferSize;
    return false;
}

bool CUTTPWriter::NextOutputBuffer()
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
