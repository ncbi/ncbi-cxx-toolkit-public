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
#include <corelib/rwstream.hpp>
#include <corelib/ncbifile.hpp>

#include <string.h>

BEGIN_NCBI_SCOPE

CRef<ILineReader> ILineReader::New(const string& filename)
{
    CRef<ILineReader> lr;
    lr.Reset(new CBufferedLineReader(filename));
    return lr;
}


CStreamLineReader::CStreamLineReader(CNcbiIstream& is,
                                     EOwnership ownership)
    : m_Stream(&is, ownership)
{
}


CStreamLineReader::~CStreamLineReader()
{
}


bool CStreamLineReader::AtEOF(void) const
{
    return m_Stream->eof()  ||  CT_EQ_INT_TYPE(m_Stream->peek(), CT_EOF);
}

char CStreamLineReader::PeekChar(void) const
{
    return m_Stream->peek();
}

void CStreamLineReader::Seek(CT_POS_TYPE pos)
{
    m_Stream->seekg(pos);
}

CStreamLineReader& CStreamLineReader::operator++(void)
{
    NcbiGetlineEOL(*m_Stream, m_Line);
    return *this;
}

CTempString CStreamLineReader::operator*(void) const
{
    return CTempString(m_Line);
}

CT_POS_TYPE CStreamLineReader::GetPosition(void) const
{
    return m_Stream->tellg();
}


CMemoryLineReader::CMemoryLineReader(CMemoryFile* mem_file,
                                     EOwnership ownership)
    : m_Start(static_cast<char*>(mem_file->GetPtr())),
      m_End(m_Start + mem_file->GetSize()),
      m_Pos(m_Start),
      m_MemFile(mem_file, ownership)
{
    m_MemFile->MemMapAdvise(CMemoryFile::eMMA_Sequential);
}

bool CMemoryLineReader::AtEOF(void) const
{
    return m_Pos >= m_End;
}

char CMemoryLineReader::PeekChar(void) const
{
    return *m_Pos;
}

void CMemoryLineReader::Seek(CT_POS_TYPE pos)
{
    Int8 offset(NcbiStreamposToInt8(pos));
    _ASSERT( (m_Start + offset) <= m_End );
    m_Pos = m_Start + offset;
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




CBufferedLineReader::CBufferedLineReader(IReader* reader,
                                         EOwnership ownership)
    : m_Reader(reader, ownership),
      m_Eof(false),
      m_BufferSize(32*1024),
      m_Buffer(new char[m_BufferSize]),
      m_Pos(m_Buffer.get()),
      m_End(m_Pos),
      m_InputPos(CT_POS_TYPE(0)),
      m_NextInputPos(CT_POS_TYPE(0))
{
    x_ReadBuffer();
}


CBufferedLineReader::CBufferedLineReader(CNcbiIstream& is,
                                         EOwnership ownership)
    : m_Reader(new CStreamReader(is, ownership)),
      m_Eof(false),
      m_BufferSize(32*1024),
      m_Buffer(new char[m_BufferSize]),
      m_Pos(m_Buffer.get()),
      m_End(m_Pos),
      m_InputPos(CT_POS_TYPE(0)),
      m_NextInputPos(CT_POS_TYPE(0))
{
    x_ReadBuffer();
}


CBufferedLineReader::CBufferedLineReader(const string& filename)
    : m_Reader(CFileReader::New(filename)),
      m_Eof(false),
      m_BufferSize(32*1024),
      m_Buffer(new char[m_BufferSize]),
      m_Pos(m_Buffer.get()),
      m_End(m_Pos),
      m_InputPos(CT_POS_TYPE(0)),
      m_NextInputPos(CT_POS_TYPE(0))
{
    x_ReadBuffer();
}


CBufferedLineReader::~CBufferedLineReader()
{
}


bool CBufferedLineReader::AtEOF(void) const
{
    return m_Eof;
}


char  CBufferedLineReader::PeekChar(void) const
{
    return *m_Pos;
}


CBufferedLineReader& CBufferedLineReader::operator++(void)
{
    // check if we are at the buffer end
    const char* start = m_Pos;
    const char* end = m_End;
    for ( const char* p = start; p < end; ++p ) {
        if ( *p == '\n' ) {
            m_Line = CTempString(start, p - start);
            m_Pos = ++p;
            if ( p == end ) {
                m_String = m_Line;
                m_Line = m_String;
                x_ReadBuffer();
            }
            return *this;
        }
        else if ( *p == '\r' ) {
            m_Line = CTempString(start, p - start);
            if ( ++p == end ) {
                m_String = m_Line;
                m_Line = m_String;
                if ( x_ReadBuffer() ) {
                    p = m_Pos;
                    if ( *p == '\n' ) {
                        m_Pos = p+1;
                    }
                }
                return *this;
            }
            if ( *p != '\n' ) {
                return *this;
            }
            m_Pos = ++p;
            if ( p == end ) {
                m_String = m_Line;
                m_Line = m_String;
                x_ReadBuffer();
            }
            return *this;
        }
    }
    x_LoadLong();
    return *this;
}


void CBufferedLineReader::x_LoadLong(void)
{
    const char* start = m_Pos;
    const char* end = m_End;
    m_String.assign(start, end);
    while ( x_ReadBuffer() ) {
        start = m_Pos;
        end = m_End;
        for ( const char* p = start; p < end; ++p ) {
            char c = *p;
            if ( c == '\r' || c == '\n' ) {
                m_String.append(start, p - start);
                m_Line = m_String;
                if ( ++p == end ) {
                    m_String = m_Line;
                    m_Line = m_String;
                    if ( x_ReadBuffer() ) {
                        p = m_Pos;
                        end = m_End;
                        if ( p < end && c == '\r' && *p == '\n' ) {
                            ++p;
                            m_Pos = p;
                        }
                    }
                }
                else {
                    if ( c == '\r' && *p == '\n' ) {
                        if ( ++p == end ) {
                            x_ReadBuffer();
                            p = m_Pos;
                        }
                    }
                    m_Pos = p;
                }
                return;
            }
        }
        m_String.append(start, end - start);
    }
    m_Line = m_String;
    return;
}


bool CBufferedLineReader::x_ReadBuffer()
{
    _ASSERT(m_Reader);

    if ( m_Eof ) {
        return false;
    }

    m_Pos = m_End = m_Buffer.get();
    for (bool flag = true; flag; ) {
        size_t size;
        ERW_Result result =
            m_Reader->Read(m_Buffer.get(), m_BufferSize, &size);
        switch (result) {
        case eRW_NotImplemented:
        case eRW_Error:
            NCBI_THROW(CIOException, eRead, "Read error");
            break;
        case eRW_Timeout:
            // keep spinning around
            break;
        case eRW_Eof:
            m_Eof = true;
            // fall through
        case eRW_Success:
            m_End = m_Pos + size;
            m_InputPos = m_NextInputPos;
            m_NextInputPos += CT_OFF_TYPE(size);
            return (result == eRW_Success  ||  size > 0);
        default:
            _ASSERT(0);
        }
    } // for
    return false;
}


CTempString CBufferedLineReader::operator*(void) const
{
    return m_Line;
}


CT_POS_TYPE CBufferedLineReader::GetPosition(void) const
{
    return m_InputPos + CT_OFF_TYPE(m_Pos - m_Buffer.get());
}

END_NCBI_SCOPE
