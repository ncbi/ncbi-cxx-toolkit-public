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
* Author: Eugene Vasilchenko
*
* File Description:
*   Input buffer
*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.1  2000/02/01 21:47:23  vasilche
* Added CGeneratedChoiceTypeInfo for generated choice classes.
* Added buffering to CObjectIStreamAsn.
* Removed CMemberInfo subclasses.
* Added support for DEFAULT/OPTIONAL members.
*
*
* ===========================================================================
*/

#include <corelib/ncbistre.hpp>
#include <serial/strbuffer.hpp>
#include <corelib/ncbiexpt.hpp>

BEGIN_NCBI_SCOPE

static const size_t KInitialBufferSize = 8192;

static inline
size_t BiggerBufferSize(size_t size)
{
    return size * 2;
}

CStreamBuffer::CStreamBuffer(CNcbiIstream& in)
    : m_Input(in),
      m_BufferSize(KInitialBufferSize), m_Buffer(new char[KInitialBufferSize]),
      m_CurrentPos(0), m_DataEndPos(0), m_MarkPos(0),
      m_Line(1)
{
}

CStreamBuffer::~CStreamBuffer(void)
{
    delete[] m_Buffer;
}

size_t CStreamBuffer::FillBuffer(size_t pos)
{
    _ASSERT(pos >= m_DataEndPos);
    // remove unused portion of buffer at the beginning
    size_t erase = m_CurrentPos;
    if ( erase > 0 ) {
        size_t newPos = m_CurrentPos - erase;
        memmove(m_Buffer + newPos, m_Buffer + m_CurrentPos,
                m_DataEndPos - m_CurrentPos);
        m_CurrentPos = newPos;
        m_DataEndPos -= erase;
        m_MarkPos = 0;
        pos -= erase;
    }
    if ( pos >= m_BufferSize ) {
        // reallocate buffer
        size_t newSize = BiggerBufferSize(m_BufferSize);
        while ( pos >= newSize ) {
            newSize = BiggerBufferSize(newSize);
        }
        char* newBuffer = new char[newSize];
        memcpy(newBuffer, m_Buffer, m_DataEndPos);
        delete[] m_Buffer;
        m_Buffer = newBuffer;
        m_BufferSize = newSize;
    }
    size_t load = m_BufferSize - m_DataEndPos;
    while ( load > 0 ) {
        m_Input.read(m_Buffer + m_DataEndPos, load);
/*        if ( !m_Input )
            m_Input.clear();*/
        size_t count = m_Input.gcount();
        if ( count == 0 ) {
            if ( pos < m_DataEndPos )
                return pos;
            if ( m_Input.eof() )
                THROW0_TRACE(CEofException());
            else
                THROW1_TRACE(runtime_error, "read fault");
        }
        m_DataEndPos += count;
        load -= count;
    }
    return pos;
}

void CStreamBuffer::SkipEndOfLine(char lastChar)
{
    _ASSERT(m_CurrentPos > 0 && m_Buffer[m_CurrentPos - 1] == lastChar);
    m_Line++;
    try {
        if ( lastChar == '\r' ) {
            if ( PeekChar() == '\n' )
                SkipChar();
        }
        else {
            _ASSERT(lastChar == '\n');
            if ( PeekChar() == '\r' )
                SkipChar();
        }
    }
    catch ( CEofException& /* ignored */ ) {
    }
}

size_t CStreamBuffer::ReadLine(char* buff, size_t size)
{
    size_t count = 0;
    try {
        while ( size > 0 ) {
            char c = *buff++ = GetChar();
            count++;
            size--;
            
            if ( size > 0 ) {
                switch ( c ) {
                case '\r':
                    if ( PeekChar() != '\n' )
                        continue;
                    break;
                case '\n':
                    if ( PeekChar() != '\r' )
                        continue;
                    break;
                }
                *buff++ = GetChar();
                count++;
                size--;
                return count;
            }
        }
        return count;
    }
    catch ( CEofException& /*ignored*/ ) {
        return count;
    }
}

CEofException::CEofException(void)
    : runtime_error("end of file")
{
}

CEofException::~CEofException(void)
{
}

END_NCBI_SCOPE
