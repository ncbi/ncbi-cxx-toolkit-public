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
* Author:  Aaron Ucko, Anatoliy Kuznetsov
*
* File Description:
*   Lightweight interface for getting lines of data with minimal
*   memory copying.
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>
#include <util/line_reader.hpp>
#include <util/util_exception.hpp>

#include <string.h>

BEGIN_NCBI_SCOPE

CRef<ILineReader> ILineReader::New(const string& filename)
{
    CRef<ILineReader> lr;
    if (filename == "-") {
        lr.Reset(new CStreamLineReader(NcbiCin, eNoOwnership));
    } else {
        try {
            // favor memory mapping, which tends to be more efficient
            // but isn't always possible
            auto_ptr<CMemoryFile> mf(new CMemoryFile(filename));
            lr.Reset(new CMemoryLineReader(mf.release(), eTakeOwnership));
        } catch (...) { // fall back to streams, which are somewhat slower
            auto_ptr<CNcbiIfstream> ifs(new CNcbiIfstream(filename.c_str()));
            if (ifs.get()  &&  ifs->is_open()) {
                lr.Reset(new CStreamLineReader(*ifs.release(), eTakeOwnership));
            } else {
                NCBI_THROW(CFileException, eNotExists,
                           "Unable to construct a line reader on " + filename);
            }
        }
    }
    return lr;
}


CStreamLineReader::~CStreamLineReader()
{
    if (m_OwnStream) {
        delete &m_Stream;
    }
}

bool CStreamLineReader::AtEOF(void) const
{
    return m_Stream.eof()  ||  CT_EQ_INT_TYPE(m_Stream.peek(), CT_EOF);
}

char CStreamLineReader::PeekChar(void) const
{
    return m_Stream.peek();
}

CStreamLineReader& CStreamLineReader::operator++(void)
{
    NcbiGetlineEOL(m_Stream, m_Line);
    return *this;
}

CTempString CStreamLineReader::operator*(void) const
{
    return CTempString(m_Line);
}

CT_POS_TYPE CStreamLineReader::GetPosition(void) const
{
    return m_Stream.tellg();
}


CMemoryLineReader::CMemoryLineReader(CMemoryFile* mem_file,
                                     EOwnership ownership)
    : m_Start(static_cast<char*>(mem_file->GetPtr())),
      m_End(m_Start + mem_file->GetSize()),
      m_Pos(m_Start)
{
    mem_file->MemMapAdvise(CMemoryFile::eMMA_Sequential);
    if (ownership == eTakeOwnership) {
        m_MemFile.reset(mem_file);
    }
}

bool CMemoryLineReader::AtEOF(void) const
{
    return m_Pos >= m_End;
}

char CMemoryLineReader::PeekChar(void) const
{
    return *m_Pos;
}

CMemoryLineReader& CMemoryLineReader::operator++(void)
{
    const char* p;
    for (p = m_Pos;  p < m_End  &&  *p != '\r'  && *p != '\n';  ++p)
        ;
    m_Line = CTempString(m_Pos, p - m_Pos);
    // skip over delimiters
    if (p + 1 < m_End  &&  *p == '\r'  &&  p[1] == '\n') {
        m_Pos = p + 2;
    } else if (p < m_End) {
        m_Pos = p + 1;
    } else { // no final line break
        m_Pos = p;
    }
    return *this;
}

CTempString CMemoryLineReader::operator*(void) const
{
    return m_Line;
}

CT_POS_TYPE CMemoryLineReader::GetPosition(void) const
{
    return NcbiInt8ToStreampos(m_Pos - m_Start);
}




CIReaderLineReader::CIReaderLineReader(IReader* reader, EOwnership ownership)
: m_Reader(reader),
  m_OwnReader(ownership),
  m_Buffer(1024 * 1024),
  m_BufferDataSize(0),
  m_RW_Result(eRW_Success),
  m_Eof(false),
  m_BufferReadSize(0)
{}

CIReaderLineReader::~CIReaderLineReader()
{
    if (eTakeOwnership == m_OwnReader) {
        delete m_Reader;
    }
}

void CIReaderLineReader::SetBufferSize(size_t buf_size)
{
    m_Buffer.resize(buf_size);
}

bool CIReaderLineReader::AtEOF(void) const
{
    return m_Eof;
}

char  CIReaderLineReader::PeekChar(void) const
{
    return m_Buffer[m_BufferReadSize];
}

CIReaderLineReader& CIReaderLineReader::operator++(void)
{
    // check if we are at the buffer end
    if (m_BufferReadSize == m_BufferDataSize) {
        m_BufferDataSize = m_BufferReadSize = 0;
        if (x_ReadBuffer() != eRW_Success) {
            m_Eof = true;
            return *this;
        }
    }

    // find the next line-end in the current buffer

    size_t line_start = m_BufferReadSize;
    while (1) {
        const char* p = &(m_Buffer[0]) + m_BufferReadSize;
        for (;m_BufferReadSize < m_BufferDataSize;  ++m_BufferReadSize, ++p) {
            if (*p == '\r'  || *p == '\n') {
                m_Line = 
                    CTempString(&(m_Buffer[0]) + line_start, m_BufferReadSize - line_start);
                // skip over delimiters
                if (++m_BufferReadSize < m_BufferDataSize && 
                    *p == '\r'  &&  p[1] == '\n') {
                    ++m_BufferReadSize;
                }
                return *this;
            }
        }

        // no final delimiter: load next portion of data to keep searching

        if (line_start) {
            _ASSERT(m_BufferReadSize > line_start);
            m_BufferDataSize = m_BufferReadSize = m_BufferReadSize - line_start;
            ::memmove(&(m_Buffer[0]), &(m_Buffer[0]) + line_start, m_BufferDataSize);
            line_start = 0;
            if (x_ReadBuffer() != eRW_Success) {
                m_Line = 
                    CTempString(&(m_Buffer[0]) + line_start, 
                                m_BufferReadSize - line_start);
                return *this;
            }
        } else {
            if (m_BufferDataSize < m_Buffer.size()) {
                if (x_ReadBuffer() != eRW_Success) {
                    m_Line = 
                        CTempString(&(m_Buffer[0]) + line_start, 
                                    m_BufferReadSize - line_start);
                    return *this;
                }
            } else {
                // string too long?
                NCBI_THROW(CIOException, eRead, "Buffer too small to accomodate line");
            }
        }

    } // while

    return *this;
}

ERW_Result CIReaderLineReader::x_ReadBuffer()
{
    _ASSERT(m_Reader);

    if (m_RW_Result == eRW_Eof) {
        return eRW_Eof;
    }

    // compute buffer availability
    size_t buf_avail = m_Buffer.size() - m_BufferDataSize;
    size_t bytes_read;
    for (bool flag = true; flag; ) {
        m_RW_Result = 
            m_Reader->Read(&(m_Buffer[0]) + m_BufferDataSize, buf_avail, &bytes_read);
        switch (m_RW_Result) {
        case eRW_NotImplemented:
        case eRW_Error:
            NCBI_THROW(CIOException, eRead, "Read error");
            break;
        case eRW_Success:
            m_BufferDataSize += bytes_read;
            flag = false;
            break;
        case eRW_Timeout:
            // keep spinning around
            break;
        case eRW_Eof:
            flag = false;
            break;
        default:
            _ASSERT(0);
        }
    } // for
    return m_RW_Result;
}

CTempString CIReaderLineReader::operator*(void) const
{
    return m_Line;
}

CT_POS_TYPE CIReaderLineReader::GetPosition(void) const
{
    _ASSERT(0);
    return 0; // TODO: implement position counter
}
END_NCBI_SCOPE
