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
* Revision 1.3  2000/02/11 17:10:25  vasilche
* Optimized text parsing.
*
* Revision 1.2  2000/02/02 19:07:41  vasilche
* Added THROWS_NONE to constructor/destructor of exception.
*
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
      m_CurrentPos(m_Buffer), m_DataEndPos(m_Buffer), m_MarkPos(m_Buffer),
      m_Line(1)
{
}

CStreamBuffer::~CStreamBuffer(void)
{
    delete[] m_Buffer;
}

char CStreamBuffer::SkipSpaces(void)
{
    // cache pointers
    char* pos = m_CurrentPos;
    char* end = m_DataEndPos;
    // make sure thire is at least one char in buffer
    if ( pos == end ) {
        // fill buffer
        pos = FillBuffer(pos);
        // cache m_DataEndPos
        end = m_DataEndPos;
    }
    // main cycle
    // at the beginning:
    //     pos == m_CurrentPos
    //     end == m_DataEndPos
    //     pos < end
    for (;;) {
        // we use do{}while() cycle because condition is true at the beginning ( pos < end )
        do {
            // cache current char
            char c = *pos;
            if ( c != ' ' ) { // it's not space (' ')
                // point m_CurrentPos to first non space char
                m_CurrentPos = pos;
                // return char value
                return c;
            }
            // skip space char
        } while ( ++pos < end );
        // here pos == end == m_DataEndPos
        // point m_CurrentPos to end of buffer
        m_CurrentPos = pos;
        // fill next portion
        pos = FillBuffer(pos);
        // cache m_DataEndPos
        end = m_DataEndPos;
    }
}

char* CStreamBuffer::FillBuffer(char* pos)
{
    _ASSERT(pos >= m_DataEndPos);
    // remove unused portion of buffer at the beginning
    _ASSERT(m_CurrentPos >= m_Buffer);
    size_t erase = m_CurrentPos - m_Buffer;
    if ( erase > 0 ) {
        char* newPos = m_CurrentPos - erase;
        memmove(newPos, m_CurrentPos, m_DataEndPos - m_CurrentPos);
        m_CurrentPos = newPos;
        m_DataEndPos -= erase;
        m_MarkPos = m_Buffer;
        pos -= erase;
    }
    size_t dataSize = m_DataEndPos - m_Buffer;
    size_t size = pos - m_Buffer;
    if ( size >= m_BufferSize ) {
        // reallocate buffer
        size_t newSize = BiggerBufferSize(m_BufferSize);
        while ( size >= newSize ) {
            newSize = BiggerBufferSize(newSize);
        }
        char* newBuffer = new char[newSize];
        memcpy(newBuffer, m_Buffer, dataSize);
        m_CurrentPos = newBuffer + (m_CurrentPos - m_Buffer);
        m_DataEndPos = newBuffer + dataSize;
        m_MarkPos = newBuffer + (m_MarkPos - m_Buffer);
        delete[] m_Buffer;
        m_Buffer = newBuffer;
        m_BufferSize = newSize;
    }
    size_t load = m_BufferSize - dataSize;
    while ( load > 0 ) {
        m_Input.read(m_DataEndPos, load);
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
    _ASSERT(lastChar == '\n' || lastChar == '\r');
    _ASSERT(m_CurrentPos > m_Buffer && m_CurrentPos[-1] == lastChar);
    m_Line++;
    char nextChar;
    try {
        nextChar = PeekChar();
    }
    catch ( CEofException& /* ignored */ ) {
        return;
    }
    // lastChar either '\r' or \n'
    // if nextChar is compliment, skip it
    if ( (lastChar + nextChar) == ('\r' + '\n') )
        SkipChar();
}

size_t CStreamBuffer::ReadLine(char* buff, size_t size)
{
    size_t count = 0;
    try {
        while ( size > 0 ) {
            char c = *buff++ = GetChar();
            count++;
            size--;
            switch ( c ) {
            case '\r':
                // replace leading '\r' by '\n'
                buff[-1] = '\n';
                if ( PeekChar() == '\n' )
                    SkipChar();
                return count;
            case '\n':
                if ( PeekChar() == '\r' )
                    SkipChar();
                return count;
            }
        }
        return count;
    }
    catch ( CEofException& /*ignored*/ ) {
        return count;
    }
}

CEofException::CEofException(void) THROWS_NONE
    : runtime_error("end of file")
{
}

CEofException::~CEofException(void) THROWS_NONE
{
}

END_NCBI_SCOPE
